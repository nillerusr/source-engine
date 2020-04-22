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
// Material editor
//=============================================================================

#include <windows.h>
#include "appframework/tier2app.h"
#include "shaderapi/ishaderdevice.h"
#include "shaderapi/ishaderutil.h"
#include "shaderapi/ishaderapi.h"
#include "materialsystem/materialsystem_config.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "vstdlib/random.h"
#include "filesystem.h"
#include "filesystem_init.h"
#include "tier0/icommandline.h"
#include "tier1/KeyValues.h"
#include "tier1/utlbuffer.h"
#include "tier1/lzmadecoder.h"
#include "materialsystem/imesh.h"
#include "materialsystem/shader_vcs_version.h"
#include "../utils/bzip2/bzlib.h"


class CShaderUtilTemp : public CBaseAppSystem< IShaderUtil >
{
public:
	// Method to allow clients access to the MaterialSystem_Config
	virtual MaterialSystem_Config_t& GetConfig() 
	{
		static MaterialSystem_Config_t config;
		return config;
	}

	// Allows us to convert image formats
	virtual bool ConvertImageFormat( unsigned char *src, enum ImageFormat srcImageFormat,
		unsigned char *dst, enum ImageFormat dstImageFormat, 
		int width, int height, int srcStride = 0, int dstStride = 0 ) 
	{
		return true;
	}

	// Figures out the amount of memory needed by a bitmap
	virtual int GetMemRequired( int width, int height, int depth, ImageFormat format, bool mipmap )
	{
		return 0;
	}

	// Gets image format info
	virtual const ImageFormatInfo_t& ImageFormatInfo( ImageFormat fmt ) const
	{
		static ImageFormatInfo_t info;
		return info;
	}

	// Allows us to set the default shadow state
	virtual void SetDefaultShadowState() { }

	// Allows us to set the default shader state
	virtual void SetDefaultState( ) { }

	// Bind standard textures
	virtual void BindStandardTexture( Sampler_t stage, StandardTextureId_t id ) { }
	virtual void BindStandardVertexTexture( VertexTextureSampler_t stage, StandardTextureId_t id ) { }
	virtual void GetStandardTextureDimensions( int *pWidth, int *pHeight, StandardTextureId_t id ) { *pWidth = *pHeight = 0; }

	// What are the lightmap dimensions?
	virtual void GetLightmapDimensions( int *w, int *h ) { *w = *h = 0; }

	// These methods are called when the shader must eject + restore HW memory
	virtual void ReleaseShaderObjects() {}
	virtual void RestoreShaderObjects( CreateInterfaceFn shaderFactory, int nChangeFlags = 0 ) {}

	// Used to prevent meshes from drawing.
	virtual bool IsInStubMode() { return false; }
	virtual bool InFlashlightMode() const { return false; }

	// For the shader API to shove the current version of aniso level into the
	// "definitive" place (g_config) when the shader API decides to change it.
	// Eventually, we should have a better system of who owns the definitive
	// versions of config vars.
	virtual void NoteAnisotropicLevel( int currentLevel ) {}

	// NOTE: Stuff after this is added after shipping HL2.

	// Are we rendering through the editor?
	virtual bool InEditorMode() const { return false; }

	// Gets the bound morph's vertex format; returns 0 if no morph is bound
	virtual MorphFormat_t GetBoundMorphFormat() { return 0; }

	virtual ITexture *GetRenderTargetEx( int nRenderTargetID ) { return 0; }

	// Tells the material system to draw a buffer clearing quad
	virtual void DrawClearBufferQuad( unsigned char r, unsigned char g, unsigned char b, unsigned char a, bool bClearColor, bool bClearAlpha, bool bClearDepth ) OVERRIDE {}

#if defined( _X360 )
	virtual void ReadBackBuffer( Rect_t *pSrcRect, Rect_t *pDstRect, unsigned char *pData, ImageFormat dstFormat, int nDstStride ) {}
#endif

	// Calls from meshes to material system to handle queing/threading
	virtual bool OnDrawMesh( IMesh *pMesh, int firstIndex, int numIndices ) { return false; }
	virtual bool OnDrawMesh( IMesh *pMesh, CPrimList *pLists, int nLists ) { return false; }
	virtual bool OnSetFlexMesh( IMesh *pStaticMesh, IMesh *pMesh, int nVertexOffsetInBytes ) { return false; }
	virtual bool OnSetColorMesh( IMesh *pStaticMesh, IMesh *pMesh, int nVertexOffsetInBytes ) { return false; }
	virtual bool OnSetPrimitiveType( IMesh *pMesh, MaterialPrimitiveType_t type ) { return false; }
	virtual bool OnFlushBufferedPrimitives() { return false; }


	virtual void SyncMatrices() {}
	virtual void SyncMatrix( MaterialMatrixMode_t ) {}
	virtual int MaxHWMorphBatchCount() const { return 0; }

	virtual void GetCurrentColorCorrection( ShaderColorCorrectionInfo_t* pInfo )
	{
		pInfo->m_bIsEnabled = false;
		pInfo->m_nLookupCount = 0;
		pInfo->m_flDefaultWeight = 0.0f;
	}
	virtual void OnThreadEvent( uint32 threadEvent ) {}

	ShaderAPITextureHandle_t GetShaderAPITextureBindHandle( ITexture *pTexture, int nFrame, int nTextureChannel ) { return 0; }

 	// Remove any materials from memory that aren't in use as determined
 	// by the IMaterial's reference count.
 	virtual void UncacheUnusedMaterials( bool bRecomputeStateSnapshots = false ) {}

	virtual MaterialThreadMode_t			GetThreadMode( ) { return MATERIAL_SINGLE_THREADED; }
	virtual bool							IsRenderThreadSafe( ) { return true; }
};


static CShaderUtilTemp g_pTemp;

static IShaderDeviceMgr *g_pShaderDeviceMgr;

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CShaderUtilTemp, IShaderUtil, 
	SHADER_UTIL_INTERFACE_VERSION, g_pTemp )

//-----------------------------------------------------------------------------
// Purpose: Warning/Msg call back through this API
// Input  : type - 
//			*pMsg - 
// Output : SpewRetval_t
//-----------------------------------------------------------------------------
SpewRetval_t SpewFunc( SpewType_t type, const char *pMsg )
{
	if ( Plat_IsInDebugSession() )
	{
		OutputDebugString( pMsg );
		if ( type == SPEW_ASSERT )
			return SPEW_DEBUGGER;
	}
	return SPEW_CONTINUE;
}


//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
class CShaderAPITestApp : public CTier2SteamApp
{
	typedef CTier2SteamApp BaseClass;

public:
	// Methods of IApplication
	virtual bool Create();
	virtual bool PreInit( );
	virtual int Main();
	virtual void PostShutdown( );
	virtual void Destroy();
	virtual const char *GetAppName() { return "InputTest"; }
	virtual bool AppUsesReadPixels() { return false; }

private:
	// Window management
	bool CreateAppWindow( const char *pTitle, bool bWindowed, int w, int h );

	// Sets up the game path
	bool SetupSearchPaths();

	// Waits for a keypress
	bool WaitForKeypress();

	// Displays information about all adapters
	void DisplayAdapterInfo();

	// Sets the video mode
	bool SetMode();

	// Creates really simple vertex + index buffers
	void CreateSimpleBuffers( ShaderBufferType_t nVBType, ShaderBufferType_t nIBType, bool bBuffered );

	// Destroys the buffers
	void DestroyBuffers();

	// Creates shaders
	void CreateShaders( const char *pVShader, int nVBufLen, const char *pGShader, int nGBufLen, const char *pPShader, int nPBufLen );

	// Destroys the buffers
	void DestroyShaders();

	// DrawUsingShaders
	void TestColoredQuad( ShaderBufferType_t nVBType, ShaderBufferType_t nIBType, bool bBuffered );

	// Tests dynamic buffers
	void TestDynamicBuffers();

	bool CreateDynamicCombos_Ver5( uint8 *pComboBuffer, bool bVertexShader );
	void LoadShaderFile( const char *pName, bool bVertexShader );

	HWND m_HWnd;
	IShaderAPI *m_pShaderAPI;
	IShaderDevice *m_pShaderDevice;

	IIndexBuffer *m_pIndexBuffer;
	IVertexBuffer *m_pVertexBuffer;

	VertexShaderHandle_t m_hVertexShader;
	GeometryShaderHandle_t m_hGeometryShader;
	PixelShaderHandle_t m_hPixelShader;
};

DEFINE_WINDOWED_STEAM_APPLICATION_OBJECT( CShaderAPITestApp );


//-----------------------------------------------------------------------------
// Create all singleton systems
//-----------------------------------------------------------------------------
bool CShaderAPITestApp::Create()
{
	SpewOutputFunc( SpewFunc );

	bool bIsVistaOrHigher = false;

	OSVERSIONINFO info;
	info.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	if ( GetVersionEx( &info ) )
	{
		bIsVistaOrHigher = info.dwMajorVersion >= 6;
	}

	const char *pShaderDLL = CommandLine()->ParmValue( "-shaderdll" );
	if ( !pShaderDLL )
	{
		pShaderDLL = "shaderapidx10.dll";
	}

	if ( !bIsVistaOrHigher && !Q_stricmp( pShaderDLL, "shaderapidx10.dll" ) )
	{
		pShaderDLL = "shaderapidx9.dll";
	}

	AppModule_t module = LoadModule( pShaderDLL );
	if ( module == APP_MODULE_INVALID )
	{
		if ( module == APP_MODULE_INVALID )
		{
			pShaderDLL = "shaderapidx9.dll";
			module = LoadModule( pShaderDLL );
			if ( module == APP_MODULE_INVALID )
			{
				pShaderDLL = "shaderapiempty.dll";
				module = LoadModule( pShaderDLL );
				if ( module == APP_MODULE_INVALID )
					return false;
			}
		}
	}
	
	g_pShaderDeviceMgr = (IShaderDeviceMgr*)AddSystem( module, SHADER_DEVICE_MGR_INTERFACE_VERSION );

	// So that shaderapi can get ahold of our bogus IShaderUtil
	module = LoadModule( Sys_GetFactoryThis() );
	AddSystem( module, SHADER_UTIL_INTERFACE_VERSION );

	return ( g_pShaderDeviceMgr != NULL );
}

void CShaderAPITestApp::Destroy()
{
}


//-----------------------------------------------------------------------------
// Window callback
//-----------------------------------------------------------------------------
static LRESULT CALLBACK ShaderAPITestWndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	switch( message )
	{
	case WM_DESTROY:
		PostQuitMessage( 0 );
		break;

	default:
		return DefWindowProc( hWnd, message, wParam, lParam );
	}

	return 0;
}


//-----------------------------------------------------------------------------
// Window management
//-----------------------------------------------------------------------------
bool CShaderAPITestApp::CreateAppWindow( const char *pTitle, bool bWindowed, int w, int h )
{
	WNDCLASSEX		wc;
	memset( &wc, 0, sizeof( wc ) );
	wc.cbSize		 = sizeof( wc );
    wc.style         = CS_OWNDC | CS_DBLCLKS;
    wc.lpfnWndProc   = ShaderAPITestWndProc;
    wc.hInstance     = (HINSTANCE)GetAppInstance();
    wc.lpszClassName = "Valve001";
	wc.hIcon		 = NULL; //LoadIcon( s_HInstance, MAKEINTRESOURCE( IDI_LAUNCHER ) );
	wc.hIconSm		 = wc.hIcon;

    RegisterClassEx( &wc );

	// Note, it's hidden
	DWORD style = WS_POPUP | WS_CLIPSIBLINGS;
	
	if ( bWindowed )
	{
		// Give it a frame
		style |= WS_OVERLAPPEDWINDOW;
		style &= ~WS_THICKFRAME;
	}

	// Never a max box
	style &= ~WS_MAXIMIZEBOX;

	RECT windowRect;
	windowRect.top		= 0;
	windowRect.left		= 0;
	windowRect.right	= w;
	windowRect.bottom	= h;

	// Compute rect needed for that size client area based on window style
	AdjustWindowRectEx(&windowRect, style, FALSE, 0);

	// Create the window
	m_HWnd = CreateWindow( wc.lpszClassName, pTitle, style, 0, 0, 
		windowRect.right - windowRect.left, windowRect.bottom - windowRect.top, 
		NULL, NULL, (HINSTANCE)GetAppInstance(), NULL );

	if (!m_HWnd)
		return false;

    int     CenterX, CenterY;

	CenterX = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
	CenterY = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;
	CenterX = (CenterX < 0) ? 0: CenterX;
	CenterY = (CenterY < 0) ? 0: CenterY;

	// In VCR modes, keep it in the upper left so mouse coordinates are always relative to the window.
	SetWindowPos (m_HWnd, NULL, CenterX, CenterY, 0, 0,
				  SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW | SWP_DRAWFRAME);

	return true;
}


//-----------------------------------------------------------------------------
// Sets up the game path
//-----------------------------------------------------------------------------
bool CShaderAPITestApp::SetupSearchPaths()
{
	if ( !BaseClass::SetupSearchPaths( NULL, false, true ) )
		return false;

	g_pFullFileSystem->AddSearchPath( GetGameInfoPath(), "SKIN", PATH_ADD_TO_HEAD );
	return true;
}


//-----------------------------------------------------------------------------
// PreInit, PostShutdown
//-----------------------------------------------------------------------------
bool CShaderAPITestApp::PreInit( )
{
	if ( !BaseClass::PreInit() )
		return false;

	if (!g_pFullFileSystem || !g_pShaderDeviceMgr )
		return false;

	// Add paths...
	if ( !SetupSearchPaths() )
		return false;

	const char *pArg;
	int iWidth = 1024;
	int iHeight = 768;
	bool bWindowed = (CommandLine()->CheckParm( "-fullscreen" ) == NULL);
	if (CommandLine()->CheckParm( "-width", &pArg ))
	{
		iWidth = atoi( pArg );
	}
	if (CommandLine()->CheckParm( "-height", &pArg ))
	{
		iHeight = atoi( pArg );
	}

	if (!CreateAppWindow( "Press a Key To Continue", bWindowed, iWidth, iHeight ))
		return false;

	return true;
}

void CShaderAPITestApp::PostShutdown( )
{
	BaseClass::PostShutdown();
}


//-----------------------------------------------------------------------------
// Waits for a keypress
//-----------------------------------------------------------------------------
bool CShaderAPITestApp::WaitForKeypress()
{
	MSG msg = {0};
	while( WM_QUIT != msg.message )
	{
		if( PeekMessage( &msg, NULL, 0, 0, PM_REMOVE ) )
		{
			TranslateMessage( &msg );
			DispatchMessage( &msg );
		}

		if ( msg.message == WM_KEYDOWN )
			return true;
	}
	return false;
}


//-----------------------------------------------------------------------------
// Displays adapter information
//-----------------------------------------------------------------------------
void CShaderAPITestApp::DisplayAdapterInfo()
{
	int nAdapterCount = g_pShaderDeviceMgr->GetAdapterCount();
	for ( int i = 0; i < nAdapterCount; ++i )
	{
		MaterialAdapterInfo_t info;
		g_pShaderDeviceMgr->GetAdapterInfo( i, info );

		Msg( "Adapter %d\n", i );
		Msg( "\tName: %s\n\tVendor: 0x%X\n\tDevice: 0x%X\n\tSubSystem: 0x%X\n\tRevision: 0x%X\n\tRecommended DX Level: %d\n\tMax DX Level: %d\n", 
			info.m_pDriverName, info.m_VendorID, info.m_DeviceID, info.m_SubSysID, info.m_Revision, info.m_nDXSupportLevel, info.m_nMaxDXSupportLevel );

		CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );
		KeyValues *pConfiguration = new KeyValues( "Config" );
		g_pShaderDeviceMgr->GetRecommendedConfigurationInfo( i, info.m_nDXSupportLevel, pConfiguration );
		pConfiguration->RecursiveSaveToFile( buf, 1 );
		Msg( "\tConfiguration:\n%s", ( const char * )buf.Base() );
		Msg( "\n" );

		int nModeCount = g_pShaderDeviceMgr->GetModeCount( i );
		Msg( "\tMode Count : %d\n", nModeCount );
		for ( int j = 0; j < nModeCount; ++j )
		{
			ShaderDisplayMode_t mode;
			g_pShaderDeviceMgr->GetModeInfo( &mode, i, j );
			Msg( "\t\tH: %5d W: %5d Format: %3d Refresh %3d/%3d\n", 
				mode.m_nWidth, mode.m_nHeight, mode.m_Format, mode.m_nRefreshRateNumerator, mode.m_nRefreshRateDenominator );
		}
	}
}


//-----------------------------------------------------------------------------
// Sets the video mode
//-----------------------------------------------------------------------------
bool CShaderAPITestApp::SetMode()
{
	int nAdapterCount = g_pShaderDeviceMgr->GetAdapterCount();
	int nAdapter = CommandLine()->ParmValue( "-adapter", 0 );
	if ( nAdapter >= nAdapterCount )
	{
		Warning( "Specified too high an adapter number on the command-line (%d/%d)!\n", nAdapter, nAdapterCount );
		return false;
	}

	ShaderDeviceInfo_t mode;
	mode.m_DisplayMode.m_nWidth = 1024;
	mode.m_DisplayMode.m_nHeight = 768;
	mode.m_DisplayMode.m_Format = IMAGE_FORMAT_BGRA8888;
	mode.m_DisplayMode.m_nRefreshRateNumerator = 60;
	mode.m_DisplayMode.m_nRefreshRateDenominator = 1;
	mode.m_bWindowed = true;
	mode.m_nBackBufferCount = 1;

	CreateInterfaceFn shaderFactory = g_pShaderDeviceMgr->SetMode( m_HWnd, nAdapter, mode );
	if ( !shaderFactory )
	{
		Warning( "Unable to set mode!\n" );
		return false;
	}

	m_pShaderAPI = (IShaderAPI*)shaderFactory( SHADERAPI_INTERFACE_VERSION, NULL );
	m_pShaderDevice = (IShaderDevice*)shaderFactory( SHADER_DEVICE_INTERFACE_VERSION, NULL );
	if ( !m_pShaderAPI || !m_pShaderDevice )
	{
		Warning( "Unable to get IShaderAPI or IShaderDevice interface!\n" );
		return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Creates really simple vertex + index buffers
//-----------------------------------------------------------------------------
void CShaderAPITestApp::CreateSimpleBuffers( ShaderBufferType_t nVBType, ShaderBufferType_t nIBType, bool bBuffered )
{
	VertexFormat_t fmt = VERTEX_POSITION | VERTEX_NORMAL | VERTEX_COLOR;
	if ( IsDynamicBufferType( nVBType ) )
	{
		m_pVertexBuffer = m_pShaderDevice->CreateVertexBuffer( 
			nVBType, VERTEX_FORMAT_UNKNOWN, 1024, "test" );
	}
	else
	{
		m_pVertexBuffer = m_pShaderDevice->CreateVertexBuffer( 
			nVBType, fmt, 4, "test" );
	}

	static unsigned char s_pColors[4][4] = 
	{
		{ 255,   0,   0, 255 },
		{   0, 255,   0, 255 },
		{   0,   0, 255, 255 },
		{ 255, 255, 255, 255 },
	};

	static int nCount = 0;

	CVertexBuilder vb( m_pVertexBuffer, fmt );
	vb.Lock( 4 );

	vb.Position3f( -1.0f, -1.0f, 0.5f );
	vb.Normal3f( 0.0f, 0.0f, 1.0f );
	vb.Color4ubv( s_pColors[nCount++ % 4] );
	vb.AdvanceVertex();

	vb.Position3f(  1.0f, -1.0f, 0.5f );
	vb.Normal3f( 0.0f, 0.0f, 1.0f );
	vb.Color4ubv( s_pColors[nCount++ % 4] );
	vb.AdvanceVertex();

	vb.Position3f(  1.0f,  1.0f, 0.5f );
	vb.Normal3f( 0.0f, 0.0f, 1.0f );
	vb.Color4ubv( s_pColors[nCount++ % 4] );
	vb.AdvanceVertex();

	vb.Position3f( -1.0f,  1.0f, 0.5f );
	vb.Normal3f( 0.0f, 0.0f, 1.0f );
	vb.Color4ubv( s_pColors[nCount++ % 4] );
	vb.AdvanceVertex();

	vb.SpewData( );
	vb.Unlock( );

	++nCount;

	if ( IsDynamicBufferType( nIBType ) )
	{
		m_pIndexBuffer = m_pShaderDevice->CreateIndexBuffer( nIBType, MATERIAL_INDEX_FORMAT_UNKNOWN, 64, "test" );
	}
	else
	{
		m_pIndexBuffer = m_pShaderDevice->CreateIndexBuffer( nIBType, MATERIAL_INDEX_FORMAT_16BIT, 6, "test" );
	}
	CIndexBuilder ib( m_pIndexBuffer, MATERIAL_INDEX_FORMAT_16BIT );

	ib.Lock( 6, 0 );
	ib.FastIndex( 0 );
	ib.FastIndex( 2 );
	ib.FastIndex( 1 );
	ib.FastIndex( 0 );
	ib.FastIndex( 3 );
	ib.FastIndex( 2 );
	ib.SpewData();
	ib.Unlock( );

	m_pShaderAPI->BindVertexBuffer( 0, m_pVertexBuffer, vb.Offset(), 0, vb.TotalVertexCount(), fmt );
	m_pShaderAPI->BindIndexBuffer( m_pIndexBuffer, ib.Offset() );
}


//-----------------------------------------------------------------------------
// Destroys the buffers
//-----------------------------------------------------------------------------
void CShaderAPITestApp::DestroyBuffers()
{
	if ( m_pVertexBuffer )
	{
		m_pShaderDevice->DestroyVertexBuffer( m_pVertexBuffer );
		m_pVertexBuffer = NULL;
	}

	if ( m_pIndexBuffer )
	{
		m_pShaderDevice->DestroyIndexBuffer( m_pIndexBuffer );
		m_pIndexBuffer = NULL;
	}
}


//-----------------------------------------------------------------------------
// shader programs
//-----------------------------------------------------------------------------
static const char s_pSimpleVertexShader[] = 
	"struct VS_INPUT															"
	"{																			"
	"	float3 vPos						: POSITION0;							"
	"	float4 vColor					: COLOR0;								"
	"};																			"
	"																			"
	"struct VS_OUTPUT															"
	"{																			"
	"	float4 projPos					: POSITION0;							"
	"	float4 vertexColor				: COLOR0;								"
	"};																			"
	"																			"
	"VS_OUTPUT main( const VS_INPUT v )											"
	"{																			"
	"	VS_OUTPUT o = ( VS_OUTPUT )0;											"
	"																			"
	"	o.projPos.xyz = v.vPos;													"
	"	o.projPos.w = 1.0f;														"
	"	o.vertexColor = v.vColor;												"
	"	return o;																"
	"}																			"
	"";

static const char s_pSimplePixelShader[] = 
	"struct PS_INPUT															"
	"{																			"
	"	float4 projPos					: POSITION0;							"
	"	float4 vColor					: COLOR0;								"
	"};																			"
	"																			"
	"float4 main( const PS_INPUT i ) : COLOR									"
	"{																			"
	"	return i.vColor;														"
	"}																			"
	"";


//-----------------------------------------------------------------------------
// Create, destroy shaders
//-----------------------------------------------------------------------------
void CShaderAPITestApp::CreateShaders( const char *pVShader, int nVBufLen, const char *pGShader, int nGBufLen, const char *pPShader, int nPBufLen )
{
	const char *pVertexShaderVersion = g_pMaterialSystemHardwareConfig->GetDXSupportLevel() == 100 ? "vs_4_0" : "vs_2_0";
	const char *pPixelShaderVersion = g_pMaterialSystemHardwareConfig->GetDXSupportLevel() == 100 ? "ps_4_0" : "ps_2_0";

	// Compile shaders
	m_hVertexShader = m_pShaderDevice->CreateVertexShader( pVShader, nVBufLen, pVertexShaderVersion );
	Assert( m_hVertexShader != VERTEX_SHADER_HANDLE_INVALID );

	m_hGeometryShader = GEOMETRY_SHADER_HANDLE_INVALID;
	if ( g_pMaterialSystemHardwareConfig->GetDXSupportLevel() >= 100 )
	{
		//		m_hGeometryShader = m_pShaderDevice->CreateGeometryShader( pGShader, nGBufLen, "gs_4_0" );
		//		Assert( m_hGeometryShader != GEOMETRY_SHADER_HANDLE_INVALID );
	}

	m_hPixelShader = m_pShaderDevice->CreatePixelShader( pPShader, nPBufLen, pPixelShaderVersion );
	Assert( m_hPixelShader != PIXEL_SHADER_HANDLE_INVALID );

	m_pShaderAPI->BindVertexShader( m_hVertexShader );
	m_pShaderAPI->BindGeometryShader( m_hGeometryShader );
	m_pShaderAPI->BindPixelShader( m_hPixelShader );
}

void CShaderAPITestApp::DestroyShaders()
{
	m_pShaderDevice->DestroyVertexShader( m_hVertexShader );
	m_pShaderDevice->DestroyGeometryShader( m_hGeometryShader );
	m_pShaderDevice->DestroyPixelShader( m_hPixelShader );

	m_hVertexShader = VERTEX_SHADER_HANDLE_INVALID;
	m_hGeometryShader = GEOMETRY_SHADER_HANDLE_INVALID;
	m_hPixelShader = PIXEL_SHADER_HANDLE_INVALID;
}


//-----------------------------------------------------------------------------
// DrawQuad
//-----------------------------------------------------------------------------
void CShaderAPITestApp::TestColoredQuad( ShaderBufferType_t nVBType, ShaderBufferType_t nIBType, bool bBuffered )
{
	// clear (so that we can make sure that we aren't getting results from the previous quad)
	m_pShaderAPI->ClearColor3ub( RandomInt( 0, 100 ), RandomInt( 0, 100 ), RandomInt( 190, 255 ) );
	m_pShaderAPI->ClearBuffers( true, false, false, -1, -1 );

	CreateSimpleBuffers( nVBType, nIBType, bBuffered );

	// Draw a quad!
	CreateShaders( s_pSimpleVertexShader, sizeof(s_pSimpleVertexShader), 
		NULL, 0, s_pSimplePixelShader, sizeof(s_pSimplePixelShader) );

	m_pShaderAPI->Draw( MATERIAL_TRIANGLES, 0, 6 );
	m_pShaderDevice->Present();

	DestroyShaders();

	DestroyBuffers();
}


//-----------------------------------------------------------------------------
// Tests dynamic buffers
//-----------------------------------------------------------------------------
void CShaderAPITestApp::TestDynamicBuffers()
{
	m_pVertexBuffer = m_pShaderDevice->CreateVertexBuffer( 
		SHADER_BUFFER_TYPE_DYNAMIC, VERTEX_FORMAT_UNKNOWN, 0x100, "test" );
	m_pIndexBuffer = m_pShaderDevice->CreateIndexBuffer( 
		SHADER_BUFFER_TYPE_DYNAMIC, MATERIAL_INDEX_FORMAT_UNKNOWN, 30, "test" );

	CreateShaders( s_pSimpleVertexShader, sizeof(s_pSimpleVertexShader), 
		NULL, 0, s_pSimplePixelShader, sizeof(s_pSimplePixelShader) );

	// clear (so that we can make sure that we aren't getting results from the previous quad)
	m_pShaderAPI->ClearColor3ub( RandomInt( 0, 100 ), RandomInt( 0, 100 ), RandomInt( 190, 255 ) );
	m_pShaderAPI->ClearBuffers( true, false, false, -1, -1 );

	static unsigned char s_pColors[4][4] = 
	{
		{ 255,   0,   0, 255 },
		{   0, 255,   0, 255 },
		{   0,   0, 255, 255 },
		{ 255, 255, 255, 255 },
	};

	static int nCount = 0;

	VertexFormat_t fmt = VERTEX_POSITION | VERTEX_NORMAL | VERTEX_COLOR;
	const int nLoopCount = 8;
	float flWidth = 2.0f / nLoopCount;
	for ( int i = 0; i < nLoopCount; ++i )
	{
		CVertexBuilder vb( m_pVertexBuffer, fmt );
		vb.Lock( 4 );

		vb.Position3f( -1.0f + i * flWidth, -1.0f, 0.5f );
		vb.Normal3f( 0.0f, 0.0f, 1.0f );
		vb.Color4ubv( s_pColors[nCount++ % 4] );
		vb.AdvanceVertex();

		vb.Position3f( -1.0f + i * flWidth + flWidth, -1.0f, 0.5f );
		vb.Normal3f( 0.0f, 0.0f, 1.0f );
		vb.Color4ubv( s_pColors[nCount++ % 4] );
		vb.AdvanceVertex();

		vb.Position3f( -1.0f + i * flWidth + flWidth,  1.0f, 0.5f );
		vb.Normal3f( 0.0f, 0.0f, 1.0f );
		vb.Color4ubv( s_pColors[nCount++ % 4] );
		vb.AdvanceVertex();

		vb.Position3f( -1.0f + i * flWidth,  1.0f, 0.5f );
		vb.Normal3f( 0.0f, 0.0f, 1.0f );
		vb.Color4ubv( s_pColors[nCount++ % 4] );
		vb.AdvanceVertex();
		vb.SpewData();
		vb.Unlock( );

		++nCount;

		CIndexBuilder ib( m_pIndexBuffer, MATERIAL_INDEX_FORMAT_16BIT );

		ib.Lock( 6, vb.GetFirstVertex() );
		ib.FastIndex( 0 );
		ib.FastIndex( 2 );
		ib.FastIndex( 1 );
		ib.FastIndex( 0 );
		ib.FastIndex( 3 );
		ib.FastIndex( 2 );
		ib.SpewData();
		ib.Unlock( );

		m_pShaderAPI->BindVertexBuffer( 0, m_pVertexBuffer, vb.Offset(), vb.GetFirstVertex(), vb.TotalVertexCount(), fmt );
		m_pShaderAPI->BindIndexBuffer( m_pIndexBuffer, ib.Offset() );
		m_pShaderAPI->Draw( MATERIAL_TRIANGLES, ib.GetFirstIndex(), ib.TotalIndexCount() );
	}

	m_pShaderDevice->Present();

	++nCount;

	DestroyShaders();
	DestroyBuffers();
}


//-----------------------------------------------------------------------------
// Create dynamic combos
//-----------------------------------------------------------------------------
static uint32 NextULONG( uint8 * &pData )
{
	// handle unaligned read
	uint32 nRet;
	memcpy( &nRet, pData, sizeof( nRet ) );
	pData += sizeof( nRet );
	return nRet;
}

bool CShaderAPITestApp::CreateDynamicCombos_Ver5( uint8 *pComboBuffer, bool bVertexShader )
{
	uint8 *pCompressedShaders = pComboBuffer;
	uint8 *pUnpackBuffer = new uint8[MAX_SHADER_UNPACKED_BLOCK_SIZE];

	// now, loop through all blocks
	bool bOK = true;
	while ( bOK )
	{
		uint32 nBlockSize = NextULONG( pCompressedShaders );
		if ( nBlockSize == 0xffffffff )	
		{
			// any more blocks?
			break;
		}

		switch( nBlockSize  & 0xc0000000 )
		{
		case 0:											// bzip2
			{
				// uncompress
				uint32 nOutsize = MAX_SHADER_UNPACKED_BLOCK_SIZE;
				int nRslt = BZ2_bzBuffToBuffDecompress( 
					reinterpret_cast<char *>( pUnpackBuffer ),
					&nOutsize,
					reinterpret_cast<char *>( pCompressedShaders ),
					nBlockSize, 1, 0 );
				if ( nRslt < 0 )
				{
					// errors are negative for bzip
					Assert( 0 );
					Warning( "BZIP Error (%d) decompressing shader", nRslt );
					bOK = false;
				}

				pCompressedShaders += nBlockSize;
				nBlockSize = nOutsize;		// how much data there is
			}
			break;

		case 0x80000000:								// uncompressed
			{
				// not compressed, as is
				nBlockSize &= 0x3fffffff;
				memcpy( pUnpackBuffer, pCompressedShaders, nBlockSize );
				pCompressedShaders += nBlockSize;
			}
			break;

		case 0x40000000:								// lzma compressed
			{
				nBlockSize &= 0x3fffffff;

				size_t nOutsize = CLZMA::Uncompress(
					reinterpret_cast<uint8 *>( pCompressedShaders ),
					pUnpackBuffer );
				pCompressedShaders += nBlockSize;
				nBlockSize = nOutsize;		// how much data there is
			}
			break;

		default:
			{
				Assert( 0 );
				Error(" unrecognized shader compression type = file corrupt?");
				bOK = false;
			}
		}

		uint8 *pReadPtr = pUnpackBuffer;
		while ( pReadPtr < pUnpackBuffer+nBlockSize )
		{
			uint32 nCombo_ID = NextULONG( pReadPtr );
			(void)nCombo_ID; // Suppress local variable is initialized but not referenced warning
			uint32 nShaderSize = NextULONG( pReadPtr );

			CUtlBuffer buf( pReadPtr, nShaderSize );
			if ( bVertexShader )
			{
				m_pShaderDevice->CreateVertexShader( buf, "vs_2_0" );
			}
			else
			{
				m_pShaderDevice->CreatePixelShader( buf, "ps_2_b" );
			}

			pReadPtr += nShaderSize;
		}
	}

	delete[] pUnpackBuffer;

	return bOK;
}


//-----------------------------------------------------------------------------
// Load shader
//-----------------------------------------------------------------------------
void CShaderAPITestApp::LoadShaderFile( const char *pName, bool bVertexShader )
{
	// next, try the fxc dir
	char pFileName[MAX_PATH];
	Q_snprintf( pFileName, MAX_PATH, "..\\hl2\\shaders\\fxc\\%s.vcs", pName );

	FileHandle_t hFile = g_pFullFileSystem->Open( pFileName, "rb", "EXECUTABLE_PATH" );
	if ( hFile == FILESYSTEM_INVALID_HANDLE )
	{
		Warning( "Couldn't load %s shader %s\n", bVertexShader ? "vertex" : "pixel", pName );
		return;
	}

	ShaderHeader_t header; 
	g_pFullFileSystem->Read( &header, sizeof( ShaderHeader_t ), hFile );

	// cache the dictionary
	int nComboSize =  header.m_nNumStaticCombos * sizeof( StaticComboRecord_t );
	StaticComboRecord_t *pRecords = (StaticComboRecord_t *)malloc( nComboSize );
	g_pFullFileSystem->Read( pRecords, nComboSize, hFile );

	for ( unsigned int i = 0; i < header.m_nNumStaticCombos - 1; ++i )
	{
		int nStartingOffset = pRecords[i].m_nFileOffset;
		int nEndingOffset = pRecords[i+1].m_nFileOffset;
		int nShaderSize = nEndingOffset - nStartingOffset;

		uint8 *pBuf = (uint8*)malloc( nShaderSize );
		g_pFullFileSystem->Seek( hFile, nStartingOffset, FILESYSTEM_SEEK_HEAD );
		g_pFullFileSystem->Read( pBuf, nShaderSize, hFile );

		CreateDynamicCombos_Ver5( pBuf, bVertexShader ); 
		free( pBuf );

	}

	free( pRecords );
	g_pFullFileSystem->Close( hFile );

}


//-----------------------------------------------------------------------------
// main application
//-----------------------------------------------------------------------------
int CShaderAPITestApp::Main()
{
	DisplayAdapterInfo();
	if ( !SetMode() )
		return 0;

	// Test buffer clearing
	m_pShaderAPI->ClearColor3ub( RandomInt( 0, 100 ), RandomInt( 0, 100 ), RandomInt( 190, 255 ) );
	m_pShaderAPI->ClearBuffers( true, false, false, -1, -1 );
	m_pShaderDevice->Present();

	SetWindowText( m_HWnd, "ClearBuffers test results . . hit a key" );

	if ( !WaitForKeypress() )
		return 1;

	// Test viewport
	int nMaxViewports = g_pMaterialSystemHardwareConfig->MaxViewports();
	ShaderViewport_t* pViewports = ( ShaderViewport_t* )_alloca( nMaxViewports * sizeof(ShaderViewport_t) );
	for ( int i = 0; i < nMaxViewports; ++i )
	{
		int x = RandomInt( 0, 100 );
		int y = RandomInt( 0, 100 );
		int w = RandomInt( 100, 200 );
		int h = RandomInt( 100, 200 );
		pViewports[i].Init( x, y, w, h );

		m_pShaderAPI->SetViewports( i+1, pViewports );
	}

	SetWindowText( m_HWnd, "SetViewports test results . . hit a key" );

	if ( !WaitForKeypress() )
		return 1;

	// Sets up a full-screen viewport
	int w, h;
	m_pShaderDevice->GetWindowSize( w, h );

	ShaderViewport_t viewport;
	viewport.Init( 0, 0, w, h );
	m_pShaderAPI->SetViewports( 1, &viewport );

	// Test drawing a full-screen quad with interpolated vertex colors for every combo of static/dynamic VB, static dynamic IB, buffered/non-buffered.
	char buf[1024];
	for ( int nVBType = 0; nVBType < SHADER_BUFFER_TYPE_COUNT; ++nVBType )
	{
		// FIXME: Remove
		if ( nVBType > SHADER_BUFFER_TYPE_DYNAMIC )
			continue;

		for( int nIBType = 0; nIBType < SHADER_BUFFER_TYPE_COUNT; ++nIBType )
		{
			// FIXME: Remove
			if ( nIBType > SHADER_BUFFER_TYPE_DYNAMIC )
				continue;

			// MESHFIXME: make buffered vertex buffers/index buffers work.
			int nBuffered = 0;
//			for( nBuffered = 0; nBuffered < 2; nBuffered++ )
			{
				TestColoredQuad( (ShaderBufferType_t)nVBType, (ShaderBufferType_t)nIBType, nBuffered != 0 );

				sprintf( buf, "TestColoredQuad results VB: %d IB: %d Buffered: %d HIT A KEY!", 
					nVBType, nIBType, nBuffered != 0 );
				SetWindowText( m_HWnd, buf );

				if ( !WaitForKeypress() )
					return 1;
			}
		}
	}

	SetWindowText( m_HWnd, "Dynamic Buffer Test: HIT A KEY!" );
	TestDynamicBuffers();
	if ( !WaitForKeypress() )
		return 1;

	g_pMaterialSystemHardwareConfig->OverrideStreamOffsetSupport( true, false );

	SetWindowText( m_HWnd, "Dynamic Buffer Test (no stream offset): HIT A KEY!" );
	TestDynamicBuffers();
	if ( !WaitForKeypress() )
		return 1;

	g_pMaterialSystemHardwareConfig->OverrideStreamOffsetSupport( false, false );

	return 1;
}
