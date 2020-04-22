//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef UITOPLEVELWINDOWOVERLAY_H
#define UITOPLEVELWINDOWOVERLAY_H

#ifdef _WIN32
#pragma once
#endif

// bad reach
#include "../../../panorama/input/controller.h"

class CSharedMemStream;
namespace IPC
{
	class IEvent;
	class ISharedMem;
}

namespace panorama
{
class CTopLevelWindowOverlay;
class COverlayInterface;


class CTopLevelWindowOverlay : public CTopLevelWindow
{
	typedef CTopLevelWindow BaseClass;
public:
	CTopLevelWindowOverlay( CUIEngine *pUIEngineParent );
	virtual ~CTopLevelWindowOverlay();

	// Access overlay window interface for this window, NULL on non Steam Overlay windows
	virtual IUIOverlayWindow *GetOverlayInterface();

	// Initialize backing surface for window
	virtual bool BInitializeSurface( const char *pchWindowTitle, int nWidth, int nHeight, IUIEngine::ERenderTarget eRenderTarget, bool bFixedSurfaceSize, bool bEnforceWindowAspectRatio, bool bUseCustomMouseCursor, const char *pchTargetMonitor );

	// Run any per window frame func logic
	virtual void RunPlatformFrame();

	// Resize the window to specified dimensions
	virtual void OnWindowResize( uint32 nWidth, uint32 nHeight );

	// Window position management
	virtual void SetWindowPosition( float x, float y );
	virtual void GetWindowPosition( float &x, float &y );
	virtual void GetWindowBounds( float &left, float &top, float &right, float &bottom );
	virtual void GetClientDimensions( float &width, float &height );
	virtual void Activate( bool bForceful );
	virtual bool BHasFocus() { return m_bFocus; } 
	virtual bool BIsFullscreen() { return m_bFullScreen; }
	virtual void* GetNativeWindowHandle() { return 0; }

	virtual bool BAllowInput( InputMessage_t &msg );
	virtual bool BIsVisible() { return m_bVisible; }
	virtual void SetVisible( bool bVisible ) { AssertMsg( false, "SetVisible not implemented on CTopLevelWindowOverlay" ); }

	void SetInputEnabled( bool bEnabled ) { m_bInputEnabled = bEnabled; }

	// Clear color for the window, normally black, transparent for overlay
	virtual Color GetClearColor() { return Color( 0, 0, 0, 0 ); }

	// Necessary for generating mouse enter & leave events on windows
	bool IsMouseOver() { return m_bMouseOverWindow; }
	void OnMouseEnter();
	void OnMouseLeave() { m_bMouseOverWindow = false; }

	void SetMouseCursor( EMouseCursors eCursor );
	
	bool SetGameProcessInfo( AppId_t nAppId, bool bCanSharedSurfaces, int32 eTextureFormat );
	void ProcessInputEvents();

	bool BVisiblityChanged() const { return m_bVisibleThisFrame != m_bVisibleLastFrame; }

	void PushOverlayRenderCmdStream( CSharedMemStream *pRenderStream, unsigned long dwPID, float flOpacity, EOverlayWindowAlignment alignment );

	void SetGameWindowSize( uint32 nWidth, uint32 nHeight );
	void SetFixedSurfaceSize( uint32 unSurfaceWidth, uint32 unSurfaceHeight );

	void OnMouseMove( float x, float y );

	void SetFocus( bool bFocus ); 

	void SetLetterboxColor( Color c );

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName );
#endif
protected:
	virtual void Shutdown();

private:

	COverlayInterface *m_pOverlayInterface;
	IUI3DSurface *m_p3DSurface;
	bool m_bMouseOverWindow;
	AppId_t m_nAppId;

	bool m_bInputEnabled;
	bool m_bFocus;
	bool m_bCanShareSurfaces;

	bool m_bVisibleThisFrame;
	bool m_bVisibleLastFrame;

	uint32 m_unGameWidth;
	uint32 m_unGameHeight;

	bool m_bFullScreen;
	
	bool m_bVisible;
};

class COverlayInterface : public IUIOverlayWindow
{
public:
	COverlayInterface( CTopLevelWindowOverlay *pWindow )
	{
		m_pWindow = pWindow;
	}

	virtual void PushOverlayRenderCmdStream( CSharedMemStream *pRenderStream, unsigned long dwPID, float flOpacity, EOverlayWindowAlignment alignment ) OVERRIDE
	{
		return m_pWindow->PushOverlayRenderCmdStream( pRenderStream, dwPID, flOpacity, alignment );
	}

	virtual void SetFocus( bool bFocus ) OVERRIDE
	{
		return m_pWindow->SetFocus( bFocus );
	}

	virtual bool SetGameProcessInfo( AppId_t nAppId, bool bCanSharedSurfaces, int32 eTextureFormat ) OVERRIDE
	{
		return m_pWindow->SetGameProcessInfo( nAppId, bCanSharedSurfaces, eTextureFormat );
	}

	virtual void SetInputEnabled( bool bEnabled ) OVERRIDE
	{
		return m_pWindow->SetInputEnabled( bEnabled );
	}

	virtual void SetGameWindowSize( uint32 nWidth, uint32 nHeight ) OVERRIDE
	{
		return m_pWindow->SetGameWindowSize( nWidth, nHeight );
	}

	virtual void SetFixedSurfaceSize( uint32 unSurfaceWidth, uint32 unSurfaceHeight ) OVERRIDE
	{
		return m_pWindow->SetFixedSurfaceSize( unSurfaceWidth, unSurfaceHeight );
	}

	virtual void OnMouseMove( float x, float y ) OVERRIDE
	{
		return m_pWindow->OnMouseMove( x, y );
	}

	virtual void OnMouseEnter() OVERRIDE
	{
		return m_pWindow->OnMouseEnter();
	}

	virtual void SetLetterboxColor( Color c ) OVERRIDE
	{
		return m_pWindow->SetLetterboxColor( c );
	}
private:
	CTopLevelWindowOverlay *m_pWindow;
};


} // namespace panorama

#endif // UITOPLEVELWINDOWOVERLAY_H
