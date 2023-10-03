//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#ifndef ANIMATIONLAYER_H
#define ANIMATIONLAYER_H
#ifdef _WIN32
#pragma once
#endif


#include "rangecheckedvar.h"
#include "tier1/lerp_functions.h"

#ifdef CLIENT_DLL
class C_BaseAnimatingOverlay;
#endif

class C_AnimationLayer
{
public:

	// This allows the datatables to access private members.
	ALLOW_DATATABLES_PRIVATE_ACCESS();

	C_AnimationLayer();
	void Reset();

#ifdef CLIENT_DLL
	void SetOwner( C_BaseAnimatingOverlay *pOverlay );
	C_BaseAnimatingOverlay *GetOwner() const;
#endif

	void SetOrder( int order );
	bool IsActive( void );
	float GetFadeout( float flCurTime );

	void SetSequence( int nSequence );
	void SetCycle( float flCycle );
	void SetPrevCycle( float flCycle );
	void SetPlaybackRate( float flPlaybackRate );
	void SetWeight( float flWeight );

	int   GetOrder() const;
	int   GetSequence( ) const;
	float GetCycle( ) const;
	float GetPrevCycle( ) const;
	float GetPlaybackRate( ) const;
	float GetWeight( ) const;

#ifdef CLIENT_DLL
	// If the weights, cycle or sequence #s changed due to interpolation then 
	//  we'll need to recompute the bbox
	int GetInvalidatePhysicsBits() const;
	void SetInvalidatePhysicsBits( int iBit ) { m_nInvalidatePhysicsBits = iBit; }
#endif

public:
	float	m_flLayerAnimtime;
	float	m_flLayerFadeOuttime;

private:
	int		m_nOrder;
	CRangeCheckedVar<int, -1, 65535, 0>		m_nSequence;
	CRangeCheckedVar<float, -2, 2, 0>		m_flPrevCycle;
	CRangeCheckedVar<float, -5, 5, 0>		m_flWeight;

	// used for automatic crossfades between sequence changes
	CRangeCheckedVar<float, -50, 50, 1>		m_flPlaybackRate;
	CRangeCheckedVar<float, -2, 2, 0>		m_flCycle;

#ifdef CLIENT_DLL
	C_BaseAnimatingOverlay	*m_pOwner;
	int					m_nInvalidatePhysicsBits;
#endif

	friend class C_BaseAnimatingOverlay;
	friend C_AnimationLayer LoopingLerp( float flPercent, C_AnimationLayer& from, C_AnimationLayer& to );
	friend C_AnimationLayer Lerp( float flPercent, const C_AnimationLayer& from, const C_AnimationLayer& to );
	friend C_AnimationLayer LoopingLerp_Hermite( const C_AnimationLayer& current, float flPercent, C_AnimationLayer& prev, C_AnimationLayer& from, C_AnimationLayer& to );
	friend C_AnimationLayer Lerp_Hermite( const C_AnimationLayer& current, float flPercent, const C_AnimationLayer& prev, const C_AnimationLayer& from, const C_AnimationLayer& to );
	friend void Lerp_Clamp( C_AnimationLayer &val );
	friend int CheckForSequenceBoxChanges( const C_AnimationLayer& newLayer, const C_AnimationLayer& oldLayer );
};

#ifdef CLIENT_DLL
	#define CAnimationLayer C_AnimationLayer
#endif


inline C_AnimationLayer::C_AnimationLayer()
{
#ifdef CLIENT_DLL
	m_pOwner = NULL;
	m_nInvalidatePhysicsBits = 0;
#endif

	Reset();
}

#ifdef GAME_DLL

inline void C_AnimationLayer::SetSequence( int nSequence )
{
	m_nSequence = nSequence;
}

inline void C_AnimationLayer::SetCycle( float flCycle )
{
	m_flCycle = flCycle;
}

inline void C_AnimationLayer::SetWeight( float flWeight )
{
	m_flWeight = flWeight;
}

#endif // GAME_DLL

FORCEINLINE void C_AnimationLayer::SetPrevCycle( float flPrevCycle )
{
	m_flPrevCycle = flPrevCycle;
}

FORCEINLINE void C_AnimationLayer::SetPlaybackRate( float flPlaybackRate )
{
	m_flPlaybackRate = flPlaybackRate;
}

FORCEINLINE int	C_AnimationLayer::GetSequence( ) const
{
	return m_nSequence;
}

FORCEINLINE float C_AnimationLayer::GetCycle( ) const
{
	return m_flCycle;
}

FORCEINLINE float C_AnimationLayer::GetPrevCycle( ) const
{
	return m_flPrevCycle;
}

FORCEINLINE float C_AnimationLayer::GetPlaybackRate( ) const
{
	return m_flPlaybackRate;
}

FORCEINLINE float C_AnimationLayer::GetWeight( ) const
{
	return m_flWeight;
}

FORCEINLINE int C_AnimationLayer::GetOrder() const
{
	return m_nOrder;
}

inline float C_AnimationLayer::GetFadeout( float flCurTime )
{
	float s;

    if (m_flLayerFadeOuttime <= 0.0f)
	{
		s = 0;
	}
	else
	{
		// blend in over 0.2 seconds
		s = 1.0 - (flCurTime - m_flLayerAnimtime) / m_flLayerFadeOuttime;
		if (s > 0 && s <= 1.0)
		{
			// do a nice spline curve
			s = 3 * s * s - 2 * s * s * s;
		}
		else if ( s > 1.0f )
		{
			// Shouldn't happen, but maybe curtime is behind animtime?
			s = 1.0f;
		}
	}
	return s;
}

#ifdef CLIENT_DLL
FORCEINLINE int C_AnimationLayer::GetInvalidatePhysicsBits() const
{
	return m_nInvalidatePhysicsBits;
}
#endif

inline C_AnimationLayer LoopingLerp( float flPercent, C_AnimationLayer& from, C_AnimationLayer& to )
{
#ifdef CLIENT_DLL
	Assert( from.GetOwner() == to.GetOwner() );
#endif

	C_AnimationLayer output;

	output.m_nSequence = to.m_nSequence;
	output.m_flCycle = LoopingLerp( flPercent, (float)from.m_flCycle, (float)to.m_flCycle );
	output.m_flPrevCycle = to.m_flPrevCycle;
	output.m_flWeight = Lerp( flPercent, from.m_flWeight, to.m_flWeight );
	output.m_nOrder = to.m_nOrder;

	output.m_flLayerAnimtime = to.m_flLayerAnimtime;
	output.m_flLayerFadeOuttime = to.m_flLayerFadeOuttime;
#ifdef CLIENT_DLL
	output.SetOwner( to.GetOwner() );
#endif
	return output;
}

inline C_AnimationLayer Lerp( float flPercent, const C_AnimationLayer& from, const C_AnimationLayer& to )
{
#ifdef CLIENT_DLL
	Assert( from.GetOwner() == to.GetOwner() );
#endif

	C_AnimationLayer output;

	output.m_nSequence = to.m_nSequence;
	output.m_flCycle = Lerp( flPercent, from.m_flCycle, to.m_flCycle );
	output.m_flPrevCycle = to.m_flPrevCycle;
	output.m_flWeight = Lerp( flPercent, from.m_flWeight, to.m_flWeight );
	output.m_nOrder = to.m_nOrder;

	output.m_flLayerAnimtime = to.m_flLayerAnimtime;
	output.m_flLayerFadeOuttime = to.m_flLayerFadeOuttime;
#ifdef CLIENT_DLL
	output.SetOwner( to.GetOwner() );
#endif
	return output;
}

inline int CheckForSequenceBoxChanges( const C_AnimationLayer& newLayer, const C_AnimationLayer& oldLayer )
{
	int nChangeFlags = 0;

	bool bOldIsZero = ( oldLayer.GetWeight() == 0.0f );
	bool bNewIsZero = ( newLayer.GetWeight() == 0.0f );

	if ( ( newLayer.GetSequence() != oldLayer.GetSequence() ) ||
		 ( bNewIsZero != bOldIsZero ) ) 
	{
		nChangeFlags |= SEQUENCE_CHANGED | BOUNDS_CHANGED;
	}

	if ( newLayer.GetCycle() != oldLayer.GetCycle() )
	{
		nChangeFlags |= ANIMATION_CHANGED;
	}

	if ( newLayer.GetOrder() != oldLayer.GetOrder() )
	{
		nChangeFlags |= BOUNDS_CHANGED;
	}

	return nChangeFlags;
}

inline C_AnimationLayer LoopingLerp_Hermite( const C_AnimationLayer& current, float flPercent, C_AnimationLayer& prev, C_AnimationLayer& from, C_AnimationLayer& to )
{
#ifdef CLIENT_DLL
	Assert( prev.GetOwner() == from.GetOwner() );
	Assert( from.GetOwner() == to.GetOwner() );
#endif

	C_AnimationLayer output;

	output.m_nSequence = to.m_nSequence;
	output.m_flCycle = LoopingLerp_Hermite( (float)current.m_flCycle, flPercent, (float)prev.m_flCycle, (float)from.m_flCycle, (float)to.m_flCycle );
	output.m_flPrevCycle = to.m_flPrevCycle;
	output.m_flWeight = Lerp( flPercent, from.m_flWeight, to.m_flWeight );
	output.m_nOrder = to.m_nOrder;

	output.m_flLayerAnimtime = to.m_flLayerAnimtime;
	output.m_flLayerFadeOuttime = to.m_flLayerFadeOuttime;

#ifdef CLIENT_DLL
	output.SetOwner( to.GetOwner() );
	output.m_nInvalidatePhysicsBits = CheckForSequenceBoxChanges( output, current );
#endif
	return output;
}

// YWB:  Specialization for interpolating euler angles via quaternions...
inline C_AnimationLayer Lerp_Hermite( const C_AnimationLayer& current, float flPercent, const C_AnimationLayer& prev, const C_AnimationLayer& from, const C_AnimationLayer& to )
{
#ifdef CLIENT_DLL
	Assert( prev.GetOwner() == from.GetOwner() );
	Assert( from.GetOwner() == to.GetOwner() );
#endif

	C_AnimationLayer output;

	output.m_nSequence = to.m_nSequence;
	output.m_flCycle = Lerp_Hermite( (float)current.m_flCycle, flPercent, (float)prev.m_flCycle, (float)from.m_flCycle, (float)to.m_flCycle );
	output.m_flPrevCycle = to.m_flPrevCycle;
	output.m_flWeight = Lerp( flPercent, from.m_flWeight, to.m_flWeight );
	output.m_nOrder = to.m_nOrder;

	output.m_flLayerAnimtime = to.m_flLayerAnimtime;
	output.m_flLayerFadeOuttime = to.m_flLayerFadeOuttime;
#ifdef CLIENT_DLL
	output.SetOwner( to.GetOwner() );
 	output.m_nInvalidatePhysicsBits = CheckForSequenceBoxChanges( output, current );
#endif
	return output;
}

inline void Lerp_Clamp( C_AnimationLayer &val )
{
	Lerp_Clamp( val.m_nSequence );
	Lerp_Clamp( val.m_flCycle );
	Lerp_Clamp( val.m_flPrevCycle );
	Lerp_Clamp( val.m_flWeight );
	Lerp_Clamp( val.m_nOrder );
	Lerp_Clamp( val.m_flLayerAnimtime );
	Lerp_Clamp( val.m_flLayerFadeOuttime );
}

#endif // ANIMATIONLAYER_H
