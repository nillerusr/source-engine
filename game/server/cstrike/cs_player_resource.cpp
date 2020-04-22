//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: CS's custom CPlayerResource
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "cs_player.h"
#include "player_resource.h"
#include "cs_simple_hostage.h"
#include "cs_player_resource.h"
#include "weapon_c4.h"
#include <coordsize.h>
#include "cs_bot_manager.h"
#include "cs_gamerules.h"

// Datatable
IMPLEMENT_SERVERCLASS_ST(CCSPlayerResource, DT_CSPlayerResource)
	SendPropInt( SENDINFO( m_iPlayerC4 ), 8, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iPlayerVIP ), 8, SPROP_UNSIGNED ),
	SendPropVector( SENDINFO(m_vecC4), -1, SPROP_COORD),
	SendPropArray3( SENDINFO_ARRAY3(m_bHostageAlive), SendPropInt( SENDINFO_ARRAY(m_bHostageAlive), 1, SPROP_UNSIGNED ) ),
	SendPropArray3( SENDINFO_ARRAY3(m_isHostageFollowingSomeone), SendPropInt( SENDINFO_ARRAY(m_isHostageFollowingSomeone), 1, SPROP_UNSIGNED ) ),
	SendPropArray3( SENDINFO_ARRAY3(m_iHostageEntityIDs), SendPropInt( SENDINFO_ARRAY(m_iHostageEntityIDs), -1, SPROP_UNSIGNED ) ),
	SendPropArray3( SENDINFO_ARRAY3(m_iHostageY), SendPropInt( SENDINFO_ARRAY(m_iHostageY), COORD_INTEGER_BITS+1, 0 ) ),
	SendPropArray3( SENDINFO_ARRAY3(m_iHostageX), SendPropInt( SENDINFO_ARRAY(m_iHostageX), COORD_INTEGER_BITS+1, 0 ) ),
	SendPropArray3( SENDINFO_ARRAY3(m_iHostageZ), SendPropInt( SENDINFO_ARRAY(m_iHostageZ), COORD_INTEGER_BITS+1, 0 ) ),
	SendPropVector( SENDINFO(m_bombsiteCenterA), -1, SPROP_COORD),
	SendPropVector( SENDINFO(m_bombsiteCenterB), -1, SPROP_COORD),
	SendPropArray3( SENDINFO_ARRAY3(m_hostageRescueX), SendPropInt( SENDINFO_ARRAY(m_hostageRescueX), COORD_INTEGER_BITS+1, 0 ) ),
	SendPropArray3( SENDINFO_ARRAY3(m_hostageRescueY), SendPropInt( SENDINFO_ARRAY(m_hostageRescueY), COORD_INTEGER_BITS+1, 0 ) ),
	SendPropArray3( SENDINFO_ARRAY3(m_hostageRescueZ), SendPropInt( SENDINFO_ARRAY(m_hostageRescueZ), COORD_INTEGER_BITS+1, 0 ) ),
	SendPropBool( SENDINFO( m_bBombSpotted ) ),
	SendPropArray3( SENDINFO_ARRAY3(m_bPlayerSpotted), SendPropInt( SENDINFO_ARRAY(m_bPlayerSpotted), 1, SPROP_UNSIGNED ) ),
	SendPropArray3( SENDINFO_ARRAY3(m_iMVPs), SendPropInt( SENDINFO_ARRAY(m_iMVPs), COORD_INTEGER_BITS+1, SPROP_UNSIGNED ) ),
	SendPropArray3( SENDINFO_ARRAY3(m_bHasDefuser), SendPropInt( SENDINFO_ARRAY(m_bHasDefuser), 1, SPROP_UNSIGNED ) ),
	SendPropArray3( SENDINFO_ARRAY3(m_szClan), SendPropStringT( SENDINFO_ARRAY(m_szClan) ) ),
END_SEND_TABLE()
//=============================================================================
// HPE_END
//=============================================================================

BEGIN_DATADESC( CCSPlayerResource )
	// DEFINE_ARRAY( m_iPing, FIELD_INTEGER, MAX_PLAYERS+1 ),
	// DEFINE_ARRAY( m_iPacketloss, FIELD_INTEGER, MAX_PLAYERS+1 ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( cs_player_manager, CCSPlayerResource );

CCSPlayerResource::CCSPlayerResource( void )
{
	
}


//--------------------------------------------------------------------------------------------------------
class Spotter
{
public:
	Spotter( CBaseEntity *entity, const Vector &target, int spottingTeam )
	{
		m_targetEntity = entity;
		m_target = target;
		m_team = spottingTeam;
		m_spotted = false;
	}

	bool operator()( CBasePlayer *player )
	{
		if ( !player->IsAlive() || player->GetTeamNumber() != m_team )
			return true;

		CCSPlayer *csPlayer = ToCSPlayer( player );
		if ( !csPlayer )
			return true;

		if ( csPlayer->IsBlind() )
			return true;

		Vector eye, forward;
		player->EyePositionAndVectors( &eye, &forward, NULL, NULL );
		Vector path( m_target - eye );
		float distance = path.Length();
		path.NormalizeInPlace();
		float dot = DotProduct( forward, path );
		if( (dot > 0.995f ) 
			|| (dot > 0.98f && distance < 900) 
			|| (dot > 0.8f && distance < 250) 
			)
		{
			trace_t tr;
			CTraceFilterSkipTwoEntities filter( player, m_targetEntity, COLLISION_GROUP_DEBRIS );
			UTIL_TraceLine( eye, m_target,
				(CONTENTS_OPAQUE|CONTENTS_SOLID|CONTENTS_MOVEABLE|CONTENTS_DEBRIS), &filter, &tr );

			if( tr.fraction == 1.0f )
			{
				if ( TheCSBots()->IsLineBlockedBySmoke( eye, m_target ) )
				{
					return true;
				}

				m_spotted = true;
				return false; // spotted already, so no reason to check for other players spotting the same thing.
			}
		}

		return true;
	}

	bool Spotted( void ) const
	{
		return m_spotted;
	}

private:
	CBaseEntity *m_targetEntity;
	Vector m_target;
	int m_team;
	bool m_spotted;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSPlayerResource::UpdatePlayerData( void )
{
	int i;

	m_iPlayerC4 = 0;
	m_iPlayerVIP = 0;

	for ( i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CCSPlayer *pPlayer = (CCSPlayer*)UTIL_PlayerByIndex( i );
		
		if ( pPlayer && pPlayer->IsConnected() )
		{
			if ( pPlayer->IsVIP() )
			{
				// we should only have one VIP
				Assert( m_iPlayerVIP == 0 );
				m_iPlayerVIP = i;
			}

			if ( pPlayer->HasC4() )
			{
				// we should only have one bomb
				m_iPlayerC4 = i;
			}

			m_szClan.Set(i, AllocPooledString( pPlayer->GetClanTag() ) );

			m_iMVPs.Set(i, pPlayer->GetNumMVPs());

			m_bHasDefuser.Set(i, pPlayer->HasDefuser());

		}
		else
		{
			m_szClan.Set( i, MAKE_STRING( "" ) );
			m_iMVPs.Set( i, 0 );
		}
	}

	CBaseEntity *c4 = NULL;
	if ( m_iPlayerC4 == 0 )
	{
		// no player has C4, update C4 position
		if ( g_C4s.Count() > 0 )
		{
			c4 = g_C4s[0];
			m_vecC4 = c4->GetAbsOrigin();
		}
		else
		{
			m_vecC4.Init();
		}
	}

	int numHostages = g_Hostages.Count();

	for ( i = 0; i < MAX_HOSTAGES; i++ )
	{
		if ( i >= numHostages )
		{
//			engine->Con_NPrintf( i, "Dead" );
			m_bHostageAlive.Set( i, false );
			m_isHostageFollowingSomeone.Set( i, false );
			continue;
		}

		CHostage* pHostage = g_Hostages[i];

		m_bHostageAlive.Set( i, pHostage->IsRescuable() );

		if ( pHostage->IsValid() )
		{
			m_iHostageX.Set( i, (int) pHostage->GetAbsOrigin().x );	
			m_iHostageY.Set( i, (int) pHostage->GetAbsOrigin().y );	
			m_iHostageZ.Set( i, (int) pHostage->GetAbsOrigin().z );	
			m_iHostageEntityIDs.Set( i, pHostage->entindex() );
			m_isHostageFollowingSomeone.Set( i, pHostage->IsFollowingSomeone() );
//			engine->Con_NPrintf( i, "ID:%d Pos:(%.0f,%.0f,%.0f)", pHostage->entindex(), pHostage->GetAbsOrigin().x, pHostage->GetAbsOrigin().y, pHostage->GetAbsOrigin().z );
		}
		else
		{
//			engine->Con_NPrintf( i, "Invalid" );
		}
	}

	if( !m_foundGoalPositions )
	{
		// We only need to update these once a map, but we need the client to know about them.
		CBaseEntity* ent = NULL;
		while ( ( ent = gEntList.FindEntityByClassname( ent, "func_bomb_target" ) ) != NULL )
		{
			const Vector &pos = ent->WorldSpaceCenter();
			CNavArea *area = TheNavMesh->GetNearestNavArea( pos, true, 10000.0f, false, false );
			const char *placeName = (area) ? TheNavMesh->PlaceToName( area->GetPlace() ) : NULL;
			if ( placeName == NULL )
			{
				// The bomb site has no area or place name, so just choose A then B
				if ( m_bombsiteCenterA.Get().IsZero() )
				{
					m_bombsiteCenterA = pos;
				}
				else
				{
					m_bombsiteCenterB = pos;
				}
			}
			else
			{
				// The bomb site has a place name, so choose accordingly
				if( FStrEq( placeName, "BombsiteA" ) )
				{
					m_bombsiteCenterA = pos;
				}
				else
				{
					m_bombsiteCenterB = pos;
				}
			}
			m_foundGoalPositions = true;
		}

		int hostageRescue = 0;
		while ( (( ent = gEntList.FindEntityByClassname( ent, "func_hostage_rescue" ) ) != NULL)  &&  (hostageRescue < MAX_HOSTAGE_RESCUES) )
		{
			const Vector &pos = ent->WorldSpaceCenter();
			m_hostageRescueX.Set( hostageRescue, (int) pos.x );	
			m_hostageRescueY.Set( hostageRescue, (int) pos.y );	
			m_hostageRescueZ.Set( hostageRescue, (int) pos.z );	

			hostageRescue++;
			m_foundGoalPositions = true;
		}
	}

	bool bombSpotted = false;
	if ( c4 )
	{
		Spotter spotter( c4, m_vecC4, TEAM_CT );
		ForEachPlayer( spotter );
		if ( spotter.Spotted() )
		{
			bombSpotted = true;
		}
	}

	for ( int i=0; i < MAX_PLAYERS+1; i++ )
	{
		CCSPlayer *target = ToCSPlayer( UTIL_PlayerByIndex( i ) );

		if ( !target || !target->IsAlive() )
		{
			m_bPlayerSpotted.Set( i, 0 );
			continue;
		}

		Spotter spotter( target, target->EyePosition(), (target->GetTeamNumber()==TEAM_CT) ? TEAM_TERRORIST : TEAM_CT );
		ForEachPlayer( spotter );
		if ( spotter.Spotted() )
		{
			if ( target->HasC4() )
			{
				bombSpotted = true;
			}
			m_bPlayerSpotted.Set( i, 1 );
		}
		else
		{
			m_bPlayerSpotted.Set( i, 0 );
		}
	}

	if ( bombSpotted )
	{
		m_bBombSpotted = true;
	}
	else
	{
		m_bBombSpotted = false;
	}

	BaseClass::UpdatePlayerData();
}

void CCSPlayerResource::Spawn( void )
{
	m_vecC4.Init();
	m_iPlayerC4 = 0;
	m_iPlayerVIP = 0;
	m_bombsiteCenterA.Init();
	m_bombsiteCenterB.Init();
	m_foundGoalPositions = false;

	for ( int i=0; i < MAX_HOSTAGES; i++ )
	{
		m_bHostageAlive.Set( i, 0 );
		m_isHostageFollowingSomeone.Set( i, 0 );
		m_iHostageEntityIDs.Set(i, 0);
	}

	for ( int i=0; i < MAX_HOSTAGE_RESCUES; i++ )
	{
		m_hostageRescueX.Set( i, 0 );
		m_hostageRescueY.Set( i, 0 );
		m_hostageRescueZ.Set( i, 0 );
	}

	m_bBombSpotted = false;
	for ( int i=0; i < MAX_PLAYERS+1; i++ )
	{
		m_bPlayerSpotted.Set( i, 0 );
		m_szClan.Set( i, MAKE_STRING( "" ) );
		m_iMVPs.Set( i, 0 );
		m_bHasDefuser.Set(i, false);
	}

	BaseClass::Spawn();
}
