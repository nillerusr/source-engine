//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "DemoPage.h"

#include <VGUI_IVGui.h>
#include <VGUI_KeyValues.h>
#include <VGUI_Controls.h>
#include <VGUI_PropertySheet.h>

using namespace vgui;


class SampleTabs2: public DemoPage
{
	public:
		SampleTabs2(Panel *parent, const char *name);
		~SampleTabs2();
	
	private:
		PropertySheet *m_pPropertySheet;

};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
SampleTabs2::SampleTabs2(Panel *parent, const char *name) : DemoPage(parent, name)
{
	m_pPropertySheet = new PropertySheet(this, "Tabs");
	m_pPropertySheet->SetBounds(90,25, 344, 200);
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

	Panel *testPanel5 = new PropertyPage (this, "tab5");
	testPanel->SetBounds(90,50, 375,200);
	m_pPropertySheet->AddPage(testPanel5, "Brother");

	Panel *testPanel6 = new PropertyPage (this, "tab6");
	testPanel->SetBounds(90,50, 375,200);
	m_pPropertySheet->AddPage(testPanel6, "Brother2");

	Panel *testPanel7 = new PropertyPage (this, "tab7");
	testPanel->SetBounds(90,50, 375,200);
	m_pPropertySheet->AddPage(testPanel7, "Brother3");

	Panel *testPanel8 = new PropertyPage (this, "tab8");
	testPanel->SetBounds(90,50, 375,200);
	m_pPropertySheet->AddPage(testPanel8, "Brother4");

	m_pPropertySheet->SetScrolling(true);


}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
SampleTabs2::~SampleTabs2()
{
}



Panel* SampleTabs2_Create(Panel *parent)
{
	return new SampleTabs2(parent, "Scrolling Tabs");
}


