#include "cbase.h"
#include "asw_jukebox.h"
#include "entityinput.h"
#include "entityoutput.h"
#include "asw_gamerules.h"

LINK_ENTITY_TO_CLASS( asw_jukebox, CASW_Jukebox );

BEGIN_DATADESC( CASW_Jukebox )
	DEFINE_KEYFIELD( m_fFadeInTime, FIELD_FLOAT, "FadeInTime" ),
	DEFINE_KEYFIELD( m_fFadeOutTime, FIELD_FLOAT, "FadeOutTime" ),
	DEFINE_KEYFIELD( m_szDefaultMusic, FIELD_STRING, "DefaultMusic" ),

	DEFINE_INPUTFUNC( FIELD_VOID, "StartMusic", InputMusicStart ),
	DEFINE_INPUTFUNC( FIELD_VOID, "StopMusic", InputMusicStop ),

END_DATADESC()

void CASW_Jukebox::InputMusicStart( inputdata_t &inputdata )
{
	// Send each client the music start event
	IGameEvent * event = gameeventmanager->CreateEvent( "jukebox_play_random" );
	if ( event )
	{
		event->SetFloat( "fadeintime", m_fFadeInTime );
		event->SetString( "defaulttrack", m_szDefaultMusic );
		gameeventmanager->FireEvent( event );

		// Stop stim music if it's playing
		ASWGameRules()->m_fPreventStimMusicTime = 5.0f;
	}
}

void CASW_Jukebox::InputMusicStop( inputdata_t &inputdata )
{
	// Tell each client to stop the music
	IGameEvent * event = gameeventmanager->CreateEvent( "jukebox_stop" );
	if ( event )
	{
		event->SetFloat( "fadeouttime", m_fFadeOutTime );
		gameeventmanager->FireEvent( event );
	}
}