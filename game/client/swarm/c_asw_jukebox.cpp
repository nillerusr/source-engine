#include "cbase.h"
#include "c_asw_jukebox.h"
#include "soundenvelope.h"
#include "filesystem.h"
#include "KeyValues.h"
#include "gamestringpool.h"
#include "fmtstr.h"

const char *sz_PlaylistFilename = "scripts/jukebox_playlist.txt";

CASWJukeboxPlaylist g_ASWJukebox;

TrackInfo_t::TrackInfo_t()
: m_szHexname( null )
, m_szFilename( null )
, m_szAlbum( null )
, m_szArtist( null )
, m_szGenre( null )
, m_bIsMarkedForDeletion( false )
{

}

TrackInfo_t::TrackInfo_t( const char *szFilename, const char *szHexname, const char *szAlbum, const char *szArtist, const char *szGenre )
: m_szHexname( AllocPooledString( szHexname ) )
, m_szFilename( AllocPooledString( szFilename ))
, m_szAlbum( szAlbum )
, m_szArtist( szArtist )
, m_szGenre( szGenre )
, m_bIsMarkedForDeletion( false )
{}

void TrackInfo_t::PrepareKVForListView( KeyValues *kv )
{
	// add the file to the list
	kv->SetString("text", m_szFilename );
	kv->SetString("genre", m_szGenre );
	kv->SetString("artist", m_szArtist );
	kv->SetString("album", m_szAlbum );
}

bool TrackInfo_t::operator==( const TrackInfo_t& rhs ) const
{
	return( FStrEq( m_szHexname, rhs.m_szHexname ) );
}

static void PlayRandomTrackFromJukebox( void )
{
	Msg( "Playing random track from jukebox..." );

	IGameEvent * event = gameeventmanager->CreateEvent( "jukebox_play_random" );
	if ( event )
	{
		gameeventmanager->FireEvent( event );
	}
}

static void StopJukebox( void )
{
	Msg( "Stopping jukebox..." );

	IGameEvent * event = gameeventmanager->CreateEvent( "jukebox_stop" );
	if ( event )
	{
		gameeventmanager->FireEvent( event );
	}
}

static ConCommand ASW_StopJukebox( "ASW_StopJukebox", StopJukebox, "Stop the jukebox music", FCVAR_DEVELOPMENTONLY );
static ConCommand ASW_PlayRandomTrackFromJukebox( "ASW_PlayRandomTrackFromJukebox", PlayRandomTrackFromJukebox, "Play a random track from the user's jukebox", FCVAR_DEVELOPMENTONLY );
static ConVar ASW_JukeboxFadeOutTime( "ASW_JukeboxFadeOutTime", "1.0" );

void CASWJukeboxPlaylist::AddMusicToPlaylist( const char *szFilename, const char *szHexname, const char *szAlbum, const char *szArtist, const char *szGenre )
{
	for( int i=0; i<m_CombatMusicPlaylist.Count(); ++i )
	{
		if( FStrEq( m_CombatMusicPlaylist[i].m_szHexname, szHexname ) )
			return;
	}
	
	m_CombatMusicPlaylist.AddToTail( TrackInfo_t( szFilename, szHexname, szAlbum, szArtist, szGenre ) );
}

void CASWJukeboxPlaylist::LoadPlaylistKV()
{
	KeyValues *pKV = new KeyValues( "playlist" );
	if( pKV->LoadFromFile( filesystem, sz_PlaylistFilename ) )
	{
		// If the load succeeded, create the playlist
		for( KeyValues *sub = pKV->GetFirstSubKey(); sub != NULL; sub = sub->GetNextTrueSubKey() )
		{
			const char *szTrackName = sub->GetString("TrackName");
			const char *szHexName = sub->GetString("HexName");
			const char *szAlbum = sub->GetString("Album");
			const char *szArtist = sub->GetString("Artist");
			const char *szGenre = sub->GetString("Genre");
			AddMusicToPlaylist( szTrackName, szHexName, szAlbum, szArtist, szGenre );
		}
	}
}

bool CASWJukeboxPlaylist::Init()
{
	m_pCombatMusic = NULL;
	m_iCurrentTrack = -1;

	// Load the saved playlist
	LoadPlaylistKV();

	ListenForGameEvent( "jukebox_play_random" );
	ListenForGameEvent( "jukebox_stop" );


	return true;
}

void CASWJukeboxPlaylist::Shutdown()
{
	StopListeningForAllEvents();
}

void CASWJukeboxPlaylist::FireGameEvent( IGameEvent *event )
{
	if( !event )
		return;

	if( FStrEq( event->GetName(), "jukebox_play_random") )
	{
		// Play some random music
		float fadeInTime = event->GetFloat( "fadeintime" );
		const char *szDefaultTrack = event->GetString( "defaulttrack" );
		PlayRandomTrack( fadeInTime, szDefaultTrack );
	}
	else if( FStrEq( event->GetName(), "jukebox_stop" ) )
	{
		// Always fade the music from the stop event
		float fadeOutTime = event->GetFloat( "fadeouttime" );
		StopTrack( false, fadeOutTime );
	}
}

void CASWJukeboxPlaylist::PlayRandomTrack( float fadeInTime /*= 1.0f*/, const char *szDefaultTrack /*= null */ )
{
	// Choose a random track to play
	int count = m_CombatMusicPlaylist.Count();
	int index = 0;
	CSoundPatch* pNewSound = null;
	CLocalPlayerFilter filter;
	if( count == 0 )
	{
		DevMsg( "JUKEBOX: Playing Track: %s%s.mp3\n", "*#music/_mp3/", szDefaultTrack );
		if( !Q_strcmp("", szDefaultTrack ) )
			return;
		pNewSound = CSoundEnvelopeController::GetController().SoundCreate( filter, 0, CHAN_STATIC, szDefaultTrack, SNDLVL_NONE );
	}
	else 
	{
		// If there's more than one track, randomize it so the current track doesn't repeat itself
		if( count > 1 )
		{
			do {
				index = rand() % (count );
			} while( index == m_iCurrentTrack );
		}

		DevMsg( "JUKEBOX: Playing Track: %s%s.mp3\n", "*#music/_mp3/", m_CombatMusicPlaylist[index].m_szHexname );
		pNewSound = CSoundEnvelopeController::GetController().SoundCreate( filter, 0, CHAN_STATIC, CFmtStr( "%s%s.mp3", "*#music/_mp3/", m_CombatMusicPlaylist[index].m_szHexname), SNDLVL_NONE );
	}

	if( !pNewSound )
	{
		return;
	}

	// If combat music is playing and there's more than one track, fade it out and play the new music once it's done
	if( m_pCombatMusic )
	{
		if( count == 1 )
			return;

		StopTrack( false, fadeInTime );
		CSoundEnvelopeController::GetController().Play( pNewSound, 0.0f, 100, 1.0f );	
	}
	else
	{
		CSoundEnvelopeController::GetController().Play( pNewSound, 0.0f, 100 );	
	}
	CSoundEnvelopeController::GetController().SoundChangeVolume( pNewSound, 1.0f, fadeInTime );

	m_pCombatMusic = pNewSound;
	m_iCurrentTrack = index;
}

void CASWJukeboxPlaylist::StopTrack( bool immediate /*= true*/, float fadeOutTime /*= 1.0f */ )
{
	if( m_pCombatMusic )
	{
		if( immediate )
			CSoundEnvelopeController::GetController().SoundDestroy( m_pCombatMusic );
		else
			CSoundEnvelopeController::GetController().SoundFadeOut( m_pCombatMusic, fadeOutTime, true );

		m_pCombatMusic = NULL;
		m_iCurrentTrack = -1;
	}
}

void CASWJukeboxPlaylist::LevelShutdownPostEntity( void )
{
	// Stop music
	StopTrack( true );
	RemoveAllMusic();
}

void CASWJukeboxPlaylist::ExportPlayistKV( void )
{
	KeyValues *pPlaylistKV = new KeyValues("playlist");

	for( int i=0; i<m_CombatMusicPlaylist.Count(); ++i )
	{
		KeyValues *pTrackKV = new KeyValues("Track");
		pTrackKV->SetString( "TrackName", m_CombatMusicPlaylist[i].m_szFilename );
		pTrackKV->SetString( "HexName", m_CombatMusicPlaylist[i].m_szHexname );
		pTrackKV->SetString( "Album", m_CombatMusicPlaylist[i].m_szAlbum );
		pTrackKV->SetString( "Artist", m_CombatMusicPlaylist[i].m_szArtist);
		pTrackKV->SetString( "Genre", m_CombatMusicPlaylist[i].m_szGenre );
		pPlaylistKV->AddSubKey( pTrackKV );
	}
	pPlaylistKV->SaveToFile( filesystem, sz_PlaylistFilename );
	pPlaylistKV->deleteThis();
}

void CASWJukeboxPlaylist::SavePlaylist()
{
	Cleanup();
	ExportPlayistKV();
}

const char * CASWJukeboxPlaylist::GetTrackName( int index )
{
	if( index >= m_CombatMusicPlaylist.Count() )
		return null;
	else
		return m_CombatMusicPlaylist[index].m_szFilename;
}

const char * CASWJukeboxPlaylist::GetTrackArtist( int index )
{
	if( index >= m_CombatMusicPlaylist.Count() )
		return null;
	else
		return m_CombatMusicPlaylist[index].m_szArtist;
}

const char * CASWJukeboxPlaylist::GetTrackAlbum( int index )
{
	if( index >= m_CombatMusicPlaylist.Count() )
		return null;
	else
		return m_CombatMusicPlaylist[index].m_szAlbum;
}

const char * CASWJukeboxPlaylist::GetTrackGenre( int index )
{
	if( index >= m_CombatMusicPlaylist.Count() )
		return null;
	else
		return m_CombatMusicPlaylist[index].m_szGenre;
}

int CASWJukeboxPlaylist::GetTrackCount()
{
	return m_CombatMusicPlaylist.Count();
}

void CASWJukeboxPlaylist::RemoveMusicFromPlaylist( const char* szHexnameToRemove )
{
	if( m_CombatMusicPlaylist.Count() > 0 )
	{
		TrackInfo_t temp;
		temp.m_szHexname = szHexnameToRemove;

		m_CombatMusicPlaylist.FindAndFastRemove( temp );
		//m_CombatMusicPlaylist.Remove( szHexnameToRemove );
	}
}

void CASWJukeboxPlaylist::PrepareTrackKV( int index, KeyValues *pKV )
{
	if( index >= m_CombatMusicPlaylist.Count() && !m_CombatMusicPlaylist[index].m_bIsMarkedForDeletion )
		return;
	else
		m_CombatMusicPlaylist[index].PrepareKVForListView( pKV );
}

void CASWJukeboxPlaylist::MarkTrackForDeletion( int index )
{
	if( index >= m_CombatMusicPlaylist.Count() )
		return;
	else
	{
		if( index == m_iCurrentTrack )
			StopTrack( true, 0.0f );

		m_CombatMusicPlaylist[index].m_bIsMarkedForDeletion = true;

		// Delete the audio file too
		const char *szFullpath = CFmtStr( "%s%s.mp3", "sound/music/_mp3/", m_CombatMusicPlaylist[index].m_szHexname );
		if( filesystem->FileExists( szFullpath, NULL ) )
		{
			filesystem->RemoveFile( szFullpath, NULL );
		}
	}
}

void CASWJukeboxPlaylist::Cleanup( void )
{
	for( int i=0; i<m_CombatMusicPlaylist.Count(); )
	{
		if( m_CombatMusicPlaylist[i].m_bIsMarkedForDeletion )
			m_CombatMusicPlaylist.Remove( i );
		else
			++i;
	}
}

void CASWJukeboxPlaylist::LevelInitPostEntity( void )
{
	RemoveAllMusic();
	// Load the saved playlist
	LoadPlaylistKV();
}

void CASWJukeboxPlaylist::RemoveAllMusic( void )
{
	StopTrack( true, 0.0f );
	m_CombatMusicPlaylist.RemoveAll();
}

bool CASWJukeboxPlaylist::IsMusicPlaying( void )
{
	return m_pCombatMusic != NULL;
}