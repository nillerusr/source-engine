//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Linux support for the IGame interface
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//
#include "iengine.h"
#include <stdlib.h>

#include "engine_launcher_api.h"
#include "basetypes.h"
#include "ivideomode.h"
#include "igame.h"

#define UINT unsigned int
#define WPARAM int
#define LPARAM int

#include "profile.h"
#include "server.h"
#include "cdll_int.h"

#ifdef SWDS
void ForceReloadProfile( void );

void ClearIOStates( void );
//-----------------------------------------------------------------------------
// Purpose: Main game interface, including message pump and window creation
//-----------------------------------------------------------------------------
class CGame : public IGame
{
public:
					CGame( void );
	virtual			~CGame( void );

	bool			Init( void *pvInstance );
	bool			Shutdown( void );

	bool			CreateGameWindow( void );
	virtual void	DestroyGameWindow( void );
	virtual void	SetGameWindow( void *hWnd );

	virtual bool	InputAttachToGameWindow();
	virtual void	InputDetachFromGameWindow();

	void*			GetMainWindow( void );
	void*			GetMainDeviceWindow( void );
	void**			GetMainWindowAddress( void );

	void			SetWindowXY( int x, int y );
	void			SetWindowSize( int w, int h );
	void			GetWindowRect( int *x, int *y, int *w, int *h );

	bool			IsActiveApp( void );
	virtual void		DispatchAllStoredGameMessages();
	virtual void		PlayStartupVideos() {}
	virtual void		GetDesktopInfo( int &width, int &height, int &refreshRate );
private:
	void			SetActiveApp( bool fActive );

private:
	bool m_bActiveApp;
	static const char CLASSNAME[];

};

static CGame g_Game;
IGame *game = ( IGame * )&g_Game;

const char CGame::CLASSNAME[] = "Valve001";

// In VCR playback mode, it sleeps this amount each frame.
int g_iVCRPlaybackSleepInterval = 0;

// During VCR playback, if this is true, then it'll pause at the end of each frame.
bool g_bVCRSingleStep = false;

void VCR_EnterPausedState()
{
        // Turn this off in case they're in single-step mode.
        g_bVCRSingleStep = false;

        // This is cheesy, but GetAsyncKeyState is blocked (in protected_things. h)
        // from being accidentally used, so we get it through it by getting its pointer directly.

         // In this mode, we enter a wait state where we only pay attention to R and Q.
 /*        while ( 1 )
         {
                 if ( pfn( 'R' ) & 0x8000 )
                        break;

                if ( pfn( 'Q' ) & 0x8000 )
                      kill( getpid(), SIGKILL );

                if ( pfn( 'S' ) & 0x8000 )
                {
                        // Do a single step.
                        g_bVCRSingleStep = true;
                        break;
                }

                Sleep( 2 );
        }
*/
}


bool CGame::CreateGameWindow( void )
{
	return true;
}

void CGame::DestroyGameWindow( void )
{
}


// This is used in edit mode to override the default wnd proc associated w/
bool CGame::InputAttachToGameWindow()
{
	return true;
}

void CGame::InputDetachFromGameWindow()
{
}

void CGame::SetGameWindow( void *hWnd )
{
	return;
}

CGame::CGame( void )
{
	m_bActiveApp = true;
}

CGame::~CGame( void )
{
}

bool CGame::Init( void *pvInstance )
{
	return true;
}

bool CGame::Shutdown( void )
{
	return true;
}

void *CGame::GetMainWindow( void )
{
	return 0;
}

void *CGame::GetMainDeviceWindow( void )
{
	return 0;
}

void **CGame::GetMainWindowAddress( void )
{
	return NULL;
}

void CGame::SetWindowXY( int x, int y )
{
}

void CGame::SetWindowSize( int w, int h )
{
}

void CGame::GetWindowRect( int *x, int *y, int *w, int *h )
{
	if ( x )
	{
		*x = 0;
	}
	if ( y )
	{
		*y = 0;
	}
	if ( w )
	{
		*w = 0;
	}
	if ( h )
	{
		*h = 0;
	}
}

bool CGame::IsActiveApp( void )
{
	return m_bActiveApp;
}

void CGame::SetActiveApp( bool active )
{
	m_bActiveApp = active;
}

void CGame::DispatchAllStoredGameMessages()
{
}

void CGame::GetDesktopInfo( int &width, int &height, int &refreshRate )
{
    width = 0;
    height = 0;
	refreshRate = 0;
}

#endif
