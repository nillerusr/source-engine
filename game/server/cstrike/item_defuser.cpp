//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Defuser kit that drops from counter-strike CTS 
//
//=============================================================================//

#include "cbase.h"
#include "items.h"
#include "cs_player.h"

class CItemDefuser : public CItem
{
public:
	DECLARE_CLASS( CItemDefuser, CItem );
	
	void	Spawn( void );
	void	Precache( void );
	void	DefuserTouch( CBaseEntity *pOther );
	void	ActivateThink( void );
	
	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS( item_defuser, CItemDefuser );
PRECACHE_REGISTER(item_defuser);


BEGIN_DATADESC( CItemDefuser )

	//Functions
	DEFINE_THINKFUNC( ActivateThink ),
	DEFINE_ENTITYFUNC( DefuserTouch ),

END_DATADESC()


void CItemDefuser::Spawn( void )
{ 
	Precache( );
	SetModel( "models/weapons/w_defuser.mdl" );
	BaseClass::Spawn();

	SetNextThink( gpGlobals->curtime + 0.5f );
	SetThink( &CItemDefuser::ActivateThink );

	SetTouch( NULL );
}
	
void CItemDefuser::Precache( void )
{
	PrecacheModel( "models/weapons/w_defuser.mdl" );

	PrecacheScriptSound( "BaseCombatCharacter.ItemPickup2" );
}

void CItemDefuser::ActivateThink( void )
{
	//since we can't stop the item from being touched while its in the air,
	//activate 1 second after being dropped

	SetTouch( &CItemDefuser::DefuserTouch );
	SetThink( NULL );
}
	
void CItemDefuser::DefuserTouch( CBaseEntity *pOther )
{
	if ( !pOther->IsPlayer() )
	{
		return;
	}

	//if( GetFlags() & FL_ONGROUND )
	{
		CCSPlayer *pPlayer = (CCSPlayer *)pOther;

		if ( !pPlayer )
		{
			Assert( false );
			return;
		}

		if( pPlayer->GetTeamNumber() == TEAM_CT && !pPlayer->HasDefuser() )
		{
            //=============================================================================
            // HPE_BEGIN:
            // [dwenger] Added for fun-fact support
            //=============================================================================

			pPlayer->GiveDefuser( true );

            //=============================================================================
            // HPE_END
            //=============================================================================

			if ( pPlayer->IsDead() == false )
			{
				CPASAttenuationFilter filter( pPlayer );
				EmitSound( filter, entindex(), "BaseCombatCharacter.ItemPickup2" );
			}

			UTIL_Remove( this );
			return;
		}	
	}
}


