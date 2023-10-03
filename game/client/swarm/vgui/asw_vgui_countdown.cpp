#include "cbase.h"
#include "asw_vgui_countdown.h"
#include "vgui/ISurface.h"
#include "vgui_controls/TextImage.h"
#include "vgui_controls/ImagePanel.h"
#include <vgui/IInput.h>
#include "c_asw_marine.h"
#include "c_asw_marine_resource.h"
#include "asw_marine_profile.h"
#include "c_asw_player.h"
#include "asw_equipment_list.h"
#include "asw_weapon_parse.h"
#include "c_asw_game_resource.h"
#include <vgui_controls/AnimationController.h>
#include <vgui_controls/Label.h>
#include "idebugoverlaypanel.h"
#include "engine/IVDebugOverlay.h"
#include "iasw_client_vehicle.h"
#include "iclientmode.h"
#include "asw_gamerules.h"
#include "iinput.h"
#include "hud.h"
#include "c_asw_objective_countdown.h"
#include "vgui_controls/AnimationController.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CASW_VGUI_Countdown_Panel::CASW_VGUI_Countdown_Panel( vgui::Panel *pParent, const char *pElementName, C_ASW_Objective_Countdown *pCountdownObjective )
:	vgui::EditablePanel( pParent, pElementName )
{	
	SetProportional( true );
	m_hCountdownObjective = pCountdownObjective;

	m_pBackgroundPanel = new vgui::Panel( this, "NukeBackground" );
	m_pWarningGlowLabel = new vgui::Label(this, "WarningGlowLabel", "#asw_nuclear_warning");
	m_pWarningLabel = new vgui::Label(this, "WarningLabel", "#asw_nuclear_warning");

	m_pDetonationGlowLabel = new vgui::Label(this, "DetonationGlowLabel", "#asw_nuclear_detonationi");
	m_pDetonationLabel = new vgui::Label(this, "DetonationLabel", "#asw_nuclear_detonationi");

	m_pCountdownGlowLabel = new vgui::Label(this, "CountdownGlowLabel", "");
	m_pCountdownLabel = new vgui::Label(this, "CountdownLabel", "");

	m_iTimeLeft = 0;

	m_bCheckedInitialTime = false;
	m_bPlayed60 = m_bPlayed30 = m_bPlayed10 = false;
	m_bShown = false;
}

void CASW_VGUI_Countdown_Panel::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	LoadControlSettings( "resource/UI/NukeDetonation.res" );
		
	if (m_hCountdownObjective.Get() && m_hCountdownObjective.Get()->GetCountdownFinishTime() != 0)
	{
		m_bShown = false;
		SetAlpha( 1 );
		ShowCountdown( true );
		SlideOut( 4.0f );
	}
	else
	{
		SetAlpha( 1 );
		m_bShown = true;
		ShowCountdown( false );
	}
}

void CASW_VGUI_Countdown_Panel::ShowCountdown( bool bShow )
{
	if ( bShow != m_bShown )
	{
		m_bShown = bShow;

		float flDelay = 0;
		float flTransitionTime = 0.5f;
		if ( m_bShown )
		{
			vgui::GetAnimationController()->RunAnimationCommand( this, "Alpha", 255, flDelay, flTransitionTime, vgui::AnimationController::INTERPOLATOR_LINEAR );
		}
		else
		{
			vgui::GetAnimationController()->RunAnimationCommand( this, "Alpha", 0, flDelay, flTransitionTime, vgui::AnimationController::INTERPOLATOR_LINEAR );
		}
	}

	if ( m_bShown )
	{
		m_pCountdownLabel->SetVisible(true);
		m_pCountdownGlowLabel->SetVisible(true);
		m_pWarningGlowLabel->SetVisible(true);
		m_pWarningLabel->SetVisible(true);
		m_pDetonationGlowLabel->SetVisible(true);
		m_pDetonationLabel->SetVisible(true);
		m_pBackgroundPanel->SetVisible(true);
		m_pWarningLabel->SetAlpha( 255 );
		m_pWarningGlowLabel->SetAlpha( 255 );
	}
	else
	{
		m_pCountdownLabel->SetVisible(false);
		m_pCountdownGlowLabel->SetVisible(false);
		m_pWarningGlowLabel->SetVisible(false);
		m_pWarningLabel->SetVisible(false);
		m_pDetonationGlowLabel->SetVisible(false);
		m_pDetonationLabel->SetVisible(false);
		m_pBackgroundPanel->SetVisible(false);
	}
}

void CASW_VGUI_Countdown_Panel::SlideOut( float flDelay )
{
	// make us shrink after a couple of seconds
	float flTransitionTime = 0.5f;
	vgui::GetAnimationController()->RunAnimationCommand( m_pWarningGlowLabel, "Alpha", 0, flDelay, flTransitionTime * 0.5f, vgui::AnimationController::INTERPOLATOR_LINEAR );
	vgui::GetAnimationController()->RunAnimationCommand( m_pWarningLabel, "Alpha", 0, flDelay, flTransitionTime * 0.5f, vgui::AnimationController::INTERPOLATOR_LINEAR );

	int wide = YRES( 155 );
	int tall = YRES( 35 );
	int det_y = YRES( 5 );
	int det_tall = YRES( 8 );
	int number_tall = YRES( 14 );
	int number_y = YRES( 15 );
	int buffer = YRES( 10 );
	vgui::GetAnimationController()->RunAnimationCommand( this, "xpos", ScreenWidth() - ( wide + buffer ), flDelay, flTransitionTime, vgui::AnimationController::INTERPOLATOR_LINEAR );
	vgui::GetAnimationController()->RunAnimationCommand( this, "ypos", buffer, flDelay, flTransitionTime, vgui::AnimationController::INTERPOLATOR_LINEAR );
	vgui::GetAnimationController()->RunAnimationCommand( this, "wide", wide, flDelay, flTransitionTime, vgui::AnimationController::INTERPOLATOR_LINEAR );
	vgui::GetAnimationController()->RunAnimationCommand( this, "tall", tall, flDelay, flTransitionTime, vgui::AnimationController::INTERPOLATOR_LINEAR );
	vgui::GetAnimationController()->RunAnimationCommand( m_pBackgroundPanel, "wide", wide, flDelay, flTransitionTime, vgui::AnimationController::INTERPOLATOR_LINEAR );
	vgui::GetAnimationController()->RunAnimationCommand( m_pBackgroundPanel, "tall", tall, flDelay, flTransitionTime, vgui::AnimationController::INTERPOLATOR_LINEAR );
	vgui::GetAnimationController()->RunAnimationCommand( m_pDetonationLabel, "wide", wide, flDelay, flTransitionTime, vgui::AnimationController::INTERPOLATOR_LINEAR );
	vgui::GetAnimationController()->RunAnimationCommand( m_pDetonationGlowLabel, "wide", wide, flDelay, flTransitionTime, vgui::AnimationController::INTERPOLATOR_LINEAR );
	vgui::GetAnimationController()->RunAnimationCommand( m_pCountdownLabel, "wide", wide, flDelay, flTransitionTime, vgui::AnimationController::INTERPOLATOR_LINEAR );
	vgui::GetAnimationController()->RunAnimationCommand( m_pCountdownGlowLabel, "wide", wide, flDelay, flTransitionTime, vgui::AnimationController::INTERPOLATOR_LINEAR );

	vgui::GetAnimationController()->RunAnimationCommand( m_pDetonationLabel, "ypos", det_y, flDelay, flTransitionTime, vgui::AnimationController::INTERPOLATOR_LINEAR );
	vgui::GetAnimationController()->RunAnimationCommand( m_pDetonationGlowLabel, "ypos", det_y, flDelay, flTransitionTime, vgui::AnimationController::INTERPOLATOR_LINEAR );
	vgui::GetAnimationController()->RunAnimationCommand( m_pCountdownLabel, "ypos", number_y, flDelay, flTransitionTime, vgui::AnimationController::INTERPOLATOR_LINEAR );
	vgui::GetAnimationController()->RunAnimationCommand( m_pCountdownGlowLabel, "ypos", number_y, flDelay, flTransitionTime, vgui::AnimationController::INTERPOLATOR_LINEAR );

	vgui::GetAnimationController()->RunAnimationCommand( m_pDetonationLabel, "tall", det_tall, flDelay, flTransitionTime, vgui::AnimationController::INTERPOLATOR_LINEAR );
	vgui::GetAnimationController()->RunAnimationCommand( m_pDetonationGlowLabel, "tall", det_tall, flDelay, flTransitionTime, vgui::AnimationController::INTERPOLATOR_LINEAR );
	vgui::GetAnimationController()->RunAnimationCommand( m_pCountdownLabel, "tall", number_tall, flDelay, flTransitionTime, vgui::AnimationController::INTERPOLATOR_LINEAR );
	vgui::GetAnimationController()->RunAnimationCommand( m_pCountdownGlowLabel, "tall", number_tall, flDelay, flTransitionTime, vgui::AnimationController::INTERPOLATOR_LINEAR );
}

void CASW_VGUI_Countdown_Panel::OnThink()
{
	if (m_hCountdownObjective.Get() && m_hCountdownObjective.Get()->GetCountdownFinishTime() != 0)
	{
		if ( !m_pCountdownLabel->IsVisible() )
		{
			SetAlpha( 1 );
			ShowCountdown( true );
			SlideOut( 5.0f );
		}
		int iTimeLeft = m_hCountdownObjective.Get()->GetCountdownFinishTime() - gpGlobals->curtime;
		if (!m_bCheckedInitialTime)
		{
			// check if the countdown is starting too late to play some of the speech
			if (iTimeLeft < 60)
				m_bPlayed60 = true;
			if (iTimeLeft < 30)
				m_bPlayed30 = true;
			if (iTimeLeft < 10)
				m_bPlayed10 = true;
			m_bCheckedInitialTime = true;
		}
		if (iTimeLeft != m_iTimeLeft)
		{
			m_iTimeLeft = iTimeLeft;
			if (iTimeLeft >= 0)
			{
				char buffer[8];
				Q_snprintf(buffer, sizeof(buffer), "%d", iTimeLeft);

				wchar_t wnumber[8];
				g_pVGuiLocalize->ConvertANSIToUnicode(buffer, wnumber, sizeof( wnumber ));

				wchar_t wbuffer[255];		
				g_pVGuiLocalize->ConstructString( wbuffer, sizeof(wbuffer),
					g_pVGuiLocalize->Find("#asw_nuclear_detonation"), 1,
						wnumber);

				m_pCountdownLabel->SetText(wbuffer);
				m_pCountdownGlowLabel->SetText(wbuffer);
			}
			else
			{
				m_pCountdownLabel->SetText("");
				m_pCountdownGlowLabel->SetText("");				
			}

			// check for playing speech
			if (iTimeLeft <= 63 && !m_bPlayed60)
			{
				CLocalPlayerFilter filter;
				C_BaseEntity::EmitSound( filter, -1 /*SOUND_FROM_LOCAL_PLAYER*/, "ASWCountdown.Countdown60" );
				m_bPlayed60 = true;
			}
			if (iTimeLeft <= 33 && !m_bPlayed30)
			{
				CLocalPlayerFilter filter;
				C_BaseEntity::EmitSound( filter, -1 /*SOUND_FROM_LOCAL_PLAYER*/, "ASWCountdown.Countdown30" );
				m_bPlayed30 = true;
			}
			if (iTimeLeft <= 13 && !m_bPlayed10)
			{
				CLocalPlayerFilter filter;
				C_BaseEntity::EmitSound( filter, -1 /*SOUND_FROM_LOCAL_PLAYER*/, "ASWCountdown.Countdown10" );
				m_bPlayed10 = true;
			}
		}
	}
	else
	{
		ShowCountdown( false );
	}
}