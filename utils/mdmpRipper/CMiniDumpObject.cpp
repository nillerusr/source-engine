//========= Copyright Valve Corporation, All rights reserved. ============//
#include <windows.h>
#include "tier1/strtools.h"
#include <conio.h>
#include "utlvector.h"
#include <Dbghelp.h>
#include "isqlwrapper.h"
#include "CMiniDumpObject.h"

extern ISQLWrapper *g_pSqlWrapper;

CMiniDumpObject::CMiniDumpObject( const char *pszFilename, CUtlVector<module> *pKnownModuleList )
{
	InitFromFilename( pszFilename, pKnownModuleList );
}

CMiniDumpObject::CMiniDumpObject( HANDLE pMiniDumpHandle, CUtlVector<module> *pKnownModuleList )
{
	InitFromHandle( pMiniDumpHandle, pKnownModuleList );
}

void CMiniDumpObject::Init( HANDLE pszFileMap, CUtlVector<module> *pKnownModuleList )
{
	PMINIDUMP_DIRECTORY miniDumpDir;
	PVOID pOutput;
	ULONG ulSize;
	
	if ( !pszFileMap )
		return;
	PVOID pMiniDump = MapViewOfFile( pszFileMap, FILE_MAP_READ, 0, 0, 0 );
	
	MiniDumpReadDumpStream( pMiniDump, ModuleListStream, &miniDumpDir, &pOutput, &ulSize );
	
	int numberOfModules = ((int *)pOutput)[0];
	MINIDUMP_MODULE *pModules = (MINIDUMP_MODULE *)((byte *)pOutput + sizeof(int));
	
	for ( int i = 0; i < numberOfModules; i++ )
	{
		MINIDUMP_MODULE *pCurrentModule = (pModules + i);
		module *newModule = new module;
		newModule->key = 0;
		newModule->knownVersion = false;
		int offset = pCurrentModule->ModuleNameRva;
		int nSizeOfName = *(int *)((byte *)pMiniDump + offset);	
		char nameBuf[1024];
		char *pszName = (char *)(byte *)pMiniDump + offset + sizeof(int);	
		int j = 0;
		for ( j = 0; j < nSizeOfName/2; j++ )
		{
			nameBuf[j] = *(pszName + j*2);
		}
		nameBuf[j] = 0;
		strcpy( newModule->name, nameBuf );
		newModule->versionInfo = GetVersionStruct(&pCurrentModule->VersionInfo);
		bool added = false;

        for ( int j = 0; j < pKnownModuleList->Count(); j++ )
		{
			if ( !Q_stricmp( strrchr( nameBuf, '\\' )+1, pKnownModuleList->Element( j ).name ) )
			{
				newModule->myType = pKnownModuleList->Element( j ).myType;

				if ( newModule->versionInfo == pKnownModuleList->Element(j).versionInfo )
				{
					newModule->key = pKnownModuleList->Element( j ).key;					
					newModule->knownVersion = true;
				}
				else
				{		
					newModule->knownVersion = false;
				}				
			}
			if ( newModule->knownVersion )
				break;
		}
		switch ( newModule->myType )
		{
		case GOOD:
			m_goodModuleList.AddToTail( *newModule );
			added = true;
			break;
		case BAD:
			m_badModuleList.AddToTail( *newModule );
			added = true;
			break;
		default:
			m_unknownModuleList.AddToTail( *newModule );
			added = true;
			break;
		}				
		if ( !added )
		{
			newModule->key = 0;
			newModule->myType = UNKNOWN;
			m_unknownModuleList.AddToTail( *newModule );
		}
	}
    UnmapViewOfFile( pMiniDump );
}

void CMiniDumpObject::InitFromHandle( HANDLE pMiniDumpHandle, CUtlVector<module> *pKnownModuleList )
{
	if ( INVALID_HANDLE_VALUE != pMiniDumpHandle )
	{
		DWORD dwNumRead = 0;
		char szMdmp[ 5 ] = { 0 };
		if ( ReadFile( pMiniDumpHandle, (LPVOID)szMdmp, 4, &dwNumRead, NULL ) )
		{
			if ( !Q_strcmp( "MDMP", szMdmp ))
			{
				HANDLE hFileMap = CreateFileMapping( pMiniDumpHandle, NULL, PAGE_READONLY, 0, 0, NULL );	
				Init( hFileMap, pKnownModuleList );
			}
		}
	}
}

void CMiniDumpObject::InitFromFilename( const char *pszFilename, CUtlVector<module> *pKnownModuleList )
{
	HANDLE hFile = CreateFile(pszFilename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	InitFromHandle( hFile, pKnownModuleList );
}

void CMiniDumpObject::GetVersionString( char *pszOutput, version *pVersionInfo )
{
	char firstHighVer[10];
	char firstLowVer[10];
	char secondHighVer[10];
	char secondLowVer[10];

	int firstHigh = pVersionInfo->v1;
	int secondHigh = pVersionInfo->v2;
	int firstLow = pVersionInfo->v3;
	int secondLow = pVersionInfo->v4;

	itoa( firstHigh, firstHighVer, 10 );
	itoa( firstLow, firstLowVer, 10 );
	itoa( secondHigh, secondHighVer, 10 );
	itoa( secondLow, secondLowVer, 10 );
    
	strcat( pszOutput, firstHighVer );
	strcat( pszOutput, "." );
	strcat( pszOutput, secondHighVer );
	strcat( pszOutput, "." );
	strcat( pszOutput, firstLowVer );
	strcat( pszOutput, "." );
	strcat( pszOutput, secondLowVer );
}

version CMiniDumpObject::GetVersionStruct( VS_FIXEDFILEINFO *pVersionInfo )
{
	version returnVersion;
	int highVal = pVersionInfo->dwFileVersionMS;
	int lowVal = pVersionInfo->dwFileVersionLS;
	returnVersion.v1 = highVal >> 16;
	returnVersion.v2 = (highVal << 16)>>16;
	returnVersion.v3 = lowVal >> 16;
	returnVersion.v4 = (lowVal << 16)>>16;

	return returnVersion;
}

int CMiniDumpObject::ModuleListToListPanel( vgui::ListPanel *pTokenList, CUtlVector<module> *pModuleList, bool bCumulative, int startingModule)
{
	char keyNameBuf[1024] = "module";
	char modNumBuf[4] = "";
	int moduleNumber = startingModule;
		
	for ( int i = 0; i < pModuleList->Count(); i++ )
	{
		char version[20] = "";
		moduleNumber++;
		itoa( moduleNumber, modNumBuf, 10 );
		strcat( keyNameBuf, modNumBuf );
		module currentModule = pModuleList->Element(i);			
		GetVersionString( version, &currentModule.versionInfo );
		bool bRepeat = false;

		if ( bCumulative )
		{			
			int tokenCount = pTokenList->GetItemCount();
			for ( int j = 0; j < tokenCount; j++ )
			{
				KeyValues *kv = pTokenList->GetItem( j );
				const char *moduleName = strrchr( kv->GetString( "name" ), '\\' );
				const char *secondModuleName = strrchr( currentModule.name, '\\' );
				const char *checksum = kv->GetString( "checksum" );
        
				if ( !stricmp( moduleName, secondModuleName ) && !strcmp( kv->GetString( "version" ), version ) )
				{					
					int count = kv->GetInt( "count" );	
					int key = kv->GetInt( "key" );
					KeyValues *newKv = new KeyValues( keyNameBuf, "name", kv->GetString("name"), "checksum", checksum );
					newKv->SetString( "version", version );
					newKv->SetInt( "count", count+1 );						
					newKv->SetInt( "key", key );
					newKv->SetColor( "cellcolor", kv->GetColor( "cellcolor" ) );			
					pTokenList->RemoveItem( j );
					pTokenList->AddItem( newKv, j, false, false );
					bRepeat = true;
					break;
				}
			}
		}
        
		if ( !bRepeat )
		{
			KeyValues *kv = new KeyValues( keyNameBuf, "name", currentModule.name);	
			kv->SetString( "version", version );
			kv->SetInt( "key", currentModule.key );
			if ( bCumulative )
			{
				kv->SetInt( "count", 1 );
			}
			int colorValue = 255;
			if ( !currentModule.knownVersion )
                colorValue = 155;
			switch ( currentModule.myType )
			{
			case GOOD:
				kv->SetColor( "cellcolor", Color(0,colorValue,0,255));
				break;
			case BAD:
				kv->SetColor( "cellcolor", Color(colorValue,0,0,255));
				break;
			default:
				kv->SetColor( "cellcolor", Color(255,255,0,255));
				break;
			}
			pTokenList->AddItem( kv, i, false, false );
			bRepeat = false;
		}
		strcpy( keyNameBuf, "module");
	}
	return moduleNumber;
}


void CMiniDumpObject::PopulateListPanel( vgui::ListPanel *pTokenList, bool bCumulative )
{
	int nextModuleNumber = 0;
	nextModuleNumber = ModuleListToListPanel( pTokenList, &m_goodModuleList, bCumulative, nextModuleNumber );
	nextModuleNumber = ModuleListToListPanel( pTokenList, &m_unknownModuleList, bCumulative, nextModuleNumber );
	nextModuleNumber = ModuleListToListPanel( pTokenList, &m_badModuleList, bCumulative, nextModuleNumber);
	ModuleListToListPanel( pTokenList, &m_badChecksumList, bCumulative, nextModuleNumber );
}
