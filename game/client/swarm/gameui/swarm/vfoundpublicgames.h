//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __VFOUND_PUBLIC_GAMES_H__
#define __VFOUND_PUBLIC_GAMES_H__

#include "basemodui.h"
#include "VFoundGames.h"

namespace BaseModUI {

	class GenericPanelList;
	class FoundGames;
	class BaseModHybridButton;


	//=============================================================================

	class FoundPublicGames : public FoundGames
	{
		DECLARE_CLASS_SIMPLE( FoundPublicGames, FoundGames );

	public:
		FoundPublicGames( vgui::Panel *parent, const char *panelName );
		~FoundPublicGames();
	
		virtual void PaintBackground();
		virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
		virtual void OnCommand( const char *command );
		virtual void Activate();
		virtual void OnEvent( KeyValues *pEvent );
		virtual void OnKeyCodePressed( vgui::KeyCode code );
		
	protected:

		virtual void StartSearching( void );
		virtual void AddServersToList( void );
		virtual void SortListItems();
		virtual bool IsADuplicateServer( FoundGameListItem *item, FoundGameListItem::Info const &fi );
		virtual char const * GetListHeaderText();

		virtual void UpdateTitle();

	private:
		bool ShouldShowPublicGame( KeyValues *pGameDetails );
		void UpdateFilters( bool newState );
		bool CanCreateGame();

#if !defined( _X360 ) && !defined( NO_STEAM )
		CCallResult<FoundPublicGames, NumberOfCurrentPlayers_t> m_callbackNumberOfCurrentPlayers;
		void Steam_OnNumberOfCurrentPlayers( NumberOfCurrentPlayers_t *pResult, bool bError );
#endif

		DropDownMenu* m_drpDifficulty;
		DropDownMenu* m_drpOnslaught;
		DropDownMenu* m_drpGameStatus;
		DropDownMenu* m_drpCampaign;
		BaseModUI::BaseModHybridButton *m_btnFilters;

		vgui::Label *m_pSupportRequiredDetails;
		BaseModUI::BaseModHybridButton *m_pInstallSupportBtn;

		int m_numCurrentPlayers;
		vgui::EditablePanel *m_pSupportRequiredPanel;
		vgui::EditablePanel *m_pInstallingSupportPanel;
	public:
		ISearchManager *m_pSearchManager;
	};

};

#endif
