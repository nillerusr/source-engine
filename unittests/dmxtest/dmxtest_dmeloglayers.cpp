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

#include "movieobjects/dmechannel.h"
#include "movieobjects/dmelog.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define NUM_CHANNELS 1
#define NUM_LOG_ENTRIES 10

enum
{
	SPEW_DIFFS = (1<<0),
	SPEW_VALUES= (1<<1),
	SPEW_KEYS= (1<<2),
};

static void ValidateDataSets( int spew, char const *testname, CUtlVector< CUtlVector< float > >& values, CUtlVector< CUtlVector< float > >& valuesbaked )
{
	int i, j;
	// Compare baked, unbaked values
	Assert( values.Count() == valuesbaked.Count() );
	int c = values.Count();
	bool differs = false;
	bool spewvalues = ( spew & SPEW_VALUES ) ? true : false;

	float tol = 0.0001f;

	for ( i = 0; i < c; ++i )
	{
		CUtlVector< float >& v = values[ i ];
		CUtlVector< float >& vb = valuesbaked[ i ];

		Assert( v.Count() == vb.Count() );

		// Now get the values of the samples in the log
		for ( j = 0; j < v.Count(); ++j )
		{
			Assert( vb.IsValidIndex( j ) );
			if ( !vb.IsValidIndex( j ) )
				continue;

			float v1 = v[ j ];
			float v2 = vb[ j ];
			if ( fabs( v1 - v2 ) > tol )
			{
				differs = true;
			}

			if ( spewvalues )
			{
				Msg( "%d %f %f\n", j, v[ j ], vb[ j ] );
			}
		}
	}

	Msg( "   %s\n", differs ? "FAILED" : "OK" );

	if ( !(spew & SPEW_DIFFS ) )
		return;

	for ( i = 0; i < c; ++i )
	{
		CUtlVector< float >& v = values[ i ];
		CUtlVector< float >& vb = valuesbaked[ i ];

		Assert( v.Count() == vb.Count() );

		// Now get the values of the samples in the log
		for ( j = 0; j < v.Count(); ++j )
		{
			Assert( vb.IsValidIndex( j ) );
			if ( !vb.IsValidIndex( j ) )
				continue;

			if ( v[ j ] == vb[ j ] )
			{
				if ( differs )
				{
					 Msg( "%d found %f to equal %f\n", j, v[ j ], vb[ j ] );
				}
				continue;
			}

			Msg( "%d expected %f to equal %f\n", j, v[ j ], vb[ j ] );
		}
	}

	if ( differs )
	{
		Msg( "End Test '%s'\n---------------\n", testname );
	}
}

static void CreateChannels( int num, CUtlVector< CDmeChannel * >& channels, DmFileId_t fileid )
{
	CDisableUndoScopeGuard guard;

	for ( int i = 0; i < num; ++i )
	{
		CDmeChannel *channel = NULL;

		channel = CreateElement< CDmeChannel >( "channel1", fileid );
		channels.AddToTail( channel );
		channel->CreateLog( AT_FLOAT ); // only care about float logs for now
		channel->SetMode( CM_PLAY );// Make sure it's in playback mode
	}
}

struct TestLayer_t
{
	enum
	{
		TYPE_SIMPLESLOPE = 0, // value == time
		TYPE_SINE,			// sinusoidal
		TYPE_CONSTANT,
	};

	TestLayer_t() : 
		startTime( 0 ),
		endTime( 0 ),
		timeStep( 0 ),
		usecurvetype( false ),
		curvetype( CURVE_DEFAULT ),
		type( TYPE_SIMPLESLOPE ),
		constantvalue( 0.0f )
	{
	}

	float	ValueForTime( DmeTime_t time ) const
	{
		float t = (float)time.GetSeconds();
		switch ( type )
		{
		default:
		case TYPE_SIMPLESLOPE:
			return t;
		case TYPE_SINE:
			return constantvalue * ( 1.0f + sin( ( t * 0.002f ) * 2 * M_PI ) ) * 0.5f;
		case TYPE_CONSTANT:
			return constantvalue;
		}

		return t;
	}

	int		type;
	DmeTime_t		startTime;
	DmeTime_t		endTime;
	DmeTime_t		timeStep;

	bool	usecurvetype;
	int		curvetype;

	float	constantvalue;
};

struct TestParams_t
{
	TestParams_t() : 
		testundo( false ),
		usecurves( false ),
		purgevalues( true ),
		testabort( false ),
		spew( 0 ),
		spewnontopmostlayers( false ),
		defaultcurve( CURVE_DEFAULT ),
		mintime( DmeTime_t( 0 ) ),
		maxtime( DmeTime_t( 100 ) )
	{
	}
	int			spew;
	bool		usecurves;
	bool		purgevalues;
	bool		testundo;
	bool		testabort;
	bool		spewnontopmostlayers;
	int			defaultcurve;
	DmeTime_t			mintime;
	DmeTime_t			maxtime;
	CUtlVector< TestLayer_t >	layers;

	void		Reset()
	{
		purgevalues = true;
		usecurves = false;
		testundo = false;
		testabort = false;
		spewnontopmostlayers = false;
		spew = 0;
		mintime = DmeTime_t( 0 );
		maxtime = DmeTime_t( 100 );
		defaultcurve = CURVE_DEFAULT;
		layers.RemoveAll();
	}

	void		AddLayer( DmeTime_t start, DmeTime_t end, DmeTime_t step, int curvetype, int valuetype, float constantvalue = 0.0f )
	{
		TestLayer_t tl;
		tl.startTime = start;
		tl.endTime = end;
		tl.timeStep = step;
		tl.curvetype = curvetype;
		tl.type = valuetype;
		tl.constantvalue = constantvalue;

		layers.AddToTail( tl );
	}
};

static void RunLayerTest( char const *testname, CUtlVector< CDmeChannel * >& channels, const TestParams_t& params )
{
	if ( params.layers.Count() == 0 )
	{
		Assert( 0 );
		return;
	}

	Msg( "Test '%s'...\n", testname );

	g_pDataModel->StartUndo( testname, testname );

	int i;
	int c = channels.Count();

	{
		CDisableUndoScopeGuard guard;

		for ( i = 0; i < NUM_CHANNELS; ++i )
		{
			CDmeChannel *channel = channels[ i ];
			CDmeTypedLog< float > *pLog = CastElement< CDmeTypedLog< float > >( channel->GetLog() );
			Assert( pLog );
			pLog->ClearKeys(); // reset it

			CDmeCurveInfo *pCurveInfo = NULL;
			if ( params.usecurves )
			{
				pCurveInfo = pLog->GetOrCreateCurveInfo();
				pCurveInfo->SetDefaultCurveType( params.defaultcurve );
				pCurveInfo->SetMinValue( 0.0f );
				pCurveInfo->SetMaxValue( 1000.0f );
			}
			else
			{
				if ( pLog->GetCurveInfo() )
				{
					g_pDataModel->DestroyElement( pLog->GetCurveInfo()->GetHandle() );
				}
				pLog->SetCurveInfo( NULL );
			}

			const TestLayer_t& tl = params.layers[ 0 ];
			// Now add entries
			DmeTime_t logStep  = tl.timeStep;
			DmeTime_t logStart = tl.startTime;

			for ( DmeTime_t t = logStart; t <= tl.endTime + logStep - DmeTime_t( 1 ); t += logStep )
			{
				DmeTime_t useTime = t;
				if ( useTime > tl.endTime )
				{
					useTime = tl.endTime;
				}
				float value = tl.ValueForTime( useTime );
				if ( tl.usecurvetype )
				{
					pLog->SetKey( useTime, value, tl.curvetype );
				}
				else
				{
					pLog->SetKey( useTime, value );
				}
			}
		}
	}

	for ( int layer = 1; layer < params.layers.Count(); ++layer )
	{
		const TestLayer_t& tl = params.layers[ layer ];

		// Test creating a layer and collapsing it back down
		g_pChannelRecordingMgr->StartLayerRecording( "layer operations" );
		for ( i = 0; i < c; ++i )
		{
			g_pChannelRecordingMgr->AddChannelToRecordingLayer( channels[ i ] );  // calls log->CreateNewLayer()
		}

		// Now add values to channel logs
		for ( i = 0; i < c; ++i )
		{
			CDmeChannel *channel = channels[ i ];
			CDmeTypedLog< float > *pLog = CastElement< CDmeTypedLog< float > >( channel->GetLog() );
			Assert( pLog );

			// Now add entries
			DmeTime_t logStep  = tl.timeStep;
			DmeTime_t logStart = tl.startTime;

			for ( DmeTime_t t = logStart; t <= tl.endTime + logStep - DmeTime_t( 1 ); t += logStep )
			{
				DmeTime_t useTime = t;
				if ( useTime > tl.endTime )
				{
					useTime = tl.endTime;
				}
				float value = tl.ValueForTime( useTime );
				if ( tl.usecurvetype )
				{
					pLog->SetKey( useTime, value, tl.curvetype );
				}
				else
				{
					pLog->SetKey( useTime, value );
				}
			}
		}

		g_pChannelRecordingMgr->FinishLayerRecording( 0.0f, false ); // don't flatten layers here, we'll do it manually
	}

	// Now sample values
	CUtlVector< CUtlVector< float > > values;
	CUtlVector< CUtlVector< float > > valuesbaked;

	for ( i = 0; i < c; ++i )
	{
		CDmeChannel *channel = channels[ i ];
		CDmeTypedLog< float > *pLog = CastElement< CDmeTypedLog< float > >( channel->GetLog() );
		Assert( pLog );

		int idx = values.AddToTail();

		CUtlVector< float >& v = values[ idx ];

		// Now get the values of the samples in the log
		for ( DmeTime_t j = params.mintime; j <= params.maxtime; j += DmeTime_t( 1 ) )
		{
			float fval = pLog->GetValue( j );
			v.AddToTail( fval );
		}
	}

	if ( params.spewnontopmostlayers )
	{
		for ( i = 0; i < c; ++i )
		{
			CDmeChannel *channel = channels[ i ];
			CDmeTypedLog< float > *pLog = CastElement< CDmeTypedLog< float > >( channel->GetLog() );
			Assert( pLog );

			// Now get the values of the samples in the log
			for ( DmeTime_t j = params.mintime; j <= params.maxtime; j += DmeTime_t( 1 ) )
			{
				float topValue = pLog->GetValue( j );
				float underlyingValue = pLog->GetValueSkippingTopmostLayer( j );

				Msg( "t(%d) top [%f] rest [%f]\n",
					j.GetTenthsOfMS(), topValue, underlyingValue );
			}
		}
	}

	// Now test creating a layer and "undo/redo" of the layer
	if ( params.testundo )
	{
		g_pDataModel->FinishUndo();
		g_pDataModel->Undo();
		g_pDataModel->Redo();
		g_pDataModel->StartUndo( testname, testname );
	}

	{
		CUndoScopeGuard guard( "Bake Layers" );
		// Now bake down and resample values
		for ( i = 0; i < c; ++i )
		{
			CDmeChannel *channel = channels[ i ];
			CDmeTypedLog< float > *pLog = CastElement< CDmeTypedLog< float > >( channel->GetLog() );
			Assert( pLog );

			pLog->FlattenLayers( 0.0f, params.spew & SPEW_DIFFS );

			int idx = valuesbaked.AddToTail();

			CUtlVector< float >& v = valuesbaked[ idx ];

			// Now get the values of the samples in the log
			for ( DmeTime_t j = params.mintime; j <= params.maxtime; j += DmeTime_t( 1 ) )
			{
				float fval = pLog->GetValue( j );
				v.AddToTail( fval );
			}
		}
	}

	ValidateDataSets( params.spew, testname, values, valuesbaked );

	// Now test creating a layer and "undo/redo" of the layer
	if ( params.testundo )
	{
		g_pDataModel->FinishUndo();
		g_pDataModel->Undo();
		g_pDataModel->Redo();
		g_pDataModel->StartUndo( testname, testname );
	}

	if ( params.testabort )
	{
		g_pDataModel->AbortUndoableOperation();
	}
	else
	{
		g_pDataModel->FinishUndo();
	}
}

static void RunTimeSelectionTest( char const *testname, CUtlVector< CDmeChannel * >& channels, 
	 const TestParams_t& params, DmeTime_t tHeadPosition, DmeLog_TimeSelection_t& ts, float value )
{
	if ( params.layers.Count() == 0 )
	{
		Assert( 0 );
		return;
	}

	Msg( "Test '%s'...\n", testname );

	int i, j;
	int c = channels.Count();

	if ( params.purgevalues )
	{
		CDisableUndoScopeGuard guard;

		for ( i = 0; i < NUM_CHANNELS; ++i )
		{
			CDmeChannel *channel = channels[ i ];
			CDmeTypedLog< float > *pLog = CastElement< CDmeTypedLog< float > >( channel->GetLog() );
			Assert( pLog );
			pLog->ClearKeys(); // reset it

			CDmeCurveInfo *pCurveInfo = params.usecurves ? pLog->GetOrCreateCurveInfo() : pLog->GetCurveInfo();
			if ( params.usecurves )
			{
				pCurveInfo->SetDefaultCurveType( params.defaultcurve );
				pCurveInfo->SetMinValue( 0.0f );
				pCurveInfo->SetMaxValue( 1000.0f );
			}
			else if ( !params.usecurves && pCurveInfo )
			{
				g_pDataModel->DestroyElement( pCurveInfo->GetHandle() );
				pLog->SetCurveInfo( NULL );
			}

			const TestLayer_t& tl = params.layers[ 0 ];
			// Now add entries
			DmeTime_t logStep  = tl.timeStep;
			DmeTime_t logStart = tl.startTime;

			for ( DmeTime_t t = logStart; t <= tl.endTime + logStep - DmeTime_t( 1 ); t += logStep )
			{
				DmeTime_t useTime = t;
				if ( useTime > tl.endTime )
					useTime = tl.endTime;

				float value = tl.ValueForTime( useTime );
				if ( tl.usecurvetype )
				{
					pLog->SetKey( useTime, value, tl.curvetype );
				}
				else
				{
					pLog->SetKey( useTime, value );
				}
			}
		}
	}

	// Test creating a layer and collapsing it back down
	g_pChannelRecordingMgr->StartLayerRecording( "layer operations", &ts );
	for ( i = 0; i < c; ++i )
	{
		g_pChannelRecordingMgr->AddChannelToRecordingLayer( channels[ i ] );  // calls log->CreateNewLayer()
	}

	// Now add values to channel logs
	for ( i = 0; i < c; ++i )
	{
		CDmeChannel *channel = channels[ i ];
		CDmeTypedLog< float > *pLog = CastElement< CDmeTypedLog< float > >( channel->GetLog() );
		Assert( pLog );

		pLog->StampKeyAtHead( tHeadPosition, tHeadPosition, ts, value );
	}

	// Flattens the layers
	g_pChannelRecordingMgr->FinishLayerRecording( 0.0f, true );

	if ( params.spew & SPEW_VALUES )
	{
		for ( i = 0; i < c; ++i )
		{
			CDmeChannel *channel = channels[ i ];
			CDmeTypedLog< float > *pLog = CastElement< CDmeTypedLog< float > >( channel->GetLog() );
			Assert( pLog );
			Assert( pLog->GetNumLayers() == 1 );

			for ( DmeTime_t j = params.mintime; j <= params.maxtime; j += DmeTime_t( 1 ) )
			{
				float fval = pLog->GetValue( j );
				Msg( "%d %f\n", j.GetTenthsOfMS(), fval );
			}
		}
	}

	if ( params.spew & SPEW_KEYS )
	{
		for ( i = 0; i < c; ++i )
		{
			CDmeChannel *channel = channels[ i ];
			CDmeTypedLog< float > *pLog = CastElement< CDmeTypedLog< float > >( channel->GetLog() );
			Assert( pLog );
			Assert( pLog->GetNumLayers() == 1 );

			int kc = pLog->GetKeyCount();
			for ( j = 0; j < kc; ++j )
			{
				DmeTime_t time = pLog->GetKeyTime( j );

				float fval = pLog->GetValue( time );
				Msg( "%d %f %f\n", j, time.GetSeconds(), fval );
			}
		}
	}

	// Now test creating a layer and "undo/redo" of the layer
	if ( params.testundo )
	{
		g_pDataModel->Undo();
		g_pDataModel->Redo();
	}
}

DEFINE_TESTCASE_NOSUITE( DmxTestDmeLogLayers )
{
	Msg( "Running CDmeTypedLog<float> layering tests...\n" );

#ifdef _DEBUG
	int nStartingCount = g_pDataModel->GetAllocatedElementCount();
#endif

	CUtlVector< CDmeChannel * > channels;

	DmFileId_t fileid = g_pDataModel->FindOrCreateFileId( "<DmxTestDmeLogLayers>" );

	CreateChannels( NUM_CHANNELS, channels, fileid );

	TestParams_t params;
	{
		params.testundo = false;
		params.usecurves = false;
		params.defaultcurve = CURVE_DEFAULT;
		params.AddLayer( DmeTime_t( 0 ), DmeTime_t( 100 ), DmeTime_t( 10 ), CURVE_DEFAULT, TestLayer_t::TYPE_SIMPLESLOPE );
		params.AddLayer( DmeTime_t( 5 ), DmeTime_t( 95 ),  DmeTime_t( 10 ), CURVE_DEFAULT, TestLayer_t::TYPE_SIMPLESLOPE );
		RunLayerTest( "One-Layer", channels, params );
		params.Reset();
	}

	// Single layer using lerp everywhere
	//   -----------------------
	{
		params.testundo = false;
		params.usecurves = true;
		params.defaultcurve = CURVE_LINEAR_INTERP_TO_LINEAR_INTERP;
		params.AddLayer( DmeTime_t( 0 ), DmeTime_t( 100 ),  DmeTime_t( 10 ), CURVE_LINEAR_INTERP_TO_LINEAR_INTERP, TestLayer_t::TYPE_SIMPLESLOPE );
		RunLayerTest( "One-Layer Lerp", channels, params );
		params.Reset();
	}

	// Two layers using lerp
	//   ----------------------------
	//     ------------------------
	{
		params.testundo = false;
		params.usecurves = true;
		params.defaultcurve = CURVE_LINEAR_INTERP_TO_LINEAR_INTERP;
		params.AddLayer( DmeTime_t( 0 ), DmeTime_t( 100 ),  DmeTime_t( 10 ), CURVE_LINEAR_INTERP_TO_LINEAR_INTERP, TestLayer_t::TYPE_SIMPLESLOPE );
		params.AddLayer( DmeTime_t( 5 ), DmeTime_t( 95 ),  DmeTime_t( 10 ), CURVE_LINEAR_INTERP_TO_LINEAR_INTERP, TestLayer_t::TYPE_SIMPLESLOPE );
		RunLayerTest( "Two-Layer Lerp (contained)", channels, params );
		params.Reset();
	}

	// Two layers using CURVE_EASE_IN_TO_EASE_OUT, there should be some disparity
	//   ----------------------------
	//     ------------------------
	{
		params.testundo = false;
		params.usecurves = true;
		params.defaultcurve = CURVE_EASE_IN_TO_EASE_OUT;
		params.AddLayer( DmeTime_t( 0 ), DmeTime_t( 100 ),  DmeTime_t( 10 ), CURVE_EASE_IN_TO_EASE_OUT, TestLayer_t::TYPE_SIMPLESLOPE );
		params.AddLayer( DmeTime_t( 5 ), DmeTime_t( 95 ),  DmeTime_t( 10 ), CURVE_EASE_IN_TO_EASE_OUT, TestLayer_t::TYPE_SIMPLESLOPE );
		RunLayerTest( "Two-Layer Ease In/Out (contained)", channels, params );
		params.Reset();
	}

	// Two layers using lerp
	//     ----------------------------
	//   ---------
	{
		params.testundo = false;
		params.usecurves = true;
		params.mintime = DmeTime_t( -20 );
		params.defaultcurve = CURVE_LINEAR_INTERP_TO_LINEAR_INTERP;
		params.AddLayer( DmeTime_t( 0 ), DmeTime_t( 100 ),  DmeTime_t( 10 ), CURVE_LINEAR_INTERP_TO_LINEAR_INTERP, TestLayer_t::TYPE_SIMPLESLOPE );
		params.AddLayer( DmeTime_t( -20 ), DmeTime_t( 20 ),  DmeTime_t( 10 ), CURVE_LINEAR_INTERP_TO_LINEAR_INTERP, TestLayer_t::TYPE_SIMPLESLOPE );
		RunLayerTest( "Two-Layer Lerp (overhang start)", channels, params );
		params.Reset();
	}

	// Two layers using lerp
	//   ----------------------------
	//                          ------------
	{
		params.testundo = false;
		params.usecurves = true;
		params.maxtime = DmeTime_t( 120 );
		params.defaultcurve = CURVE_LINEAR_INTERP_TO_LINEAR_INTERP;
		params.AddLayer( DmeTime_t( 0 ), DmeTime_t( 100 ),  DmeTime_t( 10 ), CURVE_LINEAR_INTERP_TO_LINEAR_INTERP, TestLayer_t::TYPE_SIMPLESLOPE );
		params.AddLayer( DmeTime_t( 80 ), DmeTime_t( 120 ),  DmeTime_t( 10 ), CURVE_LINEAR_INTERP_TO_LINEAR_INTERP, TestLayer_t::TYPE_SIMPLESLOPE );
		RunLayerTest( "Two-Layer Lerp (overhang end)", channels, params );
		params.Reset();
	}
	// Three layers using lerp
	//   -------------
	// -----        -----
	{
		params.testundo = false;
		params.usecurves = true;
		params.defaultcurve = CURVE_LINEAR_INTERP_TO_LINEAR_INTERP;
		params.mintime = DmeTime_t( -12 );
		params.maxtime = DmeTime_t( 115 );
		params.AddLayer( DmeTime_t( 0 ), DmeTime_t( 100 ),  DmeTime_t( 10 ), CURVE_LINEAR_INTERP_TO_LINEAR_INTERP, TestLayer_t::TYPE_SIMPLESLOPE );
		params.AddLayer( DmeTime_t( -12 ), DmeTime_t( 12 ), DmeTime_t( 4 ), CURVE_LINEAR_INTERP_TO_LINEAR_INTERP, TestLayer_t::TYPE_SIMPLESLOPE );
		params.AddLayer( DmeTime_t( 85 ), DmeTime_t( 115 ), DmeTime_t( 5 ), CURVE_LINEAR_INTERP_TO_LINEAR_INTERP, TestLayer_t::TYPE_SIMPLESLOPE );
		RunLayerTest( "Three-Layer Lerp (overhang start + end)", channels, params );
		params.Reset();
	}

	// Three layers using lerp
	//   -------------
	//	     ----- 
	//        --
	{
		params.testundo = false;
		params.usecurves = true;
		params.defaultcurve = CURVE_LINEAR_INTERP_TO_LINEAR_INTERP;
		params.AddLayer( DmeTime_t( 0 ), DmeTime_t( 100 ),  DmeTime_t( 10 ), CURVE_LINEAR_INTERP_TO_LINEAR_INTERP, TestLayer_t::TYPE_SIMPLESLOPE );
		params.AddLayer( DmeTime_t( 25 ), DmeTime_t( 75 ),  DmeTime_t( 10 ), CURVE_LINEAR_INTERP_TO_LINEAR_INTERP, TestLayer_t::TYPE_SIMPLESLOPE );
		params.AddLayer( DmeTime_t( 40 ), DmeTime_t( 60 ),  DmeTime_t( 5 ), CURVE_LINEAR_INTERP_TO_LINEAR_INTERP, TestLayer_t::TYPE_SIMPLESLOPE );
		RunLayerTest( "Three-Layer Lerp (layer inside layer)", channels, params );
		params.Reset();
	}

	// Three layers using lerp
	//   -------------
	//	     ----- 
	//           --
	{
		params.testundo = false;
		params.usecurves = true;
		params.defaultcurve = CURVE_LINEAR_INTERP_TO_LINEAR_INTERP;
		params.AddLayer( DmeTime_t( 0 ), DmeTime_t( 100 ),  DmeTime_t( 10 ), CURVE_LINEAR_INTERP_TO_LINEAR_INTERP, TestLayer_t::TYPE_SIMPLESLOPE );
		params.AddLayer( DmeTime_t( 25 ), DmeTime_t( 75 ),  DmeTime_t( 10 ), CURVE_LINEAR_INTERP_TO_LINEAR_INTERP, TestLayer_t::TYPE_SIMPLESLOPE );
		params.AddLayer( DmeTime_t( 70 ), DmeTime_t( 80 ), DmeTime_t( 2 ), CURVE_LINEAR_INTERP_TO_LINEAR_INTERP, TestLayer_t::TYPE_SIMPLESLOPE );
		RunLayerTest( "Three-Layer Lerp (first layer contained, second layer overlapping first at end)", channels, params );
		params.Reset();
	}

	// Three layers using lerp
	//   -------------
	//	     ----- 
	//      --
	{
		params.testundo = false;
		params.usecurves = true;
		params.defaultcurve = CURVE_LINEAR_INTERP_TO_LINEAR_INTERP;
		params.AddLayer( DmeTime_t( 0 ), DmeTime_t( 100 ),  DmeTime_t( 10 ), CURVE_LINEAR_INTERP_TO_LINEAR_INTERP, TestLayer_t::TYPE_SIMPLESLOPE );
		params.AddLayer( DmeTime_t( 25 ), DmeTime_t( 75 ),  DmeTime_t( 10 ), CURVE_LINEAR_INTERP_TO_LINEAR_INTERP, TestLayer_t::TYPE_SIMPLESLOPE );
		params.AddLayer( DmeTime_t( 15 ), DmeTime_t( 35 ), DmeTime_t( 5 ), CURVE_LINEAR_INTERP_TO_LINEAR_INTERP, TestLayer_t::TYPE_SIMPLESLOPE );
		RunLayerTest( "Three-Layer Lerp (first layer contained, second layer overlapping first at start)", channels, params );
		params.Reset();
	}

	// Four layers using lerp
	//   ---------------
	//	  -----   
	//             ----
	//       -------
	{
		params.testundo = false;
		params.usecurves = true;
		// params.spewnontopmostlayers = true;
		params.defaultcurve = CURVE_LINEAR_INTERP_TO_LINEAR_INTERP;
		params.AddLayer( DmeTime_t( 0 ), DmeTime_t( 100 ),  DmeTime_t( 10 ), CURVE_LINEAR_INTERP_TO_LINEAR_INTERP, TestLayer_t::TYPE_CONSTANT, 20.0f );
		params.AddLayer( DmeTime_t( 15 ), DmeTime_t( 40 ),  DmeTime_t( 10 ), CURVE_LINEAR_INTERP_TO_LINEAR_INTERP, TestLayer_t::TYPE_SIMPLESLOPE );
		params.AddLayer( DmeTime_t( 60 ), DmeTime_t( 85 ), DmeTime_t( 5 ), CURVE_LINEAR_INTERP_TO_LINEAR_INTERP, TestLayer_t::TYPE_SIMPLESLOPE );
		params.AddLayer( DmeTime_t( 35 ), DmeTime_t( 79 ), DmeTime_t( 5 ), CURVE_LINEAR_INTERP_TO_LINEAR_INTERP, TestLayer_t::TYPE_SIMPLESLOPE );
		RunLayerTest( "Four-Layer Lerp (top overlapping end of 1st and start of 2nd layer)", channels, params );
		params.Reset();
	}

	// Single layer using lerp everywhere
	//   -----------------------
	{
		params.testundo = false;
		params.usecurves = true;
		params.spew = 0; //SPEW_VALUES | SPEW_KEYS;
		params.mintime = DmeTime_t( 0 );
		params.maxtime = DmeTime_t( 10000 );
		params.defaultcurve = CURVE_LINEAR_INTERP_TO_LINEAR_INTERP;
		params.AddLayer( DmeTime_t( 0 ), DmeTime_t( 10000 ), DmeTime_t( 20 ), CURVE_LINEAR_INTERP_TO_LINEAR_INTERP, TestLayer_t::TYPE_SINE, 100.0f );

		DmeTime_t tHeadPosition = DmeTime_t( 5000 );

		DmeLog_TimeSelection_t ts;
		ts.m_nTimes[ TS_LEFT_FALLOFF ] = tHeadPosition + DmeTime_t( -987 );
		ts.m_nTimes[ TS_LEFT_HOLD ] = ts.m_nTimes[ TS_RIGHT_HOLD ] = tHeadPosition;
		ts.m_nTimes[ TS_RIGHT_FALLOFF ] = tHeadPosition + DmeTime_t( 1052 );
		ts.m_nFalloffInterpolatorTypes[ 0 ] = ts.m_nFalloffInterpolatorTypes[ 1 ] = INTERPOLATE_EASE_INOUT;

		// Resample at 50 msec intervals
		ts.m_bResampleMode = true;
		ts.m_nResampleInterval = DmeTime_t( 50 );

		///params.spew |= SPEW_KEYS | SPEW_VALUES;

		RunTimeSelectionTest( "One-Layer Time Selection at 50, falloff 25, EASE_INOUT interp", channels, params, tHeadPosition, ts, 250 );

		params.purgevalues = false;
		// params.spew |= SPEW_VALUES;

		// Shift the head and do it all again
		tHeadPosition = DmeTime_t( 2000 );
		ts.m_nTimes[ TS_LEFT_FALLOFF ] = DmeTime_t( 1487 );
		ts.m_nTimes[ TS_LEFT_HOLD ] = ts.m_nTimes[ TS_RIGHT_HOLD ] = tHeadPosition;
		ts.m_nTimes[ TS_RIGHT_FALLOFF ] = tHeadPosition + DmeTime_t( 631 ); 

		RunTimeSelectionTest( "2nd layer", channels, params, tHeadPosition, ts, 500 );
		params.Reset();
	}
	// Single layer using lerp everywhere
	//   -----------------------
	{
		params.testundo = true;
		params.usecurves = true;
		params.spew = 0; //SPEW_VALUES | SPEW_KEYS;
		params.mintime = DmeTime_t( 0 );
		params.maxtime = DmeTime_t( 1000 );
		params.defaultcurve = CURVE_LINEAR_INTERP_TO_LINEAR_INTERP;
		params.AddLayer( DmeTime_t( 0 ), DmeTime_t( 1000 ), DmeTime_t( 20 ), CURVE_LINEAR_INTERP_TO_LINEAR_INTERP, TestLayer_t::TYPE_CONSTANT, 100.0f );

		DmeTime_t tHeadPosition = DmeTime_t( 500 );
		DmeLog_TimeSelection_t ts;
		ts.m_nTimes[ TS_LEFT_FALLOFF ] = tHeadPosition + DmeTime_t( -100 );
		ts.m_nTimes[ TS_LEFT_HOLD ] = ts.m_nTimes[ TS_RIGHT_HOLD ] = tHeadPosition;
		ts.m_nTimes[ TS_RIGHT_FALLOFF ] = tHeadPosition + DmeTime_t( 100 );
		ts.m_nFalloffInterpolatorTypes[ 0 ] = ts.m_nFalloffInterpolatorTypes[ 1 ] = INTERPOLATE_LINEAR_INTERP;

		// Resample at 50 msec intervals
		ts.m_bResampleMode = true;
		ts.m_nResampleInterval = DmeTime_t( 10 );

//		params.spew |= SPEW_VALUES;

		RunTimeSelectionTest( "Resetting layer", channels, params, tHeadPosition, ts, 200 );

		params.purgevalues = false;
		//params.spew |= SPEW_VALUES;

		// Shift the head and do it all again
		//ts.m_nRelativeFalloffTimes[ 0 ] = 1487 - 2000;
		//ts.m_nRelativeHoldTimes[ 0 ] = ts.m_nRelativeHoldTimes[ 1 ] = 0;
		//ts.m_nRelativeFalloffTimes[ 1 ] = 631; 
		//ts.SetHeadPosition( 2000 );

		RunTimeSelectionTest( "2nd layer", channels, params, tHeadPosition, ts, 110 );
		params.Reset();
	}
//	g_pDataModel->TraceUndo( true );

	// Test abort undo stuff
	for ( int i = 0; i < 2; ++i )
		// Four layers using lerp
		//   ---------------
		//	  -----   
		//             ----
		//       -------
	{
		params.testundo = false;
		params.testabort = i != 1 ? true : false;
		params.usecurves = false;
		// params.spewnontopmostlayers = true;
		params.defaultcurve = CURVE_LINEAR_INTERP_TO_LINEAR_INTERP;
		params.AddLayer( DmeTime_t( 0 ), DmeTime_t( 10 ),  DmeTime_t( 10 ), CURVE_LINEAR_INTERP_TO_LINEAR_INTERP, TestLayer_t::TYPE_CONSTANT, 20.0f );
		params.AddLayer( DmeTime_t( 5 ), DmeTime_t( 6 ),  DmeTime_t( 1 ), CURVE_LINEAR_INTERP_TO_LINEAR_INTERP, TestLayer_t::TYPE_SIMPLESLOPE );
		RunLayerTest( "Four-Layer Lerp (top overlapping end of 1st and start of 2nd layer)", channels, params );
		params.Reset();
	}

	//	g_pDataModel->TraceUndo( false );


	//DestroyChannels( channels );

	g_pDataModel->ClearUndo();

	g_pDataModel->RemoveFileId( fileid );

#ifdef _DEBUG
	int nEndingCount = g_pDataModel->GetAllocatedElementCount();
	AssertEquals( nEndingCount, nStartingCount );
	if ( nEndingCount != nStartingCount )
	{
		for ( DmElementHandle_t hElement = g_pDataModel->FirstAllocatedElement() ;
			hElement != DMELEMENT_HANDLE_INVALID;
			hElement = g_pDataModel->NextAllocatedElement( hElement ) )
		{
			CDmElement *pElement = g_pDataModel->GetElement( hElement );
			Assert( pElement );
			if ( !pElement )
				return;

			Msg( "[%s : %s] in memory\n", pElement->GetName(), pElement->GetTypeString() );
		}
	}
#endif
}

DEFINE_TESTCASE_NOSUITE( DmxTestDmeLogLayersUndo )
{
	Msg( "Running CDmeTypedLog<float> layering UNDO tests...\n" );

#ifdef _DEBUG
	int nStartingCount = g_pDataModel->GetAllocatedElementCount();
#endif

	CUtlVector< CDmeChannel * > channels;
	
	DmFileId_t fileid = g_pDataModel->FindOrCreateFileId( "<DmxTestDmeLogLayersUndo>" );

	CreateChannels( NUM_CHANNELS, channels, fileid );

	TestParams_t params;
	
//	g_pDataModel->TraceUndo( true );

	// Test abort undo stuff
	for ( int i = 0; i < 2; ++i )
	{
		params.testundo = false;
		params.testabort = true;
		params.usecurves = false;
		// params.spewnontopmostlayers = true;
		params.defaultcurve = CURVE_LINEAR_INTERP_TO_LINEAR_INTERP;
		params.AddLayer( DmeTime_t( 0 ), DmeTime_t( 1000 ),  DmeTime_t( 10 ), CURVE_LINEAR_INTERP_TO_LINEAR_INTERP, TestLayer_t::TYPE_CONSTANT, 20.0f );
		params.AddLayer( DmeTime_t( 100 ), DmeTime_t( 900 ),  DmeTime_t( 5 ), CURVE_LINEAR_INTERP_TO_LINEAR_INTERP, TestLayer_t::TYPE_SIMPLESLOPE );
		RunLayerTest( "Abort undo", channels, params );
		params.Reset();
	}

//	g_pDataModel->TraceUndo( false );

	g_pDataModel->ClearUndo();
	g_pDataModel->RemoveFileId( fileid );

#ifdef _DEBUG
	int nEndingCount = g_pDataModel->GetAllocatedElementCount();
	AssertEquals( nEndingCount, nStartingCount );
	if ( nEndingCount != nStartingCount )
	{
		for ( DmElementHandle_t hElement = g_pDataModel->FirstAllocatedElement() ;
			hElement != DMELEMENT_HANDLE_INVALID;
			hElement = g_pDataModel->NextAllocatedElement( hElement ) )
		{
			CDmElement *pElement = g_pDataModel->GetElement( hElement );
			Assert( pElement );
			if ( !pElement )
				return;

			Msg( "[%s : %s] in memory\n", pElement->GetName(), pElement->GetTypeString() );
		}
	}
#endif
}
