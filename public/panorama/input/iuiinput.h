//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//
#ifndef IUIINPUT_H
#define IUIINPUT_H

#ifdef _WIN32
#pragma once
#endif

#include "keycodes.h"
#include "mousecodes.h"
#include "gamepadcodes.h"
#include "tier0/platform.h"
#include "tier1/utldelegate.h"
#include "../controls/panelhandle.h"
#include "../uieventcodes.h"
#include "../iuiengine.h"

#ifdef SOURCE2_PANORAMA
#include "inputsystem/buttoncode.h"
#endif

namespace panorama
{

class CPanel2D;
class IImageSource;
class IUISettings;

// the classes of input we understand
enum EInputType
{
	k_eInputNone		= 0,
	k_eKeyDown			= 1, // raw down press
	k_eKeyUp			= 2, // raw release
	k_eKeyChar			= 3, // composited text entry from the OS
	k_eMouseDown		= 4,
	k_eMouseUp			= 5,
	k_eMouseMove		= 6,
	k_eMouseDoubleClick	= 7,
	k_eMouseTripleClick	= 8,
	k_eMouseWheel		= 9,
	k_eMouseEnter		= 10,
	k_eMouseLeave		= 11,
	k_eGamePadDown		= 12,
	k_eGamePadUp		= 13,
	k_eGamePadAnalog	= 14,

	k_eOverlayCommand	= 5000,

	k_eInputTypeMaxRange = 0xFFFFFFFF // force size to 32 bits
};


enum EActiveControllerType
{
	k_EActiveControllerType_None,		// we aren't using a controller or we don't know to do
	k_EActiveControllerType_XInput,
	k_EActiveControllerType_Steam,
};

//
// structs container input type specific data
//
struct KeyData_t
{
	EPanelEventSource_t m_eSource; // this needs to be the first field of the struct, since it's unioned in InputMessage_t

	KeyCode m_KeyCode;
	uint8 m_RepeatCount;
	bool m_bFirstDown; // is this the first time this key was pressed
	wchar_t m_UniChar; // unicode equivalent of this key
	uint32 m_Modifiers; // alt, ctrl, etc held down?
};


struct MouseData_t
{
	EPanelEventSource_t m_eSource; // this needs to be the first field of the struct, since it's unioned in InputMessage_t

	MouseCode m_MouseCode;
	uint32 m_Modifiers;
	uint8 m_RepeatCount;
	int m_Delta;
};

struct GamePadData_t
{
	EPanelEventSource_t m_eSource; // this needs to be the first field of the struct, since it's unioned in InputMessage_t

	GamePadCode m_GamePadCode;
	uint8 m_RepeatCount; // home many times in a row we have had this button down
	float m_fValue; // the analog value of the deflection if an axis

	// if an analog event then these two are set for each axis value
	float m_fValueX; // the analog value of the deflection along horizontal
	float m_fValueY; // the analog value of the deflection along vertical

	// For Steam controller analog events this value will indicate the time the users finger went down
	float m_flFingerDown;

	// For Steam controller analog events these values indicate the starting finger position (so you can do some basic swipe stuff)
	float m_fValueXFirst;
	float m_fValueYFirst;
	
	// For Steam controller analog events this is the raw sampled coordinate without deadzoning
	float m_fValueXRaw;
	float m_fValueYRaw;
};


struct InputMessage_t
{
	EInputType m_eInputType;
	float m_flInputTime;
	union 
	{
		EPanelEventSource_t m_eSource; // where did this event originate? this is the first field of the other *Data_t structs below.
		KeyData_t m_KeyData;
		MouseData_t m_MouseData;
		GamePadData_t m_GamePadData;
	};
};


//
// A combination of event type and specific code data to mean an input event, like on key down of the A key
//
struct ActionInput_t
{
	ActionInput_t() 
	{ 
		m_InputType = k_eInputNone; 
		m_Data.m_KeyCode = KEY_NONE; 
		m_unModifiers = MODIFIER_NONE;
	}
	
	explicit ActionInput_t( EInputType type, KeyCode code, uint32 unModifiers, const char *pchNamespace ) 
	{ 
		m_InputType = type; 
		m_Data.m_KeyCode = code; 
		m_unModifiers = unModifiers;
		if ( pchNamespace && pchNamespace[0] )
			m_symNameSpace = pchNamespace;
	}
	
	explicit ActionInput_t( EInputType type, GamePadCode code, const char *pchNamespace ) 
	{ 
		m_InputType = type; 
		m_Data.m_GamePadCode = code; 
		m_unModifiers = MODIFIER_NONE;
		if ( pchNamespace && pchNamespace[0] )
			m_symNameSpace = pchNamespace;
	}
	
	explicit ActionInput_t( EInputType type, MouseCode code, const char *pchNamespace ) 
	{ 
		m_InputType = type; 
		m_Data.m_MouseCode = code; 
		m_unModifiers = MODIFIER_NONE;
		if ( pchNamespace && pchNamespace[0] )
			m_symNameSpace = pchNamespace;
	}
	
	ActionInput_t( InputMessage_t &msg, const char *pchNamespace )
	{
		m_InputType = msg.m_eInputType;
		if ( pchNamespace && pchNamespace[0] )
			m_symNameSpace = pchNamespace;
		m_unModifiers = MODIFIER_NONE;
		switch ( msg.m_eInputType )
		{
		default:
			AssertMsg( false, "Unknown input type to ActionInput_t( InputMessage_t )" );
			break;
		case k_eKeyDown:
		case k_eKeyUp:
		case k_eKeyChar:
			m_Data.m_KeyCode = msg.m_KeyData.m_KeyCode;
			m_unModifiers = msg.m_KeyData.m_Modifiers;
			break;
		case k_eMouseDown:
		case k_eMouseDoubleClick:
		case k_eMouseTripleClick:
		case k_eMouseUp:
		case k_eMouseMove:
		case k_eMouseWheel:
			m_Data.m_MouseCode = msg.m_MouseData.m_MouseCode;
			m_unModifiers = msg.m_MouseData.m_Modifiers;
			break;
		case k_eMouseEnter:
		case k_eMouseLeave:
			break;
		case k_eGamePadDown:
		case k_eGamePadUp:
		case k_eGamePadAnalog:
			m_Data.m_GamePadCode = msg.m_GamePadData.m_GamePadCode;
			break;
		}
	}


	// Can't ever make this not 32bits in size, see crazy comparison operators below.  Also, each
	// member must be exactly 32bits, not less with uninitialized data.
	EInputType m_InputType;
	union
	{
		KeyCode m_KeyCode;
		GamePadCode m_GamePadCode;
		MouseCode m_MouseCode;
	} m_Data;

	uint32 m_unModifiers;
	CPanoramaSymbol m_symNameSpace;

	bool operator<( const ActionInput_t &that ) const
	{
		if ( m_InputType != that.m_InputType  )
			return m_InputType < that.m_InputType;

		if ( m_unModifiers != that.m_unModifiers )
			return m_unModifiers < that.m_unModifiers;

		// I have a namespace and you don't
		if ( m_symNameSpace.IsValid() && !that.m_symNameSpace.IsValid() )
			return true;

		// You have a namespace and I don't
		if ( that.m_symNameSpace.IsValid() && !m_symNameSpace.IsValid() )
			return false;

		// compare namespace if both are valid but not equal
		if ( m_symNameSpace.IsValid() && that.m_symNameSpace.IsValid() && m_symNameSpace != that.m_symNameSpace )
			return m_symNameSpace < that.m_symNameSpace;

		// lets compare the raw code now
		switch( m_InputType )
		{
		default:
		case k_eKeyDown:
		case k_eKeyUp:
		case k_eKeyChar:
			return m_Data.m_KeyCode < that.m_Data.m_KeyCode;
		case k_eMouseDown:
		case k_eMouseUp:
		case k_eMouseMove:
		case k_eMouseWheel:
		case k_eMouseDoubleClick:
		case k_eMouseTripleClick:
			return m_Data.m_MouseCode < that.m_Data.m_MouseCode;
		case k_eGamePadDown:
		case k_eGamePadUp:
		case k_eGamePadAnalog:
			return m_Data.m_GamePadCode < that.m_Data.m_GamePadCode;
		}
	}

	bool operator==( const ActionInput_t &that ) const
	{
		if ( m_InputType == that.m_InputType && m_unModifiers == that.m_unModifiers &&  
			// also if both namespace are valid and equal OR either namespace is unset (i.e "global")
			( ( m_symNameSpace.IsValid() && that.m_symNameSpace.IsValid() && m_symNameSpace == that.m_symNameSpace ) || ( !m_symNameSpace.IsValid() && !that.m_symNameSpace.IsValid() ) )  )
		{
			switch( m_InputType )
			{
			default:
			case k_eKeyDown:
			case k_eKeyUp:
			case k_eKeyChar:
				return m_Data.m_KeyCode == that.m_Data.m_KeyCode;
			case k_eMouseDoubleClick:
			case k_eMouseTripleClick:
			case k_eMouseDown:
			case k_eMouseUp:
			case k_eMouseMove:
			case k_eMouseWheel:
				return m_Data.m_MouseCode == that.m_Data.m_MouseCode;
			case k_eGamePadDown:
			case k_eGamePadUp:
			case k_eGamePadAnalog:
				return m_Data.m_GamePadCode == that.m_Data.m_GamePadCode;
			}
		}
		return false;
	}

};

// Helpers for checking modifier state
inline bool IsControlPressed( uint32 unModifiers ) { return unModifiers & MODIFIER_LCONTROL || unModifiers & MODIFIER_RCONTROL; }
inline bool IsAltPressed( uint32 unModifiers ) { return unModifiers & MODIFIER_LALT || unModifiers & MODIFIER_RALT; }
inline bool IsShiftPressed( uint32 unModifiers ) { return unModifiers & MODIFIER_LSHIFT || unModifiers & MODIFIER_RSHIFT; }
inline bool IsWinPressed( uint32 unModifiers ) { return unModifiers & MODIFIER_LWIN || unModifiers & MODIFIER_RWIN; }


// 
// struct to wrap mouse move events for mouse tracked panels
//
struct MouseTrackingResults_t
{
	MouseTrackingResults_t()
	{
		m_hPanel = k_ulInvalidPanelHandle64;
		m_flX = 0.0f;
		m_flY = 0.0f;

	}
	MouseTrackingResults_t( uint64 handle, float x, float y )
	{
		m_hPanel = handle;
		m_flX = x;
		m_flY = y;
	}

	uint64 m_hPanel;
	float m_flX;
	float m_flY;
};

//
// An interface to receive captured input
//
class IInputCapture
{
public:
	// keyboard
	virtual bool OnCapturedKeyDown( IUIPanel *pPanel, const KeyData_t &code ) = 0;
	virtual bool OnCapturedKeyUp( IUIPanel *pPanel, const KeyData_t &code ) = 0;
	virtual bool OnCapturedKeyTyped( IUIPanel *pPanel, const KeyData_t &unichar ) = 0;

	// mouse
	virtual bool OnCapturedMouseMove( IUIPanel *pPanel ) = 0;
	virtual bool OnCapturedMouseButtonDown( IUIPanel *pPanel, const MouseData_t &code ) = 0;
	virtual bool OnCapturedMouseButtonUp( IUIPanel *pPanel, const MouseData_t &code ) = 0;
	virtual bool OnCapturedMouseButtonDoubleClick( IUIPanel *pPanel, const MouseData_t &code ) = 0;
	virtual bool OnCapturedMouseButtonTripleClick( IUIPanel *pPanel, const MouseData_t &code ) = 0;
	virtual bool OnCapturedMouseWheel( IUIPanel *pPanel, const MouseData_t &code ) = 0;

	// gamepad
	virtual bool OnCapturedGamePadDown( IUIPanel *pPanel, const GamePadData_t &code ) = 0;
	virtual bool OnCapturedGamePadUp( IUIPanel *pPanel, const GamePadData_t &code ) = 0;
	virtual bool OnCapturedGamePadAnalog( IUIPanel *pPanel, const GamePadData_t &code ) = 0;
};

class CDefaultInputCapture : public IInputCapture
{
public:
	// keyboard
	virtual bool OnCapturedKeyDown( panorama::IUIPanel *pPanel, const panorama::KeyData_t &code ) OVERRIDE { return false; }
	virtual bool OnCapturedKeyUp( panorama::IUIPanel *pPanel, const panorama::KeyData_t &code ) OVERRIDE { return false; }
	virtual bool OnCapturedKeyTyped( panorama::IUIPanel *pPanel, const panorama::KeyData_t &unichar ) OVERRIDE { return false; }

	// mouse
	virtual bool OnCapturedMouseMove( panorama::IUIPanel *pPanel ) OVERRIDE { return false; }
	virtual bool OnCapturedMouseButtonDown( panorama::IUIPanel *pPanel, const panorama::MouseData_t &code ) OVERRIDE { return false; }
	virtual bool OnCapturedMouseButtonUp( panorama::IUIPanel *pPanel, const panorama::MouseData_t &code ) OVERRIDE { return false; }
	virtual bool OnCapturedMouseButtonDoubleClick( panorama::IUIPanel *pPanel, const panorama::MouseData_t &code ) OVERRIDE { return false; }
	virtual bool OnCapturedMouseButtonTripleClick( panorama::IUIPanel *pPanel, const panorama::MouseData_t &code ) OVERRIDE { return false; }
	virtual bool OnCapturedMouseWheel( panorama::IUIPanel *pPanel, const panorama::MouseData_t &code ) OVERRIDE { return false; }

	// gamepad
	virtual bool OnCapturedGamePadDown( panorama::IUIPanel *pPanel, const panorama::GamePadData_t &code ) OVERRIDE { return false; }
	virtual bool OnCapturedGamePadUp( panorama::IUIPanel *pPanel, const panorama::GamePadData_t &code ) OVERRIDE { return false; }
	virtual bool OnCapturedGamePadAnalog( panorama::IUIPanel *pPanel, const panorama::GamePadData_t &code ) OVERRIDE { return false; }
};

//
// Handles per top level window focus
//
class IUIWindowInput
{
public:
	virtual bool InputEvent( InputMessage_t &msg, bool bNewEvent = true ) = 0;

	// Receive mouse move events, in window coordinate space
	virtual void OnMouseMove( float flMouseX, float flMouseY, bool bSynthesized = false ) = 0;

	// current mouse coordinates and visibility
	virtual void GetSurfaceMousePosition( float &x, float &y ) = 0;
	virtual bool BCursorVisible() = 0;
	virtual void WakeupMouseCursor() = 0;
	virtual void FadeOutCursorNow() = 0;

	// gamepad state
	virtual int GetNumGamepadsConnected() = 0;
	virtual bool BWasGamepadConnectedThisSession() = 0;
	virtual bool BWasGamepadUsedThisSession() = 0;
	virtual bool BWasSteamControllerConnectedThisSession() = 0;
	virtual bool BWasSteamControllerUsedThisSession() = 0;

	// tracking of last input type
	virtual bool BWasGamepadLastInputSource() = 0;
	virtual bool BWasMouseLastInputSource() = 0;
	virtual bool BWasKeyboardOrMouseLastInputSource() = 0;

	// Get the last input source
	virtual EPanelEventSource_t GetLastPanelEventSource() = 0;

	// Keyboard / mouse info
	virtual bool BWasKeyboardOrMouseUsedThisSession() = 0;
	virtual bool BWasMouseMovedThisSession() = 0;

	// top level OS window support
	virtual void GotWindowFocus() = 0;
	virtual void LostWindowFocus() = 0;
	virtual bool BHasWindowFocus() = 0;

	// Window can temporarily disable all input, used for overlay when the game is focused, but overlay inactive
	virtual bool BAllowInput(  InputMessage_t &msg ) = 0;

	// panel management
	virtual void SetInputFocus( IUIPanel *pPanel, bool bScrollParentToFit, bool bChangeContextIfNeeded ) = 0;
	virtual bool SetInputFocusContext( IUIPanel *pPanelInContext ) = 0;
	virtual void PopInputContext() = 0;
	virtual IUIPanel *GetInputFocusContext() = 0;
	virtual IUIPanel *GetInputFocus() = 0;
	virtual IUIPanel *GetMouseHover() = 0;
	virtual void PanelDeleted( IUIPanel *pPanel, IUIPanel *pParent ) = 0;

	// input hooks
	virtual void HookPanelInput( IUIPanel *pPanel, IInputCapture *pInputCapture ) = 0;
	virtual void RemovePanelInputHook( IUIPanel *pPanel, IInputCapture *pInputCapture ) = 0;

	// set this panel to always (or stop) getting MouseMove events
	virtual void AddMouseTrackingPanel( IUIPanel *pPanel ) = 0;
	virtual void RemoveMouseTrackingPanel( IUIPanel *pPanel ) = 0;

	// Are we currently inside a set input focus call
	virtual bool BInSetInputFocusTraverse() = 0;

	// Queue a panel focus event to occur once we finish with setting input focus
	virtual void QueuePanelFocusEvent( IUIPanel *pPanel, CPanoramaSymbol symPanelEvent ) = 0;

	// reset any mouse movement count as we just hid the cursor
	virtual void ResetMouseMoveCount() = 0;

	// Get focus panel at time of last mouse down
	virtual IUIPanel *GetFocusOnLastMouseDown() = 0;
};


//
// Handles key/mouse/gamepad input and dispatches to appropriate panels
//
class IUIInput
{
public:
	virtual void Initialize( IUISettings *pSettings ) = 0;

	// Not ifdef'd or specific to windows. v_key ended up as a common
	// denominator in lots of code (overlay as an example)
	// 0x00 for error in mapping. There is no 0x00 VKEY
	virtual uint16 KeyCodeToWindowsVKey( const KeyCode inKey ) = 0;

	// KEY_NONE will come back on error.
	virtual KeyCode WindowsVKeyToKeyCode( uint16 inKey ) = 0;

#ifdef SOURCE2_PANORAMA
	virtual ButtonCode_t KeyCodeToButtonCode( const KeyCode inKey ) = 0;
	virtual ButtonCode_t MouseCodeToButtonCode( const MouseCode inKey ) = 0;
#endif

	// kb/mouse input
	virtual bool InputEvent( InputMessage_t &msg ) = 0;

	// used to capture all input
	virtual void SetInputCapture( IInputCapture *pCapture ) = 0;
	virtual void ReleaseInputCapture( IInputCapture *pCapture ) = 0;
	virtual CUtlVector< IInputCapture * > &GetInputCapture() = 0;

	// Checks whether gamepads are connected
	virtual int GetNumGamepadsConnected() const = 0;
	
	// did any gamepad have input since we last asked
	virtual bool BWasGamepadOrSteamControllerActive() = 0;
	
	// flags to tell steam controller layer which buttons to treat as mouse and not disable cursor on seeing
	virtual void SetSteamPadButtonsToTreatAsMouse( uint64 ulButtonMask ) = 0;

	// if a gamepad is connected then its friendly name
	virtual const char *PchGamePadName() = 0;

	// return true if we are emulating a gamepad using a simple joystick, so we have less input functionality available
	virtual bool BEmulatingGamePadWithJoystick() = 0;

	// helper for gamepad codes, returns values that are inside the deadzone for this joystick
	virtual float GetDeadZoneValue( GamePadCode code ) = 0;

	// translate an event into the gamepad key bound to it, XK_NULL if not bound
	virtual const GamePadCode GetGamePadBindForEvent( const char *pchEvent, const IUIPanel *pFromPanel ) = 0;

	// is capslock on
	virtual bool BIsCapsLockOn() = 0;

	// If we're trying to show help text for a controller, which type of controller is most relevant (ie., most recently
	// used, exists, etc.)? You probably don't want to call this directly but instead listen for ActiveControllerTypeChanged.
	virtual EActiveControllerType GetActiveControllerType() const = 0;

	// Get count of actively connected Steam controllers
	virtual uint32 GetSteamControllerCount() const = 0;

	// Get the time a steam controller was last assigned/used
	virtual float GetLastSteamControllerActiveTime() const = 0;

	// Get the time a non-steam controller was last assigned/used
	virtual float GetLastGamePadControllerActiveTime() const = 0;

	// Get the ID of the steam controller currently sending events to the window
	virtual int GetLastSteamControllerActiveIndex() const = 0;
	
	// Pulse haptic feedback on active gamepad/steam controller if supported
	virtual void PulseActiveControllerHaptic( IUIEngine::EHapticFeedbackPosition ePosition, IUIEngine::EHapticFeedbackStrength eStrength ) = 0;

	// Is finger actively down on steam controller right pad?  Probably means mouse emulation in use.
	virtual bool BIsFingerDownOnSteamControllerRightPad() const = 0;

	// Register a file path to look for keybindings
	virtual void RegisterKeyBindingsFile( const char *pszFilePath ) = 0;

	// Force a reload of the keybindings
	virtual void ReloadKeyBindings() = 0;

	// Private APIs for remote gamepad input, called on a separate network thread
	virtual void RemoteGamepadAttached( int nGamepadID ) = 0;
	virtual void RemoteGamepadDetached( int nGamepadID ) = 0;
	virtual void SetRemoteGamepadAxis( int nGamepadID, int nAxis, int nValue ) = 0;
	virtual void SetRemoteGamepadButton( int nGamepadID, int nButton, int nValue ) = 0;

	// Turn off whatever (wireless) controller was last active
	virtual void TurnOffActiveController() = 0;

	// Get gamepad code value from textual name for config files, event code, etc
	virtual panorama::GamePadCode GamePadCodeFromName( const char * pchGamePadCode ) = 0;

	// Check if two gamepad codes are the 'same' button but on different vendor devices
	virtual bool BIsGamePadCodeEquivalentIgnoringVendor( GamePadCode a, GamePadCode b ) = 0;
};

} // namespace panorama

#endif // IUIINPUT_H
