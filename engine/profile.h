//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
#if !defined( PROFILE_H )
#define PROFILE_H
#ifdef _WIN32
#pragma once
#endif

void Host_ReadConfiguration();
void Host_WriteConfiguration( const char *filename = NULL, bool bAllVars = false );

#endif // PROFILE_H
