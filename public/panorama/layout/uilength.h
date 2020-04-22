//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef UILENGTH_H
#define UILENGTH_H

#ifdef _WIN32
#pragma once
#endif

#include "float.h"
#include "tier0/dbg.h"
#include "tier1/utlvector.h"
#include "mathlib/mathlib.h"
#include "../panorama.h"

namespace panorama
{

//-----------------------------------------------------------------------------
// Purpose: defines 
//-----------------------------------------------------------------------------
const float k_flFloatAuto = FLT_MAX;
const float k_flFloatNotSet = FLT_MIN;

// Sometimes we want to say "infinite width/height", but we can't use FLT_MAX as it's "auto"
const float k_flMaxWidthOrHeight = 512000000.0f;

//-----------------------------------------------------------------------------
// Purpose: Represents possible length values
//-----------------------------------------------------------------------------
class CUILength
{
public:
	enum EUILengthTypes
	{
		k_EUILengthUnset,
		k_EUILengthLength,
		k_EUILengthPercent,
		k_EUILengthFitChildren,
		k_EUILengthFillParentFlow,
		k_EUILengthHeightPercentage,
		k_EUILengthWidthPercentage,
	};

	CUILength() : m_flValue( k_flFloatNotSet ), m_eType( k_EUILengthUnset ) {}
	CUILength( float flValue, EUILengthTypes eType ) : m_flValue( flValue ), m_eType( eType ) {}

	bool IsSet() const { return (m_eType != k_EUILengthUnset); }
	bool IsLength() const { return (m_eType == k_EUILengthLength); }
	bool IsPercent() const { return (m_eType == k_EUILengthPercent); }
	bool IsFitChildren() const { return (m_eType == k_EUILengthFitChildren); }
	bool IsFillParentFlow() const { return (m_eType == k_EUILengthFillParentFlow); }
	bool IsHeightPercentage() const { return (m_eType == k_EUILengthHeightPercentage); }
	bool IsWidthPercentage() const { return (m_eType == k_EUILengthWidthPercentage); }

	float GetValue() const { return m_flValue; }
	CUILength::EUILengthTypes GetType() const { return m_eType; }

	void SetFitChildren() { Set( k_flFloatAuto, k_EUILengthFitChildren ); }
	void SetLength( float flValue ) { Set( flValue, k_EUILengthLength ); }
	void SetPercent( float flValue ) { Set( flValue, k_EUILengthPercent ); }	
	void SetFillParentFlow( float flWeight ) { Set( flWeight, k_EUILengthFillParentFlow ); }
	void SetParentFlow( float flValue ) { Set( flValue, k_EUILengthFillParentFlow ); }
	void SetHeightPercentage( float flValue ) { Set( flValue, k_EUILengthHeightPercentage ); }
	void SetWidthPercentage( float flValue ) { Set( flValue, k_EUILengthWidthPercentage ); }

	void Set( float flValue, EUILengthTypes eType )
	{
		m_flValue = flValue;
		m_eType = eType;
	}

	void ConvertToLength( float flTotalLength )
	{
		if ( m_eType == k_EUILengthPercent )
			Set( GetValueAsLength( flTotalLength ), k_EUILengthLength );
		else
			Assert( m_eType == k_EUILengthLength || m_eType == k_EUILengthUnset );
	}

	void ConvertToPercent( float flTotalLength )
	{
		if ( m_eType == k_EUILengthLength )
			Set( m_flValue / flTotalLength * 100.0, k_EUILengthPercent );
		else
			Assert( m_eType == k_EUILengthPercent || m_eType == k_EUILengthUnset );
	}

	float GetValueAsLength( float flTotalLength ) const
	{
		float flRet = m_flValue;
		if ( m_eType == k_EUILengthPercent )
			flRet = m_flValue * flTotalLength / 100.0;
		else
			Assert( m_eType == k_EUILengthLength );

		return flRet;
	}

	void ScaleLengthValue( float flScaleFactor )
	{
		if ( m_eType == k_EUILengthLength && m_flValue != k_flFloatAuto && m_flValue != k_flFloatNotSet )
		{
			// Don't make width/height values of 1.0 shrink, as things like 1 px horizontal rules will go invisible!  Kind of hacky,
			// but this mostly works without issue as we have so few things < 1px.
			if ( fabsf( 1.0f - fabsf( m_flValue ) ) < 0.1f && flScaleFactor < 1.0f  )
			{
				// Don't need to adjust
			}
			else
			{
				// If the original value was pixel aligned, try to keep that.  This is important so text doesn't accumulate error layers down
				// from us and thus get blurry.  Can't apply this rule to all values though as things like a shadow offset of 5.5 can be common
				// and the half pixel can be important for the shadow to balance on both sides of the layer.
				if ( fabsf( m_flValue - (float)RoundFloatToInt( m_flValue ) ) < 0.001f )
					m_flValue = (float)RoundFloatToInt( m_flValue * flScaleFactor );
				else
					m_flValue = m_flValue * flScaleFactor;
			}
		}
	}
	
	bool operator==( const CUILength &rhs ) const
	{
		return (m_flValue == rhs.m_flValue && m_eType == rhs.m_eType);
	}

	bool operator!=( const CUILength &rhs ) const
	{
		return !(*this == rhs);
	}


private:
	float m_flValue;
	EUILengthTypes m_eType;
};

// Helper for doing lerp on ui length values, converting % and such
CUILength LerpUILength( float flProgress, CUILength start, CUILength end, float flPixelSize );

} // namespace panorama

#endif //UILENGTH_H