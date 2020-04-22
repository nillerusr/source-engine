//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: .360 file creation of PCF files
//
//=====================================================================================//

#include "MakeGameData.h"

bool CreateTargetFile_PCF( const char *pSourceName, const char *pTargetName, bool bWriteToZip )
{	
	DmxHeader_t header;
	CDmElement *pRoot;
	if ( g_pDataModel->RestoreFromFile( pSourceName, NULL, NULL, &pRoot, CR_DELETE_NEW, &header ) == DMFILEID_INVALID )
	{
		Msg( "CreateTargetFile_PCF: Error reading file \"%s\"!\n", pSourceName );
		return false;
	}

	const char *pOutFormat = header.formatName;
	if ( !g_pDataModel->FindFormatUpdater( pOutFormat ) )
	{
		pOutFormat = "dmx";
	}

	CUtlBuffer binaryBuffer;
	if ( !g_pDataModel->Serialize( binaryBuffer, "binary", pOutFormat, pRoot->GetHandle() ) )
	{
		Msg( "CreateTargetFile_PCF: Error writing buffer\n" );
		return false;
	}

	g_pDataModel->RemoveFileId( pRoot->GetFileId() );

	WriteBufferToFile( pTargetName, binaryBuffer, bWriteToZip, g_WriteModeForConversions );

	return true;
}
