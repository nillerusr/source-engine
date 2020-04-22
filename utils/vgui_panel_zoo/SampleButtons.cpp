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
#include <vgui_controls/Button.h>
#include <vgui_controls/MenuButton.h>

using namespace vgui;


class SampleButtons: public DemoPage
{
	public:
		SampleButtons(Panel *parent, const char *name);
		~SampleButtons();

		void onButtonClicked();
	
	private:
		Button *m_pEnabledButton;
		Button *m_pDepressedButton;
		Button *m_pDefaultButton;
		Button *m_pDisabledButton;
		Button *m_pDisActiveButton;
		Button *m_pMoreInfoButton;
		Button *m_pExpandButton;
		MenuButton *m_pMenuButton;
		
		DECLARE_PANELMAP();
};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
SampleButtons::SampleButtons(Panel *parent, const char *name) : DemoPage(parent, name)
{
	// Create a button.
	m_pEnabledButton = new Button(this, "EnabledButton", "Enabled");
	// Set its position.
	m_pEnabledButton->SetPos(90, 15);
	// Set its size
	m_pEnabledButton->SetSize(90, 25);

	// Install a command that will be executed when the button is pressed
	// Here we use a KeyValues command, this is mapped using the Message map
	// below to a function.
	m_pEnabledButton->SetCommand(new KeyValues ("ButtonClicked"));

	m_pEnabledButton->SetEnabled(true);


	m_pDepressedButton = new Button(this, "ActiveButton", "Active");
	m_pDepressedButton->SetPos(192, 15);
	m_pDepressedButton->SetSize(90, 25);
	// Set it pressed down
	m_pDepressedButton->ForceDepressed(true);

	m_pDefaultButton = new Button(this, "DefaultButton", "Default");
	// Set this button to be the default
	m_pDefaultButton->SetAsDefaultButton(true);
	m_pDefaultButton->SetPos(292, 15);
	m_pDefaultButton->SetSize(90, 25);


	m_pDisabledButton = new Button(this, "DisabledButton", "Disabled");
	m_pDisabledButton->SetPos(90, 55);
	m_pDisabledButton->SetSize(90, 25);
	// Set it disabled
	m_pDisabledButton->SetEnabled(false);

	m_pDisActiveButton = new Button(this, "DisActiveButton", "Active");
	m_pDisActiveButton->SetPos(192, 55);
	m_pDisActiveButton->SetSize(90, 25);
	// Set it pressed down
	m_pDisActiveButton->ForceDepressed(true);
	// Set it disabled
	m_pDisActiveButton->SetEnabled(false);
	


	m_pMoreInfoButton = new Button(this, "MoreInfoButton", "More info required...");
	m_pMoreInfoButton->SetPos(90, 105);
	m_pMoreInfoButton->SetSize(160, 25);

	m_pExpandButton = new Button(this, "ExpandButton", "Expand this window >>");
	m_pExpandButton->SetPos(90, 155);
	m_pExpandButton->SetSize(160, 25);

	m_pMenuButton = new MenuButton(this, "MenuButton", "Open a menu");
	m_pMenuButton->SetPos(90, 205);
	m_pMenuButton->SetSize(125, 25);

}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
SampleButtons::~SampleButtons()
{
}

//-----------------------------------------------------------------------------
// Purpose:	 Respond to a message based action signal
//-----------------------------------------------------------------------------
void SampleButtons::onButtonClicked()
{
	ivgui()->DPrintf("Button was clicked.\n");
}



MessageMapItem_t SampleButtons::m_MessageMap[] =
{
	MAP_MESSAGE( SampleButtons, "ButtonClicked", onButtonClicked ),   
};

IMPLEMENT_PANELMAP(SampleButtons, DemoPage);



Panel* SampleButtons_Create(Panel *parent)
{
	return new SampleButtons(parent, "Buttons");
}


