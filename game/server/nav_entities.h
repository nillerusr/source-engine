//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// nav_entities.h
// Navigation entities
// Author: Michael S. Booth (mike@turtlerockstudios.com), January 2003

#ifndef NAV_ENTITIES_H
#define NAV_ENTITIES_H

//-----------------------------------------------------------------------------------------------------
/**
  * An entity that can block/unblock nav areas.  This is meant for semi-transient areas that block
  * pathfinding but can be ignored for longer-term queries like computing L4D flow distances and
  * escape routes.
  */
class CFuncNavBlocker : public CBaseEntity
{
	DECLARE_DATADESC();
	DECLARE_CLASS( CFuncNavBlocker, CBaseEntity );

public:
	void Spawn();
	virtual void UpdateOnRemove( void );

	void InputBlockNav( inputdata_t &inputdata );
	void InputUnblockNav( inputdata_t &inputdata );

	inline bool IsBlockingNav( int teamNumber ) const
	{
		if ( teamNumber == TEAM_ANY )
		{
			bool isBlocked = false;
			for ( int i=0; i<MAX_NAV_TEAMS; ++i )
			{
				isBlocked |= m_isBlockingNav[ i ];
			}

			return isBlocked;
		}

		teamNumber = teamNumber % MAX_NAV_TEAMS;
		return m_isBlockingNav[ teamNumber ];
	}

	int DrawDebugTextOverlays( void );

	bool operator()( CNavArea *area );	// functor that blocks areas in our extent

	static bool CalculateBlocked( bool *pResultByTeam, const Vector &vecMins, const Vector &vecMaxs );

private:

	void UpdateBlocked();

	static CUtlLinkedList<CFuncNavBlocker *> gm_NavBlockers;

	void BlockNav( void );
	void UnblockNav( void );
	bool m_isBlockingNav[MAX_NAV_TEAMS];
	int m_blockedTeamNumber;
	bool m_bDisabled;
	Vector m_CachedMins, m_CachedMaxs;

};

#endif // NAV_ENTITIES_H
