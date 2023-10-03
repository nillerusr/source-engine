//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __VINGAMEKICKPLAYERLIST_H__
#define __VINGAMEKICKPLAYERLIST_H__

#include "basemodui.h"

namespace BaseModUI {

	class InGameKickPlayerList : public CBaseModFrame
	{
		DECLARE_CLASS_SIMPLE( InGameKickPlayerList, CBaseModFrame );

	public:
		InGameKickPlayerList(vgui::Panel *parent, const char *panelName);

		virtual void PaintBackground();
		virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
		virtual void LoadLayout( void );

		void OnCommand(const char *command);

	private:
		CUtlVector<int> m_KickablePlayersUserIDs;
	};

};

#endif // __VINGAMEKICKPLAYERLIST_H__