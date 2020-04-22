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

#include <vgui_controls/TextEntry.h>


using namespace vgui;


class SampleEditFields: public DemoPage
{
	public:
		SampleEditFields(Panel *parent, const char *name);
		~SampleEditFields();

	
	private:
		TextEntry *m_pTextEntry;
};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
SampleEditFields::SampleEditFields(Panel *parent, const char *name) : DemoPage(parent, name)
{
	m_pTextEntry = new TextEntry (this, "ATextEntry");
	int wide, tall;
	m_pTextEntry->GetSize(wide, tall);
	m_pTextEntry->SetBounds(150, 200, 150, tall);
	m_pTextEntry->InsertString("with content");
	m_pTextEntry->SetEnabled(false);

	LoadControlSettings("Demo/SampleEditFields.res");

}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
SampleEditFields::~SampleEditFields()
{
}

Panel* SampleEditFields_Create(Panel *parent)
{
	return new SampleEditFields(parent, "Edit Fields");
}


