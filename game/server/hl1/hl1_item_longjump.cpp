//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//


#include "cbase.h"
#include "player.h"
#include "gamerules.h"
#include "items.h"
#include "hl1_items.h"
#include "hl1_player.h"


class CItemLongJump : public CHL1Item
{
public:
	DECLARE_CLASS( CItemLongJump, CHL1Item );

	void Spawn( void )
	{ 
		Precache( );
		SetModel( "models/w_longjump.mdl" );
		BaseClass::Spawn( );

		CollisionProp()->UseTriggerBounds( true, 16.0f );
	}
	void Precache( void )
	{
		PrecacheModel ("models/w_longjump.mdl");
	}
	bool MyTouch( CBasePlayer *pPlayer )
	{
		CHL1_Player *pHL1Player = (CHL1_Player*)pPlayer;

		if ( pHL1Player->m_bHasLongJump == true )
		{
			return false;
		}

		if ( pHL1Player->IsSuitEquipped() )
		{
			pHL1Player->m_bHasLongJump = true;// player now has longjump module

			CSingleUserRecipientFilter user( pHL1Player );
			user.MakeReliable();

			UserMessageBegin( user, "ItemPickup" );
				WRITE_STRING( STRING(m_iClassname) );
			MessageEnd();

			UTIL_EmitSoundSuit( pHL1Player->edict(), "!HEV_A1" );	// Play the longjump sound UNDONE: Kelly? correct sound?
			return true;		
		}
		return false;
	}
};

LINK_ENTITY_TO_CLASS( item_longjump, CItemLongJump );
PRECACHE_REGISTER(item_longjump);
