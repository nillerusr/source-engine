//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: CS's custom CPlayerResource
//
// $NoKeywords: $
//=============================================================================//

#ifndef CS_PLAYER_RESOURCE_H
#define CS_PLAYER_RESOURCE_H
#ifdef _WIN32
#pragma once
#endif

class CCSPlayerResource : public CPlayerResource
{
	DECLARE_CLASS( CCSPlayerResource, CPlayerResource );
	
public:
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CCSPlayerResource();

	virtual void UpdatePlayerData( void );
	virtual void Spawn( void );
protected:

	CNetworkVar( int, m_iPlayerC4 );  // entity index of C4 carrier or 0
	CNetworkVar( int, m_iPlayerVIP ); // entity index of VIP player or 0
	CNetworkVector( m_vecC4 );  // position of C4
	CNetworkArray( bool, m_bHostageAlive, MAX_HOSTAGES );
	CNetworkArray( bool, m_isHostageFollowingSomeone, MAX_HOSTAGES );
	CNetworkArray( int, m_iHostageEntityIDs, MAX_HOSTAGES );
	CNetworkArray( int, m_iHostageX, MAX_HOSTAGES );
	CNetworkArray( int, m_iHostageY, MAX_HOSTAGES );
	CNetworkArray( int, m_iHostageZ, MAX_HOSTAGES );
	CNetworkVector( m_bombsiteCenterA );// Location of bombsite A
	CNetworkVector( m_bombsiteCenterB );// Location of bombsite B
	CNetworkArray( int, m_hostageRescueX, MAX_HOSTAGE_RESCUES );// Locations of all hostage rescue spots
	CNetworkArray( int, m_hostageRescueY, MAX_HOSTAGE_RESCUES );
	CNetworkArray( int, m_hostageRescueZ, MAX_HOSTAGE_RESCUES );

	CNetworkVar( bool, m_bBombSpotted );
	CNetworkArray( bool, m_bPlayerSpotted, MAX_PLAYERS+1 );

	CNetworkArray( string_t, m_szClan, MAX_PLAYERS+1 );

	CNetworkArray( int, m_iMVPs, MAX_PLAYERS + 1 );
	CNetworkArray( bool, m_bHasDefuser, MAX_PLAYERS + 1);

private:
	bool m_foundGoalPositions;
};

#endif // CS_PLAYER_RESOURCE_H
