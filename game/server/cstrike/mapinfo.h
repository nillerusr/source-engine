//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef MAPINFO_H
#define MAPINFO_H
#ifdef _WIN32
#pragma once
#endif


#include "baseentity.h"


class CMapInfo : public CPointEntity
{
public :

	DECLARE_DATADESC();
	DECLARE_CLASS( CMapInfo, CPointEntity );
	
	CMapInfo();
	virtual ~CMapInfo();

	bool KeyValue( const char *szKeyName, const char *szValue );
	void Spawn();

	void InputFireWinCondition( inputdata_t &inputdata );

public:
	int m_iBuyingStatus;
	float m_flBombRadius;
};


// The info_map_parameters entity in this map (only one is allowed for).
extern CMapInfo *g_pMapInfo;


#endif // MAPINFO_H
