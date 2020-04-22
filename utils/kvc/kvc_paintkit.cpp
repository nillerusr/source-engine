//===================== Copyright (c) Valve Corporation. All Rights Reserved. ======================
//
//==================================================================================================


#if defined( WIN32 ) && !defined( _X360 )
#include <windows.h> // SRC only!!
#endif // 

#include "filesystem.h"
#include "tier0/icommandline.h"
#include "tier1/keyvalues.h"
#include "tier2/keyvaluesmacros.h"


#include "kvc_paintkit.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//--------------------------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------------------------
static void WriteIndent( IBaseFileSystem *pFileSystem, FileHandle_t hFile, bool bOptTabs, int nOptTabWidth, int nIndentLevel )
{
	if ( nIndentLevel <= 0 )
		return;

	if ( bOptTabs || nOptTabWidth <= 0 )
	{
		char *szTabIndent = ( char * )stackalloc( nIndentLevel + 1 );
		V_memset( szTabIndent, '\t', nIndentLevel );
		szTabIndent[nIndentLevel] = '\0';
		pFileSystem->Write( szTabIndent, nIndentLevel, hFile );
	}
	else
	{
		const int nSpaceCount = nIndentLevel * nOptTabWidth;
		char *szSpaceIndent = ( char * )stackalloc( nSpaceCount + 1 );
		V_memset( szSpaceIndent, ' ', nSpaceCount );
		szSpaceIndent[nSpaceCount] = '\0';
		pFileSystem->Write( szSpaceIndent, nSpaceCount, hFile );
	}
}


//--------------------------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------------------------
static void WritePadding( IBaseFileSystem *pFileSystem, FileHandle_t hFile, bool bOptTabs, int nOptTabWidth, int nCurrentColumn, int nTargetColumn )
{
	if ( nCurrentColumn <= 0 || nTargetColumn <= 0 || nTargetColumn <= nCurrentColumn )
	{
		WriteIndent( pFileSystem, hFile, bOptTabs, nOptTabWidth, 1 );
		return;
	}

	if ( bOptTabs || nOptTabWidth <= 0 )
	{
		if ( nOptTabWidth <= 0 )
		{
			nOptTabWidth = 4;	// Can't handle 0 tab width, use 4
		}

		nTargetColumn = ( nTargetColumn + nOptTabWidth - 1 ) / nOptTabWidth * nOptTabWidth;	// Round to next tab stop

		Assert( nTargetColumn % nOptTabWidth == 0 );

		const int nTabCount = ( nTargetColumn - nCurrentColumn ) / nOptTabWidth;

		char *szTabPadding = ( char * )stackalloc( nTabCount + 1 );
		V_memset( szTabPadding, '\t', nTabCount );
		szTabPadding[nTabCount] = '\0';
		pFileSystem->Write( szTabPadding, nTabCount, hFile );
	}
	else
	{
		nTargetColumn = ( nTargetColumn + nOptTabWidth - 1 ) / nOptTabWidth * nOptTabWidth;	// Round to next tab stop

		Assert( nTargetColumn % nOptTabWidth == 0 );

		const int nSpaceCount = nTargetColumn - nCurrentColumn;
		char *szSpacePadding = ( char * )stackalloc( nSpaceCount + 1 );
		V_memset( szSpacePadding, ' ', nSpaceCount );
		szSpacePadding[nSpaceCount] = '\0';
		pFileSystem->Write( szSpacePadding, nSpaceCount, hFile );
	}
}


//--------------------------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------------------------
static void WriteConvertedString( IBaseFileSystem *pFileSystem, FileHandle_t hFile, const char *pszString )
{
	// handle double quote chars within the string
	// the worst possible case is that the whole string is quotes
	int nSrcLen = Q_strlen( pszString );
	char *szConvertedString = ( char * )stackalloc( ( nSrcLen + 1 )  * sizeof( char ) * 2 );
	int j = 0;
	for ( int i = 0; i <= nSrcLen; ++i )
	{
		if ( pszString[i] == '\"' )
		{
			szConvertedString[j] = '\\';
			++j;
		}
		szConvertedString[j] = pszString[i];
		++j;
	}

	pFileSystem->Write( szConvertedString, V_strlen( szConvertedString ), hFile );
}


//--------------------------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------------------------
template <size_t maxLenInChars> static int CleanFloatString( OUT_Z_ARRAY char (&pszFloatBuf)[maxLenInChars] )
{ 
	int nStrLen = V_strlen( pszFloatBuf );

	const char *pDecimal = V_strnchr( pszFloatBuf, '.', maxLenInChars - 1 );
	if ( pDecimal )
	{
		++pDecimal;	// Keep number after decimal always

		char *pToStrip = pszFloatBuf + nStrLen - 1;
		while ( pToStrip > pDecimal && *pToStrip == '0' )
		{
			*pToStrip = '\0';
			--pToStrip;
			--nStrLen;
		}
	}

	return nStrLen;
}


//--------------------------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------------------------
static void SaveToFile_R( KeyValues *pkv, IBaseFileSystem *pFileSystem, FileHandle_t hFile, bool bOptTabs, int nOptTabWidth, int nIndentLevel, int nValueColumn = -1 )
{
	WriteIndent( pFileSystem, hFile, bOptTabs, nOptTabWidth, nIndentLevel );
	pFileSystem->Write( "\"", 1, hFile );
	WriteConvertedString( pFileSystem, hFile, pkv->GetName() );
	pFileSystem->Write( "\"", 1, hFile );

	if ( pkv->GetFirstSubKey() )
	{
		int nMaxSubKeyNameLength = 0;

		for ( KeyValues *pkvSubKey = pkv->GetFirstSubKey(); pkvSubKey; pkvSubKey = pkvSubKey->GetNextKey() )
		{
			const int nNameLength = V_strlen( pkvSubKey->GetName() );
			nMaxSubKeyNameLength = Max( nNameLength, nMaxSubKeyNameLength );
		}

		int nSubKeyValueColumn = -1;

		if ( nMaxSubKeyNameLength > 0 )
		{
			nSubKeyValueColumn = nMaxSubKeyNameLength + 1;
		}

		pFileSystem->Write( "\n", 1, hFile );
		WriteIndent( pFileSystem, hFile, bOptTabs, nOptTabWidth, nIndentLevel );
		pFileSystem->Write( "{\n", 2, hFile );
		for ( KeyValues *pkvSubKey = pkv->GetFirstSubKey(); pkvSubKey; pkvSubKey = pkvSubKey->GetNextKey() )
		{
			SaveToFile_R( pkvSubKey, pFileSystem, hFile, bOptTabs, nOptTabWidth, nIndentLevel + 1, nSubKeyValueColumn );
		}
		WriteIndent( pFileSystem, hFile, bOptTabs, nOptTabWidth, nIndentLevel );
		pFileSystem->Write( "}\n", 2, hFile );
	}
	else
	{
		if ( nValueColumn > 0 )
		{
			const int nNameLength = V_strlen( pkv->GetName() );
			WritePadding( pFileSystem, hFile, bOptTabs, nOptTabWidth, nNameLength, nValueColumn );
		}
		else
		{
			WriteIndent( pFileSystem, hFile, bOptTabs, nOptTabWidth, 1 );	// Don't know which column to align things on
		}

		pFileSystem->Write( "\"", 1, hFile );

		switch ( pkv->GetDataType() )
		{
		case KeyValues::TYPE_STRING:
		{
			const char *pszStringValue = pkv->GetString();
			if ( pszStringValue )
			{
				WriteConvertedString( pFileSystem, hFile, pszStringValue );
			}
			break;
		}
		case KeyValues::TYPE_WSTRING:
		{
			const wchar_t *pszWString = pkv->GetWString();
			if ( pszWString )
			{
				const size_t nTmpBufSize = 4096;
				char *szTmpBuf = ( char * )stackalloc( nTmpBufSize * sizeof( char ) );
				V_memset( szTmpBuf, '\0', nTmpBufSize );
				const int nResult = Q_UnicodeToUTF8( pszWString, szTmpBuf, nTmpBufSize );
				if ( nResult != 0 )
				{
					WriteConvertedString( pFileSystem, hFile, szTmpBuf );
				}
			}
			break;
		}
		case KeyValues::TYPE_INT:
		{
			char szTmpBuf[32] = {};
			V_snprintf( szTmpBuf, sizeof( szTmpBuf ), "%d", pkv->GetInt() );
			pFileSystem->Write( szTmpBuf, V_strlen( szTmpBuf ), hFile );
			break;
		}
		case KeyValues::TYPE_UINT64:
		{
			char szTmpBuf[32] = {};
			// write "0x" + 16 char 0-padded hex encoded 64 bit value
#ifdef WIN32
			Q_snprintf( szTmpBuf, sizeof( szTmpBuf ), "0x%016I64X", *( ( uint64 * )pkv->GetUint64() ) );
#else
			Q_snprintf( szTmpBuf, sizeof( szTmpBuf ), "0x%016llX", *( ( uint64 * )pkv->GetUint64() ) );
#endif
			pFileSystem->Write( szTmpBuf, V_strlen( szTmpBuf ), hFile );
			break;
		}
		case KeyValues::TYPE_FLOAT:
		{
			char szTmpBuf[32] = {};
			V_snprintf( szTmpBuf, sizeof( szTmpBuf ), "%f", pkv->GetFloat() );
			int nStrLen = V_strlen( szTmpBuf );
			nStrLen = CleanFloatString( szTmpBuf );
			pFileSystem->Write( szTmpBuf, nStrLen, hFile );
			break;
		}
		case KeyValues::TYPE_COLOR:
		{
			char szTmpBuf[32] = {};
			const Color c = pkv->GetColor();
			V_snprintf( szTmpBuf, sizeof( szTmpBuf ), "%d", c.r() );
			pFileSystem->Write( " ", 1, hFile );
			V_snprintf( szTmpBuf, sizeof( szTmpBuf ), "%d", c.g() );
			pFileSystem->Write( " ", 1, hFile );
			V_snprintf( szTmpBuf, sizeof( szTmpBuf ), "%d", c.b() );
			pFileSystem->Write( " ", 1, hFile );
			V_snprintf( szTmpBuf, sizeof( szTmpBuf ), "%d", c.a() );
			break;
		}
		default:
			break;
		}
		pFileSystem->Write( "\"\n", 2, hFile );
	}
}


//--------------------------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------------------------
static bool SaveCleanKeyValuesToFile( KeyValues *pkv, IBaseFileSystem *pFileSystem, const char *pszFileName, const char *pszPathID = nullptr, bool bOptTabs = true, int nOptSpaceIndent = 4 )
{
	// Write out KeyValues to the specified file but cleaner than KeyValues::SaveToFile
	// create a write file
	FileHandle_t hFile = pFileSystem->Open( pszFileName, "wb", pszPathID );

	if ( hFile == FILESYSTEM_INVALID_HANDLE )
	{
		Msg( "CleanSaveKeyValuesToFile: Couldn't open file \"%s\" for writing in path \"%s\".\n",
			pszFileName ? pszFileName : "NULL", pszPathID ? pszPathID : "NULL" );
		return false;
	}

	for ( KeyValues *pkvTmp = pkv; pkvTmp; pkvTmp = pkvTmp->GetNextKey() )
	{
		SaveToFile_R( pkvTmp, pFileSystem, hFile, bOptTabs, nOptSpaceIndent, 0 );
	}

	pFileSystem->Close( hFile );

	return true;
}


//--------------------------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------------------------
void ProcessPaintKitKeyValuesFile( const char *pszPaintKitKeyValuesFile, bool bOptFix, bool bOptVerbose )
{
	if ( bOptVerbose )
	{
		Msg( "Processing: %s\n", pszPaintKitKeyValuesFile );
	}

	char szFullPath[MAX_PATH] = {};

	if ( !V_IsAbsolutePath( pszPaintKitKeyValuesFile ) )
	{
		g_pFullFileSystem->RelativePathToFullPath_safe( pszPaintKitKeyValuesFile, nullptr, szFullPath );
		if ( !V_IsAbsolutePath( szFullPath ) )
		{
			char szTmpA[MAX_PATH] = {};
			char szTmpB[MAX_PATH] = {};

			if ( !g_pFullFileSystem->GetCurrentDirectoryA( szTmpA, ARRAYSIZE( szTmpB ) ) )
			{
				Msg( "Error: non-resolvable, non-absolute path specified (%s) and couldn't determine current directory\n", pszPaintKitKeyValuesFile );
				return;
			}

			V_ComposeFileName( szTmpA, pszPaintKitKeyValuesFile, szTmpB, ARRAYSIZE( szTmpB ) );
			V_FixupPathName( szTmpA, ARRAYSIZE( szTmpA ), szTmpB );
			g_pFullFileSystem->GetCaseCorrectFullPath( szTmpB, szFullPath );
		}
	}

	if ( !V_IsAbsolutePath( szFullPath ) )
	{
		Msg( "Error: Couldn't find absolute path to file: %s\n", pszPaintKitKeyValuesFile );
		return;
	}

	if ( !g_pFullFileSystem->FileExists( szFullPath ) )
	{
		Msg( "Error: Couldn't find file: %s\n", szFullPath );
		return;
	}

	KeyValues *pkv = new KeyValues( "" );
	const bool bLoaded = pkv->LoadFromFile( g_pFullFileSystem, szFullPath );
	if ( bLoaded )
	{
		for ( KeyValues *pkvTmp = pkv; pkvTmp; pkvTmp = pkvTmp->GetNextKey() )
		{
			HandleKeyValuesMacros( pkvTmp );
		}

		char szFilename[MAX_PATH] = {};

		if ( bOptFix )
		{
			V_strcpy_safe( szFilename, szFullPath );
			V_strcat_safe( szFilename, "_fix" );

			if ( bOptVerbose )
			{
				Msg( "    Saving: %s\n", szFilename );
			}

			if ( !SaveCleanKeyValuesToFile( pkv, g_pFullFileSystem, szFilename, nullptr, false ) )
			{
				Msg( "Error: Couldn't write: %s\n", szFilename );
			}
		}
		else
		{
			V_strcpy_safe( szFilename, szFullPath );
			V_strcat_safe( szFilename, "_bak" );

			if ( bOptVerbose )
			{
				Msg( "   Copying: %s -> %s\n", szFullPath, szFilename );
			}

			if ( CopyFile( szFullPath, szFilename, false ) )
			{
				if ( bOptVerbose )
				{
					Msg( "    Saving: %s\n", szFullPath );
				}

				if ( !SaveCleanKeyValuesToFile( pkv, g_pFullFileSystem, szFullPath, nullptr, false ) )
				{
					Msg( "Error: Couldn't write: %s\n", szFullPath );
				}
			}
			else
			{
				Msg( "Error: Couldn't copy %s to %s, aborting\n", szFullPath, szFilename );
			}
		}

		pkv->SaveToFile( g_pFullFileSystem, "c:/tmp/bar.txt" );
	}
	else
	{
		Msg( "   + Couldn't Load: %s\n", szFullPath );
	}

	pkv->deleteThis();
}


//--------------------------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------------------------
void ProcessPaintKitKeyValuesFiles( const CUtlVector< CUtlSymbol > &workList )
{
	// This bit of hackery allows us to access files on the harddrive
	g_pFullFileSystem->AddSearchPath( "", "LOCAL", PATH_ADD_TO_HEAD );

	const bool bOptFix = CommandLine()->CheckParm( "-f" ) != nullptr;
	const bool bOptVerbose = CommandLine()->CheckParm( "-v" ) != nullptr;

	for ( int i = 0; i < workList.Count(); ++i )
	{
		ProcessPaintKitKeyValuesFile( workList[i].String(), bOptFix, bOptVerbose );
	}
}