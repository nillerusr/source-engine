//========= Copyright Valve Corporation, All rights reserved. ============//
#include "movieobjects/dmetimeselection.h"
#include "interpolatortypes.h"
#include "datamodel/dmelementfactoryhelper.h"
// #include "dme_controls/RecordingState.h"
						   
IMPLEMENT_ELEMENT_FACTORY( DmeTimeSelection, CDmeTimeSelection );

void CDmeTimeSelection::OnConstruction()
{
	m_bEnabled.InitAndSet( this, "enabled", false );
	m_bRelative.InitAndSet( this, "relative", false );

	DmeTime_t one( 1.0f );

	m_falloff[ 0 ].InitAndSet( this, "falloff_left", -one.GetTenthsOfMS() );
	m_falloff[ 1 ].InitAndSet( this, "falloff_right", one.GetTenthsOfMS() );

	m_hold[ 0 ].Init( this, "hold_left" );
	m_hold[ 1 ].Init( this, "hold_right" );

	m_nFalloffInterpolatorType[ 0 ].InitAndSet( this, "interpolator_left", INTERPOLATE_LINEAR_INTERP );
	m_nFalloffInterpolatorType[ 1 ].InitAndSet( this, "interpolator_right", INTERPOLATE_LINEAR_INTERP );

	m_threshold.InitAndSet( this, "threshold", 0.0005f );

	m_nRecordingState.InitAndSet( this, "recordingstate", 3 /*AS_PLAYBACK :  HACK THIS SHOULD MOVE TO A PUBLIC HEADER*/ );
}


void CDmeTimeSelection::OnDestruction()
{
}

static int g_InterpolatorTypes[] = 
{
	INTERPOLATE_LINEAR_INTERP,
	INTERPOLATE_EASE_IN,
	INTERPOLATE_EASE_OUT,								
	INTERPOLATE_EASE_INOUT,		
};

float CDmeTimeSelection::AdjustFactorForInterpolatorType( float factor, int side )
{
	Vector points[ 4 ];
	points[ 0 ].Init();
	points[ 1 ].Init( 0.0, 0.0, 0.0f );
	points[ 2 ].Init( 1.0f, 1.0f, 0.0f );
	points[ 3 ].Init();

	Vector out;
	Interpolator_CurveInterpolate
	( 
		GetFalloffInterpolatorType( side ), 
			points[ 0 ], // unused
			points[ 1 ], 
			points[ 2 ], 
			points[ 3 ], // unused
		factor, 
		out 
	);
	return out.y; // clamp( out.y, 0.0f, 1.0f );
}


//-----------------------------------------------------------------------------
// per-type averaging methods
//-----------------------------------------------------------------------------
float CDmeTimeSelection::GetAmountForTime( DmeTime_t t, DmeTime_t curtime )
{
	Assert( IsEnabled() );

	float minfrac = 0.0f;

	// FIXME, this is slow, we should cache this maybe?
	DmeTime_t times[ 4 ];
	times[ 0 ] = GetAbsFalloff( curtime, 0 );
	times[ 1 ] = GetAbsHold( curtime, 0 );
	times[ 2 ] = GetAbsHold( curtime, 1 );
	times[ 3 ] = GetAbsFalloff( curtime, 1 );

	Vector points[ 4 ];
	points[ 0 ].Init();
	points[ 1 ].Init( 0.0, 0.0, 0.0f );
	points[ 2 ].Init( 1.0f, 1.0f, 0.0f );
	points[ 3 ].Init();

	if ( t >= times[ 0 ] && t < times[ 1 ] )
	{
		float f = GetFractionOfTimeBetween( t, times[ 0 ], times[ 1 ], true );

		Vector out;

		Interpolator_CurveInterpolate
		( 
			GetFalloffInterpolatorType( 0 ), 
				points[ 0 ], // unused
				points[ 1 ], 
				points[ 2 ], 
				points[ 3 ], // unused
			f, 
			out 
		);
		return clamp( out.y, minfrac, 1.0f );
	}
	
	if ( t >= times[ 1 ] && t <= times[ 2 ] )
	{
		return 1.0f;
	}

	if ( t > times[ 2 ] && t <= times[ 3 ] )
	{
		float f = 1.0f - GetFractionOfTimeBetween( t, times[ 2 ], times[ 3 ], true );

		Vector out;

		Interpolator_CurveInterpolate
		( 
			GetFalloffInterpolatorType( 1 ), 
				points[ 0 ], // unused
				points[ 1 ], 
				points[ 2 ], 
				points[ 3 ], // unused
			f, 
			out 
		);
		return clamp( out.y, minfrac, 1.0f );
	}
	return minfrac;
}

void CDmeTimeSelection::GetAlphaForTime( DmeTime_t t, DmeTime_t curtime, byte& alpha )
{
	Assert( IsEnabled() );

	byte minAlpha = 31;

	// FIXME, this is slow, we should cache this maybe?
	DmeTime_t times[ 4 ];
	times[ 0 ] = GetAbsFalloff( curtime, 0 );
	times[ 1 ] = GetAbsHold( curtime, 0 );
	times[ 2 ] = GetAbsHold( curtime, 1 );
	times[ 3 ] = GetAbsFalloff( curtime, 1 );

	DmeTime_t dt1, dt2;
	dt1 = times[ 1 ] - times[ 0 ];
	dt2 = times[ 3 ] - times[ 2 ];

	if ( dt1 > DmeTime_t( 0 ) &&
		t >= times[ 0 ] && t < times[ 1 ] )
	{
		float frac = GetFractionOfTime( t - times[ 0 ], dt1, false );
		alpha = clamp( alpha * frac, minAlpha, 255 );
		return;
	}
	if ( dt2 > DmeTime_t( 0 ) &&
		t > times[ 2 ] && t <= times[ 3 ] )
	{
		float frac = GetFractionOfTime( times[ 3 ] - t, dt2, false );
		alpha = clamp( alpha * frac, minAlpha, 255 );
		return;
	}
	if ( t < times[ 0 ] )
		alpha = minAlpha;
	else if ( t > times[ 3 ] )
		alpha = minAlpha;
}

int CDmeTimeSelection::GetFalloffInterpolatorType( int side )
{
	return m_nFalloffInterpolatorType[ side ];
}

void CDmeTimeSelection::SetFalloffInterpolatorType( int side, int interpolatorType )
{
	m_nFalloffInterpolatorType[ side ] = interpolatorType;
}

bool CDmeTimeSelection::IsEnabled() const
{
	return m_bEnabled;
}

void CDmeTimeSelection::SetEnabled( bool state )
{
	m_bEnabled = state;
}

bool CDmeTimeSelection::IsRelative() const
{
	return m_bRelative;
}

void CDmeTimeSelection::SetRelative( DmeTime_t time, bool state )
{
	bool changed = m_bRelative != state;
	m_bRelative = state;
	if ( changed )
	{
		if ( state )
			ConvertToRelative( time );
		else 
			ConvertToAbsolute( time );
	}
}

DmeTime_t CDmeTimeSelection::GetAbsFalloff( DmeTime_t time, int side )
{
	if ( m_bRelative )
	{
		return DmeTime_t( m_falloff[ side ] ) + time;
	}
	return DmeTime_t( m_falloff[ side ] );
}

DmeTime_t CDmeTimeSelection::GetAbsHold( DmeTime_t time, int side )
{
	if ( m_bRelative )
	{
		return DmeTime_t( m_hold[ side ] ) + time;
	}
	return DmeTime_t( m_hold[ side ] );
}

DmeTime_t CDmeTimeSelection::GetRelativeFalloff( DmeTime_t time, int side )
{
	if ( m_bRelative )
	{
		return DmeTime_t( m_falloff[ side ] );
	}
	return DmeTime_t( m_falloff[ side ] ) - time;
}

DmeTime_t CDmeTimeSelection::GetRelativeHold( DmeTime_t time, int side )
{
	if ( m_bRelative )
	{
		return DmeTime_t( m_hold[ side ] );
	}
	return DmeTime_t( m_hold[ side ] ) - time;
}

void CDmeTimeSelection::ConvertToRelative( DmeTime_t time )
{
	m_falloff[ 0 ] -= time.GetTenthsOfMS();
	m_falloff[ 1 ] -= time.GetTenthsOfMS();
	m_hold[ 0 ] -= time.GetTenthsOfMS();
	m_hold[ 1 ] -= time.GetTenthsOfMS();
}

void CDmeTimeSelection::ConvertToAbsolute( DmeTime_t time )
{
	m_falloff[ 0 ] += time.GetTenthsOfMS();
	m_falloff[ 1 ] += time.GetTenthsOfMS();
	m_hold[ 0 ] += time.GetTenthsOfMS();
	m_hold[ 1 ] += time.GetTenthsOfMS();
}

void CDmeTimeSelection::SetAbsFalloff( DmeTime_t time, int side, DmeTime_t absfallofftime )
{
	DmeTime_t newTime;
	if ( m_bRelative )
	{
		newTime = absfallofftime - time;
	}
	else
	{
		newTime = absfallofftime;
	}

	m_falloff[ side ] = newTime.GetTenthsOfMS();
}

void CDmeTimeSelection::SetAbsHold( DmeTime_t time, int side, DmeTime_t absholdtime )
{
	DmeTime_t newTime;
	if ( m_bRelative )
	{
		newTime = absholdtime - time;
	}
	else
	{
		newTime = absholdtime;
	}

	m_hold[ side ] = newTime.GetTenthsOfMS();
}

void CDmeTimeSelection::CopyFrom( const CDmeTimeSelection& src )
{
	m_bEnabled = src.m_bEnabled;
	m_bRelative = src.m_bRelative;
	m_threshold = src.m_threshold;

	for ( int i = 0 ; i < 2; ++i )
	{
		m_falloff[ i ] = src.m_falloff[ i ];
		m_hold[ i ] = src.m_hold[ i ];
		m_nFalloffInterpolatorType[ i ] = src.m_nFalloffInterpolatorType[ i ];
	}

	m_nRecordingState = src.m_nRecordingState;
}

void CDmeTimeSelection::GetCurrent( DmeTime_t pTimes[TS_TIME_COUNT] )
{
	pTimes[TS_LEFT_FALLOFF].SetTenthsOfMS( m_falloff[ 0 ] );
	pTimes[TS_LEFT_HOLD].SetTenthsOfMS( m_hold[ 0 ] );
	pTimes[TS_RIGHT_HOLD].SetTenthsOfMS( m_hold[ 1 ] );
	pTimes[TS_RIGHT_FALLOFF].SetTenthsOfMS( m_falloff[ 1 ] );
}

void CDmeTimeSelection::SetCurrent( DmeTime_t* pTimes )
{
	m_falloff[ 0 ] = pTimes[ TS_LEFT_FALLOFF ].GetTenthsOfMS();
	m_hold[ 0 ] = pTimes[ TS_LEFT_HOLD ].GetTenthsOfMS();
	m_hold[ 1 ] = pTimes[ TS_RIGHT_HOLD ].GetTenthsOfMS();
	m_falloff[ 1 ] = pTimes[ TS_RIGHT_FALLOFF ].GetTenthsOfMS();
}

float CDmeTimeSelection::GetThreshold()
{
	return m_threshold;
}


void CDmeTimeSelection::SetRecordingState( RecordingState_t state )
{
	m_nRecordingState = ( int )state;
}

RecordingState_t CDmeTimeSelection::GetRecordingState() const
{
	return ( RecordingState_t )m_nRecordingState.Get();
}