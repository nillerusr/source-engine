//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __VCUSTOMCAMPAIGNS_H__
#define __VCUSTOMCAMPAIGNS_H__

#include "basemodui.h"
#include "VGenericPanelList.h"

namespace BaseModUI {

class CustomCampaignGenericPanelList;
class BaseModHybridButton;

class CustomCampaignListItem : public vgui::EditablePanel, IGenericPanelListItem
{
	DECLARE_CLASS_SIMPLE( CustomCampaignListItem, vgui::EditablePanel );

public:
	CustomCampaignListItem(vgui::Panel *parent, const char *panelName);

	void SetCustomCampaignName( const char *pcName );

	// Inherited from IGenericPanelListItem
	virtual bool IsLabel() { return false; }
	void OnMousePressed( vgui::MouseCode code );
	virtual void OnMessage(const KeyValues *params, vgui::VPANEL ifromPanel);
	virtual void Paint();

	void SetCampaignContext( char const *szCampaignContext );
	char const * GetCampaignContext() const;

protected:
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);

private:
	bool m_bCurrentlySelected;
	vgui::Label* m_LblName;
	char m_campaignContext[256];
	vgui::HFont	m_hTextFont;
};

class CustomCampaigns : public CBaseModFrame
{
	DECLARE_CLASS_SIMPLE( CustomCampaigns, CBaseModFrame );

public:
	CustomCampaigns(vgui::Panel *parent, const char *panelName);
	~CustomCampaigns();
	void Activate();
	void PaintBackground( void );
	void OnCommand(const char *command);

	MESSAGE_FUNC_CHARPTR( OnItemSelected, "OnItemSelected", panelName );

	virtual void SetDataSettings( KeyValues *pSettings );

protected:
	void ApplySchemeSettings(vgui::IScheme *pScheme);
	void Select();

private:
	GenericPanelList* m_GplCustomCampaigns;

	KeyValues *m_pDataSettings;

	bool m_hasAddonCampaign;
	bool m_SomeAddonNoSupport;

	vgui::ImagePanel *m_imgLevelImage;
	vgui::Label *m_lblNoCustomCampaigns;
	vgui::Label *m_lblAuthor;
	vgui::Label *m_lblWebsite;
	vgui::Label *m_lblDescription;
	BaseModHybridButton *m_btnSelect;
	BaseModHybridButton *m_btnCancel;
};

};

#endif

