//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "vgui_basebudgetpanel.h"
#include "vgui_budgethistorypanel.h"
#include <vgui/ISurface.h>
#include "tier0/vprof.h"
#include "convar.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar budget_show_history( "budget_show_history", "1", FCVAR_ARCHIVE, "turn history graph off and on. . good to turn off on low end" );
ConVar budget_history_numsamplesvisible( "budget_history_numsamplesvisible", "100", FCVAR_ARCHIVE, "number of samples to draw in the budget history window.  The lower the better as far as rendering overhead of the budget panel" );

CBudgetHistoryPanel::CBudgetHistoryPanel( CBaseBudgetPanel *pParent, const char *pPanelName ) : 
	BaseClass( pParent, pPanelName )
{
	m_pBudgetPanel = pParent;

	m_nSamplesPerGroup = 0;
	SetProportional( false );
	SetKeyBoardInputEnabled( false );
	SetMouseInputEnabled( false );
	SetVisible( true );
	SetPaintBackgroundEnabled( false );
	SetBgColor( Color( 0, 0, 0, 255 ) );
	
	// For some reason, vgui::Frame likes to set the minimum size to (128,66) and we may want to get smaller.
	SetMinimumSize( 0, 0 );
}

CBudgetHistoryPanel::~CBudgetHistoryPanel()
{
}

void CBudgetHistoryPanel::Paint()
{
	if( m_nSamplesPerGroup == 0 )
	{
		// SetData hasn't been called yet.
		return;
	}
	if( !budget_show_history.GetBool() )
	{
		return;
	}

	int width, height;
	GetSize( width, height );

	int startID = m_nSampleOffset - width;
	while( startID < 0 )
	{
		startID += m_nSamplesPerGroup;
	}
	int endID = startID + width;
	int numSamplesVisible = budget_history_numsamplesvisible.GetInt(); 
	int xOffset = 0;
	if( endID - startID > numSamplesVisible )
	{
		xOffset = ( endID - numSamplesVisible ) - startID;
		startID = endID - numSamplesVisible;
	}
	static CUtlVector<vgui::IntRect> s_Rects;
	static CUtlVector<float> s_CurrentHeight;
	s_Rects.EnsureCount( ( endID - startID ) );
	s_CurrentHeight.EnsureCount( endID - startID );
	memset( &s_CurrentHeight[0], 0, sizeof( float ) * ( endID - startID ) );
	int j;
	float ooRangeMaxMinusMin = 1.0f / ( m_fRangeMax - m_fRangeMin );
	for( j = 0; j < m_nGroups; j++ )
	{
		int i;
		for( i = startID; i < endID; i++ )
		{
			int sampleOffset = i % m_nSamplesPerGroup;
			int left = i - startID + xOffset;
			int right = left + 1;
			float &curHeight = s_CurrentHeight[i - startID];
			int bottom = ( curHeight - m_fRangeMin ) * ooRangeMaxMinusMin * height;
			curHeight += m_pData[sampleOffset + m_nSamplesPerGroup * j];
			int top = ( curHeight - m_fRangeMin ) * ooRangeMaxMinusMin * height;
			bottom = height - bottom - 1;
			top = height - top - 1;
			vgui::IntRect& rect = s_Rects[( i - startID )];
			rect.x0 = left;
			rect.x1 = right;
			rect.y0 = top;
			rect.y1 = bottom;
		}
		
		int red, green, blue, alpha;
		m_pBudgetPanel->GetConfigData().m_BudgetGroupInfo[j].m_Color.GetColor( red, green, blue, alpha );
		
		vgui::surface()->DrawSetColor( red, green, blue, alpha );
		vgui::surface()->DrawFilledRectArray( &s_Rects[0], endID - startID );
	}

	for ( int i=0; i < m_pBudgetPanel->GetConfigData().m_HistoryLabelValues.Count(); i++ )
	{
		DrawBudgetLine( m_pBudgetPanel->GetConfigData().m_HistoryLabelValues[i] );
	}
}

// Only call this from Paint!!!!!
void CBudgetHistoryPanel::DrawBudgetLine( float val )
{
	int width, height;
	GetSize( width, height );
	double y = ( val - m_fRangeMin ) * ( 1.0f / ( m_fRangeMax - m_fRangeMin ) ) * height;
	int bottom = ( int )( height - y - 1 + .5 );
	int top = ( int )( height - y - 1 - .5 );
	vgui::surface()->DrawSetColor( 0, 0, 0, 255 );
	vgui::surface()->DrawFilledRect( 0, top-1, width, bottom+1 );
	vgui::surface()->DrawSetColor( 255, 255, 255, 255 );
	vgui::surface()->DrawFilledRect( 0, top, width, bottom );
}


void CBudgetHistoryPanel::SetData( double *pData, int nGroups, int nSamplesPerGroup, int nSampleOffset )
{
	m_pData = pData;
	m_nGroups = nGroups;
	m_nSamplesPerGroup = nSamplesPerGroup;
	m_nSampleOffset = nSampleOffset;
}

void CBudgetHistoryPanel::SetRange( float fMin, float fMax )
{
	m_fRangeMin = fMin;
	m_fRangeMax = fMax;
}
