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
#include "weapon_csbase.h"
#include "cs_ammodef.h"


//-----------------------------------------------------------------------------
class CItemAmmo : public CItem
{
public:
	DECLARE_CLASS( CItemAmmo, CItem );

	bool MyTouch( CBasePlayer *pBasePlayer )
	{
		CCSPlayer *pPlayer = dynamic_cast< CCSPlayer* >( pBasePlayer );
		if ( !pPlayer )
		{
			Assert( false );
			return false;
		}

		int ammoIndex = GetCSAmmoDef()->Index( GetAmmoName() );
		if ( ammoIndex < 0 )
		{
			Assert( false );
			return false;
		}

		pPlayer->GiveAmmo( GetCSAmmoDef()->GetBuySize( ammoIndex ), ammoIndex );

		return true;
	}

	virtual const char * GetAmmoName( void ) const { return NULL; }
};

//-----------------------------------------------------------------------------
class CItemAmmo50AE : public CItemAmmo
{
public:
	DECLARE_CLASS( CItemAmmo50AE, CItemAmmo );
	virtual const char * GetAmmoName( void ) const { return BULLET_PLAYER_50AE; }
};

LINK_ENTITY_TO_CLASS( ammo_50ae, CItemAmmo50AE );

//-----------------------------------------------------------------------------
class CItemAmmo762MM : public CItemAmmo
{
public:
	DECLARE_CLASS( CItemAmmo762MM, CItemAmmo );
	virtual const char * GetAmmoName( void ) const { return BULLET_PLAYER_762MM; }
};

LINK_ENTITY_TO_CLASS( ammo_762mm, CItemAmmo762MM );

//-----------------------------------------------------------------------------
class CItemAmmo556MM : public CItemAmmo
{
public:
	DECLARE_CLASS( CItemAmmo556MM, CItemAmmo );
	virtual const char * GetAmmoName( void ) const { return BULLET_PLAYER_556MM; }
};

LINK_ENTITY_TO_CLASS( ammo_556mm, CItemAmmo556MM );

//-----------------------------------------------------------------------------
class CItemAmmo556MM_BOX : public CItemAmmo
{
public:
	DECLARE_CLASS( CItemAmmo556MM_BOX, CItemAmmo );
	virtual const char * GetAmmoName( void ) const { return BULLET_PLAYER_556MM_BOX; }
};

LINK_ENTITY_TO_CLASS( ammo_556mm_box, CItemAmmo556MM_BOX );

//-----------------------------------------------------------------------------
class CItemAmmo338MAG : public CItemAmmo
{
public:
	DECLARE_CLASS( CItemAmmo338MAG, CItemAmmo );
	virtual const char * GetAmmoName( void ) const { return BULLET_PLAYER_338MAG; }
};

LINK_ENTITY_TO_CLASS( ammo_338mag, CItemAmmo338MAG );

//-----------------------------------------------------------------------------
class CItemAmmo9MM : public CItemAmmo
{
public:
	DECLARE_CLASS( CItemAmmo9MM, CItemAmmo );
	virtual const char * GetAmmoName( void ) const { return BULLET_PLAYER_9MM; }
};

LINK_ENTITY_TO_CLASS( ammo_9mm, CItemAmmo9MM );

//-----------------------------------------------------------------------------
class CItemAmmoBuckshot : public CItemAmmo
{
public:
	DECLARE_CLASS( CItemAmmoBuckshot, CItemAmmo );
	virtual const char * GetAmmoName( void ) const { return BULLET_PLAYER_BUCKSHOT; }
};

LINK_ENTITY_TO_CLASS( ammo_buckshot, CItemAmmoBuckshot );

//-----------------------------------------------------------------------------
class CItemAmmo45ACP : public CItemAmmo
{
public:
	DECLARE_CLASS( CItemAmmo45ACP, CItemAmmo );
	virtual const char * GetAmmoName( void ) const { return BULLET_PLAYER_45ACP; }
};

LINK_ENTITY_TO_CLASS( ammo_45acp, CItemAmmo45ACP );

//-----------------------------------------------------------------------------
class CItemAmmo357SIG : public CItemAmmo
{
public:
	DECLARE_CLASS( CItemAmmo357SIG, CItemAmmo );
	virtual const char * GetAmmoName( void ) const { return BULLET_PLAYER_357SIG; }
};

LINK_ENTITY_TO_CLASS( ammo_357sig, CItemAmmo357SIG );

//-----------------------------------------------------------------------------
class CItemAmmo57MM : public CItemAmmo
{
public:
	DECLARE_CLASS( CItemAmmo57MM, CItemAmmo );
	virtual const char * GetAmmoName( void ) const { return BULLET_PLAYER_57MM; }
};

LINK_ENTITY_TO_CLASS( ammo_57mm, CItemAmmo57MM );

//-----------------------------------------------------------------------------

