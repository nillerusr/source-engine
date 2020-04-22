//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: CS's custom C_PlayerResource
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "c_dod_playerresource.h"
#include <shareddefs.h>
#include <dod_shareddefs.h>
#include "hud.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


IMPLEMENT_CLIENTCLASS_DT(C_DOD_PlayerResource, DT_DODPlayerResource, CDODPlayerResource)
	RecvPropArray3( RECVINFO_ARRAY(m_iObjScore), RecvPropInt( RECVINFO(m_iObjScore[0]))),
	RecvPropArray3( RECVINFO_ARRAY(m_iPlayerClass), RecvPropInt( RECVINFO(m_iPlayerClass[0]))),
END_RECV_TABLE()


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_DOD_PlayerResource::C_DOD_PlayerResource()
{
	m_Colors[TEAM_ALLIES] = COLOR_DOD_GREEN;
	m_Colors[TEAM_AXIS] = COLOR_DOD_RED;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_DOD_PlayerResource::~C_DOD_PlayerResource()
{
}

int C_DOD_PlayerResource::GetScore( int iIndex )
{
	if ( !IsConnected( iIndex ) )
		return 0;

	return m_iObjScore[iIndex];
}

int C_DOD_PlayerResource::GetPlayerClass( int iIndex )
{
	if ( !IsConnected( iIndex ) )
		return PLAYERCLASS_UNDEFINED;

	return m_iPlayerClass[iIndex];
}
