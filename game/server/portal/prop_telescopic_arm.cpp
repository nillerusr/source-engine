//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Implements the big scary boom-boom machine Antlions fear.
//
//=============================================================================//

#include "cbase.h"
#include "baseentity.h"
#include "rotorwash.h"
#include "soundenvelope.h"
#include "engine/IEngineSound.h"
#include "te_effect_dispatch.h"
#include "point_posecontroller.h"
#include "prop_portal_shared.h"


#define TELESCOPE_ENABLE_TIME 1.0f
#define TELESCOPE_DISABLE_TIME 2.0f
#define TELESCOPE_ROTATEX_TIME 0.5f
#define TELESCOPE_ROTATEY_TIME 0.5f

#define TELESCOPING_ARM_MODEL_NAME "models/props/telescopic_arm.mdl"

#define DEBUG_TELESCOPIC_ARM_AIM 1


class CPropTelescopicArm : public CBaseAnimating
{
public:
	DECLARE_CLASS( CPropTelescopicArm, CBaseAnimating );
	DECLARE_DATADESC();

	virtual void UpdateOnRemove( void );
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual void Activate ( void );

	void	DisabledThink( void );
	void	EnabledThink( void );

	void	AimAt( Vector vTarget );
	bool	TestLOS( const Vector& vAimPoint );
	void	SetTarget( const char *pTargetName );
	void	SetTarget( CBaseEntity *pTarget );

	void	InputDisable( inputdata_t &inputdata );
	void	InputEnable( inputdata_t &inputdata );

	void	InputSetTarget( inputdata_t &inputdata );
	void	InputTargetPlayer( inputdata_t &inputdata );

private:

	Vector FindTargetAimPoint( void );
	Vector FindAimPointThroughPortal ( const CProp_Portal* pPortal );
	
	bool m_bEnabled;
	bool m_bCanSeeTarget;
	int m_iFrontMarkerAttachment;

	EHANDLE	m_hRotXPoseController;
	EHANDLE	m_hRotYPoseController;
	EHANDLE	m_hTelescopicPoseController;

	EHANDLE m_hAimTarget;
	
	COutputEvent m_OnLostTarget;
	COutputEvent m_OnFoundTarget;
};

LINK_ENTITY_TO_CLASS( prop_telescopic_arm, CPropTelescopicArm );

//-----------------------------------------------------------------------------
// Save/load 
//-----------------------------------------------------------------------------
BEGIN_DATADESC( CPropTelescopicArm )
	DEFINE_FIELD( m_bEnabled, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bCanSeeTarget, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_iFrontMarkerAttachment, FIELD_INTEGER ),
	DEFINE_FIELD( m_hRotXPoseController, FIELD_EHANDLE ),
	DEFINE_FIELD( m_hRotYPoseController, FIELD_EHANDLE ),
	DEFINE_FIELD( m_hTelescopicPoseController, FIELD_EHANDLE ),
	DEFINE_FIELD( m_hAimTarget, FIELD_EHANDLE ),
	DEFINE_THINKFUNC( DisabledThink ),
	DEFINE_THINKFUNC( EnabledThink ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Disable", InputDisable ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Enable", InputEnable ),
	DEFINE_INPUTFUNC( FIELD_STRING, "SetTarget", InputSetTarget ),
	DEFINE_INPUTFUNC( FIELD_VOID, "TargetPlayer", InputTargetPlayer ),
	DEFINE_OUTPUT ( m_OnLostTarget, "OnLostTarget" ),
	DEFINE_OUTPUT ( m_OnFoundTarget, "OnFoundTarget" ),
END_DATADESC()


void CPropTelescopicArm::UpdateOnRemove( void )
{
	CPoseController *pPoseController;

	pPoseController = static_cast<CPoseController*>( m_hRotXPoseController.Get() );
	if ( pPoseController )
		UTIL_Remove( pPoseController );
	m_hRotXPoseController = 0;

	pPoseController = static_cast<CPoseController*>( m_hRotYPoseController.Get() );
	if ( pPoseController )
		UTIL_Remove( pPoseController );
	m_hRotYPoseController = 0;

	pPoseController = static_cast<CPoseController*>( m_hTelescopicPoseController.Get() );
	if ( pPoseController )
		UTIL_Remove( pPoseController );
	m_hTelescopicPoseController = 0;
}

void CPropTelescopicArm::Spawn( void )
{
	char *szModel = (char *)STRING( GetModelName() );
	if (!szModel || !*szModel)
	{
		szModel = TELESCOPING_ARM_MODEL_NAME;
		SetModelName( AllocPooledString(szModel) );
	}

	Precache();
	SetModel( szModel );

	SetSolid( SOLID_VPHYSICS );
	SetMoveType( MOVETYPE_PUSH );
	VPhysicsInitStatic();

	BaseClass::Spawn();

	m_bEnabled = false;
	m_bCanSeeTarget = false;

	SetThink( &CPropTelescopicArm::DisabledThink );
	SetNextThink( gpGlobals->curtime + 1.0f );

	int iSequence = SelectHeaviestSequence ( ACT_IDLE );

	if ( iSequence != ACT_INVALID )
	{
		 SetSequence( iSequence );
		 ResetSequenceInfo();

		 //Do this so we get the nice ramp-up effect.
		 m_flPlaybackRate = random->RandomFloat( 0.0f, 1.0f );
	}

	m_iFrontMarkerAttachment = LookupAttachment( "Front_marker" );

	CPoseController *pPoseController;

	pPoseController = static_cast<CPoseController*>( CreateEntityByName( "point_posecontroller" ) );
	DispatchSpawn( pPoseController );
	if ( pPoseController )
	{
		pPoseController->SetProp( this );
		pPoseController->SetInterpolationWrap( true );
		pPoseController->SetPoseParameterName( "rot_x" );
		m_hRotXPoseController = pPoseController;
	}

	pPoseController = static_cast<CPoseController*>( CreateEntityByName( "point_posecontroller" ) );
	DispatchSpawn( pPoseController );
	if ( pPoseController )
	{
		pPoseController->SetProp( this );
		pPoseController->SetInterpolationWrap( true );
		pPoseController->SetPoseParameterName( "rot_y" );
		m_hRotYPoseController = pPoseController;
	}

	pPoseController = static_cast<CPoseController*>( CreateEntityByName( "point_posecontroller" ) );
	DispatchSpawn( pPoseController );
	if ( pPoseController )
	{
		pPoseController->SetProp( this );
		pPoseController->SetPoseParameterName( "telescopic" );
		m_hTelescopicPoseController = pPoseController;
	}
}

void CPropTelescopicArm::Precache( void )
{
	BaseClass::Precache();

	PrecacheModel( STRING( GetModelName() ) );
	PrecacheScriptSound( "coast.thumper_hit" );
	PrecacheScriptSound( "coast.thumper_ambient" );
	PrecacheScriptSound( "coast.thumper_dust" );
	PrecacheScriptSound( "coast.thumper_startup" );
	PrecacheScriptSound( "coast.thumper_shutdown" );
	PrecacheScriptSound( "coast.thumper_large_hit" );
}

void CPropTelescopicArm::Activate( void )
{
	BaseClass::Activate();
}

void CPropTelescopicArm::DisabledThink( void )
{
	SetNextThink( gpGlobals->curtime + 1.0 );
}

void CPropTelescopicArm::EnabledThink( void )
{
	CBaseEntity *pTarget = m_hAimTarget.Get();

	if ( !pTarget )
	{
		//SetTarget ( UTIL_PlayerByIndex( 1 ) );

		// Default to targeting a player
		for( int i = 1; i <= gpGlobals->maxClients; ++i )
		{
			CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
			if( pPlayer && FVisible( pPlayer ) && pPlayer->IsAlive() )
			{
				pTarget = pPlayer;
				break;
			}
		}
		if( pTarget == NULL )
		{
			//search again, but don't require the player to be visible
			for( int i = 1; i <= gpGlobals->maxClients; ++i )
			{
				CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
				if( pPlayer && pPlayer->IsAlive() )
				{
					pTarget = pPlayer;
					break;
				}
			}

			if( pTarget == NULL )
			{
				//search again, but don't require the player to be visible or alive
				for( int i = 1; i <= gpGlobals->maxClients; ++i )
				{
					CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
					if( pPlayer )
					{
						pTarget = pPlayer;
						break;
					}
				}
			}
		}	

		if( pTarget )
			SetTarget( pTarget );
	}

	if ( pTarget )
	{
		// Aim at the center of the abs box
		Vector vAimPoint = FindTargetAimPoint();
		Assert ( vAimPoint != vec3_invalid );
		AimAt( vAimPoint );

		// We have direct line of sight to our target
		if ( TestLOS ( vAimPoint ) )
		{
			// Just aquired LOS
			if ( !m_bCanSeeTarget )
			{
				m_OnFoundTarget.FireOutput( m_hAimTarget, this );
			}
			m_bCanSeeTarget = true;
		}
		// No LOS to target
		else
		{
			// Just lost LOS
			if ( m_bCanSeeTarget )
			{
				m_OnLostTarget.FireOutput( m_hAimTarget, this );
			}
			m_bCanSeeTarget = false;
		}
	}

	SetNextThink( gpGlobals->curtime + 0.1 );
}

//-----------------------------------------------------------------------------
// Purpose: Finds the point to aim at in order to see the target entity.
//			Note: Also considers aim paths through portals.
// Output : Point of the target
//-----------------------------------------------------------------------------
Vector CPropTelescopicArm::FindTargetAimPoint( void )
{
	CBaseEntity *pTarget = m_hAimTarget.Get();

	if ( !pTarget )
	{
		// No target to aim at, can't return meaningful info
		Warning( "CPropTelescopicArm::FindTargetAimPoint called with no valid target entity." );
		return vec3_invalid;
	}
	else
	{
		Vector vFrontPoint;
		GetAttachment( m_iFrontMarkerAttachment, vFrontPoint, NULL, NULL, NULL );

		// Aim at the target through the world
		Vector vAimPoint = pTarget->GetAbsOrigin() + ( pTarget->WorldAlignMins() + pTarget->WorldAlignMaxs() ) * 0.5f;
		//float  fDistToPoint = vFrontPoint.DistToSqr( vAimPoint );

		CProp_Portal *pShortestDistPortal = NULL;
		UTIL_Portal_ShortestDistance( vFrontPoint, vAimPoint, &pShortestDistPortal, true );

		Vector ptShortestAimPoint;
		if( pShortestDistPortal )
		{
			ptShortestAimPoint = FindAimPointThroughPortal( pShortestDistPortal );
			if( ptShortestAimPoint == vec3_invalid )
				ptShortestAimPoint = vAimPoint;
		}
		else
		{
			ptShortestAimPoint = vAimPoint;
		}

		return ptShortestAimPoint;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Find the center of the target entity as seen through the specified portal
// Input  : pPortal - The portal to look through
// Output : Vector& output point in world space where the target *appears* to be as seen through the portal
//-----------------------------------------------------------------------------
Vector CPropTelescopicArm::FindAimPointThroughPortal( const CProp_Portal* pPortal )
{ 
	if ( pPortal && pPortal->m_bActivated )
	{
		CProp_Portal* pLinked = pPortal->m_hLinkedPortal.Get();
		CBaseEntity*  pTarget = m_hAimTarget.Get();

		if ( pLinked && pLinked->m_bActivated && pTarget )
		{
			VMatrix matToPortalView = pLinked->m_matrixThisToLinked;
			Vector vTargetAimPoint = pTarget->GetAbsOrigin() + ( pTarget->WorldAlignMins() + pTarget->WorldAlignMaxs() ) * 0.5f;

			return matToPortalView * vTargetAimPoint;
		}
	}
	
	// Bad portal pointer, not linked, no target or otherwise failed
	return vec3_invalid;
}

//-----------------------------------------------------------------------------
// Purpose: Tests if this prop's front point has direct line of sight to it's target entity
// Input  : vAimPoint - The point to aim at
// Output : Returns true if target is in direct line of sight, false otherwise.
//-----------------------------------------------------------------------------
bool CPropTelescopicArm::TestLOS( const Vector& vAimPoint )
{
	// Test for LOS and fire outputs if the sight condition changes
	Vector vFaceOrigin;
	trace_t tr;
	GetAttachment( m_iFrontMarkerAttachment, vFaceOrigin, NULL, NULL, NULL );
	Ray_t ray;
	ray.Init( vFaceOrigin, vAimPoint );
	ray.m_IsRay = true;

	// This aim point does hit target, now make sure there are no blocking objects in the way
	CTraceFilterWorldAndPropsOnly filter;
	UTIL_Portal_TraceRay( ray, MASK_SHOT, &filter, &tr );
	return !(tr.fraction < 1.0f);
}

void CPropTelescopicArm::AimAt( Vector vTarget )
{
	Vector vFaceOrigin;
	GetAttachment( m_iFrontMarkerAttachment, vFaceOrigin, NULL, NULL, NULL );

	Vector vNormalToTarget = vTarget - vFaceOrigin;
	VectorNormalize( vNormalToTarget );

	VMatrix vWorldToLocalRotation = EntityToWorldTransform();
	vNormalToTarget = vWorldToLocalRotation.InverseTR().ApplyRotation( vNormalToTarget );

	Vector vUp;
	GetVectors( NULL, NULL, &vUp );

	QAngle qAnglesToTarget;
	VectorAngles( vNormalToTarget, vUp, qAnglesToTarget );

	float fNewX = ( qAnglesToTarget.x + 90.0f ) / 360.0f;
	float fNewY = qAnglesToTarget.y / 360.0f;

	if ( fNewY < 0.0f )
		fNewY += 1.0f;

	CPoseController *pPoseController = static_cast<CPoseController*>( m_hRotXPoseController.Get() );
	if ( pPoseController )
	{
		pPoseController->SetInterpolationTime( TELESCOPE_ROTATEX_TIME );
		pPoseController->SetPoseValue( fNewX );	
	}

	pPoseController = static_cast<CPoseController*>( m_hRotYPoseController.Get() );
	if ( pPoseController )
	{
		pPoseController->SetInterpolationTime( TELESCOPE_ROTATEY_TIME );
		pPoseController->SetPoseValue( fNewY );	
	}
}

void CPropTelescopicArm::SetTarget( const char *pchTargetName )
{
	CBaseEntity *pTarget = gEntList.FindEntityByName( NULL, pchTargetName, NULL, NULL );

	//if ( pTarget == NULL )
	//	pTarget = UTIL_PlayerByIndex( 1 );

	return SetTarget( pTarget );
}

void CPropTelescopicArm::SetTarget( CBaseEntity *pTarget )
{
	m_hAimTarget = pTarget;
}

void CPropTelescopicArm::InputDisable( inputdata_t &inputdata )
{
	if ( m_bEnabled )
	{
		m_bEnabled = false;
		
		EmitSound( "coast.thumper_shutdown" );

		CPoseController *pPoseController;

		pPoseController = static_cast<CPoseController*>( m_hRotXPoseController.Get() );
		if ( pPoseController )
		{
			pPoseController->SetInterpolationTime( TELESCOPE_DISABLE_TIME * 0.5f );
			pPoseController->SetPoseValue( 0.0f );
		}

		pPoseController = static_cast<CPoseController*>( m_hRotYPoseController.Get() );
		if ( pPoseController )
		{
			pPoseController->SetInterpolationTime( TELESCOPE_DISABLE_TIME * 0.5f );
			pPoseController->SetPoseValue( 0.0f );
		}

		pPoseController = static_cast<CPoseController*>( m_hTelescopicPoseController.Get() );
		if ( pPoseController )
		{
			pPoseController->SetInterpolationTime( TELESCOPE_DISABLE_TIME );
			pPoseController->SetPoseValue( 0.0f );
		}

		SetThink( &CPropTelescopicArm::DisabledThink );
		SetNextThink( gpGlobals->curtime + TELESCOPE_DISABLE_TIME );
	}
}

void CPropTelescopicArm::InputEnable( inputdata_t &inputdata )
{
	if ( !m_bEnabled )
	{
		m_bEnabled = true;

		EmitSound( "coast.thumper_startup" );

		CPoseController *pPoseController;
		pPoseController = static_cast<CPoseController*>( m_hTelescopicPoseController.Get() );

		if ( pPoseController )
		{
			pPoseController->SetInterpolationTime( TELESCOPE_ENABLE_TIME );
			pPoseController->SetPoseValue( 1.0f );
		}

		SetThink( &CPropTelescopicArm::EnabledThink );
		SetNextThink( gpGlobals->curtime + TELESCOPE_ENABLE_TIME );
	}
}

void CPropTelescopicArm::InputSetTarget( inputdata_t &inputdata )
{
	SetTarget( inputdata.value.String() );
}

void CPropTelescopicArm::InputTargetPlayer( inputdata_t &inputdata )
{
	//SetTarget( UTIL_PlayerByIndex( 1 ) );

	CBaseEntity *pTarget = NULL;
	for( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
		if( pPlayer && FVisible( pPlayer ) && pPlayer->IsAlive() )
		{
			pTarget = pPlayer;
			break;
		}
	}
	if( pTarget == NULL )
	{
		//search again, but don't require the player to be visible
		for( int i = 1; i <= gpGlobals->maxClients; ++i )
		{
			CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
			if( pPlayer && pPlayer->IsAlive() )
			{
				pTarget = pPlayer;
				break;
			}
		}

		if( pTarget == NULL )
		{
			//search again, but don't require the player to be visible or alive
			for( int i = 1; i <= gpGlobals->maxClients; ++i )
			{
				CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
				if( pPlayer )
				{
					pTarget = pPlayer;
					break;
				}
			}
		}
	}	

	if( pTarget )
		SetTarget( pTarget );
}