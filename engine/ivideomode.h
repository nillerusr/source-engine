//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//

#ifndef IVIDEOMODE_H
#define IVIDEOMODE_H

#ifdef _WIN32
#pragma once
#endif

#include "vmodes.h"
#include "vtf/vtf.h"

struct MovieInfo_t;


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
abstract_class IVideoMode
{
public:	
	virtual					~IVideoMode() {}
	virtual	bool			Init( ) = 0;
	virtual void			Shutdown( void ) = 0;

	// Shows the start-up graphics based on the mod 
	// (Filesystem path for the mod must be set up first)
	virtual void			DrawStartupGraphic() = 0;

	// Creates the game window, plays the startup movie, starts up the material system
	virtual bool			CreateGameWindow( int nWidth, int nHeight, bool bWindowed ) = 0;

	// Sets the game window in editor mode
	virtual void			SetGameWindow( void *hWnd ) = 0;

	// Sets the video mode, and re-sizes the window
	virtual bool			SetMode( int nWidth, int nHeight, bool bWindowed ) = 0;

	// Returns the fullscreen modes for the adapter the game was started on
	virtual int				GetModeCount( void ) = 0;
	virtual struct vmode_s	*GetMode( int num ) = 0;

	// Purpose: This is called in response to a WM_MOVE message
	// or whatever the equivalent that would be under linux
	virtual void			UpdateWindowPosition( void ) = 0;

	// Alt-tab handling
	virtual void			RestoreVideo( void ) = 0;
	virtual void			ReleaseVideo( void ) = 0;

	virtual void			DrawNullBackground( void *hdc, int w, int h ) = 0;
	virtual void			InvalidateWindow() = 0;

	// Returns the video mode width + height. In the case of windowed mode,
	// it returns the width and height of the drawable region of the window.
	// (it doesn't include the window borders)
	virtual int				GetModeWidth() const = 0;
	virtual int				GetModeHeight() const = 0;
	virtual	bool			IsWindowedMode() const = 0;

	// Returns the video mode width + height for a single stereo display.
	// if the game isn't running in side by side stereo, this is the same
	// as GetModeWidth and GetModeHeight
	virtual int				GetModeStereoWidth() const = 0;
	virtual int				GetModeStereoHeight() const = 0;

	// Returns the size of full screen UI. This might be wider than a single
	// eye on displays like the Oculus Rift where a single eye is very narrow.
	// if the game isn't running in side by side stereo, this is the same
	// as GetModeWidth and GetModeHeight
	virtual int				GetModeUIWidth() const = 0;
	virtual int				GetModeUIHeight() const = 0;

	// Returns the subrect to draw the client view into.
	// Coordinates are measured relative to the drawable region of the window
	virtual const vrect_t &	GetClientViewRect( ) const = 0;
	virtual void			SetClientViewRect( const vrect_t &viewRect ) = 0;

	// Lazily recomputes client view rect
	virtual void			MarkClientViewRectDirty() = 0;

	virtual void			TakeSnapshotTGA( const char *pFileName ) = 0;
	virtual void			TakeSnapshotTGARect( const char *pFilename, int x, int y, int w, int h, int resampleWidth, int resampleHeight, bool bPFM = false, CubeMapFaceIndex_t faceIndex = CUBEMAP_FACE_RIGHT ) = 0;
	virtual void			WriteMovieFrame( const MovieInfo_t& info ) = 0;

	// Takes snapshots
	virtual void			TakeSnapshotJPEG( const char *pFileName, int quality ) = 0;
	virtual bool			TakeSnapshotJPEGToBuffer( CUtlBuffer& buf, int quality ) = 0;

	virtual void			ReadScreenPixels( int x, int y, int w, int h, void *pBuffer, ImageFormat format ) = 0;
};


//-----------------------------------------------------------------------------
// Utilities for virtual screen coordinates
//-----------------------------------------------------------------------------
#define XRES(x)	( x  * ( ( float )videomode->GetModeStereoWidth() / 640.0 ) )
#define YRES(y)	( y  * ( ( float )videomode->GetModeStereoHeight() / 480.0 ) )


//-----------------------------------------------------------------------------
// Singleton accessor 
//-----------------------------------------------------------------------------
extern IVideoMode *videomode;


//-----------------------------------------------------------------------------
// Class factory 
//-----------------------------------------------------------------------------
void VideoMode_Create();
void VideoMode_Destroy();


#endif // IVIDEOMODE_H
