//========= Copyright Valve Corporation, All rights reserved. ============//
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// $Header: $
// $NoKeywords: $
//
//=============================================================================

#define PROTECTED_THINGS_DISABLE
#if !defined( _X360 )
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include <time.h>
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/IMaterialSystemHardwareConfig.h"
#include "materialsystem/imaterialproxyfactory.h"
#include "materialsystem/MaterialSystem_Config.h"
#include "appframework/appframework.h"
#include "datacache\idatacache.h"
#include "datacache\imdlcache.h"
#include "vphysics_interface.h"
#include "filesystem.h"
#include "IStudioRender.h"
#include "studio.h"
#include "clientstats.h"
#include "bone_setup.h"
#include "tier0/icommandline.h"
#include "vstdlib/cvar.h"
#include "tier0/vprof.h"
#include "tier1/tier1.h"
#include "optimize.h"
#if defined( _X360 )
#include "xbox\xbox_console.h"
#include "xbox\xbox_win32stubs.h"
#endif

//-----------------------------------------------------------------------------
// Main system interfaces
//-----------------------------------------------------------------------------
IMaterialSystem					*g_pMaterialSystem = NULL;
IStudioRender					*g_pStudioRender = NULL;
IFileSystem						*g_pFileSystem = NULL;
IMDLCache						*g_pMDLCache = NULL;


//-----------------------------------------------------------------------------
// App control defines
//-----------------------------------------------------------------------------
//#define MATERIAL_OVERRIDE
//#define USE_VTUNE
//#define USE_VPROF

#if USE_VTUNE
#include "vtuneapi.h"
#endif

static bool g_WindowMode = false;
static bool g_bUseEmptyShader = false;
static bool g_BenchFinished = false;
static bool g_BenchMode = false;
static bool g_SoftwareTL = false;

static int	g_RenderWidth = 640;
static int	g_RenderHeight = 480;
static int	g_RefreshRate = 60;
static int	g_LOD = 0;
static int	g_BodyGroup = 0;

static int	g_NumRows = 10;
static int	g_NumCols = 10;

static int	g_dxLevel = 0;
static int	g_LightingCombination = -1;

static FILE					*g_IHVTestFP = NULL;
static IMaterial			*g_pForceMaterial = NULL;

static bool g_bInError = false;

#define MAX_LIGHTS					2
#define	NUM_LIGHT_TYPES				4
#define LIGHTING_COMBINATION_COUNT	5

static const char *g_LightCombinationNames[] = 
{
	"DISABLE                ",
//	"SPOT                   ",
	"POINT                  ",
	"DIRECTIONAL            ",
	"SPOT_SPOT              ",
//	"SPOT_POINT             ",
//	"SPOT_DIRECTIONAL       ",
//	"POINT_POINT            ",
	"POINT_DIRECTIONAL      ",
	"DIRECTIONAL_DIRECTIONAL"
};

//-----------------------------------------------------------------------------
//	Test Model class
//-----------------------------------------------------------------------------
struct IHVTestModel
{
	MDLHandle_t		hMdl;
	studiohdr_t		*pStudioHdr;
	studiohwdata_t	*pHardwareData;
};

//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
class CIHVTestApp : public CDefaultAppSystemGroup< CSteamAppSystemGroup >
{
public:
	// Methods of IApplication
	virtual bool Create();
	virtual bool PreInit();
	virtual int Main();
	virtual void PostShutdown();
	virtual void Destroy();

private:
	bool	CreateAppWindow( char const *pTitle, int w, int h );
	void	AppPumpMessages( void );
	void	RenderFrame( void );
	void	RenderScene( void );
	bool	SetupMaterialSystem();
	bool	SetupStudioRender();
	bool	LoadModels( void );
	bool	LoadModel( const char *pModelName, IHVTestModel *pModel );
	bool	CreateMainWindow( int width, int height, bool fullscreen );
	matrix3x4_t*	SetUpBones( studiohdr_t *pStudioHdr, const matrix3x4_t &shapeToWorld, int iRun, int model, int boneMask );

	// Windproc
	static LONG WINAPI WinAppWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	LONG WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	HWND m_hWnd;
	bool m_bExitMainLoop;

	IHVTestModel	*m_pIHVTestModel;
};

static CIHVTestApp s_IHVTestApp;
DEFINE_WINDOWED_STEAM_APPLICATION_OBJECT_GLOBALVAR( CIHVTestApp, s_IHVTestApp );

//-----------------------------------------------------------------------------
// Create the application window
//-----------------------------------------------------------------------------
bool CIHVTestApp::CreateAppWindow( const char* pAppName, int width, int height )
{
    // Register the window class
	WNDCLASSEX		wc;
	memset( &wc, 0, sizeof( wc ) );
	wc.cbSize		 = sizeof( wc );
    wc.style         = CS_CLASSDC;
    wc.lpfnWndProc   = WinAppWindowProc;
    wc.hInstance     = (HINSTANCE)GetAppInstance();
    wc.lpszClassName = pAppName;
	wc.hIcon		 = NULL;
	wc.hIconSm		 = wc.hIcon;

    RegisterClassEx( &wc );

    // Create the application's window
    m_hWnd = CreateWindow( pAppName, pAppName,
		WS_OVERLAPPEDWINDOW, 
		0, 0, width, height,
		GetDesktopWindow(), NULL, wc.hInstance, NULL );
	
    ShowWindow (m_hWnd, SW_SHOWDEFAULT);
	
	return (m_hWnd != 0);
}

//#define TREES

// The maximum number of distinctive models each test may specify.
#ifdef TREES
const int g_nMaxModels = 1;
#else
const int g_nMaxModels = 9;
#endif

//-----------------------------------------------------------------------------
//	Benchmarking
//-----------------------------------------------------------------------------
struct BenchRunInfo
{
	const char *pModelName[g_nMaxModels];
	int numFrames;
	int rows;
	int cols;
	float modelSize;
	int sequence1[g_nMaxModels];
	int sequence2;
};

struct BenchResults
{
	BenchResults() : totalTime( 0.0f ), totalTris( 0 ) {}
	float totalTime;
	int totalTris;
};


#define NUM_BENCH_RUNS 1
static BenchResults g_BenchResults[NUM_BENCH_RUNS][LIGHTING_COMBINATION_COUNT];

#ifdef TREES

#define MODEL_ROWS	13
#define MODEL_COLUMNS 13
static BenchRunInfo g_BenchRuns[NUM_BENCH_RUNS] =
{
	{ { "models/props_foliage/tree_dead01.mdl"
	}, 100, MODEL_ROWS, MODEL_COLUMNS, 1000.0f, { 0 }, -1 },
};

#else

#define MODEL_ROWS	3
#define MODEL_COLUMNS 3
static BenchRunInfo g_BenchRuns[NUM_BENCH_RUNS] =
{
	{ { "models/alyx.mdl", 
		"models/alyx.mdl", 
		"models/alyx.mdl", 
		"models/alyx.mdl", 
		"models/alyx.mdl", 
		"models/alyx.mdl", 
		"models/alyx.mdl", 
		"models/alyx.mdl", 
		"models/alyx.mdl", 
	}, 100, MODEL_ROWS, MODEL_COLUMNS, 75.0f, { 1, 4, 20, 23, 25, 30, 34, 38, 1 }, -1 },
};

#endif

// this is used in "-bench" mode
static IHVTestModel g_BenchModels[NUM_BENCH_RUNS][g_nMaxModels];

static void WriteBenchResults( void )
{
	if( !g_BenchMode )
	{
		return;
	}

	FILE *fp = fopen( "ihvtest1.csv", "a+" );
	Assert( fp );
	if( !fp )
	{
		return;
	}

	fprintf( fp, "------------------------------------------------------------------\n" );
	
	time_t ltime;
	time( &ltime );
	
	fprintf( fp, "%s\n", GetCommandLine() );
	fprintf( fp, "Run at: %s", ctime( &ltime ) );

	int i;
	for( i = 0; i < NUM_BENCH_RUNS; i++ )
	{
		int j;
		fprintf( fp, "model,light combo,total tris,total time,tris/sec\n" );
		for( j = 0; j < LIGHTING_COMBINATION_COUNT; j++ )
		{
			int k;
			for( k = 0; k < g_nMaxModels; k++ )
			{
				if( g_BenchRuns[i].pModelName[k] )
				{
					fprintf( fp, "%s%s", k ? ", " : "", g_BenchRuns[i].pModelName[k] );
				}
			}
			fprintf( fp, "," );
			fprintf( fp, "%s,", g_LightCombinationNames[j] );
			fprintf( fp, "%d,", g_BenchResults[i][j].totalTris );
			fprintf( fp, "%0.5f,", ( float )g_BenchResults[i][j].totalTime );
			fprintf( fp, "%0.0lf\n", ( double )g_BenchResults[i][j].totalTris /
				( double )g_BenchResults[i][j].totalTime );
			Warning( "%f %d\n", ( float )g_BenchResults[i][j].totalTime, g_BenchResults[i][j].totalTris );
		}
	}
	
	fclose( fp );
}


//-----------------------------------------------------------------------------
// Destroy app
//-----------------------------------------------------------------------------
void CIHVTestApp::Destroy()
{
	// Close the window
	if (m_hWnd)
		DestroyWindow( m_hWnd );

	WriteBenchResults();
}


//-----------------------------------------------------------------------------
// Window size helper
//-----------------------------------------------------------------------------
static void CalcWindowSize( int desiredRenderingWidth, int desiredRenderingHeight,
							int *windowWidth, int *windowHeight )
{
    int     borderX, borderY;
	borderX = (GetSystemMetrics(SM_CXFIXEDFRAME) + 1) * 2;
	borderY = (GetSystemMetrics(SM_CYFIXEDFRAME) + 1) * 2 + GetSystemMetrics(SM_CYSIZE) + 1;
	*windowWidth = desiredRenderingWidth + borderX;
	*windowHeight = desiredRenderingHeight + borderY;
}


//-----------------------------------------------------------------------------
// Spew function!
//-----------------------------------------------------------------------------
SpewRetval_t IHVTestSpewFunc( SpewType_t spewType, char const *pMsg )
{
	g_bInError = true;

	OutputDebugString( pMsg );
	switch( spewType )
	{
	case SPEW_MESSAGE:
	case SPEW_WARNING:
	case SPEW_LOG:
		OutputDebugString( pMsg );
		g_bInError = false;
		return SPEW_CONTINUE;

	case SPEW_ASSERT:
	case SPEW_ERROR:
	default:
		::MessageBox( NULL, pMsg, "Error!", MB_OK );
		g_bInError = false;
		return SPEW_DEBUGGER;
	}
}


//-----------------------------------------------------------------------------
// Spew function to write to ihvtest_vprof.txt
//-----------------------------------------------------------------------------
SpewRetval_t IHVTestVProfSpewFunc( SpewType_t spewType, char const *pMsg )
{
	g_bInError = true;

	switch( spewType )
	{
	case SPEW_MESSAGE:
	case SPEW_WARNING:
	case SPEW_LOG:
		fprintf( g_IHVTestFP, "%s", pMsg );
		g_bInError = false;
		return SPEW_CONTINUE;

	case SPEW_ASSERT:
	case SPEW_ERROR:
	default:
		::MessageBox( NULL, pMsg, "Error!", MB_OK );
		g_bInError = false;
		return SPEW_DEBUGGER;
	}
}

//-----------------------------------------------------------------------------
// Warnings and Errors...
//-----------------------------------------------------------------------------
#define	MAXPRINTMSG	4096
void DisplayError( const char* pError, ... )
{
	va_list		argptr;
	char		msg[1024];

	g_bInError = true;

	va_start( argptr, pError );
	Q_vsnprintf( msg, sizeof( msg ), pError, argptr );
	va_end( argptr );

	MessageBox( 0, msg, 0, MB_OK );

	exit( -1 );
}

static void MaterialSystem_Warning( const char *fmt, ... )
{
	va_list		argptr;
	char		msg[MAXPRINTMSG];
	
	va_start( argptr, fmt );
	Q_vsnprintf( msg, sizeof ( msg ), fmt, argptr );
	va_end( argptr );

	OutputDebugString( msg );
}

// garymcthack
static void MaterialSystem_Warning( char *fmt, ... )
{
	va_list		argptr;
	char		msg[MAXPRINTMSG];
	
	va_start( argptr, fmt );
	Q_vsnprintf( msg, sizeof( msg ), fmt, argptr );
	va_end( argptr );

	OutputDebugString( msg );
}

static void MaterialSystem_Error( char *fmt, ... )
{
	va_list		argptr;
	char		msg[MAXPRINTMSG];

	g_bInError = true;

	va_start( argptr, fmt );
	Q_vsnprintf( msg, sizeof( msg ), fmt, argptr );
	va_end( argptr );

	MessageBox( NULL, (LPCTSTR)msg, "MaterialSystem Fatal Error", MB_OK | MB_ICONINFORMATION );

#ifdef _DEBUG
	Assert( 0 );	
#endif
	exit( -1 );
}


//-----------------------------------------------------------------------------
// Engine Stats
//-----------------------------------------------------------------------------
// itty bitty interface for stat time
class CStatTime : public IClientStatsTime
{
public:
	float GetTime()
	{
		return Sys_FloatTime();
	}
};
CStatTime	g_StatTime;

class CEngineStats
{
public:
	CEngineStats() : m_InFrame( false ) {};
	//
	// stats input
	//

	void BeginRun( void );
	void BeginFrame( void );

	void EndFrame( void );
	void EndRun( void );

	//
	// stats output
	// call these outside of a BeginFrame/EndFrame pair
	//

	// return the frame time in seconds for the whole system (not just graphics)
	double GetCurrentSystemFrameTime( void );
	double GetRunTime( void );
private:
	// How many frames worth of data have we logged?
	int m_totalNumFrames;

	// frame timing data
	double m_frameStartTime;
	double m_frameEndTime;
	double m_minFrameTime;
	double m_maxFrameTime;

	// run timing data
	double m_runStartTime;
	double m_runEndTime;

	bool m_InFrame;
};

void CEngineStats::BeginRun( void )
{
	m_totalNumFrames = 0;
	// frame timing data
	m_runStartTime = Sys_FloatTime();
}

void CEngineStats::EndRun( void )
{
	m_runEndTime = Sys_FloatTime();
}

void CEngineStats::BeginFrame( void )
{
	m_InFrame = true;
	m_frameStartTime = Sys_FloatTime();
}

void CEngineStats::EndFrame( void )
{
	double deltaTime;
	
	m_frameEndTime = Sys_FloatTime();
	deltaTime = GetCurrentSystemFrameTime();

	m_InFrame = false;
}

double CEngineStats::GetRunTime( void )
{
	return m_runEndTime - m_runStartTime;
}

double CEngineStats::GetCurrentSystemFrameTime( void )
{
	return m_frameEndTime - m_frameStartTime;
}
static CEngineStats g_EngineStats;


//-----------------------------------------------------------------------------
//	Lighting
//-----------------------------------------------------------------------------
// If you change the number of lighting combinations, change LIGHTING_COMBINATION_COUNT
static LightType_t g_LightCombinations[][MAX_LIGHTS] = 
{
	{ MATERIAL_LIGHT_DISABLE,		MATERIAL_LIGHT_DISABLE },			// 0
//	{ MATERIAL_LIGHT_SPOT,			MATERIAL_LIGHT_DISABLE },
//	{ MATERIAL_LIGHT_POINT,			MATERIAL_LIGHT_DISABLE },
	{ MATERIAL_LIGHT_DIRECTIONAL,	MATERIAL_LIGHT_DISABLE },
	{ MATERIAL_LIGHT_SPOT,			MATERIAL_LIGHT_SPOT },

//	{ MATERIAL_LIGHT_SPOT,			MATERIAL_LIGHT_POINT },				// 5
//	{ MATERIAL_LIGHT_SPOT,			MATERIAL_LIGHT_DIRECTIONAL },
//	{ MATERIAL_LIGHT_POINT,			MATERIAL_LIGHT_POINT },
	{ MATERIAL_LIGHT_POINT,			MATERIAL_LIGHT_DIRECTIONAL },
	{ MATERIAL_LIGHT_DIRECTIONAL,	MATERIAL_LIGHT_DIRECTIONAL },		// 9
};

LightDesc_t g_TestLights[NUM_LIGHT_TYPES][MAX_LIGHTS];

static void FixLight( LightDesc_t *pLight )
{
	pLight->m_Range = 0.0f;
	pLight->m_Falloff = 1.0f;
	pLight->m_ThetaDot = cos( pLight->m_Theta * 0.5f );
	pLight->m_PhiDot = cos( pLight->m_Phi * 0.5f );
	pLight->m_Flags = 0;
	if( pLight->m_Attenuation0 != 0.0f )
	{
		pLight->m_Flags |= LIGHTTYPE_OPTIMIZATIONFLAGS_HAS_ATTENUATION0;
	}
	if( pLight->m_Attenuation1 != 0.0f )
	{
		pLight->m_Flags |= LIGHTTYPE_OPTIMIZATIONFLAGS_HAS_ATTENUATION1;
	}
	if( pLight->m_Attenuation2 != 0.0f )
	{
		pLight->m_Flags |= LIGHTTYPE_OPTIMIZATIONFLAGS_HAS_ATTENUATION2;
	}
	VectorNormalize( pLight->m_Direction );
}


static void InitTestLights( void )
{
	LightDesc_t *pLight;
	int i;
	for( i = 0; i < MAX_LIGHTS; i++ )
	{
		// MATERIAL_LIGHT_DISABLE		
		pLight = &g_TestLights[MATERIAL_LIGHT_DISABLE][i];
		pLight->m_Type = MATERIAL_LIGHT_DISABLE;
	}

	// x - right
	// y - up
	// z - front of model
	// MATERIAL_LIGHT_SPOT 0
	pLight = &g_TestLights[MATERIAL_LIGHT_SPOT][0];
	memset( pLight, 0, sizeof( LightDesc_t ) );
	pLight->m_Type = MATERIAL_LIGHT_SPOT;
	pLight->m_Color.Init( 5000.0f, 3500.0f, 3500.0f );
	pLight->m_Position.Init( 0.0f, 0.0f, 50.0f );
	pLight->m_Direction.Init( 0.0f, 0.5f, -1.0f );
	pLight->m_Attenuation0 = 0.0f;
	pLight->m_Attenuation1 = 0.0f;
	pLight->m_Attenuation2 = 1.0f / 10;
	pLight->m_Theta = DEG2RAD( 20.0f );
	pLight->m_Phi = DEG2RAD( 30.0f );

	// MATERIAL_LIGHT_SPOT 1
	pLight = &g_TestLights[MATERIAL_LIGHT_SPOT][1];
	memset( pLight, 0, sizeof( LightDesc_t ) );
	pLight->m_Type = MATERIAL_LIGHT_SPOT;
	pLight->m_Color.Init( 3500.0f, 5000.0f, 3500.0f );
	pLight->m_Position.Init( 0.0f, 0.0f, 150.0f );
	pLight->m_Direction.Init( 0.0f, 0.5f, -1.0f );
	pLight->m_Attenuation0 = 0.0f;
	pLight->m_Attenuation1 = 0.0f;
	pLight->m_Attenuation2 = 1.0f / 10;
	pLight->m_Theta = DEG2RAD( 20.0f );
	pLight->m_Phi = DEG2RAD( 30.0f );

	// MATERIAL_LIGHT_POINT 0
	pLight = &g_TestLights[MATERIAL_LIGHT_POINT][0];
	memset( pLight, 0, sizeof( LightDesc_t ) );
	pLight->m_Type = MATERIAL_LIGHT_POINT;
	pLight->m_Color.Init( 1500.0f, 750.0f, 750.0f );
	pLight->m_Position.Init( 200.0f, 200.0f, 200.0f );
	pLight->m_Attenuation0 = 0.0f;
	pLight->m_Attenuation1 = 1.0f;
	pLight->m_Attenuation2 = 0.0f;

	// MATERIAL_LIGHT_POINT 1
	pLight = &g_TestLights[MATERIAL_LIGHT_POINT][1];
	memset( pLight, 0, sizeof( LightDesc_t ) );
	pLight->m_Type = MATERIAL_LIGHT_POINT;
	pLight->m_Color.Init( 750.0f, 750.0f, 1500.0f );
	pLight->m_Position.Init( -200.0f, 200.0f, 200.0f );
	pLight->m_Attenuation0 = 0.0f;
	pLight->m_Attenuation1 = 1.0f;
	pLight->m_Attenuation2 = 0.0f;

	// MATERIAL_LIGHT_DIRECTIONAL 0
	pLight = &g_TestLights[MATERIAL_LIGHT_DIRECTIONAL][0];
	memset( pLight, 0, sizeof( LightDesc_t ) );
	pLight->m_Type = MATERIAL_LIGHT_DIRECTIONAL;
	pLight->m_Color.Init( 2.0f, 2.0f, 1.0f );
	pLight->m_Direction.Init( -1.0f, 0.0f, 0.0f );
	pLight->m_Attenuation0 = 1.0f;
	pLight->m_Attenuation1 = 0.0f;
	pLight->m_Attenuation2 = 0.0f;

	// MATERIAL_LIGHT_DIRECTIONAL 1
	pLight = &g_TestLights[MATERIAL_LIGHT_DIRECTIONAL][1];
	memset( pLight, 0, sizeof( LightDesc_t ) );
	pLight->m_Type = MATERIAL_LIGHT_DIRECTIONAL;
	pLight->m_Color.Init( 1.0f, 1.0f, 2.0f );
	pLight->m_Direction.Init( 1.0f, 0.0f, 0.0f );
	pLight->m_Attenuation0 = 1.0f;
	pLight->m_Attenuation1 = 0.0f;
	pLight->m_Attenuation2 = 0.0f;

	for( i = 0; i < MAX_LIGHTS; i++ )
	{
		int j;
		for( j = 0; j < NUM_LIGHT_TYPES; j++ )
		{
			FixLight( &g_TestLights[j][i] );
		}
	}
}


//-----------------------------------------------------------------------------
// Setup lighting
//-----------------------------------------------------------------------------
static void SetupLighting( int lightingCombination, Vector &lightOffset )
{
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	if( lightingCombination == 0 )
	{
		g_pStudioRender->SetLocalLights( 0, NULL );
		pRenderContext->SetAmbientLight( 1.0, 1.0, 1.0 );

		static Vector white[6] = 
		{
			Vector( 1.0, 1.0, 1.0 ),
			Vector( 1.0, 1.0, 1.0 ),
			Vector( 1.0, 1.0, 1.0 ),
			Vector( 1.0, 1.0, 1.0 ),
			Vector( 1.0, 1.0, 1.0 ),
			Vector( 1.0, 1.0, 1.0 ),
		};
		g_pStudioRender->SetAmbientLightColors( white );
	}
	else
	{
		pRenderContext->SetAmbientLight( 0.0f, 0.0f, 0.0f );

		static Vector black[6] = 
		{
			Vector( 0.0, 0.0, 0.0 ),
			Vector( 0.0, 0.0, 0.0 ),
			Vector( 0.0, 0.0, 0.0 ),
			Vector( 0.0, 0.0, 0.0 ),
			Vector( 0.0, 0.0, 0.0 ),
			Vector( 0.0, 0.0, 0.0 ),
		};
		g_pStudioRender->SetAmbientLightColors( black );

		int lightID;
		LightDesc_t lightDescs[MAX_LIGHTS];
		for( lightID = 0; lightID < MAX_LIGHTS; lightID++ )
		{
			int lightType = g_LightCombinations[lightingCombination][lightID];
			lightDescs[lightID] = g_TestLights[lightType][lightID];
			lightDescs[lightID].m_Position += lightOffset;
		}
			    
		// Feed disabled lights through?
		if( g_LightCombinations[lightingCombination][1] == MATERIAL_LIGHT_DISABLE )
		{
			g_pStudioRender->SetLocalLights( 1, lightDescs );
		}
		else
		{
			g_pStudioRender->SetLocalLights( MAX_LIGHTS, lightDescs );
		}
	}
}


//-----------------------------------------------------------------------------
// Models
//-----------------------------------------------------------------------------
static float s_PoseParameter[32];
static float s_Cycle[9] = { 0.0f };

virtualmodel_t *studiohdr_t::GetVirtualModel( void ) const
{
	if ( numincludemodels == 0 )
		return NULL;
	return g_pMDLCache->GetVirtualModelFast( this, (MDLHandle_t)virtualModel );
}

byte *studiohdr_t::GetAnimBlock( int i ) const
{
	return g_pMDLCache->GetAnimBlock( (MDLHandle_t)virtualModel, i );
}

int	studiohdr_t::GetAutoplayList( unsigned short **pOut ) const
{
	return g_pMDLCache->GetAutoplayList( (MDLHandle_t)virtualModel, pOut );
}

const studiohdr_t *virtualgroup_t::GetStudioHdr( void ) const
{
	return g_pMDLCache->GetStudioHdr( (MDLHandle_t)cache );
}


//-----------------------------------------------------------------------------
// Set up the bones for a frame
//-----------------------------------------------------------------------------
matrix3x4_t* CIHVTestApp::SetUpBones( studiohdr_t *pStudioHdr, const matrix3x4_t &shapeToWorld, int iRun, int model, int boneMask )
{
	CStudioHdr studioHdr( pStudioHdr, g_pMDLCache );

	// Default to middle of the pose parameter range
	float flPoseParameter[MAXSTUDIOPOSEPARAM];
	Studio_CalcDefaultPoseParameters( &studioHdr, flPoseParameter, MAXSTUDIOPOSEPARAM );

	int nFrameCount = Studio_MaxFrame( &studioHdr, g_BenchRuns[iRun].sequence1[model], flPoseParameter );
	if ( nFrameCount == 0 )
	{
		nFrameCount = 1;
	}

	Vector		pos[MAXSTUDIOBONES];
	Quaternion	q[MAXSTUDIOBONES];

	IBoneSetup boneSetup( &studioHdr, boneMask, flPoseParameter );
	boneSetup.InitPose( pos, q );
	boneSetup.AccumulatePose( pos, q, g_BenchRuns[iRun].sequence1[model], s_Cycle[model], 1.0f, 0.0, NULL );

	// FIXME: Try enabling this?
//	CalcAutoplaySequences( pStudioHdr, NULL, pos, q, flPoseParameter, BoneMask( ), flTime );

	// Root transform
	matrix3x4_t rootToWorld, temp;

	MatrixCopy( shapeToWorld, rootToWorld );

	matrix3x4_t *pBoneToWorld = g_pStudioRender->LockBoneMatrices( studioHdr.numbones() );
	for ( int i = 0; i < studioHdr.numbones(); i++ ) 
	{
		// If it's not being used, fill with NAN for errors
		if ( !(studioHdr.pBone( i )->flags & boneMask) )
		{
			int j, k;
			for (j = 0; j < 3; j++)
			{
				for (k = 0; k < 4; k++)
				{
					pBoneToWorld[i][j][k] = VEC_T_NAN;
				}
			}
			continue;
		}

		matrix3x4_t boneMatrix;
		QuaternionMatrix( q[i], boneMatrix );
		MatrixSetColumn( pos[i], 3, boneMatrix );

		if (studioHdr.pBone(i)->parent == -1) 
		{
			ConcatTransforms (rootToWorld, boneMatrix, pBoneToWorld[i]);
		} 
		else 
		{
			ConcatTransforms (pBoneToWorld[ studioHdr.pBone(i)->parent ], boneMatrix, pBoneToWorld[i] );
		}
	}
	g_pStudioRender->UnlockBoneMatrices();
	return pBoneToWorld;
}


//-----------------------------------------------------------------------------
// Use mdlcache to load a model
//-----------------------------------------------------------------------------
bool CIHVTestApp::LoadModel( const char* pModelName, IHVTestModel *pModel )
{
	pModel->hMdl = g_pMDLCache->FindMDL( pModelName );

	pModel->pStudioHdr = g_pMDLCache->GetStudioHdr( pModel->hMdl );
	
	g_pMDLCache->GetVertexData( pModel->hMdl );
	g_pMDLCache->FinishPendingLoads();

	g_pMDLCache->GetHardwareData( pModel->hMdl );
	g_pMDLCache->FinishPendingLoads();

	pModel->pHardwareData = g_pMDLCache->GetHardwareData( pModel->hMdl );

	return true;
}


//-----------------------------------------------------------------------------
// Loads all models 
//-----------------------------------------------------------------------------
bool CIHVTestApp::LoadModels( void )
{
	const char *pArgVal;
	if( CommandLine()->CheckParm( "-rowcol", &pArgVal ) )
	{
		g_NumRows = g_NumCols = atoi( pArgVal );
	}
	
	/* figure out which LOD we are going to render */
	if( CommandLine()->CheckParm( "-lod", &pArgVal ) )
	{
		g_LOD = atoi( pArgVal );
	}

	if( CommandLine()->CheckParm( "-body", &pArgVal ) )
	{
		g_BodyGroup = atoi( pArgVal );
	}

	// figure out g_RefreshRate
	if( CommandLine()->CheckParm( "-refresh", &pArgVal ) )
	{
		g_RefreshRate = atoi( pArgVal );
	}
	
	if( CommandLine()->CheckParm( "-light", &pArgVal ) )
	{
		g_LightingCombination = atoi( pArgVal );
		if( g_LightingCombination < 0 )
		{
			g_LightingCombination = 0;
		}
		if( g_LightingCombination >= LIGHTING_COMBINATION_COUNT )
		{
			g_LightingCombination = LIGHTING_COMBINATION_COUNT - 1;
		}
	}

	g_pForceMaterial = g_pMaterialSystem->FindMaterial( "models/alyx/thigh", TEXTURE_GROUP_OTHER );
#ifdef MATERIAL_OVERRIDE
	g_pStudioRender->ForcedMaterialOverride( g_pForceMaterial );
#endif

	InitTestLights();

	if( g_BenchMode )
	{
		int i;
		for( i = 0; i < NUM_BENCH_RUNS; i++ )
		{
			// Load each of the potentially alternating models:
			int k;
			for( k = 0; k < g_nMaxModels; k++ )
			{
				if( g_BenchRuns[i].pModelName[k] ) 
				{
					if( !LoadModel( g_BenchRuns[i].pModelName[k], &g_BenchModels[i][k] ) )
					{
						return false;
					}
				}
			}
		}
	}
	else
	{
		CommandLine()->CheckParm( "-i", &pArgVal );
		if( !LoadModel( pArgVal, m_pIHVTestModel ) )
		{
			return false;
		}
	}
	g_pMaterialSystem->CacheUsedMaterials();

	return true;
}


//-----------------------------------------------------------------------------
// App window proc
//-----------------------------------------------------------------------------
LONG CIHVTestApp::WindowProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
    switch( msg )
    {
		// abort when ESC is hit
	case WM_CHAR:
		switch(wParam)
		{
		case VK_ESCAPE:
			SendMessage( m_hWnd, WM_CLOSE, 0, 0 );
			break;
		}
		break;

	case WM_DESTROY:
			m_bExitMainLoop = true;
            return 0;
    }
	
    return DefWindowProc( hWnd, msg, wParam, lParam );
}


//-----------------------------------------------------------------------------
// Static registered window proc
//-----------------------------------------------------------------------------
LONG WINAPI CIHVTestApp::WinAppWindowProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	return s_IHVTestApp.WindowProc( hWnd, uMsg, wParam, lParam );
}


//-----------------------------------------------------------------------------
// Pump messages
//-----------------------------------------------------------------------------
void CIHVTestApp::AppPumpMessages()
{
	MSG msg;
	while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) == TRUE) 
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}


//-----------------------------------------------------------------------------
// Advance the frame
//-----------------------------------------------------------------------------
void AdvanceFrame( CStudioHdr *pStudioHdr, int iRun, int model, float dt )
{
	if (dt > 0.1)
		dt = 0.1f;

	float t = Studio_Duration( pStudioHdr, g_BenchRuns[iRun].sequence1[model], s_PoseParameter );

	if (t > 0)
	{
		s_Cycle[model] += dt / t;

		// wrap
		s_Cycle[model] -= (int)(s_Cycle[model]);
	}
	else
	{
		s_Cycle[model] = 0;
	}
}


//-----------------------------------------------------------------------------
// Render a frame
//-----------------------------------------------------------------------------
void CIHVTestApp::RenderFrame( void )
{
	VPROF( "RenderFrame" );
	IHVTestModel *pModel = NULL;
	static int currentRun = 0;
	static int currentFrame = 0;
	static int currentLightCombo = 0;
	int modelAlternator = 0;

	if (g_bInError)
	{
		// error context is active
		// error may be renderer based, avoid re-entrant render to fatal crash
		return;
	}

	if( g_BenchMode )
	{
		if( currentFrame > g_BenchRuns[currentRun].numFrames )
		{
			currentLightCombo++;
			if( currentLightCombo >= LIGHTING_COMBINATION_COUNT )
			{
				currentRun++;
				currentLightCombo = 0;
				if( currentRun >= NUM_BENCH_RUNS )
				{
					g_BenchFinished = true;
					return;
				}
			}
			currentFrame = 0;
		}
	}
	if( g_BenchMode )
	{
		pModel = &g_BenchModels[currentRun][0];		
		g_NumCols = g_BenchRuns[currentRun].cols;
		g_NumRows = g_BenchRuns[currentRun].rows;
	}
	else
	{
		pModel = m_pIHVTestModel;
	}
	Assert( pModel );
	
	g_EngineStats.BeginFrame();

	g_pMaterialSystem->BeginFrame( 0 );
	g_pStudioRender->BeginFrame();

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );

	pRenderContext->ClearColor3ub( 0, 0, 0 );
	pRenderContext->ClearBuffers( true, true );

	pRenderContext->Viewport( 0, 0, g_RenderWidth, g_RenderHeight );

	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->LoadIdentity();
	pRenderContext->PerspectiveX( 90.0f, ( g_RenderWidth / g_RenderHeight), 1.0f, 500000.0f );

	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->LoadIdentity();
	if( g_BenchMode )
	{
		pRenderContext->Translate( 0.0f, 0.0f, ( float )-( g_NumCols  * g_BenchRuns[currentRun].modelSize * 0.6f ) );
	}
	else
	{
		pRenderContext->Translate( 0.0f, 0.0f, ( float )-( g_NumCols  * 80.0f * 0.5f ) );
	}

	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->LoadIdentity();

	QAngle angles;
	angles[YAW] = -90.0f;
	angles[PITCH] = -90.0f;
	angles[ROLL] = 0.0f;

	matrix3x4_t cameraMatrix;
	AngleMatrix( angles, cameraMatrix );

	int r, c;
	int trisRendered = 0;
	float boneSetupTime = 0.0f;
	for( r = 0; r < g_NumRows; r++ )
	{
		for( c = 0; c < g_NumCols; c++ )
		{
			// If we are alternating models, select the next valid model.			
			if( g_BenchMode )
			{
				do
				{
					// If I pass my maximum number of models, wrap around to model 0, which must always be valid.
					if( ++modelAlternator >= g_nMaxModels )
					{
						modelAlternator = 0;
						break;
					}
				}
				while( !g_BenchRuns[currentRun].pModelName[modelAlternator] );

				pModel = &g_BenchModels[currentRun][modelAlternator];
				Assert( pModel );
			}

			if( g_BenchMode )
			{
				cameraMatrix[0][3] = ( ( c + 0.5f ) - ( g_NumCols * .5f ) ) * g_BenchRuns[currentRun].modelSize;
				cameraMatrix[1][3] = ( ( float )r - ( g_NumCols * .5f ) ) * g_BenchRuns[currentRun].modelSize;
			}
			else
			{
				cameraMatrix[0][3] = ( ( c + 0.5f ) - ( g_NumCols * .5f ) ) * 75.0f;
				cameraMatrix[1][3] = ( ( float )r - ( g_NumCols * .5f ) ) * 75.0f;
			}
			
			Vector modelOrigin( cameraMatrix[0][3], cameraMatrix[1][3], 0.0f );
			Vector lightOffset( cameraMatrix[0][3], cameraMatrix[1][3], 0.0f );

			if (g_LightingCombination < 0)
			{
				SetupLighting( g_BenchMode ? currentLightCombo : 0, lightOffset );
			}
			else
			{
				SetupLighting( g_LightingCombination, lightOffset );
			}

			float startBoneSetupTime = Sys_FloatTime();
			int lod = g_LOD;
			lod = clamp( lod, pModel->pHardwareData->m_RootLOD, pModel->pHardwareData->m_NumLODs-1 );

			int boneMask = BONE_USED_BY_VERTEX_AT_LOD( lod );
			matrix3x4_t *pBoneToWorld = SetUpBones( pModel->pStudioHdr, cameraMatrix, currentRun, modelAlternator, boneMask );
			boneSetupTime += Sys_FloatTime() - startBoneSetupTime;
			
			pRenderContext->MatrixMode( MATERIAL_MODEL );
			pRenderContext->PushMatrix();

			DrawModelInfo_t modelInfo;
			memset( &modelInfo, 0, sizeof( modelInfo ) );
			modelInfo.m_pStudioHdr = pModel->pStudioHdr;
			modelInfo.m_pHardwareData = pModel->pHardwareData;
			modelInfo.m_Decals = STUDIORENDER_DECAL_INVALID;
			modelInfo.m_Skin = 0;
			modelInfo.m_Body = g_BodyGroup;
			modelInfo.m_HitboxSet = 0;
			modelInfo.m_pClientEntity = NULL;
			modelInfo.m_Lod = lod;
			modelInfo.m_pColorMeshes = NULL;
			g_pStudioRender->DrawModel( NULL, modelInfo, pBoneToWorld, NULL, NULL, modelOrigin );

			DrawModelResults_t results;
			g_pStudioRender->GetPerfStats( &results, modelInfo, NULL );
			trisRendered += results.m_ActualTriCount;

			pRenderContext->MatrixMode( MATERIAL_MODEL );
			pRenderContext->PopMatrix();
		}
	}

	pRenderContext->Flush( true );
	g_EngineStats.EndFrame();

	g_pStudioRender->EndFrame();
	g_pMaterialSystem->EndFrame();

	// hack - don't count the first frame in case there are any state
	// transitions computed on that frame.
	if( currentFrame != 0 )
	{
		g_BenchResults[currentRun][currentLightCombo].totalTime += g_EngineStats.GetCurrentSystemFrameTime();
		g_BenchResults[currentRun][currentLightCombo].totalTris += trisRendered;
	}

	for ( int model = 0; model < g_nMaxModels; ++model )
	{
		CStudioHdr studioHdr( g_BenchModels[currentRun][model].pStudioHdr, g_pMDLCache );
		AdvanceFrame( &studioHdr, currentRun, model, g_EngineStats.GetCurrentSystemFrameTime() );
	}

	g_pMaterialSystem->SwapBuffers();

#ifdef USE_VPROF
	g_VProfCurrentProfile.MarkFrame();
	static bool bBeenHere = false;
	if( !bBeenHere )
	{
		bBeenHere = true;
		g_VProfCurrentProfile.Reset();
	}
#endif
	currentFrame++;
}


//-----------------------------------------------------------------------------
// Create the application object
//-----------------------------------------------------------------------------
bool CIHVTestApp::Create()
{
	AppSystemInfo_t appSystems[] = 
	{
		{ "materialsystem.dll",		MATERIAL_SYSTEM_INTERFACE_VERSION },	
		{ "datacache.dll",			DATACACHE_INTERFACE_VERSION },
		{ "studiorender.dll",		STUDIO_RENDER_INTERFACE_VERSION },	
		{ "datacache.dll",			MDLCACHE_INTERFACE_VERSION },
		{ "vphysics.dll",			VPHYSICS_INTERFACE_VERSION },
		{ "", "" }	// Required to terminate the list
	};

	MathLib_Init( 2.2f, 2.2f, 0.0f, 2.0f );

	if ( !AddSystems( appSystems ) ) 
		return false;

	g_pFileSystem = ( IFileSystem * )FindSystem( FILESYSTEM_INTERFACE_VERSION );
	g_pMaterialSystem = (IMaterialSystem*)FindSystem( MATERIAL_SYSTEM_INTERFACE_VERSION );
	g_pStudioRender = (IStudioRender*)FindSystem( STUDIO_RENDER_INTERFACE_VERSION );
	g_pMDLCache = (IMDLCache*)FindSystem( MDLCACHE_INTERFACE_VERSION );

	if ( !g_pFileSystem || !g_pMaterialSystem || !g_pStudioRender || !g_pMDLCache )
	{
		DisplayError( "Unable to load required library interfaces!" );
		return false;
	}

#if defined( _X360 )
	// vxconsole - true will block (legacy behavior)
	XBX_InitConsoleMonitor( false );
#endif

	const char* pDLLName;
	if ( CommandLine()->CheckParm( "-null" ) )
	{
		g_bUseEmptyShader = true;
		pDLLName = "shaderapiempty.dll";
	}
	else
	{
		pDLLName = "shaderapidx9.dll";
	}

#if defined( _X360 )
	g_pFileSystem->LoadModule( pDLLName );
#endif
	g_pMaterialSystem->SetShaderAPI( pDLLName );

	return true;
}


//-----------------------------------------------------------------------------
// StudioRender...
//-----------------------------------------------------------------------------
bool CIHVTestApp::SetupStudioRender( void )
{	
	StudioRenderConfig_t config;
	memset( &config, 0, sizeof(config) );

	config.bEyeMove = true;
	config.bTeeth = true;
	config.bEyes = true;
	config.bFlex = true;

	config.fEyeShiftX = 0.0f;
	config.fEyeShiftY = 0.0f;
	config.fEyeShiftZ = 0.0f;
	config.fEyeSize = 0.0f;	

	config.bNoHardware = false;
	config.bNoSoftware = false;

	config.bSoftwareSkin = false;
	config.bSoftwareLighting = false;
	
	config.drawEntities = true;
	config.bWireframe = false;
	config.bDrawNormals = false;
	config.bDrawTangentFrame = false;
	config.skin = 0;
	
	config.fullbright = 0;

	config.bShowEnvCubemapOnly = false;
	
	g_pStudioRender->UpdateConfig( config );

	return true;
}


//-----------------------------------------------------------------------------
// Material system
//-----------------------------------------------------------------------------
bool InitMaterialSystem( HWND mainWindow )
{
	MaterialSystem_Config_t config;
	if( g_WindowMode )
	{
		config.SetFlag( MATSYS_VIDCFG_FLAGS_WINDOWED, true );
	}
	config.SetFlag( MATSYS_VIDCFG_FLAGS_NO_WAIT_FOR_VSYNC, IsX360() ? 0 : true );

	config.m_VideoMode.m_Width = 0;
	config.m_VideoMode.m_Height = 0;
	config.m_VideoMode.m_Format = IMAGE_FORMAT_BGRX8888;
	config.m_VideoMode.m_RefreshRate = g_RefreshRate;
	config.dxSupportLevel = IsX360() ? 98 : 0;

	bool modeSet = g_pMaterialSystem->SetMode( (void*)mainWindow, config );
	if (!modeSet)
	{
		DisplayError( "Unable to set mode\n" );
		return false;
	}

	g_pMaterialSystem->OverrideConfig( config, false );

	return true;
}


//-----------------------------------------------------------------------------
// PreInit
//-----------------------------------------------------------------------------
bool CIHVTestApp::PreInit( void )
{
	CreateInterfaceFn factory = GetFactory();
	ConnectTier1Libraries( &factory, 1 );

	// Add paths...
	if ( !SetupSearchPaths( NULL, false, true ) )
	{
		Error( "Failed to setup search paths\n" );
		return false;
	}

	const char *pArgVal;
	if ( CommandLine()->CheckParm( "-bench" ) )
	{
		g_BenchMode = true;
	}
	
	if( !g_BenchMode && !CommandLine()->CheckParm( "-i" ) )
	{
		// Set some default parameters for running as a unittest
		g_BenchMode = true;
		g_WindowMode = IsPC() ? true : false;
	}

	if( g_BenchMode )
	{
		if ( CommandLine()->CheckParm( "-i", &pArgVal ) )
		{
			g_BenchRuns[0].pModelName[0] = pArgVal;
		}
	}
	
	if( CommandLine()->CheckParm( "-softwaretl" ) )
	{
		g_SoftwareTL = true;
	}

	// Explicitly in window/fullscreen mode?
	if ( CommandLine()->CheckParm( "-window") )
	{
		g_WindowMode = true;
	}
	else if ( CommandLine()->CheckParm( "-fullscreen" ) )
	{
		g_WindowMode = false;
	}

	/* figure out g_Renderwidth and g_RenderHeight */
	g_RenderWidth = -1;
	g_RenderHeight = -1;

	if( CommandLine()->CheckParm( "-width", &pArgVal ) )
	{
		g_RenderWidth = atoi( pArgVal );
	}
	if( CommandLine()->CheckParm( "-height", &pArgVal ) )
	{
		g_RenderHeight = atoi( pArgVal );
	}

	if( g_RenderWidth == -1 && g_RenderHeight == -1 )
	{
		g_RenderWidth = 640;
		g_RenderHeight = 480;
	}
	else if( g_RenderWidth != -1 && g_RenderHeight == -1 )
	{
		switch( g_RenderWidth )
		{
		case 320:
			g_RenderHeight = 240;
			break;
		case 512:
			g_RenderHeight = 384;
			break;
		case 640:
			g_RenderHeight = 480;
			break;
		case 800:
			g_RenderHeight = 600;
			break;
		case 1024:
			g_RenderHeight = 768;
			break;
		case 1280:
			g_RenderHeight = 1024;
			break;
		case 1600:
			g_RenderHeight = 1200;
			break;
		default:
			DisplayError( "Can't figure out window dimensions!!" );
			exit( -1 );
			break;
		}
	}

	if( g_RenderWidth == -1 || g_RenderHeight == -1 )
	{
		DisplayError( "Can't figure out window dimensions!!" );
		exit( -1 );
	}
	
	int windowWidth, windowHeight;
	CalcWindowSize( g_RenderWidth, g_RenderHeight, &windowWidth, &windowHeight );

	if( !CreateAppWindow( "ihvtest1", windowWidth, windowHeight ) )
	{
		return false;
	}
	return true;
}

void CIHVTestApp::PostShutdown()
{
	DisconnectTier1Libraries();
}


//-----------------------------------------------------------------------------
// The application main loop
//-----------------------------------------------------------------------------
int CIHVTestApp::Main()
{
	SpewOutputFunc( IHVTestSpewFunc );
		    
	if ( !SetupStudioRender() )
	{
		return 0;
	}

	if ( !InitMaterialSystem( m_hWnd ) )
	{
		return 0;
	}

#if !defined( _X360 ) // X360TBD:
extern void Sys_InitFloatTime( void ); //garymcthack
	Sys_InitFloatTime();
#endif

	LoadModels();

#if USE_VTUNE
	VTResume();
#endif
#ifdef USE_VPROF
	g_VProfCurrentProfile.Start();
#endif

	bool m_bExitMainLoop = false;
	while (!m_bExitMainLoop && !g_BenchFinished)
	{
		AppPumpMessages();
		RenderFrame();
	}

#ifdef USE_VPROF
	g_VProfCurrentProfile.Stop();
#endif
	g_IHVTestFP = fopen( "ihvtest_vprof.txt", "w" );
#ifdef USE_VPROF
	SpewOutputFunc( IHVTestVProfSpewFunc );
	g_VProfCurrentProfile.OutputReport( VPRT_SUMMARY );
	g_VProfCurrentProfile.OutputReport( VPRT_HIERARCHY_TIME_PER_FRAME_AND_COUNT_ONLY );
	fclose( g_IHVTestFP );
	SpewOutputFunc( IHVTestSpewFunc );
#endif
#if USE_VTUNE
	VTPause();
#endif

    return 0;
}

