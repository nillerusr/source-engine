//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "c_basetfplayer.h"
#include "beamdraw.h"
#include <stdarg.h>
#include "ivmodemanager.h"
#include "shake.h"
#include "ivieweffects.h"
#include "c_tfteam.h"
#include "view.h"
#include "UserCmd.h"
#include "ivrenderview.h"
#include "model_types.h"
#include "view_shared.h"
#include "hud_orders.h"
#include "weapon_twohandedcontainer.h"
#include "particles_simple.h"
#include "playerandobjectenumerator.h"
#include "iclientvehicle.h"
#include "input.h"
#include "basetfvehicle.h"
#include "c_vehicle_teleport_station.h"
#include "weapon_combatshield.h"
#include "hud_vehicle_role.h"
#include "hud_technologytreedoc.h"
#include "iclientmode.h"
#include "weapon_selection.h"
#include "clienteffectprecachesystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


// Don't alias here
#if defined( CBaseTFPlayer )
#undef CBaseTFPlayer	
#endif

void CAM_ToThirdPerson(void);
void CAM_ToFirstPerson(void);
void FX_ReconParticle( const Vector &vecOrigin, bool bBlue );

static ConVar tf2_showstate( "tf2_showstate", "0", 0, "Print state name\n" );
extern ConVar mannedgun_usethirdperson;
ConVar damageboost_modeloffset( "damageboost_modeloffset", "1" );
ConVar damageboost_modelphasespeed_min( "damageboost_modelphasespeed_min", "99" );
ConVar damageboost_modelphasespeed_max( "damageboost_modelphasespeed_max", "170" );


CLIENTEFFECT_REGISTER_BEGIN( PrecacheBaseTFPlayer )
CLIENTEFFECT_MATERIAL( "sprites/physbeam" )
CLIENTEFFECT_MATERIAL( "player/infiltratorcamo/infiltratorcamo" )
CLIENTEFFECT_REGISTER_END()


class CPersonalShieldEffect
{
public:

	static CPersonalShieldEffect* Create( C_BaseTFPlayer *pEnt, const Vector &vOffsetFromEnt, const Vector &vIncomingDirection, int iDamage );

				~CPersonalShieldEffect();

	// Returns false if the effect is done and should be deleted.
	bool		Update( float dt );
	
	void		Render();	// Draw the effect.

private:

				CPersonalShieldEffect();

private:

	CHandle<C_BaseTFPlayer> m_hEnt;
	Vector		m_vOffsetFromEnt;
	Vector		m_vIncomingDirection;
	float		m_flDamage;

	enum
	{
		NUM_TRACERS	= 10
	};

	Vector		m_vTracerEndPoints[NUM_TRACERS];	// These are relative to the model's origin.
	float		m_flLifetimeRemaining;
	float		m_flTotalLifetime;
};


CPersonalShieldEffect* CPersonalShieldEffect::Create( 
	C_BaseTFPlayer *pEnt, 
	const Vector &vOffsetFromEnt,
	const Vector &vIncomingDirection, 
	int iDamage )
{
	CPersonalShieldEffect *pRet = new CPersonalShieldEffect;
	if ( pRet )
	{
		pRet->m_hEnt = pEnt;
		pRet->m_vOffsetFromEnt = vOffsetFromEnt;
		pRet->m_flDamage = iDamage;

		Vector vNormal = vIncomingDirection;
		VectorNormalize( vNormal );
		
		for ( int i=0; i < CPersonalShieldEffect::NUM_TRACERS; i++ )
		{
			pRet->m_vTracerEndPoints[i] = pRet->m_vOffsetFromEnt;
			
			Vector vOffset = RandomVector( -1, 1 );
			
			// Make it tangent to a sphere enclosing the player.
			float flDot = vNormal.Dot( vOffset );
			vOffset -= vNormal * flDot;

			VectorNormalize( vOffset );
			vOffset *= 10;

			pRet->m_vTracerEndPoints[i] += vOffset;
		}

		pRet->m_flLifetimeRemaining = pRet->m_flTotalLifetime = 0.15;
		pRet->m_vIncomingDirection = vIncomingDirection;
	}
	return pRet;
}

CPersonalShieldEffect::CPersonalShieldEffect()
{
}


CPersonalShieldEffect::~CPersonalShieldEffect()
{
}


bool CPersonalShieldEffect::Update( float dt )
{
	if ( !m_hEnt.Get() )
		return false;

	m_flLifetimeRemaining -= dt;
	if ( m_flLifetimeRemaining >= 0 )
	{
		return true;
	}
	else
	{
		return false;
	}
}


void CPersonalShieldEffect::Render()
{
	if ( !m_hEnt.Get() )
		return;

	Vector vCenter = m_hEnt->WorldSpaceCenter( );

	const Vector vBasePos = vCenter + m_vOffsetFromEnt;
	IMaterial *pMat = materials->FindMaterial( "sprites/physbeam", TEXTURE_GROUP_CLIENT_EFFECTS );

	CBeamSeg seg;

	// Get redder as their health goes down.
	float flColor = (float)m_hEnt->GetHealth() / m_hEnt->GetMaxHealth();
	flColor = clamp(flColor, 0, 1);
	seg.m_vColor.Init( 0.5 + 0.5*flColor, flColor, flColor );

	seg.m_flAlpha = 1;
	seg.m_flTexCoord = 0;
	seg.m_flWidth = 2;

	for ( int i=0; i < CPersonalShieldEffect::NUM_TRACERS; i++ )
	{
		const Vector vEndPos = vCenter + m_vTracerEndPoints[i];

		static int nSegs = 5;

		CBeamSegDraw beamDraw;
		beamDraw.Start( nSegs, pMat );
		
		for ( int iSeg=0; iSeg < nSegs; iSeg++ )
		{
			float t = (float)iSeg / (nSegs-1);
			VectorLerp( vBasePos, vEndPos, t, seg.m_vPos );
			
			// Add a random offset.
			seg.m_vPos += RandomVector( -3, 3 );
			seg.m_flTexCoord += 0.1f;
			
			beamDraw.NextSeg( &seg );
		}
		
		beamDraw.End();
	}

/*
	Vector vEggBounds[2];
	m_hEnt->GetBounds( vEggBounds[0], vEggBounds[1] );
	vEggBounds[0] += m_hEnt->GetAbsOrigin();
	vEggBounds[1] += m_hEnt->GetAbsOrigin();
	Vector vEggCenter = (vEggBounds[0] + vEggBounds[1]) * 0.5f;
	Vector vEggDims = (vEggBounds[1] - vEggBounds[0]) * 0.5f;

	Vector vUp( 0, 0, 1 );
	Vector vForward = m_vIncomingDirection;
	vForward.z = 0;
	VectorNormalize( vForward );
	Vector vRight = vUp.Cross( vForward );

	// Now draw an eggshell around the player showing their health.
	seg.m_vColor.Init( 1, flColor, flColor );
	seg.m_flAlpha = 0.3f;
	seg.m_flWidth = 1;

	static int nSamples = 30;
	CBeamSegDraw beamDraw;
	beamDraw.Start( nSamples, pMat );

	for ( int iSeg=0; iSeg < nSamples; iSeg++ )
	{
		float t = (float)iSeg / (nSamples-1);
		float angle = M_PI * 2.0f * t;
		seg.m_vPos = vEggCenter + vUp*vEggDims.z*sin( angle ) + vRight*vEggDims.x*cos( angle );
		beamDraw.NextSeg( &seg );
	}

	beamDraw.End();
*/
}


//-----------------------------------------------------------------------------
// Purpose: Helper for animation state machine
// Input  : clear - 
//			destination - 
//			*pFormat - 
//			... - 
//-----------------------------------------------------------------------------
void StatusPrintf( bool clear, int destination, char *pFormat, ... )
{
	if ( destination != 4 && destination != 5 )
		return;
	char data[ 2048 ];
	va_list argptr;

	va_start( argptr, pFormat );
	Q_vsnprintf( data, sizeof( data ), pFormat, argptr );
	va_end( argptr );

	char *out = data;
	if ( destination == 5 )
		out += 2;

	if ( destination == 5 )
	{
		char slot[3];
		Q_strncpy( slot, data, 3 );

		slot[2] = 0;

		int s = atoi( slot );
		s &= 31;

		engine->Con_NPrintf( s, "%s", out );
	}
	else
	{
		Msg( "%s", out );
	}
}

//-----------------------------------------------------------------------------
// Purpose: RecvProxy that converts the Player's object UtlVector to entindexes
//-----------------------------------------------------------------------------
void RecvProxy_PlayerObjectList( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	CTFPlayerLocalData *pLocalData = (CTFPlayerLocalData*)pStruct;
	CBaseHandle *pHandle = (CBaseHandle*)(&(pLocalData->m_aObjects[pData->m_iElement])); 
	RecvProxy_IntToEHandle( pData, pStruct, pHandle );
}


void RecvProxyArrayLength_PlayerObjects( void *pStruct, int objectID, int currentArrayLength )
{
	CTFPlayerLocalData *pLocalData = (CTFPlayerLocalData*)pStruct;
	
	if ( pLocalData->m_aObjects.Count() != currentArrayLength )
	{
		pLocalData->m_aObjects.SetSize( currentArrayLength );
	}
}

BEGIN_RECV_TABLE_NOBASE( CTFPlayerLocalData, DT_TFLocal )
	RecvPropInt( RECVINFO(m_nInTacticalView) ),
	RecvPropInt( RECVINFO( m_bKnockedDown )),
	RecvPropVector( RECVINFO( m_vecKnockDownDir )),
	RecvPropInt( RECVINFO( m_bThermalVision )),
	RecvPropInt( RECVINFO( m_iIDEntIndex )),
	RecvPropArray( RecvPropInt( RECVINFO(m_iResourceAmmo[0])), m_iResourceAmmo),	
	RecvPropInt( RECVINFO(m_iBankResources) ),
	RecvPropArray2( 
		RecvProxyArrayLength_PlayerObjects,
		RecvPropInt( "player_object_array_element", 0, SIZEOF_IGNORE, 0, RecvProxy_PlayerObjectList ), 
		MAX_OBJECTS_PER_PLAYER, 
		0, 
		"player_object_array"	),
	RecvPropInt( RECVINFO( m_bAttachingSapper ) ),
	RecvPropFloat( RECVINFO( m_flSapperAttachmentFrac ) ),
	RecvPropInt( RECVINFO( m_bForceMapOverview ) ),
END_RECV_TABLE()

IMPLEMENT_CLIENTCLASS_DT(C_BaseTFPlayer, DT_BaseTFPlayer, CBaseTFPlayer)
	RecvPropDataTable(RECVINFO_DT(m_TFLocal),0, &REFERENCE_RECV_TABLE(DT_TFLocal), DataTableRecvProxy_StaticDataTable),

	// Class Data Tables
	RecvPropInt( RECVINFO(m_iPlayerClass)),
	RecvPropDataTable( RECVINFO_DT( m_PlayerClasses ), 0, &REFERENCE_RECV_TABLE( DT_AllPlayerClasses ), DataTableRecvProxy_StaticDataTable ),

	RecvPropEHandle( RECVINFO( m_hSelectedMCV ) ),
	RecvPropInt( RECVINFO(m_iCurrentZoneState ) ),
	RecvPropInt( RECVINFO(m_iMaxHealth ) ),
	RecvPropInt( RECVINFO(m_TFPlayerFlags) ),
	RecvPropInt( RECVINFO( m_bUnderAttack )),
	RecvPropInt( RECVINFO( m_bIsBlocking )),

	// Sniper - will get moved to a class data table
	RecvPropVector( RECVINFO(m_vecDeployedAngles) ),
	RecvPropInt( RECVINFO( m_bDeployed )),
	RecvPropInt( RECVINFO( m_bDeploying )),
	RecvPropInt( RECVINFO( m_bUnDeploying )),

	// Infiltrator - will get moved to a class data table
	RecvPropFloat( RECVINFO( m_flCamouflageAmount )),
	RecvPropEHandle(RECVINFO(m_hSpawnPoint)),
END_RECV_TABLE()

BEGIN_PREDICTION_DATA_NO_BASE( CTFPlayerLocalData )

	DEFINE_PRED_FIELD( m_nInTacticalView, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bKnockedDown, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_vecKnockDownDir, FIELD_VECTOR, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bThermalVision, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
//	DEFINE_PRED_FIELD( m_iIDEntIndex, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_iBankResources, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_ARRAY( m_iResourceAmmo, FIELD_INTEGER, RESOURCE_TYPES, FTYPEDESC_INSENDTABLE ),
	//DEFINE_PRED_ARRAY( m_aObjects, FIELD_EHANDLE, MAX_OBJECTS_PER_PLAYER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bAttachingSapper, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_flSapperAttachmentFrac, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bForceMapOverview, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),

END_PREDICTION_DATA()


// TODO: consolidate all these includes and the DEFINE_PRED... for each class type
// into tf_shareddefs.h.
#include "c_tf_class_commando.h"
#include "c_tf_class_defender.h"
#include "c_tf_class_escort.h"
#include "c_tf_class_infiltrator.h"
#include "c_tf_class_medic.h"
#include "c_tf_class_recon.h"
#include "c_tf_class_sniper.h"
#include "c_tf_class_support.h"
#include "c_tf_class_sapper.h"
#include "c_tf_class_pyro.h"


BEGIN_PREDICTION_DATA( C_BaseTFPlayer )

	DEFINE_PRED_TYPEDESCRIPTION( m_TFLocal, CTFPlayerLocalData ),
	
	DEFINE_PRED_FIELD( m_flCycle, FIELD_FLOAT, FTYPEDESC_INSENDTABLE | FTYPEDESC_PRIVATE | FTYPEDESC_OVERRIDE ),
	DEFINE_PRED_FIELD( m_flPlaybackRate, FIELD_FLOAT, FTYPEDESC_INSENDTABLE | FTYPEDESC_PRIVATE | FTYPEDESC_OVERRIDE ),

	DEFINE_PRED_TYPEDESCRIPTION_PTR( m_PlayerClasses.m_pClasses[TFCLASS_COMMANDO], C_PlayerClassCommando ),
	DEFINE_PRED_TYPEDESCRIPTION_PTR( m_PlayerClasses.m_pClasses[TFCLASS_DEFENDER], C_PlayerClassDefender ),
	DEFINE_PRED_TYPEDESCRIPTION_PTR( m_PlayerClasses.m_pClasses[TFCLASS_ESCORT], C_PlayerClassEscort ),
	DEFINE_PRED_TYPEDESCRIPTION_PTR( m_PlayerClasses.m_pClasses[TFCLASS_INFILTRATOR], C_PlayerClassInfiltrator ),
	DEFINE_PRED_TYPEDESCRIPTION_PTR( m_PlayerClasses.m_pClasses[TFCLASS_MEDIC], C_PlayerClassMedic ),
	DEFINE_PRED_TYPEDESCRIPTION_PTR( m_PlayerClasses.m_pClasses[TFCLASS_RECON], C_PlayerClassRecon ),
	DEFINE_PRED_TYPEDESCRIPTION_PTR( m_PlayerClasses.m_pClasses[TFCLASS_SNIPER], C_PlayerClassSniper ),
	DEFINE_PRED_TYPEDESCRIPTION_PTR( m_PlayerClasses.m_pClasses[TFCLASS_SUPPORT], C_PlayerClassSupport ),
	DEFINE_PRED_TYPEDESCRIPTION_PTR( m_PlayerClasses.m_pClasses[TFCLASS_SAPPER], C_PlayerClassSapper ),

	DEFINE_PRED_FIELD( m_iPlayerClass, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_flCamouflageAmount, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_hSpawnPoint, FIELD_EHANDLE, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_iMaxHealth, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bDeployed, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bDeploying, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bUnDeploying, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_vecDeployedAngles, FIELD_VECTOR, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_iCurrentZoneState, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_TFPlayerFlags, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bUnderAttack, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),

	DEFINE_PRED_FIELD( m_vecConstraintCenter, FIELD_VECTOR, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_flConstraintRadius, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_flConstraintWidth, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_flConstraintSpeedFactor, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),

	DEFINE_FIELD( m_vecPosDelta, FIELD_VECTOR ),
	DEFINE_ARRAY( m_aMomentum, FIELD_FLOAT, C_BaseTFPlayer::MOMENTUM_MAXSIZE ),
	DEFINE_FIELD( m_iMomentumHead, FIELD_INTEGER ),
	//DEFINE_FIELD( m_iSelectedTarget, FIELD_INTEGER ),
	//DEFINE_FIELD( m_iPersonalTarget, FIELD_INTEGER ),
	//DEFINE_FIELD( m_iLastHealth, FIELD_INTEGER ),
	//DEFINE_FIELD( m_nOldTacticalView, FIELD_INTEGER ),
	//DEFINE_FIELD( m_nOldPlayerClass, FIELD_INTEGER ),
	//DEFINE_FIELD( m_bOldThermalVision, FIELD_BOOLEAN ),
	//DEFINE_FIELD( m_bOldKnockDownState, FIELD_BOOLEAN ),
	//DEFINE_FIELD( m_flStartKnockdown, FIELD_FLOAT ),
	//DEFINE_FIELD( m_flEndKnockdown, FIELD_FLOAT ),
	//DEFINE_FIELD( m_vecOriginalViewAngles, FIELD_VECTOR ),
	//DEFINE_FIELD( m_vecCurrentKnockdownAngles, FIELD_VECTOR ),
	//DEFINE_FIELD( m_vecKnockDownGoalAngles, FIELD_VECTOR ),
	//DEFINE_FIELD( m_bKnockdownOverrideAngles, FIELD_BOOLEAN ),
	//DEFINE_FIELD( m_flKnockdownViewheightAdjust, FIELD_FLOAT ),
	//DEFINE_FIELD( m_flLastMoveTime, FIELD_FLOAT ),
	//DEFINE_FIELD( m_vecLastOrigin, FIELD_VECTOR ),
	//DEFINE_FIELD( m_flLastDamageTime, FIELD_FLOAT ),
	//DEFINE_FIELD( m_flLastGainHealthTime, FIELD_FLOAT ),
	//DEFINE_FIELD( m_flDampeningAmount, FIELD_FLOAT ),
	//DEFINE_FIELD( m_flGoalDampeningAmount, FIELD_FLOAT ),
	//DEFINE_FIELD( m_flDampeningStayoutTime, FIELD_FLOAT ),
	//DEFINE_FIELD( m_flMovementCamoSuppression, FIELD_FLOAT ),
	//DEFINE_FIELD( m_flGoalMovementCamoSuppressionAmount, FIELD_FLOAT ),
	//DEFINE_FIELD( m_flMovementCamoSuppressionStayoutTime, FIELD_FLOAT ),
	//DEFINE_FIELD( m_flNextAdrenalinEffect, FIELD_FLOAT ),
	//DEFINE_FIELD( m_bFadingIn, FIELD_BOOLEAN ),
	//DEFINE_FIELD( m_iIDEntIndex, FIELD_INTEGER ),
	//DEFINE_ARRAY( m_BoostModelAngles, FIELD_FLOAT, 3 ),

	// DEFINE_PRED_TYPEDESCRIPTION( m_RideVehicle, CRideVehicle ),
	// DEFINE_FIELD( m_aTargetReticles, CUtlVector < CTargetReticle * > ),
	// DEFINE_FIELD( m_aSpyCameras, CUtlVector < C_SpyCamera * > ),
	// DEFINE_FIELD( m_ParticleEffect, CParticleEffectBinding ),
	// DEFINE_FIELD( m_ParticleTimer, TimedEvent ),
	// DEFINE_FIELD( m_MaterialHandle, PMaterialHandle ),
	// DEFINE_FIELD( m_pParticleMgr, CParticleMgr ),
	// DEFINE_FIELD( m_pThermalMaterial, IMaterial ),
	// DEFINE_FIELD( m_pCamoEffectMaterial, IMaterial ),
	// DEFINE_FIELD( m_BoostMaterial, CMaterialReference ),
	// DEFINE_FIELD( m_EMPMaterial, CMaterialReference ),
	// DEFINE_FIELD( m_PersonalShieldEffects, CUtlLinkedList < CPersonalShieldEffect* , int > ),
	// DEFINE_FIELD( m_hSelectedOrder, CHandle < C_Order > ),
	// DEFINE_FIELD( m_hPersonalOrder, FIELD_EHANDLE ),
	// DEFINE_FIELD( m_hSelectedObject, CHandle < C_BaseObject > ),

END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( player, C_BaseTFPlayer );

ConVar cl_TargetInfoFadeDist("cl_TargetInfoFadeDist", "800", 0, "Distance at which TF player targetting info fades out.");

//-----------------------------------------------------------------------------
// Purpose: Return true if the local player is the specified class
//-----------------------------------------------------------------------------
bool IsLocalPlayerClass( int iClass )
{
	C_BaseTFPlayer *pPlayer = C_BaseTFPlayer::GetLocalPlayer();
	if ( pPlayer )
	{
		return ( pPlayer->PlayerClass() == iClass );
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Return the local player's class
//-----------------------------------------------------------------------------
int GetLocalPlayerClass( void )
{
	C_BaseTFPlayer *pPlayer = C_BaseTFPlayer::GetLocalPlayer();
	if ( pPlayer )
	{
		return pPlayer->PlayerClass();
	}
	return TFCLASS_UNDECIDED;
}

//-----------------------------------------------------------------------------
// returns true if the local player is in tactical view
//-----------------------------------------------------------------------------
bool IsLocalPlayerInTactical( void )
{
	C_BaseTFPlayer *pPlayer = C_BaseTFPlayer::GetLocalPlayer();
	if (!pPlayer)
		return false;

	return !!pPlayer->m_TFLocal.m_nInTacticalView;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_BaseTFPlayer::C_BaseTFPlayer() :
	m_PlayerClasses( this ), m_PlayerAnimState( this )
{
	Clear();
}

void C_BaseTFPlayer::Clear()
{
	m_iPlayerClass = TFCLASS_UNDECIDED;
	m_iCurrentZoneState = 0;
	m_TFPlayerFlags = 0;
	m_hSelectedOrder = NULL;
	m_hPersonalOrder = NULL;
	m_hSelectedObject = NULL;
	m_iSelectedTarget = 0;
	m_iPersonalTarget = 0;
	m_iIDEntIndex = 0;
	m_bStoreRagdollInfo	= true;
	m_flNextUseCheck = 0;
	m_pSapperAttachmentStatus = NULL;
	
	int i;
	for ( i=0; i < ARRAYSIZE( m_BoostModelAngles ); i++ )
	{
		m_BoostModelAngles[i] = RandomFloat( 0, 360 );
	}


	for ( i = 0; i < MOMENTUM_MAXSIZE; i++ )
	{
		m_aMomentum[ i ] = 1.0f;
	}
}


bool C_BaseTFPlayer::IsHidden() const
{
	return (m_TFPlayerFlags & TF_PLAYER_HIDDEN) != 0;
}


bool C_BaseTFPlayer::IsDamageBoosted() const
{
	return (m_TFPlayerFlags & TF_PLAYER_DAMAGE_BOOST) != 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_BaseTFPlayer::~C_BaseTFPlayer()
{
	int iSize = m_aTargetReticles.Size();
	for (int i = iSize-1; i >= 0; i-- )
	{
		delete m_aTargetReticles[i];
	}
	m_aTargetReticles.Purge();

	if ( m_pThermalMaterial )
	{
		m_pThermalMaterial->DecrementReferenceCount();
	}
	if ( m_pCamoEffectMaterial )
	{
		m_pCamoEffectMaterial->DecrementReferenceCount();
	}

	m_PersonalShieldEffects.PurgeAndDeleteElements();

	if ( m_pSapperAttachmentStatus )
	{
		delete m_pSapperAttachmentStatus;
	}
}

//-----------------------------------------------------------------------------
// Add, remove object from the panel 
//-----------------------------------------------------------------------------
void C_BaseTFPlayer::SetDormant( bool bDormant )
{
	BaseClass::SetDormant( bDormant );
	ENTITY_PANEL_ACTIVATE( "player", !bDormant );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool C_BaseTFPlayer::HasNamedTechnology( const char *name )
{
	CTechnologyTree *pTree = GetTechnologyTreeDoc().GetTechnologyTree();
	if ( !pTree )
		return false;

	CBaseTechnology *pItem = pTree->GetTechnology(name);
	// If the tech doesn't exist, everyone has it by default
	if ( !pItem )
		return true;

	return pItem->GetActive();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool C_BaseTFPlayer::ShouldDraw()
{
	if ( IsHidden() )
		return false;

	C_BaseTFPlayer *local = C_BaseTFPlayer::GetLocalPlayer();
	if ( local && local->IsUsingThermalVision() )
		return true;

	// Draw the local player if he's the driver of a vehicle.
	// We can safely return true here because vehicles will hide drivers that shouldn't be visible.
	if ( mannedgun_usethirdperson.GetInt() && IsVehicleMounted() && IsLocalPlayer() )
	{
		IClientVehicle *pVehicle = GetVehicle();
		int nRole = pVehicle->GetPassengerRole( this );
		if ( nRole == VEHICLE_ROLE_DRIVER )
			return !IsEffectActive(EF_NODRAW);
	}

	return BaseClass::ShouldDraw();
}


//-----------------------------------------------------------------------------
// Should this object cast shadows?
//-----------------------------------------------------------------------------
ShadowType_t C_BaseTFPlayer::ShadowCastType()
{
	// FIXME: This check can be removed once we've dealt with the interpolation problem
	C_BaseTFPlayer *local = C_BaseTFPlayer::GetLocalPlayer();
	if (local == this)
		return SHADOWS_NONE;

	if (IsCamouflaged())
		return SHADOWS_NONE;

	return SHADOWS_RENDER_TO_TEXTURE_DYNAMIC;
}


//-----------------------------------------------------------------------------
// Purpose: Called once per frame for the local player only.
//  Called after rendering ( called in PostRender() ) the 3d objects ( i.e., other players ) 
//  but before vgui paints 2d overlays so that we can update the positions of all world 
//  targets before rendering
//-----------------------------------------------------------------------------
void C_BaseTFPlayer::UpdateTargetReticles( void )
{
	// Update all the target reticles
	for ( int i = 0; i < m_aTargetReticles.Size(); i++ )
	{
		m_aTargetReticles[i]->Update();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called to update hud elements contained in the player
//-----------------------------------------------------------------------------
void C_BaseTFPlayer::ClientThink( void )
{
	BaseClass::ClientThink();

	SetNextClientThink( CLIENT_THINK_ALWAYS );

	CheckKnockdownState();
	CheckMovementCamoSuppression();
	CheckCamoDampening();
	CheckLastMovement();
	CheckAdrenalin();
	UpdateIDTarget();

	// update personal shield effects.
	int iNext;
	for ( int i=m_PersonalShieldEffects.Head(); i != m_PersonalShieldEffects.InvalidIndex(); i = iNext )
	{
		iNext = m_PersonalShieldEffects.Next( i );
		CPersonalShieldEffect *pEffect = m_PersonalShieldEffects[i];

		if ( !pEffect->Update( gpGlobals->frametime ) )
		{
			delete pEffect;
			m_PersonalShieldEffects.Remove( i );
		}
	}

	// Let the classes think as well.
	if ( GetPlayerClass() )
	{
		GetPlayerClass()->ClassThink();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Store off old movetype ( commander or not )
// Input  : bnewentity - 
//-----------------------------------------------------------------------------
void C_BaseTFPlayer::OnPreDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnPreDataChanged( updateType );

	m_nOldTacticalView = m_TFLocal.m_nInTacticalView;

	m_iLastHealth = GetHealth();
	m_bOldKnockDownState = m_TFLocal.m_bKnockedDown; 

	m_bOldThermalVision = m_TFLocal.m_bThermalVision;

	m_nOldPlayerClass = m_iPlayerClass;

	// Chain.
	if ( GetPlayerClass() )
	{
		GetPlayerClass()->ClassPreDataUpdate();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Switch to/from commander mode if necessary
// Input  : bnewentity - 
//-----------------------------------------------------------------------------
void C_BaseTFPlayer::OnDataChanged( DataUpdateType_t updateType )
{
	bool bnewentity = (updateType == DATA_UPDATE_CREATED);
	if ( bnewentity )
	{
		// Do the minimap panel thing here because we don't
		// want predicted players to have traces
		CONSTRUCT_MINIMAP_PANEL( "minimap_player", MINIMAP_PLAYERS );

		SetNextClientThink( CLIENT_THINK_ALWAYS );
		m_BoostMaterial.Init( "player/damageboost/thermal", TEXTURE_GROUP_CLIENT_EFFECTS );
		m_EMPMaterial.Init( "player/empeffect", TEXTURE_GROUP_CLIENT_EFFECTS );
	}

	BaseClass::OnDataChanged( updateType );

	// Only care about this stuff for the local player
	if ( IsLocalPlayer() )
	{
		// Check to see if we switched into/out of the commander mode.
		if ( m_TFLocal.m_nInTacticalView != m_nOldTacticalView )
		{
			// Is this the local player
			C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();
			if ( player == this )
			{
				// Tell mode switcher that server changed our mode
				modemanager->SwitchMode( m_TFLocal.m_nInTacticalView ? true : false, false );
			}
		}

		// Knockdown
		if ( m_bOldKnockDownState != m_TFLocal.m_bKnockedDown )
		{
			if ( IsKnockedDown() )
			{
				m_flStartKnockdown = gpGlobals->curtime;
				m_vecOriginalViewAngles = GetAbsAngles();
				
				m_vecKnockDownGoalAngles = m_TFLocal.m_vecKnockDownDir;
			}
			else
			{
				m_flEndKnockdown = gpGlobals->curtime;
			}
		}

		// Thermal Vision
		if ( m_bOldThermalVision != m_TFLocal.m_bThermalVision )
		{
			if ( m_TFLocal.m_bThermalVision )
			{
				ScreenFade_t sf;
				memset( &sf, 0, sizeof( sf ) );
				sf.a = 128;
				sf.r = 0;
				sf.g = 0;
				sf.b = 255;
				sf.duration = 0;	// not used
				sf.holdTime = (float)(1<<SCREENFADE_FRACBITS) * 30.0f;
				sf.fadeFlags = FFADE_STAYOUT;
				vieweffects->Fade( sf );
			}
			else
			{
				vieweffects->ClearPermanentFades();
			}		
		}

		if ( m_nOldPlayerClass != m_iPlayerClass )
		{
			engine->ClientCmd( "cancelselect\n" );
		}
	}

	if ( bnewentity )
	{
		m_iLastHealth = GetHealth();
		m_flLastDamageTime = -10000;
		m_flLastGainHealthTime = -10000;
	}

	if (m_iLastHealth != GetHealth())
	{
		if (m_iLastHealth > GetHealth())
		{
			if (GetHealth() < GetMaxHealth())
			{
				m_flLastDamageTime = gpGlobals->curtime;

				C_TFTeam *pTeam = static_cast<C_TFTeam*>(GetTeam());
				if (pTeam && !IsLocalPlayer() && m_bUnderAttack )
					pTeam->NotifyBaseUnderAttack( GetAbsOrigin(), false );
			}
		}
		else
		{
			m_flLastGainHealthTime = gpGlobals->curtime;

			// If we were just fully healed, remove all decals
			if ( GetHealth() >= GetMaxHealth() )
			{
				RemoveAllDecals();
			}
		}
	}

	// Snap to 100 % since we round down
	if ( m_flCamouflageAmount > 99.0f )
	{
		m_flCamouflageAmount = 100.0f;
	}

	// Chain.
	if ( GetPlayerClass() )
	{
		GetPlayerClass()->ClassOnDataChanged();
	}
} 

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BaseTFPlayer::PreDataUpdate( DataUpdateType_t updateType )
{
	BaseClass::PreDataUpdate( updateType );

	m_bOldAttachingSapper = m_TFLocal.m_bAttachingSapper;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BaseTFPlayer::PostDataUpdate( DataUpdateType_t updateType )
{
	BaseClass::PostDataUpdate( updateType );

	// Only care about this stuff for the local player
	if ( IsLocalPlayer() )
	{
		// Sapper attachment
		if ( m_bOldAttachingSapper != m_TFLocal.m_bAttachingSapper || m_TFLocal.m_bAttachingSapper )
		{
			if ( !m_pSapperAttachmentStatus )
			{
				// Create the attachment status
				m_pSapperAttachmentStatus = new CHealthBarPanel();
				vgui::Panel *pParent = g_pClientMode->GetViewport();
				m_pSapperAttachmentStatus->SetParent( pParent );
				m_pSapperAttachmentStatus->SetAutoDelete( false );
				m_pSapperAttachmentStatus->SetPos( XRES(320) - XRES(40), YRES(250) );
				m_pSapperAttachmentStatus->SetSize( XRES(80), YRES(8) );
				m_pSapperAttachmentStatus->SetGoodColor( 240, 180, 63, 192 );
				m_pSapperAttachmentStatus->SetBadColor( 0, 0, 0, 192 );
			}

			m_pSapperAttachmentStatus->SetVisible( m_TFLocal.m_bAttachingSapper );
			if ( m_TFLocal.m_bAttachingSapper )
			{
				m_pSapperAttachmentStatus->SetHealth( m_TFLocal.m_flSapperAttachmentFrac );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BaseTFPlayer::ReceiveMessage( int classID, bf_read &msg )
{
	if ( classID != GetClientClass()->m_ClassID )
	{
		// message is for subclass
		BaseClass::ReceiveMessage( classID, msg );
		return;
	}

	int messageType = msg.ReadByte();
	switch( messageType )
	{
	case BASEENTITY_MSG_REMOVE_DECALS:
		{
			RemoveAllDecals();
		}
		break;
	case PLAYER_MSG_PERSONAL_SHIELD:
		{
			Vector vOffsetFromEnt;
			msg.ReadBitVec3Coord( vOffsetFromEnt );

			// Show a personal shield effect.
			Vector vIncomingDirection;
			msg.ReadBitVec3Normal( vIncomingDirection );

			short iDamage = msg.ReadShort();

			// Show the effect.
			CPersonalShieldEffect *pEffect = CPersonalShieldEffect::Create( this, vOffsetFromEnt, vIncomingDirection, iDamage );
			if ( pEffect )
				m_PersonalShieldEffects.AddToTail( pEffect );
		}
		break;
	default:
		break;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Free this entity
//-----------------------------------------------------------------------------
void C_BaseTFPlayer::Release( void )
{
	// Remove any reticles on this entity
	C_BaseTFPlayer *pPlayer = C_BaseTFPlayer::GetLocalPlayer();
	if ( pPlayer )
	{
		pPlayer->Remove_Target( this );
	}

	BaseClass::Release();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BaseTFPlayer::ItemPostFrame( void )
{
	if ( m_flNextUseCheck < gpGlobals->curtime )		
	{
		m_flNextUseCheck = gpGlobals->curtime + 0.3;

		CVehicleRoleHudElement *pElement = GET_HUDELEMENT( CVehicleRoleHudElement );
		Assert( pElement );
		pElement->ShowVehicleRole( -1 );

		// See if there's an entity we can +use
		if ( IsAlive() && !GetVehicle() && ( gpGlobals->curtime > m_flNextAttack ) )
		{
			CBaseEntity *pUseEntity = FindUseEntity();
			if ( pUseEntity )
			{
				// Vehicles need to show we're going to get in them
				C_BaseTFVehicle *pVehicle = dynamic_cast<C_BaseTFVehicle*>( pUseEntity );
				if ( pVehicle && InSameTeam(pVehicle) && !pVehicle->IsPlacing() && !pVehicle->IsBuilding() && pVehicle->GetMaxPassengerCount() >= 2 )
				{
					pElement->ShowVehicleRole( pVehicle->LocateEntryPoint( this ) );
				}
			}
		}
	}

	// Don't process items while in a vehicle.
	if ( IsInAVehicle() )
	{
		IClientVehicle *pVehicle = GetVehicle();
		Assert( pVehicle );

		// NOTE: We *have* to do this before ItemPostFrame because ItemPostFrame
		// may dump us out of the vehicle
		int nRole = pVehicle->GetPassengerRole( this );
		bool bUsingStandardWeapons = pVehicle->IsPassengerUsingStandardWeapons( nRole );

		pVehicle->ItemPostFrame( this );

		// Fall through and check weapons, etc. if we're using them 
		if (!bUsingStandardWeapons || !IsInAVehicle())
			return;
	}

	// If we're attaching a sapper, handle player use only
	if ( m_TFLocal.m_bAttachingSapper )
	{
		PlayerUse();
		return;
	}

	BaseClass::ItemPostFrame();

#if 0
	if ( GetPlayerClass()  )
	{
		GetPlayerClass()->ItemPostFrame();	// Let the player class handle it.
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Return true if I'm in a vehicle that's mounted on another vehicle
//-----------------------------------------------------------------------------
bool C_BaseTFPlayer::IsVehicleMounted() const
{
	CBaseTFVehicle *pVehicle = dynamic_cast< CBaseTFVehicle* >( GetMoveParent() );
	if ( pVehicle )
		return dynamic_cast< CBaseTFVehicle* >( pVehicle->GetMoveParent() ) != NULL;

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int C_BaseTFPlayer::DrawModel( int flags )
{
	int drawn = 0;

	if ( !m_bReadyToDraw )
		return 0;

	QAngle saveAngles(0,0,0);
	Vector saveLocalOrigin(0,0,0);
	bool angleschanged = false;
	bool originchanged = false;

	// If we're in a vehicle, use the vehicle's angles for the local player
	if ( IsInAVehicle() && !IsInAVehicle() )
	{
		IClientVehicle *pVehicle = GetVehicle();
		int nRole = pVehicle->GetPassengerRole( this );
		if ( nRole == VEHICLE_ROLE_DRIVER )
		{
			C_BaseTFVehicle *pVehicleEntity = (CBaseTFVehicle*)pVehicle->GetVehicleEnt();
			angleschanged = true;
			SetLocalAngles( pVehicleEntity->GetPassengerAngles( saveAngles, VEHICLE_ROLE_DRIVER ) );

			// HACK: Stomp the origin
			originchanged = true;
			SetLocalOrigin( vec3_origin );
		}
	}
	else
	{
		angleschanged = true;
		SetLocalAngles( m_PlayerAnimState.GetRenderAngles() );
	}

	drawn = BaseClass::DrawModel(flags);

	if ( angleschanged )
	{
		SetLocalAngles( saveAngles );
	}
	if ( originchanged )
	{
		SetLocalOrigin( saveLocalOrigin );
	}

	// Draw all personal shield effects.
	FOR_EACH_LL( m_PersonalShieldEffects, i )
	{
		m_PersonalShieldEffects[i]->Render();
	}

	return drawn;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int C_BaseTFPlayer::GetClass( void )
{
	if ( !GetPlayerClass() )
		return TFCLASS_UNDECIDED;

	return m_iPlayerClass;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BaseTFPlayer::OverrideView( CViewSetup *pSetup )
{
	if ( CheckKnockdownAngleOverride() )
	{
		float adj = GetKnockdownViewheightAdjust();

		pSetup->origin.z -= adj;

		return;
	}

	BaseClass::OverrideView( pSetup );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool C_BaseTFPlayer::ShouldDrawViewModel()
{
	if ( CheckKnockdownAngleOverride() )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Hack to search up the hierarchy looking for bones whose ultimate parent is 1 ( i.e., the pelvis ) since
//  these are the lower body bones
//-----------------------------------------------------------------------------
bool ParentIsPelvis( mstudiobone_t *bones, int start, int pelvis, int spine )
{
	int current = start;

	while ( bones[ current ].parent )
	{
		// Root?
		if ( bones[ current ].parent <= 0 )
		{
			return false;
		}

		if ( bones[ current ].parent == pelvis )
			return true;

		if ( bones[ current ].parent == spine )
			return false;

		current = bones[ current ].parent;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Hack to find the bone indices of the pelvis and the spine so we can 
//   more quickly determine if a bones parent is the pelvis so we can merge bones correctly
//-----------------------------------------------------------------------------
static void FindPelvisAndSpine( int numbones, mstudiobone_t *bones, int *pelvis, int *spine )
{
	*pelvis = *spine = -1;

	mstudiobone_t *bone = bones;
	for ( int i = 0; i < numbones; i++, bone++ )
	{
		if ( !stricmp( bone->pszName(), "Bip01 Pelvis" ) )
		{
			*pelvis = i;
		}
		else if ( !stricmp( bone->pszName(), "Bip01 Spine" ) )
		{
			*spine = i;
		}

		if ( *spine >= 0 && *pelvis >= 0 )
			break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Another hack, the TFC 1.5 v. 2 models expect controllers at the midpoint
// Input  : controllers[MAXSTUDIOBONECTRLS] - 
//-----------------------------------------------------------------------------
void C_BaseTFPlayer::GetBoneControllers(float controllers[MAXSTUDIOBONECTRLS])
{
	// Set controllers to a their zero value.
	for(int i=0; i < MAXSTUDIOBONECTRLS; i++)
	{
		controllers[i] = 0.5;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Get a text description for the object target
//-----------------------------------------------------------------------------
void C_BaseTFPlayer::GetTargetDescription( char *pDest, int bufferSize )
{
	const char *pStr = GetPlayerName();
	if ( pStr )
	{
		Q_strncpy( pDest, pStr, bufferSize );
	}
	else
	{
		pDest[0] = 0;
	}
}

//=====================================================================================================
// ORDERS
//=====================================================================================================


//-----------------------------------------------------------------------------
// Purpose: Player has received a new personal order
//-----------------------------------------------------------------------------
void C_BaseTFPlayer::SetPersonalOrder( C_Order *pOrder )
{
	// Do we have an order already?
	RemoveOrderTarget();

	m_hPersonalOrder = pOrder;
	m_iPersonalTarget = pOrder->GetTarget();

	// Add a new target to our list
	if ( pOrder->ShouldDrawReticle() )
	{
		int iTargetIndex = pOrder->GetTarget();
		if ( iTargetIndex )
		{
			C_BaseEntity *pTarget = cl_entitylist->GetBaseEntity( iTargetIndex );
			if ( pTarget )
			{
				char desc[512];
				pOrder->GetTargetDescription( desc, sizeof( desc ) );
				Add_Target( pTarget, desc );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Remove the target reticle for the specified order
//-----------------------------------------------------------------------------
void C_BaseTFPlayer::RemoveOrderTarget()
{
	if ( m_iPersonalTarget )
	{
		C_BaseEntity *pTarget = cl_entitylist->GetBaseEntity( m_iPersonalTarget );
		if ( pTarget )
		{
			Remove_Target( pTarget );
		}

		m_iPersonalTarget = 0;
	}
}

//====================================================================================================
// RESOURCES
//====================================================================================================
// Purpose: 
//-----------------------------------------------------------------------------
int C_BaseTFPlayer::GetBankResources( void )
{
	return m_TFLocal.m_iBankResources;
}


//====================================================================================================
// OBJECTS
//====================================================================================================
// Purpose: Player has selected an Object
//-----------------------------------------------------------------------------
void C_BaseTFPlayer::SetSelectedObject( C_BaseObject *pObject )
{
	// Do we have an order already?
	if ( m_hSelectedObject )
	{
		Remove_Target( m_hSelectedObject );

		// If we selected our existing one, we just wanted to deselect
		if ( pObject == m_hSelectedObject )
		{
			m_hSelectedObject = NULL;
			return;
		}
	}

	m_hSelectedObject = pObject;

	// Add a new target to our list
	if ( pObject )
	{
		Add_Target( pObject, pObject->GetTargetDescription() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Get the currently selected object
//-----------------------------------------------------------------------------
C_BaseObject *C_BaseTFPlayer::GetSelectedObject( void )
{
	return m_hSelectedObject;
}

//====================================================================================================
// TARGET RETICLES
//====================================================================================================
// Purpose: Add a new entity to the list of targets
//-----------------------------------------------------------------------------
void C_BaseTFPlayer::Add_Target( C_BaseEntity *pTarget, const char *sName )
{
	CTargetReticle *pTargetReticle = new CTargetReticle();
	pTargetReticle->Init( pTarget, sName );

	m_aTargetReticles.AddToTail( pTargetReticle );
}

//-----------------------------------------------------------------------------
// Purpose: Remove an entity from the list of targets
//-----------------------------------------------------------------------------
void C_BaseTFPlayer::Remove_Target( C_BaseEntity *pTarget )
{
	for (int i = 0; i < m_aTargetReticles.Size(); i++ )
	{
		CTargetReticle *pTargetReticle = m_aTargetReticles[i];
		if ( pTargetReticle->GetTarget() == pTarget )
		{
			Remove_Target( pTargetReticle );
			return;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Remove a specific reticle from our list
//-----------------------------------------------------------------------------
void C_BaseTFPlayer::Remove_Target( CTargetReticle *pTargetReticle )
{
	m_aTargetReticles.FindAndRemove( pTargetReticle );
	delete pTargetReticle;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool C_BaseTFPlayer::IsUsingThermalVision( void ) const
{
	return m_TFLocal.m_bThermalVision;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int C_BaseTFPlayer::GetIDTarget( void ) const
{
	return m_iIDEntIndex;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool C_BaseTFPlayer::IsKnockedDown( void ) const
{
	return m_TFLocal.m_bKnockedDown;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : ang - 
//-----------------------------------------------------------------------------
void C_BaseTFPlayer::SetKnockdownAngles( const QAngle& ang )
{
	m_bKnockdownOverrideAngles = true;
	QAngle fixedAngles = ang;
	NormalizeAngles( fixedAngles );
	m_vecCurrentKnockdownAngles = fixedAngles;
	engine->SetViewAngles( fixedAngles );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : outAngles - 
//-----------------------------------------------------------------------------
void C_BaseTFPlayer::GetKnockdownAngles( QAngle& outAngles )
{
	outAngles = m_vecCurrentKnockdownAngles;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BaseTFPlayer::CheckKnockdownState( void )
{
	m_bKnockdownOverrideAngles = false;

	if ( m_flStartKnockdown == 0.0f && 
		 m_flEndKnockdown == 0.0f &&
		 !IsKnockedDown() )
	{
		m_flKnockdownViewheightAdjust = 0.0f;
		return;
	}

	float frac = 0.0f;
	float dt;
	if ( IsKnockedDown() )
	{
		if ( m_flStartKnockdown != 0.0f )
		{
			
			dt = gpGlobals->curtime - m_flStartKnockdown;
			if ( dt >= 0.0f && dt < KNOCKDOWN_BLEND_IN )
			{
				frac = ( dt / KNOCKDOWN_BLEND_IN );
				frac = MAX( 0.0f, frac );
				frac = MIN( 1.0f, frac );
			}
			else if ( dt >= KNOCKDOWN_BLEND_IN)
			{
				m_flStartKnockdown = 0.0f;
				frac = 1.0f;
			}
		}
		else
		{
			frac = 1.0f;
		}
	}
	else
	{
		if ( m_flEndKnockdown != 0.0f )
		{
			frac = 0.0f;
			dt = gpGlobals->curtime - m_flEndKnockdown;
			if ( dt >= 0 && dt < KNOCKDOWN_BLEND_OUT )
			{
				frac = ( dt / KNOCKDOWN_BLEND_OUT );
				frac = 1.0f - frac;
			}
			else if ( dt >= KNOCKDOWN_BLEND_OUT )
			{
				m_flEndKnockdown = 0.0f;
			}
			else
			{
				frac = 1.0f;
			}
		}
	}

	if ( frac == 0.0f )
	{
		SetKnockdownAngles( m_vecOriginalViewAngles );
	}
	else if ( frac == 1.0f )
	{
		SetKnockdownAngles( m_vecKnockDownGoalAngles );
	}
	else
	{
		QAngle current;
		InterpolateAngles( m_vecOriginalViewAngles, m_vecKnockDownGoalAngles, current, frac );
		SetKnockdownAngles( current );
	}

	Vector eyeZOffset;
	VectorSubtract( EyePosition(), GetAbsOrigin(), eyeZOffset );
	float zsize = eyeZOffset.z;

	m_flKnockdownViewheightAdjust = frac * ( zsize - 12 );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool C_BaseTFPlayer::CheckKnockdownAngleOverride( void ) const
{
	return m_bKnockdownOverrideAngles;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : float
//-----------------------------------------------------------------------------
float C_BaseTFPlayer::GetKnockdownViewheightAdjust( void ) const
{
	return m_flKnockdownViewheightAdjust;
}


//-----------------------------------------------------------------------------
// Purpose: Player has changed to a new team
//-----------------------------------------------------------------------------
void C_BaseTFPlayer::TeamChange( int iNewTeam )
{
	BaseClass::TeamChange( iNewTeam );

	// Did we change team? or did we just join our first team?
	if ( iNewTeam != GetTeamNumber() )
	{
		// Tell the tech tree to reload itself
		GetTechnologyTreeDoc().ReloadTechTree();
	}
}

//-----------------------------------------------------------------------------
// Purpose: // Override so infiltrator's disguised as other team will work
// Output : int
//-----------------------------------------------------------------------------
int C_BaseTFPlayer::GetRenderTeamNumber( void )
{
	return GetTeamNumber();
}


// 50 % per second
#define CAMO_DAMPENING_CHANGERATE			300.0f
#define CAMO_MOVESUPPRESSION_CHANGERATE		100.0f
#define CAMO_DAMPENINGVELOCITY_CUTOFF		50.0f
#define CAMO_DAMPENINGVELOCITY_MAX			400.0f
#define CAMO_DAMPENINGAVEL_CUTOFF			1.0f
#define CAMO_DAMPENINGAVEL_MAX				5.0f
#define CAMO_STAYOUT_TIME					1.0f
#define CAMO_MOVEMENT_PENALTYTIME			1.0f

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool C_BaseTFPlayer::IsCamouflaged( void )
{
	return ( m_flCamouflageAmount > 0.0f ) ? true : false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : float
//-----------------------------------------------------------------------------
float C_BaseTFPlayer::GetCamouflageAmount( void )
{
	return m_flCamouflageAmount;
}

//-----------------------------------------------------------------------------
// Purpose: 0 to 1, where 1 means hardly visible and 0 means pretty noticeable
// Output : float
//-----------------------------------------------------------------------------

#define DISTANCE_CAMO_EFFECT_AMOUNT 0.1f
#define LOCAL_MOTION_CAMO_EFFECT_AMOUNT 0.3f
#define MOVEMENT_CAMO_EFFECT_AMOUNT 0.2f

float C_BaseTFPlayer::ComputeCamoEffectAmount( void )
{
	if ( !IsCamouflaged() )
		return 1.0f;

	// Start with the amount from the server....
	float effect_amount = m_flCamouflageAmount / 100.0f;

	// If this player has moved recently, camo will not be as effective for him
	effect_amount -= MOVEMENT_CAMO_EFFECT_AMOUNT * GetMovementCamoSuppression() / 100.0f;
	if (effect_amount < 0)
		effect_amount = 0;

	// Determine distance to render origin
	Vector delta = GetAbsOrigin() - CurrentViewOrigin();
	float distance = delta.Length();

	// At the max distance, make it n% less likely to see the camoed dude...
	// At min distance, we're no less likely to see the dude
	if ( distance >= CAMO_INNER_RADIUS )
		effect_amount *= 1 + DISTANCE_CAMO_EFFECT_AMOUNT;
	else
	{
		float frac = distance / CAMO_INNER_RADIUS;
		effect_amount *= 1 - frac + (1 + DISTANCE_CAMO_EFFECT_AMOUNT) * frac;
	}

	// Local viewer movements make it n% less likely to see the camoed dude...
	// No movement means we're no less likely to see the dude
	// Dampening is based on the local viewer's movements
	float dampening = 0.0f;
	C_BaseTFPlayer *local = GetLocalPlayer();
	if ( local )
	{
		dampening = local->GetDampeningAmount() / 100.0f;
	}

	// Now apply suppression (i.e., less visible) based on camera and viewer movement
	effect_amount *= 1 + LOCAL_MOTION_CAMO_EFFECT_AMOUNT * dampening;

	// Clamp to valid range
	effect_amount = MAX( 0.0f, effect_amount );
	effect_amount = MIN( 0.99f, effect_amount );

	return effect_amount;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int C_BaseTFPlayer::ComputeCamoAlpha( void )
{
	Assert( IsCamouflaged() );

	// Determine distance to render origin
	Vector delta = GetAbsOrigin() - CurrentViewOrigin();
	float distance = delta.Length();

	int baseline = 0;

	// Too far, just blend out completely
	if ( distance >= CAMO_OUTER_RADIUS )
	{
		baseline = 0;
	}
	else if ( distance >= CAMO_INNER_RADIUS )
	{
		float frac = ( distance - CAMO_INNER_RADIUS ) / ( CAMO_OUTER_RADIUS - CAMO_INNER_RADIUS );
		frac = 1 - frac;
		baseline = (int)( (float)( 255 - CAMO_INNER_ALPHA ) * frac );
	}
	else if ( distance >= CAMO_INVIS_RADIUS )
	{
		// We'll also render with the special effect
		float frac = ( distance - CAMO_INVIS_RADIUS ) / ( CAMO_INNER_RADIUS - CAMO_INVIS_RADIUS );
		baseline = (int)( (float)( CAMO_INNER_ALPHA ) * frac );
	}
	else
	{
		// NOTE:  return 1 or else the renderer will skip drawing and we won't be
		//  able to draw the up close effect
		baseline = 1;
	}
	
	// Suppress everything based on server ramp
	return baseline + (int)( (float)( 255 - baseline ) * ( m_flCamouflageAmount / 100.0f ) );

}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
void C_BaseTFPlayer::ComputeFxBlend( void )
{
	if ( !IsCamouflaged() )
	{
		BaseClass::ComputeFxBlend();
		return;
	}

	m_nRenderFXBlend = ComputeCamoAlpha();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool C_BaseTFPlayer::IsTransparent( void )
{
	if ( IsCamouflaged() )
	{
		return true;
	}

	return BaseClass::IsTransparent();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool C_BaseTFPlayer::ViewModel_IsTransparent( void )
{
	if ( IsCamouflaged() )
	{
		return true;
	}

	return BaseClass::ViewModel_IsTransparent();
}

//-----------------------------------------------------------------------------
// Purpose: Return true if the player's viewmodel should match the player's model data
//-----------------------------------------------------------------------------
bool C_BaseTFPlayer::IsOverridingViewmodel( void )
{
	// Don't override medic weapons since we have effects for them
	if ( PlayerClass() == TFCLASS_MEDIC )
		return BaseClass::IsOverridingViewmodel();

	if ( IsDamageBoosted() || IsCamouflaged() || HasPowerup(POWERUP_EMP) )
		return true;

	return BaseClass::IsOverridingViewmodel();
}

//-----------------------------------------------------------------------------
// Purpose: Draw my viewmodel in some special way
//-----------------------------------------------------------------------------
int	C_BaseTFPlayer::DrawOverriddenViewmodel( C_BaseViewModel *pViewmodel, int flags )
{
	int ret = 0;

	if ( IsCamouflaged() && ( ComputeCamoEffectAmount() != 1.0f ) )
	{
		if ( GetCamoMaterial() )
		{
			modelrender->ForcedMaterialOverride( GetCamoMaterial() );
			ret = pViewmodel->DrawOverriddenViewmodel( flags );
			modelrender->ForcedMaterialOverride( NULL );
		}
	}

	if ( IsDamageBoosted() || HasPowerup(POWERUP_EMP) )
	{

		// First draw the model once normally.
		pViewmodel->DrawOverriddenViewmodel( flags );

		// NOTE: for the demo we do a different "BOOST" effect.  What do we want to do here?
		if ( !inv_demo.GetInt() )
		{
			if ( IsDamageBoosted()  && 
				pViewmodel->ViewModelIndex() != 0 )
			{
				return ret;
			}

			// Now overlay some shimmering ones.
			render->SetBlend( 0.3 );

			if ( IsDamageBoosted() )
			{
				// Radeon 9700 having a problem with this guy:
				modelrender->ForcedMaterialOverride( m_BoostMaterial );
			}
			else
			{
				modelrender->ForcedMaterialOverride( m_EMPMaterial );
			}

			Vector vStart = pViewmodel->GetAbsOrigin();

			float flOffset = damageboost_modeloffset.GetFloat();
			for ( int i=0; i < 3; i++ )
			{
				// Place the model at a slight offset.
				vStart[i] += flOffset * sin( m_BoostModelAngles[i] );
				pViewmodel->SetLocalOrigin( vStart );
				m_BoostModelAngles[i] += RandomFloat( damageboost_modelphasespeed_min.GetFloat(), damageboost_modelphasespeed_max.GetFloat() ) * gpGlobals->frametime;

				// Invalidate the bones because they've been setup with our original position and cached,
				// and we want to render it in a new spot with different bone transforms.
				pViewmodel->InvalidateBoneCache();
				ret = pViewmodel->DrawOverriddenViewmodel( flags );
			}

			// Reset the position and bone info.
			pViewmodel->SetLocalOrigin( vStart );
			pViewmodel->InvalidateBoneCache();

			modelrender->ForcedMaterialOverride( NULL );
			render->SetBlend( 1 );
		}
	}

	return ret;
}

//-----------------------------------------------------------------------------
// Purpose: Players under adrenalin animate faster
//-----------------------------------------------------------------------------
float C_BaseTFPlayer::GetDefaultAnimSpeed( void )
{
	if ( HasPowerup( POWERUP_RUSH ) )
		return ADRENALIN_ANIM_SPEED;

	// Weapons may modify animation times
	if ( GetActiveWeapon() )
		return GetActiveWeapon()->GetDefaultAnimSpeed();

	return 1.0;
}

//-----------------------------------------------------------------------------
// Purpose: This is used to penalize the local viewer for moving around or rotating 
//  the camera, it causes camo'd guys who are close to fade out their "white" effect
//  for a bit of time
//-----------------------------------------------------------------------------
void C_BaseTFPlayer::CheckCameraMovement( void )
{
	float dt = gpGlobals->frametime;

	float vel = GetAbsVelocity().Length();
	float avel = fabs( GetLocalAngles().y - GetPrevLocalAngles().y );
	if ( dt > 0.0f )
	{
		avel *= 1.0f / dt;
	}

	float frac1 = 0.0f;
	if ( vel > CAMO_DAMPENINGVELOCITY_CUTOFF )
	{
		frac1 = ( vel - CAMO_DAMPENINGVELOCITY_CUTOFF ) / ( CAMO_DAMPENINGVELOCITY_MAX - CAMO_DAMPENINGVELOCITY_CUTOFF );
		frac1 = MIN( 0.0f, frac1 );
		frac1 = MAX( 1.0f, frac1 );

		frac1 *= 50.0f;

		m_flDampeningStayoutTime = gpGlobals->curtime + CAMO_STAYOUT_TIME;

	}

	float frac2 = 0.0f;
	if ( avel > CAMO_DAMPENINGAVEL_CUTOFF )
	{
		frac2 = ( avel - CAMO_DAMPENINGAVEL_CUTOFF ) / ( CAMO_DAMPENINGAVEL_MAX - CAMO_DAMPENINGAVEL_CUTOFF );
		frac2 = MIN( 0.0f, frac2 );
		frac2 = MAX( 1.0f, frac2 );

		frac2 *= 50.0f;

		m_flDampeningStayoutTime = gpGlobals->curtime + CAMO_STAYOUT_TIME;

	}

	// Pick the greater
	float amount = MAX( frac1, frac2 );

	SetCamoDampening( amount );
}

//-----------------------------------------------------------------------------
// Purpose: Decay suppression camo amount
//-----------------------------------------------------------------------------
void C_BaseTFPlayer::CheckCamoDampening( void )
{
	CheckCameraMovement();

	if ( m_flGoalDampeningAmount == m_flDampeningAmount )
		return;

	// Camouflage
	float dt = gpGlobals->frametime;
	float changeamount = ( dt * CAMO_DAMPENING_CHANGERATE );

	// Move but don't overshoot
	if ( m_flGoalDampeningAmount > m_flDampeningAmount )
	{
		m_flDampeningAmount += changeamount;
		m_flDampeningAmount = MIN( m_flDampeningAmount, m_flGoalDampeningAmount );
	}
	else
	{
		if ( gpGlobals->curtime < m_flDampeningStayoutTime )
			return;

		m_flDampeningAmount -= changeamount;
		m_flDampeningAmount = MAX( m_flDampeningAmount, m_flGoalDampeningAmount );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Force amount, decay happens in CheckCamoDampening
// Input  : amount - 
//-----------------------------------------------------------------------------
void C_BaseTFPlayer::SetCamoDampening( float amount )
{
	m_flGoalDampeningAmount = amount;
}

//-----------------------------------------------------------------------------
// Purpose: For camera movement by local player
// Output : float
//-----------------------------------------------------------------------------
float C_BaseTFPlayer::GetDampeningAmount( void )
{
	return m_flDampeningAmount;
}

//-----------------------------------------------------------------------------
// Purpose: This is used so that if you are watching a camo'd guy who is moving
//  he becomes more visible for a bit of time, before fading back out.
// Input  : amount - 
//-----------------------------------------------------------------------------
void C_BaseTFPlayer::SetMovementCamoSuppression( float amount )
{
	m_flGoalMovementCamoSuppressionAmount = amount;
}

//-----------------------------------------------------------------------------
// Purpose: This is used so that if you are watching a camo'd guy who is moving
//  he becomes more visible for a bit of time, before fading back out.
//-----------------------------------------------------------------------------
void C_BaseTFPlayer::CheckMovementCamoSuppression( void )
{
	// FIXME: Don't bother with suppression during deployment...
	if ( m_bDeployed )
	{
		m_flMovementCamoSuppression	= 0;
		return;
	}

	float flSuppression = 0;

	// Rotation blends them in a bit too
	float avel = fabs( GetLocalAngles().y - GetPrevLocalAngles().y ) + fabs( GetLocalAngles().x - GetPrevLocalAngles().x );
	if ( avel > CAMO_DAMPENINGAVEL_CUTOFF )
	{
		float frac2 = ( avel - CAMO_DAMPENINGAVEL_CUTOFF ) / ( CAMO_DAMPENINGAVEL_MAX - CAMO_DAMPENINGAVEL_CUTOFF );
		frac2 = MIN( 0.0f, frac2 );
		frac2 = MAX( 1.0f, frac2 );
		flSuppression = 50.0f * frac2;
	}

	// Add in velocity blend
	float vel = GetAbsVelocity().Length();
	if ( vel > CAMO_DAMPENINGVELOCITY_CUTOFF )
	{
		float frac = ( vel- CAMO_DAMPENINGVELOCITY_CUTOFF ) / ( CAMO_DAMPENINGVELOCITY_MAX - CAMO_DAMPENINGVELOCITY_CUTOFF );
		frac = MIN( 1.0f, frac );
		flSuppression = MIN( 100.f, flSuppression + (100.0f * frac) );
	}

	// Set the camo aount
	SetMovementCamoSuppression( flSuppression );
	if ( flSuppression )
	{
		m_flMovementCamoSuppressionStayoutTime = gpGlobals->curtime + CAMO_MOVEMENT_PENALTYTIME;
	}

	if ( m_flGoalMovementCamoSuppressionAmount == m_flMovementCamoSuppression )
		return;

	// Camouflage
	float dt = gpGlobals->frametime;
	float changeamount = ( dt * CAMO_MOVESUPPRESSION_CHANGERATE );

	// Move but don't overshoot
	if ( m_flGoalMovementCamoSuppressionAmount > m_flMovementCamoSuppression )
	{
		m_flMovementCamoSuppression += changeamount;
		m_flMovementCamoSuppression = MIN( m_flMovementCamoSuppression, m_flGoalMovementCamoSuppressionAmount );
	}
	else
	{
		if ( gpGlobals->curtime < m_flMovementCamoSuppressionStayoutTime )
			return;

		m_flMovementCamoSuppression -= changeamount;
		m_flMovementCamoSuppression = MAX( m_flMovementCamoSuppression, m_flGoalMovementCamoSuppressionAmount );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Get the material used for this player's camo
//-----------------------------------------------------------------------------
IMaterial *C_BaseTFPlayer::GetCamoMaterial( void )
{
	if ( !m_pCamoEffectMaterial )
	{
		m_pCamoEffectMaterial = materials->FindMaterial("player/infiltratorcamo/infiltratorcamo", TEXTURE_GROUP_CLIENT_EFFECTS);
		if ( m_pCamoEffectMaterial )
		{
			m_pCamoEffectMaterial->IncrementReferenceCount();
		}
	}

	return m_pCamoEffectMaterial;
}

//-----------------------------------------------------------------------------
// Purpose: Percentage suppression of camo effect up close based on velocity
// Output : float
//-----------------------------------------------------------------------------
float C_BaseTFPlayer::GetMovementCamoSuppression( void )
{
	return m_flMovementCamoSuppression;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BaseTFPlayer::CheckLastMovement( void )
{
	if ( GetAbsOrigin() != m_vecLastOrigin )
	{
		m_vecLastOrigin = GetAbsOrigin();
		m_flLastMoveTime = gpGlobals->curtime;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : float
//-----------------------------------------------------------------------------
float C_BaseTFPlayer::GetLastMoveTime( void )
{
	return m_flLastMoveTime;
}

float C_BaseTFPlayer::GetLastDamageTime( void ) const
{
	return m_flLastDamageTime;
}

float C_BaseTFPlayer::GetLastGainHealthTime( void ) const
{
	return m_flLastGainHealthTime;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : float
//-----------------------------------------------------------------------------
float C_BaseTFPlayer::GetOverlayAlpha( void )
{
	float alpha = 1.0f;

	C_BaseTFPlayer *local = GetLocalPlayer();
	if ( local && local != this )
	{
		if ( local && GetTeamNumber() != local->GetTeamNumber() )
		{
			if ( IsCamouflaged() )
			{
				float frac = ( GetCamouflageAmount() / 100.0f );
				alpha *= ( 1.0f - frac );
			}

			// Only applies to sniper right now
			if ( m_iPlayerClass == TFCLASS_SNIPER )
			{
				float dt = gpGlobals->curtime - GetLastMoveTime();
				if ( dt > SNIPER_STATIONARY_FADESTART )
				{
					if ( dt > SNIPER_STATIONARY_FADEFINISH )
					{
						alpha = 0.0;
					}
					else
					{
						float frac = ( dt - SNIPER_STATIONARY_FADESTART ) / ( SNIPER_STATIONARY_FADEFINISH - SNIPER_STATIONARY_FADESTART );
						alpha *= ( 1.0f - frac );
					}
				}
			}
		}
	}

	alpha = MIN( 1.0f, alpha );
	alpha = MAX( 0.0f, alpha );

	return alpha;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool C_BaseTFPlayer::IsDeployed( void )
{
	return m_bDeployed;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool C_BaseTFPlayer::IsDeploying( void )
{
	return m_bDeploying;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool C_BaseTFPlayer::IsUnDeploying( void )
{
	return m_bUnDeploying;
}
			    
//-----------------------------------------------------------------------------
// Purpose: Return true if the player's allowed to switch weapons in his current state
//-----------------------------------------------------------------------------
bool C_BaseTFPlayer::IsAllowedToSwitchWeapons( void )
{
	// Can't switch while deployed
	if ( IsDeployed() || IsDeploying() || IsUnDeploying() )
		return false;

	if ( IsInAVehicle() && GetVehicle() )
	{
		IClientVehicle *pVehicle = GetVehicle();
		int nRole = pVehicle->GetPassengerRole( this );
		if (!pVehicle->IsPassengerUsingStandardWeapons(nRole))
		{
			return false;
		}
	}

	// See if the weapon will allow us to switch
	if ( GetActiveWeapon() && GetActiveWeapon()->IsAllowedToSwitch() == false )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Get the number of objects of the specified type that this player has
//-----------------------------------------------------------------------------
int C_BaseTFPlayer::GetNumObjects( int iObjectType )
{
	int iCount = 0;
	for (int i = 0; i < GetObjectCount(); i++)
	{
		if ( !GetObject(i) )
			continue;

		if ( GetObject(i)->GetType() == iObjectType )
		{
			iCount++;
		}
	}

	return iCount;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	C_BaseTFPlayer::GetObjectCount( void )
{
	return m_TFLocal.m_aObjects.Count();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_BaseObject *C_BaseTFPlayer::GetObject( int index )
{
	return m_TFLocal.m_aObjects[index].Get();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BaseTFPlayer::AddEntity( void )
{
	BaseClass::AddEntity();

	// Zero out model pitch, blending takes care of all of it.
	SetLocalAnglesDim( X_INDEX, 0 );

	m_PlayerAnimState.Update();
}

//-----------------------------------------------------------------------------
// Purpose: See if it's time to start another adrenalin effect
//-----------------------------------------------------------------------------
void C_BaseTFPlayer::CheckAdrenalin( void )
{
	if ( m_flNextAdrenalinEffect && gpGlobals->curtime > m_flNextAdrenalinEffect )
	{
		ScreenFade_t sf;
		memset( &sf, 0, sizeof( sf ) );
		sf.a = 128;
		sf.r = 0;
		sf.g = 128;
		sf.b = 0;
		// One second
		sf.duration = (unsigned short)((float)(1<<SCREENFADE_FRACBITS) * 1.0f );
		if ( m_bFadingIn )
		{
			sf.fadeFlags = FFADE_IN;
		}
		else
		{
			sf.fadeFlags = FFADE_OUT;
		}
		vieweffects->Fade( sf );

		m_bFadingIn = !m_bFadingIn;
		m_flNextAdrenalinEffect = gpGlobals->curtime + 1.0;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Update this client's target entity
//-----------------------------------------------------------------------------
void C_BaseTFPlayer::UpdateIDTarget( void )
{
	if ( !IsLocalPlayer() )
		return;

	// If the server's forcing us to a specific ID target, use it instead
	if ( m_TFLocal.m_iIDEntIndex )
	{
		m_iIDEntIndex = m_TFLocal.m_iIDEntIndex;
		return;
	}

	// Clear old target and find a new one
	m_iIDEntIndex = 0;

	trace_t tr;
	Vector vecStart, vecEnd;
	VectorMA( MainViewOrigin(), 1500, MainViewForward(), vecEnd );
	VectorMA( MainViewOrigin(), 48,   MainViewForward(), vecStart );
	UTIL_TraceLine( vecStart, vecEnd, MASK_SOLID, NULL, COLLISION_GROUP_NONE, &tr );
	if ( tr.DidHitNonWorldEntity() )
	{
		C_BaseEntity *pEntity = tr.m_pEnt;
		IClientVehicle *vehicle = GetVehicle();
		C_BaseEntity *pVehicleEntity = vehicle ? vehicle->GetVehicleEnt() : NULL;

		if ( pEntity && (pEntity != this) && (pEntity != pVehicleEntity) )
		{
			// Make sure it's not an object
			if ( !dynamic_cast<C_BaseObject*>( pEntity ) )
			{
				m_iIDEntIndex = pEntity->entindex();
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Return the weapon to have open the weapon selection on, based upon our currently active weapon
//			If the currently active weapon is a two handed weapon, returns it's primary weapon.
//-----------------------------------------------------------------------------
C_BaseCombatWeapon *C_BaseTFPlayer::GetActiveWeaponForSelection( void )
{
	C_WeaponTwoHandedContainer *pTwoHandedWeapon = dynamic_cast<C_WeaponTwoHandedContainer*>( GetActiveWeapon() );
	if ( pTwoHandedWeapon )
		return pTwoHandedWeapon->m_hLeftWeapon;

	return BaseClass::GetActiveWeaponForSelection();
}

//-----------------------------------------------------------------------------
// HACK: Until we get a recon model, trail recon particles
//-----------------------------------------------------------------------------
void FX_ReconParticle( const Vector &vecOrigin, bool bBlue )
{
	CSmartPtr<CSimpleEmitter> pSimple = CSimpleEmitter::Create( "reconparticle" );
	pSimple->SetSortOrigin( vecOrigin );
	pSimple->SetNearClip( 32, 64 );

	SimpleParticle	*pParticle;

	Vector	offset;

	for ( int i = 0; i < 1; i++ )
	{
		pParticle = (SimpleParticle *) pSimple->AddParticle( sizeof(SimpleParticle), g_Mat_DustPuff[0], vecOrigin );

		if ( pParticle != NULL )
		{			
			pParticle->m_flLifetime		= 0.0f;
			pParticle->m_flDieTime		= random->RandomFloat( 1.0f, 1.5f );
			
			pParticle->m_vecVelocity	= vec3_origin;

			int	color = random->RandomInt( 128, 192 );

			if ( bBlue )
			{
				pParticle->m_uchColor[0]	= color;
				pParticle->m_uchColor[1]	= color;
				pParticle->m_uchColor[2]	= 255;
			}
			else
			{
				pParticle->m_uchColor[0]	= 255;
				pParticle->m_uchColor[1]	= color;
				pParticle->m_uchColor[2]	= color;
			}
			pParticle->m_uchStartAlpha	= random->RandomInt( 192, 255 );
			pParticle->m_uchEndAlpha	= 0;
			pParticle->m_uchStartSize	= random->RandomInt( 12, 16 );
			pParticle->m_uchEndSize		= 0;
			pParticle->m_flRoll			= random->RandomInt( 0, 360 );
			pParticle->m_flRollDelta	= random->RandomFloat( -1.0f, 1.0f );
		}
	}
}

// Prediction stuff
void C_BaseTFPlayer::PreThink( void )
{
	BaseClass::PreThink();

	// Chain pre-think to player class.
	if ( GetPlayerClass() )
	{
		GetPlayerClass()->PreClassThink();
	}
}

void C_BaseTFPlayer::PostThink( void )
{
	BaseClass::PostThink();

	// Chain post-think to player class.
	if ( GetPlayerClass() )
	{
		GetPlayerClass()->PostClassThink();
	}
}

C_PlayerClass *C_BaseTFPlayer::GetPlayerClass( void )
{
	return m_PlayerClasses.GetPlayerClass( PlayerClass() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BaseTFPlayer::SetVehicleRole( int nRole )
{
	if ( IsInAVehicle() )
	{
		C_BaseTFVehicle *pVehicle = ( C_BaseTFVehicle* )GetVehicle();
		if ( pVehicle )
		{
			if ( nRole >= pVehicle->GetMaxPassengerCount() )
				return;
		}
	}

	char szCmd[64];
	Q_snprintf( szCmd, sizeof( szCmd ), "vehicleRole %i\n", nRole );
	engine->ServerCmd( szCmd );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool C_BaseTFPlayer::CanGetInVehicle( void )
{
	if ( GetPlayerClass() )
	{
		return GetPlayerClass()->CanGetInVehicle();
	}

	return true;
}


// How fast to avoid collisions with center of other object, in units per second
#define AVOID_SPEED 1000.0f
extern ConVar cl_forwardspeed;
extern ConVar cl_backspeed;
extern ConVar cl_sidespeed;

static ConVar tf2_solidplayers( "tf2_solidplayers", "1", 0, "Treat players and objects as solid." );

//-----------------------------------------------------------------------------
// Client-side obstacle avoidance
//-----------------------------------------------------------------------------
void C_BaseTFPlayer::PerformClientSideObstacleAvoidance( float flFrameTime, CUserCmd *pCmd )
{
	if ( !tf2_solidplayers.GetBool() )
	{
		return;
	}

	// Don't avoid if noclipping or in movetype none
	switch ( GetMoveType() )
	{
	case MOVETYPE_NOCLIP:
	case MOVETYPE_NONE:
		return;
	default:
		break;
	}

	// Try to steer away from any objects/players we might interpenetrate
	Vector size = WorldAlignSize();

	float radius = 0.5f * sqrt( size.x * size.x + size.y * size.y );
	float curspeed = GetAbsVelocity().Length2D();

	// int slot = 1;

	//engine->Con_NPrintf( slot++, "speed %f\n", curspeed );
	//engine->Con_NPrintf( slot++, "radius %f\n", radius );

	// If running, use a larger radius
	if ( curspeed > 100.0f )
	{
		float factor = ( 1.0f + ( curspeed - 100.0f ) / 100.0f );

		// engine->Con_NPrintf( slot++, "scaleup (%f) to radius %f\n", factor, radius * factor );

		radius = radius * factor;
	}

	CPlayerAndObjectEnumerator avoid( radius );
	partition->EnumerateElementsInSphere( PARTITION_CLIENT_SOLID_EDICTS, GetAbsOrigin(), radius, false, &avoid );

	// Okay, decide how to avoid if there's anything close by
	int c = avoid.GetObjectCount();
	if ( c <= 0 )
		return;

	Vector currentdir;
	Vector rightdir;
	AngleVectors( pCmd->viewangles, &currentdir, &rightdir, NULL );
	
	bool istryingtomove = false;
	bool ismovingforward = false;
	if ( fabs( pCmd->forwardmove ) > 0.0f || 
		 fabs( pCmd->sidemove ) > 0.0f )
	{
		istryingtomove = true;
		if ( pCmd->forwardmove > 1.0f )
		{
			ismovingforward = true;
		}
	}

	//engine->Con_NPrintf( slot++, "moving %s forward %s\n", istryingtomove ? "true" : "false", ismovingforward ? "true" : "false"  );

	float adjustforwardmove = 0.0f;
	float adjustsidemove	= 0.0f;

	int i;
	for ( i = 0; i < c; i++ )
	{
		C_BaseEntity *obj = avoid.GetObject( i );
		if( !obj )
			continue;

		float flHit1, flHit2;

		// Figure out a 2D radius for the object
		Vector vecWorldMins, vecWorldMaxs;
		obj->CollisionProp()->WorldSpaceAABB( &vecWorldMins, &vecWorldMaxs );
		Vector objSize = vecWorldMaxs - vecWorldMins;

		float objectradius = /*0.5f **/ 1.0 * sqrt( objSize.x * objSize.x + objSize.y * objSize.y );

		if ( !IntersectInfiniteRayWithSphere(
				GetAbsOrigin(),
				currentdir,
				obj->GetAbsOrigin(),
				objectradius,
				&flHit1,
				&flHit2 ) )
			continue;

		float force = 0.0f;

		float forward = 0.0f, side = 0.0f;

		Vector vecToObject = obj->GetAbsOrigin() - GetAbsOrigin();
		Vector cross = vecToObject.Cross( currentdir );

		//engine->Con_NPrintf( slot++, "object side %s\n", sign > 0.0f ? "right" : "left" );

		if ( 0 && istryingtomove )
		{
/*
			// Okay, line hits sphere in two points
			// Determine how close line is to center of sphere and move sideways to avoid if we are 
			//  actually trying to move forward
			Vector deltaHit = vHit2 - vHit1;
			float leg1 = ( deltaHit.Length() ) / 2.0f;

			float distfromcenter = sqrt( leg1 * leg1 + objectradius * objectradius );

			force = distfromcenter / radius;
			force = clamp( force, 0.0f, 1.0f );
			force = 1.0f - force;

			if ( force <= 0.5f )
				continue;

			side = force * AVOID_SPEED;

			// Move to right or left of object
			side *= sign;
*/
		}
		else
		{
			Vector deltaObject = vecToObject;
			float dist = deltaObject.Length2D();

			force = dist / radius;
			force = clamp( force, 0.0f, 1.0f );
			force = 1.0f - force;

			//engine->Con_NPrintf( slot++, "dist %f/radius %f == %f\n", dist, radius, force );

			if ( force <= 0.3f )
				continue;

			force = sqrt( force );

			//engine->Con_NPrintf( slot++, "sqrt(force) == %f\n", force );

			Vector moveDir = -vecToObject;
			VectorNormalize( moveDir );

			float fwd = currentdir.Dot( moveDir );
			float rt = rightdir.Dot( moveDir );

			//engine->Con_NPrintf( slot++, "fwd %f right %f\n", fwd, rt );

			float sidescale = 2.0f;
			float forwardscale = 1.0f;

			if ( istryingtomove )
			{
				// If running, then do a lot more sideways veer since we're not going to do anything to
				//  forward velocity
				sidescale = 4.0f;
				forwardscale = 2.0f;
			}

			forward = forwardscale * fwd * force * AVOID_SPEED;
			side = sidescale * rt * force * AVOID_SPEED;

			//engine->Con_NPrintf( slot++, "forward %f side %f\n", forward, side );
		}

		adjustforwardmove	+= forward;
		adjustsidemove		+= side;
	}

	pCmd->forwardmove	+= adjustforwardmove;
	pCmd->sidemove		+= adjustsidemove;

	if ( pCmd->forwardmove > 0.0f )
	{
		pCmd->forwardmove = clamp( pCmd->forwardmove, -cl_forwardspeed.GetFloat(), cl_forwardspeed.GetFloat() );
	}
	else
	{
		pCmd->forwardmove = clamp( pCmd->forwardmove, -cl_backspeed.GetFloat(), cl_backspeed.GetFloat() );
	}
	pCmd->sidemove = clamp( pCmd->sidemove, -cl_sidespeed.GetFloat(), cl_sidespeed.GetFloat() );
}


//-----------------------------------------------------------------------------
// Purpose: Input handling
//-----------------------------------------------------------------------------
void C_BaseTFPlayer::CreateMove( float flInputSampleTime, CUserCmd *pCmd )
{
	BaseClass::CreateMove( flInputSampleTime, pCmd );

	// If the frozen flag is set, prevent view movement (server prevents the rest of the movement)
	if ( GetFlags() & FL_FROZEN )
	{
		return;
	}

	if (!IsInVGuiInputMode() && !IsInAVehicle())
	{
		PerformClientSideObstacleAvoidance( TICK_INTERVAL, pCmd );
	}
}


C_BaseAnimating* C_BaseTFPlayer::GetRenderedWeaponModel()
{
	// Attach to either their weapon model or their view model.
	if ( C_BasePlayer::ShouldDrawLocalPlayer() || !IsLocalPlayer() )
	{
		// Hook it to their external weapon model.
		C_BaseCombatWeapon *pWeapon = GetActiveWeapon();
		if ( !pWeapon )
			return NULL;

		// If this a two-handed container (shield + weapon), return the left weapon.
		C_WeaponTwoHandedContainer *pContainer = dynamic_cast< C_WeaponTwoHandedContainer* >( pWeapon );
		if ( pContainer )
		{
			return pContainer->GetLeftWeapon();
		}
		else
		{
			return pWeapon;
		}
	}
	else
	{
		return GetViewModel();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BaseTFPlayer::SetIDEnt( C_BaseEntity *pEntity )
{
	if ( pEntity )
		m_TFLocal.m_iIDEntIndex = pEntity->entindex();
	else
		m_TFLocal.m_iIDEntIndex = 0;
}


C_VehicleTeleportStation* C_BaseTFPlayer::GetSelectedMCV() const
{
	return dynamic_cast< C_VehicleTeleportStation* >( m_hSelectedMCV.Get() );
}

//-----------------------------------------------------------------------------
// Purpose: Return true if this object can be +used by the player
//-----------------------------------------------------------------------------
bool C_BaseTFPlayer::IsUseableEntity( CBaseEntity *pEntity )
{
	// I can use vehicles
	return dynamic_cast<C_BaseTFVehicle*>( pEntity );
}

//-----------------------------------------------------------------------------
// Purpose: Powerup has just started
//-----------------------------------------------------------------------------
void C_BaseTFPlayer::PowerupStart( int iPowerup, bool bInitial )
{
	Assert( iPowerup >= 0 && iPowerup < MAX_POWERUPS );

	switch( iPowerup )
	{
	case POWERUP_RUSH:
		{
			// Play the rage start
			if ( bInitial )
			{
				EmitSound( "BaseTFPlayer.Rage" );
			}

			// Start the looping breathing
			CPASAttenuationFilter filter( this, "BaseTFPlayer.HeavyBreathing" );
			EmitSound( filter, entindex(), "BaseTFPlayer.HeavyBreathing" );

			if ( IsLocalPlayer() )
			{
				// Start the visual effects
				if ( !m_flNextAdrenalinEffect )
				{
					m_flNextAdrenalinEffect = gpGlobals->curtime;
					m_bFadingIn = false;
				}
			}
		}
		break;

	default:
		break;
	}

	BaseClass::PowerupStart( iPowerup, bInitial );
}

//-----------------------------------------------------------------------------
// Purpose: Powerup has just finished
//-----------------------------------------------------------------------------
void C_BaseTFPlayer::PowerupEnd( int iPowerup )
{
	Assert( iPowerup >= 0 && iPowerup < MAX_POWERUPS );

	switch( iPowerup )
	{
	case POWERUP_RUSH:
		{
			// Stop the looping breathing
			StopSound( "BaseTFPlayer.HeavyBreathing" );

			if ( IsLocalPlayer() )
			{
				// Stop the visual effects
				if ( m_flNextAdrenalinEffect )
				{
					vieweffects->ClearAllFades();
				}

				m_flNextAdrenalinEffect = 0;
			}
		}
		break;

	default:
		break;
	}

	BaseClass::PowerupEnd( iPowerup );
}
