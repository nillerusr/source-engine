//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
	   
// world.h
#ifndef WORLD_H
#define WORLD_H

#ifdef _WIN32
#pragma once
#endif


struct edict_t;
class ICollideable;

void SV_ClearWorld (void);
// called after the world model has been loaded, before linking any entities

void SV_SolidMoved( edict_t *pSolidEnt, ICollideable *pSolidCollide, const Vector *pPrevAbsOrigin, bool testSurroundingBoundsOnly );
void SV_TriggerMoved( edict_t *pTriggerEnt, bool testSurroundingBoundsOnly );

// Needs to be called any time an entity changes origin, mins, maxs, or solid
// flags ent->v.modified
// sets ent->v.absmin and ent->v.absmax
// if touchtriggers, calls prog functions for the intersected triggers

// This is to temporarily remove an object from the collision tree.
// Unlink returns a handle we have to use to relink
int SV_FastUnlink( edict_t *ent );
void SV_FastRelink( edict_t *ent, int tempHandle );


#endif // WORLD_H
