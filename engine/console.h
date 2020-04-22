//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//
#ifndef CONSOLE_H
#define CONSOLE_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>

namespace vgui
{
	class Panel;
}

typedef unsigned char byte;

//
// console
//
extern bool con_initialized;

void Con_Init (void);
void Con_Shutdown (void);   // Free overlay line buffer.
void Con_ClearNotify (void);

bool Con_IsVisible();

/*
Read the console log from disk and return it in 'buf'. Buf should come
in as an empty TEXT_BUFFER CUtlBuffer.
Returns true if the log file is successfully read.
*/
class CUtlBuffer;
bool GetConsoleLogFileData( CUtlBuffer& buf );

vgui::Panel* Con_GetConsolePanel();


#endif			// CONSOLE_H
