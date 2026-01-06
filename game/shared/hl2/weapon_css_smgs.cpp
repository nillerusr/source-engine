//=============================================================================//
//
// Purpose: CS:S weapons recreated from scratch in Source SDK 2013 for usage in a Half-Life 2 setting.
//
// Author: Blixibon
//
//=============================================================================//

#include "cbase.h"
#include "weapon_css_base.h"

//-----------------------------------------------------------------------------
// CWeapon_CSS_HL2_TMP
//-----------------------------------------------------------------------------
class CWeapon_CSS_HL2_TMP : public CBase_CSS_HL2_SMG
{
public:
	DECLARE_CLASS( CWeapon_CSS_HL2_TMP, CBase_CSS_HL2_SMG );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();
	DECLARE_DATADESC();

	CWeapon_CSS_HL2_TMP(void);

	virtual CSS_HL2_WeaponActClass		GetCSSWeaponActClass() { return CSSHL2_WEAPON_SMG2; }

	// TMP is always silenced
	virtual bool IsSilenced() const { return true; }

	virtual float GetViewKickBase() { return 2.75f; }
	
	virtual const Vector& GetBulletSpread( void )
	{
		static const Vector cone = VECTOR_CONE_4DEGREES;
		return cone;
	}

	virtual float GetFireRate( void ) { return 0.07f; }
};

IMPLEMENT_NETWORKCLASS_DT( CWeapon_CSS_HL2_TMP, DT_Weapon_CSS_HL2_TMP )
END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( weapon_css_tmp, CWeapon_CSS_HL2_TMP );
#if PRECACHE_REGISTER_CSS_WEAPONS == 1
PRECACHE_WEAPON_REGISTER( weapon_css_tmp );
#endif

BEGIN_DATADESC( CWeapon_CSS_HL2_TMP )
END_DATADESC()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CWeapon_CSS_HL2_TMP )
END_PREDICTION_DATA()
#endif

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeapon_CSS_HL2_TMP::CWeapon_CSS_HL2_TMP( void )
{
	m_bFiresUnderwater	= true;
}

//-----------------------------------------------------------------------------
// CWeapon_CSS_HL2_MP5Navy
//-----------------------------------------------------------------------------
class CWeapon_CSS_HL2_MP5Navy : public CBase_CSS_HL2_SMG
{
public:
	DECLARE_CLASS( CWeapon_CSS_HL2_MP5Navy, CBase_CSS_HL2_SMG );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();
	DECLARE_DATADESC();

	CWeapon_CSS_HL2_MP5Navy(void);

	virtual CSS_HL2_WeaponActClass		GetCSSWeaponActClass() { return CSSHL2_WEAPON_AR1; }

	virtual float GetViewKickBase() { return 0.25f; }
	
	virtual const Vector& GetBulletSpread( void )
	{
		static const Vector cone = VECTOR_CONE_4DEGREES;
		return cone;
	}

	virtual float GetFireRate( void ) { return 0.08f; }
};

IMPLEMENT_NETWORKCLASS_DT( CWeapon_CSS_HL2_MP5Navy, DT_Weapon_CSS_HL2_MP5Navy )
END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( weapon_css_mp5navy, CWeapon_CSS_HL2_MP5Navy );
#if PRECACHE_REGISTER_CSS_WEAPONS == 1
PRECACHE_WEAPON_REGISTER( weapon_css_mp5navy );
#endif

BEGIN_DATADESC( CWeapon_CSS_HL2_MP5Navy )
END_DATADESC()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CWeapon_CSS_HL2_MP5Navy )
END_PREDICTION_DATA()
#endif

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeapon_CSS_HL2_MP5Navy::CWeapon_CSS_HL2_MP5Navy( void )
{
	m_bFiresUnderwater	= true;
}

//-----------------------------------------------------------------------------
// CWeapon_CSS_HL2_UMP45
//-----------------------------------------------------------------------------
class CWeapon_CSS_HL2_UMP45 : public CBase_CSS_HL2_SMG
{
public:
	DECLARE_CLASS( CWeapon_CSS_HL2_UMP45, CBase_CSS_HL2_SMG );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();
	DECLARE_DATADESC();

	CWeapon_CSS_HL2_UMP45(void);

	virtual CSS_HL2_WeaponActClass		GetCSSWeaponActClass() { return CSSHL2_WEAPON_SMG1; }

	virtual float GetViewKickBase() { return 2.5f; }
	
	virtual const Vector& GetBulletSpread( void )
	{
		static const Vector cone = VECTOR_CONE_4DEGREES;
		return cone;
	}

	virtual float GetFireRate( void ) { return 0.105f; }
};

IMPLEMENT_NETWORKCLASS_DT( CWeapon_CSS_HL2_UMP45, DT_Weapon_CSS_HL2_UMP45 )
END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( weapon_css_ump45, CWeapon_CSS_HL2_UMP45 );
#if PRECACHE_REGISTER_CSS_WEAPONS == 1
PRECACHE_WEAPON_REGISTER( weapon_css_ump45 );
#endif

BEGIN_DATADESC( CWeapon_CSS_HL2_UMP45 )
END_DATADESC()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CWeapon_CSS_HL2_UMP45 )
END_PREDICTION_DATA()
#endif

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeapon_CSS_HL2_UMP45::CWeapon_CSS_HL2_UMP45( void )
{
}

//-----------------------------------------------------------------------------
// CWeapon_CSS_HL2_P90
//-----------------------------------------------------------------------------
class CWeapon_CSS_HL2_P90 : public CBase_CSS_HL2_SMG
{
public:
	DECLARE_CLASS( CWeapon_CSS_HL2_P90, CBase_CSS_HL2_SMG );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();
	DECLARE_DATADESC();

	CWeapon_CSS_HL2_P90(void);

	virtual CSS_HL2_WeaponActClass		GetCSSWeaponActClass() { return CSSHL2_WEAPON_SMG2; }

	virtual float GetViewKickBase() { return 0.75f; }
	
	virtual const Vector& GetBulletSpread( void )
	{
		static const Vector cone = VECTOR_CONE_4DEGREES;
		return cone;
	}

	virtual float GetFireRate( void ) { return 0.07f; }
};

IMPLEMENT_NETWORKCLASS_DT( CWeapon_CSS_HL2_P90, DT_Weapon_CSS_HL2_P90 )
END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( weapon_css_p90, CWeapon_CSS_HL2_P90 );
#if PRECACHE_REGISTER_CSS_WEAPONS == 1
PRECACHE_WEAPON_REGISTER( weapon_css_p90 );
#endif

BEGIN_DATADESC( CWeapon_CSS_HL2_P90 )
END_DATADESC()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CWeapon_CSS_HL2_P90 )
END_PREDICTION_DATA()
#endif

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeapon_CSS_HL2_P90::CWeapon_CSS_HL2_P90( void )
{
}

//-----------------------------------------------------------------------------
// CWeapon_CSS_HL2_MAC10
//-----------------------------------------------------------------------------
class CWeapon_CSS_HL2_MAC10 : public CBase_CSS_HL2_SMG
{
public:
	DECLARE_CLASS( CWeapon_CSS_HL2_MAC10, CBase_CSS_HL2_SMG );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();
	DECLARE_DATADESC();

	CWeapon_CSS_HL2_MAC10(void);

	virtual CSS_HL2_WeaponActClass		GetCSSWeaponActClass() { return CSSHL2_WEAPON_SMG2; }

	virtual float GetViewKickBase() { return 1.0f; }
	
	virtual const Vector& GetBulletSpread( void )
	{
		static const Vector cone = VECTOR_CONE_4DEGREES;
		return cone;
	}

	virtual float GetFireRate( void ) { return 0.075f; }
};

IMPLEMENT_NETWORKCLASS_DT( CWeapon_CSS_HL2_MAC10, DT_Weapon_CSS_HL2_MAC10 )
END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( weapon_css_mac10, CWeapon_CSS_HL2_MAC10 );
#if PRECACHE_REGISTER_CSS_WEAPONS == 1
PRECACHE_WEAPON_REGISTER( weapon_css_mac10 );
#endif

BEGIN_DATADESC( CWeapon_CSS_HL2_MAC10 )
END_DATADESC()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CWeapon_CSS_HL2_MAC10 )
END_PREDICTION_DATA()
#endif

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeapon_CSS_HL2_MAC10::CWeapon_CSS_HL2_MAC10( void )
{
}
