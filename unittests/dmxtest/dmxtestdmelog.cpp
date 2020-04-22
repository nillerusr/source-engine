//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "unitlib/unitlib.h"
#include "datamodel/dmelement.h"
#include "movieobjects/movieobjects.h"
#include "datamodel/idatamodel.h"
#include "tier1/utlbuffer.h"
#include "filesystem.h"
#include "movieobjects/dmelog.h"
#include "choreoscene.h"
#include "choreoevent.h"

struct data_t
{
	int			tms; // tenths of a millisecond
	float		value;
	int			curvetype;
};

struct tvpair_t
{
	int					tms;
	float				expectedvalue;
};

data_t data[] =
{
	{ 0,		0.0f,	MAKE_CURVE_TYPE( INTERPOLATE_CATMULL_ROM_NORMALIZEX, INTERPOLATE_CATMULL_ROM_NORMALIZEX ) },
	{ 10000,	0.5f,	MAKE_CURVE_TYPE( INTERPOLATE_CATMULL_ROM_NORMALIZEX, INTERPOLATE_CATMULL_ROM_NORMALIZEX ) },
	{ 20000,	0.5f,	MAKE_CURVE_TYPE( INTERPOLATE_EASE_IN, INTERPOLATE_EASE_OUT ) },
	{ 30000,	0.5f,	MAKE_CURVE_TYPE( INTERPOLATE_EASE_OUT, INTERPOLATE_EASE_INOUT ) },
	{ 40000,	0.5f,	MAKE_CURVE_TYPE( INTERPOLATE_EASE_INOUT, INTERPOLATE_BSPLINE ) },
	{ 50000,	0.5f,	MAKE_CURVE_TYPE( INTERPOLATE_BSPLINE, INTERPOLATE_LINEAR_INTERP ) },
	{ 60000,	1.0f,	MAKE_CURVE_TYPE( INTERPOLATE_LINEAR_INTERP, INTERPOLATE_KOCHANEK_BARTELS ) },
	{ 70000,	0.0f,	MAKE_CURVE_TYPE( INTERPOLATE_KOCHANEK_BARTELS, INTERPOLATE_KOCHANEK_BARTELS_EARLY ) },
	{ 80000,	0.5f,	MAKE_CURVE_TYPE( INTERPOLATE_KOCHANEK_BARTELS_EARLY, INTERPOLATE_KOCHANEK_BARTELS_LATE ) },
	{ 90000,	0.0f,	MAKE_CURVE_TYPE( INTERPOLATE_KOCHANEK_BARTELS_LATE, INTERPOLATE_SIMPLE_CUBIC ) },
	{ 100000,	0.25f,	MAKE_CURVE_TYPE( INTERPOLATE_SIMPLE_CUBIC, INTERPOLATE_CATMULL_ROM ) },
	{ 110000,	0.0f,	MAKE_CURVE_TYPE( INTERPOLATE_CATMULL_ROM, INTERPOLATE_CATMULL_ROM_NORMALIZE ) },
	{ 120000,	0.125f,	MAKE_CURVE_TYPE( INTERPOLATE_CATMULL_ROM_NORMALIZE, INTERPOLATE_EXPONENTIAL_DECAY ) },
	{ 130000,	0.0f,	MAKE_CURVE_TYPE( INTERPOLATE_EXPONENTIAL_DECAY, INTERPOLATE_HOLD ) },
	{ 140000,	0.0625f,	MAKE_CURVE_TYPE( INTERPOLATE_CATMULL_ROM_NORMALIZEX, INTERPOLATE_EXPONENTIAL_DECAY ) },
	{ 150000,	0.0f,	MAKE_CURVE_TYPE( INTERPOLATE_CATMULL_ROM_NORMALIZEX, INTERPOLATE_CATMULL_ROM_NORMALIZEX ) },
};

#define NUM_DEF_TESTS 3

static data_t values1[] = 
{
	{ -1,			0.0f, 0 },
};
static data_t values2[] = 
{
	{ 5000,			0.5f, CURVE_DEFAULT },
	{ -1,			0.0f, 0 },
};
static data_t values3[] = 
{
	{ 2500,			0.25f, CURVE_DEFAULT },
	{ 7500,			0.75f, CURVE_DEFAULT },
	{ -1,			0.0f, 0 },
};

static data_t *defaultvaluetest[ NUM_DEF_TESTS ] =
{
	values1,
	values2,
	values3
};

#define NUM_TEST_VALUES 3

static tvpair_t expectedvalues1[NUM_TEST_VALUES] =
{
	{ 0,			0.5f },
	{ 5000,			0.5f },
	{ 10000,		0.5f },
};

static tvpair_t expectedvalues2[NUM_TEST_VALUES] =
{
	{ 0,			0.5f },
	{ 5000,			0.5f },
	{ 10000,		0.5f },
};

static tvpair_t expectedvalues3[NUM_TEST_VALUES] =
{
	{ 0,			0.25f },
	{ 5000,			0.5f },
	{ 10000,		0.75f },
};

static tvpair_t *expectedvalues[ NUM_DEF_TESTS ] =
{
	expectedvalues1,
	expectedvalues2,
	expectedvalues3
};

void ResetLog( CDmeFloatLog *log, bool useCurveTypes, int startIndex = 0, int endIndex = -1 )
{
	log->ClearKeys();

	CDmeCurveInfo *pCurveInfo = useCurveTypes ? log->GetOrCreateCurveInfo() : log->GetCurveInfo();
	if ( useCurveTypes )
	{
		pCurveInfo->SetDefaultCurveType( MAKE_CURVE_TYPE( INTERPOLATE_CATMULL_ROM_NORMALIZEX, INTERPOLATE_CATMULL_ROM_NORMALIZEX ) );
	}
	else if ( !useCurveTypes && pCurveInfo )
	{
		g_pDataModel->DestroyElement( pCurveInfo->GetHandle() );
		log->SetCurveInfo( NULL );
	}

	int i;
	int c;

	c = ARRAYSIZE( data );
	for ( i = startIndex; i < c; ++i )
	{
		log->SetKey( DmeTime_t( data[ i ].tms ), data[ i ].value, useCurveTypes ? data[ i ].curvetype : CURVE_DEFAULT ); 

		if ( endIndex != -1 && i >= endIndex )
			break;
	}
}

void CompareFloats( float f1, float f2, float tol, char const *fmt, ... )
{
	float diff = fabs( f1 - f2 );
	if ( diff < tol )
		return;

	char buf[ 256 ];
	va_list argptr;
	va_start( argptr, fmt );
	_vsnprintf( buf, sizeof( buf ) - 1, fmt, argptr );
	va_end( argptr );

	Msg( buf );
}

DEFINE_TESTCASE_NOSUITE( DmxRunDefaultValueLogTest )
{
	Msg( "Running CDmeTypedLog<float> default value (stereo channel w/ value 0.5) tests...\n" );
	CDisableUndoScopeGuard sg;

	DmFileId_t fileid = g_pDataModel->FindOrCreateFileId( "<DmxTestDmeLog>" );

	for ( int i = 0; i < NUM_DEF_TESTS; ++i )
	{
		data_t *pdata = defaultvaluetest[ i ];
		tvpair_t *pexpected = expectedvalues[ i ];

		// Run each test

		CDmeFloatLog *log = CreateElement<CDmeFloatLog>( "curve", fileid );
		if ( !log )
		{
			Msg( "Unable to create CDmeFloatLog object!!!" );
			continue;
		}

		log->SetDefaultValue( 0.5f );

		if ( pdata )
		{
			// Run the test
			for ( int j = 0; ; ++j )
			{
				if ( pdata[ j ].tms == -1 )
					break;

				log->SetKey( DmeTime_t(  pdata[ j ].tms ), pdata[ j ].value );
			}
		}

		// Now compare against expected values
		for ( int j = 0; j < NUM_TEST_VALUES; ++j )
		{
			DmeTime_t t = DmeTime_t( pexpected[ j ].tms );
			float v = pexpected[ j ].expectedvalue;
			float logv = log->GetValue( t );
			Shipping_Assert( v == logv );
		}

		DestroyElement( log );
	}

	g_pDataModel->RemoveFileId( fileid );
}

void RunDmeFloatLogTests( CDmeFloatLog *log )
{
	Msg( "  Testing general log data...\n" );

	ResetLog( log, false );	

	CompareFloats( 0.5f, log->GetValue( DmeTime_t( 2.0f ) ), 0.000001f, "log->GetValue( 2.0 ) expected to be 0.5f\n" );
	CompareFloats( 0.5f, log->GetValue( DmeTime_t( 2.5f ) ), 0.000001f, "log->GetValue( 2.5 ) expected to be 0.5f\n" );
	CompareFloats( 0.5f, log->GetValue( DmeTime_t( 2.5f ) ), 0.000001f, "log->GetValue( 2.5 ) expected to be 0.5f\n" );
	CompareFloats( 0.5f, log->GetValue( DmeTime_t( 6.5f ) ), 0.000001f, "log->GetValue( 6.5 ) expected to be 0.5f\n" );

	CDmeCurveInfo *pCurveInfo = log->GetOrCreateCurveInfo();

	int idx = log->FindKeyWithinTolerance( DmeTime_t( 6.0f ), DmeTime_t( 0 ) );
	Shipping_Assert( log->GetKeyTime( idx ) == DmeTime_t( 6.0f ) );
	log->SetKeyCurveType( idx, MAKE_CURVE_TYPE( INTERPOLATE_LINEAR_INTERP, INTERPOLATE_LINEAR_INTERP ) );
	log->SetKeyCurveType( idx + 1, MAKE_CURVE_TYPE( INTERPOLATE_LINEAR_INTERP, INTERPOLATE_LINEAR_INTERP ) );

	float val = log->GetValue( DmeTime_t( 6.5f ) );
	float qval = log->GetValue( DmeTime_t( 6.25f ) );

	CompareFloats( 0.5f, val, 0.000001f, "INTERPOLATE_LINEAR_INTERPlog->GetValue( 6500 ) expcted to be 0.5f\n" );
	CompareFloats( 0.75f, qval, 0.000001f, "INTERPOLATE_LINEAR_INTERPlog->GetValue( 6250 ) expcted to be 0.75f\n" );

	log->SetKeyCurveType( idx, MAKE_CURVE_TYPE( INTERPOLATE_CATMULL_ROM_NORMALIZEX, INTERPOLATE_CATMULL_ROM_NORMALIZEX ) );
	log->SetKeyCurveType( idx + 1, MAKE_CURVE_TYPE( INTERPOLATE_CATMULL_ROM_NORMALIZEX, INTERPOLATE_CATMULL_ROM_NORMALIZEX ) );

	float val2 = log->GetValue( DmeTime_t( 6.5f ) );
	float qval2 = log->GetValue( DmeTime_t( 6.25f ) );
	Shipping_Assert( val2 == val );
	Shipping_Assert( qval2 != val );

	log->SetKeyCurveType( idx, MAKE_CURVE_TYPE( INTERPOLATE_EASE_INOUT, INTERPOLATE_EASE_INOUT ) );
	log->SetKeyCurveType( idx + 1, MAKE_CURVE_TYPE( INTERPOLATE_EASE_INOUT, INTERPOLATE_EASE_INOUT ) );

	float val3 = log->GetValue( DmeTime_t( 6.5f ) );
	float qval3 = log->GetValue( DmeTime_t( 6.25f ) );
	Shipping_Assert( val3 == val );
	Shipping_Assert( qval3 != val );

	log->SetKeyCurveType( idx, MAKE_CURVE_TYPE( INTERPOLATE_EXPONENTIAL_DECAY, INTERPOLATE_EXPONENTIAL_DECAY ) );
	log->SetKeyCurveType( idx + 1, MAKE_CURVE_TYPE( INTERPOLATE_EXPONENTIAL_DECAY, INTERPOLATE_EXPONENTIAL_DECAY ) );

	float val4 = log->GetValue( DmeTime_t( 6.5f ) );
	float qval4 = log->GetValue( DmeTime_t( 6.25f ) );
	Shipping_Assert( val4 != val );
	Shipping_Assert( qval4 != val );

	log->SetKeyCurveType( idx, MAKE_CURVE_TYPE( INTERPOLATE_KOCHANEK_BARTELS, INTERPOLATE_KOCHANEK_BARTELS ) );
	log->SetKeyCurveType( idx + 1, MAKE_CURVE_TYPE( INTERPOLATE_KOCHANEK_BARTELS, INTERPOLATE_KOCHANEK_BARTELS ) );

	float val5 = log->GetValue( DmeTime_t( 6.5f ) );
	float qval5 = log->GetValue( DmeTime_t( 6.25f ) );
	Shipping_Assert( val5 == val );
	Shipping_Assert( qval5 != val );

	pCurveInfo->SetDefaultCurveType( MAKE_CURVE_TYPE( INTERPOLATE_KOCHANEK_BARTELS, INTERPOLATE_KOCHANEK_BARTELS ) );
	log->SetKeyCurveType( idx, MAKE_CURVE_TYPE( INTERPOLATE_DEFAULT, INTERPOLATE_DEFAULT ) );
	log->SetKeyCurveType( idx + 1, MAKE_CURVE_TYPE( INTERPOLATE_DEFAULT, INTERPOLATE_DEFAULT ) );

	float val6 = log->GetValue( DmeTime_t( 6.5f ) );
	float qval6 = log->GetValue( DmeTime_t( 6.25f ) );
	Shipping_Assert( val5 == val6 );
	Shipping_Assert( qval6 == qval5 );

}

void CompareLogToChoreo( CFlexAnimationTrack *track, CDmeFloatLog *log )
{
	// Now run tests
	for ( DmeTime_t t( 0 ); t < DmeTime_t( 20.0f ); t += DmeTime_t( 0.1f ) )
	{
		// Compare values
		float dmevalue = log->GetValue( t );
		float choreovalue = track->GetIntensity( t.GetSeconds() );

		CompareFloats( dmevalue, choreovalue, 0.001f, "Time(%f sec) , dme [%f] choreo[%f], diff[%f]\n",
			t.GetSeconds(),
			dmevalue,
			choreovalue,
			fabs( dmevalue - choreovalue ) );
	}
}

void ResetChoreo( CFlexAnimationTrack *track, bool useCurveTypes, int startIndex = 0, int endIndex = -1 )
{
	track->Clear();

	int i;
	int c;

	c = ARRAYSIZE( data );
	for ( i = startIndex; i < c; ++i )
	{
		data_t *e = &data[ i ];

		float t = (float)e->tms / 10000.0f;

		CExpressionSample *sample = track->AddSample( t, e->value );
		Shipping_Assert( sample );
		if ( useCurveTypes )
		{
			sample->SetCurveType( e->curvetype );
		}

		if ( endIndex != -1 && i >= endIndex )
			break;
	}
}

void RunDmeChoreoComparisons( CDmeFloatLog *log )
{
	Msg( "  Testing choreo-style log data...\n" );

	ResetLog( log, true );
	log->SetRightEdgeTime( DmeTime_t( 15.0f ) );

	CChoreoScene *scene = new CChoreoScene( NULL );
	CChoreoEvent *event = new CChoreoEvent( scene, CChoreoEvent::FLEXANIMATION, "test" );
	event->SetStartTime( 0.0f );
	event->SetEndTime( 15.0f );
	CFlexAnimationTrack *track = new CFlexAnimationTrack( event );
	track->SetFlexControllerName( "flextest" );
	track->SetComboType( false );

	ResetChoreo( track, true );

	Msg( "  Comparing default data...\n" );

	CompareLogToChoreo( track, log );

	ResetLog( log, true, 3, 14 );
	ResetChoreo( track, true, 3, 14 );

	Msg( "  Comparing subset of data...\n" );

	CompareLogToChoreo( track, log );

	Msg( "  Comparing left/right edge settings...\n" );
	// Now test right and left edge stuff
	// Enable left edge stuff
	track->SetEdgeActive( true, true );
	track->SetEdgeInfo( true, MAKE_CURVE_TYPE( INTERPOLATE_LINEAR_INTERP, INTERPOLATE_LINEAR_INTERP ), 0.75f );
	track->SetEdgeActive( false, true );
	track->SetEdgeInfo( false, MAKE_CURVE_TYPE( INTERPOLATE_EASE_OUT, INTERPOLATE_EASE_OUT ), 0.25f );

	// Same settings for log
	log->SetUseEdgeInfo( true );
	log->SetDefaultEdgeZeroValue( 0.0f );
	log->SetEdgeInfo( 0, true, 0.75f, MAKE_CURVE_TYPE( INTERPOLATE_LINEAR_INTERP, INTERPOLATE_LINEAR_INTERP ) );
	log->SetEdgeInfo( 1, true, 0.25f, MAKE_CURVE_TYPE( INTERPOLATE_EASE_OUT, INTERPOLATE_EASE_OUT ) );

	CompareLogToChoreo( track, log );

	int i;
	for ( i = 1; i < NUM_INTERPOLATE_TYPES; ++i )
	{
		Msg( "  Comparing left/right edge settings[ %s ]...\n", Interpolator_NameForInterpolator( i, true ) );

		float val = (float)i / (float)( NUM_INTERPOLATE_TYPES - 1 ) ;
		// Now test right and left edge stuff with different data
		track->SetEdgeInfo( true, MAKE_CURVE_TYPE( i, i ), val );
		track->SetEdgeInfo( false, MAKE_CURVE_TYPE( i, i ), val );
		log->SetEdgeInfo( 0, true, val, MAKE_CURVE_TYPE( i, i ) );
		log->SetEdgeInfo( 1, true, val, MAKE_CURVE_TYPE( i, i ) );

		CompareLogToChoreo( track, log );
	}

	delete event;
	delete scene;
}

DEFINE_TESTCASE_NOSUITE( DmxTestDmeLog )
{
	Msg( "Running CDmeTypedLog<float> tests...\n" );
	CDisableUndoScopeGuard sg;

	DmFileId_t fileid = g_pDataModel->FindOrCreateFileId( "<DmxTestDmeLog>" );

	CDmeFloatLog *pElement = CreateElement<CDmeFloatLog>( "curve", fileid );
	if ( !pElement )
	{
		Msg( "Unable to create CDmeFloatLog object!!!" );
		return;
	}

	// Run tests
	RunDmeFloatLogTests( pElement );

	RunDmeChoreoComparisons( pElement );

	g_pDataModel->RemoveFileId( fileid );
}