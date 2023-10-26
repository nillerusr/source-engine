//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef ASW_POINT_CAMERA_H
#define ASW_POINT_CAMERA_H
#ifdef _WIN32
#pragma once
#endif

#include "cbase.h"
#include "point_camera.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CASW_PointCamera : public CPointCamera
{
public:
	DECLARE_CLASS( CASW_PointCamera, CPointCamera );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();
	CASW_PointCamera();

	CNetworkVar( bool, m_bSecurityCam );
};
#endif // ASW_POINT_CAMERA_H
