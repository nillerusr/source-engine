//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: Spatial entity with simple radial falloff
//
// $NoKeywords: $
//===========================================================================//

#ifndef C_SPATIALENTITY_H
#define C_SPATIALENTITY_H

#ifdef _WIN32
#pragma once
#endif

#define SPATIAL_ENTITY_FORCED_VALUE_BLEND_DISTANCE 128.0f

//------------------------------------------------------------------------------
// Purpose : Spatial entity with radial falloff
//------------------------------------------------------------------------------
class C_SpatialEntity : public C_BaseEntity
{
public:
	DECLARE_CLASS( C_SpatialEntity, C_BaseEntity );
	DECLARE_CLIENTCLASS();

	virtual void OnDataChanged(DataUpdateType_t updateType);
	virtual void UpdateOnRemove( void );
	virtual bool ShouldDraw();

	virtual void ClientThink();
	virtual void InitSpatialEntity();

	virtual void ResetAccumulation( void ) {};
	virtual void Accumulate( void ) {};
	virtual void ApplyAccumulation( void ) {};

	float GetMinFalloff( void ) { return m_minFalloff; }
	float GetMaxFalloff( void ) { return m_maxFalloff; }

	void SetFalloff( float flMinFalloff, float flMaxFalloff ) { m_minFalloff = flMinFalloff; m_maxFalloff = flMaxFalloff; }
	void SetEnabled( bool bEnabled ) { m_bEnabled = bEnabled; }
	void SetCurWeight( float flWeight ) { m_flCurWeight = flWeight; }

protected:
	virtual void AddToPersonalSpatialEntityMgr( void ) {};
	virtual void RemoveFromPersonalSpatialEntityMgr( void ) {};

private:
	Vector	m_vecOrigin;

	float	m_minFalloff;
	float	m_maxFalloff;
	float	m_flCurWeight;
	char	m_netLookupFilename[MAX_PATH];

	bool	m_bEnabled;

protected:
	float	m_flWeight;		// Interpolator for how much it's value adds to the total
	float	m_flInfluence;	// Distance within the inner radius
};


template <class T>
class C_SpatialEntityTemplate : public C_SpatialEntity
{
	DECLARE_CLASS( C_SpatialEntityTemplate, C_SpatialEntity );
public:
	virtual void ResetAccumulation( void )
	{
		m_AccumulatedValue = 0;
		m_ForcedValue = 0;
		m_ForcedMinFalloff = GetMinFalloff();
		m_ForcedInfluence = 0.0f;
	}

	virtual void Accumulate( void )
	{
		if ( m_flInfluence > m_ForcedInfluence )
		{
			// This value is in control unless a stronger influence is accumulated
			m_ForcedValue = m_Value;
			m_ForcedMinFalloff = GetMinFalloff();
			m_ForcedInfluence = m_flInfluence;
		}

		// Contribute to the total
		m_AccumulatedValue += m_Value * m_flWeight;
	}

	T BlendedValue( void )
	{
		if ( m_ForcedInfluence <= 0.0f )
		{
			// No forced value
			return m_AccumulatedValue;
		}

		// Check if we're fully inside the forced area
		if ( m_ForcedInfluence >= SPATIAL_ENTITY_FORCED_VALUE_BLEND_DISTANCE )
		{
			// Fully inside the forced area
			return m_ForcedValue;
		}

		// Blend between the accumulated value and the forced value
		float flInterp = ( SPATIAL_ENTITY_FORCED_VALUE_BLEND_DISTANCE - m_ForcedInfluence ) / SPATIAL_ENTITY_FORCED_VALUE_BLEND_DISTANCE;

		return ( m_AccumulatedValue * flInterp ) + ( m_ForcedValue * ( 1.0f - flInterp ) );
	}

protected:

	T				m_Value;

private:
	static T		m_AccumulatedValue;
	static T		m_ForcedValue;
	static float	m_ForcedMinFalloff;
	static float	m_ForcedInfluence;
};

template <class T>
T C_SpatialEntityTemplate<T>::m_AccumulatedValue;

template <class T>
T C_SpatialEntityTemplate<T>::m_ForcedValue;

template <class T>
float C_SpatialEntityTemplate<T>::m_ForcedMinFalloff;

template <class T>
float C_SpatialEntityTemplate<T>::m_ForcedInfluence;

template <>
void C_SpatialEntityTemplate<Vector>::ResetAccumulation( void )
{
	m_AccumulatedValue.Init();
	m_ForcedValue.Init();
	m_ForcedMinFalloff = GetMinFalloff();
	m_ForcedInfluence = 0.0f;
}

#endif // C_SPATIALENTITY_H