//============ Copyright (c) Valve Corporation, All rights reserved. ============
#ifndef GAME_TIMESCALE_SHARED_H
#define GAME_TIMESCALE_SHARED_H
#ifdef _WIN32
#pragma once
#endif

#include "igamesystem.h"


//=============================================================================
//
// Smoothly blends the timescale through an engine interface
//
class CGameTimescale : public CAutoGameSystemPerFrame
{
public:

	enum Interpolators_e
	{
		INTERPOLATOR_LINEAR,
		INTERPOLATOR_ACCEL,
		INTERPOLATOR_DEACCEL,
		INTERPOLATOR_EASE_IN_OUT,
	};

	// Creation/Destruction.
	CGameTimescale();
	~CGameTimescale();


	// Initialization/Shutdown.
	virtual bool Init();	
	virtual void Shutdown();

#ifdef CLIENT_DLL
	virtual void Update( float frametime );
#else
	virtual void FrameUpdatePostEntityThink();
#endif

	// Level init, shutdown
	virtual void LevelInitPostEntity();
	virtual void LevelShutdownPostEntity();


	float GetCurrentTimescale( void ) const { return m_flCurrentTimescale; }
	float GetDesiredTimescale( void ) const { return m_flDesiredTimescale; }

	// Set the timescale to an exact value without doing a ramp in/out blend
	void SetCurrentTimescale( float flTimescale );

	// Sets the desired timescale and will automatically ramp in/out
	void SetDesiredTimescaleAtTime( float flDesiredTimescale, float flDurationRealTimeSeconds = 0.0f, Interpolators_e nInterpolatorType = INTERPOLATOR_LINEAR, float flStartBlendTime = 0.0f );
	void SetDesiredTimescale( float flDesiredTimescale, float flDurationRealTimeSeconds = 0.0f, Interpolators_e nInterpolatorType = INTERPOLATOR_LINEAR, float flDelayRealtime = 0.0f );

private:

	void UpdateTimescale( void );
	void ResetTimescale( void );

private:

	float m_flDesiredTimescale;
	float m_flCurrentTimescale;

	float m_flDurationRealTimeSeconds;
	Interpolators_e m_nInterpolatorType;

	float m_flStartTimescale;
	float m_flStartBlendTime;
	float m_flStartBlendRealtime;
};

CGameTimescale *GameTimescale();

#endif // GAME_TIMESCALE_SHARED_H