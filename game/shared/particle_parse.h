//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef PARTICLE_PARSE_H
#define PARTICLE_PARSE_H
#ifdef _WIN32
#pragma once
#endif

#include "utlvector.h"
#include "utlstring.h"

//-----------------------------------------------------------------------------
// Particle attachment methods
//-----------------------------------------------------------------------------
enum ParticleAttachment_t
{
	PATTACH_ABSORIGIN = 0,			// Create at absorigin, but don't follow
	PATTACH_ABSORIGIN_FOLLOW,		// Create at absorigin, and update to follow the entity
	PATTACH_CUSTOMORIGIN,			// Create at a custom origin, but don't follow
	PATTACH_CUSTOMORIGIN_FOLLOW,	// Create at a custom origin, follow relative position to specified entity
	PATTACH_POINT,					// Create on attachment point, but don't follow
	PATTACH_POINT_FOLLOW,			// Create on attachment point, and update to follow the entity
	PATTACH_EYES_FOLLOW,			// Create on eyes of the attached entity, and update to follow the entity

	PATTACH_WORLDORIGIN,			// Used for control points that don't attach to an entity

	MAX_PATTACH_TYPES,
};

extern int GetAttachTypeFromString( const char *pszString );

#define PARTICLE_DISPATCH_FROM_ENTITY		(1<<0)
#define PARTICLE_DISPATCH_RESET_PARTICLES	(1<<1)

//-----------------------------------------------------------------------------
// Particle parsing methods
//-----------------------------------------------------------------------------
// Parse the particle manifest file & register the effects within it
// Only needs to be called once per game, unless tools change particle definitions
void ParseParticleEffects( bool bLoadSheets );

// Precaches standard particle systems (only necessary on server)
// Should be called once per level
void PrecacheStandardParticleSystems( );

//-----------------------------------------------------------------------------
// Particle spawning methods
//-----------------------------------------------------------------------------
void DispatchParticleEffect( const char *pszParticleName, ParticleAttachment_t iAttachType, CBaseEntity *pEntity, const char *pszAttachmentName, bool bResetAllParticlesOnEntity = false, int nSplitScreenPlayerSlot = -1, IRecipientFilter *filter = NULL );
void DispatchParticleEffect( const char *pszParticleName, ParticleAttachment_t iAttachType, CBaseEntity *pEntity = NULL, int iAttachmentPoint = -1, bool bResetAllParticlesOnEntity = false, int nSplitScreenPlayerSlot = -1, IRecipientFilter *filter = NULL );
void DispatchParticleEffect( const char *pszParticleName, Vector vecOrigin, QAngle vecAngles, CBaseEntity *pEntity = NULL, int nSplitScreenPlayerSlot = -1 );
void DispatchParticleEffect( const char *pszParticleName, const Vector &vecOrigin, const QAngle &vecAngles, ParticleAttachment_t iAttachType, CBaseEntity *pEntity = NULL, int nSplitScreenPlayerSlot = -1 );
void DispatchParticleEffect( const char *pszParticleName, Vector vecOrigin, Vector vecStart, QAngle vecAngles, CBaseEntity *pEntity = NULL, int nSplitScreenPlayerSlot = -1 );
void DispatchParticleEffect( int iEffectIndex, Vector vecOrigin, Vector vecStart, QAngle vecAngles, CBaseEntity *pEntity = NULL, int nSplitScreenPlayerSlot = -1 );
void DispatchParticleEffectLink( const char *pszParticleName, ParticleAttachment_t iAttachType, CBaseEntity *pEntity = NULL, CBaseEntity *pOtherEntity = NULL, int iAttachmentPoint = -1, bool bResetAllParticlesOnEntity = false, int nSplitScreenPlayerSlot = -1 );
void DispatchParticleEffect( int iEffectIndex, const Vector &vecOrigin, const QAngle &vecAngles, ParticleAttachment_t iAttachType, CBaseEntity *pEntity = NULL, int nSplitScreenPlayerSlot = -1 );
void StopParticleEffects( CBaseEntity *pEntity );
void StopParticleEffect( CBaseEntity *pEntity, const char *pszParticleName );


#endif // PARTICLE_PARSE_H
