//=============================================================================//
//
// Purpose: CS:S weapons recreated from scratch in Source SDK 2013 for usage in a Half-Life 2 setting.
//
// Author: Blixibon
//
//=============================================================================//

#include "cbase.h"
#include "npcevent.h"
#include "weapon_css_base.h"
#include "gamerules.h"
#include "in_buttons.h"
#include "gamestats.h"
#ifndef CLIENT_DLL
#include "basecombatcharacter.h"
#include "ai_basenpc.h"
#include "player.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar pistol_use_new_accuracy;

//-----------------------------------------------------------------------------
// CWeapon_CSS_HL2_Glock18
//-----------------------------------------------------------------------------
class CWeapon_CSS_HL2_Glock18 : public CBase_CSS_HL2_BurstableWeapon<CBase_CSS_HL2_Pistol>
{
public:
	DECLARE_CLASS( CWeapon_CSS_HL2_Glock18, CBase_CSS_HL2_BurstableWeapon<CBase_CSS_HL2_Pistol> );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();
	DECLARE_DATADESC();

	CWeapon_CSS_HL2_Glock18(void);

	void	FinishBurst( void );

	Activity	GetPrimaryAttackActivity( void );

	virtual float GetViewKickBase() { return InBurst() ? 0.15f : 0.0f; }

	virtual float GetShotPenaltyTime() { return InBurst() ? 0.1f : 0.2f; }

	virtual const Vector& GetBulletSpread( void )
	{		
		// Handle NPCs first
		static Vector npcCone = VECTOR_CONE_5DEGREES;
		if ( GetOwner() && GetOwner()->IsNPC() )
			return npcCone;

		static Vector cone;

		float ramp = RemapValClamped(	GetAccuracyPenalty(), 
										0.0f, 
										1.5f, 
										0.0f, 
										1.0f ); 

		// We lerp from very accurate to inaccurate over time
		VectorLerp( VECTOR_CONE_1DEGREES, VECTOR_CONE_6DEGREES, ramp, cone );

		return cone;
	}
	
	virtual int	GetMinBurst() { return InBurst() ? 3 : 1; }
	virtual int	GetMaxBurst() { return 3; }

	virtual float GetFireRate( void ) { return InBurst() ? 0.075f : 0.5f; }
	virtual float GetRefireRate( void ) { return 0.15f;	}
	virtual float GetDryRefireRate( void ) { return 0.2f; }
};

IMPLEMENT_NETWORKCLASS_DT( CWeapon_CSS_HL2_Glock18, DT_Weapon_CSS_HL2_Glock18 )

	DEFINE_CSS_WEAPON_BURSTABLE_NETWORK_TABLE()

END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( weapon_css_glock, CWeapon_CSS_HL2_Glock18 );
#if PRECACHE_REGISTER_CSS_WEAPONS == 1
PRECACHE_WEAPON_REGISTER( weapon_css_glock );
#endif

BEGIN_DATADESC( CWeapon_CSS_HL2_Glock18 )

	DEFINE_CSS_WEAPON_BURSTABLE_DATADESC()

END_DATADESC()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CWeapon_CSS_HL2_Glock18 )

	DEFINE_CSS_WEAPON_BURSTABLE_PREDICTDESC()

END_PREDICTION_DATA()
#endif

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeapon_CSS_HL2_Glock18::CWeapon_CSS_HL2_Glock18( void )
{
	m_bCanUseBurstMode	= true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeapon_CSS_HL2_Glock18::FinishBurst( void )
{
	m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration(); // TODO: Proper cooldown?
	m_flSoonestPrimaryAttack = gpGlobals->curtime + SequenceDuration(); // TODO: Proper cooldown?
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
Activity CWeapon_CSS_HL2_Glock18::GetPrimaryAttackActivity( void )
{
	if (m_bInBurstMode)
		return ACT_VM_SECONDARYATTACK;

	return ACT_VM_PRIMARYATTACK;
}

//-----------------------------------------------------------------------------
// CWeapon_CSS_HL2_USP
//-----------------------------------------------------------------------------
class CWeapon_CSS_HL2_USP : public CBase_CSS_HL2_SilencedWeapon<CBase_CSS_HL2_Pistol>
{
public:
	DECLARE_CLASS( CWeapon_CSS_HL2_USP, CBase_CSS_HL2_SilencedWeapon<CBase_CSS_HL2_Pistol> );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();
	DECLARE_DATADESC();

	CWeapon_CSS_HL2_USP(void);

	virtual float GetViewKickBase() { return m_bSilenced ? 0.05f : 0.0f; }

	virtual float GetShotPenaltyTime() { return 0.2f; }

	virtual const Vector& GetBulletSpread( void )
	{		
		// Handle NPCs first
		static Vector npcCone = VECTOR_CONE_5DEGREES;
		if ( GetOwner() && GetOwner()->IsNPC() )
			return npcCone;

		static Vector cone;

		float ramp = RemapValClamped(	GetAccuracyPenalty(), 
										0.0f, 
										1.5f, 
										0.0f, 
										1.0f ); 

		// We lerp from very accurate to inaccurate over time
		VectorLerp( VECTOR_CONE_1DEGREES, VECTOR_CONE_6DEGREES, ramp, cone );

		return cone;
	}

	virtual float GetFireRate( void ) { return 0.5f; }
	virtual float GetRefireRate( void ) { return 0.15f;	}
	virtual float GetDryRefireRate( void ) { return 0.2f; }

	// CS:S damage boost
	// Player damage: 7 -> 7.7 (8)
	// 
	// Silencer damage adjustment
	// Player damage: 7 -> 6.3 (6)
	// 
	// NPC damage: 3 -> 3
	virtual float GetDamageMultiplier() const { return IsSilenced() ? 0.9f : 1.1f; }
	virtual float GetNPCDamageMultiplier() const { return 1.0f; }
};

IMPLEMENT_NETWORKCLASS_DT( CWeapon_CSS_HL2_USP, DT_Weapon_CSS_HL2_USP )

	DEFINE_CSS_WEAPON_SILENCED_NETWORK_TABLE()

END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( weapon_css_usp, CWeapon_CSS_HL2_USP );
#if PRECACHE_REGISTER_CSS_WEAPONS == 1
PRECACHE_WEAPON_REGISTER( weapon_css_usp );
#endif

BEGIN_DATADESC( CWeapon_CSS_HL2_USP )

	DEFINE_CSS_WEAPON_SILENCED_DATADESC()

END_DATADESC()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CWeapon_CSS_HL2_USP )
END_PREDICTION_DATA()
#endif

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeapon_CSS_HL2_USP::CWeapon_CSS_HL2_USP( void )
{
	m_bCanToggleSilencer = true;
}

//-----------------------------------------------------------------------------
// CWeapon_CSS_HL2_P228
//-----------------------------------------------------------------------------
class CWeapon_CSS_HL2_P228 : public CBase_CSS_HL2_Pistol
{
public:
	DECLARE_CLASS( CWeapon_CSS_HL2_P228, CBase_CSS_HL2_Pistol );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();
	DECLARE_DATADESC();

	CWeapon_CSS_HL2_P228(void);

	virtual float GetViewKickBase() { return 1.25f; }

	virtual float GetShotPenaltyTime() { return 0.3f; }

	virtual const Vector& GetBulletSpread( void )
	{		
		// Handle NPCs first
		static Vector npcCone = VECTOR_CONE_5DEGREES;
		if ( GetOwner() && GetOwner()->IsNPC() )
			return npcCone;

		static Vector cone;

		float ramp = RemapValClamped(	GetAccuracyPenalty(), 
										0.0f, 
										1.5f, 
										0.0f, 
										1.0f ); 

		// We lerp from very accurate to inaccurate over time
		VectorLerp( VECTOR_CONE_1DEGREES, VECTOR_CONE_6DEGREES, ramp, cone );

		return cone;
	}

	virtual float GetFireRate( void ) { return 0.5f; }
	virtual float GetRefireRate( void ) { return 0.15f;	}
	virtual float GetDryRefireRate( void ) { return 0.2f; }
};

IMPLEMENT_NETWORKCLASS_DT( CWeapon_CSS_HL2_P228, DT_Weapon_CSS_HL2_P228 )
END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( weapon_css_p228, CWeapon_CSS_HL2_P228 );
#if PRECACHE_REGISTER_CSS_WEAPONS == 1
PRECACHE_WEAPON_REGISTER( weapon_css_p228 );
#endif

BEGIN_DATADESC( CWeapon_CSS_HL2_P228 )
END_DATADESC()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CWeapon_CSS_HL2_P228 )
END_PREDICTION_DATA()
#endif

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeapon_CSS_HL2_P228::CWeapon_CSS_HL2_P228( void )
{
}

//-----------------------------------------------------------------------------
// CWeapon_CSS_HL2_Deagle
//-----------------------------------------------------------------------------
class CWeapon_CSS_HL2_Deagle : public CBase_CSS_HL2_Pistol
{
public:
	DECLARE_CLASS( CWeapon_CSS_HL2_Deagle, CBase_CSS_HL2_Pistol );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();
	DECLARE_DATADESC();

	CWeapon_CSS_HL2_Deagle(void);

	virtual Activity ActivityOverride( Activity baseAct, bool *pRequired )
	{
		Activity translatedAct = BaseClass::ActivityOverride( baseAct, pRequired );

#if MAPBASE_VER_INT >= 7000
		switch (translatedAct)
		{
			case ACT_RANGE_ATTACK_PISTOL:
				return ACT_RANGE_ATTACK_REVOLVER;
			case ACT_RANGE_ATTACK_PISTOL_LOW:
				return ACT_RANGE_ATTACK_REVOLVER_LOW;
			case ACT_RANGE_ATTACK_PISTOL_MED:
				return ACT_RANGE_ATTACK_REVOLVER_MED;
		}
#endif

		return translatedAct;
	}

	virtual float GetViewKickBase() { return 5.5f; }

	virtual float GetShotPenaltyTime() { return 0.4f; }

	virtual const Vector& GetBulletSpread( void )
	{		
		// Handle NPCs first
		static Vector npcCone = VECTOR_CONE_5DEGREES;
		if ( GetOwner() && GetOwner()->IsNPC() )
			return npcCone;

		static Vector cone;

		float ramp = RemapValClamped(	GetAccuracyPenalty(), 
										0.0f, 
										1.5f, 
										0.0f, 
										1.0f ); 

		// We lerp from very accurate to inaccurate over time
		VectorLerp( VECTOR_CONE_2DEGREES, VECTOR_CONE_8DEGREES, ramp, cone );

		return cone;
	}

	virtual float GetFireRate( void ) { return 0.5f; }
	virtual float GetRefireRate( void ) { return 0.225f; }
	virtual float GetDryRefireRate( void ) { return 0.25f; }

	// Slightly weaker than the .357
	// Player damage: 40 -> 36
	// NPC damage: 30 -> 27
	virtual float GetDamageMultiplier() const { return 0.9f; }
	virtual float GetNPCDamageMultiplier() const { return 0.9f; }
};

IMPLEMENT_NETWORKCLASS_DT( CWeapon_CSS_HL2_Deagle, DT_Weapon_CSS_HL2_Deagle )
END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( weapon_css_deagle, CWeapon_CSS_HL2_Deagle );
#if PRECACHE_REGISTER_CSS_WEAPONS == 1
PRECACHE_WEAPON_REGISTER( weapon_css_deagle );
#endif

BEGIN_DATADESC( CWeapon_CSS_HL2_Deagle )
END_DATADESC()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CWeapon_CSS_HL2_Deagle )
END_PREDICTION_DATA()
#endif

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeapon_CSS_HL2_Deagle::CWeapon_CSS_HL2_Deagle( void )
{
}

//-----------------------------------------------------------------------------
// CWeapon_CSS_HL2_FiveSeveN
//-----------------------------------------------------------------------------
class CWeapon_CSS_HL2_FiveSeveN : public CBase_CSS_HL2_Pistol
{
public:
	DECLARE_CLASS( CWeapon_CSS_HL2_FiveSeveN, CBase_CSS_HL2_Pistol );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();
	DECLARE_DATADESC();

	CWeapon_CSS_HL2_FiveSeveN(void);

	virtual float GetViewKickBase() { return 1.5f; }

	virtual float GetShotPenaltyTime() { return 0.4f; }

	virtual const Vector& GetBulletSpread( void )
	{		
		// Handle NPCs first
		static Vector npcCone = VECTOR_CONE_3DEGREES;
		if ( GetOwner() && GetOwner()->IsNPC() )
			return npcCone;

		static Vector cone;

		float ramp = RemapValClamped(	GetAccuracyPenalty(), 
										0.0f, 
										1.5f, 
										0.0f, 
										1.0f ); 

		// We lerp from very accurate to inaccurate over time
		VectorLerp( VECTOR_CONE_1DEGREES, VECTOR_CONE_5DEGREES, ramp, cone );

		return cone;
	}

	virtual float GetFireRate( void ) { return 0.5f; }
	virtual float GetRefireRate( void ) { return 0.15f; }
	virtual float GetDryRefireRate( void ) { return 0.25f; }
};

IMPLEMENT_NETWORKCLASS_DT( CWeapon_CSS_HL2_FiveSeveN, DT_Weapon_CSS_HL2_FiveSeveN )
END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( weapon_css_fiveseven, CWeapon_CSS_HL2_FiveSeveN );
#if PRECACHE_REGISTER_CSS_WEAPONS == 1
PRECACHE_WEAPON_REGISTER( weapon_css_fiveseven );
#endif

BEGIN_DATADESC( CWeapon_CSS_HL2_FiveSeveN )
END_DATADESC()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CWeapon_CSS_HL2_FiveSeveN )
END_PREDICTION_DATA()
#endif

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeapon_CSS_HL2_FiveSeveN::CWeapon_CSS_HL2_FiveSeveN( void )
{
}

#define DUAL_BERETTAS_DROPPED_MODEL "models/weapons/w_pist_elite_dropped.mdl"

//-----------------------------------------------------------------------------
// CWeapon_CSS_HL2_DualBerettas
//-----------------------------------------------------------------------------
class CWeapon_CSS_HL2_DualBerettas : public CBase_CSS_HL2_Pistol
{
public:
	DECLARE_CLASS( CWeapon_CSS_HL2_DualBerettas, CBase_CSS_HL2_Pistol );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();
	DECLARE_DATADESC();

	CWeapon_CSS_HL2_DualBerettas(void);

	void	PrimaryAttack();
	Activity	GetPrimaryAttackActivity( void );

	virtual float GetViewKickBase() { return 0.5f; }

	virtual float GetShotPenaltyTime() { return 0.2f; }

	virtual const Vector& GetBulletSpread( void )
	{		
		// Handle NPCs first
		static Vector npcCone = VECTOR_CONE_5DEGREES;
		if ( GetOwner() && GetOwner()->IsNPC() )
			return npcCone;

		static Vector cone;

		float ramp = RemapValClamped(	GetAccuracyPenalty(), 
										0.0f, 
										1.5f, 
										0.0f, 
										1.0f ); 

		// We lerp from very accurate to inaccurate over time
		VectorLerp( VECTOR_CONE_2DEGREES, VECTOR_CONE_7DEGREES, ramp, cone );

		return cone;
	}

#if MAPBASE_VER_INT < 7000
#ifndef CLIENT_DLL
	// HACKHACK: The dropped model needs to be set as the main model when the weapon drops (and when it spawns), so use this unsaved boolean
	// with FallInit() to make it use the dropped model instead
	bool m_bInFallInit;

	void FallInit()
	{
		m_bInFallInit = true;
		BaseClass::FallInit();
		m_bInFallInit = false;
	}
#endif

	void Precache()
	{
		BaseClass::Precache();

		m_iDroppedModelIndex = CBaseEntity::PrecacheModel( DUAL_BERETTAS_DROPPED_MODEL );
	}

	const char *GetWorldModel( void ) const
	{
#ifdef CLIENT_DLL
		return GetOwner() == NULL ? DUAL_BERETTAS_DROPPED_MODEL : BaseClass::GetWorldModel();
#else
		return (GetOwner() == NULL || m_bInFallInit) ? DUAL_BERETTAS_DROPPED_MODEL : BaseClass::GetWorldModel();
#endif
	}

#ifdef CLIENT_DLL
	int GetWorldModelIndex( void )
	{
		return GetOwner() == NULL ? m_iDroppedModelIndex : BaseClass::GetWorldModelIndex();
	}
#endif
#endif

	virtual float GetFireRate( void ) { return 0.5f; }
	virtual float GetRefireRate( void ) { return 0.12f; }
	virtual float GetDryRefireRate( void ) { return 0.25f; }

	// Tries to replicate CS:S's boosted damage without going crazy
	// Player damage: 5 -> 7
	// NPC damage: 3 -> 3
	virtual float GetDamageMultiplier() const { return 1.4f; }
	virtual float GetNPCDamageMultiplier() const { return 1.0f; }

private:
	CNetworkVar( bool, m_bGunMode );
#if MAPBASE_VER_INT < 7000
	CNetworkVar( int, m_iDroppedModelIndex ); // Not saved
#endif
};

IMPLEMENT_NETWORKCLASS_DT( CWeapon_CSS_HL2_DualBerettas, DT_Weapon_CSS_HL2_DualBerettas )

#ifdef CLIENT_DLL
	RecvPropBool( RECVINFO( m_bGunMode ) ),
#if MAPBASE_VER_INT < 7000
	RecvPropInt( RECVINFO( m_iDroppedModelIndex ) ),
#endif
#else
	SendPropBool( SENDINFO( m_bGunMode ) ),
#if MAPBASE_VER_INT < 7000
	SendPropModelIndex( SENDINFO( m_iDroppedModelIndex ) ),
#endif
#endif

END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( weapon_css_elite, CWeapon_CSS_HL2_DualBerettas );
#if PRECACHE_REGISTER_CSS_WEAPONS == 1
PRECACHE_WEAPON_REGISTER( weapon_css_elite );
#endif

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CWeapon_CSS_HL2_DualBerettas )

	DEFINE_PRED_FIELD( m_bGunMode, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),

END_PREDICTION_DATA()
#endif

BEGIN_DATADESC( CWeapon_CSS_HL2_DualBerettas )

	DEFINE_FIELD( m_bGunMode, FIELD_BOOLEAN ),

END_DATADESC()

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeapon_CSS_HL2_DualBerettas::CWeapon_CSS_HL2_DualBerettas( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeapon_CSS_HL2_DualBerettas::PrimaryAttack( void )
{
	BaseClass::PrimaryAttack();
	m_bGunMode = !m_bGunMode;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
Activity CWeapon_CSS_HL2_DualBerettas::GetPrimaryAttackActivity( void )
{
	if (m_bGunMode)
		return ACT_VM_SECONDARYATTACK;

	return ACT_VM_PRIMARYATTACK;
}

#ifdef EZ
//-----------------------------------------------------------------------------
// CWeapon_Arbeit_Pistol
//-----------------------------------------------------------------------------
class CWeapon_Arbeit_Pistol : public CBase_CSS_HL2_Pistol
{
public:
	DECLARE_CLASS( CWeapon_Arbeit_Pistol, CBase_CSS_HL2_Pistol );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();
	DECLARE_DATADESC();

	CWeapon_Arbeit_Pistol( void );

	virtual float GetViewKickBase() { return 1.25f; }

	virtual float GetShotPenaltyTime() { return 0.3f; }

	virtual const Vector& GetBulletSpread( void )
	{
		// Handle NPCs first
		static Vector npcCone = VECTOR_CONE_5DEGREES;
		if (GetOwner() && GetOwner()->IsNPC())
			return npcCone;

		static Vector cone;

		float ramp = RemapValClamped( GetAccuracyPenalty(),
			0.0f,
			1.5f,
			0.0f,
			1.0f );

		// We lerp from very accurate to inaccurate over time
		VectorLerp( VECTOR_CONE_1DEGREES, VECTOR_CONE_6DEGREES, ramp, cone );

		return cone;
	}

	virtual float GetFireRate( void ) { return 0.5f; }
	virtual float GetRefireRate( void ) { return 0.15f; }
	virtual float GetDryRefireRate( void ) { return 0.2f; }
};

IMPLEMENT_NETWORKCLASS_DT( CWeapon_Arbeit_Pistol, DT_Weapon_Arbeit_Pistol )
END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( weapon_arbeit_pistol, CWeapon_Arbeit_Pistol );
#if PRECACHE_REGISTER_CSS_WEAPONS == 1
PRECACHE_WEAPON_REGISTER( weapon_arbeit_pistol );
#endif

BEGIN_DATADESC( CWeapon_Arbeit_Pistol )
END_DATADESC()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CWeapon_Arbeit_Pistol )
END_PREDICTION_DATA()
#endif

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeapon_Arbeit_Pistol::CWeapon_Arbeit_Pistol( void )
{
}
#endif
