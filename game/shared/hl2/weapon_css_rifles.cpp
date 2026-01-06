//=============================================================================//
//
// Purpose: CS:S weapons recreated from scratch in Source SDK 2013 for usage in a Half-Life 2 setting.
//
// Author: Blixibon
//
//=============================================================================//

#include "cbase.h"
#include "weapon_css_base.h"

//=============================================================================
// ASSAULT RIFLES
//=============================================================================

//-----------------------------------------------------------------------------
// CWeapon_CSS_HL2_FAMAS
//-----------------------------------------------------------------------------
class CWeapon_CSS_HL2_FAMAS : public CBase_CSS_HL2_BurstableWeapon<CBase_CSS_HL2_Rifle>
{
public:
	DECLARE_CLASS( CWeapon_CSS_HL2_FAMAS, CBase_CSS_HL2_BurstableWeapon<CBase_CSS_HL2_Rifle> );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();
	DECLARE_DATADESC();

	CWeapon_CSS_HL2_FAMAS(void);

	virtual CSS_HL2_WeaponActClass		GetCSSWeaponActClass() { return CSSHL2_WEAPON_AR2; }

	virtual void FinishBurst( void )
	{
		m_flNextPrimaryAttack = gpGlobals->curtime + 0.2f; // TODO: Proper cooldown?
	}

	virtual float GetViewKickBase() { return InBurst() ? 6.25f : 7.1f; }
	
	virtual const Vector& GetBulletSpread( void )
	{
		static const Vector cone = VECTOR_CONE_3DEGREES;
		return cone;
	}

	virtual float GetFireRate( void ) { return InBurst() ? 0.075f : 0.09f; }

	// Tries to replicate CS:S's lessened damage
	// Player damage: 8 -> 7.2 (7)
	// NPC damage: 3 -> 3
	virtual float GetDamageMultiplier() const { return 0.9f; }
	virtual float GetNPCDamageMultiplier() const { return 1.0f; }
};

IMPLEMENT_NETWORKCLASS_DT( CWeapon_CSS_HL2_FAMAS, DT_Weapon_CSS_HL2_FAMAS )

	DEFINE_CSS_WEAPON_BURSTABLE_NETWORK_TABLE()

END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( weapon_css_famas, CWeapon_CSS_HL2_FAMAS );
#if PRECACHE_REGISTER_CSS_WEAPONS == 1
PRECACHE_WEAPON_REGISTER( weapon_css_famas );
#endif

BEGIN_DATADESC( CWeapon_CSS_HL2_FAMAS )

	DEFINE_CSS_WEAPON_BURSTABLE_DATADESC()

END_DATADESC()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CWeapon_CSS_HL2_FAMAS )

	DEFINE_CSS_WEAPON_BURSTABLE_PREDICTDESC()

END_PREDICTION_DATA()
#endif

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeapon_CSS_HL2_FAMAS::CWeapon_CSS_HL2_FAMAS( void )
{
	m_bCanUseBurstMode	= true;
}

//-----------------------------------------------------------------------------
// CWeapon_CSS_HL2_M4A1
//-----------------------------------------------------------------------------
class CWeapon_CSS_HL2_M4A1 : public CBase_CSS_HL2_SilencedWeapon<CBase_CSS_HL2_Rifle>
{
public:
	DECLARE_CLASS( CWeapon_CSS_HL2_M4A1, CBase_CSS_HL2_SilencedWeapon<CBase_CSS_HL2_Rifle> );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();
	DECLARE_DATADESC();

	CWeapon_CSS_HL2_M4A1(void);

	virtual CSS_HL2_WeaponActClass		GetCSSWeaponActClass() { return CSSHL2_WEAPON_AR1; }

	virtual float GetViewKickBase() { return 7.0f; }
	
	virtual const Vector& GetBulletSpread( void )
	{
		static const Vector cone = VECTOR_CONE_3DEGREES;
		return cone;
	}

	virtual float GetFireRate( void ) { return 0.09f; }
};

IMPLEMENT_NETWORKCLASS_DT( CWeapon_CSS_HL2_M4A1, DT_Weapon_CSS_HL2_M4A1 )

	DEFINE_CSS_WEAPON_SILENCED_NETWORK_TABLE()

END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( weapon_css_m4a1, CWeapon_CSS_HL2_M4A1 );
#if PRECACHE_REGISTER_CSS_WEAPONS == 1
PRECACHE_WEAPON_REGISTER( weapon_css_m4a1 );
#endif

BEGIN_DATADESC( CWeapon_CSS_HL2_M4A1 )

	DEFINE_CSS_WEAPON_SILENCED_DATADESC()

END_DATADESC()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CWeapon_CSS_HL2_M4A1 )
END_PREDICTION_DATA()
#endif

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeapon_CSS_HL2_M4A1::CWeapon_CSS_HL2_M4A1( void )
{
	m_bCanToggleSilencer = true;
}

//-----------------------------------------------------------------------------
// CWeapon_CSS_HL2_AUG
//-----------------------------------------------------------------------------
class CWeapon_CSS_HL2_AUG : public CBase_CSS_HL2_ScopeableWeapon<CBase_CSS_HL2_Rifle>
{
public:
	DECLARE_CLASS( CWeapon_CSS_HL2_AUG, CBase_CSS_HL2_ScopeableWeapon<CBase_CSS_HL2_Rifle> );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();
	DECLARE_DATADESC();

	CWeapon_CSS_HL2_AUG(void);

	virtual CSS_HL2_WeaponActClass		GetCSSWeaponActClass() { return CSSHL2_WEAPON_SMG1; } // TODO: This is a bit offset

	virtual float GetViewKickBase() { return 6.7f; }
	
	virtual const Vector& GetBulletSpread( void )
	{
		static Vector cone;
		if (IsWeaponZoomed())
			cone = VECTOR_CONE_1DEGREES;
		else
			cone = VECTOR_CONE_2DEGREES;
		return cone;
	}

	virtual float GetFireRate( void ) { return 0.09f; }
};

IMPLEMENT_NETWORKCLASS_DT( CWeapon_CSS_HL2_AUG, DT_Weapon_CSS_HL2_AUG )

	DEFINE_CSS_WEAPON_SCOPEABLE_NETWORK_TABLE()

END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( weapon_css_aug, CWeapon_CSS_HL2_AUG );
#if PRECACHE_REGISTER_CSS_WEAPONS == 1
PRECACHE_WEAPON_REGISTER( weapon_css_aug );
#endif

BEGIN_DATADESC( CWeapon_CSS_HL2_AUG )

	DEFINE_CSS_WEAPON_SCOPEABLE_DATADESC()

END_DATADESC()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CWeapon_CSS_HL2_AUG )

	DEFINE_CSS_WEAPON_SCOPEABLE_PREDICTDESC()

END_PREDICTION_DATA()
#endif

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeapon_CSS_HL2_AUG::CWeapon_CSS_HL2_AUG( void )
{
	m_bCanUseScope = true;
}

//-----------------------------------------------------------------------------
// CWeapon_CSS_HL2_SG552
//-----------------------------------------------------------------------------
class CWeapon_CSS_HL2_SG552 : public CBase_CSS_HL2_ScopeableWeapon<CBase_CSS_HL2_Rifle>
{
public:
	DECLARE_CLASS( CWeapon_CSS_HL2_SG552, CBase_CSS_HL2_ScopeableWeapon<CBase_CSS_HL2_Rifle> );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();
	DECLARE_DATADESC();

	CWeapon_CSS_HL2_SG552(void);

	virtual CSS_HL2_WeaponActClass		GetCSSWeaponActClass() { return CSSHL2_WEAPON_AR1; }

	virtual float GetViewKickBase() { return IsWeaponZoomed() ? 6.4f : 7.0f; }
	
	virtual const Vector& GetBulletSpread( void )
	{
		static Vector cone;
		if (IsWeaponZoomed())
			cone = VECTOR_CONE_2DEGREES;
		else
			cone = VECTOR_CONE_3DEGREES;
		return cone;
	}

	virtual float GetFireRate( void ) { return 0.09f; }
};

IMPLEMENT_NETWORKCLASS_DT( CWeapon_CSS_HL2_SG552, DT_Weapon_CSS_HL2_SG552 )

	DEFINE_CSS_WEAPON_SCOPEABLE_NETWORK_TABLE()

END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( weapon_css_sg552, CWeapon_CSS_HL2_SG552 );
#if PRECACHE_REGISTER_CSS_WEAPONS == 1
PRECACHE_WEAPON_REGISTER( weapon_css_sg552 );
#endif

BEGIN_DATADESC( CWeapon_CSS_HL2_SG552 )

	DEFINE_CSS_WEAPON_SCOPEABLE_DATADESC()

END_DATADESC()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CWeapon_CSS_HL2_SG552 )

	DEFINE_CSS_WEAPON_SCOPEABLE_PREDICTDESC()

END_PREDICTION_DATA()
#endif

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeapon_CSS_HL2_SG552::CWeapon_CSS_HL2_SG552( void )
{
	m_bCanUseScope = true;
}

//-----------------------------------------------------------------------------
// CWeapon_CSS_HL2_Galil
//-----------------------------------------------------------------------------
class CWeapon_CSS_HL2_Galil : public CBase_CSS_HL2_Rifle
{
public:
	DECLARE_CLASS( CWeapon_CSS_HL2_Galil, CBase_CSS_HL2_Rifle );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();
	DECLARE_DATADESC();

	CWeapon_CSS_HL2_Galil(void);

	virtual CSS_HL2_WeaponActClass		GetCSSWeaponActClass() { return CSSHL2_WEAPON_AR1; }

	virtual float GetViewKickBase() { return 6.5f; }
	
	virtual const Vector& GetBulletSpread( void )
	{
		static const Vector cone = VECTOR_CONE_4DEGREES;
		return cone;
	}

	virtual float GetFireRate( void ) { return 0.09f; }
};

IMPLEMENT_NETWORKCLASS_DT( CWeapon_CSS_HL2_Galil, DT_Weapon_CSS_HL2_Galil )
END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( weapon_css_galil, CWeapon_CSS_HL2_Galil );
#if PRECACHE_REGISTER_CSS_WEAPONS == 1
PRECACHE_WEAPON_REGISTER( weapon_css_galil );
#endif

BEGIN_DATADESC( CWeapon_CSS_HL2_Galil )
END_DATADESC()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CWeapon_CSS_HL2_Galil )
END_PREDICTION_DATA()
#endif

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeapon_CSS_HL2_Galil::CWeapon_CSS_HL2_Galil( void )
{
}

//-----------------------------------------------------------------------------
// CWeapon_CSS_HL2_AK47
//-----------------------------------------------------------------------------
class CWeapon_CSS_HL2_AK47 : public CBase_CSS_HL2_Rifle
{
public:
	DECLARE_CLASS( CWeapon_CSS_HL2_AK47, CBase_CSS_HL2_Rifle );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();
	DECLARE_DATADESC();

	CWeapon_CSS_HL2_AK47(void);

	virtual CSS_HL2_WeaponActClass		GetCSSWeaponActClass() { return CSSHL2_WEAPON_AR1; }

	virtual float GetViewKickBase() { return 6.75f; }
	
	virtual const Vector& GetBulletSpread( void )
	{
		static const Vector cone = VECTOR_CONE_3DEGREES;
		return cone;
	}

	virtual float GetFireRate( void ) { return 0.1f; }
};

IMPLEMENT_NETWORKCLASS_DT( CWeapon_CSS_HL2_AK47, DT_Weapon_CSS_HL2_AK47 )
END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( weapon_css_ak47, CWeapon_CSS_HL2_AK47 );
#if PRECACHE_REGISTER_CSS_WEAPONS == 1
PRECACHE_WEAPON_REGISTER( weapon_css_ak47 );
#endif

BEGIN_DATADESC( CWeapon_CSS_HL2_AK47 )
END_DATADESC()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CWeapon_CSS_HL2_AK47 )
END_PREDICTION_DATA()
#endif

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeapon_CSS_HL2_AK47::CWeapon_CSS_HL2_AK47( void )
{
}

//=============================================================================
// SNIPER RIFLES
//=============================================================================

//-----------------------------------------------------------------------------
// CWeapon_CSS_HL2_Scout
//-----------------------------------------------------------------------------
class CWeapon_CSS_HL2_Scout : public CBase_CSS_HL2_SniperRifle
{
public:
	DECLARE_CLASS( CWeapon_CSS_HL2_Scout, CBase_CSS_HL2_SniperRifle );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();
	DECLARE_DATADESC();

	CWeapon_CSS_HL2_Scout(void);

	virtual float GetViewKickBase() { return IsWeaponZoomed() ? 3.0f : 4.0f; }
	
	virtual const Vector& GetBulletSpread( void )
	{
		static const Vector cone = VECTOR_CONE_1DEGREES;
		return cone;
	}

	virtual float GetFireRate( void ) { return 1.25f; }

	virtual int		GetZoom1FOV() const { return 40; }
	virtual int		GetZoom2FOV() const { return 15; }
	virtual int		GetZoomRate() const { return 0.2f; }
	virtual int		GetUnZoomRate() const { return 0.15f; }

	// Player damage: 9 -> 30.6 (30)
	// NPC damage: 4 -> 20
	virtual float GetDamageMultiplier() const { return 3.4f; }
	virtual float GetNPCDamageMultiplier() const { return 5.0f; }
};

IMPLEMENT_NETWORKCLASS_DT( CWeapon_CSS_HL2_Scout, DT_Weapon_CSS_HL2_Scout )
END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( weapon_css_scout, CWeapon_CSS_HL2_Scout );
#if PRECACHE_REGISTER_CSS_WEAPONS == 1
PRECACHE_WEAPON_REGISTER( weapon_css_scout );
#endif

BEGIN_DATADESC( CWeapon_CSS_HL2_Scout )
END_DATADESC()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CWeapon_CSS_HL2_Scout )
END_PREDICTION_DATA()
#endif

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeapon_CSS_HL2_Scout::CWeapon_CSS_HL2_Scout( void )
{
}

//-----------------------------------------------------------------------------
// CWeapon_CSS_HL2_SG550
//-----------------------------------------------------------------------------
class CWeapon_CSS_HL2_SG550 : public CBase_CSS_HL2_SniperRifle
{
public:
	DECLARE_CLASS( CWeapon_CSS_HL2_SG550, CBase_CSS_HL2_SniperRifle );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();
	DECLARE_DATADESC();

	CWeapon_CSS_HL2_SG550(void);

	virtual float GetViewKickBase() { return IsWeaponZoomed() ? 3.0f : 2.5f; }
	
	virtual const Vector& GetBulletSpread( void )
	{
		static const Vector cone = VECTOR_CONE_1DEGREES;
		return cone;
	}

	virtual float GetFireRate( void ) { return 0.25f; }

	virtual int		GetZoom1FOV() const { return 40; }
	virtual int		GetZoom2FOV() const { return 15; }
	virtual int		GetZoomRate() const { return 0.2f; }
	virtual int		GetUnZoomRate() const { return 0.15f; }

	virtual int	GetMinBurst() { return 1; }
	virtual int	GetMaxBurst() { return 3; }

	// Player damage: 8 -> 24.8 (25)
	// NPC damage: 3 -> 18
	virtual float GetDamageMultiplier() const { return 3.1f; }
	virtual float GetNPCDamageMultiplier() const { return 6.0f; }
};

IMPLEMENT_NETWORKCLASS_DT( CWeapon_CSS_HL2_SG550, DT_Weapon_CSS_HL2_SG550 )
END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( weapon_css_sg550, CWeapon_CSS_HL2_SG550 );
#if PRECACHE_REGISTER_CSS_WEAPONS == 1
PRECACHE_WEAPON_REGISTER( weapon_css_sg550 );
#endif

BEGIN_DATADESC( CWeapon_CSS_HL2_SG550 )
END_DATADESC()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CWeapon_CSS_HL2_SG550 )
END_PREDICTION_DATA()
#endif

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeapon_CSS_HL2_SG550::CWeapon_CSS_HL2_SG550( void )
{
}

//-----------------------------------------------------------------------------
// CWeapon_CSS_HL2_AWP
//-----------------------------------------------------------------------------
class CWeapon_CSS_HL2_AWP : public CBase_CSS_HL2_SniperRifle
{
public:
	DECLARE_CLASS( CWeapon_CSS_HL2_AWP, CBase_CSS_HL2_SniperRifle );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();
	DECLARE_DATADESC();

	CWeapon_CSS_HL2_AWP(void);

	virtual float GetViewKickBase() { return IsWeaponZoomed() ? 2.0f : 6.0f; }
	
	virtual const Vector& GetBulletSpread( void )
	{
		static const Vector cone = VECTOR_CONE_1DEGREES;
		return cone;
	}

	virtual float GetFireRate( void ) { return 1.5f; }

	virtual int		GetZoom1FOV() const { return 40; }
	virtual int		GetZoom2FOV() const { return 10; }
	virtual int		GetZoomRate() const { return 0.2f; }
	virtual int		GetUnZoomRate() const { return 0.15f; }

	// Player damage: 40 -> 60
	// NPC damage: 30 -> 45
	virtual float GetDamageMultiplier() const { return 1.5f; }
	virtual float GetNPCDamageMultiplier() const { return 1.5f; }

	// The .357 ammo already delivers a lot of force, so use a tamer force scale for the AWP
	virtual float	GetDamageForceScale() const { return 2.0f; }
};

IMPLEMENT_NETWORKCLASS_DT( CWeapon_CSS_HL2_AWP, DT_Weapon_CSS_HL2_AWP )
END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( weapon_css_awp, CWeapon_CSS_HL2_AWP );
#if PRECACHE_REGISTER_CSS_WEAPONS == 1
PRECACHE_WEAPON_REGISTER( weapon_css_awp );
#endif

BEGIN_DATADESC( CWeapon_CSS_HL2_AWP )
END_DATADESC()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CWeapon_CSS_HL2_AWP )
END_PREDICTION_DATA()
#endif

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeapon_CSS_HL2_AWP::CWeapon_CSS_HL2_AWP( void )
{
}

//-----------------------------------------------------------------------------
// CWeapon_CSS_HL2_G3SG1
//-----------------------------------------------------------------------------
class CWeapon_CSS_HL2_G3SG1 : public CBase_CSS_HL2_SniperRifle
{
public:
	DECLARE_CLASS( CWeapon_CSS_HL2_G3SG1, CBase_CSS_HL2_SniperRifle );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();
	DECLARE_DATADESC();

	CWeapon_CSS_HL2_G3SG1(void);

	virtual float GetViewKickBase() { return IsWeaponZoomed() ? 3.5f : 4.5f; }
	
	virtual const Vector& GetBulletSpread( void )
	{
		static const Vector cone = VECTOR_CONE_1DEGREES;
		return cone;
	}

	virtual float GetFireRate( void ) { return 0.25f; }

	virtual int		GetZoom1FOV() const { return 40; }
	virtual int		GetZoom2FOV() const { return 15; }
	virtual int		GetZoomRate() const { return 0.2f; }
	virtual int		GetUnZoomRate() const { return 0.15f; }

	// Player damage: 9 -> 32.4 (32)
	// NPC damage: 4 -> 21
	virtual float GetDamageMultiplier() const { return 3.6f; }
	virtual float GetNPCDamageMultiplier() const { return 5.25f; }
};

IMPLEMENT_NETWORKCLASS_DT( CWeapon_CSS_HL2_G3SG1, DT_Weapon_CSS_HL2_G3SG1 )
END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( weapon_css_g3sg1, CWeapon_CSS_HL2_G3SG1 );
#if PRECACHE_REGISTER_CSS_WEAPONS == 1
PRECACHE_WEAPON_REGISTER( weapon_css_g3sg1 );
#endif

BEGIN_DATADESC( CWeapon_CSS_HL2_G3SG1 )
END_DATADESC()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CWeapon_CSS_HL2_G3SG1 )
END_PREDICTION_DATA()
#endif

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeapon_CSS_HL2_G3SG1::CWeapon_CSS_HL2_G3SG1( void )
{
}
