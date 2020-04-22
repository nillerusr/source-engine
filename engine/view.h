//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// view.h

#ifndef VIEW_H
#define VIEW_H

#ifdef _WIN32
#pragma once
#endif

class CViewSetup;
class CViewSetupV1;


void V_Init (void);
void V_Shutdown( void );
void ConvertViewSetup( const CViewSetup &setup, CViewSetupV1 &setupV1, int nFlags );

#endif // VIEW_H
