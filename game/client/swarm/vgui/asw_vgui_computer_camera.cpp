#include "cbase.h"
#include "asw_vgui_computer_camera.h"
#include "asw_vgui_computer_menu.h"
#include "vgui/ISurface.h"
#include "c_asw_hack_computer.h"
#include "c_asw_computer_area.h"
#include <vgui/IInput.h>
#include <vgui_controls/AnimationController.h>
#include <vgui_controls/ImagePanel.h>
#include "vgui/ILocalize.h"
#include "controller_focus.h"
#include "asw_vgui_computer_frame.h"
#include "clientmode_asw.h"
#include "ImageButton.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CASW_VGUI_Computer_Camera::CASW_VGUI_Computer_Camera( vgui::Panel *pParent, const char *pElementName, C_ASW_Hack_Computer* pHackComputer ) 
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

	if (GetControllerFocus())
	{
		GetControllerFocus()->AddToFocusList(m_pBackButton);
		GetControllerFocus()->SetFocusPanel(m_pBackButton);
	}
	m_pBackButton->SetAlpha(0);
	m_pTitleLabel = new vgui::Label(this, "TitleLabel", "#asw_SynTekSecurityCam");
	m_pCameraImage = new vgui::ImagePanel(this, "CameraImage");
	m_pCameraLabel = new vgui::Label(this, "CameraLabel", "");
	m_bSetAlpha = false;
}

CASW_VGUI_Computer_Camera::~CASW_VGUI_Computer_Camera()
{
	CASW_VGUI_Computer_Frame *pComputerFrame = dynamic_cast< CASW_VGUI_Computer_Frame* >( GetClientMode()->GetPanelFromViewport( "ComputerContainer/VGUIComputerFrame" ) );
	if ( pComputerFrame )
	{
		pComputerFrame->m_bHideLogoffButton = false;
	}

	if (GetControllerFocus())
	{
		GetControllerFocus()->RemoveFromFocusList(m_pBackButton);
	}
}

void CASW_VGUI_Computer_Camera::PerformLayout()
{
	m_fScale = ScreenHeight() / 768.0f;

	int w = GetParent()->GetWide();
	int h = GetParent()->GetTall();
	SetWide(w);
	SetTall(h);
	SetPos(0, 0);
	
	m_pTitleLabel->SetSize(w, h * 0.2f);
	m_pTitleLabel->SetContentAlignment(vgui::Label::a_center);
	m_pTitleLabel->SetPos(0, 0);//h*0.65f);
	m_pTitleLabel->SetZPos(160);
	m_pBackButton->SetPos(w * 0.75, h*0.9);
	m_pBackButton->SetSize(128 * m_fScale, 28 * m_fScale);
	m_pCameraLabel->SetPos(w*0.05 + 8 *m_fScale, h*0.25 + 8 *m_fScale);
	m_pCameraLabel->SetSize(w*0.8 - 16*m_fScale, h*0.7 - 16*m_fScale);
	m_pCameraLabel->SetContentAlignment(vgui::Label::a_northwest);
	m_pCameraImage->SetPos(w*0.05, h*0.25);
	m_pCameraImage->SetSize(w*0.7, h*0.6);
	m_pCameraImage->SetShouldScaleImage(true);

	// make sure all the labels expand to cover the new sizes	 
	m_pCameraLabel->InvalidateLayout();
	m_pTitleLabel->InvalidateLayout();
}


void CASW_VGUI_Computer_Camera::ASWInit()
{
	SetPaintBackgroundType(0);
	SetPaintBackgroundEnabled(false);
	SetBgColor( Color(0,0,0,0) );
	
	SetAlpha(255);

	CLocalPlayerFilter filter;
	C_BaseEntity::EmitSound( filter, -1 /*SOUND_FROM_LOCAL_PLAYER*/, "ASWComputer.SecurityCamera" );
}

void CASW_VGUI_Computer_Camera::ApplySchemeSettings(vgui::IScheme *pScheme)
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
	
	m_pCameraImage->SetImage("swarm/Computer/ComputerCamera");
	m_pCameraImage->SetShouldScaleImage(true);

	m_pTitleLabel->SetFgColor(Color(255,255,255,255));
	vgui::HFont LargeTitleFont = pScheme->GetFont( "DefaultLarge", IsProportional() );
	m_pTitleLabel->SetFont(LargeTitleFont);
	m_pTitleLabel->SetContentAlignment(vgui::Label::a_center);

	vgui::HFont CameraFont = pScheme->GetFont( "SmallCourier", IsProportional() );
	m_pCameraLabel->SetFont(CameraFont);
	
	if (m_pHackComputer)
	{
		
		C_ASW_Computer_Area *pArea = m_pHackComputer->GetComputerArea();
		if (pArea)
		{	
			m_pCameraLabel->SetText(pArea->m_SecurityCamLabel1);
			m_pTitleLabel->SetText(pArea->m_SecurityCamLabel1);
		}
	}

	// fade them in
	if (!m_bSetAlpha)
	{
		m_bSetAlpha = true;
		m_pCameraLabel->SetAlpha(0);
		m_pBackButton->SetAlpha(0);
		m_pCameraImage->SetAlpha(0);
		m_pTitleLabel->SetAlpha(0);
		vgui::GetAnimationController()->RunAnimationCommand(m_pTitleLabel, "Alpha", 255, 0.2f, 1.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);
		vgui::GetAnimationController()->RunAnimationCommand(m_pBackButton, "Alpha", 255, 0, 1.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);
		vgui::GetAnimationController()->RunAnimationCommand(m_pCameraImage, "Alpha", 255, 0, 1.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);
		vgui::GetAnimationController()->RunAnimationCommand(m_pCameraLabel, "Alpha", 255, 0, 1.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);	
	}
}

void CASW_VGUI_Computer_Camera::OnThink()
{	
	int x,y,w,t;
	GetBounds(x,y,w,t);

	SetPos(0,0);

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

bool CASW_VGUI_Computer_Camera::MouseClick(int x, int y, bool bRightClick, bool bDown)
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

void CASW_VGUI_Computer_Camera::OnCommand(char const* command)
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