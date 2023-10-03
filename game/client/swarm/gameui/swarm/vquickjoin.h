#ifndef __QUICKJOIN_H__
#define __QUICKJOIN_H__

#include "basemodui.h"

namespace BaseModUI
{
	class GenericPanelList;
	class QuickJoinPanelItem;

	class QuickJoinPanel : public vgui::EditablePanel
	{
		DECLARE_CLASS_SIMPLE( QuickJoinPanel, vgui::EditablePanel );
	public:
		struct QuickInfo
		{
			enum Type_t
			{
				TYPE_UNKNOWN,
				TYPE_PLAYER,
				TYPE_SERVER,
			} m_eType;
			XUID m_xuid;
			char m_szName[ MAX_PLAYER_NAME_LENGTH ];
		};

		QuickJoinPanel( vgui::Panel* parent, const char* panelName );
		virtual ~QuickJoinPanel();

		virtual bool ShouldBeVisible() const;

		void OnThink();
		void OnTick();

		virtual void NavigateToChild( Panel *pNavigateTo );
		virtual void NavigateTo();
		virtual void NavigateFrom();

		virtual void OnCursorEntered();

		bool HasMouseover( void ) { return m_bHasMouseover; }
		void SetHasMouseover( bool bHasMouseover ) { m_bHasMouseover = bHasMouseover; }

		int GetSmoothPanelY( int iCurrentY, int iDesiredY );

		QuickJoinPanelItem* GetGame( int index );
		QuickJoinPanelItem* AddGame( int iPanelIndex, QuickInfo const &qi, char *name );
		QuickJoinPanelItem* AddGame( int iPanelIndex, QuickInfo const &qi, wchar_t *name );
		void RemoveGame( int index );
		virtual void OnCommand(const char *command);
		virtual void OnMousePressed( vgui::MouseCode code );
		
	protected:
		void ApplySettings( KeyValues *inResourceData );
		void PerformLayout();
		void ClearGames();

		void UpdateNumGamesFoundLabel( void );
		virtual const char *GetTitle( void ) { return "#L4D360UI_MainMenu_FriendsPlaying"; }

		virtual void AddServersToList( void );
		void RefreshContents( int iWrap );

	protected:
		char *m_resFile;
		GenericPanelList* m_GplQuickJoinList;

		CUtlVector< QuickInfo > m_FriendInfo;
		int m_iPrevWrap;
		float m_flScrollSpeed;
		float m_flScrollAccum;

		bool m_bHasMouseover : 1;
	};

	class QuickJoinPanelItem : public vgui::EditablePanel
	{
		DECLARE_CLASS_SIMPLE( QuickJoinPanelItem, vgui::EditablePanel );

	public:
		QuickJoinPanelItem( vgui::Panel* parent, const char* panelName );
		virtual ~QuickJoinPanelItem();

#ifdef _X360
		virtual void NavigateTo();
		virtual void NavigateFrom();
#endif // _X360

		void SetInfo( QuickJoinPanel::QuickInfo const &qi );
		const wchar_t *GetName() const;
		void SetName( wchar_t *name );

		void Update();

		void SetItemTooltip( const char* tooltip );
		const char* GetItemTooltip();
		bool GetText( char* outText, int buffLen );
		void ApplySchemeSettings( vgui::IScheme *pScheme );


	protected:
		virtual void PaintBackground();
		virtual void ApplySettings( KeyValues* inResourceData );
		void PerformLayout();

	protected:
		Color m_FocusBgColor;
		Color m_UnfocusBgColor;

		const char* m_tooltip;

		QuickJoinPanel::QuickInfo m_info;
		wchar_t m_name[128];
	};

};

#endif