//========= Copyright Valve Corporation, All rights reserved. ============//
// ModelBrowser.cpp : implementation file
//

#include "stdafx.h"
#include "ModelBrowser.h"
#include "matsys_controls/mdlpicker.h"
#include "matsys_controls/matsyscontrols.h"
#include "vgui_controls/TextEntry.h"
#include "vgui_controls/Splitter.h"
#include "vgui_controls/Button.h"
#include "KeyValues.h"
#include "vgui/KeyCode.h"
#include "texturesystem.h"

static LPCTSTR pszIniSection = "Model Browser";

// CModelBrowser dialog


class CModelBrowserPanel : public vgui::EditablePanel
{
public:
	CModelBrowserPanel( CModelBrowser *pBrowser, const char *panelName ) : 
	  vgui::EditablePanel( NULL, panelName )
	{
		m_pBrowser = pBrowser;
	}

	virtual	void OnSizeChanged(int newWide, int newTall)
	{
		// call Panel and not EditablePanel OnSizeChanged.
		Panel::OnSizeChanged(newWide, newTall);
	}

	virtual void OnCommand( const char *pCommand )
	{
		if ( Q_strcmp( pCommand, "OK" ) == 0 )
		{	
			m_pBrowser->EndDialog( IDOK );
		}
		else if ( Q_strcmp( pCommand, "Cancel" ) == 0 )
		{
			m_pBrowser->EndDialog( IDCANCEL );
		}
	}

	virtual void OnKeyCodeTyped(vgui::KeyCode code)
	{
		vgui::EditablePanel::OnKeyCodeTyped( code );

		if ( code == KEY_ENTER )
		{
			m_pBrowser->EndDialog( IDOK );
		}
		else if ( code == KEY_ESCAPE )
		{
			m_pBrowser->EndDialog( IDCANCEL );
		}
	}

	virtual void OnMessage(const KeyValues *params, vgui::VPANEL ifromPanel)
	{
		vgui::EditablePanel::OnMessage( params, ifromPanel );
		
		if ( Q_strcmp( params->GetName(), "MDLPreviewChanged" ) == 0 ) 
		{
			m_pBrowser->UpdateStatusLine();
		}
	}

	CModelBrowser *m_pBrowser;
};

IMPLEMENT_DYNAMIC(CModelBrowser, CDialog)
CModelBrowser::CModelBrowser(CWnd* pParent /*=NULL*/)
	: CDialog(CModelBrowser::IDD, pParent)
{
	m_pPicker = new CMDLPicker( NULL );
	m_pStatusLine = new vgui::TextEntry( NULL, "StatusLine" );

	m_pButtonOK = new vgui::Button( NULL, "OpenButton", "OK" );
	m_pButtonCancel = new vgui::Button( NULL, "CancelButton", "Cancel" );
}

CModelBrowser::~CModelBrowser()
{
}

void CModelBrowser::SetModelName( const char *pModelName )
{
	char pszTempModelName[255];
	strcpy( pszTempModelName, pModelName );

	char * pszSelectedModel = strchr( pszTempModelName, '/' );
	if( pszSelectedModel)
	{
		pszSelectedModel += 1;
		Q_FixSlashes( pszSelectedModel, '\\' );
	}

	m_pPicker->SelectMDL( pModelName );
	m_pPicker->SetInitialSelection( pszSelectedModel );
		
	m_pStatusLine->SetText( pModelName );
}

void CModelBrowser::GetModelName( char *pModelName, int length )
{
	m_pPicker->GetSelectedMDLName( pModelName, length );

	Q_FixSlashes( pModelName, '/' );
}

void CModelBrowser::GetSkin( int &nSkin )
{
	nSkin = m_pPicker->GetSelectedSkin();
}

void CModelBrowser::SetSkin( int nSkin )
{
	m_pPicker->SelectSkin( nSkin );
}

void CModelBrowser::UpdateStatusLine()
{
	char szModel[1024];

	m_pPicker->GetSelectedMDLName( szModel, sizeof(szModel) );

	m_pStatusLine->SetText( szModel );

/*	MDLHandle_t hMDL = vgui::MDLCache()->FindMDL( szModel );

	studiohdr_t *hdr = vgui::MDLCache()->GetStudioHdr( hMDL );

	vgui::MDLCache()->Release( hMDL ); */
}

void CModelBrowser::SaveLoadSettings( bool bSave )
{
	CString	str;
	CRect	rect;
	CWinApp	*pApp = AfxGetApp();

	if ( bSave )
	{
		GetWindowRect(rect);
		str.Format("%d %d %d %d", rect.left, rect.top, rect.right, rect.bottom);
		pApp->WriteProfileString(pszIniSection, "Position", str);
		pApp->WriteProfileString(pszIniSection, "Filter", m_pPicker->GetFilter() );
	}
	else
	{
		str = pApp->GetProfileString(pszIniSection, "Position");

		if (!str.IsEmpty())
		{
			sscanf(str, "%d %d %d %d", &rect.left, &rect.top, &rect.right, &rect.bottom);

			if (rect.left < 0)
			{
				ShowWindow(SW_SHOWMAXIMIZED);
			}
			else
			{
				MoveWindow(rect.left, rect.top, rect.right-rect.left, rect.bottom-rect.top, FALSE);
			}

			Resize();
		}

		str = pApp->GetProfileString(pszIniSection, "Filter");

		if (!str.IsEmpty())
		{
			m_pPicker->SetFilter( str );
		}
	}
}



void CModelBrowser::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

void CModelBrowser::Resize()
{
	// reposition controls
	CRect rect;
	GetClientRect(&rect);

	m_VGuiWindow.MoveWindow( rect );

	m_pPicker->SetBounds( 0,0, rect.Width(), rect.Height() - 32 );
	m_pButtonCancel->SetPos( 8, rect.Height() - 30 );
	m_pButtonOK->SetPos( 84, rect.Height() - 30 );
	m_pStatusLine->SetBounds( 160, rect.Height() - 30, max( 100, rect.Width() - 166 ), 24 );
}

void CModelBrowser::OnSize(UINT nType, int cx, int cy) 
{
	if (nType == SIZE_MINIMIZED || !IsWindow(m_VGuiWindow.m_hWnd) )
	{
		CDialog::OnSize(nType, cx, cy);
		return;
	}

	Resize();

	CDialog::OnSize(nType, cx, cy);
}


BEGIN_MESSAGE_MAP(CModelBrowser, CDialog)
	ON_WM_SIZE()
	ON_WM_DESTROY()
END_MESSAGE_MAP()

BOOL CModelBrowser::PreTranslateMessage( MSG* pMsg )
{
	// don't filter dialog message
	return CWnd::PreTranslateMessage( pMsg );
}

BOOL CModelBrowser::OnInitDialog()
{
	CDialog::OnInitDialog();

	m_VGuiWindow.Create( NULL, _T("ModelViewer"), WS_VISIBLE|WS_CHILD, CRect(0,0,100,100), this, 1001);

	vgui::EditablePanel *pMainPanel = new CModelBrowserPanel( this, "ModelBrowerPanel" );
	
	m_VGuiWindow.SetParentWindow( &m_VGuiWindow );
	m_VGuiWindow.SetMainPanel( pMainPanel );
	pMainPanel->MakePopup( false, false );
    m_VGuiWindow.SetRepaintInterval( 75 );
	

	m_pPicker->SetParent( pMainPanel );
	m_pPicker->AddActionSignalTarget( pMainPanel );	

	m_pButtonOK->SetParent( pMainPanel );
	m_pButtonOK->AddActionSignalTarget( pMainPanel );
	m_pButtonOK->SetCommand( "OK" );

	m_pButtonCancel->SetParent( pMainPanel );
	m_pButtonCancel->AddActionSignalTarget( pMainPanel );
	m_pButtonCancel->SetCommand( "Cancel" );

	m_pStatusLine->SetParent( pMainPanel );
	m_pStatusLine->SetEditable( false );
	
	SaveLoadSettings( false ); // load

	m_pPicker->Activate();

	return TRUE;
}

void CModelBrowser::OnDestroy()
{
	SaveLoadSettings( true ); // save

	// model browser destoys our defualt cube map, reload it
	g_Textures.RebindDefaultCubeMap();

	CDialog::OnDestroy();
}

void CModelBrowser::Show()
{
	if (m_pPicker)
	{
		m_pPicker->SetVisible( true );
	}
	if (m_pStatusLine)
		m_pStatusLine->SetVisible( true );
	if (m_pButtonOK)
		m_pButtonOK->SetVisible( true );
	if (m_pButtonCancel)
		m_pButtonCancel->SetVisible( true );

}
void CModelBrowser::Hide()
{
	if (m_pPicker)
		m_pPicker->SetVisible( false );

	if (m_pStatusLine)
		m_pStatusLine->SetVisible( false );

	if (m_pButtonOK)
		m_pButtonOK->SetVisible( false );

	if (m_pButtonCancel)
		m_pButtonCancel->SetVisible( false );
}
