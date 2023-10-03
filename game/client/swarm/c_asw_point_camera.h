//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_ASW_POINTCAMERA_H
#define C_ASW_POINTCAMERA_H
#ifdef _WIN32
#pragma once
#endif

#include "c_point_camera.h"

class C_ASW_PointCamera : public C_PointCamera
{
public:
	DECLARE_CLASS( C_ASW_PointCamera, C_PointCamera );
	DECLARE_CLIENTCLASS();

public:
	C_ASW_PointCamera();

	void ApplyStimCamSettings();
	void SetActive( bool bActive ) { m_bActive = bActive; }

	bool m_bSecurityCam;
};

#endif // C_ASW_POINTCAMERA_H
