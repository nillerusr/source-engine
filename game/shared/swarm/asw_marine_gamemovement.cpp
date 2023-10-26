#include "cbase.h"
#ifdef CLIENT_DLL
	#include "c_asw_player.h"
	#include "c_asw_marine.h"
	#include "c_asw_weapon.h"
	#include "prediction.h"
	#define CASW_Player C_ASW_Player
	#define CASW_Marine C_ASW_Marine
	#define CASW_Weapon C_ASW_Weapon
#else
	#include "asw_marine.h"
	#include "asw_player.h"
	#include "asw_weapon.h"
	#include "asw_alien.h"
	#include "takedamageinfo.h"
	#include "ndebugoverlay.h"
	#include "world.h"
#endif
#include "asw_util_shared.h"
#include "asw_marine_profile.h"
#include "asw_marine_gamemovement.h"
#include "in_buttons.h"
#include <stdarg.h>
#include "movevars_shared.h"
#include "engine/IEngineTrace.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "decals.h"
#include "asw_shareddefs.h"
#include "coordsize.h"
#include "asw_melee_system.h"
#include "asw_movedata.h"
#include "asw_marine_skills.h"
#include "asw_gamerules.h"
#include "particle_parse.h"
#include "asw_movedata.h"
#ifdef GAME_DLL
#include "te_effect_dispatch.h"
#else
#include "c_te_effect_dispatch.h"
#endif


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// this code is based on gamemovement.cpp/.h, but is altered to drive a marine AI

#define	STOP_EPSILON		0.1
#define	MAX_CLIP_PLANES		5

#define ASW_JUMP_LATERAL_SCALE 0.85f
#define ASW_MIN_ACCELERATION_SPEED 210.0f

#define ASW_NO_WATER_JUMP 1			// stop us bouncing out of the waist high water in sewers all the time

#include "filesystem.h"
#include <stdarg.h>

extern IFileSystem *filesystem;

#ifndef CLIENT_DLL
	#include "env_player_surface_trigger.h"
	static ConVar marine_dispcoll_drawplane( "marine_dispcoll_drawplane", "0" );
#endif

static ConVar asw_marine_gravity( "asw_marine_gravity","800", FCVAR_NOTIFY | FCVAR_REPLICATED, "Marine gravity." );
static ConVar asw_marine_friction( "asw_marine_friction","10", FCVAR_NOTIFY | FCVAR_REPLICATED, "Marine movement friction." );
static ConVar asw_sv_maxspeed( "asw_sv_maxspeed", "500", FCVAR_NOTIFY | FCVAR_REPLICATED);
static ConVar asw_debug_steps("asw_debug_steps", "0", FCVAR_CHEAT | FCVAR_REPLICATED, "Gives debug info on moving up/down steps");
static ConVar asw_debug_air_move("asw_debug_air_move", "0", FCVAR_CHEAT | FCVAR_REPLICATED, "Gives debug info on air moving");
extern ConVar asw_controls;

// tickcount currently isn't set during prediction, although gpGlobals->curtime and
// gpGlobals->frametime are. We should probably set tickcount (to player->m_nTickBase),
// but we're REALLY close to shipping, so we can change that later and people can use
// player->CurrentCommandNumber() in the meantime.
#define tickcount USE_PLAYER_CURRENT_COMMAND_NUMBER__INSTEAD_OF_TICKCOUNT

extern void DiffPrint( bool bServer, int nCommandNumber, char const *fmt, ... );

// [MD] I'll remove this eventually. For now, I want the ability to A/B the optimizations.
extern bool g_bMovementOptimizations;

// Roughly how often we want to update the info about the ground surface we're on.
// We don't need to do this very often.
#define CATEGORIZE_GROUND_SURFACE_INTERVAL			0.3f
#define CATEGORIZE_GROUND_SURFACE_TICK_INTERVAL   ( (int)( CATEGORIZE_GROUND_SURFACE_INTERVAL / TICK_INTERVAL ) )

#define CHECK_STUCK_INTERVAL			0.4f
#define CHECK_STUCK_TICK_INTERVAL		( (int)( CHECK_STUCK_INTERVAL / TICK_INTERVAL ) )

#define CHECK_LADDER_INTERVAL			0.2f
#define CHECK_LADDER_TICK_INTERVAL		( (int)( CHECK_LADDER_INTERVAL / TICK_INTERVAL ) )

extern void COM_Log( char *pszFile, char *fmt, ...);

CASW_MarineGameMovement* g_pASWGameMovement = NULL;

CASW_MarineGameMovement* ASWGameMovement() { return g_pASWGameMovement; }

#ifndef CLIENT_DLL
extern ConVar asw_debug_marine_damage;
extern ConVar asw_marine_fall_damage;
//-----------------------------------------------------------------------------
// Purpose: Debug - draw the displacement collision plane.
//-----------------------------------------------------------------------------
extern void DrawDispCollPlane( CBaseTrace *pTrace );

#endif

//-----------------------------------------------------------------------------
// Traces marine movement + position
//-----------------------------------------------------------------------------
inline void CASW_MarineGameMovement::TraceMarineBBox( const Vector& start, const Vector& end, unsigned int fMask, int collisionGroup, trace_t& pm )
{
	++m_nTraceCount;
	VPROF( "CASW_MarineGameMovement::TraceMarineBBox" );

	Ray_t ray;
	ray.Init( start, end, GetPlayerMins(), GetPlayerMaxs() );
	ITraceFilter *pFilter = LockTraceFilter( collisionGroup );
	if ( m_pTraceListData && m_pTraceListData->CanTraceRay(ray) )
	{
		enginetrace->TraceRayAgainstLeafAndEntityList( ray, m_pTraceListData, fMask, pFilter, &pm );
	}
	else
	{
		enginetrace->TraceRay( ray, fMask, pFilter, &pm );
	}
	UnlockTraceFilter( pFilter );
}

static inline void DoMarineTrace( ITraceListData *pTraceListData, const Ray_t &ray, uint32 fMask, ITraceFilter *filter, trace_t *ptr, int *counter )
{
	++*counter;

	if ( pTraceListData && pTraceListData->CanTraceRay(ray) )
	{
		enginetrace->TraceRayAgainstLeafAndEntityList( ray, pTraceListData, fMask, filter, ptr );
	}
	else
	{
		enginetrace->TraceRay( ray, fMask, filter, ptr );
	}
}

//-----------------------------------------------------------------------------
// Traces the player's collision bounds in quadrants, looking for a plane that
// can be stood upon (normal's z >= flStandableZ).  Regardless of success or failure,
// replace the fraction and endpos with the original ones, so we don't try to
// move the player down to the new floor and get stuck on a leaning wall that
// the original trace hit first.
//-----------------------------------------------------------------------------
void TraceMarineBBoxForGround( ITraceListData *pTraceListData, const Vector& start, const Vector& end, const Vector& minsSrc,
							  const Vector& maxsSrc, unsigned int fMask,
							  ITraceFilter *filter, trace_t& pm, float minGroundNormalZ, bool overwriteEndpos, int *pCounter )
{
	VPROF( "TraceMarineBBoxForGround" );

	Ray_t ray;
	Vector mins, maxs;

	float fraction = pm.fraction;
	Vector endpos = pm.endpos;

	// Check the -x, -y quadrant
	mins = minsSrc;
	maxs.Init( MIN( 0, maxsSrc.x ), MIN( 0, maxsSrc.y ), maxsSrc.z );
	ray.Init( start, end, mins, maxs );
	DoMarineTrace( pTraceListData, ray, fMask, filter, &pm, pCounter );
	if ( pm.m_pEnt && pm.plane.normal[2] >= minGroundNormalZ)
	{
		if ( overwriteEndpos )
		{
			pm.fraction = fraction;
			pm.endpos = endpos;
		}
		return;
	}

	// Check the +x, +y quadrant
	mins.Init( MAX( 0, minsSrc.x ), MAX( 0, minsSrc.y ), minsSrc.z );
	maxs = maxsSrc;
	ray.Init( start, end, mins, maxs );
	DoMarineTrace( pTraceListData, ray, fMask, filter, &pm, pCounter );
	if ( pm.m_pEnt && pm.plane.normal[2] >= minGroundNormalZ)
	{
		if ( overwriteEndpos )
		{
			pm.fraction = fraction;
			pm.endpos = endpos;
		}
		return;
	}

	// Check the -x, +y quadrant
	mins.Init( minsSrc.x, MAX( 0, minsSrc.y ), minsSrc.z );
	maxs.Init( MIN( 0, maxsSrc.x ), maxsSrc.y, maxsSrc.z );
	ray.Init( start, end, mins, maxs );
	DoMarineTrace( pTraceListData, ray, fMask, filter, &pm, pCounter );
	if ( pm.m_pEnt && pm.plane.normal[2] >= 0.7)
	{
		if ( overwriteEndpos )
		{
			pm.fraction = fraction;
			pm.endpos = endpos;
		}
		return;
	}

	// Check the +x, -y quadrant
	mins.Init( MAX( 0, minsSrc.x ), minsSrc.y, minsSrc.z );
	maxs.Init( maxsSrc.x, MIN( 0, maxsSrc.y ), maxsSrc.z );
	ray.Init( start, end, mins, maxs );
	DoMarineTrace( pTraceListData, ray, fMask, filter, &pm, pCounter );
	if ( pm.m_pEnt && pm.plane.normal[2] >= minGroundNormalZ)
	{
		if ( overwriteEndpos )
		{
			pm.fraction = fraction;
			pm.endpos = endpos;
		}
		return;
	}

	if ( overwriteEndpos )
	{
		pm.fraction = fraction;
		pm.endpos = endpos;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Constructs GameMovement interface
//-----------------------------------------------------------------------------
CASW_MarineGameMovement::CASW_MarineGameMovement( void )
{
	m_nOldWaterLevel	= WL_NotInWater;
	m_nOnLadder			= 0;

	mv					= NULL;

	g_pASWGameMovement = this;
	m_pTraceListData = NULL;
	
	memset( m_flStuckCheckTime, 0, sizeof(m_flStuckCheckTime) );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CASW_MarineGameMovement::~CASW_MarineGameMovement( void )
{
	g_pASWGameMovement = NULL;
	if ( enginetrace )
	{
		enginetrace->FreeTraceListData(m_pTraceListData);
	}
}

//--------------------------------------------------------------------------------------------------------
enum
{
	MAX_NESTING = 8
};

static CTraceFilterSkipTwoEntities s_TraceFilter[MAX_NESTING];
static int s_nTraceFilterCount = 0;

ITraceFilter *CASW_MarineGameMovement::LockTraceFilter( int collisionGroup )
{
	// If this assertion triggers, you forgot to call UnlockTraceFilter
	Assert( s_nTraceFilterCount < MAX_NESTING );
	if ( s_nTraceFilterCount >= MAX_NESTING )
		return NULL;

	CTraceFilterSkipTwoEntities *pFilter = &s_TraceFilter[s_nTraceFilterCount++];
	pFilter->SetPassEntity( marine );
	pFilter->SetCollisionGroup( collisionGroup );

	return pFilter;
}

void CASW_MarineGameMovement::UnlockTraceFilter( ITraceFilter *&pFilter )
{
	Assert( s_nTraceFilterCount > 0 );
	--s_nTraceFilterCount;
	Assert( &s_TraceFilter[s_nTraceFilterCount] == pFilter );
	pFilter = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : type - 
// Output : int
//-----------------------------------------------------------------------------
int CASW_MarineGameMovement::GetCheckInterval( IntervalType_t type )
{
	int tickInterval = 1;
	switch ( type )
	{
	default:
		tickInterval = 1;
		break;
	case GROUND:
		tickInterval = CATEGORIZE_GROUND_SURFACE_TICK_INTERVAL;
		break;
	case STUCK:
		tickInterval = CHECK_STUCK_TICK_INTERVAL;
		break;
	case LADDER:
		tickInterval = CHECK_LADDER_TICK_INTERVAL;
		break;
	}
	return tickInterval;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : type - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CASW_MarineGameMovement::CheckInterval( IntervalType_t type )
{
	int tickInterval = GetCheckInterval( type );

	if ( g_bMovementOptimizations )
	{
		return (player->CurrentCommandNumber() + player->entindex()) % tickInterval == 0;
	}
	else
	{
		return true;
	}
}
	

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : ducked - 
// Output : const Vector
//-----------------------------------------------------------------------------
const Vector& CASW_MarineGameMovement::GetPlayerMins( bool ducked ) const
{
	return marine->CollisionProp()->OBBMins();
	//return ducked ? VEC_DUCK_HULL_MIN : VEC_HULL_MIN;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : ducked - 
// Output : const Vector
//-----------------------------------------------------------------------------
const Vector& CASW_MarineGameMovement::GetPlayerMaxs( bool ducked ) const
{	
	return marine->CollisionProp()->OBBMaxs();
	//return ducked ? VEC_DUCK_HULL_MAX : VEC_HULL_MAX;
	// TODO: Remove all uses of VEC_HULL_MAX and other defines
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : const Vector
//-----------------------------------------------------------------------------
const Vector& CASW_MarineGameMovement::GetPlayerMins( void ) const
{
	return marine->CollisionProp()->OBBMins();
	//return VEC_HULL_MIN;
	
	//if ( player->IsObserver() )
	//{
		//return VEC_OBS_HULL_MIN;	
	//}
	//else
	//{
		//return player->m_Local.m_bDucked  ? VEC_DUCK_HULL_MIN : VEC_HULL_MIN;
	//}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : const Vector
//-----------------------------------------------------------------------------
const Vector& CASW_MarineGameMovement::GetPlayerMaxs( void ) const
{	
	return marine->CollisionProp()->OBBMaxs();
	//return VEC_HULL_MAX;
	//if ( player->IsObserver() )
	//{
		//return VEC_OBS_HULL_MAX;	
	//}
	//else
	//{
		//return player->m_Local.m_bDucked  ? VEC_DUCK_HULL_MAX : VEC_HULL_MAX;
	//}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : ducked - 
// Output : const Vector
//-----------------------------------------------------------------------------
const Vector& CASW_MarineGameMovement::GetPlayerViewOffset( bool ducked ) const
{
	return ducked ? VEC_DUCK_VIEW : VEC_VIEW;
}

#if 0
//-----------------------------------------------------------------------------
// Traces player movement + position
//-----------------------------------------------------------------------------
inline void CASW_MarineGameMovement::TraceMarineBBox( const Vector& start, const Vector& end, unsigned int fMask, int collisionGroup, trace_t& pm )
{
	VPROF( "CASW_MarineGameMovement::TraceMarineBBox" );

	Ray_t ray;
	ray.Init( start, end, GetPlayerMins(), GetPlayerMaxs() );
	UTIL_TraceRay( ray, fMask, marine, collisionGroup, &pm );
}
#endif

inline CBaseHandle CASW_MarineGameMovement::TestPlayerPosition( const Vector& pos, int collisionGroup, trace_t& pm )
{
	Ray_t ray;
	ray.Init( pos, pos, GetPlayerMins(), GetPlayerMaxs() );
	UTIL_TraceRay( ray, MASK_PLAYERSOLID, marine, collisionGroup, &pm );
#ifdef GAME_DLL

	if ( asw_debug_steps.GetInt() == 2 )
	{
		NDebugOverlay::Box( pos, GetPlayerMins(), GetPlayerMaxs(), 100, 255, 100, 255, 0.1f );
		engine->Con_NPrintf( 10, "%s TestPlayerPosition hit %i/%s", 
			IsServerDll() ? "server" : "client",
			pm.m_pEnt ? pm.m_pEnt->entindex() : 0, pm.m_pEnt ? pm.m_pEnt->GetDebugName() : "NULL" );
		engine->Con_NPrintf( 11, "testpos %f %f %f", 
			VectorExpand( pos ) );
	}

#endif
	if ( (pm.contents & MASK_PLAYERSOLID) && pm.m_pEnt )
	{				
		return pm.m_pEnt->GetRefEHandle();
	}
	else
	{	
		return INVALID_EHANDLE_INDEX;
	}
}

/*
// FIXME FIXME:  Does this need to be hooked up?
bool CASW_MarineGameMovement::IsWet() const
{
	return ((pev->flags & FL_INRAIN) != 0) || (m_WetTime >= gpGlobals->time);
}
*/
//-----------------------------------------------------------------------------
// Plants player footprint decals
//-----------------------------------------------------------------------------
/*
#define PLAYER_HALFWIDTH 12
void CASW_MarineGameMovement::PlantFootprint( surfacedata_t *psurface )
{
	int footprintDecal = -1;

	// Figure out which footprint type to plant...
	// Use the wet footprint if we're wet...
	//if (IsWet())
	//{
		footprintDecal = 1; //DECAL_FOOTPRINT_WET;
	//}
	//else
	//{	   
		// FIXME: Activate this once we decide to pull the trigger on it.
		// NOTE: We could add in snow, mud, others here
//			switch(psurface->gameMaterial)
//			{
//			case 'D':
//				footprintDecal = DECAL_FOOTPRINT_DIRT;
//				break;
//			}
	//}

	if (footprintDecal != -1)
	{
		Vector right;
		AngleVectors( marine->GetAbsAngles(), 0, &right, 0 );

		// Figure out where the top of the stepping leg is 
		trace_t tr;
		Vector hipOrigin;
		VectorMA( marine->GetAbsOrigin(), 
			m_bIsFootprintOnLeft ? -PLAYER_HALFWIDTH : PLAYER_HALFWIDTH,
			right, hipOrigin );

		// Find where that leg hits the ground
		UTIL_TraceLine( hipOrigin, hipOrigin + Vector(0, 0, -COORD_EXTENT * 1.74), 
						MASK_SOLID_BRUSHONLY, marine, COLLISION_GROUP_NONE, &tr);

		//unsigned char mType = TEXTURETYPE_Find( &tr );

		//"swarm/decals/arrowupdecal"
		// Splat a decal
		CPVSFilter filter( tr.endpos );
		te->FootprintDecal( filter, 0.0f, &tr.endpos, &right, tr.GetEntityIndex(), 
						   10, marine->m_chTextureType ); //gDecals[footprintDecal].index
	}

	// Switch feet for next time
	m_bIsFootprintOnLeft = !m_bIsFootprintOnLeft;
}

#define WET_TIME			    5.f	// how many seconds till we're completely wet/dry
#define DRY_TIME			   20.f	// how many seconds till we're completely wet/dry

void CBasePlayer::UpdateWetness()
{
	// BRJ 1/7/01
	// Check for whether we're in a rainy area....
	// Do this by tracing a line straight down with a size guaranteed to
	// be larger than the map
	// Update wetness based on whether we're in rain or not...

	trace_t tr;
	UTIL_TraceLine( pev->origin, pev->origin + Vector(0, 0, -COORD_EXTENT * 1.74), 
					MASK_SOLID_BRUSHONLY, edict(), COLLISION_GROUP_NONE, &tr);
	if (tr.surface.flags & SURF_WET)
	{
		if (! (pev->flags & FL_INRAIN) )
		{
			// Transition...
			// Figure out how wet we are now (we were drying off...)
			float wetness = (m_WetTime - gpGlobals->time) / DRY_TIME;
			if (wetness < 0.0f)
				wetness = 0.0f;

			// Here, wet time represents the time at which we get totally wet
			m_WetTime = gpGlobals->time + (1.0 - wetness) * WET_TIME; 

			pev->flags |= FL_INRAIN;
		}
	}
	else
	{
		if ((pev->flags & FL_INRAIN) != 0)
		{
			// Transition...
			// Figure out how wet we are now (we were getting more wet...)
			float wetness = 1.0f + (gpGlobals->time - m_WetTime) / WET_TIME;
			if (wetness > 1.0f)
				wetness = 1.0f;

			// Here, wet time represents the time at which we get totally dry
			m_WetTime = gpGlobals->time + wetness * DRY_TIME; 

			pev->flags &= ~FL_INRAIN;
		}
	}
}
*/

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_MarineGameMovement::CategorizeGroundSurface( trace_t &pm )
{
	IPhysicsSurfaceProps *physprops = MoveHelper()->GetSurfaceProps();
	marine->m_surfaceProps = pm.surface.surfaceProps;
	marine->m_pSurfaceData = physprops->GetSurfaceData( marine->m_surfaceProps );
	physprops->GetPhysicsProperties( marine->m_surfaceProps, NULL, NULL, &marine->m_surfaceFriction, NULL );

	// HACKHACK: Scale this to fudge the relationship between vphysics friction values and player friction values.
	// A value of 0.8f feels pretty normal for vphysics, whereas 1.0f is normal for players.
	// This scaling trivially makes them equivalent.  REVISIT if this affects low friction surfaces too much.
	marine->m_surfaceFriction *= 1.25f;
	if ( marine->m_surfaceFriction > 1.0f )
		marine->m_surfaceFriction = 1.0f;

	if ( marine->m_pSurfaceData )
	{
		marine->m_chTextureType = marine->m_pSurfaceData->game.material;
	}
/*
	//
	// Find the name of the material that lies beneath the player.
	//
	Vector start, end;
	VectorCopy( mv->GetAbsOrigin() + Vector(0,0,1), start );
	VectorCopy( mv->GetAbsOrigin(), end );

	// Straight down
	end[2] -= 37;

	// Fill in default values, just in case.
	trace_t	trace;
	TraceMarineBBox( start, end, MASK_PLAYERSOLID, COLLISION_GROUP_PLAYER_MOVEMENT, trace ); 
	IPhysicsSurfaceProps *physprops = MoveHelper()->GetSurfaceProps();
	marine->m_surfaceProps = trace.surface.surfaceProps;
	marine->m_pSurfaceData = physprops->GetSurfaceData( marine->m_surfaceProps );
	physprops->GetPhysicsProperties( marine->m_surfaceProps, NULL, NULL, &marine->m_surfaceFriction, NULL );
	
	// HACKHACK: Scale this to fudge the relationship between vphysics friction values and player friction values.
	// A value of 0.8f feels pretty normal for vphysics, whereas 1.0f is normal for players.
	// This scaling trivially makes them equivalent.  REVISIT if this affects low friction surfaces too much.
	marine->m_surfaceFriction *= 1.25f;
	if ( marine->m_surfaceFriction > 1.0f )
		marine->m_surfaceFriction = 1.0f;

	marine->m_chTextureType = marine->m_pSurfaceData->game.material;
	*/
}

bool CASW_MarineGameMovement::IsDead( void ) const
{
	return ( marine->m_iHealth <= 0 ) ? true : false;
}

//-----------------------------------------------------------------------------
// Figures out how the constraint should slow us down
//-----------------------------------------------------------------------------
float CASW_MarineGameMovement::ComputeConstraintSpeedFactor( void )
{
	// If we have a constraint, slow down because of that too.
	if ( !mv || mv->m_flConstraintRadius == 0.0f )
		return 1.0f;

	float flDistSq = mv->GetAbsOrigin().DistToSqr( mv->m_vecConstraintCenter );

	float flOuterRadiusSq = mv->m_flConstraintRadius * mv->m_flConstraintRadius;
	float flInnerRadiusSq = mv->m_flConstraintRadius - mv->m_flConstraintWidth;
	flInnerRadiusSq *= flInnerRadiusSq;

	// Only slow us down if we're inside the constraint ring
	if ((flDistSq <= flInnerRadiusSq) || (flDistSq >= flOuterRadiusSq))
		return 1.0f;

	// Only slow us down if we're running away from the center
	Vector vecDesired;
	VectorMultiply( m_vecForward, mv->m_flForwardMove, vecDesired );
	VectorMA( vecDesired, mv->m_flSideMove, m_vecRight, vecDesired );
	VectorMA( vecDesired, mv->m_flUpMove, m_vecUp, vecDesired );

	Vector vecDelta;
	VectorSubtract( mv->GetAbsOrigin(), mv->m_vecConstraintCenter, vecDelta );
	VectorNormalize( vecDelta );
	VectorNormalize( vecDesired );
	if (DotProduct( vecDelta, vecDesired ) < 0.0f)
		return 1.0f;

	float flFrac = (sqrt(flDistSq) - (mv->m_flConstraintRadius - mv->m_flConstraintWidth)) / mv->m_flConstraintWidth;

	float flSpeedFactor = Lerp( flFrac, 1.0f, mv->m_flConstraintSpeedFactor ); 
	return flSpeedFactor;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_MarineGameMovement::CheckParameters( void )
{
	QAngle	v_angle;

	if ( marine->GetMoveType() != MOVETYPE_ISOMETRIC &&
		 marine->GetMoveType() != MOVETYPE_NOCLIP &&
		 marine->GetMoveType() != MOVETYPE_OBSERVER )
	{
		float spd;
		float maxspeed;

		spd = ( mv->m_flForwardMove * mv->m_flForwardMove ) +
			  ( mv->m_flSideMove * mv->m_flSideMove ) +
			  ( mv->m_flUpMove * mv->m_flUpMove );

		maxspeed = mv->m_flClientMaxSpeed;
		if ( maxspeed != 0.0 )
		{
			mv->m_flMaxSpeed = MIN( maxspeed, mv->m_flMaxSpeed );
		}

		// Slow down by the speed factor
		float flSpeedFactor = 1.0f;
		if (marine->m_pSurfaceData)
		{
			flSpeedFactor = marine->m_pSurfaceData->game.maxSpeedFactor;
		}

		// If we have a constraint, slow down because of that too.
		float flConstraintSpeedFactor = ComputeConstraintSpeedFactor();
		if (flConstraintSpeedFactor < flSpeedFactor)
			flSpeedFactor = flConstraintSpeedFactor;

		mv->m_flMaxSpeed *= flSpeedFactor;

		if ( g_bMovementOptimizations )
		{
			// Same thing but only do the sqrt if we have to.
			if ( ( spd != 0.0 ) && ( spd > mv->m_flMaxSpeed*mv->m_flMaxSpeed ) )
			{
				float fRatio = mv->m_flMaxSpeed / sqrt( spd );
				mv->m_flForwardMove *= fRatio;
				mv->m_flSideMove    *= fRatio;
				mv->m_flUpMove      *= fRatio;
			}
		}
		else
		{
			spd = sqrt( spd );
			if ( ( spd != 0.0 ) && ( spd > mv->m_flMaxSpeed ) )
			{
				float fRatio = mv->m_flMaxSpeed / spd;
				mv->m_flForwardMove *= fRatio;
				mv->m_flSideMove    *= fRatio;
				mv->m_flUpMove      *= fRatio;
			}
		}
	}

	//if ( !m_bSpeedCropped && ( mv->m_nButtons & IN_SPEED ) && !( player->m_Local.m_bDucked && !player->m_Local.m_bDucking ))
	if ( !m_bSpeedCropped && ( mv->m_nButtons & IN_SPEED ))
	{
		float frac = 1.0f; // TODO can we remove this ?
		mv->m_flForwardMove *= frac;
		mv->m_flSideMove    *= frac;
		mv->m_flUpMove      *= frac;
		m_bSpeedCropped = true;
	}

	// slow marine down if walk key is held down
	if ( mv->m_nButtons & IN_WALK )
	{
		//Msg("Walking, forward=%f side=%f\n", mv->m_flForwardMove, mv->m_flSideMove);
		
		float fSpeed = ( mv->m_flForwardMove * mv->m_flForwardMove ) +
			  ( mv->m_flSideMove * mv->m_flSideMove );

		if (fSpeed > 0)
		{
			float frac = 180.0f / sqrt( fSpeed );

			mv->m_flForwardMove *= frac;
			mv->m_flSideMove    *= frac;
			mv->m_flUpMove      *= frac;
		}
	}

	marine->m_bWalking = ( mv->m_nButtons & IN_WALK ) != 0;

	if ( marine->GetFlags() & FL_FROZEN ||
		 marine->GetFlags() & FL_ONTRAIN || 
		 IsDead() )
	{
		mv->m_flForwardMove = 0;
		mv->m_flSideMove    = 0;
		mv->m_flUpMove      = 0;
	}

	DecayPunchAngle();

	// Take angles from command.
	if ( !IsDead() )
	{
		v_angle = mv->m_vecAngles;
		//v_angle = v_angle + player->m_Local.m_vecPunchAngle;

		// Now adjust roll angle
		if ( marine->GetMoveType() != MOVETYPE_ISOMETRIC  &&
			 marine->GetMoveType() != MOVETYPE_NOCLIP )
		{
			mv->m_vecAngles[ROLL]  = CalcRoll( v_angle, mv->m_vecVelocity, sv_rollangle.GetFloat(), sv_rollspeed.GetFloat() );
		}
		else
		{
			mv->m_vecAngles[ROLL] = 0.0; // v_angle[ ROLL ];
		}
		mv->m_vecAngles[PITCH] = v_angle[PITCH];
		mv->m_vecAngles[YAW]   = v_angle[YAW];
	}
	else
	{
		mv->m_vecAngles = mv->m_vecOldAngles;
	}

	// Set dead player view_offset
	if ( IsDead() )
	{
		player->SetViewOffset( VEC_DEAD_VIEWHEIGHT );
	}

	// Adjust client view angles to match values used on server.
	if ( mv->m_vecAngles[YAW] > 180.0f )
	{
		mv->m_vecAngles[YAW] -= 360.0f;
	}
}

void CASW_MarineGameMovement::ReduceTimers( void )
{
	float frame_msec = 1000.0f * gpGlobals->frametime;
	int nFrameMsec = (int)frame_msec;

	if ( player->m_Local.m_nDuckTimeMsecs > 0 )
	{
		player->m_Local.m_nDuckTimeMsecs -= nFrameMsec;
		if ( player->m_Local.m_nDuckTimeMsecs < 0 )
		{
			player->m_Local.m_nDuckTimeMsecs = 0;
		}
	}
	if ( player->m_Local.m_nDuckJumpTimeMsecs > 0 )
	{
		player->m_Local.m_nDuckJumpTimeMsecs -= nFrameMsec;
		if ( player->m_Local.m_nDuckJumpTimeMsecs < 0 )
		{
			player->m_Local.m_nDuckJumpTimeMsecs = 0;
		}
	}
	if ( player->m_Local.m_nJumpTimeMsecs > 0 )
	{
		player->m_Local.m_nJumpTimeMsecs -= nFrameMsec;
		if ( player->m_Local.m_nJumpTimeMsecs < 0 )
		{
			player->m_Local.m_nJumpTimeMsecs = 0;
		}
	}

	if ( player->GetSwimSoundTime() > 0 )
	{
		player->SetSwimSoundTime(MAX(player->GetSwimSoundTime() - frame_msec, 0));
	}
}

// get a conservative bounds for this player's movement traces
// This allows gamemovement to optimize those traces
void CASW_MarineGameMovement::SetupMovementBounds( CMoveData *move )
{
	if ( m_pTraceListData )
	{
		m_pTraceListData->Reset();
	}
	else
	{
		m_pTraceListData = enginetrace->AllocTraceListData();
	}
	if ( !marine || !player )
	{
		return;
	}

	Vector moveMins, moveMaxs;
	ClearBounds( moveMins, moveMaxs );
	Vector start = move->GetAbsOrigin();
	float radius = ((move->m_vecVelocity.Length() + move->m_flMaxSpeed) * gpGlobals->frametime) + 1.0f;
	// NOTE: assumes the unducked bbox encloses the ducked bbox
	Vector boxMins = GetPlayerMins(false);
	Vector boxMaxs = GetPlayerMaxs(false);

	// bloat by traveling the max velocity in all directions, plus the stepsize up/down
	Vector bloat;
	bloat.Init(radius, radius, radius);
	bloat.z += player->m_Local.m_flStepSize;
	AddPointToBounds( start + boxMaxs + bloat, moveMins, moveMaxs );
	AddPointToBounds( start + boxMins - bloat, moveMins, moveMaxs );
	// now build an optimized trace within these bounds
	enginetrace->SetupLeafAndEntityListBox( moveMins, moveMaxs, m_pTraceListData );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pMove - 
//-----------------------------------------------------------------------------
void CASW_MarineGameMovement::ProcessMovement( CBasePlayer *pPlayer, CBaseEntity *pMarine, CMoveData *pMove )
{
	Assert( pMove && pPlayer && pMarine );

	CASW_Marine *pMarineEntity = CASW_Marine::AsMarine( pMarine );

	static float old_z_pos = 0;

#ifdef CLIENT_DLL
	//Msg("  [C] Process move jump=%d oldjump=%d\n", pMove->m_nButtons & IN_JUMP, pMove->m_nOldButtons & IN_JUMP);
	//Msg("c zpos = %f\tzchange = %f\n", pMarine->GetAbsOrigin().z, pMarine->GetAbsOrigin().z - old_z_pos);
#else/
	//Msg("[S] Process move jump=%d oldjump=%d\n", pMove->m_nButtons & IN_JUMP, pMove->m_nOldButtons & IN_JUMP);
	//Msg("s zpos = %f\tzchange = %f\n", pMarine->GetAbsOrigin().z, pMarine->GetAbsOrigin().z - old_z_pos);
#endif
	old_z_pos = pMarine->GetAbsOrigin().z;
#ifdef GAME_DLL	// hurt the marine if he's trying to walk on top of an alien and it's not friendly
	if ( pMarineEntity && pMarine->GetGroundEntity() && pMarine->GetGroundEntity()->IsNPC() )
	{
		CASW_Alien *pAlien = dynamic_cast<CASW_Alien*>(pMarine->GetGroundEntity());
		if (pAlien && gpGlobals->curtime > pMarineEntity->m_fNextAlienWalkDamage)
		{
			CTakeDamageInfo info( pAlien, pAlien, 15, DMG_SLASH );
			Vector diff = pMarine->GetAbsOrigin() - pAlien->GetAbsOrigin();
			diff.NormalizeInPlace();
			CalculateMeleeDamageForce( &info, diff, pMarine->GetAbsOrigin() - diff * 30 );
			pMarine->TakeDamage( info );
			pMarineEntity->m_fNextAlienWalkDamage = gpGlobals->curtime + 0.4f;
		}
	}
#endif

	float flStoreFrametime = gpGlobals->frametime;

	//!!HACK HACK: Adrian - slow down all player movement by this factor.
	//!!Blame Yahn for this one.
	gpGlobals->frametime *= pPlayer->GetLaggedMovementValue();

	//if (pMove->m_vecVelocity.x > 150)
	//{
		//Msg("going fast right");
	//}

	ResetGetPointContentsCache();

	// Cropping movement speed scales mv->m_fForwardSpeed etc. globally
	// Once we crop, we don't want to recursively crop again, so we set the crop
	//  flag globally here once per usercmd cycle.
	m_bSpeedCropped = false;

	player = pPlayer;
	marine = pMarineEntity;
	mv = pMove;
	mv->m_flMaxSpeed = asw_sv_maxspeed.GetFloat();

	// check for melee attacks
	ASWMeleeSystem()->ProcessMovement( pMarineEntity, mv );

	// Run the command.
	PlayerMove();

	FinishMove();

	//This is probably not needed, but just in case.
	gpGlobals->frametime = flStoreFrametime;
}


//-----------------------------------------------------------------------------
// Purpose: Sets ground entity
//-----------------------------------------------------------------------------
void CASW_MarineGameMovement::FinishMove( void )
{
	mv->m_nOldButtons = mv->m_nButtons;
}

#define PUNCH_DAMPING		9.0f		// bigger number makes the response more damped, smaller is less damped
										// currently the system will overshoot, with larger damping values it won't
#define PUNCH_SPRING_CONSTANT	65.0f	// bigger number increases the speed at which the view corrects

//-----------------------------------------------------------------------------
// Purpose: Decays the punchangle toward 0,0,0.
//			Modelled as a damped spring
//-----------------------------------------------------------------------------
void CASW_MarineGameMovement::DecayPunchAngle( void )
{
	if ( player->m_Local.m_vecPunchAngle->LengthSqr() > 0.001 || player->m_Local.m_vecPunchAngleVel->LengthSqr() > 0.001 )
	{
		player->m_Local.m_vecPunchAngle += player->m_Local.m_vecPunchAngleVel * gpGlobals->frametime;
		float damping = 1 - (PUNCH_DAMPING * gpGlobals->frametime);
		
		if ( damping < 0 )
		{
			damping = 0;
		}
		player->m_Local.m_vecPunchAngleVel *= damping;
		 
		// torsional spring
		// UNDONE: Per-axis spring constant?
		float springForceMagnitude = PUNCH_SPRING_CONSTANT * gpGlobals->frametime;
		springForceMagnitude = clamp(springForceMagnitude, 0, 2 );
		player->m_Local.m_vecPunchAngleVel -= player->m_Local.m_vecPunchAngle * springForceMagnitude;

		// don't wrap around
		player->m_Local.m_vecPunchAngle.Init( 
			clamp(player->m_Local.m_vecPunchAngle->x, -89, 89 ), 
			clamp(player->m_Local.m_vecPunchAngle->y, -179, 179 ),
			clamp(player->m_Local.m_vecPunchAngle->z, -89, 89 ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_MarineGameMovement::StartGravity( void )
{
	float ent_gravity;
	
	if (marine->GetGravity())
		ent_gravity = marine->GetGravity();
	else
		ent_gravity = 1.0;

	// Add gravity so they'll be in the correct position during movement
	// yes, this 0.5 looks wrong, but it's not.  
	mv->m_vecVelocity[2] -= (ent_gravity * asw_marine_gravity.GetFloat() * 0.5 * gpGlobals->frametime );
	mv->m_vecVelocity[2] += marine->GetBaseVelocity()[2] * gpGlobals->frametime;

	Vector temp = marine->GetBaseVelocity();
	temp[ 2 ] = 0;
	marine->SetBaseVelocity( temp );

	CheckVelocity();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_MarineGameMovement::CheckWaterJump( void )
{
	Vector	flatforward;
	Vector forward;
	Vector	flatvelocity;
	float curspeed;

#ifdef ASW_NO_WATER_JUMP
	return;
#endif

	if ( asw_controls.GetInt() == 1 )
		AngleVectors( ASWGameRules()->GetTopDownMovementAxis(), &forward ); 
	else
		AngleVectors( mv->m_vecViewAngles, &forward );  // Determine movement angles

	// Already water jumping.
	if (player->GetWaterJumpTime())
		return;

	// Don't hop out if we just jumped in
	if (mv->m_vecVelocity[2] < -180)
		return; // only hop out if we are moving up

	// See if we are backing up
	flatvelocity[0] = mv->m_vecVelocity[0];
	flatvelocity[1] = mv->m_vecVelocity[1];
	flatvelocity[2] = 0;

	// Must be moving
	curspeed = VectorNormalize( flatvelocity );
	
	// see if near an edge
	flatforward[0] = forward[0];
	flatforward[1] = forward[1];
	flatforward[2] = 0;
	VectorNormalize (flatforward);

	// Are we backing into water from steps or something?  If so, don't pop forward
	if ( curspeed != 0.0 && ( DotProduct( flatvelocity, flatforward ) < 0.0 ) )
		return;

	Vector vecStart;
	// Start line trace at waist height (using the center of the player for this here)
	vecStart= mv->GetAbsOrigin() + (GetPlayerMins() + GetPlayerMaxs() ) * 0.5;

	Vector vecEnd;
	VectorMA( vecStart, 24.0f, flatforward, vecEnd );
	
	trace_t tr;
	TraceMarineBBox( vecStart, vecEnd, MASK_PLAYERSOLID, COLLISION_GROUP_PLAYER_MOVEMENT, tr );
	if ( tr.fraction < 1.0 )		// solid at waist
	{
		vecStart.z = mv->GetAbsOrigin().z + player->GetViewOffset().z + WATERJUMP_HEIGHT; 
		VectorMA( vecStart, 24.0f, flatforward, vecEnd );
		VectorMA( vec3_origin, -50.0f, tr.plane.normal, player->m_vecWaterJumpVel );

		TraceMarineBBox( vecStart, vecEnd, MASK_PLAYERSOLID, COLLISION_GROUP_PLAYER_MOVEMENT, tr );
		if ( tr.fraction == 1.0 )		// open at eye level
		{
			// Now trace down to see if we would actually land on a standable surface.
			VectorCopy( vecEnd, vecStart );
			vecEnd.z -= 1024.0f;
			TraceMarineBBox( vecStart, vecEnd, MASK_PLAYERSOLID, COLLISION_GROUP_PLAYER_MOVEMENT, tr );
			if ( ( tr.fraction < 1.0f ) && ( tr.plane.normal.z >= 0.7 ) )
			{
				mv->m_vecVelocity[2] = 250.0f;			// Push up
				mv->m_nOldButtons |= IN_JUMP;		// Don't jump again until released
				marine->AddFlag( FL_WATERJUMP );
				player->SetWaterJumpTime(2000.0f);	// Do this for 2 seconds
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_MarineGameMovement::WaterJump( void )
{
	if (player->GetWaterJumpTime() > 10000)
		player->SetWaterJumpTime(10000);

	if (!player->GetWaterJumpTime())
		return;

	player->SetWaterJumpTime( player->GetWaterJumpTime() - 1000.0f * gpGlobals->frametime );

	if (player->GetWaterJumpTime() <= 0 || !marine->GetWaterLevel())
	{
		player->SetWaterJumpTime(0);
		marine->RemoveFlag( FL_WATERJUMP );
	}
	
	mv->m_vecVelocity[0] = player->m_vecWaterJumpVel[0];
	mv->m_vecVelocity[1] = player->m_vecWaterJumpVel[1];
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_MarineGameMovement::WaterMove( void )
{
	int		i;
	Vector	wishvel;
	float	wishspeed;
	Vector	wishdir;
	Vector	start, dest;
	Vector  temp;
	trace_t	pm;
	float speed, newspeed, addspeed, accelspeed;
	Vector forward, right, up;

	if ( asw_controls.GetInt() == 1 )
		AngleVectors( ASWGameRules()->GetTopDownMovementAxis(), &forward, &right, &up ); 
	else
		AngleVectors (mv->m_vecViewAngles, &forward, &right, &up);  // Determine movement angles 

	//
	// user intentions
	//
	for (i=0 ; i<3 ; i++)
	{
		wishvel[i] = forward[i]*mv->m_flForwardMove + right[i]*mv->m_flSideMove;
	}

	// if we have the jump key down, move us up as well
	if (mv->m_nButtons & IN_JUMP)
	{
		wishvel[2] += mv->m_flClientMaxSpeed;
	}
	// Sinking after no other movement occurs
	else if (!mv->m_flForwardMove && !mv->m_flSideMove && !mv->m_flUpMove)
	{
		wishvel[2] -= 60;		// drift towards bottom
	}
	else  // Go straight up by upmove amount.
	{
		wishvel[2] += mv->m_flUpMove;
	}

	// Copy it over and determine speed
	VectorCopy (wishvel, wishdir);
	wishspeed = VectorNormalize(wishdir);

	// Cap speed.
	if (wishspeed > mv->m_flMaxSpeed)
	{
		VectorScale (wishvel, mv->m_flMaxSpeed/wishspeed, wishvel);
		wishspeed = mv->m_flMaxSpeed;
	}

	// Slow us down a bit.
	wishspeed *= 0.8;
	
	// Water friction
	VectorCopy(mv->m_vecVelocity, temp);
	speed = VectorNormalize(temp);
	if (speed)
	{
		newspeed = speed - gpGlobals->frametime * speed * asw_marine_friction.GetFloat() * marine->m_surfaceFriction;
		if (newspeed < 0.1f)
		{
			newspeed = 0;
		}

		VectorScale (mv->m_vecVelocity, newspeed/speed, mv->m_vecVelocity);
	}
	else
	{
		newspeed = 0;
	}

	// water acceleration
	if (wishspeed >= 0.1f)  // old !
	{
		addspeed = wishspeed - newspeed;
		if (addspeed > 0)
		{
			VectorNormalize(wishvel);
			accelspeed = sv_accelerate.GetFloat() * wishspeed * gpGlobals->frametime * marine->m_surfaceFriction;
			if (accelspeed > addspeed)
			{
				accelspeed = addspeed;
			}

			for (i = 0; i < 3; i++)
			{
				float deltaSpeed = accelspeed * wishvel[i];
				mv->m_vecVelocity[i] += deltaSpeed;
				mv->m_outWishVel[i] += deltaSpeed;
			}
		}
	}

	VectorAdd (mv->m_vecVelocity, marine->GetBaseVelocity(), mv->m_vecVelocity);

	// Now move
	// assume it is a stair or a slope, so press down from stepheight above
	VectorMA (mv->GetAbsOrigin(), gpGlobals->frametime, mv->m_vecVelocity, dest);
	
	TraceMarineBBox( mv->GetAbsOrigin(), dest, MASK_PLAYERSOLID, COLLISION_GROUP_PLAYER_MOVEMENT, pm );
	if ( pm.fraction == 1.0f )
	{
		VectorCopy( dest, start );
		if ( player->m_Local.m_bAllowAutoMovement )
		{
			start[2] += player->m_Local.m_flStepSize + 1;
		}
		
		TraceMarineBBox( start, dest, MASK_PLAYERSOLID, COLLISION_GROUP_PLAYER_MOVEMENT, pm );
		
		if (!pm.startsolid && !pm.allsolid)
		{	
			float stepDist = pm.endpos.z - mv->GetAbsOrigin().z;
			mv->m_outStepHeight += stepDist;
			// walked up the step, so just keep result and exit
			mv->SetAbsOrigin( pm.endpos );
			VectorSubtract( mv->m_vecVelocity, marine->GetBaseVelocity(), mv->m_vecVelocity );
			return;
		}

		// Try moving straight along out normal path.
		TryPlayerMove();
	}
	else
	{
		if ( !marine->GetGroundEntity() )
		{
			TryPlayerMove();
			VectorSubtract( mv->m_vecVelocity, marine->GetBaseVelocity(), mv->m_vecVelocity );
			return;
		}

		StepMove( dest, pm );
	}
	
	VectorSubtract( mv->m_vecVelocity, marine->GetBaseVelocity(), mv->m_vecVelocity );
}

//-----------------------------------------------------------------------------
// Purpose: Does the basic move attempting to climb up step heights.  It uses
//          the mv->m_vecAbsOrigin and mv->m_vecVelocity.  It returns a new
//          new mv->m_vecAbsOrigin, mv->m_vecVelocity, and mv->m_outStepHeight.
//-----------------------------------------------------------------------------
void CASW_MarineGameMovement::StepMove( Vector &vecDestination, trace_t &trace )
{
	Vector vecEndPos;
	VectorCopy( vecDestination, vecEndPos );

	// Try sliding forward both on ground and up 16 pixels
	//  take the move that goes farthest
	Vector vecStartPos, vecStartVel;
	VectorCopy( mv->GetAbsOrigin(), vecStartPos );		// store start position and velocity
	VectorCopy( mv->m_vecVelocity, vecStartVel );

	// Slide move down.
	TryPlayerMove( &vecEndPos, &trace );
	
	// Down results.
	Vector vecDownPos, vecDownVel;
	Vector vecNormalMovePos, vecNormalMoveVel;
	VectorCopy( mv->GetAbsOrigin(), vecNormalMovePos );
	VectorCopy( mv->m_vecVelocity, vecNormalMoveVel );
	
	// Reset original values.
	mv->SetAbsOrigin( vecStartPos );
	VectorCopy( vecStartVel, mv->m_vecVelocity );
	
	// Takes the marine from his start position and tries to slide him up by the stepsize
	VectorCopy( mv->GetAbsOrigin(), vecEndPos );
	if ( player->m_Local.m_bAllowAutoMovement )
	{
		vecEndPos.z += player->m_Local.m_flStepSize;
	}
	
	TraceMarineBBox( mv->GetAbsOrigin(), vecEndPos, MASK_PLAYERSOLID, COLLISION_GROUP_PLAYER_MOVEMENT, trace );
	if ( !trace.startsolid && !trace.allsolid )
	{
		mv->SetAbsOrigin( trace.endpos );
	}
	
	// with the marine moved up by stepsize units, then try to do his move
	// this moves him forward by his velocity * time, sliding along the collision planes
	TryPlayerMove();
	
	// with the marine moved forward, now try to drop him straight back down to the floor again
	VectorCopy( mv->GetAbsOrigin(), vecEndPos );
	if ( player->m_Local.m_bAllowAutoMovement )
	{
		vecEndPos.z -= player->m_Local.m_flStepSize;
	}
		
	TraceMarineBBox( mv->GetAbsOrigin(), vecEndPos, MASK_PLAYERSOLID, COLLISION_GROUP_PLAYER_MOVEMENT, trace );
#ifndef CLIENT_DLL
	if (asw_debug_steps.GetBool())
	{
		NDebugOverlay::Box(mv->GetAbsOrigin(), GetPlayerMins(), GetPlayerMaxs(), 255, 0, 0, 0 ,0);
		NDebugOverlay::Box(vecEndPos, GetPlayerMins(), GetPlayerMaxs(), 0, 255, 0, 0 ,0);
	}
#endif
	
	// If we are not on the ground any more then use the original movement attempt.
	if ( trace.plane.normal[2] < 0.7 )
	{
		mv->SetAbsOrigin( vecNormalMovePos );
		VectorCopy( vecNormalMoveVel, mv->m_vecVelocity );
		float flStepDist = mv->GetAbsOrigin().z - vecStartPos.z;
		if ( flStepDist > 0.0f )
		{
			mv->m_outStepHeight += flStepDist;
		}
		return;
	}
	
	// If the trace ended up in empty space, copy the end over to the origin.
	if ( !trace.startsolid && !trace.allsolid )
	{
		mv->SetAbsOrigin( trace.endpos );
	}
	
	// Copy this origin to up.
	Vector vecUpPos;
	VectorCopy( mv->GetAbsOrigin(), vecUpPos );
	
	// decide which one went farther
	float flDownDist = ( vecNormalMovePos.x - vecStartPos.x ) * ( vecNormalMovePos.x - vecStartPos.x ) + ( vecNormalMovePos.y - vecStartPos.y ) * ( vecNormalMovePos.y - vecStartPos.y );
	float flUpDist = ( vecUpPos.x - vecStartPos.x ) * ( vecUpPos.x - vecStartPos.x ) + ( vecUpPos.y - vecStartPos.y ) * ( vecUpPos.y - vecStartPos.y );
	if ( flDownDist > flUpDist )
	{
		mv->SetAbsOrigin( vecNormalMovePos );
		VectorCopy( vecNormalMoveVel, mv->m_vecVelocity );
	}
	else 
	{
		// copy z value from slide move
		mv->m_vecVelocity.z = vecNormalMoveVel.z;
	}
	
	float flStepDist = mv->GetAbsOrigin().z - vecStartPos.z;
	if ( flStepDist > 0 )
	{
		if (asw_debug_steps.GetBool())
			Msg("Marine stepdist %f\n", flStepDist);
		mv->m_outStepHeight += flStepDist;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_MarineGameMovement::Friction( void )
{
	float	speed, newspeed, control;
	float	friction;
	float	drop;
	Vector newvel;

	
	// If we are in water jump cycle, don't apply friction
	if (player->GetWaterJumpTime())
		return;

	// Calculate speed
	speed = VectorLength( mv->m_vecVelocity );
	
	// If too slow, return
	if (speed < 0.1f)
	{
		return;
	}

	drop = 0;

	// apply ground friction
	if (marine->GetGroundEntity() != NULL)  // On an entity that is the ground
	{
		Vector start, stop;
		trace_t pm;

		//
		// NOTE: added a "1.0f" to the player minimum (bbox) value so that the 
		//       trace starts just inside of the bounding box, this make sure
		//       that we don't get any collision epsilon (on surface) errors.
		//		 The significance of the 16 below is this is how many units out front we are checking
		//		 to see if the player box would fall.  The 49 is the number of units down that is required
		//		 to be considered a fall.  49 is derived from 1 (added 1 from above) + 48 the max fall 
		//		 distance a player can fall and still jump back up.
		//
		//		 UNDONE: In some cases there are still problems here.  Specifically, no collision check is
		//		 done so 16 units in front of the player could be inside a volume or past a collision point.
		start[0] = stop[0] = mv->GetAbsOrigin()[0] + (mv->m_vecVelocity[0]/speed)*16;
		start[1] = stop[1] = mv->GetAbsOrigin()[1] + (mv->m_vecVelocity[1]/speed)*16;
		start[2] = mv->GetAbsOrigin()[2] + ( GetPlayerMins()[2] + 1.0f );
		stop[2] = start[2] - 49;

		if ( g_bMovementOptimizations )
		{
			// We don't actually need this trace.
		}
		else
		{
			TraceMarineBBox( start, stop, MASK_PLAYERSOLID, COLLISION_GROUP_PLAYER_MOVEMENT, pm ); 
		}

		friction = asw_marine_friction.GetFloat();

		// Grab friction value.
		friction *= marine->m_surfaceFriction;  // player friction?

		// Bleed off some speed, but if we have less than the bleed
		//  threshhold, bleed the theshold amount.
		control = (speed < sv_stopspeed.GetFloat()) ?
			sv_stopspeed.GetFloat() : speed;

		// Add the amount to the drop amount.
		drop += control*friction*gpGlobals->frametime;
	}

	// scale the velocity
	newspeed = speed - drop;
	if (newspeed < 0)
		newspeed = 0;

	// Determine proportion of old speed we are using.
	newspeed /= speed;

	// Adjust velocity according to proportion.
	newvel[0] = mv->m_vecVelocity[0] * newspeed;
	newvel[1] = mv->m_vecVelocity[1] * newspeed;
	newvel[2] = mv->m_vecVelocity[2] * newspeed;

	VectorCopy( newvel, mv->m_vecVelocity );
 	mv->m_outWishVel -= (1.f-newspeed) * mv->m_vecVelocity;

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_MarineGameMovement::FinishGravity( void )
{
	float ent_gravity;

	if ( player->GetWaterJumpTime() )
		return;

	if ( marine->GetGravity() )
		ent_gravity = marine->GetGravity();
	else
		ent_gravity = 1.0;

	// Get the correct velocity for the end of the dt 
  	mv->m_vecVelocity[2] -= (ent_gravity * asw_marine_gravity.GetFloat() * gpGlobals->frametime * 0.5);

	CheckVelocity();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : wishdir - 
//			accel - 
//-----------------------------------------------------------------------------
void CASW_MarineGameMovement::AirAccelerate( Vector& wishdir, float wishspeed, float accel )
{
	int i;
	float addspeed, accelspeed, currentspeed;
	float wishspd;

	wishspd = wishspeed;
	
	if (player->pl.deadflag)
		return;
	
	if (player->GetWaterJumpTime())
		return;

	// Cap speed
	if (wishspd > 30)
		wishspd = 30;
	
	// Determine veer amount
	currentspeed = mv->m_vecVelocity.Dot(wishdir);

	// See how much to add
	addspeed = wishspd - currentspeed;

	// If not adding any, done.
	if (addspeed <= 0)
		return;

	// Determine acceleration speed after acceleration
	accelspeed = accel * wishspeed * gpGlobals->frametime * marine->m_surfaceFriction;

	// Cap it
	if (accelspeed > addspeed)
		accelspeed = addspeed;
	
	// Adjust pmove vel.
	for (i=0 ; i<3 ; i++)
	{
		mv->m_vecVelocity[i] += accelspeed * wishdir[i];
		mv->m_outWishVel[i] += accelspeed * wishdir[i];
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_MarineGameMovement::AirMove( void )
{
	int			i;
	Vector		wishvel;
	float		fmove, smove;
	Vector		wishdir;
	float		wishspeed;
	Vector forward, right, up;

	if ( asw_controls.GetInt() == 1 )
		AngleVectors( ASWGameRules()->GetTopDownMovementAxis(), &forward, &right, &up ); 
	else
		AngleVectors (mv->m_vecViewAngles, &forward, &right, &up);  // Determine movement angles  
	
	// Copy movement amounts
	fmove = mv->m_flForwardMove;
	smove = mv->m_flSideMove;
	
	// Zero out z components of movement vectors
	forward[2] = 0;
	right[2]   = 0;
	VectorNormalize(forward);  // Normalize remainder of vectors
	VectorNormalize(right);    // 

	for (i=0 ; i<2 ; i++)       // Determine x and y parts of velocity
	wishvel[i] = forward[i]*fmove + right[i]*smove;
	wishvel[2] = 0;             // Zero out z part of velocity

	VectorCopy (wishvel, wishdir);   // Determine maginitude of speed of move
	wishspeed = VectorNormalize(wishdir);

	//
	// clamp to server defined max speed
	//
	if ( wishspeed != 0 && (wishspeed > mv->m_flMaxSpeed))
	{
		VectorScale (wishvel, mv->m_flMaxSpeed/wishspeed, wishvel);
		wishspeed = mv->m_flMaxSpeed;
	}
	
	if ( mv->m_bNoAirControl == false )	
	{
		AirAccelerate( wishdir, wishspeed, sv_airaccelerate.GetFloat() );
	}

	// Add in any base velocity to the current velocity.
	VectorAdd(mv->m_vecVelocity, marine->GetBaseVelocity(), mv->m_vecVelocity );

	if (asw_debug_air_move.GetBool())
		Msg("   #2: Vel=%f,%f,%f\n", mv->m_vecVelocity[0], mv->m_vecVelocity[1], mv->m_vecVelocity[2]);

	TryPlayerMove();

	// Now pull the base velocity back out.   Base velocity is set if you are on a moving object, like a conveyor (or maybe another monster?)
	VectorSubtract( mv->m_vecVelocity, marine->GetBaseVelocity(), mv->m_vecVelocity );
}


bool CASW_MarineGameMovement::CanAccelerate()
{
	// Dead players don't accelerate.
	if (player->pl.deadflag)
		return false;

	// If waterjumping, don't accelerate
	if (player->GetWaterJumpTime())
		return false;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : wishdir - 
//			wishspeed - 
//			accel - 
//-----------------------------------------------------------------------------
void CASW_MarineGameMovement::Accelerate( Vector& wishdir, float wishspeed, float accel )
{
	int i;
	float addspeed, accelspeed, currentspeed;

	// This gets overridden because some games (CSPort) want to allow dead (observer) players
	// to be able to move around.
	if ( !CanAccelerate() )
		return;

	// See if we are changing direction a bit
	currentspeed = mv->m_vecVelocity.Dot(wishdir);

	// Reduce wishspeed by the amount of veer.
	addspeed = wishspeed - currentspeed;

	// If not going to add any speed, done.
	if (addspeed <= 0)
		return;

	// Determine amount of accleration.
	accelspeed = accel * gpGlobals->frametime * MAX( wishspeed, ASW_MIN_ACCELERATION_SPEED ) * marine->m_surfaceFriction;

	// Cap at addspeed
	if (accelspeed > addspeed)
		accelspeed = addspeed;
	
	// Adjust velocity.
	for (i=0 ; i<3 ; i++)
	{
		mv->m_vecVelocity[i] += accelspeed * wishdir[i];	
	}

}

//-----------------------------------------------------------------------------
// Purpose: Try to keep a walking player on the ground when running down slopes etc
// ASW NOTE: We're not using this as we adjusted the movement code to do this ourselves before getting this code
//-----------------------------------------------------------------------------
#if 0
void CASW_MarineGameMovement::StayOnGround( void )
{
	trace_t trace;
	Vector start( mv->m_vecAbsOrigin );
	Vector end( mv->m_vecAbsOrigin );
	start.z += 2;
	end.z -= player->GetStepSize();

	// See how far up we can go without getting stuck
	TracePlayerBBox( mv->m_vecAbsOrigin, start, MASK_PLAYERSOLID, COLLISION_GROUP_PLAYER_MOVEMENT, trace );
	start = trace.endpos;

	// using trace.startsolid is unreliable here, it doesn't get set when
	// tracing bounding box vs. terrain

	// Now trace down from a known safe position
	TracePlayerBBox( start, end, MASK_PLAYERSOLID, COLLISION_GROUP_PLAYER_MOVEMENT, trace );
	if ( trace.fraction > 0.0f &&			// must go somewhere
		trace.fraction < 1.0f &&			// must hit something
		!trace.startsolid &&				// can't be embedded in a solid
		trace.plane.normal[2] >= 0.7 )		// can't hit a steep slope that we can't stand on anyway
	{
		float flDelta = fabs(mv->m_vecAbsOrigin.z - trace.endpos.z);

		//This is incredibly hacky. The real problem is that trace returning that strange value we can't network over.
		if ( flDelta > 0.5f * COORD_RESOLUTION)
		{
			mv->m_vecAbsOrigin = trace.endpos;
		}
	}
}
#endif


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_MarineGameMovement::WalkMove( void )
{
	int i;

	Vector wishvel;
	float spd;
	float fmove, smove;
	Vector wishdir;
	float wishspeed;

	Vector dest;
	trace_t pm;
	Vector forward, right, up;

	if ( asw_controls.GetInt() == 1 )
		AngleVectors( ASWGameRules()->GetTopDownMovementAxis(), &forward, &right, &up ); 
	else
		AngleVectors (mv->m_vecViewAngles, &forward, &right, &up);  // Determine movement angles  

	CHandle< CBaseEntity > oldground;
	oldground = marine->GetGroundEntity();
	
	// Copy movement amounts
	fmove = mv->m_flForwardMove;
	smove = mv->m_flSideMove;
	
	// Zero out z components of movement vectors
	if ( g_bMovementOptimizations )
	{
		if ( forward[2] != 0 )
		{
			forward[2] = 0;
			VectorNormalize( forward );
		}

		if ( right[2] != 0 )
		{
			right[2] = 0;
			VectorNormalize( right );
		}
	}
	else
	{
		forward[2] = 0;
		right[2]   = 0;
		
		VectorNormalize (forward);  // Normalize remainder of vectors.
		VectorNormalize (right);    // 
	}

	for (i=0 ; i<2 ; i++)       // Determine x and y parts of velocity
		wishvel[i] = forward[i]*fmove + right[i]*smove;
	
	wishvel[2] = 0;             // Zero out z part of velocity

	VectorCopy (wishvel, wishdir);   // Determine maginitude of speed of move
	wishspeed = VectorNormalize(wishdir);

	//
	// Clamp to server defined max speed
	//
	if ((wishspeed != 0.0f) && (wishspeed > mv->m_flMaxSpeed))
	{
		VectorScale (wishvel, mv->m_flMaxSpeed/wishspeed, wishvel);
		wishspeed = mv->m_flMaxSpeed;
	}

	// Set pmove velocity
	mv->m_vecVelocity[2] = 0;
	Accelerate ( wishdir, wishspeed, sv_accelerate.GetFloat() );
	mv->m_vecVelocity[2] = 0;

	// Add in any base velocity to the current velocity.
	VectorAdd (mv->m_vecVelocity, marine->GetBaseVelocity(), mv->m_vecVelocity );

	spd = VectorLength( mv->m_vecVelocity );

	if ( spd < 1.0f )
	{
		mv->m_vecVelocity.Init();
		// Now pull the base velocity back out.   Base velocity is set if you are on a moving object, like a conveyor (or maybe another monster?)
		VectorSubtract( mv->m_vecVelocity, marine->GetBaseVelocity(), mv->m_vecVelocity );
		return;
	}

	// first try just moving to the destination	
	dest[0] = mv->GetAbsOrigin()[0] + mv->m_vecVelocity[0]*gpGlobals->frametime;
	dest[1] = mv->GetAbsOrigin()[1] + mv->m_vecVelocity[1]*gpGlobals->frametime;	
	dest[2] = mv->GetAbsOrigin()[2];

	// first try moving directly to the next spot
	TraceMarineBBox( mv->GetAbsOrigin(), dest, MASK_PLAYERSOLID, COLLISION_GROUP_PLAYER_MOVEMENT, pm );

	// If we made it all the way, then copy trace end as new player position.
	mv->m_outWishVel += wishdir * wishspeed;

	if ( pm.fraction == 1)
	{		
		// asw check for stepping down stairs
		if (oldground != NULL)
		{
			Vector vecEndPos;
			VectorCopy( pm.endpos, vecEndPos );
			if ( player->m_Local.m_bAllowAutoMovement )
			{
				vecEndPos.z -= player->m_Local.m_flStepSize;
			}
				
			trace_t trace;
			TraceMarineBBox( pm.endpos, vecEndPos, MASK_PLAYERSOLID, COLLISION_GROUP_PLAYER_MOVEMENT, trace );
			
			// If we hit the ground, then use this move
			if ( trace.plane.normal[2] > 0.7 && !trace.startsolid && !trace.allsolid )
			{
				float flStepDist = trace.endpos.z - mv->GetAbsOrigin().z;
				if ( flStepDist != 0.0f )
				{
					mv->m_outStepHeight += flStepDist;
					mv->SetAbsOrigin( trace.endpos );
					// Now pull the base velocity back out.   Base velocity is set if you are on a moving object, like a conveyor (or maybe another monster?)
					VectorSubtract( mv->m_vecVelocity, marine->GetBaseVelocity(), mv->m_vecVelocity );
					if (asw_debug_steps.GetBool())
						Msg("moved down a stair %f\n", flStepDist);
					return;
				}				
			}
		}
		mv->SetAbsOrigin( pm.endpos );
		// Now pull the base velocity back out.   Base velocity is set if you are on a moving object, like a conveyor (or maybe another monster?)
		VectorSubtract( mv->m_vecVelocity, marine->GetBaseVelocity(), mv->m_vecVelocity );
		return;
	}

	// Don't walk up stairs if not on ground.
	if ( oldground == NULL && marine->GetWaterLevel()  == 0 )
	{
		// Now pull the base velocity back out.   Base velocity is set if you are on a moving object, like a conveyor (or maybe another monster?)
		VectorSubtract( mv->m_vecVelocity, marine->GetBaseVelocity(), mv->m_vecVelocity );
		return;
	}

	// If we are jumping out of water, don't do anything more.
	if ( player->GetWaterJumpTime() )         
	{
		// Now pull the base velocity back out.   Base velocity is set if you are on a moving object, like a conveyor (or maybe another monster?)
		VectorSubtract( mv->m_vecVelocity, marine->GetBaseVelocity(), mv->m_vecVelocity );
		return;
	}

	StepMove( dest, pm );

	// Now pull the base velocity back out.   Base velocity is set if you are on a moving object, like a conveyor (or maybe another monster?)
	VectorSubtract( mv->m_vecVelocity, marine->GetBaseVelocity(), mv->m_vecVelocity );
}

ConVar asw_jump_jet_height( "asw_jump_jet_height", "160", FCVAR_REPLICATED );
ConVar asw_jump_jet_pounce_height( "asw_jump_jet_pounce_height", "60", FCVAR_REPLICATED );
ConVar asw_blink_height( "asw_blink_height", "50", FCVAR_REPLICATED );
ConVar asw_charge_height( "asw_charge_height", "5", FCVAR_REPLICATED );
ConVar asw_blink_visible_time( "asw_blink_visible_time", "0.15", FCVAR_REPLICATED );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_MarineGameMovement::FullJumpJetMove()
{
	CASW_MoveData *pASWMove = static_cast<CASW_MoveData*>( mv );
	if ( marine->m_iJumpJetting == JJ_JUMP_JETS )
	{
		// update jump jet end over time with location sent up from the client
		if ( pASWMove->m_vecSkillDest != vec3_origin )
		{
			marine->m_vecJumpJetEnd = pASWMove->m_vecSkillDest;
		}
	}

	float flJumpJetTime = marine->m_flJumpJetEndTime - marine->m_flJumpJetStartTime;
	float flJumpJetProgress = ( gpGlobals->curtime - marine->m_flJumpJetStartTime ) / flJumpJetTime;
	flJumpJetProgress = clamp( flJumpJetProgress, 0.0f, 1.0f );

	// x/y linearly between start and end
	float xy_progress = flJumpJetProgress; //0.5f + 0.5f * sin( -0.5f * M_PI + flJumpJetProgress * M_PI );

	Vector vecJump = marine->m_vecJumpJetEnd - marine->m_vecJumpJetStart;
	Vector vecDest = marine->m_vecJumpJetStart + vecJump * xy_progress;

	if ( marine->m_iJumpJetting == JJ_JUMP_JETS )
	{
		// on the Z, blend between start and end dests, with a jump curve applied
		float flJumpHeight = asw_jump_jet_height.GetFloat();
		vecDest.z = marine->m_vecJumpJetStart.z + vecJump.z * flJumpJetProgress + sin( flJumpJetProgress * M_PI ) * flJumpHeight;
	}
	else if ( marine->m_iJumpJetting == JJ_CHARGE )
	{
		// slide to the destination Z
		float flJumpHeight = asw_charge_height.GetFloat();
		vecDest.z = marine->m_vecJumpJetStart.z + vecJump.z * flJumpJetProgress + sin( flJumpJetProgress * M_PI ) * flJumpHeight;
	}

	// check for hiding marine while he blinks
	if ( marine->m_iJumpJetting == JJ_BLINK )
	{
		bool bHide = ( flJumpJetProgress > asw_blink_visible_time.GetFloat() && flJumpJetProgress < ( 1.0f - asw_blink_visible_time.GetFloat() ) );
		if ( marine->IsEffectActive( EF_NODRAW ) != bHide )
		{
			if ( bHide )
			{
#ifdef CLIENT_DLL
				if ( prediction->InPrediction() && prediction->IsFirstTimePredicted() )
				{
#endif
					marine->EmitSound( "ASW_Blink.Blink" );
					DispatchParticleEffect( "Blink", marine->GetAbsOrigin(), vec3_angle );
					DispatchParticleEffect( "electrified_armor_burst", marine->GetAbsOrigin(), vec3_angle );
#ifdef CLIENT_DLL
				}
#else
				CASW_Weapon *pWeapon = marine->GetASWWeapon( ASW_INVENTORY_SLOT_EXTRA );
				ASWGameRules()->ShockNearbyAliens( marine, pWeapon );
#endif
				marine->AddEffects( EF_NODRAW );
			}
			else
			{
#ifdef CLIENT_DLL
				if ( prediction->InPrediction() && prediction->IsFirstTimePredicted() )
				{
#endif
					marine->EmitSound( "ASW_Blink.Teleport" );
					DispatchParticleEffect( "Blink", marine->GetAbsOrigin(), vec3_angle );
					DispatchParticleEffect( "electrified_armor_burst", marine->GetAbsOrigin(), vec3_angle );
#ifdef CLIENT_DLL
				}
#else
					CASW_Weapon *pWeapon = marine->GetASWWeapon( ASW_INVENTORY_SLOT_EXTRA );
					ASWGameRules()->ShockNearbyAliens( marine, pWeapon );
#endif
				marine->RemoveEffects( EF_NODRAW );
			}
		}

		float flJumpHeight = asw_blink_height.GetFloat();
		if ( flJumpJetProgress < asw_blink_visible_time.GetFloat() )		// jump straight up
		{
			float flUpProgress = flJumpJetProgress / asw_blink_visible_time.GetFloat();
			vecDest = marine->m_vecJumpJetStart;
			vecDest.z = marine->m_vecJumpJetStart.z + sin( flUpProgress * M_PI * 0.5f ) * flJumpHeight;
		}
		else if ( flJumpJetProgress < ( 1.0f - asw_blink_visible_time.GetFloat() ) )		// slide quickly to end X/Y
		{
			// lerp between start and end
			Vector vecStartTop = marine->m_vecJumpJetStart + Vector( 0, 0, flJumpHeight );
			Vector vecEndTop = marine->m_vecJumpJetEnd + Vector( 0, 0, flJumpHeight );

			float flSlideProgress = ( flJumpJetProgress - asw_blink_visible_time.GetFloat() ) / ( 1.0f - asw_blink_visible_time.GetFloat() * 2 );
			flSlideProgress = sin( flSlideProgress * M_PI * 0.5f );
			vecDest = vecStartTop + ( vecEndTop - vecStartTop ) * flSlideProgress;
		}
		else		// fall to ground
		{
			float flDownProgress = 1.0f - ( ( 1.0f - flJumpJetProgress ) / asw_blink_visible_time.GetFloat() );

			vecDest = marine->m_vecJumpJetEnd;
			vecDest.z = marine->m_vecJumpJetEnd.z + sin( flDownProgress * M_PI * 0.5f ) * flJumpHeight;
		}
	}

	mv->SetAbsOrigin( vecDest );

	if ( flJumpJetProgress >= 1.0f )
	{
		// TODO: Set these based on BLINK/HOVER	
		float flRadius = 200.0f;
#ifdef GAME_DLL
		int iBaseDamage = 150;
#endif
		CASW_Marine_Profile *pProfile = marine->GetMarineProfile();
		if ( pProfile )
		{
			if ( marine->m_iJumpJetting == JJ_BLINK )
			{
				// TODO: Stun aliens at the point of exit?
				/*
#ifdef GAME_DLL
				// any effects on blink landing
				CASW_Weapon_EffectVolume *pEffect = (CASW_Weapon_EffectVolume*) CreateEntityByName("asw_weapon_effect_volume");
				if ( pEffect )
				{
					pEffect->SetAbsOrigin( marine->GetAbsOrigin() );
					pEffect->SetOwnerEntity( marine );
					pEffect->SetOwnerWeapon( NULL );
					pEffect->SetEffectFlag( ASW_WEV_ELECTRIC_BIG );
					pEffect->SetDuration( pSkill->GetValue( CASW_Skill_Details::Duration, pProfile->GetMarineSkill( pSkill->m_iSkillIndex ) ) );
					pEffect->Spawn();
				}
#endif
				*/
			}
		}

		if ( marine->m_iJumpJetting == JJ_CHARGE || marine->m_iJumpJetting == JJ_JUMP_JETS )  // charge/jump jets
		{
			CEffectData	data;

			data.m_nHitBox = GetParticleSystemIndex( "jj_ground_pound" );
			data.m_vOrigin = marine->GetAbsOrigin();
			data.m_vStart = Vector( flRadius, 0, 0 );
			data.m_vAngles = marine->GetAbsAngles();
#ifdef CLIENT_DLL
			data.m_hEntity = NULL;
#else
			data.m_nEntIndex = 0;
#endif

#ifdef CLIENT_DLL
			if ( prediction->InPrediction() && prediction->IsFirstTimePredicted() )
			{
#endif
				CBroadcastRecipientFilter filter;
				DispatchEffect( filter, 0.0f, "ParticleEffect", data );
				marine->EmitSound( filter, marine->entindex(), "ASW_JumpJet.Impact" );
#ifdef CLIENT_DLL
				//ASW_TransmitShakeEvent( player, 25.0f, 1.0f, 0.4f, SHAKE_START, Vector( 0, 0, 1 ) );
				ScreenShake_t shake;
				shake.direction = Vector( 0, 0, 1 );
				shake.amplitude = 40.0f;
				shake.duration = 0.3f;
				shake.frequency = 1.0f;
				shake.command = SHAKE_START;
				ASW_TransmitShakeEvent( player, shake );
			}
#endif

#ifdef GAME_DLL

			if ( gpGlobals->maxClients == 1 )		// client isn't running prediction, so need to send down the screen shake
			{
				ScreenShake_t shake;
				shake.direction = Vector( 0, 0, 1 );
				shake.amplitude = 40.0f;
				shake.duration = 0.3f;
				shake.frequency = 1.0f;
				shake.command = SHAKE_START;
				ASW_TransmitShakeEvent( player, shake );
			}
			/*
			// scorch the ground
			trace_t		tr;
			UTIL_TraceLine ( marine->GetAbsOrigin(), marine->GetAbsOrigin() + Vector( 0, 0, -80 ), MASK_SHOT, 
				marine, COLLISION_GROUP_NONE, &tr);
			if ((tr.m_pEnt != GetWorldEntity()) || (tr.hitbox != 0))
			{
				// non-world needs smaller decals
				if( tr.m_pEnt && !tr.m_pEnt->IsNPC() )
				{
					UTIL_DecalTrace( &tr, "Rollermine.Crater" );
				}
			}
			else
			{
				UTIL_DecalTrace( &tr, "Rollermine.Crater" );
			}
			*/

			CASW_Weapon *pWeapon = marine->GetASWWeapon( ASW_INVENTORY_SLOT_EXTRA );
			CBaseEntity *pInflictor = pWeapon;
			if ( !pInflictor )
			{
				pInflictor = marine;
			}
			CTakeDamageInfo dmgInfo( pInflictor, marine, iBaseDamage, DMG_CLUB );
			dmgInfo.SetWeapon( pInflictor );
			ASWGameRules()->RadiusDamage( dmgInfo, marine->GetAbsOrigin(), flRadius * 0.5f, CLASS_NONE, NULL );
			ASWGameRules()->StumbleAliensInRadius( marine, marine->GetAbsOrigin(), flRadius );

			if ( pWeapon )
			{
				pWeapon->DestroyIfEmpty( true, false );
			}
#endif
		}

		marine->m_iJumpJetting = JJ_NONE;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_MarineGameMovement::FullMeleeMove( void )
{
	if ( !CheckWater() ) 
	{
		StartGravity();
	}

	// If we are leaping out of the water, just update the counters.
	if (player->GetWaterJumpTime())
	{
		WaterJump();
		TryPlayerMove();
		// See if we are still in water?
		CheckWater();
		return;
	}

	if (marine->GetGroundEntity() != NULL)
	{
		mv->m_vecVelocity[2] = 0.0;
		//Friction();
	}

	// Make sure velocity is valid.
	CheckVelocity();
	MeleeMove();

	// Set final flags.
	CategorizePosition();

	// Make sure velocity is valid.
	CheckVelocity();

	// Add any remaining gravitational component.
	if ( !CheckWater() )
	{
		FinishGravity();
	}

	// If we are on ground, no downward velocity.
	if ( marine->GetGroundEntity() != NULL )
	{
		mv->m_vecVelocity[2] = 0;
	}		
	CheckFalling();

	if  ( ( m_nOldWaterLevel == WL_NotInWater && marine->GetWaterLevel() != WL_NotInWater ) ||
		( m_nOldWaterLevel != WL_NotInWater && marine->GetWaterLevel() == WL_NotInWater ) )
	{
		PlaySwimSound();
	}
}
void CASW_MarineGameMovement::MeleeMove( void )
{
#ifdef GAME_DLL
	// do lag comp on melee movements
	//CASW_Player *pPlayer = marine->GetCommander();
	bool bLagComp = false;
	/*if ( pPlayer && marine->IsInhabited() && !CASW_Lag_Compensation::IsInLagCompensation() )
	{
		bLagComp = true;
		CASW_Lag_Compensation::AllowLagCompensation( pPlayer );
		CASW_Lag_Compensation::RequestLagCompensation( pPlayer, pPlayer->GetCurrentUserCommand() );
	}*/
#endif

	Vector dest;
	trace_t pm;

	CHandle< CBaseEntity > oldground = marine->GetGroundEntity();

	// first try just moving to the destination	
	dest[0] = mv->GetAbsOrigin()[0] + mv->m_vecVelocity[0]*gpGlobals->frametime;
	dest[1] = mv->GetAbsOrigin()[1] + mv->m_vecVelocity[1]*gpGlobals->frametime;	
	dest[2] = mv->GetAbsOrigin()[2];
	
	if ( !oldground )
	{
		dest[2] += mv->m_vecVelocity[2]*gpGlobals->frametime;
	}

	// first try moving directly to the next spot
	TraceMarineBBox( mv->GetAbsOrigin(), dest, MASK_PLAYERSOLID, COLLISION_GROUP_PLAYER_MOVEMENT, pm );
	if ( pm.DidHitNonWorldEntity() && marine->GetCurrentMeleeAttack() )
	{
		//marine->GetCurrentMeleeAttack()->MovementCollision( marine, mv, &pm );
	}

	// If we made it all the way, then copy trace end as new player position.
	mv->m_outWishVel = mv->m_vecVelocity;

	if ( pm.fraction == 1)
	{		
		// asw check for stepping down stairs
		if (oldground != NULL)
		{
			Vector vecEndPos;
			VectorCopy( pm.endpos, vecEndPos );
			if ( player->m_Local.m_bAllowAutoMovement )
			{
				vecEndPos.z -= player->m_Local.m_flStepSize;
			}

			trace_t trace;
			TraceMarineBBox( pm.endpos, vecEndPos, MASK_PLAYERSOLID, COLLISION_GROUP_PLAYER_MOVEMENT, trace );

			// If we hit the ground, then use this move
			if ( trace.plane.normal[2] > 0.7 && !trace.startsolid && !trace.allsolid )
			{
				float flStepDist = trace.endpos.z - mv->GetAbsOrigin().z;
				if ( flStepDist != 0.0f )
				{
					mv->m_outStepHeight += flStepDist;
					mv->SetAbsOrigin( trace.endpos );
					// Now pull the base velocity back out.   Base velocity is set if you are on a moving object, like a conveyor (or maybe another monster?)
					VectorSubtract( mv->m_vecVelocity, marine->GetBaseVelocity(), mv->m_vecVelocity );
					if (asw_debug_steps.GetBool())
						Msg("moved down a stair %f\n", flStepDist);
#ifdef GAME_DLL
					if ( bLagComp )
					{
						CASW_Lag_Compensation::FinishLagCompensation();	// undo lag compensation if we need to
					}
#endif
					return;
				}				
			}
		}
		mv->SetAbsOrigin( pm.endpos );
		// Now pull the base velocity back out.   Base velocity is set if you are on a moving object, like a conveyor (or maybe another monster?)
		VectorSubtract( mv->m_vecVelocity, marine->GetBaseVelocity(), mv->m_vecVelocity );
#ifdef GAME_DLL
		if ( bLagComp )
		{
			CASW_Lag_Compensation::FinishLagCompensation();	// undo lag compensation if we need to
		}
#endif
		return;
	}

	// Don't walk up stairs if not on ground.
	if ( oldground == NULL && marine->GetWaterLevel()  == 0 )
	{
		// Now pull the base velocity back out.   Base velocity is set if you are on a moving object, like a conveyor (or maybe another monster?)
		VectorSubtract( mv->m_vecVelocity, marine->GetBaseVelocity(), mv->m_vecVelocity );
#ifdef GAME_DLL
		if ( bLagComp )
		{
			CASW_Lag_Compensation::FinishLagCompensation();	// undo lag compensation if we need to
		}
#endif
		return;
	}

	// If we are jumping out of water, don't do anything more.
	if ( player->GetWaterJumpTime() )         
	{
		// Now pull the base velocity back out.   Base velocity is set if you are on a moving object, like a conveyor (or maybe another monster?)
		VectorSubtract( mv->m_vecVelocity, marine->GetBaseVelocity(), mv->m_vecVelocity );
#ifdef GAME_DLL
		if ( bLagComp )
		{
			CASW_Lag_Compensation::FinishLagCompensation();	// undo lag compensation if we need to
		}
#endif
		return;
	}

	StepMove( dest, pm );

	// Now pull the base velocity back out.   Base velocity is set if you are on a moving object, like a conveyor (or maybe another monster?)
	VectorSubtract( mv->m_vecVelocity, marine->GetBaseVelocity(), mv->m_vecVelocity );
#ifdef GAME_DLL
	if ( bLagComp )
	{
		CASW_Lag_Compensation::FinishLagCompensation();	// undo lag compensation if we need to
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_MarineGameMovement::FullWalkMove( )
{
	if ( marine->GetCurrentMeleeAttack() && marine->m_iMeleeAllowMovement == MELEE_MOVEMENT_FALLING_ONLY )
	{
		mv->m_flForwardMove = 0.0f;
		mv->m_flSideMove = 0.0f;
		mv->m_flUpMove = 0.0f;
		mv->m_bNoAirControl = true;
		mv->m_nButtons &= ~IN_JUMP;
	}

	if ( !CheckWater() ) 
	{
		StartGravity();
	}

	// If we are leaping out of the water, just update the counters.
	if (player->GetWaterJumpTime())
	{
		WaterJump();
		TryPlayerMove();
		// See if we are still in water?
		CheckWater();
		return;
	}

	// If we are swimming in the water, see if we are nudging against a place we can jump up out
	//  of, and, if so, start out jump.  Otherwise, if we are not moving up, then reset jump timer to 0
	if ( marine->GetWaterLevel() >= WL_Waist ) 
	{
		if ( marine->GetWaterLevel() == WL_Waist )
		{
			CheckWaterJump();
		}

			// If we are falling again, then we must not trying to jump out of water any more.
		if ( mv->m_vecVelocity[2] < 0 && 
			 player->GetWaterJumpTime() )
		{
			player->SetWaterJumpTime(0);
		}

		// Was jump button pressed?
		if (mv->m_nButtons & IN_JUMP)
		{
			CheckJumpButton();
		}
		else
		{
			mv->m_nOldButtons &= ~IN_JUMP;
		}

		// Perform regular water movement
		WaterMove();

		// Redetermine position vars
		CategorizePosition();

		// If we are on ground, no downward velocity.
		if ( marine->GetGroundEntity() != NULL )
		{
			mv->m_vecVelocity[2] = 0;			
		}
	}
	else
	// Not fully underwater
	{
		// Was jump button pressed?
		if (mv->m_nButtons & IN_JUMP)
		{
 			CheckJumpButton();
		}
		else
		{
			mv->m_nOldButtons &= ~IN_JUMP;
		}

		// Fricion is handled before we add in any base velocity. That way, if we are on a conveyor, 
		//  we don't slow when standing still, relative to the conveyor.
		if (marine->GetGroundEntity() != NULL)
		{
			mv->m_vecVelocity[2] = 0.0;
			Friction();
		}

		// Make sure velocity is valid.
		CheckVelocity();

		if (marine->GetGroundEntity() != NULL)
		{
			WalkMove();
		}
		else
		{
			if (asw_debug_air_move.GetBool())
				Msg("AirMove: Vel=%f,%f,%f\n", mv->m_vecVelocity[0], mv->m_vecVelocity[1], mv->m_vecVelocity[2]);
			AirMove();  // Take into account movement when in air.
			if (asw_debug_air_move.GetBool() && marine->GetGroundEntity() == NULL)
				Msg("  #3 Vel=%f,%f,%f\n", mv->m_vecVelocity[0], mv->m_vecVelocity[1], mv->m_vecVelocity[2]);
		}

		// Set final flags.
		CategorizePosition();

		// Make sure velocity is valid.
		CheckVelocity();

		// Add any remaining gravitational component.
		if ( !CheckWater() )
		{
			FinishGravity();
		}

		// If we are on ground, no downward velocity.
		if ( marine->GetGroundEntity() != NULL )
		{
			mv->m_vecVelocity[2] = 0;
		}		
		CheckFalling();

	}

	if  ( ( m_nOldWaterLevel == WL_NotInWater && marine->GetWaterLevel() != WL_NotInWater ) ||
		  ( m_nOldWaterLevel != WL_NotInWater && marine->GetWaterLevel() == WL_NotInWater ) )
	{
		PlaySwimSound();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_MarineGameMovement::FullObserverMove( void )
{
	int mode = player->GetObserverMode();

	if ( mode == OBS_MODE_IN_EYE || mode == OBS_MODE_CHASE )
	{
		CBaseEntity * target = player->GetObserverTarget();

		if ( target != NULL )
		{
			mv->SetAbsOrigin( target->EyePosition() );
			mv->m_vecViewAngles = target->EyeAngles();
			mv->m_vecVelocity = target->GetAbsVelocity();
		}

		return;
	}

	if ( mode != OBS_MODE_ROAMING )
	{
		// don't move in fixed or death cam mode
		return;
	}

	if ( sv_specnoclip.GetBool() )
	{
		// roam in noclip mode
		FullNoClipMove( sv_specspeed.GetFloat(), sv_specaccelerate.GetFloat() );
		return;
	}

	// do a full clipped free roam move:

	Vector wishvel;
	Vector forward, right, up;
	Vector wishdir, wishend;
	float wishspeed;

	if ( asw_controls.GetInt() == 1 )
		AngleVectors( ASWGameRules()->GetTopDownMovementAxis(), &forward, &right, &up ); 
	else
		AngleVectors (mv->m_vecViewAngles, &forward, &right, &up);  // Determine movement angles 
	
	// Copy movement amounts

	float factor = sv_specspeed.GetFloat();

	if ( mv->m_nButtons & IN_SPEED )
	{
		factor /= 2.0f;
	}

	float fmove = mv->m_flForwardMove * factor;
	float smove = mv->m_flSideMove * factor;
	
	VectorNormalize (forward);  // Normalize remainder of vectors
	VectorNormalize (right);    // 

	for (int i=0 ; i<3 ; i++)       // Determine x and y parts of velocity
		wishvel[i] = forward[i]*fmove + right[i]*smove;
	wishvel[2] += mv->m_flUpMove;

	VectorCopy (wishvel, wishdir);   // Determine maginitude of speed of move
	wishspeed = VectorNormalize(wishdir);

	//
	// Clamp to server defined max speed
	//

	float maxspeed = sv_maxvelocity.GetFloat(); 


	if (wishspeed > maxspeed )
	{
		VectorScale (wishvel, mv->m_flMaxSpeed/wishspeed, wishvel);
		wishspeed = maxspeed;
	}

	// Set pmove velocity, give observer 50% acceration bonus
	Accelerate ( wishdir, wishspeed, sv_specaccelerate.GetFloat() );

	float spd = VectorLength( mv->m_vecVelocity );
	if (spd < 1.0f)
	{
		mv->m_vecVelocity.Init();
		return;
	}
		
	float friction = asw_marine_friction.GetFloat();
					
	// Add the amount to the drop amount.
	float drop = spd * friction * gpGlobals->frametime;

			// scale the velocity
	float newspeed = spd - drop;

	if (newspeed < 0)
		newspeed = 0;

	// Determine proportion of old speed we are using.
	newspeed /= spd;

	VectorScale( mv->m_vecVelocity, newspeed, mv->m_vecVelocity );

	CheckVelocity();

	TryPlayerMove();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_MarineGameMovement::FullNoClipMove( float factor, float maxacceleration )
{
	Vector wishvel;
	Vector forward, right, up;
	Vector wishdir;
	float wishspeed;
	float maxspeed = asw_sv_maxspeed.GetFloat() * factor;

	if ( asw_controls.GetInt() == 1 )
		AngleVectors( ASWGameRules()->GetTopDownMovementAxis(), &forward, &right, &up ); 
	else
		AngleVectors (mv->m_vecViewAngles, &forward, &right, &up);  // Determine movement angles 

	if ( mv->m_nButtons & IN_SPEED )
	{
		factor /= 2.0f;
	}
	
	// Copy movement amounts
	float fmove = mv->m_flForwardMove * factor;
	float smove = mv->m_flSideMove * factor;
	
	VectorNormalize (forward);  // Normalize remainder of vectors
	VectorNormalize (right);    // 

	for (int i=0 ; i<3 ; i++)       // Determine x and y parts of velocity
		wishvel[i] = forward[i]*fmove + right[i]*smove;
	wishvel[2] += mv->m_flUpMove * factor;

	VectorCopy (wishvel, wishdir);   // Determine maginitude of speed of move
	wishspeed = VectorNormalize(wishdir);

	//
	// Clamp to server defined max speed
	//
	if (wishspeed > maxspeed )
	{
		VectorScale (wishvel, maxspeed/wishspeed, wishvel);
		wishspeed = maxspeed;
	}

	if ( maxacceleration > 0.0 )
	{
		// Set pmove velocity
		Accelerate ( wishdir, wishspeed, maxacceleration );

		float spd = VectorLength( mv->m_vecVelocity );
		if (spd < 1.0f)
		{
			mv->m_vecVelocity.Init();
			return;
		}
		
		// Bleed off some speed, but if we have less than the bleed
		//  threshhold, bleed the theshold amount.
		float control = (spd < maxspeed/4.0) ? maxspeed/4.0 : spd;
		
		float friction = asw_marine_friction.GetFloat() * marine->m_surfaceFriction;
				
		// Add the amount to the drop amount.
		float drop = control * friction * gpGlobals->frametime;

		// scale the velocity
		float newspeed = spd - drop;
		if (newspeed < 0)
			newspeed = 0;

		// Determine proportion of old speed we are using.
		newspeed /= spd;
		VectorScale( mv->m_vecVelocity, newspeed, mv->m_vecVelocity );
	}
	else
	{
		VectorCopy( wishvel, mv->m_vecVelocity );
	}

	// Just move ( don't clip or anything )
	Vector dest;
	VectorMA( mv->GetAbsOrigin(), gpGlobals->frametime, mv->m_vecVelocity, dest );
	mv->SetAbsOrigin( dest );

	// Zero out velocity if in noaccel mode
	if ( maxacceleration < 0.0f )
	{
		mv->m_vecVelocity.Init();
	}
}


void CASW_MarineGameMovement::PlaySwimSound()
{
	MoveHelper()->StartSound( mv->GetAbsOrigin(), "Player.Swim" );
}

ConVar jump_jet_height( "jump_jet_height", "150", FCVAR_CHEAT | FCVAR_REPLICATED );
ConVar jump_jet_forward( "jump_jet_forward", "320", FCVAR_CHEAT | FCVAR_REPLICATED );

void CASW_MarineGameMovement::DoJumpJet()
{
	// In the air now.
	SetGroundEntity( NULL );

	// fixme: should play from the marine, not the player
	Vector vecSrc = mv->GetAbsOrigin();
	player->PlayStepSound( vecSrc, marine->m_pSurfaceData, 1.0, true );

	// fixme: set the animation on the marine
	//MoveHelper()->PlayerSetAnimation( PLAYER_JUMP );
	CASW_Player* pASWPlayer = dynamic_cast<CASW_Player*>(player);
	if (pASWPlayer && pASWPlayer->GetMarine())
	{
		pASWPlayer->GetMarine()->DoAnimationEvent( PLAYERANIMEVENT_JUMP );
	}

	float flMul = sqrt(2 * asw_marine_gravity.GetFloat() * jump_jet_height.GetFloat() );

	// Acclerate upward
	// If we are ducking...
	float startz = mv->m_vecVelocity[2];

	// d = 0.5 * g * t^2		- distance traveled with linear accel
	// t = sqrt(2.0 * 45 / g)	- how long to fall 45 units
	// v = g * t				- velocity at the end (just invert it to jump up that high)
	// v = g * sqrt(2.0 * 45 / g )
	// v^2 = g * g * 2.0 * 45 / g
	// v = sqrt( g * 2.0 * 45 )
	mv->m_vecVelocity[2] = flMul;  // 2 * gravity * height

	Vector vecForward;
	AngleVectors( mv->m_vecViewAngles, &vecForward );

	vecForward.z = 0;
	VectorNormalize( vecForward );
	for ( int iAxis = 0; iAxis < 2 ; ++iAxis )
	{
		mv->m_vecVelocity[ iAxis ] = vecForward[ iAxis ] * jump_jet_forward.GetFloat();
	}

	FinishGravity();

	mv->m_outJumpVel.z += mv->m_vecVelocity[2] - startz;
	mv->m_outStepHeight += 0.15f;

	CASW_Melee_Attack *pAttack = ASWMeleeSystem()->GetMeleeAttackByName( "JumpJetImpact" );
	if ( pAttack )
	{
		marine->m_iOnLandMeleeAttackID = pAttack->m_nAttackID;
	}
}

extern ConVar asw_marine_rolls;

//-----------------------------------------------------------------------------
// Checks to see if we should actually jump 
//-----------------------------------------------------------------------------
bool CASW_MarineGameMovement::CheckJumpButton( void )
{
	// fixme: don't jump if marine is dead
	//if (player->pl.deadflag)
	//{
		//mv->m_nOldButtons |= IN_JUMP ;	// don't jump again until released
		//return false;
	//}

	// See if we are waterjumping.  If so, decrement count and return.
	// fixme: put water jumptime, etc into the marine
	//if (player->m_flWaterJumpTime)
	//{
		//player->m_flWaterJumpTime -= gpGlobals->frametime;
		//if (player->m_flWaterJumpTime < 0)
			//player->m_flWaterJumpTime = 0;
		
		//return false;
	//}

	// No jumping while hacking
	if ( marine->IsHacking() )
		return false;

	if ( asw_marine_rolls.GetBool() )
		return false;

	// If we are in the water most of the way...
	if ( marine->GetWaterLevel() >= 2 )
	{	
		// swimming, not jumping
		SetGroundEntity( NULL );

		if(marine->GetWaterType() == CONTENTS_WATER)    // We move up a certain amount
			mv->m_vecVelocity[2] = 100;
		else if (marine->GetWaterType() == CONTENTS_SLIME)
			mv->m_vecVelocity[2] = 80;
		
		// play swiming sound
		if ( player->GetSwimSoundTime() <= 0 )
		{
			// Don't play sound again for 1 second
			player->SetSwimSoundTime(1000);
			PlaySwimSound();
		}

		return false;
	}

	// No more effect
 	if (marine->GetGroundEntity() == NULL || marine->GetAbsVelocity().z > 0)
	{
		mv->m_nOldButtons |= IN_JUMP;
		return false;		// in air, so no effect
	}

	// Don't allow jumping when the player is in a stasis field.
	// asw: irrelevant for marines
	//if ( player->m_Local.m_bSlowMovement )
		//return false;

	if ( mv->m_nOldButtons & IN_JUMP )
		return false;		// don't pogo stick

	if (marine->GetFlags() & FL_FROZEN)	// no jumping when frozen
		return false;

	// Cannot jump will in the unduck transition.
	// asw: irrelevant for marines (since they don't duck atm)
	//if ( player->m_Local.m_bDucking && (  player->GetFlags() & FL_DUCKING ) )
		//return false;

	// Still updating the eye position.
	// asw: irrelevant for marines (since they don't duck atm)
	//if ( player->m_Local.m_nDuckJumpTimeMsecs > 0.0f )
		//return false;


	// In the air now.
    SetGroundEntity( NULL );
	
	// fixme: should play from the marine, not the player
	Vector vecSrc = mv->GetAbsOrigin();
	player->PlayStepSound( vecSrc, marine->m_pSurfaceData, 1.0, true );
	
	// fixme: set the animation on the marine
	//MoveHelper()->PlayerSetAnimation( PLAYER_JUMP );
	CASW_Player* pASWPlayer = dynamic_cast<CASW_Player*>(player);
	if (pASWPlayer && pASWPlayer->GetMarine())
	{
		pASWPlayer->GetMarine()->DoAnimationEvent( PLAYERANIMEVENT_JUMP );
	}

	float flGroundFactor = 1.0f;
	if (marine->m_pSurfaceData)
	{
		flGroundFactor = marine->m_pSurfaceData->game.jumpFactor; 
	}

	float flMul;
	// asw comment
	/*
	if ( g_bMovementOptimizations )
	{
#if defined(HL2_DLL) || defined(HL2_CLIENT_DLL)
		Assert( sv_gravity.GetFloat() == 600.0f );
		flMul = 160.0f;	// approx. 21 units.
#else
		Assert( sv_gravity.GetFloat() == 800.0f );
		flMul = 268.3281572999747f;
#endif

	}
	else*/
	{
		flMul = sqrt(2 * asw_marine_gravity.GetFloat() * GAMEMOVEMENT_ASW_JUMP_HEIGHT);
	}

	// Acclerate upward
	// If we are ducking...
	float startz = mv->m_vecVelocity[2];
	
	if ( (  player->m_Local.m_bDucking ) || (  marine->GetFlags() & FL_DUCKING ) )
	{
		// d = 0.5 * g * t^2		- distance traveled with linear accel
		// t = sqrt(2.0 * 45 / g)	- how long to fall 45 units
		// v = g * t				- velocity at the end (just invert it to jump up that high)
		// v = g * sqrt(2.0 * 45 / g )
		// v^2 = g * g * 2.0 * 45 / g
		// v = sqrt( g * 2.0 * 45 )
		mv->m_vecVelocity[2] = flGroundFactor * flMul;  // 2 * gravity * height
	}
	else
	{
		mv->m_vecVelocity[2] += flGroundFactor * flMul;  // 2 * gravity * height
	}

	// reduce the marine's x/y velocity some
	mv->m_vecVelocity[0] *= ASW_JUMP_LATERAL_SCALE;
	mv->m_vecVelocity[1] *= ASW_JUMP_LATERAL_SCALE;	

	// Add a little forward velocity based on your current forward velocity - if you are not sprinting.
	// asw: no forward boost
	/*
#if defined( HL2_DLL ) || defined( HL2_CLIENT_DLL )
	CHLMoveData *pMoveData = ( CHLMoveData* )mv;
	Vector vecForward;
	AngleVectors( mv->m_vecViewAngles, &vecForward );
	vecForward.z = 0;
	VectorNormalize( vecForward );
	if ( !pMoveData->m_bIsSprinting && !player->m_Local.m_bDucked )
	{
		for ( int iAxis = 0; iAxis < 2 ; ++iAxis )
		{
			vecForward[iAxis] *= ( mv->m_flForwardMove * 0.5f );
//			vecForward[iAxis] *= ( mv->m_flForwardMove * jumpforwardscale.GetFloat() );
		}
	}
	else
	{
		for ( int iAxis = 0; iAxis < 2 ; ++iAxis )
		{
			vecForward[iAxis] *= ( mv->m_flForwardMove * 0.1f );
//			vecForward[iAxis] *= ( mv->m_flForwardMove * jumpforwardsprintscale.GetFloat() );
		}
	}
	VectorAdd( vecForward, mv->m_vecVelocity, mv->m_vecVelocity );
#endif
	*/

	FinishGravity();
#ifdef CLIENT_DLL
	//Msg("  [C] %f Jumping! v.z=%f\n", gpGlobals->curtime, marine->GetAbsVelocity().z);
#else
	//Msg("[S] %f Jumping! v.z=%f\n", gpGlobals->curtime, marine->GetAbsVelocity().z);
#endif

  	mv->m_outJumpVel.z += mv->m_vecVelocity[2] - startz;
	mv->m_outStepHeight += 0.15f;

	// Set jump time.
	player->m_Local.m_nJumpTimeMsecs = GAMEMOVEMENT_JUMP_TIME;
	player->m_Local.m_bInDuckJump = true;

	// Flag that we jumped.
	mv->m_nOldButtons |= IN_JUMP;	// don't jump again until released

	//Msg("jumping. m_outJumpVel.z = %f m_outStepHeight = %f vecvel.z = %f\n",
		 //mv->m_outJumpVel.z, mv->m_outStepHeight, mv->m_vecVelocity[2]);

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_MarineGameMovement::FullLadderMove()
{
	CheckWater();

	// Was jump button pressed? If so, set velocity to 270 away from ladder.  
	if ( mv->m_nButtons & IN_JUMP )
	{
		CheckJumpButton();
	}
	else
	{
		mv->m_nOldButtons &= ~IN_JUMP;
	}
	
	// Perform the move accounting for any base velocity.
	VectorAdd (mv->m_vecVelocity, marine->GetBaseVelocity(), mv->m_vecVelocity);
	TryPlayerMove();
	VectorSubtract (mv->m_vecVelocity, marine->GetBaseVelocity(), mv->m_vecVelocity);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CASW_MarineGameMovement::TryPlayerMove( Vector *pFirstDest, trace_t *pFirstTrace )
{
	int			bumpcount, numbumps;
	Vector		dir;
	float		d;
	int			numplanes;
	Vector		planes[MAX_CLIP_PLANES];
	Vector		primal_velocity, original_velocity;
	Vector      new_velocity;
	int			i, j;
	trace_t	pm;
	Vector		end;
	float		time_left, allFraction;
	int			blocked;		
	
	numbumps  = 4;           // Bump up to four times
	
	blocked   = 0;           // Assume not blocked
	numplanes = 0;           //  and not sliding along any planes

	VectorCopy (mv->m_vecVelocity, original_velocity);  // Store original velocity
	VectorCopy (mv->m_vecVelocity, primal_velocity);
	
	allFraction = 0;
	time_left = gpGlobals->frametime;   // Total time for this movement operation.

	new_velocity.Init();

	for (bumpcount=0 ; bumpcount < numbumps; bumpcount++)
	{
		if ( mv->m_vecVelocity.Length() == 0.0 )
			break;

		// Assume we can move all the way from the current origin to the
		//  end point.
		VectorMA( mv->GetAbsOrigin(), time_left, mv->m_vecVelocity, end );

		// See if we can make it from origin to end point.
		if ( g_bMovementOptimizations )
		{
			// If their velocity Z is 0, then we can avoid an extra trace here during WalkMove.
			if ( pFirstDest && end == *pFirstDest )
				pm = *pFirstTrace;
			else
				TraceMarineBBox( mv->GetAbsOrigin(), end, MASK_PLAYERSOLID, COLLISION_GROUP_PLAYER_MOVEMENT, pm );
		}
		else
		{
			TraceMarineBBox( mv->GetAbsOrigin(), end, MASK_PLAYERSOLID, COLLISION_GROUP_PLAYER_MOVEMENT, pm );
		}

		allFraction += pm.fraction;

		// If we started in a solid object, or we were in solid space
		//  the whole way, zero out our velocity and return that we
		//  are blocked by floor and wall.
		if (pm.allsolid)
		{	
			// entity is trapped in another solid
			VectorCopy (vec3_origin, mv->m_vecVelocity);
			if (asw_debug_air_move.GetBool() && marine->GetGroundEntity() == NULL)
				Msg("  #T1  Stuck in a solid\n");
			return 4;
		}

		// If we moved some portion of the total distance, then
		//  copy the end position into the pmove.origin and 
		//  zero the plane counter.
		if( pm.fraction > 0 )
		{	
			// actually covered some distance
			if (asw_debug_air_move.GetBool() && marine->GetGroundEntity() == NULL)
				Msg("  #T1.5  Moving marine absorigin. vel = %f,%f,%f\n", mv->m_vecVelocity[0], mv->m_vecVelocity[1], mv->m_vecVelocity[2]);
			mv->SetAbsOrigin( pm.endpos );
			VectorCopy (mv->m_vecVelocity, original_velocity);
			numplanes = 0;
		}

		// If we covered the entire distance, we are done
		//  and can return.
		if (pm.fraction == 1)
		{
			if (asw_debug_air_move.GetBool() && marine->GetGroundEntity() == NULL)
				Msg("  #T2  Moved the entire distance\n");
			 break;		// moved the entire distance
		}

		// Save entity that blocked us (since fraction was < 1.0)
		//  for contact
		// Add it if it's not already in the list!!!
		MoveHelper( )->AddToTouched( pm, mv->m_vecVelocity );

		// If the plane we hit has a high z component in the normal, then
		//  it's probably a floor
		if (pm.plane.normal[2] > 0.7)
		{
			blocked |= 1;		// floor
		}
		// If the plane has a zero z component in the normal, then it's a 
		//  step or wall
		if (!pm.plane.normal[2])
		{
			blocked |= 2;		// step / wall
		}

		// Reduce amount of m_flFrameTime left by total time left * fraction
		//  that we covered.
		time_left -= time_left * pm.fraction;

		// Did we run out of planes to clip against?
		if (numplanes >= MAX_CLIP_PLANES)
		{	
			// this shouldn't really happen
			//  Stop our movement if so.
			VectorCopy (vec3_origin, mv->m_vecVelocity);
			//Con_DPrintf("Too many planes 4\n");

			break;
		}

		// Set up next clipping plane
		VectorCopy (pm.plane.normal, planes[numplanes]);
		numplanes++;

		// modify original_velocity so it parallels all of the clip planes
		//

		// relfect player velocity 
		// Only give this a try for first impact plane because you can get yourself stuck in an acute corner by jumping in place
		//  and pressing forward and nobody was really using this bounce/reflection feature anyway...
		if ( numplanes == 1 &&
			marine->GetMoveType() == MOVETYPE_WALK &&
			marine->GetGroundEntity() == NULL )	
		{
			for ( i = 0; i < numplanes; i++ )
			{
				if ( planes[i][2] > 0.7  )
				{
					// floor or slope
					if (asw_debug_air_move.GetBool() && marine->GetGroundEntity() == NULL)
						Msg("  #T3  On a floor or slope, clipping velocity to the 1 plane\n");
					ClipVelocity( original_velocity, planes[i], new_velocity, 1 );
					VectorCopy( new_velocity, original_velocity );
				}
				else
				{
					if (asw_debug_air_move.GetBool() && marine->GetGroundEntity() == NULL)
						Msg("  #T4  Not on a floor or slope, clipping velocity to the some sv_bounce reflected amount around 1 plane\n");
					ClipVelocity( original_velocity, planes[i], new_velocity, 1.0 + sv_bounce.GetFloat() * (1 - marine->m_surfaceFriction) );
				}
			}

			VectorCopy( new_velocity, mv->m_vecVelocity );
			VectorCopy( new_velocity, original_velocity );
		}
		else
		{
			if (asw_debug_air_move.GetBool() && marine->GetGroundEntity() == NULL)
				Msg("  #T5  Clipping velocity to the %d planes\n", numplanes);
			for (i=0 ; i < numplanes ; i++)
			{
				ClipVelocity (
					original_velocity,
					planes[i],
					mv->m_vecVelocity,
					1);

				for (j=0 ; j<numplanes ; j++)
					if (j != i)
					{
						// Are we now moving against this plane?
						if (mv->m_vecVelocity.Dot(planes[j]) < 0)
							break;	// not ok
					}
				if (j == numplanes)  // Didn't have to clip, so we're ok
					break;
			}
			
			// Did we go all the way through plane set
			if (i != numplanes)
			{	// go along this plane
				// pmove.velocity is set in clipping call, no need to set again.
				;  
			}
			else
			{	// go along the crease
				if (numplanes != 2)
				{
					VectorCopy (vec3_origin, mv->m_vecVelocity);
					break;
				}
				CrossProduct (planes[0], planes[1], dir);
				dir.NormalizeInPlace();	// asw - added sept 2nd 06
				d = dir.Dot(mv->m_vecVelocity);
				VectorScale (dir, d, mv->m_vecVelocity );
				if (asw_debug_air_move.GetBool() && marine->GetGroundEntity() == NULL)
					Msg("  #T6  Going along the crease\n", numplanes);
			}

			//
			// if original velocity is against the original velocity, stop dead
			// to avoid tiny occilations in sloping corners
			//
			d = mv->m_vecVelocity.Dot(primal_velocity);
			if (d <= 0)
			{
				//Con_DPrintf("Back\n");
				if (asw_debug_air_move.GetBool() && marine->GetGroundEntity() == NULL)
					Msg("  #T7  Going opposite, so stopping\n", numplanes);
				VectorCopy (vec3_origin, mv->m_vecVelocity);
				break;
			}
		}
	}

	if ( allFraction == 0 )
	{
		if (asw_debug_air_move.GetBool() && marine->GetGroundEntity() == NULL)
			Msg("  #T8  Going nowhere, so stopping\n", numplanes);
		VectorCopy (vec3_origin, mv->m_vecVelocity);
	}

	return blocked;
}


//-----------------------------------------------------------------------------
// Purpose: Determine whether or not the player is on a ladder (physprop or world).
//-----------------------------------------------------------------------------
inline bool CASW_MarineGameMovement::OnLadder( trace_t &trace )
{
	if ( trace.contents & CONTENTS_LADDER )
		return true;

	IPhysicsSurfaceProps *pPhysProps = MoveHelper( )->GetSurfaceProps();
	if ( pPhysProps )
	{
		surfacedata_t *pSurfaceData = pPhysProps->GetSurfaceData( trace.surface.surfaceProps );
		if ( pSurfaceData )
		{
			if ( pSurfaceData->game.climbable != 0 )
				return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CASW_MarineGameMovement::LadderMove( void )
{
	trace_t pm;
	bool onFloor;
	Vector floor;
	Vector wishdir;
	Vector end;

	if ( marine->GetMoveType() == MOVETYPE_NOCLIP )
		return false;

	// If I'm already moving on a ladder, use the previous ladder direction
	if ( marine->GetMoveType() == MOVETYPE_LADDER )
	{
		wishdir = -player->m_vecLadderNormal.Get();
	}
	else
	{
		// otherwise, use the direction player is attempting to move
		if ( mv->m_flForwardMove || mv->m_flSideMove )
		{
			for (int i=0 ; i<3 ; i++)       // Determine x and y parts of velocity
				wishdir[i] = m_vecForward[i]*mv->m_flForwardMove + m_vecRight[i]*mv->m_flSideMove;

			VectorNormalize(wishdir);
		}
		else
		{
			// Player is not attempting to move, no ladder behavior
			return false;
		}
	}

	// wishdir points toward the ladder if any exists
	VectorMA( mv->GetAbsOrigin(), 2, wishdir, end );
	TraceMarineBBox( mv->GetAbsOrigin(), end, MASK_PLAYERSOLID, COLLISION_GROUP_PLAYER_MOVEMENT, pm );

	// no ladder in that direction, return
	if ( pm.fraction == 1.0f || !OnLadder( pm ) )
		return false;

	marine->SetMoveType( MOVETYPE_LADDER );
	marine->SetMoveCollide( MOVECOLLIDE_DEFAULT );

	player->m_vecLadderNormal = pm.plane.normal;

	// On ladder, convert movement to be relative to the ladder

	VectorCopy( mv->GetAbsOrigin(), floor );
	floor[2] += GetPlayerMins()[2] - 1;

	if( enginetrace->GetPointContents( floor ) == CONTENTS_SOLID )
	{
		onFloor = true;
	}
	else
	{
		onFloor = false;
	}

	marine->SetGravity( 0 );
	
	float forwardSpeed = 0, rightSpeed = 0;
	if ( mv->m_nButtons & IN_BACK )
		forwardSpeed -= MAX_CLIMB_SPEED;
	
	if ( mv->m_nButtons & IN_FORWARD )
		forwardSpeed += MAX_CLIMB_SPEED;
	
	if ( mv->m_nButtons & IN_MOVELEFT )
		rightSpeed -= MAX_CLIMB_SPEED;
	
	if ( mv->m_nButtons & IN_MOVERIGHT )
		rightSpeed += MAX_CLIMB_SPEED;

	if ( mv->m_nButtons & IN_JUMP )
	{
		marine->SetMoveType( MOVETYPE_WALK );
		marine->SetMoveCollide( MOVECOLLIDE_DEFAULT );

		VectorScale( pm.plane.normal, 270, mv->m_vecVelocity );
	}
	else
	{
		if ( forwardSpeed != 0 || rightSpeed != 0 )
		{
			Vector velocity, perp, cross, lateral, tmp;

			//ALERT(at_console, "pev %.2f %.2f %.2f - ",
			//	pev->velocity.x, pev->velocity.y, pev->velocity.z);
			// Calculate player's intended velocity
			//Vector velocity = (forward * gpGlobals->v_forward) + (right * gpGlobals->v_right);
			VectorScale( m_vecForward, forwardSpeed, velocity );
			VectorMA( velocity, rightSpeed, m_vecRight, velocity );

			// Perpendicular in the ladder plane
			VectorCopy( vec3_origin, tmp );
			tmp[2] = 1;
			CrossProduct( tmp, pm.plane.normal, perp );
			VectorNormalize( perp );

			// decompose velocity into ladder plane
			float normal = DotProduct( velocity, pm.plane.normal );

			// This is the velocity into the face of the ladder
			VectorScale( pm.plane.normal, normal, cross );

			// This is the player's additional velocity
			VectorSubtract( velocity, cross, lateral );

			// This turns the velocity into the face of the ladder into velocity that
			// is roughly vertically perpendicular to the face of the ladder.
			// NOTE: It IS possible to face up and move down or face down and move up
			// because the velocity is a sum of the directional velocity and the converted
			// velocity through the face of the ladder -- by design.
			CrossProduct( pm.plane.normal, perp, tmp );
			VectorMA( lateral, -normal, tmp, mv->m_vecVelocity );

			if ( onFloor && normal > 0 )	// On ground moving away from the ladder
			{
				VectorMA( mv->m_vecVelocity, MAX_CLIMB_SPEED, pm.plane.normal, mv->m_vecVelocity );
			}
			//pev->velocity = lateral - (CrossProduct( trace.vecPlaneNormal, perp ) * normal);
		}
		else
		{
			mv->m_vecVelocity.Init();
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : axis - 
// Output : const char
//-----------------------------------------------------------------------------
extern const char *DescribeAxis( int axis );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_MarineGameMovement::CheckVelocity( void )
{
	int i;

	//
	// bound velocity
	//
	Vector origin = mv->GetAbsOrigin();
	bool bFixedOrigin = false;
	for (i=0; i < 3; i++)
	{
		// See if it's bogus.
		if (IS_NAN(mv->m_vecVelocity[i]))
		{
			DevMsg( 1, "PM  Got a NaN velocity %s\n", DescribeAxis( i ) );
			mv->m_vecVelocity[i] = 0;
		}
		if (IS_NAN(origin[i]))
		{
			DevMsg( 1, "PM  Got a NaN origin on %s\n", DescribeAxis( i ) );
			origin[i] = 0;
			bFixedOrigin = true;
		}

		// Bound it.
		if (mv->m_vecVelocity[i] > sv_maxvelocity.GetFloat()) 
		{
			DevMsg( 1, "PM  Got a velocity too high on %s %f\n", DescribeAxis( i ), mv->m_vecVelocity[i] );
			mv->m_vecVelocity[i] = sv_maxvelocity.GetFloat();
		}
		else if (mv->m_vecVelocity[i] < -sv_maxvelocity.GetFloat())
		{
			DevMsg( 1, "PM  Got a velocity too low on %s %f\n", DescribeAxis( i ), mv->m_vecVelocity[i] );
			mv->m_vecVelocity[i] = -sv_maxvelocity.GetFloat();
		}
	}
	if ( bFixedOrigin )
		mv->SetAbsOrigin( origin );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_MarineGameMovement::AddGravity( void )
{
	float ent_gravity;

	if ( player->GetWaterJumpTime() )
		return;

	if (marine->GetGravity())
		ent_gravity = marine->GetGravity();
	else
		ent_gravity = 1.0;

	// Add gravity incorrectly
	mv->m_vecVelocity[2] -= (ent_gravity * asw_marine_gravity.GetFloat() * gpGlobals->frametime);
	mv->m_vecVelocity[2] += marine->GetBaseVelocity()[2] * gpGlobals->frametime;
	Vector temp = marine->GetBaseVelocity();
	temp[2] = 0;
	marine->SetBaseVelocity( temp );
	
	CheckVelocity();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : push - 
// Output : trace_t
//-----------------------------------------------------------------------------
void CASW_MarineGameMovement::PushEntity( Vector& push, trace_t *pTrace )
{
	Vector	end;
		 
	VectorAdd (mv->GetAbsOrigin(), push, end);
	TraceMarineBBox( mv->GetAbsOrigin(), end, MASK_PLAYERSOLID, COLLISION_GROUP_PLAYER_MOVEMENT, *pTrace );
	mv->SetAbsOrigin( pTrace->endpos );

	// So we can run impact function afterwards.
	// If
	if ( pTrace->fraction < 1.0 && !pTrace->allsolid )
	{
		MoveHelper( )->AddToTouched( *pTrace, mv->m_vecVelocity );
	}
}	


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : in - 
//			normal - 
//			out - 
//			overbounce - 
// Output : int
//-----------------------------------------------------------------------------
int CASW_MarineGameMovement::ClipVelocity( Vector& in, Vector& normal, Vector& out, float overbounce )
{
	float	backoff;
	float	change;
	float angle;
	int		i, blocked;
	
	angle = normal[ 2 ];

	blocked = 0x00;         // Assume unblocked.
	if (angle > 0)			// If the plane that is blocking us has a positive z component, then assume it's a floor.
		blocked |= 0x01;	// 
	if (!angle)				// If the plane has no Z, it is vertical (wall/step)
		blocked |= 0x02;	// 
	

	// Determine how far along plane to slide based on incoming direction.
	backoff = DotProduct (in, normal) * overbounce;

	for (i=0 ; i<3 ; i++)
	{
		change = normal[i]*backoff;
		out[i] = in[i] - change; 
		// If out velocity is too small, zero it out.
		if (out[i] > -STOP_EPSILON && out[i] < STOP_EPSILON)
			out[i] = 0;
	}
	
#if 0
	// slight adjustment - hopefully to adjust for displacement surfaces
	float adjust = DotProduct( out, normal );
	if( adjust < 0.0f )
	{
		out += ( normal * -adjust );
//		Msg( "Adjustment = %lf\n", adjust );
	}
#endif
	// asw: added sept 2nd 06
	// iterate once to make sure we aren't still moving through the plane
	float adjust = DotProduct( out, normal );
	if( adjust < 0.0f )
	{
		out -= ( normal * adjust );
//		Msg( "Adjustment = %lf\n", adjust );
	}



	// Return blocking flags.
	return blocked;
}

//-----------------------------------------------------------------------------
// Purpose: Computes roll angle for a certain movement direction and velocity
// Input  : angles - 
//			velocity - 
//			rollangle - 
//			rollspeed - 
// Output : float 
//-----------------------------------------------------------------------------
float CASW_MarineGameMovement::CalcRoll ( const QAngle &angles, const Vector &velocity, float rollangle, float rollspeed )
{
	float   sign;
	float   side;
	float   value;
	Vector  forward, right, up;
	
	AngleVectors (angles, &forward, &right, &up);
	
	side = DotProduct (velocity, right);
	sign = side < 0 ? -1 : 1;
	side = fabs(side);
	value = rollangle;
	if (side < rollspeed)
	{
		side = side * value / rollspeed;
	}
	else
	{
		side = value;
	}
	return side*sign;
}

#define CHECKSTUCK_MINTIME 0.05  // Don't check again too quickly.

static Vector rgv3tMarineStuckTable[54];

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CreateMarineStuckTable( void )
{
	float x, y, z;
	int idx;
	int i;
	float zi[3];
	static int firsttime = 1;

	if ( !firsttime )
		return;

	firsttime = 0;

	memset(rgv3tMarineStuckTable, 0, sizeof(rgv3tMarineStuckTable));

	idx = 0;
	// Little Moves.
	x = y = 0;
	// Z moves
	for (z = -0.125 ; z <= 0.125 ; z += 0.125)
	{
		rgv3tMarineStuckTable[idx][0] = x;
		rgv3tMarineStuckTable[idx][1] = y;
		rgv3tMarineStuckTable[idx][2] = z;
		idx++;
	}
	x = z = 0;
	// Y moves
	for (y = -0.125 ; y <= 0.125 ; y += 0.125)
	{
		rgv3tMarineStuckTable[idx][0] = x;
		rgv3tMarineStuckTable[idx][1] = y;
		rgv3tMarineStuckTable[idx][2] = z;
		idx++;
	}
	y = z = 0;
	// X moves
	for (x = -0.125 ; x <= 0.125 ; x += 0.125)
	{
		rgv3tMarineStuckTable[idx][0] = x;
		rgv3tMarineStuckTable[idx][1] = y;
		rgv3tMarineStuckTable[idx][2] = z;
		idx++;
	}

	// Remaining multi axis nudges.
	for ( x = - 0.125; x <= 0.125; x += 0.250 )
	{
		for ( y = - 0.125; y <= 0.125; y += 0.250 )
		{
			for ( z = - 0.125; z <= 0.125; z += 0.250 )
			{
				rgv3tMarineStuckTable[idx][0] = x;
				rgv3tMarineStuckTable[idx][1] = y;
				rgv3tMarineStuckTable[idx][2] = z;
				idx++;
			}
		}}

	// Big Moves.
	x = y = 0;
	zi[0] = 0.0f;
	zi[1] = 1.0f;
	zi[2] = 6.0f;

	for (i = 0; i < 3; i++)
	{
		// Z moves
		z = zi[i];
		rgv3tMarineStuckTable[idx][0] = x;
		rgv3tMarineStuckTable[idx][1] = y;
		rgv3tMarineStuckTable[idx][2] = z;
		idx++;
	}

	x = z = 0;

	// Y moves
	for (y = -2.0f ; y <= 2.0f ; y += 2.0)
	{
		rgv3tMarineStuckTable[idx][0] = x;
		rgv3tMarineStuckTable[idx][1] = y;
		rgv3tMarineStuckTable[idx][2] = z;
		idx++;
	}
	y = z = 0;
	// X moves
	for (x = -2.0f ; x <= 2.0f ; x += 2.0f)
	{
		rgv3tMarineStuckTable[idx][0] = x;
		rgv3tMarineStuckTable[idx][1] = y;
		rgv3tMarineStuckTable[idx][2] = z;
		idx++;
	}

	// Remaining multi axis nudges.
	for (i = 0 ; i < 3; i++)
	{
		z = zi[i];
		
		for (x = -2.0f ; x <= 2.0f ; x += 2.0f)
		{
			for (y = -2.0f ; y <= 2.0f ; y += 2.0)
			{
				rgv3tMarineStuckTable[idx][0] = x;
				rgv3tMarineStuckTable[idx][1] = y;
				rgv3tMarineStuckTable[idx][2] = z;
				idx++;
			}
		}
	}
	Assert( idx < sizeof(rgv3tMarineStuckTable)/sizeof(rgv3tMarineStuckTable[0]));
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : nIndex - 
//			server - 
//			offset - 
// Output : int
//-----------------------------------------------------------------------------
int MarineGetRandomStuckOffsets( CBasePlayer *pPlayer, Vector& offset)
{
 // Last time we did a full
	int idx;
	idx = pPlayer->m_StuckLast;

	VectorCopy(rgv3tMarineStuckTable[idx % 54], offset);

	return (idx % 54);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : nIndex - 
//			server - 
//-----------------------------------------------------------------------------
void MarineResetStuckOffsets( CBasePlayer *pPlayer )
{
	pPlayer->m_StuckLast = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &input - 
// Output : int
//-----------------------------------------------------------------------------
int CASW_MarineGameMovement::CheckStuck( void )
{
	Vector base;
	Vector offset;
	Vector test;
	EntityHandle_t hitent;
	int idx;
	float fTime;
	//int i;
	trace_t traceresult;

	CreateMarineStuckTable();

	hitent = TestPlayerPosition( mv->GetAbsOrigin(), COLLISION_GROUP_PLAYER_MOVEMENT, traceresult );
	/*
#ifdef CLIENT_DLL
	Msg("[C] CS pos=%f,%f,%f mins=%f,%f,%f maxs=%f,%f,%f he=%i/%s\n",
		mv->m_vecAbsOrigin.x, mv->m_vecAbsOrigin.y, mv->m_vecAbsOrigin.z, 
		GetPlayerMins().x, GetPlayerMins().y, GetPlayerMins().z, 
		GetPlayerMaxs().x, GetPlayerMaxs().y, GetPlayerMaxs().z, 
		hitent , MoveHelper()->GetName(hitent));
#else
	Msg("[S] CS pos=%f,%f,%f mins=%f,%f,%f maxs=%f,%f,%f he=%i/%s\n",
		mv->m_vecAbsOrigin.x, mv->m_vecAbsOrigin.y, mv->m_vecAbsOrigin.z, 
		GetPlayerMins().x, GetPlayerMins().y, GetPlayerMins().z, 
		GetPlayerMaxs().x, GetPlayerMaxs().y, GetPlayerMaxs().z, 
		hitent , MoveHelper()->GetName(hitent));
#endif
		*/
	if ( hitent == INVALID_ENTITY_HANDLE )
	{
		MarineResetStuckOffsets( player );
		return 0;
	}

	// Deal with stuckness...
#ifndef DEDICATED
	if ( developer.GetBool() )
	{
		bool isServer = player->IsServer();
		engine->Con_NPrintf( isServer, "%s stuck on object %i/%s", 
			isServer ? "server" : "client",
			hitent , MoveHelper()->GetName(hitent) );
	}
#endif

#ifndef CLIENT_DLL
	if ( marine )
	{
		marine->m_fLastStuckTime = gpGlobals->curtime;	// let the marine know he's stuck, so he can enable his vphysics shadow (to push away any offending phys objects which might be making us stuck)
		if ( marine->m_flFirstStuckTime == 0.0f )
		{
			marine->m_flFirstStuckTime = gpGlobals->curtime;
		}
		if ( gpGlobals->curtime - marine->m_flFirstStuckTime > 1.0f )
		{
			bool bTeleportMarine = true;
			edict_t* pEdict = gEntList.GetEdict( hitent );
			if ( pEdict )
			{
				CBaseEntity *pEntity = CBaseEntity::Instance( pEdict );
				if ( pEntity && pEntity->m_takedamage == DAMAGE_YES && pEntity->Classify() == CLASS_ASW_PHYSICS_PROP )
				{
					const char *szModelName = STRING( pEntity->GetModelName() );
					if ( szModelName && !CanMarineGetStuckOnProp( szModelName ) )
					{
						bTeleportMarine = false;		// marine is stuck in a breakable crate, don't teleport him
					}
				}
			}
			if ( bTeleportMarine )
			{
				marine->TeleportStuckMarine();
				marine->m_flFirstStuckTime = 0.0f;
				mv->SetAbsOrigin( marine->GetAbsOrigin() );
				return 0;
			}
			else
			{
				marine->m_flFirstStuckTime = gpGlobals->curtime;
			}
		}
	}
#endif

	VectorCopy( mv->GetAbsOrigin(), base );

	// 
	// Deal with precision error in network.
	// 
	// World or BSP model
	if ( !player->IsServer() )
	{
		if ( MoveHelper()->IsWorldEntity( hitent ) )
		{
			//Msg("StuckWorld: y = %f :", mv->m_vecAbsOrigin.y);
			int nReps = 0;
			MarineResetStuckOffsets( player );
			do 
			{
				player->m_StuckLast = MarineGetRandomStuckOffsets( player, offset) + 1;

				VectorAdd(base, offset, test);
				//Msg("Off:%f,%f,%f ", VectorExpand(offset));
				if (TestPlayerPosition( test, COLLISION_GROUP_PLAYER_MOVEMENT, traceresult ) == INVALID_ENTITY_HANDLE)
				{
					//Msg(" FREE!\n");
					MarineResetStuckOffsets( player );
					mv->SetAbsOrigin( test );
					return 0;
				}
				nReps++;
			} while (nReps < 54);
			//Msg(" Failed\n");
		}
	}

	// Only an issue on the client.
	idx = player->IsServer() ? 0 : 1;

	fTime = Plat_FloatTime();
	// Too soon?
	if ( m_flStuckCheckTime[player->entindex()][idx] >=  fTime - CHECKSTUCK_MINTIME )
	{
		return 1;
	}
	m_flStuckCheckTime[player->entindex()][idx] = fTime;

	MoveHelper( )->AddToTouched( traceresult, mv->m_vecVelocity );

	player->m_StuckLast = MarineGetRandomStuckOffsets( player, offset) + 1;

	VectorAdd(base, offset, test);

	if (TestPlayerPosition( test, COLLISION_GROUP_PLAYER_MOVEMENT, traceresult ) == INVALID_ENTITY_HANDLE)
	{
		MarineResetStuckOffsets( player );

		if ( player->m_StuckLast - 1 >= 27 )
		{
			mv->SetAbsOrigin( test );
		}
		return 0;
	}

	/*
	// If player is flailing while stuck in another player ( should never happen ), then see
	//  if we can't "unstick" them forceably.
	if ( mv->m_nButtons & ( IN_JUMP | IN_DUCK | IN_ATTACK ) && ( pmv->physents[ hitent ].player != 0 ) )
	{
		float x, y, z;
		float xystep = 8.0;
		float zstep = 18.0;
		float xyminmax = xystep;
		float zminmax = 4 * zstep;
		
		for ( z = 0; z <= zminmax; z += zstep )
		{
			for ( x = -xyminmax; x <= xyminmax; x += xystep )
			{
				for ( y = -xyminmax; y <= xyminmax; y += xystep )
				{
					VectorCopy( base, test );
					test[0] += x;
					test[1] += y;
					test[2] += z;

					if ( pmv->TestPosition ( test, NULL ) == -1 )
					{
						VectorCopy( test, mv->m_vecAbsOrigin );
						return 0;
					}
				}
			}
		}
	}
	*/
	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : bool
//-----------------------------------------------------------------------------
bool CASW_MarineGameMovement::InWater( void )
{
	return ( marine->GetWaterLevel() > WL_Feet );
}


void CASW_MarineGameMovement::ResetGetPointContentsCache()
{
	m_CachedGetPointContents = -9999;
}


int CASW_MarineGameMovement::GetPointContentsCached( const Vector &point )
{
	if ( g_bMovementOptimizations ) 
	{
		if ( m_CachedGetPointContents == -9999 || point.DistToSqr( m_CachedGetPointContentsPoint ) > 1 )
		{
			m_CachedGetPointContents = enginetrace->GetPointContents ( point );
			m_CachedGetPointContentsPoint = point;
		}
		
		return m_CachedGetPointContents;
	}
	else
	{
		return enginetrace->GetPointContents ( point );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &input - 
// Output : bool
//-----------------------------------------------------------------------------
bool CASW_MarineGameMovement::CheckWater( void )
{
	Vector	point;
	int		cont;

	// Pick a spot just above the players feet.
	point[0] = mv->GetAbsOrigin()[0] + (GetPlayerMins()[0] + GetPlayerMaxs()[0]) * 0.5;
	point[1] = mv->GetAbsOrigin()[1] + (GetPlayerMins()[1] + GetPlayerMaxs()[1]) * 0.5;
	point[2] = mv->GetAbsOrigin()[2] + GetPlayerMins()[2] + 1;
	
	// Assume that we are not in water at all.
	marine->SetWaterLevel( WL_NotInWater );
	marine->SetWaterType( CONTENTS_EMPTY );

	// Grab point contents.
	cont = GetPointContentsCached( point );	
	
	// Are we under water? (not solid and not empty?)
	if ( cont & MASK_WATER )
	{
		// Set water type
		marine->SetWaterType( cont );

		// We are at least at level one
		marine->SetWaterLevel( WL_Feet );

		// Now check a point that is at the player hull midpoint.
		point[2] = mv->GetAbsOrigin()[2] + (GetPlayerMins()[2] + GetPlayerMaxs()[2])*0.5;
		cont = enginetrace->GetPointContents( point );
		// If that point is also under water...
		if ( cont & MASK_WATER )
		{
			// Set a higher water level.
			marine->SetWaterLevel( WL_Waist );

			// Now check the eye position.  (view_ofs is relative to the origin)
			point[2] = mv->GetAbsOrigin()[2] + player->GetViewOffset()[2];
			cont = enginetrace->GetPointContents( point );
			if ( cont & MASK_WATER )
				marine->SetWaterLevel( WL_Eyes );  // In over our eyes
		}

		// Adjust velocity based on water current, if any.
		if ( cont & MASK_CURRENT )
		{
			Vector v;
			VectorClear(v);
			if ( cont & CONTENTS_CURRENT_0 )
				v[0] += 1;
			if ( cont & CONTENTS_CURRENT_90 )
				v[1] += 1;
			if ( cont & CONTENTS_CURRENT_180 )
				v[0] -= 1;
			if ( cont & CONTENTS_CURRENT_270 )
				v[1] -= 1;
			if ( cont & CONTENTS_CURRENT_UP )
				v[2] += 1;
			if ( cont & CONTENTS_CURRENT_DOWN )
				v[2] -= 1;

			// BUGBUG -- this depends on the value of an unspecified enumerated type
			// The deeper we are, the stronger the current.
			Vector temp;
			VectorMA( marine->GetBaseVelocity(), 50.0*marine->GetWaterLevel(), v, temp );
			marine->SetBaseVelocity( temp );
		}
	}

	return ( marine->GetWaterLevel() > WL_Feet );
}

void CASW_MarineGameMovement::SetGroundEntity( trace_t *pm )
{
	CBaseEntity *newGround = pm ? pm->m_pEnt : NULL;

	CBaseEntity *oldGround = marine->GetGroundEntity();
	Vector vecBaseVelocity = marine->GetBaseVelocity();

	if ( !oldGround && newGround )
	{
		// Subtract ground velocity at instant we hit ground jumping
		vecBaseVelocity -= newGround->GetAbsVelocity(); 
		vecBaseVelocity.z = newGround->GetAbsVelocity().z;
		// paranoid to get rid of a crazy sliding physics bug: clear base velocity if we just left the world
		if (newGround->entindex() == 0)
			vecBaseVelocity = vec3_origin;
	}
	else if ( oldGround && !newGround )
	{
		// Add in ground velocity at instant we started jumping
		vecBaseVelocity += oldGround->GetAbsVelocity();
		vecBaseVelocity.z = oldGround->GetAbsVelocity().z;
		// paranoid to get rid of a crazy sliding physics bug: clear base velocity if we just left the world
		if (oldGround->entindex() == 0)
			vecBaseVelocity = vec3_origin;
	}

	marine->SetBaseVelocity( vecBaseVelocity );
	marine->SetGroundEntity( newGround );

	// If we are on something...

	if ( newGround )
	{
		CategorizeGroundSurface( *pm );

		// Then we are not in water jump sequence
		player->m_flWaterJumpTime = 0;

		// Standing on an entity other than the world, so signal that we are touching something.
		if ( !pm->DidHitWorld() )
		{
			MoveHelper()->AddToTouched( *pm, mv->m_vecVelocity );
		}

		if( marine->GetMoveType() != MOVETYPE_NOCLIP )
			mv->m_vecVelocity.z = 0.0f;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &input - 
//-----------------------------------------------------------------------------
void CASW_MarineGameMovement::CategorizePosition( void )
{
	Vector point;
	trace_t pm;

	// Reset this each time we-recategorize, otherwise we have bogus friction when we jump into water and plunge downward really quickly
	player->m_surfaceFriction = 1.0f;
	
	// if the player hull point one unit down is solid, the player
	// is on ground
	
	// see if standing on something solid	

	// Doing this before we move may introduce a potential latency in water detection, but
	// doing it after can get us stuck on the bottom in water if the amount we move up
	// is less than the 1 pixel 'threshold' we're about to snap to.	Also, we'll call
	// this several times per frame, so we really need to avoid sticking to the bottom of
	// water on each call, and the converse case will correct itself if called twice.
	CheckWater();

	point[0] = mv->GetAbsOrigin()[0];
	point[1] = mv->GetAbsOrigin()[1];
	point[2] = mv->GetAbsOrigin()[2] - 2;		// move a total of 4 units to try and avoid some
	                                        // epsilon error

	Vector bumpOrigin;
	bumpOrigin = mv->GetAbsOrigin();
	//bumpOrigin.z += 2;

	// Shooting up really fast.  Definitely not on ground.
	// On ladder moving up, so not on ground either
	// NOTE: 145 is a jump.
#define NON_JUMP_VELOCITY 140.0f

	float zvel = mv->m_vecVelocity[2];
	bool bMovingUp = zvel > 0.0f;
	bool bMovingUpRapidly = zvel > NON_JUMP_VELOCITY;
	float flGroundEntityVelZ = 0.0f;
	if ( bMovingUpRapidly )
	{
		// Tracker 73219, 75878:  ywb 8/2/07
		// After save/restore (and maybe at other times), we can get a case where we were saved on a lift and 
		//  after restore we'll have a high local velocity due to the lift making our abs velocity appear high.  
		// We need to account for standing on a moving ground object in that case in order to determine if we really 
		//  are moving away from the object we are standing on at too rapid a speed.  Note that CheckJump already sets
		//  ground entity to NULL, so this wouldn't have any effect unless we are moving up rapidly not from the jump button.
		CBaseEntity *ground = marine->GetGroundEntity();
		if ( ground )
		{
			flGroundEntityVelZ = ground->GetAbsVelocity().z;
			bMovingUpRapidly = ( zvel - flGroundEntityVelZ ) > NON_JUMP_VELOCITY;
		}
	}

	// NOTE YWB 7/5/07:  Since we're already doing a traceline here, we'll subsume the StayOnGround (stair debouncing) check into the main traceline we do here to see what we're standing on
	bool bUnderwater = ( player->GetWaterLevel() >= WL_Eyes );
	bool bMoveToEndPos = false;
	if ( marine->GetMoveType() == MOVETYPE_WALK && 
		marine->GetGroundEntity() != NULL && !bUnderwater )
	{
		// if walking and still think we're on ground, we'll extend trace down by stepsize so we don't bounce down slopes
		bMoveToEndPos = true;
		point.z -= player->m_Local.m_flStepSize;
	}

	// Was on ground, but now suddenly am not
	if ( bMovingUpRapidly || 
		( bMovingUp && marine->GetMoveType() == MOVETYPE_LADDER ) )   
	{
		SetGroundEntity( NULL );
		bMoveToEndPos = false;
	}
	else
	{
		// Try and move down.
		TraceMarineBBox( bumpOrigin, point, MASK_PLAYERSOLID, COLLISION_GROUP_PLAYER_MOVEMENT, pm );

		// Was on ground, but now suddenly am not.  If we hit a steep plane, we are not on ground
		float flStandableZ = 0.7;

		if ( !pm.m_pEnt || ( pm.plane.normal[2] < flStandableZ ) )
		{
			// Test four sub-boxes, to see if any of them would have found shallower slope we could actually stand on
			ITraceFilter *pFilter = LockTraceFilter( COLLISION_GROUP_PLAYER_MOVEMENT );
			TraceMarineBBoxForGround( m_pTraceListData, bumpOrigin, point, GetPlayerMins(), 
				GetPlayerMaxs(), MASK_PLAYERSOLID, pFilter, pm, flStandableZ, true, &m_nTraceCount );
			UnlockTraceFilter( pFilter );
			if ( !pm.m_pEnt || ( pm.plane.normal[2] < flStandableZ ) )
			{
				SetGroundEntity( NULL );
				// probably want to add a check for a +z velocity too!
				if ( ( mv->m_vecVelocity.z > 0.0f ) && 
					( player->GetMoveType() != MOVETYPE_NOCLIP ) )
				{
					player->m_surfaceFriction = 0.25f;
				}
				bMoveToEndPos = false;
			}
			else
			{
				SetGroundEntity( &pm );
			}
		}
		else
		{
			SetGroundEntity( &pm );  // Otherwise, point to index of ent under us.
		}

#ifndef CLIENT_DLL
		
		//Adrian: vehicle code handles for us.
		if ( player->IsInAVehicle() == false )
		{
			// If our gamematerial has changed, tell any player surface triggers that are watching
			IPhysicsSurfaceProps *physprops = MoveHelper()->GetSurfaceProps();
			surfacedata_t *pSurfaceProp = physprops->GetSurfaceData( pm.surface.surfaceProps );
			char cCurrGameMaterial = pSurfaceProp->game.material;
			if ( !marine->GetGroundEntity() )
			{
				cCurrGameMaterial = 0;
			}

			// Changed?
			if ( marine->m_chPreviousTextureType != cCurrGameMaterial )
			{
				CEnvPlayerSurfaceTrigger::SetPlayerSurface( player, cCurrGameMaterial );
			}

			marine->m_chPreviousTextureType = cCurrGameMaterial;
		}
#endif
	}

	// YWB:  This logic block essentially lifted from StayOnGround implementation
	if ( bMoveToEndPos &&
		!pm.startsolid &&				// not sure we need this check as fraction would == 0.0f?
		pm.fraction > 0.0f &&			// must go somewhere
		pm.fraction < 1.0f ) 			// must hit something
	{
		mv->SetAbsOrigin( pm.endpos );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Determine if the player has hit the ground while falling, apply
//			damage, and play the appropriate impact sound.
//-----------------------------------------------------------------------------
void CASW_MarineGameMovement::CheckFalling( void )
{
	float fFallVel = player->m_Local.m_flFallVelocity;
	//Msg("Checking falling, fall vel = %f\n", fFallVel);
	if ( marine->GetGroundEntity() != NULL &&
		 !IsDead() &&
		 fFallVel >= PLAYER_FALL_PUNCH_THRESHOLD )
	{
		bool bAlive = true;
		float fvol = 0.5;		

		if ( marine->GetWaterLevel() > 0 )
		{
			// They landed in water.
		}
		else
		{
			// Scale it down if we landed on something that's floating...
			if ( marine->GetGroundEntity()->IsFloating() )
			{
				fFallVel -= PLAYER_LAND_ON_FLOATING_OBJECT;
			}

			//
			// They hit the ground.
			//

			// asw added this if block: sept 2nd 06
			if( marine->GetGroundEntity()->GetAbsVelocity().z < 0.0f )
			{
				// Player landed on a descending object. Subtract the velocity of the ground entity.
				player->m_Local.m_flFallVelocity += marine->GetGroundEntity()->GetAbsVelocity().z;
				player->m_Local.m_flFallVelocity = MAX( 0.1f, player->m_Local.m_flFallVelocity );
			}
						
			if ( fFallVel > PLAYER_MAX_SAFE_FALL_SPEED )
			{
				//
				// If they hit the ground going this fast they may take damage (and die).
				//
				//bAlive = MoveHelper( )->PlayerFallingDamage();
#ifndef CLIENT_DLL
				float fFallVelMod = fFallVel;
				fFallVelMod -= PLAYER_MAX_SAFE_FALL_SPEED;
				float flFallDamage = fFallVelMod * DAMAGE_FOR_FALL_SPEED;
				if ( asw_debug_marine_damage.GetBool() )
				{
					Msg("Marine fell with speed %f modded to %f damage is %f\n", fFallVel, fFallVelMod, flFallDamage);
				}
				if ( flFallDamage > 0 )
				{
					if ( asw_marine_fall_damage.GetBool() )
					{
						marine->TakeDamage( CTakeDamageInfo( GetContainingEntity(INDEXENT(0)), GetContainingEntity(INDEXENT(0)), flFallDamage, DMG_FALL ) ); 
					}
					CRecipientFilter filter;
					filter.AddRecipientsByPAS( marine->GetAbsOrigin() );

					CBaseEntity::EmitSound( filter, marine->entindex(), "Player.FallDamage" );
				}
				bAlive = marine->GetHealth() > 0;
#endif
				fvol = 1.0;
			}
			else if ( fFallVel > PLAYER_MAX_SAFE_FALL_SPEED / 2 )
			{
				fvol = 0.85;
			}
			else if ( fFallVel < PLAYER_MIN_BOUNCE_SPEED )
			{
				fvol = 0;
			}
			
		}

		
		
		if ( fvol > 0.0 )
		{
			//
			// Play landing sound right away.
			player->m_flStepSoundTime = 400;

			// Play step sound for current texture.
			Vector vecSrc = mv->GetAbsOrigin();
			player->PlayStepSound( vecSrc, marine->m_pSurfaceData, fvol, true );

			//
			// Knock the screen around a little bit, temporary effect.
			//
			// (either of) these lines present on the server fixes the flickering gun
			player->m_Local.m_vecPunchAngle.Set( ROLL, fFallVel * 0.013 );

			if ( player->m_Local.m_vecPunchAngle[PITCH] > 8 )
			{
				// (either of) these lines present on the server fixes the flickering gun
				player->m_Local.m_vecPunchAngle.Set( PITCH, 8 );
			}
		}
		

		if (bAlive)
		{
			MoveHelper( )->PlayerSetAnimation( PLAYER_WALK );
		}
	}

	//
	// Clear the fall velocity so the impact doesn't happen again.
	//
	if ( marine->GetGroundEntity() != NULL ) 
	{		
		player->m_Local.m_flFallVelocity = 0;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Use for ease-in, ease-out style interpolation (accel/decel)  Used by ducking code.
// Input  : value - 
//			scale - 
// Output : float
//-----------------------------------------------------------------------------
float CASW_MarineGameMovement::SplineFraction( float value, float scale )
{
	float valueSquared;

	value = scale * value;
	valueSquared = value * value;

	// Nice little ease-in, ease-out spline-like curve
	return 3 * valueSquared - 2 * valueSquared * value;
}

//-----------------------------------------------------------------------------
// Purpose: Determine if crouch/uncrouch caused player to get stuck in world
// Input  : direction - 
//-----------------------------------------------------------------------------
void CASW_MarineGameMovement::FixPlayerCrouchStuck( bool upward )
{
	EntityHandle_t hitent;
	int i;
	Vector test;
	trace_t dummy;

	int direction = upward ? 1 : 0;

	hitent = TestPlayerPosition( mv->GetAbsOrigin(), COLLISION_GROUP_PLAYER_MOVEMENT, dummy );
	if (hitent == INVALID_ENTITY_HANDLE )
		return;
	
	VectorCopy( mv->GetAbsOrigin(), test );	
	for ( i = 0; i < 36; i++ )
	{
		Vector dest = mv->GetAbsOrigin();
		dest[2] += direction;
		mv->SetAbsOrigin( dest );
		hitent = TestPlayerPosition( mv->GetAbsOrigin(), COLLISION_GROUP_PLAYER_MOVEMENT, dummy );
		if (hitent == INVALID_ENTITY_HANDLE )
			return;
	}

	mv->SetAbsOrigin( test ); // Failed
}

bool CASW_MarineGameMovement::CanUnduck()
{
	int i;
	trace_t trace;
	Vector newOrigin;

	VectorCopy( mv->GetAbsOrigin(), newOrigin );

	if ( marine->GetGroundEntity() != NULL )
	{
		for ( i = 0; i < 3; i++ )
		{
			newOrigin[i] += ( VEC_DUCK_HULL_MIN[i] - VEC_HULL_MIN[i] );
		}
	}
	else
	{
		// If in air an letting go of croush, make sure we can offset origin to make
		//  up for uncrouching
		Vector hullSizeNormal = VEC_HULL_MAX - VEC_HULL_MIN;
		Vector hullSizeCrouch = VEC_DUCK_HULL_MAX - VEC_DUCK_HULL_MIN;
		Vector viewDelta = ( hullSizeNormal - hullSizeCrouch );
		viewDelta.Negate();
		VectorAdd( newOrigin, viewDelta, newOrigin );
	}

	bool saveducked = player->m_Local.m_bDucked;
	player->m_Local.m_bDucked = false;
	TraceMarineBBox( mv->GetAbsOrigin(), newOrigin, MASK_PLAYERSOLID, COLLISION_GROUP_PLAYER_MOVEMENT, trace );
	player->m_Local.m_bDucked = saveducked;
	if ( trace.startsolid || ( trace.fraction != 1.0f ) )
		return false;	

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Stop ducking
//-----------------------------------------------------------------------------
void CASW_MarineGameMovement::FinishUnDuck( void )
{
	int i;
	trace_t trace;
	Vector newOrigin;

	VectorCopy( mv->GetAbsOrigin(), newOrigin );

	if ( marine->GetGroundEntity() != NULL )
	{
		for ( i = 0; i < 3; i++ )
		{
			newOrigin[i] += ( VEC_DUCK_HULL_MIN[i] - VEC_HULL_MIN[i] );
		}
	}
	else
	{
		// If in air an letting go of croush, make sure we can offset origin to make
		//  up for uncrouching
		Vector hullSizeNormal = VEC_HULL_MAX - VEC_HULL_MIN;
		Vector hullSizeCrouch = VEC_DUCK_HULL_MAX - VEC_DUCK_HULL_MIN;
		Vector viewDelta = ( hullSizeNormal - hullSizeCrouch );
		viewDelta.Negate();
		VectorAdd( newOrigin, viewDelta, newOrigin );
	}

	player->m_Local.m_bDucked = false;
	marine->RemoveFlag( FL_DUCKING );
	player->m_Local.m_bDucking  = false;
	player->SetViewOffset( GetPlayerViewOffset( false ) );
	player->m_Local.m_nDuckTimeMsecs = 0;
	
	mv->SetAbsOrigin( newOrigin );

	// Recategorize position since ducking can change origin
	CategorizePosition();
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CASW_MarineGameMovement::UpdateDuckJumpEyeOffset( void )
{
	if ( player->m_Local.m_nDuckJumpTimeMsecs != 0 )
	{
		int nDuckMilliseconds = MAX( 0, GAMEMOVEMENT_DUCK_TIME - player->m_Local.m_nDuckJumpTimeMsecs );
		if ( nDuckMilliseconds > TIME_TO_UNDUCK_MSECS )
		{
			player->m_Local.m_nDuckJumpTimeMsecs = 0.0f;
			return;
		}

		float flDuckFraction = SimpleSpline( 1.0f - FractionUnDucked( nDuckMilliseconds ) );
		
		Vector vDuckHullMin = GetPlayerMins( true );
		Vector vStandHullMin = GetPlayerMins( false );
		
		float fMore = ( vDuckHullMin.z - vStandHullMin.z );
		
		Vector vecDuckViewOffset = GetPlayerViewOffset( true );
		Vector vecStandViewOffset = GetPlayerViewOffset( false );
		Vector vecTemp = player->GetViewOffset();
		vecTemp.z = ( ( vecDuckViewOffset.z - fMore ) * flDuckFraction ) + ( vecStandViewOffset.z * ( 1 - flDuckFraction ) );
		player->SetViewOffset( vecTemp );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CASW_MarineGameMovement::FinishUnDuckJump( trace_t &trace )
{
	Vector vecNewOrigin;
	VectorCopy( mv->GetAbsOrigin(), vecNewOrigin );

	//  Up for uncrouching.
	Vector hullSizeNormal = VEC_HULL_MAX - VEC_HULL_MIN;
	Vector hullSizeCrouch = VEC_DUCK_HULL_MAX - VEC_DUCK_HULL_MIN;
	Vector viewDelta = ( hullSizeNormal - hullSizeCrouch );

	float flDeltaZ = viewDelta.z;
	viewDelta.z *= trace.fraction;
	flDeltaZ -= viewDelta.z;

	marine->RemoveFlag( FL_DUCKING );
	player->m_Local.m_bDucked = false;
	player->m_Local.m_bDucking  = false;
	player->m_Local.m_bInDuckJump = false;
	player->m_Local.m_nDuckTimeMsecs = 0;
	player->m_Local.m_nDuckJumpTimeMsecs = 0;
	player->m_Local.m_nJumpTimeMsecs = 0;
	
	Vector vecViewOffset = GetPlayerViewOffset( false );
	vecViewOffset.z -= flDeltaZ;
	player->SetViewOffset( vecViewOffset );

	VectorSubtract( vecNewOrigin, viewDelta, vecNewOrigin );
	mv->SetAbsOrigin( vecNewOrigin );

	// Recategorize position since ducking can change origin
	CategorizePosition();
}

//-----------------------------------------------------------------------------
// Purpose: Finish ducking
//-----------------------------------------------------------------------------
void CASW_MarineGameMovement::FinishDuck( void )
{
	int i;

	marine->AddFlag( FL_DUCKING );
	player->m_Local.m_bDucked = true;
	player->m_Local.m_bDucking = false;

	player->SetViewOffset( GetPlayerViewOffset( true ) );

	// HACKHACK - Fudge for collision bug - no time to fix this properly
	if ( marine->GetGroundEntity() != NULL )
	{
		Vector origin = mv->GetAbsOrigin();
		for ( i = 0; i < 3; i++ )
		{
			origin[i] -= ( VEC_DUCK_HULL_MIN[i] - VEC_HULL_MIN[i] );
		}
		mv->SetAbsOrigin( origin );
	}
	else
	{
		Vector hullSizeNormal = VEC_HULL_MAX - VEC_HULL_MIN;
		Vector hullSizeCrouch = VEC_DUCK_HULL_MAX - VEC_DUCK_HULL_MIN;
		Vector viewDelta = ( hullSizeNormal - hullSizeCrouch );
		Vector dest;
   		VectorAdd( mv->GetAbsOrigin(), viewDelta, dest );
		mv->SetAbsOrigin( dest );
	}

	// See if we are stuck?
	FixPlayerCrouchStuck( true );

	// Recategorize position since ducking can change origin
	CategorizePosition();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CASW_MarineGameMovement::StartUnDuckJump( void )
{
	marine->AddFlag( FL_DUCKING );
	player->m_Local.m_bDucked = true;
	player->m_Local.m_bDucking = false;

	player->SetViewOffset( GetPlayerViewOffset( true ) );

	Vector hullSizeNormal = VEC_HULL_MAX - VEC_HULL_MIN;
	Vector hullSizeCrouch = VEC_DUCK_HULL_MAX - VEC_DUCK_HULL_MIN;
	Vector viewDelta = ( hullSizeNormal - hullSizeCrouch );
	Vector dest;
	VectorAdd( mv->GetAbsOrigin(), viewDelta, dest );
	mv->SetAbsOrigin( dest );

	// See if we are stuck?
	FixPlayerCrouchStuck( true );

	// Recategorize position since ducking can change origin
	CategorizePosition();
}

//
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : duckFraction - 
//-----------------------------------------------------------------------------
void CASW_MarineGameMovement::SetDuckedEyeOffset( float duckFraction )
{
	Vector vDuckHullMin = GetPlayerMins( true );
	Vector vStandHullMin = GetPlayerMins( false );

	float fMore = ( vDuckHullMin.z - vStandHullMin.z );

	Vector vecDuckViewOffset = GetPlayerViewOffset( true );
	Vector vecStandViewOffset = GetPlayerViewOffset( false );
	Vector temp = player->GetViewOffset();
	temp.z = ( ( vecDuckViewOffset.z - fMore ) * duckFraction ) +
				( vecStandViewOffset.z * ( 1 - duckFraction ) );
	player->SetViewOffset( temp );
}

//-----------------------------------------------------------------------------
// Purpose: Crop the speed of the player when ducking and on the ground.
//   Input: bInDuck - is the player already ducking
//          bInAir - is the player in air
//    NOTE: Only crop player speed once.
//-----------------------------------------------------------------------------
void CASW_MarineGameMovement::HandleDuckingSpeedCrop( void )
{
	if ( !m_bSpeedCropped && ( marine->GetFlags() & FL_DUCKING ) && ( marine->GetGroundEntity() != NULL ) )
	{
		float frac = 0.33333333f;
		mv->m_flForwardMove	*= frac;
		mv->m_flSideMove	*= frac;
		mv->m_flUpMove		*= frac;
		m_bSpeedCropped		= true;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Check to see if we are in a situation where we can unduck jump.
//-----------------------------------------------------------------------------
bool CASW_MarineGameMovement::CanUnDuckJump( trace_t &trace )
{
	// Trace down to the stand position and see if we can stand.
	Vector vecEnd( mv->GetAbsOrigin() );
	vecEnd.z -= 36.0f;						// This will have to change if bounding hull change!
	TraceMarineBBox( mv->GetAbsOrigin(), vecEnd, MASK_PLAYERSOLID, COLLISION_GROUP_PLAYER_MOVEMENT, trace );
	if ( trace.fraction < 1.0f )
	{
		// Find the endpoint.
		vecEnd.z = mv->GetAbsOrigin().z + ( -36.0f * trace.fraction );

		// Test a normal hull.
		trace_t traceUp;
		bool bWasDucked = player->m_Local.m_bDucked;
		player->m_Local.m_bDucked = false;
		TraceMarineBBox( vecEnd, vecEnd, MASK_PLAYERSOLID, COLLISION_GROUP_PLAYER_MOVEMENT, traceUp );
		player->m_Local.m_bDucked = bWasDucked;
		if ( !traceUp.startsolid  )
			return true;	
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: See if duck button is pressed and do the appropriate things
//-----------------------------------------------------------------------------
void CASW_MarineGameMovement::Duck( void )
{
	int buttonsChanged	= ( mv->m_nOldButtons ^ mv->m_nButtons );	// These buttons have changed this frame
	int buttonsPressed	=  buttonsChanged & mv->m_nButtons;			// The changed ones still down are "pressed"
	int buttonsReleased	=  buttonsChanged & mv->m_nOldButtons;		// The changed ones which were previously down are "released"

	// Check to see if we are in the air.
	bool bInAir = ( marine->GetGroundEntity() == NULL );
	bool bInDuck = ( marine->GetFlags() & FL_DUCKING ) ? true : false;
	bool bDuckJump = ( player->m_Local.m_nJumpTimeMsecs > 0 );
	bool bDuckJumpTime = ( player->m_Local.m_nDuckJumpTimeMsecs > 0 );

	if ( mv->m_nButtons & IN_DUCK )
	{
		mv->m_nOldButtons |= IN_DUCK;
	}
	else
	{
		mv->m_nOldButtons &= ~IN_DUCK;
	}

	// Handle death.
	if ( IsDead() )
		return;

	// Slow down ducked players.
	HandleDuckingSpeedCrop();

	// If the player is holding down the duck button, the player is in duck transition, ducking, or duck-jumping.
	if ( ( mv->m_nButtons & IN_DUCK ) || player->m_Local.m_bDucking  || bInDuck || bDuckJump )
	{
		// DUCK
		if ( ( mv->m_nButtons & IN_DUCK ) || bDuckJump )
		{
			// Have the duck button pressed, but the player currently isn't in the duck position.
			if ( ( buttonsPressed & IN_DUCK ) && !bInDuck && !bDuckJump && !bDuckJumpTime )
			{
				player->m_Local.m_nDuckTimeMsecs = GAMEMOVEMENT_DUCK_TIME;
				player->m_Local.m_bDucking = true;
			}
			
			// The player is in duck transition and not duck-jumping.
			if ( player->m_Local.m_bDucking && !bDuckJump && !bDuckJumpTime )
			{
				int nDuckMilliseconds = MAX( 0, GAMEMOVEMENT_DUCK_TIME - player->m_Local.m_nDuckTimeMsecs );
				
				// Finish in duck transition when transition time is over, in "duck", in air.
				if ( ( nDuckMilliseconds > TIME_TO_DUCK_MSECS ) || bInDuck || bInAir )
				{
					FinishDuck();
				}
				else
				{
					// Calc parametric time
					float flDuckFraction = SimpleSpline( FractionDucked( nDuckMilliseconds ) );
					SetDuckedEyeOffset( flDuckFraction );
				}
			}

			if ( bDuckJump )
			{
				// Make the bounding box small immediately.
				if ( !bInDuck )
				{
					StartUnDuckJump();
				}
				else
				{
					// Check for a crouch override.
					if ( !( mv->m_nButtons & IN_DUCK ) )
					{
						trace_t trace;
						if ( CanUnDuckJump( trace ) )
						{
							FinishUnDuckJump( trace );
							player->m_Local.m_nDuckJumpTimeMsecs = (int)( ( (float)GAMEMOVEMENT_TIME_TO_UNDUCK_MSECS * ( 1.0f - trace.fraction ) ) + (float)GAMEMOVEMENT_TIME_TO_UNDUCK_MSECS_INV );
						}
					}
				}
			}
		}
		// UNDUCK (or attempt to...)
		else
		{
			if ( player->m_Local.m_bInDuckJump )
			{
				// Check for a crouch override.
   				if ( !( mv->m_nButtons & IN_DUCK ) )
				{
					trace_t trace;
					if ( CanUnDuckJump( trace ) )
					{
						FinishUnDuckJump( trace );
					
						if ( trace.fraction < 1.0f )
						{
							player->m_Local.m_nDuckJumpTimeMsecs = (int)( ( (float)GAMEMOVEMENT_TIME_TO_UNDUCK_MSECS * ( 1.0f - trace.fraction ) ) + (float)GAMEMOVEMENT_TIME_TO_UNDUCK_MSECS_INV );
						}
					}
				}
				else
				{
					player->m_Local.m_bInDuckJump = false;
				}
			}

			if ( bDuckJumpTime )
				return;

			// Try to unduck unless automovement is not allowed
			// NOTE: When not onground, you can always unduck
			if ( player->m_Local.m_bAllowAutoMovement || bInAir )
			{
				// We released the duck button, we aren't in "duck" and we are not in the air - start unduck transition.
				if ( ( buttonsReleased & IN_DUCK ) && bInDuck && !bDuckJump )
				{
					player->m_Local.m_nDuckTimeMsecs = GAMEMOVEMENT_DUCK_TIME;
				}

				// Check to see if we are capable of unducking.
				if ( CanUnduck() )
				{
					// or unducking
					if ( ( player->m_Local.m_bDucking || player->m_Local.m_bDucked ) )
					{
						int nDuckMilliseconds = MAX( 0, GAMEMOVEMENT_DUCK_TIME - player->m_Local.m_nDuckTimeMsecs );

						// Finish ducking immediately if duck time is over or not on ground
						if ( nDuckMilliseconds > TIME_TO_UNDUCK_MSECS || ( bInAir && !bDuckJump ) )
						{
							FinishUnDuck();
						}
						else
						{
							// Calc parametric time
							float flDuckFraction = SimpleSpline( 1.0f - FractionUnDucked( nDuckMilliseconds ) );
							SetDuckedEyeOffset( flDuckFraction );
							player->m_Local.m_bDucking = true;
						}
					}
				}
				else
				{
					// Still under something where we can't unduck, so make sure we reset this timer so
					//  that we'll unduck once we exit the tunnel, etc.
					player->m_Local.m_nDuckTimeMsecs = GAMEMOVEMENT_DUCK_TIME;
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_MarineGameMovement::PlayerMove( void )
{
	VPROF( "CASW_MarineGameMovement::PlayerMove" );

	CheckParameters();
	
	// clear output applied velocity
	mv->m_outWishVel.Init();
	mv->m_outJumpVel.Init();

	MoveHelper( )->ResetTouchList();                    // Assume we don't touch anything

	ReduceTimers();

	if ( marine->m_iJumpJetting != JJ_NONE )
	{
		FullJumpJetMove();
		return;
	}
	
	// use fixed axis?
	if ( asw_controls.GetInt() == 1 )
		AngleVectors( ASWGameRules()->GetTopDownMovementAxis(), &m_vecForward, &m_vecRight, &m_vecUp ); 
	else
		AngleVectors (mv->m_vecViewAngles, &m_vecForward, &m_vecRight, &m_vecUp );  // Determine movement angles

	// Always try and unstick us unless we are a couple of the movement modes
	//if ( CheckInterval( STUCK ) )
	{
		if ( marine->GetMoveType() != MOVETYPE_NOCLIP && 
			 marine->GetMoveType() != MOVETYPE_NONE && 		 
			 marine->GetMoveType() != MOVETYPE_ISOMETRIC && 
			 marine->GetMoveType() != MOVETYPE_OBSERVER )
		{
			//Msg("Befor move: ");
			if ( CheckStuck() )
			{
				//Msg("*** Can't move, we're stuck\n");
				// Can't move, we're stuck
				return;  
			}
		}
	}

	// Now that we are "unstuck", see where we are (marine->GetWaterLevel() and type, marine->GetGroundEntity()).
	CategorizePosition();

	// Store off the starting water level
	m_nOldWaterLevel = marine->GetWaterLevel();

	// If we are not on ground, store off how fast we are moving down
	if ( marine->GetGroundEntity() == NULL )
	{
		player->m_Local.m_flFallVelocity = -mv->m_vecVelocity[ 2 ];
	}

	m_nOnLadder = 0;

// 	if ( CheckInterval( GROUND ) )
// 	{
// 		CategorizeGroundSurface();
// 	}

	//marine->UpdateStepSound( marine->m_pSurfaceData, mv->m_vecAbsOrigin, mv->m_vecVelocity );

	UpdateDuckJumpEyeOffset();
	//Duck();		// asw, remove duck for now (causes strange z change when you jump)

	// Don't run ladder code if dead on on a train
	if ( !player->pl.deadflag && !(marine->GetFlags() & FL_ONTRAIN) )
	{
		// If was not on a ladder now, but was on one before, 
		//  get off of the ladder
		
		// TODO: this causes lots of weirdness.
		//bool bCheckLadder = CheckInterval( LADDER );
		//if ( bCheckLadder || marine->GetMoveType() == MOVETYPE_LADDER )
		{
			if ( !LadderMove() && 
				( marine->GetMoveType() == MOVETYPE_LADDER ) )
			{
				// Clear ladder stuff unless player is dead or riding a train
				// It will be reset immediately again next frame if necessary
				marine->SetMoveType( MOVETYPE_WALK );
				marine->SetMoveCollide( MOVECOLLIDE_DEFAULT );
			}
		}
	}

	// Handle movement modes.
	switch (marine->GetMoveType())
	{
		case MOVETYPE_NONE:
			break;

		case MOVETYPE_NOCLIP:
			FullNoClipMove( sv_noclipspeed.GetFloat(), sv_noclipaccelerate.GetFloat() );
			break;

		case MOVETYPE_FLY:
		case MOVETYPE_FLYGRAVITY:
			FullTossMove();
			break;

		case MOVETYPE_LADDER:
			FullLadderMove();
			break;

		case MOVETYPE_WALK:
			if ( marine->GetCurrentMeleeAttack() && marine->m_iMeleeAllowMovement == MELEE_MOVEMENT_ANIMATION_ONLY )
			{
				FullMeleeMove();
			}
			else
			{
				FullWalkMove();
			}
			break;

		case MOVETYPE_ISOMETRIC:
			//IsometricMove();
			// Could also try:  FullTossMove();
			FullWalkMove();
			break;
			
		case MOVETYPE_OBSERVER:
			FullObserverMove(); // clips against world&players
			break;

		default:
			DevMsg( 1, "Bogus pmove marine movetype %i on (%i) 0=cl 1=sv\n", marine->GetMoveType(), player->IsServer());
			break;
	}
	//Msg("After move: ");  CheckStuck();
}


//-----------------------------------------------------------------------------
// Performs the collision resolution for fliers.
//-----------------------------------------------------------------------------
void CASW_MarineGameMovement::PerformFlyCollisionResolution( trace_t &pm, Vector &move )
{
	Vector base;
	float vel;
	float backoff;

	switch (marine->GetMoveCollide())
	{
	case MOVECOLLIDE_FLY_CUSTOM:
		// Do nothing; the velocity should have been modified by touch
		// FIXME: It seems wrong for touch to modify velocity
		// given that it can be called in a number of places
		// where collision resolution do *not* in fact occur

		// Should this ever occur for players!?
		Assert(0);
		break;

	case MOVECOLLIDE_FLY_BOUNCE:	
	case MOVECOLLIDE_DEFAULT:
		{
			if (marine->GetMoveCollide() == MOVECOLLIDE_FLY_BOUNCE)
				backoff = 2.0 - marine->m_surfaceFriction;
			else
				backoff = 1;

			ClipVelocity (mv->m_vecVelocity, pm.plane.normal, mv->m_vecVelocity, backoff);
		}
		break;

	default:
		// Invalid collide type!
		Assert(0);
		break;
	}

	// stop if on ground
	if (pm.plane.normal[2] > 0.7)
	{		
		base.Init();
		if (mv->m_vecVelocity[2] < asw_marine_gravity.GetFloat() * gpGlobals->frametime)
		{
			// we're rolling on the ground, add static friction.
			SetGroundEntity( &pm ); 
			mv->m_vecVelocity[2] = 0;
		}

		vel = DotProduct( mv->m_vecVelocity, mv->m_vecVelocity );

		// Con_DPrintf("%f %f: %.0f %.0f %.0f\n", vel, trace.fraction, ent->velocity[0], ent->velocity[1], ent->velocity[2] );

		if (vel < (30 * 30) || (marine->GetMoveCollide() != MOVECOLLIDE_FLY_BOUNCE))
		{
			SetGroundEntity( &pm ); 
			mv->m_vecVelocity.Init();
		}
		else
		{
			VectorScale (mv->m_vecVelocity, (1.0 - pm.fraction) * gpGlobals->frametime * 0.9, move);
			PushEntity( move, &pm );
		}
		VectorSubtract( mv->m_vecVelocity, base, mv->m_vecVelocity );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_MarineGameMovement::FullTossMove( void )
{
	trace_t pm;
	Vector move;
	
	CheckWater();

	// add velocity if player is moving 
	if ( (mv->m_flForwardMove != 0.0f) || (mv->m_flSideMove != 0.0f) || (mv->m_flUpMove != 0.0f))
	{
		Vector forward, right, up;
		float fmove, smove;
		Vector wishdir, wishvel;
		float wishspeed;
		int i;
		
		if ( asw_controls.GetInt() == 1 )
			AngleVectors( ASWGameRules()->GetTopDownMovementAxis(), &forward, &right, &up ); 
		else
			AngleVectors (mv->m_vecViewAngles, &forward, &right, &up);  // Determine movement angles

		// Copy movement amounts
		fmove = mv->m_flForwardMove;
		smove = mv->m_flSideMove;

		VectorNormalize (forward);  // Normalize remainder of vectors.
		VectorNormalize (right);    // 
		
		for (i=0 ; i<3 ; i++)       // Determine x and y parts of velocity
			wishvel[i] = forward[i]*fmove + right[i]*smove;

		wishvel[2] += mv->m_flUpMove;

		VectorCopy (wishvel, wishdir);   // Determine maginitude of speed of move
		wishspeed = VectorNormalize(wishdir);

		//
		// Clamp to server defined max speed
		//
		if (wishspeed > mv->m_flMaxSpeed)
		{
			VectorScale (wishvel, mv->m_flMaxSpeed/wishspeed, wishvel);
			wishspeed = mv->m_flMaxSpeed;
		}

		// Set pmove velocity
		Accelerate( wishdir, wishspeed, sv_accelerate.GetFloat() );
	}

	if ( mv->m_vecVelocity[2] > 0 )
	{
		SetGroundEntity( NULL );
	}

	// If on ground and not moving, return.
	if ( marine->GetGroundEntity() != NULL )
	{
		if (VectorCompare(marine->GetBaseVelocity(), vec3_origin) &&
		    VectorCompare(mv->m_vecVelocity, vec3_origin))
			return;
	}

	CheckVelocity();

	// add gravity
	if ( marine->GetMoveType() == MOVETYPE_FLYGRAVITY )
	{
		AddGravity();
	}

	// move origin
	// Base velocity is not properly accounted for since this entity will move again after the bounce without
	// taking it into account
	VectorAdd (mv->m_vecVelocity, marine->GetBaseVelocity(), mv->m_vecVelocity);
	
	CheckVelocity();

	VectorScale (mv->m_vecVelocity, gpGlobals->frametime, move);
	VectorSubtract (mv->m_vecVelocity, marine->GetBaseVelocity(), mv->m_vecVelocity);

	PushEntity( move, &pm );	// Should this clear basevelocity

	CheckVelocity();

	if (pm.allsolid)
	{	
		// entity is trapped in another solid
		SetGroundEntity( &pm );
		mv->m_vecVelocity.Init();
		return;
	}
	
	if (pm.fraction != 1)
	{
		PerformFlyCollisionResolution( pm, move );
	}
	
	// check for in water
	CheckWater();
}

surfacedata_t*	CASW_MarineGameMovement::GetSurfaceData()
{
	return marine->m_pSurfaceData;
}

//-----------------------------------------------------------------------------
// Purpose: TF2 commander mode movement logic
//-----------------------------------------------------------------------------

#pragma warning (disable : 4701)

void CASW_MarineGameMovement::IsometricMove( void )
{
	int i;
	Vector wishvel;
	float fmove, smove;
	Vector forward, right, up;
	
	if ( asw_controls.GetInt() == 1 )
		AngleVectors( ASWGameRules()->GetTopDownMovementAxis(), &forward, &right, &up ); 
	else
		AngleVectors (mv->m_vecViewAngles, &forward, &right, &up);  // Determine movement angles
	
	// Copy movement amounts
	fmove = mv->m_flForwardMove;
	smove = mv->m_flSideMove;
	
	// No up / down movement
	forward[2] = 0;
	right[2] = 0;

	VectorNormalize (forward);  // Normalize remainder of vectors
	VectorNormalize (right);    // 

	for (i=0 ; i<3 ; i++)       // Determine x and y parts of velocity
		wishvel[i] = forward[i]*fmove + right[i]*smove;
	//wishvel[2] += mv->m_flUpMove;

	Vector dest;
	VectorMA (mv->GetAbsOrigin(), gpGlobals->frametime, wishvel, dest);
	mv->SetAbsOrigin( dest );
	
	// Zero out the velocity so that we don't accumulate a huge downward velocity from
	//  gravity, etc.
	mv->m_vecVelocity.Init();
}

#pragma warning (default : 4701)

// Expose our interface.
static CASW_MarineGameMovement g_MarineGameMovement;
IMarineGameMovement *g_pMarineGameMovement = ( IMarineGameMovement * )&g_MarineGameMovement;

EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CASW_MarineGameMovement, IMarineGameMovement,INTERFACENAME_MARINEGAMEMOVEMENT, g_MarineGameMovement );
