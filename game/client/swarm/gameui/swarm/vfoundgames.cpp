//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "cbase.h"
#include "VFoundGames.h"
#include "VGenericPanelList.h"
#include "EngineInterface.h"
#include "VFooterPanel.h"
#include "VHybridButton.h"
#include "VDropDownMenu.h"
#include "VFlyoutMenu.h"
#include "UIGameData.h"
#include "vdownloadcampaign.h"
#include "gameui_util.h"

#include "vgui/ISurface.h"
#include "vgui/IBorder.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/Button.h"
#include "vgui_controls/ImagePanel.h"
#include "vgui/ILocalize.h"
#include "VGenericConfirmation.h"
#include "VGameSettings.h"
#include "vgetlegacydata.h"
#include "cdll_util.h"
#include "nb_header_footer.h"
#include "nb_button.h"
#include "fmtstr.h"
#include "smartptr.h"
#include "missionchooser/iasw_mission_chooser.h"
#include "missionchooser/iasw_mission_chooser_source.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace BaseModUI;

ConVar ui_foundgames_spinner_time( "ui_foundgames_spinner_time", "1", FCVAR_DEVELOPMENTONLY );
ConVar ui_foundgames_update_time( "ui_foundgames_update_time", "1", FCVAR_DEVELOPMENTONLY );
ConVar ui_foundgames_fake_content( "ui_foundgames_fake_content", "0", FCVAR_DEVELOPMENTONLY );
ConVar ui_foundgames_fake_count( "ui_foundgames_fake_count", "0", FCVAR_DEVELOPMENTONLY );

void Demo_DisableButton( Button *pButton );

const char *COM_GetModDirectory();

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

static char const * NoTeamGameMode( char const * szGameMode )
{
	if ( char const *szNoTeamMode = StringAfterPrefix( szGameMode, "team" ) )
		return szNoTeamMode;
	else
		return szGameMode;
}

bool BaseModUI::FoundGameListItem::Info::IsJoinable() const
{
	return mbInGame && mIsJoinable;
}

bool BaseModUI::FoundGameListItem::Info::IsDLC() const
{
	if ( IsX360() )
		return mbDLC;

	return false;
}

char const * BaseModUI::FoundGameListItem::Info::IsOtherTitle() const
{
	if ( mchOtherTitle[0] )
		return mchOtherTitle;

	return NULL;
}

bool BaseModUI::FoundGameListItem::Info::IsDownloadable() const
{
	if ( IsX360() )
		return false;

	if ( mbInGame && mpGameDetails )
	{
		IASW_Mission_Chooser_Source *pSource = missionchooser ? missionchooser->LocalMissionSource() : NULL;
		if ( pSource )
		{
			const char *szMissionName = mpGameDetails->GetString( "game/mission", "" );
			KeyValues *pMissionKeys = pSource->GetMissionDetails( szMissionName );
			char const *szWebsite = mpGameDetails->GetString( "game/missioninfo/website", NULL );
			if ( ( !pMissionKeys || Q_stricmp( pMissionKeys->GetString( "version", "" ),
				mpGameDetails->GetString( "game/missioninfo/version", "" ) ) )
				&& ( szWebsite && *szWebsite ) )
				return true;
		}
	}

	return false;
}

const char * BaseModUI::FoundGameListItem::Info::GetJoinButtonHint() const
{
	if ( IsOtherTitle() )
	{
		return "#L4D360UI_FoundGames_Join_Modded";
	}
	if ( IsDLC() || IsDownloadable() )
	{
		return "#L4D360UI_FoundGames_Join_Download";
	}

	if ( IsJoinable() )
	{
		return "#L4D360UI_FoundGames_Join_Success";
	}

	int numSlots = mpGameDetails->GetInt( "members/numSlots", 0 );
	int numPlayers = mpGameDetails->GetInt( "members/numPlayers", 0 );

	if ( numSlots <= 0 )
		return "#L4D360UI_FoundGames_Join_Fail_Not_In_Joinable";
	
	if ( numPlayers >= numSlots )
		return "#L4D360UI_FoundGames_Join_Fail_No_Slots";

	if ( !Q_stricmp( "private", mpGameDetails->GetString( "system/access", "" ) ) )
		return "#L4D360UI_FoundGames_Join_Fail_Private_Game";

	return "#L4D360UI_FoundGames_Join_Fail_Not_In_Joinable";
}

const char * BaseModUI::FoundGameListItem::Info::GetNonJoinableShortHint() const
{
	int numSlots = mpGameDetails->GetInt( "members/numSlots", 0 );
	int numPlayers = mpGameDetails->GetInt( "members/numPlayers", 0 );

	if ( !numSlots )
		return ""; //#L4D360UI_Lobby_NotInJoinableGame";

	if ( numPlayers >= numSlots )
		return "#L4D360UI_WaitScreen_GameFull";

	if ( !Q_stricmp( "private", mpGameDetails->GetString( "system/access", "" ) ) )
		return "#L4D360UI_WaitScreen_GamePrivate";

	return "";
}

//=============================================================================
//
//=============================================================================
FoundGameListItem::FoundGameListItem( vgui::Panel *parent, const char *panelName ):
	BaseClass( parent, panelName ),
	m_pListCtrlr( ( GenericPanelList * ) parent )
{
	m_pImgPing = NULL;
	m_pLblPing = NULL;
	m_pLblPlayerGamerTag = NULL;
	m_pLblDifficulty = NULL;
	m_pLblSwarmState = NULL;
	m_pLblPlayers = NULL;
	m_pLblNotJoinable = NULL;

	SetProportional( true );

	SetGameIndex( m_FullInfo );

	SetPaintBackgroundEnabled( true );

	m_sweep = 0;
	m_bSelected = false;
	m_bHasMouseover = false;

	CBaseModFrame::AddFrameListener( this );
}

//=============================================================================
FoundGameListItem::~FoundGameListItem()
{
	if ( m_FullInfo.mpGameDetails )
		m_FullInfo.mpGameDetails->deleteThis();
	m_FullInfo.mpGameDetails = NULL;

	CBaseModFrame::RemoveFrameListener( this );
}

//=============================================================================
void FoundGameListItem::SetAvatarXUID( XUID xuid )
{
	if ( IsPC() )
	{
		vgui::ImagePanel *imgAvatar = dynamic_cast< vgui::ImagePanel* >( FindChildByName( "PnlGamerPic" ) );
		vgui::ImagePanel *pnlModPic = dynamic_cast< vgui::ImagePanel* >( FindChildByName( "PnlModPic" ) );

		if ( GetFullInfo().mInfoType == FGT_PUBLICGAME && pnlModPic )
		{
			SetControlVisible( "ImgAvatarBG", false );

			// TODO: Show wrench icon for custom missions/campaigns
			/*
			char const *szCampaign = GetFullInfo().mpGameDetails->GetString( "game/campaign", "" );
			KeyValues *pCampaignInfo = ( szCampaign && *szCampaign ) ? g_pMatchExtSwarm->GetAllMissions()->FindKey( szCampaign ) : NULL;
			if ( pCampaignInfo )
			{
				if ( pCampaignInfo->GetInt( "builtin" ) )
				{
					//pnlModPic->SetImage( "icon_l4d" );
					pnlModPic->SetVisible( false );
				}
				else
				{
					pnlModPic->SetImage( "icon_modwrench" );
					pnlModPic->SetVisible( true );
				}
			}
			else
			{
				pnlModPic->SetImage( "icon_download" );
				pnlModPic->SetVisible( true );
			}
			*/

			
			imgAvatar->SetVisible( false );
		}
		else if ( imgAvatar )
		{
			if ( xuid != 0 )
			{
				IImage *pImage = NULL;
				
				if ( m_FullInfo.mInfoType == FoundGameListItem::FGT_PLAYER )
					pImage = CUIGameData::Get()->GetAvatarImage( xuid );

				if ( pImage )
				{
					imgAvatar->SetImage( pImage );
				}
				else
				{
					imgAvatar->SetImage( "icon_lobby" );
				}

				SetControlVisible( "ImgAvatarBG", ( pImage != NULL ) );
			}
			else
			{
				imgAvatar->SetImage( "icon_lobby" );
				SetControlVisible( "ImgAvatarBG", false );
			}

			imgAvatar->SetVisible( true );
			pnlModPic->SetVisible( false );
		}
	}
}

void FoundGameListItem::SetSweep( bool sweep )
{
	m_sweep = sweep;
}

bool FoundGameListItem::IsSweep() const
{
	return m_sweep;
}

void FoundGameListItem::SetGameIndex( const Info& fi )
{
	if ( m_FullInfo.mpGameDetails != fi.mpGameDetails )
	{
		if ( m_FullInfo.mpGameDetails )
			m_FullInfo.mpGameDetails->deleteThis();
		
		m_FullInfo = fi;
		m_FullInfo.mpGameDetails = fi.mpGameDetails ? fi.mpGameDetails->MakeCopy() : NULL;
	}
	else
	{
		m_FullInfo = fi;
	}

	SetAvatarXUID( fi.mFriendXUID );
	SetGamerTag( fi.Name );

	SetGamePing( fi.mPing );

	if ( fi.miPing > 0 )
	{
		SetControlString( "LblPing", VarArgs( "%d", fi.miPing ) );
	}

	if ( IsPC() )
		SetControlVisible( "ImgPingSmall", ( fi.miPing > 0 ) );

	if ( fi.IsDLC() )
	{
		if( m_pLblNotJoinable )
		{
			m_pLblNotJoinable->SetText( "#L4D2360_FoundGames_DLC" );
		}

		SetGamePlayerCount( 0, 0 );
		SetGameDifficulty( "" );
		SetSwarmState( "" );
	}
	else if ( char const *szOtherTitle = fi.IsOtherTitle() )
	{
		if( m_pLblNotJoinable )
		{
			wchar_t convertedString[64];
			g_pVGuiLocalize->ConvertANSIToUnicode( szOtherTitle, convertedString, sizeof( convertedString ) );
			wchar_t finalString[128];
			g_pVGuiLocalize->ConstructString( finalString, sizeof( finalString ), g_pVGuiLocalize->Find( "#L4D360UI_Not_Joinable_Mod" ), 1, convertedString );

			m_pLblNotJoinable->SetText( finalString ); 
		}

		SetGamePlayerCount( 0, 0 );
		SetGameDifficulty( "" );
		SetSwarmState( "" );
	}
	else if ( fi.IsJoinable() || fi.IsDownloadable() )
	{
		int numSlots = fi.mpGameDetails->GetInt( "members/numSlots", 0 );
		int numPlayers = fi.mpGameDetails->GetInt( "members/numPlayers", 0 );

		SetGamePlayerCount( numPlayers , numSlots );

		char const *szMode = fi.mpGameDetails->GetString( "game/mode", "campaign" );
		
		if ( !Q_stricmp( "finale", fi.mpGameDetails->GetString( "game/state", "" ) ) )
		{
			// Display it as in finale, hide the playercount, but it's still joinable if they really want to.
			SetGameDifficulty( "#L4D360UI_WaitScreen_GameInFinale" );
			SetGamePlayerCount( 0, 0 );
		}
		else if( !GameModeHasDifficulty( szMode ) )
		{
			SetGameDifficulty( CFmtStr( "#L4D360UI_Mode_%s", szMode ) );
		}
		else
		{
			char const *szDiff = fi.mpGameDetails->GetString( "game/difficulty", "normal" );
			char chDiffBuffer[64];
			Q_snprintf( chDiffBuffer, sizeof( chDiffBuffer ), "#L4D360UI_Difficulty_%s_%s", szDiff, szMode );
			SetGameDifficulty( chDiffBuffer );
		}

		char const *szDiff = fi.mpGameDetails->GetString( "game/swarmstate", "ingame" );
		Msg( "Adding a server to the list:\n" );
		KeyValuesDumpAsDevMsg( fi.mpGameDetails );
		if ( !Q_stricmp( szDiff, "ingame" ) )
		{
			SetSwarmState( "#L4D360UI_ingame" );
		}
		else
		{
			SetSwarmState( "#L4D360UI_briefing" );
		}
	}
	else
	{
		if( m_pLblNotJoinable )
		{
			char const *szHint = fi.GetNonJoinableShortHint();
			if ( !szHint || !*szHint )
				szHint = "#L4D360UI_Lobby_NotInJoinableGame";

			m_pLblNotJoinable->SetText( szHint );
		}
		
		SetGamePlayerCount( 0, 0 );
		SetGameDifficulty( "" );
	}
}

//=============================================================================
const FoundGameListItem::Info& FoundGameListItem::GetFullInfo()
{
	return m_FullInfo;
}

//=============================================================================
void FoundGameListItem::SetGamerTag( char const* gamerTag )
{
	if( m_pLblPlayerGamerTag )
	{
		m_pLblPlayerGamerTag->SetText( gamerTag ? gamerTag : "" );
	}
}

//=============================================================================
void FoundGameListItem::SetGamePing( Info::GAME_PING ping )
{
	if( m_pImgPing )
	{
		switch( ping )
		{
		case Info::GP_LOW:
			m_pImgPing->SetImage( "icon_con_high" );
			break;

		case Info::GP_MEDIUM:
			m_pImgPing->SetImage( "icon_con_medium" );
			break;

		case Info::GP_HIGH:
			m_pImgPing->SetImage( "icon_con_low" );
			break;

		case Info::GP_SYSTEMLINK:
			m_pImgPing->SetImage( "icon_lan" );
			break;

		case Info::GP_NONE:
			m_pImgPing->SetImage( "" );
			break;
		}
	}
}

//=============================================================================
void FoundGameListItem::SetGameDifficulty( const char *difficultyName )
{
	if( m_pLblDifficulty )
	{
		m_pLblDifficulty->SetText( difficultyName ? difficultyName : "" );
	}
}

//=============================================================================
void FoundGameListItem::SetSwarmState( const char *szSwarmStateText )
{
	if( m_pLblSwarmState )
	{
		m_pLblSwarmState->SetText( szSwarmStateText ? szSwarmStateText : "" );
	}
}

//=============================================================================
void FoundGameListItem::SetGamePlayerCount( int current, int max )
{
	if( m_pLblPlayers )
	{
		if ( GetFullInfo().mInfoType == FGT_PUBLICGAME )
		{
			max = GetFullInfo().mpGameDetails->GetInt( "rollup/game", 0 ) +
				GetFullInfo().mpGameDetails->GetInt( "rollup/lobby", 0 );

			wchar_t finalString[256] = L"";

			const wchar_t *countText = NULL;

			extern ConVar ui_public_lobby_filter_status;
			if ( !Q_stricmp( ui_public_lobby_filter_status.GetString(), "lobby" ) )
			{
				countText = ( max == 1 ) ? 
					g_pVGuiLocalize->Find( "#L4D360UI_FoundGames_LobbyCount_1" ) :
					g_pVGuiLocalize->Find( "#L4D360UI_FoundGames_LobbyCount" );
			}
			else
			{
				countText = ( max == 1 ) ?
					g_pVGuiLocalize->Find( "#L4D360UI_FoundGames_GameCount_1" ) :
					g_pVGuiLocalize->Find( "#L4D360UI_FoundGames_GameCount" );
			}

			if( countText  )
			{
				wchar_t convertedString[256];
				V_snwprintf( convertedString, ARRAYSIZE( convertedString ), L"%d", max );
				g_pVGuiLocalize->ConstructString( finalString, sizeof( finalString ), countText, 1, convertedString );
			}		

			m_pLblPlayers->SetText( finalString );
		}
		else if( current >= 0 && max != 0 )
		{
			char countText[256];
			V_snprintf( countText, 256, "%d/%d", current, max );
			m_pLblPlayers->SetText( countText );
		}
		else
		{
			m_pLblPlayers->SetText( "" );
		}
	}
}

//=============================================================================
void FoundGameListItem::DrawListItemLabel( vgui::Label* label, bool bSmallFont, bool bEastAligned /* = false */ )
{
	int panelWide, panelTall;
	GetSize( panelWide, panelTall );

	if ( label )
	{
		bool bHasFocus = HasFocus() || HasMouseover() || IsSelected();

		Color col( 100, 100, 100, 255 );
		if ( bHasFocus )
		{
			col.SetColor( 255, 255, 255, 255 );
		}

		int x, y;
		wchar_t szUnicode[512];
		
		label->GetText( szUnicode, sizeof( szUnicode ) );
		label->GetPos( x, y );

		HFont drawFont = ( bSmallFont ) ? ( m_hSmallTextFont ) : ( m_hTextFont );
		HFont blurFont = ( bSmallFont ) ? ( m_hSmallTextBlurFont ) : ( m_hTextBlurFont );

		int len = V_wcslen( szUnicode );
		int textWide, textTall;
		surface()->GetTextSize( drawFont, szUnicode, textWide, textTall );	// this is just ballpark numbers as they don't count & characters

		// If we drew labels properly I wouldn't be here on a saturday writing code like this:
		// Cannot ask surface about whole text size as it will skip & characters that can be
		// in player names
		int labelWide = label->GetWide();
		if ( labelWide > 0 )
		{
			textWide = 0;
			HFont wideFont = bHasFocus ? blurFont : drawFont;
			for ( int i=0;i<len;i++ )
			{
				textWide += surface()->GetCharacterWidth( wideFont, szUnicode[i] );

				if ( textWide > labelWide )
				{
					int dotWide = surface()->GetCharacterWidth( wideFont, '.' );
					for ( int k = 3; k -- > 0; )
					{
						if ( i > k )
						{
							textWide += dotWide - surface()->GetCharacterWidth( wideFont, szUnicode[i-k-1] );
							szUnicode[i-k-1] = '.';
						}
					}
					
					szUnicode[i] = 0;
					len = i;

					break;
				}
			}
		}

		// vertical center
		y = ( panelTall - textTall ) / 2;

		if ( bEastAligned )
		{
			x += labelWide - textWide;
		}

		vgui::surface()->DrawSetTextFont( drawFont );
		vgui::surface()->DrawSetTextPos( x, y );
		vgui::surface()->DrawSetTextColor( col );
		vgui::surface()->DrawPrintText( szUnicode, len );

		if ( bHasFocus )
		{
			// draw glow
			int alpha = 60.0f + 30.0f * sin( Plat_FloatTime() * 4.0f );
			vgui::surface()->DrawSetTextColor( Color( 255, 255, 255, alpha ) );
			vgui::surface()->DrawSetTextFont( blurFont );
			vgui::surface()->DrawSetTextPos( x, y );
			vgui::surface()->DrawPrintText( szUnicode, len );
		}
	}
}

//=============================================================================
void FoundGameListItem::PaintBackground()
{
	if ( !m_pListCtrlr->IsPanelItemVisible( this ) )
		return;

	// if we're hilighted, background
	if ( IsSelected() )
	{
		int y;
		int x;
		GetPos( x, y );
		int tall = GetTall() * 0.9;
		y = ( GetTall() - tall ) / 2;
		int wide = GetWide();

		// draw border lines
		surface()->DrawSetColor( Color( 65, 74, 96, 255 ) );
		surface()->DrawFilledRectFade( x, y, x + 0.5f * wide, y+2, 0, 255, true );
		surface()->DrawFilledRectFade( x + 0.5f * wide, y, x + wide, y+2, 255, 0, true );
		surface()->DrawFilledRectFade( x, y+tall-2, x + 0.5f * wide, y+tall, 0, 255, true );
		surface()->DrawFilledRectFade( x + 0.5f * wide, y+tall-2, x + wide, y+tall, 255, 0, true );

		int blotchWide = GetWide();
		int blotchX = 0;
		surface()->DrawFilledRectFade( blotchX, y, blotchX + 0.25f * blotchWide, y+tall, 0, 150, true );
		surface()->DrawFilledRectFade( blotchX + 0.25f * blotchWide, y, blotchX + blotchWide, y+tall, 150, 0, true );
	}

	DrawListItemLabel( m_pLblPing, true, !IsPC() );

	DrawListItemLabel( m_pLblPlayerGamerTag, true );

	// Depending on the game info different labels get rendered in the list
	const Info &fi = m_FullInfo;

	if ( fi.IsDLC() || fi.IsOtherTitle() )
	{
		DrawListItemLabel( m_pLblNotJoinable, true );
	}
	else if ( fi.IsJoinable() || fi.IsDownloadable() )
	{
		DrawListItemLabel( m_pLblDifficulty, true );
		DrawListItemLabel( m_pLblPlayers, true );
		DrawListItemLabel( m_pLblSwarmState, true );
	}
	else
	{
		DrawListItemLabel( m_pLblNotJoinable, true );
	}
}

//=============================================================================
void FoundGameListItem::CmdJoinGame()
{
	if ( !m_FullInfo.IsJoinable() || !m_FullInfo.mpfnJoinGame )
	{
		const char *szShortHint = m_FullInfo.GetNonJoinableShortHint();

		if ( m_FullInfo.IsDLC() )
			szShortHint = "#L4D2360_FoundGames_DLC_Msg";

		if ( char const *szOtherTitle = m_FullInfo.IsOtherTitle() )
		{
			szShortHint = "#L4D2360_JoinError_OtherTitle_Modded";	// waitscreen needs a static str ptr
		}
		
		CBaseModPanel::GetSingleton().PlayUISound( UISOUND_DENY );

		CBaseModFrame *pWaitScreen = CBaseModPanel::GetSingleton().GetWindow( WT_GENERICWAITSCREEN );
		if ( pWaitScreen )
			// Do not show a second waitscreen that can potentially lead to focus loss
			// when a waitscreen will assume that another waitscreen is the caller.
			return;
		
		if ( szShortHint && *szShortHint )
		{
			CUIGameData::Get()->OpenWaitScreen( szShortHint );
			CUIGameData::Get()->CloseWaitScreen( NULL, NULL );
		}
		return;
	}

	CBaseModPanel::GetSingleton().PlayUISound( UISOUND_ACCEPT );

	(*m_FullInfo.mpfnJoinGame)( m_FullInfo );
}

//=============================================================================
void FoundGameListItem::CmdViewGamercard()
{
#ifdef _X360
	int iUserSlot = CBaseModPanel::GetSingleton().GetLastActiveUserId();
	int iUserId = XBX_GetUserId( iUserSlot );

	// Warn a player who tries to access a gamer card without multiplayer priviledges that they may not do so
	if ( IsUserLIVEEnabled( iUserId ) == false )
	{
		bool bReplacementFound = false;
		for ( unsigned int i = 0; i < XBX_GetNumGameUsers(); ++ i )
		{
			if ( IsUserLIVEEnabled( XBX_GetUserId( i ) ) )
			{
				// Swap with the first player that CAN make this request
				iUserId = XBX_GetUserId( i );
				bReplacementFound = true;
				break;
			}
		}

		// No valid user there to call from, so just reject the call
		if ( bReplacementFound == false )
			return;
	}

	if( FGT_PLAYER == m_FullInfo.mInfoType )
	{
		XUID xuidView = m_FullInfo.mpGameDetails->GetUint64( "player/xuidOnline", 0ull );
		if ( !xuidView )
			xuidView = m_FullInfo.mpGameDetails->GetUint64( "player/xuid", 0ull );
		if ( !xuidView )
			xuidView = m_FullInfo.mFriendXUID;

		if ( xuidView )
		{
			CBaseModPanel::GetSingleton().PlayUISound( UISOUND_ACCEPT );
			XShowGamerCardUI( iUserId, xuidView );
		}
	}
#endif // _X360
}

//=============================================================================
void FoundGameListItem::OnKeyCodePressed( KeyCode code )
{
	int iUserSlot = GetJoystickForCode( code );
	CBaseModPanel::GetSingleton().SetLastActiveUserId( iUserSlot );

	switch( GetBaseButtonCode( code ) )
	{
	case KEY_XBUTTON_A:
	case KEY_ENTER:
		CmdJoinGame();
		break;
	case KEY_XBUTTON_Y:
		CmdViewGamercard();
		break;

	case KEY_XSTICK1_RIGHT:
	case KEY_XSTICK2_RIGHT:
	case KEY_XBUTTON_RIGHT:
	case KEY_RIGHT:
		if( m_pListCtrlr->NavigateRight() )
		{
			CBaseModPanel::GetSingleton().PlayUISound( UISOUND_FOCUS );
			NavigateFrom();
		}
		break;

	case KEY_XSTICK1_LEFT:
	case KEY_XSTICK2_LEFT:
	case KEY_XBUTTON_LEFT:
	case KEY_LEFT:
		if( m_pListCtrlr->NavigateLeft() )
		{
			CBaseModPanel::GetSingleton().PlayUISound( UISOUND_FOCUS );
			NavigateFrom();
		}
		break;

	default:
		BaseClass::OnKeyCodePressed( code );
		break;
	}
}

void FoundGameListItem::OnKeyCodeTyped( vgui::KeyCode code )
{
	switch( code )
	{
	case KEY_TAB:
		if( m_pListCtrlr->NavigateDown() )
		{
			CBaseModPanel::GetSingleton().PlayUISound( UISOUND_FOCUS );
			NavigateFrom();
		}
		break;

	default:
		BaseClass::OnKeyCodePressed( code );
		break;
	}
}

//=============================================================================
void FoundGameListItem::OnMousePressed( vgui::MouseCode code )
{
	FlyoutMenu::CloseActiveMenu();
	switch ( code )
	{
	case MOUSE_LEFT:
		m_pListCtrlr->SelectPanelItemByPanel( this );
		break;
	}
	BaseClass::OnMousePressed( code );
}

//=============================================================================
void FoundGameListItem::OnMouseDoublePressed(MouseCode code)
{
/*
	switch ( code )
	{
	case MOUSE_LEFT:
		// act as though the 360 "A" button got pressed
		OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_A, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );
		break;
	}
*/	
}

//=============================================================================
void FoundGameListItem::ApplySettings( KeyValues *inResourceData )
{
	BaseClass::ApplySettings( inResourceData );
}

//=============================================================================
void FoundGameListItem::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	LoadControlSettings( CFmtStr( "Resource/UI/BaseModUI/%s.res", GetName() ) );
	
	//////////////////////////////////////////////////////
	// !!!!! THE ENABLED STATES CONTROL VISIBILITY !!!! //
	//////////////////////////////////////////////////////
	// We paint the controls ourselves to achieve the new look, so they must be non-visible to allow that	
	// Toggle the enabled to make them draw/notdraw

	m_pImgPing = dynamic_cast< vgui::ImagePanel * > ( FindChildByName( "ImgPing" ) );
	if ( m_pImgPing )
	{
		m_pImgPing->SetVisible( false );
	}

	m_pLblPing = dynamic_cast< vgui::Label * > ( FindChildByName( "LblPing" ) );
	if ( m_pLblPing )
	{
		m_pLblPing->SetVisible( false );
	}

	m_pLblPlayerGamerTag = dynamic_cast< vgui::Label * > ( FindChildByName( "LblGamerTag" ) );
	if ( m_pLblPlayerGamerTag )
	{
		m_pLblPlayerGamerTag->SetVisible( false );
	}
	m_pLblDifficulty = dynamic_cast< vgui::Label * > ( FindChildByName( "LblDifficulty" ) );
	if ( m_pLblDifficulty )
	{
		m_pLblDifficulty->SetVisible( false );
	}
	m_pLblSwarmState = dynamic_cast< vgui::Label * > ( FindChildByName( "LblSwarmState" ) );
	if ( m_pLblSwarmState )
	{
		m_pLblSwarmState->SetVisible( false );
	}
	m_pLblPlayers = dynamic_cast< vgui::Label * > ( FindChildByName( "LblNumPlayers" ) );
	if ( m_pLblPlayers )
	{
		m_pLblPlayers->SetVisible( false );
	}

	m_pLblNotJoinable = dynamic_cast< vgui::Label * > ( FindChildByName( "LblNotJoinable" ) );
	if ( m_pLblNotJoinable )
	{
		m_pLblNotJoinable->SetVisible( false );
	}
	
	m_hTextFont = pScheme->GetFont( "Default", true );
	m_hTextBlurFont = pScheme->GetFont( "DefaultBlur", true );

	m_hSmallTextFont = pScheme->GetFont( "DefaultMedium", true );
	m_hSmallTextBlurFont = pScheme->GetFont( "DefaultMediumBlur", true );

	// Parse our own info again now that we have controls
	SetGameIndex( m_FullInfo );
}

//=============================================================================
void FoundGameListItem::PerformLayout()
{
	BaseClass::PerformLayout();

	// set all our children (image panel and labels) to not accept mouse input so they
	// don't eat any mouse input and it all goes to us
	for ( int i = 0; i < GetChildCount(); i++ )
	{
		Panel *panel = GetChild( i );
		Assert( panel );
		panel->SetMouseInputEnabled( false );
	}
}

//=============================================================================
void FoundGameListItem::OnCursorEntered() 
{ 
	if( GetParent() )
	{
		GetParent()->NavigateToChild( this );
	}
	else
	{
		NavigateTo();
	}
}

void FoundGameListItem::NavigateTo( void )
{
#ifdef _X360
	m_pListCtrlr->SelectPanelItemByPanel( this );
#else
	SetHasMouseover( true );
	RequestFocus();
#endif
	BaseClass::NavigateTo();
}

void FoundGameListItem::NavigateFrom( void )
{
	SetHasMouseover( false );
	BaseClass::NavigateFrom();
#ifdef _X360
	OnClose();
#endif
}


#ifdef _X360
void FoundGames::NavigateTo()
{
	BaseClass::NavigateTo();

	m_GplGames->NavigateTo();
}
#endif


//=============================================================================
//
//=============================================================================

//=============================================================================
FoundGames::FoundGames( Panel *parent, const char *panelName ):
	BaseClass( parent, panelName, true, true, false ),
	m_flSearchStartedTime( 0.0f ), m_flSearchEndTime( 0.0f ),
	m_pDataSettings( NULL ),
	m_pPreviousSelectedItem( NULL ),
	m_flScheduledUpdateGameDetails( 0.0f )
{
	SetProportional( true );
	SetPaintBackgroundEnabled( true );

	m_pHeaderFooter = new CNB_Header_Footer( this, "HeaderFooter" );
	m_pHeaderFooter->SetTitle( "" );
	m_pHeaderFooter->SetHeaderEnabled( false );
	m_pHeaderFooter->SetFooterEnabled( true );
	m_pHeaderFooter->SetGradientBarEnabled( true );
	m_pHeaderFooter->SetGradientBarPos( 80, 315 );

	m_pTitle = new vgui::Label( this, "Title", "" );


	m_GplGames = new GenericPanelList( this, "GplGames", GenericPanelList::ISM_PERITEM );
	m_GplGames->SetPaintBackgroundEnabled( true );

	m_LastEngineSpinnerTime = 0.0f;
	m_CurrentSpinnerValue = 0;

	SetLowerGarnishEnabled( true );

	OnItemSelected( NULL );

	SetDeleteSelfOnClose( true );
}

//=============================================================================
FoundGames::~FoundGames()
{
	g_pMatchFramework->GetEventsSubscription()->Unsubscribe( this );

	if ( m_pDataSettings )
		m_pDataSettings->deleteThis();
	m_pDataSettings = NULL;

	delete m_GplGames;

	RemoveFrameListener( this );
}

//=============================================================================
void FoundGames::Activate()
{
	BaseClass::Activate();

	AddFrameListener( this );

	UpdateGameDetails();
	m_GplGames->NavigateTo();

	UpdateFooterButtons();

	if ( BaseModHybridButton *pWndCreateGame = dynamic_cast< BaseModHybridButton * >( FindChildByName( "DrpCreateGame" ) ) )
	{
		pWndCreateGame->SetVisible( CanCreateGame() );
		pWndCreateGame->SetText( CFmtStr( "#L4D360UI_FoudGames_CreateNew_%s", "campaign" ) );
	}
	if ( CNB_Button *pWndCreateGame = dynamic_cast< CNB_Button * >( FindChildByName( "BtnCreateNewGame" ) ) )
	{
		pWndCreateGame->SetVisible( CanCreateGame() );
		pWndCreateGame->SetText( CFmtStr( "#L4D360UI_FoudGames_CreateNew_%s", "campaign" ) );
	}

	if ( Panel *pLabelX = FindChildByName( "LblPressX" ) )
		pLabelX->SetVisible( CanCreateGame() );
}

//=============================================================================
void FoundGames::OnCommand( const char *command )
{
	if( V_strcmp( command, "Back" ) == 0 )
	{
		// Act as though 360 B button was pressed
		OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_B, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );
	}
	else if( V_strcmp( command, "CreateGame" ) == 0 )
	{
		if ( !CanCreateGame() )
		{
			CBaseModPanel::GetSingleton().PlayUISound( UISOUND_INVALID );
			return;
		}

		KeyValues *pSettings = KeyValues::FromString(
			"settings",
			" system { "
				" network LIVE "
				" access friends "
			" } "
			" game { "
				" mode = "
				" campaign = "
				" mission = "
			" } "
			" options { "
				" action create "
			" } "
			);
		KeyValues::AutoDelete autodelete( pSettings );

		char const *szGameMode = "campaign";
		pSettings->SetString( "game/mode", szGameMode );
		pSettings->SetString( "game/campaign", "jacob" );
		pSettings->SetString( "game/mission", "asi-jac1-landingbay_01" );

		if ( !CUIGameData::Get()->SignedInToLive() )
		{
			pSettings->SetString( "system/network", "lan" );
			pSettings->SetString( "system/access", "public" );
		}

		if ( StringHasPrefix( szGameMode, "team" ) )
		{
			pSettings->SetString( "system/netflag", "teamlobby" );
		}
		else if ( !Q_stricmp( "custommatch", m_pDataSettings->GetString( "options/action", "" ) ) )
		{
			pSettings->SetString( "system/access", "public" );
		}

		// TCR: We need to respect the default difficulty
		pSettings->SetString( "game/difficulty", GameModeGetDefaultDifficulty( szGameMode ) );

		CBaseModPanel::GetSingleton().PlayUISound( UISOUND_ACCEPT );
		CBaseModPanel::GetSingleton().CloseAllWindows();
		CBaseModPanel::GetSingleton().OpenWindow( WT_GAMESETTINGS, NULL, true, pSettings );
	}
	else if ( V_strcmp( command, "JoinSelected" ) == 0 )
	{
		FoundGameListItem *pSelectedItem = 	static_cast< FoundGameListItem * >( m_GplGames->GetSelectedPanelItem() );
		if ( pSelectedItem )
		{
			PostMessage( pSelectedItem, new KeyValues( "JoinGame" ) );
		}
	}
	else if ( !V_strcmp( command, "DownloadSelected" ) || !V_strcmp( command, "Website" ) )
	{
		FoundGameListItem *pSelectedItem = 	static_cast< FoundGameListItem * >( m_GplGames->GetSelectedPanelItem() );
		if ( pSelectedItem )
		{
			// Open the download window
			KeyValues *pSelectedDetails = pSelectedItem->GetFullInfo().mpGameDetails;
			if ( pSelectedDetails )
			{
				pSelectedDetails->SetString( "game/missioninfo/from", "Join" );
				pSelectedDetails->SetString( "game/missioninfo/action", command );
			}
			CBaseModPanel::GetSingleton().OpenWindow( WT_DOWNLOADCAMPAIGN, this, true, pSelectedDetails );
		}
	}
	else if ( V_strcmp( command, "PlayerDropDown" ) == 0 )
	{
		DropDownMenu *pDrpPlayer = dynamic_cast< DropDownMenu * > ( FindChildByName( "DrpSelectedPlayerName" ) );
		if ( pDrpPlayer )
		{
			BaseModHybridButton *pBtnPlayerGamerTag = dynamic_cast< BaseModHybridButton * > ( pDrpPlayer->FindChildByName( "BtnSelectedPlayerName" ) );
			if ( pBtnPlayerGamerTag )
			{
				int x, y;
				pBtnPlayerGamerTag->GetPos( x, y );
				int tall = pBtnPlayerGamerTag->GetTall();
				pBtnPlayerGamerTag->LocalToScreen( x, y );
				OpenPlayerFlyout( pBtnPlayerGamerTag, m_SelectedGamePlayerID, x, y + tall + 1 );
			}
		}
	}
	else if ( V_strcmp( command, "#L4D360UI_SendMessage" ) == 0 )
	{
		CBaseModPanel::GetSingleton().PlayUISound( UISOUND_ACCEPT );
		char steamCmd[64];
		Q_snprintf( steamCmd, sizeof( steamCmd ), "chat/%llu", m_SelectedGamePlayerID );
		CUIGameData::Get()->ExecuteOverlayCommand( steamCmd );
	}
	else if ( V_strcmp( command, "#L4D360UI_ViewSteamID" ) == 0 )
	{
		CBaseModPanel::GetSingleton().PlayUISound( UISOUND_ACCEPT );
		char steamCmd[64];
		Q_snprintf( steamCmd, sizeof( steamCmd ), "steamid/%llu", m_SelectedGamePlayerID );
		CUIGameData::Get()->ExecuteOverlayCommand( steamCmd );
	}
}

void FoundGames::OnEvent( KeyValues *pEvent )
{
	char const *szName = pEvent->GetName();
	
	if ( !Q_stricmp( "OnMatchPlayerMgrUpdate", szName ) )
	{
		char const *szUpdate = pEvent->GetString( "update", "" );
		if ( !Q_stricmp( "searchstarted", szUpdate ) )
		{
			m_flSearchStartedTime = Plat_FloatTime();
			m_flSearchEndTime = m_flSearchStartedTime + ui_foundgames_spinner_time.GetFloat();
			OnThink();
		}
		else if ( !Q_stricmp( "searchfinished", szUpdate ) )
		{
			m_flSearchStartedTime = 0.0f;
			UpdateGameDetails();
		}
		else if ( !Q_stricmp( "friend", szUpdate ) )
		{
			// Friend's game details have been updated
			if ( !m_flScheduledUpdateGameDetails )
				m_flScheduledUpdateGameDetails = Plat_FloatTime();
		}
	}
}

//=============================================================================
void FoundGames::OpenPlayerFlyout( BaseModHybridButton *button, uint64 playerId, int x, int y )
{
#ifdef NO_STEAM
	Error( "FoundGames::OpenPlayerFlyout does not exist on the Xbox 360." );
#else
	if ( playerId == 0 )
		return;

	FlyoutMenu *flyout = NULL;

	CSteamID objSteamId( playerId );
	if ( !objSteamId.IsValid() )
		return;

	if ( !Q_stricmp( "groupserver", m_pDataSettings->GetString( "options/action" ) ) && objSteamId.BClanAccount() )
	{
		flyout = dynamic_cast< FlyoutMenu * >( FindChildByName( "FlmPlayerFlyout_SteamGroup" ) );
	}
	else if ( steamapicontext->SteamFriends()->GetFriendRelationship( playerId ) == k_EFriendRelationshipFriend )
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
#endif
}


//=============================================================================
void FoundGames::OnKeyCodePressed( KeyCode code )
{
	int iUserSlot = GetJoystickForCode( code );
	CBaseModPanel::GetSingleton().SetLastActiveUserId( iUserSlot );

	switch( GetBaseButtonCode( code ) )
	{
	case KEY_XBUTTON_X:
		OnCommand( "CreateGame" );
		break;

	default:
		BaseClass::OnKeyCodePressed( code );
		break;
	}
}

bool FoundGames::CanCreateGame()
{
	//char const *szGameMode = m_pDataSettings->GetString( "game/mode", NULL );
	bool bGroupServerList = !Q_stricmp( "groupserver", m_pDataSettings->GetString( "options/action", "" ) );

	//return ( szGameMode && *szGameMode && !bGroupServerList );
	return !bGroupServerList;
}

void FoundGames::UpdateTitle()
{
	if ( const char *gameMode = m_pDataSettings->GetString( "game/mode", NULL ) )
	{
		gameMode = NoTeamGameMode( gameMode );
		m_pTitle->SetText( CFmtStr( "#L4D360UI_FoundFriendGames_Title_%s", gameMode ) );
		//BaseClass::DrawDialogBackground( CFmtStr( "#L4D360UI_FoundFriendGames_Title_%s", gameMode ), NULL, "#L4D360UI_FoundGames_Description", NULL, NULL );
	}
	else
	{
		m_pTitle->SetText( CFmtStr( "#L4D360UI_FoundGames_AllGames" ) );
		//BaseClass::DrawDialogBackground( "#L4D360UI_FoundGames_AllGames", NULL, "#L4D360UI_FoundGames_Description", NULL, NULL );
	}
}

//=============================================================================
void FoundGames::OnThink()
{
	BaseClass::OnThink();

	UpdateTitle();

	if ( m_flScheduledUpdateGameDetails && m_flScheduledUpdateGameDetails + ui_foundgames_update_time.GetFloat() < Plat_FloatTime() )
	{
		UpdateGameDetails();
	}

	vgui::ImagePanel *pStillSearchingTag = dynamic_cast< vgui::ImagePanel* >( FindChildByName( "SearchingIcon" ) );
	if ( pStillSearchingTag )
	{
		vgui::Label* label = dynamic_cast< vgui::Label* >( FindChildByName( "LblSearching" ) );
		if ( label )
		{
			label->SetVisible( m_GplGames->GetPanelItemCount() == 0 );
		}

		// If we're searching (or haven't reached the top of the spinner) animate the spinner
		if ( m_flSearchStartedTime > 0.0f || Plat_FloatTime() < m_flSearchEndTime ||
			( m_CurrentSpinnerValue % pStillSearchingTag->GetNumFrames() != 1 ) )
		{
			// clock the anim at 10hz
			float time = Plat_FloatTime();
			if ( ( m_LastEngineSpinnerTime + 0.1f ) < time )
			{
				m_LastEngineSpinnerTime = time;
				pStillSearchingTag->SetFrame( m_CurrentSpinnerValue++ );
			}

			pStillSearchingTag->SetAlpha( 255 );

			if ( label )
				label->SetText( "#L4D360UI_FoundGames_Searching" );
		}
		else
		{
			// Fade
			pStillSearchingTag->SetAlpha( 0 );

			if ( label )
			{
				bool bGroupServer = !Q_stricmp( "groupserver", m_pDataSettings->GetString( "options/action", "" ) );
				char const *szGameMode = m_pDataSettings->GetString( "game/mode", "" );
				szGameMode = NoTeamGameMode( szGameMode );
				
				if ( bGroupServer && szGameMode && *szGameMode )
					label->SetText( CFmtStr( "#L4D360UI_FoundGames_NoGamesFound_%s", szGameMode ) );
				else
					label->SetText( "#L4D360UI_FoundGames_NoGamesFound" );
			}
		}

		if ( m_GplGames->GetPanelItemCount() > 0 && label )
		{
			if ( char const *szNonSearchingCaptionText = GetListHeaderText() )
			{
				label->SetText( szNonSearchingCaptionText );
			}
		}
	}
	
	UpdateFooterButtons();
}

//=============================================================================
void FoundGames::PaintBackground()
{
	/*
	if ( const char *gameMode = m_pDataSettings->GetString( "game/mode", NULL ) )
	{
		gameMode = NoTeamGameMode( gameMode );
		BaseClass::DrawDialogBackground( CFmtStr( "#L4D360UI_FoundFriendGames_Title_%s", gameMode ), NULL, "#L4D360UI_FoundGames_Description", NULL, NULL );
	}
	else
	{
		BaseClass::DrawDialogBackground( "#L4D360UI_FoundGames_AllGames", NULL, "#L4D360UI_FoundGames_Description", NULL, NULL );
	}
	*/
}

//=============================================================================
void FoundGames::LoadLayout()
{
	BaseClass::LoadLayout();

	// Re-trigger selection to update game details
	OnItemSelected( "" );
}

void FoundGames::UpdateFooterButtons()
{
#ifdef _X360
	CBaseModFooterPanel *footer = BaseModUI::CBaseModPanel::GetSingleton().GetFooterPanel();
	if( footer )
	{
		bool bNotSelectingListItem = true;
		bool bShouldShowGamerCard = false;

		for( int slotCount = 0; slotCount < m_GplGames->GetPanelItemCount(); ++slotCount )
		{
			FoundGameListItem *listItem = static_cast< FoundGameListItem * >( m_GplGames->GetPanelItem( slotCount ) );
			if ( listItem && listItem->HasFocus() &&
				 FoundGameListItem::FGT_PLAYER == listItem->GetFullInfo().mInfoType )
			{
				bNotSelectingListItem = false;

				if ( listItem->GetFullInfo().mPing != FoundGameListItem::Info::GP_SYSTEMLINK ||
					 listItem->GetFullInfo().mpGameDetails->GetUint64( "player/xuidOnline", 0ull ) )
				{			
					bShouldShowGamerCard = AnyUserConnectedToLIVE();
					break;
				}
			}
		}

		footer->SetButtons( FB_ABUTTON | FB_BBUTTON | ( bShouldShowGamerCard ? FB_YBUTTON : FB_NONE ), FF_ABY_ONLY, false );
		footer->SetButtonText( FB_BBUTTON, bNotSelectingListItem ? "#L4D360UI_Cancel" : "#L4D360UI_Back" );
		footer->SetButtonText( FB_ABUTTON, "#L4D360UI_Select" );
		footer->SetButtonText( FB_YBUTTON, "#L4D360UI_ViewGamerCard" );
	}
#endif
}


//=============================================================================
struct DifficultyToStringItem
{
	const char* m_difficultyStr;
	int m_difficultyNum;
};

void FoundGames::SetFoundDesiredText( bool bFoundGame )
{
	vgui::Label* label = dynamic_cast< vgui::Label* >( FindChildByName( "LblNoGamesFound" ) );
	if( label )
	{
		char const *szGameMode = m_pDataSettings->GetString( "game/mode", NULL );
		if ( !szGameMode )
		{
			label->SetVisible( false );
		}
		else
		{
			label->SetVisible( !bFoundGame && m_GplGames->GetPanelItemCount() > 0 );
			
			szGameMode = NoTeamGameMode( szGameMode );
			label->SetText( CFmtStr( "#L4D360UI_FoundGames_NoGamesFound_%s", szGameMode ) );
		}
	}
}

bool FoundGames::AddGameFromDetails( const FoundGameListItem::Info &fi )
{
	if ( !fi.Name[0] )
		return false;

	// See if already in list
	FoundGameListItem* game = NULL;

	for( int j = 0; j < m_GplGames->GetPanelItemCount(); ++j )
	{
		FoundGameListItem *item	= dynamic_cast< FoundGameListItem* >( m_GplGames->GetPanelItem( j ) );
		if ( !item )
			continue;

		if ( IsADuplicateServer( item, fi ) )
		{
			game = item;
			break;
		}
	}

	if( !game )
	{
		char const *szItemTemplateName = "FoundGameListItem";
		if ( fi.mInfoType == FoundGameListItem::FGT_PUBLICGAME )
			szItemTemplateName = "FoundGameListItemPublic";
		game = m_GplGames->AddPanelItem< FoundGameListItem >( szItemTemplateName );
	}
	else if ( !game->IsSweep() )
	{
		// Game has already been updated with a valid campaign or versus info
		return false;
	}

	game->SetSweep( false ); // No need to remove

	// Make a copy
	FoundGameListItem::Info fiCopy = fi;

	static CGameUIConVarRef cl_names_debug( "cl_names_debug" );
	if ( cl_names_debug.GetInt() )
		Q_strncpy( fiCopy.Name, "WWWWWWWWWWWWWWW", sizeof( fiCopy.Name ) );

	// fix up ping - friends games do not contain useful ping info
	if ( fi.mPing != FoundGameListItem::Info::GP_SYSTEMLINK )
	{
		fiCopy.mPing = FoundGameListItem::Info::GP_NONE;
	}

	game->SetGameIndex( fiCopy );

	return true;
}

bool FoundGames::IsADuplicateServer( FoundGameListItem *item, FoundGameListItem::Info const &fi )
{
	FoundGameListItem::Info const &ii = item->GetFullInfo();

	if ( IsX360() )
	{
		if( ii.mFriendXUID == fi.mFriendXUID ||
			ii.mpGameDetails->GetUint64( "player/xuidOnline", 0ull ) == fi.mFriendXUID ||
			fi.mpGameDetails->GetUint64( "player/xuidOnline", 0ull ) == ii.mFriendXUID )
			return true;
	}
	else
	{
		if ( fi.mFriendXUID && ii.mFriendXUID == fi.mFriendXUID)
			return true;

		uint64 xuidOnline = fi.mpGameDetails->GetUint64( "player/xuidOnline", 0ull );
		if ( xuidOnline && ii.mpGameDetails->GetUint64( "player/xuidOnline", 0ull ) == xuidOnline )
			return true;
	}

	return false;
}

void FoundGames::SetDetailsPanelVisible( bool bIsVisible )
{
	SetControlVisible( "LblCampaign", bIsVisible );
	SetControlVisible( "LblChapter", bIsVisible );
	SetControlVisible( "LblPlayerAccess", bIsVisible );
	SetControlVisible( "LblPlayerAccessText", bIsVisible );
	SetControlVisible( "LblGameDifficulty", bIsVisible );
	SetControlVisible( "LblGameDifficultyText", bIsVisible );
	SetControlVisible( "LblNumPlayers", bIsVisible );
	SetControlVisible( "LblNumPlayersText", bIsVisible );
	SetControlVisible( "LblViewingGames", bIsVisible );
	SetControlVisible( "LblGameStatus", bIsVisible );
	if ( IsX360() )
	{
		SetControlVisible( "LblGameStatus2", bIsVisible );
	}
	SetControlVisible( "LblSelectedPlayerName", bIsVisible );
	SetControlVisible( "ImgSelectedAvatar", bIsVisible );

	if ( !bIsVisible )
	{
		SetControlVisible( "ImgLevelImage", bIsVisible );
		SetControlVisible( "ImgFrame", bIsVisible );
		SetControlVisible( "BtnDownloadSelected", bIsVisible );
		SetControlVisible( "LblNewVersion", bIsVisible );
		SetControlVisible( "BtnJoinSelected", bIsVisible );
		SetControlVisible( "ImgAvatarBG", bIsVisible );
		SetControlVisible( "DrpSelectedPlayerName", bIsVisible );
		SetControlVisible( "IconForwardArrow", bIsVisible );
	}
}

char const *s_sort_pchGameModePrefer = NULL;
static int __cdecl FoundFriendsSortFunc( vgui::Panel* const *a, vgui::Panel* const *b)
{
	FoundGameListItem *fA	= dynamic_cast< FoundGameListItem* >(*a);
	FoundGameListItem *fB	= dynamic_cast< FoundGameListItem* >(*b);

	const BaseModUI::FoundGameListItem::Info &ia = fA->GetFullInfo();
	const BaseModUI::FoundGameListItem::Info &ib = fB->GetFullInfo();

	// If this is just global lister then always sort by gamertags
	// Servers sort by ping first
	const FoundGameListItem::Type_t eServer = FoundGameListItem::FGT_SERVER;
	if ( ia.mInfoType == eServer && ib.mInfoType == eServer && ia.miPing != ib.miPing )
	{
		return ( ia.miPing < ib.miPing ? -1 : 1 );
	}

	if ( !s_sort_pchGameModePrefer )
	{
		// Players sort by gamertags
		if ( int iResult = Q_stricmp( ia.Name, ib.Name ) )
			return iResult;
		else if ( int iResult2 = Q_strcmp( ia.Name, ib.Name ) )
			return iResult2;
	}

	// Ingame first
	if ( ia.mbInGame != ib.mbInGame )
	{
		return ( ia.mbInGame ? -1 : 1 );
	}

	// Not joinable games
	if ( ia.mIsJoinable != ib.mIsJoinable )
	{
		return ( ia.mIsJoinable ? -1 : 1 );
	}

	// Private games
	bool aPrivate = !Q_stricmp( "private", ia.mpGameDetails->GetString( "system/access", "" ) );
	bool bPrivate = !Q_stricmp( "private", ib.mpGameDetails->GetString( "system/access", "" ) );
	if ( aPrivate != bPrivate )
	{
		return aPrivate ? 1 : -1;
	}

	// Games in finale, etc.
	bool aFinale = !Q_stricmp( "finale", ia.mpGameDetails->GetString( "game/state", "" ) );
	bool bFinale = !Q_stricmp( "finale", ib.mpGameDetails->GetString( "game/state", "" ) );
	if ( aFinale != bFinale )
	{
		return aFinale ? 1 : -1;
	}

	// Full games
	int numASlots = ia.mpGameDetails->GetInt( "members/numSlots", 0 );
	int numAPlayers = ia.mpGameDetails->GetInt( "members/numPlayers", 0 );
	
	int numBSlots = ib.mpGameDetails->GetInt( "members/numSlots", 0 );
	int numBPlayers = ib.mpGameDetails->GetInt( "members/numPlayers", 0 );

	bool aFull = ( numASlots == numAPlayers );
	bool bFull = ( numBSlots == numBPlayers );
	if ( aFull != bFull )
	{
		return aFull ? 1 : -1;
	}

	// Then campaign vs. versus
	char const *aGameMode = ia.mpGameDetails->GetString( "game/mode", "" );
	char const *bGameMode = ib.mpGameDetails->GetString( "game/mode", "" );
	if ( s_sort_pchGameModePrefer )
	{
		aGameMode = NoTeamGameMode( aGameMode );
		bGameMode = NoTeamGameMode( bGameMode );
		
		char const *szFilter = NoTeamGameMode( s_sort_pchGameModePrefer );

		bool aPreferGameMode = !Q_stricmp( aGameMode, szFilter );
		bool bPreferGameMode = !Q_stricmp( bGameMode, szFilter );

		if ( aPreferGameMode != bPreferGameMode )
		{
			return aPreferGameMode ? -1 : 1;
		}
	}
	if ( int iCmp = Q_stricmp( aGameMode, bGameMode ) )
	{
		return iCmp;
	}

	// System Link first
	bool aLan = !Q_stricmp( ia.mpGameDetails->GetString( "system/network", "" ), "lan" );
	bool bLan = !Q_stricmp( ib.mpGameDetails->GetString( "system/network", "" ), "lan" );
	if ( aLan != bLan )
	{
		return aLan ? -1 : 1;
	}

	// Better numeric (ms) ping first
	if ( ia.miPing != ib.miPing )
	{
		return ( ia.miPing < ib.miPing ? -1 : 1 );
	}

	// Better enum ping first
	if ( ia.mPing != ib.mPing )
	{
		return ( ia.mPing > ib.mPing ? -1 : 1 );
	}

	// And as last resort by gamertags
	return Q_stricmp( ia.Name, ib.Name );
}

static void HandleJoinPlayerSession( FoundGameListItem::Info const &fi )
{
	if ( fi.mInfoType != FoundGameListItem::FGT_PLAYER )
		return;

	IPlayerFriend *item = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetFriendByXUID( fi.mFriendXUID );
	if ( item )
		item->Join();
}

void FoundGames::StartSearching()
{
	g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->EnableFriendsUpdate( true );
}

void FoundGames::AddServersToList()
{
	IPlayerManager *mgr = g_pMatchFramework->GetMatchSystem()->GetPlayerManager();
	
	int numItems = mgr->GetNumFriends();
	for( int i = 0; i < numItems; ++i )
	{
		IPlayerFriend *item = mgr->GetFriendByIndex( i );
		KeyValues *pGameDetails = item->GetGameDetails();
		
		FoundGameListItem::Info fi;

		fi.mInfoType = FoundGameListItem::FGT_PLAYER;
		Q_strncpy( fi.Name, item->GetName(), sizeof( fi.Name ) );

		fi.mIsJoinable = false;

		fi.mbInGame = item->IsJoinable();

		fi.mPing = fi.GP_HIGH;
		if ( !Q_stricmp( "lan", pGameDetails->GetString( "system/network", "" ) ) )
			fi.mPing = fi.GP_SYSTEMLINK;

		fi.miPing = pGameDetails->GetInt( "system/ping", 0 );

		fi.mpGameDetails = pGameDetails;
		fi.mFriendXUID = item->GetXUID();

		// On X360 check against our registered missions
		KeyValues *pMissionMapInfo = NULL; pMissionMapInfo;

		const char *szModDir = pGameDetails->GetString( "game/dir", "swarm" );
		if ( Q_stricmp( szModDir, COM_GetModDirectory() ) )
		{
			Q_snprintf( fi.mchOtherTitle, sizeof( fi.mchOtherTitle ), szModDir );
		}

		//
		// Check if this is actually a non-joinable game
		//
		int numSlots = fi.mpGameDetails->GetInt( "members/numSlots", 0 );
		if ( fi.IsDLC() || fi.IsOtherTitle() )
		{
			fi.mIsJoinable = false;
		}
		else if ( fi.mbInGame && numSlots > 0 )
		{
			if ( !fi.IsDownloadable() )
			{
				char const *szHint = fi.GetNonJoinableShortHint();
				if ( !*szHint )
				{
					fi.mIsJoinable = true;
					fi.mpfnJoinGame = HandleJoinPlayerSession;
				}
			}
			else
			{
				fi.mIsJoinable = false;
			}
		}
		else
		{
			// This is the case when we technically already know the session ID,
			// but the query to determine session details is still in progress
			// and actually may fail due to firewalls/NATs, so we fake the details
			// as if we do not know that the friend is in game yet.
			fi.mIsJoinable = false;
			fi.mbInGame = false;
		}
		
		AddGameFromDetails( fi );
	}
}

void FoundGames::AddFakeServersToList()
{
	KeyValues *pDetails = KeyValues::FromString( "FakeContent",
		" system { "
			" network LIVE "
			" access public "
		" } "
		" game { "
			" mode campaign "
			" campaign jacob "
			" difficulty normal "
			" state lobby "
		" } "
		" members { "
			" numMachines #int#0 "
			" numPlayers #int#1 "
			" numSlots #int#4 "
		" } "
		);
	KeyValues::AutoDelete autodelete_pDetails( pDetails );

	for ( int k = 0; k < ui_foundgames_fake_count.GetInt(); ++ k )
	{
		int n = k + ui_foundgames_fake_content.GetInt();

		FoundGameListItem::Info fi;
		fi.mInfoType = FoundGameListItem::FGT_PLAYER;

		Q_snprintf( fi.Name, ARRAYSIZE( fi.Name ), "Fake Content #%02d", n );
		fi.mIsJoinable = true;
		fi.mbInGame = true;
		fi.mPing = fi.GP_HIGH;
		fi.miPing = 0;

		fi.mFriendXUID = ( 0xFFFFFFFFull << 32ull ) | ( n << 8 );
		
		fi.mpGameDetails = pDetails;

		fi.mpGameDetails->SetInt( "members/numPlayers", 1 + n%3 );
		fi.mpGameDetails->SetString( "game/mission", "asi-jac2-deima" );

		char const *szGameModes[] = { "campaign", "single_mission" };
		fi.mpGameDetails->SetString( "game/mode", szGameModes[ n % ARRAYSIZE( szGameModes ) ] );

		AddGameFromDetails( fi );
	}
}

void FoundGames::SortListItems()
{
	s_sort_pchGameModePrefer = m_pDataSettings->GetString( "game/mode", NULL );
	m_GplGames->SortPanelItems( FoundFriendsSortFunc );
}

void FoundGames::UpdateGameDetails()
{
	m_flScheduledUpdateGameDetails = 0.0f;
	bool bEmpty = !m_GplGames->GetPanelItemCount();

	// Mark all as stale
	for( int i = 0; i < m_GplGames->GetPanelItemCount(); ++i )
	{
		FoundGameListItem *game	= dynamic_cast< FoundGameListItem* >( m_GplGames->GetPanelItem( i ) );
		if( game )
		{
			game->SetSweep( true );
		}
	}

	if ( ui_foundgames_fake_content.GetBool() )
		AddFakeServersToList();
	else
		AddServersToList();

	// Remove stale games, it's a vector, so removing an item puts the next item into the current index
	for ( int i = 0; i < m_GplGames->GetPanelItemCount(); /* */ )
	{
		FoundGameListItem *game	= dynamic_cast< FoundGameListItem* >( m_GplGames->GetPanelItem( i ) );
		if( game && game->IsSweep() )
		{
			m_GplGames->RemovePanelItem( static_cast<unsigned short>( i ) );
			continue;
		}
		++i;
	}

	//
	// Sort the list
	//
	SortListItems();

	//
	// Update whether we found the game we were looking for
	//
	if ( m_GplGames->GetPanelItemCount() == 0 )
	{
		SetDetailsPanelVisible(false);
		SetFoundDesiredText( false );
	}
	else
	{
		bool foundDesiredGame = false;
		if ( FoundGameListItem *item = dynamic_cast< FoundGameListItem* >( m_GplGames->GetPanelItem( 0 ) ) )
		{
			const FoundGameListItem::Info &fullInfo = item->GetFullInfo();
			
			char const *szListPreferredMode = m_pDataSettings->GetString( "game/mode", "campaign" );
			szListPreferredMode = NoTeamGameMode( szListPreferredMode );
			
			char const *szGameItemMode = fullInfo.mpGameDetails->GetString( "game/mode", "" );
			szGameItemMode = NoTeamGameMode( szGameItemMode );

			// just need a game in progress.  Not filtering by game mode yet.
			foundDesiredGame = fullInfo.mbInGame;	// && !Q_stricmp( szListPreferredMode, szGameItemMode );
		}
		SetFoundDesiredText( foundDesiredGame );
	}

	// Handle adding new item to empty result list
	if ( bEmpty )
		m_GplGames->SelectPanelItem( 0, GenericPanelList::SD_DOWN, true, false );

	// Re-trigger selection to update game details
	OnItemSelected( "" );
}

//=============================================================================
void FoundGames::OnItemSelected( const char* panelName )
{
	if ( !m_bLayoutLoaded )
		return;

	FoundGameListItem* gameListItem = static_cast<FoundGameListItem*>( m_GplGames->GetSelectedPanelItem() );

	bool bChangedSelection = ( gameListItem != m_pPreviousSelectedItem );
	m_pPreviousSelectedItem = gameListItem;

#if !defined( _X360 )

	// Set active state
	for ( int i = 0; i < m_GplGames->GetPanelItemCount(); /* */ )
	{
		FoundGameListItem *pItem = dynamic_cast< FoundGameListItem* >( m_GplGames->GetPanelItem( i ) );

		if ( pItem )
		{
			pItem->SetSelected( pItem == gameListItem );
		}
		++i;
	}

#endif

	const char* campaignName = "#L4D360UI_CampaignName_Unknown";
	
	const char* chapterName = "#L4D360UI_LevelName_Unknown";
	
	char chDifficultyBuffer[64] = {0};
	const char* currentDifficulty = "#L4D360UI_Unknown";
	
	const char* currentSurvivorAccess = "#L4D360UI_Unknown";
	char chImageBuffer[64] = {0};
	const char* chapterImage = "swarm/MissionPics/UnknownMissionPic";

	const char *szDownloadAuthor = NULL;
	const char *szDownloadWebsite = "";
	bool bDownloadableCampaign = false;

	const int stringSize = 256;
	char playerCountText[ stringSize ] = "";

	enum DetailsDisplayType_t { DETAILS_NONE, DETAILS_PRESENCE, DETAILS_INGAME };
	DetailsDisplayType_t eDetails = DETAILS_INGAME;

	vgui::Label *lblChapter = dynamic_cast< vgui::Label* >( FindChildByName( "LblChapter" ) );
	vgui::Label *lblGameStatus = dynamic_cast< vgui::Label* >( FindChildByName( "LblGameStatus" ) );
	vgui::Label *lblGameStatus2 = NULL;
	if ( IsX360() )
	{
		lblGameStatus2 = dynamic_cast< vgui::Label* >( FindChildByName( "LblGameStatus2" ) );
	}
	vgui::Label* lblGameDifficulty = dynamic_cast< vgui::Label* >( FindChildByName( "LblGameDifficulty" ) );
	
	CNB_Button *joinButton = dynamic_cast< CNB_Button* >( FindChildByName( "BtnJoinSelected" ) );
	BaseModUI::BaseModHybridButton *downloadButton = dynamic_cast< BaseModUI::BaseModHybridButton* >( FindChildByName( "BtnDownloadSelected" ) );
	vgui::Label *downloadVersionLabel = dynamic_cast< vgui::Label* >( FindChildByName( "LblNewVersion" ) );
	vgui::ImagePanel *imgAvatar = dynamic_cast< vgui::ImagePanel* >( FindChildByName( "ImgSelectedAvatar" ) );																						   
	DropDownMenu *pDrpPlayer = dynamic_cast< DropDownMenu * > ( FindChildByName( "DrpSelectedPlayerName" ) );
	int bBuiltIn = 0;

	if( gameListItem )
	{
		if( lblGameStatus )
		{
			lblGameStatus->SetText( "" );
		}

		if ( lblGameStatus2 )
		{
			lblGameStatus2->SetText( "" );
		}

		const FoundGameListItem::Info &fi = gameListItem->GetFullInfo();

		if ( !Q_stricmp( "noplayerinfo", m_pDataSettings->GetString( "UI/display", "" ) ) )
		{
			if ( imgAvatar )
				imgAvatar->SetVisible( false );
			
			if ( pDrpPlayer )
				pDrpPlayer->SetVisible( false );
		}
		else
		{
			if ( bChangedSelection )
			{
				DropDownMenu *pDrpPlayer = dynamic_cast< DropDownMenu * > ( FindChildByName( "DrpSelectedPlayerName" ) );
				if ( pDrpPlayer )
				{
					pDrpPlayer->SetVisible( true );

					BaseModHybridButton *pBtnPlayerGamerTag = dynamic_cast< BaseModHybridButton * > ( pDrpPlayer->FindChildByName( "BtnSelectedPlayerName" ) );
					if ( pBtnPlayerGamerTag )
					{
						pBtnPlayerGamerTag->SetText( fi.Name );

						FlyoutMenu *flyout = dynamic_cast< FlyoutMenu * >( FindChildByName( "FlmPlayerFlyout" ) );
						if ( flyout && flyout->IsVisible() )
						{
							flyout->CloseMenu( pBtnPlayerGamerTag );
						}

						flyout = dynamic_cast< FlyoutMenu * >( FindChildByName( "FlmPlayerFlyout_NotFriend" ) );
						if ( flyout && flyout->IsVisible() )
						{
							flyout->CloseMenu( pBtnPlayerGamerTag );
						}

						pBtnPlayerGamerTag->SetShowDropDownIndicator( fi.mInfoType == FoundGameListItem::FGT_PLAYER );
					}
				}
			}

			m_SelectedGamePlayerID = fi.mFriendXUID;

			if ( imgAvatar )
			{
				IImage *pImage = NULL;

				if ( fi.mInfoType == FoundGameListItem::FGT_PLAYER )
					pImage = CUIGameData::Get()->GetAvatarImage( fi.mFriendXUID );

				if ( pImage )
				{
					imgAvatar->SetImage( "" );	// must be cleared - setting an IImage doesn't reset this.
					imgAvatar->SetImage( pImage );
					SetControlVisible( "ImgAvatarBG", true );
				}
				else if ( fi.mPing == FoundGameListItem::Info::GP_SYSTEMLINK || fi.mInfoType == FoundGameListItem::FGT_SERVER )
				{
					imgAvatar->SetImage( "icon_lan" );
					SetControlVisible( "ImgAvatarBG", false );
				}
				else
				{
					imgAvatar->SetImage( "icon_lobby" );	// must be cleared - setting an IImage doesn't reset this.
					SetControlVisible( "ImgAvatarBG", false );
				}
			}
		}

		if ( fi.IsDLC() )
		{
			currentDifficulty = NULL;
			currentSurvivorAccess = NULL;
			// TODO:
			//chapterImage = "maps/addon";
			if ( lblGameStatus )
				lblGameStatus->SetText( "" );
		}
		else if ( char const *szOtherTitle = fi.IsOtherTitle() )
		{
			campaignName = "";
			chapterName = "";

			currentDifficulty = NULL;
			currentSurvivorAccess = NULL;
			Q_snprintf( chImageBuffer, sizeof( chImageBuffer ), "swarm/MissionPics/addonMissionPic" );
			chapterImage = chImageBuffer;
			if ( lblGameStatus )
				lblGameStatus->SetText( "" );
		}
		else if( fi.mbInGame )
		{
			chapterName = "";
			IASW_Mission_Chooser_Source *pSource = missionchooser ? missionchooser->LocalMissionSource() : NULL;
			const char *szDetailsMissionName = fi.mpGameDetails->GetString( "game/mission", "" );
			KeyValues *pMissionKeys = NULL;
			if ( pSource && szDetailsMissionName && szDetailsMissionName[0] )
			{
				pMissionKeys = pSource->GetMissionDetails( szDetailsMissionName );
			}

			if ( pMissionKeys )
			{
				if ( pMissionKeys->GetString( "image", NULL ) )
				{
					chapterImage = pMissionKeys->GetString( "image" );
				}
				campaignName = pMissionKeys->GetString( "displaytitle" );
				szDownloadAuthor = pMissionKeys->GetString( "author", szDownloadAuthor );
				szDownloadWebsite = pMissionKeys->GetString( "website", szDownloadWebsite );
			}
			else
			{
				campaignName = fi.mpGameDetails->GetString( "game/missioninfo/displaytitle", "#L4D360UI_CampaignName_Unknown" );
				bDownloadableCampaign = true;
			}

			campaignName = fi.mpGameDetails->GetString( "game/missioninfo/displaytitle", campaignName );
			szDownloadAuthor = fi.mpGameDetails->GetString( "game/missioninfo/author", szDownloadAuthor );
			szDownloadWebsite = fi.mpGameDetails->GetString( "game/missioninfo/website", szDownloadWebsite );
			bBuiltIn = fi.mpGameDetails->GetInt( "game/missioninfo/builtin", 0 );

			if ( bBuiltIn )
				szDownloadWebsite = "";	// no website access for builtin campaigns

			char const *szGameMode = fi.mpGameDetails->GetString( "game/mode", "" );
			if( !GameModeHasDifficulty( szGameMode ) )
			{
				Q_snprintf( chDifficultyBuffer, sizeof( chDifficultyBuffer ), "#L4D360UI_Mode_%s", szGameMode );
				currentDifficulty = chDifficultyBuffer;
			}
			else
			{
				currentDifficulty = fi.mpGameDetails->GetString( "game/difficulty", "normal" );
				Q_snprintf( chDifficultyBuffer, sizeof( chDifficultyBuffer ), "#L4D360UI_Difficulty_%s_%s", currentDifficulty, szGameMode );
				currentDifficulty = chDifficultyBuffer;
			}

			char const *szAccess = fi.mpGameDetails->GetString( "system/access", "public" );

			if ( !Q_stricmp( "private", szAccess ) )
				currentSurvivorAccess = "#L4D360UI_Access_Invite";
			else if ( !Q_stricmp( "friends", szAccess ) )
				currentSurvivorAccess = "#L4D360UI_Access_Friends";
			else
				currentSurvivorAccess = "#L4D360UI_Access_Public";

			int numSlots = fi.mpGameDetails->GetInt( "members/numSlots", 0 );
			int numPlayers = fi.mpGameDetails->GetInt( "members/numPlayers", 0 );
			if ( !numSlots )
				Q_snprintf( playerCountText, stringSize, "1" );
			else
				Q_snprintf( playerCountText, stringSize, "%d/%d", numPlayers, numSlots );

			if ( !Q_stricmp( "offline", fi.mpGameDetails->GetString( "system/network" ) ) )
			{
				playerCountText[0] = 0;
				currentSurvivorAccess = ( numPlayers > 1 ) ? "#L4D360UI_Mode_offline_SS" : "#L4D360UI_Mode_offline_SP";
			}

			if ( lblGameStatus && ( fi.mInfoType == FoundGameListItem::FGT_PUBLICGAME ) )
			{
				int numLobbies = fi.mpGameDetails->GetInt( "rollup/lobby", 0 );
				int numGames = fi.mpGameDetails->GetInt( "rollup/game", 0 );

				wchar_t finalString[256] = L"";
				wchar_t lobbyString[256] = L"";
				wchar_t gamesString[256] = L"";
				wchar_t numInLobbies[13], numInGame[13];
				V_snwprintf( numInLobbies, ARRAYSIZE( numInLobbies ), L"%d", numLobbies );
				V_snwprintf( numInGame, ARRAYSIZE( numInGame ), L"%d", numGames );

				// Lobbies
				const wchar_t *countText = NULL;
				countText = ( numLobbies == 1 ) ?	
					g_pVGuiLocalize->Find( "#L4D360UI_FoundGames_LobbyCount_1" ) :
					g_pVGuiLocalize->Find( "#L4D360UI_FoundGames_LobbyCount" );
				if ( countText )
				{
					g_pVGuiLocalize->ConstructString( lobbyString, sizeof( lobbyString ), countText, 1, numInLobbies );
				}

				// Games in Progress
				countText = ( numGames == 1 ) ?
					g_pVGuiLocalize->Find( "#L4D360UI_FoundGames_GameCountInProgress_1" ) :
					g_pVGuiLocalize->Find( "#L4D360UI_FoundGames_GameCountInProgress" );
				if ( countText )
				{
					g_pVGuiLocalize->ConstructString( gamesString, sizeof( gamesString ), countText, 1, numInGame );
				}

				if ( IsX360() )
				{
					// Split games and lobbies into two lines
					lblGameStatus->SetText( lobbyString );
					if ( lblGameStatus2 )
					{
						lblGameStatus2->SetText( gamesString );
					}
				}
				else
				{
					// merge games and lobbies
					const wchar_t *statusText =	g_pVGuiLocalize->Find( "#L4D360UI_FoundPublicGames_Status" );
					if ( statusText )
					{
						g_pVGuiLocalize->ConstructString( finalString, sizeof( finalString ), statusText, 2, lobbyString, gamesString );
					}
					lblGameStatus->SetText( finalString );
				}

				if ( numLobbies + numGames == 0 )
				{
					lblGameStatus->SetText( "#L4D360UI_FoundPublicGames_Status_None" );

					if ( lblGameStatus2 )
					{
						lblGameStatus2->SetText( "" );
					}
				}
			}
			else if( lblGameStatus )
			{
				char const *szNetwork = fi.mpGameDetails->GetString( "system/network", "LIVE" );
				
				char const *szGameState = fi.mpGameDetails->GetString( "game/state", "lobby" );
				if ( !szGameState || !*szGameState )
					szGameState = "game";

				char chStatusTextBuffer[128];
				Q_snprintf( chStatusTextBuffer, sizeof( chStatusTextBuffer ), "#L4D360UI_FoundGames_%sIn%s",
					( !Q_stricmp( szNetwork, "lan" ) ) ? "Syslink" : "XboxLive",
					szGameState );
				lblGameStatus->SetText( chStatusTextBuffer );
			}
		}
		
		if ( !fi.mbInGame || ( eDetails == DETAILS_PRESENCE ) )
		{
			// Friend not in game or in a private game, resort to presence only display
			eDetails = DETAILS_PRESENCE;
			
			// TODO?
			//if ( !fi.IsOtherTitle() )
				//chapterImage = "maps/any";

			currentDifficulty = NULL;
			currentSurvivorAccess = NULL;

			chapterName = NULL; // NOT setting chapter name to "char-string"
			campaignName = "";

			// Tokenize rich presence string
			wchar_t wszRichPresenceCopy[ MAX_RICHPRESENCE_SIZE ];
			wcsncpy( wszRichPresenceCopy, fi.mpGameDetails->GetWString( "player/richpresence", L"" ), ARRAYSIZE( wszRichPresenceCopy ) );
			wszRichPresenceCopy[ MAX_RICHPRESENCE_SIZE - 1 ] = 0;
			wchar_t *pwzLine2 = wszRichPresenceCopy;
			while ( *pwzLine2 != 0 && *pwzLine2 != L'\r' && *pwzLine2 != L'\n' )
				++ pwzLine2;
			while ( *pwzLine2 != 0 && ( *pwzLine2 == L'\r' || *pwzLine2 == L'\n' ) )
				*( pwzLine2 ++ ) = 0;

			if ( lblChapter )
			{
				lblChapter->SetText( wszRichPresenceCopy );
			}
			
			if ( lblGameDifficulty )
			{
				lblGameDifficulty->SetText( pwzLine2 );
			}
		}

		if ( fi.IsDLC() || fi.IsOtherTitle() )
		{
			// Don't do anything below
		}
		else if ( fi.IsDownloadable() )
		{
			if ( joinButton )
			{
				joinButton->SetVisible( false );
				SetControlVisible( "IconForwardArrow", false );
			}
			if ( downloadButton )
			{
				downloadButton->SetVisible( true );
				downloadButton->SetEnabled( !!*szDownloadWebsite );
				//downloadButton->SetHelpText( fi.GetJoinButtonHint(), true );
			}
			if ( downloadVersionLabel )
			{
				downloadVersionLabel->SetVisible( !bDownloadableCampaign );
			}
		}
		else if ( joinButton )
		{
			if ( downloadButton )
			{
				downloadButton->SetVisible( false );
			}
			if ( downloadVersionLabel )
			{
				downloadVersionLabel->SetVisible( false );
			}

			bool bGameJoinable = fi.IsJoinable();
			if ( !playerCountText[0] )
				bGameJoinable = false;	// single player games or offline

			joinButton->SetVisible( true );
			joinButton->SetEnabled( bGameJoinable );
			//joinButton->SetHelpText( fi.GetJoinButtonHint(), bGameJoinable );

			SetControlVisible( "IconForwardArrow", bGameJoinable );
		}
		else if ( !IsX360() )
		{
			eDetails = DETAILS_NONE;

			if ( downloadButton )
			{
				downloadButton->SetVisible( false );
			}
			if ( downloadVersionLabel )
			{
				downloadVersionLabel->SetVisible( false );
			}
			if ( joinButton )
			{
				joinButton->SetVisible( false );
			}

			SetControlVisible( "IconForwardArrow", false );
		}
	}

	vgui::Label *lblCampaign = dynamic_cast< vgui::Label* >( FindChildByName( "LblCampaign" ) );
	if( lblCampaign )
	{
		lblCampaign->SetText( campaignName );
	}

	if( lblChapter && chapterName )
	{
		lblChapter->SetText( chapterName );
	}

	//
	// Image
	//
	if ( chapterImage )
	{
		if( vgui::ImagePanel *imgLevelImage = dynamic_cast< vgui::ImagePanel* >( FindChildByName( "ImgLevelImage" ) ) )
			imgLevelImage->SetImage( chapterImage );

		SetControlVisible( "ImgLevelImage", true );
		SetControlVisible( "ImgFrame", true );
	}
	else
	{
		SetControlVisible( "ImgLevelImage", false );
		SetControlVisible( "ImgFrame", false );
	}
	
	wchar_t finalString[MAX_PATH] = L"";
	wchar_t convertedString[MAX_PATH] = L"";

	vgui::Label *lblAuthor = dynamic_cast< vgui::Label* >( FindChildByName( "LblAuthor" ) );
	if( lblAuthor )
	{
		finalString[0] = 0;
		if ( szDownloadAuthor && *szDownloadAuthor )
		{
			const wchar_t * authorFormat = g_pVGuiLocalize->Find( "#L4D360UI_FoundGames_Author" );
			g_pVGuiLocalize->ConvertANSIToUnicode( szDownloadAuthor, convertedString, sizeof( convertedString ) );
			if ( authorFormat )
			{
				g_pVGuiLocalize->ConstructString( finalString, sizeof( finalString ), authorFormat, 1, convertedString );
			}
		}
		
		lblAuthor->SetText( finalString );
		lblAuthor->SetVisible( finalString[0] != 0 );
		

		//lblAuthor->SetText( "test" );
		//lblAuthor->SetFgColor( Color( 255, 255, 255, 255 ) );
		//lblAuthor->SetVisible( true );

		//Msg( "author zpos = %d\n", lblAuthor->GetZPos() );
	}

	BaseModHybridButton *btnWebsite = dynamic_cast< BaseModHybridButton* >( FindChildByName( "BtnWebsite" ) );
	if( btnWebsite )
	{
		finalString[0] = 0;
		if ( *szDownloadWebsite )
		{
			const wchar_t * websiteFormat = g_pVGuiLocalize->Find( "#L4D360UI_FoundGames_WebSite" );
			g_pVGuiLocalize->ConvertANSIToUnicode( szDownloadWebsite, convertedString, sizeof( convertedString ) );
			if ( websiteFormat )
			{
				g_pVGuiLocalize->ConstructString( finalString, sizeof( finalString ), websiteFormat, 1, convertedString );
			}
		}

		btnWebsite->SetText( finalString );
		btnWebsite->SetVisible( finalString[0] != 0 );
	}

	vgui::Label *lblAccess = dynamic_cast< vgui::Label* >( FindChildByName( "LblPlayerAccess" ) );
	if( lblAccess )
	{
		lblAccess->SetText( currentSurvivorAccess ? currentSurvivorAccess : "" );
	}

	if( lblGameDifficulty && currentDifficulty )
	{
		lblGameDifficulty->SetText( currentDifficulty );
	}

	vgui::Label* lblNumPlayers = dynamic_cast< vgui::Label* >( FindChildByName( "LblNumPlayers" ) );
	if( lblNumPlayers && ( eDetails == DETAILS_INGAME ) )
	{
		finalString[0] = 0;
		const wchar_t * playersFormat = g_pVGuiLocalize->Find( "#L4D360UI_FoundGames_Players" );
		if( playersFormat && playerCountText[0] )
		{
			g_pVGuiLocalize->ConvertANSIToUnicode( playerCountText, convertedString, sizeof( convertedString ) );
			g_pVGuiLocalize->ConstructString( finalString, sizeof( finalString ), playersFormat, 1, convertedString );
		}		

		lblNumPlayers->SetText( finalString );
	}
	else if( lblNumPlayers )
	{
		lblNumPlayers->SetText( "" );
	}

	vgui::Label *lblViewingGames = dynamic_cast< vgui::Label* >( FindChildByName( "LblViewingGames" ) );
	if( lblViewingGames )
	{
		if( m_GplGames->GetPanelItemCount() )
		{
			char viewingGames[ 128 ];
			int firstItem = m_GplGames->GetFirstVisibleItemNumber()+1;
			int lastItem = m_GplGames->GetLastVisibleItemNumber()+1;
			lastItem = MIN( lastItem, m_GplGames->GetPanelItemCount() );
			Q_snprintf( viewingGames, 128, "%d-%d", firstItem, lastItem );
			lblViewingGames->SetVisible( true );
			lblViewingGames->SetText( viewingGames );
		}
		else
		{
			lblViewingGames->SetVisible( false );
		}
	}

	if( !gameListItem )
	{
		eDetails = DETAILS_NONE;
	}

#ifndef _X360
	// Are we on the "Play Online" menus?
	if ( CBaseModPanel::GetSingleton().GetWindow( WT_FOUNDPUBLICGAMES ) )
	{
		BaseModUI::BaseModHybridButton *m_pInstallSupportBtn = dynamic_cast< BaseModUI::BaseModHybridButton * >( FindChildByName( "BtnInstallSupport" ) );
		vgui::Label *m_pSupportRequiredDetails = dynamic_cast< vgui::Label * >( FindChildByName( "LblSupportRequiredDetails" ) );
		vgui::Label *lblInstalling = dynamic_cast< vgui::Label* >( FindChildByName( "LblInstalling" ) );
		vgui::Label *lblInstallingDetails = dynamic_cast< vgui::Label* >( FindChildByName( "LblInstallingDetails" ) );

		// If a SDK map is selected
		if ( !bBuiltIn )
		{
			// If the Legacy SDK is NOT installed
			if ( !GetLegacyData::IsInstalled() )
			{
				// If we are not already installing it
				if ( !GetLegacyData::IsInstalling() )
				{
					// Hide all buttons and prompt the user to download the SDK
					if ( joinButton && downloadButton && btnWebsite && m_pSupportRequiredDetails && m_pInstallSupportBtn )
					{
						joinButton->SetVisible( false );
						downloadButton->SetVisible( false );
						btnWebsite->SetVisible( false );
						m_pSupportRequiredDetails->SetVisible( true );
						m_pInstallSupportBtn->SetVisible( true );
					}
					
					SetControlVisible( "IconForwardArrow", false );
				}

			}
			else // The SDK is installed or is installing
			{
				// Hide the SDK install prompt buttons
				if ( m_pSupportRequiredDetails && m_pInstallSupportBtn )
				{
					m_pSupportRequiredDetails->SetVisible( false );
					m_pInstallSupportBtn->SetVisible( false );
				}

				// If we are currently installing, display the install message
				if ( GetLegacyData::IsInstalling() )
				{
					if ( lblInstalling && lblInstallingDetails )
					{
						lblInstalling->SetVisible( true );
						lblInstallingDetails->SetVisible( true );
						//joinButton->SetEnabled( false ); // comment in to force the Join Game button to only be active when the SDK is installed
					}
				}
			}
		}
		else
		{
			// A built-in map is selected. Hide any SDK install prompts
			if ( lblInstalling && lblInstallingDetails && m_pSupportRequiredDetails && m_pInstallSupportBtn )
			{
				lblInstalling->SetVisible( false );
				lblInstallingDetails->SetVisible( false );
				m_pSupportRequiredDetails->SetVisible( false );
				m_pInstallSupportBtn->SetVisible( false );
				//joinButton->SetEnabled( true ); // comment in to force the Join Game button to only be active when the SDK is installed
			}
		}
	}
#endif

	SetDetailsPanelVisible( eDetails != DETAILS_NONE );
}

//=============================================================================
void FoundGames::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	SetPaintBackgroundEnabled( true );
	SetupAsDialogStyle();

	m_pPreviousSelectedItem = NULL;

	// Subscribe to the matchmaking events
	g_pMatchFramework->GetEventsSubscription()->Subscribe( this );

	Activate();

	if( m_GplGames && m_ActiveControl != m_GplGames )
	{
		if ( m_ActiveControl )
			m_ActiveControl->NavigateFrom();

		m_GplGames->NavigateTo();
		m_ActiveControl = m_GplGames;
	}
}

//=============================================================================
FoundGameListItem* FoundGames::GetGameItem( int index )
{
	FoundGameListItem *result = NULL;
	if( index < m_GplGames->GetPanelItemCount() && index >= 0 )
	{
		result = dynamic_cast< FoundGameListItem* >( m_GplGames->GetPanelItem( index ) );
	}

	return result;
}

//=============================================================================
void FoundGames::OnOpen()
{
	SetVisible( true );

	BaseClass::OnOpen();

	m_GplGames->SetScrollBarVisible( IsPC() );

	// trigger an explicit update
	StartSearching();
	UpdateGameDetails();
}

void FoundGames::SetDataSettings( KeyValues *pSettings )
{
	if ( m_pDataSettings )
		m_pDataSettings->deleteThis();
	m_pDataSettings = pSettings ? pSettings->MakeCopy() : NULL;

	BaseClass::SetDataSettings( pSettings );
}

//=============================================================================
void FoundGames::OnClose()
{
	BaseClass::OnClose();

	RemoveFrameListener( this );
}
