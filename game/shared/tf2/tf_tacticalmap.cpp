//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Shared stuff for the Tactical map
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"

// Unfortunate hack.
// Needed to cycle through the player & radar scanner entities in both the client and game dlls
#ifdef CLIENT_DLL

// Client DLL functions
#include "c_team.h"
#include "c_tfteam.h"
#include "c_basetfplayer.h"
#include "C_BaseObject.h"

static inline bool IsPlayerCamoed( int iEntIndex )
{
	C_BaseTFPlayer* pPlayer = (C_BaseTFPlayer*)ClientEntityList().GetClientEntity(iEntIndex);
	if (!pPlayer)
		return false;

	return pPlayer->IsCamouflaged();
}

static inline bool IsPlayerVisible( int iEntIndex )
{
	C_BaseTFPlayer* pPlayer = (C_BaseTFPlayer*)ClientEntityList().GetClientEntity(iEntIndex);
	if (!pPlayer)
		return false;

	return pPlayer->GetClass() != TFCLASS_UNDECIDED;
}

static inline bool IsEntityAnObject( int iEntIndex )
{
	IClientNetworkable *pEnt = ClientEntityList().GetClientEntity(iEntIndex);
	return dynamic_cast<C_BaseObject*>(pEnt) != 0;
}

#else

// Game DLL functions
#include "team.h"
#include "tf_team.h"
#include "tf_player.h"

static inline bool IsPlayerCamoed( int iEntIndex )
{
	CBaseTFPlayer* pPlayer = (CBaseTFPlayer *)CBaseEntity::Instance( engine->PEntityOfEntIndex( iEntIndex ) );
	if (!pPlayer)
		return false;
	return pPlayer->IsCamouflaged();
}

static inline bool IsPlayerVisible( int iEntIndex )
{
	CBaseTFPlayer* pPlayer = (CBaseTFPlayer *)CBaseEntity::Instance( engine->PEntityOfEntIndex( iEntIndex ) );
	if (!pPlayer)
		return false;
	return pPlayer->PlayerClass() != TFCLASS_UNDECIDED;
}

static inline bool IsEntityAnObject( int iEntIndex )
{
	CBaseEntity* pEnt = CBaseEntity::Instance( engine->PEntityOfEntIndex( iEntIndex ) );
	CBaseObject *pObject = dynamic_cast<CBaseObject*>(pEnt);
	if (!pObject)
		return false;

	// Don't bother with boring ones... they're boring!
	return ((pObject->GetObjectFlags( ) & OF_SUPPRESS_VISIBLE_TO_TACTICAL) == 0);
}

#endif

// Visibility defines
#define PLAYER_VISIBILITY_DISTANCE			2000		// Distance around a player that's exposed on the tactical map


//-----------------------------------------------------------------------------
// Purpose: Return true if the entity is visible on this player's tactical map
//-----------------------------------------------------------------------------
bool IsEntityVisibleToTactical( int iLocalTeamNumber, int iLocalTeamPlayers, 
	int iLocalTeamObjects, int entIndex, const char *pEntName, int iEntTeamNumber, const Vector &entOrigin )
{
	// Resource zones are always visible
	if ( !strcmp( pEntName, "trigger_resourcezone") )
		return true;

	// Tunnels are always visible
	if ( !strcmp( pEntName, "obj_tunnel") || !strcmp( pEntName, "obj_tunnel_prop") )
		return true;

	// Fixed shields are never visible
	if ( !strcmp( pEntName, "shield") )
		return false;

	// NOTE: If you're looking for various object types, fix the ugly hack
	// in mapdata.cpp!!
	if ( iLocalTeamNumber == iEntTeamNumber )
	{
		// Objects are always visible to their team
		if (IsEntityAnObject( entIndex ))
			return true;

		// Players are always visible to their team
		if (!Q_strncmp( pEntName, "player", 7) )
			return true;

		// Resource collectors are always visible to their team
		if ( !strcmp( pEntName, "npc_rescollector_aerial") )
			return true;
	}

	return false;
}



