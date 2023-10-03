//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef ASW_PLAYERLOCALDATA_H
#define ASW_PLAYERLOCALDATA_H
#ifdef _WIN32
#pragma once
#endif

#include "networkvar.h"

#include "hl_movedata.h"

//-----------------------------------------------------------------------------
// Purpose: Player specific data for Infested ( sent only to local player, too )
//-----------------------------------------------------------------------------
class CASWPlayerLocalData
{
public:
	// Save/restore
	DECLARE_SIMPLE_DATADESC();
	DECLARE_CLASS_NOBASE( CASWPlayerLocalData );
	DECLARE_EMBEDDED_NETWORKVAR();

	CASWPlayerLocalData();

	CNetworkVar( EHANDLE, m_hAutoAimTarget );
	CNetworkVar( Vector, m_vecAutoAimPoint );
	CNetworkVar( bool,	m_bAutoAimTarget );
};

EXTERN_SEND_TABLE(DT_ASWLocal);


#endif // ASW_PLAYERLOCALDATA_H
