//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#include "cbase.h"
#include "c_asw_point_camera.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_CLIENTCLASS_DT( C_ASW_PointCamera, DT_ASW_PointCamera, CASW_PointCamera )
	RecvPropBool( RECVINFO( m_bSecurityCam ) ),
END_RECV_TABLE()

C_ASW_PointCamera::C_ASW_PointCamera()
{
	m_bSecurityCam = false;
}

void C_ASW_PointCamera::ApplyStimCamSettings()
{
	m_bActive = false;
	m_FOV = 90;
	m_bFogEnable = true;
	m_bUseScreenAspectRatio = false;
	m_flFogStart = 400;
	m_flFogEnd = 800;
	m_FogColor.r = 0;
	m_FogColor.g = 0;
	m_FogColor.b = 0;
	m_Resolution = 0;
	m_bNoSky = true;
	m_fBrightness = 0.125f;
}