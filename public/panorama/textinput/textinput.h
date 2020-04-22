//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef PANORAMA_TEXTINPUT_H
#define PANORAMA_TEXTINPUT_H

#include "panorama/controls/panel2d.h"
#include "panorama/input/iuiinput.h"

namespace panorama
{

class ITextInputControl;
class CTextInputHandlerSettings;

// When text input finished, bool = true for Done, = false for Cancel; char const * = string that user typed
// Dispatched to ITextInputControl::GetAssociatedPanel()
DECLARE_PANEL_EVENT2( TextInputFinished, bool, char const * );

// When the handler is up and sees gamepad right-stick input, it passes it through to the containing
// control, so the control can do something with it. Dispatched to ITextInputControl::GetAssociatedPanel()
DECLARE_PANEL_EVENT1( TextInputAnalogStickPassthrough, GamePadData_t );

panorama::ETextInputHandlerType_t ETextInputHandlerType_tFromName( const char *pchName );
const char *PchNameFromETextInputHandlerType_t( int eType );

//
// Input submodes for text input handlers, used also by textentry
//
enum ETextInputMode_t
{
	k_ETextInputModeNormal,
	k_ETextInputModeNormalLower,
	k_ETextInputModePassword,
	k_ETextInputModeEmail,
	k_ETextInputModeNumeric,
	k_ETextInputModeNumericPassword,
	k_ETextInputModeURL,
	k_ETextInputModeSteamCode,
	k_ETextInputModePhoneNumber,
};

ETextInputMode_t ETextInputMode_tFromName( const char *pchName );
const char *PchNameFromETextInputMode_t( int eMode );

//-----------------------------------------------------------------------------
// Purpose: an interface that the text input uses to feed and be fed text
//-----------------------------------------------------------------------------
class ITextInputControl
{
public:
	virtual ~ITextInputControl() {}

	virtual bool OnKeyDown( const KeyData_t &code ) = 0;
	virtual bool OnKeyUp( const KeyData_t & code ) = 0;
	virtual bool OnKeyTyped( const KeyData_t &unichar ) = 0;
	
	// return true if you own the backing store of the text and can return it immediately on request,
	// 	false otherwise (html returns false here) 
	virtual bool BSupportsImmediateTextReturn() = 0;

	virtual int32 GetCursorOffset() const = 0;
	virtual uint GetCharCount() const = 0;

	virtual const char *PchGetText() const = 0;
	virtual const wchar_t *PwchGetText() const = 0;

	virtual void InsertCharacterAtCursor( const wchar_t &unichar ) = 0;
	virtual void InsertCharactersAtCursor( const wchar_t *pwch, size_t cwch ) = 0;

	virtual CPanel2D *GetAssociatedPanel() = 0;

	// request string the control now contains
	virtual void RequestControlString() = 0;
};


//-----------------------------------------------------------------------------
// Purpose: The interface over a text input handler. Derives from CPanel2D
//			for convenience.
//-----------------------------------------------------------------------------
class CTextInputHandler : public panorama::CPanel2D
{
public:
	CTextInputHandler( panorama::IUIWindow *pParent, const char *pchID );
	CTextInputHandler( panorama::CPanel2D *pParent, const char *pchID );
	virtual ~CTextInputHandler();

	virtual void OpenHandler() = 0;
	void CloseHandler( bool bCommitText );
	virtual ETextInputHandlerType_t GetType() = 0;
	virtual ITextInputControl *GetControlInterface() = 0;
	virtual void SuggestWord( const wchar_t *pwch, int ich ) = 0;
	virtual void SetYButtonAction( const char *pchLabel, IUIEvent *pEvent ) = 0;

protected:
	virtual void CloseHandlerImpl( bool bCommitText ) = 0;
};

// Factory methods
CTextInputHandler *CreateTextInputHandler( panorama::IUIWindow *pParent, const CTextInputHandlerSettings &settings, ITextInputControl *pControl );
CTextInputHandler *CreateTextInputHandler( panorama::CPanel2D *pParent, const CTextInputHandlerSettings &settings, ITextInputControl *pControl );

} // namespace panorama

#endif // PANORAMA_TEXTINPUT_H

