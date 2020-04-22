//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef UITOPLEVELWINDOW_H
#define UITOPLEVELWINDOW_H

#ifdef _WIN32
#pragma once
#endif

#include "utlstring.h"
#include "utlmap.h"
#include "utllinkedlist.h"
#if !defined( SOURCE2_PANORAMA )
#include "constants.h"
#include "globals.h"
#endif
#include "reliabletimer.h"
#if !defined( SOURCE2_PANORAMA )
#include "framefunction.h"
#endif
#include "input/iuiinput.h"
#include "input/mousecursors.h"
#include "iuiengine.h"
#include "color.h"
#include "uievent.h"
#include "iuiwindow.h"
#include "iuirenderengine.h"
#include "uipanel.h"

namespace panorama
{

class CUIRenderEngine;
class CPanel2D;
class CUIWindowInput;
class CUIEngine;
class CMouseCursorTexture;
class CImageResourceManager;
class CFastScrollSoundManager;
class IUI3DSurface;
class CMouseCursorRender;


//
// Top level window class
//
class CTopLevelWindow : public panorama::IUIWindow
{
public:
	CTopLevelWindow( CUIEngine *pUIEngineParent );
	virtual ~CTopLevelWindow();

	// Delete the window object
	virtual void Delete() OVERRIDE{ delete this; }

	// Final step of initialization, post constructor, and post BInitializeSurface() on individidual window type
	virtual bool FinishInitialization();

	// Run any per window frame func logic
	virtual void RunPlatformFrame();
	
	// Set scaling factor that applies to all x/y values in the UI for the window, used so we can 
	// author content at say 1080p but pass 0.6666666f for this to render in 720p on cards with poor 
	// fill rates or TVs without 1080p support.
	virtual void SetUIScaleFactor( float flScaleFactor ) OVERRIDE;
	virtual float GetUIScaleFactor() OVERRIDE { return m_flScaleFactor; }

	// Window position/activation management
	const char * GetTargetMonitor() { return m_strTargetMonitor.String(); }
	virtual void GetWindowBounds( float &left, float &top, float &right, float &bottom ) = 0;
	virtual void GetClientDimensions( float &width, float &height ) OVERRIDE = 0;

	virtual bool BAllowInput( InputMessage_t &msg );

	// Access the rendering interface you use to draw onto this window
	virtual IUIRenderEngine * UIRenderEngine() OVERRIDE{ return (IUIRenderEngine*)m_pRenderEngine; }
	CUIRenderEngine *GetUIRenderEngine() { return m_pRenderEngine; }
	virtual bool BIsVisible() { return true; }
	virtual bool BIsOverlay() OVERRIDE { return UIEngine()->BIsOverlayTarget(m_eRenderTarget); }
	virtual bool BIsSteamWMOverlay() OVERRIDE { return m_eRenderTarget == IUIEngine::k_ERenderToOverlaySteamWM; }

	virtual bool BIsFullscreen() { return IUIEngine::BIsRenderingToFullScreen( m_eRenderTarget ); }
	virtual bool BIsFullscreenBorderlessWindow() { return m_eRenderTarget == IUIEngine::k_ERenderBorderlessFullScreenWindow; }
	bool SetFullscreen( bool bFullscreen );
	bool BEnforceWindowAspectRatio() { return m_bEnforceWindowAspectRatio; }
	virtual uint32 GetSurfaceWidth() OVERRIDE { return m_unSurfaceWidth; }
	virtual uint32 GetSurfaceHeight() OVERRIDE { return m_unSurfaceHeight; }
	virtual uint32 GetWindowWidth() OVERRIDE { return m_unWindowWidth; }
	virtual uint32 GetWindowHeight() OVERRIDE { return m_unWindowHeight; }
	void ConvertClientToSurfaceCoord( float *px, float *py );
	void GetFPSAverages( float &fpsPaint, float &fpsAnimation, float &fpsRender );
	virtual void GetSessionFPSAverages( float &fpsPaint, float &fpsAnimation, float &fpsRender ) OVERRIDE;
	virtual void GetNumPeriodsBelowMinFPS( int &nSlowPeriods ) OVERRIDE;

	// Clear color for the window, normally black, transparent for overlay
	virtual Color GetClearColor() { return Color( 0, 0, 0, 255 ); }

	// Panel management for the window
	int AddPanel( CUIPanel *pPanel, bool bVisible );
	void RemovePanel( int iPanelIndex, bool bVisible );
	int SetPanelVisible( int iPanelIndex, bool bVisible );
	virtual void AddClass( const char *pchName ) OVERRIDE;
	virtual void RemoveClass( const char *pchName ) OVERRIDE;

	// Layout/paint for window
	virtual void LayoutAndPaintIfNeeded();

	// Paint an empty frame so the animation/render threads will run but do nothing but LRU/clear data
	virtual void PaintEmptyFrameAndForceLaterRepaint();

	// Layout file auto-reload for windows children panels
	void ReloadLayoutFile( CPanoramaSymbol symPath );
	void OnReloadStyleFile( CPanoramaSymbol symPath );

	// Access input engine for window
	virtual IUIWindowInput *UIWindowInput() OVERRIDE { return (IUIWindowInput *)m_pInputEngine; }

	// custom mouse cursor support, returns true if we want our manually drawn one, false for OS specific ones
	bool BUseCustomMouseCursor() { return m_bUseCustomMouseCursor; }
	// used by the os specific case to update the cursor
	virtual void SetMouseCursor( EMouseCursors eCursor ) = 0;
	IImageSource *GetMouseCursorTexture( Vector2D *pptHotspot );

	virtual bool BCursorVisible() OVERRIDE;
	virtual bool BCursorFadingOut() OVERRIDE;
	virtual void WakeupMouseCursor() OVERRIDE;
	virtual void FadeOutCursorNow() OVERRIDE;
	
	// Access image manager for window
	CImageResourceManager* AccessImageManager() { return m_pImageResourceManager; }

	// Set a context ptr that is attached to the window, just lets other code (panels) that
	// has access to the window access some shared state across the window.
	virtual void SetContextPtr( void *pv ) OVERRIDE { m_pContextPtr = pv; }

	// Get the context ptr that is attached to the window
	virtual void * GetContextPtr() const OVERRIDE { return m_pContextPtr; }

	void ReloadChangedFile( const char *pchFile );
	
	// Access fast scroll sound manager for the window
	CFastScrollSoundManager * AccessFastScrollSoundMgr();

	void GetMouseWheelRepeats( bool bScrollUp, int lines, uint8 &unRepeats );

	virtual uint32 GetNumVisibleTopLevelPanels() const OVERRIDE { return (uint32)m_listVisiblePanels.Count(); }

	virtual const CUtlLinkedList< IUIPanel* > &GetTopLevelVisiblePanels() const OVERRIDE { return (CUtlLinkedList< IUIPanel* > &)m_listVisiblePanels; }

	// Get the last time the window layed out and painted
	float GetLastLayoutAndPaintTime() { return m_flLastLayoutAndPaintTime; }

	// Set max FPS for the window
	void SetMaxFPS( float flMaxFPS );

	// Set a min FPS for the window, this actually just prevents setting the max lower
	virtual void SetMinFPS( float flMinFPS ) OVERRIDE { m_flMinFPS = flMinFPS; }

	// access data about how the gamepad was used
	virtual bool BWasGamepadConnectedThisSession() OVERRIDE;
	virtual bool BWasGamepadUsedThisSession() OVERRIDE;

	virtual bool BWasSteamControllerConnectedThisSession() OVERRIDE;
	virtual bool BWasSteamControllerUsedThisSession() OVERRIDE;

	// metrics
	virtual void RecordDaisyWheelUsage( float flEntryTimeInSeconds, int nWordsEntered, bool bViaKeyboard, bool bViaGamepad ) OVERRIDE;
	virtual void GetDaisyWheelWPM( int &nWordsTyped, float &flMixedWPM, float &flKeyboardOnlyWPM, float &flGamepadOnlyWPM ) OVERRIDE;

	bool BFinishedInitialization() { return m_bFinishedInitialization; }

	virtual bool BIsWindowInLayoutPass() OVERRIDE { return m_bInLayoutTraverse; }

	virtual void SetInhibitInput( bool bInhibitInput ) OVERRIDE;
	virtual void SetPreventForceWindowOnTop( bool bPreventForceTopLevel ) OVERRIDE;

	// Access overlay window interface for this window, NULL on non Steam Overlay windows
	virtual IUIOverlayWindow *GetOverlayInterface() OVERRIDE { return NULL; }

	virtual void SetFocusBehavior( EWindowFocusBehavior eFocusBehavior ) OVERRIDE { m_eFocusBehavior = eFocusBehavior; }
	virtual EWindowFocusBehavior GetFocusBehavior() OVERRIDE{ return m_eFocusBehavior; }

	virtual void OnDeviceLost() OVERRIDE;
	virtual void OnDeviceRestored() OVERRIDE;
	virtual bool BDeviceLost() OVERRIDE;

	// Clears the GPU resources associated with the window before the next render frame
	virtual void ClearGPUResourcesBeforeNextFrame() OVERRIDE;

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName );

	bool PrepareForValidate();
	bool ResumeFromValidate();
#endif
protected:

	// Perform layout prior to paint
	void PerformLayout();
	bool BIsGuideButton( const InputMessage_t &msg );

	// Render engine instance for window
	CUIRenderEngine *m_pRenderEngine;

	// Do we need to clear gpu resources before repaint
	uint32 m_unFramesToClearGPUResourcesBeforeRepaint;

	// Image manager for the window
	CImageResourceManager *m_pImageResourceManager;

	bool m_bDeviceLost;
	bool m_bAlreadyForcedRepaintAllSinceLastPaint;
	CUtlString m_strTargetMonitor;
	uint32 m_unSurfaceWidth, m_unSurfaceHeight;
	uint32 m_unWindowWidth, m_unWindowHeight;
	IUIEngine::ERenderTarget m_eRenderTarget;
	bool m_bFixedSurfaceSize;
	bool m_bEnforceWindowAspectRatio;
	bool m_bUseCustomMouseCursor; // true if we draw our tenfoot cursors and not the win32 ones for this panel
	bool m_bCursorWasVisibleLastFrame;
	EMouseCursors m_eCursorCurrent; // current cursor to draw
	CMouseCursorTexture *m_pMouseCursor; // contains the image data for the cursors we can display

	// owner of details about the cursor state
	CMouseCursorRender *m_pCursorRender;

	// Input engine
	CUIWindowInput *m_pInputEngine;

	// the ui engine that owns us
	CUIEngine *m_pUIEngineParent;
	void *m_pContextPtr;

	// Scale factor for all drawing sizes
	float m_flScaleFactor;

	// Min fps value the max fps can be set to for this window
	float m_flMinFPS;

	// Track last layout and paint time
	double m_flLastLayoutAndPaintTime;

	// Pointer for the surface interface created for the window
	IUI3DSurface *m_pSurfaceInterface;

	// Panel lists
	CUtlLinkedList< CUIPanel * > m_listVisiblePanels;
	CUtlLinkedList< CUIPanel * > m_listInvisiblePanels;

	// Classes to apply to top level panels
	CUtlVector< CPanoramaSymbol > m_vecStyleClasses;

	// Panels to asynchronously add classes to
	CUtlVector< CPanelPtr< IUIPanel > > m_vecPanelsAddClasses;

	// Panels to asynchronously remove classes from
	CUtlVector< CPanelPtr< IUIPanel > > m_vecPanelsRemoveClasses;

	CFastScrollSoundManager *m_pFastScrollSoundManager;

	// Mouse wheel repeat tracking
	uint8 m_unMouseWheelUpRepeats;
	uint8 m_unMouseWheelDownRepeats;
	double m_flLastMouseWheelUp;
	double m_flLastMouseWheelDown;

	bool m_bInLayoutTraverse;
	bool m_bInPaintTraverse;
	bool m_bFinishedInitialization;

	// daisy wheel usage for this window
	enum EDaisyWheelInputType
	{
		eDaisyWheelInputType_KeyboardOnly,
		eDaisyWheelInputType_GamepadOnly,
		eDaisyWheelInputType_KeyboardAndGamePad,

		eDaisyWheelInputType_MAX
	};
	struct DaisyWheelUsage_t
	{
		int nWords;
		float flTime;
	};
	DaisyWheelUsage_t m_flDaisyWheelWPM[eDaisyWheelInputType_MAX];

	virtual void Shutdown();

	bool m_bInhibitInput;

	EWindowFocusBehavior m_eFocusBehavior;
};

} // namespace panorama

#endif // UITOPLEVELWINDOW_H
