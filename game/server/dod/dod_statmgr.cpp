//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Handle stats game events and route them to the appropriate place
//
// $NoKeywords: $
//
//=============================================================================//

#include "cbase.h"
#include "KeyValues.h"
#include "dod_statmgr.h"
#include "dod_player.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CDODStatManager::CDODStatManager()
{
}

bool CDODStatManager::Init()
{
	ListenForGameEvent( "player_death" );
	ListenForGameEvent( "dod_stats_weapon_attack" );
	ListenForGameEvent( "dod_stats_player_damage" );
	ListenForGameEvent( "dod_stats_player_killed" );

	return BaseClass::Init();
}

void CDODStatManager::FireGameEvent( IGameEvent *event )
{
	const char *eventName = event->GetName();

	if ( FStrEq( eventName, "dod_stats_weapon_attack" ) )
	{
		CDODPlayer *pAttacker = ToDODPlayer( UTIL_PlayerByUserId( event->GetInt("attacker") ) );

		Assert( pAttacker );

		if ( pAttacker )
			pAttacker->Stats_WeaponFired( event->GetInt("weapon") );
	}
	else if ( FStrEq( eventName, "dod_stats_player_damage" ) )
	{
		CDODPlayer *pAttacker = ToDODPlayer( UTIL_PlayerByUserId( event->GetInt("attacker") ) );

		int iVictimID = event->GetInt("victim");
		CDODPlayer *pVictim = ToDODPlayer( UTIL_PlayerByUserId( iVictimID ) );

		// discard damage to teammates or to yourself
		if ( ( pAttacker == NULL ) || ( pVictim->GetTeamNumber() == pAttacker->GetTeamNumber() ) )
			return;

		int weaponID = event->GetInt("weapon");
		int iDamage = event->GetInt("damage");
		int iDamageGiven = event->GetInt("damage_given");
		float flDistance = event->GetFloat("distance");
		int hitgroup = event->GetInt("hitgroup");

		pAttacker->Stats_WeaponHit( pVictim, weaponID, iDamage, iDamageGiven, hitgroup, flDistance );
		pVictim->Stats_HitByWeapon( pAttacker, weaponID, iDamage, iDamageGiven, hitgroup );
	}
	else if ( FStrEq( eventName, "dod_stats_player_killed" ) )
	{
		int iVictimID = event->GetInt("victim");
		CDODPlayer *pVictim = ToDODPlayer( UTIL_PlayerByUserId( iVictimID ) );

		CDODPlayer *pAttacker = ToDODPlayer( UTIL_PlayerByUserId( event->GetInt("attacker") ) );

		// discard kills to teammates or to yourself
		if ( ( pAttacker == NULL ) || ( pVictim->GetTeamNumber() == pAttacker->GetTeamNumber() ) )
			return;

		int weaponID = event->GetInt("weapon");

		pAttacker->Stats_KilledPlayer( pVictim, weaponID );
		pVictim->Stats_KilledByPlayer( pAttacker, weaponID );
	}
}

CDODStatManager g_DODStatMgr;
