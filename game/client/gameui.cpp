//===== Copyright (c) 1996-2009, Valve Corporation, All rights reserved. ====//
//
// Purpose: Contains all utility methods for the new game UI system
//
//===========================================================================//

#include "cbase.h"
#include "gameui.h"
#include "tier1/timeutils.h"
#include "cdll_client_int.h"
#include "soundemittersystem/isoundemittersystembase.h"
#include "engine/ienginesound.h"
#include "filesystem.h"
#include "inputsystem/iinputstacksystem.h"

#include "gameui/basemodpanel.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// A logging channel used during engine initialization
DEFINE_LOGGING_CHANNEL_NO_TAGS( LOG_GameUI, "GameUI" );

LINK_GAME_CONTROLS_LIB();


// A utility function to determine if we are running as part of game ui base mod panel
inline bool IsPartOfGameUiBaseModPanel()
{
	BaseModUI::CBaseModPanel *pPanel = BaseModUI::CBaseModPanel::GetSingletonPtr();
	if ( !pPanel )
		return false;

	return pPanel->IsVisible();
}

//-----------------------------------------------------------------------------
// Default game ui sound playback implementation
//-----------------------------------------------------------------------------
class CInGameUISoundPlayback : public IGameUISoundPlayback
{
public:
	void *EmitSound( const char *pSoundName )
	{
		if ( developer.GetBool() )
		{
			// Ensure the sound is valid
			int nSoundIndex = g_pSoundEmitterSystem->GetSoundIndex( pSoundName );
			if ( !g_pSoundEmitterSystem->IsValidIndex( nSoundIndex ) )
			{
				Log_Warning( LOG_GameUI, "Attempted to play invalid sound \"%s\"\n", pSoundName );
				return NULL;
			}

			const char *pSourceFile = g_pSoundEmitterSystem->GetSourceFileForSound( nSoundIndex );
			if ( !Q_stristr( pSourceFile, "game_sounds_ui.txt" ) )
			{
				Log_Warning( LOG_GameUI, "Attempted to play invalid sound \"%s\". This sound must be defined\n"
					"in game_sounds_ui.txt but was defined in \"%s\" instead.\n", pSoundName, pSourceFile );
				return NULL;
			}
		}

		// Pull data from parameters
		CSoundParameters params;
		HSOUNDSCRIPTHANDLE handle = SOUNDEMITTER_INVALID_HANDLE;
		if ( !soundemitterbase->GetParametersForSoundEx( pSoundName, handle, params, GENDER_NONE, true ) )
		{
			Log_Warning( LOG_GameUI, "Attempted to play invalid sound \"%s\"\n", pSoundName );
			return NULL;
		}

		if ( !params.soundname[0] )
		{
			Log_Warning( LOG_GameUI, "Attempted to play invalid sound \"%s\", which has no .wav!\n", pSoundName );
			return NULL;
		}

		float st = 0.0f;
		if ( params.delay_msec != 0.0f )
		{
			st = gpGlobals->curtime + (float)params.delay_msec / 1000.f;
		}

		CLocalPlayerFilter filter;
		enginesound->EmitSound( 
			filter, 
			SOUND_FROM_LOCAL_PLAYER, 
			params.channel, 
			params.soundname,
			params.volume,
			(soundlevel_t)params.soundlevel,
			0, // flags
			params.pitch,
			NULL,
			NULL,
			NULL,
			true,
			st );
		return (void*)enginesound->GetGuidForLastSoundEmitted();
	}

	void StopSound( void *pSoundHandle )
	{
		if ( !pSoundHandle || !g_pSoundSystem )
			return;

		int nGuid = (int)pSoundHandle;
		enginesound->StopSoundByGuid( nGuid );
	}
};

static CInGameUISoundPlayback s_InGameUISoundPlayback;


//-----------------------------------------------------------------------------
// Instantiate singleton game system
//-----------------------------------------------------------------------------
static CGameUIGameSystem s_GameUIGameSystem;
CGameUIGameSystem *g_pGameUIGameSystem = &s_GameUIGameSystem;


//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------
bool CGameUIGameSystem::Init()
{
	g_pGameUISystemMgr->UseGameInputSystemEventQueue( true );
	g_pGameUISystemMgr->SetSoundPlayback( &s_InGameUISoundPlayback );
	g_pGameUISystemMgr->SetInputContext( engine->GetInputContext( ENGINE_INPUT_CONTEXT_GAMEUI ) );

	// Register all client nugget factories
	CUiNuggetFactoryRegistrarBase::RegisterAll();

	return true;
}

void CGameUIGameSystem::Shutdown()
{
	g_pGameUISystemMgr->SetSoundPlayback( NULL );
	g_pGameUISystemMgr->SetInputContext( INPUT_CONTEXT_HANDLE_INVALID );
}


//-----------------------------------------------------------------------------
// PostInit
//-----------------------------------------------------------------------------
void CGameUIGameSystem::PostInit()
{
	// This cannot happen in Init() because the audio system isn't set up
	// at that point, and it needs to be for precache to succeed
	PrecacheGameUISounds();
}


//-----------------------------------------------------------------------------
// Precaches all game UI sounds
//-----------------------------------------------------------------------------
void CGameUIGameSystem::PrecacheGameUISounds()
{
	// Precache all UI sounds. These must exist in the game_sounds_ui.txt script file
	KeyValues *pUIGameSounds = new KeyValues( "Game Instructor Counts" );
	if ( !pUIGameSounds->LoadFromFile( g_pFullFileSystem, "scripts/game_sounds_ui.txt", "GAME" ) )
	{
		pUIGameSounds->deleteThis();
		return;
	}

	for ( KeyValues *pKey = pUIGameSounds; pKey; pKey = pKey->GetNextKey() )
	{
		const char *pSoundName = pKey->GetName();
		int nSoundIndex = soundemitterbase->GetSoundIndex( pSoundName );
		if ( !soundemitterbase->IsValidIndex( nSoundIndex ) )
		{
			Log_Warning( LOG_GameUI, "GameUI: Unable to precache gamesound \"%s\"\n", pSoundName );
			continue;
		}

		CSoundParametersInternal *pInternal = soundemitterbase->InternalGetParametersForSound( nSoundIndex );
		if ( !pInternal )
		{
			Log_Warning( LOG_GameUI, "GameUI: Unable to precache gamesound \"%s\"\n", pSoundName );
			continue;
		}

		int nWaveCount = pInternal->NumSoundNames();
		if ( !nWaveCount )
		{
			Log_Warning( LOG_GameUI, "GameUI: game_sounds_ui.txt entry '%s' has no waves listed under 'wave' or 'rndwave' key!!!\n", pSoundName );
			continue;
		}

		for( int nWave = 0; nWave < nWaveCount; ++nWave )
		{
			const char *pWavName = soundemitterbase->GetWaveName( pInternal->GetSoundNames()[ nWave ].symbol );
			enginesound->PrecacheSound( pWavName, true, true );
		}
	}
	pUIGameSounds->deleteThis();
}


//-----------------------------------------------------------------------------
// Reloads game GUI sounds
//-----------------------------------------------------------------------------
void CGameUIGameSystem::ReloadSounds()
{
	// Should I stop all currently playing sounds?
	PrecacheGameUISounds();
}

//-----------------------------------------------------------------------------
// Init any render targets needed by the UI.
//-----------------------------------------------------------------------------
void CGameUIGameSystem::InitRenderTargets()
{
	g_pGameUISystemMgr->InitRenderTargets();
}

//-----------------------------------------------------------------------------
// Init any render targets needed by the UI.
//-----------------------------------------------------------------------------
IMaterialProxy *CGameUIGameSystem::CreateProxy( const char *proxyName )
{
	return g_pGameUISystemMgr->CreateProxy( proxyName );
}

//-----------------------------------------------------------------------------
// Renders the game UI
//-----------------------------------------------------------------------------
void CGameUIGameSystem::Render( const Rect_t &viewport, float flCurrentTime )
{
	g_pGameUISystemMgr->Render( viewport, DmeTime_t( flCurrentTime ) );
}


//-----------------------------------------------------------------------------
// Registers an input event
//-----------------------------------------------------------------------------
bool CGameUIGameSystem::RegisterInputEvent( const InputEvent_t &iEvent )
{
	if ( !g_pGameUISystemMgr->GetGameUIVisible() )
		return false;
	switch( iEvent.m_nType )
	{
	case IE_ButtonPressed:
		{
			// NOTE: data2 is the virtual key code (data1 contains the scan-code one)
			ButtonCode_t code = (ButtonCode_t)iEvent.m_nData2;
			if ( code == KEY_BACKQUOTE )
			{
				return false;
			}
			else if ( code == KEY_ESCAPE )
			{
				return false;
			}
		}
		break;

	case IE_ButtonReleased:
		{
			// NOTE: data2 is the virtual key code (data1 contains the scan-code one)
			ButtonCode_t code = (ButtonCode_t)iEvent.m_nData2;
			if ( code == KEY_BACKQUOTE )
			{
				return false;
			}
			else if ( code == KEY_ESCAPE )
			{
				return false;
			}
		}
		break;
	default:
		break;
	}
	

	g_pGameUISystemMgr->RegisterInputEvent( iEvent );

	// FIXME: This is a hack; we're saying that the system eats the
	// input as long as a menu is visible
	return g_pGameUISystemMgr->GetGameUIVisible();
}


//-----------------------------------------------------------------------------
// Simulates the next frame
//-----------------------------------------------------------------------------
void CGameUIGameSystem::Update( float frametime )
{
	if ( IsPartOfGameUiBaseModPanel() )
		// Prevent double-updates during a frame when running as part of game ui
		// base mod panel rendering and event processing
		return;

	g_pGameUISystemMgr->RunFrame();
}
