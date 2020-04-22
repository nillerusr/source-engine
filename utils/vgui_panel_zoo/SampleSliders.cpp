//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "DemoPage.h"

#include <VGUI/IVGui.h>
#include <Keyvalues.h>
#include <vgui_controls/Slider.h>
#include <vgui_controls/Label.h>


using namespace vgui;


class SampleSliders: public DemoPage
{
	public:
		SampleSliders(Panel *parent, const char *name);
		~SampleSliders();
	
	private:
		Slider *m_pSlider;
		Slider *m_pSlider2;
		Slider *m_pSlider3;
		Label *m_pSliderLabel;
		Label *m_pSliderLabel2;
		Label *m_pSliderLabel3;

};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
SampleSliders::SampleSliders(Panel *parent, const char *name) : DemoPage(parent, name)
{
	m_pSliderLabel = new Label (this, "ASliderLabel", "Label on top");
	m_pSliderLabel->SizeToContents();
	m_pSliderLabel->SetPos(90, 25);
	m_pSliderLabel->SetZPos(1);

	m_pSlider = new Slider (this, "ALabeledSlider");
	m_pSlider->SetPos(90,50);
	m_pSlider->SetSize(165, 30);
	m_pSlider->SetTickCaptions("0", "test");
	m_pSlider->SetNumTicks(7);
	m_pSlider->SetZPos(0);

	m_pSliderLabel2 = new Label (this, "ASliderLabel", "Or, without numbers");
	m_pSliderLabel2->SizeToContents();
	m_pSliderLabel2->SetPos(290, 25);
	m_pSliderLabel2->SetZPos(1);

	m_pSlider2 = new Slider (this, "ALabeledSlider");
	m_pSlider2->SetPos(290,50);
	m_pSlider2->SetSize(165, 50);
	m_pSlider2->SetNumTicks(7);
	m_pSlider2->SetZPos(0);


	m_pSliderLabel3 = new Label (this, "ASliderLabel", "Label at left");
	m_pSliderLabel3->SizeToContents();
	m_pSliderLabel3->SetPos(90, 125);

	m_pSlider3 = new Slider (this, "ALabeledSlider");
	int wide, tall;
	m_pSliderLabel->GetSize(wide, tall);
	m_pSlider3->SetPos(90+wide,125);
	m_pSlider3->SetSize(150, 50);
	m_pSlider3->SetNumTicks(5);

}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
SampleSliders::~SampleSliders()
{
}




Panel* SampleSliders_Create(Panel *parent)
{
	return new SampleSliders(parent, "Slider Bars");
}


