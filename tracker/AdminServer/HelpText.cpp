//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================


#include <stdio.h>

#include <VGUI_Controls.h>
#include "filesystem.h"
#include "HelpText.h"
using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHelpText::CHelpText(const char *mod) 
{

	char configName[200];
	_snprintf(configName,200,"Admin\\HelpFile_%s.vdf",mod);
	
	m_pHelpData = new KeyValues ("Help");	
	
	// always load the basic definiton
	LoadHelpFile("Admin\\HelpFile.vdf");
	
	// now load mod specific stuff
	if( g_pFullFileSystem->FileExists(configName) )
	{
		LoadHelpFile(configName);	
	} 

	// and load an admin mod page if you can find it
	if( g_pFullFileSystem->FileExists("Admin\\HelpFile_adminmod.vdf") )
	{
		LoadHelpFile("Admin\\HelpFile_adminmod.vdf");	
	}

}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CHelpText::~CHelpText()
{
	m_pHelpData->deleteThis();
}


void CHelpText::LoadHelpFile(const char *filename)
{


	if (!m_pHelpData->LoadFromFile(g_pFullFileSystem, filename, true, "PLATFORM"))
	{
		// failed to load...
	}
	else
	{
		
	}

}

const char *CHelpText::GetHelp(const char *keyname)
{
	KeyValues *help = m_pHelpData->FindKey(keyname, true);
	if(help)
	{
		return help->GetString("text");
	}
	else
	{
		return NULL;
	}
}





