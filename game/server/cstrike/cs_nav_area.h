//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// nav_area.h
// Navigation areas
// Author: Michael S. Booth (mike@turtlerockstudios.com), January 2003

#ifndef _CS_NAV_AREA_H_
#define _CS_NAV_AREA_H_

#include "nav_area.h"

//-------------------------------------------------------------------------------------------------------------------
/**
 * A CNavArea is a rectangular region defining a walkable area in the environment
 */
class CCSNavArea : public CNavArea
{
public:
	DECLARE_CLASS( CCSNavArea, CNavArea );

	CCSNavArea( void );
	~CCSNavArea();

	virtual void OnServerActivate( void );						// (EXTEND) invoked when map is initially loaded
	virtual void OnRoundRestart( void );						// (EXTEND) invoked for each area when the round restarts

	virtual void Draw( void ) const;							// draw area for debugging & editing

	virtual void Save( CUtlBuffer &fileBuffer, unsigned int version ) const;	// (EXTEND)
	virtual NavErrorType Load( CUtlBuffer &fileBuffer, unsigned int version, unsigned int subVersion );		// (EXTEND)
	virtual NavErrorType PostLoad( void );								// (EXTEND) invoked after all areas have been loaded - for pointer binding, etc

	virtual void CustomAnalysis( bool isIncremental = false );		// for game-specific analysis

	//- approach areas ----------------------------------------------------------------------------------
	struct ApproachInfo
	{
		NavConnect here;										///< the approach area
		NavConnect prev;										///< the area just before the approach area on the path
		NavTraverseType prevToHereHow;
		NavConnect next;										///< the area just after the approach area on the path
		NavTraverseType hereToNextHow;
	};
	const ApproachInfo *GetApproachInfo( int i ) const	{ return &m_approach[i]; }
	int GetApproachInfoCount( void ) const				{ return m_approachCount; }
	void ComputeApproachAreas( void );							///< determine the set of "approach areas" - for map learning

	//- player counting --------------------------------------------------------------------------------
	void ClearPlayerCount( void );								///< set the player count to zero

protected:
	NavErrorType LoadLegacy( CUtlBuffer &fileBuffer, unsigned int version, unsigned int subVersion );


private:
	//- approach areas ----------------------------------------------------------------------------------
	enum { MAX_APPROACH_AREAS = 16 };
	ApproachInfo m_approach[ MAX_APPROACH_AREAS ];
	unsigned char m_approachCount;
};

//--------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------
//
// Inlines
//

inline void CCSNavArea::ClearPlayerCount( void )
{
	for( int i=0; i<MAX_NAV_TEAMS; ++i )
	{
		m_playerCount[ i ] = 0;
	}
}

#endif // _CS_NAV_AREA_H_
