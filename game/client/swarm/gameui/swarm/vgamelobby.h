//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __VGAMELOBBY_H__
#define __VGAMELOBBY_H__

#include "basemodui.h"
#include "VFlyoutMenu.h"
#include "VDropDownMenu.h"
#include "VGameLobbyChat.h"

namespace BaseModUI {

class GenericPanelList;
class BaseModHybridButton;
class CPlayersList;
class CPlayerItem;

class GameLobby : public CBaseModFrame, public FlyoutMenuListener, public IMatchEventsSink
{
	DECLARE_CLASS_SIMPLE( GameLobby, CBaseModFrame );

public:
	GameLobby(vgui::Panel *parent, const char *panelName);
	~GameLobby();

	// IMatchEventsSink implementation
public:
	virtual void OnEvent( KeyValues *pEvent );

public:
	virtual void SetDataSettings( KeyValues *pSettings );
	virtual void OnThink();
	virtual void OnTick();
	virtual void OnNavigateTo( const char *panelName );

	void NotifyLobbyNotIdleActivity();

	void OnCommand( const char *command );
	virtual void OnNotifyChildFocus( vgui::Panel* child );
	virtual void OnFlyoutMenuClose( vgui::Panel* flyTo ) {}
	virtual void OnFlyoutMenuCancelled() {}
	virtual void NavigateToChild( Panel *pNavigateTo );

	void OnKeyCodePressed( vgui::KeyCode code );

	void OpenPlayerFlyout( CPlayerItem *pPlayerItem );

	void OnOpen();
	void OnClose();

	MESSAGE_FUNC_PARAMS( MsgNoValidMissionChapter, "NoValidMissionChapter", pSettings );
	MESSAGE_FUNC( MsgChangeGameSettings, "ChangeGameSettings" );

#if !defined( _X360 ) && !defined( NO_STEAM )
	STEAM_CALLBACK( GameLobby, Steam_OnPersonaStateChanged, PersonaStateChange_t, m_CallbackPersonaStateChanged );
#endif

protected:
	virtual void PaintBackground();
	virtual void ApplySchemeSettings( vgui::IScheme* pScheme );

	// void NavigateTo_ChatHandler( Panel *pNavigateTo ); //special functionality for navigating the menu and chatting at the same time

	void ClockSpinner();
	void UpdateFooterButtons();
	void UpdateLiveWarning();
	void SetStateText( const char *pText, const wchar_t *pFormattedText );
	void SetLobbyLeaderText();
	void SetControlsState( bool bHost );
	void SetControlsLockedState( bool bHost, char const *szLock );
	void SetControlsOfflineState();

	void LeaveLobby();

	void InitializeFromSettings();
	void InitializePlayersList();
	void ApplyUpdatedSettings( KeyValues *kvUpdate );

	void Player_AddOrUpdate( KeyValues *pPlayer );
	void Player_Remove( XUID xuid );
	CPlayerItem * Player_Find( XUID xuid );

	vgui::Label * GetSettingsSummaryLabel( char const *szContentType );

	void UpdatePlayersChanged();
	void UpdateAvatars();
	void UpdateStartButton();

protected:
	static void OnDropDown_Access( DropDownMenu *pDropDownMenu, FlyoutMenu *pFlyoutMenu );
	static void OnDropDown_Character( DropDownMenu *pDropDownMenu, FlyoutMenu *pFlyoutMenu );

private:
	KeyValues *m_pSettings;
	KeyValues *m_pLobbyDetailsLayout;
	KeyValues::AutoDelete m_autodelete_pLobbyDetailsLayout;
	bool	m_bNoCommandHandling;
	bool	m_bSubscribedForEvents;

	float	m_flLastLobbyActivityTime;

	char	m_StateText[256];
	wchar_t	m_FormattedStateText[256];

	int m_nMsgBoxId;
	XUID m_xuidPlayerFlyout;

	CPlayersList *m_pPlayersList;

	CPanelAnimationVarAliasType( int, m_iTitleXOffset, "title_xpos", "0", "proportional_int" )
};


}

#endif // __VGAMELOBBY_H__
