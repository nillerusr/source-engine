//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef CREATEMODWIZARD_H
#define CREATEMODWIZARD_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Frame.h>
#include <vgui_controls/ImageList.h>
#include <vgui_controls/TextEntry.h>
#include <vgui_controls/SectionedListPanel.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/PHandle.h>
#include <vgui_controls/WizardPanel.h>
#include <FileSystem.h>
#include "vgui/mousecode.h"
#include "vgui/IScheme.h"


class CCreateModWizard : public vgui::WizardPanel
{
	typedef vgui::WizardPanel BaseClass;

public:
	CCreateModWizard( vgui::Panel *parent, const char *name, KeyValues *pKeyValues, bool bRunFromCommandLine );
	virtual ~CCreateModWizard();

	void Run();

protected:

	KeyValues *m_pKeyValues; // These come from inside the CreateMod command in the medialist.txt file.
	bool m_bRunFromCommandLine;

private:

	//DECLARE_PANELMAP();
};


// Utility functions.
bool CreateFullDirectory( const char *pDirName );
bool CreateSubdirectory( const char *pDirName, const char *pSubdirName );

bool DoCopyFile( const char *pInputFilename, const char *pOutputFilename );

void RunCreateModWizard( bool bRunFromCommandLine );

// Sets a value in the registry that other apps (like the VS Express edition) can look at.
void SetModWizardStatusCode( unsigned long code );

// Called when they get to the "finished!" dialog.
void NoteModWizardFinished();


#endif // CREATEMODWIZARD_H
