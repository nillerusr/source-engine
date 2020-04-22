//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:		Player for .
//
//===========================================================================//

#include "cbase.h"
#include "vcollide_parse.h"
#include "c_portal_player.h"
#include "view.h"
#include "c_basetempentity.h"
#include "takedamageinfo.h"
#include "in_buttons.h"
#include "iviewrender_beams.h"
#include "r_efx.h"
#include "dlight.h"
#include "PortalRender.h"
#include "toolframework/itoolframework.h"
#include "toolframework_client.h"
#include "tier1/KeyValues.h"
#include "ScreenSpaceEffects.h"
#include "portal_shareddefs.h"
#include "ivieweffects.h"		// for screenshake
#include "prop_portal_shared.h"

// NVNT for fov updates
#include "haptics/ihaptics.h"


// Don't alias here
#if defined( CPortal_Player )
#undef CPortal_Player	
#endif


#define REORIENTATION_RATE 120.0f
#define REORIENTATION_ACCELERATION_RATE 400.0f

#define ENABLE_PORTAL_EYE_INTERPOLATION_CODE


#define DEATH_CC_LOOKUP_FILENAME "materials/correction/cc_death.raw"
#define DEATH_CC_FADE_SPEED 0.05f


ConVar cl_reorient_in_air("cl_reorient_in_air", "1", FCVAR_ARCHIVE, "Allows the player to only reorient from being upside down while in the air." ); 


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
		C_Portal_Player *pPlayer = dynamic_cast< C_Portal_Player* >( m_hPlayer.Get() );
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


//=================================================================================
//
// Ragdoll Entity
//
class C_PortalRagdoll : public C_BaseFlex
{
public:

	DECLARE_CLASS( C_PortalRagdoll, C_BaseFlex );
	DECLARE_CLIENTCLASS();

	C_PortalRagdoll();
	~C_PortalRagdoll();

	virtual void OnDataChanged( DataUpdateType_t type );

	int GetPlayerEntIndex() const;
	IRagdoll* GetIRagdoll() const;

	virtual void SetupWeights( const matrix3x4_t *pBoneToWorld, int nFlexWeightCount, float *pFlexWeights, float *pFlexDelayedWeights );

private:

	C_PortalRagdoll( const C_PortalRagdoll & ) {}

	void Interp_Copy( C_BaseAnimatingOverlay *pSourceEntity );
	void CreatePortalRagdoll();

private:

	EHANDLE	m_hPlayer;
	CNetworkVector( m_vecRagdollVelocity );
	CNetworkVector( m_vecRagdollOrigin );

};

IMPLEMENT_CLIENTCLASS_DT_NOBASE( C_PortalRagdoll, DT_PortalRagdoll, CPortalRagdoll )
RecvPropVector( RECVINFO(m_vecRagdollOrigin) ),
RecvPropEHandle( RECVINFO( m_hPlayer ) ),
RecvPropInt( RECVINFO( m_nModelIndex ) ),
RecvPropInt( RECVINFO(m_nForceBone) ),
RecvPropVector( RECVINFO(m_vecForce) ),
RecvPropVector( RECVINFO( m_vecRagdollVelocity ) ),
END_RECV_TABLE()


//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
C_PortalRagdoll::C_PortalRagdoll()
{
	m_hPlayer = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
C_PortalRagdoll::~C_PortalRagdoll()
{
	( this );
}



//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pSourceEntity - 
//-----------------------------------------------------------------------------
void C_PortalRagdoll::Interp_Copy( C_BaseAnimatingOverlay *pSourceEntity )
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
			if ( !Q_strcmp( pSrcEntry->watcher->GetDebugName(), pDestEntry->watcher->GetDebugName() ) )
			{
				pDestEntry->watcher->Copy( pSrcEntry->watcher );
				break;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Setup vertex weights for drawing
//-----------------------------------------------------------------------------
void C_PortalRagdoll::SetupWeights( const matrix3x4_t *pBoneToWorld, int nFlexWeightCount, float *pFlexWeights, float *pFlexDelayedWeights )
{
	// While we're dying, we want to mimic the facial animation of the player. Once they're dead, we just stay as we are.
	if ( (m_hPlayer && m_hPlayer->IsAlive()) || !m_hPlayer )
	{
		BaseClass::SetupWeights( pBoneToWorld, nFlexWeightCount, pFlexWeights, pFlexDelayedWeights );
	}
	else if ( m_hPlayer )
	{
		m_hPlayer->SetupWeights( pBoneToWorld, nFlexWeightCount, pFlexWeights, pFlexDelayedWeights );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void C_PortalRagdoll::CreatePortalRagdoll()
{
	// First, initialize all our data. If we have the player's entity on our client,
	// then we can make ourselves start out exactly where the player is.
	C_Portal_Player *pPlayer = dynamic_cast<C_Portal_Player*>( m_hPlayer.Get() );

	if ( pPlayer && !pPlayer->IsDormant() )
	{
		// Move my current model instance to the ragdoll's so decals are preserved.
		pPlayer->SnatchModelInstance( this );

		VarMapping_t *varMap = GetVarMapping();

		// This is the local player, so set them in a default
		// pose and slam their velocity, angles and origin
		SetAbsOrigin( /* m_vecRagdollOrigin : */ pPlayer->GetRenderOrigin() );			
		SetAbsAngles( pPlayer->GetRenderAngles() );
		SetAbsVelocity( m_vecRagdollVelocity );

		// Hack! Find a neutral standing pose or use the idle.
		int iSeq = LookupSequence( "ragdoll" );
		if ( iSeq == -1 )
		{
			Assert( false );
			iSeq = 0;
		}			
		SetSequence( iSeq );
		SetCycle( 0.0 );

		Interp_Reset( varMap );

		m_nBody = pPlayer->GetBody();
		SetModelIndex( m_nModelIndex );	
		// Make us a ragdoll..
		m_nRenderFX = kRenderFxRagdoll;

		matrix3x4_t boneDelta0[MAXSTUDIOBONES];
		matrix3x4_t boneDelta1[MAXSTUDIOBONES];
		matrix3x4_t currentBones[MAXSTUDIOBONES];
		const float boneDt = 0.05f;

		pPlayer->GetRagdollInitBoneArrays( boneDelta0, boneDelta1, currentBones, boneDt );

		InitAsClientRagdoll( boneDelta0, boneDelta1, currentBones, boneDt );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
// Output : IRagdoll*
//-----------------------------------------------------------------------------
IRagdoll* C_PortalRagdoll::GetIRagdoll() const
{
	return m_pRagdoll;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : type - 
//-----------------------------------------------------------------------------
void C_PortalRagdoll::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );

	if ( type == DATA_UPDATE_CREATED )
	{
		CreatePortalRagdoll();
	}
}


LINK_ENTITY_TO_CLASS( player, C_Portal_Player );

IMPLEMENT_CLIENTCLASS_DT(C_Portal_Player, DT_Portal_Player, CPortal_Player)
RecvPropFloat( RECVINFO( m_angEyeAngles[0] ) ),
RecvPropFloat( RECVINFO( m_angEyeAngles[1] ) ),
RecvPropEHandle( RECVINFO( m_hRagdoll ) ),
RecvPropInt( RECVINFO( m_iSpawnInterpCounter ) ),
RecvPropInt( RECVINFO( m_iPlayerSoundType ) ),
RecvPropBool( RECVINFO( m_bHeldObjectOnOppositeSideOfPortal ) ),
RecvPropEHandle( RECVINFO( m_pHeldObjectPortal ) ),
RecvPropBool( RECVINFO( m_bPitchReorientation ) ),
RecvPropEHandle( RECVINFO( m_hPortalEnvironment ) ),
RecvPropEHandle( RECVINFO( m_hSurroundingLiquidPortal ) ),
RecvPropBool( RECVINFO( m_bSuppressingCrosshair ) ),
END_RECV_TABLE()


BEGIN_PREDICTION_DATA( C_Portal_Player )
END_PREDICTION_DATA()

#define	_WALK_SPEED 150
#define	_NORM_SPEED 190
#define	_SPRINT_SPEED 320

static ConVar cl_playermodel( "cl_playermodel", "none", FCVAR_USERINFO | FCVAR_ARCHIVE | FCVAR_SERVER_CAN_EXECUTE, "Default Player Model");

extern bool g_bUpsideDown;

//EHANDLE g_eKillTarget1;
//EHANDLE g_eKillTarget2;

void SpawnBlood (Vector vecSpot, const Vector &vecDir, int bloodColor, float flDamage);

C_Portal_Player::C_Portal_Player()
: m_iv_angEyeAngles( "C_Portal_Player::m_iv_angEyeAngles" )
{
	m_PlayerAnimState = CreatePortalPlayerAnimState( this );

	m_iIDEntIndex = 0;
	m_iSpawnInterpCounterCache = 0;
	m_flDeathCCWeight = 0.0f;

	m_hRagdoll.Set( NULL );
	m_flStartLookTime = 0.0f;

	m_bHeldObjectOnOppositeSideOfPortal = false;
	m_pHeldObjectPortal = 0;

	m_bPitchReorientation = false;
	m_fReorientationRate = 0.0f;

	m_angEyeAngles.Init();

	AddVar( &m_angEyeAngles, &m_iv_angEyeAngles, LATCH_SIMULATION_VAR );

	m_EntClientFlags |= ENTCLIENTFLAG_DONTUSEIK;
	m_blinkTimer.Invalidate();

	m_CCDeathHandle = INVALID_CLIENT_CCHANDLE;
}

C_Portal_Player::~C_Portal_Player( void )
{
	if ( m_PlayerAnimState )
	{
		m_PlayerAnimState->Release();
	}

	g_pColorCorrectionMgr->RemoveColorCorrection( m_CCDeathHandle );
}

int C_Portal_Player::GetIDTarget() const
{
	return m_iIDEntIndex;
}

//-----------------------------------------------------------------------------
// Purpose: Update this client's target entity
//-----------------------------------------------------------------------------
void C_Portal_Player::UpdateIDTarget()
{
	if ( !IsLocalPlayer() )
		return;

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

	if ( !tr.startsolid && tr.DidHitNonWorldEntity() )
	{
		C_BaseEntity *pEntity = tr.m_pEnt;

		if ( pEntity && (pEntity != this) )
		{
			m_iIDEntIndex = pEntity->entindex();
		}
	}
}

void C_Portal_Player::TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr )
{
	Vector vecOrigin = ptr->endpos - vecDir * 4;

	float flDistance = 0.0f;

	if ( info.GetAttacker() )
	{
		flDistance = (ptr->endpos - info.GetAttacker()->GetAbsOrigin()).Length();
	}

	if ( m_takedamage )
	{
		AddMultiDamage( info, this );

		int blood = BloodColor();

		if ( blood != DONT_BLEED )
		{
			SpawnBlood( vecOrigin, vecDir, blood, flDistance );// a little surface blood.
			TraceBleed( flDistance, vecDir, ptr, info.GetDamageType() );
		}
	}
}

void C_Portal_Player::Initialize( void )
{
	m_headYawPoseParam = LookupPoseParameter(  "head_yaw" );
	GetPoseParameterRange( m_headYawPoseParam, m_headYawMin, m_headYawMax );

	m_headPitchPoseParam = LookupPoseParameter( "head_pitch" );
	GetPoseParameterRange( m_headPitchPoseParam, m_headPitchMin, m_headPitchMax );

	CStudioHdr *hdr = GetModelPtr();
	for ( int i = 0; i < hdr->GetNumPoseParameters() ; i++ )
	{
		SetPoseParameter( hdr, i, 0.0 );
	}
}

CStudioHdr *C_Portal_Player::OnNewModel( void )
{
	CStudioHdr *hdr = BaseClass::OnNewModel();

	Initialize( );

	return hdr;
}

//-----------------------------------------------------------------------------
/**
* Orient head and eyes towards m_lookAt.
*/
void C_Portal_Player::UpdateLookAt( void )
{
	// head yaw
	if (m_headYawPoseParam < 0 || m_headPitchPoseParam < 0)
		return;

	// This is buggy with dt 0, just skip since there is no work to do.
	if ( gpGlobals->frametime <= 0.0f )
		return;

	// Player looks at themselves through portals. Pick the portal we're turned towards.
	const int iPortalCount = CProp_Portal_Shared::AllPortals.Count();
	CProp_Portal **pPortals = CProp_Portal_Shared::AllPortals.Base();
	float *fPortalDot = (float *)stackalloc( sizeof( float ) * iPortalCount );
	float flLowDot = 1.0f;
	int iUsePortal = -1;

	// defaults if no portals are around
	Vector vPlayerForward;
	GetVectors( &vPlayerForward, NULL, NULL );
	Vector vCurLookTarget = EyePosition();

	if ( !IsAlive() )
	{
		m_viewtarget = EyePosition() + vPlayerForward*10.0f;
		return;
	}

	bool bNewTarget = false;
	if ( UTIL_IntersectEntityExtentsWithPortal( this ) != NULL )
	{
		// player is in a portal
		vCurLookTarget = EyePosition() + vPlayerForward*10.0f;
	}
	else if ( pPortals && pPortals[0] )
	{
		// Test through any active portals: This may be a shorter distance to the target
		for( int i = 0; i != iPortalCount; ++i )
		{
			CProp_Portal *pTempPortal = pPortals[i];

			if( pTempPortal && pTempPortal->m_bActivated && pTempPortal->m_hLinkedPortal.Get() )
			{
				Vector vEyeForward, vPortalForward;
				EyeVectors( &vEyeForward );
				pTempPortal->GetVectors( &vPortalForward, NULL, NULL );
				fPortalDot[i] = vEyeForward.Dot( vPortalForward );
				if ( fPortalDot[i] < flLowDot )
				{
					flLowDot = fPortalDot[i];
					iUsePortal = i;
				}
			}
		}

		if ( iUsePortal >= 0 )
		{
			C_Prop_Portal* pPortal = pPortals[iUsePortal];
			if ( pPortal )
			{
				vCurLookTarget = pPortal->MatrixThisToLinked()*vCurLookTarget;
				if ( vCurLookTarget != m_vLookAtTarget )
				{
					bNewTarget = true;
				}
			}
		}
	}
	else
	{
		// No other look targets, look straight ahead
		vCurLookTarget += vPlayerForward*10.0f;
	}

	// Figure out where we want to look in world space.
	QAngle desiredAngles;
	Vector to = vCurLookTarget - EyePosition();
	VectorAngles( to, desiredAngles );
	QAngle aheadAngles;
	VectorAngles( vCurLookTarget, aheadAngles );

	// Figure out where our body is facing in world space.
	QAngle bodyAngles( 0, 0, 0 );
	bodyAngles[YAW] = GetLocalAngles()[YAW];

	m_flLastBodyYaw = bodyAngles[YAW];

	// Set the head's yaw.
	float desiredYaw = AngleNormalize( desiredAngles[YAW] - bodyAngles[YAW] );
	desiredYaw = clamp( desiredYaw, m_headYawMin, m_headYawMax );

	float desiredPitch = AngleNormalize( desiredAngles[PITCH] );
	desiredPitch = clamp( desiredPitch, m_headPitchMin, m_headPitchMax );

	if ( bNewTarget )
	{
		m_flStartLookTime = gpGlobals->curtime;
	}

	float dt = (gpGlobals->frametime);
	float flSpeed	= 1.0f - ExponentialDecay( 0.7f, 0.033f, dt );

	m_flCurrentHeadYaw = m_flCurrentHeadYaw + flSpeed * ( desiredYaw - m_flCurrentHeadYaw );
	m_flCurrentHeadYaw	= AngleNormalize( m_flCurrentHeadYaw );
	SetPoseParameter( m_headYawPoseParam, m_flCurrentHeadYaw );	

	m_flCurrentHeadPitch = m_flCurrentHeadPitch + flSpeed * ( desiredPitch - m_flCurrentHeadPitch );
	m_flCurrentHeadPitch = AngleNormalize( m_flCurrentHeadPitch );
	SetPoseParameter( m_headPitchPoseParam, m_flCurrentHeadPitch );

	// This orients the eyes
	m_viewtarget = m_vLookAtTarget = vCurLookTarget;
}

void C_Portal_Player::ClientThink( void )
{
	//PortalEyeInterpolation.m_bNeedToUpdateEyePosition = true;

	Vector vForward;
	AngleVectors( GetLocalAngles(), &vForward );

	// Allow sprinting
	HandleSpeedChanges();

	FixTeleportationRoll();

	//QAngle vAbsAngles = EyeAngles();

	// Look at the thing that killed you
	//if ( !IsAlive() )
	//{
	//	C_BaseEntity *pEntity1 = g_eKillTarget1.Get();
	//	C_BaseEntity *pEntity2 = g_eKillTarget2.Get();

	//	if ( pEntity2 && pEntity1 )
	//	{
	//		//engine->GetViewAngles( vAbsAngles );

	//		Vector vLook = pEntity1->GetAbsOrigin() - pEntity2->GetAbsOrigin();
	//		VectorNormalize( vLook );

	//		QAngle qLook;
	//		VectorAngles( vLook, qLook );

	//		if ( qLook[PITCH] > 180.0f )
	//		{
	//			qLook[PITCH] -= 360.0f;
	//		}

	//		if ( vAbsAngles[YAW] < 0.0f )
	//		{
	//			vAbsAngles[YAW] += 360.0f;
	//		}

	//		if ( vAbsAngles[PITCH] < qLook[PITCH] )
	//		{
	//			vAbsAngles[PITCH] += gpGlobals->frametime * 120.0f;
	//			if ( vAbsAngles[PITCH] > qLook[PITCH] )
	//				vAbsAngles[PITCH] = qLook[PITCH];
	//		}
	//		else if ( vAbsAngles[PITCH] > qLook[PITCH] )
	//		{
	//			vAbsAngles[PITCH] -= gpGlobals->frametime * 120.0f;
	//			if ( vAbsAngles[PITCH] < qLook[PITCH] )
	//				vAbsAngles[PITCH] = qLook[PITCH];
	//		}

	//		if ( vAbsAngles[YAW] < qLook[YAW] )
	//		{
	//			vAbsAngles[YAW] += gpGlobals->frametime * 240.0f;
	//			if ( vAbsAngles[YAW] > qLook[YAW] )
	//				vAbsAngles[YAW] = qLook[YAW];
	//		}
	//		else if ( vAbsAngles[YAW] > qLook[YAW] )
	//		{
	//			vAbsAngles[YAW] -= gpGlobals->frametime * 240.0f;
	//			if ( vAbsAngles[YAW] < qLook[YAW] )
	//				vAbsAngles[YAW] = qLook[YAW];
	//		}

	//		if ( vAbsAngles[YAW] > 180.0f )
	//		{
	//			vAbsAngles[YAW] -= 360.0f;
	//		}

	//		engine->SetViewAngles( vAbsAngles );
	//	}
	//}

	// If dead, fade in death CC lookup
	if ( m_CCDeathHandle != INVALID_CLIENT_CCHANDLE )
	{
		if ( m_lifeState != LIFE_ALIVE )
		{
			if ( m_flDeathCCWeight < 1.0f )
			{
				m_flDeathCCWeight += DEATH_CC_FADE_SPEED;
				clamp( m_flDeathCCWeight, 0.0f, 1.0f );
			}
		}
		else 
		{
			m_flDeathCCWeight = 0.0f;
		}
		g_pColorCorrectionMgr->SetColorCorrectionWeight( m_CCDeathHandle, m_flDeathCCWeight );
	}

	UpdateIDTarget();
}

void C_Portal_Player::FixTeleportationRoll( void )
{
	if( IsInAVehicle() ) //HL2 compatibility fix. do absolutely nothing to the view in vehicles
		return;

	if( !IsLocalPlayer() )
		return;

	// Normalize roll from odd portal transitions
	QAngle vAbsAngles = EyeAngles();


	Vector vCurrentForward, vCurrentRight, vCurrentUp;
	AngleVectors( vAbsAngles, &vCurrentForward, &vCurrentRight, &vCurrentUp );

	if ( vAbsAngles[ROLL] == 0.0f )
	{
		m_fReorientationRate = 0.0f;
		g_bUpsideDown = ( vCurrentUp.z < 0.0f );
		return;
	}

	bool bForcePitchReorient = ( vAbsAngles[ROLL] > 175.0f && vCurrentForward.z > 0.99f );
	bool bOnGround = ( GetGroundEntity() != NULL );

	if ( bForcePitchReorient )
	{
		m_fReorientationRate = REORIENTATION_RATE * ( ( bOnGround ) ? ( 2.0f ) : ( 1.0f ) );
	}
	else
	{
		// Don't reorient in air if they don't want to
		if ( !cl_reorient_in_air.GetBool() && !bOnGround )
		{
			g_bUpsideDown = ( vCurrentUp.z < 0.0f );
			return;
		}
	}

	if ( vCurrentUp.z < 0.75f )
	{
		m_fReorientationRate += gpGlobals->frametime * REORIENTATION_ACCELERATION_RATE;

		// Upright faster if on the ground
		float fMaxReorientationRate = REORIENTATION_RATE * ( ( bOnGround ) ? ( 2.0f ) : ( 1.0f ) );
		if ( m_fReorientationRate > fMaxReorientationRate )
			m_fReorientationRate = fMaxReorientationRate;
	}
	else
	{
		if ( m_fReorientationRate > REORIENTATION_RATE * 0.5f )
		{
			m_fReorientationRate -= gpGlobals->frametime * REORIENTATION_ACCELERATION_RATE;
			if ( m_fReorientationRate < REORIENTATION_RATE * 0.5f )
				m_fReorientationRate = REORIENTATION_RATE * 0.5f;
		}
		else if ( m_fReorientationRate < REORIENTATION_RATE * 0.5f )
		{
			m_fReorientationRate += gpGlobals->frametime * REORIENTATION_ACCELERATION_RATE;
			if ( m_fReorientationRate > REORIENTATION_RATE * 0.5f )
				m_fReorientationRate = REORIENTATION_RATE * 0.5f;
		}
	}

	if ( !m_bPitchReorientation && !bForcePitchReorient )
	{
		// Randomize which way we roll if we're completely upside down
		if ( vAbsAngles[ROLL] == 180.0f && RandomInt( 0, 1 ) == 1 )
		{
			vAbsAngles[ROLL] = -180.0f;
		}

		if ( vAbsAngles[ROLL] < 0.0f )
		{
			vAbsAngles[ROLL] += gpGlobals->frametime * m_fReorientationRate;
			if ( vAbsAngles[ROLL] > 0.0f )
				vAbsAngles[ROLL] = 0.0f;
			engine->SetViewAngles( vAbsAngles );
		}
		else if ( vAbsAngles[ROLL] > 0.0f )
		{
			vAbsAngles[ROLL] -= gpGlobals->frametime * m_fReorientationRate;
			if ( vAbsAngles[ROLL] < 0.0f )
				vAbsAngles[ROLL] = 0.0f;
			engine->SetViewAngles( vAbsAngles );
			m_angEyeAngles = vAbsAngles;
			m_iv_angEyeAngles.Reset();
		}
	}
	else
	{
		if ( vAbsAngles[ROLL] != 0.0f )
		{
			if ( vCurrentUp.z < 0.2f )
			{
				float fDegrees = gpGlobals->frametime * m_fReorientationRate;
				if ( vCurrentForward.z > 0.0f )
				{
					fDegrees = -fDegrees;
				}

				// Rotate around the right axis
				VMatrix mAxisAngleRot = SetupMatrixAxisRot( vCurrentRight, fDegrees );

				vCurrentUp = mAxisAngleRot.VMul3x3( vCurrentUp );
				vCurrentForward = mAxisAngleRot.VMul3x3( vCurrentForward );

				VectorAngles( vCurrentForward, vCurrentUp, vAbsAngles );

				engine->SetViewAngles( vAbsAngles );
				m_angEyeAngles = vAbsAngles;
				m_iv_angEyeAngles.Reset();
			}
			else
			{
				if ( vAbsAngles[ROLL] < 0.0f )
				{
					vAbsAngles[ROLL] += gpGlobals->frametime * m_fReorientationRate;
					if ( vAbsAngles[ROLL] > 0.0f )
						vAbsAngles[ROLL] = 0.0f;
					engine->SetViewAngles( vAbsAngles );
					m_angEyeAngles = vAbsAngles;
					m_iv_angEyeAngles.Reset();
				}
				else if ( vAbsAngles[ROLL] > 0.0f )
				{
					vAbsAngles[ROLL] -= gpGlobals->frametime * m_fReorientationRate;
					if ( vAbsAngles[ROLL] < 0.0f )
						vAbsAngles[ROLL] = 0.0f;
					engine->SetViewAngles( vAbsAngles );
					m_angEyeAngles = vAbsAngles;
					m_iv_angEyeAngles.Reset();
				}
			}
		}
	}

	// Keep track of if we're upside down for look control
	vAbsAngles = EyeAngles();
	AngleVectors( vAbsAngles, NULL, NULL, &vCurrentUp );

	if ( bForcePitchReorient )
		g_bUpsideDown = ( vCurrentUp.z < 0.0f );
	else
		g_bUpsideDown = false;
}

const QAngle& C_Portal_Player::GetRenderAngles()
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

void C_Portal_Player::UpdateClientSideAnimation( void )
{
	UpdateLookAt();

	// Update the animation data. It does the local check here so this works when using
	// a third-person camera (and we don't have valid player angles).
	if ( this == C_Portal_Player::GetLocalPortalPlayer() )
		m_PlayerAnimState->Update( EyeAngles()[YAW], m_angEyeAngles[PITCH] );
	else
		m_PlayerAnimState->Update( m_angEyeAngles[YAW], m_angEyeAngles[PITCH] );

	BaseClass::UpdateClientSideAnimation();
}

void C_Portal_Player::DoAnimationEvent( PlayerAnimEvent_t event, int nData )
{
	m_PlayerAnimState->DoAnimationEvent( event, nData );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int C_Portal_Player::DrawModel( int flags )
{
	if ( !m_bReadyToDraw )
		return 0;

	if( IsLocalPlayer() )
	{
		if ( !C_BasePlayer::ShouldDrawThisPlayer() )
		{
			if ( !g_pPortalRender->IsRenderingPortal() )
				return 0;

			if( (g_pPortalRender->GetViewRecursionLevel() == 1) && (m_iForceNoDrawInPortalSurface != -1) ) //CPortalRender::s_iRenderingPortalView )
				return 0;
		}
	}

	return BaseClass::DrawModel(flags);
}



//-----------------------------------------------------------------------------
// Should this object receive shadows?
//-----------------------------------------------------------------------------
bool C_Portal_Player::ShouldReceiveProjectedTextures( int flags )
{
	Assert( flags & SHADOW_FLAGS_PROJECTED_TEXTURE_TYPE_MASK );

	if ( IsEffectActive( EF_NODRAW ) )
		return false;

	if( flags & SHADOW_FLAGS_FLASHLIGHT )
	{
		return true;
	}

	return BaseClass::ShouldReceiveProjectedTextures( flags );
}

void C_Portal_Player::DoImpactEffect( trace_t &tr, int nDamageType )
{
	if ( GetActiveWeapon() )
	{
		GetActiveWeapon()->DoImpactEffect( tr, nDamageType );
		return;
	}

	BaseClass::DoImpactEffect( tr, nDamageType );
}

void C_Portal_Player::PreThink( void )
{
	QAngle vTempAngles = GetLocalAngles();

	if ( IsLocalPlayer() )
	{
		vTempAngles[PITCH] = EyeAngles()[PITCH];
	}
	else
	{
		vTempAngles[PITCH] = m_angEyeAngles[PITCH];
	}

	if ( vTempAngles[YAW] < 0.0f )
	{
		vTempAngles[YAW] += 360.0f;
	}

	SetLocalAngles( vTempAngles );

	BaseClass::PreThink();

	HandleSpeedChanges();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_Portal_Player::AddEntity( void )
{
	BaseClass::AddEntity();

	QAngle vTempAngles = GetLocalAngles();
	vTempAngles[PITCH] = m_angEyeAngles[PITCH];

	SetLocalAngles( vTempAngles );

	// Zero out model pitch, blending takes care of all of it.
	SetLocalAnglesDim( X_INDEX, 0 );

	if( this != C_BasePlayer::GetLocalPlayer() )
	{
		if ( IsEffectActive( EF_DIMLIGHT ) )
		{
			int iAttachment = LookupAttachment( "anim_attachment_RH" );

			if ( iAttachment < 0 )
				return;

			Vector vecOrigin;
			QAngle eyeAngles = m_angEyeAngles;

			GetAttachment( iAttachment, vecOrigin, eyeAngles );

			Vector vForward;
			AngleVectors( eyeAngles, &vForward );

			trace_t tr;
			UTIL_TraceLine( vecOrigin, vecOrigin + (vForward * 200), MASK_SHOT, this, COLLISION_GROUP_NONE, &tr );
		}
	}
}

ShadowType_t C_Portal_Player::ShadowCastType( void ) 
{
	// Drawing player shadows looks bad in first person when they get close to walls
	// It doesn't make sense to have shadows in the portal view, but not in the main view
	// So no shadows for the player
	return SHADOWS_NONE;
}

bool C_Portal_Player::ShouldDraw( void )
{
	if ( !IsAlive() )
		return false;

	//return true;

	//	if( GetTeamNumber() == TEAM_SPECTATOR )
	//		return false;

	if( IsLocalPlayer() && IsRagdoll() )
		return true;

	if ( IsRagdoll() )
		return false;

	return true;

	return BaseClass::ShouldDraw();
}

const QAngle& C_Portal_Player::EyeAngles()
{
	if ( IsLocalPlayer() && g_nKillCamMode == OBS_MODE_NONE )
	{
		return BaseClass::EyeAngles();
	}
	else
	{
		//C_BaseEntity *pEntity1 = g_eKillTarget1.Get();
		//C_BaseEntity *pEntity2 = g_eKillTarget2.Get();

		//Vector vLook = Vector( 0.0f, 0.0f, 0.0f );

		//if ( pEntity2 )
		//{
		//	vLook = pEntity1->GetAbsOrigin() - pEntity2->GetAbsOrigin();
		//	VectorNormalize( vLook );
		//}
		//else if ( pEntity1 )
		//{
		//	return BaseClass::EyeAngles();
		//	//vLook =  - pEntity1->GetAbsOrigin();
		//}

		//if ( vLook != Vector( 0.0f, 0.0f, 0.0f ) )
		//{
		//	VectorAngles( vLook, m_angEyeAngles );
		//}

		return m_angEyeAngles;
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
// Output : IRagdoll*
//-----------------------------------------------------------------------------
IRagdoll* C_Portal_Player::GetRepresentativeRagdoll() const
{
	if ( m_hRagdoll.Get() )
	{
		C_PortalRagdoll *pRagdoll = static_cast<C_PortalRagdoll*>( m_hRagdoll.Get() );
		if ( !pRagdoll )
			return NULL;

		return pRagdoll->GetIRagdoll();
	}
	else
	{
		return NULL;
	}
}


void C_Portal_Player::PlayerPortalled( C_Prop_Portal *pEnteredPortal )
{
	if( pEnteredPortal )
	{
		m_bPortalledMessagePending = true;
		m_PendingPortalMatrix = pEnteredPortal->MatrixThisToLinked();

		if( IsLocalPlayer() )
			g_pPortalRender->EnteredPortal( pEnteredPortal );
	}
}

void C_Portal_Player::OnPreDataChanged( DataUpdateType_t type )
{
	Assert( m_pPortalEnvironment_LastCalcView == m_hPortalEnvironment.Get() );
	PreDataChanged_Backup.m_hPortalEnvironment = m_hPortalEnvironment;
	PreDataChanged_Backup.m_hSurroundingLiquidPortal = m_hSurroundingLiquidPortal;
	PreDataChanged_Backup.m_qEyeAngles = m_iv_angEyeAngles.GetCurrent();

	BaseClass::OnPreDataChanged( type );
}

void C_Portal_Player::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );

	if( m_hSurroundingLiquidPortal != PreDataChanged_Backup.m_hSurroundingLiquidPortal )
	{
		CLiquidPortal_InnerLiquidEffect *pLiquidEffect = (CLiquidPortal_InnerLiquidEffect *)g_pScreenSpaceEffects->GetScreenSpaceEffect( "LiquidPortal_InnerLiquid" );
		if( pLiquidEffect )
		{
			C_Func_LiquidPortal *pSurroundingPortal = m_hSurroundingLiquidPortal.Get();
			if( pSurroundingPortal != NULL )
			{
				C_Func_LiquidPortal *pOldSurroundingPortal = PreDataChanged_Backup.m_hSurroundingLiquidPortal.Get();
				if( pOldSurroundingPortal != pSurroundingPortal->m_hLinkedPortal.Get() )
				{
					pLiquidEffect->m_pImmersionPortal = pSurroundingPortal;
					pLiquidEffect->m_bFadeBackToReality = false;
				}
				else
				{
					pLiquidEffect->m_bFadeBackToReality = true;
					pLiquidEffect->m_fFadeBackTimeLeft = pLiquidEffect->s_fFadeBackEffectTime;
				}
			}
			else
			{
				pLiquidEffect->m_pImmersionPortal = NULL;
				pLiquidEffect->m_bFadeBackToReality = false;
			}
		}		
	}

	DetectAndHandlePortalTeleportation();

	if ( type == DATA_UPDATE_CREATED )
	{
		// Load color correction lookup for the death effect
		m_CCDeathHandle = g_pColorCorrectionMgr->AddColorCorrection( DEATH_CC_LOOKUP_FILENAME );
		if ( m_CCDeathHandle != INVALID_CLIENT_CCHANDLE )
		{
			g_pColorCorrectionMgr->SetColorCorrectionWeight( m_CCDeathHandle, 0.0f );
		}

		SetNextClientThink( CLIENT_THINK_ALWAYS );
	}

	UpdateVisibility();
}

//CalcView() gets called between OnPreDataChanged() and OnDataChanged(), and these changes need to be known about in both before CalcView() gets called, and if CalcView() doesn't get called
bool C_Portal_Player::DetectAndHandlePortalTeleportation( void )
{
	if( m_bPortalledMessagePending )
	{
		m_bPortalledMessagePending = false;

		//C_Prop_Portal *pOldPortal = PreDataChanged_Backup.m_hPortalEnvironment.Get();
		//Assert( pOldPortal );
		//if( pOldPortal )
		{
			Vector ptNewPosition = GetNetworkOrigin();

			UTIL_Portal_PointTransform( m_PendingPortalMatrix, PortalEyeInterpolation.m_vEyePosition_Interpolated, PortalEyeInterpolation.m_vEyePosition_Interpolated );
			UTIL_Portal_PointTransform( m_PendingPortalMatrix, PortalEyeInterpolation.m_vEyePosition_Uninterpolated, PortalEyeInterpolation.m_vEyePosition_Uninterpolated );

			PortalEyeInterpolation.m_bEyePositionIsInterpolating = true;

			UTIL_Portal_AngleTransform( m_PendingPortalMatrix, m_qEyeAngles_LastCalcView, m_angEyeAngles );
			m_angEyeAngles.x = AngleNormalize( m_angEyeAngles.x );
			m_angEyeAngles.y = AngleNormalize( m_angEyeAngles.y );
			m_angEyeAngles.z = AngleNormalize( m_angEyeAngles.z );
			m_iv_angEyeAngles.Reset(); //copies from m_angEyeAngles

			if( engine->IsPlayingDemo() )
			{				
				pl.v_angle = m_angEyeAngles;		
				engine->SetViewAngles( pl.v_angle );
			}

			engine->ResetDemoInterpolation();
			if( IsLocalPlayer() ) 
			{
				//DevMsg( "FPT: %.2f %.2f %.2f\n", m_angEyeAngles.x, m_angEyeAngles.y, m_angEyeAngles.z );
				SetLocalAngles( m_angEyeAngles );
			}

			m_PlayerAnimState->Teleport ( &ptNewPosition, &GetNetworkAngles(), this );

			// Reorient last facing direction to fix pops in view model lag
			for ( int i = 0; i < MAX_VIEWMODELS; i++ )
			{
				CBaseViewModel *vm = GetViewModel( i );
				if ( !vm )
					continue;

				UTIL_Portal_VectorTransform( m_PendingPortalMatrix, vm->m_vecLastFacing, vm->m_vecLastFacing );
			}
		}
		m_bPortalledMessagePending = false;
	}

	return false;
}

/*bool C_Portal_Player::ShouldInterpolate( void )
{
if( !IsInterpolationEnabled() )
return false;

return BaseClass::ShouldInterpolate();
}*/


void C_Portal_Player::PostDataUpdate( DataUpdateType_t updateType )
{
	// C_BaseEntity assumes we're networking the entity's angles, so pretend that it
	// networked the same value we already have.
	SetNetworkAngles( GetLocalAngles() );

	if ( m_iSpawnInterpCounter != m_iSpawnInterpCounterCache )
	{
		MoveToLastReceivedPosition( true );
		ResetLatched();
		m_iSpawnInterpCounterCache = m_iSpawnInterpCounter;
	}

	BaseClass::PostDataUpdate( updateType );
}

float C_Portal_Player::GetFOV( void )
{
	//Find our FOV with offset zoom value
	float flFOVOffset = C_BasePlayer::GetFOV() + GetZoom();

	// Clamp FOV in MP
	int min_fov = GetMinFOV();

	// Don't let it go too low
	flFOVOffset = MAX( min_fov, flFOVOffset );

	return flFOVOffset;
}

//=========================================================
// Autoaim
// set crosshair position to point to enemey
//=========================================================
Vector C_Portal_Player::GetAutoaimVector( float flDelta )
{
	// Never autoaim a predicted weapon (for now)
	Vector	forward;
	AngleVectors( EyeAngles() + m_Local.m_vecPunchAngle, &forward );
	return	forward;
}

//-----------------------------------------------------------------------------
// Purpose: Returns whether or not we are allowed to sprint now.
//-----------------------------------------------------------------------------
bool C_Portal_Player::CanSprint( void )
{
	return ( (!m_Local.m_bDucked && !m_Local.m_bDucking) && (GetWaterLevel() != 3) );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void C_Portal_Player::StartSprinting( void )
{
	//if( m_HL2Local.m_flSuitPower < 10 )
	//{
	//	// Don't sprint unless there's a reasonable
	//	// amount of suit power.
	//	CPASAttenuationFilter filter( this );
	//	filter.UsePredictionRules();
	//	EmitSound( filter, entindex(), "Player.SprintNoPower" );
	//	return;
	//}

	CPASAttenuationFilter filter( this );
	filter.UsePredictionRules();
	EmitSound( filter, entindex(), "Player.SprintStart" );

	SetMaxSpeed( _SPRINT_SPEED );
	m_fIsSprinting = true;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void C_Portal_Player::StopSprinting( void )
{
	SetMaxSpeed( _NORM_SPEED );
	m_fIsSprinting = false;
}

void C_Portal_Player::HandleSpeedChanges( void )
{
	int buttonsChanged = m_afButtonPressed | m_afButtonReleased;

	if( buttonsChanged & IN_SPEED )
	{
		if ( !(m_afButtonPressed & IN_SPEED)  && IsSprinting() )
		{
			StopSprinting();
		}
		else if ( (m_afButtonPressed & IN_SPEED) && !IsSprinting() )
		{
			if ( CanSprint() )
			{
				StartSprinting();
			}
			else
			{
				// Reset key, so it will be activated post whatever is suppressing it.
				m_nButtons &= ~IN_SPEED;
			}
		}
	}
}

void C_Portal_Player::ItemPreFrame( void )
{
	if ( GetFlags() & FL_FROZEN )
		return;

	// Disallow shooting while zooming
	if ( m_nButtons & IN_ZOOM )
	{
		//FIXME: Held weapons like the grenade get sad when this happens
		m_nButtons &= ~(IN_ATTACK|IN_ATTACK2);
	}

	BaseClass::ItemPreFrame();

}

void C_Portal_Player::ItemPostFrame( void )
{
	if ( GetFlags() & FL_FROZEN )
		return;

	BaseClass::ItemPostFrame();
}

C_BaseAnimating *C_Portal_Player::BecomeRagdollOnClient()
{
	// Let the C_CSRagdoll entity do this.
	// m_builtRagdoll = true;
	return NULL;
}

void C_Portal_Player::UpdatePortalEyeInterpolation( void )
{
#ifdef ENABLE_PORTAL_EYE_INTERPOLATION_CODE
	//PortalEyeInterpolation.m_bEyePositionIsInterpolating = false;
	if( PortalEyeInterpolation.m_bUpdatePosition_FreeMove )
	{
		PortalEyeInterpolation.m_bUpdatePosition_FreeMove = false;

		C_Prop_Portal *pOldPortal = PreDataChanged_Backup.m_hPortalEnvironment.Get();
		if( pOldPortal )
		{
			UTIL_Portal_PointTransform( pOldPortal->MatrixThisToLinked(), PortalEyeInterpolation.m_vEyePosition_Interpolated, PortalEyeInterpolation.m_vEyePosition_Interpolated );
			//PortalEyeInterpolation.m_vEyePosition_Interpolated = pOldPortal->m_matrixThisToLinked * PortalEyeInterpolation.m_vEyePosition_Interpolated;

			//Vector vForward;
			//m_hPortalEnvironment.Get()->GetVectors( &vForward, NULL, NULL );

			PortalEyeInterpolation.m_vEyePosition_Interpolated = EyeFootPosition();

			PortalEyeInterpolation.m_bEyePositionIsInterpolating = true;
		}
	}

	if( IsInAVehicle() )
		PortalEyeInterpolation.m_bEyePositionIsInterpolating = false;

	if( !PortalEyeInterpolation.m_bEyePositionIsInterpolating )
	{
		PortalEyeInterpolation.m_vEyePosition_Uninterpolated = EyeFootPosition();
		PortalEyeInterpolation.m_vEyePosition_Interpolated = PortalEyeInterpolation.m_vEyePosition_Uninterpolated;
		return;
	}

	Vector vThisFrameUninterpolatedPosition = EyeFootPosition();

	//find offset between this and last frame's uninterpolated movement, and apply this as freebie movement to the interpolated position
	PortalEyeInterpolation.m_vEyePosition_Interpolated += (vThisFrameUninterpolatedPosition - PortalEyeInterpolation.m_vEyePosition_Uninterpolated);
	PortalEyeInterpolation.m_vEyePosition_Uninterpolated = vThisFrameUninterpolatedPosition;

	Vector vDiff = vThisFrameUninterpolatedPosition - PortalEyeInterpolation.m_vEyePosition_Interpolated;
	float fLength = vDiff.Length();
	float fFollowSpeed = gpGlobals->frametime * 100.0f;
	const float fMaxDiff = 150.0f;
	if( fLength > fMaxDiff )
	{
		//camera lagging too far behind, give it a speed boost to bring it within maximum range
		fFollowSpeed = fLength - fMaxDiff;
	}
	else if( fLength < fFollowSpeed )
	{
		//final move
		PortalEyeInterpolation.m_bEyePositionIsInterpolating = false;
		PortalEyeInterpolation.m_vEyePosition_Interpolated = vThisFrameUninterpolatedPosition;
		return;
	}

	if ( fLength > 0.001f )
	{
		vDiff *= (fFollowSpeed/fLength);
		PortalEyeInterpolation.m_vEyePosition_Interpolated += vDiff;
	}
	else
	{
		PortalEyeInterpolation.m_vEyePosition_Interpolated = vThisFrameUninterpolatedPosition;
	}



#else
	PortalEyeInterpolation.m_vEyePosition_Interpolated = BaseClass::EyePosition();
#endif
}

Vector C_Portal_Player::EyePosition()
{
	return PortalEyeInterpolation.m_vEyePosition_Interpolated;  
}

Vector C_Portal_Player::EyeFootPosition( const QAngle &qEyeAngles )
{
#if 0
	static int iPrintCounter = 0;
	++iPrintCounter;
	if( iPrintCounter == 50 )
	{
		QAngle vAbsAngles = qEyeAngles;
		DevMsg( "Eye Angles: %f %f %f\n", vAbsAngles.x, vAbsAngles.y, vAbsAngles.z );
		iPrintCounter = 0;
	}
#endif

	//interpolate between feet and normal eye position based on view roll (gets us wall/ceiling & ceiling/ceiling teleportations without an eye position pop)
	float fFootInterp = fabs(qEyeAngles[ROLL]) * ((1.0f/180.0f) * 0.75f); //0 when facing straight up, 0.75 when facing straight down
	return (BaseClass::EyePosition() - (fFootInterp * m_vecViewOffset)); //TODO: Find a good Up vector for this rolled player and interpolate along actual eye/foot axis
}

void C_Portal_Player::CalcView( Vector &eyeOrigin, QAngle &eyeAngles, float &zNear, float &zFar, float &fov )
{
	DetectAndHandlePortalTeleportation();
	//if( DetectAndHandlePortalTeleportation() )
	//	DevMsg( "Teleported within OnDataChanged\n" );

	m_iForceNoDrawInPortalSurface = -1;
	bool bEyeTransform_Backup = m_bEyePositionIsTransformedByPortal;
	m_bEyePositionIsTransformedByPortal = false; //assume it's not transformed until it provably is
	UpdatePortalEyeInterpolation();

	QAngle qEyeAngleBackup = EyeAngles();
	Vector ptEyePositionBackup = EyePosition();
	C_Prop_Portal *pPortalBackup = m_hPortalEnvironment.Get();

	if ( m_lifeState != LIFE_ALIVE )
	{
		if ( g_nKillCamMode != 0 )
		{
			return;
		}

		Vector origin = EyePosition();

		C_BaseEntity* pRagdoll = m_hRagdoll.Get();

		if ( pRagdoll )
		{
			origin = pRagdoll->GetAbsOrigin();
#if !PORTAL_HIDE_PLAYER_RAGDOLL
			origin.z += VEC_DEAD_VIEWHEIGHT_SCALED( this ).z; // look over ragdoll, not through
#endif //PORTAL_HIDE_PLAYER_RAGDOLL
		}

		BaseClass::CalcView( eyeOrigin, eyeAngles, zNear, zFar, fov );

		eyeOrigin = origin;

		Vector vForward; 
		AngleVectors( eyeAngles, &vForward );

		VectorNormalize( vForward );
#if !PORTAL_HIDE_PLAYER_RAGDOLL
		VectorMA( origin, -CHASE_CAM_DISTANCE_MAX, vForward, eyeOrigin );
#endif //PORTAL_HIDE_PLAYER_RAGDOLL

		Vector WALL_MIN( -WALL_OFFSET, -WALL_OFFSET, -WALL_OFFSET );
		Vector WALL_MAX( WALL_OFFSET, WALL_OFFSET, WALL_OFFSET );

		trace_t trace; // clip against world
		C_BaseEntity::PushEnableAbsRecomputations( false ); // HACK don't recompute positions while doing RayTrace
		UTIL_TraceHull( origin, eyeOrigin, WALL_MIN, WALL_MAX, MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &trace );
		C_BaseEntity::PopEnableAbsRecomputations();

		if (trace.fraction < 1.0)
		{
			eyeOrigin = trace.endpos;
		}
	}
	else
	{
		IClientVehicle *pVehicle; 
		pVehicle = GetVehicle();

		if ( !pVehicle )
		{
			if ( IsObserver() )
			{
				CalcObserverView( eyeOrigin, eyeAngles, fov );
			}
			else
			{
				CalcPlayerView( eyeOrigin, eyeAngles, fov );
				if( m_hPortalEnvironment.Get() != NULL )
				{
					//time for hax
					m_bEyePositionIsTransformedByPortal = bEyeTransform_Backup;
					CalcPortalView( eyeOrigin, eyeAngles );
				}
			}
		}
		else
		{
			CalcVehicleView( pVehicle, eyeOrigin, eyeAngles, zNear, zFar, fov );
		}
	}

	m_qEyeAngles_LastCalcView = qEyeAngleBackup;
	m_ptEyePosition_LastCalcView = ptEyePositionBackup;
	m_pPortalEnvironment_LastCalcView = pPortalBackup;

#ifdef WIN32
	// NVNT Inform haptics module of fov
	if(IsLocalPlayer())
		haptics->UpdatePlayerFOV(fov);
#endif
}

void C_Portal_Player::SetLocalViewAngles( const QAngle &viewAngles )
{
	// Nothing
	if ( engine->IsPlayingDemo() )
		return;
	BaseClass::SetLocalViewAngles( viewAngles );
}

void C_Portal_Player::SetViewAngles( const QAngle& ang )
{
	BaseClass::SetViewAngles( ang );

	if ( engine->IsPlayingDemo() )
	{
		pl.v_angle = ang;
	}
}

void C_Portal_Player::CalcPortalView( Vector &eyeOrigin, QAngle &eyeAngles )
{
	//although we already ran CalcPlayerView which already did these copies, they also fudge these numbers in ways we don't like, so recopy
	VectorCopy( EyePosition(), eyeOrigin );
	VectorCopy( EyeAngles(), eyeAngles );

	//Re-apply the screenshake (we just stomped it)
	vieweffects->ApplyShake( eyeOrigin, eyeAngles, 1.0 );

	C_Prop_Portal *pPortal = m_hPortalEnvironment.Get();
	assert( pPortal );

	C_Prop_Portal *pRemotePortal = pPortal->m_hLinkedPortal;
	if( !pRemotePortal )
	{
		return; //no hacks possible/necessary
	}

	Vector ptPortalCenter;
	Vector vPortalForward;

	ptPortalCenter = pPortal->GetNetworkOrigin();
	pPortal->GetVectors( &vPortalForward, NULL, NULL );
	float fPortalPlaneDist = vPortalForward.Dot( ptPortalCenter );

	bool bOverrideSpecialEffects = false; //sometimes to get the best effect we need to kill other effects that are simply for cleanliness

	float fEyeDist = vPortalForward.Dot( eyeOrigin ) - fPortalPlaneDist;
	bool bTransformEye = false;
	if( fEyeDist < 0.0f ) //eye behind portal
	{
		if( pPortal->m_PortalSimulator.EntityIsInPortalHole( this ) ) //player standing in portal
		{
			bTransformEye = true;
		}
		else if( vPortalForward.z < -0.01f ) //there's a weird case where the player is ducking below a ceiling portal. As they unduck their eye moves beyond the portal before the code detects that they're in the portal hole.
		{
			Vector ptPlayerOrigin = GetAbsOrigin();
			float fOriginDist = vPortalForward.Dot( ptPlayerOrigin ) - fPortalPlaneDist;

			if( fOriginDist > 0.0f )
			{
				float fInvTotalDist = 1.0f / (fOriginDist - fEyeDist); //fEyeDist is negative
				Vector ptPlaneIntersection = (eyeOrigin * fOriginDist * fInvTotalDist) - (ptPlayerOrigin * fEyeDist * fInvTotalDist);
				Assert( fabs( vPortalForward.Dot( ptPlaneIntersection ) - fPortalPlaneDist ) < 0.01f );

				Vector vIntersectionTest = ptPlaneIntersection - ptPortalCenter;

				Vector vPortalRight, vPortalUp;
				pPortal->GetVectors( NULL, &vPortalRight, &vPortalUp );

				if( (vIntersectionTest.Dot( vPortalRight ) <= PORTAL_HALF_WIDTH) &&
					(vIntersectionTest.Dot( vPortalUp ) <= PORTAL_HALF_HEIGHT) )
				{
					bTransformEye = true;
				}
			}
		}		
	}

	if( bTransformEye )
	{
		m_bEyePositionIsTransformedByPortal = true;

		//DevMsg( 2, "transforming portal view from <%f %f %f> <%f %f %f>\n", eyeOrigin.x, eyeOrigin.y, eyeOrigin.z, eyeAngles.x, eyeAngles.y, eyeAngles.z );

		VMatrix matThisToLinked = pPortal->MatrixThisToLinked();
		UTIL_Portal_PointTransform( matThisToLinked, eyeOrigin, eyeOrigin );
		UTIL_Portal_AngleTransform( matThisToLinked, eyeAngles, eyeAngles );

		//DevMsg( 2, "transforming portal view to   <%f %f %f> <%f %f %f>\n", eyeOrigin.x, eyeOrigin.y, eyeOrigin.z, eyeAngles.x, eyeAngles.y, eyeAngles.z );

		if ( IsToolRecording() )
		{
			static EntityTeleportedRecordingState_t state;

			KeyValues *msg = new KeyValues( "entity_teleported" );
			msg->SetPtr( "state", &state );
			state.m_bTeleported = false;
			state.m_bViewOverride = true;
			state.m_vecTo = eyeOrigin;
			state.m_qaTo = eyeAngles;
			MatrixInvert( matThisToLinked.As3x4(), state.m_teleportMatrix );

			// Post a message back to all IToolSystems
			Assert( (int)GetToolHandle() != 0 );
			ToolFramework_PostToolMessage( GetToolHandle(), msg );

			msg->deleteThis();
		}

		bOverrideSpecialEffects = true;
	}
	else
	{
		m_bEyePositionIsTransformedByPortal = false;
	}

	if( bOverrideSpecialEffects )
	{		
		m_iForceNoDrawInPortalSurface = ((pRemotePortal->m_bIsPortal2)?(2):(1));
		pRemotePortal->m_fStaticAmount = 0.0f;
	}
}

extern float g_fMaxViewModelLag;
void C_Portal_Player::CalcViewModelView( const Vector& eyeOrigin, const QAngle& eyeAngles)
{
	// HACK: Manually adjusting the eye position that view model looking up and down are similar
	// (solves view model "pop" on floor to floor transitions)
	Vector vInterpEyeOrigin = eyeOrigin;

	Vector vForward;
	Vector vRight;
	Vector vUp;
	AngleVectors( eyeAngles, &vForward, &vRight, &vUp );

	if ( vForward.z < 0.0f )
	{
		float fT = vForward.z * vForward.z;
		vInterpEyeOrigin += vRight * ( fT * 4.7f ) + vForward * ( fT * 5.0f ) + vUp * ( fT * 4.0f );
	}

	if ( UTIL_IntersectEntityExtentsWithPortal( this ) )
		g_fMaxViewModelLag = 0.0f;
	else
		g_fMaxViewModelLag = 1.5f;

	for ( int i = 0; i < MAX_VIEWMODELS; i++ )
	{
		CBaseViewModel *vm = GetViewModel( i );
		if ( !vm )
			continue;

		vm->CalcViewModelView( this, vInterpEyeOrigin, eyeAngles );
	}
}

bool LocalPlayerIsCloseToPortal( void )
{
	return C_Portal_Player::GetLocalPlayer()->IsCloseToPortal();
}

