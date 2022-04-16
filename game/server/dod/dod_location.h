//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================

#ifndef DOD_LOCATION_H
#define DOD_LOCATION_H
#ifdef _WIN32
#pragma once
#endif

#include "baseentity.h"

//a location on the map that players can refernce in their say messages
//with the %l escape sequence
class CDODLocation : public CBaseEntity
{
public: 
	DECLARE_CLASS( CDODLocation, CBaseEntity );
	CDODLocation()
	{
		m_szLocationName[0] = '\0';
	}

	virtual void Spawn( void );
	virtual bool KeyValue( const char *szKeyName, const char *szValue );

	inline const char *GetName( void ) { return m_szLocationName; }

private:
	char m_szLocationName[64];

private:
	CDODLocation( const CDODLocation & );
};

#endif //DOD_LOCATION_H