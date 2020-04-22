//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef IUIWINDOW_H
#define IUIWINDOW_H

#ifdef _WIN32
#pragma once
#endif


#include "panoramatypes.h"
#include "iuirenderengine.h"

#if defined( SOURCE2_PANORAMA )
#include "rendersystem/irendercontext.h"
#include "scenesystem/isceneview.h"
#endif

class CSharedMemStream;

namespace panorama
{

class CLayoutFile;
class IUIPanelStyle;
class IUIOverlayWindow;
class IUIWindowInput;

//-----------------------------------------------------------------------------
// Purpose: Basic window interface exposing operations used inside of panorama, rather
// than operations that are part of building/laying out controls in the panorama_client module
//-----------------------------------------------------------------------------
class IUIWindow
{
public:

#if defined( SOURCE2_PANORAMA )
	virtual void RenderWindow( ISceneView *pView, IRenderContext *pRenderContext, ISceneLayer *pLayer ) = 0;
#endif

	// Delete the window object
	virtual void Delete() = 0;

	// Access input system for the window
	virtual IUIWindowInput *UIWindowInput() = 0;

	// Access the rendering interface you use to draw onto this window
	virtual IUIRenderEngine * UIRenderEngine() = 0;

	// Set scaling factor that applies to all x/y values in the UI for the window, used so we can 
	// author content at say 1080p but pass 0.6666666f for this to render in 720p on cards with poor 
	// fill rates or TVs without 1080p support.
	virtual void SetUIScaleFactor( float flScaleFactor ) = 0;
	virtual float GetUIScaleFactor() = 0;

	// Check if the mouse cursor is currently visible within the window
	virtual bool BCursorVisible() = 0;

	// Get the surface width for the window, this may be larger or smaller than the actual window if we are rendering lower res and scaling output for instance
	virtual uint32 GetSurfaceWidth() = 0;

	// Get the surface height for the window, this may be larger or smaller than the actual window if we are rendering lower res and scaling output for instance
	virtual uint32 GetSurfaceHeight() = 0;

	// Get the actual window width
	virtual uint32 GetWindowWidth() = 0;

	// Get the actual window height
	virtual uint32 GetWindowHeight() = 0;

	// Get the client area of the window, minus window borders, etc
	virtual void GetClientDimensions( float &width, float &height ) = 0;

	// Handle resize event, window has resized, backing surface needs to now resize to match
	virtual void OnWindowResize( uint32 width, uint32 height ) = 0;

	// Set window position on screen
	virtual void SetWindowPosition( float x, float y ) = 0;

	// Get window position on screen
	virtual void GetWindowPosition( float &x, float &y ) = 0;

	// bForceful in this call means try to bypass OS rules that may prevent focus (like our app wasn't last to receive input), 
	// use this sparingly and only if we really know stealing focus from some other app is what the user definately desires!
	virtual void Activate( bool bForceful ) = 0;

	// Check if this window has focus currently
	virtual bool BHasFocus() = 0;

	// Get the number of visible top level panels in the window
	virtual uint32 GetNumVisibleTopLevelPanels() const = 0;

	// Access list of visible panels
	virtual const CUtlLinkedList< IUIPanel* > &GetTopLevelVisiblePanels() const = 0;

	// Check if mouse is over the window
	virtual bool IsMouseOver() = 0;

	// Helper for letting client code tell us about daisywheel usage
	virtual void RecordDaisyWheelUsage( float flEntryTimeInSeconds, int nWordsEntered, bool bViaKeyboard, bool bViaGamepad ) = 0;

	// Helper for tracking daisywheel usage
	virtual void GetDaisyWheelWPM( int &nWordsTyped, float &flMixedWPM, float &flKeyboardOnlyWPM, float &flGamepadOnlyWPM ) = 0;
	
	// Check if the window is inside it's own layout pass
	virtual bool BIsWindowInLayoutPass() = 0;

	// Set a context ptr that is attached to the window, just lets other code (panels) that
	// has access to the window access some shared state across the window.
	virtual void SetContextPtr( void *pv ) = 0;

	// Get context ptr value for this window,
	virtual void * GetContextPtr() const = 0;

	virtual bool BIsOverlay() = 0;
	virtual bool BIsSteamWMOverlay() = 0;

	// Get low level native window handle, used by streaming, should generally not be something you use
	virtual void* GetNativeWindowHandle() = 0;

	// Add a class to every top level panel in the window
	virtual void AddClass( const char *pchName ) = 0;

	// Add a class from every top level panel in the window
	virtual void RemoveClass( const char *pchName ) = 0;

	virtual void SetInhibitInput( bool bInhibitInput ) = 0;
	virtual void SetPreventForceWindowOnTop( bool bPreventForceTopLevel ) = 0;

	// Fade out the mouse cursor immediately in this window
	virtual void FadeOutCursorNow() = 0;

	// Check if the cursor is currently fading out
	virtual bool BCursorFadingOut() = 0;

	// Wakeup and show the mouse cursor immediately
	virtual void WakeupMouseCursor() = 0;

	// Set a min FPS for the window, this actually just prevents setting the max lower
	virtual void SetMinFPS( float flMinFPS ) = 0;

	// Stats tracking
	virtual void GetSessionFPSAverages( float &fpsPaint, float &fpsAnimation, float &fpsRender ) = 0;

	// Stats tracking
	virtual void GetNumPeriodsBelowMinFPS( int &nSlowPeriods ) = 0;

	// access data about how the gamepad was used
	virtual bool BWasGamepadConnectedThisSession() = 0;
	virtual bool BWasGamepadUsedThisSession() = 0;

	virtual bool BWasSteamControllerConnectedThisSession() = 0;
	virtual bool BWasSteamControllerUsedThisSession() = 0;

	// Access overlay window interface for this window, NULL on non Steam Overlay windows
	virtual IUIOverlayWindow *GetOverlayInterface() = 0;

	// Change visibility of this window
	virtual bool BIsVisible() = 0;
	virtual void SetVisible( bool bVisible ) = 0;

	// Change the focus behavior for controls in this window
	virtual EWindowFocusBehavior GetFocusBehavior() = 0;
	virtual void SetFocusBehavior( EWindowFocusBehavior eFocusBehavior ) = 0;

	virtual void OnDeviceLost() = 0;
	virtual void OnDeviceRestored() = 0;
	virtual bool BDeviceLost() = 0;

	// Clears the GPU resources associated with the window before the next render frame
	virtual void ClearGPUResourcesBeforeNextFrame() = 0;
};


//
// Overlay window interface for Steam overlay specific windows
//
class IUIOverlayWindow
{
public:
#if !defined( SOURCE2_PANORAMA )
	virtual void PushOverlayRenderCmdStream( CSharedMemStream *pRenderStream, unsigned long dwPID, float flOpacity, EOverlayWindowAlignment alignment ) = 0;

	virtual void SetFocus( bool bFocus ) = 0;

	virtual bool SetGameProcessInfo( AppId_t nAppId, bool bCanSharedSurfaces, int32 eTextureFormat ) = 0;

	virtual void SetInputEnabled( bool bEnabled ) = 0;

	virtual void SetGameWindowSize( uint32 nWidth, uint32 nHeight ) = 0;

	virtual void SetFixedSurfaceSize( uint32 unSurfaceWidth, uint32 unSurfaceHeight ) = 0;

	virtual void OnMouseMove( float x, float y ) = 0;

	virtual void OnMouseEnter() = 0;

	virtual void SetLetterboxColor( Color c ) = 0;
#endif
};
}
#endif // IUIWINDOW_H
