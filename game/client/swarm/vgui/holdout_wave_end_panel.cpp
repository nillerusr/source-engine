#include "cbase.h"
#include "holdout_wave_end_panel.h"
#include "vgui_controls/ImagePanel.h"
#include "vgui_controls/Label.h"
#include <vgui/ILocalize.h>
#include "clientmode_asw.h"
#include "vgui_controls/AnimationController.h"
#include "asw_holdout_mode.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


Holdout_Wave_End_Panel::Holdout_Wave_End_Panel( vgui::Panel *parent, const char *name ) : vgui::EditablePanel( parent, name )
{	
	m_WaveCompleteLabel = new vgui::Label( this, "WaveCompleteLabel", "" );
	m_flClosePanelTime = 0;
}

Holdout_Wave_End_Panel::~Holdout_Wave_End_Panel()
{

}

void Holdout_Wave_End_Panel::PerformLayout()
{
	BaseClass::PerformLayout();
}

void Holdout_Wave_End_Panel::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	LoadControlSettings( "resource/UI/HoldoutWaveEndPanel.res" );

	GetClientMode()->GetViewportAnimationController()->StartAnimationSequence( this, "WaveComplete" );
}

void Holdout_Wave_End_Panel::Init( int nWave, float flDuration )
{
	m_nWave = nWave;
	m_flClosePanelTime = gpGlobals->curtime + flDuration;

	if ( !ASWHoldoutMode() )
		return;

	if ( m_nWave >= ASWHoldoutMode()->GetNumWaves() - 1 )
	{
		m_WaveCompleteLabel->SetText( "#asw_holdout_complete" );
	}
	else
	{
		wchar_t wzValue[5];
		_snwprintf( wzValue, ARRAYSIZE( wzValue ), L"%d", m_nWave + 1 );

		wchar_t wbuffer[ 256 ];		
		g_pVGuiLocalize->ConstructString( wbuffer, sizeof(wbuffer),
			g_pVGuiLocalize->Find("#asw_holdout_wave_complete"), 1,
			wzValue);
		m_WaveCompleteLabel->SetText( wbuffer );
	}

	MakeReadyForUse();
	GetClientMode()->GetViewportAnimationController()->StartAnimationSequence( this, "WaveComplete" );
}

void Holdout_Wave_End_Panel::OnThink()
{
	BaseClass::OnThink();

	if ( gpGlobals->curtime > m_flClosePanelTime )
	{
		MarkForDeletion();
	}
}