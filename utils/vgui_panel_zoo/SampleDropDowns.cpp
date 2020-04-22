//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "DemoPage.h"

#include <vgui_controls/ComboBox.h>
#include <vgui_controls/Label.h>

using namespace vgui;


class SampleDropDowns: public DemoPage
{
	public:
		SampleDropDowns(Panel *parent, const char *name);
		~SampleDropDowns();
	
	private:
		ComboBox *m_pNormal;
		ComboBox *m_pNormalScroll;
		ComboBox *m_pNormal2;
		ComboBox *m_pNormal3;

		Label *m_pLabel1;
		Label *m_pLabel2;
		Label *m_pLabel3;
		Label *m_pLabel4;
		
};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
SampleDropDowns::SampleDropDowns(Panel *parent, const char *name) : DemoPage(parent, name)
{
	m_pNormal = new ComboBox(this, "Pills", 6, false);
	m_pNormal->SetPos(90, 25);
	m_pNormal->SetWide(80);
	m_pNormal->AddItem("Red Pill", NULL );
	m_pNormal->AddItem("Blue Pill", NULL );
	m_pNormal->AddItem("ReallyLongName Pill", NULL );
	m_pNormal->ActivateItem(0);
	m_pLabel1 = new Label (this, "WhichLabel", "Which");
	m_pLabel1->SizeToContents();
	m_pLabel1->SetPos(43, 25);

	m_pNormalScroll = new ComboBox(this, "Moves", 6, false);
	m_pNormalScroll->SetPos(243, 25);
	m_pNormalScroll->SetWide(130);
	m_pNormalScroll->AddItem("Freezes", NULL );
	m_pNormalScroll->AddItem("Kipup", NULL );
	m_pNormalScroll->AddItem("Donkey", NULL );
	m_pNormalScroll->AddItem("Sidewinder", NULL );
	m_pNormalScroll->AddItem("Handspin", NULL );
	m_pNormalScroll->AddItem("Coffee Grinder", NULL );
	m_pNormalScroll->AddItem("Headspin", NULL );
	m_pNormalScroll->AddItem("The Worm", NULL );
	m_pNormalScroll->ActivateItem(6);
	m_pLabel2 = new Label (this, "MoveLabel", "Move");
	m_pLabel2->SizeToContents();
	m_pLabel2->SetPos(200, 25);

	m_pNormal2 = new ComboBox(this, "Pills2", 6, false);
	m_pNormal2->SetPos(90, 125);
	m_pNormal2->SetWide(80);
	m_pNormal2->AddItem("one", NULL );
	m_pNormal2->AddItem("two", NULL );
	m_pNormal2->ActivateItem(1);
	m_pLabel3 = new Label (this, "ALabel", "Label on top");
	m_pLabel3->SizeToContents();
	m_pLabel3->SetPos(90, 100);

	m_pNormal3 = new ComboBox(this, "Pills", 6, false);
	m_pNormal3->SetPos(90, 200);
	m_pNormal3->SetWide(80);
	m_pNormal3->AddItem("one", NULL );
	m_pNormal3->AddItem("two", NULL );
	m_pNormal3->ActivateItem(0);
	m_pNormal3->SetEnabled(false);
	m_pLabel4 = new Label (this, "DLabel", "Disabled");
	m_pLabel4->SetPos(90, 175);
	m_pLabel4->SizeToContents();
	m_pLabel4->SetEnabled(false);


}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
SampleDropDowns::~SampleDropDowns()
{
}



Panel* SampleDropDowns_Create(Panel *parent)
{
	return new SampleDropDowns(parent, "Drop-downs");
}


