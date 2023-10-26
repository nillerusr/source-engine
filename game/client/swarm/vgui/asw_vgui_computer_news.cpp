#include "cbase.h"
#include "asw_vgui_computer_news.h"
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
#include "asw_vgui_computer_frame.h"
#include "clientmode_asw.h"
#include "ImageButton.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CASW_VGUI_Computer_News::CASW_VGUI_Computer_News( vgui::Panel *pParent, const char *pElementName, C_ASW_Hack_Computer* pHackComputer ) 
:	vgui::Panel( pParent, pElementName ),
	CASW_VGUI_Ingame_Panel(),
	m_pHackComputer( pHackComputer )
{
	CASW_VGUI_Computer_Frame *pComputerFrame = dynamic_cast< CASW_VGUI_Computer_Frame* >( GetClientMode()->GetPanelFromViewport( "ComputerContainer/VGUIComputerFrame" ) );
	if ( pComputerFrame )
	{
		pComputerFrame->m_bHideLogoffButton = true;
	}

	m_bSetAlpha = false;
	m_bMouseOverBackButton = false;

	m_pBackButton = new ImageButton(this, "BackButton", "#asw_SynTekBackButton");
	m_pBackButton->SetContentAlignment(vgui::Label::a_center);
	m_pBackButton->AddActionSignalTarget(this);
	KeyValues *msg = new KeyValues("Command");	
	msg->SetString("command", "Back");
	m_pBackButton->SetCommand(msg->MakeCopy());
	m_pBackButton->SetCancelCommand(msg);
	m_pBackButton->SetAlpha(0);
	m_pTitleLabel = new vgui::Label(this, "TitleLabel", "#asw_SynTekNews");
	m_pTitleIcon = new vgui::ImagePanel(this, "TitleIcon");
	m_pTitleIconShadow = new vgui::ImagePanel(this, "TitleIconShadow");		

	m_pBodyLabel = new vgui::Label(this, "TitleLabel", "");
		
	for (int i=0;i<4;i++)
	{
		m_pBody[i] = new vgui::WrappedLabel(this, "NewsBody", "");
	}
	m_pHeadlineBackdrop = new vgui::Panel(this, "HeadlineBG");
	m_pHeadline = new vgui::Label(this, "Headline", "");
	m_pHeadlineDate = new vgui::Label(this, "HeadlineDate", "");

	m_pKeyValues = NULL;
	SetLabelsFromFile();

	if (GetControllerFocus())
	{
		GetControllerFocus()->AddToFocusList(m_pBackButton);
		GetControllerFocus()->SetFocusPanel(m_pBackButton);
	}
}

CASW_VGUI_Computer_News::~CASW_VGUI_Computer_News()
{
	CASW_VGUI_Computer_Frame *pComputerFrame = dynamic_cast< CASW_VGUI_Computer_Frame* >( GetClientMode()->GetPanelFromViewport( "ComputerContainer/VGUIComputerFrame" ) );
	if ( pComputerFrame )
	{
		pComputerFrame->m_bHideLogoffButton = false;
	}

	if (m_pKeyValues)
		m_pKeyValues->deleteThis();

	if (GetControllerFocus())
	{
		GetControllerFocus()->RemoveFromFocusList(m_pBackButton);
	}
}

void CASW_VGUI_Computer_News::SetLabelsFromFile()
{
	if (!m_pHackComputer)
		return;

	C_ASW_Computer_Area *pArea = m_pHackComputer->GetComputerArea();
	if (!pArea)
		return;
	
	char buffer[MAX_PATH];		
	char uilanguage[ 64 ];
	const char* pszNewsFile = STRING(pArea->m_NewsFile);

	// first try to load in a localised file
	if (pArea->m_NewsFile == NULL_STRING)
		return;
	engine->GetUILanguage( uilanguage, sizeof( uilanguage ) );
	Q_snprintf(buffer, sizeof(buffer), "resource/news/%s_%s.txt", pszNewsFile, uilanguage);
	if ( m_pKeyValues )
		m_pKeyValues->deleteThis();

	m_pKeyValues = new KeyValues( pszNewsFile );

	if ( !m_pKeyValues->LoadFromFile( filesystem, buffer, "GAME" ) )
	{
		// if we failed, fall back to the english file
		Q_snprintf(buffer, sizeof(buffer), "resource/news/%s_english.txt", pszNewsFile);
		if ( !m_pKeyValues->LoadFromFile( filesystem, buffer, "GAME" ) )
		{
			DevMsg(1, "CASW_VGUI_Computer_News::SetLabelsFromFile failed to load %s\n", buffer);
			return;
		}
	}
	// now set our labels from the keyvalues
	char keybuffer[64];
	for (int i=0;i<4;i++)
	{
		Q_snprintf(keybuffer, sizeof(keybuffer), "Paragraph%d", i+1);
		m_pBody[i]->SetText(m_pKeyValues->GetString(keybuffer));		
	}	
	m_pHeadline->SetText(m_pKeyValues->GetString("Headline"));
	m_pHeadlineDate->SetText(m_pKeyValues->GetString("HeadlineDate"));
}

void CASW_VGUI_Computer_News::PerformLayout()
{
	m_fScale = ScreenHeight() / 768.0f;

	int w = GetParent()->GetWide();
	int h = GetParent()->GetTall();
	SetWide(w);
	SetTall(h);
	SetPos(0, 0);

	m_pBackButton->SetPos(w * 0.75, h*0.9);
	m_pBackButton->SetSize(128 * m_fScale, 28 * m_fScale);
	m_pTitleLabel->SetContentAlignment(vgui::Label::a_center);
	
	m_pTitleLabel->SetSize(w, h * 0.2f);
	m_pTitleLabel->SetContentAlignment(vgui::Label::a_center);
	m_pTitleLabel->SetPos(0, 0);//h*0.65f);
	m_pTitleLabel->SetZPos(160);

	const float ypos = 0.3f * h;
	m_pHeadline->SetPos(0.2f*w, 0.15f * h);
	m_pHeadline->SetSize(0.75f*w, 0.1f * h);	
	m_pHeadline->SetContentAlignment(vgui::Label::a_center);
	m_pHeadlineBackdrop->SetPos(0.2f*w, 0.15f * h);
	m_pHeadlineBackdrop->SetSize(0.75f*w, 0.11f * h);	

	m_pHeadlineDate->SetPos(0.2f*w, 0.16f * h);
	m_pHeadlineDate->SetSize(0.75f*w, 0.1f * h);
	m_pHeadlineDate->SetContentAlignment(vgui::Label::a_southeast);
	
	m_pTitleIcon->SetShouldScaleImage(true);
	int ix,iy,iw,it;
	ix = w*0.05f;//w*0.25f;
	iy = h*0.05f;//h*0.15f;
	iw = w*0.5f;
	it = h*0.5f;
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
	//const float row_height = 0.05f * h;	
	const float full_width = 0.9 * w;	
	for (int i=0;i<4;i++)
	{
		// todo: resize these based on their content
		const float yypos = 0.3f * h + i * 0.12f;
		m_pBody[i]->SetPos(left_edge, yypos);
		m_pBody[i]->SetSize(full_width, 0.7f * h - yypos);
		m_pBody[i]->InvalidateLayout();
	}
	
	m_pBodyLabel->SetPos(left_edge, ypos);
	m_pBodyLabel->SetSize(full_width, 0.85f * h - ypos);	

	// make sure all the labels expand to cover the new sizes	 
	m_pBodyLabel->InvalidateLayout();
	m_pTitleLabel->InvalidateLayout();
	m_pHeadline->InvalidateLayout();
	m_pHeadlineDate->InvalidateLayout();
}


void CASW_VGUI_Computer_News::ASWInit()
{
	SetPaintBackgroundType(0);
	SetPaintBackgroundEnabled(false);
	SetBgColor( Color(0,0,0,0) );
	
	SetAlpha(255);

	CLocalPlayerFilter filter;
	C_BaseEntity::EmitSound( filter, -1 /*SOUND_FROM_LOCAL_PLAYER*/, "ASWComputer.News" );
}

void CASW_VGUI_Computer_News::ApplySchemeSettings(vgui::IScheme *pScheme)
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
	m_pBackButton->SetBorders("TitleButtonBorder", "TitleButtonBorder");
	Color white(255,255,255,255);
	Color blue(19,20,40, 255);
	m_pBackButton->SetColors(white, white, white, white, blue);
	m_pBackButton->SetPaintBackgroundType(2);
	
	vgui::HFont LargeTitleFont = pScheme->GetFont( "DefaultLarge", IsProportional() );
	//vgui::HFont TitleFont = pScheme->GetFont("Default");

	m_pTitleLabel->SetFgColor(Color(255,255,255,255));
	m_pTitleLabel->SetFont(LargeTitleFont);
	m_pTitleLabel->SetContentAlignment(vgui::Label::a_center);
			
	for (int i=0;i<4;i++)
	{
		ApplySettingAndFadeLabelIn(m_pBody[i]);
		m_pBody[i]->SetFont(LabelFont);
		m_pBody[i]->SetContentAlignment(vgui::Label::a_northwest);		
		m_pBody[i]->SetBgColor(Color(0,0,0,0)); // no bg on each paragraph, the bodylabel fills in a large black area for the body text
	}
	m_pBodyLabel->SetFont(LabelFont);
	ApplySettingAndFadeLabelIn(m_pBodyLabel);
	m_pBodyLabel->SetContentAlignment(vgui::Label::a_northwest);
	
	m_pHeadline->SetFont(LargeTitleFont);
	ApplySettingAndFadeLabelIn(m_pHeadline);
	m_pHeadline->SetContentAlignment(vgui::Label::a_center);
	m_pHeadline->SetPaintBackgroundEnabled(false);
	m_pHeadline->SetFgColor(Color(255,255,255,255));

	m_pHeadlineDate->SetFont(LabelFont);
	ApplySettingAndFadeLabelIn(m_pHeadlineDate);
	m_pHeadlineDate->SetBgColor(Color(0,0,0,0));

	m_pHeadlineBackdrop->SetPaintBackgroundEnabled(true);	
	m_pHeadlineBackdrop->SetBgColor(Color(0,0,0,96));
	
	m_pTitleIcon->SetImage("swarm/Computer/IconNewsLCD");
	m_pTitleIconShadow->SetImage("swarm/Computer/IconNewsLCD");

	if (m_bSetAlpha)
	{
		m_pBackButton->SetAlpha(0);
		m_pHeadlineBackdrop->SetAlpha(0);
		vgui::GetAnimationController()->RunAnimationCommand(m_pHeadlineBackdrop, "Alpha", 255, 0, 1.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);

		m_pTitleIcon->SetAlpha(0);
		m_pTitleIconShadow->SetAlpha(0);
		m_pTitleLabel->SetAlpha(0);	
		vgui::GetAnimationController()->RunAnimationCommand(m_pBackButton, "Alpha", 255, 0, 1.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);	
	
		vgui::GetAnimationController()->RunAnimationCommand(m_pTitleLabel, "Alpha", 255, 0.2f, 1.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);
		vgui::GetAnimationController()->RunAnimationCommand(m_pTitleIcon, "Alpha", 255, 0.2f, 1.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);
		vgui::GetAnimationController()->RunAnimationCommand(m_pTitleIconShadow, "Alpha", 30, 0.2f, 1.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);		

		m_bSetAlpha = true;
	}
	else
	{
		m_pTitleIconShadow->SetAlpha(30);
		m_pHeadlineBackdrop->SetAlpha(255);
		m_pBodyLabel->SetAlpha(255);
	}
}

void CASW_VGUI_Computer_News::ApplySettingAndFadeLabelIn(vgui::Label* pLabel)
{	
	pLabel->SetFgColor(Color(255,255,255,255));
	pLabel->SetBgColor(Color(0,0,0,96));
	if (!m_bSetAlpha)
	{
		pLabel->SetAlpha(0);
		vgui::GetAnimationController()->RunAnimationCommand(pLabel, "Alpha", 255, 0, 1.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);
	}
}


void CASW_VGUI_Computer_News::OnThink()
{	
	int x,y,w,t;
	GetBounds(x,y,w,t);

	SetPos(0,0);

	//const float full_width = 0.9 * w;
	const float left_edge = 0.05f * w;
	float cursor_y = 0.3f * t;	
	for (int i=0;i<4;i++)
	{
		// todo: resize these based on their content
		m_pBody[i]->SetPos(left_edge, cursor_y);
		//m_pBody[i]->SetSize(full_width, 0.7f * t - ypos);
		int cw, ch;
		m_pBody[i]->GetTextImage()->GetContentSize(cw, ch);
		m_pBody[i]->SetTall(ch);
		cursor_y += ch + 0.01f * t;
	}

	m_bMouseOverBackButton = false;
	m_bMouseOverBackButton = m_pBackButton->IsCursorOver();

	if (m_bMouseOverBackButton)
	{
		m_pBackButton->SetBgColor(Color(255,255,255,255));
		m_pBackButton->SetFgColor(Color(0,0,0,255));
	}
	else
	{
		m_pBackButton->SetBgColor(Color(19,20,40,255));
		m_pBackButton->SetFgColor(Color(255,255,255,255));
	}

	m_fLastThinkTime = gpGlobals->curtime;
}

bool CASW_VGUI_Computer_News::MouseClick(int x, int y, bool bRightClick, bool bDown)
{
	if (m_bMouseOverBackButton && !bDown)
	{
		// fade out and reshow menu
		CASW_VGUI_Computer_Menu *pMenu = dynamic_cast<CASW_VGUI_Computer_Menu*>(GetParent());
		if (pMenu)
		{
			pMenu->FadeCurrentPage();
		}		
		return true;
	}
	return true;
}

void CASW_VGUI_Computer_News::OnCommand(char const* command)
{
	if (!Q_strcmp(command, "Back"))
	{
		// fade out and reshow menu
		CASW_VGUI_Computer_Menu *pMenu = dynamic_cast<CASW_VGUI_Computer_Menu*>(GetParent());
		if (pMenu)
		{
			pMenu->FadeCurrentPage();
		}
		return;
	}
	BaseClass::OnCommand(command);
}