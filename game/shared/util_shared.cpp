//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#include "cbase.h"
#include "mathlib/mathlib.h"
#include "util_shared.h"
#include "model_types.h"
#include "convar.h"
#include "IEffects.h"
#include "vphysics/object_hash.h"
#include "mathlib/IceKey.H"
#include "checksum_crc.h"
#include "particle_parse.h"
#include "KeyValues.h"
#include "icommandline.h"

#ifdef CLIENT_DLL
	#include "clientleafsystem.h"
	#include "c_te_effect_dispatch.h"
#else
	#include "te_effect_dispatch.h"

bool NPC_CheckBrushExclude( CBaseEntity *pEntity, CBaseEntity *pBrush );
#endif


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar r_visualizetraces( "r_visualizetraces", "0", FCVAR_CHEAT );
ConVar developer("developer", "0", FCVAR_RELEASE, "Set developer message level" ); // developer mode

#ifdef DETECT_TRACE_SPIKES
float g_TraceSpikeTolerance = 0.25;
ConVar trace_spike_tolerance( "trace_spike_tolerance", "0.25" );

void DoReportExpensiveTrace( bool repeat, float time )
{
	if ( g_TraceSpikeTolerance > 0.0f )
	{
		Msg( "%s%f!\n", ( repeat ) ? "    R: " : "", time );
	}
	g_TraceSpikeTolerance = trace_spike_tolerance.GetFloat();
}
#endif

float UTIL_VecToYaw( const Vector &vec )
{
	if (vec.y == 0 && vec.x == 0)
		return 0;
	
	float yaw = atan2( vec.y, vec.x );

	yaw = RAD2DEG(yaw);

	if (yaw < 0)
		yaw += 360;

	return yaw;
}

float UTIL_VecToPitch( const Vector &vec )
{
	float pitch = 0;
	Vector tmp = vec;
	if ( VectorNormalize( tmp ) > 0 )
	{
		pitch = RAD2DEG( asin( -tmp.z ) );
	}
	return pitch;
}

float UTIL_VecToYaw( const matrix3x4_t &matrix, const Vector &vec )
{
	Vector tmp = vec;
	VectorNormalize( tmp );

	float x = matrix[0][0] * tmp.x + matrix[1][0] * tmp.y + matrix[2][0] * tmp.z;
	float y = matrix[0][1] * tmp.x + matrix[1][1] * tmp.y + matrix[2][1] * tmp.z;

	if (x == 0.0f && y == 0.0f)
		return 0.0f;
	
	float yaw = atan2( -y, x );

	yaw = RAD2DEG(yaw);

	if (yaw < 0)
		yaw += 360;

	return yaw;
}

float UTIL_VecToPitch( const matrix3x4_t &matrix, const Vector &vec )
{
	float pitch = 0;
	Vector tmp = vec;
	if ( VectorNormalize( tmp ) > 0 )
	{
		float z = matrix[0][2] * tmp.x + matrix[1][2] * tmp.y + matrix[2][2] * tmp.z;
		pitch = RAD2DEG( asin( -z ) );
		if (pitch < 0)
			pitch += 360;
	}
	return pitch;
}

Vector UTIL_YawToVector( float yaw )
{
	Vector ret;
	
	ret.z = 0;
	float angle = DEG2RAD( yaw );
	SinCos( angle, &ret.y, &ret.x );

	return ret;
}

//-----------------------------------------------------------------------------
// Purpose: Helper function get get determinisitc random values for shared/prediction code
// Input  : seedvalue - 
//			*module - 
//			line - 
// Output : static int
//-----------------------------------------------------------------------------
static int SeedFileLineHash( int seedvalue, const char *sharedname, int additionalSeed )
{
	CRC32_t retval;

	CRC32_Init( &retval );

	CRC32_ProcessBuffer( &retval, (void *)&seedvalue, sizeof( int ) );
	CRC32_ProcessBuffer( &retval, (void *)&additionalSeed, sizeof( int ) );
	CRC32_ProcessBuffer( &retval, (void *)sharedname, Q_strlen( sharedname ) );
	
	CRC32_Final( &retval );

	return (int)( retval );
}

float SharedRandomFloat( const char *sharedname, float flMinVal, float flMaxVal, int additionalSeed /*=0*/ )
{
	Assert( CBaseEntity::GetPredictionRandomSeed() != -1 );

	int seed = SeedFileLineHash( CBaseEntity::GetPredictionRandomSeed(), sharedname, additionalSeed );
	RandomSeed( seed );
	return RandomFloat( flMinVal, flMaxVal );
}

int SharedRandomInt( const char *sharedname, int iMinVal, int iMaxVal, int additionalSeed /*=0*/ )
{
	Assert( CBaseEntity::GetPredictionRandomSeed() != -1 );

	int seed = SeedFileLineHash( CBaseEntity::GetPredictionRandomSeed(), sharedname, additionalSeed );
	RandomSeed( seed );
	return RandomInt( iMinVal, iMaxVal );
}

Vector SharedRandomVector( const char *sharedname, float minVal, float maxVal, int additionalSeed /*=0*/ )
{
	Assert( CBaseEntity::GetPredictionRandomSeed() != -1 );

	int seed = SeedFileLineHash( CBaseEntity::GetPredictionRandomSeed(), sharedname, additionalSeed );
	RandomSeed( seed );
	// HACK:  Can't call RandomVector/Angle because it uses rand() not vstlib Random*() functions!
	// Get a random vector.
	Vector random;
	random.x = RandomFloat( minVal, maxVal );
	random.y = RandomFloat( minVal, maxVal );
	random.z = RandomFloat( minVal, maxVal );
	return random;
}

QAngle SharedRandomAngle( const char *sharedname, float minVal, float maxVal, int additionalSeed /*=0*/ )
{
	Assert( CBaseEntity::GetPredictionRandomSeed() != -1 );

	int seed = SeedFileLineHash( CBaseEntity::GetPredictionRandomSeed(), sharedname, additionalSeed );
	RandomSeed( seed );

	// HACK:  Can't call RandomVector/Angle because it uses rand() not vstlib Random*() functions!
	// Get a random vector.
	Vector random;
	random.x = RandomFloat( minVal, maxVal );
	random.y = RandomFloat( minVal, maxVal );
	random.z = RandomFloat( minVal, maxVal );
	return QAngle( random.x, random.y, random.z );
}


//-----------------------------------------------------------------------------
//
// Shared client/server trace filter code
//
//-----------------------------------------------------------------------------
bool PassServerEntityFilter( const IHandleEntity *pTouch, const IHandleEntity *pPass ) 
{
	if ( !pPass )
		return true;

	if ( pTouch == pPass )
		return false;

	const CBaseEntity *pEntTouch = EntityFromEntityHandle( pTouch );
	const CBaseEntity *pEntPass = EntityFromEntityHandle( pPass );
	if ( !pEntTouch || !pEntPass )
		return true;

	// don't clip against own missiles
	if ( pEntTouch->GetOwnerEntity() == pEntPass )
		return false;
	
	// don't clip against owner
	if ( pEntPass->GetOwnerEntity() == pEntTouch )
		return false;	


	return true;
}


//-----------------------------------------------------------------------------
// A standard filter to be applied to just about everything.
//-----------------------------------------------------------------------------
bool StandardFilterRules( IHandleEntity *pHandleEntity, int fContentsMask )
{
	CBaseEntity *pCollide = EntityFromEntityHandle( pHandleEntity );

	// Static prop case...
	if ( !pCollide )
		return true;

	SolidType_t solid = pCollide->GetSolid();
	const model_t *pModel = pCollide->GetModel();

	if ( ( modelinfo->GetModelType( pModel ) != mod_brush ) || (solid != SOLID_BSP && solid != SOLID_VPHYSICS) )
	{
		if ( (fContentsMask & CONTENTS_MONSTER) == 0 )
			return false;
	}

	// This code is used to cull out tests against see-thru entities
	if ( !(fContentsMask & CONTENTS_WINDOW) )
	{
#ifdef CLIENT_DLL
		if ( g_pClientLeafSystem->GetTranslucencyType( pCollide->RenderHandle() ) == RENDERABLE_IS_TRANSLUCENT )
			return false;
#else
		bool bIsTranslucent = modelinfo->IsTranslucent( pModel );
		bool bIsTwoPass = modelinfo->IsTranslucentTwoPass( pModel );
		if ( bIsTranslucent && !bIsTwoPass )
			return false;
#endif
	}

	// FIXME: this is to skip BSP models that are entities that can be 
	// potentially moved/deleted, similar to a monster but doors don't seem to 
	// be flagged as monsters
	// FIXME: the FL_WORLDBRUSH looked promising, but it needs to be set on 
	// everything that's actually a worldbrush and it currently isn't
	if ( !(fContentsMask & CONTENTS_MOVEABLE) && (pCollide->GetMoveType() == MOVETYPE_PUSH))// !(touch->flags & FL_WORLDBRUSH) )
		return false;

	return true;
}



//-----------------------------------------------------------------------------
// Simple trace filter
//-----------------------------------------------------------------------------
CTraceFilterSimple::CTraceFilterSimple( const IHandleEntity *passedict, int collisionGroup,
										ShouldHitFunc_t pExtraShouldHitFunc )
{
	m_pPassEnt = passedict;
	m_collisionGroup = collisionGroup;
	m_pExtraShouldHitCheckFunction = pExtraShouldHitFunc;
}


//-----------------------------------------------------------------------------
// The trace filter!
//-----------------------------------------------------------------------------
bool CTraceFilterSimple::ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
{
	if ( !StandardFilterRules( pHandleEntity, contentsMask ) )
		return false;

	if ( m_pPassEnt )
	{
		if ( !PassServerEntityFilter( pHandleEntity, m_pPassEnt ) )
		{
			return false;
		}
	}

	// Don't test if the game code tells us we should ignore this collision...
	CBaseEntity *pEntity = EntityFromEntityHandle( pHandleEntity );
	if ( !pEntity )
		return false;
	if ( !pEntity->ShouldCollide( m_collisionGroup, contentsMask ) )
		return false;
	if ( pEntity && !g_pGameRules->ShouldCollide( m_collisionGroup, pEntity->GetCollisionGroup() ) )
		return false;
	if ( m_pExtraShouldHitCheckFunction &&
		 (! ( m_pExtraShouldHitCheckFunction( pHandleEntity, contentsMask ) ) ) )
		return false;
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Trace filter that only hits NPCs and the player
//-----------------------------------------------------------------------------
bool CTraceFilterOnlyNPCsAndPlayer::ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
{
	if ( CTraceFilterSimple::ShouldHitEntity( pHandleEntity, contentsMask ) )
	{
		CBaseEntity *pEntity = EntityFromEntityHandle( pHandleEntity );
		if ( !pEntity )
			return false;


		return (pEntity->IsNPC() || pEntity->IsPlayer());
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Trace filter that only hits anything but NPCs and the player
//-----------------------------------------------------------------------------
bool CTraceFilterNoNPCsOrPlayer::ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
{
	if ( CTraceFilterSimple::ShouldHitEntity( pHandleEntity, contentsMask ) )
	{
		CBaseEntity *pEntity = EntityFromEntityHandle( pHandleEntity );
		if ( !pEntity )
			return NULL;
#ifndef CLIENT_DLL
		if ( pEntity->Classify() == CLASS_PLAYER_ALLY )
			return false; // CS hostages are CLASS_PLAYER_ALLY but not IsNPC()
#endif
		return (!pEntity->IsNPC() && !pEntity->IsPlayer());
	}
	return false;
}

//-----------------------------------------------------------------------------
// Trace filter that skips two entities
//-----------------------------------------------------------------------------
CTraceFilterSkipTwoEntities::CTraceFilterSkipTwoEntities( const IHandleEntity *passentity, const IHandleEntity *passentity2, int collisionGroup ) :
	BaseClass( passentity, collisionGroup ), m_pPassEnt2(passentity2)
{
}

bool CTraceFilterSkipTwoEntities::ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
{
	Assert( pHandleEntity );
	if ( !PassServerEntityFilter( pHandleEntity, m_pPassEnt2 ) )
		return false;

	return BaseClass::ShouldHitEntity( pHandleEntity, contentsMask );
}


//-----------------------------------------------------------------------------
// Trace filter that can take a list of entities to ignore
//-----------------------------------------------------------------------------
CTraceFilterSimpleList::CTraceFilterSimpleList( int collisionGroup ) :
	CTraceFilterSimple( NULL, collisionGroup )
{
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTraceFilterSimpleList::ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
{
	if ( m_PassEntities.Find(pHandleEntity) != m_PassEntities.InvalidIndex() )
		return false;

	return CTraceFilterSimple::ShouldHitEntity( pHandleEntity, contentsMask );
}


//-----------------------------------------------------------------------------
// Purpose: Add an entity to my list of entities to ignore in the trace
//-----------------------------------------------------------------------------
void CTraceFilterSimpleList::AddEntityToIgnore( IHandleEntity *pEntity )
{
	m_PassEntities.AddToTail( pEntity );
}

void CTraceFilterSimpleList::AddEntitiesToIgnore( int nCount, IHandleEntity **ppEntities )
{
	int nIndex = m_PassEntities.AddMultipleToTail( nCount );
	memcpy( &m_PassEntities[nIndex], ppEntities, nCount * sizeof( IHandleEntity* ) );
}

//-----------------------------------------------------------------------------
// Trace filter that hits only the pass entity
//-----------------------------------------------------------------------------
CTraceFilterOnlyHitThis::CTraceFilterOnlyHitThis( const IHandleEntity *hitentity )
{
	m_pHitEnt = hitentity;
}

bool CTraceFilterOnlyHitThis::ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
{
	return m_pHitEnt == pHandleEntity;
}


//-----------------------------------------------------------------------------
// Purpose: Custom trace filter used for NPC LOS traces
//-----------------------------------------------------------------------------
CTraceFilterLOS::CTraceFilterLOS( IHandleEntity *pHandleEntity, int collisionGroup, IHandleEntity *pHandleEntity2 ) :
		CTraceFilterSkipTwoEntities( pHandleEntity, pHandleEntity2, collisionGroup )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTraceFilterLOS::ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
{
	CBaseEntity *pEntity = EntityFromEntityHandle( pHandleEntity );

	if ( !pEntity->BlocksLOS() )
		return false;

	return CTraceFilterSimple::ShouldHitEntity( pHandleEntity, contentsMask );
}

//-----------------------------------------------------------------------------
// Trace filter that can take a classname to ignore
//-----------------------------------------------------------------------------
CTraceFilterSkipClassname::CTraceFilterSkipClassname( const IHandleEntity *passentity, const char *pchClassname, int collisionGroup ) :
CTraceFilterSimple( passentity, collisionGroup ), m_pchClassname( pchClassname )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTraceFilterSkipClassname::ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
{
	CBaseEntity *pEntity = EntityFromEntityHandle( pHandleEntity );
	if ( !pEntity || FClassnameIs( pEntity, m_pchClassname ) )
		return false;

	return CTraceFilterSimple::ShouldHitEntity( pHandleEntity, contentsMask );
}

//-----------------------------------------------------------------------------
// Trace filter that skips two classnames
//-----------------------------------------------------------------------------
CTraceFilterSkipTwoClassnames::CTraceFilterSkipTwoClassnames( const IHandleEntity *passentity, const char *pchClassname, const char *pchClassname2, int collisionGroup ) :
BaseClass( passentity, pchClassname, collisionGroup ), m_pchClassname2(pchClassname2)
{
}

bool CTraceFilterSkipTwoClassnames::ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
{
	CBaseEntity *pEntity = EntityFromEntityHandle( pHandleEntity );
	if ( !pEntity || FClassnameIs( pEntity, m_pchClassname2 ) )
		return false;

	return BaseClass::ShouldHitEntity( pHandleEntity, contentsMask );
}

//-----------------------------------------------------------------------------
// Trace filter that can take a list of entities to ignore
//-----------------------------------------------------------------------------
CTraceFilterSimpleClassnameList::CTraceFilterSimpleClassnameList( const IHandleEntity *passentity, int collisionGroup ) :
CTraceFilterSimple( passentity, collisionGroup )
{
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTraceFilterSimpleClassnameList::ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
{
	CBaseEntity *pEntity = EntityFromEntityHandle( pHandleEntity );
	if ( !pEntity )
		return false;

	for ( int i = 0; i < m_PassClassnames.Count(); ++i )
	{
		if ( FClassnameIs( pEntity, m_PassClassnames[ i ] ) )
			return false;
	}

	return CTraceFilterSimple::ShouldHitEntity( pHandleEntity, contentsMask );
}


//-----------------------------------------------------------------------------
// Purpose: Add an entity to my list of entities to ignore in the trace
//-----------------------------------------------------------------------------
void CTraceFilterSimpleClassnameList::AddClassnameToIgnore( const char *pchClassname )
{
	m_PassClassnames.AddToTail( pchClassname );
}

CTraceFilterChain::CTraceFilterChain( ITraceFilter *pTraceFilter1, ITraceFilter *pTraceFilter2 )
{
	m_pTraceFilter1 = pTraceFilter1;
	m_pTraceFilter2 = pTraceFilter2;
}

bool CTraceFilterChain::ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
{
	bool bResult1 = true;
	bool bResult2 = true;

	if ( m_pTraceFilter1 )
		bResult1 = m_pTraceFilter1->ShouldHitEntity( pHandleEntity, contentsMask );

	if ( m_pTraceFilter2 )
		bResult2 = m_pTraceFilter2->ShouldHitEntity( pHandleEntity, contentsMask );

	return ( bResult1 && bResult2 );
}

//-----------------------------------------------------------------------------
// Sweeps against a particular model, using collision rules 
//-----------------------------------------------------------------------------
void UTIL_TraceModel( const Vector &vecStart, const Vector &vecEnd, const Vector &hullMin, 
					  const Vector &hullMax, CBaseEntity *pentModel, int collisionGroup, trace_t *ptr )
{
	// Cull it....
	if ( pentModel && pentModel->ShouldCollide( collisionGroup, MASK_ALL ) )
	{
		Ray_t ray;
		ray.Init( vecStart, vecEnd, hullMin, hullMax );
		enginetrace->ClipRayToEntity( ray, MASK_ALL, pentModel, ptr ); 
	}
	else
	{
		memset( ptr, 0, sizeof(trace_t) );
		ptr->fraction = 1.0f;
	}
}

bool UTIL_EntityHasMatchingRootParent( CBaseEntity *pRootParent, CBaseEntity *pEntity )
{
	if ( pRootParent )
	{
		// NOTE: Don't let siblings/parents collide.
		if ( pRootParent == pEntity->GetRootMoveParent() )
			return true;
		if ( pEntity->GetOwnerEntity() && pRootParent == pEntity->GetOwnerEntity()->GetRootMoveParent() )
			return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
// Sweep an entity from the starting to the ending position 
//-----------------------------------------------------------------------------
class CTraceFilterEntity : public CTraceFilterSimple
{
	DECLARE_CLASS( CTraceFilterEntity, CTraceFilterSimple );

public:
	CTraceFilterEntity( CBaseEntity *pEntity, int nCollisionGroup ) 
		: CTraceFilterSimple( pEntity, nCollisionGroup )
	{
		m_pRootParent = pEntity->GetRootMoveParent();
		m_pEntity = pEntity;
		m_checkHash = g_EntityCollisionHash->IsObjectInHash(pEntity);
	}

	bool ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
	{
		CBaseEntity *pEntity = EntityFromEntityHandle( pHandleEntity );
		if ( !pEntity )
			return false;

		// Check parents against each other
		// NOTE: Don't let siblings/parents collide.
		if ( UTIL_EntityHasMatchingRootParent( m_pRootParent, pEntity ) )
			return false;

		if ( m_checkHash )
		{
			if ( g_EntityCollisionHash->IsObjectPairInHash( m_pEntity, pEntity ) )
				return false;
		}

#ifndef CLIENT_DLL
		if ( m_pEntity->IsNPC() )
		{
			if ( NPC_CheckBrushExclude( m_pEntity, pEntity ) )
				 return false;

		}
#endif

		return BaseClass::ShouldHitEntity( pHandleEntity, contentsMask );
	}

private:

	CBaseEntity *m_pRootParent;
	CBaseEntity *m_pEntity;
	bool		m_checkHash;
};

class CTraceFilterEntityIgnoreOther : public CTraceFilterEntity
{
	DECLARE_CLASS( CTraceFilterEntityIgnoreOther, CTraceFilterEntity );
public:
	CTraceFilterEntityIgnoreOther( CBaseEntity *pEntity, const IHandleEntity *pIgnore, int nCollisionGroup ) : 
		CTraceFilterEntity( pEntity, nCollisionGroup ), m_pIgnoreOther( pIgnore )
	{
	}

	bool ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
	{
		if ( pHandleEntity == m_pIgnoreOther )
			return false;

		return BaseClass::ShouldHitEntity( pHandleEntity, contentsMask );
	}

private:
	const IHandleEntity *m_pIgnoreOther;
};

//-----------------------------------------------------------------------------
// Sweeps a particular entity through the world 
//-----------------------------------------------------------------------------
void UTIL_TraceEntity( CBaseEntity *pEntity, const Vector &vecAbsStart, const Vector &vecAbsEnd, unsigned int mask, trace_t *ptr )
{
	ICollideable *pCollision = pEntity->GetCollideable();

	// Adding this assertion here so game code catches it, but really the assertion belongs in the engine
	// because one day, rotated collideables will work!
	Assert( pCollision->GetCollisionAngles() == vec3_angle );

	CTraceFilterEntity traceFilter( pEntity, pCollision->GetCollisionGroup() );


	enginetrace->SweepCollideable( pCollision, vecAbsStart, vecAbsEnd, pCollision->GetCollisionAngles(), mask, &traceFilter, ptr );

}

void UTIL_TraceEntity( CBaseEntity *pEntity, const Vector &vecAbsStart, const Vector &vecAbsEnd, 
					  unsigned int mask, const IHandleEntity *pIgnore, int nCollisionGroup, trace_t *ptr )
{
	ICollideable *pCollision;
	pCollision = pEntity->GetCollideable();

	// Adding this assertion here so game code catches it, but really the assertion belongs in the engine
	// because one day, rotated collideables will work!
	Assert( pCollision->GetCollisionAngles() == vec3_angle );

	CTraceFilterEntityIgnoreOther traceFilter( pEntity, pIgnore, nCollisionGroup );


	enginetrace->SweepCollideable( pCollision, vecAbsStart, vecAbsEnd, pCollision->GetCollisionAngles(), mask, &traceFilter, ptr );

}

void UTIL_TraceEntity( CBaseEntity *pEntity, const Vector &vecAbsStart, const Vector &vecAbsEnd, 
					  unsigned int mask, ITraceFilter *pFilter, trace_t *ptr )
{
	ICollideable *pCollision;
	pCollision = pEntity->GetCollideable();

	// Adding this assertion here so game code catches it, but really the assertion belongs in the engine
	// because one day, rotated collideables will work!
	Assert( pCollision->GetCollisionAngles() == vec3_angle );


	enginetrace->SweepCollideable( pCollision, vecAbsStart, vecAbsEnd, pCollision->GetCollisionAngles(), mask, pFilter, ptr );

}

// ----
// This is basically a regular TraceLine that uses the FilterEntity filter.
void UTIL_TraceLineFilterEntity( CBaseEntity *pEntity, const Vector &vecAbsStart, const Vector &vecAbsEnd, 
					   unsigned int mask, int nCollisionGroup, trace_t *ptr )
{
	CTraceFilterEntity traceFilter( pEntity, nCollisionGroup );
	UTIL_TraceLine( vecAbsStart, vecAbsEnd, mask, &traceFilter, ptr );
}

void UTIL_ClipTraceToPlayers( const Vector& vecAbsStart, const Vector& vecAbsEnd, unsigned int mask, ITraceFilter *filter, trace_t *tr )
{
	trace_t playerTrace;
	Ray_t ray;
	float smallestFraction = tr->fraction;
	const float maxRange = 60.0f;

	ray.Init( vecAbsStart, vecAbsEnd );

	for ( int k = 1; k <= gpGlobals->maxClients; ++k )
	{
		CBasePlayer *player = UTIL_PlayerByIndex( k );

		if ( !player || !player->IsAlive() )
			continue;

#ifdef CLIENT_DLL
		if ( player->IsDormant() )
			continue;
#endif // CLIENT_DLL

		if ( filter && filter->ShouldHitEntity( player, mask ) == false )
			continue;

		float range = DistanceToRay( player->WorldSpaceCenter(), vecAbsStart, vecAbsEnd );
		if ( range < 0.0f || range > maxRange )
			continue;

		enginetrace->ClipRayToEntity( ray, mask|CONTENTS_HITBOX, player, &playerTrace );
		if ( playerTrace.fraction < smallestFraction )
		{
			// we shortened the ray - save off the trace
			*tr = playerTrace;
			smallestFraction = playerTrace.fraction;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Make a tracer using a particle effect
//-----------------------------------------------------------------------------
void UTIL_ParticleTracer( const char *pszTracerEffectName, const Vector &vecStart, const Vector &vecEnd, 
				 int iEntIndex, int iAttachment, bool bWhiz )
{
	int iParticleIndex = GetParticleSystemIndex( pszTracerEffectName );
	UTIL_Tracer( vecStart, vecEnd, iEntIndex, iAttachment, 0, bWhiz, "ParticleTracer", iParticleIndex );
}

//-----------------------------------------------------------------------------
// Purpose: Make a tracer effect using the old, non-particle system, tracer effects.
//-----------------------------------------------------------------------------
void UTIL_Tracer( const Vector &vecStart, const Vector &vecEnd, int iEntIndex, 
				 int iAttachment, float flVelocity, bool bWhiz, const char *pCustomTracerName, int iParticleID )
{
	CEffectData data;
	data.m_vStart = vecStart;
	data.m_vOrigin = vecEnd;
#ifdef CLIENT_DLL
	data.m_hEntity = ClientEntityList().EntIndexToHandle( iEntIndex );
#else
	data.m_nEntIndex = iEntIndex;
#endif
	data.m_flScale = flVelocity;
	data.m_nHitBox = iParticleID;

	// Flags
	if ( bWhiz )
	{
		data.m_fFlags |= TRACER_FLAG_WHIZ;
	}

	if ( iAttachment != TRACER_DONT_USE_ATTACHMENT )
	{
		data.m_fFlags |= TRACER_FLAG_USEATTACHMENT;
		// Stomp the start, since it's not going to be used anyway
		data.m_nAttachmentIndex = iAttachment;
	}

	// Fire it off
	if ( pCustomTracerName )
	{
		DispatchEffect( pCustomTracerName, data );
	}
	else
	{
		DispatchEffect( "Tracer", data );
	}
}


void UTIL_BloodDrips( const Vector &origin, const Vector &direction, int color, int amount )
{
	if ( !UTIL_ShouldShowBlood( color ) )
		return;

	if ( color == DONT_BLEED || amount == 0 )
		return;

	if ( g_Language.GetInt() == LANGUAGE_GERMAN && color == BLOOD_COLOR_RED )
		color = 0;

	if ( g_pGameRules->IsMultiplayer() )
	{
		// scale up blood effect in multiplayer for better visibility
		amount *= 5;
	}

	if ( amount > 255 )
		amount = 255;

	if (color == BLOOD_COLOR_MECH)
	{
		g_pEffects->Sparks(origin);
		if (random->RandomFloat(0, 2) >= 1)
		{
			UTIL_Smoke(origin, random->RandomInt(10, 15), 10);
		}
	}
	else
	{
		// Normal blood impact
		UTIL_BloodImpact( origin, direction, color, amount );
	}
}	

//-----------------------------------------------------------------------------
// Purpose: Returns low violence settings
//-----------------------------------------------------------------------------
static ConVar	violence_hblood( "violence_hblood","1", 0, "Draw human blood" );
static ConVar	violence_hgibs( "violence_hgibs","1", 0, "Show human gib entities" );
static ConVar	violence_ablood( "violence_ablood","1", 0, "Draw alien blood" );
static ConVar	violence_agibs( "violence_agibs","1", 0, "Show alien gib entities" );

bool UTIL_IsLowViolence( void )
{
	// These convars are no longer necessary -- the engine is the final arbiter of
	// violence settings -- but they're here for legacy support and for testing low
	// violence when the engine is in normal violence mode.
	if ( !violence_hblood.GetBool() || !violence_ablood.GetBool() || !violence_hgibs.GetBool() || !violence_agibs.GetBool() )
		return true;

	return engine->IsLowViolence();
}

bool UTIL_ShouldShowBlood( int color )
{
	if ( color != DONT_BLEED )
	{
		if ( color == BLOOD_COLOR_RED )
		{
			return violence_hblood.GetBool();
		}
		else
		{
			return violence_ablood.GetBool();
		}
	}
	return false;
}


//------------------------------------------------------------------------------
// Purpose : Use trace to pass a specific decal type to the entity being decaled
// Input   :
// Output  :
//------------------------------------------------------------------------------
void UTIL_DecalTrace( trace_t *pTrace, char const *decalName )
{
	if (pTrace->fraction == 1.0)
		return;

	CBaseEntity *pEntity = pTrace->m_pEnt;
	if ( !pEntity )
		return;
	pEntity->DecalTrace( pTrace, decalName );
}


void UTIL_BloodDecalTrace( trace_t *pTrace, int bloodColor )
{
	if ( UTIL_ShouldShowBlood( bloodColor ) )
	{
		if ( bloodColor == BLOOD_COLOR_RED )
		{
			UTIL_DecalTrace( pTrace, "Blood" );
		}
#if defined( HL2_EPISODIC )
		else if ( bloodColor == BLOOD_COLOR_BLOB )
		{
			UTIL_DecalTrace( pTrace, "BlobBlood" );
		}
		//don't draw a any decals if the blob is frozen
		else if ( bloodColor == BLOOD_COLOR_BLOB_FROZEN )
		{
			return;
		}
#endif
		else if (bloodColor == BLOOD_COLOR_BRIGHTGREEN)
		{				
			UTIL_DecalTrace( pTrace, "GreenBlood" );
		}
		else
		{
			UTIL_DecalTrace( pTrace, "YellowBlood" );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &pos - 
//			&dir - 
//			color - 
//			amount - 
//-----------------------------------------------------------------------------
void UTIL_BloodImpact( const Vector &pos, const Vector &dir, int color, int amount )
{
	CEffectData	data;

	data.m_vOrigin = pos;
	data.m_vNormal = dir;
	data.m_flScale = (float)amount;
	data.m_nColor = (unsigned char)color;

	DispatchEffect( "bloodimpact", data );
}

bool UTIL_IsSpaceEmpty( CBaseEntity *pMainEnt, const Vector &vMin, const Vector &vMax )
{
	Vector vHalfDims = ( vMax - vMin ) * 0.5f;
	Vector vCenter = vMin + vHalfDims;

	trace_t trace;
	int mask = (pMainEnt) ? pMainEnt->PhysicsSolidMaskForEntity() : MASK_SOLID;
	UTIL_TraceHull( vCenter, vCenter, -vHalfDims, vHalfDims, mask, pMainEnt, COLLISION_GROUP_NONE, &trace );

	bool bClear = ( trace.fraction == 1 && trace.allsolid != 1 && (trace.startsolid != 1) );
	return bClear;
}

bool UTIL_IsSpaceEmpty( CBaseEntity *pMainEnt, const Vector &vMin, const Vector &vMax, unsigned int mask, ITraceFilter *pFilter )
{
	Vector vHalfDims = ( vMax - vMin ) * 0.5f;
	Vector vCenter = vMin + vHalfDims;

	trace_t trace;
	UTIL_TraceHull( vCenter, vCenter, -vHalfDims, vHalfDims, mask, pFilter, &trace );

	bool bClear = ( trace.fraction == 1 && trace.allsolid != 1 && (trace.startsolid != 1) );
	return bClear;
}

void UTIL_StringToFloatArray( float *pVector, int count, const char *pString )
{
	char *pstr, *pfront, tempString[128];
	int	j;

	Q_strncpy( tempString, pString, sizeof(tempString) );
	pstr = pfront = tempString;

	for ( j = 0; j < count; j++ )			// lifted from pr_edict.c
	{
		pVector[j] = atof( pfront );

		// skip any leading whitespace
		while ( *pstr && *pstr <= ' ' )
			pstr++;

		// skip to next whitespace
		while ( *pstr && *pstr > ' ' )
			pstr++;

		if (!*pstr)
			break;

		pstr++;
		pfront = pstr;
	}
	for ( j++; j < count; j++ )
	{
		pVector[j] = 0;
	}
}

void UTIL_StringToVector( float *pVector, const char *pString )
{
	UTIL_StringToFloatArray( pVector, 3, pString );
}

#ifndef _XBOX
void UTIL_DecodeICE( unsigned char * buffer, int size, const unsigned char *key )
{
	if ( !key )
		return;

	IceKey ice( 0 ); // level 0 = 64bit key
	ice.set( key ); // set key

	int blockSize = ice.blockSize();

	unsigned char *temp = (unsigned char *) stackalloc( PAD_NUMBER( size, blockSize ) );
	unsigned char *p1 = buffer;
	unsigned char *p2 = temp;
				
	// encrypt data in 8 byte blocks
	int bytesLeft = size;
	while ( bytesLeft >= blockSize )
	{
		ice.decrypt( p1, p2 );
		bytesLeft -= blockSize;
		p1+=blockSize;
		p2+=blockSize;
	}

	// copy encrypted data back to original buffer
	Q_memcpy( buffer, temp, size-bytesLeft );
}

void UTIL_EncodeICE( unsigned char * buffer, unsigned int size, const unsigned char *key )
{
	if ( !key )
		return;

	IceKey ice( 0 ); // level 0 = 64bit key
	ice.set( key ); // set key

	unsigned char *cipherText = buffer;
	unsigned char *plainText = buffer;
	uint bytesEncrypted = 0;

	while (bytesEncrypted < size)
	{
		ice.encrypt( plainText, cipherText );
		bytesEncrypted += 8;
		cipherText += 8;
		plainText += 8;
	}
}
#endif

// work-around since client header doesn't like inlined gpGlobals->curtime
float IntervalTimer::Now( void ) const
{
	return gpGlobals->curtime;
}

// work-around since client header doesn't like inlined gpGlobals->curtime
float CountdownTimer::Now( void ) const
{
	return gpGlobals->curtime;
}


BEGIN_DATADESC_NO_BASE( IntervalTimer )
END_DATADESC()

BEGIN_NETWORK_TABLE_NOBASE( IntervalTimer, DT_IntervalTimer )
#ifdef CLIENT_DLL
	RecvPropFloat(RECVINFO(m_timestamp)),
#else
	SendPropFloat	(SENDINFO(m_timestamp), 0, SPROP_NOSCALE ),
#endif
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA_NO_BASE( IntervalTimer )
	DEFINE_PRED_FIELD( m_timestamp, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
END_PREDICTION_DATA()	
#endif


#ifdef CLIENT_DLL
BEGIN_RECV_TABLE_NOBASE( CountdownTimer, DT_CountdownTimer )
	RecvPropFloat(RECVINFO(m_duration)),
	RecvPropFloat(RECVINFO(m_timestamp)),
END_RECV_TABLE()
BEGIN_PREDICTION_DATA_NO_BASE( CountdownTimer )
	DEFINE_PRED_FIELD( m_duration, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_timestamp, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
END_PREDICTION_DATA()	
#else
BEGIN_SEND_TABLE_NOBASE( CountdownTimer, DT_CountdownTimer )
	SendPropFloat	(SENDINFO(m_duration), 0, SPROP_NOSCALE ),
	SendPropFloat	(SENDINFO(m_timestamp), 0, SPROP_NOSCALE ),
END_SEND_TABLE()
#endif


BEGIN_DATADESC( CTimeline )
	DEFINE_ARRAY( m_flValues, FIELD_FLOAT, TIMELINE_ARRAY_SIZE ),
	DEFINE_ARRAY( m_nValueCounts, FIELD_FLOAT, TIMELINE_ARRAY_SIZE ),
	DEFINE_FIELD( m_nBucketCount, FIELD_INTEGER ),
	DEFINE_FIELD( m_flInterval, FIELD_FLOAT ),
	DEFINE_FIELD( m_flFinalValue, FIELD_FLOAT ),
	DEFINE_FIELD( m_nCompressionType, FIELD_INTEGER ),
	DEFINE_FIELD( m_bStopped, FIELD_BOOLEAN ),
END_DATADESC()

BEGIN_NETWORK_TABLE_NOBASE( CTimeline, DT_Timeline )
#ifdef CLIENT_DLL
	RecvPropArray3( RECVINFO_ARRAY( m_flValues ), RecvPropFloat( RECVINFO( m_flValues[0] ) ) ),
	RecvPropArray3( RECVINFO_ARRAY( m_nValueCounts ), RecvPropFloat( RECVINFO( m_nValueCounts[0] ) ) ),
	RecvPropInt( RECVINFO( m_nBucketCount ) ),
	RecvPropFloat( RECVINFO( m_flInterval ) ),
	RecvPropFloat( RECVINFO( m_flFinalValue ) ),
	RecvPropInt( RECVINFO( m_nCompressionType ) ),
	RecvPropBool( RECVINFO( m_bStopped ) ),
#else
	SendPropArray3( SENDINFO_ARRAY3( m_flValues ), SendPropFloat( SENDINFO_ARRAY( m_flValues ), 0, SPROP_NOSCALE ) ),
	SendPropArray3( SENDINFO_ARRAY3( m_nValueCounts ), SendPropFloat( SENDINFO_ARRAY( m_nValueCounts ), 0, SPROP_NOSCALE ) ),
	SendPropInt( SENDINFO( m_nBucketCount ), NumBitsForCount( TIMELINE_ARRAY_SIZE ), SPROP_UNSIGNED ),
	SendPropFloat( SENDINFO( m_flInterval ), 0, SPROP_NOSCALE ),
	SendPropFloat( SENDINFO( m_flFinalValue ), 0, SPROP_NOSCALE ),
	SendPropInt( SENDINFO( m_nCompressionType ), -1, SPROP_UNSIGNED ),
	SendPropBool( SENDINFO( m_bStopped ) ),
#endif
END_NETWORK_TABLE()

void CTimeline::ClearValues( void )
{
	Invalidate();

	memset( m_flValues.m_Value, 0, sizeof( m_flValues.m_Value ) );
	memset( m_nValueCounts.m_Value, 0, sizeof( m_nValueCounts.m_Value ) );
	m_nBucketCount = 0;
	m_flInterval = TIMELINE_INTERVAL_START;
	m_flFinalValue = 0.0f;
	m_bStopped = false;
}

void CTimeline::RecordValue( float flValue )
{
	if ( !HasStarted() || m_bStopped )
		return;

	int iBucket = GetCurrentBucket();

	Assert( iBucket >= 0 );

	while ( iBucket >= TIMELINE_ARRAY_SIZE )
	{
		Compress();
		iBucket = GetCurrentBucket();
	}

	if ( iBucket >= m_nBucketCount )
	{
		m_nBucketCount = iBucket + 1;
	}

	m_flValues.GetForModify( iBucket ) += flValue;
	m_nValueCounts.GetForModify( iBucket )++;

	if ( m_nCompressionType == TIMELINE_COMPRESSION_SUM || 
		 m_nCompressionType == TIMELINE_COMPRESSION_AVERAGE || 
		 m_nCompressionType == TIMELINE_COMPRESSION_AVERAGE_BLEND )
	{
		// Fill in blank preceding entries
		float flCurrentValue = m_flValues[ iBucket ] / m_nValueCounts[ iBucket ];

		int iPrecedingBucket = iBucket - 1;

		while ( iPrecedingBucket >= 0 && m_nValueCounts[ iPrecedingBucket ] <= 0 )
		{
			iPrecedingBucket--;
		}

		// Get the last logged value (or the current if this is the first)
		float flPrecedingValue = flCurrentValue;

		if ( iPrecedingBucket >= 0 )
		{
			// Last logged value
			if ( m_nCompressionType == TIMELINE_COMPRESSION_SUM )
			{
				flPrecedingValue = m_flValues[ iPrecedingBucket ];

				if ( m_nValueCounts[ iBucket ] == 1 )
				{
					// Sum in the previous bucket if this is the first value in this bucket
					m_flValues.GetForModify( iBucket ) += flPrecedingValue;
				}
			}
			else
			{
				flPrecedingValue = m_flFinalValue;
			}
		}

		// Number of buckets for blending from old to new value
		float flNumBuckets = ( iBucket - iPrecedingBucket ) + 1;

		if ( flNumBuckets >= 3.0f )
		{
			if ( m_nCompressionType == TIMELINE_COMPRESSION_AVERAGE_BLEND )
			{
				// Blend empty values between preceding and current
				float flInterpBucket = 1.0f;

				for ( int i = iPrecedingBucket + 1; i < iBucket; ++i, flInterpBucket += 1.0f )
				{
					float flInterp = flInterpBucket / flNumBuckets;
					m_flValues.Set( i, flPrecedingValue * ( 1.0f - flInterp ) + flCurrentValue * flInterp );
					m_nValueCounts.Set( i, 1 );
				}
			}
			else
			{
				// Set empty values to preceding value
				for ( int i = iPrecedingBucket + 1; i < iBucket; ++i )
				{
					m_flValues.Set( i, flPrecedingValue );
					m_nValueCounts.Set( i, 1 );
				}
			}
		}
	}

	m_flFinalValue = flValue;
}

float CTimeline::GetValue( int i ) const
{
	Assert( i >= 0 && i < m_nBucketCount );

	if ( i < 0 || i >= m_nBucketCount )
	{
		return 0.0f;
	}

	if ( m_nValueCounts[ i ] <= 0 )
	{
		return 0.0f;
	}
	else if ( i == 0 && m_nCompressionType == TIMELINE_COMPRESSION_SUM && m_nBucketCount > 1 )
	{
		// Aways start at 0 for sums!
		return 0.0;
	}
	else
	{
		switch ( m_nCompressionType )
		{
		case TIMELINE_COMPRESSION_AVERAGE:
		case TIMELINE_COMPRESSION_AVERAGE_BLEND:
			return m_flValues[ i ] / m_nValueCounts[ i ];

		case TIMELINE_COMPRESSION_SUM:
		case TIMELINE_COMPRESSION_COUNT_PER_INTERVAL:
		default:
			return m_flValues[ i ];
		}
	}
}

float CTimeline::GetValueAtInterp( float fInterp ) const
{
	if ( fInterp <= 0.0f )
	{
		return GetValue( 0 );
	}

	if ( fInterp >= 1.0f )
	{
		if ( m_nCompressionType == TIMELINE_COMPRESSION_SUM || 
			 m_nCompressionType == TIMELINE_COMPRESSION_COUNT_PER_INTERVAL )
		{
			return GetValue( Count() - 1 );
		}
		else
		{
			return m_flFinalValue;
		}
	}

	float fBucket = fInterp * ( Count() - 1 );
	int nBucket = fBucket;
	fBucket -= nBucket;

	float fValue = GetValue( nBucket );
	float fNextValue = GetValue( nBucket + 1 );

	return fValue * ( 1.0f - fBucket ) + fNextValue * fBucket;
}

void CTimeline::Compress( void )
{
	int i, j;

	switch ( m_nCompressionType )
	{
	case TIMELINE_COMPRESSION_SUM:
		for ( i = 0, j = 0; i < TIMELINE_ARRAY_SIZE; i += 2, ++j )
		{
			m_flValues.GetForModify( j ) = MAX( m_flValues[ i ], m_flValues[ i + 1 ] );
			m_nValueCounts.GetForModify( j ) = m_nValueCounts[ i ] + m_nValueCounts[ i + 1 ];
		}
		break;

	default:
		for ( i = 0, j = 0; i < TIMELINE_ARRAY_SIZE; i += 2, ++j )
		{
			m_flValues.GetForModify( j ) = m_flValues[ i ] + m_flValues[ i + 1 ];
			m_nValueCounts.GetForModify( j ) = m_nValueCounts[ i ] + m_nValueCounts[ i + 1 ];
		}
		break;
	}

	int nRemainingBytes = ( TIMELINE_ARRAY_SIZE - j ) * sizeof( m_flValues[0] );

	memset( &( m_flValues.GetForModify( j ) ), 0, nRemainingBytes );
	memset( &( m_nValueCounts.GetForModify( j ) ), 0, nRemainingBytes );

	m_flInterval *= 2.0f;

	m_nBucketCount = j;
}


#ifdef CLIENT_DLL
	CBasePlayer *UTIL_PlayerByIndex( int entindex )
	{	
		// Sanity check the index being passed in
		if ( entindex < 1 || entindex > gpGlobals->maxClients )
			return NULL;

		return ToBasePlayer( ClientEntityList().GetEnt( entindex ) );
	}
#endif

unsigned short UTIL_GetAchievementEventMask( void )
{
	CRC32_t mapCRC;
	CRC32_Init( &mapCRC );

	char lowercase[ 256 ];
#ifdef CLIENT_DLL
	Q_FileBase( engine->GetLevelName(), lowercase, sizeof( lowercase ) );
#else
	Q_strncpy( lowercase, STRING( gpGlobals->mapname ), sizeof( lowercase ) );
#endif
	Q_strlower( lowercase );

	CRC32_ProcessBuffer( &mapCRC, lowercase, Q_strlen( lowercase ) );
	CRC32_Final( &mapCRC );

	return ( mapCRC & 0xFFFF );
}

char* ReadAndAllocStringValue( KeyValues *pSub, const char *pName, const char *pFilename )
{
	const char *pValue = pSub->GetString( pName, NULL );
	if ( !pValue )
	{
		if ( pFilename )
		{
			DevWarning( "Can't get key value	'%s' from file '%s'.\n", pName, pFilename );
		}
		return "";
	}

	int len = Q_strlen( pValue ) + 1;
	char *pAlloced = new char[ len ];
	Assert( pAlloced );
	Q_strncpy( pAlloced, pValue, len );
	return pAlloced;
}

int UTIL_StringFieldToInt( const char *szValue, const char **pValueStrings, int iNumStrings )
{
	if ( !szValue || !szValue[0] )
		return -1;

	for ( int i = 0; i < iNumStrings; i++ )
	{
		if ( FStrEq(szValue, pValueStrings[i]) )
			return i;
	}

	Assert(0);
	return -1;
}

static char s_NumBitsInNibble[ 16 ] = 
{
	0, // 0000 = 0
	1, // 0001 = 1
	1, // 0010 = 2
	2, // 0011 = 3
	1, // 0100 = 4
	2, // 0101 = 5
	2, // 0110 = 6
	3, // 0111 = 7
	1, // 1000 = 8
	2, // 1001 = 9
	2, // 1010 = 10
	3, // 1011 = 11
	2, // 1100 = 12
	3, // 1101 = 13
	3, // 1110 = 14
	4, // 1111 = 15
};

int UTIL_CountNumBitsSet( unsigned int nVar )
{
	int nNumBits = 0;

	while ( nVar > 0 )
	{
		// Look up and add in bits in the bottom nibble
		nNumBits += s_NumBitsInNibble[ nVar & 0x0f ];

		// Shift one nibble to the right
		nVar >>= 4;
	}

	return nNumBits;
}

int UTIL_CountNumBitsSet( uint64 nVar )
{
	int nNumBits = 0;

	while ( nVar > 0 )
	{
		// Look up and add in bits in the bottom nibble
		nNumBits += s_NumBitsInNibble[ nVar & 0x0f ];

		// Shift one nibble to the right
		nVar >>= 4;
	}

	return nNumBits;
}

bool UTIL_FindClosestPassableSpace( const Vector &vOriginalCenter, const Vector &vExtents, const Vector &vIndecisivePush, ITraceFilter *pTraceFilter, unsigned int fMask, unsigned int iIterations, Vector &vCenterOut, int nAxisRestrictionFlags )
{
	Assert( vExtents != vec3_origin );

	trace_t traces[2];
	Ray_t entRay;
	entRay.m_Extents = vExtents;
	entRay.m_IsRay = false;
	entRay.m_IsSwept = true;
	entRay.m_StartOffset = vec3_origin;

	Vector vOriginalExtents = vExtents;
	Vector vCenter = vOriginalCenter;
	Vector vGrowSize = vExtents * (1.0f / (float)(iIterations + 1));
	Vector vCurrentExtents = vExtents - vGrowSize;

	int iLargestExtent = 0;
	{
		float fLargestExtent = vOriginalExtents[0];
		for( int i = 1; i != 3; ++i )
		{
			if( vOriginalExtents[i] > fLargestExtent )
			{
				iLargestExtent = i;
				fLargestExtent = vOriginalExtents[i];
			}
		}
	}


	Ray_t testRay;
	testRay.m_Extents = vGrowSize;
	testRay.m_IsRay = false;
	testRay.m_IsSwept = true;
	testRay.m_StartOffset = vec3_origin;

	float fOriginalExtentDists[8]; //distance between extents
	//generate distance lookup. We reference this by XOR'ing the indices of two extents to find the axis of difference
	{
		//Since the ratios of lengths never change, we're going to normalize these distances to a value so we can simply scale on each iteration
		//We've picked the largest extent as the basis simply because it's nonzero
		float fNormalizer = 1.0f / vOriginalExtents[iLargestExtent];
				
		float fXDiff = vOriginalExtents.x * 2.0f * fNormalizer;
		float fXSqr = fXDiff * fXDiff;

		float fYDiff = vOriginalExtents.y * 2.0f * fNormalizer;
		float fYSqr = fYDiff * fYDiff;

		float fZDiff = vOriginalExtents.z * 2.0f * fNormalizer;
		float fZSqr = fZDiff * fZDiff;

		fOriginalExtentDists[0] = 0.0f; //should never get hit
		fOriginalExtentDists[1] = fXDiff; //line along x axis		
		fOriginalExtentDists[2] = fYDiff; //line along y axis
		fOriginalExtentDists[3] = sqrt( fXSqr + fYSqr ); //diagonal perpendicular to z-axis
		fOriginalExtentDists[4] = fZDiff; //line along z axis
		fOriginalExtentDists[5] = sqrt( fXSqr + fZSqr ); //diagonal perpendicular to y-axis
		fOriginalExtentDists[6] = sqrt( fYSqr + fZSqr ); //diagonal perpendicular to x-axis
		fOriginalExtentDists[7] = sqrt( fXSqr + fYSqr + fZSqr ); //diagonal on all axes
	}

	Vector ptExtents[8]; //ordering is going to be like 3 bits, where 0 is a min on the related axis, and 1 is a max on the same axis, axis order x y z
	float fExtentsValidation[8]; //some points are more valid than others, and this is our measure

	vCenter.z += 0.001f; //to satisfy m_IsSwept on first pass
	
	unsigned int iFailCount;
	for( iFailCount = 0; iFailCount != iIterations; ++iFailCount )
	{
		//float fXDistribution[2] = { -vCurrentExtents.x, vCurrentExtents.x };
		//float fYDistribution[3] = { -vCurrentExtents.y, 0.0f, vCurrentExtents.y };
		//float fZDistribution[5] = { -vCurrentExtents.z, 0.0f, 0.0f, 0.0f, vCurrentExtents.z };

		//hey look, they can overlap
		float fExtentDistribution[6];
		fExtentDistribution[ 0 ] = vCenter.z + ( ( ( nAxisRestrictionFlags & FL_AXIS_DIRECTION_NZ ) == 0 ) ? ( -vCurrentExtents.z ) : ( 0.0f ) );	// Z-
		fExtentDistribution[ 1 ] = vCenter.x + ( ( ( nAxisRestrictionFlags & FL_AXIS_DIRECTION_NX ) == 0 ) ? ( -vCurrentExtents.x ) : ( 0.0f ) );	// X-
		fExtentDistribution[ 2 ] = vCenter.x + ( ( ( nAxisRestrictionFlags & FL_AXIS_DIRECTION_X ) == 0 ) ? ( vCurrentExtents.x ) : ( 0.0f ) );		// X+
		fExtentDistribution[ 3 ] = vCenter.y + ( ( ( nAxisRestrictionFlags & FL_AXIS_DIRECTION_NY ) == 0 ) ? ( -vCurrentExtents.y ) : ( 0.0f ) );	// Y-
		fExtentDistribution[ 4 ] = vCenter.z + ( ( ( nAxisRestrictionFlags & FL_AXIS_DIRECTION_Z ) == 0 ) ? ( vCurrentExtents.z ) : ( 0.0f ) );		// Z+
		fExtentDistribution[ 5 ] = vCenter.y + ( ( ( nAxisRestrictionFlags & FL_AXIS_DIRECTION_Y ) == 0 ) ? ( vCurrentExtents.y ) : ( 0.0f ) );		// Y+

		float *pXDistribution = &fExtentDistribution[1];
		float *pYDistribution = &fExtentDistribution[3];

		bool bExtentInvalid[8];
		float fExtentDists[8];
		bool bAnyInvalid = false;
		for( int i = 0; i != 8; ++i )
		{
			ptExtents[i].x = pXDistribution[i & (1<<0)]; //fExtentDistribution[(0 or 1) + 1]
			ptExtents[i].y = pYDistribution[i & (1<<1)]; //fExtentDistribution[(0 or 2) + 3]
			ptExtents[i].z = fExtentDistribution[i & (1<<2)]; //fExtentDistribution[(0 or 4)]

			fExtentsValidation[i] = 0.0f;
			bExtentInvalid[i] = enginetrace->PointOutsideWorld( ptExtents[i] );
			bAnyInvalid |= bExtentInvalid[i];
			fExtentDists[i] = fOriginalExtentDists[i] * vExtents[iLargestExtent];
		}

		//trace from all extents to all other extents and rate the validity
		{
			unsigned int counters[2]; //I know it's weird, get over it
			for( counters[0] = 0; counters[0] != 7; ++counters[0] )
			{
				for( counters[1] = counters[0] + 1; counters[1] != 8; ++counters[1] )
				{
					for( int i = 0; i != 2; ++i )
					{
						if( bExtentInvalid[counters[i]] )
						{
							traces[i].startsolid = true;
							traces[i].fraction = 0.0f;
						}
						else
						{
							testRay.m_Start = ptExtents[counters[i]];
							testRay.m_Delta = ptExtents[counters[1-i]] - ptExtents[counters[i]];
							enginetrace->TraceRay( testRay, fMask, pTraceFilter, &traces[i] );
						}
					}

					float fDistance = fExtentDists[counters[0] ^ counters[1]];

					for( int i = 0; i != 2; ++i )
					{
						if( (traces[i].fraction == 1.0f) && (traces[1-i].fraction != 1.0f) )
						{
							//One sided collision >_<
							traces[i].startsolid = true;
							traces[i].fraction = 0.0f;
							break;
						}
					}

					for( int i = 0; i != 2; ++i )
					{
						if( traces[i].startsolid )
						{
							bExtentInvalid[counters[i]] = true;
							bAnyInvalid = true;
						}
						else
						{
							fExtentsValidation[counters[i]] += traces[i].fraction * fDistance;
						}
					}
				}
			}
		}

		//optimally we should do this check before tracing extents. But one sided collision is a bitch
		if( !bAnyInvalid )
		{
			//try to trace back to the starting position (if we start in valid, the endpoint will be closer to the original center)
			entRay.m_Start = vCenter;
			entRay.m_Delta = vOriginalCenter - vCenter;

			enginetrace->TraceRay( entRay, fMask, pTraceFilter, &traces[0] );
			if( traces[0].startsolid == false )
			{
				//damned one sided collision
				vCenterOut = traces[0].endpos;
				return true; //current placement worked
			}
		}

		//find the direction to move based on the extent validity
		{
			Vector vNewOriginDirection( 0.0f, 0.0f, 0.0f );
			float fTotalValidation = 0.0f;
			for( int i = 0; i != 8; ++i )
			{
				if( !bExtentInvalid[i] )
				{
					vNewOriginDirection += (ptExtents[i] - vCenter) * fExtentsValidation[i];
					fTotalValidation += fExtentsValidation[i];
				}
			}

			if( fTotalValidation != 0.0f )
			{
				vCenter += (vNewOriginDirection / fTotalValidation);

				//increase sizing
				testRay.m_Extents += vGrowSize; //increase the ray size
				vCurrentExtents -= vGrowSize; //while reducing the overall test region size (so outermost ray extents are the same)
			}
			else
			{
				//no point was valid, apply the indecisive vector
				vCenter += vIndecisivePush;

				//reset sizing
				testRay.m_Extents = vGrowSize;
				vCurrentExtents = vOriginalExtents - vGrowSize;
			}
		}
	}

	//Warning( "FindClosestPassableSpace() failure.\n" );

	// X360TBD: Hits in portal devtest
	//AssertMsg( IsX360() || iFailCount != iIterations, "FindClosestPassableSpace() failure." );
	vCenterOut = vOriginalCenter;
	return false;
}

bool UTIL_FindClosestPassableSpace( CBaseEntity *pEntity, const Vector &vIndecisivePush, unsigned int fMask, unsigned int iIterations, Vector &vOriginOut, Vector *pStartingPosition, int nAxisRestrictionFlags ) //assumes the object is already in a mostly passable space
{
	// Don't ever do this to entities with a move parent
	if ( pEntity->GetMoveParent() )
	{
		vOriginOut = pEntity->GetAbsOrigin();
		return false;
	}

	Vector vEntityMaxs;
	Vector vEntityMins;
	pEntity->CollisionProp()->WorldSpaceAABB( &vEntityMins, &vEntityMaxs );

	Vector ptEntityCenter = ((vEntityMins + vEntityMaxs) / 2.0f);
	//vEntityMins -= ptEntityCenter;
	vEntityMaxs -= ptEntityCenter;
	
	Vector vCenterToOrigin = pEntity->GetAbsOrigin() - ptEntityCenter;
	if( pStartingPosition != NULL )
	{
		Vector vOriginOffset = (*pStartingPosition) - pEntity->GetAbsOrigin();
		ptEntityCenter += vOriginOffset;
	}

	CTraceFilterSimple traceFilter( pEntity, pEntity->GetCollisionGroup() );

	Vector vResult;
	bool bSuccess = UTIL_FindClosestPassableSpace( ptEntityCenter, vEntityMaxs, vIndecisivePush, &traceFilter, fMask, iIterations, vResult, nAxisRestrictionFlags );
	vOriginOut = vResult + vCenterToOrigin;
	return bSuccess;
}


bool UTIL_FindClosestPassableSpace( CBaseEntity *pEntity, const Vector &vIndecisivePush, unsigned int fMask, Vector *pStartingPosition, int nAxisRestrictionFlags )
{
	Vector vNewPos;
	bool bWorked = UTIL_FindClosestPassableSpace( pEntity, vIndecisivePush, fMask, 100, vNewPos, pStartingPosition, nAxisRestrictionFlags );
	if( bWorked )
	{
#ifdef CLIENT_DLL
		pEntity->SetAbsOrigin( vNewPos );
#else
		pEntity->Teleport( &vNewPos, NULL, NULL );
#endif
	}
	return bWorked;
}

//-----------------------------------------------------------------------------
// Purpose: Retrieves the MOD directory for the active game (ie. "hl2")
//-----------------------------------------------------------------------------

bool UTIL_GetModDir( char *lpszTextOut, unsigned int nSize )
{
	// Must pass in a buffer at least large enough to hold the desired string
	const char *pGameDir = CommandLine()->ParmValue( "-game", "hl2" );
	Assert( strlen(pGameDir) <= nSize );
	if ( strlen(pGameDir) > nSize )
		return false;

	Q_strncpy( lpszTextOut, pGameDir, nSize );
	if ( Q_strnchr( lpszTextOut, '/', nSize ) || Q_strnchr( lpszTextOut, '\\', nSize ) )
	{
		// Strip the last directory off (which will be our game dir)
		Q_StripLastDir( lpszTextOut, nSize );

		// Find the difference in string lengths and take that difference from the original string as the mod dir
		int dirlen = Q_strlen( lpszTextOut );
		Q_strncpy( lpszTextOut, pGameDir + dirlen, Q_strlen( pGameDir ) - dirlen + 1 );
	}

	return true;
}

//#define FRUSTUM_DEBUGGING //for dumping some clipping information in UTIL_CalcFrustumThroughConvexPolygon

int UTIL_CalcFrustumThroughConvexPolygon( const Vector *pPolyVertices, int iPolyVertCount, const Vector &vFrustumOrigin, const VPlane *pInputFrustumPlanes, int iInputFrustumPlanes, VPlane *pOutputFrustumPlanes, int iMaxOutputPlanes, int iPreserveCount )
{
	Assert( iPreserveCount <= iMaxOutputPlanes );
	Assert( iPreserveCount <= iInputFrustumPlanes );
	if( iPolyVertCount < 3 )
		return 0;

#if defined( FRUSTUM_DEBUGGING )
	//put case-specific debug logic here and it will propogate
	const bool bDebugThisCall = (iInputFrustumPlanes > 0);
	const float fDisplayTime = 0.0f;
#endif

	int iMaxComplexity = iMaxOutputPlanes - iPreserveCount;
	
	Vector *pClippedVerts;
	int iClippedVertCount;
	if( iInputFrustumPlanes > 0 )
	{
		//clip the polygon by the input frustum
		int iAllocSize = iPolyVertCount + iInputFrustumPlanes;

		Vector *pWorkVerts[2];
		pWorkVerts[0] = (Vector *)stackalloc( sizeof( Vector ) * iAllocSize * 2 ); //possible to add 1 point per cut, iPolyVertCount starting points, iInputFrustumPlaneCount cuts
		pWorkVerts[1] = pWorkVerts[0] + iAllocSize;

		//clip by first plane and put output into pInVerts
		iClippedVertCount = ClipPolyToPlane( (Vector *)pPolyVertices, iPolyVertCount, pWorkVerts[0], pInputFrustumPlanes[0].m_Normal, pInputFrustumPlanes[0].m_Dist, 0.01f );

		//clip by other planes and flipflop in and out pointers
		for( int i = 1; i != iInputFrustumPlanes; ++i )
		{
			if( iClippedVertCount < 3 )
				return 0; //nothing left in the frustum

			iClippedVertCount = ClipPolyToPlane( pWorkVerts[(i & 1) ^ 1], iClippedVertCount, pWorkVerts[i & 1], pInputFrustumPlanes[i].m_Normal, pInputFrustumPlanes[i].m_Dist, 0.01f );
		}

		if( iClippedVertCount < 3 )
			return false; //nothing left in the frustum

		pClippedVerts = pWorkVerts[(iInputFrustumPlanes & 1) ^ 1];
	}
	else
	{
		//no input frustum
		if( iPolyVertCount > iMaxComplexity )
		{
			//we'll need to reduce our output frustum, copy the input polygon
			pClippedVerts = (Vector *)stackalloc( sizeof( Vector ) * iPolyVertCount );
			memcpy( pClippedVerts, pPolyVertices, sizeof( Vector ) * iPolyVertCount );
		}
		else
		{
			//we won't need to simplify the polygon to reduce output planes, just point at the input polygon
			pClippedVerts = (Vector *)pPolyVertices;
		}
		iClippedVertCount = iPolyVertCount;
	}

#if defined( FRUSTUM_DEBUGGING ) //for visibility culling debugging
	if( bDebugThisCall )
	{
		NDebugOverlay::Line( pClippedVerts[iClippedVertCount - 1], pClippedVerts[0], 255, 0, 0, true, fDisplayTime );
		for( int j = 0; j != iClippedVertCount - 1; ++j )
		{
			NDebugOverlay::Line( pClippedVerts[j], pClippedVerts[j+1], 255, 0, 0, true, fDisplayTime );
		}
	}
#endif

	Assert( iClippedVertCount <= (iPolyVertCount + iInputFrustumPlanes) );

	if( iClippedVertCount > iMaxComplexity )
	{
#if defined( FRUSTUM_DEBUGGING )
		if( bDebugThisCall )
		{
			NDebugOverlay::Line( pClippedVerts[iClippedVertCount - 1], pClippedVerts[0], 0, 255, 0, false, fDisplayTime );
			for( int j = 0; j != iClippedVertCount - 1; ++j )
			{
				NDebugOverlay::Line( pClippedVerts[j], pClippedVerts[j+1], 0, 255, 0, true, fDisplayTime );
			}
		}
#endif
		float *fLineLengthSqr = (float *)stackalloc( sizeof( float ) * iClippedVertCount );

		for( int i = 0; i != (iClippedVertCount - 1); ++i )
		{
			fLineLengthSqr[i] = (pClippedVerts[i + 1] - pClippedVerts[i]).LengthSqr();
		}
		fLineLengthSqr[(iClippedVertCount - 1)] = (pClippedVerts[0] - pClippedVerts[(iClippedVertCount - 1)]).LengthSqr(); //wrap around


#if defined( FRUSTUM_DEBUGGING ) //for visibility culling debugging
		Vector vDebugBoxExtent;
		vDebugBoxExtent.Init( 1.0f, 1.0f, 1.0f );
#endif
		while( iClippedVertCount > iMaxComplexity ) //vert count == number of planes we need to bound the polygon
		{
			//we have too many verts to represent this accurately in the output frustum plane count
			//so, we're going to eliminate the smallest sides one at a time and bridge the surrounding sides until we're down to iMaxComplexity
			float fMinSide = fLineLengthSqr[0];
			int iMinSideFirstPoint = 0;
			int iOldVertCount = iClippedVertCount;
			--iClippedVertCount; //we're going to decrement this sometime in this block, it makes math easier to do it now

			for( int i = 1; i != iOldVertCount; ++i )
			{
				if( fLineLengthSqr[i] < fMinSide )
				{
					fMinSide = fLineLengthSqr[i];
					iMinSideFirstPoint = i;
				}
			}

			int i1, i2, i3, i4;
			i1 = (iMinSideFirstPoint + iClippedVertCount)%(iOldVertCount); //-1 with a wrap
			i2 = iMinSideFirstPoint;
			i3 = (iMinSideFirstPoint + 1)%(iOldVertCount);
			i4 = (iMinSideFirstPoint + 2)%(iOldVertCount);

			Vector *p1, *p2, *p3, *p4;
			p1 = &pClippedVerts[i1];
			p2 = &pClippedVerts[i2];
			p3 = &pClippedVerts[i3]; //this is the one we'll actually be dropping in the merge
			p4 = &pClippedVerts[i4];


			//now we know the two points that we have to merge to one, project and make a merged point from the surrounding lines
			//if( fMinSide >= 0.1f ) //only worth doing the math if it's actually going to be accurate and make a difference
			{
				//http://mathworld.wolfram.com/Line-LineIntersection.html (20)
				Vector vA = *p2 - *p1;
				Vector vB = *p4 - *p3;
				Vector vC = *p3 - *p1;
				Vector vCxB = vC.Cross( vB );
				Vector vAxB = vA.Cross( vB );
				float fS = vCxB.Dot(vAxB)/vAxB.LengthSqr();

				*p2 = *p1 + (vA * fS);

				fLineLengthSqr[i1] = (*p2 - *p1).LengthSqr();
			}
			
			fLineLengthSqr[i2] = (*p4 - *p2).LengthSqr(); //must do this BEFORE possibly shifting points p4+ left

			if( i3 < i4 ) //not the last point in the array
			{
				int iElementShift = (iOldVertCount - i4);

				//eliminate p3, we merged p2+p3 and already stored the result in p2
				memmove( p3, p4, sizeof( Vector ) * iElementShift );
				memmove( &fLineLengthSqr[i3], &fLineLengthSqr[i4], sizeof( float ) * iElementShift );
			}
		}

#if defined(FRUSTUM_DEBUGGING) //for visibility culling debugging
		if( bDebugThisCall )
		{
			NDebugOverlay::Line( pClippedVerts[iClippedVertCount - 1], pClippedVerts[0], 0, 0, 255, false, fDisplayTime );
			for( int j = 0; j != iClippedVertCount - 1; ++j )
			{
				NDebugOverlay::Line( pClippedVerts[j], pClippedVerts[j+1], 0, 0, 255, true, fDisplayTime );
			}
		}
#endif
	}

	//generate planes defined by each line around the convex and the frustum origin
	{
		int iFlipNormalsXOR = 0; //this algorithm was written assuming polygon vertices would be in a clockwise order from the perspective of vFrustumOrigin, some logic needs to flip if the inverse is true
		{
			Vector vLine1 = pPolyVertices[1] - pPolyVertices[0];
			Vector vLine2 = pPolyVertices[2] - pPolyVertices[1];
			Vector vFrontFace = vLine2.Cross( vLine1 );

			iFlipNormalsXOR = (vFrontFace.Dot( vFrustumOrigin - pPolyVertices[0] ) < 0.0f) ? 1 : 0; //this will assist in reversing the normal by flipping the cross product
		}		

		Vector vTemp[2];
		vTemp[0] = pClippedVerts[iClippedVertCount - 1] - vFrustumOrigin;
		for( int i = 0; i != iClippedVertCount; ++i )
		{
			int iIndexing = i & 1; //we can carry over the line computation from one iteration to the next, flip which order we look at the temps with
			vTemp[iIndexing ^ 1] = pClippedVerts[i] - vFrustumOrigin; 

			Vector vNormal = vTemp[iIndexing ^ iFlipNormalsXOR].Cross( vTemp[(iIndexing ^ iFlipNormalsXOR) ^ 1] ); //vLine1.Cross( vLine2 );
			vNormal.NormalizeInPlace();

			pOutputFrustumPlanes[i].Init( vNormal, vNormal.Dot( vFrustumOrigin ) );
		}
	}

	//preserve input planes on request
	if( iPreserveCount > 0 )
	{
		memcpy( &pOutputFrustumPlanes[iClippedVertCount], &pInputFrustumPlanes[iInputFrustumPlanes - iPreserveCount], sizeof( VPlane ) * iPreserveCount );
	}

	return (iClippedVertCount + iPreserveCount);
}



//-----------------------------------------------------------------------------
// class CFlaggedEntitiesEnum
//-----------------------------------------------------------------------------

CFlaggedEntitiesEnum::CFlaggedEntitiesEnum( CBaseEntity **pList, int listMax, int flagMask )
{
	m_pList = pList;
	m_listMax = listMax;
	m_flagMask = flagMask;
	m_count = 0;
}

bool CFlaggedEntitiesEnum::AddToList( CBaseEntity *pEntity )
{
	if ( m_count >= m_listMax )
	{
		AssertMsgOnce( 0, "reached enumerated list limit.  Increase limit, decrease radius, or make it so entity flags will work for you" );
		return false;
	}
	m_pList[m_count] = pEntity;
	m_count++;
	return true;
}

IterationRetval_t CFlaggedEntitiesEnum::EnumElement( IHandleEntity *pHandleEntity )
{
#if defined( CLIENT_DLL )
	IClientEntity *pClientEntity = cl_entitylist->GetClientEntityFromHandle( pHandleEntity->GetRefEHandle() );
	C_BaseEntity *pEntity = pClientEntity ? pClientEntity->GetBaseEntity() : NULL;
#else
	CBaseEntity *pEntity = gEntList.GetBaseEntity( pHandleEntity->GetRefEHandle() );
#endif
	if ( pEntity )
	{
		if ( m_flagMask && !(pEntity->GetFlags() & m_flagMask) )	// Does it meet the criteria?
			return ITERATION_CONTINUE;

		if ( !AddToList( pEntity ) )
			return ITERATION_STOP;
	}

	return ITERATION_CONTINUE;
}


//-----------------------------------------------------------------------------
// class CHurtableEntitiesEnum
//-----------------------------------------------------------------------------

CHurtableEntitiesEnum::CHurtableEntitiesEnum( CBaseEntity **pList, int listMax )
{
	m_pList = pList;
	m_listMax = listMax;
	m_count = 0;
}

bool CHurtableEntitiesEnum::AddToList( CBaseEntity *pEntity )
{
	if ( m_count >= m_listMax )
	{
		AssertMsgOnce( 0, "reached enumerated list limit.  Increase limit, decrease radius, or make it so entity flags will work for you" );
		return false;
	}
	m_pList[m_count] = pEntity;
	m_count++;
	return true;
}

IterationRetval_t CHurtableEntitiesEnum::EnumElement( IHandleEntity *pHandleEntity )
{
#if defined( CLIENT_DLL )
	IClientEntity *pClientEntity = cl_entitylist->GetClientEntityFromHandle( pHandleEntity->GetRefEHandle() );
	C_BaseEntity *pEntity = pClientEntity ? pClientEntity->GetBaseEntity() : NULL;
#else
	CBaseEntity *pEntity = gEntList.GetBaseEntity( pHandleEntity->GetRefEHandle() );
#endif
	if ( pEntity )
	{
		if ( ( pEntity->m_takedamage == DAMAGE_NO || pEntity->GetHealth() <= 0 ) && pEntity->GetMoveType() != MOVETYPE_VPHYSICS )	// Does it meet the criteria?
			return ITERATION_CONTINUE;

		if ( !AddToList( pEntity ) )
			return ITERATION_STOP;
	}

	return ITERATION_CONTINUE;
}



int UTIL_EntitiesAlongRay( const Ray_t &ray, CFlaggedEntitiesEnum *pEnum )
{
#if defined( CLIENT_DLL )
	partition->EnumerateElementsAlongRay( PARTITION_CLIENT_NON_STATIC_EDICTS, ray, false, pEnum );
#else
	partition->EnumerateElementsAlongRay( PARTITION_ENGINE_NON_STATIC_EDICTS, ray, false, pEnum );
#endif
	return pEnum->GetCount();
}
