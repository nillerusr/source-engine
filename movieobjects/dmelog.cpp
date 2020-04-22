//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================
#include "movieobjects/dmelog.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "datamodel/dmehandle.h"
#include "vstdlib/random.h"

#include "tier0/dbg.h"

#include <limits.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

LayerSelectionData_t::DataLayer_t::DataLayer_t( float frac, CDmeLogLayer *layer ) : 
	m_flStartFraction( frac )
{
	m_hData = layer;
}

LayerSelectionData_t::LayerSelectionData_t() :
	m_DataType( AT_UNKNOWN ),
	m_nDuration( 0 ),
	m_tStartOffset( DMETIME_ZERO )
{
	m_nHoldTimes[ 0 ] = m_nHoldTimes[ 1 ] = 0;
}

void LayerSelectionData_t::Release()
{
	for ( int i = 0; i < m_vecData.Count(); ++i )
	{
		DataLayer_t *dl = &m_vecData[ i ];
		if ( dl->m_hData.Get() )
		{
			g_pDataModel->DestroyElement( dl->m_hData->GetHandle() );
		}
	}
	m_vecData.Purge();
}

//-----------------------------------------------------------------------------
// Interpolatable types
//-----------------------------------------------------------------------------
inline bool IsInterpolableType( DmAttributeType_t type )
{
	return  ( type == AT_FLOAT ) ||
		( type == AT_COLOR ) ||
		( type == AT_VECTOR2 ) ||
		( type == AT_VECTOR3 ) ||
		( type == AT_QANGLE ) ||											  
		( type == AT_QUATERNION );
}

static Vector s_pInterolationPoints[ 4 ] = 
{
	Vector( 0.0f, 0.0f, 0.0f ),
	Vector( 0.0f, 0.0f, 0.0f ),
	Vector( 1.0f, 1.0f, 0.0f ),
	Vector( 1.0f, 1.0f, 0.0f )
};

static inline float ComputeInterpolationFactor( float flFactor, int nInterpolatorType )
{
	Vector out;
	Interpolator_CurveInterpolate
	( 
		nInterpolatorType, 
		s_pInterolationPoints[ 0 ], // unused
		s_pInterolationPoints[ 1 ], 
		s_pInterolationPoints[ 2 ], 
		s_pInterolationPoints[ 3 ], // unused
		flFactor, 
		out 
	);
	return out.y; // clamp( out.y, 0.0f, 1.0f );
}

float DmeLog_TimeSelection_t::AdjustFactorForInterpolatorType( float flFactor, int nSide ) const
{
	return ComputeInterpolationFactor( flFactor, m_nFalloffInterpolatorTypes[ nSide ] );
}

//-----------------------------------------------------------------------------
// NOTE: See DmeTimeSelectionTimes_t for return values, -1 means before, TS_TIME_COUNT means after
//-----------------------------------------------------------------------------
static inline int ComputeRegionForTime( DmeTime_t t, const DmeTime_t *pRegionTimes )
{
	if ( t >= pRegionTimes[TS_LEFT_HOLD] )
	{
		if ( t <= pRegionTimes[TS_RIGHT_HOLD] )
			return 2;
		return ( t <= pRegionTimes[TS_RIGHT_FALLOFF] ) ? 3 : 4;
	}
	return ( t >= pRegionTimes[TS_LEFT_FALLOFF] ) ? 1 : 0;
}


//-----------------------------------------------------------------------------
// NOTE: See DmeTimeSelectionTimes_t for return values, -1 means before, TS_TIME_COUNT means after
//-----------------------------------------------------------------------------
int DmeLog_TimeSelection_t::ComputeRegionForTime( DmeTime_t curtime ) const
{
	return ::ComputeRegionForTime( curtime, m_nTimes );
}


//-----------------------------------------------------------------------------
// per-type averaging methods
//-----------------------------------------------------------------------------
float DmeLog_TimeSelection_t::GetAmountForTime( DmeTime_t dmetime ) const
{
	float minfrac = 0.0f;

	float t = dmetime.GetSeconds();

	// FIXME, this is slow, we should cache this maybe?
	COMPILE_TIME_ASSERT( TS_TIME_COUNT == 4 );
	float times[ TS_TIME_COUNT ];
	times[ 0 ] = m_nTimes[ 0 ].GetSeconds();
	times[ 1 ] = m_nTimes[ 1 ].GetSeconds();
	times[ 2 ] = m_nTimes[ 2 ].GetSeconds();
	times[ 3 ] = m_nTimes[ 3 ].GetSeconds();

	float dt1, dt2;
	dt1 = times[ 1 ] - times[ 0 ];
	dt2 = times[ 3 ] - times[ 2 ];

	if ( dt1 > 0.0f && t >= times[ 0 ] && t < times[ 1 ] )
	{
		float f = ( t - times[ 0 ] ) / dt1;

		Vector out;

		Interpolator_CurveInterpolate
		( 
			m_nFalloffInterpolatorTypes[ 0 ], 
				s_pInterolationPoints[ 0 ], // unused
				s_pInterolationPoints[ 1 ], 
				s_pInterolationPoints[ 2 ], 
				s_pInterolationPoints[ 3 ], // unused
			f, 
			out 
		);
		return clamp( out.y, minfrac, 1.0f );
	}
	
	if ( t >= times[ 1 ] && t <= times[ 2 ] )
		return 1.0f;

	if ( dt2 > 0.0f &&	t > times[ 2 ] && t <= times[ 3 ] )
	{
		float f = ( times[ 3 ] - t ) / dt2;

		Vector out;

		Interpolator_CurveInterpolate
		( 
			m_nFalloffInterpolatorTypes[ 1 ], 
				s_pInterolationPoints[ 0 ], // unused
				s_pInterolationPoints[ 1 ], 
				s_pInterolationPoints[ 2 ], 
				s_pInterolationPoints[ 3 ], // unused
			f, 
			out 
		);
		return clamp( out.y, minfrac, 1.0f );
	}

	return minfrac;
}

// catch-all for non-interpolable types - just holds first value
template < class T >
T Average( const T *pValues, int nValues)
{
	if ( IsInterpolableType( CDmAttributeInfo< T >::AttributeType() ) )
	{
		static bool first = true;
		if ( first )
		{
			first = false;
			Warning( "CDmeLog: interpolable type %s doesn't have an averaging function!", CDmAttributeInfo< T >::AttributeTypeName() );
		}
	}

	Assert( nValues > 0 );
	if ( nValues <= 0 )
		return T(); // uninitialized for most value classes!!!

	return pValues[ 0 ];
}

// float version
template <>
float Average( const float *pValues, int nValues )
{
	Assert( nValues > 0 );
	if ( nValues <= 0 )
		return 0.0f;

	float sum = 0.0f;
	for ( int i = 0; i < nValues; ++i )
	{
		sum += pValues[ i ];
	}
	return sum / nValues;
}

// Color version
template <>
Color Average( const Color *pValues, int nValues )
{
	Assert( nValues > 0 );
	if ( nValues <= 0 )
		return Color( 0, 0, 0, 0 );

	float r = 0.0f, g = 0.0f, b = 0.0f, a = 0.0f;
	for ( int i = 0; i < nValues; ++i )
	{
		r += pValues[ i ].r();
		g += pValues[ i ].g();
		b += pValues[ i ].b();
		a += pValues[ i ].a();
	}
	float inv = nValues;
	return Color( r * inv, g * inv, b * inv, a * inv );
}

// Vector2 version
template <>
Vector2D Average( const Vector2D *pValues, int nValues )
{
	Assert( nValues > 0 );
	if ( nValues <= 0 )
		return Vector2D( 0.0f, 0.0f );

	Vector2D sum( 0.0f, 0.0f );
	for ( int i = 0; i < nValues; ++i )
	{
		sum += pValues[ i ];
	}
	return sum / nValues;
}

// Vector3 version
template <>
Vector Average( const Vector *pValues, int nValues )
{
	Assert( nValues > 0 );
	if ( nValues <= 0 )
		return Vector( 0.0f, 0.0f, 0.0f );

	Vector sum( 0.0f, 0.0f, 0.0f );
	for ( int i = 0; i < nValues; ++i )
	{
		sum += pValues[ i ];
	}
	return sum / nValues;
}

// QAngle version
template <>
QAngle Average( const QAngle *pValues, int nValues )
{
	Assert( nValues > 0 );
	if ( nValues <= 0 )
		return QAngle( 0.0f, 0.0f, 0.0f );

	Quaternion ave;
	AngleQuaternion( pValues[ 0 ], ave );

	// this is calculating the average by slerping with decreasing weights
	// for example: ave = 1/3 * q2 + 2/3 ( 1/2 * q1 + 1/2 * q0 )
	for ( int i = 1; i < nValues; ++i )
	{
		Quaternion quat;
		AngleQuaternion( pValues[ i ], quat );
		QuaternionSlerp( ave, quat, 1 / float( i + 1 ), ave );
	}

	QAngle qangle;
	QuaternionAngles( ave, qangle );
	return qangle;
}

// Quaternion version
template <>
Quaternion Average( const Quaternion *pValues, int nValues )
{
	Assert( nValues > 0 );
	if ( nValues <= 0 )
		return Quaternion( 0.0f, 0.0f, 0.0f, 1.0f );

	Quaternion ave = pValues[ 0 ];

	// this is calculating the average by slerping with decreasing weights
	// for example: ave = 1/3 * q2 + 2/3 ( 1/2 * q1 + 1/2 * q0 )
	for ( int i = 1; i < nValues; ++i )
	{
		QuaternionSlerp( ave, pValues[ i ], 1 / float( i + 1 ), ave );
	}

	return ave;
}



//-----------------------------------------------------------------------------
// per-type interpolation methods
//-----------------------------------------------------------------------------

// catch-all for non-interpolable types - just holds first value
template < class T >
T Interpolate( float t, const T& ti, const T& tj )
{
	if ( IsInterpolableType( CDmAttributeInfo< T >::AttributeType() ) )
	{
		static bool first = true;
		if ( first )
		{
			first = false;
			Warning( "CDmeLog: interpolable type %s doesn't have an interpolation function!", CDmAttributeInfo< T >::AttributeTypeName() );
		}
	}

	return ti;
}

// float version
template <>
float Interpolate( float t, const float& ti, const float& tj )
{
	return t * tj + (1.0f - t) * ti;
}

// Color version
template <>
Color Interpolate( float t, const Color& ti, const Color& tj )
{
	int ri, gi, bi, ai;
	int rj, gj, bj, aj;

	ti.GetColor( ri, gi, bi, ai );
	tj.GetColor( rj, gj, bj, aj );

	return Color( t * rj + (1.0f - t) * ri,
				  t * gj + (1.0f - t) * gi,
				  t * bj + (1.0f - t) * bi,
				  t * aj + (1.0f - t) * ai);
}

// Vector2 version
template <>
Vector2D Interpolate( float t, const Vector2D& ti, const Vector2D& tj )
{
	return t * tj + (1.0f - t) * ti;
}

// Vector3 version
template <>
Vector Interpolate( float t, const Vector& ti, const Vector& tj )
{
	return t * tj + (1.0f - t) * ti;
}

// QAngle version
template <>
QAngle Interpolate( float t, const QAngle& ti, const QAngle& tj )
{
	QAngle qaResult;
	Quaternion q, qi, qj;	// Some Quaternion temps for doing the slerp

	AngleQuaternion( ti, qi );			// Convert QAngles to Quaternions
	AngleQuaternion( tj, qj );
	QuaternionSlerp( qi, qj, t, q );	// Do a slerp as Quaternions
	QuaternionAngles( q, qaResult );	// Convert back to QAngles
	return qaResult;
}

// Quaternion version
template <>
Quaternion Interpolate( float t, const Quaternion& ti, const Quaternion& tj )
{
	static Quaternion s_value;
	QuaternionSlerp( ti, tj, t, s_value );
	return s_value;
}

// catch-all for non-interpolable types - just holds first value
template < class T >
T Curve_Interpolate( float t, DmeTime_t times[ 4 ], const T values[ 4 ], int curveTypes[ 4 ], float fmin, float fmax )
{
	if ( IsInterpolableType( CDmAttributeInfo< T >::AttributeType() ) )
	{
		static bool first = true;
		if ( first )
		{
			first = false;
			Warning( "CDmeLog: interpolable type %s doesn't have an interpolation function!", CDmAttributeInfo< T >::AttributeTypeName() );
		}
	}

	return t;
}

// float version
template <>
float Curve_Interpolate( float t, DmeTime_t times[ 4 ], const float values[ 4 ], int curveTypes[ 4 ], float fmin, float fmax )
{
	Vector args[ 4 ];
	for ( int i = 0; i < 4; ++i )
	{
		args[ i ].Init( times[ i ].GetSeconds(), values[ i ], 0.0f );
	}

	Vector vOut;
	int dummy;
	int earlypart, laterpart;

	// Not holding out value of previous curve...
	Interpolator_CurveInterpolatorsForType( curveTypes[ 1 ], dummy, earlypart );
	Interpolator_CurveInterpolatorsForType( curveTypes[ 2 ], laterpart, dummy );

	if ( earlypart == INTERPOLATE_HOLD )
	{
		// Hold "out" of previous sample (can cause a discontinuity)
		VectorLerp( args[ 1 ], args[ 2 ], t, vOut );
		vOut.y = args[ 1 ].y;
	}
	else if ( laterpart == INTERPOLATE_HOLD )
	{
		// Hold "out" of previous sample (can cause a discontinuity)
		VectorLerp( args[ 1 ], args[ 2 ], t, vOut );
		vOut.y = args[ 2 ].y;
	}
	else
	{
		bool sameCurveType = earlypart == laterpart ? true : false;
		if ( sameCurveType )
		{
			Interpolator_CurveInterpolate( laterpart, args[ 0 ], args[ 1 ], args[ 2 ], args[ 3 ], t, vOut );
		}
		else // curves differ, sigh
		{
			Vector vOut1, vOut2;

			Interpolator_CurveInterpolate( earlypart, args[ 0 ], args[ 1 ], args[ 2 ], args[ 3 ], t, vOut1 );
			Interpolator_CurveInterpolate( laterpart, args[ 0 ], args[ 1 ], args[ 2 ], args[ 3 ], t, vOut2 );

			VectorLerp( vOut1, vOut2, t, vOut );
		}
	}

	// FIXME:  This means we can only work with curves that range from 0.0 to 1.0f!!!
	float retval = clamp( vOut.y, fmin, fmax );
	return retval;
}

// Vector version
template <>
Vector Curve_Interpolate( float t, DmeTime_t times[ 4 ], const Vector values[ 4 ], int curveTypes[ 4 ], float fmin, float fmax )
{
	Vector vOut;
	int dummy;
	int earlypart, laterpart;

	// Not holding out value of previous curve...
	Interpolator_CurveInterpolatorsForType( curveTypes[ 1 ], dummy, earlypart );
	Interpolator_CurveInterpolatorsForType( curveTypes[ 2 ], laterpart, dummy );

	if ( earlypart == INTERPOLATE_HOLD )
	{
		// Hold "out" of previous sample (can cause a discontinuity)
		vOut = values[ 1 ];
	}
	else if ( laterpart == INTERPOLATE_HOLD )
	{
		// Hold "out" of previous sample (can cause a discontinuity)
		vOut = values[ 2 ];
	}
	else
	{
		bool sameCurveType = earlypart == laterpart;
		if ( sameCurveType )
		{
			Interpolator_CurveInterpolate_NonNormalized( laterpart, values[ 0 ], values[ 1 ], values[ 2 ], values[ 3 ], t, vOut );
		}
		else // curves differ, sigh
		{
			Vector vOut1, vOut2;

			Interpolator_CurveInterpolate_NonNormalized( earlypart, values[ 0 ], values[ 1 ], values[ 2 ], values[ 3 ], t, vOut1 );
			Interpolator_CurveInterpolate_NonNormalized( laterpart, values[ 0 ], values[ 1 ], values[ 2 ], values[ 3 ], t, vOut2 );

			VectorLerp( vOut1, vOut2, t, vOut );
		}
	}

	return vOut;
}

// Quaternion version
template <>
Quaternion Curve_Interpolate( float t, DmeTime_t times[ 4 ], const Quaternion values[ 4 ], int curveTypes[ 4 ], float fmin, float fmax )
{
	Quaternion vOut;
	int dummy;
	int earlypart, laterpart;

	// Not holding out value of previous curve...
	Interpolator_CurveInterpolatorsForType( curveTypes[ 1 ], dummy, earlypart );
	Interpolator_CurveInterpolatorsForType( curveTypes[ 2 ], laterpart, dummy );

	if ( earlypart == INTERPOLATE_HOLD )
	{
		// Hold "out" of previous sample (can cause a discontinuity)
		vOut = values[ 1 ];
	}
	else if ( laterpart == INTERPOLATE_HOLD )
	{
		// Hold "out" of previous sample (can cause a discontinuity)
		vOut = values[ 2 ];
	}
	else
	{
		bool sameCurveType = ( earlypart == laterpart ) ? true : false;
		if ( sameCurveType )
		{
			Interpolator_CurveInterpolate_NonNormalized( laterpart, values[ 0 ], values[ 1 ], values[ 2 ], values[ 3 ], t, vOut );
		}
		else // curves differ, sigh
		{
			Quaternion vOut1, vOut2;

			Interpolator_CurveInterpolate_NonNormalized( earlypart, values[ 0 ], values[ 1 ], values[ 2 ], values[ 3 ], t, vOut1 );
			Interpolator_CurveInterpolate_NonNormalized( laterpart, values[ 0 ], values[ 1 ], values[ 2 ], values[ 3 ], t, vOut2 );

			QuaternionSlerp( vOut1, vOut2, t, vOut );
		}
	}

	return vOut;
}


template< class T >
T ScaleValue( const T& value, float scale )
{
	return value * scale;
}
template<>
bool ScaleValue( const bool& value, float scale )
{
	Assert( 0 );
	return value;
}
template<>
Color ScaleValue( const Color& value, float scale )
{
	Assert( 0 );
	return value;
}
template<>
Vector4D ScaleValue( const Vector4D& value, float scale )
{
	return Vector4D( value.x * scale, value.y * scale, value.z * scale, value.w * scale );
}

template<>
Quaternion ScaleValue( const Quaternion& value, float scale )
{
	return Quaternion( value.x * scale, value.y * scale, value.z * scale, value.w * scale );
}

template<>
VMatrix ScaleValue( const VMatrix& value, float scale )
{
	Assert( 0 );
	return value;
}

template<>
CUtlString ScaleValue( const CUtlString& value, float scale )
{
	Assert( 0 );
	return value;
}

template< class T >
float LengthOf( const T& value )
{
	return value;
}

template<>
float LengthOf( const bool& value )
{
	if ( value )
		return 1.0f;
	return 0.0f;
}
template<>
float LengthOf( const Color& value )
{
	return (float)sqrt( (float)( value.r() * value.r() +
		value.g() * value.g() +
		value.b() * value.b() +
		value.a() * value.a()) );
}
template<>
float LengthOf( const Vector4D& value )
{
	return sqrt( value.x * value.x +
		value.y * value.y +
		value.z * value.z +
		value.w * value.w );
}

template<>
float LengthOf( const Quaternion& value )
{
	return sqrt( value.x * value.x +
		value.y * value.y +
		value.z * value.z +
		value.w * value.w );
}

template<>
float LengthOf( const VMatrix& value )
{
	return 0.0f;
}

template<>
float LengthOf( const CUtlString& value )
{
	return 0.0f;
}

template<>
float LengthOf( const Vector2D& value )
{
	return value.Length();
}

template<>
float LengthOf( const Vector& value )
{
	return value.Length();
}

template<>
float LengthOf( const QAngle& value )
{
	return value.Length();
}

template< class T >
T Subtract( const T& v1, const T& v2 )
{
	return v1 - v2;
}

template<>
bool Subtract( const bool& v1, const bool& v2 )
{
	return v1;
}

template<>
CUtlString Subtract( const CUtlString& v1, const CUtlString& v2 )
{
	return v1;
}

template<>
Color Subtract( const Color& v1, const Color& v2 )
{
	Color ret;
	for ( int i = 0; i < 4; ++i )
	{
		ret[ i ] = clamp( v1[ i ] - v2[ i ], 0, 255 );
	}
	return ret;
}

template<>
Vector4D Subtract( const Vector4D& v1, const Vector4D& v2 )
{
	Vector4D ret;
	for ( int i = 0; i < 4; ++i )
	{
		ret[ i ] = v1[ i ] - v2[ i ];
	}
	return ret;
}

template<>
Quaternion Subtract( const Quaternion& v1, const Quaternion& v2 )
{
	Quaternion ret;
	for ( int i = 0; i < 4; ++i )
	{
		ret[ i ] = v1[ i ];
	}
	return ret;
}

template< class T >
T Add( const T& v1, const T& v2 )
{
	return v1 + v2;
}

template<>
bool Add( const bool& v1, const bool& v2 )
{
	return v1;
}

template<>
CUtlString Add( const CUtlString& v1, const CUtlString& v2 )
{
	return v1;
}


template<>
Color Add( const Color& v1, const Color& v2 )
{
	Color ret;
	for ( int i = 0; i < 4; ++i )
	{
		ret[ i ] = clamp( v1[ i ] + v2[ i ], 0, 255 );
	}
	return ret;
}

template<>
Vector4D Add( const Vector4D& v1, const Vector4D& v2 )
{
	Vector4D ret;
	for ( int i = 0; i < 4; ++i )
	{
		ret[ i ] = v1[ i ] + v2[ i ];
	}
	return ret;
}

template<>
Quaternion Add( const Quaternion& v1, const Quaternion& v2 )
{
	return v1;
}

IMPLEMENT_ABSTRACT_ELEMENT( DmeLogLayer,			CDmeLogLayer );

IMPLEMENT_ELEMENT_FACTORY( DmeIntLogLayer,			CDmeIntLogLayer );
IMPLEMENT_ELEMENT_FACTORY( DmeFloatLogLayer,		CDmeFloatLogLayer );
IMPLEMENT_ELEMENT_FACTORY( DmeBoolLogLayer,			CDmeBoolLogLayer );
IMPLEMENT_ELEMENT_FACTORY( DmeColorLogLayer,		CDmeColorLogLayer );
IMPLEMENT_ELEMENT_FACTORY( DmeVector2LogLayer,		CDmeVector2LogLayer );
IMPLEMENT_ELEMENT_FACTORY( DmeVector3LogLayer,		CDmeVector3LogLayer );
IMPLEMENT_ELEMENT_FACTORY( DmeVector4LogLayer,		CDmeVector4LogLayer );
IMPLEMENT_ELEMENT_FACTORY( DmeQAngleLogLayer,		CDmeQAngleLogLayer );
IMPLEMENT_ELEMENT_FACTORY( DmeQuaternionLogLayer,	CDmeQuaternionLogLayer );
IMPLEMENT_ELEMENT_FACTORY( DmeVMatrixLogLayer,		CDmeVMatrixLogLayer );
IMPLEMENT_ELEMENT_FACTORY( DmeStringLogLayer,		CDmeStringLogLayer );

//-----------------------------------------------------------------------------
// explicit template instantiation
//-----------------------------------------------------------------------------
template class CDmeTypedLogLayer<int>;
template class CDmeTypedLogLayer<float>;
template class CDmeTypedLogLayer<bool>;
template class CDmeTypedLogLayer<Color>;
template class CDmeTypedLogLayer<Vector2D>;
template class CDmeTypedLogLayer<Vector>;
template class CDmeTypedLogLayer<Vector4D>;
template class CDmeTypedLogLayer<QAngle>;
template class CDmeTypedLogLayer<Quaternion>;
template class CDmeTypedLogLayer<VMatrix>;
template class CDmeTypedLogLayer<CUtlString>;


IMPLEMENT_ABSTRACT_ELEMENT( DmeCurveInfo,			CDmeCurveInfo );

IMPLEMENT_ELEMENT_FACTORY( DmeIntCurveInfo,			CDmeIntCurveInfo );
IMPLEMENT_ELEMENT_FACTORY( DmeFloatCurveInfo,		CDmeFloatCurveInfo );
IMPLEMENT_ELEMENT_FACTORY( DmeBoolCurveInfo,		CDmeBoolCurveInfo );
IMPLEMENT_ELEMENT_FACTORY( DmeColorCurveInfo,		CDmeColorCurveInfo );
IMPLEMENT_ELEMENT_FACTORY( DmeVector2CurveInfo,		CDmeVector2CurveInfo );
IMPLEMENT_ELEMENT_FACTORY( DmeVector3CurveInfo,		CDmeVector3CurveInfo );
IMPLEMENT_ELEMENT_FACTORY( DmeVector4CurveInfo,		CDmeVector4CurveInfo );
IMPLEMENT_ELEMENT_FACTORY( DmeQAngleCurveInfo,		CDmeQAngleCurveInfo );
IMPLEMENT_ELEMENT_FACTORY( DmeQuaternionCurveInfo,	CDmeQuaternionCurveInfo );
IMPLEMENT_ELEMENT_FACTORY( DmeVMatrixCurveInfo,		CDmeVMatrixCurveInfo );
IMPLEMENT_ELEMENT_FACTORY( DmeStringCurveInfo,		CDmeStringCurveInfo );

//-----------------------------------------------------------------------------
// explicit template instantiation
//-----------------------------------------------------------------------------
template class CDmeTypedCurveInfo<int>;
template class CDmeTypedCurveInfo<float>;
template class CDmeTypedCurveInfo<bool>;
template class CDmeTypedCurveInfo<Color>;
template class CDmeTypedCurveInfo<Vector2D>;
template class CDmeTypedCurveInfo<Vector>;
template class CDmeTypedCurveInfo<Vector4D>;
template class CDmeTypedCurveInfo<QAngle>;
template class CDmeTypedCurveInfo<Quaternion>;
template class CDmeTypedCurveInfo<VMatrix>;
template class CDmeTypedCurveInfo<CUtlString>;


//-----------------------------------------------------------------------------
// Class factory 
//-----------------------------------------------------------------------------
IMPLEMENT_ABSTRACT_ELEMENT( DmeLog,				CDmeLog );

IMPLEMENT_ELEMENT_FACTORY( DmeIntLog,			CDmeIntLog );
IMPLEMENT_ELEMENT_FACTORY( DmeFloatLog,			CDmeFloatLog );
IMPLEMENT_ELEMENT_FACTORY( DmeBoolLog,			CDmeBoolLog );
IMPLEMENT_ELEMENT_FACTORY( DmeColorLog,			CDmeColorLog );
IMPLEMENT_ELEMENT_FACTORY( DmeVector2Log,		CDmeVector2Log );
IMPLEMENT_ELEMENT_FACTORY( DmeVector3Log,		CDmeVector3Log );
IMPLEMENT_ELEMENT_FACTORY( DmeVector4Log,		CDmeVector4Log );
IMPLEMENT_ELEMENT_FACTORY( DmeQAngleLog,		CDmeQAngleLog );
IMPLEMENT_ELEMENT_FACTORY( DmeQuaternionLog,	CDmeQuaternionLog );
IMPLEMENT_ELEMENT_FACTORY( DmeVMatrixLog,		CDmeVMatrixLog );
IMPLEMENT_ELEMENT_FACTORY( DmeStringLog,		CDmeStringLog );


//-----------------------------------------------------------------------------
// explicit template instantiation
//-----------------------------------------------------------------------------
template class CDmeTypedLog<int>;
template class CDmeTypedLog<float>;
template class CDmeTypedLog<bool>;
template class CDmeTypedLog<Color>;
template class CDmeTypedLog<Vector2D>;
template class CDmeTypedLog<Vector>;
template class CDmeTypedLog<Vector4D>;
template class CDmeTypedLog<QAngle>;
template class CDmeTypedLog<Quaternion>;
template class CDmeTypedLog<VMatrix>;
template class CDmeTypedLog<CUtlString>;


//-----------------------------------------------------------------------------
// instantiate and initialize static vars
//-----------------------------------------------------------------------------
float CDmeIntLog::s_defaultThreshold = 0.0f;
float CDmeFloatLog::s_defaultThreshold = 0.0f;
float CDmeBoolLog::s_defaultThreshold = 0.0f;
float CDmeColorLog::s_defaultThreshold = 0.0f;
float CDmeVector2Log::s_defaultThreshold = 0.0f;
float CDmeVector3Log::s_defaultThreshold = 0.0f;
float CDmeVector4Log::s_defaultThreshold = 0.0f;
float CDmeQAngleLog::s_defaultThreshold = 0.0f;
float CDmeQuaternionLog::s_defaultThreshold = 0.0f;
float CDmeVMatrixLog::s_defaultThreshold = 0.0f;
float CDmeStringLog::s_defaultThreshold = 0.0f;


void CDmeLogLayer::OnConstruction()
{
	m_pOwnerLog = NULL;
	m_lastKey = 0;
	m_times.Init( this, "times" );
	m_CurveTypes.Init( this, "curvetypes" );
}

void CDmeLogLayer::OnDestruction()
{
}

CDmeLog *CDmeLogLayer::GetOwnerLog()
{
	return m_pOwnerLog;
}

const CDmeLog *CDmeLogLayer::GetOwnerLog() const
{
	return m_pOwnerLog;
}

DmeTime_t CDmeLogLayer::GetBeginTime() const
{
	if ( m_times.Count() == 0 )
		return DmeTime_t::MinTime();

	return DmeTime_t( m_times[ 0 ] );
}

DmeTime_t CDmeLogLayer::GetEndTime() const
{
	uint tn = m_times.Count();
	if ( tn == 0 )
		return DmeTime_t::MaxTime();

	return DmeTime_t( m_times[ tn - 1 ] );
}


// Validates that all keys are correctly sorted in time
bool CDmeLogLayer::ValidateKeys() const
{
	int nCount = m_times.Count();
	for ( int i = 1; i < nCount; ++i )
	{
		if ( m_times[i] <= m_times[i-1] )
		{
			Warning( "Error in log %s! Key times are out of order [keys %d->%d: %d->%d]!\n",
				GetName(), i-1, i, m_times[i-1], m_times[i] );
			return false;
		}
	}
	return true;
}

int CDmeLogLayer::FindKey( DmeTime_t time ) const
{
	int tn = m_times.Count();
	if ( m_lastKey >= 0 && m_lastKey < tn )
	{
		if ( time >= DmeTime_t( m_times[ m_lastKey ] ) )
		{
			// common case - playing forward
			for ( ; m_lastKey < tn - 1; ++m_lastKey )
			{
				if ( time < DmeTime_t( m_times[ m_lastKey + 1 ] ) )
					return m_lastKey;
			}

			// if time past the end, return the last key
			return m_lastKey;
		}
		else
		{
			tn = m_lastKey;
		}
	}

	for ( int ti = tn - 1; ti >= 0; --ti )
	{
		if ( time >= DmeTime_t(  m_times[ ti ] ) )
		{
			m_lastKey = ti;
			return ti;
		}
	}

	return -1;
}


//-----------------------------------------------------------------------------
// Returns the number of keys
//-----------------------------------------------------------------------------
int CDmeLogLayer::GetKeyCount() const
{
	return m_times.Count();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : nKeyIndex - 
//			keyTime - 
//-----------------------------------------------------------------------------
void CDmeLogLayer::SetKeyTime( int nKeyIndex, DmeTime_t keyTime )
{
	m_times.Set( nKeyIndex, keyTime.GetTenthsOfMS() );
}

//-----------------------------------------------------------------------------
// Returns a specific key's value
//-----------------------------------------------------------------------------
DmeTime_t CDmeLogLayer::GetKeyTime( int nKeyIndex ) const
{
	return DmeTime_t( m_times[ nKeyIndex ] );
}


//-----------------------------------------------------------------------------
// Scale + bias key times
//-----------------------------------------------------------------------------
void CDmeLogLayer::ScaleBiasKeyTimes( double flScale, DmeTime_t nBias )
{
	// Don't waste time on the identity transform
	if ( ( nBias == DMETIME_ZERO ) && ( fabs( flScale - 1.0 ) < 1e-5 ) )
		return;

	int nCount = GetKeyCount();
	for ( int i = 0; i < nCount; ++i )
	{
		DmeTime_t t = GetKeyTime( i );
		t.SetSeconds( t.GetSeconds() * flScale );
		t += nBias;
		SetKeyTime( i, t );
	}
}


//-----------------------------------------------------------------------------
// Returns the index of a particular key
//-----------------------------------------------------------------------------
int CDmeLogLayer::FindKeyWithinTolerance( DmeTime_t nTime, DmeTime_t nTolerance )
{
	int nClosest = -1;
	DmeTime_t nClosestTolerance = DmeTime_t::MaxTime();
	DmeTime_t nCurrTolerance;
	int start = 0, end = GetKeyCount() - 1;
	while ( start <= end )
	{
		int mid = (start + end) >> 1;
		DmeTime_t nDelta = nTime - DmeTime_t( m_times[mid] );
		if ( nDelta > DmeTime_t( 0 ) )
		{
			nCurrTolerance = nDelta;
			start = mid + 1;
		}
		else if ( nDelta < DmeTime_t( 0 ) )
		{
			nCurrTolerance = -nDelta;
			end = mid - 1;
		}
		else
		{
			return mid;
		}

		if ( nCurrTolerance < nClosestTolerance )
		{
			nClosest = mid;
			nClosestTolerance = nCurrTolerance;
		}
	}
	if ( nClosestTolerance > nTolerance )
		return -1;
	return nClosest;
}

void CDmeLogLayer::OnUsingCurveTypesChanged()
{
	if ( g_pDataModel->IsUnserializing() )
		return;

	if ( !IsUsingCurveTypes() )
	{
		m_CurveTypes.RemoveAll();
	}
	else
	{
		m_CurveTypes.RemoveAll();
		// Fill in an array with the default curve type for
		int c = m_times.Count();
		for ( int i = 0; i < c; ++i )
		{
			m_CurveTypes.AddToTail( GetDefaultCurveType() );
		}
	}
}

bool CDmeLogLayer::IsUsingCurveTypes() const
{
	return GetOwnerLog() ? GetOwnerLog()->IsUsingCurveTypes() : false;
}

int CDmeLogLayer::GetDefaultCurveType() const
{
	return GetOwnerLog()->GetDefaultCurveType();
}

void CDmeLogLayer::SetKeyCurveType( int nKeyIndex, int curveType )
{
	Assert( GetOwnerLog() );
	if ( !GetOwnerLog() )
		return;

	Assert( GetOwnerLog()->IsUsingCurveTypes() );
	Assert( m_CurveTypes.IsValidIndex( nKeyIndex ) );
	if ( !m_CurveTypes.IsValidIndex( nKeyIndex ) )
		return;

	m_CurveTypes.Set( nKeyIndex, curveType );
}

int CDmeLogLayer::GetKeyCurveType( int nKeyIndex ) const
{
	Assert( GetOwnerLog() );
	if ( !GetOwnerLog() )
		return CURVE_DEFAULT;

	Assert( GetOwnerLog()->IsUsingCurveTypes() );
	Assert( m_CurveTypes.IsValidIndex( nKeyIndex ) );
	if ( !m_CurveTypes.IsValidIndex( nKeyIndex ) )
		return GetOwnerLog()->GetDefaultCurveType();

	return m_CurveTypes[ nKeyIndex ];
}


//-----------------------------------------------------------------------------
// Removes all keys outside the specified time range
//-----------------------------------------------------------------------------
void CDmeLogLayer::RemoveKeysOutsideRange( DmeTime_t tStart, DmeTime_t tEnd )
{
	int i;
	int nKeysToRemove = 0;
	int nKeyCount = m_times.Count();
	for ( i = 0; i < nKeyCount; ++i, ++nKeysToRemove )
	{
		if ( m_times[i] >= tStart.GetTenthsOfMS() )
			break;
	}

	if ( nKeysToRemove )
	{
		RemoveKey( 0, nKeysToRemove );
	}

	nKeyCount = m_times.Count();
	for ( i = 0; i < nKeyCount; ++i )
	{
		if ( m_times[i] > tEnd.GetTenthsOfMS() )
			break;
	}
	nKeysToRemove = nKeyCount - i;
	if ( nKeysToRemove )
	{
		RemoveKey( i, nKeysToRemove );
	}
}


template < class T >
class CUndoLayerAdded : public CUndoElement
{
	typedef CUndoElement BaseClass;

public:
	CUndoLayerAdded( const char *desc, CDmeLog *pLog ) :
		BaseClass( desc ),
		m_bNeedsCleanup( false ),
		m_hLog( pLog )
	{
		Assert( pLog && pLog->GetFileId() != DMFILEID_INVALID );
	}

	virtual ~CUndoLayerAdded()
	{
		if ( m_bNeedsCleanup )
		{
			g_pDataModel->DestroyElement( m_hLayer );
		}
	}

	virtual void Undo()
	{
		m_bNeedsCleanup = true;
		m_hLayer = m_hLog->RemoveLayerFromTail()->GetHandle();
		g_pDataModel->MarkHandleInvalid( m_hLayer );
	}

	virtual void Redo()
	{
		m_bNeedsCleanup = false;
		g_pDataModel->MarkHandleValid( m_hLayer );
		m_hLog->AddLayerToTail( GetElement< CDmeTypedLogLayer< T > >( m_hLayer ) );
	}

	virtual const char	*GetDesc()
	{
		static char sz[ 512 ];
		int iLayer = m_hLog->GetTopmostLayer();
		if ( iLayer >= 0 )
		{
			CDmeLogLayer *layer = m_hLog->GetLayer( iLayer );
			Q_snprintf( sz, sizeof( sz ), "addlayer: log %p lc[%d], layer %p",
				m_hLog.Get(), m_hLog->GetNumLayers(), layer );
		}
		else
		{
			Q_snprintf( sz, sizeof( sz ), "addlayer: log %p lc[%d], layer NULL",
				m_hLog.Get(), m_hLog->GetNumLayers() );
		}
		return sz;
	}

private:
	CDmeHandle< CDmeLog >	m_hLog;
	bool					m_bNeedsCleanup;
	CDmeCountedHandle		m_hLayer;
};

template < class T >
class CUndoFlattenLayers : public CUndoElement
{
	typedef CUndoElement BaseClass;

public:

	CUndoFlattenLayers( const char *desc, CDmeTypedLog< T > *pLog, float threshold, int flags ) :
		BaseClass( desc ),
		m_bNeedsCleanup( true ),
		m_hLog( pLog ),
		m_nFlags( flags ),
		m_flThreshold( threshold )
	{
		Assert( pLog && pLog->GetFileId() != DMFILEID_INVALID );
		LatchCurrentLayers();
	}

	virtual ~CUndoFlattenLayers()
	{
		if ( m_bNeedsCleanup )
		{
			for ( int i = 0; i < m_hLayers.Count(); ++i )
			{
				m_hLayers[ i ] = DMELEMENT_HANDLE_INVALID;
#ifdef _DEBUG
				CDmElement *pElement = g_pDataModel->GetElement( m_hLayers[ i ] );
				Assert( !pElement || pElement->IsStronglyReferenced() );
#endif
			}
		}
	}

	virtual void Undo()
	{
		m_bNeedsCleanup = false;

		for ( int i = 0; i < m_hLayers.Count(); ++i )
		{
			if ( i == 0 )
			{
				// Copy base layer in place so handles to the base layer remain valid
				CDmeTypedLogLayer< T > *base = m_hLog->GetLayer( i );
				base->CopyLayer( GetElement< CDmeTypedLogLayer< T > >( m_hLayers[ i ] ) );
				// Release it since we didn't txfer it over
				g_pDataModel->DestroyElement( m_hLayers[ i ] );
			}
			else
			{
				// This transfers ownership, so no Release needed
				m_hLog->AddLayerToTail( GetElement< CDmeTypedLogLayer< T > >( m_hLayers[ i ] ) );
			}
		}

		m_hLayers.RemoveAll();
	}

	virtual void Redo()
	{
		m_bNeedsCleanup = true;
		Assert( m_hLayers.Count() == 0 );

		LatchCurrentLayers();
		// Flatten them again (won't create undo records since we're in undo already)

		m_hLog->FlattenLayers( m_flThreshold, m_nFlags );
	}

	virtual const char	*GetDesc()
	{
		static char sz[ 512 ];
		Q_snprintf( sz, sizeof( sz ), "flatten log %p lc[%d]",
			m_hLog.Get(), m_hLayers.Count() );
		return sz;
	}

private:

	void LatchCurrentLayers()
	{
		CDisableUndoScopeGuard guard;
		Assert( m_hLayers.Count() == 0 );
		Assert( m_hLog->GetNumLayers() >= 1 );

		// Entry 0 is the original "base" layer
		for ( int i = 0; i < m_hLog->GetNumLayers(); ++i )
		{
			CDmeTypedLogLayer< T > *pLayer = CastElement< CDmeTypedLogLayer< T > >( CreateLayer< T >( m_hLog ) );
			pLayer->CopyLayer( m_hLog->GetLayer( i ) );
			m_hLayers.AddToTail( pLayer->GetHandle() );
		}
	}

	CDmeHandle< CDmeTypedLog< T > >	m_hLog;
	bool							m_bNeedsCleanup;
	CUtlVector< CDmeCountedHandle >	m_hLayers;
	int								m_nFlags;
	float							m_flThreshold;
};

//-----------------------------------------------------------------------------
// CDmeTypedLogLayer - a generic typed layer used by a log
//-----------------------------------------------------------------------------
template< class T >
void CDmeTypedLogLayer< T >::OnConstruction()
{
//	m_times.Init( this, "times" );
//	m_CurveTypes.Init( this, "curvetypes" );
	m_values.Init( this, "values" );
}

template< class T >
void CDmeTypedLogLayer< T >::SetOwnerLog( CDmeLog *owner )
{
	Assert( owner );
	Assert( assert_cast< CDmeTypedLog< T > * >( owner ) );
	m_pOwnerLog = owner;
}

template< class T >
CDmeTypedLog< T > *CDmeTypedLogLayer< T >::GetTypedOwnerLog()
{
	return assert_cast< CDmeTypedLog< T > * >( m_pOwnerLog );
}

template< class T >
const CDmeTypedLog< T >	*CDmeTypedLogLayer< T >::GetTypedOwnerLog() const
{
	return assert_cast< CDmeTypedLog< T > * >( m_pOwnerLog );
}

template< class T >
void CDmeTypedLogLayer< T >::OnDestruction()
{
}

template< class T >
void CDmeTypedLogLayer< T >::RemoveKeys( DmeTime_t starttime )
{
	int ti = FindKey( starttime );
	if ( ti < 0 )
		return;

	if ( starttime > DmeTime_t( m_times[ ti ] ) )
		++ti;

	int nKeys = m_times.Count() - ti;
	if ( nKeys == 0 )
		return;

	m_times.RemoveMultiple( ti, nKeys );
	m_values.RemoveMultiple( ti, nKeys );
	if ( IsUsingCurveTypes() )
	{
		m_CurveTypes.RemoveMultiple( ti, nKeys );
	}

	if ( m_lastKey >= ti && m_lastKey < ti + nKeys )
	{
		m_lastKey = ( ti > 0 ) ? ti - 1 : 0;
	}
}

template< class T >
void CDmeTypedLogLayer< T >::ClearKeys()
{
	m_times.RemoveAll();
	m_values.RemoveAll();
	m_CurveTypes.RemoveAll();
	m_lastKey = 0;
}

template< class T >
void CDmeTypedLogLayer< T >::RemoveKey( int nKeyIndex, int nNumKeysToRemove /*= 1*/ )
{
	m_times.RemoveMultiple( nKeyIndex, nNumKeysToRemove );
	m_values.RemoveMultiple( nKeyIndex, nNumKeysToRemove );
	if ( IsUsingCurveTypes() )
	{
		m_CurveTypes.RemoveMultiple( nKeyIndex, nNumKeysToRemove );
	}
}

//-----------------------------------------------------------------------------
// Sets a key, removes all keys after this time
// FIXME: This needs to account for interpolation!!!
//-----------------------------------------------------------------------------
template< class T >
void CDmeTypedLogLayer< T >::SetKey( DmeTime_t time, const T& value, int curveType /*=CURVE_DEFAULT*/)
{
	Assert( m_values.Count() == m_times.Count() );
	Assert( !IsUsingCurveTypes() || ( m_CurveTypes.Count() == m_times.Count() ) );

	// Remove all keys after this time
	RemoveKeys( time );

	// Add the key and then check to see if the penultimate key is still necessary
	m_times.AddToTail( time.GetTenthsOfMS() );
	m_values.AddToTail( value );
	if ( IsUsingCurveTypes() )
	{
		m_CurveTypes.AddToTail( curveType );
	}

	int nKeys = m_values.Count();
	if ( ( nKeys < 3 ) || 
		 ( IsUsingCurveTypes() && ( curveType != m_CurveTypes[ nKeys -1 ] || ( curveType != m_CurveTypes[ nKeys - 2 ] ) ) ) 
		 )
	{
		return;
	}

	// If adding the new means that the penultimate key's value was unneeded, then we will remove the penultimate key value
	T check = GetValueSkippingKey( nKeys - 2 );
	T oldPenultimateValue = m_values[ nKeys - 2 ];
	if ( GetTypedOwnerLog()->ValuesDiffer( oldPenultimateValue, check ) )
	{
		return;
	}
	
	// Remove penultimate, it's not needed
	m_times.Remove( nKeys - 2 );
	m_values.Remove( nKeys - 2 );
	if ( IsUsingCurveTypes() )
	{
		m_CurveTypes.Remove( nKeys - 2 );
	}
}


//-----------------------------------------------------------------------------
// Finds a key within tolerance, or adds one
//-----------------------------------------------------------------------------
template< class T >
int CDmeTypedLogLayer< T >::FindOrAddKey( DmeTime_t nTime, DmeTime_t nTolerance, const T& value, int curveType /*=CURVE_DEFAULT*/ )
{
	Assert( m_values.Count() == m_times.Count() );
	Assert( !IsUsingCurveTypes() || ( m_CurveTypes.Count() == m_times.Count() ) );

	// NOTE: This math must occur in 64bits because the max delta nDelta
	// can be 33 bits large. Bleah.
	int nClosest = -1;
	int64 nClosestTolerance = DmeTime_t::MinTime().GetTenthsOfMS();
	int64 nCurrTolerance;
	int start = 0, end = GetKeyCount() - 1;
	while ( start <= end )
	{
		int mid = (start + end) >> 1;
		int64 nDelta = (int64)nTime.GetTenthsOfMS() - (int64)m_times[mid];
		if ( nDelta > 0 )
		{
			nCurrTolerance = nDelta;
			start = mid + 1;
		}
		else if ( nDelta < 0 )
		{
			nCurrTolerance = -nDelta;
			end = mid - 1;
		}
		else
		{
			nClosest = end = mid;
			nClosestTolerance = 0;
			break;
		}

		if ( nCurrTolerance < nClosestTolerance )
		{
			nClosest = mid;
			nClosestTolerance = nCurrTolerance;
		}
	}

	// At this point, end is the entry less than or equal to the entry
	if ( nClosest == -1 || nTolerance.GetTenthsOfMS() < nClosestTolerance )
	{
		++end;
		nClosest = m_times.InsertBefore( end, nTime.GetTenthsOfMS() );
		m_values.InsertBefore( end, value );
		if ( IsUsingCurveTypes() )
		{
			m_CurveTypes.InsertBefore( end, curveType );
		}
	}
	return nClosest;
}


//-----------------------------------------------------------------------------
// This inserts a key. Unlike SetKey, this will *not* delete keys after the specified time
//-----------------------------------------------------------------------------
template < class T >
int CDmeTypedLogLayer< T >::InsertKey( DmeTime_t nTime, const T& value, int curveType /*=CURVE_DEFAULT*/ )
{
	int idx = FindOrAddKey( nTime, DmeTime_t( 0 ), value );
	m_times .Set( idx, nTime.GetTenthsOfMS() );
	m_values.Set( idx, value );
	if ( IsUsingCurveTypes() )
	{
		m_CurveTypes.Set( idx, curveType );
	}
	return idx;
}

template< class T >
int CDmeTypedLogLayer< T >::InsertKeyAtTime( DmeTime_t nTime, int curveType /*=CURVE_DEFAULT*/ )
{
	T curVal = GetValue( nTime );
	return InsertKey( nTime, curVal, curveType );
}

static bool CanInterpolateType( DmAttributeType_t attType )
{
	switch ( attType )
	{
	default:
		return false;
	case AT_FLOAT:
	case AT_VECTOR3:
	case AT_QUATERNION:
		break;
	}
	return true;
}

template< class T >
const T& CDmeTypedLogLayer< T >::GetValue( DmeTime_t time ) const
{
	// Curve Interpolation only for 1-D float data right now!!!
	if ( IsUsingCurveTypes() && 
		CanInterpolateType( GetDataType() ) )
	{
		static T out;
		GetValueUsingCurveInfo( time, out );
		return out;
	}

	int tc = m_times.Count();

	Assert( m_values.Count() == tc );
	Assert( !IsUsingCurveTypes() || ( m_CurveTypes.Count() == tc ) );

	int ti = FindKey( time );
	if ( ti < 0 )
	{
		if ( tc > 0 )
			return m_values[ 0 ];

		const CDmeTypedLog< T > *pOwner = GetTypedOwnerLog();
		if ( pOwner->HasDefaultValue() )
			return pOwner->GetDefaultValue();

		static T s_value;
		CDmAttributeInfo< T >::SetDefaultValue( s_value ); // TODO - create GetDefaultValue that returns a default T, to avoid rebuilding every time
		return s_value;
	}

	// Early out if we're at the end
	if ( ti >= tc - 1 )
		return m_values[ ti ];

	if ( !IsInterpolableType( GetDataType() ) )
		return m_values[ ti ];

	// Figure out the lerp factor
	float t = GetFractionOfTimeBetween( time, DmeTime_t( m_times[ti] ), DmeTime_t( m_times[ti+1] ) );
	static T s_value;
	s_value = Interpolate( t, m_values[ti], m_values[ti+1] );	// Compute the lerp between ti and ti+1
	return s_value;
}

template< class T >
void CDmeTypedLogLayer< T >::SetKey( DmeTime_t time, const CDmAttribute *pAttr, uint index, int curveType /*= CURVE_DEFAULT*/ )
{
	DmAttributeType_t type = pAttr->GetType();
	if ( IsValueType( type ) )
	{
		Assert( pAttr->GetType() == GetDataType() );
		SetKey( time, pAttr->GetValue< T >(), curveType );
	}
	else if ( IsArrayType( type ) )
	{
		Assert( ArrayTypeToValueType( type ) == GetDataType() );
		CDmrArrayConst<T> array( pAttr );
		SetKey( time, array[ index ], curveType );
	}
	else
	{
		Assert( 0 );
	}
}

template< class T >
bool CDmeTypedLogLayer< T >::SetDuplicateKeyAtTime( DmeTime_t time )
{
	int nKeys = m_times.Count();
	if ( nKeys == 0 || DmeTime_t( m_times[ nKeys - 1 ] ) == time )
		return false;

	T value = GetValue( time );
	// these two calls need to be separated (and we need to make an extra copy here) because
	// CUtlVector has an assert to try to safeguard against inserting an existing value
	// therefore, m_values.AddToTail( m_values[ i ] ) is illegal (or at least, triggers the assert)
	SetKey( time, value );
	return true;
}


//-----------------------------------------------------------------------------
// Returns a specific key's value
//-----------------------------------------------------------------------------
template< class T >
const T& CDmeTypedLogLayer< T >::GetKeyValue( int nKeyIndex ) const
{
	Assert( m_values.Count() == m_times.Count() );
	Assert( !IsUsingCurveTypes() || ( m_CurveTypes.Count() == m_times.Count() ) );

	return m_values[ nKeyIndex ];
}


template< class T >
void CDmeTypedLogLayer< T >::GetValue( DmeTime_t time, CDmAttribute *pAttr, uint index ) const
{
	DmAttributeType_t attrtype = pAttr->GetType();
	if ( IsValueType( attrtype ) )
	{
		Assert( attrtype == GetDataType() );
		pAttr->SetValue( GetValue( time ) );
	}
	else if ( IsArrayType( attrtype ) )
	{
		Assert( ArrayTypeToValueType( attrtype ) == GetDataType() );
		CDmrArray<T> array( pAttr );
		array.Set( index, GetValue( time ) );
	}
	else
	{
		Assert( 0 );
	}
}

template< class T >
float CDmeTypedLogLayer< T >::GetComponent( DmeTime_t time, int componentIndex ) const
{
	return ::GetComponent( GetValue( time ), componentIndex );
}

template< class T >
void CDmeTypedLogLayer< T >::SetKeyValue( int nKey, const T& value )
{
	Assert( nKey >= 0 );
	Assert( nKey < m_values.Count() );

	m_values.Set( nKey,  value );
}


//-----------------------------------------------------------------------------
// resampling and filtering
//-----------------------------------------------------------------------------
template< class T >
void CDmeTypedLogLayer< T >::Resample( DmeFramerate_t samplerate )
{
	// FIXME:  Might have to revisit how to determine "curve types" for "resampled points...
	Assert( !IsUsingCurveTypes() );

	// make sure we resample to include _at_least_ the existing time range
	DmeTime_t begin = GetBeginTime();
	DmeTime_t end = GetEndTime();
	int nSamples = 2 + FrameForTime( end - begin, samplerate );

	CUtlVector< int > resampledTimes;
	CUtlVector< T > resampledValues;
	CUtlVector< int > resampledCurveTypes;

	resampledValues.EnsureCapacity( nSamples );
	resampledTimes.EnsureCapacity( nSamples );

	DmeTime_t time( begin );
	for ( int i = 0; i < nSamples; ++i )
	{
		resampledTimes.AddToTail( time.GetTenthsOfMS() );
		resampledValues.AddToTail( GetValue( time ) );
		if ( IsUsingCurveTypes() )
		{
			resampledCurveTypes.AddToTail( CURVE_DEFAULT );
		}

		time = time.TimeAtNextFrame( samplerate );
	}

	m_times.SwapArray( resampledTimes );
	m_values.SwapArray( resampledValues );
	if ( IsUsingCurveTypes() )
	{
		m_CurveTypes.SwapArray( resampledCurveTypes );
	}
}

template< class T >
void CDmeTypedLogLayer< T >::Filter( int nSampleRadius )
{
	// Doesn't mess with curvetypes!!!

	const CUtlVector< T > &values = m_values.Get();
	CUtlVector< T > filteredValues;

	int nValues = values.Count();
	filteredValues.EnsureCapacity( nValues );

	for ( int i = 0; i < nValues; ++i )
	{
		int nSamples = min( nSampleRadius, min( i, nValues - i - 1 ) );
		filteredValues.AddToTail( Average( values.Base() + i - nSamples, 2 * nSamples + 1 ) );
	}

	m_values.SwapArray( filteredValues );
}

template< class T >
void CDmeTypedLogLayer< T >::Filter2( DmeTime_t sampleRadius )
{
	// Doesn't mess with curvetypes!!!

	const CUtlVector< T > &values = m_values.Get();
	CUtlVector< T > filteredValues;

	int nValues = values.Count();
	filteredValues.EnsureCapacity( nValues );

	DmeTime_t earliest = DMETIME_ZERO;
	if ( nValues > 0 )
	{
		earliest = DmeTime_t( m_times[ 0 ] );
	}
	for ( int i = 0; i < nValues; ++i )
	{
		T vals[ 3 ];
		DmeTime_t t = GetKeyTime( i );
		DmeTime_t t0 = t - sampleRadius;
		DmeTime_t t1 = t + sampleRadius;

		if ( t0 >= earliest )
		{
			vals[ 0 ] = GetValue( t0 );
		}
		else
		{
			vals[ 0 ] = m_values[ 0 ];
		}
		vals[ 1 ] = GetValue( t );
		vals[ 2 ] = GetValue( t1 );

		if ( i == 0 || i == nValues - 1 )
		{
			filteredValues.AddToTail( values[ i ] );
		}
		else
		{
			filteredValues.AddToTail( Average( vals, 3 ) );
		}
	}

	m_values.SwapArray( filteredValues );
}

template< class T >
const T& CDmeTypedLogLayer< T >::GetValueSkippingKey( int nKeyToSkip ) const
{
	// Curve Interpolation only for 1-D float data right now!!!
	if ( IsUsingCurveTypes() && CanInterpolateType( GetDataType() ) )
	{
		static T out;
		GetValueUsingCurveInfoSkippingKey( nKeyToSkip, out );
		return out;
	}

	Assert( m_values.Count() == m_times.Count() );
	Assert( !IsUsingCurveTypes() || ( m_CurveTypes.Count() == m_times.Count() ) );

	DmeTime_t time = GetKeyTime( nKeyToSkip );

	int prevKey = nKeyToSkip - 1;
	int nextKey = nKeyToSkip  + 1;

	DmeTime_t prevTime;
	T prevValue;
	int prevCurveType;
	DmeTime_t nextTime;
	T nextValue;
	int nextCurveType;

    GetBoundedSample( prevKey, prevTime, prevValue, prevCurveType );
    GetBoundedSample( nextKey, nextTime, nextValue, nextCurveType );

	// Figure out the lerp factor
	float t = GetFractionOfTimeBetween( time, prevTime, nextTime );

	static T s_value;
	s_value = Interpolate( t, prevValue, nextValue );
	return s_value;
}

template< class T >
void CDmeTypedLog<T>::RemoveRedundantKeys( float threshold )
{
	int bestLayer = GetTopmostLayer();
	if ( bestLayer < 0 )
		return;

	GetLayer( bestLayer )->RemoveRedundantKeys( threshold );
}

template< class T >
void CDmeTypedLogLayer<T>::RemoveRedundantKeys( float threshold )
{
	Assert( GetTypedOwnerLog() );
	if ( !GetTypedOwnerLog() )
		return;

	float saveThreshold;
	{
		CDisableUndoScopeGuard sg;
		saveThreshold = GetTypedOwnerLog()->GetValueThreshold();
		GetTypedOwnerLog()->SetValueThreshold( threshold );
	}

	RemoveRedundantKeys();

	{
		CDisableUndoScopeGuard sg;
		GetTypedOwnerLog()->SetValueThreshold( saveThreshold );
	}
	
}

// Implementation of Douglas-Peucker curve simplification routine (hacked to only care about error against original curve (sort of 1D)

template< class T >
void CDmeTypedLogLayer< T >::CurveSimplify_R( float thresholdSqr, int startPoint, int endPoint, CDmeTypedLogLayer< T > *output )
{
	if ( endPoint <= startPoint + 1 )
	{
		return;
	}

	int maxPoint = startPoint;
	float maxDistanceSqr = 0.0f;

	for ( int i = startPoint + 1 ; i < endPoint; ++i )
	{
		DmeTime_t keyTime = GetKeyTime( i );
        T check = GetKeyValue( i );
		T check2 = output->GetValue( keyTime );
		T dist = Subtract( check, check2 );
		float distSqr = LengthOf( dist ) * LengthOf( dist );

		if ( distSqr < maxDistanceSqr )
			continue;

		maxPoint = i;
		maxDistanceSqr = distSqr;
	}

	if ( maxDistanceSqr > thresholdSqr )
	{
		output->InsertKey( GetKeyTime( maxPoint ), GetKeyValue( maxPoint ) );
		CurveSimplify_R( thresholdSqr, startPoint, maxPoint, output );
		CurveSimplify_R( thresholdSqr, maxPoint, endPoint, output );
	}
}

template<> void CDmeTypedLogLayer< bool >::CurveSimplify_R( float thresholdSqr, int startPoint, int endPoint, CDmeTypedLogLayer< bool > *output ) {};
template<> void CDmeTypedLogLayer< int >::CurveSimplify_R( float thresholdSqr, int startPoint, int endPoint, CDmeTypedLogLayer< int > *output ) {};
template<> void CDmeTypedLogLayer< Color >::CurveSimplify_R( float thresholdSqr, int startPoint, int endPoint, CDmeTypedLogLayer< Color > *output ) {};
template<> void CDmeTypedLogLayer< Quaternion >::CurveSimplify_R( float thresholdSqr, int startPoint, int endPoint, CDmeTypedLogLayer< Quaternion > *output ) {};
template<> void CDmeTypedLogLayer< VMatrix >::CurveSimplify_R( float thresholdSqr, int startPoint, int endPoint, CDmeTypedLogLayer< VMatrix > *output ) {};

// We can't just walk the keys linearly since it'll accumulate too much error and give us a bad curve after simplification.  We do a recursive subdivide which has a worst case of O(n^2) but
//  probably is better than that in most cases.
template< class T >
void CDmeTypedLogLayer<T>::RemoveRedundantKeys()
{
	CDmeTypedLog< T > *pOwner = GetTypedOwnerLog();
	if ( !pOwner )
		return;

	int nKeys = GetKeyCount();
	if ( nKeys <= 2 )
		return;

	float thresh =  pOwner->GetValueThreshold();
	if ( thresh < 0.0f )
		return;

	CDmeTypedLogLayer< T > *save = 0;
	{
		CDisableUndoScopeGuard guard;
		save = CastElement< CDmeTypedLogLayer< T > >( CreateLayer< T >( pOwner ) );
		Assert( save );

		save->m_times.EnsureCapacity( nKeys );
		save->m_values.EnsureCapacity( nKeys );

		// Insert start and end points as first "guess" at simplified curve
		// Skip preceeding and ending keys that have the same value
		int nFirstKey, nLastKey;
		for ( nFirstKey = 1; nFirstKey < nKeys; ++nFirstKey )
		{
			// FIXME: Should we use a tolerance check here?
			if ( GetKeyValue( nFirstKey ) != GetKeyValue( nFirstKey - 1 ) )
				break;
		}
		--nFirstKey;

		for ( nLastKey = nKeys; --nLastKey >= 1; )
		{
			// FIXME: Should we use a tolerance check here?
			if ( GetKeyValue( nLastKey ) != GetKeyValue( nLastKey - 1 ) )
				break;
		}

		if ( nLastKey <= nFirstKey )
		{
			save->InsertKey( GetKeyTime( 0 ), GetKeyValue( 0 ) );
		}
		else
		{
			if ( GetDataType() == AT_FLOAT )
			{
				save->InsertKey( GetKeyTime( nFirstKey ), GetKeyValue( nFirstKey ) );
				save->InsertKey( GetKeyTime( nLastKey ), GetKeyValue( nLastKey ) );

				// Recursively finds the point with the largest error from the "simplified curve" and subdivides the problem on both sides until the largest delta from the simplified
				//  curve is less than the tolerance (squared)
				CurveSimplify_R( thresh * thresh, nFirstKey, nLastKey, save );
			}
			else
			{
				save->InsertKey( GetKeyTime( nFirstKey ), GetKeyValue( nFirstKey ) );

				// copy over keys that differ from their prior or next keys - this keeps the first and last key of a run of same-valued keys
				for ( int i = nFirstKey + 1; i < nLastKey; ++i )
				{
					// prev is from the saved log to allow deleting runs of same-valued keys
					const T &prev = save->GetKeyValue( save->GetKeyCount() - 1 ); 
					const T &curr = GetKeyValue( i );
					const T &next = GetKeyValue( i + 1  );
					if ( pOwner->ValuesDiffer( prev, curr ) || pOwner->ValuesDiffer( curr, next ) )
					{
						save->InsertKey( GetKeyTime( i ), curr );
					}
				}

				save->InsertKey( GetKeyTime( nLastKey ), GetKeyValue( nLastKey ) );
			}
		}
	}

	// This operation is undoable
	CopyLayer( save );
	
	{
		CDisableUndoScopeGuard guard;
		g_pDataModel->DestroyElement( save->GetHandle() );
	}
}

// curve info helpers
template< class T >
const CDmeTypedCurveInfo< T > *CDmeTypedLogLayer<T>::GetTypedCurveInfo() const
{
	Assert( GetTypedOwnerLog() );
	return GetTypedOwnerLog()->GetTypedCurveInfo();
}

template< class T >
CDmeTypedCurveInfo< T > *CDmeTypedLogLayer<T>::GetTypedCurveInfo()
{
	Assert( GetTypedOwnerLog() );
	return GetTypedOwnerLog()->GetTypedCurveInfo();
}

template< class T >
bool CDmeTypedLogLayer< T >::IsUsingEdgeInfo() const
{
	return GetTypedOwnerLog()->IsUsingEdgeInfo();
}

template< class T >
const T& CDmeTypedLogLayer< T >::GetDefaultEdgeZeroValue() const
{
	return GetTypedOwnerLog()->GetDefaultEdgeZeroValue();
}

template< class T >
DmeTime_t CDmeTypedLogLayer< T >::GetRightEdgeTime() const
{
	return GetTypedOwnerLog()->GetRightEdgeTime();
}


template< class T >
void CDmeTypedLogLayer< T >::GetEdgeInfo( int edge, bool& active, T& val, int& curveType ) const
{
	GetTypedOwnerLog()->GetEdgeInfo( edge, active, val, curveType );
}

template< class T >
int CDmeTypedLogLayer< T >::GetEdgeCurveType( int edge ) const
{
	return GetTypedOwnerLog()->GetEdgeCurveType( edge );
}

template< class T >
void CDmeTypedLogLayer< T >::GetZeroValue( int side, T& val ) const
{
	return GetTypedOwnerLog()->GetZeroValue( side, val );
}

template< class T >
void CDmeTypedLogLayer< T >::GetBoundedSample( int keyindex, DmeTime_t& time, T& val, int& curveType ) const
{
	Assert( GetOwnerLog() );
	if ( !GetOwnerLog() )
	{
		time = DmeTime_t( 0 );
		CDmAttributeInfo< T >::SetDefaultValue( val );
		curveType = CURVE_DEFAULT;
		return;
	}

	if ( keyindex < 0 )
	{
		time = DmeTime_t( 0 );
		GetZeroValue( 0, val );
		curveType = GetEdgeCurveType( 0 );
		return;
	}
	else if ( keyindex >= m_times.Count() )
	{
		time = GetTypedOwnerLog()->GetRightEdgeTime();
		if ( time == DmeTime_t( 0 ) && m_times.Count() > 0 )
		{
			// Push it one msec past the final end time
			time = DmeTime_t( m_times[ m_times.Count() - 1 ] ) + DmeTime_t( 1 );
		}
		GetTypedOwnerLog()->GetZeroValue( 1, val );
		curveType = GetTypedOwnerLog()->GetEdgeCurveType( 1 );
		return;
	}

	time = DmeTime_t( m_times[ keyindex ] );
	val = m_values[ keyindex ];
	if ( IsUsingCurveTypes() )
	{
		curveType = m_CurveTypes[ keyindex ];
		if ( curveType == CURVE_DEFAULT )
		{
			curveType = GetTypedOwnerLog()->GetDefaultCurveType();
		}
	}
}

template<>
void CDmeTypedLogLayer< float >::GetValueUsingCurveInfoSkippingKey( int nKeyToSkip, float& out ) const
{
	Assert( GetOwnerLog() );
	if ( !GetOwnerLog() )
	{
		out = 0.0f;
		return;
	}

	Assert( CanInterpolateType( GetDataType() ) );
	Assert( m_values.Count() == m_times.Count() );
	Assert( !IsUsingCurveTypes() || ( m_CurveTypes.Count() == m_times.Count() ) );
	Assert( IsInterpolableType( GetDataType() ) );

	float v[ 4 ];
	DmeTime_t t[ 4 ];
	int curvetypes[ 4 ];
	int ti = nKeyToSkip;
	DmeTime_t time = GetKeyTime( nKeyToSkip );

	if ( !IsUsingCurveTypes() )
	{
		if ( ti < 0 )
		{
			CDmAttributeInfo< float >::SetDefaultValue( out ); // TODO - create GetDefaultValue that returns a default T, to avoid rebuilding every time
			return;
		}
		else if ( ti >= m_times.Count() - 1 )
		{
			out = m_values[ ti + 1 ];
			return;
		}
	}

	DmeTime_t finalTime = GetTypedOwnerLog()->GetRightEdgeTime();
	if ( finalTime != DmeTime_t( 0 ) )
	{
		if ( time > finalTime )
		{
			GetZeroValue( 1, out );
			return;
		}
	}
	else
	{
		if ( ti >= m_times.Count() - 1 )
		{
			out = m_values[ ti + 1 ];
			return;
		}
	}

	GetBoundedSample( ti - 2, t[ 0 ], v[ 0 ], curvetypes[ 0 ] );
	GetBoundedSample( ti - 1, t[ 1 ], v[ 1 ], curvetypes[ 1 ] );
	GetBoundedSample( ti + 1, t[ 2 ], v[ 2 ], curvetypes[ 2 ] );
	GetBoundedSample( ti + 2, t[ 3 ], v[ 3 ], curvetypes[ 3 ] );

	float frac = 0.0f;
	if ( t[2] > t[ 1 ] )
	{
		frac = (time.GetSeconds() - t[1].GetSeconds()) / (float) ( t[2].GetSeconds() - t[ 1 ].GetSeconds() );
	}

	// Compute the lerp between ti and ti+1
	out = Curve_Interpolate( frac, t, v, curvetypes, GetOwnerLog()->GetMinValue(), GetOwnerLog()->GetMaxValue() );
}

template<>
void CDmeTypedLogLayer< Vector >::GetValueUsingCurveInfoSkippingKey( int nKeyToSkip, Vector& out ) const
{
	Assert( GetOwnerLog() );
	if ( !GetOwnerLog() )
	{
		CDmAttributeInfo< Vector >::SetDefaultValue( out );
		return;
	}

	Assert( CanInterpolateType( GetDataType() ) );
	Assert( m_values.Count() == m_times.Count() );
	Assert( !IsUsingCurveTypes() || ( m_CurveTypes.Count() == m_times.Count() ) );
	Assert( IsInterpolableType( GetDataType() ) );

	Vector v[ 4 ];
	DmeTime_t t[ 4 ];
	int curvetypes[ 4 ];
	int ti = nKeyToSkip;
	DmeTime_t time = GetKeyTime( nKeyToSkip );

	if ( !IsUsingCurveTypes() )
	{
		if ( ti < 0 )
		{
			CDmAttributeInfo< Vector >::SetDefaultValue( out ); // TODO - create GetDefaultValue that returns a default T, to avoid rebuilding every time
			return;
		}
		else if ( ti >= m_times.Count() - 1 )
		{
			out = m_values[ ti + 1 ];
			return;
		}
	}

	DmeTime_t finalTime = GetTypedOwnerLog()->GetRightEdgeTime();
	if ( finalTime != DmeTime_t( 0 ) )
	{
		if ( time > finalTime )
		{
			CDmAttributeInfo< Vector >::SetDefaultValue( out );
			return;
		}
	}
	else
	{
		if ( ti >= m_times.Count() - 1 )
		{
			out = m_values[ ti + 1 ];
			return;
		}
	}

	GetBoundedSample( ti - 2, t[ 0 ], v[ 0 ], curvetypes[ 0 ] );
	GetBoundedSample( ti - 1, t[ 1 ], v[ 1 ], curvetypes[ 1 ] );
	GetBoundedSample( ti + 1, t[ 2 ], v[ 2 ], curvetypes[ 2 ] );
	GetBoundedSample( ti + 2, t[ 3 ], v[ 3 ], curvetypes[ 3 ] );

	float frac = 0.0f;
	if ( t[2] > t[ 1 ] )
	{
		frac = (time.GetSeconds() - t[1].GetSeconds()) / (float) ( t[2].GetSeconds() - t[ 1 ].GetSeconds() );
	}

	// Compute the lerp between ti and ti+1
	out = Curve_Interpolate( frac, t, v, curvetypes, GetOwnerLog()->GetMinValue(), GetOwnerLog()->GetMaxValue() );
}

template<>
void CDmeTypedLogLayer< Quaternion >::GetValueUsingCurveInfoSkippingKey( int nKeyToSkip, Quaternion& out ) const
{
	Assert( GetOwnerLog() );
	if ( !GetOwnerLog() )
	{
		CDmAttributeInfo< Quaternion >::SetDefaultValue( out );
		return;
	}

	Assert( CanInterpolateType( GetDataType() ) );
	Assert( m_values.Count() == m_times.Count() );
	Assert( !IsUsingCurveTypes() || ( m_CurveTypes.Count() == m_times.Count() ) );
	Assert( IsInterpolableType( GetDataType() ) );

	Quaternion v[ 4 ];
	DmeTime_t t[ 4 ];
	int curvetypes[ 4 ];
	int ti = nKeyToSkip;
	DmeTime_t time = GetKeyTime( nKeyToSkip );

	if ( !IsUsingCurveTypes() )
	{
		if ( ti < 0 )
		{
			CDmAttributeInfo< Quaternion >::SetDefaultValue( out ); // TODO - create GetDefaultValue that returns a default T, to avoid rebuilding every time
			return;
		}
		else if ( ti >= m_times.Count() - 1 )
		{
			out = m_values[ ti + 1 ];
			return;
		}
	}

	DmeTime_t finalTime = GetTypedOwnerLog()->GetRightEdgeTime();
	if ( finalTime != DmeTime_t( 0 ) )
	{
		if ( time > finalTime )
		{
			CDmAttributeInfo< Quaternion >::SetDefaultValue( out );
			return;
		}
	}
	else
	{
		if ( ti >= m_times.Count() - 1 )
		{
			out = m_values[ ti + 1 ];
			return;
		}
	}

	GetBoundedSample( ti - 2, t[ 0 ], v[ 0 ], curvetypes[ 0 ] );
	GetBoundedSample( ti - 1, t[ 1 ], v[ 1 ], curvetypes[ 1 ] );
	GetBoundedSample( ti + 1, t[ 2 ], v[ 2 ], curvetypes[ 2 ] );
	GetBoundedSample( ti + 2, t[ 3 ], v[ 3 ], curvetypes[ 3 ] );

	float frac = 0.0f;
	if ( t[2] > t[ 1 ] )
	{
		frac = (time.GetSeconds() - t[1].GetSeconds()) / (float) ( t[2].GetSeconds() - t[ 1 ].GetSeconds() );
	}

	// Compute the lerp between ti and ti+1
	out = Curve_Interpolate( frac, t, v, curvetypes, GetOwnerLog()->GetMinValue(), GetOwnerLog()->GetMaxValue() );
}


template<>
void CDmeTypedLogLayer< float >::GetValueUsingCurveInfo( DmeTime_t time, float& out ) const
{
	Assert( GetOwnerLog() );
	if ( !GetOwnerLog() )
	{
		out = 0.0f;
		return;
	}

	Assert( CanInterpolateType( GetDataType() ) );
	Assert( m_values.Count() == m_times.Count() );
	Assert( !IsUsingCurveTypes() || ( m_CurveTypes.Count() == m_times.Count() ) );
	Assert( IsInterpolableType( GetDataType() ) );

	float v[ 4 ];
	DmeTime_t t[ 4 ];
	int curvetypes[ 4 ];
	int ti = FindKey( time );
	if ( !IsUsingCurveTypes() )
	{
		if ( ti < 0 )
		{
			CDmAttributeInfo< float >::SetDefaultValue( out ); // TODO - create GetDefaultValue that returns a default T, to avoid rebuilding every time
			return;
		}
		else if ( ti >= m_times.Count() - 1 )
		{
			out = m_values[ ti ];
			return;
		}
	}

	DmeTime_t finalTime = GetTypedOwnerLog()->GetRightEdgeTime();
	if ( finalTime != DmeTime_t( 0 ) )
	{
		if ( time > finalTime )
		{
			GetZeroValue( 1, out );
			return;
		}
	}
	else
	{
		if ( ti >= m_times.Count() - 1 )
		{
			out = m_values[ ti ];
			return;
		}
	}

	GetBoundedSample( ti - 1, t[ 0 ], v[ 0 ], curvetypes[ 0 ] );
	GetBoundedSample( ti + 0, t[ 1 ], v[ 1 ], curvetypes[ 1 ] );
	GetBoundedSample( ti + 1, t[ 2 ], v[ 2 ], curvetypes[ 2 ] );
	GetBoundedSample( ti + 2, t[ 3 ], v[ 3 ], curvetypes[ 3 ] );

	float frac = 0.0f;
	if ( t[2] > t[ 1 ] )
	{
		frac = (time.GetSeconds() - t[1].GetSeconds()) / (float) ( t[2].GetSeconds() - t[ 1 ].GetSeconds() );
	}

	// Compute the lerp between ti and ti+1
	out = Curve_Interpolate( frac, t, v, curvetypes, GetOwnerLog()->GetMinValue(), GetOwnerLog()->GetMaxValue() );
}

template<>
void CDmeTypedLogLayer< Vector >::GetValueUsingCurveInfo( DmeTime_t time, Vector& out ) const
{
	Assert( GetOwnerLog() );
	if ( !GetOwnerLog() )
	{
		CDmAttributeInfo< Vector >::SetDefaultValue( out );
		return;
	}

	Assert( CanInterpolateType( GetDataType() ) );
	Assert( m_values.Count() == m_times.Count() );
	Assert( !IsUsingCurveTypes() || ( m_CurveTypes.Count() == m_times.Count() ) );
	Assert( IsInterpolableType( GetDataType() ) );

	Vector v[ 4 ];
	DmeTime_t t[ 4 ];
	int curvetypes[ 4 ];
	int ti = FindKey( time );
	if ( !IsUsingCurveTypes() )
	{
		if ( ti < 0 )
		{
			CDmAttributeInfo< Vector >::SetDefaultValue( out ); // TODO - create GetDefaultValue that returns a default T, to avoid rebuilding every time
			return;
		}
		else if ( ti >= m_times.Count() - 1 )
		{
			out = m_values[ ti ];
			return;
		}
	}

	DmeTime_t finalTime = GetTypedOwnerLog()->GetRightEdgeTime();
	if ( finalTime != DmeTime_t( 0 ) )
	{
		if ( time > finalTime )
		{
			CDmAttributeInfo< Vector >::SetDefaultValue( out );
			return;
		}
	}
	else
	{
		if ( ti >= m_times.Count() - 1 )
		{
			out = m_values[ ti ];
			return;
		}
	}

	GetBoundedSample( ti - 1, t[ 0 ], v[ 0 ], curvetypes[ 0 ] );
	GetBoundedSample( ti + 0, t[ 1 ], v[ 1 ], curvetypes[ 1 ] );
	GetBoundedSample( ti + 1, t[ 2 ], v[ 2 ], curvetypes[ 2 ] );
	GetBoundedSample( ti + 2, t[ 3 ], v[ 3 ], curvetypes[ 3 ] );

	float frac = 0.0f;
	if ( t[2] > t[ 1 ] )
	{
		frac = (time.GetSeconds() - t[1].GetSeconds()) / (float) ( t[2].GetSeconds() - t[ 1 ].GetSeconds() );
	}

	// Compute the lerp between ti and ti+1
	out = Curve_Interpolate( frac, t, v, curvetypes, GetOwnerLog()->GetMinValue(), GetOwnerLog()->GetMaxValue() );
}

template<>
void CDmeTypedLogLayer< Quaternion >::GetValueUsingCurveInfo( DmeTime_t time, Quaternion& out ) const
{
	Assert( GetOwnerLog() );
	if ( !GetOwnerLog() )
	{
		CDmAttributeInfo< Quaternion >::SetDefaultValue( out );
		return;
	}

	Assert( CanInterpolateType( GetDataType() ) );
	Assert( m_values.Count() == m_times.Count() );
	Assert( !IsUsingCurveTypes() || ( m_CurveTypes.Count() == m_times.Count() ) );
	Assert( IsInterpolableType( GetDataType() ) );

	Quaternion v[ 4 ];
	DmeTime_t t[ 4 ];
	int curvetypes[ 4 ];
	int ti = FindKey( time );
	if ( !IsUsingCurveTypes() )
	{
		if ( ti < 0 )
		{
			CDmAttributeInfo< Quaternion >::SetDefaultValue( out ); // TODO - create GetDefaultValue that returns a default T, to avoid rebuilding every time
			return;
		}
		else if ( ti >= m_times.Count() - 1 )
		{
			out = m_values[ ti ];
			return;
		}
	}

	DmeTime_t finalTime = GetTypedOwnerLog()->GetRightEdgeTime();
	if ( finalTime != DmeTime_t( 0 ) )
	{
		if ( time > finalTime )
		{
			CDmAttributeInfo< Quaternion >::SetDefaultValue( out );
			return;
		}
	}
	else
	{
		if ( ti >= m_times.Count() - 1 )
		{
			out = m_values[ ti ];
			return;
		}
	}

	GetBoundedSample( ti - 1, t[ 0 ], v[ 0 ], curvetypes[ 0 ] );
	GetBoundedSample( ti + 0, t[ 1 ], v[ 1 ], curvetypes[ 1 ] );
	GetBoundedSample( ti + 1, t[ 2 ], v[ 2 ], curvetypes[ 2 ] );
	GetBoundedSample( ti + 2, t[ 3 ], v[ 3 ], curvetypes[ 3 ] );

	float frac = 0.0f;
	if ( t[2] > t[ 1 ] )
	{
		frac = (time.GetSeconds() - t[1].GetSeconds()) / (float) ( t[2].GetSeconds() - t[ 1 ].GetSeconds() );
	}

	// Compute the lerp between ti and ti+1
	out = Curve_Interpolate( frac, t, v, curvetypes, GetOwnerLog()->GetMinValue(), GetOwnerLog()->GetMaxValue() );
}

template< class T >
void CDmeTypedLogLayer< T >::CopyLayer( const CDmeLogLayer *src )
{
	const CDmeTypedLogLayer< T > *pSrc = static_cast< const CDmeTypedLogLayer< T > * >( src );
	m_times = pSrc->m_times;
	m_lastKey = pSrc->m_lastKey;
	m_values = pSrc->m_values;
	m_CurveTypes = pSrc->m_CurveTypes;
}

template< class T >
void CDmeTypedLogLayer< T >::InsertKeyFromLayer( DmeTime_t keyTime, const CDmeLogLayer *src, DmeTime_t srcKeyTime )
{
	const CDmeTypedLogLayer< T > *pSrc = static_cast< const CDmeTypedLogLayer< T > * >( src );
	Assert( pSrc );

	// NOTE: This copy is necessary if src == this
	T value = pSrc->GetValue( srcKeyTime );
	InsertKey( keyTime, value );
}

template< class T >
void CDmeTypedLogLayer< T >::ExplodeLayer( const CDmeLogLayer *src, DmeTime_t startTime, DmeTime_t endTime, bool bRebaseTimestamps, DmeTime_t tResampleInterval )
{
	const CDmeTypedLogLayer< T > *pSrc = static_cast< const CDmeTypedLogLayer< T > * >( src );
	Assert( pSrc );

	DmeTime_t tTimeOffset = DMETIME_ZERO;
	if ( bRebaseTimestamps )
	{
		tTimeOffset = -startTime;
	}

	m_times.RemoveAll();
	m_values.RemoveAll();
	m_CurveTypes.RemoveAll();

	bool usecurvetypes = pSrc->IsUsingCurveTypes();

	// Now copy the data for the later
	for ( DmeTime_t t = startTime ; t + tResampleInterval < endTime; t += tResampleInterval )
	{
		DmeTime_t keyTime = DmeTime_t( t );
		if ( keyTime > endTime )
		{
			keyTime = endTime;
		}

		T val = pSrc->GetValue( keyTime );

		keyTime += tTimeOffset;

		InsertKey( keyTime, val, usecurvetypes ? GetDefaultCurveType() : CURVE_DEFAULT );
	}

	m_lastKey = m_times.Count() - 1;
}

template< class T >
void CDmeTypedLogLayer< T >::CopyPartialLayer( const CDmeLogLayer *src, DmeTime_t startTime, DmeTime_t endTime, bool bRebaseTimestamps )
{
	const CDmeTypedLogLayer< T > *pSrc = static_cast< const CDmeTypedLogLayer< T > * >( src );
	Assert( pSrc );

	int nTimeOffset = 0;
	if ( bRebaseTimestamps )
	{
		nTimeOffset = -startTime.GetTenthsOfMS();
	}

	m_times.RemoveAll();
	m_values.RemoveAll();
	m_CurveTypes.RemoveAll();

	bool usecurvetypes = pSrc->IsUsingCurveTypes();

	// Now copy the data for the later
	int c = pSrc->m_times.Count();
	for ( int i = 0; i < c; ++i )
	{
		DmeTime_t keyTime = DmeTime_t( pSrc->m_times[ i ] );
		if ( keyTime < startTime || keyTime > endTime )
			continue;

		m_times.AddToTail( pSrc->m_times[ i ] + nTimeOffset );
		m_values.AddToTail( pSrc->m_values[ i ] );
		if ( usecurvetypes )
		{
			m_CurveTypes.AddToTail( pSrc->m_CurveTypes[ i ] );
		}
	}

	m_lastKey = m_times.Count() - 1;
}

//-----------------------------------------------------------------------------
// Creates a log of a specific type
//-----------------------------------------------------------------------------
template< class T >
CDmeLogLayer *CreateLayer< T >( CDmeTypedLog< T > *pOwnerLog )
{
	DmFileId_t fileid = pOwnerLog ? pOwnerLog->GetFileId() : DMFILEID_INVALID;
	CDmeLogLayer *layer = NULL;

	switch ( CDmAttributeInfo<T>::AttributeType() )
	{
	case AT_INT:
	case AT_INT_ARRAY:
		layer = CreateElement< CDmeIntLogLayer >( "int log", fileid );
		break;
	case AT_FLOAT:
	case AT_FLOAT_ARRAY:
		layer = CreateElement< CDmeFloatLogLayer >( "float log", fileid );
		break;
	case AT_BOOL:
	case AT_BOOL_ARRAY:
		layer = CreateElement< CDmeBoolLogLayer >( "bool log", fileid );
		break;
	case AT_COLOR:
	case AT_COLOR_ARRAY:
		layer = CreateElement< CDmeColorLogLayer >( "color log", fileid );
		break;
	case AT_VECTOR2:
	case AT_VECTOR2_ARRAY:
		layer = CreateElement< CDmeVector2LogLayer >( "vector2 log", fileid );
		break;
	case AT_VECTOR3:
	case AT_VECTOR3_ARRAY:
		layer = CreateElement< CDmeVector3LogLayer >( "vector3 log", fileid );
		break;
	case AT_VECTOR4:
	case AT_VECTOR4_ARRAY:
		layer = CreateElement< CDmeVector4LogLayer >( "vector4 log", fileid );
		break;
	case AT_QANGLE:
	case AT_QANGLE_ARRAY:
		layer = CreateElement< CDmeQAngleLogLayer >( "qangle log", fileid );
		break;
	case AT_QUATERNION:
	case AT_QUATERNION_ARRAY:
		layer = CreateElement< CDmeQuaternionLogLayer >( "quaternion log", fileid );
		break;
	case AT_VMATRIX:
	case AT_VMATRIX_ARRAY:
		layer = CreateElement< CDmeVMatrixLogLayer >( "vmatrix log", fileid );
		break;
	case AT_STRING:
	case AT_STRING_ARRAY:
		layer = CreateElement< CDmeStringLogLayer >( "string log", fileid );
		break;
	}

	if ( layer )
	{
		layer->SetOwnerLog( pOwnerLog );
	}
	return layer;
}



//-----------------------------------------------------------------------------
//
// CDmeCurveInfo - abstract base class
//
//-----------------------------------------------------------------------------
void CDmeCurveInfo::OnConstruction()
{
	m_DefaultCurveType.Init( this, "defaultCurveType" );

	m_MinValue.InitAndSet( this, "minvalue", 0.0f );
	m_MaxValue.InitAndSet( this, "maxvalue", 1.0f );
}

void CDmeCurveInfo::OnDestruction()
{
}

// Global override for all keys unless overriden by specific key
void CDmeCurveInfo::SetDefaultCurveType( int curveType )
{
	m_DefaultCurveType = curveType;
}

int CDmeCurveInfo::GetDefaultCurveType() const
{
	return m_DefaultCurveType.Get();
}

void CDmeCurveInfo::SetMinValue( float val )
{
	m_MinValue = val;
}

float CDmeCurveInfo::GetMinValue() const
{
	return m_MinValue;
}

void CDmeCurveInfo::SetMaxValue( float val )
{
	m_MaxValue = val;
}

float CDmeCurveInfo::GetMaxValue() const
{
	return m_MaxValue;
}


//-----------------------------------------------------------------------------
//
// CDmeTypedCurveInfo - implementation class for all logs
//
//-----------------------------------------------------------------------------
template< class T >
void CDmeTypedCurveInfo< T >::OnConstruction()
{
	m_bUseEdgeInfo.Init( this, "useEdgeInfo" );
	m_DefaultEdgeValue.Init( this, "defaultEdgeZeroValue" );
	m_RightEdgeTime.Init( this, "rightEdgeTime" );

	for ( int i = 0; i < 2; ++i )
	{
		char edgename[ 32 ];
		Q_snprintf( edgename, sizeof( edgename ), "%s", i == 0 ? "left" : "right" );
		char name[ 32 ];
		Q_snprintf( name, sizeof( name ), "%sEdgeActive", edgename );
		m_bEdgeActive[ i ].Init( this, name );
		Q_snprintf( name, sizeof( name ), "%sEdgeValue", edgename );
		m_EdgeValue[ i ].Init( this, name );
		Q_snprintf( name, sizeof( name ), "%sEdgeCurveType", edgename );
		m_EdgeCurveType[ i ].Init( this, name );
	}
}

template< class T >
void CDmeTypedCurveInfo< T >::OnDestruction()
{
}

template< class T >
void CDmeTypedCurveInfo< T >::SetUseEdgeInfo( bool state )
{
	m_bUseEdgeInfo = state;
}

template< class T >
bool CDmeTypedCurveInfo< T >::IsUsingEdgeInfo() const
{
	return m_bUseEdgeInfo;
}

template< class T >
void CDmeTypedCurveInfo< T >::SetEdgeInfo( int edge, bool active, const T& val, int curveType )
{
	SetUseEdgeInfo( true );

	Assert( edge == 0 || edge == 1 );

	m_bEdgeActive[ edge ] = active;
	m_EdgeValue[ edge ] = val;
	m_EdgeCurveType[ edge ] = curveType;
}

template< class T >
void CDmeTypedCurveInfo< T >::SetDefaultEdgeZeroValue( const T& val )
{
	m_DefaultEdgeValue = val;
}

template< class T >
const T& CDmeTypedCurveInfo< T >::GetDefaultEdgeZeroValue() const
{
	return m_DefaultEdgeValue;
}

template< class T >
void CDmeTypedCurveInfo< T >::SetRightEdgeTime( DmeTime_t time )
{
	m_RightEdgeTime = time.GetTenthsOfMS();
}

template< class T >
DmeTime_t CDmeTypedCurveInfo< T >::GetRightEdgeTime() const
{
	return DmeTime_t( m_RightEdgeTime );
}

template< class T >
void CDmeTypedCurveInfo< T >::GetEdgeInfo( int edge, bool& active, T& val, int& curveType ) const
{
	Assert( IsUsingEdgeInfo() );

	Assert( edge == 0 || edge == 1 );

	active = m_bEdgeActive[ edge ];
	val = m_EdgeValue[ edge ];
	curveType = m_EdgeCurveType[ edge ];
}

template< class T >
int CDmeTypedCurveInfo< T >::GetEdgeCurveType( int edge ) const
{
	Assert( edge == 0 || edge == 1 );

	if ( !m_bEdgeActive[ edge ] )
	{
		return m_DefaultCurveType;
	}

	if ( m_EdgeCurveType[ edge ] == CURVE_DEFAULT )
	{
		return m_DefaultCurveType;
	}

	return m_EdgeCurveType[ edge ];
}

template<>
void CDmeTypedCurveInfo<float>::GetZeroValue( int side, float& val ) const
{
	if ( !m_bUseEdgeInfo )
	{
		val = 0.0f;
		return;
	}

	if ( m_bEdgeActive[ side ] )
	{
		val = m_EdgeValue[ side ];
		return;
	}

	val = m_DefaultEdgeValue;
}

template<>
bool CDmeTypedCurveInfo<float>::IsEdgeActive( int edge ) const
{
	return m_bEdgeActive[ edge ];
}

template<>
void CDmeTypedCurveInfo<float>::GetEdgeValue( int edge, float& value ) const
{
	value = m_EdgeValue[ edge ];
}

template<>
void CDmeTypedCurveInfo<Vector>::GetZeroValue( int side, Vector& val ) const
{
	if ( !m_bUseEdgeInfo )
	{
		val = vec3_origin;
		return;
	}

	if ( m_bEdgeActive[ side ] )
	{
		val = m_EdgeValue[ side ];
		return;
	}

	val = m_DefaultEdgeValue;
}

template<>
void CDmeTypedCurveInfo<Quaternion>::GetZeroValue( int side, Quaternion& val ) const
{
	if ( !m_bUseEdgeInfo )
	{
		val.Init();
		return;
	}

	if ( m_bEdgeActive[ side ] )
	{
		val = m_EdgeValue[ side ];
		return;
	}

	val = m_DefaultEdgeValue;
}


//-----------------------------------------------------------------------------
//
// CDmeLog - abstract base class
//
//-----------------------------------------------------------------------------
void CDmeLog::OnConstruction()
{
	m_Layers.Init( this, "layers", FATTRIB_MUSTCOPY | FATTRIB_HAS_ARRAY_CALLBACK );
	m_CurveInfo.Init( this, "curveinfo", FATTRIB_MUSTCOPY | FATTRIB_HAS_CALLBACK );
}

void CDmeLog::OnDestruction()
{
}

int CDmeLog::GetTopmostLayer() const
{
	return m_Layers.Count() - 1;
}

int	CDmeLog::GetNumLayers() const
{
	return m_Layers.Count();
}

CDmeLogLayer *CDmeLog::GetLayer( int index )
{
	return m_Layers[ index ];
}

const CDmeLogLayer *CDmeLog::GetLayer( int index ) const
{
	return m_Layers[ index ];
}

bool CDmeLog::IsEmpty() const
{
	int c = m_Layers.Count();
	for ( int i = 0; i < c; ++i )
	{
		CDmeLogLayer* layer = m_Layers[ i ];
		if ( layer->GetKeyCount() > 0 )
			return false;
	}
	return true;
}

void CDmeLog::FindLayersForTime( DmeTime_t time, CUtlVector< int >& list ) const
{
	list.RemoveAll();
	int c = m_Layers.Count();
	// The base layer is always available!!!
	if ( c > 0 )
	{
		list.AddToTail( 0 );
	}
	for ( int i = 1; i < c; ++i )
	{
		CDmeLogLayer* layer = m_Layers[ i ];
		DmeTime_t layerStart = layer->GetBeginTime();
		if ( layerStart == DmeTime_t::MinTime() )
			continue;
		DmeTime_t layerEnd = layer->GetEndTime();
		if ( layerEnd == DmeTime_t::MaxTime() )
			continue;

		if ( time >= layerStart && time <= layerEnd )
		{
			list.AddToTail( i );
		}
	}
}

int CDmeLog::FindLayerForTimeSkippingTopmost( DmeTime_t time ) const
{
	int c = m_Layers.Count() - 1; // This makes it never consider the topmost layer!!!
	for ( int i = c - 1; i >= 0; --i )
	{
		CDmeLogLayer* layer = m_Layers[ i ];
		DmeTime_t layerStart = layer->GetBeginTime();
		if ( layerStart == DmeTime_t::MinTime() )
			continue;
		DmeTime_t layerEnd = layer->GetEndTime();
		if ( layerEnd == DmeTime_t::MaxTime() )
			continue;

		if ( time >= layerStart && time <= layerEnd )
			return i;
	}
	return ( c > 0 ) ? 0 : -1;
}

int CDmeLog::FindLayerForTime( DmeTime_t time ) const
{
	int c = m_Layers.Count();
	for ( int i = c - 1; i >= 0; --i )
	{
		CDmeLogLayer* layer = m_Layers[ i ];
		DmeTime_t layerStart = layer->GetBeginTime();
		if ( layerStart == DmeTime_t::MinTime() )
			continue;
		DmeTime_t layerEnd = layer->GetEndTime();
		if ( layerEnd == DmeTime_t::MaxTime() )
			continue;

		if ( time >= layerStart && time <= layerEnd )
			return i;
	}
	return ( c > 0 ) ? 0 : -1;
}

DmeTime_t CDmeLog::GetBeginTime() const
{
	int c = m_Layers.Count();
	if ( c == 0 )
		return DmeTime_t::MinTime();

	DmeTime_t bestMin = DmeTime_t::MinTime();
	for ( int i = 0; i < c; ++i )
	{
		CDmeLogLayer* layer = m_Layers[ i ];
		DmeTime_t layerStart = layer->GetBeginTime();
		if ( layerStart == DmeTime_t::MinTime() )
			continue;

		if ( bestMin == DmeTime_t::MinTime() )
		{
			bestMin = layerStart;
		}
		else if ( layerStart < bestMin )
		{
			bestMin = layerStart;
		}
	}

	return bestMin;
}

DmeTime_t CDmeLog::GetEndTime() const
{
	int c = m_Layers.Count();
	if ( c == 0 )
		return DmeTime_t::MaxTime();

	DmeTime_t bestMax = DmeTime_t::MaxTime();
	for ( int i = 0; i < c; ++i )
	{
		CDmeLogLayer *layer = m_Layers[ i ];
		DmeTime_t layerEnd = layer->GetEndTime();
		if ( layerEnd == DmeTime_t::MaxTime() )
			continue;
		if ( bestMax == DmeTime_t::MaxTime() )
		{
			bestMax = layerEnd;
		}
		else if ( layerEnd > bestMax )
		{
			bestMax = layerEnd;
		}
	}

	return bestMax;
}

//-----------------------------------------------------------------------------
// Returns the number of keys
//-----------------------------------------------------------------------------
int CDmeLog::GetKeyCount() const
{
	int count = 0;
	int c = m_Layers.Count();
	for ( int i = 0; i < c; ++i )
	{
		CDmeLogLayer* layer = m_Layers[ i ];
		int timecount = layer->GetKeyCount();
		count += timecount;
	}
	return count;
}


//-----------------------------------------------------------------------------
// Scale + bias key times
//-----------------------------------------------------------------------------
void CDmeLog::ScaleBiasKeyTimes( double flScale, DmeTime_t nBias )
{
	// Don't waste time on the identity transform
	if ( ( nBias == DMETIME_ZERO ) && ( fabs( flScale - 1.0 ) < 1e-5 ) )
		return;

	int nCount = GetNumLayers();
	for ( int i = 0; i < nCount; ++i )
	{
		CDmeLogLayer *pLayer = GetLayer( i );
		pLayer->ScaleBiasKeyTimes( flScale, nBias );
	}
}


//-----------------------------------------------------------------------------
// Resolve - keeps non-attribute data in sync with attribute data
//-----------------------------------------------------------------------------
void CDmeLog::Resolve()
{
	int c = m_Layers.Count();
	for ( int i = 0; i < c; ++i )
	{
		CDmeLogLayer* layer = m_Layers[ i ];
		layer->SetOwnerLog( this );
	}
}

void CDmeLog::OnAttributeChanged( CDmAttribute *pAttribute )
{
	if ( pAttribute == m_CurveInfo.GetAttribute() )
	{
		OnUsingCurveTypesChanged();
	}
}

void CDmeLog::OnUsingCurveTypesChanged()
{
	int c = m_Layers.Count();
	for ( int i = 0; i < c; ++i )
	{
		GetLayer( i )->OnUsingCurveTypesChanged();
	}
}

// curve info helpers
bool CDmeLog::IsUsingCurveTypes() const
{
	return m_CurveInfo.GetElement() != NULL;
}

const CDmeCurveInfo *CDmeLog::GetCurveInfo() const
{
	return m_CurveInfo.GetElement();
}

CDmeCurveInfo *CDmeLog::GetCurveInfo()
{
	return m_CurveInfo.GetElement();
}

// accessors for CurveInfo data
int CDmeLog::GetDefaultCurveType() const
{
	Assert( IsUsingCurveTypes() );
	return m_CurveInfo->GetDefaultCurveType();
}

// min/max accessors
float CDmeLog::GetMinValue() const
{
	Assert( IsUsingCurveTypes() );
	return m_CurveInfo->GetMinValue();
}

void CDmeLog::SetMinValue( float val )
{
	Assert( IsUsingCurveTypes() );
	m_CurveInfo->SetMinValue( val );
}

float CDmeLog::GetMaxValue() const
{
	Assert( IsUsingCurveTypes() );
	return m_CurveInfo->GetMaxValue();
}

void CDmeLog::SetMaxValue( float val )
{
	Assert( IsUsingCurveTypes() );
	m_CurveInfo->SetMaxValue( val );
}

void CDmeLog::SetKeyCurveType( int nKeyIndex, int curveType )
{
	int bestLayer = GetTopmostLayer();
	if ( bestLayer < 0 )
		return;

	GetLayer( bestLayer )->SetKeyCurveType( nKeyIndex, curveType );
}

int CDmeLog::GetKeyCurveType( int nKeyIndex ) const
{
	int bestLayer = GetTopmostLayer();
	if ( bestLayer < 0 )
		return CURVE_DEFAULT;

	return GetLayer( bestLayer )->GetKeyCurveType( nKeyIndex );
}


//-----------------------------------------------------------------------------
// Removes all keys in a certain time interval
//-----------------------------------------------------------------------------
bool CDmeLog::RemoveKeys( DmeTime_t tStartTime, DmeTime_t tEndTime )
{
	CDmeLogLayer *pLayer = GetLayer( GetTopmostLayer() );

	int nKeyCount = pLayer->GetKeyCount();
	int nFirstRemove = -1;
	int nLastRemove = -1;
	for ( int nKey = 0; nKey < nKeyCount; ++nKey )
	{
		DmeTime_t tKeyTime = pLayer->GetKeyTime( nKey );
		if ( tKeyTime < tStartTime )
			continue;
		if ( tKeyTime > tEndTime )
			break;
		if ( nFirstRemove == -1 )
		{
			nFirstRemove = nKey;
		}
		nLastRemove = nKey;
	}

	if ( nFirstRemove != -1 )
	{
		int nRemoveCount = nLastRemove - nFirstRemove + 1;
		pLayer->RemoveKey( nFirstRemove, nRemoveCount );
		return true;
	}
	return false;
}


//-----------------------------------------------------------------------------
// CDmeTypedLog - implementation class for all logs
//-----------------------------------------------------------------------------
template< class T >
void CDmeTypedLog< T >::OnConstruction()
{
	if ( !g_pDataModel->IsUnserializing() )
	{
		// Add the default layer!!!
		AddNewLayer();
		Assert( m_Layers.Count() == 1 );
	}

	m_threshold = s_defaultThreshold;

	m_UseDefaultValue.InitAndSet( this, "usedefaultvalue", false );
	m_DefaultValue.Init( this, "defaultvalue" );
}

template< class T >
void CDmeTypedLog< T >::OnDestruction()
{
}

template< class T >
void CDmeTypedLog< T >::SetDefaultValue( const T& value )
{
	m_UseDefaultValue = true;
	m_DefaultValue.Set( value );
}

template< class T >
const T& CDmeTypedLog< T >::GetDefaultValue() const
{
	Assert( (bool)m_UseDefaultValue );
	return m_DefaultValue;
}

template< class T >
bool CDmeTypedLog< T >::HasDefaultValue() const
{
	return m_UseDefaultValue;
}

template< class T >
void CDmeTypedLog< T >::ClearDefaultValue()
{
	m_UseDefaultValue = false;
	T out;
	CDmAttributeInfo< T >::SetDefaultValue( out );
	m_DefaultValue.Set( out );
}

// Only used by undo system!!!
template< class T >
void CDmeTypedLog< T >::AddLayerToTail( CDmeLogLayer *layer )
{
	Assert( layer );
	Assert( (static_cast< CDmeTypedLogLayer< T > * >( layer ))->GetTypedOwnerLog() == this );
	m_Layers.AddToTail( layer );
}

template< class T >
CDmeLogLayer *CDmeTypedLog< T >::RemoveLayerFromTail()
{
	Assert( m_Layers.Count() >= 1 );
	CDmeLogLayer *layer = m_Layers[ m_Layers.Count() -1 ];
	m_Layers.Remove( m_Layers.Count() - 1 );
	return layer;
}

template< class T >
CDmeLogLayer *CDmeTypedLog< T >::RemoveLayer( int iLayer )
{
	Assert( m_Layers.IsValidIndex( iLayer ) );
	CDmeLogLayer *layer = m_Layers[ iLayer ];
	m_Layers.Remove( iLayer );
	return layer;
}


template< class T >
CDmeLogLayer *CDmeTypedLog< T >::AddNewLayer()
{
	if ( g_pDataModel->UndoEnabledForElement( this ) )
	{
		CUndoLayerAdded<T> *pUndo = new CUndoLayerAdded<T>( "AddNewLayer", this );
		g_pDataModel->AddUndoElement( pUndo );
	}

	CDisableUndoScopeGuard guard;

	// Now add the layer to the stack!!!
	CDmeTypedLogLayer< T > *layer = static_cast< CDmeTypedLogLayer< T > * >( CreateLayer<T>( this ) );
	if ( layer )
	{
		layer->SetOwnerLog( this );
		m_Layers.AddToTail( layer );
	}

	return layer;
}

// curve info helpers
template< class T >
const CDmeTypedCurveInfo< T > *CDmeTypedLog<T>::GetTypedCurveInfo() const
{
	Assert( !m_CurveInfo.GetElement() || dynamic_cast< const CDmeTypedCurveInfo< T > * >( m_CurveInfo.GetElement() ) );
	return static_cast< const CDmeTypedCurveInfo< T > * >( m_CurveInfo.GetElement() );
}

template< class T >
CDmeTypedCurveInfo< T > *CDmeTypedLog<T>::GetTypedCurveInfo()
{
	Assert( !m_CurveInfo.GetElement() || dynamic_cast< CDmeTypedCurveInfo< T > * >( m_CurveInfo.GetElement() ) );
	return static_cast< CDmeTypedCurveInfo< T > * >( m_CurveInfo.GetElement() );
}

template< class T >
void CDmeTypedLog<T>::SetCurveInfo( CDmeCurveInfo *pCurveInfo )
{
	Assert( !pCurveInfo || dynamic_cast< CDmeTypedCurveInfo< T > * >( pCurveInfo ) );
	m_CurveInfo = pCurveInfo;
	OnUsingCurveTypesChanged(); // FIXME: Is this really necessary? OnAttributeChanged should have already called this!
}

template< class T >
CDmeCurveInfo *CDmeTypedLog<T>::GetOrCreateCurveInfo()
{
	CDmeCurveInfo *pCurveInfo = m_CurveInfo.GetElement();
	if ( pCurveInfo )
		return pCurveInfo;

	SetCurveInfo( CreateElement< CDmeTypedCurveInfo< T > >( "curveinfo", GetFileId() ) );
	return m_CurveInfo.GetElement();
}



template < class T >
struct ActiveLayer_t
{
	ActiveLayer_t() :
		bValid( false ),
		priority( 0 ),
		firstTime( 0 ),
		lastTime( 0 ),
		layer( NULL )
	{
	}

	static bool PriorityLessFunc( ActiveLayer_t< T > * const & lhs, ActiveLayer_t< T > * const & rhs )
	{
		return lhs->priority < rhs->priority;
	}

	int		priority; // higher wins

	bool	bValid;
	DmeTime_t		firstTime;
	DmeTime_t		lastTime;

	CDmeTypedLogLayer< T > *layer;
};

template < class T >
struct LayerEvent_t
{
	enum EventType_t
	{
		LE_START = 0,
		LE_END
	};

	LayerEvent_t() : m_pList( NULL ), m_Type( LE_START ), m_nLayer( 0 ), m_Time( 0 )
	{
	}

	static bool LessFunc( const LayerEvent_t& lhs, const LayerEvent_t& rhs )
	{
		return lhs.m_Time < rhs.m_Time;
	}

	CUtlVector< ActiveLayer_t< T > > *m_pList;
	EventType_t		m_Type;
	int				m_nLayer;
	DmeTime_t		m_Time;
	T				m_NeighborValue;
};

template< class T >
static const T& GetActiveLayerValue( CUtlVector< ActiveLayer_t< T > > &layerlist, DmeTime_t t, int nTopmostLayer )
{
	int nCount = layerlist.Count();
#ifdef _DEBUG
	Assert( nCount >= nTopmostLayer );
#endif

	for ( int i = nTopmostLayer; i >= 0; --i )
	{
		ActiveLayer_t< T > &layer = layerlist[i];
		if ( layer.firstTime > t || layer.lastTime < t )
			continue;

		return layer.layer->GetValue( t );
	}

	if ( nCount != 0 )
	{
		const CDmeTypedLog< T > *pOwner = layerlist[0].layer->GetTypedOwnerLog();
		if ( pOwner->HasDefaultValue() )
			return pOwner->GetDefaultValue();
	}

	static T defaultVal;
	CDmAttributeInfo<T>::SetDefaultValue( defaultVal );
	return defaultVal;
}


template< class T >
static void SpewEvents( CUtlRBTree< LayerEvent_t< T > > &events )
{
	for ( unsigned short idx = events.FirstInorder(); idx != events.InvalidIndex(); idx = events.NextInorder( idx ) )
	{
		LayerEvent_t< T > *pEvent = &events[ idx ];
		Msg( "Event %u layer %i at time %i type %s\n",
			(unsigned)idx, pEvent->m_nLayer, pEvent->m_Time.GetTenthsOfMS(), pEvent->m_Type == LayerEvent_t< T >::LE_START ? "start" : "end" );
	}
}

template< class T >
inline void SpewKey( const T& )
{
	Msg( "GenericType" );
}

template<>
inline void SpewKey<float>( const float& val )
{
	Msg( "%f", val );
}

template<>
inline void SpewKey<int>( const int& val )
{
	Msg( "%d", val );
}

template<>
inline void SpewKey<Vector2D>( const Vector2D& val )
{
	Msg( "%f,%f", val.x, val.y );
}

template<>
inline void SpewKey<Vector4D>( const Vector4D& val )
{
	Msg( "%f,%f,%f,%f", val.x, val.y, val.z, val.w );
}

template<>
inline void SpewKey<DmeTime_t>( const DmeTime_t& val )
{
	Msg( "%d", val.GetTenthsOfMS() );
}

template<>
inline void SpewKey<bool>( const bool& val )
{
	Msg( "%s", val ? "true" : "false" );
}

template<>
inline void SpewKey<Color>( const Color& val )
{
	Msg( "%08x", val.GetRawColor() );
}

template< >
inline void SpewKey( const Vector& val )
{
	Msg( "[%f %f %f]", val.x, val.y, val.z );
}

template< >
inline void SpewKey( const Quaternion& val )
{
	Msg( "[%f %f %f %f]", val.x, val.y, val.z, val.w );
}

template< class T >
static void SpewFlattenedKey( CDmeTypedLogLayer< T > *pLogLayer, ActiveLayer_t< T > *pActiveLayer, DmeTime_t t, const T& val )
{
	Msg( "Layer %d:  adding key at time %f [%d -> %d], value ", 
		pActiveLayer->priority, t.GetSeconds(), pActiveLayer->firstTime.GetTenthsOfMS(), pActiveLayer->lastTime.GetTenthsOfMS() );
	SpewKey( val );
	Msg( "\n" );
}

template< class T >
static void ComputeLayerEvents( CDmeTypedLog< T >* pLog, 
	CUtlVector< ActiveLayer_t< T > > &layerlist, 
	CUtlRBTree< LayerEvent_t< T > > &events )
{
	// Build a list of all known layers and a sorted list of layer "transitions"
	for ( int i = 0; i < pLog->GetNumLayers(); ++i )
	{
		ActiveLayer_t< T > layer;
		layer.priority = i;
		layer.layer = static_cast< CDmeTypedLogLayer< T > * >( pLog->GetLayer( i ) );
		layer.firstTime = layer.layer->GetBeginTime();
		layer.lastTime = layer.layer->GetEndTime();
		layer.bValid = true;

		if ( ( layer.firstTime == DMETIME_MINTIME || layer.lastTime == DMETIME_MAXTIME ) && ( i > 0 ) ) // Base layer is always valid
		{
			layer.bValid = false;
		}

		// Skip invalid layers
		if ( !layer.bValid )
			continue;

		// Layer zero can capture everything from above...
		if ( i == 0 )
		{
			layer.firstTime = DmeTime_t::MinTime();
			layer.lastTime = DmeTime_t::MaxTime();
		}

		// Add layer to global list
		int nIndex = layerlist.AddToTail( layer );

		// Add layer start/end events
		DmeTime_t tNeighbor = ( layer.firstTime != DMETIME_MINTIME ) ? ( layer.firstTime - DMETIME_MINDELTA ) : DMETIME_MINTIME;
		LayerEvent_t< T > start;
		start.m_pList = &layerlist;
		start.m_nLayer = nIndex;
		start.m_Type = LayerEvent_t< T >::LE_START;
		start.m_Time = layer.firstTime;
		start.m_NeighborValue = GetActiveLayerValue( layerlist, tNeighbor, nIndex - 1 );
		events.Insert( start );

		tNeighbor = ( layer.lastTime != DMETIME_MAXTIME ) ? ( layer.lastTime + DMETIME_MINDELTA ) : DMETIME_MAXTIME;
		LayerEvent_t< T > end;
		end.m_pList = &layerlist;
		end.m_nLayer = nIndex;
		end.m_Type = LayerEvent_t< T >::LE_END;
		end.m_Time = layer.lastTime;
		end.m_NeighborValue = GetActiveLayerValue( layerlist, tNeighbor, nIndex - 1 );
		events.Insert( end );
	}
}


template< class T >
static void AddDiscontinitySample( CDmeTypedLogLayer< T > *pTargetLayer, CDmeTypedLog< T > *pLog, DmeTime_t tKeyTime, const T& val, const char *pSpewLabel )
{
	// Finally, add a helper key
	if ( pLog->IsUsingCurveTypes() )
	{
		if ( pSpewLabel )
		{
			Msg( "Adding %s helper key at %d value ", pSpewLabel, tKeyTime.GetTenthsOfMS() );
			SpewKey( val );
			Msg( " [curvetype %s]\n", Interpolator_NameForCurveType( pLog->GetDefaultCurveType(), false ) );
		}
		pTargetLayer->SetKey( tKeyTime, val, pLog->GetDefaultCurveType() );
	}
	else
	{
		if ( pSpewLabel )
		{
			Msg( "Adding %s helper key at %d value ", pSpewLabel, tKeyTime.GetTenthsOfMS() );
			SpewKey( val );
			Msg( "\n" );
		}
		pTargetLayer->SetKey( tKeyTime, val );
	}
}


template< class T >
static DmeTime_t ProcessStartLayerStartEvent( 
	bool bSpew,
	bool bFixupDiscontinuities,
	CDmeTypedLog< T > *pLog, 
	LayerEvent_t< T > *pEvent, 
	CUtlVector< ActiveLayer_t< T > > &layerlist, 
	CUtlRBTree< ActiveLayer_t< T > * > &active, 
	CDmeTypedLogLayer< T > *flattenedlayer )
{
	Assert( pEvent->m_Type == LayerEvent_t< T >::LE_START );

	// Push it onto the active stack if it's not already on the stack
	if ( active.Find( &layerlist[ pEvent->m_nLayer ] ) != active.InvalidIndex() )
		return pEvent->m_Time;

	if ( bSpew )
	{
		Msg( "adding layer %d to stack\n", layerlist[ pEvent->m_nLayer ].priority );
	}
	active.Insert( &layerlist[ pEvent->m_nLayer ] );

	if ( !bFixupDiscontinuities || ( pEvent->m_Time == DMETIME_MINTIME ) )
		return pEvent->m_Time;

	// We'll need to add 2 new "discontinuity" fixup samples.
	// 1) A sample from the base layer @ start time - .1 msec
	// 2) A sample from the new layer @ start time
	int nActiveCount = active.Count();
	if ( nActiveCount >= 2 )
	{
		DmeTime_t tKeyTime = pEvent->m_Time - DmeTime_t( 1 );
		AddDiscontinitySample( flattenedlayer, pLog, tKeyTime, pEvent->m_NeighborValue, bSpew ? "start" : NULL ); 
	}
	AddDiscontinitySample( flattenedlayer, pLog, pEvent->m_Time, GetActiveLayerValue( layerlist, pEvent->m_Time, pEvent->m_nLayer ), bSpew ? "start" : NULL );
	return pEvent->m_Time;
}

template< class T >
static DmeTime_t ProcessStartLayerEndEvent( 
	bool bSpew,
	bool bFixupDiscontinuities,
	CDmeTypedLog< T > *pLog, 
	LayerEvent_t< T > *pEvent, 
	CUtlVector< ActiveLayer_t< T > > &layerlist, 
	CUtlRBTree< ActiveLayer_t< T > * > &active, 
	CDmeTypedLogLayer< T > *pBaseLayer )
{
	Assert( pEvent->m_Type == LayerEvent_t< T >::LE_END );

	// Push it onto the active stack if it's not already on the stack
	if ( bSpew )
	{
		Msg( "removing layer %d from stack\n", layerlist[ pEvent->m_nLayer ].priority );
	}

	// We'll need to add a "discontinuity" fixup sample from the 
	// 1) A sample from the ending layer @ start time
	// 2) A sample from the new layer @ start time + .1 msec 
	// NOTE: This will cause problems if there are non-default value keys at max time
	Assert( active.Count() >= 1 );
	if ( bFixupDiscontinuities && ( pEvent->m_Time != DMETIME_MAXTIME ) )
	{
		AddDiscontinitySample( pBaseLayer, pLog, pEvent->m_Time, GetActiveLayerValue( layerlist, pEvent->m_Time, pEvent->m_nLayer ), bSpew ? "end" : NULL );
		if ( active.Count() >= 2 )
		{
			DmeTime_t keyTime = pEvent->m_Time + DmeTime_t( 1 );
			AddDiscontinitySample( pBaseLayer, pLog, keyTime, pEvent->m_NeighborValue, bSpew ? "end" : NULL ); 
		}
	}

	active.Remove( &layerlist[ pEvent->m_nLayer ] );
	return ( active.Count() >= 2 ) ? pEvent->m_Time + DmeTime_t( 1 ) : pEvent->m_Time;
}

template< class T >
void CDmeTypedLog< T >::FlattenLayers( float threshold, int flags )
{
	// Already flattened
	if ( m_Layers.Count() <= 1 )
		return;

	if ( g_pDataModel->UndoEnabledForElement( this ) )
	{
		CUndoFlattenLayers<T> *pUndo = new CUndoFlattenLayers<T>( "FlattenLayers", this, threshold, flags );
		g_pDataModel->AddUndoElement( pUndo );
	}

	bool bSpew = ( flags & FLATTEN_SPEW ) != 0;
	bool bFixupDiscontinuities = true; //( flags & FLATTEN_NODISCONTINUITY_FIXUP ) == 0;

	// NOTE:  UNDO IS DISABLED FOR THE REST OF THIS OPERATION (the above function does what we need to preserve the layers)
	CDisableUndoScopeGuard guard;

	CDmeTypedLogLayer< T > *flattenedlayer = static_cast< CDmeTypedLogLayer< T > * >( CreateLayer< T >( this ) );
	flattenedlayer->SetOwnerLog( this );

	// Global list of layers
	CUtlVector< ActiveLayer_t< T > > layerlist;
	// List of all start/end layer events, sorted by the time at which the event occurs ( we walk this list in order )
	CUtlRBTree< LayerEvent_t< T > >	 events( 0, 0, LayerEvent_t< T >::LessFunc );
	// Stack of active events, sorted by event "priority", which means last item is the one writing data into the new base layer
	CUtlRBTree< ActiveLayer_t< T > * > active( 0, 0, ActiveLayer_t< T >::PriorityLessFunc );

	// Build layer list and list of start/end events and times
	ComputeLayerEvents( this, layerlist, events );

	// Debuggins
	if ( bSpew )
	{
		SpewEvents( events );
	}

	// Now walk from the earliest time in any layer until the latest time, going key by key and checking if the active layer should change as we go

	DmeTime_t iCurrentKeyTime = DmeTime_t::MinTime();
	unsigned short idx = events.FirstInorder();
	while ( 1 )
	{
		if ( idx == events.InvalidIndex() )
			break;

		LayerEvent_t< T > *pEvent = &events[ idx ];

		switch ( pEvent->m_Type )
		{
		default:
			iCurrentKeyTime = pEvent->m_Time;
			Assert( 0 );
			break;
		case LayerEvent_t< T >::LE_START:
			iCurrentKeyTime = ProcessStartLayerStartEvent( bSpew, bFixupDiscontinuities, this, pEvent, layerlist, active, flattenedlayer );
			break;

		case LayerEvent_t< T >::LE_END:
			iCurrentKeyTime = ProcessStartLayerEndEvent( bSpew, bFixupDiscontinuities, this, pEvent, layerlist, active, flattenedlayer );
			break;
		}

		int nNextIndex = events.NextInorder( idx );

		// We popped the last item off the stack
		if ( nNextIndex == events.InvalidIndex() )
		{
			Assert( active.Count() == 0 );
			break;
		}

		// Walk from current time up to the time of the next relevant event
		LayerEvent_t< T > *nextevent = &events[ nNextIndex ];
		DmeTime_t layerFinishTime = nextevent->m_Time;

		// The topmost layer is the active layer
		int layernum = active.LastInorder();
		if ( layernum == active.InvalidIndex() )
			break;

		ActiveLayer_t< T > *activeLayer = active[ layernum ];
		CDmeTypedLogLayer< T > *loglayer = activeLayer->layer;

		// Splat all keys betweeen the current head position and the next event time (layerFinishTime) into the flattened layer
		int keyCount = loglayer->GetKeyCount();
		for ( int j = 0; j < keyCount; ++j )
		{
			DmeTime_t keyTime = loglayer->GetKeyTime( j );
			// Key is too early, skip
			if ( keyTime < iCurrentKeyTime )
				continue;

			// Done with this layer, set time exactly equal to end time so next layer can take over 
			//  at the correct spot
			if ( keyTime >= layerFinishTime )
			{
				iCurrentKeyTime = layerFinishTime;
				break;
			}

			// Advance the head position
			iCurrentKeyTime = keyTime;

			// Because it's a key, the interpolated value should == the actual value (not true for certain 4 point curve types, but we shouldn't support them
			//  for this type of operation anyway)
			const T& val = loglayer->GetKeyValue( j );

			// Debugging spew
			if ( bSpew )
			{
				SpewFlattenedKey( loglayer, activeLayer, iCurrentKeyTime, val );
			}

			// Now set the key into the flattened layer
			flattenedlayer->SetKey( iCurrentKeyTime, val, loglayer->IsUsingCurveTypes() ? loglayer->GetKeyCurveType( j ) : CURVE_DEFAULT );
		}
		idx = nNextIndex;
	}

	// Blow away all of the existing layers except the original base layer
	while ( GetNumLayers() > 1 )
	{
		CDmeTypedLogLayer< T > *layer = static_cast< CDmeTypedLogLayer< T > * >( RemoveLayerFromTail() );
		g_pDataModel->DestroyElement( layer->GetHandle() );
	}

	// Compress the flattened layer
	flattenedlayer->RemoveRedundantKeys( threshold );
	
	// Copy the flattened layer over the existing base layer
	GetLayer( 0 )->CopyLayer( flattenedlayer );

	g_pDataModel->DestroyElement( flattenedlayer->GetHandle() );
}

template< class T >
void CDmeTypedLog< T >::StampKeyAtHead( DmeTime_t tHeadPosition, DmeTime_t tPreviousHeadPosition, const DmeLog_TimeSelection_t& params, const CDmAttribute *pAttr, uint index /*= 0*/ )
{
	DmAttributeType_t type = pAttr->GetType();
	if ( IsValueType( type ) )
	{
		Assert( pAttr->GetType() == GetDataType() );
		StampKeyAtHead( tHeadPosition, tPreviousHeadPosition, params, pAttr->GetValue< T >() );
	}
	else if ( IsArrayType( type ) )
	{
		Assert( ArrayTypeToValueType( type ) == GetDataType() );
		CDmrArrayConst<T> array( pAttr );
		StampKeyAtHead( tHeadPosition, tPreviousHeadPosition, params, array[ index ] );
	}
	else
	{
		Assert( 0 );
	}
}

template< class T >
void CDmeTypedLog< T >::FinishTimeSelection( DmeTime_t tHeadPosition, DmeLog_TimeSelection_t& params )
{
	bool bWasAdvancing = params.IsTimeAdvancing();
	params.ResetTimeAdvancing();

	if ( !params.m_bAttachedMode )
		return;

	if ( !bWasAdvancing )
		return;

	// Should be in "layer recording" mode!!!
	Assert( GetNumLayers() >= 2 );
	int nBestLayer = GetTopmostLayer(); // Topmost should be at least layer # 1 (0 is the base layer)
	if ( nBestLayer < 1 )
		return;

	CDmeTypedLogLayer< T > *pWriteLayer = GetLayer( nBestLayer );
	Assert( pWriteLayer );
	if ( !pWriteLayer )
		return;

	int nKeyCount = pWriteLayer->GetKeyCount();
	if ( nKeyCount <= 0 )
		return;

	// The head is considered to be at the "last" value
 	T headValue = pWriteLayer->GetKeyValue( nKeyCount - 1 );
	_StampKeyAtHeadResample( tHeadPosition, params, headValue, true, false ); 
}

template< >
float CDmeTypedLog< float >::ClampValue( const float& value )
{
	float retval;
	if ( !IsUsingCurveTypes() )
	{
		retval = clamp( value, 0.0f, 1.0f );
	}
	else
	{
		retval = clamp( value, GetMinValue(), GetMaxValue() );
	}
	return retval;
}

template< class T >
void CDmeTypedLog< T >::StampKeyAtHead( DmeTime_t tHeadPosition, DmeTime_t tPreviousHeadPosition, const DmeLog_TimeSelection_t& params, const T& value )
{
	//T useValue = ClampValue( value );

	// This gets set if time ever starts moving (even if the user pauses time while still holding a slider)
	if ( params.IsTimeAdvancing() )
	{
		// This uses the time selection as a "filter" to decide whether to stamp a new key at the current position
		_StampKeyAtHeadFilteredByTimeSelection( tHeadPosition, tPreviousHeadPosition, params, value );
	}
	else
	{
		Assert( params.m_bResampleMode );
		_StampKeyAtHeadResample( tHeadPosition, params, value, false, true );
	}
}

/*
template<>
void CDmeTypedLog< float >::_StampKeyAtHeadResample( const DmeLog_TimeSelection_t& params, const float& value );
template<>
void CDmeTypedLog< bool >::_StampKeyAtHeadResample( const DmeLog_TimeSelection_t& params, const bool& value );
template<>
void CDmeTypedLog< Color >::_StampKeyAtHeadResample( const DmeLog_TimeSelection_t& params, const Color& value );
template<>
void CDmeTypedLog< Vector4D >::_StampKeyAtHeadResample( const DmeLog_TimeSelection_t& params, const Vector4D& value );
template<>
void CDmeTypedLog< Vector2D >::_StampKeyAtHeadResample( const DmeLog_TimeSelection_t& params, const Vector2D& value );
template<>
void CDmeTypedLog< VMatrix >::_StampKeyAtHeadResample( const DmeLog_TimeSelection_t& params, const VMatrix& value );
template<>
void CDmeTypedLog< Quaternion >::_StampKeyAtHeadResample( const DmeLog_TimeSelection_t& params, const Quaternion& value );
template<>
void CDmeTypedLog< QAngle >::_StampKeyAtHeadResample( const DmeLog_TimeSelection_t& params, const QAngle& value );
*/


//-----------------------------------------------------------------------------
// Helper class used to compute falloff blend factors
//-----------------------------------------------------------------------------
template< class T >
struct LogClampHelper_t
{
public:
	LogClampHelper_t() : m_tLastTime( DMETIME_MINTIME ) {}

	DmeTime_t m_tLastTime;
	T m_LastUnclampedValue;
};


template< class T >
class CLogFalloffBlend
{
public:
	void Init( CDmeTypedLog<T> *pLog, DmeTime_t tFalloff, DmeTime_t tHold, bool bLeftFalloff, int nInterpolatorType, const T& delta );
	void Init( CDmeTypedLog<T> *pLog, const T& delta );
	float ComputeBlendFactor( DmeTime_t tTime, const T& oldVal, bool bUsingInterpolation ) const;
	const T& GetDelta() const;
	void StampKey( CDmeTypedLogLayer<T>* pWriteLayer, DmeTime_t t, const CDmeTypedLogLayer<T>* pReadLayer, float flIntensity, LogClampHelper_t<T> &helper, bool bSpew, const T* pInterpTarget );
	void UpdateClampHelper( DmeTime_t t, const CDmeTypedLogLayer<T>* pReadLayer, float flIntensity, LogClampHelper_t<T> &helper, const T* pInterpTarget );

private:
	void ComputeDelta( CDmeTypedLog<T> *pLog, const T& delta, const T& holdValue );
	void InsertClampTransitionPoints( CDmeTypedLogLayer<T>* pWriteLayer, DmeTime_t t, LogClampHelper_t<T> &clampHelper, const T& val, bool bSpew );
	void ComputeBounds( CDmeTypedLog<T> *pLog );

	T m_BaseValue;
	T m_Delta;
	DmeTime_t m_tBaseTime;
	DmeTime_t m_tHoldTime;
	float m_flOOTime;
	int m_nTestSign;
	int m_nInterpolatorType;
	int m_nCurveType;
	T m_MinValue;
	T m_MaxValue;
	bool m_bHold;
};

template< class T >
void CLogFalloffBlend< T >::Init( CDmeTypedLog<T> *pLog, DmeTime_t tFalloffTime, DmeTime_t tHoldTime, bool bLeftFalloff, int nInterpolatorType, const T& delta )
{
	m_tBaseTime = tFalloffTime;
	m_tHoldTime = tHoldTime;
	m_BaseValue = pLog->GetValueSkippingTopmostLayer( tFalloffTime );
	T holdValue = pLog->GetValueSkippingTopmostLayer( tHoldTime );
	m_nTestSign = bLeftFalloff ? 1 : -1;
	m_nInterpolatorType = nInterpolatorType;
	m_bHold = false;
	m_nCurveType = pLog->IsUsingCurveTypes() ? pLog->GetDefaultCurveType() : CURVE_DEFAULT;

	float flDuration = tHoldTime.GetSeconds() - tFalloffTime.GetSeconds();
	m_flOOTime = ( flDuration != 0.0f ) ? 1.0f / flDuration : 0.0f;
	ComputeBounds( pLog );
	ComputeDelta( pLog, delta, holdValue );
}

template< class T >
void CLogFalloffBlend< T >::Init( CDmeTypedLog<T> *pLog, const T& delta )
{
	m_nTestSign = 0;
	m_nInterpolatorType = INTERPOLATE_DEFAULT;
	m_bHold = true;
	m_nCurveType = pLog->IsUsingCurveTypes() ? pLog->GetDefaultCurveType() : CURVE_DEFAULT;
	m_Delta = delta;
	ComputeBounds( pLog );
}

template< class T >
void CLogFalloffBlend< T >::ComputeBounds( CDmeTypedLog<T> *pLog )
{
}

template<>
void CLogFalloffBlend< float >::ComputeBounds( CDmeTypedLog<float> *pLog )
{
	m_MinValue = pLog->IsUsingCurveTypes() ? pLog->GetMinValue() : 0.0f;
	m_MaxValue = pLog->IsUsingCurveTypes() ? pLog->GetMaxValue() : 1.0f;
}

template< class T >
void CLogFalloffBlend< T >::ComputeDelta( CDmeTypedLog<T> *pLog, const T& delta, const T& holdValue )
{
	// By default, no clamping
	m_Delta = delta;
}

template<>
void CLogFalloffBlend< float >::ComputeDelta( CDmeTypedLog<float> *pLog, const float& delta, const float& holdValue )
{
	if ( LengthOf( delta ) > 0.0f )
	{
		m_Delta = min( delta, m_MaxValue - holdValue ); // Max amount we can move up...
	}
	else
	{
		m_Delta = max( delta, m_MinValue - holdValue );  // Amount we can move down...
	}
}


template< class T >
float CLogFalloffBlend< T >::ComputeBlendFactor( DmeTime_t tTime, const T& oldVal, bool bUsingInterpolation ) const
{
	if ( m_bHold )
		return 1.0f;

	// Clamp inside region; hold time beats base time (for zero width regions)
	if ( ( tTime - m_tHoldTime ) * m_nTestSign >= DMETIME_ZERO )
		return 1.0f;

	if ( ( tTime - m_tBaseTime ) * m_nTestSign <= DMETIME_ZERO )
		return 0.0f;

	float flFactor = ( tTime.GetSeconds() - m_tBaseTime.GetSeconds() ) * m_flOOTime;
	return ComputeInterpolationFactor( flFactor, m_nInterpolatorType );
}

template< class T >
const T& CLogFalloffBlend< T >::GetDelta( ) const
{
	return m_Delta;
}


//-----------------------------------------------------------------------------
// Insert points where clamping begins or ends
//-----------------------------------------------------------------------------
template< class T >
void CLogFalloffBlend< T >::InsertClampTransitionPoints( CDmeTypedLogLayer<T>* pWriteLayer, 
	DmeTime_t t, LogClampHelper_t<T> &clampHelper, const T& val, bool bSpew )
{
	// NOTE: By default, nothing clamps, so no transition points are needed
}

template<>
void CLogFalloffBlend< float >::InsertClampTransitionPoints( CDmeTypedLogLayer<float>* pWriteLayer, 
	DmeTime_t t, LogClampHelper_t<float> &clampHelper, const float& val, bool bSpew )
{
	bool bLastLess, bLastGreater, bCurrLess, bCurrGreater;
	DmeTime_t tCrossing, tDuration;
	double flOODv;

	// First time through? cache last values.
	if ( clampHelper.m_tLastTime == DMETIME_MINTIME )
		goto cacheLastValues;

	bLastLess = clampHelper.m_LastUnclampedValue < m_MinValue; 
	bLastGreater = clampHelper.m_LastUnclampedValue > m_MaxValue; 
	bCurrLess = val < m_MinValue; 
	bCurrGreater = val > m_MaxValue;
	if ( bLastLess == bCurrLess && bLastGreater == bCurrGreater )
		goto cacheLastValues;

	// NOTE: The check above means val != m_LastUnclampedValue
	flOODv = 1.0 / ( val - clampHelper.m_LastUnclampedValue );
	tDuration = t - clampHelper.m_tLastTime;

	// NOTE: Clamp semantics here favor keeping the non-clamped value
	// That's why when we start outside + end inside, we never overwrite the dest
	// and why when we start inside + end outside, we never overwrite the start
	// These two checks deal with starting outside + heading inside
	if ( bLastLess && !bCurrLess )
	{
		// Insert at min crossing
		double flFactor = ( m_MinValue - clampHelper.m_LastUnclampedValue ) * flOODv;
		tCrossing = clampHelper.m_tLastTime + tDuration * flFactor;
		tCrossing.Clamp( clampHelper.m_tLastTime, t - DMETIME_MINDELTA );
		pWriteLayer->InsertKey( tCrossing, m_MinValue, m_nCurveType );
		if ( bSpew )
		{
			Msg("	Clamp Crossing Key: %d %f\n", tCrossing.GetTenthsOfMS(), m_MinValue );
		}
	}
	else if ( bLastGreater && !bCurrGreater )
	{
		// Insert at max crossing
		double flFactor = ( m_MaxValue - clampHelper.m_LastUnclampedValue ) * flOODv;
		tCrossing = clampHelper.m_tLastTime + tDuration * flFactor;
		tCrossing.Clamp( clampHelper.m_tLastTime, t - DMETIME_MINDELTA );
		pWriteLayer->InsertKey( tCrossing, m_MaxValue, m_nCurveType );
		if ( bSpew )
		{
			Msg("	Clamp Crossing Key: %d %f\n", tCrossing.GetTenthsOfMS(), m_MaxValue );
		}
	}

	// These two checks deal with starting inside + heading outside
	if ( !bLastLess && bCurrLess )
	{
		// Insert at min crossing
		// NOTE: Clamp semantics here favor keeping the non-clamped value
		double flFactor = ( m_MinValue - clampHelper.m_LastUnclampedValue ) * flOODv;
		tCrossing = clampHelper.m_tLastTime + tDuration * flFactor;
		tCrossing.Clamp( clampHelper.m_tLastTime + DMETIME_MINDELTA, t );
		pWriteLayer->InsertKey( tCrossing, m_MinValue, m_nCurveType );
		if ( bSpew )
		{
			Msg("	Clamp Crossing Key: %d %f\n", tCrossing.GetTenthsOfMS(), m_MinValue );
		}
	}
	else if ( !bLastGreater && bCurrGreater )
	{
		// Insert at max crossing
		double flFactor = ( m_MaxValue - clampHelper.m_LastUnclampedValue ) * flOODv;
		tCrossing = clampHelper.m_tLastTime + tDuration * flFactor;
		tCrossing.Clamp( clampHelper.m_tLastTime + DMETIME_MINDELTA, t );
		pWriteLayer->InsertKey( tCrossing, m_MaxValue, m_nCurveType );
		if ( bSpew )
		{
			Msg("	Clamp Crossing Key: %d %f\n", tCrossing.GetTenthsOfMS(), m_MaxValue );
		}
	}

	// Cache off the last values
cacheLastValues:
	clampHelper.m_tLastTime = t;
	clampHelper.m_LastUnclampedValue = val;
}



//-----------------------------------------------------------------------------
// Stamp the key at the specified time
//-----------------------------------------------------------------------------
template< class T >
void CLogFalloffBlend< T >::StampKey( CDmeTypedLogLayer<T>* pWriteLayer, DmeTime_t t, 
	const CDmeTypedLogLayer<T>* pReadLayer, float flIntensity, LogClampHelper_t<T> &clampHelper, bool bSpew, const T* pInterpTarget )
{
	// Stamp the key at the current time
	T oldVal = pReadLayer->GetValue( t );

	// In the falloff area
	float flFactor = ComputeBlendFactor( t, oldVal, ( pInterpTarget != NULL ) );
	flFactor *= flIntensity;

	T newVal;
	if ( !pInterpTarget )
	{
		newVal = ScaleValue( m_Delta, flFactor );
		newVal = Add( oldVal, newVal );
	}
	else
	{
		newVal = Interpolate( flFactor, oldVal, *pInterpTarget );
	}

	InsertClampTransitionPoints( pWriteLayer, t, clampHelper, newVal, bSpew );

	T clampedVal = pWriteLayer->GetTypedOwnerLog()->ClampValue( newVal );

	// Add a key to the new "layer" at this time with this value
	pWriteLayer->InsertKey( t, clampedVal, m_nCurveType );

	if ( bSpew )
	{
		Msg("	Key: %d ", t.GetTenthsOfMS() );
		SpewKey( clampedVal );
		Msg(" [" );
		SpewKey( newVal );
		Msg( "]\n" );
	}
}


//-----------------------------------------------------------------------------
// Stamp the key at the specified time
//-----------------------------------------------------------------------------
template< class T >
void CLogFalloffBlend< T >::UpdateClampHelper( DmeTime_t t, const CDmeTypedLogLayer<T>* pReadLayer, 
	float flIntensity, LogClampHelper_t<T> &clampHelper, const T* pInterpTarget )
{
	// Stamp the key at the current time
	T oldVal = pReadLayer->GetValue( t );

	// In the falloff area
	float flFactor = ComputeBlendFactor( t, oldVal, ( pInterpTarget != NULL ) );
	flFactor *= flIntensity;

	T val;
	if ( !pInterpTarget )
	{
		val = ScaleValue( m_Delta, flFactor );
		val = Add( oldVal, val );
	}
	else
	{
		val = Interpolate( flFactor, oldVal, *pInterpTarget );
	}

	clampHelper.m_tLastTime = t;
	clampHelper.m_LastUnclampedValue = val;
}


//-----------------------------------------------------------------------------
// This is used to modify the entire portion of the curve under the time selection
//-----------------------------------------------------------------------------
static inline DmeTime_t ComputeResampleStartTime( const DmeLog_TimeSelection_t &params, int nSide )
{
	// NOTE: This logic will place the resampled points centered in the falloff regions
	DmeTimeSelectionTimes_t start = ( nSide == 0 ) ? TS_LEFT_FALLOFF : TS_RIGHT_HOLD;
	DmeTimeSelectionTimes_t end = ( nSide == 0 ) ? TS_LEFT_HOLD : TS_RIGHT_FALLOFF;

	if ( params.m_nFalloffInterpolatorTypes[nSide] != INTERPOLATE_LINEAR_INTERP )
	{
		DmeTime_t tDuration = params.m_nTimes[end] - params.m_nTimes[start];
		if ( tDuration > params.m_nResampleInterval )
		{
			int nFactor = tDuration.GetTenthsOfMS() / params.m_nResampleInterval.GetTenthsOfMS();
			tDuration -= params.m_nResampleInterval * nFactor;
			tDuration /= 2;
			return params.m_nTimes[start] + tDuration;
		}
	}
	return DMETIME_MAXTIME;
}


//-----------------------------------------------------------------------------
// This is used to modify the entire portion of the curve under the time selection
//-----------------------------------------------------------------------------
template< class T >
void CDmeTypedLog< T >::_StampKeyAtHeadResample( DmeTime_t tHeadPosition, const DmeLog_TimeSelection_t& params, const T& value, bool bSkipToHead, bool bClearPreviousKeys )
{
	Assert( params.m_nResampleInterval > DmeTime_t( 0 ) );
	if ( params.m_nResampleInterval < DmeTime_t( 0 ) )
		return;

	// Should be in "layer recording" mode!!!
	Assert( GetNumLayers() >= 2 );
	int nBestLayer = GetTopmostLayer(); // Topmost should be at least layer # 1 (0 is the base layer)
	if ( nBestLayer < 1 )
		return;
	CDmeTypedLogLayer< T > *pWriteLayer = GetLayer( nBestLayer );
	Assert( pWriteLayer );
	if ( !pWriteLayer )
		return;

	if ( bClearPreviousKeys )
	{
		pWriteLayer->ClearKeys();
	}

	bool bSpew = false;

	// NOTE: The headDelta is only used when not blending toward a preset
	// When not blending toward a preset, just add the head delta onto everything.
	// When blending toward a preset, lerp towards the preset.
	T oldHeadValue = GetValueSkippingTopmostLayer( tHeadPosition );
	T headDelta = Subtract( value, oldHeadValue );

	// When dragging preset fader, eveything get's blended in by the amount of the preset being applied
	bool bUsePresetRules = ( RECORD_PRESET == params.GetRecordingMode() );
	bool bIsStampingQuaternions = ( CDmAttributeInfo<T>::ATTRIBUTE_TYPE == AT_QUATERNION );
	bool bPerformInterpolation = bUsePresetRules || bIsStampingQuaternions; 

	// FIXME: Preset value should never be NULL. We need to grab it from the attribute
	bool bUsePresetValue = bUsePresetRules && params.m_pPresetValue && params.m_pPresetValue->GetType() == CDmAttributeInfo<T>::ATTRIBUTE_TYPE;
	const T& interpTarget = bUsePresetValue ? params.m_pPresetValue->GetValue<T>() : value;

	// Compute falloff region blend factors
	CLogFalloffBlend< T > blend[ 3 ];
	blend[0].Init( this, params.m_nTimes[ TS_FALLOFF(0) ], params.m_nTimes[ TS_HOLD(0) ], true, params.m_nFalloffInterpolatorTypes[0], headDelta );
	blend[1].Init( this, headDelta );
	blend[2].Init( this, params.m_nTimes[ TS_FALLOFF(1) ], params.m_nTimes[ TS_HOLD(1) ], false, params.m_nFalloffInterpolatorTypes[1], headDelta );

	// The algorithm we're going to use is to add samples in the following places:
	// 1) At each time selection transition point (start, end of falloff regions)
	//	  NOTE: If a falloff region has 0 size, we'll add points right outside the transition
	// 2) At the resample point (we're going to base this so the resamples always occur at the same spots)
	// 3) At any existing sample position
	// 4) Any time we switch from clamped to not clamped
	// By doing this, we will guarantee no bogus slope changes

	// First, compute times for transition regions
	DmeTime_t tTransitionTimes[TS_TIME_COUNT];
	memcpy( &tTransitionTimes, &params.m_nTimes, sizeof(params.m_nTimes) );
	if ( tTransitionTimes[TS_LEFT_FALLOFF] == tTransitionTimes[TS_LEFT_HOLD] )
	{
		tTransitionTimes[TS_LEFT_FALLOFF] -= DMETIME_MINDELTA;
	}
	if ( tTransitionTimes[TS_RIGHT_FALLOFF] == tTransitionTimes[TS_RIGHT_HOLD] )
	{
		tTransitionTimes[TS_RIGHT_FALLOFF] += DMETIME_MINDELTA;
	}

	DmeTime_t tStartTime = params.m_nTimes[ TS_LEFT_FALLOFF ];

	// Next, compute the first resample time for each region
	DmeTime_t tResampleStartTime[TS_TIME_COUNT];
	tResampleStartTime[TS_LEFT_FALLOFF] = DMETIME_MAXTIME;
	tResampleStartTime[TS_LEFT_HOLD] = ComputeResampleStartTime( params, 0 );
	tResampleStartTime[TS_RIGHT_HOLD] = DMETIME_MAXTIME;
	tResampleStartTime[TS_RIGHT_FALLOFF] = ComputeResampleStartTime( params, 1 );

	// Finally, figure out which layer we're reading from, 
	// where the next key is, and when we must stop reading from it
	int nReadLayer = FindLayerForTimeSkippingTopmost( tStartTime );
	CDmeTypedLogLayer< T > *pReadLayer = GetLayer( nReadLayer );
	int nLayerSampleIndex = pReadLayer->FindKey( tStartTime ) + 1;
	DmeTime_t tLayerEndTime = pReadLayer->GetEndTime();
	// NOTE: This can happen after reading off the end of layer 0
	if ( tLayerEndTime <= tStartTime )
	{
		tLayerEndTime = DMETIME_MAXTIME;
	}
	DmeTime_t tNextSampleTime = nLayerSampleIndex >= pReadLayer->GetKeyCount() ? tLayerEndTime : pReadLayer->GetKeyTime( nLayerSampleIndex );
	if ( tNextSampleTime > tLayerEndTime )
	{
		tNextSampleTime = tLayerEndTime;
	}

	// Now keep going until we've hit the end point
	// NOTE: We use tTransitionTimes, *not* params.m_nTimes, so that we can get a single
	// sample before zero-width left falloff regions
	DmeTime_t tCurrent = tTransitionTimes[TS_LEFT_FALLOFF];
	int nNextTransition = TS_LEFT_HOLD;
	DmeTime_t tResampleTime = tResampleStartTime[nNextTransition];

	const T* pInterpTarget = bPerformInterpolation ? &interpTarget : NULL;

	if ( bSpew )
	{
		Msg( "Stamp key at head resample: %s\n", GetName() );
	}

	LogClampHelper_t<T> clampHelper;
	while( nNextTransition < TS_TIME_COUNT )
	{
		// Stamp the key at the current time
		if ( !bSkipToHead || ( tCurrent >= tHeadPosition ) )
		{
			blend[nNextTransition-1].StampKey( pWriteLayer, tCurrent, pReadLayer, params.m_flIntensity, clampHelper, bSpew, pInterpTarget );
		}

		// Update the read layer sample
		if ( tCurrent == tNextSampleTime )
		{
			++nLayerSampleIndex;
			tNextSampleTime = nLayerSampleIndex >= pReadLayer->GetKeyCount() ? tLayerEndTime : pReadLayer->GetKeyTime( nLayerSampleIndex );
		}

		// Update the read layer
		if ( tCurrent == tLayerEndTime )
		{
			nReadLayer = FindLayerForTimeSkippingTopmost( tCurrent + DMETIME_MINDELTA );
			pReadLayer = GetLayer( nReadLayer );
			nLayerSampleIndex = pReadLayer->FindKey( tCurrent ) + 1;
			tLayerEndTime = pReadLayer->GetEndTime();

			// NOTE: This can happen after reading off the end of layer 0
			if ( tLayerEndTime <= tCurrent )
			{
				tLayerEndTime = DMETIME_MAXTIME;
			}

			tNextSampleTime = nLayerSampleIndex >= pReadLayer->GetKeyCount() ? tLayerEndTime : pReadLayer->GetKeyTime( nLayerSampleIndex );
			if ( tNextSampleTime > tLayerEndTime )
			{
				tNextSampleTime = tLayerEndTime;
			}
		}
					    
		// Update the transition time
		if ( tCurrent == tTransitionTimes[nNextTransition] )
		{
			// NOTE: This is necessary because each blend region has different 'deltas'
			// to avoid overdriving in the falloff regions. Therefore, the 'previous value'
			// used in the clamping operation will be different 
			if ( nNextTransition < ARRAYSIZE(blend) )
			{
				blend[nNextTransition].UpdateClampHelper( tCurrent, pReadLayer, params.m_flIntensity, clampHelper, pInterpTarget );
			}

			// Also need to update the 'previous' value stored in the 
			++nNextTransition;
			if ( nNextTransition >= ARRAYSIZE(tResampleStartTime) )
				break;

			// Update the first resample time
			tResampleTime = tResampleStartTime[nNextTransition];

			if ( bSpew )
			{
				Msg( "   Entering region %d\n", nNextTransition-1 );
			}
		}

		// Update the resample time
		if ( tCurrent == tResampleTime )
		{
			tResampleTime += params.m_nResampleInterval;
		}

		// Now that the key is stamped, update current time.
		tCurrent = tTransitionTimes[nNextTransition];
		if ( tResampleTime < tCurrent )
		{
			tCurrent = tResampleTime;
		}
		if ( tNextSampleTime < tCurrent )
		{
			tCurrent = tNextSampleTime;
		}
	}
}

						  
//-----------------------------------------------------------------------------
// In this case, we actually stamp a key right at the head position unlike the above method
//-----------------------------------------------------------------------------
template< class T > 
void CDmeTypedLog< T >::_StampKeyFilteredByTimeSelection( CDmeTypedLogLayer< T > *pWriteLayer, DmeTime_t t, const DmeLog_TimeSelection_t &params, const T& value, bool bForce )
{
	// Found a key which needs to be modulated upward
	float flFraction = params.GetAmountForTime( t ) * params.m_flIntensity;
	if ( flFraction <= 0.0f && !bForce )
		return;

	// When dragging preset fader, eveything get's blended in by the amount of the preset being applied
	bool bUsePresetRules = ( RECORD_PRESET == params.GetRecordingMode() );

	// FIXME: Preset value should never be NULL. We need to grab it from the attribute
	const T& interpTarget = ( bUsePresetRules && params.m_pPresetValue ) ? params.m_pPresetValue->GetValue<T>() : value;
	T oldVal = GetValueSkippingTopmostLayer( t );
	T newVal = Interpolate( flFraction, oldVal, interpTarget );
	T writeVal = ClampValue( newVal );
	pWriteLayer->InsertKey( t, writeVal, IsUsingCurveTypes() ? GetDefaultCurveType() : CURVE_DEFAULT ); 
}


//-----------------------------------------------------------------------------
// In this case, we actually stamp a key right at the head position unlike the above method
//-----------------------------------------------------------------------------
template< class T > 
void CDmeTypedLog< T >::_StampKeyAtHeadFilteredByTimeSelection( DmeTime_t tHeadPosition, DmeTime_t tPreviousHeadPosition, const DmeLog_TimeSelection_t &params, const T& value )
{
	// Should be in "layer recording" mode!!!
	Assert( GetNumLayers() >= 2 );
	int nBestLayer = GetTopmostLayer(); // Topmost should be at least layer # 1 (0 is the base layer)
	if ( nBestLayer < 1 )
		return;

	CDmeTypedLogLayer< T > *pWriteLayer = GetLayer( nBestLayer );
	Assert( pWriteLayer );
	if ( !pWriteLayer )
		return;

	// NOTE: This little trickery is necessary to generate samples right outside the
	// transition region in the case of zero length falloff regions
	DmeLog_TimeSelection_t tempParams = params;
	if ( tempParams.m_nTimes[TS_LEFT_FALLOFF] == tempParams.m_nTimes[TS_LEFT_HOLD] )
	{
		tempParams.m_nTimes[TS_LEFT_FALLOFF] -= DMETIME_MINDELTA;
	}
	if ( tempParams.m_nTimes[TS_RIGHT_FALLOFF] == tempParams.m_nTimes[TS_RIGHT_HOLD] )
	{
		tempParams.m_nTimes[TS_RIGHT_FALLOFF] += DMETIME_MINDELTA;
	}

	int nPrevRegion = tempParams.ComputeRegionForTime( tPreviousHeadPosition );
	int nCurrRegion = tempParams.ComputeRegionForTime( tHeadPosition );

	// Test for backward performance!
	if ( nCurrRegion < nPrevRegion )
	{
		V_swap( nCurrRegion, nPrevRegion );
	}

	// Insert samples at each transition point we skipped over
	for ( int i = nPrevRegion; i < nCurrRegion; ++i )
	{
		_StampKeyFilteredByTimeSelection( pWriteLayer, tempParams.m_nTimes[i], params, value, true );
	}
	
	_StampKeyFilteredByTimeSelection( pWriteLayer, tHeadPosition, params, value );
}

template< class T >
void CDmeTypedLog< T >::RemoveKeys( DmeTime_t starttime )
{
	int bestLayer = GetTopmostLayer();
	if ( bestLayer < 0 )
		return;

	GetLayer( bestLayer )->RemoveKeys( starttime );
}

template< class T >
void CDmeTypedLog< T >::RemoveKey( int nKeyIndex, int nNumKeysToRemove /*= 1*/ )
{
	int bestLayer = GetTopmostLayer();
	if ( bestLayer < 0 )
		return;

	GetLayer( bestLayer )->RemoveKey( nKeyIndex, nNumKeysToRemove );
}

template< class T >
void CDmeTypedLog< T >::ClearKeys()
{
	int bestLayer = GetTopmostLayer();
	if ( bestLayer < 0 )
		return;

	GetLayer( bestLayer )->ClearKeys();
}

//-----------------------------------------------------------------------------
// Returns a specific key's value
//-----------------------------------------------------------------------------
template< class T >
DmeTime_t CDmeTypedLog< T >::GetKeyTime( int nKeyIndex ) const
{
	int bestLayer = GetTopmostLayer();
	if ( bestLayer < 0 )
		return DmeTime_t::MinTime();
	return GetLayer( bestLayer )->GetKeyTime( nKeyIndex );
}

template< class T >
void CDmeTypedLog< T >::SetKeyTime( int nKeyIndex, DmeTime_t keyTime )
{
	int bestLayer = GetTopmostLayer();
	if ( bestLayer < 0 )
		return;
	return GetLayer( bestLayer )->SetKeyTime( nKeyIndex, keyTime );
}

//-----------------------------------------------------------------------------
// Returns the index of a particular key
//-----------------------------------------------------------------------------
template< class T >
int CDmeTypedLog< T >::FindKeyWithinTolerance( DmeTime_t nTime, DmeTime_t nTolerance )
{
	int bestLayer = GetTopmostLayer();
	if ( bestLayer < 0 )
		return -1;

	return GetLayer( bestLayer )->FindKeyWithinTolerance( nTime, nTolerance );
}

//-----------------------------------------------------------------------------
// tests whether two values differ by more than the threshold
//-----------------------------------------------------------------------------
template<>
bool CDmeTypedLog< Vector >::ValuesDiffer( const Vector& a, const Vector& b ) const
{
	return a.DistToSqr( b ) > m_threshold * m_threshold;
}

template<>
bool CDmeTypedLog< QAngle >::ValuesDiffer( const QAngle& a, const QAngle& b ) const
{
	return ( a - b ).LengthSqr() > m_threshold * m_threshold;
}

template<>
bool CDmeTypedLog< Quaternion >::ValuesDiffer( const Quaternion& a, const Quaternion& b ) const
{
	return QuaternionAngleDiff( a, b ) > m_threshold;
}

template<>
bool CDmeTypedLog< float >::ValuesDiffer( const float& a, const float& b ) const
{
	return fabs( a - b ) > m_threshold;
}

template< class T >
bool CDmeTypedLog< T >::ValuesDiffer( const T& a, const T& b ) const
{
	return a != b;
}

//-----------------------------------------------------------------------------
// Sets a key, removes all keys after this time
//-----------------------------------------------------------------------------
template< class T >
void CDmeTypedLog< T >::SetKey( DmeTime_t time, const T& value, int curveType /*=CURVE_DEFAULT*/)
{
	int bestLayer = GetTopmostLayer();
	if ( bestLayer < 0 )
		return;

	GetLayer( bestLayer )->SetKey( time, value, curveType );
}

template< class T >
CDmeTypedLogLayer< T > *CDmeTypedLog< T >::GetLayer( int index )
{
	if ( index < 0 )
		return NULL;

	return static_cast< CDmeTypedLogLayer< T > * >( m_Layers[ index ] );
}

template< class T >
const CDmeTypedLogLayer< T > *CDmeTypedLog< T >::GetLayer( int index ) const
{
	if ( index < 0 )
		return NULL;

	return static_cast< CDmeTypedLogLayer< T > * >( m_Layers[ index ] );
}


//-----------------------------------------------------------------------------
// Finds a key within tolerance, or adds one
//-----------------------------------------------------------------------------
template< class T >
int CDmeTypedLog< T >::FindOrAddKey( DmeTime_t nTime, DmeTime_t nTolerance, const T& value, int curveType /*=CURVE_DEFAULT*/ )
{
	int bestLayer = GetTopmostLayer();
	if ( bestLayer == -1 )
		return -1;

	return GetLayer( bestLayer )->FindOrAddKey( nTime, nTolerance, value, curveType );
}


//-----------------------------------------------------------------------------
// This inserts a key. Unlike SetKey, this will *not* delete keys after the specified time
//-----------------------------------------------------------------------------
template < class T >
int CDmeTypedLog< T >::InsertKey( DmeTime_t nTime, const T& value, int curveType /*=CURVE_DEFAULT*/ )
{
	int bestLayer = GetTopmostLayer();
	if ( bestLayer == -1 )
		return -1;

	return GetLayer( bestLayer )->InsertKey( nTime, value, curveType );
}

template < class T >
int CDmeTypedLog< T >::InsertKeyAtTime( DmeTime_t nTime, int curveType /*=CURVE_DEFAULT*/ )
{
	int bestLayer = GetTopmostLayer();
	if ( bestLayer == -1 )
		return -1;

	return GetLayer( bestLayer )->InsertKeyAtTime( nTime, curveType );
}

template< class T >
const T& CDmeTypedLog< T >::GetValue( DmeTime_t time ) const
{
	int bestLayer = FindLayerForTime( time );
	if ( bestLayer < 0 )
	{
		static T s_value;
		CDmAttributeInfo< T >::SetDefaultValue( s_value ); // TODO - create GetDefaultValue that returns a default T, to avoid rebuilding every time
		return s_value;
	}

	return GetLayer( bestLayer )->GetValue( time );
}

template< class T >
const T& CDmeTypedLog< T >::GetValueSkippingTopmostLayer( DmeTime_t time ) const
{
	int nLayer = FindLayerForTimeSkippingTopmost( time );
	if ( nLayer < 0 )
		return GetValue( time );
	return GetLayer( nLayer )->GetValue( time );
}

template< class T >
void CDmeTypedLog< T >::SetKey( DmeTime_t time, const CDmAttribute *pAttr, uint index, int curveType /*= CURVE_DEFAULT*/ )
{
	int bestLayer = GetTopmostLayer();
	if ( bestLayer == -1 )
		return;

	GetLayer( bestLayer )->SetKey( time, pAttr, index, curveType );
}

template< class T >
bool CDmeTypedLog< T >::SetDuplicateKeyAtTime( DmeTime_t time )
{
	int bestLayer = GetTopmostLayer();
	if ( bestLayer == -1 )
		return false;

	return GetLayer( bestLayer )->SetDuplicateKeyAtTime( time );
}


//-----------------------------------------------------------------------------
// Returns a specific key's value
//-----------------------------------------------------------------------------
template< class T >
const T& CDmeTypedLog< T >::GetKeyValue( int nKeyIndex ) const
{
	int bestLayer = GetTopmostLayer();
	if ( bestLayer == -1 )
	{
		static T s_value;
		CDmAttributeInfo< T >::SetDefaultValue( s_value ); // TODO - create GetDefaultValue that returns a default T, to avoid rebuilding every time
		return s_value;
	}

	return GetLayer( bestLayer )->GetKeyValue( nKeyIndex );
}

template< class T >
void CDmeTypedLog< T >::GetValue( DmeTime_t time, CDmAttribute *pAttr, uint index ) const
{
	int bestLayer = FindLayerForTime( time );
	if ( bestLayer < 0 )
	{
		T value;
		CDmAttributeInfo< T >::SetDefaultValue( value ); // TODO - create GetDefaultValue that returns a default T, to avoid rebuilding every time
		pAttr->SetValue( CDmAttributeInfo< T >::AttributeType(), &value );
	}

	return GetLayer( bestLayer )->GetValue( time, pAttr, index );
}

template< class T >
void CDmeTypedLog< T >::GetValueSkippingTopmostLayer( DmeTime_t time, CDmAttribute *pAttr, uint index = 0 ) const 
{
	CUtlVector< int > layers;
	FindLayersForTime( time, layers );
	int layerCount = layers.Count();
	if ( layerCount <= 1 )
	{
		return GetValue( time, pAttr, index );
	}

	int topMostLayer = GetTopmostLayer();
	int useLayer =  layers[ layerCount - 1 ];
	if ( topMostLayer == useLayer )
	{
		useLayer = layers[ layerCount - 2 ];
	}
	Assert( useLayer >= 0 );
	return GetLayer( useLayer )->GetValue( time, pAttr, index );
}

template< class T >
float CDmeTypedLog< T >::GetComponent( DmeTime_t time, int componentIndex ) const
{
	return ::GetComponent( GetValue( time ), componentIndex );
}

//-----------------------------------------------------------------------------
// resampling and filtering
//-----------------------------------------------------------------------------
template< class T >
void CDmeTypedLog< T >::Resample( DmeFramerate_t samplerate )
{
	int c = m_Layers.Count();
	for ( int i = 0; i < c; ++i )
	{
		GetLayer( i )->Resample( samplerate );
	}
}

template< class T >
void CDmeTypedLog< T >::Filter( int nSampleRadius )
{
	int c = m_Layers.Count();
	for ( int i = 0; i < c; ++i )
	{
		GetLayer( i )->Filter( nSampleRadius );
	}
}

template< class T >
void CDmeTypedLog< T >::Filter2( DmeTime_t sampleRadius )
{
	int c = m_Layers.Count();
	for ( int i = 0; i < c; ++i )
	{
		GetLayer( i )->Filter2( sampleRadius );
	}
}

template< class T >
void CDmeTypedLog< T >::OnAttributeArrayElementAdded( CDmAttribute *pAttribute, int nFirstElem, int nLastElem )
{
	BaseClass::OnAttributeArrayElementAdded( pAttribute, nFirstElem, nLastElem );
	if ( pAttribute == m_Layers.GetAttribute() )
	{
		for ( int i = nFirstElem; i <= nLastElem; ++i )
		{
			m_Layers[i]->SetOwnerLog( this );
		}
		return;
	}
}

template< class T >
void CDmeTypedLog< T >::SetUseEdgeInfo( bool state )
{
	Assert( IsUsingCurveTypes() );
	GetTypedCurveInfo()->SetUseEdgeInfo( state );
}

template< class T >
bool CDmeTypedLog< T >::IsUsingEdgeInfo() const
{
	Assert( IsUsingCurveTypes() );
	return GetTypedCurveInfo()->IsUsingEdgeInfo();
}

template< class T >
void CDmeTypedLog< T >::SetEdgeInfo( int edge, bool active, const T& val, int curveType )
{
	Assert( IsUsingCurveTypes() );
	GetTypedCurveInfo()->SetEdgeInfo( edge, active, val, curveType );
}

template< class T >
void CDmeTypedLog< T >::SetDefaultEdgeZeroValue( const T& val )
{
	Assert( IsUsingCurveTypes() );
	GetTypedCurveInfo()->SetDefaultEdgeZeroValue( val );
}

template< class T >
const T& CDmeTypedLog< T >::GetDefaultEdgeZeroValue() const
{
	Assert( IsUsingCurveTypes() );
	return GetTypedCurveInfo()->GetDefaultEdgeZeroValue();
}

template< class T >
void CDmeTypedLog< T >::SetRightEdgeTime( DmeTime_t time )
{
	Assert( IsUsingCurveTypes() );
	GetTypedCurveInfo()->SetRightEdgeTime( time );
}

template< class T >
DmeTime_t CDmeTypedLog< T >::GetRightEdgeTime() const
{
	Assert( IsUsingCurveTypes() );
	return GetTypedCurveInfo()->GetRightEdgeTime();
}

template< class T >
void CDmeTypedLog< T >::GetEdgeInfo( int edge, bool& active, T& val, int& curveType ) const
{
	Assert( IsUsingCurveTypes() );
	GetTypedCurveInfo()->GetEdgeInfo( edge, active, val, curveType );
}

template< class T >
int CDmeTypedLog< T >::GetEdgeCurveType( int edge ) const
{
	Assert( IsUsingCurveTypes() );
	return GetTypedCurveInfo()->GetEdgeCurveType( edge );
}

template< class T >
void CDmeTypedLog< T >::GetZeroValue( int side, T& val ) const
{
	Assert( IsUsingCurveTypes() );
	GetTypedCurveInfo()->GetZeroValue( side, val );
}

template< class T >
bool CDmeTypedLog< T >::IsEdgeActive( int edge ) const
{
	Assert( IsUsingCurveTypes() );
	return GetTypedCurveInfo()->IsEdgeActive( edge );
}

template< class T >
void CDmeTypedLog< T >::GetEdgeValue( int edge, T& val ) const
{
	Assert( IsUsingCurveTypes() );
	GetTypedCurveInfo()->GetEdgeValue( edge, val );
}

template< class T > 
void CDmeTypedLog< T >::BlendTimesUsingTimeSelection( const CDmeLogLayer *firstLayer, const CDmeLogLayer *secondLayer, CDmeLogLayer *outputLayer, const DmeLog_TimeSelection_t &params, DmeTime_t tStartOffset )
{
	const CDmeTypedLogLayer< T > *topLayer = static_cast< const CDmeTypedLogLayer< T > * >( secondLayer );
	if ( !topLayer )
		return;

	const CDmeTypedLogLayer< T > *baseLayer = static_cast< const CDmeTypedLogLayer< T > * >( firstLayer );
	if ( !baseLayer )
		return;

	CDmeTypedLogLayer< T > *newLayer = static_cast< CDmeTypedLogLayer< T > * >( outputLayer );
	if ( !newLayer )
		return;

	Assert( topLayer->GetKeyCount() == baseLayer->GetKeyCount() );

	int i;
	// Resample everything in the base layer first
	int kc = baseLayer->GetKeyCount();

	newLayer->ClearKeys();

	for ( i = 0; i < kc; ++i )
	{
		DmeTime_t baseKeyTime = baseLayer->GetKeyTime( i );
		DmeTime_t checkTime = baseKeyTime + tStartOffset;
		if ( checkTime < params.m_nTimes[ TS_LEFT_FALLOFF ] )
			continue;
		if ( checkTime > params.m_nTimes[ TS_RIGHT_FALLOFF ] )
			break;
		float frac = params.GetAmountForTime( checkTime );
		float frac2 = params.m_flIntensity;
		float flInterp = frac2 * frac;

		DmeTime_t targetKeyTime = topLayer->GetKeyTime( i );

		DmeTime_t blendedKeyTime = Lerp( flInterp, baseKeyTime, targetKeyTime ) + tStartOffset;

		T baseVal = baseLayer->GetKeyValue( i );

		newLayer->InsertKey( blendedKeyTime, baseVal );
	}
}

template< class T > 
void CDmeTypedLog< T >::BlendLayersUsingTimeSelection( const CDmeLogLayer *firstLayer, const CDmeLogLayer *secondLayer, CDmeLogLayer *outputLayer, const DmeLog_TimeSelection_t &params, bool bUseBaseLayerSamples, DmeTime_t tStartOffset )
{
	const CDmeTypedLogLayer< T > *topLayer = static_cast< const CDmeTypedLogLayer< T > * >( secondLayer );
	if ( !topLayer )
		return;

	const CDmeTypedLogLayer< T > *baseLayer = static_cast< const CDmeTypedLogLayer< T > * >( firstLayer );
	if ( !baseLayer )
		return;

	CDmeTypedLogLayer< T > *newLayer = static_cast< CDmeTypedLogLayer< T > * >( outputLayer );
	if ( !newLayer )
		return;

	int i;
	// Resample everything in the base layer first
	int kc = baseLayer->GetKeyCount();
	if ( bUseBaseLayerSamples )
	{
		for ( i = 0; i < kc; ++i )
		{
			DmeTime_t keyTime = baseLayer->GetKeyTime( i );
			if ( keyTime < params.m_nTimes[ TS_LEFT_FALLOFF ] )
				continue;
			if ( keyTime > params.m_nTimes[ TS_RIGHT_FALLOFF ] )
				break;

			float frac = params.GetAmountForTime( keyTime );
			float frac2 = params.m_flIntensity;

			T baseVal = baseLayer->GetKeyValue( i );
			T newVal = topLayer->GetValue( keyTime );
			T blended = Interpolate( frac2 * frac, baseVal, newVal );

			newLayer->SetKey( keyTime + tStartOffset, blended );
		}
	}

	kc = topLayer->GetKeyCount();
	for ( i = 0; i < kc; ++i )
	{
		DmeTime_t keyTime = topLayer->GetKeyTime( i );
		DmeTime_t finalKeyTime = keyTime + tStartOffset;
		if ( finalKeyTime < params.m_nTimes[ TS_LEFT_FALLOFF ] )
			continue;
		if ( finalKeyTime > params.m_nTimes[ TS_RIGHT_FALLOFF ] )
			break;
		float frac = params.GetAmountForTime( finalKeyTime );
		float frac2 = params.m_flIntensity;

		T baseVal = baseLayer->GetValue( keyTime );
		T newVal = topLayer->GetKeyValue( i );
		T blended = Interpolate( frac2 *frac, baseVal, newVal );

		newLayer->InsertKey( finalKeyTime, blended );
	}

	if ( g_pDmElementFramework->GetPhase() == PH_EDIT )
	{
		newLayer->RemoveRedundantKeys( params.m_flThreshold );
	}
}

template< class T > 
void CDmeTypedLog< T >::BlendLayersUsingTimeSelection( const DmeLog_TimeSelection_t &params )
{
	Assert( GetNumLayers() >= 2 );
	int bestLayer = GetTopmostLayer(); // Topmost should be at least layer # 1 (0 is the base layer)
	if ( bestLayer <= 0 )
		return;

	Assert( params.m_nResampleInterval > DmeTime_t( 0 ) );
	if ( params.m_nResampleInterval < DmeTime_t( 0 ) )
		return;

	CDmeTypedLogLayer< T > *topLayer = GetLayer( bestLayer );
	Assert( topLayer );
	if ( !topLayer )
		return;

	CDmeTypedLogLayer< T > *baseLayer = GetLayer( 0 );
	if ( !baseLayer )
		return;

	CDmeTypedLogLayer< T > *newLayer = static_cast< CDmeTypedLogLayer< T > * >( CreateLayer< T >( this ) );
	if ( !newLayer )
		return;

	BlendLayersUsingTimeSelection( baseLayer, topLayer, newLayer, params, true, DMETIME_ZERO );

	// Store it back into the new topmost layer
	topLayer->CopyLayer( newLayer );

	g_pDataModel->DestroyElement( newLayer->GetHandle() );
}

template< class T > 
void CDmeTypedLog< T >::RevealUsingTimeSelection( const DmeLog_TimeSelection_t &params, CDmeLogLayer *savedLayer )
{
	CDmeTypedLogLayer< T > *saved = static_cast< CDmeTypedLogLayer< T > * >( savedLayer );
	if ( !saved )
		return;

	Assert( GetNumLayers() >= 2 );
	int bestLayer = GetTopmostLayer(); // Topmost should be at least layer # 1 (0 is the base layer)
	if ( bestLayer <= 0 )
		return;
	
	Assert( params.m_nResampleInterval > DmeTime_t( 0 ) );
	if ( params.m_nResampleInterval < DmeTime_t( 0 ) )
		return;

	CDmeTypedLogLayer< T > *writeLayer = static_cast< CDmeTypedLogLayer< T > * >( GetLayer( bestLayer ) );
	Assert( writeLayer );
	if ( !writeLayer )
		return;

	CDmeLogLayer *baseLayer = GetLayer( 0 );
	if ( !baseLayer )
		return;

	DmeTime_t resample = 0.5f * params.m_nResampleInterval;

	// Do a second pass where we bis the keys in the falloff area back toward the original value
	for ( int t = params.m_nTimes[ TS_LEFT_FALLOFF ].GetTenthsOfMS(); t < params.m_nTimes[ TS_RIGHT_FALLOFF ].GetTenthsOfMS() + resample.GetTenthsOfMS(); t += resample.GetTenthsOfMS() )
	{
		DmeTime_t curtime = DmeTime_t( t );
		if ( curtime > params.m_nTimes[ TS_RIGHT_FALLOFF ] )
			curtime = params.m_nTimes[ TS_RIGHT_FALLOFF ];

		float frac = params.GetAmountForTime( curtime );
		frac *= params.m_flIntensity;

		if ( frac <= 0.0f )
			continue;

		// Get current value in layer
		T curValue = GetValueSkippingTopmostLayer( curtime );
		T revealValue = saved->GetValue( curtime );

		T newValue = Interpolate( frac, curValue, revealValue );

		// Overwrite key
		writeLayer->InsertKey( curtime, newValue, IsUsingCurveTypes() ? GetDefaultCurveType() : CURVE_DEFAULT ); 
	}

	if ( g_pDmElementFramework->GetPhase() == PH_EDIT )
	{
		writeLayer->RemoveRedundantKeys( params.m_flThreshold );
	}
}

template< class T > 
void RandomValue( const T& average, const T& oldValue, T& newValue )
{
	newValue = oldValue;
}

template<> void RandomValue( const Vector& average, const Vector& oldValue, Vector& newValue )
{
	newValue = oldValue;
	
	for ( int i = 0; i < 3; ++i )
	{
		newValue[ i ] += RandomFloat( -fabs( average[ i ] ), fabs( average[ i ] ) );
	}
}

template<> void RandomValue( const Quaternion& average, const Quaternion& oldValue, Quaternion& newValue )
{
	newValue = oldValue;
	
	for ( int i = 0; i < 4; ++i )
	{
		newValue[ i ] += RandomFloat( -fabs( average[ i ] ), fabs( average[ i ] ) );
	}
}

template<> void RandomValue( const Vector4D& average, const Vector4D& oldValue, Vector4D& newValue )
{
	newValue = oldValue;
	
	for ( int i = 0; i < 4; ++i )
	{
		newValue[ i ] += RandomFloat( -fabs( average[ i ] ), fabs( average[ i ] ) );
	}
}

template<> void RandomValue( const Vector2D& average, const Vector2D& oldValue, Vector2D& newValue )
{
	newValue = oldValue;
	
	for ( int i = 0; i < 2; ++i )
	{
		newValue[ i ] += RandomFloat( -fabs( average[ i ] ), fabs( average[ i ] ) );
	}
}

template<> void RandomValue( const float& average, const float& oldValue, float& newValue )
{
	newValue = oldValue + RandomFloat( -average, average );
}

template<> void RandomValue( const int& average, const int& oldValue, int& newValue )
{
	newValue = oldValue + RandomInt( -average, average );
}

// Builds a layer with samples matching the times in reference layer, from the data in pDataLayer, putting the resulting keys into pOutputLayer
template< class T >
void CDmeTypedLog< T >::BuildCorrespondingLayer( const CDmeLogLayer *pReferenceLayer, const CDmeLogLayer *pDataLayer, CDmeLogLayer *pOutputLayer )
{
	const CDmeTypedLogLayer< T > *ref = static_cast< const CDmeTypedLogLayer< T > * >( pReferenceLayer );
	const CDmeTypedLogLayer< T > *data = static_cast< const CDmeTypedLogLayer< T > * >( pDataLayer );
	CDmeTypedLogLayer< T > *out = static_cast< CDmeTypedLogLayer< T > * >( pOutputLayer );

	if ( !ref || !data || !out )
	{
		Assert( 0 );
		return;
	}

	bool usecurvetypes = ref->IsUsingCurveTypes();

	out->ClearKeys();
	int kc = ref->GetKeyCount();
	for ( int i = 0; i < kc; ++i )
	{
		DmeTime_t keyTime = ref->GetKeyTime( i );
		T value = data->GetValue( keyTime );

		out->InsertKey( keyTime, value, usecurvetypes ? GetDefaultCurveType() : CURVE_DEFAULT );
	}
}

template< class T >
void CDmeTypedLog< T >::StaggerUsingTimeSelection( const DmeLog_TimeSelection_t& params, DmeTime_t tStaggerAmount, const CDmeLogLayer *pBaseLayer, CDmeLogLayer *pWriteLayer )
{
	CDmeTypedLogLayer< T > *writeLayer = static_cast< CDmeTypedLogLayer< T > * >( pWriteLayer );
	Assert( writeLayer );
	if ( !writeLayer )
		return;

	const CDmeTypedLogLayer< T > *baseLayer = static_cast< const CDmeTypedLogLayer< T > * >( pBaseLayer );
	if ( !baseLayer )
		return;

	writeLayer->ClearKeys();

	DmeLog_TimeSelection_t newParams;
	newParams = params;

	// Move the hold area by the stagger amount
	float flScaleFactor[ 2 ] = { 1.0f, 1.0f };

	newParams.m_nTimes[ TS_LEFT_HOLD ] += tStaggerAmount;
	newParams.m_nTimes[ TS_RIGHT_HOLD ] += tStaggerAmount;

	for ( int i = 0; i < 2 ; ++i )
	{
		DmeTime_t dt = params.m_nTimes[ 2 * i + 1 ] - params.m_nTimes[ 2 * i ];
		if ( dt > DMETIME_ZERO )
		{
			DmeTime_t newDt = newParams.m_nTimes[ 2 * i + 1 ] - newParams.m_nTimes[ 2 * i ];
			flScaleFactor[ i ] = newDt / dt;
		}
	}

	int kc = baseLayer->GetKeyCount();
	for ( int i = 0; i < kc; ++i )
	{
		DmeTime_t curtime = baseLayer->GetKeyTime( i );
		T oldValue = baseLayer->GetKeyValue( i );

		// Classify time
		if ( curtime <= params.m_nTimes[ TS_LEFT_HOLD ] )
		{
			curtime = curtime * flScaleFactor[ 0 ];
		}
		else if ( curtime >= params.m_nTimes[ TS_RIGHT_HOLD ] )
		{
			curtime = params.m_nTimes[ TS_RIGHT_FALLOFF ] - ( params.m_nTimes[ TS_RIGHT_FALLOFF ] - curtime ) * flScaleFactor[ 1 ];
		}
		else
		{
			curtime += tStaggerAmount;
		}

		writeLayer->InsertKey( curtime, oldValue, IsUsingCurveTypes() ? GetDefaultCurveType() : CURVE_DEFAULT ); 
	}
}

template< class T >
void CDmeTypedLog< T >::FilterUsingTimeSelection( IUniformRandomStream *random, const DmeLog_TimeSelection_t& params, int filterType, bool bResample, bool bApplyFalloff )
{
	Assert( GetNumLayers() >= 2 );
	int bestLayer = GetTopmostLayer(); // Topmost should be at least layer # 1 (0 is the base layer)
	if ( bestLayer <= 0 )
		return;

	CDmeTypedLogLayer< T > *writeLayer = GetLayer( bestLayer );
	Assert( writeLayer );
	if ( !writeLayer )
		return;

	CDmeTypedLogLayer< T > *baseLayer = GetLayer( 0 );
	if ( !baseLayer )
		return;

	FilterUsingTimeSelection( random, 1.0f, params, filterType, bResample, bApplyFalloff, baseLayer, writeLayer );
}

template< class T >
void CDmeTypedLog< T >::FilterUsingTimeSelection( IUniformRandomStream *random, float flScale, const DmeLog_TimeSelection_t& params, int filterType, bool bResample, bool bApplyFalloff, const CDmeLogLayer *pBaseLayer, CDmeLogLayer *pWriteLayer )
{
	Assert( params.m_nResampleInterval > DmeTime_t( 0 ) );
	if ( params.m_nResampleInterval < DmeTime_t( 0 ) )
		return;

	CDmeTypedLogLayer< T > *writeLayer = static_cast< CDmeTypedLogLayer< T > * >( pWriteLayer );
	Assert( writeLayer );
	if ( !writeLayer )
		return;

	const CDmeTypedLogLayer< T > *baseLayer = static_cast< const CDmeTypedLogLayer< T > * >( pBaseLayer );
	if ( !baseLayer )
		return;

	writeLayer->ClearKeys();

	DmeTime_t resample = 0.5f * params.m_nResampleInterval;

	switch ( filterType )
	{
	default:
	case FILTER_SMOOTH:
		{
			int t;
			if ( bResample )
			{
				for ( t = params.m_nTimes[ TS_LEFT_FALLOFF ].GetTenthsOfMS(); t < params.m_nTimes[ TS_RIGHT_FALLOFF ].GetTenthsOfMS() + resample.GetTenthsOfMS(); t += resample.GetTenthsOfMS() )
				{
					DmeTime_t curtime = DmeTime_t( t );
					if ( curtime > params.m_nTimes[ TS_RIGHT_FALLOFF ] )
						curtime = params.m_nTimes[ TS_RIGHT_FALLOFF ];
	
					T curValue = baseLayer->GetValue( curtime );
					writeLayer->SetKey( curtime, curValue, IsUsingCurveTypes() ? GetDefaultCurveType() : CURVE_DEFAULT ); 
				}
			}
			else
			{
				// Do a second pass where we bias the keys in the falloff area back toward the original value
				int kc = baseLayer->GetKeyCount();
				for ( int i = 0; i < kc; ++i )
				{
					DmeTime_t curtime = baseLayer->GetKeyTime( i );
					if ( curtime < params.m_nTimes[ TS_LEFT_FALLOFF ] )
						continue;

					if ( curtime > params.m_nTimes[ TS_RIGHT_FALLOFF ] )
						continue;

					T oldValue = baseLayer->GetKeyValue( i );
					writeLayer->InsertKey( curtime, oldValue, IsUsingCurveTypes() ? GetDefaultCurveType() : CURVE_DEFAULT );
				}
			}

			writeLayer->Filter2( params.m_nResampleInterval * 0.95f * flScale );

			if ( bApplyFalloff )
			{
				if ( bResample )
				{
					// Do a second pass where we bias the keys in the falloff area back toward the original value
					for ( t = params.m_nTimes[ TS_LEFT_FALLOFF ].GetTenthsOfMS(); t < params.m_nTimes[ TS_RIGHT_FALLOFF ].GetTenthsOfMS() + resample.GetTenthsOfMS(); t += resample.GetTenthsOfMS() )
					{
						DmeTime_t curtime = DmeTime_t( t );
						if ( curtime > params.m_nTimes[ TS_RIGHT_FALLOFF ] )
							curtime = params.m_nTimes[ TS_RIGHT_FALLOFF ];

						T oldValue = baseLayer->GetValue( curtime );

						if ( curtime >= params.m_nTimes[ TS_LEFT_HOLD ] && curtime <= params.m_nTimes[ TS_RIGHT_HOLD ] )
							continue;

						// Modulate these keys back down toward the original value
						T newValue = writeLayer->GetValue( curtime );

						float frac = bApplyFalloff ? params.GetAmountForTime( curtime ) : 1.0f;

						newValue = Interpolate( frac, oldValue, newValue );

						// Overwrite key
						writeLayer->InsertKey( curtime, newValue, IsUsingCurveTypes() ? GetDefaultCurveType() : CURVE_DEFAULT ); 
					}
				}
				else
				{
					// Do a second pass where we bias the keys in the falloff area back toward the original value
					int kc = writeLayer->GetKeyCount();
					for ( int i = 0; i < kc; ++i )
					{
						DmeTime_t curtime = writeLayer->GetKeyTime( i );
						if ( curtime < params.m_nTimes[ TS_LEFT_FALLOFF ] )
							continue;

						if ( curtime > params.m_nTimes[ TS_RIGHT_FALLOFF ] )
							continue;

						if ( curtime >= params.m_nTimes[ TS_LEFT_HOLD ] && curtime <= params.m_nTimes[ TS_RIGHT_HOLD ] )
							continue;

						T oldValue = baseLayer->GetValue( curtime );

						// Modulate these keys back down toward the original value
						T newValue = writeLayer->GetValue( curtime );

						float frac = bApplyFalloff ? params.GetAmountForTime( curtime ) : 1.0f;

						newValue = Interpolate( frac, oldValue, newValue );

						// Overwrite key
						writeLayer->InsertKey( curtime, newValue, IsUsingCurveTypes() ? GetDefaultCurveType() : CURVE_DEFAULT ); 
					}
				}
			}

			if ( bResample )
			{
				writeLayer->RemoveRedundantKeys( params.m_flThreshold );
			}
		}
		break;
	case FILTER_JITTER:
		{
			// Compute average value in entire log
			
			T average = Average( baseLayer->m_values.Base(), baseLayer->m_values.Count() );
			average = ScaleValue( average, 0.05f * flScale );

			if ( bResample )
			{
				int t;
				for ( t = params.m_nTimes[ TS_LEFT_FALLOFF ].GetTenthsOfMS(); t < params.m_nTimes[ TS_RIGHT_FALLOFF ].GetTenthsOfMS() + resample.GetTenthsOfMS(); t += resample.GetTenthsOfMS() )
				{
					DmeTime_t curtime = DmeTime_t( t );
					if ( curtime > params.m_nTimes[ TS_RIGHT_FALLOFF ] )
						curtime = params.m_nTimes[ TS_RIGHT_FALLOFF ];

					float frac = bApplyFalloff ? params.GetAmountForTime( curtime ) : 1.0f;

					T oldValue = baseLayer->GetValue( curtime );

					T newValue;
					RandomValue( average, oldValue, newValue );

					newValue = Interpolate( frac, oldValue, newValue );

					writeLayer->SetKey( curtime, newValue, IsUsingCurveTypes() ? GetDefaultCurveType() : CURVE_DEFAULT ); 
				}
				
			}
			else
			{
				int kc = baseLayer->GetKeyCount();
				for ( int i = 0; i < kc; ++i )
				{
					DmeTime_t curtime = baseLayer->GetKeyTime( i );
					if ( curtime < params.m_nTimes[ TS_LEFT_FALLOFF ] )
						continue;

					if ( curtime > params.m_nTimes[ TS_RIGHT_FALLOFF ] )
						continue;

					float frac = bApplyFalloff ? params.GetAmountForTime( curtime ) : 1.0f;

					T oldValue = baseLayer->GetValue( curtime );

					T newValue;
					RandomValue( average, oldValue, newValue );

					newValue = Interpolate( frac, oldValue, newValue );

					writeLayer->InsertKey( curtime, newValue, IsUsingCurveTypes() ? GetDefaultCurveType() : CURVE_DEFAULT ); 
				}
			}
		}
		break;
	case FILTER_SHARPEN:
	case FILTER_SOFTEN:
		{
			writeLayer->ClearKeys();

			bool bSharpen = filterType == FILTER_SHARPEN;
			int kc = baseLayer->GetKeyCount();
			for ( int i = 0; i < kc; ++i )
			{
				DmeTime_t curtime = baseLayer->GetKeyTime( i );
				if ( curtime < params.m_nTimes[ TS_LEFT_FALLOFF ] )
					continue;

				if ( curtime > params.m_nTimes[ TS_RIGHT_FALLOFF ] )
					continue;

				float frac = bApplyFalloff ? params.GetAmountForTime( curtime ) : 1.0f;

				T oldValue = baseLayer->GetValue( curtime );

				T newValue = oldValue;
				if ( frac != 1.0f )
				{
					T crossingValue[ 2 ] = { oldValue, oldValue };
					if ( curtime <= params.m_nTimes[ TS_LEFT_HOLD ] )
					{
						// Get the value at the crossing point (either green edge for sharpen, or left edge for soften...)
						crossingValue[ 0 ] = baseLayer->GetValue( params.m_nTimes[ TS_LEFT_FALLOFF ] );
						crossingValue[ 1 ] = baseLayer->GetValue( params.m_nTimes[ TS_LEFT_HOLD ] );
					}
					else if ( curtime >= params.m_nTimes[ TS_RIGHT_HOLD ] )
					{
						crossingValue[ 0 ] = baseLayer->GetValue( params.m_nTimes[ TS_RIGHT_FALLOFF ] );
						crossingValue[ 1 ] = baseLayer->GetValue( params.m_nTimes[ TS_RIGHT_HOLD ] );
					}
					else
					{
						Assert( 0 );
					}

					T dynamicRange = Subtract( crossingValue[ 1 ], crossingValue[ 0 ] );

					int iType = bSharpen ? INTERPOLATE_EASE_IN : INTERPOLATE_EASE_OUT;

					Vector points[ 4 ];
					points[ 0 ].Init();
					points[ 1 ].Init( 0.0, 0.0, 0.0f );
					points[ 2 ].Init( 1.0f, 1.0f, 0.0f );
					points[ 3 ].Init();

					Vector out;

					Interpolator_CurveInterpolate
						( 
						iType, 
						points[ 0 ], // unused
						points[ 1 ], 
						points[ 2 ], 
						points[ 3 ], // unused
						frac, 
						out 
						);

					float flBias = clamp( out.y, 0.0f, 1.0f );
					float dFrac = flScale * ( frac - flBias );

					newValue = Add( oldValue, ScaleValue( dynamicRange, dFrac ) );
				}

				writeLayer->InsertKey( curtime, newValue, IsUsingCurveTypes() ? GetDefaultCurveType() : CURVE_DEFAULT ); 
			}
		}
		break;
	}
}

enum PasteState_t
{
	PASTE_STATE_BEFORE = -1,

	PASTE_STATE_RAMP_IN = 0,
	PASTE_STATE_HOLD,
	PASTE_STATE_RAMP_OUT,

	PASTE_STATE_COUNT,
	PASTE_STATE_AFTER = PASTE_STATE_COUNT,
};

template<class T >
static void CountClipboardSamples( int *pCount, CDmeTypedLogLayer< T > *pClipboard, const DmeLog_TimeSelection_t &params )
{
	pCount[0] = pCount[1] = pCount[2] = 0;

	int nKeyCount = pClipboard->GetKeyCount();
	for ( int i = 0; i < nKeyCount; ++i )
	{
		DmeTime_t tKeyTime = pClipboard->GetKeyTime( i );
		int nIndex = params.ComputeRegionForTime( tKeyTime ) - 1;
		if ( nIndex < 0 || nIndex > 2 )
			continue;

		// Only count interstitial samples.. don't count ones that land exactly on boundaries
		if ( tKeyTime != params.m_nTimes[nIndex] && tKeyTime != params.m_nTimes[nIndex+1] )
		{
			pCount[nIndex]++;
		}
	}
}


//-----------------------------------------------------------------------------
// Used by PasteAndRescaleSamples to determine if it should skip a transition or not
//-----------------------------------------------------------------------------
static inline bool ShouldSkipTransition( int nTransition, int nZeroField )
{
	// NOTE: This is pretty tricky. The bits of the 'zero field' are set to true
	// for each region whose source + dest region size is exactly 0 seconds.
	// Here's the table this logic is reproducing:
	// 0,1,2,3 are the time selection m_nTimes, and A,B,C are the regions
	//  0     1     2     3
	//  |  A  |  B  |  C  |
	//
	//  nZeroField bits		
	//  C B A				Skip transitions
	//	0 0 0				none
	//	0 0 1				2
	//	0 1 0				2
	//	0 1 1				1, 2
	//	1 0 0				1
	//	1 0 1				1, 2
	//	1 1 0				1, 2
	//	1 1 1				1, 2, 3
	switch( nTransition )
	{
	default: case 0: return false;
	case 1: return ( (( nZeroField & 0x1 ) != 0 ) || nZeroField >= 5 );
	case 2: return ( nZeroField >= 2 );
	case 3: return ( nZeroField == 7 );
	}
}

template< class T >
void CDmeTypedLog< T >::PasteAndRescaleSamples( 
	const CDmeLogLayer *pBase, 
	const CDmeLogLayer *pDataLayer, 
	CDmeLogLayer *pOutputLayer, 
	const DmeLog_TimeSelection_t& srcParams, 
	const DmeLog_TimeSelection_t& destParams, 
	bool bBlendAreaInFalloffRegion )
{
	Assert( GetNumLayers() >= 2 );
	if ( GetNumLayers() < 2 )
		return;

	CDmeTypedLogLayer< T > *pClipboard = CastElement< CDmeTypedLogLayer< T > >( const_cast< CDmeLogLayer * >( pDataLayer ) );

	// Could have passed in layer with wrong attribute type?!
	Assert( pClipboard );
	if ( !pClipboard )
		return;

	CDmeTypedLogLayer< T > *pBaseLayer = CastElement< CDmeTypedLogLayer< T > >( const_cast< CDmeLogLayer * >( pBase ) );
	CDmeTypedLogLayer< T > *pWriteLayer = CastElement< CDmeTypedLogLayer< T > >( pOutputLayer );
	Assert( pBaseLayer );
	Assert( pWriteLayer );

	// NOTE: Array index 0 is src (pClipboard), index 1 is dest (pWriteLayer)
	DmeTime_t tStartTime[ PASTE_STATE_COUNT+1 ][ 2 ] =	
	{ 
		{ DmeTime_t( srcParams.m_nTimes[ 0 ] ),	DmeTime_t( destParams.m_nTimes[ 0 ] ) },
		{ DmeTime_t( srcParams.m_nTimes[ 1 ] ),	DmeTime_t( destParams.m_nTimes[ 1 ] ) },
		{ DmeTime_t( srcParams.m_nTimes[ 2 ] ),	DmeTime_t( destParams.m_nTimes[ 2 ] ) },
		{ DmeTime_t( srcParams.m_nTimes[ 3 ] ),	DmeTime_t( destParams.m_nTimes[ 3 ] ) },
	};

	// compute rescaling factors
	int pDuration[ PASTE_STATE_COUNT ][ 2 ];
	double pScaleFactor[ PASTE_STATE_COUNT ];
	int nZeroField = 0;
	for ( int i = 0; i < PASTE_STATE_COUNT; ++i )
	{
		for ( int s = 0; s < 2; ++s )
		{
			pDuration[ i ][ s ]	= tStartTime[ i+1 ][ s ].GetTenthsOfMS() - tStartTime[ i ][ s ].GetTenthsOfMS();
		}

		// We're building up a bitfield to find which regions have src + dest durations of 0
		// for use in determining which regions to completely skip processing
		if ( pDuration[i][0] == 0 && pDuration[i][1] == 0 )
		{
			nZeroField |= ( 1 << i );
		}

		pScaleFactor[i] = 1.0;
		if ( pDuration[ i ][ 0 ] > 0 )
		{
			pScaleFactor[i] = 1.0 / ( double )pDuration[ i ][ 0 ];
		}
	}

	// Compute values used to paste into selection state transitions
	T	pStartValue[ PASTE_STATE_COUNT + 1 ] =
	{
		bBlendAreaInFalloffRegion ? pBaseLayer->GetValue( tStartTime[ PASTE_STATE_RAMP_IN ][ 1 ] ) : pClipboard->GetValue( tStartTime[ PASTE_STATE_RAMP_IN ][ 0 ] ),
		pClipboard->GetValue( tStartTime[ PASTE_STATE_HOLD ][ 0 ] ),
		pClipboard->GetValue( tStartTime[ PASTE_STATE_RAMP_OUT ][ 0 ] ),
		bBlendAreaInFalloffRegion ? pBaseLayer->GetValue( tStartTime[ PASTE_STATE_AFTER ][ 1 ] ) : pClipboard->GetValue( tStartTime[ PASTE_STATE_AFTER ][ 0 ] ) 
	};

	// Compute state necessary to blend in the ramp in + ramp out regions
	// NOTE: These computations are only used if bBlendAreaInFalloffRegion is true
	T		pBlendBase[ 2 ];
	float	pOOBlendLength[ 2 ];
	DmeTime_t pBlendTime[ 2 ];
	for ( int s = 0; s < 2; ++s )
	{
		pBlendTime[ s ] = destParams.m_nTimes[ TS_FALLOFF(s) ];
		pBlendBase[ s ] = pBaseLayer->GetValue( pBlendTime[ s ] );
		T holdValue = pBaseLayer->GetValue( destParams.m_nTimes[ TS_HOLD(s) ] );

		Vector2D vec;
		vec.x = destParams.m_nTimes[ TS_HOLD(s) ].GetSeconds() - pBlendTime[ s ].GetSeconds();
		vec.y = LengthOf( Subtract( holdValue, pBlendBase[ s ] ) );
		pOOBlendLength[ s ] = vec.Length();
		if ( pOOBlendLength[ s ] != 0.0f )
		{
			pOOBlendLength[ s ] = 1.0f / pOOBlendLength[ s ];
		}
	}

	// Count the number of samples on the clipboard in the various regions
	int pKeyCount[PASTE_STATE_COUNT];
	CountClipboardSamples( pKeyCount, pClipboard, srcParams );

	// Walk the samples in the clipboard
	int nKeyCount = pClipboard->GetKeyCount();
	int nPrevState = PASTE_STATE_BEFORE;
	DmeTime_t tLastWrittenTime = DMETIME_MINTIME;
	DmeTime_t tMaxKeyTime = DMETIME_MAXTIME;
	bool bCollapseSamples = false;
	for ( int j = 0 ; j < nKeyCount; ++j )
	{
		DmeTime_t tKeyTime = pClipboard->GetKeyTime( j );
		T val = pClipboard->GetKeyValue( j );

		// Determine which state we're in
		// NOTE: Don't use ComputeRegionForTime here because it includes 
		// the endpoint of the hold region into the hold region.
		int nState;
		for ( nState = nPrevState; nState < PASTE_STATE_COUNT; ++nState )
		{
			if ( tKeyTime < tStartTime[ nState + 1 ][ 0 ] )
				break;
		}

		// This logic inserts a key if there is no sample in the clipboard at the transition time
		bool bForceKey = false;
		if ( nPrevState < nState )
		{
			nState = ++nPrevState;

			// This logic will prevent samples at the hold start + end if 
			// the source + dest regions are 0 width and will only do the first and last
			// if we're squeezing the entire time selection down to a single point.
			bForceKey = true;

			if ( nState != PASTE_STATE_AFTER )
			{
				bCollapseSamples = ( pKeyCount[nState] >= pDuration[nState][1] );
				tMaxKeyTime = bCollapseSamples ? tStartTime[ nState ][ 1 ] : ( tStartTime[ nState+1 ][ 1 ] - DmeTime_t( pKeyCount[nState] + 1 ) );
			}
			else
			{
				bCollapseSamples = false;
				tMaxKeyTime = DMETIME_MAXTIME;
			}

			// NOTE: This has to occur after collapse samples + max key time has been set
			if ( ShouldSkipTransition( nState, nZeroField ) )
			{
				--j;
				continue;
			}

			// Don't insert an extra key if the current one we're looking at is right at that point
			if ( tKeyTime != tStartTime[ nPrevState ][ 0 ] )
			{
				tKeyTime = tStartTime[ nPrevState ][ 0 ];
				val = pStartValue[nPrevState];

				// We want to re-do this key, since we inserted a key beforehand
				--j;
			}
		}

		if ( nState == PASTE_STATE_BEFORE )
			continue;

		if ( nState == PASTE_STATE_AFTER && !bForceKey )
			return;

		// Compute destination time based on scale + offset
		double flFactor = ( tKeyTime - tStartTime[ nState ][ 0 ] ).GetTenthsOfMS() * pScaleFactor[ nState ];

		// FIXME: Fix the algorithm, then uncomment to get time-scaled falloff regions
		//		if ( nState == PASTE_STATE_RAMP_IN || nState == PASTE_STATE_RAMP_OUT )
		//		{
		//			int s = ( nState == PASTE_STATE_RAMP_IN ) ? 0 : 1;
		//			flFactor = ComputeInterpolationFactor( flFactor, destParams.m_nFalloffInterpolatorTypes[s] );
		//		}
		double flTempTime = flFactor * pDuration[ nState ][ 1 ];
		DmeTime_t tDestTime( (int)( flTempTime + 0.5 ) );
		tDestTime += tStartTime[ nState ][ 1 ];

		// Clamp necessary to not lose samples
		// NOTE: The !bForceKey check here makes it so we don't clamp points
		// in time corresponding to transitions of the time selection
		if ( !bForceKey && ( tDestTime > tMaxKeyTime ) )
		{
			tDestTime = tMaxKeyTime;
		}
		if ( tMaxKeyTime != DMETIME_MAXTIME )
		{
			tMaxKeyTime += DMETIME_MINDELTA;
		}

		// This logic will cause *all* samples to appear if we have enough room for them
		if ( !bCollapseSamples )
		{
			bForceKey = true;
		}

		// If we'd go outside our region and we're not forcing the key, then skip
		if ( !bForceKey && tDestTime >= tStartTime[ nState+1 ][ 1 ] )
			continue;

		// Perform blending on ramp in + ramp out regions
		if ( bBlendAreaInFalloffRegion && ( nState != PASTE_STATE_HOLD ) )
		{
			int nBlendIndex = ( nState < PASTE_STATE_HOLD ) ? 0 : 1;
			T baseValue = pBaseLayer->GetValue( tDestTime );

			Vector2D oldDist;
			oldDist.x = tDestTime.GetSeconds() - pBlendTime[ nBlendIndex ].GetSeconds();
			oldDist.y = LengthOf( Subtract( baseValue, pBlendBase[ nBlendIndex ] ) );

			float flDistance = oldDist.Length();
			float flFactorBlend = flDistance * pOOBlendLength[ nBlendIndex ];
			flFactorBlend = destParams.AdjustFactorForInterpolatorType( flFactorBlend, nBlendIndex );
			val = Interpolate( flFactorBlend, baseValue, val );
		}

		// Force key insertion when we transition between states
		if ( bForceKey && ( tLastWrittenTime >= tDestTime ) )
		{
			tDestTime = tLastWrittenTime + DMETIME_MINDELTA;
		}

		// Insert the key into the log
		if ( tLastWrittenTime < tDestTime )
		{
			pWriteLayer->InsertKey( tDestTime, val );
			tLastWrittenTime = tDestTime;
		}
	}
}

template< class T >
void CDmeTypedLog< T >::PasteAndRescaleSamples( 
	const CDmeLogLayer *src,   // clipboard data
	const DmeLog_TimeSelection_t& srcParams,   // clipboard time selection
	const DmeLog_TimeSelection_t& destParams,   // current time selection
	bool bBlendAreaInFalloffRegion ) // blending behavior in falloff area of current time selection
{
	CDmeLogLayer *pBaseLayer = GetLayer( 0 );
	CDmeLogLayer *pWriteLayer = GetLayer( GetTopmostLayer() );
	PasteAndRescaleSamples( pBaseLayer, src, pWriteLayer, srcParams, destParams, bBlendAreaInFalloffRegion );
}

template<>
void CDmeTypedLog< Vector >::BuildNormalizedLayer( CDmeTypedLogLayer< float > *target )
{
	Assert( target );
	Assert( GetDataType() != AT_FLOAT );

	CDmeTypedLogLayer< Vector > *baseLayer = static_cast< CDmeTypedLogLayer< Vector > * >( GetLayer( 0 ) );
	if ( !baseLayer )
		return;

	float flMin = FLT_MAX;
	float flMax = FLT_MIN;

	int kc = baseLayer->GetKeyCount();
	for ( int i = 0; i < kc; ++i )
	{
		DmeTime_t keyTime = baseLayer->GetKeyTime( i );
		Vector keyValue = baseLayer->GetKeyValue( i );

		float len = keyValue.Length();
		if ( len < flMin )
		{
			flMin = len;
		}
		if ( len > flMax )
		{
			flMax = len;
		}

		target->InsertKey( keyTime, len );
	}

	for ( int i = 0; i < kc; ++i )
	{
		float keyValue = target->GetKeyValue( i );
		float normalized = RemapVal( keyValue, flMin, flMax, 0.0f, 1.0f );
		target->SetKeyValue( i, normalized );
	}

	if ( HasDefaultValue() )
	{
		target->GetTypedOwnerLog()->SetDefaultValue( RemapVal( GetDefaultValue().Length(), flMin, flMax, 0.0f, 1.0f ) );
	}
}

template<>
void CDmeTypedLog< Vector2D >::BuildNormalizedLayer( CDmeTypedLogLayer< float > *target )
{
	Assert( target );
	Assert( GetDataType() != AT_FLOAT );

	CDmeTypedLogLayer< Vector2D > *baseLayer = static_cast< CDmeTypedLogLayer< Vector2D > * >( GetLayer( 0 ) );
	if ( !baseLayer )
		return;

	float flMin = FLT_MAX;
	float flMax = FLT_MIN;

	int kc = baseLayer->GetKeyCount();
	for ( int i = 0; i < kc; ++i )
	{
		DmeTime_t keyTime = baseLayer->GetKeyTime( i );
		Vector2D keyValue = baseLayer->GetKeyValue( i );

		float len = keyValue.Length();

		if ( len < flMin )
		{
			flMin = len;
		}
		if ( len > flMax )
		{
			flMax = len;
		}

		target->InsertKey( keyTime, len );
	}

	for ( int i = 0; i < kc; ++i )
	{
		float keyValue = target->GetKeyValue( i );
		float normalized = RemapVal( keyValue, flMin, flMax, 0.0f, 1.0f );
		target->SetKeyValue( i, normalized );
	}

	if ( HasDefaultValue() )
	{
		target->GetTypedOwnerLog()->SetDefaultValue( RemapVal( GetDefaultValue().Length(), flMin, flMax, 0.0f, 1.0f ) );
	}
}

template<>
void CDmeTypedLog< Vector4D >::BuildNormalizedLayer( CDmeTypedLogLayer< float > *target )
{
	Assert( target );
	Assert( GetDataType() != AT_FLOAT );

	CDmeTypedLogLayer< Vector4D > *baseLayer = static_cast< CDmeTypedLogLayer< Vector4D > * >( GetLayer( 0 ) );
	if ( !baseLayer )
		return;

	float flMin = FLT_MAX;
	float flMax = FLT_MIN;

	int kc = baseLayer->GetKeyCount();
	for ( int i = 0; i < kc; ++i )
	{
		DmeTime_t keyTime = baseLayer->GetKeyTime( i );
		Vector4D keyValue = baseLayer->GetKeyValue( i );

		float len = keyValue.Length();

		if ( len < flMin )
		{
			flMin = len;
		}
		if ( len > flMax )
		{
			flMax = len;
		}

		target->InsertKey( keyTime, len );
	}

	for ( int i = 0; i < kc; ++i )
	{
		float keyValue = target->GetKeyValue( i );
		float normalized = RemapVal( keyValue, flMin, flMax, 0.0f, 1.0f );
		target->SetKeyValue( i, normalized );
	}

	if ( HasDefaultValue() )
	{
		target->GetTypedOwnerLog()->SetDefaultValue( RemapVal( GetDefaultValue().Length(), flMin, flMax, 0.0f, 1.0f ) );
	}
}

template<>
void CDmeTypedLog< int >::BuildNormalizedLayer( CDmeTypedLogLayer< float > *target )
{
	Assert( target );
	Assert( GetDataType() != AT_FLOAT );

	CDmeTypedLogLayer< int > *baseLayer = static_cast< CDmeTypedLogLayer< int > * >( GetLayer( 0 ) );
	if ( !baseLayer )
		return;

	float flMin = FLT_MAX;
	float flMax = FLT_MIN;

	int kc = baseLayer->GetKeyCount();
	for ( int i = 0; i < kc; ++i )
	{
		DmeTime_t keyTime = baseLayer->GetKeyTime( i );
		int keyValue = baseLayer->GetKeyValue( i );

		float len = (float)keyValue;

		if ( len < flMin )
		{
			flMin = len;
		}
		if ( len > flMax )
		{
			flMax = len;
		}

		target->InsertKey( keyTime, len );
	}

	for ( int i = 0; i < kc; ++i )
	{
		float keyValue = target->GetKeyValue( i );
		float normalized = RemapVal( keyValue, flMin, flMax, 0.0f, 1.0f );
		target->SetKeyValue( i, normalized );
	}

	if ( HasDefaultValue() )
	{
		target->GetTypedOwnerLog()->SetDefaultValue( RemapVal( GetDefaultValue(), flMin, flMax, 0.0f, 1.0f ) );
	}
}

template<>
void CDmeTypedLog< float >::BuildNormalizedLayer( CDmeTypedLogLayer< float > *target )
{
	Assert( target );
	Assert( GetDataType() != AT_FLOAT );

	CDmeTypedLogLayer< float > *baseLayer = static_cast< CDmeTypedLogLayer< float > * >( GetLayer( 0 ) );
	if ( !baseLayer )
		return;

	float flMin = FLT_MAX;
	float flMax = FLT_MIN;

	int kc = baseLayer->GetKeyCount();
	for ( int i = 0; i < kc; ++i )
	{
		DmeTime_t keyTime = baseLayer->GetKeyTime( i );
		int keyValue = baseLayer->GetKeyValue( i );

		float len = (float)keyValue;

		if ( len < flMin )
		{
			flMin = len;
		}
		if ( len > flMax )
		{
			flMax = len;
		}

		target->InsertKey( keyTime, len );
	}

	for ( int i = 0; i < kc; ++i )
	{
		float keyValue = target->GetKeyValue( i );
		float normalized = RemapVal( keyValue, flMin, flMax, 0.0f, 1.0f );
		target->SetKeyValue( i, normalized );
	}

	if ( HasDefaultValue() )
	{
		target->GetTypedOwnerLog()->SetDefaultValue( RemapVal( GetDefaultValue(), flMin, flMax, 0.0f, 1.0f ) );
	}
}

//-----------------------------------------------------------------------------
// Creates a log of a specific type
//-----------------------------------------------------------------------------
CDmeLog *CDmeLog::CreateLog( DmAttributeType_t type, DmFileId_t fileid )
{
	switch ( type )
	{
	case AT_INT:
	case AT_INT_ARRAY:
		return CreateElement< CDmeIntLog >( "int log", fileid );
	case AT_FLOAT:
	case AT_FLOAT_ARRAY:
		return CreateElement< CDmeFloatLog >( "float log", fileid );
	case AT_BOOL:
	case AT_BOOL_ARRAY:
		return CreateElement< CDmeBoolLog >( "bool log", fileid );
	case AT_COLOR:
	case AT_COLOR_ARRAY:
		return CreateElement< CDmeColorLog >( "color log", fileid );
	case AT_VECTOR2:
	case AT_VECTOR2_ARRAY:
		return CreateElement< CDmeVector2Log >( "vector2 log", fileid );
	case AT_VECTOR3:
	case AT_VECTOR3_ARRAY:
		return CreateElement< CDmeVector3Log >( "vector3 log", fileid );
	case AT_VECTOR4:
	case AT_VECTOR4_ARRAY:
		return CreateElement< CDmeVector4Log >( "vector4 log", fileid );
	case AT_QANGLE:
	case AT_QANGLE_ARRAY:
		return CreateElement< CDmeQAngleLog >( "qangle log", fileid );
	case AT_QUATERNION:
	case AT_QUATERNION_ARRAY:
		return CreateElement< CDmeQuaternionLog >( "quaternion log", fileid );
	case AT_VMATRIX:
	case AT_VMATRIX_ARRAY:
		return CreateElement< CDmeVMatrixLog >( "vmatrix log", fileid );
	case AT_STRING:
	case AT_STRING_ARRAY:
		return CreateElement< CDmeStringLog >( "string log", fileid );
	}

	return NULL;
}

// Disallowed methods for types
//template<> void CDmeTypedLog< bool >::StampKeyAtHead( const DmeLog_TimeSelection_t& params, const bool& value ) { Assert( 0 ); }
//template<> void CDmeTypedLog< bool >::_StampKeyAtHeadResample( const DmeLog_TimeSelection_t& params, const bool& value ) { Assert( 0 ); }
//template<> void CDmeTypedLog< bool >::StampKeyAtHead( const DmeLog_TimeSelection_t& params, const CDmAttribute *pAttr, uint index /*= 0*/ ) { Assert( 0 ); }
//template<> void CDmeTypedLog< bool >::FinishTimeSelection( DmeLog_TimeSelection_t& params ) { Assert( 0 ); }
//
//template<> void CDmeTypedLog< Color >::StampKeyAtHead( const DmeLog_TimeSelection_t& params, const Color& value ) { Assert( 0 ); }
//template<> void CDmeTypedLog< Color >::_StampKeyAtHeadResample( const DmeLog_TimeSelection_t& params, const Color& value ) { Assert( 0 ); }
//template<> void CDmeTypedLog< Color >::StampKeyAtHead( const DmeLog_TimeSelection_t& params, const CDmAttribute *pAttr, uint index /*= 0*/ ) { Assert( 0 ); }
//template<> void CDmeTypedLog< Color >::FinishTimeSelection( DmeLog_TimeSelection_t& params ) { Assert( 0 ); }
//
//template<> void CDmeTypedLog< Vector4D >::StampKeyAtHead( const DmeLog_TimeSelection_t& params, const Vector4D& value ) { Assert( 0 ); }
//template<> void CDmeTypedLog< Vector4D >::_StampKeyAtHeadResample( const DmeLog_TimeSelection_t& params, const Vector4D& value ) { Assert( 0 ); }
//template<> void CDmeTypedLog< Vector4D >::StampKeyAtHead( const DmeLog_TimeSelection_t& params, const CDmAttribute *pAttr, uint index /*= 0*/ ) { Assert( 0 ); }
//template<> void CDmeTypedLog< Vector4D >::FinishTimeSelection( DmeLog_TimeSelection_t& params ) { Assert( 0 ); }
//
//template<> void CDmeTypedLog< Vector2D >::StampKeyAtHead( const DmeLog_TimeSelection_t& params, const Vector2D& value ) { Assert( 0 ); }
//template<> void CDmeTypedLog< Vector2D >::_StampKeyAtHeadResample( const DmeLog_TimeSelection_t& params, const Vector2D& value ) { Assert( 0 ); }
//template<> void CDmeTypedLog< Vector2D >::StampKeyAtHead( const DmeLog_TimeSelection_t& params, const CDmAttribute *pAttr, uint index /*= 0*/ ) { Assert( 0 ); }
//template<> void CDmeTypedLog< Vector2D >::FinishTimeSelection( DmeLog_TimeSelection_t& params ) { Assert( 0 ); }

//template<> void CDmeTypedLog< Vector >::StampKeyAtHead( const DmeLog_TimeSelection_t& params, const Vector& value ) { Assert( 0 ); }
//template<> void CDmeTypedLog< Vector >::_StampKeyAtHeadResample( const DmeLog_TimeSelection_t& params, const Vector& value ) { Assert( 0 ); }
//template<> void CDmeTypedLog< Vector >::StampKeyAtHead( const DmeLog_TimeSelection_t& params, const CDmAttribute *pAttr, uint index /*= 0*/ ) { Assert( 0 ); }
//template<> void CDmeTypedLog< Vector >::FinishTimeSelection( DmeLog_TimeSelection_t& params ) { Assert( 0 ); }

//template<> void CDmeTypedLog< VMatrix >::StampKeyAtHead( const DmeLog_TimeSelection_t& params, const VMatrix& value ) { Assert( 0 ); }
//template<> void CDmeTypedLog< VMatrix >::_StampKeyAtHeadResample( const DmeLog_TimeSelection_t& params, const VMatrix& value ) { Assert( 0 ); }
//template<> void CDmeTypedLog< VMatrix >::StampKeyAtHead( const DmeLog_TimeSelection_t& params, const CDmAttribute *pAttr, uint index /*= 0*/ ) { Assert( 0 ); }
//template<> void CDmeTypedLog< VMatrix >::FinishTimeSelection( DmeLog_TimeSelection_t& params ) { Assert( 0 ); }
//
//template<> void CDmeTypedLog< Quaternion >::StampKeyAtHead( const DmeLog_TimeSelection_t& params, const Quaternion& value ) { Assert( 0 ); }
//template<> void CDmeTypedLog< Quaternion >::_StampKeyAtHeadResample( const DmeLog_TimeSelection_t& params, const Quaternion& value ) { Assert( 0 ); }
//template<> void CDmeTypedLog< Quaternion >::StampKeyAtHead( const DmeLog_TimeSelection_t& params, const CDmAttribute *pAttr, uint index /*= 0*/ ) { Assert( 0 ); }
//template<> void CDmeTypedLog< Quaternion >::FinishTimeSelection( DmeLog_TimeSelection_t& params ) { Assert( 0 ); }
//
//template<> void CDmeTypedLog< QAngle >::StampKeyAtHead( const DmeLog_TimeSelection_t& params, const QAngle& value ) { Assert( 0 ); }
//template<> void CDmeTypedLog< QAngle >::_StampKeyAtHeadResample( const DmeLog_TimeSelection_t& params, const QAngle& value ) { Assert( 0 ); }
//template<> void CDmeTypedLog< QAngle >::StampKeyAtHead( const DmeLog_TimeSelection_t& params, const CDmAttribute *pAttr, uint index /*= 0*/ ) { Assert( 0 ); }
//template<> void CDmeTypedLog< QAngle >::FinishTimeSelection( DmeLog_TimeSelection_t& params ) { Assert( 0 ); }


//-----------------------------------------------------------------------------
// Helpers for particular types of log layers
//-----------------------------------------------------------------------------
void GenerateRotationLog( CDmeQuaternionLogLayer *pLayer, const Vector &vecAxis, DmeTime_t pTime[4], float pRevolutionsPerSec[4] )
{
	for ( int i = 1; i < 4; ++i )
	{
		if ( pTime[i] < pTime[i-1] )
		{
			Warning( "Bogus times passed into GenerateRotationLog\n" );
			return;
		}
	}

	// Gets the initial value
	matrix3x4_t initial;
	Quaternion q = pLayer->GetValue( pTime[0] );
	QuaternionMatrix( q, initial );

	// Find the max rps, and compute the total rotation in degrees
	// by the time we reach the transition points. The total rotation = 
	// integral from 0 to t of 360 * ( rate[i] - rate[i-1] ) t / tl + rate[i-1] )
	// == 360 * ( ( rate[i] - rate[i-1] ) t^2 / 2 + rate[i-1] t )
	float pTotalRotation[4];
	float flMaxRPS = pRevolutionsPerSec[0];
	pTotalRotation[0] = 0.0f;
	for ( int i = 1; i < 4; ++i )
	{
		if ( pRevolutionsPerSec[i] > flMaxRPS )
		{
			flMaxRPS = pRevolutionsPerSec[i];
		}
		float dt = pTime[i].GetSeconds() - pTime[i-1].GetSeconds();
		float dRot = pRevolutionsPerSec[i] - pRevolutionsPerSec[i-1];
		pTotalRotation[i] = 360.0f * ( dRot * dt * 0.5 + pRevolutionsPerSec[i-1] * dt ) + pTotalRotation[i-1];
	}

	// We need to compute how long a single rotation takes, then create samples
	// at 1/4 the frequency of that amount of time
	VMatrix rot;
	matrix3x4_t total;
	QAngle angles;
	float flMaxRotationTime = (flMaxRPS != 0.0f) ? ( 0.125f / flMaxRPS ) : ( pTime[3].GetSeconds() - pTime[0].GetSeconds() );
	DmeTime_t dt( flMaxRotationTime );
	for ( DmeTime_t t = pTime[0]; t <= pTime[3]; t += dt )
	{
		int i = ( t < pTime[1] ) ? 1 : ( ( t < pTime[2] ) ? 2 : 3 );
		float flInterval = t.GetSeconds() - pTime[i-1].GetSeconds();
		float flOOSegmentDur = pTime[i].GetSeconds() - pTime[i-1].GetSeconds();
		if ( flOOSegmentDur == 0.0f )
		{
			Assert( flInterval == 0.0f );
			flOOSegmentDur = 1.0f;
		}
		else
		{
			flOOSegmentDur = 1.0f / flOOSegmentDur;
		}
		float dRot = pRevolutionsPerSec[i] - pRevolutionsPerSec[i-1];
		float flRotation = 360.0f * ( dRot * flInterval * flInterval * 0.5f * flOOSegmentDur + pRevolutionsPerSec[i-1] * flInterval ) + pTotalRotation[i-1];

		MatrixBuildRotationAboutAxis( rot, vecAxis, flRotation );
		ConcatTransforms( initial, rot.As3x4(), total );
		MatrixToAngles( total, angles );
		AngleQuaternion( angles, q );
		pLayer->SetKey( t, q );
	}
}


//-----------------------------------------------------------------------------
// Transforms a position log
//-----------------------------------------------------------------------------
void RotatePositionLog( CDmeVector3LogLayer *pPositionLog, const matrix3x4_t& matrix )
{
	Assert( fabs( matrix[0][3] ) < 1e-3 && fabs( matrix[1][3] ) < 1e-3 && fabs( matrix[2][3] ) < 1e-3 );
	Vector position;
	int nCount = pPositionLog->GetKeyCount();
	for ( int i = 0; i < nCount; ++i )
	{
		const Vector &srcPosition = pPositionLog->GetKeyValue( i );
		VectorTransform( srcPosition, matrix, position );
		pPositionLog->SetKeyValue( i, position );
	}
}


//-----------------------------------------------------------------------------
// Transforms a orientation log
//-----------------------------------------------------------------------------
void RotateOrientationLog( CDmeQuaternionLogLayer *pOrientationLog, const matrix3x4_t& matrix, bool bPreMultiply = false )
{
	Assert( fabs( matrix[0][3] ) < 1e-3 && fabs( matrix[1][3] ) < 1e-3 && fabs( matrix[2][3] ) < 1e-3 );
	matrix3x4_t orientation, newOrientation;
	Quaternion q;
	int nCount = pOrientationLog->GetKeyCount();
	for ( int i = 0; i < nCount; ++i )
	{
		const Quaternion &srcQuat = pOrientationLog->GetKeyValue( i );
		QuaternionMatrix( srcQuat, orientation );
		if ( bPreMultiply )
		{
			ConcatTransforms( matrix, orientation, newOrientation );
		}
		else
		{
			ConcatTransforms( orientation, matrix, newOrientation );
		}
		MatrixQuaternion( newOrientation, q );
		pOrientationLog->SetKeyValue( i, q );
	}
}
