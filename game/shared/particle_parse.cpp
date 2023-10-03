//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "filesystem.h"
#include <KeyValues.h>
#include "particle_parse.h"
#include "particles/particles.h"

#ifdef GAME_DLL
#include "te_effect_dispatch.h"
#include "networkstringtable_gamedll.h"
#else
#include "c_te_effect_dispatch.h"
#include "networkstringtable_clientdll.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern void StartParticleEffect( const CEffectData &data, int nSplitScreenPlayerSlot = -1 );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int GetAttachTypeFromString( const char *pszString )
{
	if ( !pszString || !pszString[0] )
		return -1;

	// If you add new attach types, you need to add them to this list
	static const char *pAttachmentNames[MAX_PATTACH_TYPES] =
	{
		"start_at_origin",		// PATTACH_ABSORIGIN = 0,
		"follow_origin",		// PATTACH_ABSORIGIN_FOLLOW,
		"start_at_customorigin",// PATTACH_CUSTOMORIGIN,
		"follow_customorigin",  // PATTACH_CUSTOMORIGIN_FOLLOW,
		"start_at_attachment",	// PATTACH_POINT,
		"follow_attachment",	// PATTACH_POINT_FOLLOW,
		"follow_eyes",			// PATTACH_EYES_FOLLOW,
		"world_origin",			// PATTACH_WORLDORIGIN
	};

	for ( int i = 0; i < MAX_PATTACH_TYPES; i++ )
	{
		if ( FStrEq( pAttachmentNames[i], pszString ) )
			return i;
	}

	return -1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ParseParticleEffects( bool bLoadSheets )
{
	MEM_ALLOC_CREDIT();

	g_pParticleSystemMgr->ShouldLoadSheets( bLoadSheets );

	CUtlVector<CUtlString> files;
	GetParticleManifest( files );

	int nCount = files.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		g_pParticleSystemMgr->ReadParticleConfigFile( files[i], false, false );
	}

	g_pParticleSystemMgr->DecommitTempMemory();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void PrecacheStandardParticleSystems( )
{
#ifdef GAME_DLL
	// Now add each particle system name to the network string pool, so we can send string_t's 
	// down to the client instead of full particle system names.
	for ( int i = 0; i < g_pParticleSystemMgr->GetParticleSystemCount(); i++ )
	{
		const char *pParticleSystemName = g_pParticleSystemMgr->GetParticleSystemNameFromIndex(i);
		CParticleSystemDefinition *pParticleSystem = g_pParticleSystemMgr->FindParticleSystem( pParticleSystemName );
		if ( pParticleSystem->ShouldAlwaysPrecache() )
		{
			PrecacheParticleSystem( pParticleSystemName );
		}
	}
#endif
}



//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void DispatchParticleEffect( const char *pszParticleName, ParticleAttachment_t iAttachType, CBaseEntity *pEntity, const char *pszAttachmentName, bool bResetAllParticlesOnEntity, int nSplitScreenPlayerSlot /*= -1*/, IRecipientFilter *filter /*= NULL*/ )
{
	int iAttachment = -1;
	if ( pEntity && pEntity->GetBaseAnimating() )
	{
		// Find the attachment point index
		iAttachment = pEntity->GetBaseAnimating()->LookupAttachment( pszAttachmentName );
		if ( iAttachment == -1 )
		{
			Warning("Model '%s' doesn't have attachment '%s' to attach particle system '%s' to.\n", STRING(pEntity->GetBaseAnimating()->GetModelName()), pszAttachmentName, pszParticleName );
			return;
		}
	}

	DispatchParticleEffect( pszParticleName, iAttachType, pEntity, iAttachment, bResetAllParticlesOnEntity, nSplitScreenPlayerSlot, filter );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void DispatchParticleEffect( const char *pszParticleName, ParticleAttachment_t iAttachType, CBaseEntity *pEntity, int iAttachmentPoint, bool bResetAllParticlesOnEntity, int nSplitScreenPlayerSlot /*= -1*/, IRecipientFilter *filter /*= NULL*/ )
{
	CEffectData	data;

	if ( pEntity )
	{
		data.m_vOrigin = pEntity->GetAbsOrigin();
	}

	data.m_nHitBox = GetParticleSystemIndex( pszParticleName );
	if ( pEntity )
	{
#ifdef CLIENT_DLL
		data.m_hEntity = pEntity;
#else
		data.m_nEntIndex = pEntity->entindex();
#endif
		data.m_fFlags |= PARTICLE_DISPATCH_FROM_ENTITY;
	}
	data.m_nDamageType = iAttachType;
	data.m_nAttachmentIndex = iAttachmentPoint;

	if ( bResetAllParticlesOnEntity )
	{
		data.m_fFlags |= PARTICLE_DISPATCH_RESET_PARTICLES;
	}

	// Avoid an unnecessary string search, also behaves better w/ precache checks
#ifndef CLIENT_DLL
	if ( filter )
		DispatchEffect( *filter, 0.0f, "ParticleEffect", data );
	else
		DispatchEffect( "ParticleEffect", data );
#else
	StartParticleEffect( data, nSplitScreenPlayerSlot );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void DispatchParticleEffectLink( const char *pszParticleName, ParticleAttachment_t iAttachType, CBaseEntity *pEntity, CBaseEntity *pOtherEntity, int iAttachmentPoint, bool bResetAllParticlesOnEntity, int nSplitScreenPlayerSlot /*= -1*/ )
{
	CEffectData	data;

	if ( pEntity )
	{
		data.m_vOrigin = pEntity->GetAbsOrigin();
	}

	data.m_nHitBox = GetParticleSystemIndex( pszParticleName );
	if ( pEntity && pOtherEntity )
	{
#ifdef CLIENT_DLL
		data.m_hEntity = pEntity;
#else
		data.m_nEntIndex = pEntity->entindex();
#endif
		data.m_nOtherEntIndex = pOtherEntity->entindex();

		data.m_fFlags |= PARTICLE_DISPATCH_FROM_ENTITY;
	}
	data.m_nDamageType = iAttachType;
	data.m_nAttachmentIndex = iAttachmentPoint;

	if ( bResetAllParticlesOnEntity )
	{
		data.m_fFlags |= PARTICLE_DISPATCH_RESET_PARTICLES;
	}

	// Avoid an unnecessary string search, also behaves better w/ precache checks
#ifndef CLIENT_DLL
	DispatchEffect( "ParticleEffect", data );
#else
	StartParticleEffect( data, nSplitScreenPlayerSlot );
#endif
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void DispatchParticleEffect( int nEffectIndex, const Vector &vecOrigin, const QAngle &vecAngles, ParticleAttachment_t iAttachType, CBaseEntity *pEntity, int nSplitScreenPlayerSlot /*= -1*/ )
{
	CEffectData	data;

	data.m_nHitBox = nEffectIndex;
	data.m_vOrigin = vecOrigin;
	data.m_vAngles = vecAngles;

	if ( pEntity )
	{
#ifdef CLIENT_DLL
		data.m_hEntity = pEntity;
#else
		data.m_nEntIndex = pEntity->entindex();
#endif
		data.m_fFlags |= PARTICLE_DISPATCH_FROM_ENTITY;
		data.m_nDamageType = iAttachType;
	}
	else
	{
#ifdef CLIENT_DLL
		data.m_hEntity = NULL;
#else
		data.m_nEntIndex = 0;
#endif
	}

#ifndef CLIENT_DLL
	DispatchEffect( "ParticleEffect", data );
#else
	// Avoid an unnecessary search, also behaves better w/ precache checks...
	StartParticleEffect( data, nSplitScreenPlayerSlot );
#endif
}

void DispatchParticleEffect( const char *pszParticleName, const Vector &vecOrigin, const QAngle &vecAngles, ParticleAttachment_t iAttachType, CBaseEntity *pEntity, int nSplitScreenPlayerSlot /*= -1*/ )
{
	int nEffectIndex = GetParticleSystemIndex( pszParticleName );
	DispatchParticleEffect( nEffectIndex, vecOrigin, vecAngles, iAttachType, pEntity, nSplitScreenPlayerSlot );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void DispatchParticleEffect( int iEffectIndex, Vector vecOrigin, Vector vecStart, QAngle vecAngles, CBaseEntity *pEntity, int nSplitScreenPlayerSlot /*= -1*/ )
{
	CEffectData	data;

	data.m_nHitBox = iEffectIndex;
	data.m_vOrigin = vecOrigin;
	data.m_vStart = vecStart;
	data.m_vAngles = vecAngles;

	if ( pEntity )
	{
#ifdef CLIENT_DLL
		data.m_hEntity = pEntity;
#else
		data.m_nEntIndex = pEntity->entindex();
#endif
		data.m_fFlags |= PARTICLE_DISPATCH_FROM_ENTITY;
		data.m_nDamageType = PATTACH_CUSTOMORIGIN;
	}
	else
	{
#ifdef CLIENT_DLL
		data.m_hEntity = NULL;
#else
		data.m_nEntIndex = 0;
#endif
	}

#ifndef CLIENT_DLL
	DispatchEffect( "ParticleEffect", data );
#else
	// Avoid an unnecessary search, also behaves better w/ precache checks...
	StartParticleEffect( data, nSplitScreenPlayerSlot );
#endif
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void DispatchParticleEffect( const char *pszParticleName, Vector vecOrigin, QAngle vecAngles, CBaseEntity *pEntity, int nSplitScreenPlayerSlot /*= -1*/ )
{
	int iIndex = GetParticleSystemIndex( pszParticleName );
	DispatchParticleEffect( iIndex, vecOrigin, vecOrigin, vecAngles, pEntity, nSplitScreenPlayerSlot );
}

//-----------------------------------------------------------------------------
// Purpose: Yet another overload, lets us supply vecStart
//-----------------------------------------------------------------------------
void DispatchParticleEffect( const char *pszParticleName, Vector vecOrigin, Vector vecStart, QAngle vecAngles, CBaseEntity *pEntity, int nSplitScreenPlayerSlot /*= -1*/ )
{
	int iIndex = GetParticleSystemIndex( pszParticleName );
	DispatchParticleEffect( iIndex, vecOrigin, vecStart, vecAngles, pEntity, nSplitScreenPlayerSlot );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void StopParticleEffects( CBaseEntity *pEntity )
{
	CEffectData	data;

	if ( pEntity )
	{
#ifdef CLIENT_DLL
		data.m_hEntity = pEntity;
#else
		data.m_nEntIndex = pEntity->entindex();
#endif
	}

	DispatchEffect( "ParticleEffectStop", data );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void StopParticleEffect( CBaseEntity *pEntity, const char *pszParticleName )
{
	CEffectData	data;

	if ( pEntity )
	{
#ifdef CLIENT_DLL
		data.m_hEntity = pEntity;
#else
		data.m_nEntIndex = pEntity->entindex();
#endif
	}

	data.m_nHitBox = GetParticleSystemIndex( pszParticleName );

	DispatchEffect( "ParticleEffectStop", data );
}


#ifndef CLIENT_DLL

	extern CBaseEntity *GetNextCommandEntity( CBasePlayer *pPlayer, const char *name, CBaseEntity *ent );

	ConVar particle_test_file( "particle_test_file", "", FCVAR_CHEAT, "Name of the particle system to dynamically spawn" );
	ConVar particle_test_attach_mode( "particle_test_attach_mode", "follow_attachment", FCVAR_CHEAT, "Possible Values: 'start_at_attachment', 'follow_attachment', 'start_at_origin', 'follow_origin'" );
	ConVar particle_test_attach_attachment( "particle_test_attach_attachment", "0", FCVAR_CHEAT, "Attachment index for attachment mode" );

	void Particle_Test_Start( CBasePlayer* pPlayer, const char *name, bool bStart )
	{
		if ( !pPlayer )
			return;

		int iAttachType = GetAttachTypeFromString( particle_test_attach_mode.GetString() );

		if ( iAttachType < 0 )
		{
			Warning( "Invalid attach type specified for particle_test in cvar 'particle_test_attach_mode.\n" );
			return;
		}

		int iAttachmentIndex = particle_test_attach_attachment.GetInt();

		const char *pszParticleFile = particle_test_file.GetString();

		CBaseEntity *pEntity = NULL;
		while ( (pEntity = GetNextCommandEntity( pPlayer, name, pEntity )) != NULL )
		{
			/* 
			Fire the test particle system on this entity
			*/

			DispatchParticleEffect( 
				pszParticleFile,
				(ParticleAttachment_t)iAttachType,
				pEntity,
				iAttachmentIndex,
				true );				// stops existing particle systems
		}
	}

	void CC_Particle_Test_Start( const CCommand& args )
	{
		Particle_Test_Start( UTIL_GetCommandClient(), args[1], true );
	}
	static ConCommand particle_test_start("particle_test_start", CC_Particle_Test_Start, "Dispatches the test particle system with the parameters specified in particle_test_file,\n particle_test_attach_mode and particle_test_attach_param on the entity the player is looking at.\n\tArguments:   	{entity_name} / {class_name} / no argument picks what player is looking at ", FCVAR_CHEAT);


	void Particle_Test_Stop( CBasePlayer* pPlayer, const char *name, bool bStart )
	{
		if ( !pPlayer )
			return;

		CBaseEntity *pEntity = NULL;
		while ( (pEntity = GetNextCommandEntity( pPlayer, name, pEntity )) != NULL )
		{
			//Stop all particle systems on the selected entity
			DispatchParticleEffect( "", PATTACH_ABSORIGIN, pEntity, 0, true );
		}
	}

	void CC_Particle_Test_Stop( const CCommand& args )
	{
		Particle_Test_Stop( UTIL_GetCommandClient(), args[1], false );
	}
	static ConCommand particle_test_stop("particle_test_stop", CC_Particle_Test_Stop, "Stops all particle systems on the selected entities.\n\tArguments:   	{entity_name} / {class_name} / no argument picks what player is looking at ", FCVAR_CHEAT);

#endif	//CLIENT_DLL
