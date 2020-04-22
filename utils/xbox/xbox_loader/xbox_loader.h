//========= Copyright Valve Corporation, All rights reserved. ============//
//
//	XBOX_LOADER.H
//
//	Master Include
//=====================================================================================//

#pragma once

#include <xtl.h>
#include <XBApp.h>
#include <XBFont.h>
#include <XBHelp.h>
#include <xgraphics.h>
#include <xfont.h>
#include <xmv.h>
#include <xbdm.h>
#include <math.h>
#include "XBResource.h"
#include "xmvhelper.h"
#include "toollib.h"
#include "scriplib.h"
#include "loader.h"
#include "jcalg1.h"
#include "xbox/xbox_launch.h"

#define XBOX_FORENSIC_LOG

#define SCREEN_WIDTH			640
#define SCREEN_HEIGHT			480
#define MAX_FILES				500
#define MAX_SLIDESHOW_TEXTURES	9

#define LEGAL_DISPLAY_TIME		6000
#define LOADINGBAR_UPTIME		500.0f	// slid up or down
#define LOADINGBAR_SLIDETIME	1500.0f	// progress speed
#define LOADINGBAR_WAITTIME		500.0f	// delay after up to begin slide
#define SLIDESHOW_SLIDETIME		7000
#define SLIDESHOW_FLIPTIME		1000

#define LEGAL_MAIN				0
#define LEGAL_SOURCE			1

#define FOOTER_W				512

#define SEGMENT_W				10
#define SEGMENT_GAP				1
#define SEGMENT_COUNT			26

#define PROGRESS_Y				405
#define PROGRESS_W				(SEGMENT_COUNT*(SEGMENT_W+SEGMENT_GAP))
#define PROGRESS_H				15
#define PROGRESS_X				124

#define PROGRESS_FOOTER_COLOR	0x88FFFFFF
#define PROGRESS_INSET_COLOR	0xFF222222
#define PROGRESS_SEGMENT_COLOR	0xFFCC6C00
#define PROGRESS_TEXT_COLOR		0xFFFFFFFF

//-----------------------------------------------------------------------------
// Main class to run this application. Most functionality is inherited
// from the CXBApplication base class.
//-----------------------------------------------------------------------------
class CXBoxLoader : public CXBApplication
{
public:
    CXBoxLoader();

	virtual HRESULT Initialize( void );
    virtual HRESULT Render( void );
    virtual HRESULT FrameMove( void );

	void			DrawRect( int x, int y, int w, int h, DWORD color );
	void			DrawLegals();
	void			DrawDebug();
	BOOL			PlayVideoFrame();
    HRESULT			StartVideo( const CHAR* strFilename, bool bFromMemory, bool bFatalOnError );
	void			StopVideo();
	bool			StartInstall( void );
	bool			LoadInstallScript( void );
	D3DTexture		*LoadTexture( int resourceID );
	HRESULT			LoadFont( CXBFont *pFont, int resourceID ); 
	void			DrawTexture( D3DTexture *pD3DTexture, int x, int y, int w, int h, int color );
	void			StartLegalScreen( int legal );
	void			DrawProgressBar();
	void			DrawLoadingMarquee();
	void			DrawSlideshow();
	bool			VerifyInstall();
	void			StartDashboard( bool bGotoMemory );
	void			LoadLogFile();
	void			DrawLog();
	void			FatalMediaError();
	void			LaunchHL2( unsigned int contextCode );
	void			TickVideo();

private:
	IDirect3DTexture8	*m_pLastMovieFrame;
	D3DTexture			*m_pFooterTexture;
	D3DTexture			*m_pLoadingIconTexture;
	D3DTexture			*m_pMainLegalTexture;
	D3DTexture			*m_pSourceLegalTexture;
	D3DTexture			*m_pLegalTexture;
	D3DTexture			*m_pSlideShowTextures[MAX_SLIDESHOW_TEXTURES];

    CXMVPlayer			m_player;

	D3DVertexBuffer		*m_pVB;
	CXBPackedResource	m_xprResource;

	CXBFont				m_Font;

	int					m_contextCode;

	char				*m_fileSrc[MAX_FILES];
	char				*m_fileDest[MAX_FILES];
	xCompressHeader		*m_fileCompressionHeaders[MAX_FILES];
	DWORD				m_fileDestSizes[MAX_FILES];
	int					m_numFiles;

	bool				m_bAllowAttractAbort;
	bool				m_bDrawLoading;
	bool				m_bDrawProgress;
	bool				m_bDrawDebug;
	bool				m_bLaunch;
	DWORD				m_dwLoading;
	bool				m_bDrawLegal;
	HANDLE				m_installThread;
	DWORD				m_LegalTime;
	int					m_State;
	DWORD				m_LoadingBarStartTime;
	DWORD				m_LoadingBarEndTime;
	DWORD				m_LegalStartTime;
	bool				m_bInstallComplete;
	int					m_Version;
	int					m_FrameCounter;
	int					m_MovieCount;
	bool				m_bMovieErrorIsFatal;
	bool				m_bCaptureLastMovieFrame;
	DWORD				m_SlideShowStartTime;
	bool				m_bDrawSlideShow;
	int					m_SlideShowCount;
	bool				m_bFinalSlide;
	char				*m_pLogData;
	XFONT*				m_pDefaultTrueTypeFont;
};

struct CopyStats_t
{
	char				m_srcFilename[MAX_PATH];
	char				m_dstFilename[MAX_PATH];
	DWORD				m_readSize;
	DWORD				m_writeSize;
	DWORD				m_bytesCopied;
	DWORD				m_totalReadTime;
	DWORD				m_totalWriteTime;
	DWORD				m_totalReadSize;
	DWORD				m_totalWriteSize;
	DWORD				m_bufferReadSize;
	DWORD				m_bufferWriteSize;
	DWORD				m_bufferReadTime;
	DWORD				m_bufferWriteTime;
	DWORD				m_inflateSize;
	DWORD				m_inflateTime;
	DWORD				m_copyTime;
	DWORD				m_copyErrors;
	DWORD				m_numReadBuffers;
	DWORD				m_numWriteBuffers;
};

extern bool CopyFileOverlapped( const char *pSrc, const char *pDest, xCompressHeader *pxcHeader, CopyStats_t *pCopyStats );
extern bool CreateFilePath( const char *inPath );
