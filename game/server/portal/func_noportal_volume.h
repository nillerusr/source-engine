//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: A volume in which no portal can be placed. Keeps a global list loaded in from the map
//			and provides an interface with which prop_portal can get this list and avoid successfully
//			creating portals wholly or partially inside the volume.
//
// $NoKeywords: $
//======================================================================================//

#ifndef _FUNC_NOPORTAL_VOLUME_H_
#define _FUNC_NOPORTAL_VOLUME_H_

#ifdef _WIN32
#pragma once
#endif

#include "cbase.h"

class CFuncNoPortalVolume : public CBaseEntity
{
public:
	DECLARE_CLASS( CFuncNoPortalVolume, CBaseEntity );

	CFuncNoPortalVolume();
	~CFuncNoPortalVolume();

	// Overloads from base entity
	virtual void	Spawn( void );
	
	void OnActivate( void );

	// Inputs to flip functionality on and off
	void InputActivate( inputdata_t &inputdata );
	void InputDeactivate( inputdata_t &inputdata );
	void InputToggle( inputdata_t &inputdata );

	// misc public methods
	unsigned int GetIndex () { return m_iListIndex; } // returns the list index of this camera
	bool IsActive() { return m_bActive; }	// is this area currently blocking portals

	CFuncNoPortalVolume		*m_pNext;			// Needed for the template list	

	DECLARE_DATADESC();

private:
	bool					m_bActive;			// are we currently blocking portals
	unsigned int			m_iListIndex;		// what is my index into the global noportal_volume list
	
	
};

// Global interface for getting the list of noportal_volumes
CFuncNoPortalVolume* GetNoPortalVolumeList();


#endif