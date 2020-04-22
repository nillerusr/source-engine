//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#if !defined( R_EFXEXTERNH )
#define R_EFXEXTERNH
#ifdef _WIN32
#pragma once
#endif

struct dlight_t;

class IMaterial;

int			Draw_DecalIndex				( int id );
int			Draw_DecalIndexFromName		( char *name, bool *found );
void		R_DecalShoot				( int textureIndex, int entity, const model_t *model, 
										 const Vector& position, const Vector *saxis, int flags, const color32 &rgbaColor, const Vector *pNormal );
void		R_PlayerDecalShoot			( IMaterial *material, void *userdata, int entity, const model_t *model, 
										  const Vector& position, const Vector *saxis, int flags, const color32 &rgbaColor );
dlight_t	*CL_AllocDlight				( int key );
dlight_t	*CL_AllocElight				( int key );

#endif