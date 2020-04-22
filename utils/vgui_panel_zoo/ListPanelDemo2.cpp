//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "DemoPage.h"

#include <VGUI_IVGui.h>
#include <VGUI_Controls.h>
#include <VGUI_KeyValues.h>
#include <VGUI_ListPanel.h>

using namespace vgui;

class ListPanelDemo2: public DemoPage
{
	public:
		ListPanelDemo2(Panel *parent, const char *name);
		~ListPanelDemo2();

		void onButtonClicked();
	
	private:
		ListPanel *m_pListPanel;
		
};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
ListPanelDemo2::ListPanelDemo2(Panel *parent, const char *name) : DemoPage(parent, name)
{
	// Create a list panel.
	m_pListPanel = new ListPanel(this, "AListPanel");

	// Add a column header
	m_pListPanel->addColumnHeader(0, "Muppet", "Muppet", 150, true, 20, 200, true);  

	// Add another column header
	m_pListPanel->addColumnHeader(1, "Description", "Description", 150, true, 20, 200, true);  

	// Set its position.
	m_pListPanel->setPos(90, 25);
	m_pListPanel->setSize(400, 250);
	
	// Add rows of data to the table
	KeyValues *data = new KeyValues ("item");
	data->SetString("Muppet", "Kermit");
	data->SetString("Description", "The frog");
	m_pListPanel->addItem(data); 

	data->SetString("Muppet", "Miss Piggy");
	data->SetString("Description", "The diva");
	m_pListPanel->addItem(data);

	data->SetString("Muppet", "Scooter");
	data->SetString("Description", "The man");
	m_pListPanel->addItem(data);

	data->SetString("Muppet", "Statler");
	data->SetString("Description", "Old guy");
	m_pListPanel->addItem(data);

	data->SetString("Muppet", "Waldorf");
	data->SetString("Description", "Old guy");
	m_pListPanel->addItem(data);

	data->SetString("Muppet", "Gonzo");
	data->SetString("Description", "The unknown");
	m_pListPanel->addItem(data);

	data->SetString("Muppet", "Scooter");
	data->SetString("Description", "The man");
	m_pListPanel->addItem(data);

	data->SetString("Muppet", "Fozzie");
	data->SetString("Description", "The bear");
	m_pListPanel->addItem(data);

	data->SetString("Muppet", "Betty Lou");
	data->SetString("Description", "[none]");
	m_pListPanel->addItem(data);
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
ListPanelDemo2::~ListPanelDemo2()
{
}


Panel* ListPanelDemo2_Create(Panel *parent)
{
	return new ListPanelDemo2(parent, "ListPanelDemo2");
}


