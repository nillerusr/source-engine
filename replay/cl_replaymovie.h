//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#ifndef REPLAYMOVIE_H
#define REPLAYMOVIE_H
#ifdef _WIN32
#pragma once
#endif

//----------------------------------------------------------------------------------------

#include "replay/ireplaymovie.h"
#include "replay/basereplayserializeable.h"
#include "replay/replaytime.h"
#include "replay/rendermovieparams.h"
#include "utlstring.h"

//----------------------------------------------------------------------------------------

#define REPLAY_MOVIE_HANDLE_FIRST_VALID		((ReplayHandle_t)5000)

//----------------------------------------------------------------------------------------

class CReplayMovie : public CBaseReplaySerializeable,
					 public IReplayMovie
{
	typedef CBaseReplaySerializeable BaseClass;

public:
	CReplayMovie();

	//
	// IReplaySerializeable
	//
	virtual bool			Read( KeyValues *pIn );
	virtual void			Write( KeyValues *pOut );
	virtual const char		*GetSubKeyTitle() const;
	virtual const char		*GetPath() const;
	virtual void			OnDelete();

	//
	// IReplayMovie
	//
	virtual ReplayHandle_t	GetMovieHandle() const;
	virtual ReplayHandle_t	GetReplayHandle() const;
	virtual const ReplayRenderSettings_t &GetRenderSettings();
	virtual void			GetFrameDimensions( int &nWidth, int &nHeight );
	virtual void			SetIsRendered( bool bIsRendered );
	virtual void			SetMovieFilename( const char *pFilename );
	virtual const char		*GetMovieFilename() const;
	virtual void			SetMovieTitle( const wchar_t *pTitle );
	virtual void			SetRenderTime( float flRenderTime );
	virtual float			GetRenderTime() const;
	virtual void			CaptureRecordTime();
	virtual void			SetLength( float flLength );
	virtual bool			IsUploaded() const;
	virtual void			SetUploaded( bool bUploaded );
	virtual void			SetUploadURL( const char *pURL );
	virtual const char		*GetUploadURL() const;

	//
	// IQueryableReplayItem
	//
	virtual const CReplayTime &	GetItemDate() const;
	virtual bool			IsItemRendered() const;
	virtual CReplay			*GetItemReplay();
	virtual ReplayHandle_t	GetItemReplayHandle() const;
	virtual QueryableReplayItemHandle_t	GetItemHandle() const;
	virtual const wchar_t	*GetItemTitle() const;
	virtual void			SetItemTitle( const wchar_t *pTitle );
	virtual float			GetItemLength() const;
	virtual void			*GetUserData();
	virtual void			SetUserData( void *pUserData );
	virtual bool			IsItemAMovie() const;

	CReplay					*GetReplay() const;
	bool					ReadRenderSettings( KeyValues *pIn );
	void					WriteRenderSettings( KeyValues *pOut );

	ReplayHandle_t			m_hReplay;		// The replay associated with this movie, or 0 if the replay has been deleted
	wchar_t					m_wszTitle[256];// Title for the movie
	CUtlString				m_strFilename;	// Relative (to game dir) path and filename of the movie
	CUtlString				m_strUploadURL;	// Link to uploaded YouTube video
	bool					m_bRendered;	// Has the movie finished rendering?
	void					*m_pUserData;
	bool					m_bUploaded;
	float					m_flRenderTime;	// How many seconds it took to render the movie
	CReplayTime				m_RecordTime;	// What date/time was this movie recorded?
	float					m_flLength;		// The movie length

	ReplayRenderSettings_t	m_RenderSettings;
};

//----------------------------------------------------------------------------------------

inline CReplayMovie *ToMovie( IReplaySerializeable *pMovie )
{
	return static_cast< CReplayMovie * >( pMovie );
}

//----------------------------------------------------------------------------------------

#endif	// REPLAYMOVIE_H
