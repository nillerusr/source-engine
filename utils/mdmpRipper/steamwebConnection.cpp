//========= Copyright Valve Corporation, All rights reserved. ============//

#include <windows.h>
//#include <conio.h>
#include "vgui_controls/ListPanel.h"
#include "KeyValues.h"
#include "isqlwrapper.h"
#include "vgui/ISystem.h"

#include "tier0/memdbgon.h"

using namespace vgui;

extern ISQLWrapper *g_pSqlWrapper;

void getMiniDumpHandles( char* pszQuery, const char *errorid, CUtlVector<HANDLE> *pMiniDumpHandles )
{	
	char rgchQueryBuf[ 1024 ];
	
	Q_snprintf( rgchQueryBuf, sizeof(rgchQueryBuf), pszQuery ); 
	IResultSet *results = g_pSqlWrapper->PResultSetQuery( rgchQueryBuf );	// do the query
	Assert( results != NULL );

	char command[1024] = "";
		
	strcat( command, errorid );	   
	strcat( command, " minidumptool" );
	::_spawnl( _P_WAIT, ".\\minidump.bat", "minidump.bat ", command, NULL );	

	char path[1024] = "";
	char *pathTraverse;
	char newPath[1024] = "";
 	for ( int i = 0; i < results->GetCSQLRow(); i++ )
	{
		const ISQLRow *row = results->PSQLRowNextResult();		
		Assert( row != NULL );
		strcpy( path, row->PchData(0) );
		pathTraverse = strchr( path, '/' ) + 1;
		pathTraverse = strchr( pathTraverse, '/' ) + 1;
		pathTraverse = strchr( pathTraverse, '/' ) + 1;
		pathTraverse = strchr( pathTraverse, '/' );
		strcat( newPath, "c:/minidumptool" );
		strcat( newPath, pathTraverse );		
		HANDLE hFile = CreateFile(newPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		pMiniDumpHandles->AddToTail( hFile );
		newPath[0] = 0;
	}
	g_pSqlWrapper->FreeResult();
}
void errorsToListPanel( vgui::ListPanel *pTokenList, char* pszQuery )
{	
	char rgchQueryBuf[ 1024 ];
	
	Q_snprintf( rgchQueryBuf, sizeof(rgchQueryBuf), pszQuery ); 
	IResultSet *results = g_pSqlWrapper->PResultSetQuery( rgchQueryBuf );	// do the query
	Assert( results != NULL );
	
	int errorid;
	char errorbuf[128];
	char module[128];
	int count;
	int minidumps;
	char keyNameBuf[1024] = "module";
	char modNumBuf[4] = "";
 	for ( int i = 0; i < results->GetCSQLRow(); i++ )
	{
		const ISQLRow *row = results->PSQLRowNextResult();
		itoa( i, modNumBuf, 10 );
		strcat( keyNameBuf, modNumBuf );

		errorid = row->NData(0);
		itoa( errorid, errorbuf, 10 );
		strcpy( module, row->PchData(1) );
		count = row->NData(2);
		minidumps = row->NData(3);

		KeyValues *kv = new KeyValues( keyNameBuf, "errorid", errorbuf, "module", module );	
		kv->SetInt( "count", count );
		kv->SetInt( "minidumps", minidumps );
		pTokenList->AddItem(kv, i, false, false);		
	}	
	g_pSqlWrapper->FreeResult();
}