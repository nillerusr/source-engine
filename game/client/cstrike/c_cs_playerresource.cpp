//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: CS's custom C_PlayerResource
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "c_cs_playerresource.h"
#include <shareddefs.h>
#include <cs_shareddefs.h>
#include "hud.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_CLIENTCLASS_DT(C_CS_PlayerResource, DT_CSPlayerResource, CCSPlayerResource)
	RecvPropInt( RECVINFO( m_iPlayerC4 ) ),
	RecvPropInt( RECVINFO( m_iPlayerVIP ) ),
	RecvPropVector( RECVINFO(m_vecC4) ),
	RecvPropArray3( RECVINFO_ARRAY(m_bHostageAlive), RecvPropInt( RECVINFO(m_bHostageAlive[0]))),
	RecvPropArray3( RECVINFO_ARRAY(m_isHostageFollowingSomeone), RecvPropInt( RECVINFO(m_isHostageFollowingSomeone[0]))),
	RecvPropArray3( RECVINFO_ARRAY(m_iHostageEntityIDs), RecvPropInt( RECVINFO(m_iHostageEntityIDs[0]))),
	RecvPropArray3( RECVINFO_ARRAY(m_iHostageX), RecvPropInt( RECVINFO(m_iHostageX[0]))),
	RecvPropArray3( RECVINFO_ARRAY(m_iHostageY), RecvPropInt( RECVINFO(m_iHostageY[0]))),
	RecvPropArray3( RECVINFO_ARRAY(m_iHostageZ), RecvPropInt( RECVINFO(m_iHostageZ[0]))),
	RecvPropVector( RECVINFO(m_bombsiteCenterA) ),
	RecvPropVector( RECVINFO(m_bombsiteCenterB) ),
	RecvPropArray3( RECVINFO_ARRAY(m_hostageRescueX), RecvPropInt( RECVINFO(m_hostageRescueX[0]))),
	RecvPropArray3( RECVINFO_ARRAY(m_hostageRescueY), RecvPropInt( RECVINFO(m_hostageRescueY[0]))),
	RecvPropArray3( RECVINFO_ARRAY(m_hostageRescueZ), RecvPropInt( RECVINFO(m_hostageRescueZ[0]))),
	RecvPropInt( RECVINFO( m_bBombSpotted ) ),
	RecvPropArray3( RECVINFO_ARRAY(m_bPlayerSpotted), RecvPropInt( RECVINFO(m_bPlayerSpotted[0]))),
	RecvPropArray3( RECVINFO_ARRAY(m_iMVPs), RecvPropInt( RECVINFO(m_iMVPs[0]))),
	RecvPropArray3( RECVINFO_ARRAY(m_bHasDefuser), RecvPropInt( RECVINFO(m_bHasDefuser[0]))),
	RecvPropArray3( RECVINFO_ARRAY(m_szClan), RecvPropString( RECVINFO(m_szClan[0]))),
END_RECV_TABLE()
 
//=============================================================================
// HPE_END
//=============================================================================

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_CS_PlayerResource::C_CS_PlayerResource()
{
	m_Colors[TEAM_TERRORIST] = COLOR_RED;
	m_Colors[TEAM_CT] = COLOR_BLUE;
	memset( m_iMVPs, 0, sizeof( m_iMVPs ) );
	memset( m_bHasDefuser, 0, sizeof( m_bHasDefuser ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_CS_PlayerResource::~C_CS_PlayerResource()
{
}

bool C_CS_PlayerResource::IsVIP(int iIndex )
{
	return m_iPlayerVIP == iIndex;
}

bool C_CS_PlayerResource::HasC4(int iIndex )
{
	return m_iPlayerC4 == iIndex;
}

bool C_CS_PlayerResource::IsHostageAlive(int iIndex)
{
	if ( iIndex < 0 || iIndex >= MAX_HOSTAGES )
		return false;

	return m_bHostageAlive[iIndex];
}

bool C_CS_PlayerResource::IsHostageFollowingSomeone(int iIndex)
{
	if ( iIndex < 0 || iIndex >= MAX_HOSTAGES )
		return false;

	return m_isHostageFollowingSomeone[iIndex];
}

int C_CS_PlayerResource::GetHostageEntityID(int iIndex)
{
	if ( iIndex < 0 || iIndex >= MAX_HOSTAGES )
		return -1;

	return m_iHostageEntityIDs[iIndex];
}

const Vector C_CS_PlayerResource::GetHostagePosition( int iIndex )
{
	if ( iIndex < 0 || iIndex >= MAX_HOSTAGES )
		return vec3_origin;

	Vector ret;

	ret.x = m_iHostageX[iIndex];
	ret.y = m_iHostageY[iIndex];
	ret.z = m_iHostageZ[iIndex];

	return ret;
}

const Vector C_CS_PlayerResource::GetC4Postion()
{
	if ( m_iPlayerC4 > 0 )
	{
		// C4 is carried by player
		C_BasePlayer *pPlayer = UTIL_PlayerByIndex( m_iPlayerC4 );

		if ( pPlayer )
		{
			return pPlayer->GetAbsOrigin();
		}
	}

	// C4 is lying on ground
	return m_vecC4;
}

const Vector C_CS_PlayerResource::GetBombsiteAPosition()
{
	return m_bombsiteCenterA;
}

const Vector C_CS_PlayerResource::GetBombsiteBPosition()
{
	return m_bombsiteCenterB;
}

const Vector C_CS_PlayerResource::GetHostageRescuePosition( int iIndex )
{
	if ( iIndex < 0 || iIndex >= MAX_HOSTAGE_RESCUES )
		return vec3_origin;

	Vector ret;

	ret.x = m_hostageRescueX[iIndex];
	ret.y = m_hostageRescueY[iIndex];
	ret.z = m_hostageRescueZ[iIndex];

	return ret;
}

int C_CS_PlayerResource::GetPlayerClass( int iIndex )
{
	if ( !IsConnected( iIndex ) )
	{
		return CS_CLASS_NONE;
	}

	return m_iPlayerClasses[ iIndex ];
}

//--------------------------------------------------------------------------------------------------------
bool C_CS_PlayerResource::IsBombSpotted( void ) const
{
	return m_bBombSpotted;
}


//--------------------------------------------------------------------------------------------------------
bool C_CS_PlayerResource::IsPlayerSpotted( int iIndex )
{
	if ( !IsConnected( iIndex ) )
		return false;

	return m_bPlayerSpotted[iIndex];
}

//-----------------------------------------------------------------------------
const char *C_CS_PlayerResource::GetClanTag( int iIndex )
{
	if ( iIndex < 1 || iIndex > MAX_PLAYERS )
	{
		Assert( false );
		return "";
	}

	if ( !IsConnected( iIndex ) )
		return "";

	return m_szClan[iIndex];
}

//-----------------------------------------------------------------------------
int C_CS_PlayerResource::GetNumMVPs( int iIndex )
{
	if ( !IsConnected( iIndex ) )
		return false;

	return m_iMVPs[iIndex];
} 

//-----------------------------------------------------------------------------
bool C_CS_PlayerResource::HasDefuser( int iIndex )
{
	if ( !IsConnected( iIndex ) )
		return false;

	return m_bHasDefuser[iIndex];
} 
