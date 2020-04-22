//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#ifndef CL_MAIN_H
#define CL_MAIN_H
#ifdef _WIN32
#pragma once
#endif


#include "basetypes.h"
#include "networkstringtable.h"
#include "const.h"
#include "utlvector.h"
#include "checksum_crc.h"
#include "cdll_int.h"
#include "netmessages.h"
#include "dlight.h"
#include "iefx.h"
#include "tier1/convar.h"
#include "cmodel_engine.h"
#include "video/ivideoservices.h"

class CCommand;

#define	MAX_STYLESTRING	64
#define	MAX_ELIGHTS		64		// entity only point lights

extern	dlight_t		cl_dlights[MAX_DLIGHTS];
extern	dlight_t		cl_elights[MAX_ELIGHTS];
extern bool g_bActiveDlights;
extern bool g_bActiveElights;

// These can be used for fast access to the leaf indices of each light.
extern CFastPointLeafNum g_DLightLeafAccessors[MAX_DLIGHTS];
extern CFastPointLeafNum g_ELightLeafAccessors[MAX_ELIGHTS];

class CBaseClientState;
class CEntityReadInfo;
class CPureServerWhitelist;
struct SoundInfo_t;

#define DEFAULT_JPEG_QUALITY 50

void CL_TakeJpeg( const char *name = NULL, int quality = DEFAULT_JPEG_QUALITY );

struct MovieInfo_t
{
	enum
	{
		FMOVIE_TGA	= ( 1 << 0 ),
		FMOVIE_VID	= ( 1 << 1 ),
		FMOVIE_WAV	= ( 1 << 2 ),
		FMOVIE_VIDSOUND = ( 1 << 3 ),
		FMOVIE_JPG = ( 1<< 4 )
	};

	MovieInfo_t()
	{
		moviename[ 0 ] = 0;
		movieframe = 0;
		type = FMOVIE_TGA | FMOVIE_WAV;
		jpeg_quality = DEFAULT_JPEG_QUALITY;
	}

	void Reset()
	{
		moviename[ 0 ] = 0;
		movieframe = 0;
		type = FMOVIE_TGA | FMOVIE_WAV;
		jpeg_quality = DEFAULT_JPEG_QUALITY;
	}

	bool	IsRecording() const
	{
		return moviename[ 0 ] != 0 ? true : false;
	}

	bool	DoWav() const
	{
		return ( type & FMOVIE_WAV ) ? true : false;
	}

	bool	DoTga() const
	{
		return ( type & FMOVIE_TGA ) ? true : false;
	}

	bool    DoJpg() const
	{
		return ( type & FMOVIE_JPG ) ? true : false;
	}


	bool	DoVideo() const
	{
		return ( type & FMOVIE_VID ) ? true : false;
	}

	bool	DoVideoSound() const
	{
		return ( type & FMOVIE_VIDSOUND ) ? true : false;
	}

	char	moviename[ MAX_PATH ];
	int		movieframe;

	int		type;
	int		jpeg_quality;
};

extern MovieInfo_t cl_movieinfo;

//=============================================================================

// cl_main.cpp
//

void CL_StartMovie( const char *filename, int flags, int nWidth, int nHeight, float flFrameRate, int jpeg_quality, VideoSystem_t videoSystem = VideoSystem::NONE );
bool CL_IsRecordingMovie();
void CL_EndMovie();

void CL_DecayLights (void);
void CL_AddSound( const SoundInfo_t &sound );
void CL_DispatchSounds( void );
void CL_DispatchSound( const SoundInfo_t &sound );
void CL_SetServerTick( int tick );

void CL_Init (void);
void CL_Shutdown( void );

void CL_FireEvents( void );
void CL_NextDemo (void);
void CL_TakeScreenshot(const char *name);

const struct CPrecacheUserData* CL_GetPrecacheUserData( INetworkStringTable *table, int index );

void Callback_ModelChanged( void *object, INetworkStringTable *stringTable, int stringNumber, const char *newString, const void *newData );
void Callback_GenericChanged( void *object, INetworkStringTable *stringTable, int stringNumber, const char *newString, const void *newData );
void Callback_SoundChanged( void *object, INetworkStringTable *stringTable, int stringNumber, const char *newString, const void *newData );
void Callback_DecalChanged( void *object, INetworkStringTable *stringTable, int stringNumber, const char *newString, const void *newData );
void Callback_InstanceBaselineChanged( void *object, INetworkStringTable *stringTable, int stringNumber, const char *newString, const void *newData );
void Callback_UserInfoChanged( void *object, INetworkStringTable *stringTable, int stringNumber, const char *newString, const void *newData );
void Callback_DynamicModelsChanged( void *object, INetworkStringTable *stringTable, int stringNumber, const char *newString, const void *newData );

void CL_InstallAndInvokeClientStringTableCallbacks();
void CL_HookClientStringTables();
void CL_LatchInterpolationAmount();

// Resource
void CL_RegisterResources ( void );

//
// cl_input
//
void CL_Move( float accumulated_extra_samples, bool bFinalTick );
void CL_ExtraMouseUpdate( float remainder );

void CL_ClearState (void);
void CL_ReadPackets ( bool framefinished );        // Read packets from server and other sources (ping requests, etc.)

//
// cl_main.cpp
//
void CL_FullyConnected( void );
void CL_Retry( void );
void CL_HudMessage( const char *pMessage );
void CL_CheckClientState( void );
void CL_TakeSnapshotAndSwap();
void CL_ReallocateDynamicData( int maxclients );
void CL_SetupMapName( const char* pName, char* pFixedName, int maxlen );
bool CL_CheckCRCs( const char *pszMap );
bool CL_ShouldLoadBackgroundLevel( const CCommand &args );

bool CL_IsHL2Demo();
bool CL_IsPortalDemo();
void CL_SetSteamCrashComment();

void CL_CheckForPureServerWhitelist( /* out */ IFileList *&pFilesToReload );
void CL_ReloadFilesInList( IFileList *pFilesToReload );
void CL_HandlePureServerWhitelist( CPureServerWhitelist *pWhitelist, /* out */ IFileList *&pFilesToReload );

// Special mode where the client uses a console window and has no graphics. Useful for stress-testing a server
// without having to round up 32 people.
extern bool g_bTextMode;

extern bool cl_takesnapshot;
extern ConVar cl_language;


#endif // CL_MAIN_H
