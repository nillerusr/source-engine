//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#include "quakedef.h"
#include <assert.h>
#include "engine_launcher_api.h"
#include "iengine.h"
#include "ivideomode.h"
#include "igame.h"
#include "vmodes.h"
#include "modes.h"
#include "sys.h"
#include "host.h"
#include "keys.h"
#include "cdll_int.h"
#include "host_state.h"
#include "cdll_engine_int.h"
#include "sys_dll.h"
#include "tier0/vprof.h"
#include "profile.h"
#include "gl_matsysiface.h"
#include "vprof_engine.h"
#include "server.h"
#include "cl_demo.h"
#include "toolframework/itoolframework.h"
#include "toolframework/itoolsystem.h"
#include "inputsystem/iinputsystem.h"
#include "gl_cvars.h"
#include "filesystem_engine.h"
#include "tier0/cpumonitoring.h"
#ifndef SWDS
#include "vgui_baseui_interface.h"
#endif
#include "tier0/etwprof.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
void Sys_ShutdownGame( void );
int Sys_InitGame( CreateInterfaceFn appSystemFactory, 
			char const* pBaseDir, void *pwnd, int bIsDedicated );

// Sleep time when not focus. Set to 0 to not sleep even if app doesn't have focus.
ConVar engine_no_focus_sleep( "engine_no_focus_sleep", "50", FCVAR_ARCHIVE );

// sleep time when not focus
#define NOT_FOCUS_SLEEP	50				

#define DEFAULT_FPS_MAX	300
#define DEFAULT_FPS_MAX_S "300"
static int s_nDesiredFPSMax = DEFAULT_FPS_MAX;
static bool s_bFPSMaxDrivenByPowerSavings = false;

//-----------------------------------------------------------------------------
// ConVars and ConCommands
//-----------------------------------------------------------------------------
static void fps_max_callback( IConVar *var, const char *pOldValue, float flOldValue )
{
	// Only update s_nDesiredFPSMax when not driven by the mat_powersavingsmode ConVar (see below)
	if ( !s_bFPSMaxDrivenByPowerSavings )
	{
		s_nDesiredFPSMax = ( (ConVar *)var)->GetInt();
	}
}
ConVar fps_max( "fps_max", DEFAULT_FPS_MAX_S, FCVAR_NOT_CONNECTED, "Frame rate limiter, cannot be set while connected to a server.", fps_max_callback );

// When set, this ConVar (typically driven from the advanced video settings) will drive fps_max (see above) to
// half of the refresh rate, if the user hasn't otherwise set fps_max (via console, commandline etc)
static void mat_powersavingsmode_callback( IConVar *var, const char *pOldValue, float flOldValue )
{
	s_bFPSMaxDrivenByPowerSavings = true;
	int nRefresh = s_nDesiredFPSMax;

	if ( ( (ConVar *)var)->GetBool() )
	{
		MaterialVideoMode_t mode;
		materials->GetDisplayMode( mode );
		nRefresh = MAX( 30, ( mode.m_RefreshRate + 1 ) >> 1 ); // Half of display refresh rate (min of 30Hz)
	}

	fps_max.SetValue( nRefresh );
	s_bFPSMaxDrivenByPowerSavings = false;
}
static ConVar mat_powersavingsmode( "mat_powersavingsmode", "0", FCVAR_ARCHIVE, "Power Savings Mode", mat_powersavingsmode_callback );

#ifndef _RETAIL
static ConVar async_serialize( "async_serialize", "0", 0, "Force async reads to serialize for profiling" );
#define ShouldSerializeAsync() async_serialize.GetBool()
#else
#define ShouldSerializeAsync() false
#endif

extern ConVar host_timer_spin_ms;
extern float host_nexttick;
extern IVEngineClient *engineClient;

#ifdef WIN32
static void cpu_frequency_monitoring_callback( IConVar *var, const char *pOldValue, float flOldValue )
{
	// Set the specified interval for CPU frequency monitoring
	SetCPUMonitoringInterval( (unsigned)( ( (ConVar *)var)->GetFloat() * 1000 ) );
}
ConVar cpu_frequency_monitoring( "cpu_frequency_monitoring", "0", 0, "Set CPU frequency monitoring interval in seconds. Zero means disabled.", true, 0.0f, true, 10.0f, cpu_frequency_monitoring_callback );
#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CEngine : public IEngine
{
public:
					CEngine( void );
	virtual			~CEngine( void );

	bool			Load( bool dedicated, const char *basedir );

	virtual void	Unload( void );
	virtual EngineState_t GetState( void );
	virtual void	SetNextState( EngineState_t iNextState );

	void			Frame( void );

	float			GetFrameTime( void );
	float			GetCurTime( void );
	
	bool			TrapKey_Event( ButtonCode_t key, bool down );
	void			TrapMouse_Event( int buttons, bool down );

	void			StartTrapMode( void );
	bool			IsTrapping( void );
	bool			CheckDoneTrapping( ButtonCode_t& key );

	int				GetQuitting( void );
	void			SetQuitting( int quittype );

private:
	bool			FilterTime( float t );

	int				m_nQuitting;

	EngineState_t	m_nDLLState;
	EngineState_t	m_nNextDLLState;

	double			m_flCurrentTime;
	double			m_flFrameTime;
	double			m_flPreviousTime;
	float			m_flFilteredTime;
	float			m_flMinFrameTime; // Expected duration of a frame, or zero if it is unlimited.
	float			m_flLastRemainder; // 'Unused' time on the last render loop.
	bool			m_bCatchupTime;
};

static CEngine g_Engine;

IEngine *eng = ( IEngine * )&g_Engine;
//IEngineAPI *engine = NULL;


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CEngine::CEngine( void )
{
	m_nDLLState			= DLL_INACTIVE;
	m_nNextDLLState		= DLL_INACTIVE;

	m_flCurrentTime		= 0.0;
	m_flFrameTime		= 0.0f;
	m_flPreviousTime	= 0.0;
	m_flFilteredTime	= 0.0f;
	m_flMinFrameTime	= 0.0f;
	m_flLastRemainder	= 0.0f;
	m_bCatchupTime		= false;

	m_nQuitting			= QUIT_NOTQUITTING;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CEngine::~CEngine( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEngine::Unload( void )
{
	Sys_ShutdownGame();

	m_nDLLState			= DLL_INACTIVE;
	m_nNextDLLState		= DLL_INACTIVE;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CEngine::Load( bool bDedicated, const char *rootdir )
{
	bool success = false;

	// Activate engine
	// NOTE: We must bypass the 'next state' block here for initialization to work properly.
	m_nDLLState = m_nNextDLLState = InEditMode() ? DLL_PAUSED : DLL_ACTIVE;

	if ( Sys_InitGame( 
		g_AppSystemFactory,
		rootdir, 
		game->GetMainWindowAddress(), 
		bDedicated ) )
	{
		success = true;

		UpdateMaterialSystemConfig();
	}
	
	return success;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : dt - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CEngine::FilterTime( float dt )
{
	if ( sv.IsDedicated() && !g_bDedicatedServerBenchmarkMode )
	{
		m_flMinFrameTime = host_nexttick;
		return ( dt >= host_nexttick );
	}

	m_flMinFrameTime = 0.0f;

	// Dedicated's tic_rate regulates server frame rate.  Don't apply fps filter here.
	// Only do this restriction on the client. Prevents clients from accomplishing certain
	// hacks by pausing their client for a period of time.
	if ( IsPC() && !sv.IsDedicated() && !CanCheat() && fps_max.GetFloat() < 30 )
	{
		// Don't do anything if fps_max=0 (which means it's unlimited).
		if ( fps_max.GetFloat() != 0.0f )
		{
			Warning( "sv_cheats is 0 and fps_max is being limited to a minimum of 30 (or set to 0).\n" );
			fps_max.SetValue( 30.0f );
		}
	}

	float fps = fps_max.GetFloat();
	if ( fps > 0.0f )
	{
		// Limit fps to withing tolerable range
//		fps = max( MIN_FPS, fps ); // red herring - since we're only checking if dt < 1/fps, clamping against MIN_FPS has no effect
		fps = min( MAX_FPS, (double)fps );

		float minframetime = 1.0 / fps;

		m_flMinFrameTime = minframetime;

		if (
#if !defined(SWDS)
		    !demoplayer->IsPlayingTimeDemo() && 
#endif
			!g_bDedicatedServerBenchmarkMode && 
			dt < minframetime )
		{
			// framerate is too high
			return false;		
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
void CEngine::Frame( void )
{
	// yield the CPU for a little while when paused, minimized, or not the focus
	// FIXME:  Move this to main windows message pump?
	if ( IsPC() && !game->IsActiveApp() && !sv.IsDedicated() && engine_no_focus_sleep.GetInt() > 0 )
	{
		VPROF_BUDGET( "Sleep", VPROF_BUDGETGROUP_SLEEPING );
#if defined( RAD_TELEMETRY_ENABLED )
		if( !g_Telemetry.Level )
#endif
			g_pInputSystem->SleepUntilInput( engine_no_focus_sleep.GetInt() );
	}

	if ( m_flPreviousTime == 0 )
	{
		(void) FilterTime( 0.0f );
		m_flPreviousTime = Sys_FloatTime() - m_flMinFrameTime;
	}

	// Watch for data from the CPU frequency monitoring system and print it to the console.
	const CPUFrequencyResults frequency = GetCPUFrequencyResults();
	static double s_lastFrequencyTimestamp;
	if ( frequency.m_timeStamp > s_lastFrequencyTimestamp )
	{
		s_lastFrequencyTimestamp = frequency.m_timeStamp;
		Msg( "~CPU Freq: %1.3f GHz    Percent of requested: %3.1f%%    Minimum percent seen: %3.1f%%\n",
					frequency.m_GHz, frequency.m_percentage, frequency.m_lowestPercentage );
	}

	// Loop until it is time for our frame. Don't return early because pumping messages
	// and processing console input is expensive (0.1 ms for each call to ProcessConsoleInput).
	for (;;)
	{
		// Get current time
		m_flCurrentTime	= Sys_FloatTime();

		// Determine dt since we last ticked
		m_flFrameTime = m_flCurrentTime - m_flPreviousTime;

		// This should never happen...
		Assert( m_flFrameTime >= 0.0f );
		if ( m_flFrameTime < 0.0f )
		{
			// ... but if the clock ever went backwards due to a bug,
			// we'd have no idea how much time has elapsed, so just 
			// catch up to the next scheduled server tick.
			m_flFrameTime = host_nexttick;
		}

		if ( FilterTime( m_flFrameTime )  )
		{
			// Time to render our frame.
			break;
		}

		if ( IsPC() && ( !sv.IsDedicated() || host_timer_spin_ms.GetFloat() != 0 ) )
		{
			// ThreadSleep may be imprecise. On non-dedicated servers, we busy-sleep
			// for the last one or two milliseconds to ensure very tight timing.
			float fBusyWaitMS = IsWindows() ? 2.25f : 1.5f;
			if ( sv.IsDedicated() )
			{
				fBusyWaitMS = host_timer_spin_ms.GetFloat();
				fBusyWaitMS = MAX( fBusyWaitMS, 0.5f );
			}

			// If we are meeting our frame rate then go idle for a while
			// to avoid wasting power and to let other threads/processes run.
			// Calculate how long we need to wait.
			int nSleepMS = (int)( ( m_flMinFrameTime - m_flFrameTime ) * 1000 - fBusyWaitMS );
			if ( nSleepMS > 0 )
			{
				ThreadSleep( nSleepMS );
			}
			else
			{
				// On x86, busy-wait using PAUSE instruction which encourages
				// power savings by idling for ~10 cycles (also yielding to
				// the other logical hyperthread core if the CPU supports it)
				for (int i = 2000; i >= 0; --i)
				{
#if defined(POSIX)
					__asm( "pause" ); __asm( "pause" ); __asm( "pause" ); __asm( "pause" );
#elif defined(IS_WINDOWS_PC)
					_asm { pause }; _asm { pause }; _asm { pause }; _asm { pause };
#endif
				}
			}

			// Go back to the top of the loop and see if it is time yet.
		}
		else
		{
			int nSleepMicrosecs = (int) ceilf( clamp( ( m_flMinFrameTime - m_flFrameTime ) * 1000000.f, 1.f, 1000000.f ) );
#ifdef POSIX
			usleep( nSleepMicrosecs );
#else
			ThreadSleep( (nSleepMicrosecs + 999) / 1000 );
#endif
		}
	}

	if ( ShouldSerializeAsync() )
	{
		static ConVar *pSyncReportConVar = g_pCVar->FindVar( "fs_report_sync_opens" );
		bool bReportingSyncOpens = ( pSyncReportConVar && pSyncReportConVar->GetInt() );
		int reportLevel = 0;
		if ( bReportingSyncOpens )
		{
			reportLevel = pSyncReportConVar->GetInt();
			pSyncReportConVar->SetValue( 0 );
		}
		g_pFileSystem->AsyncFinishAll();
		if ( bReportingSyncOpens )
		{
			pSyncReportConVar->SetValue( reportLevel );
		}
	}

#ifdef VPROF_ENABLED
	PreUpdateProfile( m_flFrameTime );
#endif
	
	// Reset swallowed time...
	m_flFilteredTime = 0.0f;

#ifndef SWDS
	if ( !sv.IsDedicated() )
	{
		ClientDLL_FrameStageNotify( FRAME_START );
		ETWRenderFrameMark( false );
	}
#endif

#ifdef VPROF_ENABLED
	PostUpdateProfile();
#endif
	TelemetryTick();

	{ // profile scope

	VPROF_BUDGET( "CEngine::Frame", VPROF_BUDGETGROUP_OTHER_UNACCOUNTED );
	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );
#ifdef RAD_TELEMETRY_ENABLED
	TmU64 time0 = tmFastTime();
#endif


	switch( m_nDLLState )
	{
	case DLL_PAUSED:			// paused, in hammer
	case DLL_INACTIVE:			// no dll
		break;

	case DLL_ACTIVE:			// engine is focused
	case DLL_CLOSE:				// closing down dll
	case DLL_RESTART:			// engine is shutting down but will restart right away
		// Run the engine frame
		HostState_Frame( m_flFrameTime );
		break;
	}

	// Has the state changed?
	if ( m_nNextDLLState != m_nDLLState )
	{
		m_nDLLState = m_nNextDLLState;

		// Do special things if we change to particular states
		switch( m_nDLLState )
		{
		case DLL_CLOSE:
			SetQuitting( QUIT_TODESKTOP );
			break;
		case DLL_RESTART:
			SetQuitting( QUIT_RESTART );
			break;
		}
	}

#ifdef RAD_TELEMETRY_ENABLED
	float time = ( tmFastTime() - time0 ) * g_Telemetry.flRDTSCToMilliSeconds;
	if( time > 0.5f )
	{
		tmPlot( TELEMETRY_LEVEL0, TMPT_TIME_MS, 0, time, "CEngine::Frame" );
	}
#endif
	} // profile scope


	// Remember old time
	m_flPreviousTime = m_flCurrentTime;

#if defined( VPROF_ENABLED ) && defined( _X360 )
	UpdateVXConsoleProfile();
#endif
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CEngine::EngineState_t CEngine::GetState( void )
{
	return m_nDLLState;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEngine::SetNextState( EngineState_t iNextState )
{
	m_nNextDLLState = iNextState;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CEngine::GetFrameTime( void )
{
	return m_flFrameTime;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CEngine::GetCurTime( void )
{
	return m_flCurrentTime;
}


//-----------------------------------------------------------------------------
// Purpose: Flag that we are in the process of quiting
//-----------------------------------------------------------------------------
void CEngine::SetQuitting( int quittype )
{
	m_nQuitting = quittype;
}


//-----------------------------------------------------------------------------
// Purpose: Check whether we are ready to exit
//-----------------------------------------------------------------------------
int CEngine::GetQuitting( void )
{
	return m_nQuitting;
}
