//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef MODWIZARD_INTRO_H
#define MODWIZARD_INTRO_H
#ifdef _WIN32
#pragma once
#endif


#include <vgui_controls/WizardSubPanel.h>
#include <vgui_controls/RadioButton.h>

#include "configs.h"


// --------------------------------------------------------------------------------------------------------------------- //
// CreateModWizard sub panel 1.
// This panel just tells them about the wizard and what it's going to do, and gives them a chance to cancel.
// --------------------------------------------------------------------------------------------------------------------- //

namespace vgui
{

	class CModWizardSubPanel_Intro : public WizardSubPanel
	{
	public:
		typedef WizardSubPanel BaseClass;

	public:
		CModWizardSubPanel_Intro( Panel *parent, const char *panelName );
		
		virtual WizardSubPanel* GetNextSubPanel();
		virtual void OnDisplayAsNext();
		virtual void PerformLayout();
		virtual bool OnNextButton();

		ModType_t GetModType();

		RadioButton *m_pModHL2Button;
		RadioButton *m_pModHL2MPButton;
		RadioButton *m_pModFromScratchButton;
		RadioButton *m_pSourceCodeOnlyButton;
	};

}

	
#endif // MODWIZARD_INTRO_H
