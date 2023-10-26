#include "cbase.h"
#include "stat_graph_player.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/Button.h"
#include "objectivemap.h"
#include "stat_graph.h"
#include "c_asw_player.h"
#include "c_playerresource.h"
#include "asw_gamerules.h"
#include "c_asw_game_resource.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


ConVar asw_stats_scrub_time( "asw_stats_scrub_time", "6.0", FCVAR_CHEAT, "Time that it takes to do the full stats time scrub." );


void GetVGUICursorPos( int& x, int& y );


StatGraphPlayer::StatGraphPlayer( vgui::Panel *parent, const char *name ) : vgui::EditablePanel( parent, name )
{
	for ( int i = 0; i < ASW_STATS_REPORT_MAX_PLAYERS; i++ )
	{
		m_pStatGraphs[ i ] = new StatGraph( this, VarArgs( "StatGraph%d", i ) );
	}

	m_pValueBox = new vgui::Panel( this, "ValueBox" );

	for ( int i = 0; i < ASW_STAT_GRAPH_VALUE_STAMPS; i++ )
	{
		m_pValueStamps[ i ] = new vgui::Label( this, VarArgs( "ValueStamp%d", i ), "" );
	}

	m_pTimeLine = new vgui::Panel( this, "TimeLine" );
	m_pTimeScrub = new vgui::Panel( this, "TimeScrub" );
	m_pTimeScrubBar = new vgui::Panel( this, "TimeScrubBar" );

	for ( int i = 0; i < ASW_STAT_GRAPH_TIME_STAMPS; i++ )
	{
		m_pTimeStamps[ i ] = new vgui::Label( this, VarArgs( "TimeStamp%d", i ), "" );
	}

	m_fTimeInterp = 0.0f;
	m_bPaused = false;
}

StatGraphPlayer::~StatGraphPlayer()
{

}

void StatGraphPlayer::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	LoadControlSettings( "resource/UI/StatGraphPlayer.res" );

	BaseClass::ApplySchemeSettings(pScheme);
}

void StatGraphPlayer::PerformLayout()
{
	BaseClass::PerformLayout();

	int padding = YRES( 4 );

	int nGraphX, nGraphY, nGraphW, nGraphH;
	m_pStatGraphs[ 0 ]->GetBounds( nGraphX, nGraphY, nGraphW, nGraphH );

	for ( int i = 1; i < ASW_STATS_REPORT_MAX_PLAYERS; i++ )
	{
		m_pStatGraphs[ i ]->SetPos( nGraphX, nGraphY + i * 2 );
		m_pStatGraphs[ i ]->SetSize( nGraphW, nGraphH );
		m_pStatGraphs[ i ]->SetVerticalPixelSeparation( ASW_STATS_REPORT_MAX_PLAYERS * 2 );
	}

	int nValueBoxX, nValueBoxY, nValueBoxW, nValueBoxH;
	m_pValueBox->GetBounds( nValueBoxX, nValueBoxY, nValueBoxW, nValueBoxH );

	m_pValueStamps[ 0 ]->SizeToContents();

	int nValueStampHeight = ( nValueBoxH - m_pValueStamps[ 0 ]->GetTall() ) / ( ASW_STAT_GRAPH_VALUE_STAMPS - 1 );

	for ( int i = 0; i < ASW_STAT_GRAPH_VALUE_STAMPS; i++ )
	{
		m_pValueStamps[ i ]->SetPos( nValueBoxX, nValueBoxY + nValueStampHeight * i );
		m_pValueStamps[ i ]->SetSize( nValueBoxW, nValueStampHeight );

		float fValueInterp = static_cast< float >( i ) / ( ASW_STAT_GRAPH_VALUE_STAMPS - 1 );
		float fValue = m_pStatGraphs[ 0 ]->GetMinValue() * fValueInterp + m_pStatGraphs[ 0 ]->GetMaxValue() * ( 1.0f - fValueInterp );
		m_pValueStamps[ i ]->SetText( VarArgs( "%.0f", fValue ) );
	}

	int nTimelineX, nTimelineY, nTimelineW, nTimelineH;
	m_pTimeLine->GetBounds( nTimelineX, nTimelineY, nTimelineW, nTimelineH );

	m_pTimeStamps[ ASW_STAT_GRAPH_TIME_STAMPS - 1 ]->SizeToContents();

	int nTimeStampWidth = ( nTimelineW - m_pTimeStamps[ ASW_STAT_GRAPH_TIME_STAMPS - 1 ]->GetWide() - padding / 2 ) / ( ASW_STAT_GRAPH_TIME_STAMPS - 1 );

	for ( int i = 0; i < ASW_STAT_GRAPH_TIME_STAMPS; i++ )
	{
		m_pTimeStamps[ i ]->SetPos( nTimelineX + padding / 2 + nTimeStampWidth * i, nTimelineY );
		m_pTimeStamps[ i ]->SetSize( nTimeStampWidth, nTimelineH );

		float fValueInterp = static_cast< float >( i ) / ( ASW_STAT_GRAPH_TIME_STAMPS - 1 );
		int nSeconds = m_pStatGraphs[ 0 ]->GetStartTime() * ( 1.0f - fValueInterp ) + m_pStatGraphs[ 0 ]->GetEndTime() * fValueInterp - m_pStatGraphs[ 0 ]->GetStartTime();
		int nMinutes = nSeconds / 60;
		nSeconds -= nMinutes * 60;

		m_pTimeStamps[ i ]->SetText( VarArgs( "%i:%02i", nMinutes, nSeconds ) );
	}

	m_pTimeScrub->SetTall( nTimelineH );
	m_pTimeScrub->SetWide( ( nTimelineH - 2 ) / 2 );

	m_pTimeScrubBar->SetTall( nGraphH );
}

void StatGraphPlayer::OnThink()
{
	int nTimeLineX, nTimeLineY;
	m_pTimeLine->GetPos( nTimeLineX, nTimeLineY );

	if ( m_pTimeLine->IsCursorOver() || m_pStatGraphs[ 0 ]->IsCursorOver() )
	{
		int nMouseX, nMouseY;
		GetVGUICursorPos( nMouseX, nMouseY );

		m_pTimeLine->ScreenToLocal( nMouseX, nMouseY );

		m_fTimeInterp = static_cast< float >( nMouseX ) / m_pTimeLine->GetWide();
	}
	else if ( !m_bPaused )
	{
		if ( m_fTimeInterp > 1.5f )
		{
			m_fTimeInterp = 0.0f;
		}
		else
		{
			m_fTimeInterp += gpGlobals->frametime / asw_stats_scrub_time.GetFloat();
		}
	}

	int nGraphX, nGraphY, nGraphW, nGraphH;
	m_pStatGraphs[ 0 ]->GetBounds( nGraphX, nGraphY, nGraphW, nGraphH );

	int nScrubPosX = nTimeLineX + 1 + MIN( 1.0f, m_fTimeInterp ) * ( m_pTimeLine->GetWide() - m_pTimeScrub->GetWide() - 1 );

	m_pTimeScrub->SetPos( nScrubPosX, nTimeLineY );
	m_pTimeScrubBar->SetPos( nScrubPosX + m_pTimeScrub->GetWide() / 2 - m_pTimeScrubBar->GetWide() / 2, nGraphY );
}

