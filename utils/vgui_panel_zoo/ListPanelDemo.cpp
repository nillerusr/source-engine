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
#include <vgui_controls/ListPanel.h>


using namespace vgui;


class ListPanelDemo: public DemoPage
{
	public:
		ListPanelDemo(Panel *parent, const char *name);
		~ListPanelDemo();

		void onButtonClicked();
	
	private:
		ListPanel *m_pListPanel;
		
};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
ListPanelDemo::ListPanelDemo(Panel *parent, const char *name) : DemoPage(parent, name)
{
	// Create a list panel.
	m_pListPanel = new ListPanel(this, "AListPanel");

	// Add a column header
	m_pListPanel->AddColumnHeader(0, "Muppet", "Muppet", 150, 20, 200);  

	// Add another column header
	m_pListPanel->AddColumnHeader(1, "Description", "Description", 150, 20, 200 );  

	// Set its position.
	m_pListPanel->SetPos(90, 25);
	m_pListPanel->SetSize(400, 250);
	
	// Add rows of data to the table
	KeyValues *data = new KeyValues ("item");
	data->SetString("Muppet", "Kermit");
	data->SetString("Description", "The frog");
	m_pListPanel->AddItem(data, 0, false, false); 

	data->SetString("Muppet", "Miss Piggy");
	data->SetString("Description", "The diva");
	m_pListPanel->AddItem(data, 0, false, false);

	data->SetString("Muppet", "Scooter");
	data->SetString("Description", "The man");
	m_pListPanel->AddItem(data, 0, false, false);

	data->SetString("Muppet", "Statler");
	data->SetString("Description", "Old guy");
	m_pListPanel->AddItem(data, 0, false, false);

	data->SetString("Muppet", "Waldorf");
	data->SetString("Description", "Old guy");
	m_pListPanel->AddItem(data, 0, false, false);

	data->SetString("Muppet", "Gonzo");
	data->SetString("Description", "The unknown");
	m_pListPanel->AddItem(data, 0, false, false);

	data->SetString("Muppet", "Scooter");
	data->SetString("Description", "The man");
	m_pListPanel->AddItem(data, 0, false, false);

	data->SetString("Muppet", "Fozzie");
	data->SetString("Description", "The bear");
	m_pListPanel->AddItem(data, 0, false, false);

	data->SetString("Muppet", "Betty Lou");
	data->SetString("Description", "[none]");
	m_pListPanel->AddItem(data, 0, false, false);
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
ListPanelDemo::~ListPanelDemo()
{
}




Panel* ListPanelDemo_Create(Panel *parent)
{
	return new ListPanelDemo(parent, "ListPanelDemo");
}


