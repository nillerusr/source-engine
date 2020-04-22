//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "DemoPage.h"

#include <VGUI/IVGui.h>
#include <Keyvalues.h>
#include <vgui_controls/PropertySheet.h>

using namespace vgui;


class SampleTabs: public DemoPage
{
	public:
		SampleTabs(Panel *parent, const char *name);
		~SampleTabs();
	
	private:
		PropertySheet *m_pPropertySheet;

};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
SampleTabs::SampleTabs(Panel *parent, const char *name) : DemoPage(parent, name)
{
	m_pPropertySheet = new PropertySheet(this, "Tabs");
	m_pPropertySheet->SetBounds(90,25, 375, 200);
	m_pPropertySheet->SetTabWidth(75);

	Panel *testPanel = new PropertyPage (this, "tab1");
	testPanel->SetBounds(90,50, 375, 200);
	m_pPropertySheet->AddPage(testPanel, "Keyboard");

	Panel *testPanel2 = new PropertyPage (this, "tab2");
	testPanel->SetBounds(90,50,375,200);
	m_pPropertySheet->AddPage(testPanel2, "Mouse");

	Panel *testPanel3 = new PropertyPage (this, "tab3");
	testPanel->SetBounds(90,50,375,200);
	m_pPropertySheet->AddPage(testPanel3, "Audio");

	Panel *testPanel4 = new PropertyPage (this, "tab4");
	testPanel->SetBounds(90,50, 375,200);
	m_pPropertySheet->AddPage(testPanel4, "Video");

}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
SampleTabs::~SampleTabs()
{
}




Panel* SampleTabs_Create(Panel *parent)
{
	return new SampleTabs(parent, "Tabs");
}


