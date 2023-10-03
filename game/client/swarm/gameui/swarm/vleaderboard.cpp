//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "vleaderboard.h"
#include "VGenericPanelList.h"
#include "EngineInterface.h"
#include "VFooterPanel.h"
#include "UIGameData.h"
#include "gameui_util.h"

#include "vgui/ISurface.h"
#include "vgui/IBorder.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/Button.h"
#include "vgui_controls/ImagePanel.h"
#include "vgui/ILocalize.h"
#include "vstdlib/random.h"	// remove once we have real data in the leaderboard
#include "VFlyoutMenu.h"
#include "VHybridButton.h"
#include "VDropDownMenu.h"
#include "VGameSettings.h"
#include "FileSystem.h"

#include "fmtstr.h"
#include "checksum_crc.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


using namespace vgui;
using namespace BaseModUI;

static bool IsUserLIVEEnabled( int nUserID )
{
#ifdef _X360
	return ( eXUserSigninState_SignedInToLive == XUserGetSigninState( nUserID ) );
#endif // _X360

	return false;
}

static bool AnyUserConnectedToLIVE()
{
#ifdef _X360
	for ( uint idx = 0; idx < XBX_GetNumGameUsers(); ++ idx )
	{
		if ( IsUserLIVEEnabled( XBX_GetUserId( idx ) ) )
			return true;
	}
#endif

	return false;
}

enum 
{
	HOLDOUT_MEDAL_BRONZE = 0,
	HOLDOUT_MEDAL_SILVER,
	HOLDOUT_MEDAL_GOLD,
	HOLDOUT_MEDAL_PLATINUM,

	NUM_HOLDOUT_MEDALS
} HoldoutMedal;

#define HOLDOUT_MEDAL_TIME_BRONZE	240
#define HOLDOUT_MEDAL_TIME_SILVER	420
#define HOLDOUT_MEDAL_TIME_GOLD		600

KeyValues *g_pPreloadedListItemItemLayout = NULL;

//=============================================================================
//
//=============================================================================
LeaderboardListItem::LeaderboardListItem( vgui::Panel *parent, const char *panelName ):
	BaseClass( parent, panelName )
{
	SetScheme( CBaseModPanel::GetSingleton().GetScheme() );

	SetProportional( true );

	SetPaintBackgroundEnabled( true );

	m_bSelected = false;
#if !defined( _X360 )
	m_bHasMouseover = false;
#endif

	memset( &m_data, 0, sizeof( m_data ) );

	CBaseModFrame::AddFrameListener( this );
}

//=============================================================================
LeaderboardListItem::~LeaderboardListItem()
{
	CBaseModFrame::RemoveFrameListener( this );

	if ( g_pPreloadedListItemItemLayout )
	{
		g_pPreloadedListItemItemLayout->deleteThis();
		g_pPreloadedListItemItemLayout = NULL;
	}
}

//=============================================================================
void LeaderboardListItem::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	if ( !g_pPreloadedListItemItemLayout )
	{
		const char *pszResource = "Resource/UI/BaseModUI/LeaderboardListItem.res";
		g_pPreloadedListItemItemLayout = new KeyValues( pszResource );
		g_pPreloadedListItemItemLayout->LoadFromFile(g_pFullFileSystem, pszResource);
	}

	LoadControlSettings( "", NULL, g_pPreloadedListItemItemLayout );
}

//=============================================================================
void LeaderboardListItem::SetRankLabelText()
{
	Label *pRank = dynamic_cast< Label * >( FindChildByName( "LblRank" ) );
	if ( pRank )
	{
		pRank->SetVisible( m_data.m_bShowRank );

		if ( m_data.m_iDisplayRank == 0 )
		{
			pRank->SetText( "" );
		}
		else if ( m_data.m_iDisplayRank < 1000 )
		{
			pRank->SetText( VarArgs( "%d", m_data.m_iDisplayRank ) );
		}
		else if ( m_data.m_iDisplayRank < 1000000 )
		{
			// 4 - 6 digits, one comma
			int iLower = m_data.m_iDisplayRank % 1000;
			int iUpper = m_data.m_iDisplayRank / 1000;

			pRank->SetText( VarArgs( "%d,%03d", iUpper, iLower ) );
		}
		else
		{
			// 7+ digits, 2 commas
			int iLower = m_data.m_iDisplayRank % 1000;
			int iMiddle = ( m_data.m_iDisplayRank / 1000 ) % 1000;
			int iUpper = m_data.m_iDisplayRank / 1000000;

			pRank->SetText( VarArgs( "%d,%03d,%03d", iUpper, iMiddle, iLower ) );
		}
	}
}

//=============================================================================
void LeaderboardListItem::PerformLayout()
{
	SetRankLabelText();

#if !defined( _X360 )

	DropDownMenu *pDrpPlayer = dynamic_cast< DropDownMenu * > ( FindChildByName( "DrpPlayerName" ) );
	if ( pDrpPlayer )
	{
		BaseModUI::BaseModHybridButton *pBtnPlayerName = dynamic_cast< BaseModUI::BaseModHybridButton * > ( pDrpPlayer->FindChildByName( "BtnPlayerName" ) );
		if ( pBtnPlayerName )
		{
			pBtnPlayerName->SetText( GetPlayerName() );

			FlyoutMenu *flyout = dynamic_cast< FlyoutMenu * >( FindChildByName( "FlmPlayerFlyout" ) );
			if ( flyout && flyout->IsVisible() )
			{
				flyout->CloseMenu( pBtnPlayerName );
			}

			pBtnPlayerName->SetShowDropDownIndicator( true );
		}
	}

	// set all our children (image panel and labels) to not accept mouse input so they
	// don't eat any mouse input and it all goes to us
	for ( int i = 0; i < GetChildCount(); i++ )
	{
		Panel *panel = GetChild( i );
		Assert( panel );

		if ( panel == pDrpPlayer )
			continue;

		panel->SetMouseInputEnabled( false );
	}

#endif

#if defined( _X360 )

	Label *pGamerTag = dynamic_cast< Label * >( FindChildByName( "LblGamerTag" ) );
	if ( pGamerTag )
	{
		pGamerTag->SetText( GetPlayerName() );
	}

	ImagePanel *pLocalPlayerIcon = dynamic_cast< ImagePanel * >( FindChildByName( "ImgLocalPlayer" ) );
	if ( pLocalPlayerIcon )
	{
		pLocalPlayerIcon->SetVisible( m_data.m_bLocalPlayer );

		if ( m_data.m_bLocalPlayer )
		{
			switch( m_data.m_iControllerIndex )
			{
			case 0:
				pLocalPlayerIcon->SetImage( "hud/360_controller_0" );
				break;
			case 1:
				pLocalPlayerIcon->SetImage( "hud/360_controller_1" );
				break;
			case 2:
				pLocalPlayerIcon->SetImage( "hud/360_controller_2" );
				break;
			case 3:
				pLocalPlayerIcon->SetImage( "hud/360_controller_3" );
				break;

			default:
				break;
			}
		}
	}
#endif

#if !defined( _X360 )
	vgui::ImagePanel *pGamerPic = dynamic_cast< vgui::ImagePanel* >( FindChildByName( "PnlGamerPic" ) );																						   
	if ( pGamerPic )
	{
		IImage *pImage = CUIGameData::Get()->GetAvatarImage( m_data.m_steamIDUser.ConvertToUint64() );
		if ( pImage )
		{
			pGamerPic->SetImage( pImage );
		}

		pGamerPic->SetVisible( pImage != NULL );
	}
#endif

	ImagePanel *pAward = dynamic_cast< ImagePanel * >( FindChildByName( "ImgAward" ) );
	if ( pAward )
	{
		pAward->SetVisible( true );

		switch( m_data.m_iAwards )
		{
		case HOLDOUT_MEDAL_PLATINUM:
			pAward->SetImage( "hud/survival_medal_platinum" );
			break;
		case HOLDOUT_MEDAL_GOLD:
			pAward->SetImage( "hud/survival_medal_gold" );
			break;
		case HOLDOUT_MEDAL_SILVER:
			pAward->SetImage( "hud/survival_medal_silver" );
			break;
		case HOLDOUT_MEDAL_BRONZE:
			pAward->SetImage("hud/survival_medal_bronze" );
			break;

		default:
			pAward->SetVisible( false );
			break;
		}
	}

	Label *pTime = dynamic_cast< Label * >( FindChildByName( "LblTime" ) );
	if ( pTime )
	{
		if ( m_data.m_iTimeInHundredths <= 0 )
		{
			pTime->SetText( "-:--.--" );
		}
		else
		{
			int iTimeInSeconds  = m_data.m_iTimeInHundredths / 100;

			int iMinutes = iTimeInSeconds / 60;
			int iSeconds = iTimeInSeconds % 60;
			int iHundredths = m_data.m_iTimeInHundredths % 100;

			pTime->SetText( VarArgs( "%01d:%02d.%02d", iMinutes, iSeconds, iHundredths ) );
		}
	}

	BaseClass::PerformLayout();
}

//=============================================================================
void LeaderboardListItem::OnCommand( const char *command )
{
#ifndef _X360
	if ( !Q_strcmp( command, "PlayerDropDown" ) )
	{
		// send a message to Leaderboard with the steamID of the player to show
		KeyValues *pKeys = new KeyValues( "PlayerDropDown" );
		if ( pKeys )
		{
			pKeys->SetUint64( "playerID", m_data.m_steamIDUser.ConvertToUint64() );


			DropDownMenu *pDrpPlayer = dynamic_cast< DropDownMenu * > ( FindChildByName( "DrpPlayerName" ) );
			if ( pDrpPlayer )
			{
				BaseModHybridButton *pBtnPlayerName = dynamic_cast< BaseModHybridButton * > ( pDrpPlayer->FindChildByName( "BtnPlayerName" ) );
				if ( pBtnPlayerName )
				{
					pKeys->SetPtr( "pButton", pBtnPlayerName );

					int x, y;
					pBtnPlayerName->GetPos( x, y );
					pBtnPlayerName->LocalToScreen( x, y );

					y += pBtnPlayerName->GetTall();

					pKeys->SetInt( "xpos", x );
					pKeys->SetInt( "ypos", y );
				}
			}

			PostActionSignal( pKeys );
		}
	}	
	else if ( !Q_strcmp( command, "#L4D360UI_SendMessage" ) )
	{
		CBaseModPanel::GetSingleton().PlayUISound( UISOUND_ACCEPT );
		char steamCmd[64];
		Q_snprintf( steamCmd, sizeof( steamCmd ), "chat/%I64u", m_data.m_steamIDUser.ConvertToUint64() );
		CUIGameData::Get()->ExecuteOverlayCommand( steamCmd );
	}
	else if ( !Q_strcmp( command, "#L4D360UI_ViewSteamID" ) )
	{
		CBaseModPanel::GetSingleton().PlayUISound( UISOUND_ACCEPT );
		char steamCmd[64];
		Q_snprintf( steamCmd, sizeof( steamCmd ), "steamid/%I64u", m_data.m_steamIDUser.ConvertToUint64() );
		CUIGameData::Get()->ExecuteOverlayCommand( steamCmd );
	}
	else 
#endif
	{
		BaseClass::OnCommand( command );
	}
}

//=============================================================================
void LeaderboardListItem::SetData( LeaderboardItem_t data )
{
	m_data = data;
}

//=============================================================================
LeaderboardItem_t *LeaderboardListItem::GetDataForModify( void )
{
	return &m_data;
}

//=============================================================================
int LeaderboardListItem::GetRank( void )
{
	return m_data.m_iRank;
}

//=============================================================================
const char *LeaderboardListItem::GetPlayerName( void )
{
#if defined( _X360 )
	return m_data.m_szGamerTag;
#else
	return m_data.m_szName;
#endif
}

//=============================================================================
void LeaderboardListItem::SetShowRank( bool bShowRank, int iRankToDisplay )
{
	m_data.m_bShowRank = bShowRank;
	m_data.m_iDisplayRank = iRankToDisplay;

	if ( bShowRank != m_data.m_bShowRank )
	{
		m_data.m_bShowRank = bShowRank;

		SetRankLabelText();
	}
}

//=============================================================================
void LeaderboardListItem::SetSelected( bool bSelected )
{
	m_bSelected = bSelected;
	Repaint();
}

//=============================================================================
void LeaderboardListItem::PaintBackground()
{
// 	if ( GenericPanelList *pGpl = dynamic_cast< GenericPanelList * >( GetParent() ? GetParent()->GetParent() : NULL ) )
// 	{
// 		SetAlpha( pGpl->IsPanelItemVisible( this, true ) ? 255 : 0 );
// 	}
// 
	// if we're hilighted, background
	if ( IsSelected() )
	{
		int y;
		int x;
		GetPos( x, y );
		int tall = GetTall() * 0.9;
		y = ( GetTall() - tall ) / 2;
		int wide = GetWide();

		y = 0;
		tall = GetTall();

		vgui::IScheme *pScheme = vgui::scheme()->GetIScheme( GetScheme() );
		Color blotchColor = pScheme->GetColor( "HybridButton.BlotchColor", Color( 0, 0, 255, 128 ) );
		Color borderColor = pScheme->GetColor( "HybridButton.BorderColor", Color( 0, 0, 255, 255 ) );

		// draw border lines
		surface()->DrawSetColor( borderColor );
		surface()->DrawFilledRectFade( x, y, x + 0.5f * wide, y+2, 0, 255, true );
		surface()->DrawFilledRectFade( x + 0.5f * wide, y, x + wide, y+2, 255, 0, true );
		surface()->DrawFilledRectFade( x, y+tall-2, x + 0.5f * wide, y+tall, 0, 255, true );
		surface()->DrawFilledRectFade( x + 0.5f * wide, y+tall-2, x + wide, y+tall, 255, 0, true );

		surface()->DrawSetColor( blotchColor );
		int blotchWide = GetWide();
		int blotchX = 0;
		surface()->DrawFilledRectFade( blotchX, y, blotchX + 0.25f * blotchWide, y+tall, 0, 150, true );
		surface()->DrawFilledRectFade( blotchX + 0.25f * blotchWide, y, blotchX + blotchWide, y+tall, 150, 0, true );
	}
}

//=============================================================================
void LeaderboardListItem::OnKeyCodePressed( KeyCode code )
{
	BaseClass::OnKeyCodePressed( code );
}

#ifdef _X360
//=============================================================================
void LeaderboardListItem::NavigateFrom()
{
	BaseClass::NavigateFrom();

	OnClose();	//?

	SetSelected( false );
}

//=============================================================================
void LeaderboardListItem::NavigateTo()
{
	BaseClass::NavigateFrom();

	SetSelected( true );
}
#endif

#if !defined( _X360 )

//=============================================================================
void LeaderboardListItem::NavigateTo( void )
{
	//SetHasMouseover( true );
	//RequestFocus();
	BaseClass::NavigateTo();
}

//=============================================================================
void LeaderboardListItem::NavigateFrom( void )
{
	//SetHasMouseover( false );
	BaseClass::NavigateFrom();
}

//=============================================================================
void LeaderboardListItem::OnCursorEntered() 
{ 
// 	if( GetParent() )
// 	{
// 		GetParent()->NavigateToChild( this );
// 	}
// 	else
// 	{
// 		NavigateTo();
// 	}

	BaseClass::OnCursorEntered();
}

#endif	// !_X360 


//=============================================================================
//
//=============================================================================

#define LEADERBOARD_PAGE_SIZE	100

//=============================================================================
Leaderboard::Leaderboard( Panel *parent ):
	BaseClass( parent, "Leaderboard", true, true, false ),
	m_pDataSettings( NULL )
{
	SetProportional( true );
	SetPaintBackgroundEnabled( true );

	m_pPanelList = new GenericPanelList( this, "PanelList", GenericPanelList::ISM_PERITEM | GenericPanelList::ISM_ALPHA_INVISIBLE );
	m_pPanelList->SetPaintBackgroundEnabled( false );

	m_pPanelList->AddActionSignalTarget( this );

	for ( int i=0; i<LEADERBOARD_PAGE_SIZE*3; i++ )
	{
		LeaderboardListItem *pItem = new LeaderboardListItem( NULL, "LeaderboardListItem" );

		if ( pItem )
		{
			pItem->MakeReadyForUse();
			pItem->AddActionSignalTarget( this );
		}

		ReturnListItemToPool( pItem );
	}

	m_ActiveControl = NULL;	// m_pPanelList;

	SetLowerGarnishEnabled( true );

	SetDeleteSelfOnClose( true );

#if defined( _X360 )
	m_hOverlapped = NULL;
	memset( &m_xOverlapped, 0, sizeof( m_xOverlapped ) );

	// initialize the spec
	ZeroMemory( &m_spec, sizeof(m_spec) );
	ZeroMemory( m_szLeaderboardViewNameSpec, sizeof( m_szLeaderboardViewNameSpec ) );
#endif

	ClearAsyncData();

	m_iPrevSelectedRank = -1;
	m_iMinPageLoaded = -1;
	m_iMaxPageLoaded = -1;
	m_iSelectRankWhenLoaded = 0;

	m_bIgnoreSelectionChanges = false;

#if defined( _X360 )
	m_hOverlapped = NULL;
	memset( &m_xOverlapped, 0, sizeof( m_xOverlapped ) );
#else
	SetDefLessFunc( m_LeaderboardHandles );

	GameUI().PreventEngineHideGameUI();
#endif
}

//=============================================================================
Leaderboard::~Leaderboard()
{
	CancelCurrentSearch();

	while( m_ListItemPool.Count() > 0 )
	{
		LeaderboardListItem *pItem = m_ListItemPool.Top();
		pItem->DeletePanel();
		m_ListItemPool.Pop();
	}

#if !defined( _X360 )
	GameUI().AllowEngineHideGameUI();
#endif

	if ( m_pDataSettings )
		m_pDataSettings->deleteThis();
	m_pDataSettings = NULL;
}

//=============================================================================
LeaderboardListItem *Leaderboard::GetListItemFromPool( void )
{
	if ( m_ListItemPool.Count() <= 0 )
		return NULL;

	LeaderboardListItem *pItem = m_ListItemPool.Top();

	m_ListItemPool.Pop();

	//DevMsg( 1, "POP: list item pool %d\n", m_ListItemPool.Count() );

	pItem->SetVisible( true );

	return pItem;
}

//=============================================================================
void Leaderboard::ReturnListItemToPool( LeaderboardListItem *pItem )
{
	if ( !pItem )
		return;

	pItem->SetParent( (vgui::VPANEL)0 );
	pItem->SetVisible( false );
	pItem->SetSelected( false );

	m_ListItemPool.Push( pItem );

	//DevMsg( 1, "PUSH: list item pool %d\n", m_ListItemPool.Count() );
}


//=============================================================================
void Leaderboard::Activate()
{
	BaseClass::Activate();

	// clear out the panel list
	ReturnAllListItemsToPool();

	m_pPanelList->SetScrollBarVisible( IsPC() );
	m_pPanelList->NavigateTo();

	AddFrameListener( this );

#if defined( _X360 )
	UpdateFooter();

	if ( CBaseModFooterPanel *footer = BaseModUI::CBaseModPanel::GetSingleton().GetFooterPanel() )
		footer->SetButtonText( FB_XBUTTON, "#L4D360UI_Leaderboard_Show_Global" );


	m_localPlayerXUIDs.RemoveAll();

	// store the xuids of the local players
	for ( uint idx = 0; idx < XBX_GetNumGameUsers(); ++ idx )
	{
		int iUserId = XBX_GetUserId( idx );

		// omit guests
		if ( XBX_GetUserIsGuest( iUserId ) )
			continue;

		// omit local profiles and silver accounts
		IPlayerLocal *player = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( iUserId );
		if ( !player )
			continue;

		if ( player->GetOnlineState() == IPlayer::STATE_OFFLINE )
			continue;

		XUID xuid = 0ull;
		XUserGetXUID( iUserId, &xuid );

		m_localPlayerXUIDs.AddToTail( xuid );
	}

#endif

	m_Mode = LEADERBOARD_FRIENDS;

	KeyValues *pInfoMission = NULL;
	KeyValues *pInfoMap = g_pMatchExtSwarm->GetMapInfo( m_pDataSettings, &pInfoMission );
	
	char const *szGameMode = m_pDataSettings->GetString( "game/mode", "survival" );
	KeyValues *pAllMissions = g_pMatchExtSwarm->GetAllMissions();
	pInfoMission = pAllMissions ? pAllMissions->GetFirstTrueSubKey() : NULL;

	while ( !pInfoMap && pInfoMission )
	{
		pInfoMap = pInfoMission->FindKey( CFmtStr( "modes/%s/1", szGameMode ) );
		if ( pInfoMap )
		{
			if ( !m_pDataSettings )
			{
				m_pDataSettings = new KeyValues( "settings" );
				m_pDataSettings->SetString( "game/mode", szGameMode );
			}
			
			m_pDataSettings->SetString( "game/campaign", pInfoMission->GetString( "name" ) );
			m_pDataSettings->SetInt( "game/chapter", pInfoMap->GetInt( "chapter" ) );
			
			break;
		}
		else
			pInfoMission = pInfoMission->GetNextTrueSubKey();
	}

	Assert( pInfoMap && pInfoMission );
	if ( !pInfoMap )
	{
		Warning( "No maps defined for '%s'\n", szGameMode );
		return;
	}

#ifndef _X360
	m_bIsSearching = false;

	m_iCurrentSpinnerValue = 0;
	m_flLastEngineSpinnerTime = 0.f;
#endif

	OnMissionChapterChanged();

	// If we're connected to a server, don't show "Play this Chapter"
	if ( engine->IsConnected() )
	{
		SetControlEnabled( "BtnFindServer", false );
	}
}

//=============================================================================
void Leaderboard::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	SetPaintBackgroundEnabled( true );
	SetupAsDialogStyle();

	Activate();
}

//=============================================================================
int Leaderboard::GetCurrentChapterContext( void )
{
	KeyValues *pMissionInfo = NULL;
	if ( KeyValues *pMapInfo = g_pMatchExtSwarm->GetMapInfo( m_pDataSettings, &pMissionInfo ) )
	{
		if ( IsX360() )
		{
			return pMapInfo->GetInt( "x360ctx" );
		}
		else
		{
			CFmtStr chapterCtx( "%s_%d", pMissionInfo->GetString( "name" ), pMapInfo->GetInt( "chapter" ) );
			char *szStringVal = chapterCtx.Access();
			V_strlower( szStringVal );
			return CRC32_ProcessSingleBuffer( szStringVal, strlen( szStringVal ) );
		}
	}
	return 0;
}


//=============================================================================
char *GetTimeString( int iTimeInSeconds )
{
	static char szTime[20];

	int iMinutes = iTimeInSeconds / 60;
	int iSeconds = iTimeInSeconds % 60;

	Q_snprintf( szTime, sizeof(szTime), "%01d:%02d", iMinutes, iSeconds );

	return szTime;
}

//=============================================================================
void Leaderboard::MsgOnCustomCampaignSelected( int chapter, const char *campaign )
{
	// Leaderboard always goes to 1st chapter
	chapter = 1;

	// Update the session settings
	m_pDataSettings->SetString( "game/campaign", campaign );
	if ( chapter > 0 )
	{
		m_pDataSettings->SetInt( "game/chapter", chapter );
	}

	// Trigger UI
	OnMissionChapterChanged();
}

void Leaderboard::SetDataSettings( KeyValues *pSettings )
{
	if ( m_pDataSettings )
		m_pDataSettings->deleteThis();
	m_pDataSettings = pSettings ? pSettings->MakeCopy() : NULL;
}

//=============================================================================
void Leaderboard::OnMissionChapterChanged()
{
	static bool s_bNoReentry = false;
	if ( s_bNoReentry )
		return;
	s_bNoReentry = true;

#if !defined( _X360 )
	ClosePlayerFlyout();
#endif

	InitializeDropDownControls();

	KeyValues *pInfoMission = NULL;
	KeyValues *pInfoMap = g_pMatchExtSwarm->GetMapInfo( m_pDataSettings, &pInfoMission );

	const char *pszChapter = pInfoMap ? pInfoMap->GetString( "displayname" ) : NULL;

	if ( pInfoMap )
	{
		DropDownMenu *pMissionDropDown = dynamic_cast< DropDownMenu* >( FindChildByName( "DrpMission" ) );
		if ( pMissionDropDown )
		{
			if ( !pMissionDropDown->GetCurrentSelection() ||
				 Q_stricmp( pMissionDropDown->GetCurrentSelection(), pInfoMission->GetString( "displaytitle" ) ) != 0 )
			{
				pMissionDropDown->SetCurrentSelection( pInfoMission->GetString( "displaytitle" ) );
			}
		}
	}

	// change the map label
	SetControlString( "LblMapName", pszChapter );

	// change the map image
	vgui::ImagePanel *pLevelImage = dynamic_cast< vgui::ImagePanel * > ( FindChildByName( "ImgLevelImage" ) );
	if ( pLevelImage )
	{
		pLevelImage->SetImage( pInfoMap ? pInfoMap->GetString( "image" ) : "maps/unknown" );
	}

	// update the standard times
	Label *pBronzeTime = dynamic_cast< Label * >( FindChildByName( "LblBronzeMedalTime" ) );
	if ( pBronzeTime )
	{
		pBronzeTime->SetText( GetTimeString( HOLDOUT_MEDAL_TIME_BRONZE ) );
	}

	Label *pSilverTime = dynamic_cast< Label * >( FindChildByName( "LblSilverMedalTime" ) );
	if ( pSilverTime )
	{
		pSilverTime->SetText( GetTimeString( HOLDOUT_MEDAL_TIME_SILVER ) );
	}

	Label *pGoldTime = dynamic_cast< Label * >( FindChildByName( "LblGoldMedalTime" ) );
	if ( pGoldTime )
	{
		pGoldTime->SetText( GetTimeString( HOLDOUT_MEDAL_TIME_GOLD ) );
	}

#if defined( _X360 )
	// initialize the spec
	ZeroMemory( &m_spec, sizeof(m_spec) );

	// The rating is the same as the time, so we don't need to retrieve any other columns
	m_spec.dwNumColumnIds = 0;
	m_spec.dwViewId = GetCurrentLeaderboardView();
#endif
	
	// reload the leaderboard with the new leaderboard view
	StartSearching( false );

	s_bNoReentry = false;
}

void Leaderboard::InitializeDropDownControls()
{

}


//=============================================================================
int Leaderboard::GetCurrentLeaderboardView( void )
{
#if defined( _X360 )
	KeyValues *pInfoMission = NULL;
	KeyValues *pInfoMap = g_pMatchExtSwarm->GetMapInfo( m_pDataSettings, &pInfoMission );
	if ( !pInfoMap )
	{
		Assert( 0 );
		m_szLeaderboardViewNameSpec[0] = 0;
		return 0;
	}

	Q_snprintf( m_szLeaderboardViewNameSpec, sizeof( m_szLeaderboardViewNameSpec ),
		"%s_%s", m_pDataSettings->GetString( "game/mode", "survival" ), pInfoMap->GetString( "map" ) );
	return pInfoMap->GetInt( "x360leaderboard/:id" );
#endif

	return 0;
}

//=============================================================================
int Leaderboard::GetPageFromRank( int rank )
{
	// change to 0 base, so page 0 is 0-(LEADERBOARD_PAGE_SIZE-1), but rank 1 - LEADERBOARD_PAGE_SIZE
	int page = ( rank - 1 ) / LEADERBOARD_PAGE_SIZE;

	return page;
}

//=============================================================================
void Leaderboard::OnItemSelected( const char *pszIgnored )
{
	if ( m_bIgnoreSelectionChanges )
		return;

	if ( m_Mode == LEADERBOARD_GLOBAL )
	{
		LeaderboardListItem *pSelectedItem = (LeaderboardListItem *)m_pPanelList->GetSelectedPanelItem();
		if ( !pSelectedItem )
			return;
	
		int iRank = pSelectedItem->GetRank();

		// figure out if we should start loading pages in the background
		int iPreviousPage = GetPageFromRank( m_iPrevSelectedRank );
		int iCurrentPage = GetPageFromRank( iRank );

		if ( iPreviousPage != iCurrentPage )
		{
			// we crossed a page boundary, start loading the next page in that direction
			// when it finishes loading async, we'll throw out the previous page

			int iLoadPage = ( iPreviousPage < iCurrentPage ) ? iCurrentPage+1 : iCurrentPage-1;

			if ( iLoadPage < m_iMinPageLoaded || iLoadPage > m_iMaxPageLoaded )
			{
				CancelCurrentSearch();

				// load iLoadPage
				PendingAsyncRequest_t request;

				request.asyncQueryType = QUERY_STATS_GLOBAL_PAGE;
				request.iData = iLoadPage;

				m_AsyncQueryQueue.Insert( request );

				SendNextRequestInQueue();
			}
		}	

		m_iPrevSelectedRank = iRank;
	}
}

//=============================================================================
#if !defined( _X360 )
void Leaderboard::OnPlayerDropDown( KeyValues *pKeyValues )
{
	uint64 playerID = pKeyValues->GetUint64( "playerID", 0 );

	if ( playerID == 0 )
	{
		return;
	}

	int xpos = pKeyValues->GetInt( "xpos" );
	int ypos = pKeyValues->GetInt( "ypos" );

	BaseModHybridButton *pButton = (BaseModHybridButton *)pKeyValues->GetPtr( "pButton" );

	OpenPlayerFlyout( pButton, playerID, xpos, ypos );
}
#endif

//=============================================================================
void Leaderboard::ReturnAllListItemsToPool( void )
{
	int iCount = m_pPanelList->GetPanelItemCount();
	for ( int i=iCount-1; i>=0; i-- )
	{
		LeaderboardListItem *pItem = (LeaderboardListItem *)m_pPanelList->GetPanelItem(i);
		if ( pItem )
		{
			m_pPanelList->RemovePanelItem( i, false );
			ReturnListItemToPool( pItem );
		}
	}
}

//=============================================================================
bool Leaderboard::StartSearching( bool bCenterOnLocalPlayer )
{
	m_bIgnoreSelectionChanges = true;

	CancelCurrentSearch();

	// clear out the panel list
	ReturnAllListItemsToPool();

	m_bIgnoreSelectionChanges = false;

	m_iPrevSelectedRank = -1;
	m_iMinPageLoaded = -1;
	m_iMaxPageLoaded = -1;

#if defined( _X360 )

	if ( m_Mode == LEADERBOARD_FRIENDS )
	{
		// show the friends of the player who pushed the last button
		int iUserSlot = CBaseModPanel::GetSingleton().GetLastActiveUserId();
		int iController = XBX_GetUserId( iUserSlot );

		PendingAsyncRequest_t request;

		request.asyncQueryType = QUERY_ENUM_FRIENDS;
		request.iData = iController;
		m_AsyncQueryQueue.Insert( request );

		// this will be followed by queries of the friends' stats, and the rank 1 time.
	}	
	else if ( m_Mode == LEADERBOARD_GLOBAL )
	{
		// See if we want to load stats around the local player
		if ( bCenterOnLocalPlayer )	
		{	
			// show the stats of the player who pushed the last button
			int iUserSlot = CBaseModPanel::GetSingleton().GetLastActiveUserId();
			int iController = XBX_GetUserId( iUserSlot );

			PendingAsyncRequest_t request;

			// Get the player's rank
			request.asyncQueryType = QUERY_PLAYER_RANK;
			request.iData = iController;
			m_AsyncQueryQueue.Insert( request );			
		}
		else
		{
			m_iSelectRankWhenLoaded = 1;

			PendingAsyncRequest_t request;

			// Get the player's rank
			request.asyncQueryType = QUERY_STATS_GLOBAL_PAGE;
			request.iData = 0;
			m_AsyncQueryQueue.Insert( request );

			request.iData = 1;
			m_AsyncQueryQueue.Insert( request );

			request.iData = 2;
			m_AsyncQueryQueue.Insert( request );
		}	
	}


#else
	// STEAM Leaderboard

	m_bIsSearching = true;

	PendingAsyncRequest_t request;

	// Find the leaderboard handle
	request.asyncQueryType = QUERY_FIND_LEADERBOARD;
	request.iData = 0;
	m_AsyncQueryQueue.Insert( request );

	// Enum friends' stats on this board
	request.asyncQueryType = QUERY_STATS_FRIENDS;
	request.iData = 0;
	m_AsyncQueryQueue.Insert( request );

#endif 

	SendNextRequestInQueue();

	return true;
}

void Leaderboard::SendNextRequestInQueue( void )
{
	if ( m_AsyncQueryQueue.Count() == 0 )
	{
		// nothing left to load
		return;
	}

	Assert( !IsAQueryPending() );

#ifdef _X360
	Assert( !m_hOverlapped );
	Assert( !m_bufAsyncReadResults.Size() );

	if ( !AnyUserConnectedToLIVE() )
	{
		ClearAsyncData();
		return;
	}
#endif

	m_pendingRequest = m_AsyncQueryQueue.RemoveAtHead();
	m_iAsyncQueryMapContext = GetCurrentChapterContext();

	bool bSuccess = false;

	switch( m_pendingRequest.asyncQueryType )
	{
	case QUERY_ENUM_FRIENDS:
		bSuccess = SendQuery_EnumFriends();
		break;

	case QUERY_STATS_FRIENDS:
		bSuccess = SendQuery_StatsFriends();
		break;

	case QUERY_PLAYER_RANK:
		bSuccess = SendQuery_PlayerRank();
		break;

	case QUERY_RANK_ONE_STATS:
		bSuccess = SendQuery_RankOneStats();
		break;

	case QUERY_STATS_GLOBAL_PAGE:
		bSuccess = SendQuery_StatsGlobalPage();
		break;

#if !defined( _X360 )

	case QUERY_FIND_LEADERBOARD:
		bSuccess = SendQuery_FindLeaderboard();
		break;

#endif // !_X360

	default:
		Assert( 0 );
		break;
	}	

	if ( !bSuccess )
	{
		m_pendingRequest.Init();
		m_iAsyncQueryMapContext = -1;
	}
}


//=============================================================================
void Leaderboard::OnThink()
{
	BaseClass::OnThink();

	UpdateFooter();

	if ( IsAQueryPending() )
	{
#if defined( _X360 )
		if ( XHasOverlappedIoCompleted( &m_xOverlapped ) )
		{
			AsyncLoadCompleted();
		}
#endif
	}
	else
	{
		SendNextRequestInQueue();
	}	

#ifdef _X360

	if ( !AnyUserConnectedToLIVE() && !IsAQueryPending() && m_pPanelList && m_pPanelList->GetPanelItemCount() )
	{
		// Make sure all items are removed when disconnected from LIVE
		StartSearching( false );
	}

	vgui::Label	*pNoEntriesLabel = dynamic_cast< vgui::Label* >( FindChildByName( "LblNoEntriesFound" ) );
	if ( pNoEntriesLabel )
	{
		bool bVisible = !m_pPanelList->GetPanelItemCount();
		pNoEntriesLabel->SetVisible( bVisible );

		// Set the correct text too
		if ( bVisible )
		{
			char const *szText = "#L4D360UI_Leaderboard_No_Times";

			if ( !AnyUserConnectedToLIVE() )
				szText = "#L4D360UI_MainMenu_SurvivalLeaderboards_Tip_Disabled";
			else if ( IsAQueryPending() )
				szText = "#L4D360UI_StillSearching";
			
			pNoEntriesLabel->SetText( szText );
		}
	}

#else
	
	vgui::ImagePanel *pSpinnerImage = dynamic_cast< vgui::ImagePanel* >( FindChildByName( "SearchingIcon" ) );
	if ( pSpinnerImage )
	{
		// If we're searching (or haven't reached the top of the spinner) animate the spinner
		if ( IsSearching() )
		{
			float flTime = Plat_FloatTime();

			// clock the anim at 10hz
			if ( ( m_flLastEngineSpinnerTime + 0.1f ) < flTime )
			{
				pSpinnerImage->SetFrame( m_iCurrentSpinnerValue++ );
				m_flLastEngineSpinnerTime = Plat_FloatTime();
			}

			pSpinnerImage->SetVisible( true );
		}
		else
		{
			// Hide
			pSpinnerImage->SetVisible( false );
		}
	}

	if ( !IsSearching() )
	{
		vgui::Label	*pNoEntriesLabel = dynamic_cast< vgui::Label* >( FindChildByName( "LblNoEntriesFound" ) );
		if ( pNoEntriesLabel )
		{
			pNoEntriesLabel->SetVisible( m_pPanelList->GetPanelItemCount() == 0 );
		}
	}

#endif

}

#if defined( _X360 )

//=============================================================================
void Leaderboard::AsyncLoadCompleted( void )
{
	Assert( IsAQueryPending() == true );
	Assert( m_bufAsyncReadResults.Size() != NULL );

	bool bSuccess = false;

	Assert( m_iAsyncQueryMapContext == GetCurrentChapterContext() );	// if we hit this, we're not clearing queries properly when changing map context

	if ( m_iAsyncQueryMapContext == GetCurrentChapterContext() )
	{
		switch( m_pendingRequest.asyncQueryType )
		{
		case QUERY_ENUM_FRIENDS:
			bSuccess = HandleQuery_EnumFriends();
			break;

		case QUERY_STATS_FRIENDS:
			bSuccess = HandleQuery_StatsFriends();
			break;

		case QUERY_PLAYER_RANK:
			bSuccess = HandleQuery_PlayerRank();
			break;

		case QUERY_RANK_ONE_STATS:
			bSuccess = HandleQuery_RankOneStats();
			break;

		case QUERY_STATS_GLOBAL_PAGE:
			bSuccess = HandleQuery_StatsGlobalPage();
			break;

		default:
			Assert( 0 );
			break;
		}		 
	}

	// if there is something else in the queue, send that request now
	SendNextRequestInQueue();
}

#endif // _X360

void Leaderboard::ClearAsyncData( void )
{
#ifdef _X360
	// release the async handle
	if ( !XHasOverlappedIoCompleted( &m_xOverlapped ) )
	{
		XCancelOverlapped( &m_xOverlapped );
		memset( &m_xOverlapped, 0, sizeof( m_xOverlapped ) );
	}

	if ( m_hOverlapped )
	{
		CloseHandle( m_hOverlapped );
		m_hOverlapped = NULL;
	}

	m_bufAsyncReadResults.Purge();
#endif

	// clear the query data
	m_pendingRequest.Init();
	m_iAsyncQueryMapContext = -1;
}


//=============================================================================
bool Leaderboard::SendQuery_EnumFriends()
{
#if defined( _X360 )
	DevMsg( 1, "SendQuery_EnumFriends\n" );

	int iController = m_pendingRequest.iData;

	Assert( !m_hOverlapped );

	DWORD dwBufferSize = 0;
	DWORD ret = xonline->XFriendsCreateEnumerator( iController, 0, 100, &dwBufferSize, &m_hOverlapped );
	if ( ret == ERROR_SUCCESS )
	{
		m_bufAsyncReadResults.EnsureCapacity( dwBufferSize );
		ret = XEnumerate( m_hOverlapped, m_bufAsyncReadResults.Base(), dwBufferSize, NULL, &m_xOverlapped );
	}

	if ( ret != ERROR_IO_PENDING )
	{
		ClearAsyncData();
		return false;
	}

#endif // _X360

	return true;
}


//=============================================================================
bool Leaderboard::SendQuery_StatsFriends( void )
{
	//DevMsg( 1, "SendQuery_StatsFriends\n" );

#if defined( _X360 )
	Assert( m_hOverlapped == NULL );

	DWORD dwBufferSize = 0;
	DWORD ret = XUserReadStats( 0, m_friendsXuids.Count(), m_friendsXuids.Base(), 1, &m_spec, &dwBufferSize, NULL, &m_xOverlapped );
	if ( ret == ERROR_INSUFFICIENT_BUFFER )
	{
		m_bufAsyncReadResults.EnsureCapacity( dwBufferSize );
		ret = XUserReadStats( 0, m_friendsXuids.Count(), m_friendsXuids.Base(), 1, &m_spec, &dwBufferSize, ( PXUSER_STATS_READ_RESULTS ) m_bufAsyncReadResults.Base(), &m_xOverlapped );
	}
	if ( ret != ERROR_IO_PENDING )
	{
		ClearAsyncData();
		return false;
	}

	return true;

#else

	// Steam friends query

	m_bIsSearching = true;

	SteamLeaderboard_t hLeaderboard = GetLeaderboardHandle( m_iAsyncQueryMapContext );

	if ( hLeaderboard != 0 )
	{
		// load the specified leaderboard data. We only display k_nMaxLeaderboardEntries entries at a time
		SteamAPICall_t hSteamAPICall = steamapicontext->SteamUserStats()->DownloadLeaderboardEntries( hLeaderboard, k_ELeaderboardDataRequestFriends, 0, 0 );

		if ( hSteamAPICall )
		{
			// Register for the async callback
			m_callResultDownloadEntries.Set( hSteamAPICall, this, &Leaderboard::OnLeaderboardDownloadedEntries );

			return true;
		}
	}

	return false;

#endif // _X360
}

ConVar leaderboard_duplicate_entries( "leaderboard_duplicate_entries", "1", FCVAR_CHEAT );

#if !defined( _X360 )

// If in future we want to read the stats off this entry, see LeaderboardExtraStats_t layout in client.dll

void Leaderboard::OnLeaderboardDownloadedEntries( LeaderboardScoresDownloaded_t *pLeaderboardScoresDownloaded, bool bIOFailure )
{
	if ( bIOFailure || !pLeaderboardScoresDownloaded )
	{
		m_bIsSearching = false;
		ClearAsyncData();
		return;
	}

	// If they changed maps while this call was out, ignore the results
	if ( m_iAsyncQueryMapContext != GetCurrentChapterContext() )
	{
		return;
	}

	//DevMsg( 1, "OnLeaderboardDownloadedEntries\n" );

	LeaderboardEntry_t leaderboardEntry;

	// leaderboard entries handle will be invalid once we return from this function. Copy all data now.
	int iEntries = MIN( pLeaderboardScoresDownloaded->m_cEntryCount, LEADERBOARD_PAGE_SIZE );
	for ( int i=0; i<iEntries; i++ )
	{
		//LeaderboardExtraStats_t stats;
		//memset( &stats, 0, sizeof(stats) );

		if ( steamapicontext->SteamUserStats()->GetDownloadedLeaderboardEntry( pLeaderboardScoresDownloaded->m_hSteamLeaderboardEntries, i, &leaderboardEntry, NULL, 0 ) )
		{
			for ( int j=0;j<leaderboard_duplicate_entries.GetInt();j++ )
			{
				AddLeaderboardEntry( &leaderboardEntry );
			}
		}
	}

	m_bIsSearching = false;

	SortListItems();

	ClearAsyncData();
}


#endif


//=============================================================================
bool Leaderboard::SendQuery_PlayerRank()
{
#if defined( _X360 )
	DevMsg( 1, "SendQuery_PlayerRank\n" );

	XUID xuid = 0ull;
	XUserGetXUID( m_pendingRequest.iData, &xuid );

	DWORD dwBufferSize = 0;
	DWORD ret = XUserCreateStatsEnumeratorByXuid( 0, xuid, 1, 1, &m_spec, &dwBufferSize, &m_hOverlapped );
	if ( ret == ERROR_SUCCESS )
	{
		m_bufAsyncReadResults.EnsureCapacity( dwBufferSize );
		ret = XEnumerate( m_hOverlapped, m_bufAsyncReadResults.Base(), dwBufferSize, NULL, &m_xOverlapped );
	}

	if ( ret != ERROR_IO_PENDING )
	{
		ClearAsyncData();
		return false;
	}

#endif // _X360

	return true;
}

//=============================================================================
bool Leaderboard::SendQuery_RankOneStats()
{
#if defined( _X360 )
	DevMsg( 1, "SendQuery_RankOneStats\n" );

	DWORD dwBufferSize = 0;
	DWORD ret = XUserCreateStatsEnumeratorByRank( 0, 1, 1, 1, &m_spec, &dwBufferSize, &m_hOverlapped );
	if ( ret == ERROR_SUCCESS )
	{
		m_bufAsyncReadResults.EnsureCapacity( dwBufferSize );
		ret = XEnumerate( m_hOverlapped, m_bufAsyncReadResults.Base(), dwBufferSize, NULL, &m_xOverlapped );
	}

	if ( ret != ERROR_IO_PENDING )
	{
		ClearAsyncData();
		return false;
	}

#endif // _X360

	return true;
}

//=============================================================================
bool Leaderboard::SendQuery_StatsGlobalPage()
{
#if defined( _X360 )

	DevMsg( 1, "SendQuery_StatsGlobalPage\n" );

	int iPageToLoad = m_pendingRequest.iData;

	int iFirstRankOnPage = ( iPageToLoad * LEADERBOARD_PAGE_SIZE ) + 1;

	// Get stats starting at the passed rank
	DWORD dwBufferSize = 0;
	DWORD ret = XUserCreateStatsEnumeratorByRank( 0, iFirstRankOnPage, LEADERBOARD_PAGE_SIZE, 1, &m_spec, &dwBufferSize, &m_hOverlapped );
	if ( ret == ERROR_SUCCESS )
	{
		m_bufAsyncReadResults.EnsureCapacity( dwBufferSize );
		ret = XEnumerate( m_hOverlapped, m_bufAsyncReadResults.Base(), dwBufferSize, NULL, &m_xOverlapped );
	}

	if ( ret != ERROR_IO_PENDING )
	{
		ClearAsyncData();
		return false;
	}

#endif // _X360

	return true;
}


//=============================================================================
bool Leaderboard::HandleQuery_EnumFriends( void )
{
#if defined( _X360 )
	DevMsg( 1, "HandleQuery_EnumFriends\n" );

	// read the list of XUIDS!
	// if we got some xuids to query for stats, queue it up

	int iController = m_pendingRequest.iData;

	DWORD iFriendsRead = 0;
	XGetOverlappedResult( &m_xOverlapped, &iFriendsRead, false );

	XONLINE_FRIEND *pFriends = ( XONLINE_FRIEND * )m_bufAsyncReadResults.Base();

	m_friendsXuids.RemoveAll();
	
	if ( iFriendsRead > 0 && pFriends != NULL )
	{
		// copy off the xuids into an array we can pass around
		for ( uint i=0; i<iFriendsRead; i++ )
		{
			m_friendsXuids.AddToTail( pFriends[i].xuid );
		}
	}

	// add the xuids of the local players if they are not duplicates
	for ( int i=0; i<m_localPlayerXUIDs.Count(); i++ )
	{
		if ( m_friendsXuids.Find( m_localPlayerXUIDs.Element(i) ) == m_friendsXuids.InvalidIndex() )
		{
			m_friendsXuids.AddToTail( m_localPlayerXUIDs.Element(i) );
		}
	}

	ClearAsyncData();

	if ( m_friendsXuids.Count() > 0 )
	{
		// send the stats query
		PendingAsyncRequest_t request;
		request.asyncQueryType = QUERY_STATS_FRIENDS;
		request.iData = iController;

		m_AsyncQueryQueue.Insert( request );

		SendNextRequestInQueue();
	}

#endif // _X360

	return true;
}

#if !defined( _X360 )

//=============================================================================
SteamLeaderboard_t Leaderboard::GetLeaderboardHandle( int iMapContext )
{
	// make a mapping of map contexts to leaderboard handles
	unsigned short index = m_LeaderboardHandles.Find( iMapContext );

	if ( index != m_LeaderboardHandles.InvalidIndex() )
	{
		return m_LeaderboardHandles.Element(index);
	}

	return (SteamLeaderboard_t)0;
}

//=============================================================================
void Leaderboard::SetLeaderboardHandle( int iMapContext, SteamLeaderboard_t hLeaderboard )
{
	m_LeaderboardHandles.InsertOrReplace( iMapContext, hLeaderboard );
}

//=============================================================================
void Leaderboard::GetLeaderboardName( int iMapContext, char *pszBuf, int iBufLen )
{
	// name = "<mapname>_<gamemode>"

	KeyValues *pInfoMission = NULL;
	KeyValues *pInfoMap = g_pMatchExtSwarm->GetMapInfo( m_pDataSettings, &pInfoMission );

	if ( pInfoMap )
	{
		Q_snprintf( pszBuf, iBufLen, "%s_%s", m_pDataSettings->GetString( "game/mode", "survival" ), pInfoMap->GetString( "map" ) );
	}
	else if ( iBufLen > 0 )
	{
		pszBuf[0] = '\0';
	}
}

//=============================================================================
bool Leaderboard::SendQuery_FindLeaderboard()
{
	//DevMsg( 1, "SendQuery_FindLeaderboard\n" );

	SteamLeaderboard_t hLeaderboard = GetLeaderboardHandle( m_iAsyncQueryMapContext );

	if ( hLeaderboard != 0 )
	{
		//DevMsg( 1, "- leaderboard already loaded\n" );

		// already have this board

		ClearAsyncData();

		SendNextRequestInQueue();
		return true;
	}

	//DevMsg( 1, "- finding leaderboard\n" );

	char szLeaderboardName[128];
	GetLeaderboardName( m_iAsyncQueryMapContext, szLeaderboardName, 128 );

	SteamAPICall_t hSteamAPICall = steamapicontext->SteamUserStats()->FindLeaderboard( szLeaderboardName );
	if ( hSteamAPICall != 0 )
	{
		m_SteamCallResultCreateLeaderboard.Set( hSteamAPICall, this, &Leaderboard::OnFindLeaderboard );
		return true;
	}

	return false;
}

//=============================================================================
// Callback for QUERY_FIND_LEADERBOARD
//=============================================================================
void Leaderboard::OnFindLeaderboard( LeaderboardFindResult_t *pFindLeaderboardResult, bool bIOFailure )
{
	//DevMsg( 1, "OnFindLeaderboard\n" );

	// see if we encountered an error during the call
	if ( !pFindLeaderboardResult->m_bLeaderboardFound || bIOFailure )
	{
		m_bIsSearching = false;
		ClearAsyncData();

		// remove queued queries
		m_AsyncQueryQueue.RemoveAll();

		return;
	}

	// check to see which leaderboard handle we just retrieved
	const char *pszReturnedName = steamapicontext->SteamUserStats()->GetLeaderboardName( pFindLeaderboardResult->m_hSteamLeaderboard );

	char szLeaderboardName[128];
	GetLeaderboardName( m_iAsyncQueryMapContext, szLeaderboardName, 128 );

	if ( !Q_strcmp( pszReturnedName, szLeaderboardName ) )
	{
		Assert( pFindLeaderboardResult->m_hSteamLeaderboard != 0 );

		SetLeaderboardHandle( m_iAsyncQueryMapContext, pFindLeaderboardResult->m_hSteamLeaderboard );
	}

	m_bIsSearching = false;

	ClearAsyncData();

	SendNextRequestInQueue();
}

#endif // !_X360

//=============================================================================
bool Leaderboard::HandleQuery_StatsFriends( void )
{
#if defined( _X360 )
	DevMsg( 1, "HandleQuery_StatsFriends\n" );

	int iController = m_pendingRequest.iData;	// the controller index from the previous query

	XUSER_STATS_READ_RESULTS *pResults = ( XUSER_STATS_READ_RESULTS * )m_bufAsyncReadResults.Base();

	if ( !pResults || pResults->dwNumViews <= 0 || !pResults->pViews )
	{
		ClearAsyncData();
		return true;
	}

	AddLeaderboardEntries( pResults );

	ClearAsyncData();

	// after we get the stats we'll figure out the world best time so we can mark up players if they are #1
	PendingAsyncRequest_t request;
	request.asyncQueryType = QUERY_RANK_ONE_STATS;
	request.iData = iController;
	m_AsyncQueryQueue.Insert( request );
	
	SendNextRequestInQueue();	

#endif // _X360

	return true;
}

//=============================================================================
bool Leaderboard::HandleQuery_PlayerRank( void )
{
#if defined( _X360 )
	DevMsg( 1, "HandleQuery_PlayerRank\n" );

	XUSER_STATS_READ_RESULTS *pResults = ( XUSER_STATS_READ_RESULTS * )m_bufAsyncReadResults.Base();

	if ( !pResults || pResults->dwNumViews <= 0 || !pResults->pViews )
	{
		ClearAsyncData();
		return true;
	}

	PendingAsyncRequest_t request;
	request.asyncQueryType = QUERY_STATS_GLOBAL_PAGE;

	if ( pResults->pViews->dwNumRows == 0 )
	{
		// player is not in this leaderboard, load rank 1 as normal
		m_iSelectRankWhenLoaded = 1;

		request.iData = 0;
		m_AsyncQueryQueue.Insert( request );

		request.iData = 1;
		m_AsyncQueryQueue.Insert( request );

		request.iData = 2;
		m_AsyncQueryQueue.Insert( request );
	}
	else
	{
		// found it, let's now get a the page he's on and the two pages around us
		int iLocalPlayerPage = GetPageFromRank( pResults->pViews->pRows[0].dwRank );

		m_iSelectRankWhenLoaded = pResults->pViews->pRows[0].dwRank;

		if ( iLocalPlayerPage > 1 )
		{
			request.iData = iLocalPlayerPage;
			m_AsyncQueryQueue.Insert( request );

			request.iData = iLocalPlayerPage - 1;
			m_AsyncQueryQueue.Insert( request );

			request.iData = iLocalPlayerPage + 1;
			m_AsyncQueryQueue.Insert( request );
		}
		else
		{
			request.iData = 0;
			m_AsyncQueryQueue.Insert( request );

			request.iData = 1;
			m_AsyncQueryQueue.Insert( request );

			request.iData = 2;
			m_AsyncQueryQueue.Insert( request );
		}
	}

	ClearAsyncData();

#endif // _X360

	return true;
}

//=============================================================================
bool Leaderboard::HandleQuery_RankOneStats( void )
{
#if defined( _X360 )
	DevMsg( 1, "HandleQuery_RankOneStats\n" );

	XUSER_STATS_READ_RESULTS *pResults = ( XUSER_STATS_READ_RESULTS * )m_bufAsyncReadResults.Base();

	if ( !pResults || pResults->dwNumViews <= 0 || !pResults->pViews || !pResults->pViews->pRows )
	{
		ClearAsyncData();
		return true;
	}

	// we have at least one row
	m_iCurrentMapBestTime =	pResults->pViews->pRows[0].i64Rating;

	SortListItems();

	m_pPanelList->SelectPanelItem( 0, GenericPanelList::SD_DOWN, true, false );

	ClearAsyncData();

#endif // _X360

	return true;
}

//=============================================================================
bool Leaderboard::HandleQuery_StatsGlobalPage( void )
{
#if defined( _X360 )

	// Loaded a new page
	// See if we need to throw out any data to fit this page

	XUSER_STATS_READ_RESULTS *pResults = ( XUSER_STATS_READ_RESULTS * )m_bufAsyncReadResults.Base();

	if ( !pResults || pResults->dwNumViews <= 0 || !pResults->pViews )
	{
		ClearAsyncData();
		return true;
	}

	int iRowsLoaded = pResults->pViews->dwNumRows;

	int iPageLoaded = m_pendingRequest.iData;

	DevMsg( 1, "Leaderboard finished loading page %d, %d rows found\n", iPageLoaded, iRowsLoaded );

	if ( iRowsLoaded > 0 )
	{
		// Discard page we're moving away from
		int iPageToDiscard = -1;

		if ( iPageLoaded < m_iMinPageLoaded || m_iMinPageLoaded == -1 )
			m_iMinPageLoaded = iPageLoaded;

		if ( iPageLoaded >m_iMaxPageLoaded || m_iMaxPageLoaded == -1 )
			m_iMaxPageLoaded = iPageLoaded;

		// Assume we keep 3 pages at once
		if ( iPageLoaded == m_iMaxPageLoaded )
		{
			// - 3
			iPageToDiscard = m_iMaxPageLoaded - 3;
		}
		else if ( iPageLoaded == m_iMinPageLoaded )
		{
			// + 3
			iPageToDiscard = m_iMinPageLoaded + 3;
		}

		if ( iPageToDiscard != -1 && iPageToDiscard >= m_iMinPageLoaded && iPageToDiscard <= m_iMaxPageLoaded )
		{
			// discard this page
			DevMsg( 1, "Leaderboard discarding page %d ( page size %d )\n", iPageToDiscard, LEADERBOARD_PAGE_SIZE );

			int iCount = (int)m_pPanelList->GetPanelItemCount();

			for ( int i=iCount-1; i >= 0; i-- )
			{
				LeaderboardListItem *pItem = (LeaderboardListItem *)m_pPanelList->GetPanelItem( i );
				if ( pItem )
				{
					int iPage = GetPageFromRank( pItem->GetRank() );
					if ( iPage == iPageToDiscard )
					{
						m_pPanelList->RemovePanelItem( i, false );

						ReturnListItemToPool( pItem );
					}
				}
			}

			// fixup min / max pages loaded
			// assume we're only throwing away the min or the max page
			if ( iPageToDiscard == m_iMinPageLoaded )
			{
				m_iMinPageLoaded = min( m_iMinPageLoaded + 1, m_iMaxPageLoaded );
			}
			else if ( iPageToDiscard == m_iMaxPageLoaded )
			{
				m_iMaxPageLoaded = max( m_iMaxPageLoaded - 1, m_iMinPageLoaded );
			}
		}

		AddLeaderboardEntries( pResults );

		if ( m_iSelectRankWhenLoaded > 0 )
		{
			m_bIgnoreSelectionChanges = true;

			unsigned short iCount = m_pPanelList->GetPanelItemCount();
			for ( unsigned short i=0; i<iCount; i++ )
			{
				LeaderboardListItem *pItem = ( LeaderboardListItem * )m_pPanelList->GetPanelItem( i );
				if ( pItem )
				{
					if ( pItem->GetRank() == m_iSelectRankWhenLoaded )
					{
						m_pPanelList->SelectPanelItem( i, GenericPanelList::SD_DOWN, true, false );
						m_pPanelList->ScrollToPanelItem( i );
						break;
					}
				}
			}

			m_iSelectRankWhenLoaded = 0;
			m_bIgnoreSelectionChanges = false;
		}
	}

	ClearAsyncData();

#endif // _X360

	return true;
}
	

void Leaderboard::CancelCurrentSearch( void )
{
	ClearAsyncData();

	// remove all queued queries
	m_AsyncQueryQueue.RemoveAll();
}

#if defined( _X360 )
void Leaderboard::AddLeaderboardEntries( XUSER_STATS_READ_RESULTS *pResults )
{
	int iRows = pResults->pViews->dwNumRows;

	for ( int i=0; i<iRows; i++ )
	{
		for ( int j=0; j<leaderboard_duplicate_entries.GetInt(); j++ )
		{
			AddLeaderboardEntry( &pResults->pViews->pRows[i] );
		}
	}

	SortListItems();
}

ConVar ui_leaderboard_allow_cached_time( "ui_leaderboard_allow_cached_time", "0" );

void Leaderboard::AddLeaderboardEntry( XUSER_STATS_ROW *pRow )
{
	if ( !pRow )
		return;

	// find an unused list item
	LeaderboardListItem *pListItem = GetListItemFromPool();

	if ( !pListItem )
		return;

	pListItem->SetAlpha( 0 );
	m_pPanelList->AddPanelItem( pListItem, false );

	//
	// Fill in basic information from Xbox LIVE leaderboard row
	//
	LeaderboardItem_t data;

	data.m_xuid = pRow->xuid;
	Q_strncpy( data.m_szGamerTag, pRow->szGamertag, XUSER_NAME_SIZE );

	data.m_iRank = pRow->dwRank;
	data.m_iDisplayRank = data.m_iRank;
	data.m_iTimeInHundredths = pRow->i64Rating;

	static CGameUIConVarRef cl_names_debug( "cl_names_debug" );
	if ( cl_names_debug.GetInt() )
	{
		Q_strcpy( data.m_szGamerTag, "WWWWWWWWWWWWWWW" );
		data.m_iRank += 2222222;
		data.m_iDisplayRank += 2222222;
	}

	//
	// Detect a local player's controller index
	//
	data.m_bLocalPlayer = false;
	data.m_iControllerIndex = 0;

	int index = m_localPlayerXUIDs.Find( data.m_xuid );

	if ( index != m_localPlayerXUIDs.InvalidIndex() )
	{
		data.m_bLocalPlayer = true;

		int iController = XBX_GetUserId( index );

		data.m_iControllerIndex = iController;
	}

	//
	// Use local leaderboard data if it is available and better
	//
	if ( ui_leaderboard_allow_cached_time.GetBool() &&
		 data.m_bLocalPlayer &&
		 data.m_iControllerIndex >= 0 && data.m_iControllerIndex < XUSER_MAX_COUNT )
	{
		if ( IPlayerLocal *pLocalPlayer = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( data.m_iControllerIndex ) )
		{
			// Prepare the leaderboard request
			KeyValues *pLeaderboardRequest = new KeyValues( "Leaderboard" );
			KeyValues::AutoDelete autodelete( pLeaderboardRequest );

			KeyValues *pLeaderboardData = new KeyValues( m_szLeaderboardViewNameSpec );
			pLeaderboardRequest->AddSubKey( pLeaderboardData );

			pLeaderboardData->SetUint64( "besttime", data.m_iTimeInHundredths );	// fetching "besttime" field

			pLocalPlayer->GetLeaderboardData( pLeaderboardRequest );
			uint64 uiLocalTime = pLeaderboardData->GetUint64( "besttime" );
			if ( uiLocalTime > data.m_iTimeInHundredths )
			{
				DevWarning( "Using cached leaderboard value %llu > %llu for player %s (%llx)!\n",
					uiLocalTime, pRow->i64Rating, data.m_szGamerTag, data.m_xuid );
				data.m_iTimeInHundredths = uiLocalTime;
			}
		}
	}

	//
	// Analyze the leaderboard data for awards
	//
	int iTimeInSeconds = data.m_iTimeInHundredths / 100;

	if ( iTimeInSeconds >= HOLDOUT_MEDAL_TIME_GOLD )
	{
		data.m_iAwards = HOLDOUT_MEDAL_GOLD;

		if ( data.m_iRank == 1 )
		{
			data.m_iAwards = HOLDOUT_MEDAL_PLATINUM;
		}
	}
	else if ( iTimeInSeconds >= HOLDOUT_MEDAL_TIME_SILVER )
	{
		data.m_iAwards = HOLDOUT_MEDAL_SILVER;
	}
	else if ( iTimeInSeconds >= HOLDOUT_MEDAL_TIME_BRONZE )
	{
		data.m_iAwards = HOLDOUT_MEDAL_BRONZE;
	}
	else
	{
		data.m_iAwards = -1;
	}

	pListItem->SetData( data );
}
#endif	// _X360

#if !defined( _X360 )

void Leaderboard::AddLeaderboardEntry( LeaderboardEntry_t *pEntry )
{
	if ( !pEntry )
		return;

	// find an unused list item
	LeaderboardListItem *pListItem = GetListItemFromPool();

	if ( !pListItem )
		return;

	m_pPanelList->AddPanelItem( pListItem, false );

	LeaderboardItem_t data;

	data.m_iRank = data.m_iDisplayRank = pEntry->m_nGlobalRank;
	data.m_iTimeInHundredths = pEntry->m_nScore;

	int iTimeInSeconds = data.m_iTimeInHundredths / 100;

	if ( iTimeInSeconds >= HOLDOUT_MEDAL_TIME_GOLD )
	{
		data.m_iAwards = HOLDOUT_MEDAL_GOLD;
	}
	else if ( iTimeInSeconds >= HOLDOUT_MEDAL_TIME_SILVER )
	{
		data.m_iAwards = HOLDOUT_MEDAL_SILVER;
	}
	else if ( iTimeInSeconds >= HOLDOUT_MEDAL_TIME_BRONZE )
	{
		data.m_iAwards = HOLDOUT_MEDAL_BRONZE;
	}
	else
	{
		data.m_iAwards = -1;
	}

	data.m_steamIDUser = pEntry->m_steamIDUser;
	Q_strncpy( data.m_szName, steamapicontext->SteamFriends()->GetFriendPersonaName( data.m_steamIDUser ), MAX_PLAYER_NAME_LENGTH );

	data.m_bLocalPlayer = false;

	// TODO: am i the local player

	/*
	player_info_t pi;
	if ( engine->GetPlayerInfo( entindex(), &pi ) && ( pi.friendsID ) )
	{
		const CSteamID steamIDForPlayer( pi.friendsID, 1, GetSteamUniverse(), k_EAccountTypeIndividual );

		*pID = steamIDForPlayer;
		return true;
	}

	int index = m_localPlayerXUIDs.Find( data.m_xuid );

	if ( index != m_localPlayerXUIDs.InvalidIndex() )
	{
		data.m_bLocalPlayer = true;
	}
	*/

	pListItem->SetData( data );
}

#endif // !_X360 

//=============================================================================
void Leaderboard::OnKeyCodePressed( KeyCode code )
{
#if defined( _X360 )

	int iUserSlot = GetJoystickForCode( code );
	CBaseModPanel::GetSingleton().SetLastActiveUserId( iUserSlot );

	switch( GetBaseButtonCode( code ) )
	{
	case KEY_XBUTTON_A:
		CmdViewGamercard();
		break;

	case KEY_XBUTTON_X:
		CmdToggleLeaderboardType();
		break;

	case KEY_XBUTTON_Y:
		OnCommand( "FindGameForThisChapter" );
		break;

	case KEY_XBUTTON_LTRIGGER:
		CmdJumpToTop();
		break;

	case KEY_XBUTTON_RTRIGGER:
		CmdJumpToMe();
		break;

	case KEY_XSTICK1_RIGHT:
	case KEY_XBUTTON_RIGHT:
	case KEY_XBUTTON_RIGHT_SHOULDER:
		CmdNextLeaderboard();
		break;

	case KEY_XSTICK1_LEFT:
	case KEY_XBUTTON_LEFT:
	case KEY_XBUTTON_LEFT_SHOULDER:
		CmdPrevLeaderboard();
		break;

	default:
		BaseClass::OnKeyCodePressed( code );
		break;
	}

#endif
}

//=============================================================================
void Leaderboard::OnCommand( const char *command )
{
	if ( !Q_strcmp( command, "FindGameForThisChapter" ) )
	{
		if ( engine->IsConnected() )
			return;

		// Show the game settings dialog so we can try to join a server running the current chapter context
		KeyValues *pNewDataSettings = m_pDataSettings ? m_pDataSettings->MakeCopy() : new KeyValues( "settings" );
		KeyValues::AutoDelete autodelete_pNewDataSettings( pNewDataSettings );
		if ( CUIGameData::Get()->SignedInToLive() )
		{
			pNewDataSettings->SetString( "system/network", "LIVE" );
			pNewDataSettings->SetString( "options/action", "custommatch" );
		}
		else
		{
			pNewDataSettings->SetString( "system/network", "lan" );
			pNewDataSettings->SetString( "options/action", "create" );
		}
		pNewDataSettings->SetString( "game/mode", m_pDataSettings->GetString( "game/mode", "survival" ) );

		// close the leaderboard
		CBaseModPanel::GetSingleton().CloseAllWindows();

		// open the game settings
		CBaseModPanel::GetSingleton().OpenWindow( WT_GAMESETTINGS, NULL, true, pNewDataSettings );
		return;
	}
#if !defined( _X360 )
	else if ( !Q_strcmp( command, "Exit" ) )
	{
		NavigateBack();
	}
	else if ( !Q_strcmp( command, "PanelListSliderMoved" ) )
	{
		ClosePlayerFlyout();
	}
	else if ( char const *szChapterSelected = StringAfterPrefix( command, "#L4D360UI_Chapter_" ) )
	{
		m_pDataSettings->SetInt( "game/chapter", atoi( szChapterSelected ) );

		OnMissionChapterChanged();

		DropDownMenu *pChapterDropDown = dynamic_cast< DropDownMenu* >( FindChildByName( "DrpChapter" ) );
		if( pChapterDropDown ) //missions change what the available campaigns are, we should listen on that flyout as well
		{
			FlyoutMenu* flyout = pChapterDropDown->GetCurrentFlyout();
			if( flyout )
			{
				//we will need to find the child button that is normally triggered when we make this new selection
				//and run the same code path as if the user had selected that mission from the list
				vgui::Button *newChapter = flyout->FindChildButtonByCommand( command );
				OnNotifyChildFocus( newChapter );
			}

			if ( m_ActiveControl )
			{
				m_ActiveControl->NavigateFrom( );
			}
			pChapterDropDown->NavigateTo();
			m_ActiveControl = pChapterDropDown;
		}
	}
	else if ( V_strcmp( command, "cmd_addoncampaign" ) == 0 )
	{
		CBaseModPanel::GetSingleton().OpenWindow( WT_CUSTOMCAMPAIGNS, this, true, m_pDataSettings );
	}
	else if ( const char* szMissionItem = StringAfterPrefix( command, "cmd_campaign_" ) )
	{
		DropDownMenu *pMissionDropDown = dynamic_cast< DropDownMenu* >( FindChildByName( "DrpMission" ) );
		if( pMissionDropDown ) //we should become a listener for events pertaining to the mission flyout
		{
			FlyoutMenu* missionFlyout = pMissionDropDown->GetCurrentFlyout();
			if( missionFlyout )
			{
				missionFlyout->SetListener( this );
			}
		}

		DropDownMenu *pChapterDropDown = dynamic_cast< DropDownMenu* >( FindChildByName( "DrpChapter" ) );
		if( pChapterDropDown ) //missions change what the available campaigns are, we should listen on that flyout as well
		{
			m_pDataSettings->SetString( "game/campaign", szMissionItem );
			m_pDataSettings->SetInt( "game/chapter", 1 );
		}

		OnMissionChapterChanged();
	}
#endif
	else 
	{
		BaseClass::OnCommand( command );
	}
}

#if !defined( _X360 )

//=============================================================================
void Leaderboard::OpenPlayerFlyout( BaseModHybridButton *button, uint64 playerId, int x, int y )
{
	if ( playerId == 0 )
		return;

	FlyoutMenu *flyout = NULL;

	if ( steamapicontext->SteamFriends()->GetFriendRelationship( playerId ) == k_EFriendRelationshipFriend )
	{
		flyout = dynamic_cast< FlyoutMenu * >( FindChildByName( "FlmPlayerFlyout" ) );
	}
	else
	{
		flyout = dynamic_cast< FlyoutMenu * >( FindChildByName( "FlmPlayerFlyout_NotFriend" ) );
	}

	if ( flyout )
	{
		// If one is open for this player, close it
		if ( playerId == m_flyoutPlayerId && flyout->IsVisible() )
		{
			flyout->CloseMenu( button );
			return;
		}

		int wndX, wndY;
		GetPos( wndX, wndY );

		m_flyoutPlayerId = playerId;
		flyout->OpenMenu( button );
		flyout->SetPos( x, y - wndY );
		flyout->SetOriginalTall( 0 );
	}
}

//=============================================================================
void Leaderboard::ClosePlayerFlyout( void )
{
	FlyoutMenu *pFlyout = dynamic_cast< FlyoutMenu * >( FindChildByName( "FlmPlayerFlyout" ) );
	if ( pFlyout && pFlyout == FlyoutMenu::GetActiveMenu() )
	{
		pFlyout->CloseMenu( NULL );
	}

	pFlyout = dynamic_cast< FlyoutMenu * >( FindChildByName( "FlmPlayerFlyout_NotFriend" ) );
	if ( pFlyout && pFlyout == FlyoutMenu::GetActiveMenu() )
	{
		pFlyout->CloseMenu( NULL );
	}

}

#endif // !_X360

//=============================================================================
void Leaderboard::CmdViewGamercard( void )
{
#ifdef _X360
	// Warn a player who tries to access a gamer card without multiplayer privileges that they may not do so
	// shouldn't be able to get into this screen without being connected to live anyway
	if ( AnyUserConnectedToLIVE() == false )
	{
		return;
	}

	int iUserSlot = CBaseModPanel::GetSingleton().GetLastActiveUserId();
	int iController;

	if ( XBX_GetUserIsGuest( iUserSlot ) )
	{
		iController = XBX_GetPrimaryUserId();
	}
	else
	{
		iController = XBX_GetUserId( iUserSlot );
	}

	if ( !IsUserLIVEEnabled( iController ) )
		return;

	// get the xuid of the selected list item
	LeaderboardListItem *pItem = dynamic_cast< LeaderboardListItem * >( m_pPanelList->GetSelectedPanelItem() );
	if ( pItem )
	{
		LeaderboardItem_t *data = pItem->GetDataForModify();

		CBaseModPanel::GetSingleton().PlayUISound( UISOUND_ACCEPT );
		XShowGamerCardUI( iController, data->m_xuid );
	}
#endif // _X360
}

//=============================================================================
void Leaderboard::CmdToggleLeaderboardType()
{
#if defined( _X360 )
	m_Mode = ( m_Mode == LEADERBOARD_FRIENDS ) ? LEADERBOARD_GLOBAL : LEADERBOARD_FRIENDS;

	CBaseModFooterPanel *footer = BaseModUI::CBaseModPanel::GetSingleton().GetFooterPanel();
	if( footer )
	{
		switch( m_Mode )
		{
		case LEADERBOARD_FRIENDS:
			footer->SetButtonText( FB_XBUTTON, "#L4D360UI_Leaderboard_Show_Global" );
			break;

		case LEADERBOARD_GLOBAL:
		default:
			footer->SetButtonText( FB_XBUTTON, "#L4D360UI_Leaderboard_Show_Friends" );
			break;
		}
	}

	// Refresh the stats shown
	StartSearching( false );
#endif
}

//=============================================================================
void Leaderboard::CmdJumpToTop()
{
	// try to view rank 1
	// if it is in the loaded range ( m_iMinRankLoaded - m_iMaxRankLoaded ) we'll just scroll to it
	// otherwise we'll load data around rank 1

	//DevMsg( 1, "Jump to top!\n" );

	if ( m_Mode == LEADERBOARD_FRIENDS )
	{
		if ( m_pPanelList->GetPanelItemCount() > 0 )
		{
			m_pPanelList->SelectPanelItem( 0, GenericPanelList::SD_DOWN );
			m_pPanelList->ScrollToPanelItem( 0 );
		}
	}
	else
	{
		// we want to scroll to rank 1.

		// if page 0 is not loaded, start the search over, it will select rank 1
		if ( m_iMinPageLoaded > 0 )
		{
			StartSearching( false );
		}
		else
		{
			if ( m_pPanelList->GetPanelItemCount() > 0 )
			{
				// we have page 0 loaded, rank 1 must be at the top so just jump to it
				m_pPanelList->SelectPanelItem( 0, GenericPanelList::SD_DOWN );
				m_pPanelList->ScrollToPanelItem( 0 );
			}
		}
	}
}

//=============================================================================
void Leaderboard::CmdJumpToMe()
{
	// figure out our rank in this leaderboard
	// if it is in the range, scroll to it, else load the data around us

	// find my xuid
#if defined( _X360 )
	int iUserSlot = CBaseModPanel::GetSingleton().GetLastActiveUserId();
	int iController = XBX_GetUserId( iUserSlot );

	// omit guests
	if ( XBX_GetUserIsGuest( iController ) )
		return;

	XUID xuid = 0ull;
	XUserGetXUID( iController, &xuid );

	LeaderboardListItem *pMyItem = NULL;

	// locate the list item that has that xuid
	int iPanelCount = m_pPanelList->GetPanelItemCount();
	for ( int i=0; i<iPanelCount; i++ )
	{
		LeaderboardListItem *pItem = ( LeaderboardListItem * )m_pPanelList->GetPanelItem( i );
		if ( pItem )
		{
			if ( pItem->GetXUID() == xuid )
			{
				pMyItem = pItem;
				break;
			}
		}
	}

	if ( pMyItem )
	{
		unsigned short index;
		m_pPanelList->GetPanelItemIndex( pMyItem, index );

		m_pPanelList->SelectPanelItem( index, GenericPanelList::SD_DOWN );
		m_pPanelList->ScrollToPanelItem( index );
	}
	else
	{
		// we're not in this list, we may have to load new data to get us in the list

		Assert( m_Mode != LEADERBOARD_FRIENDS );	// We should always be in the list if we're in friends mode

		StartSearching( true );
	}
#endif // _X360
}

//=============================================================================
void Leaderboard::CmdLeaderboardHelper( int nOffset )
{
	KeyValues *pMissionInfo = NULL;
	KeyValues *pMapInfo = g_pMatchExtSwarm->GetMapInfo( m_pDataSettings, &pMissionInfo );
	if ( !pMapInfo )
		return;

	KeyValues *pAllMissions = g_pMatchExtSwarm->GetAllMissions();
	if ( !pAllMissions )
		return;

	CUtlVector< KeyValues * > arrMissions;
	CUtlVector< int > arrCountChaps;
	CFmtStr strChapters( "modes/%s/chapters", m_pDataSettings->GetString( "game/mode", "survival" ) );

	int idxCurrent = 0;

	// Build the array
	for ( KeyValues *pM = pAllMissions->GetFirstTrueSubKey(); pM; pM = pM->GetNextTrueSubKey() )
	{
		int numChapters = pM->GetInt( strChapters );
		if ( numChapters <= 0 )
			continue;

		arrMissions.AddToTail( pM );
		arrCountChaps.AddToTail( numChapters );

		if ( pM == pMissionInfo )
			idxCurrent = arrMissions.Count() - 1;
	}
	if ( !arrMissions.IsValidIndex( idxCurrent ) )
		return;

	// Now that we know current position in the array
	int nNewChapter = pMapInfo->GetInt( "chapter", -1000 );
	nNewChapter += nOffset;

	if ( nNewChapter < 1 || nNewChapter > arrCountChaps[ idxCurrent ] )
	{
		idxCurrent = ( idxCurrent + nOffset + arrMissions.Count() ) % arrMissions.Count();

		// Got the next mission, look for chapter
		nNewChapter = ( nOffset > 0 ) ? 1 : arrCountChaps[ idxCurrent ];

		m_pDataSettings->SetString( "game/campaign", arrMissions[idxCurrent]->GetString( "name" ) );
	}
	m_pDataSettings->SetInt( "game/chapter", nNewChapter );

	// Prepare UI
	OnMissionChapterChanged();
}

//=============================================================================
void Leaderboard::CmdNextLeaderboard()
{
	CmdLeaderboardHelper( +1 );
}
void Leaderboard::CmdPrevLeaderboard()
{
	CmdLeaderboardHelper( -1 );
}


//=============================================================================
void Leaderboard::PaintBackground()
{
	// Friends or Global? Change the description
	const char *pszDescription;

	if ( m_Mode == LEADERBOARD_FRIENDS )
		pszDescription = "#L4D360UI_Leaderboard_Subtitle_Survival_Friends";
	else
		pszDescription = "#L4D360UI_Leaderboard_Subtitle_Survival_Global";

	BaseClass::DrawDialogBackground( "#L4D360UI_Leaderboard_Title", NULL, pszDescription, NULL, NULL, false, m_iTitleXOffset );
}

//=============================================================================
void Leaderboard::NavigateTo()
{
	BaseClass::NavigateTo();

	m_pPanelList->NavigateTo();
}

//=============================================================================
static int __cdecl LeaderboardSortFunc( vgui::Panel* const *a, vgui::Panel* const *b)
{
	LeaderboardListItem *fA	= dynamic_cast< LeaderboardListItem* >(*a);
	LeaderboardListItem *fB	= dynamic_cast< LeaderboardListItem* >(*b);

	int rankA = fA->GetRank();
	if ( rankA == 0 ) rankA = INT_MAX;

	int rankB = fB->GetRank();
	if ( rankB == 0 ) rankB = INT_MAX;

	if ( rankA != rankB )
	{
		return ( rankA < rankB ? -1 : 1 );
	}

	// then by gamertag
	return Q_stricmp( fA->GetPlayerName(), fB->GetPlayerName() );
}

ConVar leaderboard_force_rank_show_interval( "leaderboard_force_rank_show_interval", "10" );

void Leaderboard::SortListItems()
{
#if defined( _X360 )
	m_pPanelList->SortPanelItems( LeaderboardSortFunc );
#endif

	// items with the same time should draw the same rank ( or none at all )

	// LEADERBOARD_FRIENDS mode only - change the ranks to be friends ranks rather than the global rank that gets passed back

	int iFriendsRank = 1;

	int iPreviousTime = 0;

#if defined( _X360 )
	bool bPreviousHasPlatinum = false;
#endif

	int iForceShowRankCounter = 0;
	int iCurrentDisplayRank = 0;

	unsigned short iCount = m_pPanelList->GetPanelItemCount();
	for ( unsigned int i=0; i<iCount; i++ )
	{
		LeaderboardListItem *pItem = ( LeaderboardListItem * )m_pPanelList->GetPanelItem( i );
		if ( pItem )
		{
			LeaderboardItem_t *pData = pItem->GetDataForModify();
			if ( !pData )
				continue;

			// If we're in friends display mode
			// Re-number the items to be the rank among friends
			// give platinum medal if our time is equal to the world best
			if ( m_Mode == LEADERBOARD_FRIENDS && pData->m_iRank > 0 )
			{
				pData->m_iRank = iFriendsRank;
				iFriendsRank++;
#if defined( _X360 )
				if ( pData->m_iTimeInHundredths >= m_iCurrentMapBestTime )
				{
					int iTimeInSeconds = pData->m_iTimeInHundredths / 100;

					if ( iTimeInSeconds >= HOLDOUT_MEDAL_TIME_GOLD )
					{
						pData->m_iAwards = HOLDOUT_MEDAL_PLATINUM;
					}
				}
#endif
			}

			// Show or hide the rank based on if we have the same time as the person above us
			// Also give out platinum medals to people who have the same time as a rank 1 person ( global mode )
			if ( i == 0 )
			{
				pItem->SetShowRank( true, pData->m_iRank );
#if defined( _X360 )
				bPreviousHasPlatinum = ( pData->m_iAwards == HOLDOUT_MEDAL_PLATINUM );
#endif
				iCurrentDisplayRank = pData->m_iRank;
			}
			else
			{
				// hide the rank if we're equal to the entry above us
				if ( pData->m_iTimeInHundredths != iPreviousTime )
				{
					// New rank display
					pItem->SetShowRank( true, pData->m_iRank );

					// start a new run
					iForceShowRankCounter = 0;
					iCurrentDisplayRank = pData->m_iRank;
				}
				else
				{
					// same as previous time, either show no rank, or show iCurrentDisplayRank
					iForceShowRankCounter++;
					if ( iForceShowRankCounter >= leaderboard_force_rank_show_interval.GetInt() )
					{
						iForceShowRankCounter = 0;
					}

					if ( pData->m_bLocalPlayer || iForceShowRankCounter == 0 )
					{
						pItem->SetShowRank( true, iCurrentDisplayRank );
					}
					else
					{
						pItem->SetShowRank( false, 0 );
					}
				}

#if defined( _X360 )
				if ( bPreviousHasPlatinum && m_Mode == LEADERBOARD_GLOBAL )
				{
					bool bHasPlatinum = false;

					// give us the platinum medal if the person above us has platinum and has the same time as us
					if ( pData->m_iTimeInHundredths == iPreviousTime )
					{
						int iTimeInSeconds = pData->m_iTimeInHundredths / 100;

						if ( iTimeInSeconds >= HOLDOUT_MEDAL_TIME_GOLD )
						{
							bHasPlatinum = true;
						}
					}

					if ( bHasPlatinum )
					{
						pData->m_iAwards = HOLDOUT_MEDAL_PLATINUM;
					}

					bPreviousHasPlatinum = bHasPlatinum;
				}	
#endif	//_X360 
			}

			iPreviousTime = pData->m_iTimeInHundredths;
		}
	}
}

//=============================================================================
void Leaderboard::OnOpen()
{
//	SetVisible( true );

	BaseClass::OnOpen();
}

void Leaderboard::UpdateFooter()
{
#ifdef _X360
	CBaseModFooterPanel *footer = BaseModUI::CBaseModPanel::GetSingleton().GetFooterPanel();
	if ( footer )
	{
		bool bY = !engine->IsConnected();
		footer->SetButtons(
			( bY ? FB_YBUTTON : FB_NONE ) | FB_XBUTTON | FB_BBUTTON |
			( AnyUserConnectedToLIVE() ? FB_ABUTTON : FB_NONE ),
			bY ? FF_A_GAMERCARD_BY__X_HIGH : FF_A_GAMERCARD_BX, false );

		footer->SetButtonText( FB_BBUTTON, "#L4D360UI_Back" );
		footer->SetButtonText( FB_ABUTTON, "#L4D360UI_ViewGamerCard" );
		footer->SetButtonText( FB_YBUTTON, "#L4D360UI_Leaderboard_Join_Game" );
	}
#endif
}

//=============================================================================
void Leaderboard::OnClose()
{
	BaseClass::OnClose();

	RemoveFrameListener( this );
}
