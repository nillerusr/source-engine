//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#define PROTECTED_THINGS_DISABLE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/IMaterialSystemHardwareConfig.h"
#include "materialsystem/imaterialproxyfactory.h"
#include "filesystem.h"
#include "bitmap/imageformat.h"
#include "materialsystem/MaterialSystem_Config.h"
#include "tier0/icommandline.h"
#include "tier1/strtools.h"
#include "mathlib/mathlib.h"
#include "materialsystem/imesh.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialvar.h"
#include "bitmap/tgawriter.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <direct.h>
#include "vstdlib/cvar.h"
#include "KeyValues.h"
#include "tier1/utlbuffer.h"
#include "tier2/tier2.h"

#define CHECKERBOARD_DIMS 16

IMaterial *g_pProceduralMaterial = NULL;

static char g_pCommandLine[1024];
static int g_ArgC = 0;
static char* g_ppArgV[32];
static HWND g_HWnd = 0;
static CreateInterfaceFn g_MaterialsFactory;

static CSysModule			*g_MaterialsDLL = NULL;

static int g_RenderWidth = 640;
static int g_RenderHeight = 480;
static int g_RefreshRate = 60;

static bool g_Exiting = false;

void CreateLightmapPages( void );
void RenderFrame( void );

//-----------------------------------------------------------------------------
// Command-line...
//-----------------------------------------------------------------------------

int FindCommand( const char *pCommand )
{
	for ( int i = 0; i < g_ArgC; i++ )
	{
		if ( !stricmp( pCommand, g_ppArgV[i] ) )
			return i;
	}
	return -1;
}

const char* CommandArgument( const char *pCommand )
{
	int cmd = FindCommand( pCommand );
	if ((cmd != -1) && (cmd < g_ArgC - 1))
	{
		return g_ppArgV[cmd+1];
	}
	return 0;
}

void SetupCommandLine( const char* pCommandLine )
{
	Q_strncpy( g_pCommandLine, pCommandLine, sizeof( g_pCommandLine ) );

	g_ArgC = 0;

	char* pStr = g_pCommandLine;
	while ( *pStr )
	{
		pStr = strtok( pStr, " " );
		if( !pStr )
		{
			break;
		}
		g_ppArgV[g_ArgC++] = pStr;
		pStr += strlen(pStr) + 1;
	}
}
//-----------------------------------------------------------------------------
// DummyMaterialProxyFactory...
//-----------------------------------------------------------------------------

class DummyMaterialProxyFactory : public IMaterialProxyFactory
{
public:
	virtual IMaterialProxy *CreateProxy( const char *proxyName )	{return NULL;}
	virtual void DeleteProxy( IMaterialProxy *pProxy )				{}
};
static DummyMaterialProxyFactory	g_DummyMaterialProxyFactory;

//-----------------------------------------------------------------------------
// Spew function!
//-----------------------------------------------------------------------------
SpewRetval_t SpewFunc( SpewType_t spewType, char const *pMsg )
{
	OutputDebugString( pMsg );
	switch( spewType )
	{
	case SPEW_MESSAGE:
	case SPEW_WARNING:
	case SPEW_LOG:
		OutputDebugString( pMsg );
		return SPEW_CONTINUE;

	case SPEW_ASSERT:
	case SPEW_ERROR:
	default:
		::MessageBox( NULL, pMsg, "Error!", MB_OK );
		return SPEW_DEBUGGER;
	}
}

//-----------------------------------------------------------------------------
// Error...
//-----------------------------------------------------------------------------

void DisplayError( const char* pError, ... )
{
	va_list		argptr;
	char		msg[1024];
	
	va_start( argptr, pError );
	Q_vsnprintf( msg, sizeof( msg ), pError, argptr );
	va_end( argptr );

	MessageBox( 0, msg, 0, MB_OK );
}



IFileSystem *g_pFileSystem;
static CSysModule *g_pFileSystemModule = NULL;
static CreateInterfaceFn g_pFileSystemFactory = NULL;
static bool FileSystem_LoadDLL( void )
{
	g_pFileSystemModule = Sys_LoadModule( "filesystem_stdio.dll" );
	Assert( g_pFileSystemModule );
	if( !g_pFileSystemModule )
	{
		return false;
	}

	g_pFileSystemFactory = Sys_GetFactory( g_pFileSystemModule );
	if( !g_pFileSystemFactory )
	{
		return false;
	}
	g_pFileSystem = ( IFileSystem * )g_pFileSystemFactory( FILESYSTEM_INTERFACE_VERSION, NULL );
	Assert( g_pFileSystem );
	if( !g_pFileSystem )
	{
		return false;
	}
	return true;
}

void FileSystem_UnloadDLL( void )
{
	if ( !g_pFileSystemModule )
		return;

	Sys_UnloadModule( g_pFileSystemModule );
	g_pFileSystemModule = 0;
}

void FileSystem_Init( )
{
	if( !FileSystem_LoadDLL() )
	{
		return;
	}

	g_pFileSystem->RemoveSearchPath( NULL, "GAME" );
	g_pFileSystem->AddSearchPath( "hl2", "GAME", PATH_ADD_TO_HEAD );
}

void FileSystem_Shutdown( void )
{
	g_pFileSystem->Shutdown();

	FileSystem_UnloadDLL();
}


//-----------------------------------------------------------------------------
// Purpose: Unloads the material system .dll
//-----------------------------------------------------------------------------
void UnloadMaterialSystem( void )
{
	if ( !g_MaterialsFactory )
		return;

	Sys_UnloadModule( g_MaterialsDLL );
	g_MaterialsDLL = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Loads the material system dll
//-----------------------------------------------------------------------------
void LoadMaterialSystem( void )
{
	if( g_MaterialsFactory )
	{
		return;
	}

	Assert( !g_MaterialsDLL );

	g_MaterialsDLL = Sys_LoadModule( "MaterialSystem.dll" );

	g_MaterialsFactory = Sys_GetFactory( g_MaterialsDLL );
	if ( !g_MaterialsFactory )
	{
		DisplayError( "LoadMaterialSystem:  Failed to load MaterialSystem.DLL\n" );
		return;
	}

	if ( g_MaterialsFactory )
	{
		g_pMaterialSystem = (IMaterialSystem *)g_MaterialsFactory( MATERIAL_SYSTEM_INTERFACE_VERSION, NULL );
		if ( !g_pMaterialSystem )
		{
			DisplayError( "Could not get the material system interface from materialsystem.dll (a)" );
		}
	}
	else
	{
		DisplayError( "Could not find factory interface in library MaterialSystem.dll" );
	}
}

void InitMaterialSystemConfig(MaterialSystem_Config_t *pConfig)
{
}

CreateInterfaceFn g_MaterialSystemClientFactory;
void Shader_Init( HWND mainWindow )
{
	LoadMaterialSystem();
	Assert( g_pMaterialSystem );

	// FIXME: Where do we put this?
	const char* pDLLName;
	pDLLName = "shaderapidx9";

	// assume that IFileSystem paths have already been set via g_pFileSystem 
	g_MaterialSystemClientFactory = g_pMaterialSystem->Init( 
		pDLLName, &g_DummyMaterialProxyFactory, g_pFileSystemFactory, VStdLib_GetICVarFactory() );
	if (!g_MaterialSystemClientFactory)
	{
		DisplayError( "Unable to init shader system\n" );
	}

	// Get the adapter from the command line....
	MaterialVideoMode_t mode;
	memset( &mode, 0, sizeof( mode ) );
	mode.m_Width = g_RenderWidth;
	mode.m_Height = g_RenderHeight;
	mode.m_Format = IMAGE_FORMAT_BGRX8888;
	mode.m_RefreshRate = g_RefreshRate;
//	bool modeSet = g_pMaterialSystem->SetMode( (void*)mainWindow, mode, modeFlags );
//	if (!modeSet)
//	{
//		DisplayError( "Unable to set mode\n" );
//	}

	g_pMaterialSystemHardwareConfig = (IMaterialSystemHardwareConfig*)
		g_MaterialSystemClientFactory( MATERIALSYSTEM_HARDWARECONFIG_INTERFACE_VERSION, 0 );
	if ( !g_pMaterialSystemHardwareConfig )
	{
		DisplayError( "Could not get the material system hardware config interface!" );
	}

	MaterialSystem_Config_t config;
	InitMaterialSystemConfig(&config);
	g_pMaterialSystem->OverrideConfig( config, false );
	if( FindCommand( "-stub" ) != -1 )
	{
		g_pMaterialSystem->SetInStubMode( true );
	}
}

void Shader_Shutdown( HWND hwnd )
{
	// Recursive shutdown
	if ( !g_pMaterialSystem )
		return;

	g_pMaterialSystem->Shutdown( );
	g_pMaterialSystem = NULL;
	UnloadMaterialSystem();
}

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
// Window create, destroy...
//-----------------------------------------------------------------------------
LRESULT WINAPI MsgProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
    switch( msg )
    {
	case WM_PAINT:
		RenderFrame();
		break;
		// abort when ESC is hit
	case WM_CHAR:
		switch(wParam)
		{
		case VK_ESCAPE:
			SendMessage( g_HWnd, WM_CLOSE, 0, 0 );
			break;
		}
		break;
		
        case WM_DESTROY:
			g_Exiting = true;
            PostQuitMessage( 0 );
            return 0;
    }
	
    return DefWindowProc( hWnd, msg, wParam, lParam );
}

bool CreateAppWindow( const char* pAppName, int width, int height )
{
    // Register the window class
	WNDCLASSEX windowClass = 
	{ 
		sizeof(WNDCLASSEX), CS_CLASSDC, MsgProc, 0L, 0L,
        GetModuleHandle(NULL), NULL, NULL, NULL, NULL,
        pAppName, NULL 
	};

    RegisterClassEx( &windowClass );

    // Create the application's window
    g_HWnd = CreateWindow( pAppName, pAppName,
		WS_OVERLAPPEDWINDOW, 
		0, 0, width, height,
		GetDesktopWindow(), NULL, windowClass.hInstance, NULL );
	
    ShowWindow (g_HWnd, SW_SHOWDEFAULT);
	
	return (g_HWnd != 0);
}

void DestroyAppWindow()
{
	if (g_HWnd)
		DestroyWindow( g_HWnd );
}
bool Init( const char* pCommands)
{
	// Store off the command line...
	SetupCommandLine( pCommands );
	FileSystem_Init( );

	/* figure out g_Renderwidth and g_RenderHeight */
	g_RenderWidth = -1;
	g_RenderHeight = -1;

	g_RenderWidth = 640;
	g_RenderHeight = 480;

	int windowWidth, windowHeight;
	CalcWindowSize( g_RenderWidth, g_RenderHeight, &windowWidth, &windowHeight );

	if( !CreateAppWindow( "matsys_regressiontest", windowWidth, windowHeight ) )
	{
		return false;
	}

	Shader_Init( g_HWnd );
	g_pMaterialSystem->CacheUsedMaterials();
	CreateLightmapPages();
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Return the directory where this .exe is running from
// Output : char
//-----------------------------------------------------------------------------

static char *GetBaseDir( const char *pszBuffer )
{
	static char	basedir[ MAX_PATH ];
	char szBuffer[ MAX_PATH ];
	int j;
	char *pBuffer = NULL;

	Q_strncpy( szBuffer, pszBuffer, sizeof( szBuffer ) );

	pBuffer = strrchr( szBuffer,'\\' );
	if ( pBuffer )
	{
		*(pBuffer+1) = '\0';
	}

	Q_strncpy( basedir, szBuffer, sizeof( basedir ) );

	j = strlen( basedir );
	if (j > 0)
	{
		if ( ( basedir[ j-1 ] == '\\' ) || 
			 ( basedir[ j-1 ] == '/' ) )
		{
			basedir[ j-1 ] = 0;
		}
	}

	return basedir;
}

//-----------------------------------------------------------------------------
// Shutdown
//-----------------------------------------------------------------------------

void Shutdown()
{
	// Close the window
	DestroyAppWindow();

	Shader_Shutdown( g_HWnd );
}

//-----------------------------------------------------------------------------
// Pump windows messages
//-----------------------------------------------------------------------------

void PumpWindowsMessages ()
{
	MSG msg;
	while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) == TRUE) 
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	
	InvalidateRect(g_HWnd, NULL, false);
	UpdateWindow(g_HWnd);
}

MaterialSystem_SortInfo_t *g_pMaterialSortInfo = NULL;
struct LightmapInfo_t
{
	int m_LightmapDims[2];
	int m_LightmapPageDims[2];
	int m_OffsetIntoLightmapPage[2];
	int m_SortID;
};

LightmapInfo_t g_CheckerboardLightmapInfo;

void AllocateLightmap( LightmapInfo_t& lightmapInfo, int width, int height )
{
	lightmapInfo.m_LightmapDims[0] = width;
	lightmapInfo.m_LightmapDims[1] = height;
	// HACK!  We don't ever use this material for anything. . just need it for allocating lightmaps.
	CMatRenderContextPtr pRenderContext( materials );
	IMaterial *pHackMaterial = g_pMaterialSystem->FindMaterial( "shadertest/lightmappedgeneric", TEXTURE_GROUP_OTHER );
	pRenderContext->Bind( pHackMaterial );
	lightmapInfo.m_SortID = 
		g_pMaterialSystem->AllocateLightmap( 16, 16, lightmapInfo.m_OffsetIntoLightmapPage, pHackMaterial );
}

void Lightmap_PostAllocation( LightmapInfo_t& lightmapInfo )
{
	g_pMaterialSystem->GetLightmapPageSize( lightmapInfo.m_SortID,
		&lightmapInfo.m_LightmapPageDims[0], &lightmapInfo.m_LightmapPageDims[1] );
}

void UpdateLightmap( LightmapInfo_t& lightmapInfo, float *data )
{
	g_pMaterialSystem->UpdateLightmap( g_pMaterialSortInfo[lightmapInfo.m_SortID].lightmapPageID,
		lightmapInfo.m_LightmapDims, lightmapInfo.m_OffsetIntoLightmapPage, data, NULL, NULL, NULL );
}

void CreateLightmapPages( void )
{
	g_pMaterialSystem->BeginLightmapAllocation();

	AllocateLightmap( g_CheckerboardLightmapInfo, CHECKERBOARD_DIMS, CHECKERBOARD_DIMS );

	g_pMaterialSystem->EndLightmapAllocation();
	int numSortIDs = g_pMaterialSystem->GetNumSortIDs();
	g_pMaterialSortInfo = new MaterialSystem_SortInfo_t[numSortIDs];
	g_pMaterialSystem->GetSortInfo( g_pMaterialSortInfo );

	Lightmap_PostAllocation( g_CheckerboardLightmapInfo );

	float checkerboardImage[CHECKERBOARD_DIMS*CHECKERBOARD_DIMS*4];
	int x, y;
	for( y = 0; y < CHECKERBOARD_DIMS; y++ )
	{
		for( x = 0; x < CHECKERBOARD_DIMS; x++ )
		{
			if( ( x + y ) & 1 )
			{
				checkerboardImage[(y*CHECKERBOARD_DIMS+x)*4+0] = 1.0f;
				checkerboardImage[(y*CHECKERBOARD_DIMS+x)*4+1] = 1.0f;
				checkerboardImage[(y*CHECKERBOARD_DIMS+x)*4+2] = 1.0f;
				checkerboardImage[(y*CHECKERBOARD_DIMS+x)*4+3] = 1.0f;
			}
			else
			{
				checkerboardImage[(y*CHECKERBOARD_DIMS+x)*4+0] = 0.0f;
				checkerboardImage[(y*CHECKERBOARD_DIMS+x)*4+1] = 0.0f;
				checkerboardImage[(y*CHECKERBOARD_DIMS+x)*4+2] = 0.0f;
				checkerboardImage[(y*CHECKERBOARD_DIMS+x)*4+3] = 1.0f;
			}
		}
	}

	UpdateLightmap( g_CheckerboardLightmapInfo, checkerboardImage );
}

void LightmapTexCoord( CMeshBuilder& meshBuilder, LightmapInfo_t lightmapInfo, float s, float t )
{
	// add a one texel border.
	float newS = ( s * ( lightmapInfo.m_LightmapDims[0] - 2 ) ) + 1;
	float newT = ( t * ( lightmapInfo.m_LightmapDims[1] - 2 ) ) + 1;
	newS += lightmapInfo.m_OffsetIntoLightmapPage[0];
	newT += lightmapInfo.m_OffsetIntoLightmapPage[1];
	newS *= 1.0f / lightmapInfo.m_LightmapPageDims[0];
	newT *= 1.0f / lightmapInfo.m_LightmapPageDims[1];
	meshBuilder.TexCoord2f( 1, newS, newT );
}

//-----------------------------------------------------------------------------
// Update
//-----------------------------------------------------------------------------
bool Update()
{
	PumpWindowsMessages();
	return g_Exiting;
}

void ScreenShot( const char *pFilename )
{
	// bitmap bits
	unsigned char *pImage = ( unsigned char * )malloc( g_RenderWidth * 3 * g_RenderHeight );

	// Get Bits from the material system
	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->ReadPixels( 0, 0, g_RenderWidth, g_RenderHeight, pImage, IMAGE_FORMAT_RGB888 );

	CUtlBuffer outBuf;
	if( !TGAWriter::WriteToBuffer( pImage, outBuf, g_RenderWidth, g_RenderHeight, IMAGE_FORMAT_RGB888,
		IMAGE_FORMAT_RGB888 ) )
	{
		Error( "Couldn't write %s\n", pFilename );
	}
	if ( !g_pFullFileSystem->WriteFile( pFilename, NULL, outBuf ) )
	{
		Error( "Couldn't write %s\n", pFilename );
	}
	
	free( pImage );
}

void SetVar( const char *pVarName, const char *pStringValue )
{
	IMaterialVar *pVar = g_pProceduralMaterial->FindVar( pVarName, NULL );
	if( pStringValue )
	{
		pVar->SetStringValue( pStringValue );
	}
	else
	{
		pVar->SetUndefined();
	}
}

void SetVar( const char *pVarName, int val )
{
	IMaterialVar *pVar = g_pProceduralMaterial->FindVar( pVarName, NULL );
	pVar->SetIntValue( val );
}

void SetFlag( MaterialVarFlags_t flag, int val )
{
	g_pProceduralMaterial->SetMaterialVarFlag( flag, val ? true : false );
}

void DrawBackground( void )
{
	CMatRenderContextPtr pRenderContext( materials );
	IMaterial *pMaterial = g_pMaterialSystem->FindMaterial( "matsys_regressiontest/background", TEXTURE_GROUP_OTHER );
	pRenderContext->Bind( pMaterial );

	IMesh *pMesh = pRenderContext->GetDynamicMesh();
	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );
	
#define X 500.0f
#define Y 500.0f
#define Z -500.0f

	meshBuilder.Position3f( -X, Y, Z );
	meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( X, Y, Z );
	meshBuilder.TexCoord2f( 0, 1.0f, 0.0f );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( X, -Y, Z );
	meshBuilder.TexCoord2f( 0, 1.0f, 1.0f );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( -X, -Y, Z );
	meshBuilder.TexCoord2f( 0, 0.0f, 1.0f );
	meshBuilder.AdvanceVertex();
	
	meshBuilder.End();
	pMesh->Draw();

#undef X
#undef Y
#undef Z
}

void BeginFrame( void )
{
	g_pMaterialSystem->BeginFrame( 0 );

	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->ClearColor3ub( 255, 0, 0 );
	pRenderContext->ClearBuffers( true, true );
	pRenderContext->Viewport( 0, 0, g_RenderWidth, g_RenderHeight );

	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->LoadIdentity();
	pRenderContext->PerspectiveX( 90.0f, ( g_RenderWidth / g_RenderHeight), 1.0f, 500000.0f );

	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->LoadIdentity();

	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->LoadIdentity();
}

void EndFrame( void )
{
	g_pMaterialSystem->EndFrame();
	g_pMaterialSystem->SwapBuffers();
}

void DrawQuad( const LightmapInfo_t& lightmapInfo )
{
	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->BindLightmapPage( g_pMaterialSortInfo[lightmapInfo.m_SortID].lightmapPageID );
	
	pRenderContext->Bind( g_pProceduralMaterial );

	IMesh *pMesh = pRenderContext->GetDynamicMesh();
	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );
	
#define X 350.0f
#define Y (X*(4.0f/3.0f))
#define Z -500.0f

	meshBuilder.Position3f( -X, Y, Z );
	meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
	LightmapTexCoord( meshBuilder, lightmapInfo, 0.0f, 0.0f );
	meshBuilder.Normal3f( 0.0f, 0.0f, 1.0f );
	meshBuilder.Color4f( 1.0f, 0.0f, 0.0f, 1.0f );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( X, Y, Z );
	meshBuilder.TexCoord2f( 0, 1.0f, 0.0f );
	LightmapTexCoord( meshBuilder, lightmapInfo, 1.0f, 0.0f );
	meshBuilder.Normal3f( 0.0f, 0.0f, 1.0f );
	meshBuilder.Color4f( 1.0f, 1.0f, 0.0f, 1.0f );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( X, -Y, Z );
	meshBuilder.TexCoord2f( 0, 1.0f, 1.0f );
	LightmapTexCoord( meshBuilder, lightmapInfo, 1.0f, 1.0f );
	meshBuilder.Normal3f( 0.0f, 0.0f, 1.0f );
	meshBuilder.Color4f( 1.0f, 1.0f, 1.0f, 0.0f );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( -X, -Y, Z );
	meshBuilder.TexCoord2f( 0, 0.0f, 1.0f );
	LightmapTexCoord( meshBuilder, lightmapInfo, 0.0f, 1.0f );
	meshBuilder.Normal3f( 0.0f, 0.0f, 1.0f );
	meshBuilder.Color4f( 0.0f, 1.0f, 1.0f, 1.0f );
	meshBuilder.AdvanceVertex();
	
	meshBuilder.End();
	pMesh->Draw();

#undef X
#undef Y
#undef Z
}

void TestLightmappedGeneric_dx80( void )
{
/*
based on lightmappedgeneric_vs11.fxc:
// STATIC: "DETAIL" "0..1"
// STATIC: "ENVMAP" "0..1"
// STATIC: "ENVMAPCAMERASPACE" "0..1"
// STATIC: "ENVMAPSPHERE" "0..1"
// STATIC: "VERTEXCOLOR" "0..1"
// DYNAMIC: "FOGTYPE"				"0..1"

// SKIP: $ENVMAPCAMERASPACE && $ENVMAPSPHERE
// SKIP: !$ENVMAP && ( $ENVMAPCAMERASPACE || $ENVMAPSPHERE )

based on lightmappedgeneric_ps11.fxc:
// STATIC: "BASETEXTURE"			"0..1"
// STATIC: "ENVMAP"					"0..1"
// STATIC: "ENVMAPMASK"				"0..1"
// STATIC: "SELFILLUM"				"0..1"
// STATIC: "BASEALPHAENVMAPMASK"	"0..1"

// SKIP: !$ENVMAP && ( $BASEALPHAENVMAPMASK || $ENVMAPMASK )
// SKIP: !$BASETEXTURE && $BASEALPHAENVMAPMASK
// SKIP: $BASEALPHAENVMAPMASK && $ENVMAPMASK
// SKIP: !$BASETEXTURE && $BASEALPHAENVMAPMASK
// SKIP: $SELFILLUM && $BASEALPHAENVMAPMASK
// SKIP: !$BASETEXTURE && $SELFILLUM


Versions with DETAIL aren't implemented yet in HLSL, but we'll go ahead and test those.
*/  	
	// The following are shader combos from lightmappedgeneric_ps20
	for( int BUMPMAP = 0; BUMPMAP <= 1; BUMPMAP++ )
	{
		for( int NORMALMAPALPHAENVMAPMASK = 0; NORMALMAPALPHAENVMAPMASK <= 1; NORMALMAPALPHAENVMAPMASK++ )
		{
			// The following are shader combos from lightmappedgeneric_ps11 (that aren't in lightmappedgenerc_vs11)
			for( int BASETEXTURE = 0; BASETEXTURE <= 1; BASETEXTURE++ )
			{
				for( int ENVMAPMASK = 0; ENVMAPMASK <= 1; ENVMAPMASK++ )
				{
					for( int SELFILLUM = 0; SELFILLUM <= 1; SELFILLUM++ )
					{
						for( int BASEALPHAENVMAPMASK = 0; BASEALPHAENVMAPMASK <= 1; BASEALPHAENVMAPMASK++ )
						{
							// The following are shader combos from lightmappedgeneric_vs11
							for( int DETAIL = 0; DETAIL <= 1; DETAIL++ )
							{
								for( int ENVMAP = 0; ENVMAP <= 1; ENVMAP++ )
								{
									for( int ENVMAPCAMERASPACE = 0; ENVMAPCAMERASPACE <= 1; ENVMAPCAMERASPACE++ )
									{
										for( int ENVMAPSPHERE = 0; ENVMAPSPHERE <= 1; ENVMAPSPHERE++ )
										{
											for( int VERTEXCOLOR = 0; VERTEXCOLOR <= 1; VERTEXCOLOR++ )
											{
	//											for( int FOGTYPE = 0; FOGTYPE <= 1; FOGTYPE++ )
												{
													// conditional beneath here are flags, not shader combos
													for( int ALPHATEST = 0; ALPHATEST <= 1; ALPHATEST++ )
													{
														for( int TRANSLUCENT = 0; TRANSLUCENT <= 1; TRANSLUCENT++ )
														{
															for( int COLORMODULATE = 0; COLORMODULATE <= 1; COLORMODULATE++ )
															{
																for( int ALPHAMODULATE = 0; ALPHAMODULATE <= 1; ALPHAMODULATE++ )
																{
#if 0
																	BUMPMAP = 0;
																	NORMALMAPALPHAENVMAPMASK = 0;
																	BASETEXTURE = 1;
																	ENVMAPMASK = 0;
																	SELFILLUM = 0;
																	BASEALPHAENVMAPMASK = 1;
																	DETAIL = 0;
																	ENVMAP = 1;
																	ENVMAPCAMERASPACE = 0;
																	ENVMAPSPHERE = 0;
																	VERTEXCOLOR = 0;
																	ALPHATEST = 0;
																	TRANSLUCENT = 0;
																	COLORMODULATE = 0;
																	ALPHAMODULATE = 0;
#endif
																	// skip commands from shader lightmappedgeneric_vs11
																	if( ENVMAPCAMERASPACE && ENVMAPSPHERE ) continue;
																	if( !ENVMAP && ( ENVMAPCAMERASPACE || ENVMAPSPHERE ) ) continue;
																	
																	// skip commands from shader lihgtmappedgeneric_ps11
																	if( !ENVMAP && ( BASEALPHAENVMAPMASK || ENVMAPMASK ) ) continue;
																	if( !BASETEXTURE && BASEALPHAENVMAPMASK ) continue;
																	if( BASEALPHAENVMAPMASK && ENVMAPMASK ) continue;
																	if( !BASETEXTURE && BASEALPHAENVMAPMASK ) continue;
																	if( SELFILLUM && BASEALPHAENVMAPMASK ) continue;
																	if( !BASETEXTURE && SELFILLUM ) continue;
																	
																	// skip commands from shader lightmappedgeneric_ps20
																	if( DETAIL && BUMPMAP ) continue;
																	if( ENVMAPMASK && BUMPMAP ) continue;
																	if( NORMALMAPALPHAENVMAPMASK && BASEALPHAENVMAPMASK ) continue;
																	if( NORMALMAPALPHAENVMAPMASK && ENVMAPMASK ) continue;
																	if( BASEALPHAENVMAPMASK && ENVMAPMASK ) continue;
																	if( BASEALPHAENVMAPMASK && SELFILLUM ) continue;
																	
																	// skip commands from flags that don't make sense (or we don't want to test)
																	if( !BASETEXTURE && ( ALPHATEST || TRANSLUCENT ) ) continue;
																	if( TRANSLUCENT && ALPHATEST ) continue;
																	
																	KeyValues *pKeyValues = new KeyValues( "lightmappedgeneric" );
																	if ( BASETEXTURE )
																	{
																		pKeyValues->SetString( "$basetexture", "matsys_regressiontest/basetexture" );
																	}
																	if ( ENVMAPMASK )
																	{
																		pKeyValues->SetString( "$envmapmask", "matsys_regressiontest/specularmask" );
																	}
																	if ( SELFILLUM )
																	{
																		pKeyValues->SetInt( "$selfillum", 1 );
																	}
																	if ( BASEALPHAENVMAPMASK )
																	{
																		pKeyValues->SetInt( "$basealphaenvmapmask", 1 );
																	}
																	if ( BUMPMAP )
																	{
																		pKeyValues->SetString( "$bumpmap", "matsys_regressiontest/normalmap_normal" );
																	}
																	if ( DETAIL )
																	{
																		pKeyValues->SetString( "$detail", "matsys_regressiontest/detail" );
																		pKeyValues->SetFloat( "$detailscale", 0.5f );
																	}
																	if ( ENVMAP )
																	{
																		pKeyValues->SetString( "$envmap", "matsys_regressiontest/cubemap" );
																	}
																	if ( BASEALPHAENVMAPMASK )
																	{
																		pKeyValues->SetInt( "$basealphaenvmapmask", 1 );
																	}
																	if ( NORMALMAPALPHAENVMAPMASK )
																	{
																		pKeyValues->SetInt( "$normalmapalphaenvmapmask", 1 );
																	}
																	if ( TRANSLUCENT )
																	{
																		pKeyValues->SetInt( "$translucent", 1 );
																	}
																	if ( ENVMAPCAMERASPACE )
																	{
																		pKeyValues->SetInt( "$envmapcameraspace", 1 );
																	}
																	if ( ENVMAPSPHERE )
																	{
																		pKeyValues->SetInt( "$envmapsphere", 1 );
																	}
																	if ( VERTEXCOLOR )
																	{
																		pKeyValues->SetInt( "$vertexcolor", 1 );
																	}
																	if ( ALPHATEST )
																	{
																		pKeyValues->SetInt( "$alphatest", 1 );
																	}
																	if ( COLORMODULATE )
																	{
																		pKeyValues->SetString( "$color", "[1.0 0.8 0.5]" );
																	}
																	if ( ALPHAMODULATE )
																	{
																		pKeyValues->SetFloat( "$alpha", 0.6f );
																	}

																	// FIXME: ignoring fogtype for now.
																	// hack hack - LEAK - need to figure out how to clear a material out.
																	g_pProceduralMaterial = g_pMaterialSystem->CreateMaterial( "test", pKeyValues );
																	g_pProceduralMaterial->Refresh();

																	BeginFrame();
																	DrawBackground();
																	DrawQuad( g_CheckerboardLightmapInfo );
																	EndFrame();
																	char filename[MAX_PATH];
																	sprintf( filename, "%s\\lightmappedgeneric_dx80", CommandLine()->ParmValue( "-targetdir" ) );
																	
																	if( BUMPMAP ) Q_strcat( filename, "_BUMPMAP", sizeof(filename) );
																	if( NORMALMAPALPHAENVMAPMASK ) Q_strcat( filename, "_NORMALMAPALPHAENVMAPMASK", sizeof(filename) );

																	if( BASETEXTURE ) Q_strcat( filename, "_BASE", sizeof(filename) );
																	if( ENVMAPMASK ) Q_strcat( filename, "_ENVMAPMASK", sizeof(filename) );
																	if( SELFILLUM ) Q_strcat( filename, "_SELFILLUM", sizeof(filename) );
																	if( BASEALPHAENVMAPMASK ) Q_strcat( filename, "_BASEALPHAENVMAPMASK", sizeof(filename) );
																	
																	if( DETAIL ) Q_strcat( filename, "_DETAIL", sizeof(filename) );
																	if( ENVMAP ) Q_strcat( filename, "_ENVMAP", sizeof(filename) );
																	if( ENVMAPCAMERASPACE ) Q_strcat( filename, "_ENVMAPCAMERASPACE", sizeof(filename) );
																	if( ENVMAPSPHERE ) Q_strcat( filename, "_ENVMAPSPHERE", sizeof(filename) );
																	if( VERTEXCOLOR ) Q_strcat( filename, "_VERTEXCOLOR", sizeof(filename) );
																	//							if( FOGTYPE ) Q_strcat( filename, "_FOGTYPE", sizeof(filename) );
																	if( ALPHATEST ) Q_strcat( filename, "_ALPHATEST", sizeof(filename) );
																	if( TRANSLUCENT ) Q_strcat( filename, "_TRANSLUCENT", sizeof(filename) );
																	if( COLORMODULATE ) Q_strcat( filename, "_COLORMODULATE", sizeof(filename) );
																	if( ALPHAMODULATE ) Q_strcat( filename, "_ALPHAMODULATE", sizeof(filename) );
																	
																	if( BUMPMAP ) Q_strcat( filename, "_BUMPMAP", sizeof(filename) );
																	Q_strcat( filename, ".tga", sizeof(filename) );
																	ScreenShot( filename );

																	g_pProceduralMaterial->DecrementReferenceCount();
																}
															}
														}
													}
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Render a frame
//-----------------------------------------------------------------------------

void RenderFrame( void )
{
	TestLightmappedGeneric_dx80();
}

INT WINAPI WinMain( HINSTANCE hInst, HINSTANCE hInstance, LPSTR pCommands, INT )
{
	SpewOutputFunc( SpewFunc );
	CommandLine()->CreateCmdLine( ::GetCommandLine() );
	InitDefaultFileSystem();

	if( !CommandLine()->ParmValue( "-targetdir" ) )
	{
		Error( "Must specify -targetdir on command line!\n" );
	}
	
	struct _stat statbuf;
	if( 0 != _stat( CommandLine()->ParmValue( "-targetdir" ), &statbuf ) )
	{
		if( 0 != _mkdir( CommandLine()->ParmValue( "-targetdir" ) ) )
		{
			Error( "Couldn't create path %s\n", CommandLine()->ParmValue( "-targetdir" ) );
		}
	}

	// Must add 'bin' to the path....
	char* pPath = getenv("PATH");

	// Use the .EXE name to determine the root directory
	char moduleName[ MAX_PATH ];
	char szBuffer[ 4096 ];
	if ( !GetModuleFileName( hInstance, moduleName, MAX_PATH ) )
	{
		MessageBox( 0, "Failed calling GetModuleFileName", "Launcher Error", MB_OK );
		return 0;
	}

	// Get the root directory the .exe is in
	char* pRootDir = GetBaseDir( moduleName );

#ifdef DBGFLAG_ASSERT
	int len = 
#endif

	Q_snprintf( szBuffer, sizeof( szBuffer ), "PATH=%s;%s", pRootDir, pPath );
	Assert( len < 4096 );
	_putenv( szBuffer );
		    
	MathLib_Init( 2.2f, 2.2f, 0.0f, 2.0f );

	if (!Init( pCommands ) )
		return false;

/*
	bool done = false;
	while( !done )
	{
		done = Update();
	}
	*/
	RenderFrame();
	Shutdown();

    return 0;
}

