//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Present a list of sessions from which the player can choose a game to join.
//
//=============================================================================//

#include "sessionbrowserdialog.h"
#include "engine/imatchmaking.h"
#include "EngineInterface.h"
#include "vgui_controls/ImagePanel.h"
#include "vgui_controls/Label.h"
#include "KeyValues.h"
#include "vgui/ISurface.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CSessionBrowserDialog *g_pBrowserDialog;

//--------------------------------
// CSessionBrowserDialog
//--------------------------------
CSessionBrowserDialog::CSessionBrowserDialog( vgui::Panel *pParent, KeyValues *pDialogKeys ) : BaseClass( pParent, "" )
{
	g_pBrowserDialog = this;
	m_pDialogKeys = pDialogKeys;

	SetDeleteSelfOnClose( true );
}

CSessionBrowserDialog::~CSessionBrowserDialog()
{
	m_pScenarioInfos.PurgeAndDeleteElements();
}

//---------------------------------------------------------------------
// Purpose: Center the dialog on the screen
//---------------------------------------------------------------------
void CSessionBrowserDialog::PerformLayout()
{
	BaseClass::PerformLayout();

	MoveToCenterOfScreen();
	UpdateScenarioDisplay();
}

//---------------------------------------------------------------------
// Purpose: Parse session properties and contexts from the resource file
//---------------------------------------------------------------------
void CSessionBrowserDialog::ApplySettings( KeyValues *pResourceData )
{
	BaseClass::ApplySettings( pResourceData );

	KeyValues *pScenarios = pResourceData->FindKey( "ScenarioInfoPanels" );
	if ( pScenarios )
	{
		for ( KeyValues *pScenario = pScenarios->GetFirstSubKey(); pScenario != NULL; pScenario = pScenario->GetNextKey() )
		{
			CScenarioInfoPanel *pScenarioInfo = new CScenarioInfoPanel( this, "ScenarioInfoPanel" );
			SETUP_PANEL( pScenarioInfo );
			pScenarioInfo->m_pTitle->SetText( pScenario->GetString( "title" ) );
			pScenarioInfo->m_pSubtitle->SetText( pScenario->GetString( "subtitle" ) );
			pScenarioInfo->m_pMapImage->SetImage( pScenario->GetString( "image" ) );

			int nTall = pScenario->GetInt( "tall", -1 );
			if ( nTall > 0 )
			{
				pScenarioInfo->SetTall( nTall );
			}

			int nXPos = pScenario->GetInt( "xpos", -1 );
			if ( nXPos >= 0 )
			{
				int x, y;
				pScenarioInfo->GetPos( x, y );
				pScenarioInfo->SetPos( nXPos, y );
			}

			int nDescOneYpos = pScenario->GetInt( "descOneY", -1 );
			if ( nDescOneYpos > 0 )
			{
				int x, y;
				pScenarioInfo->m_pDescOne->GetPos( x, y );
				pScenarioInfo->m_pDescOne->SetPos( x, nDescOneYpos );
			}

			int nDescTwoYpos = pScenario->GetInt( "descTwoY", -1 );
			if ( nDescTwoYpos > 0 )
			{
				int x, y;
				pScenarioInfo->m_pDescTwo->GetPos( x, y );
				pScenarioInfo->m_pDescTwo->SetPos( x, nDescTwoYpos );
			}

			m_pScenarioInfos.AddToTail( pScenarioInfo );
		}
	}
}

//---------------------------------------------------------------------
// Purpose: Set up colors and other such stuff
//---------------------------------------------------------------------
void CSessionBrowserDialog::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	for ( int i = 0; i < m_pScenarioInfos.Count(); ++i )
	{
		m_pScenarioInfos[i]->SetBgColor( pScheme->GetColor( "TanDark", Color( 0, 0, 0, 255 ) ) );
	}
}

//---------------------------------------------------------------------
// Purpose: Info about a session, sent from matchmaking
//---------------------------------------------------------------------
void CSessionBrowserDialog::SessionSearchResult( int searchIdx, void *pHostData, XSESSION_SEARCHRESULT *pResult, int ping )
{
	char *pPing;
	switch ( ping )
	{
	case 0:
		pPing = "#TF_Icon_Ping_Green";
		break;
	case 1:
		pPing = "#TF_Icon_Ping_Yellow";
		break;
	case 2:
	default:
		pPing = "#TF_Icon_Ping_Red";
		break;
	}

	int nScenarioId = 0;
	uint nContextId = m_pDialogKeys->GetInt( "scenario", -1 );
	for ( uint i = 0; i < pResult->cContexts; ++i )
	{
		if ( pResult->pContexts[i].dwContextId == nContextId )
		{
			nScenarioId = pResult->pContexts[i].dwValue;
			break;
		}
	}
 
	hostData_s *pData = (hostData_s*)pHostData;

	int filledSlots = pResult->dwFilledPublicSlots + pResult->dwFilledPrivateSlots;
	int totalSlots = filledSlots + pResult->dwOpenPublicSlots + pResult->dwOpenPrivateSlots;

	m_GameStates.AddToTail( pData->gameState );
	m_GameTimes.AddToTail( pData->gameTime );
	m_XUIDs.AddToTail( pData->xuid );

	char szSlots[16] = {0};
	Q_snprintf( szSlots, sizeof( szSlots ), "%d/%d", filledSlots, totalSlots );

	int ct = 0;
	const char *ppStrings[4];
	ppStrings[ct++] = pData->hostName;
	ppStrings[ct++] = szSlots;
	ppStrings[ct++] = pData->scenario;
	if ( ping != -1 )
	{
		ppStrings[ct++] = pPing;
	}
	m_Menu.AddSectionedItem( ppStrings, ct );

	m_ScenarioIndices.AddToTail( nScenarioId );
	m_SearchIndices.AddToTail( searchIdx );

	if ( m_Menu.GetItemCount() == 1 )
	{
		m_Menu.SetFocus( 0 );
	}

	UpdateScenarioDisplay();
}

//-----------------------------------------------------------------
// Purpose: Show the correct scenario image and text
//-----------------------------------------------------------------
void CSessionBrowserDialog::UpdateScenarioDisplay( void )
{
	if ( !m_ScenarioIndices.Count() )
		return;

	// Check if the selected map has changed (first menu item)
	int idx = m_Menu.GetActiveItemIndex();
	for ( int i = 0; i < m_pScenarioInfos.Count(); ++i )
	{
		m_pScenarioInfos[i]->SetVisible( i == m_ScenarioIndices[idx] );
	}

	// Get the screen size
	int wide, tall;
	vgui::surface()->GetScreenSize(wide, tall);
	bool bLodef = ( tall <= 480 );

	const char *pState = "";
	switch( m_GameStates[idx] )
	{
	case 0:
		if ( bLodef )
		{
			pState = "#TF_GameState_InLobby_lodef";
		}
		else
		{
			pState = "#TF_GameState_InLobby";
		}
		break;

	case 1:
		if ( bLodef )
		{
			pState = "#TF_GameState_GameInProgress_lodef";
		}
		else
		{
			pState = "#TF_GameState_GameInProgress";
		}
		break;
	}

	char szTime[32] = {0};
	if ( m_GameTimes[idx] >= NO_TIME_LIMIT )
	{
		Q_strncpy( szTime, "#TF_NoTimeLimit", sizeof( szTime) );
	}
	else
	{
		Q_snprintf( szTime, sizeof( szTime), "%d:00", m_GameTimes[idx] );
	}

	if ( !m_pScenarioInfos.IsValidIndex( m_ScenarioIndices[idx] ) )
		return;

	CScenarioInfoPanel *pPanel = m_pScenarioInfos[ m_ScenarioIndices[idx] ];
	pPanel->m_pDescOne->SetText( pState );
	pPanel->m_pDescTwo->SetText( szTime );
}


//-----------------------------------------------------------------
// helper to swap two ints in a utlvector
//-----------------------------------------------------------------
static void Swap( CUtlVector< int > &vec, int iOne, int iTwo )
{
	int temp = vec[iOne];
	vec[iOne] = vec[iTwo];
	vec[iTwo] = temp;
}

//-----------------------------------------------------------------
// Purpose: Swap the order of two menu items
//-----------------------------------------------------------------
void CSessionBrowserDialog::SwapMenuItems( int iOne, int iTwo )
{
	Swap( m_ScenarioIndices, iOne, iTwo );
	Swap( m_SearchIndices, iOne, iTwo );
	Swap( m_GameStates, iOne, iTwo );
	Swap( m_GameTimes, iOne, iTwo );

	// swap the XUIDs, too
	XUID temp = m_XUIDs[iOne];
	m_XUIDs[iOne] = m_XUIDs[iTwo];
	m_XUIDs[iTwo] = temp;
}

//-----------------------------------------------------------------
// Purpose: Handle commands from the dialog menu
//-----------------------------------------------------------------
void CSessionBrowserDialog::OnCommand( const char *pCommand )
{
	if ( !Q_stricmp( pCommand, "SelectSession" ) )
	{
		int idx = m_SearchIndices[ m_Menu.GetActiveItemIndex() ];
		matchmaking->SelectSession( idx );
	}

	m_pParent->OnCommand( pCommand );
}

//-----------------------------------------------------------------
// Purpose: Send key presses to the dialog's menu
//-----------------------------------------------------------------
void CSessionBrowserDialog::OnKeyCodePressed( vgui::KeyCode code )
{
	switch( code )
	{
	case KEY_XBUTTON_B:
		matchmaking->KickPlayerFromSession( 0 );
		break;
	case KEY_XBUTTON_X:
#ifdef _X360
		int idx = m_Menu.GetActiveItemIndex();
		if ( m_XUIDs.IsValidIndex( idx ) )
		{
			XShowGamerCardUI( XBX_GetPrimaryUserId(), m_XUIDs[idx] );
		}
#endif
		break;
	}

	BaseClass::OnKeyCodePressed( code );

	// Selected session may have been updated
	UpdateScenarioDisplay();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSessionBrowserDialog::OnThink()
{
	vgui::KeyCode code = m_KeyRepeat.KeyRepeated();
	if ( code )
	{
		m_Menu.HandleKeyCode( code );
		UpdateScenarioDisplay();
	}

	BaseClass::OnThink();
}
