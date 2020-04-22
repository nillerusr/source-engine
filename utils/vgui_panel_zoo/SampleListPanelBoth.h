//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "DemoPage.h"

#include <VGUI/IVGui.h>
#include <vgui_controls/Controls.h>
#include <Keyvalues.h>
#include <vgui_controls/SectionedListPanel.h>


using namespace vgui;


class SampleListPanelBoth: public DemoPage
{
	public:
		SampleListPanelBoth(Panel *parent, const char *name);
		~SampleListPanelBoth();

		void onButtonClicked();
	
	private:
		SectionedListPanel *m_pSectionedListPanel;
		
};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
SampleListPanelBoth::SampleListPanelBoth(Panel *parent, const char *name) : DemoPage(parent, name)
{
	// Create a list panel.
	m_pSectionedListPanel = new SectionedListPanel(this, "AListPanel");

	// Add a new section
	m_pSectionedListPanel->addSection(0, "LIST ITEMS");
	m_pSectionedListPanel->addColumnToSection(0, "items", SectionedListPanel::COLUMN_TEXT);

	// Add items to the list
	KeyValues *data = new KeyValues ("items");
	data->SetString("items", "Many actions");
	m_pSectionedListPanel->addItem(0, 0, data);

	data->SetString("items", "Performed on");
	m_pSectionedListPanel->addItem(1, 0, data);

	data->SetString("items", "List items can");
	m_pSectionedListPanel->addItem(2, 0, data);

	data->SetString("items", "Only be accessed");
	m_pSectionedListPanel->addItem(3, 0, data);

	data->SetString("items", "Using the right-");
	m_pSectionedListPanel->addItem(4, 0, data);

	data->SetString("items", "Click menu");
	m_pSectionedListPanel->addItem(5, 0, data);

	// Add a new section
	m_pSectionedListPanel->addSection(1, "RIGHT CLICK");
	m_pSectionedListPanel->addColumnToSection(0, "items", SectionedListPanel::COLUMN_TEXT);

	// Add items to the list
	data->SetString("items", "Right-click the item");
	m_pSectionedListPanel->addItem(10, 1, data);

	data->SetString("items", "To access its");
	m_pSectionedListPanel->addItem(11, 1, data);

	data->SetString("items", "List of associated");
	m_pSectionedListPanel->addItem(12, 1, data);

	data->SetString("items", "Commands");
	m_pSectionedListPanel->addItem(13, 1, data);


	// Add a new section
	m_pSectionedListPanel->addSection(2, "RIGHT CLICK");
	m_pSectionedListPanel->addColumnToSection(2, "items", SectionedListPanel::COLUMN_TEXT);

	// Add items to the list
	data->SetString("items", "Right-click the item");
	m_pSectionedListPanel->addItem(10, 2, data);

	data->SetString("items", "To access its");
	m_pSectionedListPanel->addItem(11, 2, data);

	data->SetString("items", "List of associated");
	m_pSectionedListPanel->addItem(12, 2, data);

	data->SetString("items", "Commands");
	m_pSectionedListPanel->addItem(13, 2, data);


	// Set its position.
	m_pSectionedListPanel->setPos(90, 25);
	m_pSectionedListPanel->setSize(200, 150);
	
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
SampleListPanelBoth::~SampleListPanelBoth()
{
}




Panel* SampleListPanelBoth_Create(Panel *parent)
{
	return new SampleListPanelBoth(parent, "List Panel - categories");
}


