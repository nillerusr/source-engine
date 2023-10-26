#include "cbase.h"
#include "asw_vgui_stylin_cam.h"
#include "vgui/ISurface.h"
#include "vgui_controls/TextImage.h"
#include "vgui_controls/ImagePanel.h"
#include <vgui/IInput.h>
#include "c_asw_marine.h"
#include "c_asw_marine_resource.h"
#include "asw_marine_profile.h"
#include "c_asw_player.h"
#include "asw_weapon_medical_satchel_shared.h"
#include "asw_vgui_computer_camera.h"
#include "asw_equipment_list.h"
#include "asw_weapon_parse.h"
#include "c_asw_game_resource.h"
#include <vgui_controls/AnimationController.h>
#include "idebugoverlaypanel.h"
#include "engine/IVDebugOverlay.h"
#include "iasw_client_vehicle.h"
#include "iclientmode.h"
#include "asw_gamerules.h"
#include "iinput.h"
#include "hud.h"
#include "c_asw_point_camera.h"
#include "asw_util_shared.h"
#include "game_timescale_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar asw_stim_cam_time("asw_stim_cam_time", "0.6", FCVAR_CHEAT, "When time scale drops below this value, the stim cam will show");
extern ConVar asw_spinning_stim_cam;

CASW_VGUI_Stylin_Cam* s_pStylinCamera = NULL;

DECLARE_HUDELEMENT( CASW_VGUI_Stylin_Cam );

CASW_VGUI_Stylin_Cam::CASW_VGUI_Stylin_Cam( const char *pElementName ) 
:	CASW_HudElement( pElementName ), BaseClass( NULL, "ASWHudStylinCam" )
{	
	vgui::Panel *pParent = GetClientMode()->GetViewport();
	SetParent( pParent );

	vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFile("resource/SwarmSchemeNew.res", "SwarmSchemeNew");
	SetScheme(scheme);
	
	m_pCameraImage = new vgui::ImagePanel(this, "StylinCameraImage");
	m_pCameraImage->SetShouldScaleImage( true );

	m_pCommanderImage = new vgui::ImagePanel(this, "CommanderImage");
	m_pCommanderImage->SetShouldScaleImage( true );

	m_pCommanderFlash = new vgui::ImagePanel( this, "CommanderFlash" );
	m_pCommanderFlash->SetShouldScaleImage( true );
	m_pCommanderFlash->SetZPos( 2 );

	m_bFadingOutCameraImage = false;
	m_bFadingInCameraImage = false;
	m_bFadingOutCommanderFace = false;
	m_bFadingInCommanderFace = false;
	
}

CASW_VGUI_Stylin_Cam::~CASW_VGUI_Stylin_Cam()
{
	if (s_pStylinCamera == this)
		s_pStylinCamera = NULL;
}

// camera view takes up our full space
void CASW_VGUI_Stylin_Cam::PerformLayout()
{
	BaseClass::PerformLayout();

	int x = ScreenWidth() * 0.01f;
	int y = ScreenHeight() * 0.40f;
	int w = GetStylinCamSize();
	int h = w;

	SetBounds( x, y, w, h );
	if ( ShouldShowStylinCam() )
	{
		m_pCameraImage->SetBounds( 0, 0, w, h );		
	}
	else
	{
		m_pCameraImage->SetBounds( 0, 0, 0, 0 );
	}

	if ( ShouldShowCommanderFace() )
	{
		m_pCommanderImage->SetBounds( 0, 0, YRES( 82 ), YRES( 110 ) );
		m_pCommanderFlash->SetBounds( 0, 0, YRES( 82 ), YRES( 110 ) );
	}
	else
	{
		m_pCommanderImage->SetBounds( 0, YRES( 55 ), YRES( 82 ), 0 );
		m_pCommanderFlash->SetBounds( 0, YRES( 55 ), YRES( 82 ), 0 );
	}	
}

void CASW_VGUI_Stylin_Cam::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	SetPaintBackgroundType(0);
	SetPaintBackgroundEnabled(false);
	SetBgColor( Color(0,0,0,0) );
	SetAlpha(255);
	SetMouseInputEnabled(false);

	m_pCameraImage->SetAlpha( 0 );
	m_pCameraImage->SetImage( "swarm/Computer/ComputerCamera" );

	m_pCommanderImage->SetAlpha( 0 );
	m_pCommanderImage->SetImage( "briefing/face_pilot" );
	m_pCommanderFlash->SetAlpha( 255 );
	m_pCommanderFlash->SetImage( "white" );

	m_bFadingInCommanderFace = false;
	m_bFadingOutCommanderFace = false;
}

bool CASW_VGUI_Stylin_Cam::ShouldShowStylinCam()
{
	if ( !ASWGameRules() )
		return false;

	if ( ASWGameRules()->ShouldShowCommanderFace() )
		return false;

	C_ASW_Player *pLocal = C_ASW_Player::GetLocalASWPlayer();
	bool bHasMarine = pLocal && pLocal->GetMarine();

	bool bAdrenalineActive = GameTimescale()->GetCurrentTimescale() <= asw_stim_cam_time.GetFloat() && ASWGameRules()->GetStimEndTime() >= gpGlobals->curtime;
	bool bShowCam = ASWGameRules()->ShouldForceStylinCam() ||
		( bAdrenalineActive && bHasMarine );

	if ( bShowCam && !ASWGameRules()->ShouldShowCommanderFace() )
	{
		// check if there's a mapper designed camera turned on
		C_PointCamera *pCameraEnt = GetPointCameraList();
		bool bMapperCam = false;
		for ( int cameraNum = 0; pCameraEnt != NULL; pCameraEnt = pCameraEnt->m_pNext )
		{
			if ( pCameraEnt != pLocal->GetStimCam() && pCameraEnt->IsActive() && !pCameraEnt->IsDormant()
				&& !(ASW_IsSecurityCam(pCameraEnt) && asw_stim_cam_time.GetFloat()!=2.0f))
			{							
				bMapperCam = true;
				break;
			}
			++cameraNum;
		}
		if ( !bMapperCam && !asw_spinning_stim_cam.GetBool() && !ASWGameRules()->ShouldShowCommanderFace() )	// don't show the cam if the mapper hasn't set a specific view, unless the convar is set
			bShowCam = false;
	}

	return bShowCam;
}

bool CASW_VGUI_Stylin_Cam::ShouldShowCommanderFace()
{
	return ASWGameRules() && ASWGameRules()->ShouldShowCommanderFace();
}

void CASW_VGUI_Stylin_Cam::OnThink()
{
	if ( !ASWGameRules() )
		return;
	
	// should fade the camera view in and out depending on if we're in slomo or not
	if (m_bFadingOutCameraImage)
	{
		if (m_pCameraImage->GetAlpha() <= 0)
		{
			m_bFadingOutCameraImage = false;
			m_bFadingInCameraImage= false;
		}
	}
	else if (m_bFadingInCameraImage)
	{
		if (m_pCameraImage->GetAlpha() >= 255)
		{
			m_bFadingInCameraImage = false;
			m_bFadingOutCameraImage = false;
		}
	}
	else
	{
		bool bShow = ShouldShowStylinCam();
		if ( bShow && m_pCameraImage->GetAlpha() != 255 )
		{
			m_bFadingInCameraImage = true;
			m_bFadingOutCameraImage = false;
			vgui::GetAnimationController()->RunAnimationCommand(m_pCameraImage, "Alpha", 255, 0, 0.5f, vgui::AnimationController::INTERPOLATOR_LINEAR);
			vgui::GetAnimationController()->RunAnimationCommand(m_pCameraImage, "wide", GetStylinCamSize(), 0, 0.5f, vgui::AnimationController::INTERPOLATOR_LINEAR);
			vgui::GetAnimationController()->RunAnimationCommand(m_pCameraImage, "tall", GetStylinCamSize(), 0, 0.5f, vgui::AnimationController::INTERPOLATOR_LINEAR);				
		}
		else if ( !bShow && m_pCameraImage->GetAlpha() != 0 )
		{
			m_bFadingOutCameraImage = true;
			m_bFadingInCameraImage = false;
			vgui::GetAnimationController()->RunAnimationCommand(m_pCameraImage, "Alpha", 0, 0, 1.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);
			vgui::GetAnimationController()->RunAnimationCommand(m_pCameraImage, "wide", 0, 0, 0.5f, vgui::AnimationController::INTERPOLATOR_LINEAR);
			vgui::GetAnimationController()->RunAnimationCommand(m_pCameraImage, "tall", 0, 0, 0.5f, vgui::AnimationController::INTERPOLATOR_LINEAR);
		}
	}

	// Fade in and out the commander face
	if ( m_bFadingOutCommanderFace )
	{
		if (m_pCommanderImage->GetAlpha() <= 0)
		{
			m_bFadingOutCommanderFace = false;
			m_bFadingInCommanderFace = false;
		}
	}
	else if ( m_bFadingInCommanderFace )
	{
		if ( m_pCommanderImage->GetAlpha() >= 255 )
		{
			m_bFadingInCommanderFace = false;
			m_bFadingOutCommanderFace = false;
		}
	}
	else
	{
		// commander face source art is 1200x1600
		if ( ShouldShowCommanderFace() && m_pCommanderImage->GetAlpha() != 255 )
		{
			m_bFadingInCommanderFace = true;
			m_bFadingOutCommanderFace = false;
			vgui::GetAnimationController()->RunAnimationCommand(m_pCommanderImage, "Alpha", 255, 0, 0.2f, vgui::AnimationController::INTERPOLATOR_LINEAR);
			vgui::GetAnimationController()->RunAnimationCommand(m_pCommanderImage, "wide", YRES( 82 ), 0, 0.2f, vgui::AnimationController::INTERPOLATOR_LINEAR);
			vgui::GetAnimationController()->RunAnimationCommand(m_pCommanderImage, "tall", YRES( 110 ), 0, 0.2f, vgui::AnimationController::INTERPOLATOR_LINEAR);
			vgui::GetAnimationController()->RunAnimationCommand(m_pCommanderImage, "ypos", 0, 0, 0.2f, vgui::AnimationController::INTERPOLATOR_LINEAR);

			m_pCommanderFlash->SetAlpha( 192 );
			vgui::GetAnimationController()->RunAnimationCommand(m_pCommanderFlash, "Alpha", 0, 0, 0.15f, vgui::AnimationController::INTERPOLATOR_LINEAR);
			vgui::GetAnimationController()->RunAnimationCommand(m_pCommanderFlash, "wide", YRES( 82 ), 0, 0.2f, vgui::AnimationController::INTERPOLATOR_LINEAR);
			vgui::GetAnimationController()->RunAnimationCommand(m_pCommanderFlash, "tall", YRES( 110 ), 0, 0.2f, vgui::AnimationController::INTERPOLATOR_LINEAR);
			vgui::GetAnimationController()->RunAnimationCommand(m_pCommanderFlash, "ypos", 0, 0, 0.2f, vgui::AnimationController::INTERPOLATOR_LINEAR);
		}
		else if ( !ShouldShowCommanderFace() && m_pCommanderImage->GetAlpha() != 0 )
		{
			m_bFadingOutCommanderFace = true;
			m_bFadingInCommanderFace = false;			
			vgui::GetAnimationController()->RunAnimationCommand(m_pCommanderImage, "Alpha", 0, 0, 0.2f, vgui::AnimationController::INTERPOLATOR_LINEAR);
			vgui::GetAnimationController()->RunAnimationCommand(m_pCommanderImage, "wide", YRES( 82 ), 0, 0.2f, vgui::AnimationController::INTERPOLATOR_LINEAR);
			vgui::GetAnimationController()->RunAnimationCommand(m_pCommanderImage, "tall", 0, 0, 0.2f, vgui::AnimationController::INTERPOLATOR_LINEAR);
			vgui::GetAnimationController()->RunAnimationCommand(m_pCommanderImage, "ypos", YRES( 55 ), 0, 0.2f, vgui::AnimationController::INTERPOLATOR_LINEAR);

			vgui::GetAnimationController()->RunAnimationCommand(m_pCommanderFlash, "Alpha", 192, 0.05f, 0.15f, vgui::AnimationController::INTERPOLATOR_LINEAR);
			vgui::GetAnimationController()->RunAnimationCommand(m_pCommanderFlash, "wide", YRES( 82 ), 0, 0.2f, vgui::AnimationController::INTERPOLATOR_LINEAR);
			vgui::GetAnimationController()->RunAnimationCommand(m_pCommanderFlash, "tall", 0, 0, 0.2f, vgui::AnimationController::INTERPOLATOR_LINEAR);
			vgui::GetAnimationController()->RunAnimationCommand(m_pCommanderFlash, "ypos", YRES( 55 ), 0, 0.2f, vgui::AnimationController::INTERPOLATOR_LINEAR);
		}
	}
}

int CASW_VGUI_Stylin_Cam::GetStylinCamSize()
{
	return YRES( 110 );
}