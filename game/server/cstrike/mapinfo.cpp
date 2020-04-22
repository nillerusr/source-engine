//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "mapinfo.h"
#include "cs_gamerules.h"

LINK_ENTITY_TO_CLASS( info_map_parameters, CMapInfo );

BEGIN_DATADESC( CMapInfo )

	DEFINE_INPUTFUNC( FIELD_INTEGER, "FireWinCondition", InputFireWinCondition ),

END_DATADESC()

CMapInfo *g_pMapInfo = NULL;


CMapInfo::CMapInfo()
{
	m_flBombRadius = 500.0f;
	m_iBuyingStatus = 0;

	if ( g_pMapInfo )
	{
		// Should only be one of these.
		Warning( "Warning: Multiple info_map_parameters entities in map!\n" );
	}
	else
	{
		g_pMapInfo = this;
	}
}


CMapInfo::~CMapInfo()
{
	if ( g_pMapInfo == this )
		g_pMapInfo = NULL;
}
 

bool CMapInfo::KeyValue( const char *szKeyName, const char *szValue )
{
	if (FStrEq(szKeyName, "buying"))
	{
		m_iBuyingStatus = atoi(szValue);
		return true;
	}
	else if (FStrEq(szKeyName, "bombradius"))
	{
		m_flBombRadius = (float)(atoi(szValue));
		if (m_flBombRadius > 2048)
			m_flBombRadius = 2048;
		
		return true;
	}
	
	return BaseClass::KeyValue( szKeyName, szValue );
}


void CMapInfo::Spawn( void )
{ 
	SetMoveType( MOVETYPE_NONE );
	SetSolid( SOLID_NONE );
	AddEffects( EF_NODRAW );
}

void CMapInfo::InputFireWinCondition(inputdata_t &inputdata )
{
	CSGameRules()->TerminateRound( 5, inputdata.value.Int() );
}
