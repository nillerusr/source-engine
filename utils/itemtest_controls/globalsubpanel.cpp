//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=============================================================================


// Valve includes
#include "itemtest/itemtest_controls.h"
#include "vgui_controls/CheckButton.h"
#include "vgui_controls/ComboBox.h"
#include "vgui_controls/MenuItem.h"
#include "vgui_controls/PanelListPanel.h"
#include "tier1/fmtstr.h"


// Last include
#include <tier0/memdbgon.h>


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CGlobalSubPanel::CGlobalSubPanel( vgui::Panel *pParent, const char *pszName, const char *pszNextName )
: BaseClass( pParent, pszName, pszNextName )
{
	{
		vgui::Label *pNameLabel = new vgui::Label( this, "nameLabel", "#nameLabel" );
		m_pNameTextEntry = new vgui::TextEntry( this, "nameTextEntry" );
		m_pNameTextEntry->AddActionSignalTarget( this );

		m_pPanelListPanel->AddItem( pNameLabel, m_pNameTextEntry );
	}

	AddStatusPanels( "name" );

	{
		vgui::Label *pClassLabel = new vgui::Label( this, "classLabel", "#classLabel" );
		m_pClassComboBox = new vgui::ComboBox( this, "classComboBox", CItemUploadTF::GetClassCount(), false );
		m_pClassComboBox->AddItem( "#none", NULL );
		for ( int i = 0; i < CItemUploadTF::GetClassCount(); ++i )
		{
			m_pClassComboBox->AddItem( CItemUploadTF::GetClassString( i ), NULL );
		}
		m_pClassComboBox->SilentActivateItemByRow( 0 );
		m_pClassComboBox->AddActionSignalTarget( this );

		m_pPanelListPanel->AddItem( pClassLabel, m_pClassComboBox );
	}

	AddStatusPanels( "class" );

	{
		vgui::Label *pAutoSkinLabel = new vgui::Label( this, "autoSkinLabel", "#autoSkinLabel" );
		m_pAutoSkinCheckButton = new vgui::CheckButton( this, "autoSkinCheckButton", "#autoSkinCheckButtonText" );
		m_pAutoSkinCheckButton->AddActionSignalTarget( this );

		m_pPanelListPanel->AddItem( pAutoSkinLabel, m_pAutoSkinCheckButton );
	}

	AddStatusPanels( "autoSkin" );

	{
		vgui::Label *pSteamLabel = new vgui::Label( this, "steamLabel", "#steamLabel" );
		m_pSteamTextEntry = new vgui::TextEntry( this, "steamTextEntry" );
		m_pSteamTextEntry->SetEditable( false );

		m_pPanelListPanel->AddItem( pSteamLabel, m_pSteamTextEntry );
	}

	AddStatusPanels( "steam" );

	UpdateStatus();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CGlobalSubPanel::UpdateGUI()
{
	CItemUploadWizard *pItemUploadWizard = dynamic_cast< CItemUploadWizard * >( GetWizardPanel() );
	if ( !pItemUploadWizard )
		return;

	m_pNameTextEntry->SetText( pItemUploadWizard->Asset().GetName() );
	const char *pszSteamId = pItemUploadWizard->Asset().GetSteamId();
	if ( CItemUpload::GetDevMode() && ( !pszSteamId || *pszSteamId == '\0' ) )
	{
		m_pSteamTextEntry->SetText( "<dev mode>" );
	}
	else
	{
		m_pSteamTextEntry->SetText( pszSteamId );
	}

	vgui::Menu *pMenu = m_pClassComboBox->GetMenu();
	if ( pMenu )
	{
		bool bFound = false;

		const char *pszClass = pItemUploadWizard->Asset().GetClass();
		for ( int i = 0; i < pMenu->GetItemCount(); ++i )
		{
			vgui::MenuItem *pMenuItem = pMenu->GetMenuItem( i );
			if ( !pMenuItem )
				continue;

			if ( !V_stricmp( pszClass, pMenuItem->GetName() ) )
			{
				m_pClassComboBox->ActivateItemByRow( i );
				bFound = true;
				break;
			}
		}

		if ( !bFound )
		{
			m_pClassComboBox->ActivateItemByRow( 0 );
		}
	}

	m_pAutoSkinCheckButton->SetSelected( pItemUploadWizard->Asset().SkinToBipHead() );

	UpdateStatus();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CGlobalSubPanel::UpdateStatus()
{
	CItemUploadWizard *pItemUploadWizard = dynamic_cast< CItemUploadWizard * >( GetWizardPanel() );
	if ( !pItemUploadWizard )
		return false;

	CAssetTF &asset = pItemUploadWizard->Asset();

	SetStatus( asset.IsNameValid(), "name" );
	SetStatus( asset.IsClassValid(), "class" );
	SetStatus( true, "autoSkin" );
	SetStatus( asset.IsSteamIdValid(), "steam" );

	return BaseClass::UpdateStatus();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CGlobalSubPanel::OnTextChanged( vgui::Panel *pPanel )
{
	CItemUploadWizard *pItemUploadWizard = dynamic_cast< CItemUploadWizard * >( GetWizardPanel() );
	if ( !pItemUploadWizard )
		return;

	char szBuf[ BUFSIZ ];

	if ( pPanel == m_pNameTextEntry )
	{
		m_pNameTextEntry->GetText( szBuf, ARRAYSIZE( szBuf ) );

		if ( !V_strcmp( pItemUploadWizard->Asset().GetName(), szBuf ) )
			return;

		pItemUploadWizard->Asset().SetName( szBuf );

		UpdateStatus();
	}
	else if ( pPanel == m_pClassComboBox )
	{
		m_pClassComboBox->GetText( szBuf, ARRAYSIZE( szBuf ) );

		if ( !V_strcmp( pItemUploadWizard->Asset().GetClass(), szBuf ) )
			return;

		pItemUploadWizard->Asset().SetClass( szBuf );

		UpdateStatus();
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CGlobalSubPanel::OnCheckButtonChecked( vgui::Panel *pPanel )
{
	CItemUploadWizard *pItemUploadWizard = dynamic_cast< CItemUploadWizard * >( GetWizardPanel() );
	if ( !pItemUploadWizard )
		return;

	if ( pPanel != m_pAutoSkinCheckButton )
		return;

	pItemUploadWizard->Asset().SetSkinToBipHead( m_pAutoSkinCheckButton->IsSelected() );
}