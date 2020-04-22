//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef MODWIZARD_TEMPLATEOPTIONS_H
#define MODWIZARD_TEMPLATEOPTIONS_H
#ifdef _WIN32
#pragma once
#endif


#include <vgui_controls/WizardSubPanel.h>
#include <vgui_controls/TextEntry.h>
#include <vgui_controls/Label.h>
#include "configs.h"

enum numOptions_e
{
	TPOPTION_TEAMS,
	TPOPTION_CLASSES,
	TPOPTION_STAMINA,
	TPOPTION_SPRINTING,
	TPOPTION_PRONE,
	TPOPTION_SHOOTSPRINTING,
	TPOPTION_SHOOTLADDERS,
	TPOPTION_SHOOTJUMPING,
	TPOPTIONS_TOTAL,
};

// --------------------------------------------------------------------------------------------------------------------- //
// CreateModWizard sub panel 2b (only when scratch / template sdk is chosen!)
// This panel asks for options 
// --------------------------------------------------------------------------------------------------------------------- //

namespace vgui
{

	class CModWizardSubPanel_TemplateOptions : public WizardSubPanel
	{
	public:
		DECLARE_CLASS_SIMPLE( CModWizardSubPanel_TemplateOptions, vgui::WizardSubPanel );

	public:
		CModWizardSubPanel_TemplateOptions( Panel *parent, const char *panelName );

		virtual WizardSubPanel* GetNextSubPanel();
		virtual void OnDisplayAsNext();
		virtual void PerformLayout();
		virtual void OnCommand( const char *command );
		virtual bool OnNextButton();
		virtual void SetDefaultOptions();
		virtual void UpdateOptions(); // Must be called before GetOption is used!!!
		virtual char *GetOption( int optionType );

	public:
		CheckButton *m_pOptionTeams;
		CheckButton *m_pOptionClasses;
		CheckButton *m_pOptionStamina;
		CheckButton *m_pOptionSprinting;
		CheckButton *m_pOptionProne;
		CheckButton *m_pOptionShootSprinting;
		CheckButton *m_pOptionShootOnLadders;
		CheckButton *m_pOptionShootJumping;

		bool m_bOptions[TPOPTIONS_TOTAL];

	};

}


#endif // MODWIZARD_TEMPLATEOPTIONS_H
