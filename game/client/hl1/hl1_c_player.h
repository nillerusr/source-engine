//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef C_HL1_PLAYER_H
#define C_HL1_PLAYER_H
#ifdef _WIN32
#pragma once
#endif

#include "c_baseplayer.h"


#define CHL1_Player C_HL1_Player

class C_HL1_Player : public C_BasePlayer
{
public:
	DECLARE_CLASS( C_HL1_Player, C_BasePlayer );
	DECLARE_CLIENTCLASS();

	C_HL1_Player();

	bool IsPullingObject() { return m_bIsPullingObject; }

	CNetworkVar( bool, m_bHasLongJump );
	CNetworkVar( int, m_nFlashBattery );
	CNetworkVar( bool, m_bIsPullingObject );

	CNetworkVar( float, m_flStartCharge );
	CNetworkVar( float, m_flAmmoStartCharge );
	CNetworkVar( float, m_flPlayAftershock );
	CNetworkVar( float, m_flNextAmmoBurn );	// while charging, when to absorb another unit of player's ammo?

private:
	C_HL1_Player( const C_HL1_Player & );
	
};

inline C_HL1_Player *ToHL1Player( CBaseEntity *pEntity )
{
	if ( !pEntity || !pEntity->IsPlayer() )
		return NULL;

	return dynamic_cast<C_HL1_Player*>( pEntity );
}

#endif //C_HL1_PLAYER_H