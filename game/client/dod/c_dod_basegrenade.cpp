//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "c_dod_basegrenade.h"


#include "c_dod_player.h"
#include "dodoverview.h"

IMPLEMENT_NETWORKCLASS_ALIASED( DODBaseGrenade, DT_DODBaseGrenade )

BEGIN_NETWORK_TABLE(C_DODBaseGrenade, DT_DODBaseGrenade )
	RecvPropVector( RECVINFO( m_vInitialVelocity ) )
END_NETWORK_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_DODBaseGrenade::~C_DODBaseGrenade()
{
	GetDODOverview()->RemoveGrenade( this );
	ParticleProp()->StopEmission();
}

void C_DODBaseGrenade::PostDataUpdate( DataUpdateType_t type )
{
	BaseClass::PostDataUpdate( type );

	if ( type == DATA_UPDATE_CREATED )
	{
		// Now stick our initial velocity into the interpolation history 
		CInterpolatedVar< Vector > &interpolator = GetOriginInterpolator();

		interpolator.ClearHistory();
		float changeTime = GetLastChangeTime( LATCH_SIMULATION_VAR );

		// Add a sample 1 second back.
		Vector vCurOrigin = GetLocalOrigin() - m_vInitialVelocity;
		interpolator.AddToHead( changeTime - 1.0, &vCurOrigin, false );

		// Add the current sample.
		vCurOrigin = GetLocalOrigin();
		interpolator.AddToHead( changeTime, &vCurOrigin, false );

		// BUG ? this may call multiple times
		GetDODOverview()->AddGrenade( this );

		const char *pszParticleTrail = GetParticleTrailName();
		if ( pszParticleTrail )
		{
			ParticleProp()->Create( pszParticleTrail, PATTACH_ABSORIGIN_FOLLOW );
		}
	}
}

int C_DODBaseGrenade::DrawModel( int flags )
{
	if( m_flSpawnTime + 0.15 > gpGlobals->curtime )
		return 0;

	C_DODPlayer *pPlayer = C_DODPlayer::GetLocalDODPlayer();

	if ( pPlayer && GetAbsVelocity().Length() < 30 )
	{
		pPlayer->CheckGrenadeHint( GetAbsOrigin() );
	}

	return BaseClass::DrawModel( flags );
}

void C_DODBaseGrenade::Spawn()
{
	m_flSpawnTime = gpGlobals->curtime;
	BaseClass::Spawn();
}

const char *C_DODBaseGrenade::GetOverviewSpriteName( void )
{
	const char *pszSprite = "";

	switch( GetTeamNumber() )
	{
	case TEAM_ALLIES:
		pszSprite = "sprites/minimap_icons/grenade_hltv";
		break;
	case TEAM_AXIS:
		pszSprite = "sprites/minimap_icons/stick_hltv";
		break;
	default:
		break;
	}

	return pszSprite;
}

IMPLEMENT_NETWORKCLASS_ALIASED( DODRifleGrenadeUS, DT_DODRifleGrenadeUS )

BEGIN_NETWORK_TABLE(C_DODRifleGrenadeUS, DT_DODRifleGrenadeUS )
END_NETWORK_TABLE()


IMPLEMENT_CLIENTCLASS_DT(C_DODRifleGrenadeGER, DT_DODRifleGrenadeGER, CDODRifleGrenadeGER)
END_RECV_TABLE()