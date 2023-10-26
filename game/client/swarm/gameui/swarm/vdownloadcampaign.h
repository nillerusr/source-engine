//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __VDOWNLOADCAMPAIGN_H__
#define __VDOWNLOADCAMPAIGN_H__

#include "vgui_controls/CvarToggleCheckButton.h"
#include "gameui_util.h"

#include "basemodui.h"

namespace BaseModUI {

class DownloadCampaign : public CBaseModFrame
{
	DECLARE_CLASS_SIMPLE( DownloadCampaign, CBaseModFrame );
public:
	DownloadCampaign(vgui::Panel *parent, const char *panelName);
	~DownloadCampaign();

	virtual void SetDataSettings( KeyValues *pSettings );

protected:
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void OnCommand(const char *command);
	virtual void PaintBackground();
	void UpdateText();

private:
//	vgui::CvarToggleCheckButton<CGameUIConVarRef> *m_pDoNotShowWarning;

	CUtlString m_campaignName;
	CUtlString m_author;
	CUtlString m_webSite;
	bool m_fromLobby;
	bool m_bDownload;
};

};

#endif

