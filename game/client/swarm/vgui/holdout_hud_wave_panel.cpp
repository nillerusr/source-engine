#include "cbase.h"
#include "holdout_hud_wave_panel.h"
#include "vgui_controls/ImagePanel.h"
#include "vgui_controls/Label.h"
#include <vgui/ILocalize.h>
#include "asw_holdout_mode.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


Holdout_Hud_Wave_Panel::Holdout_Hud_Wave_Panel( vgui::Panel *parent, const char *name ) : vgui::EditablePanel( parent, name )
{	
	m_pWaveNumberLabel = new vgui::Label( this, "WaveNumber", "" );
	m_pScoreLabel = new vgui::Label( this, "ScoreLabel", "" );
	m_pWaveProgressBarBG = new vgui::Panel( this, "WaveProgressBarBG" );
	m_pWaveProgressBar = new vgui::Panel( this, "WaveProgressBar" );
	m_pCountdownLabel = new vgui::Label( this, "WavePanelCountdownLabel", "" );
	m_pCountdownTimeLabel = new vgui::Label( this, "WavePanelCountdownTimeLabel", "" );
}
	
Holdout_Hud_Wave_Panel::~Holdout_Hud_Wave_Panel()
{

}

void Holdout_Hud_Wave_Panel::PerformLayout()
{
	BaseClass::PerformLayout();

	if ( !ASWHoldoutMode() )
		return;

	UpdateWaveProgressBar();
}

void Holdout_Hud_Wave_Panel::UpdateWaveProgressBar()
{
	int border = YRES( 1 );
	int x, y, w, h;
	m_pWaveProgressBarBG->GetBounds( x, y, w, h );

	float flWaveProgress = ASWHoldoutMode()->GetWaveProgress();

	m_pWaveProgressBar->SetPos( x + border, y + border );
	m_pWaveProgressBar->SetWide( ( w - border * 2 ) * flWaveProgress );
	m_pWaveProgressBar->SetTall( h - border * 2 );
}

void Holdout_Hud_Wave_Panel::OnThink()
{
	if ( !ASWHoldoutMode() )
		return;
	wchar_t wzValue[15];
	_snwprintf( wzValue, ARRAYSIZE( wzValue ), L"%d", ASWHoldoutMode()->GetCurrentWave() + 1 );

	wchar_t wbuffer[ 256 ];		
	g_pVGuiLocalize->ConstructString( wbuffer, sizeof(wbuffer),
		g_pVGuiLocalize->Find("#asw_holdout_hud_wave"), 1,
		wzValue);
	m_pWaveNumberLabel->SetText( wbuffer );

	_snwprintf( wzValue, ARRAYSIZE( wzValue ), L"%d", ASWHoldoutMode()->GetCurrentScore() );
	g_pVGuiLocalize->ConstructString( wbuffer, sizeof(wbuffer),
		g_pVGuiLocalize->Find("#asw_holdout_hud_score"), 1,
		wzValue);
	m_pScoreLabel->SetText( wbuffer );

	// timer
	float flTimeLeft = ASWHoldoutMode()->GetResupplyEndTime() - gpGlobals->curtime;
	flTimeLeft = MAX( 0, flTimeLeft );
	if ( flTimeLeft <= 0 )
	{
		m_pCountdownTimeLabel->SetVisible( false );
		m_pCountdownLabel->SetVisible( false );
	}
	else
	{
		m_pCountdownTimeLabel->SetVisible( true );
		m_pCountdownLabel->SetVisible( true );

		wchar_t wzValue[15];
		_snwprintf( wzValue, ARRAYSIZE( wzValue ), L"%d", (int) flTimeLeft );

		m_pCountdownTimeLabel->SetText( wzValue );
	}

	UpdateWaveProgressBar();
}

void Holdout_Hud_Wave_Panel::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	LoadControlSettings( "resource/UI/HoldoutHudWavePanel.res" );
}