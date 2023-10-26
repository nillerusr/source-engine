#include "cbase.h"
#include "nb_suggest_difficulty.h"
#include "nb_button.h"
#include "asw_model_panel.h"
#include <vgui/ISurface.h>
#include "c_asw_player.h"
#include "asw_weapon_parse.h"
#include "asw_equipment_list.h"
#include "vgui_controls/AnimationController.h"
#include "nb_header_footer.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CNB_Suggest_Difficulty::CNB_Suggest_Difficulty( vgui::Panel *parent, const char *name ) : BaseClass( parent, name )
{
	m_pBanner = new CNB_Gradient_Bar( this, "Banner" );
	m_pTitle = new vgui::Label( this, "Title", "" );
	m_pPerformanceLabel = new vgui::Label( this, "PerformanceLabel", "" );
	m_pNoButton = new CNB_Button( this, "NoButton", "", this, "NoButton" );
	m_pYesButton = new CNB_Button( this, "YesButton", "", this, "YesButton" );

	m_bIncrease = true;
}

void CNB_Suggest_Difficulty::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	
	LoadControlSettings( "resource/ui/nb_suggest_difficulty.res" );

	SetAlpha( 0 );
	vgui::GetAnimationController()->RunAnimationCommand( this, "alpha", 255, 0, 0.5f, vgui::AnimationController::INTERPOLATOR_LINEAR );

	CLocalPlayerFilter filter;
	C_BaseEntity::EmitSound( filter, -1 /*SOUND_FROM_LOCAL_PLAYER*/, "ASWInterface.SelectWeapon" );
}

void CNB_Suggest_Difficulty::PerformLayout()
{
	KeyValues *pKV = new KeyValues( "ItemModelPanel" );
	pKV->SetInt( "fov", 20 );
	pKV->SetInt( "start_framed", 0 );
	pKV->SetInt( "disable_manipulation", 1 );

	BaseClass::PerformLayout();
}

void CNB_Suggest_Difficulty::OnCommand( const char *command )
{
	if ( !Q_stricmp( command, "NoButton" ) )
	{
		MarkForDeletion();
		return;
	}
	if ( !Q_stricmp( command, "YesButton" ) )
	{
		engine->ClientCmd( m_bIncrease ? "cl_skill 3" : "cl_skill 1" );

		MarkForDeletion();
		return;
	}
	BaseClass::OnCommand( command );
}

void CNB_Suggest_Difficulty::PaintBackground()
{
	// fill background
	float flAlpha = 200.0f / 255.0f;
	vgui::surface()->DrawSetColor( Color( 16, 32, 46, 230 * flAlpha ) );
	vgui::surface()->DrawFilledRect( 0, 0, GetWide(), GetTall() );
}

void CNB_Suggest_Difficulty::SetSuggestIncrease( void )
{
	m_bIncrease = true;
	UpdateDetails();
}

void CNB_Suggest_Difficulty::SetSuggestDecrease( void )
{
	m_bIncrease = false;
	UpdateDetails();
}

void CNB_Suggest_Difficulty::UpdateDetails()
{
	m_pPerformanceLabel->SetText( m_bIncrease ? "#asw_suggest_harder" : "#asw_suggest_easier" );
}