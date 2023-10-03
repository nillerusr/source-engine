#include "cbase.h"
#include "stat_graph.h"
#include <vgui/isurface.h>
#include "vgui_controls/AnimationController.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


StatGraph::StatGraph( vgui::Panel *parent, const char *name ) :
	vgui::Panel( parent, name )
{
	m_pTimeline = NULL;

	m_flCrestValue = 0.0f;
	m_flTroughValue = 0.0f;
	m_flAverageValue = 0.0f;

	m_BarColor = Color(66,142,192,255);	// lightblue

	m_bHighlight = false;

	m_nNumTimelineValues = 50;

	m_nVerticalPixelSeparation = 0;
}

void StatGraph::SetLineColor( Color lineColor )
{
	m_BarColor = lineColor;
}

void StatGraph::SetHighlight( bool bHighlight )
{
	m_bHighlight = bHighlight;
}

void StatGraph::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	SetPaintBackgroundEnabled( true );
	SetPaintBackgroundType( 0 );
}

void StatGraph::SetTimeline( CTimeline *pTimeline )
{
	m_pTimeline = pTimeline;

	CalculateMinMaxValues();
}

void StatGraph::SetMinValue( float flMinValue )
{
	m_fMinValue = flMinValue;
}

void StatGraph::SetMaxValue( float flMaxValue )
{
	m_fMaxValue = flMaxValue;
}

void StatGraph::SetMinMaxValues( float flMinValue, float flMaxValue )
{
	m_fMinValue = flMinValue;
	m_fMaxValue = flMaxValue;
}

float StatGraph::GetFinalValue( void )
{
	if ( !m_pTimeline || m_pTimeline->Count() <= 0 )
		return 0.0f;

	return m_pTimeline->GetValueAtInterp( 1.0f );
}

float StatGraph::GetStartTime( void )
{
	if ( !m_pTimeline || m_pTimeline->Count() <= 0 )
		return 0.0f;

	return m_pTimeline->GetValueTime( 0 );
}

float StatGraph::GetEndTime( void )
{
	if ( !m_pTimeline || m_pTimeline->Count() <= 0 )
		return 0.0f;

	return m_pTimeline->GetValueTime( m_pTimeline->Count() - 1 );
}

void StatGraph::Paint()
{
	BaseClass::Paint();

	if ( !m_pTimeline )
		return;

	if ( m_nNumTimelineValues <= 0 )
		return;

	float fRange = m_fMaxValue - m_fMinValue;

	if ( fRange <= 0.0f )
		return;

	float flWide = static_cast<float>( GetWide() ) / m_nNumTimelineValues;
	float flTall = GetTall() - 1 - m_nVerticalPixelSeparation;
	float flPrevYNormalized = 1.0f - ( ( m_pTimeline->GetValueAtInterp( 0.0f ) - m_fMinValue ) / fRange );

	// This timer is used figure the current position of where the line is taking shape when it first appears
	for ( int i = 0; i < m_nNumTimelineValues; ++i )
	{
		float flXPos = flWide * i;
		float flYNormalized = ( m_pTimeline->GetValueAtInterp( static_cast< float >( i ) / ( m_nNumTimelineValues - 1 ) ) - m_fMinValue ) / fRange;

		// Invert it
		flYNormalized = 1.0f - flYNormalized;

		int nPrevY = flTall * flPrevYNormalized;
		int nCurrentY = flTall * flYNormalized;

		// Force height separation
		if ( m_nVerticalPixelSeparation > 1 )
		{
			nPrevY = ( nPrevY / m_nVerticalPixelSeparation ) * m_nVerticalPixelSeparation;
			nCurrentY = ( nCurrentY / m_nVerticalPixelSeparation ) * m_nVerticalPixelSeparation;
		}

		// Draw the line
		vgui::surface()->DrawSetColor( m_BarColor );
		vgui::surface()->DrawLine( flXPos, flTall * flPrevYNormalized, flXPos + flWide, flTall * flYNormalized );

		flPrevYNormalized = flYNormalized;
	}
}

void StatGraph::CalculateMinMaxValues( void )
{
	if ( !m_pTimeline || m_pTimeline->Count() <= 0 )
		return;

	float fValue = m_pTimeline->GetValue( 0 );

	m_flAverageValue = m_flTroughValue = m_flCrestValue = fValue;

	for ( int i = 1; i < m_pTimeline->Count(); ++i )
	{
		fValue = m_pTimeline->GetValue( i );

		m_flTroughValue = MIN( m_flTroughValue, fValue );
		m_flCrestValue = MAX( m_flCrestValue, fValue );
		m_flAverageValue += fValue;
	}

	m_flAverageValue /= m_pTimeline->Count();

	m_fMinValue = m_flTroughValue;
	m_fMaxValue = m_flCrestValue;
}
