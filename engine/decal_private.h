//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef DECAL_PRIVATE_H
#define DECAL_PRIVATE_H

#ifdef _WIN32
#pragma once
#endif

#include "gl_model_private.h"
#include "idispinfo.h"

#define DECAL_NORMAL 0x00  // Default
#define DECAL_CUSTOM 0x01  // Clan logo, etc.

// JAY: Compress this as much as possible
// decal instance
struct decal_t
{
	decal_t				*pnext;				// linked list for each surface
	decal_t			    *pDestroyList;		//
	SurfaceHandle_t		surfID;		// Surface id for persistence / unlinking
	IMaterial			*material;
	float				lightmapOffset;

	// FIXME:
	// make dx and dy in decal space and get rid of position, so that
	// position can be rederived from the decal basis.
	Vector		position;		// location of the decal center in world space.
	Vector		saxis;			// direction of the s axis in world space
	float		dx;				// Offsets into surface texture (in texture coordinates, so we don't need floats)
	float		dy;
	float		scale;			// Pixel scale
	float		flSize;			// size of decal, used for rejecting on dispinfo planes
	float		fadeDuration;				// Negative value means to fade in
	float		fadeStartTime;
	color32		color;
	void		*userdata;		// For player decals only, decal index ( first player at slot 1 )
	DispDecalHandle_t	m_DispDecal;	// Handle to displacement decals associated with this
	unsigned short		clippedVertCount;
	unsigned short		cacheHandle;
	unsigned short		m_iDecalPool;		// index into the decal pool.
	short		flags;			// Decal flags  DECAL_*		!!!SAVED AS A BYTE (SEE HOST_CMD.C)
	short		entityIndex;	// Entity this is attached to

	// NOTE: The following variables are dynamic variables.
	// We could put these into a separate array and reference them
	// by index to reduce memory costs of this...

	int			m_iSortTree;			// MaterialSort tree id
	int			m_iSortMaterial;		// MaterialSort id.
};


#define FDECAL_PERMANENT			0x01		// This decal should not be removed in favor of any new decals
#define FDECAL_REFERENCE			0x02		// This is a decal that's been moved from another level
#define FDECAL_CUSTOM               0x04        // This is a custom clan logo and should not be saved/restored
#define FDECAL_HFLIP				0x08		// Flip horizontal (U/S) axis
#define FDECAL_VFLIP				0x10		// Flip vertical (V/T) axis

// NOTE: There are used by footprints; maybe we separate into a separate struct?
#define FDECAL_USESAXIS				0x80		// Uses the s axis field to determine orientation
#define FDECAL_DYNAMIC				0x100		// Indicates the decal is dynamic
#define FDECAL_SECONDPASS			0x200		// Decals that have to be drawn after everything else
#define FDECAL_DONTSAVE				0x800		// Decal was loaded from adjacent level, don't save out to save file for this level
#define FDECAL_PLAYERSPRAY			0x1000		// Decal is a player spray
#define FDECAL_DISTANCESCALE		0x2000		// Decal is dynamically scaled based on distance.
#define FDECAL_HASUPDATED			0x4000		// Decal has not been updated this frame yet

// Max decal (see r_decal.cpp for initialization).
extern int g_nMaxDecals;

struct worldbrushdata_t;
void R_DecalUnlink( decal_t *pdecal, worldbrushdata_t *model );

#endif			// DECAL_PRIVATE_H
