//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Dme version of a hitbox
//
//===========================================================================//

#ifndef DMEHITBOX_H
#define DMEHITBOX_H

#ifdef _WIN32
#pragma once
#endif

#include "movieobjects/dmeshape.h"


//-----------------------------------------------------------------------------
// Forward Declarations
//-----------------------------------------------------------------------------
class CDmeDrawSettings;
struct matrix3x4_t;


//-----------------------------------------------------------------------------
// A class representing an attachment point
//-----------------------------------------------------------------------------
class CDmeHitbox : public CDmeShape
{
	DEFINE_ELEMENT( CDmeHitbox, CDmeShape );

public:
	virtual void Draw( const matrix3x4_t &shapeToWorld, CDmeDrawSettings *pDrawSettings = NULL );

	CDmaString m_SurfaceProperty;
	CDmaString m_Group;
	CDmaString m_Bone;
	CDmaVar< Vector > m_vecMins;	
	CDmaVar< Vector > m_vecMaxs;
	CDmaColor m_RenderColor;	// used for visualization

private:

};


#endif // DMEHITBOX_H
