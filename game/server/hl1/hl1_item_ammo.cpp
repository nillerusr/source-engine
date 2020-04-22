//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Ammo boxes for HL1
//
//=============================================================================//


#include "cbase.h"
#include "player.h"
#include "gamerules.h"
#include "items.h"
#include "hl1_items.h"


// ========================================================================
//	>> Crossbow bolts
// ========================================================================
#define AMMO_CROSSBOW_GIVE	5
#define AMMO_CROSSBOW_MODEL	"models/w_crossbow_clip.mdl"

class CCrossbowAmmo : public CHL1Item
{
public:
	DECLARE_CLASS( CCrossbowAmmo, CHL1Item );

	void Spawn( void )
	{ 
		Precache();
		SetModel( AMMO_CROSSBOW_MODEL );
		BaseClass::Spawn();
	}
	void Precache( void )
	{
		PrecacheModel( AMMO_CROSSBOW_MODEL );
	}
	bool MyTouch( CBasePlayer *pPlayer )
	{
		if (pPlayer->GiveAmmo( AMMO_CROSSBOW_GIVE, "XBowBolt" ) )
		{
			if ( g_pGameRules->ItemShouldRespawn( this ) == GR_ITEM_RESPAWN_NO )
			{
				UTIL_Remove( this );
			}
			return true;
		}
		return false;
	}
};
LINK_ENTITY_TO_CLASS(ammo_crossbow, CCrossbowAmmo);
PRECACHE_REGISTER(ammo_crossbow);


// ========================================================================
//	>> Egon ammo
// ========================================================================
#define AMMO_EGON_GIVE		20
#define AMMO_EGON_MODEL		"models/w_chainammo.mdl"

class CEgonAmmo : public CHL1Item
{
public:
	DECLARE_CLASS( CEgonAmmo, CHL1Item );

	void Spawn( void )
	{ 
		Precache();
		SetModel( AMMO_EGON_MODEL );
		BaseClass::Spawn();
	}
	void Precache( void )
	{
		PrecacheModel ( AMMO_EGON_MODEL );
	}
	bool MyTouch( CBasePlayer *pPlayer )
	{
		if (pPlayer->GiveAmmo( AMMO_EGON_GIVE, "Uranium" ) )
		{
			if ( g_pGameRules->ItemShouldRespawn( this ) == GR_ITEM_RESPAWN_NO )
			{
				UTIL_Remove( this );	
			}
			return true;
		}
		return false;
	}
};
LINK_ENTITY_TO_CLASS(ammo_egonclip, CEgonAmmo);
PRECACHE_REGISTER(ammo_egonclip);


// ========================================================================
//	>> Gauss ammo
// ========================================================================
#define AMMO_GAUSS_GIVE		20
#define AMMO_GAUSS_MODEL	"models/w_gaussammo.mdl"

class CGaussAmmo : public CHL1Item
{
public:
	DECLARE_CLASS( CGaussAmmo, CHL1Item );

	void Spawn( void )
	{ 
		Precache();
		SetModel( AMMO_GAUSS_MODEL );
		BaseClass::Spawn();
	}
	void Precache( void )
	{
		PrecacheModel ( AMMO_GAUSS_MODEL );
	}
	bool MyTouch( CBasePlayer *pPlayer )
	{
		if (pPlayer->GiveAmmo( AMMO_GAUSS_GIVE, "Uranium" ) )
		{
			if ( g_pGameRules->ItemShouldRespawn( this ) == GR_ITEM_RESPAWN_NO )
			{
				UTIL_Remove( this );
			}
			return true;
		}
		return false;
	}
};
LINK_ENTITY_TO_CLASS(ammo_gaussclip, CGaussAmmo);
PRECACHE_REGISTER(ammo_gaussclip);


// ========================================================================
//	>> Glock ammo
// ========================================================================
#define AMMO_GLOCK_GIVE		18
#define AMMO_GLOCK_MODEL	"models/w_9mmclip.mdl"

class CGlockAmmo : public CHL1Item
{
public:
	DECLARE_CLASS( CGlockAmmo, CHL1Item );

	void Spawn( void )
	{ 
		Precache();
		SetModel( AMMO_GLOCK_MODEL );
		BaseClass::Spawn();
	}
	void Precache( void )
	{
		PrecacheModel ( AMMO_GLOCK_MODEL );
	}
	bool MyTouch( CBasePlayer *pPlayer )
	{
		if (pPlayer->GiveAmmo( AMMO_GLOCK_GIVE, "9mmRound" ) )
		{
			if ( g_pGameRules->ItemShouldRespawn( this ) == GR_ITEM_RESPAWN_NO )
			{
				UTIL_Remove( this );
			}
			return true;
		}
		return false;
	}
};
LINK_ENTITY_TO_CLASS(ammo_glockclip, CGlockAmmo);
PRECACHE_REGISTER(ammo_glockclip);
LINK_ENTITY_TO_CLASS(ammo_9mmclip, CGlockAmmo);
PRECACHE_REGISTER(ammo_9mmclip);


// ========================================================================
//	>> MP5 ammo
// ========================================================================
#define AMMO_MP5_GIVE		50
#define AMMO_MP5_MODEL		"models/w_9mmARclip.mdl"

class CMP5AmmoClip : public CHL1Item
{
public:
	DECLARE_CLASS( CMP5AmmoClip, CHL1Item );

	void Spawn( void )
	{ 
		Precache();
		SetModel( AMMO_MP5_MODEL );
		BaseClass::Spawn();
	}
	void Precache( void )
	{
		PrecacheModel ( AMMO_MP5_MODEL );
	}
	bool MyTouch( CBasePlayer *pPlayer )
	{
		if (pPlayer->GiveAmmo( AMMO_MP5_GIVE, "9mmRound" ) )
		{
			if ( g_pGameRules->ItemShouldRespawn( this ) == GR_ITEM_RESPAWN_NO )
			{
				UTIL_Remove( this );
			}
			return true;
		}
		return false;
	}
};
LINK_ENTITY_TO_CLASS(ammo_mp5clip, CMP5AmmoClip);
PRECACHE_REGISTER(ammo_mp5clip);
LINK_ENTITY_TO_CLASS(ammo_9mmar, CMP5AmmoClip);
PRECACHE_REGISTER(ammo_9mmar);


// ========================================================================
//	>> MP5Chain (?) ammo
// ========================================================================
#define AMMO_MP5CHAIN_GIVE		200
#define AMMO_MP5CHAIN_MODEL		"models/w_chainammo.mdl"

class CMP5Chainammo : public CHL1Item
{
public:
	DECLARE_CLASS( CMP5Chainammo, CHL1Item );

	void Spawn( void )
	{ 
		Precache();
		SetModel( AMMO_MP5CHAIN_MODEL );
		BaseClass::Spawn();
	}
	void Precache( void )
	{
		PrecacheModel ( AMMO_MP5CHAIN_MODEL );
	}
	bool MyTouch( CBasePlayer *pPlayer )
	{
		if (pPlayer->GiveAmmo( AMMO_MP5CHAIN_GIVE, "9mmRound" ) )
		{
			if ( g_pGameRules->ItemShouldRespawn( this ) == GR_ITEM_RESPAWN_NO )
			{
				UTIL_Remove( this );
			}
			return true;
		}
		return false;
	}
};
LINK_ENTITY_TO_CLASS(ammo_9mmbox, CMP5Chainammo);
PRECACHE_REGISTER(ammo_9mmbox);


// ========================================================================
//	>> MP5 grenades
// ========================================================================
#define AMMO_MP5GRENADE_GIVE		2
#define AMMO_MP5GRENADE_MODEL		"models/w_ARgrenade.mdl"

class CMP5AmmoGrenade : public CHL1Item
{
public:
	DECLARE_CLASS( CMP5AmmoGrenade, CHL1Item );

	void Spawn( void )
	{ 
		Precache();
		SetModel( AMMO_MP5GRENADE_MODEL );
		BaseClass::Spawn();
	}
	void Precache( void )
	{
		PrecacheModel ( AMMO_MP5GRENADE_MODEL );
	}
	bool MyTouch( CBasePlayer *pPlayer )
	{
		if (pPlayer->GiveAmmo( AMMO_MP5GRENADE_GIVE, "MP5_Grenade" ) )
		{
			if ( g_pGameRules->ItemShouldRespawn( this ) == GR_ITEM_RESPAWN_NO )
			{
				UTIL_Remove( this );
			}
			return true;
		}
		return false;
	}
};
LINK_ENTITY_TO_CLASS(ammo_mp5grenades, CMP5AmmoGrenade);
PRECACHE_REGISTER(ammo_mp5grenades);
LINK_ENTITY_TO_CLASS(ammo_argrenades, CMP5AmmoGrenade);
PRECACHE_REGISTER(ammo_argrenades);


// ========================================================================
//	>> 357 ammo
// ========================================================================
#define AMMO_357_GIVE		6
#define AMMO_357_MODEL		"models/w_357ammobox.mdl"

class CPythonAmmo : public CHL1Item
{
public:
	DECLARE_CLASS( CPythonAmmo, CHL1Item );

	void Spawn( void )
	{ 
		Precache();
		SetModel( AMMO_357_MODEL );
		BaseClass::Spawn();
	}
	void Precache( void )
	{
		PrecacheModel ( AMMO_357_MODEL );
	}
	bool MyTouch( CBasePlayer *pPlayer )
	{
		if (pPlayer->GiveAmmo( AMMO_357_GIVE, "357Round" ) )
		{
			if ( g_pGameRules->ItemShouldRespawn( this ) == GR_ITEM_RESPAWN_NO )
			{
				UTIL_Remove( this );
			}
			return true;
		}
		return false;
	}
};
LINK_ENTITY_TO_CLASS(ammo_357, CPythonAmmo);
PRECACHE_REGISTER(ammo_357);


// ========================================================================
//	>> RPG rockets
// ========================================================================
#define AMMO_RPG_GIVE		1
#define AMMO_RPG_MODEL		"models/w_rpgammo.mdl"

class CRpgAmmo : public CHL1Item
{
public:
	DECLARE_CLASS( CRpgAmmo, CHL1Item );

	void Spawn( void )
	{ 
		Precache();
		SetModel( AMMO_RPG_MODEL );
		BaseClass::Spawn();
	}
	void Precache( void )
	{
		PrecacheModel ( AMMO_RPG_MODEL );
	}
	bool MyTouch( CBasePlayer *pPlayer )
	{
		int nGive;

		if ( g_pGameRules->IsMultiplayer() )
		{
			// hand out more ammo per rocket in multiplayer.
			nGive = AMMO_RPG_GIVE * 2;
		}
		else
		{
			nGive = AMMO_RPG_GIVE;
		}

		if (pPlayer->GiveAmmo( nGive, "RPG_Rocket" ) )
		{
			if ( g_pGameRules->ItemShouldRespawn( this ) == GR_ITEM_RESPAWN_NO )
			{
				UTIL_Remove( this );
			}
			return true;
		}
		return false;
	}
};
LINK_ENTITY_TO_CLASS(ammo_rpgclip, CRpgAmmo);
PRECACHE_REGISTER(ammo_rpgclip);


// ========================================================================
//	>> Shotgun ammo
// ========================================================================
#define AMMO_SHOTGUN_GIVE		12
#define AMMO_SHOTGUN_MODEL		"models/w_shotbox.mdl"

class CShotgunAmmo : public CHL1Item
{
public:
	DECLARE_CLASS( CShotgunAmmo, CHL1Item );

	void Spawn( void )
	{ 
		Precache();
		SetModel( AMMO_SHOTGUN_MODEL );
		BaseClass::Spawn();
	}
	void Precache( void )
	{
		PrecacheModel ( AMMO_SHOTGUN_MODEL );
	}
	bool MyTouch( CBasePlayer *pPlayer )
	{
		if (pPlayer->GiveAmmo( AMMO_SHOTGUN_GIVE, "Buckshot" ) )
		{
			if ( g_pGameRules->ItemShouldRespawn( this ) == GR_ITEM_RESPAWN_NO )
			{
				UTIL_Remove( this );
			}
			return true;
		}
		return false;
	}
};
LINK_ENTITY_TO_CLASS(ammo_buckshot, CShotgunAmmo);
PRECACHE_REGISTER(ammo_buckshot);
