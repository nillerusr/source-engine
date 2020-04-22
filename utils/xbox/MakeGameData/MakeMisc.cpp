//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Conversion for general wierd files.
//
//=====================================================================================//

#include "MakeGameData.h"

//-----------------------------------------------------------------------------
// The DX Support file is a very fat expensive KV file, causes a run-time startup slowdown.
// Becauase it normally lives in the game\bin directory, it can't be in the zip or preloaded.
// Thus, it gets reprocessed into just the trivial 360 portion and placed into the platform.zip
// Yes, it's evil.
//-----------------------------------------------------------------------------
bool ProcessDXSupportConfig( bool bWriteToZip )
{
	if ( !g_bIsPlatformZip )
	{
		// only relevant when building platform zip, otherwise no-op
		return false;
	}

	const char *pConfigName = "dxsupport.cfg";
	char szTempPath[MAX_PATH];
	char szSourcePath[MAX_PATH];
	V_ComposeFileName( g_szModPath, "../bin", szTempPath, sizeof( szTempPath ) );
	V_ComposeFileName( szTempPath, pConfigName, szSourcePath, sizeof( szSourcePath ) );

	CUtlBuffer sourceBuf( 0, 0, CUtlBuffer::TEXT_BUFFER );
	if ( !g_pFullFileSystem->ReadFile( szSourcePath, NULL, sourceBuf ) )
	{
		Msg( "Error! Couldn't open file '%s'!\n", pConfigName ); 
		return false;
	}

	KeyValues *pKV = new KeyValues( "" );
	if ( !pKV->LoadFromBuffer( "dxsupport.cfg", sourceBuf ) )
	{
		Msg( "Error! Couldn't parse config file '%s'!\n", pConfigName ); 
		pKV->deleteThis();
		return false;
	}

	// only care about the xbox specific dxlevel 98 block
	KeyValues *pXboxSubKey = NULL;
	for ( KeyValues *pSubKey = pKV->GetFirstSubKey(); pSubKey != NULL && pXboxSubKey == NULL; pSubKey = pSubKey->GetNextKey() )
	{
		// descend each sub block
		for ( KeyValues *pKey = pSubKey->GetFirstSubKey(); pKey != NULL && pXboxSubKey == NULL; pKey = pKey->GetNextKey() )
		{
			if ( !V_stricmp( pKey->GetName(), "name" ) && pKey->GetInt( (const char *)NULL ) == 98 )
			{
				pXboxSubKey = pSubKey;
			}
		}
	}
	
	if ( !pXboxSubKey )
	{
		Msg( "Error! Couldn't find expected dxlevel 98 in config file '%s'!\n", pConfigName ); 
		pKV->deleteThis();
		return false;
	}

	CUtlBuffer kvBuffer( 0, 0, CUtlBuffer::TEXT_BUFFER );
	kvBuffer.Printf( "\"dxsupport\"\n" );
	kvBuffer.Printf( "{\n" );
	kvBuffer.Printf( "\t\"0\"\n" );
	kvBuffer.Printf( "\t{\n" );
	for ( KeyValues *pKey = pXboxSubKey->GetFirstSubKey(); pKey != NULL; pKey = pKey->GetNextKey() )
	{
		kvBuffer.Printf( "\t\t\"%s\" \"%s\"\n", pKey->GetName(), pKey->GetString( (const char *)NULL ) );
	}
	kvBuffer.Printf( "\t}\n" );
	kvBuffer.Printf( "}\n" );

	CUtlBuffer targetBuf( 0, 0, CUtlBuffer::TEXT_BUFFER | CUtlBuffer::CONTAINS_CRLF );
	kvBuffer.ConvertCRLF( targetBuf );

	// only appears in zip file
	bool bSuccess = WriteBufferToFile( pConfigName, targetBuf, bWriteToZip, WRITE_TO_DISK_NEVER );

	pKV->deleteThis();

	return bSuccess;
}