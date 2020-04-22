//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#ifndef HOST_JMP_H
#define HOST_JMP_H
#pragma once

#ifndef _INC_SETJMP
#include <setjmp.h>
#endif

extern jmp_buf 		host_abortserver;
extern jmp_buf     host_enddemo;

#endif

