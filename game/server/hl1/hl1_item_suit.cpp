//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//
/*
===== item_suit.cpp ========================================================

  handling for the player's suit.
*/

#include "cbase.h"
#include "player.h"
#include "gamerules.h"
#include "items.h"
#include "hl1_items.h"


#define SF_SUIT_SHORTLOGON		0x0001

#define SUIT_MODEL "models/w_suit.mdl"

extern int gEvilImpulse101;

class CItemSuit : public CHL1Item
{
public:
	DECLARE_CLASS( CItemSuit, CHL1Item );

	void Spawn( void )
	{ 
		Precache( );
		SetModel( SUIT_MODEL );
		BaseClass::Spawn( );

		CollisionProp()->UseTriggerBounds( true, 12.0f );
	}
	void Precache( void )
	{
		PrecacheModel( SUIT_MODEL );
	}
	bool MyTouch( CBasePlayer *pPlayer )
	{
		if ( pPlayer->IsSuitEquipped() )
			return false;

		if( !gEvilImpulse101 )
		{
			if ( HasSpawnFlags( SF_SUIT_SHORTLOGON ) )
				UTIL_EmitSoundSuit(pPlayer->edict(), "!HEV_A0");		// short version of suit logon,
			else
				UTIL_EmitSoundSuit(pPlayer->edict(), "!HEV_AAx");	// long version of suit logon
		}

		pPlayer->EquipSuit();
		return true;
	}
};

LINK_ENTITY_TO_CLASS(item_suit, CItemSuit);
PRECACHE_REGISTER(item_suit);
