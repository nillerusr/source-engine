//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#pragma once

#include <windows.h>
#include <mmreg.h>
#include <stdio.h>
#include <stdlib.h>
#include <direct.h>
#include <io.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utime.h>
#include "tier0/dbg.h"
#include "tier0/icommandline.h"
#include "appframework/appframework.h"
#include "characterset.h"
#include "tier1/strtools.h"
#include "tier1/UtlVector.h"
#include "tier1/UtlBuffer.h"
#include "tier1/UtlString.h"
#include "tier1/UtlRBTree.h"
#include "tier1/UtlDict.h"
#include "tier1/UtlSortVector.h"
#include "tier1/UtlStringMap.h"
#include "tier1/UtlSymbol.h"
#include "tier1/KeyValues.h"
#include "tier1/lzss.h"
#include "lzma/lzma.h"
#include "datamap.h"
#include "XZipTool.h"
#include "../../common/scriplib.h"
#include "../../common/cmdlib.h"
#include "tier2/tier2.h"
#include "dmserializers/idmserializers.h"
#include "datamodel/dmattribute.h"
#include "datamodel/dmelement.h"
#include "studiobyteswap.h"
#include "studio.h"
#include "vphysics_interface.h"
#include "materialsystem/IMaterial.h"
#include "materialsystem/hardwareverts.h"
#include "optimize.h"
#include "soundchars.h"
#include "mdllib/mdllib.h"

enum fileType_e
{
	FILETYPE_UNKNOWN = 0,
	FILETYPE_WAV,
	FILETYPE_VTF,
	FILETYPE_MDL,
	FILETYPE_ANI,
	FILETYPE_VTX,
	FILETYPE_VVD,
	FILETYPE_PHY,
	FILETYPE_BSP,
	FILETYPE_AIN,
	FILETYPE_CCDAT,
	FILETYPE_MP3,
	FILETYPE_RESLST,
	FILETYPE_PCF,
	FILETYPE_SCENEIMAGE
};

typedef struct
{
	int			result;
	CUtlString	fileName;
} errorList_t;

//-----------------------------------------------------------------------------
// MakeGameData.cpp
//-----------------------------------------------------------------------------


bool								IsLocalizedFile( const char *pFileName );
bool								IsLocalizedFileValid( const char *pFileName, const char *pLanguageSuffix = NULL );
bool								CompressCallback( CUtlBuffer &inputBuffer, CUtlBuffer &outputBuffer );
bool								ReadFileToBuffer( const char *pSourceName, CUtlBuffer &buffer, bool bText, bool bNoOpenFailureWarning );
bool								WriteBufferToFile( const char *pTargetName, CUtlBuffer &buffer, bool bWriteToZip, DiskWriteMode_t writeMode );
fileType_e							ResolveFileType( const char *pSourceName, char *pTargetName, int targetNameSize );

extern DiskWriteMode_t				g_WriteModeForConversions;
extern CXZipTool					g_MasterXZip;
extern char							g_szGamePath[];
extern char							g_szModPath[];
extern bool							g_bModPathIsValid;
extern const char					*g_GameNames[];
extern bool							g_bForce;
extern bool							g_bQuiet;
extern bool							g_bMakeZip;
extern bool							g_bIsPlatformZip;
extern IPhysicsCollision			*g_pPhysicsCollision;
extern char							g_szSourcePath[];
extern CUtlVector< errorList_t >	g_errorList;

//-----------------------------------------------------------------------------
// MakeTextures.cpp
//-----------------------------------------------------------------------------
bool CreateTargetFile_VTF( const char *pSourceName, const char *pTargetName, bool bWriteToZip );
bool GetPreloadData_VTF( const char *pFilename, CUtlBuffer &fileBufferIn, CUtlBuffer &preloadBufferOut );

//-----------------------------------------------------------------------------
// MakeScenes.cpp
//-----------------------------------------------------------------------------
bool CreateSceneImageFile( char const *pchModPath, bool bWriteToZip, bool bLittleEndian, bool bQuiet, DiskWriteMode_t eWriteModeForConversions );

//-----------------------------------------------------------------------------
// MakeSounds.cpp
//-----------------------------------------------------------------------------
bool CreateTargetFile_WAV( const char *pSourceName, const char *pTargetName, bool bWriteToZip );
bool CreateTargetFile_MP3( const char *pSourceName, const char *pTargetName, bool bWriteToZip );
bool GetPreloadData_WAV( const char *pFilename, CUtlBuffer &fileBufferIn, CUtlBuffer &preloadBufferOut );

//-----------------------------------------------------------------------------
// MakeMaps.cpp
//-----------------------------------------------------------------------------
bool CreateTargetFile_BSP( const char *pSourceName, const char *pTargetName, bool bWriteToZip );
bool CreateTargetFile_AIN( const char *pSourceName, const char *pTargetName, bool bWriteToZip );
bool GetDependants_BSP( const char *pBspName, CUtlVector< CUtlString > *pList );

//-----------------------------------------------------------------------------
// MakeResources.cpp
//-----------------------------------------------------------------------------
bool CreateTargetFile_CCDAT( const char *pSourceName, const char *pTargetName, bool bWriteToZip );
bool CreateTargetFile_RESLST( const char *pSourceName, const char *pTargetName, bool bWriteToZip );

//-----------------------------------------------------------------------------
// MakeModels.cpp
//-----------------------------------------------------------------------------
bool InitStudioByteSwap( void );
void ShutdownStudioByteSwap( void );
bool CreateTargetFile_Model( const char *pSourceName, const char *pTargetName, bool bWriteToZip );
bool GetDependants_MDL( const char *pModelName, CUtlVector< CUtlString > *pList );
bool GetPreloadData_VHV( const char *pFilename, CUtlBuffer &fileBufferIn, CUtlBuffer &preloadBufferOut );
bool GetPreloadData_VTX( const char *pFilename, CUtlBuffer &fileBufferIn, CUtlBuffer &preloadBufferOut );
bool GetPreloadData_VVD( const char *pFilename, CUtlBuffer &fileBufferIn, CUtlBuffer &preloadBufferOut );
bool PreprocessModelFiles( CUtlVector<fileList_t> &fileList );

//-----------------------------------------------------------------------------
// MakeShaders.cpp
//-----------------------------------------------------------------------------
bool GetPreloadData_VCS( const char *pFilename, CUtlBuffer &fileBufferIn, CUtlBuffer &preloadBufferOut );

//-----------------------------------------------------------------------------
// MakeMisc.cpp
//-----------------------------------------------------------------------------
bool ProcessDXSupportConfig( bool bWriteToZip );

//-----------------------------------------------------------------------------
// MakeResources.cpp
//-----------------------------------------------------------------------------
bool CreateTargetFile_PCF( const char *pSourceName, const char *pTargetName, bool bWriteToZip );

//-----------------------------------------------------------------------------
// MakeZip.cpp
//-----------------------------------------------------------------------------
void SetupCriticalPreloadScript( const char *pModPath );
