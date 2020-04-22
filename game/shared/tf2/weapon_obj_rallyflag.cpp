//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "basetfplayer_shared.h"
#include "weapon_basecombatobject.h"

#if !defined( CLIENT_DLL )
// #include "grenade_antipersonnel.h"
#else

#define CWeaponObjRallyFlag C_WeaponObjRallyFlag

#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Purpose: Combat object weapon for the Rally Flag
//-----------------------------------------------------------------------------
class CWeaponObjRallyFlag : public CWeaponBaseCombatObject
{
	DECLARE_CLASS( CWeaponObjRallyFlag, CWeaponBaseCombatObject );
public:
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CWeaponObjRallyFlag( void );

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
	CWeaponObjRallyFlag( const CWeaponObjRallyFlag & );						

};

CWeaponObjRallyFlag::CWeaponObjRallyFlag( void )
{
	m_szObjectName = "obj_rallyflag";
	m_vecBuildMins = RALLYFLAG_MINS - Vector( 4,4,4 );
	m_vecBuildMaxs = RALLYFLAG_MAXS + Vector( 4,4,4 );
	SetPredictionEligible( true );
}

LINK_ENTITY_TO_CLASS( weapon_obj_rallyflag, CWeaponObjRallyFlag );


IMPLEMENT_NETWORKCLASS_ALIASED( WeaponObjRallyFlag, DT_WeaponObjRallyFlag )

BEGIN_NETWORK_TABLE( CWeaponObjRallyFlag, DT_WeaponObjRallyFlag )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponObjRallyFlag )
END_PREDICTION_DATA()

PRECACHE_WEAPON_REGISTER(weapon_obj_rallyflag);
