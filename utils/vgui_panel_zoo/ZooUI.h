//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#ifndef ZOOUI_H
#define ZOOUI_H
#ifdef _WIN32
#pragma once
#endif

#include <VGUI_Frame.h>

namespace vgui
{
	class MenuButton;
	class TextEntry;
	class PropertySheet;
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

class CZooUI: public Frame
{
public:
	CZooUI();
	~CZooUI();

	void OnClose();	
	void OnMinimize();
	void OnCommand(const char *command);

private:

	vgui::PropertySheet *m_pTabPanel;

	DECLARE_PANELMAP();
};

#endif // SURVEY_H





