//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//
#define DISABLE_PROTECTED_THINGS
#include "locald3dtypes.h"

#include "shaderdevicedx8.h"
#include "shaderapi/ishaderutil.h"
#include "shaderapidx8_global.h"
#include "filesystem.h"
#include "tier0/icommandline.h"
#include "tier2/tier2.h"
#include "shadershadowdx8.h"
#include "colorformatdx8.h"
#include "materialsystem/IShader.h"
#include "shaderapidx8.h"
#include "shaderapidx8_global.h"
#include "imeshdx8.h"
#include "materialsystem/materialsystem_config.h"
#include "vertexshaderdx8.h"
#include "recording.h"
#include "winutils.h"
#include "tier0/vprof_telemetry.h"

#if defined ( DX_TO_GL_ABSTRACTION )
// Placed here so inlines placed in dxabstract.h can access gGL
COpenGLEntryPoints *gGL = NULL;
#endif

#define D3D_BATCH_PERF_ANALYSIS 0

#if D3D_BATCH_PERF_ANALYSIS
#if defined( DX_TO_GL_ABSTRACTION )
#error Cannot enable D3D_BATCH_PERF_ANALYSIS when using DX_TO_GL_ABSTRACTION, use GL_BATCH_PERF_ANALYSIS instead.
#endif
// Define this if you want all d3d9 interfaces hooked and run through the dx9hook.h shim interfaces. For profiling, etc.
#define DO_DX9_HOOK
#endif

#ifdef DO_DX9_HOOK

#if D3D_BATCH_PERF_ANALYSIS
ConVar d3d_batch_vis( "d3d_batch_vis", "0" );
ConVar d3d_batch_vis_abs_scale( "d3d_batch_vis_abs_scale", ".050" );
ConVar d3d_present_vis_abs_scale( "d3d_batch_vis_abs_scale", ".050" );
ConVar d3d_batch_vis_y_scale( "d3d_batch_vis_y_scale", "0.0" );
uint64 g_nTotalD3DCalls, g_nTotalD3DCycles;
static double s_rdtsc_to_ms;
#endif

#include "dx9hook.h"
#endif

#ifndef _X360
#include "wmi.h"
#endif

#if defined( _X360 )
#include "xbox/xbox_console.h"
#include "xbox/xbox_win32stubs.h"
#endif


//#define DX8_COMPATABILITY_MODE

//-----------------------------------------------------------------------------
// Globals
//-----------------------------------------------------------------------------
static CShaderDeviceMgrDx8 g_ShaderDeviceMgrDx8;
CShaderDeviceMgrDx8* g_pShaderDeviceMgrDx8 = &g_ShaderDeviceMgrDx8;

#ifndef SHADERAPIDX10

// In the shaderapidx10.dll, we use its version of IShaderDeviceMgr. 
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CShaderDeviceMgrDx8, IShaderDeviceMgr, 
	SHADER_DEVICE_MGR_INTERFACE_VERSION, g_ShaderDeviceMgrDx8 )

#endif

#if defined( _X360 )
IDirect3D9 *m_pD3D;
#endif

IDirect3DDevice *g_pD3DDevice = NULL;

#if defined(IS_WINDOWS_PC) && defined(SHADERAPIDX9)
// HACK: need to pass knowledge of D3D9Ex usage into callers of D3D Create* methods
// so they do not try to specify D3DPOOL_MANAGED, which is unsupported in D3D9Ex
bool g_ShaderDeviceUsingD3D9Ex = false;
static ConVar mat_supports_d3d9ex( "mat_supports_d3d9ex", "0", FCVAR_HIDDEN );
#endif

// hook into mat_forcedynamic from the engine.
static ConVar mat_forcedynamic( "mat_forcedynamic", "0", FCVAR_CHEAT );

// this is hooked into the engines convar
ConVar mat_debugalttab( "mat_debugalttab", "0", FCVAR_CHEAT );


//-----------------------------------------------------------------------------
//
// Device manager
//
//-----------------------------------------------------------------------------

	
//-----------------------------------------------------------------------------
// constructor, destructor
//-----------------------------------------------------------------------------
CShaderDeviceMgrDx8::CShaderDeviceMgrDx8()
{
	m_pD3D = NULL;
	m_bObeyDxCommandlineOverride = true;
	m_bAdapterInfoIntialized = false;

#if defined( PIX_INSTRUMENTATION ) && defined ( DX_TO_GL_ABSTRACTION ) && defined( _WIN32 )
	m_hD3D9 = NULL;
	m_pBeginEvent = NULL;
	m_pEndEvent = NULL;
	m_pSetMarker = NULL;
	m_pSetOptions = NULL;
#endif	
}

CShaderDeviceMgrDx8::~CShaderDeviceMgrDx8()
{
}

#ifdef OSX
#include <Carbon/Carbon.h>
#endif
//-----------------------------------------------------------------------------
// Connect, disconnect
//-----------------------------------------------------------------------------
bool CShaderDeviceMgrDx8::Connect( CreateInterfaceFn factory )
{
	LOCK_SHADERAPI();

	if ( !BaseClass::Connect( factory ) )
		return false;

#if defined ( DX_TO_GL_ABSTRACTION )
	gGL = ToGLConnectLibraries( factory );
#endif

#if defined(IS_WINDOWS_PC) && defined(SHADERAPIDX9) && !defined(RECORDING) && !defined( DX_TO_GL_ABSTRACTION )
	m_pD3D = NULL;

	// Attempt to create a D3D9Ex device (Windows Vista and later) if possible
	bool bD3D9ExForceDisable = ( CommandLine()->FindParm( "-nod3d9ex" ) != 0 ) ||
								( CommandLine()->ParmValue( "-dxlevel", 95 ) < 90 );

	bool bD3D9ExAvailable = false;
	if ( HMODULE hMod = ::LoadLibraryA( "d3d9.dll" ) )
	{
		typedef HRESULT ( WINAPI *CreateD3D9ExFunc_t )( UINT, IUnknown** );
		if ( CreateD3D9ExFunc_t pfnCreateD3D9Ex = (CreateD3D9ExFunc_t) ::GetProcAddress( hMod, "Direct3DCreate9Ex" ) )
		{
			IUnknown *pD3D9Ex = NULL;
			if ( (*pfnCreateD3D9Ex)( D3D_SDK_VERSION, &pD3D9Ex ) == S_OK && pD3D9Ex )
			{
				bD3D9ExAvailable = true;
				if ( bD3D9ExForceDisable )
				{
					pD3D9Ex->Release();
				}
				else
				{
					g_ShaderDeviceUsingD3D9Ex = true;
					// The following is more "correct" but incompatible with the Steam overlay:
					//pD3D9Ex->QueryInterface( IID_IDirect3D9, (void**) &m_pD3D );
					//pD3D9Ex->Release();
					m_pD3D = static_cast< IDirect3D9* >( pD3D9Ex );
				}
			}
		}
		::FreeLibrary( hMod );
	}

	if ( !m_pD3D )
	{
		g_ShaderDeviceUsingD3D9Ex = false;
		m_pD3D = Direct3DCreate9(D3D_SDK_VERSION);
	}
	
	mat_supports_d3d9ex.SetValue( bD3D9ExAvailable ? 1 : 0 );
#else
	#if defined( DO_DX9_HOOK )
		m_pD3D = Direct3DCreate9Hook(D3D_SDK_VERSION);
	#else
		m_pD3D = Direct3DCreate9(D3D_SDK_VERSION);
	#endif
#endif

	if ( !m_pD3D )
	{
		Warning( "Failed to create D3D9!\n" );
		return false;
	}

#if defined( PIX_INSTRUMENTATION ) && defined ( DX_TO_GL_ABSTRACTION ) && defined( _WIN32 )
	// This is a little odd, but AMD PerfStudio hooks D3D9.DLL and intercepts all of the D3DPERF API's (even for OpenGL apps).
	// So dynamically load d3d9.dll and get the address of these exported functions.
	if ( !m_hD3D9 )
	{
		m_hD3D9 = LoadLibraryA("d3d9.dll");
	}
	if ( m_hD3D9 )
	{
		Plat_DebugString( "PIX_INSTRUMENTATION: Loaded d3d9.dll\n" );
		printf( "PIX_INSTRUMENTATION: Loaded d3d9.dll\n" );

		m_pBeginEvent = (D3DPERF_BeginEvent_FuncPtr)GetProcAddress( m_hD3D9, "D3DPERF_BeginEvent" );
		m_pEndEvent = (D3DPERF_EndEvent_FuncPtr)GetProcAddress( m_hD3D9, "D3DPERF_EndEvent" );
		m_pSetMarker = (D3DPERF_SetMarker_FuncPtr)GetProcAddress( m_hD3D9, "D3DPERF_SetOptions" );
		m_pSetOptions = (D3DPERF_SetOptions_FuncPtr)GetProcAddress( m_hD3D9, "D3DPERF_SetMarker" );
	}
#endif

	// FIXME: Want this to be here, but we can't because Steam
	// hasn't had it's application ID set up yet.

//	InitAdapterInfo();
	return true;
}

void CShaderDeviceMgrDx8::Disconnect()
{
	LOCK_SHADERAPI();

#if defined( PIX_INSTRUMENTATION ) && defined ( DX_TO_GL_ABSTRACTION ) && defined( _WIN32 )
	if ( m_hD3D9 )
	{
		m_pBeginEvent = NULL;
		m_pEndEvent = NULL;
		m_pSetMarker = NULL;
		m_pSetOptions = NULL;

		FreeLibrary( m_hD3D9 );
		m_hD3D9 = NULL;
	}
#endif

	if ( m_pD3D )
	{
		m_pD3D->Release();
		m_pD3D = 0;
	}

#if defined ( DX_TO_GL_ABSTRACTION )
	ToGLDisconnectLibraries();
#endif

	BaseClass::Disconnect();
}



//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------
InitReturnVal_t CShaderDeviceMgrDx8::Init( )
{
	// FIXME: Remove call to InitAdapterInfo once Steam startup issues are resolved.
	// Do it in Connect instead.
	InitAdapterInfo();

	return INIT_OK;
}


//-----------------------------------------------------------------------------
// Shutdown
//-----------------------------------------------------------------------------
void CShaderDeviceMgrDx8::Shutdown( )
{
	LOCK_SHADERAPI();
	
// FIXME: Make PIX work
	
// BeginPIXEvent( PIX_VALVE_ORANGE, "Shutdown" );
	
	if ( g_pShaderAPI )
	{
		g_pShaderAPI->OnDeviceShutdown();
	}
	
	if ( g_pShaderDevice )
	{
		g_pShaderDevice->ShutdownDevice();
		g_pMaterialSystemHardwareConfig = NULL;
	}

//	EndPIXEvent();

	
}



//-----------------------------------------------------------------------------
// Inline methods
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Initialize adapter information
//-----------------------------------------------------------------------------
void CShaderDeviceMgrDx8::InitAdapterInfo()
{
	if ( m_bAdapterInfoIntialized )
		return;

	m_bAdapterInfoIntialized = true;
	m_Adapters.RemoveAll();

	Assert(m_pD3D);
	int nCount = m_pD3D->GetAdapterCount( );
	for( int i = 0; i < nCount; ++i )
	{
		int j = m_Adapters.AddToTail();
		AdapterInfo_t &info = m_Adapters[j];

#ifdef _DEBUG
		memset( &info.m_ActualCaps, 0xDD, sizeof(info.m_ActualCaps) );
#endif

		info.m_ActualCaps.m_bDeviceOk = ComputeCapsFromD3D( &info.m_ActualCaps, i );
		if ( !info.m_ActualCaps.m_bDeviceOk )
			continue;

		ReadDXSupportLevels( info.m_ActualCaps );

		// Read dxsupport.cfg which has config overrides for particular cards.
		ReadHardwareCaps( info.m_ActualCaps, info.m_ActualCaps.m_nMaxDXSupportLevel );

		// What's in "-shader" overrides dxsupport.cfg
		const char *pShaderParam = CommandLine()->ParmValue( "-shader" );
		if ( pShaderParam )
		{
			Q_strncpy( info.m_ActualCaps.m_pShaderDLL, pShaderParam, sizeof( info.m_ActualCaps.m_pShaderDLL ) );
		}
	}
}

//--------------------------------------------------------------------------------
// Code to detect support for texture border color (widely supported but the caps
// bit is messed up in drivers due to a stupid WHQL test that requires this to work
// with float textures which we don't generally care about wrt this address mode)
//--------------------------------------------------------------------------------
void CShaderDeviceMgrDx8::CheckBorderColorSupport( HardwareCaps_t *pCaps, int nAdapter )
{
#ifdef DX_TO_GL_ABSTRACTION
	if( true )
#else
	if( IsX360() )
#endif
	{
		pCaps->m_bSupportsBorderColor = true;
	}
	else // Most PC parts do this, but let's not deal with that yet (JasonM)
	{
		pCaps->m_bSupportsBorderColor = false;
	}
}

//--------------------------------------------------------------------------------
// Vendor-dependent code to detect support for various flavors of shadow mapping
//--------------------------------------------------------------------------------
void CShaderDeviceMgrDx8::CheckVendorDependentShadowMappingSupport( HardwareCaps_t *pCaps, int nAdapter )
{
	// Set a default null texture format...may be overridden below by IHV-specific surface type
	pCaps->m_NullTextureFormat = IMAGE_FORMAT_ARGB8888;
	if ( m_pD3D->CheckDeviceFormat( nAdapter, DX8_DEVTYPE, D3DFMT_X8R8G8B8, D3DUSAGE_RENDERTARGET, D3DRTYPE_TEXTURE, D3DFMT_R5G6B5 ) == S_OK )
	{
		pCaps->m_NullTextureFormat = IMAGE_FORMAT_RGB565;
	}

#if defined( _X360 )
	pCaps->m_ShadowDepthTextureFormat = ReverseDepthOnX360() ? IMAGE_FORMAT_X360_DST24F : IMAGE_FORMAT_X360_DST24;
	pCaps->m_bSupportsShadowDepthTextures = true;
	pCaps->m_bSupportsFetch4 = false;
	return;
#elif defined ( DX_TO_GL_ABSTRACTION )
	// We may want to only do this on the higher-end Mac SKUs, since it's not free...
	pCaps->m_ShadowDepthTextureFormat = IMAGE_FORMAT_NV_DST16; // This format shunts us down the right shader combo path

	pCaps->m_bSupportsShadowDepthTextures = true;

	pCaps->m_bSupportsFetch4 = false;
	return;
#endif

	if ( IsPC() || !IsX360() )
	{
		bool bToolsMode = IsWindows() && ( CommandLine()->CheckParm( "-tools" ) != NULL );
		bool bFound16Bit = false;

		if ( ( pCaps->m_VendorID == VENDORID_NVIDIA ) && ( pCaps->m_SupportsShaderModel_3_0  ) )	// ps_3_0 parts from nVidia
		{
			// First, test for null texture support
			if ( m_pD3D->CheckDeviceFormat( nAdapter, DX8_DEVTYPE, D3DFMT_X8R8G8B8, D3DUSAGE_RENDERTARGET, D3DRTYPE_TEXTURE, NVFMT_NULL ) == S_OK )
			{
				pCaps->m_NullTextureFormat = IMAGE_FORMAT_NV_NULL;
			}

			//
			// NVIDIA has two no-PCF formats (these are not filtering modes, but surface formats
			//   NVFMT_RAWZ is supported by NV4x (not supported here yet...requires a dp3 to reconstruct in shader code, which doesn't seem to work)
			//   NVFMT_INTZ is supported on newer chips as of G8x (just read like ATI non-fetch4 mode)
			//
/*
			if ( m_pD3D->CheckDeviceFormat( nAdapter, DX8_DEVTYPE, D3DFMT_X8R8G8B8, D3DUSAGE_RENDERTARGET, D3DRTYPE_TEXTURE, NVFMT_INTZ ) == S_OK )
			{
				pCaps->m_ShadowDepthTextureFormat = IMAGE_FORMAT_NV_INTZ;
				pCaps->m_bSupportsFetch4 = false;
				pCaps->m_bSupportsShadowDepthTextures = true;
				return;
			}
*/
			if ( m_pD3D->CheckDeviceFormat( nAdapter, DX8_DEVTYPE, D3DFMT_X8R8G8B8, D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_TEXTURE, D3DFMT_D16 ) == S_OK )
			{
				pCaps->m_ShadowDepthTextureFormat = IMAGE_FORMAT_NV_DST16;
				pCaps->m_bSupportsFetch4 = false;
				pCaps->m_bSupportsShadowDepthTextures = true;
				bFound16Bit = true;

				if ( !bToolsMode )	// Tools will continue on and try for 24 bit...
					return;
			}
			
			if ( m_pD3D->CheckDeviceFormat( nAdapter, DX8_DEVTYPE, D3DFMT_X8R8G8B8, D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_TEXTURE, D3DFMT_D24S8 ) == S_OK )
			{
				pCaps->m_ShadowDepthTextureFormat = IMAGE_FORMAT_NV_DST24;
				pCaps->m_bSupportsFetch4 = false;
				pCaps->m_bSupportsShadowDepthTextures = true;
				return;
			}

			if ( bFound16Bit )		// Found 16 bit but not 24
				return;
		}
		else if ( ( pCaps->m_VendorID == VENDORID_ATI ) && pCaps->m_SupportsPixelShaders_2_b )		// ps_2_b parts from ATI
		{
			// Initially, check for Fetch4 (tied to ATIFMT_D24S8 support)
			pCaps->m_bSupportsFetch4 = false;
			if ( m_pD3D->CheckDeviceFormat( nAdapter, DX8_DEVTYPE, D3DFMT_X8R8G8B8, D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_TEXTURE, ATIFMT_D24S8 ) == S_OK )
			{
				pCaps->m_bSupportsFetch4 = true;
			}

			if ( m_pD3D->CheckDeviceFormat( nAdapter, DX8_DEVTYPE, D3DFMT_X8R8G8B8, D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_TEXTURE, ATIFMT_D16 ) == S_OK )	// Prefer 16-bit
			{
				pCaps->m_ShadowDepthTextureFormat = IMAGE_FORMAT_ATI_DST16;
				pCaps->m_bSupportsShadowDepthTextures = true;
				bFound16Bit = true;

				if ( !bToolsMode )	// Tools will continue on and try for 24 bit...
					return;
			}
			
			if ( m_pD3D->CheckDeviceFormat( nAdapter, DX8_DEVTYPE, D3DFMT_X8R8G8B8, D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_TEXTURE, ATIFMT_D24S8 ) == S_OK )
			{
				pCaps->m_ShadowDepthTextureFormat = IMAGE_FORMAT_ATI_DST24;
				pCaps->m_bSupportsShadowDepthTextures = true;
				return;
			}

			if ( bFound16Bit )		// Found 16 bit but not 24
				return;
		}
	}

	// Other vendor or old hardware
	pCaps->m_ShadowDepthTextureFormat = IMAGE_FORMAT_UNKNOWN;
	pCaps->m_bSupportsShadowDepthTextures = false;
	pCaps->m_bSupportsFetch4 = false;
}


//-----------------------------------------------------------------------------
// Vendor-dependent code to detect Alpha To Coverage Backdoors
//-----------------------------------------------------------------------------
void CShaderDeviceMgrDx8::CheckVendorDependentAlphaToCoverage( HardwareCaps_t *pCaps, int nAdapter )
{
	pCaps->m_bSupportsAlphaToCoverage = false;

	// Bail out on OpenGL
#ifdef DX_TO_GL_ABSTRACTION
	pCaps->m_bSupportsAlphaToCoverage	 = true;
	pCaps->m_AlphaToCoverageEnableValue	 = TRUE;
	pCaps->m_AlphaToCoverageDisableValue = FALSE;
	pCaps->m_AlphaToCoverageState		 = D3DRS_ADAPTIVETESS_Y; // Just match the NVIDIA state hackery
	return;
#endif

	if ( pCaps->m_nDXSupportLevel < 90 )
		return;

#ifdef _X360
	{
		pCaps->m_bSupportsAlphaToCoverage	 = true;
		pCaps->m_AlphaToCoverageEnableValue	 = TRUE;
		pCaps->m_AlphaToCoverageDisableValue = FALSE;
		pCaps->m_AlphaToCoverageState		 = D3DRS_ALPHATOMASKENABLE;
		return;
	}
#endif // _X360

	if ( pCaps->m_VendorID == VENDORID_NVIDIA )
	{
		// nVidia has two modes...assume SSAA is superior to MSAA and hence more desirable (though it's probably not)
		//
		// Currently, they only seem to expose any of this on 7800 and up though older parts certainly
		// support at least the MSAA mode since they support it on OpenGL via the arb_multisample extension
		bool bNVIDIA_MSAA = false;
		bool bNVIDIA_SSAA = false;

		if ( m_pD3D->CheckDeviceFormat( nAdapter, DX8_DEVTYPE,					// Check MSAA version
			D3DFMT_X8R8G8B8, 0, D3DRTYPE_SURFACE,
			(D3DFORMAT)MAKEFOURCC('A', 'T', 'O', 'C')) == S_OK )
		{
			bNVIDIA_MSAA = true;
		}

		if ( m_pD3D->CheckDeviceFormat( nAdapter, DX8_DEVTYPE,					// Check SSAA version
			D3DFMT_X8R8G8B8, 0, D3DRTYPE_SURFACE,
			(D3DFORMAT)MAKEFOURCC('S', 'S', 'A', 'A')) == S_OK )
		{
			bNVIDIA_SSAA = true;
		}

		// nVidia pitches SSAA but we prefer ATOC
		if ( bNVIDIA_MSAA )//  || bNVIDIA_SSAA )
		{
			//			if ( bNVIDIA_SSAA )
			//				m_AlphaToCoverageEnableValue  = MAKEFOURCC('S', 'S', 'A', 'A');
			//			else
			pCaps->m_AlphaToCoverageEnableValue	= MAKEFOURCC('A', 'T', 'O', 'C');

			pCaps->m_AlphaToCoverageState = D3DRS_ADAPTIVETESS_Y;
			pCaps->m_AlphaToCoverageDisableValue = (DWORD)D3DFMT_UNKNOWN;
			pCaps->m_bSupportsAlphaToCoverage = true;
			return;
		}
	}
	else if ( pCaps->m_VendorID == VENDORID_ATI )
	{
		// Supported on all ATI parts...just go ahead and set the state when appropriate
		pCaps->m_AlphaToCoverageState		= D3DRS_POINTSIZE;
		pCaps->m_AlphaToCoverageEnableValue	= MAKEFOURCC('A','2','M','1');
		pCaps->m_AlphaToCoverageDisableValue = MAKEFOURCC('A','2','M','0');
		pCaps->m_bSupportsAlphaToCoverage = true;
		return;
	}
}

ConVar mat_hdr_level( "mat_hdr_level", "2", FCVAR_ARCHIVE );
ConVar mat_slopescaledepthbias_shadowmap( "mat_slopescaledepthbias_shadowmap", "16", FCVAR_CHEAT );
#ifdef DX_TO_GL_ABSTRACTION
ConVar mat_depthbias_shadowmap(	"mat_depthbias_shadowmap", "20", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );
#else
ConVar mat_depthbias_shadowmap(	"mat_depthbias_shadowmap", "0.0005", FCVAR_CHEAT  );
#endif

// For testing Fast Clip
ConVar mat_fastclip( "mat_fastclip", "0", FCVAR_CHEAT  );

//-----------------------------------------------------------------------------
// Determine capabilities
//-----------------------------------------------------------------------------
bool CShaderDeviceMgrDx8::ComputeCapsFromD3D( HardwareCaps_t *pCaps, int nAdapter )
{
	D3DCAPS caps;
	D3DADAPTER_IDENTIFIER9 ident;
	HRESULT hr;

	// NOTE: When getting the caps, we want to be limited by the hardware
	// even if we're running with software T&L...
	hr = m_pD3D->GetDeviceCaps( nAdapter, DX8_DEVTYPE, &caps );
	if ( FAILED( hr ) )
		return false;

	hr = m_pD3D->GetAdapterIdentifier( nAdapter, D3DENUM_WHQL_LEVEL, &ident );
	if ( FAILED( hr ) )
		return false;

	if ( IsOpenGL() )
	{
		if ( !ident.DeviceId && !ident.VendorId )
		{
			ident.DeviceId = 1; // fake default device/vendor ID for OpenGL
			ident.VendorId = 1;
		}
	}

	// Intended for debugging only
	if ( CommandLine()->CheckParm( "-force_device_id" ) )
	{
		const char *pDevID = CommandLine()->ParmValue( "-force_device_id", "" );
		if ( pDevID )
		{
			int nDevID = V_atoi( pDevID );	// use V_atoi for hex support
			if ( nDevID > 0 )
			{
				ident.DeviceId = nDevID;
			}
		}
	}

	// Intended for debugging only
	if ( CommandLine()->CheckParm( "-force_vendor_id" ) )
	{
		const char *pVendorID = CommandLine()->ParmValue( "-force_vendor_id", "" );
		if ( pVendorID )
		{
			int nVendorID = V_atoi( pVendorID );	// use V_atoi for hex support
			if ( pVendorID > 0 )
			{
				ident.VendorId = nVendorID;
			}
		}
	}

	Q_strncpy( pCaps->m_pDriverName, ident.Description, MATERIAL_ADAPTER_NAME_LENGTH );
	pCaps->m_VendorID = ident.VendorId;
	pCaps->m_DeviceID = ident.DeviceId;
	pCaps->m_SubSysID = ident.SubSysId;
	pCaps->m_Revision = ident.Revision;

	pCaps->m_nDriverVersionHigh =  ident.DriverVersion.HighPart;
	pCaps->m_nDriverVersionLow = ident.DriverVersion.LowPart;

	pCaps->m_pShaderDLL[0] = 0;
	pCaps->m_nMaxViewports = 1;

	pCaps->m_PreferDynamicTextures = ( caps.Caps2 & D3DCAPS2_DYNAMICTEXTURES ) ? 1 : 0;

	pCaps->m_HasProjectedBumpEnv = ( caps.TextureCaps & D3DPTEXTURECAPS_NOPROJECTEDBUMPENV ) == 0;

	pCaps->m_HasSetDeviceGammaRamp = (caps.Caps2 & D3DCAPS2_CANCALIBRATEGAMMA) != 0;
	pCaps->m_SupportsVertexShaders = ((caps.VertexShaderVersion >> 8) & 0xFF) >= 1;
	pCaps->m_SupportsPixelShaders = ((caps.PixelShaderVersion >> 8) & 0xFF) >= 1;

	pCaps->m_bScissorSupported = ( caps.RasterCaps & D3DPRASTERCAPS_SCISSORTEST ) !=  0;

#if defined( DX8_COMPATABILITY_MODE )
	pCaps->m_SupportsPixelShaders_1_4  = false;
	pCaps->m_SupportsPixelShaders_2_0  = false;
	pCaps->m_SupportsPixelShaders_2_b  = false;
	pCaps->m_SupportsVertexShaders_2_0 = false;
	pCaps->m_SupportsShaderModel_3_0  = false;
	pCaps->m_SupportsMipmappedCubemaps = false;
#else
	pCaps->m_SupportsPixelShaders_1_4 = ( caps.PixelShaderVersion & 0xffff ) >= 0x0104;
	pCaps->m_SupportsPixelShaders_2_0 = ( caps.PixelShaderVersion & 0xffff ) >= 0x0200;
	pCaps->m_SupportsPixelShaders_2_b = ( ( caps.PixelShaderVersion & 0xffff ) >= 0x0200) && (caps.PS20Caps.NumInstructionSlots >= 512); // More caps to this, but this will do
	pCaps->m_SupportsVertexShaders_2_0 = ( caps.VertexShaderVersion & 0xffff ) >= 0x0200;
	pCaps->m_SupportsShaderModel_3_0 = ( caps.PixelShaderVersion & 0xffff ) >= 0x0300;
	pCaps->m_SupportsMipmappedCubemaps = ( caps.TextureCaps & D3DPTEXTURECAPS_MIPCUBEMAP ) ? true : false;
#endif

	// Slam this off for OpenGL
	if ( IsOpenGL() )
	{
		pCaps->m_SupportsShaderModel_3_0 = false;
	}

	// Slam 3.0 shaders off for Intel
	if ( pCaps->m_VendorID == VENDORID_INTEL )
	{
		pCaps->m_SupportsShaderModel_3_0 = false;
	}

	pCaps->m_MaxVertexShader30InstructionSlots = 0;
	pCaps->m_MaxPixelShader30InstructionSlots  = 0;

	if ( pCaps->m_SupportsShaderModel_3_0 )
	{
		pCaps->m_MaxVertexShader30InstructionSlots = caps.MaxVertexShader30InstructionSlots;
		pCaps->m_MaxPixelShader30InstructionSlots  = caps.MaxPixelShader30InstructionSlots;
	}

	if( CommandLine()->CheckParm( "-nops2b" ) )
	{
		pCaps->m_SupportsPixelShaders_2_b = false;
	}

	pCaps->m_bSoftwareVertexProcessing = false;
	if ( IsWindows() && CommandLine()->CheckParm( "-mat_softwaretl" ) )
	{
		pCaps->m_bSoftwareVertexProcessing = true;
	}

	if ( IsWindows() && !( caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT ) )
	{
		// no hardware t&l. . use software
		pCaps->m_bSoftwareVertexProcessing = true;
	}

	// Set mat_forcedynamic if software vertex processing since the software vp pipe has 
	// problems with sparse vertex buffers (it transforms the whole thing.)
	if ( pCaps->m_bSoftwareVertexProcessing )
	{
		mat_forcedynamic.SetValue( 1 );
	}

	if ( pCaps->m_bSoftwareVertexProcessing )
	{
		pCaps->m_SupportsVertexShaders = true;
		pCaps->m_SupportsVertexShaders_2_0 = true;
	}

#ifdef OSX
	// Static control flow is disabled by default on OSX (the Mac version of togl has known bugs preventing this path from working properly that we've fixed in togl linux/win)
	pCaps->m_bSupportsStaticControlFlow = CommandLine()->CheckParm( "-glslcontrolflow" ) != NULL;
#else
	pCaps->m_bSupportsStaticControlFlow = !CommandLine()->CheckParm( "-noglslcontrolflow" );
#endif

	// NOTE: Texture stages is a fixed-function concept
	// NOTE: Normally, the number of texture units == the number of texture
	// stages except for NVidia hardware, which reports more stages than units.
	// The reason for this is because they expose the inner hardware pixel
	// pipeline through the extra stages. The only thing we use stages for
	// in the hardware is for configuring the color + alpha args + ops.
	pCaps->m_NumSamplers = caps.MaxSimultaneousTextures;
	pCaps->m_NumTextureStages = caps.MaxTextureBlendStages;
	if ( pCaps->m_SupportsPixelShaders_2_0 )
	{
		pCaps->m_NumSamplers = 16;
	}
	else
	{
		Assert( pCaps->m_NumSamplers <= pCaps->m_NumTextureStages );
	}

	// Clamp
	pCaps->m_NumSamplers = min( pCaps->m_NumSamplers, (int)MAX_SAMPLERS );
	pCaps->m_NumTextureStages = min( pCaps->m_NumTextureStages, (int)MAX_TEXTURE_STAGES );

	if ( D3DSupportsCompressedTextures() )
	{
		pCaps->m_SupportsCompressedTextures = COMPRESSED_TEXTURES_ON;
	}
	else
	{
		pCaps->m_SupportsCompressedTextures = COMPRESSED_TEXTURES_OFF;
	}

	pCaps->m_bSupportsAnisotropicFiltering = (caps.TextureFilterCaps & D3DPTFILTERCAPS_MINFANISOTROPIC) != 0;
	pCaps->m_bSupportsMagAnisotropicFiltering = (caps.TextureFilterCaps & D3DPTFILTERCAPS_MAGFANISOTROPIC) != 0;

	// OpenGL does not support this--at least not on OSX which is the primary GL target, so just don't use that path on GL at all.
#if !defined( DX_TO_GL_ABSTRACTION )
	pCaps->m_bCanStretchRectFromTextures = ( ( caps.DevCaps2 & D3DDEVCAPS2_CAN_STRETCHRECT_FROM_TEXTURES ) != 0 ) && ( pCaps->m_VendorID != VENDORID_INTEL );
#else
	pCaps->m_bCanStretchRectFromTextures = false;
#endif

	pCaps->m_nMaxAnisotropy = pCaps->m_bSupportsAnisotropicFiltering ? caps.MaxAnisotropy : 1; 

	pCaps->m_SupportsCubeMaps = ( caps.TextureCaps & D3DPTEXTURECAPS_CUBEMAP ) ? true : false;
	pCaps->m_SupportsNonPow2Textures = 
		( !( caps.TextureCaps & D3DPTEXTURECAPS_POW2 ) || 
		( caps.TextureCaps & D3DPTEXTURECAPS_NONPOW2CONDITIONAL ) );

	Assert( caps.TextureCaps & D3DPTEXTURECAPS_PROJECTED );

	if ( pCaps->m_bSoftwareVertexProcessing )
	{
		// This should be pushed down based on pixel shaders.
		pCaps->m_NumVertexShaderConstants = 256;
		pCaps->m_NumBooleanVertexShaderConstants = pCaps->m_SupportsPixelShaders_2_0 ? 16 : 0;	// 2.0 parts have 16 bool vs registers
		pCaps->m_NumBooleanPixelShaderConstants = pCaps->m_SupportsPixelShaders_2_0 ? 16 : 0;	// 2.0 parts have 16 bool ps registers
		pCaps->m_NumIntegerVertexShaderConstants = pCaps->m_SupportsPixelShaders_2_0 ? 16 : 0;	// 2.0 parts have 16 bool vs registers
		pCaps->m_NumIntegerPixelShaderConstants = pCaps->m_SupportsPixelShaders_2_0 ? 16 : 0;	// 2.0 parts have 16 bool ps registers
	}
	else
	{
		pCaps->m_NumVertexShaderConstants = caps.MaxVertexShaderConst;
		if ( CommandLine()->FindParm( "-limitvsconst" ) )
		{
			pCaps->m_NumVertexShaderConstants = min( 256, pCaps->m_NumVertexShaderConstants );
		}
		pCaps->m_NumBooleanVertexShaderConstants = pCaps->m_SupportsPixelShaders_2_0 ? 16 : 0;	// 2.0 parts have 16 bool vs registers
		pCaps->m_NumBooleanPixelShaderConstants = pCaps->m_SupportsPixelShaders_2_0 ? 16 : 0;	// 2.0 parts have 16 bool ps registers

		// This is a little misleading...this is really 16 int4 registers
		pCaps->m_NumIntegerVertexShaderConstants = pCaps->m_SupportsPixelShaders_2_0 ? 16 : 0;	// 2.0 parts have 16 bool vs registers
		pCaps->m_NumIntegerPixelShaderConstants = pCaps->m_SupportsPixelShaders_2_0 ? 16 : 0;	// 2.0 parts have 16 bool ps registers
	}

	if ( pCaps->m_SupportsPixelShaders )
	{
		if ( pCaps->m_SupportsPixelShaders_2_0 )
		{
			pCaps->m_NumPixelShaderConstants = 32;
		}
		else
		{
			pCaps->m_NumPixelShaderConstants = 8;
		}
	}
	else
	{
		pCaps->m_NumPixelShaderConstants = 0;
	}

	pCaps->m_SupportsHardwareLighting = (caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT) != 0;

	pCaps->m_MaxNumLights = caps.MaxActiveLights;
	if ( pCaps->m_MaxNumLights > MAX_NUM_LIGHTS )
	{
		pCaps->m_MaxNumLights = MAX_NUM_LIGHTS;
	}

	if ( IsOpenGL() )
	{
		// Set according to control flow bit on OpenGL
		pCaps->m_MaxNumLights = MIN( pCaps->m_MaxNumLights, ( pCaps->m_bSupportsStaticControlFlow && pCaps->m_SupportsPixelShaders_2_b ) ? MAX_NUM_LIGHTS : ( MAX_NUM_LIGHTS - 2 ) );
	}

	if ( pCaps->m_bSoftwareVertexProcessing )
	{
		pCaps->m_SupportsHardwareLighting = true;
		pCaps->m_MaxNumLights = 2;
	}
	pCaps->m_MaxTextureWidth = caps.MaxTextureWidth;
	pCaps->m_MaxTextureHeight = caps.MaxTextureHeight;
	pCaps->m_MaxTextureDepth = caps.MaxVolumeExtent ? caps.MaxVolumeExtent : 1;
	pCaps->m_MaxTextureAspectRatio = caps.MaxTextureAspectRatio;
	if ( pCaps->m_MaxTextureAspectRatio == 0 )
	{
		pCaps->m_MaxTextureAspectRatio = max( pCaps->m_MaxTextureWidth, pCaps->m_MaxTextureHeight);
	}
	pCaps->m_MaxPrimitiveCount = caps.MaxPrimitiveCount;
	pCaps->m_MaxBlendMatrices = caps.MaxVertexBlendMatrices;
	pCaps->m_MaxBlendMatrixIndices = caps.MaxVertexBlendMatrixIndex;

	bool addSupported = (caps.TextureOpCaps & D3DTEXOPCAPS_ADD) != 0;
	bool modSupported = (caps.TextureOpCaps & D3DTEXOPCAPS_MODULATE2X) != 0;

	pCaps->m_bNeedsATICentroidHack = false;
	pCaps->m_bDisableShaderOptimizations = false;

	pCaps->m_SupportsMipmapping = true;
	pCaps->m_SupportsOverbright = true;

	// Thank you to all you driver writers who actually correctly return caps
	if ( !modSupported || !addSupported )
	{
		Assert( 0 );
		pCaps->m_SupportsOverbright = false;
	}

	// Check if ZBias and SlopeScaleDepthBias are supported. .if not, tweak the projection matrix instead
	// for polyoffset.
	pCaps->m_ZBiasAndSlopeScaledDepthBiasSupported =
		( ( caps.RasterCaps & D3DPRASTERCAPS_DEPTHBIAS) != 0 ) &&
		( ( caps.RasterCaps & D3DPRASTERCAPS_SLOPESCALEDEPTHBIAS ) != 0 );
	if ( IsX360() )
	{
		// driver lies, force it
		pCaps->m_ZBiasAndSlopeScaledDepthBiasSupported = true;
	}

	// Spheremapping supported?
	pCaps->m_bSupportsSpheremapping = (caps.VertexProcessingCaps & D3DVTXPCAPS_TEXGEN_SPHEREMAP) != 0;

	// How many user clip planes?
	pCaps->m_MaxUserClipPlanes = caps.MaxUserClipPlanes;
	if ( CommandLine()->CheckParm( "-nouserclip" ) /* || (IsOpenGL() && (!CommandLine()->FindParm("-glslmode"))) || r_emulategl.GetBool() */ )
	{
		// rbarris 03Feb10: this now ignores POSIX / -glslmode / r_emulategl because we're defaulting GLSL mode "on".
		// so this will mean that the engine will always ask for user clip planes.
		// this will misbehave under ARB mode, since ARB shaders won't respect that state.
		// it's difficult to make this fluid without teaching the engine about a cap that could change during run.
		
		pCaps->m_MaxUserClipPlanes = 0;
	}

	if ( pCaps->m_MaxUserClipPlanes > MAXUSERCLIPPLANES )
	{
		pCaps->m_MaxUserClipPlanes = MAXUSERCLIPPLANES;
	}

	pCaps->m_FakeSRGBWrite = false;
	pCaps->m_CanDoSRGBReadFromRTs = true;
	pCaps->m_bSupportsGLMixedSizeTargets = false;
#ifdef DX_TO_GL_ABSTRACTION
	// using #if because we're referencing fields in the RHS which don't exist in Windows headers for the caps9 struct
	pCaps->m_FakeSRGBWrite = caps.FakeSRGBWrite != 0;
	pCaps->m_CanDoSRGBReadFromRTs = caps.CanDoSRGBReadFromRTs != 0;
	pCaps->m_bSupportsGLMixedSizeTargets = caps.MixedSizeTargets != 0; 	
#endif
	
	// Query for SRGB support as needed for our DX 9 stuff
	if ( IsPC() || !IsX360() )
	{
		pCaps->m_SupportsSRGB = ( D3D()->CheckDeviceFormat( nAdapter, DX8_DEVTYPE, D3DFMT_X8R8G8B8, D3DUSAGE_QUERY_SRGBREAD, D3DRTYPE_TEXTURE, D3DFMT_DXT1 ) == S_OK);

		if ( pCaps->m_SupportsSRGB )
		{
			pCaps->m_SupportsSRGB = ( D3D()->CheckDeviceFormat( nAdapter, DX8_DEVTYPE, D3DFMT_X8R8G8B8, D3DUSAGE_QUERY_SRGBREAD | D3DUSAGE_QUERY_SRGBWRITE, D3DRTYPE_TEXTURE, D3DFMT_A8R8G8B8 ) == S_OK);
		}
	}
	else
	{
		// 360 does support it, but is queried in the wrong manner, so force it
		pCaps->m_SupportsSRGB = true;
	}

	if ( CommandLine()->CheckParm( "-nosrgb" ) )
	{
		pCaps->m_SupportsSRGB = false;
	}

	pCaps->m_bSupportsVertexTextures = ( D3D()->CheckDeviceFormat( nAdapter, DX8_DEVTYPE, D3DFMT_X8R8G8B8,
		D3DUSAGE_QUERY_VERTEXTEXTURE, D3DRTYPE_TEXTURE, D3DFMT_R32F ) == S_OK );

	if ( IsOpenGL() )
	{
		pCaps->m_bSupportsVertexTextures = false;
	}

	// FIXME: vs30 has a fixed setting here at 4.
	// Future hardware will need some other way of computing this.
	pCaps->m_nVertexTextureCount = pCaps->m_bSupportsVertexTextures ? 4 : 0;

	// FIXME: How do I actually compute this?
	pCaps->m_nMaxVertexTextureDimension = pCaps->m_bSupportsVertexTextures ? 4096 : 0;

	// Does the device support filterable int16 textures?
	bool bSupportsInteger16Textures = 		
		( D3D()->CheckDeviceFormat( nAdapter, DX8_DEVTYPE,
		D3DFMT_X8R8G8B8, D3DUSAGE_QUERY_FILTER,
		D3DRTYPE_TEXTURE, D3DFMT_A16B16G16R16 ) == S_OK );

	// Does the device support filterable fp16 textures?
	bool bSupportsFloat16Textures = 		
		( D3D()->CheckDeviceFormat( nAdapter, DX8_DEVTYPE,
		D3DFMT_X8R8G8B8, D3DUSAGE_QUERY_FILTER,
		D3DRTYPE_TEXTURE, D3DFMT_A16B16G16R16F ) == S_OK );

	// Does the device support blendable fp16 render targets?
	bool bSupportsFloat16RenderTargets = 		
		( D3D()->CheckDeviceFormat( nAdapter, DX8_DEVTYPE,
		D3DFMT_X8R8G8B8, D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING | D3DUSAGE_RENDERTARGET,
		D3DRTYPE_TEXTURE, D3DFMT_A16B16G16R16F ) == S_OK );

	// Essentially a proxy for a DX10 device running DX9 code path
	pCaps->m_bSupportsFloat32RenderTargets = ( D3D()->CheckDeviceFormat( nAdapter, DX8_DEVTYPE,
	D3DFMT_X8R8G8B8, D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING | D3DUSAGE_RENDERTARGET,
		D3DRTYPE_TEXTURE, D3DFMT_A32B32G32R32F ) == S_OK );

	pCaps->m_bFogColorSpecifiedInLinearSpace = false;
	pCaps->m_bFogColorAlwaysLinearSpace = false;

	// Assume not DX10.  Check below.
	pCaps->m_bDX10Card = false;
	pCaps->m_bDX10Blending = false;

	if ( IsOpenGL() && ( pCaps->m_VendorID == 1 ) )
	{
		// Linux/Win OpenGL - always assume the device supports DX10 style blending
		pCaps->m_bFogColorAlwaysLinearSpace = true;
		pCaps->m_bDX10Card = true;
		pCaps->m_bDX10Blending = true;
	}

	// NVidia wants fog color to be specified in linear space
	if ( IsPC() && pCaps->m_SupportsSRGB )
	{
		if ( pCaps->m_VendorID == VENDORID_NVIDIA )
		{
			pCaps->m_bFogColorSpecifiedInLinearSpace = true;

			if ( IsOpenGL() )
			{
				// If we're not the Quadro 4500 or GeForce 7x000, we're an NVIDIA DX10 part on MacOS
				if ( !( (pCaps->m_DeviceID == 0x009d) || ( (pCaps->m_DeviceID >= 0x0391) && (pCaps->m_DeviceID <= 0x0395) ) ) )
				{
					pCaps->m_bFogColorAlwaysLinearSpace = true;
					pCaps->m_bDX10Card = true;
					pCaps->m_bDX10Blending = true;
				}
			}
			else
			{	
				// On G80 and later, always specify in linear space
				if ( pCaps->m_bSupportsFloat32RenderTargets )
				{
					pCaps->m_bFogColorAlwaysLinearSpace = true;
					pCaps->m_bDX10Card = true;
					pCaps->m_bDX10Blending = true;
				}
			}
		}
		else if ( pCaps->m_VendorID == VENDORID_ATI )
		{
			if ( IsOpenGL() )
			{
				// If we're not a Radeon X1x00 (device IDs in this range), we're a DX10 chip
				if ( !( (pCaps->m_DeviceID >= 0x7109) && (pCaps->m_DeviceID <= 0x7291) ) )
				{
					pCaps->m_bFogColorSpecifiedInLinearSpace = true;
					pCaps->m_bFogColorAlwaysLinearSpace = true;
					pCaps->m_bDX10Card = true;
					pCaps->m_bDX10Blending = true;
				}
			}
			else
			{
				// Check for DX10 part
				pCaps->m_bDX10Card = pCaps->m_SupportsShaderModel_3_0 &&
				( pCaps->m_MaxVertexShader30InstructionSlots > 1024 ) &&
				( pCaps->m_MaxPixelShader30InstructionSlots > 512 ) ;
				
				// On ATI, DX10 card means DX10 blending
				pCaps->m_bDX10Blending = pCaps->m_bDX10Card;

				if( pCaps->m_bDX10Blending )
				{
					pCaps->m_bFogColorSpecifiedInLinearSpace = true;
					pCaps->m_bFogColorAlwaysLinearSpace = true;
				}
			}
		}
		else if ( pCaps->m_VendorID == VENDORID_INTEL )
		{
			// Intel does not have performant vertex textures
			pCaps->m_bDX10Card = false;

			// Intel supports DX10 SRGB on Broadwater and better
			// The two checks are for devices from GMA generation (0x29A2-0x2A43) and HD graphics (0x0042-0x2500)
			pCaps->m_bDX10Blending = ( ( pCaps->m_DeviceID >= 0x29A2 ) && ( pCaps->m_DeviceID <= 0x2A43  ) ) ||
									 ( ( pCaps->m_DeviceID >= 0x0042 ) && ( pCaps->m_DeviceID <= 0x2500  ) );

			if( pCaps->m_bDX10Blending )
			{
				pCaps->m_bFogColorSpecifiedInLinearSpace = true;
				pCaps->m_bFogColorAlwaysLinearSpace = true;
			}
		}
	}

	// Do we have everything necessary to run with integer HDR?  Note that
	// even if we don't support integer 16-bit/component textures, we
	// can still run in this mode if fp16 textures are supported.
	bool bSupportsIntegerHDR = pCaps->m_SupportsPixelShaders_2_0 &&
		pCaps->m_SupportsVertexShaders_2_0 &&
		//		(caps.Caps3 & D3DCAPS3_ALPHA_FULLSCREEN_FLIP_OR_DISCARD) &&
		//		(caps.PrimitiveMiscCaps & D3DPMISCCAPS_SEPARATEALPHABLEND) &&
		( bSupportsInteger16Textures || bSupportsFloat16Textures ) &&
		pCaps->m_SupportsSRGB;

	// Do we have everything necessary to run with float HDR?
	bool bSupportsFloatHDR = pCaps->m_SupportsShaderModel_3_0 &&
		//		(caps.Caps3 & D3DCAPS3_ALPHA_FULLSCREEN_FLIP_OR_DISCARD) &&
		//		(caps.PrimitiveMiscCaps & D3DPMISCCAPS_SEPARATEALPHABLEND) &&
		bSupportsFloat16Textures &&
		bSupportsFloat16RenderTargets &&
		pCaps->m_SupportsSRGB && 
		!IsX360();

	pCaps->m_MaxHDRType = HDR_TYPE_NONE;
	if ( bSupportsFloatHDR )
		pCaps->m_MaxHDRType = HDR_TYPE_FLOAT;
	else
		if ( bSupportsIntegerHDR )
			pCaps->m_MaxHDRType = HDR_TYPE_INTEGER;
		
	if ( bSupportsFloatHDR  && ( mat_hdr_level.GetInt() == 3 ) )
	{
		pCaps->m_HDRType = HDR_TYPE_FLOAT;
	}
	else if ( bSupportsIntegerHDR )
	{
		pCaps->m_HDRType = HDR_TYPE_INTEGER;
	}
	else
	{
		pCaps->m_HDRType = HDR_TYPE_NONE;
	}

	pCaps->m_bColorOnSecondStream = caps.MaxStreams > 1;

	pCaps->m_bSupportsStreamOffset = ( ( caps.DevCaps2 & D3DDEVCAPS2_STREAMOFFSET ) &&	// Tie these caps together since we want to filter out
										pCaps->m_SupportsPixelShaders_2_0 );			// any DX8 parts which export D3DDEVCAPS2_STREAMOFFSET

	pCaps->m_flMinGammaControlPoint = 0.0f;
	pCaps->m_flMaxGammaControlPoint = 65535.0f;
	pCaps->m_nGammaControlPointCount = 256;

	// Compute the effective DX support level based on all the other caps
	ComputeDXSupportLevel( *pCaps );
	int nCmdlineMaxDXLevel = CommandLine()->ParmValue( "-maxdxlevel", 0 );
	if ( IsOpenGL() && ( nCmdlineMaxDXLevel > 0 ) )
	{
		// Prevent customers from slamming us below DX level 90 in OpenGL mode.
		nCmdlineMaxDXLevel = MAX( nCmdlineMaxDXLevel, 90 );
	}
	if( nCmdlineMaxDXLevel > 0 )
	{
		pCaps->m_nMaxDXSupportLevel = min( pCaps->m_nMaxDXSupportLevel, nCmdlineMaxDXLevel );
	}
	pCaps->m_nDXSupportLevel = pCaps->m_nMaxDXSupportLevel;

	int nModelIndex = pCaps->m_nDXSupportLevel < 90 ? VERTEX_SHADER_MODEL - 10 : VERTEX_SHADER_MODEL;
	pCaps->m_MaxVertexShaderBlendMatrices = (pCaps->m_NumVertexShaderConstants - nModelIndex) / 3;

	if ( pCaps->m_MaxVertexShaderBlendMatrices > NUM_MODEL_TRANSFORMS )
	{
		pCaps->m_MaxVertexShaderBlendMatrices = NUM_MODEL_TRANSFORMS;
	}

	CheckBorderColorSupport( pCaps, nAdapter );

	// This may get more complex if we start using multiple flavors of compressed vertex - for now it's "on or off"
	pCaps->m_SupportsCompressedVertices = ( pCaps->m_nDXSupportLevel >= 90 ) && ( pCaps->m_CanDoSRGBReadFromRTs ) ? VERTEX_COMPRESSION_ON : VERTEX_COMPRESSION_NONE;
	if ( CommandLine()->CheckParm( "-no_compressed_verts" ) )						  // m_CanDoSRGBReadFromRTs limits us to Snow Leopard or later on OSX
	{
		pCaps->m_SupportsCompressedVertices = VERTEX_COMPRESSION_NONE;
	}

	// Various vendor-dependent checks...
	CheckVendorDependentAlphaToCoverage( pCaps, nAdapter );
	CheckVendorDependentShadowMappingSupport( pCaps, nAdapter );

	// If we're not on a 3.0 part, these values are more appropriate (X800 & X850 parts from ATI do shadow mapping but not 3.0 )
	if ( !IsOpenGL() )
	{
		if ( !pCaps->m_SupportsShaderModel_3_0 )
		{
			mat_slopescaledepthbias_shadowmap.SetValue( 5.9f );
			mat_depthbias_shadowmap.SetValue( 0.003f );
		}
	}

	if( pCaps->m_MaxUserClipPlanes == 0 )
	{
		pCaps->m_UseFastClipping = true;
	}

	pCaps->m_MaxSimultaneousRenderTargets = caps.NumSimultaneousRTs;

	return true;
}

//-----------------------------------------------------------------------------
// Compute the effective DX support level based on all the other caps
//-----------------------------------------------------------------------------
void CShaderDeviceMgrDx8::ComputeDXSupportLevel( HardwareCaps_t &caps )
{
	// NOTE: Support level is actually DX level * 10 + subversion
	// So, 70 = DX7, 80 = DX8, 81 = DX8 w/ 1.4 pixel shaders
	// 90 = DX9 w/ 2.0 pixel shaders
	// 95 = DX9 w/ 3.0 pixel shaders and vertex textures
	// 98 = DX9 XBox360
	// NOTE: 82 = NVidia nv3x cards, which can't run dx9 fast

	// FIXME: Improve this!! There should be a whole list of features
	// we require in order to be considered a DX7 board, DX8 board, etc.

	if ( IsX360() )
	{
		caps.m_nMaxDXSupportLevel = 98;
		return;
	}

	bool bIsOpenGL = IsOpenGL();

	if ( caps.m_SupportsShaderModel_3_0 && !bIsOpenGL ) // Note that we don't tie vertex textures to 30 shaders anymore
	{
		caps.m_nMaxDXSupportLevel = 95;
		return;
	}

	// NOTE: sRGB is currently required for DX90 because it isn't doing 
	// gamma correctly if that feature doesn't exist
	if ( caps.m_SupportsVertexShaders_2_0 && caps.m_SupportsPixelShaders_2_0 && caps.m_SupportsSRGB )
	{
		caps.m_nMaxDXSupportLevel = 90;
		return;
	}

	if ( caps.m_SupportsPixelShaders && caps.m_SupportsVertexShaders )// && caps.m_bColorOnSecondStream)
	{
		if (caps.m_SupportsPixelShaders_1_4)
		{
			caps.m_nMaxDXSupportLevel = 81;
			return;
		}
		caps.m_nMaxDXSupportLevel = 80;
		return;
	}

	if( caps.m_SupportsCubeMaps && ( caps.m_MaxBlendMatrices >= 2 ) )
	{
		caps.m_nMaxDXSupportLevel = 70;
		return;
	}

	if ( ( caps.m_NumSamplers >= 2) && caps.m_SupportsMipmapping )
	{
		caps.m_nMaxDXSupportLevel = 60;
		return;
	}

	Assert( 0 ); 
	// we don't support this!
	caps.m_nMaxDXSupportLevel = 50;
}



//-----------------------------------------------------------------------------
// Gets the number of adapters...
//-----------------------------------------------------------------------------
int CShaderDeviceMgrDx8::GetAdapterCount() const
{
	// FIXME: Remove call to InitAdapterInfo once Steam startup issues are resolved.
	const_cast<CShaderDeviceMgrDx8*>( this )->InitAdapterInfo();

	return m_Adapters.Count();
}


//-----------------------------------------------------------------------------
// Returns info about each adapter
//-----------------------------------------------------------------------------
void CShaderDeviceMgrDx8::GetAdapterInfo( int nAdapter, MaterialAdapterInfo_t& info ) const
{
	// FIXME: Remove call to InitAdapterInfo once Steam startup issues are resolved.
	const_cast<CShaderDeviceMgrDx8*>( this )->InitAdapterInfo();

	Assert( ( nAdapter >= 0 ) && ( nAdapter < m_Adapters.Count() ) );
	const HardwareCaps_t &caps = m_Adapters[ nAdapter ].m_ActualCaps;
	memcpy( &info, &caps, sizeof(MaterialAdapterInfo_t) );
}


//-----------------------------------------------------------------------------
// Sets the adapter
//-----------------------------------------------------------------------------
bool CShaderDeviceMgrDx8::SetAdapter( int nAdapter, int nAdapterFlags )
{
	LOCK_SHADERAPI();

	// FIXME:
	//	g_pShaderDeviceDx8->m_bReadPixelsEnabled = (nAdapterFlags & MATERIAL_INIT_READ_PIXELS_ENABLED) != 0;

	// Set up hardware information for this adapter...
	g_pShaderDeviceDx8->m_DeviceType = (nAdapterFlags & MATERIAL_INIT_REFERENCE_RASTERIZER) ? 
		D3DDEVTYPE_REF : D3DDEVTYPE_HAL;

	g_pShaderDeviceDx8->m_DisplayAdapter = nAdapter;
	if ( g_pShaderDeviceDx8->m_DisplayAdapter >= (UINT)GetAdapterCount() )
	{
		g_pShaderDeviceDx8->m_DisplayAdapter = 0;
	}

#ifdef NVPERFHUD
	// hack for nvperfhud
	g_pShaderDeviceDx8->m_DisplayAdapter = m_pD3D->GetAdapterCount() - 1;
	g_pShaderDeviceDx8->m_DeviceType = D3DDEVTYPE_REF;
#endif

	// backward compat
	if ( !g_pShaderDeviceDx8->OnAdapterSet() )
		return false;

//	if ( !g_pShaderDeviceDx8->Init() )
//	{
//		Warning( "Unable to initialize dx8 device!\n" );
//		return false;
//	}

	g_pShaderDevice = g_pShaderDeviceDx8;

	return true;
}


//-----------------------------------------------------------------------------
// Returns the number of modes
//-----------------------------------------------------------------------------
int CShaderDeviceMgrDx8::GetModeCount( int nAdapter ) const
{
	LOCK_SHADERAPI();
	Assert( m_pD3D && (nAdapter < GetAdapterCount() ) );

#if !defined( _X360 )
	// fixme - what format should I use here?
	return m_pD3D->GetAdapterModeCount( nAdapter, D3DFMT_X8R8G8B8 );
#else
	return 1; // Only one mode, which is the current mode set in the 360 dashboard.  Going to fill it in with exactly what the 360 is set to.
#endif
}


//-----------------------------------------------------------------------------
// Returns mode information..
//-----------------------------------------------------------------------------
void CShaderDeviceMgrDx8::GetModeInfo( ShaderDisplayMode_t* pInfo, int nAdapter, int nMode ) const
{
	Assert( pInfo->m_nVersion == SHADER_DISPLAY_MODE_VERSION );

	LOCK_SHADERAPI();
	Assert( m_pD3D && (nAdapter < GetAdapterCount() ) );
	Assert( nMode < GetModeCount( nAdapter ) );

#if !defined( _X360 )
	HRESULT hr;
	D3DDISPLAYMODE d3dInfo;

	// fixme - what format should I use here?
	hr = D3D()->EnumAdapterModes( nAdapter, D3DFMT_X8R8G8B8, nMode, &d3dInfo );
	Assert( !FAILED(hr) );

	pInfo->m_nWidth       = d3dInfo.Width;
	pInfo->m_nHeight      = d3dInfo.Height;
	pInfo->m_Format      = ImageLoader::D3DFormatToImageFormat( d3dInfo.Format );
	pInfo->m_nRefreshRateNumerator = d3dInfo.RefreshRate;
	pInfo->m_nRefreshRateDenominator = 1;
#else
	pInfo->m_Format = ImageLoader::D3DFormatToImageFormat( D3DFMT_X8R8G8B8 );
	pInfo->m_nRefreshRateNumerator = 60;
	pInfo->m_nRefreshRateDenominator = 1;

	pInfo->m_nWidth = GetSystemMetrics( SM_CXSCREEN );
	pInfo->m_nHeight = GetSystemMetrics( SM_CYSCREEN );
#endif
}


//-----------------------------------------------------------------------------
// Returns the current mode information for an adapter
//-----------------------------------------------------------------------------
void CShaderDeviceMgrDx8::GetCurrentModeInfo( ShaderDisplayMode_t* pInfo, int nAdapter ) const
{
	Assert( pInfo->m_nVersion == SHADER_DISPLAY_MODE_VERSION );

	LOCK_SHADERAPI();
	Assert( D3D() );

	HRESULT hr;
	D3DDISPLAYMODE mode = { 0 };
#if !defined( _X360 )
	hr = D3D()->GetAdapterDisplayMode( nAdapter, &mode );
	Assert( !FAILED(hr) );
#else
	if ( !g_pD3DDevice )
	{
		// the console has no prior display or mode until its created
		mode.Width  = GetSystemMetrics( SM_CXSCREEN );
		mode.Height = GetSystemMetrics( SM_CYSCREEN );
		mode.RefreshRate = 60;
		mode.Format = D3DFMT_X8R8G8B8;
	}
	else
	{
		hr = g_pD3DDevice->GetDisplayMode( 0, &mode );
		Assert( !FAILED(hr) );
	}
#endif

	pInfo->m_nWidth = mode.Width;
	pInfo->m_nHeight = mode.Height;
	pInfo->m_Format = ImageLoader::D3DFormatToImageFormat( mode.Format );
	pInfo->m_nRefreshRateNumerator = mode.RefreshRate;
	pInfo->m_nRefreshRateDenominator = 1;
}


//-----------------------------------------------------------------------------
// Sets the video mode
//-----------------------------------------------------------------------------
CreateInterfaceFn CShaderDeviceMgrDx8::SetMode( void *hWnd, int nAdapter, const ShaderDeviceInfo_t& mode )
{
	LOCK_SHADERAPI();

	Assert( nAdapter < GetAdapterCount() );
	int nDXLevel = mode.m_nDXLevel != 0 ? mode.m_nDXLevel : m_Adapters[nAdapter].m_ActualCaps.m_nDXSupportLevel;
	if ( m_bObeyDxCommandlineOverride )
	{
		nDXLevel = CommandLine()->ParmValue( "-dxlevel", nDXLevel );
		m_bObeyDxCommandlineOverride = false;
	}
	if ( nDXLevel > m_Adapters[nAdapter].m_ActualCaps.m_nMaxDXSupportLevel )
	{
		nDXLevel = m_Adapters[nAdapter].m_ActualCaps.m_nMaxDXSupportLevel;
	}
	nDXLevel = GetClosestActualDXLevel( nDXLevel );

	if ( nDXLevel >= 100 )
		return NULL;

	bool bReacquireResourcesNeeded = false;
	if ( g_pShaderDevice )
	{
		bReacquireResourcesNeeded = IsPC();
		g_pShaderDevice->ReleaseResources();
	}

	if ( g_pShaderAPI )
	{
		g_pShaderAPI->OnDeviceShutdown();
		g_pShaderAPI = NULL;
	}

	if ( g_pShaderDevice )
	{
		g_pShaderDevice->ShutdownDevice();
		g_pShaderDevice = NULL;
	}

	g_pShaderShadow = NULL;

	ShaderDeviceInfo_t adjustedMode = mode;
	adjustedMode.m_nDXLevel = nDXLevel;
	if ( !g_pShaderDeviceDx8->InitDevice( hWnd, nAdapter, adjustedMode ) )
		return NULL;

	if ( !g_pShaderAPIDX8->OnDeviceInit() )
		return NULL;

	g_pShaderDevice = g_pShaderDeviceDx8;
	g_pShaderAPI = g_pShaderAPIDX8;
	g_pShaderShadow = g_pShaderShadowDx8;

	if ( bReacquireResourcesNeeded )
	{
		g_pShaderDevice->ReacquireResources();
	}

	return ShaderInterfaceFactory;
}


//-----------------------------------------------------------------------------
// Validates the mode...
//-----------------------------------------------------------------------------
bool CShaderDeviceMgrDx8::ValidateMode( int nAdapter, const ShaderDeviceInfo_t &info ) const
{
	if ( nAdapter >= (int)D3D()->GetAdapterCount() )
		return false;

	ShaderDisplayMode_t displayMode;

	if ( info.m_bWindowed )
	{
		// windowed mode always appears on the primary display, so we should use that adapter's
		// settings
		GetCurrentModeInfo( &displayMode, 0 );

		// make sure the window fits within the current video mode
		if ( ( info.m_DisplayMode.m_nWidth > displayMode.m_nWidth ) ||
			 ( info.m_DisplayMode.m_nHeight > displayMode.m_nHeight ) )
			return false;
	}
	else
	{
		GetCurrentModeInfo( &displayMode, nAdapter );
	}

	// Make sure the image format requested is valid
	ImageFormat backBufferFormat = FindNearestSupportedBackBufferFormat( nAdapter,
		DX8_DEVTYPE, displayMode.m_Format, info.m_DisplayMode.m_Format, info.m_bWindowed );
	return ( backBufferFormat != IMAGE_FORMAT_UNKNOWN );
}


//-----------------------------------------------------------------------------
// Returns the amount of video memory in bytes for a particular adapter
//-----------------------------------------------------------------------------
int CShaderDeviceMgrDx8::GetVidMemBytes( int nAdapter ) const
{
#if defined( _X360 )
	return 256*1024*1024;
#elif defined (DX_TO_GL_ABSTRACTION)
	D3DADAPTER_IDENTIFIER9 devIndentifier;
	D3D()->GetAdapterIdentifier( nAdapter, D3DENUM_WHQL_LEVEL, &devIndentifier );
	return devIndentifier.VideoMemory;
#else
	// FIXME: This currently ignores the adapter
	uint64 nBytes = ::GetVidMemBytes();
	if ( nBytes > INT_MAX )
		return INT_MAX;
	return nBytes;
#endif
}



//-----------------------------------------------------------------------------
//
// Shader device
//
//-----------------------------------------------------------------------------

#if 0
// FIXME: Enable after I've separated it out from shaderapidx8 a little better
static CShaderDeviceDx8 s_ShaderDeviceDX8;
CShaderDeviceDx8* g_pShaderDeviceDx8 = &s_ShaderDeviceDX8;
#endif


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CShaderDeviceDx8::CShaderDeviceDx8()
{
	g_pD3DDevice = NULL;
	for ( int i = 0; i < ARRAYSIZE(m_pFrameSyncQueryObject); i++ )
	{
		m_pFrameSyncQueryObject[i] = NULL;
		m_bQueryIssued[i] = false;
	}
	m_pFrameSyncTexture = NULL;
	m_bQueuedDeviceLost = false;
	m_DeviceState = DEVICE_STATE_OK;
	m_bOtherAppInitializing = false;
	m_IsResizing = false;
	m_bPendingVideoModeChange = false;
	m_DeviceSupportsCreateQuery = -1;
	m_bUsingStencil = false;
	m_bResourcesReleased = false;
	m_iStencilBufferBits = 0;
	m_NonInteractiveRefresh.m_Mode = MATERIAL_NON_INTERACTIVE_MODE_NONE;
	m_NonInteractiveRefresh.m_pVertexShader = NULL;
	m_NonInteractiveRefresh.m_pPixelShader = NULL;
	m_NonInteractiveRefresh.m_pPixelShaderStartup = NULL;
	m_NonInteractiveRefresh.m_pPixelShaderStartupPass2 = NULL;
	m_NonInteractiveRefresh.m_pVertexDecl = NULL;
	m_NonInteractiveRefresh.m_nPacifierFrame = 0;
	m_numReleaseResourcesRefCount = 0;
}

CShaderDeviceDx8::~CShaderDeviceDx8()
{
}


//-----------------------------------------------------------------------------
// Computes device creation paramters
//-----------------------------------------------------------------------------
static DWORD ComputeDeviceCreationFlags( D3DCAPS& caps, bool bSoftwareVertexProcessing )
{
	// Find out what type of device to make
	bool bPureDeviceSupported = (caps.DevCaps & D3DDEVCAPS_PUREDEVICE) != 0;

	DWORD nDeviceCreationFlags;
	if ( !bSoftwareVertexProcessing )
	{
		nDeviceCreationFlags = D3DCREATE_HARDWARE_VERTEXPROCESSING;
		if ( bPureDeviceSupported )
		{
			nDeviceCreationFlags |= D3DCREATE_PUREDEVICE;
		}
	}
	else
	{
		nDeviceCreationFlags = D3DCREATE_SOFTWARE_VERTEXPROCESSING;
	}
	nDeviceCreationFlags |= D3DCREATE_FPU_PRESERVE;

#ifdef _X360
	nDeviceCreationFlags |= D3DCREATE_BUFFER_2_FRAMES;
#endif

	return nDeviceCreationFlags;
}


//-----------------------------------------------------------------------------
// Computes the supersample flags
//-----------------------------------------------------------------------------
D3DMULTISAMPLE_TYPE CShaderDeviceDx8::ComputeMultisampleType( int nSampleCount )
{
	switch (nSampleCount)
	{
#if !defined( _X360 )
	case 2: return D3DMULTISAMPLE_2_SAMPLES;
	case 3: return D3DMULTISAMPLE_3_SAMPLES;
	case 4: return D3DMULTISAMPLE_4_SAMPLES;
	case 5: return D3DMULTISAMPLE_5_SAMPLES;
	case 6: return D3DMULTISAMPLE_6_SAMPLES;
	case 7: return D3DMULTISAMPLE_7_SAMPLES;
	case 8: return D3DMULTISAMPLE_8_SAMPLES;
	case 9: return D3DMULTISAMPLE_9_SAMPLES;
	case 10: return D3DMULTISAMPLE_10_SAMPLES;
	case 11: return D3DMULTISAMPLE_11_SAMPLES;
	case 12: return D3DMULTISAMPLE_12_SAMPLES;
	case 13: return D3DMULTISAMPLE_13_SAMPLES;
	case 14: return D3DMULTISAMPLE_14_SAMPLES;
	case 15: return D3DMULTISAMPLE_15_SAMPLES;
	case 16: return D3DMULTISAMPLE_16_SAMPLES;
#else
	case 2: return D3DMULTISAMPLE_2_SAMPLES;
	case 4: return D3DMULTISAMPLE_4_SAMPLES;
#endif
	default:
	case 0:
	case 1:
		return D3DMULTISAMPLE_NONE;
	}
}


//-----------------------------------------------------------------------------
// Sets the present parameters
//-----------------------------------------------------------------------------
void CShaderDeviceDx8::SetPresentParameters( void* hWnd, int nAdapter, const ShaderDeviceInfo_t &info )
{
	ShaderDisplayMode_t mode;
	g_pShaderDeviceMgr->GetCurrentModeInfo( &mode, nAdapter );

	HRESULT hr;
	ZeroMemory( &m_PresentParameters, sizeof(m_PresentParameters) );

	m_PresentParameters.Windowed = info.m_bWindowed;
	m_PresentParameters.SwapEffect = info.m_bUsingMultipleWindows ? D3DSWAPEFFECT_COPY : D3DSWAPEFFECT_DISCARD;

	// for 360, we want to create it ourselves for hierarchical z support
	m_PresentParameters.EnableAutoDepthStencil = IsX360() ? FALSE : TRUE; 

	// What back-buffer format should we use?
	ImageFormat backBufferFormat = FindNearestSupportedBackBufferFormat( nAdapter,
		DX8_DEVTYPE, m_AdapterFormat, info.m_DisplayMode.m_Format, info.m_bWindowed );

	// What depth format should we use?
	m_bUsingStencil = info.m_bUseStencil;
	if ( info.m_nDXLevel >= 80 )
	{
		// always stencil for dx9/hdr
		m_bUsingStencil = true;
	}
#if defined( _X360 )
	D3DFORMAT nDepthFormat = ReverseDepthOnX360() ? D3DFMT_D24FS8 : D3DFMT_D24S8;
#else
	D3DFORMAT nDepthFormat = m_bUsingStencil ? D3DFMT_D24S8 : D3DFMT_D24X8;
#endif
	m_PresentParameters.AutoDepthStencilFormat = FindNearestSupportedDepthFormat( 
		nAdapter, m_AdapterFormat, backBufferFormat, nDepthFormat );
	m_PresentParameters.hDeviceWindow = (VD3DHWND)hWnd;

	// store how many stencil buffer bits we have available with the depth/stencil buffer
	switch( m_PresentParameters.AutoDepthStencilFormat )
	{
	case D3DFMT_D24S8:
		m_iStencilBufferBits = 8;
		break;
#if defined( _X360 )
	case D3DFMT_D24FS8:
		m_iStencilBufferBits = 8;
		break;
#else
	case D3DFMT_D24X4S4:
		m_iStencilBufferBits = 4;
		break;
	case D3DFMT_D15S1:
		m_iStencilBufferBits = 1;
		break;
#endif
	default:
		m_iStencilBufferBits = 0;
		m_bUsingStencil = false; //couldn't acquire a stencil buffer
	};

	if ( IsX360() || !info.m_bWindowed )
	{
		bool useDefault = ( info.m_DisplayMode.m_nWidth == 0 ) || ( info.m_DisplayMode.m_nHeight == 0 );
		m_PresentParameters.BackBufferCount = 1;
		m_PresentParameters.BackBufferWidth = useDefault ? mode.m_nWidth : info.m_DisplayMode.m_nWidth;
		m_PresentParameters.BackBufferHeight = useDefault ? mode.m_nHeight : info.m_DisplayMode.m_nHeight;
		m_PresentParameters.BackBufferFormat = ImageLoader::ImageFormatToD3DFormat( backBufferFormat );
#if defined( _X360 )
		m_PresentParameters.FrontBufferFormat = D3DFMT_LE_X8R8G8B8;
#endif
		if ( !info.m_bWaitForVSync || CommandLine()->FindParm( "-forcenovsync" ) )
		{
			m_PresentParameters.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
		}
		else
		{
			m_PresentParameters.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
		}

		m_PresentParameters.FullScreen_RefreshRateInHz = info.m_DisplayMode.m_nRefreshRateDenominator ? 
			info.m_DisplayMode.m_nRefreshRateNumerator / info.m_DisplayMode.m_nRefreshRateDenominator : D3DPRESENT_RATE_DEFAULT;

#if defined( _X360 )
		XVIDEO_MODE videoMode;
		XGetVideoMode( &videoMode );

		// want 30 for 60Hz, and 25 for 50Hz (PAL)
		int nNewFpsMax = ( ( int )( videoMode.RefreshRate + 0.5f ) ) >> 1;
		// slam to either 30 or 25 so that we don't end up with any other cases.
		if( nNewFpsMax < 26 )
		{
			nNewFpsMax = 25;
		}
		else
		{
			nNewFpsMax = 30;
		}
		DevMsg( "*******Monitor refresh is %f, setting fps_max to %d*********\n", videoMode.RefreshRate, nNewFpsMax );
		ConVarRef fps_max( "fps_max" );
		fps_max.SetValue( nNewFpsMax );

		// setup hardware scaling - should be native 720p upsampling to 1080i
		if ( info.m_bScaleToOutputResolution )
		{
			m_PresentParameters.VideoScalerParameters.ScalerSourceRect.x2 = m_PresentParameters.BackBufferWidth;
			m_PresentParameters.VideoScalerParameters.ScalerSourceRect.y2 = m_PresentParameters.BackBufferHeight;
			m_PresentParameters.VideoScalerParameters.ScaledOutputWidth = videoMode.dwDisplayWidth;
			m_PresentParameters.VideoScalerParameters.ScaledOutputHeight = videoMode.dwDisplayHeight;
			DevMsg( "VIDEO SCALING: scaling from %dx%d to %dx%d\n", ( int )m_PresentParameters.BackBufferWidth, ( int )m_PresentParameters.BackBufferHeight,
				( int )videoMode.dwDisplayWidth, ( int )videoMode.dwDisplayHeight );
		}
		else
		{
			DevMsg( "VIDEO SCALING: No scaling: %dx%d\n", ( int )m_PresentParameters.BackBufferWidth, ( int )m_PresentParameters.BackBufferHeight );
		}
#endif
	}
	else
	{
		// NJS: We are seeing a lot of time spent in present in some cases when this isn't set.
		m_PresentParameters.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
		if ( info.m_bResizing )
		{
			if ( info.m_bLimitWindowedSize &&
				( info.m_nWindowedSizeLimitWidth < mode.m_nWidth || info.m_nWindowedSizeLimitHeight < mode.m_nHeight ) )
			{
				// When using material system in windowed resizing apps, it's
				// sometimes not a good idea to allocate stuff as big as the screen
				// video cards can soo run out of resources
				m_PresentParameters.BackBufferWidth = info.m_nWindowedSizeLimitWidth;
				m_PresentParameters.BackBufferHeight = info.m_nWindowedSizeLimitHeight;
			}
			else
			{
				// When in resizing windowed mode, 
				// we want to allocate enough memory to deal with any resizing...
				m_PresentParameters.BackBufferWidth = mode.m_nWidth;
				m_PresentParameters.BackBufferHeight = mode.m_nHeight;
			}
		}
		else
		{
			m_PresentParameters.BackBufferWidth = info.m_DisplayMode.m_nWidth;
			m_PresentParameters.BackBufferHeight = info.m_DisplayMode.m_nHeight;
		}
		m_PresentParameters.BackBufferFormat = ImageLoader::ImageFormatToD3DFormat( backBufferFormat );
		m_PresentParameters.BackBufferCount = 1;
	}

	if ( info.m_nAASamples > 0 && ( m_PresentParameters.SwapEffect == D3DSWAPEFFECT_DISCARD ) )
	{
		D3DMULTISAMPLE_TYPE multiSampleType = ComputeMultisampleType( info.m_nAASamples );
		DWORD nQualityLevel;

		// FIXME: Should we add the quality level to the ShaderAdapterMode_t struct?
		// 16x on nVidia refers to CSAA or "Coverage Sampled Antialiasing"
		const HardwareCaps_t &adapterCaps = g_ShaderDeviceMgrDx8.GetHardwareCaps( nAdapter );
		if ( ( info.m_nAASamples == 16 ) && ( adapterCaps.m_VendorID == VENDORID_NVIDIA ) )
		{
			multiSampleType = ComputeMultisampleType(4);
			hr = D3D()->CheckDeviceMultiSampleType( nAdapter, DX8_DEVTYPE, 
				m_PresentParameters.BackBufferFormat, m_PresentParameters.Windowed, 
				multiSampleType, &nQualityLevel );						// 4x at highest quality level

			if ( !FAILED( hr ) && ( nQualityLevel == 16 ) )
			{
				nQualityLevel = nQualityLevel - 1;						// Highest quality level triggers 16x CSAA
			}
			else
			{
				nQualityLevel  = 0;										// No CSAA
			}
		}
		else	// Regular MSAA on any old vendor
		{
			hr = D3D()->CheckDeviceMultiSampleType( nAdapter, DX8_DEVTYPE, 
				m_PresentParameters.BackBufferFormat, m_PresentParameters.Windowed, 
				multiSampleType, &nQualityLevel );

			nQualityLevel = 0;
		}

		if ( !FAILED( hr ) )
		{
			m_PresentParameters.MultiSampleType = multiSampleType;
			m_PresentParameters.MultiSampleQuality = nQualityLevel;
		}
	}
	else
	{
		m_PresentParameters.MultiSampleType = D3DMULTISAMPLE_NONE;
		m_PresentParameters.MultiSampleQuality = 0;
	}
}


//-----------------------------------------------------------------------------
// Initializes, shuts down the D3D device
//-----------------------------------------------------------------------------
bool CShaderDeviceDx8::InitDevice( void* hwnd, int nAdapter, const ShaderDeviceInfo_t &info )
{
	//Debugger();
	
	// good place to run some self tests.
	//#if OSX
	//{
	//	extern void GLMgrSelfTests( void );
	//	GLMgrSelfTests();
	//}
	//#endif
	
	// windowed
	if ( !CreateD3DDevice( (VD3DHWND)hwnd, nAdapter, info ) )
		return false;

	// Hook up our own windows proc to get at messages to tell us when
	// other instances of the material system are trying to set the mode
	InstallWindowHook( (VD3DHWND)m_hWnd );
	return true;
}

void CShaderDeviceDx8::ShutdownDevice()
{
	if ( IsPC() && IsActive() )
	{
		Dx9Device()->Release();

#ifdef STUBD3D
		delete ( CStubD3DDevice * )Dx9Device();
#endif

		g_pD3DDevice = NULL;

		RemoveWindowHook( (VD3DHWND)m_hWnd );
		m_hWnd = 0;
	}
}


//-----------------------------------------------------------------------------
// Are we using graphics?
//-----------------------------------------------------------------------------
bool CShaderDeviceDx8::IsUsingGraphics() const
{
	//*****LOCK_SHADERAPI();
	return IsActive();
}


//-----------------------------------------------------------------------------
// Returns the current adapter in use
//-----------------------------------------------------------------------------
int CShaderDeviceDx8::GetCurrentAdapter() const
{
	LOCK_SHADERAPI();
	return m_DisplayAdapter;
}


//-----------------------------------------------------------------------------
// Returns the current adapter in use
//-----------------------------------------------------------------------------
char *CShaderDeviceDx8::GetDisplayDeviceName() 
{
	if( m_sDisplayDeviceName.IsEmpty() )
	{
		D3DADAPTER_IDENTIFIER9 ident;
		// On Win10, this function is getting called with m_nAdapter still initialized to -1.
		// It's failing, and m_sDisplayDeviceName has garbage, and tf2 fails to launch.
		// To repro this, run "hl2.exe -dev -fullscreen -game tf" on Win10.
		HRESULT hr = D3D()->GetAdapterIdentifier( Max( m_nAdapter, 0 ), 0, &ident );
		if ( FAILED(hr) )
		{
			Assert( false );
			ident.DeviceName[0] = 0;
		}
		m_sDisplayDeviceName = ident.DeviceName;
	}
	return m_sDisplayDeviceName.GetForModify();
}


//-----------------------------------------------------------------------------
// Use this to spew information about the 3D layer 
//-----------------------------------------------------------------------------
void CShaderDeviceDx8::SpewDriverInfo() const
{
	LOCK_SHADERAPI();
	HRESULT hr;
	D3DCAPS caps;
	D3DADAPTER_IDENTIFIER9 ident;

	RECORD_COMMAND( DX8_GET_DEVICE_CAPS, 0 );

	RECORD_COMMAND( DX8_GET_ADAPTER_IDENTIFIER, 2 );
	RECORD_INT( m_nAdapter );
	RECORD_INT( 0 );

	Dx9Device()->GetDeviceCaps( &caps );
	hr = D3D()->GetAdapterIdentifier( m_nAdapter, D3DENUM_WHQL_LEVEL, &ident );

	Warning("Shader API Driver Info:\n\nDriver : %s Version : %lld\n", 
		ident.Driver, ident.DriverVersion.QuadPart );
	Warning("Driver Description :  %s\n", ident.Description );
	Warning("Chipset version %d %d %d %d\n\n", 
		ident.VendorId, ident.DeviceId, ident.SubSysId, ident.Revision );

	ShaderDisplayMode_t mode;
	g_pShaderDeviceMgr->GetCurrentModeInfo( &mode, m_nAdapter );
	Warning("Display mode : %d x %d (%s)\n", 
		mode.m_nWidth, mode.m_nHeight, ImageLoader::GetName( mode.m_Format ) );
	Warning("Vertex Shader Version : %d.%d Pixel Shader Version : %d.%d\n",
		(caps.VertexShaderVersion >> 8) & 0xFF, caps.VertexShaderVersion & 0xFF,
		(caps.PixelShaderVersion >> 8) & 0xFF, caps.PixelShaderVersion & 0xFF);
	Warning("\nDevice Caps :\n");
	Warning("CANBLTSYSTONONLOCAL %s CANRENDERAFTERFLIP %s HWRASTERIZATION %s\n",
		(caps.DevCaps & D3DDEVCAPS_CANBLTSYSTONONLOCAL) ? " Y " : " N ",
		(caps.DevCaps & D3DDEVCAPS_CANRENDERAFTERFLIP) ? " Y " : " N ",
		(caps.DevCaps & D3DDEVCAPS_HWRASTERIZATION) ? " Y " : "*N*" );
	Warning("HWTRANSFORMANDLIGHT %s NPATCHES %s PUREDEVICE %s\n",
		(caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT) ? " Y " : " N ",
		(caps.DevCaps & D3DDEVCAPS_NPATCHES) ? " Y " : " N ",
		(caps.DevCaps & D3DDEVCAPS_PUREDEVICE) ? " Y " : " N " );
	Warning("SEPARATETEXTUREMEMORIES %s TEXTURENONLOCALVIDMEM %s TEXTURESYSTEMMEMORY %s\n",
		(caps.DevCaps & D3DDEVCAPS_SEPARATETEXTUREMEMORIES) ? "*Y*" : " N ",
		(caps.DevCaps & D3DDEVCAPS_TEXTURENONLOCALVIDMEM) ? " Y " : " N ",
		(caps.DevCaps & D3DDEVCAPS_TEXTURESYSTEMMEMORY) ? " Y " : " N " );
	Warning("TEXTUREVIDEOMEMORY %s TLVERTEXSYSTEMMEMORY %s TLVERTEXVIDEOMEMORY %s\n",
		(caps.DevCaps & D3DDEVCAPS_TEXTUREVIDEOMEMORY) ? " Y " : "*N*",
		(caps.DevCaps & D3DDEVCAPS_TLVERTEXSYSTEMMEMORY) ? " Y " : "*N*",
		(caps.DevCaps & D3DDEVCAPS_TLVERTEXVIDEOMEMORY) ? " Y " : " N " );

	Warning("\nPrimitive Caps :\n");
	Warning("BLENDOP %s CLIPPLANESCALEDPOINTS %s CLIPTLVERTS %s\n",
		(caps.PrimitiveMiscCaps & D3DPMISCCAPS_BLENDOP) ? " Y " : " N ",
		(caps.PrimitiveMiscCaps & D3DPMISCCAPS_CLIPPLANESCALEDPOINTS) ? " Y " : " N ",
		(caps.PrimitiveMiscCaps & D3DPMISCCAPS_CLIPTLVERTS) ? " Y " : " N " );
	Warning("COLORWRITEENABLE %s MASKZ %s TSSARGTEMP %s\n",
		(caps.PrimitiveMiscCaps & D3DPMISCCAPS_COLORWRITEENABLE) ? " Y " : " N ",
		(caps.PrimitiveMiscCaps & D3DPMISCCAPS_MASKZ) ? " Y " : "*N*",
		(caps.PrimitiveMiscCaps & D3DPMISCCAPS_TSSARGTEMP) ? " Y " : " N " );

	Warning("\nRaster Caps :\n");
	Warning("FOGRANGE %s FOGTABLE %s FOGVERTEX %s ZFOG %s WFOG %s\n",
		(caps.RasterCaps & D3DPRASTERCAPS_FOGRANGE) ? " Y " : " N ",
		(caps.RasterCaps & D3DPRASTERCAPS_FOGTABLE) ? " Y " : " N ",
		(caps.RasterCaps & D3DPRASTERCAPS_FOGVERTEX) ? " Y " : " N ",
		(caps.RasterCaps & D3DPRASTERCAPS_ZFOG) ? " Y " : " N ",
		(caps.RasterCaps & D3DPRASTERCAPS_WFOG) ? " Y " : " N " );
	Warning("MIPMAPLODBIAS %s WBUFFER %s ZBIAS %s ZTEST %s\n",
		(caps.RasterCaps & D3DPRASTERCAPS_MIPMAPLODBIAS) ? " Y " : " N ",
		(caps.RasterCaps & D3DPRASTERCAPS_WBUFFER) ? " Y " : " N ",
		(caps.RasterCaps & D3DPRASTERCAPS_DEPTHBIAS) ? " Y " : " N ",
		(caps.RasterCaps & D3DPRASTERCAPS_ZTEST) ? " Y " : "*N*" );

	Warning("Size of Texture Memory : %d kb\n", g_pHardwareConfig->Caps().m_TextureMemorySize / 1024 );
	Warning("Max Texture Dimensions : %d x %d\n", 
		caps.MaxTextureWidth, caps.MaxTextureHeight );
	if (caps.MaxTextureAspectRatio != 0)
		Warning("Max Texture Aspect Ratio : *%d*\n", caps.MaxTextureAspectRatio );
	Warning("Max Textures : %d Max Stages : %d\n", 
		caps.MaxSimultaneousTextures, caps.MaxTextureBlendStages );

	Warning("\nTexture Caps :\n");
	Warning("ALPHA %s CUBEMAP %s MIPCUBEMAP %s SQUAREONLY %s\n",
		(caps.TextureCaps & D3DPTEXTURECAPS_ALPHA) ? " Y " : " N ",
		(caps.TextureCaps & D3DPTEXTURECAPS_CUBEMAP) ? " Y " : " N ",
		(caps.TextureCaps & D3DPTEXTURECAPS_MIPCUBEMAP) ? " Y " : " N ",
		(caps.TextureCaps & D3DPTEXTURECAPS_SQUAREONLY) ? "*Y*" : " N " );

	Warning( "vendor id: 0x%x\n", g_pHardwareConfig->ActualCaps().m_VendorID );
	Warning( "device id: 0x%x\n", g_pHardwareConfig->ActualCaps().m_DeviceID );

	Warning( "SHADERAPI CAPS:\n" );
	Warning( "m_NumSamplers: %d\n", g_pHardwareConfig->Caps().m_NumSamplers );
	Warning( "m_NumTextureStages: %d\n", g_pHardwareConfig->Caps().m_NumTextureStages );
	Warning( "m_HasSetDeviceGammaRamp: %s\n", g_pHardwareConfig->Caps().m_HasSetDeviceGammaRamp ? "yes" : "no" );
	Warning( "m_SupportsVertexShaders (1.1): %s\n", g_pHardwareConfig->Caps().m_SupportsVertexShaders ? "yes" : "no" );
	Warning( "m_SupportsVertexShaders_2_0: %s\n", g_pHardwareConfig->Caps().m_SupportsVertexShaders_2_0 ? "yes" : "no" );
	Warning( "m_SupportsPixelShaders (1.1): %s\n", g_pHardwareConfig->Caps().m_SupportsPixelShaders ? "yes" : "no" );
	Warning( "m_SupportsPixelShaders_1_4: %s\n", g_pHardwareConfig->Caps().m_SupportsPixelShaders_1_4 ? "yes" : "no" );
	Warning( "m_SupportsPixelShaders_2_0: %s\n", g_pHardwareConfig->Caps().m_SupportsPixelShaders_2_0 ? "yes" : "no" );
	Warning( "m_SupportsPixelShaders_2_b: %s\n", g_pHardwareConfig->Caps().m_SupportsPixelShaders_2_b ? "yes" : "no" );
	Warning( "m_SupportsShaderModel_3_0: %s\n", g_pHardwareConfig->Caps().m_SupportsShaderModel_3_0 ? "yes" : "no" );
	
	switch( g_pHardwareConfig->Caps().m_SupportsCompressedTextures )
	{
	case COMPRESSED_TEXTURES_ON:
		Warning( "m_SupportsCompressedTextures: COMPRESSED_TEXTURES_ON\n" );
		break;
	case COMPRESSED_TEXTURES_OFF:
		Warning( "m_SupportsCompressedTextures: COMPRESSED_TEXTURES_ON\n" );
		break;
	case COMPRESSED_TEXTURES_NOT_INITIALIZED:
		Warning( "m_SupportsCompressedTextures: COMPRESSED_TEXTURES_NOT_INITIALIZED\n" );
		break;
	default:
		Assert( 0 );
		break;
	}
	Warning( "m_SupportsCompressedVertices: %d\n", g_pHardwareConfig->Caps().m_SupportsCompressedVertices );
	Warning( "m_bSupportsAnisotropicFiltering: %s\n", g_pHardwareConfig->Caps().m_bSupportsAnisotropicFiltering ? "yes" : "no" );
	Warning( "m_nMaxAnisotropy: %d\n", g_pHardwareConfig->Caps().m_nMaxAnisotropy );
	Warning( "m_MaxTextureWidth: %d\n", g_pHardwareConfig->Caps().m_MaxTextureWidth );
	Warning( "m_MaxTextureHeight: %d\n", g_pHardwareConfig->Caps().m_MaxTextureHeight );
	Warning( "m_MaxTextureAspectRatio: %d\n", g_pHardwareConfig->Caps().m_MaxTextureAspectRatio );
	Warning( "m_MaxPrimitiveCount: %d\n", g_pHardwareConfig->Caps().m_MaxPrimitiveCount );
	Warning( "m_ZBiasAndSlopeScaledDepthBiasSupported: %s\n", g_pHardwareConfig->Caps().m_ZBiasAndSlopeScaledDepthBiasSupported ? "yes" : "no" );
	Warning( "m_SupportsMipmapping: %s\n", g_pHardwareConfig->Caps().m_SupportsMipmapping ? "yes" : "no" );
	Warning( "m_SupportsOverbright: %s\n", g_pHardwareConfig->Caps().m_SupportsOverbright ? "yes" : "no" );
	Warning( "m_SupportsCubeMaps: %s\n", g_pHardwareConfig->Caps().m_SupportsCubeMaps ? "yes" : "no" );
	Warning( "m_NumPixelShaderConstants: %d\n", g_pHardwareConfig->Caps().m_NumPixelShaderConstants );
	Warning( "m_NumVertexShaderConstants: %d\n", g_pHardwareConfig->Caps().m_NumVertexShaderConstants );
	Warning( "m_NumBooleanVertexShaderConstants: %d\n", g_pHardwareConfig->Caps().m_NumBooleanVertexShaderConstants );
	Warning( "m_NumIntegerVertexShaderConstants: %d\n", g_pHardwareConfig->Caps().m_NumIntegerVertexShaderConstants );
	Warning( "m_TextureMemorySize: %d\n", g_pHardwareConfig->Caps().m_TextureMemorySize );
	Warning( "m_MaxNumLights: %d\n", g_pHardwareConfig->Caps().m_MaxNumLights );
	Warning( "m_SupportsHardwareLighting: %s\n", g_pHardwareConfig->Caps().m_SupportsHardwareLighting ? "yes" : "no" );
	Warning( "m_MaxBlendMatrices: %d\n", g_pHardwareConfig->Caps().m_MaxBlendMatrices );
	Warning( "m_MaxBlendMatrixIndices: %d\n", g_pHardwareConfig->Caps().m_MaxBlendMatrixIndices );
	Warning( "m_MaxVertexShaderBlendMatrices: %d\n", g_pHardwareConfig->Caps().m_MaxVertexShaderBlendMatrices );
	Warning( "m_SupportsMipmappedCubemaps: %s\n", g_pHardwareConfig->Caps().m_SupportsMipmappedCubemaps ? "yes" : "no" );
	Warning( "m_SupportsNonPow2Textures: %s\n", g_pHardwareConfig->Caps().m_SupportsNonPow2Textures ? "yes" : "no" );
	Warning( "m_nDXSupportLevel: %d\n", g_pHardwareConfig->Caps().m_nDXSupportLevel );
	Warning( "m_PreferDynamicTextures: %s\n", g_pHardwareConfig->Caps().m_PreferDynamicTextures ? "yes" : "no" );
	Warning( "m_HasProjectedBumpEnv: %s\n", g_pHardwareConfig->Caps().m_HasProjectedBumpEnv ? "yes" : "no" );
	Warning( "m_MaxUserClipPlanes: %d\n", g_pHardwareConfig->Caps().m_MaxUserClipPlanes );
	Warning( "m_SupportsSRGB: %s\n", g_pHardwareConfig->Caps().m_SupportsSRGB ? "yes" : "no" );
	switch( g_pHardwareConfig->Caps().m_HDRType )
	{
	case HDR_TYPE_NONE:
		Warning( "m_HDRType: HDR_TYPE_NONE\n" );
		break;
	case HDR_TYPE_INTEGER:
		Warning( "m_HDRType: HDR_TYPE_INTEGER\n" );
		break;
	case HDR_TYPE_FLOAT:
		Warning( "m_HDRType: HDR_TYPE_FLOAT\n" );
		break;
	default:
		Assert( 0 );
		break;
	}
	Warning( "m_bSupportsSpheremapping: %s\n", g_pHardwareConfig->Caps().m_bSupportsSpheremapping ? "yes" : "no" );
	Warning( "m_UseFastClipping: %s\n", g_pHardwareConfig->Caps().m_UseFastClipping ? "yes" : "no" );
	Warning( "m_pShaderDLL: %s\n", g_pHardwareConfig->Caps().m_pShaderDLL );
	Warning( "m_bNeedsATICentroidHack: %s\n", g_pHardwareConfig->Caps().m_bNeedsATICentroidHack ? "yes" : "no" );
	Warning( "m_bDisableShaderOptimizations: %s\n", g_pHardwareConfig->Caps().m_bDisableShaderOptimizations ? "yes" : "no" );
	Warning( "m_bColorOnSecondStream: %s\n", g_pHardwareConfig->Caps().m_bColorOnSecondStream ? "yes" : "no" );
	Warning( "m_MaxSimultaneousRenderTargets: %d\n", g_pHardwareConfig->Caps().m_MaxSimultaneousRenderTargets );
}


//-----------------------------------------------------------------------------
// Back buffer information
//-----------------------------------------------------------------------------
ImageFormat CShaderDeviceDx8::GetBackBufferFormat() const
{
	return ImageLoader::D3DFormatToImageFormat( m_PresentParameters.BackBufferFormat );
}

void CShaderDeviceDx8::GetBackBufferDimensions( int& width, int& height ) const
{
	width = m_PresentParameters.BackBufferWidth;
	height = m_PresentParameters.BackBufferHeight;
}


//-----------------------------------------------------------------------------
// Detects support for CreateQuery
//-----------------------------------------------------------------------------
void CShaderDeviceDx8::DetectQuerySupport( IDirect3DDevice9 *pD3DDevice )
{
	// Do I need to detect whether this device supports CreateQuery before creating it?
	if ( m_DeviceSupportsCreateQuery != -1 )
		return;

	IDirect3DQuery9 *pQueryObject = NULL;

	// Detect whether query is supported by creating and releasing:
	HRESULT hr = pD3DDevice->CreateQuery( D3DQUERYTYPE_EVENT, &pQueryObject );
	if ( !FAILED(hr) && pQueryObject ) 
	{
		pQueryObject->Release();
		m_DeviceSupportsCreateQuery = 1;
	} 
	else
	{
		m_DeviceSupportsCreateQuery = 0;
	}
}


const char *GetD3DErrorText( HRESULT hr )
{
	const char *pszMoreInfo = NULL;

#if defined( _WIN32 ) && !defined(DX_TO_GL_ABSTRACTION)
	switch ( hr )
	{
	case D3DERR_WRONGTEXTUREFORMAT:
		pszMoreInfo = "D3DERR_WRONGTEXTUREFORMAT: The pixel format of the texture surface is not valid.";
		break;
	case D3DERR_UNSUPPORTEDCOLOROPERATION:
		pszMoreInfo = "D3DERR_UNSUPPORTEDCOLOROPERATION: The device does not support a specified texture-blending operation for color values.";
		break;
	case D3DERR_UNSUPPORTEDCOLORARG:
		pszMoreInfo = "D3DERR_UNSUPPORTEDCOLORARG: The device does not support a specified texture-blending argument for color values.";
		break;
	case D3DERR_UNSUPPORTEDALPHAOPERATION:
		pszMoreInfo = "D3DERR_UNSUPPORTEDALPHAOPERATION: The device does not support a specified texture-blending operation for the alpha channel.";
		break;
	case D3DERR_UNSUPPORTEDALPHAARG:
		pszMoreInfo = "D3DERR_UNSUPPORTEDALPHAARG: The device does not support a specified texture-blending argument for the alpha channel.";
		break;
	case D3DERR_TOOMANYOPERATIONS:
		pszMoreInfo = "D3DERR_TOOMANYOPERATIONS: The application is requesting more texture-filtering operations than the device supports.";
		break;
	case D3DERR_CONFLICTINGTEXTUREFILTER:
		pszMoreInfo = "D3DERR_CONFLICTINGTEXTUREFILTER: The current texture filters cannot be used together.";
		break;
	case D3DERR_UNSUPPORTEDFACTORVALUE:
		pszMoreInfo = "D3DERR_UNSUPPORTEDFACTORVALUE: The device does not support the specified texture factor value.";
		break;
	case D3DERR_CONFLICTINGRENDERSTATE:
		pszMoreInfo = "D3DERR_CONFLICTINGRENDERSTATE: The currently set render states cannot be used together.";
		break;
	case D3DERR_UNSUPPORTEDTEXTUREFILTER:
		pszMoreInfo = "D3DERR_UNSUPPORTEDTEXTUREFILTER: The device does not support the specified texture filter.";
		break;
	case D3DERR_CONFLICTINGTEXTUREPALETTE:
		pszMoreInfo = "D3DERR_CONFLICTINGTEXTUREPALETTE: The current textures cannot be used simultaneously.";
		break;
	case D3DERR_DRIVERINTERNALERROR:
		pszMoreInfo = "D3DERR_DRIVERINTERNALERROR: Internal driver error.";
		break;
	case D3DERR_NOTFOUND:
		pszMoreInfo = "D3DERR_NOTFOUND: The requested item was not found.";
		break;
	case D3DERR_DEVICELOST:
		pszMoreInfo = "D3DERR_DEVICELOST: The device has been lost but cannot be reset at this time. Therefore, rendering is not possible.";
		break;
	case D3DERR_DEVICENOTRESET:
		pszMoreInfo = "D3DERR_DEVICENOTRESET: The device has been lost.";
		break;
	case D3DERR_NOTAVAILABLE:
		pszMoreInfo = "D3DERR_NOTAVAILABLE: This device does not support the queried technique.";
		break;
	case D3DERR_OUTOFVIDEOMEMORY:
		pszMoreInfo = "D3DERR_OUTOFVIDEOMEMORY: Direct3D does not have enough display memory to perform the operation. The device is using more resources in a single scene than can fit simultaneously into video memory.";
		break;
	case D3DERR_INVALIDDEVICE:
		pszMoreInfo = "D3DERR_INVALIDDEVICE: The requested device type is not valid.";
		break;
	case D3DERR_INVALIDCALL:
		pszMoreInfo = "D3DERR_INVALIDCALL: The method call is invalid.";
		break;
	case D3DERR_DRIVERINVALIDCALL:
		pszMoreInfo = "D3DERR_DRIVERINVALIDCALL";
		break;
	case D3DERR_WASSTILLDRAWING:
		pszMoreInfo = "D3DERR_WASSTILLDRAWING: The previous blit operation that is transferring information to or from this surface is incomplete.";
		break;
	}
#endif // _WIN32

	return pszMoreInfo;
}


//-----------------------------------------------------------------------------
// Actually creates the D3D Device once the present parameters are set up
//-----------------------------------------------------------------------------
IDirect3DDevice9* CShaderDeviceDx8::InvokeCreateDevice( void* hWnd, int nAdapter, DWORD deviceCreationFlags )
{
	IDirect3DDevice9 *pD3DDevice = NULL;
	D3DDEVTYPE devType = DX8_DEVTYPE;

#if NVPERFHUD
	nAdapter = D3D()->GetAdapterCount()-1;
	devType = D3DDEVTYPE_REF;
	deviceCreationFlags = D3DCREATE_FPU_PRESERVE | D3DCREATE_HARDWARE_VERTEXPROCESSING;
#endif

#if 1	// with the changes for opengl to enable threading, we no longer need the d3d device to have threading guards
#ifndef _X360
	// Create the device with multi-threaded safeguards if we're using mat_queue_mode 2.
	// The logic to enable multithreaded rendering happens well after the device has been created, 
	// so we replicate some of that logic here.
	ConVarRef mat_queue_mode( "mat_queue_mode" );
	if ( mat_queue_mode.GetInt() == 2 ||
		 ( mat_queue_mode.GetInt() == -2 && GetCPUInformation()->m_nPhysicalProcessors >= 2 ) ||
	     ( mat_queue_mode.GetInt() == -1 && GetCPUInformation()->m_nPhysicalProcessors >= 2 ) )
	{
		deviceCreationFlags |= D3DCREATE_MULTITHREADED;
	}
#endif
#endif

#ifdef ENABLE_NULLREF_DEVICE_SUPPORT
	devType =  CommandLine()->FindParm( "-nulldevice" ) ? D3DDEVTYPE_NULLREF: devType;
#endif

	HRESULT hr = D3D()->CreateDevice( nAdapter, devType,
		(VD3DHWND)hWnd, deviceCreationFlags, &m_PresentParameters, &pD3DDevice );

	if ( !FAILED( hr ) && pD3DDevice )
		return pD3DDevice;

	if ( !IsPC() )
		return NULL;

	// try again, other applications may be taking their time
	Sleep( 1000 );
	hr = D3D()->CreateDevice( nAdapter, devType,
		(VD3DHWND)hWnd, deviceCreationFlags, &m_PresentParameters, &pD3DDevice );
	if ( !FAILED( hr ) && pD3DDevice )
		return pD3DDevice;

	// in this case, we actually are allocating too much memory....
	// This will cause us to use less buffers...
	if ( m_PresentParameters.Windowed )
	{
		m_PresentParameters.SwapEffect = D3DSWAPEFFECT_COPY; 
		m_PresentParameters.BackBufferCount = 0;
		hr = D3D()->CreateDevice( nAdapter, devType,
			(VD3DHWND)hWnd, deviceCreationFlags, &m_PresentParameters, &pD3DDevice );
	}
	if ( !FAILED( hr ) && pD3DDevice )
		return pD3DDevice;

	const char *pszMoreInfo = NULL;
	switch ( hr )
	{
#ifdef _WIN32
	case D3DERR_INVALIDCALL:
		// Override the error text for this error since it has a known meaning for CreateDevice failures.
		pszMoreInfo = "D3DERR_INVALIDCALL: The device or the device driver may not support Direct3D or may not support the resolution or color depth specified.";
		break;
#endif // _WIN32
	default:
		pszMoreInfo = GetD3DErrorText( hr );
		break;
	}

	// Otherwise we failed, show a message and shutdown
	if ( pszMoreInfo )
	{
		DWarning( "init", 0, "Failed to create %s device!\nError 0x%lX: %s\n\nPlease see the following for more info.\n"
			"http://support.steampowered.com/cgi-bin/steampowered.cfg/php/enduser/std_adp.php?p_faqid=772\n", IsOpenGL() ? "OpenGL" : "D3D", hr, pszMoreInfo );
	}
	else
	{
		DWarning( "init", 0, "Failed to create %s device!\nError 0x%lX.\n\nPlease see the following for more info.\n"
			"http://support.steampowered.com/cgi-bin/steampowered.cfg/php/enduser/std_adp.php?p_faqid=772\n", IsOpenGL() ? "OpenGL" : "D3D", hr );
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Creates the D3D Device
//-----------------------------------------------------------------------------
bool CShaderDeviceDx8::CreateD3DDevice( void* pHWnd, int nAdapter, const ShaderDeviceInfo_t &info )
{
	Assert( info.m_nVersion == SHADER_DEVICE_INFO_VERSION );

	MEM_ALLOC_CREDIT_( __FILE__ ": D3D Device" );

	VD3DHWND hWnd = (VD3DHWND)pHWnd;

#if ( !defined( PIX_INSTRUMENTATION ) && !defined( _X360 ) && !defined( NVPERFHUD ) )
	D3DPERF_SetOptions(1);	// Explicitly disallow PIX instrumented profiling in external builds
#endif

	// Get some caps....
	D3DCAPS caps;
	HRESULT hr = D3D()->GetDeviceCaps( nAdapter, DX8_DEVTYPE, &caps );
	if ( FAILED( hr ) )
		return false;

	// Determine the adapter format
	ShaderDisplayMode_t mode;
	g_pShaderDeviceMgrDx8->GetCurrentModeInfo( &mode, nAdapter );
	m_AdapterFormat = mode.m_Format;

	// FIXME: Need to do this prior to SetPresentParameters. Fix.
	// Make it part of HardwareCaps_t
	InitializeColorInformation( nAdapter, DX8_DEVTYPE, m_AdapterFormat );

	const HardwareCaps_t &adapterCaps = g_ShaderDeviceMgrDx8.GetHardwareCaps( nAdapter );
	DWORD deviceCreationFlags = ComputeDeviceCreationFlags( caps, adapterCaps.m_bSoftwareVertexProcessing );
	SetPresentParameters( hWnd, nAdapter, info );

	// Tell all other instances of the material system to let go of memory
	SendIPCMessage( RELEASE_MESSAGE );

	// Creates the device
	IDirect3DDevice9 *pD3DDevice = InvokeCreateDevice( pHWnd, nAdapter, deviceCreationFlags );

	if ( !pD3DDevice )
		return false;

	// Check to see if query is supported
	DetectQuerySupport( pD3DDevice );

#ifdef STUBD3D
	Dx9Device() = new CStubD3DDevice( pD3DDevice, g_pFullFileSystem );
#else
	g_pD3DDevice = pD3DDevice;
#endif

#if defined( _X360 )
	// Create the depth buffer, created manually to enable hierarchical z
	{
		D3DSURFACE_PARAMETERS DepthStencilParams;

		// Depth is immediately after the back buffer in EDRAM
		// allocate the hierarchical z tiles at the end of the area so all other allocations can trivially allocate at 0
		DepthStencilParams.Base = XGSurfaceSize( 
			m_PresentParameters.BackBufferWidth,
			m_PresentParameters.BackBufferHeight, 
			m_PresentParameters.BackBufferFormat, 
			m_PresentParameters.MultiSampleType );
		DepthStencilParams.ColorExpBias = 0;
		DepthStencilParams.HierarchicalZBase = GPU_HIERARCHICAL_Z_TILES - XGHierarchicalZSize( m_PresentParameters.BackBufferWidth, m_PresentParameters.BackBufferHeight, m_PresentParameters.MultiSampleType );

		IDirect3DSurface *pDepthStencilSurface = NULL;
		hr = Dx9Device()->CreateDepthStencilSurface( 
			m_PresentParameters.BackBufferWidth, 
			m_PresentParameters.BackBufferHeight, 
			m_PresentParameters.AutoDepthStencilFormat,
			m_PresentParameters.MultiSampleType, 
			m_PresentParameters.MultiSampleQuality,
			TRUE,
			&pDepthStencilSurface,
			&DepthStencilParams );
		Assert( SUCCEEDED( hr ) );
		if ( FAILED( hr ) )
			return false;

		hr = Dx9Device()->SetDepthStencilSurface( pDepthStencilSurface );
		Assert( SUCCEEDED( hr ) );
		if ( FAILED( hr ) )
			return false;
	}

	// Initialize XUI, needed for TTF font rasterization
	// xui requires and shares our d3d device
	{
		hr = XuiRenderInitShared( pD3DDevice, &m_PresentParameters, XuiD3DXTextureLoader );
		if ( FAILED( hr ) )
			return false;

		XUIInitParams xuiInit;
		XUI_INIT_PARAMS( xuiInit );
		xuiInit.dwFlags = XUI_INIT_PARAMS_FLAGS_NONE;
		xuiInit.pHooks = NULL;
		hr = XuiInit( &xuiInit );
		if ( FAILED( hr ) )
			return false;

		hr = XuiRenderCreateDC( &m_hDC );
		if ( FAILED( hr ) )
			return false;
	}
#endif

	// CheckDeviceLost();

	// Tell all other instances of the material system it's ok to grab memory
	SendIPCMessage( REACQUIRE_MESSAGE );

	m_hWnd = pHWnd;
	m_nAdapter = m_DisplayAdapter = nAdapter;
	m_DeviceState = DEVICE_STATE_OK;
	m_bIsMinimized = false;
	m_bQueuedDeviceLost = false;

	m_IsResizing = info.m_bWindowed && info.m_bResizing;

	// This is our current view.
	m_ViewHWnd = hWnd;
	GetWindowSize( m_nWindowWidth, m_nWindowHeight );

	g_pHardwareConfig->SetupHardwareCaps( info, g_ShaderDeviceMgrDx8.GetHardwareCaps( nAdapter ) );

	// FIXME: Bake this into hardware config
	// What texture formats do we support?
	if ( D3DSupportsCompressedTextures() )
	{
		g_pHardwareConfig->ActualCapsForEdit().m_SupportsCompressedTextures = COMPRESSED_TEXTURES_ON;
		g_pHardwareConfig->CapsForEdit().m_SupportsCompressedTextures = COMPRESSED_TEXTURES_ON;
	}
	else
	{
		g_pHardwareConfig->ActualCapsForEdit().m_SupportsCompressedTextures = COMPRESSED_TEXTURES_OFF;
		g_pHardwareConfig->CapsForEdit().m_SupportsCompressedTextures = COMPRESSED_TEXTURES_OFF;
	}

	return ( !FAILED( hr ) );
}


//-----------------------------------------------------------------------------
// Frame sync
//-----------------------------------------------------------------------------
void CShaderDeviceDx8::AllocFrameSyncTextureObject()
{
	if ( IsX360() )
		return;

	FreeFrameSyncTextureObject();

	// Create a tiny managed texture.
	HRESULT hr = Dx9Device()->CreateTexture( 
		1, 1,	// width, height
		0,		// levels
		D3DUSAGE_DYNAMIC,	// usage
		D3DFMT_A8R8G8B8,	// format
		D3DPOOL_DEFAULT,
		&m_pFrameSyncTexture,
		NULL );
	if ( FAILED( hr ) )
	{
		m_pFrameSyncTexture = NULL;
	}
}

void CShaderDeviceDx8::FreeFrameSyncTextureObject()
{
	if ( IsX360() )
		return;

	if ( m_pFrameSyncTexture )
	{
		m_pFrameSyncTexture->Release();
		m_pFrameSyncTexture = NULL;
	}
}

void CShaderDeviceDx8::AllocFrameSyncObjects( void )
{
	if ( IsX360() )
		return;

	if ( mat_debugalttab.GetBool() )
	{
		Warning( "mat_debugalttab: CShaderAPIDX8::AllocFrameSyncObjects\n" );
	}

	// Allocate the texture for frame syncing in case we force that to be on.
	AllocFrameSyncTextureObject();

	if ( m_DeviceSupportsCreateQuery == 0 )
	{
		for ( int i = 0; i < ARRAYSIZE(m_pFrameSyncQueryObject); i++ )
		{
			m_pFrameSyncQueryObject[i] = NULL;
			m_bQueryIssued[i] = false;
		}
		return;
	}

	// FIXME FIXME FIXME!!!!!  Need to record this.
	for ( int i = 0; i < ARRAYSIZE(m_pFrameSyncQueryObject); i++ )
	{
		HRESULT hr = Dx9Device()->CreateQuery( D3DQUERYTYPE_EVENT, &m_pFrameSyncQueryObject[i] );
		if( hr == D3DERR_NOTAVAILABLE )
		{
			Warning( "D3DQUERYTYPE_EVENT not available on this driver\n" );
			Assert( m_pFrameSyncQueryObject[i] == NULL );
		}
		else
		{
			Assert( hr == D3D_OK );
			Assert( m_pFrameSyncQueryObject[i] );
			m_pFrameSyncQueryObject[i]->Issue( D3DISSUE_END );
			m_bQueryIssued[i] = true;
		}
	}
}

void CShaderDeviceDx8::FreeFrameSyncObjects( void )
{
	if ( IsX360() )
		return;

	if ( mat_debugalttab.GetBool() )
	{
		Warning( "mat_debugalttab: CShaderAPIDX8::FreeFrameSyncObjects\n" );
	}

	FreeFrameSyncTextureObject();

	// FIXME FIXME FIXME!!!!!  Need to record this.
	for ( int i = 0; i < ARRAYSIZE(m_pFrameSyncQueryObject); i++ )
	{
		if ( m_pFrameSyncQueryObject[i] )
		{
			if ( m_bQueryIssued[i] )
			{
				tmZone( TELEMETRY_LEVEL1, TMZF_NONE, "D3DQueryGetData %t", tmSendCallStack( TELEMETRY_LEVEL0, 0 ) );

				double flStartTime = Plat_FloatTime();
				BOOL dummyData = 0;
				HRESULT hr = S_OK;

				// Make every attempt (within 2 seconds) to get the result from the query.  Doing so may prevent
				// crashes in the driver if we try to release outstanding queries.
				do
				{
					hr = m_pFrameSyncQueryObject[i]->GetData( &dummyData, sizeof( dummyData ), D3DGETDATA_FLUSH );
					double flCurrTime = Plat_FloatTime();

					// don't wait more than 2 seconds for these
					if ( flCurrTime - flStartTime > 2.00 )
						break;
				} while ( hr == S_FALSE );
			}
#ifdef DBGFLAG_ASSERT
			int nRetVal = 
#endif
			m_pFrameSyncQueryObject[i]->Release();
			Assert( nRetVal == 0 );
			m_pFrameSyncQueryObject[i] = NULL;
			m_bQueryIssued[i] = false;
		}
	}
}


//-----------------------------------------------------------------------------
// Occurs when another application is initializing
//-----------------------------------------------------------------------------
void CShaderDeviceDx8::OtherAppInitializing( bool initializing )
{
	if ( !ThreadOwnsDevice() || !ThreadInMainThread() )
	{
		if ( initializing )
		{
			ShaderUtil()->OnThreadEvent( SHADER_THREAD_OTHER_APP_START );
		}
		else
		{
			ShaderUtil()->OnThreadEvent( SHADER_THREAD_OTHER_APP_END );
		}
		return;
	}
	Assert( m_bOtherAppInitializing != initializing );

	if ( !IsDeactivated() )
	{
		Dx9Device()->EndScene();
	}

	// NOTE: OtherApp is set in this way because we need to know we're
	// active as we release and restore everything
	CheckDeviceLost( initializing );

	if ( !IsDeactivated() )
	{
		Dx9Device()->BeginScene();
	}
}


void CShaderDeviceDx8::HandleThreadEvent( uint32 threadEvent )
{
	Assert(ThreadOwnsDevice());
	switch ( threadEvent )
	{
	case SHADER_THREAD_OTHER_APP_START:
		OtherAppInitializing(true);
		break;
	case SHADER_THREAD_RELEASE_RESOURCES:
		ReleaseResources();
		break;
	case SHADER_THREAD_EVICT_RESOURCES:
		EvictManagedResourcesInternal();
		break;
	case SHADER_THREAD_RESET_RENDER_STATE:
		ResetRenderState();
		break;
	case SHADER_THREAD_ACQUIRE_RESOURCES:
		ReacquireResources();
		break;
	case SHADER_THREAD_OTHER_APP_END:
		OtherAppInitializing(false);
		break;

	}
}

//-----------------------------------------------------------------------------
// We lost the device, but we have a chance to recover
//-----------------------------------------------------------------------------
bool CShaderDeviceDx8::TryDeviceReset()
{
	if ( IsX360() )
		return true;

	// Don't try to reset the device until we're sure our resources have been released
	if ( !m_bResourcesReleased )
	{
		return false;
	}

	// FIXME: Make this rebuild the Dx9Device from scratch!
	// Helps with compatibility
	HRESULT hr = Dx9Device()->Reset( &m_PresentParameters );
	bool bResetSuccess = !FAILED(hr);

#if defined(IS_WINDOWS_PC) && defined(SHADERAPIDX9)
	if ( bResetSuccess && g_ShaderDeviceUsingD3D9Ex )
	{
		bResetSuccess = SUCCEEDED( Dx9Device()->TestCooperativeLevel() );
		if ( bResetSuccess )
		{
			Warning("video driver has crashed and been reset, re-uploading resources now");
		}
	}
#endif

	if ( bResetSuccess )
		m_bResourcesReleased = false;
	return bResetSuccess;
}


//-----------------------------------------------------------------------------
// Release, reacquire resources
//-----------------------------------------------------------------------------
void CShaderDeviceDx8::ReleaseResources()
{
	if ( !ThreadOwnsDevice() || !ThreadInMainThread() )
	{
		// Set our resources as not being released yet.  
		// We reset this in two places since release resources can be called without a call to TryDeviceReset.
		m_bResourcesReleased = false;
		ShaderUtil()->OnThreadEvent( SHADER_THREAD_RELEASE_RESOURCES );
		return;
	}

	// Only the initial "ReleaseResources" actually has effect
	if ( m_numReleaseResourcesRefCount ++ != 0 )
	{
		Warning( "ReleaseResources has no effect, now at level %d.\n", m_numReleaseResourcesRefCount );
		DevWarning( "ReleaseResources called twice is a bug: use IsDeactivated to check for a valid device.\n" );
		Assert( 0 );
		return;
	}

	LOCK_SHADERAPI();
	CPixEvent( PIX_VALVE_ORANGE, "ReleaseResources" );

	FreeFrameSyncObjects();
	FreeNonInteractiveRefreshObjects();
	ShaderUtil()->ReleaseShaderObjects();
	MeshMgr()->ReleaseBuffers();
	g_pShaderAPI->ReleaseShaderObjects();

#ifdef _DEBUG
	if ( MeshMgr()->BufferCount() != 0 )
	{
		for( int i = 0; i < MeshMgr()->BufferCount(); i++ )
		{
		}
	}
#endif

	// All meshes cleaned up?
	Assert( MeshMgr()->BufferCount() == 0 );

	// Signal that our resources have been released so that we can try to reset the device
	m_bResourcesReleased = true;
}


void CShaderDeviceDx8::ReacquireResources()
{
	ReacquireResourcesInternal();
}

void CShaderDeviceDx8::ReacquireResourcesInternal( bool bResetState, bool bForceReacquire, char const *pszForceReason )
{
	if ( !ThreadOwnsDevice() || !ThreadInMainThread() )
	{
		if ( bResetState )
		{
			ShaderUtil()->OnThreadEvent( SHADER_THREAD_RESET_RENDER_STATE );
		}
		ShaderUtil()->OnThreadEvent( SHADER_THREAD_ACQUIRE_RESOURCES );
		return;
	}
	if ( bForceReacquire )
	{
		// If we are forcing reacquire then warn if release calls are remaining unpaired
		if ( m_numReleaseResourcesRefCount > 1 )
		{
			Warning( "Forcefully resetting device (%s), resources release level was %d.\n", pszForceReason ? pszForceReason : "unspecified", m_numReleaseResourcesRefCount );
			Assert( 0 );
		}
		m_numReleaseResourcesRefCount = 0;
	}
	else
	{
		// Only the final "ReacquireResources" actually has effect
		if ( -- m_numReleaseResourcesRefCount != 0 )
		{
			Warning( "ReacquireResources has no effect, now at level %d.\n", m_numReleaseResourcesRefCount );
			DevWarning( "ReacquireResources being discarded is a bug: use IsDeactivated to check for a valid device.\n" );
			Assert( 0 );

			if ( m_numReleaseResourcesRefCount < 0 )
			{
				m_numReleaseResourcesRefCount = 0;
			}

			return;
		}
	}

	if ( bResetState )
	{
		ResetRenderState();
	}

	LOCK_SHADERAPI();
	CPixEvent event( PIX_VALVE_ORANGE, "ReacquireResources" );

	g_pShaderAPI->RestoreShaderObjects();
	AllocFrameSyncObjects();
	AllocNonInteractiveRefreshObjects();
	MeshMgr()->RestoreBuffers();
	ShaderUtil()->RestoreShaderObjects( CShaderDeviceMgrBase::ShaderInterfaceFactory );
}


//-----------------------------------------------------------------------------
// Changes the window size
//-----------------------------------------------------------------------------
bool CShaderDeviceDx8::ResizeWindow( const ShaderDeviceInfo_t &info ) 
{
	if ( IsX360() )
		return false;

	m_bPendingVideoModeChange = false;

	// We don't need to do crap if the window was set up to set up
	// to be resizing...
	if ( info.m_bResizing )
		return false;

	g_pShaderDeviceMgr->InvokeModeChangeCallbacks();

	ReleaseResources();

	SetPresentParameters( (VD3DHWND)m_hWnd, m_DisplayAdapter, info );
	HRESULT hr = Dx9Device()->Reset( &m_PresentParameters );
	if ( FAILED( hr ) )
	{
		Warning( "ResizeWindow: Reset failed, hr = 0x%08lX.\n", hr );
		return false;
	}
	else
	{
		ReacquireResourcesInternal( true, true, "ResizeWindow" );
	}

	return true;
}


//-----------------------------------------------------------------------------
// Queue up the fact that the device was lost
//-----------------------------------------------------------------------------
void CShaderDeviceDx8::MarkDeviceLost( )
{
	if ( IsX360() )
		return;

	m_bQueuedDeviceLost = true;
}


//-----------------------------------------------------------------------------
// Checks if the device was lost
//-----------------------------------------------------------------------------
#if defined( _DEBUG ) && !defined( _X360 )
ConVar mat_forcelostdevice( "mat_forcelostdevice", "0" );
#endif

void CShaderDeviceDx8::CheckDeviceLost( bool bOtherAppInitializing )
{
#if !defined( _X360 )
	// FIXME: We could also queue up if WM_SIZE changes and look at that
	// but that seems to only make sense if we have resizable windows where 
	// we do *not* allocate buffers as large as the entire current video mode
	// which we're not doing
#ifdef _WIN32
	m_bIsMinimized = ( static_cast<BOOL>(IsIconic( ( HWND )m_hWnd )) == (BOOL)TRUE );
#else
	m_bIsMinimized = ( IsIconic( (VD3DHWND)m_hWnd ) == TRUE );
#endif
	m_bOtherAppInitializing = bOtherAppInitializing;

#ifdef _DEBUG
	if ( mat_forcelostdevice.GetBool() )
	{
		mat_forcelostdevice.SetValue( 0 );
		MarkDeviceLost();
	}
#endif
	
	HRESULT hr = D3D_OK;
#if defined(IS_WINDOWS_PC) && defined(SHADERAPIDX9)
	if ( g_ShaderDeviceUsingD3D9Ex && m_DeviceState == DEVICE_STATE_OK )
	{
		// Steady state - PresentEx return value will mark us lost if necessary.
		// We do not care if we are minimized in this state.
		m_bIsMinimized = false; 
	}
	else
#endif
	{
		RECORD_COMMAND( DX8_TEST_COOPERATIVE_LEVEL, 0 );
		hr = Dx9Device()->TestCooperativeLevel();
	}

	// If some other call returned device lost previously in the frame, spoof the return value from TCL
	if ( m_bQueuedDeviceLost )
	{
		hr = (hr != D3D_OK) ? hr : D3DERR_DEVICENOTRESET;
		m_bQueuedDeviceLost = false;
	}

	if ( m_DeviceState == DEVICE_STATE_OK )
	{
		// We can transition out of ok if bOtherAppInitializing is set
		// or if we become minimized, or if TCL returns anything other than D3D_OK.
		if ( ( hr != D3D_OK ) || m_bIsMinimized )
		{
			// purge unreferenced materials
			g_pShaderUtil->UncacheUnusedMaterials( true );

			// We were ok, now we're not. Release resources
			ReleaseResources();
			m_DeviceState = DEVICE_STATE_LOST_DEVICE; 
		}
		else if ( bOtherAppInitializing )
		{
			// purge unreferenced materials
			g_pShaderUtil->UncacheUnusedMaterials( true );

			// We were ok, now we're not. Release resources
			ReleaseResources();
			m_DeviceState = DEVICE_STATE_OTHER_APP_INIT; 
		}
	}

	// Immediately checking devicelost after ok helps in the case where we got D3DERR_DEVICENOTRESET
	// in which case we want to immdiately try to switch out of DEVICE_STATE_LOST and into DEVICE_STATE_NEEDS_RESET
	if ( m_DeviceState == DEVICE_STATE_LOST_DEVICE )
	{
		// We can only try to reset if we're not minimized and not lost
		if ( !m_bIsMinimized && (hr != D3DERR_DEVICELOST) )
		{
			m_DeviceState = DEVICE_STATE_NEEDS_RESET; 
		}
	}

	// Immediately checking needs reset also helps for the case where we got D3DERR_DEVICENOTRESET
	if ( m_DeviceState == DEVICE_STATE_NEEDS_RESET )
	{
		if ( ( hr == D3DERR_DEVICELOST ) || m_bIsMinimized )
		{
			m_DeviceState = DEVICE_STATE_LOST_DEVICE; 
		}
		else
		{
			bool bResetSucceeded = TryDeviceReset();
			if ( bResetSucceeded )
			{
				if ( !bOtherAppInitializing	)
				{
					m_DeviceState = DEVICE_STATE_OK;

					// We were bad, now we're ok. Restore resources and reset render state.
					ReacquireResourcesInternal( true, true, "NeedsReset" );
				}
				else
				{
					m_DeviceState = DEVICE_STATE_OTHER_APP_INIT;
				}
			}
		}
	}

	if ( m_DeviceState == DEVICE_STATE_OTHER_APP_INIT )
	{
		if ( ( hr != D3D_OK ) || m_bIsMinimized )
		{
			m_DeviceState = DEVICE_STATE_LOST_DEVICE; 
		}
		else if ( !bOtherAppInitializing )
		{
			m_DeviceState = DEVICE_STATE_OK; 

			// We were bad, now we're ok. Restore resources and reset render state.
			ReacquireResourcesInternal( true, true, "OtherAppInit" );
		}
	}

	// Do mode change if we have a video mode change.
	if ( m_bPendingVideoModeChange && !IsDeactivated() )
	{
#ifdef _DEBUG
		Warning( "mode change!\n" );
#endif
		// now purge unreferenced materials
		g_pShaderUtil->UncacheUnusedMaterials( true );

		ResizeWindow( m_PendingVideoModeChangeConfig );
	}
#endif
}


//-----------------------------------------------------------------------------
// Special method to refresh the screen on the XBox360
//-----------------------------------------------------------------------------
bool CShaderDeviceDx8::AllocNonInteractiveRefreshObjects()
{
#if defined( _X360 )

	const char *strVertexShaderProgram = 
		" float4x4 matWVP : register(c0);"  
		" struct VS_IN"  
		" {" 
		" float4 ObjPos : POSITION;"
		" float2 TexCoord : TEXCOORD;"
		" };" 
		" struct VS_OUT" 
		" {" 
		" float4 ProjPos : POSITION;" 
		" float2 TexCoord : TEXCOORD;"
		" };"  
		" VS_OUT main( VS_IN In )"  
		" {"  
		" VS_OUT Out; "  
		" Out.ProjPos = mul( matWVP, In.ObjPos );"
		" Out.TexCoord = In.TexCoord;"
		" return Out;"  
		" }";

	const char *strPixelShaderProgram = 
		" struct PS_IN"
		" {"
		" float2 TexCoord : TEXCOORD;"
		" };"
		" sampler detail : register( s0 );"
		" float4 main( PS_IN In ) : COLOR"  
		" {"  
		" return tex2D( detail, In.TexCoord );"
		" }"; 

	const char *strPixelShaderProgram2 = 
		" struct PS_IN"
		" {"
		" float2 TexCoord : TEXCOORD;"
		" };"
		" sampler detail : register( s0 );"
		" float4 main( PS_IN In ) : COLOR"  
		" {"  
		" return tex2D( detail, In.TexCoord );"
		" }"; 

	const char *strPixelShaderProgram3 = 
		" struct PS_IN"
		" {"
		" float2 TexCoord : TEXCOORD;"
		" };"
		" float SrgbGammaToLinear( float flSrgbGammaValue )"
		" {"
		"	float x = saturate( flSrgbGammaValue );"
		"	return ( x <= 0.04045f ) ? ( x / 12.92f ) : ( pow( ( x + 0.055f ) / 1.055f, 2.4f ) );"
		" }"
		"float X360LinearToGamma( float flLinearValue )"
		"{"
		"		float fl360GammaValue;"
		""
		"	flLinearValue = saturate( flLinearValue );"
		"	if ( flLinearValue < ( 128.0f / 1023.0f ) )"
		"	{"
		"		if ( flLinearValue < ( 64.0f / 1023.0f ) )"
		"		{"
		"			fl360GammaValue = flLinearValue * ( 1023.0f * ( 1.0f / 255.0f ) );"
		"		}"
		"		else"
		"		{"
		"			fl360GammaValue = flLinearValue * ( ( 1023.0f / 2.0f ) * ( 1.0f / 255.0f ) ) + ( 32.0f / 255.0f );"
		"		}"
		"	}"
		"	else"
		"	{"
		"		if ( flLinearValue < ( 512.0f / 1023.0f ) )"
		"		{"
		"			fl360GammaValue = flLinearValue * ( ( 1023.0f / 4.0f ) * ( 1.0f / 255.0f ) ) + ( 64.0f / 255.0f );"
		"		}"
		"		else"
		"		{"
		"			fl360GammaValue = flLinearValue * ( ( 1023.0f /8.0f ) * ( 1.0f / 255.0f ) ) + ( 128.0f /255.0f );"
		"			if ( fl360GammaValue > 1.0f )"
		"			{"
		"				fl360GammaValue = 1.0f;"
		"			}"
		"		}"
		"	}"
		""
		"	fl360GammaValue = saturate( fl360GammaValue );"
		"	return fl360GammaValue;"
		"}"
		" sampler detail : register( s0 );"
		" float4 main( PS_IN In ) : COLOR"  
		" {"  
		"	float4 vTextureColor = tex2D( detail, In.TexCoord );"
		"	vTextureColor.r = X360LinearToGamma( SrgbGammaToLinear( vTextureColor.r ) );" 
		"	vTextureColor.g = X360LinearToGamma( SrgbGammaToLinear( vTextureColor.g ) );"
		"	vTextureColor.b = X360LinearToGamma( SrgbGammaToLinear( vTextureColor.b ) );"
		"	return vTextureColor;"
		" }"; 

	D3DVERTEXELEMENT9 VertexElements[4] =
	{
		{ 0,  0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
		{ 0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
		D3DDECL_END()
	};

	ID3DXBuffer *pErrorMsg = NULL;
	ID3DXBuffer *pShaderCode = NULL;

	HRESULT hr = D3DXCompileShader( strVertexShaderProgram, (UINT)strlen( strVertexShaderProgram ), NULL, NULL, "main", "vs_2_0", 0, &pShaderCode, &pErrorMsg, NULL );
	if ( FAILED( hr ) )
		return false;

	Dx9Device()->CreateVertexShader( (DWORD*)pShaderCode->GetBufferPointer(), &m_NonInteractiveRefresh.m_pVertexShader );
	pShaderCode->Release();
	pShaderCode = NULL;
	if ( pErrorMsg )
	{
		pErrorMsg->Release();
		pErrorMsg = NULL;
	}

	hr = D3DXCompileShader( strPixelShaderProgram, (UINT)strlen( strPixelShaderProgram ), NULL, NULL, "main", "ps_2_0", 0, &pShaderCode, &pErrorMsg, NULL );
	if ( FAILED(hr) )
		return false;

	Dx9Device()->CreatePixelShader( (DWORD*)pShaderCode->GetBufferPointer(), &m_NonInteractiveRefresh.m_pPixelShader );
	pShaderCode->Release();
	if ( pErrorMsg )
	{
		pErrorMsg->Release();
		pErrorMsg = NULL;
	}

	hr = D3DXCompileShader( strPixelShaderProgram3, (UINT)strlen( strPixelShaderProgram3 ), NULL, NULL, "main", "ps_2_0", 0, &pShaderCode, &pErrorMsg, NULL );
	if ( FAILED(hr) )
		return false;

	Dx9Device()->CreatePixelShader( (DWORD*)pShaderCode->GetBufferPointer(), &m_NonInteractiveRefresh.m_pPixelShaderStartup );
	pShaderCode->Release();
	if ( pErrorMsg )
	{
		pErrorMsg->Release();
		pErrorMsg = NULL;
	}

	hr = D3DXCompileShader( strPixelShaderProgram2, (UINT)strlen( strPixelShaderProgram2 ), NULL, NULL, "main", "ps_2_0", 0, &pShaderCode, &pErrorMsg, NULL );
	if ( FAILED(hr) )
		return false;

	Dx9Device()->CreatePixelShader( (DWORD*)pShaderCode->GetBufferPointer(), &m_NonInteractiveRefresh.m_pPixelShaderStartupPass2 );
	pShaderCode->Release();
	if ( pErrorMsg )
	{
		pErrorMsg->Release();
		pErrorMsg = NULL;
	}

	// Create a vertex declaration from the element descriptions.
	Dx9Device()->CreateVertexDeclaration( VertexElements, &m_NonInteractiveRefresh.m_pVertexDecl );
	
#endif	
	
	return true;
}

void CShaderDeviceDx8::FreeNonInteractiveRefreshObjects()
{
	if ( m_NonInteractiveRefresh.m_pVertexShader )
	{
		m_NonInteractiveRefresh.m_pVertexShader->Release();
		m_NonInteractiveRefresh.m_pVertexShader = NULL;
	}

	if ( m_NonInteractiveRefresh.m_pPixelShader )
	{
		m_NonInteractiveRefresh.m_pPixelShader->Release();
		m_NonInteractiveRefresh.m_pPixelShader = NULL;
	}

	if ( m_NonInteractiveRefresh.m_pPixelShaderStartup )
	{
		m_NonInteractiveRefresh.m_pPixelShaderStartup->Release();
		m_NonInteractiveRefresh.m_pPixelShaderStartup = NULL;
	}

	if ( m_NonInteractiveRefresh.m_pPixelShaderStartupPass2 )
	{
		m_NonInteractiveRefresh.m_pPixelShaderStartupPass2->Release();
		m_NonInteractiveRefresh.m_pPixelShaderStartupPass2 = NULL;
	}

	if ( m_NonInteractiveRefresh.m_pVertexDecl )
	{
		m_NonInteractiveRefresh.m_pVertexDecl->Release();
		m_NonInteractiveRefresh.m_pVertexDecl = NULL;
	}
}

bool CShaderDeviceDx8::InNonInteractiveMode() const
{
	return m_NonInteractiveRefresh.m_Mode != MATERIAL_NON_INTERACTIVE_MODE_NONE;
}

void CShaderDeviceDx8::EnableNonInteractiveMode( MaterialNonInteractiveMode_t mode, ShaderNonInteractiveInfo_t *pInfo )
{
	if ( !IsX360() )
		return;
	if ( pInfo && ( pInfo->m_hTempFullscreenTexture == INVALID_SHADERAPI_TEXTURE_HANDLE ) )
	{
		mode = MATERIAL_NON_INTERACTIVE_MODE_NONE;
	}
	m_NonInteractiveRefresh.m_Mode = mode;
	if ( pInfo )
	{
		m_NonInteractiveRefresh.m_Info = *pInfo;
	}
	m_NonInteractiveRefresh.m_nPacifierFrame = 0;

	if ( mode != MATERIAL_NON_INTERACTIVE_MODE_NONE )
	{
		ConVarRef mat_monitorgamma( "mat_monitorgamma" );
		ConVarRef mat_monitorgamma_tv_range_min( "mat_monitorgamma_tv_range_min" );
		ConVarRef mat_monitorgamma_tv_range_max( "mat_monitorgamma_tv_range_max" );
		ConVarRef mat_monitorgamma_tv_exp( "mat_monitorgamma_tv_exp" );
		ConVarRef mat_monitorgamma_tv_enabled( "mat_monitorgamma_tv_enabled" );
		SetHardwareGammaRamp( mat_monitorgamma.GetFloat(), mat_monitorgamma_tv_range_min.GetFloat(), mat_monitorgamma_tv_range_max.GetFloat(),
			mat_monitorgamma_tv_exp.GetFloat(), mat_monitorgamma_tv_enabled.GetBool() );
	}

#ifdef _X360
	if ( mode != MATERIAL_NON_INTERACTIVE_MODE_NONE )
	{
		// HACK: VSync off (prevents us wasting time blocking on VSync due to our irregular present intervals)
		Dx9Device()->SetRenderState( D3DRS_PRESENTINTERVAL, D3DPRESENT_INTERVAL_IMMEDIATE );
	}
	else
	{
		// HACK: VSync on (defaulting to on on 360 is fine, but really should save+restore this state)
		Dx9Device()->SetRenderState( D3DRS_PRESENTINTERVAL, D3DPRESENT_INTERVAL_ONE );
	}
#endif

//	Msg( "Time elapsed: %.3f Peak %.3f Ave %.5f Count %d Count Above %d\n", Plat_FloatTime() - m_NonInteractiveRefresh.m_flStartTime,
//		m_NonInteractiveRefresh.m_flPeakDt, m_NonInteractiveRefresh.m_flTotalDt / m_NonInteractiveRefresh.m_nSamples, m_NonInteractiveRefresh.m_nSamples, m_NonInteractiveRefresh.m_nCountAbove66 );

	m_NonInteractiveRefresh.m_flStartTime = m_NonInteractiveRefresh.m_flLastPresentTime = 
		m_NonInteractiveRefresh.m_flLastPacifierTime = Plat_FloatTime();
	m_NonInteractiveRefresh.m_flPeakDt = 0.0f;
	m_NonInteractiveRefresh.m_flTotalDt = 0.0f;
	m_NonInteractiveRefresh.m_nSamples = 0;
	m_NonInteractiveRefresh.m_nCountAbove66 = 0;
}

void CShaderDeviceDx8::UpdatePresentStats()
{
	float t = Plat_FloatTime();
	float flActualDt = t - m_NonInteractiveRefresh.m_flLastPresentTime;
	if ( flActualDt > m_NonInteractiveRefresh.m_flPeakDt )
	{
		m_NonInteractiveRefresh.m_flPeakDt = flActualDt;
	}
	if ( flActualDt > 0.066 )
	{
		++m_NonInteractiveRefresh.m_nCountAbove66;
	}

	m_NonInteractiveRefresh.m_flTotalDt += flActualDt;
	++m_NonInteractiveRefresh.m_nSamples;

	t = Plat_FloatTime();
	m_NonInteractiveRefresh.m_flLastPresentTime = t;
}

void CShaderDeviceDx8::RefreshFrontBufferNonInteractive()
{
	if ( !IsX360() || !InNonInteractiveMode() )
		return;

	// Other code should not be talking to D3D at the same time as this
	AUTO_LOCK( m_nonInteractiveModeMutex );

#ifdef _X360
	g_pShaderAPI->OwnGPUResources( false );
	IDirect3DBaseTexture *pTexture = g_pShaderAPI->GetD3DTexture( m_NonInteractiveRefresh.m_Info.m_hTempFullscreenTexture );

	int w, h;
	g_pShaderAPI->GetBackBufferDimensions( w, h );
	XMMATRIX matWVP = XMMatrixOrthographicOffCenterLH( 0, (FLOAT)w, (FLOAT)h, 0, 0, 1 );

	// Structure to hold vertex data.
	struct TEXVERTEX
	{
		FLOAT       Position[3];
		FLOAT       TexCoord[2];
	};
	TEXVERTEX Vertices[4];

	Vertices[0].Position[0] = -0.5f;
	Vertices[0].Position[1] = -0.5f;
	Vertices[0].Position[2] = 0;
	Vertices[0].TexCoord[0] = 0;
	Vertices[0].TexCoord[1] = 0;

	Vertices[1].Position[0] = w-0.5f;
	Vertices[1].Position[1] = -0.5f;
	Vertices[1].Position[2] = 0;
	Vertices[1].TexCoord[0] = 1;
	Vertices[1].TexCoord[1] = 0;

	Vertices[2].Position[0] = w-0.5f;
	Vertices[2].Position[1] = h-0.5f;
	Vertices[2].Position[2] = 0;
	Vertices[2].TexCoord[0] = 1;
	Vertices[2].TexCoord[1] = 1;

	Vertices[3].Position[0] = -0.5f;
	Vertices[3].Position[1] = h-0.5f;
	Vertices[3].Position[2] = 0;
	Vertices[3].TexCoord[0] = 0;
	Vertices[3].TexCoord[1] = 1;

	D3DVIEWPORT9 viewport;
	viewport.X = viewport.Y = 0;
	viewport.Width = w; viewport.Height = h;
	viewport.MinZ = ReverseDepthOnX360() ? 1.0f : 0.0f;
	viewport.MaxZ = 1.0f - viewport.MinZ;

	bool bInStartupMode = ( m_NonInteractiveRefresh.m_Mode == MATERIAL_NON_INTERACTIVE_MODE_STARTUP );

	float flDepth = (ShaderUtil()->GetConfig().bReverseDepth ^ ReverseDepthOnX360()) ? 0.0f : 1.0f;
	Dx9Device()->Clear( 0, NULL, D3DCLEAR_ZBUFFER, 0, flDepth, 0L );

	Dx9Device()->SetViewport( &viewport );
	Dx9Device()->SetTexture( 0, pTexture );
	Dx9Device()->SetVertexShader( m_NonInteractiveRefresh.m_pVertexShader );
	Dx9Device()->SetPixelShader( bInStartupMode ? m_NonInteractiveRefresh.m_pPixelShaderStartup : m_NonInteractiveRefresh.m_pPixelShader );
	Dx9Device()->SetVertexShaderConstantF( 0, (FLOAT*)&matWVP, 4 );
	Dx9Device()->SetVertexDeclaration( m_NonInteractiveRefresh.m_pVertexDecl );
	Dx9Device()->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
	Dx9Device()->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );
	Dx9Device()->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
	Dx9Device()->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );

	tmZone( TELEMETRY_LEVEL1, TMZF_NONE, "%s", __FUNCTION__ );

	Dx9Device()->DrawPrimitiveUP( D3DPT_QUADLIST, 1, Vertices, sizeof( TEXVERTEX ) );

	if ( bInStartupMode )
	{
		float flXPos = m_NonInteractiveRefresh.m_Info.m_flNormalizedX;
		float flYPos = m_NonInteractiveRefresh.m_Info.m_flNormalizedY;
		float flHeight = m_NonInteractiveRefresh.m_Info.m_flNormalizedSize;
		int nSize = h * flHeight;
		int x = w * flXPos - nSize * 0.5f;
		int y = h * flYPos - nSize * 0.5f;
		w = h = nSize;

		Vertices[0].Position[0] = x - 0.5f;
		Vertices[0].Position[1] = y - 0.5f;
		Vertices[1].Position[0] = x+w-0.5f;
		Vertices[1].Position[1] = y - 0.5f;
		Vertices[2].Position[0] = x+w-0.5f;
		Vertices[2].Position[1] = y+h-0.5f;
		Vertices[3].Position[0] = x - 0.5f;
		Vertices[3].Position[1] = y+h-0.5f;

		float t = Plat_FloatTime();
		float flDt = t - m_NonInteractiveRefresh.m_flLastPacifierTime;
		if ( flDt > 0.030f )
		{
			if ( ++m_NonInteractiveRefresh.m_nPacifierFrame >= m_NonInteractiveRefresh.m_Info.m_nPacifierCount )
			{
				m_NonInteractiveRefresh.m_nPacifierFrame = 0;
			}
			m_NonInteractiveRefresh.m_flLastPacifierTime = t;
		}

		pTexture = g_pShaderAPI->GetD3DTexture( m_NonInteractiveRefresh.m_Info.m_pPacifierTextures[ m_NonInteractiveRefresh.m_nPacifierFrame ] );
		Dx9Device()->SetRenderState( D3DRS_ALPHABLENDENABLE, 1 );
		Dx9Device()->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
		Dx9Device()->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
		Dx9Device()->SetTexture( 0, pTexture );
		Dx9Device()->SetPixelShader( m_NonInteractiveRefresh.m_pPixelShaderStartupPass2 );
		Dx9Device()->DrawPrimitiveUP( D3DPT_QUADLIST, 1, Vertices, sizeof( TEXVERTEX ) );
	}

	Dx9Device()->SetVertexShader( NULL );
	Dx9Device()->SetPixelShader( NULL );
	Dx9Device()->SetTexture( 0, NULL );
	Dx9Device()->SetVertexDeclaration( NULL );

	tmZone( TELEMETRY_LEVEL1, TMZF_NONE, "D3DPresent" );

	Dx9Device()->Present( 0, 0, 0, 0 );
	g_pShaderAPI->QueueResetRenderState();
	g_pShaderAPI->OwnGPUResources( true );

	UpdatePresentStats();
#endif
}


//-----------------------------------------------------------------------------
// Page flip
//-----------------------------------------------------------------------------
void CShaderDeviceDx8::Present()
{
	LOCK_SHADERAPI();

	// need to flush the dynamic buffer
	g_pShaderAPI->FlushBufferedPrimitives();

	if ( !IsDeactivated() )
	{
		Dx9Device()->EndScene();
	}

	HRESULT hr = S_OK;

	// if we're in queued mode, don't present if the device is already lost
	bool bValidPresent = true;
	bool bInMainThread = ThreadInMainThread();
	if ( !bInMainThread )
	{
		// don't present if the device is in an invalid state and in queued mode
		if ( m_DeviceState != DEVICE_STATE_OK )
		{
			bValidPresent = false;
		}
		// check for lost device early in threaded mode
		CheckDeviceLost( m_bOtherAppInitializing );
		if ( m_DeviceState != DEVICE_STATE_OK )
		{
			bValidPresent = false;
		}
	}
	// Copy the back buffer into the non-interactive temp buffer
	if ( m_NonInteractiveRefresh.m_Mode == MATERIAL_NON_INTERACTIVE_MODE_LEVEL_LOAD )
	{
		g_pShaderAPI->CopyRenderTargetToTextureEx( m_NonInteractiveRefresh.m_Info.m_hTempFullscreenTexture, 0, NULL, NULL );
	}

	// If we're not iconified, try to present (without this check, we can flicker when Alt-Tabbed away)
#ifdef _WIN32
	if ( IsX360() || (IsIconic( ( HWND )m_hWnd ) == 0 && bValidPresent) )
#else
	if ( IsX360() || (IsIconic( (VD3DHWND)m_hWnd ) == 0 && bValidPresent) )
#endif
	{
		if ( IsPC() && ( m_IsResizing || ( m_ViewHWnd != (VD3DHWND)m_hWnd ) ) )
		{
			RECT destRect;
			#ifndef DX_TO_GL_ABSTRACTION
					GetClientRect( ( HWND )m_ViewHWnd, &destRect );
			#else
					toglGetClientRect( (VD3DHWND)m_ViewHWnd, &destRect );
			#endif

			ShaderViewport_t viewport;
			g_pShaderAPI->GetViewports( &viewport, 1 );

			RECT srcRect;
			srcRect.left = viewport.m_nTopLeftX;
			srcRect.right = viewport.m_nTopLeftX + viewport.m_nWidth;
			srcRect.top = viewport.m_nTopLeftY;
			srcRect.bottom = viewport.m_nTopLeftY + viewport.m_nHeight;

			hr = Dx9Device()->Present( &srcRect, &destRect, (VD3DHWND)m_ViewHWnd, 0 );
		}
		else
		{
			g_pShaderAPI->OwnGPUResources( false );
			hr = Dx9Device()->Present( 0, 0, 0, 0 );
		}
	}

	UpdatePresentStats();

	if ( IsWindows() )
	{
		if ( hr == D3DERR_DRIVERINTERNALERROR )
		{
			/*	Usually this bug means that the driver has run out of internal video
				memory, due to leaking it slowly over several application restarts. 
				As of summer 2007, IE in particular seemed to leak a lot of driver 
				memory for every image context it created in the browser window. A
				reboot clears out the leaked memory and will generally allow the game
				to be run again; occasionally (but not frequently) it's necessary to 
				reduce video settings in the game as well to run. But, this is too 
				fine a distinction to explain in a dialog, so place the guilt on the 
				user and ask them to reduce video settings regardless.
			*/

			Error( "Internal driver error at Present.\n"
				   "You're likely out of OS Paged Pool Memory! For more info, see\n"
				   "http://support.steampowered.com/cgi-bin/steampowered.cfg/php/enduser/std_adp.php?p_faqid=150\n" );
		}
		if ( hr == D3DERR_DEVICELOST )
		{
			MarkDeviceLost();
		}
	}

	MeshMgr()->DiscardVertexBuffers();

	if ( bInMainThread )
	{
		CheckDeviceLost( m_bOtherAppInitializing );
	}

	if ( IsX360() )
	{
		// according to docs  - "Mandatory Reset of GPU Registers"
		// 360 must force the cached state to be dirty after any present()
		g_pShaderAPI->ResetRenderState( false );
	}

#ifdef RECORD_KEYFRAMES
	static int frame = 0;
	++frame;
	if (frame == KEYFRAME_INTERVAL)
	{
		RECORD_COMMAND( DX8_KEYFRAME, 0 );

		g_pShaderAPI->ResetRenderState();
		frame = 0;
	}
#endif

	g_pShaderAPI->AdvancePIXFrame();

	if ( !IsDeactivated() )
	{
#ifndef DX_TO_GL_ABSTRACTION
		if ( ( ShaderUtil()->GetConfig().bMeasureFillRate || ShaderUtil()->GetConfig().bVisualizeFillRate ) )
		{
			g_pShaderAPI->ClearBuffers( true, true, true, -1, -1 );
		}
#endif

		Dx9Device()->BeginScene();
	}
}


// We need to scale our colors to the range [16, 235] to keep our colors within TV standards.  Some colors might
//    still be out of gamut if any of the R, G, or B channels are more than 191 units apart from each other in
//    the 0-255 scale, but it looks like the 360 deals with this for us by lowering the bright saturated color components.
// NOTE: I'm leaving the max at 255 to retain whiter than whites.  On most TV's, we seems a little dark in the bright colors
//    compared to TV and movies when played in the same conditions.  This keeps out brights on par with what customers are
//    used to seeing.
// TV's generally have a 2.5 gamma, so we need to convert our 2.2 frame buffer into a 2.5 frame buffer for display on a TV

void CShaderDeviceDx8::SetHardwareGammaRamp( float fGamma, float fGammaTVRangeMin, float fGammaTVRangeMax, float fGammaTVExponent, bool bTVEnabled )
{
	DevMsg( 2, "SetHardwareGammaRamp( %f )\n", fGamma );

	Assert( Dx9Device() );
	if( !Dx9Device() )
		return;

	D3DGAMMARAMP gammaRamp;
	for ( int i = 0; i < 256; i++ )
	{
		float flInputValue = float( i ) / 255.0f;

		// Since the 360's sRGB read/write is a piecewise linear approximation, we need to correct for the difference in gamma space here
		float flSrgbGammaValue;
		if ( IsX360() ) // Should we also do this for the PS3?
		{
			// First undo the 360 broken sRGB curve by bringing the value back into linear space
			float flLinearValue = X360GammaToLinear( flInputValue );
			flLinearValue = clamp( flLinearValue, 0.0f, 1.0f );

			// Now apply a true sRGB curve to mimic PC hardware
			flSrgbGammaValue = SrgbLinearToGamma( flLinearValue ); // ( flLinearValue <= 0.0031308f ) ? ( flLinearValue * 12.92f ) : ( 1.055f * powf( flLinearValue, ( 1.0f / 2.4f ) ) ) - 0.055f;
			flSrgbGammaValue = clamp( flSrgbGammaValue, 0.0f, 1.0f );
		}
		else
		{
			flSrgbGammaValue = flInputValue;
		}

		// Apply the user controlled exponent curve
		float flCorrection = pow( flSrgbGammaValue, ( fGamma / 2.2f ) );
		flCorrection = clamp( flCorrection, 0.0f, 1.0f );

		// TV adjustment - Apply an exp and a scale and bias
		if ( bTVEnabled )
		{
			// Adjust for TV gamma of 2.5 by applying an exponent of 2.2 / 2.5 = 0.88
			flCorrection = pow( flCorrection, 2.2f / fGammaTVExponent );
			flCorrection = clamp( flCorrection, 0.0f, 1.0f );

			// Scale and bias to fit into the 16-235 range for TV's
			flCorrection = ( flCorrection * ( fGammaTVRangeMax - fGammaTVRangeMin ) / 255.0f ) + ( fGammaTVRangeMin / 255.0f );
			flCorrection = clamp( flCorrection, 0.0f, 1.0f );
		}

		// Generate final int value
		unsigned int val = ( int )( flCorrection * 65535.0f );
		gammaRamp.red[i] = val;
		gammaRamp.green[i] = val;
		gammaRamp.blue[i] = val;
	}

	Dx9Device()->SetGammaRamp( 0, D3DSGR_NO_CALIBRATION, &gammaRamp );
}


//-----------------------------------------------------------------------------
// Shader compilation
//-----------------------------------------------------------------------------
IShaderBuffer* CShaderDeviceDx8::CompileShader( const char *pProgram, size_t nBufLen, const char *pShaderVersion )
{
	return ShaderManager()->CompileShader( pProgram, nBufLen, pShaderVersion );
}

VertexShaderHandle_t CShaderDeviceDx8::CreateVertexShader( IShaderBuffer *pBuffer )
{
	return ShaderManager()->CreateVertexShader( pBuffer );
}

void CShaderDeviceDx8::DestroyVertexShader( VertexShaderHandle_t hShader )
{
	ShaderManager()->DestroyVertexShader( hShader );
}

GeometryShaderHandle_t CShaderDeviceDx8::CreateGeometryShader( IShaderBuffer* pShaderBuffer )
{
	Assert( 0 );
	return GEOMETRY_SHADER_HANDLE_INVALID;
}

void CShaderDeviceDx8::DestroyGeometryShader( GeometryShaderHandle_t hShader )
{
	Assert( hShader == GEOMETRY_SHADER_HANDLE_INVALID );
}

PixelShaderHandle_t CShaderDeviceDx8::CreatePixelShader( IShaderBuffer *pBuffer )
{
	return ShaderManager()->CreatePixelShader( pBuffer );
}

void CShaderDeviceDx8::DestroyPixelShader( PixelShaderHandle_t hShader )
{
	ShaderManager()->DestroyPixelShader( hShader );
}

#ifdef DX_TO_GL_ABSTRACTION
void CShaderDeviceDx8::DoStartupShaderPreloading( void )
{
	ShaderManager()->DoStartupShaderPreloading();
}
#endif


//-----------------------------------------------------------------------------
// Creates/destroys Mesh
// NOTE: Will be deprecated soon!
//-----------------------------------------------------------------------------
IMesh* CShaderDeviceDx8::CreateStaticMesh( VertexFormat_t vertexFormat, const char *pTextureBudgetGroup, IMaterial * pMaterial )
{
	LOCK_SHADERAPI();
	return MeshMgr()->CreateStaticMesh( vertexFormat, pTextureBudgetGroup, pMaterial );
}

void CShaderDeviceDx8::DestroyStaticMesh( IMesh* pMesh )
{
	LOCK_SHADERAPI();
	MeshMgr()->DestroyStaticMesh( pMesh );
}


//-----------------------------------------------------------------------------
// Creates/destroys vertex buffers + index buffers
//-----------------------------------------------------------------------------
IVertexBuffer *CShaderDeviceDx8::CreateVertexBuffer( ShaderBufferType_t type, VertexFormat_t fmt, int nVertexCount, const char *pBudgetGroup )
{
	LOCK_SHADERAPI();
	return MeshMgr()->CreateVertexBuffer( type, fmt, nVertexCount, pBudgetGroup );
}

void CShaderDeviceDx8::DestroyVertexBuffer( IVertexBuffer *pVertexBuffer )
{
	LOCK_SHADERAPI();
	MeshMgr()->DestroyVertexBuffer( pVertexBuffer );
}

IIndexBuffer *CShaderDeviceDx8::CreateIndexBuffer( ShaderBufferType_t bufferType, MaterialIndexFormat_t fmt, int nIndexCount, const char *pBudgetGroup )
{
	LOCK_SHADERAPI();
	return MeshMgr()->CreateIndexBuffer( bufferType, fmt, nIndexCount, pBudgetGroup );
}

void CShaderDeviceDx8::DestroyIndexBuffer( IIndexBuffer *pIndexBuffer )
{
	LOCK_SHADERAPI();
	MeshMgr()->DestroyIndexBuffer( pIndexBuffer );
}

IVertexBuffer *CShaderDeviceDx8::GetDynamicVertexBuffer( int streamID, VertexFormat_t vertexFormat, bool bBuffered )
{
	LOCK_SHADERAPI();
	return MeshMgr()->GetDynamicVertexBuffer( streamID, vertexFormat, bBuffered );
}

IIndexBuffer *CShaderDeviceDx8::GetDynamicIndexBuffer( MaterialIndexFormat_t fmt, bool bBuffered )
{
	LOCK_SHADERAPI();
	return MeshMgr()->GetDynamicIndexBuffer( fmt, bBuffered );
}

#ifdef _X360
void CShaderDeviceDx8::SpewVideoInfo360( const CCommand &args )
{
	XVIDEO_MODE videoMode;
	XGetVideoMode( &videoMode );

	Warning( "back buffer size: %dx%d\n", m_PresentParameters.BackBufferWidth, m_PresentParameters.BackBufferHeight );
	Warning( "display resolution: %dx%d %s\n", videoMode.dwDisplayWidth, videoMode.dwDisplayHeight, videoMode.fIsInterlaced ? "interlaced" : "progressive" );
	Warning( "refresh rate: %f\n", videoMode.RefreshRate );
	Warning( "aspect: %s\n", videoMode.fIsWideScreen ? "16x9 (widescreen)" : "4x3 (normal)" );
	Warning( "%s\n", videoMode.fIsHiDef ? "hidef" : "lodef" );
	switch( videoMode.VideoStandard )
	{
	case XC_VIDEO_STANDARD_NTSC_M:
		Warning( "video standard: NTSC_M\n" );
		break;
	case XC_VIDEO_STANDARD_NTSC_J:
		Warning( "video standard: NTSC_J\n" );
		break;
	case XC_VIDEO_STANDARD_PAL_I:
		Warning( "video standard: PAL_I\n" );
		break;
	default:
		Warning( "error: UNKNOWN VIDEO STANDARD!\n" );
		Assert( 0 );
		break;
	}
	ConVarRef fps_max( "fps_max" );
	Warning( "fps_max: %f\n", fps_max.GetFloat() );
	switch( m_PresentParameters.MultiSampleType )
	{
	case D3DMULTISAMPLE_NONE:
		Warning( "multisample type: D3DMULTISAMPLE_NONE\n" );
		break;
	case D3DMULTISAMPLE_2_SAMPLES:
		Warning( "multisample type: D3DMULTISAMPLE_2_SAMPLES\n" );
		break;
	case D3DMULTISAMPLE_4_SAMPLES:
		Warning( "multisample type: D3DMULTISAMPLE_4_SAMPLES\n" );
		break;
	}
}
#endif
