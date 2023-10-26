#include "cbase.h"
#include "squad_inventory_panel.h"
#include <vgui_controls/ImagePanel.h>
#include <vgui_controls/Label.h>
#include "c_asw_marine.h"
#include "asw_shareddefs.h"
#include "c_asw_game_resource.h"
#include "c_asw_marine_resource.h"
#include "c_asw_weapon.h"
#include "asw_marine_profile.h"
#include "asw_weapon_parse.h"
#include "clientmode_asw.h"
#include "briefingtooltip.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

CSquad_Inventory_Panel::CSquad_Inventory_Panel( vgui::Panel *pParent, const char *pElementName ) : BaseClass( pParent, pElementName )
{
	//SetScheme( vgui::scheme()->LoadSchemeFromFile( "resource/SwarmSchemeNew.res", "SwarmSchemeNew" ) );
}

CSquad_Inventory_Panel::~CSquad_Inventory_Panel()
{
	if ( g_hBriefingTooltip.Get() )
	{
		g_hBriefingTooltip->MarkForDeletion();
		g_hBriefingTooltip->SetVisible( false );
		g_hBriefingTooltip = NULL;
	}
}

void CSquad_Inventory_Panel::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
}

void CSquad_Inventory_Panel::PerformLayout()
{
	BaseClass::PerformLayout();

	// position our child entries in a vertical list and expand to fit them
	int border = YRES( 2 );
	int tw = 0, th = border;
	for ( int i = 0; i < m_pEntries.Count(); i++ )
	{
		m_pEntries[i]->InvalidateLayout( true );
		m_pEntries[i]->SetPos( border, th );
		int w, h;
		m_pEntries[i]->GetSize( w, h );
		tw = MAX( w, tw );
		th += h;
	}
	tw = border + tw + border;
	th = th + border;
	SetSize( tw, th );
	SetPos( ScreenWidth() * 0.5f - tw * 0.5f, ScreenHeight() * 0.5f - th * 0.5f );	// position in center of screen
}

void CSquad_Inventory_Panel::OnThink()
{
	BaseClass::OnThink();

	UpdateList();

	for ( int i = 0; i < m_pEntries.Count(); i++ )
	{
		if ( m_pEntries[ i ]->IsCursorOver() )
		{
			// make sure tooltip is created and parented correctly
			if ( !g_hBriefingTooltip.Get() )
			{
				g_hBriefingTooltip = new BriefingTooltip( GetParent(), "SquadInventoryTooltip" );
				g_hBriefingTooltip->SetScheme( vgui::scheme()->LoadSchemeFromFile( "resource/SwarmSchemeNew.res", "SwarmSchemeNew" ) );
			}
			else if ( g_hBriefingTooltip->GetParent() != GetParent() )
			{
				g_hBriefingTooltip->SetParent( GetParent() );
			}

			m_pEntries[ i ]->ShowTooltip();
			break;
		}
	}
}

bool CSquad_Inventory_Panel::MouseClick(int x, int y, bool bRightClick, bool bDown)
{
	for ( int i = 0; i < m_pEntries.Count(); i++ )
	{
		if ( m_pEntries[ i ]->IsCursorOver() )
		{
			if ( bDown )
			{
				m_pEntries[ i ]->ActivateItem();
				SetVisible( false );
				MarkForDeletion();
			}
			return true;
		}
	}
	return false;
}

void CSquad_Inventory_Panel::UpdateList()
{
	if ( !ASWGameResource() )
		return;

	int iEntry = 0;

	for ( int i = 0; i < ASWGameResource()->GetMaxMarineResources(); i++ )
	{
		C_ASW_Marine_Resource* pMR = ASWGameResource()->GetMarineResource( i );
		if ( !pMR )
			continue;

		if ( pMR->IsInhabited() )		// don't show inventory items for player controlled marines
			continue;
		
		C_ASW_Marine *pMarine = pMR->GetMarineEntity();
		if ( !pMarine )
			continue;

		for ( int k = 0; k < ASW_NUM_INVENTORY_SLOTS; k++ )
		{
			C_ASW_Weapon *pWeapon = pMarine->GetASWWeapon( k );
			if ( !pWeapon )
				continue;

			const CASW_WeaponInfo* pInfo = pWeapon->GetWeaponInfo();
			if ( !pInfo || !pInfo->m_bOffhandActivate )		// TODO: Fix for sentry guns
				continue;

			if ( iEntry >= m_pEntries.Count() )
			{
				CSquad_Inventory_Panel_Entry *pPanel = new CSquad_Inventory_Panel_Entry( this, "SquadInventoryPanelEntry" );
				m_pEntries.AddToTail( pPanel );
				InvalidateLayout();
			}

			m_pEntries[ iEntry ]->SetDetails( pMarine, k );
			iEntry++;
		}
	}
}

/// =========================================

CSquad_Inventory_Panel_Entry::CSquad_Inventory_Panel_Entry( vgui::Panel *pParent, const char *pElementName ) : BaseClass( pParent, pElementName )
{
	m_pWeaponImage = new vgui::ImagePanel( this, "Image" );
	m_pMarineNameLabel = new vgui::Label( this, "MarineNameLabel", "" );
	m_bMouseOver = false;
}

CSquad_Inventory_Panel_Entry::~CSquad_Inventory_Panel_Entry()
{

}

void CSquad_Inventory_Panel_Entry::SetDetails( C_ASW_Marine *pMarine, int iInventoryIndex )
{
	m_hMarine = pMarine;
	m_iInventoryIndex = iInventoryIndex;

	UpdateImage();
}

void CSquad_Inventory_Panel_Entry::UpdateImage()
{
	if ( !m_hMarine.Get() )
		return;

	C_ASW_Weapon *pWeapon = m_hMarine->GetASWWeapon( m_iInventoryIndex );
	if ( !pWeapon )
		return;

	const CASW_WeaponInfo* pInfo = pWeapon->GetWeaponInfo();
	if ( !pInfo || !pInfo->m_bOffhandActivate )		// TODO: Fix for sentry guns
		return;

	m_pWeaponImage->SetImage( pInfo->szEquipIcon );

	CASW_Marine_Profile *pProfile = m_hMarine->GetMarineProfile();
	if ( pProfile )
	{
		m_pMarineNameLabel->SetText( pProfile->GetShortName() );
	}
}

void CSquad_Inventory_Panel_Entry::ShowTooltip()
{
	if ( !m_hMarine.Get() )
		return;

	C_ASW_Weapon *pWeapon = m_hMarine->GetASWWeapon( m_iInventoryIndex );
	if ( !pWeapon )
		return;

	const CASW_WeaponInfo* pInfo = pWeapon->GetWeaponInfo();
	if ( !pInfo || !pInfo->m_bOffhandActivate )		// TODO: Fix for sentry guns
		return;

	int x = GetWide() * 0.8f;
	int y = GetTall() * 0.02f;
	LocalToScreen( x, y );
	g_hBriefingTooltip->SetTooltip( this, pInfo->szPrintName, "#asw_weapon_tooltip_activate", x, y, true );
}

void CSquad_Inventory_Panel_Entry::ActivateItem()
{
	if ( !m_hMarine.Get() )
		return;

	C_ASW_Weapon *pWeapon = m_hMarine->GetASWWeapon( m_iInventoryIndex );
	if ( !pWeapon )
		return;

	const CASW_WeaponInfo* pInfo = pWeapon->GetWeaponInfo();
	if ( !pInfo || !pInfo->m_bOffhandActivate )		// TODO: Fix for sentry guns
		return;

	//Msg( "Activating item %s (%s) on marine %s\n", pInfo->szPrintName, pWeapon->GetDebugName(), m_hMarine->GetDebugName() );
	pWeapon->OffhandActivate();
	
	char buffer[ 64 ];
	Q_snprintf(buffer, sizeof(buffer), "cl_ai_offhand %d %d", m_iInventoryIndex, m_hMarine->entindex() );
	engine->ClientCmd( buffer );
}

void CSquad_Inventory_Panel_Entry::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_pMarineNameLabel->SetFgColor( Color( 255, 255, 255, 255 ) );
	m_pMarineNameLabel->SetBgColor( Color( 0, 0, 0, 128 ) );
	m_pMarineNameLabel->SetFont( pScheme->GetFont( "DefaultVerySmall", IsProportional() ) );

	UpdateImage();
}

void CSquad_Inventory_Panel_Entry::PerformLayout()
{
	BaseClass::PerformLayout();

	int w = YRES( 76 );
	int h = YRES( 38 );
	
	SetSize( w, h );

	int border = YRES( 2 );
	m_pMarineNameLabel->SetBounds( border, border, w - border * 2, YRES( 12 ) );
	m_pWeaponImage->SetBounds( 0, 0, w, h );
}

// =========================================================

void ShowSquadInventory()
{
	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if (!pPlayer)
		return;

	if ( engine->IsLevelMainMenuBackground() )
		return;

	vgui::Panel *pPanel = GetClientMode()->GetViewport()->FindChildByName( "SquadInventoryPanel", true );
	if ( pPanel )	
	{
		pPanel->SetVisible(false);
		pPanel->MarkForDeletion();
		return;
	}

	CSquad_Inventory_Panel *pSquadPanel = new CSquad_Inventory_Panel( GetClientMode()->GetViewport(), "SquadInventoryPanel" );		
	if ( !pSquadPanel )
	{
		Msg("Error: CSquad_Inventory_Panel was closed immediately on opening\n");
		return;
	}
	
	pSquadPanel->SetScheme( vgui::scheme()->LoadSchemeFromFile("resource/SwarmSchemeNew.res", "SwarmSchemeNew") );
	pSquadPanel->InvalidateLayout( true, true );
	pSquadPanel->RequestFocus();
	pSquadPanel->SetVisible(true);
	pSquadPanel->SetEnabled(true);
	pSquadPanel->SetKeyBoardInputEnabled(false);
	pSquadPanel->SetZPos( 200 );
	pSquadPanel->MakeReadyForUse();
}

static ConCommand asw_squad_inventory( "asw_squad_inventory", ShowSquadInventory, "Shows squad inventory", 0 );