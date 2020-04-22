//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#include "cbase.h"
#include "c_dod_player.h"
#include "c_user_message_register.h"
#include "view.h"
#include "iclientvehicle.h"
#include "ivieweffects.h"
#include "input.h"
#include "IEffects.h"
#include "fx.h"
#include "c_basetempentity.h"
#include "hud_macros.h"
#include "engine/ivdebugoverlay.h"
#include "smoke_fog_overlay.h"
#include "bone_setup.h"
#include "in_buttons.h"
#include "r_efx.h"
#include "dlight.h"
#include "shake.h"
#include "cl_animevent.h"
#include "fx_dod_blood.h"
#include "effect_dispatch_data.h"	//for water ripple / splash effect
#include "c_te_effect_dispatch.h"	//ditto
#include "dod_gamerules.h"
#include <igameevents.h>
#include "physpropclientside.h"
#include "obstacle_pushaway.h"
#include "prediction.h"
#include "viewangleanim.h"
#include "soundenvelope.h"
#include "weapon_dodbipodgun.h"
#include "c_dod_basegrenade.h"
#include "dod_weapon_parse.h"
#include "view_scene.h"
#include "dod_headiconmanager.h"
#include "c_world.h"
#include "c_dod_bombtarget.h"
#include "toolframework/itoolframework.h"
#include "toolframework_client.h"
#include "c_team.h"
#include "collisionutils.h"
#include "weapon_dodsniper.h"
// NVNT - haptics system for spectating and grenades
#include "haptics/haptic_utils.h"
// NVNT - for grenade effects
#include "weapon_dodbasegrenade.h"
// NVNT - for planting bomb effect
#include "weapon_dodbasebomb.h"

#if defined( CDODPlayer )
	#undef CDODPlayer
#endif

#include "iviewrender_beams.h"			// flashlight beam

#include "materialsystem/imesh.h"		//for materials->FindMaterial
#include "iviewrender.h"				//for view->
ConVar cl_ragdoll_physics_enable( "cl_ragdoll_physics_enable", "1", 0, "Enable/disable ragdoll physics." );

ConVar cl_autoreload( "cl_autoreload", "1", FCVAR_USERINFO | FCVAR_ARCHIVE, "Set to 1 to auto reload your weapon when it is empty" );
ConVar cl_autorezoom( "cl_autorezoom", "1", FCVAR_USERINFO | FCVAR_ARCHIVE, "When set to 1, sniper rifles and bazooka weapons will automatically raise after each shot" );

#include "tier0/memdbgon.h"

//======================================================
//
// Cold Breath Emitter - for DOD players.
//
class ColdBreathEmitter : public CSimpleEmitter
{
public:
	
	ColdBreathEmitter( const char *pDebugName ) : CSimpleEmitter( pDebugName ) {}

	static ColdBreathEmitter *Create( const char *pDebugName )
	{
		return new ColdBreathEmitter( pDebugName );
	}

	void UpdateVelocity( SimpleParticle *pParticle, float timeDelta )
	{
		// Float up when lifetime is half gone.
		pParticle->m_vecVelocity[2] -= ( 8.0f * timeDelta );


		// FIXME: optimize this....
		pParticle->m_vecVelocity *= ExponentialDecay( 0.9, 0.03, timeDelta );
	}

	virtual	float UpdateRoll( SimpleParticle *pParticle, float timeDelta )
	{
		pParticle->m_flRoll += pParticle->m_flRollDelta * timeDelta;
		
		pParticle->m_flRollDelta += pParticle->m_flRollDelta * ( timeDelta * -2.0f );

		//Cap the minimum roll
		if ( fabs( pParticle->m_flRollDelta ) < 0.5f )
		{
			pParticle->m_flRollDelta = ( pParticle->m_flRollDelta > 0.0f ) ? 0.5f : -0.5f;
		}

		return pParticle->m_flRoll;
	}

private:

	ColdBreathEmitter( const ColdBreathEmitter & );
};


void RecvProxy_StunTime( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	C_DODPlayer *pPlayerData = (C_DODPlayer *) pStruct;

	if( pPlayerData != C_BasePlayer::GetLocalPlayer() )
		return;

	if ( (pPlayerData->m_flStunDuration != pData->m_Value.m_Float) && pData->m_Value.m_Float > 0 )
	{
		pPlayerData->m_flStunAlpha = 1;
	}

	pPlayerData->m_flStunDuration = pData->m_Value.m_Float;
	pPlayerData->m_flStunEffectTime = gpGlobals->curtime + pPlayerData->m_flStunDuration;
}

// -------------------------------------------------------------------------------- //
// Player animation event. Sent to the client when a player fires, jumps, reloads, etc..
// -------------------------------------------------------------------------------- //

class C_TEPlayerAnimEvent : public C_BaseTempEntity
{
public:
	DECLARE_CLASS( C_TEPlayerAnimEvent, C_BaseTempEntity );
	DECLARE_CLIENTCLASS();

	virtual void PostDataUpdate( DataUpdateType_t updateType )
	{
		// Create the effect.
		C_DODPlayer *pPlayer = dynamic_cast< C_DODPlayer* >( m_hPlayer.Get() );
		if ( pPlayer && !pPlayer->IsDormant() )
		{
			pPlayer->DoAnimationEvent( (PlayerAnimEvent_t)m_iEvent.Get(), m_nData );
		}	
	}

public:
	CNetworkHandle( CBasePlayer, m_hPlayer );
	CNetworkVar( int, m_iEvent );
	CNetworkVar( int, m_nData );
};

IMPLEMENT_CLIENTCLASS_EVENT( C_TEPlayerAnimEvent, DT_TEPlayerAnimEvent, CTEPlayerAnimEvent );


BEGIN_RECV_TABLE_NOBASE( C_TEPlayerAnimEvent, DT_TEPlayerAnimEvent )
	RecvPropEHandle( RECVINFO( m_hPlayer ) ),
	RecvPropInt( RECVINFO( m_iEvent ) ),
	RecvPropInt( RECVINFO( m_nData ) )
END_RECV_TABLE()

void RecvProxy_CapAreaIndex( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	CDODPlayerShared *pShared = ( CDODPlayerShared *)pStruct;

	int iCapAreaIndex = pData->m_Value.m_Int;

	if ( iCapAreaIndex != pShared->GetCPIndex() )
	{
		pShared->SetCPIndex( iCapAreaIndex );
	}
}

// ------------------------------------------------------------------------------------------ //
// Data tables.
// ------------------------------------------------------------------------------------------ //

// CDODPlayerShared Data Tables
//=============================

// specific to the local player ( ideally should not be in CDODPlayerShared! )
BEGIN_RECV_TABLE_NOBASE( CDODPlayerShared, DT_DODSharedLocalPlayerExclusive )
	RecvPropInt( RECVINFO( m_iPlayerClass ) ),
	RecvPropInt( RECVINFO( m_iDesiredPlayerClass ) ),
	RecvPropFloat( RECVINFO( m_flDeployedYawLimitLeft ) ),
	RecvPropFloat( RECVINFO( m_flDeployedYawLimitRight ) ),
	RecvPropInt( RECVINFO( m_iCPIndex ), 0, RecvProxy_CapAreaIndex ),
	RecvPropArray3( RECVINFO_ARRAY( m_bPlayerDominated ), RecvPropBool( RECVINFO( m_bPlayerDominated[0] ) ) ),
	RecvPropArray3( RECVINFO_ARRAY( m_bPlayerDominatingMe ), RecvPropBool( RECVINFO( m_bPlayerDominatingMe[0] ) ) ),
END_RECV_TABLE()

// main table
BEGIN_RECV_TABLE_NOBASE( CDODPlayerShared, DT_DODPlayerShared )
	RecvPropFloat( RECVINFO( m_flStamina ) ),
	RecvPropTime( RECVINFO( m_flSlowedUntilTime ) ),
	RecvPropBool( RECVINFO( m_bProne ) ),
	RecvPropBool( RECVINFO( m_bIsSprinting ) ),
	RecvPropTime( RECVINFO( m_flGoProneTime ) ),
	RecvPropTime( RECVINFO( m_flUnProneTime ) ),
	RecvPropTime( RECVINFO( m_flDeployChangeTime ) ),
	RecvPropFloat( RECVINFO( m_flDeployedHeight ) ),
	RecvPropBool( RECVINFO( m_bPlanting ) ),
	RecvPropBool( RECVINFO( m_bDefusing ) ),
	RecvPropDataTable( "dodsharedlocaldata", 0, 0, &REFERENCE_RECV_TABLE(DT_DODSharedLocalPlayerExclusive) ),
END_RECV_TABLE()


// C_DODPlayer Data Tables
//=========================

// specific to the local player
BEGIN_RECV_TABLE_NOBASE( C_DODPlayer, DT_DODLocalPlayerExclusive )
	RecvPropVector( RECVINFO_NAME( m_vecNetworkOrigin, m_vecOrigin ) ),
	RecvPropFloat( RECVINFO( m_flStunDuration ), 0, RecvProxy_StunTime ),
	RecvPropFloat( RECVINFO( m_flStunMaxAlpha)),
	RecvPropInt( RECVINFO( m_iProgressBarDuration ) ),
	RecvPropFloat( RECVINFO( m_flProgressBarStartTime ) ),
END_RECV_TABLE()

// all players except the local player
BEGIN_RECV_TABLE_NOBASE( C_DODPlayer, DT_DODNonLocalPlayerExclusive )
	RecvPropVector( RECVINFO_NAME( m_vecNetworkOrigin, m_vecOrigin ) ),
END_RECV_TABLE()

// main table
IMPLEMENT_CLIENTCLASS_DT( C_DODPlayer, DT_DODPlayer, CDODPlayer )
	RecvPropDataTable( RECVINFO_DT( m_Shared ), 0, &REFERENCE_RECV_TABLE( DT_DODPlayerShared ) ),

	RecvPropDataTable( "dodlocaldata", 0, 0, &REFERENCE_RECV_TABLE(DT_DODLocalPlayerExclusive) ),
	RecvPropDataTable( "dodnonlocaldata", 0, 0, &REFERENCE_RECV_TABLE(DT_DODNonLocalPlayerExclusive) ),

	RecvPropFloat( RECVINFO( m_angEyeAngles[0] ) ),
	RecvPropFloat( RECVINFO( m_angEyeAngles[1] ) ),
	RecvPropEHandle( RECVINFO( m_hRagdoll ) ),
	RecvPropBool( RECVINFO( m_bSpawnInterpCounter ) ),
	RecvPropInt( RECVINFO( m_iAchievementAwardsMask ) ),

END_RECV_TABLE()


// ------------------------------------------------------------------------------------------ //
// Prediction tables.
// ------------------------------------------------------------------------------------------ //
BEGIN_PREDICTION_DATA_NO_BASE( CDODPlayerShared )
	DEFINE_PRED_FIELD( m_bProne, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_flStamina, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bIsSprinting, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_flGoProneTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_flUnProneTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_flDeployChangeTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_flDeployedHeight, FIELD_FLOAT, FTYPEDESC_INSENDTABLE )
END_PREDICTION_DATA()

BEGIN_PREDICTION_DATA( C_DODPlayer )
	DEFINE_PRED_TYPEDESCRIPTION( m_Shared, CDODPlayerShared ),
	DEFINE_PRED_FIELD( m_flCycle, FIELD_FLOAT, FTYPEDESC_OVERRIDE | FTYPEDESC_PRIVATE | FTYPEDESC_NOERRORCHECK ),
	DEFINE_PRED_FIELD( m_nSequence, FIELD_INTEGER, FTYPEDESC_OVERRIDE | FTYPEDESC_PRIVATE | FTYPEDESC_NOERRORCHECK ),
END_PREDICTION_DATA()


// ----------------------------------------------------------------------------- //
// Client ragdoll entity.
// ----------------------------------------------------------------------------- //

ConVar cl_low_violence( "cl_low_violence", "0" );
ConVar cl_ragdoll_fade_time( "cl_ragdoll_fade_time", "15", FCVAR_CLIENTDLL );
ConVar cl_ragdoll_pronecheck_distance( "cl_ragdoll_pronecheck_distance", "64", FCVAR_GAMEDLL );

class C_DODRagdoll : public C_BaseAnimatingOverlay
{
public:
	DECLARE_CLASS( C_DODRagdoll, C_BaseAnimatingOverlay );
	DECLARE_CLIENTCLASS();
	
	C_DODRagdoll();
	~C_DODRagdoll();

	virtual void OnDataChanged( DataUpdateType_t type );

	int GetPlayerEntIndex() const;
	IRagdoll* GetIRagdoll() const;

	void ImpactTrace( trace_t *pTrace, int iDamageType, const char *pCustomImpactName );

	void ClientThink( void );
	void StartFadeOut( float fDelay );
	
	bool IsRagdollVisible();
private:
	
	C_DODRagdoll( const C_DODRagdoll & ) {}

	void Interp_Copy( C_BaseAnimatingOverlay *pSourceEntity );

	void CreateLowViolenceRagdoll();
	void CreateDODRagdoll();

private:

	EHANDLE	m_hPlayer;
	CNetworkVector( m_vecRagdollVelocity );
	CNetworkVector( m_vecRagdollOrigin );
	float m_fDeathTime;
	bool  m_bFadingOut;
};


IMPLEMENT_CLIENTCLASS_DT_NOBASE( C_DODRagdoll, DT_DODRagdoll, CDODRagdoll )
	RecvPropVector( RECVINFO(m_vecRagdollOrigin) ),
	RecvPropEHandle( RECVINFO( m_hPlayer ) ),
	RecvPropInt( RECVINFO( m_nModelIndex ) ),
	RecvPropInt( RECVINFO(m_nForceBone) ),
	RecvPropVector( RECVINFO(m_vecForce) ),
	RecvPropVector( RECVINFO( m_vecRagdollVelocity ) )
END_RECV_TABLE()


C_DODRagdoll::C_DODRagdoll()
{
	m_fDeathTime = -1;
	m_bFadingOut = false;
}

C_DODRagdoll::~C_DODRagdoll()
{
	PhysCleanupFrictionSounds( this );
}

void C_DODRagdoll::Interp_Copy( C_BaseAnimatingOverlay *pSourceEntity )
{
	if ( !pSourceEntity )
		return;
	
	VarMapping_t *pSrc = pSourceEntity->GetVarMapping();
	VarMapping_t *pDest = GetVarMapping();
    	
	// Find all the VarMapEntry_t's that represent the same variable.
	for ( int i = 0; i < pDest->m_Entries.Count(); i++ )
	{
		VarMapEntry_t *pDestEntry = &pDest->m_Entries[i];
		for ( int j=0; j < pSrc->m_Entries.Count(); j++ )
		{
			VarMapEntry_t *pSrcEntry = &pSrc->m_Entries[j];
			if ( !Q_strcmp( pSrcEntry->watcher->GetDebugName(),
				pDestEntry->watcher->GetDebugName() ) )
			{
				pDestEntry->watcher->Copy( pSrcEntry->watcher );
				break;
			}
		}
	}
}

void C_DODRagdoll::ImpactTrace( trace_t *pTrace, int iDamageType, const char *pCustomImpactName )
{
	IPhysicsObject *pPhysicsObject = VPhysicsGetObject();

	if( !pPhysicsObject )
		return;

	Vector dir = pTrace->endpos - pTrace->startpos;

	if ( iDamageType == DMG_BLAST )
	{
		dir *= 4000;  // adjust impact strength
				
		// apply force at object mass center
		pPhysicsObject->ApplyForceCenter( dir );
	}
	else
	{
		Vector hitpos;  
	
		VectorMA( pTrace->startpos, pTrace->fraction, dir, hitpos );
		VectorNormalize( dir );

		dir *= 4000;  // adjust impact strength

		// apply force where we hit it
		pPhysicsObject->ApplyForceOffset( dir, hitpos );	

		// Blood spray!
		FX_DOD_BloodSpray( hitpos, dir, 10 );
	}

	m_pRagdoll->ResetRagdollSleepAfterTime();
}


void C_DODRagdoll::CreateLowViolenceRagdoll()
{
	// Just play a death animation.
	// Find a death anim to play.
	int iMinDeathAnim = 9999, iMaxDeathAnim = -9999;
	for ( int iAnim=1; iAnim < 100; iAnim++ )
	{
		char str[512];
		Q_snprintf( str, sizeof( str ), "death%d", iAnim );
		if ( LookupSequence( str ) == -1 )
			break;
		
		iMinDeathAnim = MIN( iMinDeathAnim, iAnim );
		iMaxDeathAnim = MAX( iMaxDeathAnim, iAnim );
	}

	if ( iMinDeathAnim == 9999 )
	{
		CreateDODRagdoll();
	}
	else
	{
		int iDeathAnim = RandomInt( iMinDeathAnim, iMaxDeathAnim );
		char str[512];
		Q_snprintf( str, sizeof( str ), "death%d", iDeathAnim );

		SetSequence( LookupSequence( str ) );
		ForceClientSideAnimationOn();

		SetNetworkOrigin( m_vecRagdollOrigin );
		SetAbsOrigin( m_vecRagdollOrigin );
		SetAbsVelocity( m_vecRagdollVelocity );

		C_DODPlayer *pPlayer = dynamic_cast< C_DODPlayer* >( m_hPlayer.Get() );
		if ( pPlayer && !pPlayer->IsDormant() )
		{
			// move my current model instance to the ragdoll's so decals are preserved.
			pPlayer->SnatchModelInstance( this );

			SetAbsAngles( pPlayer->GetRenderAngles() );
			SetNetworkAngles( pPlayer->GetRenderAngles() );
		}
	
		Interp_Reset( GetVarMapping() );
	}
}

void C_DODRagdoll::CreateDODRagdoll()
{
	// First, initialize all our data. If we have the player's entity on our client,
	// then we can make ourselves start out exactly where the player is.
	C_DODPlayer *pPlayer = dynamic_cast< C_DODPlayer* >( m_hPlayer.Get() );

#ifdef _DEBUG
	DevMsg( 2, "CreateDODRagdoll %d %d\n", gpGlobals->framecount, pPlayer ? pPlayer->entindex() : 0 );
#endif
	
	if ( pPlayer && !pPlayer->IsDormant() )
	{
		// move my current model instance to the ragdoll's so decals are preserved.
		pPlayer->SnatchModelInstance( this );

		VarMapping_t *varMap = GetVarMapping();

		// Copy all the interpolated vars from the player entity.
		// The entity uses the interpolated history to get bone velocity.		
		if ( !pPlayer->IsLocalPlayer() && pPlayer->dod_IsInterpolationEnabled() )
		{
			Interp_Copy( pPlayer );

			SetAbsAngles( pPlayer->GetRenderAngles() );
			GetRotationInterpolator().Reset();

			m_flAnimTime = pPlayer->m_flAnimTime;
			SetSequence( pPlayer->GetSequence() );
			m_flPlaybackRate = pPlayer->GetPlaybackRate();
		}
		else
		{
			// This is the local player, so set them in a default
			// pose and slam their velocity, angles and origin
			SetAbsOrigin( m_vecRagdollOrigin );
			
			SetAbsAngles( pPlayer->GetRenderAngles() );

			SetAbsVelocity( m_vecRagdollVelocity );

			int iSeq = LookupSequence( "RagdollSpawn" );	// hax, find a neutral standing pose
			if ( iSeq == -1 )
			{
				Assert( false );	// missing look_idle?
				iSeq = 0;
			}
			
			SetSequence( iSeq );	// look_idle, basic pose
			SetCycle( 0.0 );

			Interp_Reset( varMap );
		}		

		m_nBody = pPlayer->GetBody();
	}
	else
	{
		// overwrite network origin so later interpolation will
		// use this position
		SetNetworkOrigin( m_vecRagdollOrigin );

		SetAbsOrigin( m_vecRagdollOrigin );
		SetAbsVelocity( m_vecRagdollVelocity );

		Interp_Reset( GetVarMapping() );
		
	}

	SetModelIndex( m_nModelIndex );
	
	// Turn it into a ragdoll.
	if ( cl_ragdoll_physics_enable.GetInt() )
	{
		// Make us a ragdoll..
		m_nRenderFX = kRenderFxRagdoll;

		matrix3x4_t boneDelta0[MAXSTUDIOBONES];
		matrix3x4_t boneDelta1[MAXSTUDIOBONES];
		matrix3x4_t currentBones[MAXSTUDIOBONES];
		const float boneDt = 0.05f;

		if ( pPlayer && pPlayer == C_BasePlayer::GetLocalPlayer() )
		{
			pPlayer->GetRagdollInitBoneArrays( boneDelta0, boneDelta1, currentBones, boneDt );
		}
		else
		{
			GetRagdollInitBoneArrays( boneDelta0, boneDelta1, currentBones, boneDt );
		}

		InitAsClientRagdoll( boneDelta0, boneDelta1, currentBones, boneDt );
	}
	else
	{
		ClientLeafSystem()->SetRenderGroup( GetRenderHandle(), RENDER_GROUP_TRANSLUCENT_ENTITY );
	}		

	// Fade out the ragdoll in a while
	StartFadeOut( cl_ragdoll_fade_time.GetFloat() );
	SetNextClientThink( gpGlobals->curtime + 5.0f );
}

void C_DODRagdoll::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );

	if ( type == DATA_UPDATE_CREATED )
	{
		if ( cl_low_violence.GetInt() )
		{
			CreateLowViolenceRagdoll();
		}
		else
		{
			CreateDODRagdoll();
		}
	}
	else 
	{
		if ( !cl_ragdoll_physics_enable.GetInt() )
		{
			// Don't let it set us back to a ragdoll with data from the server.
			m_nRenderFX = kRenderFxNone;
		}
	}
}

IRagdoll* C_DODRagdoll::GetIRagdoll() const
{
	return m_pRagdoll;
}

bool C_DODRagdoll::IsRagdollVisible()
{
	Vector vMins = Vector(-1,-1,-1);	//WorldAlignMins();
	Vector vMaxs = Vector(1,1,1);	//WorldAlignMaxs();
		
	Vector origin = GetAbsOrigin();
	
	if( !engine->IsBoxInViewCluster( vMins + origin, vMaxs + origin) )
	{
		return false;
	}
	else if( engine->CullBox( vMins + origin, vMaxs + origin ) )
	{
		return false;
	}

	return true;
}

void C_DODRagdoll::ClientThink( void )
{
	SetNextClientThink( CLIENT_THINK_ALWAYS );

	if ( m_bFadingOut == true )
	{
		int iAlpha = GetRenderColor().a;
		int iFadeSpeed = 600.0f;

		iAlpha = MAX( iAlpha - ( iFadeSpeed * gpGlobals->frametime ), 0 );

		SetRenderMode( kRenderTransAlpha );
		SetRenderColorA( iAlpha );

		if ( iAlpha == 0 )
		{
			//Release();
			AddEffects( EF_NODRAW );
		}

		return;
	}

	for( int iClient = 1; iClient <= gpGlobals->maxClients; ++iClient )
	{
		C_DODPlayer *pEnt = static_cast< C_DODPlayer *> ( UTIL_PlayerByIndex( iClient ) );

		if(!pEnt || !pEnt->IsPlayer())
			continue;

		if ( m_hPlayer == NULL )
			continue;

		if ( pEnt->entindex() == m_hPlayer->entindex() )
			continue;
		
		if ( pEnt->GetHealth() <= 0 )
			continue;

		if ( pEnt->m_Shared.IsProne() == false )
			continue;

		Vector vTargetOrigin = pEnt->GetAbsOrigin();
		Vector vMyOrigin =  GetAbsOrigin();

		Vector vDir = vTargetOrigin - vMyOrigin;

		if ( vDir.Length() > cl_ragdoll_pronecheck_distance.GetInt() ) 
			continue;

		SetNextClientThink( CLIENT_THINK_ALWAYS );
		m_bFadingOut = true;
		return;
	}

	// if the player is looking at us, delay the fade
	if ( IsRagdollVisible() )
	{
		StartFadeOut( 5.0 );
		return;
	}

	if ( m_fDeathTime > gpGlobals->curtime )
		return;

	//Release(); // Die
	AddEffects( EF_NODRAW );
}

void C_DODRagdoll::StartFadeOut( float fDelay )
{
	m_fDeathTime = gpGlobals->curtime + fDelay;
	SetNextClientThink( CLIENT_THINK_ALWAYS );
}

// ------------------------------------------------------------------------------------------ //
// C_DODPlayer implementation.
// ------------------------------------------------------------------------------------------ //
C_DODPlayer::C_DODPlayer() : 
	m_iv_angEyeAngles( "C_DODPlayer::m_iv_angEyeAngles" )
{
	m_PlayerAnimState = CreatePlayerAnimState( this );
	
	m_Shared.Init( this );

	m_flPitchRecoilAccumulator = 0.0;
	m_flYawRecoilAccumulator = 0.0;
	m_flRecoilTimeRemaining = 0.0;

	m_iProgressBarDuration = 0;
	m_flProgressBarStartTime = 0.0f;

	AddVar( &m_angEyeAngles, &m_iv_angEyeAngles, LATCH_SIMULATION_VAR );

	m_flProneViewOffset = 0.0;
	m_bProneSwayingRight = true;
	m_iIDEntIndex = 0;

	m_Hints.Init( this, NUM_HINTS, g_pszHintMessages );

	m_pFlashlightBeam = NULL;

	m_fNextThinkPushAway = 0.0f;

	// Cold breath.
	m_bColdBreathOn = false;
	m_flColdBreathTimeStart = 0.0f;
	m_flColdBreathTimeEnd = 0.0f;
	m_hColdBreathEmitter = NULL;
	m_hColdBreathMaterial = INVALID_MATERIAL_HANDLE;

	m_flHideHeadIconUntilTime = 0.0f;

	m_iAchievementAwardsMask = 0;
	m_pHeadIconMaterial = NULL;
}


C_DODPlayer::~C_DODPlayer()
{
	m_PlayerAnimState->Release();

	ReleaseFlashlight();

	// Kill the stamina sound!
	if ( m_pStaminaSound )
	{
		CSoundEnvelopeController::GetController().SoundDestroy( m_pStaminaSound );
		m_pStaminaSound = NULL;
	}

	// Cold breath.
	DestroyColdBreathEmitter();
}


C_DODPlayer* C_DODPlayer::GetLocalDODPlayer()
{
	return ToDODPlayer( C_BasePlayer::GetLocalPlayer() );
}

IRagdoll* C_DODPlayer::GetRepresentativeRagdoll() const
{
	if ( m_hRagdoll.Get() )
	{
		C_DODRagdoll *pRagdoll = (C_DODRagdoll*)m_hRagdoll.Get();

		return pRagdoll->GetIRagdoll();
	}
	else
	{
		return NULL;
	}
}

const QAngle& C_DODPlayer::GetRenderAngles()
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


void C_DODPlayer::UpdateClientSideAnimation()
{
	// Update the animation data. It does the local check here so this works when using
	// a third-person camera (and we don't have valid player angles).
	if ( this == C_DODPlayer::GetLocalDODPlayer() )
		m_PlayerAnimState->Update( EyeAngles()[YAW], m_angEyeAngles[PITCH] );
	else
		m_PlayerAnimState->Update( m_angEyeAngles[YAW], m_angEyeAngles[PITCH] );

	BaseClass::UpdateClientSideAnimation();
}

ConVar dod_playachievementsound( "dod_playachievementsound", "1", FCVAR_ARCHIVE );

void C_DODPlayer::OnAchievementAchieved( int iAchievement )
{
	// don't draw the head icon for a length of time after showing the particle effect
	m_flHideHeadIconUntilTime = gpGlobals->curtime + 2.5;

	if ( dod_playachievementsound.GetBool() )
	{
		EmitSound( "Achievement.Earned" );
	}

	BaseClass::OnAchievementAchieved( iAchievement );
}

int C_DODPlayer::DrawModel( int flags )
{
	int nRetval = BaseClass::DrawModel( flags );
	if ( nRetval != 0 )
	{
		// register to draw the head icon, unless we're hiding it due to the "achieved" particle effect
		if ( gpGlobals->curtime > m_flHideHeadIconUntilTime )
		{
			HeadIconManager()->PlayerDrawn( this );
		}
	}
	return nRetval;
}

void C_DODPlayer::DoAnimationEvent( PlayerAnimEvent_t event, int nData )
{
	m_PlayerAnimState->DoAnimationEvent( event, nData );
}

DODPlayerState C_DODPlayer::State_Get() const
{
	return m_iPlayerState;
}

bool C_DODPlayer::CanShowClassMenu( void )
{
	return ( GetTeamNumber() == TEAM_ALLIES || GetTeamNumber() == TEAM_AXIS );
}

void C_DODPlayer::DoRecoil( int iWpnID, float flWpnRecoil )
{
	float flPitchRecoil = flWpnRecoil;
	float flYawRecoil = flPitchRecoil / 4;

	if( iWpnID == WEAPON_BAR )
		flYawRecoil = MIN( flYawRecoil, 1.3 );

	if ( m_Shared.IsInMGDeploy() )
	{
		flPitchRecoil = 0.0;
		flYawRecoil = 0.0;
	}
	else if ( m_Shared.IsProne() && 
		iWpnID != WEAPON_30CAL && 
		iWpnID != WEAPON_MG42 ) //minor hackage
	{
		flPitchRecoil = flPitchRecoil / 4;
		flYawRecoil = flYawRecoil / 4;
	}
	else if ( m_Shared.IsDucking() )
	{
		flPitchRecoil = flPitchRecoil / 2;
		flYawRecoil = flYawRecoil / 2;
	}

	SetRecoilAmount( flPitchRecoil, flYawRecoil );
}

//Set the amount of pitch and yaw recoil we want to do over the next RECOIL_DURATION seconds
void C_DODPlayer::SetRecoilAmount( float flPitchRecoil, float flYawRecoil )
{
	//Slam the values, abandon previous recoils
	m_flPitchRecoilAccumulator = flPitchRecoil;

	flYawRecoil = flYawRecoil * random->RandomFloat( 0.8, 1.1 );

	if( random->RandomInt( 0,1 ) <= 0 )
		m_flYawRecoilAccumulator = flYawRecoil;
	else
		m_flYawRecoilAccumulator = -flYawRecoil;

	m_flRecoilTimeRemaining = RECOIL_DURATION;
}

//Get the amount of recoil we should do this frame
void C_DODPlayer::GetRecoilToAddThisFrame( float &flPitchRecoil, float &flYawRecoil )
{
	if( m_flRecoilTimeRemaining <= 0 )
	{
		flPitchRecoil = 0.0;
		flYawRecoil = 0.0;
		return;
	}

	float flRemaining = MIN( m_flRecoilTimeRemaining, gpGlobals->frametime );

	float flRecoilProportion = ( flRemaining / RECOIL_DURATION );

	flPitchRecoil = m_flPitchRecoilAccumulator * flRecoilProportion;
	flYawRecoil = m_flYawRecoilAccumulator * flRecoilProportion;

	m_flRecoilTimeRemaining -= gpGlobals->frametime;
}

//-----------------------------------------------------------------------------
// Purpose: Input handling
//-----------------------------------------------------------------------------
bool C_DODPlayer::CreateMove( float flInputSampleTime, CUserCmd *pCmd )
{
	// Lock view if deployed
	if( m_Shared.IsInMGDeploy() )
	{
		m_Shared.ClampDeployedAngles( &pCmd->viewangles );
	}

	//if we're prone and moving, do some sway
	if( m_Shared.IsProne() && IsAlive() )
	{
		float flSpeed = GetAbsVelocity().Length();

		float flSwayAmount = PRONE_SWAY_AMOUNT * gpGlobals->frametime;

		if( flSpeed > 10 )
		{
			if (m_flProneViewOffset >= PRONE_MAX_SWAY)
			{
				m_bProneSwayingRight = false;
			}
			else if (m_flProneViewOffset <= -PRONE_MAX_SWAY)
			{
				m_bProneSwayingRight = true;
			}

			if (m_bProneSwayingRight)
			{
				pCmd->viewangles[YAW]	+= flSwayAmount;
				m_flProneViewOffset		+= flSwayAmount;
			}
			else
			{
				pCmd->viewangles[YAW]	-= flSwayAmount;
				m_flProneViewOffset		-= flSwayAmount;
			}
		}
		else
		{
			// Return to 0 prone sway offset gradually

			//Quick Checks to make sure it isn't bigger or smaller than our sway amount
			if ( (m_flProneViewOffset < 0.0 && m_flProneViewOffset > -flSwayAmount) ||
				 (m_flProneViewOffset > 0.0 && m_flProneViewOffset < flSwayAmount) )
			{
				m_flProneViewOffset = 0.0;
			}

			if (m_flProneViewOffset > 0.0)
			{
				pCmd->viewangles[YAW]	-= flSwayAmount;
				m_flProneViewOffset		-= flSwayAmount;
			}
			else if (m_flProneViewOffset < 0.0)
			{
				pCmd->viewangles[YAW]	+= flSwayAmount;
				m_flProneViewOffset		+= flSwayAmount;
			}
		}
	}

	bool bResult = BaseClass::CreateMove( flInputSampleTime, pCmd );

	AvoidPlayers( pCmd );

	return bResult;
}

// How fast to avoid collisions with center of other object, in units per second
#define AVOID_SPEED 1000.0f
ConVar cl_avoidspeed( "cl_avoidspeed", "1000.0f", FCVAR_CLIENTDLL );
extern ConVar cl_forwardspeed;
extern ConVar cl_backspeed;
extern ConVar cl_sidespeed;

bool C_DODPlayer::ShouldDraw( void )
{
	if( IsDormant() )
		return false;

	// If we're dead, our ragdoll will be drawn for us instead.
	if ( !IsAlive() )
		return false;

	if( GetTeamNumber() == TEAM_SPECTATOR )
		return false;

	if( IsLocalPlayer() )
	{
		if ( IsRagdoll() )
			return true;
	}

	return BaseClass::ShouldDraw();
}

//-----------------------------------------------------------------------------
// Deal with visibility
//-----------------------------------------------------------------------------
void C_DODPlayer::GetToolRecordingState( KeyValues *msg )
{
#ifndef _XBOX
	BaseClass::GetToolRecordingState( msg );
	BaseEntityRecordingState_t *pBaseEntityState = (BaseEntityRecordingState_t*)msg->GetPtr( "baseentity" );
	if ( IsLocalPlayer() )
	{
		pBaseEntityState->m_bVisible = !IsDormant() && IsAlive() && ( GetTeamNumber() != TEAM_SPECTATOR ) &&
			( GetRenderMode() != kRenderNone ) && (GetObserverMode() != OBS_MODE_DEATHCAM) && !IsEffectActive(EF_NODRAW);
	}
#endif
}


CWeaponDODBase* C_DODPlayer::GetActiveDODWeapon() const
{
	C_BaseCombatWeapon *pWpn = GetActiveWeapon();

	if ( !pWpn )
		return NULL;

	return dynamic_cast< CWeaponDODBase* >( pWpn );
}

C_BaseAnimating * C_DODPlayer::BecomeRagdollOnClient()
{
	return NULL;
}

void C_DODPlayer::FireEvent( const Vector& origin, const QAngle& angles, int event, const char *options )
{
	if( event == 7002 )
	{
		if( this == C_BasePlayer::GetLocalPlayer() )
			return;

		CWeaponDODBase *pWeapon = GetActiveDODWeapon();

		if ( !pWeapon )
			return;

		int iAttachment = 2;
		Vector vecOrigin;
		QAngle angAngles;

		if( pWeapon->GetAttachment( iAttachment, vecOrigin, angAngles ) )
		{
			int shellType = atoi(options);

			CEffectData data;
			data.m_nHitBox = shellType;
			data.m_vOrigin = vecOrigin;
			data.m_vAngles = angAngles;
			DispatchEffect( "DOD_EjectBrass", data );
		}
	}
	else
		BaseClass::FireEvent( origin, angles, event, options );

	/*
	// MATTTODO: water footstep effects
	if( event == 7001 )
	{
		bool bInWater = ( enginetrace->GetPointContents(origin) & CONTENTS_WATER );

		if( bInWater )
		{
			//run splash
			CEffectData data;

			//trace up from foot position to the water surface
			trace_t tr;
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
			data.m_flScale = random->RandomFloat( 4.0f, 5.0f );
			DispatchEffect( "watersplash", data );
		}		
	}
	else if( event == 7002 )
	{
		bool bInWater = ( enginetrace->GetPointContents(origin) & CONTENTS_WATER );
		
		if( bInWater )
		{
			//walk ripple
			CEffectData data;

			//trace up from foot position to the water surface
			trace_t tr;
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
			data.m_flScale = random->RandomFloat( 4.0f, 7.0f );
			DispatchEffect( "waterripple", data );
		}
	}
	*/
}
// NVNT gate for spectating.
static bool inSpectating_Haptics = false;
// NVNT check grenade things ( -- this is here to avoid modificaions to the grenade class -- )
static bool s_holdingGrenade = false;
static bool s_grenadePinPulled = false;
static bool s_grenadeArmed = false;
static bool s_bombPlanting = false;
void C_DODPlayer::ClientThink()
{
	BaseClass::ClientThink();

	if ( gpGlobals->curtime >= m_fNextThinkPushAway )
	{
		PerformObstaclePushaway( this );
		m_fNextThinkPushAway =  gpGlobals->curtime + PUSHAWAY_THINK_INTERVAL;
	}

	if ( IsLocalPlayer() )
	{	
		UpdateIDTarget();

		StaminaSoundThink();

		// Recoil
		QAngle viewangles;
		engine->GetViewAngles( viewangles );

		float flYawRecoil;
		float flPitchRecoil;
		GetRecoilToAddThisFrame( flPitchRecoil, flYawRecoil );

		// Recoil
		if( flPitchRecoil > 0 )
		{
			//add the recoil pitch
			viewangles[PITCH] -= flPitchRecoil;
			viewangles[YAW] += flYawRecoil;
		}

		// Sniper sway
		if( m_Shared.IsSniperZoomed() && GetFOV() <= 20 )
		{
			//multiply by frametime to balance for framerate changes
			float x = gpGlobals->frametime * cos( gpGlobals->curtime );
			float y = gpGlobals->frametime * 2 * cos( 2 * gpGlobals->curtime );

			float scale;

			if( m_Shared.IsDucking() )				//duck
				scale = ZOOM_SWAY_DUCKING;
			else if( m_Shared.IsProne() )
				scale = ZOOM_SWAY_PRONE;
			else									//standing
				scale = ZOOM_SWAY_STANDING; 

			if( GetAbsVelocity().Length() > 10 )
				scale += ZOOM_SWAY_MOVING_PENALTY;

			viewangles[PITCH] += y * scale;
			viewangles[YAW] += x * scale;
		}

		engine->SetViewAngles( viewangles );
		// NVNT check spectator nav.
		if(	( ( GetTeamNumber() == TEAM_SPECTATOR ) || ( !this->IsAlive() ) ) ) {
				if(!inSpectating_Haptics)
				{
					if ( haptics )
						haptics->SetNavigationClass("spectate");
					inSpectating_Haptics = true;
				}
		}else{
			if(inSpectating_Haptics) {
				if ( haptics )
					haptics->SetNavigationClass("on_foot");
				inSpectating_Haptics = false;
			}
		}

		// NVNT check grenade things ( -- this is here to avoid modificaions to the grenade class -- )
		C_WeaponDODBaseGrenade *heldGrenade = dynamic_cast<C_WeaponDODBaseGrenade*>(GetActiveDODWeapon());
		if(heldGrenade)
		{
			if(!s_holdingGrenade)
			{
				s_holdingGrenade = true;
			}
			bool pinPulled = heldGrenade->m_bPinPulled;
			if(pinPulled != s_grenadePinPulled) {
				if(pinPulled)
				{
					if ( haptics )
						haptics->ProcessHapticEvent(3, "Weapons", heldGrenade->GetClassname(), "PinPulled");
				}else {
					if ( haptics )
						haptics->ProcessHapticEvent(3, "Weapons", heldGrenade->GetClassname(), "PinReplaced");
				}
				s_grenadePinPulled = pinPulled;
			}
			bool grenadeArmed = heldGrenade->m_bArmed;
			if(grenadeArmed != s_grenadeArmed) {
				if(grenadeArmed)
				{
					if ( haptics )
						haptics->ProcessHapticEvent(3, "Weapons", heldGrenade->GetClassname(), "Armed");
				}else {
					if ( haptics )
						haptics->ProcessHapticEvent(3, "Weapons", heldGrenade->GetClassname(), "Unarmed");
				}
				s_grenadeArmed = grenadeArmed;
			}
		}else{
			if( s_holdingGrenade ) 
			{
				if(s_grenadeArmed && s_grenadePinPulled) {
					if ( haptics )
						haptics->ProcessHapticEvent(3, "Weapons", "Grenades", "Thrown");
				}
				s_holdingGrenade = false;
				s_grenadeArmed = false;
				s_grenadePinPulled = false;
			}
			C_DODBaseBombWeapon *heldBomb = dynamic_cast<C_DODBaseBombWeapon*>(GetActiveDODWeapon());

			if(heldBomb) {
				bool isPlanting = heldBomb->IsPlanting();
				if(isPlanting!=s_bombPlanting) {
					if(isPlanting) {
						if(!s_bombPlanting) {
							s_bombPlanting = true;
							if ( haptics )
								haptics->ProcessHapticEvent(3, "Weapons", heldBomb->GetClassname(), "Plant");
						}
					}else{
						if(s_bombPlanting) {
							if ( haptics )
								haptics->ProcessHapticEvent(3, "Weapons", heldBomb->GetClassname(), "StopPlant");
						}
					}
				}
			}else if(s_bombPlanting) {
				s_bombPlanting = false;
				if ( haptics )
					haptics->ProcessHapticEvent(3, "Weapons", "Bomb", "Complete");
			}
		}
		
	}
	else
	{
		// Cold breath.
		UpdateColdBreath();
	}
}

// Start or stop the stamina breathing sound if necessary
void C_DODPlayer::StaminaSoundThink( void )
{
	if ( m_bPlayingLowStaminaSound )
	{
		if ( !IsAlive() || m_Shared.GetStamina() >= LOW_STAMINA_THRESHOLD )
		{
			// stop the sprint sound
			CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();
			controller.SoundFadeOut( m_pStaminaSound, 1.0, true );

			// SoundFadeOut will destroy this sound, so we will have to create another one
			// if we go below the threshold again soon
			m_pStaminaSound = NULL;

			m_bPlayingLowStaminaSound = false;
		}
	}
	else
	{
		if ( IsAlive() && m_Shared.GetStamina() < LOW_STAMINA_THRESHOLD )
		{
			// we are alive and have low stamina
			CLocalPlayerFilter filter;

			CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();

			if ( !m_pStaminaSound )
                m_pStaminaSound = controller.SoundCreate( filter, entindex(), "Player.Sprint" );

			controller.Play( m_pStaminaSound, 0.0, 100 );
			controller.SoundChangeVolume( m_pStaminaSound, 1.0, 2.0 );

			m_bPlayingLowStaminaSound = true;
		}
	}
}

void C_DODPlayer::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );

	if ( type == DATA_UPDATE_CREATED )
	{
		SetNextClientThink( CLIENT_THINK_ALWAYS );
	}

	UpdateVisibility();
}

void C_DODPlayer::PostDataUpdate( DataUpdateType_t updateType )
{
	// C_BaseEntity assumes we're networking the entity's angles, so pretend that it
	// networked the same value we already have.
	SetNetworkAngles( GetLocalAngles() );

	BaseClass::PostDataUpdate( updateType );

	if( m_bSpawnInterpCounter != m_bSpawnInterpCounterCache )
	{
		if ( IsLocalPlayer() )
		{
			LocalPlayerRespawn();
		}

		m_bSpawnInterpCounterCache = m_bSpawnInterpCounter.m_Value;
	}
}

// Called every time the player respawns
void C_DODPlayer::LocalPlayerRespawn( void )
{
	MoveToLastReceivedPosition( true );
	ResetLatched();

	ResetToneMapping(1.0);

	m_Shared.m_bForceProneChange = true;

	m_flLastRespawnTime = gpGlobals->curtime;
}

class C_FadingPhysPropClientside : public C_PhysPropClientside
{
public:	
	DECLARE_CLASS( C_FadingPhysPropClientside, C_PhysPropClientside );

	// if we wake, extend fade time

	virtual void ImpactTrace( trace_t *pTrace, int iDamageType, const char *pCustomImpactName )
	{
		// If we haven't started fading
		if( GetRenderColor().a >= 255 )
		{
			// delay the fade
			StartFadeOut( 10.0 );

			// register the impact
			BaseClass::ImpactTrace( pTrace, iDamageType, pCustomImpactName );
		}		
	}
};

void C_DODPlayer::PopHelmet( Vector vecDir, Vector vecForceOrigin, int iModel )
{
	if ( IsDormant() )
		return;	// We can't see them anyway, just bail

	C_FadingPhysPropClientside *pEntity = new C_FadingPhysPropClientside();

	if ( !pEntity )
		return;

	const model_t *model = modelinfo->GetModel( iModel );

	if ( !model )
	{
		DevMsg("CTempEnts::PhysicsProp: model index %i not found\n", iModel );
		return;
	}

	Vector vecHead;
	QAngle angHeadAngles;

	{
		C_BaseAnimating::AutoAllowBoneAccess boneaccess( true, false );
		int iAttachment = LookupAttachment( "head" );
		GetAttachment( iAttachment, vecHead, angHeadAngles );	//attachment 1 is the head attachment
	}

	pEntity->SetModelName( modelinfo->GetModelName(model) );
	pEntity->SetAbsOrigin( vecHead );
	pEntity->SetAbsAngles( angHeadAngles );
	pEntity->SetPhysicsMode( PHYSICS_MULTIPLAYER_CLIENTSIDE );

	if ( !pEntity->Initialize() )
	{
		pEntity->Release();
		return;
	}

	IPhysicsObject *pPhysicsObject = pEntity->VPhysicsGetObject();

	if( pPhysicsObject )
	{
#ifdef DEBUG
		if( vecForceOrigin == vec3_origin )
		{
			vecForceOrigin = GetAbsOrigin();
		}
#endif

		Vector vecForce = vecDir;		
		Vector vecOffset = vecForceOrigin - pEntity->GetAbsOrigin();
		pPhysicsObject->ApplyForceOffset( vecForce, vecOffset );
	}
	else
	{
		// failed to create a physics object
		pEntity->Release();
		return;
	}

	pEntity->StartFadeOut( 10.0 );
}

void C_DODPlayer::ReceiveMessage( int classID, bf_read &msg )
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
	case DOD_PLAYER_POP_HELMET:
		{
			Vector	vecDir, vecForceOffset;
			msg.ReadBitVec3Coord( vecDir );
			msg.ReadBitVec3Coord( vecForceOffset );

			int model = msg.ReadShort();

			PopHelmet( vecDir, vecForceOffset, model );
		}
		break;
	case DOD_PLAYER_REMOVE_DECALS:
		{
			RemoveAllDecals();
		}
		break;
	default:
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Update this client's target entity
//-----------------------------------------------------------------------------
void C_DODPlayer::UpdateIDTarget()
{
	Assert( IsLocalPlayer() );

	// Clear old target and find a new one
 	m_iIDEntIndex = 0;

	// don't show IDs in chase spec mode
	if ( GetObserverMode() == OBS_MODE_CHASE || 
		 GetObserverMode() == OBS_MODE_DEATHCAM )
		 return;

	trace_t tr;
	Vector vecStart, vecEnd;
	VectorMA( MainViewOrigin(), 1500, MainViewForward(), vecEnd );
	VectorMA( MainViewOrigin(), 10,   MainViewForward(), vecStart );
	UTIL_TraceLine( vecStart, vecEnd, MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );

	C_BaseEntity *pEntity = NULL;
	if ( !tr.startsolid && tr.DidHitNonWorldEntity() )
	{
		pEntity = tr.m_pEnt;

		if ( pEntity && (pEntity != this) )
		{
			m_iIDEntIndex = pEntity->entindex();
		}
	}

	// if we haven't done the weapon hint, and this entity is a weapon,
	// show the weapon hint
	if ( m_Hints.HasPlayedHint( HINT_PICK_UP_WEAPON ) == false && pEntity )
	{
		Vector vecDist = vecStart - tr.endpos;

		// if m_iIDEntIndex is a CWeaponDODBase, show pick up hint
		CWeaponDODBase *pWpn = dynamic_cast<CWeaponDODBase *>( pEntity );
		if ( pWpn && vecDist.Length() < 100 )
		{
			HintMessage( HINT_PICK_UP_WEAPON );
		}
	}
}

bool C_DODPlayer::ShouldAutoReload( void )
{
	return cl_autoreload.GetBool();
}

bool C_DODPlayer::ShouldAutoRezoom( void )
{
	return cl_autorezoom.GetBool();
}

void C_DODPlayer::CheckGrenadeHint( Vector vecGrenadeOrigin )
{
	if ( m_Hints.HasPlayedHint( HINT_PICK_UP_GRENADE ) == false )
	{
		// if its within 500 units
		float flDist = ( vecGrenadeOrigin - GetAbsOrigin() ).Length2D();

		if ( flDist < 500 )
		{
			m_Hints.HintMessage( HINT_PICK_UP_GRENADE );
		}
	}
}

void C_DODPlayer::CheckBombTargetPlantHint( void )
{
	if ( m_Hints.HasPlayedHint( HINT_BOMB_TARGET ) == false )
	{
		m_Hints.HintMessage( HINT_BOMB_TARGET );
	}
}

void C_DODPlayer::CheckBombTargetDefuseHint( void )
{
	if ( m_Hints.HasPlayedHint( HINT_DEFUSE_BOMB ) == false )
	{
		m_Hints.HintMessage( HINT_DEFUSE_BOMB );
	}
}

void C_DODPlayer::LowerWeapon( void )
{
	m_bWeaponLowered = true;
}

void C_DODPlayer::RaiseWeapon( void )
{
	m_bWeaponLowered = false;
}

bool C_DODPlayer::IsWeaponLowered( void )
{
	if ( GetMoveType() == MOVETYPE_LADDER )
		return true;

	CWeaponDODBase *pWeapon = GetActiveDODWeapon();

	if ( !pWeapon )
		return false;

	// Lower when underwater ( except if its melee )
	if ( GetWaterLevel() > 2 && pWeapon->GetDODWpnData().m_WeaponType != WPN_TYPE_MELEE )
		return true;

	if ( m_Shared.IsProne() && GetAbsVelocity().LengthSqr() > 1 )
		return true;

	if ( m_Shared.IsGoingProne() || m_Shared.IsGettingUpFromProne() )
		return true;

	if ( m_Shared.IsJumping() )
		return true;

	if ( m_Shared.IsDefusing() )
		return true;

	// Lower losing team's weapons in bonus round
	int state = DODGameRules()->State_Get();

	if ( state == STATE_ALLIES_WIN && GetTeamNumber() == TEAM_AXIS )
		return true;

	if ( state == STATE_AXIS_WIN && GetTeamNumber() == TEAM_ALLIES )
		return true;

	if ( m_Shared.IsBazookaDeployed() )
		return false;

	Vector vel = GetAbsVelocity();
	if ( vel.Length2D() < 50 )
		return false;

	if ( m_nButtons & IN_SPEED && ( m_nButtons & IN_FORWARD ) &&
		m_Shared.GetStamina() >= 5 &&
		!m_Shared.IsDucking() )
		return true;

	return m_bWeaponLowered;
}

// Shadows

ConVar cl_blobbyshadows( "cl_blobbyshadows", "0", FCVAR_CLIENTDLL );
ShadowType_t C_DODPlayer::ShadowCastType( void ) 
{
	if ( !IsVisible() )
		return SHADOWS_NONE;

	C_DODPlayer *pLocalPlayer = C_DODPlayer::GetLocalDODPlayer();

	// if we're first person spectating this player
	if ( pLocalPlayer && 
		pLocalPlayer->GetObserverTarget() == this &&
		pLocalPlayer->GetObserverMode() == OBS_MODE_IN_EYE )
	{
		return SHADOWS_NONE;		
	}

	if( cl_blobbyshadows.GetBool() )
		return SHADOWS_SIMPLE;

	return SHADOWS_RENDER_TO_TEXTURE_DYNAMIC;
}

float g_flFattenAmt = 4;
void C_DODPlayer::GetShadowRenderBounds( Vector &mins, Vector &maxs, ShadowType_t shadowType )
{
	if ( shadowType == SHADOWS_SIMPLE )
	{
		// Don't let the render bounds change when we're using blobby shadows, or else the shadow
		// will pop and stretch.
		mins = CollisionProp()->OBBMins();
		maxs = CollisionProp()->OBBMaxs();
	}
	else
	{
		GetRenderBounds( mins, maxs );

		// We do this because the normal bbox calculations don't take pose params into account, and 
		// the rotation of the guy's upper torso can place his gun a ways out of his bbox, and 
		// the shadow will get cut off as he rotates.
		//
		// Thus, we give it some padding here.
		mins -= Vector( g_flFattenAmt, g_flFattenAmt, 0 );
		maxs += Vector( g_flFattenAmt, g_flFattenAmt, 0 );
	}
}


void C_DODPlayer::GetRenderBounds( Vector& theMins, Vector& theMaxs )
{
	// TODO POSTSHIP - this hack/fix goes hand-in-hand with a fix in CalcSequenceBoundingBoxes in utils/studiomdl/simplify.cpp.
	// When we enable the fix in CalcSequenceBoundingBoxes, we can get rid of this.
	//
	// What we're doing right here is making sure it only uses the bbox for our lower-body sequences since,
	// with the current animations and the bug in CalcSequenceBoundingBoxes, are WAY bigger than they need to be.
	C_BaseAnimating::GetRenderBounds( theMins, theMaxs );
}


bool C_DODPlayer::GetShadowCastDirection( Vector *pDirection, ShadowType_t shadowType ) const
{ 
	if ( shadowType == SHADOWS_SIMPLE )
	{
		// Blobby shadows should sit directly underneath us.
		pDirection->Init( 0, 0, -1 );
		return true;
	}
	else
	{
		return BaseClass::GetShadowCastDirection( pDirection, shadowType );
	}
}

ConVar cl_muzzleflash_dlight_3rd( "cl_muzzleflash_dlight_3rd", "1" );

void C_DODPlayer::ProcessMuzzleFlashEvent()
{
	CBasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();

	bool bInToolRecordingMode = ToolsEnabled() && clienttools->IsInRecordingMode();

	// Reenable when the weapons have muzzle flash attachments in the right spot.
	if ( this == pLocalPlayer && !bInToolRecordingMode )
		return; // don't show own world muzzle flashs in for localplayer

	if ( pLocalPlayer && pLocalPlayer->GetObserverMode() == OBS_MODE_IN_EYE )
	{
		// also don't show in 1st person spec mode
		if ( pLocalPlayer->GetObserverTarget() == this )
			return;
	}

	CWeaponDODBase *pWeapon = GetActiveDODWeapon();
	if ( !pWeapon )
		return;

	int nModelIndex = pWeapon->GetModelIndex();
	int nWorldModelIndex = pWeapon->GetWorldModelIndex();
	if ( bInToolRecordingMode && nModelIndex != nWorldModelIndex )
	{
		pWeapon->SetModelIndex( nWorldModelIndex );
	}

	Vector vecOrigin;
	QAngle angAngles;

	//MATTTODO - use string names of the weapon
	const static int iMuzzleFlashAttachment = 1;
	const static int iEjectBrassAttachment = 2;

	// If we have an attachment, then stick a light on it.
	if ( cl_muzzleflash_dlight_3rd.GetBool() && pWeapon->GetAttachment( iMuzzleFlashAttachment, vecOrigin, angAngles ) )
	{
		// Muzzleflash light
		dlight_t *el = effects->CL_AllocDlight( LIGHT_INDEX_MUZZLEFLASH );
		el->origin = vecOrigin;
		el->radius = 70; 

		if ( pWeapon->GetDODWpnData().m_WeaponType == WPN_TYPE_SNIPER )
			el->radius = 150;

		el->decay = el->radius / 0.05f;
		el->die = gpGlobals->curtime + 0.05f;
		el->color.r = 255;
		el->color.g = 192;
		el->color.b = 64;
		el->color.exponent = 5;

		if ( bInToolRecordingMode )
		{
			Color clr( el->color.r, el->color.g, el->color.b );

			KeyValues *msg = new KeyValues( "TempEntity" );

			msg->SetInt( "te", TE_DYNAMIC_LIGHT );
			msg->SetString( "name", "TE_DynamicLight" );
			msg->SetFloat( "time", gpGlobals->curtime );
			msg->SetFloat( "duration", el->die );
			msg->SetFloat( "originx", el->origin.x );
			msg->SetFloat( "originy", el->origin.y );
			msg->SetFloat( "originz", el->origin.z );
			msg->SetFloat( "radius", el->radius );
			msg->SetFloat( "decay", el->decay );
			msg->SetColor( "color", clr );
			msg->SetInt( "exponent", el->color.exponent );
			msg->SetInt( "lightindex", LIGHT_INDEX_MUZZLEFLASH );

			ToolFramework_PostToolMessage( HTOOLHANDLE_INVALID, msg );
			msg->deleteThis();
		}
	}

	const char *pszMuzzleFlashEffect = NULL;

	switch( pWeapon->GetDODWpnData().m_iMuzzleFlashType )
	{
	case DOD_MUZZLEFLASH_PISTOL:
		pszMuzzleFlashEffect = "muzzle_pistols";
		break;
	case DOD_MUZZLEFLASH_AUTO:
		pszMuzzleFlashEffect = "muzzle_fullyautomatic";
		break;
	case DOD_MUZZLEFLASH_RIFLE:
		pszMuzzleFlashEffect = "muzzle_rifles";
		break;
	case DOD_MUZZLEFLASH_ROCKET:
		pszMuzzleFlashEffect = "muzzle_rockets";
		break;
	case DOD_MUZZLEFLASH_MG42:
		pszMuzzleFlashEffect = "muzzle_mg42";
		break;
	default:
		break;
	}

	if ( pszMuzzleFlashEffect )
	{
		DispatchParticleEffect( pszMuzzleFlashEffect, PATTACH_POINT_FOLLOW, pWeapon, 1 );
	}

	if( pWeapon->ShouldAutoEjectBrass() )
	{
		// shell eject
		if( pWeapon->GetAttachment( iEjectBrassAttachment, vecOrigin, angAngles ) )
		{
			int shellType = pWeapon->GetEjectBrassShellType();

			CEffectData data;
			data.m_nHitBox = shellType;
			data.m_vOrigin = vecOrigin;
			data.m_vAngles = angAngles;
			DispatchEffect( "DOD_EjectBrass", data );
		}
	}

	if ( bInToolRecordingMode && nModelIndex != nWorldModelIndex )
	{
		pWeapon->SetModelIndex( nModelIndex );
	}
}

void C_DODPlayer::NotifyShouldTransmit( ShouldTransmitState_t state )
{
	// Remove all effects if we go out of the PVS.
	if ( state == SHOULDTRANSMIT_END )
	{
		if( m_pFlashlightBeam != NULL )
		{
			ReleaseFlashlight();
		}
	}

	BaseClass::NotifyShouldTransmit( state );
}

void C_DODPlayer::Simulate( void )
{
	if( this != C_BasePlayer::GetLocalPlayer() )
	{
		if ( IsEffectActive( EF_DIMLIGHT ) )
		{
			QAngle eyeAngles = m_angEyeAngles;
			Vector vForward;
			AngleVectors( eyeAngles, &vForward );

			int iAttachment = LookupAttachment( "anim_attachment_RH" );

			Vector vecOrigin;
			QAngle dummy;
			GetAttachment( iAttachment, vecOrigin, dummy );

			trace_t tr;
			UTIL_TraceLine( vecOrigin, vecOrigin + (vForward * 200), MASK_SHOT, this, COLLISION_GROUP_NONE, &tr );

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
				beamInfo.m_flEndWidth = 35.0f;
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

				dlight_t *el = effects->CL_AllocDlight( 0 );
				el->origin = tr.endpos;
				el->radius = 50; 
				el->color.r = 200;
				el->color.g = 200;
				el->color.b = 200;
				el->die = gpGlobals->curtime + 0.1;
			}
		}
		else if ( m_pFlashlightBeam )
		{
			ReleaseFlashlight();
		}
	}

	BaseClass::Simulate();
}

void C_DODPlayer::ReleaseFlashlight( void )
{
	if( m_pFlashlightBeam )
	{
		m_pFlashlightBeam->flags = 0;
		m_pFlashlightBeam->die = gpGlobals->curtime - 1;

		m_pFlashlightBeam = NULL;
	}
}

bool C_DODPlayer::SetFOV( CBaseEntity *pRequester, int FOV, float zoomRate /* = 0.0f */ )
{
	/*
	if( FOV < 30 )
	{
		// fade in
		ScreenFade_t sf;
		memset( &sf, 0, sizeof( sf ) );
		sf.a = 255;
		sf.r = 0;
		sf.g = 0;
		sf.b = 0;
		sf.duration = (unsigned short)((float)(1<<SCREENFADE_FRACBITS) * 2.5f );
		sf.fadeFlags = FFADE_IN;
		vieweffects->Fade( sf );
	}
	else
	{
		//cancel the fade if its active
		ScreenFade_t sf;
		memset( &sf, 0, sizeof( sf ) );
		sf.fadeFlags = FFADE_IN | FFADE_PURGE;
		vieweffects->Fade( sf );
	}
	*/

	return true;
}

void C_DODPlayer::CalcObserverView( Vector& eyeOrigin, QAngle& eyeAngles, float& fov )
{
	if( GetObserverMode() == OBS_MODE_DEATHCAM )
	{
		CalcDODDeathCamView( eyeOrigin, eyeAngles, fov );
	}
	else
		BaseClass::CalcObserverView( eyeOrigin, eyeAngles, fov );
}

static Vector WALL_MIN(-WALL_OFFSET,-WALL_OFFSET,-WALL_OFFSET);
static Vector WALL_MAX(WALL_OFFSET,WALL_OFFSET,WALL_OFFSET);

void C_DODPlayer::CalcDODDeathCamView(Vector& eyeOrigin, QAngle& eyeAngles, float& fov)
{
	CBaseEntity	* killer = GetObserverTarget();

	//float interpolation = ( gpGlobals->curtime - m_flDeathTime ) / DEATH_ANIMATION_TIME;

	// Interpolate very quickly to the killer and follow
	float interpolation = ( gpGlobals->curtime - m_flDeathTime ) / 0.2f;
	interpolation = clamp( interpolation, 0.0f, 1.0f );

	m_flObserverChaseDistance += gpGlobals->frametime*48.0f;
	m_flObserverChaseDistance = clamp( m_flObserverChaseDistance, CHASE_CAM_DISTANCE_MIN, CHASE_CAM_DISTANCE_MAX );

	QAngle aForward = eyeAngles = EyeAngles();
	Vector origin = EyePosition();			

	IRagdoll *pRagdoll = GetRepresentativeRagdoll();
	if ( pRagdoll )
	{
		origin = pRagdoll->GetRagdollOrigin();
		origin.z += VEC_DEAD_VIEWHEIGHT_SCALED( this ).z; // look over ragdoll, not through
	}

	if ( killer && (killer != this) ) 
	{
		Vector vecKiller = killer->GetAbsOrigin();
		
		C_DODPlayer *player = ToDODPlayer( killer );
		if ( player && player->IsAlive() )
		{
			if ( player->m_Shared.IsProne() )
			{
				VectorAdd( vecKiller, VEC_PRONE_VIEW_SCALED( this ), vecKiller );
			}
			else if( player->GetFlags() & FL_DUCKING )
			{
				VectorAdd( vecKiller, VEC_DUCK_VIEW_SCALED( this ), vecKiller );
			}
			else
			{
				VectorAdd( vecKiller, VEC_VIEW_SCALED( this ), vecKiller );
			}
		}

		Vector vecToKiller = vecKiller - origin;
		QAngle aKiller;
		VectorAngles( vecToKiller, aKiller );
		InterpolateAngles( aForward, aKiller, eyeAngles, interpolation );
	}

	Vector vForward; AngleVectors( eyeAngles, &vForward );

	VectorNormalize( vForward );

	VectorMA( origin, -m_flObserverChaseDistance, vForward, eyeOrigin );

	trace_t trace; // clip against world
	C_BaseEntity::PushEnableAbsRecomputations( false ); // HACK don't recompute positions while doing RayTrace
	UTIL_TraceHull( origin, eyeOrigin, WALL_MIN, WALL_MAX, MASK_SOLID, this, COLLISION_GROUP_NONE, &trace );
	C_BaseEntity::PopEnableAbsRecomputations();

	if (trace.fraction < 1.0)
	{
		eyeOrigin = trace.endpos;
		m_flObserverChaseDistance = VectorLength(origin - eyeOrigin);
	}

	fov = GetFOV();
}

void C_DODPlayer::CalcChaseCamView(Vector& eyeOrigin, QAngle& eyeAngles, float& fov)
{
	C_BaseEntity *target = GetObserverTarget();

	if ( !target ) 
	{
		// just copy a save in-map position
		VectorCopy( EyePosition(), eyeOrigin );
		VectorCopy( EyeAngles(), eyeAngles );
		return;
	};

	Vector forward, viewpoint;

	// GetRenderOrigin() returns ragdoll pos if player is ragdolled
	Vector origin = target->GetRenderOrigin();

	C_DODPlayer *player = ToDODPlayer( target );

	if ( player && player->IsAlive() )
	{
		if ( player->m_Shared.IsProne() )
		{
			VectorAdd( origin, VEC_PRONE_VIEW_SCALED( this ), origin );
		}
		else if( player->GetFlags() & FL_DUCKING )
		{
			VectorAdd( origin, VEC_DUCK_VIEW_SCALED( this ), origin );
		}
		else
		{
			VectorAdd( origin, VEC_VIEW_SCALED( this ), origin );
		}
	}
	else
	{
		// assume it's the players ragdoll
		VectorAdd( origin, VEC_DEAD_VIEWHEIGHT_SCALED( this ), origin );
	}

	QAngle viewangles;

	if ( IsLocalPlayer() )
	{
		engine->GetViewAngles( viewangles );
	}
	else
	{
		viewangles = EyeAngles();
	}

	m_flObserverChaseDistance += gpGlobals->frametime*48.0f;
	m_flObserverChaseDistance = clamp( m_flObserverChaseDistance, CHASE_CAM_DISTANCE_MIN, CHASE_CAM_DISTANCE_MAX );

	AngleVectors( viewangles, &forward );

	VectorNormalize( forward );

	VectorMA(origin, -m_flObserverChaseDistance, forward, viewpoint );

	trace_t trace;
	C_BaseEntity::PushEnableAbsRecomputations( false ); // HACK don't recompute positions while doing RayTrace
	UTIL_TraceHull( origin, viewpoint, WALL_MIN, WALL_MAX, MASK_SOLID, target, COLLISION_GROUP_NONE, &trace );
	C_BaseEntity::PopEnableAbsRecomputations();

	if (trace.fraction < 1.0)
	{
		viewpoint = trace.endpos;
		m_flObserverChaseDistance = VectorLength(origin - eyeOrigin);
	}

	VectorCopy( viewangles, eyeAngles );
	VectorCopy( viewpoint, eyeOrigin );

	fov = GetFOV();
}

extern ConVar spec_freeze_traveltime;
extern ConVar spec_freeze_time;
extern ConVar cl_dod_freezecam;

//-----------------------------------------------------------------------------
// Purpose: Calculate the view for the player while he's in freeze frame observer mode
//-----------------------------------------------------------------------------
void C_DODPlayer::CalcFreezeCamView( Vector& eyeOrigin, QAngle& eyeAngles, float& fov )
{
	C_BaseEntity *pTarget = GetObserverTarget();
	if ( !pTarget || !cl_dod_freezecam.GetBool() )
	{
		CalcDeathCamView( eyeOrigin, eyeAngles, fov );
		return;
	}

	// Zoom towards our target
	float flCurTime = (gpGlobals->curtime - m_flFreezeFrameStartTime);
	float flBlendPerc = clamp( flCurTime / spec_freeze_traveltime.GetFloat(), 0, 1 );
	flBlendPerc = SimpleSpline( flBlendPerc );

	// Find the position we would like to be lookin at
	Vector vecCamDesired = pTarget->GetObserverCamOrigin();	// Returns ragdoll origin if they're ragdolled
	VectorAdd( vecCamDesired, GetChaseCamViewOffset( pTarget ), vecCamDesired );
	Vector vecCamTarget = vecCamDesired;
	if ( !pTarget->IsAlive() )
	{
		vecCamTarget.z += pTarget->GetBaseAnimating() ? VEC_DEAD_VIEWHEIGHT_SCALED( pTarget->GetBaseAnimating() ).z : VEC_DEAD_VIEWHEIGHT.z;	// look over ragdoll, not through
	}

	// Figure out a view position in front of the target
	Vector vecEyeOnPlane = eyeOrigin;
	vecEyeOnPlane.z = vecCamTarget.z;
	Vector vecTargetPos = vecCamTarget;
	Vector vecToTarget = vecTargetPos - vecEyeOnPlane;
	VectorNormalize( vecToTarget );

	// Stop a few units away from the target, and shift up to be at the same height
	vecTargetPos = vecCamTarget - (vecToTarget * m_flFreezeFrameDistance);
	float flEyePosZ = pTarget->EyePosition().z;
	vecTargetPos.z = flEyePosZ + m_flFreezeZOffset;

	// Now trace out from the target, so that we're put in front of any walls
	trace_t trace;
	C_BaseEntity::PushEnableAbsRecomputations( false ); // HACK don't recompute positions while doing RayTrace
	UTIL_TraceLine( vecCamTarget, vecTargetPos, MASK_SOLID, pTarget, COLLISION_GROUP_NONE, &trace );
	C_BaseEntity::PopEnableAbsRecomputations();
	if (trace.fraction < 1.0 )
	{
		// The camera's going to be really close to the target. So we don't end up
		// looking at someone's chest, aim close freezecams at the target's eyes.
		vecTargetPos = trace.endpos;
		vecCamTarget = vecCamDesired;

		// To stop all close in views looking up at character's chins, move the view up.
		vecTargetPos.z += fabs(vecCamTarget.z - vecTargetPos.z) * 0.85;
		C_BaseEntity::PushEnableAbsRecomputations( false ); // HACK don't recompute positions while doing RayTrace
		UTIL_TraceLine( vecCamTarget, vecTargetPos, MASK_SOLID, pTarget, COLLISION_GROUP_NONE, &trace );
		C_BaseEntity::PopEnableAbsRecomputations();
		vecTargetPos = trace.endpos;
	}

	// Look directly at the target
	vecToTarget = vecCamTarget - vecTargetPos;
	VectorNormalize( vecToTarget );
	VectorAngles( vecToTarget, eyeAngles );

	VectorLerp( m_vecFreezeFrameStart, vecTargetPos, flBlendPerc, eyeOrigin );

	if ( flCurTime >= spec_freeze_traveltime.GetFloat() && !m_bSentFreezeFrame )
	{
		IGameEvent *pEvent = gameeventmanager->CreateEvent( "freezecam_started" );
		if ( pEvent )
		{
			gameeventmanager->FireEventClientSide( pEvent );
		}

		m_bSentFreezeFrame = true;
		view->FreezeFrame( spec_freeze_time.GetFloat() );
	}
}

const Vector& C_DODPlayer::GetRenderOrigin( void )
{
	if ( !IsAlive() && m_hRagdoll.Get() )
		return m_hRagdoll.Get()->GetRenderOrigin();

	return BaseClass::GetRenderOrigin();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Vector C_DODPlayer::GetChaseCamViewOffset( CBaseEntity *target )
{
	C_DODPlayer *pPlayer = ToDODPlayer( target );

	if ( pPlayer && pPlayer->IsAlive() )
	{
		if ( pPlayer->m_Shared.IsProne() )
		{
			return VEC_PRONE_VIEW;
		}
	}

	return BaseClass::GetChaseCamViewOffset( target );
}

const QAngle& C_DODPlayer::EyeAngles()
{
	if ( IsLocalPlayer() && g_nKillCamMode == OBS_MODE_NONE )
	{
		return BaseClass::EyeAngles();
	}
	else
	{
		return m_angEyeAngles;
	}
}

// Cold breath defines.
#define COLDBREATH_EMIT_MIN				2.0f
#define COLDBREATH_EMIT_MAX				3.0f
#define COLDBREATH_EMIT_SCALE			0.35f
#define COLDBREATH_PARTICLE_LIFE_MIN	0.25f
#define COLDBREATH_PARTICLE_LIFE_MAX	1.0f
#define COLDBREATH_PARTICLE_LIFE_SCALE  0.75
#define COLDBREATH_PARTICLE_SIZE_MIN	1.0f
#define COLDBREATH_PARTICLE_SIZE_MAX	4.0f
#define COLDBREATH_PARTICLE_SIZE_SCALE	1.1f
#define COLDBREATH_PARTICLE_COUNT		1
#define COLDBREATH_DURATION_MIN			0.0f
#define COLDBREATH_DURATION_MAX			1.0f
#define COLDBREATH_ALPHA_MIN			0.0f
#define COLDBREATH_ALPHA_MAX			0.3f
#define COLDBREATH_ENDSCALE_MIN			0.1f
#define COLDBREATH_ENDSCALE_MAX			0.4f

static ConVar cl_coldbreath_forcestamina( "cl_coldbreath_forcestamina", "0", FCVAR_CHEAT );
static ConVar cl_coldbreath_enable( "cl_coldbreath_enable", "1" );

//-----------------------------------------------------------------------------
// Purpose: Create the emitter of cold breath particles
//-----------------------------------------------------------------------------
bool C_DODPlayer::CreateColdBreathEmitter( void )
{
	// Check to see if we are in a cold breath scenario.
	if ( !GetClientWorldEntity()->m_bColdWorld )
		return false;

	// Set cold breath to true.
	m_bColdBreathOn = true;

	// Create a cold breath emitter if one doesn't already exist.
	if ( !m_hColdBreathEmitter )
	{
		m_hColdBreathEmitter = ColdBreathEmitter::Create( "ColdBreath" );
		if ( !m_hColdBreathEmitter )
			return false;

		// Get the particle material.
		m_hColdBreathMaterial = m_hColdBreathEmitter->GetPMaterial( "sprites/frostbreath" );
		Assert( m_hColdBreathMaterial != INVALID_MATERIAL_HANDLE );

		// Cache off the head attachment for setting up cold breath.
		m_iHeadAttach = LookupAttachment( "head" );		
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Destroy the cold breath emitter
//-----------------------------------------------------------------------------
void C_DODPlayer::DestroyColdBreathEmitter( void )
{
#if 0
	if ( m_hColdBreathEmitter.IsValid() )
	{
		UTIL_Remove( m_hColdBreathEmitter );
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void C_DODPlayer::UpdateColdBreath( void )
{
	if ( !cl_coldbreath_enable.GetBool() )
		return;

	// Check to see if the cold breath emitter has been created.
	if ( !m_hColdBreathEmitter.IsValid() )
	{
		if ( !CreateColdBreathEmitter() )
			return;
	}

	// Cold breath updates.
	if ( !m_bColdBreathOn )
		return;

	// Don't emit breath if we are dead.
	if ( !IsAlive() || IsDormant() )
		return;

	// Check player speed, do emit cold breath when moving quickly.
	float flSpeed = GetAbsVelocity().Length();
	if ( flSpeed > 60.0f )
		return;

	if ( m_flColdBreathTimeStart < gpGlobals->curtime )
	{
		// Spawn cold breath particles.
		EmitColdBreathParticles();

		// Update the timer.
		if ( m_flColdBreathTimeEnd < gpGlobals->curtime )
		{
			// Check stamina and modify the time accordingly.
			if ( m_Shared.m_flStamina < LOW_STAMINA_THRESHOLD || cl_coldbreath_forcestamina.GetBool() )
			{
				m_flColdBreathTimeStart = gpGlobals->curtime + RandomFloat( COLDBREATH_EMIT_MIN * COLDBREATH_EMIT_SCALE, COLDBREATH_EMIT_MAX * COLDBREATH_EMIT_SCALE );
				float flDuration = RandomFloat( COLDBREATH_DURATION_MIN, COLDBREATH_DURATION_MAX );
				m_flColdBreathTimeEnd = m_flColdBreathTimeStart + flDuration;
			}
			else
			{
				m_flColdBreathTimeStart = gpGlobals->curtime + RandomFloat( COLDBREATH_EMIT_MIN, COLDBREATH_EMIT_MAX );
				float flDuration = RandomFloat( COLDBREATH_DURATION_MIN, COLDBREATH_DURATION_MAX );
				m_flColdBreathTimeEnd = m_flColdBreathTimeStart + flDuration;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_DODPlayer::CalculateIKLocks( float currentTime )
{
	if (!m_pIk) 
		return;

	int targetCount = m_pIk->m_target.Count();
	if ( targetCount == 0 )
		return;

	// In TF, we might be attaching a player's view to a walking model that's using IK. If we are, it can
	// get in here during the view setup code, and it's not normally supposed to be able to access the spatial
	// partition that early in the rendering loop. So we allow access right here for that special case.
	SpatialPartitionListMask_t curSuppressed = partition->GetSuppressedLists();
	partition->SuppressLists( PARTITION_ALL_CLIENT_EDICTS, false );
	CBaseEntity::PushEnableAbsRecomputations( false );

	for (int i = 0; i < targetCount; i++)
	{
		trace_t trace;
		CIKTarget *pTarget = &m_pIk->m_target[i];

		if (!pTarget->IsActive())
			continue;

		switch( pTarget->type)
		{
		case IK_GROUND:
			{
				pTarget->SetPos( Vector( pTarget->est.pos.x, pTarget->est.pos.y, GetRenderOrigin().z ));
				pTarget->SetAngles( GetRenderAngles() );
			}
			break;

		case IK_ATTACHMENT:
			{
				C_BaseEntity *pEntity = NULL;
				float flDist = pTarget->est.radius;

				// FIXME: make entity finding sticky!
				// FIXME: what should the radius check be?
				for ( CEntitySphereQuery sphere( pTarget->est.pos, 64 ); ( pEntity = sphere.GetCurrentEntity() ) != NULL; sphere.NextEntity() )
				{
					C_BaseAnimating *pAnim = pEntity->GetBaseAnimating( );
					if (!pAnim)
						continue;

					int iAttachment = pAnim->LookupAttachment( pTarget->offset.pAttachmentName );
					if (iAttachment <= 0)
						continue;

					Vector origin;
					QAngle angles;
					pAnim->GetAttachment( iAttachment, origin, angles );

					// debugoverlay->AddBoxOverlay( origin, Vector( -1, -1, -1 ), Vector( 1, 1, 1 ), QAngle( 0, 0, 0 ), 255, 0, 0, 0, 0 );

					float d = (pTarget->est.pos - origin).Length();

					if ( d >= flDist)
						continue;

					flDist = d;
					pTarget->SetPos( origin );
					pTarget->SetAngles( angles );
					// debugoverlay->AddBoxOverlay( pTarget->est.pos, Vector( -pTarget->est.radius, -pTarget->est.radius, -pTarget->est.radius ), Vector( pTarget->est.radius, pTarget->est.radius, pTarget->est.radius), QAngle( 0, 0, 0 ), 0, 255, 0, 0, 0 );
				}

				if (flDist >= pTarget->est.radius)
				{
					// debugoverlay->AddBoxOverlay( pTarget->est.pos, Vector( -pTarget->est.radius, -pTarget->est.radius, -pTarget->est.radius ), Vector( pTarget->est.radius, pTarget->est.radius, pTarget->est.radius), QAngle( 0, 0, 0 ), 0, 0, 255, 0, 0 );
					// no solution, disable ik rule
					pTarget->IKFailed( );
				}
			}
			break;
		}
	}

	CBaseEntity::PopEnableAbsRecomputations();
	partition->SuppressLists( curSuppressed, true );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void C_DODPlayer::EmitColdBreathParticles( void )
{
	// Get the position to emit from - look into caching this off we are doing redundant work in the case
	// of allies (see dod_headiconmanager.cpp).
	Vector vecOrigin; 
	QAngle vecAngle;
	GetAttachment( m_iHeadAttach, vecOrigin, vecAngle );
	Vector vecForward, vecRight, vecUp;
	AngleVectors( vecAngle, &vecUp, &vecForward, &vecRight );

	vecOrigin += ( vecForward * 8.0f );

	SimpleParticle *pParticle = static_cast<SimpleParticle*>( m_hColdBreathEmitter->AddParticle( sizeof( SimpleParticle ),m_hColdBreathMaterial, vecOrigin ) );
	if ( pParticle )
	{
		pParticle->m_flLifetime	= 0.0f;
		pParticle->m_flDieTime = RandomFloat( COLDBREATH_PARTICLE_LIFE_MIN, COLDBREATH_PARTICLE_LIFE_MAX );
		if ( m_Shared.m_flStamina < LOW_STAMINA_THRESHOLD || cl_coldbreath_forcestamina.GetBool() )
		{
			pParticle->m_flDieTime *= COLDBREATH_PARTICLE_LIFE_SCALE;
		}

		// Add just a little movement.
		if ( m_Shared.m_flStamina < LOW_STAMINA_THRESHOLD || cl_coldbreath_forcestamina.GetBool() )
		{
			pParticle->m_vecVelocity = ( vecForward * RandomFloat( 10.0f, 30.0f ) ) + ( vecRight * RandomFloat( -2.0f, 2.0f ) ) +
				                       ( vecUp * RandomFloat( 0.0f, 0.5f ) );
		}
		else
		{
			pParticle->m_vecVelocity = ( vecForward * RandomFloat( 10.0f, 20.0f ) ) + ( vecRight * RandomFloat( -2.0f, 2.0f ) ) +
				                       ( vecUp * RandomFloat( 0.0f, 1.5f ) );
		}
				
		pParticle->m_uchColor[0] = 200;
		pParticle->m_uchColor[1] = 200;
		pParticle->m_uchColor[2] = 210;
				
		float flParticleSize = RandomFloat( COLDBREATH_PARTICLE_SIZE_MIN, COLDBREATH_PARTICLE_SIZE_MAX );
		float flParticleScale = RandomFloat( COLDBREATH_ENDSCALE_MIN, COLDBREATH_ENDSCALE_MAX );
		if ( m_Shared.m_flStamina < LOW_STAMINA_THRESHOLD || cl_coldbreath_forcestamina.GetBool() )
		{
			pParticle->m_uchEndSize = flParticleSize * COLDBREATH_PARTICLE_SIZE_SCALE;
			flParticleScale *= COLDBREATH_PARTICLE_SIZE_SCALE;
		}
		else
		{
			pParticle->m_uchEndSize = flParticleSize;
		}
		pParticle->m_uchStartSize = ( flParticleSize * flParticleScale );
				
		float flAlpha = RandomFloat( COLDBREATH_ALPHA_MIN, COLDBREATH_ALPHA_MAX );
		pParticle->m_uchStartAlpha = flAlpha * 255; 
		pParticle->m_uchEndAlpha = 0;
				
		pParticle->m_flRoll	= RandomInt( 0, 360 );
		pParticle->m_flRollDelta = RandomFloat( 0.0f, 1.25f );
	}
}

void C_DODPlayer::ComputeWorldSpaceSurroundingBox( Vector *pVecWorldMins, Vector *pVecWorldMaxs )
{
	m_Shared.ComputeWorldSpaceSurroundingBox( pVecWorldMins, pVecWorldMaxs );
}

//-----------------------------------------------------------------------------
// Purpose: Try to steer away from any players and objects we might interpenetrate
//-----------------------------------------------------------------------------
#define DOD_AVOID_MAX_RADIUS_SQR		5184.0f			// Based on player extents and max buildable extents.
#define DOD_OO_AVOID_MAX_RADIUS_SQR		0.00019f

#define DOD_MAX_SEPARATION_FORCE		256

extern ConVar cl_forwardspeed;
extern ConVar cl_backspeed;
extern ConVar cl_sidespeed;

void C_DODPlayer::AvoidPlayers( CUserCmd *pCmd )
{
	// Don't test if the player is dead.
	if ( IsAlive() == false )
		return;

	C_Team *pTeam = ( C_Team * )GetTeam();
	if ( !pTeam )
		return;

	// Up vector.
	static Vector vecUp( 0.0f, 0.0f, 1.0f );

	Vector vecDODPlayerCenter = GetAbsOrigin();
	Vector vecDODPlayerMin = GetPlayerMins();
	Vector vecDODPlayerMax = GetPlayerMaxs();
	float flZHeight = vecDODPlayerMax.z - vecDODPlayerMin.z;
	vecDODPlayerCenter.z += 0.5f * flZHeight;
	VectorAdd( vecDODPlayerMin, vecDODPlayerCenter, vecDODPlayerMin );
	VectorAdd( vecDODPlayerMax, vecDODPlayerCenter, vecDODPlayerMax );

	// Find an intersecting player or object.
	int nAvoidPlayerCount = 0;
	C_DODPlayer *pAvoidPlayerList[MAX_PLAYERS];

	C_DODPlayer *pIntersectPlayer = NULL;
	float flAvoidRadius = 0.0f;

	Vector vecAvoidCenter, vecAvoidMin, vecAvoidMax;
	for ( int i = 0; i < pTeam->GetNumPlayers(); ++i )
	{
		C_DODPlayer *pAvoidPlayer = static_cast< C_DODPlayer * >( pTeam->GetPlayer( i ) );
		if ( pAvoidPlayer == NULL )
			continue;
		// Is the avoid player me?
		if ( pAvoidPlayer == this )
			continue;

		// Save as list to check against for objects.
		pAvoidPlayerList[nAvoidPlayerCount] = pAvoidPlayer;
		++nAvoidPlayerCount;

		// Check to see if the avoid player is dormant.
		if ( pAvoidPlayer->IsDormant() )
			continue;

		// Is the avoid player solid?
		if ( pAvoidPlayer->IsSolidFlagSet( FSOLID_NOT_SOLID ) )
			continue;

		Vector t1, t2;

		vecAvoidCenter = pAvoidPlayer->GetAbsOrigin();
		vecAvoidMin = pAvoidPlayer->GetPlayerMins();
		vecAvoidMax = pAvoidPlayer->GetPlayerMaxs();
		flZHeight = vecAvoidMax.z - vecAvoidMin.z;
		vecAvoidCenter.z += 0.5f * flZHeight;
		VectorAdd( vecAvoidMin, vecAvoidCenter, vecAvoidMin );
		VectorAdd( vecAvoidMax, vecAvoidCenter, vecAvoidMax );

		if ( IsBoxIntersectingBox( vecDODPlayerMin, vecDODPlayerMax, vecAvoidMin, vecAvoidMax ) )
		{
			// Need to avoid this player.
			if ( !pIntersectPlayer )
			{
				pIntersectPlayer = pAvoidPlayer;
				break;
			}
		}
	}

	// Anything to avoid?
	if ( !pIntersectPlayer )
	{
		return;
	}

	// Calculate the push strength and direction.
	Vector vecDelta;

	// Avoid a player - they have precedence.
	if ( pIntersectPlayer )
	{
		VectorSubtract( pIntersectPlayer->WorldSpaceCenter(), vecDODPlayerCenter, vecDelta );

		Vector vRad = pIntersectPlayer->WorldAlignMaxs() - pIntersectPlayer->WorldAlignMins();
		vRad.z = 0;

		flAvoidRadius = vRad.Length();
	}

	float flPushStrength = RemapValClamped( vecDelta.Length(), flAvoidRadius, 0, 0, DOD_MAX_SEPARATION_FORCE ); //flPushScale;

	//Msg( "PushScale = %f\n", flPushStrength );

	// Check to see if we have enough push strength to make a difference.
	if ( flPushStrength < 0.01f )
		return;

	Vector vecPush;
	if ( GetAbsVelocity().Length2DSqr() > 0.1f )
	{
		Vector vecVelocity = GetAbsVelocity();
		vecVelocity.z = 0.0f;
		CrossProduct( vecUp, vecVelocity, vecPush );
		VectorNormalize( vecPush );
	}
	else
	{
		// We are not moving, but we're still intersecting.
		QAngle angView = pCmd->viewangles;
		angView.x = 0.0f;
		AngleVectors( angView, NULL, &vecPush, NULL );
	}

	// Move away from the other player/object.
	Vector vecSeparationVelocity;
	if ( vecDelta.Dot( vecPush ) < 0 )
	{
		vecSeparationVelocity = vecPush * flPushStrength;
	}
	else
	{
		vecSeparationVelocity = vecPush * -flPushStrength;
	}

	// Don't allow the max push speed to be greater than the max player speed.
	float flMaxPlayerSpeed = MaxSpeed();
	float flCropFraction = 1.33333333f;

	if ( ( GetFlags() & FL_DUCKING ) && ( GetGroundEntity() != NULL ) )
	{	
		flMaxPlayerSpeed *= flCropFraction;
	}	

	float flMaxPlayerSpeedSqr = flMaxPlayerSpeed * flMaxPlayerSpeed;

	if ( vecSeparationVelocity.LengthSqr() > flMaxPlayerSpeedSqr )
	{
		vecSeparationVelocity.NormalizeInPlace();
		VectorScale( vecSeparationVelocity, flMaxPlayerSpeed, vecSeparationVelocity );
	}

	QAngle vAngles = pCmd->viewangles;
	vAngles.x = 0;
	Vector currentdir;
	Vector rightdir;

	AngleVectors( vAngles, &currentdir, &rightdir, NULL );

	Vector vDirection = vecSeparationVelocity;

	VectorNormalize( vDirection );

	float fwd = currentdir.Dot( vDirection );
	float rt = rightdir.Dot( vDirection );

	float forward = fwd * flPushStrength;
	float side = rt * flPushStrength;

	//Msg( "fwd: %f - rt: %f - forward: %f - side: %f\n", fwd, rt, forward, side );

	pCmd->forwardmove	+= forward;
	pCmd->sidemove		+= side;

	// Clamp the move to within legal limits, preserving direction. This is a little
	// complicated because we have different limits for forward, back, and side

	//Msg( "PRECLAMP: forwardmove=%f, sidemove=%f\n", pCmd->forwardmove, pCmd->sidemove );

	float flForwardScale = 1.0f;
	if ( pCmd->forwardmove > fabs( cl_forwardspeed.GetFloat() ) )
	{
		flForwardScale = fabs( cl_forwardspeed.GetFloat() ) / pCmd->forwardmove;
	}
	else if ( pCmd->forwardmove < -fabs( cl_backspeed.GetFloat() ) )
	{
		flForwardScale = fabs( cl_backspeed.GetFloat() ) / fabs( pCmd->forwardmove );
	}

	float flSideScale = 1.0f;
	if ( fabs( pCmd->sidemove ) > fabs( cl_sidespeed.GetFloat() ) )
	{
		flSideScale = fabs( cl_sidespeed.GetFloat() ) / fabs( pCmd->sidemove );
	}

	float flScale = MIN( flForwardScale, flSideScale );
	pCmd->forwardmove *= flScale;
	pCmd->sidemove *= flScale;

	//Msg( "Pforwardmove=%f, sidemove=%f\n", pCmd->forwardmove, pCmd->sidemove );
}


//-----------------------------------------------------------------------------
// Purpose: Returns whether this player is the nemesis of the local player
//-----------------------------------------------------------------------------
bool C_DODPlayer::IsNemesisOfLocalPlayer()
{
	C_DODPlayer *pLocalPlayer = C_DODPlayer::GetLocalDODPlayer();
	if ( pLocalPlayer )
	{
		// return whether this player is dominating the local player
		return m_Shared.IsPlayerDominated( pLocalPlayer->entindex() );
	}		
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Returns whether we should show the nemesis icon for this player
//-----------------------------------------------------------------------------
bool C_DODPlayer::ShouldShowNemesisIcon()
{
	/*
	// we should show the nemesis effect on this player if he is the nemesis of the local player,
	// and is not dead, cloaked or disguised
	if ( IsNemesisOfLocalPlayer() && g_DODPR && g_PR->IsConnected( entindex() ) )
	{
		if ( IsAlive() )
			return true;
	}
	*/
	return false;
}

int C_DODPlayer::GetActiveAchievementAward( void )
{
	int iAward = ACHIEVEMENT_AWARDS_NONE;

	int iClassBit = m_Shared.PlayerClass() + 1;

	if ( m_iAchievementAwardsMask & (1<<ACHIEVEMENT_AWARDS_ALL_PACK_1) )
	{
		iAward = ACHIEVEMENT_AWARDS_ALL_PACK_1;
	}
	else if ( m_iAchievementAwardsMask & ( 1<<iClassBit ) )
	{
		iAward = iClassBit;
	}

	return iAward;
}

IMaterial *C_DODPlayer::GetHeadIconMaterial( void )
{
	const char *pszMaterial = "";

	int iAchievementAward = GetActiveAchievementAward();

	if ( iAchievementAward >= 0 && iAchievementAward < NUM_ACHIEVEMENT_AWARDS )
	{
		switch ( GetTeamNumber() )
		{
		case TEAM_ALLIES:
			pszMaterial = g_pszAchievementAwardMaterials_Allies[iAchievementAward];
			break;
		case TEAM_AXIS:
			pszMaterial = g_pszAchievementAwardMaterials_Axis[iAchievementAward];
			break;
		default:
			break;
		}
	}

	IMaterial *pMaterial = NULL;
	if ( pszMaterial )
	{
		pMaterial = materials->FindMaterial( pszMaterial, TEXTURE_GROUP_VGUI );
	}	

	// clear the old one if its different
	if ( m_pHeadIconMaterial != pMaterial )
	{
		if ( m_pHeadIconMaterial )
		{
			m_pHeadIconMaterial->DecrementReferenceCount();
		}

		m_pHeadIconMaterial = pMaterial;
		m_pHeadIconMaterial->IncrementReferenceCount();
	}

	return m_pHeadIconMaterial;
}
