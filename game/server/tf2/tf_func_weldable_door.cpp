//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Func door that's weldable shut
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "beam_shared.h"
#include "tf_player.h"
#include "tf_team.h"
#include "tf_basecombatweapon.h"
#include "tf_func_weldable_door.h"
#include "IEffects.h"


LINK_ENTITY_TO_CLASS( func_door_weldable, CWeldableDoor );

BEGIN_DATADESC( CWeldableDoor )

	// keys 
	DEFINE_KEYFIELD( m_iszWeldPoints, FIELD_STRING, "weldpoints" ),

END_DATADESC()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeldableDoor::Spawn( void )
{
	BaseClass::Spawn();

	m_hWeldingPlayer = NULL;
	m_flMaxWeldedPercentage = 0.0;
	m_iWeldLeader = WL_UNASSIGNED;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeldableDoor::Activate( void )
{
	BaseClass::Activate();

	// Find my weld points
	if ( !m_iszWeldPoints )
	{
		Msg( "func_weldable_door without weldpoints specified.\n" );
		UTIL_Remove( this );
		return;
	}

	// Find the weld points. Doors will almost always have multiple weld point pairs.
	CWeldPoint *pWeldStartPoint = NULL;
	while( (pWeldStartPoint = (CWeldPoint*)gEntList.FindEntityByName( pWeldStartPoint, m_iszWeldPoints ) ) != NULL )
	{
		// Does it have an endpoint specified?
		if ( !pWeldStartPoint->m_target )
		{
			Msg( "func_weldable_door weldpoint '%s' didn't have an endpoint specified.\n", STRING(m_iszWeldPoints) );
			continue;
		}

		// Find the endpoint for this startpoint
		CWeldPoint *pWeldEndPoint = (CWeldPoint*)gEntList.FindEntityByName( NULL, pWeldStartPoint->m_target );
		if ( pWeldEndPoint )
		{
			// Connect the start point to the endpoint
			pWeldStartPoint->SetEndPoint( pWeldEndPoint->GetLocalOrigin() );

			// Add it to the list
			m_aWeldPoints.AddToTail( pWeldStartPoint );
		}
		else
		{
			Msg( "func_weldable_door weldpoint couldn't find it's endpoint of '%s'.\n", STRING(pWeldStartPoint->m_target) );
			continue;
		}
	}

	// Did we find any weldpoint pairs?
	if ( m_aWeldPoints.Size() == 0 )
	{
		Msg( "func_weldable_door couldn't find any weldpoints for '%s'.\n", STRING(m_iszWeldPoints) );
		UTIL_Remove( this );
		return;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if someone's allowed to start welding on this door
//-----------------------------------------------------------------------------
bool CWeldableDoor::IsWeldable( CBaseTFPlayer *pWeldee )
{
	// Can't be welded if I'm open
	if ( m_toggle_state != TS_AT_BOTTOM )
		return false;

	// Can't be welded if I'm already being welded by someone else
	if ( m_hWeldingPlayer != NULL && (((CBaseEntity*)m_hWeldingPlayer) != pWeldee) )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Return the weld leader. 
//			Doors can be made up of multiple door entities, so one of them needs to volunteer to control the welding.
//-----------------------------------------------------------------------------
CWeldableDoor *CWeldableDoor::GetWeldLeader( void )
{
	// Am I the leader?
	if ( m_iWeldLeader == WL_WELD_LEADER )
		return this;

	// If this guy's unassigned, he's volunteering
	if ( m_iWeldLeader == WL_UNASSIGNED )
	{
		m_iWeldLeader = WL_WELD_LEADER;

		// Tell all other friends they're children
		CBaseEntity *pTarget = NULL;
		if ( GetEntityName() != NULL_STRING )
		{
			while ( (pTarget = gEntList.FindEntityByName( pTarget, GetEntityName() )) != NULL )
			{
				if ( pTarget != this )
				{
					CWeldableDoor *pWeldableDoor = dynamic_cast< CWeldableDoor * >( pTarget );
					if ( pWeldableDoor )
					{
						pWeldableDoor->m_iWeldLeader = WL_WELD_CHILD;
					}
				}
			}
		}

		return this;
	}

	// We're not the leader. so find him
	CBaseEntity *pTarget = NULL;
	if ( GetEntityName() != NULL_STRING )
	{
		while ( (pTarget = gEntList.FindEntityByName( pTarget, GetEntityName() )) != NULL )
		{
			if ( pTarget != this )
			{
				CWeldableDoor *pWeldableDoor = dynamic_cast< CWeldableDoor * >( pTarget );
				if ( pWeldableDoor && pWeldableDoor->m_iWeldLeader == WL_WELD_LEADER )
					return pWeldableDoor;
			}
		}
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: The Player's going to start welding this door.
//-----------------------------------------------------------------------------
void CWeldableDoor::StartWelding( CBaseTFPlayer *pWeldee )
{
	m_hWeldingPlayer = pWeldee;

	// If this is the weld leader, tell all the door pieces that the player's welding them
	if ( m_iWeldLeader == WL_WELD_LEADER )
	{
		CBaseEntity *pTarget = NULL;
		if ( GetEntityName() != NULL_STRING )
		{
			while ( (pTarget = gEntList.FindEntityByName( pTarget, GetEntityName() )) != NULL )
			{
				if ( pTarget != this )
				{
					CWeldableDoor *pWeldableDoor = dynamic_cast< CWeldableDoor * >( pTarget );
					if ( pWeldableDoor )
					{
						pWeldableDoor->StartWelding( pWeldee );
					}
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: The player's stopped welding this door
//-----------------------------------------------------------------------------
void CWeldableDoor::StopWelding( void )
{
	m_hWeldingPlayer = NULL;

	// If this is the weld leader, tell all the door pieces that the player's stopped welding them
	if ( m_iWeldLeader == WL_WELD_LEADER )
	{
		CBaseEntity *pTarget = NULL;
		if ( GetEntityName() != NULL_STRING )
		{
			while ( (pTarget = gEntList.FindEntityByName( pTarget, GetEntityName() )) != NULL )
			{
				if ( pTarget != this )
				{
					CWeldableDoor *pWeldableDoor = dynamic_cast< CWeldableDoor * >( pTarget );
					if ( pWeldableDoor )
					{
						pWeldableDoor->StopWelding();
					}
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Return the amount the door's been welded
//-----------------------------------------------------------------------------
float CWeldableDoor::GetWeldPercentage( void )
{
	return m_flWeldedPercentage;
}

//-----------------------------------------------------------------------------
// Purpose: Update the amount the door's been welded
//-----------------------------------------------------------------------------
void CWeldableDoor::UpdateWeld( bool bCutting, float flWeldPercentage )
{
	if ( m_flMaxWeldedPercentage < flWeldPercentage )
		m_flMaxWeldedPercentage = flWeldPercentage;
	m_flWeldedPercentage = flWeldPercentage;

	// Did we cut away the entire weld?
	if ( m_flWeldedPercentage <= 0.0 )
	{
		// Clear welded
		m_flMaxWeldedPercentage = 0.0;

		if ( m_bLocked )
		{
			// Unlock all the pieces of this door
			m_bLocked = false;

			CBaseEntity *pTarget = NULL;
			if ( GetEntityName() != NULL_STRING )
			{
				while ( (pTarget = gEntList.FindEntityByName( pTarget, GetEntityName() )) != NULL )
				{
					if ( pTarget != this )
					{
						CWeldableDoor *pWeldableDoor = dynamic_cast< CWeldableDoor * >( pTarget );
						if ( pWeldableDoor )
						{
							pWeldableDoor->Unlock();
						}
					}
				}
			}
		}
	}
	else
	{
		if ( !m_bLocked )
		{
			// Lock all the pieces of this door
			m_bLocked = true;

			CBaseEntity *pTarget = NULL;
			if ( GetEntityName() != NULL_STRING )
			{
				while ( (pTarget = gEntList.FindEntityByName( pTarget, GetEntityName() )) != NULL )
				{
					if ( pTarget != this )
					{
						CWeldableDoor *pWeldableDoor = dynamic_cast< CWeldableDoor * >( pTarget );
						if ( pWeldableDoor )
						{
							pWeldableDoor->Lock();
						}
					}
				}
			}
		}
	}

	// Update the beams
	for ( int i = 0; i < m_aWeldPoints.Size(); i++ )
	{
		// First time we've updated?
		if ( m_aWeldBeams.Size() <= i )
		{
			CBeam *pBeam = CBeam::BeamCreate( "sprites/physbeam.vmt", 4.0 );
			pBeam->SetColor( 128, 128, 128 ); 
			pBeam->SetBrightness( 128 );
			pBeam->PointsInit( m_aWeldPoints[i]->GetStartPoint(), m_aWeldPoints[i]->GetEndPoint() );
			m_aWeldBeams.AddToTail( pBeam );
		}
		if ( m_aCutBeams.Size() <= i )
		{
			CBeam *pBeam = CBeam::BeamCreate( "sprites/physbeam.vmt", 4.0 );
			pBeam->SetColor( 255, 255, 255 ); 
			pBeam->SetBrightness( 255 );
			pBeam->PointsInit( m_aWeldPoints[i]->GetStartPoint(), m_aWeldPoints[i]->GetEndPoint() );
			m_aCutBeams.AddToTail( pBeam );
		}

		// Figure out how far we've welded
		Vector vecLine = ( m_aWeldPoints[i]->GetEndPoint() - m_aWeldPoints[i]->GetStartPoint());
		float flLength = vecLine.Length();
		VectorNormalize(vecLine);
		vecLine = vecLine * (flLength * flWeldPercentage);
		Vector vecWeldPoint = m_aWeldPoints[i]->GetStartPoint() + vecLine;

		// Update the beams
		m_aWeldBeams[i]->SetStartPos( m_aWeldPoints[i]->GetStartPoint() );
		m_aWeldBeams[i]->SetEndPos( vecWeldPoint );
		m_aWeldBeams[i]->RelinkBeam();
		if ( flWeldPercentage <= m_flMaxWeldedPercentage )
		{
			// Get the cut point
			VectorNormalize(vecLine);
			vecLine = vecLine * (flLength * m_flMaxWeldedPercentage);
			Vector vecCutPoint = m_aWeldPoints[i]->GetStartPoint() + vecLine;

			m_aCutBeams[i]->SetEndPos( vecWeldPoint );
			m_aCutBeams[i]->SetStartPos( vecCutPoint );
			m_aCutBeams[i]->RelinkBeam();
		}

		// Some sparks
		if ( random->RandomInt(0,2) == 0 )
		{
			g_pEffects->Sparks( vecWeldPoint );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Find a weld point to watch for this player
//-----------------------------------------------------------------------------
Vector CWeldableDoor::GetPlayerWeldPoint( void )
{
	CBaseTFPlayer *pPlayer = (CBaseTFPlayer *)(CBaseEntity*)m_hWeldingPlayer;
	Vector vecSrc = pPlayer->Weapon_ShootPosition( );
	trace_t tr;

	for ( int i = 0; i < m_aWeldPoints.Size(); i++ )
	{
		// Figure out where the weldpoint is
		Vector vecLine = ( m_aWeldPoints[i]->GetEndPoint() - m_aWeldPoints[i]->GetStartPoint());
		float flLength = vecLine.Length();
		VectorNormalize(vecLine);
		vecLine = vecLine * (flLength * m_flWeldedPercentage);
		Vector vecWeldPoint = m_aWeldPoints[i]->GetStartPoint() + vecLine;

		// Is the weld point visible to our player?
		UTIL_TraceLine( vecSrc, vecWeldPoint, MASK_SOLID, pPlayer, TFCOLLISION_GROUP_WEAPON, &tr );
		if ( tr.fraction == 1.0 )
			return vecWeldPoint;
	}

	return vec3_origin;
}


//============================================================================================================
// WELD POINTS
//============================================================================================================

LINK_ENTITY_TO_CLASS( info_weldpoint, CWeldPoint );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeldPoint::Spawn( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeldPoint::SetEndPoint( const Vector &vecEndPoint )
{
	m_vecEndPoint = vecEndPoint;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const Vector &CWeldPoint::GetStartPoint( void ) const
{
	return GetLocalOrigin();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const Vector &CWeldPoint::GetEndPoint( void ) const
{
	return m_vecEndPoint;
}
