//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//

#include "cbase.h"
#include "tf_ping_panel.h"

#include "vgui_controls/ProgressBar.h"
#include "vgui_controls/AnimationController.h"

#include "tf_gc_client.h"

#include "clientmode_tf.h"

#include "tf_matchmaking_shared.h"

static void OnConVarChangeCustomPingTolerance( IConVar *pConVar, const char *pOldString, float flOldValue )
{
	if ( GTFGCClientSystem() )
	{
		// Otherwise the client system will do this when it starts
		GTFGCClientSystem()->UpdateCustomPingTolerance();
	}
}

ConVar tf_custom_ping_enabled( "tf_custom_ping_enabled", "0", FCVAR_ARCHIVE, "",
                               false, 0.f, false, 0.f, OnConVarChangeCustomPingTolerance );
ConVar tf_custom_ping( "tf_custom_ping", "100", FCVAR_ARCHIVE, "",
                       true, (float)CUSTOM_PING_TOLERANCE_MIN, true, (float)CUSTOM_PING_TOLERANCE_MAX,
                       OnConVarChangeCustomPingTolerance );

#ifdef STAGING_ONLY
ConVar tf_custom_ping_add_random_datacenters( "tf_custom_ping_add_random_datacenters", "0" );
#endif // STAGING_ONLY

CTFPingPanel::CTFPingPanel( Panel* pPanel, const char *pszName, EMatchGroup eMatchGroup )
	: EditablePanel( pPanel, pszName ),
	m_eMatchGroup( eMatchGroup )
{
	SetProportional( true );

	m_pMainContainer = new EditablePanel( this, "MainContainer" );
	m_pCheckButton = new CheckButton( m_pMainContainer, "CheckButton", "" );
	m_pCurrentPingLabel = new Label( m_pMainContainer, "CurrentPingLabel", "" );
	m_pPingSlider = new CCvarSlider( m_pMainContainer, "PingSlider" );

	ListenForGameEvent( "ping_updated" );
}


CTFPingPanel::~CTFPingPanel()
{
	CleanupPingPanels();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPingPanel::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	LoadControlSettings( "resource/ui/MatchMakingPingPanel.res" );

	CleanupPingPanels();

	m_pCheckButton->AddActionSignalTarget( this );
	m_pPingSlider->AddActionSignalTarget( this );

	CScrollableList *pDataCenterList = FindControl< CScrollableList >( "DataCenterList", true );

	static const wchar_t *s_pwszPingFormat = L"%ls (%d ms)";

	if ( pDataCenterList && GTFGCClientSystem()->BHavePingData() )
	{
		auto pingData = GTFGCClientSystem()->GetPingData();
		const auto& dictDataCenterPopulations = GTFGCClientSystem()->GetDataCenterPopulationRatioDict( m_eMatchGroup );

		bool bTesting = false;
#ifdef STAGING_ONLY
		if ( tf_custom_ping_add_random_datacenters.GetBool() )
		{
			bTesting = true;
			const char* pszDataCenterNames[] = { "eat", "lax","iad","atl","gru","scl","lim","lux","vie","sto",
												 "mad","sgp","hkg","tyo","syd","dxb","bom","maa","ord","waw","jhb" };

			CUniformRandomStream randomstream;
			randomstream.SetSeed( tf_custom_ping_add_random_datacenters.GetInt() );
			for( int i=0; i < ARRAYSIZE( pszDataCenterNames ); ++i )
			{
				auto pNewPingData = pingData.add_pingdata();
				pNewPingData->set_name( pszDataCenterNames[ i ] );
				pNewPingData->set_ping( randomstream.RandomInt( 0, 250 ) );
				pNewPingData->set_ping_status( CMsgGCDataCenterPing_Update_Status_Normal );
			}
		}
#endif

		// for each ping data, check for intersection with data center population from MMStats
		for ( int iPing=0; iPing<pingData.pingdata_size(); ++iPing )
		{
			auto pingEntry = pingData.pingdata( iPing );
			if ( pingEntry.ping_status() != CMsgGCDataCenterPing_Update_Status_Normal )
				continue;

			const char *pszPingFromDataCenterName = pingEntry.name().c_str();
			auto dictIndex = dictDataCenterPopulations.Find( pszPingFromDataCenterName );
			// found intersection. add a population health panel
			if ( dictIndex != dictDataCenterPopulations.InvalidIndex() || bTesting)
			{
				// Load control settings
				EditablePanel* pDataCenterPopulationPanel = new EditablePanel( pDataCenterList, "DataCenterPopulationPanel" );
				pDataCenterPopulationPanel->LoadControlSettings( "resource/ui/MatchMakingDataCenterPopulationPanel.res" );
				pDataCenterPopulationPanel->SetAutoDelete( false );
				pDataCenterList->ResetScrollAmount();
				pDataCenterList->InvalidateLayout();

				// Update label
				wchar_t wszDataCenterName[ 128 ];
				wchar_t* pwszLocalizedDataCenterName = g_pVGuiLocalize->Find( CFmtStr( "#TF_DataCenter_%s", pszPingFromDataCenterName ) );
				if ( pwszLocalizedDataCenterName )
				{
					V_wcsncpy( wszDataCenterName, pwszLocalizedDataCenterName, sizeof( wszDataCenterName ) );
				}
				else
				{
					// Fallback is no token.  If you hit this, go add a string for this data center in tf_english!
					Assert( false );
					g_pVGuiLocalize->ConvertANSIToUnicode( pszPingFromDataCenterName, wszDataCenterName, sizeof( wszDataCenterName ) );
				}

				wchar_t wszLabelText[ 128 ];
				V_snwprintf( wszLabelText, sizeof( wszLabelText ), s_pwszPingFormat, wszDataCenterName, pingEntry.ping() );

				pDataCenterPopulationPanel->SetDialogVariable( "datacenter_name", wszLabelText );

				PingPanelInfo panelInfo;
				panelInfo.m_pPanel = pDataCenterPopulationPanel;
				panelInfo.m_flPopulationRatio = bTesting ? RandomFloat( 0.f, 1.f ) : dictDataCenterPopulations[dictIndex];
				panelInfo.m_nPing = pingEntry.ping();
				m_vecDataCenterPingPanels.AddToTail( panelInfo );
			}
		}
	}

	struct PingPanelInfoSorter
	{
		static int SortPingPanelInfo( const PingPanelInfo* a, const PingPanelInfo* b )
		{
			return a->m_nPing - b->m_nPing;
		}
	};
	m_vecDataCenterPingPanels.Sort( &PingPanelInfoSorter::SortPingPanelInfo );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPingPanel::PerformLayout()
{
	BaseClass::PerformLayout();

	m_pCheckButton->SetSelected( tf_custom_ping_enabled.GetBool() );

	FOR_EACH_VEC( m_vecDataCenterPingPanels, i )
	{
		const PingPanelInfo& info = m_vecDataCenterPingPanels[i];
		int iTall = info.m_pPanel->GetTall();
		int iYGap = i > 0 ? m_iDataCenterYSpace : 0;
		int iXPos = info.m_pPanel->GetXPos();
		info.m_pPanel->SetPos( iXPos, m_iDataCenterY + iYGap + i * iTall );

		// Update bars with latest health data
		ProgressBar* pProgress = info.m_pPanel->FindControl< ProgressBar >( "HealthProgressBar", true );
		if ( pProgress )
		{
			auto healthData = GTFGCClientSystem()->GetHealthBracketForRatio( info.m_flPopulationRatio );

			pProgress->MakeReadyForUse();
			pProgress->SetProgress( healthData.m_flRatio );
			pProgress->SetFgColor( healthData.m_colorBar );
		}
	}

	UpdateCurrentPing();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPingPanel::OnCommand( const char *command )
{
	if ( FStrEq( command, "close" ) )
	{
		MarkForDeletion();
		return;
	}

	BaseClass::OnCommand( command );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPingPanel::FireGameEvent( IGameEvent *event )
{
	const char *pszEventName = event->GetName();
	if ( FStrEq( pszEventName, "ping_updated" ) )
	{
		InvalidateLayout( true, true );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPingPanel::CleanupPingPanels()
{
	FOR_EACH_VEC( m_vecDataCenterPingPanels, i )
	{
		m_vecDataCenterPingPanels[i].m_pPanel->MarkForDeletion();
	}

	m_vecDataCenterPingPanels.Purge();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPingPanel::UpdateCurrentPing()
{
	bool bUsePingLimit = m_pCheckButton->IsSelected();
	int nLowestDataCenterPing = m_vecDataCenterPingPanels.Count() ? m_vecDataCenterPingPanels[0].m_nPing : 0;
	int nCurrentPingLimit = MAX( (int)m_pPingSlider->GetSliderValue(), nLowestDataCenterPing );
	m_pCurrentPingLabel->SetText( bUsePingLimit ? CFmtStr( "Ping Limit: %d", nCurrentPingLimit ) : "Ping Limit: AUTO" );

	FOR_EACH_VEC( m_vecDataCenterPingPanels, i )
	{
		const PingPanelInfo& info = m_vecDataCenterPingPanels[i];

		int bHighLight = !bUsePingLimit || info.m_nPing <= nCurrentPingLimit;
		if ( bHighLight )
		{
			g_pClientMode->GetViewportAnimationController()->StopAnimationSequence( info.m_pPanel, "HealthProgressBar_NotSelected" );
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( info.m_pPanel, "HealthProgressBar_Selected" );
		}
		else
		{
			g_pClientMode->GetViewportAnimationController()->StopAnimationSequence( info.m_pPanel, "HealthProgressBar_Selected" );
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( info.m_pPanel, "HealthProgressBar_NotSelected" );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPingPanel::OnCheckButtonChecked( vgui::Panel *panel )
{
	if ( m_pCheckButton == panel )
	{
		tf_custom_ping_enabled.SetValue( m_pCheckButton->IsSelected() );
	}

	m_pPingSlider->SetVisible( tf_custom_ping_enabled.GetBool() );

	UpdateCurrentPing();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPingPanel::OnSliderMoved()
{
	UpdateCurrentPing();
}
