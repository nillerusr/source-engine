//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Methods associated with the cursor
//
// $Revision: $
// $NoKeywords: $
//===========================================================================//

#if !defined( _X360 )
	#define OEMRESOURCE //for OCR_* cursor junk
	#include "winlite.h"
#endif
#include <appframework/ilaunchermgr.h>

#if defined( USE_SDL )
#undef M_PI
#include "SDL.h"
#endif

#include "tier0/dbg.h"
#include "tier0/vcrmode.h"
#include "tier0/icommandline.h"
#include "tier1/utldict.h"
#include "Cursor.h"
#include "vguimatsurface.h"
#include "MatSystemSurface.h"
#include "filesystem.h"
#if defined( _X360 )
#include "xbox/xbox_win32stubs.h"
#endif

#if defined( USE_SDL ) 
#include "materialsystem/imaterialsystem.h"
#endif

#include "inputsystem/iinputsystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
#if defined( USE_SDL )
static SDL_Cursor *s_pDefaultCursor[ dc_last ];
static SDL_Cursor *s_hCurrentCursor = NULL;
static SDL_Cursor *s_hCurrentlySetCursor = NULL;
#elif defined( WIN32 )
static HICON s_pDefaultCursor[ dc_last ];
static HICON s_hCurrentCursor = NULL;
#endif
static bool s_bCursorLocked = false; 
static bool s_bCursorVisible = true;
static int s_nForceCursorVisibleCount = 0;
static bool s_bSoftwareCursorActive = false;
static int  s_nSoftwareCursorTexture = -1;
static float  s_fSoftwareCursorOffsetX = 0;
static float  s_fSoftwareCursorOffsetY = 0;
static int	s_rnSoftwareCursorID[20];
static float s_rfSoftwareCursorOffset[20][2];
static bool s_bSoftwareCursorsInitialized = false;

extern CMatSystemSurface g_MatSystemSurface;

//-----------------------------------------------------------------------------
// Initializes cursors
//-----------------------------------------------------------------------------
void InitCursors()
{
	// load up all default cursors
#if defined( USE_SDL )

	s_pDefaultCursor[ dc_none ]     = NULL;
	s_pDefaultCursor[ dc_arrow ]    = SDL_CreateSystemCursor( SDL_SYSTEM_CURSOR_ARROW );
	s_pDefaultCursor[ dc_ibeam ]    = SDL_CreateSystemCursor( SDL_SYSTEM_CURSOR_IBEAM );
	s_pDefaultCursor[ dc_hourglass ]= SDL_CreateSystemCursor( SDL_SYSTEM_CURSOR_WAIT );
	s_pDefaultCursor[ dc_crosshair ]= SDL_CreateSystemCursor( SDL_SYSTEM_CURSOR_CROSSHAIR );
	s_pDefaultCursor[ dc_waitarrow ]= SDL_CreateSystemCursor( SDL_SYSTEM_CURSOR_WAITARROW );
	s_pDefaultCursor[ dc_sizenwse ] = SDL_CreateSystemCursor( SDL_SYSTEM_CURSOR_SIZENWSE );
	s_pDefaultCursor[ dc_sizenesw ] = SDL_CreateSystemCursor( SDL_SYSTEM_CURSOR_SIZENESW );
	s_pDefaultCursor[ dc_sizewe ]   = SDL_CreateSystemCursor( SDL_SYSTEM_CURSOR_SIZEWE );
	s_pDefaultCursor[ dc_sizens ]   = SDL_CreateSystemCursor( SDL_SYSTEM_CURSOR_SIZENS );
	s_pDefaultCursor[ dc_sizeall ]  = SDL_CreateSystemCursor( SDL_SYSTEM_CURSOR_SIZEALL );
	s_pDefaultCursor[ dc_no ]       = SDL_CreateSystemCursor( SDL_SYSTEM_CURSOR_NO );
	s_pDefaultCursor[ dc_hand ]     = SDL_CreateSystemCursor( SDL_SYSTEM_CURSOR_HAND );

	s_hCurrentCursor = s_pDefaultCursor[ dc_arrow ];

#elif defined( WIN32 )

	s_pDefaultCursor[ dc_none ]      = NULL;
	s_pDefaultCursor[ dc_arrow ]     =(HICON)LoadCursor(NULL, (LPCTSTR)OCR_NORMAL);
	s_pDefaultCursor[ dc_ibeam ]     =(HICON)LoadCursor(NULL, (LPCTSTR)OCR_IBEAM);
	s_pDefaultCursor[ dc_hourglass ] =(HICON)LoadCursor(NULL, (LPCTSTR)OCR_WAIT);
	s_pDefaultCursor[ dc_crosshair ] =(HICON)LoadCursor(NULL, (LPCTSTR)OCR_CROSS);
	s_pDefaultCursor[ dc_waitarrow ] =(HICON)LoadCursor(NULL, (LPCTSTR)32650);
	s_pDefaultCursor[ dc_up ]        =(HICON)LoadCursor(NULL, (LPCTSTR)OCR_UP);
	s_pDefaultCursor[ dc_sizenwse ]  =(HICON)LoadCursor(NULL, (LPCTSTR)OCR_SIZENWSE);
	s_pDefaultCursor[ dc_sizenesw ]  =(HICON)LoadCursor(NULL, (LPCTSTR)OCR_SIZENESW);
	s_pDefaultCursor[ dc_sizewe ]    =(HICON)LoadCursor(NULL, (LPCTSTR)OCR_SIZEWE);
	s_pDefaultCursor[ dc_sizens ]    =(HICON)LoadCursor(NULL, (LPCTSTR)OCR_SIZENS);
	s_pDefaultCursor[ dc_sizeall ]   =(HICON)LoadCursor(NULL, (LPCTSTR)OCR_SIZEALL);
	s_pDefaultCursor[ dc_no ]        =(HICON)LoadCursor(NULL, (LPCTSTR)OCR_NO);
	s_pDefaultCursor[ dc_hand ]      =(HICON)LoadCursor(NULL, (LPCTSTR)32649);

	s_hCurrentCursor = s_pDefaultCursor[ dc_arrow ];

#endif

	s_bCursorLocked = false;
	s_bCursorVisible = true;
	s_nForceCursorVisibleCount = 0;
}


#define USER_CURSOR_MASK 0x80000000

#ifdef WIN32
//-----------------------------------------------------------------------------
// Purpose: Simple manager for user loaded windows cursors in vgui
//-----------------------------------------------------------------------------
class CUserCursorManager
{
public:
	void Shutdown();
	vgui::HCursor CreateCursorFromFile( char const *curOrAniFile, char const *pPathID );
	bool LookupCursor( vgui::HCursor cursor, HCURSOR& handle );
private:
	CUtlDict< HCURSOR, int >	m_UserCursors;
};

void CUserCursorManager::Shutdown()
{
	for ( int i = m_UserCursors.First() ; i != m_UserCursors.InvalidIndex(); i = m_UserCursors.Next( i ) )
	{
		::DestroyCursor( m_UserCursors[ i ] );
	}
	m_UserCursors.RemoveAll();
}

vgui::HCursor CUserCursorManager::CreateCursorFromFile( char const *curOrAniFile, char const *pPathID )
{
	char fn[ 512 ];
	Q_strncpy( fn, curOrAniFile, sizeof( fn ) );
	Q_strlower( fn );
	Q_FixSlashes( fn );
	
	int cursorIndex = m_UserCursors.Find( fn );
	if ( cursorIndex != m_UserCursors.InvalidIndex() )
	{
		return cursorIndex | USER_CURSOR_MASK;
	}

	g_pFullFileSystem->GetLocalCopy( fn );

	char fullpath[ 512 ];
	g_pFullFileSystem->RelativePathToFullPath( fn, pPathID, fullpath, sizeof( fullpath ) );
	
	HCURSOR newCursor = (HCURSOR)LoadCursorFromFile( fullpath );
	cursorIndex = m_UserCursors.Insert( fn, newCursor );
	return cursorIndex | USER_CURSOR_MASK;
}

bool CUserCursorManager::LookupCursor( vgui::HCursor cursor, HCURSOR& handle )
{
	if ( !( (int)cursor & USER_CURSOR_MASK ) )
	{
		handle = 0;
		return false;
	}

	int cursorIndex = (int)cursor & ~USER_CURSOR_MASK;
	if ( !m_UserCursors.IsValidIndex( cursorIndex ) )
	{
		handle = 0;
		return false;
	}

	handle = m_UserCursors[ cursorIndex ];
	return true;
}

static CUserCursorManager g_UserCursors;
#endif

vgui::HCursor Cursor_CreateCursorFromFile( char const *curOrAniFile, char const *pPathID )
{
#ifdef WIN32 
	return g_UserCursors.CreateCursorFromFile( curOrAniFile, pPathID );
#else
	return dc_user;
#endif
}


void Cursor_ClearUserCursors()
{
#ifdef WIN32 
	g_UserCursors.Shutdown();
#endif
}


//-----------------------------------------------------------------------------
// Initializes all the textures for software cursors
//-----------------------------------------------------------------------------
int InitSoftwareCursorTexture( const char *pchFilename )
{
	if( !pchFilename || !*pchFilename )
		return -1;

	int nTextureID = g_MatSystemSurface.DrawGetTextureId( pchFilename );
	if( nTextureID == -1 )
	{
		nTextureID = g_MatSystemSurface.CreateNewTextureID();
		g_MatSystemSurface.DrawSetTextureFile( nTextureID, pchFilename, true, false );
	}
	return nTextureID;
}

void InitSoftwareCursors()
{
	if( s_bSoftwareCursorsInitialized )
		return;

	memset( s_rfSoftwareCursorOffset, 0, sizeof( s_rfSoftwareCursorOffset ) );

	s_rnSoftwareCursorID[dc_none]     = -1;
	s_rnSoftwareCursorID[dc_arrow]    =InitSoftwareCursorTexture( "vgui/cursors/arrow" );
	s_rnSoftwareCursorID[dc_ibeam]    =InitSoftwareCursorTexture( "vgui/cursors/ibeam" );
	s_rnSoftwareCursorID[dc_hourglass]=InitSoftwareCursorTexture( "vgui/cursors/hourglass" );
	s_rnSoftwareCursorID[dc_crosshair]=InitSoftwareCursorTexture( "vgui/cursors/crosshair" );
	s_rnSoftwareCursorID[dc_waitarrow]=InitSoftwareCursorTexture( "vgui/cursors/waitarrow" );
	s_rnSoftwareCursorID[dc_up]       =InitSoftwareCursorTexture( "vgui/cursors/up" );
	s_rnSoftwareCursorID[dc_sizenwse] =InitSoftwareCursorTexture( "vgui/cursors/sizenwse" );
	s_rnSoftwareCursorID[dc_sizenesw] =InitSoftwareCursorTexture( "vgui/cursors/sizenesw" );
	s_rnSoftwareCursorID[dc_sizewe]   =InitSoftwareCursorTexture( "vgui/cursors/sizewe" );
	s_rnSoftwareCursorID[dc_sizens]   =InitSoftwareCursorTexture( "vgui/cursors/sizens" );
	s_rnSoftwareCursorID[dc_sizeall]  =InitSoftwareCursorTexture( "vgui/cursors/sizeall" );
	s_rnSoftwareCursorID[dc_no]       =InitSoftwareCursorTexture( "vgui/cursors/no" );
	s_rnSoftwareCursorID[dc_hand]     =InitSoftwareCursorTexture( "vgui/cursors/hand" );

	// handle the cursor hotspots not being at their origin
	s_rfSoftwareCursorOffset[dc_arrow][0] = -0.1;
	s_rfSoftwareCursorOffset[dc_arrow][1] = -0.1;
	s_rfSoftwareCursorOffset[dc_ibeam][0] = -0.5;
	s_rfSoftwareCursorOffset[dc_ibeam][1] = -0.8;
	s_rfSoftwareCursorOffset[dc_hourglass][0] = -0.5;
	s_rfSoftwareCursorOffset[dc_hourglass][1] = -0.5;
	s_rfSoftwareCursorOffset[dc_crosshair][0] = -0.5;
	s_rfSoftwareCursorOffset[dc_crosshair][1] = -0.5;
	s_rfSoftwareCursorOffset[dc_waitarrow][0] = -0.1;
	s_rfSoftwareCursorOffset[dc_waitarrow][1] = -0.1;
	s_rfSoftwareCursorOffset[dc_up][0] = -0.5;
	s_rfSoftwareCursorOffset[dc_up][1] = -0.5;
	s_rfSoftwareCursorOffset[dc_sizenwse][0] = -0.5;
	s_rfSoftwareCursorOffset[dc_sizenwse][1] = -0.5;
	s_rfSoftwareCursorOffset[dc_sizenesw][0] = -0.5;
	s_rfSoftwareCursorOffset[dc_sizenesw][1] = -0.5;
	s_rfSoftwareCursorOffset[dc_sizewe][0] = -0.5;
	s_rfSoftwareCursorOffset[dc_sizewe][1] = -0.5;
	s_rfSoftwareCursorOffset[dc_sizens][0] = -0.5;
	s_rfSoftwareCursorOffset[dc_sizens][1] = -0.5;
	s_rfSoftwareCursorOffset[dc_sizeall][0] = -0.5;
	s_rfSoftwareCursorOffset[dc_sizeall][1] = -0.5;
	s_rfSoftwareCursorOffset[dc_no][0] = -0.5;
	s_rfSoftwareCursorOffset[dc_no][1] = -0.5;
	s_rfSoftwareCursorOffset[dc_hand][0] = -0.5;
	s_rfSoftwareCursorOffset[dc_hand][1] = -0.5;

	s_bSoftwareCursorsInitialized = true;
}


//-----------------------------------------------------------------------------
// Selects a cursor
//-----------------------------------------------------------------------------
void CursorSelect(HCursor hCursor)
{
	if ( ( hCursor == dc_alwaysvisible_push ) || ( hCursor == dc_alwaysvisible_pop ) )
	{
		// CConPanel in engine/console.cpp does a SetCursor(null). So when the TF2 chat window pops up
		//	and there are console commands showing and fading out in the top left, our chat window
		//	will have a cursor show/hide fight with them. So the cursor flickers or doesn't show up
		//	at all. Unfortunately on Linux, it's even worse since we recenter the mouse when it's
		//	not shown - so we added this API call which causes cursor.cpp to always show the cursor.
		s_nForceCursorVisibleCount += ( hCursor == dc_alwaysvisible_push ? 1 : -1 );
		Assert( s_nForceCursorVisibleCount >= 0 );

		if( ( s_nForceCursorVisibleCount && !s_bCursorVisible ) ||
			( !s_nForceCursorVisibleCount && s_bCursorVisible ) )
		{
			ActivateCurrentCursor();
		}
		return;
	}

	if (s_bCursorLocked)
		return;

#if defined( WIN32 ) && !defined( USE_SDL )
	s_bCursorVisible = true;
	switch (hCursor)
	{
	case dc_user:
	case dc_none:
	case dc_blank:
		s_bCursorVisible = false;
		break;

	case dc_arrow:
	case dc_waitarrow:
	case dc_ibeam:
	case dc_hourglass:
	case dc_crosshair:
	case dc_up:
	case dc_sizenwse:
	case dc_sizenesw:
	case dc_sizewe:
	case dc_sizens:
	case dc_sizeall:
	case dc_no:
	case dc_hand:
		if( !s_bSoftwareCursorActive )
		{
			s_hCurrentCursor = s_pDefaultCursor[hCursor];
		}
		else
		{
			s_nSoftwareCursorTexture = s_rnSoftwareCursorID[ hCursor ];
			s_fSoftwareCursorOffsetX = s_rfSoftwareCursorOffset[ hCursor ][0];
			s_fSoftwareCursorOffsetY = s_rfSoftwareCursorOffset[ hCursor ][1];
		}
		break;

	default:
		{
			HCURSOR custom = 0;
			if ( g_UserCursors.LookupCursor( hCursor, custom ) && custom != 0 )
			{
				s_hCurrentCursor = custom;
			}
			else
			{
				s_bCursorVisible = false;
				Assert(0);
			}
		}
		break;
	}

	ActivateCurrentCursor();

#elif defined( USE_SDL )

	switch (hCursor)
	{
	case dc_user:
	case dc_none:
	case dc_blank:
		s_bCursorVisible = false;
		break;

	default:
		// We don't support custom cursors at the moment (but could, if necessary).
		// Fall through and use the arrow for now...
		Assert(0);
		hCursor = dc_arrow;

	case dc_arrow:
	case dc_waitarrow:
	case dc_ibeam:
	case dc_hourglass:
	case dc_crosshair:
	case dc_up:
	case dc_sizenwse:
	case dc_sizenesw:
	case dc_sizewe:
	case dc_sizens:
	case dc_sizeall:
	case dc_no:
	case dc_hand:
		s_bCursorVisible = true;
		if( !s_bSoftwareCursorActive )
		{
			s_hCurrentCursor = s_pDefaultCursor[hCursor];
		}
		else
		{
			s_nSoftwareCursorTexture = s_rnSoftwareCursorID[ hCursor ];
			s_fSoftwareCursorOffsetX = s_rfSoftwareCursorOffset[ hCursor ][0];
			s_fSoftwareCursorOffsetY = s_rfSoftwareCursorOffset[ hCursor ][1];
		}
		break;
	}

	ActivateCurrentCursor();

#else
#error
#endif

}


//-----------------------------------------------------------------------------
// Hides the hardware cursor
//-----------------------------------------------------------------------------
void HideHardwareCursor()
{
#if defined( WIN32 ) && !defined( USE_SDL )
	::SetCursor(NULL);
#elif defined( USE_SDL )
	//if ( s_hCurrentlySetCursor != s_pDefaultCursor[ dc_none ] )
	{
		s_hCurrentlySetCursor = s_pDefaultCursor[ dc_none ];
		g_pLauncherMgr->SetMouseCursor( s_hCurrentlySetCursor );
		g_pLauncherMgr->SetMouseVisible( false );
	}
#else
#error
#endif
}


//-----------------------------------------------------------------------------
// Activates the current cursor
//-----------------------------------------------------------------------------
void ActivateCurrentCursor()
{
	if( s_bSoftwareCursorActive )
	{
		HideHardwareCursor();
		return;
	}

	if ( s_bCursorVisible || ( s_nForceCursorVisibleCount > 0 ) )
	{
#if defined( WIN32 ) && !defined( USE_SDL )
		::SetCursor(s_hCurrentCursor);
#elif defined( USE_SDL )
		if (s_hCurrentlySetCursor != s_hCurrentCursor )
		{
			s_hCurrentlySetCursor = s_hCurrentCursor;
			g_pLauncherMgr->SetMouseCursor( s_hCurrentlySetCursor );
			g_pLauncherMgr->SetMouseVisible( true );
		}
#else
#error
#endif
	}
	else
	{
		HideHardwareCursor();
	}
}


//-----------------------------------------------------------------------------
// Purpose: prevents vgui from changing the cursor
//-----------------------------------------------------------------------------
void LockCursor( bool bEnable )
{
	s_bCursorLocked = bEnable;
	ActivateCurrentCursor();
}


//-----------------------------------------------------------------------------
// Purpose: unlocks the cursor state
//-----------------------------------------------------------------------------
bool IsCursorLocked()
{
	return s_bCursorLocked;
}


//-----------------------------------------------------------------------------
// handles mouse movement
//-----------------------------------------------------------------------------
void CursorSetPos( void *hwnd, int x, int y )
{
#if defined( USE_SDL )
	if ( s_bCursorVisible )
#endif
		g_pInputSystem->SetCursorPosition( x, y );
}

void CursorGetPos(void *hwnd, int &x, int &y)
{
#if defined ( USE_SDL ) && !defined( PLATFORM_WINDOWS )
	if ( s_bCursorVisible )
	{
		SDL_GetMouseState( &x, &y );

		int windowHeight = 0;
		int windowWidth = 0;
		//unsigned int ignored;
		SDL_GetWindowSize( ( SDL_Window * )g_pLauncherMgr->GetWindowRef(), &windowWidth, &windowHeight );

		CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
		int rx, ry, width, height;
		pRenderContext->GetViewport( rx, ry, width, height );
	
		if ( !s_bSoftwareCursorActive && (width != windowWidth || height != windowHeight )  )
		{
			// scale the x/y back into the co-ords of the back buffer, not the scaled up window 
			//DevMsg( "Mouse x:%d y:%d %d %d %d %d\n", x, y, width, windowWidth, height, abs( height - windowHeight ) );
			x = x * (float)width/windowWidth;
			y = y * (float)height/windowHeight;
		}
	}
	else 
	{
		// cursor is invisible, just say we have it pinned to the middle of the screen
		CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
		int rx, ry, width, height;
		pRenderContext->GetViewport( rx, ry, width, height );
		x = rx + width/2;
		y = ry + height/2;
		//printf( "Mouse(inv) x:%d y:%d %d %d\n", x, y, width, height );
	}
#else
	POINT pt;

	// Default implementation
	VCRHook_GetCursorPos( &pt );
	VCRHook_ScreenToClient((HWND)hwnd, &pt);
	x = pt.x; y = pt.y;
#endif
}


void EnableSoftwareCursor( bool bEnable )
{
	if( bEnable )
		InitSoftwareCursors();

	bool bWasEnabled = s_bSoftwareCursorActive;
	s_bSoftwareCursorActive = bEnable;

	// set the cursor to the arrow (or none if appropriate) if we're activating the
	// software cursor. VGUI will likely update it again soon, but this will give
	// us some kind of cursor in the meantime
	if( !bWasEnabled && bEnable )
	{
		if( s_bCursorVisible )
			CursorSelect( dc_arrow );
	}
}

bool ShouldDrawSoftwareCursor()
{
	return s_bSoftwareCursorActive && s_bCursorVisible;
}

int  GetSoftwareCursorTexture( float *px, float *py )
{
	if( px && py )
	{
		*px = s_fSoftwareCursorOffsetX;
		*py = s_fSoftwareCursorOffsetY;
	}
	return s_nSoftwareCursorTexture;
}


