#ifndef __QUICKJOIN_GROUPS_H__
#define __QUICKJOIN_GROUPS_H__

#include "VQuickJoin.h"

namespace BaseModUI
{
	class QuickJoinGroupsPanel : public QuickJoinPanel
	{
		DECLARE_CLASS_SIMPLE( QuickJoinGroupsPanel, QuickJoinPanel );

	public:
		QuickJoinGroupsPanel( vgui::Panel* parent, const char* panelName );

		virtual void OnMousePressed( vgui::MouseCode code );
		virtual void OnCommand(const char *command);

	protected:
		virtual void AddServersToList( void );
		virtual const char *GetTitle( void ) { return "#L4D360UI_MainMenu_SteamGroupServers"; }

	};
};

#endif	// __QUICKJOIN_GROUPS_H__