//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Dialog for setting customizable game options
//
//=============================================================================//

#include "sessionoptionsdialog.h"
#include "engine/imatchmaking.h"
#include "EngineInterface.h"
#include "vgui_controls/ImagePanel.h"
#include "vgui_controls/Label.h"
#include "KeyValues.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//---------------------------------------------------------------------
// CSessionOptionsDialog
//---------------------------------------------------------------------
CSessionOptionsDialog::CSessionOptionsDialog( vgui::Panel *pParent ) : BaseClass( pParent, "SessionOptions" )
{
	SetDeleteSelfOnClose( true );

	m_pDialogKeys = NULL;
	m_bModifySession = false;
}

CSessionOptionsDialog::~CSessionOptionsDialog()
{
	m_pScenarioInfos.PurgeAndDeleteElements();
}

//---------------------------------------------------------------------
// Purpose: Dialog keys contain session contexts and properties
//---------------------------------------------------------------------
void CSessionOptionsDialog::SetDialogKeys( KeyValues *pKeys )
{
	m_pDialogKeys = pKeys;
}


//---------------------------------------------------------------------
// Purpose: Strip off the game type from the resource name
//---------------------------------------------------------------------
void CSessionOptionsDialog::SetGameType( const char *pString )
{
	// Get the full gametype from the resource name
	const char *pGametype = Q_stristr( pString, "_" );
	if ( !pGametype )
		return;

	Q_strncpy( m_szGametype, pGametype + 1, sizeof( m_szGametype ) );

	// set the menu filter
	m_Menu.SetFilter( m_szGametype );
}

//---------------------------------------------------------------------
// Purpose: Center the dialog on the screen
//---------------------------------------------------------------------
void CSessionOptionsDialog::PerformLayout( void )
{
	BaseClass::PerformLayout();

	UpdateScenarioDisplay();

	MoveToCenterOfScreen();

	if ( m_pRecommendedLabel )
	{
		bool bHosting = ( Q_stristr( m_szGametype, "Host" ) );
		m_pRecommendedLabel->SetVisible( bHosting );
	}
}

//---------------------------------------------------------------------
// Purpose: Apply common properties and contexts
//---------------------------------------------------------------------
void CSessionOptionsDialog::ApplyCommonProperties( KeyValues *pKeys )
{
	for ( KeyValues *pProperty = pKeys->GetFirstSubKey(); pProperty != NULL; pProperty = pProperty->GetNextKey() )
	{
		const char *pName = pProperty->GetName();

		if ( !Q_stricmp( pName, "SessionContext" ) )
		{
			// Create a new session context
			sessionProperty_t ctx;
			ctx.nType = SESSION_CONTEXT;
			Q_strncpy( ctx.szID, pProperty->GetString( "id", "NULL" ), sizeof( ctx.szID ) );
			Q_strncpy( ctx.szValue, pProperty->GetString( "value", "NULL" ), sizeof( ctx.szValue ) );
			// ctx.szValueType not used

			m_SessionProperties.AddToTail( ctx );
		}
		else if ( !Q_stricmp( pName, "SessionProperty" ) )
		{
			// Create a new session property
			sessionProperty_t prop;
			prop.nType = SESSION_PROPERTY;
			Q_strncpy( prop.szID, pProperty->GetString( "id", "NULL" ), sizeof( prop.szID ) );
			Q_strncpy( prop.szValue, pProperty->GetString( "value", "NULL" ), sizeof( prop.szValue ) );
			Q_strncpy( prop.szValueType, pProperty->GetString( "valuetype", "NULL" ), sizeof( prop.szValueType ) );

			m_SessionProperties.AddToTail( prop );
		}
		else if ( !Q_stricmp( pName, "SessionFlag" ) )
		{
			sessionProperty_t flag;
			flag.nType = SESSION_FLAG;
			Q_strncpy( flag.szID, pProperty->GetString(), sizeof( flag.szID ) );

			m_SessionProperties.AddToTail( flag );
		}
	}
}

//---------------------------------------------------------------------
// Purpose: Parse session properties and contexts from the resource file
//---------------------------------------------------------------------
void CSessionOptionsDialog::ApplySettings( KeyValues *pResourceData )
{
	BaseClass::ApplySettings( pResourceData );

	// Apply settings common to all game types
	ApplyCommonProperties( pResourceData );

	// Apply settings specific to this game type
	KeyValues *pSettings = pResourceData->FindKey( m_szGametype );
	if ( pSettings )
	{
		Q_strncpy( m_szCommand, pSettings->GetString( "commandstring", "NULL" ), sizeof( m_szCommand ) );
		m_pTitle->SetText( pSettings->GetString( "title", "Unknown" ) );

		ApplyCommonProperties( pSettings );
	}

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

			m_pScenarioInfos.AddToTail( pScenarioInfo );
		}
	}

	if ( Q_stristr( m_szGametype, "Modify" ) )
	{
		m_bModifySession = true;
	}
}

int CSessionOptionsDialog::GetMaxPlayersRecommendedOption( void )
{
	MM_QOS_t qos = matchmaking->GetQosWithLIVE();

	// Conservatively assume that every player needs ~ 7 kBytes/s
	// plus one for the hosting player.
	int numPlayersCanService = 1 + int( qos.flBwUpKbs / 7.0f );

	// Determine the option that suits our B/W bests
	int options[] = { 8, 12, 16 };

	for ( int k = 1; k < ARRAYSIZE( options ); ++ k )
	{
		if ( options[k] > numPlayersCanService )
		{
			Msg( "[SessionOptionsDialog] Defaulting number of players to %d (upstream b/w = %.1f kB/s ~ %d players).\n",
				options[k - 1], qos.flBwUpKbs, numPlayersCanService );
			return k - 1;
		}
	}

	return ARRAYSIZE( options ) - 1;
}

//---------------------------------------------------------------------
// Purpose: Set up colors and other such stuff
//---------------------------------------------------------------------
void CSessionOptionsDialog::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	for ( int i = 0; i < m_pScenarioInfos.Count(); ++i )
	{
		m_pScenarioInfos[i]->SetBgColor( pScheme->GetColor( "TanDark", Color( 0, 0, 0, 255 ) ) );
	}

	m_pRecommendedLabel = dynamic_cast<vgui::Label *>(FindChildByName( "RecommendedLabel" ));
}

//---------------------------------------------------------------------
// Purpose: Send all properties and contexts to the matchmaking session
//---------------------------------------------------------------------
void CSessionOptionsDialog::SetupSession( void )
{
	KeyValues *pKeys = new KeyValues( "SessionKeys" );

	// Send user-selected properties and contexts
	for ( int i = 0; i < m_Menu.GetItemCount(); ++i )
	{
		COptionsItem *pItem = dynamic_cast< COptionsItem* >( m_Menu.GetItem( i ) );
		if ( !pItem )
		{                    
			continue;
		}		

		const sessionProperty_t &prop = pItem->GetActiveOption();

		KeyValues *pProperty = pKeys->CreateNewKey();
		pProperty->SetName( prop.szID );
		pProperty->SetInt( "type", prop.nType );
		pProperty->SetString( "valuestring", prop.szValue );
		pProperty->SetString( "valuetype", prop.szValueType );
		pProperty->SetInt( "optionindex", pItem->GetActiveOptionIndex() );
	}

	// Send contexts and properties parsed from the resource file
	for ( int i = 0; i < m_SessionProperties.Count(); ++i )
	{
		const sessionProperty_t &prop = m_SessionProperties[i];

		KeyValues *pProperty = pKeys->CreateNewKey();
		pProperty->SetName( prop.szID );
		pProperty->SetInt( "type", prop.nType );
		pProperty->SetString( "valuestring", prop.szValue );
		pProperty->SetString( "valuetype", prop.szValueType );
	}

	// Matchmaking will make a copy of these keys
	matchmaking->SetSessionProperties( pKeys );
	pKeys->deleteThis();
}

//-----------------------------------------------------------------
// Purpose: Show the correct scenario image and text
//-----------------------------------------------------------------
void CSessionOptionsDialog::UpdateScenarioDisplay( void )
{
	// Check if the selected map has changed (first menu item)
	int idx = m_Menu.GetActiveOptionIndex( 0 );
	for ( int i = 0; i < m_pScenarioInfos.Count(); ++i )
	{
		m_pScenarioInfos[i]->SetVisible( i == idx );
	}
}

//-----------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------
void CSessionOptionsDialog::OnMenuItemChanged( KeyValues *pData )
{
	// which item changed
	int iItem = pData->GetInt( "item", -1 );

	if ( iItem >= 0 && iItem < m_Menu.GetItemCount() )
	{
		COptionsItem *pActiveOption = dynamic_cast< COptionsItem* >( m_Menu.GetItem( iItem ) );
		if ( pActiveOption )
		{    
			const sessionProperty_t &activeOption = pActiveOption->GetActiveOption();
				
			if ( !Q_strncmp( activeOption.szID, "PROPERTY_GAME_SIZE", sessionProperty_t::MAX_KEY_LEN ) )
			{
				// make sure the private slots is less than prop.szValue

				int iMaxPlayers = atoi(activeOption.szValue);
				bool bShouldWarnMaxPlayers = ( pActiveOption->GetActiveOptionIndex() > GetMaxPlayersRecommendedOption() );
				m_pRecommendedLabel->SetVisible( bShouldWarnMaxPlayers );

				// find the private slots option and repopulate it
				for ( int iMenu = 0; iMenu < m_Menu.GetItemCount(); ++iMenu )
				{
					COptionsItem *pItem = dynamic_cast< COptionsItem* >( m_Menu.GetItem( iMenu ) );
					if ( !pItem )
					{                    
						continue;
					}		

					const sessionProperty_t &prop = pItem->GetActiveOption();

					if ( !Q_strncmp( prop.szID, "PROPERTY_PRIVATE_SLOTS", sessionProperty_t::MAX_KEY_LEN ) )
					{
						const sessionProperty_t baseProp = pItem->GetActiveOption();

						// preserve the selection
						int iActiveItem = pItem->GetActiveOptionIndex();

						// clear all options
						pItem->DeleteAllOptions();

						// re-add the items 0 - maxplayers
						int nStart		= 0;
						int nEnd		= iMaxPlayers;
						int nInterval	= 1;

						for ( int i = nStart; i <= nEnd; i += nInterval )
						{
							sessionProperty_t propNew;
							propNew.nType = SESSION_PROPERTY;
							Q_strncpy( propNew.szID, baseProp.szID, sizeof( propNew.szID ) );
							Q_strncpy( propNew.szValueType, baseProp.szValueType, sizeof( propNew.szValueType ) );
							Q_snprintf( propNew.szValue, sizeof( propNew.szValue), "%d", i );
							pItem->AddOption( propNew.szValue, propNew );
						}

						// re-set the focus
						pItem->SetOptionFocus( min( iActiveItem, iMaxPlayers ) );

						// fixup the option sizes
						m_Menu.InvalidateLayout();
					}
				}
			}
		}
	}
}

//-----------------------------------------------------------------
// Purpose: Change properties of a menu item
//-----------------------------------------------------------------
void CSessionOptionsDialog::OverrideMenuItem( KeyValues *pItemKeys )
{
	if ( m_bModifySession && m_pDialogKeys )
	{
		if ( !Q_stricmp( pItemKeys->GetName(), "OptionsItem" ) )
		{
			const char *pID	= pItemKeys->GetString( "id", "NULL" );

			KeyValues *pKey = m_pDialogKeys->FindKey( pID );
			if ( pKey )
			{
				pItemKeys->SetInt( "activeoption", pKey->GetInt( "optionindex" ) );	
			}
		}
	}

	//
	// When hosting a new session on LIVE:
	//	- restrict max number of players to bandwidth allowed
	//
	if ( !m_bModifySession &&
		( !Q_stricmp( m_szGametype, "hoststandard" ) || !Q_stricmp( m_szGametype, "hostranked" ) )
		)
	{
		if ( !Q_stricmp( pItemKeys->GetName(), "OptionsItem" ) )
		{
			const char *pID	= pItemKeys->GetString( "id", "NULL" );
			if ( !Q_stricmp( pID, "PROPERTY_GAME_SIZE" ) )
			{
				pItemKeys->SetInt( "activeoption", GetMaxPlayersRecommendedOption() );
			}
		}
	}
}

//-----------------------------------------------------------------
// Purpose: Send key presses to the dialog's menu
//-----------------------------------------------------------------
void CSessionOptionsDialog::OnKeyCodePressed( vgui::KeyCode code )
{
	if ( code == KEY_XBUTTON_A || code == STEAMCONTROLLER_A )
	{
		OnCommand( m_szCommand );
	}
	else if ( (code == KEY_XBUTTON_B || code == STEAMCONTROLLER_B) && m_bModifySession )
	{
		// Return to the session lobby without making any changes
		OnCommand( "DialogClosing" );
	}
	else
	{
		BaseClass::OnKeyCodePressed( code );
	}

	UpdateScenarioDisplay();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSessionOptionsDialog::OnThink()
{
	vgui::KeyCode code = m_KeyRepeat.KeyRepeated();
	if ( code )
	{
		m_Menu.HandleKeyCode( code );
		UpdateScenarioDisplay();
	}

	BaseClass::OnThink();
}

//---------------------------------------------------------------------
// Purpose: Handle menu commands
//---------------------------------------------------------------------
void CSessionOptionsDialog::OnCommand( const char *pCommand )
{
	// Don't set up the session if the dialog is just closing
	if ( Q_stricmp( pCommand, "DialogClosing" ) )
	{
		SetupSession();
		OnClose();
	}
		
	GetParent()->OnCommand( pCommand );
}

