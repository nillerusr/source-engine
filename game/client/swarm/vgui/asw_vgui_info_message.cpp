#include "cbase.h"
#include "asw_vgui_info_message.h"
#include "vgui/ISurface.h"
#include "vgui_controls/TextImage.h"
#include "vgui_controls/Button.h"
#include "asw_info_message_shared.h"
#include <vgui_controls/AnimationController.h>
#include "WrappedLabel.h"
#include <vgui/IInput.h>
#include "ImageButton.h"
#include "controller_focus.h"
#include <vgui_controls/ImagePanel.h>
#include "iclientmode.h"
#include "engine/IEngineSound.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "c_asw_player.h"
#include "iinput.h"
#include "input.h"
#include "asw_input.h"
#include "clientmode_asw.h"
#include "c_user_message_register.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

int g_asw_iGUIWindowsOpen = 0;

vgui::DHANDLE< CASW_VGUI_Info_Message >	g_hCurrentInfoPanel;

int g_LastInfoSoundGUID = -1;
ConVar asw_info_sound_continue("asw_info_sound_continue", "1", FCVAR_ARCHIVE, "Info message sounds continue after you close the window");

void StopInfoMessageSound()
{
	if ( g_LastInfoSoundGUID != -1 )
	{
		enginesound->StopSoundByGuid( g_LastInfoSoundGUID );
		g_LastInfoSoundGUID = -1;
	}
}

CASW_VGUI_Info_Message::CASW_VGUI_Info_Message( vgui::Panel *pParent, const char *pElementName, C_ASW_Info_Message* pMessage ) 
:	vgui::Panel( pParent, pElementName ),
	CASW_VGUI_Ingame_Panel()
{	
	//input->MouseEvent(0, false);	// unclick all our mouse buttons when this panel pops up (so firing doesn't get stuck on)

	if (g_hCurrentInfoPanel.Get())
	{
		g_hCurrentInfoPanel->MarkForDeletion();
		g_hCurrentInfoPanel->SetVisible(false);
	}
	g_hCurrentInfoPanel = this;
	m_hMessage = pMessage;
	for (int i=0;i<4;i++)
	{
		m_pLine[i] = new vgui::WrappedLabel(this, "InfoMessageLabel", "");
		m_pLine[i]->SetContentAlignment(vgui::Label::a_northwest);
		m_pLine[i]->SetMouseInputEnabled(false);
	}	
	m_pOkayButton = new ImageButton(this, "OkayButton", "#asw_close");
	m_pOkayButton->AddActionSignalTarget(this);
	KeyValues *msg = new KeyValues("Command");	
	msg->SetString("command", "OkayButton");
	m_pOkayButton->SetCommand(msg);

	m_pMessageImage = new vgui::ImagePanel(this, "MessageImage");
	m_pMessageImage->SetVisible(false);
	m_pMessageImage->SetShouldScaleImage(true);
	m_szImageName[0] = '\0';
	
	if (ShouldAddLogButton())
	{
		m_pLogButton = new ImageButton(this, "LogButton", "#asw_message_log");
		m_pLogButton->AddActionSignalTarget(this);
		KeyValues *msg = new KeyValues("Command");	
		msg->SetString("command", "MessageLog");
		m_pLogButton->SetCommand(msg);
	}
	else
	{
		Msg("  so not adding it\n");
		m_pLogButton = NULL;
	}

	// find use key bind	
	char lkeybuffer[12];
	Q_snprintf(lkeybuffer, sizeof(lkeybuffer), "%s", ASW_FindKeyBoundTo("+use"));
	Q_strupr(lkeybuffer);

	// copy the found key into wchar_t format (localize it if it's a token rather than a normal keyname)
	wchar_t keybuffer[24];
	if (lkeybuffer[0] == '#')
	{
		const wchar_t *pLocal = g_pVGuiLocalize->Find(lkeybuffer);
		if (pLocal)
			wcsncpy(keybuffer, pLocal, 24);
		else
			g_pVGuiLocalize->ConvertANSIToUnicode(lkeybuffer, keybuffer, sizeof(keybuffer));
	}
	else
		g_pVGuiLocalize->ConvertANSIToUnicode(lkeybuffer, keybuffer, sizeof(keybuffer));

	// look up close text localised
	const wchar_t *pLocal = g_pVGuiLocalize->Find("#asw_close");
	
	if (pLocal)
	{
		// join use key and close text together
		wchar_t buffer[ 256 ];
		g_pVGuiLocalize->ConstructString( buffer, sizeof(buffer), g_pVGuiLocalize->Find("#asw_use_icon_format"), 2, keybuffer, pLocal );
		// set label
		m_pOkayButton->SetText(buffer);
	}

	if (GetControllerFocus())
	{
		GetControllerFocus()->AddToFocusList(m_pOkayButton);
		GetControllerFocus()->SetFocusPanel(m_pOkayButton);
		if (m_pLogButton)
		{
			GetControllerFocus()->AddToFocusList(m_pLogButton);
		}
	}

	m_bClosingMessage = false;	
	
	CLocalPlayerFilter filter;

	// check for a special sound in the info message
	const char *pszSound = pMessage ? pMessage->GetSound() : NULL;	
	if (pszSound && Q_strlen(pszSound) > 0)
	{
		StopInfoMessageSound();

		EmitSound_t ep;
		ep.m_pSoundName = pszSound;
		ep.m_flVolume = 1.0f;
		ep.m_nPitch = PITCH_NORM;
		ep.m_SoundLevel = SNDLVL_NONE;
		ep.m_nChannel = CHAN_STATIC;

		C_BaseEntity::EmitSound( filter, SOUND_FROM_LOCAL_PLAYER, ep ); 
		g_LastInfoSoundGUID = enginesound->GetGuidForLastSoundEmitted();
	}

	UpdateMessage();

	SetAlpha(0);
	vgui::GetAnimationController()->RunAnimationCommand(this, "Alpha", 255, 0.0f, 0.3f, vgui::AnimationController::INTERPOLATOR_LINEAR);	

	SetKeyBoardInputEnabled(true);
	SetMouseInputEnabled(true);
	RequestFocus();

	ASWInput()->SetCameraFixed( true );

	g_asw_iGUIWindowsOpen++;		
}

CASW_VGUI_Info_Message::~CASW_VGUI_Info_Message()
{
	if (GetControllerFocus())
	{
		GetControllerFocus()->RemoveFromFocusList(m_pOkayButton);
		if (m_pLogButton)
			GetControllerFocus()->RemoveFromFocusList(m_pLogButton);
	}
	g_asw_iGUIWindowsOpen--;
	if (g_hCurrentInfoPanel.Get() == this)
		g_hCurrentInfoPanel = NULL;

	NotifyServerOfClose();

	if ( !asw_info_sound_continue.GetBool() )
	{
		StopInfoMessageSound();
	}

	ASWInput()->SetCameraFixed( false );
}

bool CASW_VGUI_Info_Message::ShouldAddLogButton()
{
	Msg("CASW_VGUI_Info_Message::ShouldAddLogButton\n");
	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if (!pPlayer)
		return false;

	// FIXME: Log is busted, so let's not show this button for now
	return false;

	// set/show the message buttons according to the logged handles in player
	int m = GetClientModeASW()->m_InfoMessageLog.Count();
	return (m > 1);
}

#define TEXT_BORDER_X 0.02f
#define TEXT_BORDER_Y 0.03f

void CASW_VGUI_Info_Message::GetMessagePanelSize(int &w, int &t)
{
	w = ScreenWidth() * 0.5f;
	t = ScreenHeight() * 0.5f;
	if (m_hMessage.Get())
	{
		float f = m_hMessage->m_iWindowSize * 0.1f;
		if (f == 0)
			f = 0.5f;
		f = clamp(f, 0.4f, 0.6f);
		w = ScreenWidth() * f;
		t = ScreenHeight() * f;
	}
}

void CASW_VGUI_Info_Message::PerformLayout()
{			
	int w, t;

	GetMessagePanelSize(w, t);	
	SetSize(w, t);
	
	m_fScale = ScreenWidth() / 1024.0f;

	// layout lines	
	float panel_y = ScreenHeight() * TEXT_BORDER_Y;
	char buffer[256];	
	for (int i=0;i<4;i++)
	{
		m_pLine[i]->GetText(buffer, 256);
		if (buffer[0]!=NULL)
		{
			int linew = GetWide()- (ScreenWidth() * TEXT_BORDER_X * 2);
			m_pLine[i]->SetWide(linew);
			int w, h;
			m_pLine[i]->GetTextImage()->GetContentSize(w, h);
			m_pLine[i]->SetTall(h);
			//Msg("setting line %d to %d %d\n", i, linew,  h);			
			int posx = ScreenWidth() * TEXT_BORDER_X;
			int posy = panel_y;
			//Msg(" and pos %d %d\n", posx, posy);
			m_pLine[i]->SetPos(posx, posy);
			panel_y += h + ScreenHeight() * 0.015f;
		}
	}

	// if any image, center it below the text
	if (HasMessageImage())
	{
		int padding = ScreenHeight() * 0.015f;
		int space_left = 200 * m_fScale;
		// assume 8:4 image size
		int image_tall = space_left;
		int image_wide = space_left * 2;

		// if image is wider than panel, reduce size overall
		if (image_wide > (w - (padding * 2)))
		{
			image_wide = (w - (padding * 2));
			image_tall = image_wide / 2;
		}
		int image_y = panel_y + ((space_left * 0.5f) - (image_tall * 0.5f));
		m_pMessageImage->SetBounds((w * 0.5f) - (image_wide * 0.5f), image_y, image_wide, image_tall);

		panel_y = image_y + image_tall;
	}

	int button_wide = 190 * m_fScale;
	int button_high = 40 * m_fScale;		
	int button_y = panel_y + ScreenHeight() * 0.025f;

	if (m_pLogButton)
	{
		m_pOkayButton->SetSize(button_wide, button_high);
		m_pOkayButton->SetPos((w * 0.75f) - (button_wide * 0.5f), button_y);
		m_pLogButton->SetSize(button_wide, button_high);
		m_pLogButton->SetPos((w * 0.25f) - (button_wide * 0.5f), button_y);
	}
	else
	{
		m_pOkayButton->SetSize(button_wide, button_high);
		m_pOkayButton->SetPos((w - button_wide) * 0.5f, button_y);
	}

	t = button_y + button_high * 1.4f;

	SetSize( GetWide(), t );
	SetPos( (ScreenWidth() * 0.5f) - (w * 0.5f), (ScreenHeight() * 0.5) - (t * 0.5f) );
}

void CASW_VGUI_Info_Message::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	SetPaintBackgroundType(0);
	SetPaintBackgroundEnabled(true);
	SetBgColor( Color(16,16,64,128) );	

	for (int i=0;i<4;i++)
	{
		m_pLine[i]->SetFgColor(Color(255,255,255,255));
	}

	m_pOkayButton->SetAlpha(255);
	m_pOkayButton->SetPaintBackgroundEnabled(true);
	m_pOkayButton->SetContentAlignment(vgui::Label::a_center);
	m_pOkayButton->SetBorders("TitleButtonBorder", "TitleButtonBorder");
	Color white(255,255,255,255);
	Color blue(19,20,40, 255);
	m_pOkayButton->SetColors(white, white, white, white, blue);
	m_pOkayButton->SetPaintBackgroundType(2);

	if (m_pLogButton)
	{
		m_pLogButton->SetAlpha(255);
		m_pLogButton->SetPaintBackgroundEnabled(true);
		m_pLogButton->SetContentAlignment(vgui::Label::a_center);
		m_pLogButton->SetBorders("TitleButtonBorder", "TitleButtonBorder");
		Color white(255,255,255,255);
		Color blue(19,20,40, 255);
		m_pLogButton->SetColors(white, white, white, white, blue);
		m_pLogButton->SetPaintBackgroundType(2);
	}

	SetAlpha(0);
	vgui::GetAnimationController()->RunAnimationCommand(this, "Alpha", 255, 0.0f, 0.3f, vgui::AnimationController::INTERPOLATOR_LINEAR);	
}

void CASW_VGUI_Info_Message::UpdateMessage()
{
	if (m_hMessage.Get())
	{
		//SetTitle(m_hMessage->GetTitle(), true);
		m_pLine[0]->SetText(m_hMessage->GetLine1());
		m_pLine[1]->SetText(m_hMessage->GetLine2());
		m_pLine[2]->SetText(m_hMessage->GetLine3());
		m_pLine[3]->SetText(m_hMessage->GetLine4());
		if (HasMessageImage())
		{			
			const char *pNewImage = m_hMessage->GetImageName();
			if (Q_strcmp(pNewImage, m_szImageName))
			{
				m_pMessageImage->SetImage(pNewImage);
				Q_strcpy(m_szImageName, pNewImage);
			}
			m_pMessageImage->SetVisible(true);
		}
		else
		{
			m_szImageName[0] = '\0';
			m_pMessageImage->SetVisible(false);
		}
	}
}

bool CASW_VGUI_Info_Message::HasMessageImage()
{
	if (!m_hMessage.Get())
		return false;

	if (!m_hMessage->GetImageName())
		return false;

	if (Q_strlen(m_hMessage->GetImageName()) < 1)
		return false;

	return true;
}

void CASW_VGUI_Info_Message::OnThink()
{
	UpdateMessage();

	int x,y,w,t;
	GetBounds(x,y,w,t);
	
	InvalidateLayout(true);

	if (m_pOkayButton->IsCursorOver())
	{
		m_pOkayButton->SetBgColor(Color(255,255,255,200));
		m_pOkayButton->SetFgColor(Color(0,0,0,255));
	}
	else
	{
		m_pOkayButton->SetBgColor(Color(19,20,40,200));
		m_pOkayButton->SetFgColor(Color(255,255,255,255));
	}

	if (m_pLogButton)
	{
		if (m_pLogButton->IsCursorOver())
		{
			m_pLogButton->SetBgColor(Color(255,255,255,200));
			m_pLogButton->SetFgColor(Color(0,0,0,255));
		}
		else
		{
			m_pLogButton->SetBgColor(Color(19,20,40,200));
			m_pLogButton->SetFgColor(Color(255,255,255,255));
		}
	}
	
	if (m_bClosingMessage && GetAlpha() <= 0)
	{
		SetVisible(false);
		MarkForDeletion();
	}
}

bool CASW_VGUI_Info_Message::MouseClick(int x, int y, bool bRightClick, bool bDown)
{
	if (bDown)
		return true;

	if (m_pOkayButton->IsCursorOver())
	{
		CloseMessage();
	}
	else if (m_pLogButton && m_pLogButton->IsCursorOver())
	{
		C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
		if (pPlayer)
			pPlayer->ShowMessageLog();
	}
	
	return true;	// always swallow clicks in our window
}

void CASW_VGUI_Info_Message::CloseMessage()
{
	// fade out and reshow menu
	m_bClosingMessage = true;
	vgui::GetAnimationController()->RunAnimationCommand(this, "Alpha", 0, 0.0f, 0.3f, vgui::AnimationController::INTERPOLATOR_LINEAR);		
	CLocalPlayerFilter filter;
	C_BaseEntity::EmitSound( filter, -1 /*SOUND_FROM_LOCAL_PLAYER*/, "ASWInterface.Button3" );	
}

void CASW_VGUI_Info_Message::NotifyServerOfClose()
{
	char buffer[64];
	Q_snprintf(buffer, sizeof(buffer), "cl_mread %d", m_hMessage.Get() ? m_hMessage->entindex() : -1 );
	engine->ClientCmd(buffer);
}

void CASW_VGUI_Info_Message::OnCommand(char const* command)
{
	if (!Q_strcmp(command, "OkayButton"))
	{
		CloseMessage();
		return;
	}
	else if (!Q_strcmp(command, "MessageLog"))
	{
		C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
		if (pPlayer)
			pPlayer->ShowMessageLog();
		return;
	}
	BaseClass::OnCommand(command);
}

bool CASW_VGUI_Info_Message::CloseInfoMessage()
{
	if (g_asw_iGUIWindowsOpen <= 0)
		return false;

	bool bFound = false;
	for (int k=0;k<GetClientMode()->GetViewport()->GetChildCount();k++)
	{
		Panel *pChild = GetClientMode()->GetViewport()->GetChild(k);
		if (pChild && 
			( !strcmp(pChild->GetName(), "InfoMessageWindow") || !strcmp(pChild->GetName(), "InfoMessageLog") )
			)
		{
			pChild->MarkForDeletion();
			pChild->SetVisible(false);
			bFound = true;
		}
	}
	return bFound;
}

bool CASW_VGUI_Info_Message::HasInfoMessageOpen()
{
	return (g_asw_iGUIWindowsOpen > 0);
}

// =======================
// Message Log
// =======================

CASW_VGUI_Message_Log::CASW_VGUI_Message_Log( vgui::Panel *pParent, const char *pElementName ) 
:	CASW_VGUI_Info_Message( pParent, pElementName, NULL )
{
	for (int i=0;i<ASW_MAX_LOGGED_MESSAGES;i++)
	{
		m_pMessageButton[i] = new ImageButton(this, "MsgButton", " ");
		m_pMessageButton[i]->AddActionSignalTarget(this);
		KeyValues *msg = new KeyValues("Command");
		char buffer[24];
		Q_snprintf(buffer, sizeof(buffer), "MsgButton%d", i);
		msg->SetString("command", buffer);
		m_pMessageButton[i]->SetCommand(msg);
	}

	if (GetControllerFocus())
	{
		for (int i=0;i<ASW_MAX_LOGGED_MESSAGES;i++)
		{
			GetControllerFocus()->AddToFocusList(m_pMessageButton[i]);			
		}
		GetControllerFocus()->SetFocusPanel(m_pMessageButton[0]);
	}
}

CASW_VGUI_Message_Log::~CASW_VGUI_Message_Log()
{
	if (GetControllerFocus())
	{
		for (int i=0;i<ASW_MAX_LOGGED_MESSAGES;i++)
		{
			GetControllerFocus()->RemoveFromFocusList(m_pMessageButton[i]);
		}
	}
}

void CASW_VGUI_Message_Log::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	Color white(255,255,255,255);
		Color blue(19,20,40, 255);

	for (int i=0;i<ASW_MAX_LOGGED_MESSAGES;i++)
	{
		if (!m_pMessageButton)
			continue;

		m_pMessageButton[i]->SetAlpha(255);
		m_pMessageButton[i]->SetPaintBackgroundEnabled(true);
		m_pMessageButton[i]->SetContentAlignment(vgui::Label::a_center);
		m_pMessageButton[i]->SetBorders("TitleButtonBorder", "TitleButtonBorder");		
		m_pMessageButton[i]->SetColors(white, white, white, white, blue);
		m_pMessageButton[i]->SetPaintBackgroundType(2);
	}
}

void CASW_VGUI_Message_Log::OnCommand(char const* command)
{
	if (!Q_strncmp(command, "MsgButton", 9))
	{
		int index = atoi(command + 9);
		ShowLoggedMessage(index);
		return;
	}
	BaseClass::OnCommand(command);
}

void CASW_VGUI_Message_Log::ShowLoggedMessage(int index)
{
	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if (!pPlayer)
		return;

	if (index < 0 || index >= GetClientModeASW()->m_InfoMessageLog.Count())
		return;

	pPlayer->ShowPreviousInfoMessage(GetClientModeASW()->m_InfoMessageLog[index].Get());
}

void CASW_VGUI_Message_Log::GetMessagePanelSize(int &w, int &t)
{
	w = ScreenWidth() * 0.5f;
	t = ScreenHeight() * 0.8f;	
}

void CASW_VGUI_Message_Log::PerformLayout()
{	
	BaseClass::PerformLayout();

	// sizes uses by baseclass
	int w = GetWide();
	int t = GetTall();
	m_fScale = ScreenWidth() / 1024.0f;
	float panel_y = ScreenHeight() * TEXT_BORDER_Y;
	int button_high = 40 * m_fScale;

	// how much space do we have for message buttons?
	int message_y_space = ( (t - button_high * 1.7f) - (ScreenHeight() * TEXT_BORDER_Y) ) - panel_y;
	int padding = 5 * m_fScale;
	int message_y_per_button = (message_y_space / float(ASW_MAX_LOGGED_MESSAGES));
	int pos_x = ScreenWidth() * TEXT_BORDER_X;

	// position message buttons
	for (int i=0;i<ASW_MAX_LOGGED_MESSAGES;i++)
	{
		if (!m_pMessageButton[i])
			continue;

		m_pMessageButton[i]->SetBounds(pos_x, panel_y, w - (pos_x * 2), message_y_per_button - padding);
		panel_y += message_y_per_button;
	}
}

void CASW_VGUI_Message_Log::UpdateMessage()
{
	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if (!pPlayer)
		return;

	// set/show the message buttons according to the logged handles in player
	int m = GetClientModeASW()->m_InfoMessageLog.Count();
	for (int i=0;i<ASW_MAX_LOGGED_MESSAGES;i++)
	{
		if (i < m)
		{			
			C_ASW_Info_Message *pMessage = GetClientModeASW()->m_InfoMessageLog[i].Get();
			if (!pMessage)
			{
				m_pMessageButton[i]->SetVisible(false);
			}
			else
			{
				char buffer[255];
				Q_snprintf(buffer, sizeof(buffer), "%d. %s", i + 1, pMessage->GetTitle());
				m_pMessageButton[i]->SetText(buffer);
				m_pMessageButton[i]->SetVisible(true);
			}
		}
		else
		{
			m_pMessageButton[i]->SetVisible(false);
		}
	}
}

void CASW_VGUI_Message_Log::OnThink()
{
	BaseClass::OnThink();

	if (m_pLogButton)
		m_pLogButton->SetVisible(false);

	// highlight the message that the mouse button is over?
	for (int i=0;i<ASW_MAX_LOGGED_MESSAGES;i++)
	{
		if (m_pMessageButton[i] && m_pMessageButton[i]->IsVisible() && m_pMessageButton[i]->GetAlpha() > 0)
		{
			if (m_pMessageButton[i]->IsCursorOver())
			{
				m_pMessageButton[i]->SetBgColor(Color(255,255,255,128));
				m_pMessageButton[i]->SetFgColor(Color(0,0,0,255));
			}
			else
			{
				m_pMessageButton[i]->SetBgColor(Color(19,20,40,0));
				m_pMessageButton[i]->SetFgColor(Color(255,255,255,255));
			}
		}
	}
}

bool CASW_VGUI_Message_Log::MouseClick(int x, int y, bool bRightClick, bool bDown)
{
	if (bDown)
		return true;

	for (int i=0;i<ASW_MAX_LOGGED_MESSAGES;i++)
	{
		if (m_pMessageButton[i] && m_pMessageButton[i]->IsVisible() && m_pMessageButton[i]->GetAlpha() > 0
			&& m_pMessageButton[i]->IsCursorOver())
		{
			ShowLoggedMessage(i);
			return true;
		}
	}

	return BaseClass::MouseClick(x, y, bRightClick, bDown);
}

bool CASW_VGUI_Message_Log::ShouldAddLogButton()
{
	return false;
}

void __MsgFunc_ASWStopInfoMessageSound( bf_read &msg )
{
	StopInfoMessageSound();
}
USER_MESSAGE_REGISTER( ASWStopInfoMessageSound );