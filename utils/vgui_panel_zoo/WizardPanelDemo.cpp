//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "DemoPage.h"

#include <VGUI/IVGui.h>
//#include <vgui_controls/Controls.h>

#include <vgui_controls/WizardPanel.h>
#include <vgui_controls/WizardSubPanel.h>
#include <vgui_controls/PHandle.h>

#include <vgui_controls/RadioButton.h>
#include <vgui_controls/TextEntry.h>
#include <vgui/ISurface.h>

using namespace vgui;

//-----------------------------------------------------------------------------
// This is a demo of a Wizard.
// A wizard is an interactive utility within an application that guides the user through 
// each step of a task.
//
// Wizards typically display a sequence of steps, the user fills in information
// or makes selections and then clicks a "next" button to go to the next panel
// in the sequence. After all panels have been completed, the user clicks "finish"
// and the wizard exits.
//
// In VGUI, the Wizard class is the panel that holds the wizard navigation buttons
// to move to the previous or next panel, and the finish and cancel buttons to 
// exit. It also creates the panels that display when the buttons are pressed, called
// WizardSubPanels. These panels have thier own layout and functions that determine
// when to enable/disable the Wizard's navigation buttons.
//
// In this demo we have a Wizard class, called CWonderfulWizard, that contains
// two WizardSubPanel classes, called CSomeSelections and CMoreSelections.
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// CSomeSelections: First sub panel of the Wonderful wizard
//			Provide some user options that we load from a resource file.
//-----------------------------------------------------------------------------
class CSomeSelections : public WizardSubPanel
{
public:
	CSomeSelections(Panel *parent, const char *panelName);
	~CSomeSelections(){};
	
	virtual WizardSubPanel *GetNextSubPanel();
	virtual void OnDisplayAsPrev();
	// Called when the wizard 'next' button is pressed.
	// Return true if the wizard should advance.
	virtual bool OnNextButton()	{ return true;}
	virtual void PerformLayout();
	
private:
	TextEntry *m_pFirstNameEdit;
	TextEntry *m_pLastNameEdit;
	TextEntry *m_pUserNameEdit;
	TextEntry *m_pEmailEdit;
	};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CSomeSelections::CSomeSelections(Panel *parent, const char *panelName) : 
WizardSubPanel(parent, panelName)
{
	// create the controls
	m_pUserNameEdit = new TextEntry(this, "UserNameEdit");
	m_pUserNameEdit->SetPos(100,100);
	m_pFirstNameEdit = new TextEntry(this, "FirstNameEdit");
	m_pLastNameEdit = new TextEntry(this, "LastNameEdit");
	m_pEmailEdit = new TextEntry(this, "EmailEdit");
		
	// The layout of the controls is loaded from a resource file.
	LoadControlSettings("Demo/WizardPanelDemo.res");
}


//-----------------------------------------------------------------------------
// Purpose: Return a pointer to the next subpanel that should be displayed
// Output : WizardSubPanel *
//-----------------------------------------------------------------------------
WizardSubPanel *CSomeSelections::GetNextSubPanel()
{
	// The next panel to be displayed is called 'CMoreSelections'
	return dynamic_cast<WizardSubPanel *>(GetWizardPanel()->FindChildByName("CMoreSelections"));
}

//-----------------------------------------------------------------------------
// Purpose: Execute this code when a panel has had the 'prev' button pressed
// and the panel to be displayed is this one.
// Input  :  
//-----------------------------------------------------------------------------
void CSomeSelections::OnDisplayAsPrev()
{
	// Enable the 'next' button
	GetWizardPanel()->SetNextButtonEnabled(true);
	// Buttons are disabled by default, so the prev button will be disabled,
	// which is correct since there are no panels before this one.
}

//-----------------------------------------------------------------------------
// Purpose: Layout the window.
//-----------------------------------------------------------------------------
void CSomeSelections::PerformLayout()
{
	// Set the title of the Wizard.
	GetWizardPanel()->SetTitle("Some Selections", false);
	// Make sure the 'finish' button is disabled, since we are not on the last panel.
	GetWizardPanel()->SetFinishButtonEnabled(false);
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// CMoreSelections: Second and last sub panel of the Wonderful wizard
//			Just one radio button in here. If the button is selected
//          The 'finish' button becomes enabled.
//-----------------------------------------------------------------------------
class CMoreSelections : public WizardSubPanel
{
public:
	CMoreSelections(Panel *parent, const char *panelName);
	~CMoreSelections(){};
	
	virtual WizardSubPanel *GetNextSubPanel();
	virtual void OnDisplayAsNext();
	virtual bool OnPrevButton() { return true;}
	virtual void PerformLayout();
	void OnRadioButtonChecked(Panel *panel);

	DECLARE_PANELMAP();
		
private:
	RadioButton *m_pDoneRadio;
};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CMoreSelections::CMoreSelections(Panel *parent, const char *panelName) : 
WizardSubPanel(parent, panelName)
{
	// create the controls
	// a radio button
	m_pDoneRadio = new RadioButton(this, "DoneRadio", "Are you done?");
	m_pDoneRadio->SizeToContents();
	m_pDoneRadio->SetPos(100,100);
}

//-----------------------------------------------------------------------------
// Purpose: The wizard tried to get the subpanel after this one.
//  There is no panel to be displayed after this one. So return NULL
//-----------------------------------------------------------------------------
WizardSubPanel *CMoreSelections::GetNextSubPanel()
{
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Called when the subpanel is displayed
// All controls & data should be reinitialized at this time
//-----------------------------------------------------------------------------
void CMoreSelections::OnDisplayAsNext()
{
	// There is no next panel so disable this button.
	GetWizardPanel()->SetNextButtonEnabled(false);
	// We want the finish button disabled until the radio button is set.
	GetWizardPanel()->SetFinishButtonEnabled(false);
}

//-----------------------------------------------------------------------------
// Purpose: Layout the window and enable/disable buttons as appropriate.
//-----------------------------------------------------------------------------
void CMoreSelections::PerformLayout()
{
	// Set the title of the Wizard.
	GetWizardPanel()->SetTitle("All finished?", false);

	// Check if the radio button is selected.
	if ( m_pDoneRadio->IsSelected())
	{
		// If it is, we will enable the 'finish' button.
		GetWizardPanel()->SetFinishButtonEnabled(true);

	}
	GetWizardPanel()->SetNextButtonEnabled(false);	
}

//-----------------------------------------------------------------------------
// Purpose: Upon checking the radio button, enable the 'finish' button.
//-----------------------------------------------------------------------------
void CMoreSelections::OnRadioButtonChecked(Panel *panel)
{
	if ( m_pDoneRadio->IsSelected())
	{
		GetWizardPanel()->SetFinishButtonEnabled(true);

	}
}

//-----------------------------------------------------------------------------
// Purpose: Message map
//-----------------------------------------------------------------------------
MessageMapItem_t CMoreSelections::m_MessageMap[] =
{
	MAP_MESSAGE_PTR( CMoreSelections, "RadioButtonChecked", OnRadioButtonChecked, "panel" ),	// custom message
};
IMPLEMENT_PANELMAP(CMoreSelections, Panel);



//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// Purpose: A wizard panel containing two 
// wizard sub panels
//-----------------------------------------------------------------------------
class CWonderfulWizard : public WizardPanel
{
public:
	CWonderfulWizard();
	~CWonderfulWizard(){};
	
	void Run(void);
	void Open();
	
private:
};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWonderfulWizard::CWonderfulWizard() : WizardPanel(NULL, "WonderfulWizard")
{
	// The size of the Wizard.
	//SetBounds(0, 0, 480, 360);
	
	// The first panel to be displayed.
	WizardSubPanel *subPanel = new CSomeSelections(this, "CSomeSelections");
	subPanel->SetVisible(false);
	
	// The second panel to be displayed.
	subPanel = new CMoreSelections(this, "CMoreSelections");
	subPanel->SetVisible(false);
}

//-----------------------------------------------------------------------------
// Purpose: Start the wizard, starting with the startPanel
//-----------------------------------------------------------------------------
void CWonderfulWizard::Run( void )
{
	SetVisible(true);

	// Call run, with the name of the first panel to be displayed.
	WizardPanel::Run(dynamic_cast<WizardSubPanel *>(FindChildByName("CSomeSelections")));
	
	SetTitle("A Wizard Panel ", true);
}	 

//-----------------------------------------------------------------------------
// Purpose: Display the wizard.
//-----------------------------------------------------------------------------
void CWonderfulWizard::Open()
{
	RequestFocus();
	MoveToFront();
	SetVisible(true);
	surface()->SetMinimized(this->GetVPanel(), false);
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// Purpose: A demonstration of a wizard panel containing two 
// wizard sub panels
//-----------------------------------------------------------------------------
class WizardPanelDemo: public DemoPage
{
public:
	WizardPanelDemo(Panel *parent, const char *name);
	~WizardPanelDemo(){};
	
	void SetVisible(bool status);
	
private:
	// We use a handle because the window could be destroyed if someone
	// closed the wizard. 
	DHANDLE<CWonderfulWizard> m_hWizardPanel;
	
};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
WizardPanelDemo::WizardPanelDemo(Panel *parent, const char *name) : DemoPage(parent, name)
{
}

//-----------------------------------------------------------------------------
// Purpose: When we make this this demo page visible we make the wizard visible.
//-----------------------------------------------------------------------------
void WizardPanelDemo::SetVisible(bool status)
{
	if (status)
	{
		// Pop up the dialog
		if (m_hWizardPanel.Get())
		{
			m_hWizardPanel->Open();
		}
		else
		{
			CWonderfulWizard *pWizardPanel = new CWonderfulWizard();
			pWizardPanel->SetPos(100, 100);
			pWizardPanel->SetSize(480, 360);

			surface()->CreatePopup(pWizardPanel->GetVPanel(), false);
			m_hWizardPanel = pWizardPanel;
			m_hWizardPanel->Run();
		}
	}
	else
	{
		if (m_hWizardPanel.Get())
		{		
			m_hWizardPanel->SetVisible(false);
		}
	}

	DemoPage::SetVisible(status);	
}



Panel* WizardPanelDemo_Create(Panel *parent)
{
	return new WizardPanelDemo(parent, "WizardPanelDemo");
}


