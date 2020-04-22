//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_FLARE_H
#define TF_FLARE_H
#pragma once

#define	SIGNALFLARE_NO_DLIGHT			0x01
#define	SIGNALFLARE_NO_SMOKE			0x02
#define	SIGNALFLARE_INFINITE			0x04
#define SIGNALFLARE_DURATION			5.0f

//=============================================================================
//
// Signal Flare Class
//
class CSignalFlare : public CBaseAnimating
{

	DECLARE_CLASS( CSignalFlare, CBaseAnimating );

public:

	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	CSignalFlare( void );

	static CSignalFlare *Create( Vector vecOrigin, QAngle vecAngles, CBaseEntity *pOwner, float lifetime );

	// Initialization
	void Spawn( void );
	void Precache( void );

	// Think and Touch
	void FlareThink( void );
	void FlareTouch( CBaseEntity *pOther );

public:

	CBaseEntity		*m_pOwner;
	int				m_nBounces;			// how many times has this flare bounced?
	CNetworkVar( float, m_flDuration );		// when will the flare burn out?
	CNetworkVar( float, m_flScale );
	float			m_flNextDamage;
	
	bool			m_bFading;
	CNetworkVar( bool, m_bLight );
	CNetworkVar( bool, m_bSmoke );
};

EXTERN_SEND_TABLE( DT_SignalFlare );

#endif // TF_FLARE_H