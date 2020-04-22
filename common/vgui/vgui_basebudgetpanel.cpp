//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "vgui_basebudgetpanel.h"
#include "vgui_controls/Label.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CBaseBudgetPanel::CBaseBudgetPanel( vgui::Panel *pParent, const char *pElementName )
	 :	vgui::Panel( pParent, pElementName )
{
	m_BudgetHistoryOffset = 0;

	SetProportional( false );
	SetKeyBoardInputEnabled( false );
	SetMouseInputEnabled( false );
	SetVisible( true );

	// set the scheme before any child control is created
	vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFile("resource/SourceScheme.res", "Client");
	SetScheme( scheme );

	m_pBudgetHistoryPanel = NULL;
	m_pBudgetBarGraphPanel = NULL;
	SetZPos( 1001 );

	m_bDedicated = false;
}


CBaseBudgetPanel::~CBaseBudgetPanel()
{
}


float CBaseBudgetPanel::GetBudgetGroupPercent( float value )
{
	if ( m_ConfigData.m_flBarGraphRange == 0.0f )
		return 1.0f;
	return value / m_ConfigData.m_flBarGraphRange;
}


const double *CBaseBudgetPanel::GetBudgetGroupData( int &nGroups, int &nSamplesPerGroup, int &nSampleOffset ) const
{
	nGroups = m_ConfigData.m_BudgetGroupInfo.Count();
	nSamplesPerGroup = BUDGET_HISTORY_COUNT;
	nSampleOffset = m_BudgetHistoryOffset;
	if( m_BudgetGroupTimes.Count() == 0 )
	{
		return NULL;
	}
	else
	{
		return &m_BudgetGroupTimes[0].m_Time[0];
	}
}


void CBaseBudgetPanel::ClearTimesForAllGroupsForThisFrame( void )
{
	int i;
	for( i = 0; i < m_ConfigData.m_BudgetGroupInfo.Count(); i++ )
	{
		m_BudgetGroupTimes[i].m_Time[m_BudgetHistoryOffset] = 0.0;
	}
}

void CBaseBudgetPanel::ClearAllTimesForGroup( int groupID )
{
	int i;
	for( i = 0; i < BUDGET_HISTORY_COUNT; i++ )
	{
		m_BudgetGroupTimes[groupID].m_Time[i] = 0.0;
	}
}


void CBaseBudgetPanel::OnConfigDataChanged( const CBudgetPanelConfigData &data )
{
	int oldNumGroups = m_ConfigData.m_BudgetGroupInfo.Count();

	// Copy in the config data and rebuild everything.
	Rebuild( data );
	
	if ( m_ConfigData.m_BudgetGroupInfo.Count() > m_BudgetGroupTimes.Count() )
	{
		m_BudgetGroupTimes.EnsureCount( m_ConfigData.m_BudgetGroupInfo.Count() );
		for ( int i = oldNumGroups; i < m_ConfigData.m_BudgetGroupInfo.Count(); i++ )
		{
			ClearAllTimesForGroup( i );
		}
	}
	else
	{
		m_BudgetGroupTimes.SetSize( m_ConfigData.m_BudgetGroupInfo.Count() );
		for ( int i = 0; i < m_BudgetGroupTimes.Count(); i++ )
		{
			ClearAllTimesForGroup( i );
		}
	}

	InvalidateLayout( false, true );
}


void CBaseBudgetPanel::ResetAll()
{
	m_ConfigData.m_BudgetGroupInfo.Purge();
	
	for ( int i=0; i < m_GraphLabels.Count(); i++ )
		m_GraphLabels[i]->MarkForDeletion();
	m_GraphLabels.Purge();

	for ( int i=0; i < m_TimeLabels.Count(); i++ )
		m_TimeLabels[i]->MarkForDeletion();
	m_TimeLabels.Purge();
}


void CBaseBudgetPanel::Rebuild( const CBudgetPanelConfigData &data )
{
	int oldNumBudgetGroups = m_ConfigData.m_BudgetGroupInfo.Count();
	int oldNumHistoryLabels = m_ConfigData.m_HistoryLabelValues.Count();
	
	int oldNumTimeLabels = m_TimeLabels.Count();

	// Copy the new config in.
	m_ConfigData = data;

	int nParentWidth, nParentHeight;
	GetParent()->GetSize( nParentWidth, nParentHeight );
	if ( m_ConfigData.m_Width > nParentWidth )
	{
		m_ConfigData.m_Width = nParentWidth;
	}
	if ( m_ConfigData.m_Height > nParentHeight )
	{
		m_ConfigData.m_Height = nParentHeight;
	}
	if ( m_ConfigData.m_xCoord + m_ConfigData.m_Width > nParentWidth )
	{
		m_ConfigData.m_xCoord = nParentWidth - m_ConfigData.m_Width;
	}
	if ( m_ConfigData.m_yCoord + m_ConfigData.m_Height > nParentHeight )
	{
		m_ConfigData.m_yCoord = nParentHeight - m_ConfigData.m_Height;
	}

	// Recreate the history and bar graph panels.
	if( m_pBudgetHistoryPanel )
	{
		m_pBudgetHistoryPanel->MarkForDeletion();
	}
	m_pBudgetHistoryPanel = new CBudgetHistoryPanel( this, "FrametimeHistory" );

	if( m_pBudgetBarGraphPanel )
	{
		m_pBudgetBarGraphPanel->MarkForDeletion();
	}
	m_pBudgetBarGraphPanel = new CBudgetBarGraphPanel( this, "BudgetBarGraph" );

	// Create any new labels we need.
	int i;

	if ( m_ConfigData.m_BudgetGroupInfo.Count() > m_GraphLabels.Count() )
	{
		m_GraphLabels.EnsureCount( m_ConfigData.m_BudgetGroupInfo.Count() );
		for( i = oldNumBudgetGroups; i < m_ConfigData.m_BudgetGroupInfo.Count(); i++ )
		{
			const char *pBudgetGroupName = m_ConfigData.m_BudgetGroupInfo[i].m_Name.String();
			m_GraphLabels[i] = new vgui::Label( this, pBudgetGroupName, pBudgetGroupName );
		}
	}
	else
	{
		while ( m_GraphLabels.Count() > m_ConfigData.m_BudgetGroupInfo.Count() )
		{
			m_GraphLabels[m_GraphLabels.Count()-1]->MarkForDeletion();
			m_GraphLabels.Remove( m_GraphLabels.Count()-1 );
		}
	}
	Assert( m_GraphLabels.Count() == m_ConfigData.m_BudgetGroupInfo.Count() );


	// Create new history labels.
	if ( m_ConfigData.m_HistoryLabelValues.Count() > m_HistoryLabels.Count() )
	{
		m_HistoryLabels.EnsureCount( m_ConfigData.m_HistoryLabelValues.Count() );
		for ( i=oldNumHistoryLabels; i < m_HistoryLabels.Count(); i++ )
		{
			m_HistoryLabels[i] = new vgui::Label( this, "history label", "history label" );
		}
	}
	else
	{
		while ( m_HistoryLabels.Count() > m_ConfigData.m_HistoryLabelValues.Count() )
		{
			m_HistoryLabels[m_HistoryLabels.Count()-1]->MarkForDeletion();
			m_HistoryLabels.Remove( m_HistoryLabels.Count()-1 );
		}
	}
	SetHistoryLabelText();

	
	// Note: the time lines still use milliseconds for the computations about where to draw them,
	// but each BudgetGroupDataType_t has its own scale.
	int nTimeLabels = m_ConfigData.m_flBarGraphRange + data.m_flTimeLabelInterval;
	if ( data.m_flTimeLabelInterval != 0.0f )
	{
		nTimeLabels /= data.m_flTimeLabelInterval;
	}

	if ( nTimeLabels > m_TimeLabels.Count() )
	{
		m_TimeLabels.EnsureCount( nTimeLabels );
		for( i = oldNumTimeLabels; i < m_TimeLabels.Count(); i++ )
		{
			char name[1024];
			Q_snprintf( name, sizeof( name ), "time_label_%d", i );
			m_TimeLabels[i] = new vgui::Label( this, name, "TEXT NOT SET YET" );
		}
	}
	else
	{
		while ( m_TimeLabels.Count() > nTimeLabels )
		{
			m_TimeLabels[m_TimeLabels.Count()-1]->MarkForDeletion();
			m_TimeLabels.Remove( m_TimeLabels.Count()-1 );
		}
	}

	SetTimeLabelText();
}

void CBaseBudgetPanel::UpdateWindowGeometry()
{
	if( m_ConfigData.m_Width > BUDGET_HISTORY_COUNT )
	{
		m_ConfigData.m_Width = BUDGET_HISTORY_COUNT;
	}

	SetPos( m_ConfigData.m_xCoord, m_ConfigData.m_yCoord );
	SetSize( m_ConfigData.m_Width, m_ConfigData.m_Height );
}

void CBaseBudgetPanel::PerformLayout()
{
	if ( !m_pBudgetHistoryPanel || !m_pBudgetBarGraphPanel )
		return;


	int maxFPSLabelWidth = 0;
	int i;
	for( i = 0; i < m_HistoryLabels.Count(); i++ )
	{
		int labelWidth, labelHeight;
		m_HistoryLabels[i]->GetContentSize( labelWidth, labelHeight );
		if( labelWidth > maxFPSLabelWidth )
		{
			maxFPSLabelWidth = labelWidth;
		}
	}

	m_pBudgetHistoryPanel->SetRange( 0, m_ConfigData.m_flHistoryRange );

	
	float bottomOfHistoryPercentage = m_ConfigData.m_flBottomOfHistoryFraction;
	UpdateWindowGeometry();
	int x, y, totalWidth, totalHeight;
	int totalHeightMinusTimeLabels;
	GetPos( x, y );
	GetSize( totalWidth, totalHeight );

	int maxTimeLabelHeight = 0;
	for( i = 0; i < m_TimeLabels.Count(); i++ )
	{
		int labelWidth, labelHeight;
		m_TimeLabels[i]->GetContentSize( labelWidth, labelHeight );
		maxTimeLabelHeight = max( maxTimeLabelHeight, labelHeight );
	}

	totalHeightMinusTimeLabels = totalHeight - maxTimeLabelHeight;
	
	m_pBudgetHistoryPanel->SetPos( 0, 0 );
	int budgetHistoryHeight = totalHeightMinusTimeLabels * bottomOfHistoryPercentage;
	m_pBudgetHistoryPanel->SetSize( totalWidth - maxFPSLabelWidth, 
		budgetHistoryHeight );

	int maxLabelWidth = 0;
	for( i = 0; i < m_GraphLabels.Count(); i++ )
	{
		int width, height;
		m_GraphLabels[i]->GetContentSize( width, height );
		if( maxLabelWidth < width )
		{
			maxLabelWidth = width;
		}
	}
	
	m_pBudgetBarGraphPanel->SetPos( maxLabelWidth, 
		totalHeightMinusTimeLabels * bottomOfHistoryPercentage );
	m_pBudgetBarGraphPanel->SetSize( totalWidth - maxLabelWidth, 
		totalHeightMinusTimeLabels * ( 1 - bottomOfHistoryPercentage ) );

	for( i = 0; i < m_GraphLabels.Count(); i++ )
	{
		m_GraphLabels[i]->SetPos( 0, 
			( bottomOfHistoryPercentage * totalHeightMinusTimeLabels ) +
			( i * totalHeightMinusTimeLabels * 
			( 1 - bottomOfHistoryPercentage ) ) / m_ConfigData.m_BudgetGroupInfo.Count() );
		// fudge height by 1 for rounding 
		m_GraphLabels[i]->SetSize( maxLabelWidth, 1 + ( totalHeightMinusTimeLabels * 
			( 1 - bottomOfHistoryPercentage ) ) / m_ConfigData.m_BudgetGroupInfo.Count() );
		m_GraphLabels[i]->SetContentAlignment( vgui::Label::a_east );
	}

	// Note: the time lines still use milliseconds for the computations about where to draw them,
	// but each BudgetGroupDataType_t has its own scale.
	float fRange = m_ConfigData.m_flBarGraphRange;
	for( i = 0; i < m_TimeLabels.Count(); i++ )
	{
		int labelWidth, labelHeight;
		m_TimeLabels[i]->GetContentSize( labelWidth, labelHeight );
		x = maxLabelWidth + ( i * m_ConfigData.m_flTimeLabelInterval ) / fRange * ( totalWidth - maxLabelWidth );
		
		m_TimeLabels[i]->SetPos( x - ( labelWidth * 0.5 ), totalHeight - labelHeight );
		m_TimeLabels[i]->SetSize( labelWidth, labelHeight );
		m_TimeLabels[i]->SetContentAlignment( vgui::Label::a_east );
	}


	// position the fps labels
	fRange = m_ConfigData.m_flHistoryRange;
	for( i = 0; i < m_HistoryLabels.Count(); i++ )
	{
		int labelWidth, labelHeight;
		m_HistoryLabels[i]->GetContentSize( labelWidth, labelHeight );
		y = (fRange != 0) ? budgetHistoryHeight * m_ConfigData.m_HistoryLabelValues[i] / ( float )fRange : 0.0f;
		int top = ( int )( budgetHistoryHeight - y - 1 - labelHeight * 0.5f );
		m_HistoryLabels[i]->SetPos( totalWidth - maxFPSLabelWidth, top );
		m_HistoryLabels[i]->SetSize( labelWidth, labelHeight );
		m_HistoryLabels[i]->SetContentAlignment( vgui::Label::a_east );
	}
}

void CBaseBudgetPanel::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	int i;
	for( i = 0; i < m_ConfigData.m_BudgetGroupInfo.Count(); i++ )
	{
		m_GraphLabels[i]->SetFgColor( m_ConfigData.m_BudgetGroupInfo[i].m_Color );
		m_GraphLabels[i]->SetBgColor( Color( 0, 0, 0, 255 ) );
		m_GraphLabels[i]->SetPaintBackgroundEnabled( false );
		m_GraphLabels[i]->SetFont( pScheme->GetFont( "BudgetLabel", IsProportional() ) );
		if ( m_bDedicated )
		{
			m_GraphLabels[i]->SetBgColor( pScheme->GetColor( "ControlBG", Color( 0, 0, 0, 255 ) ) );
		}
	}

	for( i = 0; i < m_TimeLabels.Count(); i++ )
	{
		int red, green, blue, alpha;
		red = green = blue = alpha = 255;
		m_TimeLabels[i]->SetFgColor( Color( red, green, blue, alpha ) );
		m_TimeLabels[i]->SetBgColor( Color( 0, 0, 0, 255 ) );
		m_TimeLabels[i]->SetPaintBackgroundEnabled( false );
		m_TimeLabels[i]->SetFont( pScheme->GetFont( "BudgetLabel", IsProportional() ) );
		if ( m_bDedicated )
		{
			m_TimeLabels[i]->SetBgColor( pScheme->GetColor( "ControlBG", Color( 0, 0, 0, 255 ) ) );
		}
	}

	for( i = 0; i < m_HistoryLabels.Count(); i++ )
	{
		int red, green, blue, alpha;
		red = green = blue = alpha = 255;
		m_HistoryLabels[i]->SetFgColor( Color( red, green, blue, alpha ) );
		m_HistoryLabels[i]->SetBgColor( Color( 0, 0, 0, 255 ) );
		m_HistoryLabels[i]->SetPaintBackgroundEnabled( false );
		m_HistoryLabels[i]->SetFont( pScheme->GetFont( "BudgetLabel", IsProportional() ) );
		if ( m_bDedicated )
		{
			m_HistoryLabels[i]->SetBgColor( pScheme->GetColor( "ControlBG", Color( 0, 0, 0, 255 ) ) );
		}
	}

	m_hFont = pScheme->GetFont( "DefaultFixed" );

	if ( m_bDedicated )
	{
		SetBgColor( pScheme->GetColor( "ControlBG", Color( 0, 0, 0, 255 ) ) );
	}
	SetPaintBackgroundEnabled( true );
}

void CBaseBudgetPanel::PaintBackground()
{
	if ( m_bDedicated )
	{
		m_pBudgetBarGraphPanel->SetBgColor( GetBgColor() );
	}
	else
	{
		SetBgColor( Color( 0, 0, 0, m_ConfigData.m_flBackgroundAlpha ) );
	}
	BaseClass::PaintBackground();
}

void CBaseBudgetPanel::Paint()
{
	m_pBudgetHistoryPanel->SetData( &m_BudgetGroupTimes[0].m_Time[0], GetNumCachedBudgetGroups(), BUDGET_HISTORY_COUNT, m_BudgetHistoryOffset );
	BaseClass::Paint();
}

void CBaseBudgetPanel::MarkForFullRepaint()
{
	Repaint();
	m_pBudgetHistoryPanel->Repaint();
	m_pBudgetBarGraphPanel->Repaint();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseBudgetPanel::GetGraphLabelScreenSpaceTopAndBottom( int id, int &top, int &bottom )
{
	int x = 0;
	int y = 0;
	m_GraphLabels[id]->LocalToScreen( x, y );
	top = y;
	bottom = top + m_GraphLabels[id]->GetTall();
}

