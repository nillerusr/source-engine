//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: .360.VTF Creation
//
//=====================================================================================//

#include "MakeGameData.h"
#include "vtf/vtf.h"

//-----------------------------------------------------------------------------
// Purpose: Create the 360 VTF, use the library!
//-----------------------------------------------------------------------------
bool CreateTargetFile_VTF( const char *pSourceName, const char *pTargetName, bool bWriteToZip )
{
	CUtlBuffer	sourceBuffer;
	CUtlBuffer	targetBuffer;

	if ( !scriptlib->ReadFileToBuffer( pSourceName, sourceBuffer ) )
	{
		return false;
	}

	// using library conversion routine
	bool bSuccess = ConvertVTFTo360Format( pSourceName, sourceBuffer, targetBuffer, CompressCallback );
	if ( bSuccess )
	{
		bSuccess = WriteBufferToFile( pTargetName, targetBuffer, bWriteToZip, g_WriteModeForConversions );
	}

	return bSuccess;
}

//-----------------------------------------------------------------------------
// Get the preload data for a vtf file
//-----------------------------------------------------------------------------
bool GetPreloadData_VTF( const char *pFilename, CUtlBuffer &fileBufferIn, CUtlBuffer &preloadBufferOut )
{
	if ( !GetVTFPreload360Data( pFilename, fileBufferIn, preloadBufferOut ) )
	{
		Msg( "Can't preload: '%s', has bad version\n", pFilename );
		return false;
	}

	return true;
}