//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "vgui_budgetpanelshared.h"
#include "vgui/IVGui.h"
#include "vgui/ILocalize.h"
#include "vgui/ISurface.h"
#include "vgui_controls/Label.h"
#include "ivprofexport.h"
#include "convar.h"
#include "mathlib/mathlib.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


#ifdef VPROF_ENABLED

#ifdef BUDGET_ADMIN_SERVER
	#define BUDGET_CVAR_CALLBACK NULL
#else
	#define BUDGET_CVAR_CALLBACK PanelGeometryChangedCallBack
#endif


// Global ConVars.
ConVar budget_history_range_ms( "budget_history_range_ms", "66.666666667", FCVAR_ARCHIVE, "budget history range in milliseconds", BUDGET_CVAR_CALLBACK );
ConVar budget_panel_bottom_of_history_fraction( "budget_panel_bottom_of_history_fraction", ".25", FCVAR_ARCHIVE, "number between 0 and 1", BUDGET_CVAR_CALLBACK );
ConVar budget_bargraph_range_ms( "budget_bargraph_range_ms", "16.6666666667", FCVAR_ARCHIVE, "budget bargraph range in milliseconds", BUDGET_CVAR_CALLBACK );
ConVar budget_background_alpha( "budget_background_alpha", "128", FCVAR_ARCHIVE, "how translucent the budget panel is" );

ConVar budget_panel_x( "budget_panel_x", "0", FCVAR_ARCHIVE, "number of pixels from the left side of the game screen to draw the budget panel", BUDGET_CVAR_CALLBACK );
ConVar budget_panel_y( "budget_panel_y", "50", FCVAR_ARCHIVE, "number of pixels from the top side of the game screen to draw the budget panel", BUDGET_CVAR_CALLBACK );
ConVar budget_panel_width( "budget_panel_width", "512", FCVAR_ARCHIVE, "width in pixels of the budget panel", BUDGET_CVAR_CALLBACK );
ConVar budget_panel_height( "budget_panel_height", "384", FCVAR_ARCHIVE, "height in pixels of the budget panel", BUDGET_CVAR_CALLBACK );


static CUtlVector<IVProfExport::CExportedBudgetGroupInfo> g_TempBudgetGroupSpace;


double CBudgetPanelShared::g_fFrameTimeLessBudget = 0;
double CBudgetPanelShared::g_fFrameRate = 0;

static CFastTimer g_TimerLessBudget;
static CBudgetPanelShared *g_pBudgetPanelShared = NULL;
extern IVProfExport *g_pVProfExport;


void PanelGeometryChangedCallBack( IConVar *var, const char *pOldString, float flOldValue )
{
	// screw it . . rebuild the whole damn thing.
	//	GetBudgetPanel()->InvalidateLayout();
	if ( g_pBudgetPanelShared )
	{
		g_pBudgetPanelShared->SendConfigDataToBase();
	}
}


// -------------------------------------------------------------------------------------------------------------------- //
// CBudgetPanelShared implementation.
// -------------------------------------------------------------------------------------------------------------------- //

CBudgetPanelShared::CBudgetPanelShared( vgui::Panel *pParent, const char *pElementName, int budgetFlagsFilter )
	: BaseClass( pParent, pElementName )
{
	Assert( !g_pBudgetPanelShared );
	g_pBudgetPanelShared = this;
	
	if ( g_pVProfExport )
		g_pVProfExport->SetBudgetFlagsFilter( budgetFlagsFilter );

	SendConfigDataToBase();
	SetZPos( 1001 );
	SetVisible( false );
	vgui::ivgui()->AddTickSignal( GetVPanel() );
	SetPostChildPaintEnabled( true ); // so we can turn vprof back on
}


CBudgetPanelShared::~CBudgetPanelShared()
{
	Assert( g_pBudgetPanelShared == this );
	g_pBudgetPanelShared = NULL;
}


void CBudgetPanelShared::OnNumBudgetGroupsChanged()
{
	SendConfigDataToBase();
}


void CBudgetPanelShared::SetupCustomConfigData( CBudgetPanelConfigData &data )
{
	data.m_xCoord = budget_panel_x.GetInt();
	data.m_yCoord = budget_panel_y.GetInt();
	data.m_Width = budget_panel_width.GetInt();
	data.m_Height = budget_panel_height.GetInt();
}


void CBudgetPanelShared::SendConfigDataToBase()
{
	// Setup all the data.
	CBudgetPanelConfigData data;

	// Copy the budget group names in.
	int nGroups = 0;
	if ( g_pVProfExport )
	{
		nGroups = g_pVProfExport->GetNumBudgetGroups();
	
		// Make sure we have space to store the results.
		if ( g_TempBudgetGroupSpace.Count() < nGroups )
			g_TempBudgetGroupSpace.SetSize( nGroups );

		g_pVProfExport->GetBudgetGroupInfos( g_TempBudgetGroupSpace.Base() );
	}

	data.m_BudgetGroupInfo.SetSize( nGroups );
	for ( int i=0; i < nGroups; i++ )
	{
		data.m_BudgetGroupInfo[i].m_Name = g_TempBudgetGroupSpace[i].m_pName;
		data.m_BudgetGroupInfo[i].m_Color = g_TempBudgetGroupSpace[i].m_Color;
	}

	data.m_HistoryLabelValues.AddToTail( 1000.0 / 20 );
	data.m_HistoryLabelValues.AddToTail( 1000.0 / 30 );
	data.m_HistoryLabelValues.AddToTail( 1000.0 / 60 );
	
	// Copy all the cvars in.
	data.m_flHistoryRange = budget_history_range_ms.GetFloat();
	data.m_flBottomOfHistoryFraction = budget_panel_bottom_of_history_fraction.GetFloat();

	data.m_flBarGraphRange = budget_bargraph_range_ms.GetFloat();
	data.m_flTimeLabelInterval = 5;
	data.m_nLinesPerTimeLabel = 5;

	data.m_flBackgroundAlpha = budget_background_alpha.GetFloat();
	
	SetupCustomConfigData( data );

	// Send the config data to the base class.
	OnConfigDataChanged( data );
}	


void CBudgetPanelShared::DrawColoredText( 
	vgui::HFont font, 
	int x, int y, 
	int r, int g, int b, int a,
	const char *pText,
	... )
{
	char msg[4096];
	va_list marker;
	va_start( marker, pText );
	_vsnprintf( msg, sizeof( msg ), pText, marker );
	va_end( marker );

	wchar_t unicodeStr[4096];
	int nChars = g_pVGuiLocalize->ConvertANSIToUnicode( msg, unicodeStr, sizeof( unicodeStr ) );

	vgui::surface()->DrawSetTextFont( font );
	vgui::surface()->DrawSetTextColor( r, g, b, a );
	vgui::surface()->DrawSetTextPos( x, y );
	vgui::surface()->DrawPrintText( unicodeStr, nChars );
}


void CBudgetPanelShared::PaintBackground()
{
	if ( g_pVProfExport )
		g_pVProfExport->PauseProfile();

	BaseClass::PaintBackground();
}


void CBudgetPanelShared::Paint()
{
	if( m_BudgetGroupTimes.Count() == 0 )
	{
		return;
	}

	static bool TimerInitialized = false;
	if( !TimerInitialized )
	{
		g_TimerLessBudget.Start();
		TimerInitialized=true;
	}

	g_TimerLessBudget.End();

	BaseClass::Paint();
	
	g_fFrameTimeLessBudget = ( g_TimerLessBudget.GetDuration().GetSeconds() );
	g_fFrameRate = 1.0 / g_fFrameTimeLessBudget;
}

void CBudgetPanelShared::PostChildPaint()
{
	g_TimerLessBudget.Start();

	if ( g_pVProfExport )
		g_pVProfExport->ResumeProfile();
}

void CBudgetPanelShared::SnapshotVProfHistory( float filteredtime  )
{
	m_BudgetHistoryOffset = ( m_BudgetHistoryOffset + 1 ) % BUDGET_HISTORY_COUNT;
	ClearTimesForAllGroupsForThisFrame();
	
	if ( g_pVProfExport )
	{
		if ( GetNumCachedBudgetGroups() != g_pVProfExport->GetNumBudgetGroups() )
		{
			SendConfigDataToBase();
		}

		float times[IVProfExport::MAX_BUDGETGROUP_TIMES];
		g_pVProfExport->GetBudgetGroupTimes( times );

		for ( int groupID=0; groupID < GetNumCachedBudgetGroups(); groupID++ )
		{
			float dt = times[groupID];
			// Hack:  add filtered time into unnaccounted group...
			if ( groupID == VPROF_BUDGET_GROUP_ID_UNACCOUNTED )
			{
				dt += 1000.0f * filteredtime;
			}
			m_BudgetGroupTimes[groupID].m_Time[m_BudgetHistoryOffset] = dt;
		}
	}
}


void CBudgetPanelShared::SetTimeLabelText()
{
	for ( int i=0; i < m_TimeLabels.Count(); i++ )
	{
		char text[512];
		Q_snprintf( text, sizeof( text ), "%dms", (int)( i * GetConfigData().m_flTimeLabelInterval ) );
		m_TimeLabels[i]->SetText( text );
	}
}


void CBudgetPanelShared::SetHistoryLabelText()
{
	Assert( m_HistoryLabels.Count() == 3 );
	m_HistoryLabels[0]->SetText( "20 fps (50 ms)" );
	m_HistoryLabels[1]->SetText( "30 fps (33 1/3 ms)" );
	m_HistoryLabels[2]->SetText( "60 fps (16 2/3 ms)" );
}


#endif // VPROF_ENABLED
