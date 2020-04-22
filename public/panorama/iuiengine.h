//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef IUIENGINE_H
#define IUIENGINE_H
#pragma once

#include "tier0/platform.h"

#if defined( SOURCE2_PANORAMA )
#include "tier0/platwindow.h"
#endif

#include "tier1/convar.h"
#include "panorama.h"
#include "./iuipanel.h"
#include "utlsymbol.h"
#include "panoramatypes.h"
#include "iuilayoutmanager.h"
#include "controls/panelhandle.h"
#include "tier1/utllinkedlist.h"
#include "tier1/utlstring.h"
#include "tier1/utldelegate.h"
#include "tier0/validator.h"
#ifdef SOURCE_PANORAMA_FIXME
	#include "tier1/UtlSortVector.h"
#else
	#include "tier1/utlsortvector.h"
#endif
#include "language.h"
#include "panorama/layout/panel2dfactory.h"
#include "iuistylefactory.h"
#include "interface.h"
#include "steam/isteamhttp.h"
#include "iuifilesystem.h"

#if !defined( SOURCE2_PANORAMA )
#include "audio/iaudiointerface.h"
#include <openvr.h>
#endif

#if _GNUC
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#endif
#if defined( SOURCE2_PANORAMA )
#include "../thirdparty/v8/include/v8.h"
#else
#include "tier0/memdbgoff.h"
#include "../external/v8/include/v8.h"
#include "tier0/memdbgon.h"
#endif
#if _GNUC
#pragma GCC diagnostic pop
#endif

#ifdef SUPPORTS_AUDIO
class IAudioOutputStream;
#endif
typedef uint64 GID_t;
class KeyValues;
#if !defined( SOURCE2_PANORAMA )
class IHTMLChromeController;
#endif

typedef CUtlDelegate< void( GID_t, KeyValues *, void * ) > JSONWebAPIDelegate_t;

#if defined( SOURCE2_PANORAMA )
DECLARE_LOGGING_CHANNEL( LOG_PANORAMA );
DECLARE_LOGGING_CHANNEL( LOG_PANORAMA_SCRIPT );
#endif

namespace v8
{
	class Isolate;
}

namespace panorama
{

// forward decl for engine components
class CUIProtoBufMsgMemoryPoolMgr;
class CUIRenderEngine;
class IUIInput;
class IUIEvent;
class IUILocalization;
class IUISoundSystem;
class IUILayoutManager;
class CMovieManager;
class IUITextLayout;
class IUISettings;
class IUIWindow;
class CPanelStyle;
class IUIJSObject;

typedef void * HAUDIOSAMPLE;

class IUIEngineFrameListener
{
public:
	virtual void OnPreFrame() = 0;
	virtual void OnPostFrame() = 0;
	virtual void OnEngineShutdown() = 0;
};


class CJSONWebAPIParams
{
public:

	struct WebAPIParam_t
	{
		CUtlString strParamName;
		CUtlString strParamValue;
	};

	CJSONWebAPIParams() { }

	void AddParam( const char *pchParamName, const char *pchParamValue )
	{
		int iVec = m_vecParams.AddToTail();
		WebAPIParam_t &param = m_vecParams[iVec];
		param.strParamName = pchParamName;
		param.strParamValue = pchParamValue;
	}

	CUtlVector<WebAPIParam_t> *AccessParams() { return &m_vecParams; }

private:

	CCopyableUtlVector< WebAPIParam_t > m_vecParams;
};

typedef void (*PanoramaFrameFunc_t)();

typedef int JSGenericCallbackHandle_t;
const JSGenericCallbackHandle_t JS_GENERIC_CALLBACK_HANDLE_INVALID = -1;

struct RegisterJSScopeInfo_t
{
	const char *pName;
	const char *pDescription;
	int nEntries;
};

enum RegisterJSType_t : uint8
{
	k_ERegisterJSTypeUnknown,
	k_ERegisterJSTypeInvalid,
	k_ERegisterJSTypeVoid,
	k_ERegisterJSTypeBool,
	k_ERegisterJSTypeInt8,
	k_ERegisterJSTypeUint8,
	k_ERegisterJSTypeInt16,
	k_ERegisterJSTypeUint16,
	k_ERegisterJSTypeInt32,
	k_ERegisterJSTypeUint32,
	k_ERegisterJSTypeInt64,
	k_ERegisterJSTypeUint64,
	k_ERegisterJSTypeFloat,
	k_ERegisterJSTypeDouble,
	k_ERegisterJSTypeConstString,
	k_ERegisterJSTypePanoramaSymbol,
	k_ERegisterJSTypeRawV8Args,
};

struct RegisterJSEntryInfo_t
{
	enum
	{
		k_EGlobalFunction   = 0x00000000,
		k_EMethod           = 0x00000001,
		k_EAccessor         = 0x00000002,
		k_EAccessorReadOnly = 0x00000003,
		k_EConstantValue    = 0x00000004,
		k_EEntryTypeMask    = 0x0000000f,
	};

	const char *pName;
	const char *pDescription;
	uint32 unFlags;

	// Return or value type.
	RegisterJSType_t eDataType;

	// Prototype information may not be known.
	static const uint8 k_unNumParamsUnknown = 0xff;
	uint8 unNumParams;

	static const uint8 k_unMaxParams = 10;
	RegisterJSType_t pParamTypes[k_unMaxParams];

	uint32 GetEntryType() const
	{
		return unFlags & k_EEntryTypeMask;
	}
};

//
// Interface that needs to be implemented for game engines on all platforms
//
class IUIEngine
{
public:
	enum ERenderTarget
	{
		// these assignments are important because they are cross-cast at the protobuf layer
		k_ERenderToWindow = 1,
		k_ERenderFullScreen = 2,
		k_ERenderBorderlessFullScreenWindow = 3,
		k_ERenderToOverlayTexture = 4,
		k_ERenderToOverlaySharedTexture = 5,
		k_ERenderToOverlaySteamWM = 6,
		k_ERenderToLegacyVR = 7, // This one is used for the Steam Client's main interface and will die eventually
		k_ERenderToOpenVROverlay = 8,

		k_ERenderTargetUnset = 0,
	};

	enum EHapticFeedbackPosition
	{
		k_EHapticFeedbackPosition_Left,
		k_EHapticFeedbackPosition_Right,
	};

	enum EHapticFeedbackStrength
	{
		k_EHapticFeedbackStrength_VeryLow,
		k_EHapticFeedbackStrength_Low,
		k_EHapticFeedbackStrength_Medium,
		k_EHapticFeedbackStrength_High,
	};

	// return true if the states are different and a valid transition.  Used to test fullscreen transition
	static bool BValidRenderStateChange( ERenderTarget eCurrentRenderTarget, ERenderTarget eRequestedRenderTarget );
	static bool BIsRenderingToTexture( ERenderTarget eTarget ) { return (eTarget == k_ERenderToOverlayTexture || eTarget == k_ERenderToOverlaySharedTexture || eTarget == k_ERenderToOpenVROverlay ); }
	static bool BIsOverlayTarget( ERenderTarget eTarget )
	{
		return (eTarget == IUIEngine::k_ERenderToOverlayTexture ||
			eTarget == IUIEngine::k_ERenderToOverlaySharedTexture ||
			eTarget == IUIEngine::k_ERenderToOverlaySteamWM);
	}
	static bool BIsRenderingToLegacyVR( ERenderTarget eTarget ) { return (eTarget == k_ERenderToLegacyVR); }
	static bool BIsRenderingToOpenVROverlay( ERenderTarget eTarget ) { return (eTarget == k_ERenderToOpenVROverlay); }
	static bool BIsRenderingToFullScreen( ERenderTarget eTarget ) { return (eTarget == k_ERenderFullScreen); }

public:
	virtual ~IUIEngine() {}

	// Starts up subsystems and makes engine ready for use, you need to call RegisterNamedLocalPath() for at least the <config> path first!
#if defined( SOURCE2_PANORAMA )
	virtual bool StartupSubsystems( IUISettings *psettings, PlatWindow_t hWindow ) = 0;
#else
	virtual bool StartupSubsystems( IUISettings *psettings, IHTMLChromeController *pHTMLController ) = 0;
#endif

	// Hookup convars
	virtual void ConCommandInit( IConCommandBaseAccessor *pAccessor ) = 0;

	// Shutdown
	virtual void Shutdown() = 0;
	virtual void RequestShutdown() = 0;

	// Run will run frames and block, letting the UI engine control the whole frame loop.
	virtual void Run() = 0;

	// Run frame will run a single frame, letting you wrap the UI engine in an external frame loop
	virtual void RunFrame() = 0;

	// Will set the UI engine to aggressively limit frame rate it runs at to avoid resource usage
	virtual void SetAggressiveFrameRateLimit( bool bLimit ) = 0;

	virtual bool BIsRunning() = 0;
	virtual bool BHasFocus() = 0;
	virtual double GetCurrentFrameTime() = 0;

#if !defined( SOURCE2_PANORAMA )
	virtual IUIWindow *CreateNewWindow( const char *pchWindowTitle, uint32 width, uint32 height, ERenderTarget eRenderType, bool bFixedSurfaceSize, bool bEnforceWindowAspectRatio, bool bUseCustomMouseCursor, const char *pchMonitorName ) = 0;
	virtual IUIWindow *CreateNewOverlayWindow( const char *pchWindowTitle, uint32 width, uint32 height, panorama::IUIEngine::ERenderTarget eTarget, bool bFixedSize, bool bDrawCustomMouseCursor ) = 0;
	virtual IUIWindow *CreateNewOpenVROverlayWindow( uint32 width, uint32 height, vr::VROverlayHandle_t ulOverlayHandle ) = 0;
#else
	virtual IUIWindow *CreateNewUILayerWindow( uint32 xPos, uint32 yPos, uint32 width, uint32 height, bool bFixedSurfaceSize, bool bEnforceWindowAspectRatio, bool bUseCustomMouseCursor, bool bAcceptKBandMouse, const char *pName ) = 0;
#endif

	virtual IUITextLayout *CreateTextLayout( const char *pchText, const char *pchFontName, float flSize, float flLineHeight, EFontWeight weight, EFontStyle style, ETextAlign align, bool bWrap, bool bEllipsis, int nLetterSpacing, float flMaxWidth, float flMaxHeight ) = 0;
	virtual IUITextLayout *CreateTextLayout( const wchar_t *pwchText, const char *pchFontName, float flSize, float flLineHeight, EFontWeight weight, EFontStyle style, ETextAlign align, bool bWrap, bool bEllipsis, int nLetterSpacing, float flMaxWidth, float flMaxHeight ) = 0;
	virtual void FreeTextLayout( IUITextLayout *pLayout ) = 0;
	virtual const CUtlSortVector< CUtlString > &GetSortedValidFontNames() = 0;


	virtual IUIInput *UIInputEngine() = 0;
	virtual IUILocalization *UILocalize() = 0;
	virtual IUISoundSystem *UISoundSystem() = 0;
	virtual IUISettings *UISettings() = 0;
	virtual IUILayoutManager *UILayoutManager() = 0;
	virtual IUIFileSystem *UIFileSystem() = 0;

	virtual void RegisterFrameFunc( PanoramaFrameFunc_t frameFunc ) = 0;

	virtual void ReloadLayoutFile( CPanoramaSymbol symPath ) = 0;
	virtual void ToggleDebugMode() = 0;
	virtual CUtlLinkedList<CUtlString> &GetConsoleHistory() = 0;

	// panel management
	virtual IUIPanel * CreatePanel() = 0;
	virtual void PanelDestroyed( IUIPanel *pPanel, IUIPanel *pOldParent ) = 0;
	virtual bool IsValidPanelPointer( const IUIPanel *pPanel ) = 0;
	virtual PanelHandle_t GetPanelHandle( const IUIPanel *pPanel ) = 0;
	virtual IUIPanel *GetPanelPtr( const PanelHandle_t &handle ) = 0;
	virtual void CallBeforeStyleAndLayout( IUIPanel *pPanel ) = 0;

	// registration for panel destroyed
	typedef CUtlDelegate< void( const panorama::IUIPanel *, IUIPanel *pOldParent ) > PanelDestroyedDel_t;
	virtual void RegisterForPanelDestroyed( PanelDestroyedDel_t del ) = 0;
	virtual void UnregisterForPanelDestroyed( PanelDestroyedDel_t del ) = 0;

	// Event management
	virtual void RegisterEventHandler( CPanoramaSymbol symMsg, IUIPanel *pPanel, CUtlAbstractDelegate pFunc ) = 0;
	virtual void UnregisterEventHandler( CPanoramaSymbol symMsg, IUIPanel *pPanel, CUtlAbstractDelegate pFunc ) = 0;
	virtual void RegisterEventHandler( CPanoramaSymbol symMsg, IUIPanelClient *pPanel, CUtlAbstractDelegate pFunc ) = 0;
	virtual void UnregisterEventHandler( CPanoramaSymbol symMsg, IUIPanelClient *pPanel, CUtlAbstractDelegate pFunc ) = 0;
	virtual void UnregisterEventHandlersForPanel( IUIPanel *pPanel ) = 0;
	virtual void RegisterForUnhandledEvent( CPanoramaSymbol symMsg, CUtlAbstractDelegate pFunc ) = 0;
	virtual void UnregisterForUnhandledEvent( CPanoramaSymbol symMsg, CUtlAbstractDelegate pFunc ) = 0;
	virtual bool BHaveEventHandlersRegisteredForType( CPanoramaSymbol symPanelType ) = 0;
	virtual void RegisterPanelTypeEventHandler( CPanoramaSymbol symMsg, CPanoramaSymbol symPanelType, CUtlAbstractDelegate pFunc, bool bThisPtrIsUIPanel = false ) = 0;
	virtual bool DispatchEvent( IUIEvent *pEvent ) = 0;
	virtual void DispatchEventAsync( float flDelay, IUIEvent *pEvent ) = 0;
	virtual bool BAnyHandlerRegisteredForEvent( const CPanoramaSymbol &symEvent ) = 0;
	virtual CPanoramaSymbol GetLastDispatchedEventSymbol() = 0;
	virtual IUIPanel *GetLastDispatchedEventTargetPanel() = 0;

	// Event filtering
	virtual void RegisterEventFilter( CUtlAbstractDelegate pFunc ) = 0;
	virtual void UnregisterEventFilter( CUtlAbstractDelegate pFunc ) = 0;

	// Repaints all windows immediately, queuing for animation/render threads
	virtual void LayoutAndPaintWindows() = 0;

	// Get install path for the application
	virtual const char *GetApplicationInstallPath() = 0;

	// Get the userdata path for the application
	virtual const char *GetApplicationUserDataPath() = 0;

	// Register a named local path (executable-relative, readonly on POSIX) for resources, should be something like {images} or {movies}
	virtual void RegisterNamedLocalPath( const char *pathName, const char *pchLocalPath, bool bWatchForFileChanges, bool bAddToOverwriteIfExists = false ) = 0;

	// Register a named userdata path (userdata-relative, writable on POSIX) for local read/write storage like {config}
	virtual void RegisterNamedUserPath( const char *pathName, const char *pchUserPath, bool bWatchForFileChanges, bool bAddToOverwriteIfExists = false ) = 0;

	// Register the path to load custom vfont files from
	virtual void RegisterCustomFontPath( const char *pchFontPath ) = 0;

	// Get the local path for a named path like {images} or {movies}
	virtual const char *GetLocalPathForNamedPath( const char *pathName ) = 0;

	// Get the local path for a named path like {images} or {movies} combined with the relative pathname
	// and also following up the possible overwritten paths
	virtual void GetLocalPathForRelativePath( const char *pchLocalPathName, const char *pchRelativePathname, CUtlString &strLocalPath ) = 0;

	// Register for a named hostname in http:// paths like {steamcommunity}
	virtual void RegisterNamedRemoteHost( const char *hostName, const char*pchRemoteHost ) = 0;

	// Get the named host for a named remote host like {steamcommunity}
	virtual const char *GetRemoteHostForNamedHost( const char *hostName ) = 0;

	// Register an additional X-Header to be sent when requesting layout from the web
	virtual void RegisterXHeader( const char *pchHeaderName, const char *pchHeaderValue ) = 0;
	
	// Get the current count of registers X-Headers
	virtual int GetXHeaderCount() const = 0;
	
	// Get the name and value for the given indexed X-Header
	virtual void GetXHeader( int i, CUtlString &strName, CUtlString &strValue ) const = 0;

	// Set the cookie header to use for the named remote host
	virtual void SetCookieHeaderForNamedRemoteHost( const char *hostName, const CUtlVector<CUtlString> &vecCookies ) = 0;

	// Sets the cookie header to use for the remote host (doesn't require named host use)
	virtual void SetCookieHeaderForRemoteHost( const char *hostName, const CUtlVector<CUtlString> &vecCookies ) = 0;

	// Get the cookie header string that should be used for the remote host
	virtual const CUtlVector<CUtlString> &GetCookieHeadersForNamedRemoteHost( const char *namedRemoteHost ) = 0;

	// Get the cookie header string that should be used for the remote host
	virtual const CUtlVector<CUtlString> &GetCookieHeadersForRemoteHost( const char *hostName ) = 0;

	// Gets the value of a cookie. Returns false if the cookie does not exist.
	virtual bool GetCookieValueForRemoteHost( const char *hostName, const char *cookieName, CUtlString *pstrCookieValue ) = 0;

#if defined( SOURCE2_PANORAMA ) || defined( PANORAMA_PUBLIC_STEAM_SDK )
	virtual ISteamHTMLSurface *AccessHTMLController() = 0;
#else
	virtual IHTMLChromeController *AccessHTMLController() = 0;
#endif

	// native message box. Used usually for development.
	enum ENativeMessageBoxType_t
	{
		k_ENativeMessageOk = 1,
		k_ENativeMessageYesNo = 2
	};
	virtual bool ShowNativeTopMostMessageBox( const char *pchMsg, const char *pchTitle, ENativeMessageBoxType_t eType ) = 0;

	// Add a listener callback for pre/post frame events 
	virtual void AddFrameListener( IUIEngineFrameListener *pListener ) = 0;

	// Remove a listener callback for pre/post frame events 
	virtual void RemoveFrameListener( IUIEngineFrameListener *pListener ) = 0;

	// storage for mouse can activate info
	virtual void RegisterMouseCanActivateParent( IUIPanel *pPanel, const char *pchParent ) = 0;
	virtual void UnregisterMouseCanActivateParent( IUIPanel *pPanel ) = 0;
	virtual const char *GetMouseCanActivateParent( IUIPanel *pPanel ) = 0;

	// See if any window owned by the ui engine has focus
	virtual bool BAnyWindowHasFocus() = 0;

	// See if any window owned by the ui engine is visible and has focus, sometimes overlay windows have focus but are not visible
	// and you generally don't want to consider them as focused then
	virtual bool BAnyVisibleWindowHasFocus() = 0;

	// See if any overlay window owned by the ui engine has focus
	virtual bool BAnyOverlayWindowHasFocus() = 0;

	// Get the focused window, there should really be only one, if some bug allows multiple the first found is returned
	virtual IUIWindow *GetFocusedWindow() = 0;

	// Get the last time any input event happened across any of our windows
	virtual double GetLastInputTime() = 0;

	// Called internally from input layer to update last input time to current frame
	virtual void UpdateLastInputTime() = 0;

	// Clipboard access
	virtual void CopyToClipboard( const char *pchTextUTF8 ) = 0;
	virtual void GetClipboardText( CUtlString &strUTF8 ) = 0;

	// Input locale support
	virtual ELanguage GetDisplayLanguage() = 0;
	virtual ELanguage GetCurrentInputLocale() = 0;
	virtual bool BHaveInputLocale( ELanguage language ) = 0;
	virtual void SetInputLocale( ELanguage language ) = 0;

	// Overlay tracking
	virtual bool BHasOverlayForApp( uint64 gameID, uint64 ulPID ) = 0;
	virtual void TrackOverlayForApp( uint64 gameID, uint64 ulPID, void * pOverlay ) = 0;
	virtual void DeleteOverlayInstanceForApp( uint64 gameID, uint64 dwPID, void * pOverlay ) = 0;
	virtual void *OverlayForApp( uint64 gameID, uint64 ulPID ) = 0;

	// GPU Information
	virtual bool BGetGPUInformation( char *rgchGPUDesc, uint32 unGPUDescBytes, uint64 *pulDedicatedGPUMem, uint64 *pulDedicatedSystemMem, uint64 *pulSharedMem ) = 0;

	// Pool allocations for panel styles
	virtual IUIPanelStyle *AllocPanelStyle( IUIPanel *pStyle, float flUIScaleFactor ) = 0;
	virtual void FreePanelStyle( IUIPanelStyle *pPanel ) = 0;

	virtual void SetPanelWaitingAsyncDelete( IUIPanel *pPanel ) = 0;
	virtual bool BIsPanelWaitingAsyncDelete( IUIPanel *pPanel ) = 0;

	virtual void PulseActiveControllerHaptic( EHapticFeedbackPosition ePosition, EHapticFeedbackStrength eStrength ) = 0;

	virtual void MarkLayerToRepaintThreadSafe( uint64 ulCompositionLayerID ) = 0;

	virtual void AddDirectoryChangeWatch( const char *pchPath ) = 0;

	// Get how many lines to scroll for mouse wheel
	virtual uint32 GetWheelScrollLines() = 0;

	// Execute some javascript in the given panel context
	virtual void RunScript( IUIPanel *pPanelContext, const char *pchScriptString, const char *pchSourceFilename, int nSourceBeginLine, int nSourceBeginCol, bool bPrintRetValue ) = 0;

	// Expose a new object type/template to javascript with the given name, 
	// the function pointer passed should setup member accssors/methods with the functions
	// from uijsregistration.h
	virtual void ExposeObjectTypeToJavaScript( const char *pchObjectTypeName, CUtlAbstractDelegate &del ) = 0;

	// Expose an instance of an object type as a global with specified name to javascript
	virtual void ExposeGlobalObjectToJavaScript( const char *pchJSVarName, void *pInstance, const char *pchJsTypeName, bool bTrueGlobal = false ) = 0;

	virtual void ClearGlobalObjectForJavaScript( const char *pchJSVarName, void *pInstance ) = 0;

	virtual void DeleteJSObjectInstance( IUIJSObject *pInstance ) = 0;

	// Get panel that contains the javascript context
	virtual panorama::IUIPanel *GetPanelForJavaScriptContext( v8::Context *pContext ) = 0;

	// Get javascript context for a panel (may be NULL)
	virtual v8::Persistent<v8::Context> *GetJavaScriptContextForPanel( panorama::IUIPanel *pPanel ) = 0;

	// Helper to spew exceptions to console
	virtual void OutputJSExceptionToConsole( v8::TryCatch &try_catch, IUIPanel *pPanelContext ) = 0;

	// Add a function template to global namespace, by default this is really panorama., but you can specify to make it really global as well
	virtual void AddGlobalV8FunctionTemplate( const char *pchJSFuncName, v8::Handle< v8::FunctionTemplate > *pFunc, bool bTrueGlobal = false ) = 0;

	// Get global v8 context
	virtual v8::Persistent<v8::Context> &GetV8GlobalContext() = 0;

	// Access the current object template we are setting up
	virtual v8::Handle<v8::ObjectTemplate> GetCurrentV8ObjectTemplateToSetup() = 0;

	// Allow access to the proto buf msg memory pool
	virtual CUIProtoBufMsgMemoryPoolMgr *MsgMemoryPoolMgr() = 0;

	// Allow access to style factory interface
	virtual IUIStyleFactory *UIStyleFactory() = 0;

	// Various code that uses JS needs this
	virtual v8::Isolate * GetV8Isolate() = 0;

	// Create a V8 object to wrap a panel
	virtual v8::Persistent<v8::Object> *CreateV8PanelInstance( IUIPanel *pPanel ) = 0;

	// Helper to create a JS object to wrap a given panel style
	virtual v8::Persistent<v8::Object> *CreateV8PanelStyleInstance( IUIPanelStyle *pPanelStyle ) = 0;

	// Helper to create JS object for given js object type
	virtual v8::Persistent<v8::Object> *CreateV8ObjectInstance( const char *pchObjectType, void *pActualObject, IUIJSObject *pJSObject ) = 0;

	// Create JSON web api job, use the helpers in uiwebapiclient.h directly instead, this is there for them to use internally
	virtual uint32 InitiateAsyncJSONWebAPIRequest( EHTTPMethod eMethod, CUtlString strURL, IUIPanel *pCallbackTargetPanel, void *pContext, CJSONWebAPIParams *pParams = NULL, HTTPCookieContainerHandle hCookieContainer = INVALID_HTTPCOOKIE_HANDLE ) = 0;

	// Create JSON web api job, use the helpers in uiwebapiclient.h directly instead, this is there for them to use internally
	virtual uint32 InitiateAsyncJSONWebAPIRequest( EHTTPMethod eMethod, CUtlString strURL, JSONWebAPIDelegate_t callback, void *pContext, CJSONWebAPIParams *pParams = NULL, HTTPCookieContainerHandle hCookieContainer = INVALID_HTTPCOOKIE_HANDLE ) = 0;

	// Cancel previously created web api request job
	virtual void CancelAsyncJSONWebAPIRequest( uint32 requestID ) = 0;

	// Resolves a path that may contain named path portions, etc, to a full local path
	virtual CUtlString ResolvePath( const char *pchPath ) = 0;

	// Used internally by initialization code to register events with framework
	virtual void RegisterEventWithEngine( CPanoramaSymbol symEvent, UIEventFactory factory ) = 0;

	// Check if a symbol is a valid event name
	virtual bool IsValidEventName( const CPanoramaSymbol symEvent ) = 0;

	// Check if a symbol is a valid panel event name
	virtual bool IsValidPanelEvent( const CPanoramaSymbol symEvent, int *pParams ) = 0;

	// Create input event from symbol, internal framework use
	virtual IUIEvent *CreateInputEventFromSymbol( CPanoramaSymbol symEvent, IUIPanel *pPanel, EPanelEventSource_t eSource, int nRepeats ) = 0;

	// Create an event from a string representation
	virtual IUIEvent *CreateEventFromString( IUIPanel *pCreatingPanel, const char *pchEvent, const char **pchEventEnd ) = 0;

	// Used internally by initialization code to register panels with framework
	virtual void RegisterPanelFactoryWithEngine( CPanoramaSymbol symPanelType, CPanel2DFactory *pFactory ) = 0;

	// Is the panel type registered
	virtual bool BRegisteredPanelType( CPanoramaSymbol symPanelType ) = 0;

	// Factory func for creating panels
	virtual IUIPanelClient *CreatePanel( CPanoramaSymbol symName, const char *pchID, panorama::IUIPanel *parent ) = 0;

	// Create debugger window
	virtual void CreateDebuggerWindow() = 0;

	// Close debugger window
	virtual void CloseDebuggerWindow() = 0;

	// Register any delegate to run at specified time, be sure to use CancelScheduledDelgate if you delete the object the delgate runs on, etc.
	virtual int RegisterScheduledDelegate( double flTargetFrameTime, CUtlDelegate< void() > del ) = 0;

	// Cancel a scheduled delegate by index returned from RegisterScheduledDelegate
	virtual void CancelScheduledDelegate( int iScheduleIndex ) = 0;

	// Return the last frame time for which we already ran scheduled delegates
	virtual double GetLastScheduledDelegateRunTime() = 0;

	// CPanoramaSymbol support for cross DLL symbols
	virtual UtlSymId_t MakeSymbol( const char *pchText ) = 0;

	// CPanoramaSymbol support for cross DLL symbols
	virtual const char * ResolveSymbol( const UtlSymId_t sym ) = 0;

	// Interface to allow animation/render threads to queue a decrement of a ref count on an object next frame in the main thread
	virtual void QueueDecrementRefNextFrame( CRefCount *pRefCountObj ) = 0;

	// Register a V8 callback function associated with a given UI panel
	virtual JSGenericCallbackHandle_t RegisterJSGenericCallback( panorama::IUIPanel *pContextPanel, v8::Handle< v8::Function > callbackFunc ) = 0;
	
	// Invoke a callback previously registered with the system. Returns false if the handle has expired (the context panel is gone, or it was explicitly unregistered)
	virtual bool InvokeJSGenericCallback( JSGenericCallbackHandle_t nHandle, int nArgs = 0, v8::Handle< v8::Value > *pArgs = NULL, v8::Handle< v8::Value > *pOutRetVal = NULL ) = 0;
	
	// Explicitly remove the callback from the system (future invokes on the handle will do nothing and return false)
	virtual void UnregisterJSGenericCallback( JSGenericCallbackHandle_t nHandle ) = 0;

	// Return the number of scopes, such as classes, that JS functions
	// have been registered in.
	virtual int GetNumRegisterJSScopes() = 0;

	// Return information on a JS registration scope.
	virtual void GetRegisterJSScopeInfo( int nScope, RegisterJSScopeInfo_t *pInfo ) = 0;

	// Return information on a specific entry in a JS registration scope,
	// such as a method in a class scope.
	virtual void GetRegisterJSEntryInfo( int nScope, int nEntry, RegisterJSEntryInfo_t *pInfo ) = 0;

	// Open a new scope for JS registrations.
	virtual int StartRegisterJSScope( const char *pName, const char *pDesc = NULL ) = 0;

	// Close current JS registration scope.
	virtual void EndRegisterJSScope() = 0;
	
	// If there is a current JS registration scope allocate a new entry
	// and fill it out.  If there is no scope, which is possible as
	// recording registration info is only enabled in certain places,
	// this will return -1.
	virtual int NewRegisterJSEntry( const char *pName, uint32 unFlags, const char *pDesc = NULL, RegisterJSType_t eDataType = k_ERegisterJSTypeUnknown ) = 0;

	// If there is a current JS registration entry set the parameter
	// type information in it.  Silently ignores -1 entry indices
	// so this can be called safely when there is no scope.
	virtual void SetRegisterJSEntryParams( int nEntry, uint8 unNumParams, RegisterJSType_t *pParamTypes ) = 0;

	// Invalidate cached copies of all layout/style/script files (used eg. by the game when search paths change)
	// (Does NOT rebuild or reload any existing UI, just causes subsequent references to the files to load from scratch.)
	virtual void ClearFileCache() = 0;
	
	// Spew the current list of all cached files and their refcounts (for debugging)
	virtual void PrintCacheStatus() = 0;

	// get a list of all the windows owned by the engine
	virtual void GetWindowsForDebugger( CUtlVector<IUIWindow *> &vecWindows ) = 0;

	// Turn on paint count tracking for panels
	virtual void SetPaintCountTrackingEnabled( bool bEnablePaintCountTracking ) = 0;

	// Is paint count tracking on for panels
	virtual bool GetPaintCountTrackingEnabled() = 0;

	// Increment paint count tracking for panels
	virtual void IncrementPaintCountForPanel( uint64 ulPanelPtrValue, bool bRequiredCompositionLayer, double flFrameTime ) = 0;

	// Get panel paint info for the panel
	virtual void GetPanelPaintInfo( uint64 ulPanelPtrValue, uint32 &unPaintCount, bool &bRequiredCompositionLayer, double &flFrameTimeLastPaint ) = 0;

	// Returns whether any windows exist for the UI engine
	virtual bool BHasAnyWindows() = 0;

#ifdef DBGFLAG_VALIDATE
	// Memory validation related interfaces
	virtual bool PrepareForValidate() = 0;
	virtual bool ResumeFromValidate() = 0;
	virtual void Validate( CValidator &validator, const tchar *pchName ) = 0;
#endif

#if defined( SOURCE2_PANORAMA )
	virtual void TextEntryFocusChange( IUIPanel *pPanel ) = 0;
	virtual void TextEntryInvalid( IUIPanel *pPanel ) = 0;
#endif
};


//-----------------------------------------------------------------------------
// Purpose: Helper for render state transitions, returns true if a state
//			change is being requested, and it is valid. Helper for fullscreen switching.
//-----------------------------------------------------------------------------
inline bool IUIEngine::BValidRenderStateChange( ERenderTarget eCurrentRenderTarget, ERenderTarget eRequestedRenderTarget )
{
	if( eCurrentRenderTarget == eRequestedRenderTarget )
		return false;

	// if we are in a mode that can switch...
	if( eCurrentRenderTarget == IUIEngine::k_ERenderFullScreen || eCurrentRenderTarget == IUIEngine::k_ERenderToWindow )
	{
		switch( eRequestedRenderTarget ) // test if request is for a state that can be switched to
		{
		case IUIEngine::k_ERenderFullScreen:
		case IUIEngine::k_ERenderToWindow:
			return true; // we can change
		default:
			return false; // not so much
		}
	}

	// render to texture toggled shared vs copied texture
	if( BIsRenderingToTexture( eCurrentRenderTarget ) &&
		BIsRenderingToTexture( eRequestedRenderTarget ) &&
		(eCurrentRenderTarget != eRequestedRenderTarget) )
	{
		return true;
	}

	return false;
}

#ifndef PANORAMA_EXPORTS
extern IUIEngine *g_pUIEngineSingleton;
extern CSysModule *g_PanoramaModule;

inline IUIEngine *UIEngine() { return g_pUIEngineSingleton; }
inline IUILocalization *UILocalize() { return UIEngine()->UILocalize(); }
inline IUISoundSystem *UISoundSystem() { return UIEngine()->UISoundSystem(); }
inline IUIInput *UIInputEngine() { return UIEngine() ? UIEngine()->UIInputEngine() : NULL; }
#else
IUIEngine *UIEngine();
IUILocalization *UILocalize();
IUIInput *UIInputEngine();
IUISoundSystem *UISoundSystem();
#endif

#ifdef DBGFLAG_VALIDATE
PANORAMA_INTERFACE void ValidateStaticsInternal( CValidator &validator );
#endif

PANORAMA_INTERFACE IUIEngine *CreatePanoramaUIEngineInternal();

extern void RegisterEventTypesWithEngine( IUIEngine *pEngine );

#ifndef PANORAMA_EXPORTS
extern bool LoadPanoramaModule( const char *pchPanoramaModulePath );
extern void UnloadPanoramaModule();

extern IUIEngine *CreatePanoramaUIEngine();
extern void ConnectPanoramaUIEngine( IUIEngine * );
extern void ShutdownPanoramaUIEngine( IUIEngine * );

#ifdef DBGFLAG_VALIDATE
extern void ValidateStatics( CValidator &validator );
#endif
#endif


// To expose a generic object via UIEngine()->ExposeObjectTypeToJavaScript() you should derive from this interface
// the GetTypeName member should return the same type name you register as exposed
class IUIJSObject
{
public:
	~IUIJSObject()
	{
		UIEngine()->DeleteJSObjectInstance( this );
	}

	virtual const char *GetJSTypeName() = 0;
};


} // namespace panorama

#endif // IUIENGINE_H
