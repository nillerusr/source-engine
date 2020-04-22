//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#ifndef DECAL_H
#define DECAL_H

#ifdef _WIN32
#pragma once
#endif

#include "gl_model_private.h"

#ifndef DRAW_H
#include "draw.h"
#endif


void				Decal_Init( void );
void				Decal_Shutdown( void );
IMaterial			*Draw_DecalMaterial( int index );
int					Draw_DecalIndexFromName( char *name, bool *found );
const char *		Draw_DecalNameFromIndex( int index );
void				Draw_DecalSetName( int decal, char *name );
void				R_DecalShoot( int textureIndex, int entity, const model_t *model, const Vector &position, const float *saxis, int flags, const color32 &rgbaColor );
int					Draw_DecalMax( void );

#endif		// DECAL_H
