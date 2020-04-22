//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//
	   
#ifndef KEYS_H
#define KEYS_H

#ifdef _WIN32
#pragma once
#endif

#include "inputsystem/ButtonCode.h"

class CUtlBuffer;
struct InputEvent_t;


void		Key_Event( const InputEvent_t &event );

void		Key_Init( void );
void		Key_Shutdown( void );
void		Key_WriteBindings( CUtlBuffer &buf );
int			Key_CountBindings( void );
void		Key_SetBinding( ButtonCode_t code, const char *pBinding );
const char	*Key_BindingForKey( ButtonCode_t code );
const char	*Key_NameForBinding( const char *pBinding );
const char	*Key_NameForBindingExact( const char *pBinding );

void		Key_StartTrapMode( void );
bool		Key_CheckDoneTrapping( ButtonCode_t& key );


#endif // KEYS_H
