//========= Copyright Valve Corporation, All rights reserved. ============//
//#include <windows.h>
#include "mdmpRipper.h"
#include "vgui_controls/MessageMap.h"
#include "vgui_controls/MenuBar.h"
#include "vgui_controls/Menu.h"
#include "vgui_controls/TextEntry.h"
#include "vgui/ISurface.h"
#include "vgui_controls/Frame.h"
#include "CMDErrorPanel.h"
#include "vgui_controls/ListPanel.h"
#include "KeyValues.h"
#include "vgui/ISystem.h"
#include "CMDDetailPanel.h"

using namespace vgui;

CMDErrorPanel::CMDErrorPanel( vgui::Panel *pParent, const char *pName ) :
	BaseClass( pParent, pName )
{
	SetParent( pParent );
	m_pTokenList = new ListPanel(this, "ErrorList");
	m_pTokenList->AddColumnHeader(0, "errorid", "Error ID", 100, 0);
	m_pTokenList->AddColumnHeader(1, "module", "Module Name", 200, 0);
	m_pTokenList->AddColumnHeader(2, "count", "Count", 100, 0);
	m_pTokenList->AddColumnHeader(3, "minidumps", "Minidump Count", 100, 0);
	
	m_pMaxResults = new vgui::TextEntry( this, "maxResults" );
	m_pQueryCounts = new vgui::TextEntry( this, "numCrashes" );

	m_pMaxResults->SetText( "100" );
	m_pQueryCounts->SetText( "10000" );			

	LoadControlSettings( "MDErrorPanel.res" );		
}
	
void CMDErrorPanel::OnCommand( const char *pCommand )
{	
	if ( !Q_strcmp( pCommand, "Close" ) )
	{		
		Close();	
	}	
	if ( !Q_strcmp( pCommand, "CheckModules" ) )
	{
		CheckModules();
	}
	if ( !Q_strcmp( pCommand, "NewQuery" ) )
	{
		NewQuery();
	}
	if ( !Q_strcmp( pCommand, "Download" ) )
	{
		DownloadMinidumps();
	}
	if ( !Q_strcmp( pCommand, "Details" ) )
	{
		DetailScreen();
	}
}

void CMDErrorPanel::Close()
{
	if ( this )
	{
		m_pTokenList->DeleteAllItems();				
		SetVisible( false );	
		KeyValues *kv = new KeyValues( "Refresh" );
		this->PostActionSignal( kv );
	}
}

void CMDErrorPanel::CheckModules()
{
	char sql[255] = "";
	extern void getMiniDumpHandles( char *pszQuery, const char *errorid, CUtlVector<HANDLE> *pMiniDumpHandles );

	
	int selectedIndex = m_pTokenList->GetSelectedItem( 0 );
    void *kv = m_pTokenList->GetItem( selectedIndex );
	if ( kv )
	{
		strcat( sql, "select MinidumpFilePath from minidumps where ErrorID=" );
		strcat( sql, ((KeyValues *)kv)->GetString( "errorid", "" ) );
		strcat( sql, " order by MinidumpFilePath desc limit " );
		strcat( sql, ((KeyValues *)kv)->GetString( "minidumps", "" ) );
		strcat( sql, ";" );
		getMiniDumpHandles( sql, ((KeyValues *)kv)->GetString( "errorid", "" ), &m_MiniDumpHandles );
		KeyValues *kv = new KeyValues( "Compare", "handlePointer", (int)(&m_MiniDumpHandles) );
		this->PostActionSignal( kv );
	}	
}

void CMDErrorPanel::NewQuery()
{
	m_pTokenList->DeleteAllItems();
	extern void errorsToListPanel( vgui::ListPanel *pTokenList, char* pszQuery );	
	char sql[255] = "";
	char temp[10];
	strcat( sql, "select errorid, module, count, minidumpsonhand from error_types where processed=0 and minidumpsonhand > 0 and count > " );
	m_pQueryCounts->GetText( temp, 10 );
	strcat( sql, temp );
	strcat( sql, " limit " );
	m_pMaxResults->GetText( temp, 10 );
	strcat( sql, temp );
	strcat( sql, ";" );
	errorsToListPanel( m_pTokenList, sql );
	Repaint();
}

void CMDErrorPanel::DownloadMinidumps()
{
	int selectedIndex = m_pTokenList->GetSelectedItem( 0 );
    void *kv = m_pTokenList->GetItem( selectedIndex );
	if ( kv )
	{
		char command[1024] = "";
		
		strcat( command, ((KeyValues *)kv)->GetString( "errorid", "" ));	   
		strcat( command, " minidumpSaves" );		
		::_spawnl( _P_WAIT, ".\\minidump.bat", "minidump.bat ", command, NULL );	
	}
}

void CMDErrorPanel::DetailScreen()
{
	int selectedIndex = m_pTokenList->GetSelectedItem( 0 );
    void *kv = m_pTokenList->GetItem( selectedIndex );
	if ( kv )
	{	
		KeyValues *kvPost = new KeyValues( "Detail", "errorID", ((KeyValues *)kv)->GetString( "errorid", "" ) );
		this->PostActionSignal( kvPost );
	}	
}

		




