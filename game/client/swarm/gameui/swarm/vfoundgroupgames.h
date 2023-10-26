//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __VFOUND_GROUP_GAMES_H__
#define __VFOUND_GROUP_GAMES_H__

#include "VFoundGames.h"

namespace BaseModUI {

	class GenericPanelList;
	class FoundGames;
	class BaseModHybridButton;


	//=============================================================================

	class FoundGroupGames : public FoundGames
	{
		DECLARE_CLASS_SIMPLE( FoundGroupGames, FoundGames );

	public:
		FoundGroupGames( vgui::Panel *parent, const char *panelName );
	
		virtual void PaintBackground();
		virtual void OnEvent( KeyValues *pEvent );

		virtual bool CanCreateGame() { return false; }
		
	protected:

		virtual void StartSearching( void );
		virtual void AddServersToList( void );
		virtual void SortListItems();
		virtual bool IsADuplicateServer( FoundGameListItem *item, FoundGameListItem::Info const &fi );

		bool ShouldAddServerToList( KeyValues *pGameSettings );
	};

};

#endif
