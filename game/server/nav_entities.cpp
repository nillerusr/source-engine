//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// nav_entities.cpp
// AI Navigation entities
// Author: Michael S. Booth (mike@turtlerockstudios.com), January 2003

#include "cbase.h"

#include "nav_mesh.h"
#include "nav_node.h"
#include "nav_pathfind.h"
#include "nav_colors.h"
#include "fmtstr.h"
#include "props_shared.h"
#include "func_breakablesurf.h"



#include "Color.h"
#include "collisionutils.h"
#include "functorutils.h"
#include "team.h"
#include "nav_entities.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//--------------------------------------------------------------------------------------------------------
BEGIN_DATADESC( CFuncNavBlocker )

	// Inputs
	DEFINE_INPUTFUNC( FIELD_VOID, "BlockNav", InputBlockNav ),
	DEFINE_INPUTFUNC( FIELD_VOID, "UnblockNav", InputUnblockNav ),
	DEFINE_KEYFIELD( m_blockedTeamNumber, FIELD_INTEGER, "teamToBlock" ),
	DEFINE_KEYFIELD( m_bDisabled,	FIELD_BOOLEAN,	"StartDisabled" ),

END_DATADESC()


LINK_ENTITY_TO_CLASS( func_nav_blocker, CFuncNavBlocker );


CUtlLinkedList<CFuncNavBlocker *> CFuncNavBlocker::gm_NavBlockers;

//-----------------------------------------------------------------------------------------------------
int CFuncNavBlocker::DrawDebugTextOverlays( void )
{
	int offset = BaseClass::DrawDebugTextOverlays();

	if (m_debugOverlays & OVERLAY_TEXT_BIT) 
	{
		CFmtStr str;

		// FIRST_GAME_TEAM skips TEAM_SPECTATOR and TEAM_UNASSIGNED, so we can print
		// useful team names in a non-game-specific fashion.
		for ( int i=FIRST_GAME_TEAM; i<FIRST_GAME_TEAM + MAX_NAV_TEAMS; ++i )
		{
			if ( IsBlockingNav( i ) )
			{
				CTeam *team = GetGlobalTeam( i );
				if ( team )
				{
					EntityText( offset++, str.sprintf( "blocking team %s", team->GetName() ), 0 );
				}
				else
				{
					EntityText( offset++, str.sprintf( "blocking team %d", i ), 0 );
				}
			}
		}

		NavAreaCollector collector( true );
		Extent extent;
		extent.Init( this );
		TheNavMesh->ForAllAreasOverlappingExtent( collector, extent );

		for ( int i=0; i<collector.m_area.Count(); ++i )
		{
			CNavArea *area = collector.m_area[i];
			Extent areaExtent;
			area->GetExtent( &areaExtent );
			debugoverlay->AddBoxOverlay( vec3_origin, areaExtent.lo, areaExtent.hi, vec3_angle, 0, 255, 0, 10, NDEBUG_PERSIST_TILL_NEXT_SERVER );
		}
	}

	return offset;
}


//--------------------------------------------------------------------------------------------------------
void CFuncNavBlocker::UpdateBlocked()
{
	NavAreaCollector collector( true );
	Extent extent;
	extent.Init( this );
	TheNavMesh->ForAllAreasOverlappingExtent( collector, extent );

	for ( int i=0; i<collector.m_area.Count(); ++i )
	{
		CNavArea *area = collector.m_area[i];
		area->UpdateBlocked( true );
	}

}


//--------------------------------------------------------------------------------------------------------
// Forces nav areas to unblock when the nav blocker is deleted (round restart) so flow can compute properly
void CFuncNavBlocker::UpdateOnRemove( void )
{
	UnblockNav();

	gm_NavBlockers.FindAndRemove( this );

	BaseClass::UpdateOnRemove();
}


//--------------------------------------------------------------------------------------------------------
void CFuncNavBlocker::Spawn( void )
{
	gm_NavBlockers.AddToTail( this );

	if ( !m_blockedTeamNumber )
		m_blockedTeamNumber = TEAM_ANY;

	SetMoveType( MOVETYPE_NONE );
	SetModel( STRING( GetModelName() ) );
	AddEffects( EF_NODRAW );
	SetCollisionGroup( COLLISION_GROUP_NONE );
	SetSolid( SOLID_NONE );
	AddSolidFlags( FSOLID_NOT_SOLID );
	CollisionProp()->WorldSpaceAABB( &m_CachedMins, &m_CachedMaxs );
	if ( m_bDisabled )
	{
		UnblockNav();
	}
	else
	{
		BlockNav();
	}
}


//--------------------------------------------------------------------------------------------------------
void CFuncNavBlocker::InputBlockNav( inputdata_t &inputdata )
{
	BlockNav();
}


//--------------------------------------------------------------------------------------------------------
void CFuncNavBlocker::InputUnblockNav( inputdata_t &inputdata )
{
	UnblockNav();
}


//--------------------------------------------------------------------------------------------------------
void CFuncNavBlocker::BlockNav( void )
{
	if ( m_blockedTeamNumber == TEAM_ANY )
	{
		for ( int i=0; i<MAX_NAV_TEAMS; ++i )
		{
			m_isBlockingNav[ i ] = true;
		}
	}
	else
	{
		int teamNumber = m_blockedTeamNumber % MAX_NAV_TEAMS;
		m_isBlockingNav[ teamNumber ] = true;
	}

	Extent extent;
	extent.Init( this );
	TheNavMesh->ForAllAreasOverlappingExtent( *this, extent );
}


//--------------------------------------------------------------------------------------------------------
void CFuncNavBlocker::UnblockNav( void )
{
	if ( m_blockedTeamNumber == TEAM_ANY )
	{
		for ( int i=0; i<MAX_NAV_TEAMS; ++i )
		{
			m_isBlockingNav[ i ] = false;
		}
	}
	else
	{
		int teamNumber = m_blockedTeamNumber % MAX_NAV_TEAMS;
		m_isBlockingNav[ teamNumber ] = false;
	}

	UpdateBlocked();
}


//--------------------------------------------------------------------------------------------------------
// functor that blocks areas in our extent
bool CFuncNavBlocker::operator()( CNavArea *area )
{
	area->MarkAsBlocked( m_blockedTeamNumber, this );
	return true;
}


//--------------------------------------------------------------------------------------------------------
bool CFuncNavBlocker::CalculateBlocked( bool *pResultByTeam, const Vector &vecMins, const Vector &vecMaxs )
{
	int nTeamsBlocked = 0;
	int i;
	bool bBlocked = false;
	for ( i=0; i<MAX_NAV_TEAMS; ++i )
	{
		pResultByTeam[i] = false;
	}

	FOR_EACH_LL( gm_NavBlockers, iBlocker )
	{
		CFuncNavBlocker *pBlocker = gm_NavBlockers[iBlocker];
		bool bIsIntersecting = false;

		for ( i=0; i<MAX_NAV_TEAMS; ++i )
		{
			if ( pBlocker->m_isBlockingNav[i] )
			{
				if ( !pResultByTeam[i] )
				{
					if ( bIsIntersecting || ( bIsIntersecting = IsBoxIntersectingBox( pBlocker->m_CachedMins, pBlocker->m_CachedMaxs, vecMins, vecMaxs ) ) != false )
					{
						bBlocked = true;
						pResultByTeam[i] = true;
						nTeamsBlocked++;
					}
					else
					{
						continue;
					}
				}
			}
		}

		if ( nTeamsBlocked == MAX_NAV_TEAMS )
		{
			break;
		}
 	}
	return bBlocked;
}


//-----------------------------------------------------------------------------------------------------
/**
  * An entity that can obstruct nav areas.  This is meant for semi-transient areas that obstruct
  * pathfinding but can be ignored for longer-term queries like computing L4D flow distances and
  * escape routes.
  */
class CFuncNavObstruction : public CBaseEntity, public INavAvoidanceObstacle
{
	DECLARE_DATADESC();
	DECLARE_CLASS( CFuncNavObstruction, CBaseEntity );

public:
	void Spawn();
	virtual void UpdateOnRemove( void );

	void InputEnable( inputdata_t &inputdata );
	void InputDisable( inputdata_t &inputdata );

	virtual bool IsPotentiallyAbleToObstructNavAreas( void ) const { return true; }		// could we at some future time obstruct nav?
	virtual float GetNavObstructionHeight( void ) const { return JumpCrouchHeight; }	// height at which to obstruct nav areas
	virtual bool CanObstructNavAreas( void ) const { return !m_bDisabled; }				// can we obstruct nav right this instant?
	virtual CBaseEntity *GetObstructingEntity( void ) { return this; }
	virtual void OnNavMeshLoaded( void )
	{
		if ( !m_bDisabled )
		{
			ObstructNavAreas();
		}
	}

	int DrawDebugTextOverlays( void );

	bool operator()( CNavArea *area );	// functor that obstructs areas in our extent

private:

	void ObstructNavAreas( void );
	bool m_bDisabled;
};



//--------------------------------------------------------------------------------------------------------
BEGIN_DATADESC( CFuncNavObstruction )
	DEFINE_KEYFIELD( m_bDisabled,	FIELD_BOOLEAN,	"StartDisabled" ),
END_DATADESC()


LINK_ENTITY_TO_CLASS( func_nav_avoidance_obstacle, CFuncNavObstruction );


//-----------------------------------------------------------------------------------------------------
int CFuncNavObstruction::DrawDebugTextOverlays( void )
{
	int offset = BaseClass::DrawDebugTextOverlays();

	if (m_debugOverlays & OVERLAY_TEXT_BIT) 
	{
		if ( CanObstructNavAreas() )
		{
			EntityText( offset++, "Obstructing nav", NDEBUG_PERSIST_TILL_NEXT_SERVER );
		}
		else
		{
			EntityText( offset++, "Not obstructing nav", NDEBUG_PERSIST_TILL_NEXT_SERVER );
		}
	}

	return offset;
}


//--------------------------------------------------------------------------------------------------------
void CFuncNavObstruction::UpdateOnRemove( void )
{
	TheNavMesh->UnregisterAvoidanceObstacle( this );

	BaseClass::UpdateOnRemove();
}


//--------------------------------------------------------------------------------------------------------
void CFuncNavObstruction::Spawn( void )
{
	SetMoveType( MOVETYPE_NONE );
	SetModel( STRING( GetModelName() ) );
	AddEffects( EF_NODRAW );
	SetCollisionGroup( COLLISION_GROUP_NONE );
	SetSolid( SOLID_NONE );
	AddSolidFlags( FSOLID_NOT_SOLID );

	if ( !m_bDisabled )
	{
		ObstructNavAreas();
		TheNavMesh->RegisterAvoidanceObstacle( this );
	}
}


//--------------------------------------------------------------------------------------------------------
void CFuncNavObstruction::InputEnable( inputdata_t &inputdata )
{
	m_bDisabled = false;
	ObstructNavAreas();
	TheNavMesh->RegisterAvoidanceObstacle( this );
}


//--------------------------------------------------------------------------------------------------------
void CFuncNavObstruction::InputDisable( inputdata_t &inputdata )
{
	m_bDisabled = true;
	TheNavMesh->UnregisterAvoidanceObstacle( this );
}


//--------------------------------------------------------------------------------------------------------
void CFuncNavObstruction::ObstructNavAreas( void )
{
	Extent extent;
	extent.Init( this );
	TheNavMesh->ForAllAreasOverlappingExtent( *this, extent );
}


//--------------------------------------------------------------------------------------------------------
// functor that blocks areas in our extent
bool CFuncNavObstruction::operator()( CNavArea *area )
{
	area->MarkObstacleToAvoid( GetNavObstructionHeight() );
	return true;
}


//--------------------------------------------------------------------------------------------------------------
