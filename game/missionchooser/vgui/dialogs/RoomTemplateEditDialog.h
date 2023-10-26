#ifndef ROOMTEMPLATEEDITDIALOG_H
#define ROOMTEMPLATEEDITDIALOG_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Frame.h>
#include "RoomTemplate.h"

namespace vgui {
	class PanelListPanel;
	class Label;
	class ImagePanel;
	class TextEntry;
	class RichText;
	class Button;
	class ComboBox;
};

class CRoomTemplatePanel;
class CToggleExitsPanel;

// this dialog allows you to edit a room template's properties

class CRoomTemplateEditDialog : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CRoomTemplateEditDialog, vgui::Frame );

public:

	CRoomTemplateEditDialog( vgui::Panel *parent, const char *name, CRoomTemplate *pRoomTemplate, bool bCreatingNew );
	virtual ~CRoomTemplateEditDialog();

	virtual void OnCommand( const char *command );
	void AddExit(int iXPos, int iYPos, ExitDirection_t dir);
	void EditExit(int iXPos, int iYPos, ExitDirection_t dir);
	void ToggleExit(int iXPos, int iYPos, ExitDirection_t dir);

	void DoPickVMF();
	void OnFileSelected(const char *fullpath);

	const char* GetVMFFilename();

	MESSAGE_FUNC_PTR( OnSliderMoved, "SliderMoved", panel );
	MESSAGE_FUNC_PTR( OnCheckButtonChecked, "CheckButtonChecked", panel );
	MESSAGE_FUNC_PTR( OnTextChanged, "TextChanged", panel );
	MESSAGE_FUNC_PARAMS( OnCheckOutFromP4, "CheckOutFromP4", pKV );
	MESSAGE_FUNC_PARAMS( OnAddToP4, "AddToP4", pKV );
	DECLARE_PANELMAP();	

	CRoomTemplatePanel* m_pRoomTemplatePanel;
	CToggleExitsPanel* m_pToggleExitsPanel;

	vgui::Button* m_pPickVMFButton;
	vgui::Button* m_pCancelButton;
	vgui::Label* m_pThemeLabel;
	vgui::TextEntry* m_pRoomTemplateNameEdit;
	vgui::TextEntry* m_pRoomTemplateDescriptionEdit;
	vgui::TextEntry* m_pRoomTemplateSoundscapeEdit;
	vgui::PanelListPanel *m_pTagListPanel;

	vgui::Slider* m_pSpawnWeightSlider;
	vgui::Label* m_pSpawnWeightValue;

	vgui::ComboBox* m_pTileTypeBox;

	vgui::Slider* m_pTilesXSlider;
	vgui::Slider* m_pTilesYSlider;
	vgui::Label* m_pTilesXValue;
	vgui::Label* m_pTilesYValue;

	// tile coords set when you right click on the room template panel
	int m_iSelectedTileX, m_iSelectedTileY;
	
	CRoomTemplate* m_pRoomTemplate;
	bool m_bCreatingNew;
};

#endif // ROOMTEMPLATEEDITDIALOG_H