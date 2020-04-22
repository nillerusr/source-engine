//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#ifndef REPLAYMOVIEMANAGER_H
#define REPLAYMOVIEMANAGER_H
#ifdef _WIN32
#pragma once
#endif

//----------------------------------------------------------------------------------------

#include "replay/ireplaymoviemanager.h"
#include "replay/shared_defs.h"
#include "genericpersistentmanager.h"
#include "cl_replaymovie.h"
#include "utlvector.h"

//----------------------------------------------------------------------------------------

class IClientReplayHistoryManager;
struct MaterialSystem_Config_t;

//----------------------------------------------------------------------------------------

class CReplayMovieManager : public CGenericPersistentManager< CReplayMovie >,
							public IReplayMovieManager
{
	typedef CGenericPersistentManager< CReplayMovie > BaseClass;

public:
	CReplayMovieManager();
	~CReplayMovieManager();

	virtual bool			Init();

	//
	// CBaseThinker
	//
	virtual float			GetNextThinkTime() const;

	//
	// IReplayMovieManager
	//
	virtual int				GetMovieCount();
	virtual void			GetMovieList( CUtlLinkedList< IReplayMovie * > &list );
	virtual IReplayMovie	*GetMovie( ReplayHandle_t hMovie );
	virtual IReplayMovie	*CreateAndAddMovie( ReplayHandle_t hReplay );
	virtual void			DeleteMovie( ReplayHandle_t hMovie );
	virtual int				GetNumMoviesDependentOnReplay( const CReplay *pReplay );
	virtual void			SetPendingMovie( IReplayMovie *pMovie );
	virtual IReplayMovie	*GetPendingMovie();
	virtual void			FlagMovieForFlush( IReplayMovie *pMovie, bool bImmediate );
	virtual void			GetMoviesAsQueryableItems( CUtlLinkedList< IQueryableReplayItem *, int > &lstMovies );
	virtual const char		*GetRenderDir() const;
	virtual const char		*GetRawExportDir() const;

	virtual void			RenderMovie( RenderMovieParams_t const& params );
	virtual void			RenderNextMovie();
	virtual bool			IsRendering() const		{ return m_bIsRendering; }
	virtual bool			RenderingCancelled() const { return m_bRenderingCancelled; }
	virtual void			CompleteRender( bool bSuccess, bool bShowBrowser );
	virtual void			ClearRenderCancelledFlag();
	virtual void			CancelRender();

	void					AddMovie( CReplayMovie *pNewMovie );
	CReplayMovie			*CastMovie( IReplayMovie *pMovie );
	void 					CacheMovieTitle( const wchar_t *pTitle );
	void 					GetCachedMovieTitleAndClear( wchar_t *pOut, int nBufLength );
	RenderMovieParams_t		&GetRenderMovieSettings()		{ return *m_pRenderMovieSettings; }

private:
	//
	// CGenericPersistentManager
	//
	virtual const char		*GetDebugName() const			{ return "movie manager"; }
	virtual const char		*GetIndexFilename() const		{ return "movies." GENERIC_FILE_EXTENSION; }
	virtual CReplayMovie	*Create();
	virtual const char		*GetRelativeIndexPath() const;
	virtual int				GetVersion() const;
	virtual int				GetHandleBase() const			{ return MOVIE_HANDLE_BASE; }
	virtual IReplayContext	*GetReplayContext() const;

	void					AddReplayForRender( CReplay *pReplay, int iPerformance );
	void					SetupVideo( RenderMovieParams_t const &params );
	void					SetupHighDetailModels();
	void					SetupHighDetailTextures();
	void					SetupHighQualityAntialiasing();
	void					SetupHighQualityFiltering();
	void					SetupHighQualityShadowDetail();
	void					SetupHighQualityHDR();
	void					SetupHighQualityWaterDetail();
	void					SetupMulticoreRender();
	void					SetupHighQualityShaderDetail();
	void					SetupColorCorrection();
	void					SetupMotionBlur();

	wchar_t					m_wszCachedMovieTitle[MAX_REPLAY_TITLE_LENGTH];
	IReplayMovie			*m_pPendingMovie;
	MaterialSystem_Config_t	*m_pVidModeSettings;	// Used to restore video mode settings after render completion
	RenderMovieParams_t		*m_pRenderMovieSettings;

	bool					m_bIsRendering;
	bool					m_bRenderingCancelled;

	//
	// TODO:
	//   - date rendered
	//
};

//----------------------------------------------------------------------------------------

#endif		// REPLAYMOVIEMANAGER_H
