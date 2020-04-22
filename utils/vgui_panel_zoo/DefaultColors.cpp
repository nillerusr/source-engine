//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "DemoPage.h"

#include <VGUI/IVGui.h>
#include <Keyvalues.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/Panel.h>
#include <vgui/IScheme.h>
#include <Color.h>
#include <stdio.h>

using namespace vgui;

Color colors[] = {
	Color(0,0,0),
	Color(30, 30, 30),
	Color(45, 49, 40),
	Color(62, 70, 55),
	Color(76, 88, 68),
	Color(100, 120, 100),
	Color(136, 145, 128),
	Color(160, 170, 149),
	Color(216, 222, 211),
	Color(255, 255, 255),
	Color(149, 136, 49),
	Color(196, 181, 80)
};
	


class DefaultColors: public DemoPage
{
	public:
		DefaultColors(Panel *parent, const char *name);
		~DefaultColors();

		void Paint();
	private:
		Panel *m_pColorPanel[12];

};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
DefaultColors::DefaultColors(Panel *parent, const char *name) : DemoPage(parent, name)
{
	char buf[32];
	for (int i=1; i<=12; i++)
	{
		sprintf(buf, "ColorPanel%d", i);
		m_pColorPanel[i-1] = new Panel (this, buf);
	}
	LoadControlSettings("Demo/DefaultColors.res");

}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
DefaultColors::~DefaultColors()
{
}


//-----------------------------------------------------------------------------
// Purpose: Apply settings
//-----------------------------------------------------------------------------
void DefaultColors::Paint()
{
	for (int i=0; i<12; i++)
	{
		IScheme *pScheme = scheme()->GetIScheme( m_pColorPanel[i]->GetScheme() );
		m_pColorPanel[i]->SetBorder(pScheme->GetBorder("DefaultBorder"));
		m_pColorPanel[i]->SetBgColor(colors[i]);
	}

	Panel::Paint();
}


Panel* DefaultColors_Create(Panel *parent)
{
	return new DefaultColors(parent, "Default Colors");
}


