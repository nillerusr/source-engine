//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#ifndef SYSEXTERNAL_H
#define SYSEXTERNAL_H
#ifdef _WIN32
#pragma once
#endif

// an error will cause the entire program to exit and upload a minidump to us
void Sys_Error(PRINTF_FORMAT_STRING const char *psz, ...) FMTFUNCTION( 1, 2 );
// kill the process with an error but don't send us a minidump, its not a bug but a user config problem
void Sys_Exit( PRINTF_FORMAT_STRING const char *error, ... ) FMTFUNCTION( 1, 2 );

#endif // SYSEXTERNAL_H
