#include "cbase.h"
#include "holdout_wave_announce_panel.h"
#include "vgui_controls/ImagePanel.h"
#include "vgui_controls/Label.h"
#include <vgui/ILocalize.h>
#include "clientmode_asw.h"
#include "vgui_controls/AnimationController.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


Holdout_Wave_Announce_Panel::Holdout_Wave_Announce_Panel( vgui::Panel *parent, const char *name ) : vgui::EditablePanel( parent, name )
{	
	SetProportional( true );
	m_pWaveAnnounceLabel = new vgui::Label( this, "WaveAnnounceLabel", "" );
	m_pGetReadyLabel = new vgui::Label( this, "GetReadyLabel", "" );
	m_flWaveNumberSoundTime = 0.0f;
	m_flGetReadySoundTime = 0.0f;
	m_flGetReadySlideSoundTime = 0.0f;
}

Holdout_Wave_Announce_Panel::~Holdout_Wave_Announce_Panel()
{

}

void Holdout_Wave_Announce_Panel::PerformLayout()
{
	BaseClass::PerformLayout();
}

void Holdout_Wave_Announce_Panel::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	LoadControlSettings( "resource/UI/HoldoutWaveAnnouncePanel.res" );

	GetClientMode()->GetViewportAnimationController()->StartAnimationSequence( this, "WaveAnnounce" );
	GetClientMode()->GetViewportAnimationController()->StartAnimationSequence( this, "GetReady" );
}

void Holdout_Wave_Announce_Panel::Init( int nWave, float flDuration )
{
	m_nWave = nWave;
	m_flClosePanelTime = gpGlobals->curtime + flDuration;

	wchar_t wzValue[5];
	_snwprintf( wzValue, ARRAYSIZE( wzValue ), L"%d", m_nWave + 1 );

	wchar_t wbuffer[ 256 ];		
	g_pVGuiLocalize->ConstructString( wbuffer, sizeof(wbuffer),
		g_pVGuiLocalize->Find("#asw_holdout_wave_announce"), 1,
		wzValue);
	m_pWaveAnnounceLabel->SetText( wbuffer );

	MakeReadyForUse();
	GetClientMode()->GetViewportAnimationController()->StartAnimationSequence( this, "WaveAnnounce" );
	GetClientMode()->GetViewportAnimationController()->StartAnimationSequence( this, "GetReady" );

	// TODO: Add sound playing support to the animation controller
	m_flWaveNumberSoundTime = gpGlobals->curtime + 0.5f; //GetClientMode()->GetViewportAnimationController()->GetAnimationSequenceLength( "WaveAnnounce" );
	m_flGetReadySlideSoundTime = gpGlobals->curtime + 0.4f;
	m_flGetReadySoundTime = gpGlobals->curtime + 0.9f; //GetClientMode()->GetViewportAnimationController()->GetAnimationSequenceLength( "WaveAnnounceGetReady" );
}

void Holdout_Wave_Announce_Panel::OnThink()
{
	BaseClass::OnThink();

#if 0
	if ( m_flWaveNumberSoundTime != 0.0f && gpGlobals->curtime >= m_flWaveNumberSoundTime )
	{
		m_flWaveNumberSoundTime = 0.0f;
		CLocalPlayerFilter filter;
		C_BaseEntity::EmitSound( filter, -1 /*SOUND_FROM_LOCAL_PLAYER*/, "Holdout.GetReadySlam" );
	}
	if ( m_flGetReadySlideSoundTime != 0.0f && gpGlobals->curtime >= m_flGetReadySlideSoundTime )
	{
		m_flGetReadySlideSoundTime = 0.0f;
		CLocalPlayerFilter filter;
		C_BaseEntity::EmitSound( filter, -1 /*SOUND_FROM_LOCAL_PLAYER*/, "Holdout.GetReadySlide" );
	}
	if ( m_flGetReadySoundTime != 0.0f && gpGlobals->curtime >= m_flWaveNumberSoundTime )
	{
		m_flGetReadySoundTime = 0.0f;
		CLocalPlayerFilter filter;
		C_BaseEntity::EmitSound( filter, -1 /*SOUND_FROM_LOCAL_PLAYER*/, "Holdout.GetReadySlam" );
	}
#endif

	if ( gpGlobals->curtime > m_flClosePanelTime )
	{
		MarkForDeletion();
	}
}