
#include "cbase.h"
#include "ImageButton.h"
#include <vgui_controls/Label.h>
#include <vgui_controls/ImagePanel.h>
#include <vgui/ISystem.h>
#include <KeyValues.h>
#include <vgui/MouseCode.h>


// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

ImageButton::ImageButton(vgui::Panel *parent, const char *panelName, const char *text) :
	EditablePanel(parent, panelName)
{
	m_pBackdrop = new vgui::ImagePanel(this, "ImageButtonBackdrop");
	m_pLabel = new vgui::Label(this, "ImageButtonLabel", text);		
	ButtonInit();
}

ImageButton::ImageButton(vgui::Panel *parent, const char *panelName, const wchar_t *wszText) :
	EditablePanel(parent, panelName)
{
	m_pBackdrop = new vgui::ImagePanel(this, "ImageButtonBackdrop");
	m_pLabel = new vgui::Label(this, "ImageButtonLabel", wszText);
	ButtonInit();
}

void ImageButton::ButtonInit()
{
	m_Font = vgui::INVALID_FONT;
	m_szButtonTexture[0] = '\0';
	m_szButtonOverTexture[0] = '\0';
	Q_snprintf(m_szBorder, sizeof(m_szBorder), "ASWBriefingButtonBorder");
	Q_snprintf(m_szBorderDisabled, sizeof(m_szBorderDisabled), "ASWBriefingButtonBorderDisabled");
	m_bUsingOverTexture = false;
	m_bButtonEnabled = true;
	m_ActivationType = ACTIVATE_ONRELEASED;
	m_ImageColor = Color(255,255,255,255);
	m_DisabledImageColor = Color(128,128,128,255);
	m_TextColor = Color(64,142,192,255);
	m_DisabledTextColor = Color(32,71,86,255);
	m_MouseOverTextColor = Color(255,255,255,255);
	SetEnabled(true);
	_actionMessage = NULL;
	_cancelMessage = NULL;
	SetMouseInputEnabled(true);
	m_pBackdrop->SetMouseInputEnabled(false);
	m_pLabel->SetMouseInputEnabled(false);
	m_bDoMouseOvers = true;
}

void ImageButton::SetBorders(const char* szBorder, const char *szBorderDisabled)
{
	Q_snprintf(m_szBorder, sizeof(m_szBorder), "%s", szBorder);
	Q_snprintf(m_szBorderDisabled, sizeof(m_szBorderDisabled), "%s", szBorderDisabled);
	UpdateColors();
}


ImageButton::~ImageButton()
{
	
}

void ImageButton::SetButtonTexture(const char *szFilename)
{
	Q_strcpy(m_szButtonTexture, szFilename);
	if ((!IsCursorOver() || !m_bButtonEnabled) && m_pBackdrop)
	{
		m_pBackdrop->SetImage(szFilename);
		m_pBackdrop->SetShouldScaleImage(true);
		m_bUsingOverTexture = false;
	}
}

void ImageButton::SetButtonOverTexture(const char *szFilename)
{
	Q_strcpy(m_szButtonOverTexture, szFilename);
	if (IsCursorOver() && m_bButtonEnabled && m_pBackdrop)
	{
		m_pBackdrop->SetImage(szFilename);
		m_pBackdrop->SetShouldScaleImage(true);
		m_bUsingOverTexture = true;
	}
}

void ImageButton::OnThink()
{	
	BaseClass::OnThink();

	// check our label and backdrop are the right sizes
	int w = GetWide();
	int t = GetTall();

	int bw = m_pBackdrop->GetWide();
	int bt = m_pBackdrop->GetTall();
	if (bw != w || bt != t)
	{
		m_pBackdrop->SetSize(w,t);
	}

	int lw = m_pLabel->GetWide();
	int lt = m_pLabel->GetTall();
	if (lw != w || lt != t)
	{
		m_pLabel->SetSize(w,t);
	}
	
	// check for changing texture
	if (m_bDoMouseOvers)
	{
		if (IsCursorOver() && !m_bUsingOverTexture)
		{
			if (m_pBackdrop && m_szButtonOverTexture[0] != '\0')
				m_pBackdrop->SetImage(m_szButtonOverTexture);
			m_bUsingOverTexture = true;
			UpdateColors();
		}
		if (!IsCursorOver() && m_bUsingOverTexture)
		{
			if (m_pBackdrop && m_szButtonTexture[0] != '\0')
				m_pBackdrop->SetImage(m_szButtonTexture);
			m_bUsingOverTexture = false;
			UpdateColors();
		}
	}
	else
	{
		if (m_bUsingOverTexture)
		{
			if (m_pBackdrop && m_szButtonTexture[0] != '\0')
				m_pBackdrop->SetImage(m_szButtonTexture);
			m_bUsingOverTexture = false;
			UpdateColors();
		}
	}
}

void ImageButton::SetFractionalSize(float xfraction, float yfraction)
{
	SetSize(xfraction*ScreenWidth(), yfraction*ScreenHeight());
	if (m_pBackdrop)
		m_pBackdrop->SetSize(xfraction*ScreenWidth(), yfraction*ScreenHeight());
	if (m_pLabel)
		m_pLabel->SetSize(xfraction*ScreenWidth(), yfraction*ScreenHeight());
}

void ImageButton::SetFont(vgui::HFont font)
{
	m_Font = font;
	m_pLabel->SetFont(m_Font);
}

void ImageButton::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	if (m_Font == vgui::INVALID_FONT)
	{
		m_Font = pScheme->GetFont( "Default", IsProportional() );
	}	
	m_pBackdrop->SetShouldScaleImage(true);
	m_pLabel->SetFont(m_Font);
	//m_pLabel->SetFgColor(Color(255,255,255,255));
	UpdateColors();
	//SetBorder(pScheme->GetBorder("ASWBriefingButtonBorder"));
}

//-----------------------------------------------------------------------------
// Purpose: sets the message to send when the button is pressed
//-----------------------------------------------------------------------------
void ImageButton::SetCommand( KeyValues *message )
{
	// delete the old message
	if (_actionMessage)
	{
		_actionMessage->deleteThis();
	}

	_actionMessage = message;
}

void ImageButton::SetCancelCommand( KeyValues *message )
{
	// delete the old message
	if (_cancelMessage)
	{
		_cancelMessage->deleteThis();
	}

	_cancelMessage = message;
}

//-----------------------------------------------------------------------------
// Purpose: Peeks at the message to send when button is pressed
// Input  :  - 
// Output : KeyValues
//-----------------------------------------------------------------------------
KeyValues *ImageButton::GetCommand()
{
	return _actionMessage;
}

KeyValues *ImageButton::GetCancelCommand()
{
	return _cancelMessage;
}

void ImageButton::OnMousePressed(vgui::MouseCode code)
{
	if (m_ActivationType == ACTIVATE_ONPRESSED)
	{
		if (code == MOUSE_LEFT)
			FireActionSignal();
		else
			FireCancelSignal();
	}
}

void ImageButton::OnMouseReleased(vgui::MouseCode code)
{
	if (m_ActivationType == ACTIVATE_ONRELEASED)
	{
		if (code == MOUSE_LEFT)
			FireActionSignal();
		else
			FireCancelSignal();
	}
}

void ImageButton::SetContentAlignment(vgui::Label::Alignment Alignment)
{
	if (m_pLabel)
		m_pLabel->SetContentAlignment(Alignment);
}

void ImageButton::SetCurrentTextColor(Color col)
{
	if (m_pLabel)
		m_pLabel->SetFgColor(col);
}

void ImageButton::SetColors(Color ImageColor, Color DisabledImageColor, Color TextColor, Color DisabledTextColor, Color MouseOverTextColor)
{
	m_ImageColor = ImageColor;
	m_DisabledImageColor = DisabledImageColor;
	m_TextColor = TextColor;
	m_DisabledTextColor = DisabledTextColor;
	m_MouseOverTextColor = MouseOverTextColor;
	UpdateColors();
}

void ImageButton::UpdateColors()
{
	if (m_bButtonEnabled)
	{
		if (m_pLabel)
		{
			if (!IsCursorOver() || !m_bDoMouseOvers)
				m_pLabel->SetFgColor(m_TextColor);
			else
				m_pLabel->SetFgColor(m_MouseOverTextColor);
		}
		if (m_pBackdrop)
		{
			m_pBackdrop->SetDrawColor(m_ImageColor);
		}
		IScheme *pScheme = scheme()->GetIScheme( GetScheme() );
		if (pScheme)
		{
			int bg = GetPaintBackgroundType();
			SetBorder(pScheme->GetBorder(m_szBorder));
			SetPaintBackgroundType(bg);
		}
	}
	else
	{
		if (m_pLabel)
		{
			m_pLabel->SetFgColor(m_DisabledTextColor);
		}
		if (m_pBackdrop)
		{
			m_pBackdrop->SetDrawColor(m_DisabledImageColor);
		}
		IScheme *pScheme = scheme()->GetIScheme( GetScheme() );
		if (pScheme)
		{
			int bg = GetPaintBackgroundType();
			SetBorder(pScheme->GetBorder(m_szBorderDisabled));
			SetPaintBackgroundType(bg);
		}
	}
}

void ImageButton::SetButtonEnabled(bool bEnabled)
{
	if (bEnabled != m_bButtonEnabled)
	{
		m_bButtonEnabled = bEnabled;
		UpdateColors();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Message targets that the button has been pressed
//-----------------------------------------------------------------------------
void ImageButton::FireActionSignal()
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
			vgui::system()->ShellExecute("open", _actionMessage->GetString("command", "      ") + 4);
		}
		PostActionSignal(_actionMessage->MakeCopy());
	}
}

void ImageButton::FireCancelSignal()
{
	// message-based action signal
	if (_cancelMessage)
	{
		// see if it's a url
		if (!stricmp(_cancelMessage->GetName(), "command")
			&& !strnicmp(_cancelMessage->GetString("command", ""), "url ", strlen("url "))
			&& strstr(_cancelMessage->GetString("command", ""), "://"))
		{
			// it's a command to launch a url, run it
			vgui::system()->ShellExecute("open", _cancelMessage->GetString("command", "      ") + 4);
		}
		PostActionSignal(_cancelMessage->MakeCopy());
	}
}

void ImageButton::SetText(const char *text)
{
	if (m_pLabel)
		m_pLabel->SetText(text);
}

void ImageButton::SetText(const wchar_t *text)
{
	if (m_pLabel)
		m_pLabel->SetText(text);
}