//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __VGAMESETTINGS_H__
#define __VGAMESETTINGS_H__

#include "basemodui.h"
#include "VFlyoutMenu.h"

class CNB_Header_Footer;

namespace BaseModUI {

class DropDownMenu;

class GameSettings : public CBaseModFrame, public FlyoutMenuListener
{
	DECLARE_CLASS_SIMPLE( GameSettings, CBaseModFrame );

public:
	GameSettings(vgui::Panel *parent, const char *panelName);
	~GameSettings();

	void Activate();

	virtual void OnThink();

	void OnKeyCodePressed( vgui::KeyCode code );
	void OnCommand(const char *command);
	virtual void ApplySchemeSettings( vgui::IScheme* pScheme );
	virtual void PaintBackground();
	virtual void SetDataSettings( KeyValues *pSettings );
	
	//FloutMenuListener
	virtual void OnNotifyChildFocus( vgui::Panel* child );
	virtual void OnFlyoutMenuClose( vgui::Panel* flyTo );
	virtual void OnFlyoutMenuCancelled();

protected:
	virtual void OnClose();

	void Navigate();

	bool IsCustomMatchSearchCriteria();
	bool IsEditingExistingLobby();

protected:
	void UpdateMissionImage();
	int CountChaptersInCurrentCampaign();

	void SelectNetworkAccess( char const *szNetworkType, char const *szAccessType );
	void DoCustomMatch( char const *szGameState );

	void UpdateSelectMissionButton();
	void ShowMissionSelect();
	void ShowStartingMissionSelect();

private:
	void UpdateFooter();

	KeyValues *m_pSettings;
	KeyValues::AutoDelete m_autodelete_pSettings;
	
	bool m_bEditingSession;
	bool m_bPreventSessionModifications;
	void UpdateSessionSettings( KeyValues *pUpdate );

	DropDownMenu* m_drpDifficulty;
	DropDownMenu* m_drpGameType;
	DropDownMenu* m_drpGameAccess;
	DropDownMenu* m_drpServerType;
	DropDownMenu* m_drpStartingMission;
	DropDownMenu* m_drpFriendlyFire;
	DropDownMenu* m_drpOnslaught;
	CNB_Header_Footer *m_pHeaderFooter;
	vgui::Label *m_pTitle;

	bool m_bBackButtonMeansDone;
	bool m_bCloseSessionOnClose;
	bool m_bAllowChangeToCustomCampaign;

	vgui::DHANDLE<vgui::Panel> m_hSubScreen;
};

}

#endif
