#include "cbase.h"
#include "asw_vgui_computer_tumbler_hack.h"
#include "asw_vgui_computer_menu.h"
#include "vgui/ISurface.h"
#include "c_asw_hack_computer.h"
#include "c_asw_computer_area.h"
#include "c_asw_player.h"
#include <vgui/IInput.h>
#include <vgui_controls/AnimationController.h>
#include <vgui_controls/ImagePanel.h>
#include "vgui_controls/TextImage.h"
#include "vgui/ILocalize.h"
#include "WrappedLabel.h"
#include <filesystem.h>
#include <keyvalues.h>
#include <vgui_controls/ImagePanel.h>
#include "controller_focus.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar asw_tumbler_debug("asw_tumbler_debug", "0", FCVAR_CHEAT, "Debug info on tumbler offsets");

// This class is responsible for displaying the status of the computer hack tumblers.

CASW_VGUI_Computer_Tumbler_Hack::CASW_VGUI_Computer_Tumbler_Hack( vgui::Panel *pParent, const char *pElementName, C_ASW_Hack_Computer* pHackComputer ) 
:	vgui::Panel( pParent, pElementName ),
	CASW_VGUI_Ingame_Panel(),
	m_hHackComputer( pHackComputer )
{
	// create titles, progress bar, exit button

	m_iNumColumns = pHackComputer->m_iNumTumblers;
	m_iColumnHeight = pHackComputer->m_iEntriesPerTumbler;
	m_iFirstIncorrect = -1;

	m_pStatusBg = new vgui::Panel(this, "StatusBG");

	// create tumblers and tumbler controls
	for (int i=0;i<m_iNumColumns;i++)
	{
		CASW_TumblerPanel *pTumbler = new CASW_TumblerPanel(this, "Tumbler", m_iColumnHeight, i);
		if (pTumbler)
		{
			ImageButton *pControl = new ImageButton(this, "TumblerControl", "");
			if (pControl)
			{
				pControl->SetButtonTexture("swarm/Computer/TumblerSwitchDirection");
				pControl->SetButtonOverTexture("swarm/Computer/TumblerSwitchDirection_over");
				//pControl->SetShouldScaleImage(true);
				pControl->SetAlpha(0);
				pControl->AddActionSignalTarget(this);
				KeyValues *msg = new KeyValues("Command");
				char buffer[16];
				Q_snprintf(buffer, sizeof(buffer), "Reverse%d", i);
				msg->SetString("command", buffer);
				pControl->SetCommand(msg);
				m_pTumblerPanels.AddToTail(pTumbler);
				m_pTumblerControls.AddToTail(pControl);
				m_bTumblerControlMouseOver.AddToTail(false);
				if (GetControllerFocus())
				{
					GetControllerFocus()->AddToFocusList(pControl);
					if ( i==0 && ASWInput()->ControllerModeActive() )
						GetControllerFocus()->SetFocusPanel(pControl, false);
				}
			}

			vgui::Panel *pPanel = new vgui::Panel(this, "TumblerStatusBlock");
			if (pPanel)
			{
				pPanel->SetAlpha(0);
				m_pStatusBlocks.AddToTail(pPanel);
				vgui::Label *pStatusLabel = new vgui::Label(this, "TumblerStatusBlockLabel", " ");
				if (pStatusLabel)
				{
					pStatusLabel->SetAlpha(0);
					m_pStatusBlockLabels.AddToTail(pStatusLabel);
				}
			}
		}
	}
	m_iMouseOverTumblerControl = -1;

	m_pTitleLabel = new vgui::Label(this, "TitleLabel", "#asw_hacking_tumbler_title");
	m_pAccessStatusLabel = new vgui::Label(this, "AccessStatus", "#asw_hacking_tumbler_status");
	m_pAlignmentLabel = new vgui::Label(this, "Alignment", "#asw_hacking_tumbler_alignment");
	m_pRejectedLabel = new vgui::WrappedLabel(this, "Rejected", "#asw_hacking_tumbler_rejected_1");	
	m_pAccessLoggedLabel = new vgui::Label(this, "AccessLogged", "#asw_hacking_access_logged");

	m_pTitleLabel->SetContentAlignment(vgui::Label::a_center);
	m_pAccessStatusLabel->SetContentAlignment(vgui::Label::a_center);
	m_pAlignmentLabel->SetContentAlignment(vgui::Label::a_center);
	m_pRejectedLabel->SetContentAlignment(vgui::Label::a_center);
	m_pAccessLoggedLabel->SetContentAlignment(vgui::Label::a_northeast);

	m_pTracePanel = new vgui::Panel(this, "TracePanel");
	m_pFastMarker = new vgui::ImagePanel(this, "FastMarker");
	m_pFastMarker->SetImage("swarm/Hacking/FastMarker");
	m_pFastMarker->SetShouldScaleImage(true);
	m_pFastMarker->SetVisible(false);

	m_iStatusBlocksCorrect = 0;
	UpdateCorrectStatus(true);

	C_BaseEntity::StopSound( -1 /*SOUND_FROM_LOCAL_PLAYER*/, CHAN_STATIC, "ASWComputer.Startup" );
	CLocalPlayerFilter filter;
	C_BaseEntity::EmitSound( filter, -1 /*SOUND_FROM_LOCAL_PLAYER*/, "ASWComputer.StartedHacking" );
}

CASW_VGUI_Computer_Tumbler_Hack::~CASW_VGUI_Computer_Tumbler_Hack()
{
	for (int i=0;i<m_pTumblerPanels.Count();i++)
	{
		if (GetControllerFocus())
		{
			GetControllerFocus()->RemoveFromFocusList(m_pTumblerPanels[i]);
		}
	}
}

float CASW_VGUI_Computer_Tumbler_Hack::GetTracePanelY()
{
	int w = GetParent()->GetWide();
	int h = GetParent()->GetTall();
	SetWide(w);
	SetTall(h);
	SetPos(0, 0);
		
	float status_box_top_edge = 0.204f * h;		// 498
	float status_box_height = 0.612f * h;		// 196x305

	if (m_hHackComputer.Get())
	{
		if (m_hHackComputer->m_iNumTumblers >= 8)
			return status_box_top_edge + status_box_height * 0.15f;
	}
	return status_box_top_edge + status_box_height * 0.2f;
}

void CASW_VGUI_Computer_Tumbler_Hack::PerformLayout()
{
	m_fScale = ScreenHeight() / 768.0f;

	int w = GetParent()->GetWide();
	int h = GetParent()->GetTall();
	SetWide(w);
	SetTall(h);
	SetPos(0, 0);

	vgui::HFont TitleFont = m_pTitleLabel->GetFont();
	int title_tall = vgui::surface()->GetFontTall(TitleFont);
	m_pTitleLabel->SetSize(w, title_tall);
	m_pTitleLabel->SetPos(0, 0.08f * h);

	// position exit button, status blocks and status bg

	// position titles
	float status_box_left_edge = 0.614f * w;		// 664
	float status_box_top_edge = 0.204f * h;		// 498
	float status_box_width = 0.295f * w;
	float status_box_height = 0.612f * h;		// 196x305
	m_pStatusBg->SetSize(status_box_width, status_box_height);
	m_pStatusBg->SetPos(status_box_left_edge, status_box_top_edge);

	m_pTracePanel->SetSize(status_box_width * 0.9f, 4.0f * m_fScale);
	m_pTracePanel->SetPos(status_box_left_edge + status_box_width * 0.05f,
		GetTracePanelY());	

	vgui::HFont DefaultFont = m_pAccessStatusLabel->GetFont();
	int default_tall = vgui::surface()->GetFontTall(DefaultFont);
	m_pAccessStatusLabel->SetSize(w * 0.3f, default_tall * 1.5f);
	m_pAccessStatusLabel->SetPos(status_box_left_edge, status_box_top_edge);
	int padding = 20 * (ScreenWidth() / 1024.0f);
	m_pAlignmentLabel->SetSize(w * 0.7f - padding * 2, default_tall * 1.5f);
	m_pAlignmentLabel->SetPos(padding, status_box_top_edge);		

	// position tumbler panels and controls	
	float column_width = 30.0f * m_fScale;
	float column_spacing = 5.0f * m_fScale;
	float total_width = column_width * m_iNumColumns + column_spacing * ( m_iNumColumns - 1);
	float tumbler_height = column_width * (m_iColumnHeight);	// height of tumbler is number of numbers in it, -2
	//float left_edge = (w - total_width) * 0.5f;		// centers them
	float left_edge = (0.316f * w) - (total_width * 0.5f);
	float tumbler_top_edge = (h - tumbler_height + column_spacing + column_width) * 0.5f;	// centered
	for (int i=0;i<m_iNumColumns;i++)
	{
		m_pTumblerPanels[i]->SetSize(column_width, tumbler_height);

		m_pTumblerControls[i]->SetSize(column_width, column_width);	// square
		m_pTumblerPanels[i]->SetPos(left_edge + (column_width + column_spacing) * i, tumbler_top_edge);
		m_pTumblerControls[i]->SetPos(left_edge + (column_width + column_spacing) * i, tumbler_top_edge + tumbler_height + column_spacing);		
		//m_pTumblerControls[i]->SetImage("swarm/Computer/TumblerSwitchDirection");
	}

	//m_pRejectedLabel->SetSize(w * 0.7f - padding * 2, default_tall * 4.0f);
	m_pRejectedLabel->SetSize(status_box_left_edge - padding * 2, default_tall * 4.0f);
	//m_pRejectedLabel->SetPos(padding, status_box_top_edge + status_box_height);
	m_pRejectedLabel->SetPos(padding, tumbler_top_edge + tumbler_height + column_spacing + column_width);

	// position status blocks
	for (int i=0;i<m_iNumColumns;i++)
	{
		float status_block_x = status_box_width * 0.2f;
		float status_block_y = status_block_x * 0.5f;
		m_pStatusBlocks[i]->SetSize(status_block_x, status_block_y);

		float status_fraction_x = 0.5f;
		if (i==1 || i==4 || i==7)
			status_fraction_x = 0.3f;
		else if (i==2 || i==5 || i==8)
			status_fraction_x = 0.7f;

		float status_pos_x = status_box_left_edge + status_fraction_x * status_box_width - status_block_x * 0.5f;
		int ipos = 0;
		if (i == 1 || i ==2) ipos = 1;
		else if (i == 3) ipos = 2;
		else if (i == 4 || i ==5) ipos = 3;
		else if (i == 6) ipos = 4;
		else if (i >= 7) ipos = 5;
		float status_pos_y = status_box_top_edge + (status_box_height *0.8f) - (ipos * status_block_y * 1.8f);
		m_pStatusBlocks[i]->SetPos(status_pos_x, status_pos_y);

		m_pStatusBlockLabels[i]->SetSize(status_block_x * 2, status_block_y);
		m_pStatusBlockLabels[i]->SetContentAlignment(vgui::Label::a_north);
		m_pStatusBlockLabels[i]->SetPos(status_pos_x + (status_block_x * 0.5f) - (status_block_x),
										status_pos_y + status_block_y * 1.05f);	
		m_pStatusBlockLabels[i]->InvalidateLayout();

		switch (i)
		{
			case 0: m_pStatusBlockLabels[i]->SetText("#asw_status_block_0"); break;
			case 1: m_pStatusBlockLabels[i]->SetText("#asw_status_block_1"); break;
			case 2: m_pStatusBlockLabels[i]->SetText("#asw_status_block_2"); break;
			case 3: m_pStatusBlockLabels[i]->SetText("#asw_status_block_3"); break;
			case 4: m_pStatusBlockLabels[i]->SetText("#asw_status_block_4"); break;
			case 5: m_pStatusBlockLabels[i]->SetText("#asw_status_block_5"); break;
			case 6: m_pStatusBlockLabels[i]->SetText("#asw_status_block_6"); break;
			case 7: m_pStatusBlockLabels[i]->SetText("#asw_status_block_7"); break;
			default: m_pStatusBlockLabels[i]->SetText("Unknown"); break;
		}
	}
	// make sure the last block in the list is always the kernel
	m_pStatusBlockLabels[m_iNumColumns-1]->SetText("#asw_status_block_7");

	m_pAccessLoggedLabel->SetBounds(status_box_left_edge + status_box_width * 0.05f,
									GetTracePanelY() + 10.0f * m_fScale,
									//status_box_top_edge + default_tall * 1.2f,
							status_box_width * 0.9f, default_tall * 1.5f);

	// make sure all the labels expand to cover the new sizes	 
	m_pRejectedLabel->InvalidateLayout();
	m_pAlignmentLabel->InvalidateLayout();
	m_pAccessStatusLabel->InvalidateLayout();
	m_pTitleLabel->InvalidateLayout();
	m_pAccessLoggedLabel->InvalidateLayout();
}

void CASW_VGUI_Computer_Tumbler_Hack::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	SetPaintBackgroundType(0);
	SetPaintBackgroundEnabled(false);
	SetBgColor( Color(0,0,0,0) );
	SetMouseInputEnabled(true);

	vgui::HFont DefaultFont = pScheme->GetFont( "Default", IsProportional() );
	vgui::HFont TitleFont = pScheme->GetFont( "DefaultLarge", IsProportional() );
	vgui::HFont SmallCourier = pScheme->GetFont( "SmallCourier", IsProportional() );

	// apply settings to status blocks, exit button
	// apply settings to status background
	m_pStatusBg->SetPaintBackgroundEnabled(true);
	m_pStatusBg->SetPaintBackgroundType(0);
	m_pStatusBg->SetBgColor( Color(0,0,0,96) );
	// apply settings to our titles
	m_pTitleLabel->SetFgColor(Color(255,255,255,255));
	m_pTitleLabel->SetFont(TitleFont);
	m_pAccessStatusLabel->SetFgColor(Color(255,255,255,255));
	m_pAccessStatusLabel->SetFont(DefaultFont);
	m_pAlignmentLabel->SetFgColor(Color(255,255,255,255));
	m_pAlignmentLabel->SetFont(DefaultFont);
	m_pRejectedLabel->SetFgColor(Color(255,255,255,255));
	m_pRejectedLabel->SetFont(DefaultFont);

	m_pAccessLoggedLabel->SetFgColor(Color(255,0,0,255));
	m_pAccessLoggedLabel->SetFont(SmallCourier);

	m_pTracePanel->SetPaintBackgroundEnabled(true);
	m_pTracePanel->SetPaintBackgroundType(0);
	m_pTracePanel->SetBgColor(Color(0,0,0,192));
	m_pFastMarker->SetDrawColor(Color(255,0,0,255));
	
	
	// apply settings to tumbler controls (tumbler panels do it themselves)
	for (int i=0;i<m_iNumColumns;i++)
	{
		Color white(255,255,255,255);
		m_pTumblerControls[i]->SetColors(white, white, white, white, white);
		m_pTumblerControls[i]->SetBorders("TitleButtonBorder", "TitleButtonBorder");
		m_pStatusBlocks[i]->SetBgColor(Color(32,32,32,255));
		m_pStatusBlocks[i]->SetPaintBackgroundEnabled(true);
		m_pStatusBlocks[i]->SetPaintBackgroundType(0);
		m_pStatusBlockLabels[i]->SetFgColor(Color(255,255,255,255));
		m_pStatusBlockLabels[i]->SetFont(SmallCourier);
	}

	// fade in titles, progress bar and exit button
	// fade in tumbler panels and tumbler controls
	for (int i=0;i<m_iNumColumns;i++)
	{		
		FadePanelIn(m_pTumblerControls[i]);
		FadePanelIn(m_pTumblerPanels[i]);
		FadePanelIn(m_pStatusBlocks[i]);
		FadePanelIn(m_pStatusBlockLabels[i]);
	}
	FadePanelIn(m_pTitleLabel);
	FadePanelIn(m_pAccessStatusLabel);
	FadePanelIn(m_pAlignmentLabel);
	FadePanelIn(m_pRejectedLabel);
	FadePanelIn(m_pStatusBg);
	FadePanelIn(m_pTracePanel);
	FadePanelIn(m_pFastMarker);
	FadePanelIn(m_pAccessLoggedLabel);
	m_pAccessLoggedLabel->SetVisible(false);
	m_pFastMarker->SetVisible(false);	// OnThink will make us visible if appropriate for this hack
}

void CASW_VGUI_Computer_Tumbler_Hack::FadePanelIn(vgui::Panel* pPanel)
{
	pPanel->SetAlpha(0);	
	vgui::GetAnimationController()->RunAnimationCommand(pPanel, "Alpha", 255, 0, 1.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);
}

void CASW_VGUI_Computer_Tumbler_Hack::UpdateCorrectStatus(bool bForce)
{
	float fDiff = 0;
	if (m_hHackComputer.Get())
		fDiff = m_hHackComputer->GetTumblerDiffTime();

	if (fDiff > 0 || bForce)
		return;

	int iStatusBlocksCorrect = 0;
	int iFirstIncorrect = -1;

	for (int i=0;i<m_iNumColumns;i++)
	{
		if (m_hHackComputer.Get() && fDiff <= 0)	// don't adjust tumbler correct status if we're in the wrong timing?
		{
			if (m_hHackComputer->IsTumblerCorrect(i))
			{
				m_pStatusBlocks[i]->SetBgColor(Color(255,255,255,255));
				iStatusBlocksCorrect++;
			}
			else
			{
				m_pStatusBlocks[i]->SetBgColor(Color(32,32,32,255));
				if (iFirstIncorrect == -1)
					iFirstIncorrect = i;
			}
		}
	}

	if (!bForce && iStatusBlocksCorrect != m_iStatusBlocksCorrect)
	{		
		if (iStatusBlocksCorrect > m_iStatusBlocksCorrect)
		{
			if (m_hHackComputer.Get() && m_hHackComputer->GetComputerArea())
			{
				m_hHackComputer->GetComputerArea()->PlayPositiveSound(C_ASW_Player::GetLocalASWPlayer());
			}
		}
		m_iStatusBlocksCorrect = iStatusBlocksCorrect;
	}

	if (iFirstIncorrect != -1 && iFirstIncorrect != m_iFirstIncorrect)
	{
		m_iFirstIncorrect = iFirstIncorrect;
		switch (iFirstIncorrect)
		{
		case 0: m_pRejectedLabel->SetText("#asw_hacking_tumbler_rejected_1"); break;
		case 1: m_pRejectedLabel->SetText("#asw_hacking_tumbler_rejected_2"); break;
		case 2: m_pRejectedLabel->SetText("#asw_hacking_tumbler_rejected_3"); break;
		case 3: m_pRejectedLabel->SetText("#asw_hacking_tumbler_rejected_4"); break;
		case 4: m_pRejectedLabel->SetText("#asw_hacking_tumbler_rejected_5"); break;
		case 5: m_pRejectedLabel->SetText("#asw_hacking_tumbler_rejected_6"); break;
		case 6: m_pRejectedLabel->SetText("#asw_hacking_tumbler_rejected_7"); break;
		case 7: m_pRejectedLabel->SetText("#asw_hacking_tumbler_rejected_8"); break;
		default: break;
		}
	}
}

void CASW_VGUI_Computer_Tumbler_Hack::OnThink()
{	
	int x,y,w,t;
	GetBounds(x,y,w,t);

	// check if the mouse is over any tumbler controls we need to press
	m_iMouseOverTumblerControl = -1;
	
	for (int i=0;i<m_iNumColumns;i++)
	{
		if (m_pTumblerControls[i]->IsCursorOver())
		{
			m_iMouseOverTumblerControl = i;
			m_bTumblerControlMouseOver[i] = true;		
		}
		else if (m_pTumblerPanels[i]->IsCursorOver())
		{
			m_iMouseOverTumblerControl = i;
			m_bTumblerControlMouseOver[i] = true;		
		}
		else
		{
			m_bTumblerControlMouseOver[i] = false;
		}
	}

	UpdateCorrectStatus();

	if (m_hHackComputer.Get() && m_pFastMarker)
	{
		if (m_hHackComputer->m_iNumTumblers < ASW_MIN_TUMBLERS_FAST_HACK)
		{
			m_pFastMarker->SetVisible(false);
		}
		else if (m_hHackComputer->m_fStartedHackTime == 0)
		{
			m_pFastMarker->SetVisible(false);
		}
		else
		{
			m_pFastMarker->SetVisible(true);
			m_pFastMarker->SetSize(12.0f * m_fScale, 12.0f * m_fScale);
			float total_time = m_hHackComputer->m_fFastFinishTime - m_hHackComputer->m_fStartedHackTime;
			if (total_time <= 0)
				total_time = 1.0f;
			float fMarkerFraction = (gpGlobals->curtime - m_hHackComputer->m_fStartedHackTime) / total_time;
			if (fMarkerFraction > 1.0f)
				fMarkerFraction = 1.0f;
			if (fMarkerFraction < 0)
				fMarkerFraction = 0;

			int w = GetWide();
			float status_box_left_edge = 0.614f * w;		// 664
			float status_box_width = 0.295f * w;
						
			m_pFastMarker->SetPos(status_box_left_edge + (status_box_width * 0.05f)
				+ (status_box_width * 0.9f * fMarkerFraction) - (6.0f * m_fScale),
				GetTracePanelY() - 9.0f * m_fScale);	

			if (fMarkerFraction >= 1.0f && !m_pAccessLoggedLabel->IsVisible())
			{
				CLocalPlayerFilter filter;
				C_BaseEntity::EmitSound( filter, -1 /*SOUND_FROM_LOCAL_PLAYER*/, "ASWComputer.Select" );	
				m_pAccessLoggedLabel->SetVisible(true);
			}
		}
	}	

	UpdateTumblerButtonTextures();

	m_pFastMarker->SetDrawColor(Color(255,0,0,255));
	
	m_fLastThinkTime = gpGlobals->curtime;
}

void CASW_VGUI_Computer_Tumbler_Hack::UpdateTumblerButtonTextures()
{
	C_ASW_Hack_Computer *pComputer = m_hHackComputer.Get();
	if ( !pComputer )
		return;

	for (int i=0;i<m_iNumColumns;i++)
	{
		if ( pComputer->m_iTumblerDirection[i] > 0 )
		{
			if ( i == m_iMouseOverTumblerControl )
			{
				m_pTumblerControls[i]->SetButtonTexture("swarm/Computer/TumblerSwitchUPMOUSE");
			}
			else
			{
				m_pTumblerControls[i]->SetButtonTexture("swarm/Computer/TumblerSwitchUP");
			}

			m_pTumblerControls[i]->SetButtonOverTexture("swarm/Computer/TumblerSwitchUPMOUSE");
		}
		else
		{
			if ( i == m_iMouseOverTumblerControl )
			{
				m_pTumblerControls[i]->SetButtonTexture("swarm/Computer/TumblerSwitchDOWNMOUSE");
			}
			else
			{
				m_pTumblerControls[i]->SetButtonTexture("swarm/Computer/TumblerSwitchDOWN");
			}

			m_pTumblerControls[i]->SetButtonOverTexture("swarm/Computer/TumblerSwitchDOWNMOUSE");
		}
	}
}

void CASW_VGUI_Computer_Tumbler_Hack::OnCommand(char const* command)
{
	//Msg("BriefingOptionsPanel row got command: %s\n", command);
	if ( StringHasPrefix( command, "Reverse" ) )
	{
		int iControl = atoi( command + Q_strlen( "Reverse" ) );
		ClickedTumblerControl( iControl );
		return;
	}
	BaseClass::OnCommand(command);
}

bool CASW_VGUI_Computer_Tumbler_Hack::MouseClick(int x, int y, bool bRightClick, bool bDown)
{
	// check for clicking on tumbler controls
	if (m_iMouseOverTumblerControl != -1 && !bDown)
	{
		ClickedTumblerControl(m_iMouseOverTumblerControl);			
	}

	return false;
}

void CASW_VGUI_Computer_Tumbler_Hack::ClickedTumblerControl(int i)
{
	Msg("Clicked on %d\n", i);
	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if (pPlayer)
	{            
		if (i == 0)		pPlayer->SelectTumbler(ASW_IMPULSE_REVERSE_TUMBLER_1);
		else if (i == 1)	pPlayer->SelectTumbler(ASW_IMPULSE_REVERSE_TUMBLER_2);
		else if (i == 2)	pPlayer->SelectTumbler(ASW_IMPULSE_REVERSE_TUMBLER_3);
		else if (i == 3)	pPlayer->SelectTumbler(ASW_IMPULSE_REVERSE_TUMBLER_4);
		else if (i == 4)	pPlayer->SelectTumbler(ASW_IMPULSE_REVERSE_TUMBLER_5);
		else if (i == 5)	pPlayer->SelectTumbler(ASW_IMPULSE_REVERSE_TUMBLER_6);
		else if (i == 6)	pPlayer->SelectTumbler(ASW_IMPULSE_REVERSE_TUMBLER_7);
		else if (i == 7)	pPlayer->SelectTumbler(ASW_IMPULSE_REVERSE_TUMBLER_8);
	}
	// play a sound
	CLocalPlayerFilter filter;
	C_BaseEntity::EmitSound( filter, -1 /*SOUND_FROM_LOCAL_PLAYER*/, "ASWComputer.Select" );	
}

CASW_TumblerPanel::CASW_TumblerPanel( vgui::Panel *pParent, const char *pElementName, int iNumNumbers, int iTumblerIndex )
	:	vgui::Panel( pParent, pElementName )
{
	m_pTumblerHack = dynamic_cast<CASW_VGUI_Computer_Tumbler_Hack*>(pParent);
	if (!m_pTumblerHack)
	{
		Msg("Error, tumbler created without tumbler hack parent");
		return;
	}
	m_iTumblerIndex = iTumblerIndex;
	
	char buffer[64];
	for (int i=0;i<iNumNumbers;i++)
	{
		Q_snprintf(buffer, sizeof(buffer), "%d", i);
		vgui::Label* pLabel = new vgui::Label(this, "TumblerEntry", buffer);
		if (pLabel)
		{
			m_Numbers.AddToTail(pLabel);
		}
	}
	m_pEchoLabel = new vgui::Label(this, "TumblerEntryEcho", buffer);
	m_iEchoEntry = -1;
	m_LastYCoord = -1;
	// todo: make sure it doesn't match up with the previous column?
}

CASW_TumblerPanel::~CASW_TumblerPanel()
{
}

void CASW_TumblerPanel::OnThink()
{
	if (m_pTumblerHack && m_pTumblerHack->m_hHackComputer.Get())
	{
		C_ASW_Hack_Computer *pComputer = m_pTumblerHack->m_hHackComputer.Get();
		if (m_iTumblerIndex < pComputer->m_iNumTumblers)
		{			
			// update the y coord of every label to apply to this offset
			for (int i=0;i<m_Numbers.Count();i++)
			{
				// make sure the number is the right colour
				if (i == pComputer->m_iTumblerCorrectNumber[m_iTumblerIndex])
					m_Numbers[i]->SetFgColor( Color(255,255,255,255) );
				else
					m_Numbers[i]->SetFgColor( Color(64,64,64,255) );

				float number_height = m_Numbers[i]->GetTall();

				// calc the offset for number 0
				//float y_coord = pComputer->m_iTumblerPosition[m_iTumblerIndex] * number_height;	// number position at the integer value of its location
				float y_coord = pComputer->GetTumblerPosition(m_iTumblerIndex) * number_height;	// number position at the integer value of its location				
				if (i==0 && m_iTumblerIndex==0 && asw_tumbler_debug.GetBool())
				{
					Msg("i=%d pos=%d nh=%f dir=%d\n", i, pComputer->GetTumblerPosition(m_iTumblerIndex), number_height,
							pComputer->m_iTumblerDirection[m_iTumblerIndex]);
				}

				// add in the fraction as we approach the move time
				float fDiff = pComputer->GetTumblerDiffTime();// (gpGlobals->curtime - pComputer->GetNextMoveTime());
				if (i==0 && m_iTumblerIndex==0 && asw_tumbler_debug.GetBool())
					Msg("cur=%f move=%f diff=%f ", gpGlobals->curtime, pComputer->GetNextMoveTime(), fDiff);
				if (fDiff > 0)	// we're past the time when we should start sliding.  the hack is now in a 0.5 second long 'sliding' period where reverses aren't allowed
				{
					if (fDiff <= 1.0f)
					{
						if (i==0 && m_iTumblerIndex==0 && asw_tumbler_debug.GetBool())
							Msg("fDiff=%f ", fDiff);
						y_coord += number_height * fDiff * pComputer->m_iTumblerDirection[m_iTumblerIndex];
						
						//if (pComputer->m_iTumblerPosition[m_iTumblerIndex] != m_LastYCoord && m_iTumblerIndex == 0)
						if (pComputer->GetTumblerPosition(m_iTumblerIndex) != m_LastYCoord && m_iTumblerIndex == 0)
						{
							//Msg("New tumbler position = %d, last was = %d\n",
								//pComputer->m_iTumblerPosition[m_iTumblerIndex], m_LastYCoord);
							//m_LastYCoord = pComputer->m_iTumblerPosition[m_iTumblerIndex];
							m_LastYCoord = pComputer->GetTumblerPosition(m_iTumblerIndex);
							CLocalPlayerFilter filter;
							C_BaseEntity::EmitSound( filter, -1 /*SOUND_FROM_LOCAL_PLAYER*/, "ASWComputer.ColumnTick" );
						}
					}
				}
				
				if (i==0 && m_iTumblerIndex==0 && asw_tumbler_debug.GetBool())
					Msg("\n");
				
				// add in the offset of this digit
				y_coord += number_height * i;
							
				// make sure they wrap around
				float max_height = number_height * (m_Numbers.Count());
				if (y_coord >= max_height)
					y_coord -= max_height;
				if (y_coord < 0)
					y_coord += max_height;

				// pull it back one, so there's a number off the top and a number off the bottom
				//y_coord -= number_height;
				
				// if it's at the ver				
				m_Numbers[i]->SetPos(0, y_coord);

				// if it's at the very bottom, show the echo
				max_height -= number_height;
				if (y_coord >= max_height)
				{
					if (!m_pEchoLabel->IsVisible())	// make sure the echo is visible
						m_pEchoLabel->SetVisible(true);					
					if (m_iEchoEntry != i)	// make sure the echo has the same text as the one its echoing
					{
						//m_pEchoLabel->SetPos(0,0);
						//m_pEchoLabel->SetSize(ScreenWidth(),ScreenHeight());
						//char buffer[64];
						//m_Numbers[i]->GetText(buffer, 256);
						
						//Q_snprintf(buffer, sizeof(buffer), "%d", i);
						//m_pEchoLabel->SetText(buffer);

						m_iEchoEntry = i;
						if (i == pComputer->m_iTumblerCorrectNumber[m_iTumblerIndex])
						{
							m_pEchoLabel->SetFgColor( Color(255,255,255,255) );
							m_pEchoLabel->SetText( "1" );
						}
						else
						{
							m_pEchoLabel->SetFgColor( Color(64,64,64,255) );
							m_pEchoLabel->SetText( "0" );
						}

						for (int k=0;k<m_Numbers.Count();k++)
						{
							//Q_snprintf(buffer, sizeof(buffer), "%d", k);
							//m_Numbers[k]->SetText(buffer);
							if ( k == pComputer->m_iTumblerCorrectNumber[m_iTumblerIndex] )
							{
								m_Numbers[k]->SetText( "1" );
							}
							else
							{
								m_Numbers[k]->SetText( "0" );
							}
						}
					}
					m_pEchoLabel->SetPos(0, y_coord - number_height * m_Numbers.Count());	// position the echo at the appropirate top position
					//m_pEchoLabel->SetSize(number_height, number_height);
				}
			}
		}
	}
}


void CASW_TumblerPanel::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	SetPaintBackgroundType(0);
	SetPaintBackgroundEnabled(true);
	SetBgColor( Color(0,0,0,96) );
	SetMouseInputEnabled(false);

	//vgui::HFont LabelFont = pScheme->GetFont("Verdana", true);
	vgui::HFont LabelFont = pScheme->GetFont( "Default", IsProportional() );

	C_ASW_Hack_Computer *pComputer = NULL;
	if (m_pTumblerHack && m_pTumblerHack->m_hHackComputer.Get())
		pComputer = m_pTumblerHack->m_hHackComputer.Get();


	// apply settings to our numbers controls (tumbler panels do it themselves)
	for (int i=0;i<m_Numbers.Count();i++)
	{		
		//m_Numbers[i]->SetPaintBackgroundType(0);
		m_Numbers[i]->SetPaintBackgroundEnabled(false);
		//m_Numbers[i]->SetBgColor( Color(64,0,0,128) );		
		m_Numbers[i]->SetFont(LabelFont);
		m_Numbers[i]->SetContentAlignment(vgui::Label::a_center);
		if (pComputer && i == pComputer->m_iTumblerCorrectNumber[m_iTumblerIndex])
			m_Numbers[i]->SetFgColor( Color(255,255,255,255) );
		else
			m_Numbers[i]->SetFgColor( Color(64,64,64,255) );
	}
	m_pEchoLabel->SetPaintBackgroundEnabled(false);
	m_pEchoLabel->SetFont(LabelFont);
	m_pEchoLabel->SetContentAlignment(vgui::Label::a_center);
	m_pEchoLabel->SetFgColor( Color(64,64,64,255) );
	
	// fade in our numbers
	for (int i=0;i<m_Numbers.Count();i++)
	{		
		FadePanelIn(m_Numbers[i]);
		FadePanelIn(m_pEchoLabel);
	}
}

void CASW_TumblerPanel::FadePanelIn(vgui::Panel* pPanel)
{
	pPanel->SetAlpha(0);	
	vgui::GetAnimationController()->RunAnimationCommand(pPanel, "Alpha", 255, 0, 1.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);
}

void CASW_TumblerPanel::PerformLayout()
{
	BaseClass::PerformLayout();

	float fScale = ScreenHeight() / 768.0f;
	float column_width = 30.0f * fScale;

	for (int i=0;i<m_Numbers.Count();i++)
	{
		m_Numbers[i]->SetSize(column_width, column_width);
	}
	m_pEchoLabel->SetSize(column_width, column_width);
	SetSize(column_width, column_width * m_Numbers.Count());	

	char buffer[64];
	for (int i=0;i<<m_Numbers.Count();i++)
	{
		Q_snprintf(buffer, sizeof(buffer), "%d", i);
		m_Numbers[i]->SetText(buffer);
	}
}
