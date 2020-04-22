//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//


#ifndef CControlCatalog_H
#define CControlCatalog_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Frame.h>
#include <utlvector.h>

namespace vgui
{
	class ComboBox;
	class Label;
};

using namespace vgui;

Panel* ImageDemo_Create(Panel *parent);
Panel* ImagePanelDemo_Create(Panel *parent);
Panel* TextImageDemo_Create(Panel *parent);
Panel* LabelDemo_Create(Panel *parent);
Panel* Label2Demo_Create(Panel *parent);
Panel* TextEntryDemo_Create(Panel *parent);
Panel* TextEntryDemo2_Create(Panel *parent);
Panel* TextEntryDemo3_Create(Panel *parent);
Panel* TextEntryDemo4_Create(Panel *parent);
Panel* TextEntryDemo5_Create(Panel *parent);

Panel* MenuDemo_Create(Panel *parent);
Panel* MenuDemo2_Create(Panel *parent);
Panel* CascadingMenuDemo_Create(Panel *parent);

Panel* ButtonDemo_Create(Panel *parent);
Panel* ButtonDemo2_Create(Panel *parent);
Panel* CheckButtonDemo_Create(Panel *parent);
Panel* ToggleButtonDemo_Create(Panel *parent);
Panel* RadioButtonDemo_Create(Panel *parent);

Panel* MessageBoxDemo_Create(Panel *parent);
Panel* QueryBoxDemo_Create(Panel *parent);
Panel* ComboBoxDemo_Create(Panel *parent);
Panel* ComboBox2Demo_Create(Panel *parent);

Panel* FrameDemo_Create(Panel *parent);
Panel* ProgressBarDemo_Create(Panel *parent);
Panel* ScrollBarDemo_Create(Panel *parent);
Panel* ScrollBar2Demo_Create(Panel *parent);

Panel* EditablePanelDemo_Create(Panel *parent);
Panel* EditablePanel2Demo_Create(Panel *parent);

Panel* ListPanelDemo_Create(Panel *parent);

Panel* TooltipsDemo_Create(Panel *parent);
Panel* AnimatingImagePanelDemo_Create(Panel *parent);
Panel* WizardPanelDemo_Create(Panel *parent);
Panel* FileOpenDemo_Create(Panel *parent);


Panel* SampleButtons_Create(Panel *parent);
Panel* SampleMenus_Create(Panel *parent);
Panel* SampleDropDowns_Create(Panel *parent);
Panel* SampleListPanelColumns_Create(Panel *parent);
Panel* SampleListPanelCats_Create(Panel *parent);
Panel* SampleListPanelBoth_Create(Panel *parent);
Panel* SampleRadioButtons_Create(Panel *parent);
Panel* SampleCheckButtons_Create(Panel *parent);
Panel* SampleTabs_Create(Panel *parent);
Panel* SampleEditFields_Create(Panel *parent);
Panel* SampleSliders_Create(Panel *parent);
Panel* DefaultColors_Create(Panel *parent);

Panel* HTMLDemo_Create(Panel *parent);
Panel* HTMLDemo2_Create(Panel *parent);

Panel* MenuBarDemo_Create(Panel *parent);

class CControlCatalog: public Frame
{
public:
	CControlCatalog();
	~CControlCatalog();

	void OnClose();	

private:

	void OnTextChanged();

	ComboBox *m_pSelectControl;
	CUtlVector<Panel *> m_PanelList;
	Panel * m_pPrevPanel;
	Label *m_pCategoryLabel;

	DECLARE_PANELMAP();
};

#endif // CControlCatalog_H


