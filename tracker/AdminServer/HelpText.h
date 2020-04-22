//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef HELPTEXT_H
#define HELPTEXT_H
#ifdef _WIN32
#pragma once
#endif

#include <VGUI_KeyValues.h>


//-----------------------------------------------------------------------------
// Purpose: parses in a text file and returns help strings about key words
//-----------------------------------------------------------------------------
class CHelpText 
{
public:
	CHelpText(const char *mod);
	~CHelpText();

	void LoadHelpFile(const char *filename);
	const char *GetHelp(const char *keyname);

private:

	vgui::KeyValues *m_pHelpData;

};

#endif // HELPTEXT_H
