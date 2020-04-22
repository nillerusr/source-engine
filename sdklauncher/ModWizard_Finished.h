//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef MODWIZARD_FINISHED_H
#define MODWIZARD_FINISHED_H
#ifdef _WIN32
#pragma once
#endif


#include <vgui_controls/WizardSubPanel.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/CheckButton.h>


// --------------------------------------------------------------------------------------------------------------------- //
// CreateModWizard sub panel 1.
// This panel just tells them about the wizard and what it's going to do, and gives them a chance to cancel.
// --------------------------------------------------------------------------------------------------------------------- //

namespace vgui
{

	class CModWizardSubPanel_Finished : public WizardSubPanel
	{
	public:
		typedef WizardSubPanel BaseClass;

	public:
		CModWizardSubPanel_Finished( Panel *parent, const char *panelName );

		void GetReady( const char *pOutputDirName );
		
		virtual WizardSubPanel* GetNextSubPanel();
		virtual void OnDisplayAsNext();
		virtual void PerformLayout();
		virtual bool OnFinishButton();

	private:
		Label *m_pFinishedText;
		CheckButton *m_pOpenReadme;
		char m_OutputDirName[MAX_PATH];		// c:\mymod
	};

}


#endif // MODWIZARD_FINISHED_H
