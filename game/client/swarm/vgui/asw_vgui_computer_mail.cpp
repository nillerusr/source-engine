#include "cbase.h"
#include "asw_vgui_computer_mail.h"
#include "asw_vgui_computer_menu.h"
#include "vgui/ISurface.h"
#include "c_asw_hack_computer.h"
#include "c_asw_computer_area.h"
#include <vgui/IInput.h>
#include <vgui_controls/AnimationController.h>
#include <vgui_controls/ImagePanel.h>
#include "vgui_controls/TextImage.h"
#include "vgui/ILocalize.h"
#include "WrappedLabel.h"
#include "filesystem.h"
#include <keyvalues.h>
#include "controller_focus.h"
#include "vgui_controls\PanelListPanel.h"
#include "ImageButton.h"
#include <vgui_controls/ScrollBar.h>
#include <vgui_controls/Button.h>
#include "asw_vgui_computer_frame.h"
#include "clientmode_asw.h"
#include "c_asw_player.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CASW_VGUI_Computer_Mail::CASW_VGUI_Computer_Mail( vgui::Panel *pParent, const char *pElementName, C_ASW_Hack_Computer* pHackComputer ) 
:	vgui::Panel( pParent, pElementName ),
	CASW_VGUI_Ingame_Panel(),
	m_pHackComputer( pHackComputer )
{
	CASW_VGUI_Computer_Frame *pComputerFrame = dynamic_cast< CASW_VGUI_Computer_Frame* >( GetClientMode()->GetPanelFromViewport( "ComputerContainer/VGUIComputerFrame" ) );
	if ( pComputerFrame )
	{
		pComputerFrame->m_bHideLogoffButton = true;
	}

	m_bMouseOverBackButton = false;

	m_pBackButton = new ImageButton(this, "BackButton", "#asw_SynTekBackButton");
	m_pBackButton->SetContentAlignment(vgui::Label::a_center);
	m_pBackButton->AddActionSignalTarget(this);
	KeyValues *msg = new KeyValues("Command");	
	msg->SetString("command", "Back");
	m_pBackButton->SetCommand(msg->MakeCopy());
	m_pBackButton->SetCancelCommand(msg);
	m_pBackButton->SetAlpha(0);

	if (IsPDA())
	{
		m_pBackButton->SetText("#asw_log_off");
	}
	
	m_pMoreButton = new ImageButton(this, "BackButton", "#asw_SynTekMoreButton");
	m_pMoreButton->AddActionSignalTarget(this);
	KeyValues *moremsg = new KeyValues("Command");	
	moremsg->SetString("command", "More");
	m_pMoreButton->SetCommand(moremsg);
	m_pMoreButton->SetAlpha(0);

	m_pTitleLabel = new vgui::Label(this, "TitleLabel", "#asw_SynTekMail");
	m_pTitleIcon = new vgui::ImagePanel(this, "TitleIcon");
	m_pTitleIconShadow = new vgui::ImagePanel(this, "TitleIconShadow");
	if (IsPDA())
	{
		if (m_pHackComputer && m_pHackComputer->GetComputerArea())
		{
			// set the label based on PDA name
			char namebuffer[64];
			Q_snprintf(namebuffer, sizeof(namebuffer), "%s", m_pHackComputer->GetComputerArea()->m_PDAName.Get());

			wchar_t wnamebuffer[64];
			g_pVGuiLocalize->ConvertANSIToUnicode(namebuffer, wnamebuffer, sizeof( wnamebuffer ));
			
			wchar_t wbuffer[256];		
			g_pVGuiLocalize->ConstructString( wbuffer, sizeof(wbuffer),
				g_pVGuiLocalize->Find("#asw_SynTekAccount"), 1,
					wnamebuffer);
			m_pTitleLabel->SetText(wbuffer);
		}
	}
	
	m_pInboxIcon[0] = new vgui::ImagePanel(this, "InboxIcon");
	m_pInboxIcon[1] = new vgui::ImagePanel(this, "InboxIcon");
	m_pInboxLabel = new vgui::Label(this, "OutboxLabel", "#asw_SynTekMailInbox");
	m_pOutboxLabel = new vgui::Label(this, "OutboxLabel", "#asw_SynTekMailOutbox");
	for (int i=0;i<ASW_MAIL_ROWS;i++)
	{
		m_pMailFrom[i] = new vgui::Label(this, "MailFrom", "#asw_SynTekMailFrom");
		m_pMailDate[i] = new vgui::Label(this, "MailDate", "#asw_SynTekMailDate");
		m_pMailSubject[i] = new ImageButton(this, "MailSubject", "#asw_SynTekMailSubject");
		m_pMailSubject[i]->AddActionSignalTarget(this);
		KeyValues *msg = new KeyValues("Command");	
		char buffer[32];
		Q_snprintf(buffer, sizeof(buffer), "Mail%d\n", i);
		msg->SetString("command", buffer);
		m_pMailSubject[i]->SetCommand(msg);
	}		
	//m_pBodyLabel = new vgui::Label(this, "MailBodyLabel", " ");	
	m_pBodyList = new vgui::PanelListPanel(this, "BodyList");	
	
	for (int i=0;i<4;i++)
	{
		m_pBody[i] = new vgui::WrappedLabel(this, "MailBody", "");
		m_pBodyList->AddItem(NULL, m_pBody[i]);
	}	
	m_pAccountLabel = new vgui::Label(this, "MailAccount", "#asw_SynTekMailAccount");
	m_pName = new vgui::Label(this, "MailName", "");
	
	m_pAccountLabel->SetVisible(false);
	m_pName->SetVisible(false);

	m_pMailKeyValues = NULL;
	ShowMail(0);
	for (int k=0;k<ASW_MAIL_ROWS;k++)
	{
		m_pMailFrom[k]->SetAlpha(0);
		m_pMailDate[k]->SetAlpha(0);
		m_pMailSubject[k]->SetAlpha(0);
	}

	if (GetControllerFocus())
	{
		GetControllerFocus()->AddToFocusList(m_pBackButton);
		GetControllerFocus()->AddToFocusList(m_pMoreButton);
		GetControllerFocus()->SetFocusPanel(m_pBackButton);		
	}

	m_bSetAlpha = false;
}

CASW_VGUI_Computer_Mail::~CASW_VGUI_Computer_Mail()
{
	CASW_VGUI_Computer_Frame *pComputerFrame = dynamic_cast< CASW_VGUI_Computer_Frame* >( GetClientMode()->GetPanelFromViewport( "ComputerContainer/VGUIComputerFrame" ) );
	if ( pComputerFrame )
	{
		pComputerFrame->m_bHideLogoffButton = false;
	}

	if (m_pMailKeyValues)
		m_pMailKeyValues->deleteThis();

	if (GetControllerFocus())
	{
		GetControllerFocus()->RemoveFromFocusList(m_pBackButton);
		GetControllerFocus()->RemoveFromFocusList(m_pMoreButton);
		for (int k=0;k<ASW_MAIL_ROWS;k++)
		{
			GetControllerFocus()->RemoveFromFocusList(m_pMailSubject[k]);
		}
	}
}

void CASW_VGUI_Computer_Mail::ShowMail(int i)
{
	if (i < 0 || i >= ASW_MAIL_ROWS)
	{
		return;
	}
	// make sure our keyvalues are loaded in
	if (!m_pMailKeyValues)
		SetLabelsFromMailFile();

	if (!m_pMailKeyValues)
		return;

	// set the body text
	char buffer[64];
	Q_snprintf(buffer, sizeof(buffer), "MailBody%dA", i+1);
	m_pBody[0]->SetText(m_pMailKeyValues->GetString(buffer));
	Q_snprintf(buffer, sizeof(buffer), "MailBody%dB", i+1);
	m_pBody[1]->SetText(m_pMailKeyValues->GetString(buffer));
	Q_snprintf(buffer, sizeof(buffer), "MailBody%dC", i+1);
	m_pBody[2]->SetText(m_pMailKeyValues->GetString(buffer));
	Q_snprintf(buffer, sizeof(buffer), "MailBody%dD", i+1);
	m_pBody[3]->SetText(m_pMailKeyValues->GetString(buffer));	

	// todo: stop these lines all overlapping

	// highlight the line of the selected mail
	for (int k=0;k<ASW_MAIL_ROWS;k++)
	{
		m_pMailFrom[k]->SetBgColor(Color(0,0,0,96));
		m_pMailDate[k]->SetBgColor(Color(0,0,0,96));
		m_pMailSubject[k]->SetBgColor(Color(0,0,0,96));
	}
	m_pMailFrom[i]->SetBgColor(Color(64,64,64,96));
	m_pMailDate[i]->SetBgColor(Color(64,64,64,96));
	m_pMailSubject[i]->SetBgColor(Color(64,64,64,96));	

	m_iSelectedMail = i;

	// let the server know we looked at a mail
	Q_snprintf(buffer, sizeof(buffer), "cl_viewmail %d", i+1);
	Msg("sending clientcmd %s\n", buffer);
	engine->ClientCmd(buffer);
}

void CASW_VGUI_Computer_Mail::SetLabelsFromMailFile()
{
	if (!m_pHackComputer)
		return;

	C_ASW_Computer_Area *pArea = m_pHackComputer->GetComputerArea();
	if (!pArea)
		return;
	
	char buffer[MAX_PATH];		
	char uilanguage[ 64 ];
	const char* pszMailFile = STRING(pArea->m_MailFile);
	Msg("Mail file is %s\n", pszMailFile);

	// first try to load in a localised mail file
	if (pArea->m_MailFile == NULL_STRING)
		return;
	engine->GetUILanguage( uilanguage, sizeof( uilanguage ) );
	Q_snprintf(buffer, sizeof(buffer), "resource/mail/%s_%s.txt", pszMailFile, uilanguage);
	if ( m_pMailKeyValues )
		m_pMailKeyValues->deleteThis();

	m_pMailKeyValues = new KeyValues( pszMailFile );


	if ( !m_pMailKeyValues->LoadFromFile( filesystem, buffer, "GAME" ) )
	{
		// if we failed, fall back to the english file
		Q_snprintf(buffer, sizeof(buffer), "resource/mail/%s_english.txt", pszMailFile);
		if ( !m_pMailKeyValues->LoadFromFile( filesystem, buffer, "GAME" ) )
		{
			DevMsg(1, "CASW_VGUI_Computer_Mail::SetLabelsFromMailFile failed to load %s\n", buffer);
			return;
		}
	}
	// now set our labels from the keyvalues
	char keybuffer[64];
	for (int i=0;i<ASW_MAIL_ROWS;i++)
	{
		Q_snprintf(keybuffer, sizeof(keybuffer), "MailFrom%d", i+1);
		m_pMailFrom[i]->SetText(m_pMailKeyValues->GetString(keybuffer));
		Q_snprintf(keybuffer, sizeof(keybuffer), "MailDate%d", i+1);
		m_pMailDate[i]->SetText(m_pMailKeyValues->GetString(keybuffer));
		Q_snprintf(keybuffer, sizeof(keybuffer), "MailSubject%d", i+1);
		const char *subject = m_pMailKeyValues->GetString(keybuffer);
		m_pMailSubject[i]->SetText(subject);		
		m_pMailSubject[i]->SetActivationType(ImageButton::ACTIVATE_ONPRESSED);

		if (GetControllerFocus() && Q_strlen(subject) > 0)
		{
			GetControllerFocus()->AddToFocusList(m_pMailSubject[i]);
		}
	}
	//m_pBodyLabel->SetText("#asw_MailBodyLabel");
	m_pBody[0]->SetText(m_pMailKeyValues->GetString("MailBody1A"));
	m_pBody[1]->SetText(m_pMailKeyValues->GetString("MailBody1B"));
	m_pBody[2]->SetText(m_pMailKeyValues->GetString("MailBody1C"));
	m_pBody[3]->SetText(m_pMailKeyValues->GetString("MailBody1D"));
	m_pAccountLabel->SetText(m_pMailKeyValues->GetString("#asw_MailAccount"));
	m_pName->SetText(m_pMailKeyValues->GetString("MailAccountName"));
}

void CASW_VGUI_Computer_Mail::PerformLayout()
{
	m_fScale = ScreenHeight() / 768.0f;

	int w = GetParent()->GetWide();
	int h = GetParent()->GetTall();
	SetWide(w);
	SetTall(h);
	SetPos(0, 0);

	m_pBackButton->SetPos(w * 0.75, h*0.9);
	m_pBackButton->SetSize(128 * m_fScale, 28 * m_fScale);

	m_pMoreButton->SetBounds(w * 0.75 - 136 * m_fScale, h * 0.9, 128 * m_fScale, 28 * m_fScale);
	
	m_pTitleLabel->SetSize(w, h * 0.2f);
	m_pTitleLabel->SetContentAlignment(vgui::Label::a_center);
	m_pTitleLabel->SetPos(0, 0);//h*0.65f);
	m_pTitleLabel->SetZPos(160);
	m_pTitleLabel->InvalidateLayout();
	
	m_pTitleIcon->SetShouldScaleImage(true);
	m_pInboxIcon[0]->SetShouldScaleImage(true);
	m_pInboxIcon[1]->SetShouldScaleImage(true);
	int ix,iy,iw,it;
	//ix = w*0.05f;//w*0.25f;	
	iy = h*0.05f;//h*0.15f;
	iw = w*0.5f;
	it = h*0.5f;
	ix = w * 0.05f - iw;
	m_pTitleIcon->SetPos(ix,iy);
	m_pTitleIcon->SetSize(iw, it);	
	m_pTitleIcon->SetZPos(160);
		
	iw = it = 96 * m_fScale;
	m_pTitleIcon->SetShouldScaleImage(true);
	m_pTitleIcon->SetSize(iw,it);
	m_pTitleIcon->SetZPos(155);
	m_pTitleIconShadow->SetShouldScaleImage(true);
	m_pTitleIconShadow->SetSize(iw * 1.3f, it * 1.3f);
	//m_pTitleIconShadow[i]->SetAlpha(m_pMenuIcon[i]->GetAlpha()*0.5f);
	m_pTitleIconShadow->SetZPos(154);
	m_pTitleIconShadow->SetPos(ix - iw * 0.25f, iy + it * 0.0f);

	const float left_edge = 0.05f * w;
	const float rows_top = 0.2f * h;
	const float row_height = 0.05f * h;
	const float from_width = 0.24f * w;
	const float date_width = 0.24f * w;
	const float subject_width = 0.9f * w - (from_width + date_width);
	const float full_width = 0.9 * w;
	for (int i=0;i<ASW_MAIL_ROWS;i++)
	{
		int row_top = rows_top + row_height * i;
		if (i< 2)
			row_top -= row_height;
		m_pMailFrom[i]->SetPos(left_edge, row_top);
		m_pMailFrom[i]->SetSize(from_width, row_height);
		m_pMailDate[i]->SetPos(left_edge + from_width + 0.02f, row_top);
		m_pMailDate[i]->SetSize(date_width, row_height);
		m_pMailSubject[i]->SetPos(left_edge + from_width + date_width + 0.04f, row_top);
		m_pMailSubject[i]->SetSize(subject_width, row_height);		

		m_pMailFrom[i]->InvalidateLayout();
		m_pMailDate[i]->InvalidateLayout();
		m_pMailSubject[i]->InvalidateLayout();
	}
	int inbox_top = rows_top - row_height * 2;
	int icon_size = row_height;
	m_pInboxLabel->SetBounds(left_edge + icon_size, inbox_top, from_width, row_height);
	m_pOutboxLabel->SetBounds(left_edge + icon_size, inbox_top + row_height * 3, from_width, row_height);
	m_pInboxIcon[0]->SetBounds(left_edge, inbox_top, icon_size, icon_size);
	m_pInboxIcon[1]->SetBounds(left_edge, inbox_top + row_height * 3, icon_size, icon_size);	

	float list_ypos = rows_top + row_height * (ASW_MAIL_ROWS);	
	m_pBodyList->SetFirstColumnWidth(0);
	m_pBodyList->SetPos(left_edge, list_ypos);
	const float ypos = rows_top + row_height * ASW_MAIL_ROWS;
	m_pBodyList->SetSize(full_width, 0.85f * h - ypos);
	for (int i=0;i<4;i++)
	{
		// these get resized these based on their content in onthink
		const float ypos = rows_top + row_height * (ASW_MAIL_ROWS) + i *0.12f;
		//m_pBody[i]->SetPos(left_edge, ypos);
		float body_width = m_pBodyList->GetWide() - (m_pBodyList->GetScrollBar()->GetWide() + 15);
		m_pBody[i]->SetSize(body_width, 0.7f * h - ypos);
		m_pBody[i]->GetTextImage()->SetDrawWidth(body_width);
		int texwide, texttall;
		//m_pBody[i]->GetTextImage()->PerformLayout(true);
		m_pBody[i]->GetTextImage()->GetContentSize(texwide, texttall);
		m_pBody[i]->SetSize(texwide, texttall);
		m_pBody[i]->InvalidateLayout();
	}	
	//m_pBodyLabel->SetPos(left_edge, ypos);
	//m_pBodyLabel->SetSize(full_width, 0.85f * h - ypos);
	const float right_labels_edge = 0.6f * w;
	const float right_labels_width = 0.35f * w;	
	m_pAccountLabel->SetPos(right_labels_edge, 0.05f * h);
	m_pAccountLabel->SetSize(right_labels_width, 0.1f * h);
	m_pName->SetPos(right_labels_edge, 0.1f * h);
	m_pName->SetSize(right_labels_width, 0.1f * h);	

	// make sure all the labels expand to cover the new sizes
	m_pAccountLabel->InvalidateLayout();
	m_pName->InvalidateLayout();
	//m_pBodyLabel->InvalidateLayout();
}


void CASW_VGUI_Computer_Mail::ASWInit()
{
	SetPaintBackgroundType(0);
	SetPaintBackgroundEnabled(false);
	SetBgColor( Color(0,0,0,0) );
	
	SetAlpha(255);

	if (!IsPDA())
	{
		CLocalPlayerFilter filter;
		C_BaseEntity::EmitSound( filter, -1 /*SOUND_FROM_LOCAL_PLAYER*/, "ASWComputer.Mail" );
	}
}

void CASW_VGUI_Computer_Mail::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	SetPaintBackgroundType(0);
	SetPaintBackgroundEnabled(false);
	SetBgColor( Color(0,0,0,0) );
	SetMouseInputEnabled(true);

	vgui::HFont LabelFont = pScheme->GetFont( "Default", IsProportional() );
	
	m_pBackButton->SetFont(LabelFont);
	m_pBackButton->SetPaintBackgroundEnabled(true);
	m_pBackButton->SetContentAlignment(vgui::Label::a_center);
	m_pBackButton->SetBgColor(Color(19,20,40,255));
	m_pBackButton->SetBorders("TitleButtonBorder", "TitleButtonBorder");
	Color white(255,255,255,255);
	Color blue(19,20,40, 255);
	m_pBackButton->SetColors(white, white, white, white, blue);
	m_pBackButton->SetPaintBackgroundType(2);	
	
	m_pMoreButton->SetFont(LabelFont);
	m_pMoreButton->SetPaintBackgroundEnabled(true);
	m_pMoreButton->SetContentAlignment(vgui::Label::a_center);
	m_pMoreButton->SetBgColor(Color(19,20,40,255));
	m_pMoreButton->SetBorders("TitleButtonBorder", "TitleButtonBorder");
	m_pMoreButton->SetColors(white, white, white, white, blue);
	m_pMoreButton->SetPaintBackgroundType(2);	
		
	vgui::HFont LargeTitleFont = pScheme->GetFont( "DefaultLarge", IsProportional() );
	vgui::HFont TitleFont = pScheme->GetFont( "Default", IsProportional() );

	m_pTitleLabel->SetFgColor(Color(255,255,255,255));
	m_pTitleLabel->SetFont(LargeTitleFont);
	m_pTitleLabel->SetContentAlignment(vgui::Label::a_center);
	

	//vgui::HFont SmallFont = pScheme->GetFont("DefaultSmall");
	m_pInboxLabel->SetFgColor(Color(255,255,255,255));
	m_pInboxLabel->SetFont(TitleFont);
	m_pInboxLabel->SetContentAlignment(vgui::Label::a_west);
	m_pInboxLabel->SetPaintBackgroundEnabled(false);
	m_pOutboxLabel->SetFgColor(Color(255,255,255,255));
	m_pOutboxLabel->SetFont(TitleFont);
	m_pOutboxLabel->SetContentAlignment(vgui::Label::a_west);
	m_pOutboxLabel->SetPaintBackgroundEnabled(false);
	ApplySettingAndFadeLabelIn(m_pInboxLabel);
	ApplySettingAndFadeLabelIn(m_pOutboxLabel);

	for (int i=0;i<ASW_MAIL_ROWS;i++)
	{					
		m_pMailFrom[i]->SetFont(LabelFont);
		m_pMailFrom[i]->SetContentAlignment(vgui::Label::a_west);		
		m_pMailDate[i]->SetFont(LabelFont);
		m_pMailDate[i]->SetContentAlignment(vgui::Label::a_west);		
		m_pMailSubject[i]->SetFont(LabelFont);
		m_pMailSubject[i]->SetContentAlignment(vgui::Label::a_west);
		m_pMailSubject[i]->SetBorders("TitleButtonBorder", "TitleButtonBorder");		
		m_pMailSubject[i]->SetColors(white, white, white, white, white);
		ApplySettingAndFadeLabelIn(m_pMailFrom[i]);
		ApplySettingAndFadeLabelIn(m_pMailDate[i]);				
		m_pMailSubject[i]->SetBgColor(Color(0,0,0,96));

		if (!m_bSetAlpha)
		{
			m_pMailSubject[i]->SetAlpha(0);
			vgui::GetAnimationController()->RunAnimationCommand(m_pMailSubject[i], "Alpha", 255, 0, 1.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);
		}
	}
	for (int k=0;k<ASW_MAIL_ROWS;k++)
	{
		m_pMailFrom[k]->SetBgColor(Color(0,0,0,96));
		m_pMailDate[k]->SetBgColor(Color(0,0,0,96));
		m_pMailSubject[k]->SetBgColor(Color(0,0,0,96));
	}
	m_pMailFrom[m_iSelectedMail]->SetBgColor(Color(64,64,64,128));
	m_pMailDate[m_iSelectedMail]->SetBgColor(Color(64,64,64,128));
	m_pMailSubject[m_iSelectedMail]->SetBgColor(Color(64,64,64,128));	

	for (int i=0;i<4;i++)
	{		
		m_pBody[i]->SetFont(LabelFont);
		m_pBody[i]->SetContentAlignment(vgui::Label::a_northwest);
		ApplySettingAndFadeLabelIn(m_pBody[i]);
		m_pBody[i]->SetBgColor(Color(0,0,0,0)); // no bg on each paragraph, the bodylabel fills in a large black area for the mail body text
	}
	//m_pBodyLabel->SetFont(LabelFont);
	//m_pBodyLabel->SetContentAlignment(vgui::Label::a_northwest);
	//ApplySettingAndFadeLabelIn(m_pBodyLabel);

	m_pAccountLabel->SetFont(TitleFont);
	m_pAccountLabel->SetContentAlignment(vgui::Label::a_northwest);
	ApplySettingAndFadeLabelIn(m_pAccountLabel);

	m_pName->SetFont(TitleFont);
	m_pName->SetContentAlignment(vgui::Label::a_northwest);
	ApplySettingAndFadeLabelIn(m_pName);	

	m_pTitleIcon->SetImage("swarm/Computer/IconMail");
	m_pTitleIconShadow->SetImage("swarm/Computer/IconMail");
	m_pInboxIcon[0]->SetImage("swarm/Computer/IconMailSmall");
	m_pInboxIcon[1]->SetImage("swarm/Computer/IconMailSmall");
	
	// fade them in
	if (!m_bSetAlpha)
	{
		m_bSetAlpha = true;
		m_pTitleIcon->SetAlpha(0);
		m_pMoreButton->SetAlpha(0);
		m_pTitleIconShadow->SetAlpha(0);
		m_pTitleLabel->SetAlpha(0);	
		m_pBackButton->SetAlpha(0);
		vgui::GetAnimationController()->RunAnimationCommand(m_pBackButton, "Alpha", 255, 0, 1.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);	
		vgui::GetAnimationController()->RunAnimationCommand(m_pMoreButton, "Alpha", 255, 0, 1.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);	
		m_pInboxIcon[0]->SetAlpha(0);	
		vgui::GetAnimationController()->RunAnimationCommand(m_pInboxIcon[0], "Alpha", 255, 0, 1.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);	
		m_pInboxIcon[1]->SetAlpha(0);	
		vgui::GetAnimationController()->RunAnimationCommand(m_pInboxIcon[1], "Alpha", 255, 0, 1.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);				
		vgui::GetAnimationController()->RunAnimationCommand(m_pTitleLabel, "Alpha", 255, 0.2f, 1.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);
		vgui::GetAnimationController()->RunAnimationCommand(m_pTitleIcon, "Alpha", 255, 0.2f, 1.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);
		vgui::GetAnimationController()->RunAnimationCommand(m_pTitleIconShadow, "Alpha", 30, 0.2f, 1.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);		
	}
}

void CASW_VGUI_Computer_Mail::ApplySettingAndFadeLabelIn(vgui::Label* pLabel)
{	
	pLabel->SetFgColor(Color(255,255,255,255));
	pLabel->SetBgColor(Color(0,0,0,96));
	if (!m_bSetAlpha)
	{
		pLabel->SetAlpha(0);
		vgui::GetAnimationController()->RunAnimationCommand(pLabel, "Alpha", 255, 0, 1.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);
	}
}


void CASW_VGUI_Computer_Mail::OnThink()
{	
	int x,y,w,t;
	GetBounds(x,y,w,t);

	SetPos(0,0);

	m_bMouseOverBackButton = false;

	m_bMouseOverBackButton = m_pBackButton->IsCursorOver();
	
	if (m_bMouseOverBackButton)
	{
		m_pBackButton->SetBgColor(Color(255,255,255,m_pBackButton->GetAlpha()));
	}
	else
	{
		m_pBackButton->SetBgColor(Color(19,20,40,m_pBackButton->GetAlpha()));
	}

	if (m_pMoreButton->IsCursorOver())
	{
		m_pMoreButton->SetBgColor(Color(255,255,255,m_pBackButton->GetAlpha()));
	}
	else
	{
		m_pMoreButton->SetBgColor(Color(19,20,40,m_pBackButton->GetAlpha()));
	}

	if (m_pBodyList && m_pBodyList->GetScrollBar())
	{
		int smin, smax;
		int rw = m_pBodyList->GetScrollBar()->GetRangeWindow();
		m_pBodyList->GetScrollBar()->GetRange(smin, smax);
		if (smax > rw)
		{
			m_pMoreButton->SetVisible(true);
			m_pBodyList->SetShowScrollBar(true);
			m_pBodyList->GetScrollBar()->GetButton(0)->SetVisible(false);
			m_pBodyList->GetScrollBar()->GetButton(1)->SetVisible(false);
			m_pMoreButton->SetPaintBackgroundType(2);
		}
		else
		{
			m_pMoreButton->SetVisible(false);
			m_pBodyList->SetShowScrollBar(false);
		}
	}

	const float rows_top = 0.2f * t;
	const float row_height = 0.05f * t;
	float cursor_y = rows_top + row_height * (ASW_MAIL_ROWS);
	for (int i=0;i<4;i++)
	{
		// resize these based on their content
		int ch = 0;

		float body_width = m_pBodyList->GetWide() - (m_pBodyList->GetScrollBar()->GetWide() + 15);
		m_pBody[i]->SetSize(body_width, m_pBodyList->GetTall());
		m_pBody[i]->GetTextImage()->SetDrawWidth(body_width);
		int texwide, texttall;
		m_pBody[i]->GetTextImage()->GetContentSize(texwide, texttall);		
		m_pBody[i]->SetSize(texwide, texttall);
		m_pBodyList->InvalidateLayout(true);
		cursor_y += ch + 0.01f * t;
	}

	m_fLastThinkTime = gpGlobals->curtime;
}

bool CASW_VGUI_Computer_Mail::MouseClick(int x, int y, bool bRightClick, bool bDown)
{
	if (bDown)
		return true;

	if (m_bMouseOverBackButton)
	{
		if (IsPDA())
		{
			C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
			if (pPlayer)
				pPlayer->StopUsing();
			return true;
		}

		// fade out and reshow menu
		CASW_VGUI_Computer_Menu *pMenu = dynamic_cast<CASW_VGUI_Computer_Menu*>(GetParent());
		if (pMenu)
		{
			pMenu->FadeCurrentPage();
		}		
		return true;
	}
	else
	{
		if (m_pMoreButton->IsCursorOver())
		{
			ScrollMail();
		}
		ScreenToLocal(x, y);
		// if over a mail line, then show that mail
		for (int i=0;i<ASW_MAIL_ROWS;i++)
		{
			int mx, my, mw, mt;
			m_pMailSubject[i]->GetBounds(mx,my,mw,mt);
			//m_pMailSubject[i]->LocalToScreen(mx,my);
			if (y >= my && y <= my + mt)
			{
				ShowMail(i);
				CLocalPlayerFilter filter;
				C_BaseEntity::EmitSound( filter, -1 /*SOUND_FROM_LOCAL_PLAYER*/, "ASWComputer.MenuButton" );
				return true;
			}
		}
	}
	return true;
}

void CASW_VGUI_Computer_Mail::OnCommand(char const* command)
{
	if (!Q_strcmp(command, "Back"))
	{
		if (IsPDA())
		{
			C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
			if (pPlayer)
				pPlayer->StopUsing();
			return;
		}
		// fade out and reshow menu
		CASW_VGUI_Computer_Menu *pMenu = dynamic_cast<CASW_VGUI_Computer_Menu*>(GetParent());
		if (pMenu)
		{
			pMenu->FadeCurrentPage();
		}
		return;
	}
	else if (!Q_strnicmp(command, "Mail", 4))
	{
		int iMail = atoi(command+4);
		ShowMail(iMail);
		CLocalPlayerFilter filter;
		C_BaseEntity::EmitSound( filter, -1 /*SOUND_FROM_LOCAL_PLAYER*/, "ASWComputer.MenuButton" );
		return;
	}
	else if (!Q_strcmp(command, "More"))
	{		
		ScrollMail();
	}
	
	BaseClass::OnCommand(command);
}

void CASW_VGUI_Computer_Mail::ScrollMail()
{
	if (!m_pBodyList || !m_pBodyList->GetScrollBar())
		return;

	int val = m_pBodyList->GetScrollBar()->GetValue();
	m_pBodyList->GetScrollBar()->SetValue(val + m_pBodyList->GetScrollBar()->GetRangeWindow());
	// no more room to scroll down?
	if (m_pBodyList->GetScrollBar()->GetValue() == val)
	{
		// put us back to the top
		m_pBodyList->GetScrollBar()->SetValue(0);
	}	
}

bool CASW_VGUI_Computer_Mail::IsPDA()
{
	if (m_pHackComputer && m_pHackComputer->GetComputerArea())
		return m_pHackComputer->GetComputerArea()->IsPDA();

	return false;
}