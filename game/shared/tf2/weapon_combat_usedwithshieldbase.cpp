//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "basetfplayer_shared.h"
#include "weapon_combat_usedwithshieldbase.h"
#include "weapon_combatshield.h"
#include "weapon_twohandedcontainer.h"

//-----------------------------------------------------------------------------
// Purpose: Make sure we're not switching directly to this weapon, since this 
//			weapon can only be "switched" to by the twohandedcontainer weapon.
//-----------------------------------------------------------------------------
bool CWeaponCombatUsedWithShieldBase::CanDeploy( void )
{
	CBaseTFPlayer *pPlayer = ToBaseTFPlayer( GetOwner() );
	if (!pPlayer)
		return false;

	// Make sure the current weapon is a twohanded weapon container
	CWeaponTwoHandedContainer *pContainer = dynamic_cast<CWeaponTwoHandedContainer*>(pPlayer->GetActiveWeapon());
	if ( !pContainer )
	{
		// Not the two handed container, so deploy the two handed container instead
		pContainer = ( CWeaponTwoHandedContainer * )pPlayer->Weapon_OwnsThisType( "weapon_twohandedcontainer" );
		if ( !pContainer )
			return BaseClass::Deploy();

		// Deploy the container
		pPlayer->Weapon_Switch( pContainer );

		// Make sure the container's using the desired weapon
		pContainer->SetWeapons( this, pContainer->GetRightWeapon() );
		return false;
	}

	return BaseClass::CanDeploy();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : allow - 
//-----------------------------------------------------------------------------
void CWeaponCombatUsedWithShieldBase::AllowShieldPostFrame( bool allow )
{
	CBaseTFPlayer *pOwner = ToBaseTFPlayer( GetOwner() );
	if (!pOwner)
		return;

	CWeaponCombatShield *shield = static_cast< CWeaponCombatShield * >( pOwner->Weapon_OwnsThisType( "weapon_combat_shield" ) );
	if ( !shield )
		return;

	shield->SetAllowPostFrame( allow );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CWeaponCombatUsedWithShieldBase::GetShieldState( void )
{	
	CBaseTFPlayer *pOwner = ToBaseTFPlayer( GetOwner() );
	if ( !pOwner )
		return SS_DOWN;

	CWeaponCombatShield *pShield = pOwner->GetCombatShield();
	if ( !pShield )
		return SS_DOWN;

	return pShield->GetShieldState();
}

//-----------------------------------------------------------------------------
// Purpose: Mirror the values in the container, if there is one
// Input  : *pPlayer - 
// Output : int
//-----------------------------------------------------------------------------
int CWeaponCombatUsedWithShieldBase::UpdateClientData( CBasePlayer *pPlayer )
{
	if ( !pPlayer )
	{
		return BaseClass::UpdateClientData( pPlayer );
	}

	CWeaponTwoHandedContainer *pContainer = ( CWeaponTwoHandedContainer * )pPlayer->Weapon_OwnsThisType( "weapon_twohandedcontainer" );
	if ( !pContainer || pContainer != pPlayer->GetActiveWeapon() )
		return BaseClass::UpdateClientData( pPlayer );

	// Make sure this weapon is one of the container's active weapons
	if ( pContainer->GetLeftWeapon() != this && pContainer->GetRightWeapon() != this )
		return BaseClass::UpdateClientData( pPlayer );

	int retval = pContainer->UpdateClientData( pPlayer );
	m_iState =  pContainer->m_iState;
	return retval;
}

LINK_ENTITY_TO_CLASS( weapon_combat_usedwithshieldbase, CWeaponCombatUsedWithShieldBase );

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponCombatUsedWithShieldBase, DT_WeaponCombatUsedWithShieldBase )

BEGIN_NETWORK_TABLE( CWeaponCombatUsedWithShieldBase, DT_WeaponCombatUsedWithShieldBase )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponCombatUsedWithShieldBase )
END_PREDICTION_DATA()
