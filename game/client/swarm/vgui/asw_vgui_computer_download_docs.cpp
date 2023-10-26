#include "cbase.h"
#include "asw_vgui_computer_download_docs.h"
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
#include <filesystem.h>
#include <keyvalues.h>
#include "controller_focus.h"
#include "asw_vgui_computer_frame.h"
#include "clientmode_asw.h"
#include "ImageButton.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define BAR_HEIGHT		(12.0f * m_fScale)
#define BAR_WIDTH		(300.0f * m_fScale)
#define BAR_PADDING		(6.0f * m_fScale)

CASW_VGUI_Computer_Download_Docs::CASW_VGUI_Computer_Download_Docs( vgui::Panel *pParent, const char *pElementName, C_ASW_Hack_Computer* pHackComputer ) 
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
	m_pTitleLabel = new vgui::Label(this, "TitleLabel", "#asw_SynTekDownloadingDocs");
	m_pFilesIcon = new vgui::ImagePanel(this, "FilesIcon");
	m_pFilesIconShadow = new vgui::ImagePanel(this, "FilesIconShadow");
	m_pDeviceIcon = new vgui::ImagePanel(this, "DeviceIcon");
	m_pDeviceIconShadow = new vgui::ImagePanel(this, "DeviceIconShadow");
	m_pArrow = new vgui::ImagePanel(this, "Arrow");	
	m_pArrow->SetAlpha(0);

	m_pProgressBarBackdrop = new vgui::Panel(this, "ProgressBarBackdrop");
	m_pProgressBar = new vgui::Panel(this, "ProgressBar");

	m_pKeyValues = NULL;	
	m_bSetTextComplete = false;
	m_bSetAlpha = false;

	if (GetControllerFocus())
	{
		GetControllerFocus()->AddToFocusList(m_pBackButton);
		GetControllerFocus()->SetFocusPanel(m_pBackButton);
	}
}

CASW_VGUI_Computer_Download_Docs::~CASW_VGUI_Computer_Download_Docs()
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

void CASW_VGUI_Computer_Download_Docs::PerformLayout()
{
	m_fScale = ScreenHeight() / 768.0f;

	int w = GetParent()->GetWide();
	int h = GetParent()->GetTall();
	SetWide(w);
	SetTall(h);
	SetPos(0, 0);

	m_pBackButton->SetPos(w * 0.75, h*0.9);
	m_pBackButton->SetSize(128 * m_fScale, 28 * m_fScale);
	
	m_pTitleLabel->SetSize(w, h * 0.2f);
	m_pTitleLabel->SetContentAlignment(vgui::Label::a_center);
	m_pTitleLabel->SetPos(0, 0);//h*0.65f);
	m_pTitleLabel->SetZPos(160);

	//const float ypos = 0.3f * h;
	m_pFilesIcon->SetShouldScaleImage(true);
	m_pFilesIconShadow->SetShouldScaleImage(true);
	m_pArrow->SetShouldScaleImage(true);
	m_pDeviceIcon->SetShouldScaleImage(true);
	m_pDeviceIconShadow->SetShouldScaleImage(true);
	
	int ix,iy,iw,it;	
	iw = it = 96 * m_fScale;
	ix = w*0.25f - (iw * 0.5f);
	int ix2 = w*0.75f - (iw * 0.5f);
	iy = h*0.25f;

	m_pFilesIcon->SetPos(ix,iy);
	m_pFilesIcon->SetSize(iw,it);
	m_pFilesIcon->SetZPos(155);	
	m_pFilesIconShadow->SetSize(iw * 1.3f, it * 1.3f);
	m_pFilesIconShadow->SetZPos(154);
	m_pFilesIconShadow->SetPos(ix - iw * 0.25f, iy + it * 0.0f);

	m_pArrow->SetAlpha(0);
	m_pArrow->SetPos(ix2,iy + it * 0.25f);
	m_pArrow->SetSize(iw * 0.5f,it * 0.5f);
	m_pArrow->SetZPos(155);
	
	ix = w*0.75f - (iw * 0.5f);
	m_pDeviceIcon->SetPos(ix,iy);
	m_pDeviceIcon->SetSize(iw,it);
	m_pDeviceIcon->SetZPos(155);
	m_pDeviceIconShadow->SetSize(iw * 1.3f, it * 1.3f);
	m_pDeviceIconShadow->SetZPos(154);
	m_pDeviceIconShadow->SetPos(ix - iw * 0.25f, iy + it * 0.0f);
	
	m_pProgressBarBackdrop->SetSize(BAR_WIDTH + BAR_PADDING * 2, BAR_HEIGHT + BAR_PADDING * 2);
	m_pProgressBarBackdrop->SetPos(0.5f * w - ((BAR_WIDTH + BAR_PADDING * 2) * 0.5f),
									0.6f * h);
	//m_pProgressBarBackdrop->SetZPos(154);

	m_pProgressBar->SetSize(0, BAR_HEIGHT);
	m_pProgressBar->SetPos(0.5f * w - (BAR_WIDTH * 0.5f),
									0.6f * h + BAR_PADDING);
	//m_pProgressBar->SetZPos(155);

	// make sure all the labels expand to cover the new sizes	 
	m_pTitleLabel->InvalidateLayout();
}


void CASW_VGUI_Computer_Download_Docs::ASWInit()
{
	SetPaintBackgroundType(0);
	SetPaintBackgroundEnabled(false);
	SetBgColor( Color(0,0,0,0) );
	
	SetAlpha(255);

	CLocalPlayerFilter filter;
	C_BaseEntity::EmitSound( filter, -1 /*SOUND_FROM_LOCAL_PLAYER*/, "ASWComputer.CriticalData" );
}

void CASW_VGUI_Computer_Download_Docs::ApplySchemeSettings(vgui::IScheme *pScheme)
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
	m_pTitleLabel->SetFgColor(Color(255,255,255,255));
	m_pTitleLabel->SetFont(LargeTitleFont);
	m_pTitleLabel->SetContentAlignment(vgui::Label::a_center);
	
	m_pProgressBarBackdrop->SetPaintBackgroundEnabled(true);
	m_pProgressBarBackdrop->SetPaintBackgroundType(2);
	m_pProgressBarBackdrop->SetBgColor(Color(0,0,0,192));
	m_pProgressBar->SetPaintBackgroundEnabled(true);
	m_pProgressBar->SetPaintBackgroundType(0);
	m_pProgressBar->SetBgColor(Color(255,255,255,192));	
		
	m_pFilesIcon->SetImage("swarm/Computer/IconFiles");
	m_pFilesIconShadow->SetImage("swarm/Computer/IconFilesStriped");
	m_pArrow->SetImage("swarm/Computer/IconFiles");
	m_pDeviceIcon->SetImage("swarm/Computer/IconFolder");
	m_pDeviceIconShadow->SetImage("swarm/Computer/IconFolderStriped");	
	
	// fade them in	
	if (!m_bSetAlpha)
	{
		m_bSetAlpha = true;
		m_pBackButton->SetAlpha(0);
		m_pTitleLabel->SetAlpha(0);
		m_pFilesIcon->SetAlpha(0);
		m_pFilesIconShadow->SetAlpha(0);
		m_pDeviceIcon->SetAlpha(0);
		m_pDeviceIconShadow->SetAlpha(0);
		m_pProgressBarBackdrop->SetAlpha(0);
		m_pProgressBar->SetAlpha(0);
		vgui::GetAnimationController()->RunAnimationCommand(m_pBackButton, "Alpha", 255, 0, 1.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);	
		vgui::GetAnimationController()->RunAnimationCommand(m_pTitleLabel, "Alpha", 255, 0.2f, 1.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);
		vgui::GetAnimationController()->RunAnimationCommand(m_pFilesIcon, "Alpha", 255, 0.2f, 1.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);
		vgui::GetAnimationController()->RunAnimationCommand(m_pFilesIconShadow, "Alpha", 30, 0.2f, 1.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);
		vgui::GetAnimationController()->RunAnimationCommand(m_pDeviceIcon, "Alpha", 255, 0.2f, 1.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);
		vgui::GetAnimationController()->RunAnimationCommand(m_pDeviceIconShadow, "Alpha", 30, 0.2f, 1.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);	
		vgui::GetAnimationController()->RunAnimationCommand(m_pProgressBarBackdrop, "Alpha", 255, 0.2f, 1.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);
		vgui::GetAnimationController()->RunAnimationCommand(m_pProgressBar, "Alpha", 255, 0.2f, 1.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);	
	}
}

void CASW_VGUI_Computer_Download_Docs::ApplySettingAndFadeLabelIn(vgui::Label* pLabel)
{
	pLabel->SetAlpha(0);
	pLabel->SetFgColor(Color(255,255,255,255));
	pLabel->SetBgColor(Color(0,0,0,96));
	vgui::GetAnimationController()->RunAnimationCommand(pLabel, "Alpha", 255, 0, 1.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);
}


void CASW_VGUI_Computer_Download_Docs::OnThink()
{	
	int x,y,w,t;
	GetBounds(x,y,w,t);

	SetPos(0,0);

	// check if the arrow needs to restart its scroll across
	int ax, ay;
	m_pArrow->GetPos(ax, ay);
	int iw = 96 * m_fScale;
	int ix = w * 0.75f - (iw * 0.5f);	
	if (ax >= ix && !m_bSetTextComplete)
	{
		//Msg("w = %d ix = %d scale = %f iw = %d ax = %d\n", w, ix, m_fScale, iw, ax);
		m_pArrow->SetAlpha(0);
		ax = GetWide() * 0.29f - (iw * 0.5f);
		m_pArrow->SetPos(ax, ay);		
		vgui::GetAnimationController()->RunAnimationCommand(m_pArrow, "Alpha", 255, 0, 0.1f, vgui::AnimationController::INTERPOLATOR_LINEAR);
		vgui::GetAnimationController()->RunAnimationCommand(m_pArrow, "xpos", ix, 0, 1.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);
	}	

	// update the progress bar to match the % complete of the computer area
	float fFraction = 0.0f;
	if (m_pHackComputer && m_pHackComputer->GetComputerArea())
	{
		fFraction = m_pHackComputer->GetComputerArea()->GetHackProgress();

		m_pBackButton->SetVisible( fFraction >= 1.0f || m_pHackComputer->GetComputerArea()->GetNumMenuOptions() > 1 );
	}
	
	m_pProgressBar->SetSize(BAR_WIDTH * fFraction, BAR_HEIGHT);
	m_pProgressBar->SetPos(0.5f * w - (BAR_WIDTH * 0.5f),
									0.6f * t + BAR_PADDING);
	if (fFraction >= 1.0f && !m_bSetTextComplete)
	{
		m_bSetTextComplete = true;
		m_pTitleLabel->SetText("#asw_SynTekDownloadDocsComplete");
		// fade out the downloading anim
		vgui::GetAnimationController()->RunAnimationCommand(m_pArrow, "Alpha", 0, 0, 0.1f, vgui::AnimationController::INTERPOLATOR_LINEAR);
	}

	// update the alpha of the icon
	if (!m_bSetTextComplete)
	{
		float fAlpha = 1.0f;
		int ix1 = GetWide() * 0.29f - (iw * 0.5f);
		float fAnimFraction = float(ax - ix1) / float(ix - ix1);		
		if (fAnimFraction < 0.25f)
		{
			fAlpha = fAnimFraction / 0.25f;
		}
		else if (fAnimFraction > 0.75f)
		{
			fAlpha = (1.0f - fAnimFraction) / 0.25f;
		}
		m_pArrow->SetAlpha(m_pProgressBar->GetAlpha() * fAlpha);
	}

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

bool CASW_VGUI_Computer_Download_Docs::MouseClick(int x, int y, bool bRightClick, bool bDown)
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
	return false;
}

void CASW_VGUI_Computer_Download_Docs::OnCommand(char const* command)
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