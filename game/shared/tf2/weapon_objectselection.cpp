//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "basetfplayer_shared.h"
#include "basetfcombatweapon_shared.h"

#if defined( CLIENT_DLL )
#include "hud.h"
#include <vgui_controls/Controls.h>
#include <vgui/ISurface.h>
#endif

#include "weapon_objectselection.h"

#if !defined( CLIENT_DLL )
#include "weapon_builder.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS( weapon_objectselection, CWeaponObjectSelection );

PRECACHE_WEAPON_REGISTER( weapon_objectselection );

BEGIN_PREDICTION_DATA( CWeaponObjectSelection )
END_PREDICTION_DATA()

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponObjectSelection, DT_WeaponObjectSelection )

BEGIN_NETWORK_TABLE( CWeaponObjectSelection, DT_WeaponObjectSelection )
#if !defined( CLIENT_DLL )
	SendPropInt( SENDINFO( m_iObjectType ), 8, SPROP_UNSIGNED ),
#else
	RecvPropInt( RECVINFO( m_iObjectType ) ),
#endif
END_NETWORK_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CWeaponObjectSelection::CWeaponObjectSelection()
{
#if defined( CLIENT_DLL )
	m_bNeedSpriteSetup = true;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Deploy the builder weapon instead of this weapon, and tell it to switch to this object.
//-----------------------------------------------------------------------------
bool CWeaponObjectSelection::CanDeploy( void )
{
	CBaseTFPlayer *pPlayer = ToBaseTFPlayer( GetOwner() );
	if (!pPlayer)
		return false;

#if !defined(CLIENT_DLL)
	// Select a state for the builder weapon
	CWeaponBuilder *pBuilder = pPlayer->GetWeaponBuilder();
	if ( pBuilder )
	{
		pBuilder->SetCurrentObject( m_iObjectType );
		pPlayer->Weapon_Switch( pBuilder );
		pPlayer->SetNextAttack( gpGlobals->curtime );
	}
#endif

	// Never deploy this weapon
	return false;
}


void CWeaponObjectSelection::SetType( int iType )
{
	m_iObjectType = iType;
}


//-----------------------------------------------------------------------------
// Purpose: Setup the weapon selection sprite we should use for this weapon
//-----------------------------------------------------------------------------
void CWeaponObjectSelection::SetupObjectSelectionSprite( void )
{
#ifdef CLIENT_DLL
	// Use the sprite details from the text file, with a custom sprite
	char *iconTexture = GetObjectInfo( m_iObjectType )->m_pIconActive;
	if ( iconTexture && iconTexture[ 0 ] )
	{
		m_pSelectionTexture = gHUD.GetIcon( iconTexture );
	}
	else
	{
		m_pSelectionTexture = NULL;
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Return true if this weapon has some ammo
//-----------------------------------------------------------------------------
bool CWeaponObjectSelection::HasAmmo( void )
{
	CBaseTFPlayer *pOwner = ToBaseTFPlayer( GetOwner() );
	if ( !pOwner )
		return false;

	int iCost = CalculateObjectCost( m_iObjectType,  pOwner->GetNumObjects(m_iObjectType), pOwner->GetTeamNumber() );
	return ( pOwner->GetBankResources() >= iCost );
}

#if defined( CLIENT_DLL )
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponObjectSelection::PreDataUpdate( DataUpdateType_t updateType )
{
	BaseClass::PreDataUpdate(updateType);

	m_iOldObjectType = m_iObjectType;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : updateType - 
//-----------------------------------------------------------------------------
void CWeaponObjectSelection::PostDataUpdate( DataUpdateType_t updateType )
{
	BaseClass::PostDataUpdate( updateType );

	// Note, can't do this in OnDataChanged since you can get multiple
	//  predataupdates/postdataupdates in one frame but only one OnDataChanged just before 
	//  rendering
	if ( m_iOldObjectType != m_iObjectType )
	{
		m_bNeedSpriteSetup = true;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponObjectSelection::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged(updateType);

	// If our type has changed or we've never been set up, then
	//  setup our object selection sprite
	if ( m_bNeedSpriteSetup)
	{
		m_bNeedSpriteSetup = false;
		SetupObjectSelectionSprite();
	}
}
#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CWeaponObjectSelection::GetSubType( void )
{ 
#if !defined( CLIENT_DLL )
	return BaseClass::GetSubType();
#else
	// We don't network down the subtype, so use out object type
	return m_iObjectType; 
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CWeaponObjectSelection::GetSlot( void ) const
{
	return GetObjectInfo( m_iObjectType )->m_SelectionSlot;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CWeaponObjectSelection::GetPosition( void ) const
{
	return GetObjectInfo( m_iObjectType )->m_SelectionPosition;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : char const
//-----------------------------------------------------------------------------
const char *CWeaponObjectSelection::GetPrintName( void ) const
{
	return GetObjectInfo( m_iObjectType )->m_pStatusName;
}

#if defined( CLIENT_DLL )
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CHudTexture const * CWeaponObjectSelection::GetSpriteActive( void ) const
{
	return m_pSelectionTexture;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CHudTexture const * CWeaponObjectSelection::GetSpriteInactive( void ) const
{
	return m_pSelectionTexture;
}

#endif