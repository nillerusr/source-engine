//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:  Implements the Effects API
// Created:  YWB 9/5/2000
//
//===========================================================================//


#include "quakedef.h"
#include "r_efx.h"
#include "r_efxextern.h"
#include "r_local.h"
#include "cl_main.h"
#include "decal.h"
#include "client.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Effects API object.
static CVEfx efx;

// Engine internal accessor to effects api ( see cl_parsetent.cpp, etc. )
CVEfx *g_pEfx = &efx;
 
extern	CClientState	cl;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CVEfx::Draw_DecalIndexFromName( char *name )
{
	bool found = false;
	return ::Draw_DecalIndexFromName( name, &found );
}

//-----------------------------------------------------------------------------
// Retrieve decal texture name from decal by index
//-----------------------------------------------------------------------------
const char *CVEfx::Draw_DecalNameFromIndex( int nIndex )
{
	return ::Draw_DecalNameFromIndex( nIndex );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : textureIndex - 
//			entity - 
//			modelIndex - 
//			position - 
//			flags - 
//-----------------------------------------------------------------------------
void CVEfx::DecalShoot( int textureIndex, int entity, const model_t *model, const Vector& model_origin, const QAngle& model_angles, const Vector& position, const Vector *saxis, int flags)
{
	color32 white = {255,255,255,255};
	DecalColorShoot( textureIndex, entity, model, model_origin, model_angles, position, saxis, flags, white );
}

void CVEfx::DecalColorShoot( int textureIndex, int entity, const model_t *model, const Vector& model_origin, const QAngle& model_angles, 
	const Vector& position, const Vector *saxis, int flags, const color32 &rgbaColor)
{
	Vector localPosition = position;
	if ( entity ) 	// Not world?
	{
		matrix3x4_t matrix;
		AngleMatrix( model_angles, model_origin, matrix );
		VectorITransform( position, matrix, localPosition );
	}

	::R_DecalShoot( textureIndex, entity, model, localPosition, saxis, flags, rgbaColor, NULL );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *material - 
//			userdata - 
//			entity - 
//			*model - 
//			position - 
//			*saxis - 
//			flags - 
//			&rgbaColor - 
//-----------------------------------------------------------------------------
void CVEfx::PlayerDecalShoot( IMaterial *material, void *userdata, int entity, const model_t *model, const Vector& model_origin, const QAngle& model_angles, 
	const Vector& position, const Vector *saxis, int flags, const color32 &rgbaColor )
{
	Vector localPosition = position;
	if ( entity ) 	// Not world?
	{
		matrix3x4_t matrix;
		AngleMatrix( model_angles, model_origin, matrix );
		VectorITransform( position, matrix, localPosition );
	}

	R_PlayerDecalShoot( material, userdata, entity, model, position, saxis, flags, rgbaColor );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : key - 
// Output : dlight_t
//-----------------------------------------------------------------------------
dlight_t *CVEfx::CL_AllocDlight( int key )
{
	return ::CL_AllocDlight( key );
}

int CVEfx::CL_GetActiveDLights( dlight_t *pList[MAX_DLIGHTS] )
{
	int nOut = 0;
	if ( g_bActiveDlights )
	{
		for ( int i=0; i < MAX_DLIGHTS; i++ )
		{
			if ( r_dlightactive & (1 << i) )
			{
				pList[nOut++] = &cl_dlights[i];
			}
		}
	}
	return nOut;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : key - 
// Output : dlight_t
//-----------------------------------------------------------------------------
dlight_t *CVEfx::CL_AllocElight( int key )
{
	return ::CL_AllocElight( key );
}


// Given an elight key, find it. Does not search ordinary dlights. May return NULL.
dlight_t *CVEfx::GetElightByKey( int key )
{
	if ( g_bActiveElights )
	{
		for ( unsigned int i = 0 ; i < MAX_ELIGHTS ; ++i )
		{
			// if the keys match...
			if (cl_elights[i].key == key)
			{
				// then if the light is active, return it. If it's died,
				// return NULL.
				if ( cl_elights[i].die > cl.GetTime() ) 
				{
					return cl_elights + i;
				}
				else
				{
					return NULL;
				}
			}
		}
	}

	// if we are down here, we found nothing, or no lights were active
	return NULL;
}

// Expose it to the client .dll
EXPOSE_SINGLE_INTERFACE( CVEfx, IVEfx, VENGINE_EFFECTS_INTERFACE_VERSION );
