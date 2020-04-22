//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#include "client_pch.h"

#include "vgui_texturebudgetpanel.h"
#include "vgui_controls/Label.h"
#include "VGuiMatSurface/IMatSystemSurface.h"
#include "vgui_baseui_interface.h"
#include "ivideomode.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifdef VPROF_ENABLED

// Globals.
static CTextureBudgetPanel *g_pTextureBudgetPanel = NULL;

static void TextureCVarChangedCallBack( IConVar *pConVar, const char *pOldString, float flOldValue );


ConVar texture_budget_panel_global( "texture_budget_panel_global", "0", 0, "Show global times in the texture budget panel." );
ConVar showbudget_texture( "showbudget_texture", "0", FCVAR_CHEAT, "Enable the texture budget panel." );
ConVar showbudget_texture_global_sum( "showbudget_texture_global_sum", "0.0f" );


// Commands to turn on the texture budget panel, with per-FRAME settings.
void showbudget_texture_on_f()
{
	texture_budget_panel_global.SetValue( 0 );
	showbudget_texture.SetValue( 1 );
}
void showbudget_texture_off_f()
{
	showbudget_texture.SetValue( 0 );
}
ConCommand showbudget_texture_on( "+showbudget_texture", showbudget_texture_on_f, "", FCVAR_CHEAT );
ConCommand showbudget_texture_off( "-showbudget_texture", showbudget_texture_off_f, "", FCVAR_CHEAT );


// Commands to turn on the texture budget panel, with GLOBAL settings.
void showbudget_texture_global_on_f()
{
	texture_budget_panel_global.SetValue( 1 );
	showbudget_texture.SetValue( 1 );
}
void showbudget_texture_global_off_f()
{
	showbudget_texture.SetValue( 0 );
}
ConCommand showbudget_texture_global_on( "+showbudget_texture_global", showbudget_texture_global_on_f, "", FCVAR_CHEAT );
ConCommand showbudget_texture_global_off( "-showbudget_texture_global", showbudget_texture_global_off_f, "", FCVAR_CHEAT );


ConVar texture_budget_panel_x( "texture_budget_panel_x", "0", FCVAR_ARCHIVE, "number of pixels from the left side of the game screen to draw the budget panel", TextureCVarChangedCallBack );
ConVar texture_budget_panel_y( "texture_budget_panel_y", "450", FCVAR_ARCHIVE, "number of pixels from the top side of the game screen to draw the budget panel", TextureCVarChangedCallBack );
ConVar texture_budget_panel_width( "texture_budget_panel_width", "512", FCVAR_ARCHIVE, "width in pixels of the budget panel", TextureCVarChangedCallBack );
ConVar texture_budget_panel_height( "texture_budget_panel_height", "284", FCVAR_ARCHIVE, "height in pixels of the budget panel", TextureCVarChangedCallBack );
ConVar texture_budget_panel_bottom_of_history_fraction( "texture_budget_panel_bottom_of_history_fraction", ".25", FCVAR_ARCHIVE, "number between 0 and 1", TextureCVarChangedCallBack );

ConVar texture_budget_background_alpha( "texture_budget_background_alpha", "128", FCVAR_ARCHIVE, "how translucent the budget panel is" );


CTextureBudgetPanel *GetTextureBudgetPanel( void )
{
	return g_pTextureBudgetPanel;
}


static void TextureCVarChangedCallBack( IConVar *pConVar, const char *pOldString, float flOldValue )
{
	if ( GetTextureBudgetPanel() )
	{
		GetTextureBudgetPanel()->OnCVarStateChanged();
	}
}


CTextureBudgetPanel::CTextureBudgetPanel( vgui::Panel *pParent, const char *pElementName )
	: BaseClass( pParent, pElementName )
{
	m_LastCounterGroup = -1;
	g_pTextureBudgetPanel = this;
	
	m_MaxValue = 1000;
	m_SumOfValues = 0;

	m_pModeLabel = new vgui::Label( this, "mode label", "" );
	m_pModeLabel->SetParent( pParent );
	SetVisible( false );
	vgui::ivgui()->AddTickSignal( GetVPanel(), 0 );
}


CTextureBudgetPanel::~CTextureBudgetPanel()
{
	Assert( g_pTextureBudgetPanel == this );
	g_pTextureBudgetPanel = NULL;
	if ( m_pModeLabel )
	{
		delete m_pModeLabel;
		m_pModeLabel = NULL;
	}
}

			 
void CTextureBudgetPanel::OnTick()
{
	BaseClass::OnTick();
	if ( showbudget_texture.GetBool() )
	{
		m_pModeLabel->SetVisible( true );
		SetVisible( true );
	}
	else
	{
		m_pModeLabel->SetVisible( false );
		SetVisible( false );
	}
}


void CTextureBudgetPanel::Paint()
{
	SnapshotTextureHistory();
	g_VProfCurrentProfile.ResetCounters( COUNTER_GROUP_TEXTURE_PER_FRAME );

	BaseClass::Paint();
}


void CTextureBudgetPanel::SendConfigDataToBase()
{
	// Setup all the data.
	CBudgetPanelConfigData data;

	// Copy the budget group names in.
	for ( int i=0; i < g_VProfCurrentProfile.GetNumCounters(); i++ )
	{
		if ( g_VProfCurrentProfile.GetCounterGroup( i ) == GetCurrentCounterGroup() )
		{		
			// Strip off the TexGroup__ prefix.
			const char *pGroupName = g_VProfCurrentProfile.GetCounterName( i );

			const char *pPrefixes[2] = { "TexGroup_global_", "TexGroup_frame_" };
			for ( int iPrefix=0; iPrefix < 2; iPrefix++ )
			{
				char alternateName[256];

				if ( strstr( pGroupName, pPrefixes[iPrefix] ) == pGroupName )
				{
					int len = strlen( pPrefixes[iPrefix] );
					Q_strncpy( alternateName, &pGroupName[len], len );
					alternateName[len] = 0;

					pGroupName = alternateName;
					break;
				}
			}

			CBudgetGroupInfo info;
			
			int r, g, b, a;
			g_VProfCurrentProfile.GetBudgetGroupColor( data.m_BudgetGroupInfo.Count(), r, g, b, a );
			info.m_Color.SetColor( r, g, b, a );

			info.m_Name = pGroupName;
			data.m_BudgetGroupInfo.AddToTail( info );
		}
	}


	// Copy all the cvars in.
	data.m_flBottomOfHistoryFraction = texture_budget_panel_bottom_of_history_fraction.GetFloat();

	data.m_flBarGraphRange = (m_MaxValue * 4) / 3;
	data.m_flTimeLabelInterval = data.m_flBarGraphRange / 4;
	data.m_nLinesPerTimeLabel = 4;

	data.m_flHistoryRange = (m_SumOfValues * 4) / 3;

	// Use the middle three fifths for history labels.
	data.m_HistoryLabelValues.SetSize( 3 );
	for ( int i=0; i < data.m_HistoryLabelValues.Count(); i++ )
	{
		data.m_HistoryLabelValues[i] = (i+1) * data.m_flHistoryRange / 4;
	}

	data.m_flBackgroundAlpha = texture_budget_background_alpha.GetFloat();
	
	data.m_xCoord = texture_budget_panel_x.GetInt();
	data.m_yCoord = texture_budget_panel_y.GetInt();
	data.m_Width = texture_budget_panel_width.GetInt();
	data.m_Height = texture_budget_panel_height.GetInt();

	// Shift it..
	if ( data.m_xCoord + data.m_Width > videomode->GetModeStereoWidth() )
	{
		data.m_xCoord = videomode->GetModeStereoWidth() - data.m_Width;
	}
	if ( data.m_yCoord + data.m_Height > videomode->GetModeStereoHeight() )
	{
		data.m_yCoord = videomode->GetModeStereoHeight() - data.m_Height;
	}

	// Send the config data to the base class.
	OnConfigDataChanged( data );
}	


void CTextureBudgetPanel::PerformLayout()
{
	BaseClass::PerformLayout();

	// Update our label that tells what kind of data we are.
	const char *pStr = "Per-frame texture stats";

	if ( texture_budget_panel_global.GetInt() )
		pStr = "Global texture stats";

	m_pModeLabel->SetText( pStr );
	int width = g_pMatSystemSurface->DrawTextLen( m_pModeLabel->GetFont(), "%s", pStr );
	m_pModeLabel->SetSize( width + 10, m_pModeLabel->GetTall() );

	int x, y;
	GetPos( x, y );

	m_pModeLabel->SetPos( x, y - m_pModeLabel->GetTall() );
	m_pModeLabel->SetFgColor( Color( 255, 255, 255, 255 ) );
	m_pModeLabel->SetBgColor( Color( 0, 0, 0, texture_budget_background_alpha.GetInt() ) );
}


void CTextureBudgetPanel::OnCVarStateChanged()
{
	SendConfigDataToBase();
}


CounterGroup_t CTextureBudgetPanel::GetCurrentCounterGroup() const
{
	if ( texture_budget_panel_global.GetInt() )
		return COUNTER_GROUP_TEXTURE_GLOBAL;
	else
		return COUNTER_GROUP_TEXTURE_PER_FRAME;
}


void CTextureBudgetPanel::SnapshotTextureHistory()
{
	// Now sample all the data.
	CVProfile *pProf = &g_VProfCurrentProfile;

	m_SumOfValues = 0;
	for ( int i=0; i < pProf->GetNumCounters(); i++ )
	{
		if ( pProf->GetCounterGroup( i ) == GetCurrentCounterGroup() )
		{
			// The counters are in bytes and the panel is all in kilobytes.
			int value = pProf->GetCounterValue( i ) / 1024;
			
			m_SumOfValues += value;
			m_MaxValue = max( m_MaxValue, value );
		}
	}
	
	showbudget_texture_global_sum.SetValue( m_SumOfValues );
	
	// Send new config data if the range has expanded.
	bool bForceSendConfigData = false;
	if ( (float)m_MaxValue > GetConfigData().m_flBarGraphRange || m_SumOfValues > GetConfigData().m_flHistoryRange )
	{
		bForceSendConfigData = true;
	}

	
	// If we switched counter groups, reset everything.
	if ( m_LastCounterGroup != GetCurrentCounterGroup() )
	{
		ResetAll();
		m_LastCounterGroup = GetCurrentCounterGroup();
	}


	// Count up the current number of counters.
	int nCounters = 0;
	for ( int i=0; i < g_VProfCurrentProfile.GetNumCounters(); i++ )
	{
		if ( g_VProfCurrentProfile.GetCounterGroup( i ) == GetCurrentCounterGroup() )
			++nCounters;
	}

	// Do we need to reset all the data?
	if ( bForceSendConfigData || nCounters != GetNumCachedBudgetGroups() )
	{
		SendConfigDataToBase();
	}


	// Now update the data for this frame.
	m_BudgetHistoryOffset = ( m_BudgetHistoryOffset + 1 ) % BUDGET_HISTORY_COUNT;

	int groupID = 0;
	for ( int i=0; i < pProf->GetNumCounters(); i++ )
	{
		if ( pProf->GetCounterGroup( i ) == GetCurrentCounterGroup() )
		{
			// The counters are in bytes and the panel is all in kilobytes.
			int value = pProf->GetCounterValue( i ) / 1024;
			m_BudgetGroupTimes[groupID].m_Time[m_BudgetHistoryOffset] = value;
			++groupID;
		}
	}
}


void CTextureBudgetPanel::SetTimeLabelText()
{
	for ( int i=0; i < m_TimeLabels.Count(); i++ )
	{
		char text[512];
		Q_snprintf( text, sizeof( text ), "%.1fM", (float)( i * GetConfigData().m_flTimeLabelInterval ) / 1024 );
		m_TimeLabels[i]->SetText( text );
	}
}


void CTextureBudgetPanel::SetHistoryLabelText()
{
	for ( int i=0; i < m_HistoryLabels.Count(); i++ )
	{
		char text[512];
		Q_snprintf( text, sizeof( text ), "%.1fM", GetConfigData().m_HistoryLabelValues[i] / 1024 );
		m_HistoryLabels[i]->SetText( text );
	}
}


void CTextureBudgetPanel::ResetAll()
{
	BaseClass::ResetAll();
	
	m_MaxValue = 0;
	m_SumOfValues = 0;
}

void CTextureBudgetPanel::DumpGlobalTextureStats( const CCommand &args )
{
	// Setup all the data.
	CBudgetPanelConfigData data;

	// Copy the budget group names in.
	for ( int i=0; i < g_VProfCurrentProfile.GetNumCounters(); i++ )
	{
		if ( g_VProfCurrentProfile.GetCounterGroup( i ) == COUNTER_GROUP_TEXTURE_GLOBAL )
		{		
			// Strip off the TexGroup__ prefix.
			const char *pGroupName = g_VProfCurrentProfile.GetCounterName( i );

			// The counters are in bytes and the panel is all in kilobytes.
//			int value = pProf->GetCounterValue( i ) / 1024;

			Warning( "%s: %d\n", pGroupName,  ( int )g_VProfCurrentProfile.GetCounterValue( i ) );
		}
	}
}

#endif // VPROF_ENABLED
