//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "vgui_budgetbargraphpanel.h"
#include "vgui_basebudgetpanel.h"
#include <vgui/ISurface.h>
#include "vgui_controls/Label.h"
#include "convar.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar budget_bargraph_background_alpha( "budget_bargraph_background_alpha", "128", FCVAR_ARCHIVE, "how translucent the budget panel is" );

ConVar budget_peaks_window( "budget_peaks_window", "30", FCVAR_ARCHIVE, "number of frames to look at when figuring out peak frametimes" );
ConVar budget_averages_window( "budget_averages_window", "30", FCVAR_ARCHIVE, "number of frames to look at when figuring out average frametimes" );
ConVar budget_show_peaks( "budget_show_peaks", "1", FCVAR_ARCHIVE, "enable/disable peaks in the budget panel" );
ConVar budget_show_averages( "budget_show_averages", "0", FCVAR_ARCHIVE, "enable/disable averages in the budget panel" );


CBudgetBarGraphPanel::CBudgetBarGraphPanel( CBaseBudgetPanel *pParent, const char *pPanelName ) : 
	BaseClass( pParent, pPanelName )
{
	m_pBudgetPanel = pParent;

	SetProportional( false );
	SetKeyBoardInputEnabled( false );
	SetMouseInputEnabled( false );
	SetVisible( true );

	SetPaintBackgroundEnabled( true );
	SetBgColor( Color( 255, 0, 0, budget_bargraph_background_alpha.GetInt() ) );
}

CBudgetBarGraphPanel::~CBudgetBarGraphPanel()
{
}

void CBudgetBarGraphPanel::GetBudgetGroupTopAndBottom( int id, int &top, int &bottom )
{
	// Ask where the corresponding graph label is.
	m_pBudgetPanel->GetGraphLabelScreenSpaceTopAndBottom( id, top, bottom );
	int tall = bottom - top;

	int x = 0;
	ScreenToLocal( x, top );

	bottom = top + tall;
}

void CBudgetBarGraphPanel::DrawBarAtIndex( int id, float percent )
{
	int panelWidth, panelHeight;
	GetSize( panelWidth, panelHeight );

	int top, bottom;
	GetBudgetGroupTopAndBottom( id, top, bottom );

	int left = 0;
	int right = panelWidth * percent;

	int red, green, blue, alpha;
	m_pBudgetPanel->GetConfigData().m_BudgetGroupInfo[id].m_Color.GetColor( red, green, blue, alpha );
										 
	// DrawFilledRect is panel relative
	vgui::surface()->DrawSetColor( 0, 0, 0, alpha );
	vgui::surface()->DrawFilledRect( left, top, right+2, bottom );

	vgui::surface()->DrawSetColor( 255, 255, 255, alpha );
	vgui::surface()->DrawFilledRect( left, top+1, right+1, bottom-1 );

	vgui::surface()->DrawSetColor( red, green, blue, alpha );
	vgui::surface()->DrawFilledRect( left, top+2, right, bottom-2 );
}

void CBudgetBarGraphPanel::DrawTickAtIndex( int id, float percent, int red, int green, int blue, int alpha )
{
	if( percent > 1.0f )
	{
		percent = 1.0f;
	}
	int panelWidth, panelHeight;
	GetSize( panelWidth, panelHeight );

	int top, bottom;
	GetBudgetGroupTopAndBottom( id, top, bottom );

	int right = ( int )( panelWidth * percent + 1.0f );
	int left = right - 2;

	// DrawFilledRect is panel relative
	vgui::surface()->DrawSetColor( 0, 0, 0, alpha );
	vgui::surface()->DrawFilledRect( left-2, top, right+2, bottom );

	vgui::surface()->DrawSetColor( 255, 255, 255, alpha );
	vgui::surface()->DrawFilledRect( left-1, top+1, right+1, bottom-1 );

	vgui::surface()->DrawSetColor( red, green, blue, alpha );
	vgui::surface()->DrawFilledRect( left, top+2, right, bottom-2 );
}

void CBudgetBarGraphPanel::DrawTimeLines( void )
{
	int panelWidth, panelHeight;
	GetSize( panelWidth, panelHeight );
	int i;
	int left, right, top, bottom;
	top = 0;
	bottom = panelHeight;

	const CBudgetPanelConfigData &config = m_pBudgetPanel->GetConfigData();

	float flValueInterval = config.m_flTimeLabelInterval;
	if ( config.m_nLinesPerTimeLabel != 0.0f )
	{
		flValueInterval = config.m_flTimeLabelInterval / config.m_nLinesPerTimeLabel;
	}
	
	int nTotalLines = config.m_flBarGraphRange;
	if ( flValueInterval != 0.0f )
	{
		nTotalLines /= flValueInterval;
	}
	nTotalLines += 2;
	
	for( i = 0; i < nTotalLines; i++ )
	{
		int alpha;
		if( i % (config.m_nLinesPerTimeLabel*2) == 0 )
		{
			alpha = 150;
		}
		else if( i % config.m_nLinesPerTimeLabel == 0 ) 
		{
			alpha = 100;
		}
		else
		{
			alpha = 50;
		}
		
		float flTemp = ( config.m_flBarGraphRange != 0.0f ) ? ( flValueInterval / config.m_flBarGraphRange ) : flValueInterval;
		left = -0.5f + panelWidth * ( float )( i * flTemp );
		right = left + 1;

		vgui::surface()->DrawSetColor( 0, 0, 0, alpha );
		vgui::surface()->DrawFilledRect( left-1, top, right+1, bottom );

		vgui::surface()->DrawSetColor( 255, 255, 255, alpha );
		vgui::surface()->DrawFilledRect( left, top+1, right, bottom-1 );
	}
}

void CBudgetBarGraphPanel::DrawInstantaneous()
{
	int nGroups, nSamplesPerGroup, nSampleOffset;
	const double *pBudgetGroupTimes = m_pBudgetPanel->GetBudgetGroupData( nGroups, nSamplesPerGroup, nSampleOffset );
	if( !pBudgetGroupTimes )
	{
		return;
	}

	int i;
	for( i = 0; i < nGroups; i++ )
	{
		float percent = m_pBudgetPanel->GetBudgetGroupPercent( pBudgetGroupTimes[nSamplesPerGroup * i + nSampleOffset] );
		DrawBarAtIndex( i, percent );
	}
}

void CBudgetBarGraphPanel::DrawPeaks()
{
	int nGroups, nSamplesPerGroup, nSampleOffset;
	const double *pBudgetGroupTimes = m_pBudgetPanel->GetBudgetGroupData( nGroups, nSamplesPerGroup, nSampleOffset );
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
			int offset = ( nSampleOffset - j + BUDGET_HISTORY_COUNT ) % BUDGET_HISTORY_COUNT;
			tmp = pBudgetGroupTimes[i * nSamplesPerGroup + offset];
			if( tmp > max )
			{
				max = tmp;
			}
		}
		float percent = m_pBudgetPanel->GetBudgetGroupPercent( max );
		DrawTickAtIndex( i, percent, 255, 0, 0, 255 );
	}
}

void CBudgetBarGraphPanel::DrawAverages()
{
	int nGroups, nSamplesPerGroup, nSampleOffset;
	const double *pBudgetGroupTimes = m_pBudgetPanel->GetBudgetGroupData( nGroups, nSamplesPerGroup, nSampleOffset );
	if( !pBudgetGroupTimes )
	{
		return;
	}
	int numSamples = budget_averages_window.GetInt();
	int i;
	for( i = 0; i < nGroups; i++ )
	{
		int red, green, blue, alpha;
		m_pBudgetPanel->GetConfigData().m_BudgetGroupInfo[i].m_Color.GetColor( red, green, blue, alpha );

		double sum = 0;
		int j;
		for( j = 0; j < numSamples; j++ )
		{
			int offset = ( nSampleOffset - j + BUDGET_HISTORY_COUNT ) % BUDGET_HISTORY_COUNT;
			sum += pBudgetGroupTimes[i * nSamplesPerGroup + offset];
		}
		sum *= ( 1.0f / numSamples );
		float percent = m_pBudgetPanel->GetBudgetGroupPercent( sum );
		DrawTickAtIndex( i, percent, red, green, blue, alpha );
	}
}

void CBudgetBarGraphPanel::Paint( void )
{
	int width, height;
	GetSize( width, height );

	if ( !m_pBudgetPanel->IsDedicated() )
	{
		SetBgColor( Color( 255, 0, 0, budget_bargraph_background_alpha.GetInt() ) );
	}

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
}

