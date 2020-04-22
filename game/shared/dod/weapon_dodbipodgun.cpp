//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h" 
#include "fx_dod_shared.h"
#include "weapon_dodbipodgun.h"
#include "dod_gamerules.h"
#include "engine/IEngineSound.h"

#ifndef CLIENT_DLL
#include "ndebugoverlay.h"
#endif

IMPLEMENT_NETWORKCLASS_ALIASED( DODBipodWeapon, DT_BipodWeapon )

BEGIN_NETWORK_TABLE( CDODBipodWeapon, DT_BipodWeapon )
#ifdef CLIENT_DLL
	RecvPropBool( RECVINFO( m_bDeployed ) ),
	RecvPropInt( RECVINFO( m_iDeployedReloadModelIndex) ),
#else
	SendPropBool( SENDINFO( m_bDeployed ) ),
	SendPropModelIndex( SENDINFO(m_iDeployedReloadModelIndex) ),
#endif
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CDODBipodWeapon )
	DEFINE_PRED_FIELD( m_bDeployed, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE )
END_PREDICTION_DATA()
#endif

CDODBipodWeapon::CDODBipodWeapon()
{
}

void CDODBipodWeapon::Spawn()
{
	SetDeployed( false );
	m_flNextDeployCheckTime = 0;

	m_iCurrentWorldModel = 0;

	m_iAltFireHint = HINT_USE_DEPLOY;

	BaseClass::Spawn();
}

void CDODBipodWeapon::SetDeployed( bool bDeployed )
{
	if ( bDeployed == false )
	{
		m_hDeployedOnEnt = NULL;
		m_DeployedEntOrigin = vec3_origin;
		m_flDeployedHeight = 0;

#ifdef GAME_DLL
		CDODPlayer *pPlayer = ToDODPlayer( GetPlayerOwner() );
		if ( pPlayer )
		{
			pPlayer->HandleDeployedMGKillCount( 0 );	// reset when we undeploy
		}
#endif
	}

	m_bDeployed = bDeployed;
}

void CDODBipodWeapon::Precache( void )
{
	// precache base first, it loads weapon scripts
	BaseClass::Precache();

	const CDODWeaponInfo &info = GetDODWpnData();

	if( Q_strlen(info.m_szDeployedModel) > 0 )
	{
		Assert( info.m_iAltWpnCriteria & ALTWPN_CRITERIA_DEPLOYED );
		m_iDeployedModelIndex = CBaseEntity::PrecacheModel( info.m_szDeployedModel );
	}

	if( Q_strlen(info.m_szDeployedReloadModel) > 0 )
	{
		Assert( info.m_iAltWpnCriteria & ALTWPN_CRITERIA_DEPLOYED_RELOAD );
		m_iDeployedReloadModelIndex = CBaseEntity::PrecacheModel( info.m_szDeployedReloadModel );
	}

	if( Q_strlen(info.m_szProneDeployedReloadModel) > 0 )
	{
		Assert( info.m_iAltWpnCriteria & ALTWPN_CRITERIA_PRONE_DEPLOYED_RELOAD );
		m_iProneDeployedReloadModelIndex = CBaseEntity::PrecacheModel( info.m_szProneDeployedReloadModel );
	}

	m_iCurrentWorldModel = m_iWorldModelIndex;
	Assert( m_iCurrentWorldModel != 0 );
}

bool CDODBipodWeapon::CanDrop( void )
{
	return ( IsDeployed() == false );
}

bool CDODBipodWeapon::CanHolster( void )
{
	return ( IsDeployed() == false );
}

void CDODBipodWeapon::Drop( const Vector &vecVelocity )
{
	// If a player is killed while deployed, this resets the weapon state
	SetDeployed( false );

	BaseClass::Drop( vecVelocity );
}

void CDODBipodWeapon::SecondaryAttack( void )
{
	// Toggle deployed / undeployed

	if ( IsDeployed() )
		UndeployBipod();
	else 
	{
		if ( CanAttack() )
		{
			bool bSuccess = AttemptToDeploy();

			CDODPlayer *pPlayer = ToDODPlayer( GetPlayerOwner() );
			Assert( pPlayer );

			if ( !bSuccess )
			{
				pPlayer->HintMessage( HINT_MG_DEPLOY_USAGE );
			}
			else
			{
#ifndef CLIENT_DLL
				pPlayer->RemoveHintTimer( m_iAltFireHint );
#endif
			}
		}
	}
}

bool CDODBipodWeapon::Reload( void )
{
	bool bSuccess = BaseClass::Reload();

	if ( bSuccess )
	{
		m_flNextSecondaryAttack = gpGlobals->curtime;
	}

	return bSuccess;
}

#include "in_buttons.h"
// check in busy frame too, to catch cancelling reloads
void CDODBipodWeapon::ItemBusyFrame( void )
{
	BipodThink();

	CBasePlayer *pPlayer = GetPlayerOwner();

	if ( !pPlayer )
		return;

	if ((pPlayer->m_nButtons & IN_ATTACK2) && (m_flNextSecondaryAttack <= gpGlobals->curtime))
	{
		SecondaryAttack();

		pPlayer->m_nButtons &= ~IN_ATTACK2;
	}

	BaseClass::ItemBusyFrame();
}

void CDODBipodWeapon::ItemPostFrame( void )
{
	BipodThink();
	BaseClass::ItemPostFrame();
}

// see if we're still deployed on the same entity at the same height
// in future can be expanded to check when deploying on other ents that may move / die / break
void CDODBipodWeapon::BipodThink( void )
{
	if ( m_flNextDeployCheckTime < gpGlobals->curtime )
	{
		if ( IsDeployed() )
		{
			if ( CheckDeployEnt() == false )
			{
				UndeployBipod();

				// cancel any reload in progress
				m_bInReload = false;
				m_flNextPrimaryAttack = gpGlobals->curtime + 0.1;
				m_flNextSecondaryAttack = gpGlobals->curtime + 0.1;
			}
		}

		m_flNextDeployCheckTime = gpGlobals->curtime + 0.2;
	}	
}

void CDODBipodWeapon::DoFireEffects()
{
	BaseClass::DoFireEffects();

	CBaseEntity *pDeployedOn = m_hDeployedOnEnt.Get();

	// in future can be expanded to check when deploying on other ents that may move / die / break
	if ( pDeployedOn && pDeployedOn->IsPlayer() && IsDeployed() )
	{
#ifndef CLIENT_DLL
		CSingleUserRecipientFilter user( (CBasePlayer *)pDeployedOn );
		enginesound->SetPlayerDSP( user, 32, false );
#endif
	}
}

// Do the work of deploying the gun at the current location and angles
void CDODBipodWeapon::DeployBipod( float flHeight, CBaseEntity *pDeployedOn, float flYawLimitLeft, float flYawLimitRight )
{
	m_flDeployedHeight = flHeight;
	m_hDeployedOnEnt = pDeployedOn;

	if ( pDeployedOn )
        m_DeployedEntOrigin = pDeployedOn->GetAbsOrigin();
	else
		m_DeployedEntOrigin = vec3_origin;	// world ent 

	SendWeaponAnim( GetDeployActivity() );
	SetDeployed( true );

	CDODPlayer *pPlayer = ToDODPlayer( GetPlayerOwner() );
	pPlayer->m_Shared.SetDeployed( true, flHeight );
	pPlayer->m_Shared.SetDeployedYawLimits( flYawLimitLeft, flYawLimitRight );

	// Save this off so we do duck checks later, even though we won't be flagged as ducking
	m_bDuckedWhenDeployed = pPlayer->m_Shared.IsDucking();	

	// More TODO:
	// recalc our yaw limits if the item we're deployed on has moved or rotated
	// if our new limits are outside our current eye angles, undeploy us

	m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration();
	m_flNextSecondaryAttack = gpGlobals->curtime + SequenceDuration();
	m_flTimeWeaponIdle = gpGlobals->curtime + SequenceDuration();
}

// Do the work of undeploying the gun
void CDODBipodWeapon::UndeployBipod( void )
{
	SendWeaponAnim( GetUndeployActivity() );
	SetDeployed( false );

	CDODPlayer *pPlayer = ToDODPlayer( GetPlayerOwner() );
	pPlayer->m_Shared.SetDeployed( false );

	// if we cancelled our reload by undeploying, don't let the reload complete
	if ( m_bInReload )
		m_bInReload = false;

	m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration();
	m_flNextSecondaryAttack = gpGlobals->curtime + SequenceDuration();
	pPlayer->m_flNextAttack = m_flNextPrimaryAttack;
	m_flTimeWeaponIdle = gpGlobals->curtime + SequenceDuration();
}

#ifndef CLIENT_DLL
ConVar dod_debugmgdeploy( "dod_debugmgdeploy", "0", FCVAR_CHEAT|FCVAR_GAMEDLL );
#endif

bool CDODBipodWeapon::AttemptToDeploy( void )
{
	CDODPlayer *pPlayer = ToDODPlayer( GetPlayerOwner() );

	if ( pPlayer->GetGroundEntity() == NULL )
		return false;

	if ( pPlayer->m_Shared.IsGettingUpFromProne() || pPlayer->m_Shared.IsGoingProne() )
		return false;

	CBaseEntity *pDeployedOn = NULL;
	float flDeployedHeight = 0.0f;
	float flYawLimitLeft = 0;
	float flYawLimitRight = 0;

	if ( TestDeploy( &flDeployedHeight, &pDeployedOn, &flYawLimitLeft, &flYawLimitRight ) )
	{
		if ( pPlayer->m_Shared.IsProne() && !pPlayer->m_Shared.IsGettingUpFromProne() )
		{
			DeployBipod( flDeployedHeight, NULL, flYawLimitLeft, flYawLimitRight );
			return true;
		}
		else
		{
			float flMinDeployHeight = 24.0;
			if( flDeployedHeight >= flMinDeployHeight )
			{
				DeployBipod( flDeployedHeight, pDeployedOn, flYawLimitLeft, flYawLimitRight );
				return true;
			}
		}		
	}

	return false;
}

bool CDODBipodWeapon::CheckDeployEnt( void )
{
	CBaseEntity *pDeployedOn = NULL;
	float flDeployedHeight = 0.0f;

	if ( TestDeploy( &flDeployedHeight, &pDeployedOn ) == false )
		return false;

	// If the entity we were deployed on has changed, or has moved, the origin
	// of it will be different. If so, recalc our yaw limits.
	if ( pDeployedOn )
	{
		if ( m_DeployedEntOrigin != pDeployedOn->GetAbsOrigin() )
		{
			float flYawLimitLeft = 0, flYawLimitRight = 0;
			TestDeploy( &flDeployedHeight, &pDeployedOn, &flYawLimitLeft, &flYawLimitRight );

			CDODPlayer *pPlayer = ToDODPlayer( GetPlayerOwner() );
			
			if ( pPlayer )
				pPlayer->m_Shared.SetDeployedYawLimits( flYawLimitLeft, flYawLimitRight );

			m_DeployedEntOrigin = pDeployedOn->GetAbsOrigin();
		}
	}
	
	// 20 unit tolerance in height
	if ( abs( m_flDeployedHeight - flDeployedHeight ) > 20 )
		return false;

	return true;
}

bool CDODBipodWeapon::TestDeploy( float *flDeployedHeight, CBaseEntity **pDeployedOn, float *flYawLimitLeft /* = NULL */, float *flYawLimitRight /* = NULL */ )
{
	CDODPlayer *pPlayer = ToDODPlayer( GetPlayerOwner() );

	QAngle angles = pPlayer->EyeAngles();

	float flPitch = angles[PITCH];
	if( flPitch > 180 )
	{
		flPitch -= 360;
	}

	if( flPitch > MIN_DEPLOY_PITCH || flPitch < MAX_DEPLOY_PITCH )
	{
		return false;
	}

	bool bSuccess = false;

	// if we're not finding the range, test at the current angles
	if ( flYawLimitLeft == NULL && flYawLimitRight == NULL )
	{
		// test our current angle only
		bSuccess = TestDeployAngle( pPlayer, flDeployedHeight, pDeployedOn, angles );
	}
	else
	{
		float flSaveYaw = angles[YAW];

		const float flAngleDelta = 5;
		const float flMaxYaw = 45;

		float flLeft = 0;
		float flRight = 0;

		float flTestDeployHeight = 0;
		CBaseEntity *pTestDeployedOn = NULL;

		// Sweep Left
		while ( flLeft <= flMaxYaw )
		{
			angles[YAW] = flSaveYaw + flLeft;

			if ( TestDeployAngle( pPlayer, &flTestDeployHeight, &pTestDeployedOn, angles ) == true )
			{
				if ( flLeft == 0 )	// first sweep is authoritative on deploy height and entity
				{
					*flDeployedHeight = flTestDeployHeight;
					*pDeployedOn = pTestDeployedOn;
				}
				else if ( abs( *flDeployedHeight - flTestDeployHeight ) > 20 )
				{
					// don't allow yaw to a position that is too different in height
					break;
				}

				*flYawLimitLeft = flLeft;
			}
			else
			{
				break;
			}
			flLeft += flAngleDelta;
		}

		// can't deploy here, drop out early
		if ( flLeft <= 0 )
			return false;

		// we already tested directly ahead and it was clear. skip one test
		flRight += flAngleDelta;

		// Sweep Right
		while ( flRight <= flMaxYaw )
		{
			angles[YAW] = flSaveYaw - flRight;

			if ( TestDeployAngle( pPlayer, &flTestDeployHeight, &pTestDeployedOn, angles ) == true )
			{
				if ( abs( *flDeployedHeight - flTestDeployHeight ) > 20 )
				{
					// don't allow yaw to a position that is too different in height
					break;
				}

				*flYawLimitRight = flRight;
			}
			else
			{
				break;
			}
			flRight += flAngleDelta;
		}

		bSuccess = true;
	}

	return bSuccess;
}

//ConVar dod_deploy_box_size( "dod_deploy_box_size", "8", FCVAR_REPLICATED );

#include "util_shared.h"

// trace filter that ignores all players except the passed one
class CTraceFilterIgnorePlayersExceptFor : public CTraceFilterSimple
{
public:
	// It does have a base, but we'll never network anything below here..
	DECLARE_CLASS( CTraceFilterIgnorePlayersExceptFor, CTraceFilterSimple );

	CTraceFilterIgnorePlayersExceptFor( const IHandleEntity *passentity, int collisionGroup )
		: CTraceFilterSimple( passentity, collisionGroup )
	{
	}

	virtual bool ShouldHitEntity( IHandleEntity *pServerEntity, int contentsMask )
	{
		CBaseEntity *pEntity = EntityFromEntityHandle( pServerEntity );

		if ( pEntity->IsPlayer() )
		{
			if ( pEntity != GetPassEntity() )
			{
				return false;
			}
			else
				return true;
		}

		return true;
	}
};

#define DEPLOY_DOWNTRACE_FORWARD_DIST	16
#define DEPLOY_DOWNTRACE_OFFSET	16	// yay for magic numbers

bool CDODBipodWeapon::TestDeployAngle( CDODPlayer *pPlayer, float *flDeployedHeight, CBaseEntity **pDeployedOn, QAngle angles )
{
	// make sure we are deployed on the same entity at the same height
	trace_t tr;	

	angles[PITCH] = 0;

	Vector forward, right, up;
	AngleVectors( angles, &forward, &right, &up );

	// start at top of player bbox
	Vector vecStart = pPlayer->GetAbsOrigin();

	float flForwardTraceDist = 32;

	// check us as ducking if we are ducked, or if were ducked when we were deployed
	bool bDucking = pPlayer->m_Shared.IsDucking() || ( IsDeployed() && m_bDuckedWhenDeployed );

	if ( pPlayer->m_Shared.IsProne() )
	{
		vecStart.z += VEC_PRONE_HULL_MAX[2];
		flForwardTraceDist = 16;
	}		
	else if ( bDucking )
	{
		vecStart.z += VEC_DUCK_HULL_MAX[2];
	}
	else
	{
		vecStart.z += 60;
	}

	int dim = 1;	//	dod_deploy_box_size.GetInt();
	Vector vecDeployTraceBoxSize( dim, dim, dim );

	vecStart.z -= vecDeployTraceBoxSize[2];
	vecStart.z -= 4;

	// sandbags are around 50 units high. Shouldn't be able to deploy on anything a lot higher than that

	// optimal standing height ( for animation's sake ) is around 42 units
	// optimal ducking height is around 20 units ( 20 unit high object, plus 8 units of gun )

	// Start one half box width away from the edge of the player hull
	Vector vecForwardStart = vecStart + forward * ( VEC_HULL_MAX_SCALED( pPlayer )[0] + vecDeployTraceBoxSize[0] );

	int traceMask = MASK_SOLID;

	CBaseEntity *pDeployedOnPlayer = NULL;

	if ( m_hDeployedOnEnt && m_hDeployedOnEnt->IsPlayer() )
	{
		pDeployedOnPlayer = m_hDeployedOnEnt.Get();
	}

	CTraceFilterIgnorePlayersExceptFor deployedFilter( pDeployedOnPlayer, COLLISION_GROUP_NONE );
	CTraceFilterSimple undeployedFilter( pPlayer, COLLISION_GROUP_NONE );

	// if we're deployed, skip all players except for the deployed on player
	// if we're not, only skip ourselves
	ITraceFilter *filter;
	if ( IsDeployed() )
		filter = &deployedFilter;
	else
		filter = &undeployedFilter;

	UTIL_TraceHull( vecForwardStart,
		vecForwardStart + forward * ( flForwardTraceDist - 2 * vecDeployTraceBoxSize[0] ),
		-vecDeployTraceBoxSize,
		vecDeployTraceBoxSize,
		traceMask,	
		filter,
		&tr );

#ifndef CLIENT_DLL
	if ( dod_debugmgdeploy.GetBool() )
	{
		NDebugOverlay::Line( vecForwardStart, vecForwardStart + forward * ( flForwardTraceDist - 2 * vecDeployTraceBoxSize[0] ), 0, 0, 255, true, 0.1 );
		NDebugOverlay::Box( vecForwardStart, -vecDeployTraceBoxSize, vecDeployTraceBoxSize, 255, 0, 0, 128, 0.1 );
		NDebugOverlay::Box( tr.endpos, -vecDeployTraceBoxSize, vecDeployTraceBoxSize, 0, 0, 255, 128, 0.1 );
	}
#endif

	// Test forward, are we trying to deploy into a solid object?
	if ( tr.fraction < 1.0 )
	{
		return false;
	}

	// If we're prone, we can always deploy, don't do the ground test
	if ( pPlayer->m_Shared.IsProne() && !pPlayer->m_Shared.IsGettingUpFromProne() )
	{
		// MATTTODO: do trace from *front* of player, not from the edge of crouch hull
		// this is sufficient
		*flDeployedHeight = PRONE_DEPLOY_HEIGHT;
		return true;
	}

	// fix prediction hitch when coming up from prone. client thinks we aren't
	// prone, but hull is still prone hull
	// assumes prone hull is shorter than duck hull!
	if ( pPlayer->WorldAlignMaxs().z <= VEC_PRONE_HULL_MAX.z )
		return false;

	// Else trace down
	Vector vecDownTraceStart = vecStart + forward * ( VEC_HULL_MAX_SCALED( pPlayer )[0] + DEPLOY_DOWNTRACE_FORWARD_DIST );
	int iTraceHeight = -( pPlayer->WorldAlignMaxs().z );


	// search down from the forward trace
	// use the farthest point first. If that fails, move towards the player a few times
	// to see if they are trying to deploy on a thin railing

	bool bFound = false;

	int maxAttempts = 4;
	float flHighestTraceEnd = vecDownTraceStart.z + iTraceHeight;
	CBaseEntity *pBestDeployEnt = NULL;

	while( maxAttempts > 0 )
	{
		UTIL_TraceHull( vecDownTraceStart,
			vecDownTraceStart + Vector(0,0,iTraceHeight),	// trace forward one box width
			-vecDeployTraceBoxSize,
			vecDeployTraceBoxSize,
			traceMask,	
			filter,
			&tr );

#ifndef CLIENT_DLL
		if ( dod_debugmgdeploy.GetBool() )
		{
			NDebugOverlay::Line( vecDownTraceStart, tr.endpos, 255, 0, 0, true, 0.1 );
			NDebugOverlay::Box( vecDownTraceStart, -vecDeployTraceBoxSize, vecDeployTraceBoxSize, 255, 0, 0, 128, 0.1 );
			NDebugOverlay::Box( tr.endpos, -vecDeployTraceBoxSize, vecDeployTraceBoxSize, 0, 0, 255, 128, 0.1 );
		}
#endif

		bool bSuccess = ( tr.fraction < 1.0 ) && !tr.startsolid && !tr.allsolid;

		// if this is the first one found, set found flag
		if ( bSuccess && !bFound )
		{
			bFound = true;
		}
		else if ( bFound == true && bSuccess == false )
		{
			// it failed and we have some data. break here
			break;
		}

		// if this trace is better ( higher ) use this one
		if ( tr.endpos.z > flHighestTraceEnd )
		{
			flHighestTraceEnd = tr.endpos.z;
			pBestDeployEnt = tr.m_pEnt;
		}

		--maxAttempts;

		// move towards the player, looking for a better height to deploy on
		vecDownTraceStart += forward * -4;
	}

	if ( bFound == false || pBestDeployEnt == NULL )
		return false;

	*pDeployedOn = pBestDeployEnt;
	*flDeployedHeight = flHighestTraceEnd - vecDeployTraceBoxSize[0] + DEPLOY_DOWNTRACE_OFFSET - pPlayer->GetAbsOrigin().z;
	return true;
}

Activity CDODBipodWeapon::GetUndeployActivity( void )
{
	return ACT_VM_UNDEPLOY;
}

Activity CDODBipodWeapon::GetDeployActivity( void )
{
	return ACT_VM_DEPLOY;
}


Activity CDODBipodWeapon::GetPrimaryAttackActivity( void )
{
	Activity actPrim;

	if( IsDeployed() )
		actPrim = ACT_VM_PRIMARYATTACK_DEPLOYED;	
	else
		actPrim = ACT_VM_PRIMARYATTACK;

	return actPrim;
}

Activity CDODBipodWeapon::GetReloadActivity( void )
{
	Activity actReload;

	if( IsDeployed() )
		actReload = ACT_VM_RELOAD_DEPLOYED;	
	else
		actReload = ACT_VM_RELOAD;

	return actReload;
}

Activity CDODBipodWeapon::GetIdleActivity( void )
{
	Activity actIdle;

	if( IsDeployed() )
		actIdle = ACT_VM_IDLE_DEPLOYED;	
	else
		actIdle = ACT_VM_IDLE;

	return actIdle;
}


float CDODBipodWeapon::GetWeaponAccuracy( float flPlayerSpeed )
{
	float flSpread = BaseClass::GetWeaponAccuracy( flPlayerSpeed );
	
	if( IsDeployed() )
	{
		flSpread = m_pWeaponInfo->m_flSecondaryAccuracy;
	}

	return flSpread;
}

#ifdef CLIENT_DLL

	int CDODBipodWeapon::GetWorldModelIndex( void )
	{
		if( GetOwner() == NULL )
			return m_iWorldModelIndex;
		else if( m_bUseAltWeaponModel )
			return m_iWorldModelIndex;	//override for hand signals etc
		else
            return m_iCurrentWorldModel;
	}

	void CDODBipodWeapon::CheckForAltWeapon( int iCurrentState )
	{
		int iCriteria = GetDODWpnData().m_iAltWpnCriteria;

		bool bReloading = ( iCurrentState & ALTWPN_CRITERIA_RELOADING );

		if( bReloading )
		{
			if( IsDeployed() && iCurrentState & ALTWPN_CRITERIA_PRONE && 
				iCriteria & ALTWPN_CRITERIA_PRONE_DEPLOYED_RELOAD )
			{
				m_iCurrentWorldModel = m_iProneDeployedReloadModelIndex;		// prone deployed reload	
			}
			else if( IsDeployed() && iCriteria & ALTWPN_CRITERIA_DEPLOYED_RELOAD )
			{
				m_iCurrentWorldModel = m_iDeployedReloadModelIndex;		// deployed reload			
			}
			else if( iCriteria & ALTWPN_CRITERIA_RELOADING )
			{
				m_iCurrentWorldModel = m_iReloadModelIndex;				// left handed reload			
			}
			else 
			{
				m_iCurrentWorldModel = m_iWorldModelIndex;				// normal weapon reload
			}
		}
		else if( IsDeployed() && iCriteria & ALTWPN_CRITERIA_DEPLOYED )
		{
			m_iCurrentWorldModel = m_iDeployedModelIndex;				// bipod down
		}
		else if( (iCurrentState & iCriteria) & ALTWPN_CRITERIA_FIRING )
		{
			// don't think we have any weapons that do this
			m_iCurrentWorldModel = m_iReloadModelIndex;					// left handed shooting?	
		}
		else
		{
			m_iCurrentWorldModel = m_iWorldModelIndex;					// normal weapon
		}
	}

	ConVar deployed_mg_sensitivity( "deployed_mg_sensitivity", "0.9", FCVAR_CHEAT, "Mouse sensitivity while deploying a machine gun" );

	void CDODBipodWeapon::OverrideMouseInput( float *x, float *y )
	{
		if( IsDeployed() )
		{
			float flSensitivity = deployed_mg_sensitivity.GetFloat();

			*x *= flSensitivity;
			*y *= flSensitivity;		
		}
	}

#endif
