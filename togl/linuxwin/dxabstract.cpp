//========= Copyright Valve Corporation, All rights reserved. ============//
//                       TOGL CODE LICENSE
//
//  Copyright 2011-2014 Valve Corporation
//  All Rights Reserved.
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.
//
// dxabstract.cpp
//
//==================================================================================================
#include "togl/rendermechanism.h"
#include "tier0/vprof_telemetry.h"
#include "tier0/dbg.h"
#include "tier0/threadtools.h"
#include "tier0/vprof.h"
#include "tier1/strtools.h"
#include "tier1/utlbuffer.h"
#include "dx9asmtogl2.h"
#include "mathlib/vmatrix.h"
#include "materialsystem/IShader.h"

#include "glmgr_flush.inl"

#if defined(OSX) || defined(LINUX) || (defined (WIN32) && defined( DX_TO_GL_ABSTRACTION ))
	#include "appframework/ilaunchermgr.h"
	extern ILauncherMgr *g_pLauncherMgr;
#endif

#include "tier0/icommandline.h"
#include "tier0/memdbgon.h"

#ifdef USE_ACTUAL_DX

#pragma comment( lib, "../../dx9sdk/lib/d3d9.lib" )
#pragma comment( lib, "../../dx9sdk/lib/d3dx9.lib" )

#else

#ifdef POSIX
#define strcat_s( a, b, c) V_strcat( a, c, b )
#endif

#define D3D_DEVICE_VALID_MARKER 0x12EBC845
#define GL_PUBLIC_ENTRYPOINT_CHECKS( dev ) Assert( dev->GetCurrentOwnerThreadId() == ThreadGetCurrentId() ); Assert( dev->m_nValidMarker == D3D_DEVICE_VALID_MARKER );
// ------------------------------------------------------------------------------------------------------------------------------ //
bool g_bNullD3DDevice;

static D3DToGL		g_D3DToOpenGLTranslatorGLSL;
static IDirect3DDevice9 *g_pD3D_Device;

#if GL_BATCH_PERF_ANALYSIS
	#include "../../thirdparty/miniz/simple_bitmap.h"
	#include "../../thirdparty/miniz/miniz.c"

	ConVar gl_batch_vis_abs_scale( "gl_batch_vis_abs_scale", ".050" );
	ConVar gl_present_vis_abs_scale( "gl_present_vis_abs_scale", ".050" );
	//ConVar gl_batch_vis_y_scale( "gl_batch_vis_y_scale", "0.007" );
	ConVar gl_batch_vis_y_scale( "gl_batch_vis_y_scale", "0.0" );
	static double s_rdtsc_to_ms;

	uint64 g_nTotalD3DCycles;
	uint g_nTotalD3DCalls;

	class CGLBatchPerfCallTimer
	{
	public:
		inline CGLBatchPerfCallTimer() { g_nTotalD3DCalls++; g_nTotalD3DCycles -= TM_FAST_TIME(); }
		inline ~CGLBatchPerfCallTimer() { g_nTotalD3DCycles += TM_FAST_TIME(); }
	};

	#define GL_BATCH_PERF_CALL_TIMER CGLBatchPerfCallTimer scopedCallTimer;
#else
	#define GL_BATCH_PERF_CALL_TIMER
#endif // GL_BATCH_PERF_ANALYSIS

ConVar gl_batch_vis( "gl_batch_vis", "0" );

// ------------------------------------------------------------------------------------------------------------------------------ //
// functions that are dependant on g_pLauncherMgr

inline GLMDisplayDB *GetDisplayDB( void )
{
	return g_pLauncherMgr->GetDisplayDB();
}

inline void RenderedSize( uint &width, uint &height, bool set )
{
	g_pLauncherMgr->RenderedSize( width, height, set );
}

inline void GetStackCrawl( CStackCrawlParams *params )
{
	g_pLauncherMgr->GetStackCrawl(params);
}

// ------------------------------------------------------------------------------------------------------------------------------ //

#if defined( WIN32 )

bool D3DMATRIX::operator == ( CONST D3DMATRIX& src) const 
{
	return V_memcmp( (void*)this, (void*)&src, sizeof(this) ) == 0;
}

D3DMATRIX::operator void* ()
{
	return (void*)this;
}

#endif

// ------------------------------------------------------------------------------------------------------------------------------ //

#ifdef OSX

#pragma mark ----- D3DXMATRIX operators

#endif

D3DXMATRIX D3DXMATRIX::operator*( const D3DXMATRIX &o ) const
{
	D3DXMATRIX result;
	
	D3DXMatrixMultiply( &result, this, &o );	// this = lhs    o = rhs    result = this * o

	return result;
}

D3DXMATRIX::operator FLOAT* ()
{
	return (float*)this;
}

float& D3DXMATRIX::operator()( int row, int column )
{
	return m[row][column];
}

const float& D3DXMATRIX::operator()( int row, int column ) const
{
	return m[row][column];
}

bool D3DXMATRIX::operator != ( CONST D3DXMATRIX& src ) const 
{
	return V_memcmp( (void*)this, (void*)&src, sizeof(this) ) != 0;
}

// ------------------------------------------------------------------------------------------------------------------------------ //

#ifdef OSX

#pragma mark ----- D3DXPLANE operators

#endif

float& D3DXPLANE::operator[]( int i )
{
	return ((float*)this)[i];
}

bool D3DXPLANE::operator==( const D3DXPLANE &o )
{
	return a == o.a && b == o.b && c == o.c && d == o.d;
}

bool D3DXPLANE::operator!=( const D3DXPLANE &o )
{
	return !( *this == o );
}

D3DXPLANE::operator float*()
{
	return (float*)this;
}

D3DXPLANE::operator const float*() const
{
	return (const float*)this;
}

// ------------------------------------------------------------------------------------------------------------------------------ //

#ifdef OSX

#pragma mark ----- D3DXVECTOR2 operators

#endif

D3DXVECTOR2::operator FLOAT* ()
{
	return (float*)this;
}

D3DXVECTOR2::operator CONST FLOAT* () const
{
	return (const float*)this;
}

// ------------------------------------------------------------------------------------------------------------------------------ //

#ifdef OSX

#pragma mark ----- D3DXVECTOR3 operators

#endif

D3DXVECTOR3::D3DXVECTOR3( float a, float b, float c )
{
	x = a;
	y = b;
	z = c;
}

D3DXVECTOR3::operator FLOAT* ()
{
	return (float*)this;
}

D3DXVECTOR3::operator CONST FLOAT* () const
{
	return (const float*)this;
}

// ------------------------------------------------------------------------------------------------------------------------------ //

#ifdef OSX

#pragma mark ----- D3DXVECTOR4 operators

#endif

D3DXVECTOR4::D3DXVECTOR4( float a, float b, float c, float d )
{
	x = a;
	y = b;
	z = c;
	w = d;
}

// ------------------------------------------------------------------------------------------------------------------------------ //

DWORD IDirect3DResource9::SetPriority(DWORD PriorityNew)
{
//	DXABSTRACT_BREAK_ON_ERROR();
//	GLMPRINTF(( "-X- SetPriority" ));
	// no-op city
	return 0;
}

// ------------------------------------------------------------------------------------------------------------------------------ //

#ifdef OSX

#pragma mark ----- IDirect3DBaseTexture9

#endif

IDirect3DBaseTexture9::~IDirect3DBaseTexture9()
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( m_device );
	GLMPRINTF(( ">-A- ~IDirect3DBaseTexture9" ));

	if (m_device)
	{
		Assert( m_device->m_ObjectStats.m_nTotalTextures >= 1 );
		m_device->m_ObjectStats.m_nTotalTextures--;

		GLMPRINTF(( "-A- ~IDirect3DBaseTexture9 taking normal delete path on %08x, device is %08x ", this, m_device ));
		m_device->ReleasedTexture( this );
		
		if (m_tex)
		{
			GLMPRINTF(("-A- ~IDirect3DBaseTexture9 deleted '%s' @ %08x (GLM %08x) %s",m_tex->m_layout->m_layoutSummary, this, m_tex, m_tex->m_debugLabel ? m_tex->m_debugLabel : "" ));

			m_device->ReleasedCGLMTex( m_tex );

			m_tex->m_ctx->DelTex( m_tex );
			m_tex = NULL;
		}
		else
		{
			GLMPRINTF(( "-A- ~IDirect3DBaseTexture9 : whoops, no tex to delete here ?" ));
		}		
		m_device = NULL;	// ** THIS ** is the only place to scrub this.  Don't do it in the subclass destructors.
	}
	else
	{
		GLMPRINTF(( "-A- ~IDirect3DBaseTexture9 taking strange delete path on %08x, device is %08x ", this, m_device ));
	}

	GLMPRINTF(( "<-A- ~IDirect3DBaseTexture9" ));
}

D3DRESOURCETYPE IDirect3DBaseTexture9::GetType()
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( m_device );

	return m_restype;	//D3DRTYPE_TEXTURE;
}

DWORD IDirect3DBaseTexture9::GetLevelCount()
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( m_device );

	return m_tex->m_layout->m_mipCount;
}

HRESULT IDirect3DBaseTexture9::GetLevelDesc(UINT Level,D3DSURFACE_DESC *pDesc)
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( m_device );

	Assert (Level < static_cast<uint>(m_tex->m_layout->m_mipCount));

	D3DSURFACE_DESC result = m_descZero;
	// then mutate it for the level of interest
	
	GLMTexLayoutSlice *slice = &m_tex->m_layout->m_slices[ m_tex->CalcSliceIndex( 0, Level ) ];

	result.Width = slice->m_xSize;
	result.Height = slice->m_ySize;
	
	*pDesc = result;

	return S_OK;
}

// ------------------------------------------------------------------------------------------------------------------------------ //

#ifdef OSX

#pragma mark ----- IDirect3DTexture9

#endif

HRESULT IDirect3DDevice9::CreateTexture(UINT Width,UINT Height,UINT Levels,DWORD Usage,D3DFORMAT Format,D3DPOOL Pool,IDirect3DTexture9** ppTexture,VD3DHANDLE* pSharedHandle, char *pDebugLabel )
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( this );

	m_ObjectStats.m_nTotalTextures++;

	GLMPRINTF((">-A-IDirect3DDevice9::CreateTexture"));
	IDirect3DTexture9	*dxtex = new IDirect3DTexture9;
	dxtex->m_restype = D3DRTYPE_TEXTURE;
	
	dxtex->m_device		= this;

	dxtex->m_descZero.Format	= Format;
	dxtex->m_descZero.Type		= D3DRTYPE_TEXTURE;
	dxtex->m_descZero.Usage		= Usage;
	dxtex->m_descZero.Pool		= Pool;

	dxtex->m_descZero.MultiSampleType		= D3DMULTISAMPLE_NONE;
	dxtex->m_descZero.MultiSampleQuality	= 0;
	dxtex->m_descZero.Width		= Width;
	dxtex->m_descZero.Height	= Height;
	
	GLMTexLayoutKey key;
	memset( &key, 0, sizeof(key) );
	
	key.m_texGLTarget	= GL_TEXTURE_2D;
	key.m_texFormat		= Format;

	if (Levels>1)
	{
		key.m_texFlags |= kGLMTexMipped;
	}

	// http://msdn.microsoft.com/en-us/library/bb172625(VS.85).aspx
	
	// complain if any usage bits come down that I don't know.
	uint knownUsageBits = (D3DUSAGE_AUTOGENMIPMAP | D3DUSAGE_RENDERTARGET | D3DUSAGE_DYNAMIC | D3DUSAGE_TEXTURE_SRGB | D3DUSAGE_DEPTHSTENCIL);
	if ( (Usage & knownUsageBits) != Usage )
	{
		DXABSTRACT_BREAK_ON_ERROR();
	}
	
	if (Usage & D3DUSAGE_AUTOGENMIPMAP)
	{
		key.m_texFlags |= kGLMTexMipped | kGLMTexMippedAuto;
	}
	
	if (Usage & D3DUSAGE_DYNAMIC)
	{
		// GLMPRINTF(("-X- DYNAMIC tex usage ignored.."));	//FIXME
	}
	
	if (Usage & D3DUSAGE_TEXTURE_SRGB)
	{
		key.m_texFlags |= kGLMTexSRGB;
	}
	
	if (Usage & D3DUSAGE_RENDERTARGET)
	{
		Assert( !(Usage & D3DUSAGE_DEPTHSTENCIL) );
		
		m_ObjectStats.m_nTotalRenderTargets++;
						
		key.m_texFlags |= kGLMTexRenderable;
		
		const GLMTexFormatDesc *pFmtDesc = GetFormatDesc( key.m_texFormat );
		if ( pFmtDesc->m_glIntFormatSRGB != 0 )
		{
			key.m_texFlags |= kGLMTexSRGB;			// this catches callers of CreateTexture who set the "renderable" option - they get an SRGB tex
		}
		
		if (m_ctx->Caps().m_cantAttachSRGB)
		{
			// this config can't support SRGB render targets.  quietly turn off the sRGB bit.
			key.m_texFlags &= ~kGLMTexSRGB;
		}
	}

	if ( ( Format == D3DFMT_D16 ) || ( Format == D3DFMT_D24X8 ) || ( Format == D3DFMT_D24S8 ) )
	{
		key.m_texFlags |= kGLMTexIsDepth;
	}
	if ( Format == D3DFMT_D24S8 )
	{
		key.m_texFlags |= kGLMTexIsStencil;
	}
			
	key.m_xSize = Width;
	key.m_ySize = Height;
	key.m_zSize = 1;
	
	CGLMTex *tex = m_ctx->NewTex( &key, Levels, pDebugLabel );
	if (!tex)
	{
		DXABSTRACT_BREAK_ON_ERROR();
	}
	dxtex->m_tex = tex;

	dxtex->m_tex->m_srgbFlipCount = 0;

	m_ObjectStats.m_nTotalSurfaces++;

	dxtex->m_surfZero = new IDirect3DSurface9;
	dxtex->m_surfZero->m_restype = (D3DRESOURCETYPE)0;	// this is a ref to a tex, not the owner... 
	
	// do not do an AddRef here.	
	
	dxtex->m_surfZero->m_device = this;

	dxtex->m_surfZero->m_desc	=	dxtex->m_descZero;
	dxtex->m_surfZero->m_tex	=	tex;
	dxtex->m_surfZero->m_face	=	0;
	dxtex->m_surfZero->m_mip	=	0;
	
	GLMPRINTF(("-A- IDirect3DDevice9::CreateTexture created '%s' @ %08x (GLM %08x) %s",tex->m_layout->m_layoutSummary, dxtex, tex, pDebugLabel ? pDebugLabel : "" ));
	
	*ppTexture = dxtex;
	
	GLMPRINTF(("<-A-IDirect3DDevice9::CreateTexture"));
	return S_OK;
}


IDirect3DTexture9::~IDirect3DTexture9()
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( m_device );
	
	GLMPRINTF(( ">-A- IDirect3DTexture9" ));

	// IDirect3DBaseTexture9::~IDirect3DBaseTexture9 frees up m_tex
	// we take care of surfZero

	if (m_device)
	{
		m_device->ReleasedTexture( this );

		if (m_surfZero)
		{
			ULONG refc = m_surfZero->Release( 0, "~IDirect3DTexture9 public release (surfZero)" ); (void)refc;
			Assert( !refc );
			m_surfZero = NULL;
		}
		// leave m_device alone!
	}

	GLMPRINTF(( "<-A- IDirect3DTexture9" ));
}

HRESULT IDirect3DTexture9::LockRect(UINT Level,D3DLOCKED_RECT* pLockedRect,CONST RECT* pRect,DWORD Flags)
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( m_device );
	DXABSTRACT_BREAK_ON_ERROR();
	return S_OK;
}

HRESULT IDirect3DTexture9::UnlockRect(UINT Level)
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( m_device );
	DXABSTRACT_BREAK_ON_ERROR();
	return S_OK;
}

HRESULT IDirect3DTexture9::GetSurfaceLevel(UINT Level,IDirect3DSurface9** ppSurfaceLevel)
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( m_device );
	// we create and pass back a surface, and the client is on the hook to release it.  tidy.

	m_device->m_ObjectStats.m_nTotalSurfaces++;

	IDirect3DSurface9 *surf = new IDirect3DSurface9;
	surf->m_restype = (D3DRESOURCETYPE)0;	// 0 is special and means this 'surface' does not own its m_tex

	// Dicey...higher level code seems to want this and not want this.  Are we missing some AddRef/Release behavior elsewhere?
	// trying to turn this off - experimental - 26Oct2010 surf->AddRef();
	
	surf->m_device = this->m_device;
	
	GLMTexLayoutSlice *slice = &m_tex->m_layout->m_slices[ m_tex->CalcSliceIndex( 0, Level ) ];
		
	surf->m_desc = m_descZero;
		surf->m_desc.Width = slice->m_xSize;
		surf->m_desc.Height = slice->m_ySize;
		
	surf->m_tex	= m_tex;
	surf->m_face = 0;
	surf->m_mip = Level;

	*ppSurfaceLevel = surf;

	return S_OK;
}

// ------------------------------------------------------------------------------------------------------------------------------ //

#ifdef OSX

#pragma mark ----- IDirect3DCubeTexture9

#endif

HRESULT IDirect3DDevice9::CreateCubeTexture(UINT EdgeLength,UINT Levels,DWORD Usage,D3DFORMAT Format,D3DPOOL Pool,IDirect3DCubeTexture9** ppCubeTexture,VD3DHANDLE* pSharedHandle, char *pDebugLabel)
{
	GL_BATCH_PERF_CALL_TIMER;
	Assert( m_ctx->m_nCurOwnerThreadId == ThreadGetCurrentId() );
	GLMPRINTF((">-A-  IDirect3DDevice9::CreateCubeTexture"));

	Assert( m_ctx->m_nCurOwnerThreadId == ThreadGetCurrentId() );

	m_ObjectStats.m_nTotalTextures++;

	IDirect3DCubeTexture9	*dxtex = new IDirect3DCubeTexture9;
	dxtex->m_restype = D3DRTYPE_CUBETEXTURE;
	
	dxtex->m_device			= this;

	dxtex->m_descZero.Format	= Format;
	dxtex->m_descZero.Type		= D3DRTYPE_CUBETEXTURE;
	dxtex->m_descZero.Usage		= Usage;
	dxtex->m_descZero.Pool		= Pool;

	dxtex->m_descZero.MultiSampleType		= D3DMULTISAMPLE_NONE;
	dxtex->m_descZero.MultiSampleQuality	= 0;
	dxtex->m_descZero.Width		= EdgeLength;
	dxtex->m_descZero.Height	= EdgeLength;
	
	GLMTexLayoutKey key;
	memset( &key, 0, sizeof(key) );
	
	key.m_texGLTarget	= GL_TEXTURE_CUBE_MAP;
	key.m_texFormat		= Format;

	if (Levels>1)
	{
		key.m_texFlags |= kGLMTexMipped;
	}

	// http://msdn.microsoft.com/en-us/library/bb172625(VS.85).aspx	
	// complain if any usage bits come down that I don't know.
	uint knownUsageBits = (D3DUSAGE_AUTOGENMIPMAP | D3DUSAGE_RENDERTARGET | D3DUSAGE_DYNAMIC | D3DUSAGE_TEXTURE_SRGB);
	if ( (Usage & knownUsageBits) != Usage )
	{
		DXABSTRACT_BREAK_ON_ERROR();
	}
	
	if (Usage & D3DUSAGE_AUTOGENMIPMAP)
	{
		key.m_texFlags |= kGLMTexMipped | kGLMTexMippedAuto;
	}
	
	if (Usage & D3DUSAGE_RENDERTARGET)
	{
		key.m_texFlags |= kGLMTexRenderable;
		
		m_ObjectStats.m_nTotalRenderTargets++;
	}
		
	if (Usage & D3DUSAGE_DYNAMIC)
	{
		//GLMPRINTF(("-X- DYNAMIC tex usage ignored.."));	//FIXME
	}
	
	if (Usage & D3DUSAGE_TEXTURE_SRGB)
	{
		key.m_texFlags |= kGLMTexSRGB;
	}
	
	key.m_xSize = EdgeLength;
	key.m_ySize = EdgeLength;
	key.m_zSize = 1;
	
	CGLMTex *tex = m_ctx->NewTex( &key, Levels, pDebugLabel );
	if (!tex)
	{
		DXABSTRACT_BREAK_ON_ERROR();
	}
	dxtex->m_tex = tex;
	
	dxtex->m_tex->m_srgbFlipCount = 0;

	for( int face = 0; face < 6; face ++)
	{
		m_ObjectStats.m_nTotalSurfaces++;

		dxtex->m_surfZero[face] = new IDirect3DSurface9;
		dxtex->m_surfZero[face]->m_restype = (D3DRESOURCETYPE)0;	// 0 is special and means this 'surface' does not own its m_tex
		// do not do an AddRef here.	
		
		dxtex->m_surfZero[face]->m_device = this;
		
		dxtex->m_surfZero[face]->m_desc	=	dxtex->m_descZero;
		dxtex->m_surfZero[face]->m_tex	=	tex;
		dxtex->m_surfZero[face]->m_face	=	face;
		dxtex->m_surfZero[face]->m_mip	=	0;
	}
	
	GLMPRINTF(("-A- IDirect3DDevice9::CreateCubeTexture created '%s' @ %08x (GLM %08x)",tex->m_layout->m_layoutSummary, dxtex, tex ));
	
	*ppCubeTexture = dxtex;
	
	GLMPRINTF(("<-A- IDirect3DDevice9::CreateCubeTexture"));

	return S_OK;
}

IDirect3DCubeTexture9::~IDirect3DCubeTexture9()
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( m_device );
	GLMPRINTF(( ">-A- ~IDirect3DCubeTexture9" ));

	if (m_device)
	{
		GLMPRINTF(( "-A- ~IDirect3DCubeTexture9 taking normal delete path on %08x, device is %08x, surfzero[0] is %08x ", this, m_device, m_surfZero[0] ));
		m_device->ReleasedTexture( this );

		// let IDirect3DBaseTexture9::~IDirect3DBaseTexture9 free up m_tex
		// we handle the surfZero array for the faces
		
		for( int face = 0; face < 6; face ++)
		{
			if (m_surfZero[face])
			{
				Assert( m_surfZero[face]->m_device == m_device );
				ULONG refc = m_surfZero[face]->Release( 0, "~IDirect3DCubeTexture9 public release (surfZero)");
				if ( refc!=0 )
				{
					GLMPRINTF(( "-A- ~IDirect3DCubeTexture9 seeing non zero refcount on surfzero[%d] => %d ", face, refc ));
				}
				m_surfZero[face] = NULL;
			}
		}
		// leave m_device alone!
	}
	else
	{
		GLMPRINTF(( "-A- ~IDirect3DCubeTexture9 taking strange delete path on %08x, device is %08x, surfzero[0] is %08x ", this, m_device, m_surfZero[0] ));
	}

	GLMPRINTF(( "<-A- ~IDirect3DCubeTexture9" ));
}

HRESULT IDirect3DCubeTexture9::GetCubeMapSurface(D3DCUBEMAP_FACES FaceType,UINT Level,IDirect3DSurface9** ppCubeMapSurface)
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( m_device );
	// we create and pass back a surface, and the client is on the hook to release it...

	m_device->m_ObjectStats.m_nTotalSurfaces++;

	IDirect3DSurface9 *surf = new IDirect3DSurface9;
	surf->m_restype = (D3DRESOURCETYPE)0;	// 0 is special and means this 'surface' does not own its m_tex
	
	GLMTexLayoutSlice *slice = &m_tex->m_layout->m_slices[ m_tex->CalcSliceIndex( FaceType, Level ) ];
	
	surf->m_device = this->m_device;
	
	surf->m_desc = m_descZero;
		surf->m_desc.Width = slice->m_xSize;
		surf->m_desc.Height = slice->m_ySize;
		
	surf->m_tex	= m_tex;
	surf->m_face = FaceType;
	surf->m_mip = Level;

	*ppCubeMapSurface = surf;

	return S_OK;
}

HRESULT IDirect3DCubeTexture9::GetLevelDesc(UINT Level,D3DSURFACE_DESC *pDesc)
{
	GL_BATCH_PERF_CALL_TIMER;
	Assert (Level < static_cast<uint>(m_tex->m_layout->m_mipCount));

	D3DSURFACE_DESC result = m_descZero;
	// then mutate it for the level of interest
	
	GLMTexLayoutSlice *slice = &m_tex->m_layout->m_slices[ m_tex->CalcSliceIndex( 0, Level ) ];

	result.Width = slice->m_xSize;
	result.Height = slice->m_ySize;

	*pDesc = result;

	return S_OK;
}


// ------------------------------------------------------------------------------------------------------------------------------ //

#ifdef OSX

#pragma mark ----- IDirect3DVolumeTexture9

#endif

HRESULT IDirect3DDevice9::CreateVolumeTexture(UINT Width,UINT Height,UINT Depth,UINT Levels,DWORD Usage,D3DFORMAT Format,D3DPOOL Pool,IDirect3DVolumeTexture9** ppVolumeTexture,VD3DHANDLE* pSharedHandle, char *pDebugLabel)
{
	GL_BATCH_PERF_CALL_TIMER;
	GLMPRINTF((">-A-  IDirect3DDevice9::CreateVolumeTexture"));
	// set dxtex->m_restype to D3DRTYPE_VOLUMETEXTURE...

	Assert( m_ctx->m_nCurOwnerThreadId == ThreadGetCurrentId() );

	m_ObjectStats.m_nTotalTextures++;

	IDirect3DVolumeTexture9	*dxtex = new IDirect3DVolumeTexture9;
	dxtex->m_restype = D3DRTYPE_VOLUMETEXTURE;
	
	dxtex->m_device			= this;

	dxtex->m_descZero.Format	= Format;
	dxtex->m_descZero.Type		= D3DRTYPE_VOLUMETEXTURE;
	dxtex->m_descZero.Usage		= Usage;
	dxtex->m_descZero.Pool		= Pool;

	dxtex->m_descZero.MultiSampleType		= D3DMULTISAMPLE_NONE;
	dxtex->m_descZero.MultiSampleQuality	= 0;
	dxtex->m_descZero.Width		= Width;
	dxtex->m_descZero.Height	= Height;

	// also a volume specific desc
	dxtex->m_volDescZero.Format		= Format;
	dxtex->m_volDescZero.Type		= D3DRTYPE_VOLUMETEXTURE;
	dxtex->m_volDescZero.Usage		= Usage;
	dxtex->m_volDescZero.Pool		= Pool;

	dxtex->m_volDescZero.Width		= Width;
	dxtex->m_volDescZero.Height		= Height;
	dxtex->m_volDescZero.Depth		= Depth;
	
	GLMTexLayoutKey key;
	memset( &key, 0, sizeof(key) );
	
	key.m_texGLTarget	= GL_TEXTURE_3D;
	key.m_texFormat		= Format;

	if (Levels>1)
	{
		key.m_texFlags |= kGLMTexMipped;
	}

	// http://msdn.microsoft.com/en-us/library/bb172625(VS.85).aspx	
	// complain if any usage bits come down that I don't know.
	uint knownUsageBits = (D3DUSAGE_AUTOGENMIPMAP | D3DUSAGE_RENDERTARGET | D3DUSAGE_DYNAMIC | D3DUSAGE_TEXTURE_SRGB);
	if ( (Usage & knownUsageBits) != Usage )
	{
		DXABSTRACT_BREAK_ON_ERROR();
	}
	
	if (Usage & D3DUSAGE_AUTOGENMIPMAP)
	{
		key.m_texFlags |= kGLMTexMipped | kGLMTexMippedAuto;
	}
	
	if (Usage & D3DUSAGE_RENDERTARGET)
	{
		key.m_texFlags |= kGLMTexRenderable;
		
		m_ObjectStats.m_nTotalRenderTargets++;
	}
	
	if (Usage & D3DUSAGE_DYNAMIC)
	{
		GLMPRINTF(("-X- DYNAMIC tex usage ignored.."));	//FIXME
	}
	
	if (Usage & D3DUSAGE_TEXTURE_SRGB)
	{
		key.m_texFlags |= kGLMTexSRGB;
	}
	
	key.m_xSize = Width;
	key.m_ySize = Height;
	key.m_zSize = Depth;
	
	CGLMTex *tex = m_ctx->NewTex( &key, Levels, pDebugLabel );
	if (!tex)
	{
		DXABSTRACT_BREAK_ON_ERROR();
	}
	dxtex->m_tex = tex;
	
	dxtex->m_tex->m_srgbFlipCount = 0;

	m_ObjectStats.m_nTotalSurfaces++;

	dxtex->m_surfZero = new IDirect3DSurface9;
	dxtex->m_surfZero->m_restype = (D3DRESOURCETYPE)0;	// this is a ref to a tex, not the owner... 
	// do not do an AddRef here.	
	
	dxtex->m_surfZero->m_device = this;
	
	dxtex->m_surfZero->m_desc	=	dxtex->m_descZero;
	dxtex->m_surfZero->m_tex	=	tex;
	dxtex->m_surfZero->m_face	=	0;
	dxtex->m_surfZero->m_mip	=	0;
	
	GLMPRINTF(("-A- IDirect3DDevice9::CreateVolumeTexture created '%s' @ %08x (GLM %08x)",tex->m_layout->m_layoutSummary, dxtex, tex ));
	
	*ppVolumeTexture = dxtex;

	GLMPRINTF(("<-A-  IDirect3DDevice9::CreateVolumeTexture"));

	return S_OK;
}

IDirect3DVolumeTexture9::~IDirect3DVolumeTexture9()
{
	GL_BATCH_PERF_CALL_TIMER;
	GLMPRINTF((">-A-  ~IDirect3DVolumeTexture9"));

	if (m_device)
	{
		m_device->ReleasedTexture( this );

		// let IDirect3DBaseTexture9::~IDirect3DBaseTexture9 free up m_tex
		// we handle m_surfZero
		
		if (m_surfZero)
		{
			ULONG refc = m_surfZero->Release( 0, "~IDirect3DVolumeTexture9 public release (surfZero)" ); (void)refc;
			Assert( !refc );
			m_surfZero = NULL;
		}
		// leave m_device alone!
	}

	GLMPRINTF(("<-A-  ~IDirect3DVolumeTexture9"));
}

HRESULT IDirect3DVolumeTexture9::LockBox(UINT Level,D3DLOCKED_BOX* pLockedVolume,CONST D3DBOX* pBox,DWORD Flags)
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( m_device );

	GLMTexLockParams lockreq;
	memset( &lockreq, 0, sizeof(lockreq) );
	
	lockreq.m_tex		= this->m_tex;
	lockreq.m_face		= 0;
	lockreq.m_mip		= Level;

	lockreq.m_region.xmin = pBox->Left;
	lockreq.m_region.ymin = pBox->Top;
	lockreq.m_region.zmin = pBox->Front;
	lockreq.m_region.xmax = pBox->Right;
	lockreq.m_region.ymax = pBox->Bottom;
	lockreq.m_region.zmax = pBox->Back;
	
	char	*lockAddress;
	int		yStride;
	int		zStride;
	
	lockreq.m_tex->Lock( &lockreq, &lockAddress, &yStride, &zStride );

	pLockedVolume->RowPitch = yStride;
	pLockedVolume->SlicePitch = yStride;
	pLockedVolume->pBits = lockAddress;	
	
	return S_OK;
}

HRESULT IDirect3DVolumeTexture9::UnlockBox(UINT Level)
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( m_device );

	GLMTexLockParams lockreq;
	memset( &lockreq, 0, sizeof(lockreq) );
	
	lockreq.m_tex		= this->m_tex;
	lockreq.m_face		= 0;
	lockreq.m_mip		= Level;

	this->m_tex->Unlock( &lockreq );
	
	return S_OK;
}

HRESULT IDirect3DVolumeTexture9::GetLevelDesc( UINT Level, D3DVOLUME_DESC *pDesc )
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( m_device );

	if (Level > static_cast<uint>(m_tex->m_layout->m_mipCount))
	{
		DXABSTRACT_BREAK_ON_ERROR();
	}

	D3DVOLUME_DESC result = m_volDescZero;
	// then mutate it for the level of interest
	
	GLMTexLayoutSlice *slice = &m_tex->m_layout->m_slices[ m_tex->CalcSliceIndex( 0, Level ) ];

	result.Width = slice->m_xSize;
	result.Height = slice->m_ySize;
	result.Depth = slice->m_zSize;
	
	*pDesc = result;

	return S_OK;
}

// ------------------------------------------------------------------------------------------------------------------------------ //

#ifdef OSX

#pragma mark ----- IDirect3DSurface9

#endif

IDirect3DSurface9::~IDirect3DSurface9()
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( m_device );

	// not much to do here, but good to verify that these things are being freed (and they are)
	//GLMPRINTF(("-A-  ~IDirect3DSurface9 - signpost"));

	if (m_device)
	{
		GLMPRINTF(("-A-  ~IDirect3DSurface9 - taking real delete path on %08x device %08x", this, m_device));
		m_device->ReleasedSurface( this );

		memset( &m_desc, 0, sizeof(m_desc) );

		if (m_restype != 0)	// signal that we are a surface that owns this tex (render target)
		{
			if (m_tex)
			{
				GLMPRINTF(("-A- ~IDirect3DSurface9 deleted '%s' @ %08x (GLM %08x) %s",m_tex->m_layout->m_layoutSummary, this, m_tex, m_tex->m_debugLabel ? m_tex->m_debugLabel : "" ));

				m_device->ReleasedCGLMTex( m_tex );

				m_tex->m_ctx->DelTex( m_tex );
				m_tex = NULL;
			}
			else
			{
				GLMPRINTF(( "-A- ~IDirect3DSurface9 : whoops, no tex to delete here ?" ));
			}		
		}
		else
		{
			m_tex = NULL;	// we are just a view on the tex, we don't own the tex, do not delete it
		}

		m_face = m_mip = 0;
		
		m_device = NULL;
	}
	else
	{
		GLMPRINTF(("-A-  ~IDirect3DSurface9 - taking strange delete path on %08x device %08x", this, m_device));
	}
}

HRESULT IDirect3DSurface9::LockRect(D3DLOCKED_RECT* pLockedRect,CONST RECT* pRect,DWORD Flags)
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( m_device );
	GLMTexLockParams lockreq;
	memset( &lockreq, 0, sizeof(lockreq) );
	
	lockreq.m_tex	= this->m_tex;
	lockreq.m_face	= this->m_face;
	lockreq.m_mip	= this->m_mip;

	lockreq.m_region.xmin = pRect->left;
	lockreq.m_region.ymin = pRect->top;
	lockreq.m_region.zmin = 0;
	lockreq.m_region.xmax = pRect->right;
	lockreq.m_region.ymax = pRect->bottom;
	lockreq.m_region.zmax = 1;
	
	if ((Flags & (D3DLOCK_READONLY | D3DLOCK_NOSYSLOCK)) == (D3DLOCK_READONLY | D3DLOCK_NOSYSLOCK) )
	{
		// smells like readback, force texel readout
		lockreq.m_readback = true;
	}
	
	char	*lockAddress;
	int		yStride;
	int		zStride;
	
	lockreq.m_tex->Lock( &lockreq, &lockAddress, &yStride, &zStride );

	pLockedRect->Pitch = yStride;
	pLockedRect->pBits = lockAddress;
	
	return S_OK;
}

HRESULT IDirect3DSurface9::UnlockRect()
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( m_device );
	GLMTexLockParams lockreq;
	memset( &lockreq, 0, sizeof(lockreq) );
	
	lockreq.m_tex	= this->m_tex;
	lockreq.m_face	= this->m_face;
	lockreq.m_mip	= this->m_mip;

	lockreq.m_tex->Unlock( &lockreq );

	return S_OK;
}

HRESULT IDirect3DSurface9::GetDesc(D3DSURFACE_DESC *pDesc)
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( m_device );
	*pDesc = m_desc;
	return S_OK;
}


// ------------------------------------------------------------------------------------------------------------------------------ //


#ifdef OSX

#pragma mark ----- IDirect3D9 -------------------------------------------------------

#endif

IDirect3D9::~IDirect3D9()
{
	GL_BATCH_PERF_CALL_TIMER;
	GLMPRINTF(("-A-  ~IDirect3D9 - signpost"));
}

UINT IDirect3D9::GetAdapterCount()
{	
	GL_BATCH_PERF_CALL_TIMER;
	GLMgr::NewGLMgr();				// init GL manager

	GLMDisplayDB *db = GetDisplayDB();
	int dxAdapterCount = db->GetFakeAdapterCount();

	return dxAdapterCount;
}

static void FillD3DCaps9( const GLMRendererInfoFields &glmRendererInfo, D3DCAPS9* pCaps )
{
	// fill in the pCaps record for adapter... we zero most of it and just fill in the fields that we think the caller wants.
	Q_memset( pCaps, 0, sizeof(*pCaps) );


	/* Device Info */
	pCaps->DeviceType					=	D3DDEVTYPE_HAL;

	/* Caps from DX7 Draw */
	pCaps->Caps							=	0;									// does anyone look at this ?

	pCaps->Caps2						=	D3DCAPS2_DYNAMICTEXTURES;    
	/* Cursor Caps */
	pCaps->CursorCaps					=	0;									// nobody looks at this

	/* 3D Device Caps */
	pCaps->DevCaps						=	D3DDEVCAPS_HWTRANSFORMANDLIGHT;

	pCaps->TextureCaps					=	D3DPTEXTURECAPS_CUBEMAP | D3DPTEXTURECAPS_MIPCUBEMAP | D3DPTEXTURECAPS_NONPOW2CONDITIONAL | D3DPTEXTURECAPS_PROJECTED;
	// D3DPTEXTURECAPS_NOPROJECTEDBUMPENV ?
	// D3DPTEXTURECAPS_POW2 ? 
	// caller looks at POT support like this:
	//		pCaps->m_SupportsNonPow2Textures = 
	//			( !( caps.TextureCaps & D3DPTEXTURECAPS_POW2 ) || 
	//			( caps.TextureCaps & D3DPTEXTURECAPS_NONPOW2CONDITIONAL ) );
	// so we should set D3DPTEXTURECAPS_NONPOW2CONDITIONAL bit ?


	pCaps->PrimitiveMiscCaps			=	0;									//only the HDR setup looks at this for D3DPMISCCAPS_SEPARATEALPHABLEND.
	// ? D3DPMISCCAPS_SEPARATEALPHABLEND 
	// ? D3DPMISCCAPS_BLENDOP
	// ? D3DPMISCCAPS_CLIPPLANESCALEDPOINTS
	// ? D3DPMISCCAPS_CLIPTLVERTS D3DPMISCCAPS_COLORWRITEENABLE D3DPMISCCAPS_MASKZ D3DPMISCCAPS_TSSARGTEMP


	pCaps->RasterCaps					=	D3DPRASTERCAPS_SCISSORTEST
		|	D3DPRASTERCAPS_SLOPESCALEDEPTHBIAS	// ref'd in CShaderDeviceMgrDx8::ComputeCapsFromD3D
		|	D3DPRASTERCAPS_DEPTHBIAS			// ref'd in CShaderDeviceMgrDx8::ComputeCapsFromD3D
		;

	pCaps->TextureFilterCaps			=	D3DPTFILTERCAPS_MINFANISOTROPIC | D3DPTFILTERCAPS_MAGFANISOTROPIC;

	pCaps->MaxTextureWidth				=	4096;
	pCaps->MaxTextureHeight				=	4096;
	pCaps->MaxVolumeExtent				=	1024;	//guesses

	pCaps->MaxTextureAspectRatio		=	0;		// imply no limit on AR

	pCaps->MaxAnisotropy				=	glmRendererInfo.m_maxAniso;

	pCaps->TextureOpCaps				=	D3DTEXOPCAPS_ADD | D3DTEXOPCAPS_MODULATE2X;	//guess
	//DWORD   MaxTextureBlendStages;
	//DWORD   MaxSimultaneousTextures;

	pCaps->VertexProcessingCaps			=	D3DVTXPCAPS_TEXGEN_SPHEREMAP;

	pCaps->MaxActiveLights				=	8;		// guess


	// MaxUserClipPlanes.  A bit complicated..
	// it's difficult to make this fluid without teaching the engine about a cap that could change during run.

	// start it out set to '2'.
	// turn it off, if we're in GLSL mode but do not have native clip plane capability.
	pCaps->MaxUserClipPlanes			=	2;		// assume good news

	// is user asking for it to be off ?
	if ( CommandLine()->CheckParm( "-nouserclip" ) )
	{
		pCaps->MaxUserClipPlanes		=	0;
	}

	pCaps->MaxVertexBlendMatrices		=	0;		// see if anyone cares
	pCaps->MaxVertexBlendMatrixIndex	=	0;		// see if anyone cares

	pCaps->MaxPrimitiveCount			=	32768;	// guess
	pCaps->MaxStreams					=	D3D_MAX_STREAMS;		// guess

	pCaps->VertexShaderVersion			=	0x300;	// model 3.0
	pCaps->MaxVertexShaderConst			=	DXABSTRACT_VS_PARAM_SLOTS;	// number of vertex shader constant registers

	pCaps->PixelShaderVersion			=	0x300;	// model 3.0

	// Here are the DX9 specific ones
	pCaps->DevCaps2						=	D3DDEVCAPS2_STREAMOFFSET;

	pCaps->PS20Caps.NumInstructionSlots	=	512;	// guess
	// only examined once:
	// pCaps->m_SupportsPixelShaders_2_b = ( ( caps.PixelShaderVersion & 0xffff ) >= 0x0200) && (caps.PS20Caps.NumInstructionSlots >= 512);
	//pCaps->m_SupportsPixelShaders_2_b = 1;

	pCaps->NumSimultaneousRTs					=	1;         // Will be at least 1
	pCaps->MaxVertexShader30InstructionSlots	=	0; 
	pCaps->MaxPixelShader30InstructionSlots		=	0;

#if DX_TO_GL_ABSTRACTION
	pCaps->FakeSRGBWrite			=	!glmRendererInfo.m_hasGammaWrites;
	pCaps->CanDoSRGBReadFromRTs		=	!glmRendererInfo.m_cantAttachSRGB;
	pCaps->MixedSizeTargets			=	glmRendererInfo.m_hasMixedAttachmentSizes;
#endif
}

HRESULT IDirect3D9::GetDeviceCaps(UINT Adapter, D3DDEVTYPE DeviceType, D3DCAPS9* pCaps)
{
	GL_BATCH_PERF_CALL_TIMER;
	// Generally called from "CShaderDeviceMgrDx8::ComputeCapsFromD3D" in ShaderDeviceDX8.cpp

	// "Adapter" is used to index amongst the set of fake-adapters maintained in the display DB
	GLMDisplayDB *db = GetDisplayDB();
	int glmRendererIndex = -1;
	int glmDisplayIndex = -1;
	
	GLMRendererInfoFields	glmRendererInfo;
	GLMDisplayInfoFields	glmDisplayInfo;
	
	bool result = db->GetFakeAdapterInfo( Adapter, &glmRendererIndex, &glmDisplayIndex, &glmRendererInfo, &glmDisplayInfo ); (void)result;
	Assert (!result);
	// just leave glmRendererInfo filled out for subsequent code to look at as needed.

	FillD3DCaps9( glmRendererInfo, pCaps );

	return S_OK;
}

HRESULT IDirect3D9::GetAdapterIdentifier( UINT Adapter, DWORD Flags, D3DADAPTER_IDENTIFIER9* pIdentifier )
{
	GL_BATCH_PERF_CALL_TIMER;
	// Generally called from "CShaderDeviceMgrDx8::ComputeCapsFromD3D" in ShaderDeviceDX8.cpp

	Assert( Flags == D3DENUM_WHQL_LEVEL );	// we're not handling any other queries than this yet
	
	Q_memset( pIdentifier, 0, sizeof(*pIdentifier) );

	GLMDisplayDB *db = GetDisplayDB();
	int glmRendererIndex = -1;
	int glmDisplayIndex = -1;
	
	GLMRendererInfoFields	glmRendererInfo;
	GLMDisplayInfoFields	glmDisplayInfo;
	
	// the D3D "Adapter" number feeds the fake adapter index
	bool result = db->GetFakeAdapterInfo( Adapter, &glmRendererIndex, &glmDisplayIndex, &glmRendererInfo, &glmDisplayInfo ); (void)result;
	Assert (!result);

#ifndef OSX
	if( glmRendererInfo.m_rendererID )
#endif
	{
		const char *pRenderer = GLMDecode( eGL_RENDERER, glmRendererInfo.m_rendererID & 0x00FFFF00 );

		Q_snprintf( pIdentifier->Driver, sizeof(pIdentifier->Driver), "OpenGL %s (%08x)",
			pRenderer, glmRendererInfo.m_rendererID	);

		Q_snprintf( pIdentifier->Description, sizeof(pIdentifier->Description), "%s - %dx%d - %dMB VRAM",
			pRenderer,
			glmDisplayInfo.m_displayPixelWidth, glmDisplayInfo.m_displayPixelHeight,
			glmRendererInfo.m_vidMemory >> 20 );
	}
#ifndef OSX
	else
	{
		static CDynamicFunctionOpenGL< true, const GLubyte *( APIENTRY *)(GLenum name), const GLubyte * > glGetString("glGetString");

		const char *pszStringVendor = ( const char * )glGetString( GL_VENDOR );		// NVIDIA Corporation
		const char *pszStringRenderer = ( const char * )glGetString( GL_RENDERER );   // GeForce GTX 680/PCIe/SSE2
		const char *pszStringVersion = ( const char * )glGetString( GL_VERSION );     // 4.2.0 NVIDIA 304.22

		Q_snprintf( pIdentifier->Driver, sizeof( pIdentifier->Driver ), "OpenGL %s (%s)",
			pszStringVendor, pszStringRenderer );
		Q_snprintf( pIdentifier->Description, sizeof( pIdentifier->Description ), "%s (%s) %s - %dx%d",
			pszStringVendor, pszStringRenderer, pszStringVersion,
			glmDisplayInfo.m_displayPixelWidth, glmDisplayInfo.m_displayPixelHeight );
	}
#endif // !OSX

	pIdentifier->VendorId				= glmRendererInfo.m_pciVendorID;	// 4318;
	pIdentifier->DeviceId				= glmRendererInfo.m_pciDeviceID;	// 401;
	pIdentifier->SubSysId				= 0;								// 3358668866;
	pIdentifier->Revision				= 0;								// 162;
	pIdentifier->VideoMemory			= glmRendererInfo.m_vidMemory;		// amount of video memory in bytes

	#if 0
		// this came from the shaderapigl effort	
		Q_strncpy( pIdentifier->Driver, "Fake-Video-Card", MAX_DEVICE_IDENTIFIER_STRING );
		Q_strncpy( pIdentifier->Description, "Fake-Video-Card", MAX_DEVICE_IDENTIFIER_STRING );
		pIdentifier->VendorId				= 4318;
		pIdentifier->DeviceId				= 401;
		pIdentifier->SubSysId				= 3358668866;
		pIdentifier->Revision				= 162;
	#endif
	
	return S_OK;
}

HRESULT IDirect3D9::CheckDeviceFormat(UINT Adapter,D3DDEVTYPE DeviceType,D3DFORMAT AdapterFormat,DWORD Usage,D3DRESOURCETYPE RType,D3DFORMAT CheckFormat)
{
	GL_BATCH_PERF_CALL_TIMER;
	if (0)	// hush for now, less spew
	{
		GLMPRINTF(("-X- ** IDirect3D9::CheckDeviceFormat:  \n -- Adapter=%d || DeviceType=%4x:%s || AdapterFormat=%8x:%s\n -- RType       %8x: %s\n -- CheckFormat %8x: %s\n -- Usage       %8x: %s",
			Adapter,													
			DeviceType,		GLMDecode(eD3D_DEVTYPE, DeviceType),				
			AdapterFormat,	GLMDecode(eD3D_FORMAT, AdapterFormat),
			RType,			GLMDecode(eD3D_RTYPE, RType),							
			CheckFormat,	GLMDecode(eD3D_FORMAT, CheckFormat),
			Usage,			GLMDecodeMask( eD3D_USAGE, Usage ) ));			
	}

	HRESULT result = D3DERR_NOTAVAILABLE;	// failure
	
	DWORD	knownUsageMask =	D3DUSAGE_RENDERTARGET | D3DUSAGE_DEPTHSTENCIL | D3DUSAGE_DYNAMIC | D3DUSAGE_AUTOGENMIPMAP
							|	D3DUSAGE_QUERY_SRGBREAD | D3DUSAGE_QUERY_FILTER | D3DUSAGE_QUERY_SRGBWRITE | D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING
							|	D3DUSAGE_QUERY_VERTEXTEXTURE;
	(void)knownUsageMask;

	//	FramebufferSRGB stuff.
	//	basically a format is only allowed to have SRGB usage for writing, if you have the framebuffer SRGB extension.
	//	so, check for that capability with GLM adapter db, and if it's not there, don't mark that bit as usable in any of our formats.
	GLMDisplayDB *db = GetDisplayDB();
	int glmRendererIndex = -1;
	int glmDisplayIndex = -1;
	
	GLMRendererInfoFields	glmRendererInfo;
	GLMDisplayInfoFields	glmDisplayInfo;
	
	bool dbresult = db->GetFakeAdapterInfo( Adapter, &glmRendererIndex, &glmDisplayIndex, &glmRendererInfo, &glmDisplayInfo ); (void)dbresult;
	Assert (!dbresult);

	Assert ((Usage & knownUsageMask) == Usage);

	DWORD	legalUsage = 0;
	switch( AdapterFormat )
	{
		case	D3DFMT_X8R8G8B8:
			switch( RType )
			{
				case	D3DRTYPE_TEXTURE:
					switch( CheckFormat )
					{
						case D3DFMT_DXT1:
						case D3DFMT_DXT3:
						case D3DFMT_DXT5:
													legalUsage	=	D3DUSAGE_DYNAMIC | D3DUSAGE_AUTOGENMIPMAP | D3DUSAGE_QUERY_FILTER;
													legalUsage	|=	D3DUSAGE_QUERY_SRGBREAD;
													
													//open question: is auto gen of mipmaps is allowed or attempted on any DXT textures.
						break;

						case D3DFMT_A8R8G8B8:		legalUsage	=	D3DUSAGE_DYNAMIC | D3DUSAGE_AUTOGENMIPMAP | D3DUSAGE_QUERY_FILTER;
													legalUsage |=	D3DUSAGE_RENDERTARGET | D3DUSAGE_QUERY_SRGBREAD | D3DUSAGE_QUERY_SRGBWRITE | D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING;
						break;

//$ TODO: Need to merge bitmap changes over from Dota to get these formats.
#if 0
						case D3DFMT_A2R10G10B10:	legalUsage	=	D3DUSAGE_DYNAMIC | D3DUSAGE_AUTOGENMIPMAP | D3DUSAGE_QUERY_FILTER;
													legalUsage |=	D3DUSAGE_RENDERTARGET | D3DUSAGE_QUERY_SRGBREAD | D3DUSAGE_QUERY_SRGBWRITE | D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING;
						break;

						case D3DFMT_A2B10G10R10:	legalUsage	=	D3DUSAGE_DYNAMIC | D3DUSAGE_AUTOGENMIPMAP | D3DUSAGE_QUERY_FILTER;
													legalUsage |=	D3DUSAGE_RENDERTARGET | D3DUSAGE_QUERY_SRGBREAD | D3DUSAGE_QUERY_SRGBWRITE | D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING;
						break;
#endif

						case D3DFMT_R32F:			legalUsage	=	D3DUSAGE_DYNAMIC | D3DUSAGE_AUTOGENMIPMAP | D3DUSAGE_QUERY_FILTER;
													legalUsage |=	D3DUSAGE_RENDERTARGET | D3DUSAGE_QUERY_SRGBREAD | D3DUSAGE_QUERY_SRGBWRITE | D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING;
						break;

						case D3DFMT_A16B16G16R16:
													legalUsage	=	D3DUSAGE_DYNAMIC | D3DUSAGE_AUTOGENMIPMAP | D3DUSAGE_QUERY_FILTER;
													legalUsage |=	D3DUSAGE_RENDERTARGET | D3DUSAGE_QUERY_SRGBREAD | D3DUSAGE_QUERY_SRGBWRITE | D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING;
						break;

						case D3DFMT_A16B16G16R16F:	legalUsage	=	D3DUSAGE_DYNAMIC | D3DUSAGE_AUTOGENMIPMAP | D3DUSAGE_RENDERTARGET | D3DUSAGE_QUERY_SRGBREAD | D3DUSAGE_QUERY_SRGBWRITE;

													if ( !glmRendererInfo.m_atiR5xx )
													{
														legalUsage |= D3DUSAGE_QUERY_FILTER | D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING;
													}
						break;

						case D3DFMT_A32B32G32R32F:	legalUsage	=	D3DUSAGE_DYNAMIC | D3DUSAGE_AUTOGENMIPMAP | D3DUSAGE_RENDERTARGET | D3DUSAGE_QUERY_SRGBREAD | D3DUSAGE_QUERY_SRGBWRITE;

													if ( !glmRendererInfo.m_atiR5xx && !glmRendererInfo.m_nvG7x )
													{
														legalUsage |= D3DUSAGE_QUERY_FILTER | D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING;
													}
						break;

						case D3DFMT_R5G6B5:			legalUsage	=	D3DUSAGE_DYNAMIC | D3DUSAGE_AUTOGENMIPMAP | D3DUSAGE_QUERY_FILTER;
						break;

						//-----------------------------------------------------------
						// these come in from TestTextureFormat in ColorFormatDX8.cpp which is being driven by InitializeColorInformation...
						// which is going to try all 8 combinations of (vertex texturable / render targetable / filterable ) on every image format it knows.

						case D3DFMT_R8G8B8:			legalUsage	=	D3DUSAGE_DYNAMIC | D3DUSAGE_AUTOGENMIPMAP | D3DUSAGE_QUERY_FILTER;
													legalUsage	|=	D3DUSAGE_QUERY_SRGBREAD;
						break;
												
						case D3DFMT_X8R8G8B8:		legalUsage	=	D3DUSAGE_DYNAMIC | D3DUSAGE_AUTOGENMIPMAP | D3DUSAGE_QUERY_FILTER;
													legalUsage	|=	D3DUSAGE_QUERY_SRGBREAD | D3DUSAGE_QUERY_SRGBWRITE;
						break;

							// one and two channel textures... we'll have to fake these as four channel tex if we want to support them
						case D3DFMT_L8:				legalUsage	=	D3DUSAGE_DYNAMIC | D3DUSAGE_AUTOGENMIPMAP | D3DUSAGE_QUERY_FILTER;
						break;

						case D3DFMT_A8L8:			legalUsage	=	D3DUSAGE_DYNAMIC | D3DUSAGE_AUTOGENMIPMAP | D3DUSAGE_QUERY_FILTER;
						break;

						case D3DFMT_A8:				legalUsage	=	D3DUSAGE_DYNAMIC | D3DUSAGE_AUTOGENMIPMAP | D3DUSAGE_QUERY_FILTER;
						break;
						
							// going to need to go back and double check all of these..
						case D3DFMT_X1R5G5B5:		legalUsage	=	D3DUSAGE_DYNAMIC | D3DUSAGE_AUTOGENMIPMAP | D3DUSAGE_QUERY_FILTER;
						break;
						
						case D3DFMT_A4R4G4B4:		legalUsage	=	D3DUSAGE_DYNAMIC | D3DUSAGE_AUTOGENMIPMAP | D3DUSAGE_QUERY_FILTER;
						break;

						case D3DFMT_A1R5G5B5:		legalUsage	=	D3DUSAGE_DYNAMIC | D3DUSAGE_AUTOGENMIPMAP | D3DUSAGE_QUERY_FILTER;
						break;
						
						case D3DFMT_V8U8:			legalUsage	=	D3DUSAGE_DYNAMIC | D3DUSAGE_AUTOGENMIPMAP | D3DUSAGE_QUERY_FILTER;
						break;

						case D3DFMT_Q8W8V8U8:		legalUsage	=	D3DUSAGE_DYNAMIC | D3DUSAGE_AUTOGENMIPMAP | D3DUSAGE_QUERY_FILTER;
							// what the heck is QWVU8 ... ?
						break;

						case D3DFMT_X8L8V8U8:		legalUsage	=	D3DUSAGE_DYNAMIC | D3DUSAGE_AUTOGENMIPMAP | D3DUSAGE_QUERY_FILTER;
							// what the heck is XLVU8 ... ?
						break;

							// formats with depth...
							
						case	D3DFMT_D16:			legalUsage =	D3DUSAGE_DYNAMIC | D3DUSAGE_RENDERTARGET | D3DUSAGE_DEPTHSTENCIL;
							// just a guess on the legal usages
						break;

						case	D3DFMT_D24S8:		legalUsage =	D3DUSAGE_DYNAMIC | D3DUSAGE_RENDERTARGET | D3DUSAGE_DEPTHSTENCIL;
							// just a guess on the legal usages
						break;

							// vendor formats... try marking these all invalid for now
						case	D3DFMT_NV_INTZ:
						case	D3DFMT_NV_RAWZ:
						case	D3DFMT_NV_NULL:
						case	D3DFMT_ATI_D16:
						case	D3DFMT_ATI_D24S8:
						case	D3DFMT_ATI_2N:
						case	D3DFMT_ATI_1N:
							legalUsage = 0;
						break;

						//-----------------------------------------------------------
												
						default:
							Assert(!"Unknown check format");
							result = D3DERR_NOTAVAILABLE;
						break;
					}

					if ((Usage & legalUsage) == Usage)
					{
						result = S_OK;
					}
					else
					{
						DWORD unsatBits = Usage & (~legalUsage);	// clear the bits of the req that were legal, leaving the illegal ones
						unsatBits;
						GLMPRINTF(( "-X- --> NOT OK: flags %8x:%s", unsatBits,GLMDecodeMask( eD3D_USAGE, unsatBits ) ));
						result = D3DERR_NOTAVAILABLE;
					}
				break;				
				
				case	D3DRTYPE_SURFACE:
					switch( static_cast<uint>(CheckFormat) )
					{
						case	0x434f5441:
						case	0x41415353:
							result = D3DERR_NOTAVAILABLE;
						break;
							
						case	D3DFMT_D24S8:
							result = S_OK;
						break;
						//** IDirect3D9::CheckDeviceFormat  adapter=0, DeviceType=   1:D3DDEVTYPE_HAL, AdapterFormat=       5:D3DFMT_X8R8G8B8, RType=   1:D3DRTYPE_SURFACE, CheckFormat=434f5441:UNKNOWN
						//** IDirect3D9::CheckDeviceFormat  adapter=0, DeviceType=   1:D3DDEVTYPE_HAL, AdapterFormat=       5:D3DFMT_X8R8G8B8, RType=   1:D3DRTYPE_SURFACE, CheckFormat=41415353:UNKNOWN
						//** IDirect3D9::CheckDeviceFormat  adapter=0, DeviceType=   1:D3DDEVTYPE_HAL, AdapterFormat=       5:D3DFMT_X8R8G8B8, RType=   1:D3DRTYPE_SURFACE, CheckFormat=434f5441:UNKNOWN
						//** IDirect3D9::CheckDeviceFormat  adapter=0, DeviceType=   1:D3DDEVTYPE_HAL, AdapterFormat=       5:D3DFMT_X8R8G8B8, RType=   1:D3DRTYPE_SURFACE, CheckFormat=41415353:UNKNOWN
					}
				break;
				
				default:
					Assert(!"Unknown resource type");
					result = D3DERR_NOTAVAILABLE;
				break;
			}
		break;
		
		default:
			Assert(!"Unknown adapter format");
			result = D3DERR_NOTAVAILABLE;
		break;
	}
	
	return result;
}

UINT IDirect3D9::GetAdapterModeCount(UINT Adapter,D3DFORMAT Format)
{
	GL_BATCH_PERF_CALL_TIMER;
	GLMPRINTF(( "-X- IDirect3D9::GetAdapterModeCount: Adapter=%d || Format=%8x:%s", Adapter, Format, GLMDecode(eD3D_FORMAT, Format) ));

	uint modeCount=0;
	
	GLMDisplayDB *db = GetDisplayDB();
	int glmRendererIndex = -1;
	int glmDisplayIndex = -1;
	
	GLMRendererInfoFields	glmRendererInfo;
	GLMDisplayInfoFields	glmDisplayInfo;
	
	// the D3D "Adapter" number feeds the fake adapter index
	bool result = db->GetFakeAdapterInfo( Adapter, &glmRendererIndex, &glmDisplayIndex, &glmRendererInfo, &glmDisplayInfo ); (void)result;
	Assert (!result);

	modeCount = db->GetModeCount( glmRendererIndex, glmDisplayIndex );
	GLMPRINTF(( "-X-   --> result is %d", modeCount ));		
	
	return modeCount;
}

HRESULT IDirect3D9::EnumAdapterModes(UINT Adapter,D3DFORMAT Format,UINT Mode,D3DDISPLAYMODE* pMode)
{
	GL_BATCH_PERF_CALL_TIMER;
	GLMPRINTF(( "-X- IDirect3D9::EnumAdapterModes:    Adapter=%d || Format=%8x:%s || Mode=%d", Adapter, Format, GLMDecode(eD3D_FORMAT, Format), Mode ));

	Assert(Format==D3DFMT_X8R8G8B8);

	GLMDisplayDB *db = GetDisplayDB();
	
	int glmRendererIndex = -1;
	int glmDisplayIndex = -1;
	
	GLMRendererInfoFields		glmRendererInfo;
	GLMDisplayInfoFields		glmDisplayInfo;
	GLMDisplayModeInfoFields	glmModeInfo;
	
	// the D3D "Adapter" number feeds the fake adapter index
	bool result = db->GetFakeAdapterInfo( Adapter, &glmRendererIndex, &glmDisplayIndex, &glmRendererInfo, &glmDisplayInfo );
	Assert (!result);
		if (result) return D3DERR_NOTAVAILABLE;

	bool result2 = db->GetModeInfo( glmRendererIndex, glmDisplayIndex, Mode, &glmModeInfo );
	Assert( !result2 );
		if (result2) return D3DERR_NOTAVAILABLE;

	pMode->Width		= glmModeInfo.m_modePixelWidth;
	pMode->Height		= glmModeInfo.m_modePixelHeight;
	pMode->RefreshRate	= glmModeInfo.m_modeRefreshHz;		// "adapter default"
	pMode->Format		= Format;							// whatever you asked for ?
	
	GLMPRINTF(( "-X- IDirect3D9::EnumAdapterModes returning mode size (%d,%d) and D3DFMT_X8R8G8B8",pMode->Width,pMode->Height ));
	return S_OK;	
}

HRESULT IDirect3D9::CheckDeviceType(UINT Adapter,D3DDEVTYPE DevType,D3DFORMAT AdapterFormat,D3DFORMAT BackBufferFormat,BOOL bWindowed)
{
	GL_BATCH_PERF_CALL_TIMER;
	//FIXME: we just say "OK" on any query

	GLMPRINTF(( "-X- IDirect3D9::CheckDeviceType:    Adapter=%d || DevType=%d:%s || AdapterFormat=%d:%s || BackBufferFormat=%d:%s || bWindowed=%d",
		Adapter,
		DevType, GLMDecode(eD3D_DEVTYPE,DevType),
		AdapterFormat, GLMDecode(eD3D_FORMAT, AdapterFormat),
		BackBufferFormat, GLMDecode(eD3D_FORMAT, BackBufferFormat),
		(int) bWindowed ));
		
	return S_OK;
}

HRESULT IDirect3D9::GetAdapterDisplayMode(UINT Adapter,D3DDISPLAYMODE* pMode)
{
	GL_BATCH_PERF_CALL_TIMER;
	// asking what the current mode is
	GLMPRINTF(("-X- IDirect3D9::GetAdapterDisplayMode: Adapter=%d", Adapter ));

	GLMDisplayDB *db = GetDisplayDB();

	int glmRendererIndex = -1;
	int glmDisplayIndex = -1;
	
	GLMRendererInfoFields		glmRendererInfo;
	GLMDisplayInfoFields		glmDisplayInfo;
	GLMDisplayModeInfoFields	glmModeInfo;
	
	// the D3D "Adapter" number feeds the fake adapter index
	bool result = db->GetFakeAdapterInfo( Adapter, &glmRendererIndex, &glmDisplayIndex, &glmRendererInfo, &glmDisplayInfo );
	Assert(!result);
		if (result)	return D3DERR_INVALIDCALL;

	int modeIndex = -1;	// pass -1 as a mode index to find out about whatever the current mode is on the selected display

	bool modeResult = db->GetModeInfo( glmRendererIndex, glmDisplayIndex, modeIndex, &glmModeInfo );
	Assert (!modeResult);
		if (modeResult)	return D3DERR_INVALIDCALL;

	pMode->Width		= glmModeInfo.m_modePixelWidth;
	pMode->Height		= glmModeInfo.m_modePixelHeight;
	pMode->RefreshRate	= glmModeInfo.m_modeRefreshHz;		// "adapter default"
	pMode->Format		= D3DFMT_X8R8G8B8;					//FIXME, this is a SWAG

	return S_OK;
}

HRESULT IDirect3D9::CheckDepthStencilMatch(UINT Adapter,D3DDEVTYPE DeviceType,D3DFORMAT AdapterFormat,D3DFORMAT RenderTargetFormat,D3DFORMAT DepthStencilFormat)
{
	GL_BATCH_PERF_CALL_TIMER;
	GLMPRINTF(("-X- IDirect3D9::CheckDepthStencilMatch:    Adapter=%d || DevType=%d:%s || AdapterFormat=%d:%s || RenderTargetFormat=%d:%s || DepthStencilFormat=%d:%s",
		Adapter,
		DeviceType, GLMDecode(eD3D_DEVTYPE,DeviceType),
		AdapterFormat, GLMDecode(eD3D_FORMAT, AdapterFormat),
		RenderTargetFormat, GLMDecode(eD3D_FORMAT, RenderTargetFormat),
		DepthStencilFormat, GLMDecode(eD3D_FORMAT, DepthStencilFormat) ));
	
	// one known request looks like this:
	// AdapterFormat=5:D3DFMT_X8R8G8B8 || RenderTargetFormat=3:D3DFMT_A8R8G8B8 || DepthStencilFormat=2:D3DFMT_D24S8
	
	// return S_OK for that one combo, DXABSTRACT_BREAK_ON_ERROR() on anything else
	HRESULT result = D3DERR_NOTAVAILABLE;	// failure
	
	switch( AdapterFormat )
	{
		case	D3DFMT_X8R8G8B8:
		{
			if ( (RenderTargetFormat == D3DFMT_A8R8G8B8) && (DepthStencilFormat == D3DFMT_D24S8) )
			{
				result = S_OK;
			}
		}
		break;
	}
	
	Assert( result == S_OK );

	return result;
}

HRESULT IDirect3D9::CheckDeviceMultiSampleType( UINT Adapter,D3DDEVTYPE DeviceType,D3DFORMAT SurfaceFormat,BOOL Windowed,D3DMULTISAMPLE_TYPE MultiSampleType,DWORD* pQualityLevels )
{
	GL_BATCH_PERF_CALL_TIMER;
	GLMDisplayDB *db = GetDisplayDB();

	int glmRendererIndex = -1;
	int glmDisplayIndex = -1;
	
	GLMRendererInfoFields		glmRendererInfo;
	GLMDisplayInfoFields		glmDisplayInfo;
	//GLMDisplayModeInfoFields	glmModeInfo;
	
	// the D3D "Adapter" number feeds the fake adapter index
	bool result = db->GetFakeAdapterInfo( Adapter, &glmRendererIndex, &glmDisplayIndex, &glmRendererInfo, &glmDisplayInfo );
	Assert( !result );
	if ( result )
		return D3DERR_INVALIDCALL;

	
	if ( !CommandLine()->FindParm("-glmenabletrustmsaa") )
	{
		// These ghetto drivers don't get MSAA
		if ( ( glmRendererInfo.m_nvG7x || glmRendererInfo.m_atiR5xx ) && ( MultiSampleType > D3DMULTISAMPLE_NONE ) )
		{
			if ( pQualityLevels )
			{
				*pQualityLevels = 0;
			}
			return D3DERR_NOTAVAILABLE;
		}
	}

	switch ( MultiSampleType )
	{
		case D3DMULTISAMPLE_NONE:		// always return true
			if ( pQualityLevels )
			{
				*pQualityLevels = 1;
			}
			return S_OK;
		break;

		case D3DMULTISAMPLE_2_SAMPLES:
		case D3DMULTISAMPLE_4_SAMPLES:
		case D3DMULTISAMPLE_6_SAMPLES:
		case D3DMULTISAMPLE_8_SAMPLES:
			// note the fact that the d3d enums for 2, 4, 6, 8 samples are equal to 2,4,6,8...
			if (glmRendererInfo.m_maxSamples >= (int)MultiSampleType )
			{
				if ( pQualityLevels )
				{
					*pQualityLevels = 1;
				}
				return S_OK;
			}
			else
			{
				return D3DERR_NOTAVAILABLE;
			}
		break;
		
		default:
			if ( pQualityLevels )
			{
				*pQualityLevels = 0;
			}
			return D3DERR_NOTAVAILABLE;
		break;
	}
	return D3DERR_NOTAVAILABLE;
}

HRESULT IDirect3D9::CreateDevice(UINT Adapter,D3DDEVTYPE DeviceType,VD3DHWND hFocusWindow,DWORD BehaviorFlags,D3DPRESENT_PARAMETERS* pPresentationParameters,IDirect3DDevice9** ppReturnedDeviceInterface)
{
	GL_BATCH_PERF_CALL_TIMER;

#if GLMDEBUG
	GLMDebugPrintf( "WARNING: GLMEBUG is 1, perf. is going to be low!" );
	Warning( "WARNING: GLMEBUG is 1, perf. is going to be low!" );
#endif
#if !TOGL_SUPPORT_NULL_DEVICE	
	if (DeviceType == D3DDEVTYPE_NULLREF)
	{
		Error( "Must define TOGL_SUPPORT_NULL_DEVICE to use the NULL device" );
		DebuggerBreak();
		return E_FAIL;
	}
#endif
	
	// constrain these inputs for the time being
	// BackBufferFormat			-> A8R8G8B8
	// BackBufferCount			-> 1;
	// MultiSampleType			-> D3DMULTISAMPLE_NONE
	// AutoDepthStencilFormat	-> D3DFMT_D24S8
	
	// NULL out the return pointer so if we exit early it is not set
	*ppReturnedDeviceInterface = NULL;
	
	// assume success unless something is sour
	HRESULT result = S_OK;
	
	// relax this check for now
	//if (pPresentationParameters->BackBufferFormat != D3DFMT_A8R8G8B8)
	//{
	//	DXABSTRACT_BREAK_ON_ERROR();
	//	result = -1;
	//}
	
	//rbarris 24Aug10 - relaxing this check - we don't care if the game asks for two backbuffers, it's moot
	//if ( pPresentationParameters->BackBufferCount != 1 )
	//{
	//	DXABSTRACT_BREAK_ON_ERROR();
	//	result = D3DERR_NOTAVAILABLE;
	//}
		
	if ( pPresentationParameters->AutoDepthStencilFormat != D3DFMT_D24S8 )
	{
		DXABSTRACT_BREAK_ON_ERROR();
		result = D3DERR_NOTAVAILABLE;
	}

	if ( result == S_OK )
	{
		// create an IDirect3DDevice9
		// it will make a GLMContext and set up some drawables

		IDirect3DDevice9Params	devparams;
		memset( &devparams, 0, sizeof(devparams) );
		
		devparams.m_adapter					= Adapter;
		devparams.m_deviceType				= DeviceType;
		devparams.m_focusWindow				= hFocusWindow;				// is this meaningful?  is this a WindowRef ?  follow it up the chain..
		devparams.m_behaviorFlags			= BehaviorFlags;
		devparams.m_presentationParameters	= *pPresentationParameters;

		IDirect3DDevice9 *dev = new IDirect3DDevice9;
		
		result = dev->Create( &devparams );
		
		if ( result == S_OK )
		{
			*ppReturnedDeviceInterface = dev;
		}
		
		g_bNullD3DDevice = ( DeviceType == D3DDEVTYPE_NULLREF );
	}
	return result;
}

// ------------------------------------------------------------------------------------------------------------------------------ //

#ifdef OSX

#pragma mark ----- IDirect3DQuery9

#endif

HRESULT IDirect3DQuery9::Issue(DWORD dwIssueFlags)
{
	GL_BATCH_PERF_CALL_TIMER;
	Assert( m_device->m_nValidMarker == D3D_DEVICE_VALID_MARKER );
	// Flags field for Issue
	//	#define D3DISSUE_END (1 << 0) // Tells the runtime to issue the end of a query, changing it's state to "non-signaled".
	//	#define D3DISSUE_BEGIN (1 << 1) // Tells the runtime to issue the beginng of a query.

	// Make sure calling thread owns the GL context.
	Assert( m_ctx->m_nCurOwnerThreadId == ThreadGetCurrentId() );
		
	if (dwIssueFlags & D3DISSUE_BEGIN)
	{
		m_nIssueStartThreadID = ThreadGetCurrentId();
		m_nIssueStartDrawCallIndex = g_nTotalDrawsOrClears;
		m_nIssueStartFrameIndex = m_ctx->m_nCurFrame;
		m_nIssueStartQueryCreationCounter = CGLMQuery::s_nTotalOcclusionQueryCreatesOrDeletes;
						
		switch( m_type )
		{
			case	D3DQUERYTYPE_OCCLUSION:
				m_query->Start();	// drop "start counter" call into stream
			break;

			default:
				Assert(!"Can't use D3DISSUE_BEGIN on this query");
			break;
		}
	}
	
	if (dwIssueFlags & D3DISSUE_END)
	{
		m_nIssueEndThreadID = ThreadGetCurrentId();
		m_nIssueEndDrawCallIndex = g_nTotalDrawsOrClears;
		m_nIssueEndFrameIndex = m_ctx->m_nCurFrame;
		m_nIssueEndQueryCreationCounter = CGLMQuery::s_nTotalOcclusionQueryCreatesOrDeletes;
		
		switch( m_type )
		{
			case	D3DQUERYTYPE_OCCLUSION:
				m_query->Stop();	// drop "end counter" call into stream
			break;
			
			case	D3DQUERYTYPE_EVENT:
				m_nIssueStartThreadID = m_nIssueEndThreadID;
				m_nIssueStartDrawCallIndex = m_nIssueEndDrawCallIndex;
				m_nIssueStartFrameIndex = m_nIssueEndFrameIndex;
				m_nIssueStartQueryCreationCounter = m_nIssueEndQueryCreationCounter;
				
				// End is very weird with respect to Events (fences).
				// DX9 docs say to use End to put the fence in the stream.  So we map End to GLM's Start.
				// http://msdn.microsoft.com/en-us/library/ee422167(VS.85).aspx
				m_query->Start();	// drop "set fence" into stream
			break;
		}
	}
	return S_OK;
}

HRESULT IDirect3DQuery9::GetData(void* pData,DWORD dwSize,DWORD dwGetDataFlags)
{
	GL_BATCH_PERF_CALL_TIMER;
	Assert( m_device->m_nValidMarker == D3D_DEVICE_VALID_MARKER );
	HRESULT	result = S_FALSE ;
	DWORD nCurThreadId = ThreadGetCurrentId();

	// Make sure calling thread owns the GL context.
	Assert( m_ctx->m_nCurOwnerThreadId == nCurThreadId );
	if ( pData )
	{
		*(uint*)pData = 0;
	}

	if ( !m_query->IsStarted() || !m_query->IsStopped() ) 
	{
		Assert(!"Can't GetData before issue/start/stop");
		printf("\n** IDirect3DQuery9::GetData: can't GetData before issue/start/stop");
		return S_FALSE;
	}
					
	// GetData is not always called with the flush bit.
		
	// if an answer is not yet available - return S_FALSE.
	// if an answer is available - return S_OK and write the answer into *pData.
	bool done = false;
	bool flush = (dwGetDataFlags & D3DGETDATA_FLUSH) != 0;	// aka spin until done

	// hmmm both of these paths are the same, maybe we could fold them up
		if ( m_type == D3DQUERYTYPE_OCCLUSION )
		{
			// Detect cases that are actually just not supported with the way we're using GL queries. (For example, beginning a query, then creating/deleting any query, the ending the same query is not supported.)
			// Also extra paranoid to detect/work around various NV/AMD driver issues.
			if ( ( ( m_nIssueStartThreadID != nCurThreadId ) || ( m_nIssueEndThreadID != nCurThreadId ) ) ||
				 ( m_nIssueStartDrawCallIndex == m_nIssueEndDrawCallIndex ) || ( m_nIssueStartFrameIndex != m_nIssueEndFrameIndex ) ||
				 ( m_nIssueStartQueryCreationCounter != m_nIssueEndQueryCreationCounter ) )
			{
				// The thread Issue() was called on differs from GetData() - NV's driver doesn't like this, not sure about AMD. Just fake the results if a flush is requested.
				// There are various ways to properly handle this scenario, but in practice it only seems to occur in non-critical times (during shutdown or when mat_queue_mode is changed in L4D2).
				if ( flush )
				{
					gGL->glFlush();
				}

#if 0
				if ( ( m_nIssueStartThreadID != nCurThreadId ) || ( m_nIssueEndThreadID != nCurThreadId ) )
				{
					GLMDebugPrintf( "IDirect3DQuery9::GetData: GetData() called from different thread verses the issueing thread()!\n" );
				}
#endif
				if ( m_nIssueStartQueryCreationCounter != m_nIssueEndQueryCreationCounter )
				{
					GLMDebugPrintf( "IDirect3DQuery9::GetData: One or more queries have been created or released while this query was still issued! This scenario is not supported in GL.\n");
				}

				// Return with a non-standard error code, so the caller has a chance to do something halfway intelligent.
				return D3DERR_NOTAVAILABLE;
			}
		}
		
		switch( m_type )
		{
			case	D3DQUERYTYPE_OCCLUSION:
			{
				
			if ( flush )
				{
				uint oqValue = 0;
					CFastTimer tm;
					tm.Start();
										

						// Is this flush actually necessary? According to the extension it's not.
						// It doesn't seem to matter if this is a glFlush() or glFinish() with NVidia's driver (tested in MT mode - not sure if it matters), it still can take several calls to IsDone() before we can stop waiting for the query results.
						// On AMD, this flush logic fails during shutdown (the query results never become available) - tried a bunch of experiments and checks with no luck.
																						
				m_query->Complete(&oqValue);

											
				double flTotalTime = tm.GetDurationInProgress().GetSeconds() * 1000.0f;
				if ( flTotalTime > .5f )
						{
							// Give up - something silly has obviously gone wrong in the driver, lying is better than stalling potentially forever.
							// This occurs on AMD (single threaded driver) during shutdown, not sure why yet. It has nothing to do with threading. It may have to do with releasing queries or other objects.
							// We must return a result otherwise the app itself could hang, waiting infinitely.
							//Assert( 0 );
					Warning( "IDirect3DQuery9::GetData(): Occlusion query flush took %3.3fms!\n", flTotalTime );
						}
				if (pData)
				{
					*(uint*)pData = oqValue;
					}
				result = S_OK;
				}
			else
			{
				done = m_query->IsDone();
				if (done)
				{
					uint oqValue = 0;				// or we could just pass pData directly to Complete...
					m_query->Complete(&oqValue);
					if (pData)
					{
						*(uint*)pData = oqValue;
					}					
					result = S_OK;
				}
				else
				{
					result = S_FALSE;
					Assert( !flush );
				}
				}
			}
			break;

			case	D3DQUERYTYPE_EVENT:
			{
				done = m_query->IsDone();
				if ( ( done ) || ( flush ) )
				{
					m_query->Complete(NULL);	// this will block on pre-SLGU
					
					result = S_OK;
				}
				else
				{
					result = S_FALSE;
					Assert( !flush );
				}
			}
			break;
	}

	return result;
}

// ------------------------------------------------------------------------------------------------------------------------------ //

#ifdef OSX

#pragma mark ----- IDirect3DVertexBuffer9

#endif

HRESULT IDirect3DDevice9::CreateVertexBuffer(UINT Length,DWORD Usage,DWORD FVF,D3DPOOL Pool,IDirect3DVertexBuffer9** ppVertexBuffer,VD3DHANDLE* pSharedHandle)
{
	GL_BATCH_PERF_CALL_TIMER;
	GLMPRINTF(( ">-A- IDirect3DDevice9::CreateVertexBuffer" ));
	Assert( m_ctx->m_nCurOwnerThreadId == ThreadGetCurrentId() );
	
	m_ObjectStats.m_nTotalVertexBuffers++;

	IDirect3DVertexBuffer9 *newbuff = new IDirect3DVertexBuffer9;
	
	newbuff->m_device = this;
	
	newbuff->m_ctx = m_ctx;

		// FIXME need to find home or use for the Usage, FVF, Pool values passed in
	uint options = 0;
	
	if (Usage&D3DUSAGE_DYNAMIC)
	{
		options |= GLMBufferOptionDynamic;
	}
	
	newbuff->m_vtxBuffer = m_ctx->NewBuffer( kGLMVertexBuffer, Length, options  ) ;
	
	newbuff->m_vtxDesc.Type		= D3DRTYPE_VERTEXBUFFER;
	newbuff->m_vtxDesc.Usage	= Usage;
	newbuff->m_vtxDesc.Pool		= Pool;
	newbuff->m_vtxDesc.Size		= Length;

	*ppVertexBuffer = newbuff;
	
	GLMPRINTF(( "<-A- IDirect3DDevice9::CreateVertexBuffer" ));

	return S_OK;
}

IDirect3DVertexBuffer9::~IDirect3DVertexBuffer9()
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( m_device );
	GLMPRINTF(( ">-A- ~IDirect3DVertexBuffer9" ));
	
	if (m_device)
	{
		m_device->ReleasedVertexBuffer( this );

		if (m_ctx && m_vtxBuffer)
		{
			GLMPRINTF(( ">-A- ~IDirect3DVertexBuffer9 deleting m_vtxBuffer" ));
			m_ctx->DelBuffer( m_vtxBuffer );
			m_vtxBuffer = NULL;
			GLMPRINTF(( "<-A- ~IDirect3DVertexBuffer9 deleting m_vtxBuffer - done" ));
		}
		m_device = NULL;
	}
	
	GLMPRINTF(( "<-A- ~IDirect3DVertexBuffer9" ));
}

HRESULT IDirect3DVertexBuffer9::Lock(UINT OffsetToLock,UINT SizeToLock,void** ppbData,DWORD Flags)
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( m_device );
	tmZoneFiltered( TELEMETRY_LEVEL2, 25, TMZF_NONE, "VB Lock" );

	// FIXME would be good to have "can't lock twice" logic

	Assert( !(Flags & D3DLOCK_READONLY) );	// not impl'd
//	Assert( !(Flags & D3DLOCK_NOSYSLOCK) );	// not impl'd - it triggers though
	
	GLMBuffLockParams lockreq;
	lockreq.m_nOffset		= OffsetToLock;
	lockreq.m_nSize			= SizeToLock;
	lockreq.m_bNoOverwrite	= (Flags & D3DLOCK_NOOVERWRITE) != 0;
	lockreq.m_bDiscard		= (Flags & D3DLOCK_DISCARD) != 0;
			
	m_vtxBuffer->Lock( &lockreq, (char**)ppbData );

	GLMPRINTF(("-X- IDirect3DDevice9::Lock on D3D buf %p (GL name %d) offset %d, size %d => address %p", this, this->m_vtxBuffer->m_nHandle, OffsetToLock, SizeToLock, *ppbData));
	return S_OK;
}

HRESULT IDirect3DVertexBuffer9::Unlock()
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( m_device );
	
	tmZoneFiltered( TELEMETRY_LEVEL2, 25, TMZF_NONE, "VB Unlock" );

	m_vtxBuffer->Unlock();
	return S_OK;
}

void IDirect3DVertexBuffer9::UnlockActualSize( uint nActualSize, const void *pActualData )
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( m_device );
	tmZoneFiltered( TELEMETRY_LEVEL2, 25, TMZF_NONE, "VB UnlockActualSize" );

	m_vtxBuffer->Unlock( nActualSize, pActualData );
}

// ------------------------------------------------------------------------------------------------------------------------------ //


#ifdef OSX

#pragma mark ----- IDirect3DIndexBuffer9

#endif

HRESULT IDirect3DDevice9::CreateIndexBuffer(UINT Length,DWORD Usage,D3DFORMAT Format,D3DPOOL Pool,IDirect3DIndexBuffer9** ppIndexBuffer,VD3DHANDLE* pSharedHandle)
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( this );
	GLMPRINTF(( ">-A- IDirect3DDevice9::CreateIndexBuffer" ));

	// it is important to save all the create info, since GetDesc could get called later to query it
	
	m_ObjectStats.m_nTotalIndexBuffers++;

	IDirect3DIndexBuffer9 *newbuff = new IDirect3DIndexBuffer9;

	newbuff->m_device = this;

	newbuff->m_restype = D3DRTYPE_INDEXBUFFER;		//	hmmmmmmm why are we not derived from d3dresource..
		
	newbuff->m_ctx = m_ctx;
	
		// FIXME need to find home or use for the Usage, Format, Pool values passed in
	uint options = 0;
	
	if (Usage&D3DUSAGE_DYNAMIC)
	{
		options |= GLMBufferOptionDynamic;
	}

	newbuff->m_idxBuffer = m_ctx->NewBuffer( kGLMIndexBuffer, Length, options ) ;
	
	newbuff->m_idxDesc.Format	= Format;
	newbuff->m_idxDesc.Type		= D3DRTYPE_INDEXBUFFER;
	newbuff->m_idxDesc.Usage	= Usage;
	newbuff->m_idxDesc.Pool		= Pool;
	newbuff->m_idxDesc.Size		= Length;
		
	*ppIndexBuffer = newbuff;

	GLMPRINTF(( "<-A- IDirect3DDevice9::CreateIndexBuffer" ));

	return S_OK;
}

IDirect3DIndexBuffer9::~IDirect3DIndexBuffer9()
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( m_device );
	GLMPRINTF(( ">-A- ~IDirect3DIndexBuffer9" ));
	
	if (m_device)
	{
		m_device->ReleasedIndexBuffer( this );

		if (m_ctx && m_idxBuffer)
		{
			GLMPRINTF(( ">-A- ~IDirect3DIndexBuffer9 deleting m_idxBuffer" ));
			m_ctx->DelBuffer( m_idxBuffer );
			GLMPRINTF(( "<-A- ~IDirect3DIndexBuffer9 deleting m_idxBuffer - done" ));
		}
		m_device = NULL;
	}
	else
	{
	}
	
	GLMPRINTF(( "<-A- ~IDirect3DIndexBuffer9" ));
}


HRESULT IDirect3DIndexBuffer9::Lock(UINT OffsetToLock,UINT SizeToLock,void** ppbData,DWORD Flags)
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( m_device );
	// FIXME would be good to have "can't lock twice" logic

	tmZoneFiltered( TELEMETRY_LEVEL2, 25, TMZF_NONE, "IB Lock" );
	
	GLMBuffLockParams lockreq;
	lockreq.m_nOffset		= OffsetToLock;
	lockreq.m_nSize			= SizeToLock;
	lockreq.m_bNoOverwrite	= ( Flags & D3DLOCK_NOOVERWRITE ) != 0;
	lockreq.m_bDiscard		= ( Flags & D3DLOCK_DISCARD ) != 0;
	
	m_idxBuffer->Lock( &lockreq, (char**)ppbData );

	return S_OK;
}

HRESULT IDirect3DIndexBuffer9::Unlock()
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( m_device );

	tmZoneFiltered( TELEMETRY_LEVEL2, 25, TMZF_NONE, "IB Unlock" );

	m_idxBuffer->Unlock();

	return S_OK;
}

void IDirect3DIndexBuffer9::UnlockActualSize( uint nActualSize, const void *pActualData )
{ 
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( m_device );
	tmZoneFiltered( TELEMETRY_LEVEL2, 25, TMZF_NONE, "IB UnlockActualSize" );

	m_idxBuffer->Unlock( nActualSize, pActualData );
}

HRESULT IDirect3DIndexBuffer9::GetDesc(D3DINDEXBUFFER_DESC *pDesc)
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( m_device );
	*pDesc = m_idxDesc;
	return S_OK;
}


// ------------------------------------------------------------------------------------------------------------------------------ //

#ifdef OSX

#pragma mark ----- IDirect3DDevice9 -------------------------------------------------

#endif

void	ConvertPresentationParamsToGLMDisplayParams( D3DPRESENT_PARAMETERS *d3dp, GLMDisplayParams *gldp )
{
	memset( gldp, 0, sizeof(*gldp) );

	gldp->m_fsEnable					=	!d3dp->Windowed;

	// see http://msdn.microsoft.com/en-us/library/ee416515(VS.85).aspx
	// note that the values below are the only ones mentioned by Source engine; there are many others
	switch(d3dp->PresentationInterval)
	{
		case D3DPRESENT_INTERVAL_ONE:
			gldp->m_vsyncEnable					=	true;	// "The driver will wait for the vertical retrace period (the runtime will beam-follow to prevent tearing)."
		break;

		case D3DPRESENT_INTERVAL_IMMEDIATE:
			gldp->m_vsyncEnable					=	false;	// "The runtime updates the window client area immediately and might do so more than once during the adapter refresh period."
		break;
		
		default:
			gldp->m_vsyncEnable					=	true;	// if I don't know it, you're getting vsync enabled.
		break;
	}
	
	gldp->m_backBufferWidth				=	d3dp->BackBufferWidth;
	gldp->m_backBufferHeight			=	d3dp->BackBufferHeight;
	gldp->m_backBufferFormat			=	d3dp->BackBufferFormat;
	gldp->m_multiSampleCount			=	d3dp->MultiSampleType;	// it's a count really

	gldp->m_enableAutoDepthStencil		=	d3dp->EnableAutoDepthStencil != 0;
	gldp->m_autoDepthStencilFormat		=	d3dp->AutoDepthStencilFormat;

	gldp->m_fsRefreshHz					=	d3dp->FullScreen_RefreshRateInHz;
	
	// some fields in d3d PB we're not acting on yet...
	//	UINT                BackBufferCount;
	//	DWORD               MultiSampleQuality;
	//	D3DSWAPEFFECT       SwapEffect;
	//	VD3DHWND            hDeviceWindow;
	//	DWORD               Flags;
}

void	UnpackD3DRSITable( void );

HRESULT	IDirect3DDevice9::Create( IDirect3DDevice9Params *params )
{
	g_pD3D_Device = this;

	GLMDebugPrintf( "IDirect3DDevice9::Create: BackBufWidth: %u, BackBufHeight: %u, D3DFMT: %u, BackBufCount: %u, MultisampleType: %u, MultisampleQuality: %u\n",
		params->m_presentationParameters.BackBufferWidth,
		params->m_presentationParameters.BackBufferHeight,
		params->m_presentationParameters.BackBufferFormat,
		params->m_presentationParameters.BackBufferCount,
		params->m_presentationParameters.MultiSampleType,
		params->m_presentationParameters.MultiSampleQuality );
		
	UnpackD3DRSITable();
	
	m_ObjectStats.clear();
	m_PrevObjectStats.clear();
	
#if GL_BATCH_PERF_ANALYSIS && GL_BATCH_PERF_ANALYSIS_WRITE_PNGS
	m_pBatch_vis_bitmap = NULL;
#endif

	GL_BATCH_PERF_CALL_TIMER;
	GLMPRINTF((">-X-IDirect3DDevice9::Create"));
	HRESULT result = S_OK;
				
	// create an IDirect3DDevice9
	// make a GLMContext and set up some drawables
	m_params = *params;

	m_ctx = NULL;
		
	V_memset( m_pRenderTargets, 0, sizeof( m_pRenderTargets ) );
	m_pDepthStencil = NULL;
		
	m_pDefaultColorSurface = NULL;
	m_pDefaultDepthStencilSurface = NULL;
	
	memset( m_streams, 0, sizeof(m_streams) );
	memset( m_vtx_buffers, 0, sizeof( m_vtx_buffers ) );
	memset( m_textures, 0, sizeof(m_textures) );
	//memset( m_samplers, 0, sizeof(m_samplers) );

	m_indices.m_idxBuffer = NULL;

	m_vertexShader = NULL;
	m_pixelShader = NULL;

	m_pVertDecl = NULL;
			
	//============================================================================
	// param block for GLM context create
	GLMDisplayParams	glmParams;	
	ConvertPresentationParamsToGLMDisplayParams( &params->m_presentationParameters, &glmParams );

	glmParams.m_mtgl						=	true;	// forget this idea -> (params->m_behaviorFlags & D3DCREATE_MULTITHREADED) != 0;
	// the call above fills in a bunch of things, but doesn't know about anything outside of the presentation params.
	// those tend to be the things that do not change after create, so we do those here in Create.

	glmParams.m_focusWindow					=	params->m_focusWindow;	

		#if 0	//FIXME-HACK
			// map the D3D "adapter" to a renderer/display pair
			// (that GPU will have to stay set as-is for any subsequent mode changes)
		
			int glmRendererIndex = -1;
			int glmDisplayIndex = -1;
		
			GLMRendererInfoFields		glmRendererInfo;
			GLMDisplayInfoFields		glmDisplayInfo;
		
			// the D3D "Adapter" number feeds the fake adapter index
			bool adaptResult = GLMgr::aGLMgr()->GetDisplayDB()->GetFakeAdapterInfo( params->m_adapter, &glmRendererIndex, &glmDisplayIndex, &glmRendererInfo, &glmDisplayInfo );
			Assert(!adaptResult);

			glmParams.m_rendererIndex				=	glmRendererIndex;
			glmParams.m_displayIndex				=	glmDisplayIndex;
				// glmParams.m_modeIndex  hmmmmm, client doesn't give us a mode number, just a resolution..
		#endif
	
	m_ctx = GLMgr::aGLMgr()->NewContext( this, &glmParams );
	if (!m_ctx)
	{
		GLMPRINTF(("<-X- IDirect3DDevice9::Create (error out)"));
		return (HRESULT) -1;
	}
	
	// make an FBO to draw into and activate it.
	m_ctx->m_drawingFBO = m_ctx->NewFBO();					
				
	// bind it to context.  will receive attachments shortly.
	m_ctx->BindFBOToCtx( m_ctx->m_drawingFBO, GL_FRAMEBUFFER_EXT );
	
	m_bFBODirty = false;

	m_pFBOs = new CGLMFBOMap();
	m_pFBOs->SetLessFunc( RenderTargetState_t::LessFunc );

	// we create two IDirect3DSurface9's.  These will be known as the internal render target 0 and the depthstencil.
	
	GLMPRINTF(("-X- IDirect3DDevice9::Create making color render target..."));
	// color surface
	result = this->CreateRenderTarget( 
		m_params.m_presentationParameters.BackBufferWidth,			// width
		m_params.m_presentationParameters.BackBufferHeight,			// height
		m_params.m_presentationParameters.BackBufferFormat,			// format
		m_params.m_presentationParameters.MultiSampleType,			// MSAA depth
		m_params.m_presentationParameters.MultiSampleQuality,		// MSAA quality
		true,														// lockable
		&m_pDefaultColorSurface,										// ppSurface
		NULL,														// shared handle
		"InternalRT0"
		);

	if (result != S_OK)
	{
		GLMPRINTF(("<-X- IDirect3DDevice9::Create (error out)"));
		return result;
	}
		// do not do an AddRef..

	GLMPRINTF(("-X- IDirect3DDevice9::Create making color render target complete -> %08x", m_pDefaultColorSurface ));

	GLMPRINTF(("-X- IDirect3DDevice9::Create setting color render target..."));
	result = this->SetRenderTarget(0, m_pDefaultColorSurface);
	if (result != S_OK)
	{
		GLMPRINTF(("< IDirect3DDevice9::Create (error out)"));
		return result;
	}
	GLMPRINTF(("-X- IDirect3DDevice9::Create setting color render target complete."));

	Assert (m_params.m_presentationParameters.EnableAutoDepthStencil);

	GLMPRINTF(("-X- IDirect3DDevice9::Create making depth-stencil..."));
    result = CreateDepthStencilSurface(
		m_params.m_presentationParameters.BackBufferWidth,			// width
		m_params.m_presentationParameters.BackBufferHeight,			// height
		m_params.m_presentationParameters.AutoDepthStencilFormat,	// format
		m_params.m_presentationParameters.MultiSampleType,			// MSAA depth
		m_params.m_presentationParameters.MultiSampleQuality,		// MSAA quality
		TRUE,														// enable z-buffer discard ????
		&m_pDefaultDepthStencilSurface,								// ppSurface
		NULL														// shared handle
		);
	if (result != S_OK)
	{
		GLMPRINTF(("<-X- IDirect3DDevice9::Create (error out)"));
		return result;
	}
		// do not do an AddRef here..

	GLMPRINTF(("-X- IDirect3DDevice9::Create making depth-stencil complete -> %08x", m_pDefaultDepthStencilSurface));
	GLMPRINTF(("-X- Direct3DDevice9::Create setting depth-stencil render target..."));
	result = this->SetDepthStencilSurface(m_pDefaultDepthStencilSurface);
	if (result != S_OK)
	{
		DXABSTRACT_BREAK_ON_ERROR();
		GLMPRINTF(("<-X- IDirect3DDevice9::Create (error out)"));
		return result;
	}
	GLMPRINTF(("-X- IDirect3DDevice9::Create setting depth-stencil render target complete."));

	UpdateBoundFBO();

	bool ready = m_ctx->m_drawingFBO->IsReady();
	if (!ready)
	{
		GLMPRINTF(("<-X- IDirect3DDevice9::Create (error out)"));
		return (HRESULT)-1;
	}

	// this next part really needs to be inside GLMContext.. or replaced with D3D style viewport setup calls.
	m_ctx->GenDebugFontTex();
	
	// blast the gl state mirror...
	memset( &this->gl, 0, sizeof( this->gl ) );

	InitStates();

	GLScissorEnable_t		defScissorEnable		= { true };
	GLScissorBox_t			defScissorBox			= { 0,0, (GLsizei)m_params.m_presentationParameters.BackBufferWidth, (GLsizei)m_params.m_presentationParameters.BackBufferHeight };
	GLViewportBox_t			defViewportBox			= { 0,0, (GLsizei)m_params.m_presentationParameters.BackBufferWidth, (GLsizei)m_params.m_presentationParameters.BackBufferHeight, m_params.m_presentationParameters.BackBufferWidth | ( m_params.m_presentationParameters.BackBufferHeight << 16 ) };
	GLViewportDepthRange_t	defViewportDepthRange	= { 0.1, 1000.0 };
	GLCullFaceEnable_t		defCullFaceEnable		= { true };
	GLCullFrontFace_t		defCullFrontFace		= { GL_CCW };
	
	gl.m_ScissorEnable		=	defScissorEnable;
	gl.m_ScissorBox			=	defScissorBox;
	gl.m_ViewportBox		=	defViewportBox;
	gl.m_ViewportDepthRange	=	defViewportDepthRange;
	gl.m_CullFaceEnable		=	defCullFaceEnable;
	gl.m_CullFrontFace		=	defCullFrontFace;
		
	FullFlushStates();
	
	GLMPRINTF(("<-X- IDirect3DDevice9::Create complete"));

	// so GetClientRect can return sane answers
	//uint width, height;		
	RenderedSize( m_params.m_presentationParameters.BackBufferWidth, m_params.m_presentationParameters.BackBufferHeight, true );	// true = set
			
#if GL_TELEMETRY_GPU_ZONES
	g_TelemetryGPUStats.Clear();
#endif

	GL_BATCH_PERF( 
		g_nTotalD3DCalls = 0, g_nTotalD3DCycles = 0, m_nBatchVisY = 0, m_nBatchVisFrameIndex = 0, m_nBatchVisFileIdx = 0, m_nNumProgramChanges = 0, m_flTotalD3DTime = 0, m_nTotalD3DCalls = 0, 
		m_flTotalD3DTime = 0, m_nTotalGLCalls = 0, m_flTotalGLTime = 0, m_nOverallDraws = 0, m_nOverallPrims = 0, m_nOverallD3DCalls = 0, m_flOverallD3DTime = 0, m_nOverallGLCalls = 0, m_flOverallGLTime = 0, m_nOverallProgramChanges = 0, 
		m_flOverallPresentTime = 0, m_flOverallPresentTimeSquared = 0, m_nOverallPresents = 0, m_flOverallSwapWindowTime = 0, m_flOverallSwapWindowTimeSquared = 0, m_nTotalPrims = 0; );

	g_nTotalDrawsOrClears = 0;

	gGL->m_nTotalGLCycles = 0;
	gGL->m_nTotalGLCalls = 0;

	m_pDummy_vtx_buffer = new CGLMBuffer( m_ctx, kGLMVertexBuffer, 4096, 0 );
	m_vtx_buffers[0] = m_pDummy_vtx_buffer;
	m_vtx_buffers[1] = m_pDummy_vtx_buffer;
	m_vtx_buffers[2] = m_pDummy_vtx_buffer;
	m_vtx_buffers[3] = m_pDummy_vtx_buffer;
	
	return result;
}

IDirect3DDevice9::IDirect3DDevice9() :
	m_nValidMarker( D3D_DEVICE_VALID_MARKER )
{
}
IDirect3DDevice9::~IDirect3DDevice9()
{
	Assert( m_nValidMarker == D3D_DEVICE_VALID_MARKER );
#if GL_BATCH_PERF_ANALYSIS && GL_BATCH_PERF_ANALYSIS_WRITE_PNGS
	delete m_pBatch_vis_bitmap;
#endif
	
	delete m_pDummy_vtx_buffer;
	for ( int i = 0; i < 4; i++ )
		SetRenderTarget( i, NULL );
	SetDepthStencilSurface( NULL );
	if ( m_pDefaultColorSurface )
	{
		m_pDefaultColorSurface->Release( 0, "IDirect3DDevice9::~IDirect3DDevice9 release color surface" ); 
		m_pDefaultColorSurface = NULL;
	}
	if ( m_pDefaultDepthStencilSurface )
	{
		m_pDefaultDepthStencilSurface->Release( 0, "IDirect3DDevice9::~IDirect3DDevice9 release depth surface" ); 
		m_pDefaultDepthStencilSurface = NULL;
	}
	
	if ( m_pFBOs )
	{
	ResetFBOMap();
	}

	GLMPRINTF(( "-D- IDirect3DDevice9::~IDirect3DDevice9 signpost" ));	// want to know when this is called, if ever

	g_pD3D_Device = NULL;
	if ( m_ObjectStats.m_nTotalFBOs ) GLMDebugPrintf( "Leaking %i FBOs\n", m_ObjectStats.m_nTotalFBOs );
	if ( m_ObjectStats.m_nTotalVertexShaders ) ConMsg( "Leaking %i vertex shaders\n", m_ObjectStats.m_nTotalVertexShaders );
	if ( m_ObjectStats.m_nTotalPixelShaders ) ConMsg( "Leaking %i pixel shaders\n", m_ObjectStats.m_nTotalPixelShaders );
	if ( m_ObjectStats.m_nTotalVertexDecls ) ConMsg( "Leaking %i vertex decls\n", m_ObjectStats.m_nTotalVertexDecls );
	if ( m_ObjectStats.m_nTotalIndexBuffers ) ConMsg( "Leaking %i index buffers\n", m_ObjectStats.m_nTotalIndexBuffers );
	if ( m_ObjectStats.m_nTotalVertexBuffers ) ConMsg( "Leaking %i vertex buffers\n", m_ObjectStats.m_nTotalVertexBuffers );
	if ( m_ObjectStats.m_nTotalTextures ) ConMsg( "Leaking %i textures\n", m_ObjectStats.m_nTotalTextures );
	if ( m_ObjectStats.m_nTotalSurfaces ) ConMsg( "Leaking %i surfaces\n", m_ObjectStats.m_nTotalSurfaces );
	if ( m_ObjectStats.m_nTotalQueries ) ConMsg( "Leaking %i queries\n", m_ObjectStats.m_nTotalQueries );
	if ( m_ObjectStats.m_nTotalRenderTargets ) ConMsg( "Leaking %i render targets\n", m_ObjectStats.m_nTotalRenderTargets );
	GLMgr::aGLMgr()->DelContext( m_ctx );
	m_ctx = NULL;
	m_nValidMarker = 0xDEADBEEF;
}

#ifdef OSX

#pragma mark ----- Basics - (IDirect3DDevice9)

#endif


HRESULT IDirect3DDevice9::Reset(D3DPRESENT_PARAMETERS* pPresentationParameters)
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( this );
	Assert( m_nValidMarker == D3D_DEVICE_VALID_MARKER );
	HRESULT result = S_OK;

	// define the task of reset as:
	// provide new drawable RT's for the backbuffer (color and depthstencil).
	// fix up viewport / scissor..
	// then pass the new presentation parameters through to GLM.
	// (it will in turn notify appframework on the next present... which may be very soon, as mode changes are usually spotted inside Present() ).
	
	// so some of this looks a lot like Create - we're just a subset of what it does.
	// with a little work you could refactor this to be common code.

	GLMDebugPrintf( "IDirect3DDevice9::Reset: BackBufWidth: %u, BackBufHeight: %u, D3DFMT: %u, BackBufCount: %u, MultisampleType: %u, MultisampleQuality: %u\n",
		pPresentationParameters->BackBufferWidth,
		pPresentationParameters->BackBufferHeight,
		pPresentationParameters->BackBufferFormat,
		pPresentationParameters->BackBufferCount,
		pPresentationParameters->MultiSampleType,
		pPresentationParameters->MultiSampleQuality );

	//------------------------------------------------------------------------------- absorb new presentation params..
	
	m_params.m_presentationParameters = *pPresentationParameters;
	
	//------------------------------------------------------------------------------- color buffer..
	// release old color surface if it's there..
	if ( m_pDefaultColorSurface )
	{
		ULONG refc = m_pDefaultColorSurface->Release( 0, "IDirect3DDevice9::Reset public release color surface" ); (void)refc;
		Assert( !refc );
		m_pDefaultColorSurface = NULL;
	}
	
	GLMPRINTF(("-X- IDirect3DDevice9::Reset making new color render target..."));

	// color surface
	result = this->CreateRenderTarget( 
		m_params.m_presentationParameters.BackBufferWidth,			// width
		m_params.m_presentationParameters.BackBufferHeight,			// height
		m_params.m_presentationParameters.BackBufferFormat,			// format
		m_params.m_presentationParameters.MultiSampleType,			// MSAA depth
		m_params.m_presentationParameters.MultiSampleQuality,		// MSAA quality
		true,														// lockable
		&m_pDefaultColorSurface,										// ppSurface
		NULL														// shared handle
		);

	if ( result != S_OK )
	{
		GLMPRINTF(("<-X- IDirect3DDevice9::Reset (error out)"));
		return result;
	}
	// do not do an AddRef here..

	GLMPRINTF(("-X- IDirect3DDevice9::Reset making color render target complete -> %08x", m_pDefaultColorSurface ));

	GLMPRINTF(("-X- IDirect3DDevice9::Reset setting color render target..."));
	
	result = this->SetDepthStencilSurface( NULL );

	result = this->SetRenderTarget( 0, m_pDefaultColorSurface );
	if (result != S_OK)
	{
		GLMPRINTF(("< IDirect3DDevice9::Reset (error out)"));
		return result;
	}
	GLMPRINTF(("-X- IDirect3DDevice9::Reset setting color render target complete."));


	//-------------------------------------------------------------------------------depth stencil buffer
	// release old depthstencil surface if it's there..
	if ( m_pDefaultDepthStencilSurface )
	{
		ULONG refc = m_pDefaultDepthStencilSurface->Release( 0, "IDirect3DDevice9::Reset public release depthstencil surface" ); (void)refc;
		Assert(!refc);
		m_pDefaultDepthStencilSurface = NULL;
	}
	
	Assert (m_params.m_presentationParameters.EnableAutoDepthStencil);

	GLMPRINTF(("-X- IDirect3DDevice9::Reset making depth-stencil..."));
    result = CreateDepthStencilSurface(
		m_params.m_presentationParameters.BackBufferWidth,			// width
		m_params.m_presentationParameters.BackBufferHeight,			// height
		m_params.m_presentationParameters.AutoDepthStencilFormat,	// format
		m_params.m_presentationParameters.MultiSampleType,			// MSAA depth
		m_params.m_presentationParameters.MultiSampleQuality,		// MSAA quality
		TRUE,														// enable z-buffer discard ????
		&m_pDefaultDepthStencilSurface,								// ppSurface
		NULL														// shared handle
		);
	if (result != S_OK)
	{
		GLMPRINTF(("<-X- IDirect3DDevice9::Reset (error out)"));
		return result;
	}
		// do not do an AddRef here..

	GLMPRINTF(("-X- IDirect3DDevice9::Reset making depth-stencil complete -> %08x", m_pDefaultDepthStencilSurface));

	GLMPRINTF(("-X- IDirect3DDevice9::Reset setting depth-stencil render target..."));
	result = this->SetDepthStencilSurface(m_pDefaultDepthStencilSurface);
	if (result != S_OK)
	{
		GLMPRINTF(("<-X- IDirect3DDevice9::Reset (error out)"));
		return result;
	}
	GLMPRINTF(("-X- IDirect3DDevice9::Reset setting depth-stencil render target complete."));

	UpdateBoundFBO();

	bool ready = m_ctx->m_drawingFBO->IsReady();
	if (!ready)
	{
		GLMPRINTF(("<-X- IDirect3DDevice9::Reset (error out)"));
		return D3DERR_DEVICELOST;
	}

	//-------------------------------------------------------------------------------zap viewport and scissor to new backbuffer size

	InitStates();

	GLScissorEnable_t		defScissorEnable		= { true };
	GLScissorBox_t			defScissorBox			= { 0,0, (GLsizei)m_params.m_presentationParameters.BackBufferWidth, (GLsizei)m_params.m_presentationParameters.BackBufferHeight };
	GLViewportBox_t			defViewportBox			= { 0,0, (GLsizei)m_params.m_presentationParameters.BackBufferWidth, (GLsizei)m_params.m_presentationParameters.BackBufferHeight, m_params.m_presentationParameters.BackBufferWidth | ( m_params.m_presentationParameters.BackBufferHeight << 16 ) };
	GLViewportDepthRange_t	defViewportDepthRange	= { 0.1, 1000.0 };
	GLCullFaceEnable_t		defCullFaceEnable		= { true };
	GLCullFrontFace_t		defCullFrontFace		= { GL_CCW };
	
	gl.m_ScissorEnable		=	defScissorEnable;
	gl.m_ScissorBox			=	defScissorBox;
	gl.m_ViewportBox		=	defViewportBox;
	gl.m_ViewportDepthRange	=	defViewportDepthRange;
	gl.m_CullFaceEnable		=	defCullFaceEnable;
	gl.m_CullFrontFace		=	defCullFrontFace;
			
	FullFlushStates();
	
	//-------------------------------------------------------------------------------finally, propagate new display params to GLM context
	GLMDisplayParams	glmParams;	
	ConvertPresentationParamsToGLMDisplayParams( pPresentationParameters, &glmParams );

	// steal back previously sent focus window...
	glmParams.m_focusWindow = m_ctx->m_displayParams.m_focusWindow;
	Assert( glmParams.m_focusWindow != NULL );

	// so GetClientRect can return sane answers
	//uint width, height;		
	RenderedSize( pPresentationParameters->BackBufferWidth, pPresentationParameters->BackBufferHeight, true );	// true = set

	m_ctx->Reset();
		
	m_ctx->SetDisplayParams( &glmParams );
	
	return S_OK;
}

HRESULT IDirect3DDevice9::SetViewport(CONST D3DVIEWPORT9* pViewport)
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( this );
	GLMPRINTF(("-X- IDirect3DDevice9::SetViewport : minZ %f, maxZ %f",pViewport->MinZ, pViewport->MaxZ ));
	
	gl.m_ViewportBox.x		= pViewport->X;
	gl.m_ViewportBox.width	= pViewport->Width;

	gl.m_ViewportBox.y		= pViewport->Y;
	gl.m_ViewportBox.height	= pViewport->Height;	

	gl.m_ViewportBox.widthheight = pViewport->Width | ( pViewport->Height << 16 );

	m_ctx->WriteViewportBox( &gl.m_ViewportBox );

	gl.m_ViewportDepthRange.flNear	=	pViewport->MinZ;
	gl.m_ViewportDepthRange.flFar	=	pViewport->MaxZ;
	m_ctx->WriteViewportDepthRange( &gl.m_ViewportDepthRange );

	return S_OK;
}

HRESULT IDirect3DDevice9::GetViewport( D3DVIEWPORT9* pViewport )
{
	// 7LS - unfinished, used in scaleformuirenderimpl.cpp (only width and height required)
	GL_BATCH_PERF_CALL_TIMER;
	Assert( GetCurrentOwnerThreadId() == ThreadGetCurrentId() );
	GLMPRINTF(("-X- IDirect3DDevice9::GetViewport " ));

	pViewport->X = gl.m_ViewportBox.x;
	pViewport->Width = gl.m_ViewportBox.width;

	pViewport->Y = gl.m_ViewportBox.y;
	pViewport->Height = gl.m_ViewportBox.height;	

	pViewport->MinZ = gl.m_ViewportDepthRange.flNear;
	pViewport->MaxZ = gl.m_ViewportDepthRange.flFar;

	return S_OK;
}

HRESULT IDirect3DDevice9::BeginScene()
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( this );
	m_ctx->BeginFrame();

	return S_OK;
}

HRESULT IDirect3DDevice9::EndScene()
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( this );
	m_ctx->EndFrame();
	return S_OK;
}


// stolen from glmgrbasics.cpp

enum ECarbonModKeyIndex
{
  EcmdKeyBit                     = 8,    /* command key down?*/
  EshiftKeyBit                   = 9,    /* shift key down?*/
  EalphaLockBit                  = 10,   /* alpha lock down?*/
  EoptionKeyBit                  = 11,   /* option key down?*/
  EcontrolKeyBit                 = 12    /* control key down?*/
};

enum ECarbonModKeyMask
{
  EcmdKey                        = 1 << EcmdKeyBit,
  EshiftKey                      = 1 << EshiftKeyBit,
  EalphaLock                     = 1 << EalphaLockBit,
  EoptionKey                     = 1 << EoptionKeyBit,
  EcontrolKey                    = 1 << EcontrolKeyBit
};

void IDirect3DDevice9::PrintObjectStats( const ObjectStats_t &stats )
{
	ConMsg( "Total FBOs: %i\n", stats.m_nTotalFBOs );
	ConMsg( "Total vertex shaders: %i\n", stats.m_nTotalVertexShaders );
	ConMsg( "Total pixel shaders: %i\n", stats.m_nTotalPixelShaders );
	ConMsg( "Total vertex decls: %i\n", stats.m_nTotalVertexDecls );
	ConMsg( "Total index buffers: %i\n", stats.m_nTotalIndexBuffers );
	ConMsg( "Total vertex buffers: %i\n", stats.m_nTotalVertexBuffers );
	ConMsg( "Total textures: %i\n", stats.m_nTotalTextures );
	ConMsg( "Total surfaces: %i\n", stats.m_nTotalSurfaces );
	ConMsg( "Total queries: %i\n", stats.m_nTotalQueries );
	ConMsg( "Total render targets: %i\n", stats.m_nTotalRenderTargets );
}

void IDirect3DDevice9::DumpStatsToConsole( const CCommand *pArgs )
{
#if GL_BATCH_PERF_ANALYSIS
	ConMsg( "Overall: Batches: %u, Prims: %u, Program Changes: %u\n", m_nOverallDraws, m_nOverallPrims, m_nOverallProgramChanges  );
	ConMsg( "Overall: D3D Calls: %u D3D Time: %4.3fms, Avg D3D Time Per Call: %4.9fms\n", m_nOverallD3DCalls, m_flOverallD3DTime, m_nOverallD3DCalls ? ( m_flOverallD3DTime / m_nOverallD3DCalls ) : 0.0f );
	ConMsg( "Overall:  GL Calls: %u  GL Time: %4.3fms, Avg GL Time Per Call: %4.9fms\n", m_nOverallGLCalls, m_flOverallGLTime, m_nOverallGLCalls ? ( m_flOverallGLTime / m_nOverallGLCalls ) : 0.0f );
	
	ConMsg( "D3DPresent: %u, Overall Time: %4.3fms, Avg: %4.6fms, Std Dev: %4.6fms\n",
		m_nOverallPresents,
		m_flOverallPresentTime, m_nOverallPresents ? ( m_flOverallPresentTime / m_nOverallPresents ) : 0.0f,
		m_nOverallPresents ? ( sqrt( ( m_flOverallPresentTimeSquared / m_nOverallPresents ) - ( m_flOverallPresentTime / m_nOverallPresents ) * ( m_flOverallPresentTime / m_nOverallPresents ) ) ) : 0.0f );

	ConMsg( "GL SwapWindow(): Overall Time: %4.3fms, Avg: %4.6fms, Std Dev: %4.6fms\n",
		m_flOverallSwapWindowTime, m_nOverallPresents ? ( m_flOverallSwapWindowTime / m_nOverallPresents ) : 0.0f,
		m_nOverallPresents ? ( sqrt( ( m_flOverallSwapWindowTimeSquared / m_nOverallPresents ) - ( m_flOverallSwapWindowTime / m_nOverallPresents ) * ( m_flOverallSwapWindowTime / m_nOverallPresents ) ) ) : 0.0f );
		
	if ( ( pArgs ) && ( pArgs->ArgC() == 2 ) && (pArgs->Arg(1)[0] != '0') )
	{
		m_nOverallDraws = 0;
		m_nOverallPrims = 0;
		m_nOverallProgramChanges = 0;
		m_nOverallD3DCalls = 0;
		m_flOverallD3DTime = 0;
		m_nOverallGLCalls = 0;
		m_flOverallGLTime = 0;
		m_flOverallPresentTime = 0;
		m_flOverallPresentTimeSquared = 0;
		m_flOverallSwapWindowTime = 0;
		m_flOverallSwapWindowTimeSquared = 0;
		m_nOverallPresents = 0;
	}
#endif
	ConMsg( "Totals:\n" );
	m_ObjectStats.m_nTotalFBOs = m_pFBOs->Count();
	PrintObjectStats( m_ObjectStats );
	ObjectStats_t delta( m_ObjectStats );
	delta -= m_PrevObjectStats;
	ConMsg( "Delta:\n" );
	PrintObjectStats( delta );
	m_PrevObjectStats = m_ObjectStats;
}

static void gl_dump_stats_func( const CCommand &args )
{
	if ( g_pD3D_Device )
	{
		g_pD3D_Device->DumpStatsToConsole( &args );
	}
}

static ConCommand gl_dump_stats( "gl_dump_stats", gl_dump_stats_func );
#if GLMDEBUG
void IDirect3DDevice9::DumpTextures( const CCommand *pArgs )
{
	Assert( m_nValidMarker == D3D_DEVICE_VALID_MARKER );
	(void)pArgs;
	CGLMTex *pCurTex = g_pFirstCGMLTex;
	if ( pCurTex )
	{
		Assert( pCurTex->m_pPrevTex == NULL );
	}
	ConMsg( "--- Internal CGLMTex's:\n" );
	uint nNumFound = 0;
	while ( pCurTex )
	{
		nNumFound++;
		ConMsg( "Tex \"%s\", Layout: \"%s\", Size: %u, RT: %u, Depth: %u, Stencil: %u, MSAA: %u\n", 
			pCurTex->m_debugLabel ? pCurTex->m_debugLabel : "?", 
			pCurTex->m_layout->m_layoutSummary,
			pCurTex->m_layout->m_storageTotalSize,
			( pCurTex->m_layout->m_key.m_texFlags & kGLMTexRenderable ) ? 1 : 0,
			( pCurTex->m_layout->m_key.m_texFlags & kGLMTexIsDepth ) ? 1 : 0,
			( pCurTex->m_layout->m_key.m_texFlags & kGLMTexIsStencil ) ? 1 : 0,
			( pCurTex->m_layout->m_key.m_texFlags & kGLMTexMultisampled ) ? 1 : 0 );
		CGLMTex *pNextTex = pCurTex->m_pNextTex;
		if ( pNextTex )
		{
			Assert( pNextTex->m_pPrevTex == pCurTex );
		}
		pCurTex = pNextTex;
	}
	ConMsg( "--- Found %u total CGLMTex's\n", nNumFound );
}
static void gl_dump_textures_func( const CCommand &args )
{
	if ( g_pD3D_Device )
	{
		g_pD3D_Device->DumpTextures( &args );
	}
}
static ConCommand gl_dump_textures( "gl_dump_textures", gl_dump_textures_func );

#endif

ConVar gl_blitmode( "gl_blitmode", "1" );
ConVar dxa_nullrefresh_capslock( "dxa_nullrefresh_capslock", "0" );

HRESULT IDirect3DDevice9::Present(CONST RECT* pSourceRect,CONST RECT* pDestRect,VD3DHWND hDestWindowOverride,CONST RGNDATA* pDirtyRegion)
{
	GL_BATCH_PERF( g_nTotalD3DCalls++; )
	GL_PUBLIC_ENTRYPOINT_CHECKS( this );
			
	TOGL_NULL_DEVICE_CHECK;

	if ( m_bFBODirty )
	{
		UpdateBoundFBO();
	}
		
	// before attempting to present a tex, make sure it's been resolved if it was MSAA.
		// if we push that responsibility down to m_ctx->Present, it could probably do it without an extra copy.
		// i.e. anticipate the blit from the resolvedtex to GL_BACK, and just do that instead.

	// no explicit ResolveTex call first - that got pushed down into GLMContext::Present
		
#if GL_BATCH_PERF_ANALYSIS
	uint64 nStartGLCycles = gGL->m_nTotalGLCycles;
	nStartGLCycles;
	
	CFastTimer tm;
	tm.Start();
#endif

	m_ctx->Present( m_pDefaultColorSurface->m_tex );
		
#if GL_BATCH_PERF_ANALYSIS
	double flPresentTime = tm.GetDurationInProgress().GetMillisecondsF();
	double flGLSwapWindowTime = g_pLauncherMgr->GetPrevGLSwapWindowTime();

	m_flOverallPresentTime += flPresentTime;
	m_flOverallPresentTimeSquared += flPresentTime * flPresentTime;
	m_flOverallSwapWindowTime += flGLSwapWindowTime;
	m_flOverallSwapWindowTimeSquared += flGLSwapWindowTime * flGLSwapWindowTime;
	m_nOverallPresents++;

	uint64 nEndGLCycles = gGL->m_nTotalGLCycles;
	nEndGLCycles;

	m_flTotalD3DTime += flPresentTime + g_nTotalD3DCycles * s_rdtsc_to_ms;
	m_nTotalD3DCalls += g_nTotalD3DCalls;
		
	m_flTotalGLTime += gGL->m_nTotalGLCycles * s_rdtsc_to_ms;
	m_nTotalGLCalls += gGL->m_nTotalGLCalls;

	m_nOverallProgramChanges += m_nNumProgramChanges;
	m_nOverallDraws += g_nTotalDrawsOrClears;
	m_nOverallPrims += m_nTotalPrims;
	m_nOverallD3DCalls += m_nTotalD3DCalls;
	m_flOverallD3DTime += m_flTotalD3DTime;
	m_nOverallGLCalls += m_nTotalGLCalls;
	m_flOverallGLTime += m_flTotalGLTime;
					
	static int nPrevBatchVis = -1;

#if GL_BATCH_PERF_ANALYSIS_WRITE_PNGS
	if ((nPrevBatchVis == 1) && m_pBatch_vis_bitmap && m_pBatch_vis_bitmap->is_valid())
	{
		double flTotalGLPresentTime = ( nEndGLCycles - nStartGLCycles ) * s_rdtsc_to_ms;

		m_pBatch_vis_bitmap->fill_box(0, m_nBatchVisY, (uint)(.5f + flPresentTime / gl_present_vis_abs_scale.GetFloat() * m_pBatch_vis_bitmap->width()), 10, 255, 16, 128);
		m_pBatch_vis_bitmap->additive_fill_box(0, m_nBatchVisY, (uint)(.5f + flTotalGLPresentTime / gl_present_vis_abs_scale.GetFloat() * m_pBatch_vis_bitmap->width()), 10, 0, 255, 128);
		m_nBatchVisY += 10;

		uint y = MAX(m_nBatchVisY + 20, 600), l = 0;
		m_pBatch_vis_bitmap->draw_formatted_text(0, y+8*(l++), 1, 255, 255, 255, "OpenGL Frame: %u, Batches+Clears: %u, Prims: %u, Program Changes: %u", m_nOverallPresents, g_nTotalDrawsOrClears, m_nTotalPrims, m_nNumProgramChanges );
		m_pBatch_vis_bitmap->draw_formatted_text(0, y+8*(l++), 1, 255, 255, 255, "Frame: D3D Calls: %u, D3D Time: %3.3fms", m_nTotalD3DCalls, m_flTotalD3DTime);
		m_pBatch_vis_bitmap->draw_formatted_text(0, y+8*(l++), 1, 255, 255, 255, "Frame:  GL Calls: %u,  GL Time: %3.3fms", m_nTotalGLCalls, m_flTotalGLTime);
		l++;
		m_pBatch_vis_bitmap->draw_formatted_text(0, y+8*(l++), 1, 255, 255, 255, "Overall: Batches: %u, Prims: %u, Program Changes: %u", m_nOverallDraws, m_nOverallPrims, m_nOverallProgramChanges  );
		m_pBatch_vis_bitmap->draw_formatted_text(0, y+8*(l++), 1, 255, 255, 255, "Overall: D3D Calls: %u D3D Time: %4.3fms", m_nOverallD3DCalls, m_flOverallD3DTime );
		m_pBatch_vis_bitmap->draw_formatted_text(0, y+8*(l++), 1, 255, 255, 255, "Overall:  GL Calls: %u  GL Time: %4.3fms", m_nOverallGLCalls, m_flOverallGLTime );
		
		size_t png_size = 0;
		void *pPNG_data = tdefl_write_image_to_png_file_in_memory(m_pBatch_vis_bitmap->get_ptr(), m_pBatch_vis_bitmap->width(), m_pBatch_vis_bitmap->height(), 3, &png_size, true);
		if (pPNG_data)
		{
			char filename[256];
			V_snprintf(filename, sizeof(filename), "left4dead2/batchvis_%u_%u.png", m_nBatchVisFileIdx, m_nBatchVisFrameIndex);
			FILE* pFile = fopen(filename, "wb");
			if (pFile)
			{
				fwrite(pPNG_data, png_size, 1, pFile);
				fclose(pFile);
			}
			free(pPNG_data);
		}
		m_nBatchVisFrameIndex++;
		m_nBatchVisY = 0;
		m_pBatch_vis_bitmap->cls();
	}
#endif

	if (nPrevBatchVis != (int)gl_batch_vis.GetBool())
	{
		if ( !m_pBatch_vis_bitmap )
			m_pBatch_vis_bitmap = new simple_bitmap;

		nPrevBatchVis = gl_batch_vis.GetBool();
		if (!nPrevBatchVis)
		{
			DumpStatsToConsole( NULL );
			m_pBatch_vis_bitmap->clear();
		}
		else
		{
			m_pBatch_vis_bitmap->init(768, 1024);
		}
		m_nBatchVisY = 0;
		m_nBatchVisFrameIndex = 0;
		m_nBatchVisFileIdx = (uint)time(NULL); //rand();

		m_nOverallProgramChanges  = 0;
		m_nOverallDraws = 0;
		m_nOverallD3DCalls = 0;
		m_flOverallD3DTime = 0;
		m_nOverallGLCalls = 0;
		m_flOverallGLTime  = 0;
		m_flOverallPresentTime = 0;
		m_flOverallPresentTimeSquared = 0;
		m_flOverallSwapWindowTime = 0;
		m_flOverallSwapWindowTimeSquared = 0;
		m_nOverallPresents = 0;
	}
	
	g_nTotalD3DCycles = 0;
	g_nTotalD3DCalls = 0;
	gGL->m_nTotalGLCycles = 0;
	gGL->m_nTotalGLCalls = 0;
		
	m_nNumProgramChanges = 0;
	m_flTotalD3DTime = 0;
	m_nTotalD3DCalls = 0;
	m_flTotalGLTime = 0;
	m_nTotalGLCalls = 0;
	m_nTotalPrims = 0;
#else
	if ( gl_batch_vis.GetBool() )
	{
		gl_batch_vis.SetValue( false );
		
		ConMsg( "Must define GL_BATCH_PERF_ANALYSIS to use this feature" );
	}
#endif

	g_nTotalDrawsOrClears = 0;
				
#if GL_TELEMETRY_GPU_ZONES
	g_TelemetryGPUStats.Clear();
#endif

	return S_OK;
}

#ifdef OSX

#pragma mark ----- Textures - (IDirect3DDevice9)
#pragma mark ( create functions for each texture are now adjacent to the rest of the methods for each texture class)

#endif

HRESULT IDirect3DDevice9::GetTexture(DWORD Stage,IDirect3DBaseTexture9** ppTexture)
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( this );
	// if implemented, should it increase the ref count ??
	DXABSTRACT_BREAK_ON_ERROR();
	return S_OK;
}


#ifdef OSX

#pragma mark ----- RTs and Surfaces - (IDirect3DDevice9)

#endif

HRESULT IDirect3DDevice9::CreateRenderTarget(UINT Width,UINT Height,D3DFORMAT Format,D3DMULTISAMPLE_TYPE MultiSample,DWORD MultisampleQuality,BOOL Lockable,IDirect3DSurface9** ppSurface,VD3DHANDLE* pSharedHandle, char *pDebugLabel)
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( this );
	HRESULT result = S_OK;
	
	m_ObjectStats.m_nTotalSurfaces++;
	m_ObjectStats.m_nTotalRenderTargets++;

	IDirect3DSurface9 *surf = new IDirect3DSurface9;
	surf->m_restype = D3DRTYPE_SURFACE;

	surf->m_device		= this;				// always set device on creations!
	
	GLMTexLayoutKey rtkey;
	memset( &rtkey, 0, sizeof(rtkey) );
	
	rtkey.m_texGLTarget	=	GL_TEXTURE_2D;
	rtkey.m_xSize		=	Width;
	rtkey.m_ySize		=	Height;
	rtkey.m_zSize		=	1;

	rtkey.m_texFormat	=	Format;
	rtkey.m_texFlags	=	kGLMTexRenderable;

	rtkey.m_texFlags |= kGLMTexSRGB;	// all render target tex are SRGB mode
	if (m_ctx->Caps().m_cantAttachSRGB)
	{
		// this config can't support SRGB render targets.  quietly turn off the sRGB bit.
		rtkey.m_texFlags &= ~kGLMTexSRGB;
	}
	
	if ( (MultiSample !=0) && (!m_ctx->Caps().m_nvG7x) )
	{
		rtkey.m_texFlags |= kGLMTexMultisampled;
		rtkey.m_texSamples = MultiSample;
		// FIXME no support for "MS quality" yet
	}

	surf->m_tex			= m_ctx->NewTex( &rtkey, 1, pDebugLabel );
	surf->m_face		= 0;
	surf->m_mip			= 0;
	
	//desc
	surf->m_desc.Format				=	Format;
    surf->m_desc.Type				=	D3DRTYPE_SURFACE;
    surf->m_desc.Usage				=	0;					//FIXME ???????????
    surf->m_desc.Pool				=	D3DPOOL_DEFAULT;	//FIXME ???????????
	surf->m_desc.MultiSampleType	=	MultiSample;
    surf->m_desc.MultiSampleQuality	=	MultisampleQuality;
    surf->m_desc.Width				=	Width;
    surf->m_desc.Height				=	Height;

	*ppSurface = (result==S_OK) ? surf : NULL;

	#if IUNKNOWN_ALLOC_SPEW
		char scratch[1024];
		sprintf(scratch,"RT %s", surf->m_tex->m_layout->m_layoutSummary );
		surf->SetMark( true, scratch ); 
	#endif
	
	
	return result;
}

void IDirect3DDevice9::UpdateBoundFBO()
{
	RenderTargetState_t renderTargetState;
	for ( uint i = 0; i < 4; i++ )
	{
		renderTargetState.m_pRenderTargets[i] = m_pRenderTargets[i] ? m_pRenderTargets[i]->m_tex : NULL;
	}
	renderTargetState.m_pDepthStencil = m_pDepthStencil ? m_pDepthStencil->m_tex : NULL;
	CUtlMap < RenderTargetState_t, CGLMFBO * >::IndexType_t index = m_pFBOs->Find( renderTargetState );

	if ( m_pFBOs->IsValidIndex( index ) )
	{
		Assert( (*m_pFBOs)[index] );
		
		m_ctx->m_drawingFBO = (*m_pFBOs)[index];
	} 
	else 
	{
		CGLMFBO *newFBO = m_ctx->NewFBO();

		m_pFBOs->Insert( renderTargetState, newFBO );
		
		uint nNumBound = 0;

		for ( uint i = 0; i < 4; i++ )
		{
			if ( !m_pRenderTargets[i] )
				continue;

			GLMFBOTexAttachParams rtParams;
			memset( &rtParams, 0, sizeof(rtParams) );

			rtParams.m_tex		= m_pRenderTargets[i]->m_tex;
			rtParams.m_face		= m_pRenderTargets[i]->m_face;
			rtParams.m_mip		= m_pRenderTargets[i]->m_mip;
			rtParams.m_zslice	= 0;

			newFBO->TexAttach( &rtParams, (EGLMFBOAttachment)(kAttColor0 + i) );
			nNumBound++;
		}

		if ( m_pDepthStencil ) 
		{
			GLMFBOTexAttachParams	depthParams;
			memset( &depthParams, 0, sizeof(depthParams) );

			depthParams.m_tex = m_pDepthStencil->m_tex;

			EGLMFBOAttachment destAttach = (depthParams.m_tex->m_layout->m_format->m_glDataFormat != 34041) ? kAttDepth : kAttDepthStencil;

			newFBO->TexAttach( &depthParams, destAttach );
			nNumBound++;
		}
		
		(void)nNumBound;

		Assert( nNumBound );

#if GLMDEBUG
		Assert( newFBO->IsReady() );
#endif

		m_ctx->m_drawingFBO = newFBO;
	}

	m_ctx->BindFBOToCtx( m_ctx->m_drawingFBO, GL_FRAMEBUFFER_EXT );

	m_bFBODirty = false;
}

void IDirect3DDevice9::ResetFBOMap()
{
	if ( !m_pFBOs )
		return;

	FOR_EACH_MAP_FAST( (*m_pFBOs), i )
	{
		const RenderTargetState_t &rtState = m_pFBOs->Key( i ); (void)rtState;
		CGLMFBO *pFBO = (*m_pFBOs)[i];
				
		m_ctx->DelFBO( pFBO );
	}

	m_pFBOs->Purge();

	m_bFBODirty = true;
}

void IDirect3DDevice9::ScrubFBOMap( CGLMTex *pTex )
{
	Assert( pTex );

	if ( !m_pFBOs )
		return;
				
	CUtlVectorFixed< RenderTargetState_t, 128 > fbosToRemove;
	
	FOR_EACH_MAP_FAST( (*m_pFBOs), i )
	{
		const RenderTargetState_t &rtState = m_pFBOs->Key( i );
		CGLMFBO *pFBO = (*m_pFBOs)[i]; (void)pFBO;

		if ( rtState.RefersTo( pTex ) )
		{
			fbosToRemove.AddToTail( rtState );
		}
	}

	for ( int i = 0; i < fbosToRemove.Count(); ++i )
	{
		const RenderTargetState_t &rtState = fbosToRemove[i];

		CUtlMap < RenderTargetState_t, CGLMFBO * >::IndexType_t index = m_pFBOs->Find( rtState );

		if ( !m_pFBOs->IsValidIndex( index ) )
		{
			Assert( 0 );
			continue;
		}
		
		CGLMFBO *pFBO = (*m_pFBOs)[index];
						
		m_ctx->DelFBO( pFBO );

		m_pFBOs->RemoveAt( index );

		m_bFBODirty = true;
	}

	//GLMDebugPrintf( "IDirect3DDevice9::ScrubFBOMap: Removed %u entries\n", fbosToRemove.Count() );
}
HRESULT IDirect3DDevice9::SetRenderTarget(DWORD RenderTargetIndex,IDirect3DSurface9* pRenderTarget)
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( this );
	tmZone( TELEMETRY_LEVEL2, TMZF_NONE, "%s", __FUNCTION__ );
	
	Assert( RenderTargetIndex < 4 );

	HRESULT result = S_OK;

	GLMPRINTF(("-F- SetRenderTarget index=%d, surface=%8x (tex=%8x %s)",
		RenderTargetIndex,
		pRenderTarget,
		pRenderTarget ? pRenderTarget->m_tex : NULL,
		pRenderTarget ? pRenderTarget->m_tex->m_layout->m_layoutSummary : ""
	));

	// note that it is OK to pass NULL for pRenderTarget, it implies that you would like to detach any color buffer from that target index
	
	// behaviors...
	// if new surf is same as old surf, no change in refcount, in fact, it's early exit
	IDirect3DSurface9 *oldTarget = m_pRenderTargets[RenderTargetIndex];

	if (pRenderTarget == oldTarget)
	{
		GLMPRINTF(("-F-             --> no change",RenderTargetIndex));
		return S_OK;
	}
			

	// Fix this if porting to x86_64
	if ( m_pRenderTargets[RenderTargetIndex] )


	{











	// we now know that the new surf is not the same as the old surf.
	// you can't assume either one is non NULL here though.
	
		m_pRenderTargets[RenderTargetIndex]->Release( 1, "-A  SetRenderTarget private release" );
	}

	if (pRenderTarget)
	{
		pRenderTarget->AddRef( 1, "+A  SetRenderTarget private addref"  );						// again, private refcount being raised
	}
	m_pRenderTargets[RenderTargetIndex] = pRenderTarget;	
	
	m_bFBODirty = true;

/*
	if (!pRenderTarget)
	{		
		GLMPRINTF(("-F-             --> Setting NULL render target on index=%d ",RenderTargetIndex));
	}
	else
	{
		GLMPRINTF(("-F-             --> attaching index=%d on drawing FBO (%8x)",RenderTargetIndex, m_drawableFBO));
		// attach color to FBO
		GLMFBOTexAttachParams	rtParams;
		memset( &rtParams, 0, sizeof(rtParams) );
		
		rtParams.m_tex		= pRenderTarget->m_tex;
		rtParams.m_face		= pRenderTarget->m_face;
		rtParams.m_mip		= pRenderTarget->m_mip;
		rtParams.m_zslice	= 0;	// FIXME if you ever want to be able to render to slices of a 3D tex..
		
		m_drawableFBO->TexAttach( &rtParams, (EGLMFBOAttachment)(kAttColor0 + RenderTargetIndex) );
	}
*/

#if GL_BATCH_PERF_ANALYSIS && GL_BATCH_PERF_ANALYSIS_WRITE_PNGS
	if ( m_pBatch_vis_bitmap && m_pBatch_vis_bitmap->is_valid() && !RenderTargetIndex )
	{
		m_pBatch_vis_bitmap->fill_box(0, m_nBatchVisY, m_pBatch_vis_bitmap->width(), 1, 30, 20, 20);
		m_nBatchVisY += 1;
	}
#endif

	return result;
}


HRESULT IDirect3DDevice9::GetRenderTarget(DWORD RenderTargetIndex,IDirect3DSurface9** ppRenderTarget)
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( this );
	if ( !m_pRenderTargets[ RenderTargetIndex ] )
		return D3DERR_NOTFOUND;
	
	if ( ( RenderTargetIndex > 4 ) || !ppRenderTarget )
		return D3DERR_INVALIDCALL;

	// safe because of early exit on NULL above
	m_pRenderTargets[ RenderTargetIndex ]->AddRef(0, "+B GetRenderTarget public addref");	// per http://msdn.microsoft.com/en-us/library/bb174404(VS.85).aspx
	
	*ppRenderTarget = m_pRenderTargets[ RenderTargetIndex ];
	
	return S_OK;
}

HRESULT IDirect3DDevice9::CreateOffscreenPlainSurface( UINT Width,UINT Height,D3DFORMAT Format,D3DPOOL Pool,IDirect3DSurface9** ppSurface,VD3DHANDLE* pSharedHandle )
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( this );
	// set surf->m_restype to D3DRTYPE_SURFACE...

	// this is almost identical to CreateRenderTarget..
	
	HRESULT result = S_OK;
	
	m_ObjectStats.m_nTotalSurfaces++;
	m_ObjectStats.m_nTotalRenderTargets++;

	IDirect3DSurface9 *surf = new IDirect3DSurface9;
	surf->m_restype = D3DRTYPE_SURFACE;

	surf->m_device		= this;				// always set device on creations!

	GLMTexLayoutKey rtkey;
	memset( &rtkey, 0, sizeof(rtkey) );
	
	rtkey.m_texGLTarget	=	GL_TEXTURE_2D;
	rtkey.m_xSize		=	Width;
	rtkey.m_ySize		=	Height;
	rtkey.m_zSize		=	1;

	rtkey.m_texFormat	=	Format;
	rtkey.m_texFlags	=	kGLMTexRenderable;

	surf->m_tex			=	m_ctx->NewTex( &rtkey, 1, "offscreen plain surface" );
	surf->m_face		=	0;
	surf->m_mip			=	0;
	
	//desc
	surf->m_desc.Format				=	Format;
    surf->m_desc.Type				=	D3DRTYPE_SURFACE;
    surf->m_desc.Usage				=	0;
    surf->m_desc.Pool				=	D3DPOOL_DEFAULT;
	surf->m_desc.MultiSampleType	=	D3DMULTISAMPLE_NONE;
    surf->m_desc.MultiSampleQuality	=	0;
    surf->m_desc.Width				=	Width;
    surf->m_desc.Height				=	Height;

	*ppSurface = (result==S_OK) ? surf : NULL;
	
	return result;
}

HRESULT IDirect3DDevice9::CreateDepthStencilSurface(UINT Width,UINT Height,D3DFORMAT Format,D3DMULTISAMPLE_TYPE MultiSample,DWORD MultisampleQuality,BOOL Discard,IDirect3DSurface9** ppSurface,VD3DHANDLE* pSharedHandle)
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( this );
	Assert( ( Format == D3DFMT_D16 ) || ( Format == D3DFMT_D24X8 ) || ( Format == D3DFMT_D24S8 ) );
	HRESULT result = S_OK;
	
	m_ObjectStats.m_nTotalSurfaces++;
	m_ObjectStats.m_nTotalRenderTargets++;

	IDirect3DSurface9 *surf = new IDirect3DSurface9;
	surf->m_restype = D3DRTYPE_SURFACE;

	surf->m_device = this;				// always set device on creations!
	
	GLMTexLayoutKey depthkey;
	memset( &depthkey, 0, sizeof(depthkey) );

	depthkey.m_texGLTarget	=	GL_TEXTURE_2D;
	depthkey.m_xSize		=	Width;
	depthkey.m_ySize		=	Height;
	depthkey.m_zSize		=	1;

	depthkey.m_texFormat	=	Format;
	depthkey.m_texFlags		=	kGLMTexRenderable | kGLMTexIsDepth;
		
	if ( Format == D3DFMT_D24S8 )
	{
		depthkey.m_texFlags |= kGLMTexIsStencil;
	}

	if ( (MultiSample !=0) && (!m_ctx->Caps().m_nvG7x) )
	{
		depthkey.m_texFlags |= kGLMTexMultisampled;
		depthkey.m_texSamples = MultiSample;
		// FIXME no support for "MS quality" yet
	}

	surf->m_tex				= m_ctx->NewTex( &depthkey, 1, "depth-stencil surface" );
	surf->m_face			= 0;
	surf->m_mip				= 0;

	//desc

	surf->m_desc.Format				=	Format;
    surf->m_desc.Type				=	D3DRTYPE_SURFACE;
    surf->m_desc.Usage				=	0;					//FIXME ???????????
    surf->m_desc.Pool				=	D3DPOOL_DEFAULT;	//FIXME ???????????
	surf->m_desc.MultiSampleType	=	MultiSample;
    surf->m_desc.MultiSampleQuality	=	MultisampleQuality;
    surf->m_desc.Width				=	Width;
    surf->m_desc.Height				=	Height;

	*ppSurface = (result==S_OK) ? surf : NULL;
	
	return result;
}

HRESULT IDirect3DDevice9::SetDepthStencilSurface( IDirect3DSurface9* pNewZStencil )
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( this );
	HRESULT	result = S_OK;

	GLMPRINTF(("-F- SetDepthStencilSurface, surface=%8x (tex=%8x %s)",
		pNewZStencil,
		pNewZStencil ? pNewZStencil->m_tex : NULL,
		pNewZStencil ? pNewZStencil->m_tex->m_layout->m_layoutSummary : ""
	));

	if ( pNewZStencil == m_pDepthStencil )
	{
		GLMPRINTF(("-F-             --> no change"));
		return S_OK;
	}

	if ( pNewZStencil )
	{
		pNewZStencil->AddRef(1, "+A  SetDepthStencilSurface private addref");
	}

	if ( m_pDepthStencil )
	{
		// Note this Release() could cause the surface to be deleted!
		m_pDepthStencil->Release(1, "-A  SetDepthStencilSurface private release");
	}
	
	m_pDepthStencil = pNewZStencil;

	m_bFBODirty = true;
		
	return result;
}

HRESULT IDirect3DDevice9::GetDepthStencilSurface(IDirect3DSurface9** ppZStencilSurface)
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( this );
	if ( !ppZStencilSurface )
	{
		return D3DERR_INVALIDCALL;		
	}
	
	if ( !m_pDepthStencil )
	{
		*ppZStencilSurface = NULL;
		return D3DERR_NOTFOUND;
	}

	m_pDepthStencil->AddRef(0, "+B  GetDepthStencilSurface public addref");			// per http://msdn.microsoft.com/en-us/library/bb174384(VS.85).aspx

	*ppZStencilSurface = m_pDepthStencil;

	return S_OK;
}

HRESULT IDirect3DDevice9::GetRenderTargetData(IDirect3DSurface9* pRenderTarget,IDirect3DSurface9* pDestSurface)
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( this );
	// is it just a blit ?

	this->StretchRect( pRenderTarget, NULL, pDestSurface, NULL, D3DTEXF_NONE ); // is this good enough ???

	return S_OK;
}

HRESULT IDirect3DDevice9::GetFrontBufferData(UINT iSwapChain,IDirect3DSurface9* pDestSurface)
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( this );
	DXABSTRACT_BREAK_ON_ERROR();
	return S_OK;
}

HRESULT IDirect3DDevice9::StretchRect(IDirect3DSurface9* pSourceSurface,CONST RECT* pSourceRect,IDirect3DSurface9* pDestSurface,CONST RECT* pDestRect,D3DTEXTUREFILTERTYPE Filter)
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( this );
	// find relevant slices in GLM tex

	if ( m_bFBODirty )
	{
		UpdateBoundFBO();
	}
		
	CGLMTex	*srcTex = pSourceSurface->m_tex;
	int srcSliceIndex = srcTex->CalcSliceIndex( pSourceSurface->m_face, pSourceSurface->m_mip );
	GLMTexLayoutSlice *srcSlice = &srcTex->m_layout->m_slices[ srcSliceIndex ];

	CGLMTex	*dstTex = pDestSurface->m_tex;
	int dstSliceIndex = dstTex->CalcSliceIndex( pDestSurface->m_face, pDestSurface->m_mip );
	GLMTexLayoutSlice *dstSlice = &dstTex->m_layout->m_slices[ dstSliceIndex ];

	if ( dstTex->m_rboName != 0 )
	{
		Assert(!"No path yet for blitting into an MSAA tex");
		return S_OK;
	}

	bool useFastBlit = (gl_blitmode.GetInt() != 0);
	
	if ( !useFastBlit && (srcTex->m_rboName !=0))		// old way, we do a resolve to scratch tex first (necessitating two step blit)
	{
		m_ctx->ResolveTex( srcTex, true );
	}

	// set up source/dest rect in GLM form
	GLMRect	srcRect, dstRect;

	// d3d nomenclature:
	// Y=0 is the visual top and also aligned with V=0.

	srcRect.xmin	=	pSourceRect		?	pSourceRect->left	:	0;
	srcRect.xmax	=	pSourceRect		?	pSourceRect->right	:	srcSlice->m_xSize;
	srcRect.ymin	=	pSourceRect		?	pSourceRect->top	:	0;
	srcRect.ymax	=	pSourceRect		?	pSourceRect->bottom	:	srcSlice->m_ySize;

	dstRect.xmin	=	pDestRect		?	pDestRect->left		:	0;
	dstRect.xmax	=	pDestRect		?	pDestRect->right	:	dstSlice->m_xSize;
	dstRect.ymin	=	pDestRect		?	pDestRect->top		:	0;
	dstRect.ymax	=	pDestRect		?	pDestRect->bottom	:	dstSlice->m_ySize;

	GLenum filterGL = 0;
	switch(Filter)
	{
		case	D3DTEXF_NONE:
		case	D3DTEXF_POINT:
			filterGL = GL_NEAREST;
		break;
		
		case	D3DTEXF_LINEAR:
			filterGL = GL_LINEAR;
		break;
		
		default:			// D3DTEXF_ANISOTROPIC
			Assert(!"Impl aniso stretch");
		break;
	}
	
	if (useFastBlit)
	{
		m_ctx->Blit2(		srcTex, &srcRect, pSourceSurface->m_face, pSourceSurface->m_mip, 
							dstTex, &dstRect, pDestSurface->m_face, pDestSurface->m_mip, 
							filterGL
					);
	}
	else
	{
		m_ctx->BlitTex(		srcTex, &srcRect, pSourceSurface->m_face, pSourceSurface->m_mip, 
							dstTex, &dstRect, pDestSurface->m_face, pDestSurface->m_mip, 
							filterGL
					);
	}
						
	return S_OK;
}


// This totally sucks, but this information can't be gleaned any
// other way when translating from D3D to GL at this level
//
// This returns a mask, since multiple GLSL "varyings" can be tagged with centroid
static uint32 CentroidMaskFromName( bool bPixelShader, const char *pName )
{
	// Important note: This code has been customized for TF2 - don't blindly merge it into other branches!
	if ( !pName )
		return 0;
	
	// Important: The centroid bitflags must match between all linked vertex/pixel shaders!
	if ( bPixelShader )
	{
		if ( V_stristr( pName, "lightmappedgeneric_ps" ) || V_strstr( pName, "worldtwotextureblend_ps" ) )
		{
			return (0x01 << 2) | (0x01 << 3); // iterators 2 and 3
		}
		else if ( V_stristr( pName, "lightmappedreflective_ps" ) )
		{
			return (0x01 << 6) | (0x01 << 7); // iterators 6 and 7
		}
		else if ( V_stristr( pName, "water_ps" ) )
		{
			return 0xC0;
		}
		else if ( V_stristr( pName, "shadow_ps" ) )
		{
			return 0x1F;
		}
		else if ( V_stristr( pName, "ShatteredGlass_ps" ) )
		{
			return 0xC;
		}
		else if ( V_stristr( pName, "WorldVertexAlpha_ps" ) || V_stristr( pName, "WorldVertexTransition_ps" ) )
		{
			// These pixel shaders want centroid but shouldn't be used
			Assert(0);
			return 0;
		}
		else if ( V_stristr( pName, "flashlight_ps" ) )
		{
			return 0xC;
		}
	}
	else // vertex shader
	{
		// Vertex shaders also
		if ( V_stristr( pName, "lightmappedgeneric_vs" ) )
		{
			return (0x01 << 2) | (0x01 << 3); // iterators 2 and 3
		}
		else if ( V_stristr( pName, "lightmappedreflective_vs" ) )
		{
			return (0x01 << 6) | (0x01 << 7); // iterators 6 and 7
		}
		else if ( V_stristr( pName, "water_vs" ) )
		{
			return 0xC0;
		}
		else if ( V_stristr( pName, "shadow_vs" ) )
		{
			return 0x1F;
		}
		else if ( V_stristr( pName, "ShatteredGlass_vs" ) )
		{
			return 0xC;
		}
		else if ( V_stristr( pName, "flashlight_vs" ) )
		{
			return 0xC;
		}
	}
	
	// This shader doesn't have any centroid iterators
	return 0;
}


// This totally sucks, but this information can't be gleaned any
// other way when translating from D3D to GL at this level
static int ShadowDepthSamplerMaskFromName( const char *pName )
{
	if ( !pName )
		return 0;	
	
	if ( V_stristr( pName, "water_ps" ) )
	{
		return (1<<7);
	}
	else if ( V_stristr( pName, "infected_ps" ) )
	{
		return (1<<1);
	}
	else if ( V_stristr( pName, "phong_ps" ) )
	{
		return (1<<4) | (1<<15);
	}
	else if ( V_stristr( pName, "vertexlit_and_unlit_generic_bump_ps" ) )
	{
		return (1<<8) | (1<<15);
	}
	else if ( V_stristr( pName, "vertexlit_and_unlit_generic_ps" ) )
	{
		return (1<<8) | (1<<15);
	}
	else if ( V_stristr( pName, "eye_refract_ps" ) )
	{
		return (1<<6);
	}
	else if ( V_stristr( pName, "eyes_flashlight_ps" ) )
	{
		return (1<<4);
	}
	else if ( V_stristr( pName, "worldtwotextureblend_ps" ) ) 
	{
		return (1<<7);
	}
	else if ( V_stristr( pName, "teeth_flashlight_ps" ) ) 
	{
		return (1<<2);
	}
	else if ( V_stristr( pName, "flashlight_ps" ) ) // substring of above, make sure this comes last!!
	{
		return (1<<7);
	}
	else if ( V_stristr( pName, "lightmappedgeneric_ps" ) )
	{
		return (1<<15);
	}
	else if ( V_stristr( pName, "deferred_global_light_ps" ) )
	{
		return (1<<14);
	}	
	else if ( V_stristr( pName, "global_lit_simple_ps" ) )
	{
		return (1<<14);
	}	
	else if ( V_stristr( pName, "lightshafts_ps" ) )
	{
		return (1<<1);
	}	
	else if ( V_stristr( pName, "multiblend_combined_ps" ) )
	{
		return (1<<14);
	}	
	else if ( V_stristr( pName, "multiblend_ps" ) )
	{
		return (1<<14);
	}	
	else if ( V_stristr( pName, "customhero_ps" ) )
	{
		return (1<<14);
	}	

	// This shader doesn't have a shadow depth map sampler
	return 0;
}

#ifdef OSX

#pragma mark ----- Pixel Shaders - (IDirect3DDevice9)

#endif

HRESULT IDirect3DDevice9::CreatePixelShader(CONST DWORD* pFunction,IDirect3DPixelShader9** ppShader, const char *pShaderName, char *pDebugLabel, const uint32 *pCentroidMask )
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( this );
	HRESULT	result = D3DERR_INVALIDCALL;
	*ppShader = NULL;
	
	int nShadowDepthSamplerMask = ShadowDepthSamplerMaskFromName( pShaderName );
	uint nCentroidMask = CentroidMaskFromName( true, pShaderName );

	if ( pCentroidMask )
	{
		if ( *pCentroidMask != nCentroidMask )
		{
			GLMDebugPrintf( "IDirect3DDevice9::CreatePixelShader: shaderapi's centroid mask (0x%08X) differs from mask derived from shader name (0x%08X) for shader %s\n", *pCentroidMask, nCentroidMask, pDebugLabel );
		}
		// It would be great if we could use these centroid masks passed in from shaderapi - but unfortunately they're only available for pixel shaders, and we also need to compute matching masks for vertex shaders!
		//nCentroidMask = *pCentroidMask;
	}

	{
		int numTranslations = 1;
		
		bool bVertexShader = false;

		// we can do one or two translated forms. they go together in a single buffer with some markers to allow GLM to break it up.
		// this also lets us mirror each set of translations to disk with a single file making it easier to view and edit side by side.
		
		int maxTranslationSize = 50000;	// size of any one translation
		
		CUtlBuffer transbuf( 3000, numTranslations * maxTranslationSize, CUtlBuffer::TEXT_BUFFER );
		CUtlBuffer tempbuf( 3000, maxTranslationSize, CUtlBuffer::TEXT_BUFFER );

		transbuf.PutString( "//GLSLfp\n" );		// this is required so GLM can crack the text apart

		// note the GLSL translator wants its own buffer
		tempbuf.EnsureCapacity( maxTranslationSize );
			
		uint glslPixelShaderOptions = D3DToGL_OptionUseEnvParams;// | D3DToGL_OptionAllowStaticControlFlow;
			

		// Fake SRGB mode - needed on R500, probably indefinitely.
		// Do this stuff if caps show m_needsFakeSRGB=true and the sRGBWrite state is true
		// (but not if it's engine_post which is special)

		if (!m_ctx->Caps().m_hasGammaWrites)
		{
			if ( pShaderName )
			{
				if ( !V_stristr( pShaderName, "engine_post" ) )
				{
					glslPixelShaderOptions |= D3DToGL_OptionSRGBWriteSuffix;
				}
			}
		}

		g_D3DToOpenGLTranslatorGLSL.TranslateShader( (uint32 *) pFunction, &tempbuf, &bVertexShader, glslPixelShaderOptions, nShadowDepthSamplerMask, nCentroidMask, pDebugLabel );
			
		transbuf.PutString( (char*)tempbuf.Base() );
		transbuf.PutString( "\n\n" );	// whitespace
				
		if ( bVertexShader )
		{
			// don't cross the streams
			Assert(!"Can't accept vertex shader in CreatePixelShader");
			result = D3DERR_INVALIDCALL;
		}
		else
		{
			m_ObjectStats.m_nTotalPixelShaders++;

			IDirect3DPixelShader9 *newprog = new IDirect3DPixelShader9;

			newprog->m_pixHighWater = 0;
			newprog->m_pixSamplerMask = 0;
			newprog->m_pixSamplerTypes = 0;
					
			newprog->m_pixProgram = m_ctx->NewProgram( kGLMFragmentProgram, (char *)transbuf.Base(), pShaderName ? pShaderName : "?" ) ;
			newprog->m_pixProgram->m_nCentroidMask = nCentroidMask;
			newprog->m_pixProgram->m_nShadowDepthSamplerMask = nShadowDepthSamplerMask;
			
			newprog->m_pixProgram->m_bTranslatedProgram = true;
			newprog->m_pixProgram->m_maxVertexAttrs = 0;

			newprog->m_device = this;
			
			//------ find the frag program metadata and extract it..
						
			
			{
				// find the highwater mark
				char *highWaterPrefix = "//HIGHWATER-";		// try to arrange this so it can work with pure GLSL if needed
				char *highWaterStr = strstr( (char *)transbuf.Base(), highWaterPrefix );
				if (highWaterStr)
				{
					char *highWaterActualData = highWaterStr + strlen( highWaterPrefix );
				
					int value = -1;
					sscanf( highWaterActualData, "%d", &value );

					newprog->m_pixHighWater = value;
					newprog->m_pixProgram->m_descs[kGLMGLSL].m_highWater = value;
				}
				else
				{
					Assert(!"couldn't find sampler map in pixel shader");
				}
			}

			{
				// find the sampler map
				char *samplerMaskPrefix = "//SAMPLERMASK-";		// try to arrange this so it can work with pure GLSL if needed
			
				char *samplerMaskStr = strstr( (char *)transbuf.Base(), samplerMaskPrefix );
				if (samplerMaskStr)
				{
					char *samplerMaskActualData = samplerMaskStr + strlen( samplerMaskPrefix );
				
					int value = -1;
					sscanf( samplerMaskActualData, "%04x", &value );

					newprog->m_pixSamplerMask = value;
					newprog->m_pixProgram->m_samplerMask = value;	// helps GLM maintain a better linked pair cache even when SRGB sampler state changes
										
					int nMaxReg;
					for ( nMaxReg = 31; nMaxReg >= 0; --nMaxReg )
						if ( value & ( 1 << nMaxReg ) )
							break;

					newprog->m_pixProgram->m_maxSamplers = nMaxReg + 1;

					int nNumUsedSamplers = 0;
					for ( int i = 31; i >= 0; --i)
						if ( value & ( 1 << i ) )
							nNumUsedSamplers++;
					newprog->m_pixProgram->m_nNumUsedSamplers = nNumUsedSamplers;
				}
				else
				{
					Assert(!"couldn't find sampler map in pixel shader");
				}
			}

			{
				// find the sampler map
				char *samplerTypesPrefix = "//SAMPLERTYPES-";		// try to arrange this so it can work with pure GLSL if needed

				char *samplerTypesStr = strstr( (char *)transbuf.Base(), samplerTypesPrefix );
				if (samplerTypesStr)
				{
					char *samplerTypesActualData = samplerTypesStr + strlen( samplerTypesPrefix );

					int value = -1;
					sscanf( samplerTypesActualData, "%08x", &value );

					newprog->m_pixSamplerTypes = value;
					newprog->m_pixProgram->m_samplerTypes = value;	// helps GLM maintain a better linked pair cache even when SRGB sampler state changes
				}
				else
				{
					Assert(!"couldn't find sampler types in pixel shader");
				}
			}

			{
				// find the fb outputs used by this shader/combo
				const GLenum buffers[] = { GL_COLOR_ATTACHMENT0_EXT, GL_COLOR_ATTACHMENT1_EXT, GL_COLOR_ATTACHMENT2_EXT, GL_COLOR_ATTACHMENT3_EXT };

				char *fragDataMaskPrefix = "//FRAGDATAMASK-";		

				char *fragDataMaskStr = strstr( (char *)transbuf.Base(), fragDataMaskPrefix );
				if ( fragDataMaskStr )
				{
					char *fragDataActualData = fragDataMaskStr + strlen( fragDataMaskPrefix );

					int value = -1;
					sscanf( fragDataActualData, "%04x", &value );

					newprog->m_pixFragDataMask = value;
					newprog->m_pixProgram->m_fragDataMask = value;

					newprog->m_pixProgram->m_numDrawBuffers = 0;
					for( int i = 0; i < 4; i++ )
					{
						if( newprog->m_pixProgram->m_fragDataMask & ( 1 << i ) )
						{
							newprog->m_pixProgram->m_drawBuffers[ newprog->m_pixProgram->m_numDrawBuffers ] = buffers[ i ];
							newprog->m_pixProgram->m_numDrawBuffers++;
						}
					}

					if( newprog->m_pixProgram->m_numDrawBuffers ==  0 )
					{
						Assert(!"couldn't find fragment output in pixel shader");
						newprog->m_pixProgram->m_drawBuffers[ 0 ] = buffers[ 0 ];
						newprog->m_pixProgram->m_numDrawBuffers = 1;
					}
				}
				else
				{
					newprog->m_pixFragDataMask = 0;
					newprog->m_pixProgram->m_fragDataMask = 0;
				}

			}
									
			*ppShader = newprog;
			
			result = S_OK;
		}
	}


	return result;
}

IDirect3DPixelShader9::~IDirect3DPixelShader9()
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( m_device );
	GLMPRINTF(( ">-A- ~IDirect3DPixelShader9" ));

	if (m_device)
	{
		m_device->ReleasedPixelShader( this );

		if (m_pixProgram)
		{
			m_pixProgram->m_ctx->DelProgram( m_pixProgram );
			m_pixProgram = NULL;
		}
		m_device = NULL;
	}
	
	GLMPRINTF(( "<-A- ~IDirect3DPixelShader9" ));
}


HRESULT IDirect3DDevice9::SetPixelShaderNonInline(IDirect3DPixelShader9* pShader)
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( this );
	
	m_ctx->SetFragmentProgram( pShader ? pShader->m_pixProgram : NULL );
	m_pixelShader = pShader;

	return S_OK;
}

HRESULT IDirect3DDevice9::SetPixelShaderConstantFNonInline(UINT StartRegister,CONST float* pConstantData,UINT Vector4fCount)
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( this );
	TOGL_NULL_DEVICE_CHECK;
#if 0
	const uint nRegToWatch = 3;
	if ( ( ( StartRegister + Vector4fCount ) > nRegToWatch ) && ( StartRegister <= nRegToWatch ) )
	{
		GLMDebugPrintf( "-- %f %f %f %f\n", pConstantData[(nRegToWatch - StartRegister)*4+0], pConstantData[(nRegToWatch - StartRegister)*4+1], pConstantData[(nRegToWatch - StartRegister)*4+2], pConstantData[(nRegToWatch - StartRegister)*4+3] );
	}
#endif
	m_ctx->SetProgramParametersF( kGLMFragmentProgram, StartRegister, (float *)pConstantData, Vector4fCount );
	return S_OK;
}

HRESULT IDirect3DDevice9::SetPixelShaderConstantB(UINT StartRegister,CONST BOOL* pConstantData,UINT  BoolCount)
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( this );
	TOGL_NULL_DEVICE_CHECK;
	m_ctx->SetProgramParametersB( kGLMFragmentProgram, StartRegister, (int *)pConstantData, BoolCount );
	return S_OK;
}

HRESULT IDirect3DDevice9::SetPixelShaderConstantI(UINT StartRegister,CONST int* pConstantData,UINT Vector4iCount)
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( this );
	TOGL_NULL_DEVICE_CHECK;
	GLMPRINTF(("-X- Ignoring IDirect3DDevice9::SetPixelShaderConstantI call, count was %d", Vector4iCount ));
//	m_ctx->SetProgramParametersI( kGLMFragmentProgram, StartRegister, pConstantData, Vector4iCount );
	return S_OK;
}


#ifdef OSX

#pragma mark ----- Vertex Shaders - (IDirect3DDevice9)

#endif

HRESULT IDirect3DDevice9::CreateVertexShader(CONST DWORD* pFunction, IDirect3DVertexShader9** ppShader, const char *pShaderName, char *pDebugLabel)
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( this );
	HRESULT	result = D3DERR_INVALIDCALL;
	*ppShader = NULL;

	uint32 nCentroidMask = CentroidMaskFromName( false, pShaderName );
			
	{
		int numTranslations = 1;
		
		bool bVertexShader = false;

		// we can do one or two translated forms. they go together in a single buffer with some markers to allow GLM to break it up.
		// this also lets us mirror each set of translations to disk with a single file making it easier to view and edit side by side.
		
		int maxTranslationSize = 500000;	// size of any one translation

		CUtlBuffer transbuf( 1000, numTranslations * maxTranslationSize, CUtlBuffer::TEXT_BUFFER );
		CUtlBuffer tempbuf( 1000, maxTranslationSize, CUtlBuffer::TEXT_BUFFER );

		transbuf.PutString( "//GLSLvp\n" );		// this is required so GLM can crack the text apart

		// note the GLSL translator wants its own buffer
		tempbuf.EnsureCapacity( maxTranslationSize );
			
		uint glslVertexShaderOptions = D3DToGL_OptionUseEnvParams | D3DToGL_OptionDoFixupZ | D3DToGL_OptionDoFixupY;

		if ( m_ctx->Caps().m_hasNativeClipVertexMode )
		{
			// note the matched trickery over in IDirect3DDevice9::FlushStates - 
			// if on a chipset that does no have native gl_ClipVertex support, then
			// omit writes to gl_ClipVertex, and instead submit plane equations that have been altered,
			// and clipping will take place in GL space using gl_Position instead of gl_ClipVertex.
				
			// note that this is very much a hack to mate up with ATI R5xx hardware constraints, and with older
			// drivers even for later ATI parts like r6xx/r7xx.   And it doesn't work on NV parts, so you really
			// do have to choose the right way to go.
				
			glslVertexShaderOptions |= D3DToGL_OptionDoUserClipPlanes; 
		}
			
		if ( !CommandLine()->CheckParm("-disableboneuniformbuffers") )
		{
			// If using GLSL, enabling a uniform buffer specifically for bone registers. (Not currently supported with ARB shaders, which are not optimized at all anyway.)
			glslVertexShaderOptions |= D3DToGL_OptionGenerateBoneUniformBuffer;
		}

		g_D3DToOpenGLTranslatorGLSL.TranslateShader( (uint32 *) pFunction, &tempbuf, &bVertexShader, glslVertexShaderOptions, -1, nCentroidMask, pDebugLabel );
			
		transbuf.PutString( (char*)tempbuf.Base() );
		transbuf.PutString( "\n\n" );	// whitespace
				
		if ( !bVertexShader )
		{
			// don't cross the streams
			Assert(!"Can't accept pixel shader in CreateVertexShader");
			result = D3DERR_INVALIDCALL;
		}
		else
		{
			m_ObjectStats.m_nTotalVertexShaders++;

			IDirect3DVertexShader9 *newprog = new IDirect3DVertexShader9;

			newprog->m_device = this;
					
			newprog->m_vtxProgram = m_ctx->NewProgram( kGLMVertexProgram, (char *)transbuf.Base(), pShaderName ? pShaderName : "?" ) ;
			newprog->m_vtxProgram->m_nCentroidMask = nCentroidMask;

			newprog->m_vtxProgram->m_bTranslatedProgram = true;
			newprog->m_vtxProgram->m_maxVertexAttrs = 0;
			newprog->m_maxVertexAttrs = 0;
						
			// find the highwater mark..
						
			char *highWaterPrefix = "//HIGHWATER-";		// try to arrange this so it can work with pure GLSL if needed
			char *highWaterStr = strstr( (char *)transbuf.Base(), highWaterPrefix );
			if (highWaterStr)
			{
				char *highWaterActualData = highWaterStr + strlen( highWaterPrefix );
				
				int value = -1;
				sscanf( highWaterActualData, "%d", &value );

				newprog->m_vtxHighWater = value;
				newprog->m_vtxProgram->m_descs[kGLMGLSL].m_highWater = value;
			}
			else
			{
				Assert(!"couldn't find highwater mark in vertex shader");
			}

			char *highWaterBonePrefix = "//HIGHWATERBONE-";		// try to arrange this so it can work with pure GLSL if needed
			char *highWaterBoneStr = strstr( (char *)transbuf.Base(), highWaterBonePrefix );
			if (highWaterBoneStr)
			{
				char *highWaterActualData = highWaterBoneStr + strlen( highWaterBonePrefix );

				int value = -1;
				sscanf( highWaterActualData, "%d", &value );

				newprog->m_vtxHighWaterBone = value;
				newprog->m_vtxProgram->m_descs[kGLMGLSL].m_VSHighWaterBone = value;
			}
			else
			{
				newprog->m_vtxHighWaterBone = 0;
				newprog->m_vtxProgram->m_descs[kGLMGLSL].m_VSHighWaterBone = 0;
			}
									
			// find the attrib map..
			char *attribMapPrefix = "//ATTRIBMAP-";		// try to arrange this so it can work with pure GLSL if needed
			char *attribMapStr = strstr( (char *)transbuf.Base(), attribMapPrefix );
			
			if (attribMapStr)
			{
				char *attribMapActualData = attribMapStr + strlen( attribMapPrefix );
				uint nMaxVertexAttribs = 0;
				for( int i=0; i<16; i++)
				{
					int value = -1;
					char *dataItem = attribMapActualData + (i*3);
					sscanf( dataItem, "%02x", &value );
					if (value >=0)
					{
						// make sure it's not a terminator
						if (value == 0xBB)
						{
							DXABSTRACT_BREAK_ON_ERROR();
						}
					}
					else
					{
						// probably an 'xx'... check
						if ( (dataItem[0] != 'x') || (dataItem[1] != 'x') )
						{
							DXABSTRACT_BREAK_ON_ERROR();	// bad news
						}
						else
						{
							value = 0xBB;		// not likely to see one of these... "fog with usage index 11"
						}
					}

					if ( value != 0xBB )
						nMaxVertexAttribs = i;

					newprog->m_vtxAttribMap[i] = value;
				}

				newprog->m_vtxProgram->m_maxVertexAttrs = nMaxVertexAttribs + 1;
				newprog->m_maxVertexAttrs = nMaxVertexAttribs + 1;
			}
			else
			{
				DXABSTRACT_BREAK_ON_ERROR();	// that's bad...
			}
									
			*ppShader = newprog;
			
			result = S_OK;
		}
	}

	return result;
}

IDirect3DVertexShader9::~IDirect3DVertexShader9()
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( m_device );
	GLMPRINTF(( ">-A- ~IDirect3DVertexShader9" ));

	if (m_device)
	{
		m_device->ReleasedVertexShader( this );

		if (m_vtxProgram)
		{
			m_vtxProgram->m_ctx->DelProgram( m_vtxProgram );
			m_vtxProgram = NULL;
		}
		m_device = NULL;
	}
	else
	{
	}

	
	GLMPRINTF(( "<-A- ~IDirect3DVertexShader9" ));
}

HRESULT IDirect3DDevice9::SetVertexShaderNonInline(IDirect3DVertexShader9* pShader)
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( this );
	m_ctx->SetVertexProgram( pShader ? pShader->m_vtxProgram : NULL );
	m_vertexShader = pShader;
	return S_OK;
}

HRESULT IDirect3DDevice9::SetVertexShaderConstantFNonInline(UINT StartRegister,CONST float* pConstantData,UINT Vector4fCount)	// groups of 4 floats!
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( this );
	TOGL_NULL_DEVICE_CHECK;
	m_ctx->SetProgramParametersF( kGLMVertexProgram, StartRegister, (float *)pConstantData, Vector4fCount );
	return S_OK;
}

HRESULT IDirect3DDevice9::SetVertexShaderConstantBNonInline(UINT StartRegister,CONST BOOL* pConstantData,UINT  BoolCount)		// individual bool count!
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( this );
	TOGL_NULL_DEVICE_CHECK;
	m_ctx->SetProgramParametersB( kGLMVertexProgram, StartRegister, (int *)pConstantData, BoolCount );
	return S_OK;
}

HRESULT IDirect3DDevice9::SetVertexShaderConstantINonInline(UINT StartRegister,CONST int* pConstantData,UINT Vector4iCount)		// groups of 4 ints!
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( this );
	TOGL_NULL_DEVICE_CHECK;
	m_ctx->SetProgramParametersI( kGLMVertexProgram, StartRegister, (int *)pConstantData, Vector4iCount );
	return S_OK;
}

#ifdef OSX

#pragma mark ----- Shader Pairs - (IDirect3DDevice9)

#endif

// callers need to ifdef POSIX this, because this method does not exist on real DX9
HRESULT IDirect3DDevice9::LinkShaderPair( IDirect3DVertexShader9* vs, IDirect3DPixelShader9* ps )
{
	GL_BATCH_PERF_CALL_TIMER;
	// these are really GLSL "shaders" not "programs" but the old reference to "program" persists due to the assembler heritage
	if (vs->m_vtxProgram && ps->m_pixProgram)
	{
		m_ctx->LinkShaderPair( vs->m_vtxProgram, ps->m_pixProgram );
	}
	return S_OK;
}

HRESULT IDirect3DDevice9::ValidateShaderPair( IDirect3DVertexShader9* vs, IDirect3DPixelShader9* ps )
{
	GL_BATCH_PERF_CALL_TIMER;
	// these are really GLSL "shaders" not "programs" but the old reference to "program" persists due to the assembler heritage
	if (vs->m_vtxProgram && ps->m_pixProgram)
	{
		m_ctx->ValidateShaderPair( vs->m_vtxProgram, ps->m_pixProgram );
	}
	return S_OK;
}

// callers need to ifdef POSIX this, because this method does not exist on real DX9
// 
HRESULT IDirect3DDevice9::QueryShaderPair( int index, GLMShaderPairInfo *infoOut )
{
	GL_BATCH_PERF_CALL_TIMER;
	// these are really GLSL "shaders" not "programs" ...

	m_ctx->QueryShaderPair( index, infoOut );
	
	return S_OK;
}


#ifdef OSX

#pragma mark ----- Vertex Buffers and Vertex Declarations - (IDirect3DDevice9)

#endif

HRESULT IDirect3DDevice9::CreateVertexDeclaration(CONST D3DVERTEXELEMENT9* pVertexElements,IDirect3DVertexDeclaration9** ppDecl)
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( this );
	*ppDecl = NULL;
	
	// the goal here is to arrive at something which lets us quickly generate GLMVertexSetups.

	// the information we don't have, that must be inferred from the decls, is:
	// -> how many unique streams (buffers) are used - pure curiosity
	// -> what the stride and offset is for each decl.  Size you can figure out on the spot, stride requires surveying all the components in each stream first.
	//	so init an array of per-stream offsets to 0.
	//	each one is a cursor that gets bumped by decls.
	uint	streamOffsets[ D3D_MAX_STREAMS ];
	uint	streamCount = 0;
	
	uint	attribMap[16];
	uint	attribMapIndex = 0;
	memset( attribMap, 0xFF, sizeof( attribMap ) );
	
	memset( streamOffsets, 0, sizeof( streamOffsets ) );

	m_ObjectStats.m_nTotalVertexDecls++;

	IDirect3DVertexDeclaration9 *decl9 = new IDirect3DVertexDeclaration9;
	decl9->m_device = this;
	
	decl9->m_elemCount = 0;
	
	for (const D3DVERTEXELEMENT9 *src = pVertexElements; (src->Stream != 0xFF); src++)
	{
		// element
		D3DVERTEXELEMENT9_GL *elem = &decl9->m_elements[ decl9->m_elemCount++ ];

		// copy the D3D decl wholesale.
		elem->m_dxdecl = *src;
		
		// latch current offset in this stream.
		elem->m_gldecl.m_offset = streamOffsets[ elem->m_dxdecl.Stream ];
		
		// figure out size of this attr and move the cursor
		// if cursor was on zero, bump the active stream count
		
		if (!streamOffsets[ elem->m_dxdecl.Stream ])
			streamCount++;
		
		int bytes = 0;
		switch( elem->m_dxdecl.Type )
		{
			case D3DDECLTYPE_FLOAT1:	elem->m_gldecl.m_nCompCount = 1; elem->m_gldecl.m_datatype = GL_FLOAT; elem->m_gldecl.m_normalized=0; bytes = 4; break;
			case D3DDECLTYPE_FLOAT2:	elem->m_gldecl.m_nCompCount = 2; elem->m_gldecl.m_datatype = GL_FLOAT; elem->m_gldecl.m_normalized=0; bytes = 8; break;

			//case D3DVSDT_FLOAT3:
			case D3DDECLTYPE_FLOAT3:	elem->m_gldecl.m_nCompCount = 3; elem->m_gldecl.m_datatype = GL_FLOAT; elem->m_gldecl.m_normalized=0; bytes = 12; break;
			
			//case D3DVSDT_FLOAT4:
			case D3DDECLTYPE_FLOAT4:	elem->m_gldecl.m_nCompCount = 4; elem->m_gldecl.m_datatype = GL_FLOAT; elem->m_gldecl.m_normalized=0; bytes = 16; break;
			
			// case D3DVSDT_UBYTE4:		
			case D3DDECLTYPE_D3DCOLOR:
			case D3DDECLTYPE_UBYTE4:
			case D3DDECLTYPE_UBYTE4N:
				
				// Force this path since we're on 10.6.2 and can't rely on EXT_vertex_array_bgra
				if ( 1 )
				{
					// pass 4 UB's but we know this is out of order compared to D3DCOLOR data
					elem->m_gldecl.m_nCompCount = 4; elem->m_gldecl.m_datatype = GL_UNSIGNED_BYTE;
				}
				else
				{
					// pass a GL BGRA color courtesy of http://www.opengl.org/registry/specs/ARB/vertex_array_bgra.txt
					elem->m_gldecl.m_nCompCount = GL_BGRA; elem->m_gldecl.m_datatype = GL_UNSIGNED_BYTE;
				}

				elem->m_gldecl.m_normalized = ( (elem->m_dxdecl.Type == D3DDECLTYPE_D3DCOLOR) ||
												(elem->m_dxdecl.Type == D3DDECLTYPE_UBYTE4N) );
				
				bytes = 4;
			break;
			
			case D3DDECLTYPE_SHORT2:
				// pass 2 US's but we know this is out of order compared to D3DCOLOR data
				elem->m_gldecl.m_nCompCount = 2; elem->m_gldecl.m_datatype = GL_SHORT;

				elem->m_gldecl.m_normalized = 0;
				bytes = 4;
			break;
			
			default:	DXABSTRACT_BREAK_ON_ERROR(); return D3DERR_INVALIDCALL; break;

			/*
				typedef enum _D3DDECLTYPE
				{
					D3DDECLTYPE_FLOAT1    =  0,  // 1D float expanded to (value, 0., 0., 1.)
					D3DDECLTYPE_FLOAT2    =  1,  // 2D float expanded to (value, value, 0., 1.)
					D3DDECLTYPE_FLOAT3    =  2,  // 3D float expanded to (value, value, value, 1.)
					D3DDECLTYPE_FLOAT4    =  3,  // 4D float
					D3DDECLTYPE_D3DCOLOR  =  4,  // 4D packed unsigned bytes mapped to 0. to 1. range
												 // Input is in D3DCOLOR format (ARGB) expanded to (R, G, B, A)
					D3DDECLTYPE_UBYTE4    =  5,  // 4D unsigned byte
					D3DDECLTYPE_SHORT2    =  6,  // 2D signed short expanded to (value, value, 0., 1.)
					D3DDECLTYPE_SHORT4    =  7,  // 4D signed short

				// The following types are valid only with vertex shaders >= 2.0


					D3DDECLTYPE_UBYTE4N   =  8,  // Each of 4 bytes is normalized by dividing to 255.0
					D3DDECLTYPE_SHORT2N   =  9,  // 2D signed short normalized (v[0]/32767.0,v[1]/32767.0,0,1)
					D3DDECLTYPE_SHORT4N   = 10,  // 4D signed short normalized (v[0]/32767.0,v[1]/32767.0,v[2]/32767.0,v[3]/32767.0)
					D3DDECLTYPE_USHORT2N  = 11,  // 2D unsigned short normalized (v[0]/65535.0,v[1]/65535.0,0,1)
					D3DDECLTYPE_USHORT4N  = 12,  // 4D unsigned short normalized (v[0]/65535.0,v[1]/65535.0,v[2]/65535.0,v[3]/65535.0)
					D3DDECLTYPE_UDEC3     = 13,  // 3D unsigned 10 10 10 format expanded to (value, value, value, 1)
					D3DDECLTYPE_DEC3N     = 14,  // 3D signed 10 10 10 format normalized and expanded to (v[0]/511.0, v[1]/511.0, v[2]/511.0, 1)
					D3DDECLTYPE_FLOAT16_2 = 15,  // Two 16-bit floating point values, expanded to (value, value, 0, 1)
					D3DDECLTYPE_FLOAT16_4 = 16,  // Four 16-bit floating point values
					D3DDECLTYPE_UNUSED    = 17,  // When the type field in a decl is unused.
				} D3DDECLTYPE;
			*/
		}
		
		// write the offset and move the cursor
		elem->m_gldecl.m_offset = streamOffsets[elem->m_dxdecl.Stream];
		streamOffsets[ elem->m_dxdecl.Stream ] += bytes;
		
		// cannot write m_stride yet, so zero it
		elem->m_gldecl.m_stride = 0;
		
		elem->m_gldecl.m_pBuffer = NULL;	// must be filled in at draw time..
		
		// elem count was already bumped.
		
		// update attrib map
		attribMap[ attribMapIndex++ ] = (elem->m_dxdecl.Usage << 4) | (elem->m_dxdecl.UsageIndex);
	}
	// the loop is done, we now know how many active streams there are, how many atribs are active in the declaration,
	// and how big each one is in terms of stride.

	// all that is left is to go back and write the strides - the stride comes from the stream offset cursors accumulated earlier.
	for( uint j=0; j< decl9->m_elemCount; j++)
	{
		D3DVERTEXELEMENT9_GL *elem = &decl9->m_elements[ j ];
		
		elem->m_gldecl.m_stride = streamOffsets[ elem->m_dxdecl.Stream ];
	}
		
	memset( decl9->m_VertexAttribDescToStreamIndex, 0xFF, sizeof( decl9->m_VertexAttribDescToStreamIndex ) );
	D3DVERTEXELEMENT9_GL *pDeclElem = decl9->m_elements;
	for( uint j = 0; j < decl9->m_elemCount; j++, pDeclElem++)
	{
		uint nPackedVertexAttribDesc = ( pDeclElem->m_dxdecl.Usage << 4 ) | pDeclElem->m_dxdecl.UsageIndex;
		if ( nPackedVertexAttribDesc == 0xBB )
		{
			// 0xBB is a reserved packed vertex attrib value - shouldn't encounter in practice
			DXABSTRACT_BREAK_ON_ERROR();
		}
		decl9->m_VertexAttribDescToStreamIndex[ nPackedVertexAttribDesc ] = j;
	}
	
	*ppDecl = decl9;
	
	return S_OK;
}

IDirect3DVertexDeclaration9::~IDirect3DVertexDeclaration9()
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( m_device );
	GLMPRINTF(("-A- ~IDirect3DVertexDeclaration9 signpost"));

	m_device->ReleasedVertexDeclaration( this );
}

HRESULT IDirect3DDevice9::SetVertexDeclarationNonInline(IDirect3DVertexDeclaration9* pDecl)
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( this );
	m_pVertDecl = pDecl;
	return S_OK;
}

HRESULT IDirect3DDevice9::SetFVF(DWORD FVF)
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( this );
	DXABSTRACT_BREAK_ON_ERROR();
	return S_OK;
}

HRESULT IDirect3DDevice9::GetFVF(DWORD* pFVF)
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( this );
	DXABSTRACT_BREAK_ON_ERROR();
	return S_OK;
}


#ifdef OSX

#pragma mark ----- Vertex Buffers and Streams - (IDirect3DDevice9)

#pragma mark ----- Create function moved to be adjacent to other buffer methods

#endif

HRESULT IDirect3DDevice9::SetStreamSourceNonInline(UINT StreamNumber,IDirect3DVertexBuffer9* pStreamData,UINT OffsetInBytes,UINT Stride)
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( this );
	Assert( StreamNumber < D3D_MAX_STREAMS );
	Assert( ( Stride & 3 ) == 0 ); // we support non-DWORD aligned strides, but on some drivers (like AMD's) perf goes off a cliff 

	// perfectly legal to see a vertex buffer of NULL get passed in here.
	// so we need an array to track these.
	// OK, we are being given the stride, we don't need to calc it..
	
	GLMPRINTF(("-X- IDirect3DDevice9::SetStreamSource setting stream #%d to D3D buf %p (GL name %d); offset %d, stride %d", StreamNumber, pStreamData, (pStreamData) ? pStreamData->m_vtxBuffer->m_nHandle: -1, OffsetInBytes, Stride));

	if ( !pStreamData )
	{
		OffsetInBytes = 0;
		Stride = 0;
		
		m_vtx_buffers[ StreamNumber ] = m_pDummy_vtx_buffer;
	}
	else
	{
		// We do not support strides of 0
		Assert( Stride > 0 );
		m_vtx_buffers[ StreamNumber ] = pStreamData->m_vtxBuffer;
	}

	m_streams[ StreamNumber ].m_vtxBuffer = pStreamData;
	m_streams[ StreamNumber ].m_offset	= OffsetInBytes;
	m_streams[ StreamNumber ].m_stride	= Stride;
		
	return S_OK;
}

#ifdef OSX

#pragma mark ----- Index Buffers - (IDirect3DDevice9)
#pragma mark ----- Creatue function relocated to be adjacent to the rest of the index buffer methods

#endif

HRESULT IDirect3DDevice9::SetIndicesNonInline(IDirect3DIndexBuffer9* pIndexData)
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( this );
	// just latch it.
	m_indices.m_idxBuffer = pIndexData;

	return S_OK;
}


#ifdef OSX

#pragma mark ----- Release Handlers - (IDirect3DDevice9)

#endif

void IDirect3DDevice9::ReleasedVertexDeclaration( IDirect3DVertexDeclaration9 *pDecl )
{
	m_ctx->ClearCurAttribs();

	Assert( m_ObjectStats.m_nTotalVertexDecls >= 1 );
	m_ObjectStats.m_nTotalVertexDecls--;
}

void IDirect3DDevice9::ReleasedTexture( IDirect3DBaseTexture9 *baseTex )
{
	GL_BATCH_PERF_CALL_TIMER;
	TOGL_NULL_DEVICE_CHECK_RET_VOID;

	// see if this texture is referenced in any of the texture units and scrub it if so.
	for( int i=0; i< GLM_SAMPLER_COUNT; i++)
	{
		if (m_textures[i] == baseTex)
		{
			m_textures[i] = NULL;
			m_ctx->SetSamplerTex( i, NULL );	// texture sets go straight through to GLM, no dirty bit
		}
	}
}

void IDirect3DDevice9::ReleasedCGLMTex( CGLMTex *pTex)
{
	GL_BATCH_PERF_CALL_TIMER;
	TOGL_NULL_DEVICE_CHECK_RET_VOID;

	ScrubFBOMap( pTex );
	if ( pTex->m_layout )
	{
		if ( pTex->m_layout->m_key.m_texFlags & kGLMTexRenderable )
		{
			Assert( m_ObjectStats.m_nTotalRenderTargets >= 1 );
			m_ObjectStats.m_nTotalRenderTargets--;
		}
	}
}
void IDirect3DDevice9::ReleasedSurface( IDirect3DSurface9 *pSurface )
{
	for( int i = 0; i < 4; i++ )
	{
		if ( m_pRenderTargets[i] == pSurface )
		{
			// this was a surprise release... scrub it
			m_pRenderTargets[i] = NULL;
			m_bFBODirty = true;
			GLMPRINTF(( "-A- Scrubbed pSurface %08x from m_pRenderTargets[%d]", pSurface, i ));
		}
	}

	if ( m_pDepthStencil == pSurface )
	{
		m_pDepthStencil = NULL;
		m_bFBODirty = true;
		GLMPRINTF(( "-A- Scrubbed pSurface %08x from m_pDepthStencil", pSurface ));
	}
	
	if ( m_pDefaultColorSurface == pSurface )
	{
		m_pDefaultColorSurface = NULL;
		GLMPRINTF(( "-A- Scrubbed pSurface %08x from m_pDefaultColorSurface", pSurface ));
	}
	
	if ( m_pDefaultDepthStencilSurface == pSurface )
	{
		m_pDefaultDepthStencilSurface = NULL;
		GLMPRINTF(( "-A- Scrubbed pSurface %08x from m_pDefaultDepthStencilSurface", pSurface ));
	}

	Assert( m_ObjectStats.m_nTotalSurfaces >= 1 );
	m_ObjectStats.m_nTotalSurfaces--;
}

void IDirect3DDevice9::ReleasedPixelShader( IDirect3DPixelShader9 *pixelShader )
{
	if ( m_pixelShader == pixelShader )
	{
		m_pixelShader = NULL;
		GLMPRINTF(( "-A- Scrubbed pixel shader %08x from m_pixelShader", pixelShader ));
	}
	m_ctx->ReleasedShader();
	
	Assert( m_ObjectStats.m_nTotalPixelShaders >= 1 );
	m_ObjectStats.m_nTotalPixelShaders--;
}

void IDirect3DDevice9::ReleasedVertexShader( IDirect3DVertexShader9 *vertexShader )
{
	if ( m_vertexShader == vertexShader )
	{
		m_vertexShader = NULL;
		GLMPRINTF(( "-A- Scrubbed vertex shader %08x from m_vertexShader", vertexShader ));
	}
	m_ctx->ClearCurAttribs();
	m_ctx->ReleasedShader();
	
	Assert( m_ObjectStats.m_nTotalVertexShaders >= 1 );
	m_ObjectStats.m_nTotalVertexShaders--;
}

void IDirect3DDevice9::ReleasedVertexBuffer( IDirect3DVertexBuffer9 *vertexBuffer )
{
	for (int i=0; i< D3D_MAX_STREAMS; i++)
	{
		if ( m_streams[i].m_vtxBuffer == vertexBuffer )
		{
			m_streams[i].m_vtxBuffer = NULL;
			m_vtx_buffers[i] = m_pDummy_vtx_buffer;

			GLMPRINTF(( "-A- Scrubbed vertex buffer %08x from m_streams[%d]", vertexBuffer, i ));
		}
	}
	m_ctx->ClearCurAttribs();

	Assert( m_ObjectStats.m_nTotalVertexBuffers >= 1 );
	m_ObjectStats.m_nTotalVertexBuffers--;
}

void IDirect3DDevice9::ReleasedIndexBuffer( IDirect3DIndexBuffer9 *indexBuffer )
{
	if ( m_indices.m_idxBuffer == indexBuffer )
	{
		m_indices.m_idxBuffer = NULL;
		GLMPRINTF(( "-A- Scrubbed index buffer %08x from m_indices", indexBuffer ));
	}
	
	Assert( m_ObjectStats.m_nTotalIndexBuffers >= 1 );
	m_ObjectStats.m_nTotalIndexBuffers--;
}


void IDirect3DDevice9::ReleasedQuery( IDirect3DQuery9 *query )
{
	Assert( m_ObjectStats.m_nTotalQueries >= 1 );
	m_ObjectStats.m_nTotalQueries--;
}

#ifdef OSX

#pragma mark ----- Queries - (IDirect3DDevice9)

#endif

// note that detection of whether queries are supported is done by trying to create one.
// so for GL, be observant here of whether we have that capability or not.
// pretty much have this everywhere but i950.

HRESULT IDirect3DDevice9::CreateQuery(D3DQUERYTYPE Type,IDirect3DQuery9** ppQuery)
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( this );
	if (m_ctx->Caps().m_hasOcclusionQuery)
	{
		m_ObjectStats.m_nTotalQueries++;

		IDirect3DQuery9	*newquery = new IDirect3DQuery9;
		
		newquery->m_device = this;
		
		newquery->m_type = Type;
		newquery->m_ctx = m_ctx;
		newquery->m_nIssueStartThreadID = 0;
		newquery->m_nIssueEndThreadID = 0;
		newquery->m_nIssueStartDrawCallIndex = 0;
		newquery->m_nIssueEndDrawCallIndex = 0;

		GLMQueryParams	params;
		memset( &params, 0, sizeof(params) );
		
		//bool known = false;
		switch(newquery->m_type)
		{
			case	D3DQUERYTYPE_OCCLUSION:				/* D3DISSUE_BEGIN, D3DISSUE_END */
				// create an occlusion query
				params.m_type = EOcclusion;
			break;
			
			case	D3DQUERYTYPE_EVENT:					/* D3DISSUE_END */
				params.m_type = EFence;
			break;
			
			case	D3DQUERYTYPE_RESOURCEMANAGER:		/* D3DISSUE_END */
			case	D3DQUERYTYPE_TIMESTAMP:				/* D3DISSUE_END */
			case	D3DQUERYTYPE_TIMESTAMPFREQ:			/* D3DISSUE_END */
			case	D3DQUERYTYPE_INTERFACETIMINGS:		/* D3DISSUE_BEGIN, D3DISSUE_END */
			case	D3DQUERYTYPE_PIXELTIMINGS:			/* D3DISSUE_BEGIN, D3DISSUE_END */
			case	D3DQUERYTYPE_CACHEUTILIZATION:		/* D3DISSUE_BEGIN, D3DISSUE_END */
				Assert( !"Un-implemented query type" );
			break;
			
			default:
				Assert( !"Unknown query type" );
			break;
		}
		newquery->m_query = m_ctx->NewQuery( &params );
		
		*ppQuery = newquery;
		return S_OK;
	}
	else
	{
		*ppQuery = NULL;
		return -1;	// failed
	}

}

IDirect3DQuery9::~IDirect3DQuery9()
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( m_device );
	GLMPRINTF((">-A- ~IDirect3DQuery9"));

	if (m_device)
	{
		m_device->ReleasedQuery( this );

		if (m_query)
		{
			GLMPRINTF((">-A- ~IDirect3DQuery9 freeing m_query"));
			
			m_query->m_ctx->DelQuery( m_query );
			m_query = NULL;

			GLMPRINTF(("<-A- ~IDirect3DQuery9 freeing m_query done"));
		}
		m_device = NULL;
	}
	
	GLMPRINTF(("<-A- ~IDirect3DQuery9"));
}

#ifdef OSX

#pragma mark ----- Render States - (IDirect3DDevice9)

#endif

#define	D3DRS_VALUE_LIMIT 210

struct	D3D_RSINFO
{
	int					m_class;
	D3DRENDERSTATETYPE	m_state;
	DWORD				m_defval;
	// m_class runs 0-3.
	// 3 = must implement - fully general - "obey"
	// 2 = implement setup to the default value (it has a GL effect but does not change later) "obey once"
	// 1 = "fake implement" setup to the default value no GL effect, debug break if anything but default value comes through - "ignore"
	// 0 = game never ever sets this one, break if someone even tries. "complain"
};

D3D_RSINFO g_D3DRS_INFO_unpacked[ D3DRS_VALUE_LIMIT+1 ];

#ifdef D3D_RSI
	#error macro collision... rename this
#else
	#define D3D_RSI(nclass,nstate,ndefval)	{ nclass, nstate, ndefval }
#endif

// FP conversions to hex courtesy of http://babbage.cs.qc.cuny.edu/IEEE-754/Decimal.html
#define	CONST_DZERO		0x00000000
#define	CONST_DONE		0x3F800000
#define	CONST_D64		0x42800000
#define	DONT_KNOW_YET	0x31415926


// see http://www.toymaker.info/Games/html/render_states.html

D3D_RSINFO	g_D3DRS_INFO_packed[] = 
{
	// these do not have to be in any particular order.  they get unpacked into the empty array above for direct indexing.

	D3D_RSI(	3,	D3DRS_ZENABLE,						DONT_KNOW_YET			),	// enable Z test (or W buffering)
	D3D_RSI(	3,	D3DRS_ZWRITEENABLE,					DONT_KNOW_YET			),	// enable Z write
	D3D_RSI(	3,	D3DRS_ZFUNC,						DONT_KNOW_YET			),	// select Z func

	D3D_RSI(	3,	D3DRS_COLORWRITEENABLE,				TRUE					),	// see transitiontable.cpp "APPLY_RENDER_STATE_FUNC( D3DRS_COLORWRITEENABLE, ColorWriteEnable )"

	D3D_RSI(	3,	D3DRS_CULLMODE,						D3DCULL_CCW				),	// backface cull control

	D3D_RSI(	3,	D3DRS_ALPHABLENDENABLE,				DONT_KNOW_YET			),	// ->CTransitionTable::ApplySeparateAlphaBlend and ApplyAlphaBlend
	D3D_RSI(	3,	D3DRS_BLENDOP,						D3DBLENDOP_ADD			),
	D3D_RSI(	3,	D3DRS_SRCBLEND,						DONT_KNOW_YET			),
	D3D_RSI(	3,	D3DRS_DESTBLEND,					DONT_KNOW_YET			),

	D3D_RSI(	1,	D3DRS_SEPARATEALPHABLENDENABLE,		FALSE					),	// hit in CTransitionTable::ApplySeparateAlphaBlend
	D3D_RSI(	1,	D3DRS_SRCBLENDALPHA,				D3DBLEND_ONE			),	// going to demote these to class 1 until I figure out if they are implementable
	D3D_RSI(	1,	D3DRS_DESTBLENDALPHA,				D3DBLEND_ZERO			),
	D3D_RSI(	1,	D3DRS_BLENDOPALPHA,					D3DBLENDOP_ADD			),

	// what is the deal with alpha test... looks like it is inited to off.
	D3D_RSI(	3,	D3DRS_ALPHATESTENABLE,				0						),
	D3D_RSI(	3,	D3DRS_ALPHAREF,						0						),
	D3D_RSI(	3,	D3DRS_ALPHAFUNC,					D3DCMP_GREATEREQUAL		),

	D3D_RSI(	3,	D3DRS_STENCILENABLE,				FALSE					),
	D3D_RSI(	3,	D3DRS_STENCILFAIL,					D3DSTENCILOP_KEEP		),
	D3D_RSI(	3,	D3DRS_STENCILZFAIL,					D3DSTENCILOP_KEEP		),
	D3D_RSI(	3,	D3DRS_STENCILPASS,					D3DSTENCILOP_KEEP		),
	D3D_RSI(	3,	D3DRS_STENCILFUNC,					D3DCMP_ALWAYS			),
	D3D_RSI(	3,	D3DRS_STENCILREF,					0						),
	D3D_RSI(	3,	D3DRS_STENCILMASK,					0xFFFFFFFF				),
	D3D_RSI(	3,	D3DRS_STENCILWRITEMASK,				0xFFFFFFFF				),

	D3D_RSI(	3,	D3DRS_TWOSIDEDSTENCILMODE,			FALSE					),
	D3D_RSI(	3,	D3DRS_CCW_STENCILFAIL,				D3DSTENCILOP_KEEP		),
	D3D_RSI(	3,	D3DRS_CCW_STENCILZFAIL,				D3DSTENCILOP_KEEP		),
	D3D_RSI(	3,	D3DRS_CCW_STENCILPASS,				D3DSTENCILOP_KEEP		),
	D3D_RSI(	3,	D3DRS_CCW_STENCILFUNC,				D3DCMP_ALWAYS 			),

	D3D_RSI(	3,	D3DRS_FOGENABLE,					FALSE					),	// see CShaderAPIDx8::FogMode and friends - be ready to do the ARB fog linear option madness
	D3D_RSI(	3,	D3DRS_FOGCOLOR,						0						),
	D3D_RSI(	3,	D3DRS_FOGTABLEMODE,					D3DFOG_NONE				),
	D3D_RSI(	3,	D3DRS_FOGSTART,						CONST_DZERO				),
	D3D_RSI(	3,	D3DRS_FOGEND,						CONST_DONE				),
	D3D_RSI(	3,	D3DRS_FOGDENSITY,					CONST_DZERO				),
	D3D_RSI(	3,	D3DRS_RANGEFOGENABLE,				FALSE					),
	D3D_RSI(	3,	D3DRS_FOGVERTEXMODE,				D3DFOG_NONE				),	// watch out for CShaderAPIDx8::CommitPerPassFogMode....

	D3D_RSI(	3,	D3DRS_MULTISAMPLEANTIALIAS,			TRUE					),
	D3D_RSI(	3,	D3DRS_MULTISAMPLEMASK,				0xFFFFFFFF				),

	D3D_RSI(	3,	D3DRS_SCISSORTESTENABLE,			FALSE					),	// heed IDirect3DDevice9::SetScissorRect

	D3D_RSI(	3,	D3DRS_DEPTHBIAS,					CONST_DZERO				),
	D3D_RSI(	3,	D3DRS_SLOPESCALEDEPTHBIAS,			CONST_DZERO				),

	D3D_RSI(	3,	D3DRS_COLORWRITEENABLE1,			0x0000000f				),
	D3D_RSI(	3,	D3DRS_COLORWRITEENABLE2,			0x0000000f				),
	D3D_RSI(	3,	D3DRS_COLORWRITEENABLE3,			0x0000000f				),

	D3D_RSI(	3,	D3DRS_SRGBWRITEENABLE,				0						),	// heeded but ignored..

	D3D_RSI(	2,	D3DRS_CLIPPING,						TRUE					),	// um, yeah, clipping is enabled (?)
	D3D_RSI(	3,	D3DRS_CLIPPLANEENABLE,				0						),	// mask 1<<n of active user clip planes.

	D3D_RSI(	0,	D3DRS_LIGHTING,						0						),	// strange, someone turns it on then off again. move to class 0 and just ignore it (lie)?

	D3D_RSI(	3,	D3DRS_FILLMODE,						D3DFILL_SOLID			),

	D3D_RSI(	1,	D3DRS_SHADEMODE,					D3DSHADE_GOURAUD		),
	D3D_RSI(	1,	D3DRS_LASTPIXEL,					TRUE					),
	D3D_RSI(	1,	D3DRS_DITHERENABLE,					0						),	//set to false by game, no one sets it to true
	D3D_RSI(	1,	D3DRS_SPECULARENABLE,				FALSE					),
	D3D_RSI(	1,	D3DRS_TEXTUREFACTOR,				0xFFFFFFFF				),	// watch out for CShaderAPIDx8::Color3f et al.
	D3D_RSI(	1,	D3DRS_WRAP0,						0						),
	D3D_RSI(	1,	D3DRS_WRAP1,						0						),
	D3D_RSI(	1,	D3DRS_WRAP2,						0						),
	D3D_RSI(	1,	D3DRS_WRAP3,						0						),
	D3D_RSI(	1,	D3DRS_WRAP4,						0						),
	D3D_RSI(	1,	D3DRS_WRAP5,						0						),
	D3D_RSI(	1,	D3DRS_WRAP6,						0						),
	D3D_RSI(	1,	D3DRS_WRAP7,						0						),
	D3D_RSI(	1,	D3DRS_AMBIENT,						0						),	// FF lighting, no
	D3D_RSI(	1,	D3DRS_COLORVERTEX,					TRUE					),	// FF lighing again
	D3D_RSI(	1,	D3DRS_LOCALVIEWER,					TRUE					),	// FF lighting
	D3D_RSI(	1,	D3DRS_NORMALIZENORMALS,				FALSE					),	// FF mode I think.  CShaderAPIDx8::SetVertexBlendState says it might switch this on when skinning is in play
	D3D_RSI(	1,	D3DRS_DIFFUSEMATERIALSOURCE,		D3DMCS_MATERIAL			),	// hit only in CShaderAPIDx8::ResetRenderState
	D3D_RSI(	1,	D3DRS_SPECULARMATERIALSOURCE,		D3DMCS_COLOR2			),
	D3D_RSI(	1,	D3DRS_AMBIENTMATERIALSOURCE,		D3DMCS_MATERIAL			),
	D3D_RSI(	1,	D3DRS_EMISSIVEMATERIALSOURCE,		D3DMCS_MATERIAL			),
	D3D_RSI(	1,	D3DRS_VERTEXBLEND,					D3DVBF_DISABLE			),	// also being set by CShaderAPIDx8::SetVertexBlendState, so might be FF
	D3D_RSI(	1,	D3DRS_POINTSIZE,					CONST_DONE				),
	D3D_RSI(	1,	D3DRS_POINTSIZE_MIN,				CONST_DONE				),
	D3D_RSI(	1,	D3DRS_POINTSPRITEENABLE,			FALSE					),
	D3D_RSI(	1,	D3DRS_POINTSCALEENABLE,				FALSE					),
	D3D_RSI(	1,	D3DRS_POINTSCALE_A,					CONST_DONE				),
	D3D_RSI(	1,	D3DRS_POINTSCALE_B,					CONST_DZERO				),
	D3D_RSI(	1,	D3DRS_POINTSCALE_C,					CONST_DZERO				),
	D3D_RSI(	1,	D3DRS_PATCHEDGESTYLE,				D3DPATCHEDGE_DISCRETE	),
	D3D_RSI(	1,	D3DRS_DEBUGMONITORTOKEN,			D3DDMT_ENABLE			),
	D3D_RSI(	1,	D3DRS_POINTSIZE_MAX,				CONST_D64				),
	D3D_RSI(	1,	D3DRS_INDEXEDVERTEXBLENDENABLE,		FALSE					),
	D3D_RSI(	1,	D3DRS_TWEENFACTOR,					CONST_DZERO				),
	D3D_RSI(	1,	D3DRS_POSITIONDEGREE,				D3DDEGREE_CUBIC			),
	D3D_RSI(	1,	D3DRS_NORMALDEGREE,					D3DDEGREE_LINEAR		),
	D3D_RSI(	1,	D3DRS_ANTIALIASEDLINEENABLE,		FALSE					),	// just ignore it
	D3D_RSI(	1,	D3DRS_MINTESSELLATIONLEVEL,			CONST_DONE				),
	D3D_RSI(	1,	D3DRS_MAXTESSELLATIONLEVEL,			CONST_DONE				),
	D3D_RSI(	1,	D3DRS_ADAPTIVETESS_X,				CONST_DZERO				),
	D3D_RSI(	1,	D3DRS_ADAPTIVETESS_Y,				CONST_DZERO				), // Overridden as Alpha-to-coverage contrl
	D3D_RSI(	1,	D3DRS_ADAPTIVETESS_Z,				CONST_DONE				),
	D3D_RSI(	1,	D3DRS_ADAPTIVETESS_W,				CONST_DZERO				),
	D3D_RSI(	1,	D3DRS_ENABLEADAPTIVETESSELLATION,	FALSE					),
	D3D_RSI(	1,	D3DRS_BLENDFACTOR,					0xffffffff				),
	D3D_RSI(	1,	D3DRS_WRAP8,						0						),
	D3D_RSI(	1,	D3DRS_WRAP9,						0						),
	D3D_RSI(	1,	D3DRS_WRAP10,						0						),
	D3D_RSI(	1,	D3DRS_WRAP11,						0						),
	D3D_RSI(	1,	D3DRS_WRAP12,						0						),
	D3D_RSI(	1,	D3DRS_WRAP13,						0						),
	D3D_RSI(	1,	D3DRS_WRAP14,						0						),
	D3D_RSI(	1,	D3DRS_WRAP15,						0						),
	D3D_RSI(	-1,	(D3DRENDERSTATETYPE)0,				0						)	// terminator
};

void	UnpackD3DRSITable( void )
{
	memset (g_D3DRS_INFO_unpacked, 0, sizeof(g_D3DRS_INFO_unpacked) );
	
	for( D3D_RSINFO *packed = g_D3DRS_INFO_packed; packed->m_class >= 0; packed++ )
	{
		if ( (packed->m_state <0) || (packed->m_state >= D3DRS_VALUE_LIMIT) )
		{
			// bad
			DXABSTRACT_BREAK_ON_ERROR();
		}
		else
		{
			// dispatch it to the unpacked array
			g_D3DRS_INFO_unpacked[ packed->m_state ] = *packed;
		}
	}
}

// convenience functions

#ifdef OSX

#pragma mark ----- Sampler States - (IDirect3DDevice9)

#endif

void IDirect3DDevice9::FlushClipPlaneEquation()
{
	for( int x=0; x<kGLMUserClipPlanes; x++)
	{
		GLClipPlaneEquation_t temp1;	// Antonio's way
		GLClipPlaneEquation_t temp2;	// our way

		// if we don't have native clip vertex support. then munge the plane coeffs
		// this should engage on ALL ATI PARTS < 10.6.4
		// and should continue to engage on R5xx forever.
			
		if ( !m_ctx->Caps().m_hasNativeClipVertexMode )
		{
			// hacked coeffs = { src->x, -src->y, 0.5f * src->z, src->w + (0.5f * src->z) };
			// Antonio's trick - so we can use gl_Position as the clippee, not gl_ClipVertex.

			GLClipPlaneEquation_t *equ = &gl.m_ClipPlaneEquation[x];

			///////////////// temp1
			temp1.x	=	equ->x;
			temp1.y	=	equ->y * -1.0;
			temp1.z	=	equ->z * 0.5;
			temp1.w	=	equ->w + (equ->z * 0.5);

				
			//////////////// temp2
			VMatrix mat1(	1,	0,	0,	0,
							0,	-1,	0,	0,
							0,	0,	2,	-1,
							0,	0,	0,	1
							);
			//mat1 = mat1.Transpose();
								
			VMatrix mat2;
			bool success = mat1.InverseGeneral( mat2 );
				
			if (success)
			{
				VMatrix mat3;
				mat3 = mat2.Transpose();

				VPlane origPlane( Vector( equ->x, equ->y, equ->z ), equ->w );
				VPlane newPlane;
					
				newPlane = mat3 * origPlane /* * mat3 */;
					
				VPlane finalPlane = newPlane;
					
				temp2.x = newPlane.m_Normal.x;
				temp2.y = newPlane.m_Normal.y;
				temp2.z = newPlane.m_Normal.z;
				temp2.w = newPlane.m_Dist;
			}
			else
			{
				temp2.x = 0;
				temp2.y = 0;
				temp2.z = 0;
				temp2.w = 0;
			}
		}
		else
		{
			temp1 = temp2 = gl.m_ClipPlaneEquation[x];
		}

		if (1)	//GLMKnob("caps-key",NULL)==0.0)
		{
			m_ctx->WriteClipPlaneEquation( &temp1, x );		// no caps lock = Antonio or classic
				
			/*
			if (x<1)
			{
				GLMPRINTF(( " plane %d  √vers1[ %5.2f %5.2f %5.2f %5.2f ]    vers2[ %5.2f %5.2f %5.2f %5.2f ]",
					x,
					temp1.x,temp1.y,temp1.z,temp1.w,
					temp2.x,temp2.y,temp2.z,temp2.w
				));
			}
			*/
		}
		else
		{
			m_ctx->WriteClipPlaneEquation( &temp2, x );		// caps = our way or classic

			/*
			if (x<1)
			{
				GLMPRINTF(( " plane %d   vers1[ %5.2f %5.2f %5.2f %5.2f ]    √vers2[ %5.2f %5.2f %5.2f %5.2f ]",
					x,
					temp1.x,temp1.y,temp1.z,temp1.w,
					temp2.x,temp2.y,temp2.z,temp2.w
				));
			}
			*/
		}
	}
}

void IDirect3DDevice9::InitStates()
{
	m_ctx->m_AlphaTestEnable.Read( &gl.m_AlphaTestEnable, 0 );
	m_ctx->m_AlphaTestFunc.Read( &gl.m_AlphaTestFunc, 0 );
	m_ctx->m_CullFaceEnable.Read( &gl.m_CullFaceEnable, 0 );
	m_ctx->m_DepthBias.Read( &gl.m_DepthBias, 0 );
	m_ctx->m_ScissorEnable.Read( &gl.m_ScissorEnable, 0 );
	m_ctx->m_ScissorBox.Read( &gl.m_ScissorBox, 0 );
	m_ctx->m_ViewportBox.Read( &gl.m_ViewportBox, 0 );
	m_ctx->m_ViewportDepthRange.Read( &gl.m_ViewportDepthRange, 0 );

	for( int x=0; x<kGLMUserClipPlanes; x++)
		m_ctx->m_ClipPlaneEnable.ReadIndex( &gl.m_ClipPlaneEnable[x], x, 0 );

	m_ctx->m_PolygonMode.Read( &gl.m_PolygonMode, 0 );
	m_ctx->m_CullFrontFace.Read( &gl.m_CullFrontFace, 0 );
	m_ctx->m_AlphaToCoverageEnable.Read( &gl.m_AlphaToCoverageEnable, 0 );
	m_ctx->m_BlendEquation.Read( &gl.m_BlendEquation, 0 );
	m_ctx->m_BlendColor.Read( &gl.m_BlendColor, 0 );
	
	for( int x=0; x<kGLMUserClipPlanes; x++)
		m_ctx->m_ClipPlaneEquation.ReadIndex( &gl.m_ClipPlaneEquation[x], x, 0 );

	m_ctx->m_ColorMaskSingle.Read( &gl.m_ColorMaskSingle, 0 );
	
	m_ctx->m_BlendEnable.Read( &gl.m_BlendEnable, 0 );
	m_ctx->m_BlendFactor.Read( &gl.m_BlendFactor, 0 );
	m_ctx->m_BlendEnableSRGB.Read( &gl.m_BlendEnableSRGB, 0 );
	m_ctx->m_DepthTestEnable.Read( &gl.m_DepthTestEnable, 0 );
	m_ctx->m_DepthFunc.Read( &gl.m_DepthFunc, 0 );
	m_ctx->m_DepthMask.Read( &gl.m_DepthMask, 0 );
	m_ctx->m_StencilTestEnable.Read( &gl.m_StencilTestEnable, 0 );
	m_ctx->m_StencilFunc.Read( &gl.m_StencilFunc, 0 );

	m_ctx->m_StencilOp.ReadIndex( &gl.m_StencilOp, 0, 0 );
	m_ctx->m_StencilOp.ReadIndex( &gl.m_StencilOp, 1, 0 );

	m_ctx->m_StencilWriteMask.Read( &gl.m_StencilWriteMask, 0 );
	m_ctx->m_ClearColor.Read( &gl.m_ClearColor, 0 );
	m_ctx->m_ClearDepth.Read( &gl.m_ClearDepth, 0 );
	m_ctx->m_ClearStencil.Read( &gl.m_ClearStencil, 0 );
}

void IDirect3DDevice9::FullFlushStates()
{
	m_ctx->WriteAlphaTestEnable( &gl.m_AlphaTestEnable );
	m_ctx->WriteAlphaTestFunc( &gl.m_AlphaTestFunc );
	m_ctx->WriteCullFaceEnable( &gl.m_CullFaceEnable );
	m_ctx->WriteDepthBias( &gl.m_DepthBias );
	m_ctx->WriteScissorEnable( &gl.m_ScissorEnable );
	m_ctx->WriteScissorBox( &gl.m_ScissorBox );
	m_ctx->WriteViewportBox( &gl.m_ViewportBox );
	m_ctx->WriteViewportDepthRange( &gl.m_ViewportDepthRange );

	for( int x=0; x<kGLMUserClipPlanes; x++)
		m_ctx->WriteClipPlaneEnable( &gl.m_ClipPlaneEnable[x], x );
	
	m_ctx->WritePolygonMode( &gl.m_PolygonMode );
	m_ctx->WriteCullFrontFace( &gl.m_CullFrontFace );
	m_ctx->WriteAlphaToCoverageEnable( &gl.m_AlphaToCoverageEnable );
	m_ctx->WriteBlendEquation( &gl.m_BlendEquation );
	m_ctx->WriteBlendColor( &gl.m_BlendColor );
	FlushClipPlaneEquation();
	m_ctx->WriteColorMaskSingle( &gl.m_ColorMaskSingle );

	m_ctx->WriteBlendEnable( &gl.m_BlendEnable );
	m_ctx->WriteBlendFactor( &gl.m_BlendFactor );
	m_ctx->WriteBlendEnableSRGB( &gl.m_BlendEnableSRGB );
	m_ctx->WriteDepthTestEnable( &gl.m_DepthTestEnable );
	m_ctx->WriteDepthFunc( &gl.m_DepthFunc );
	m_ctx->WriteDepthMask( &gl.m_DepthMask );
	m_ctx->WriteStencilTestEnable( &gl.m_StencilTestEnable );
	m_ctx->WriteStencilFunc( &gl.m_StencilFunc );

	m_ctx->WriteStencilOp( &gl.m_StencilOp,0 );
	m_ctx->WriteStencilOp( &gl.m_StencilOp,1 );		// ********* need to recheck this
	
	m_ctx->WriteStencilWriteMask( &gl.m_StencilWriteMask );
	m_ctx->WriteClearColor( &gl.m_ClearColor );
	m_ctx->WriteClearDepth( &gl.m_ClearDepth );
	m_ctx->WriteClearStencil( &gl.m_ClearStencil );
}

HRESULT IDirect3DDevice9::DrawPrimitive(D3DPRIMITIVETYPE PrimitiveType,UINT StartVertex,UINT PrimitiveCount)
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( this );
	DXABSTRACT_BREAK_ON_ERROR();
	return S_OK;
}

// 7LS - TODO
#ifndef DX_TO_GL_ABSTRACTION
HRESULT IDirect3DDevice9::DrawPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType,UINT PrimitiveCountx,CONST void* pVertexStreamZeroData,UINT VertexStreamZeroStride)
{
// 7LS
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( this );
	DXABSTRACT_BREAK_ON_ERROR();
	return S_OK;
}
#endif

//	Type
//	[in] Member of the D3DPRIMITIVETYPE enumerated type, describing the type of primitive to render. D3DPT_POINTLIST is not supported with this method. See Remarks.

//	BaseVertexIndex
//	[in] Offset from the start of the vertex buffer to the first vertex. See Scenario 4.

//	MinIndex
//	[in] Minimum vertex index for vertices used during this call. This is a zero based index relative to BaseVertexIndex.

//	NumVertices
//	[in] Number of vertices used during this call. The first vertex is located at index: BaseVertexIndex + MinIndex.

//	StartIndex
//	[in] Index of the first index to use when accessing the index buffer.

//	PrimitiveCount
//	[in] Number of primitives to render. The number of vertices used is a function of the primitive count and the primitive type. The maximum number of primitives allowed is determined by checking the MaxPrimitiveCount member of the D3DCAPS9 structure.

// BE VERY CAREFUL what you do in this function. It's extremely hot, and calling the wrong GL API's in here will crush perf. on NVidia threaded drivers.
#ifndef OSX

HRESULT IDirect3DDevice9::DrawIndexedPrimitive( D3DPRIMITIVETYPE Type, INT BaseVertexIndex, UINT MinVertexIndex, UINT NumVertices, UINT startIndex, UINT primCount )
{
	tmZone( TELEMETRY_LEVEL2, TMZF_NONE, "%s", __FUNCTION__ );
	Assert( m_ctx->m_nCurOwnerThreadId == ThreadGetCurrentId() );
		
	TOGL_NULL_DEVICE_CHECK;
	if ( m_bFBODirty )
	{
		UpdateBoundFBO();
	}

	g_nTotalDrawsOrClears++;

#if GL_BATCH_PERF_ANALYSIS
	m_nTotalPrims += primCount;
	CFastTimer tm;
	CFlushDrawStatesStats& flushStats = m_ctx->m_FlushStats;
	tm.Start();
	flushStats.Clear();
#endif

#if GLMDEBUG
	if ( gl.m_FogEnable )
	{
		GLMPRINTF(("-D- IDirect3DDevice9::DrawIndexedPrimitive is seeing enabled fog..."));
	}
#endif

	if ( ( !m_indices.m_idxBuffer ) || ( !m_vertexShader ) )
		goto draw_failed;
	
	{
		GL_BATCH_PERF_CALL_TIMER;
								
		m_ctx->FlushDrawStates( MinVertexIndex, MinVertexIndex + NumVertices - 1, BaseVertexIndex );

		{
#if !GL_TELEMETRY_ZONES && GL_BATCH_TELEMETRY_ZONES
			tmZone( TELEMETRY_LEVEL2, TMZF_NONE, "glDrawRangeElements %u", primCount );
#endif
			Assert( ( D3DPT_LINELIST == 2 ) && ( D3DPT_TRIANGLELIST == 4 ) && ( D3DPT_TRIANGLESTRIP == 5 ) );

			static const struct prim_t
			{
				GLenum m_nType;
				uint m_nPrimMul;
				uint m_nPrimAdd;
			} s_primTypes[6] = 
			{ 
				{ 0, 0, 0 },				// 0
				{ 0, 0, 0 },				// 1
				{ GL_LINES, 2, 0 },			// 2 D3DPT_LINELIST
				{ 0, 0, 0 },				// 3 
				{ GL_TRIANGLES, 3, 0 },		// 4 D3DPT_TRIANGLELIST
				{ GL_TRIANGLE_STRIP, 1, 2 }	// 5 D3DPT_TRIANGLESTRIP
			};

			if ( Type <= D3DPT_TRIANGLESTRIP )	
			{
				const prim_t& p = s_primTypes[Type];
				Assert( p.m_nType );
				Assert( NumVertices >= 1 );

				m_ctx->DrawRangeElements( p.m_nType, (GLuint)MinVertexIndex, (GLuint)( MinVertexIndex + NumVertices - 1 ), (GLsizei)p.m_nPrimAdd + primCount * p.m_nPrimMul, (GLenum)GL_UNSIGNED_SHORT, (const GLvoid *)( startIndex * sizeof(short) ), BaseVertexIndex, m_indices.m_idxBuffer->m_idxBuffer );
			}
		}
	}

#if GL_BATCH_PERF_ANALYSIS
	if ( s_rdtsc_to_ms == 0.0f )
	{
		TmU64 t0 = Plat_Rdtsc();
		double d0 = Plat_FloatTime();

		ThreadSleep( 1000 );

		TmU64 t1 = Plat_Rdtsc();
		double d1 = Plat_FloatTime();

		s_rdtsc_to_ms = ( 1000.0f * ( d1 - d0 ) ) / ( t1 - t0 );
	}

#if GL_BATCH_PERF_ANALYSIS_WRITE_PNGS
	if ( m_pBatch_vis_bitmap && m_pBatch_vis_bitmap->is_valid() )
	{
		double t = tm.GetDurationInProgress().GetMillisecondsF();

		uint h = 1;
		if ( gl_batch_vis_y_scale.GetFloat() > 0.0f)
		{
			h = ceil( t / gl_batch_vis_y_scale.GetFloat() );
			h = MAX(h, 1);
		}

		// Total time spent inside any and all our "D3D9" calls
		double flTotalD3DTime = g_nTotalD3DCycles * s_rdtsc_to_ms;
		m_pBatch_vis_bitmap->fill_box(0, m_nBatchVisY, (uint)(.5f + flTotalD3DTime / gl_batch_vis_abs_scale.GetFloat() * m_pBatch_vis_bitmap->width()), h, 150, 150, 150);

		// Total total spent processing just DrawIndexedPrimitive() for this batch.
		m_pBatch_vis_bitmap->fill_box(0, m_nBatchVisY, (uint)(.5f + t / gl_batch_vis_abs_scale.GetFloat() * m_pBatch_vis_bitmap->width()), h, 70, 70, 70);
		
		double flTotalGLMS = gGL->m_nTotalGLCycles * s_rdtsc_to_ms;
		
		// Total time spent inside of all OpenGL calls
		m_pBatch_vis_bitmap->additive_fill_box(0, m_nBatchVisY, (uint)(.5f + flTotalGLMS / gl_batch_vis_abs_scale.GetFloat() * m_pBatch_vis_bitmap->width()), h, 0, 0, 64);
								
		if (flushStats.m_nNewVS) m_pBatch_vis_bitmap->additive_fill_box(80-16, m_nBatchVisY, 8, h, 0, 110, 0);
		if (flushStats.m_nNewPS) m_pBatch_vis_bitmap->additive_fill_box(80-8, m_nBatchVisY, 8, h, 110, 0, 110);
		
		int lm = 80;
		m_pBatch_vis_bitmap->fill_box(lm+0+flushStats.m_nFirstVSConstant, m_nBatchVisY, flushStats.m_nNumVSConstants, h, 64, 255, 255);
		m_pBatch_vis_bitmap->fill_box(lm+64, m_nBatchVisY, flushStats.m_nNumVSBoneConstants, h, 255, 64, 64);
		m_pBatch_vis_bitmap->fill_box(lm+64+256+flushStats.m_nFirstPSConstant, m_nBatchVisY, flushStats.m_nNumPSConstants, h, 64, 64, 255);

		m_pBatch_vis_bitmap->fill_box(lm+64+256+32, m_nBatchVisY, flushStats.m_nNumChangedSamplers, h, 255, 255, 255);
		m_pBatch_vis_bitmap->fill_box(lm+64+256+32+16, m_nBatchVisY, flushStats.m_nNumSamplingParamsChanged, h, 92, 128, 255);

		if ( flushStats.m_nVertexBufferChanged ) m_pBatch_vis_bitmap->fill_box(lm+64+256+32+16+64, m_nBatchVisY, 16, h, 128, 128, 128);
		if ( flushStats.m_nIndexBufferChanged ) m_pBatch_vis_bitmap->fill_box(lm+64+256+32+16+64+16, m_nBatchVisY, 16, h, 128, 128, 255);

		m_pBatch_vis_bitmap->fill_box(lm+64+256+32+16+64+16+16, m_nBatchVisY, ( ( g_nTotalVBLockBytes + g_nTotalIBLockBytes ) * 64 + 2047 ) / 2048, h, 120, 120, 120 );
		m_pBatch_vis_bitmap->additive_fill_box(lm+64+256+32+16+64+16+16, m_nBatchVisY, ( g_nTotalVBLockBytes * 64 + 2047 ) / 2048, h, 120, 0, 0);

		m_nBatchVisY += h;
	}
#endif
	
	m_nNumProgramChanges += ((flushStats.m_nNewVS + flushStats.m_nNewPS) != 0);

	m_flTotalD3DTime += g_nTotalD3DCycles * s_rdtsc_to_ms;
	m_nTotalD3DCalls += g_nTotalD3DCalls;
	g_nTotalD3DCycles = 0;
	g_nTotalD3DCalls = 0;

	m_flTotalGLTime += gGL->m_nTotalGLCycles * s_rdtsc_to_ms;
	m_nTotalGLCalls += gGL->m_nTotalGLCalls;
	gGL->m_nTotalGLCycles = 0;
	gGL->m_nTotalGLCalls = 0; 

	g_nTotalVBLockBytes = 0;
	g_nTotalIBLockBytes = 0;
#endif

	return S_OK;

draw_failed:
	Assert( 0 );
	return E_FAIL;
}

#else

// OSX 10.6 support

HRESULT IDirect3DDevice9::FlushIndexBindings( void )
{
	// push index buffer state
	m_ctx->SetIndexBuffer( m_indices.m_idxBuffer->m_idxBuffer );
	return S_OK;
}

HRESULT IDirect3DDevice9::FlushVertexBindings( uint baseVertexIndex )
{
	// push vertex buffer state for the current vertex decl
	// in this variant we just walk the attrib map in the VS and do a pull for each one.
	// if we can't find a match in the vertex decl, we may fall back to the secret 'dummy' VBO that GLM maintains

	GLMVertexSetup	setup;
	memset( &setup, 0, sizeof( setup ) );

	IDirect3DVertexDeclaration9 *vxdecl = m_pVertDecl;
	unsigned char *vshAttribMap = m_vertexShader->m_vtxAttribMap;

	// this loop could be tightened if we knew the number of live entries in the shader attrib map.
	// which of course would be easy to do in the create shader function or even in the translator.

	GLMVertexAttributeDesc *dstAttr = setup.m_attrs;
	for( int i=0; i<16; i++,dstAttr++ )
	{
		unsigned char vshattrib = vshAttribMap[ i ];
		if (vshattrib != 0xBB)
		{
			// try to find the match in the decl.
			// idea: put some inverse table in the decl which could accelerate this search.

			D3DVERTEXELEMENT9_GL *elem = m_pVertDecl->m_elements;
			for( int j=0; j< m_pVertDecl->m_elemCount; j++,elem++)
			{
				// if it matches, install it, change vshattrib so the code below does not trigger, then end the loop
				if ( ((vshattrib>>4) == elem->m_dxdecl.Usage) && ((vshattrib & 0x0F) == elem->m_dxdecl.UsageIndex) )
				{
					// targeting attribute #i in the setup with element data #j from the decl

					*dstAttr = elem->m_gldecl;

					// then fix buffer, stride, offset - note that we honor the base vertex index here by fiddling the offset
					int streamIndex = elem->m_dxdecl.Stream;
					dstAttr->m_pBuffer = m_streams[ streamIndex ].m_vtxBuffer->m_vtxBuffer;
					dstAttr->m_stride = m_streams[ streamIndex ].m_stride;
					dstAttr->m_offset += m_streams[ streamIndex ].m_offset + (baseVertexIndex * dstAttr->m_stride); 

					// set mask
					setup.m_attrMask |= (1 << i);

					// end loop
					vshattrib = 0xBB;
					j = 999;
				}
			}

			// if vshattrib is not 0xBB here, that means we could not find a source in the decl for it
			if (vshattrib != 0xBB)
			{
				// fill out attr the same way as usual, we just pass NULL for the buffer and ask GLM to have mercy on us

				dstAttr->m_pBuffer = NULL;
				dstAttr->m_stride = 0;
				dstAttr->m_offset = 0;

				// only implement certain usages... if we haven't seen it before, stop.
				switch (vshattrib >> 4)	// aka usage
				{
				case	D3DDECLUSAGE_POSITION:
				case	D3DDECLUSAGE_BLENDWEIGHT:
				case	D3DDECLUSAGE_BLENDINDICES:
					Debugger();
					break;

				case	D3DDECLUSAGE_NORMAL:
					dstAttr->m_nCompCount = 3;
					dstAttr->m_datatype = GL_FLOAT;
					dstAttr->m_normalized = false;
					break;

				case	D3DDECLUSAGE_PSIZE:
					Debugger();
					break;

				case	D3DDECLUSAGE_TEXCOORD:
					dstAttr->m_nCompCount = 3;
					dstAttr->m_datatype = GL_FLOAT;
					dstAttr->m_normalized = false;
					break;

				case	D3DDECLUSAGE_TANGENT:
				case	D3DDECLUSAGE_BINORMAL:
				case	D3DDECLUSAGE_TESSFACTOR:
				case	D3DDECLUSAGE_PLUGH:
					Debugger();
					break;

				case	D3DDECLUSAGE_COLOR:
					dstAttr->m_nCompCount = 4;
					dstAttr->m_datatype = GL_UNSIGNED_BYTE;
					dstAttr->m_normalized = true;
					break;

				case	D3DDECLUSAGE_FOG:
				case	D3DDECLUSAGE_DEPTH:
				case	D3DDECLUSAGE_SAMPLE:
					Debugger();
					break;
				}
			}
		}
	}

	// copy active program's vertex attrib map into the vert setup info
	memcpy(&setup.m_vtxAttribMap, m_vertexShader->m_vtxAttribMap, sizeof(m_vertexShader->m_vtxAttribMap));

	m_ctx->SetVertexAttributes(&setup);
	return S_OK;
}


// OSX path offering support for 10.6 (we do not have support for glDrawRangeElementsBaseVertex)
HRESULT IDirect3DDevice9::DrawIndexedPrimitive( D3DPRIMITIVETYPE Type,INT BaseVertexIndex,UINT MinVertexIndex,UINT NumVertices,UINT startIndex,UINT primCount )
{
	Assert( m_ctx->m_nCurOwnerThreadId == ThreadGetCurrentId() );

	TOGL_NULL_DEVICE_CHECK;
	if ( m_bFBODirty )
	{
		UpdateBoundFBO();
	}

	g_nTotalDrawsOrClears++;

#if GL_BATCH_PERF_ANALYSIS
	m_nTotalPrims += primCount;
	CFastTimer tm;
	CFlushDrawStatesStats& flushStats = m_ctx->m_FlushStats;
	tm.Start();
	flushStats.Clear();
#endif

#if GLMDEBUG
	if ( gl.m_FogEnable )
	{
		GLMPRINTF(("-D- IDirect3DDevice9::DrawIndexedPrimitive is seeing enabled fog..."));
	}
#endif

	if ( ( !m_indices.m_idxBuffer ) || ( !m_vertexShader ) )
		goto draw_failed;    

	this->FlushIndexBindings( );
	this->FlushVertexBindings( BaseVertexIndex );
	m_ctx->FlushDrawStates( MinVertexIndex, MinVertexIndex + NumVertices - 1, 0 );

	if (gl.m_FogEnable)
	{
		GLMPRINTF(("-D- IDirect3DDevice9::DrawIndexedPrimitive is seeing enabled fog..."));
	}

	switch(Type)
	{
	case	D3DPT_POINTLIST:
		Debugger();
		break;

	case	D3DPT_LINELIST:
		GLMPRINTF(("-X- IDirect3DDevice9::DrawIndexedPrimitive( D3DPT_LINELIST ) - ignored."));
		//			Debugger();
		m_ctx->DrawRangeElements( (GLenum)GL_LINES, (GLuint)MinVertexIndex, (GLuint)(MinVertexIndex + NumVertices), (GLsizei)primCount*2, (GLenum)GL_UNSIGNED_SHORT, (const GLvoid *)(startIndex * sizeof(short)), m_indices.m_idxBuffer->m_idxBuffer );
		break;

	case	D3DPT_TRIANGLELIST:
		m_ctx->DrawRangeElements(GL_TRIANGLES, (GLuint)MinVertexIndex, (GLuint)(MinVertexIndex + NumVertices), (GLsizei)primCount*3, (GLenum)GL_UNSIGNED_SHORT, (const GLvoid *)(startIndex * sizeof(short)), m_indices.m_idxBuffer->m_idxBuffer );
		break;

	case D3DPT_TRIANGLESTRIP:
		// enabled... Debugger();
		m_ctx->DrawRangeElements(GL_TRIANGLE_STRIP, (GLuint)MinVertexIndex, (GLuint)(MinVertexIndex + NumVertices), (GLsizei)(2+primCount), (GLenum)GL_UNSIGNED_SHORT, (const GLvoid *)(startIndex * sizeof(short)), m_indices.m_idxBuffer->m_idxBuffer );
		break;
	}

	return S_OK;

draw_failed:
	Assert( 0 );
	return E_FAIL;
}

#endif // #ifndef OSX

HRESULT IDirect3DDevice9::DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType,UINT MinVertexIndex,UINT NumVertices,UINT PrimitiveCount,CONST void* pIndexData,D3DFORMAT IndexDataFormat,CONST void* pVertexStreamZeroData,UINT VertexStreamZeroStride)
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( this );
	
	DXABSTRACT_BREAK_ON_ERROR();
	return S_OK;
}

BOOL IDirect3DDevice9::ShowCursor(BOOL bShow)
{
	// FIXME NOP
	//DXABSTRACT_BREAK_ON_ERROR();
	return TRUE;
}

void	d3drect_to_glmbox( D3DRECT *src, GLScissorBox_t *dst )
{
	// to convert from a d3d rect to a GL rect you have to fix up the vertical axis, since D3D Y=0 is the top, but GL Y=0 is the bottom.
	// you can't fix it without knowing the height.

	dst->width	= src->x2 - src->x1;
	dst->x		= src->x1;				// left edge

	dst->height	= src->y2 - src->y1;
	dst->y		= src->y1;				// bottom edge - take large Y from d3d and subtract from surf height.
}

HRESULT IDirect3DDevice9::Clear(DWORD Count,CONST D3DRECT* pRects,DWORD Flags,D3DCOLOR Color,float Z,DWORD Stencil)
{
	GL_BATCH_PERF_CALL_TIMER;

	if ( m_bFBODirty )
	{
		UpdateBoundFBO();
	}
		
	g_nTotalDrawsOrClears++;

	m_ctx->FlushDrawStatesNoShaders();

	
	//debugging Color = (rand() | 0xFF0000FF) & 0xFF3F3FFF;
	if (!Count)
	{
		// run clear with no added rectangle
		m_ctx->Clear(	(Flags&D3DCLEAR_TARGET)!=0, Color,
						(Flags&D3DCLEAR_ZBUFFER)!=0, Z,
						(Flags&D3DCLEAR_STENCIL)!=0, Stencil,
						NULL
					);
	}
	else
	{
		GLScissorBox_t	tempbox;
		
		// do the rects one by one and convert each one to GL form
		for( uint i=0; i<Count; i++)
		{
			D3DRECT d3dtempbox = pRects[i];
			d3drect_to_glmbox( &d3dtempbox, &tempbox );

			m_ctx->Clear(	(Flags&D3DCLEAR_TARGET)!=0, Color,
							(Flags&D3DCLEAR_ZBUFFER)!=0, Z,
							(Flags&D3DCLEAR_STENCIL)!=0, Stencil,
							&tempbox
						);
		}
	}

	return S_OK;
}

HRESULT IDirect3DDevice9::SetTransform(D3DTRANSFORMSTATETYPE State,CONST D3DMATRIX* pMatrix)
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( this );
	DXABSTRACT_BREAK_ON_ERROR();
	return S_OK;
}

HRESULT IDirect3DDevice9::SetTextureStageState(DWORD Stage,D3DTEXTURESTAGESTATETYPE Type,DWORD Value)
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( this );
	DXABSTRACT_BREAK_ON_ERROR();
	return S_OK;
}

HRESULT IDirect3DDevice9::ValidateDevice(DWORD* pNumPasses)
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( this );
	DXABSTRACT_BREAK_ON_ERROR();
	return S_OK;
}

HRESULT IDirect3DDevice9::SetMaterial(CONST D3DMATERIAL9* pMaterial)
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( this );
	GLMPRINTF(("-X- IDirect3DDevice9::SetMaterial - ignored."));
//	DXABSTRACT_BREAK_ON_ERROR();
	return S_OK;
}


HRESULT IDirect3DDevice9::LightEnable(DWORD Index,BOOL Enable)
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( this );
	DXABSTRACT_BREAK_ON_ERROR();
	return S_OK;
}

HRESULT IDirect3DDevice9::SetScissorRect(CONST RECT* pRect)
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( this );
	//int nSurfaceHeight = m_ctx->m_drawingFBO->m_attach[ kAttColor0 ].m_tex->m_layout->m_key.m_ySize;
	
	GLScissorBox_t newScissorBox = { pRect->left, pRect->top, pRect->right - pRect->left, pRect->bottom - pRect->top };
	gl.m_ScissorBox	= newScissorBox;
	m_ctx->WriteScissorBox( &gl.m_ScissorBox );
	return S_OK;
}

HRESULT IDirect3DDevice9::GetDeviceCaps(D3DCAPS9* pCaps)
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( this );
	
	// "Adapter" is used to index amongst the set of fake-adapters maintained in the display DB
	GLMDisplayDB *db = GetDisplayDB();
	int glmRendererIndex = -1;
	int glmDisplayIndex = -1;

	GLMRendererInfoFields	glmRendererInfo;
	GLMDisplayInfoFields	glmDisplayInfo;

	bool result = db->GetFakeAdapterInfo( m_params.m_adapter, &glmRendererIndex, &glmDisplayIndex, &glmRendererInfo, &glmDisplayInfo ); (void)result;
	Assert (!result);
	// just leave glmRendererInfo filled out for subsequent code to look at as needed.

	FillD3DCaps9( glmRendererInfo, pCaps );

	return S_OK;
}


HRESULT IDirect3DDevice9::TestCooperativeLevel()
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( this );
	// game calls this to see if device was lost.
	// last I checked the device was still attached to the computer.
	// so, return OK.

	return S_OK;
}

HRESULT IDirect3DDevice9::SetClipPlane(DWORD Index,CONST float* pPlane)
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( this );
	Assert(Index<2);

	// We actually push the clip plane coeffs to two places
	// - into a shader param for ARB mode
	// - and into the API defined clip plane slots for GLSL mode.
	
	// if ARB mode... THIS NEEDS TO GO... it's messing up the dirty ranges..
	{
	//	this->SetVertexShaderConstantF( DXABSTRACT_VS_CLIP_PLANE_BASE+Index, pPlane, 1 );	// stash the clip plane values into shader param - translator knows where to look
	}
	
	// if GLSL mode... latch it and let FlushStates push it out
	{
		GLClipPlaneEquation_t	peq;
		peq.x = pPlane[0];
		peq.y = pPlane[1];
		peq.z = pPlane[2];
		peq.w = pPlane[3];

		gl.m_ClipPlaneEquation[ Index ] = peq;
		FlushClipPlaneEquation();

		// m_ctx->WriteClipPlaneEquation( &peq, Index );
	}

	return S_OK;
}

HRESULT IDirect3DDevice9::EvictManagedResources()
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( this );
	GLMPRINTF(("-X- IDirect3DDevice9::EvictManagedResources --> IGNORED"));
	return S_OK;
}

HRESULT IDirect3DDevice9::SetLight(DWORD Index,CONST D3DLIGHT9*)
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( this );
	DXABSTRACT_BREAK_ON_ERROR();
	return S_OK;
}

void IDirect3DDevice9::SetGammaRamp(UINT iSwapChain,DWORD Flags,CONST D3DGAMMARAMP* pRamp)
{
	GL_BATCH_PERF_CALL_TIMER;
	Assert( GetCurrentOwnerThreadId() == ThreadGetCurrentId() );
	
	if ( g_pLauncherMgr )
	{
		g_pLauncherMgr->SetGammaRamp( pRamp->red, pRamp->green, pRamp->blue );
	}
}

void TOGLMETHODCALLTYPE IDirect3DDevice9::SaveGLState()
{
}

void TOGLMETHODCALLTYPE IDirect3DDevice9::RestoreGLState()
{
	m_ctx->ForceFlushStates();

	m_bFBODirty = true;
}

void IDirect3DDevice9::AcquireThreadOwnership( )
{
	GL_BATCH_PERF_CALL_TIMER;
	m_ctx->MakeCurrent( true );
}


void IDirect3DDevice9::ReleaseThreadOwnership( )
{
	GL_BATCH_PERF_CALL_TIMER;
	m_ctx->ReleaseCurrent( true );
}

void IDirect3DDevice9::SetMaxUsedVertexShaderConstantsHintNonInline( uint nMaxReg )
{
	GL_BATCH_PERF_CALL_TIMER;
	m_ctx->SetMaxUsedVertexShaderConstantsHint( nMaxReg );
}

HRESULT IDirect3DDevice9::SetRenderState( D3DRENDERSTATETYPE State, DWORD Value )
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( this );
	TOGL_NULL_DEVICE_CHECK;

#if GLMDEBUG
	// None of this is needed normally.
	char	rsSpew = 1;
	char	ignored = 0;

	if (State >= D3DRS_VALUE_LIMIT)
	{
		DXABSTRACT_BREAK_ON_ERROR();		// bad
		return S_OK;
	}

	const D3D_RSINFO *info = &g_D3DRS_INFO_unpacked[ State ];
	if (info->m_state != State)
	{
		DXABSTRACT_BREAK_ON_ERROR();	// bad - we never set up that state in our list
		return S_OK;
	}

	if (rsSpew)
	{
		GLMPRINTF(("-X- IDirect3DDevice9::SetRenderState: set %s(%d) to %d(0x%08x) ( class %d, defval is %d(0x%08x) )", GLMDecode( eD3D_RSTATE,State),State, Value,Value, info->m_class, info->m_defval,info->m_defval ));
	}

	switch( info->m_class )
	{
		case 0:		// just ignore quietly. example: D3DRS_LIGHTING
			ignored = 1;
			break;

		case 1:
		{
			// no GL response - and no error as long as the write value matches the default
			if (Value != info->m_defval)
			{
				static char stop_here_1 = 0;
				if (stop_here_1)
					DXABSTRACT_BREAK_ON_ERROR();
			}
			break;
		}

		case 2:
		{
			// provide GL response, but only support known default value
			if (Value != info->m_defval)
			{
				static char stop_here_2 = 0;
				if (stop_here_2)
					DXABSTRACT_BREAK_ON_ERROR();
			}
			// fall through to mode 3
		}
		case 3:
			// normal case - the switch statement below will handle this
			break;
	}
#endif

	switch (State)
	{
		case D3DRS_ZENABLE:				// kGLDepthTestEnable
		{
			gl.m_DepthTestEnable.enable = Value;
			m_ctx->WriteDepthTestEnable( &gl.m_DepthTestEnable );
			break;
		}
		case D3DRS_ZWRITEENABLE:			// kGLDepthMask
		{
			gl.m_DepthMask.mask = Value;
			m_ctx->WriteDepthMask( &gl.m_DepthMask );
			break;
		}
		case D3DRS_ZFUNC:	
		{
			// kGLDepthFunc
			GLenum func = D3DCompareFuncToGL( Value );
			gl.m_DepthFunc.func = func;
			m_ctx->WriteDepthFunc( &gl.m_DepthFunc );
			break;
		}

		case D3DRS_COLORWRITEENABLE:		// kGLColorMaskSingle
		{
			gl.m_ColorMaskSingle.r	=	((Value & D3DCOLORWRITEENABLE_RED)  != 0) ? 0xFF : 0x00;
			gl.m_ColorMaskSingle.g	=	((Value & D3DCOLORWRITEENABLE_GREEN)!= 0) ? 0xFF : 0x00;	
			gl.m_ColorMaskSingle.b	=	((Value & D3DCOLORWRITEENABLE_BLUE) != 0) ? 0xFF : 0x00;
			gl.m_ColorMaskSingle.a	=	((Value & D3DCOLORWRITEENABLE_ALPHA)!= 0) ? 0xFF : 0x00;
			m_ctx->WriteColorMaskSingle( &gl.m_ColorMaskSingle );
			break;
		}

		case D3DRS_CULLMODE:				// kGLCullFaceEnable / kGLCullFrontFace
		{
			switch (Value)
			{
				case D3DCULL_NONE:
				{
					gl.m_CullFaceEnable.enable = false;
					gl.m_CullFrontFace.value = GL_CCW;	//doesn't matter																

					m_ctx->WriteCullFaceEnable( &gl.m_CullFaceEnable );
					m_ctx->WriteCullFrontFace( &gl.m_CullFrontFace );
					break;
				}
					
				case D3DCULL_CW:
				{
					gl.m_CullFaceEnable.enable = true;
					gl.m_CullFrontFace.value = GL_CW;	//origGL_CCW;

					m_ctx->WriteCullFaceEnable( &gl.m_CullFaceEnable );
					m_ctx->WriteCullFrontFace( &gl.m_CullFrontFace );
					break;
				}
				case D3DCULL_CCW:
				{
					gl.m_CullFaceEnable.enable = true;
					gl.m_CullFrontFace.value = GL_CCW;	//origGL_CW;

					m_ctx->WriteCullFaceEnable( &gl.m_CullFaceEnable );
					m_ctx->WriteCullFrontFace( &gl.m_CullFrontFace );
					break;
				}
				default:	
				{
					DXABSTRACT_BREAK_ON_ERROR();	
					break;
				}
			}
			break;
		}

		//-------------------------------------------------------------------------------------------- alphablend stuff

		case D3DRS_ALPHABLENDENABLE:		// kGLBlendEnable
		{
			gl.m_BlendEnable.enable = Value;
			m_ctx->WriteBlendEnable( &gl.m_BlendEnable );
			break;
		}

		case D3DRS_BLENDOP:				// kGLBlendEquation				// D3D blend-op ==> GL blend equation
		{
			GLenum	equation = D3DBlendOperationToGL( Value );
			gl.m_BlendEquation.equation = equation;
			m_ctx->WriteBlendEquation( &gl.m_BlendEquation );
			break;
		}

		case D3DRS_SRCBLEND:				// kGLBlendFactor				// D3D blend-factor ==> GL blend factor
		case D3DRS_DESTBLEND:			// kGLBlendFactor
		{
			GLenum	factor = D3DBlendFactorToGL( Value );

			if (State==D3DRS_SRCBLEND)
			{
				gl.m_BlendFactor.srcfactor = factor;
			}
			else
			{
				gl.m_BlendFactor.dstfactor = factor;
			}
			m_ctx->WriteBlendFactor( &gl.m_BlendFactor );
			break;
		}

		case D3DRS_SRGBWRITEENABLE:			// kGLBlendEnableSRGB
		{
			gl.m_BlendEnableSRGB.enable = Value;
			m_ctx->WriteBlendEnableSRGB( &gl.m_BlendEnableSRGB );
			break;					
		}

		//-------------------------------------------------------------------------------------------- alphatest stuff

		case D3DRS_ALPHATESTENABLE:
		{
			gl.m_AlphaTestEnable.enable = Value;
			m_ctx->WriteAlphaTestEnable( &gl.m_AlphaTestEnable );
			break;
		}

		case D3DRS_ALPHAREF:
		{
			gl.m_AlphaTestFunc.ref = Value / 255.0f;
			m_ctx->WriteAlphaTestFunc( &gl.m_AlphaTestFunc );
			break;
		}

		case D3DRS_ALPHAFUNC:
		{
			GLenum func = D3DCompareFuncToGL( Value );;
			gl.m_AlphaTestFunc.func = func;
			m_ctx->WriteAlphaTestFunc( &gl.m_AlphaTestFunc );
			break;
		}

		//-------------------------------------------------------------------------------------------- stencil stuff

		case D3DRS_STENCILENABLE:		// GLStencilTestEnable_t
		{
			gl.m_StencilTestEnable.enable = Value;
			m_ctx->WriteStencilTestEnable( &gl.m_StencilTestEnable );
			break;
		}

		case D3DRS_STENCILFAIL:			// GLStencilOp_t		"what do you do if stencil test fails"
		{
			GLenum stencilop = D3DStencilOpToGL( Value );
			gl.m_StencilOp.sfail = stencilop;

			m_ctx->WriteStencilOp( &gl.m_StencilOp,0 );
			m_ctx->WriteStencilOp( &gl.m_StencilOp,1 );		// ********* need to recheck this
			break;
		}

		case D3DRS_STENCILZFAIL:			// GLStencilOp_t		"what do you do if stencil test passes *but* depth test fails, if depth test happened"
		{
			GLenum stencilop = D3DStencilOpToGL( Value );
			gl.m_StencilOp.dpfail = stencilop;

			m_ctx->WriteStencilOp( &gl.m_StencilOp,0 );
			m_ctx->WriteStencilOp( &gl.m_StencilOp,1 );		// ********* need to recheck this
			break;
		}

		case D3DRS_STENCILPASS:			// GLStencilOp_t		"what do you do if stencil test and depth test both pass"
		{
			GLenum stencilop = D3DStencilOpToGL( Value );
			gl.m_StencilOp.dppass = stencilop;

			m_ctx->WriteStencilOp( &gl.m_StencilOp,0 );
			m_ctx->WriteStencilOp( &gl.m_StencilOp,1 );		// ********* need to recheck this
			break;
		}

		case D3DRS_STENCILFUNC:			// GLStencilFunc_t
		{
			GLenum stencilfunc = D3DCompareFuncToGL( Value );
			gl.m_StencilFunc.frontfunc = gl.m_StencilFunc.backfunc = stencilfunc;

			m_ctx->WriteStencilFunc( &gl.m_StencilFunc );
			break;
		}

		case D3DRS_STENCILREF:			// GLStencilFunc_t
		{
			gl.m_StencilFunc.ref = Value;
			m_ctx->WriteStencilFunc( &gl.m_StencilFunc );
			break;
		}

		case D3DRS_STENCILMASK:			// GLStencilFunc_t
		{
			gl.m_StencilFunc.mask = Value;
			m_ctx->WriteStencilFunc( &gl.m_StencilFunc );
			break;
		}

		case D3DRS_STENCILWRITEMASK:		// GLStencilWriteMask_t
		{
			gl.m_StencilWriteMask.mask = Value;
			m_ctx->WriteStencilWriteMask( &gl.m_StencilWriteMask );
			break;
		}

		case D3DRS_FOGENABLE:			// none of these are implemented yet... erk
		{
			gl.m_FogEnable = (Value != 0);
			GLMPRINTF(("-D- fogenable = %d",Value ));
			break;
		}
		
		case D3DRS_SCISSORTESTENABLE:	// kGLScissorEnable
		{
			gl.m_ScissorEnable.enable = Value;
			m_ctx->WriteScissorEnable( &gl.m_ScissorEnable );
			break;
		}

		case D3DRS_DEPTHBIAS:			// kGLDepthBias
		{
			// the value in the dword is actually a float
			float	fvalue = *(float*)&Value;
			gl.m_DepthBias.units = fvalue;

			m_ctx->WriteDepthBias( &gl.m_DepthBias );
			break;
		}

		// good ref on these: http://aras-p.info/blog/2008/06/12/depth-bias-and-the-power-of-deceiving-yourself/
		case D3DRS_SLOPESCALEDEPTHBIAS:
		{
			// the value in the dword is actually a float
			float	fvalue = *(float*)&Value;
			gl.m_DepthBias.factor = fvalue;

			m_ctx->WriteDepthBias( &gl.m_DepthBias );
			break;
		}

		// Alpha to coverage
		case D3DRS_ADAPTIVETESS_Y:
		{
			gl.m_AlphaToCoverageEnable.enable = Value;
			m_ctx->WriteAlphaToCoverageEnable( &gl.m_AlphaToCoverageEnable );
			break;
		}

		case D3DRS_CLIPPLANEENABLE:		// kGLClipPlaneEnable
		{
			// d3d packs all the enables into one word.
			// we break that out so we don't do N glEnable calls to sync - 
			// GLM is tracking one unique enable per plane.
			for( int i=0; i<kGLMUserClipPlanes; i++)
			{
				gl.m_ClipPlaneEnable[i].enable = (Value & (1<<i)) != 0;
			}

			for( int x=0; x<kGLMUserClipPlanes; x++)
				m_ctx->WriteClipPlaneEnable( &gl.m_ClipPlaneEnable[x], x );
			break;
		}

		//-------------------------------------------------------------------------------------------- polygon/fill mode

		case D3DRS_FILLMODE:
		{
			GLuint mode = 0;
			switch(Value)
			{
				case D3DFILL_POINT:			mode = GL_POINT; break;
				case D3DFILL_WIREFRAME:		mode = GL_LINE; break;
				case D3DFILL_SOLID:			mode = GL_FILL; break;
				default:					DXABSTRACT_BREAK_ON_ERROR(); break;
			}
			gl.m_PolygonMode.values[0] = gl.m_PolygonMode.values[1] = mode;						
			m_ctx->WritePolygonMode( &gl.m_PolygonMode );
			break;
		}

#if GLMDEBUG					
		case D3DRS_MULTISAMPLEANTIALIAS:
		case D3DRS_MULTISAMPLEMASK:
		case D3DRS_FOGCOLOR:
		case D3DRS_FOGTABLEMODE:
		case D3DRS_FOGSTART:
		case D3DRS_FOGEND:
		case D3DRS_FOGDENSITY:
		case D3DRS_RANGEFOGENABLE:
		case D3DRS_FOGVERTEXMODE:
		case D3DRS_COLORWRITEENABLE1:	// kGLColorMaskMultiple
		case D3DRS_COLORWRITEENABLE2:	// kGLColorMaskMultiple
		case D3DRS_COLORWRITEENABLE3:	// kGLColorMaskMultiple
		case D3DRS_SEPARATEALPHABLENDENABLE:
		case D3DRS_BLENDOPALPHA:
		case D3DRS_SRCBLENDALPHA:
		case D3DRS_DESTBLENDALPHA:
		case D3DRS_TWOSIDEDSTENCILMODE:	// -> GL_STENCIL_TEST_TWO_SIDE_EXT... not yet implemented ?
		case D3DRS_CCW_STENCILFAIL:		// GLStencilOp_t
		case D3DRS_CCW_STENCILZFAIL:		// GLStencilOp_t
		case D3DRS_CCW_STENCILPASS:		// GLStencilOp_t
		case D3DRS_CCW_STENCILFUNC:		// GLStencilFunc_t
		case D3DRS_CLIPPING:				// ???? is clipping ever turned off ??
		case D3DRS_LASTPIXEL:
		case D3DRS_DITHERENABLE:
		case D3DRS_SHADEMODE:
		default:
			ignored = 1;
			break;
#endif
	}

#if GLMDEBUG
	if (rsSpew && ignored)
	{
		GLMPRINTF(("-X-  (ignored)"));
	}
#endif

	return S_OK;
}

HRESULT IDirect3DDevice9::SetSamplerStateNonInline( DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD Value )
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( this );
		
	Assert( Sampler < GLM_SAMPLER_COUNT );

	m_ctx->SetSamplerDirty( Sampler );

	switch( Type )
	{
	case D3DSAMP_ADDRESSU:
		m_ctx->SetSamplerAddressU( Sampler, Value );
		break;
	case D3DSAMP_ADDRESSV:
		m_ctx->SetSamplerAddressV( Sampler, Value );
		break;
	case D3DSAMP_ADDRESSW:
		m_ctx->SetSamplerAddressW( Sampler, Value );
		break;
	case D3DSAMP_BORDERCOLOR:
		m_ctx->SetSamplerBorderColor( Sampler, Value );
		break;
	case D3DSAMP_MAGFILTER:
		m_ctx->SetSamplerMagFilter( Sampler, Value );
		break;
	case D3DSAMP_MIPFILTER:	
		m_ctx->SetSamplerMipFilter( Sampler, Value );
		break;
	case D3DSAMP_MINFILTER:	
		m_ctx->SetSamplerMinFilter( Sampler, Value );
		break;
	case D3DSAMP_MIPMAPLODBIAS: 
		m_ctx->SetSamplerMipMapLODBias( Sampler, Value );
		break;		
	case D3DSAMP_MAXMIPLEVEL: 
		m_ctx->SetSamplerMaxMipLevel( Sampler, Value);
		break;
	case D3DSAMP_MAXANISOTROPY: 
		m_ctx->SetSamplerMaxAnisotropy( Sampler, Value);
		break;
	case D3DSAMP_SRGBTEXTURE: 
		//m_samplers[ Sampler ].m_srgb = Value;
		m_ctx->SetSamplerSRGBTexture(Sampler, Value);
		break;
	case D3DSAMP_SHADOWFILTER: 
		m_ctx->SetShadowFilter(Sampler, Value);
		break;

	default: DXABSTRACT_BREAK_ON_ERROR(); break;
	}

	return S_OK;
}

void IDirect3DDevice9::SetSamplerStatesNonInline(
	DWORD Sampler, DWORD AddressU, DWORD AddressV, DWORD AddressW,
	DWORD MinFilter, DWORD MagFilter, DWORD MipFilter, 
	DWORD MinLod, float LodBias)
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( this );
		
	Assert( Sampler < GLM_SAMPLER_COUNT);

	m_ctx->SetSamplerDirty( Sampler );

	m_ctx->SetSamplerStates( Sampler, AddressU, AddressV, AddressW, MinFilter, MagFilter, MipFilter, MinLod, LodBias );
}

HRESULT IDirect3DDevice9::SetTextureNonInline(DWORD Stage,IDirect3DBaseTexture9* pTexture)
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( this );
	Assert( Stage < GLM_SAMPLER_COUNT );
	m_textures[Stage] = pTexture;
	m_ctx->SetSamplerTex( Stage, pTexture ? pTexture->m_tex : NULL );
	return S_OK;
}

// ------------------------------------------------------------------------------------------------------------------------------ //

void* ID3DXBuffer::GetBufferPointer()
{
	GL_BATCH_PERF_CALL_TIMER;
	DXABSTRACT_BREAK_ON_ERROR();
	return NULL;
}

DWORD ID3DXBuffer::GetBufferSize()
{
	GL_BATCH_PERF_CALL_TIMER;
	DXABSTRACT_BREAK_ON_ERROR();
	return 0;
}



#ifndef _MSC_VER
#pragma mark ----- More D3DX stuff
#endif

// ------------------------------------------------------------------------------------------------------------------------------ //
// D3DX stuff.
// ------------------------------------------------------------------------------------------------------------------------------ //

// matrix stack...

HRESULT D3DXCreateMatrixStack( DWORD Flags, LPD3DXMATRIXSTACK* ppStack)
{
	
	*ppStack = new ID3DXMatrixStack;
	
	(*ppStack)->Create();
	
	return S_OK;
}

ID3DXMatrixStack::ID3DXMatrixStack( void )
{
	m_refcount[0] = 1;
	m_refcount[1] = 0;
};
		
void ID3DXMatrixStack::AddRef( int which, char *comment )
{
	Assert( which >= 0 );
	Assert( which < 2 );
	m_refcount[which]++;
};
		
ULONG ID3DXMatrixStack::Release( int which, char *comment )
{
	Assert( which >= 0 );
	Assert( which < 2 );
			
	bool deleting = false;
			
	m_refcount[which]--;
	if ( (!m_refcount[0]) && (!m_refcount[1]) )
	{
		deleting = true;
	}
			
	if (deleting)
	{
		if (m_mark)
		{
			GLMPRINTF((""))	;		// place to hang a breakpoint
		}
		delete this;
		return 0;
	}
	else
	{
		return m_refcount[0];
	}
};


HRESULT	ID3DXMatrixStack::Create()
{
	m_stack.EnsureCapacity( 16 );	// 1KB ish
	m_stack.AddToTail();
	m_stackTop = 0;				// top of stack is at index 0 currently
	
	LoadIdentity();
	
	return S_OK;
}

D3DXMATRIX* ID3DXMatrixStack::GetTop()
{
	return (D3DXMATRIX*)&m_stack[ m_stackTop ];
}

void ID3DXMatrixStack::Push()
{
	D3DMATRIX temp = m_stack[ m_stackTop ];
	m_stack.AddToTail( temp );
	m_stackTop ++;
}

void ID3DXMatrixStack::Pop()
{
	int elem = m_stackTop--;
	m_stack.Remove( elem );
}

void ID3DXMatrixStack::LoadIdentity()
{
	D3DXMATRIX *mat = GetTop();

	D3DXMatrixIdentity( mat );
}

void ID3DXMatrixStack::LoadMatrix( const D3DXMATRIX *pMat )
{
	*(GetTop()) = *pMat;
}


void ID3DXMatrixStack::MultMatrix( const D3DXMATRIX *pMat )
{

	// http://msdn.microsoft.com/en-us/library/bb174057(VS.85).aspx
	//	This method right-multiplies the given matrix to the current matrix
	//	(transformation is about the current world origin).
	//		m_pstack[m_currentPos] = m_pstack[m_currentPos] * (*pMat);
	//	This method does not add an item to the stack, it replaces the current
	//  matrix with the product of the current matrix and the given matrix.


	DXABSTRACT_BREAK_ON_ERROR();
}

void ID3DXMatrixStack::MultMatrixLocal( const D3DXMATRIX *pMat )
{
	//	http://msdn.microsoft.com/en-us/library/bb174058(VS.85).aspx
	//	This method left-multiplies the given matrix to the current matrix
	//	(transformation is about the local origin of the object).
	//		m_pstack[m_currentPos] = (*pMat) * m_pstack[m_currentPos];
	//	This method does not add an item to the stack, it replaces the current
	//	matrix with the product of the given matrix and the current matrix.


	DXABSTRACT_BREAK_ON_ERROR();
}

HRESULT ID3DXMatrixStack::ScaleLocal(FLOAT x, FLOAT y, FLOAT z)
{
	//	http://msdn.microsoft.com/en-us/library/bb174066(VS.85).aspx
	//	Scale the current matrix about the object origin.
	//	This method left-multiplies the current matrix with the computed
	//	scale matrix. The transformation is about the local origin of the object.
	//
	//	D3DXMATRIX tmp;
	//	D3DXMatrixScaling(&tmp, x, y, z);
	//	m_stack[m_currentPos] = tmp * m_stack[m_currentPos];

	DXABSTRACT_BREAK_ON_ERROR();
	return S_OK;
}


HRESULT ID3DXMatrixStack::RotateAxisLocal(CONST D3DXVECTOR3* pV, FLOAT Angle)
{
	//	http://msdn.microsoft.com/en-us/library/bb174062(VS.85).aspx
	//	Left multiply the current matrix with the computed rotation
	//	matrix, counterclockwise about the given axis with the given angle.
	//	(rotation is about the local origin of the object)

	//	D3DXMATRIX tmp;
	//	D3DXMatrixRotationAxis( &tmp, pV, angle );
	//	m_stack[m_currentPos] = tmp * m_stack[m_currentPos];
	//	Because the rotation is left-multiplied to the matrix stack, the rotation
	//	is relative to the object's local coordinate space.
	
	DXABSTRACT_BREAK_ON_ERROR();
	return S_OK;
}

HRESULT ID3DXMatrixStack::TranslateLocal(FLOAT x, FLOAT y, FLOAT z)
{
	//	http://msdn.microsoft.com/en-us/library/bb174068(VS.85).aspx
	//	Left multiply the current matrix with the computed translation
	//	matrix. (transformation is about the local origin of the object)

	//	D3DXMATRIX tmp;
	//	D3DXMatrixTranslation( &tmp, x, y, z );
	//	m_stack[m_currentPos] = tmp * m_stack[m_currentPos];

	DXABSTRACT_BREAK_ON_ERROR();
	return S_OK;
}




const char* D3DXGetPixelShaderProfile( IDirect3DDevice9 *pDevice )
{
	DXABSTRACT_BREAK_ON_ERROR();
	return "";
}

#ifdef _MSC_VER
#pragma warning (push)
#pragma warning (disable:4701) // potentially uninitialized local variable 'temp' used
#endif
D3DXMATRIX* D3DXMatrixMultiply( D3DXMATRIX *pOut, CONST D3DXMATRIX *pM1, CONST D3DXMATRIX *pM2 )
{
	D3DXMATRIX temp;
	
	for( int i=0; i<4; i++)
	{
		for( int j=0; j<4; j++)
		{
			temp.m[i][j]	=	(pM1->m[ i ][ 0 ] * pM2->m[ 0 ][ j ])
							+	(pM1->m[ i ][ 1 ] * pM2->m[ 1 ][ j ])
							+	(pM1->m[ i ][ 2 ] * pM2->m[ 2 ][ j ])
							+	(pM1->m[ i ][ 3 ] * pM2->m[ 3 ][ j ]);
		}
	}
	*pOut = temp;
	return pOut;
}
#ifdef _MSC_VER
#pragma warning(pop)
#endif

// Transform a 3D vector by a given matrix, projecting the result back into w = 1
// http://msdn.microsoft.com/en-us/library/ee417622(VS.85).aspx
D3DXVECTOR3* D3DXVec3TransformCoord(D3DXVECTOR3 *pOut, CONST D3DXVECTOR3 *pV, CONST D3DXMATRIX *pM)
{
	D3DXVECTOR3 vOut;

	float norm = (pM->m[0][3] * pV->x) + (pM->m[1][3] * pV->y) + (pM->m[2][3] *pV->z) + pM->m[3][3];
	if ( norm )
	{
		float norm_inv = 1.0f / norm;
		vOut.x = (pM->m[0][0] * pV->x + pM->m[1][0] * pV->y + pM->m[2][0] * pV->z + pM->m[3][0]) * norm_inv;
		vOut.y = (pM->m[0][1] * pV->x + pM->m[1][1] * pV->y + pM->m[2][1] * pV->z + pM->m[3][1]) * norm_inv;
		vOut.z = (pM->m[0][2] * pV->x + pM->m[1][2] * pV->y + pM->m[2][2] * pV->z + pM->m[3][2]) * norm_inv;
	}
	else
	{
		vOut.x = vOut.y = vOut.z = 0.0f;
	}

	*pOut = vOut;

	return pOut;
}


void D3DXMatrixIdentity( D3DXMATRIX *mat )
{
	for( int i=0; i<4; i++)
	{
		for( int j=0; j<4; j++)
		{
			mat->m[i][j] = (i==j) ? 1.0f : 0.0f;	// 1's on the diagonal.
		}
	}
}

D3DXMATRIX* D3DXMatrixTranslation( D3DXMATRIX *pOut, FLOAT x, FLOAT y, FLOAT z )
{
	D3DXMatrixIdentity( pOut );
	pOut->m[3][0] = x;
	pOut->m[3][1] = y;
	pOut->m[3][2] = z;
	return pOut;
}

D3DXMATRIX* D3DXMatrixInverse( D3DXMATRIX *pOut, FLOAT *pDeterminant, CONST D3DXMATRIX *pM )
{
	Assert( sizeof( D3DXMATRIX ) == (16 * sizeof(float) ) );
	Assert( sizeof( VMatrix ) == (16 * sizeof(float) ) );
	Assert( pDeterminant == NULL );	// homey don't play that

	VMatrix *origM = (VMatrix*)pM;
	VMatrix *destM = (VMatrix*)pOut;

	bool success = MatrixInverseGeneral( *origM, *destM ); (void)success;
	Assert( success );

	return pOut;
}


D3DXMATRIX* D3DXMatrixTranspose( D3DXMATRIX *pOut, CONST D3DXMATRIX *pM )
{
	if (pOut != pM)
	{
		for( int i=0; i<4; i++)
		{
			for( int j=0; j<4; j++)
			{
				pOut->m[i][j] = pM->m[j][i];
			}
		}
	}
	else
	{
		D3DXMATRIX temp = *pM;
		D3DXMatrixTranspose( pOut, &temp );
	}

	return NULL;
}


D3DXPLANE* D3DXPlaneNormalize( D3DXPLANE *pOut, CONST D3DXPLANE *pP)
{
	// not very different from normalizing a vector.
	// figure out the square root of the sum-of-squares of the x,y,z components
	// make sure that's non zero
	// then divide all four components by that value
	// or return some dummy plane like 0,0,1,0 if it fails
	
	float	len = sqrt( (pP->a * pP->a) + (pP->b * pP->b) + (pP->c * pP->c) );
	if (len > 1e-10)	//FIXME need a real epsilon here ?
	{
		pOut->a = pP->a / len;		pOut->b = pP->b / len;		pOut->c = pP->c / len;		pOut->d = pP->d / len;
	}
	else
	{
		pOut->a = 0.0f;				pOut->b = 0.0f;				pOut->c = 1.0f;				pOut->d = 0.0f;
	}
	return pOut;
}


D3DXVECTOR4* D3DXVec4Transform( D3DXVECTOR4 *pOut, CONST D3DXVECTOR4 *pV, CONST D3DXMATRIX *pM )
{
	VMatrix *mat = (VMatrix*)pM;
	Vector4D *vIn = (Vector4D*)pV;
	Vector4D *vOut = (Vector4D*)pOut;

	Vector4DMultiplyTranspose( *mat, *vIn, *vOut );

	return pOut;
}



D3DXVECTOR4* D3DXVec4Normalize( D3DXVECTOR4 *pOut, CONST D3DXVECTOR4 *pV )
{
	Vector4D *vIn = (Vector4D*) pV;
	Vector4D *vOut = (Vector4D*) pOut;

	*vOut = *vIn;
	Vector4DNormalize( *vOut );
	
	return pOut;
}


D3DXMATRIX* D3DXMatrixOrthoOffCenterRH( D3DXMATRIX *pOut, FLOAT l, FLOAT r, FLOAT b, FLOAT t, FLOAT zn,FLOAT zf )
{
	DXABSTRACT_BREAK_ON_ERROR();
	return NULL;
}


D3DXMATRIX* D3DXMatrixPerspectiveRH( D3DXMATRIX *pOut, FLOAT w, FLOAT h, FLOAT zn, FLOAT zf )
{
	DXABSTRACT_BREAK_ON_ERROR();
	return NULL;
}


D3DXMATRIX* D3DXMatrixPerspectiveOffCenterRH( D3DXMATRIX *pOut, FLOAT l, FLOAT r, FLOAT b, FLOAT t, FLOAT zn, FLOAT zf )
{
	DXABSTRACT_BREAK_ON_ERROR();
	return NULL;
}


D3DXPLANE* D3DXPlaneTransform( D3DXPLANE *pOut, CONST D3DXPLANE *pP, CONST D3DXMATRIX *pM )
{
	float *out = &pOut->a;

	// dot dot dot
	for( int x=0; x<4; x++ )
	{
		out[x] =	(pM->m[0][x] * pP->a)
				+	(pM->m[1][x] * pP->b)
				+	(pM->m[2][x] * pP->c)
				+	(pM->m[3][x] * pP->d);
	}
	
	return pOut;
}

// ------------------------------------------------------------------------------------------------------------------------------ //

IDirect3D9 *Direct3DCreate9(UINT SDKVersion)
{
	GLMPRINTF(( "-X- Direct3DCreate9: %d", SDKVersion ));

	return new IDirect3D9;
}

// ------------------------------------------------------------------------------------------------------------------------------ //

void D3DPERF_SetOptions( DWORD dwOptions )
{
}


HRESULT D3DXCompileShader(
        LPCSTR                          pSrcData,
        UINT                            SrcDataLen,
        CONST D3DXMACRO*                pDefines,
        LPD3DXINCLUDE                   pInclude,
        LPCSTR                          pFunctionName,
        LPCSTR                          pProfile,
        DWORD                           Flags,
        LPD3DXBUFFER*                   ppShader,
        LPD3DXBUFFER*                   ppErrorMsgs,
        LPD3DXCONSTANTTABLE*            ppConstantTable)
{
	DXABSTRACT_BREAK_ON_ERROR();	// is anyone calling this ?
	return S_OK;
}

#if defined(DX_TO_GL_ABSTRACTION)
void toglGetClientRect( void *hWnd, RECT *destRect )
{
	// the only useful answer this call can offer, is the size of the canvas.
	// actually getting the window bounds is not useful.
	// so, see if a D3D device is up and running, and if so,
	// dig in and find out its backbuffer size and use that.

	uint width, height;	
	g_pLauncherMgr->RenderedSize( width, height, false );	// false = get them, don't set them
	Assert( width!=0 && height!=0 );

	destRect->left = 0;
	destRect->top = 0;
	destRect->right = width;
	destRect->bottom = height;		
	
	//GLMPRINTF(( "-D- GetClientRect returning rect of (0,0, %d,%d)",width,height ));
	
	return;	
}

#endif

#endif
