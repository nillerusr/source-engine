//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:		
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "ai_link.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


ASSERT_INVARIANT( ( bits_LINK_STALE_SUGGESTED | bits_LINK_OFF ) <= 255 && ( AI_MOVE_TYPE_BITS <= 255 ) );
//-----------------------------------------------------------------------------
// Purpose: Constructor
// Input  :
// Output :
//-----------------------------------------------------------------------------
CAI_Link::CAI_Link(void)
{
	m_iSrcID			= -1;
	m_iDestID			= -1;
	m_LinkInfo			= 0;
	m_timeStaleExpires	= 0;
	m_pDynamicLink		= NULL;
	m_nDangerCount		= 0;
	for (int hull=0;hull<NUM_HULLS;hull++)
	{
		m_iAcceptedMoveTypes[hull] = 0;
	}

};
