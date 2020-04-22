//========= Copyright Valve Corporation, All rights reserved. ============//
#include <windows.h>
#include "mdmpRipper.h"
#include <vgui/IVGui.h>
#include "vgui_controls/MessageMap.h"
#include "vgui_controls/MenuBar.h"
#include "vgui_controls/Menu.h"
#include "vgui_controls/MessageBox.h"
#include "tier1/KeyValues.h"
#include "vgui/ISurface.h"
#include <vgui/ILocalize.h>
#include "vgui_controls/Frame.h"
#include "CMDModulePanel.h"
#include "vgui_controls/ListPanel.h"
#include <vgui_controls/RichText.h>
#include "KeyValues.h"
#include "vgui/ISystem.h"
#include "vgui_controls/FileOpenDialog.h"
#include "isqlwrapper.h"
#include "CMDRipperMain.h"

extern ISQLWrapper *g_pSqlWrapper;

using namespace vgui;



CMDModulePanel::CMDModulePanel( vgui::Panel *pParent, const char *pName ) :
	BaseClass( pParent, pName, true )
{
	m_pTokenList = new ListPanel(this, "ModuleList");
	m_pTokenList->AddColumnHeader(0, "name", "Module Name", 600, 0);
	m_pTokenList->AddColumnHeader(1, "version", "Version", 100, 0);	
	m_pTokenList->AddColumnHeader(2, "count", "Count", 86, 0);	
	m_pTokenList->AddActionSignalTarget( this );

	m_pAnalyzeText = new RichText(this, "AnalyzeText");
	m_pAnalyzeText->SetVerticalScrollbar(true);
	LoadControlSettings( "MDModulePanel.res" );	
	m_pAnalyzeText->InsertString("Initializing...\n");

	InitializeDebugEngine();
	LoadKnownModules();

	m_hThread = NULL;

//	SetTitleBarVisible( false );
//	SetSizeable( false );

	
	//SETUP_PANEL( this );	
}

CMDModulePanel::~CMDModulePanel( void )
{
	ReleaseDebugEngine( );
}

void CMDModulePanel::OnKeyCodeTyped( KeyCode code )
{
	switch ( code )
	{
	case KEY_G:
		UpdateKnownDB( "GOOD" );
		break;
	case KEY_B:
		UpdateKnownDB( "BAD" );
		break;
	case KEY_U:
		UpdateKnownDB( "UNKNOWN" );
		break;
	case KEY_F:
		ModuleLookUp();
		break;
	}
}
	
void CMDModulePanel::OnCommand( const char *pCommand )
{	
	if ( !Q_strcmp( pCommand, "Close" ) )
	{
		//we want to close
		Close();	
	}	
	if ( !Q_strcmp( pCommand, "ModuleLookUp" ) )
	{
		ModuleLookUp();
	}	
	if ( !Q_strcmp( pCommand, "SetGood" ) )
	{
		UpdateKnownDB( "GOOD" );
	}
	if ( !Q_strcmp( pCommand, "SetBad" ) )
	{
		UpdateKnownDB( "BAD" );
	}
	if ( !Q_strcmp( pCommand, "SetUnknown" ) )
	{
		UpdateKnownDB( "UNKNOWN" );
	}
}

void CMDModulePanel::Close()
{
	if ( this )
	{
		m_pTokenList->DeleteAllItems();
		m_MiniDumpList.RemoveAll();
		m_knownModuleList.RemoveAll();
		m_pAnalyzeText->SetText("");
		SetVisible( false );
		KeyValues *kv = new KeyValues( "Refresh" );
		this->PostActionSignal( kv );
	}
}

void CMDModulePanel::Create( CUtlVector<CMiniDumpObject *> *pMiniDump )
{
	LoadKnownModules();

	for ( int i = 0; i < pMiniDump->Count(); i++ )
	{
		pMiniDump->Element(i)->PopulateListPanel( m_pTokenList, true );
	}
}

void CMDModulePanel::Create( const char *filename )
{
	if ( g_pFullFileSystem->FileExists( filename ) )
	{
		LoadKnownModules();

		CMiniDumpObject *newMDObj = new CMiniDumpObject( filename, &m_knownModuleList );
		m_MiniDumpList.AddToTail( newMDObj );
		newMDObj->PopulateListPanel( m_pTokenList, false );	

		AnalyzeDumpFile( filename );
	}
}

void CMDModulePanel::ModuleLookUp()
{
	int selectedIndex = m_pTokenList->GetSelectedItem( 0 );
    void *kv = m_pTokenList->GetItem( selectedIndex );
	if ( kv )
	{
		const char *val = ((KeyValues *)kv)->GetString( "name", "" );
		if ( val )
		{
			const char *moduleName = strrchr( val, '\\' ) + 1;
			char google[1024] = "";
			sprintf( google, "http://www.google.com/search?hl=en&q=%s", moduleName);	
			KeyValues *kvPost = new KeyValues( "ModuleLookUp", "url", google );
			this->PostActionSignal( kvPost );						
		}
	}
}

void SeparateVersion( const char *version, char *v1buf, char *v2buf, char *v3buf, char *v4buf )
{
	const char *endV1 = strchr( version, '.' )+1;
	const char *endV2 = strchr( endV1+1, '.' )+1;
	const char *endV3 = strchr( endV2+1, '.' )+1;
	_mbsnbcpy( (unsigned char *)v1buf, (const unsigned char*)version, endV1 - version );
	v1buf[endV1 - version - 1] = 0;
	_mbsnbcpy( (unsigned char *)v2buf, (const unsigned char*)endV1, endV2 - endV1 );
	v2buf[endV2 - endV1 - 1] = 0;
	_mbsnbcpy( (unsigned char *)v3buf, (const unsigned char*)endV2, endV3 - endV2 );
	v3buf[endV3 - endV2 - 1] = 0;
	strcpy( v4buf, endV3 );
}
		
void SetKeyValueColor( char *type, KeyValues *kv, bool knownVersion )
{
	int colorValue = 255;
	if( !knownVersion )
		colorValue = 155;

	if ( !Q_strcmp( "GOOD", type ) )
	{
		((KeyValues *)kv)->SetColor( "cellcolor", Color(0,colorValue,0,255));
	}
	else if ( !Q_strcmp( "BAD", type ) )
	{
		((KeyValues *)kv)->SetColor( "cellcolor", Color(colorValue,0,0,255));
	}
	else
	{
		((KeyValues *)kv)->SetColor( "cellcolor", Color(255,255,0,255));
	}
}

void CMDModulePanel::UpdateKnownDB( char *type )
{
	int selectedIndex = m_pTokenList->GetSelectedItem( 0 );
    void *kv = m_pTokenList->GetItem( selectedIndex );
	char v1buf[10];
	char v2buf[10];
	char v3buf[10];
	char v4buf[10];
	char name[65];
	char keybuf[10];
	if ( kv )
	{		
		SetKeyValueColor( type, (KeyValues *)kv, true );
		int key = ((KeyValues *)kv)->GetInt( "key" );
		itoa( key, keybuf, 10 );
		strcpy( name, strrchr(((KeyValues *)kv)->GetString( "name" ), '\\')+1);
		SeparateVersion( ((KeyValues *)kv)->GetString("version"), v1buf, v2buf, v3buf, v4buf );		
		if ( key == 0 )
		{
			//as far as we know, this is a non-existant module.
			if ( !Q_strcmp( type, "UNKNOWN" ) )
			{
				return;
			}
			else 
			{
				char query[1024];
				sprintf( query, "select * from knownmodules where name = \"%s\" and version1 = %s and version2 = %s and version3 = %s and version4 = %s;",
					name, v1buf, v2buf, v3buf, v4buf );				
				IResultSet *results = g_pSqlWrapper->PResultSetQuery( query );	// do the query
				if ( !results )
				{
					return;
				}

				int numResults = results->GetCSQLRow();
				if ( numResults > 0 )
				{
					//there is an entry... get our module list up to date with this entry

					const ISQLRow *row = results->PSQLRowNextResult();		
					Assert( row != NULL );
					int realKey = row->NData(0);
					const char *realType = row->PchData(6);
					g_pSqlWrapper->FreeResult();

					((KeyValues *)kv)->SetInt( "key", realKey );											
					
					if ( !Q_strcmp( realType, type ) )
					{
						//this user was out of sync with the database.  It doesn't actually need updating.
						return;
					}
					else
					{
						char update[1024];
						sprintf( update, "update knownmodules set type=\"%s\" where id = %i;", type, realKey);
						g_pSqlWrapper->BInsert( update );						
					}
				}
				else
				{
					g_pSqlWrapper->FreeResult();
					//it isn't in there.  Let's add it.
					char update[1024];
					sprintf( update, "insert into knownmodules set name = \"%s\", version1 = %s, version2 = %s, version3 = %s, version4 = %s, type = \"%s\";",
						name, v1buf, v2buf, v3buf, v4buf, type);
					g_pSqlWrapper->BInsert( update );
					results = g_pSqlWrapper->PResultSetQuery( query );	// do the query		
					int numResults = results->GetCSQLRow();
					if ( numResults > 0 )
					{
						const ISQLRow *row = results->PSQLRowNextResult();		
						Assert( row != NULL );
						int realKey = row->NData(0);
						((KeyValues *)kv)->SetInt( "key", realKey );						
					}                    
					g_pSqlWrapper->FreeResult();
				}
			}
		}
		else
		{
			char query[1024];
			sprintf( query, "select * from knownmodules where id = %i;",
				key);				
			IResultSet *results = g_pSqlWrapper->PResultSetQuery( query );	// do the query		

			int numResults = results->GetCSQLRow();
			if ( numResults > 0 )
			{
				//there is an entry... update it with the new info...

				const ISQLRow *row = results->PSQLRowNextResult();		
				Assert( row != NULL );
				Assert( numResults == 1 );
				Assert( !Q_stricmp( name, row->PchData(1) ) && atoi( v1buf ) == row->NData(2) && atoi( v2buf ) == row->NData(3) && 
					atoi( v3buf ) == row->NData(4) && atoi( v4buf ) == row->NData(5) );
				int realKey = row->NData(0);
				const char *realType = row->PchData(6);
				g_pSqlWrapper->FreeResult();

				if ( !Q_strcmp( realType, type ) )
				{
					//we don't need to update... it is already updated already
					return;
				}								
                
				char update[1024];
				sprintf( update, "update knownmodules set type=\"%s\" where id = %i;", type, realKey);
				g_pSqlWrapper->BInsert( update );		
				
			}
			else
			{
				//the module entry was mis-keyed.  First, check for an existing entry of this module.
				char query[1024];
				sprintf( query, "select * from knownmodules where name = \"%s\" and version1 = %s and version2 = %s and version3 = %s and version4 = %s;",
					name, v1buf, v2buf, v3buf, v4buf );	
				IResultSet *results = g_pSqlWrapper->PResultSetQuery( query );	// do the query		
				
				int numResults = results->GetCSQLRow();
				if ( numResults > 0 )
				{
                    //there is an existing entry.  Update its type and update the key for this keyvalue;
					const ISQLRow *row = results->PSQLRowNextResult();
					int realKey = row->NData(0);
					((KeyValues *)kv)->SetInt( "key", realKey );
					g_pSqlWrapper->FreeResult();
					char update[1024];
					sprintf( update, "update knownmodules set type=\"%s\" where id = %i;", type, realKey);
					g_pSqlWrapper->BInsert( update );
				}
				else
				{
					g_pSqlWrapper->FreeResult();
					//no exisiting entry.  Insert it.
					char update[1024];
					sprintf( update, "insert into knownmodules set name = \"%s\", version1 = %s, version2 = %s, version3 = %s, version4 = %s, type = \"%s\";",
						name, v1buf, v2buf, v3buf, v4buf, type);
					g_pSqlWrapper->BInsert( update );
					results = g_pSqlWrapper->PResultSetQuery( query );	// do the query		
					int numResults = results->GetCSQLRow();
					if ( numResults > 0 )
					{
						const ISQLRow *row = results->PSQLRowNextResult();		
						Assert( row != NULL );
						int realKey = row->NData(0);
						((KeyValues *)kv)->SetInt( "key", realKey );						
					}                    
					g_pSqlWrapper->FreeResult();
				}
			}
		}
	}
}


void CMDModulePanel::OnCompare( KeyValues *data )
{
	LoadKnownModules();

	CUtlVector<HANDLE> *pMiniDumpHandles = (CUtlVector<HANDLE> *)(void *)data->GetInt( "handlePointer" );
	

	DWORD error;
	int returnValue = 0;
	for( int i = 0; i < pMiniDumpHandles->Count(); i++ )
	{
		m_MiniDumpList.AddToTail( new CMiniDumpObject( pMiniDumpHandles->Element( i ), &m_knownModuleList  ) );
		returnValue = CloseHandle( pMiniDumpHandles->Element( i ) );
		error = GetLastError();
	}
	Create( &m_MiniDumpList );
	SetVisible( true );
	MoveToFront();

	pMiniDumpHandles->RemoveAll();
	system("rmdir c:\\minidumptool /s/q");		
}


void CMDModulePanel::OnDbgOutput( int iMask, const char *pszDebugText)
{
	if ( m_pAnalyzeText && pszDebugText )
	{
		m_pAnalyzeText->InsertString( pszDebugText );
	}
}


DWORD WINAPI CMDModulePanel::StaticAnalyzeThread( LPVOID lParam )
{
	CMDModulePanel *pClass = (CMDModulePanel *)lParam;
	if ( pClass )
	{
		pClass->AnalyzeThread( );
	}

	return ( 0 );
}

void CMDModulePanel::LoadKnownModules()
{
	if ( m_knownModuleList.Count() > 0 )
		return;

	char rgchQueryBuf[ 1024 ] = "SELECT * from knownmodules;";
	IResultSet *results = g_pSqlWrapper->PResultSetQuery( rgchQueryBuf );

	if ( !results )
	{
		ivgui()->DPrintf( "LoadKnownModules() results are NULL" );
		VGUIMessageBox( GetParent(), "Error", "Unable to retrieve known modules from database" );
		return;
	}

	for ( int i = 0; i < results->GetCSQLRow(); i++ )
	{
		module newModule;
		const ISQLRow *row = results->PSQLRowNextResult();		
		Assert( row != NULL );
		newModule.key = row->NData(0);
		strcpy( newModule.name, row->PchData(1));
		newModule.versionInfo.v1 = row->NData(2);
		newModule.versionInfo.v2 = row->NData(3);
		newModule.versionInfo.v3 = row->NData(4);
		newModule.versionInfo.v4 = row->NData(5);		
		if ( !Q_strcmp( row->PchData(6), "GOOD" ) )
		{
			newModule.myType = GOOD;
		}
		else if ( !Q_strcmp( row->PchData(6), "BAD" ) )
		{
			newModule.myType = BAD;
		}
		else
		{
			newModule.myType = UNKNOWN;
		}
		m_knownModuleList.AddToTail( newModule );
	}

	g_pSqlWrapper->FreeResult();
}


void CMDModulePanel::InitializeDebugEngine( void )
{
    // Start things off by getting an initial interface from
    // the engine.  This can be any engine interface but is
    // generally IDebugClient as the client interface is
    // where sessions are started.
    if ( S_OK == DebugCreate( __uuidof ( IDebugClient ),
                                 (void**)&m_pDbgClient ) )
    {
        m_pDbgClient->QueryInterface( __uuidof ( IDebugControl ),
									  ( void** )&m_pDbgControl );
		m_pDbgClient->QueryInterface( __uuidof ( IDebugSymbols2 ),
									  ( void** )&m_pDbgSymbols );

		// Set out Panel to receive the debug outputs from the engine
		m_cDbgOutput.SetOutputPanel( GetVPanel() );

		// Install output callbacks so we get any output that the
		// later calls produce.
		m_pDbgClient->SetOutputCallbacks(&m_cDbgOutput);

		if ( m_pDbgSymbols )
		{
			// Make sure we have a symbol path to use
			char szSymbolSrv[ 512 ] = { 0 };
			ExpandEnvironmentStrings( "%_NT_SYMBOL_PATH%", szSymbolSrv, sizeof (szSymbolSrv) );
			if ( !Q_stricmp( "%_NT_SYMBOL_PATH%", szSymbolSrv ) )
			{
				ivgui()->DPrintf( "Setting symbol server" );
				Q_strcpy( szSymbolSrv, "SRV*c:\\localsymbols*\\\\perforce\\symbols*http://msdl.microsoft.com/download/symbols" );
				m_pDbgSymbols->SetSymbolPath( szSymbolSrv );
			}
			m_pDbgSymbols->AddSymbolOptions(SYMOPT_LOAD_LINES);
		}
    }
}

void CMDModulePanel::ReleaseDebugEngine( void )
{
	// Clean up any resources.
    if ( m_pDbgSymbols != NULL )
    {
        m_pDbgSymbols->Release( );
    }

    if ( m_pDbgControl != NULL )
    {
        m_pDbgControl->Release( );
    }

    if ( m_pDbgClient != NULL )
    {
        // We don't want to see any output from the shutdown.
        m_pDbgClient->SetOutputCallbacks( NULL );
        
        m_pDbgClient->EndSession( DEBUG_END_PASSIVE );
        
        m_pDbgClient->Release( );
    }
}


void CMDModulePanel::AnalyzeDumpFile( const char *pszDumpFile )
{
	if (m_pDbgClient && m_pDbgControl)
	{
		char szBuf[ 256 ] = { 0 };
		Q_snprintf( szBuf, sizeof ( szBuf ), "About to open [%s].\n", pszDumpFile );
		m_pAnalyzeText->InsertString( szBuf );

		// Everything's set up so open the dump file.
		m_pDbgClient->OpenDumpFile(pszDumpFile);

		// Finish initialization by waiting for the event that
		// caused the dump.  This will return immediately as the
		// dump file is considered to be at its event.
		m_pDbgControl->WaitForEvent(DEBUG_WAIT_DEFAULT, INFINITE);

		DWORD dwThreadId = 0;
		m_hThread = CreateThread( NULL, 0, StaticAnalyzeThread, (LPVOID)this, 0, &dwThreadId );
	}
}


DWORD CMDModulePanel::AnalyzeThread( void )
{
	if ( m_pDbgControl )
	{
		// Tell the debug engine to analyze the current dump file
		m_pDbgControl->Execute( DEBUG_OUTCTL_THIS_CLIENT,
								"!analyze -v", 
								DEBUG_EXECUTE_DEFAULT);
	}

	CloseHandle( m_hThread );
	m_pAnalyzeText->InsertString( "Finished analyzing minidump file.\n" );

	return ( 0 );
}

