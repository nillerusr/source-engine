#ifndef TILEGEN_ROOMTEMPLATEPANEL_H
#define TILEGEN_ROOMTEMPLATEPANEL_H
#ifdef _WIN32
#pragma once
#endif

class CRoomTemplate;
class CMissionChooserTGAImagePanel;

namespace vgui
{
	class Menu;
};

// draws a particular room template (based on its image and grid/exits)
class CRoomTemplatePanel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CRoomTemplatePanel, vgui::Panel );

public:

	CRoomTemplatePanel(Panel *parent, const char *name);
	virtual ~CRoomTemplatePanel();

	virtual void PerformLayout();
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void OnMouseReleased(vgui::MouseCode code);

	void SetRoomTemplate( const CRoomTemplate* pTemplate );	
	void UpdateImages();
	const CRoomTemplate* m_pRoomTemplate;

	CMissionChooserTGAImagePanel* m_pRoomTGAPanel;
	CUtlVector<CMissionChooserTGAImagePanel*> m_pGridImagePanels;
	CUtlVector<CMissionChooserTGAImagePanel*> m_pExitImagePanels;
	vgui::Panel* m_pSelectedOutline;

	int m_iLastTilesX, m_iLastTilesY;	

	bool m_bRoomTemplateEditMode;	// if set to true, this panel is in template edit model and right clicking should bring up exit manipulation menu
	bool m_bRoomTemplateBrowserMode;	// if set to true, this panel is in the template browser, and right clicking should edit our room template
	vgui::DHANDLE< vgui::Menu >	m_hMenu;

	bool m_bForceShowExits;
	bool m_bForceShowTileSquares;
	static void UpdateAllImages();	// causes all the RoomTemplatePanels to update their images, tile grid squares and exits	

	vgui::Label *m_pTagsLabel;
};

#endif TILEGEN_ROOMTEMPLATEPANEL_H