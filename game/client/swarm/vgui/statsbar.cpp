#include "cbase.h"
#include <vgui_controls/ImagePanel.h>
#include "vgui_controls/AnimationController.h"
#include <vgui_controls/ImagePanel.h>
#include "soundenvelope.h"
#include "BriefingTooltip.h"
#include "StatsBar.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

StatsBar::StatsBar(vgui::Panel *parent, const char *name) :
	vgui::Panel(parent, name)
{
	m_pUnusedBar = new vgui::Panel( this, "UnusedBarPanel" );
	m_pUnusedBar->SetVisible( true );
	m_pBar = new vgui::Panel(this, "StatsBarPanel");
	m_pBar->SetVisible(false);
	m_pIncreaseBar = new vgui::Panel( this, "IncreaseBarPanel" );
	m_pIncreaseBar->SetVisible( false );	

	m_pLabel = new vgui::Label(this, "StatsBarLabel", "");
	m_pLabel->SetContentAlignment(vgui::Label::a_center);
	m_pLabel->SetVisible(false);	

	m_fStart = 0;
	m_fTarget = 0;
	m_fCurrent = 0;
	m_flBorder = 2.0f;
	m_fIncreaseRate = 100.0f;
	m_bDisplayInteger = true;
	m_bDisplayPercentage = false;
	m_bDisplayTime = false;
	m_bShowCumulativeTotal = false;
	m_flNextUpdateTime = 0;
	m_flUpdateInterval = 0.05f;
	
	m_bInit = false;
	m_fStartTime = 0;
	m_pLoopingSound = NULL;
	m_hExternalCounter = NULL;
	m_bShowMaxOnCounter = false;

	m_TextColor = Color(255,255,255,255);	// white
	m_BarColor = Color(66,142,192,255);	// lightblue
	m_IncreaseBarColor = Color(66,142,192,255);	// white
	m_BackgroundColor = Color(65,74,96,255);	// greyblue
	m_UnusedColor = Color(65,74,96,255);
}

void StatsBar::SetColors(Color text, Color bar, Color increasebar, Color unusedbar, Color background)
{
	m_TextColor = text;
	m_BarColor = bar;
	m_IncreaseBarColor = increasebar;
	m_BackgroundColor = background;
	m_UnusedColor = unusedbar;

	m_pLabel->SetFgColor( m_TextColor );
	m_pBar->SetBgColor( m_BarColor );
	m_pUnusedBar->SetBgColor( unusedbar );
	m_pIncreaseBar->SetBgColor( m_IncreaseBarColor );
	SetBgColor( m_BackgroundColor );

	m_flNextUpdateTime = 0;
// 	if ( m_hExternalCounter.Get() )
// 	{
// 		m_hExternalCounter->SetFgColor( m_TextColor );
// 	}
}

void StatsBar::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	if ( m_hExternalCounter.Get() )
	{
// 		m_hExternalCounter->SetFgColor(m_TextColor);
// 		m_hExternalCounter->SetFont(pScheme->GetFont("Default"));
	}
	else
	{
		m_pLabel->SetFgColor(m_TextColor);
		m_pLabel->SetFont( pScheme->GetFont( "Default", IsProportional() ) );
	}

	m_pUnusedBar->SetPaintBackgroundEnabled( true );
	m_pUnusedBar->SetPaintBackgroundType( 0 );
	m_pUnusedBar->SetBgColor( m_UnusedColor );

	m_pBar->SetPaintBackgroundEnabled( true );
	m_pBar->SetPaintBackgroundType( 0 );
	m_pBar->SetBgColor( m_BarColor );

	m_pIncreaseBar->SetPaintBackgroundEnabled( true );
	m_pIncreaseBar->SetPaintBackgroundType( 0 );
	m_pIncreaseBar->SetBgColor( m_IncreaseBarColor );

	SetPaintBackgroundEnabled(true);
	SetPaintBackgroundType(0);
	SetBgColor(m_BackgroundColor);
	m_flNextUpdateTime = 0;
}

void StatsBar::PerformLayout()
{
	BaseClass::PerformLayout();
	
	int w, t;
	GetSize(w, t);

	m_pLabel->SetBounds(0, 0, w, t);
}

void StatsBar::OnThink()
{
	BaseClass::OnThink();

	if ( m_flNextUpdateTime > gpGlobals->curtime )
		return;

	m_flNextUpdateTime = gpGlobals->curtime + m_flUpdateInterval;

	if (m_fStartTime > 0 && gpGlobals->curtime > m_fStartTime && m_bInit)
	{		
		float passed = gpGlobals->curtime - m_fStartTime;
		float increase = m_fStart + passed * m_fIncreaseRate;
		//if (m_bDisplayInteger)
		//increase = int(increase);

		m_fCurrent = MIN(increase, m_fTarget);
	}

	float fScale = (ScreenWidth() / 1024.0f);
	int w, t, padding;
	GetSize(w, t);
	padding = m_flBorder * fScale;

	float flBarMin = GetBarMin();
	float flBarMax = GetBarMax();
	float initial_bar_fraction = ( m_fStart - flBarMin ) / ( flBarMax - flBarMin );
	initial_bar_fraction = MAX( initial_bar_fraction, 0 );

	m_pUnusedBar->SetBounds( padding, padding, w - padding * 2, t - padding * 2 );

	if ( initial_bar_fraction > 0 )
	{
		m_pBar->SetVisible(true);
		m_pBar->SetBounds(padding, padding, (w - padding * 2) * initial_bar_fraction, (t - padding * 2));
	}
	else
	{
		m_pBar->SetVisible(false);
	}

	if (m_fStartTime > 0 && gpGlobals->curtime > m_fStartTime && m_bInit)
	{
		// update bar and label
		float bar_fraction = ( m_fCurrent - MAX( m_fStart, flBarMin ) ) / ( flBarMax - flBarMin );
		bar_fraction = MAX( bar_fraction, 0 );		

		if ( bar_fraction > 0 )
		{
			m_pIncreaseBar->SetVisible(true);
			m_pIncreaseBar->SetBounds( padding + (w - padding * 2) * initial_bar_fraction, padding, (w - padding * 2) * bar_fraction, (t - padding * 2));
		}
		else
		{
			m_pIncreaseBar->SetVisible(false);
		}

		bool bFinished = (m_fCurrent == m_fTarget);
		if (m_fTarget > 0 && m_fCurrent > 0)
		{
			if (m_pLoopingSound == NULL && !bFinished)
			{
				CLocalPlayerFilter filter;
				m_pLoopingSound = CSoundEnvelopeController::GetController().SoundCreate( filter, 0, "ASWInterface.AreaBracketsLoop" );
				CSoundEnvelopeController::GetController().Play( m_pLoopingSound, 1.0, 100 );
			}
			else if (m_pLoopingSound && bFinished)
			{			
				CSoundEnvelopeController::GetController().SoundDestroy( m_pLoopingSound );
				m_pLoopingSound = NULL;
				CLocalPlayerFilter filter;
				C_BaseEntity::EmitSound( filter, -1 /*SOUND_FROM_LOCAL_PLAYER*/, "ASWInterface.Teletype3" );
			}
		}
	}

	// update label text
	int current_number = (int) m_fCurrent;
	int max_number = (int) flBarMax;

	if ( !m_bShowCumulativeTotal )
	{
		current_number = (int) ( m_fCurrent - flBarMin );
		max_number = (int) ( flBarMax - flBarMin );
	}

	char maxbuffer[64] = "";
	if ( m_bShowMaxOnCounter )
	{
		Q_snprintf( maxbuffer, sizeof( maxbuffer ), " / %d", max_number );		// TODO: Support float/time/percentage displays for max?
	}

	char buffer[64];
	if (m_bDisplayTime)
	{
		int minutes = (current_number) / 60;
		int remainder = (current_number) % 60;
		Q_snprintf(buffer, sizeof(buffer), "%.2d:%.2d%s", minutes, remainder, maxbuffer);
	}
	else if (m_bDisplayInteger)
	{
		if (m_bDisplayPercentage)
			Q_snprintf(buffer, sizeof(buffer), "%d%%%s", current_number, maxbuffer);
		else
			Q_snprintf(buffer, sizeof(buffer), "%d%s", current_number, maxbuffer);
	}
	else
	{
		if (m_bDisplayPercentage)
			Q_snprintf(buffer, sizeof(buffer), "%f%%%s", current_number, maxbuffer);
		else
			Q_snprintf(buffer, sizeof(buffer), "%f%s", current_number, maxbuffer);
	}
	if ( m_hExternalCounter.Get() )
	{
		m_hExternalCounter->SetText( buffer );
		m_pLabel->SetVisible( false );
	}
	else
	{
		m_pLabel->SetVisible( true );
		m_pLabel->SetBounds( 0, 0, w, t );
		m_pLabel->SetText( buffer );
	}
}

void StatsBar::Init(float fCurrent, float fTarget, float fIncreaseRate, bool bDisplayInteger, bool bDisplayPercentage)
{
	m_fStart = fCurrent;
	m_fCurrent = fCurrent;
	m_fTarget = fTarget;
	m_fIncreaseRate = fIncreaseRate;
	m_bDisplayInteger = bDisplayInteger;
	m_bDisplayPercentage = bDisplayPercentage;
	m_bInit = true;
	m_flNextUpdateTime = 0;
	InvalidateLayout(false, true);
	//Msg("StatsBar::Init cur=%f target=%f rate=%f max=%f int=%d perc=%d\n", fCurrent, fTarget, fIncreaseRate, fBarMax, bDisplayInteger, bDisplayPercentage);
}

void StatsBar::UseExternalCounter( vgui::Label *pCounter )
{
	m_hExternalCounter = pCounter;
}

void StatsBar::AddMinMax( float flBarMin, float flBarMax )
{
	StatsBarMinMax bounds;
	bounds.flMin = flBarMin;
	bounds.flMax = flBarMax;
	m_Bounds.AddToTail( bounds );
	m_flNextUpdateTime = 0;
}

void StatsBar::ClearMinMax()
{
	m_Bounds.Purge();
	m_flNextUpdateTime = 0;
}

float StatsBar::GetBarMin()
{
	for ( int i = 0; i < m_Bounds.Count(); i++ )
	{
		if ( ( m_fCurrent >= m_Bounds[ i ].flMin && m_fCurrent < m_Bounds[ i ].flMax ) || ( i == m_Bounds.Count() - 1 ) )
			return m_Bounds[ i ].flMin;
	}
	return 0.0f;
}

float StatsBar::GetBarMax()
{
	for ( int i = 0; i < m_Bounds.Count(); i++ )
	{
		if ( ( m_fCurrent >= m_Bounds[ i ].flMin && m_fCurrent < m_Bounds[ i ].flMax ) || ( i == m_Bounds.Count() - 1 ) )
			return m_Bounds[ i ].flMax;
	}
	return 100.0f;
}