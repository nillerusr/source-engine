//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//===========================================================================//
#ifndef IGAME_H
#define IGAME_H

#ifdef _WIN32
#pragma once
#endif

class IGame
{
public:
	virtual			~IGame( void ) { }

	virtual	bool	Init( void *pvInstance ) = 0;
	virtual bool	Shutdown( void ) = 0;

	virtual bool	CreateGameWindow( void ) = 0;
	virtual void	DestroyGameWindow( void ) = 0;

	// This is used in edit mode to specify a particular game window (created by hammer)
	virtual void	SetGameWindow( void* hWnd ) = 0;

	// This is used in edit mode to override the default wnd proc associated w/
	// the game window specified in SetGameWindow. 
	virtual bool	InputAttachToGameWindow() = 0;
	virtual void	InputDetachFromGameWindow() = 0;

	virtual void	PlayStartupVideos( void ) = 0;

        // This will be the SDL_Window* under SDL, the WindowRef under Mac, and
        // the HWND on Win32.
	virtual void*	GetMainWindow( void ) = 0;
        // This will be the HWND under Direct3D, and the SDL_Window* or
        // WindowRef under GL.
	virtual void*	GetMainDeviceWindow( void ) = 0;

	virtual void**	GetMainWindowAddress( void ) = 0;
	virtual void	GetDesktopInfo( int &width, int &height, int &refreshrate ) = 0;

	virtual void	SetWindowXY( int x, int y ) = 0;
	virtual void	SetWindowSize( int w, int h ) = 0;

	virtual void	GetWindowRect( int *x, int *y, int *w, int *h ) = 0;

	// Not Alt-Tabbed away
	virtual bool	IsActiveApp( void ) = 0;

	virtual void	DispatchAllStoredGameMessages() = 0;
};

extern IGame *game;

#endif // IGAME_H
