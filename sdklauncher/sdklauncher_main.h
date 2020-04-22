//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef SDKLAUNCHER_MAIN_H
#define SDKLAUNCHER_MAIN_H
#ifdef _WIN32
#pragma once
#endif

#include "tier2/vconfig.h"
#include <vgui_controls/Frame.h>


// This points at the root sourcesdk directory.
#define SDKLAUNCHER_MAIN_PATH_ID	"MAIN"


const char* GetSDKLauncherBinDirectory();
const char* GetSDKToolsBinDirectory();
const char* GetSDKLauncherBaseDirectory();
const char* GetLastWindowsErrorString();

// Replace all occurences of %basedir% with the actual base dir.
void SubstituteBaseDir( const char *pIn, char *pOut, int outLen );

// Copy a file, applying replacements you specify.
// ppReplacments must come in pairs - the first one is the string to match and 
// the second is the string to replace it with.
bool CopyWithReplacements( 
	const char *pInputFilename, 
	const char **ppReplacements, int nReplacements,
	const char *pOutputFilenameFormat, ... );

// Open the file, read it in, and do some replacements. Returns a pointer to
// a string with the contents replaced. dataWriteLen specifies how much
// data should be written to an output file if you write the string out
// (since it may have added a null terminator).
CUtlVector<char>* GetFileStringWithReplacements( 
	const char *pInputFilename, 
	const char **ppReplacements, int nReplacements,
	int &dataWriteLen );


void VGUIMessageBox( vgui::Panel *pParent, const char *pTitle, PRINTF_FORMAT_STRING const char *pMsg, ... );

class CSDKLauncherDialog;
extern CSDKLauncherDialog *g_pMainFrame;


extern bool g_bAutoHL2Mod; // skip modwizard_intro...
extern bool g_bModWizard_CmdLineFields;
extern char g_ModWizard_CmdLine_ModDir[MAX_PATH];
extern char g_ModWizard_CmdLine_ModName[256];

// Set this to make the app exit.
extern bool g_bAppQuit;


#endif // SDKLAUNCHER_MAIN_H
