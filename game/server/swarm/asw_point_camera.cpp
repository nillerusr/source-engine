//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "igamesystem.h"
#include "asw_point_camera.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS( asw_point_camera, CASW_PointCamera );
LINK_ENTITY_TO_CLASS( point_camera, CASW_PointCamera );

CASW_PointCamera::CASW_PointCamera()
{
	m_bSecurityCam = false;
}

BEGIN_DATADESC( CASW_PointCamera )
	DEFINE_FIELD( m_bSecurityCam,	FIELD_BOOLEAN ),
END_DATADESC()

IMPLEMENT_SERVERCLASS_ST( CASW_PointCamera, DT_ASW_PointCamera )
	SendPropBool( SENDINFO( m_bSecurityCam ) ),	
END_SEND_TABLE()
