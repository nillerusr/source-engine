//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Implmentation of IEngineTool callback interface
//  Tool .dlls can call back through this interface to talk to the engine
//
//=============================================================================

#include "ienginetoolinternal.h"
#include "EngineSoundInternal.h"
#include "vengineserver_impl.h"
#include "cdll_engine_int.h"
#include "toolframework/ienginetool.h"
#include "client.h"
#include "server.h"
#include "con_nprint.h"
#include "toolframework/itoolframework.h"
#include "sound.h"
#include "screen.h"
#include "render.h"
#include "gl_matsysiface.h"
#include "cl_main.h"
#include "sys_dll.h"
#include "ivideomode.h"
#include "voice.h"
#include "filesystem_engine.h"
#include "enginetrace.h"
#include "Overlay.h"
#include "r_efx.h"
#include "r_local.h"
#include "lightcache.h"
#include "ispatialpartitioninternal.h"
#include "networkstringtableserver.h"
#include "networkstringtable.h"
#include "gl_rmain.h"
#include "vprof_telemetry.h"

#ifndef SWDS
#include "vgui_baseui_interface.h"
#endif

// External variables and APIs needed
extern CSysModule *g_GameDLL;
extern ConVar host_timescale;
extern	CGlobalVars g_ServerGlobalVariables;
void SV_ForceSend();
CreateInterfaceFn ClientDLL_GetFactory( void );
extern CNetworkStringTableContainer *networkStringTableContainerServer;

IOverlayMgr *OverlayMgr();

void SV_ForceSend();

extern ConVar host_framerate;

void VGui_SetGameDLLPanelsVisible( bool show );
float AudioSource_GetSoundDuration( char const *pName );

//-----------------------------------------------------------------------------
// Purpose: Singleton implementation of external tools callback interface
//-----------------------------------------------------------------------------
class CEngineTool : public IEngineToolInternal
{
public:
	CEngineTool();

	// Methods of IEngineToolFramework
	// Take over input
	virtual void		ShowCursor( bool show );
	virtual bool		IsCursorVisible() const;

	// Helpers for implementing a tool switching UI
	virtual int			GetToolCount() const;
	virtual const char	*GetToolName( int index ) const;
	virtual void		SwitchToTool( int index );

	virtual bool		IsTopmostTool( const IToolSystem *sys ) const;
 	virtual const IToolSystem *GetToolSystem( int index ) const;
	virtual IToolSystem *GetTopmostTool();

public:
	// Retrieve factories from server.dll and client.dll to get at specific interfaces defined within
	virtual void		GetServerFactory( CreateInterfaceFn& factory );
	virtual void		GetClientFactory( CreateInterfaceFn& factory );

	// Issue a console command
	virtual void		Command( const char *cmd );
	// Flush console command buffer right away
	virtual void		Execute();

	// If in a level, get name of current level
	virtual const char	*GetCurrentMap();
	virtual void		ChangeToMap( const char *mapname );
	virtual bool		IsMapValid( const char *mapname );

	// Method for causing engine to call client to render scene with no view model or overlays
	virtual void		RenderView( CViewSetup &view, int nFlags, int nWhatToRender );

	// Returns true if the player is fully connected and active in game (i.e, not still loading)
	virtual bool		IsInGame();
	// Returns true if the player is connected, but not necessarily active in game (could still be loading)
	virtual bool		IsConnected();

	virtual int			GetMaxClients(); // Tools might want to ensure single player, e.g.
	
	virtual bool		IsGamePaused();
	virtual void		SetGamePaused( bool paused );

	virtual float		GetTimescale(); // Could do this via ConVar system, too
	virtual void		SetTimescale( float scale );

	// Real time is unscaled, but is updated once per frame
	virtual float		GetRealTime();
	virtual float		GetRealFrameTime(); // unscaled

	virtual float		Time(); // Get high precision timer (for profiling?)

	// Host time is scaled
	virtual float		HostFrameTime(); // host_frametime
	virtual float		HostTime(); // host_time
	virtual int			HostTick(); // host_tickcount
	virtual int			HostFrameCount(); // total famecount

	virtual float		ServerTime(); // gpGlobals->curtime on server
	virtual float		ServerFrameTime(); // gpGlobals->frametime on server
	virtual int			ServerTick(); // gpGlobals->tickcount on server
	virtual float		ServerTickInterval(); // tick interval on server

	virtual float		ClientTime(); // gpGlobals->curtime on client
	virtual float		ClientFrameTime(); // gpGlobals->frametime on client
	virtual int			ClientTick(); // gpGlobals->tickcount on client

	virtual void		SetClientFrameTime( float frametime ); // gpGlobals->frametime on client

	// Currently the engine doesn't like to do networking when it's paused, but if a tool changes entity state, it can be useful to force 
	//  a network update to get that state over to the client
	virtual void		ForceUpdateDuringPause();

	// Maybe through modelcache???
	virtual model_t		*GetModel( HTOOLHANDLE hEntity );
	// Get the .mdl file used by entity (if it's a cbaseanimating)
	virtual studiohdr_t *GetStudioModel( HTOOLHANDLE hEntity );

	// SINGLE PLAYER/LISTEN SERVER ONLY (just matching the client .dll api for this)
	// Prints the formatted string to the notification area of the screen ( down the right hand edge
	//  numbered lines starting at position 0
	virtual void		Con_NPrintf( int pos, const char *fmt, ... );
	// SINGLE PLAYER/LISTEN SERVER ONLY(just matching the client .dll api for this)
	// Similar to Con_NPrintf, but allows specifying custom text color and duration information
	virtual void		Con_NXPrintf( const struct con_nprint_s *info, const char *fmt, ... );

	// Get the current game directory (hl2, tf2, hl1, cstrike, etc.)
	virtual void        GetGameDir( char *szGetGameDir, int maxlength );

// Do we need separate rects for the 3d "viewport" vs. the tools surface??? and can we control viewports from
	virtual void		GetScreenSize( int& width, int &height );

	virtual int			StartSound( 
		int iUserData,
		bool staticsound,
		int iEntIndex, 
		int iChannel, 
		const char *pSample, 
		float flVolume, 
		soundlevel_t iSoundlevel, 
		const Vector& origin,
		const Vector& direction,
		int iFlags = 0, 
		int iPitch = PITCH_NORM, 
		bool bUpdatePositions = true, 
		float delay = 0.0f, 
		int speakerentity = -1 );

	virtual void	StopSoundByGuid( int guid );
	virtual bool	IsSoundStillPlaying( int guid );
	virtual float	GetSoundDuration( int guid );
	virtual void	ReloadSound( const char *pSample );
	virtual void	StopAllSounds( );
	virtual void	SetAudioState( const AudioState_t &audioState );
	virtual void	SetMainView( const Vector &vecOrigin, const QAngle &angles );
	virtual bool	GetPlayerView( CViewSetup &playerView, int x, int y, int w, int h );
	virtual void	CreatePickingRay( const CViewSetup &viewSetup, int x, int y, Vector& org, Vector& forward );
	virtual bool	IsLoopingSound( int guid );

	virtual void	InstallQuitHandler( void *pvUserData, FnQuitHandler func );
	virtual void	TakeTGAScreenShot( const char *filename, int width, int height );
	// Even if game is paused, force networking to update to get new server state down to client
	virtual void	ForceSend();

	virtual bool	IsRecordingMovie();

	// NOTE: Params can contain file name, frame rate, output avi, output raw, and duration
	virtual void	StartMovieRecording( KeyValues *pMovieParams );
	virtual void	EndMovieRecording();
	virtual void	CancelMovieRecording();
	virtual IVideoRecorder *GetActiveVideoRecorder();

	virtual void	StartRecordingVoiceToFile( const char *filename, const char *pPathID = 0 );
	virtual void	StopRecordingVoiceToFile();
	virtual bool	IsVoiceRecording();

	virtual void	TraceRay( const Ray_t &ray, unsigned int fMask, ITraceFilter *pTraceFilter, CBaseTrace *pTrace );
	virtual void	TraceRayServer( const Ray_t &ray, unsigned int fMask, ITraceFilter *pTraceFilter, CBaseTrace *pTrace );

	bool			CanQuit();
	void			UpdateScreenshot();

	bool			ShouldSuppressDeInit() const;

	virtual bool		IsConsoleVisible();
	virtual int			GetPointContents( const Vector &vecPosition );
	virtual int			GetActiveDLights( dlight_t *pList[MAX_DLIGHTS] );
	virtual int			GetLightingConditions( const Vector &vecPosition, Vector *pColors, int nMaxLocalLights, LightDesc_t *pLocalLights );

	// precache methods
	virtual bool		PrecacheSound( const char *pName, bool bPreload = false );
	virtual bool		PrecacheModel( const char *pName, bool bPreload = false );

	virtual float		GetMono16Samples( const char *pszName, CUtlVector< short >& sampleList );

	virtual void		GetWorldToScreenMatrixForView( const CViewSetup &view, VMatrix *pVMatrix );
	virtual SpatialPartitionHandle_t CreatePartitionHandle( IHandleEntity *pEntity, SpatialPartitionListMask_t listMask, const Vector& mins, const Vector& maxs );
	virtual void DestroyPartitionHandle( SpatialPartitionHandle_t hPartition );
	virtual void InstallPartitionQueryCallback( IPartitionQueryCallback *pQuery );
	virtual void RemovePartitionQueryCallback( IPartitionQueryCallback *pQuery );
	virtual void ElementMoved( SpatialPartitionHandle_t handle, const Vector& mins, const Vector& maxs );

	virtual float		GetSoundDuration( const char *pszName );

public:
	// Methods of IEngineToolInternal
	virtual void	SetIsInGame( bool bIsInGame );

private:
	bool m_bIsInGame;

	struct QuitHandler_t
	{
		void			*userdata;
		FnQuitHandler	func;
	};

	CUtlVector< QuitHandler_t >	m_QuitHandlers;

	char				m_szScreenshotFile[ MAX_OSPATH ];
	int					m_nScreenshotWidth;
	int					m_nScreenshotHeight;

	bool				m_bRecordingMovie;
	bool				m_bSuppressDeInit;
	char				m_szVoiceoverFile[ MAX_OSPATH ];
};


//-----------------------------------------------------------------------------
// Singleton
//-----------------------------------------------------------------------------
static CEngineTool g_EngineTool;
IEngineToolInternal *g_pEngineToolInternal = &g_EngineTool;

void EngineTool_InstallQuitHandler( void *pvUserData, FnQuitHandler func )
{
	g_EngineTool.InstallQuitHandler( pvUserData, func );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool EngineTool_CheckQuitHandlers()
{
	return g_EngineTool.CanQuit();
}

void EngineTool_UpdateScreenshot()
{
	g_EngineTool.UpdateScreenshot();
}

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CEngineTool::CEngineTool()
{
	m_bIsInGame = false;
	m_szScreenshotFile[ 0 ] = 0;
	m_nScreenshotWidth = 180;
	m_nScreenshotHeight = 100;

	m_bRecordingMovie = false;
	m_bSuppressDeInit = false;
	m_szVoiceoverFile[ 0 ] = 0;
}


//-----------------------------------------------------------------------------
// Singleton
//-----------------------------------------------------------------------------
void CEngineTool::ShowCursor( bool show )
{
	Assert( 0 );
}

bool CEngineTool::IsCursorVisible() const
{
	Assert( 0 );
	return true;
}

int CEngineTool::GetPointContents( const Vector &vecPosition )
{
	return g_pEngineTraceClient->GetPointContents( vecPosition, NULL );
}

int CEngineTool::GetActiveDLights( dlight_t *pList[MAX_DLIGHTS] )
{
	return g_pEfx->CL_GetActiveDLights( pList );
}

bool WorldLightToMaterialLight( dworldlight_t* pWorldLight, LightDesc_t& light );

int CEngineTool::GetLightingConditions( const Vector &vecLightingOrigin, Vector *pColors, int nMaxLocalLights, LightDesc_t *pLocalLights )
{
	LightcacheGetDynamic_Stats stats;
	LightingState_t state;
	LightcacheGetDynamic( vecLightingOrigin, state, stats );
	Assert( state.numlights >= 0 && state.numlights < MAXLOCALLIGHTS );
	memcpy( pColors, state.r_boxcolor, sizeof(state.r_boxcolor) );

	int nLightCount = 0;
	for ( int i = 0; i < state.numlights; ++i )
	{
		LightDesc_t *pLightDesc = &pLocalLights[nLightCount];
		if (!WorldLightToMaterialLight( state.locallight[i], *pLightDesc ))
			continue;

		// Apply lightstyle
		float bias = LightStyleValue( state.locallight[i]->style );

		// Deal with overbrighting + bias
		pLightDesc->m_Color[0] *= bias;
		pLightDesc->m_Color[1] *= bias;
		pLightDesc->m_Color[2] *= bias;

		if ( ++nLightCount >= nMaxLocalLights )
			break;
	}
	return nLightCount;
}

void CEngineTool::GetServerFactory( CreateInterfaceFn& factory )
{
	factory = Sys_GetFactory( g_GameDLL );
}

void CEngineTool::GetClientFactory( CreateInterfaceFn& factory )
{
	factory = ClientDLL_GetFactory();
}

void CEngineTool::Command( const char *cmd )
{
	Cbuf_AddText( cmd );
}

void CEngineTool::Execute()
{
	Cbuf_Execute();
}

const char *CEngineTool::GetCurrentMap()
{
	if ( sv.IsDedicated() )
		return "Dedicated Server";

	if ( !cl.IsConnected() )
	{
		if ( sv.IsLoading() )
			return sv.GetMapName();

		return "";
	}

	return cl.m_szLevelFileName;
}

void CEngineTool::ChangeToMap( const char *mapname )
{
	if ( modelloader->Map_IsValid( mapname ) )
	{
		Cbuf_AddText( va( "map \"%s\"\n", mapname ) );
	}
}

bool CEngineTool::IsMapValid( const char *mapname )
{
	return modelloader->Map_IsValid( mapname );
}

// Allows tools to kick off rendering by having the engine call the client
void CEngineTool::RenderView( CViewSetup &view, int nFlags, int whatToRender )
{
	// Call client
	g_ClientDLL->RenderView( view, nFlags, whatToRender );
}

void CEngineTool::SetIsInGame( bool bIsInGame )
{
	m_bIsInGame = bIsInGame;
}

bool CEngineTool::IsInGame()
{
	return m_bIsInGame && cl.IsConnected();
}

bool CEngineTool::IsConnected()
{
	return cl.IsConnected();
}

int	 CEngineTool::GetMaxClients()
{
	return cl.m_nMaxClients;
}

bool CEngineTool::IsGamePaused()
{
	return cl.IsPaused();
}

bool CEngineTool::IsConsoleVisible()
{
#ifdef SWDS
	return false;
#else
	return EngineVGui()->IsConsoleVisible();
#endif
}

void CEngineTool::SetGamePaused( bool paused )
{
	sv.SetPaused( paused );
}

float CEngineTool::GetTimescale()
{
	return host_timescale.GetFloat();
}

void CEngineTool::SetTimescale( float scale )
{
	host_timescale.SetValue( scale );
}

float CEngineTool::Time()
{
	return Plat_FloatTime();
}

// Real time is unscaled, but is updated once per frame
float CEngineTool::GetRealTime()
{
	return realtime;
}

float CEngineTool::GetRealFrameTime()
{
	return host_frametime;
}

float CEngineTool::HostFrameTime()
{
	return host_frametime;
}

float CEngineTool::HostTime()
{
	return host_time;
}

int CEngineTool::HostTick()
{
	return host_tickcount;
}

int CEngineTool::HostFrameCount()
{
	return host_framecount;
}

float CEngineTool::ServerTime()
{
	return g_ServerGlobalVariables.curtime;
}

float CEngineTool::ServerFrameTime()
{
	return g_ServerGlobalVariables.frametime;
}
int CEngineTool::ServerTick()
{
	return g_ServerGlobalVariables.tickcount;
}

float CEngineTool::ServerTickInterval()
{
	return g_ServerGlobalVariables.interval_per_tick;
}

float CEngineTool::ClientTime()
{
	return g_ClientGlobalVariables.curtime;
}

float CEngineTool::ClientFrameTime()
{
	return g_ClientGlobalVariables.frametime;
}

int CEngineTool::ClientTick()
{
	return g_ClientGlobalVariables.tickcount;
}

void CEngineTool::SetClientFrameTime( float frametime )
{
	g_ClientGlobalVariables.frametime = frametime;
}

void CEngineTool::ForceUpdateDuringPause()
{
	SV_ForceSend();
}

model_t *CEngineTool::GetModel( HTOOLHANDLE hEntity )
{
	Assert( 0 );
	return NULL;
}

studiohdr_t *CEngineTool::GetStudioModel( HTOOLHANDLE hEntity )
{
	Assert( 0 );
	return NULL;
}

void CEngineTool::Con_NPrintf( int pos, const char *fmt, ... )
{
	char buf[ 1024 ];
	va_list argptr;
	va_start( argptr, fmt );
	_vsnprintf( buf, sizeof( buf ) - 1, fmt, argptr );
	va_end( argptr );

	return ::Con_NPrintf( pos, "%s", buf );
}

void CEngineTool::Con_NXPrintf( const struct con_nprint_s *info, const char *fmt, ... )
{
	char buf[ 1024 ];
	va_list argptr;
	va_start( argptr, fmt );
	_vsnprintf( buf, sizeof( buf ) - 1, fmt, argptr );
	va_end( argptr );

	::Con_NXPrintf( info, "%s", buf );
}

void CEngineTool::GetGameDir( char *szGetGameDir, int maxlength )
{
	Q_strncpy( szGetGameDir, com_gamedir, maxlength );
}

void CEngineTool::GetScreenSize( int& width, int &height )
{
	CMatRenderContextPtr pRenderContext( materials );

	pRenderContext->GetWindowSize( width, height );
}

//-----------------------------------------------------------------------------
// Purpose: Helpers for implementing a tool switching UI
// Input  :  - 
// Output : int
//-----------------------------------------------------------------------------
int CEngineTool::GetToolCount() const
{
	return toolframework->GetToolCount();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : index - 
// Output : char
//-----------------------------------------------------------------------------
const char	*CEngineTool::GetToolName( int index ) const
{
	return toolframework->GetToolName( index );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : index - 
//-----------------------------------------------------------------------------
void CEngineTool::SwitchToTool( int index )
{
	toolframework->SwitchToTool( index );
}

bool CEngineTool::IsTopmostTool( const IToolSystem *sys ) const
{
	return toolframework->IsTopmostTool( sys );
}

IToolSystem *CEngineTool::GetTopmostTool()
{
	return toolframework->GetTopmostTool();
}

const IToolSystem *CEngineTool::GetToolSystem( int index ) const
{
	return toolframework->GetToolSystem( index );
}

int CEngineTool::StartSound( 
	int iUserData,
	bool staticsound,
	int iEntIndex, 
	int iChannel, 
	const char *pSample, 
	float flVolume, 
	soundlevel_t iSoundlevel, 
	const Vector& origin,
	const Vector& direction,
	int iFlags /*= 0*/, 
	int iPitch /*= PITCH_NORM*/, 
	bool bUpdatePositions /*= true*/, 
	float delay /*= 0.0f*/, 
	int speakerentity /*= -1*/ )
{
	StartSoundParams_t params;
	params.userdata = iUserData;
	params.staticsound = staticsound;
	params.soundsource = iEntIndex;
	params.entchannel = iChannel;
	params.pSfx = S_PrecacheSound( pSample );
	params.origin = origin; 
	params.direction = direction; 
	params.bUpdatePositions = bUpdatePositions;
	params.fvol = flVolume;
	params.soundlevel = iSoundlevel;
	params.flags = iFlags;
	params.pitch = iPitch; 
	params.fromserver = false;
	params.delay = delay;
	params.speakerentity = speakerentity;
	params.suppressrecording = true;

	int guid = S_StartSound( params );

	return guid;
}

void CEngineTool::StopSoundByGuid( int guid )
{
	S_StopSoundByGuid( guid );
}

bool CEngineTool::IsSoundStillPlaying( int guid )
{
	return S_IsSoundStillPlaying( guid );
}

float CEngineTool::GetSoundDuration( int guid )
{
	return S_SoundDurationByGuid( guid );
}

float CEngineTool::GetSoundDuration( const char *pszName )
{
	return AudioSource_GetSoundDuration( pszName );
}

void CEngineTool::ReloadSound( const char *pSample )
{
	S_ReloadSound( pSample );
}

void CEngineTool::StopAllSounds( )
{
	S_StopAllSounds( true );
}

// Returns if the sound is looping
bool CEngineTool::IsLoopingSound( int guid )
{
	return S_IsLoopingSoundByGuid( guid );
}

void CEngineTool::TraceRay( const Ray_t &ray, unsigned int fMask, ITraceFilter *pTraceFilter, CBaseTrace *pTrace )
{
	trace_t tempTrace;

	g_pEngineTraceClient->TraceRay( ray, fMask, pTraceFilter, &tempTrace );

	memcpy( pTrace, &tempTrace, sizeof ( CBaseTrace ) );
}


void CEngineTool::TraceRayServer( const Ray_t &ray, unsigned int fMask, ITraceFilter *pTraceFilter, CBaseTrace *pTrace )
{
	trace_t tempTrace;

	g_pEngineTraceServer->TraceRay( ray, fMask, pTraceFilter, &tempTrace );

	memcpy( pTrace, &tempTrace, sizeof ( CBaseTrace ) );
}


void CEngineTool::SetAudioState( const AudioState_t &audioState )
{
	Host_SetAudioState( audioState );
}


// Sets the location of the main view
void CEngineTool::SetMainView( const Vector &vecOrigin, const QAngle &angles )
{
	g_EngineRenderer->SetMainView( vecOrigin, angles );
}

static float ScaleFOVByWidthRatio( float fovDegrees, float ratio )
{
	float halfAngleRadians = fovDegrees * ( 0.5f * M_PI / 180.0f );
	float t = tan( halfAngleRadians );
	t *= ratio;
	float retDegrees = ( 180.0f / M_PI ) * atan( t );
	return retDegrees * 2.0f;
}

// Gets the player view
bool CEngineTool::GetPlayerView( CViewSetup &viewSetup, int x, int y, int w, int h )
{
	if ( g_ClientDLL )
	{
		if ( !g_ClientDLL->GetPlayerView( viewSetup ) )
			return false;

		// Initialize view setup given the desired rectangle
		viewSetup.x = x;
		viewSetup.y = y;
		viewSetup.width = w;
		viewSetup.height = h;
		viewSetup.m_flAspectRatio = (viewSetup.height != 0) ? (float)viewSetup.width / (float)viewSetup.height : 4.0f / 3.0f;
		viewSetup.m_bRenderToSubrectOfLargerScreen = true;
		viewSetup.fov = ScaleFOVByWidthRatio( viewSetup.fov, viewSetup.m_flAspectRatio / ( 4.0f / 3.0f ) );
		viewSetup.fovViewmodel = ScaleFOVByWidthRatio( viewSetup.fovViewmodel, viewSetup.m_flAspectRatio / ( 4.0f / 3.0f ) );
		return true;
	}
	return false;
}


//-----------------------------------------------------------------------------
// From a location on the screen, figure out the vector into the world
//-----------------------------------------------------------------------------

void CEngineTool::CreatePickingRay( const CViewSetup &viewSetup, int x, int y, Vector& org, Vector& forward )
{
	// Remap x and y into -1 to 1 normalized space
	float xf, yf;
	xf = ( 2.0f * (float)x / (float)viewSetup.width ) - 1.0f;
	yf = ( 2.0f * (float)y / (float)viewSetup.height ) - 1.0f;

	// Flip y axis
	yf = -yf;

	VMatrix worldToScreen;
	GetWorldToScreenMatrixForView( viewSetup, &worldToScreen );
	VMatrix screenToWorld;
	MatrixInverseGeneral( worldToScreen, screenToWorld );

	// Create two points at the normalized mouse x, y pos and at the near and far z planes (0 and 1 depth)
	Vector v1, v2;
	v1.Init( xf, yf, 0.0f );
	v2.Init( xf, yf, 1.0f );
    
	Vector o2;
	// Transform the two points by the screen to world matrix
	screenToWorld.V3Mul( v1, org ); // ray start origin
	screenToWorld.V3Mul( v2, o2 );  // ray end origin
	VectorSubtract( o2, org, forward );
	forward.NormalizeInPlace();
}


//-----------------------------------------------------------------------------
// Purpose: Returns true if all handlers say we can quit the engine
//-----------------------------------------------------------------------------
bool CEngineTool::CanQuit()
{
	int c = m_QuitHandlers.Count();
	for ( int i = 0; i < c; ++i )
	{
		QuitHandler_t& qh = m_QuitHandlers[ i ];
		FnQuitHandler func = qh.func;
		if ( func )
		{
			if ( !func( qh.userdata ) )
			{
				return false;
			}
		}
	}

	return true;
}

void CEngineTool::InstallQuitHandler( void *pvUserData, FnQuitHandler func )
{
	QuitHandler_t qh;
	qh.userdata = pvUserData;
	qh.func = func;

	m_QuitHandlers.AddToTail( qh );
}

// precache methods
bool CEngineTool::PrecacheSound( const char *pName, bool bPreload )
{
	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s(%s, %s)", __FUNCTION__, tmDynamicString( TELEMETRY_LEVEL0, pName ), bPreload ? "true" : "false" );

	if ( pName && TestSoundChar( pName, CHAR_SENTENCE ) )
		return true;

	bool bState = networkStringTableContainerServer->Lock( false );
	int flags = bPreload ? RES_PRELOAD : 0;
	int i = sv.PrecacheSound( pName, flags );
	networkStringTableContainerServer->Lock( bState );
	return i >= 0;
}

bool CEngineTool::PrecacheModel( const char *pName, bool bPreload )
{
	int flags = bPreload ? RES_PRELOAD : 0;
	bool bState = networkStringTableContainerServer->Lock( false );
	int i = sv.PrecacheModel( pName, flags );
	networkStringTableContainerServer->Lock( bState );
	return i >= 0;
}

void CEngineTool::TakeTGAScreenShot( const char *filename, int width, int height )
{
	Q_strncpy( m_szScreenshotFile, filename, sizeof( m_szScreenshotFile ) );

	m_nScreenshotWidth = width;
	m_nScreenshotHeight = height;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEngineTool::UpdateScreenshot()
{
	if ( g_LostVideoMemory )
		return;

	if ( m_szScreenshotFile[0] )
	{
		g_ClientDLL->WriteSaveGameScreenshotOfSize( m_szScreenshotFile, m_nScreenshotWidth, m_nScreenshotHeight );
		m_szScreenshotFile[0] = 0;
	}
}

// Even if game is paused, force networking to update to get new server state down to client
void CEngineTool::ForceSend()
{
	SV_ForceSend();
}

bool CEngineTool::IsRecordingMovie()
{
	if ( m_bRecordingMovie )
	{
		Assert( CL_IsRecordingMovie() );
		return true;
	}
	return false;
}

// NOTE: Params can contain file name, frame rate, output avi, output raw, and duration
void CEngineTool::StartMovieRecording( KeyValues *pMovieParams )
{
	if ( CL_IsRecordingMovie() )
	{
		Warning( "Can't record movie, already recording!!!\n" );
		return;
	}

	if ( m_bRecordingMovie )
		return;

	int jpeg_quality = DEFAULT_JPEG_QUALITY;

	int flags = 0;
	VideoSystem_t videoSystem = VideoSystem::NONE;
	if ( pMovieParams->GetInt( "outputavi", 0 ) )
	{
		Warning( "Got a request to record a movie using AVI, but AVI is deprecated. Using QuickTime/H264 instead.\n" );
		videoSystem = VideoSystem::QUICKTIME;
		if ( pMovieParams->GetInt( "avisoundonly", 0 ) )
		{
			flags |= MovieInfo_t::FMOVIE_VIDSOUND;
		}
		else
		{
			flags |= MovieInfo_t::FMOVIE_VID | MovieInfo_t::FMOVIE_VIDSOUND;
		}
	}
	if  ( pMovieParams->GetInt( "outputtga", 0 ) )
	{
		flags |= MovieInfo_t::FMOVIE_TGA;
	}
	if  ( pMovieParams->GetInt( "outputjpg", 0 ) )
	{
		flags |= MovieInfo_t::FMOVIE_JPG;
		jpeg_quality = pMovieParams->GetInt( "jpeg_quality" );
	}
	if  ( pMovieParams->GetInt( "outputwav", 0 ) )
	{
		flags |= MovieInfo_t::FMOVIE_WAV;
	}

	const char *pFileName = pMovieParams->GetString( "filename", NULL );
	if ( !pFileName )
	{
		Warning( "Output filename not specified!\n" );
		return;
	}

	int nWidth = pMovieParams->GetInt( "width", videomode->GetModeStereoWidth() );
	int nHeight = pMovieParams->GetInt( "height", videomode->GetModeStereoHeight() );
	float flFrameRate = pMovieParams->GetFloat( "framerate", 30.0f );

	m_bRecordingMovie = true;
	CL_StartMovie( pFileName, flags, nWidth, nHeight, flFrameRate, jpeg_quality, videoSystem );
}

void CEngineTool::EndMovieRecording()
{
	if ( !m_bRecordingMovie )
		return;

	CL_EndMovie();
	m_bRecordingMovie = false;
}

void CEngineTool::CancelMovieRecording()
{
	EndMovieRecording();
}


IVideoRecorder *CEngineTool::GetActiveVideoRecorder()
{
 	if ( !CL_IsRecordingMovie() )
 	{
 		return NULL;
 	}
 	
 	return g_pVideoRecorder;
}

void Voice_ForceInit();

bool CEngineTool::ShouldSuppressDeInit() const
{
	return m_bSuppressDeInit;
}

void CEngineTool::StartRecordingVoiceToFile( const char *filename, const char *pPathID /*= 0*/ )
{	
	FileHandle_t fh = g_pFileSystem->Open( filename, "wb", pPathID );
	if ( fh != FILESYSTEM_INVALID_HANDLE )
	{
		byte foo = 'b';

		g_pFileSystem->Write( &foo, 1, fh );
		g_pFileSystem->Close( fh );
	}

	g_pFileSystem->RelativePathToFullPath( filename, pPathID, m_szVoiceoverFile, sizeof( m_szVoiceoverFile ) );

	g_pFileSystem->RemoveFile( filename, pPathID );

#if !defined( NO_VOICE )
	if ( IsVoiceRecording() )
	{
		Voice_RecordStop();
	}
	m_bSuppressDeInit = true;

	Voice_ForceInit();
	Voice_RecordStart( m_szVoiceoverFile, NULL, NULL);
#endif
}

void CEngineTool::StopRecordingVoiceToFile()
{
#if !defined( NO_VOICE )
	Voice_RecordStop();
	m_bSuppressDeInit = false;
#endif
}

float CEngineTool::GetMono16Samples( const char *pszName, CUtlVector< short >& sampleList )
{
	return S_GetMono16Samples( pszName, sampleList );
}

void CEngineTool::GetWorldToScreenMatrixForView( const CViewSetup &view, VMatrix *pVMatrix )
{
	VMatrix worldToView, viewToProjection;
	ComputeViewMatrices( &worldToView, &viewToProjection, pVMatrix, view );
}

SpatialPartitionHandle_t CEngineTool::CreatePartitionHandle( IHandleEntity *pEntity,
	SpatialPartitionListMask_t listMask, const Vector& mins, const Vector& maxs )
{
	return SpatialPartition()->CreateHandle( pEntity, listMask, mins, maxs );
}

void CEngineTool::DestroyPartitionHandle( SpatialPartitionHandle_t hPartition )
{
	SpatialPartition()->DestroyHandle( hPartition );
}

void CEngineTool::InstallPartitionQueryCallback( IPartitionQueryCallback *pQuery )
{
	SpatialPartition()->InstallQueryCallback( pQuery );
}

void CEngineTool::RemovePartitionQueryCallback( IPartitionQueryCallback *pQuery )
{
	SpatialPartition()->RemoveQueryCallback( pQuery );
}

void CEngineTool::ElementMoved( SpatialPartitionHandle_t handle, const Vector& mins, const Vector& maxs )
{
	SpatialPartition()->ElementMoved( handle, mins, maxs );
}

bool CEngineTool::IsVoiceRecording()
{
#if !defined( NO_VOICE )
	return Voice_IsRecording();
#else
	return false;
#endif
}

bool EngineTool_SuppressDeInit()
{
	return g_EngineTool.ShouldSuppressDeInit();
}

void EngineTool_OverrideSampleRate( int& rate )
{
	if ( EngineTool_SuppressDeInit() )
	{
		rate = 11025;
	}
}

// Expose complex interface
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CEngineTool, IEngineTool, VENGINETOOL_INTERFACE_VERSION, g_EngineTool );
// Expose simple interface
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CEngineTool, IEngineToolFramework, VENGINETOOLFRAMEWORK_INTERFACE_VERSION, g_EngineTool );
