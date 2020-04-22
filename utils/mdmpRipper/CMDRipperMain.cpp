//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Dialog for selecting game configurations
//
//=====================================================================================//

#include <windows.h>

#include <vgui/IVGui.h>
#include <vgui/IInput.h>
#include <vgui/ISystem.h>
#include <vgui_controls/ComboBox.h>
#include <vgui_controls/MessageBox.h>
#include <vgui_controls/FileOpenDialog.h>
#include <KeyValues.h>
#include "CMDErrorPanel.h"
#include "CMDModulePanel.h"
#include "isqlwrapper.h"
#include "CMDRipperMain.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

CMDRipperMain *g_pCMDRipperMain = NULL;
extern ISQLWrapper *g_pSqlWrapper;

class CModalPreserveMessageBox : public vgui::MessageBox
{
public:
	CModalPreserveMessageBox(const char *title, const char *text, vgui::Panel *parent)
		: vgui::MessageBox( title, text, parent )
	{
		m_PrevAppFocusPanel = vgui::input()->GetAppModalSurface();
	}

	~CModalPreserveMessageBox()
	{
		vgui::input()->SetAppModalSurface( m_PrevAppFocusPanel );
	}

public:
	vgui::VPANEL	m_PrevAppFocusPanel;
};
		
//-----------------------------------------------------------------------------
// Purpose: Utility function to pop up a VGUI message box
//-----------------------------------------------------------------------------
void VGUIMessageBox( vgui::Panel *pParent, const char *pTitle, const char *pMsg, ... )
{
	char msg[4096];
	va_list marker;
	va_start( marker, pMsg );
	Q_vsnprintf( msg, sizeof( msg ), pMsg, marker );
	va_end( marker );

	vgui::MessageBox *dlg = new CModalPreserveMessageBox( pTitle, msg, pParent );
	dlg->DoModal();	
	dlg->Activate();
	dlg->RequestFocus();
}

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CMDRipperMain::CMDRipperMain( Panel *parent, const char *name  ) : BaseClass( parent, name ), m_bChanged( false )
{
	Assert( !g_pCMDRipperMain );
	g_pCMDRipperMain = this;

	Sys_LoadInterface( "sqlwrapper", INTERFACEVERSION_ISQLWRAPPER, &hSQLWrapper, (void **)&sqlWrapperFactory );
	Assert( hSQLWrapper != NULL );
	Assert( sqlWrapperFactory != NULL );

	sqlWrapper = sqlWrapperFactory->Create( "cserr", "steamweb-sql", "root", "" );	
	if ( !sqlWrapper )
	{
		ivgui()->DPrintf( "SQLWrapper is NULL" );
	}
	Assert( sqlWrapper != NULL );
	g_pSqlWrapper = sqlWrapper;

	SetSize(800, 600);
	SetMinimumSize(500, 200);

	SetMinimizeButtonVisible( true );	

	m_pMenuBar = new vgui::MenuBar( this, "Main Menu Bar" );
	m_pMenuBar->SetPos( 5, 26 );
	m_pMenuBar->SetSize( 690, 28 );

 	Menu *pFileMenu = new Menu(NULL, "File");
	pFileMenu->AddMenuItem( "&Open", new KeyValues( "Open" ), this );
	m_pMenuBar->AddMenu( "&File", pFileMenu );

    Menu *pErrorMenu = new Menu(NULL, "Error");
	pErrorMenu->AddMenuItem( "&Error", new KeyValues("Error"), this);
	m_pMenuBar->AddMenu( "&Error", pErrorMenu );		

	m_pErrorPanel = new CMDErrorPanel( this, "MDError Panel" );
	m_pErrorPanel->AddActionSignalTarget( this );		
	
	m_pModulePanel = new CMDModulePanel( this, "MDModule Panel" );
	m_pModulePanel->AddActionSignalTarget( this );	
	
	m_pErrorPanel->AddActionSignalTarget( m_pModulePanel );

	m_pDetailPanel = new CMDDetailPanel( this, "MDDetail Panel" );
	m_pErrorPanel->AddActionSignalTarget( this );
	
	LoadControlSettings( "MDRipperMain.res" );	

	m_pErrorPanel->SetVisible( false );
	m_pModulePanel->SetVisible( false );
	m_pDetailPanel->SetVisible( false );	
}

//-----------------------------------------------------------------------------
// Destructor 
//-----------------------------------------------------------------------------
CMDRipperMain::~CMDRipperMain()
{
	g_pCMDRipperMain = NULL;
}



//-----------------------------------------------------------------------------
// Purpose: Kills the whole app on close
//-----------------------------------------------------------------------------
void CMDRipperMain::OnClose( void )
{
	BaseClass::OnClose();
	ivgui()->Stop();

	sqlWrapper->FreeResult();
	sqlWrapperFactory->Free( sqlWrapper );
	Sys_UnloadModule( hSQLWrapper );	
}

/*
//-----------------------------------------------------------------------------
// Purpose: Select the item from the list (updating the environment variable as well)
// Input  : index - item to select
//-----------------------------------------------------------------------------
void CMDRipperMain::SetGlobalConfig( const char *modDir )
{
	// Set our environment variable
	SetVConfigRegistrySetting( GAMEDIR_TOKEN, modDir );
}
*/

//-----------------------------------------------------------------------------
// Purpose: Parse commands coming in from the VGUI dialog
//-----------------------------------------------------------------------------
void CMDRipperMain::OnCommand( const char *command )
{	
	if ( Q_stricmp( command, "Open" ) == 0 )
	{
		OnOpen();
	}
	else if ( Q_stricmp( command, "Error" ) == 0 )
	{
		OnError();
	}
	BaseClass::OnCommand( command );
}


bool CMDRipperMain::RequestInfo( KeyValues *outputData )
{
	const char * szName = outputData->GetName();
	if ( !Q_stricmp( szName, "DragDrop" ))
	{
		bool bAccept = false;

		if ( !Q_stricmp( outputData->GetString( "type" ), "Files" ) )
		{
			// Make sure we only get .mdmp files
			KeyValues *pFiles = outputData->FindKey( "list", false );
			if ( pFiles )
			{
				const char *pszFile = pFiles->GetString( "0" );
				const char *pszExtension = Q_strrchr( pszFile, '.' );
				if ( pszExtension )
				{
					if ( !Q_stricmp( pszExtension, ".mdmp" ) )
					{
						outputData->SetPtr( "AcceptPanel", ( Panel * )this );
						bAccept = true;
					}
				}
			}
		}
		
		return ( bAccept );
	}

	return ( BaseClass::RequestInfo( outputData ) );
}


void CMDRipperMain::OnOpen()
{
	FileOpenDialog *pFileDialog = new FileOpenDialog ( this, "File Open", true);
	pFileDialog->AddActionSignalTarget(this);
	pFileDialog->AddFilter( "*.mdmp", "MiniDumps", true );
	pFileDialog->DoModal( true );
}

void CMDRipperMain::OnError()
{
	m_pErrorPanel->NewQuery();	
	m_pErrorPanel->SetVisible( true );
	m_pErrorPanel->MoveToFront();
	Repaint();
}

void CMDRipperMain::OnFileSelected( const char *filename ) 
{
	m_pModulePanel->Create( filename );	
	m_pModulePanel->SetVisible( true );	
	Repaint();
}

void CMDRipperMain::OnDetail( KeyValues *data )
{	
	char URL[1024] = "";
	strcat( URL, "http://steamweb/cserr_detailsnograph.php?errorid=" );
	strcat( URL, data->GetString( "errorID" ) );
	m_pDetailPanel->OpenURL( URL );	
	m_pDetailPanel->SetVisible( true );
	m_pDetailPanel->MoveToFront();
	Repaint();
}

void CMDRipperMain::OnRefresh()
{
	Repaint();
}

void CMDRipperMain::OnLookUp( KeyValues *data )
{
	m_pDetailPanel->OpenURL( data->GetString( "url" ) );
	m_pDetailPanel->SetVisible( true );
	m_pDetailPanel->MoveToFront();
	Repaint();
}


void CMDRipperMain::OnDragDrop( KeyValues *pData ) 
{
	KeyValues *pFiles = pData->FindKey( "list", false );
	if ( pFiles )
	{
		DWORD dwIndex = 0;
		const char *pszFile = NULL;
		char szIndex[ 64 ] = { 0 };

		do
		{
			Q_snprintf( szIndex, sizeof ( szIndex ), "%d", dwIndex );
			pszFile = pFiles->GetString( szIndex );
			ivgui()->DPrintf( "Got file [%s]", pszFile );
			OnFileSelected( pszFile );
			dwIndex++;
		}
		while ( g_pFullFileSystem->FileExists( pszFile ) );
	}

}