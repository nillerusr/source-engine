//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef MODWIZARD_GETMODINFO_H
#define MODWIZARD_GETMODINFO_H
#ifdef _WIN32
#pragma once
#endif


#include <vgui_controls/WizardSubPanel.h>
#include <vgui_controls/TextEntry.h>
#include <vgui_controls/Label.h>
#include "configs.h"


// --------------------------------------------------------------------------------------------------------------------- //
// CreateModWizard sub panel 2.
// This panel asks for the directory to install in and the mod name.
// --------------------------------------------------------------------------------------------------------------------- //

namespace vgui
{

	class CModWizardSubPanel_GetModInfo : public WizardSubPanel
	{
	public:
		DECLARE_CLASS_SIMPLE( CModWizardSubPanel_GetModInfo, vgui::WizardSubPanel );

	public:
		CModWizardSubPanel_GetModInfo( Panel *parent, const char *panelName );
		
		virtual WizardSubPanel* GetNextSubPanel();
		virtual void OnDisplayAsNext();
		virtual void PerformLayout();
		virtual void OnCommand( const char *command );
		virtual bool OnNextButton();
		MESSAGE_FUNC_CHARPTR( OnChooseDirectory, "DirectorySelected", dir );

	public:
		TextEntry *m_pModPath;
		TextEntry *m_pModName;
		Label *m_pModNameInfoLabel;
		ModType_t m_ModType;
	};

}


#endif // MODWIZARD_GETMODINFO_H
