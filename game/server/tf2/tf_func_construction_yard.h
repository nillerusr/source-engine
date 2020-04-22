//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Place where we can build vehicles
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_FUNC_CONSTRUCTION_YARD_H
#define TF_FUNC_CONSTRUCTION_YARD_H

#ifdef _WIN32
#pragma once
#endif

class CBaseObject;
class Vector;

//-----------------------------------------------------------------------------
// Is a given point contained within any construction yard?
//-----------------------------------------------------------------------------
bool PointInConstructionYard( const Vector &vecBuildOrigin );

//-----------------------------------------------------------------------------
// Does a construction yard (or lack of one) prevent us from building?
//-----------------------------------------------------------------------------

bool ConstructionYardPreventsBuild( CBaseObject *pObject, const Vector &vecBuildOrigin );


#endif // TF_FUNC_CONSTRUCTION_YARD_H
