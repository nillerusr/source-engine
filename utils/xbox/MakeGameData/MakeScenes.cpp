//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: .360 file creation of Choreo VCD files
//
//=====================================================================================//

#include "MakeGameData.h"
#include "sceneimage.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CDefaultStatus : public ISceneCompileStatus
{
public:
	virtual void UpdateStatus( char const *pchSceneName, bool bQuiet, int nIndex, int nCount )
	{
		if ( !bQuiet )
		{
			Msg( "Scenes: Compiling: %s\n", pchSceneName );
		}
	}
};

bool CreateSceneImageFile( char const *pchModPath, bool bWriteToZip, bool bLittleEndian, bool bQuiet, DiskWriteMode_t eWriteModeForConversions )
{
	CUtlBuffer	targetBuffer;

	const char *pFilename = bLittleEndian ? "scenes/scenes.image" : "scenes/scenes.360.image";

	CDefaultStatus statusHelper;

	bool bSuccess = g_pSceneImage->CreateSceneImageFile( targetBuffer, pchModPath, bLittleEndian, bQuiet, &statusHelper );
	if ( bSuccess )
	{
		bSuccess = WriteBufferToFile( pFilename, targetBuffer, bWriteToZip, eWriteModeForConversions );
	}

	return bSuccess;
}