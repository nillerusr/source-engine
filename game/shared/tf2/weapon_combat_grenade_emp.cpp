//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:	The Commando's anti-personnel grenades
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "Sprite.h"
#include "basetfplayer_shared.h"
#include "weapon_combat_usedwithshieldbase.h"
#include "weapon_combat_basegrenade.h"
#include "weapon_combatshield.h"
#include "in_buttons.h"
#include "grenade_emp.h"


#if defined( CLIENT_DLL )

#define CWeaponCombatGrenadeEMP C_WeaponCombatGrenadeEMP

#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Purpose: Combo shield & grenade weapon
//-----------------------------------------------------------------------------
class CWeaponCombatGrenadeEMP : public CWeaponCombatBaseGrenade
{
	DECLARE_CLASS( CWeaponCombatGrenadeEMP, CWeaponCombatBaseGrenade );
public:
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CWeaponCombatGrenadeEMP();

	virtual void Precache( void );
	virtual CBaseGrenade *CreateGrenade( const Vector &vecOrigin, const Vector &vecAngles, CBasePlayer *pOwner );

	// All predicted weapons need to implement and return true
	virtual bool			IsPredicted( void ) const
	{ 
		return true;
	}

#if defined( CLIENT_DLL )
	virtual bool	ShouldPredict( void )
	{
		if ( GetOwner() == C_BasePlayer::GetLocalPlayer() )
			return true;

		return BaseClass::ShouldPredict();
	}
#endif
private:														
	CWeaponCombatGrenadeEMP( const CWeaponCombatGrenadeEMP & );						

};

LINK_ENTITY_TO_CLASS( weapon_combat_grenade_emp, CWeaponCombatGrenadeEMP );

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponCombatGrenadeEMP, DT_WeaponCombatGrenadeEMP )

BEGIN_NETWORK_TABLE( CWeaponCombatGrenadeEMP, DT_WeaponCombatGrenadeEMP )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponCombatGrenadeEMP )
END_PREDICTION_DATA()

PRECACHE_WEAPON_REGISTER(weapon_combat_grenade_emp);

CWeaponCombatGrenadeEMP::CWeaponCombatGrenadeEMP( void )
{
	SetPredictionEligible( true );
}
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponCombatGrenadeEMP::Precache( void )
{
	BaseClass::Precache();
#if !defined( CLIENT_DLL )
	UTIL_PrecacheOther( "grenade_emp" );
#endif
}

CBaseGrenade *CWeaponCombatGrenadeEMP::CreateGrenade( const Vector &vecOrigin, const Vector &vecAngles, CBasePlayer *pOwner )
{
	return CGrenadeEMP::Create(vecOrigin, vecAngles, pOwner );
}
