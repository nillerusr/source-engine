//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose:		Used to deal with AI navigation
//
// $Workfile:     $
// $Date:         $
//===========================================================================//

#ifndef AI_LINK_H
#define AI_LINK_H

#ifdef _WIN32
#pragma once
#endif

#include "ai_hull.h"	// For num hulls

enum Link_Info_t
{
	bits_LINK_STALE_SUGGESTED	=	0x01,		// NPC found this link to be blocked
	bits_LINK_OFF				=	0x02,		// This link has been turned off
	bits_LINK_PRECISE_MOVEMENT	=	0x04,		// This link requires precise movement near it (for moving through doors for NPCs w/ sloppy movement)
	bits_PREFER_AVOID			=	0x08,		// This link has been marked as to prefer not to use it
	bits_LINK_ASW_BASHABLE		=   0x10,		// This link is marked as blocked by a door that certain NPCs can break down
};

// for most purposes a bashable door link is considered impassable (only special case NPCs can break them down and they
// need custom behaviors)

//=============================================================================
//	>> CAI_Link
//=============================================================================

class CAI_DynamicLink;

#define AI_MOVE_TYPE_BITS ( bits_CAP_MOVE_GROUND | bits_CAP_MOVE_JUMP | bits_CAP_MOVE_FLY | bits_CAP_MOVE_CLIMB | bits_CAP_MOVE_SWIM | bits_CAP_MOVE_CRAWL )

class CAI_Link
{
public:
	short	m_iSrcID;							// the node that 'owns' this link
	short	m_iDestID;							// the node on the other end of the link. 
	
	inline int	DestNodeID( int srcID );			// Given the source node ID, returns the destination ID

	byte 	m_iAcceptedMoveTypes[NUM_HULLS];	// Capability_T of motions acceptable for each hull type

	byte	m_LinkInfo;							// other information about this link

	float	m_timeStaleExpires;
	int		m_nDangerCount;						// How many dangerous things are near this link?

	CAI_DynamicLink *m_pDynamicLink;
	
private:
	friend class CAI_Network;
	CAI_Link(void);
};


//-----------------------------------------------------------------------------
// Purpose:	Given the source node ID, returns the destination ID
// Input  :
// Output :
//-----------------------------------------------------------------------------	
inline int	CAI_Link::DestNodeID(int srcID)
{
	// hardware op for ( srcID==m_iSrcID ? m_iDestID : m_iSrcID )
	return ieqsel( srcID, m_iSrcID, m_iDestID, m_iSrcID ); 
}


#endif // AI_LINK_H
