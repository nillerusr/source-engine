//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Basic button control
//
// $NoKeywords: $
//=============================================================================//

#include <stdio.h>
#include <UtlSymbol.h>

#include <vgui/IBorder.h>
#include <vgui/IInput.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>
#include <vgui/IVGui.h>
#include <vgui/MouseCode.h>
#include <vgui/KeyCode.h>
#include <KeyValues.h>

#include "URLButton.h"
#include <vgui_controls/FocusNavGroup.h>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

DECLARE_BUILD_FACTORY_DEFAULT_TEXT( URLButton, URLButton );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
URLButton::URLButton(Panel *parent, const char *panelName, const char *text, Panel *pActionSignalTarget, const char *pCmd ) : Label(parent, panelName, text)
{
	Init();
	if ( pActionSignalTarget && pCmd )
	{
		AddActionSignalTarget( pActionSignalTarget );
		SetCommand( pCmd );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
URLButton::URLButton(Panel *parent, const char *panelName, const wchar_t *wszText, Panel *pActionSignalTarget, const char *pCmd ) : Label(parent, panelName, wszText)
{
	Init();
	if ( pActionSignalTarget && pCmd )
	{
		AddActionSignalTarget( pActionSignalTarget );
		SetCommand( pCmd );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void URLButton::Init()
{
	_buttonFlags.SetFlag( USE_CAPTURE_MOUSE | BUTTON_BORDER_ENABLED );

	_mouseClickMask = 0;
	_actionMessage = NULL;
	m_bSelectionStateSaved = false;
	SetTextInset(0, 0);
	SetMouseClickEnabled( MOUSE_LEFT, true );
	SetButtonActivationType(ACTIVATE_ONPRESSEDANDRELEASED);

	// labels have this off by default, but we need it on
	SetPaintBackgroundEnabled( true );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
URLButton::~URLButton()
{
	if (_actionMessage)
	{
		_actionMessage->deleteThis();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void URLButton::SetButtonActivationType(ActivationType_t activationType)
{
	_activationType = activationType;
}

//-----------------------------------------------------------------------------
// Purpose: Set button border attribute enabled.
//-----------------------------------------------------------------------------
void URLButton::SetButtonBorderEnabled( bool state )
{
	if ( state != _buttonFlags.IsFlagSet( BUTTON_BORDER_ENABLED ) )
	{
		_buttonFlags.SetFlag( BUTTON_BORDER_ENABLED, state );
		InvalidateLayout(false);
	}
}

//-----------------------------------------------------------------------------
// Purpose:	Set button selected state.
//-----------------------------------------------------------------------------
void URLButton::SetSelected( bool state )
{
	if ( _buttonFlags.IsFlagSet( SELECTED ) != state )
	{
		_buttonFlags.SetFlag( SELECTED, state );
		RecalculateDepressedState();
		InvalidateLayout(false);
	}
}


//-----------------------------------------------------------------------------
// Purpose:	Set button force depressed state.
//-----------------------------------------------------------------------------
void URLButton::ForceDepressed(bool state)
{
	if ( _buttonFlags.IsFlagSet( FORCE_DEPRESSED ) != state )
	{
		_buttonFlags.SetFlag( FORCE_DEPRESSED, state );
		RecalculateDepressedState();
		InvalidateLayout(false);
	}
}

//-----------------------------------------------------------------------------
// Purpose:	Set button depressed state with respect to the force depressed state.
//-----------------------------------------------------------------------------
void URLButton::RecalculateDepressedState( void )
{
	bool newState;
	if (!IsEnabled())
	{
		newState = false;
	}
	else
	{
		newState = _buttonFlags.IsFlagSet( FORCE_DEPRESSED ) ? true : (_buttonFlags.IsFlagSet(ARMED) && _buttonFlags.IsFlagSet( SELECTED ) );
	}

	_buttonFlags.SetFlag( DEPRESSED, newState );
}

//-----------------------------------------------------------------------------
// Purpose: Sets whether or not the button captures all mouse input when depressed
//			Defaults to true
//			Should be set to false for things like menu items where there is a higher-level mouse capture
//-----------------------------------------------------------------------------
void URLButton::SetUseCaptureMouse( bool state )
{
	_buttonFlags.SetFlag( USE_CAPTURE_MOUSE, state );
}

//-----------------------------------------------------------------------------
// Purpose: Check if mouse capture is enabled.
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool URLButton::IsUseCaptureMouseEnabled( void )
{
	return _buttonFlags.IsFlagSet( USE_CAPTURE_MOUSE );
}

//-----------------------------------------------------------------------------
// Purpose:	Set armed state.
//-----------------------------------------------------------------------------
void URLButton::SetArmed(bool state)
{
	if ( _buttonFlags.IsFlagSet( ARMED ) != state )
	{
		_buttonFlags.SetFlag( ARMED, state );
		RecalculateDepressedState();
		InvalidateLayout(false);
	}
}

//-----------------------------------------------------------------------------
// Purpose:	Check armed state
//-----------------------------------------------------------------------------
bool URLButton::IsArmed()
{
	return _buttonFlags.IsFlagSet( ARMED );
}


KeyValues *URLButton::GetActionMessage()
{
	return _actionMessage->MakeCopy();
}



//-----------------------------------------------------------------------------
// Purpose:	Activate a button click.
//-----------------------------------------------------------------------------
void URLButton::DoClick()
{
	SetSelected(true);
	FireActionSignal();
	SetSelected(false);
}

//-----------------------------------------------------------------------------
// Purpose: Check selected state
//-----------------------------------------------------------------------------
bool URLButton::IsSelected()
{
	return _buttonFlags.IsFlagSet( SELECTED );
}

//-----------------------------------------------------------------------------
// Purpose:	Check depressed state
//-----------------------------------------------------------------------------
bool URLButton::IsDepressed()
{
	return _buttonFlags.IsFlagSet( DEPRESSED );
}


//-----------------------------------------------------------------------------
// Drawing focus box?
//-----------------------------------------------------------------------------
bool URLButton::IsDrawingFocusBox()
{
	return _buttonFlags.IsFlagSet( DRAW_FOCUS_BOX );
}

void URLButton::DrawFocusBox( bool bEnable )
{
	_buttonFlags.SetFlag( DRAW_FOCUS_BOX, bEnable );
}

	
//-----------------------------------------------------------------------------
// Purpose:	Paint button on screen
//-----------------------------------------------------------------------------
void URLButton::Paint(void)
{
	BaseClass::Paint();

	int x, y;
	int controlWidth, controlHeight, textWidth, textHeight;
   GetSize(controlWidth, controlHeight);
   GetContentSize(textWidth, textHeight);
   x = textWidth;
   y = controlHeight - 4;
   
   surface()->DrawSetColor(GetButtonFgColor());
   surface()->DrawLine(0, y, x, y); 
}

//-----------------------------------------------------------------------------
// Purpose: Perform graphical layout of button.
//-----------------------------------------------------------------------------
void URLButton::PerformLayout()
{
	// set our color
	SetFgColor(GetButtonFgColor());
	SetBgColor(GetButtonBgColor());

	BaseClass::PerformLayout();
}

//-----------------------------------------------------------------------------
// Purpose: Get button foreground color
// Output : Color
//-----------------------------------------------------------------------------
Color URLButton::GetButtonFgColor()
{
   return _defaultFgColor;
}

//-----------------------------------------------------------------------------
// Purpose: Get button background color
//-----------------------------------------------------------------------------
Color URLButton::GetButtonBgColor()
{
	return _defaultBgColor;
}

//-----------------------------------------------------------------------------
// Purpose: Called when key focus is received
//-----------------------------------------------------------------------------
void URLButton::OnSetFocus()
{
	InvalidateLayout(false);
	BaseClass::OnSetFocus();
}

//-----------------------------------------------------------------------------
// Purpose: Respond when focus is killed
//-----------------------------------------------------------------------------
void URLButton::OnKillFocus()
{
	InvalidateLayout(false);
	BaseClass::OnKillFocus();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void URLButton::ApplySchemeSettings(IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	_defaultFgColor = GetSchemeColor("Button.TextColor", Color(255, 255, 255, 255), pScheme);
	_defaultBgColor = GetSchemeColor("Button.BgColor", Color(0, 0, 0, 255), pScheme);

	InvalidateLayout();
}


//-----------------------------------------------------------------------------
// Purpose: Set button to be mouse clickable or not.
//-----------------------------------------------------------------------------
void URLButton::SetMouseClickEnabled(MouseCode code,bool state)
{
	if(state)
	{
		//set bit to 1
		_mouseClickMask|=1<<((int)(code+1));
	}
	else
	{
		//set bit to 0
		_mouseClickMask&=~(1<<((int)(code+1)));
	}	
}

//-----------------------------------------------------------------------------
// Purpose: sets the command to send when the button is pressed
//-----------------------------------------------------------------------------
void URLButton::SetCommand( const char *command )
{
	SetCommand(new KeyValues("Command", "command", command));
}

//-----------------------------------------------------------------------------
// Purpose: sets the message to send when the button is pressed
//-----------------------------------------------------------------------------
void URLButton::SetCommand( KeyValues *message )
{
	// delete the old message
	if (_actionMessage)
	{
		_actionMessage->deleteThis();
	}

	_actionMessage = message;
}

//-----------------------------------------------------------------------------
// Purpose: Peeks at the message to send when button is pressed
// Input  :  - 
// Output : KeyValues
//-----------------------------------------------------------------------------
KeyValues *URLButton::GetCommand()
{
	return _actionMessage;
}

//-----------------------------------------------------------------------------
// Purpose: Message targets that the button has been pressed
//-----------------------------------------------------------------------------
void URLButton::FireActionSignal()
{
	// message-based action signal
	if (_actionMessage)
	{
		// see if it's a url
		if (!stricmp(_actionMessage->GetName(), "command")
			&& !strnicmp(_actionMessage->GetString("command", ""), "url ", strlen("url "))
			&& strstr(_actionMessage->GetString("command", ""), "://"))
		{
			// it's a command to launch a url, run it
			system()->ShellExecute("open", _actionMessage->GetString("command", "      ") + 4);
		}
		PostActionSignal(_actionMessage->MakeCopy());
	}
}

//-----------------------------------------------------------------------------
// Purpose: gets info about the button
//-----------------------------------------------------------------------------
bool URLButton::RequestInfo(KeyValues *outputData)
{
	if (!stricmp(outputData->GetName(), "GetState"))
	{
		outputData->SetInt("state", IsSelected());
		return true;
	}
	else if ( !stricmp( outputData->GetName(), "GetCommand" ))
	{
		if ( _actionMessage )
		{
			outputData->SetString( "command", _actionMessage->GetString( "command", "" ) );
		}
		else
		{
			outputData->SetString( "command", "" );
		}
		return true;
	}


	return BaseClass::RequestInfo(outputData);
}

//-----------------------------------------------------------------------------
// Purpose: Get control settings for editing
//-----------------------------------------------------------------------------
void URLButton::GetSettings( KeyValues *outResourceData )
{
	BaseClass::GetSettings(outResourceData);

	if (_actionMessage)
	{
		outResourceData->SetString("command", _actionMessage->GetString("command", ""));
	}
	outResourceData->SetInt("default", _buttonFlags.IsFlagSet( DEFAULT_BUTTON ) );
	if ( m_bSelectionStateSaved )
	{
		outResourceData->SetInt( "selected", IsSelected() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void URLButton::ApplySettings( KeyValues *inResourceData )
{
	BaseClass::ApplySettings(inResourceData);

	const char *cmd = inResourceData->GetString("command", "");
	if (*cmd)
	{
		// add in the command
		SetCommand(cmd);
	}

	// saved selection state
	int iSelected = inResourceData->GetInt( "selected", -1 );
	if ( iSelected != -1 )
	{
		SetSelected( iSelected != 0 );
		m_bSelectionStateSaved = true;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Describes editing details
//-----------------------------------------------------------------------------
const char *URLButton::GetDescription( void )
{
	static char buf[1024];
	Q_snprintf(buf, sizeof(buf), "%s, string command, int default", BaseClass::GetDescription());
	return buf;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void URLButton::OnSetState(int state)
{
	SetSelected(state ? true : false);
	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void URLButton::OnCursorEntered()
{
	if (IsEnabled())
	{
		SetArmed(true);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void URLButton::OnCursorExited()
{
	if ( !_buttonFlags.IsFlagSet( BUTTON_KEY_DOWN ) )
	{
		SetArmed(false);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void URLButton::OnMousePressed(MouseCode code)
{
	if (!IsEnabled())
		return;
	
	if (_activationType == ACTIVATE_ONPRESSED)
	{
		if ( IsKeyBoardInputEnabled() )
		{
			RequestFocus();
		}
		DoClick();
		return;
	}


	if (IsUseCaptureMouseEnabled() && _activationType == ACTIVATE_ONPRESSEDANDRELEASED)
	{
		{
			if ( IsKeyBoardInputEnabled() )
			{
				RequestFocus();
			}
			SetSelected(true);
			Repaint();
		}

		// lock mouse input to going to this button
		input()->SetMouseCapture(GetVPanel());
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void URLButton::OnMouseDoublePressed(MouseCode code)
{
	OnMousePressed(code);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void URLButton::OnMouseReleased(MouseCode code)
{
	// ensure mouse capture gets released
	if (IsUseCaptureMouseEnabled())
	{
		input()->SetMouseCapture(NULL);
	}

	if (_activationType == ACTIVATE_ONPRESSED)
		return;


	if (!IsSelected() && _activationType == ACTIVATE_ONPRESSEDANDRELEASED)
		return;

	// it has to be both enabled and (mouse over the button or using a key) to fire
	if ( IsEnabled() && ( GetVPanel() == input()->GetMouseOver() || _buttonFlags.IsFlagSet( BUTTON_KEY_DOWN ) ) )
	{
		DoClick();
	}
	else
	{
		SetSelected(false);
	}

	// make sure the button gets unselected
	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void URLButton::OnKeyCodePressed(KeyCode code)
{
	if (code == KEY_SPACE || code == KEY_ENTER)
	{
		SetArmed(true);
		_buttonFlags.SetFlag( BUTTON_KEY_DOWN );
		OnMousePressed(MOUSE_LEFT);
		if (IsUseCaptureMouseEnabled()) // undo the mouse capture since its a fake mouse click!
		{
			input()->SetMouseCapture(NULL);
		}
	}
	else
	{
		_buttonFlags.ClearFlag( BUTTON_KEY_DOWN );
		BaseClass::OnKeyCodePressed(code);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void URLButton::OnKeyCodeReleased(KeyCode code)
{
	if (_buttonFlags.IsFlagSet( BUTTON_KEY_DOWN ) && (code == KEY_SPACE || code == KEY_ENTER))
	{
		SetArmed(true);
		OnMouseReleased(MOUSE_LEFT);
	}
	else
	{
		BaseClass::OnKeyCodeReleased(code);
	}
	_buttonFlags.ClearFlag( BUTTON_KEY_DOWN );
	SetArmed(false);
}

//-----------------------------------------------------------------------------
// Purpose: Size the object to its button and text.  - only works from in ApplySchemeSettings or PerformLayout()
//-----------------------------------------------------------------------------
void URLButton::SizeToContents()
{
	int wide, tall;
	GetContentSize(wide, tall);
	SetSize(wide + Label::Content, tall + Label::Content);
}

