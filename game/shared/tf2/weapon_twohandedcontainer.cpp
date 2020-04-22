//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Support weapon and weapons contained within it
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "basetfplayer_shared.h"
#include "weapon_twohandedcontainer.h"
#include "baseviewmodel_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CWeaponTwoHandedContainer::CWeaponTwoHandedContainer()
{
	m_hRightWeapon = NULL;
	m_hLeftWeapon = NULL;
	SetPredictionEligible( true );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CWeaponTwoHandedContainer::~CWeaponTwoHandedContainer()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponTwoHandedContainer::Spawn( void )
{
	BaseClass::Spawn();
}

#ifdef CLIENT_DLL
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponTwoHandedContainer::GetViewmodelBoneControllers( CBaseViewModel *pViewModel, float controllers[MAXSTUDIOBONECTRLS])
{
	C_BasePlayer *player = ToBasePlayer( GetOwner() );
	Assert( player );

	if ( !player )
		return;

	// Find the weapon that matches the viewmodel
	if ( m_hLeftWeapon != NULL && player->GetViewModel(0) == pViewModel )
	{
		m_hLeftWeapon->GetViewmodelBoneControllers( pViewModel, controllers);
	}
	else if ( m_hRightWeapon != NULL && player->GetViewModel(1) == pViewModel )
	{
		m_hRightWeapon->GetViewmodelBoneControllers( pViewModel, controllers);
	}
}

#else // CLIENT_DLL

void CWeaponTwoHandedContainer::SetTransmit( CCheckTransmitInfo *pInfo, bool bAlways )
{
	// Skip this work if we're already marked for transmission.
	if ( pInfo->m_pTransmitEdict->Get( entindex() ) )
		return;

	// Send our left and right weapons.
	if ( m_hLeftWeapon )
		m_hLeftWeapon->SetTransmit( pInfo, bAlways );

	if ( m_hRightWeapon )
		m_hRightWeapon->SetTransmit( pInfo, bAlways );

	BaseClass::SetTransmit( pInfo, bAlways );
}

#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CWeaponTwoHandedContainer::GetViewModel( int viewmodelindex /*=0*/ )
{
	if ( m_hLeftWeapon != NULL && m_hRightWeapon != NULL )
	{
		if ( viewmodelindex == 0 )
		{
			return m_hLeftWeapon->GetViewModel();
		}
		else
		{
			return m_hRightWeapon->GetViewModel();
		}
	}
	return BaseClass::GetViewModel( viewmodelindex );
}

//-----------------------------------------------------------------------------
// Purpose: Get the string to print death notices with
//-----------------------------------------------------------------------------
char *CWeaponTwoHandedContainer::GetDeathNoticeName( void )
{
	// If we have a weapon in our left slot, return it. Otherwise, return this weapon.
	if ( m_hLeftWeapon )
		return m_hLeftWeapon->GetDeathNoticeName();

	return BaseClass::GetDeathNoticeName();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponTwoHandedContainer::ItemPostFrame( void )
{
	// HACK HACK:  Do nonshield first in case it disallows ItemPostFrame on shield
	if ( m_hLeftWeapon != NULL )
	{
// REMOVE WHEN ALL WEAPONS ARE PREDICTED!
#if defined( CLIENT_DLL )
		if ( m_hLeftWeapon->IsPredicted() )
#endif
		m_hLeftWeapon->ItemPostFrame();
	}

	if ( m_hRightWeapon != NULL && m_hRightWeapon->IsPredicted() )
	{
// REMOVE WHEN ALL WEAPONS ARE PREDICTED!
#if defined( CLIENT_DLL )
		if ( m_hRightWeapon->IsPredicted() )
#endif
		m_hRightWeapon->ItemPostFrame();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called each frame by the player PostThink, if the player's not ready to attack yet
//-----------------------------------------------------------------------------
void CWeaponTwoHandedContainer::ItemBusyFrame( void )
{
	// HACK HACK:  Do nonshield first in case it disallows ItemPostFrame on shield
	if ( m_hLeftWeapon != NULL )
	{
		m_hLeftWeapon->ItemBusyFrame();
	}

	if ( m_hRightWeapon != NULL )
	{
		m_hRightWeapon->ItemBusyFrame();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponTwoHandedContainer::SetWeapons( CBaseTFCombatWeapon *left, CBaseTFCombatWeapon *right )
{
	CBaseTFPlayer *pOwner = ToBaseTFPlayer( GetOwner() );
	if ( !pOwner )
		return;

	// Do we have a different left weapon?
	if ( m_hLeftWeapon.Get() && m_hLeftWeapon != left )
	{
		// Holster our old one
		m_hLeftWeapon->Holster();
		m_hLeftWeapon = NULL;
	}
	// Do we have a different right weapon?
	if ( m_hRightWeapon.Get() && m_hRightWeapon != right )
	{
		// Holster our old one
		m_hRightWeapon->Holster();
		m_hRightWeapon = NULL;
	}

	// Make new weapons if we need to
	if ( !m_hLeftWeapon )
	{
		m_hLeftWeapon = left;
		if ( m_hLeftWeapon )
		{
			m_hLeftWeapon->SetOwner( pOwner );
			m_hLeftWeapon->Deploy();
			m_hLeftWeapon->SetViewModelIndex( 0 );
			//m_hLeftWeapon->SendWeaponAnim( ACT_IDLE );
		}
	}

	if ( !m_hRightWeapon )
	{
		m_hRightWeapon = right;
		if ( m_hRightWeapon )
		{
			m_hRightWeapon->SetOwner( pOwner );
			m_hRightWeapon->Deploy();
			m_hRightWeapon->SetViewModelIndex( 1 );
			//m_hRightWeapon->SendWeaponAnim( ACT_IDLE );

			UnhideSecondViewmodel();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Unhide the second viewmodel, in case we're switching from a single weapon
//-----------------------------------------------------------------------------
void CWeaponTwoHandedContainer::UnhideSecondViewmodel( void )
{
	CBaseTFPlayer *pOwner = ToBaseTFPlayer( GetOwner() );
	if ( pOwner )
	{
		CBaseViewModel *pVM = pOwner->GetViewModel(1);
		if ( pVM )
		{
			pVM->RemoveEffects( EF_NODRAW );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Abort any reload we have in progress
//-----------------------------------------------------------------------------
void CWeaponTwoHandedContainer::AbortReload( void )
{
	BaseClass::AbortReload();

	if ( m_hLeftWeapon )
	{
		m_hLeftWeapon->AbortReload();
	}
	if ( m_hRightWeapon )
	{
		m_hRightWeapon->AbortReload();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Return true if the left weapon has any ammo
//-----------------------------------------------------------------------------
bool CWeaponTwoHandedContainer::HasAnyAmmo( void )
{
	if ( m_hLeftWeapon )
		return m_hLeftWeapon->HasAnyAmmo();

	return BaseClass::HasAnyAmmo();
}

//-----------------------------------------------------------------------------
// Purpose: Deploy and start thinking
//-----------------------------------------------------------------------------
bool CWeaponTwoHandedContainer::Deploy( void )
{
	if ( !BaseClass::Deploy() )
		return false;

	if ( m_hLeftWeapon )
	{
		m_hLeftWeapon->Deploy();
	}
	if ( m_hRightWeapon )
	{
		m_hRightWeapon->Deploy();
		UnhideSecondViewmodel();
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseCombatWeapon *CWeaponTwoHandedContainer::GetLastWeapon( void )
{
	if ( m_hLeftWeapon )
		return m_hLeftWeapon->GetLastWeapon();

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CWeaponTwoHandedContainer::GetDefaultAnimSpeed( void )
{
	if ( m_hLeftWeapon )
		return m_hLeftWeapon->GetDefaultAnimSpeed();

	return 1.0;
}

//-----------------------------------------------------------------------------
// Purpose: Stop thinking and holster
//-----------------------------------------------------------------------------
bool CWeaponTwoHandedContainer::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	CBaseTFPlayer *pOwner = ToBaseTFPlayer( GetOwner() );

	// If I'm holstering a weapon for another weapon that supports two-handed, just switch them out
	CBaseTFCombatWeapon	*pWeapon = (CBaseTFCombatWeapon	*)pSwitchingTo;
	if ( pWeapon && pWeapon->SupportsTwoHanded() )
	{
		// For now, holster the left weapon and switch it.
		// In the future, we might want weapons to say which side they'd like to be on
		SetWeapons( pWeapon, m_hRightWeapon );
		
		// We might need to force the new weapon to be in the right animation
		if ( 0 ) //if ( m_hRightWeapon.Get() && m_hRightWeapon->IsReflectingAnimations() )
		{
			pWeapon->SendWeaponAnim( m_hRightWeapon->GetLastReflectedActivity() );
		}

		UnhideSecondViewmodel();
		return false;
	}

	if ( m_hLeftWeapon )
	{
		m_hLeftWeapon->Holster(pSwitchingTo);
	}
	if ( m_hRightWeapon )
	{
		m_hRightWeapon->Holster(pSwitchingTo);
	}

	// We're changing to a single weapon, so hide the second viewmodel
	if ( pOwner )
	{
		CBaseViewModel *pVM = pOwner->GetViewModel(1);
		if ( pVM )
		{
			pVM->AddEffects( EF_NODRAW );
		}
	}

	return BaseClass::Holster(pSwitchingTo);
}

//-----------------------------------------------------------------------------
// Purpose: Get the correct weight of our active weapon
//-----------------------------------------------------------------------------
int CWeaponTwoHandedContainer::GetWeight( void )
{
	if ( !m_hLeftWeapon )
		return BaseClass::GetWeight();

	return m_hLeftWeapon->GetWpnData().iWeight;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : CBaseTFCombatWeapon
//-----------------------------------------------------------------------------
CBaseTFCombatWeapon *CWeaponTwoHandedContainer::GetLeftWeapon( void )
{
	return m_hLeftWeapon;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : CBaseTFCombatWeapon
//-----------------------------------------------------------------------------
CBaseTFCombatWeapon *CWeaponTwoHandedContainer::GetRightWeapon( void )
{
	return m_hRightWeapon;
}

LINK_ENTITY_TO_CLASS( weapon_twohandedcontainer, CWeaponTwoHandedContainer );

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponTwoHandedContainer , DT_WeaponTwoHandedContainer )
BEGIN_NETWORK_TABLE( CWeaponTwoHandedContainer , DT_WeaponTwoHandedContainer )
#if !defined( CLIENT_DLL )
	SendPropEHandle( SENDINFO(m_hRightWeapon) ),
	SendPropEHandle( SENDINFO(m_hLeftWeapon) ),
#else
	RecvPropEHandle( RECVINFO(m_hRightWeapon ) ),
	RecvPropEHandle( RECVINFO(m_hLeftWeapon ) ),
#endif
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponTwoHandedContainer  )

	DEFINE_PRED_FIELD( m_hRightWeapon, FIELD_EHANDLE, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_hLeftWeapon, FIELD_EHANDLE, FTYPEDESC_INSENDTABLE ),
#if defined( CLIENT_DLL )
	// DEFINE_FIELD( m_hOldRightWeapon, FIELD_EHANDLE ),
	// DEFINE_FIELD( m_hOldLeftWeapon, FIELD_EHANDLE ),
#endif

END_PREDICTION_DATA()

PRECACHE_WEAPON_REGISTER(weapon_twohandedcontainer);
