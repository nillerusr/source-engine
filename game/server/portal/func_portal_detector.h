//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: A volume which fires an output when a portal is placed in it.
//
// $NoKeywords: $
//======================================================================================//

#ifndef _FUNC_PORTAL_DETECTOR_H_
#define _FUNC_PORTAL_DETECTOR_H_

#ifdef _WIN32
#pragma once
#endif

#include "cbase.h"

class CFuncPortalDetector : public CBaseEntity
{
public:
	DECLARE_CLASS( CFuncPortalDetector, CBaseEntity );

	// Overloads from base entity
	virtual void	Spawn( void );
	
	void OnActivate( void );

	// Inputs to flip functionality on and off
	void InputDisable( inputdata_t &inputdata );
	void InputEnable( inputdata_t &inputdata );
	void InputToggle( inputdata_t &inputdata );

	// misc public methods
	bool IsActive( void ) { return m_bActive; }	// is this area currently detecting portals
	int GetLinkageGroupID( void ) { return m_iLinkageGroupID; }

	COutputEvent m_OnStartTouchPortal1;
	COutputEvent m_OnStartTouchPortal2;
	COutputEvent m_OnStartTouchLinkedPortal;
	COutputEvent m_OnStartTouchBothLinkedPortals;

	DECLARE_DATADESC();

private:
	bool	m_bActive;			// are we currently detecting portals
	int		m_iLinkageGroupID;	// what set of portals are we testing for?
	
};

#endif
