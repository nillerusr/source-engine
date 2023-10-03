//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#ifndef CLIENTALPHAPROPERTY_H
#define CLIENTALPHAPROPERTY_H
#ifdef _WIN32
#pragma once
#endif

#include "iclientalphaproperty.h"


#define CLIENT_ALPHA_DISTANCE_FADE_MODE_BIT_COUNT 1

//-----------------------------------------------------------------------------
// Implementation class
//-----------------------------------------------------------------------------
class CClientAlphaProperty : public IClientAlphaProperty
{
	// Inherited from IClientAlphaProperty
public:
	virtual IClientUnknown*	GetIClientUnknown();
	virtual void SetAlphaModulation( uint8 a );
	virtual void SetRenderFX( RenderFx_t nRenderFx, RenderMode_t nRenderMode, float flStartTime = FLT_MAX, float flDuration = 0.0f );
	virtual void SetFade( float flGlobalFadeScale, float flDistFadeMinDist, float flDistFadeMaxDist );	
	virtual void SetDesyncOffset( int nOffset );
	virtual void EnableAlphaModulationOverride( bool bEnable );
	virtual void EnableShadowAlphaModulationOverride( bool bEnable );
	virtual void SetDistanceFadeMode( ClientAlphaDistanceFadeMode_t nFadeMode );

	// Other public methods
public:
	CClientAlphaProperty( );
	void Init( IClientUnknown *pUnk );

	// NOTE: Only the client shadow manager should ever call this method!
	void SetShadowHandle( ClientShadowHandle_t hShadowHandle );

	// Returns the current alpha modulation (no fades or render FX taken into account)
	uint8 GetAlphaModulation() const;

	// Compute the render alpha (after fades + render FX are applied)
	uint8 ComputeRenderAlpha( ) const;

	// Returns alpha fade
	float GetMinFadeDist() const;
	float GetMaxFadeDist() const;
	float GetGlobalFadeScale() const;

	// Should this ignore the Z buffer?
	bool IgnoresZBuffer( void ) const;

private:
	// NOTE: Be careful if you add data to this class.
	// It needs to be no more than 32 bytes, which it is right now
	// (remember the vtable adds 4 bytes). Try to restrict usage
	// to reserved areas or figure out a way of compressing existing fields
	IClientUnknown *m_pOuter;

	ClientShadowHandle_t m_hShadowHandle;
	uint16 m_nRenderFX : 5;
	uint16 m_nRenderMode : 4;
	uint16 m_bAlphaOverride : 1;
	uint16 m_bShadowAlphaOverride : 1;
	uint16 m_nDistanceFadeMode : CLIENT_ALPHA_DISTANCE_FADE_MODE_BIT_COUNT;
	uint16 m_nReserved : 4;

	uint16 m_nDesyncOffset;
	uint8 m_nAlpha;
	uint8 m_nReserved2;

	uint16 m_nDistFadeStart;
	uint16 m_nDistFadeEnd;

	float m_flFadeScale;
	float m_flRenderFxStartTime;
	float m_flRenderFxDuration;

	friend class CClientLeafSystem;
};

// Returns the current alpha modulation
inline uint8 CClientAlphaProperty::GetAlphaModulation() const
{
	return m_nAlpha;
}

inline float CClientAlphaProperty::GetMinFadeDist() const
{
	return m_nDistFadeStart;
}

inline float CClientAlphaProperty::GetMaxFadeDist() const
{
	return m_nDistFadeEnd;
}

inline float CClientAlphaProperty::GetGlobalFadeScale() const
{
	return m_flFadeScale;
}

inline bool CClientAlphaProperty::IgnoresZBuffer( void ) const
{
	return m_nRenderMode == kRenderGlow || m_nRenderMode == kRenderWorldGlow;
}


#endif // CLIENTALPHAPROPERTY_H
