#include "cbase.h"
#include "asw_vgui_computer_menu.h"
#include "asw_vgui_computer_camera.h"
#include "asw_vgui_computer_plant.h"
#include "asw_vgui_computer_mail.h"
#include "asw_vgui_computer_news.h"
#include "asw_vgui_computer_stocks.h"
#include "asw_vgui_computer_weather.h"
#include "asw_vgui_computer_frame.h"
#include "asw_vgui_computer_download_docs.h"
#include "asw_vgui_computer_tumbler_hack.h"
#include "vgui/ISurface.h"
#include "c_asw_hack_computer.h"
#include "c_asw_computer_area.h"
#include "c_asw_player.h"
#include <vgui/IInput.h>
#include <vgui_controls/AnimationController.h>
#include <vgui_controls/ImagePanel.h>
#include "vgui/ILocalize.h"
#include "iclientmode.h"
#include "WrappedLabel.h"
#include <vgui/mousecode.h>
#include "controller_focus.h"
#include <vgui_controls/Button.h>
#include <vgui_controls/TextImage.h>
#include "ImageButton.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CASW_VGUI_Computer_Menu::CASW_VGUI_Computer_Menu( vgui::Panel *pParent, const char *pElementName, C_ASW_Hack_Computer* pHackComputer ) 
:	vgui::Panel( pParent, pElementName ),
	CASW_VGUI_Ingame_Panel(),
	m_pHackComputer( pHackComputer )
{
	for (int i=0;i<ASW_COMPUTER_MAX_MENU_ITEMS;i++)
	{		
		m_pMenuLabel[i] = new ImageButton(this, "ComputerMenuLabel", "");
		m_pMenuLabel[i]->AddActionSignalTarget(this);
		
		KeyValues *msg = new KeyValues("Command");
		char buffer[16];
		Q_snprintf(buffer, sizeof(buffer), "Option%d", i);
		msg->SetString("command", buffer);
		m_pMenuLabel[i]->SetCommand(msg);

		KeyValues *cmsg = new KeyValues("Command");			
		cmsg->SetString( "command", "Cancel" );
		m_pMenuLabel[i]->SetCancelCommand( cmsg );

		m_pMenuIcon[i] = new vgui::ImagePanel(this, "ComputerMenuIcon");
		m_pMenuIconShadow[i] = new vgui::ImagePanel(this, "ComputerMenuIconShadow");		
	}

	m_pBlackBar[0] = new vgui::ImagePanel(this, "ComputerMenuBar1");
	m_pBlackBar[1] = new vgui::ImagePanel(this, "ComputerMenuBar2");
	m_hCurrentPage = NULL;
	m_bFadingCurrentPage = false;
	m_iPrepareHackOption = -1;
	m_iMouseOverOption = -1;
	m_iOldMouseOverOption = -1;
	m_iPreviousHackOption = 0;
	m_bSetAlpha = false;
	m_iAutodownload = -1;

	m_pAccessDeniedLabel = new vgui::Label(this, "AccessDeniedLabel", "#asw_computer_access_denied");
	m_pAccessDeniedLabel->SetContentAlignment(vgui::Label::a_center);
	m_pInsufficientRightsLabel = new vgui::Label(this, "AccessDeniedLabel", "#asw_computer_insufficient_rights");	
}

CASW_VGUI_Computer_Menu::~CASW_VGUI_Computer_Menu()
{
	if (GetControllerFocus())
	{
		for (int i=0;i<ASW_COMPUTER_MAX_MENU_ITEMS;i++)
		{
			GetControllerFocus()->RemoveFromFocusList(m_pMenuLabel[i]);
		}
	}
}

void CASW_VGUI_Computer_Menu::PerformLayout()
{
	m_fScale = ScreenHeight() / 768.0f;

	int w = GetParent()->GetWide();
	int h = GetParent()->GetTall();
	SetWide(w);
	SetTall(h);
	SetPos(0, 0);

	if (m_hCurrentPage.Get())
	{
		m_hCurrentPage->SetPos(0,0);
		m_hCurrentPage->InvalidateLayout(true);
	}

	m_pAccessDeniedLabel->SetSize(w, h * 0.3);
	m_pAccessDeniedLabel->SetPos(0, h * 0.25f);
	m_pInsufficientRightsLabel->SetContentAlignment(vgui::Label::a_northwest);
	m_pInsufficientRightsLabel->InvalidateLayout(true);
	int ix, iy;
	m_pInsufficientRightsLabel->GetTextImage()->GetContentSize(ix, iy);
	m_pInsufficientRightsLabel->SetSize(ix, iy);	
	m_pInsufficientRightsLabel->SetPos(w * 0.5f - ix * 0.455f, h * 0.56f);		

	LayoutMenuOptions();
}

#define ASW_BLACK_BAR_HEIGHT 109

void CASW_VGUI_Computer_Menu::LayoutMenuOptions()
{
	if (!m_pHackComputer || !m_pHackComputer->GetComputerArea())
		return;	

	int x,y,w,h;
	GetBounds(x,y,w,h);
	// layout icons according to number of options on the computer
	m_iOptions = m_pHackComputer->GetComputerArea()->GetNumMenuOptions();
	float left_edge = w * 0.12f;
	float top_edge = h * 0.2f;
	float x_gap = w * 0.21f;
	float y_gap = h * 0.1f;	

	switch (m_iOptions)
	{
	case 1:
		{
			m_pMenuIcon[0]->SetPos(left_edge + x_gap, top_edge + y_gap * 2);
			break;
		}
	case 2:
		{
			m_pMenuIcon[0]->SetPos(left_edge, top_edge + y_gap * 2);
			m_pMenuIcon[1]->SetPos(left_edge + x_gap*2, top_edge + y_gap * 2);
			break;
		}
	case 3:
		{
			m_pMenuIcon[0]->SetPos(left_edge + x_gap, top_edge);
			m_pMenuIcon[1]->SetPos(left_edge + x_gap, top_edge + y_gap * 2);
			m_pMenuIcon[2]->SetPos(left_edge + x_gap, top_edge + y_gap * 4);
			break;
		}
	case 4:
		{
			m_pMenuIcon[0]->SetPos(left_edge, top_edge + y_gap);
			m_pMenuIcon[1]->SetPos(left_edge, top_edge + y_gap * 3);
			m_pMenuIcon[2]->SetPos(left_edge + x_gap*2, top_edge + y_gap);
			m_pMenuIcon[3]->SetPos(left_edge + x_gap*2, top_edge + y_gap*3);
			break;
		}
	case 5:
		{
			m_pMenuIcon[0]->SetPos(left_edge, top_edge);
			m_pMenuIcon[1]->SetPos(left_edge, top_edge + y_gap * 2);
			m_pMenuIcon[2]->SetPos(left_edge, top_edge + y_gap * 4);
			m_pMenuIcon[3]->SetPos(left_edge + x_gap*2, top_edge + y_gap);
			m_pMenuIcon[4]->SetPos(left_edge + x_gap*2, top_edge + y_gap*3);
			break;
		}
	case 6:
		{
			m_pMenuIcon[0]->SetPos(left_edge, top_edge);
			m_pMenuIcon[1]->SetPos(left_edge, top_edge + y_gap * 2);
			m_pMenuIcon[2]->SetPos(left_edge, top_edge + y_gap * 4);
			m_pMenuIcon[3]->SetPos(left_edge + x_gap*2, top_edge);
			m_pMenuIcon[4]->SetPos(left_edge + x_gap*2, top_edge + y_gap*2);
			m_pMenuIcon[5]->SetPos(left_edge + x_gap*2, top_edge + y_gap*4);
			break;
		}
	default:
		{
			Assert(m_iOptions<1 || m_iOptions>6);
			break;
		}
	}
	// layout labels next to icons
	for (int i=0;i<m_iOptions;i++)
	{
		int ix,iy,iw,it;
		m_pMenuIcon[i]->GetBounds(ix,iy,iw,it);
		iw = it = 96 * m_fScale;
		m_pMenuIcon[i]->SetShouldScaleImage(true);
		m_pMenuIcon[i]->SetSize(iw,it);
		m_pMenuIcon[i]->SetZPos(155);
		m_pMenuIconShadow[i]->SetShouldScaleImage(true);
		m_pMenuIconShadow[i]->SetSize(iw * 1.3f, it * 1.3f);
		m_pMenuIconShadow[i]->SetZPos(154);
		m_pMenuIconShadow[i]->SetPos(ix - iw * 0.25f, iy + it * 0.0f);
		m_pMenuLabel[i]->SetSize(128 * m_fScale, 28 * m_fScale);
		m_pMenuLabel[i]->SetPos(ix+iw,iy+it*0.4f);
	}
	m_pBlackBar[0]->SetPos(0,0);
	m_pBlackBar[0]->SetZPos(-50);
	m_pBlackBar[0]->SetShouldScaleImage(true);
	m_pBlackBar[0]->SetSize(w, 0);
	m_pBlackBar[1]->SetPos(0,h);
	m_pBlackBar[1]->SetZPos(-50);
	m_pBlackBar[1]->SetShouldScaleImage(true);
	m_pBlackBar[1]->SetSize(w, m_fScale*ASW_BLACK_BAR_HEIGHT*0.8f+1);
}

void CASW_VGUI_Computer_Menu::SetIcons()
{

	if (!m_pHackComputer)
		return;	

	C_ASW_Computer_Area *pArea = m_pHackComputer->GetComputerArea();
	if (!pArea)
		return;

	Msg("menu setting icons. num options=%d\n", pArea->GetNumMenuOptions());

	int icon = 0;
	if (pArea->m_DownloadObjectiveName.Get()[0] != 0 && pArea->GetHackProgress() < 1.0f)
		SetIcon(icon++, "#asw_SynTekDocs", "swarm/Computer/IconFiles");
	if (pArea->m_hSecurityCam1.Get())
		SetIcon(icon++, "#asw_SynTekSecurityCam1", "swarm/Computer/IconCamera");	
	if (pArea->m_hTurret1.Get())
		SetIcon(icon++, "#asw_SynTekTurret1", "swarm/Computer/IconTurret");
	if (pArea->m_MailFile.Get()[0] != 0)
		SetIcon(icon++, "#asw_SynTekMail", "swarm/Computer/IconMail");
	if (pArea->m_NewsFile.Get()[0] != 0)
		SetIcon(icon++, "#asw_SynTekNews", "swarm/Computer/IconNewsLCD");
	if (pArea->m_StocksSeed.Get()[0] != 0)
		SetIcon(icon++, "#asw_SynTekStocks", "swarm/Computer/IconStocks");
	if (pArea->m_WeatherSeed.Get()[0] != 0)
		SetIcon(icon++, "#asw_SynTekAtmosphere", "swarm/Computer/IconWeather");
	if (pArea->m_PlantFile.Get()[0] != 0)
		SetIcon(icon++, "#asw_SynTekPlantStatus", "swarm/Computer/IconCog");
}

void CASW_VGUI_Computer_Menu::SetIcon(int icon, const char *szLabel, const char *szIcon)
{
	if (icon < 0 || icon >= ASW_COMPUTER_MAX_MENU_ITEMS)
		return;

	Msg(" Set icon %d to %s\n", icon, szLabel);
	m_pMenuLabel[icon]->SetText(szLabel);
	m_pMenuIcon[icon]->SetImage(szIcon); // temp use icon until we know how to import
	char buffer[256];
	Q_snprintf(buffer, sizeof(buffer), "%sStriped", szIcon);
	m_pMenuIconShadow[icon]->SetImage(buffer);
	if (szLabel && szLabel[0] != '\0')
	{
		if (GetControllerFocus())
		{
			GetControllerFocus()->AddToFocusList(m_pMenuLabel[icon]);
		}
	}
}

void CASW_VGUI_Computer_Menu::ASWInit()
{
	SetPaintBackgroundType(0);
	SetPaintBackgroundEnabled(false);
	SetBgColor( Color(0,0,0,0) );
	
	SetAlpha(255);
	SetIcons();
	m_pBlackBar[0]->SetImage("swarm/HUD/ASWHUDBlackBar");
	m_pBlackBar[1]->SetImage("swarm/HUD/ASWHUDBlackBar");

	m_pAccessDeniedLabel->SetAlpha(0);
	m_pInsufficientRightsLabel->SetAlpha(0);
}

void CASW_VGUI_Computer_Menu::ShowMenu()
{
	// if the computer is locked, don't show the menu, instead show our 'terminal secure' message
	C_ASW_Computer_Area *pArea = m_pHackComputer->GetComputerArea();
	if (!pArea)
		return;

	if (pArea->IsLocked())
	{
		//todo: show labels (+and red background?)
		vgui::GetAnimationController()->RunAnimationCommand(m_pAccessDeniedLabel, "Alpha", 255, 0, 1.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);	
		vgui::GetAnimationController()->RunAnimationCommand(m_pInsufficientRightsLabel, "Alpha", 255, 0, 1.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);			
		int x,y,w,h;
		GetBounds(x,y,w,h);
		vgui::GetAnimationController()->RunAnimationCommand(m_pBlackBar[0], "Alpha", 255, 0, 1.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);	
		vgui::GetAnimationController()->RunAnimationCommand(m_pBlackBar[1], "Alpha", 255, 0, 1.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);	
		vgui::GetAnimationController()->RunAnimationCommand(m_pBlackBar[0], "tall", m_fScale*ASW_BLACK_BAR_HEIGHT, 0, 1.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);	
		vgui::GetAnimationController()->RunAnimationCommand(m_pBlackBar[1], "ypos", h  - m_fScale*ASW_BLACK_BAR_HEIGHT*0.8f, 0, 1.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);
		m_pBlackBar[1]->SetTall(m_fScale*ASW_BLACK_BAR_HEIGHT*0.8f+1);	
		// make sure the background is red
		CASW_VGUI_Computer_Frame* pFrame = dynamic_cast<CASW_VGUI_Computer_Frame*>(GetParent());
		if (pFrame)
			pFrame->SetBackdrop(1);
		CLocalPlayerFilter filter;
		C_BaseEntity::EmitSound( filter, -1 /*SOUND_FROM_LOCAL_PLAYER*/, "ASWComputer.AccessDenied" );
		return;
	}
	// make sure the background is blue
	CASW_VGUI_Computer_Frame* pFrame = dynamic_cast<CASW_VGUI_Computer_Frame*>(GetParent());
	if (pFrame)
		pFrame->SetBackdrop(0);
	SetIcons();
	LayoutMenuOptions();
	// fade them in
	for (int i=0;i<m_iOptions;i++)
	{
		vgui::GetAnimationController()->RunAnimationCommand(m_pMenuLabel[i], "Alpha", 255, 0.2f * i, 1.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);
		vgui::GetAnimationController()->RunAnimationCommand(m_pMenuIcon[i], "Alpha", 255, 0.2f * i, 1.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);
		vgui::GetAnimationController()->RunAnimationCommand(m_pMenuIconShadow[i], "Alpha", 30, 0.2f * i, 1.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);		
	}
	int x,y,w,h;
	GetBounds(x,y,w,h);
	vgui::GetAnimationController()->RunAnimationCommand(m_pBlackBar[0], "Alpha", 255, 0, 1.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);	
	vgui::GetAnimationController()->RunAnimationCommand(m_pBlackBar[1], "Alpha", 255, 0, 1.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);	
	vgui::GetAnimationController()->RunAnimationCommand(m_pBlackBar[0], "tall", m_fScale*ASW_BLACK_BAR_HEIGHT, 0, 1.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);	
	vgui::GetAnimationController()->RunAnimationCommand(m_pBlackBar[1], "ypos", h  - m_fScale*ASW_BLACK_BAR_HEIGHT*0.8f, 0, 1.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);
	m_pBlackBar[1]->SetTall(m_fScale*ASW_BLACK_BAR_HEIGHT*0.8f+1);
	//vgui::GetAnimationController()->RunAnimationCommand(m_pBlackBar[1], "tall", m_fScale*ASW_BLACK_BAR_HEIGHT*0.8f+1, 0, 1.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);	
}

void CASW_VGUI_Computer_Menu::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	SetPaintBackgroundType(0);
	SetPaintBackgroundEnabled(false);
	SetBgColor( Color(0,0,0,0) );
	SetMouseInputEnabled(true);

	vgui::HFont LabelFont = pScheme->GetFont( "Default", IsProportional() );
	// hide the menu at first
	Color white(255,255,255,255);
	Color blue(19,20,40, 255);
	for (int i=0;i<ASW_COMPUTER_MAX_MENU_ITEMS;i++)
	{
		//if (!m_bSetAlpha)
		//{
			m_pMenuLabel[i]->SetAlpha(0);
			m_pMenuIcon[i]->SetAlpha(0);
			m_pMenuIconShadow[i]->SetAlpha(0);
		//}
		m_pMenuLabel[i]->SetFont(LabelFont);
		m_pMenuLabel[i]->SetPaintBackgroundEnabled(true);
		m_pMenuLabel[i]->SetContentAlignment(vgui::Label::a_center);
		m_pMenuLabel[i]->SetBgColor(Color(19,20,40,255));
		m_pMenuLabel[i]->SetColors(white, white, white, white, blue);
		m_pMenuLabel[i]->SetBorders("TitleButtonBorder", "TitleButtonBorder");
		m_pMenuLabel[i]->SetPaintBackgroundType(2);
	}
	

	vgui::HFont DefaultFont = pScheme->GetFont( "Default", IsProportional() );
	vgui::HFont XLargeFont = pScheme->GetFont( "DefaultExtraLarge", IsProportional() );

	m_pAccessDeniedLabel->SetFont(XLargeFont);
	m_pAccessDeniedLabel->SetFgColor(Color(255,0,0,255));	
	m_pInsufficientRightsLabel->SetFont(DefaultFont);	
	m_pInsufficientRightsLabel->SetFgColor(Color(255,255,255,255));		

	//if (!m_bSetAlpha)
	//{
		m_pBlackBar[0]->SetAlpha(0);
		m_pBlackBar[1]->SetAlpha(0);
		m_pAccessDeniedLabel->SetAlpha(0);
		m_pInsufficientRightsLabel->SetAlpha(0);		
	//}
	if (!m_bSetAlpha)
	{
		m_bSetAlpha = true;
	}
	else
	{
		if (m_hCurrentPage.Get())
			HideMenu(true);
		else
			ShowMenu();	// makes sure the right things are faded in
	}
}

void CASW_VGUI_Computer_Menu::OnThink()
{	
	int x,y,w,t;
	GetBounds(x,y,w,t);

	FindMouseOverOption();
	if (m_iMouseOverOption != m_iOldMouseOverOption)
	{
		if (m_iMouseOverOption != -1 && m_hCurrentPage.Get() == NULL)
		{
			//CLocalPlayerFilter filter;
			//C_BaseEntity::EmitSound( filter, -1 /*SOUND_FROM_LOCAL_PLAYER*/, "ASWComputer.Mouseover" );
		}
		m_iOldMouseOverOption = m_iMouseOverOption;
	}
	for (int i=0;i<m_iOptions;i++)
	{
		if (m_iMouseOverOption == i)
		{
			m_pMenuLabel[i]->SetBgColor(Color(255,255,255,255));
			m_pMenuLabel[i]->SetFgColor(Color(0,0,0,255));
		}
		else
		{
			m_pMenuLabel[i]->SetBgColor(Color(19,20,40,255));
			m_pMenuLabel[i]->SetFgColor(Color(255,255,255,255));
		}
	}
	
	if (m_bFadingCurrentPage)
	{
		if (m_hCurrentPage.Get() == NULL || m_hCurrentPage->GetAlpha() <= 0)
		{			
			m_hCurrentPage->SetVisible(false);
			m_hCurrentPage->MarkForDeletion();
			m_hCurrentPage = NULL;
			m_bFadingCurrentPage = false;

			bool bAutodownload = (m_iAutodownload != -1) && m_pHackComputer->GetComputerArea() &&
						!m_pHackComputer->GetComputerArea()->IsLocked();
			if ( bAutodownload )
			{
				m_iPrepareHackOption = m_iAutodownload;
				m_iAutodownload = -1;
			}
			else
			{
				ShowMenu();
			}
		}
	}

	// if we want to launch a hack option, check if we're all faded out yet
	if (m_iPrepareHackOption != -1)
	{
		if (m_pBlackBar[0]->GetAlpha() <= 0)
		{
			LaunchHackOption(m_iPrepareHackOption);
			m_iPrepareHackOption = -1;
		}
	}

	//float deltatime = gpGlobals->curtime - m_fLastThinkTime;
	m_fLastThinkTime = gpGlobals->curtime;
}

void CASW_VGUI_Computer_Menu::FindMouseOverOption()
{
	m_iMouseOverOption = -1;

	// if the computer is locked, we can't mouse over any options (shouldn't ever get to the main menu though)
	C_ASW_Computer_Area *pArea = m_pHackComputer->GetComputerArea();
	if (!pArea || pArea->IsLocked())
		return;
	
	for (int i=0;i<m_iOptions;i++)
	{
		if (m_pMenuLabel[i]->IsCursorOver() || m_pMenuLabel[i]->IsCursorOver())
		{
			m_iMouseOverOption = i;
			return;
		}
	}
}

bool CASW_VGUI_Computer_Menu::MouseClick(int x, int y, bool bRightClick, bool bDown)
{
	CASW_VGUI_Ingame_Panel *pIngame = dynamic_cast<CASW_VGUI_Ingame_Panel*>(m_hCurrentPage.Get());
	if (pIngame != NULL || bDown)
	{		
		return false;
	}

	FindMouseOverOption();
	// send server a click
	//  server should then change asw_hack_computer's status and asw_vgui_computer_frame should trigger a menu fade out and the actual choice fade in
	
	if (m_iMouseOverOption > -1)
	{
		ClickedMenuOption(m_iMouseOverOption);		
	}	

	// @TODO: this does not work properly in computer sub-menus 
	CASW_VGUI_Computer_Frame* pFrame = dynamic_cast<CASW_VGUI_Computer_Frame*>(GetParent());
	if ( pFrame )
	{
		return pFrame->MouseClick( x, y, bRightClick, bDown );
	}
	else
	{
		return true;	// swallow click
	}
}

void CASW_VGUI_Computer_Menu::ClickedMenuOption(int i)
{
	// don't allow menu clicks if we're still playing the splash intro
	CASW_VGUI_Computer_Frame* pFrame = dynamic_cast<CASW_VGUI_Computer_Frame*>(GetParent());
	if (pFrame && pFrame->m_bPlayingSplash)
		return;
	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if (!pPlayer)
		return;
	pPlayer->SelectHackOption(i+1);
	Msg("pPlayer->SelectHackOption %d\n", i+1);
	CLocalPlayerFilter filter;
	C_BaseEntity::EmitSound( filter, -1 /*SOUND_FROM_LOCAL_PLAYER*/, "ASWComputer.MenuButton" );
}

void CASW_VGUI_Computer_Menu::HideMenu(bool bHideHeader)
{
	// fade it all out
	for (int i=0;i<m_iOptions;i++)
	{
		vgui::GetAnimationController()->RunAnimationCommand(m_pMenuLabel[i], "Alpha", 0, 0.05f * i, 0.3f, vgui::AnimationController::INTERPOLATOR_LINEAR);
		vgui::GetAnimationController()->RunAnimationCommand(m_pMenuIcon[i], "Alpha", 0, 0.05f * i, 0.3f, vgui::AnimationController::INTERPOLATOR_LINEAR);
		vgui::GetAnimationController()->RunAnimationCommand(m_pMenuIconShadow[i], "Alpha", 0, 0.05f * i, 0.3f, vgui::AnimationController::INTERPOLATOR_LINEAR);	
	}
	int x,y,w,h;
	GetBounds(x,y,w,h);
	vgui::GetAnimationController()->RunAnimationCommand(m_pBlackBar[0], "Alpha", 0, 0, 0.6f, vgui::AnimationController::INTERPOLATOR_LINEAR);	
	vgui::GetAnimationController()->RunAnimationCommand(m_pBlackBar[1], "Alpha", 0, 0, 0.6f, vgui::AnimationController::INTERPOLATOR_LINEAR);	
	//vgui::GetAnimationController()->RunAnimationCommand(m_pBlackBar[0], "tall", 0, 0, 0.6f, vgui::AnimationController::INTERPOLATOR_LINEAR);	
	//vgui::GetAnimationController()->RunAnimationCommand(m_pBlackBar[1], "ypos", GetTall(), 0, 0.6f, vgui::AnimationController::INTERPOLATOR_LINEAR);
	//vgui::GetAnimationController()->RunAnimationCommand(m_pBlackBar[1], "tall", 0, 0, 0.6f, vgui::AnimationController::INTERPOLATOR_LINEAR);
	if (bHideHeader)
	{
		CASW_VGUI_Computer_Frame* pFrame = dynamic_cast<CASW_VGUI_Computer_Frame*>(GetParent());
		if (pFrame)
			pFrame->SetTitleHidden(true);
	}
	else
	{
		CASW_VGUI_Computer_Frame* pFrame = dynamic_cast<CASW_VGUI_Computer_Frame*>(GetParent());
		if (pFrame)
			pFrame->SetTitleHidden(false);
	}

	vgui::GetAnimationController()->RunAnimationCommand(m_pAccessDeniedLabel, "Alpha", 0, 0, 0.6f, vgui::AnimationController::INTERPOLATOR_LINEAR);	
	vgui::GetAnimationController()->RunAnimationCommand(m_pInsufficientRightsLabel, "Alpha", 0, 0, 0.6f, vgui::AnimationController::INTERPOLATOR_LINEAR);		
}

// activates the menu option we clicked on
void CASW_VGUI_Computer_Menu::SetHackOption(int iOption)
{
	Msg("CASW_VGUI_Computer_Menu::SetHackOption %d\n", iOption);
	if (!m_pHackComputer)
		return;	

	C_ASW_Computer_Area *pArea = m_pHackComputer->GetComputerArea();
	if (!pArea)
		return;

	if ( iOption == 0 && m_hCurrentPage.Get() )
	{
		DoFadeCurrentPage();
		if (m_iPreviousHackOption == ASW_HACK_OPTION_OVERRIDE)	// finished a hack?
		{
			CLocalPlayerFilter filter;
			C_BaseEntity::EmitSound( filter, -1 /*SOUND_FROM_LOCAL_PLAYER*/, "ASWComputer.HackComplete" );
		}
		return;
	}
	else
	{
		if (m_iPreviousHackOption == ASW_HACK_OPTION_OVERRIDE)	// finished a hack?
		{
			m_iAutodownload = iOption;
			DoFadeCurrentPage();
			return;
		}
	}

	m_iPrepareHackOption = iOption;
	HideMenu(true);
}

void CASW_VGUI_Computer_Menu::LaunchHackOption(int iOption)
{
	Msg("CASW_VGUI_Computer_Menu::LaunchHackOption %d\n", iOption);
	if (!m_pHackComputer)
		return;	

	C_ASW_Computer_Area *pArea = m_pHackComputer->GetComputerArea();
	if (!pArea)
		return;

	m_iPreviousHackOption = iOption;

	// check for the special override option
	if (iOption == ASW_HACK_OPTION_OVERRIDE)
	{
		// make sure the background is blue
		CASW_VGUI_Computer_Frame* pFrame = dynamic_cast<CASW_VGUI_Computer_Frame*>(GetParent());
		if (pFrame)
			pFrame->SetBackdrop(0);
		CASW_VGUI_Computer_Tumbler_Hack* pPage = new CASW_VGUI_Computer_Tumbler_Hack(this, "ComputerTumblerHack", m_pHackComputer);
		pPage->SetPos(0,0);
		m_hCurrentPage = pPage;
		return;
	}

	// otherwise check which icon we pressed
	int icon = 1;
	if (pArea->m_DownloadObjectiveName.Get()[0] != 0 && pArea->GetHackProgress() < 1.0f )
	{
		if (iOption == icon)
		{
			// activate this one
			CASW_VGUI_Computer_Download_Docs* pPage = new CASW_VGUI_Computer_Download_Docs(this, "ComputerDocs", m_pHackComputer);
			pPage->ASWInit();
			pPage->SetPos(0,0);
			m_hCurrentPage = pPage;
			return;
		}
		icon++;
	}
	if (pArea->m_hSecurityCam1.Get())
	{
		if (iOption == icon)
		{
			// activate this one
			CASW_VGUI_Computer_Camera* pCam = new CASW_VGUI_Computer_Camera(this, "ComputerCamera", m_pHackComputer);
			pCam->ASWInit();
			pCam->SetPos(0,0);
			m_hCurrentPage = pCam;			
			return;
		}
		icon++;
	}	
	if (pArea->m_hTurret1.Get())
	{
		if (iOption == icon)
		{			
			// don't need to do anything clientside if we're controlling a turret, this window will be hidden
			// activate this one
			// TODO: this should effectively close the computer (the server will make us remote control a turret)
			//Msg("Turret section of computers not hooked up yet serverside\n");
			CLocalPlayerFilter filter;
			C_BaseEntity::EmitSound( filter, -1 /*SOUND_FROM_LOCAL_PLAYER*/, "ASWComputer.RemoteTurret" );
			return;
		}
		icon++;
	}
	if (pArea->m_MailFile.Get()[0] != 0)
	{
		if (iOption == icon)
		{
			// activate this one
			CASW_VGUI_Computer_Mail* pPage = new CASW_VGUI_Computer_Mail(this, "ComputerMail", m_pHackComputer);
			pPage->ASWInit();
			pPage->SetPos(0,0);
			m_hCurrentPage = pPage;
			return;
		}
		icon++;
	}
	if (pArea->m_NewsFile.Get()[0] != 0)
	{
		if (iOption == icon)
		{
			// activate this one
			CASW_VGUI_Computer_News* pPage = new CASW_VGUI_Computer_News(this, "ComputerNews", m_pHackComputer);
			pPage->ASWInit();
			pPage->SetPos(0,0);
			m_hCurrentPage = pPage;
			return;
		}
		icon++;
	}
	if (pArea->m_StocksSeed.Get()[0] != 0)
	{
		if (iOption == icon)
		{
			// activate this one			
			CASW_VGUI_Computer_Stocks* pPage = new CASW_VGUI_Computer_Stocks(this, "ComputerStocks", m_pHackComputer);
			pPage->ASWInit();
			pPage->SetPos(0,0);
			m_hCurrentPage = pPage;
			return;
		}
		icon++;
	}
	if (pArea->m_WeatherSeed.Get()[0] != 0)
	{
		if (iOption == icon)
		{			
			CASW_VGUI_Computer_Weather* pPage = new CASW_VGUI_Computer_Weather(this, "ComputerWeather", m_pHackComputer);
			pPage->ASWInit();
			pPage->SetPos(0,0);
			m_hCurrentPage = pPage;
			return;
		}
		icon++;
	}
	if (pArea->m_PlantFile.Get()[0] != 0)
	{
		if (iOption == icon)
		{
			// activate this one
			CASW_VGUI_Computer_Plant* pPage = new CASW_VGUI_Computer_Plant(this, "ComputerPlant", m_pHackComputer);
			pPage->ASWInit();
			pPage->SetPos(0,0);
			m_hCurrentPage = pPage;
			return;
		}
		icon++;
	}	
	Msg("Error: activated an option that wasn't in this computer\n");
}

void CASW_VGUI_Computer_Menu::FadeCurrentPage()
{
	if (m_hCurrentPage.Get() == NULL)
		return;

	// request a return to main menu from the server
	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	pPlayer->SelectHackOption(0);
	CLocalPlayerFilter filter;
	C_BaseEntity::EmitSound( filter, -1 /*SOUND_FROM_LOCAL_PLAYER*/, "ASWComputer.MenuBack" );
}

void CASW_VGUI_Computer_Menu::DoFadeCurrentPage()
{
	if (m_hCurrentPage.Get() == NULL)
		return;

	m_bFadingCurrentPage = true;	
	vgui::GetAnimationController()->RunAnimationCommand(m_hCurrentPage.Get(), "Alpha", 0, 0, 0.3f, vgui::AnimationController::INTERPOLATOR_LINEAR);	
}

void CASW_VGUI_Computer_Menu::OnCommand(char const* command)
{	
	if (!strnicmp(command, "Option", 6))
	{
		int iControl = atoi(command+6);
		ClickedMenuOption(iControl);
		return;
	}
	else if ( !Q_stricmp( command, "Cancel" ) )
	{
		if ( m_hCurrentPage.Get() )
		{
			SetHackOption( 0 );
		}
		else
		{
			C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
			if ( pPlayer )
			{
				pPlayer->StopUsing();
			}
		}
	}
	
	BaseClass::OnCommand(command);
}

bool CASW_VGUI_Computer_Menu::IsHacking()
{
	return (dynamic_cast<CASW_VGUI_Computer_Tumbler_Hack*>(m_hCurrentPage.Get()) != NULL);
}