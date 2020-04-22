//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef UITOPLEVELWINDOWOPENVROVERLAY_H
#define UITOPLEVELWINDOWOPENVROVERLAY_H

#ifdef _WIN32
#pragma once
#endif

#include <openvr.h>

namespace panorama
{
class CTopLevelWindowOverlay;


class CTopLevelWindowOpenVROverlay : public CTopLevelWindow
{
	typedef CTopLevelWindow BaseClass;
public:
	CTopLevelWindowOpenVROverlay( CUIEngine *pUIEngineParent );
	virtual ~CTopLevelWindowOpenVROverlay();

	// Initialize backing surface for window
	virtual bool BInitializeSurface( int nWidth, int nHeight, vr::VROverlayHandle_t ulOverlayHandle );

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

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName );
#endif
protected:
	virtual void Shutdown();

private:

	IUI3DSurface *m_p3DSurface;
	bool m_bMouseOverWindow;

	bool m_bInputEnabled;
	bool m_bFocus;
	bool m_bCanShareSurfaces;

	bool m_bVisibleThisFrame;
	bool m_bVisibleLastFrame;

	uint32 m_unGameWidth;
	uint32 m_unGameHeight;

	bool m_bFullScreen;
	
	bool m_bVisible;

	vr::VROverlayHandle_t m_ulOverlayHandle;
};



} // namespace panorama

#endif // UITOPLEVELWINDOWOPENVROVERLAY_H
