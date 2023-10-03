//====== Copyright © 1996-2006, Valve Corporation, All rights reserved. =======//
//
// Purpose: Handles construction of a user defined music playlist.
//
//=============================================================================//
#ifndef _DEFINED_C_ASW_JUKEBOX_H
#define _DEFINED_C_ASW_JUKEBOX_H

#include "igamesystem.h"
#include "GameEventListener.h"
#include "utlvector.h"
#include "platform.h"

class CSoundPatch;

// Information about music tracks
struct TrackInfo_t
{
	TrackInfo_t();
	TrackInfo_t( const char *szFilename, const char *szHexname, const char *szAlbum, const char *szArtist, const char *szGenre );

	string_t	m_szHexname;
	string_t	m_szFilename;
	string_t	m_szAlbum;
	string_t	m_szArtist;
	string_t	m_szGenre;
	bool		m_bIsMarkedForDeletion;

	bool	operator==( const TrackInfo_t& rhs ) const;
	void	PrepareKVForListView( KeyValues *kv );
};

struct ID3Info_t;

//-----------------------------------------------------------------------------
// Purpose: Lets a client define a playlist to use for various music events in game.
//-----------------------------------------------------------------------------
class CASWJukeboxPlaylist : public CGameEventListener, public CAutoGameSystem
{
public:
	void AddMusicToPlaylist( const char *szFilename, const char *szHexname, const char *szAlbum, const char *szArtist, const char *szGenre );
	void MarkTrackForDeletion( int index );

	// CAutoGameSystem methods
	virtual bool Init( void );
	virtual void Shutdown( void );
	virtual void LevelInitPostEntity( void );
	virtual void LevelShutdownPostEntity( void );

	// CGameEventListener methods
	virtual void FireGameEvent( IGameEvent *event );
	void StopTrack( bool immediate = true, float fadeOutTime = 1.0f );
	void SavePlaylist( void );
	const char *GetTrackName( int index );
	const char *GetTrackArtist( int index );
	const char *GetTrackAlbum( int index );
	const char *GetTrackGenre( int index );
	void PrepareTrackKV( int index, KeyValues *pKV );
	int GetTrackCount( void );
	void Cleanup( void );
	bool IsMusicPlaying( void );

private:
	void RemoveAllMusic( void );
	void RemoveMusicFromPlaylist( const char* szHexnameToRemove );
	void LoadPlaylistKV( void );
	void ExportPlayistKV( void );
	void PlayRandomTrack( float fadeInTime = 1.0f, const char *szDefaultTrack = null );

	typedef CUtlVector< TrackInfo_t > TrackList_t;
	TrackList_t		m_CombatMusicPlaylist;
	CSoundPatch		*m_pCombatMusic; // The current combat music that's playing
	int				m_iCurrentTrack; // The index of the current track being played
};

extern CASWJukeboxPlaylist g_ASWJukebox;

#endif // #ifndef _DEFINED_C_ASW_JUKEBOX_H