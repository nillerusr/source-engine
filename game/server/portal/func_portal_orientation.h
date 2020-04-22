//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Volume entity which overrides the placement angles of a portal placed within its bounds.
//
// $NoKeywords: $
//======================================================================================//

#ifndef _FUNC_PORTAL_ORIENTATION_H_
#define _FUNC_PORTAL_ORIENTATION_H_

#ifdef _WIN32
#pragma once
#endif

#include "cbase.h"

class CFuncPortalOrientation : public CBaseEntity
{
public:
	DECLARE_CLASS( CFuncPortalOrientation, CBaseEntity );
	DECLARE_DATADESC();

	CFuncPortalOrientation();
	~CFuncPortalOrientation();

	// Overloads from base entity
	virtual void	Spawn( void );
	
	void			OnActivate ( void );

	// Inputs to flip functionality on and off
	void InputEnable( inputdata_t &inputdata );
	void InputDisable( inputdata_t &inputdata );

	bool IsActive() { return !m_bDisabled; }	// is this area causing portals to lock orientation

	bool					m_bMatchLinkedAngles;
	QAngle					m_vecAnglesToFace;

	CFuncPortalOrientation		*m_pNext;			// Needed for the template list
	unsigned int				m_iListIndex;
private:
	bool					m_bDisabled;				// are we currently locking portal orientations
};

CFuncPortalOrientation* GetPortalOrientationVolumeList();

// Upon portal placement, test for orientation changing volumes
bool UTIL_TestForOrientationVolumes( QAngle& vecCurAngles, const Vector& vecCurOrigin, const CProp_Portal* pPortal );

#endif //_FUNC_PORTAL_ORIENTATION_H_