//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//
	   
#ifndef CHEATCODES_H
#define CHEATCODES_H

#ifdef _WIN32
#pragma once
#endif


#include "inputsystem/ButtonCode.h"


void	ClearCheatCommands( void );
void	ReadCheatCommandsFromFile( char *pchFileName );
void	LogKeyPress( ButtonCode_t code );
void	CheckCheatCodes();


#endif // CHEATCODES_H
