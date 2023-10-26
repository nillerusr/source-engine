//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Floating numbers
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "hudelement.h"
#include "hud_macros.h"
#include <game_controls/baseviewport.h>
#include <vgui_controls/controls.h>
#include <vgui_controls/panel.h>
#include <vgui_controls/ImagePanel.h>
#include <vgui_controls/TextImage.h>
#include "vgui/ILocalize.h"
#include "asw_hud_floating_number.h"
#include "iclientmode.h"
#include "cdll_int.h"
#include <vgui/ISurface.h>
#include <vgui_controls/AnimationController.h>
#include "fmtstr.h"
#include "engine/IEngineSound.h"
#include "engine/IVDebugOverlay.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;


//-----------------------------------------------------------------------------
// Purpose: Floating numbers
//-----------------------------------------------------------------------------

CFloatingNumber::CFloatingNumber( int iProgress, const floating_number_params_t &params, vgui::Panel* pParent )
		: BaseClass( pParent, "FloatingNumber" )
{
	char szLabel[64];
	Q_snprintf( szLabel, sizeof( szLabel ), "%s%d", params.bShowPlus ? "+" : "", iProgress );

	m_params = params;

	Initialize( szLabel );
}

CFloatingNumber::CFloatingNumber( const char *pchText, const floating_number_params_t &params, vgui::Panel* pParent )
: BaseClass( pParent, "FloatingNumber" )
{
	m_params = params;

	Initialize( pchText );
}

CFloatingNumber::~CFloatingNumber()
{
	delete m_pNumberLabel;
}

void CFloatingNumber::Initialize( const char *pchText )
{
	vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFile("resource/SwarmSchemeNew.res", "SwarmSchemeNew");
	SetScheme(scheme);	

	m_fStartTime = gpGlobals->curtime + m_params.flStartDelay;

	m_pNumberLabel = new vgui::Label( this, "FloatingNumberLabel", pchText );
}

void CFloatingNumber::ApplySchemeSettings( vgui::IScheme *scheme )
{
	BaseClass::ApplySchemeSettings( scheme );

	LoadControlSettings( "resource/UI/HudFloatingNumber.res" );

	if ( m_params.hFont != NULL )
	{
		m_pNumberLabel->SetFont( m_params.hFont );
	}

	wchar_t wszText[ 256 ];
	m_pNumberLabel->GetText( wszText, 256 );

	surface()->GetTextSize( m_pNumberLabel->GetFont(), wszText, m_iTextWide, m_iTextTall );
	
	m_pNumberLabel->SetSize( m_iTextWide, m_iTextTall );
	SetSize( m_iTextWide, m_iTextTall );

	m_pNumberLabel->SetAlpha( 0 );

	m_pNumberLabel->SetFgColor( Color( m_params.rgbColor.r(), m_params.rgbColor.g(), m_params.rgbColor.b(), 255 ) );

	if ( m_params.bWorldSpace )
	{
		PositionWorldSpaceNumber();
	}
	else
	{
		PositionNumber( m_params.x, m_params.y );
	}
}

void CFloatingNumber::PositionNumber( int x, int y )
{
	switch ( m_params.alignment )
	{
	case Label::a_north:
		SetPos( x - m_iTextWide/2, y );
		break;
	case Label::a_northeast:
		SetPos( x - m_iTextWide, y );
		break;
	case Label::a_west:
		SetPos( x, y - m_iTextTall/2 );
		break;
	case Label::a_center:
		SetPos( x - m_iTextWide/2, y - m_iTextTall/2 );
		break;
	case Label::a_east:
		SetPos( x - m_iTextWide, y - m_iTextTall/2 );
		break;
	case Label::a_southwest:
		SetPos( x, y - m_iTextTall );
		break;
	case Label::a_south:
		SetPos( x - m_iTextWide/2, y - m_iTextTall );
		break;
	case Label::a_southeast:
		SetPos( x - m_iTextWide, y - m_iTextTall );
		break;
	case Label::a_northwest:
	default:
		SetPos( x, y );
	}
}

void CFloatingNumber::PositionWorldSpaceNumber()
{
	Vector screenPos;
	debugoverlay->ScreenPosition( m_params.vecPos, screenPos );

	float flProgress = ( gpGlobals->curtime - m_fStartTime ) / m_params.flMoveDuration;
	flProgress = clamp( flProgress, 0.0f, 1.0f );
	m_params.x = screenPos.x;
	m_params.y = screenPos.y - flProgress * m_iScrollDistance;

	PositionNumber( m_params.x, m_params.y );
}

//-----------------------------------------------------------------------------
// Purpose: Delete panel when floating number has faded out
//-----------------------------------------------------------------------------
void CFloatingNumber::OnThink()
{
	if ( m_params.bWorldSpace )
	{
		PositionWorldSpaceNumber();
	}

	if ( gpGlobals->curtime > m_fStartTime + m_params.flFadeStart )
	{
		if ( m_pNumberLabel->GetAlpha() >=  255 )
		{
			m_pNumberLabel->SetAlpha( 254 );
			GetAnimationController()->RunAnimationCommand( m_pNumberLabel, "alpha", 0, 0.0, m_params.flFadeDuration, vgui::AnimationController::INTERPOLATOR_LINEAR );
		}
		else if ( m_pNumberLabel->GetAlpha() <= 0 )
		{
			MarkForDeletion();
			SetVisible( false );
		}
	}
	else if ( gpGlobals->curtime > m_fStartTime )
	{
		if ( m_pNumberLabel->GetAlpha() <= 0 )
		{
			m_pNumberLabel->SetAlpha( 1 );

			GetAnimationController()->RunAnimationCommand( m_pNumberLabel, "alpha", 255, 0, 0.3, vgui::AnimationController::INTERPOLATOR_LINEAR );

			if ( !m_params.bWorldSpace )
			{
				switch ( m_params.iDir )
				{
				default:
				case FN_DIR_UP:
					vgui::GetAnimationController()->RunAnimationCommand( this, "ypos", m_params.y - m_iScrollDistance, 0, m_params.flMoveDuration, vgui::AnimationController::INTERPOLATOR_LINEAR );
					break;
				case FN_DIR_DOWN:
					vgui::GetAnimationController()->RunAnimationCommand( this, "ypos", m_params.y + m_iScrollDistance, 0, m_params.flMoveDuration, vgui::AnimationController::INTERPOLATOR_LINEAR );
					break;
				case FN_DIR_LEFT:
					vgui::GetAnimationController()->RunAnimationCommand( this, "xpos", m_params.x - m_iScrollDistance, 0, m_params.flMoveDuration, vgui::AnimationController::INTERPOLATOR_LINEAR );
					break;
				case FN_DIR_RIGHT:
					vgui::GetAnimationController()->RunAnimationCommand( this, "xpos", m_params.x + m_iScrollDistance, 0, m_params.flMoveDuration, vgui::AnimationController::INTERPOLATOR_LINEAR );
					break;
				}
			}
		}
	}
}
