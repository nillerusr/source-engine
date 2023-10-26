//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "VGameLobby.h"
#include "KeyValues.h"
#include "VGenericConfirmation.h"
#include "VGenericPanelList.h"
#include "VFooterPanel.h"
#include "gameui_util.h"
#include "GameUI/IGameUI.h"
#include "EngineInterface.h"
#include "VHybridButton.h"
#include "VGameSettings.h"

#include "vgui_controls/Button.h"
#include "vgui_controls/ImagePanel.h"
#include "vgui_controls/ListPanel.h"
#include "vgui_controls/RichText.h"
#include "vgui/ILocalize.h"
#include "vgui/ISurface.h"
#include "vgui/IVGui.h"
#include "FileSystem.h"

#include "fmtstr.h"
#include "smartptr.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace BaseModUI;

ConVar ui_lobby_start_enabled( "ui_lobby_start_enabled", "0", FCVAR_DEVELOPMENTONLY );
ConVar ui_lobby_stat_team_search_max( "ui_lobby_stat_team_search_max", "1800", FCVAR_DEVELOPMENTONLY );
ConVar ui_lobby_stat_switch_time( "ui_lobby_stat_switch_time", "15", FCVAR_DEVELOPMENTONLY );
ConVar ui_lobby_idle_time( "ui_lobby_idle_time", "300", FCVAR_DEVELOPMENTONLY );


//=============================================================================

namespace BaseModUI
{

// Destroying the lobby without any confirmations
static void LeaveLobbyImpl()
{
	g_pMatchFramework->CloseSession();
	CBaseModPanel::GetSingleton().CloseAllWindows();
	CBaseModPanel::GetSingleton().OpenFrontScreen();
}


//////////////////////////////////////////////////////////////////////////
//
// Player list item implementation
//

class CPlayerItem : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CPlayerItem, vgui::EditablePanel );

public:
	CPlayerItem( vgui::Panel *parent, const char *panelName );
	~CPlayerItem();

public:
	virtual void PaintBackground();
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void ApplySettings( KeyValues *inResourceData );

	virtual void OnKeyCodeTyped( KeyCode code );
	virtual void OnKeyCodePressed( KeyCode code );
	virtual void OnCommand( const char *command );

	virtual void NavigateTo( void )
	{
#ifdef _X360
		m_pListCtrlr->SelectPanelItemByPanel( this );
#endif
		BaseClass::NavigateTo();
	}

public:
	void SetPlayerInfo( KeyValues *pPlayer );
	KeyValues * GetPlayerInfo() const;

	void OnPlayerActivity( KeyValues *pEvent );

protected:
	char const * GetVoiceStatus() const;

	void CmdKickPlayer();

protected:
	GenericPanelList *m_pListCtrlr;
	KeyValues *m_pInfo;
	float m_flVoiceTalking;

	vgui::ImagePanel	*m_pImgPlayerPortrait;
	vgui::ImagePanel	*m_pImgGamerPic;
	DropDownMenu        *m_pDrpPlayer;
	BaseModHybridButton  *m_pBtnPlayerGamerTag;
	vgui::Label			*m_pLblPlayerVoiceStatus;
	vgui::Label			*m_pLblPartyIcon;
	vgui::Label			*m_pLblEmptySlotAd;

	vgui::HFont	m_hTextFont;
	vgui::HFont	m_hTextBlurFont;
	
	Color m_bkFadeColor;
	Color m_OutOfFocusBgColor;
	Color m_FocusBgColor;
};

CPlayerItem::CPlayerItem( vgui::Panel *parent, const char *panelName ) :
	BaseClass( parent, panelName ),
	m_pListCtrlr( ( GenericPanelList * ) parent ),
	m_pInfo( NULL ),
	m_flVoiceTalking( 0 ),
	m_pImgPlayerPortrait( NULL ),
	m_pImgGamerPic( NULL ),
	m_pDrpPlayer( NULL ),
	m_pBtnPlayerGamerTag( NULL ),
	m_pLblPlayerVoiceStatus( NULL ),
	m_pLblPartyIcon( NULL ),
	m_pLblEmptySlotAd( NULL )
{
	char chResFile[MAX_PATH] = {0};
	Q_snprintf( chResFile, ARRAYSIZE( chResFile ), "Resource/UI/BaseModUI/%s.res", panelName );
	LoadControlSettings( chResFile );
	
	SetPaintBackgroundEnabled( true );
}

CPlayerItem::~CPlayerItem()
{
	if ( m_pInfo )
		m_pInfo->deleteThis();
	m_pInfo = NULL;
}

void CPlayerItem::OnKeyCodePressed( KeyCode code )
{
	GameLobby* gameLobby = static_cast<GameLobby*>( CBaseModPanel::GetSingleton().GetWindow( WT_GAMELOBBY ) );
	if ( gameLobby )
	{
		gameLobby->NotifyLobbyNotIdleActivity();
	}

	KeyValues *pInfo = GetPlayerInfo();
	if ( !pInfo )
	{
		BaseClass::OnKeyCodePressed( code );
		return;
	}

	XUID xuidPlayer = pInfo->GetUint64( "xuid", 0ull );
	xuidPlayer;

	int iUserSlot = GetJoystickForCode( code );
	BaseModUI::CBaseModPanel::GetSingleton().SetLastActiveUserId( iUserSlot );

	switch ( GetBaseButtonCode( code ) )
	{
	case KEY_XBUTTON_A:
		{
#ifdef _X360
			CBaseModPanel::GetSingleton().PlayUISound( UISOUND_ACCEPT );

			int iSlot = CBaseModPanel::GetSingleton().GetLastActiveUserId();
			int iCtrlr;
			if ( XBX_GetUserIsGuest( iSlot ) )
			{
				iCtrlr = XBX_GetPrimaryUserId();
			}
			else
			{
				iCtrlr = XBX_GetUserId( iSlot );
			}
			XShowGamerCardUI( iCtrlr, SessionMembersFindNonGuestXuid( xuidPlayer ) );
#endif
		}
		return;

	case KEY_XBUTTON_X:
		{
			bool bHost = !Q_stricmp( "host", g_pMatchFramework->GetMatchSession()->GetSessionSystemData()->
				GetString( "type", "host" ) );
			if( bHost )
			{
				CBaseModPanel::GetSingleton().PlayUISound( UISOUND_ACCEPT );
				CmdKickPlayer();
			}
		}
		return;

	case KEY_XBUTTON_Y:
		{
			CBaseModPanel::GetSingleton().PlayUISound( UISOUND_ACCEPT );

#ifdef _X360
			int iSlot = CBaseModPanel::GetSingleton().GetLastActiveUserId();
			int iCtrlr;
			if ( XBX_GetUserIsGuest( iSlot ) )
			{
				iCtrlr = XBX_GetPrimaryUserId();
			}
			else
			{
				iCtrlr = XBX_GetUserId( iSlot );
			}
			XShowPlayerReviewUI( iCtrlr, SessionMembersFindNonGuestXuid( xuidPlayer ) );
#endif
		}
		return;
	}

	BaseClass::OnKeyCodePressed( code );
}

void CPlayerItem::OnKeyCodeTyped( vgui::KeyCode code )
{
	switch( code )
	{
	case KEY_ENTER: //HACK: absorb the command. Somehow the player drop down gets activated twice (which opens then immediately closes the menu) when using the enter key 
		//and the messaging voodoo makes it a pain to track down exactly why (which I just don't have time for right now). This absorbs one of the two and everything magically works again
		break;

	default:
		BaseClass::OnKeyCodePressed(code);
		break;
	}
}

void CPlayerItem::OnCommand( const char *command )
{
	GameLobby* gameLobby = static_cast<GameLobby*>( CBaseModPanel::GetSingleton().GetWindow( WT_GAMELOBBY ) );
	if ( gameLobby )
	{
		gameLobby->NotifyLobbyNotIdleActivity();
	}

	XUID xuidPlayer = GetPlayerInfo()->GetUint64( "xuid", 0ull );

	if ( !xuidPlayer )
	{
		BaseClass::OnCommand( command );
		return;
	}

	if ( !Q_strcmp( command, "PlayerDropDown" ) )
	{
		if ( gameLobby && m_pBtnPlayerGamerTag )
		{
			gameLobby->OpenPlayerFlyout( this );
		}
	}
	else if( !Q_strcmp( command, "#L4D360UI_BootPlayer" ) )
	{
		CmdKickPlayer();
	}
	else if ( !Q_strcmp( command, "#L4D360UI_SendMessage" ) )
	{
		char steamCmd[64];
		Q_snprintf( steamCmd, sizeof( steamCmd ), "chat/%I64u", xuidPlayer );
		CUIGameData::Get()->ExecuteOverlayCommand( steamCmd );
	}
	else if ( !Q_strcmp( command, "#L4D360UI_ViewSteamID" ) )
	{
		char steamCmd[64];
		Q_snprintf( steamCmd, sizeof( steamCmd ), "steamid/%I64u", xuidPlayer );
		CUIGameData::Get()->ExecuteOverlayCommand( steamCmd );
	}
	else if ( !Q_strcmp( command, "#L4D360UI_MutePlayer" ) )
	{
		IMatchVoice *pIMatchVoice = g_pMatchFramework->GetMatchSystem()->GetMatchVoice();
		bool bMuted = pIMatchVoice->IsTalkerMuted( xuidPlayer );
		pIMatchVoice->MuteTalker( xuidPlayer, !bMuted );
	}
	else
	{
		BaseClass::OnCommand( command );
	}
}

void CPlayerItem::CmdKickPlayer()
{
	XUID xuid = GetPlayerInfo()->GetUint64( "xuid", 0ull );
	DevMsg( "Kicking player from lobby: %llx\n", xuid );

	KeyValues *pRequest = new KeyValues( "Kick" );
	KeyValues::AutoDelete autodelete( pRequest );

	pRequest->SetString( "run", "local" );
	pRequest->SetUint64( "xuid", xuid );
	pRequest->SetString( "reason", "Kicked by lobby leader" );

	g_pMatchFramework->GetMatchSession()->Command( pRequest );
}

void CPlayerItem::SetPlayerInfo( KeyValues *pPlayer )
{
	// If setting new information, then make a copy or NULL out
	if ( pPlayer != m_pInfo )
	{
		if ( m_pInfo )
			m_pInfo->deleteThis();
		
		m_pInfo = pPlayer ? pPlayer->MakeCopy() : NULL;
	}

	// If our layout hasn't been loaded yet, then wait
	// until the layout is available
	if ( !m_pImgPlayerPortrait )
		return;

	//
	// Apply player information
	//

	XUID xuid = m_pInfo->GetUint64( "xuid", 0ull );

	if ( m_pImgPlayerPortrait )
	{
		m_pImgPlayerPortrait->SetEnabled( m_pInfo != NULL );

		char const *pszAvatar = NULL;
		if ( xuid )
		{
			pszAvatar = m_pInfo->GetString( "game/avatar", "random" );
		}

		if ( pszAvatar )
		{
			const char *pszAvatarImage = s_characterPortraits->GetTokenI( pszAvatar );
			m_pImgPlayerPortrait->SetImage( vgui::scheme()->GetImage( pszAvatarImage, true ) );
			m_pImgPlayerPortrait->SetVisible( true );
		}
		else
		{
			m_pImgPlayerPortrait->SetVisible( false );
		}
	}

	if ( m_pImgGamerPic && IsPC() )
	{
		IImage *pSteamImage = CUIGameData::Get()->GetAvatarImage( xuid );

		if ( pSteamImage )
		{
			m_pImgGamerPic->SetEnabled( true );
			m_pImgGamerPic->SetImage( pSteamImage ); 
		}
		else if ( m_pImgGamerPic->IsEnabled() )
		{
			m_pImgGamerPic->SetEnabled( false );
		}
	}

	if ( m_pBtnPlayerGamerTag )
	{
		m_pBtnPlayerGamerTag->SetEnabled( m_pInfo != NULL );
		m_pBtnPlayerGamerTag->SetVisible( m_pInfo != NULL );

		char const *szName = m_pInfo->GetString( "name", "" );
		szName = CUIGameData::Get()->GetPlayerName( xuid, szName );
		m_pBtnPlayerGamerTag->SetText( szName );
	}

	if ( m_pDrpPlayer )
	{
		m_pDrpPlayer->SetEnabled( m_pInfo != NULL );
		m_pDrpPlayer->SetVisible( m_pInfo != NULL );
	}

	if ( m_pLblPlayerVoiceStatus )
	{
		m_pLblPlayerVoiceStatus->SetEnabled( m_pInfo != NULL );
		m_pLblPlayerVoiceStatus->SetText( GetVoiceStatus() );
	}

	if ( m_pLblPartyIcon )
	{
		m_pLblPartyIcon->SetEnabled( m_pInfo != NULL );
	}

	if ( m_pLblEmptySlotAd )
	{
		m_pLblEmptySlotAd->SetEnabled( m_pInfo == NULL );
	}

	//
	// Enable the entire player item
	//

	SetEnabled( m_pInfo != NULL );
#ifdef _X360
	if ( !m_pInfo && HasFocus() )
	{
		// This slot is selected, but has become invalid, so go to the next available slot!
		// Get the generic panel list we are part of, which happens to be the parent of our parent
		Panel *parent = GetParent();
		Assert( parent );
		if ( parent )
		{
			GenericPanelList *parent2 = dynamic_cast<GenericPanelList *>( parent->GetParent() );
			if ( parent2 )
			{
				if ( parent2->GetPanelItem( 0 ) && parent2->GetPanelItem( 0 ) == this )
				{
					// At the top of the list! Go left!
					parent2->NavigateLeft();
				}
				else
				{
					// Go up to the previous item
					parent2->SelectPanelItem( 0, GenericPanelList::SD_UP, true );
				}
			}
		}
	}
#endif
}

void CPlayerItem::OnPlayerActivity( KeyValues *pEvent )
{
	char const *szAct = pEvent->GetString( "act" );

	if ( !Q_stricmp( "voice", szAct ) )
	{
		m_flVoiceTalking = Plat_FloatTime() + 0.4;	// show the speaker for 0.4 sec

		if ( m_pLblPlayerVoiceStatus )
			m_pLblPlayerVoiceStatus->SetText( GetVoiceStatus() );
	}
}

char const * CPlayerItem::GetVoiceStatus() const
{
	KeyValues *pInfo = GetPlayerInfo();

	char const *szVoice = pInfo->GetString( "voice", "" );
	XUID xuid = pInfo->GetUint64( "xuid", 0ull );
	
	if ( !xuid )
		return "#GameUI_Icons_VOICE_OFF";

	if ( g_pMatchFramework->GetMatchSystem()->GetMatchVoice()->IsTalkerMuted( xuid ) )
		return "#GameUI_Icons_VOICE_MUTED";

	if ( ( Plat_FloatTime() < m_flVoiceTalking ) && g_pMatchFramework->GetMatchSystem()->GetMatchVoice()->CanPlaybackTalker( xuid ) )
		return "#GameUI_Icons_VOICE_TALKING";

	if ( !szVoice || !szVoice[0] )
		return "#GameUI_Icons_VOICE_OFF";

	return "#GameUI_Icons_VOICE_IDLE";
}

KeyValues * CPlayerItem::GetPlayerInfo() const
{
	return m_pInfo;
}

void CPlayerItem::ApplySettings( KeyValues *inResourceData )
{
	BaseClass::ApplySettings( inResourceData );

	m_bkFadeColor = inResourceData->GetColor( "BKFadeColor" );
}

void CPlayerItem::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	
	m_OutOfFocusBgColor = pScheme->GetColor( "PlayerListItem.OutOfFocusBgColor", Color( 32, 32, 32, 255 ) );
	m_FocusBgColor = pScheme->GetColor( "PlayerListItem.FocusBgColor", Color( 32, 32, 32, 255 ) );

	SetBgColor( m_OutOfFocusBgColor );

	m_hTextFont = pScheme->GetFont( "DefaultBold", true );
	m_hTextBlurFont = pScheme->GetFont( "DefaultBoldBlur", true );

	//
	// Find all our controls
	//
#define FIND_CONTROL_BASE( fcType, fcName, pCtrl ) \
	fcName = dynamic_cast< fcType * >( pCtrl ); \
	if ( fcName ) \
	{ \
		fcName->SetVisible( false ); \
		fcName->SetEnabled( false ); \
	}
#define FIND_CONTROL( fcType, fcName, szName ) FIND_CONTROL_BASE( fcType, fcName, FindChildByName( szName ) )

	FIND_CONTROL( DropDownMenu, m_pDrpPlayer, "DrpPlayer" );
	FIND_CONTROL( vgui::ImagePanel, m_pImgPlayerPortrait, "ImgPlayerPortrait" );
	FIND_CONTROL( vgui::ImagePanel, m_pImgGamerPic, "PnlGamerPic" );
	FIND_CONTROL( vgui::Label, m_pLblPlayerVoiceStatus, "LblPlayerVoiceStatus" );
	FIND_CONTROL( vgui::Label, m_pLblPartyIcon, "LblPartyIcon" );
	FIND_CONTROL( vgui::Label, m_pLblEmptySlotAd, "LblEmptySlotAd");

	if ( m_pDrpPlayer )
	{
		m_pDrpPlayer->SetSelectedTextEnabled( false );
		FIND_CONTROL_BASE( BaseModHybridButton, m_pBtnPlayerGamerTag, m_pDrpPlayer->FindChildByName( "BtnDropButton" ) );
	}

#undef FIND_CONTROL
#undef FIND_CONTROL_BASE

	// Force all our info to be applied again
	SetPlayerInfo( m_pInfo );
}

void CPlayerItem::PaintBackground()
{
	//
	// Update code
	//

	if ( m_flVoiceTalking && Plat_FloatTime() > m_flVoiceTalking )
	{
		m_flVoiceTalking = 0;

		if ( m_pLblPlayerVoiceStatus )
			m_pLblPlayerVoiceStatus->SetText( GetVoiceStatus() );
	}

	//
	// Painting code
	//

	int panelWide, panelTall;
	GetSize( panelWide, panelTall );

	int c = 200.0f + 55.0f * sin( Plat_FloatTime() * 4.0f );

	if ( m_pImgPlayerPortrait )
	{
		int x, y, tall, wide;
		m_pImgPlayerPortrait->GetBounds( x, y, tall, wide );

		if ( m_pImgPlayerPortrait->IsEnabled() && m_pImgPlayerPortrait->GetImage() )
		{
			surface()->DrawSetColor( Color( 80, 80, 80, 255 ) );
			surface()->DrawFilledRect( x, y, x+wide, y+tall );

			Color col( 200, 200, 200, 255 );
			if ( HasFocus() )
			{
				col.SetColor( c, c, c, 255 );
			}
			surface()->DrawSetColor( col );
			surface()->DrawSetTexture( m_pImgPlayerPortrait->GetImage()->GetID() );
			surface()->DrawTexturedRect( x+2, y+2, x+wide-2, y+tall-2 );
		}
		else
		{
			Color col( 80, 80, 80, 255 );
			surface()->DrawSetColor( col );
			surface()->DrawOutlinedRect( x, y, x+wide, y+tall );
			surface()->DrawOutlinedRect( x+1, y+1, x+wide-1, y+tall-1 );
		}
	}

	if ( m_pImgGamerPic )
	{
		int x, y, tall, wide;
		m_pImgGamerPic->GetBounds( x, y, tall, wide );

		// vertical center
		y = ( panelTall - tall ) / 2;

		if ( m_pImgGamerPic->IsEnabled() )
		{
			surface()->DrawSetColor( Color( 80, 80, 80, 255 ) );
			surface()->DrawFilledRect( x, y, x+wide, y+tall );

			Color col( 200, 200, 200, 255 );
			if ( HasFocus() )
			{
				col.SetColor( c, c, c, 255 );
			}
			surface()->DrawSetColor( col );
			surface()->DrawSetTexture( m_pImgGamerPic->GetImage()->GetID() );
			surface()->DrawTexturedRect( x+2, y+2, x+wide-2, y+tall-2 );
		}
		else
		{
			Color col( 80, 80, 80, 255 );
			surface()->DrawSetColor( col );
			surface()->DrawOutlinedRect( x, y, x+wide, y+tall );
			surface()->DrawOutlinedRect( x+1, y+1, x+wide-1, y+tall-1 );
		}
	}

	if ( m_pLblPlayerVoiceStatus && m_pLblPlayerVoiceStatus->IsEnabled() ) 
	{
		Color col( 100, 100, 100, 255 );
		
		int x, y;
		wchar_t szUnicode[512];
		m_pLblPlayerVoiceStatus->GetText( szUnicode, sizeof( szUnicode ) );
		m_pLblPlayerVoiceStatus->GetPos( x, y );
		int len = V_wcslen( szUnicode );
		int textWide, textTall;
		surface()->GetTextSize( m_pLblPlayerVoiceStatus->GetFont(), szUnicode, textWide, textTall );

		// vertical center
		y = ( panelTall - textTall ) / 2;

		vgui::surface()->DrawSetTextFont( m_pLblPlayerVoiceStatus->GetFont() );
		vgui::surface()->DrawSetTextPos( x, y );
		vgui::surface()->DrawSetTextColor( col );
		vgui::surface()->DrawPrintText( szUnicode, len );
	}

	if ( m_pLblEmptySlotAd && m_pLblEmptySlotAd->IsEnabled() )
	{
		Color col( 100, 100, 100, 255 );

		int x, y;
		wchar_t szUnicode[512];
		m_pLblEmptySlotAd->GetText( szUnicode, sizeof( szUnicode ) );
		m_pLblEmptySlotAd->GetPos( x, y );
		int len = V_wcslen( szUnicode );
		int textWide, textTall;
		surface()->GetTextSize( m_hTextFont, szUnicode, textWide, textTall );

		// vertical center
		y = ( panelTall -  textTall ) / 2;

		vgui::surface()->DrawSetTextFont( m_hTextFont );
		vgui::surface()->DrawSetTextPos( x, y );
		vgui::surface()->DrawSetTextColor( col );
		vgui::surface()->DrawPrintText( szUnicode, len );
	}

	if ( ( !m_pDrpPlayer || m_pDrpPlayer->IsEnabled() ) && // m_pDrpPlayer can actually be NULL on X360
		m_pBtnPlayerGamerTag && m_pBtnPlayerGamerTag->IsEnabled() ) 
	{
		Color col( 100, 100, 100, 255 );

		Panel *pPosPanel = m_pDrpPlayer ? ( Panel * ) m_pDrpPlayer : ( Panel * ) m_pBtnPlayerGamerTag;

		wchar_t szUnicode[512];
		m_pBtnPlayerGamerTag->GetText( szUnicode, sizeof( szUnicode ) );
		int len = V_wcslen( szUnicode );
		int textWide, textTall;
		surface()->GetTextSize( m_hTextFont, szUnicode, textWide, textTall );

		// vertical center
		int x, y;
		pPosPanel->GetPos( x, y );
		y = ( panelTall -  textTall ) / 2;
		pPosPanel->SetPos( x, y );

		bool bRenderText = IsX360();
		pPosPanel->SetVisible( !bRenderText );
		if ( bRenderText )
		{
			if ( HasFocus() )
			{
				col.SetColor( 255, 255, 255, 255 );
			}

			vgui::surface()->DrawSetTextFont( m_hTextFont );
			vgui::surface()->DrawSetTextPos( x, y );
			vgui::surface()->DrawSetTextColor( col );
			vgui::surface()->DrawPrintText( szUnicode, len );

			if ( HasFocus() )
			{
				// draw glow
				int alpha = 60.0f + 30.0f * sin( Plat_FloatTime() * 4.0f );
				vgui::surface()->DrawSetTextColor( Color( 255, 255, 255, alpha ) );
				vgui::surface()->DrawSetTextFont( m_hTextBlurFont );
				vgui::surface()->DrawSetTextPos( x, y );
				vgui::surface()->DrawPrintText( szUnicode, len );
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
//
// Players list implementation
//

class CPlayersList : public GenericPanelList
{
	DECLARE_CLASS_SIMPLE( CPlayersList, GenericPanelList );

public:
	CPlayersList( vgui::Panel *parent, const char *panelName, ITEM_SELECTION_MODE selectionMode ) :
		BaseClass( parent, panelName, selectionMode )
	{
		SetNavigationChangedCallback( FixNavigationLinks );
	}

	static void FixNavigationLinks( GenericPanelList *pList, Panel *pPanel )
	{
		//THIS FUNCTION IS A HACK.
		//Here's the problem. Keyboard navigation is wired into the hybrid button of the dropdown list of the player entry of the panel list.
		//Hybrid buttons understand that they are sometimes children of dropdowns and pass the navigation up in that case.
		//But we have a disparity, the dropdown in every other case should directly navigate and so it doesn't pass it up any more.
		//The simple solution is to make sure keyboard navigation from one of these dropdowns navigates to one the one in another player list entry.
		//BUT THE PANEL LIST CAN CHANGE AT HIGH FREQUENCY AND IT'S TOO LATE TO HOOK INTO Panel::SetNavUp()/Down()/Left()/Right() AND fix THAT DATA DOWN ONE MORE LEVEL BECAUSE VGUI_CONTROLS.LIB CAN'T CHANGE. GAHHHHHH!H!H!HH!H!!!H!

		CPlayerItem *pItem = ( CPlayerItem * ) pPanel;

		pItem->SetNavLeft( pList->GetNavLeft() );
		pItem->SetNavRight( pList->GetNavRight() );

		// Don't allow navigating to items not representing a player
		if ( Panel *pNav = pItem->GetNavDown() )
		{
			bool bNavValid = ( ( CPlayerItem * )( pNav ) )->GetPlayerInfo() != NULL;
			if ( !bNavValid )
				pItem->SetNavDown( pItem );
		}
		if ( Panel *pNav = pItem->GetNavUp() )
		{
			bool bNavValid = ( ( CPlayerItem * )( pNav ) )->GetPlayerInfo() != NULL;
			if ( !bNavValid )
				pItem->SetNavUp( pItem );
		}

		if ( DropDownMenu *pPlayerDropDown = ( DropDownMenu * ) pItem->FindChildByName( "DrpPlayer" ) )
		{
			pPlayerDropDown->SetNavUp( pItem->GetNavUp() );
			pPlayerDropDown->SetNavDown( pItem->GetNavDown() );
			pPlayerDropDown->SetNavLeft( pItem->GetNavLeft() );
			pPlayerDropDown->SetNavRight( pItem->GetNavRight() );
		}
		else if ( BaseModHybridButton *pBtnPlayerGamerTag = ( BaseModHybridButton * ) pItem->FindChildByName( "LblPlayerGamerTag" ) )
		{
			pBtnPlayerGamerTag->SetNavUp( pItem->GetNavUp() );
			pBtnPlayerGamerTag->SetNavDown( pItem->GetNavDown() );
			pBtnPlayerGamerTag->SetNavLeft( pItem->GetNavLeft() );
			pBtnPlayerGamerTag->SetNavRight( pItem->GetNavRight() );
		}

		//
		// Link all parent buttons to first list item
		//
		Panel *pLinkItem = NULL;
		if ( pList->GetPanelItemCount() > 0 )
			pLinkItem = pList->GetPanelItem( 0 );

		if ( Panel *pParent = pList->GetParent() )
		{
			for( int i = 0; i < pParent->GetChildCount(); ++ i )
			{
				Panel *pChild = pParent->GetChild( i );
				if ( !pChild || pChild == pList )
					continue;
				
				pChild->SetNavRight( pLinkItem );
			}
		}
	}
};


//////////////////////////////////////////////////////////////////////////
//
// Game lobby implementation
//

GameLobby::GameLobby(vgui::Panel *parent, const char *panelName) :
	BaseClass( parent, panelName, true, true, false ),
	m_pSettings( NULL ),
#if !defined( _X360 ) && !defined( NO_STEAM )
	m_CallbackPersonaStateChanged( this, &GameLobby::Steam_OnPersonaStateChanged ),
#endif
	m_nMsgBoxId( 0 ),
	m_xuidPlayerFlyout( 0 ),
	m_bNoCommandHandling( false ),
	m_bSubscribedForEvents( false ),
	m_pLobbyDetailsLayout( NULL ),
	m_autodelete_pLobbyDetailsLayout( m_pLobbyDetailsLayout ),
	m_flLastLobbyActivityTime( 0.f )
{
	memset( m_StateText, 0, sizeof( m_StateText ) );
	memset( m_FormattedStateText, 0, sizeof( m_FormattedStateText ) );

	SetDeleteSelfOnClose( true );

	vgui::ivgui()->AddTickSignal( GetVPanel(), 1000 );	// used to track idle state

	SetProportional( true );
	SetPaintBackgroundEnabled( true );

	SetLowerGarnishEnabled( true );

	//
	// Add players list control
	//
	m_pPlayersList = new CPlayersList( this, "GplSurvivors", GenericPanelList::ISM_PERITEM );
	m_pPlayersList->SetScrollArrowsVisible( false );
	m_pPlayersList->SetSchemeBgColorName( "PlayerPanelList.BgColor" );

	//
	// Add chat history on PC, note the chat objects are autonomous
	// and will be deleted when the parent window is deleted.
	//
#ifndef _X360
	new CGameLobbyChatInputLine( this, "ChatInputLine" );
	new CGameLobbyChatHistory( this, "LobbyChatHistory" );
#endif
}

GameLobby::~GameLobby()
{
	if ( m_bSubscribedForEvents )
	{
		g_pMatchFramework->GetEventsSubscription()->Unsubscribe( this );
		m_bSubscribedForEvents = false;
	}

	delete m_pPlayersList;
}

void GameLobby::SetDataSettings( KeyValues *pSettings )
{
	// Maintain a reference to session settings
	m_pSettings = pSettings;
}

void GameLobby::OnCommand(const  char *command )
{
	NotifyLobbyNotIdleActivity();

	if ( m_bNoCommandHandling )
		return;

	if ( !Q_stricmp( "StartGame", command ) )
	{
		IMatchSession *pMatchSession = g_pMatchFramework->GetMatchSession();
		if ( pMatchSession )
		{
			char const *szGameMode = m_pSettings->GetString( "game/mode", "" );
			char const *szCommand = StringHasPrefix( szGameMode, "team" ) ? "Match" : "Start";
			pMatchSession->Command( KeyValues::AutoDeleteInline( new KeyValues( szCommand ) ) );
		}
	}
	else if ( !Q_stricmp( "CancelDedicatedSearch", command ) )
	{
		IMatchSession *pMatchSession = g_pMatchFramework->GetMatchSession();
		if ( pMatchSession )
		{
			pMatchSession->Command( KeyValues::AutoDeleteInline( new KeyValues( "Cancel" ) ) );
		}
	}
	else if ( ! Q_strcmp( command, "ChangeGameSettings" ) )
	{
		CBaseModPanel::GetSingleton().OpenWindow( WT_GAMESETTINGS, this, true );
	}
	else if ( ! Q_strcmp( command, "LeaveLobby" ) )
	{
		LeaveLobby();
	}
	else if( !Q_strcmp( command, "ToggleVoice" ) )
	{
		bool bRecording = g_pMatchFramework->GetMatchSystem()->GetMatchVoice()->IsVoiceRecording();

		g_pMatchFramework->GetMatchSystem()->GetMatchVoice()->SetVoiceRecording( !bRecording );
		
		if ( !bRecording )
			bRecording = g_pMatchFramework->GetMatchSystem()->GetMatchVoice()->IsVoiceRecording(); // check again if the mode was enabled
		else
			bRecording = false;

		if ( BaseModHybridButton *voiceButton = dynamic_cast< BaseModHybridButton * >( FindChildByName( "BtnVoiceButton" ) ) )
		{
			char const * arrAction[2] = { "Stop", "Start" };
			voiceButton->SetText( CFmtStr( "#L4D360UI_Lobby_%sVoiceChat", arrAction[!bRecording] ) );
			voiceButton->SetHelpText( CFmtStr( "#L4D360UI_Lobby_%sVoiceChat_Tip", arrAction[!bRecording] ) );
		}
	}
	else if ( char const *szNewAccess = StringAfterPrefix( command, "GameAccess_" ) )
	{
		KeyValues *pSettings = new KeyValues( "update" );
		KeyValues::AutoDelete autodelete( pSettings );

		pSettings->SetString( "update/system/access", szNewAccess );

		g_pMatchFramework->GetMatchSession()->UpdateSessionSettings( pSettings );
	}
	else if ( char const *pszAvatar = StringAfterPrefix( command, "character_" ) )
	{
		KeyValues *pRequest = new KeyValues( "Game::Avatar" );
		KeyValues::AutoDelete autodelete( pRequest );

		int iController = 0;
#ifdef _X360
		int iSlot = CBaseModPanel::GetSingleton().GetLastActiveUserId();
		iController = XBX_GetUserId( iSlot );
#endif
		XUID xuid = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()
			->GetLocalPlayer( iController )->GetXUID();

		pRequest->SetString( "run", "host" );
		pRequest->SetUint64( "xuid", xuid );
		pRequest->SetString( "avatar", pszAvatar );

		g_pMatchFramework->GetMatchSession()->Command( pRequest );
	}
	else if ( char const *szInviteType = StringAfterPrefix( command, "InviteUI_" ) )
	{
		FlyoutMenu::CloseActiveMenu();
		if ( IsX360() )
		{
			CUIGameData::Get()->OpenInviteUI( szInviteType );
		}
		else
		{
			CUIGameData::Get()->ExecuteOverlayCommand( "LobbyInvite" );
		}
	}
	else
	{
		BaseClass::OnCommand( command );
	}
}

void GameLobby::NotifyLobbyNotIdleActivity()
{
	m_flLastLobbyActivityTime = 0.f;	// nudge lobby activity timer
}

void GameLobby::OnTick()
{
	vgui::Panel * btnStartGame = FindChildByName( "BtnStartGame" );
	if ( btnStartGame && btnStartGame->IsEnabled() &&
		!Q_stricmp( "LIVE", m_pSettings->GetString( "system/network" ) ) &&
		!Q_stricmp( "public", m_pSettings->GetString( "system/access" ) ) &&
		!*m_pSettings->GetString( "system/lock" ) )
	{
		// This is a public host lobby which is subject to access change if idle too long
		float flCurrentTime = Plat_FloatTime();
		if ( m_flLastLobbyActivityTime <= 0.f )
		{
			m_flLastLobbyActivityTime = flCurrentTime;	// Initiate the countdown
		}
		else if ( m_flLastLobbyActivityTime + ui_lobby_idle_time.GetFloat() < flCurrentTime )
		{
			// Lobby has been idle for too long
			m_flLastLobbyActivityTime = 0.f;
			OnCommand( "GameAccess_friends" );	// Change permissions to friends-only
		}
	}

	BaseClass::OnTick();
}

void GameLobby::OnNavigateTo( const char *panelName )
{
	NotifyLobbyNotIdleActivity();
	BaseClass::OnNavigateTo( panelName );
}

void GameLobby::OnThink()
{
	ClockSpinner();
	UpdateFooterButtons();
	UpdateLiveWarning();

	BaseClass::OnThink();
}

void GameLobby::NavigateToChild( Panel *pNavigateTo )
{
	for( int i = 0; i != GetChildCount(); ++i )
	{
		vgui::Panel *pChild = GetChild(i);
		if( pChild )
			pChild->NavigateFrom();
	}

	// TODO: NavigateTo_ChatHandler( pNavigateTo );
}

void GameLobby::OnKeyCodePressed( vgui::KeyCode code )
{
	NotifyLobbyNotIdleActivity();

	int userID = GetJoystickForCode( code );
	CBaseModPanel::GetSingleton().SetLastActiveUserId( userID );

	switch( GetBaseButtonCode( code ) )
	{
	case KEY_XBUTTON_B: // the player is leaving the lobby, so cleanup
		LeaveLobby();
		return;

	case KEY_XBUTTON_START:
		{
			vgui::Panel * btnStartGame = FindChildByName( "BtnStartGame" );
			if( btnStartGame && btnStartGame->IsEnabled() )
			{
				OnCommand( "StartGame" );
			}
		}
		return;

	default:
		BaseClass::OnKeyCodePressed( code );
		break;
	}
}

void GameLobby::OpenPlayerFlyout( CPlayerItem *pPlayerItem )
{
	if ( !pPlayerItem )
		return;

	//
	// Determine the anchor point
	//
	Panel *pDrpPlayer = pPlayerItem->FindChildByName( "DrpPlayer" );
	Panel *pAnchor = pDrpPlayer ? pDrpPlayer->FindChildByName( "BtnDropButton" ) : NULL;
	if ( !pAnchor )
		return;
	int x, y;
	pAnchor->GetPos( x, y );
	int tall = pAnchor->GetTall();
	pAnchor->LocalToScreen( x, y );
	y = y + tall + 1;

	//
	// Determine which flyout will be used
	//
	bool bHost = !Q_stricmp( "host", g_pMatchFramework->GetMatchSession()
		->GetSessionSystemData()->GetString( "type", "host" ) );
	XUID xuidHost = g_pMatchFramework->GetMatchSession()
		->GetSessionSystemData()->GetUint64( "xuidHost", 0ull );

	KeyValues *pPlayer = pPlayerItem->GetPlayerInfo();
	XUID xuidPlayer = pPlayer->GetUint64( "xuid", 0ull );
	XUID xuidSelf = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( XBX_GetPrimaryUserId() )->GetXUID();

	FlyoutMenu *flyoutLeader = dynamic_cast< FlyoutMenu * >( FindChildByName( "FlmPlayerFlyoutLeader" ) );
	FlyoutMenu *flyout = dynamic_cast< FlyoutMenu * >( FindChildByName( "FlmPlayerFlyout" ) );

	FlyoutMenu *playerFlyout = NULL;

	if( bHost && xuidHost && xuidHost != xuidPlayer )
	{
		playerFlyout = flyoutLeader;
	}
	else
	{
		playerFlyout = flyout;
	}
	if ( !playerFlyout )
		return;

	int wndX, wndY;
	GetPos( wndX, wndY );

	m_xuidPlayerFlyout = xuidPlayer;
	playerFlyout->OpenMenu( pAnchor );
	playerFlyout->SetPos( x, y - wndY );
	playerFlyout->SetOriginalTall( 0 );

	// Update mute button
	// Disable if leader
	BaseModHybridButton *muteButton = dynamic_cast< BaseModHybridButton * >( playerFlyout->FindChildByName( "BtnMute" ) );
	if ( muteButton )
	{
		muteButton->SetEnabled( xuidPlayer != xuidSelf );
		if ( g_pMatchFramework->GetMatchSystem()->GetMatchVoice()->IsTalkerMuted( xuidPlayer ) )
		{
			muteButton->SetText( "#L4D360UI_UnMutePlayer" );
		}
		else
		{
			muteButton->SetText( "#L4D360UI_MutePlayer" );
		}
	}

	// Disable Send Message if self
	BaseModHybridButton *sendMessasge = dynamic_cast< BaseModHybridButton * >( playerFlyout->FindChildByName( "BtnSendMessage" ) );
	if ( sendMessasge )
	{
		sendMessasge->SetEnabled( xuidPlayer != xuidSelf );
	}
}

void GameLobby::OnOpen()
{
	SetVisible( true );
	BaseClass::OnOpen();
}

void GameLobby::OnClose()
{
	if ( m_nMsgBoxId )
	{
		GenericConfirmation* confirmation = 
			static_cast< GenericConfirmation* >( CBaseModPanel::GetSingleton().GetWindow( WT_GENERICCONFIRMATION ) );
		if ( confirmation != NULL && confirmation->GetUsageId() == m_nMsgBoxId )
		{
			confirmation->Close();
		}
	}

	// Disable voice recording if it was enabled
	g_pMatchFramework->GetMatchSystem()->GetMatchVoice()->SetVoiceRecording( false );

	BaseClass::OnClose();
}

void GameLobby::MsgNoValidMissionChapter( KeyValues *pSettings )
{
	LeaveLobbyImpl();	// Lobby is no valid at this point

	if ( IsX360() )
	{
		// The required DLC is missing on the client, fire a notification that would
		// allow us to download the DLC from the marketplace
		uint64 uiDlcMask = pSettings->GetUint64( "game/dlcrequired" );
		uint64 uiDlcInstalled = GetDlcInstalledMask();

		uint64 uiDlcNotify = ( uiDlcMask &~uiDlcInstalled );

		KeyValues *kvDlcNotify = new KeyValues( "OnMatchSessionUpdate" );
		KeyValues::AutoDelete autodelete_kvDlcNotify( kvDlcNotify );

		kvDlcNotify->SetString( "state", "error" );
		kvDlcNotify->SetString( "error", "dlcrequired" );
		kvDlcNotify->SetUint64( "dlcrequired", uiDlcNotify );
		kvDlcNotify->SetUint64( "dlcmask", uiDlcMask );
		kvDlcNotify->SetString( "action", "kicked" );

		CUIGameData::Get()->OnEvent( kvDlcNotify );
		return;
	}

	const char *szCampaignWebsite = pSettings->GetString( "game/missioninfo/website", NULL );

	if ( szCampaignWebsite && *szCampaignWebsite )
	{
		if ( pSettings )
			pSettings->SetString( "game/missioninfo/from", "Lobby" );

		CBaseModPanel::GetSingleton().OpenWindow( WT_DOWNLOADCAMPAIGN,
			CBaseModPanel::GetSingleton().GetWindow( CBaseModPanel::GetSingleton().GetActiveWindowType() ),
			true, pSettings );
	}
	else
	{
		GenericConfirmation::Data_t data;

		data.pWindowTitle = "#L4D360UI_Lobby_MissingContent";
		data.pMessageText = "#L4D360UI_Lobby_MissingContent_Message";
		data.bOkButtonEnabled = true;

		GenericConfirmation* confirmation = 
			static_cast< GenericConfirmation* >( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, NULL, true ) );

		confirmation->SetUsageData(data);
	}
}

void GameLobby::MsgChangeGameSettings()
{
	if ( CBaseModFrame *pSpinner = CBaseModPanel::GetSingleton().GetWindow( WT_GENERICWAITSCREEN ) )
		pSpinner->Close();
	else
		return;

	CBaseModPanel::GetSingleton().PlayUISound( UISOUND_ACCEPT );
	OnCommand( "ChangeGameSettings" );
}

void GameLobby::PaintBackground()
{
	if ( !m_pSettings )
		return;

	char const *szGameMode = m_pSettings->GetString( "game/mode", "campaign" );

	CFmtStr szDlgCaption( "#L4D360UI_Lobby_Title_%s", szGameMode );

	const char *pMinor = NULL;
	const wchar_t *pFormattedMinor = NULL;
	
	CUtlVectorFixedGrowable< wchar_t const *, 10 > arrActiveContent;
	wchar_t wchScratch[MAX_PATH] = {0};

	if ( m_StateText[0] )
	{
		pMinor = m_StateText;
	}
	else if ( m_FormattedStateText[0] )
	{
		pFormattedMinor = m_FormattedStateText;
	}
	else
	{
		IDatacenter *pDatacenter = g_pMatchFramework->GetMatchSystem()->GetDatacenter();
		KeyValues *pStats = pDatacenter ? pDatacenter->GetStats() : NULL;

		// There's no minor status yet, show some active content there
		if ( StringHasPrefix( szGameMode, "team" ) )
		{
			int nSearchTime = pStats->GetInt( "search_team_time", 0 );
			if ( nSearchTime > 0 && nSearchTime < ui_lobby_stat_team_search_max.GetInt() )
			{
				const int numBufferBytes = 2 * MAX_PATH;
				wchar_t *pchBuffer = ( wchar_t * ) stackalloc( numBufferBytes );
				char const *pchFmt = "";
				if ( nSearchTime <= 60 )
				{
					pchFmt = "#L4D360UI_Stat_TeamSearch_Time_Nsec";
					V_snwprintf( wchScratch, ARRAYSIZE( wchScratch ), L"%d", nSearchTime );
				}
				else
				{
					pchFmt = "#L4D360UI_Stat_TeamSearch_Time_Nmin";
					V_snwprintf( wchScratch, ARRAYSIZE( wchScratch ), L"%d", 1 + ( nSearchTime / 60 ) );
				}
				if ( wchar_t *fmtStat = g_pVGuiLocalize->Find( pchFmt ) )
				{
					g_pVGuiLocalize->ConstructString( pchBuffer, numBufferBytes, fmtStat, 1, wchScratch );
					arrActiveContent.AddToTail( pchBuffer );
				}
			}
		}

		// Now if we have active content, then scroll it as formatted minor
		if ( arrActiveContent.Count() )
		{
			int nTime = Plat_FloatTime();
			nTime /= ui_lobby_stat_switch_time.GetInt();
			nTime %= arrActiveContent.Count();
			pFormattedMinor = arrActiveContent[nTime];
		}
	}

	if ( !pMinor && !pFormattedMinor )
	{
		// prevent dialog caption from shrinking when we don't have a minor title
		pMinor = " ";
	}

	BaseClass::DrawDialogBackground( szDlgCaption, NULL, pMinor, pFormattedMinor, NULL, false, m_iTitleXOffset );
}

void GameLobby::ApplySchemeSettings( vgui::IScheme* pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	// Load the proper control settings file
	char const *szSettingsFile = "GameLobby";
	char const *szGameMode = m_pSettings->GetString( "game/mode", "" );
	if ( StringHasPrefix( szGameMode, "team" ) )
		szSettingsFile = "TeamLobby";

	char szPath[MAX_PATH];
	V_snprintf( szPath, sizeof( szPath ), "Resource/UI/BaseModUI/%s.res", szSettingsFile );

	LoadControlSettings( szPath );

	// required for new style
	SetPaintBackgroundEnabled( true );
	SetupAsDialogStyle();

	// Subscribe to the matchmaking events
	if ( !m_bSubscribedForEvents )
	{
		g_pMatchFramework->GetEventsSubscription()->Subscribe( this );
		m_bSubscribedForEvents = true;
	}

	InitializePlayersList();
	InitializeFromSettings();
}

void GameLobby::SetStateText( const char *pText, const wchar_t *pFormattedText )
{
	if ( pText )
	{
		V_strncpy( m_StateText, pText, sizeof( m_StateText ) );
	}
	else if ( pFormattedText )
	{
		m_StateText[0] = 0;
		V_wcsncpy( m_FormattedStateText, pFormattedText, sizeof( m_FormattedStateText ) );
	}
	else
	{
		m_StateText[0] = 0;
		m_FormattedStateText[0] = 0;
	}
}

void GameLobby::ClockSpinner()
{
	vgui::ImagePanel *pWorkingAnim = dynamic_cast< vgui::ImagePanel * >( FindChildByName( "WorkingAnim" ) );
	if ( !pWorkingAnim )
		return;

	int nAnimFrame = ( uint64 )( Plat_FloatTime() * 10 );
	pWorkingAnim->SetFrame( nAnimFrame );
}

void GameLobby::UpdateFooterButtons()
{
	CBaseModFooterPanel *footer = BaseModUI::CBaseModPanel::GetSingleton().GetFooterPanel();
	if ( !footer )
		return;

	IMatchSession *pMatchSession = g_pMatchFramework->GetMatchSession();
	if ( !pMatchSession )
		return;

	bool bHost = !Q_stricmp( "host", pMatchSession->GetSessionSystemData()->GetString( "type", "host" ) );
	char const *szNetwork = m_pSettings->GetString( "system/network", "LIVE" );

	bool bIsLiveGame = !Q_stricmp( szNetwork, "LIVE" );
	bool bCanKick = bHost && Q_stricmp( szNetwork, "offline" );

	bool bShouldShowGamerCard = false;

	for( int slotCount = 0; slotCount < m_pPlayersList->GetPanelItemCount(); ++slotCount )
	{
		CPlayerItem * listItem = static_cast< CPlayerItem * >( m_pPlayersList->GetPanelItem( slotCount ) );
		if( listItem->HasFocus() )
		{			
			bShouldShowGamerCard = true;
			break;
		}
	}

	// We don't show the A button if we're not in a live game and hovering over a player
	Button_t iAButton = ( ( bShouldShowGamerCard && !bIsLiveGame ) ? ( FB_NONE ) : ( FB_ABUTTON ) );

	if ( !bCanKick )
	{
		// They don't own the lobby or are in an offline game
		footer->SetButtons( iAButton | FB_BBUTTON | ( !bIsLiveGame ? FB_NONE : FB_YBUTTON ), FF_A_GAMERCARD_BY, false );
		footer->SetButtonText( FB_YBUTTON, "#L4D360UI_ReviewPlayer" );
	}
	else
	{
		// They own the lobby
		if ( !bShouldShowGamerCard || !bIsLiveGame )
		{
			// We don't show the review button while they're not hovering over a player or they're not in a live game
			footer->SetButtons( iAButton | FB_BBUTTON | FB_XBUTTON, FF_A_GAMERCARD_BX, false );
		}
		else
		{
			// Need to show kick and review
			footer->SetButtons( iAButton | FB_BBUTTON | FB_XBUTTON | FB_YBUTTON, FF_A_GAMERCARD_BY__X_HIGH, false );
			footer->SetButtonText( FB_YBUTTON, "#L4D360UI_ReviewPlayer" );
		}

		footer->SetButtonText( FB_XBUTTON, "#L4D360UI_Kick" );
	}

	if ( bShouldShowGamerCard )
	{
		footer->SetButtonText( FB_ABUTTON, "#L4D360UI_ViewGamerCard" );
	}
	else
	{
		footer->SetButtonText( FB_ABUTTON, "#L4D360UI_Select" );
	}

	footer->SetButtonText( FB_BBUTTON, "#L4D360UI_Back" );
}

void GameLobby::UpdateLiveWarning()
{
#ifdef _X360
	bool bShowWarning = false;
	if ( !Q_stricmp( "survival", m_pSettings->GetString( "game/mode" ) ) )
	for ( int k = 0; k < ( int ) XBX_GetNumGameUsers(); ++ k )
	{
		int iCtrlr = XBX_GetUserId( k );
		IPlayerLocal *pLocal = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( iCtrlr );
		if ( pLocal && pLocal->GetOnlineState() == IPlayer::STATE_OFFLINE )
			bShowWarning = true;
	}
	SetControlVisible( "LblNoLiveWarning", bShowWarning );
#endif
}

void GameLobby::LeaveLobby()
{
	CBaseModPanel::GetSingleton().PlayUISound( UISOUND_ACCEPT );

	char const *szNetwork = m_pSettings->GetString( "system/network", "LIVE" );

	// Local splitscreen lobby has no confirmation
	if ( !Q_stricmp( szNetwork, "offline" ) )
	{
		LeaveLobbyImpl();
		return;
	}

	GenericConfirmation::Data_t data;
	char szTxt[256] = {0};

	if ( IsX360() )
	{
		data.pWindowTitle = "#L4D360UI_LeaveLobbyConf";

		char const *szSessionType = g_pMatchFramework->GetMatchSession()->GetSessionSystemData()->GetString( "type", "host" );
		char const *szTxtHosting = Q_stricmp( szSessionType, "host" ) ? "" : "Hosting";
		char const *szTxtNetwork = Q_stricmp( szNetwork, "LIVE" ) ? "SysLink" : "Live";

		Q_snprintf( szTxt, ARRAYSIZE( szTxt ), "#L4D360UI_LeaveLobbyConf%s%s", szTxtHosting, szTxtNetwork );
		data.pMessageText = szTxt;
	}
	else
	{
		data.pWindowTitle = "#L4D360UI_Lobby_LeaveLobby";
		data.pMessageText = "#L4D360UI_LeaveLobbyConf";
	}

	data.bOkButtonEnabled = true;
	data.bCancelButtonEnabled = true;
	data.pfnOkCallback = LeaveLobbyImpl;

	GenericConfirmation* confirmation = 
		static_cast< GenericConfirmation* >( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, this, false ) );

	if ( confirmation )
	{
		m_nMsgBoxId = confirmation->SetUsageData(data);
	}
}

void GameLobby::InitializePlayersList()
{
	KeyValues *pMembers = m_pSettings->FindKey( "members" );

	int numSlots = pMembers->GetInt( "numSlots"	);
	DevMsg( "Lobby: Session slots: %d\n", numSlots );
	m_pPlayersList->RemoveAllPanelItems();
	for ( int k = 0; k < numSlots; ++ k )
	{
		m_pPlayersList->AddPanelItem< CPlayerItem >( "SurvivorListItem" );
	}

	int numMachines = pMembers->GetInt( "numMachines" );
	for ( int k = 0; k < numMachines; ++ k )
	{
		KeyValues *pMachine = pMembers->FindKey( CFmtStr( "machine%d", k ) );
		int numPlayers = pMachine->GetInt( "numPlayers" );
		for ( int j = 0; j < numPlayers; ++ j )
		{
			KeyValues *pPlayer = pMachine->FindKey( CFmtStr( "player%d", j ) );
			Player_AddOrUpdate( pPlayer );
		}
	}
}

void GameLobby::InitializeFromSettings()
{
	CAutoPushPop< bool > autoPreventCommandHandling( m_bNoCommandHandling, true );

	//
	// Clear out all the summary
	//
	for ( int k = 0; k < 10; ++ k )
	{
		if ( vgui::Label *pLbl = dynamic_cast< vgui::Label * > ( FindChildByName( CFmtStr( "LblSummaryLine%d", k ) ) ) )
			pLbl->SetText( "" );
	}

	// Here we have to traverse the session settings and initialize all the
	// slots & settings & players

	char const *szGameMode = m_pSettings->GetString( "game/mode", "" );

	if ( vgui::ImagePanel *pWorkingAnim = dynamic_cast< vgui::ImagePanel * >( FindChildByName( "WorkingAnim" ) ) )
		pWorkingAnim->SetVisible( false );

	// Force all settings to be applied
	ApplyUpdatedSettings( m_pSettings );

	// Game access
	if ( DropDownMenu *drpGameAccess = dynamic_cast< DropDownMenu* >( FindChildByName( "DrpGameAccess" ) ) )
	{
		drpGameAccess->SetSelectedTextEnabled( false );
		drpGameAccess->SetCurrentSelection( CFmtStr( "GameAccess_%s", m_pSettings->GetString( "system/access", "public" ) ) );
		drpGameAccess->SetOpenCallback( &OnDropDown_Access );
	}

	// Invitations
	if ( DropDownMenu *drpInvitations = dynamic_cast< DropDownMenu* >( FindChildByName( "DrpInviteFriends" ) ) )
	{
		drpInvitations->SetSelectedTextEnabled( false );
	}

	// Character selector
	char const *szCharacterSelectorNames[] = { "DrpCharacter", "DrpVersusCharacter", "DrpCharacterUnavailable" };
	int iActiveCharacterSelector = 0;
	if ( !Q_stricmp( "versus", szGameMode ) || !Q_stricmp( "scavenge", szGameMode ) )
	{
		iActiveCharacterSelector = 1;
	}
	for ( int k = 0; k < ARRAYSIZE( szCharacterSelectorNames ); ++ k )
	{
		if ( vgui::Panel *btnCharacter = FindChildByName( szCharacterSelectorNames[k] ) )
		{
			btnCharacter->SetEnabled( false );
			btnCharacter->SetVisible( false );
		}
	}
	
	if ( Panel *pStart = FindChildByName( "BtnStartGame" ) )
	{
		// Set our initial focus
		if ( !m_ActiveControl && pStart->IsEnabled() )
		{
			pStart->NavigateTo();
			m_ActiveControl = pStart;
		}
	}

	if ( DropDownMenu *btnChangeCharacter = dynamic_cast< DropDownMenu* >(
		FindChildByName( szCharacterSelectorNames[ iActiveCharacterSelector ] ) ) )
	{
		// make our actual button visible
		btnChangeCharacter->SetSelectedTextEnabled( false );
		btnChangeCharacter->SetVisible( true );
		btnChangeCharacter->SetEnabled( true );
		btnChangeCharacter->SetOpenCallback( &OnDropDown_Character );

		// Set our initial focus
		if ( !m_ActiveControl )
			btnChangeCharacter->NavigateTo();
	}

	UpdatePlayersChanged();
}

void GameLobby::OnNotifyChildFocus( vgui::Panel* child )
{
	NotifyLobbyNotIdleActivity();

	vgui::ImagePanel *pPanel = dynamic_cast< vgui::ImagePanel* >( child->FindSiblingByName( "HeroPortrait" ) );
	if ( !pPanel )
		return;

	pPanel->SetVisible( true );


	BaseModHybridButton *pButton = dynamic_cast< BaseModHybridButton * >( child );
	if ( !pButton )
		return;

	KeyValues *pCommand = pButton->GetCommand();
	if ( pCommand )
	{
		const char *pszCommand = pCommand->GetString( "command", NULL );

		if ( char const *pszCharacter = StringAfterPrefix( pszCommand, "character_" ) )
		{
			const char *pszPortrait = s_characterPortraits->GetTokenI( pszCharacter );
			if ( pszPortrait )
			{
				pPanel->SetImage( pszPortrait );
			}
		}
	}
}

void GameLobby::UpdatePlayersChanged()
{
	UpdateAvatars();
	UpdateStartButton();
}

// Store the state of selected avatars and enable/disable the avatar selection options inside the dropdown
void GameLobby::UpdateAvatars( void )
{
	KeyValues *pMembers = m_pSettings->FindKey( "members" );

	if ( !pMembers )
		return;

	char const *szGameMode = m_pSettings->GetString( "game/mode", "campaign" );
	char const *szCharacterSelectorNames[] = { "DrpCharacter", "DrpVersusCharacter" };
	int iActiveCharacterSelector = 0;
	if ( !Q_stricmp( "versus", szGameMode ) || !Q_stricmp( "scavenge", szGameMode ) )
	{
		iActiveCharacterSelector = 1;
	}

	DropDownMenu *pDropDown = dynamic_cast< DropDownMenu* >( FindChildByName( szCharacterSelectorNames[ iActiveCharacterSelector ] ) );
	if ( !pDropDown )
	{
		return;
	}

	FlyoutMenu *pFlyout = pDropDown->GetCurrentFlyout();
	if ( !pFlyout )
	{
		return;
	}

	KeyValues *pSelectedAvatars = new KeyValues( "SelectedAvatars" );

	int numMachines = pMembers->GetInt( "numMachines" );
	for ( int k = 0; k < numMachines; ++ k )
	{
		KeyValues *pMachine = pMembers->FindKey( CFmtStr( "machine%d", k ) );
		int numPlayers = pMachine->GetInt( "numPlayers" );
		for ( int j = 0; j < numPlayers; ++ j )
		{
			KeyValues *pPlayer = pMachine->FindKey( CFmtStr( "player%d", j ) );
			const char *pszAvatar = pPlayer->GetString( "game/Avatar", "" );

			if ( *pszAvatar )
			{
				KeyValues *pAvatar = pSelectedAvatars->FindKey( VarArgs( "Btn%s", pszAvatar ), true );	// create it if not found
				int iCount = pAvatar->GetInt( "count", 0 );
				iCount++;
				pAvatar->SetInt( "count", iCount );
			}
		}
	}

	// For each button child of this flyout, see if it has a key in our pSelectedAvatars and set the enabled state
	for( int i = 0; i < pFlyout->GetChildCount(); ++i )
	{
		if ( BaseModHybridButton *pButton = dynamic_cast< BaseModHybridButton* >( pFlyout->GetChild(i) ) )
		{
			const char *pszName = pButton->GetName();

			int iFullCount = ( !Q_strcmp( pszName, "BtnInfected" ) ) ? 4 : 1;

			bool bEnabled = true;

			if ( KeyValues *pKey = pSelectedAvatars->FindKey( pszName ) )
			{
				if ( pKey->GetInt( "count", 1 ) >= iFullCount )
				{
					bEnabled = false;
				}
			}

			pButton->SetEnabled( bEnabled );
		}
	}	

	pSelectedAvatars->deleteThis();
}

void GameLobby::UpdateStartButton()
{
	Panel *btn = FindChildByName( "BtnStartGame" );
	if ( !btn )
		return;

	IMatchSession *pMatchSession = g_pMatchFramework->GetMatchSession();
	if ( !pMatchSession )
		return;

	// Current game mode
	char const *szGameMode = m_pSettings->GetString( "game/mode", "" );
	
	// Check that the current lobby is host and not in locked mode, then it is startable
	bool bHost = !Q_stricmp( "host", pMatchSession->GetSessionSystemData()->GetString( "type", "host" ) );
	char const *szLock = m_pSettings->GetString( "system/lock", "" );
	bool bStartable = bHost && !*szLock;

	// Check if the lobby start button is in override mode
	if ( bStartable && !ui_lobby_start_enabled.GetBool() )
	{
		if ( StringHasPrefix( szGameMode, "team" ) )
		{
			int numPlayers = m_pSettings->GetInt( "members/numPlayers", 0 );
			int numSlots = m_pSettings->GetInt( "members/numSlots", 0 );

			bStartable = ( numPlayers == numSlots );
		}
		else if ( !Q_stricmp( "versus", szGameMode ) || !Q_stricmp( "scavenge", szGameMode ) )
		{
			// Need at least one person on each team before the game can start
			int numRandom = 0, numInfected = 0, numSurvivors = 0;
			int numMachines = m_pSettings->GetInt( "members/numMachines", 0 );
			for ( int k = 0; k < numMachines; ++ k )
			{
				KeyValues *pMachine = m_pSettings->FindKey( CFmtStr( "members/machine%d", k ) );
				int numPlayers = pMachine->GetInt( "numPlayers" );
				for ( int j = 0; j < numPlayers; ++ j )
				{
					KeyValues *pPlayer = pMachine->FindKey( CFmtStr( "player%d", j ) );
					const char *pszAvatar = pPlayer->GetString( "game/Avatar", "" );

					if ( !*pszAvatar )
						++ numRandom;
					else if ( !Q_stricmp( "infected", pszAvatar ) )
						++ numInfected;
					else
						++ numSurvivors;
				}
			}
			
			bStartable =
				( numRandom + numInfected + numSurvivors >= 2 ) &&	// at least 2 players
				( numRandom || ( numInfected && numSurvivors ) );	// at least one random or at least one on each team
		}
	}

	// Set the START button
	btn->SetEnabled( bStartable );
}

void GameLobby::OnEvent( KeyValues *pEvent )
{
	char const *szEvent = pEvent->GetName();

	if ( !Q_stricmp( "OnPlayerRemoved", szEvent ) )
	{
		XUID xuidPlayer = pEvent->GetUint64( "xuid" );
		Player_Remove( xuidPlayer );

		UpdatePlayersChanged();
	}
	else if ( !Q_stricmp( "OnPlayerUpdated", szEvent ) )
	{
		XUID xuidPlayer = pEvent->GetUint64( "xuid" );
		
		KeyValues *pPlayer = SessionMembersFindPlayer( m_pSettings, xuidPlayer );
		Player_AddOrUpdate( pPlayer );

		UpdatePlayersChanged();
	}
	else if ( !Q_stricmp( "OnPlayerLeaderChanged", szEvent ) )
	{
		if ( !Q_stricmp( "host", pEvent->GetString( "state" ) ) )
			ApplyUpdatedSettings( m_pSettings );
		else
			SetLobbyLeaderText();

		if ( !Q_stricmp( "started", pEvent->GetString( "migrate" ) ) )
		{
			CUIGameData::Get()->OpenWaitScreen( "#L4D360UI_MsgBx_DisconnectedLeaderLeft" );
			CUIGameData::Get()->CloseWaitScreen( NULL, NULL );
		}
	}
	else if ( !Q_stricmp( "OnPlayerActivity", szEvent ) )
	{
		XUID xuidPlayer = pEvent->GetUint64( "xuid" );

		if ( CPlayerItem *pPlayerItem = Player_Find( xuidPlayer ) )
		{
			pPlayerItem->OnPlayerActivity( pEvent );
		}
	}
	else if ( !Q_stricmp( "OnSysMuteListChanged", szEvent ) )
	{
		for ( int k = 0; k < m_pPlayersList->GetPanelItemCount(); ++ k )
		{
			CPlayerItem *pItem = dynamic_cast< CPlayerItem * >( m_pPlayersList->GetPanelItem( k ) );
			if ( !pItem )
				continue;

			if ( KeyValues *pInfo = pItem->GetPlayerInfo() )
				pItem->SetPlayerInfo( pInfo );	// Update all items
		}
	}
	else if ( !Q_stricmp( "OnMatchSessionUpdate", szEvent ) )
	{
		char const *szState = pEvent->GetString( "state" );
		if ( !Q_stricmp( szState, "updated" ) )
		{
			// Some properties of the session got updated
			// We care only of the properties that got updated
			if ( KeyValues *kvUpdate = pEvent->FindKey( "update" ) )
			{
				ApplyUpdatedSettings( kvUpdate );
			}
		}
		else if ( !Q_stricmp( szState, "closed" ) )
		{
			m_pSettings = NULL; // Session has closed, stop using the destroyed pointer
		}
	}
}

void GameLobby::ApplyUpdatedSettings( KeyValues *kvUpdate )
{
	NotifyLobbyNotIdleActivity();

	if ( kvUpdate != m_pSettings && kvUpdate->GetString( "game/mode", NULL ) )
	{
		// When game mode changes we need a complete re-initialization
		InitializeFromSettings();
		return;
	}

	CAutoPushPop< bool > autoPreventCommandHandling( m_bNoCommandHandling, true );

	// Always apply the lobby leader update
	bool bHost = !Q_stricmp( "host", g_pMatchFramework->GetMatchSession()
		->GetSessionSystemData()->GetString( "type", "host" ) );
	SetLobbyLeaderText();
	SetControlsState( bHost );


	if ( char const *szAccess = kvUpdate->GetString( "system/access", NULL ) )
	{
		if ( vgui::Label * pAccessLabel = GetSettingsSummaryLabel( "access" ) )
		{
			char const *szDisplay = szAccess;
			char const *szNetwork = m_pSettings->GetString( "system/network" );
			if ( !Q_stricmp( "lan", szNetwork ) )
				szDisplay = szNetwork;

			pAccessLabel->SetText( CFmtStr( "#L4D360UI_Access_%s", szDisplay ) );
		}

		if ( DropDownMenu *drpGameAccess = dynamic_cast< DropDownMenu* >( FindChildByName( "DrpGameAccess" ) ) )
			drpGameAccess->SetCurrentSelection( CFmtStr( "GameAccess_%s", szAccess ) );
	}

	if ( char const *szLock = kvUpdate->GetString( "system/lock", NULL ) )
	{
		RemapText_t arrWaits[] = {
			{ "matching",	"#MatchLobby_matching", RemapText_t::MATCH_FULL },	// leader is searching for opponents
			{ "matchingsearchidle", "#MatchLobby_matching_searchidle", RemapText_t::MATCH_FULL },	// searching for opponents, search idle
			{ "matchingsearcherror", "#MatchLobby_matching_searcherror", RemapText_t::MATCH_FULL },	// searching for opponents error
			{ "matchingsearchawaitingpeer", "#MatchLobby_matching_searchidle", RemapText_t::MATCH_FULL },	// searching for opponents, search idle
			{ "matchingsearchlinked", "#MatchLobby_matching_searchlinked", RemapText_t::MATCH_FULL },	// linked with another team
			{ "matchingsearching", "#MatchLobby_matching", RemapText_t::MATCH_FULL },	// searching for opponents, initiating search
			{ "matchingsearchresult", "#MatchLobby_matching_searchresult", RemapText_t::MATCH_START },	// searching for opponents, have some results
			{ "matchingdedicated", "#MatchLobby_matching_dedicated", RemapText_t::MATCH_FULL },	// looking for dedicated server
			{ "matchingpeerserver", "#MatchLobby_matching_peerserver", RemapText_t::MATCH_FULL },	// peer is looking for server
			{ "starting",	"#MatchLobby_starting", RemapText_t::MATCH_FULL },	// leader is searching for server
			{ "loading",	"#MatchLobby_loading", RemapText_t::MATCH_FULL },	// leader is loading a listen server
			{ "ending",		"#MatchLobby_ending", RemapText_t::MATCH_FULL },	// game ended and loading into lobby
			{ "",	NULL, RemapText_t::MATCH_START },	// empty lock
			{ NULL, NULL, RemapText_t::MATCH_FULL }
		};

		char const *szStateText = RemapText_t::RemapRawText( arrWaits, szLock );
		SetStateText( szStateText, NULL );

		// Update the spinner
		if ( vgui::ImagePanel *pWorkingAnim = dynamic_cast< vgui::ImagePanel * >( FindChildByName( "WorkingAnim" ) ) )
			pWorkingAnim->SetVisible( szStateText != NULL );

		// Set the controls into locked/unlocked state
		SetControlsLockedState( bHost, szLock );
	}

	// Check the DLC required setting if it changed
	if ( uint64 uiDlcRequiredMask = kvUpdate->GetUint64( "game/dlcrequired" ) )
	{
		// Get client's dlc mask
		uint64 uiDlcInstalledMask = GetDlcInstalledMask();
		if ( ( uiDlcRequiredMask & uiDlcInstalledMask ) != uiDlcRequiredMask )
		{
			KeyValues *pLeaveLobbyMsg = m_pSettings ? m_pSettings->MakeCopy() : new KeyValues( "" );
			pLeaveLobbyMsg->SetName( "NoValidMissionChapter" );
			PostMessage( this, pLeaveLobbyMsg );
			return;
		}
	}

	KeyValues *pInfoMission = NULL;
	// TODO:
	KeyValues *pInfoChapter = NULL;		//GetMapInfoRespectingAnyChapter( m_pSettings, &pInfoMission );

	// If we do not have a valid chapter/mission, then we need to quit
	if ( !pInfoChapter || !pInfoMission ||
		 ( *pInfoMission->GetName() && pInfoMission->GetInt( "version" ) != m_pSettings->GetInt( "game/missioninfo/version", -1 ) ) )
	{
		KeyValues *pLeaveLobbyMsg = m_pSettings ? m_pSettings->MakeCopy() : new KeyValues( "" );
		pLeaveLobbyMsg->SetName( "NoValidMissionChapter" );
		PostMessage( this, pLeaveLobbyMsg );
		return;
	}

	if ( char const *szCampaign = kvUpdate->GetString( "game/campaign", NULL ) )
	{
		if ( vgui::Label * campaignLabel = GetSettingsSummaryLabel( "campaign" ) )
		{
			campaignLabel->SetText( pInfoMission ? pInfoMission->GetString( "displaytitle" ) :
				m_pSettings->GetString( "game/missioninfo/displaytitle", "#L4D360UI_CampaignName_Unknown" ) );
		}
	}

	if ( kvUpdate->GetInt( "game/chapter", -1 ) >= 0 )
	{
		if ( vgui::Label * mapLabel = GetSettingsSummaryLabel( "chapter" ) )
		{
			mapLabel->SetText( pInfoChapter ? pInfoChapter->GetString( "displayname" ) : "#L4D360UI_LevelName_Unknown" );
		}

		vgui::ImagePanel * imgLevelImage = dynamic_cast< vgui::ImagePanel * > ( FindChildByName( "ImgLevelImage" ) );
		if ( imgLevelImage )
		{
			char const *szMapImagePath = "maps/unknown";

			// Resolve to actual image path
			if ( pInfoChapter )
				szMapImagePath = pInfoChapter->GetString( "image" );

			imgLevelImage->SetImage( szMapImagePath );
			imgLevelImage->SetMouseInputEnabled( false );
		}
	}

	if ( char const *szDifficulty = kvUpdate->GetString( "game/difficulty", NULL ) )
	{
		if ( vgui::Label * difficultyLabel = GetSettingsSummaryLabel( "difficulty" ) )
		{
			char const *szGameMode = m_pSettings->GetString( "game/mode", "campaign" );
			if( !GameModeHasDifficulty( szGameMode ) )
			{
				difficultyLabel->SetText( CFmtStr( "#L4D360UI_Mode_%s", szGameMode ) );
			}
			else
			{
				difficultyLabel->SetText( CFmtStr( "#L4D360UI_Difficulty_%s_%s", szDifficulty, szGameMode ) );
			}
		}
	}

	if ( int nRoundLimit = kvUpdate->GetInt( "game/maxrounds" ) )
	{
		if ( vgui::Label * pLabel = GetSettingsSummaryLabel( "maxrounds" ) )
		{
			char const *szGameMode = m_pSettings->GetString( "game/mode", "campaign" );
			if( !GameModeHasRoundLimit( szGameMode ) )
			{
				pLabel->SetText( "" );
			}
			else
			{
				pLabel->SetText( CFmtStr( "#L4D360UI_RoundLimit_%d", nRoundLimit ) );
			}
		}
	}

	if ( char const *szServer = kvUpdate->GetString( "options/server", NULL ) )
	{
		if ( vgui::Label * serverTypeLabel = GetSettingsSummaryLabel( "server" ) )
		{
			if ( !Q_stricmp( "LIVE", m_pSettings->GetString( "system/network", "" ) ) )
			{
				char chBuffer[64] = {0};
				Q_snprintf( chBuffer, ARRAYSIZE( chBuffer ), "#L4D360UI_Lobby_ServerType_%s", szServer );
				serverTypeLabel->SetText( chBuffer );
			}
			else
			{
				serverTypeLabel->SetText( "" );
			}
		}
	}
}

#if !defined( _X360 ) && !defined( NO_STEAM )
void GameLobby::Steam_OnPersonaStateChanged( PersonaStateChange_t *pParam )
{
	if ( !m_bSubscribedForEvents )
		return;

	KeyValues *pPlayer = SessionMembersFindPlayer( m_pSettings, pParam->m_ulSteamID );
	if ( !pPlayer )
		return;

	Player_AddOrUpdate( pPlayer );
	SetLobbyLeaderText();
}
#endif

void GameLobby::SetLobbyLeaderText()
{
	IMatchSession *pMatchSession = g_pMatchFramework->GetMatchSession();
	XUID xuidLeader = pMatchSession ? pMatchSession->GetSessionSystemData()->GetUint64( "xuidHost" ) : 0ull;
	if ( !xuidLeader )
	{
		xuidLeader = m_pSettings->GetUint64( "members/machine0/id", 0ull );
	}
	if ( KeyValues *pPlayerLobbyLeader = SessionMembersFindPlayer( m_pSettings, xuidLeader ) )
	{
		vgui::Label *pLabel = dynamic_cast< vgui::Label * > ( FindChildByName( "LblLeaderLine" ) );
		if( pLabel )
		{
			char const *szName = pPlayerLobbyLeader->GetString( "name", "" );
			szName = CUIGameData::Get()->GetPlayerName( xuidLeader, szName );

			wchar_t nameBuffer[MAX_PLAYER_NAME_LENGTH] = {0};
			g_pVGuiLocalize->ConvertANSIToUnicode( szName, nameBuffer, sizeof(nameBuffer) );

			bool bTeamLobby = StringHasPrefix( m_pSettings->GetString( "game/mode", "" ), "team" );

			wchar_t finalMessage[MAX_PATH] = {0};
			if ( wchar_t* wMsg = g_pVGuiLocalize->Find( bTeamLobby ? "#L4D360UI_Lobby_TeamLeaderLine" : "#L4D360UI_Lobby_LeaderLine" ) )
			{
				g_pVGuiLocalize->ConstructString( finalMessage, sizeof( finalMessage ), wMsg, 1, nameBuffer );
			}

			pLabel->SetText( finalMessage );
		}
	}
}

void GameLobby::SetControlsState( bool bHost )
{
	char const *arrHostOnlyControls[] = {
		"BtnStartGame", "BtnChangeGameSettings",
		"DrpGameAccess"
	};

	for ( int k = 0; k < ARRAYSIZE( arrHostOnlyControls ); ++ k )
	{
		if ( vgui::Panel * panel = FindChildByName( arrHostOnlyControls[k] ) )
			panel->SetEnabled( bHost );
	}

	SetControlsOfflineState();
	UpdateStartButton();
}

void GameLobby::SetControlsLockedState( bool bHost, char const *szLock )
{
	// Controls that are shared on host and client
	char const *arrLockableControls[] = {
		"DrpInviteFriends", "DrpInviteFriends", "BtnInviteFriends", "DrpCharacter", "DrpVersusCharacter"
	};

	for ( int k = 0; k < ARRAYSIZE( arrLockableControls ); ++ k )
	{
		if ( vgui::Panel * panel = FindChildByName( arrLockableControls[k] ) )
			if ( panel->IsVisible() )
				panel->SetEnabled( !szLock[0] );
	}

	if ( bHost )
	{
		// Controls that are only ever enabled on the host
		char const *arrHostOnlyControls[] = {
			"BtnStartGame", "BtnChangeGameSettings",
			"DrpGameAccess"
		};

		for ( int k = 0; k < ARRAYSIZE( arrHostOnlyControls ); ++ k )
		{
			if ( vgui::Panel * panel = FindChildByName( arrHostOnlyControls[k] ) )
				panel->SetEnabled( !szLock[0] );
		}

		// Special control to cancel dedicated search
		if ( szLock &&
			( !Q_stricmp( "starting", szLock ) ||
			StringHasPrefix( szLock, "matching" ) ) )
		{
			if ( vgui::Panel * panel = FindChildByName( "BtnCancelDedicatedSearch" ) )
			{
				if ( !panel->IsVisible() )
				{
					panel->SetVisible( true );
					CBaseModPanel::GetSingletonPtr()->SafeNavigateTo( m_ActiveControl, panel, false );
				}
			}

			if ( vgui::Panel *panel = FindChildByName( "BtnStartGame" ) )
				panel->SetVisible( false );
		}
		else
		{
			if ( vgui::Panel * panel = FindChildByName( "BtnStartGame" ) )
			{
				if ( !panel->IsVisible() )
				{
					panel->SetVisible( true );
					CBaseModPanel::GetSingletonPtr()->SafeNavigateTo( m_ActiveControl, panel, false );
				}
			}

			if ( vgui::Panel * panel = FindChildByName( "BtnCancelDedicatedSearch" ) )
				panel->SetVisible( false );
		}
	}

	SetControlsOfflineState();
	UpdateStartButton();
}

void GameLobby::SetControlsOfflineState()
{
	char const *szNetwork = m_pSettings->GetString( "system/network", "LIVE" );

	// Some of the controls are never enabled in offline mode
	if ( Q_stricmp( "LIVE", szNetwork ) )
	{
		char const *arrOnlineOnlyControls[] = {
			"DrpGameAccess", "DrpInviteFriends", "BtnInviteFriends"
		};

		for ( int k = 0; k < ARRAYSIZE( arrOnlineOnlyControls ); ++ k )
		{
			if ( vgui::Panel * panel = FindChildByName( arrOnlineOnlyControls[k] ) )
				panel->SetEnabled( false );
		}
	}
}

void GameLobby::Player_Remove( XUID xuid )
{
	if ( !xuid )
		return;

	for ( int k = 0; k < m_pPlayersList->GetPanelItemCount(); ++ k )
	{
		CPlayerItem *pItem = dynamic_cast< CPlayerItem * >( m_pPlayersList->GetPanelItem( k ) );
		if ( !pItem )
			continue;
		
		KeyValues *pInfo = pItem->GetPlayerInfo();
		if ( xuid != pInfo->GetUint64( "xuid", 0ull ) )
			continue;

		DevMsg( "Lobby: removing player %llx\n", xuid );
		KeyValuesDumpAsDevMsg( pInfo, 1 );

		// If there is a flyout for this player open, we need to close it
		if ( xuid == m_xuidPlayerFlyout )
		{
			FlyoutMenu *flyoutLeader = dynamic_cast< FlyoutMenu * >( FindChildByName( "FlmPlayerFlyoutLeader" ) );
			FlyoutMenu *flyout = dynamic_cast< FlyoutMenu * >( FindChildByName( "FlmPlayerFlyout" ) );
			FlyoutMenu *activeFlyout = FlyoutMenu::GetActiveMenu();
			if ( activeFlyout && ( activeFlyout == flyoutLeader || activeFlyout == flyout ) && activeFlyout->IsVisible() )
				FlyoutMenu::CloseActiveMenu();
		}

		// We have found the item that is being removed, remove it and replace with a blank
		m_pPlayersList->RemovePanelItem( k );
		m_pPlayersList->AddPanelItem< CPlayerItem >( "SurvivorListItem" );
		return;
	}

	DevWarning( "Lobby: failed to remove player %llx\n", xuid );
}

void GameLobby::Player_AddOrUpdate( KeyValues *pPlayer )
{
	if ( !pPlayer )
		return;

	XUID xuid = pPlayer->GetUint64( "xuid", 0ull );
	if ( !xuid )
		return;

	if ( !m_pPlayersList->GetPanelItemCount() )
		return;

	// Try to find this item for update
	for ( int k = 0; k < m_pPlayersList->GetPanelItemCount(); ++ k )
	{
		CPlayerItem *pItem = dynamic_cast< CPlayerItem * >( m_pPlayersList->GetPanelItem( k ) );
		if ( !pItem )
			continue;

		KeyValues *pInfo = pItem->GetPlayerInfo();
		XUID xuidItem = pInfo->GetUint64( "xuid", 0ull );
		if ( xuid == xuidItem || !xuidItem )
		{
			if ( !xuidItem )
			{
				DevMsg( "Lobby: adding player %llx\n", xuid );
				KeyValuesDumpAsDevMsg( pPlayer, 1 );
			}

			// Update inplace
			pItem->SetPlayerInfo( pPlayer );

			// If adding a new player, need to relink navigation
			if ( !xuidItem )
				m_pPlayersList->RelinkNavigation();

			return;
		}
	}

	ExecuteNTimes( 10, DevWarning( "Lobby: failed to add/update player %llx, slot count = %d\n", xuid, m_pPlayersList->GetPanelItemCount() ) );
	Assert( 0 );
}

CPlayerItem * GameLobby::Player_Find( XUID xuid )
{
	// Try to find this item for update
	for ( int k = 0; k < m_pPlayersList->GetPanelItemCount(); ++ k )
	{
		CPlayerItem *pItem = dynamic_cast< CPlayerItem * >( m_pPlayersList->GetPanelItem( k ) );
		if ( !pItem )
			continue;

		KeyValues *pInfo = pItem->GetPlayerInfo();
		XUID xuidItem = pInfo->GetUint64( "xuid", 0ull );
		if ( xuid == xuidItem )
			return pItem;
	}

	return NULL;
}

vgui::Label * GameLobby::GetSettingsSummaryLabel( char const *szContentType )
{
	if ( !m_pLobbyDetailsLayout )
	{
		m_pLobbyDetailsLayout = new KeyValues( "LobbyDetailsSummary" );
		if ( !m_pLobbyDetailsLayout->LoadFromFile( g_pFullFileSystem, "resource/UI/BaseModUI/LobbyDetailsSummary.res", "MOD" ) )
		{
			DevWarning( "Failed to load lobby details summary information!\n" );
			m_pLobbyDetailsLayout->deleteThis();
			m_pLobbyDetailsLayout = NULL;
			m_pLobbyDetailsLayout = new KeyValues( "LobbyDetailsSummary" );
		}
		m_autodelete_pLobbyDetailsLayout.Assign( m_pLobbyDetailsLayout );
	}

	char const *szGameMode = m_pSettings->GetString( "game/mode", NULL );
	if ( !szGameMode )
		return NULL;

	char const *szSummaryField = m_pLobbyDetailsLayout->GetString( CFmtStr( "%s/%s", szGameMode, szContentType ), NULL );
	if ( !szSummaryField )
		return NULL;

	return dynamic_cast< vgui::Label * > ( FindChildByName( CFmtStr( "LblSummaryLine%s", szSummaryField ) ) );
}

void GameLobby::OnDropDown_Access( DropDownMenu *pDropDownMenu, FlyoutMenu *pFlyoutMenu )
{
	pFlyoutMenu->SetListener( dynamic_cast< FlyoutMenuListener * >( pDropDownMenu->GetParent() ) );
}

void GameLobby::OnDropDown_Character( DropDownMenu *pDropDownMenu, FlyoutMenu *pFlyoutMenu )
{
	pFlyoutMenu->SetListener( dynamic_cast< FlyoutMenuListener * >( pDropDownMenu->GetParent() ) );
}

}; // namespace BaseModUI

