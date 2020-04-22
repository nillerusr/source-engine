//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//

#ifndef GL_DRAWLIGHTS_H
#define GL_DRAWLIGHTS_H

#ifdef _WIN32
#pragma once
#endif

// Should we draw light sprites over visible lights?
bool ActivateLightSprites( bool bActive );

// Draws sprites over all visible lights
void DrawLightSprites( void );

// Draws lighting debugging information
void DrawLightDebuggingInfo( void );

float ComputeLightRadius( dworldlight_t *pLight, bool bIsHDR );

#endif // GL_DRAWLIGHTS_H
