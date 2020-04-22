//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef ENV_LIGHTRAIL_ENDPOINT_SHARED_H
#define ENV_LIGHTRAIL_ENDPOINT_SHARED_H
#ifdef _WIN32
#pragma once
#endif

#define	SF_ENDPOINT_START_SMALLFX		(1<<0)		//Define spawnflags
//#define	SF_ENDPOINT_START_LARGEFX		(1<<1)

enum		//Enumeration of the 4 states the endpoints can be in.
{
	ENDPOINT_STATE_OFF,			//No FX displayed
	ENDPOINT_STATE_SMALLFX,		//Just the small particle trail is displayed and a faint glow		
	ENDPOINT_STATE_CHARGING,	//Ramp up over a certain amount of time to the large bright glow
	ENDPOINT_STATE_LARGEFX,		//Shows a particle trail and a large bright glow
	ENDPOINT_STATE_COUNT,
};

#ifndef CLIENT_DLL

// ============================================================================
//
//  Energy core - charges up and then releases energy from its position
//
// ============================================================================

class CEnv_Lightrail_Endpoint : public CBaseEntity
{
	DECLARE_CLASS( CEnv_Lightrail_Endpoint, CBaseEntity );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

public:
	void	InputStartCharge( inputdata_t &inputdata );
	void	InputStartSmallFX(inputdata_t &inputdata );
	void	InputStartLargeFX( inputdata_t &inputdata );
	void	InputStop( inputdata_t &inputdata );
	void	SetSmallFXScale( float flSmallScale ) { m_flSmallScale = flSmallScale; }
	void	SetLargeFXScale( float flLargeScale ) { m_flLargeScale = flLargeScale; }

	void	StartCharge( float flWarmUpTime );		//Charging difference between the small and large fx
	void	StartSmallFX();							//Start discharging the scaled down version of the FX
	void	StartLargeFX();							//Start discharging the larger brighter version of the FX
	void	StopSmallFX( float flCoolDownTime );	//Stop discharging the small fx
	void	StopLargeFX( float flCoolDownTime );	//Stop discharging the small fx

	virtual int	ShouldTransmit( const CCheckTransmitInfo *pInfo );
	virtual int UpdateTransmitState( void );

	virtual void Precache();
	void	Spawn( void );

private:
	CNetworkVar( float, m_flSmallScale );		//Scale of the small fx
	CNetworkVar( float, m_flLargeScale );		//Scale of the large fx
	CNetworkVar( int, m_nState );			//Current state of the fx
	CNetworkVar( float, m_flDuration );		
	CNetworkVar( float, m_flStartTime );
};

#endif

#endif // ENV_LIGHTRAIL_ENDPOINT_SHARED_H
