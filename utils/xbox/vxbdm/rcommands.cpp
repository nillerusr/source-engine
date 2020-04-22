//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Xbox Remote Commands
//
//=====================================================================================//

#include "xbox/xbox_console.h"
#include "xbox/xbox_vxconsole.h"
#include "tier0/tslist.h"
#include "tier0/memdbgon.h"

CInterlockedInt	g_xbx_numProfileCounters;
unsigned int	g_xbx_profileCounters[XBX_MAX_PROFILE_COUNTERS];
char			g_xbx_profileName[32];

//-----------------------------------------------------------------------------
//	XBX_rSetProfileAttributes
//
//	Expose profile counters attributes to the console.
//-----------------------------------------------------------------------------
int CXboxConsole::SetProfileAttributes( const char *pProfileName, int numCounters, const char *names[], unsigned int colors[] )
{
	char			dbgCommand[XBX_MAX_RCMDLENGTH];
	int				retVal;
	xrProfile_t*	profileList;
	
	if ( numCounters > XBX_MAX_PROFILE_COUNTERS )
	{
		numCounters = XBX_MAX_PROFILE_COUNTERS;
	}

	profileList = new xrProfile_t[numCounters];
	memset( profileList, 0, numCounters*sizeof( xrProfile_t ) );

	for ( int i=0; i<numCounters; i++ )
	{
		strncpy( profileList[i].labelString, names[i], sizeof( profileList[i].labelString ) );
		profileList[i].labelString[sizeof( profileList[i].labelString ) - 1] = '\0';

		profileList[i].color = colors[i];
	}

	_snprintf( dbgCommand, sizeof( dbgCommand ), "SetProfile() %s 0x%8.8x 0x%8.8x 0x%8.8x", pProfileName, numCounters, profileList, &retVal );
	XBX_SendRemoteCommand( dbgCommand, false );

	delete[] profileList;

	return ( retVal );
}

//-----------------------------------------------------------------------------
//	XBX_rSetProfileData
//
//	Expose profile counters to the console. Expected to be called once per game frame.
//-----------------------------------------------------------------------------
void CXboxConsole::SetProfileData( const char *pProfileName, int numCounters, unsigned int *pCounters )
{
	static unsigned int lastTime = 0;
	unsigned int		time;

	// not faster than 10Hz
	time = GetTickCount();
	if ( time - lastTime < 100 )
	{
		return;
	}

	if ( g_xbx_numProfileCounters == 0 )
	{
		_snprintf( g_xbx_profileName, sizeof( g_xbx_profileName ), pProfileName );

		if ( numCounters > XBX_MAX_PROFILE_COUNTERS )
		{
			numCounters = XBX_MAX_PROFILE_COUNTERS;
		}
		memcpy( g_xbx_profileCounters, pCounters, numCounters * sizeof( unsigned int ) );	

		// mark for sending
		g_xbx_numProfileCounters = numCounters;

		lastTime = time;
	}
}

//-----------------------------------------------------------------------------
//	XBX_rMemDump
//
//	Send signal to remote console to read mempry dump in specified filename.
//-----------------------------------------------------------------------------
int CXboxConsole::MemDump( const char *pDumpFileName )
{
	char	dbgCommand[XBX_MAX_RCMDLENGTH];

	_snprintf( dbgCommand, sizeof( dbgCommand ), "MemDump() %s", pDumpFileName );
	XBX_SendRemoteCommand( dbgCommand, true );

	return 0;
}

//-----------------------------------------------------------------------------
//	XBX_rTimeStampLog
//
//	Send time stamp to remote console
//-----------------------------------------------------------------------------
int CXboxConsole::TimeStampLog( float time, const char *pString )
{
	char				dbgCommand[XBX_MAX_RCMDLENGTH];
	int					retVal;
	xrTimeStamp_t		timeStamp;
	MEMORYSTATUS		stat;
	static int			lastMemoryStamp = 0;
	static float		lastTimeStamp = 0;

	// get current available memory
	GlobalMemoryStatus( &stat );

	if ( !lastTimeStamp || time < lastTimeStamp )
	{
		// first entry or restart, reset stamps
		lastMemoryStamp = stat.dwAvailPhys;
		lastTimeStamp = time;
	}

	timeStamp.time = time;
	timeStamp.deltaTime = time - lastTimeStamp;
	timeStamp.memory = stat.dwAvailPhys;
	timeStamp.deltaMemory = stat.dwAvailPhys - lastMemoryStamp;

	strncpy( timeStamp.messageString, pString, sizeof( timeStamp.messageString ) );
	timeStamp.messageString[sizeof( timeStamp.messageString ) - 1] = '\0';

	_snprintf( dbgCommand, sizeof( dbgCommand ), "TimeStampLog() 0x%8.8x 0x%8.8x", &timeStamp, &retVal );
	XBX_SendRemoteCommand( dbgCommand, false );

	lastTimeStamp   = time;
	lastMemoryStamp = stat.dwAvailPhys;

	return retVal;
}

//-----------------------------------------------------------------------------
//	XBX_rMaterialList
//
//	Send material list to remote console
//-----------------------------------------------------------------------------
int CXboxConsole::MaterialList( int nMaterials, const xMaterialList_t* pXMaterialList )
{
	char				dbgCommand[XBX_MAX_RCMDLENGTH];
	int					retVal;
	xrMaterial_t*		pRemoteList;
	
	pRemoteList = new xrMaterial_t[nMaterials];
	memset( pRemoteList, 0, nMaterials*sizeof( xrMaterial_t ) );

	for ( int i=0; i<nMaterials; i++ )
	{
		strncpy( pRemoteList[i].nameString, pXMaterialList[i].pName, sizeof( pRemoteList[i].nameString ) );
		pRemoteList[i].nameString[sizeof( pRemoteList[i].nameString ) - 1] = '\0';

		strncpy( pRemoteList[i].shaderString, pXMaterialList[i].pShaderName, sizeof( pRemoteList[i].shaderString ) );
		pRemoteList[i].shaderString[sizeof( pRemoteList[i].shaderString ) - 1] = '\0';

		pRemoteList[i].refCount = pXMaterialList[i].refCount;
	}

	_snprintf( dbgCommand, sizeof( dbgCommand ), "MaterialList() 0x%8.8x 0x%8.8x 0x%8.8x", nMaterials, pRemoteList, &retVal );
	XBX_SendRemoteCommand( dbgCommand, false );

	delete [] pRemoteList;

	return retVal;
}

//-----------------------------------------------------------------------------
//	XBX_rTextureList
//
//	Send texture list to remote console
//-----------------------------------------------------------------------------
int CXboxConsole::TextureList( int nTextures, const xTextureList_t* pXTextureList )
{
	char			dbgCommand[XBX_MAX_RCMDLENGTH];
	int				retVal;
	xrTexture_t*	pRemoteList;
	
	pRemoteList = new xrTexture_t[nTextures];
	memset( pRemoteList, 0, nTextures*sizeof( xrTexture_t ) );

	for ( int i=0; i<nTextures; i++ )
	{
		strncpy( pRemoteList[i].nameString, pXTextureList[i].pName, sizeof( pRemoteList[i].nameString ) );
		pRemoteList[i].nameString[sizeof( pRemoteList[i].nameString ) - 1] = '\0';

		strncpy( pRemoteList[i].groupString, pXTextureList[i].pGroupName, sizeof( pRemoteList[i].groupString ) );
		pRemoteList[i].groupString[sizeof( pRemoteList[i].groupString ) - 1] = '\0'; 

		strncpy( pRemoteList[i].formatString, pXTextureList[i].pFormatName, sizeof( pRemoteList[i].formatString ) );
		pRemoteList[i].formatString[sizeof( pRemoteList[i].formatString ) - 1] = '\0';

		pRemoteList[i].size = pXTextureList[i].size;
		pRemoteList[i].width = pXTextureList[i].width;
		pRemoteList[i].height = pXTextureList[i].height;
		pRemoteList[i].depth = pXTextureList[i].depth;
		pRemoteList[i].numLevels = pXTextureList[i].numLevels;
		pRemoteList[i].binds = pXTextureList[i].binds;
		pRemoteList[i].refCount = pXTextureList[i].refCount;
		pRemoteList[i].sRGB = pXTextureList[i].sRGB;
		pRemoteList[i].edram = pXTextureList[i].edram;
		pRemoteList[i].procedural = pXTextureList[i].procedural;
		pRemoteList[i].fallback = pXTextureList[i].fallback;
		pRemoteList[i].final = pXTextureList[i].final;
		pRemoteList[i].failed = pXTextureList[i].failed;
	}

	_snprintf( dbgCommand, sizeof( dbgCommand ), "TextureList() 0x%8.8x 0x%8.8x 0x%8.8x", nTextures, pRemoteList, &retVal );
	XBX_SendRemoteCommand( dbgCommand, false );

	delete [] pRemoteList;

	return ( retVal );
}

//-----------------------------------------------------------------------------
//	XBX_rSoundList
//
//	Send sound list to remote console
//-----------------------------------------------------------------------------
int CXboxConsole::SoundList( int nSounds, const xSoundList_t* pXSoundList )
{
	char		dbgCommand[XBX_MAX_RCMDLENGTH];
	int			retVal;
	xrSound_t*	pRemoteList;
	
	pRemoteList = new xrSound_t[nSounds];
	memset( pRemoteList, 0, nSounds*sizeof( xrSound_t ) );

	for ( int i=0; i<nSounds; i++ )
	{
		strncpy( pRemoteList[i].nameString, pXSoundList[i].name, sizeof( pRemoteList[i].nameString ) );
		pRemoteList[i].nameString[sizeof( pRemoteList[i].nameString ) - 1] = '\0';

		strncpy( pRemoteList[i].formatString, pXSoundList[i].formatName, sizeof( pRemoteList[i].formatString ) );
		pRemoteList[i].formatString[sizeof( pRemoteList[i].formatString ) - 1] = '\0';

		pRemoteList[i].rate          = pXSoundList[i].rate;
		pRemoteList[i].bits          = pXSoundList[i].bits;
		pRemoteList[i].channels      = pXSoundList[i].channels;
		pRemoteList[i].looped        = pXSoundList[i].looped;
		pRemoteList[i].dataSize      = pXSoundList[i].dataSize;
		pRemoteList[i].numSamples    = pXSoundList[i].numSamples;
		pRemoteList[i].streamed      = pXSoundList[i].streamed;
	}

	_snprintf( dbgCommand, sizeof( dbgCommand ), "SoundList() 0x%8.8x 0x%8.8x 0x%8.8x", nSounds, pRemoteList, &retVal );
	XBX_SendRemoteCommand( dbgCommand, false );

	delete [] pRemoteList;

	return ( retVal );
}

//-----------------------------------------------------------------------------
//	XBX_rMapInfo
//
//	Send signal to remote console with various info
//-----------------------------------------------------------------------------
int CXboxConsole::MapInfo( const xMapInfo_t *pXMapInfo )
{
	char		dbgCommand[XBX_MAX_RCMDLENGTH];
	int			retVal;
	xrMapInfo_t	xrMapInfo;

	memcpy( xrMapInfo.position, pXMapInfo->position, 3 * sizeof( float ) );
	memcpy( xrMapInfo.angle, pXMapInfo->angle, 3 * sizeof( float ) );

	strncpy( xrMapInfo.mapPath, pXMapInfo->mapPath, sizeof( xrMapInfo.mapPath ) );
	xrMapInfo.mapPath[sizeof( xrMapInfo.mapPath ) - 1] = '\0';

	strncpy( xrMapInfo.savePath, pXMapInfo->savePath, sizeof( xrMapInfo.savePath ) );
	xrMapInfo.savePath[sizeof( xrMapInfo.savePath ) - 1] = '\0';

	xrMapInfo.build = pXMapInfo->build;
	xrMapInfo.skill = pXMapInfo->skill;

	_snprintf( dbgCommand, sizeof( dbgCommand ), "MapInfo() 0x%8.8x 0x%8.8x", &xrMapInfo, &retVal );
	XBX_SendRemoteCommand( dbgCommand, false );

	return retVal;
}

//-----------------------------------------------------------------------------
//	XBX_rAddCommands
//
//	Expose commands to remote console
//-----------------------------------------------------------------------------
int CXboxConsole::AddCommands( int numCommands, const char* commands[], const char* help[] )
{
	char			dbgCommand[XBX_MAX_RCMDLENGTH];
	int				retVal;
	xrCommand_t*	cmdList;
	
	cmdList = new xrCommand_t[numCommands];
	memset( cmdList, 0, numCommands*sizeof( xrCommand_t ) );

	for ( int i=0; i<numCommands; i++ )
	{
		strncpy( cmdList[i].nameString, commands[i], sizeof( cmdList[i].nameString ) );
		cmdList[i].nameString[sizeof( cmdList[i].nameString ) - 1] = '\0';	

		strncpy( cmdList[i].helpString, help[i], sizeof( cmdList[i].helpString ) );
		cmdList[i].helpString[sizeof( cmdList[i].helpString ) - 1] = '\0';	
	}

	_snprintf( dbgCommand, sizeof( dbgCommand ), "AddCommands() 0x%8.8x 0x%8.8x 0x%8.8x", numCommands, cmdList, &retVal );
	XBX_SendRemoteCommand( dbgCommand, false );

	delete [] cmdList;

	return ( retVal );
}

