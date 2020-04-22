//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Methods associated with the cursor
//
// $Revision: $
// $NoKeywords: $
//=============================================================================//

#ifndef MATSURFACE_CURSOR_H
#define MATSURFACE_CURSOR_H

#ifdef _WIN32
#pragma once
#endif

#include "VGuiMatSurface/IMatSystemSurface.h"
#include <vgui/Cursor.h>

//-----------------------------------------------------------------------------
// Initializes cursors
//-----------------------------------------------------------------------------
void InitCursors();


//-----------------------------------------------------------------------------
// Selects a cursor
//-----------------------------------------------------------------------------
void CursorSelect(vgui::HCursor hCursor);


//-----------------------------------------------------------------------------
// Activates the current cursor
//-----------------------------------------------------------------------------
void ActivateCurrentCursor();


//-----------------------------------------------------------------------------
// Handles software cursors
//-----------------------------------------------------------------------------
void EnableSoftwareCursor( bool bEnable );
bool ShouldDrawSoftwareCursor();
int  GetSoftwareCursorTexture( float *px, float *py );

//-----------------------------------------------------------------------------
// handles mouse movement
//-----------------------------------------------------------------------------
void CursorSetPos(void *hwnd, int x, int y);
void CursorGetPos(void *hwnd, int &x, int &y);


//-----------------------------------------------------------------------------
// Purpose: prevents vgui from changing the cursor
//-----------------------------------------------------------------------------
void LockCursor( bool bEnable );


//-----------------------------------------------------------------------------
// Purpose: unlocks the cursor state
//-----------------------------------------------------------------------------
bool IsCursorLocked();

//-----------------------------------------------------------------------------
// Purpose: loads a custom cursor file from the file system
//-----------------------------------------------------------------------------
vgui::HCursor Cursor_CreateCursorFromFile( char const *curOrAniFile, char const *pPathID );

// Helper for shutting down cursors
void Cursor_ClearUserCursors();

#endif // MATSURFACE_CURSOR_H





