//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:	Some little utility drawing methods
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//===========================================================================//
#ifndef DRAW_H
#define DRAW_H

#ifdef _WIN32
#pragma once
#endif

class IMaterial;
IMaterial	*GL_LoadMaterial( const char *pName, const char *pTextureGroupName );
void		GL_UnloadMaterial( IMaterial *pMaterial );

#endif			// DRAW_H
