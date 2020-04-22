//========= Copyright Valve Corporation, All rights reserved. ============//
// traceperf.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "gametrace.h"
#include "fmtstr.h"
#include "appframework/appframework.h"
#include "filesystem.h"
#include "filesystem_init.h"
#include "tier1/tier1.h"
#include "tier2/tier2.h"
#include "tier3/tier3.h"

IPhysicsCollision *physcollision = NULL;

#define NUM_COLLISION_TESTS 2500

void ReadPHYFile(const char *name, vcollide_t &collide )
{
	FileHandle_t fp = g_pFullFileSystem->Open(name, "rb");
	if (!fp)
		Error ("Couldn't open %s", name);

	phyheader_t header;

	g_pFullFileSystem->Read( &header, sizeof(header), fp );
	if ( header.size != sizeof(header) || header.solidCount <= 0 )
		return;

	int fileSize = g_pFullFileSystem->Size(fp);

	char *buf = (char *)_alloca( fileSize );
	g_pFullFileSystem->Read( buf, fileSize, fp );
	g_pFullFileSystem->Close( fp );

	physcollision->VCollideLoad( &collide, header.solidCount, (const char *)buf, fileSize );
}


struct testlist_t
{
	Vector start;
	Vector end;
	Vector normal;
	bool hit;
};

struct benchresults_t
{
	int		collisionTests;
	int		collisionHits;
	float	totalTime;
	float	rayTime;
	float	boxTime;
};

testlist_t g_Traces[NUM_COLLISION_TESTS];
void Benchmark_PHY( const CPhysCollide *pCollide, benchresults_t *pOut )
{
	int i;
	Vector start = vec3_origin;
	static Vector *targets = NULL;
	static bool first = true;
	static float test[2] = {1,1};
	if ( first )
	{
		float radius = 0;
		float theta = 0;
		float phi = 0;
		for ( int i = 0; i < NUM_COLLISION_TESTS; i++ )
		{
			radius += NUM_COLLISION_TESTS * 123.123f;
			radius = fabs(fmod(radius, 128));
			theta += NUM_COLLISION_TESTS * 0.76f;
			theta = fabs(fmod(theta, DEG2RAD(360)));
			phi += NUM_COLLISION_TESTS * 0.16666666f;
			phi = fabs(fmod(phi, DEG2RAD(180)));

			float st, ct, sp, cp;
			SinCos( theta, &st, &ct );
			SinCos( phi, &sp, &cp );
			st = sin(theta);
			ct = cos(theta);
			sp = sin(phi);
			cp = cos(phi);

			g_Traces[i].start.x = radius * ct * sp;
			g_Traces[i].start.y = radius * st * sp;
			g_Traces[i].start.z = radius * cp;
		}
		first = false;
	}

	float duration = 0;
	Vector size[2];
	size[0].Init(0,0,0);
	size[1].Init(16,16,16);

#if VPROF_LEVEL > 0 
	g_VProfCurrentProfile.Reset();
	g_VProfCurrentProfile.ResetPeaks();
	g_VProfCurrentProfile.Start();
#endif

#if TEST_BBOX
	Vector mins, maxs;
	physcollision->CollideGetAABB( &mins, &maxs, pCollide, Vector(-500, 200, -100), vec3_angle );
	Vector extents = maxs - mins;
	Vector center = 0.5f * (maxs+mins);
	Msg("bbox: %.2f,%.2f, %.2f @ %.2f, %.2f, %.2f\n", extents.x, extents.y, extents.z, center.x, center.y, center.z );
#endif
	unsigned int hitCount = 0;
	double startTime = Plat_FloatTime();
	trace_t tr;
	for ( i = 0; i < NUM_COLLISION_TESTS; i++ )
	{
		physcollision->TraceBox( g_Traces[i].start, start, -size[0], size[0], pCollide, vec3_origin, vec3_angle, &tr );
		if ( tr.DidHit() )
		{
			g_Traces[i].end = tr.endpos;
			g_Traces[i].normal = tr.plane.normal;
			g_Traces[i].hit = true;
			hitCount++;
		}
		else
		{
			g_Traces[i].hit = false;
		}
	}
	for ( i = 0; i < NUM_COLLISION_TESTS; i++ )
	{
		physcollision->TraceBox( g_Traces[i].start, start, -size[1], size[1], pCollide, vec3_origin, vec3_angle, &tr );
	}
	duration = Plat_FloatTime() - startTime;

#if VPROF_LEVEL > 0 
	g_VProfCurrentProfile.MarkFrame();
	g_VProfCurrentProfile.Stop();
	g_VProfCurrentProfile.Reset();
	g_VProfCurrentProfile.ResetPeaks();
	g_VProfCurrentProfile.Start();
#endif
	hitCount = 0;
	startTime = Plat_FloatTime();
	for ( i = 0; i < NUM_COLLISION_TESTS; i++ )
	{
		physcollision->TraceBox( g_Traces[i].start, start, -size[0], size[0], pCollide, vec3_origin, vec3_angle, &tr );
		if ( tr.DidHit() )
		{
			g_Traces[i].end = tr.endpos;
			g_Traces[i].normal = tr.plane.normal;
			g_Traces[i].hit = true;
			hitCount++;
		}
		else
		{
			g_Traces[i].hit = false;
		}
#if VPROF_LEVEL > 0 
		g_VProfCurrentProfile.MarkFrame();
#endif
	}
	double midTime = Plat_FloatTime();
	for ( i = 0; i < NUM_COLLISION_TESTS; i++ )
	{
		physcollision->TraceBox( g_Traces[i].start, start, -size[1], size[1], pCollide, vec3_origin, vec3_angle, &tr );
#if VPROF_LEVEL > 0 
		g_VProfCurrentProfile.MarkFrame();
#endif
	}
	double endTime = Plat_FloatTime();
	duration = endTime - startTime;
	pOut->collisionTests = NUM_COLLISION_TESTS;
	pOut->collisionHits = hitCount;
	pOut->totalTime = duration * 1000.0f;
	pOut->rayTime = (midTime - startTime) * 1000.0f;
	pOut->boxTime = (endTime - midTime)*1000.0f;

#if VPROF_LEVEL > 0 
	g_VProfCurrentProfile.Stop();
	g_VProfCurrentProfile.OutputReport( VPRT_FULL & ~VPRT_HIERARCHY, NULL );
#endif
}

//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//


//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
class CBenchmarkApp : public CDefaultAppSystemGroup< CSteamAppSystemGroup >
{
	typedef CDefaultAppSystemGroup< CSteamAppSystemGroup > BaseClass;

public:
	// Methods of IApplication
	virtual bool Create();
	virtual bool PreInit( );
	virtual int Main();
	virtual void PostShutdown();
	bool SetupSearchPaths();

private:
	bool ParseArguments();
};

DEFINE_CONSOLE_STEAM_APPLICATION_OBJECT( CBenchmarkApp );


//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
bool CBenchmarkApp::Create()
{
	MathLib_Init( 2.2f, 2.2f, 0.0f, 2.0f, false, false, false, false );

	// Add in the cvar factory
	//AppModule_t cvarModule = LoadModule( VStdLib_GetICVarFactory() );
	//AddSystem( cvarModule, VENGINE_CVAR_INTERFACE_VERSION );

	AppSystemInfo_t appSystems[] = 
	{
		{ "vphysics.dll",			VPHYSICS_INTERFACE_VERSION },
		{ "", "" }	// Required to terminate the list
	};

	bool bRet = AddSystems( appSystems );
	if ( bRet )
	{
		physcollision = (IPhysicsCollision*)FindSystem( VPHYSICS_COLLISION_INTERFACE_VERSION );
		if ( !physcollision )
			return false;
	}
	return bRet;
}

bool CBenchmarkApp::SetupSearchPaths()
{
	CFSSteamSetupInfo steamInfo;
	steamInfo.m_pDirectoryName = NULL;
	steamInfo.m_bOnlyUseDirectoryName = false;
	steamInfo.m_bToolsMode = true;
	steamInfo.m_bSetSteamDLLPath = true;
	steamInfo.m_bSteam = g_pFullFileSystem->IsSteam();

	if ( FileSystem_SetupSteamEnvironment( steamInfo ) != FS_OK )
		return false;

	CFSMountContentInfo fsInfo;
	fsInfo.m_pFileSystem = g_pFullFileSystem;
	fsInfo.m_bToolsMode = true;
	fsInfo.m_pDirectoryName = steamInfo.m_GameInfoPath;

	if ( FileSystem_MountContent( fsInfo ) != FS_OK )
		return false;

	// Finally, load the search paths for the "GAME" path.
	CFSSearchPathsInit searchPathsInit;
	searchPathsInit.m_pDirectoryName = steamInfo.m_GameInfoPath;
	searchPathsInit.m_pFileSystem = g_pFullFileSystem;
	if ( FileSystem_LoadSearchPaths( searchPathsInit ) != FS_OK )
		return false;

	g_pFullFileSystem->AddSearchPath( steamInfo.m_GameInfoPath, "SKIN", PATH_ADD_TO_HEAD );

	FileSystem_AddSearchPath_Platform( g_pFullFileSystem, steamInfo.m_GameInfoPath );

	return true;
}

bool CBenchmarkApp::PreInit( )
{
	CreateInterfaceFn factory = GetFactory();
	ConnectTier1Libraries( &factory, 1 );
	ConnectTier2Libraries( &factory, 1 );
	ConnectTier3Libraries( &factory, 1 );

	if ( !g_pFullFileSystem || !physcollision )
	{
		Warning( "benchmark is missing a required interface!\n" );
		return false;
	}

	return SetupSearchPaths();//( NULL, false, true );
}


void CBenchmarkApp::PostShutdown()
{
	DisconnectTier3Libraries();
	DisconnectTier2Libraries();
	DisconnectTier1Libraries();
}

struct baseline_t
{
	float	total;
	float	ray;
	float	box;
};

// current baseline measured on Core2DuoE6600
baseline_t g_Baselines[] =
{
	{ 40.56f,	10.64f,		29.92f},		// bench01a.phy
	{ 38.13f,	10.76f,		27.37f },		// bicycle01a.phy
	{ 25.46f,	8.34f,		17.13f },		// furnituretable001a.phy
	{ 12.65f,	6.02f,		6.62f },		// gravestone003a.phy
	{ 40.58f,	16.49f,		24.10f },		// combineinnerwall001a.phy
};

const float g_TotalBaseline = 157.38f;

/*
Benchmark models\props_c17\bench01a.phy!

33.90 ms [1.20 X] 2500/2500 hits
8.95 ms rays    [1.19 X]        24.95 ms boxes [1.20 X]
Benchmark models\props_junk\bicycle01a.phy!

30.55 ms [1.25 X] 803/2500 hits
8.96 ms rays    [1.20 X]        21.59 ms boxes [1.27 X]
Benchmark models\props_c17\furnituretable001a.phy!

20.61 ms [1.24 X] 755/2500 hits
6.88 ms rays    [1.21 X]        13.73 ms boxes [1.25 X]
Benchmark models\props_c17\gravestone003a.phy!

9.13 ms [1.39 X] 2500/2500 hits
4.34 ms rays    [1.39 X]        4.79 ms boxes [1.38 X]
Benchmark models\props_combine\combineinnerwall001a.phy!

33.04 ms [1.23 X] 985/2500 hits
13.37 ms rays   [1.23 X]        19.67 ms boxes [1.23 X]

127.22s total   [1.24 X]!
*/

#define IMPROVEMENT_FACTOR(x,baseline) (baseline/(x))
#define IMPROVEMENT_PERCENT(x,baseline) (((baseline-(x)) / baseline) * 100.0f)
#include <windows.h>
//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
int CBenchmarkApp::Main()
{
	const char *pFileNames[] = 
	{
		"models\\props_c17\\bench01a.phy",
		"models\\props_junk\\bicycle01a.phy",
		"models\\props_c17\\furnituretable001a.phy",
		"models\\props_c17\\gravestone003a.phy",
		"models\\props_combine\\combineinnerwall001a.phy",
	};
	vcollide_t testModels[ARRAYSIZE(pFileNames)];
	for ( int i = 0; i < ARRAYSIZE(pFileNames); i++ )
	{
		ReadPHYFile( pFileNames[i], testModels[i] );
	}
	SetPriorityClass( GetCurrentProcess(), REALTIME_PRIORITY_CLASS );
	SetThreadPriority( GetCurrentThread(), THREAD_PRIORITY_HIGHEST );
	float totalTime = 0.0f;
	int loopCount = ARRAYSIZE(pFileNames);
#if VPROF_LEVEL > 0
//	loopCount = 3;
#endif
	for ( int i = 0; i < loopCount; i++ )
	{
		if ( testModels[i].solidCount < 1 )
		{
			Msg("Failed to load %s, skipping test!\n", pFileNames[i] );
			continue;
		}
		Msg("Benchmark %s!\n\n", pFileNames[i] );

		benchresults_t results;
		memset( &results, 0, sizeof(results));
		int numRepeats = 3;
#if VPROF_LEVEL > 0
		numRepeats = 1;
#endif
		for ( int j = 0; j < numRepeats; j++ )
		{
			Benchmark_PHY( testModels[i].solids[0], &results );
		}
		Msg("%.2f ms [%.2f X] %d/%d hits\n", results.totalTime, IMPROVEMENT_FACTOR(results.totalTime, g_Baselines[i].total), results.collisionHits, results.collisionTests);
		Msg("%.2f ms rays \t[%.2f X] \t%.2f ms boxes [%.2f X]\n", 
			results.rayTime, IMPROVEMENT_FACTOR(results.rayTime, g_Baselines[i].ray), 
			results.boxTime, IMPROVEMENT_FACTOR(results.boxTime, g_Baselines[i].box));
		totalTime += results.totalTime;
	}
	SetPriorityClass( GetCurrentProcess(), NORMAL_PRIORITY_CLASS );

	Msg("\n%.2fs total \t[%.2f X]!\n", totalTime, IMPROVEMENT_FACTOR(totalTime, g_TotalBaseline) );
	return 0;
}



