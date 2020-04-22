//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "report_player_dialog.h"
#include "gc_clientsystem.h"
#include "ienginevgui.h"

using namespace vgui;

// in seconds
static const float MIN_REPORT_INTERVAL = 300.f;

struct ReportedPlayer_t
{
	CSteamID steamID;
	float flReportedTime;
};
CUtlVector< ReportedPlayer_t > vecReportedPlayers;

bool CanReportPlayer( CSteamID steamID, bool bVerbose )
{
	bool bCanReport = true;
	for (int i = 0; i < vecReportedPlayers.Count(); ++i)
	{
		if ( vecReportedPlayers[i].steamID == steamID )
		{
			float flTimeSinceLastReported = gpGlobals->curtime - vecReportedPlayers[i].flReportedTime;
			bCanReport = flTimeSinceLastReported >= MIN_REPORT_INTERVAL;
			if ( !bCanReport && bVerbose )
			{
				float flCooldownTime = MIN_REPORT_INTERVAL - flTimeSinceLastReported;
				ConMsg( "Already reported this player. You can report this player again in %.2f seconds\n", flCooldownTime );
			}
			break;
		}
	}

	return bCanReport;
}

bool ReportPlayerAccount( CSteamID steamID, int nReason )
{
	if ( !steamID.IsValid() )
	{
		Warning( "Reporting an invalid steam ID\n" );
		return false;
	}

	if ( !CanReportPlayer( steamID, true ) )
	{
		return false;
	}

	if ( nReason <= CMsgGC_ReportPlayer_EReason_kReason_INVALID || nReason >= CMsgGC_ReportPlayer_EReason_kReason_COUNT )
	{
		Assert( !"Invalid report reason" );
		return false;
	}

	GCSDK::CProtoBufMsg< CMsgGC_ReportPlayer > msg( k_EMsgGC_ReportPlayer );
	msg.Body().set_account_id_target( steamID.GetAccountID() );
	msg.Body().set_reason( (CMsgGC_ReportPlayer_EReason)nReason );
	GCClientSystem()->BSendMessage( msg );
	ConMsg( "Report sent. Thank you.\n" );

	ReportedPlayer_t reportedPlayer;
	reportedPlayer.steamID = steamID;
	reportedPlayer.flReportedTime = gpGlobals->curtime;
	vecReportedPlayers.AddToTail( reportedPlayer );
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CReportPlayerDialog::CReportPlayerDialog( vgui::Panel *parent ) : BaseClass( parent, "ReportPlayerDialog" )
{
	vgui::VPANEL gameuiPanel = enginevgui->GetPanel( PANEL_GAMEUIDLL );
	SetParent( gameuiPanel );

	vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFile("resource/SourceScheme.res", "Client");
	SetScheme(scheme);

	SetSize( 320, 270 );
	SetTitle( "#GameUI_ReportPlayerCaps", true );

	m_pReportButton = new Button( this, "ReportButton", "" );
	m_pPlayerList = new ListPanel( this, "PlayerList" );
	m_pPlayerList->AddColumnHeader( 0, "Name", "#GameUI_PlayerName", 180 );
	m_pPlayerList->AddColumnHeader( 1, "Properties", "#GameUI_Properties", 80 );
	m_pPlayerList->SetEmptyListText( "#GameUI_NoOtherPlayersInGame" );
	m_pReasonBox = new ComboBox( this, "ReasonBox", 5, false );

	LoadControlSettings( "Resource/ReportPlayerDialog.res" );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CReportPlayerDialog::~CReportPlayerDialog()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CReportPlayerDialog::Activate()
{
	BaseClass::Activate();

	m_pPlayerList->DeleteAllItems();

	static EUniverse universe = steamapicontext->SteamUtils()->GetConnectedUniverse();

	for ( int i = 1; i <= engine->GetMaxClients(); i++ )
	{
		player_info_t pi;
		if ( !engine->GetPlayerInfo( i, &pi ) )
			continue;

		// no need to add local player
		if ( engine->GetLocalPlayer() == i )
			continue;

		// Already reported
		CSteamID steamID( pi.friendsID, universe, k_EAccountTypeIndividual );
		if ( !CanReportPlayer( steamID, false ) )
		{
			continue;
		}

		char szPlayerIndex[32];
		Q_snprintf( szPlayerIndex, sizeof( szPlayerIndex ), "%d", i );

		KeyValues *pData = new KeyValues( szPlayerIndex );
		pData->SetString( "Name", pi.name );
		pData->SetInt( "index", i );
		m_pPlayerList->AddItem( pData, 0, false, false );
	}

	m_pReasonBox->RemoveAll();
	KeyValues *pKeyValues = new KeyValues( "data" );
	SetDialogVariable( "combo_label", g_pVGuiLocalize->Find( "#GameUI_ReportPlayerReason" ) );
	pKeyValues->SetInt( "reason", 0 );
	m_pReasonBox->AddItem( g_pVGuiLocalize->Find( "GameUI_ReportPlayer_Choose" ), pKeyValues );
	pKeyValues->SetInt( "reason", 1 );
	m_pReasonBox->AddItem( g_pVGuiLocalize->Find( "GameUI_ReportPlayer_Cheating" ), pKeyValues );
	pKeyValues->SetInt( "reason", 2 );
	m_pReasonBox->AddItem( g_pVGuiLocalize->Find( "GameUI_ReportPlayer_Idle" ), pKeyValues );
	pKeyValues->SetInt( "reason", 3 );
	m_pReasonBox->AddItem( g_pVGuiLocalize->Find( "GameUI_ReportPlayer_Harassment" ), pKeyValues );
	pKeyValues->SetInt( "reason", 4 );
	m_pReasonBox->AddItem( g_pVGuiLocalize->Find( "GameUI_ReportPlayer_Griefing" ), pKeyValues );
	m_pReasonBox->SilentActivateItemByRow( 0 );
	pKeyValues->deleteThis();

	RefreshPlayerProperties();
	m_pPlayerList->SetSingleSelectedItem( m_pPlayerList->GetItemIDFromRow( 0 ) );
	OnItemSelected();
}

//-----------------------------------------------------------------------------
// Purpose: walks the players and sets their info display in the list
//-----------------------------------------------------------------------------
void CReportPlayerDialog::RefreshPlayerProperties()
{
	for ( int i = 0; i <= m_pPlayerList->GetItemCount(); i++ )
	{
		KeyValues *pData = m_pPlayerList->GetItem( i );
		if ( !pData )
			continue;

		int playerIndex = pData->GetInt( "index" );
		
		player_info_t pi;
		if ( !engine->GetPlayerInfo( playerIndex, &pi ) )
		{
			pData->SetString( "properties", "Disconnected" );
			continue;
		}

		pData->SetString( "name", pi.name );

		if ( pi.fakeplayer )
		{
			pData->SetString( "properties", "CPU Player" );
		}
		else
		{
			pData->SetString( "properties", "" );
		}
	}
	m_pPlayerList->RereadAllItems();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CReportPlayerDialog::IsValidPlayerSelected()
{
	bool bIsValidPlayer = false;

	if ( m_pPlayerList->GetSelectedItemsCount() > 0 )
	{
		KeyValues *pData = m_pPlayerList->GetItem( m_pPlayerList->GetSelectedItem( 0 ) );
		player_info_t pi;
		bIsValidPlayer = engine->GetPlayerInfo( pData->GetInt( "index" ), &pi );
#ifdef _DEBUG
		bIsValidPlayer = bIsValidPlayer && pData->GetInt( "index" ) != engine->GetLocalPlayer();
#else
		bIsValidPlayer = bIsValidPlayer && !pi.fakeplayer && pData->GetInt( "index" ) != engine->GetLocalPlayer();
#endif
	}

	return bIsValidPlayer;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CReportPlayerDialog::OnCommand( const char *command )
{
	if ( !stricmp( command, "Report" ) )
	{
		ReportPlayer();
	}
	else
	{
		BaseClass::OnCommand( command );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CReportPlayerDialog::ReportPlayer()
{
	for ( int iSelectedItem = 0; iSelectedItem < m_pPlayerList->GetSelectedItemsCount(); iSelectedItem++ )
	{
		KeyValues *pPlayerData = m_pPlayerList->GetItem( m_pPlayerList->GetSelectedItem( iSelectedItem ) );
		if ( !pPlayerData )
			return;

		Assert( pPlayerData->GetInt( "index" ) );

		// 	INVALID = 0;
		// 	CHEATING = 1;
		// 	IDLE = 2;
		// 	HARASSMENT = 3;
		//	GRIEFING = 4;

		player_info_t pi;
		if ( !engine->GetPlayerInfo( pPlayerData->GetInt( "index" ), &pi ) )
			return;

		CSteamID steamID( pi.friendsID, GetUniverse(), k_EAccountTypeIndividual );
		KeyValues *pReasonData = m_pReasonBox->GetActiveItemUserData();
		int nReason = ( pReasonData ) ? pReasonData->GetInt( "reason", 0 ) : 0;
		ReportPlayerAccount( steamID, nReason );
		Close();
		return;
	}

	RefreshPlayerProperties();
	OnItemSelected();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CReportPlayerDialog::OnItemSelected()
{
	RefreshPlayerProperties();

	bool bReportButtonEnabled = IsValidPlayerSelected();
	if ( !bReportButtonEnabled )
	{
		m_pReportButton->SetText( "#GameUI_ReportPlayer" );
	}

	// Reason selected?
	KeyValues *pUserData = m_pReasonBox->GetActiveItemUserData();
	bReportButtonEnabled = bReportButtonEnabled && pUserData && pUserData->GetInt( "reason", 0 ) > 0;
	m_pReportButton->SetEnabled( bReportButtonEnabled );
}

//-----------------------------------------------------------------------------
// Purpose: Called when text changes in combo box
//-----------------------------------------------------------------------------
void CReportPlayerDialog::OnTextChanged( KeyValues *data )
{
	Panel *pPanel = reinterpret_cast< vgui::Panel* >( data->GetPtr( "panel" ) );
	vgui::ComboBox *pComboBox = dynamic_cast< vgui::ComboBox* >( pPanel );
	if ( pComboBox && pComboBox == m_pReasonBox )
	{
		bool bReportButtonEnabled = IsValidPlayerSelected();
		KeyValues *pReasonData = m_pReasonBox->GetActiveItemUserData();
		bReportButtonEnabled = bReportButtonEnabled && pReasonData && pReasonData->GetInt( "reason", 0 ) > 0;
		m_pReportButton->SetEnabled( bReportButtonEnabled );
	}
}
