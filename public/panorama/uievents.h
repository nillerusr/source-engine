//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef UIEVENTS_H
#define UIEVENTS_H

#ifdef _WIN32
#pragma once
#endif

#include "uievent.h"
#include "uieventcodes.h"
#include "layout/stylesymbol.h"
#include "localization/ilocalize.h"
class IVideoPlayer;


namespace panorama
{

// general panel events
DECLARE_PANEL_EVENT1( AddStyle, const char * );
DECLARE_PANEL_EVENT1( RemoveStyle, const char * );
DECLARE_PANEL_EVENT1( ToggleStyle, const char * );
DECLARE_PANEL_EVENT1( AddStyleToEachChild, const char * );
DECLARE_PANEL_EVENT1( RemoveStyleFromEachChild, const char * );
DECLARE_PANEL_EVENT0( PanelLoaded );
DECLARE_PANEL_EVENT0( CheckChildrenScrolledIntoView );
DECLARE_PANEL_EVENT2( ScrollPanelIntoView, ScrollBehavior_t, bool );
DECLARE_PANEL_EVENT0( ScrolledIntoView );
DECLARE_PANEL_EVENT0( ScrolledOutOfView );
DECLARE_PANEL_EVENT2( LoadLayoutFileAsync, const char *, bool );
DECLARE_PANEL_EVENT1( AppendChildrenFromLayoutFileAsync, const char * );
DECLARE_PANEL_EVENT2( LoadLayoutFromXMLStringAsync, const char *, bool );
DECLARE_PANEL_EVENT2( LoadLayoutFromBase64XMLStringAsync, const char *, bool );
DECLARE_PANEL_EVENT1( Activated, EPanelEventSource_t );
DECLARE_PANEL_EVENT1( Cancelled, EPanelEventSource_t );
DECLARE_PANEL_EVENT1( ContextMenu, EPanelEventSource_t );
DECLARE_PANEL_EVENT1( LocalizationChanged, const ILocalizationString * );
DECLARE_PANEL_EVENT0( InputFocusSet );
DECLARE_PANEL_EVENT0( InputFocusLost );
DECLARE_PANORAMA_EVENT1( InputFocusTopLevelChanged, CPanelPtr< CPanel2D > );
DECLARE_PANEL_EVENT0( SetInputFocus );
DECLARE_PANEL_EVENT0( ShowTooltip );
DECLARE_PANEL_EVENT0( StyleFlagsChanged );
DECLARE_PANEL_EVENT0( StyleClassesChanged );
DECLARE_PANEL_EVENT0( PanelStyleChanged );
DECLARE_PANEL_EVENT1( AnimationStart, CPanoramaSymbol );
DECLARE_PANEL_EVENT1( AnimationEnd, CPanoramaSymbol );
DECLARE_PANEL_EVENT1( PropertyTransitionEnd, CStyleSymbol );
DECLARE_PANEL_EVENT1( CopyStringToClipboard, const char * );
DECLARE_PANEL_EVENT1( SetAllChildrenActivationEnabled, bool );
DECLARE_PANEL_EVENT2( SetPanelEvent, const char *, const char * );
DECLARE_PANEL_EVENT1( ClearPanelEvent, const char * );
DECLARE_PANEL_EVENT2( IfHasClassEvent, const char *, IUIEvent * );
DECLARE_PANEL_EVENT2( IfNotHasClassEvent, const char *, IUIEvent * );
DECLARE_PANEL_EVENT2( IfHoverOtherEvent, const char *, IUIEvent * );
DECLARE_PANEL_EVENT2( IfNotHoverOtherEvent, const char *, IUIEvent * );
DECLARE_PANEL_EVENT0( ScrollToTop );
DECLARE_PANEL_EVENT0( ScrollToBottom );
DECLARE_PANEL_EVENT3( LoadAsyncComplete, bool, ELoadLayoutAsyncDetails, bool );
DECLARE_PANEL_EVENT1( SetPanelSelected, bool );
DECLARE_PANEL_EVENT0( ResetToDefaultValue );
DECLARE_PANEL_EVENT0( TogglePanelSelected );
DECLARE_PANEL_EVENT1( SetChildPanelsSelected, bool );
DECLARE_PANEL_EVENT0( ScrollPanelLeft );
DECLARE_PANEL_EVENT0( ScrollPanelRight );
DECLARE_PANEL_EVENT0( ScrollPanelUp );
DECLARE_PANEL_EVENT0( ScrollPanelDown );
DECLARE_PANEL_EVENT0( PagePanelLeft );
DECLARE_PANEL_EVENT0( PagePanelRight );
DECLARE_PANEL_EVENT0( PagePanelUp );
DECLARE_PANEL_EVENT0( PagePanelDown );
DECLARE_PANEL_EVENT1( DropdownMenuFocusChanged, CPanelPtr< IUIPanel > );

// window events
class CTopLevelWindow;
DECLARE_PANORAMA_EVENT1( WindowGotFocus, IUIWindow * );
DECLARE_PANORAMA_EVENT1( WindowLostFocus, IUIWindow * );
DECLARE_PANORAMA_EVENT1( WindowCursorShown, IUIWindow * );
DECLARE_PANORAMA_EVENT1( WindowCursorHidden, IUIWindow * );
DECLARE_PANORAMA_EVENT1( WindowShown, IUIWindow * );
DECLARE_PANORAMA_EVENT1( WindowHidden, IUIWindow * );
DECLARE_PANORAMA_EVENT1( WindowOffScreen, IUIWindow * );
DECLARE_PANORAMA_EVENT1( WindowOnScreen, IUIWindow * );

// global events
DECLARE_PANORAMA_EVENT0( QuitApp );
DECLARE_PANORAMA_EVENT0( ExitSteam );
DECLARE_PANORAMA_EVENT0( ShutdownMachine );
DECLARE_PANORAMA_EVENT0( RestartMachine );
DECLARE_PANORAMA_EVENT0( SuspendMachine );
DECLARE_PANORAMA_EVENT0( TurnOffActiveController );
DECLARE_PANORAMA_EVENT0( GoOffline );
DECLARE_PANORAMA_EVENT0( GoOnline );
DECLARE_PANORAMA_EVENT0( ShowQuitDialog );
DECLARE_PANORAMA_EVENT0( ChangeUser );
DECLARE_PANORAMA_EVENT0( ToggleDebugger );
DECLARE_PANORAMA_EVENT0( ShowPanelZoo );
DECLARE_PANORAMA_EVENT0( DumpMemory );
DECLARE_PANORAMA_EVENT0( ProfileOn );
DECLARE_PANORAMA_EVENT0( ProfileOff );
DECLARE_PANORAMA_EVENT0( ToggleConsole );
DECLARE_PANORAMA_EVENT0( Refresh );
DECLARE_PANORAMA_EVENT1( MoveUp, int );
DECLARE_PANORAMA_EVENT1( MoveDown, int );
DECLARE_PANORAMA_EVENT1( MoveLeft, int );
DECLARE_PANORAMA_EVENT1( MoveRight, int );
DECLARE_PANORAMA_EVENT0( ScrollUp );
DECLARE_PANORAMA_EVENT0( ScrollDown );
DECLARE_PANORAMA_EVENT0( ScrollLeft );
DECLARE_PANORAMA_EVENT0( ScrollRight );
DECLARE_PANORAMA_EVENT0( PageUp );
DECLARE_PANORAMA_EVENT0( PageDown );
DECLARE_PANORAMA_EVENT0( PageLeft );
DECLARE_PANORAMA_EVENT0( PageRight );
DECLARE_PANORAMA_EVENT1( TabForward, int );
DECLARE_PANORAMA_EVENT1( TabBackward, int );
DECLARE_PANORAMA_EVENT0( GamepadInserted );
DECLARE_PANORAMA_EVENT0( GamepadRemoved );
DECLARE_PANORAMA_EVENT1( ReloadStyleFile, CPanoramaSymbol );
DECLARE_PANORAMA_EVENT1( TopLevelWindowClose, IUIWindow* );		// fired when top level window is destructing while all children are still valid
DECLARE_PANORAMA_EVENT1( TopLevelWindowClosed, IUIWindow* );		// fired after top level window has already destroyed all children
DECLARE_PANORAMA_EVENT0( GamepadInput );
DECLARE_PANEL_EVENT0( DeletePanel );
DECLARE_PANORAMA_EVENT0( ActivateMainWindow );
DECLARE_PANORAMA_EVENT2( ToggleFullscreen, IUIWindow*, bool );
DECLARE_PANORAMA_EVENT0( GuideButton );
DECLARE_PANORAMA_EVENT0( GuideButtonUp );
DECLARE_PANORAMA_EVENT2( SoundFinished, const char *, HAUDIOSAMPLE );
DECLARE_PANORAMA_EVENT0( None );											// short circuited in the bind code not to fire an event
DECLARE_PANORAMA_EVENT1( ExecuteSteamURL, const char * );
DECLARE_PANORAMA_EVENT0( UserInputActive ); 
DECLARE_PANORAMA_EVENT1( AsyncPanoramaQuitWithError, const char * );
DECLARE_PANORAMA_EVENT0( GameControllerMappingChanged );
DECLARE_PANORAMA_EVENT0( StopStreaming );
DECLARE_PANORAMA_EVENT0( CloseModalDialog );
DECLARE_PANORAMA_EVENT2( SoundVolumeChanged, ESoundType, float );
DECLARE_PANORAMA_EVENT1( SoundMuteChanged, bool );
DECLARE_PANORAMA_EVENT1( ActiveControllerTypeChanged, EActiveControllerType );

void OnActiveControllerTypeChangedDefaultHandler( IUIPanel *pPanel, EActiveControllerType eActiveControllerType );

DECLARE_PANORAMA_EVENT0( MediaVolumeMute );
DECLARE_PANORAMA_EVENT0( MediaVolumeDown );
DECLARE_PANORAMA_EVENT0( MediaVolumeUp );
DECLARE_PANORAMA_EVENT0( MediaNextTrack );
DECLARE_PANORAMA_EVENT0( MediaPrevTrack );
DECLARE_PANORAMA_EVENT0( MediaStop );
DECLARE_PANORAMA_EVENT0( MediaPlayPause );

DECLARE_PANORAMA_EVENT0( SteamPadRightHighActivity );


DECLARE_PANORAMA_EVENT2( JSConsoleOutput, CPanelPtr< CPanel2D >, const char * );

// Not necessarily universal but shared across multiple panels
DECLARE_PANORAMA_EVENT0( RemoveUser );
DECLARE_PANEL_EVENT0( PollingForSteamClientUpdate );
DECLARE_PANORAMA_EVENT0( SettingsPanelShown );

// Event to wrap any other event up async
DECLARE_PANORAMA_EVENT2( AsyncEvent, float, IUIEvent * );

// Request from some UI to show a URL in the systems browser, may be hooked and handled differently in different applications, not handled by default in panorama itself
DECLARE_PANEL_EVENT1( BrowserGoToURL, const char * );

DECLARE_PANORAMA_EVENT0( AsyncPanoramaSurfaceLost ); // 3d surface detected that its output became unavailable, currently fired by Linux on VTT switch
DECLARE_PANORAMA_EVENT0( AsyncPanoramaSurfaceReturned ); // 3d surface detected that its previously lost surface is now renderable again but will need a full reload

// When text input handler is coming up or down, bool = true when showing, = false for hiding
DECLARE_PANEL_EVENT1( TextInputHandlerStateChange, bool );

// InMemoryFileUpdate event params:
// 1 - symbol of file that changed
// 2 - location in file that changed
// 3 - old size
// 4 - new size
DECLARE_PANORAMA_EVENT4( InMemoryFileUpdate, CPanoramaSymbol, uint, uint, uint );
DECLARE_PANORAMA_EVENT0( InMemoryFilesSaved );


// video player events
DECLARE_PANORAMA_EVENT1( VideoPlayerInitalized, IVideoPlayer* );
DECLARE_PANORAMA_EVENT1( VideoPlayerRepeated, IVideoPlayer* );
DECLARE_PANORAMA_EVENT1( VideoPlayerEnded, IVideoPlayer* );
DECLARE_PANORAMA_EVENT1( VideoPlayerPlaybackStateChange, IVideoPlayer* );
DECLARE_PANORAMA_EVENT1( VideoPlayerChangedRepresentation, IVideoPlayer* );

DECLARE_PANORAMA_EVENT2( OverlayGamepadInputMsg, panorama::IUIWindow *, InputMessage_t * )

// debugger events
DECLARE_PANORAMA_EVENT0( CreateDebuggerWindow );
DECLARE_PANORAMA_EVENT0( CloseDebuggerWindow );
DECLARE_PANORAMA_EVENT0( BeginDebuggerInspect );

DECLARE_PANEL_EVENT2( JSONWebAPIResponse, KeyValues *, void * );

// panel drag events
DECLARE_PANEL_EVENT1( DragStart, IUIPanel** );										// first event for drag sent to panel user started dragging on (must be marked with draggable="true")
DECLARE_PANEL_EVENT1( DragEnter, panorama::CPanelPtr< panorama::IUIPanel > );		// sent to panel drag is hovering over
DECLARE_PANEL_EVENT1( DragDrop, panorama::CPanelPtr< panorama::IUIPanel > );		// sent to panel where user released mouse while dragging
DECLARE_PANEL_EVENT1( DragLeave, panorama::CPanelPtr< panorama::IUIPanel > );		// sent to panel drag stopped hovering over
DECLARE_PANEL_EVENT1( DragEnd, panorama::CPanelPtr< panorama::IUIPanel > );			// sent to panel which received DragStart after Drop event was sent


} // namespace panorama

#endif // UIEVENTS_H
