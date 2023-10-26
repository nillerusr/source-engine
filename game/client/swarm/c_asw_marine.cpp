#include "cbase.h"
#include "c_asw_marine.h"
#include "c_asw_generic_emitter.h"
#include "c_asw_player.h"
#include "c_asw_marine_resource.h"
#include "asw_marine_profile.h"
#include "c_asw_game_resource.h"
#include "c_asw_weapon.h"
#include "fx.h"
#include "asw_fx_shared.h"
#include "eventlist.h"
#include "decals.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "flashlighteffect.h"
#include "c_asw_door_area.h"
#include "soundenvelope.h"
#include "iasw_client_vehicle.h"
#include "asw_remote_turret_shared.h"
#include "iviewrender_beams.h"			// flashlight beam
#include "dlight.h"
#include "iefx.h"
#include "asw_shareddefs.h"
#include "tier0/vprof.h"
#include "bone_setup.h"
#include "c_asw_generic_emitter_entity.h"
#include "datacache/imdlcache.h"
#include "c_asw_fx.h"
#include "fx_water.h"
#include "effect_dispatch_data.h"	//for water ripple / splash effect
#include "c_te_effect_dispatch.h"	//ditto
#include "c_asw_order_arrow.h"
#include "c_env_projectedtexture.h"
#include "asw_gamerules.h"
#include "cdll_bounded_cvars.h"
#include "c_asw_computer_area.h"
#include "c_asw_button_area.h"
#include "tier1/fmtstr.h"
#include "baseparticleentity.h"
#include "asw_util_shared.h"
#include "asw_melee_system.h"
#include "asw_marine_gamemovement.h"
#include "game_timescale_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//extern ConVar cl_predict;
static ConVar	cl_asw_smooth		( "cl_asw_smooth", "1", 0, "Smooth marine's render origin after prediction errors" );
static ConVar	cl_asw_smoothtime	( 
	"cl_asw_smoothtime", 
	"0.1", 
	0, 
	"Smooth marine's render origin after prediction error over this many seconds",
	true, 0.01,	// min/max is 0.01/2.0
	true, 2.0
	 );
ConVar asw_flashlight_dlight_radius("asw_flashlight_dlight_radius", "100", FCVAR_CHEAT, "Radius of the light around the marine.");
ConVar asw_flashlight_dlight_offsetx("asw_flashlight_dlight_offsetx", "30", FCVAR_CHEAT, "Offset of the flashlight dlight");
ConVar asw_flashlight_dlight_offsety("asw_flashlight_dlight_offsety", "0", FCVAR_CHEAT, "Offset of the flashlight dlight");
ConVar asw_flashlight_dlight_offsetz("asw_flashlight_dlight_offsetz", "60", FCVAR_CHEAT, "Offset of the flashlight dlight");
ConVar asw_flashlight_dlight_r("asw_flashlight_dlight_r", "250", FCVAR_CHEAT, "Red component of flashlight colour");
ConVar asw_flashlight_dlight_g("asw_flashlight_dlight_g", "250", FCVAR_CHEAT, "Green component of flashlight colour");
ConVar asw_flashlight_dlight_b("asw_flashlight_dlight_b", "250", FCVAR_CHEAT, "Blue component of flashlight colour");
ConVar asw_marine_ambient("asw_marine_ambient", "0.02", FCVAR_CHEAT, "Ambient light of the marine");
ConVar asw_marine_lightscale("asw_marine_lightscale", "4.0", FCVAR_CHEAT, "Light scale on the marine");
ConVar asw_flashlight_marine_ambient("asw_flashlight_marine_ambient", "0.1", FCVAR_CHEAT, "Ambient light of the marine with flashlight on");
ConVar asw_flashlight_marine_lightscale("asw_flashlight_marine_lightscale", "1.0", FCVAR_CHEAT, "Light scale on the marine with flashlight on");
ConVar asw_left_hand_ik("asw_left_hand_ik", "0", FCVAR_CHEAT, "IK the marine's left hand to his weapon");
ConVar asw_marine_shoulderlight("asw_marine_shoulderlight", "2", 0, "Should marines have a shoulder light effect on them.");
ConVar asw_hide_local_marine("asw_hide_local_marine", "0", FCVAR_CHEAT, "If enabled, your current marine will be invisible");
ConVar asw_override_footstep_volume( "asw_override_footstep_volume", "0", FCVAR_CHEAT, "Overrides footstep volume instead of it being surface dependent" );
ConVar asw_marine_object_motion_blur_scale( "asw_marine_object_motion_blur_scale", "0.0" );
ConVar asw_damage_spark_rate( "asw_damage_spark_rate", "0.24", FCVAR_CHEAT, "Base number of seconds between spark sounds/effects at critical damage." );
extern ConVar asw_DebugAutoAim;
extern float g_fMarinePoisonDuration;

#define FLASHLIGHT_DISTANCE		1000
#define ASW_PROJECTOR_FLASHLIGHT 1

// uncomment to enable a dlight on the marine's flashlight (disabled for perf reasons)
//#define ASW_FLASHLIGHT_DLIGHT

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------

void RecvProxy_Marine_LocalVelocityX( const CRecvProxyData *pData, void *pStruct, void *pOut );
void RecvProxy_Marine_LocalVelocityY( const CRecvProxyData *pData, void *pStruct, void *pOut );
void RecvProxy_Marine_LocalVelocityZ( const CRecvProxyData *pData, void *pStruct, void *pOut );
//void RecvProxy_Marine_GroundEnt( const CRecvProxyData *pData, void *pStruct, void *pOut );

IMPLEMENT_NETWORKCLASS_ALIASED( ASW_Marine, DT_ASW_Marine )

BEGIN_NETWORK_TABLE( CASW_Marine, DT_ASW_Marine )
	RecvPropVectorXY( RECVINFO_NAME( m_vecNetworkOrigin, m_vecOrigin ), 0, C_BasePlayer::RecvProxy_LocalOriginXY ),
	RecvPropFloat( RECVINFO_NAME( m_vecNetworkOrigin[2], m_vecOrigin[2] ), 0, C_BasePlayer::RecvProxy_LocalOriginZ ),

	RecvPropFloat		( RECVINFO( m_vecVelocity[0] ), 0, RecvProxy_Marine_LocalVelocityX ),
	RecvPropFloat		( RECVINFO( m_vecVelocity[1] ), 0, RecvProxy_Marine_LocalVelocityY ),
	RecvPropFloat		( RECVINFO( m_vecVelocity[2] ), 0, RecvProxy_Marine_LocalVelocityZ ),

	RecvPropFloat( RECVINFO_NAME( m_angNetworkAngles[0], m_angRotation[0] ) ),
	RecvPropFloat( RECVINFO_NAME( m_angNetworkAngles[1], m_angRotation[1] ) ),
	RecvPropFloat( RECVINFO_NAME( m_angNetworkAngles[2], m_angRotation[2] ) ),

	RecvPropFloat		( RECVINFO( m_fAIPitch ) ),
	RecvPropInt			( RECVINFO( m_fFlags) ),
	RecvPropInt			( RECVINFO( m_iHealth) ),
	RecvPropInt			( RECVINFO( m_iMaxHealth ) ),
	RecvPropFloat		( RECVINFO( m_fInfestedTime ) ),
	RecvPropFloat		( RECVINFO( m_fInfestedStartTime ) ),
	RecvPropInt			( RECVINFO( m_ASWOrders), 4),	
	RecvPropEHandle		( RECVINFO( m_Commander) ),
	RecvPropArray3		( RECVINFO_ARRAY( m_iAmmo ), RecvPropInt( RECVINFO( m_iAmmo[0] ) ) ),
	RecvPropBool		( RECVINFO( m_bSlowHeal ) ),	
	RecvPropInt			( RECVINFO( m_iSlowHealAmount ) ),
	RecvPropBool		( RECVINFO( m_bPreventMovement ) ),
	RecvPropBool		( RECVINFO( m_bWalking ) ),
	RecvPropFloat		( RECVINFO( m_fFFGuardTime ) ),	
	RecvPropEHandle		( RECVINFO( m_hUsingEntity ) ),
	RecvPropVector		( RECVINFO( m_vecFacingPointFromServer ) ),
	RecvPropEHandle		( RECVINFO( m_hGroundEntity ) ),	// , RecvProxy_Marine_GroundEnt
	RecvPropEHandle		( RECVINFO( m_hMarineFollowTarget ) ),
	
	RecvPropTime		( RECVINFO(m_fStopMarineTime) ),
	RecvPropTime		( RECVINFO(m_fNextMeleeTime) ),
	RecvPropTime		( RECVINFO( m_flNextAttack ) ),
	RecvPropInt			( RECVINFO( m_iMeleeAttackID ) ),
	
	RecvPropEHandle		( RECVINFO ( m_hCurrentHack ) ),
	RecvPropBool		( RECVINFO ( m_bOnFire ) ),

	//emotes
	RecvPropBool	(RECVINFO(bEmoteMedic)),
	RecvPropBool	(RECVINFO(bEmoteAmmo)),
	RecvPropBool	(RECVINFO(bEmoteSmile)),
	RecvPropBool	(RECVINFO(bEmoteStop)),
	RecvPropBool	(RECVINFO(bEmoteGo)),
	RecvPropBool	(RECVINFO(bEmoteExclaim)),
	RecvPropBool	(RECVINFO(bEmoteAnimeSmile)),
	RecvPropBool	(RECVINFO(bEmoteQuestion)),

	// driving
	RecvPropEHandle (RECVINFO(m_hASWVehicle)),
	RecvPropBool	(RECVINFO(m_bDriving)),	
	RecvPropBool	(RECVINFO(m_bIsInVehicle)),	

	// falling over
	RecvPropBool	(RECVINFO(m_bKnockedOut)),

	// turret	
	RecvPropEHandle( RECVINFO ( m_hRemoteTurret ) ),

	// We send all the marine's weapons to all the other marines
	RecvPropArray3( RECVINFO_ARRAY(m_hMyWeapons), RecvPropEHandle( RECVINFO( m_hMyWeapons[0] ) ) ),

	RecvPropFloat( RECVINFO(m_vecViewOffset[0]) ),
	RecvPropFloat( RECVINFO(m_vecViewOffset[1]) ),
	RecvPropFloat( RECVINFO(m_vecViewOffset[2]) ),
#ifdef MELEE_CHARGE_ATTACKS
	RecvPropFloat	( RECVINFO( m_flMeleeHeavyKeyHoldStart ) ),
#endif
	RecvPropInt( RECVINFO( m_iForcedActionRequest ) ),
	RecvPropBool	( RECVINFO( m_bReflectingProjectiles ) ),
	RecvPropTime( RECVINFO( m_flDamageBuffEndTime ) ),
	RecvPropTime( RECVINFO( m_flElectrifiedArmorEndTime ) ),

	RecvPropInt		( RECVINFO( m_iPowerupType ) ),	
	RecvPropTime		( RECVINFO( m_flPowerupExpireTime ) ),	
	RecvPropBool	( RECVINFO( m_bPowerupExpires ) ),
	RecvPropFloat	( RECVINFO( m_flKnockdownYaw ) ),
	RecvPropFloat	( RECVINFO( m_flMeleeYaw ) ),
	RecvPropBool	( RECVINFO( m_bFaceMeleeYaw ) ),
	RecvPropFloat	( RECVINFO( m_flPreventLaserSightTime ) ),
	RecvPropBool	( RECVINFO( m_bAICrouch ) ),
	
	RecvPropInt		( RECVINFO( m_iJumpJetting ) ),	
END_RECV_TABLE()

BEGIN_PREDICTION_DATA( C_ASW_Marine )
	//DEFINE_PRED_FIELD( m_vecVelocity, FIELD_FLOAT, FTYPEDESC_OVERRIDE | FTYPEDESC_PRIVATE | FTYPEDESC_NOERRORCHECK ),
	DEFINE_PRED_FIELD( m_flCycle, FIELD_FLOAT, FTYPEDESC_OVERRIDE | FTYPEDESC_PRIVATE | FTYPEDESC_NOERRORCHECK ),
	//DEFINE_PRED_FIELD( m_flAnimTime, FIELD_FLOAT, FTYPEDESC_OVERRIDE | FTYPEDESC_PRIVATE | FTYPEDESC_NOERRORCHECK ),	
	DEFINE_PRED_FIELD( m_nSequence, FIELD_INTEGER, FTYPEDESC_OVERRIDE | FTYPEDESC_PRIVATE | FTYPEDESC_NOERRORCHECK ),	
	DEFINE_PRED_FIELD( m_nNewSequenceParity, FIELD_INTEGER, FTYPEDESC_OVERRIDE | FTYPEDESC_PRIVATE | FTYPEDESC_NOERRORCHECK ),
	DEFINE_PRED_FIELD( m_nResetEventsParity, FIELD_INTEGER, FTYPEDESC_OVERRIDE | FTYPEDESC_PRIVATE | FTYPEDESC_NOERRORCHECK ),
	DEFINE_PRED_FIELD( m_hGroundEntity, FIELD_EHANDLE, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD_TOL( m_fStopMarineTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE, TD_MSECTOLERANCE ),	
	DEFINE_PRED_FIELD_TOL( m_fNextMeleeTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE, TD_MSECTOLERANCE ),	
	DEFINE_PRED_FIELD_TOL( m_flNextAttack, FIELD_FLOAT, FTYPEDESC_INSENDTABLE, TD_MSECTOLERANCE ),		
	DEFINE_PRED_FIELD( m_iEFlags, FIELD_INTEGER, FTYPEDESC_OVERRIDE | FTYPEDESC_PRIVATE | FTYPEDESC_NOERRORCHECK ),
	DEFINE_PRED_FIELD( m_iMeleeAttackID, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
#ifdef MELEE_CHARGE_ATTACKS
	DEFINE_PRED_FIELD_TOL( m_flMeleeHeavyKeyHoldStart, FIELD_FLOAT, FTYPEDESC_INSENDTABLE, TD_MSECTOLERANCE ),
#endif
	DEFINE_PRED_FIELD( m_iForcedActionRequest, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bReflectingProjectiles, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_flMeleeYaw, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bFaceMeleeYaw, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bWalking, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD_TOL( m_flPreventLaserSightTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE, TD_MSECTOLERANCE ),
	DEFINE_PRED_FIELD( m_iJumpJetting, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),

	DEFINE_FIELD( m_nOldButtons, FIELD_INTEGER ),
	DEFINE_FIELD( m_surfaceFriction, FIELD_FLOAT ),
	DEFINE_FIELD( m_vecMeleeStartPos, FIELD_VECTOR ),
	DEFINE_FIELD( m_flMeleeStartTime, FIELD_FLOAT ),
	DEFINE_FIELD( m_flMeleeLastCycle, FIELD_FLOAT ),
	
	DEFINE_FIELD( m_bMeleeCollisionDamage, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bMeleeComboKeypressAllowed, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bMeleeComboKeyPressed, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bMeleeComboTransitionAllowed, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bMeleeMadeContact, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_iUsableItemsOnMeleePress, FIELD_INTEGER ),
	DEFINE_FIELD( m_iMeleeAllowMovement, FIELD_INTEGER ),
	DEFINE_FIELD( m_bMeleeKeyReleased, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bPlayedMeleeHitSound, FIELD_BOOLEAN ),
#ifdef MELEE_CHARGE_ATTACKS
	DEFINE_FIELD( m_bMeleeHeavyKeyHeld, FIELD_BOOLEAN ),
#endif
	DEFINE_FIELD( m_bMeleeChargeActivate, FIELD_BOOLEAN ),
	DEFINE_AUTO_ARRAY( m_iPredictedEvent, FIELD_INTEGER ),
	DEFINE_AUTO_ARRAY( m_flPredictedEventTime, FIELD_FLOAT ),
	DEFINE_FIELD( m_iNumPredictedEvents, FIELD_INTEGER ),
	DEFINE_FIELD( m_iOnLandMeleeAttackID, FIELD_INTEGER ),
	DEFINE_FIELD( m_vecJumpJetStart, FIELD_VECTOR ),
	DEFINE_FIELD( m_vecJumpJetEnd, FIELD_VECTOR ),
	DEFINE_FIELD( m_flJumpJetStartTime, FIELD_FLOAT ),
	DEFINE_FIELD( m_flJumpJetEndTime, FIELD_FLOAT ),

	/*
	DEFINE_FIELD( m_bSlowHeal, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_vecFacingPointFromServer, FIELD_VECTOR ),
	DEFINE_FIELD( m_hRemoteTurret, FIELD_EHANDLE ),
	DEFINE_FIELD( m_hASWVehicle, FIELD_EHANDLE ),
	DEFINE_FIELD( m_bDriving, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bIsInVehicle, FIELD_BOOLEAN ),
	DEFINE_FIELD( bEmoteMedic, FIELD_BOOLEAN ),
	DEFINE_FIELD( bEmoteAmmo, FIELD_BOOLEAN ),
	DEFINE_FIELD( bEmoteStop, FIELD_BOOLEAN ),
	DEFINE_FIELD( bEmoteGo, FIELD_BOOLEAN ),
	DEFINE_FIELD( bEmoteExclaim, FIELD_BOOLEAN ),
	DEFINE_FIELD( bEmoteAnimeSmile, FIELD_BOOLEAN ),
	DEFINE_FIELD( bEmoteQuestion, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_Commander, FIELD_EHANDLE),
	DEFINE_FIELD( m_fStopFacingPointTime, FIELD_FLOAT ),
	DEFINE_FIELD( m_bHacking, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_hCurrentHack, FIELD_EHANDLE),
	DEFINE_FIELD( m_hUsingEntity, FIELD_EHANDLE),
	DEFINE_FIELD( m_fLastTurningYaw, FIELD_FLOAT),
	DEFINE_FIELD( m_vecLastRenderedPos, FIELD_VECTOR),
	DEFINE_FIELD( m_bUseLastRenderedEyePosition, FIELD_BOOLEAN),

	// extra test
	//DEFINE_FIELD( m_vecCustomRenderOrigin, FIELD_VECTOR),
	//DEFINE_FIELD( m_vecPredictionError, FIELD_VECTOR),
	//DEFINE_FIELD(  m_flPredictionErrorTime, FIELD_FLOAT),
	//DEFINE_FIELD( m_fAIPitch, FIELD_FLOAT),
	//DEFINE_FIELD( m_AIEyeAngles, FIELD_VECTOR),
	DEFINE_FIELD( m_hLastWeaponSwitchedTo, FIELD_EHANDLE),
	DEFINE_FIELD( m_ShadowDirection, FIELD_VECTOR),
	DEFINE_FIELD( m_hMarineResource, FIELD_EHANDLE),
	DEFINE_FIELD( m_CurrentBlipStrength, FIELD_FLOAT),
	DEFINE_FIELD( m_CurrentBlipDirection, FIELD_INTEGER),
	DEFINE_FIELD( m_LastThinkTime, FIELD_FLOAT),
	DEFINE_FIELD( m_hMarineFollowTarget, FIELD_EHANDLE),	
	DEFINE_FIELD( m_bPreventMovement, FIELD_BOOLEAN),
	DEFINE_FIELD( m_ASWOrders, FIELD_INTEGER),
	DEFINE_FIELD( m_hOrderArrow, FIELD_EHANDLE),			
	DEFINE_FIELD( bPlayingFlamerSound, FIELD_BOOLEAN),
	DEFINE_FIELD( m_fFlameTime, FIELD_FLOAT),
	DEFINE_SOUNDPATCH( m_pFlamerLoopSound),
	DEFINE_FIELD( bPlayingFireExtinguisherSound, FIELD_BOOLEAN),
	DEFINE_FIELD( m_fFireExtinguisherTime, FIELD_FLOAT),
	DEFINE_SOUNDPATCH( m_pFireExtinguisherLoopSound ),
	DEFINE_FIELD( m_fNextHeartbeat, FIELD_FLOAT),
	DEFINE_FIELD( m_fFFGuardTime, FIELD_FLOAT),
	DEFINE_FIELD( m_iMaxHealth, FIELD_INTEGER),
	DEFINE_FIELD( m_bOnFire, FIELD_BOOLEAN),
	DEFINE_FIELD( m_bClientOnFire, FIELD_BOOLEAN),
	DEFINE_FIELD( m_fInfestedTime, FIELD_FLOAT),
	DEFINE_FIELD( m_fInfestedStartTime, FIELD_FLOAT),
	DEFINE_FIELD( m_fRedNamePulse, FIELD_FLOAT),
	DEFINE_FIELD( m_bRedNamePulseUp, FIELD_BOOLEAN),
	DEFINE_FIELD( m_vecFacingPoint, FIELD_VECTOR),
	DEFINE_FIELD( bEmoteSmile, FIELD_BOOLEAN),
	DEFINE_FIELD( bClientEmoteMedic, FIELD_BOOLEAN),
	DEFINE_FIELD( bClientEmoteAmmo, FIELD_BOOLEAN),
	DEFINE_FIELD( bClientEmoteSmile, FIELD_BOOLEAN),
	DEFINE_FIELD( bClientEmoteStop, FIELD_BOOLEAN),
	DEFINE_FIELD( bClientEmoteGo, FIELD_BOOLEAN),
	DEFINE_FIELD( bClientEmoteExclaim, FIELD_BOOLEAN),
	DEFINE_FIELD( bClientEmoteAnimeSmile, FIELD_BOOLEAN),
	DEFINE_FIELD( bClientEmoteQuestion, FIELD_BOOLEAN),
	DEFINE_FIELD( fEmoteMedicTime, FIELD_FLOAT),
	DEFINE_FIELD( fEmoteAmmoTime, FIELD_FLOAT),
	DEFINE_FIELD( fEmoteSmileTime, FIELD_FLOAT),
	DEFINE_FIELD( fEmoteStopTime, FIELD_FLOAT),
	DEFINE_FIELD( fEmoteGoTime, FIELD_FLOAT),
	DEFINE_FIELD( fEmoteExclaimTime, FIELD_FLOAT),
	DEFINE_FIELD( fEmoteAnimeSmileTime, FIELD_FLOAT),
	DEFINE_FIELD( fEmoteQuestionTime, FIELD_FLOAT),
	DEFINE_FIELD( m_bHasClientsideVehicle, FIELD_BOOLEAN),
	DEFINE_FIELD( m_bKnockedOut, FIELD_BOOLEAN),
	DEFINE_FIELD( m_fPoison, FIELD_FLOAT),
	DEFINE_FIELD( m_hShoulderCone, FIELD_EHANDLE),
	DEFINE_FIELD( m_fLastYawHack, FIELD_FLOAT),
	DEFINE_FIELD( m_fLastPitchHack, FIELD_FLOAT),
	DEFINE_FIELD( m_bStepSideLeft, FIELD_BOOLEAN),
	*/

	/*
	DEFINE_FIELD( CUtlVector < EHANDLE > m_TouchingDoors'
	DEFINE_FIELD( IASW_Client_Vehicle* m_pClientsideVehicle'
	DEFINE_FIELD( IASWPlayerAnimState *m_PlayerAnimState'
	DEFINE_FIELD( Beam_t *m_pFlashlightBeam'
	DEFINE_FIELD( dlight_t* m_pFlashlightDLight'
	DEFINE_FIELD( CSmartPtr < CASWGenericEmitter > m_hFlameEmitter'
	DEFINE_FIELD( CSmartPtr < CASWGenericEmitter > m_hFlameStreamEmitter'
	DEFINE_FIELD( CSmartPtr < CASWGenericEmitter > m_hFireExtinguisherEmitter'
	DEFINE_FIELD( CSmartPtr < CASWGenericEmitter > m_hHealEmitter'
	DEFINE_FIELD( CNewParticleEffect *m_pBurningEffect'
	*/
END_PREDICTION_DATA()

/*
void RecvProxy_Marine_GroundEnt( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	C_ASW_Marine *pMarine = (C_ASW_Marine *) pStruct;

	Assert( pMarine );

	EHANDLE hTarget;

	RecvProxy_IntToEHandle( pData, pStruct, &hTarget );

	pMarine->m_hGroundEntity.Init( hTarget.GetEntryIndex(), hTarget.GetSerialNumber() );
}
*/

void RecvProxy_Marine_LocalVelocityX( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	C_ASW_Marine *pMarine = (C_ASW_Marine *) pStruct;

	Assert( pMarine );

	float flNewVel_x = pData->m_Value.m_Float;
	
	Vector vecVelocity = pMarine->GetLocalVelocity();	

	if( vecVelocity.x != flNewVel_x )	// Should this use an epsilon check?
	{
		//if (vecVelocity.x > 30.0f)
		//{
			
		//}		
		vecVelocity.x = flNewVel_x;
		pMarine->SetLocalVelocity( vecVelocity );
	}
}

void RecvProxy_Marine_LocalVelocityY( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	C_ASW_Marine *pMarine = (C_ASW_Marine *) pStruct;

	Assert( pMarine );

	float flNewVel_y = pData->m_Value.m_Float;

	Vector vecVelocity = pMarine->GetLocalVelocity();

	if( vecVelocity.y != flNewVel_y )
	{
		vecVelocity.y = flNewVel_y;
		pMarine->SetLocalVelocity( vecVelocity );
	}
}

void RecvProxy_Marine_LocalVelocityZ( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	C_ASW_Marine *pMarine = (C_ASW_Marine *) pStruct;
	
	Assert( pMarine );

	float flNewVel_z = pData->m_Value.m_Float;

	Vector vecVelocity = pMarine->GetLocalVelocity();

	if( vecVelocity.z != flNewVel_z )
	{
		vecVelocity.z = flNewVel_z;
		pMarine->SetLocalVelocity( vecVelocity );
	}
}

C_ASW_Marine::C_ASW_Marine() :
	m_MotionBlurObject( this, asw_marine_object_motion_blur_scale.GetFloat() ),
	m_vLaserSightCorrection( 0, 0, 0 ),
	m_flLaserSightLength( 0 )
{
	m_hShoulderCone = NULL;
	m_PlayerAnimState = CreatePlayerAnimState(this, this, LEGANIM_9WAY, false);
	SetPredictionEligible( true );
	m_Commander = NULL;
	m_hMarineResource = NULL;
	m_fPoison = 0;
	m_ShadowDirection.x = 0;
	m_ShadowDirection.y = 0;
	m_ShadowDirection.z = -1;	
	m_CurrentBlipStrength = 0;
	m_CurrentBlipDirection = 1;
	m_LastThinkTime = gpGlobals->curtime;
	m_fLastYawHack = m_fLastPitchHack = 0;	
	m_bStepSideLeft = true;
	m_nOldButtons = 0;
	//m_fAmbientLight = asw_marine_ambient.GetFloat();
	//m_fLightingScale = asw_marine_lightscale.GetFloat();
	m_pFlashlight = NULL;
	m_TouchingDoors.RemoveAll();
	bPlayingFlamerSound = false;
	bPlayingFireExtinguisherSound = false;
	m_pFlamerLoopSound = 0;
	m_fFlameTime = 0;
	m_flNextDamageSparkTime = 0.0f;
	m_bClientElectrifiedArmor = false;
	m_fFireExtinguisherTime = 0;
	m_fNextHeartbeat = 0;
	m_iOldHealth = m_iHealth;
	m_fLastHealTime = 0.0f;
	m_vecFacingPoint = vec3_origin;	
	m_fStopFacingPointTime = 0;	
	m_fStopMarineTime = 0;
	m_bHasClientsideVehicle = false;
	m_vecPredictionError.Init();
	m_flPredictionErrorTime = 0;
	m_pFlashlightBeam = NULL;
	m_pFlashlightDLight = NULL;	
	m_fNextMeleeTime = 0;
#ifdef MELEE_CHARGE_ATTACKS
	m_flMeleeHeavyKeyHoldStart = 0;
#endif
	m_bClientOnFire = false;
	m_pBurningEffect = NULL;
	m_pJumpJetEffect[0] = NULL;
	m_pJumpJetEffect[1] = NULL;
	m_hOrderArrow = C_ASW_Order_Arrow::CreateOrderArrow();
	m_fRedNamePulse = 0;
	m_bRedNamePulseUp = true;

	bClientEmoteMedic = bClientEmoteAmmo = bClientEmoteSmile = bClientEmoteStop
		= bClientEmoteGo = bClientEmoteExclaim = bClientEmoteAnimeSmile = bClientEmoteQuestion = false;
	fEmoteMedicTime = fEmoteAmmoTime = fEmoteSmileTime = fEmoteStopTime
		= fEmoteGoTime = fEmoteExclaimTime = fEmoteAnimeSmileTime = fEmoteQuestionTime = 0;

	m_surfaceProps = 0;
	m_pSurfaceData = NULL;
	m_surfaceFriction = 1.0f;
	m_chTextureType = m_chPreviousTextureType = 0;
	m_iPowerupType = -1;
	m_flPowerupExpireTime = -1;
	m_bPowerupExpires = false;
}


C_ASW_Marine::~C_ASW_Marine()
{
	m_PlayerAnimState->Release();
	if (m_pFlashlight)
	{
		delete m_pFlashlight;
	}
	StopFlamerLoop();
	StopFireExtinguisherLoop();
	m_bOnFire = false;
	UpdateFireEmitters();
	if (m_hOrderArrow.Get())
		m_hOrderArrow->Release();

	if ( m_hLowHeathEffect )
	{
		m_hLowHeathEffect->StopEmission(false, false , true);
		m_hLowHeathEffect = NULL;
	}

	if ( m_hCriticalHeathEffect )
	{
		m_hCriticalHeathEffect->StopEmission(false, false , true);
		m_hCriticalHeathEffect = NULL;
	}

	if ( m_hSentryBuildDisplay )
	{
		m_hSentryBuildDisplay->StopEmission(false, false , true);
		m_hSentryBuildDisplay = NULL;
	}
}


bool C_ASW_Marine::ShouldPredict()
{
	if (C_BasePlayer::IsLocalPlayer(GetCommander()))
	{
		FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
		{
			ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );

			C_ASW_Player* player = C_ASW_Player::GetLocalASWPlayer();
			if (player && player->GetMarine() == this)
			{
				return true;
			}
		}
	}
	
	return false;
}

C_BasePlayer *C_ASW_Marine::GetPredictionOwner()
{
	return GetCommander();
}

void C_ASW_Marine::PhysicsSimulate( void )
{
	if (ShouldPredict())
	{
		m_nSimulationTick = gpGlobals->tickcount;
		return;
	}
	BaseClass::PhysicsSimulate();
}

void C_ASW_Marine::InitPredictable( C_BasePlayer *pOwner )
{
	SetLocalVelocity(vec3_origin);
	BaseClass::InitPredictable( pOwner );
}

void C_ASW_Marine::PostDataUpdate( DataUpdateType_t updateType )
{
	bool bPredict = ShouldPredict();
	if ( bPredict )
	{
		SetSimulatedEveryTick( true );		
	}
	else
	{
		SetSimulatedEveryTick( false );

		// estimate velocity for non local players
		float flTimeDelta = m_flSimulationTime - m_flOldSimulationTime;
		if ( flTimeDelta > 0  && !IsEffectActive(EF_NOINTERP) )
		{
			Vector newVelo = (GetNetworkOrigin() - GetOldOrigin()  ) / flTimeDelta;
			SetAbsVelocity( newVelo);
		}
	}

	// if player has switched into this marine, set it to be prediction eligible
	if (bPredict)
	{
		// C_BaseEntity assumes we're networking the entity's angles, so pretend that it
		// networked the same value we already have.
		//SetNetworkAngles( GetLocalAngles() );
		SetPredictionEligible( true );
	}
	else
	{
		SetPredictionEligible( false );
	}
	
	BaseClass::PostDataUpdate( updateType );

	if ( GetPredictable() && !bPredict )
	{
		MDLCACHE_CRITICAL_SECTION();
		ShutdownPredictable();
	}
}

void C_ASW_Marine::UpdateClientSideAnimation()
{
	VPROF_BUDGET( "C_ASW_Marine::UpdateClientSideAnimation", VPROF_BUDGETGROUP_ASW_CLIENT );
	if ( GetSequence() != -1 )
	{
		// latch old values
		//OnLatchInterpolatedVariables( LATCH_ANIMATION_VAR );
		// move frame forward
		FrameAdvance( gpGlobals->frametime );
	}

	C_ASW_Player *pPlayer = GetCommander();
		
	if ( pPlayer && C_BasePlayer::IsLocalPlayer(pPlayer) && pPlayer->GetMarine() == this && !IsControllingTurret())
	{
		m_PlayerAnimState->Update( pPlayer->EyeAngles()[YAW], pPlayer->EyeAngles()[PITCH] );
		m_fLastYawHack = pPlayer->EyeAngles()[YAW];
		m_fLastPitchHack = pPlayer->EyeAngles()[PITCH];
	}
	else
		m_PlayerAnimState->Update( ASWEyeAngles()[YAW], ASWEyeAngles()[PITCH] );

	if ( GetSequence() != -1 )
	{
		// latch old values
		OnLatchInterpolatedVariables( LATCH_ANIMATION_VAR );
	}
}


const Vector& C_ASW_Marine::GetRenderOrigin()
{
	m_vecCustomRenderOrigin = GetAbsOrigin();

	/*
	if (m_PlayerAnimState && !m_PlayerAnimState->IsAnimatingJump())
	{
		C_BaseEntity *pEnt = cl_entitylist->FirstBaseEntity();
		while (pEnt)
		{
			if (FClassnameIs(pEnt, "class C_DynamicProp"))
			{
				//Msg("Setting z to %f\n", pEnt->GetAbsOrigin().z + 10);
				m_vecCustomRenderOrigin.z = pEnt->GetAbsOrigin().z + 10;			
				break;
			}
			pEnt = cl_entitylist->NextBaseEntity( pEnt );
		}
	}
	*/
	if (IsInhabited())
	{
		Vector vSmoothOffset;
		GetPredictionErrorSmoothingVector( vSmoothOffset );
		m_vecCustomRenderOrigin += vSmoothOffset;
	}

	return m_vecCustomRenderOrigin;
}

void C_ASW_Marine::NotePredictionError( const Vector &vDelta )
{
	Vector vOldDelta;

	GetPredictionErrorSmoothingVector( vOldDelta );

	// sum all errors within smoothing time
	m_vecPredictionError = vDelta + vOldDelta;

	// remember when last error happened
	m_flPredictionErrorTime = gpGlobals->curtime;
 
	ResetLatched(); 
}

void C_ASW_Marine::GetPredictionErrorSmoothingVector( Vector &vOffset )
{
	if ( engine->IsPlayingDemo() || !cl_asw_smooth.GetInt() || !cl_predict->GetBool() )
	{
		vOffset.Init();
		return;
	}

	float errorAmount = ( gpGlobals->curtime - m_flPredictionErrorTime ) / cl_asw_smoothtime.GetFloat();

	if ( errorAmount >= 1.0f )
	{
		vOffset.Init();
		return;
	}
	
	errorAmount = 1.0f - errorAmount;

	vOffset = m_vecPredictionError * errorAmount;
}

CBaseCombatWeapon* C_ASW_Marine::ASWAnim_GetActiveWeapon()
{
	return GetActiveWeapon();
}

CASW_Marine_Profile* C_ASW_Marine::GetMarineProfile()
{
	C_ASW_Marine_Resource* pMR = GetMarineResource();
	if (!pMR)
		return NULL;

	return pMR->GetProfile();
}

bool C_ASW_Marine::ASWAnim_CanMove()
{
	return true;
}

const QAngle& C_ASW_Marine::GetRenderAngles()
{
	//if (ShouldPredict())
	{
		if ( IsRagdoll() )
		{
			return vec3_angle;
		}
		else
		{
			return m_PlayerAnimState->GetRenderAngles();
		}
	}
	//else
	//{
		//return BaseClass::GetRenderAngles();
	//}
}

C_ASW_Marine_Resource* C_ASW_Marine::GetMarineResource() 
{	
	if (m_hMarineResource.Get() != NULL)
		return m_hMarineResource.Get();

	// find our marine info
	C_ASW_Game_Resource* pGameResource = ASWGameResource();
	if (pGameResource != NULL)
	{
		for (int i=0;i<pGameResource->GetMaxMarineResources();i++)
		{
			C_ASW_Marine_Resource* pMR = pGameResource->GetMarineResource(i);
			if (pMR != NULL && pMR->GetMarineEntity() == this)
			{
				m_hMarineResource = pMR;
				return pMR;
			}
		}
	}
	return NULL;
}

// shadow direction test
bool C_ASW_Marine::GetShadowCastDirection( Vector *pDirection, ShadowType_t shadowType ) const			
{ 	
	return false;
	pDirection->x = m_ShadowDirection.x;
	pDirection->y = m_ShadowDirection.y;
	pDirection->z = m_ShadowDirection.z;	

	return true;
}

void C_ASW_Marine::ClientThink()
{
	VPROF_BUDGET( "C_ASW_Marine::ClientThink", VPROF_BUDGETGROUP_ASW_CLIENT );
	BaseClass::ClientThink();

	if ( ShouldPredict() )
	{
		// Update projected texture culling helper
		C_EnvProjectedTexture::SetVisibleBBoxMinHeight( GetAbsOrigin().z - 64.0f );
	}

	if (IsInhabited())
	{
		//m_vecLastRenderedPos = GetRenderOrigin();
	}
	if (asw_DebugAutoAim.GetInt() == 3)
	{
		Msg("%f: Drawmodel render origin %s\n", gpGlobals->curtime, VecToString(GetRenderOrigin()));
	}

	//if (GetActiveWeapon() && GetActiveWeapon()->GetPrimaryAmmoType())
	//Msg("Ammo = %d\n", GetAmmoCount(GetActiveWeapon()->GetPrimaryAmmoType()));
	//Msg("Ammo = %d\n", GetAmmoCount("ASW_R"));

	// tick up/down our minimap blip, reversing direction at the bounds
	float deltatime = gpGlobals->curtime - m_LastThinkTime;	

	m_CurrentBlipStrength += m_CurrentBlipDirection * deltatime * 3;
	if (m_CurrentBlipStrength > 1)
	{
		m_CurrentBlipStrength = 1;
		m_CurrentBlipDirection = - m_CurrentBlipDirection;
	}
	else if (m_CurrentBlipStrength < 0)
	{
		m_CurrentBlipStrength = 0;
		m_CurrentBlipDirection = - m_CurrentBlipDirection;
	}

	if ( IsInfested() )
	{
		// Predict
		m_fInfestedTime -= deltatime;
	}
	
	//Vector light_pos = engine->GetClosestLightLocation(GetAbsOrigin());
	//m_ShadowDirection.x = RandomFloat( -1.0, 1.0 );
	//m_ShadowDirection.y = RandomFloat( -1.0, 1.0 );
	//m_ShadowDirection.z = RandomFloat( -1.0, 1.0 );
	//VectorSubtract(GetAbsOrigin(), light_pos, m_ShadowDirection);
	//m_ShadowDirection = engine->GetClosestLightPosition(GetAbsOrigin());
	//m_ShadowDirection.NormalizeInPlace();

	// turn the flame emitter on or off
	C_ASW_Weapon *pWeapon = GetActiveASWWeapon();
	// flamethrower handles its own effects now
	if ( pWeapon && IsAlive() )//m_hFlameEmitter.IsValid() && 
	{			
		bool bFlameOn = pWeapon->ShouldMarineFlame();
		if (bFlameOn && !bPlayingFlamerSound)
		{
			StartFlamerLoop();
		}
		else if (!bFlameOn && bPlayingFlamerSound)
		{
			StopFlamerLoop();
		}			
		bPlayingFlamerSound = bFlameOn;
		/*
		m_hFlameEmitter->SetActive(bFlameOn);
		Vector vecMuzzle = GetAbsOrigin()+Vector(0,0,45);
		QAngle angMuzzle;
		
		C_BaseAnimating::PushAllowBoneAccess( true, false, "ClientThink" );
		pWeapon->GetAttachment( pWeapon->LookupAttachment("muzzle"), vecMuzzle, angMuzzle );

		m_hFlameEmitter->Think(gpGlobals->frametime, vecMuzzle, angMuzzle);
		if (m_hFlameStreamEmitter.IsValid())
		{
			m_hFlameStreamEmitter->SetActive(bFlameOn);
			m_hFlameStreamEmitter->Think(gpGlobals->frametime, vecMuzzle, angMuzzle);		
		}
		C_BaseAnimating::PopBoneAccess( "ClientThink" );
		*/
	}
	else
	{
		if (bPlayingFlamerSound)
			StopFlamerLoop();
	}
	if ( pWeapon && IsAlive())
	{			
		//bool bFireExtinguisherOn = m_fFireExtinguisherTime >= gpGlobals->curtime;
		bool bFireExtinguisherOn = pWeapon->ShouldMarineFireExtinguish();
		if (bFireExtinguisherOn && !bPlayingFireExtinguisherSound)
		{
			StartFireExtinguisherLoop();
		}
		else if (!bFireExtinguisherOn && bPlayingFireExtinguisherSound)
		{
			StopFireExtinguisherLoop();
		}			
		bPlayingFireExtinguisherSound = bFireExtinguisherOn;
		// extinguisher weapons now handle their own effects
		/*
		m_hFireExtinguisherEmitter->SetActive(bFireExtinguisherOn);	

		Vector vecMuzzle = GetAbsOrigin()+Vector(0,0,45);
		QAngle angMuzzle;
		int iMuzzleAttach = pWeapon->LookupAttachment("muzzle");
		//Msg("Firext muzzle attach = %d\n", iMuzzleAttach);
		if (!pWeapon->GetAttachment(iMuzzleAttach , vecMuzzle, angMuzzle ))
		{
			vecMuzzle = GetAbsOrigin()+Vector(0,0,45);
			angMuzzle = ASWEyeAngles();
		}
		else
		{
			//Msg("  from weapon %s vec = %s ang = %s\n", pWeapon->GetClassname(), VecToString(vecMuzzle), VecToString(angMuzzle));
		}

		m_hFireExtinguisherEmitter->Think(gpGlobals->frametime, vecMuzzle, angMuzzle);
		//m_hFireExtinguisherEmitter->Think(gpGlobals->frametime, GetAbsOrigin()+Vector(0,0,45), ASWEyeAngles());
		*/
	}
	else
	{
		if (bPlayingFireExtinguisherSound)
			StopFireExtinguisherLoop();
	}

	// if we're healing and we don't have an emitter, create a heal emitter
	if ( m_bSlowHeal && GetHealth()>0 )
	{
		if ( !m_pHealEmitter )
			m_pHealEmitter = ParticleProp()->Create( "heal_effect", PATTACH_ABSORIGIN_FOLLOW, -1, Vector( 0, 0, 50 ) );
	}
	else if ( m_pHealEmitter ) //we aren't healing, but the emitter is active, kill it
	{
		m_pHealEmitter->StopEmission(false, false, true);
		m_pHealEmitter = NULL;
	}

	if (m_hShoulderCone.Get() && ( GetHealth()<=0  || IsEffectActive(EF_NODRAW)) )
	{
		UTIL_Remove( m_hShoulderCone );
		m_hShoulderCone = NULL;
	}

	if (ShouldPredict())
	{
		UpdateHeartbeat();		
	}

	if ( IsAlive() )
	{
		UpdateDamageEffects( false );
	}

	if (m_vecFacingPoint!=vec3_origin && gpGlobals->curtime > m_fStopFacingPointTime)
	{
		m_vecFacingPoint = vec3_origin;
	}

	if (IsInhabited() && ShouldPredict())
	{	
		g_fMarinePoisonDuration = m_fPoison;		
		m_fPoison -= gpGlobals->frametime;
	}

	TickEmotes(deltatime);
	TickRedName(deltatime);

	UpdateFireEmitters();
	UpdateJumpJetEffects();
	UpdateElectrifiedArmor();

	//SetNextClientThink( gpGlobals->curtime + 0.1f );
	m_LastThinkTime = gpGlobals->curtime;
}

void C_ASW_Marine::DoWaterRipples()
{
	Vector knee = GetAbsOrigin() + Vector(0, 0, 12);
	if ( enginetrace->GetPointContents( knee ) & MASK_WATER )
	{
		// spawn water ripples if we're in water
		//run splash
		CEffectData data;

		//trace up from foot position to the water surface
		trace_t tr;
		Vector origin = GetAbsOrigin();
		Vector vecTrace(0,0,1024);
		UTIL_TraceLine( origin, origin + vecTrace, MASK_WATER, NULL, COLLISION_GROUP_NONE, &tr );
		if ( tr.fractionleftsolid )
		{
			data.m_vOrigin = origin + (vecTrace * tr.fractionleftsolid);
		}
		else
		{
			data.m_vOrigin = origin;
		}
		
		data.m_vNormal = Vector( 0,0,1 );
		data.m_flScale = random->RandomFloat( 4.0f, 5.0f ) * 2.0f;
		DispatchEffect( "aswwatersplash", data );
		//static Vector s_MarineWaterSplashColor( 0.5, 0.5, 0.5 );
		//FX_ASWWaterRipple(data.m_vOrigin, 1.0f, &s_MarineWaterSplashColor, 1.5f, 0.1f);
	}	
}

void C_ASW_Marine::SetClientsideVehicle(IASW_Client_Vehicle* pVehicle)
{
	m_pClientsideVehicle = pVehicle;

	m_bHasClientsideVehicle = (m_pClientsideVehicle != NULL);
}

void C_ASW_Marine::CreateWeaponEmitters()
{
	// flamethrower weapon handles its own effects now
	/*
	m_hFlameEmitter = CASWGenericEmitter::Create( "asw_emitter" );
	if ( m_hFlameEmitter.IsValid() )
	{			
		m_hFlameEmitter->UseTemplate("flamer5");
		m_hFlameEmitter->SetActive(false);
		m_hFlameEmitter->m_hCollisionIgnoreEntity = this;
		m_hFlameEmitter->SetCustomCollisionGroup(ASW_COLLISION_GROUP_IGNORE_NPCS);
		//m_hFlameEmitter->SetGlowMaterial("swarm/sprites/aswredglow2");
	}
	else
	{
		Warning("Failed to create a marine's flame emitter\n");
	}

	m_hFlameStreamEmitter = CASWGenericEmitter::Create( "asw_emitter" );
	if ( m_hFlameStreamEmitter.IsValid() )
	{			
		m_hFlameStreamEmitter->UseTemplate("flamerstream1");
		m_hFlameStreamEmitter->SetActive(false);
		m_hFlameStreamEmitter->m_hCollisionIgnoreEntity = this;
		m_hFlameStreamEmitter->SetCustomCollisionGroup(ASW_COLLISION_GROUP_IGNORE_NPCS);
		//m_hFlameEmitter->SetGlowMaterial("swarm/sprites/aswredglow2");
	}
	else
	{
		Warning("Failed to create a marine's flame stream emitter\n");
	}*/

	// fire extinguisher weapons now handle their own effects
	/*
	m_hFireExtinguisherEmitter = CASWGenericEmitter::Create( "asw_emitter" );
	if ( m_hFireExtinguisherEmitter.IsValid() )
	{			
		m_hFireExtinguisherEmitter->UseTemplate("fireextinguisher2");
		m_hFireExtinguisherEmitter->SetActive(false);
		m_hFireExtinguisherEmitter->m_hCollisionIgnoreEntity = this;
	}
	else
	{
		Warning("Failed to create a marine's fire extinguisher emitter\n");
	}
	*/
}

/*
void C_ASW_Marine::CreateHealEmitter()
{
	m_hHealEmitter = CASWGenericEmitter::Create( "asw_emitter" );

	if ( m_hHealEmitter.IsValid() )
	{			
		m_hHealEmitter->UseTemplate("healfast");
		m_hHealEmitter->SetActive(false);
	}
	else
	{
		Warning("Failed to create a marine's heal emitter\n");
	}
}
*/

void C_ASW_Marine::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );
	
	if ( updateType == DATA_UPDATE_CREATED )
	{
		CreateWeaponEmitters();
		//CreateHealEmitter();
		CreateShoulderCone();

		// We want to think every frame.
		SetNextClientThink( CLIENT_THINK_ALWAYS );
		return;
	}

	if ( m_bClientSideRagdoll && m_pClientsideRagdoll )
	{
		ASWGameRules()->m_hMarineDeathRagdoll = m_pClientsideRagdoll;
	}

	if ( m_iOldHealth != m_iHealth )
	{
		if ( m_iOldHealth < m_iHealth )
		{
			m_fLastHealTime = gpGlobals->curtime;
		}

		UpdateDamageEffects( true );

		m_iOldHealth = m_iHealth;
	}

	bool bNoDraw = IsEffectActive( EF_NODRAW ); 
	if ( bNoDraw != m_bLastNoDraw )
	{
		m_bLastNoDraw = bNoDraw;

		// give weapons a chance to update visibility as I'm hidden/shown
		for ( int i = 0; i < WeaponCount(); i++ )
		{
			C_BaseCombatWeapon	*pWeapon = GetWeapon( i );
			if ( pWeapon )
			{
				pWeapon->UpdateVisibility();
			}
		}
	}
	UpdateFireEmitters();
}

// note: this function doesn't seem to be used in sp. Maybe just in a netgame?
void C_ASW_Marine::ProcessMuzzleFlashEvent()
{
	Warning("[C] marine ProcessMuzzleFlashEvent\n");
	C_ASW_Weapon *pWeapon = GetActiveASWWeapon();

	if ( !pWeapon )
		return;
	/*
	Vector vector;
	QAngle angles;
	
	int iAttachment = LookupAttachment( "muzzle_flash" );

	if ( iAttachment >= 0 )
	{
		bool bFoundAttachment = GetAttachment( iAttachment, vector, angles );
		// If we have an attachment, then stick a light on it.
		if ( bFoundAttachment )
		{
			dlight_t *el = effects->CL_AllocDlight( LIGHT_INDEX_MUZZLEFLASH + index );
			el->origin = vector;
			el->radius = 70; 
			el->decay = el->radius / 0.05f;
			el->die = gpGlobals->curtime + 0.05f;
			el->color.r = 255;
			el->color.g = 192;
			el->color.b = 64;
			el->color.exponent = 5;

			int shellType = GetShellForAmmoType( pWeapon->GetCSWpnData().szAmmo1 );
			
			QAngle playerAngle = EyeAngles();
			Vector vForward, vRight, vUp;

			AngleVectors( playerAngle, &vForward, &vRight, &vUp );

			QAngle angVelocity;
			Vector vVel = vRight * 100 + vUp * 20;
			VectorAngles( vVel, angVelocity );

			tempents->CSEjectBrass( vector, angVelocity, 120, shellType, this  );
		}
	}
	*/
	pWeapon->OnMuzzleFlashed();
	if (pWeapon->ShouldMarineFlame())
	{
		m_fFlameTime = gpGlobals->curtime + pWeapon->GetFireRate() + 0.02f;
		return;
	}
	else if (pWeapon->ShouldMarineFireExtinguish())
	{
		m_fFireExtinguisherTime = gpGlobals->curtime + pWeapon->GetFireRate() + 0.02f;
		return;
	}


	// attach muzzle flash particle system effect
	int iAttachment = pWeapon->GetMuzzleAttachment();		
	if ( iAttachment > 0 )
	{
#ifndef _DEBUG
		float flScale = pWeapon->GetMuzzleFlashScale();				
		if (pWeapon->GetMuzzleFlashRed())
		{			
			FX_ASW_RedMuzzleEffectAttached( flScale, pWeapon->GetRefEHandle(), iAttachment, NULL, false );
		}
		else
		{
			FX_ASW_MuzzleEffectAttached( flScale, pWeapon->GetRefEHandle(), iAttachment, NULL, false );
		}
#endif
	}
}

C_ASW_Weapon* C_ASW_Marine::GetActiveASWWeapon( void ) const
{
	return static_cast<C_ASW_Weapon*>( GetActiveWeapon() );
}

void C_ASW_Marine::DoAnimationEvent( PlayerAnimEvent_t event )
{
	MDLCACHE_CRITICAL_SECTION();
	m_PlayerAnimState->DoAnimationEvent( event );
}


void C_ASW_Marine::FireEvent( const Vector& origin, const QAngle& angles, int event, const char *options )
{
	if ( GetCurrentMeleeAttack() )
	{
		if ( !GetCurrentMeleeAttack()->AllowNormalAnimEvent( this, event ) )
			return;
	}
	//Msg("event = %d\n", event);
	if ( event == AE_ASW_FOOTSTEP || event == AE_MARINE_FOOTSTEP )
	{
		Vector vel;
		EstimateAbsVelocity( vel );
		surfacedata_t *pSurface = GetGroundSurface();
		//Msg("Footstep t=%f velsize=%f\n", gpGlobals->curtime, vel.Length());
		if (pSurface)
		{
			//ASWGameMovement()->PlantFootprint( pSurface );
			MarineFootprintFX( pSurface, vel );
			MarineStepSound( pSurface, GetAbsOrigin(), vel );
		}
		return;
	}
	else if (event == AE_MARINE_RELOAD_SOUND_A)
	{
		if (GetActiveASWWeapon())
			GetActiveASWWeapon()->ASWReloadSound(0);
	}
	else if (event == AE_MARINE_RELOAD_SOUND_B)
	{
		if (GetActiveASWWeapon())
			GetActiveASWWeapon()->ASWReloadSound(1);
	}
	else if (event == AE_MARINE_RELOAD_SOUND_C)
	{
		if (GetActiveASWWeapon())
			GetActiveASWWeapon()->ASWReloadSound(2);
	}
	else if (event == AE_NPC_BODYDROP_HEAVY)
	{
		// play a sound of him dropping to the floor
		if ( GetFlags() & FL_ONGROUND )
		{
			EmitSound( "AI_BaseNPC.BodyDrop_Heavy" );
		}
		return;
	}
	else if ( event == AE_SCREEN_SHAKE )
	{
		// TODO: Fix
		//ASW_ShakeAnimEvent( this, options );
	}
	BaseClass::FireEvent(origin, angles, event, options);
}

void C_ASW_Marine::MarineStepSound( surfacedata_t *psurface, const Vector &vecOrigin, const Vector &vecVelocity )
{
	int	fWalking;
	float fvol;
	Vector knee;
	Vector feet;
	float height;
	float speed;
	float velrun;
	float velwalk;
	float flduck;
	int	fLadder;

	if ( GetFlags() & (FL_FROZEN|FL_ATCONTROLS))
		return;

	if ( GetMoveType() == MOVETYPE_NOCLIP || GetMoveType() == MOVETYPE_OBSERVER )
		return;

	speed = VectorLength( vecVelocity );
	float groundspeed = Vector2DLength( vecVelocity.AsVector2D() );

	// determine if we are on a ladder
	fLadder = ( GetMoveType() == MOVETYPE_LADDER );

	// UNDONE: need defined numbers for run, walk, crouch, crouch run velocities!!!!	
	if ( ( GetFlags() & FL_DUCKING) || fLadder )
	{
		velwalk = 60;		// These constants should be based on cl_movespeedkey * cl_forwardspeed somehow
		velrun = 80;		
		flduck = 100;
	}
	else
	{
		velwalk = 90;
		velrun = 220;
		flduck = 0;
	}

	bool onground = true; //( GetFlags() & FL_ONGROUND );
	bool movingalongground = ( groundspeed > 0.0f );
	bool moving_fast_enough =  ( speed >= velwalk );

	// To hear step sounds you must be either on a ladder or moving along the ground AND
	// You must be moving fast enough
	//Msg("og=%d ma=%d mf=%d\n", onground, movingalongground, moving_fast_enough);
	if ( !moving_fast_enough || !(fLadder || ( onground && movingalongground )) )
			return;

//	MoveHelper()->PlayerSetAnimation( PLAYER_WALK );

	fWalking = speed < velrun;		

	VectorCopy( vecOrigin, knee );
	VectorCopy( vecOrigin, feet );

	height = 72.0f; // bad

	knee[2] = vecOrigin[2] + 0.2 * height;

	// find out what we're stepping in or on...
	if ( fLadder )
	{
		psurface = physprops->GetSurfaceData( physprops->GetSurfaceIndex( "ladder" ) );
		fvol = 0.5;
	}
	else if ( enginetrace->GetPointContents( knee ) & MASK_WATER )
	{		
		static int iSkipStep = 0;
		//DoWaterRipples();

		if ( iSkipStep == 0 )
		{
			iSkipStep++;
			return;
		}

		if ( iSkipStep++ == 3 )
		{
			iSkipStep = 0;
		}
		psurface = physprops->GetSurfaceData( physprops->GetSurfaceIndex( "wade" ) );
		
		/*
		static Vector s_MarineWaterSplashColor( 0.5, 0.5, 0.5 );
		trace_t trace;
		Vector vecStart = GetAbsOrigin() + Vector(0, 0, 60);
		Vector vecEnd = GetAbsOrigin() + Vector(0, 0, 1);;
		UTIL_TraceLine(vecStart, vecEnd, MASK_WATER, NULL, COLLISION_GROUP_NONE, &trace);
		if( trace.fraction < 1 && !trace.startsolid)
		{
			FX_WaterRipple(trace.endpos, 1.0f, &s_MarineWaterSplashColor, 1.5f, 1.0f);
		}
		*/
		
		fvol = 0.65;
	}
	else if ( enginetrace->GetPointContents( feet ) & MASK_WATER )
	{
		psurface = physprops->GetSurfaceData( physprops->GetSurfaceIndex( "water" ) );
		fvol = fWalking ? 0.2 : 0.5;		
	}
	else
	{
		if ( !psurface )
			return;

		switch ( psurface->game.material )
		{
		default:
		case CHAR_TEX_CONCRETE:						
			fvol = fWalking ? 0.2 : 0.5;
			break;

		case CHAR_TEX_METAL:	
			fvol = fWalking ? 0.4 : 1.0;
			break;

		case CHAR_TEX_DIRT:
			fvol = fWalking ? 0.25 : 0.55;
			break;

		case CHAR_TEX_VENT:	
			fvol = fWalking ? 0.4 : 0.7;
			break;

		case CHAR_TEX_GRATE:
			fvol = fWalking ? 0.2 : 0.5;
			break;

		case CHAR_TEX_TILE:	
			fvol = fWalking ? 0.2 : 0.5;
			break;

		case CHAR_TEX_SLOSH:
			fvol = fWalking ? 0.2 : 0.5;
			break;
		}
	}

	// play the sound
	// 65% volume if ducking
	if ( GetFlags() & FL_DUCKING )
	{
		fvol *= 0.65;
	}

	PlayStepSound( feet, psurface, fvol, false );
}

const char *C_ASW_Marine::GetMarineFootprintParticleName( surfacedata_t *psurface )
{
	switch ( psurface->game.material )
	{
		case CHAR_TEX_SNOW:	return "footstep_snow"; break;
	}

	return "footstep_snow";
}

#define PLAYER_HALFWIDTH 12
void C_ASW_Marine::MarineFootprintFX( surfacedata_t *psurface, const Vector &vecVelocity )
{
	// note, velocity is not used yet, will use it to determine how big of an effect the footprint will have eventually
	if ( !psurface )
		return;

	bool bPlantFootprint = false;

	switch ( psurface->game.material )
	{
		case CHAR_TEX_SNOW:	bPlantFootprint = true; break;
	}

	if ( bPlantFootprint )
	{
		Vector right;
		AngleVectors( GetAbsAngles(), 0, &right, 0 );

		// Figure out where the top of the stepping leg is 
		trace_t tr;
		Vector hipOrigin;
		VectorMA( GetAbsOrigin(), 
			m_bIsFootprintOnLeft ? -PLAYER_HALFWIDTH : PLAYER_HALFWIDTH,
			right, hipOrigin );

		// Find where that leg hits the ground
		UTIL_TraceLine( hipOrigin, hipOrigin + Vector(0, 0, -COORD_EXTENT * 1.74), 
			MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr);

		// Splat a decal
		//CPVSFilter filter( tr.endpos );
		//te->FootprintDecal( filter, 0.0f, &tr.endpos, &right, tr.GetEntityIndex(), 
		//	10, marine->m_chTextureType ); //gDecals[footprintDecal].index

		DispatchParticleEffect( GetMarineFootprintParticleName( psurface ), tr.endpos, GetAbsAngles() );
	}

	// Switch feet for next time
	m_bIsFootprintOnLeft = !m_bIsFootprintOnLeft;
}

surfacedata_t* C_ASW_Marine::GetGroundSurface()
{
	//
	// Find the name of the material that lies beneath the player.
	//
	Vector start, end;
	VectorCopy( GetAbsOrigin() + Vector(0,0,1), start );
	VectorCopy( start, end );

	// Straight down
	end.z -= 38; // was 64

	// Fill in default values, just in case.
	
	Ray_t ray;
	ray.Init( start, end, GetCollideable()->OBBMins(), GetCollideable()->OBBMaxs() );

	trace_t	trace;
	UTIL_TraceRay( ray, MASK_PLAYERSOLID, this, COLLISION_GROUP_PLAYER_MOVEMENT, &trace );

	if ( trace.fraction == 1.0f )
		return NULL;	// no ground
	
	return physprops->GetSurfaceData( trace.surface.surfaceProps );
}

void C_ASW_Marine::PlayStepSound( Vector &vecOrigin, surfacedata_t *psurface, float fvol, bool force )
{
	if ( !psurface )
		return;

	unsigned short stepSoundName = m_bStepSideLeft ? psurface->sounds.runStepLeft : psurface->sounds.runStepRight;
	m_bStepSideLeft = !m_bStepSideLeft;

	if ( !stepSoundName )
		return;

	IPhysicsSurfaceProps *physprops = MoveHelper( )->GetSurfaceProps();
	const char *pSoundName = physprops->GetString( stepSoundName );
	CSoundParameters params;
	if ( !CBaseEntity::GetParametersForSound( pSoundName, params, NULL ) )
		return;

	CLocalPlayerFilter filter;

	EmitSound_t ep;
	ep.m_nChannel = CHAN_BODY;
	ep.m_pSoundName = params.soundname;
	ep.m_flVolume = ( asw_override_footstep_volume.GetBool() ) ? fvol : params.volume;
	ep.m_SoundLevel = params.soundlevel;
	ep.m_nFlags = 0;
	ep.m_nPitch = params.pitch;
	ep.m_pOrigin = &vecOrigin;

	EmitSound( filter, entindex(), ep );
}

bool C_ASW_Marine::IsAnimatingReload()
{
	return m_PlayerAnimState->IsAnimatingReload();
}

bool C_ASW_Marine::IsDoingEmoteGesture()
{
	return m_PlayerAnimState->IsDoingEmoteGesture();
}

void C_ASW_Marine::PostThink()
{

}

//-----------------------------------------------------------------------------
// Purpose: Creates, destroys, and updates the flashlight effect as needed.
//-----------------------------------------------------------------------------
void C_ASW_Marine::UpdateFlashlight()
{
	// The dim light is the flashlight.
#if ASW_PROJECTOR_FLASHLIGHT
	if ( IsEffectActive( EF_DIMLIGHT ) )
	{
		if ( !m_pFlashlight )
		{
			// Turned on the headlight; create it.
			m_pFlashlight = new CFlashlightEffect(index);

			if ( !m_pFlashlight )
				return;

			m_pFlashlight->TurnOn();
		}
#ifdef ASW_FLASHLIGHT_DLIGHT
		if ( !m_pFlashlightDLight || (m_pFlashlightDLight->key != index) )
		{
			m_pFlashlightDLight = effects->CL_AllocDlight ( index );
		}
#endif

		//m_fAmbientLight = asw_flashlight_marine_ambient.GetFloat();
		//m_fLightingScale = asw_flashlight_marine_lightscale.GetFloat();

		Vector vecForward, vecRight, vecUp;

		if (m_pFlashlightDLight)
		{
			AngleVectors( GetLocalAngles(), &vecForward, &vecRight, &vecUp );
			m_pFlashlightDLight->origin = GetAbsOrigin() + vecForward * asw_flashlight_dlight_offsetx.GetFloat()
						+ vecRight * asw_flashlight_dlight_offsety.GetFloat() + vecUp * asw_flashlight_dlight_offsetz.GetFloat();			
			m_pFlashlightDLight->color.r = asw_flashlight_dlight_r.GetInt();
			m_pFlashlightDLight->color.g = asw_flashlight_dlight_g.GetInt();
			m_pFlashlightDLight->color.b = asw_flashlight_dlight_b.GetInt();
			m_pFlashlightDLight->radius = asw_flashlight_dlight_radius.GetFloat();
			//m_pFlashlightDLight->color.exponent = 5;
			//m_pFlashlightDLight->decay = 0;
			m_pFlashlightDLight->die = gpGlobals->curtime + 30.0f;
		}

		
		C_ASW_Player *pPlayer = GetCommander();
		QAngle angFlashlight;
		if ( pPlayer && C_BasePlayer::IsLocalPlayer(pPlayer) && pPlayer->GetMarine() == this && GetHealth() > 0 )
		{
			angFlashlight = pPlayer->EyeAnglesWithCursorRoll();	// z component of eye angles holds the distance of the cursor from the marine
			angFlashlight.x = angFlashlight.z;
			angFlashlight.z = 0;
			if (angFlashlight.x > 42)
				angFlashlight.x = 42;
		}
		else
		{
			angFlashlight = ASWEyeAngles();
			angFlashlight.x = 20;	// tilt the flashlight down by default
		}
		AngleVectors( angFlashlight, &vecForward, &vecRight, &vecUp );

		// Update the light with the new position and direction.
		// asw fixme: change FLASHLIGHT_DISTANCE to a trace out ahead?
		//   hm, distance is only used for the old flashlight (which was just a dlight?)

		Vector vecFlashLightPos = EyePosition() + vecForward * 30.0f;
		// projector light:
		m_pFlashlight->UpdateLight( entindex(), vecFlashLightPos, vecForward, vecRight, vecUp, 0.0f, 0.0f, 0.0f, false, NULL );
	}
	else
	{
		if (m_pFlashlight)
		{
			// Turned off the flashlight; delete it.			
			m_pFlashlight->TurnOff();
			delete m_pFlashlight;
			m_pFlashlight = NULL;
		}
		if (m_pFlashlightDLight)
		{			
			m_pFlashlightDLight->die = gpGlobals->curtime + 0.001;
			m_pFlashlightDLight = NULL;
		}
		//m_fAmbientLight = asw_marine_ambient.GetFloat();
		//m_fLightingScale = asw_marine_lightscale.GetFloat();
	}
#else
	// beam flashlight
	if ( IsEffectActive( EF_DIMLIGHT ) )
	{
		// get eye angles
		Vector vecForward, vecRight, vecUp;
		QAngle angFlashlight;

		/*C_ASW_Player *pPlayer = ToASW_Player( C_BasePlayer::GetLocalPlayer() );
		if ( pPlayer && pPlayer->GetMarine() == this && GetHealth() > 0 )
		{
			angFlashlight = pPlayer->EyeAnglesWithCursorRoll();	// z component of eye angles holds the distance of the cursor from the marine
			angFlashlight.x = angFlashlight.z;
			angFlashlight.z = 0;
			if (angFlashlight.x > 42)
				angFlashlight.x = 42;
		}
		else*/
		{
			angFlashlight = ASWEyeAngles();
			//angFlashlight.x = 20;	// tilt the flashlight down by default
		}
		AngleVectors( angFlashlight, &vecForward, &vecRight, &vecUp );

		int iAttachment = LookupAttachment( "muzzle_flash" );

		if ( iAttachment < 0 )
			return;

		Vector vecOrigin;
		QAngle dummy;
		GetAttachment( iAttachment, vecOrigin, dummy );
		vecOrigin = EyePosition();
			
		trace_t tr;
		UTIL_TraceLine( vecOrigin, vecOrigin + (vecForward * 200), MASK_SHOT, this, COLLISION_GROUP_NONE, &tr );

		if( !m_pFlashlightBeam )
		{
			BeamInfo_t beamInfo;
			beamInfo.m_nType = TE_BEAMPOINTS;
			beamInfo.m_vecStart = tr.startpos;
			beamInfo.m_vecEnd = tr.endpos;
			beamInfo.m_pszModelName = "sprites/glow01.vmt";
			beamInfo.m_pszHaloName = "sprites/glow01.vmt";
			beamInfo.m_flHaloScale = 3.0;
			beamInfo.m_flWidth = 8.0f;
			beamInfo.m_flEndWidth = 55.0f;
			beamInfo.m_flFadeLength = 300.0f;
			beamInfo.m_flAmplitude = 0;
			beamInfo.m_flBrightness = 60.0;
			beamInfo.m_flSpeed = 0.0f;
			beamInfo.m_nStartFrame = 0.0;
			beamInfo.m_flFrameRate = 0.0;
			beamInfo.m_flRed = 255.0;
			beamInfo.m_flGreen = 255.0;
			beamInfo.m_flBlue = 255.0;
			beamInfo.m_nSegments = 8;
			beamInfo.m_bRenderable = true;
			beamInfo.m_flLife = 0.5;
			beamInfo.m_nFlags = FBEAM_FOREVER | FBEAM_ONLYNOISEONCE | FBEAM_NOTILE | FBEAM_HALOBEAM;
			
			m_pFlashlightBeam = beams->CreateBeamPoints( beamInfo );
		}

		if( m_pFlashlightBeam )
		{
			BeamInfo_t beamInfo;
			beamInfo.m_vecStart = tr.startpos;
			beamInfo.m_vecEnd = tr.endpos;
			beamInfo.m_flRed = 255.0;
			beamInfo.m_flGreen = 255.0;
			beamInfo.m_flBlue = 255.0;

			beams->UpdateBeamInfo( m_pFlashlightBeam, beamInfo );

			//dlight_t *el = effects->CL_AllocDlight( 0 );
			//el->origin = tr.endpos;
			//el->radius = 50; 
			//el->color.r = 200;
			//el->color.g = 200;
			//el->color.b = 200;
			//el->die = gpGlobals->curtime + 0.1;
		}
	}
	else if ( m_pFlashlightBeam )
	{
		ReleaseFlashlightBeam();
	}
#endif
}

bool C_ASW_Marine::CreateLightEffects( void )
{
	// we ignore the normal dlight created for the EF_ flags
	// as we use proper flashlights
	return false;
}

bool C_ASW_Marine::Simulate()
{
	UpdateFlashlight();
	
	BaseClass::Simulate();
	return true;
}

void C_ASW_Marine::EstimateAbsVelocity( Vector& vel )
{
	if (ShouldPredict())
	{
		vel = GetAbsVelocity();
		return;
	}
	BaseClass::EstimateAbsVelocity(vel);
}

void C_ASW_Marine::StartFlamerLoop()
{
	if ( m_pFlamerLoopSound )
		return;

	CPASAttenuationFilter filter( this );
	CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();
	m_pFlamerLoopSound = controller.SoundCreate( filter, entindex(), "ASW_Weapon_Flamer.FlameLoop" );
	CSoundEnvelopeController::GetController().Play( m_pFlamerLoopSound, 1.0, 100 );
	//CSoundEnvelopeController::GetController().SoundChangeVolume( m_pEngineSound1, 0.7, 2.0 );
}

void C_ASW_Marine::StopFlamerLoop()
{
	if ( m_pFlamerLoopSound )
	{
		//Msg("Ending flamer loop!\n");
		CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();
		controller.SoundDestroy( m_pFlamerLoopSound );
		m_pFlamerLoopSound = NULL;
		EmitSound("ASW_Weapon_Flamer.FlameStop");
	}
}

void C_ASW_Marine::StartFireExtinguisherLoop()
{
	if ( m_pFireExtinguisherLoopSound )
		return;

	CPASAttenuationFilter filter( this );
	CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();
	m_pFireExtinguisherLoopSound = controller.SoundCreate( filter, entindex(), "ASW_Extinguisher.OnLoop" );
	CSoundEnvelopeController::GetController().Play( m_pFireExtinguisherLoopSound, 1.0, 100 );
}

void C_ASW_Marine::StopFireExtinguisherLoop()
{
	if ( m_pFireExtinguisherLoopSound )
	{
		//Msg("Ending fire extinguisher loop!\n");
		CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();
		controller.SoundDestroy( m_pFireExtinguisherLoopSound );
		m_pFireExtinguisherLoopSound = NULL;
		EmitSound( "ASW_Extinguisher.Stop" );
	}
}


void C_ASW_Marine::DoMuzzleFlash()
{
	return;	 // asw - muzzle flashes are triggered by tracer usermessages instead to save bandwidth

	// Our weapon takes our muzzle flash command
	C_BaseCombatWeapon *pWeapon = GetActiveWeapon();
	if ( pWeapon )
	{
		pWeapon->DoMuzzleFlash();
		//NOTENOTE: We do not chain to the base here
	}
	else
	{
		BaseClass::DoMuzzleFlash();
	}
}

void C_ASW_Marine::CreateSentryBuildDisplay()
{
	if ( m_hSentryBuildDisplay )
	{
		ParticleProp()->StopEmissionAndDestroyImmediately( m_hSentryBuildDisplay );
	}

	m_hSentryBuildDisplay = ParticleProp()->Create( "sentry_build_display", PATTACH_ABSORIGIN_FOLLOW, -1, Vector( 0, 0, 8 ) );
	SetSentryBuildDisplayEnabled( true );
}

void C_ASW_Marine::SetSentryBuildDisplayEnabled( bool state )
{
	if ( !m_hSentryBuildDisplay )
		CreateSentryBuildDisplay();

	if ( m_hSentryBuildDisplay )
	{
		if ( state == true )
			m_hSentryBuildDisplay->SetControlPoint( 5, Vector( 10, 124, 203 ) );
		else
			m_hSentryBuildDisplay->SetControlPoint( 5, Vector( 240, 40, 40 ) );
	}
}

void C_ASW_Marine::DestroySentryBuildDisplay()
{
	if ( m_hSentryBuildDisplay )
	{
		ParticleProp()->StopEmissionAndDestroyImmediately( m_hSentryBuildDisplay );
		m_hSentryBuildDisplay = NULL;
	}
}

void C_ASW_Marine::UpdateDamageEffects( bool bHealthChanged )
{
	//bool bHealthChanged = m_iLastHealth != GetHealth();

	//if ( bHealthChanged )
	//	m_iLastHealth = GetHealth();

	float flHealthFraction = HealthFraction();

	if ( flHealthFraction < 0.35f && !m_bKnockedOut )
	{
		if ( bHealthChanged )
		{
			// Critical health
			if ( !m_hCriticalHeathEffect )
			{
				m_hCriticalHeathEffect = ParticleProp()->Create( "criticalHealth", PATTACH_ABSORIGIN_FOLLOW, -1, Vector( 0, 0, 50 ) );
			}

			if ( m_hLowHeathEffect )
			{
				m_hLowHeathEffect->StopEmission(false, false, true);
				m_hLowHeathEffect = NULL;
			}
		}

		if ( m_flNextDamageSparkTime < gpGlobals->curtime )
		{
			// spark at slightly random intervals
			float flSparkRate = asw_damage_spark_rate.GetFloat();
			EmitSound( "SuitDamage.Sparks" );
			ParticleProp()->Create( "sparks_heavy_master", PATTACH_ABSORIGIN_FOLLOW, -1, Vector( 0, 0, 50 ) );
			m_flNextDamageSparkTime = gpGlobals->curtime + flSparkRate + flSparkRate * RandomFloat( 0.0f, 2.0f );
		}
	}
	else if ( flHealthFraction < 0.65f && !m_bKnockedOut )
	{
		if ( bHealthChanged )
		{
			// Low health
			if ( !m_hLowHeathEffect )
			{
				m_hLowHeathEffect = ParticleProp()->Create( "lowHealth", PATTACH_ABSORIGIN_FOLLOW, -1, Vector( 0, 0, 50 ) );
			}

			if ( m_hCriticalHeathEffect )
			{
				m_hCriticalHeathEffect->StopEmission(false, false , true);
				m_hCriticalHeathEffect = NULL;
			}
		}

		if ( m_flNextDamageSparkTime < gpGlobals->curtime )
		{
			// spark at slightly random intervals
			float flSparkRate = asw_damage_spark_rate.GetFloat() * 3.0f;
			EmitSound( "SuitDamage.Sparks" );
			ParticleProp()->Create( "sparks_light_master", PATTACH_ABSORIGIN_FOLLOW, -1, Vector( 0, 0, 50 ) );
			m_flNextDamageSparkTime = gpGlobals->curtime + flSparkRate + flSparkRate * RandomFloat( 0.0f, 2.0f );
		}
	}
	else
	{
		// Safe health
		if ( m_hLowHeathEffect )
		{
			m_hLowHeathEffect->StopEmission(false, false, true);
			m_hLowHeathEffect = NULL;
		}

		if ( m_hCriticalHeathEffect )
		{
			m_hCriticalHeathEffect->StopEmission(false, false, true);
			m_hCriticalHeathEffect = NULL;
		}
	}

}

void C_ASW_Marine::DoImpactEffect( trace_t &tr, int nDamageType )
{
	return;	
}

void C_ASW_Marine::UpdateHeartbeat()
{
	if ( !ShouldPredict() || !GetMarineResource() )
		return;

	if (GetHealth() > 0 && GetMarineResource()->GetHealthPercent() < 0.6f)
	{
		if (!ASWGameRules() || ASWGameRules()->GetGameState() != ASW_GS_INGAME)
			return;
		if (gpGlobals->curtime > m_fNextHeartbeat)
		{
			float fHeartVolume = GetMarineResource()->GetHealthPercent() / 0.6f;
			fHeartVolume = (1.0f - fHeartVolume); // * 3.0f;

			CLocalPlayerFilter filter;

			EmitSound_t ep;
			ep.m_nChannel = CHAN_AUTO;	// CHAN_STATIC ?
			ep.m_pSoundName = "Misc.Heartbeat";
			ep.m_flVolume = fHeartVolume;
			//ep.m_nPitch = PITCH_NORM;
			ep.m_SoundLevel = SNDLVL_NORM;
			//ep.m_pOrigin = &GetAbsOrigin();
			//Msg("heart volume = %f\n", fHeartVolume);

			//while (ep.m_flVolume > 0)
			//{
				//EmitSound(filter, entindex(), ep);
				//ep.m_flVolume -= 1.0f;
			//}
			C_BaseEntity::EmitSound( filter, SOUND_FROM_LOCAL_PLAYER, ep ); 

			// 1.2 when on 50 health
			// 0.8 when on 0
			float fHeartDelay = 0.8 + (GetMarineResource()->GetHealthPercent() / 0.6f) * 0.4;

			m_fNextHeartbeat = gpGlobals->curtime + fHeartDelay;
		}
	}
}

const Vector& C_ASW_Marine::GetFacingPoint()
{
	if (m_vecFacingPointFromServer != vec3_origin)
	{
		return m_vecFacingPointFromServer;
	}

	return m_vecFacingPoint;
}

IASW_Client_Vehicle* C_ASW_Marine::GetASWVehicle()
{
	return dynamic_cast<IASW_Client_Vehicle*>(m_hASWVehicle.Get());
}

// player has the mouse over something, check if we want to offer an interaction with that
void C_ASW_Marine::MouseOverEntity( C_BaseEntity* pEnt, Vector vecCrosshairAimingPos )
{
	if (!GetCommander() || !IsInhabited())
		return;
	
	if (!IsAlive())
		return;
	C_ASW_Weapon* pWeapon = GetActiveASWWeapon();
	if (pWeapon)
	{
		pWeapon->MouseOverEntity( pEnt, vecCrosshairAimingPos );
	}
	// asw temp
	//ASWInput()->SetHighlightEntity(pEnt);
}

void C_ASW_Marine::NotifyShouldTransmit( ShouldTransmitState_t state )
{
	// Remove all addon models if we go out of the PVS.
	if ( state == SHOULDTRANSMIT_END )
	{
		if( m_pFlashlightBeam != NULL )
		{
			ReleaseFlashlightBeam();
		}
	}

	BaseClass::NotifyShouldTransmit( state );
}

void C_ASW_Marine::ReleaseFlashlightBeam( void )
{
	if( m_pFlashlightBeam )
	{
		m_pFlashlightBeam->flags = 0;
		m_pFlashlightBeam->die = gpGlobals->curtime - 1;

		m_pFlashlightBeam = NULL;
	}
}

// EF_NODRAW isn't preventing the marine from being drawn, strangely.  So we do a visible check here before drawing.
int C_ASW_Marine::DrawModel( int flags, const RenderableInstance_t &instance )
{
	if (!IsVisible())
		return 0;	

	int iResult = BaseClass::DrawModel(flags, instance);

	m_vecLastRenderedPos = GetRenderOrigin();

	return iResult;
}

void C_ASW_Marine::SetPoisoned(float f)
{
	m_fPoison = f;
}

// IK the left hand?

bool FindWeaponAttachmentBone( C_BaseCombatWeapon *pWeapon, int &iWeaponBone )
{
	if ( !pWeapon )
		return false;

	CStudioHdr *pHdr = pWeapon->GetModelPtr();
	if ( !pHdr )
		return false;

	for ( iWeaponBone=0; iWeaponBone < pHdr->numbones(); iWeaponBone++ )
	{
		if ( stricmp( pHdr->pBone( iWeaponBone )->pszName(), "ValveBiped.Bip01_R_Hand" ) == 0 )	//L_Hand_Attach
			break;
	}

	return iWeaponBone != pHdr->numbones();
}

bool FindLeftHandAttachment( C_BaseCombatWeapon *pWeapon, Vector &vecHandPos, QAngle &angHandFacing )
{
	if (!pWeapon)
		return false;

	int iAttachment = pWeapon->LookupAttachment("L_Hand_Attach");
	if (iAttachment == -1)
		return false;


	return pWeapon->GetAttachment( iAttachment, vecHandPos, angHandFacing ); //GetAttachment( "L_Hand_Attach", vecHandPos, angHandFacing );
}

bool FindMyAttachmentBone( C_BaseAnimating *pModel, int &iBone, CStudioHdr *pHdr )
{
	if ( !pHdr )
		return false;

	for ( iBone=0; iBone < pHdr->numbones(); iBone++ )
	{
		if ( stricmp( pHdr->pBone( iBone )->pszName(), "Valvebiped.Bip01_L_Hand" ) == 0 )
			break;
	}

	return iBone != pHdr->numbones();
}

inline bool IsBoneChildOf( CStudioHdr *pHdr, int iBone, int iParent )
{
	if ( iBone == iParent )
		return false;

	while ( iBone != -1 )
	{
		if ( iBone == iParent )
			return true;
	
		iBone = pHdr->pBone( iBone )->parent;
	}
	return false;
}

void ApplyDifferenceTransformToChildren( 
	C_BaseAnimating *pModel,
	const matrix3x4_t &mSource,
	const matrix3x4_t &mDest,
	int iParentBone )
{
	CStudioHdr *pHdr = pModel->GetModelPtr();
	if ( !pHdr )
		return;

	// Build a matrix to go from mOriginalHand to mHand.
	// ( mDest * Inverse( mSource ) ) * mSource = mDest
	matrix3x4_t mSourceInverse, mToDest;
	MatrixInvert( mSource, mSourceInverse );
	ConcatTransforms( mDest, mSourceInverse, mToDest );

	// Now multiply iMyBone and all its children by mToWeaponBone.
	for ( int i=0; i < pHdr->numbones(); i++ )
	{
		if ( IsBoneChildOf( pHdr, i, iParentBone ) )
		{
			matrix3x4_t &mCur = pModel->GetBoneForWrite( i );
			matrix3x4_t mNew;
			ConcatTransforms( mToDest, mCur, mNew );
			mCur = mNew;
		}
	}
}


void GetCorrectionMatrices(
	const matrix3x4_t &mShoulder,
	const matrix3x4_t &mElbow,
	const matrix3x4_t &mHand,
	matrix3x4_t &mShoulderCorrection,
	matrix3x4_t &mElbowCorrection
	)
{
	// Get the positions of each node so we can get the direction vectors.
	Vector vShoulder, vElbow, vHand;
	MatrixPosition( mShoulder, vShoulder );
	MatrixPosition( mElbow, vElbow );
	MatrixPosition( mHand, vHand );

	// Get rid of the translation.
	matrix3x4_t mOriginalShoulder = mShoulder;
	matrix3x4_t mOriginalElbow = mElbow;
	MatrixSetColumn( Vector( 0, 0, 0 ), 3, mOriginalShoulder );
	MatrixSetColumn( Vector( 0, 0, 0 ), 3, mOriginalElbow );

	// Let the IK code align them like it would if we did IK on the joint.
	matrix3x4a_t mAlignedShoulder, mAlignedElbow;
	mAlignedShoulder = mOriginalShoulder;
	mAlignedElbow = mOriginalElbow;
	Studio_AlignIKMatrix( mAlignedShoulder, vElbow-vShoulder );
	Studio_AlignIKMatrix( mAlignedElbow, vHand-vElbow );

	// Figure out the transformation from the aligned bones to the original ones.
	matrix3x4a_t mInvAlignedShoulder, mInvAlignedElbow;
	MatrixInvert( mAlignedShoulder, mInvAlignedShoulder );
	MatrixInvert( mAlignedElbow, mInvAlignedElbow );

	ConcatTransforms( mInvAlignedShoulder, mOriginalShoulder, mShoulderCorrection );
	ConcatTransforms( mInvAlignedElbow, mOriginalElbow, mElbowCorrection );
}

void C_ASW_Marine::BuildTransformations( CStudioHdr *pHdr, Vector *pos, Quaternion q[], const matrix3x4_t& cameraTransform, int boneMask, CBoneBitList &boneComputed )
{
	// First, setup our model's transformations like normal.
	BaseClass::BuildTransformations( pHdr, pos, q, cameraTransform, boneMask, boneComputed );

	if ( !asw_left_hand_ik.GetInt() )
		return;

	// If our current weapon has a bone named L_Hand_Attach, then we attach the player's
	// left hand (Valvebiped.Bip01_L_Hand) to it.
	C_BaseCombatWeapon *pWeapon = GetActiveWeapon();

	if ( !pWeapon )
		return;

	// Have the weapon setup its bones.
	pWeapon->SetupBones( NULL, 0, BONE_USED_BY_ANYTHING, gpGlobals->curtime );

	int iWeaponBone = 0;
	Vector vHandTarget, vecHandFacing;
	QAngle angHandFacing;
	if ( FindWeaponAttachmentBone( pWeapon, iWeaponBone ) )
	//if ( FindLeftHandAttachment( pWeapon, vHandTarget, angHandFacing ) )
	{
		int iMyBone = 0;
		if ( FindMyAttachmentBone( this, iMyBone, pHdr ) )
		{
			int iHand = iMyBone;
			int iElbow = pHdr->pBone( iHand )->parent;
			int iShoulder = pHdr->pBone( iElbow )->parent;
			matrix3x4a_t *pBones = GetBoneArrayForWrite( );

			// Store off the original hand position.
			matrix3x4_t mSource = pBones[iHand];


			// Figure out the rotation offset from the current shoulder and elbow bone rotations
			// and what the IK code's alignment code is going to produce, because we'll have to
			// re-apply that offset after the IK runs.
			matrix3x4_t mShoulderCorrection, mElbowCorrection;
			GetCorrectionMatrices( pBones[iShoulder], pBones[iElbow], pBones[iHand], mShoulderCorrection, mElbowCorrection );


			// Do the IK solution.
			//Vector vHandTarget;
			MatrixPosition( pWeapon->GetBone( iWeaponBone ), vHandTarget );
			MatrixAngles( pWeapon->GetBone( iWeaponBone ), angHandFacing );
			AngleVectors( angHandFacing, &vecHandFacing );
			vHandTarget += vecHandFacing * 15;
			Studio_SolveIK( iShoulder, iElbow, iHand, vHandTarget, pBones );


			// Now reapply the rotation correction.
			matrix3x4_t mTempShoulder = pBones[iShoulder];
			matrix3x4_t mTempElbow = pBones[iElbow];
			ConcatTransforms( mTempShoulder, mShoulderCorrection, pBones[iShoulder] );
			ConcatTransforms( mTempElbow, mElbowCorrection, pBones[iElbow] );


			// Now apply the transformation on the hand to the fingers.
			matrix3x4_t &mDest = GetBoneForWrite( iHand );
			ApplyDifferenceTransformToChildren( this, mSource, mDest, iHand );
		}
	}
}

void C_ASW_Marine::UpdateFireEmitters()
{
	bool bOnFire = (m_bOnFire && !IsEffectActive(EF_NODRAW));
	if (bOnFire != m_bClientOnFire)
	{
		m_bClientOnFire = bOnFire;
		if (m_bClientOnFire)
		{
			if ( !m_pBurningEffect )
			{
				m_pBurningEffect = UTIL_ASW_CreateFireEffect( this );
			}
			EmitSound( "ASWFire.BurningFlesh" );
		}
		else
		{
			if ( m_pBurningEffect )
			{
				ParticleProp()->StopEmission( m_pBurningEffect );
				m_pBurningEffect = NULL;
			}
			StopSound("ASWFire.BurningFlesh");
			if ( C_BaseEntity::IsAbsQueriesValid() )
				EmitSound("ASWFire.StopBurning");
		}
	}
}

void C_ASW_Marine::UpdateOnRemove()
{
	BaseClass::UpdateOnRemove();
	m_bOnFire = false;
	UpdateFireEmitters();

	if ( m_pHealEmitter )
	{			
		ParticleProp()->StopEmission( m_pHealEmitter );
		m_pHealEmitter = NULL;
	}

	// flamethrower handles its own effects now
	/*
	if ( m_hFlameEmitter.IsValid() )
	{			
		m_hFlameEmitter->SetActive(false);
	}
	if ( m_hFlameStreamEmitter.IsValid() )
	{			
		m_hFlameStreamEmitter->SetActive(false);
	}
	*/
	// extinguisher weapons handles its own effects now
	/*
	if ( m_hFireExtinguisherEmitter.IsValid() )
	{			
		m_hFireExtinguisherEmitter->SetActive(false);
	}
	*/
	if (m_hShoulderCone.Get())
	{
		UTIL_Remove( m_hShoulderCone );
		m_hShoulderCone = NULL;
	}
}

// helper for movement code which will disable movement in controller mode if you're interacting and need those directionals
bool C_ASW_Marine::IsUsingComputerOrButtonPanel()
{
	return m_hUsingEntity.Get() && (
									dynamic_cast<C_ASW_Button_Area*>(m_hUsingEntity.Get()) ||
										(
											dynamic_cast<C_ASW_Computer_Area*>(m_hUsingEntity.Get()) &&
											!IsControllingTurret()
										)
		
									);
}

void C_ASW_Marine::CreateShoulderCone()
{	
	if (asw_marine_shoulderlight.GetInt()!=1)
		return;

	int iAttachment = LookupAttachment( "shoulderlight" );
	if ( iAttachment <= 0 )
	{
		Msg("error, couldn't find shoulderlight attachment for marine\n");
		return;
	}

	C_BaseAnimating *pEnt = new C_BaseAnimating;
	if (!pEnt)
	{
		Msg("Error, couldn't create new C_BaseAnimating\n");
		return;
	}
	if (!pEnt->InitializeAsClientEntity( "models/swarm/shouldercone/shouldercone.mdl", false ))
	{
		Msg("Error, couldn't InitializeAsClientEntity\n");
		pEnt->Release();
		return;
	}
	pEnt->SetParent( this, iAttachment );
	pEnt->SetLocalOrigin( Vector( 0, 0, 0 ) );
	pEnt->SetLocalAngles( QAngle( 0, 0, 0 ) );	
	pEnt->SetSolid( SOLID_NONE );
	pEnt->RemoveEFlags( EFL_USE_PARTITION_WHEN_NOT_SOLID );

	m_hShoulderCone = pEnt;
}

// when marine's health falls below this, name starts to blink red
#define MARINE_NAME_PULSE_THRESHOLD 0.5f
void C_ASW_Marine::TickRedName(float delta)
{
	if (!GetMarineResource())
		return;

	// deltatime should be normal regardless of slowmo
	float fTimeScale = GameTimescale()->GetCurrentTimescale();

 	delta *= ( 1.0f / fTimeScale );

	float fHealth = GetMarineResource()->GetHealthPercent();
	
	if ( fHealth > MARINE_NAME_PULSE_THRESHOLD || fHealth <= 0 )
	{
		m_fRedNamePulse -= delta * 2;	// take 0.5 seconds to fade completely to normal
	}
	else
	{
		float drate = ((MARINE_NAME_PULSE_THRESHOLD - fHealth) / 0.1f) + 2.0f;
		if (m_bRedNamePulseUp)
		{
			m_fRedNamePulse += drate * delta * 0.5f;
		}
		else
		{
			m_fRedNamePulse -= drate * delta * 0.5f;
		}
		// how quick should we pulse?  at 60, once per second  i.e. 2d
		// at 0, 4 times a second?  i.e. 8d
	}

	if (m_fRedNamePulse <= 0)
	{
		m_fRedNamePulse = 0;
		m_bRedNamePulseUp = true;
	}
	if (m_fRedNamePulse >= 1.0f)
	{
		m_fRedNamePulse= 1.0f;
		m_bRedNamePulseUp = false;
	}
}

void C_ASW_Marine::ImpactTrace( trace_t *pTrace, int iDamageType, char *pCustomImpactName )
{
	// do nothing
	// effects re handled in TraceAttack in the shared file
}



C_BaseAnimating *C_ASW_Marine::BecomeRagdollOnClient()
{
	C_BaseAnimating *pRagdoll = BaseClass::BecomeRagdollOnClient();
	if ( pRagdoll )
	{
		pRagdoll->ParticleProp()->Create( "marine_death_ragdoll", PATTACH_ABSORIGIN_FOLLOW );
	}
	return pRagdoll;
}

C_ASW_Marine* C_ASW_Marine::GetLocalMarine()
{
	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if ( !pPlayer )
		return NULL;

	return pPlayer->GetMarine();
}

void C_ASW_Marine::UpdateElectrifiedArmor()
{
	bool bElectrified = IsElectrifiedArmorActive();

	if ( bElectrified != m_bClientElectrifiedArmor )
	{
		bool bLocalPlayer = false;
		C_ASW_Player *pPlayer = GetCommander();
		C_ASW_Player *pLocalPlayer = C_ASW_Player::GetLocalASWPlayer();
		if ( pPlayer == pLocalPlayer && IsInhabited() )
			bLocalPlayer = true;

		m_bClientElectrifiedArmor = bElectrified;
		if ( m_bClientElectrifiedArmor )
		{
			EmitSound( "ASW_ElectrifiedSuit.TurnOn" );

			if ( bLocalPlayer )
				EmitSound( "ASW_ElectrifiedSuit.LoopFP" );
			else
				EmitSound( "ASW_ElectrifiedSuit.Loop" );


			if ( !m_pElectrifiedArmorEmitter )
			{
				m_pElectrifiedArmorEmitter = ParticleProp()->Create( "thorns_marine_buff", PATTACH_ABSORIGIN_FOLLOW, -1, Vector( 0, 0, 50 ) );
			}
		}
		else
		{
			if ( bLocalPlayer )
			{
				StopSound( "ASW_ElectrifiedSuit.LoopFP" );
				EmitSound( "ASW_ElectrifiedSuit.OffFP" );
			}
			else
			{
				StopSound( "ASW_ElectrifiedSuit.Loop" );
			}

			if ( m_pElectrifiedArmorEmitter )
			{
				m_pElectrifiedArmorEmitter->StopEmission(false, false, true);
				m_pElectrifiedArmorEmitter = NULL;
			}
		}	
	} 
}

void C_ASW_Marine::UpdateJumpJetEffects()
{
	if ( m_iJumpJetting == JJ_JUMP_JETS || m_iJumpJetting == JJ_CHARGE )
	{
		if ( !m_pJumpJetEffect[0] )
		{
			const char *pszEffect = "jj_trail_small";
			m_pJumpJetEffect[0] = ParticleProp()->Create( pszEffect, PATTACH_POINT_FOLLOW, "jump_jet_l" );
			m_pJumpJetEffect[1] = ParticleProp()->Create( pszEffect, PATTACH_POINT_FOLLOW, "jump_jet_r" );

			CLocalPlayerFilter filter;
			EmitSound( filter, entindex(), "ASW_JumpJet.Activate" );
			EmitSound( filter, entindex(), "ASW_JumpJet.Loop" );
		}
	}
	else if ( m_pJumpJetEffect[0] )
	{
		StopSound( "ASW_JumpJet.Loop" );
		ParticleProp()->StopEmission( m_pJumpJetEffect[0] );
		m_pJumpJetEffect[0] = NULL;
		ParticleProp()->StopEmission( m_pJumpJetEffect[1] );
		m_pJumpJetEffect[1] = NULL;
	}
}