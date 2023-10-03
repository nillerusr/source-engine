//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __VINGAMECHAPTERSELECT_H__
#define __VINGAMECHAPTERSELECT_H__

#include "basemodui.h"
#include "VFlyoutMenu.h"

namespace BaseModUI {

	class InGameChapterSelect : public CBaseModFrame, public FlyoutMenuListener
	{
		DECLARE_CLASS_SIMPLE( InGameChapterSelect, CBaseModFrame );

	public:
		InGameChapterSelect(vgui::Panel *parent, const char *panelName);
		~InGameChapterSelect();

		virtual void PaintBackground();
		virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
		virtual void Activate( void );
		virtual void OnCommand(const char *command);

	private:
		static void StartNewCampaignOkCallback();
		static void StartNewCampaignCancelCallback();

		// IFlyoutMenuListener
		virtual void OnNotifyChildFocus( vgui::Panel* child );
		virtual void OnFlyoutMenuClose( vgui::Panel* flyTo );
		virtual void OnFlyoutMenuCancelled();

		void UpdateChapterImage( char const *szMission, int nChapter, char const *szForceImage = NULL );

	private:
		vgui::Button* m_BtnStartNewCampaign;

		char m_chCampaign[128];
		int m_nChapter;
	};

}

#endif // __VINGAMECHAPTERSELECT_H__