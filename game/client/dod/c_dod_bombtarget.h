//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef C_DOD_BOMBTARGET_H
#define C_DOD_BOMBTARGET_H
#ifdef _WIN32
#pragma once
#endif

#include "c_baseanimating.h"

class C_DODBombTarget : public C_BaseAnimating
{
	DECLARE_CLASS( C_DODBombTarget, C_BaseAnimating );

	DECLARE_NETWORKCLASS();

public:
	virtual int DrawModel( int flags );
	virtual void NotifyShouldTransmit( ShouldTransmitState_t state );
	virtual void ClientThink( void );

	// play the hint telling them how to defuse
	bool ShouldPlayDefuseHint( int team );
	bool ShouldPlayPlantHint( int team );

private:
	CNetworkVar( int, m_iState );
	CNetworkVar( int, m_iBombingTeam );

	CNetworkVar( int, m_iTargetModel );
	CNetworkVar( int, m_iUnavailableModel );
};

#endif //C_DOD_BOMBTARGET_H