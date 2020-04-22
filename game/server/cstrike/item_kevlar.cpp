//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "cbase.h"
#include "items.h"
#include "cs_player.h"


class CItemKevlar : public CItem
{
public:

	DECLARE_CLASS( CItemKevlar, CItem );

	void Spawn( void )
	{ 
		Precache( );
		BaseClass::Spawn( );
	}
	
	void Precache( void )
	{
		PrecacheScriptSound( "BaseCombatCharacter.ItemPickup2" );
	}
	
	bool MyTouch( CBasePlayer *pBasePlayer )
	{
		CCSPlayer *pPlayer = dynamic_cast< CCSPlayer* >( pBasePlayer );
		if ( !pPlayer )
		{
			Assert( false );
			return false;
		}
		
		pPlayer->SetArmorValue( 100 );

		if ( pPlayer->IsDead() == false )
		{
			CPASAttenuationFilter filter( pBasePlayer );
			EmitSound( filter, entindex(), "BaseCombatCharacter.ItemPickup2" );

			CSingleUserRecipientFilter user( pPlayer );
			UserMessageBegin( user, "ItemPickup" );
			WRITE_STRING( "item_kevlar" );
			MessageEnd();
		}

		return true;
	}
};

LINK_ENTITY_TO_CLASS( item_kevlar, CItemKevlar );


