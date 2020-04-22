//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "vgui_BudgetFPSPanel.H"
#include "vgui_BudgetPanel.h"
#include <vgui/ISurface.h>
#include "tier0/vprof.h"
#include "materialsystem/imaterialsystem.h"

static ConVar budget_peaks_window( "budget_peaks_window", "30", FCVAR_ARCHIVE, "number of frames to look at when figuring out peak frametimes" );
static ConVar budget_averages_window( "budget_averages_window", "30", FCVAR_ARCHIVE, "number of frames to look at when figuring out average frametimes" );
static ConVar budget_show_peaks( "budget_show_peaks", "1", FCVAR_ARCHIVE, "enable/disable peaks in the budget panel" );
static ConVar budget_show_averages( "budget_show_averages", "0", FCVAR_ARCHIVE, "enable/disable averages in the budget panel" );

CBudgetFPSPanel::CBudgetFPSPanel( vgui::Panel *pParent, const char *pPanelName ) : BaseClass( pParent, pPanelName )
{
	SetProportional( false );
	SetKeyBoardInputEnabled( false );
	SetMouseInputEnabled( false );
	SetSizeable( false ); 
	SetVisible( true );
	SetPaintBackgroundEnabled( false );
	
	// hide the system buttons
	SetTitleBarVisible( false );
}

CBudgetFPSPanel::~CBudgetFPSPanel()
{
}

#define PIXELS_BETWEEN_BARS 4

void CBudgetFPSPanel::DrawBarAtIndex( int id, float percent )
{
	int panelWidth, panelHeight;
	GetSize( panelWidth, panelHeight );

	int nGroups = g_VProfCurrentProfile.GetNumBudgetGroups();
	int top = 2 + ( id * panelHeight ) / nGroups;
	int bottom = ( ( ( id + 1 ) * panelHeight ) / nGroups ) - PIXELS_BETWEEN_BARS;
	int left = 0;
	int right = panelWidth * percent;

	int red, green, blue, alpha;
	g_VProfCurrentProfile.GetBudgetGroupColor( id, red, green, blue, alpha );
										 
	// DrawFilledRect is panel relative
	vgui::surface()->DrawSetColor( 0, 0, 0, alpha );
	vgui::surface()->DrawFilledRect( left, top-2, right+2, bottom+2 );

	vgui::surface()->DrawSetColor( 255, 255, 255, alpha );
	vgui::surface()->DrawFilledRect( left, top-1, right+1, bottom+1 );

	vgui::surface()->DrawSetColor( red, green, blue, alpha );
	vgui::surface()->DrawFilledRect( left, top, right, bottom );
}

void CBudgetFPSPanel::DrawTickAtIndex( int id, float percent, int red, int green, int blue, int alpha )
{
	if( percent > 1.0f )
	{
		percent = 1.0f;
	}
	int panelWidth, panelHeight;
	GetSize( panelWidth, panelHeight );

	int nGroups = g_VProfCurrentProfile.GetNumBudgetGroups();
	int top = 2 + ( id * panelHeight ) / nGroups;
	int bottom = ( ( ( id + 1 ) * panelHeight ) / nGroups ) - PIXELS_BETWEEN_BARS;
	int right = ( int )( panelWidth * percent + 1.0f );
	int left = right - 2;

	// DrawFilledRect is panel relative
	vgui::surface()->DrawSetColor( 0, 0, 0, alpha );
	vgui::surface()->DrawFilledRect( left-2, top-2, right+2, bottom+2 );

	vgui::surface()->DrawSetColor( 255, 255, 255, alpha );
	vgui::surface()->DrawFilledRect( left-1, top-1, right+1, bottom+1 );

	vgui::surface()->DrawSetColor( red, green, blue, alpha );
	vgui::surface()->DrawFilledRect( left, top, right, bottom );
}

void CBudgetFPSPanel::DrawTimeLines( void )
{
	int panelWidth, panelHeight;
	GetSize( panelWidth, panelHeight );
	int i;
	int left, right, top, bottom;
	top = 0;
	bottom = panelHeight;
	for( i = ( int )m_fRangeMin; i < ( int )( m_fRangeMax + 0.5f ); i++ )
	{
		int alpha;
		if( i % 10 == 0 )
		{
			alpha = 150;
		}
		else if( i % 5 == 0 ) 
		{
			alpha = 100;
		}
		else
		{
			alpha = 50;
		}
		
		left = -0.5f + panelWidth * ( ( float )i - m_fRangeMin ) / ( m_fRangeMax - m_fRangeMin );
		right = left + 1;

		vgui::surface()->DrawSetColor( 0, 0, 0, alpha );
		vgui::surface()->DrawFilledRect( left-1, top, right+1, bottom );

		vgui::surface()->DrawSetColor( 255, 255, 255, alpha );
		vgui::surface()->DrawFilledRect( left, top+1, right, bottom-1 );
	}
}

void CBudgetFPSPanel::DrawInstantaneous()
{
	int nGroups, nSamplesPerGroup, nSampleOffset;
	const double *pBudgetGroupTimes = GetBudgetPanel()->GetBudgetGroupData( nGroups, nSamplesPerGroup, nSampleOffset );
	if( !pBudgetGroupTimes )
	{
		return;
	}

	int i;
	for( i = 0; i < nGroups; i++ )
	{
		DrawBarAtIndex( i, ( pBudgetGroupTimes[nSamplesPerGroup * i + nSampleOffset] - m_fRangeMin ) / ( m_fRangeMax - m_fRangeMin ) );
	}
}

void CBudgetFPSPanel::DrawPeaks()
{
	int nGroups, nSamplesPerGroup, nSampleOffset;
	const double *pBudgetGroupTimes = GetBudgetPanel()->GetBudgetGroupData( nGroups, nSamplesPerGroup, nSampleOffset );
	if( !pBudgetGroupTimes )
	{
		return;
	}
	int numSamples = budget_peaks_window.GetInt();
	int i;
	for( i = 0; i < nGroups; i++ )
	{
		double max = 0;
		int j;
		for( j = 0; j < numSamples; j++ )
		{
			double tmp;
			int offset = ( nSampleOffset - j + VPROF_HISTORY_COUNT ) % VPROF_HISTORY_COUNT;
			tmp = pBudgetGroupTimes[i * nSamplesPerGroup + offset];
			if( tmp > max )
			{
				max = tmp;
			}
		}
		float percent = ( max - m_fRangeMin ) / ( m_fRangeMax - m_fRangeMin );
		DrawTickAtIndex( i, percent, 255, 0, 0, 255 );
	}
}

void CBudgetFPSPanel::DrawAverages()
{
	int nGroups, nSamplesPerGroup, nSampleOffset;
	const double *pBudgetGroupTimes = GetBudgetPanel()->GetBudgetGroupData( nGroups, nSamplesPerGroup, nSampleOffset );
	if( !pBudgetGroupTimes )
	{
		return;
	}
	int numSamples = budget_averages_window.GetInt();
	int i;
	for( i = 0; i < nGroups; i++ )
	{
		int red, green, blue, alpha;
		g_VProfCurrentProfile.GetBudgetGroupColor( i, red, green, blue, alpha );
		double sum = 0;
		int j;
		for( j = 0; j < numSamples; j++ )
		{
			int offset = ( nSampleOffset - j + VPROF_HISTORY_COUNT ) % VPROF_HISTORY_COUNT;
			sum += pBudgetGroupTimes[i * nSamplesPerGroup + offset];
		}
		sum *= ( 1.0f / numSamples );
		float percent = ( sum - m_fRangeMin ) / ( m_fRangeMax - m_fRangeMin );
		DrawTickAtIndex( i, percent, red, green, blue, alpha );
	}
}

void CBudgetFPSPanel::Paint( void )
{
	materials->Flush();
	g_VProfCurrentProfile.Pause();
	int width, height;
	GetSize( width, height );

	DrawTimeLines();
	DrawInstantaneous();
	if( budget_show_peaks.GetBool() )
	{
		DrawPeaks();
	}
	if( budget_show_averages.GetBool() )
	{
		DrawAverages();
	}
	materials->Flush();
	g_VProfCurrentProfile.Resume();
}


void CBudgetFPSPanel::SetRange( float fMin, float fMax )
{
	m_fRangeMin = fMin;
	m_fRangeMax = fMax;
}
