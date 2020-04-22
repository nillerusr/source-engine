//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef INFO_CUSTOMTECH_H
#define INFO_CUSTOMTECH_H
#ifdef _WIN32
#pragma once
#endif

#include "entityoutput.h"
#include "baseentity.h"

//-----------------------------------------------------------------------------
// Purpose: Map entity that adds a custom technology to the techtree
//-----------------------------------------------------------------------------
class CInfoCustomTechnology : public CPointEntity
{
	DECLARE_CLASS( CInfoCustomTechnology, CPointEntity );
public:
	void	Spawn( void );
	void	Activate( void );
	void	UpdateTechPercentage( float flPercentage );

	virtual int UpdateTransmitState() { return SetTransmitState( FL_EDICT_FULLCHECK ); };
	virtual int ShouldTransmit( const CCheckTransmitInfo *pInfo );

	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

public:
	COutputFloat m_flTechPercentage;	// Percentage of the tech that's owned
	string_t	 m_iszTech;
	string_t	 m_iszTechTreeFile;

	// Sent via datatable
	CNetworkString( m_szTechTreeFile, 128 );
};

#endif // INFO_CUSTOMTECH_H
