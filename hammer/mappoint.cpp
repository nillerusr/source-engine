//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "stdafx.h"
#include "Box3D.h"
#include "MapDefs.h" // for COORD_NOTINIT
#include "MapPoint.h"
#include "hammer_mathlib.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


//-----------------------------------------------------------------------------
// Purpose: Constructor. Initializes the origin point to all zeros.
//-----------------------------------------------------------------------------
CMapPoint::CMapPoint(void)
{
	m_Origin.Init();
}


//-----------------------------------------------------------------------------
// Purpose: Returns this point's X, Y, Z coordinates.
//-----------------------------------------------------------------------------
void CMapPoint::GetOrigin(Vector &Origin)
{
	Origin = m_Origin;
}


//-----------------------------------------------------------------------------
// Purpose: Sets this point's X, Y, Z coordinates.
//-----------------------------------------------------------------------------
void CMapPoint::SetOrigin(Vector &Origin)
{
	m_Origin = Origin;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pTransBox - 
//-----------------------------------------------------------------------------
void CMapPoint::DoTransform(const VMatrix &matrix)
{
	TransformPoint( matrix, m_Origin );
}

