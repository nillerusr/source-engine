//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Entity that propagates general data needed by clients for every player.
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_DOD_OBJECTIVE_RESOURCE_H
#define C_DOD_OBJECTIVE_RESOURCE_H
#ifdef _WIN32
#pragma once
#endif

#include "dod_shareddefs.h"
#include "const.h"
#include "c_baseentity.h"
#include <igameresources.h>


class C_DODObjectiveResource : public C_BaseEntity
{
	DECLARE_CLASS( C_DODObjectiveResource, C_BaseEntity );
public:
	DECLARE_CLIENTCLASS();

					C_DODObjectiveResource();
	virtual			~C_DODObjectiveResource();

public:

	float GetCPCapPercentage( int index );
	int GetNumControlPoints( void ) { return m_iNumControlPoints; }

	void SetOwningTeam( int index, int team );
	void SetCappingTeam( int index, int team );

	// Is the point visible in the objective display
	bool IsCPVisible( int index )
	{
		Assert( index < m_iNumControlPoints );

		return m_bCPIsVisible[index];
	}

	// Get the world location of this control point
	Vector& GetCPPosition( int index )
	{
		Assert( index < m_iNumControlPoints );
		return m_vCPPositions[index];
	}

	int GetOwningTeam( int index )
	{
		if ( index >= m_iNumControlPoints )
			return TEAM_UNASSIGNED;

		return m_iOwner[index];
	}	

	int GetCappingTeam( int index )
	{
		if ( index >= m_iNumControlPoints )
			return TEAM_UNASSIGNED;

		return m_iCappingTeam[index];
	}

	// Icons
	int GetCPCurrentOwnerIcon( int index )
	{
		Assert( index < m_iNumControlPoints );

		int iOwner = GetOwningTeam(index);

		return GetIconForTeam( index, iOwner );
	}

	int GetCPCappingIcon( int index )
	{
		Assert( index < m_iNumControlPoints );

		int iCapper = GetCappingTeam(index);

		Assert( iCapper != TEAM_UNASSIGNED );

		return GetIconForTeam( index, iCapper );;
	}

	int GetCPTimerCapIcon( int index )
	{
		Assert( index < m_iNumControlPoints );

        return m_iTimerCapIcons[index];
	}

	int GetCPBombedIcon( int index )
	{
		Assert( index < m_iNumControlPoints );

		return m_iBombedIcons[index];
	}

	int GetIconForTeam( int index, int team )
	{
		int icon = 0;

		switch ( team )
		{
		case TEAM_ALLIES:
			icon = m_iAlliesIcons[index];
			break;
		case TEAM_AXIS:
			icon = m_iAxisIcons[index];
			break;
		case TEAM_UNASSIGNED:
			icon = m_iNeutralIcons[index];
			break;
		default:
			Assert(0);
			break;
		}

		return icon;
	}

	// Number of players in the area
	int GetNumPlayersInArea( int index, int team )
	{
		Assert( index < m_iNumControlPoints );

		int num = 0;

		switch ( team )
		{
		case TEAM_ALLIES:
			num = m_iNumAllies[index];
			break;
		case TEAM_AXIS:
			num = m_iNumAxis[index];
			break;
		default:
			Assert(0);
			break;
		}

		return num;
	}
	
	// get the required cappers for the passed team
	int GetRequiredCappers( int index, int team )
	{
		Assert( index < m_iNumControlPoints );

		int num = 0;

		switch ( team )
		{
		case TEAM_ALLIES:
			num = m_iAlliesReqCappers[index];
			break;
		case TEAM_AXIS:
			num = m_iAxisReqCappers[index];
			break;
		default:
			Assert(0);
			break;
		}

		return num;
	}

	void SetBombPlanted( int index, int iBombPlanted );

	void SetBombBeingDefused( int index, bool bBombBeingDefused )
	{
		m_bBombBeingDefused[index] = bBombBeingDefused;
	}

	bool IsBombSetAtPoint( int index )
	{
		return m_bBombPlanted[index];
	}

	bool IsBombBeingDefused( int index )
	{
		return m_bBombBeingDefused[index];
	}

	float GetBombTimeForPoint( int index )
	{
		return MAX( 0, m_flBombEndTimes[index] - gpGlobals->curtime );
	}

	int GetBombsRequired( int index )
	{
		return m_iBombsRequired[index];
	}

	int GetBombsRemaining( int index )
	{
		return m_iBombsRemaining[index];
	}

protected:

	int		m_iNumControlPoints;

	// data variables
	Vector	m_vCPPositions[MAX_CONTROL_POINTS];
	bool	m_bCPIsVisible[MAX_CONTROL_POINTS];
	int		m_iAlliesIcons[MAX_CONTROL_POINTS];
	int		m_iAxisIcons[MAX_CONTROL_POINTS];
	int		m_iNeutralIcons[MAX_CONTROL_POINTS];
	int		m_iTimerCapIcons[MAX_CONTROL_POINTS];
	int		m_iBombedIcons[MAX_CONTROL_POINTS];
	int		m_iAlliesReqCappers[MAX_CONTROL_POINTS];
	int		m_iAxisReqCappers[MAX_CONTROL_POINTS];
	float	m_flAlliesCapTime[MAX_CONTROL_POINTS];
	float	m_flAxisCapTime[MAX_CONTROL_POINTS];
	bool	m_bBombPlanted[MAX_CONTROL_POINTS];
	int		m_iBombsRequired[MAX_CONTROL_POINTS];
	int		m_iBombsRemaining[MAX_CONTROL_POINTS];
	bool	m_bBombBeingDefused[MAX_CONTROL_POINTS];

	// state variables
	int		m_iNumAllies[MAX_CONTROL_POINTS];
	int		m_iNumAxis[MAX_CONTROL_POINTS];
	int		m_iCappingTeam[MAX_CONTROL_POINTS];
	int		m_iOwner[MAX_CONTROL_POINTS];

	// client calculated state
	float	m_flCapStartTimes[MAX_CONTROL_POINTS];
	float	m_flCapEndTimes[MAX_CONTROL_POINTS];
	float   m_flBombEndTimes[MAX_CONTROL_POINTS];
};

extern C_DODObjectiveResource *g_pObjectiveResource;

#endif // C_DOD_OBJECTIVE_RESOURCE_H
