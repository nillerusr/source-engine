//========= Copyright Valve Corporation, All rights reserved. ============//
//=============================================================================
//
//=============================================================================


// Standard includes
#define WIN32_LEAN_AND_MEAN
#include <direct.h>
#include <Windows.h>


// Valve includes
#include "itemtest/itemtest_controls.h"
#include "vgui/IVGui.h"
#include "vgui/IInput.h"
#include "vgui/ISystem.h"
#include "vgui/IPanel.h"
#include "vgui_controls/CheckButton.h"
#include "vgui_controls/ComboBox.h"
#include "vgui_controls/FileOpenDialog.h"
#include "vgui_controls/Menu.h"
#include "vgui_controls/MenuItem.h"
#include "vgui_controls/MessageBox.h"
#include "vgui_controls/PanelListPanel.h"
#include "vgui_controls/ScrollBar.h"
#include "vgui_controls/TextImage.h"
#include "tier1/fmtstr.h"


// Local includes
#include "dualpanellist.h"


// Last include
#include <tier0/memdbgon.h>


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CItemUploadDialog *g_pItemUploadDialog = NULL;


//=============================================================================
//
//=============================================================================
CStatusLabel::CStatusLabel( vgui::Panel *pPanel, const char *pszName, bool bValid /* = false */ )
: BaseClass( pPanel, pszName, "" )
, m_bValid( bValid )
, m_cValid( 0, 192, 0, 255 )
, m_cInvalid( 192, 0, 0, 255 )
{
	SetText( m_bValid ? "#valid" : "#invalid" );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CStatusLabel::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	SetContentAlignment( vgui::Label::a_center );

	m_cValid = pScheme->GetColor( "StatusLabel.ValidColor", m_cValid );
	m_cInvalid = pScheme->GetColor( "StatusLabel.InvalidColor", m_cInvalid );

	UpdateColors();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CStatusLabel::SetValid( bool bValid )
{
	if ( bValid == m_bValid )
		return;

	m_bValid = bValid;
	SetText( m_bValid ? "#valid" : "#invalid" );

	UpdateColors();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CStatusLabel::GetValid() const
{
	return m_bValid;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CStatusLabel::UpdateColors()
{
	// TODO: Set valid/invalid colors in scheme .res file...
	if ( GetValid() )
	{
		SetBgColor( m_cValid );
	}
	else
	{
		SetBgColor( m_cInvalid );
	}
}


//=============================================================================
//
//=============================================================================
CItemUploadSubPanel::CItemUploadSubPanel( vgui::Panel *pParent, const char *pszName, const char *pszNextName )
: BaseClass( pParent, pszName )
, m_sNextName( pszNextName )
{
	// Set the Wizard panel if the parent is a Wizard panel (it should be)
	vgui::WizardPanel *pWizardPanel = dynamic_cast< vgui::WizardPanel * >( pParent );
	if ( pWizardPanel )
	{
		SetWizardPanel( pWizardPanel );
	}

	CFmtStr sTmp;

	// Create the two standard widgets

	if ( CItemUpload::GetDevMode() )
	{
		sTmp.sprintf( "#itemtest_wizard_%s_info_dev", GetName() );
		const char *pszCheck = g_pVGuiLocalize->FindAsUTF8( sTmp );

		if ( pszCheck == sTmp.Access() )
		{
			sTmp.Clear();
		}
	}

	if ( sTmp.Length() <= 0 )
	{
		sTmp.sprintf( "#itemtest_wizard_%s_info", GetName() );
	}

	m_pLabel = new vgui::Label( this, "info", sTmp );

	m_pPanelListPanel = new vgui::PanelListPanel( this, "list" );

	m_pStatusLabel = new CStatusLabel( this, "statusLabel", false );
	m_pStatusText = new vgui::Label( this, "statusText", "wonk" );

	sTmp.sprintf( "itemtest_wizard_%s.res", GetName() );

	LoadControlSettings( sTmp );

	m_pLabel->SetAutoResize( PIN_TOPLEFT, AUTORESIZE_RIGHT, 0, 0, 0, 0 );
	m_pLabel->SizeToContents();	// Supposedly doesn't work until layout but kind of does...
	m_pLabel->SetWrap( true );

	m_pStatusLabel->SetAutoResize( PIN_BOTTOMLEFT, AUTORESIZE_NO, 0, 0, 0, 0 );

	if ( GetWizardPanel() && pszNextName == NULL )
	{
		GetWizardPanel()->SetFinishButtonEnabled( false );
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CItemUploadSubPanel::PerformLayout()
{
	BaseClass::PerformLayout();

	int nOldWide = m_pLabel->GetWide();
	m_pLabel->SizeToContents();
	m_pLabel->SetWide( nOldWide );

	int nX = 0;
	int nY = 0;

	m_pLabel->GetPos( nX, nY );

	int nX1 = 0;
	int nY1 = 0;

	m_pStatusLabel->GetPos( nX1, nY1 );

	m_pPanelListPanel->SetBounds( nX, nY + m_pLabel->GetTall() + 10, m_pLabel->GetWide(), nY1 - nY - m_pLabel->GetTall() - 20 );

	bool bDone = false;

	for ( int i = 1; i < m_pPanelListPanel->GetItemCount(); i += 2 )
	{
		vgui::Panel *pPanelA0 = m_pPanelListPanel->GetItemLabel( i - 1 );
		vgui::Panel *pPanelA1 = m_pPanelListPanel->GetItemPanel( i - 1 );
		vgui::Panel *pPanelB0 = m_pPanelListPanel->GetItemLabel( i );
		vgui::Panel *pPanelB1 = m_pPanelListPanel->GetItemPanel( i );

		pPanelA0->SetTall( pPanelA1->GetTall() );
		pPanelB1->SetTall( pPanelB0->GetTall() );

		if ( !bDone )
		{
			bDone = true;
			m_pStatusLabel->SetSize( pPanelA0->GetWide(), pPanelB0->GetTall() );
		}
	}

	m_pStatusLabel->GetPos( nX, nY );
	m_pStatusText->SetPos( nX + m_pStatusLabel->GetWide() + 5, nY );
	m_pStatusText->SetWide( m_pLabel->GetWide() - nX );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CItemUploadSubPanel::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	if ( dynamic_cast< CGlobalSubPanel * >( this ) )
		return;

	if ( dynamic_cast< CGeometrySubPanel * >( this ) )
		return;

	/*
#ifdef _DEBUG

	m_pLabel->SetBgColor( Color( 255, 127, 0, 255 ) );

	m_pPanelListPanel->SetBgColor( Color( 0, 127, 0 ) );

	for ( int i = 1; i < m_pPanelListPanel->GetItemCount(); i += 2 )
	{
		vgui::Panel *pPanel = m_pPanelListPanel->GetItemPanel( i );
		if ( pPanel )
		{
			pPanel->SetBgColor( Color( 0, 0, 255 ) );
		}
	}

#endif // _DEBUG
	*/

}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CItemUploadSubPanel::OnDisplay()
{
	UpdateGUI();
//	UpdateStatus();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
vgui::WizardSubPanel *CItemUploadSubPanel::GetNextSubPanel()
{
	return dynamic_cast< WizardSubPanel * >( GetWizardPanel()->FindChildByName( m_sNextName.Get() ) );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CItemUploadSubPanel::UpdateStatus()
{
	CItemUploadWizard *pItemUploadWizard = dynamic_cast< CItemUploadWizard * >( GetWizardPanel() );
	if ( !pItemUploadWizard )
		return false;

	if ( pItemUploadWizard->GetCurrentItemUploadSubPanel() != this )
		return false;

	CAsset &asset = pItemUploadWizard->Asset();

	CFmtStr sTmp;
	sTmp.sprintf( "#itemtest_wizard_%s_title", GetName() );

	CUtlString sRelativeDir;
	asset.GetRelativeDir( sRelativeDir, NULL );

	const char *pszTitle = g_pVGuiLocalize->FindAsUTF8( sTmp );

	CUtlString sStatusMsg;
	if ( asset.IsOk( sStatusMsg ) && !sRelativeDir.IsEmpty() && pszTitle )
	{
		sTmp.sprintf( "%s : %s", pszTitle, sRelativeDir.Get() );
	}
	else if ( !sStatusMsg.IsEmpty() )
	{
	}

	pItemUploadWizard->SetTitle( sTmp, true );

	bool bValid = true;
	for ( int i = 1; bValid && i < m_pPanelListPanel->GetItemCount(); i += 2 )
	{
		CStatusLabel *pStatusLabel = dynamic_cast< CStatusLabel * >( m_pPanelListPanel->GetItemLabel( i ) );
		if ( !pStatusLabel )
			continue;

		bValid = bValid && pStatusLabel->GetValid();
	}

	m_pStatusLabel->SetValid( bValid );

	sTmp.sprintf( "#itemtest_wizard_%s_%s", GetName(), bValid ? "valid" : "invalid" );
	m_pStatusText->SetText( sTmp );

	pItemUploadWizard->SetNextButtonEnabled( bValid );

	if ( pItemUploadWizard->GetCurrentSubPanel() == this )
	{
		pItemUploadWizard->SetFinishButtonEnabled( m_sNextName.IsEmpty() ? true : false );
	}

	return bValid;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CItemUploadSubPanel::AddStatusPanels( const char *pszPrefix )
{
	CFmtStr sTmp;

	sTmp.sprintf( "%sStatusLabel", pszPrefix );
	CStatusLabel *pStatusLabel = new CStatusLabel( this, sTmp );

	sTmp.sprintf( "%sStatusText", pszPrefix );
	vgui::Label *pStatusText = new vgui::Label( this, sTmp, "" );

	m_pPanelListPanel->AddItem( pStatusLabel, pStatusText );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CItemUploadSubPanel::SetStatus( bool bValid, const char *pszPrefix, const char *pszMessage /* = NULL */, bool bHide /* = false */  )
{
	CFmtStr sTmp;
	sTmp.sprintf( "%sStatusLabel", pszPrefix );

	CStatusLabel *pStatusLabel = dynamic_cast< CStatusLabel * >( m_pPanelListPanel->FindChildByName( sTmp, true ) );
	if ( pStatusLabel )
	{
		pStatusLabel->SetValid( bValid );
		pStatusLabel->SetVisible( !bHide );
	}

	sTmp.sprintf( "%sStatusText", pszPrefix );
	vgui::Label *pStatusText = dynamic_cast< vgui::Label * >( m_pPanelListPanel->FindChildByName( sTmp, true ) );
	if ( pStatusText )
	{
		if ( pszMessage )
		{
			pStatusText->SetText( pszMessage );
		}
		else
		{
			sTmp.sprintf( "#%s%s", pszPrefix, bValid ? "Valid" : "Invalid" );
			pStatusText->SetText( sTmp );
		}

		pStatusText->SetVisible( !bHide );
	}

}


//=============================================================================
//
//=============================================================================
class CFileLocationPanel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CFileLocationPanel, vgui::Panel );

public:

	CFileLocationPanel( vgui::Panel *pParent, int nLodIndex )
	: BaseClass( pParent )
	{
		m_pButtonBrowse = new vgui::Button( this, "BrowseButton", "#BrowseButton", pParent );
		m_pButtonBrowse->SetCommand( new KeyValues( "Open" ) );
		m_pButtonBrowse->AddActionSignalTarget( pParent );

		m_pLabel = new vgui::Label( this, "FileLabel", "" );
	}

	virtual void PerformLayout()
	{
		BaseClass::PerformLayout();

		int w = 0;
		int h = 0;
		GetSize( w, h );

		m_pButtonBrowse->SizeToContents();

		SetTall( m_pButtonBrowse->GetTall() );

		m_pButtonBrowse->SetPos( w - m_pButtonBrowse->GetWide(), 0 );
		m_pLabel->SetSize( w - m_pButtonBrowse->GetWide(), m_pButtonBrowse->GetTall() );
		m_pLabel->SetPos( 0, 0 );
	}

	vgui::Button *m_pButtonBrowse;
	vgui::Label *m_pLabel;

};


//=============================================================================
//
//=============================================================================
class CLODFileLocationPanel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CLODFileLocationPanel, vgui::Panel );

public:

	CLODFileLocationPanel( vgui::Panel *pParent, int nLodIndex )
	: BaseClass( pParent )
	{
		CFmtStr sButtonDelete;
		sButtonDelete.sprintf( "#LOD%dDelete", nLodIndex );

		m_pButtonDelete = new vgui::Button( this, sButtonDelete, sButtonDelete );
		m_pButtonDelete->SetCommand( new KeyValues( "Delete", "nLODIndex", nLodIndex ) );
		m_pButtonDelete->AddActionSignalTarget( pParent );

		CFmtStr sButton;
		sButton.sprintf( "#LOD%dButton", nLodIndex );

		m_pButtonBrowse = new vgui::Button( this, sButton, sButton, pParent );
		m_pButtonBrowse->SetCommand( new KeyValues( "Open", "nLODIndex", nLodIndex ) );
		m_pButtonBrowse->AddActionSignalTarget( pParent );

		CFmtStr sTextEntry;
		sTextEntry.sprintf( "#LOD%dTextEntry", nLodIndex );

		m_pLabel = new vgui::Label( this, sTextEntry, "" );
	}

	virtual void PerformLayout()
	{
		BaseClass::PerformLayout();

		int w = 0;
		int h = 0;
		GetSize( w, h );

		m_pButtonDelete->SizeToContents();
		m_pButtonBrowse->SizeToContents();

		SetTall( m_pButtonBrowse->GetTall() );

		m_pButtonDelete->SetPos( w - m_pButtonDelete->GetWide(), 0 );
		m_pButtonBrowse->SetPos( w - ( m_pButtonDelete->GetWide() + m_pButtonBrowse->GetWide() ), 0 );
		m_pLabel->SetSize( w - ( m_pButtonDelete->GetWide() + m_pButtonBrowse->GetWide() ), m_pButtonBrowse->GetTall() );
		m_pLabel->SetPos( 0, 0 );
	}

	vgui::Button *m_pButtonBrowse;
	vgui::Button *m_pButtonDelete;
	vgui::Label *m_pLabel;

};


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CGeometrySubPanel::CGeometrySubPanel( vgui::Panel *pParent, const char *pszName, const char *pszNextName )
: BaseClass( pParent, pszName, pszNextName )
{
	m_pFileOpenStateMachine = new vgui::FileOpenStateMachine( this, this );
	m_pFileOpenStateMachine->AddActionSignalTarget( this );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CGeometrySubPanel::UpdateGUI()
{
	CItemUploadWizard *pItemUploadWizard = dynamic_cast< CItemUploadWizard * >( GetWizardPanel() );
	if ( !pItemUploadWizard )
		return;

	CAsset &asset = pItemUploadWizard->Asset();

	int nGeometryCount = m_pPanelListPanel->GetItemCount() / 2;

	for ( int i = 0; i < asset.TargetDMXCount(); ++i )
	{
		if ( i >= nGeometryCount )
		{
			AddGeometry();
			nGeometryCount = m_pPanelListPanel->GetItemCount() / 2;
		}

		if ( i >= nGeometryCount )
			break;	// unrecoverable error

		CLODFileLocationPanel *pFileLocationPanel = dynamic_cast< CLODFileLocationPanel * >( m_pPanelListPanel->GetItemPanel( i * 2 ) );
		if ( !pFileLocationPanel )
			continue;

		vgui::Label *pLabel = pFileLocationPanel->m_pLabel;
		if ( !pLabel )
			continue;

		CSmartPtr< CTargetDMX > pTargetDmx = asset.GetTargetDMX( i );
		if ( !pTargetDmx )
			continue;

		pLabel->SetText( pTargetDmx->GetInputFile().Get() );
	}

	// Ensure an empty blank one at the end
	if ( nGeometryCount == asset.TargetDMXCount() )
	{
		AddGeometry();
	}
	else
	{
		// Remove superfluous
		while ( m_pPanelListPanel->GetItemCount() / 2 > ( asset.TargetDMXCount() + 1 ) )
		{
			m_pPanelListPanel->RemoveItem( m_pPanelListPanel->GetItemCount() - 1 );
			m_pPanelListPanel->RemoveItem( m_pPanelListPanel->GetItemCount() - 1 );
		}

		// Set last one to empty
		if ( ( m_pPanelListPanel->GetItemCount() / 2 ) == ( asset.TargetDMXCount() + 1 ) )
		{
			CLODFileLocationPanel *pFileLocationPanel = dynamic_cast< CLODFileLocationPanel * >( m_pPanelListPanel->GetItemPanel( m_pPanelListPanel->GetItemCount() - 2 ) );
			if ( pFileLocationPanel )
			{
				vgui::Label *pLabel = pFileLocationPanel->m_pLabel;
				if ( pLabel )
				{
					pLabel->SetText( "" );
				}
			}
		}
	}

	UpdateStatus();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CGeometrySubPanel::UpdateStatus()
{
	CItemUploadWizard *pItemUploadWizard = dynamic_cast< CItemUploadWizard * >( GetWizardPanel() );
	if ( !pItemUploadWizard )
		return false;

	CAsset &asset = pItemUploadWizard->Asset();

	CFmtStr sTmp;
	CFmtStr sErrString;

	int nLODIndex = 0;
	int nThisPolyCount = 0;
	int nLastPolyCount = 0;

	for ( int i = 0; i < m_pPanelListPanel->GetItemCount(); i += 2, ++nLODIndex )
	{
		CLODFileLocationPanel *pFileLocationPanel = dynamic_cast< CLODFileLocationPanel * >( m_pPanelListPanel->GetItemPanel( i ) );
		if ( !pFileLocationPanel )
			continue;

		vgui::Label *pLabel = pFileLocationPanel->m_pLabel;
		if ( !pLabel )
			continue;

		if ( i == ( m_pPanelListPanel->GetItemCount() - 4 ) )
		{
			pFileLocationPanel->m_pButtonDelete->SetEnabled( true );
		}
		else
		{
			pFileLocationPanel->m_pButtonDelete->SetEnabled( false );
		}

		sTmp.sprintf( "LOD%d", nLODIndex );

		if ( i < ( m_pPanelListPanel->GetItemCount() - 2 ) )
		{
			CSmartPtr< CTargetDMX > pTargetDMX = asset.GetTargetDMX( nLODIndex );

			if ( !pTargetDMX )
			{
				sErrString.sprintf( "Invalid Geometry: LOD %d is NULL", nLODIndex );
				SetStatus( false, sTmp, sErrString );
				continue;
			}

			CUtlString sStatusMsg;
			if ( !pTargetDMX->IsOk( sStatusMsg ) )
			{
				SetStatus( false, sStatusMsg.Get() );
				continue;
			}

			nLastPolyCount = nThisPolyCount;
			nThisPolyCount = pTargetDMX->GetPolyCount();

			if ( nThisPolyCount <= 0 )
			{
				sErrString.sprintf( "LOD %d has bad polygon count: %d", i, nThisPolyCount );
				SetStatus( false, sTmp, sErrString );
				continue;
			}

			if ( ( i > 0 ) && ( nThisPolyCount == nLastPolyCount ) )
			{
				sErrString.sprintf( "LOD %d (%d polys) has the same number of polygons as previous LOD %d (%d polys)",
					nLODIndex, nThisPolyCount, nLODIndex - 1, nLastPolyCount );
				SetStatus( false, sTmp, sErrString );
				continue;
			}

			if ( ( i > 0 ) && ( nThisPolyCount >= nLastPolyCount ) )
			{
				sErrString.sprintf( "LOD %d (%d polys) has more polygons than previous LOD %d (%d polys)",
					nLODIndex, nThisPolyCount, nLODIndex - 1, nLastPolyCount );
				SetStatus( false, sTmp, sErrString );
				continue;
			}

			SetStatus( true, sTmp );
		}
		else
		{
			SetStatus( true, sTmp, NULL, true );
		}
	}

	bool bValid = BaseClass::UpdateStatus();

	// In this case there are more validation checks to be performed

	// Ensure each LOD is non-empty
	if ( bValid )
	{
		sErrString.Clear();

		// Ensure at least two LODs
		if ( CItemUpload::GetDevMode() )
		{
			if ( asset.TargetDMXCount() < 1 )
			{
				bValid = false;
				sErrString.sprintf( "At least 1 LOD is required" );
			}
		}
		else if ( asset.TargetDMXCount() < 2 )
		{
			bValid = false;
			sErrString.sprintf( "At least 2 LODs are required" );
		}

		// Any other overall checks can go here

		m_pStatusLabel->SetValid( bValid );

		if ( !bValid )
		{
			// Not sure how to translate this message with parameters, etc...
			m_pStatusText->SetText( sErrString );
		}

		pItemUploadWizard->SetNextButtonEnabled( bValid );
	}

	return bValid;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CGeometrySubPanel::SetupFileOpenDialog(
	vgui::FileOpenDialog *pDialog,
	bool bOpenFile,
	const char *pszFileName,
	KeyValues *pContextKeyValues )
{
	char pszStartingDir[ MAX_PATH ];
	if ( !vgui::system()->GetRegistryString(
		"HKEY_CURRENT_USER\\Software\\Valve\\itemtest\\geometry\\opendir",
		pszStartingDir, sizeof( pszStartingDir ) ) )
	{
		_getcwd( pszStartingDir, ARRAYSIZE( pszStartingDir ));

		CUtlString sVMod;
		CUtlString sContentDir;
		if ( CItemUpload::GetVMod( sVMod ) && !sVMod.IsEmpty() && CItemUpload::GetContentDir( sContentDir ) && !sContentDir.IsEmpty() && CItemUpload::FileExists( sContentDir.Get() ) )
		{
			sContentDir += "/";
			sContentDir += sVMod;
			sContentDir += "/models";

			CUtlString sTmp = sContentDir;
			sTmp += "/player";

			if ( CItemUpload::FileExists( sTmp.Get() ) )
			{
				sContentDir = sTmp;
				sTmp += "/items";

				if ( CItemUpload::FileExists( sTmp.Get() ) )
				{
					CItemUploadWizard *pItemUploadWizard = dynamic_cast< CItemUploadWizard * >( GetWizardPanel() );
					if ( pItemUploadWizard )
					{
						CAsset &asset = pItemUploadWizard->Asset();

						sContentDir = sTmp;
						sTmp += "/";
						sTmp += asset.GetClass();
					}

					// TODO: Add steam id?

					V_FixupPathName( pszStartingDir, ARRAYSIZE( pszStartingDir ), sTmp.Get() );
				}
			}
		}
	}

	pDialog->SetStartDirectoryContext( "itemtest_geometry_browser", pszStartingDir );

	if ( bOpenFile )
	{
		pDialog->AddFilter( "*.obj", "OBJ File (*.obj)", true, "obj" );
		pDialog->AddFilter( "*.smd", "Valve SMD File (*.smd)", false, "smd" );
		pDialog->AddFilter( "*.dmx", "Valve DMX File (*.dmx)", false, "dmx" );
		pDialog->AddFilter( "*.*", "All Files (*.*)", false, "geometry" );

		pDialog->SetTitle( "Open Geometry ( OBJ/SMD/DMX ) File", true );
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CGeometrySubPanel::OnReadFileFromDisk(
	const char *pszFileName,
	const char *pszFileFormat,
	KeyValues *pContextKeyValues )
{
	CItemUploadWizard *pItemUploadWizard = dynamic_cast< CItemUploadWizard * >( GetWizardPanel() );
	if ( !pItemUploadWizard )
		return false;

	const int nLODIndex = pContextKeyValues->GetInt( "nLODIndex", -1 );
	if ( nLODIndex < 0 )
		return false;

	char szBuf0[ MAX_PATH ];
	char szBuf1[ MAX_PATH ];

	// Extract path and save to registry to open browser there next time
	{
		V_strncpy( szBuf0, pszFileName, sizeof( szBuf0 ) );
		V_FixSlashes( szBuf0 );
		V_StripFilename( szBuf0 );

		_fullpath( szBuf1, szBuf0, ARRAYSIZE( szBuf1 ) );

		vgui::system()->SetRegistryString(
			"HKEY_CURRENT_USER\\Software\\Valve\\itemtest\\geometry\\opendir",
			szBuf1 );
	}

	// Get the full path
	_fullpath( szBuf1, pszFileName, ARRAYSIZE( szBuf1 ) );

	CAsset &asset = pItemUploadWizard->Asset();

	for ( int i = 0; i < asset.TargetDMXCount(); ++i )
	{
		CSmartPtr< CTargetDMX > pTargetDMX = asset.GetTargetDMX( i );
		if ( !pTargetDMX )
			continue;

		if ( !V_strcmp( pTargetDMX->GetInputFile().Get(), szBuf1 ) )
		{
			vgui::MessageBox *pMessageBox = new vgui::MessageBox( "#duplicate_file_title", "#duplicate_file_text", this );
			if ( pMessageBox )
			{
				pMessageBox->SetSize( 640, 480 );
				pMessageBox->SetMinimumSize( 320, 120 );

				pMessageBox->DoModal();
			}

			return false;
		}
	}

	bool bRet = false;

	if ( nLODIndex < asset.TargetDMXCount() )
	{
		bRet = asset.SetTargetDMX( nLODIndex, szBuf1 );
	}
	else if ( nLODIndex == asset.TargetDMXCount() )
	{
		const int nCheck = asset.AddTargetDMX( szBuf1 );
		Assert( nCheck == nLODIndex );
		bRet = ( nCheck >= 0 );
	}

	UpdateGUI();

	return bRet;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CGeometrySubPanel::OnWriteFileToDisk(
	const char *pszFileName,
	const char *pszFileFormat,
	KeyValues *pContextKeyValues )
{
	return false;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CGeometrySubPanel::OnOpen( int nLodIndex )
{
	KeyValues *pContextKeyValues = new KeyValues( "FileOpen", "nLODIndex", nLodIndex );
	m_pFileOpenStateMachine->OpenFile( "geometry", pContextKeyValues );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CGeometrySubPanel::OnDelete( int nLodIndex )
{
	CItemUploadWizard *pItemUploadWizard = dynamic_cast< CItemUploadWizard * >( GetWizardPanel() );
	if ( !pItemUploadWizard )
		return;

	CAsset &asset = pItemUploadWizard->Asset();

	asset.RemoveTargetDMX( nLodIndex );

	UpdateGUI();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CGeometrySubPanel::AddGeometry()
{
	const int nLodIndex = m_pPanelListPanel->GetItemCount() / 2;

	CFmtStr sLabel;
	sLabel.sprintf( "#LOD%dLabel", nLodIndex );

	vgui::Label *pLabel = new vgui::Label( this, sLabel, sLabel );
	pLabel->SetContentAlignment( vgui::Label::a_center );

	CLODFileLocationPanel *pFileLocationPanel = new CLODFileLocationPanel( this, nLodIndex );
	m_pPanelListPanel->AddItem( pLabel, pFileLocationPanel );
	pFileLocationPanel->m_pLabel->AddActionSignalTarget( this );
	pFileLocationPanel->m_pButtonBrowse->AddActionSignalTarget( this );

	sLabel.sprintf( "lod%d", nLodIndex );
	AddStatusPanels( sLabel );
}


//=============================================================================
//
//=============================================================================
class CVmtEntry : public CDualPanelList
{
	DECLARE_CLASS_SIMPLE( CVmtEntry, CDualPanelList );
public:

	MESSAGE_FUNC_PTR( OnTextChanged, "TextChanged", panel );
	MESSAGE_FUNC_PTR( OnOpen, "Open", panel );

	CVmtEntry( CMaterialSubPanel *pMaterialSubPanel, const char *pszName, int nVmtIndex )
	: CDualPanelList( pMaterialSubPanel, pszName )
	, m_pMaterialSubPanel( pMaterialSubPanel )
	, m_nVmtIndex( nVmtIndex )
	{
		{
			vgui::Label *pMaterialLabel = new vgui::Label( this, "MaterialLabel", "#MaterialLabel" );
			m_pMaterialName = new vgui::Label( this, "MaterialName", "#MaterialName" );

			AddItem( pMaterialLabel, m_pMaterialName );
		}

		{
			vgui::Label *pMaterialTypeLabel = new vgui::Label( this, "MaterialTypeLabel", "#MaterialTypeLabel" );
			m_pMaterialType = new vgui::ComboBox( this, "MaterialTypeComboBox", 0, false );
			m_pMaterialType->AddItem( "#Invalid", new KeyValues( "Invalid" ) );
			m_pMaterialType->AddItem( "#Primary", new KeyValues( "Primary" ) );
			m_pMaterialType->AddItem( "#Secondary", new KeyValues( "Secondary" ) );
			m_pMaterialType->AddItem( "#DuplicateOfPrimary", new KeyValues( "DuplicateOfPrimary" ) );
			m_pMaterialType->AddItem( "#DuplicateOfSecondary", new KeyValues( "DuplicateOfSecondary" ) );
			m_pMaterialType->AddActionSignalTarget( this );

			AddItem( pMaterialTypeLabel, m_pMaterialType );
		}

		{
			vgui::Label *pRedBlueLabel = new vgui::Label( this, "CommonRedBlueLabel", "#CommonRedBlueLabel" );
			m_pCommonRedBlue = new vgui::ComboBox( this, "CommonRedBlue", 0, false );
			m_pCommonRedBlue->AddItem( "#Common", new KeyValues( "Common" ) );
			m_pCommonRedBlue->AddItem( "#RedAndBlue", new KeyValues( "RedAndBlue" ) );
			m_pCommonRedBlue->AddActionSignalTarget( this );

			m_nCommonRedBlueId = AddItem( pRedBlueLabel, m_pCommonRedBlue );
		}

		{
			vgui::Label *pCommonTextureLabel = new vgui::Label( this, "CommonTextureLabel", "#CommonTextureLabel" );
			m_pCommonTextureFileLocation = new CFileLocationPanel( this, 0 );

			m_nCommonId = AddItem( pCommonTextureLabel, m_pCommonTextureFileLocation );
		}

		{
			vgui::Label *pRedTextureLabel = new vgui::Label( this, "RedTextureLabel", "#RedTextureLabel" );
			m_pRedTextureFileLocation = new CFileLocationPanel( this, 1 );

			m_nRedId = AddItem( pRedTextureLabel, m_pRedTextureFileLocation );
		}

		{
			vgui::Label *pBlueTextureLabel = new vgui::Label( this, "BlueTextureLabel", "#BlueTextureLabel" );
			m_pBlueTextureFileLocation = new CFileLocationPanel( this, 1 );

			m_nBlueId = AddItem( pBlueTextureLabel, m_pBlueTextureFileLocation );
		}

		{
			vgui::Label *pColorAlphaLabel = new vgui::Label( this, "ColorAlphaLabel", "#ColorAlphaLabel" );

			m_pColorAlpha = new vgui::ComboBox( this, "ColorAlphaComboBox", 0, false );
			m_pColorAlpha->AddItem( "#Nothing", new KeyValues( "None" ) );
			m_pColorAlpha->AddItem( "#Transparency", new KeyValues( "Transparency" ) );
			m_pColorAlpha->AddItem( "#Paintable", new KeyValues( "Paintable" ) );
			m_pColorAlpha->AddItem( "#SpecPhong", new KeyValues( "SpecPhong" ) );
			m_pColorAlpha->AddActionSignalTarget( this );

			m_nColorAlphaId = AddItem( pColorAlphaLabel, m_pColorAlpha );
		}

		{
			vgui::Label *pNormalMapLabel = new vgui::Label( this, "NormalMapLabel", "#NormalMapLabel" );
			m_pNormalTextureFileLocation = new CFileLocationPanel( this, 2 );

			m_nNormalId = AddItem( pNormalMapLabel, m_pNormalTextureFileLocation );
		}

		{
			vgui::Label *pNormalAlphaLabel = new vgui::Label( this, "NormalAlphaLabel", "#NormalAlphaLabel" );

			m_pNormalAlpha = new vgui::ComboBox( this, "NormalAlphaComboBox", 0, false );
			m_pNormalAlpha->AddItem( "#Nothing", new KeyValues( "None" ) );
			m_pNormalAlpha->AddItem( "#SpecPhong", new KeyValues( "SpecPhong" ) );
			m_pNormalAlpha->AddActionSignalTarget( this );

			m_nNormalAlphaId = AddItem( pNormalAlphaLabel, m_pNormalAlpha );
		}

		m_pCommonRedBlue->ActivateItem( 0 );
		SetItemVisible( m_nColorAlphaId, false );
		m_pColorAlpha->SilentActivateItem( 0 );
		SetItemVisible( m_nNormalAlphaId, false );
		m_pNormalAlpha->SilentActivateItem( 0 );
	}

	int m_nCommonRedBlueId;
	int m_nCommonId;
	int m_nRedId;
	int m_nBlueId;
	int m_nColorAlphaId;
	int m_nNormalId;
	int m_nNormalAlphaId;

	vgui::Label *m_pMaterialName;
	vgui::ComboBox *m_pMaterialType;
	vgui::ComboBox *m_pCommonRedBlue;
	CFileLocationPanel *m_pCommonTextureFileLocation;
	CFileLocationPanel *m_pRedTextureFileLocation;
	CFileLocationPanel *m_pBlueTextureFileLocation;
	vgui::ComboBox *m_pColorAlpha;
	CFileLocationPanel *m_pNormalTextureFileLocation;
	vgui::ComboBox *m_pNormalAlpha;

	CMaterialSubPanel *m_pMaterialSubPanel;

	int m_nVmtIndex;

	void SetMaterialId( const char *pszMaterialName )
	{
		if ( !m_pMaterialName )
			return;

		m_pMaterialName->SetText( pszMaterialName );
	}

};


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CVmtEntry::OnTextChanged( vgui::Panel *pPanel )
{
	CTargetVMT *pTargetVMT = m_pMaterialSubPanel->GetTargetVMT( m_nVmtIndex );
	if ( !pTargetVMT )
		return;

	bool bUpdate = false;

	if ( pPanel == m_pMaterialType )
	{
		KeyValues *pUserData = m_pMaterialType->GetActiveItemUserData();
		if ( pUserData )
		{
			if ( !V_strcmp( "Invalid", pUserData->GetName() ) )
			{
				pTargetVMT->SetMaterialType( CTargetVMT::kInvalidMaterialType );
			}
			else if ( !V_strcmp( "Primary", pUserData->GetName() ) )
			{
				pTargetVMT->SetMaterialType( CTargetVMT::kPrimary );
			}
			else if ( !V_strcmp( "Secondary", pUserData->GetName() ) )
			{
				pTargetVMT->SetMaterialType( CTargetVMT::kSecondary );
			}
			else if ( !V_strcmp( "DuplicateOfPrimary", pUserData->GetName() ) )
			{
				pTargetVMT->SetDuplicate( CTargetVMT::kPrimary );
			}
			else if ( !V_strcmp( "DuplicateOfSecondary", pUserData->GetName() ) )
			{
				pTargetVMT->SetDuplicate( CTargetVMT::kSecondary );
			}
			else
			{
				AssertMsg1( 0, "Unknown Material Type: %s\n", pUserData->GetName() );
			}

			bUpdate = true;
		}
	}
	else if ( pPanel == m_pCommonRedBlue )
	{
		KeyValues *pUserData = m_pCommonRedBlue->GetActiveItemUserData();
		if ( pUserData )
		{
			const bool bCommon = !V_strcmp( "Common", pUserData->GetName() );

			pTargetVMT->SetColorMapCommon( bCommon );

			bUpdate = true;
		}
	}
	else if ( pPanel == m_pColorAlpha )
	{
		KeyValues *pUserData = m_pColorAlpha->GetActiveItemUserData();
		if ( pUserData )
		{
			const char *pszUserData = pUserData->GetName();

			if ( StringHasPrefix( pszUserData, "T" ) )
			{
				pTargetVMT->SetColorAlphaType( CTargetVMT::kTransparency );
			}
			else if ( StringHasPrefix( pszUserData, "P" ) )
			{
				pTargetVMT->SetColorAlphaType( CTargetVMT::kPaintable );
			}
			else if ( StringHasPrefix( pszUserData, "S" ) )
			{
				pTargetVMT->SetColorAlphaType( CTargetVMT::kColorSpecPhong );
			}
			else
			{
				pTargetVMT->SetColorAlphaType( CTargetVMT::kNoColorAlpha );
			}

			bUpdate = true;
		}
	}
	else if ( pPanel == m_pNormalAlpha )
	{
		KeyValues *pUserData = m_pNormalAlpha->GetActiveItemUserData();
		if ( pUserData )
		{
			const char *pszUserData = pUserData->GetName();

			if ( StringHasPrefix( pszUserData, "S" ) )
			{
				pTargetVMT->SetNormalAlphaType( CTargetVMT::kNormalSpecPhong );
			}
			else
			{
				pTargetVMT->SetNormalAlphaType( CTargetVMT::kNoNormalAlpha );
			}

			bUpdate = true;
		}
	}

	if ( bUpdate )
	{
		InvalidateLayout();
		m_pMaterialSubPanel->InvalidateLayout();
		m_pMaterialSubPanel->UpdateGUI();
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CVmtEntry::OnOpen( vgui::Panel *pPanel )
{
	if ( !m_pMaterialSubPanel )
		return;

	if ( pPanel == m_pCommonTextureFileLocation->m_pButtonBrowse )
	{
		m_pMaterialSubPanel->Browse( this, CMaterialSubPanel::kCommon );
	}
	else if ( pPanel == m_pRedTextureFileLocation->m_pButtonBrowse )
	{
		m_pMaterialSubPanel->Browse( this, CMaterialSubPanel::kRed );
	}
	else if ( pPanel == m_pBlueTextureFileLocation->m_pButtonBrowse )
	{
		m_pMaterialSubPanel->Browse( this, CMaterialSubPanel::kBlue );
	}
	else if ( pPanel == m_pNormalTextureFileLocation->m_pButtonBrowse )
	{
		m_pMaterialSubPanel->Browse( this, CMaterialSubPanel::kNormal );
	}
}


//=============================================================================
//
//=============================================================================
CMaterialSubPanel::CMaterialSubPanel( vgui::Panel *pParent, const char *pszName, const char *pszNextName )
: BaseClass( pParent, pszName, pszNextName )
{
	m_pFileOpenStateMachine = new vgui::FileOpenStateMachine( this, this );
	m_pFileOpenStateMachine->AddActionSignalTarget( this );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CMaterialSubPanel::InvalidateLayout()
{
	BaseClass::InvalidateLayout();
	m_pPanelListPanel->InvalidateLayout();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CMaterialSubPanel::UpdateGUI()
{
	CItemUploadWizard *pItemUploadWizard = dynamic_cast< CItemUploadWizard * >( GetWizardPanel() );
	if ( !pItemUploadWizard )
		return;

	CAsset &asset = pItemUploadWizard->Asset();

	int nMaterialCount = m_pPanelListPanel->GetItemCount() / 2;

	CFmtStr sTmp;

	// Add new gui elements & update existing
	for ( int i = 0; i < asset.GetTargetVMTCount(); ++i )
	{
		if ( i >= nMaterialCount )
		{
			AddMaterial();
			nMaterialCount = m_pPanelListPanel->GetItemCount() / 2;
		}

		if ( i >= nMaterialCount )
			break;	// unrecoverable error

		CTargetVMT *pTargetVmt = asset.GetTargetVMT( i );
		Assert( pTargetVmt );

		if ( !pTargetVmt )
			continue;

		CVmtEntry *pVmtEntry = dynamic_cast< CVmtEntry * >( m_pPanelListPanel->GetItemPanel( i * 2 ) );
		if ( !pVmtEntry )
			continue;

		CUtlString sMaterialId;
		pTargetVmt->GetMaterialId( sMaterialId );
		pVmtEntry->SetMaterialId( sMaterialId.Get() );

		const bool bCommon = pTargetVmt->GetColorMapCommon();

		pVmtEntry->m_pCommonRedBlue->SilentActivateItemByRow( bCommon ? 0 : 1 );

		bool bVisible = true;

		if ( pTargetVmt->GetDuplicate() )
		{
			bVisible = false;

			switch ( pTargetVmt->GetMaterialType() )
			{
			case CTargetVMT::kInvalidMaterialType:
				pVmtEntry->m_pMaterialType->SilentActivateItemByRow( 0 );
				break;
			case CTargetVMT::kPrimary:
				pVmtEntry->m_pMaterialType->SilentActivateItemByRow( 3 );
				break;
			case CTargetVMT::kSecondary:
				pVmtEntry->m_pMaterialType->SilentActivateItemByRow( 4 );
				break;
			default:
				pVmtEntry->m_pMaterialType->ActivateItem( 0 );
				break;
			}
		}
		else
		{
			switch ( pTargetVmt->GetMaterialType() )
			{
			case CTargetVMT::kInvalidMaterialType:
				pVmtEntry->m_pMaterialType->SilentActivateItemByRow( 0 );
				break;
			case CTargetVMT::kPrimary:
				pVmtEntry->m_pMaterialType->SilentActivateItemByRow( 1 );
				break;
			case CTargetVMT::kSecondary:
				pVmtEntry->m_pMaterialType->SilentActivateItemByRow( 2 );
				break;
			default:
				pVmtEntry->m_pMaterialType->ActivateItem( 0 );
				break;
			}
		}

		pVmtEntry->SetItemVisible( pVmtEntry->m_nCommonRedBlueId, bVisible );
		pVmtEntry->SetItemVisible( pVmtEntry->m_nCommonId, bVisible && bCommon );
		pVmtEntry->SetItemVisible( pVmtEntry->m_nRedId, bVisible && !bCommon );
		pVmtEntry->SetItemVisible( pVmtEntry->m_nBlueId, bVisible && !bCommon );
		pVmtEntry->SetItemVisible( pVmtEntry->m_nNormalId, bVisible );

		bool bColorVisible = false;

		CSmartPtr< CTargetVTF > pCommonTargetVTF = pTargetVmt->GetCommonTargetVTF();
		if ( pCommonTargetVTF.IsValid() )
		{
			pVmtEntry->m_pCommonTextureFileLocation->m_pLabel->SetText( pCommonTargetVTF->GetInputFile() );
			if ( pTargetVmt->GetColorMapCommon() && pCommonTargetVTF->HasAlpha() )
			{
				bColorVisible = true;
			}
		}
		else
		{
			pVmtEntry->m_pCommonTextureFileLocation->m_pLabel->SetText( "" );
			bColorVisible = false;
		}

		switch ( pTargetVmt->GetColorAlphaType() )
		{
		case CTargetVMT::kNoColorAlpha:
			pVmtEntry->m_pColorAlpha->SilentActivateItemByRow( CTargetVMT::kNoColorAlpha );
			break;
		case CTargetVMT::kTransparency:
			pVmtEntry->m_pColorAlpha->SilentActivateItemByRow( CTargetVMT::kTransparency );
			break;
		case CTargetVMT::kPaintable:
			pVmtEntry->m_pColorAlpha->SilentActivateItemByRow( CTargetVMT::kPaintable );
			break;
		case CTargetVMT::kColorSpecPhong:
			pVmtEntry->m_pColorAlpha->SilentActivateItemByRow( CTargetVMT::kColorSpecPhong );
			break;
		default:
			pVmtEntry->m_pColorAlpha->SilentActivateItemByRow( CTargetVMT::kNoColorAlpha );
			break;
		}

		CSmartPtr< CTargetVTF > pRedTargetVTF = pTargetVmt->GetRedTargetVTF();
		CSmartPtr< CTargetVTF > pBlueTargetVTF = pTargetVmt->GetBlueTargetVTF();
		if ( pRedTargetVTF.IsValid() )
		{
			pVmtEntry->m_pRedTextureFileLocation->m_pLabel->SetText( pRedTargetVTF->GetInputFile() );
		}
		else
		{
			pVmtEntry->m_pRedTextureFileLocation->m_pLabel->SetText( "" );
		}

		if ( pBlueTargetVTF.IsValid() )
		{
			pVmtEntry->m_pBlueTextureFileLocation->m_pLabel->SetText( pBlueTargetVTF->GetInputFile() );
		}
		else
		{
			pVmtEntry->m_pBlueTextureFileLocation->m_pLabel->SetText( "" );
		}

		if ( !pTargetVmt->GetColorMapCommon() && pRedTargetVTF.IsValid() && pBlueTargetVTF.IsValid() )
		{
			if ( pRedTargetVTF->HasAlpha() && pBlueTargetVTF->HasAlpha() )
			{
				bColorVisible = true;
			}
		}

		pVmtEntry->SetItemVisible( pVmtEntry->m_nColorAlphaId, bVisible && bColorVisible );

		bool bNormalVisible = false;

		CSmartPtr< CTargetVTF > pNormalTargetVTF = pTargetVmt->GetNormalTargetVTF();
		if ( pNormalTargetVTF.IsValid() )
		{
			pVmtEntry->m_pNormalTextureFileLocation->m_pLabel->SetText( pNormalTargetVTF->GetInputFile() );
			if ( pNormalTargetVTF->HasAlpha() )
			{
				bNormalVisible = true;
			}
		}

		switch ( pTargetVmt->GetNormalAlphaType() )
		{
		case CTargetVMT::kNoNormalAlpha:
			pVmtEntry->m_pNormalAlpha->SilentActivateItemByRow( CTargetVMT::kNoNormalAlpha );
			break;
		case CTargetVMT::kNormalSpecPhong:
			pVmtEntry->m_pNormalAlpha->SilentActivateItemByRow( CTargetVMT::kNormalSpecPhong );
			break;
		default:
			pVmtEntry->m_pNormalAlpha->SilentActivateItemByRow( CTargetVMT::kNoNormalAlpha );
			break;
		}

		pVmtEntry->SetItemVisible( pVmtEntry->m_nNormalAlphaId, bVisible && bNormalVisible );

		pVmtEntry->InvalidateLayout();
	}

	// Remove superfluous
	while ( ( m_pPanelListPanel->GetItemCount() / 2 ) > asset.GetTargetVMTCount() )
	{
		m_pPanelListPanel->RemoveItem( m_pPanelListPanel->GetItemCount() - 1 );
		m_pPanelListPanel->RemoveItem( m_pPanelListPanel->GetItemCount() - 1 );
	}

	InvalidateLayout();

	UpdateStatus();
}


//-----------------------------------------------------------------------------
// TODO: Set status
//-----------------------------------------------------------------------------
bool CMaterialSubPanel::UpdateStatus()
{
	CFmtStr sTmp;

	int nIndex = 0;
	for ( int i = 0; i < m_pPanelListPanel->GetItemCount(); i += 2, ++nIndex )
	{
		CVmtEntry *pVmtEntry = dynamic_cast< CVmtEntry * >( m_pPanelListPanel->GetItemPanel( i ) );
		if ( !pVmtEntry )
			continue;

		CTargetVMT *pTargetVMT = GetTargetVMT( pVmtEntry->m_nVmtIndex );
		Assert( pTargetVMT );
		if ( !pTargetVMT )
			continue;

		sTmp.sprintf( "VMT%d", nIndex );

		CUtlString sMsg;
		if ( pTargetVMT->IsOk( sMsg ) )
		{
			SetStatus( true, sTmp );
			continue;
		}

		if ( sMsg.IsEmpty() )
		{
			SetStatus( false, sTmp, "VMT is not valid\n" );
		}
		else
		{
			SetStatus( false, sTmp, sMsg.Get() );
		}
	}

	bool bValid = BaseClass::UpdateStatus();

	return bValid;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CMaterialSubPanel::SetupFileOpenDialog(
	vgui::FileOpenDialog *pDialog,
	bool bOpenFile,
	const char *pszFileName,
	KeyValues *pContextKeyValues )
{
	char pszStartingDir[ MAX_PATH ];

	if ( !vgui::system()->GetRegistryString(
		"HKEY_CURRENT_USER\\Software\\Valve\\itemtest\\texture\\opendir",
		pszStartingDir, sizeof( pszStartingDir ) ) )
	{
		_getcwd( pszStartingDir, ARRAYSIZE( pszStartingDir ));

		CUtlString sVMod;
		CUtlString sContentDir;
		if ( CItemUpload::GetVMod( sVMod ) && !sVMod.IsEmpty() && CItemUpload::GetContentDir( sContentDir ) && !sContentDir.IsEmpty() && CItemUpload::FileExists( sContentDir.Get() ) )
		{
			sContentDir += "/";
			sContentDir += sVMod;
			sContentDir += "/materialsrc/models";

			CUtlString sTmp = sContentDir;
			sTmp += "/player";

			if ( CItemUpload::FileExists( sTmp.Get() ) )
			{
				sContentDir = sTmp;
				sTmp += "/items";

				if ( CItemUpload::FileExists( sTmp.Get() ) )
				{
					CItemUploadWizard *pItemUploadWizard = dynamic_cast< CItemUploadWizard * >( GetWizardPanel() );
					if ( pItemUploadWizard )
					{
						CAsset &asset = pItemUploadWizard->Asset();

						sContentDir = sTmp;
						sTmp += "/";
						sTmp += asset.GetClass();
					}

					// TODO: Add steam id?

					V_FixupPathName( pszStartingDir, ARRAYSIZE( pszStartingDir ), sTmp.Get() );
				}
			}
		}
	}

	pDialog->SetStartDirectoryContext( "itemtest_texture_browser", pszStartingDir );

	// TODO: Remember the mask the user likes
	if ( bOpenFile )
	{
		pDialog->AddFilter( "*.tga", "Targa TrueVision File (*.tga)", true, "tga" );
		pDialog->AddFilter( "*.psd", "Photoshop Document (*.psd)", false, "psd" );

		pDialog->SetTitle( "Open Texture File", true );
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CMaterialSubPanel::OnReadFileFromDisk(
	const char *pszFileName,
	const char *pszFileFormat,
	KeyValues *pContextKeyValues )
{
	CItemUploadWizard *pItemUploadWizard = dynamic_cast< CItemUploadWizard * >( GetWizardPanel() );
	if ( !pItemUploadWizard )
		return false;

	char szBuf0[ MAX_PATH ];
	char szBuf1[ MAX_PATH ];

	// Extract path and save to registry to open browser there next time
	{
		V_strncpy( szBuf0, pszFileName, sizeof( szBuf0 ) );
		V_FixSlashes( szBuf0 );
		V_StripFilename( szBuf0 );

		_fullpath( szBuf1, szBuf0, ARRAYSIZE( szBuf1 ) );

		vgui::system()->SetRegistryString(
			"HKEY_CURRENT_USER\\Software\\Valve\\itemtest\\texture\\opendir",
			szBuf1 );
	}

	// Get the full path
	_fullpath( szBuf1, pszFileName, ARRAYSIZE( szBuf1 ) );

	CAsset &asset = pItemUploadWizard->Asset();

	CVmtEntry *pVmtEntry = reinterpret_cast< CVmtEntry * >( pContextKeyValues->GetPtr( "pVmtEntry" ) );
	const Browse_t nBrowseType = static_cast< Browse_t >( pContextKeyValues->GetInt( "nBrowseType" ) );

	bool bReturnVal = false;

	for ( int i = 0; i < m_pPanelListPanel->GetItemCount(); i += 2 )
	{
		if ( pVmtEntry == dynamic_cast< CVmtEntry * >( m_pPanelListPanel->GetItemPanel( i ) ) )
		{
			const int nVmtIndex = i / 2;
			CTargetVMT *pTargetVMT = asset.GetTargetVMT( nVmtIndex );
			if ( pTargetVMT )
			{
				bReturnVal = true;

				switch( nBrowseType )
				{
				case CMaterialSubPanel::kCommon:
					pTargetVMT->SetCommonTargetVTF( szBuf1 );
					break;
				case CMaterialSubPanel::kRed:
					pTargetVMT->SetRedTargetVTF( szBuf1 );
					break;
				case CMaterialSubPanel::kBlue:
					pTargetVMT->SetBlueTargetVTF( szBuf1 );
					break;
				case CMaterialSubPanel::kNormal:
					pTargetVMT->SetNormalTargetVTF( szBuf1 );
					break;
				default:
					bReturnVal = false;
					break;
				}
			}
			break;
		}
	}

	UpdateGUI();

	return bReturnVal;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CMaterialSubPanel::OnWriteFileToDisk(
	const char *pszFileName,
	const char *pszFileFormat,
	KeyValues *pContextKeyValues )
{
	return false;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CMaterialSubPanel::Browse( CVmtEntry *pVmtEntry, Browse_t nBrowseType )
{
	if ( !pVmtEntry )
		return;

	KeyValues *pContextKeyValues = new KeyValues( "FileOpen", "nBrowseType", nBrowseType );
	pContextKeyValues->SetPtr( "pVmtEntry", pVmtEntry );
	m_pFileOpenStateMachine->OpenFile( "geometry", pContextKeyValues );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CTargetVMT *CMaterialSubPanel::GetTargetVMT( int nTargetVMTIndex )
{
	CItemUploadWizard *pItemUploadWizard = dynamic_cast< CItemUploadWizard * >( GetWizardPanel() );
	if ( !pItemUploadWizard )
		return NULL;

	return pItemUploadWizard->Asset().GetTargetVMT( nTargetVMTIndex );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CMaterialSubPanel::AddMaterial()
{
	const int nIndex = m_pPanelListPanel->GetItemCount() / 2;

	CFmtStr sLabel;
	sLabel.sprintf( "#VMT%dLabel", nIndex );

	vgui::Label *pLabel = new vgui::Label( this, sLabel, sLabel );
	pLabel->SetContentAlignment( vgui::Label::a_center );

	sLabel.sprintf( "#VMT%dLabel2", nIndex );

	CVmtEntry *pVmtEntry = new CVmtEntry( this, sLabel, nIndex );

	pVmtEntry->SetAutoResize( PIN_TOPLEFT, AUTORESIZE_RIGHT, 0, 0, 0, 0 );

	m_pPanelListPanel->AddItem( pLabel, pVmtEntry );

	sLabel.sprintf( "vmt%d", nIndex );
	AddStatusPanels( sLabel );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CItemUploadWizard::CItemUploadWizard(
	vgui::Panel *pParent,
	const char *pszName )
: BaseClass( pParent, pszName )
{
	SetSize( 1024, 768 );
	SetMinimumSize( 640, 480 );

	vgui::WizardSubPanel *pSubPanel = NULL;
	vgui::DHANDLE< vgui::WizardSubPanel > hSubPanel;
	
	pSubPanel = new CGlobalSubPanel( this, "global", "geometry" );
	pSubPanel->SetVisible( false );
	hSubPanel = pSubPanel;
	m_hSubPanelList.AddToTail( hSubPanel );

	pSubPanel = new CGeometrySubPanel( this, "geometry", "texture" );
	pSubPanel->SetVisible( false );
	hSubPanel = pSubPanel;
	m_hSubPanelList.AddToTail( hSubPanel );

	pSubPanel = new CMaterialSubPanel( this, "texture", "final" );
	pSubPanel->SetVisible( false );
	hSubPanel = pSubPanel;
	m_hSubPanelList.AddToTail( hSubPanel );

	m_pFinalSubPanel = new CFinalSubPanel( this, "final", NULL );
	pSubPanel = m_pFinalSubPanel;
	pSubPanel->SetVisible( false );
	hSubPanel = pSubPanel;
	m_hSubPanelList.AddToTail( hSubPanel );

	Run();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CItemUploadWizard::~CItemUploadWizard()
{
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CItemUploadWizard::Run()
{
	vgui::WizardSubPanel *pStartPanel = dynamic_cast< vgui::WizardSubPanel * >( FindChildByName( "global" ) );

	if ( !pStartPanel )
	{
		Error( "Missing CItemUploadWizard global Panel" );
	}

	BaseClass::Run( pStartPanel );

	MoveToCenterOfScreen();
	Activate();

	vgui::input()->SetAppModalSurface( GetVPanel() );

	CGlobalSubPanel *pGlobalSubPanel = dynamic_cast< CGlobalSubPanel * >( pStartPanel );
	if ( pGlobalSubPanel )
	{
		pGlobalSubPanel->UpdateStatus();
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CItemUploadWizard::UpdateGUI()
{
	for ( int i = 0; i < m_hSubPanelList.Count(); ++i )
	{
		CItemUploadSubPanel *pSubPanel = dynamic_cast< CItemUploadSubPanel * >( m_hSubPanelList.Element( i ).Get() );
		if ( !pSubPanel )
			continue;

		pSubPanel->UpdateGUI();
	}

	CItemUploadSubPanel *pItemUploadSubPanel = dynamic_cast< CItemUploadSubPanel * >( GetCurrentSubPanel() );
	if ( pItemUploadSubPanel )
	{
		pItemUploadSubPanel->UpdateStatus();
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CItemUploadWizard::OnFinishButton()
{
	m_pFinalSubPanel->OnZip();
}