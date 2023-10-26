#ifndef _DEFINED_ASW_VGUI_EDIT_EMITTER_DIALOGS_H
#define _DEFINED_ASW_VGUI_EDIT_EMITTER_DIALOGS_H

#include <vgui_controls/Frame.h>
#include <vgui_controls/PropertyPage.h>
#include <vgui_controls/Slider.h>
#include <vgui/IScheme.h>
#include "vgui_controls\PanelListPanel.h"
#include "vgui_controls\ComboBox.h"

class CASW_VGUI_Edit_Emitter;

// dialog used for naming and saving particle emitter templates

class CASW_VGUI_Edit_Emitter_SaveDialog : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CASW_VGUI_Edit_Emitter_SaveDialog, vgui::Frame );

public:
	//CASW_VGUI_Edit_Emitter_SaveDialog( vgui::Panel *pParent, const char *pElementName );
	CASW_VGUI_Edit_Emitter_SaveDialog( vgui::Panel *pParent, const char *pElementName, CASW_VGUI_Edit_Emitter* pEditFrame );
	virtual ~CASW_VGUI_Edit_Emitter_SaveDialog();

	MESSAGE_FUNC_PTR( TextEntryChanged, "TextChanged", panel );
	MESSAGE_FUNC( SaveButtonClicked, "SaveDButton" );
	MESSAGE_FUNC( CancelButtonClicked, "CancelDButton" );

	vgui::TextEntry* m_pSaveText;
	vgui::Button* m_pSaveButton;
	vgui::Button* m_pCancelButton;
	CASW_VGUI_Edit_Emitter* m_pEditFrame;
	vgui::Label* m_pExistsLabel;
};


#endif /* _DEFINED_ASW_VGUI_EDIT_EMITTER_DIALOGS_H */