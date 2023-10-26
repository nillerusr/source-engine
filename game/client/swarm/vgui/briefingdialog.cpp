#include "cbase.h"
#include "BriefingDialog.h"
#include <vgui/vgui.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/TextImage.h>
#include "vgui_controls/frame.h"
#include "iclientmode.h"
#include "vgui_controls\Label.h"
#include <vgui/ILocalize.h>
#include <vgui/ISurface.h>
#include "controller_focus.h"
#include "WrappedLabel.h"
#include "ImageButton.h"
#include "BriefingTooltip.h"
#include "ForceReadyPanel.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

BriefingDialog::BriefingDialog(vgui::Panel *parent, const char *name, const char *message,
		const char *oktext, const char *canceltext, const char *okcmd, const char *cancelcmd)
				: vgui::Panel(parent, name)
{	
	if (g_hBriefingTooltip.Get())
		g_hBriefingTooltip->SetTooltipsEnabled(false);

	if (g_hPopupDialog.Get())
	{
		g_hPopupDialog->MarkForDeletion();
		g_hPopupDialog->SetVisible(false);
		g_hPopupDialog = NULL;
	}
	g_hPopupDialog = this;
		
	Q_snprintf(m_szOkayCommand, sizeof(m_szOkayCommand), "%s", okcmd ? okcmd : "");
	Q_snprintf(m_szCancelCommand, sizeof(m_szCancelCommand), "%s", cancelcmd ? cancelcmd : "");
	
	m_pMessageBox = new vgui::Panel(this, "MessageBox");
	m_pMessage = new vgui::WrappedLabel(this, "Message", message);
	m_pOkayButton = new ImageButton(this, "OkayButton", oktext);
	m_pOkayButton->AddActionSignalTarget(this);	
	KeyValues *msg = new KeyValues("Command");	
	msg->SetString("command", "OkayButton");
	m_pOkayButton->SetCommand(msg);

	m_pCancelButton = new ImageButton(this, "CancelButton", canceltext);
	m_pCancelButton->AddActionSignalTarget(this);	
	KeyValues *msg2 = new KeyValues("Command");	
	msg2->SetString("command", "CancelButton");
	m_pCancelButton->SetCommand(msg2);

	m_pOkayButton->SetButtonTexture("swarm/Briefing/ShadedButton");
	m_pOkayButton->SetButtonOverTexture("swarm/Briefing/ShadedButton_over");	
	m_pOkayButton->SetContentAlignment(vgui::Label::a_center);
	m_pOkayButton->SetEnabled(true);

	m_pCancelButton->SetButtonTexture("swarm/Briefing/ShadedButton");
	m_pCancelButton->SetButtonOverTexture("swarm/Briefing/ShadedButton_over");	
	m_pCancelButton->SetContentAlignment(vgui::Label::a_center);
	m_pCancelButton->SetEnabled(true);

	if (GetControllerFocus())
	{
		GetControllerFocus()->AddToFocusList(m_pOkayButton, false, true);
		GetControllerFocus()->AddToFocusList(m_pCancelButton, false, true);
		GetControllerFocus()->SetFocusPanel(m_pCancelButton, false);
	}

	CLocalPlayerFilter filter;
	C_BaseEntity::EmitSound( filter, -1 /*SOUND_FROM_LOCAL_PLAYER*/, "ASWInterface.Button3" );
}

BriefingDialog::~BriefingDialog()
{
	if (g_hBriefingTooltip.Get())
		g_hBriefingTooltip->SetTooltipsEnabled(true);

	if (g_hPopupDialog.Get() == this)
	{
		g_hPopupDialog = NULL;
	}

	if (GetControllerFocus())
	{
		GetControllerFocus()->RemoveFromFocusList(m_pOkayButton);
		GetControllerFocus()->RemoveFromFocusList(m_pCancelButton);
		//GetControllerFocus()->
	}
}

void BriefingDialog::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	m_pMessageBox->SetBgColor(pScheme->GetColor("DarkBlueTrans", Color(0,0,0,128)));
	m_pMessageBox->SetPaintBackgroundEnabled(true);
	m_pMessageBox->SetPaintBackgroundType(0);

	m_pMessage->SetContentAlignment(vgui::Label::a_northwest);
	m_pMessage->SetFont( pScheme->GetFont( "Default", IsProportional() ) );
	m_pMessage->SetFgColor(pScheme->GetColor("LightBlue", Color(255,255,255,255)));	

	SetBgColor(Color(0,0,0,220));
	SetPaintBackgroundEnabled(true);
	SetPaintBackgroundType(0);
}

void BriefingDialog::PerformLayout()
{
	BaseClass::PerformLayout();

	SetBounds(0, 0, ScreenWidth(), ScreenHeight());

	int text_wide = ScreenWidth() * 0.6f;
	m_pMessage->SetBounds(0, 0, text_wide, ScreenHeight());
	m_pMessage->InvalidateLayout(true);

	int w, t;
	m_pMessage->GetTextImage()->GetContentSize(w, t);
	int text_left = ScreenWidth() * 0.5f - w * 0.5f;
	int text_top = ScreenHeight() * 0.5f - t * 0.5f;
	m_pMessage->SetBounds(text_left, text_top, text_wide, ScreenHeight());

	int padding = 0.02f * ScreenHeight();
	int button_wide = ScreenWidth()*0.16f;
	int button_height = ScreenHeight()*0.04f;
	m_pMessageBox->SetBounds(text_left - padding, text_top - padding, w + padding * 2, t + padding * 3 + button_height);

	int button_top = text_top + t + padding;
	m_pOkayButton->SetBounds(ScreenWidth() * 0.5f - (button_wide + padding), button_top, button_wide, button_height);
	m_pCancelButton->SetBounds(ScreenWidth() * 0.5f + padding, button_top, button_wide, button_height);
}

void BriefingDialog::OnCommand(char const* command)
{
	if (!strcmp(command, "CancelButton"))
	{
		CLocalPlayerFilter filter;
		C_BaseEntity::EmitSound( filter, -1 /*SOUND_FROM_LOCAL_PLAYER*/, "ASWInterface.Button3" );

		if (Q_strlen(m_szCancelCommand) > 0)
		{
			engine->ClientCmd(m_szCancelCommand);

			MarkForDeletion();
			SetVisible(false);
			return;
		}
		else
		{
			MarkForDeletion();
			SetVisible(false);
			// pass on up to parent
			PostActionSignal( new KeyValues("Command", "command", "CancelButton") );
		}
	}
	else if (!strcmp(command, "OkayButton"))
	{
		CLocalPlayerFilter filter;
		C_BaseEntity::EmitSound( filter, -1 /*SOUND_FROM_LOCAL_PLAYER*/, "ASWInterface.Button3" );

		if (Q_strlen(m_szOkayCommand) > 0)
		{
			engine->ClientCmd(m_szOkayCommand);

			MarkForDeletion();
			SetVisible(false);
			return;
		}
		else
		{
			MarkForDeletion();
			SetVisible(false);
			// pass on up to parent
			PostActionSignal( new KeyValues("Command", "command", "OkayButton") );
		}
	}
	BaseClass::OnCommand(command);
}