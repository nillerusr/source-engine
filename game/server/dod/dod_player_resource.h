//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: DOD's custom CPlayerResource
//
// $NoKeywords: $
//=============================================================================//

#ifndef DOD_PLAYER_RESOURCE_H
#define DOD_PLAYER_RESOURCE_H
#ifdef _WIN32
#pragma once
#endif

class CDODPlayerResource : public CPlayerResource
{
	DECLARE_CLASS( CDODPlayerResource, CPlayerResource );
	
public:
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CDODPlayerResource();

	virtual void UpdatePlayerData( void );
	virtual void Spawn( void );

protected:
	CNetworkArray( int, m_iObjScore, MAX_PLAYERS+1 );
	CNetworkArray( int, m_iPlayerClass, MAX_PLAYERS+1 );
};

#endif // DOD_PLAYER_RESOURCE_H
