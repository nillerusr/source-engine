//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//
#if !defined( HOST_H )
#define HOST_H
#ifdef _WIN32
#pragma once
#endif

#include "convar.h"
#include "steam/steamclientpublic.h"

#define SCRIPT_DIR			"scripts/"

struct model_t;
struct AudioState_t;


class CCommonHostState
{
public:
	model_t		*worldmodel;	// cl_entitites[0].model
	struct worldbrushdata_t *worldbrush;
	float		interval_per_tick;		// Tick interval for game
	void SetWorldModel( model_t *pModel );
};

extern CCommonHostState host_state;

//=============================================================================
// the host system specifies the base of the directory tree, the mod + base mod
// and the amount of memory available for the program to use
struct engineparms_t
{
	char	*basedir;	// Executable directory ("c:/program files/half-life 2", for example)
	char	*mod;		// Mod name ("cstrike", for example)
	char	*game;		// Root game name ("hl2", for example, in the case of cstrike)
	unsigned int	memsize;
};
extern engineparms_t host_parms;


//-----------------------------------------------------------------------------
// Human readable methods to get at engineparms info
//-----------------------------------------------------------------------------
inline const char *GetCurrentMod()
{
	return host_parms.mod;
}

inline const char *GetCurrentGame()
{
	return host_parms.game;
}

inline const char *GetBaseDirectory()
{
	return host_parms.basedir;
}


//=============================================================================

//
// host
// FIXME, move all this crap somewhere else
//

extern	ConVar		developer;
extern	bool		host_initialized;		// true if into command execution
extern	float		host_frametime;
extern  float		host_frametime_unbounded;
extern  float		host_frametime_stddeviation;
extern	int			host_framecount;	// incremented every frame, never reset
extern	double		realtime;			// not bounded in any way, changed at
// start of every frame, never reset
void Host_Error (PRINTF_FORMAT_STRING const char *error, ...) FMTFUNCTION( 1, 2 );
void Host_EndGame (bool bShowMainMenu, PRINTF_FORMAT_STRING const char *message, ...) FMTFUNCTION( 2, 3 );

// user message
#define MAX_USER_MSG_DATA 255

// build info
// day counter from Sep 30 2003
extern int build_number( void );


// Choke local client's/server's packets?
extern  ConVar		host_limitlocal;      
// Print a debug message when the client or server cache is missed
extern	ConVar		host_showcachemiss;

extern bool			g_bInEditMode;
extern bool			g_bInCommentaryMode;
extern bool			g_bAllowSecureServers;
extern bool			g_bLowViolence;

// Returns true if host is not single stepping/pausing through code/
// FIXME:  Remove from final, retail version of code.
bool Host_ShouldRun( void );
void Host_FreeToLowMark( bool server );
void Host_FreeStateAndWorld( bool server );
void Host_Disconnect( bool bShowMainMenu, const char *pszReason = "" );
void Host_RunFrame( float time );
void Host_DumpMemoryStats( void );
void Host_UpdateMapList( void );
float Host_GetSoundDuration( const char *pSample );
bool Host_IsSinglePlayerGame( void );
int Host_GetServerCount( void );
bool Host_AllowQueuedMaterialSystem( bool bAllow );

bool Host_IsSecureServerAllowed();
void FORCEINLINE Host_DisallowSecureServers()
{
#if !defined(SWDS)
	g_bAllowSecureServers = false;
#endif
}

bool Host_AllowLoadModule( const char *pFilename, const char *pPathID, bool bAllowUnknown, bool bIsServerOnly = false );

// Force the voice stuff to send a packet out.
// bFinal is true when the user is done talking.
void CL_SendVoicePacket(bool bFinal);

// Accumulated filtered time on machine ( i.e., host_framerate can alter host_time )
extern float host_time;

class NET_SetConVar;

void		Host_BuildConVarUpdateMessage( NET_SetConVar *cvarMsg, int flags, bool nonDefault );
char const *Host_CleanupConVarStringValue( char const *invalue );
void		Host_SetAudioState( const AudioState_t &audioState );
void		Host_DefaultMapFileName( const char *pFullMapName, /* out */ char *pDiskName, unsigned int nDiskNameSize );

bool CheckVarRange_Generic( ConVar *pVar, int minVal, int maxVal );

// Total ticks run
extern int	host_tickcount;
// Number of ticks being run this frame
extern int	host_frameticks;
// Which tick are we currently on for this frame
extern int	host_currentframetick;

// PERFORMANCE INFO
#define MIN_FPS         0.1         // Host minimum fps value for maxfps.
#define MAX_FPS         1000.0        // Upper limit for maxfps.

#define MAX_FRAMETIME	0.1
#define MIN_FRAMETIME	0.001

#define TIME_TO_TICKS( dt )		( (int)( 0.5f + (float)(dt) / host_state.interval_per_tick ) )
#define TICKS_TO_TIME( dt )		( host_state.interval_per_tick * (float)(dt) )

// Normally, this is off, and it keeps the VCR file size smaller, but it can help
// to turn it on when tracking down out-of-sync errors, because it verifies that more
// things are the same during playback.
extern ConVar vcr_verbose;

// Set by the game DLL to tell us to do the same timing tricks as timedemo.
extern bool g_bDedicatedServerBenchmarkMode;

extern uint GetSteamAppID();
extern EUniverse GetSteamUniverse();

#define STEAMREMOTESTORAGE_CLOUD_OFF	0
#define STEAMREMOTESTORAGE_CLOUD_ON		1

#endif // HOST_H

