//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "ZooUI.h"
#include "stdio.h"

#include <VGUI_ISurface.h>
#include <VGUI_Controls.h>
#include <VGUI_KeyValues.h>

#include <VGUI_PropertySheet.h>


#include <VGUI_IVGui.h> // for dprinf statements


using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CZooUI::CZooUI(): Frame(NULL, "PanelZoo")
{
	SetTitle("Panel Zoo", true);
	// calculate defaults
	int x, y, wide, tall;
	vgui::surface()->GetScreenSize(wide, tall);
	
	int dwide, dtall;
	dwide = 1400;
	dtall = 500;
	x = (int)((wide - dwide) * 0.5);
	y = (int)((tall - dtall) * 0.5);
	SetBounds (x, y, dwide, dtall);

	SetVisible(true);
	vgui::surface()->CreatePopup(GetVPanel(), false);
	//loadControlSettings("PanelZoo.res");

	// property sheet
	m_pTabPanel = new PropertySheet(this, "ZooTabs");
	m_pTabPanel->SetBounds(0,50, 1400, 450);
	m_pTabPanel->SetTabWidth(50);

	m_pTabPanel->AddPage(ImageDemo_Create(this), "ImageDemo");
	m_pTabPanel->AddPage(ImagePanelDemo_Create(this), "ImagePanelDemo");
	m_pTabPanel->AddPage(TextImageDemo_Create(this), "TextImageDemo");


	m_pTabPanel->AddPage(LabelDemo_Create(this), "LabelDemo");
	m_pTabPanel->AddPage(Label2Demo_Create(this), "Label2Demo");


	m_pTabPanel->AddPage(TextEntryDemo_Create(this), "TextEntryDemo");
	m_pTabPanel->AddPage(TextEntryDemo2_Create(this), "TextEntryDemo2");
	m_pTabPanel->AddPage(TextEntryDemo3_Create(this), "TextEntryDemo3");
	m_pTabPanel->AddPage(TextEntryDemo4_Create(this), "TextEntryDemo4");

	m_pTabPanel->AddPage(ButtonDemo_Create(this), "ButtonDemo");
	m_pTabPanel->AddPage(ButtonDemo2_Create(this), "ButtonDemo2");

	m_pTabPanel->AddPage(CheckButtonDemo_Create(this), "CheckButtonDemo");
	m_pTabPanel->AddPage(ToggleButtonDemo_Create(this), "ToggleButtonDemo");
	m_pTabPanel->AddPage(RadioButtonDemo_Create(this), "RadioButtonDemo");


	m_pTabPanel->AddPage(MenuDemo_Create(this), "MenuDemo");
	m_pTabPanel->AddPage(MenuDemo2_Create(this), "MenuDemo2");
	m_pTabPanel->AddPage(CascadingMenuDemo_Create(this), "CascadingMenuDemo");

	m_pTabPanel->AddPage(MessageBoxDemo_Create(this), "MessageBoxDemo");
	m_pTabPanel->AddPage(QueryBoxDemo_Create(this), "QueryBoxDemo");


	m_pTabPanel->AddPage(ComboBoxDemo_Create(this), "ComboBoxDemo");
	m_pTabPanel->AddPage(ComboBox2Demo_Create(this), "ComboBox2Demo");


	m_pTabPanel->AddPage(FrameDemo_Create(this), "FrameDemo");

	m_pTabPanel->AddPage(ProgressBarDemo_Create(this), "ProgressBarDemo");
	m_pTabPanel->AddPage(ScrollBarDemo_Create(this), "ScrollBarDemo");
	m_pTabPanel->AddPage(ScrollBar2Demo_Create(this), "ScrollBar2Demo");

	m_pTabPanel->AddPage(EditablePanelDemo_Create(this), "EditablePanelDemo");
	m_pTabPanel->AddPage(EditablePanel2Demo_Create(this), "EditablePanel2Demo");

}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CZooUI::~CZooUI()
{ 
}

void CZooUI::OnCommand(const char *command)
{
	if (!stricmp(command, "Close"))
	{
		OnClose();
	}
}
 
//-----------------------------------------------------------------------------
// Purpose: Handles closing of the dialog - shuts down the whole app
//-----------------------------------------------------------------------------
void CZooUI::OnClose()
{
	Frame::OnClose();

	// stop vgui running
	vgui::ivgui()->Stop();
}

//-----------------------------------------------------------------------------
// Purpose: Handles closing of the dialog - shuts down the whole app
//-----------------------------------------------------------------------------
void CZooUI::OnMinimize()
{
	Frame::OnMinimize();
}

//-----------------------------------------------------------------------------
// Purpose: Message map
//-----------------------------------------------------------------------------
MessageMapItem_t CZooUI::m_MessageMap[] =
{
	MAP_MESSAGE( CZooUI, "Close", OnClose ),
};

IMPLEMENT_PANELMAP(CZooUI, BaseClass);


