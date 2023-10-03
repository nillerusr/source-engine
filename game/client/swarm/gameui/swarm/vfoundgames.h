//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __VFOUNDGAMES_H__
#define __VFOUNDGAMES_H__

#include "basemodui.h"
#include "matchmaking/imatchframework.h"

class CNB_Header_Footer;

namespace BaseModUI {

class GenericPanelList;
class FoundGames;
class BaseModHybridButton;

class FoundGameListItem : public vgui::EditablePanel, public IBaseModFrameListener
{
	DECLARE_CLASS_SIMPLE( FoundGameListItem, vgui::EditablePanel );

public:
	enum Type_t
	{
		FGT_UNKNOWN,
		FGT_PLAYER,
		FGT_SERVER,
		FGT_PUBLICGAME,
	};

	struct Info
	{
		Type_t mInfoType;
		char Name[64];
		bool mIsJoinable;
		bool mbDLC;
		char mchOtherTitle[64];
		bool mbInGame; // If I have a presence party

		enum GAME_PING { GP_LOW, GP_MEDIUM, GP_HIGH, GP_SYSTEMLINK, GP_NONE } mPing;
		KeyValues *mpGameDetails;
		XUID mFriendXUID;
		int miPing;		// actual ping value

		void (*mpfnJoinGame)( Info const& fi );

		Info()
		{
			mInfoType = FGT_UNKNOWN;
			Name[0] = 0;
			mchOtherTitle[0] = 0;
			mIsJoinable = false;
			mbDLC = false;
			mbInGame = false;
			mPing = GP_NONE;
			mFriendXUID = 0;
			miPing = 0;
			mpGameDetails = NULL;
			mpfnJoinGame = NULL;
		}

		bool IsJoinable() const;
		bool IsDownloadable() const;
		bool IsDLC() const;
		char const * IsOtherTitle() const;

		char const * GetNonJoinableShortHint() const;
		char const * GetJoinButtonHint() const;
	};

public:
	FoundGameListItem( vgui::Panel *parent, const char *panelName );
	~FoundGameListItem();

public:
	void SetGameIndex( const Info& fi );
	const Info& GetFullInfo();

	void SetGamerTag( char const* gamerTag );
	void SetAvatarXUID( XUID xuid );

	void SetGamePing( Info::GAME_PING ping );
	void SetGameDifficulty( const char* difficultyName );
	void SetSwarmState( const char* szSwarmStateText );
	void SetGamePlayerCount( int current, int max );

	void DrawListItemLabel( vgui::Label* label, bool bSmallFont, bool bEastAligned = false );

	void SetSweep( bool sweep );
	bool IsSweep() const;

	void RunFrame() {};

	virtual void PaintBackground();
	void OnKeyCodePressed( vgui::KeyCode code );
	void OnKeyCodeTyped( vgui::KeyCode code );
	void OnMousePressed( vgui::MouseCode code );
	void OnMouseDoublePressed( vgui::MouseCode code );

	virtual void OnCursorEntered();
	//virtual void OnCursorExited() { SetHasMouseover( false ); }
	virtual void NavigateTo( void );
	virtual void NavigateFrom( void );

	bool IsSelected( void ) { return m_bSelected; }
	void SetSelected( bool bSelected ) { m_bSelected = bSelected; }

	bool HasMouseover( void ) { return m_bHasMouseover; }
	void SetHasMouseover( bool bHasMouseover ) { m_bHasMouseover = bHasMouseover; }

	void SetFocusBgColor( Color focusColor );
	void SetOutOfFocusBgColor( Color outOfFocusBgColor );

protected:
	void ApplySettings( KeyValues *inResourceData );
	void ApplySchemeSettings( vgui::IScheme *pScheme );
	void PerformLayout();

protected:
	MESSAGE_FUNC( CmdJoinGame, "JoinGame" );
	void CmdViewGamercard();

private:
	Info m_FullInfo;

	Color m_OutOfFocusBgColor;
	Color m_FocusBgColor;

	GenericPanelList	*m_pListCtrlr;
	vgui::ImagePanel	*m_pImgPing;
	vgui::Label			*m_pLblPing;
	vgui::Label			*m_pLblPlayerGamerTag;
	vgui::Label			*m_pLblDifficulty;
	vgui::Label			*m_pLblSwarmState;
	vgui::Label			*m_pLblPlayers;
	vgui::Label			*m_pLblNotJoinable;

	vgui::HFont	m_hTextFont;
	vgui::HFont	m_hTextBlurFont;

	vgui::HFont	m_hSmallTextFont;
	vgui::HFont	m_hSmallTextBlurFont;

	CPanelAnimationVar( Color, m_SelectedColor, "selected_color", "255 0 0 128" );
	bool m_sweep : 1;
	bool m_bSelected : 1;
	bool m_bHasMouseover : 1;

};

//=============================================================================

class FoundGames : public CBaseModFrame, public IBaseModFrameListener, public IMatchEventsSink
{
	DECLARE_CLASS_SIMPLE( FoundGames, CBaseModFrame );

public:
	FoundGames( vgui::Panel *parent, const char *panelName );
	~FoundGames();

	// IMatchEventsSink
public:
	virtual void OnEvent( KeyValues *pEvent );

public:
	void Activate();
	void OnCommand( const char *command );
	void OnKeyCodePressed( vgui::KeyCode code );
	void OnThink();
	void RunFrame() {};
	void PaintBackground();
	void LoadLayout();

	void UpdateFooterButtons();
	virtual void UpdateTitle();

#ifdef _X360
	void NavigateTo();
#endif // _X360
	
	void SetFoundDesiredText( bool bFoundGame );

	void SetDetailsPanelVisible( bool bIsVisible );

	virtual bool AddGameFromDetails( const FoundGameListItem::Info &fi );
	void UpdateGameDetails();

	MESSAGE_FUNC_CHARPTR( OnItemSelected, "OnItemSelected", panelName );

protected:
	void ApplySchemeSettings( vgui::IScheme *pScheme );
	FoundGameListItem* GetGameItem( int index );

	virtual void OnClose();
	virtual void OnOpen();

	virtual void SetDataSettings( KeyValues *pSettings );

	void OpenPlayerFlyout( BaseModHybridButton *button, uint64 playerId, int x, int y );

	virtual void StartSearching();
	virtual void AddFakeServersToList();
	virtual void AddServersToList();
	virtual void SortListItems();
	virtual char const * GetListHeaderText() { return NULL; }

	virtual bool IsADuplicateServer( FoundGameListItem *item, FoundGameListItem::Info const &fi );
	virtual bool CanCreateGame();

protected:
	float m_flSearchStartedTime, m_flSearchEndTime;
	float m_flScheduledUpdateGameDetails;

	GenericPanelList* m_GplGames;

	float m_LastEngineSpinnerTime;
	int m_CurrentSpinnerValue;

	uint64 m_SelectedGamePlayerID;
	uint64 m_flyoutPlayerId;

	FoundGameListItem *m_pPreviousSelectedItem;

	KeyValues *m_pDataSettings;

	vgui::Label *m_pTitle;
	CNB_Header_Footer *m_pHeaderFooter;
};

};

#endif
