//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: Classes/types to represent fill brushes of varius types
//
//=============================================================================//

#ifndef FILLBRUSH_H
#define FILLBRUSH_H

#ifdef _WIN32
#pragma once
#endif

#include <float.h>
#include "color.h"
#include "utlvector.h"
#include "uilength.h"

namespace panorama
{

//-----------------------------------------------------------------------------
// Purpose: Represents a gradient color stop value
//-----------------------------------------------------------------------------
class CGradientColorStop
{
public:

	CGradientColorStop()
	{
		m_flPercent = 0.0f;
		m_color = Color( 0, 0, 0, 0 );
	}

	CGradientColorStop( float flPercent, Color color )
	{
		m_flPercent = clamp( flPercent, 0.0f, 1.0f );
		m_color = color;
	}

	void SetPosition( float flPercent )
	{
		m_flPercent = clamp( flPercent, 0.0f, 1.0f );
	}

	void SetColor( Color color )
	{
		m_color = color;
	}

	void SetColor( int r, int g, int b, int a )
	{
		m_color.SetColor( r, g, b, a );
	}

	float GetPosition() const { return m_flPercent; }
	Color GetColor() const { return m_color; }

	bool operator==( const CGradientColorStop &rhs ) const
	{
		return ( m_flPercent == rhs.m_flPercent && m_color == rhs.m_color );
	}

	bool operator!=( const CGradientColorStop &rhs ) const
	{
		return !(*this == rhs);
	}

private:

	// 0.0->1.0f
	float m_flPercent;

	// rgba color
	Color m_color;
};



//-----------------------------------------------------------------------------
// Purpose: Represents a particle system 
//-----------------------------------------------------------------------------
class CParticleSystem
{
public:

	// Default constructor, sets empty system
	CParticleSystem()
	{
		SetParticleSystem( Vector( 0.0f, 0.0f, 0.0f ), Vector( 0.0f, 0.0f, 0.0f ), 0.0f, 0.0f, 0.0f, 0.0f,
						   0.0f, 0.0f, Vector( 0.0f, 0.0f, 0.0f ), Vector( 0.0f, 0.0f, 0.0f ), 
						   Vector( -9999999.0f, -9999999.0f, -9999999.0f ), Vector( 9999999.0f, 9999999.0f, 9999999.0f ),
						   Vector( 0.0f, 0.0f, 0.0f ), Vector( 0.0f, 0.0f, 0.0f ),
						   Color( 0, 0, 0, 0 ), Color( 0, 0, 0, 0 ), Color( 0, 0, 0, 0 ), Color (0, 0, 0, 0 ), 1.0, 0.0f, 0.0f, 0.0f );
	}

	CParticleSystem( Vector vecBasePositon, Vector vecBasePositionVariance, float flSize, float flSizeVariance, float flParticlesPerSecond, float flParticlesPerSecondVariance,
					 float flLifeSpanSeconds, float flLifeSpanSecondsVariance, Vector vecInitialVelocity, Vector vecInitialVelocityVariance,  Vector vecVelocityMin, Vector vecVelocityMax,
					 Vector vecGravityAcceleration, Vector vecGravityAccelerationParticleVariance,
					 Color colorStart, Color colorStartVariance, Color colorEnd, Color colorEndVariance, float flSharpness, float flSharpnessVariance, float flFlicker, float flFlickerVariance )
	{
		SetParticleSystem( vecBasePositon, vecBasePositionVariance, flSize, flSizeVariance, flParticlesPerSecond, flParticlesPerSecondVariance,
			flLifeSpanSeconds, flLifeSpanSecondsVariance, vecInitialVelocity, vecInitialVelocityVariance, vecVelocityMin, vecVelocityMax, 
			vecGravityAcceleration, vecGravityAccelerationParticleVariance,
			colorStart, colorStartVariance, colorEnd, colorEndVariance, flSharpness, flSharpnessVariance, flFlicker, flFlickerVariance );
	}


	void SetParticleSystem( Vector vecBasePositon, Vector vecBasePositionVariance, float flSize, float flSizeVariance, float flParticlesPerSecond, float flParticlesPerSecondVariance,
		float flLifeSpanSeconds, float flLifeSpanSecondsVariance, Vector vecInitialVelocity, Vector vecInitialVelocityVariance, Vector vecVelocityMin, Vector vecVelocityMax,
		Vector vecGravityAcceleration, Vector vecGravityAccelerationParticleVariance, Color colorStart, Color colorStartVariance, Color colorEnd, Color colorEndVariance, 
		float flSharpness, float flSharpnessVariance, float flFlicker, float flFlickerVariance )
	{
		m_vecBasePosition = vecBasePositon;
		m_vecBasePositionVariance = vecBasePositionVariance;

		m_flParticleSize = flSize;
		m_flParticleSizeVariance = flSizeVariance;

		m_flParticlesPerSecond = flParticlesPerSecond;
		m_flParticlesPerSecondVariance = flParticlesPerSecondVariance;

		m_flParticleLifeSpanSeconds = flLifeSpanSeconds;
		m_flParticleLifeSpanSecondsVariance = flLifeSpanSecondsVariance;

		m_vecParticleInitialVelocity = vecInitialVelocity;
		m_vecParticleInitialVelocityVariance = vecInitialVelocityVariance;

		m_vecVelocityMin = vecVelocityMin;
		m_vecVelocityMax = vecVelocityMax;

		m_vecGravityAcceleration = vecGravityAcceleration;
		m_vecGravityAccelerationParticleVariance = vecGravityAccelerationParticleVariance;

		m_colorStartRGBA = colorStart;
		m_colorStartRGBAVariance = colorStartVariance;

		m_colorEndRGBA = colorEnd;
		m_colorEndRGBAVariance = colorEndVariance;

		m_flSharpness = flSharpness;
		m_flSharpnessVariance = flSharpnessVariance;

		m_flFlicker = flFlicker;
		m_flFlickerVariance = flFlickerVariance;
	}


	bool operator==( const CParticleSystem &rhs ) const
	{
		if ( m_vecBasePosition != rhs.m_vecBasePosition )
			return false;

		if ( m_vecBasePositionVariance != rhs.m_vecBasePositionVariance )
			return false;

		if ( m_flParticleSize != rhs.m_flParticleSize )
			return false;

		if ( m_flParticleSizeVariance != rhs.m_flParticleSizeVariance )
			return false;

		if ( m_flParticlesPerSecond != rhs.m_flParticlesPerSecond )
			return false;

		if ( m_flParticlesPerSecondVariance != rhs.m_flParticlesPerSecondVariance ) 
			return false;

		if ( m_flParticleLifeSpanSeconds != rhs.m_flParticleLifeSpanSeconds )
			return false;

		if ( m_flParticleLifeSpanSecondsVariance != rhs.m_flParticleLifeSpanSecondsVariance )
			return false;

		if ( m_vecParticleInitialVelocity != rhs.m_vecParticleInitialVelocity )
			return false;

		if ( m_vecParticleInitialVelocityVariance != rhs.m_vecParticleInitialVelocityVariance )
			return false;

		if ( m_vecGravityAcceleration != rhs.m_vecGravityAcceleration )
			return false;

		if ( m_vecGravityAccelerationParticleVariance != rhs.m_vecGravityAccelerationParticleVariance )
			return false;

		if ( m_colorStartRGBA != rhs.m_colorStartRGBA )
			return false;

		if ( m_colorStartRGBAVariance != rhs.m_colorStartRGBAVariance )
			return false;

		if ( m_colorEndRGBA != rhs.m_colorEndRGBA )
			return false;

		if ( m_colorEndRGBAVariance != rhs.m_colorEndRGBAVariance )
			return false;

		if ( m_flSharpness != rhs.m_flSharpness )
			return false;

		if ( m_flSharpnessVariance != rhs.m_flSharpnessVariance )
			return false;

		if ( m_flFlicker != rhs.m_flFlicker )
			return false;

		if ( m_flFlickerVariance != rhs.m_flFlickerVariance )
			return false;

		if ( m_vecVelocityMin != rhs.m_vecVelocityMin )
			return false;

		if ( m_vecVelocityMax != rhs.m_vecVelocityMax )
			return false;

		return true;
	}

	void Interpolate( float flProgress, CParticleSystem &target )
	{
		m_vecBasePosition.x = Lerp( flProgress, m_vecBasePosition.x, target.m_vecBasePosition.x );
		m_vecBasePosition.y = Lerp( flProgress, m_vecBasePosition.y, target.m_vecBasePosition.y );
		m_vecBasePosition.z = Lerp( flProgress, m_vecBasePosition.z, target.m_vecBasePosition.z );

		m_vecBasePositionVariance.x = Lerp( flProgress, m_vecBasePositionVariance.x, target.m_vecBasePositionVariance.x );
		m_vecBasePositionVariance.y = Lerp( flProgress, m_vecBasePositionVariance.y, target.m_vecBasePositionVariance.y );
		m_vecBasePositionVariance.z = Lerp( flProgress, m_vecBasePositionVariance.z, target.m_vecBasePositionVariance.z );

		m_flParticleSize = Lerp( flProgress, m_flParticleSize, target.m_flParticleSize );
		m_flParticleSizeVariance = Lerp( flProgress, m_flParticleSizeVariance, target.m_flParticleSizeVariance );

		m_flParticlesPerSecond = Lerp( flProgress, m_flParticlesPerSecond, target.m_flParticlesPerSecond );
		m_flParticlesPerSecondVariance = Lerp( flProgress, m_flParticlesPerSecondVariance, target.m_flParticlesPerSecondVariance );

		m_vecParticleInitialVelocity.x = Lerp( flProgress, m_vecParticleInitialVelocity.x, target.m_vecParticleInitialVelocity.x );
		m_vecParticleInitialVelocity.y = Lerp( flProgress, m_vecParticleInitialVelocity.y, target.m_vecParticleInitialVelocity.y );
		m_vecParticleInitialVelocity.z = Lerp( flProgress, m_vecParticleInitialVelocity.z, target.m_vecParticleInitialVelocity.z );

		m_vecParticleInitialVelocityVariance.x = Lerp( flProgress, m_vecParticleInitialVelocityVariance.x, target.m_vecParticleInitialVelocityVariance.x );
		m_vecParticleInitialVelocityVariance.y = Lerp( flProgress, m_vecParticleInitialVelocityVariance.y, target.m_vecParticleInitialVelocityVariance.y );
		m_vecParticleInitialVelocityVariance.z = Lerp( flProgress, m_vecParticleInitialVelocityVariance.z, target.m_vecParticleInitialVelocityVariance.z );

		m_vecVelocityMin.x = Lerp( flProgress, m_vecVelocityMin.x, target.m_vecVelocityMin.x );
		m_vecVelocityMin.y = Lerp( flProgress, m_vecVelocityMin.y, target.m_vecVelocityMin.y );

		m_vecVelocityMax.x = Lerp( flProgress, m_vecVelocityMax.x, target.m_vecVelocityMax.x );
		m_vecVelocityMax.y = Lerp( flProgress, m_vecVelocityMax.y, target.m_vecVelocityMax.y );

		m_vecGravityAcceleration.x = Lerp( flProgress, m_vecGravityAcceleration.x, target.m_vecGravityAcceleration.x );
		m_vecGravityAcceleration.y = Lerp( flProgress, m_vecGravityAcceleration.y, target.m_vecGravityAcceleration.y );
		m_vecGravityAcceleration.z = Lerp( flProgress, m_vecGravityAcceleration.z, target.m_vecGravityAcceleration.z );

		m_vecGravityAccelerationParticleVariance.x = Lerp( flProgress, m_vecGravityAccelerationParticleVariance.x, target.m_vecGravityAccelerationParticleVariance.x );
		m_vecGravityAccelerationParticleVariance.y = Lerp( flProgress, m_vecGravityAccelerationParticleVariance.y, target.m_vecGravityAccelerationParticleVariance.y );
		m_vecGravityAccelerationParticleVariance.z = Lerp( flProgress, m_vecGravityAccelerationParticleVariance.z, target.m_vecGravityAccelerationParticleVariance.z );

		m_colorStartRGBA.SetColor( 
			Lerp( flProgress, m_colorStartRGBA.r(), target.m_colorStartRGBA.r() ),
			Lerp( flProgress, m_colorStartRGBA.g(), target.m_colorStartRGBA.g() ),
			Lerp( flProgress, m_colorStartRGBA.b(), target.m_colorStartRGBA.b() ),
			Lerp( flProgress, m_colorStartRGBA.a(), target.m_colorStartRGBA.a() )
		);

		m_colorStartRGBAVariance.SetColor( 
			Lerp( flProgress, m_colorStartRGBAVariance.r(), target.m_colorStartRGBAVariance.r() ),
			Lerp( flProgress, m_colorStartRGBAVariance.g(), target.m_colorStartRGBAVariance.g() ),
			Lerp( flProgress, m_colorStartRGBAVariance.b(), target.m_colorStartRGBAVariance.b() ),
			Lerp( flProgress, m_colorStartRGBAVariance.a(), target.m_colorStartRGBAVariance.a() )
		);

		m_colorEndRGBA.SetColor( 
			Lerp( flProgress, m_colorEndRGBA.r(), target.m_colorEndRGBA.r() ),
			Lerp( flProgress, m_colorEndRGBA.g(), target.m_colorEndRGBA.g() ),
			Lerp( flProgress, m_colorEndRGBA.b(), target.m_colorEndRGBA.b() ),
			Lerp( flProgress, m_colorEndRGBA.a(), target.m_colorEndRGBA.a() )
			);

		m_colorEndRGBAVariance.SetColor( 
			Lerp( flProgress, m_colorEndRGBAVariance.r(), target.m_colorEndRGBAVariance.r() ),
			Lerp( flProgress, m_colorEndRGBAVariance.g(), target.m_colorEndRGBAVariance.g() ),
			Lerp( flProgress, m_colorEndRGBAVariance.b(), target.m_colorEndRGBAVariance.b() ),
			Lerp( flProgress, m_colorEndRGBAVariance.a(), target.m_colorEndRGBAVariance.a() )
			);

		m_flSharpness = Lerp( flProgress, m_flSharpness, target.m_flSharpness );
		m_flSharpnessVariance = Lerp( flProgress, m_flSharpnessVariance, target.m_flSharpnessVariance );

		m_flFlicker = Lerp( flProgress, m_flFlicker, target.m_flFlicker );
		m_flFlickerVariance = Lerp( flProgress, m_flFlickerVariance, target.m_flFlickerVariance );

	}

	void ScaleLengthValues( float flScaleFactor )
	{
		// Scale all X/Y size/position values
		m_vecBasePosition.x *= flScaleFactor;
		m_vecBasePosition.y *= flScaleFactor;

		m_vecBasePositionVariance.x *= flScaleFactor;
		m_vecBasePositionVariance.y *= flScaleFactor;

		m_flParticleSize *= flScaleFactor;
		m_flParticleSizeVariance *= flScaleFactor;

		m_vecParticleInitialVelocity.x *= flScaleFactor;
		m_vecParticleInitialVelocity.y *= flScaleFactor;

		m_vecParticleInitialVelocityVariance.x *= flScaleFactor;
		m_vecParticleInitialVelocityVariance.y *= flScaleFactor;

		m_vecGravityAcceleration.x *= flScaleFactor;
		m_vecGravityAcceleration.y *= flScaleFactor;

		m_vecGravityAccelerationParticleVariance.x *= flScaleFactor;
		m_vecGravityAccelerationParticleVariance.y *= flScaleFactor;

		m_vecVelocityMin.x *= flScaleFactor;
		m_vecVelocityMin.y *= flScaleFactor;

		m_vecVelocityMax.x *= flScaleFactor;
		m_vecVelocityMax.y *= flScaleFactor;
	}

	Vector GetBasePosition() const
	{
		return m_vecBasePosition;
	}

	Vector GetBasePositionVariance() const
	{
		return m_vecBasePositionVariance;
	}

	float GetParticleSize() const
	{
		return m_flParticleSize;
	}

	float GetParticleSizeVariance() const
	{
		return m_flParticleSizeVariance;
	}

	float GetParticlesPerSecond() const
	{
		return m_flParticlesPerSecond;
	}

	float GetParticlesPerSecondVariance() const
	{
		return m_flParticlesPerSecondVariance;
	}

	float GetParticleLifeSpanSeconds() const
	{
		return m_flParticleLifeSpanSeconds;
	}

	float GetParticleLifeSpanSecondsVariance() const
	{
		return m_flParticleLifeSpanSecondsVariance;
	}

	Vector GetParticleInitialVelocity() const
	{
		return m_vecParticleInitialVelocity;
	}

	Vector GetParticleInitialVelocityVariance() const
	{
		return m_vecParticleInitialVelocityVariance;
	}

	Vector GetParticleVelocityMin() const
	{
		return m_vecVelocityMin;
	}

	Vector GetParticleVelocityMax() const
	{
		return m_vecVelocityMax;
	}

	Vector GetGravityAcceleration() const
	{
		return m_vecGravityAcceleration;
	}

	Vector GetGravityAccelerationParticleVariance() const
	{
		return m_vecGravityAccelerationParticleVariance;
	}

	Color GetStartColor() const
	{
		return m_colorStartRGBA;
	}

	Color GetStartColorVariance() const
	{
		return m_colorStartRGBAVariance;
	}

	Color GetEndColor() const
	{
		return m_colorEndRGBA;
	}

	Color GetEndColorVariance() const
	{
		return m_colorEndRGBAVariance;
	}
	
	float GetSharpness() const
	{
		return m_flSharpness;
	}

	float GetSharpnessVariance() const
	{
		return m_flSharpnessVariance;
	}

	float GetFlicker() const 
	{
		return m_flFlicker;
	}

	float GetFlickerVariance() const 
	{
		return m_flFlickerVariance;
	}

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName )
	{
		VALIDATE_SCOPE();
	}
#endif

private:

	Vector m_vecBasePosition;
	Vector m_vecBasePositionVariance;

	float m_flParticleSize;
	float m_flParticleSizeVariance;

	float m_flParticlesPerSecond;
	float m_flParticlesPerSecondVariance;

	float m_flParticleLifeSpanSeconds;
	float m_flParticleLifeSpanSecondsVariance;

	Vector m_vecParticleInitialVelocity;
	Vector m_vecParticleInitialVelocityVariance;

	Vector m_vecVelocityMin;
	Vector m_vecVelocityMax;

	Vector m_vecGravityAcceleration;
	Vector m_vecGravityAccelerationParticleVariance;
	
	Color m_colorStartRGBA;
	Color m_colorStartRGBAVariance;

	Color m_colorEndRGBA;
	Color m_colorEndRGBAVariance;

	float m_flSharpness;
	float m_flSharpnessVariance;

	float m_flFlicker;
	float m_flFlickerVariance;
};


//-----------------------------------------------------------------------------
// Purpose: Represents a linear gradient 
//-----------------------------------------------------------------------------
class CLinearGradient
{
public:
	CLinearGradient()
	{
		m_StartPoint[0].SetLength( 0.0 );
		m_StartPoint[1].SetLength( 0.0 );
		m_EndPoint[0].SetLength( 0.0 );
		m_EndPoint[1].SetLength( 0.0 );
	}

	CLinearGradient( CUILength StartX, CUILength StartY, CUILength EndX, CUILength EndY, const CUtlVector<CGradientColorStop> &vecStopColors )
	{
		SetGradient( StartX, StartY, EndX, EndY, vecStopColors );
	}

	void SetGradient( CUILength StartX, CUILength StartY, CUILength EndX, CUILength EndY, const CUtlVector<CGradientColorStop> &vecStopColors )
	{
		m_StartPoint[0] = StartX;
		m_StartPoint[1] = StartY;
		m_EndPoint[0] = EndX;
		m_EndPoint[1] = EndY;

		m_vecStops.AddMultipleToTail( vecStopColors.Count(), vecStopColors.Base() );
	}

	void SetControlPoints( CUILength StartX, CUILength StartY, CUILength EndX, CUILength EndY )
	{
		m_StartPoint[0] = StartX;
		m_StartPoint[1] = StartY;
		m_EndPoint[0] = EndX;
		m_EndPoint[1] = EndY;
	}

	void GetStartPoint( CUILength &startX, CUILength &startY ) const
	{
		startX = m_StartPoint[0];
		startY = m_StartPoint[1];
	}

	void GetEndPoint( CUILength &endX, CUILength &endY ) const 
	{
		endX = m_EndPoint[0];
		endY = m_EndPoint[1];
	}

	const CUtlVector<CGradientColorStop> &AccessStopColors() const
	{
		return m_vecStops;
	}

	CUtlVector<CGradientColorStop> &AccessMutableStopColors()
	{
		return m_vecStops;
	}

	bool operator==( const CLinearGradient &rhs ) const
	{
		if ( m_StartPoint[0] != rhs.m_StartPoint[0] || m_StartPoint[1] != rhs.m_StartPoint[1] || m_EndPoint[0] != rhs.m_EndPoint[0] || m_EndPoint[1] != rhs.m_EndPoint[1] )
			return false;

		if ( m_vecStops.Count() != rhs.m_vecStops.Count() )
			return false;

		FOR_EACH_VEC( m_vecStops, i )
		{
			if ( m_vecStops[i] != rhs.m_vecStops[i] )
				return false;
		}

		return true;
	}

	void ScaleLengthValues( float flScaleFactor )
	{
		m_StartPoint[0].ScaleLengthValue( flScaleFactor );
		m_StartPoint[1].ScaleLengthValue( flScaleFactor );
		m_EndPoint[0].ScaleLengthValue( flScaleFactor );
		m_EndPoint[1].ScaleLengthValue( flScaleFactor );
	}

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName )
	{
		VALIDATE_SCOPE();
		ValidateObj( m_vecStops );
	}
#endif

private:

	// Start/end point determine angle vector
	CUILength m_StartPoint[2];
	CUILength m_EndPoint[2];
	CCopyableUtlVector<CGradientColorStop> m_vecStops;
};


//-----------------------------------------------------------------------------
// Purpose: Represents a radial gradient 
//-----------------------------------------------------------------------------
class CRadialGradient
{
public:
	CRadialGradient()
	{
		m_Center[0].SetLength( 0.0 );
		m_Center[1].SetLength( 0.0 );
		m_Offset[0].SetLength( 0.0 );
		m_Offset[1].SetLength( 0.0 );
		m_Radius[0].SetLength( 0.0 );
		m_Radius[1].SetLength( 0.0 );
	}

	CRadialGradient( CUILength centerX, CUILength centerY, CUILength offsetX, CUILength offsetY, CUILength radiusX, CUILength radiusY, const CUtlVector<CGradientColorStop> &vecStopColors )
	{
		SetGradient( centerX, centerY, offsetX, offsetY, radiusX, radiusY, vecStopColors );
	}

	void SetGradient( CUILength centerX, CUILength centerY, CUILength offsetX, CUILength offsetY, CUILength radiusX, CUILength radiusY, const CUtlVector<CGradientColorStop> &vecStopColors )
	{
		m_Center[0] = centerX;
		m_Center[1] = centerY;
		m_Offset[0] = offsetX;
		m_Offset[1] = offsetY;
		m_Radius[0] = radiusX;
		m_Radius[1] = radiusY;

		m_vecStops.AddMultipleToTail( vecStopColors.Count(), vecStopColors.Base() );
	}

	void GetCenterPoint( CUILength &centerX, CUILength &centerY ) const
	{
		centerX = m_Center[0];
		centerY = m_Center[1];
	}

	void GetOffsetDistance( CUILength &offsetX, CUILength &offsetY ) const 
	{
		offsetX = m_Offset[0];
		offsetY = m_Offset[1];
	}

	void GetRadii( CUILength &radiusX, CUILength &radiusY ) const 
	{
		radiusX = m_Radius[0];
		radiusY = m_Radius[1];
	}

	void SetCenterPoint( const CUILength &x, const CUILength &y )
	{
		m_Center[0] = x;
		m_Center[1] = y;
	}

	void SetOffsetDistance( const CUILength &x, const CUILength &y )
	{
		m_Offset[0] = x;
		m_Offset[1] = y;
	}

	void SetRadii( const CUILength &x, const CUILength &y )
	{
		m_Radius[0] = x;
		m_Radius[1] = y;
	}

	const CUtlVector<CGradientColorStop> &AccessStopColors() const
	{
		return m_vecStops;
	}

	CUtlVector<CGradientColorStop> &AccessMutableStopColors()
	{
		return m_vecStops;
	}

	bool operator==( const CRadialGradient &rhs ) const
	{
		if (	m_Center[0] != rhs.m_Center[0] || m_Center[1] != rhs.m_Center[1] || 
				m_Offset[0] != rhs.m_Offset[0] || m_Offset[1] != rhs.m_Offset[1] ||
				m_Radius[0] != rhs.m_Radius[0] || m_Radius[1] != rhs.m_Radius[1] )
		{
			return false;
		}

		if ( m_vecStops.Count() != rhs.m_vecStops.Count() )
			return false;

		FOR_EACH_VEC( m_vecStops, i )
		{
			if ( m_vecStops[i] != rhs.m_vecStops[i] )
				return false;
		}

		return true;
	}

	void ScaleLengthValues( float flScaleFactor )
	{
		m_Center[0].ScaleLengthValue( flScaleFactor );
		m_Center[1].ScaleLengthValue( flScaleFactor );
		m_Offset[0].ScaleLengthValue( flScaleFactor );
		m_Offset[1].ScaleLengthValue( flScaleFactor );
		m_Radius[0].ScaleLengthValue( flScaleFactor );
		m_Radius[1].ScaleLengthValue( flScaleFactor );
	}

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName )
	{
		VALIDATE_SCOPE();
		ValidateObj( m_vecStops );
	}
#endif

private:

	// Start/end point determine angle vector
	CUILength m_Center[2];
	CUILength m_Offset[2];
	CUILength m_Radius[2];
	CCopyableUtlVector<CGradientColorStop> m_vecStops;
};


//-----------------------------------------------------------------------------
// Purpose: Represents a fill color/stroke, may be a gradient
//-----------------------------------------------------------------------------
class CFillBrush
{
public:

	// Types of strokes we support
	enum EStrokeType
	{
		k_EStrokeTypeFillColor,
		k_EStrokeTypeLinearGradient,
		k_EStrokeTypeRadialGradient,
		k_EStrokeTypeParticleSystem,
	};

	// Default constructor, sets transparent fill color
	CFillBrush()
	{
		m_eType = k_EStrokeTypeFillColor;
		m_FillColor.SetColor( 0, 0, 0, 0 );
	}

	// Constructor for standard color fill
	CFillBrush( int r, int g, int b, int a )
	{
		m_eType = k_EStrokeTypeFillColor;
		m_FillColor.SetColor( r, g, b, a );
	}

	// Constructor for linear gradient fill
	CFillBrush( CUILength StartX, CUILength StartY, CUILength EndX, CUILength EndY, const CUtlVector<CGradientColorStop> &vecStopColors )
	{
		m_eType = k_EStrokeTypeLinearGradient;
		m_LinearGradient.SetGradient( StartX, StartY, EndX, EndY, vecStopColors );
	}

	// Constructor for radial gradient
	CFillBrush( CUILength centerX, CUILength centerY, CUILength offsetX, CUILength offsetY, CUILength radiusX, CUILength radiusY, const CUtlVector<CGradientColorStop> &vecStopColors )
	{
		m_eType = k_EStrokeTypeRadialGradient;
		m_RadialGradient.SetGradient( centerX, centerY, offsetX, offsetY, radiusX, radiusY, vecStopColors );
	}

	// Constructor for particle system
	CFillBrush( Vector vecBasePositon, Vector vecBasePositionVariance, float flSize, float flSizeVariance, float flParticlesPerSecond, float flParticlesPerSecondVariance,
		float flLifeSpanSeconds, float flLifeSpanSecondsVariance, Vector vecInitialVelocity, Vector vecInitialVelocityVariance, Vector vecVelocityMin, Vector vecVelocityMax,
		Vector vecGravityAcceleration, Vector vecGravityAccelerationParticleVariance,
		Color colorStart, Color colorStartVariance, Color colorEnd, Color colorEndVariance, float flSharpness, float flSharpnessVariance, float flFlicker, float flFlickerVariance )
	{
		m_eType = k_EStrokeTypeParticleSystem;
		m_ParticleSystem.SetParticleSystem( vecBasePositon, vecBasePositionVariance, flSize, flSizeVariance, flParticlesPerSecond, flParticlesPerSecondVariance,
			flLifeSpanSeconds, flLifeSpanSecondsVariance, vecInitialVelocity, vecInitialVelocityVariance, vecVelocityMin, vecVelocityMax,
			vecGravityAcceleration, vecGravityAccelerationParticleVariance,
			colorStart, colorStartVariance, colorEnd, colorEndVariance, flSharpness, flSharpnessVariance, flFlicker, flFlickerVariance );
	}

	// Get type
	EStrokeType GetType() const { return m_eType; }

	// Set to a fill color value
	void SetToFillColor( Color color )
	{
		m_eType = k_EStrokeTypeFillColor;
		m_FillColor = color;
	}

	// Get fill color for fill stroke types
	Color GetFillColor() const 
	{ 
		Assert( m_eType == k_EStrokeTypeFillColor ); 
		return m_FillColor; 
	}

	// Set brush to a linear gradient value
	void SetToLinearGradient( const CUILength &startX, const CUILength &startY, const CUILength &endX, const CUILength &endY, const CUtlVector<CGradientColorStop> &vecColorStops )
	{
		m_eType = k_EStrokeTypeLinearGradient;
		m_LinearGradient.SetGradient( startX, startY, endX, endY, vecColorStops );
	}

	// Set to a radial gradient value
	void SetToRadialGradient( CUILength centerX, CUILength centerY, CUILength offsetX, CUILength offsetY, CUILength radiusX, CUILength radiusY, const CUtlVector<CGradientColorStop> &vecStopColors )
	{
		m_eType = k_EStrokeTypeRadialGradient;
		m_RadialGradient.SetGradient( centerX, centerY, offsetX, offsetY, radiusX, radiusY, vecStopColors );
	}

	// Set to particle system value
	void SetToParticleSystem( Vector vecBasePositon, Vector vecBasePositionVariance, float flSize, float flSizeVariance, float flParticlesPerSecond, float flParticlesPerSecondVariance,
		float flLifeSpanSeconds, float flLifeSpanSecondsVariance, Vector vecInitialVelocity, Vector vecInitialVelocityVariance, Vector vecVelocityMin, Vector vecVelocityMax,
		Vector vecGravityAcceleration, Vector vecGravityAccelerationParticleVariance,
		Color colorStart, Color colorStartVariance, Color colorEnd, Color colorEndVariance, float flSharpness, float flSharpnessVariance, float flFlicker, float flFlickerVariance )
	{
		m_eType = k_EStrokeTypeParticleSystem;
		m_ParticleSystem.SetParticleSystem( vecBasePositon, vecBasePositionVariance, flSize, flSizeVariance, flParticlesPerSecond, flParticlesPerSecondVariance, 
			flLifeSpanSeconds, flLifeSpanSecondsVariance, vecInitialVelocity, vecInitialVelocityVariance, vecVelocityMin, vecVelocityMax, vecGravityAcceleration, vecGravityAccelerationParticleVariance,
			colorStart, colorStartVariance, colorEnd, colorEndVariance, flSharpness, flSharpnessVariance, flFlicker, flFlickerVariance );
	}

	// Get start/end pos for linear gradient types
	void GetStartAndEndPoints( CUILength &StartX, CUILength &StartY, CUILength &EndX, CUILength &EndY ) const
	{
		Assert( m_eType == k_EStrokeTypeLinearGradient ); 
		m_LinearGradient.GetStartPoint( StartX, StartY );
		m_LinearGradient.GetEndPoint( EndX, EndY );
	}

	// Access start/end pos for linear or radial gradient types
	const CUtlVector<CGradientColorStop> &AccessStopColors() const
	{
		Assert( m_eType == k_EStrokeTypeLinearGradient || m_eType == k_EStrokeTypeRadialGradient ); 
		if ( m_eType == k_EStrokeTypeLinearGradient )
			return m_LinearGradient.AccessStopColors();

		if ( m_eType == k_EStrokeTypeRadialGradient )
			return m_RadialGradient.AccessStopColors();

		// Otherwise this is bad, let's just return the hopefully empty linear vector
		return m_LinearGradient.AccessStopColors();
	}

	// Get start/end pos for linear gradient types
	void GetRadialGradientValues( CUILength &centerX, CUILength &centerY, CUILength &offsetX, CUILength &offsetY, CUILength &radiusX, CUILength	&radiusY ) const
	{
		Assert( m_eType == k_EStrokeTypeRadialGradient ); 
		m_RadialGradient.GetCenterPoint( centerX, centerY );
		m_RadialGradient.GetOffsetDistance( offsetX, offsetY );
		m_RadialGradient.GetRadii( radiusX, radiusY );
	}

	CParticleSystem * AccessParticleSystem()
	{
		Assert( m_eType == k_EStrokeTypeParticleSystem );
		return &m_ParticleSystem;
	}

	bool Interpolate( float flActualWidth, float flActualHeight, CFillBrush &target, float flProgress );

	bool operator==( const CFillBrush &rhs ) const
	{
		if ( m_eType != rhs.m_eType )
			return false;

		switch( m_eType )
		{
		case k_EStrokeTypeFillColor:
			return m_FillColor == rhs.m_FillColor;
		case k_EStrokeTypeLinearGradient:
			return m_LinearGradient == rhs.m_LinearGradient;
		case k_EStrokeTypeRadialGradient:
			return m_RadialGradient == rhs.m_RadialGradient;
		case k_EStrokeTypeParticleSystem:
			return m_ParticleSystem == rhs.m_ParticleSystem;
		default:
			AssertMsg( false, "Invalid type on fillbrush" );
			return false;
		}
	}

	bool operator!=( const CFillBrush &rhs ) const
	{
		return !( *this == rhs );
	}

	void ScaleLengthValues( float flScaleFactor )
	{
		if ( m_eType == k_EStrokeTypeLinearGradient )
			m_LinearGradient.ScaleLengthValues( flScaleFactor );
		else if ( m_eType == k_EStrokeTypeRadialGradient )
			m_RadialGradient.ScaleLengthValues( flScaleFactor );
		else if ( m_eType == k_EStrokeTypeParticleSystem )
			m_ParticleSystem.ScaleLengthValues( flScaleFactor );

	}
	
#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName )
	{
		VALIDATE_SCOPE();
		ValidateObj( m_LinearGradient );
		ValidateObj( m_RadialGradient );
		ValidateObj( m_ParticleSystem );
	}
#endif
private:

	void ConvertToRadialGradient();
	void ConvertToLinearGradient();
	void NormalizeStopCount( CUtlVector<CGradientColorStop> &vec, int nStopsNeeded, Color defaultColor );

	EStrokeType m_eType;

	Color m_FillColor;
	CLinearGradient m_LinearGradient;
	CRadialGradient m_RadialGradient;
	CParticleSystem m_ParticleSystem;
};


//-----------------------------------------------------------------------------
// Purpose: Represents a collection of fill brushes, when filling geometry with
// such a collection they should be applied in order and blended appropriately.
//-----------------------------------------------------------------------------
#define MAX_FILL_BRUSHES_PER_COLLECTION 8
class CFillBrushCollection
{
public:


	struct FillBrush_t
	{
		CFillBrush m_Brush;
		float m_Opacity;

		bool operator==( const FillBrush_t &rhs ) const { return (m_Brush == rhs.m_Brush && m_Opacity == rhs.m_Opacity); }
		bool operator!=( const FillBrush_t &rhs ) const { return !(*this == rhs ); }
	};

	CFillBrushCollection() { }

	// Get brush count
	uint32 GetBrushCount() const { return m_vecFillBrushes.Count(); }

	// Access brush vector directly
	CUtlVectorFixed< FillBrush_t, MAX_FILL_BRUSHES_PER_COLLECTION > &AccessBrushes() { return m_vecFillBrushes; }
	const CUtlVectorFixed< FillBrush_t, MAX_FILL_BRUSHES_PER_COLLECTION> &AccessBrushes() const { return m_vecFillBrushes; }

	// Add brush to collection
	void AddFillBrush( const CFillBrush &brush, float opacity = 1.0 ) 
	{ 

		int i = m_vecFillBrushes.AddToTail(); 
		m_vecFillBrushes[i].m_Brush = brush;
		m_vecFillBrushes[i].m_Opacity = opacity;
	}

	void ScaleLengthValues( float flScaleFactor )
	{
		FOR_EACH_VEC( m_vecFillBrushes, i )
		{
			m_vecFillBrushes[i].m_Brush.ScaleLengthValues( flScaleFactor );
		}
	}

	int GetNumParticleSystems()
	{
		int nParticleSystems = 0;
		FOR_EACH_VEC( m_vecFillBrushes, i )
		{
			if ( m_vecFillBrushes[i].m_Brush.GetType() == CFillBrush::k_EStrokeTypeParticleSystem )
				++nParticleSystems;
		}

		return nParticleSystems;
	}

	void Clear()
	{
		m_vecFillBrushes.RemoveAll();
	}

	// Interpolate between two fill brushes
	void Interpolate( float flActualWidth, float flActualHeight, const CFillBrushCollection &target, float flProgress );
	bool operator==( const CFillBrushCollection &rhs ) const;

#ifdef DBGFLAG_VALIDATE
	void Validate( CValidator &validator, const tchar *pchName )
	{
		VALIDATE_SCOPE();
		ValidateObj( m_vecFillBrushes );
		FOR_EACH_VEC( m_vecFillBrushes, i )
		{
			ValidateObj( m_vecFillBrushes[i].m_Brush );
		}
	}
#endif

private:

	CCopyableUtlVectorFixed< FillBrush_t, MAX_FILL_BRUSHES_PER_COLLECTION > m_vecFillBrushes;
};

} // namespace panorama

#endif // FILLBRUSH_H