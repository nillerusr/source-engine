//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Client's CBaseTFCombatWeapon
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "c_tf_basecombatweapon.h"
#include "hud.h"
#include "iclientmode.h"
#include "tf_hints.h"
#include "itfhintitem.h"
#include "c_tf_basehint.h"
#include "hud_technologytreedoc.h"  
#include "c_tf_hintmanager.h"
#include "hud_ammo.h"
#include "c_weapon__stubs.h"
#include "c_tf_class_sapper.h"
#include <vgui/ISurface.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

STUB_WEAPON_CLASS_IMPLEMENT( foo_tf_machine_gun, C_TFMachineGun );

IMPLEMENT_CLIENTCLASS_DT( C_TFMachineGun, DT_TFMachineGun, CTFMachineGun )
END_RECV_TABLE()

// Share crosshair stuff among all weapons
bool C_BaseTFCombatWeapon::m_bCrosshairInitialized = false;
vgui::Label	*C_BaseTFCombatWeapon::m_pCrosshairAmmo = NULL;


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int C_BaseTFCombatWeapon::DrawModel( int flags )
{
	int iDrawn = 0;

	// If my carrier is camouflaged, apply the camo to me too
	if ( IsBeingCarried() )
	{
		C_BaseTFPlayer *pPlayer = (C_BaseTFPlayer *)GetOwner();
		if ( pPlayer )
		{
			if ( pPlayer->IsCamouflaged())
			{
				if ( pPlayer->GetCamoMaterial() && ( pPlayer->ComputeCamoEffectAmount() != 1.0f ) )
				{
					modelrender->ForcedMaterialOverride( pPlayer->GetCamoMaterial() );
					iDrawn = BaseClass::DrawModel(flags);
					modelrender->ForcedMaterialOverride( NULL );
				}
				return iDrawn;
			}
		}
	}

	return BaseClass::DrawModel(flags);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int C_BaseTFCombatWeapon::GetFxBlend( void )
{
	if ( !IsCamouflaged() )
		return BaseClass::GetFxBlend();

	return ((C_BaseTFPlayer *)GetOwner())->GetFxBlend();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool C_BaseTFCombatWeapon::IsTransparent( void )
{
	if ( IsCamouflaged() )
		return true;

	return BaseClass::IsTransparent();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : static void
//-----------------------------------------------------------------------------
void C_BaseTFCombatWeapon::CreateCrosshairPanels( void )
{
	m_pCrosshairAmmo	= new vgui::Label( (vgui::Panel *)NULL, "crosshairammo", "100" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BaseTFCombatWeapon::DestroyCrosshairPanels( void )
{
	delete m_pCrosshairAmmo;
	m_pCrosshairAmmo = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BaseTFCombatWeapon::InitializeCrosshairPanels( void )
{
	// Init the crosshair labels if they haven't been
	if ( m_bCrosshairInitialized )
		return;

	m_bCrosshairInitialized = true;

	vgui::Panel *pParent = g_pClientMode->GetViewport();
	m_pCrosshairAmmo->SetContentAlignment( vgui::Label::a_northeast );
	m_pCrosshairAmmo->SetFgColor( Color( 255, 170, 0, 255 ) );
	m_pCrosshairAmmo->SetPaintBackgroundEnabled( false );
	m_pCrosshairAmmo->SetPos( CROSSHAIR_AMMO_LEFT, CROSSHAIR_AMMO_TOP );
	m_pCrosshairAmmo->SetSize( CROSSHAIR_HEALTH_OFFSET / 2, m_pCrosshairAmmo->GetTall() );
	m_pCrosshairAmmo->SetParent( pParent );
	m_pCrosshairAmmo->SetAutoDelete( false );
}

//-----------------------------------------------------------------------------
// Purpose: Draw the ammo counts
//-----------------------------------------------------------------------------
void C_BaseTFCombatWeapon::DrawAmmo()
{
	// Get the local player
	C_BaseTFPlayer *player = C_BaseTFPlayer::GetLocalPlayer();
	if ( player == NULL )
		return;

	GetHudAmmo()->SetPrimaryAmmo( 
		m_iPrimaryAmmoType, 
		GetPrimaryAmmo(), 
		Clip1(), 
		GetMaxClip1() );
	GetHudAmmo()->SetSecondaryAmmo( 
		m_iSecondaryAmmoType, 
		GetSecondaryAmmo(), 
		Clip2(), 
		GetMaxClip2() );

	// ROBIN: Disabled mini ammo count for now
	/*
	InitializeCrosshairPanels();
	DrawMiniAmmo();
	*/

	// HACK: Draw technician's drain level
	if ( IsLocalPlayerClass( TFCLASS_SAPPER ) )
	{
		C_PlayerClassSapper *pSapper = (C_PlayerClassSapper *)player->GetPlayerClass();

		int r, g, b, a;
		int x, y;

		// Get the drained energy
		float flPowerLevel = pSapper->m_flDrainedEnergy;
		float flInverseFactor = 1.0 - flPowerLevel;
		
		gHUD.m_clrNormal.GetColor( r, g, b, a );

		int iWidth = XRES(12);
		int iHeight = YRES(64);

		x = XRES(64);
		y = ( ScreenHeight() - YRES(2) - iHeight );

		// draw the exhausted portion of the bar.
		vgui::surface()->DrawSetColor( Color( r, g * flPowerLevel, b * flPowerLevel, 100 ) );
		vgui::surface()->DrawFilledRect( x, y, x + iWidth, y + iHeight * flInverseFactor );

		// draw the powerered portion of the bar
		vgui::surface()->DrawSetColor( Color( r, g * flPowerLevel, b * flPowerLevel, 190 ) );
		vgui::surface()->DrawFilledRect( x, y + iHeight * flInverseFactor, x + iWidth, y + iHeight * flInverseFactor + iHeight * flPowerLevel );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Draw the mini ammo/health counts around the health
//-----------------------------------------------------------------------------
void C_BaseTFCombatWeapon::DrawMiniAmmo()
{
	// Get the local player
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( pPlayer == NULL )
		return;

	// Draw the ammo to the right of the crosshair
	if ( m_iClip1 >= 0 )
		m_pCrosshairAmmo->SetText( VarArgs("%.3d", m_iClip1 < 999 ? m_iClip1 : 999) );
	else
		m_pCrosshairAmmo->SetText( VarArgs("%.3d", GetPrimaryAmmo() < 999 ? GetPrimaryAmmo() : 999) );

	// BUG: This shouldn't need to be reset, since it's set in the section above, on initialization
	m_pCrosshairAmmo->SetSize( CROSSHAIR_HEALTH_OFFSET / 2, m_pCrosshairAmmo->GetTall() );
}

//-----------------------------------------------------------------------------
// Purpose: Return true if a weapon-pickup icon should be displayed when this weapon is received
//-----------------------------------------------------------------------------
bool C_BaseTFCombatWeapon::ShouldDrawPickup( void )
{
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : char const
//-----------------------------------------------------------------------------
const char *C_BaseTFCombatWeapon::GetPrintName( void )
{
	return GetWpnData().szPrintName;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool C_BaseTFCombatWeapon::ShouldShowUsageHint( void )
{
	return GetWpnData().bShowUsageHint;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bnewentity - 
//-----------------------------------------------------------------------------
void C_BaseTFCombatWeapon::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	if ( updateType != DATA_UPDATE_CREATED )
		return;

	if ( !ShouldShowUsageHint() )
		return;

	// See if the weapon is given as an associated weapon of a technology that
	//  is of level 1 or greater
	CTechnologyTree *tree = GetTechnologyTreeDoc().GetTechnologyTree();
	if ( !tree )
		return;

	bool done = false;
	for ( int i = 0; !done && ( i < tree->GetNumberTechnologies() ); i++ )
	{
		CBaseTechnology *t = tree->GetTechnology( i );
		Assert( t );
		if ( !t )
			continue;

		// Must be associated with a tech >= level 1
		if ( t->GetLevel() < 1 )
			continue;

		for ( int j = 0; j < t->GetNumWeaponAssociations(); j++ )
		{
			const char *associated_weapon = t->GetAssociatedWeapon( j );
			if ( !associated_weapon )
				continue;

			if ( !GetName() )
				continue;

			// Is this tech associating the weapon
			if ( stricmp( GetName(), associated_weapon ) )
				continue;

			if ( t->GetHintsGiven( TF_HINT_WEAPONRECEIVED ) )
				continue;

			t->SetHintsGiven( TF_HINT_WEAPONRECEIVED, true );

			// Fill in hint data for this weapon
			// Only show a max of 3 or 4 weapon received hints at a time
			C_TFBaseHint *hint = CreateGlobalHint( TF_HINT_WEAPONRECEIVED, GetPrintName(), -1, 3 );
			if ( hint )
			{
				ITFHintItem *item = hint->GetHintItem( 0 );
				if ( item )
				{
					item->SetKeyValue( "weapon", GetPrintName() );
					item->SetKeyValue( "weapontype", GetName() );
				}
			}

			done = true;
			break;
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	C_BaseTFCombatWeapon ::GetSecondaryAmmo( void )
{
	// Get the local player
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( pPlayer == NULL )
		return 0;

	return pPlayer->GetAmmoCount( m_iSecondaryAmmoType );
}
