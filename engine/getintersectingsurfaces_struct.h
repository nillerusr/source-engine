//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef GETINTERSECTINGSURFACES_STRUCT_H
#define GETINTERSECTINGSURFACES_STRUCT_H
#ifdef _WIN32
#pragma once
#endif

struct model_t;
class Vector;
class SurfInfo;


// Used by GetIntersectingSurfaces.
class GetIntersectingSurfaces_Struct
{
public:
	model_t			*m_pModel;
	const Vector	*m_pCenter;
	const byte		*m_pCenterPVS;	// PVS for the leaf m_pCenter is in.
	float			m_Radius;
	bool			m_bOnlyVisible;

	SurfInfo		*m_pInfos;
	int				m_nMaxInfos;
	int				m_nSetInfos;
};


#endif // GETINTERSECTINGSURFACES_STRUCT_H
