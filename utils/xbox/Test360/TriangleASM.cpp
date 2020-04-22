//========= Copyright Valve Corporation, All rights reserved. ============//
//--------------------------------------------------------------------------------------
// TriangleASM.cpp
//
// Hijacked from samples, assimilated into valve app.
//--------------------------------------------------------------------------------------

#include "tier0\platform.h"
#if !defined( _X360 )
#include <windows.h>
#endif
#include "appframework\iappsystemgroup.h"
#include "appframework\appframework.h"
#include "tier0\dbg.h"
#include "tier1\interface.h"
#include "filesystem.h"
#include "vstdlib\cvar.h"
#include "filesystem_init.h"
#include "tier1/utlbuffer.h"
#include "icommandline.h"
#include "datacache\idatacache.h"
#include "datacache\imdlcache.h"
#include "studio.h"
#include "utlbuffer.h"
#include "tier2\utlstreambuffer.h"
#include "tier2\tier2.h"
#include "tier3\tier3.h"
#include "mathlib/mathlib.h"
#include "inputsystem\iinputsystem.h"
#include "vphysics_interface.h"
#include "istudiorender.h"
#include "studio.h"
#include "tier1\KeyValues.h"
#include "vgui\IVGui.h"
#include "vguimatsurface\imatsystemsurface.h"
#include "matsys_controls\matsyscontrols.h"
#include "vgui\ILocalize.h"
#include "vgui_controls\panel.h"
#include "vgui_controls\label.h"
#if defined( _X360 )
#include "xbox\xbox_console.h"
#include "xbox\xbox_win32stubs.h"
#endif
#include "materialsystem\imaterialsystem.h"
#include "materialsystem\imesh.h"
#include "materialsystem\materialsystem_config.h"
#include "materialsystem\MaterialSystemUtil.h"
#include "materialsystem\ishaderapi.h"
#if !defined( _X360 )
#include "xbox\xboxstubs.h"
#endif
#include "bone_setup.h"
#include "tier0\memdbgon.h"

bool g_bActive = true;

extern SpewOutputFunc_t g_DefaultSpewFunc;

// These must be turned on in order....
#define USE_FILESYSTEM
#define USE_MATERIALSYSTEM
#define USE_VPHYSICS

// Note: VPHYSICS is just used via the datacache to load a model's physics collision mesh

// These can be turned on in any order
#define USE_INPUTSYSTEM
#define USE_VGUI
#define USE_STUDIORENDER

#pragma warning(disable:4189) // local variable is initialized but not referenced

//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
class CTest360App : public CDefaultAppSystemGroup<CSteamAppSystemGroup>
{
public:
	virtual bool	Create();
	virtual bool	PreInit();
	virtual int		Main();
	virtual void	Destroy();

private:
	const char		*GetAppName() { return "TEST360"; }
	void			RenderScene();
	bool			CreateMainWindow( int width, int height, bool fullscreen );

#if defined( USE_MATERIALSYSTEM )
	bool			SetupMaterialSystem();
#endif

#if defined( USE_STUDIORENDER )
	bool			SetupStudioRender();
	bool			LoadModel( const char *pModelName );
	matrix3x4_t* 	SetUpBones( studiohdr_t *pStudioHdr, const matrix3x4_t &modelMatrix );
#endif

#if defined( USE_VGUI )
	int				InitializeVGUI( void );
	void			ShutdownVGUI( void );
#endif

	IPhysicsCollision *m_pCollision;
    IMaterialSystem	*m_pMaterialSystem;
	IFileSystem		*m_pFileSystem;
	int				m_nWidth;
	int				m_nHeight;
	float			m_fAspect;
	float			m_NearClip;
	float			m_FarClip;
	float			m_fov;
	HWND			m_hWnd;
	studiohdr_t		*m_pStudioHdr;
	studiohwdata_t	*m_pStudioHWData;
	int				m_nLod;
	float			m_flTime;
	float			m_flPlaybackRate;
	vgui::Panel		*m_pMainPanel;
};


static float	g_yaw;
static float	g_horizontalPan;
static float	g_verticalPan;
static float	g_zoom;
static int		g_sequence;
static bool		g_bWireframe;

DEFINE_WINDOWED_STEAM_APPLICATION_OBJECT( CTest360App );

#if defined( USE_MATERIALSYSTEM )
static void MaterialSystem_Warning( const char *fmt, ... )
{
	va_list		argptr;
	char		msg[2048];
	
	va_start( argptr, fmt );
	Q_vsnprintf( msg, sizeof ( msg ), fmt, argptr );
	va_end( argptr );

	OutputDebugString( msg );
}
#endif

#if defined( USE_MATERIALSYSTEM )
static void MaterialSystem_Warning( char *fmt, ... )
{
	va_list		argptr;
	char		msg[2048];
	
	va_start( argptr, fmt );
	Q_vsnprintf( msg, sizeof( msg ), fmt, argptr );
	va_end( argptr );

	OutputDebugString( msg );
}
#endif

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

#if defined( USE_STUDIORENDER )
matrix3x4_t* CTest360App::SetUpBones( studiohdr_t *pStudioHdr, const matrix3x4_t &shapeToWorld )
{
	// Default to middle of the pose parameter range
	float pPoseParameter[MAXSTUDIOPOSEPARAM];
	for ( int i = 0; i < MAXSTUDIOPOSEPARAM; ++i )
	{
		pPoseParameter[i] = 0.5f;
	}

	CStudioHdr studioHdr( pStudioHdr, g_pMDLCache );

	int nFrameCount = Studio_MaxFrame( &studioHdr, g_sequence, pPoseParameter );
	if ( nFrameCount == 0 )
	{
		nFrameCount = 1;
	}
	float flCycle = ( m_flTime * m_flPlaybackRate ) / nFrameCount;

	// FIXME: We're always wrapping; may want to determing if we should clamp
	flCycle -= (int)(flCycle);

	int boneMask = BONE_USED_BY_VERTEX_AT_LOD( m_nLod );

	Vector		pos[MAXSTUDIOBONES];
	Quaternion	q[MAXSTUDIOBONES];

	IBoneSetup boneSetup( &studioHdr, boneMask, pPoseParameter );
	boneSetup.InitPose( pos, q );
	boneSetup.AccumulatePose( pos, q, g_sequence, flCycle, 1.0f, m_flTime, NULL );

	// FIXME: Try enabling this?
//	CalcAutoplaySequences( pStudioHdr, NULL, pos, q, pPoseParameter, BoneMask( ), flTime );

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
	return pBoneToWorld;
}
#endif

//--------------------------------------------------------------------------------------
// LoadModel
//
//--------------------------------------------------------------------------------------
#if defined( USE_STUDIORENDER )
bool CTest360App::LoadModel( const char* pModelName )
{
	MDLHandle_t hMdl = g_pMDLCache->FindMDL( pModelName );

	m_pStudioHdr = g_pMDLCache->GetStudioHdr( hMdl );
	
	g_pMDLCache->GetVertexData( hMdl );
	g_pMDLCache->FinishPendingLoads();

	g_pMDLCache->GetHardwareData( hMdl );
	g_pMDLCache->FinishPendingLoads();

	m_pStudioHWData = g_pMDLCache->GetHardwareData( hMdl );

	g_sequence = 0;
	m_nLod = 0;
	m_flPlaybackRate = 30.0;
	g_yaw = 0;
	g_zoom = -100;
	g_horizontalPan = 0;
	g_verticalPan = -30;

	return true;
}
#endif

//--------------------------------------------------------------------------------------
// SetupMaterialSystem
//
//--------------------------------------------------------------------------------------
#if defined( USE_MATERIALSYSTEM )
bool CTest360App::SetupMaterialSystem()
{
	RECT rect;
	
	MaterialSystem_Config_t config;
	config.SetFlag( MATSYS_VIDCFG_FLAGS_WINDOWED, IsPC() ? true : false );
	config.SetFlag( MATSYS_VIDCFG_FLAGS_NO_WAIT_FOR_VSYNC, 0 );

	config.m_VideoMode.m_Width = 0;
	config.m_VideoMode.m_Height = 0;
	config.m_VideoMode.m_Format = IMAGE_FORMAT_BGRX8888;
	config.m_VideoMode.m_RefreshRate = 0;
	config.dxSupportLevel = IsX360() ? 98 : 0;

	bool modeSet = m_pMaterialSystem->SetMode( m_hWnd, config );
	if ( !modeSet )
	{
		Error( "Failed to set mode\n" );
		return false;
	}

	m_pMaterialSystem->OverrideConfig( config, false );

	GetClientRect( m_hWnd, &rect );
	m_nWidth = rect.right;
	m_nHeight = rect.bottom;
	m_fAspect = (float)m_nWidth/(float)m_nHeight;
	
    m_NearClip = 8.0f;
    m_FarClip = 28400.0f;
    m_fov = 90;

	return true;
}
#endif

//--------------------------------------------------------------------------------------
// SetupStudioRender
//
//--------------------------------------------------------------------------------------
#if defined( USE_STUDIORENDER )
bool CTest360App::SetupStudioRender()
{
	StudioRenderConfig_t config;
	memset( &config, 0, sizeof(config) );

	config.bEyeMove = false;
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
	config.SetNormals( false );
	config.SetTangentFrame( false );
	config.skin = 0;
	
	config.fullbright = 0;
	config.pConDPrintf = MaterialSystem_Warning;
	config.pConPrintf = MaterialSystem_Warning;

	config.bShowEnvCubemapOnly = false;
	
	g_pStudioRender->UpdateConfig( config );

	return true;
}
#endif

//--------------------------------------------------------------------------------------
// Render a model using the MaterialSystem
//--------------------------------------------------------------------------------------
void CTest360App::RenderScene()
{
	m_flTime = Plat_FloatTime();

#if defined( USE_MATERIALSYSTEM )
	CMatRenderContextPtr pRenderContext( m_pMaterialSystem );
	m_pMaterialSystem->BeginFrame();
#endif

#if defined( USE_STUDIORENDER )
	g_pStudioRender->BeginFrame();
#endif

#if defined( USE_MATERIALSYSTEM )
	pRenderContext->ClearColor3ub( 0, 0, 0 );
	pRenderContext->ClearBuffers( true, true );
#endif

#if defined( USE_STUDIORENDER )
	pRenderContext->Viewport( 0, 0, m_nWidth, m_nHeight );
	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->LoadIdentity();
	pRenderContext->PerspectiveX( m_fov, m_fAspect, m_NearClip, m_FarClip );

	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->LoadIdentity();
	pRenderContext->Translate( g_horizontalPan, g_verticalPan, g_zoom );

	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->LoadIdentity();

	g_pStudioRender->SetLocalLights( 0, NULL );
	pRenderContext->SetAmbientLight( 1.0, 1.0, 1.0 );

	QAngle angles;
	angles[YAW] = 0;
	angles[PITCH] = -90 + g_yaw;
	angles[ROLL] = -90;

	matrix3x4_t cameraMatrix;
	AngleMatrix( angles, cameraMatrix );

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

	matrix3x4_t *pBoneToWorld = SetUpBones( m_pStudioHdr, cameraMatrix );

	Vector modelOrigin( 0, 0, 0 );

	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PushMatrix();

	DrawModelInfo_t modelInfo;
	memset( &modelInfo, 0, sizeof( modelInfo ) );
	modelInfo.m_pStudioHdr = m_pStudioHdr;
	modelInfo.m_pHardwareData = m_pStudioHWData;
	modelInfo.m_Decals = STUDIORENDER_DECAL_INVALID;
	modelInfo.m_Skin = 0;
	modelInfo.m_Body = 0;
	modelInfo.m_HitboxSet = 0;
	modelInfo.m_pClientEntity = NULL;
	modelInfo.m_Lod = 0;
	modelInfo.m_ppColorMeshes = NULL;

	int drawFlags = 0;
	if ( g_bWireframe )
	{
		drawFlags |= STUDIORENDER_DRAW_WIREFRAME;
	}

	g_pStudioRender->DrawModel( NULL, modelInfo, pBoneToWorld, modelOrigin, drawFlags );

	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PopMatrix();
#endif

#if defined( USE_MATERIALSYSTEM )
	pRenderContext->Flush( true );
#endif

#if defined( USE_VGUI )
	vgui::ivgui()->RunFrame();
	vgui::surface()->PaintTraverseEx( vgui::surface()->GetEmbeddedPanel() );
#endif

#if defined( USE_STUDIORENDER )
	g_pStudioRender->EndFrame();
#endif

#if defined( USE_MATERIALSYSTEM )
	m_pMaterialSystem->EndFrame();
	m_pMaterialSystem->SwapBuffers();
#endif
}

//--------------------------------------------------------------------------------------
// Window Proc
//--------------------------------------------------------------------------------------
LRESULT CALLBACK WndProc( HWND hWnd, UINT iMsg, WPARAM wParam, LPARAM lParam )
{
	switch ( iMsg )
	{
		case WM_CLOSE:
			g_bActive = false;
			break;
			
		case WM_DESTROY:
			PostQuitMessage( 0 );
			return 0L;

		case WM_XCONTROLLER_INSERTED:
			Msg( "Port %d: Gamepad Activated\n", wParam );
			break;

		case WM_XCONTROLLER_UNPLUGGED:
			Msg( "Port %d: Gamepad Unplugged\n", wParam );
			break;

		case WM_XCONTROLLER_KEY:
			// wParam == key
			// HIWORD( lParam ) = port
			// LOWWORD( lParam ) = sample
			Msg( "Port %d: Button %d %s\n", HIWORD( lParam ), wParam, LOWORD( lParam ) ? "Pressed" : "Released" );
			switch ( wParam )
			{
			case XK_BUTTON_RTRIGGER:
				if ( LOWORD( lParam ) )
				{
					g_zoom++;
				}
				break;

			case XK_BUTTON_LTRIGGER:
				if ( LOWORD( lParam ) )
				{
					g_zoom--;
				}
				break;

			case XK_BUTTON_DOWN:
				if ( LOWORD( lParam ) )
				{
					g_verticalPan += 2;
				}
				break;

			case XK_BUTTON_UP:
				if ( LOWORD( lParam ) )
				{
					g_verticalPan -= 2;
				}
				break;

			case XK_BUTTON_LEFT:
				if ( LOWORD( lParam ) )
				{
					g_horizontalPan += 2;
				}
				break;

			case XK_BUTTON_RIGHT:
				if ( LOWORD( lParam ) )
				{
					g_horizontalPan -= 2;
				}
				break;

			case XK_STICK2_LEFT:
				if ( LOWORD( lParam ) )
				{
					g_yaw += 5;
				}
				break;

			case XK_STICK2_RIGHT:
				if ( LOWORD( lParam ) )
				{
					g_yaw -= 5;
				}
				break;

			case XK_BUTTON_A:
				if ( LOWORD( lParam ) )
				{
					g_sequence++;
				}
				break;

			case XK_BUTTON_B:
				if ( LOWORD( lParam ) )
				{
					g_sequence--;
					if ( g_sequence < 0 )
						g_sequence = 0;
				}
				break;

			case XK_BUTTON_Y:
				if ( LOWORD( lParam ) )
				{
					g_bWireframe ^= 1;
				}
				break;
			}
			break;

		case WM_KEYDOWN:
			switch ( wParam )
			{
			case 'O':
				g_zoom++;
				break;

			case 'P':
				g_zoom--;
				break;

			case 'W':
				g_verticalPan += 2;
				break;

			case 'S':
				g_verticalPan -= 2;
				break;

			case 'A':
				g_horizontalPan += 2;
				break;

			case 'D':
				g_horizontalPan -= 2;
				break;

			case 'N':
				g_sequence--;
				if ( g_sequence < 0 )
					g_sequence = 0;
				break;

			case 'M':
				g_sequence++;
				break;
			}
			break;
	}
   
   return DefWindowProc( hWnd, iMsg, wParam, lParam );
}

//--------------------------------------------------------------------------------------
// CreateMainWindow
//
//--------------------------------------------------------------------------------------
bool CTest360App::CreateMainWindow( int width, int height, bool fullscreen )
{
   HWND        hwnd;
   WNDCLASSEX  wndclass;
   DWORD       dwStyle, dwExStyle;
   int         x, y, sx, sy;
   
   if ( ( hwnd = FindWindow( GetAppName(), GetAppName() ) ) != NULL )
   {
	   SetForegroundWindow( hwnd );
	   return true;
   }
   
   wndclass.cbSize        = sizeof (wndclass);
   wndclass.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
   wndclass.lpfnWndProc   = ::WndProc;
   wndclass.cbClsExtra    = 0;
   wndclass.cbWndExtra    = 0;
   wndclass.hInstance     = (HINSTANCE)GetAppInstance();
   wndclass.hIcon         = 0;
   wndclass.hCursor       = LoadCursor(NULL, IDC_ARROW);
   wndclass.hbrBackground = (HBRUSH)COLOR_GRAYTEXT;
   wndclass.lpszMenuName  = NULL;
   wndclass.lpszClassName = GetAppName();
   wndclass.hIconSm       = 0;
   
   if ( !RegisterClassEx( &wndclass ) )
   {
	   Error( "Window class registration failed\n" );
	   return false;
   }
   
   if ( fullscreen )
   {
	   dwExStyle = WS_EX_TOPMOST;
	   dwStyle = WS_POPUP | WS_VISIBLE;
   }
   else
   {
	   dwExStyle = 0;
	   dwStyle = WS_CAPTION | WS_SYSMENU;
   }
	
   x = y = 0;
   sx = width;
   sy = height;

   hwnd = CreateWindowEx(
			dwExStyle,
			GetAppName(),				// window class name
			GetAppName(),				// window caption
			dwStyle | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, // window style
			x,							// initial x position
			y,							// initial y position
			sx,							// initial x size
			sy,							// initial y size
			NULL,						// parent window handle
			NULL,						// window menu handle
			(HINSTANCE)GetAppInstance(),// program instance handle
			NULL);						// creation parameter
	
	if ( hwnd == NULL )
	{
	   ChangeDisplaySettings( 0, 0 );
	   Error( "Window creation failed\n" );
	   return false;
   }
   
   m_hWnd = hwnd;
   
   return true;
}

//-----------------------------------------------------------------------------
//	Create
//-----------------------------------------------------------------------------
bool CTest360App::Create()
{
	AppSystemInfo_t appSystems[] = 
	{
#if defined( USE_STUDIORENDER )
		{ "datacache.dll",			DATACACHE_INTERFACE_VERSION },
		{ "datacache.dll",			MDLCACHE_INTERFACE_VERSION },
#endif
#if defined( USE_MATERIALSYSTEM )
		{ "materialsystem.dll",		MATERIAL_SYSTEM_INTERFACE_VERSION },
#endif
#if defined( USE_STUDIORENDER )
		{ "studiorender.dll",		STUDIO_RENDER_INTERFACE_VERSION },
#endif
#if defined( USE_INPUTSYSTEM )
		{ "inputsystem.dll",		INPUTSYSTEM_INTERFACE_VERSION },
#endif
#if defined( USE_VGUI )
		{ "vgui2.dll",				VGUI_IVGUI_INTERFACE_VERSION },
		{ "vguimatsurface.dll",		VGUI_SURFACE_INTERFACE_VERSION },
#endif
#if defined( USE_STUDIORENDER )
		{ "vphysics.dll",			VPHYSICS_INTERFACE_VERSION },
#endif
		{ "", "" }
	};

	MathLib_Init( 2.2f, 2.2f, 0.0f, 2.0f );

	SpewOutputFunc( g_DefaultSpewFunc );

#if defined( USE_FILESYSTEM )
	// Add in the cvar factory
	AppModule_t cvarModule = LoadModule( VStdLib_GetICVarFactory() );
	AddSystem( cvarModule, VENGINE_CVAR_INTERFACE_VERSION );
#endif

#if defined( _X360 )
	// vxconsole - true will block (legacy behavior)
	XBX_InitConsoleMonitor( false );
#endif

	if ( !AddSystems( appSystems ) ) 
		return false;

#if defined( USE_FILESYSTEM )
	m_pFileSystem = (IFileSystem*)FindSystem( FILESYSTEM_INTERFACE_VERSION );
	if ( !m_pFileSystem )
	{
		Error( "Failed to find %s\n", FILESYSTEM_INTERFACE_VERSION );
		return false;
	}
#endif

#if defined( USE_VPHYSICS )
	m_pCollision = (IPhysicsCollision*)FindSystem( VPHYSICS_COLLISION_INTERFACE_VERSION );
	if ( !m_pCollision )
	{
		Error( "Failed to find %s\n", VPHYSICS_COLLISION_INTERFACE_VERSION );
		return false;
	}
#endif

#if defined( USE_MATERIALSYSTEM )
	m_pMaterialSystem = (IMaterialSystem*)FindSystem( MATERIAL_SYSTEM_INTERFACE_VERSION );
	if ( !m_pMaterialSystem )
	{
		Error( "Failed to find %s\n", MATERIAL_SYSTEM_INTERFACE_VERSION );
		return false;
	}
#if defined( _X360 )
	m_pFileSystem->LoadModule( "shaderapidx9.dll" );
#endif
	m_pMaterialSystem->SetShaderAPI( "shaderapidx9.dll" );
#endif

	return true;
}

//-----------------------------------------------------------------------------
// PreInit
//-----------------------------------------------------------------------------
bool CTest360App::PreInit()
{
#if defined( USE_FILESYSTEM )
	// Add paths...
	if ( !SetupSearchPaths( NULL, false, true ) )
	{
		Error( "Failed to setup search paths\n" );
		return false;
	}
#endif

	CreateInterfaceFn factory = GetFactory();
	ConnectTier1Libraries( &factory, 1 );
	ConnectTier2Libraries( &factory, 1 );
	ConnectTier3Libraries( &factory, 1 );

	// Create the main program window and our viewport
	int w = 640;
	int h = 480;
	if ( IsX360() )
	{
		w = GetSystemMetrics( SM_CXSCREEN );
		h = GetSystemMetrics( SM_CYSCREEN );
	}
    if ( !CreateMainWindow( w, h, false ) )
	{
        ChangeDisplaySettings( 0, 0 );
        Error( "Unable to create main window\n" );
        return false;
	}

	ShowWindow( m_hWnd, SW_SHOWNORMAL );
	UpdateWindow( m_hWnd );
	SetForegroundWindow( m_hWnd );
	SetFocus( m_hWnd );

	return true;
}

//-----------------------------------------------------------------------------
// Destroy
//-----------------------------------------------------------------------------
void CTest360App::Destroy()
{
}

//-----------------------------------------------------------------------------
// Purpose: Setup all our VGUI info
//-----------------------------------------------------------------------------
#if defined( USE_VGUI )

static CreateInterfaceFn s_pFactoryList[3];

void *VGuiFactory( const char *pName, int *pReturnCode )
{
	for ( int i = 0; i < ARRAYSIZE( s_pFactoryList ); ++i )
	{
		void *pInterface = s_pFactoryList[i]( pName, pReturnCode );
		if ( pInterface )
			return pInterface;
	}
	return NULL;
}

int CTest360App::InitializeVGUI( void )
{
	s_pFactoryList[0] = Sys_GetFactory( m_pFileSystem->LoadModule( "filesystem_stdio" ) );
	s_pFactoryList[1] = Sys_GetFactory( m_pFileSystem->LoadModule( "vguimatsurface" ) );
	s_pFactoryList[2] = Sys_GetFactory( m_pFileSystem->LoadModule( "vgui2" ) );
	int factorycount = ARRAYSIZE( s_pFactoryList );

	if ( !vgui::VGui_InitInterfacesList( "test360", s_pFactoryList, factorycount ) )
		return 3;

	vgui::ivgui()->Connect( VGuiFactory );
	vgui::ivgui()->Init();

	vgui::ivgui()->SetSleep(false);

	// Init the surface
	vgui::Panel *pPanel = new vgui::Panel( NULL, "TopPanel" );
	pPanel->SetBounds( 0, 0, m_nWidth, m_nHeight );
	pPanel->SetPaintBackgroundEnabled( false );
	pPanel->SetVisible(true);

	vgui::surface()->SetEmbeddedPanel(pPanel->GetVPanel());

	// load the scheme
	vgui::scheme()->LoadSchemeFromFile( "resource/clientscheme.res", NULL );

	// localization
	//vgui::localize()->AddFile( "resource/vgui_%language%.txt" );

	// Start vgui
	vgui::ivgui()->Start();

	// add a panel
	m_pMainPanel = new vgui::Panel( pPanel, "MainPanel" );
	SETUP_PANEL( m_pMainPanel );
	m_pMainPanel->SetBounds( 30, 30, 200, 100 );
	m_pMainPanel->SetBgColor( Color(255,255,0,255) );

	// add text
	vgui::Label *pLabel = new vgui::Label( m_pMainPanel, "Text", L"" );
	SETUP_PANEL( pLabel );
	
	vgui::HScheme scheme = vgui::scheme()->GetScheme( "ClientScheme" );
//	vgui::HFont hFont = vgui::scheme()->GetIScheme(scheme)->GetFont( "BudgetLabel" );
//	pLabel->SetFont( hFont );
	pLabel->SetText( L"This is text" );

	pLabel->SetFgColor( Color(0,0,0,255) );

	return 0;
}
#endif

//-----------------------------------------------------------------------------
// Purpose: Stop VGUI
//-----------------------------------------------------------------------------
#if defined( USE_VGUI )
void CTest360App::ShutdownVGUI( void )
{
	delete m_pMainPanel;

	// Shutdown
	vgui::surface()->Shutdown();
}
#endif

//-----------------------------------------------------------------------------
// Main
//-----------------------------------------------------------------------------
int CTest360App::Main()
{   
#if defined( USE_MATERIALSYSTEM )
	if ( !SetupMaterialSystem() )
	{
		return 0;
	}
#endif

#if defined( USE_STUDIORENDER )
	if ( !SetupStudioRender() )
	{
		return 0;
	}
#endif

#if defined( USE_VGUI )
	int ret = InitializeVGUI();
	if ( ret != 0 )
		return ret;
#endif

	const char *pArgVal;
	const char* pModelName = "models\\alyx.mdl";
	if ( CommandLine()->CheckParm( "-model", &pArgVal ) )
	{
		pModelName = pArgVal;
	}

#if defined( USE_STUDIORENDER )
	// the easiest model to load - no anims
	//const char* pModelName = "models\\items\\item_item_crate.mdl";
	if ( !LoadModel( pModelName ) )
	{
		return 0;
	}
#endif

	MSG msg;
    while ( g_bActive == TRUE )
	{
        while ( PeekMessage( &msg, NULL, 0, 0, PM_REMOVE ) )
		{
            TranslateMessage( &msg );
            DispatchMessage( &msg );
		}

#if defined( USE_INPUTSYSTEM )
		g_pInputSystem->PollInputState();
#endif
		RenderScene();
	}

#if defined( USE_VGUI )
	ShutdownVGUI();
#endif

	return 0;
}
