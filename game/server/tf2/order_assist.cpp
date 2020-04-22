//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "order_assist.h"
#include "tf_team.h"
#include "order_helpers.h"


// If a player has been shot within this time delta, commandos will get orders to assist.
#define COMMANDO_ASSIST_SHOT_DELAY	3.0f

// How close a commando has to be to a teammate to get an assist order.
#define COMMAND_ASSIST_DISTANCE		1200
#define COMMAND_ASSIST_DISTANCE_SQR	(COMMAND_ASSIST_DISTANCE*COMMAND_ASSIST_DISTANCE)



IMPLEMENT_SERVERCLASS_ST( COrderAssist, DT_OrderAssist )
END_SEND_TABLE()


static bool IsValidFn_OnEnemyTeam( void *pUserData, int a )
{
	edict_t *pEdict = engine->PEntityOfEntIndex( a+1 );
	if ( !pEdict )
		return false;

	CBaseEntity *pBaseEntity = CBaseEntity::Instance( pEdict );
	if ( !pBaseEntity )
		return false;

	CSortBase *p = (CSortBase*)pUserData;
	return pBaseEntity->GetTeam() != p->m_pPlayer->GetTeam();
}


static bool IsValidFn_PlayersWantingAssist( void *pUserData, int a )
{
	CSortBase *pSortBase = (CSortBase*)pUserData;
	CBaseTFPlayer *pPlayer = (CBaseTFPlayer*)pSortBase->m_pPlayer->GetTeam()->GetPlayer( a );

	if ( !pPlayer->IsAlive() )
	{
		// This guy sure could have used an assist but YOU'RE TOO SLOW!!!
		return false;
	}

	// Don't try to assist yourself...
	if ( pPlayer == pSortBase->m_pPlayer )
		return false;

	// Make sure this guy was shot recently.
	if ( (gpGlobals->curtime - pPlayer->LastTimeDamagedByEnemy()) > COMMANDO_ASSIST_SHOT_DELAY )
		return false;

	// Is the guy close enough?
	if ( pSortBase->m_pPlayer->GetAbsOrigin().DistToSqr( pPlayer->GetAbsOrigin() ) > COMMAND_ASSIST_DISTANCE_SQR )
		return false;

	return true;
}


bool COrderAssist::CreateOrder( CPlayerClass *pClass )
{
	// Search for a (live) nearby player who's just been shot.
	CSortBase info;
	info.m_pPlayer = pClass->GetPlayer();

	int sorted[512];
	int nSorted = BuildSortedActiveList(
		sorted,
		ARRAYSIZE( sorted ),
		SortFn_TeamPlayersByDistance,
		IsValidFn_PlayersWantingAssist,
		&info,
		pClass->GetTeam()->GetNumPlayers()
		);

	if ( nSorted )
	{
		COrderAssist *pOrder = new COrderAssist;

		CBaseTFPlayer *pPlayerToAssist = (CBaseTFPlayer*)pClass->GetTeam()->GetPlayer( sorted[0] );

		pClass->GetTeam()->AddOrder( 
			ORDER_ASSIST, 
			pPlayerToAssist, 
			info.m_pPlayer, 
			COMMAND_ASSIST_DISTANCE,
			25,
			pOrder );

		// Add the closest enemies.
		CSortBase enemySortInfo;
		enemySortInfo.m_pPlayer = pPlayerToAssist;

		int sortedEnemies[256];
		int nSortedEnemies = BuildSortedActiveList(	
			sortedEnemies,
			ARRAYSIZE( sortedEnemies ),
			SortFn_PlayerEntitiesByDistance,
			IsValidFn_OnEnemyTeam,
			&info,
			gpGlobals->maxClients
			);

		nSortedEnemies = MIN( nSortedEnemies, NUM_ASSIST_ENEMIES );
		for ( int i=0; i < nSortedEnemies; i++ )
		{
			CBaseEntity *pEnt = CBaseEntity::Instance( engine->PEntityOfEntIndex( sortedEnemies[i] + 1 ) );
			Assert( dynamic_cast<CBasePlayer*>( pEnt ) );
			pOrder->m_Enemies[i] = pEnt;
		}
	}

	return false;
}


bool COrderAssist::Update()
{
	if ( !GetTargetEntity() || !GetTargetEntity()->IsAlive() )
		return true;

	return BaseClass::Update();
}


bool COrderAssist::UpdateOnEvent( COrderEvent_Base *pEvent )
{
	if ( !GetTargetEntity() )
		return true;

	switch( pEvent->GetType() )
	{
		// If our boy dies, then he doesn't care about assistance anymore.
		case ORDER_EVENT_PLAYER_KILLED:
		{
			COrderEvent_PlayerKilled *pKilled = (COrderEvent_PlayerKilled*)pEvent;
			if ( pKilled->m_pPlayer == GetTargetEntity() )
				return true;
		}
		break;

		// Did we damage one of the enemies?
		case ORDER_EVENT_PLAYER_DAMAGED:
		{
			COrderEvent_PlayerDamaged *pPlayerDamaged = (COrderEvent_PlayerDamaged*)pEvent;
			if ( pPlayerDamaged->m_TakeDamageInfo.GetInflictor() == GetOwner() )
			{
				for ( int i=0; i < NUM_ASSIST_ENEMIES; i++ )
				{
					if ( pPlayerDamaged->m_pPlayerDamaged == m_Enemies[i].Get() )
					{
						// Reset the timer on the guy we're defending, in case we killed his
						// attacker really quickly.
//						CBaseTFPlayer *pPlayer = (CBaseTFPlayer*)GetTargetEntity();
//						pPlayer->m_flLastTimeDamagedByEnemy = 0;

						return true;
					}
				}
			}
		}
		break;
	}

	return BaseClass::UpdateOnEvent( pEvent );
}


