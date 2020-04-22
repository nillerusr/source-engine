//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "DemoPage.h"

#include "vgui/IVGui.h"
#include "vgui_controls/Controls.h"
#include "tier1/KeyValues.h"
#include "vgui_controls/SectionedListPanel.h"


using namespace vgui;


class SampleListPanelCats: public DemoPage
{
	public:
		SampleListPanelCats(Panel *parent, const char *name);
		~SampleListPanelCats();

		void OnButtonClicked();
	
	private:
		SectionedListPanel *m_pSectionedListPanel;
		
};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
SampleListPanelCats::SampleListPanelCats(Panel *parent, const char *name) : DemoPage(parent, name)
{
	// Create a list panel.
	m_pSectionedListPanel = new SectionedListPanel(this, "AListPanel");

	// Add a new section
	m_pSectionedListPanel->AddSection(0, "LIST ITEMS");
	m_pSectionedListPanel->AddColumnToSection(0, "items", "items", 0, 150);

	// Add items to the list
	KeyValues *data = new KeyValues ("items");
	data->SetString("items", "Many actions");
	m_pSectionedListPanel->AddItem(0, data);

	data->SetString("items", "Performed on");
	m_pSectionedListPanel->AddItem(0, data);

	data->SetString("items", "List items can");
	m_pSectionedListPanel->AddItem(0, data);

	data->SetString("items", "Only be accessed");
	m_pSectionedListPanel->AddItem(0, data);

	data->SetString("items", "Using the right-");
	m_pSectionedListPanel->AddItem(0, data);

	data->SetString("items", "Click menu");
	m_pSectionedListPanel->AddItem(0, data);


	// Add a new section
	m_pSectionedListPanel->AddSection(1, "RIGHT CLICK");
	m_pSectionedListPanel->AddColumnToSection(1, "items", "items", 0, 150);

	// Add items to the list
	data->SetString("items", "Right-click the item");
	m_pSectionedListPanel->AddItem(1, data);

	data->SetString("items", "To access its");
	m_pSectionedListPanel->AddItem(1, data);

	data->SetString("items", "List of associated");
	m_pSectionedListPanel->AddItem(1, data);

	data->SetString("items", "Commands");
	m_pSectionedListPanel->AddItem(1, data);


	// Set its position.
	m_pSectionedListPanel->SetPos(90, 25);
	m_pSectionedListPanel->SetSize(200, 150);
	
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
SampleListPanelCats::~SampleListPanelCats()
{
}




Panel* SampleListPanelCats_Create(Panel *parent)
{
	return new SampleListPanelCats(parent, "List Panel - categories");
}


