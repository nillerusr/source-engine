#include "cbase.h"
#include "asw_vgui_computer_frame.h"
#include "asw_vgui_computer_splash.h"
#include "asw_vgui_computer_menu.h"
#include "vgui/ISurface.h"
#include "c_asw_hack_computer.h"
#include "c_asw_computer_area.h"
#include <vgui/IInput.h>
#include <vgui_controls/AnimationController.h>
#include <vgui_controls/ImagePanel.h>
#include "ImageButton.h"
#include "controller_focus.h"
#include "asw_gamerules.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CASW_VGUI_Computer_Frame::CASW_VGUI_Computer_Frame( vgui::Panel *pParent, const char *pElementName, C_ASW_Hack_Computer* pHackComputer ) 
:	vgui::Panel( pParent, pElementName ),
	CASW_VGUI_Ingame_Panel(),
	m_pHackComputer( pHackComputer )
{
	m_bHideLogoffButton = false;

	SetKeyBoardInputEnabled(true);
	m_fLastThinkTime = gpGlobals->curtime;
	m_pCurrentPanel = NULL;
	m_pMenuPanel = NULL;
	m_pSplash = NULL;
	m_bSetAlpha = false;

	m_pLogoffLabel = new ImageButton(this, "LogoffLabel", "");
	m_pLogoffLabel->AddActionSignalTarget(this);

	KeyValues *msg = new KeyValues("Command");	
	msg->SetString("command", "Logoff");
	m_pLogoffLabel->SetCommand(msg);

	KeyValues *cmsg = new KeyValues("Command");			
	cmsg->SetString( "command", "Cancel" );
	m_pLogoffLabel->SetCancelCommand( cmsg );
	
	m_pLogoffLabel->SetText("#asw_log_off");


	for (int i=0;i<3;i++)
	{
		m_pScan[i] = new vgui::ImagePanel(this, "ComputerScan0");
		m_pScan[i]->SetShouldScaleImage(true);
		m_pScan[i]->SetImage("swarm/Computer/ComputerScan");
	}
	m_pBackdropImage = new vgui::ImagePanel(this, "SplashImage");
	m_pBackdropImage->SetShouldScaleImage(true);
	m_iBackdropType = -1;
	SetBackdrop(0);
	
	RequestFocus();

	m_bPlayingSplash = !IsPDA();
}

CASW_VGUI_Computer_Frame::~CASW_VGUI_Computer_Frame()
{
	if ( GetControllerFocus() )
	{
		GetControllerFocus()->RemoveFromFocusList( m_pLogoffLabel );
	}
}

void CASW_VGUI_Computer_Frame::SetBackdrop(int iBackdropType)
{
	if (m_iBackdropType == iBackdropType)
		return;

	m_iBackdropType = iBackdropType;
	if (iBackdropType == 0)
	{
		m_pBackdropImage->SetImage("swarm/Computer/ComputerBackdrop");
	}
	else
	{
		m_pBackdropImage->SetImage("swarm/Computer/ComputerBackdropRed");
	}
}

void CASW_VGUI_Computer_Frame::SplashFinished()
{
	if (m_pMenuPanel && !m_pMenuPanel->IsHacking())
		m_pMenuPanel->ShowMenu();
	m_bPlayingSplash = false;
}

void CASW_VGUI_Computer_Frame::ASWInit()
{
	SetPaintBackgroundType(0);
	SetPaintBackgroundEnabled(true);
	SetBgColor( Color(0,0,0,255) );

	m_pBackdropImage->SetAlpha(0);
	vgui::GetAnimationController()->RunAnimationCommand(m_pBackdropImage, "Alpha", 255, 0, 2.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);
	
	SetAlpha(255);

	if (IsPDA())
	{
		m_pSplash = NULL;		
	}
	else
	{
		m_pSplash = new CASW_VGUI_Computer_Splash(this, "ComputerSplash", m_pHackComputer);
		m_pSplash->ASWInit();	
		m_pSplash->SetPos(0,0);
		m_pCurrentPanel = m_pSplash;
	}
	m_iScanHeight = 0;

	m_pMenuPanel = new CASW_VGUI_Computer_Menu(this, "ComputerMenu", m_pHackComputer);
	m_pMenuPanel->ASWInit();	
	m_pMenuPanel->SetPos(0,0);

	if (IsPDA())
	{
		// hm, not true.. we want the first panel to be the mail one
		m_pCurrentPanel = m_pMenuPanel;
	}

	if (GetControllerFocus())
	{
		GetControllerFocus()->AddToFocusList(m_pLogoffLabel);
		GetControllerFocus()->SetFocusPanel(m_pLogoffLabel);
	}

	vgui::GetAnimationController()->RunAnimationCommand(m_pLogoffLabel, "Alpha", 255, 0.0f, 1.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);


}

bool CASW_VGUI_Computer_Frame::IsPDA()
{
	if (m_pHackComputer && m_pHackComputer->GetComputerArea())
		return m_pHackComputer->GetComputerArea()->IsPDA();

	return false;
}

void CASW_VGUI_Computer_Frame::PerformLayout()
{
	m_fScale = ScreenHeight() / 768.0f;
	int w = m_fScale * (0.65f * 1024);
	int h = m_fScale * (0.65f * 768);
	SetWide(w);
	SetTall(h);
	int frame_border = 10.0f * m_fScale;
	int frame_title_border = 40.0f * m_fScale;
	SetPos(frame_border,frame_title_border);	// assumes its inside an asw_vgui_frame..

	if (m_pCurrentPanel)
		m_pCurrentPanel->SetPos(0,0);
	if (m_pSplash)
		m_pSplash->SetPos(0,0);

	m_iScanHeight = 0.166667f * 768 * m_fScale;
	for (int i=0;i<3;i++)
	{
		m_pScan[i]->SetSize(w, m_iScanHeight);
		m_pScan[i]->SetPos(0,-m_iScanHeight);
		m_pScan[i]->SetZPos(200);	// make sure scan lines are on top of everything (or should they be under?)
	}

	m_pBackdropImage->SetShouldScaleImage(true);
	m_pBackdropImage->SetPos(0,0);
	m_pBackdropImage->SetSize(w, h);

	if (m_pMenuPanel)
		m_pMenuPanel->InvalidateLayout(true);

	m_pLogoffLabel->SetSize(128 * m_fScale, 28 * m_fScale);
	//m_pLogoffLabel->SetPos(w*0.05, h*0.9);
	m_pLogoffLabel->SetPos( w * 0.75, h*0.9 );
	m_pLogoffLabel->SetZPos(200);
}

void CASW_VGUI_Computer_Frame::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	SetPaintBackgroundType(0);
	SetPaintBackgroundEnabled(true);
	SetBgColor( Color(0,0,0,255) );
	SetMouseInputEnabled(true);

	vgui::HFont LabelFont = pScheme->GetFont( "Default", IsProportional() );
	Color white(255,255,255,255);
	Color blue(19,20,40, 255);
	m_pLogoffLabel->SetFont(LabelFont);
	m_pLogoffLabel->SetPaintBackgroundEnabled(true);
	m_pLogoffLabel->SetContentAlignment(vgui::Label::a_center);
	m_pLogoffLabel->SetBgColor(Color(19,20,40,255));
	m_pLogoffLabel->SetBorders("TitleButtonBorder", "TitleButtonBorder");
	m_pLogoffLabel->SetColors(white, white, white, white, blue);
	m_pLogoffLabel->SetPaintBackgroundType(2);

	// fade scanlines in
	if (!m_bSetAlpha)
	{
		m_bSetAlpha = true;
		m_pScan[0]->SetAlpha(0);
		m_pScan[1]->SetAlpha(0);
		m_pScan[2]->SetAlpha(0);
		// alphas were 85 170 255
		vgui::GetAnimationController()->RunAnimationCommand(m_pScan[0], "Alpha", 42, 0, 2.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);
		vgui::GetAnimationController()->RunAnimationCommand(m_pScan[1], "Alpha", 85, 0, 2.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);
		vgui::GetAnimationController()->RunAnimationCommand(m_pScan[2], "Alpha", 128, 0, 2.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);

		m_pBackdropImage->SetAlpha(0);
		vgui::GetAnimationController()->RunAnimationCommand(m_pBackdropImage, "Alpha", 255, 0, 2.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);
	}
	else
	{
		m_pScan[0]->SetAlpha(42);
		m_pScan[1]->SetAlpha(85);
		m_pScan[2]->SetAlpha(128);
		m_pBackdropImage->SetAlpha(255);
	}
}

void CASW_VGUI_Computer_Frame::OnThink()
{
	int x,y,w,t;
	GetBounds(x,y,w,t);

	if (m_pCurrentPanel)
		m_pCurrentPanel->SetPos(0,0);

	for (int i=0;i<3;i++)
	{
		m_pScan[i]->GetPos(x,y);
		if (y <= -m_iScanHeight)
		{
			m_pScan[i]->SetPos(0,t);			
			vgui::GetAnimationController()->RunAnimationCommand(m_pScan[i], "ypos", -m_iScanHeight, 0, (i+1)*2, vgui::AnimationController::INTERPOLATOR_LINEAR);			
		}
	}

	m_pLogoffLabel->SetVisible( !IsPDA() && !m_bHideLogoffButton );

	if (m_pLogoffLabel->IsCursorOver() && m_pLogoffLabel->IsVisible() )
	{
		m_pLogoffLabel->SetBgColor(Color(255,255,255,255));
		m_pLogoffLabel->SetFgColor(Color(0,0,0,255));
	}
	else
	{
		m_pLogoffLabel->SetBgColor(Color(19,20,40,255));
		m_pLogoffLabel->SetFgColor(Color(255,255,255,255));
	}

	m_fLastThinkTime = gpGlobals->curtime;
}

void CASW_VGUI_Computer_Frame::OnCommand( const char *pCommand )
{
	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();

	if ( !Q_stricmp( pCommand, "Cancel" ) )
	{
		if ( m_pMenuPanel && m_pMenuPanel->m_hCurrentPage.Get() )
		{
			m_pMenuPanel->SetHackOption( 0 );
		}
		else
		{
			if ( pPlayer )
			{
				pPlayer->StopUsing();
			}
		}
	}
	else if ( !Q_stricmp( pCommand, "Logoff" ) )
	{
		if ( pPlayer )
		{
			pPlayer->StopUsing();
		}
	}

	BaseClass::OnCommand( pCommand );
}

void CASW_VGUI_Computer_Frame::Paint()
{
	BaseClass::Paint();
}

void CASW_VGUI_Computer_Frame::PaintBackground()
{
	BaseClass::PaintBackground();
}

bool CASW_VGUI_Computer_Frame::MouseClick(int x, int y, bool bRightClick, bool bDown)
{
	if (m_pLogoffLabel->IsCursorOver() && m_pLogoffLabel->IsVisible() )
	{
		C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
		if (pPlayer)
			pPlayer->StopUsing();
		return true;
	}

	return true;	// swallow click
}

void CASW_VGUI_Computer_Frame::SetTitleHidden(bool bHidden)
{
	if (m_pSplash)
		m_pSplash->SetHidden(bHidden);
}

void CASW_VGUI_Computer_Frame::SetHackOption(int iOption)
{
	if (m_pMenuPanel)
		m_pMenuPanel->SetHackOption(iOption);
}

// ================================== Container ==================================================

CASW_VGUI_Computer_Container::CASW_VGUI_Computer_Container(Panel *parent, const char *panelName, const char* pszTitle) :
	CASW_VGUI_Frame(parent, panelName, pszTitle)
{
}

CASW_VGUI_Computer_Container::~CASW_VGUI_Computer_Container()
{
}

void CASW_VGUI_Computer_Container::PerformLayout()
{
	BaseClass::PerformLayout();

	float fScale = ScreenHeight() / 768.0f;			
	int border = 10.0f * fScale;
	int title_border = 40.0f * fScale;
	if (m_bFrameMinimized)
	{
		int w = fScale * 0.3f * 1024.0f;
		vgui::HFont title_font = m_pTitleLabel->GetFont();
		int title_tall = vgui::surface()->GetFontTall(title_font);
		int h = title_tall * 1.2f;

		SetBounds((ScreenWidth() * 0.5f) - (w * 0.5f), (ScreenHeight() * 0.08f), 
									w , h);
	}
	else
	{
		int w = fScale * (0.65f * 1024) + border * 2;
		int h = fScale * (0.65f * 768) + border + title_border;
		SetBounds((ScreenWidth() * 0.5f) - (w * 0.5f), (ScreenHeight() * 0.5f) - (h * 0.5f), 
									w , h);
	}
}

void CASW_VGUI_Computer_Container::ApplySchemeSettings(vgui::IScheme *scheme)
{
	BaseClass::ApplySchemeSettings(scheme);
}

void CASW_VGUI_Computer_Container::OnThink()
{
	BaseClass::OnThink();

	float fDeathCamInterp = ASWGameRules()->GetMarineDeathCamInterp();

	if ( fDeathCamInterp > 0.0f && GetAlpha() >= 255 )
	{
		SetAlpha( 254 );
		vgui::GetAnimationController()->RunAnimationCommand(this, "Alpha", 0, 0, 0.5f, vgui::AnimationController::INTERPOLATOR_LINEAR);
	}
	else if ( fDeathCamInterp <= 0.0f && GetAlpha() <= 0 )
	{
		SetAlpha( 1 );
		vgui::GetAnimationController()->RunAnimationCommand(this, "Alpha", 255, 0, 0.5f, vgui::AnimationController::INTERPOLATOR_LINEAR);
	}
}