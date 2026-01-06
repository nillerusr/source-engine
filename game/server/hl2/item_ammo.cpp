//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: The various ammo types for HL2	
//
//=============================================================================//

#include "cbase.h"
#include "player.h"
#include "gamerules.h"
#include "items.h"
#include "ammodef.h"
#include "eventlist.h"
#include "npcevent.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifdef MAPBASE
// ========================================================================
//	>> CItemAmmo
// 
// All ammo items now derive from this for multiplier purposes.
// ========================================================================
class CItemAmmo : public CItem
{
public:
	DECLARE_CLASS( CItemAmmo, CItem );
	DECLARE_DATADESC();

	int ITEM_GiveAmmo( CBasePlayer *pPlayer, float flCount, const char *pszAmmoName, bool bSuppressSound = false )
	{
		int iAmmoType = GetAmmoDef()->Index(pszAmmoName);
		if (iAmmoType == -1)
		{
			Msg("ERROR: Attempting to give unknown ammo type (%s)\n",pszAmmoName);
			return 0;
		}

		flCount *= g_pGameRules->GetAmmoQuantityScale(iAmmoType);

		// Don't give out less than 1 of anything.
		flCount = MAX( 1.0f, flCount );

		// Mapper-specific ammo multiplier.
		// If it results in 0, the ammo will simply be ignored.
		// If the ammo multiplier is negative, assume it's actually a direct number to override with.
		if (m_flAmmoMultiplier != 1.0f)
		{
			if (m_flAmmoMultiplier >= 0)
				flCount *= m_flAmmoMultiplier;
			else
				flCount = -m_flAmmoMultiplier;
		}

		return pPlayer->GiveAmmo( flCount, iAmmoType, bSuppressSound );
	}

	void	InputSetAmmoMultiplier( inputdata_t &inputdata ) { m_flAmmoMultiplier = inputdata.value.Float(); }

	float m_flAmmoMultiplier = 1.0f;
};

BEGIN_DATADESC( CItemAmmo )

	DEFINE_KEYFIELD( m_flAmmoMultiplier,	FIELD_FLOAT, "AmmoMultiplier" ),

	DEFINE_INPUTFUNC( FIELD_FLOAT, "SetAmmoMultiplier", InputSetAmmoMultiplier ),

END_DATADESC()

// Almost all instances of CItem below are for declaring the base class, which is now CItemAmmo.
// This is here so we don't have to #ifdef all of them.
#define CItem CItemAmmo

#else
//---------------------------------------------------------
// Applies ammo quantity scale.
//---------------------------------------------------------
int ITEM_GiveAmmo( CBasePlayer *pPlayer, float flCount, const char *pszAmmoName, bool bSuppressSound = false )
{
	int iAmmoType = GetAmmoDef()->Index(pszAmmoName);
	if (iAmmoType == -1)
	{
		Msg("ERROR: Attempting to give unknown ammo type (%s)\n",pszAmmoName);
		return 0;
	}

	flCount *= g_pGameRules->GetAmmoQuantityScale(iAmmoType);

	// Don't give out less than 1 of anything.
	flCount = MAX( 1.0f, flCount );

	return pPlayer->GiveAmmo( flCount, iAmmoType, bSuppressSound );
}
#endif

// ========================================================================
//	>> BoxSRounds
// ========================================================================
class CItem_BoxSRounds : public CItem
{
public:
	DECLARE_CLASS( CItem_BoxSRounds, CItem );

	void Spawn( void )
	{ 
		Precache( );
		SetModel( "models/items/boxsrounds.mdl" );
		BaseClass::Spawn( );
	}
	void Precache( void )
	{
		PrecacheModel ("models/items/boxsrounds.mdl");
	}
	bool MyTouch( CBasePlayer *pPlayer )
	{
		if (ITEM_GiveAmmo( pPlayer, SIZE_AMMO_PISTOL, "Pistol"))
		{
			if ( g_pGameRules->ItemShouldRespawn( this ) == GR_ITEM_RESPAWN_NO )
			{
				UTIL_Remove(this);	
			}

			return true;
		}
		return false;
	}
};
LINK_ENTITY_TO_CLASS(item_box_srounds, CItem_BoxSRounds);
LINK_ENTITY_TO_CLASS(item_ammo_pistol, CItem_BoxSRounds);

// ========================================================================
//	>> LargeBoxSRounds
// ========================================================================
class CItem_LargeBoxSRounds : public CItem
{
public:
	DECLARE_CLASS( CItem_LargeBoxSRounds, CItem );

	void Spawn( void )
	{ 
		Precache( );
		SetModel( "models/items/boxsrounds.mdl" );
		BaseClass::Spawn( );
	}
	void Precache( void )
	{
		PrecacheModel ("models/items/boxsrounds.mdl");
	}
	bool MyTouch( CBasePlayer *pPlayer )
	{
		if (ITEM_GiveAmmo( pPlayer, SIZE_AMMO_PISTOL_LARGE, "Pistol"))
		{
			if ( g_pGameRules->ItemShouldRespawn( this ) == GR_ITEM_RESPAWN_NO )
			{
				UTIL_Remove(this);	
			}
			return true;
		}
		return false;
	}
};
LINK_ENTITY_TO_CLASS(item_large_box_srounds, CItem_LargeBoxSRounds);
LINK_ENTITY_TO_CLASS(item_ammo_pistol_large, CItem_LargeBoxSRounds);

// ========================================================================
//	>> BoxMRounds
// ========================================================================
class CItem_BoxMRounds : public CItem
{
public:
	DECLARE_CLASS( CItem_BoxMRounds, CItem );

	void Spawn( void )
	{ 
		Precache( );
		SetModel( "models/items/boxmrounds.mdl");
		BaseClass::Spawn( );
	}
	void Precache( void )
	{
		PrecacheModel ("models/items/boxmrounds.mdl");
	}
	bool MyTouch( CBasePlayer *pPlayer )
	{
		if (ITEM_GiveAmmo( pPlayer, SIZE_AMMO_SMG1, "SMG1"))
		{
			if ( g_pGameRules->ItemShouldRespawn( this ) == GR_ITEM_RESPAWN_NO )
			{
				UTIL_Remove(this);	
			}
			return true;
		}
		return false;
	}
};
LINK_ENTITY_TO_CLASS(item_box_mrounds, CItem_BoxMRounds);
LINK_ENTITY_TO_CLASS(item_ammo_smg1, CItem_BoxMRounds);

// ========================================================================
//	>> LargeBoxMRounds
// ========================================================================
class CItem_LargeBoxMRounds : public CItem
{
public:
	DECLARE_CLASS( CItem_LargeBoxMRounds, CItem );

	void Spawn( void )
	{ 
		Precache( );
		SetModel( "models/items/boxmrounds.mdl");
		BaseClass::Spawn( );
	}
	void Precache( void )
	{
		PrecacheModel ("models/items/boxmrounds.mdl");
	}
	bool MyTouch( CBasePlayer *pPlayer )
	{
		if (ITEM_GiveAmmo( pPlayer, SIZE_AMMO_SMG1_LARGE, "SMG1"))
		{
			if ( g_pGameRules->ItemShouldRespawn( this ) == GR_ITEM_RESPAWN_NO )
			{
				UTIL_Remove(this);	
			}
			return true;
		}
		return false;
	}
};
LINK_ENTITY_TO_CLASS(item_large_box_mrounds, CItem_LargeBoxMRounds);
LINK_ENTITY_TO_CLASS(item_ammo_smg1_large, CItem_LargeBoxMRounds);

// ========================================================================
//	>> BoxLRounds
// ========================================================================
class CItem_BoxLRounds : public CItem
{
public:
	DECLARE_CLASS( CItem_BoxLRounds, CItem );

	void Spawn( void )
	{ 
		Precache( );
		SetModel( "models/items/combine_rifle_cartridge01.mdl");
		BaseClass::Spawn( );
	}
	void Precache( void )
	{
		PrecacheModel ("models/items/combine_rifle_cartridge01.mdl");
	}
	bool MyTouch( CBasePlayer *pPlayer )
	{
		if (ITEM_GiveAmmo( pPlayer, SIZE_AMMO_AR2, "AR2"))
		{
			if ( g_pGameRules->ItemShouldRespawn( this ) == GR_ITEM_RESPAWN_NO )
			{
				UTIL_Remove(this);	
			}	
			return true;
		}
		return false;
	}
};
LINK_ENTITY_TO_CLASS(item_box_lrounds, CItem_BoxLRounds);
LINK_ENTITY_TO_CLASS(item_ammo_ar2, CItem_BoxLRounds);

// ========================================================================
//	>> LargeBoxLRounds
// ========================================================================
class CItem_LargeBoxLRounds : public CItem
{
public:
	DECLARE_CLASS( CItem_LargeBoxLRounds, CItem );

	void Spawn( void )
	{ 
		Precache( );
		SetModel( "models/items/combine_rifle_cartridge01.mdl");
		BaseClass::Spawn( );
	}
	void Precache( void )
	{
		PrecacheModel ("models/items/combine_rifle_cartridge01.mdl");
	}
	bool MyTouch( CBasePlayer *pPlayer )
	{
		if (ITEM_GiveAmmo( pPlayer, SIZE_AMMO_AR2_LARGE, "AR2"))
		{
			if ( g_pGameRules->ItemShouldRespawn( this ) == GR_ITEM_RESPAWN_NO )
			{
				UTIL_Remove(this);	
			}	
			return true;
		}
		return false;
	}
};
LINK_ENTITY_TO_CLASS(item_large_box_lrounds, CItem_LargeBoxLRounds);
LINK_ENTITY_TO_CLASS(item_ammo_ar2_large, CItem_LargeBoxLRounds);


// ========================================================================
//	>> CItem_Box357Rounds
// ========================================================================
class CItem_Box357Rounds : public CItem
{
public:
	DECLARE_CLASS( CItem_Box357Rounds, CItem );

	void Precache( void )
	{
		PrecacheModel ("models/items/357ammo.mdl");
	}
	void Spawn( void )
	{ 
		Precache( );
		SetModel( "models/items/357ammo.mdl");
		BaseClass::Spawn( );
	}

	bool MyTouch( CBasePlayer *pPlayer )
	{
		if (ITEM_GiveAmmo( pPlayer, SIZE_AMMO_357, "357"))
		{
			if ( g_pGameRules->ItemShouldRespawn( this ) == GR_ITEM_RESPAWN_NO )
			{
				UTIL_Remove(this);	
			}	
			return true;
		}
		return false;
	}
};
LINK_ENTITY_TO_CLASS(item_ammo_357, CItem_Box357Rounds);


// ========================================================================
//	>> CItem_LargeBox357Rounds
// ========================================================================
class CItem_LargeBox357Rounds : public CItem
{
public:
	DECLARE_CLASS( CItem_LargeBox357Rounds, CItem );

	void Precache( void )
	{
		PrecacheModel ("models/items/357ammobox.mdl");
	}
	void Spawn( void )
	{ 
		Precache( );
		SetModel( "models/items/357ammobox.mdl");
		BaseClass::Spawn( );
	}

	bool MyTouch( CBasePlayer *pPlayer )
	{
		if (ITEM_GiveAmmo( pPlayer, SIZE_AMMO_357_LARGE, "357"))
		{
			if ( g_pGameRules->ItemShouldRespawn( this ) == GR_ITEM_RESPAWN_NO )
			{
				UTIL_Remove(this);	
			}
			return true;
		}
		return false;
	}
};
LINK_ENTITY_TO_CLASS(item_ammo_357_large, CItem_LargeBox357Rounds);


// ========================================================================
//	>> CItem_BoxXBowRounds
// ========================================================================
class CItem_BoxXBowRounds : public CItem
{
public:
	DECLARE_CLASS( CItem_BoxXBowRounds, CItem );

	void Precache( void )
	{
		PrecacheModel ("models/items/crossbowrounds.mdl");
	}

	void Spawn( void )
	{ 
		Precache( );
		SetModel( "models/items/crossbowrounds.mdl");
		BaseClass::Spawn( );
	}

	bool MyTouch( CBasePlayer *pPlayer )
	{
		if (ITEM_GiveAmmo( pPlayer, SIZE_AMMO_CROSSBOW, "XBowBolt" ))
		{
			if ( g_pGameRules->ItemShouldRespawn( this ) == GR_ITEM_RESPAWN_NO )
			{
				UTIL_Remove(this);	
			}	
			return true;
		}
		return false;
	}
};
LINK_ENTITY_TO_CLASS(item_ammo_crossbow, CItem_BoxXBowRounds);


// ========================================================================
//	>> FlareRound
// ========================================================================
class CItem_FlareRound : public CItem
{
public:
	DECLARE_CLASS( CItem_FlareRound, CItem );

	void Spawn( void )
	{ 
		Precache( );
		SetModel( "models/items/flare.mdl");
		BaseClass::Spawn( );
	}
	void Precache( void )
	{
		PrecacheModel ("models/items/flare.mdl");
	}
	bool MyTouch( CBasePlayer *pPlayer )
	{
		if (ITEM_GiveAmmo( pPlayer, 1, "FlareRound"))
		{
			if ( g_pGameRules->ItemShouldRespawn( this ) == GR_ITEM_RESPAWN_NO )
			{
				UTIL_Remove(this);	
			}
			return true;
		}
		return false;
	}
};
LINK_ENTITY_TO_CLASS(item_flare_round, CItem_FlareRound);

// ========================================================================
//	>> BoxFlareRounds
// ========================================================================
#define SIZE_BOX_FLARE_ROUNDS 5

class CItem_BoxFlareRounds : public CItem
{
public:
	DECLARE_CLASS( CItem_BoxFlareRounds, CItem );

	void Spawn( void )
	{ 
		Precache( );
		SetModel( "models/items/boxflares.mdl");
		BaseClass::Spawn( );
	}
	void Precache( void )
	{
		PrecacheModel ("models/items/boxflares.mdl");
	}
	bool MyTouch( CBasePlayer *pPlayer )
	{
		if (ITEM_GiveAmmo( pPlayer, SIZE_BOX_FLARE_ROUNDS, "FlareRound"))
		{
			if ( g_pGameRules->ItemShouldRespawn( this ) == GR_ITEM_RESPAWN_NO )
			{
				UTIL_Remove(this);	
			}
			return true;
		}
		return false;
	}
};
LINK_ENTITY_TO_CLASS(item_box_flare_rounds, CItem_BoxFlareRounds);

// ========================================================================
// RPG Round
// ========================================================================
class CItem_RPG_Round : public CItem
{
public:
	DECLARE_CLASS( CItem_RPG_Round, CItem );

	void Spawn( void )
	{ 
		Precache( );
		SetModel( "models/weapons/w_missile_closed.mdl");
		BaseClass::Spawn( );
	}
	void Precache( void )
	{
		PrecacheModel ("models/weapons/w_missile_closed.mdl");
	}
	bool MyTouch( CBasePlayer *pPlayer )
	{
		if (ITEM_GiveAmmo( pPlayer, SIZE_AMMO_RPG_ROUND, "RPG_Round"))
		{
			if ( g_pGameRules->ItemShouldRespawn( this ) == GR_ITEM_RESPAWN_NO )
			{
				UTIL_Remove(this);	
			}	
			return true;
		}
		return false;
	}
};
LINK_ENTITY_TO_CLASS( item_ml_grenade, CItem_RPG_Round );
LINK_ENTITY_TO_CLASS( item_rpg_round, CItem_RPG_Round );

// ========================================================================
//	>> AR2_Grenade
// ========================================================================
class CItem_AR2_Grenade : public CItem
{
public:
	DECLARE_CLASS( CItem_AR2_Grenade, CItem );

	void Spawn( void )
	{ 
		Precache( );
		SetModel( "models/items/ar2_grenade.mdl");
		BaseClass::Spawn( );
	}
	void Precache( void )
	{
		PrecacheModel ("models/items/ar2_grenade.mdl");
	}
	bool MyTouch( CBasePlayer *pPlayer )
	{
		if (ITEM_GiveAmmo( pPlayer, SIZE_AMMO_SMG1_GRENADE, "SMG1_Grenade"))
		{
			if ( g_pGameRules->ItemShouldRespawn( this ) == GR_ITEM_RESPAWN_NO )
			{
				UTIL_Remove(this);	
			}	
			return true;
		}
		return false;
	}
};
LINK_ENTITY_TO_CLASS(item_ar2_grenade, CItem_AR2_Grenade);
LINK_ENTITY_TO_CLASS(item_ammo_smg1_grenade, CItem_AR2_Grenade);

// ========================================================================
//	>> BoxSniperRounds
// ========================================================================
#define SIZE_BOX_SNIPER_ROUNDS 10

class CItem_BoxSniperRounds : public CItem
{
public:
	DECLARE_CLASS( CItem_BoxSniperRounds, CItem );

	void Spawn( void )
	{ 
		Precache( );
		SetModel( "models/items/boxsniperrounds.mdl");
		BaseClass::Spawn( );
	}
	void Precache( void )
	{
		PrecacheModel ("models/items/boxsniperrounds.mdl");
	}
	bool MyTouch( CBasePlayer *pPlayer )
	{
		if (ITEM_GiveAmmo( pPlayer, SIZE_BOX_SNIPER_ROUNDS, "SniperRound"))
		{
			if ( g_pGameRules->ItemShouldRespawn( this ) == GR_ITEM_RESPAWN_NO )
			{
				UTIL_Remove(this);	
			}	
			return true;
		}
		return false;
	}
};
LINK_ENTITY_TO_CLASS(item_box_sniper_rounds, CItem_BoxSniperRounds);


// ========================================================================
//	>> BoxBuckshot
// ========================================================================
class CItem_BoxBuckshot : public CItem
{
public:
	DECLARE_CLASS( CItem_BoxBuckshot, CItem );

	void Spawn( void )
	{ 
		Precache( );
		SetModel( "models/items/boxbuckshot.mdl");
		BaseClass::Spawn( );
	}
	void Precache( void )
	{
		PrecacheModel ("models/items/boxbuckshot.mdl");
	}
	bool MyTouch( CBasePlayer *pPlayer )
	{
		if (ITEM_GiveAmmo( pPlayer, SIZE_AMMO_BUCKSHOT, "Buckshot"))
		{
			if ( g_pGameRules->ItemShouldRespawn( this ) == GR_ITEM_RESPAWN_NO )
			{
				UTIL_Remove(this);	
			}
			return true;
		}
		return false;
	}
};
LINK_ENTITY_TO_CLASS(item_box_buckshot, CItem_BoxBuckshot);

// ========================================================================
//	>> CItem_AR2AltFireRound
// ========================================================================
class CItem_AR2AltFireRound : public CItem
{
public:
	DECLARE_CLASS( CItem_AR2AltFireRound, CItem );

	void Precache( void )
	{
		PrecacheParticleSystem( "combineball" );
#ifdef EZ
		SetModelName( AllocPooledString( pModelNames[ GetEZVariant() ] ) );
		PrecacheModel( STRING( GetModelName() ) );

		// Goo-covered is just a separate skin of the Arbeit model
		if (GetEZVariant() == EZ_VARIANT_RAD)
			m_nSkin = 1;
#else
		PrecacheModel ("models/items/combine_rifle_ammo01.mdl");
#endif
	}

	void Spawn( void )
	{ 
		Precache( );
#ifdef EZ
		SetModel( STRING( GetModelName() ) );
#else
		SetModel( "models/items/combine_rifle_ammo01.mdl");
#endif
		BaseClass::Spawn( );
	}

	bool MyTouch( CBasePlayer *pPlayer )
	{
		if (ITEM_GiveAmmo( pPlayer, SIZE_AMMO_AR2_ALTFIRE, "AR2AltFire" ) )
		{
			if ( g_pGameRules->ItemShouldRespawn( this ) == GR_ITEM_RESPAWN_NO )
			{
				UTIL_Remove(this);	
			}
			return true;
		}
		return false;
	}

#ifdef EZ
	static const char *pModelNames[];
#endif
};

LINK_ENTITY_TO_CLASS( item_ammo_ar2_altfire, CItem_AR2AltFireRound );

#ifdef EZ
const char *CItem_AR2AltFireRound::pModelNames[EZ_VARIANT_COUNT] = {
	"models/items/combine_rifle_ammo01.mdl",
	"models/items/xen/combine_rifle_ammo01.mdl",
	"models/items/arbeit/combine_rifle_ammo01.mdl", // Skin 1
	"models/items/temporal/combine_rifle_ammo01.mdl",
	"models/items/arbeit/combine_rifle_ammo01.mdl",
	"models/items/blood/combine_rifle_ammo01.mdl",
	"models/items/athenaeum/combine_rifle_ammo01.mdl",
	"models/items/ash/combine_rifle_ammo01.mdl",
};
#endif

#ifdef CSS_WEAPONS_IN_HL2
// ========================================================================
//	>> Box45ACPRounds
// ========================================================================
class CItem_Box45ACPRounds : public CItem
{
public:
	DECLARE_CLASS( CItem_Box45ACPRounds, CItem );

	void Spawn( void )
	{ 
		Precache( );
		SetModel( "models/items/45acp_box.mdl" );
		BaseClass::Spawn( );
	}
	void Precache( void )
	{
		PrecacheModel ("models/items/45acp_box.mdl");
	}
	bool MyTouch( CBasePlayer *pPlayer )
	{
		if (ITEM_GiveAmmo( pPlayer, !IsLarge() ? SIZE_AMMO_45ACP : SIZE_AMMO_45ACP_LARGE, "45ACP"))
		{
			if ( g_pGameRules->ItemShouldRespawn( this ) == GR_ITEM_RESPAWN_NO )
			{
				UTIL_Remove(this);	
			}

			return true;
		}
		return false;
	}
	inline bool IsLarge() { return ClassMatches("item_css_ammo_45acp_large"); }
};
LINK_ENTITY_TO_CLASS( item_css_ammo_45acp, CItem_Box45ACPRounds );
LINK_ENTITY_TO_CLASS( item_css_ammo_45acp_large, CItem_Box45ACPRounds );

// ========================================================================
//	>> Box357SIGRounds
// ========================================================================
class CItem_Box357SIGRounds : public CItem
{
public:
	DECLARE_CLASS( CItem_Box357SIGRounds, CItem );

	void Spawn( void )
	{ 
		Precache( );
		SetModel( "models/items/357sig_box.mdl" );
		BaseClass::Spawn( );
	}
	void Precache( void )
	{
		PrecacheModel ("models/items/357sig_box.mdl");
	}
	bool MyTouch( CBasePlayer *pPlayer )
	{
		if (ITEM_GiveAmmo( pPlayer, !IsLarge() ? SIZE_AMMO_357SIG : SIZE_AMMO_357SIG_LARGE, "357SIG"))
		{
			if ( g_pGameRules->ItemShouldRespawn( this ) == GR_ITEM_RESPAWN_NO )
			{
				UTIL_Remove(this);	
			}

			return true;
		}
		return false;
	}
	inline bool IsLarge() { return ClassMatches("item_css_ammo_357sig_large"); }
};
LINK_ENTITY_TO_CLASS( item_css_ammo_357sig, CItem_Box357SIGRounds );
LINK_ENTITY_TO_CLASS( item_css_ammo_357sig_large, CItem_Box357SIGRounds );

// ========================================================================
//	>> Box556mmRounds
// ========================================================================
class CItem_Box556mmRounds : public CItem
{
public:
	DECLARE_CLASS( CItem_Box556mmRounds, CItem );

	void Spawn( void )
	{ 
		Precache( );
		SetModel( "models/items/556mm_box.mdl" );
		BaseClass::Spawn( );
	}
	void Precache( void )
	{
		PrecacheModel ("models/items/556mm_box.mdl");
	}
	bool MyTouch( CBasePlayer *pPlayer )
	{
		if (ITEM_GiveAmmo( pPlayer, !IsLarge() ? SIZE_AMMO_556mm : SIZE_AMMO_556mm_LARGE, "556mm"))
		{
			if ( g_pGameRules->ItemShouldRespawn( this ) == GR_ITEM_RESPAWN_NO )
			{
				UTIL_Remove(this);	
			}

			return true;
		}
		return false;
	}
	inline bool IsLarge() { return ClassMatches("item_css_ammo_556mm_large"); }
};
LINK_ENTITY_TO_CLASS( item_css_ammo_556mm, CItem_Box556mmRounds );
LINK_ENTITY_TO_CLASS( item_css_ammo_556mm_large, CItem_Box556mmRounds );

// ========================================================================
//	>> Box762mmRounds
// ========================================================================
class CItem_Box762mmRounds : public CItem
{
public:
	DECLARE_CLASS( CItem_Box762mmRounds, CItem );

	void Spawn( void )
	{ 
		Precache( );
		SetModel( "models/items/762mm_box.mdl" );
		BaseClass::Spawn( );
	}
	void Precache( void )
	{
		PrecacheModel ("models/items/762mm_box.mdl");
	}
	bool MyTouch( CBasePlayer *pPlayer )
	{
		if (ITEM_GiveAmmo( pPlayer, !IsLarge() ? SIZE_AMMO_762mm : SIZE_AMMO_762mm_LARGE, "762mm"))
		{
			if ( g_pGameRules->ItemShouldRespawn( this ) == GR_ITEM_RESPAWN_NO )
			{
				UTIL_Remove(this);	
			}

			return true;
		}
		return false;
	}
	inline bool IsLarge() { return ClassMatches("item_css_ammo_762mm_large"); }
};
LINK_ENTITY_TO_CLASS( item_css_ammo_762mm, CItem_Box762mmRounds );
LINK_ENTITY_TO_CLASS( item_css_ammo_762mm_large, CItem_Box762mmRounds );
#endif

// ==================================================================
// Ammo crate which will supply infinite ammo of the specified type
// ==================================================================

// Ammo types
enum
{
	AMMOCRATE_SMALL_ROUNDS,
	AMMOCRATE_MEDIUM_ROUNDS,
	AMMOCRATE_LARGE_ROUNDS,
	AMMOCRATE_RPG_ROUNDS,
	AMMOCRATE_BUCKSHOT,
	AMMOCRATE_GRENADES,
	AMMOCRATE_357,
	AMMOCRATE_CROSSBOW,
	AMMOCRATE_AR2_ALTFIRE,
	AMMOCRATE_SMG_ALTFIRE,
#ifdef MAPBASE
	AMMOCRATE_SLAM,
	AMMOCRATE_EMPTY,
#endif
	NUM_AMMO_CRATE_TYPES,
};

// Ammo crate

class CItem_AmmoCrate : public CBaseAnimating
{
public:
	DECLARE_CLASS( CItem_AmmoCrate, CBaseAnimating );

	void	Spawn( void );
	void	Precache( void );
	bool	CreateVPhysics( void );

	virtual void HandleAnimEvent( animevent_t *pEvent );

	void	SetupCrate( void );
	void	OnRestore( void );

	//FIXME: May not want to have this used in a radius
	int		ObjectCaps( void ) { return (BaseClass::ObjectCaps() | (FCAP_IMPULSE_USE|FCAP_USE_IN_RADIUS)); };
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	void	InputKill( inputdata_t &data );
	void	CrateThink( void );
	
	virtual int OnTakeDamage( const CTakeDamageInfo &info );

protected:

	int		m_nAmmoType;
	int		m_nAmmoIndex;

	static const char *m_lpzModelNames[NUM_AMMO_CRATE_TYPES];
	static const char *m_lpzAmmoNames[NUM_AMMO_CRATE_TYPES];
	static int m_nAmmoAmounts[NUM_AMMO_CRATE_TYPES];
	static const char *m_pGiveWeapon[NUM_AMMO_CRATE_TYPES];

	float	m_flCloseTime;
	COutputEvent	m_OnUsed;
	CHandle< CBasePlayer > m_hActivator;

#ifdef MAPBASE
	COutputEvent	m_OnAmmoTaken;
#endif

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS( item_ammo_crate, CItem_AmmoCrate );

BEGIN_DATADESC( CItem_AmmoCrate )

	DEFINE_KEYFIELD( m_nAmmoType,	FIELD_INTEGER, "AmmoType" ),	

	DEFINE_FIELD( m_flCloseTime, FIELD_FLOAT ),
	DEFINE_FIELD( m_hActivator, FIELD_EHANDLE ),

	// These can be recreated
	//DEFINE_FIELD( m_nAmmoIndex,		FIELD_INTEGER ),
	//DEFINE_FIELD( m_lpzModelNames,	FIELD_ ),
	//DEFINE_FIELD( m_lpzAmmoNames,	FIELD_ ),
	//DEFINE_FIELD( m_nAmmoAmounts,	FIELD_INTEGER ),

	DEFINE_OUTPUT( m_OnUsed, "OnUsed" ),

#ifdef MAPBASE
	DEFINE_OUTPUT( m_OnAmmoTaken, "OnAmmoTaken" ),
#endif

	DEFINE_INPUTFUNC( FIELD_VOID, "Kill", InputKill ),

	DEFINE_THINKFUNC( CrateThink ),

END_DATADESC()

//-----------------------------------------------------------------------------
// Animation events.
//-----------------------------------------------------------------------------

// Models names
const char *CItem_AmmoCrate::m_lpzModelNames[NUM_AMMO_CRATE_TYPES] =
{
	"models/items/ammocrate_pistol.mdl",	// Small rounds
	"models/items/ammocrate_smg1.mdl",		// Medium rounds
	"models/items/ammocrate_ar2.mdl",		// Large rounds
	"models/items/ammocrate_rockets.mdl",	// RPG rounds
	"models/items/ammocrate_buckshot.mdl",	// Buckshot
	"models/items/ammocrate_grenade.mdl",	// Grenades
#ifdef MAPBASE
	"models/items/ammocrate_357.mdl",		// 357
	"models/items/ammocrate_xbow.mdl",		// Crossbow
	"models/items/ammocrate_ar2.mdl",		// Combine Ball 
#else
	"models/items/ammocrate_smg1.mdl",		// 357
	"models/items/ammocrate_smg1.mdl",		// Crossbow

	//FIXME: This model is incorrect!
	"models/items/ammocrate_ar2.mdl",		// Combine Ball 
#endif
	"models/items/ammocrate_smg2.mdl",	    // smg grenade
#ifdef MAPBASE
	"models/items/ammocrate_slam.mdl",	    // slam
	"models/items/ammocrate_empty.mdl",	    // empty
#endif
};

// Ammo type names
const char *CItem_AmmoCrate::m_lpzAmmoNames[NUM_AMMO_CRATE_TYPES] =
{
	"Pistol",		
	"SMG1",			
	"AR2",			
	"RPG_Round",	
	"Buckshot",		
	"Grenade",
	"357",
	"XBowBolt",
	"AR2AltFire",
	"SMG1_Grenade",
#ifdef MAPBASE
	"slam",
	NULL,
#endif
};

// Ammo amount given per +use
int CItem_AmmoCrate::m_nAmmoAmounts[NUM_AMMO_CRATE_TYPES] =
{
	300,	// Pistol
	300,	// SMG1
	300,	// AR2
	3,		// RPG rounds
	120,	// Buckshot
	5,		// Grenades
	50,		// 357
	50,		// Crossbow
	3,		// AR2 alt-fire
	5,
#ifdef MAPBASE
	5,		// SLAM
	NULL	// Empty
#endif
};

const char *CItem_AmmoCrate::m_pGiveWeapon[NUM_AMMO_CRATE_TYPES] =
{
	NULL,	// Pistol
	NULL,	// SMG1
	NULL,	// AR2
	NULL,		// RPG rounds
	NULL,	// Buckshot
	"weapon_frag",		// Grenades
	NULL,		// 357
	NULL,		// Crossbow
	NULL,		// AR2 alt-fire
	NULL,		// SMG alt-fire
#ifdef MAPBASE
	"weapon_slam",		// SLAM
	NULL	// Empty
#endif
};

#define	AMMO_CRATE_CLOSE_DELAY	1.5f

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CItem_AmmoCrate::Spawn( void )
{
	Precache();

	BaseClass::Spawn();

	SetModel( STRING( GetModelName() ) );
	SetMoveType( MOVETYPE_NONE );
	SetSolid( SOLID_VPHYSICS );
	CreateVPhysics();

	ResetSequence( LookupSequence( "Idle" ) );
	SetBodygroup( 1, true );

	m_flCloseTime = gpGlobals->curtime;
	m_flAnimTime = gpGlobals->curtime;
	m_flPlaybackRate = 0.0;
	SetCycle( 0 );

	m_takedamage = DAMAGE_EVENTS_ONLY;

}

//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
bool CItem_AmmoCrate::CreateVPhysics( void )
{
	return ( VPhysicsInitStatic() != NULL );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CItem_AmmoCrate::Precache( void )
{
	SetupCrate();
	PrecacheModel( STRING( GetModelName() ) );

	PrecacheScriptSound( "AmmoCrate.Open" );
	PrecacheScriptSound( "AmmoCrate.Close" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CItem_AmmoCrate::SetupCrate( void )
{
#ifdef MAPBASE
	// Custom models might be desired on, say, empty crates with custom textures
	if (GetModelName() == NULL_STRING)
#endif
	SetModelName( AllocPooledString( m_lpzModelNames[m_nAmmoType] ) );
	
	m_nAmmoIndex = GetAmmoDef()->Index( m_lpzAmmoNames[m_nAmmoType] );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CItem_AmmoCrate::OnRestore( void )
{
	BaseClass::OnRestore();

	// Restore our internal state
	SetupCrate();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pActivator - 
//			*pCaller - 
//			useType - 
//			value - 
//-----------------------------------------------------------------------------
void CItem_AmmoCrate::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CBasePlayer *pPlayer = ToBasePlayer( pActivator );

	if ( pPlayer == NULL )
		return;

	m_OnUsed.FireOutput( pActivator, this );

	int iSequence = LookupSequence( "Open" );

	// See if we're not opening already
	if ( GetSequence() != iSequence )
	{
		Vector mins, maxs;
		trace_t tr;

		CollisionProp()->WorldSpaceAABB( &mins, &maxs );

		Vector vOrigin = GetAbsOrigin();
		vOrigin.z += ( maxs.z - mins.z );
		mins = (mins - GetAbsOrigin()) * 0.2f;
		maxs = (maxs - GetAbsOrigin()) * 0.2f;
		mins.z = ( GetAbsOrigin().z - vOrigin.z );  
		
		UTIL_TraceHull( vOrigin, vOrigin, mins, maxs, MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );

		if ( tr.startsolid || tr.allsolid )
			 return;
			
		m_hActivator = pPlayer;

		// Animate!
		ResetSequence( iSequence );

		// Make sound
		CPASAttenuationFilter sndFilter( this, "AmmoCrate.Open" );
		EmitSound( sndFilter, entindex(), "AmmoCrate.Open" );

		// Start thinking to make it return
		SetThink( &CItem_AmmoCrate::CrateThink );
		SetNextThink( gpGlobals->curtime + 0.1f );
	}

	// Don't close again for two seconds
	m_flCloseTime = gpGlobals->curtime + AMMO_CRATE_CLOSE_DELAY;
}

//-----------------------------------------------------------------------------
// Purpose: allows the crate to open up when hit by a crowbar
//-----------------------------------------------------------------------------
int CItem_AmmoCrate::OnTakeDamage( const CTakeDamageInfo &info )
{
	// if it's the player hitting us with a crowbar, open up
	CBasePlayer *player = ToBasePlayer(info.GetAttacker());
	if (player)
	{
		CBaseCombatWeapon *weapon = player->GetActiveWeapon();

		if (weapon && !stricmp(weapon->GetName(), "weapon_crowbar"))
		{
			// play the normal use sound
			player->EmitSound( "HL2Player.Use" );
			// open the crate
			Use(info.GetAttacker(), info.GetAttacker(), USE_TOGGLE, 0.0f);
		}
	}

	// don't actually take any damage
	return 0;
}


//-----------------------------------------------------------------------------
// Purpose: Catches the monster-specific messages that occur when tagged
//			animation frames are played.
// Input  : *pEvent - 
//-----------------------------------------------------------------------------
void CItem_AmmoCrate::HandleAnimEvent( animevent_t *pEvent )
{
	if ( pEvent->event == AE_AMMOCRATE_PICKUP_AMMO )
	{
		if ( m_hActivator )
		{
			if ( m_pGiveWeapon[m_nAmmoType] && !m_hActivator->Weapon_OwnsThisType( m_pGiveWeapon[m_nAmmoType] ) )
			{
				CBaseEntity *pEntity = CreateEntityByName( m_pGiveWeapon[m_nAmmoType] );
				CBaseCombatWeapon *pWeapon = dynamic_cast<CBaseCombatWeapon*>(pEntity);
				if ( pWeapon )
				{
					pWeapon->SetAbsOrigin( m_hActivator->GetAbsOrigin() );
					pWeapon->m_iPrimaryAmmoType = 0;
					pWeapon->m_iSecondaryAmmoType = 0;
					pWeapon->Spawn();
					if ( !m_hActivator->BumpWeapon( pWeapon ) )
					{
						UTIL_Remove( pEntity );
					}
					else
					{
#ifdef MAPBASE
						m_OnAmmoTaken.FireOutput(m_hActivator, this);
#endif
						SetBodygroup( 1, false );
					}
				}
			}

#ifdef MAPBASE
			// Empty ammo crates should still fire OnAmmoTaken
			if ( m_hActivator->GiveAmmo( m_nAmmoAmounts[m_nAmmoType], m_nAmmoIndex ) != 0 || m_nAmmoType == AMMOCRATE_EMPTY )
#else
			if ( m_hActivator->GiveAmmo( m_nAmmoAmounts[m_nAmmoType], m_nAmmoIndex ) != 0 )
#endif
			{
#ifdef MAPBASE
				m_OnAmmoTaken.FireOutput(m_hActivator, this);
#endif
				SetBodygroup( 1, false );
			}
			m_hActivator = NULL;
		}
		return;
	}

	BaseClass::HandleAnimEvent( pEvent );
}

	
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CItem_AmmoCrate::CrateThink( void )
{
	StudioFrameAdvance();
	DispatchAnimEvents( this );

	SetNextThink( gpGlobals->curtime + 0.1f );

	// Start closing if we're not already
	if ( GetSequence() != LookupSequence( "Close" ) )
	{
		// Not ready to close?
		if ( m_flCloseTime <= gpGlobals->curtime )
		{
			m_hActivator = NULL;

			ResetSequence( LookupSequence( "Close" ) );
		}
	}
	else
	{
		// See if we're fully closed
		if ( IsSequenceFinished() )
		{
			// Stop thinking
			SetThink( NULL );
			CPASAttenuationFilter sndFilter( this, "AmmoCrate.Close" );
			EmitSound( sndFilter, entindex(), "AmmoCrate.Close" );

			// FIXME: We're resetting the sequence here
			// but setting Think to NULL will cause this to never have
			// StudioFrameAdvance called. What are the consequences of that?
			ResetSequence( LookupSequence( "Idle" ) );
			SetBodygroup( 1, true );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &data - 
//-----------------------------------------------------------------------------
void CItem_AmmoCrate::InputKill( inputdata_t &data )
{
#ifdef MAPBASE
	// Why is this its own function?
	// item_dynamic_resupply and item_item_crate are in the same boat.
	// I don't understand.
	m_OnKilled.FireOutput( data.pActivator, this );
#endif

	UTIL_Remove( this );
}

#ifdef EZ2
#undef CItem

#define XEN_GRENADE_SLOT_1 1
#define XEN_GRENADE_SLOT_2 2
#define XEN_GRENADE_SLOT_3 3

#define XEN_GRENADE_SLOTS 3

extern void VerifyXenRecipeManager( const char *pszActivator );

// ==================================================================
// Xen grenade holder
// ==================================================================
class CItem_XenGrenadeHolder : public CItem
{
public:
	DECLARE_CLASS( CItem_XenGrenadeHolder, CItem );

	CItem_XenGrenadeHolder()
	{
		m_bRemoveWhenEmpty = true;
		m_bCustomSlots = false;
	}

	void Spawn( void )
	{ 
		Precache( );
		SetModel( STRING( GetModelName() ) );

		if (!m_bCustomSlots)
		{
			// Enable all slots by default
			for (int i = 0; i < XEN_GRENADE_SLOTS; i++)
			{
				SetSlot( i, true );
			}
		}

		BaseClass::Spawn( );
	}

	void Activate( void )
	{
		m_nAttachmentSlots[0] = LookupAttachment( "slot1" );
		m_nAttachmentSlots[1] = LookupAttachment( "slot2" );
		m_nAttachmentSlots[2] = LookupAttachment( "slot3" );

		BaseClass::Activate( );
	}

	void Precache( void )
	{
		if (GetModelName() == NULL_STRING)
			SetModelName( AllocPooledString( "models/items/xen_grenade_holder001a.mdl" ) );

		PrecacheModel( STRING( GetModelName() ) );

		VerifyXenRecipeManager( GetClassname() );
		UTIL_PrecacheOther( "weapon_hopwire" );
	}

	bool KeyValue( const char *szKeyName, const char *szValue )
	{
		if (FStrEq( szKeyName, "body" ))
		{
			m_bCustomSlots = true;
		}

		return BaseClass::KeyValue( szKeyName, szValue );
	}

	bool MyTouch( CBasePlayer *pPlayer )
	{
		if (IsEmpty())
			return false;

		int iAmmoType = GetAmmoDef()->Index( "XenGrenade" );
		if (iAmmoType == -1)
			return 0;

		// Give the closest slot until we're either empty or the player is full
		while (!IsEmpty())
		{
			if ( !GiveClosestSlot(pPlayer, iAmmoType) )
				break;
		}

		if (IsEmpty())
		{
			// We are now debris
			m_OnEmpty.FireOutput( pPlayer, this );

			if (m_bRemoveWhenEmpty)
			{
				SetContextThink( &CBaseEntity::SUB_RemoveWhenNotVisible, gpGlobals->curtime + 10.0f, "SUB_RemoveWhenNotVisible" );
			}
		}

		// Never return true to avoid direct removal
		return false;
	}

	bool GiveClosestSlot( CBasePlayer *pPlayer, int iAmmoType )
	{
		// Figure out which slots the player is closest to
		int iClosestSlot = 0;
		float flClosestSqr = FLT_MAX;
		for (int i = 0; i < XEN_GRENADE_SLOTS; i++)
		{
			if (!SlotOccupied(i))
				continue;

			// TODO: Perhaps use a dot product method instead?
			Vector vecOrigin;
			GetAttachment( m_nAttachmentSlots[i], vecOrigin );
			float flLengthSqr = (vecOrigin - pPlayer->GetAbsOrigin()).LengthSqr();
			if (flLengthSqr < flClosestSqr)
			{
				iClosestSlot = i;
				flClosestSqr = flLengthSqr;
			}
		}

		if (SlotOccupied( iClosestSlot ))
		{
			// Give a weapon_hopwire first in case the player doesn't have it yet.
			if (pPlayer->GiveNamedItem( "weapon_hopwire" ) || pPlayer->GiveAmmo( 1, iAmmoType ))
			{
				SetSlot( iClosestSlot, false );
				m_OnPickupSlot[iClosestSlot].FireOutput( pPlayer, this );
				return true;
			}
		}

		return false;
	}

	void	SetSlot( int iSlot, bool bOn ) { SetBodygroup( iSlot+1, bOn ); }
	bool	SlotOccupied( int iSlot ) { return GetBodygroup( iSlot+1 ) != 0; }

	bool	IsEmpty() { return m_nBody == 0; }

	void	InputSetSlot1( inputdata_t &inputdata ) { SetSlot( 0, inputdata.value.Bool() ); }
	void	InputSetSlot2( inputdata_t &inputdata ) { SetSlot( 1, inputdata.value.Bool() ); }
	void	InputSetSlot3( inputdata_t &inputdata ) { SetSlot( 2, inputdata.value.Bool() ); }

protected:

	int m_nAttachmentSlots[XEN_GRENADE_SLOTS];

	bool m_bRemoveWhenEmpty;

	bool m_bCustomSlots; // Hack for non-Hammer placement; Don't save

	COutputEvent m_OnPickupSlot[XEN_GRENADE_SLOTS];
	COutputEvent m_OnEmpty;

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS( item_hopwire_holder, CItem_XenGrenadeHolder );

BEGIN_DATADESC( CItem_XenGrenadeHolder )

	DEFINE_KEYFIELD( m_bRemoveWhenEmpty,	FIELD_BOOLEAN, "RemoveWhenEmpty" ),

	DEFINE_INPUTFUNC( FIELD_BOOLEAN, "SetSlot1", InputSetSlot1 ),
	DEFINE_INPUTFUNC( FIELD_BOOLEAN, "SetSlot2", InputSetSlot2 ),
	DEFINE_INPUTFUNC( FIELD_BOOLEAN, "SetSlot3", InputSetSlot3 ),

	DEFINE_OUTPUT( m_OnPickupSlot[0], "OnPickupSlot1" ),
	DEFINE_OUTPUT( m_OnPickupSlot[1], "OnPickupSlot2" ),
	DEFINE_OUTPUT( m_OnPickupSlot[3], "OnPickupSlot3" ),
	DEFINE_OUTPUT( m_OnEmpty, "OnEmpty" ),

END_DATADESC()
#endif

