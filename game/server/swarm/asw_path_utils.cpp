#include "cbase.h"
#include "asw_path_utils.h"
#include "ai_pathfinder.h"
#include "ai_waypoint.h"
#include "asw_player.h"
#include "asw_marine.h"
#include "asw_shareddefs.h"
#include "asw_trace_filter_doors.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CASW_Path_Utils_NPC : public CAI_BaseNPC
{
public:
	DECLARE_CLASS( CASW_Path_Utils_NPC, CAI_BaseNPC );

	void	Precache( void );
	void	Spawn( void );
	float	MaxYawSpeed( void ) { return 90.0f; }
	Class_T Classify ( void ) { return CLASS_NONE; }
};

LINK_ENTITY_TO_CLASS( asw_pathfinder_npc, CASW_Path_Utils_NPC );

CHandle<CASW_Path_Utils> g_hASWPathfinder;

void CASW_Path_Utils_NPC::Spawn()
{
	CapabilitiesAdd( bits_CAP_MOVE_GROUND | bits_CAP_OPEN_DOORS );

	SetModel( SWARM_NEW_DRONE_MODEL );
	NPCInit();

	UTIL_SetSize(this, NAI_Hull::Mins(HULL_MEDIUMBIG), NAI_Hull::Maxs(HULL_MEDIUMBIG));
	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_NOT_SOLID );
	AddSolidFlags( FSOLID_NOT_STANDABLE );
	SetMoveType( MOVETYPE_STEP );
	m_takedamage = DAMAGE_NO;
}

void CASW_Path_Utils_NPC::Precache()
{
	PrecacheModel( SWARM_NEW_DRONE_MODEL );
}


CASW_Path_Utils g_ASWPathfinder;
CASW_Path_Utils* ASWPathUtils() { return & g_ASWPathfinder; }

CASW_Path_Utils_NPC* CASW_Path_Utils::GetPathfinderNPC()
{
	if ( m_hPathfinderNPC.Get() == NULL )
	{
		m_hPathfinderNPC = dynamic_cast<CASW_Path_Utils_NPC*>( CBaseEntity::Create( "asw_pathfinder_npc", vec3_origin, vec3_angle, NULL ) );
	}

	return m_hPathfinderNPC.Get();
}


AI_Waypoint_t *CASW_Path_Utils::BuildRoute( const Vector &vStart, const Vector &vEnd, 
										  CBaseEntity *pTarget, float goalTolerance, Navigation_t curNavType, int nBuildFlags )
{
	if ( !GetPathfinderNPC() )
		return NULL;

	m_pLastRoute = GetPathfinderNPC()->GetPathfinder()->BuildRoute( vStart, vEnd, pTarget, goalTolerance, curNavType, nBuildFlags );

	return m_pLastRoute;
}

Vector g_vecPathStart = vec3_origin;

void CASW_Path_Utils::DeleteRoute( AI_Waypoint_t *pWaypointList )
{
	while ( pWaypointList )
	{
		AI_Waypoint_t *pPrevWaypoint = pWaypointList;
		pWaypointList = pWaypointList->GetNext();
		delete pPrevWaypoint;
	}
}

void asw_path_start_f()
{
	CASW_Player *pPlayer = ToASW_Player(UTIL_GetCommandClient());
	if ( !pPlayer || !ASWPathUtils() )
		return;

	if ( !pPlayer->GetMarine() )
		return;
	
	g_vecPathStart = pPlayer->GetMarine()->GetAbsOrigin();
}
ConCommand asw_path_start( "asw_path_start", asw_path_start_f, "mark start of pathfinding test", FCVAR_CHEAT );

void CASW_Path_Utils::DebugDrawRoute( const Vector &vecStartPos, AI_Waypoint_t *pWaypoint )
{
	Vector vecLastPos = vecStartPos;
	Msg(" Pathstart = %f %f %f\n", VectorExpand( g_vecPathStart ) );
	while ( pWaypoint )		
	{
		Msg("  waypoint = %f %f %f\n", VectorExpand( pWaypoint->GetPos() ) );
		DebugDrawLine( vecLastPos + Vector( 0, 0, 10 ) , pWaypoint->GetPos() + Vector( 0, 0, 10 ), 255, 0, 255, true, 30.0f );
		vecLastPos = pWaypoint->GetPos();
		pWaypoint = pWaypoint->GetNext();
	}
}

void asw_path_end_f()
{
	CASW_Player *pPlayer = ToASW_Player(UTIL_GetCommandClient());
	if ( !pPlayer || !ASWPathUtils() )
		return;

	if ( !pPlayer->GetMarine() )
		return;

	Vector vecPathEnd = pPlayer->GetMarine()->GetAbsOrigin();
	debugoverlay->AddBoxOverlay( g_vecPathStart, Vector(-10, -10, -10 ), Vector(10, 10, 10) , vec3_angle, 255, 0, 0, 255, 30.0f );
	debugoverlay->AddBoxOverlay( vecPathEnd, Vector(-10, -10, -10 ), Vector(10, 10, 10) , vec3_angle, 255, 255, 0, 255, 30.0f );

	AI_Waypoint_t *pWaypoint = ASWPathUtils()->BuildRoute( g_vecPathStart, vecPathEnd, NULL, 100, NAV_NONE, bits_BUILD_GET_CLOSE );
	if ( !pWaypoint )
	{
		Msg( "Failed to find route\n" );
		return;
	}

	if ( UTIL_ASW_DoorBlockingRoute( pWaypoint, true ) )
	{
		Msg(" Route blocked by sealed/locked door\n" );
	}

	ASWPathUtils()->DebugDrawRoute( g_vecPathStart, pWaypoint );
}
ConCommand asw_path_end( "asw_path_end", asw_path_end_f, "mark end of pathfinding test", FCVAR_CHEAT );