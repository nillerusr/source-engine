//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:	The Commando's anti-personnel grenades
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "basetfplayer_shared.h"
#include "weapon_combat_usedwithshieldbase.h"
#include "weapon_combat_basegrenade.h"
#include "weapon_combatshield.h"
#include "in_buttons.h"

#if !defined( CLIENT_DLL )
#include "grenade_antipersonnel.h"
#else
#define CWeaponCombatGrenade C_WeaponCombatGrenade
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


class CBaseGrenade;

//-----------------------------------------------------------------------------
// Purpose: Combo shield & grenade weapon
//-----------------------------------------------------------------------------
class CWeaponCombatGrenade : public CWeaponCombatBaseGrenade
{
	DECLARE_CLASS( CWeaponCombatGrenade, CWeaponCombatBaseGrenade );
public:
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CWeaponCombatGrenade();

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
	CWeaponCombatGrenade( const CWeaponCombatGrenade & );						

};

CWeaponCombatGrenade::CWeaponCombatGrenade( void )
{
	SetPredictionEligible( true );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponCombatGrenade::Precache( void )
{
	BaseClass::Precache();
#if !defined( CLIENT_DLL )
	UTIL_PrecacheOther( "grenade_antipersonnel" );
#endif
}

CBaseGrenade *CWeaponCombatGrenade::CreateGrenade( const Vector &vecOrigin, const Vector &vecAngles, CBasePlayer *pOwner )
{
#if !defined( CLIENT_DLL )
	return CGrenadeAntiPersonnel::Create(vecOrigin, vecAngles, pOwner );
#else
	return NULL;
#endif
}


LINK_ENTITY_TO_CLASS( weapon_combat_grenade, CWeaponCombatGrenade );

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponCombatGrenade, DT_WeaponCombatGrenade )

BEGIN_NETWORK_TABLE( CWeaponCombatGrenade, DT_WeaponCombatGrenade )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponCombatGrenade )
END_PREDICTION_DATA()

PRECACHE_WEAPON_REGISTER(weapon_combat_grenade);
