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
// glmgr.cpp
//
//===============================================================================
#include "togl/rendermechanism.h"

#include "tier0/icommandline.h"

#include "tier0/vprof.h"
#include "glmtexinlines.h"

#include "materialsystem/IShader.h"
#include "appframework/ilaunchermgr.h"

#include "convar.h"

#include "glmgr_flush.inl"

#ifdef OSX
#include <OpenGL/OpenGL.h>
#include "intelglmallocworkaround.h"
#endif

// memdbgon -must- be the last include file in a .cpp file.
#include "tier0/memdbgon.h"


// Whether the code should use gl_arb_debug_output. This causes error messages to be streamed, via callback, to the application. 
// It is much friendlier to the MTGL driver. 
// NOTE: This can be turned off after launch, but it cannot be turned on after launch--it implies a context-creation-time 
// behavior.
ConVar gl_debug_output( "gl_debug_output", "1" );


// Whether or not we should batch up our creation and deletion behavior. 
ConVar gl_batch_tex_creates( "gl_batch_tex_creates", "0" );
ConVar gl_batch_tex_destroys( "gl_batch_tex_destroys", "0" ); 

//===============================================================================

// g_nTotalDrawsOrClears is reset to 0 in Present()
uint g_nTotalDrawsOrClears, g_nTotalVBLockBytes, g_nTotalIBLockBytes;

#if GL_TELEMETRY_GPU_ZONES
TelemetryGPUStats_t g_TelemetryGPUStats;
#endif

const int kGLMInitialTexCount = 4096;
const int kGLMReUpTexCount = 1024;
const int kGLMHighWaterUndeleted = 2048;
const int kDeletedTextureDim = 4;
const uint32 g_garbageTextureBits[ 4 * kDeletedTextureDim * kDeletedTextureDim ] = { 0 };

char g_nullFragmentProgramText [] =
{
	"!!ARBfp1.0  \n"
	"PARAM black = { 0.0, 0.0, 0.0, 1.0 };  \n"		// opaque black
	"MOV result.color, black;  \n"
	"END  \n\n\n"
	"//GLSLfp\n"
	"void main()\n"
	"{\n"
	"gl_FragColor = vec4( 0.0, 0.0, 0.0, 1.0 );\n"
	"}\n"
	
};

// make dummy programs for doing texture preload via dummy draw
char g_preloadTexVertexProgramText[] =
{
	"//GLSLvp  \n"
	"#version 120  \n"
	"varying vec4 otex;  \n"
	"void main()  \n"
	"{  \n"
	"vec4 pos = ftransform(); // vec4( 0.1, 0.1, 0.1, 0.1 );  \n"
	"vec4 tex = vec4( 0.0, 0.0, 0.0, 0.0 );  \n"
	"  \n"
	"gl_Position = pos;  \n"
	"otex = tex;  \n"
	"}  \n"
};

char g_preload2DTexFragmentProgramText[] =
{
	"//GLSLfp  \n"
	"#version 120  \n"
	"varying vec4 otex;  \n"
	"//SAMPLERMASK-8000		// may not be needed  \n"
	"//HIGHWATER-30			// may not be needed  \n"
	"  \n"
	"uniform vec4 pc[31];  \n"
	"uniform sampler2D sampler15;  \n"
	"  \n"
	"void main()  \n"
	"{  \n"
	"vec4 r0;  \n"
	"r0 = texture2D( sampler15, otex.xy );  \n"
	"gl_FragColor = r0;	//discard;  \n"
	"}  \n"
};

char g_preload3DTexFragmentProgramText[] =
{
	"//GLSLfp  \n"
	"#version 120  \n"
	"varying vec4 otex;  \n"
	"//SAMPLERMASK-8000		// may not be needed  \n"
	"//HIGHWATER-30			// may not be needed  \n"
	"  \n"
	"uniform vec4 pc[31];  \n"
	"uniform sampler3D sampler15;  \n"
	"  \n"
	"void main()  \n"
	"{  \n"
	"vec4 r0;  \n"
	"r0 = texture3D( sampler15, otex.xyz );  \n"
	"gl_FragColor = r0;	//discard;  \n"
	"}  \n"
};

char g_preloadCubeTexFragmentProgramText[] =
{
	"//GLSLfp  \n"
	"#version 120  \n"
	"varying vec4 otex;  \n"
	"//SAMPLERMASK-8000		// may not be needed  \n"
	"//HIGHWATER-30			// may not be needed  \n"
	"  \n"
	"uniform vec4 pc[31];  \n"
	"uniform samplerCube sampler15;  \n"
	"  \n"
	"void main()  \n"
	"{  \n"
	"vec4 r0;  \n"
	"r0 = textureCube( sampler15, otex.xyz );  \n"
	"gl_FragColor = r0;	//discard;  \n"
	"}  \n"
};

const char* glSourceToString(GLenum source)
{
	switch (source)
	{
		case GL_DEBUG_SOURCE_API_ARB:				return "API";
		case GL_DEBUG_SOURCE_WINDOW_SYSTEM_ARB:		return "WINDOW_SYSTEM";
		case GL_DEBUG_SOURCE_SHADER_COMPILER_ARB:	return "SHADER_COMPILER";
		case GL_DEBUG_SOURCE_THIRD_PARTY_ARB:		return "THIRD_PARTY";
		case GL_DEBUG_SOURCE_APPLICATION_ARB:		return "APPLICATION";
		case GL_DEBUG_SOURCE_OTHER_ARB:				return "OTHER";
		default:									break;
	}
	return "UNKNOWN";
}

const char* glTypeToString(GLenum type)
{
	switch (type)
	{
		case GL_DEBUG_TYPE_ERROR_ARB:				return "ERROR";
		case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR_ARB:	return "DEPRECATION";
		case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR_ARB:	return "UNDEFINED_BEHAVIOR";
		case GL_DEBUG_TYPE_PORTABILITY_ARB:			return "PORTABILITY";
		case GL_DEBUG_TYPE_PERFORMANCE_ARB:			return "PERFORMANCE";
		case GL_DEBUG_TYPE_OTHER_ARB:				return "OTHER";
		default:									break;
	}
	return "UNKNOWN";
}

const char* glSeverityToString(GLenum severity)
{
	switch (severity)
	{
		case GL_DEBUG_SEVERITY_HIGH_ARB:			return "HIGH";
		case GL_DEBUG_SEVERITY_MEDIUM_ARB:			return "MEDIUM";
		case GL_DEBUG_SEVERITY_LOW_ARB:				return "LOW";
		default:									break;
	}
	return "UNKNOWN";
}

bool g_bDebugOutputBreakpoints = true;

void APIENTRY GL_Debug_Output_Callback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, GLvoid* userParam)
{
	const char *sSource = glSourceToString(source),
		       *sType = glTypeToString(type),
			   *sSeverity = glSeverityToString(severity);
	
	// According to NVidia, this error is a bug in the driver and not really an error (it's a warning in newer drivers): "Texture X is base level inconsistent. Check texture size"
	if (  ( type == GL_DEBUG_TYPE_ERROR_ARB ) && strstr( message, "base level inconsistent" ) )
	{
		return;
	}
	
	if ( gl_debug_output.GetBool() || type == GL_DEBUG_TYPE_ERROR_ARB )
	{
		Msg( "GL: [%s][%s][%s][%d]: %s\n", sSource, sType, sSeverity, id, message );
	}

#ifdef WIN32
	OutputDebugStringA( message );
#endif
			
	if ( ( type == GL_DEBUG_TYPE_ERROR_ARB ) && ( g_bDebugOutputBreakpoints ) )
	{
		DebuggerBreak();
	}
}

void GLMDebugPrintf( const char *pMsg, ... )
{
	//$ TODO: Should this call Warning()?
	//$ TODO: Should replace call these calls with Warning() / Msg() / DevMsg()?

	va_list args;
	va_start( args, pMsg );
	vprintf( pMsg, args );
	va_end( args );
}

//===============================================================================
// functions that are dependant on g_pLauncherMgr

inline bool MakeContextCurrent( PseudoGLContextPtr hContext ) 
{
	return g_pLauncherMgr->MakeContextCurrent( hContext );
}

inline PseudoGLContextPtr GetMainContext()
{
	return g_pLauncherMgr->GetMainContext();
}

inline PseudoGLContextPtr GetGLContextForWindow( void* windowref )
{
	return g_pLauncherMgr->GetGLContextForWindow( windowref );
}

inline void IncrementWindowRefCount()
{
	g_pLauncherMgr->IncWindowRefCount();
}
inline void DecrementWindowRefCount()
{
	g_pLauncherMgr->DecWindowRefCount();
}
inline void ShowPixels( CShowPixelsParams *params )
{
	g_pLauncherMgr->ShowPixels(params);
}

inline void DisplayedSize( uint &width, uint &height ) 
{
	g_pLauncherMgr->DisplayedSize( width, height );
}

inline void GetDesiredPixelFormatAttribsAndRendererInfo( uint **ptrOut, uint *countOut, GLMRendererInfoFields *rendInfoOut )
{
	g_pLauncherMgr->GetDesiredPixelFormatAttribsAndRendererInfo( ptrOut, countOut, rendInfoOut );
}

inline void GetStackCrawl( CStackCrawlParams *params )
{
	g_pLauncherMgr->GetStackCrawl(params);
}

#if GLMDEBUG
inline void PumpWindowsMessageLoop()
{
	g_pLauncherMgr->PumpWindowsMessageLoop();
}
inline int GetEvents( CCocoaEvent *pEvents, int nMaxEventsToReturn, bool debugEvents = false ) 
{
	return g_pLauncherMgr->GetEvents( pEvents, nMaxEventsToReturn, debugEvents );
}
#endif

//===============================================================================
// helper routines for debug

static bool hasnonzeros( float *values, int count )
{
	for( int i=0; i<count; i++)
	{
		if (values[i] != 0.0)
		{
			return true;
		}
	}
	return false;
}

static void printmat( char *label, int baseSlotNumber, int slots, float *m00 )
{
	// print label..
	// fetch 4 from row, print as a row
	// fetch 4 from column, print as a row
	
	float	row[4];
	float	col[4];
	
	if (hasnonzeros( m00, slots*4) )
	{
		GLMPRINTF(("-D-  %s", label ));
		for( int islot=0; islot<4; islot++ )			// we always run this loop til 4, but we special case the printing if there are only 3 submitted
		{
			// extract row and column floats
			for( int slotcol=0; slotcol<4; slotcol++)
			{
				//copy
				row[slotcol] = m00[(islot*4)+slotcol];
				
				// transpose
				col[slotcol] = m00[(slotcol*4)+islot];
			}
			if (slots==4)
			{
				GLMPRINTF((		"-D-    %03d: [ %10.5f %10.5f %10.5f %10.5f ] T=> [ %10.5f %10.5f %10.5f %10.5f ]",
								baseSlotNumber+islot,
								row[0],row[1],row[2],row[3],
								col[0],col[1],col[2],col[3]						
								));
			}
			else
			{
				if (islot<3)
				{
					GLMPRINTF((		"-D-    %03d: [ %10.5f %10.5f %10.5f %10.5f ] T=> [ %10.5f %10.5f %10.5f ]",
									baseSlotNumber+islot,
									row[0],row[1],row[2],row[3],
									col[0],col[1],col[2]
									));
				}
				else
				{
					GLMPRINTF((		"-D-    %03d:                                                 T=> [ %10.5f %10.5f %10.5f ]",
									baseSlotNumber+islot,
									col[0],col[1],col[2]
									));
				}
			}
		}
		GLMPRINTSTR(("-D-"));
	}
	else
	{
		GLMPRINTF(("-D-  %s - (all 0.0)", label ));
	}

}


static void transform_dp4( float *in4, float *m00, int slots, float *out4 )
{
	// m00 points to a column.
	// each DP is one column of the matrix ( m00[4*n]
	// if we are passed a three slot matrix, this is three columns, the source W plays into all three columns, but we must set the final output W to 1 ?
	for( int n=0; n<slots; n++)
	{
		float col4[4];
		
		col4[0] = m00[(4*n)+0];
		col4[1] = m00[(4*n)+1];
		col4[2] = m00[(4*n)+2];
		col4[3] = m00[(4*n)+3];
		
		out4[n] = 0.0;
		for( int inner = 0; inner < 4; inner++ )
		{
			out4[n] += in4[inner] * col4[inner];
		}
	}
	if (slots==3)
	{
		out4[3] = 1.0;
	}
}

//===============================================================================


//===============================================================================
// GLMgr static methods

GLMgr	*g_glmgr = NULL;

void GLMgr::NewGLMgr( void )
{
	if (!g_glmgr)
	{
		#if GLMDEBUG
			// check debug mode early in program lifetime
			GLMDebugInitialize( true );
		#endif

		g_glmgr = new GLMgr;
	}
}

GLMgr *GLMgr::aGLMgr( void )
{
	assert( g_glmgr != NULL);
	return g_glmgr;
}

void	GLMgr::DelGLMgr( void )
{
	if (g_glmgr)
	{
		delete g_glmgr;
		g_glmgr = NULL;
	}
}

// GLMgr class methods

GLMgr::GLMgr()
{
}	


GLMgr::~GLMgr()
{
}

//===============================================================================

GLMContext *GLMgr::NewContext( IDirect3DDevice9 *pDevice, GLMDisplayParams *params )
{
	// this now becomes really simple.  We just pass through the params.
	
	return new GLMContext( pDevice, params );
}

void GLMgr::DelContext( GLMContext *context )
{
	delete context;
}

void GLMgr::SetCurrentContext( GLMContext *context )
{
#if defined( USE_SDL )
	context->m_nCurOwnerThreadId = ThreadGetCurrentId();
	if ( !MakeContextCurrent( context->m_ctx ) )
	{
		// give up
		GLMStop();
	}
	Assert( 0 );
#endif
}

GLMContext *GLMgr::GetCurrentContext( void )
{
#if defined( USE_SDL )
	PseudoGLContextPtr context = GetMainContext();
	return (GLMContext*) context;
#else
	Assert( 0 );
	return NULL;
#endif
}
	

// #define CHECK_THREAD_USAGE	1


//===============================================================================
// GLMContext public methods
void GLMContext::MakeCurrent( bool bRenderThread )
{
	tmZone( TELEMETRY_LEVEL0, 0, "GLMContext::MakeCurrent" );
	Assert( m_nCurOwnerThreadId == 0 || m_nCurOwnerThreadId == ThreadGetCurrentId() );
		
#if defined( USE_SDL )

#ifndef CHECK_THREAD_USAGE
	if ( bRenderThread )
	{
//		Msg( "********************************************  %08x Acquiring Context\n", ThreadGetCurrentId() );
		m_nCurOwnerThreadId = ThreadGetCurrentId();
		bool bSuccess = MakeContextCurrent( m_ctx );
		if ( !bSuccess )
		{
			Assert( 0 );
		}
	}
#else
	uint32 dwThreadId = ThreadGetCurrentId();

	if ( bRenderThread || dwThreadId == m_dwRenderThreadId )
	{
		m_nCurOwnerThreadId = ThreadGetCurrentId();
		m_dwRenderThreadId = dwThreadId;
		MakeContextCurrent( m_ctx );
		m_bIsThreading = true;
	}
	else if ( !m_bIsThreading )
	{
		m_nCurOwnerThreadId = ThreadGetCurrentId();
		MakeContextCurrent( m_ctx );
	}
	else
	{
		Assert( 0 );
	}
#endif

#else
	Assert( 0 );
#endif
}


void GLMContext::ReleaseCurrent( bool bRenderThread )
{
	tmZone( TELEMETRY_LEVEL0, 0, "GLMContext::ReleaseCurrent" );
	Assert( m_nCurOwnerThreadId == ThreadGetCurrentId() );
		
#if defined( USE_SDL )

#ifndef CHECK_THREAD_USAGE
	if ( bRenderThread )
	{
//		Msg( "********************************************  %08x Releasing Context\n", ThreadGetCurrentId() );
		m_nCurOwnerThreadId = 0;
		m_nThreadOwnershipReleaseCounter++;
		MakeContextCurrent( NULL );
	}
#else
	m_nCurOwnerThreadId = 0;
	m_nThreadOwnershipReleaseCounter++;
	MakeContextCurrent( NULL );
	if ( bRenderThread )
	{
		m_bIsThreading = false;
	}
#endif

#else
	Assert( 0 );
#endif
}


// This function forces all GL state to be re-sent to the context. Some state will only be set on the next batch flush.
void GLMContext::ForceFlushStates()
{
	// Flush various render states
	m_AlphaTestEnable.Flush();
	m_AlphaTestFunc.Flush();

	m_DepthBias.Flush();

	m_ScissorEnable.Flush();	
	m_ScissorBox.Flush();

	m_ViewportBox.Flush();		
	m_ViewportDepthRange.Flush();

	m_ColorMaskSingle.Flush();	

	m_BlendEnable.Flush();
	m_BlendFactor.Flush();

	m_BlendEnableSRGB.Flush();

	m_DepthTestEnable.Flush();
	m_DepthFunc.Flush();
	m_DepthMask.Flush();

	m_StencilTestEnable.Flush();
	m_StencilFunc.Flush();
	m_StencilOp.Flush();
	m_StencilWriteMask.Flush();

	m_ClearColor.Flush();
	m_ClearDepth.Flush();
	m_ClearStencil.Flush();

	m_ClipPlaneEnable.Flush();	// always push clip state
	m_ClipPlaneEquation.Flush();

	m_CullFaceEnable.Flush();

	m_CullFrontFace.Flush();

	m_PolygonMode.Flush();

	m_AlphaToCoverageEnable.Flush();
	m_ColorMaskMultiple.Flush();
	m_BlendEquation.Flush();
	m_BlendColor.Flush();
	// Reset various things so they get reset on the next batch flush
	m_activeTexture = -1;

	for ( int i = 0; i < GLM_SAMPLER_COUNT; i++ )
	{
		SetSamplerTex( i, m_samplers[i].m_pBoundTex );
		SetSamplerDirty( i );
	}

	// Attributes/vertex attribs
	ClearCurAttribs();

	m_lastKnownVertexAttribMask = 0;
	m_nNumSetVertexAttributes = 16;
	memset( &m_boundVertexAttribs[0], 0xFF, sizeof( m_boundVertexAttribs ) );
	for( int index=0; index < kGLMVertexAttributeIndexMax; index++ )
		gGL->glDisableVertexAttribArray( index );

	// Program
	NullProgram();

	// FBO
	BindFBOToCtx( m_boundReadFBO, GL_READ_FRAMEBUFFER_EXT );
	BindFBOToCtx( m_boundDrawFBO, GL_DRAW_FRAMEBUFFER_EXT );

	// Current VB/IB/pinned memory buffers
	gGL->glBindBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB, m_nBoundGLBuffer[ kGLMIndexBuffer] );
	gGL->glBindBufferARB( GL_ARRAY_BUFFER_ARB, m_nBoundGLBuffer[ kGLMVertexBuffer] );

#ifndef OSX
	if ( gGL->m_bHave_GL_AMD_pinned_memory )
	{
		gGL->glBindBufferARB( GL_EXTERNAL_VIRTUAL_MEMORY_BUFFER_AMD, m_PinnedMemoryBuffers[m_nCurPinnedMemoryBuffer].GetHandle() );
	}
#endif
}

const GLMRendererInfoFields& GLMContext::Caps( void )
{
	return m_caps;
}

void GLMContext::DumpCaps( void )
{
	/*
		#define	dumpfield( fff ) printf( "\n  "#fff" : %d", (int) m_caps.fff )
		#define	dumpfield_hex( fff ) printf( "\n  "#fff" : 0x%08x", (int) m_caps.fff )
		#define	dumpfield_str( fff ) printf( "\n  "#fff" : %s", m_caps.fff )
	*/

	#define	dumpfield( fff )		printf( "\n  %-30s : %d", #fff, (int) m_caps.fff )
	#define	dumpfield_hex( fff )	printf( "\n  %-30s : 0x%08x", #fff, (int) m_caps.fff )
	#define	dumpfield_str( fff )	printf( "\n  %-30s : %s", #fff, m_caps.fff )

	printf("\n-------------------------------- context caps for context %08x", (uint)this);

	dumpfield( m_fullscreen );
	dumpfield( m_accelerated );
	dumpfield( m_windowed );
	dumpfield_hex( m_rendererID );
	dumpfield( m_displayMask );
	dumpfield( m_bufferModes );
	dumpfield( m_colorModes );
	dumpfield( m_accumModes );
	dumpfield( m_depthModes );
	dumpfield( m_stencilModes );
	dumpfield( m_maxAuxBuffers );
	dumpfield( m_maxSampleBuffers );
	dumpfield( m_maxSamples );
	dumpfield( m_sampleModes );
	dumpfield( m_sampleAlpha );
	dumpfield_hex( m_vidMemory );
	dumpfield_hex( m_texMemory );

	dumpfield_hex( m_pciVendorID );
	dumpfield_hex( m_pciDeviceID );
	dumpfield_str( m_pciModelString );
	dumpfield_str( m_driverInfoString );

	printf( "\n  m_osComboVersion: 0x%08x (%d.%d.%d)", m_caps.m_osComboVersion, (m_caps.m_osComboVersion>>16)&0xFF, (m_caps.m_osComboVersion>>8)&0xFF, (m_caps.m_osComboVersion)&0xFF );

	dumpfield( m_ati );
	if (m_caps.m_ati)
	{
		dumpfield( m_atiR5xx );
		dumpfield( m_atiR6xx );
		dumpfield( m_atiR7xx );
		dumpfield( m_atiR8xx );
		dumpfield( m_atiNewer );
	}

	dumpfield( m_intel );
	if (m_caps.m_intel)
	{
		dumpfield( m_intel95x );
		dumpfield( m_intel3100 );
		dumpfield( m_intelHD4000 );
	}

	dumpfield( m_nv );
	if (m_caps.m_nv)
	{
		//dumpfield( m_nvG7x );
		dumpfield( m_nvG8x );
		dumpfield( m_nvNewer );
	}

	dumpfield( m_hasGammaWrites );
	dumpfield( m_hasMixedAttachmentSizes );
	dumpfield( m_hasBGRA );
	dumpfield( m_hasNewFullscreenMode );
	dumpfield( m_hasNativeClipVertexMode );
	dumpfield( m_maxAniso );
	
	dumpfield( m_hasBindableUniforms );
	dumpfield( m_maxVertexBindableUniforms );
	dumpfield( m_maxFragmentBindableUniforms );
	dumpfield( m_maxBindableUniformSize );

	dumpfield( m_hasUniformBuffers );
	dumpfield( m_hasPerfPackage1 );
	
	dumpfield( m_cantBlitReliably );
	dumpfield( m_cantAttachSRGB );
	dumpfield( m_cantResolveFlipped );
	dumpfield( m_cantResolveScaled );
	dumpfield( m_costlyGammaFlips );
	dumpfield( m_badDriver1064NV );
	dumpfield( m_badDriver108Intel );

	printf("\n--------------------------------");
	
	#undef dumpfield
	#undef dumpfield_hex
	#undef dumpfield_str
}

CGLMTex	*GLMContext::NewTex( GLMTexLayoutKey *key, uint levels, const char *debugLabel )
{
	// get a layout based on the key
	GLMTexLayout *layout = m_texLayoutTable->NewLayoutRef( key );
			
	CGLMTex *tex = new CGLMTex( this, layout, levels, debugLabel );
	
	return tex;
}

void GLMContext::DelTex( CGLMTex * tex )
{
	//Queue the texture for deletion in ProcessTextureDeletes
	//when we are sure we will hold the context.
	m_DeleteTextureQueue.PushItem(tex);
}

void GLMContext::ProcessTextureDeletes()
{
#if GL_TELEMETRY_GPU_ZONES
	CScopedGLMPIXEvent glmEvent( "GLMContext::ProcessTextureDeletes" );
#endif

	CGLMTex* tex = nullptr;
	while ( m_DeleteTextureQueue.PopItem( &tex ) )
	{
		for( int i = 0; i < GLM_SAMPLER_COUNT; i++)
		{
			if ( m_samplers[i].m_pBoundTex == tex )
			{
				BindTexToTMU( NULL, i );
			}
		}
			
		if ( tex->m_rtAttachCount != 0 )
		{
			// RG - huh? wtf? TODO: fix this code which seems to be purposely leaking
			// leak it and complain - we may have to implement a deferred-delete system for tex like these

			GLMDebugPrintf("GLMContext::DelTex: Leaking tex %08x [ %s ] - was attached for drawing at time of delete",tex, tex->m_layout->m_layoutSummary );

			#if 0
				// can't actually do this yet as the draw calls will tank
				FOR_EACH_VEC( m_fboTable, i )
				{
					CGLMFBO *fbo = m_fboTable[i];
					fbo->TexScrub( tex );
				}
				tex->m_rtAttachCount = 0;
			#endif
		}
		else
		{	
			delete tex;
		}
	}
}

// push and pop attrib when blit has mixed srgb source and dest?		
ConVar	gl_radar7954721_workaround_mixed ( "gl_radar7954721_workaround_mixed", "1" );

// push and pop attrib on any blit?
ConVar	gl_radar7954721_workaround_all ( "gl_radar7954721_workaround_all", "0" );

// what attrib mask to use ?
ConVar	gl_radar7954721_workaround_maskval ( "gl_radar7954721_workaround_maskval", "0" );

enum eBlitFormatClass
{
	eColor,
	eDepth,				// may not get used.  not sure..
	eDepthStencil
};

uint	glAttachFromClass[ 3 ] = 		{ GL_COLOR_ATTACHMENT0_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_DEPTH_STENCIL_ATTACHMENT_EXT };

void glScrubFBO			( GLenum target )
{
	gGL->glFramebufferRenderbufferEXT	( target, GL_COLOR_ATTACHMENT0_EXT, GL_RENDERBUFFER_EXT, 0);
	gGL->glFramebufferRenderbufferEXT	( target, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, 0);
	gGL->glFramebufferRenderbufferEXT	( target, GL_STENCIL_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, 0);

	gGL->glFramebufferTexture2DEXT		( target, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, 0, 0 );
	gGL->glFramebufferTexture2DEXT		( target, GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_2D, 0, 0 );
	gGL->glFramebufferTexture2DEXT		( target, GL_STENCIL_ATTACHMENT_EXT, GL_TEXTURE_2D, 0, 0 );
}

void glAttachRBOtoFBO	( GLenum target, eBlitFormatClass formatClass, uint rboName )
{
	switch( formatClass )
	{
		case eColor:
			gGL->glFramebufferRenderbufferEXT	( target, GL_COLOR_ATTACHMENT0_EXT, GL_RENDERBUFFER_EXT, rboName);
		break;
		
		case eDepth:
			gGL->glFramebufferRenderbufferEXT	( target, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, rboName);
		break;
		
		case eDepthStencil:
			gGL->glFramebufferRenderbufferEXT	( target, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, rboName);
			gGL->glFramebufferRenderbufferEXT	( target, GL_STENCIL_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, rboName);
		break;
	}
}

void glAttachTex2DtoFBO	( GLenum target, eBlitFormatClass formatClass, uint texName, uint texMip )
{
	switch( formatClass )
	{
		case eColor:
			gGL->glFramebufferTexture2DEXT		( target, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, texName, texMip );
		break;
		
		case eDepth:
			gGL->glFramebufferTexture2DEXT		( target, GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_2D, texName, texMip );
		break;
		
		case eDepthStencil:
			gGL->glFramebufferTexture2DEXT		( target, GL_DEPTH_STENCIL_ATTACHMENT_EXT, GL_TEXTURE_2D, texName, texMip );
		break;
	}
}

ConVar gl_can_resolve_flipped("gl_can_resolve_flipped", "0" );
ConVar gl_cannot_resolve_flipped("gl_cannot_resolve_flipped", "0" );

// these are only consulted if the m_cant_resolve_scaled cap bool is false.

ConVar gl_minify_resolve_mode("gl_minify_resolve_mode", "1" );		// if scaled resolve available, for downscaled resolve blits only (i.e. internal blits)
ConVar gl_magnify_resolve_mode("gl_magnify_resolve_mode", "2" );	// if scaled resolve available, for upscaled resolve blits only

	// 0 == old style, two steps
	// 1 == faster, one step blit aka XGL_SCALED_RESOLVE_FASTEST_EXT - if available.
	// 2 == faster, one step blit aka XGL_SCALED_RESOLVE_NICEST_EXT - if available.

void GLMContext::SaveColorMaskAndSetToDefault()
{
	// NVidia's driver doesn't ignore the colormask during blitframebuffer calls, so we need to save/restore it: 
	// “The bug here is that our driver fails to ignore colormask for BlitFramebuffer calls. This was unclear in the original spec, but we resolved it in Khronos last year (https://cvs.khronos.org/bugzilla/show_bug.cgi?id=7969).”
	m_ColorMaskSingle.Read( &m_SavedColorMask, 0 );

	GLColorMaskSingle_t newColorMask;
	newColorMask.r = newColorMask.g = newColorMask.b = newColorMask.a = -1;
	m_ColorMaskSingle.Write( &newColorMask );
}

void GLMContext::RestoreSavedColorMask()
{
	m_ColorMaskSingle.Write( &m_SavedColorMask );
}

void GLMContext::Blit2( CGLMTex *srcTex, GLMRect *srcRect, int srcFace, int srcMip, CGLMTex *dstTex, GLMRect *dstRect, int dstFace, int dstMip, uint filter )
{
#if GL_TELEMETRY_GPU_ZONES
	CScopedGLMPIXEvent glmPIXEvent( "Blit2" );
	g_TelemetryGPUStats.m_nTotalBlit2++;
#endif
	
	SaveColorMaskAndSetToDefault();
	
	Assert( srcFace == 0 );
	Assert( dstFace == 0 );
	
	//----------------------------------------------------------------- format assessment

	eBlitFormatClass	formatClass = eColor;
	uint				blitMask= 0;

	switch( srcTex->m_layout->m_format->m_glDataFormat )
	{
		case GL_RED: case GL_BGRA:	case GL_RGB:	case GL_RGBA:	case GL_ALPHA:	case GL_LUMINANCE:	case GL_LUMINANCE_ALPHA:
			formatClass = eColor;
			blitMask = GL_COLOR_BUFFER_BIT;
		break;

		case GL_DEPTH_COMPONENT:
			formatClass = eDepth;
			blitMask = GL_DEPTH_BUFFER_BIT;
		break;
		
		case GL_DEPTH_STENCIL_EXT:
			formatClass = eDepthStencil;
			blitMask = GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
		break;

		default:
			Assert(!"Unsupported format for blit" );
			GLMStop();
		break;
	}

	//----------------------------------------------------------------- blit assessment
	

	bool blitResolves	=	srcTex->m_rboName != 0;
	bool blitScales		=	((srcRect->xmax - srcRect->xmin) != (dstRect->xmax - dstRect->xmin)) || ((srcRect->ymax - srcRect->ymin) != (dstRect->ymax - dstRect->ymin));

	bool blitToBack		=	(dstTex == NULL);
	bool blitFlips		=	blitToBack;			// implicit y-flip upon blit to GL_BACK supplied

	//should we support blitFromBack ?

	bool srcGamma = srcTex && ((srcTex->m_layout->m_key.m_texFlags & kGLMTexSRGB) != 0);
	bool dstGamma = dstTex && ((dstTex->m_layout->m_key.m_texFlags & kGLMTexSRGB) != 0);

	bool doPushPop = (srcGamma != dstGamma) && gl_radar7954721_workaround_mixed.GetInt() && m_caps.m_nv;		// workaround for cross gamma blit problems on NV
		// ^^ need to re-check this on some post-10.6.3 build on NV to see if it was fixed

	if (doPushPop)
	{
		gGL->glPushAttrib( 0 );
	}
	
	//----------------------------------------------------------------- figure out the plan
	
	bool blitTwoStep = false;		// think positive
	
	// each subsequent segment here can only set blitTwoStep, not clear it.
	// the common case where these get hit is resolve out to presentation
	// there may be GL extensions or driver revisions which start doing these safely.
	// ideally many blits internally resolve without scaling and can thus go direct without using the scratch tex.
	
	if (blitResolves && (blitFlips||blitToBack))		// flips, blit to back, same thing (for now)
	{
		if( gl_cannot_resolve_flipped.GetInt() )
		{
			blitTwoStep = true;
		}
		else if (!gl_can_resolve_flipped.GetInt())
		{
			blitTwoStep = blitTwoStep || m_caps.m_cantResolveFlipped;	// if neither convar renders an opinion, fall back to the caps to decide if we have to two-step.
		}		
	}

	// only consider trying to use the scaling resolve filter,
	// if we are confident we are not headed for two step mode already.
	if (!blitTwoStep)
	{
		if (blitResolves && blitScales)
		{
			if (m_caps.m_cantResolveScaled)
			{
				// filter is unchanged, two step mode switches on
				blitTwoStep = true;
			}
			else
			{
				bool	blitScalesDown	= ((srcRect->xmax - srcRect->xmin) > (dstRect->xmax - dstRect->xmin)) || ((srcRect->ymax - srcRect->ymin) > (dstRect->ymax - dstRect->ymin));
				int		mode			= (blitScalesDown) ? gl_minify_resolve_mode.GetInt() : gl_magnify_resolve_mode.GetInt();
				
				// roughly speaking, resolve blits that minify represent setup for special effects ("copy framebuffer to me")
				// resolve blits that magnify are almost always on the final present in the case where remder size < display size
				
				switch( mode )
				{
					case 0:
					default:
						// filter is unchanged, two step mode
						blitTwoStep = true;
					break;
						
					case 1:
						// filter goes to fastest, one step mode
						blitTwoStep = false;
						filter = XGL_SCALED_RESOLVE_FASTEST_EXT;
					break;
						
					case 2:
						// filter goes to nicest, one step mode
						blitTwoStep = false;
						filter = XGL_SCALED_RESOLVE_NICEST_EXT;
					break;					
				}
			}
		}	
	}

	//----------------------------------------------------------------- save old scissor state and disable scissor
	GLScissorEnable_t	oldsciss,newsciss;
	m_ScissorEnable.Read( &oldsciss, 0 );

	if (oldsciss.enable)
	{
		//	turn off scissor
		newsciss.enable = false;
		m_ScissorEnable.Write( &newsciss );
	}

	//----------------------------------------------------------------- fork in the road, depending on two-step or not
	if (blitTwoStep)
	{
		// a resolve that can't be done directly due to constraints on scaling or flipping.
		
		// bind scratch FBO0 to read, scrub it, attach RBO
		BindFBOToCtx		( m_scratchFBO[0], GL_READ_FRAMEBUFFER_EXT );
		glScrubFBO			( GL_READ_FRAMEBUFFER_EXT );
		glAttachRBOtoFBO	( GL_READ_FRAMEBUFFER_EXT, formatClass, srcTex->m_rboName );
		
		// bind scratch FBO1 to write, scrub it, attach scratch tex
		BindFBOToCtx		( m_scratchFBO[1], GL_DRAW_FRAMEBUFFER_EXT );
		glScrubFBO			( GL_DRAW_FRAMEBUFFER_EXT );
		glAttachTex2DtoFBO	( GL_DRAW_FRAMEBUFFER_EXT, formatClass, srcTex->m_texName, 0 );

		// set read and draw buffers appropriately		
		gGL->glReadBuffer		( glAttachFromClass[formatClass] );
		gGL->glDrawBuffer		( glAttachFromClass[formatClass] );
		
		// blit#1 - to resolve to scratch
		// implicitly means no scaling, thus will be done with NEAREST sampling

		GLenum resolveFilter = GL_NEAREST;
		
		gGL->glBlitFramebufferEXT(	0, 0,	srcTex->m_layout->m_key.m_xSize, srcTex->m_layout->m_key.m_ySize,
								0, 0,	srcTex->m_layout->m_key.m_xSize, srcTex->m_layout->m_key.m_ySize,	// same source and dest rect, whole surface
								blitMask, resolveFilter );
								
		// FBO1 now holds the interesting content.
		// scrub FBO0, bind FBO1 to READ, fall through to next stage of blit where 1 goes onto 0 (or BACK)
		
		glScrubFBO			( GL_READ_FRAMEBUFFER_EXT );	// zap FBO0
		BindFBOToCtx		( m_scratchFBO[1], GL_READ_FRAMEBUFFER_EXT );

		srcTex->ForceRBONonDirty();
	}
	else
	{
#if 1
		if (srcTex->m_pBlitSrcFBO == NULL) 
		{
			srcTex->m_pBlitSrcFBO = NewFBO();
			BindFBOToCtx( srcTex->m_pBlitSrcFBO, GL_READ_FRAMEBUFFER_EXT );
			if (blitResolves)
			{
				glAttachRBOtoFBO( GL_READ_FRAMEBUFFER_EXT, formatClass, srcTex->m_rboName );
			}
			else
			{
				glAttachTex2DtoFBO( GL_READ_FRAMEBUFFER_EXT, formatClass, srcTex->m_texName, srcMip );
			}
		} 
		else 
		{
			BindFBOToCtx		( srcTex->m_pBlitSrcFBO, GL_READ_FRAMEBUFFER_EXT );
			//                     GLMCheckError();
		}
#else
		// arrange source surface on FBO1 for blit directly to dest (which could be FBO0 or BACK)
		BindFBOToCtx( m_scratchFBO[1], GL_READ_FRAMEBUFFER_EXT );
		glScrubFBO( GL_READ_FRAMEBUFFER_EXT );
		GLMCheckError();
		if (blitResolves)
		{
			glAttachRBOtoFBO( GL_READ_FRAMEBUFFER_EXT, formatClass, srcTex->m_rboName );
		}
		else
		{
			glAttachTex2DtoFBO( GL_READ_FRAMEBUFFER_EXT, formatClass, srcTex->m_texName, srcMip );
		}
#endif

		gGL->glReadBuffer( glAttachFromClass[formatClass] );
	}
	
	//----------------------------------------------------------------- zero or one blits may have happened above, whichever took place, FBO1 is now on read
	
	bool yflip = false;
	if (blitToBack)
	{
		// backbuffer is special - FBO0 is left out (either scrubbed already, or not used)
		
		BindFBOToCtx		( NULL, GL_DRAW_FRAMEBUFFER_EXT );
		gGL->glDrawBuffer		( GL_BACK );
		
		yflip = true;
	}
	else
	{
		// not going to GL_BACK - use FBO0. set up dest tex or RBO on it.  i.e. it's OK to blit from MSAA to MSAA if needed, though unlikely.
		Assert( dstTex != NULL );
#if 1
		if (dstTex->m_pBlitDstFBO == NULL) 
		{
			dstTex->m_pBlitDstFBO = NewFBO();
			BindFBOToCtx( dstTex->m_pBlitDstFBO, GL_DRAW_FRAMEBUFFER_EXT );
			if (dstTex->m_rboName)
			{
				glAttachRBOtoFBO( GL_DRAW_FRAMEBUFFER_EXT, formatClass, dstTex->m_rboName );		
			}
			else
			{
				glAttachTex2DtoFBO( GL_DRAW_FRAMEBUFFER_EXT, formatClass, dstTex->m_texName, dstMip );
			}
		} 
		else
		{
			BindFBOToCtx( dstTex->m_pBlitDstFBO, GL_DRAW_FRAMEBUFFER_EXT );
		}
#else
		BindFBOToCtx( m_scratchFBO[0], GL_DRAW_FRAMEBUFFER_EXT );							GLMCheckError();								
		glScrubFBO( GL_DRAW_FRAMEBUFFER_EXT );

		if (dstTex->m_rboName)
		{
			glAttachRBOtoFBO( GL_DRAW_FRAMEBUFFER_EXT, formatClass, dstTex->m_rboName );		
		}
		else
		{
			glAttachTex2DtoFBO( GL_DRAW_FRAMEBUFFER_EXT, formatClass, dstTex->m_texName, dstMip );
		}	

		gGL->glDrawBuffer		( glAttachFromClass[formatClass] );										GLMCheckError();
#endif							
	}

	// final blit
	
	// i think in general, if we are blitting same size, gl_nearest is the right filter to pass.
	// this re-steering won't kick in if there is scaling or a special scaled resolve going on.
	if (!blitScales)
	{
		// steer it
		filter = GL_NEAREST;
	}
	
	// this is blit #1 or #2 depending on what took place above.
	if (yflip)
	{
		gGL->glBlitFramebufferEXT(	srcRect->xmin, srcRect->ymin, srcRect->xmax, srcRect->ymax,
								dstRect->xmin, dstRect->ymax, dstRect->xmax, dstRect->ymin,		// note dest Y's are flipped
								blitMask, filter );
	}
	else
	{
		gGL->glBlitFramebufferEXT(	srcRect->xmin, srcRect->ymin, srcRect->xmax, srcRect->ymax,
								dstRect->xmin, dstRect->ymin, dstRect->xmax, dstRect->ymax,
								blitMask, filter );
	}

	//----------------------------------------------------------------- scrub READ and maybe DRAW FBO, and unbind

//	glScrubFBO			( GL_READ_FRAMEBUFFER_EXT );
	BindFBOToCtx		( NULL, GL_READ_FRAMEBUFFER_EXT );
	if (!blitToBack)
	{
//		glScrubFBO			( GL_DRAW_FRAMEBUFFER_EXT );
		BindFBOToCtx		( NULL, GL_DRAW_FRAMEBUFFER_EXT );
	}
		
	//----------------------------------------------------------------- restore GLM's drawing FBO

	//	restore GLM drawing FBO
	BindFBOToCtx( m_drawingFBO, GL_FRAMEBUFFER_EXT );
	
	if (doPushPop)
	{
		gGL->glPopAttrib( );
	}
	

	//----------------------------------------------------------------- restore old scissor state
	if (oldsciss.enable)
	{
		m_ScissorEnable.Write( &oldsciss );
	}

	RestoreSavedColorMask();
}


void GLMContext::BlitTex( CGLMTex *srcTex, GLMRect *srcRect, int srcFace, int srcMip, CGLMTex *dstTex, GLMRect *dstRect, int dstFace, int dstMip, GLenum filter, bool useBlitFB )
{
	// This path doesn't work anymore (or did it ever work in the L4D2 Linux branch?)
	DXABSTRACT_BREAK_ON_ERROR();
	return;

	SaveColorMaskAndSetToDefault();

	switch( srcTex->m_layout->m_format->m_glDataFormat )
	{
		case GL_BGRA:
		case GL_RGB:
		case GL_RGBA:
		case GL_ALPHA:
		case GL_LUMINANCE:
		case GL_LUMINANCE_ALPHA:
			#if 0
				if (GLMKnob("caps-key",NULL) > 0.0)
				{
					useBlitFB = false;
				}
			#endif

			if ( m_caps.m_cantBlitReliably )	// this is referring to a problem with the x3100..
			{
				useBlitFB = false;
			}
		break;
	}
	
	if (0)
	{
		GLMPRINTF(("-D- Blit from %d %d %d %d  to %d %d %d %d",
			srcRect->xmin, srcRect->ymin, srcRect->xmax, srcRect->ymax,
			dstRect->xmin, dstRect->ymin, dstRect->xmax, dstRect->ymax
		));
		
		GLMPRINTF(( "-D-       src tex layout is %s", srcTex->m_layout->m_layoutSummary ));
		GLMPRINTF(( "-D-       dst tex layout is %s", dstTex->m_layout->m_layoutSummary ));
	}

	int pushed = 0;
	uint pushmask = gl_radar7954721_workaround_maskval.GetInt();
		//GL_COLOR_BUFFER_BIT
		//| GL_CURRENT_BIT
		//| GL_ENABLE_BIT
		//| GL_FOG_BIT
		//| GL_PIXEL_MODE_BIT
		//| GL_SCISSOR_BIT
		//| GL_STENCIL_BUFFER_BIT
		//| GL_TEXTURE_BIT
		//GL_VIEWPORT_BIT
		//;
	
	if (gl_radar7954721_workaround_all.GetInt()!=0)
	{
		gGL->glPushAttrib( pushmask );
		pushed++;
	}
	else
	{
		bool srcGamma = (srcTex->m_layout->m_key.m_texFlags & kGLMTexSRGB) != 0;
		bool dstGamma = (dstTex->m_layout->m_key.m_texFlags & kGLMTexSRGB) != 0;

		if (srcGamma != dstGamma)
		{
			if (gl_radar7954721_workaround_mixed.GetInt())
			{
				gGL->glPushAttrib( pushmask );
				pushed++;
			}
		}
	}

	if (useBlitFB)
	{
		// state we need to save
		//	current setting of scissor
		//	current setting of the drawing fbo (no explicit save, it's in the context)
		GLScissorEnable_t	oldsciss,newsciss;
		m_ScissorEnable.Read( &oldsciss, 0 );

		// remember to restore m_drawingFBO at end of effort
		
		// setup
		//	turn off scissor
		newsciss.enable = false;
		m_ScissorEnable.Write( &newsciss );

		// select which attachment enum we're going to use for the blit
		// default to color0, unless it's a depth or stencil flava
		
		Assert( srcTex->m_layout->m_format->m_glDataFormat == dstTex->m_layout->m_format->m_glDataFormat );
		
		EGLMFBOAttachment	attachIndex = (EGLMFBOAttachment)0;
		GLenum				attachIndexGL = 0;
		GLuint				blitMask = 0;
		switch( srcTex->m_layout->m_format->m_glDataFormat )
		{
			case GL_BGRA:
			case GL_RGB:
			case GL_RGBA:
			case GL_ALPHA:
			case GL_LUMINANCE:
			case GL_LUMINANCE_ALPHA:
				attachIndex = kAttColor0;
				attachIndexGL = GL_COLOR_ATTACHMENT0_EXT;
				blitMask = GL_COLOR_BUFFER_BIT;
			break;

			case GL_DEPTH_COMPONENT:
				attachIndex = kAttDepth;
				attachIndexGL = GL_DEPTH_ATTACHMENT_EXT;
				blitMask = GL_DEPTH_BUFFER_BIT;
			break;
			
			case GL_DEPTH_STENCIL_EXT:
				attachIndex = kAttDepthStencil;
				attachIndexGL = GL_DEPTH_STENCIL_ATTACHMENT_EXT;
				blitMask = GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
			break;

			default:
				Assert(0);
			break;
		}

		//	set the read fb, attach read tex at appropriate attach point, set read buffer
		BindFBOToCtx( m_blitReadFBO, GL_READ_FRAMEBUFFER_EXT );

		GLMFBOTexAttachParams attparams;
		attparams.m_tex		=	srcTex;
		attparams.m_face	=	srcFace;
		attparams.m_mip		=	srcMip;
		attparams.m_zslice	=	0;
		m_blitReadFBO->TexAttach( &attparams, attachIndex, GL_READ_FRAMEBUFFER_EXT );

		gGL->glReadBuffer( attachIndexGL );
		

		//	set the write fb and buffer, and attach write tex
		BindFBOToCtx( m_blitDrawFBO, GL_DRAW_FRAMEBUFFER_EXT );

		attparams.m_tex		=	dstTex;
		attparams.m_face	=	dstFace;
		attparams.m_mip		=	dstMip;
		attparams.m_zslice	=	0;
		m_blitDrawFBO->TexAttach( &attparams, attachIndex, GL_DRAW_FRAMEBUFFER_EXT );

		gGL->glDrawBuffer( attachIndexGL );

		//	do the blit
		gGL->glBlitFramebufferEXT(	srcRect->xmin, srcRect->ymin, srcRect->xmax, srcRect->ymax,
								dstRect->xmin, dstRect->ymin, dstRect->xmax, dstRect->ymax,
								blitMask, filter );
							
		// cleanup
		//	unset the read fb and buffer, detach read tex
		//	unset the write fb and buffer, detach write tex

		m_blitReadFBO->TexDetach( attachIndex, GL_READ_FRAMEBUFFER_EXT );

		m_blitDrawFBO->TexDetach( attachIndex, GL_DRAW_FRAMEBUFFER_EXT );

		//	put the original FB back in place (both read and draw)
		// this bind will hit both read and draw bindings
		BindFBOToCtx( m_drawingFBO, GL_FRAMEBUFFER_EXT );
		
			//	set the read and write buffers back to... what ? does it matter for anything but copies ?  don't worry about it
		
		// restore the scissor state
		m_ScissorEnable.Write( &oldsciss );
	}
	else
	{
		// textured quad style

		// we must attach the dest tex as the color buffer on the blit draw FBO
		// so that means we need to re-set the drawing FBO on exit

		EGLMFBOAttachment	attachIndex = (EGLMFBOAttachment)0;
		GLenum				attachIndexGL = 0;
		switch( srcTex->m_layout->m_format->m_glDataFormat )
		{
			case GL_BGRA:
			case GL_RGB:
			case GL_RGBA:
			case GL_ALPHA:
			case GL_LUMINANCE:
			case GL_LUMINANCE_ALPHA:
				attachIndex = kAttColor0;
				attachIndexGL = GL_COLOR_ATTACHMENT0_EXT;
			break;

			default:
				Assert(!"Can't blit that format");
			break;
		}
		
		BindFBOToCtx( m_blitDrawFBO, GL_DRAW_FRAMEBUFFER_EXT );

		GLMFBOTexAttachParams attparams;
		attparams.m_tex		=	dstTex;
		attparams.m_face	=	dstFace;
		attparams.m_mip		=	dstMip;
		attparams.m_zslice	=	0;
		m_blitDrawFBO->TexAttach( &attparams, attachIndex, GL_DRAW_FRAMEBUFFER_EXT );

		gGL->glDrawBuffer( attachIndexGL );
		
		// attempt to just set states directly the way we want them, then use the latched states to repair them afterward.
		NullProgram();	// out of program mode
		
		gGL->glDisable ( GL_ALPHA_TEST );
		gGL->glDisable ( GL_CULL_FACE );
		gGL->glDisable ( GL_POLYGON_OFFSET_FILL );
		gGL->glDisable ( GL_SCISSOR_TEST );

		gGL->glDisable ( GL_CLIP_PLANE0 );
		gGL->glDisable ( GL_CLIP_PLANE1 );
		
		gGL->glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
		gGL->glDisable ( GL_BLEND );

		gGL->glDepthMask ( GL_FALSE );
		gGL->glDisable ( GL_DEPTH_TEST );

		gGL->glDisable ( GL_STENCIL_TEST );
		gGL->glStencilMask ( GL_FALSE );


		// now do the unlit textured quad...
		gGL->glActiveTexture( GL_TEXTURE0 );
		gGL->glBindTexture( GL_TEXTURE_2D, srcTex->m_texName );

		gGL->glEnable(GL_TEXTURE_2D);

		// immediate mode is fine

		float topv = 1.0;
		float botv = 0.0;
		
		gGL->glBegin(GL_QUADS);
			gGL->glTexCoord2f	( 0.0, botv );
			gGL->glVertex3f		( -1.0, -1.0, 0.0 );
			
			gGL->glTexCoord2f	( 1.0, botv );
			gGL->glVertex3f		( 1.0, -1.0, 0.0 );
			
			gGL->glTexCoord2f	( 1.0, topv );
			gGL->glVertex3f		( 1.0, 1.0, 0.0 );

			gGL->glTexCoord2f	( 0.0, topv );
			gGL->glVertex3f		( -1.0, 1.0, 0.0 );
		gGL->glEnd();

		gGL->glBindTexture( GL_TEXTURE_2D, 0 );

		gGL->glDisable(GL_TEXTURE_2D);

		BindTexToTMU( m_samplers[0].m_pBoundTex, 0 );
		
		// leave active program empty - flush draw states will fix
		
		// then restore states using the scoreboard

		m_AlphaTestEnable.Flush();
		m_AlphaToCoverageEnable.Flush();
		m_CullFaceEnable.Flush();
		m_DepthBias.Flush();
		m_ScissorEnable.Flush();
		
		m_ClipPlaneEnable.FlushIndex( 0 );
		m_ClipPlaneEnable.FlushIndex( 1 );
		
		m_ColorMaskSingle.Flush();
		m_BlendEnable.Flush();

		m_DepthMask.Flush();
		m_DepthTestEnable.Flush();
		
		m_StencilWriteMask.Flush();
		m_StencilTestEnable.Flush();

		//	unset the write fb and buffer, detach write tex

		m_blitDrawFBO->TexDetach( attachIndex, GL_DRAW_FRAMEBUFFER_EXT );

		//	put the original FB back in place (both read and draw)
		BindFBOToCtx( m_drawingFBO, GL_FRAMEBUFFER_EXT );
	}
	
	while(pushed)
	{
		gGL->glPopAttrib();
		pushed--;
	}

	RestoreSavedColorMask();
}

void GLMContext::ResolveTex( CGLMTex *tex, bool forceDirty )
{
#if GL_TELEMETRY_GPU_ZONES
	CScopedGLMPIXEvent glmPIXEvent( "ResolveTex" );
	g_TelemetryGPUStats.m_nTotalResolveTex++;
#endif

	// only run resolve if it's (a) possible and (b) dirty or force-dirtied
	if ( ( tex->m_rboName ) && ( tex->IsRBODirty() || forceDirty ) )
	{
		// state we need to save
		//	current setting of scissor
		//	current setting of the drawing fbo (no explicit save, it's in the context)
		GLScissorEnable_t	oldsciss,newsciss;
		m_ScissorEnable.Read( &oldsciss, 0 );

		// remember to restore m_drawingFBO at end of effort
		
		// setup
		//	turn off scissor
		newsciss.enable = false;
		m_ScissorEnable.Write( &newsciss );

		// select which attachment enum we're going to use for the blit
		// default to color0, unless it's a depth or stencil flava
		
		// for resolve, only handle a modest subset of the possible formats
		EGLMFBOAttachment	attachIndex = (EGLMFBOAttachment)0;
		GLenum				attachIndexGL = 0;
		GLuint				blitMask = 0;
		switch( tex->m_layout->m_format->m_glDataFormat )
		{
			case GL_BGRA:
			case GL_RGB:
			case GL_RGBA:
	//		case GL_ALPHA:
	//		case GL_LUMINANCE:
	//		case GL_LUMINANCE_ALPHA:
				attachIndex = kAttColor0;
				attachIndexGL = GL_COLOR_ATTACHMENT0_EXT;
				blitMask = GL_COLOR_BUFFER_BIT;
			break;

	//		case GL_DEPTH_COMPONENT:
	//			attachIndex = kAttDepth;
	//			attachIndexGL = GL_DEPTH_ATTACHMENT_EXT;
	//			blitMask = GL_DEPTH_BUFFER_BIT;
	//		break;
			
			case GL_DEPTH_STENCIL_EXT:
				attachIndex = kAttDepthStencil;
				attachIndexGL = GL_DEPTH_STENCIL_ATTACHMENT_EXT;
				blitMask = GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
			break;

			default:
				Assert(!"Unsupported format for MSAA resolve" );
			break;
		}

		
		//	set the read fb, attach read RBO at appropriate attach point, set read buffer
		BindFBOToCtx( m_blitReadFBO, GL_READ_FRAMEBUFFER_EXT );

		// going to avoid the TexAttach / TexDetach calls due to potential confusion, implement it directly here
		
		//-----------------------------------------------------------------------------------
		// put tex->m_rboName on the read FB's attachment
		if (attachIndexGL==GL_DEPTH_STENCIL_ATTACHMENT_EXT)
		{
			// you have to attach it both places...
			// http://www.opengl.org/wiki/GL_EXT_framebuffer_object

			// bind the RBO to the GL_RENDERBUFFER_EXT target - is this extraneous ?
			//glBindRenderbufferEXT( GL_RENDERBUFFER_EXT, tex->m_rboName );
			
			// attach the GL_RENDERBUFFER_EXT target to the depth and stencil attach points
			gGL->glFramebufferRenderbufferEXT( GL_READ_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, tex->m_rboName);						
				
			gGL->glFramebufferRenderbufferEXT( GL_READ_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, tex->m_rboName);

			// no need to leave the RBO hanging on
			//glBindRenderbufferEXT( GL_RENDERBUFFER_EXT, 0 );
		}
		else
		{
			//glBindRenderbufferEXT( GL_RENDERBUFFER_EXT, tex->m_rboName );
			
			gGL->glFramebufferRenderbufferEXT( GL_READ_FRAMEBUFFER_EXT, attachIndexGL, GL_RENDERBUFFER_EXT, tex->m_rboName);

			//glBindRenderbufferEXT( GL_RENDERBUFFER_EXT, 0 );
		}

		gGL->glReadBuffer( attachIndexGL );

		//-----------------------------------------------------------------------------------
		// put tex->m_texName on the draw FBO attachment

		//	set the write fb and buffer, and attach write tex
		BindFBOToCtx( m_blitDrawFBO, GL_DRAW_FRAMEBUFFER_EXT );

		// regular path - attaching a texture2d
		
		if (attachIndexGL==GL_DEPTH_STENCIL_ATTACHMENT_EXT)
		{
			gGL->glFramebufferTexture2DEXT( GL_DRAW_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_2D, tex->m_texName, 0 );

			gGL->glFramebufferTexture2DEXT( GL_DRAW_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, GL_TEXTURE_2D, tex->m_texName, 0 );
		}
		else
		{
			gGL->glFramebufferTexture2DEXT( GL_DRAW_FRAMEBUFFER_EXT, attachIndexGL, GL_TEXTURE_2D, tex->m_texName, 0 );
		}

		gGL->glDrawBuffer( attachIndexGL );

		//-----------------------------------------------------------------------------------

		// blit
		gGL->glBlitFramebufferEXT(	0, 0,	tex->m_layout->m_key.m_xSize, tex->m_layout->m_key.m_ySize,
								0, 0,	tex->m_layout->m_key.m_xSize, tex->m_layout->m_key.m_ySize,
								blitMask, GL_NEAREST );
			// or should it be GL_LINEAR?  does it matter ?
			
		//-----------------------------------------------------------------------------------
		// cleanup
		//-----------------------------------------------------------------------------------


		//	unset the read fb and buffer, detach read RBO
		//glBindRenderbufferEXT( GL_RENDERBUFFER_EXT, 0 );

		if (attachIndexGL==GL_DEPTH_STENCIL_ATTACHMENT_EXT)
		{
			// detach the GL_RENDERBUFFER_EXT target from the depth and stencil attach points
			gGL->glFramebufferRenderbufferEXT( GL_READ_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, 0);						
				
			gGL->glFramebufferRenderbufferEXT( GL_READ_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, 0);
		}
		else
		{
			gGL->glFramebufferRenderbufferEXT( GL_READ_FRAMEBUFFER_EXT, attachIndexGL, GL_RENDERBUFFER_EXT, 0);
		}

		//-----------------------------------------------------------------------------------
		//	unset the write fb and buffer, detach write tex
		

		if (attachIndexGL==GL_DEPTH_STENCIL_ATTACHMENT_EXT)
		{
			gGL->glFramebufferTexture2DEXT( GL_DRAW_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_2D, 0, 0 );

			gGL->glFramebufferTexture2DEXT( GL_DRAW_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, GL_TEXTURE_2D, 0, 0 );
		}
		else
		{
			gGL->glFramebufferTexture2DEXT( GL_DRAW_FRAMEBUFFER_EXT, attachIndexGL, GL_TEXTURE_2D, 0, 0 );
		}

		//	put the original FB back in place (both read and draw)
		// this bind will hit both read and draw bindings
		BindFBOToCtx( m_drawingFBO, GL_FRAMEBUFFER_EXT );
		
		//	set the read and write buffers back to... what ? does it matter for anything but copies ?  don't worry about it
		
		// restore the scissor state
		m_ScissorEnable.Write( &oldsciss );
		
		// mark the RBO clean on the resolved tex
		tex->ForceRBONonDirty();
	}
}

void GLMContext::PreloadTex( CGLMTex *tex, bool force )
{
	// if conditions allow (i.e. a drawing surface is active)
	// bind the texture on TMU 15
	// set up a dummy program to sample it but not write (use 'discard')
	// draw a teeny little triangle that won't generate a lot of fragments
	if (!m_pairCache)
		return;
		
	if (!m_drawingFBO)
		return;
			
	if (tex->m_texPreloaded && !force)	// only do one preload unless forced to re-do
	{
		//printf("\nnot-preloading %s", tex->m_debugLabel ? tex->m_debugLabel : "(unknown)");
		return;
	}

	//printf("\npreloading     %s", tex->m_debugLabel ? tex->m_debugLabel : "(unknown)");

	CGLMProgram *vp = m_preloadTexVertexProgram;
	CGLMProgram *fp = NULL;
	switch(tex->m_layout->m_key.m_texGLTarget)
	{
		case GL_TEXTURE_2D:			fp = m_preload2DTexFragmentProgram;
		break;
		
		case GL_TEXTURE_3D:			fp = m_preload3DTexFragmentProgram;
		break;
		
		case GL_TEXTURE_CUBE_MAP:	fp = m_preloadCubeTexFragmentProgram;
		break;
	}
	if (!fp)
		return;
	
	CGLMShaderPair	*preloadPair = m_pairCache->SelectShaderPair( vp, fp, 0 );
	if (!preloadPair)
		return;

	if ( !preloadPair->m_valid )
	{
		if ( !preloadPair->ValidateProgramPair() )
		{
			return;
		}
	}

	gGL->glUseProgram( (GLuint)preloadPair->m_program );
					
	m_pBoundPair = preloadPair;
	m_bDirtyPrograms = true;
			
	// almost ready to draw...

	//int tmuForPreload = 15;
	
	// shut down all the generic attribute arrays on the detention level - next real draw will activate them again
	m_lastKnownVertexAttribMask = 0;
	m_nNumSetVertexAttributes = 16;
	memset( &m_boundVertexAttribs[0], 0xFF, sizeof( m_boundVertexAttribs ) );
	
	// Force the next flush to reset the attributes.
	ClearCurAttribs();

	for( int index=0; index < kGLMVertexAttributeIndexMax; index++ )
	{
		gGL->glDisableVertexAttribArray( index );
	}
		
	// bind texture and sampling params
	CGLMTex *pPrevTex = m_samplers[15].m_pBoundTex;

#ifndef OSX // 10.6
	if ( m_bUseSamplerObjects )
	{
		gGL->glBindSampler( 15, 0 );
	}
#endif // !OSX

	BindTexToTMU( tex, 15 );
	
	// unbind vertex/index buffers
	BindBufferToCtx( kGLMVertexBuffer, NULL );
	BindBufferToCtx( kGLMIndexBuffer, NULL );
			
	// draw
	static float posns[] = {	0.0f, 0.0f, 0.0f,
								0.0f, 0.0f, 0.0f,
								0.0f, 0.0f, 0.0f };

	static int indices[] = { 0, 1, 2 };
	

	gGL->glEnableVertexAttribArray( 0 );

	gGL->glVertexAttribPointer( 0, 3, GL_FLOAT, 0, 0, posns );

	gGL->glDrawRangeElements( GL_TRIANGLES, 0, 3, 3, GL_UNSIGNED_INT, indices);

	gGL->glDisableVertexAttribArray( 0 );
	
	SetSamplerDirty( 15 );

	BindTexToTMU( pPrevTex, 15 );
		
	tex->m_texPreloaded = true;
}



CGLMFBO	*GLMContext::NewFBO( void )
{
	GLM_FUNC;

	CGLMFBO *fbo = new CGLMFBO( this );

	m_fboTable.AddToTail( fbo );
	
	return fbo;
}

void GLMContext::DelFBO( CGLMFBO *fbo )
{
	GLM_FUNC;

	if (m_drawingFBO == fbo)
	{
		m_drawingFBO = NULL;	//poof!
	}
	
	if (m_boundReadFBO == fbo )
	{
		BindFBOToCtx( NULL, GL_READ_FRAMEBUFFER_EXT );
		m_boundReadFBO = NULL;
	}

	if (m_boundDrawFBO == fbo )
	{
		BindFBOToCtx( NULL, GL_DRAW_FRAMEBUFFER_EXT );
		m_boundDrawFBO = NULL;
	}

	int idx = m_fboTable.Find( fbo );
	Assert( idx >= 0 );
	if ( idx >= 0 )
	{
		m_fboTable.FastRemove( idx );
	}
	
	delete fbo;
}

//===============================================================================

CGLMProgram	*GLMContext::NewProgram( EGLMProgramType type, char *progString, const char *pShaderName )
{
	//hushed GLM_FUNC;

	CGLMProgram *prog = new CGLMProgram( this, type );
	
	prog->SetProgramText( progString );
	prog->SetShaderName( pShaderName );
	prog->CompileActiveSources();

	return prog;
}

void GLMContext::DelProgram( CGLMProgram *pProg )
{
	GLM_FUNC;

	if ( m_drawingProgram[ pProg->m_type ] == pProg )
	{
		SetProgram( pProg->m_type, ( pProg->m_type == kGLMFragmentProgram ) ? m_pNullFragmentProgram : NULL );
	}

	// make sure to eliminate any cached pairs using this shader
	bool purgeResult = m_pairCache->PurgePairsWithShader( pProg );
	(void)purgeResult;
	Assert( !purgeResult );	// very unlikely to trigger

	NullProgram();
	
	delete pProg;
}

void GLMContext::NullProgram( void )
{
	gGL->glUseProgram( 0 );
	m_pBoundPair = NULL;
	m_bDirtyPrograms = true;
}

void GLMContext::SetDrawingLang( EGLMProgramLang lang, bool immediate )
{
	if ( !m_caps.m_hasDualShaders ) return;		// ignore attempts to change language when -glmdualshaders is not engaged
	
	m_drawingLangAtFrameStart = lang;
	if (immediate)
	{	
		NullProgram();

		m_drawingLang = m_drawingLangAtFrameStart;
	}
}

void GLMContext::LinkShaderPair( CGLMProgram *vp, CGLMProgram *fp )
{
	if ( (m_pairCache) && (m_drawingLang==kGLMGLSL) && (vp) && (fp) )
	{
		CGLMShaderPair	*pair = m_pairCache->SelectShaderPair( vp, fp, 0 );
		(void)pair;
		
		Assert( pair != NULL );

		NullProgram();	// clear out any binds that were done - next draw will set it right
	}
}

void GLMContext::ValidateShaderPair( CGLMProgram *vp, CGLMProgram *fp )
{
	if ((m_pairCache) && (m_drawingLang == kGLMGLSL) && (vp) && (fp))
	{
		CGLMShaderPair	*pair = m_pairCache->SelectShaderPair( vp, fp, 0 );
		Assert( pair != NULL );
		pair->ValidateProgramPair();

		NullProgram();	// clear out any binds that were done - next draw will set it right
	}
}

void GLMContext::ClearShaderPairCache( void )
{
	if (m_pairCache)
	{
		NullProgram();
		m_pairCache->Purge();	// bye bye all linked pairs
		NullProgram();
	}
}

void GLMContext::QueryShaderPair( int index, GLMShaderPairInfo *infoOut )
{
	if (m_pairCache)
	{
		m_pairCache->QueryShaderPair( index, infoOut );		
	}
	else
	{
		memset( infoOut, 0, sizeof( *infoOut ) );
		infoOut->m_status = -1;
	}
}

CGLMBuffer *GLMContext::NewBuffer( EGLMBufferType type, uint size, uint options )
{
	//hushed GLM_FUNC;

	CGLMBuffer *prog = new CGLMBuffer( this, type, size, options );

	return prog;
}

void GLMContext::DelBuffer( CGLMBuffer *buff )
{
	GLM_FUNC;

	for( int index = 0; index < kGLMVertexAttributeIndexMax; index++ )
	{
		if ( m_drawVertexSetup.m_attrs[index].m_pBuffer == buff )
		{
			// just clear the enable mask - this will force all the attrs to get re-sent on next sync
			m_drawVertexSetup.m_attrMask = 0;
		}
	}
		
	BindGLBufferToCtx( buff->m_buffGLTarget, NULL, false );
			
	delete buff;
}

GLMVertexSetup g_blank_setup;

void GLMContext::Clear( bool color, unsigned long colorValue, bool depth, float depthValue, bool stencil, unsigned int stencilValue, GLScissorBox_t *box )
{
	GLM_FUNC;
		
	++m_nBatchCounter;

#if GLMDEBUG
	GLMDebugHookInfo info;
	memset( &info, 0, sizeof(info) );
	info.m_caller = eClear;
	
	do
	{
#endif
		uint mask = 0;

		GLClearColor_t clearcol;
		GLClearDepth_t cleardep = { depthValue };
		GLClearStencil_t clearsten = { (GLint)stencilValue };

		// depth write mask must be saved&restored
		GLDepthMask_t			olddepthmask;
		GLDepthMask_t			newdepthmask = { true };

		// stencil write mask must be saved and restored
		GLStencilWriteMask_t			oldstenmask;
		GLStencilWriteMask_t			newstenmask = { (GLint)0xFFFFFFFF };
		
		GLColorMaskSingle_t		oldcolormask;
		GLColorMaskSingle_t		newcolormask = { -1,-1,-1,-1 };	// D3D clears do not honor color mask, so force it
		
		if (color)
		{
			// #define D3DCOLOR_ARGB(a,r,g,b) ((D3DCOLOR)((((a)&0xff)<<24)|(((r)&0xff)<<16)|(((g)&0xff)<<8)|((b)&0xff)))

			clearcol.r =	((colorValue >> 16) & 0xFF) / 255.0f;	//R
			clearcol.g =	((colorValue >>  8) & 0xFF) / 255.0f;	//G
			clearcol.b =	((colorValue      ) & 0xFF) / 255.0f;	//B
			clearcol.a =	((colorValue >> 24) & 0xFF) / 255.0f;	//A

			m_ClearColor.Write( &clearcol );	// no check, no wait
			mask |= GL_COLOR_BUFFER_BIT;
			
			// save and set color mask
			m_ColorMaskSingle.Read( &oldcolormask, 0 );			
			m_ColorMaskSingle.Write( &newcolormask );			
		}

		if (depth)
		{
			// get old depth write mask
			m_DepthMask.Read( &olddepthmask, 0 );
			m_DepthMask.Write( &newdepthmask );
			m_ClearDepth.Write( &cleardep );	// no check, no wait
			mask |= GL_DEPTH_BUFFER_BIT;
		}

		if (stencil)
		{
			m_ClearStencil.Write( &clearsten );	// no check, no wait
			mask |= GL_STENCIL_BUFFER_BIT;

			// save and set sten mask
			m_StencilWriteMask.Read( &oldstenmask, 0 );			
			m_StencilWriteMask.Write( &newstenmask );			
		}

		bool subrect = (box != NULL);
		GLScissorEnable_t scissorEnableSave;
		GLScissorEnable_t scissorEnableNew = { true };
		
		GLScissorBox_t scissorBoxSave;
		GLScissorBox_t scissorBoxNew;
		
		if (subrect)
		{
			// save current scissorbox and enable
			m_ScissorEnable.Read( &scissorEnableSave, 0 );
			m_ScissorBox.Read( &scissorBoxSave, 0 );
			
			if(0)
			{
				// calc new scissorbox as intersection against *box

					// max of the mins
				scissorBoxNew.x = MAX(scissorBoxSave.x, box->x);
				scissorBoxNew.y = MAX(scissorBoxSave.y, box->y);
				
					// min of the maxes
				scissorBoxNew.width = ( MIN(scissorBoxSave.x+scissorBoxSave.width, box->x+box->width)) - scissorBoxNew.x;
				
					// height is just min of the max y's, minus the new base Y
				scissorBoxNew.height = ( MIN(scissorBoxSave.y+scissorBoxSave.height, box->y+box->height)) - scissorBoxNew.y;				
			}
			else
			{
				// ignore old scissor box completely.
				scissorBoxNew = *box;
			}
			// set new box and enable
			m_ScissorEnable.Write( &scissorEnableNew );
			m_ScissorBox.Write( &scissorBoxNew );
		}

		gGL->glClear( mask );

		if (subrect)
		{
			// put old scissor box and enable back
			m_ScissorEnable.Write( &scissorEnableSave );
			m_ScissorBox.Write( &scissorBoxSave );
		}
		
		if (depth)
		{
			// put old depth write mask
			m_DepthMask.Write( &olddepthmask );
		}
		
		if (color)
		{
			// put old color write mask
			m_ColorMaskSingle.Write( &oldcolormask );			
		}
		
		if (stencil)
		{
			// put old sten mask
			m_StencilWriteMask.Write( &oldstenmask );			
		}

#if GLMDEBUG
		DebugHook( &info );
	} while (info.m_loop);
#endif
}


// stolen from glmgrbasics.cpp
extern "C" uint GetCurrentKeyModifiers( void );
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

static	ConVar gl_flushpaircache ("gl_flushpaircache", "0");
static	ConVar gl_paircachestats ("gl_paircachestats", "0");
static	ConVar gl_mtglflush_at_tof ("gl_mtglflush_at_tof", "0");
static	ConVar gl_texlayoutstats ("gl_texlayoutstats", "0" );

void GLMContext::BeginFrame( void )
{
	GLM_FUNC;

	m_debugFrameIndex++;
	
	// check for lang change at TOF
	if (m_caps.m_hasDualShaders)
	{
		if (m_drawingLang != m_drawingLangAtFrameStart)
		{
			// language change.  unbind everything..
			NullProgram();

			m_drawingLang = m_drawingLangAtFrameStart;
		}
	}

	// scrub some critical shock absorbers
	for( int i=0; i< 16; i++)
	{
		gGL->glDisableVertexAttribArray( i );						// enable GLSL attribute- this is just client state - will be turned back off
	}
	m_lastKnownVertexAttribMask = 0;
	m_nNumSetVertexAttributes = 0;
	
	//FIXME should we also zap the m_lastKnownAttribs array ? (worst case it just sets them all again on first batch)

	BindBufferToCtx( kGLMVertexBuffer, NULL, true );
	BindBufferToCtx( kGLMIndexBuffer, NULL, true );

	if (gl_flushpaircache.GetInt())
	{
		// do the flush and then set back to zero
		ClearShaderPairCache();
		
		printf("\n\n##### shader pair cache cleared\n\n");
		gl_flushpaircache.SetValue( 0 );
	}
	
	if (gl_paircachestats.GetInt())
	{
		// do the flush and then set back to zero
		m_pairCache->DumpStats();
		
		gl_paircachestats.SetValue( 0 );
	}
	
	if (gl_texlayoutstats.GetInt())
	{
		m_texLayoutTable->DumpStats();
		
		gl_texlayoutstats.SetValue( 0 );
	}
	
	if (gl_mtglflush_at_tof.GetInt())
	{
		gGL->glFlush();									// TOF flush - skip this if benchmarking, enable it if human playing (smoothness)
	}
	
#if GLMDEBUG
	// init debug hook information
	GLMDebugHookInfo info;
	memset( &info, 0, sizeof(info) );
	info.m_caller = eBeginFrame;
	
	do
	{
		DebugHook( &info );
	} while (info.m_loop);

#endif

}

void GLMContext::EndFrame( void )
{
	GLM_FUNC;

#if GLMDEBUG
	// init debug hook information
	GLMDebugHookInfo info;
	memset( &info, 0, sizeof(info) );
	info.m_caller = eEndFrame;
	
	do
	{
		DebugHook( &info );
	} while (info.m_loop);
#endif
}

//===============================================================================

CGLMQuery *GLMContext::NewQuery( GLMQueryParams *params )
{
	CGLMQuery *query = new CGLMQuery( this, params );
	
	return query;
}

void GLMContext::DelQuery( CGLMQuery *query )
{
	// may want to do some finish/
	delete query;
}

static ConVar mat_vsync( "mat_vsync", "0", 0, "Force sync to vertical retrace", true, 0.0, true, 1.0 );

//===============================================================================

ConVar glm_nullrefresh_capslock( "glm_nullrefresh_capslock", "0" );
ConVar glm_literefresh_capslock( "glm_literefresh_capslock", "0" );

extern ConVar gl_blitmode;

void GLMContext::Present( CGLMTex *tex )
{
	GLM_FUNC;
	
	{
#if GL_TELEMETRY_GPU_ZONES
		CScopedGLMPIXEvent glmPIXEvent( "GLMContext::Present" );
		g_TelemetryGPUStats.m_nTotalPresent++;
#endif

		ProcessTextureDeletes();

#ifdef HAVE_GL_ARB_SYNC

		if ( gGL->m_bHave_GL_AMD_pinned_memory )
		{
			m_PinnedMemoryBuffers[m_nCurPinnedMemoryBuffer].InsertFence();
			
			m_nCurPinnedMemoryBuffer = ( m_nCurPinnedMemoryBuffer + 1 ) % cNumPinnedMemoryBuffers;
			
			m_PinnedMemoryBuffers[m_nCurPinnedMemoryBuffer].BlockUntilNotBusy();
			
			gGL->glBindBufferARB( GL_EXTERNAL_VIRTUAL_MEMORY_BUFFER_AMD, m_PinnedMemoryBuffers[m_nCurPinnedMemoryBuffer].GetHandle() );
		}

		if ( gGL->m_bHave_GL_ARB_buffer_storage )
		{
			for (uint lpType = 0; lpType < kGLMNumBufferTypes; ++lpType)
			{
				m_persistentBuffer[m_nCurPersistentBuffer][lpType].InsertFence();
			}

			m_nCurPersistentBuffer = ( m_nCurPersistentBuffer + 1 ) % cNumPersistentBuffers;

			for (uint lpType = 0; lpType < kGLMNumBufferTypes; ++lpType)
			{
				m_persistentBuffer[m_nCurPersistentBuffer][lpType].BlockUntilNotBusy();
			}
		}

#endif // HAVE_GL_ARB_SYNC
					
		bool newRefreshMode = false;
		// two ways to go:
	
		// old school, do the resolve, had the tex down to cocoamgr to actually blit.
		// that way is required if you are not in one-context mode (10.5.8)
	
		if ( (gl_blitmode.GetInt() != 0) )
		{
			newRefreshMode = true;
		}
	
		// this is the path whether full screen or windowed... we always blit.
		CShowPixelsParams showparams;
		memset( &showparams, 0, sizeof(showparams) );

		showparams.m_srcTexName	= tex->m_texName;
		showparams.m_width		= tex->m_layout->m_key.m_xSize;
		showparams.m_height		= tex->m_layout->m_key.m_ySize;
		showparams.m_vsyncEnable = m_displayParams.m_vsyncEnable = mat_vsync.GetBool();
		showparams.m_fsEnable	= m_displayParams.m_fsEnable;
		showparams.m_useBlit	= m_caps.m_hasFramebufferBlit;

		// we call showpixels once with the "only sync view" arg set, so we know what the latest surface size is, before trying to do our own blit !
		showparams.m_onlySyncView = true;
		ShowPixels(&showparams);	// doesn't actually show anything, just syncs window/fs state (would make a useful separate call)
		showparams.m_onlySyncView = false;
	
		bool refresh = true;
	#ifdef OSX
		if ( (glm_nullrefresh_capslock.GetInt()) && (GetCurrentKeyModifiers() & EalphaLock) )
		{
			refresh = false;
		}
	#endif	
		static int counter;
		counter ++;

	#ifdef OSX
		if ( (glm_literefresh_capslock.GetInt()) && (GetCurrentKeyModifiers() & EalphaLock) && (counter & 127) )
		{
			// just show every 128th frame
			refresh = false;
		}
	#endif

		if (refresh)
		{
			if (newRefreshMode)
			{
				// blit to GL_BACK done here, not in CocoaMgr, this lets us do resolve directly if conditions are right

				GLMRect	srcRect, dstRect;
			
				uint dstWidth,dstHeight;
				DisplayedSize( dstWidth,dstHeight );

				srcRect.xmin	=	0;
				srcRect.ymin	=	0;
				srcRect.xmax	=	showparams.m_width;
				srcRect.ymax	=	showparams.m_height;

				dstRect.xmin	=	0;
				dstRect.ymin	=	0;
				dstRect.xmax	=	dstWidth;
				dstRect.ymax	=	dstHeight;
			
				// do not ask for LINEAR if blit is unscaled
				// NULL means targeting GL_BACK.  Blit2 will break it down into two steps if needed, and will handle resolve, scale, flip.
				bool blitScales	=	(showparams.m_width != static_cast<int>(dstWidth)) || (showparams.m_height != static_cast<int>(dstHeight));
				Blit2(	tex, &srcRect, 0,0,
								NULL, &dstRect, 0,0,
								blitScales ? GL_LINEAR : GL_NEAREST );

				// we set showparams.m_noBlit, and just let CocoaMgr handle the swap (flushbuffer / page flip)
				showparams.m_noBlit = true;

				BindFBOToCtx( NULL, GL_FRAMEBUFFER_EXT );
			}
			else
			{
				ResolveTex( tex, true );	// dxabstract used to do this unconditionally.we still do if new refresh mode doesn't engage.

				BindFBOToCtx( NULL, GL_FRAMEBUFFER_EXT );

				// showparams.m_noBlit is left set to 0.  CocoaMgr does the blit.
			}

			ShowPixels(&showparams);
		}

		//	put the original FB back in place (both read and draw)
		// this bind will hit both read and draw bindings
		BindFBOToCtx( m_drawingFBO, GL_FRAMEBUFFER_EXT );

		// put em back !!
		m_ScissorEnable.Flush();	
		m_ScissorBox.Flush();
		m_ViewportBox.Flush();		
	}

	m_nCurFrame++;

#if GL_BATCH_PERF_ANALYSIS
	tmMessage( TELEMETRY_LEVEL2, TMMF_ICON_EXCLAMATION, "VS Uniform Calls: %u, VS Uniforms: %u|VS Uniform Bone Calls: %u, VS Bone Uniforms: %u|PS Uniform Calls: %u, PS Uniforms: %u", m_nTotalVSUniformCalls, m_nTotalVSUniformsSet, m_nTotalVSUniformBoneCalls, m_nTotalVSUniformsBoneSet, m_nTotalPSUniformCalls, m_nTotalPSUniformsSet );
	m_nTotalVSUniformCalls = 0, m_nTotalVSUniformBoneCalls = 0, m_nTotalVSUniformsSet = 0, m_nTotalVSUniformsBoneSet = 0, m_nTotalPSUniformCalls = 0, m_nTotalPSUniformsSet = 0;
#endif

#ifndef OSX
	GLMGPUTimestampManagerTick();
#endif
}

//===============================================================================
// GLMContext protected methods

// a naive implementation of this would just clear-drawable on the context at entry,
// and then capture and set fullscreen if requested.
// however that would glitch thescreen every time the user changed resolution while staying in full screen.
// but in windowed mode there's really not much to do in here.  Yeah, this routine centers around obtaining
// drawables for fullscreen mode, and/or dropping those drawables if we're going back to windowed.

// um, are we expected to re-make the standard surfaces (color, depthstencil) if the res changes?  is that now this routine's job ?

// so, kick it off with an assessment of whather we were FS previously or not.
// if there was no prior display params latched, then it wasn't.

// changes in here take place immediately.  If you want to defer display changes then that's going to be a different method.
// common assumption is that there will be two places that call this: context create and the implementation of the DX9 Reset method.
// in either case the client code is aware of what it signed up for.

bool GLMContext::SetDisplayParams( GLMDisplayParams *params )
{	
	m_displayParams = *params;	// latch em
	m_displayParamsValid = true;

	return true;
}


ConVar gl_can_query_fast("gl_can_query_fast", "0");

static uint gPersistentBufferSize[kGLMNumBufferTypes] = 
{
	2 * 1024 * 1024,	// kGLMVertexBuffer
	1 * 1024 * 1024,	// kGLMIndexBuffer
	0,					// kGLMUniformBuffer
	0,					// kGLMPixelBuffer
};

GLMContext::GLMContext( IDirect3DDevice9 *pDevice, GLMDisplayParams *params )
{
// 	m_bUseSamplerObjects = true;
// 	
// 	// On most AMD drivers (like the current latest, 12.10 Windows), the PCF depth comparison mode doesn't work on sampler objects, so just punt them.
// 	if ( gGL->m_nDriverProvider == cGLDriverProviderAMD )
// 	{
// 		m_bUseSamplerObjects = false;
// 	}
	
// 	if ( CommandLine()->CheckParm( "-gl_disablesamplerobjects" ) )
// 	{
	// Disable sampler object usage for now since ScaleForm isn't aware of them
	// and doesn't know how to push/pop their binding state. It seems we don't
	// really use them in this codebase anyhow, except to preload textures.
	m_bUseSamplerObjects = false;
	if ( CommandLine()->CheckParm( "-gl_enablesamplerobjects" ) )
		m_bUseSamplerObjects = true;

	// Try to get some more free memory by relying on driver host copies instead of ours.
	//  In some cases the driver will be able to discard their own host copy and rely on GPU
	//  memory, reducing memory usage.
	// Sadly, we have to enable tex client storage for srgb decoding. This should only happen
	//  on Macs w/ OSX 10.6.
	m_bTexClientStorage = !gGL->m_bHave_GL_EXT_texture_sRGB_decode;
	if ( CommandLine()->CheckParm( "-gl_texclientstorage" ) )
		m_bTexClientStorage = true;

	GLMDebugPrintf( "GL sampler object usage: %s\n", m_bUseSamplerObjects ? "ENABLED" : "DISABLED" );

	m_nCurOwnerThreadId = ThreadGetCurrentId();
	m_nThreadOwnershipReleaseCounter = 0;

	m_pDevice = pDevice;
	m_nCurFrame = 0;
	m_nBatchCounter = 0;

	ClearCurAttribs();

#ifndef OSX
	m_nCurPinnedMemoryBuffer = 0;
	if ( gGL->m_bHave_GL_AMD_pinned_memory )
	{
		for ( uint t = 0; t < cNumPinnedMemoryBuffers; t++ )
		{
			m_PinnedMemoryBuffers[t].Init( GLMGR_PINNED_MEMORY_BUFFER_SIZE );
		}

		gGL->glBindBufferARB( GL_EXTERNAL_VIRTUAL_MEMORY_BUFFER_AMD, m_PinnedMemoryBuffers[m_nCurPinnedMemoryBuffer].GetHandle() );
	}
#endif // OSX

	m_nCurPersistentBuffer = 0;
	if ( gGL->m_bHave_GL_ARB_buffer_storage )
	{
		for ( uint lpType = 0; lpType < kGLMNumBufferTypes; ++lpType )
		{
			for ( uint lpNum = 0; lpNum < cNumPersistentBuffers; ++lpNum )
			{
				m_persistentBuffer[lpNum][lpType].Init( (EGLMBufferType)lpType, gPersistentBufferSize[lpType] );
			}
		}
	}

	m_bUseBoneUniformBuffers = true;
	if (CommandLine()->CheckParm("-disableboneuniformbuffers"))
	{
		m_bUseBoneUniformBuffers = false;
	}

	m_nMaxUsedVertexProgramConstantsHint = 256;

	// flag our copy of display params as blank
	m_displayParamsValid = false;

	// peek at any CLI options
	m_slowAssertEnable = CommandLine()->FindParm("-glmassertslow") != 0;
	m_slowSpewEnable = CommandLine()->FindParm("-glmspewslow") != 0;
	m_checkglErrorsAfterEveryBatch = CommandLine()->FindParm("-glcheckerrors") != 0;
	m_slowCheckEnable = m_slowAssertEnable || m_slowSpewEnable || m_checkglErrorsAfterEveryBatch;

	m_drawingLangAtFrameStart = m_drawingLang = kGLMGLSL;		// default to GLSL
	
	// this affects FlushDrawStates which will route program bindings, uniform delivery, sampler setup, and enables accordingly.

	if ( CommandLine()->FindParm("-glslmode") )
	{
		m_drawingLangAtFrameStart = m_drawingLang = kGLMGLSL;
	}
	if ( CommandLine()->FindParm("-arbmode") && !CommandLine()->FindParm("-glslcontrolflow") )
	{
		m_drawingLangAtFrameStart = m_drawingLang = kGLMARB;
	}

	// proceed with rest of init
	
	m_dwRenderThreadId = 0;
	m_bIsThreading = false;
	
	m_nsctx	= NULL;
	m_ctx	= NULL;
	
	int *selAttribs	=	NULL;
	uint					selWords	=	0;

	memset( &m_caps, 0, sizeof( m_caps ) );
	GetDesiredPixelFormatAttribsAndRendererInfo( (uint**)&selAttribs, &selWords, &m_caps );
	uint selBytes = selWords * sizeof( uint ); selBytes;

#if defined( USE_SDL )
	m_ctx = (SDL_GLContext)GetGLContextForWindow( params ? (void*)params->m_focusWindow : NULL );
	MakeCurrent( true );
#else
#error
#endif
	IncrementWindowRefCount();

	// If we're using GL_ARB_debug_output, go ahead and setup the callback here.
	if ( gGL->m_bHave_GL_ARB_debug_output && CommandLine()->FindParm( "-gl_debug" ) ) 
	{
#if GLMDEBUG
		// Turning this on is a perf loss, but it ensures that you can (at least) swap to the other 
		// threads to see what call is currently being made.
		// Note that if the driver is in multithreaded mode, you can put it back into singlethreaded mode
		// and get a real stack for the offending gl call.
		gGL->glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB);

#ifdef WIN32
		// This happens early enough during init that DevMsg() does nothing.
		OutputDebugStringA( "GLMContext::GLMContext: GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB enabled!\n" );
#else
		printf( "GLMContext::GLMContext: GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB enabled!\n" );
#endif

#endif
		// This should be there if we get in here--make sure.
		Assert(gGL->glDebugMessageControlARB);
		gGL->glDebugMessageControlARB(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, (const GLuint *)NULL, GL_TRUE);

		// Gonna filter these out, they're "chatty".
		gGL->glDebugMessageControlARB(GL_DEBUG_SOURCE_API_ARB, GL_DEBUG_TYPE_OTHER_ARB, GL_DEBUG_SEVERITY_LOW_ARB, 0, (const GLuint *)NULL, GL_FALSE);
		gGL->glDebugMessageCallbackARB(GL_Debug_Output_Callback, (void*)NULL);

		GLMDebugPrintf( "GLMContext::GLMContext: Debug output (gl_arb_debug_output) enabled!\n" );
	}


	if (CommandLine()->FindParm("-glmspewcaps"))
	{
		DumpCaps();
	}
	
	SetDisplayParams( params );

	m_texLayoutTable = new CGLMTexLayoutTable;

#ifndef OSX
	if ( m_bUseSamplerObjects )
	{
		memset( m_samplerObjectHash, 0, sizeof( m_samplerObjectHash ) );
		m_nSamplerObjectHashNumEntries = 0;
	
		for ( uint i = 0; i < cSamplerObjectHashSize; ++i )
		{
			gGL->glGenSamplers( 1, &m_samplerObjectHash[i].m_samplerObject );
		}
	}
#endif // !OSX

	memset( m_samplers, 0, sizeof( m_samplers ) );
	for( int i=0; i< GLM_SAMPLER_COUNT; i++)
	{
		GLMTexSamplingParams &params = m_samplers[i].m_samp;
		params.m_packed.m_addressU = D3DTADDRESS_WRAP;
		params.m_packed.m_addressV = D3DTADDRESS_WRAP;
		params.m_packed.m_addressW = D3DTADDRESS_WRAP;
		params.m_packed.m_minFilter = D3DTEXF_POINT;
		params.m_packed.m_magFilter = D3DTEXF_POINT;
		params.m_packed.m_mipFilter = D3DTEXF_NONE;
		params.m_packed.m_maxAniso = 1;
		params.m_packed.m_isValid = true;
		params.m_packed.m_compareMode = 0;
	}
	
	MarkAllSamplersDirty();
	
	m_activeTexture = -1;
					
	m_texLocks.EnsureCapacity( 16 );	// should be sufficient

	// FIXME need a texture tracking table so we can reliably delete CGLMTex objects at context teardown
		
	m_boundReadFBO = NULL;
	m_boundDrawFBO = NULL;
	m_drawingFBO = NULL;
											
	memset( m_drawingProgram, 0, sizeof( m_drawingProgram ) );
	m_bDirtyPrograms = true;
	memset( m_programParamsF , 0, sizeof( m_programParamsF ) );
	memset( m_programParamsB , 0, sizeof( m_programParamsB ) );
	memset( m_programParamsI , 0, sizeof( m_programParamsI ) );

	for (uint i = 0; i < ARRAYSIZE(m_programParamsF); i++)
	{
		m_programParamsF[i].m_firstDirtySlotNonBone = 256;
		m_programParamsF[i].m_dirtySlotHighWaterNonBone = 0;

		m_programParamsF[i].m_dirtySlotHighWaterBone = 0;
	}

	m_paramWriteMode = eParamWriteDirtySlotRange;	// default to fastest mode
	
	if (CommandLine()->FindParm("-glmwriteallslots"))				m_paramWriteMode = eParamWriteAllSlots;
	if (CommandLine()->FindParm("-glmwriteshaderslots"))			m_paramWriteMode = eParamWriteShaderSlots;
	if (CommandLine()->FindParm("-glmwriteshaderslotsoptional"))	m_paramWriteMode = eParamWriteShaderSlotsOptional;
	if (CommandLine()->FindParm("-glmwritedirtyslotrange"))			m_paramWriteMode = eParamWriteDirtySlotRange;
	
	m_attribWriteMode = eAttribWriteDirty;

	if (CommandLine()->FindParm("-glmwriteallattribs"))				m_attribWriteMode = eAttribWriteAll;
	if (CommandLine()->FindParm("-glmwritedirtyattribs"))			m_attribWriteMode = eAttribWriteDirty;	

	m_pairCache	= new CGLMShaderPairCache( this );
	m_pBoundPair = NULL;

	m_fragDataMask = 0;
			
	memset( m_nBoundGLBuffer, 0xFF, sizeof( m_nBoundGLBuffer ) );

	memset( m_boundVertexAttribs, 0xFF, sizeof(m_boundVertexAttribs) );
	m_lastKnownVertexAttribMask = 0;
	m_nNumSetVertexAttributes = 16;

	// make a null program for use when client asks for NULL FP
	m_pNullFragmentProgram = NewProgram(kGLMFragmentProgram, g_nullFragmentProgramText, "null" );
	SetProgram( kGLMFragmentProgram, m_pNullFragmentProgram );

	// make dummy programs for doing texture preload via dummy draw
	m_preloadTexVertexProgram		=	NewProgram(kGLMVertexProgram, g_preloadTexVertexProgramText, "preloadTex" );
	m_preload2DTexFragmentProgram	=	NewProgram(kGLMFragmentProgram, g_preload2DTexFragmentProgramText, "preload2DTex" );
	m_preload3DTexFragmentProgram	=	NewProgram(kGLMFragmentProgram, g_preload3DTexFragmentProgramText, "preload3DTex" );
	m_preloadCubeTexFragmentProgram	=	NewProgram(kGLMFragmentProgram, g_preloadCubeTexFragmentProgramText, "preloadCube" );
		
	//memset( &m_drawVertexSetup, 0, sizeof(m_drawVertexSetup) );
	SetVertexAttributes( NULL );	// will set up all the entries in m_drawVertexSetup
	
	m_debugFontTex = NULL;

	// debug state
	m_debugFrameIndex = -1;
	
#if GLMDEBUG
	// #######################################################################################

	// DebugHook state - we could set these to more interesting values in response to a CLI arg like "startpaused" or something if desired
	//m_paused = false;
	m_holdFrameBegin = -1;
	m_holdFrameEnd = -1;
	m_holdBatch = m_holdBatchFrame = -1;
	
	m_debugDelayEnable = false;
	m_debugDelay = 1<<19;	// ~0.5 sec delay

	m_autoClearColor = m_autoClearDepth = m_autoClearStencil = false;
	m_autoClearColorValues[0] = 0.0;	//red
	m_autoClearColorValues[1] = 1.0;	//green
	m_autoClearColorValues[2] = 0.0;	//blue
	m_autoClearColorValues[3] = 1.0;	//alpha
	
	m_selKnobIndex = 0;
	m_selKnobMinValue = -10.0f;

	m_selKnobMaxValue = 10.0f;
	m_selKnobIncrement = 1/256.0f;

	// #######################################################################################
#endif

	// make two scratch FBO's for blit purposes
	m_blitReadFBO = NewFBO();
	m_blitDrawFBO = NewFBO();

	for( int i=0; i<kGLMScratchFBOCount; i++)
	{
		m_scratchFBO[i] = NewFBO();
	}

	// Create a PBO that we can use to fill textures with bogus data asyncronously.
	m_nBoundGLBuffer[ kGLMPixelBuffer ] = 0;

	gGL->glGenBuffersARB( 1, &m_destroyPBO );
	gGL->glBindBufferARB( GL_PIXEL_UNPACK_BUFFER_ARB, m_destroyPBO );
	gGL->glBufferDataARB( GL_PIXEL_UNPACK_BUFFER_ARB, sizeof( g_garbageTextureBits ), g_garbageTextureBits, GL_STATIC_DRAW );
	gGL->glBindBufferARB( GL_PIXEL_UNPACK_BUFFER_ARB, m_nBoundGLBuffer[ kGLMPixelBuffer ] );

	// Create a bunch of texture names for us to use forever and ever ramen.
	FillTexCache( false, kGLMInitialTexCount );

#ifdef OSX
	bool new_mtgl = m_caps.m_hasPerfPackage1;	// i.e. 10.6.4 plus new driver
	
	if ( CommandLine()->FindParm("-glmenablemtgl2") )
	{
		new_mtgl = true;
	}

	if ( CommandLine()->FindParm("-glmdisablemtgl2") )
	{
		new_mtgl = false;
	}

	bool mtgl_on = params->m_mtgl;
	if (CommandLine()->FindParm("-glmenablemtgl"))
	{
		mtgl_on = true;
	}
	
	if (CommandLine()->FindParm("-glmdisablemtgl"))
	{
		mtgl_on = false;
	}

	CGLError result = (CGLError)0;
	if (mtgl_on)
	{
		bool ready = false;
		CGLContextObj context = GetCGLContextFromNSGL(m_ctx);
		if (new_mtgl)
		{
			// afterburner
			CGLContextEnable kCGLCPGCDMPEngine = ((CGLContextEnable)1314);
			result = CGLEnable( context, kCGLCPGCDMPEngine );
			if (!result)
			{
				ready = true;	// succeeded - no need to try non-MTGL
				printf("\nMTGL detected.\n");
			}
			else
			{
				printf("\nMTGL *not* detected, falling back.\n");
			}
		}

		if (!ready)
		{
			// try old MTGL
			result = CGLEnable( context, kCGLCEMPEngine );
			if (!result)
			{
				printf("\nMTGL has been detected.\n");
				ready = true;	// succeeded - no need to try non-MTGL
			}
		}
	}

	if ( m_caps.m_badDriver108Intel )
	{
		// this way we have something to look for in terminal spew if users report issues related to this in the future.
		printf( "\nEnabling GLSL compiler `malloc' workaround.\n" );
		if ( !IntelGLMallocWorkaround::Get()->Enable() )
		{
			Warning( "Unable to enable OSX 10.8 / Intel HD4000 workaround, there might be crashes.\n" );
		}
	}

#endif
	// also, set the remote convar "gl_can_query_fast" to 1 if perf package present, else 0.
	gl_can_query_fast.SetValue( m_caps.m_hasPerfPackage1?1:0 );

#if GL_BATCH_PERF_ANALYSIS		
	m_nTotalVSUniformCalls = 0;
	m_nTotalVSUniformBoneCalls = 0;
	m_nTotalVSUniformsSet = 0;
	m_nTotalVSUniformsBoneSet = 0;
	m_nTotalPSUniformCalls = 0;
	m_nTotalPSUniformsSet = 0;
#endif

	// See g_D3DRS_INFO_packed in dxabstract.cpp; dithering is a non-managed
	// piece of state that we consider off by default. However it is actually
	// enabled by default in the GL spec, so account for that here.
	// See: https://bugs.freedesktop.org/show_bug.cgi?id=74700
	gGL->glDisable( GL_DITHER );
}

void GLMContext::Reset()
{
}

GLMContext::~GLMContext	()
{
#ifndef OSX
	GLMGPUTimestampManagerDeinit();
		
	for ( uint t = 0; t < cNumPinnedMemoryBuffers; t++ )
	{
		m_PinnedMemoryBuffers[t].Deinit();
	}

	if (gGL->m_bHave_GL_ARB_buffer_storage)
	{
		for (uint lpType = 0; lpType < kGLMNumBufferTypes; ++lpType)
		{
			for (uint lpNum = 0; lpNum < cNumPersistentBuffers; ++lpNum)
			{
				m_persistentBuffer[lpNum][lpType].Deinit();
			}
		}
	}

	if ( m_bUseSamplerObjects )
	{
		for( int i=0; i< GLM_SAMPLER_COUNT; i++)
		{
			gGL->glBindSampler( i, 0 );
		}

		for( int i=0; i< cSamplerObjectHashSize; i++)
		{
			gGL->glDeleteSamplers( 1, &m_samplerObjectHash[i].m_samplerObject );
			m_samplerObjectHash[i].m_samplerObject = 0;
		}
	}
#endif // !OSX

	if (m_debugFontTex)
	{
		DelTex( m_debugFontTex );
		m_debugFontTex = NULL;
	}

	ProcessTextureDeletes();

	if ( m_pNullFragmentProgram )
	{
		DelProgram( m_pNullFragmentProgram );
		m_pNullFragmentProgram = NULL;
	}
	
	// walk m_fboTable and free them up..
	FOR_EACH_VEC( m_fboTable, i )
	{
		CGLMFBO *fbo = m_fboTable[i];
		DelFBO( fbo );
	}
	m_fboTable.SetSize( 0 );

	if (m_pairCache)
	{
		delete m_pairCache;
		m_pairCache = NULL;
	}
	
	// we need a m_texTable I think..

	// m_texLayoutTable can be scrubbed once we know that all the tex are freed

	gGL->glDeleteBuffersARB( 1, &m_destroyPBO );

	PurgeTexCache();

	DecrementWindowRefCount();
}

// This method must call SelectTMU()/glActiveTexture() (it's expected as a side effect).
// This method is no longer called from any performance sensitive code paths.
void GLMContext::BindTexToTMU( CGLMTex *pTex, int tmu )
{
#if GLMDEBUG
	GLM_FUNC;
#endif

	GLMPRINTF(("--- GLMContext::BindTexToTMU tex %p GL name %d -> TMU %d ", pTex, pTex ? pTex->m_texName : -1, tmu ));

	CheckCurrent();
		
	SelectTMU( tmu );
		
	if ( !pTex )
	{
		gGL->glBindTexture( GL_TEXTURE_1D, 0 );
		gGL->glBindTexture( GL_TEXTURE_2D, 0 );
		gGL->glBindTexture( GL_TEXTURE_3D, 0 );
		gGL->glBindTexture( GL_TEXTURE_CUBE_MAP, 0 );
	}
	else
	{
		const GLenum texGLTarget = pTex->m_texGLTarget;
		if ( texGLTarget != GL_TEXTURE_1D ) gGL->glBindTexture( GL_TEXTURE_1D, 0 );
		if ( texGLTarget != GL_TEXTURE_2D ) gGL->glBindTexture( GL_TEXTURE_2D, 0 );
		if ( texGLTarget != GL_TEXTURE_3D ) gGL->glBindTexture( GL_TEXTURE_3D, 0 );
		if ( texGLTarget != GL_TEXTURE_CUBE_MAP ) gGL->glBindTexture( GL_TEXTURE_CUBE_MAP, 0 );
		gGL->glBindTexture( texGLTarget, pTex->m_texName );
	}

	m_samplers[tmu].m_pBoundTex = pTex;
}

void GLMContext::BindFBOToCtx( CGLMFBO *fbo, GLenum bindPoint )
{
#if GLMDEBUG
	GLM_FUNC;
#endif
	GLMPRINTF(( "--- GLMContext::BindFBOToCtx fbo %p, GL name %d", fbo, (fbo) ? fbo->m_name : -1 ));

	CheckCurrent();

	if ( bindPoint == GL_FRAMEBUFFER_EXT )
	{
		gGL->glBindFramebufferEXT( GL_FRAMEBUFFER_EXT, fbo ? fbo->m_name : 0 );
		m_boundReadFBO = fbo;
		m_boundDrawFBO = fbo;
		return;
	}
	
	bool	targetRead = (bindPoint==GL_READ_FRAMEBUFFER_EXT);
	bool	targetDraw = (bindPoint==GL_DRAW_FRAMEBUFFER_EXT);
			
	if (targetRead)
	{
		if (fbo)	// you can pass NULL to go back to no-FBO
		{
			gGL->glBindFramebufferEXT( GL_READ_FRAMEBUFFER_EXT, fbo->m_name );
			
			m_boundReadFBO = fbo;
			//dontcare fbo->m_bound = true;
		}
		else
		{
			gGL->glBindFramebufferEXT( GL_READ_FRAMEBUFFER_EXT, 0 );
			
			m_boundReadFBO = NULL;
		}
	}
	
	if (targetDraw)
	{
		if (fbo)	// you can pass NULL to go back to no-FBO
		{
			gGL->glBindFramebufferEXT( GL_DRAW_FRAMEBUFFER_EXT, fbo->m_name );
			
			m_boundDrawFBO = fbo;
			//dontcare fbo->m_bound = true;
		}
		else
		{
			gGL->glBindFramebufferEXT( GL_DRAW_FRAMEBUFFER_EXT, 0 );
			
			m_boundDrawFBO = NULL;
		}
	}
}

void GLMContext::BindBufferToCtx( EGLMBufferType type, CGLMBuffer *pBuff, bool bForce )
{
#if GLMDEBUG
	GLM_FUNC;
#endif
	GLMPRINTF(( "--- GLMContext::BindBufferToCtx buff %p, GL name %d", pBuff, (pBuff) ? pBuff->m_nHandle : -1 ));

	CheckCurrent();

	GLuint nGLName = pBuff ? pBuff->GetHandle() : 0;
	if ( !bForce ) 
	{
		if ( m_nBoundGLBuffer[type] == nGLName )
			return;
	}
	
	GLenum target = 0;
	switch( type )
	{	
		case kGLMVertexBuffer:		target = GL_ARRAY_BUFFER_ARB;			break;
		case kGLMIndexBuffer:		target = GL_ELEMENT_ARRAY_BUFFER_ARB;	break;
		case kGLMUniformBuffer:		target = GL_UNIFORM_BUFFER_EXT;			break;
		case kGLMPixelBuffer:		target = GL_PIXEL_UNPACK_BUFFER_ARB;	break;
		default: Assert(!"Unknown buffer type" );
	}
			
	Assert( !pBuff || ( pBuff->m_buffGLTarget == target ) );
			
	m_nBoundGLBuffer[type] = nGLName;
	gGL->glBindBufferARB( target, nGLName );
}


GLuint GLMContext::CreateTex( GLenum texBind, GLenum internalFormat )
{
	GLM_FUNC;

	// If we're not doing batch create, just return one here.
	if ( !gl_batch_tex_creates.GetBool() )
	{
		GLuint tex = 0;
		gGL->glGenTextures( 1, &tex );
		return tex;
	}

	FOR_EACH_VEC( m_availableTextures, i )
	{
		TextureEntry_t& tex = m_availableTextures[ i ];
		if (    ( tex.m_nTexBind == GL_NONE || tex.m_nTexBind == texBind  )
			 && ( tex.m_nInternalFormat == GL_NONE || tex.m_nInternalFormat == internalFormat ) )
		{
			// Hit!
			GLuint retVal = tex.m_nTexName;
			m_availableTextures.Remove( i );
			return retVal;
		}
	}

	if ( m_availableTextures.Count() >= kGLMHighWaterUndeleted )
	{
		PurgeTexCache();
	}

	return FillTexCache( true, kGLMReUpTexCount );
}

void GLMContext::CleanupTex( GLenum texBind, GLMTexLayout* pLayout, GLuint tex )
{
	// If the total 
	if ( pLayout->m_storageTotalSize <= ( kDeletedTextureDim * kDeletedTextureDim * sizeof( uint32 ) ) )
		return;

	const GLuint oldPBO = m_nBoundGLBuffer[ kGLMPixelBuffer ];
	const GLuint oldTex = ( m_samplers[ m_activeTexture ].m_pBoundTex != NULL ) ? m_samplers[ m_activeTexture ].m_pBoundTex->GetTexName() : 0;

	gGL->glBindBufferARB( GL_PIXEL_UNPACK_BUFFER_ARB, m_destroyPBO );
	gGL->glBindTexture( texBind, tex );

	// Clear out old data.
	for ( int i = 0; i < pLayout->m_mipCount; ++i )
	{
		int mipDim = ( i == 0 ) ? kDeletedTextureDim : 0;
		if ( pLayout->m_format->m_chunkSize != 1 )
		{
			const int chunks = ( mipDim + ( pLayout->m_format->m_chunkSize - 1 ) ) / pLayout->m_format->m_chunkSize;
			const int dataSize = ( chunks * chunks ) * pLayout->m_format->m_bytesPerSquareChunk;
			Assert( dataSize <= ( sizeof( uint32) * ARRAYSIZE( g_garbageTextureBits ) ) );

			gGL->glCompressedTexImage2D( texBind, i, pLayout->m_format->m_glIntFormat, mipDim, mipDim, 0, dataSize, 0 );
		}
		else
		{
			gGL->glTexImage2D( texBind, i, pLayout->m_format->m_glIntFormat, mipDim, mipDim, 0, pLayout->m_format->m_glDataFormat, pLayout->m_format->m_glDataType, 0 );
		}
	}

	gGL->glBindTexture( texBind, oldTex );
	gGL->glBindBufferARB( GL_PIXEL_UNPACK_BUFFER_ARB, oldPBO );
}

void GLMContext::DestroyTex( GLenum texBind, GLMTexLayout* pLayout, GLuint tex )
{
	GLM_FUNC;

	// Code only handles 2D for now.
	if ( texBind != GL_TEXTURE_2D || !gl_batch_tex_destroys.GetBool() )
	{
		gGL->glDeleteTextures( 1, &tex );
		return;
	}
	
	CleanupTex( texBind, pLayout, tex );

	TextureEntry_t entry;
	entry.m_nTexBind = texBind;
	entry.m_nInternalFormat = pLayout->m_format->m_glIntFormat;
	entry.m_nTexName = tex;

	m_availableTextures.AddToTail( entry );
}

GLuint GLMContext::FillTexCache( bool holdOne, int newTextures )
{
	// If we aren't doing batch creates, then don't fill the cache. 
	if ( !gl_batch_tex_creates.GetBool() )
		return 0;
	
	// If we have to hit the name table, might as well hit it a bunch because this causes 
	// serialization either way--at least we can do it less often.
	GLuint* textures = (GLuint*) stackalloc( newTextures * sizeof( GLuint ) );
	gGL->glGenTextures( newTextures, textures );

	Assert( textures[ 0 ] );

	TextureEntry_t entry;
	entry.m_nTexBind = GL_NONE;
	entry.m_nInternalFormat = GL_NONE;

	// We may return 0, so skip adding it here.
	for ( int i = 1; i < newTextures; ++i )
	{
		Assert( textures[ i ] );
		if ( textures[ i ] )
		{
			// We still add these to the tail because we'd prefer to reuse old textures (rather 
			// than these new ones).
			entry.m_nTexName = textures[ i ];
			m_availableTextures.AddToTail( entry );
		}
	}

	if ( holdOne )
		return textures[ 0 ];
	
	// If not, stick that last one in the list and return 0.
	entry.m_nTexName = textures[ 0 ];
	m_availableTextures.AddToTail( entry );

	return 0;
}

void GLMContext::PurgeTexCache()
{
	GLM_FUNC;

	int textureCount = m_availableTextures.Count();

	if ( textureCount == 0 )
		return;

	GLuint* textures = (GLuint*) stackalloc( textureCount * sizeof( GLuint ) );
	
	FOR_EACH_VEC( m_availableTextures, i )
	{
		TextureEntry_t& tex = m_availableTextures[ i ];
		textures[ i ] = tex.m_nTexName;
	}

	gGL->glDeleteTextures( textureCount, textures );
	
	m_availableTextures.RemoveAll();
}

#ifdef OSX
// As far as I can tell this stuff is only useful under OSX.
ConVar	gl_can_mix_shader_gammas( "gl_can_mix_shader_gammas", 0 );
ConVar	gl_cannot_mix_shader_gammas( "gl_cannot_mix_shader_gammas", 0 );
#endif

// ConVar param_write_mode("param_write_mode", "0");

void GLMContext::MarkAllSamplersDirty()
{
	m_nNumDirtySamplers = GLM_SAMPLER_COUNT;
	for (uint i = 0; i < GLM_SAMPLER_COUNT; i++)
	{
		m_nDirtySamplerFlags[i] = 0;
		m_nDirtySamplers[i] = (uint8)i;
	}
}

void GLMContext::FlushDrawStatesNoShaders( )
{
	Assert( ( m_drawingFBO == m_boundDrawFBO ) && ( m_drawingFBO == m_boundReadFBO ) ); // this check MUST succeed

	GLM_FUNC;

	GL_BATCH_PERF( m_FlushStats.m_nTotalBatchFlushes++; )
			
	NullProgram();
}

#if GLMDEBUG

enum EGLMDebugDumpOptions
{
	eDumpBatchInfo,
	eDumpSurfaceInfo,
	eDumpStackCrawl,
	eDumpShaderLinks,
//	eDumpShaderText,			// we never use this one
	eDumpShaderParameters,
	eDumpTextureSetup,
	eDumpVertexAttribSetup,
	eDumpVertexData,
	eOpenShadersForEdit
};

enum EGLMVertDumpMode
{
	// options that affect eDumpVertexData above
	eDumpVertsNoTransformDump,
	eDumpVertsTransformedByViewProj,
	eDumpVertsTransformedByModelViewProj,
	eDumpVertsTransformedByBoneZeroThenViewProj,
	eDumpVertsTransformedByBonesThenViewProj,
	eLastDumpVertsMode
};

char *g_vertDumpModeNames[] = 
{
	"noTransformDump",
	"transformedByViewProj",
	"transformedByModelViewProj",
	"transformedByBoneZeroThenViewProj",
	"transformedByBonesThenViewProj"
};

static void CopyTilEOL( char *dst, char *src, int dstSize )
{
	dstSize--;
	
	int i=0;
	while ( (i<dstSize) && (src[i] != 0) && (src[i] != '\n') && (src[i] != '\r') )
	{
		dst[i] = src[i];
		i++;
	}
	dst[i] = 0;
}

static uint g_maxVertsToDumpLog2 = 4;
static uint g_maxFramesToCrawl = 20;			// usually enough.  Not enough? change it..

extern char sg_pPIXName[128];

// min is eDumpVertsNormal, max is the one before eLastDumpVertsMode
static enum EGLMVertDumpMode g_vertDumpMode = eDumpVertsNoTransformDump;

void GLMContext::DebugDump( GLMDebugHookInfo *info, uint options, uint vertDumpMode )
{
	int oldIndent = GLMGetIndent();
	GLMSetIndent(0);
	
	CGLMProgram *vp = m_drawingProgram[kGLMVertexProgram];
	CGLMProgram *fp = m_drawingProgram[kGLMFragmentProgram];

	bool is_draw = (info->m_caller==eDrawElements);
	const char *batchtype = is_draw ? "draw" : "clear";

	if (options & (1<<eDumpBatchInfo))
	{
		GLMPRINTF(("-D- %s === %s %d ======================================================== %s %d  frame %d", sg_pPIXName, batchtype, m_nBatchCounter, batchtype, m_nBatchCounter, m_debugFrameIndex ));
	}

	if (options & (1<<eDumpSurfaceInfo))
	{
		GLMPRINTF(("-D-" ));
		GLMPRINTF(("-D- surface info:"));
		GLMPRINTF(("-D- drawing FBO: %8x   bound draw-FBO: %8x  (%s)", m_drawingFBO, m_boundDrawFBO, (m_drawingFBO==m_boundDrawFBO) ? "in sync" : "desync!" ));

		CGLMFBO	*fbo = m_boundDrawFBO;
		for( int i=0; i<kAttCount; i++)
		{
			CGLMTex *tex = fbo->m_attach[i].m_tex;
			if (tex)
			{
				GLMPRINTF(("-D-    bound FBO (%8x)  attachment %d = tex %8x (GL %d) (%s)", fbo, i, tex, tex->m_texName, tex->m_layout->m_layoutSummary ));
			}
			else
			{
				// warning if no depthstencil attachment
				switch(i)
				{
					case kAttDepth:
					case kAttStencil:
					case kAttDepthStencil:
						GLMPRINTF(("-D-    bound FBO (%8x)  attachment %d = NULL, warning!", fbo, i ));
					break;
				}
			}
		}
	}

	if (options & (1<<eDumpStackCrawl))
	{
		CStackCrawlParams	cp;
		memset( &cp, 0, sizeof(cp) );
		cp.m_frameLimit = g_maxFramesToCrawl;
		
		GetStackCrawl(&cp);
		
		GLMPRINTF(("-D-" ));
		GLMPRINTF(("-D- stack crawl"));
		for( uint i=0; i< cp.m_frameCount; i++)
		{
			GLMPRINTF(("-D-\t%s", cp.m_crawlNames[i] ));
		}
	}

	if ( (options & (1<<eDumpShaderLinks)) && is_draw)
	{
		// we want to print out - GL name, pathname to disk copy if editable, extra credit would include the summary translation line
		// so grep for "#// trans#"
		char	attribtemp[1000];
		char	transtemp[1000];

		if (vp)
		{
			char *attribmap = strstr(vp->m_text, "#//ATTRIBMAP");
			if (attribmap)
			{
				CopyTilEOL( attribtemp, attribmap, sizeof(attribtemp) );
			}
			else
			{
				strcpy( attribtemp, "no attrib map" );
			}
			
			char *trans = strstr(vp->m_text, "#// trans#");
			if (trans)
			{
				CopyTilEOL( transtemp, trans, sizeof(transtemp) );
			}
			else
			{
				strcpy( transtemp, "no translation info" );
			}
			
			char *linkpath = "no file link";

			#if GLMDEBUG
				linkpath = vp->m_editable->m_mirror->m_path;
			#endif
			
			GLMPRINTF(("-D-"));
			GLMPRINTF(("-D- ARBVP ||  GL %d || Path %s ", vp->m_descs[kGLMARB].m_object.arb, linkpath ));
			GLMPRINTF(("-D-   Attribs %s", attribtemp ));
			GLMPRINTF(("-D-   Trans %s", transtemp ));

			/*
			if ( (options & (1<<eDumpShaderText)) && is_draw )
			{
				GLMPRINTF(("-D-"));
				GLMPRINTF(("-D- VP text " ));
				GLMPRINTTEXT(vp->m_string, eDebugDump ));
			}
			*/
		}
		else
		{
			GLMPRINTF(("-D- VP (none)" ));
		}

		if (fp)
		{
			char *trans = strstr(fp->m_text, "#// trans#");
			if (trans)
			{
				CopyTilEOL( transtemp, trans, sizeof(transtemp) );
			}
			else
			{
				strcpy( transtemp, "no translation info" );
			}
			
			char *linkpath = "no file link";

			#if GLMDEBUG
				linkpath = fp->m_editable->m_mirror->m_path;
			#endif
			
			GLMPRINTF(("-D-"));
			GLMPRINTF(("-D- FP ||  GL %d || Path %s ", fp->m_descs[kGLMARB].m_object.arb, linkpath ));
			GLMPRINTF(("-D-   Trans %s", transtemp ));

			/*
			if ( (options & (1<<eDumpShaderText)) && is_draw )
			{
				GLMPRINTF(("-D-"));
				GLMPRINTF(("-D- FP text " ));
				GLMPRINTTEXT((fp->m_string, eDebugDump));
			}
			*/
		}
		else
		{
			GLMPRINTF(("-D- FP (none)" ));
		}
	}

	if ( (options & (1<<eDumpShaderParameters)) && is_draw )
	{
		GLMPRINTF(("-D-"));
		GLMPRINTF(("-D- VP parameters" ));
		char *label = "";
		//int labelcounter = 0;
		
		static int vmaskranges[] = { /*18,47,*/ -1,-1 };
		//float	transposeTemp;				// row, column for printing
		
		int slotIndex = 0;
		int upperSlotLimit = 61;
			
			// take a peek at the vertex attrib setup.  If it has an attribute for bone weights, then raise the shader param dump limit to 256.
		bool usesSkinning = false;
		GLMVertexSetup *pSetup = &m_drawVertexSetup;			
		for( int index=0; index < kGLMVertexAttributeIndexMax; index++ )
		{
			usesSkinning |= (pSetup->m_attrMask & (1<<index)) && ((pSetup->m_vtxAttribMap[index]>>4)== D3DDECLUSAGE_BLENDWEIGHT);
		}
		if (usesSkinning)
		{
			upperSlotLimit = 256;
		}

		while( slotIndex < upperSlotLimit )
		{
			// if slot index is in a masked range, skip it
			// if slot index is the start of a matrix, label it, print it, skip ahead 4 slots
			for( int maski=0; vmaskranges[maski] >=0; maski+=2)
			{
				if ( (slotIndex >= vmaskranges[maski]) && (slotIndex <= vmaskranges[maski+1]) )
				{
					// that index is masked. set to one past end of range, print a blank line for clarity
					slotIndex = vmaskranges[maski+1]+1;
					GLMPrintStr("-D-     .....");
				}
			}

			if (slotIndex < upperSlotLimit)
			{
				float *values = &m_programParamsF[ kGLMVertexProgram ].m_values[slotIndex][0];
				switch( slotIndex )
				{
					case	4:
						printmat( "MODELVIEWPROJ", slotIndex, 4, values );
						slotIndex += 4;
					break;
					
					case	8:
						printmat( "VIEWPROJ", slotIndex, 4, values );
						slotIndex += 4;
						break;
						
					default:
						if (slotIndex>=58)
						{
							// bone
							char bonelabel[100];

							sprintf(bonelabel, "MODEL_BONE%-2d", (slotIndex-58)/3 );
							printmat( bonelabel, slotIndex, 3, values );

							slotIndex += 3;
						}
						else
						{
							// just print the one slot
							GLMPRINTF(("-D-    %03d: [ %10.5f %10.5f %10.5f %10.5f ]  %s",  slotIndex, values[0], values[1], values[2], values[3], label ));
							slotIndex++;
						}
					break;
				}
			}
		}

		// VP stage still, if in GLSL mode, find the bound pair and see if it has live i0, b0-b3 uniforms
		if (m_pBoundPair)	// should only be non-NULL in GLSL mode
		{
#if 0
			if (m_pBoundPair->m_locVertexBool0>=0)
			{
				GLMPRINTF(("-D- GLSL 'b0': %d",  m_programParamsB[kGLMVertexProgram].m_values[0] ));
			}

			if (m_pBoundPair->m_locVertexBool1>=0)
			{
				GLMPRINTF(("-D- GLSL 'b1': %d",  m_programParamsB[kGLMVertexProgram].m_values[1] ));
			}

			if (m_pBoundPair->m_locVertexBool2>=0)
			{
				GLMPRINTF(("-D- GLSL 'b2': %d",  m_programParamsB[kGLMVertexProgram].m_values[2] ));
			}

			if (m_pBoundPair->m_locVertexBool3>=0)
			{
				GLMPRINTF(("-D- GLSL 'b3': %d",  m_programParamsB[kGLMVertexProgram].m_values[3] ));
			}

			if (m_pBoundPair->m_locVertexInteger0>=0)
			{
				GLMPRINTF(("-D- GLSL 'i0': %d",  m_programParamsI[kGLMVertexProgram].m_values[0][0] ));
			}
#endif
		}

		GLMPRINTF(("-D-"));
		GLMPRINTF(("-D- FP parameters " ));

		static int fmaskranges[] = { 40,41, -1,-1 };
		
		slotIndex = 0;
		label = "";
		while(slotIndex < 40)
		{
			// if slot index is in a masked range, skip it
			// if slot index is the start of a matrix, label it, print it, skip ahead 4 slots
			for( int maski=0; fmaskranges[maski] >=0; maski+=2)
			{
				if ( (slotIndex >= fmaskranges[maski]) && (slotIndex <= fmaskranges[maski+1]) )
				{
					// that index is masked. set to one past end of range, print a blank line for clarity
					slotIndex = fmaskranges[maski+1]+1;
					GLMPrintStr("-D-     .....");
				}
			}

			if (slotIndex < 40)
			{
				float *values = &m_programParamsF[ kGLMFragmentProgram ].m_values[slotIndex][0];
				switch( slotIndex )
				{
					case 0:	label = "g_EnvmapTint";										break;
					case 1:	label = "g_DiffuseModulation";								break;
					case 2:	label = "g_EnvmapContrast_ShadowTweaks";					break;
					case 3:	label = "g_EnvmapSaturation_SelfIllumMask (xyz, and w)";	break;
					case 4:	label = "g_SelfIllumTint_and_BlendFactor (xyz, and w)";		break;

					case 12:	label = "g_ShaderControls";				break;
					case 13:	label = "g_DepthFeatheringConstants";	break;

					case 20:	label = "g_EyePos";						break;
					case 21:	label = "g_FogParams";					break;
					case 22:	label = "g_FlashlightAttenuationFactors";	break;
					case 23:	label = "g_FlashlightPos";				break;
					case 24:	label = "g_FlashlightWorldToTexture";	break;

					case 28:	label = "cFlashlightColor";				break;
					case 29:	label = "g_LinearFogColor";				break;
					case 30:	label = "cLightScale";					break;
					case 31:	label = "cFlashlightScreenScale";		break;

					default:
						label = "";
					break;
				}

				GLMPRINTF(("-D-    %03d: [ %10.5f %10.5f %10.5f %10.5f ]  %s",  slotIndex, values[0], values[1], values[2], values[3], label ));

				slotIndex ++;
			}
		}
		
		if (m_pBoundPair->m_locFragmentFakeSRGBEnable)
		{
			GLMPRINTF(("-D- GLSL 'flEnableSRGBWrite': %f",  m_pBoundPair->m_fakeSRGBEnableValue ));
		}
	}

	if ( (options & (1<<eDumpTextureSetup)) && is_draw )
	{
		GLMPRINTF(( "-D-" ));
		GLMPRINTF(( "-D- Texture / Sampler setup" ));
		GLMPRINTF(( "-D- TODO" ));
#if 0
		for( int i=0; i<GLM_SAMPLER_COUNT; i++ )
		{
			if (m_samplers[i].m_pBoundTex)
			{
				GLMTexSamplingParams *samp = &m_samplers[i].m_samp;
				GLMPRINTF(( "-D-" ));
				GLMPRINTF(("-D- Sampler %-2d tex %08x  layout %s", i, m_samplers[i].m_pBoundTex, m_samplers[i].m_pBoundTex->m_layout->m_layoutSummary ));

				GLMPRINTF(("-D-           addressMode[ %s %s %s ]",
					GLMDecode( eGL_ENUM, samp->m_addressModes[0] ),
					GLMDecode( eGL_ENUM, samp->m_addressModes[1] ),
					GLMDecode( eGL_ENUM, samp->m_addressModes[2] )
				));
					
				GLMPRINTF(("-D-           magFilter    [ %s ]", GLMDecode( eGL_ENUM, samp->m_magFilter ) ));
				GLMPRINTF(("-D-           minFilter    [ %s ]", GLMDecode( eGL_ENUM, samp->m_minFilter ) ));
				GLMPRINTF(("-D-           srgb         [ %s ]", samp->m_srgb ? "T" : "F" ));
				GLMPRINTF(("-D-           shadowFilter [ %s ]", samp->m_compareMode == GL_COMPARE_R_TO_TEXTURE_ARB ? "T" : "F" ));
				
				// add more as needed later..
			}
		}		
#endif
	}

	if ( (options & (1<<eDumpVertexAttribSetup)) && is_draw )
	{
		GLMVertexSetup *pSetup = &m_drawVertexSetup;
		
		uint	nRelevantMask =	pSetup->m_attrMask;		
		for( int index=0; index < kGLMVertexAttributeIndexMax; index++ )
		{
			uint mask = 1<<index;
			if (nRelevantMask & mask)
			{
				GLMVertexAttributeDesc *setdesc = &pSetup->m_attrs[index];

				char	sizestr[100];
				if (setdesc->m_nCompCount < 32)
				{
					sprintf( sizestr, "%d", setdesc->m_nCompCount);
				}
				else
				{
					strcpy( sizestr, GLMDecode( eGL_ENUM, setdesc->m_nCompCount ) );
				}
				
				if (pSetup->m_vtxAttribMap[index] != 0xBB)
				{
					GLMPRINTF(("-D-   attr=%-2d  decl=$%s%1d  stride=%-2d  offset=%-3d  buf=%08x  size=%s  type=%s  normalized=%s  ",
									index,
									GLMDecode(eD3D_VTXDECLUSAGE, pSetup->m_vtxAttribMap[index]>>4 ),
									pSetup->m_vtxAttribMap[index]&0x0F,
									setdesc->m_stride,
									setdesc->m_offset,
									setdesc->m_pBuffer,
									sizestr,
									GLMDecode( eGL_ENUM, setdesc->m_datatype),
									setdesc->m_normalized?"Y":"N"
								));
				}
				else
				{
					// the attrib map is referencing an attribute that is not wired up in the vertex setup...
					DebuggerBreak();
				}
			}
		}
	}

	if ( (options & (1<<eDumpVertexData)) && is_draw )
	{
		GLMVertexSetup *pSetup = &m_drawVertexSetup;
		int start = info->m_drawStart;
		int end = info->m_drawEnd;
		int endLimit = start + (1<<g_maxVertsToDumpLog2);
		int realEnd = MIN( end, endLimit );

		// vertex data
		GLMPRINTF(("-D-"));
		GLMPRINTF(("-D- Vertex Data : %d of %d verts (index %d through %d)", realEnd-start, end-start, start, realEnd-1));
	
		for( int vtxIndex=-1; vtxIndex < realEnd; vtxIndex++ )	// vtxIndex will jump from -1 to start after first spin, not necessarily to 0
		{
			char buf[64000];
			char *mark = buf;
			
			// index -1 is the first run through the loop, we just print a header
			
			// iterate attrs
			if (vtxIndex>=0)
			{
				mark += sprintf(mark, "-D-  %04d: ", vtxIndex );
			}
			
				// for transform dumping, we latch values as we spot them
			float	vtxPos[4];
			int		vtxBoneIndices[4];	// only three get used
			float	vtxBoneWeights[4];	// only three get used and index 2 is synthesized from 0 and 1
			
			vtxPos[0] = vtxPos[1] = vtxPos[2] = 0.0;
			vtxPos[3] = 1.0;
			
			vtxBoneIndices[0] = vtxBoneIndices[1] = vtxBoneIndices[2] = vtxBoneIndices[3] = 0;
			vtxBoneWeights[0] = vtxBoneWeights[1] = vtxBoneWeights[2] = vtxBoneWeights[3] = 0.0;
			
			for( int attr = 0; attr < kGLMVertexAttributeIndexMax; attr++ )
			{
				if (pSetup->m_attrMask & (1<<attr) )
				{
					GLMVertexAttributeDesc *desc = &pSetup->m_attrs[ attr ];
					
					// print that attribute.

					// on OSX, VB's never move unless resized.  You can peek at them when unmapped.  Safe enough for debug..
					char *bufferBase = (char*)desc->m_pBuffer->m_pLastMappedAddress;

					uint stride = desc->m_stride;
					uint fieldoffset = desc->m_offset;
					uint baseoffset = vtxIndex * stride;
					
					char *attrBase = bufferBase + baseoffset + fieldoffset;

					uint usage = pSetup->m_vtxAttribMap[attr]>>4;
					uint usageindex = pSetup->m_vtxAttribMap[attr]&0x0F;
					
					if (vtxIndex <0)
					{
						mark += sprintf(mark, "[%s%1d @ offs=%04d / strd %03d] ", GLMDecode(eD3D_VTXDECLUSAGE, usage ), usageindex, fieldoffset, stride );
					}
					else
					{
						mark += sprintf(mark, "[%s%1d ", GLMDecode(eD3D_VTXDECLUSAGE, usage ), usageindex );
						
						if (desc->m_nCompCount<32)
						{
							for( uint which = 0; which < desc->m_nCompCount; which++ )
							{
								static char *fieldname = "xyzw";
								switch( desc->m_datatype )
								{
									case GL_FLOAT:
									{
										float	*floatbase = (float*)attrBase;
										mark += sprintf(mark, (usage != D3DDECLUSAGE_TEXCOORD) ? "%c%7.3f " : "%c%.3f", fieldname[which], floatbase[which] );
										
										if (usage==D3DDECLUSAGE_POSITION)
										{
											if (which<4)
											{
												// latch pos
												vtxPos[which] = floatbase[which];
											}
										}

										if (usage==D3DDECLUSAGE_BLENDWEIGHT)
										{
											if (which<4)
											{
												// latch weight
												vtxBoneWeights[which] = floatbase[which];
											}
										}
									}
									break;
									
									case GL_UNSIGNED_BYTE:
									{
										unsigned char *unchbase = (unsigned char*)attrBase;
										mark += sprintf(mark, "%c$%02X ", fieldname[which], unchbase[which] );
									}
									break;

									default:
										// hold off on other formats for now
										mark += sprintf(mark, "%c????? ", fieldname[which] );
									break;
								}
							}
						}
						else	// special path for BGRA bytes which are expressed in GL by setting the *size* to GL_BGRA (gross large enum)
						{
							switch(desc->m_nCompCount)
							{
								case GL_BGRA:		// byte reversed color
								{
									for( int which = 0; which < 4; which++ )
									{
										static const char *fieldname = "BGRA";
										switch( desc->m_datatype )
										{
											case GL_UNSIGNED_BYTE:
											{
												unsigned char *unchbase = (unsigned char*)attrBase;
												mark += sprintf(mark, "%c$%02X ", fieldname[which], unchbase[which] );
												
												if (usage==D3DDECLUSAGE_BLENDINDICES)
												{
													if (which<4)
													{
														// latch index
														vtxBoneIndices[which] = unchbase[which];		// ignoring the component reverse which BGRA would inflict, but we also ignore it below so it matches up.
													}
												}
											}												
											break;

											default:
												DebuggerBreak();
												break;
										}
									}
								}
								break;
							}
						}
						mark += sprintf(mark, "] " );
					}
				}
			}
			GLMPrintStr( buf, eDebugDump );

			if (vtxIndex >=0)
			{
				// if transform dumping requested, and we've reached the actual vert dump phase, do it
				float	vtxout[4];
				char	*translabel = NULL;   // NULL means no print...
				
				switch( g_vertDumpMode )
				{
					case eDumpVertsNoTransformDump:	break;
					
					case eDumpVertsTransformedByViewProj:				// viewproj is slot 8
					{
						float *viewproj = &m_programParamsF[ kGLMVertexProgram ].m_values[8][0];
						transform_dp4( vtxPos, viewproj, 4, vtxout );
						translabel = "post-viewproj";
					}
					break;
					
					case eDumpVertsTransformedByModelViewProj:			// modelviewproj is slot 4
					{
						float *modelviewproj = &m_programParamsF[ kGLMVertexProgram ].m_values[4][0];
						transform_dp4( vtxPos, modelviewproj, 4, vtxout );
						translabel = "post-modelviewproj";
					}
					break;
					
					case eDumpVertsTransformedByBoneZeroThenViewProj:
					{
						float	postbone[4];
						postbone[3] = 1.0;
						
						float *bonemat = &m_programParamsF[ kGLMVertexProgram ].m_values[58][0];
						transform_dp4( vtxPos, bonemat, 3, postbone );
						
						float *viewproj = &m_programParamsF[ kGLMVertexProgram ].m_values[8][0];	// viewproj is slot 8
						transform_dp4( postbone, viewproj, 4, vtxout );

						translabel = "post-bone0-viewproj";
					}
					break;
					
					case eDumpVertsTransformedByBonesThenViewProj:
					{
						//float	bone[4][4];			// [bone index][bone member]	// members are adjacent
						
						vtxout[0] = vtxout[1] = vtxout[2] = vtxout[3] = 0;
						
						// unpack the third weight
						vtxBoneWeights[2] = 1.0 - (vtxBoneWeights[0] + vtxBoneWeights[1]);
						
						for( int ibone=0; ibone<3; ibone++ )
						{
							int boneindex = vtxBoneIndices[ ibone ];
							float *bonemat = &m_programParamsF[ kGLMVertexProgram ].m_values[58+(boneindex*3)][0];
							
							float boneweight = vtxBoneWeights[ibone];
							
							float	postbonevtx[4];
							
							transform_dp4( vtxPos, bonemat, 3, postbonevtx );
							
							// add weighted sum into output
							for( int which=0; which<4; which++ )
							{
								vtxout[which] += boneweight * postbonevtx[which];
							}
						}
						
						// fix W ?  do we care ?  check shaders to see what they do...
						translabel = "post-skin3bone-viewproj";
					}
					break;
				}
				if(translabel)
				{
					// for extra credit, do the perspective divide and viewport
					
					GLMPRINTF(("-D-   %-24s: [ %7.4f %7.4f %7.4f %7.4f ]", translabel, vtxout[0],vtxout[1],vtxout[2],vtxout[3] ));
					GLMPRINTF(("-D-" ));
				}
			}
			
			if (vtxIndex<0)
			{
				vtxIndex = start-1; // for printing of the data (note it will be incremented at bottom of loop, so bias down by 1)
			}
			else
			{	// no more < and > around vert dump lines
				//mark += sprintf(mark, "" );
			}
		}
	}

	if (options & (1<<eOpenShadersForEdit) )
	{
		#if GLMDEBUG
			if (m_drawingProgram[ kGLMVertexProgram ])
			{
				m_drawingProgram[ kGLMVertexProgram ]->m_editable->OpenInEditor();
			}
			
			if (m_drawingProgram[ kGLMFragmentProgram ])
			{
				m_drawingProgram[ kGLMFragmentProgram ]->m_editable->OpenInEditor();
			}
		#endif
	}
/*
	if (options & (1<<))
	{
	}
*/
	// trailer line
	GLMPRINTF(("-D- ===================================================================================== end %s %d  frame %d", batchtype, m_nBatchCounter, m_debugFrameIndex  ));

	GLMSetIndent(oldIndent);
}

// here is the table that binds knob numbers to names.  change at will.
char *g_knobnames[] = 
{
/*0*/	"dummy",

/*1*/	"FB-SRGB",
	#if 0
		/*1*/	"tex-U0-bias",	// src left
		/*2*/	"tex-V0-bias",	// src upper
		/*3*/	"tex-U1-bias",	// src right
		/*4*/	"tex-V1-bias",	// src bottom

		/*5*/	"pos-X0-bias",	// dst left
		/*6*/	"pos-Y0-bias",	// dst upper
		/*7*/	"pos-X1-bias",	// dst right
		/*8*/	"pos-Y1-bias",	// dst bottom
	#endif

};
int g_knobcount = sizeof( g_knobnames ) / sizeof( g_knobnames[0] );

void GLMContext::DebugHook( GLMDebugHookInfo *info )
{
	// FIXME: This has seriously bitrotted.
	return;

	bool debughook = false;
	// debug hook is called after an action has taken place.
	// that would be the initial action, or a repeat.
	// if paused, we stay inside this function until return.
	// when returning, we inform the caller if it should repeat its last action or continue.
	// there is no global pause state.  The rest of the app runs at the best speed it can.

	// initial stuff we do unconditionally
	
	// increment iteration
	info->m_iteration++;						// can be thought of as "number of times the caller's action has now occurred - starting at 1"

	// now set initial state guess for the info block (outcome may change below)
	info->m_loop = false;

	// check prior hold-conditions to see if any of them hit.
	// note we disarm each trigger once the hold has occurred (one-shot style)
	
	switch( info->m_caller )
	{
		case eBeginFrame:
			if (debughook) GLMPRINTF(("-D- Caller: BeginFrame" ));
			if ( (m_holdFrameBegin>=0) && (m_holdFrameBegin==m_debugFrameIndex) )		// did we hit a frame breakpoint?
			{
				if (debughook) GLMPRINTF(("-D-         BeginFrame trigger match, clearing m_holdFrameBegin, hold=true" ));

				m_holdFrameBegin = -1;

				info->m_holding = true;
			}
		break;

		case eClear:
			if (debughook) GLMPRINTF(("-D- Caller: Clear" ));
			if ( (m_holdBatch>=0) && (m_holdBatchFrame>=0) && ((int)m_holdBatch==(int)m_nBatchCounter) && ((int)m_holdBatchFrame==(int)m_debugFrameIndex) )
			{
				if (debughook) GLMPRINTF(("-D-         Clear trigger match, clearing m_holdBatch&Frame, hold=true" ));

				m_holdBatch = m_holdBatchFrame = -1;

				info->m_holding = true;
			}
			break;

		case eDrawElements:
			if (debughook) GLMPRINTF(( (info->m_caller==eClear) ? "-D- Caller: Clear" : "-D- Caller: Draw" ));
			if ( (m_holdBatch>=0) && (m_holdBatchFrame>=0) && ((int)m_holdBatch==(int)m_nBatchCounter) && ((int)m_holdBatchFrame==(int)m_debugFrameIndex) )
			{
				if (debughook) GLMPRINTF(("-D-         Draw trigger match, clearing m_holdBatch&Frame, hold=true" ));

				m_holdBatch = m_holdBatchFrame = -1;

				info->m_holding = true;
			}
		break;

		case eEndFrame:
			if (debughook) GLMPRINTF(("-D- Caller: EndFrame" ));

			// check for any expired batch hold req
			if ( (m_holdBatch>=0) && (m_holdBatchFrame>=0) && (m_holdBatchFrame==m_debugFrameIndex) )
			{
				// you tried to say 'next batch', but there wasn't one in this frame.
				// target first batch of next frame instead
				if (debughook) GLMPRINTF(("-D-         EndFrame noticed an expired draw hold trigger, rolling to next frame, hold=false"));

				m_holdBatch = 0;
				m_holdBatchFrame++;

				info->m_holding = false;
			}
			
			// now check for an explicit hold on end of this frame..
			if ( (m_holdFrameEnd>=0) && (m_holdFrameEnd==m_debugFrameIndex) )
			{
				if (debughook) GLMPRINTF(("-D-         EndFrame trigger match, clearing m_holdFrameEnd, hold=true" ));

				m_holdFrameEnd = -1;

				info->m_holding = true;
			}
		break;
	}

	// spin until event queue is empty *and* hold is false

	int evtcount=0;

	bool refresh = info->m_holding || m_debugDelayEnable;  // only refresh once per initial visit (if paused!) or follow up event input	
	int breakToDebugger = 0;
		// 1 = break to GDB
		// 2 = break to OpenGL Profiler if attached
	
	do
	{
		if (refresh)
		{
			if (debughook) GLMPRINTF(("-D- pushing pixels" ));
			DebugPresent();		// show pixels
			
			uint minidumpOptions = (1<<eDumpBatchInfo) /* | (1<<eDumpSurfaceInfo) */;
			DebugDump( info, minidumpOptions, g_vertDumpMode );

			ThreadSleep( 10000 / 1000 );				// lil sleep
			
			refresh = false;
		}

		bool eventCheck = true;			// event pull will be skipped if we detect a shader edit being done
		// keep editable shaders in sync
		#if GLMDEBUG
		
			bool redrawBatch = false;
			if (m_drawingProgram[ kGLMVertexProgram ])
			{
				if( m_drawingProgram[ kGLMVertexProgram ]->SyncWithEditable() )
				{
					redrawBatch = true;
				}
			}
			
			if (m_drawingProgram[ kGLMFragmentProgram ])
			{
				if( m_drawingProgram[ kGLMFragmentProgram ]->SyncWithEditable() )
				{
					redrawBatch = true;
				}
			}
			
			if (redrawBatch)
			{
				// act as if user pressed the option-\ key
				
				if (m_drawingLang == kGLMGLSL)
				{
					// if GLSL mode, force relink - and refresh the pair cache as needed
					if (m_pBoundPair)
					{
						// fix it in place
						m_pBoundPair->RefreshProgramPair();
					}
				}
				
				// TODO - need to retest this whole path
				FlushDrawStates( 0, 0, 0 );	// this is key, because the linked shader pair may have changed (note call to PurgePairsWithShader in cglmprogram.cpp)
				
				GLMPRINTF(("-- Shader changed, re-running batch" ));

				m_holdBatch = m_nBatchCounter;
				m_holdBatchFrame = m_debugFrameIndex;
				m_debugDelayEnable = false;
				
				info->m_holding = false;
				info->m_loop = true;
				
				eventCheck = false;
			}
		#endif
		
		if(eventCheck)
		{
			PumpWindowsMessageLoop();
			CCocoaEvent	evt;
			evtcount = 	GetEvents( &evt, 1, true );	// asking for debug events only.
			if (evtcount)
			{
				// print it
				if (debughook) GLMPRINTF(("-D- Received debug key '%c' with modifiers %x", evt.m_UnicodeKeyUnmodified, evt.m_ModifierKeyMask ));
				
				// flag for refresh if we spin again
				refresh = 1;
				
				switch(evt.m_UnicodeKeyUnmodified)
				{
					case ' ':					// toggle pause
						// clear all the holds to be sure
						m_holdFrameBegin = m_holdFrameEnd = m_holdBatch = m_holdBatchFrame = -1;
						info->m_holding = !info->m_holding;
						
						if (!info->m_holding)
						{
							m_debugDelayEnable = false;	// coming out of pause means no slow mo
						}

						GLMPRINTF((info->m_holding ? "-D- Paused." :  "-D- Unpaused." ));
					break;
					
					case 'f':					// frame advance
						GLMPRINTF(("-D- Command: next frame" ));
						m_holdFrameBegin = m_debugFrameIndex+1;		// stop at top of next numbered frame
						m_debugDelayEnable = false;					// get there fast

						info->m_holding = false;
					break;

					case ']':					// ahead 1 batch
					case '}':					// ahead ten batches
					{
						int delta = evt.m_UnicodeKeyUnmodified == ']' ? 1 : 10;
						m_holdBatch = m_nBatchCounter+delta;
						m_holdBatchFrame = m_debugFrameIndex;
						m_debugDelayEnable = false;					// get there fast
						info->m_holding = false;
						GLMPRINTF(("-D- Command: advance %d batches to %d", delta, m_holdBatch ));
					}
					break;

					case '[':					// back one batch
					case '{':					// back 10 batches
					{
						int delta = evt.m_UnicodeKeyUnmodified == '[' ? -1 : -10;
						m_holdBatch = m_nBatchCounter + delta;
						if (m_holdBatch<0)
						{
							m_holdBatch = 0;
						}
						m_holdBatchFrame = m_debugFrameIndex+1;		// next frame, but prev batch #
						m_debugDelayEnable = false;					// get there fast
						info->m_holding = false;
						GLMPRINTF(("-D- Command: rewind %d batches to %d", delta, m_holdBatch ));
					}
					break;

					case '\\':					// batch rerun

						m_holdBatch = m_nBatchCounter;
						m_holdBatchFrame = m_debugFrameIndex;
						m_debugDelayEnable = false;						
						info->m_holding = false;
						info->m_loop = true;
						GLMPRINTF(("-D- Command: re-run batch %d", m_holdBatch ));
					break;
					
					case 'c':					// toggle auto color clear
						m_autoClearColor = !m_autoClearColor;
						GLMPRINTF((m_autoClearColor ? "-D- Auto color clear ON" :  "-D- Auto color clear OFF" ));
					break;

					case 's':					// toggle auto stencil clear
						m_autoClearStencil = !m_autoClearStencil;
						GLMPRINTF((m_autoClearStencil ? "-D- Auto stencil clear ON" :  "-D- Auto stencil clear OFF" ));
					break;

					case 'd':					// toggle auto depth clear
						m_autoClearDepth = !m_autoClearDepth;
						GLMPRINTF((m_autoClearDepth ? "-D- Auto depth clear ON" :  "-D- Auto depth clear OFF" ));
					break;

					case '.':					// break to debugger  or insta-quit
						if (evt.m_ModifierKeyMask & (1<<eControlKey))
						{
							GLMPRINTF(( "-D- INSTA QUIT!  (TM) (PAT PEND)" ));
							abort();
						}
						else
						{
							GLMPRINTF(( "-D- Breaking to debugger" ));
							breakToDebugger = 1;

							info->m_holding = true;
							info->m_loop = true;	// so when you come back from debugger, you get another spin (i.e. you enter paused mode)
						}						
					break;
					
					case 'g':					// break to OGLP and enable OGLP logging of spew
						if (GLMDetectOGLP())	// if this comes back true, there will be a breakpoint set on glColor4sv.
						{
							uint channelMask = GLMDetectAvailableChannels();	// will re-assert whether spew goes to OGLP log
							
							if (channelMask & (1<<eGLProfiler))
							{
								GLMDebugChannelMask(&channelMask);
								breakToDebugger = 2;

								info->m_holding = true;
								info->m_loop = true;	// so when you come back from debugger, you get another spin (i.e. you enter paused mode)
							}
						}
					break;

					case '_':					// toggle slow mo
						m_debugDelayEnable = !m_debugDelayEnable;
					break;

					case '-':					// go slower
						if (m_debugDelayEnable)
						{
							// already in slow mo, so lower speed
							m_debugDelay <<= 1;	// double delay
							if (m_debugDelay > (1<<24))
							{
								m_debugDelay = (1<<24);
							}
						}
						else
						{
							// enter slow mo
							m_debugDelayEnable = true;
						}
					break;

					case '=':					// go faster
						if (m_debugDelayEnable)
						{
							// already in slow mo, so raise speed
							m_debugDelay >>= 1;	// halve delay
							if (m_debugDelay < (1<<17))
							{
								m_debugDelay = (1<<17);
							}
						}
						else
						{
							// enter slow mo
							m_debugDelayEnable = true;
						}
					break;
					
					case 'v':
						// open vs in editor (foreground pop)
						#if GLMDEBUG
							if (m_drawingProgram[ kGLMVertexProgram ])
							{
								m_drawingProgram[ kGLMVertexProgram ]->m_editable->OpenInEditor( true );
							}
						#endif						
					break;

					case 'p':
						// open fs/ps in editor (foreground pop)
						#if GLMDEBUG
							if (m_drawingProgram[ kGLMFragmentProgram ])
							{
								m_drawingProgram[ kGLMFragmentProgram ]->m_editable->OpenInEditor( true );
							}
						#endif
					break;
					
					case '<':	// dump fewer verts
					case '>':	// dump more verts
					{
						int delta = (evt.m_UnicodeKeyUnmodified=='>') ? 1 : -1;
						g_maxVertsToDumpLog2 = MIN( MAX( g_maxVertsToDumpLog2+delta, 0 ), 16 );
						
						// just re-dump the verts
						DebugDump( info, 1<<eDumpVertexData, g_vertDumpMode );
					}
					break;
					
					case 'x':	// adjust transform dump mode
					{
						int newmode = g_vertDumpMode+1;
						if (newmode >= eLastDumpVertsMode)
						{
							// wrap
							newmode = eDumpVertsNoTransformDump;
						}
						g_vertDumpMode = (EGLMVertDumpMode)newmode;
						
						GLMPRINTF(("-D- New vert dump mode is %s", g_vertDumpModeNames[g_vertDumpMode] ));
					}
					break;

					case	'u':	// more crawl
					{
						CStackCrawlParams	cp;
						memset( &cp, 0, sizeof(cp) );
						cp.m_frameLimit = kMaxCrawlFrames;
						
						GetStackCrawl(&cp);
						
						GLMPRINTF(("-D-" ));
						GLMPRINTF(("-D- extended stack crawl:"));
						for( uint i=0; i< cp.m_frameCount; i++)
						{
							GLMPRINTF(("-D-\t%s", cp.m_crawlNames[i] ));
						}
					}

					break;
						
					case 'q':
						DebugDump( info, 0xFFFFFFFF, g_vertDumpMode );
					break;
					
	
					case 'H':
					case 'h':
					{
						// toggle drawing language.  hold down shift key to do it immediately.
						
						if (m_caps.m_hasDualShaders)
						{
							bool immediate;
							
							immediate = evt.m_UnicodeKeyUnmodified == 'H';	// (evt.m_ModifierKeyMask & (1<<eShiftKey)) != 0;
							
							if (m_drawingLang==kGLMARB)
							{
								GLMPRINTF(( "-D- Setting GLSL language mode %s.", immediate ? "immediately" : "for next frame start" ));
								SetDrawingLang( kGLMGLSL, immediate );
							}
							else
							{
								GLMPRINTF(( "-D- Setting ARB language mode %s.", immediate ? "immediately" : "for next frame start" ));
								SetDrawingLang( kGLMARB, immediate );
							}
							refresh = immediate;
						}
						else
						{
							GLMPRINTF(("You can't change shader languages unless you launch with -glmdualshaders enabled"));
						}

					}
					break;
					

					// ======================================================== debug knobs.  change these as needed to troubleshoot stuff
					
					// keys to select a knob
					// or, toggle a debug flavor, if control is being held down
					case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
					{
						if (evt.m_ModifierKeyMask & (1<<eControlKey))
						{
							// '0' toggles the all-channels on or off
							int flavorSelect = evt.m_UnicodeKeyUnmodified - '0';
							
							if ( (flavorSelect >=0) && (flavorSelect<eFlavorCount) )
							{
								uint mask = GLMDebugFlavorMask();
								
								mask ^= (1<<flavorSelect);

								GLMDebugFlavorMask(&mask);
							}
						}
						else
						{
							// knob selection
							m_selKnobIndex = evt.m_UnicodeKeyUnmodified - '0';

							GLMPRINTF(("-D- Knob # %d (%s) selected.", m_selKnobIndex, g_knobnames[ m_selKnobIndex ] ));
							
							m_selKnobIncrement = (m_selKnobIndex<5) ? (1.0f / 2048.0f) : (1.0 / 256.0f);
							ThreadSleep( 500000 / 1000 );
						}
						refresh = false;
					}
					break;
					
					// keys to adjust or zero a knob
					case 't':		// toggle
					{
						if (m_selKnobIndex < g_knobcount)
						{
							GLMKnobToggle( g_knobnames[ m_selKnobIndex ] );
						}
					}
					break;
					
					case 'l':		// less
					case 'm':		// more
					case 'z':		// zero
					{						
						if (m_selKnobIndex < g_knobcount)
						{
							float val = GLMKnob( g_knobnames[ m_selKnobIndex ], NULL );
							
							if (evt.m_UnicodeKeyUnmodified == 'l')
							{
								// minus (less)
								val -= m_selKnobIncrement;
								if (val < m_selKnobMinValue)
								{
									val = m_selKnobMinValue;
								}
								// send new value back to the knob
								GLMKnob( g_knobnames[ m_selKnobIndex ], &val );
							}

							if (evt.m_UnicodeKeyUnmodified == 'm')
							{
								// plus (more)
								val += m_selKnobIncrement;
								if (val > m_selKnobMaxValue)
								{
									val = m_selKnobMaxValue;
								}
								// send new value back to the knob
								GLMKnob( g_knobnames[ m_selKnobIndex ], &val );
							}
							
							if (evt.m_UnicodeKeyUnmodified == 'z')
							{
								// zero
								val = 0.0f;

								// send new value back to the knob
								GLMKnob( g_knobnames[ m_selKnobIndex ], &val );
							}
							
							GLMPRINTF(("-D- Knob # %d (%s) set to %f  (%f/1024.0)", m_selKnobIndex, g_knobnames[ m_selKnobIndex ], val, val * 1024.0 ));

							ThreadSleep( 500000 / 1000 );

							refresh = false;
						}
					}
					break;

				}
			}
		}		
	}	while( ((evtcount>0) || info->m_holding) && (!breakToDebugger) );

	if (m_debugDelayEnable)
	{
		ThreadSleep( m_debugDelay / 1000 );
	}

	if (breakToDebugger)
	{
		switch (breakToDebugger)
		{
			case 1:
				DebuggerBreak();
			break;
			
			case 2:
				short fakecolor[4] = { 0, 0, 0, 0 };
				gGL->glColor4sv( fakecolor );	// break to OGLP
			break;
		}
		// re-flush all GLM states so you can fiddle with them in the debugger. then run the batch again and spin..
		ForceFlushStates();
	}
}

void GLMContext::DebugPresent( void )
{
	CGLMTex *drawBufferTex = m_drawingFBO->m_attach[kAttColor0].m_tex;
	gGL->glFinish();
	Present( drawBufferTex );
}

void GLMContext::DebugClear( void )
{
	// get old clear color
	GLClearColor_t clearcol_orig;
	m_ClearColor.Read( &clearcol_orig,0 );
	
	// new clear color
	GLClearColor_t clearcol;
	clearcol.r = m_autoClearColorValues[0];
	clearcol.g = m_autoClearColorValues[1];
	clearcol.b = m_autoClearColorValues[2];
	clearcol.a = m_autoClearColorValues[3];
	m_ClearColor.Write( &clearcol ); // don't check, don't defer
	
	uint mask = 0;
	
	if (m_autoClearColor) mask |= GL_COLOR_BUFFER_BIT;
	if (m_autoClearDepth) mask |= GL_DEPTH_BUFFER_BIT;
	if (m_autoClearStencil) mask |= GL_STENCIL_BUFFER_BIT;
	
	gGL->glClear( mask );
	gGL->glFinish();

	// put old color back
	m_ClearColor.Write( &clearcol_orig ); // don't check, don't defer
}

#endif

void GLMContext::CheckNative( void )
{
	// note that this is available in release.  We don't use GLMPRINTF for that reason.
	// note we do not get called unless either slow-batch asserting or logging is enabled.
#ifdef OSX	
	bool gpuProcessing;
	GLint fragmentGPUProcessing, vertexGPUProcessing;
	
	CGLGetParameter (CGLGetCurrentContext(), kCGLCPGPUFragmentProcessing, &fragmentGPUProcessing);
	CGLGetParameter(CGLGetCurrentContext(), kCGLCPGPUVertexProcessing, &vertexGPUProcessing);

	// spews then asserts.
	// that way you can enable both, get log output on a pair if it's slow, and then the debugger will pop.
	if(m_slowSpewEnable)
	{
		if ( !vertexGPUProcessing )
		{
			m_drawingProgram[ kGLMVertexProgram ]->LogSlow( m_drawingLang );
		}
		if ( !fragmentGPUProcessing )
		{
			m_drawingProgram[ kGLMFragmentProgram ]->LogSlow( m_drawingLang );
		}
	}

	if(m_slowAssertEnable)
	{
		if ( !vertexGPUProcessing || !fragmentGPUProcessing)
		{
			Assert( !"slow batch" );
		}
	}
#else
	//Assert( !"impl GLMContext::CheckNative()" );

	if (m_checkglErrorsAfterEveryBatch)
	{
		// This is slow, and somewhat redundant (-gldebugoutput uses the GL_ARB_debug_output extension, which can be at least asynchronous), but having a straightforward backup can be useful.
		// This is useful for callstack purposes - GL_ARB_debug_output may break in a different thread that the thread triggering the GL error.
		//gGL->glFlush();
		GLenum errorcode = (GLenum)gGL->glGetError();
		if ( errorcode != GL_NO_ERROR )
		{
			const char	*decodedStr = GLMDecode( eGL_ERROR, errorcode );

			char buf[512];
			V_snprintf( buf, sizeof( buf), "\nGL ERROR! %08x = '%s'\n", errorcode, decodedStr );

			// Make sure the dev sees something, because these errors can happen early enough that DevMsg() does nothing.
#ifdef WIN32
			OutputDebugStringA( buf );
#else
			printf( "%s", buf );
#endif
		}
	}

#endif

}



// debug font
void GLMContext::GenDebugFontTex( void )
{
	if(!m_debugFontTex)
	{
		// make a 128x128 RGBA texture
		GLMTexLayoutKey key;
		memset( &key, 0, sizeof(key) );
		
		key.m_texGLTarget	= GL_TEXTURE_2D;
		key.m_xSize			= 128;
		key.m_ySize			= 128;
		key.m_zSize			= 1;
		key.m_texFormat		= D3DFMT_A8R8G8B8;
		key.m_texFlags		= 0;

		m_debugFontTex = NewTex( &key, 1, "GLM debug font" );
		

		//-----------------------------------------------------
		GLMTexLockParams lockreq;
		
		lockreq.m_tex = m_debugFontTex;
		lockreq.m_face = 0;
		lockreq.m_mip = 0;

		GLMTexLayoutSlice *slice = &m_debugFontTex->m_layout->m_slices[ lockreq.m_tex->CalcSliceIndex( lockreq.m_face, lockreq.m_mip ) ];
		
		lockreq.m_region.xmin = lockreq.m_region.ymin = lockreq.m_region.zmin = 0;
		lockreq.m_region.xmax = slice->m_xSize;
		lockreq.m_region.ymax = slice->m_ySize;
		lockreq.m_region.zmax = slice->m_zSize;

		lockreq.m_readback = false;
		
		char	*lockAddress;
		int		yStride;
		int		zStride;
		
		m_debugFontTex->Lock( &lockreq, &lockAddress, &yStride, &zStride );
		
		//-----------------------------------------------------
		// fetch elements of font data and make texels... we're doing the whole slab so we don't really need the stride info
		unsigned long *destTexelPtr = (unsigned long *)lockAddress;

		for( int index = 0; index < 16384; index++ )
		{
			if (g_glmDebugFontMap[index] == ' ')
			{
				// clear
				*destTexelPtr = 0x00000000;
			}
			else
			{
				// opaque white (drawing code can modulate if desired)
				*destTexelPtr = 0xFFFFFFFF;
			}
			destTexelPtr++;
		}
		
		//-----------------------------------------------------
		GLMTexLockParams unlockreq;
		
		unlockreq.m_tex = m_debugFontTex;
		unlockreq.m_face = 0;
		unlockreq.m_mip = 0;

		// region need not matter for unlocks
		unlockreq.m_region.xmin = unlockreq.m_region.ymin = unlockreq.m_region.zmin = 0;
		unlockreq.m_region.xmax = unlockreq.m_region.ymax = unlockreq.m_region.zmax = 0;

		unlockreq.m_readback = false;

		m_debugFontTex->Unlock( &unlockreq );

		//-----------------------------------------------------
			// change up the tex sampling on this texture to be "nearest" not linear
			
		//-----------------------------------------------------

		// don't leave texture bound on the TMU
		BindTexToTMU( NULL, 0 );
		
		// also make the index and vertex buffers for use - up to 1K indices and 1K verts
		
		uint indexBufferSize = 1024*2;
		
		m_debugFontIndices = NewBuffer(kGLMIndexBuffer, indexBufferSize, 0);	// two byte indices
		
		// we go ahead and lock it now, and fill it with indices 0-1023.
		char *indices = NULL;
		GLMBuffLockParams		idxLock;
		idxLock.m_nOffset		= 0;
		idxLock.m_nSize			= indexBufferSize;
		idxLock.m_bNoOverwrite	= false;
		idxLock.m_bDiscard		= true;
		m_debugFontIndices->Lock( &idxLock, &indices );
		for( int i=0; i<1024; i++)
		{
			unsigned short *idxPtr = &((unsigned short*)indices)[i];
			*idxPtr = i;
		}
		m_debugFontIndices->Unlock();
		
		m_debugFontVertices = NewBuffer(kGLMVertexBuffer, 1024 * 128, 0);	// up to 128 bytes per vert
	}
}

#define MAX_DEBUG_CHARS 256
struct GLMDebugTextVertex
{
	float	x,y,z;
	float	u,v;
	char	rgba[4];
};

void GLMContext::DrawDebugText( float x, float y, float z, float drawCharWidth, float drawCharHeight, char *string )
{
	if (!m_debugFontTex)
	{
		GenDebugFontTex();
	}
	
	// setup needed to draw text
	
	// we're assuming that +x goes left to right on screen, no billboarding math in here
	// and that +y goes bottom up
	// caller knows projection / rectangle so it gets to decide vertex spacing
	
	// debug font must be bound to TMU 0
	// texturing enabled
	// alpha blending enabled
	// generate a quad per character
	//  characters are 6px wide by 11 px high.
	//	upper left character in tex is 0x20
	// y axis will need to be flipped for display
	
	// for any character in 0x20 - 0x7F - here are the needed UV's
	
	// leftU = ((character % 16) * 6.0f / 128.0f)
	// rightU = lowU + (6.0 / 128.0);
	// topV = ((character - 0x20) * 11.0f / 128.0f)
	// bottomV = lowV + (11.0f / 128.0f)
	
	int stringlen = strlen( string );
	if (stringlen > MAX_DEBUG_CHARS)
	{
		stringlen = MAX_DEBUG_CHARS;
	}

	// lock
	char					*vertices = NULL;
	GLMBuffLockParams		vtxLock;
	vtxLock.m_nOffset		= 0;
	vtxLock.m_nSize			= 1024 * stringlen;
	vtxLock.m_bNoOverwrite	= false;
	vtxLock.m_bDiscard		= false;
	m_debugFontVertices->Lock( &vtxLock, &vertices );
			
	GLMDebugTextVertex	*vtx =  (GLMDebugTextVertex*)vertices;
	GLMDebugTextVertex *vtxOutPtr = vtx;
	
	for( int charindex = 0; charindex < stringlen; charindex++ )
	{
		float	leftU,rightU,topV,bottomV;
		
		int character = (int)string[charindex];
		character -= 0x20;
		if ( (character<0) || (character > 0x7F) )
		{
			character = '*' - 0x20;
		}
		
		leftU	= ((character & 0x0F) * 6.0f ) / 128.0f;
		rightU	= leftU + (6.0f / 128.0f);

		topV	= ((character >> 4) * 11.0f ) / 128.0f;
		bottomV	= topV + (11.0f / 128.0f);
		
		float posx,posy,posz;
		
		posx = x + (drawCharWidth * (float)charindex);
		posy = y;
		posz = z;
		
		// generate four verts
		// first vert will be upper left of displayed quad (low X, high Y) then we go clockwise
		for( int quadvert = 0; quadvert < 4; quadvert++ )
		{
			bool isTop	= (quadvert <2);						// verts 0 and 1
			bool isLeft	= (quadvert & 1) == (quadvert >> 1);	// verts 0 and 3
			
			vtxOutPtr->x = posx + (isLeft ? 0.0f : drawCharWidth);
			vtxOutPtr->y = posy + (isTop ? drawCharHeight : 0.0f);
			vtxOutPtr->z = posz;
			
			vtxOutPtr->u = isLeft ? leftU : rightU;
			vtxOutPtr->v = isTop ? topV : bottomV;
			
			vtxOutPtr++;
		}
	}
	
	// verts are done.
	// unlock...
	
	m_debugFontVertices->Unlock();
	
	// make a vertex setup
	GLMVertexSetup vertSetup;
	
		// position, color, tc = 0, 3, 8
	vertSetup.m_attrMask = (1<<kGLMGenericAttr00) |  (1<<kGLMGenericAttr03) |  (1<<kGLMGenericAttr08);
	
	vertSetup.m_attrs[kGLMGenericAttr00].m_pBuffer = m_debugFontVertices;
	vertSetup.m_attrs[kGLMGenericAttr00].m_nCompCount	= 3;			// 3 floats
	vertSetup.m_attrs[kGLMGenericAttr00].m_datatype	= GL_FLOAT;
	vertSetup.m_attrs[kGLMGenericAttr00].m_stride	= sizeof(GLMDebugTextVertex);
	vertSetup.m_attrs[kGLMGenericAttr00].m_offset	= offsetof(GLMDebugTextVertex, x);
	vertSetup.m_attrs[kGLMGenericAttr00].m_normalized= false;

	vertSetup.m_attrs[kGLMGenericAttr03].m_pBuffer = m_debugFontVertices;
	vertSetup.m_attrs[kGLMGenericAttr03].m_nCompCount	= 4;			// four bytes
	vertSetup.m_attrs[kGLMGenericAttr03].m_datatype	= GL_UNSIGNED_BYTE;
	vertSetup.m_attrs[kGLMGenericAttr03].m_stride	= sizeof(GLMDebugTextVertex);
	vertSetup.m_attrs[kGLMGenericAttr03].m_offset	= offsetof(GLMDebugTextVertex, rgba);
	vertSetup.m_attrs[kGLMGenericAttr03].m_normalized= true;

	vertSetup.m_attrs[kGLMGenericAttr08].m_pBuffer = m_debugFontVertices;
	vertSetup.m_attrs[kGLMGenericAttr08].m_nCompCount	= 2;			// 2 floats
	vertSetup.m_attrs[kGLMGenericAttr08].m_datatype	= GL_FLOAT;
	vertSetup.m_attrs[kGLMGenericAttr08].m_stride	= sizeof(GLMDebugTextVertex);
	vertSetup.m_attrs[kGLMGenericAttr08].m_offset	= offsetof(GLMDebugTextVertex, u);
	vertSetup.m_attrs[kGLMGenericAttr03].m_normalized= false;

		
	// bind texture and draw it..
	CGLMTex *pPrevTex = m_samplers[0].m_pBoundTex;
	BindTexToTMU( m_debugFontTex, 0 );
	
	SelectTMU(0);	// somewhat redundant
	
	gGL->glDisable( GL_DEPTH_TEST );
	
	gGL->glEnable(GL_TEXTURE_2D);

	if (0)
	{
		gGL->glEnableClientState(GL_VERTEX_ARRAY);

		gGL->glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		
		gGL->glVertexPointer( 3, GL_FLOAT, sizeof( vtx[0] ), &vtx[0].x );
		
		gGL->glClientActiveTexture(GL_TEXTURE0);

		gGL->glTexCoordPointer( 2, GL_FLOAT, sizeof( vtx[0] ), &vtx[0].u );
	}
	else
	{
		SetVertexAttributes( &vertSetup );
	}

	gGL->glDrawArrays( GL_QUADS, 0, stringlen * 4 );

	// disable all the input streams
	if (0)
	{
		gGL->glDisableClientState(GL_VERTEX_ARRAY);

		gGL->glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	}
	else
	{
		SetVertexAttributes( NULL );
	}

	gGL->glDisable(GL_TEXTURE_2D);

	BindTexToTMU( pPrevTex, 0 );
}

//===============================================================================

void GLMgrSelfTests( void )	
{
	return;	// until such time as the tests are revised or axed
	
	GLMDisplayParams	glmParams;	
	glmParams.m_fsEnable					=	false;

	glmParams.m_vsyncEnable				=	false;	// "The runtime updates the window client area immediately and might do so more 
	glmParams.m_backBufferWidth				=	1024;
	glmParams.m_backBufferHeight			=	768;
	glmParams.m_backBufferFormat			=	D3DFMT_A8R8G8B8;
	glmParams.m_multiSampleCount			=	2;

	glmParams.m_enableAutoDepthStencil		=	true;
	glmParams.m_autoDepthStencilFormat		=	D3DFMT_D24S8;

	glmParams.m_fsRefreshHz					=	60;

	glmParams.m_mtgl						=	true;
	glmParams.m_focusWindow					=	0;

	// make a new context on renderer 0.
	GLMContext *ctx = GLMgr::aGLMgr()->NewContext( NULL, &glmParams );	////FIXME you can't make contexts this way any more.
	if (!ctx)
	{
		DebuggerBreak();		// no go
		return;
	}

	// make a test object based on that context.
	//int alltests[] = {0,1,2,3,   -1};
	//int newtests[] = {3, -1};
	int twotests[] = {2, -1};
	//int notests[] = {-1};
	
	int *testlist = twotests;

	GLMTestParams	params;
	memset( &params, 0, sizeof(params) );

	params.m_ctx = ctx;
	params.m_testList = testlist;

	params.m_glErrToDebugger = true;
	params.m_glErrToConsole = true;
	
	params.m_intlErrToDebugger = true;
	params.m_intlErrToConsole = true;
	
	params.m_frameCount = 1000;

	GLMTester testobj( &params );

	testobj.RunTests( );
	
	GLMgr::aGLMgr()->DelContext( ctx );
}

void GLMContext::SetDefaultStates( void )
{
	GLM_FUNC;
	CheckCurrent();

	m_AlphaTestEnable.Default();
	m_AlphaTestFunc.Default();

	m_AlphaToCoverageEnable.Default();
	
	m_CullFaceEnable.Default();
	m_CullFrontFace.Default();
	
	m_PolygonMode.Default();
	m_DepthBias.Default();

	m_ClipPlaneEnable.Default();
	m_ClipPlaneEquation.Default();
	
	m_ScissorEnable.Default();	
	m_ScissorBox.Default();
	
	m_ViewportBox.Default();		
	m_ViewportDepthRange.Default();

	m_ColorMaskSingle.Default();	
	m_ColorMaskMultiple.Default();

	m_BlendEnable.Default();
	m_BlendFactor.Default();
	m_BlendEquation.Default();
	m_BlendColor.Default();
	//m_BlendEnableSRGB.Default();	// this isn't useful until there is an FBO bound - in fact it will trip a GL error.

	m_DepthTestEnable.Default();
	m_DepthFunc.Default();
	m_DepthMask.Default();
	
	m_StencilTestEnable.Default();
	m_StencilFunc.Default();
	m_StencilOp.Default();
	m_StencilWriteMask.Default();

	m_ClearColor.Default();
	m_ClearDepth.Default();
	m_ClearStencil.Default();	
}

void GLMContext::VerifyStates		( void )
{
	GLM_FUNC;
	CheckCurrent();

	// bare bones sanity check, head over to the debugger if our sense of the current context state is not correct
	// we should only want to call this after a flush or the checks will flunk.
	
	if( m_AlphaTestEnable.Check() )			GLMStop();
	if( m_AlphaTestFunc.Check() )			GLMStop();

	if( m_AlphaToCoverageEnable.Check() )	GLMStop();	

	if( m_CullFaceEnable.Check() )			GLMStop();
	if( m_CullFrontFace.Check() )			GLMStop();
	
	if( m_PolygonMode.Check() )				GLMStop();
	if( m_DepthBias.Check() )				GLMStop();

	if( m_ClipPlaneEnable.Check() )			GLMStop();
	//if( m_ClipPlaneEquation.Check() )		GLMStop();
	
	if( m_ScissorEnable.Check() )			GLMStop();	
	if( m_ScissorBox.Check() )				GLMStop();
	

	if( m_ViewportBox.Check() )				GLMStop();		
	if( m_ViewportDepthRange.Check() )		GLMStop();

	if( m_ColorMaskSingle.Check() )			GLMStop();	
	if( m_ColorMaskMultiple.Check() )		GLMStop();

	if( m_BlendEnable.Check() )				GLMStop();
	if( m_BlendFactor.Check() )				GLMStop();
	if( m_BlendEquation.Check() )			GLMStop();
	if( m_BlendColor.Check() )				GLMStop();
	
	// only do this as caps permit
	if (m_caps.m_hasGammaWrites)
	{
		if( m_BlendEnableSRGB.Check() )		GLMStop();
	}

	if( m_DepthTestEnable.Check() )			GLMStop();
	if( m_DepthFunc.Check() )				GLMStop();
	if( m_DepthMask.Check() )				GLMStop();
	
	if( m_StencilTestEnable.Check() )		GLMStop();
	if( m_StencilFunc.Check() )				GLMStop();
	if( m_StencilOp.Check() )				GLMStop();
	if( m_StencilWriteMask.Check() )		GLMStop();

	if( m_ClearColor.Check() )				GLMStop();
	if( m_ClearDepth.Check() )				GLMStop();
	if( m_ClearStencil.Check() )			GLMStop();
}

static inline uint GetDataTypeSizeInBytes( GLenum dataType ) 
{
	switch ( dataType )
	{
	case GL_BYTE:
	case GL_UNSIGNED_BYTE:
		return 1;
	case GL_SHORT:
	case GL_UNSIGNED_SHORT:
	case GL_HALF_FLOAT:
		return 2;
	case GL_INT:
	case GL_FLOAT:
		return 4;
	default:
		Assert( 0 );
		break;
	}
	return 0;
}

#ifndef OSX

void GLMContext::DrawRangeElementsNonInline( GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid *indices, uint baseVertex, CGLMBuffer *pIndexBuf )
{
#if GLMDEBUG
	GLM_FUNC;
#else
	//tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s %d-%d count:%d mode:%d type:%d", __FUNCTION__, start, end, count, mode, type );
#endif

	++m_nBatchCounter;

	SetIndexBuffer( pIndexBuf );

	void *indicesActual = (void*)indices;

	if ( pIndexBuf->m_bPseudo )
	{
		// you have to pass actual address, not offset
		indicesActual = (void*)( (int)indicesActual + (int)pIndexBuf->m_pPseudoBuf );
	}
	if (pIndexBuf->m_bUsingPersistentBuffer)
	{
		indicesActual = (void*)( (int)indicesActual + (int)pIndexBuf->m_nPersistentBufferStartOffset );
	}

#if GL_ENABLE_INDEX_VERIFICATION
	// Obviously only for debugging.
	if ( !pIndexBuf->IsSpanValid( (uint)indices, count * GetDataTypeSizeInBytes( type ) ) )
	{
		// The consumption range crosses more than one lock span, or the lock is trying to consume a bad IB range.
		DXABSTRACT_BREAK_ON_ERROR();
	}
		
	if ( ( type == GL_UNSIGNED_SHORT ) && ( pIndexBuf->m_bPseudo ) )
	{
		Assert( start <= end );
		for ( int i = 0; i < count; i++)
		{
			uint n = ((const uint16*)indicesActual)[i];
			if ( ( n < start ) || ( n > end ) )
			{
				DXABSTRACT_BREAK_ON_ERROR();
			}
		}

		unsigned char *pVertexShaderAttribMap = m_pDevice->m_vertexShader->m_vtxAttribMap;
		const int nMaxVertexAttributesToCheck = m_drawingProgram[ kGLMVertexProgram ]->m_maxVertexAttrs;

		IDirect3DVertexDeclaration9	*pVertDecl = m_pDevice->m_pVertDecl;
		const uint8	*pVertexAttribDescToStreamIndex = pVertDecl->m_VertexAttribDescToStreamIndex;

		// FIXME: Having to duplicate all this flush logic is terrible here
		for( int nMask = 1, nIndex = 0; nIndex < nMaxVertexAttributesToCheck; ++nIndex, nMask <<= 1 )
		{
			uint8 vertexShaderAttrib = pVertexShaderAttribMap[ nIndex ];

			uint nDeclIndex = pVertexAttribDescToStreamIndex[vertexShaderAttrib];
			if ( nDeclIndex == 0xFF )
				continue;

			D3DVERTEXELEMENT9_GL *pDeclElem = &pVertDecl->m_elements[nDeclIndex];

			Assert( ( ( vertexShaderAttrib >> 4 ) == pDeclElem->m_dxdecl.Usage ) && ( ( vertexShaderAttrib & 0x0F ) == pDeclElem->m_dxdecl.UsageIndex) );

			const uint nStreamIndex = pDeclElem->m_dxdecl.Stream;
			const D3DStreamDesc *pStream = &m_pDevice->m_streams[ nStreamIndex ];

			CGLMBuffer *pBuf = m_pDevice->m_vtx_buffers[ nStreamIndex ];
			if ( pBuf == m_pDevice->m_pDummy_vtx_buffer )
				continue;

			Assert( pStream->m_vtxBuffer->m_vtxBuffer == pBuf );

			int nBufOffset = pDeclElem->m_gldecl.m_offset + pStream->m_offset;
			Assert( nBufOffset >= 0 );
			Assert( nBufOffset < (int)pBuf->m_nSize );

			uint nBufSize = pStream->m_vtxBuffer->m_vtxBuffer->m_nSize;
			uint nDataTypeSize = GetDataTypeSizeInBytes( pDeclElem->m_gldecl.m_datatype );
			uint nActualStride = pStream->m_stride ? pStream->m_stride : nDataTypeSize;
			uint nStart = nBufOffset + ( start + baseVertex ) * nActualStride;
			uint nEnd = nBufOffset + ( end + baseVertex ) * nActualStride + nDataTypeSize;

			if ( nEnd > nBufSize )
			{
				DXABSTRACT_BREAK_ON_ERROR();
			}

			if ( !pStream->m_vtxBuffer->m_vtxBuffer->IsSpanValid( nStart, nEnd - nStart ) )
			{
				// The draw is trying to consume a range of the bound VB that hasn't been set to valid data!
				DXABSTRACT_BREAK_ON_ERROR();
			}
		}
	}
#endif

	Assert( m_drawingLang == kGLMGLSL );

	if ( m_pBoundPair )
	{
		gGL->glDrawRangeElementsBaseVertex( mode, start, end, count, type, indicesActual, baseVertex );

#if GLMDEBUG
		if ( m_slowCheckEnable )
		{
			CheckNative();
		}
#endif
	}
}

#else

// support for OSX 10.6 (no support for glDrawRangeElementsBaseVertex)
void GLMContext::DrawRangeElements(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid *indices, CGLMBuffer *pIndexBuf)
{
	GLM_FUNC;

	//	CheckCurrent();
	++m_nBatchCounter;				// batch index increments unconditionally on entry

	SetIndexBuffer( pIndexBuf );
	void *indicesActual = (void*)indices;    
	if ( pIndexBuf->m_bPseudo )
	{
		// you have to pass actual address, not offset
		indicesActual = (void*)( (int)indicesActual + (int)pIndexBuf->m_pPseudoBuf );
	} 
	if (pIndexBuf->m_bUsingPersistentBuffer)
	{
		indicesActual = (void*)( (int)indicesActual + (int)pIndexBuf->m_nPersistentBufferStartOffset );
	}

#if GLMDEBUG
	// init debug hook information
	GLMDebugHookInfo info;
	memset(&info, 0, sizeof(info));
	info.m_caller = eDrawElements;

	// relay parameters we're operating under
	info.m_drawMode = mode;
	info.m_drawStart = start;
	info.m_drawEnd = end;
	info.m_drawCount = count;
	info.m_drawType = type;
	info.m_drawIndices = indices;

	do
	{
		// obey global options re pre-draw clear
		if (m_autoClearColor || m_autoClearDepth || m_autoClearStencil)
		{
			GLMPRINTF(("-- DrawRangeElements auto clear"));
			this->DebugClear();
		}

		// always sync with editable shader text prior to draw
#if GLMDEBUG
		// TODO - fixup OSX 10.6 m_boundProg not used in this version togl (m_pBoundPair)
		//FIXME disengage this path if context is in GLSL mode..
		// it will need fixes to get the shader pair re-linked etc if edits happen anyway.

		if (m_boundProgram[kGLMVertexProgram])
		{
			m_boundProgram[kGLMVertexProgram]->SyncWithEditable();
		}
		else
		{
			AssertOnce(!"drawing with no vertex program bound");
		}

		if (m_boundProgram[kGLMFragmentProgram])
		{
			m_boundProgram[kGLMFragmentProgram]->SyncWithEditable();
		}
		else
		{
			AssertOnce(!"drawing with no fragment program bound");
		}
#endif

		// do the drawing
		if ( m_pBoundPair )
		{
			gGL->glDrawRangeElements(mode, start, end, count, type, indicesActual);
			// GLMCheckError();

			if (m_slowCheckEnable)
			{
				CheckNative();
			}
		}
		this->DebugHook(&info);
	} while (info.m_loop);
#else
	if ( m_pBoundPair )
	{
		gGL->glDrawRangeElements(mode, start, end, count, type, indicesActual);

#if GLMDEBUG
		if ( m_slowCheckEnable )
		{
			CheckNative();
		}
#endif
	}
#endif
}

#endif // !OSX

#if 0
// helper function to do enable or disable in one step
void glSetEnable( GLenum which, bool enable )
{
	if (enable)
		gGL->glEnable(which);
	else
		gGL->glDisable(which);
}

// helper function for int vs enum clarity
void glGetEnumv( GLenum which, GLenum *dst )
{
	gGL->glGetIntegerv( which, (int*)dst );
}
#endif

//===============================================================================


GLMTester::GLMTester(GLMTestParams *params)
{
	m_params = *params;
	
	m_drawFBO = NULL;
	m_drawColorTex = NULL;
	m_drawDepthTex = NULL;
}

GLMTester::~GLMTester()
{
}

void GLMTester::StdSetup( void )
{
	GLMContext *ctx = m_params.m_ctx;	

	m_drawWidth = 1024;
	m_drawHeight = 768;
	
	// make an FBO to draw into and activate it. no depth buffer yet	
	m_drawFBO = ctx->NewFBO();					

	// make color buffer texture

	GLMTexLayoutKey colorkey;
	//CGLMTex			*colortex;
	memset( &colorkey, 0, sizeof(colorkey) );
	
	colorkey.m_texGLTarget = GL_TEXTURE_2D;
	colorkey.m_xSize =	m_drawWidth;
	colorkey.m_ySize =	m_drawHeight;
	colorkey.m_zSize =	1;

	colorkey.m_texFormat	= D3DFMT_A8R8G8B8;
	colorkey.m_texFlags		= kGLMTexRenderable;

	m_drawColorTex = ctx->NewTex( &colorkey );

	// do not leave that texture bound on the TMU
	ctx->BindTexToTMU(NULL, 0 );
	
	
	// attach color to FBO
	GLMFBOTexAttachParams	colorParams;
	memset( &colorParams, 0, sizeof(colorParams) );
	
	colorParams.m_tex	= m_drawColorTex;
	colorParams.m_face	= 0;
	colorParams.m_mip	= 0;
	colorParams.m_zslice= 0;	// for clarity..
	
	m_drawFBO->TexAttach( &colorParams, kAttColor0 );
	
	// check it.
	bool ready = m_drawFBO->IsReady();
	InternalError( !ready, "drawing FBO no go");

	// bind it
	ctx->BindFBOToCtx( m_drawFBO, GL_FRAMEBUFFER_EXT );
	
	gGL->glViewport(0, 0, (GLsizei) m_drawWidth, (GLsizei) m_drawHeight );
	CheckGLError("stdsetup viewport");
	
	gGL->glScissor( 0,0,  (GLsizei) m_drawWidth, (GLsizei) m_drawHeight );
	CheckGLError("stdsetup scissor");

	gGL->glOrtho( -1,1, -1,1, -1,1 );
	CheckGLError("stdsetup ortho");
	
	// activate debug font
	ctx->GenDebugFontTex();
}

void GLMTester::StdCleanup( void )
{
	GLMContext *ctx = m_params.m_ctx;	

	// unbind
	ctx->BindFBOToCtx( NULL, GL_FRAMEBUFFER_EXT );
	
	// del FBO
	if (m_drawFBO)
	{
		ctx->DelFBO( m_drawFBO );
		m_drawFBO = NULL;
	}
	
	// del tex
	if (m_drawColorTex)
	{
		ctx->DelTex( m_drawColorTex );
		m_drawColorTex = NULL;
	}

	if (m_drawDepthTex)
	{
		ctx->DelTex( m_drawDepthTex );
		m_drawDepthTex = NULL;
	}
}


void GLMTester::Clear( void )
{
	GLMContext *ctx = m_params.m_ctx;	
	ctx->MakeCurrent();
	
	gGL->glViewport(0, 0, (GLsizei) m_drawWidth, (GLsizei) m_drawHeight );
	gGL->glScissor( 0,0,  (GLsizei) m_drawWidth, (GLsizei) m_drawHeight );
	gGL->glOrtho( -1,1, -1,1, -1,1 );
	CheckGLError("clearing viewport");

	// clear to black
	gGL->glClearColor(0.0f, 0.0f, 0.0, 1.0f);
	CheckGLError("clearing color");

	gGL->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	CheckGLError("clearing");

	//glFinish();
	//CheckGLError("clear finish");
}

void GLMTester::Present( int seed )
{
	GLMContext *ctx = m_params.m_ctx;	
	ctx->Present( m_drawColorTex );
}

void GLMTester::CheckGLError( const char *comment )
{
return;
	char errbuf[1024];

	//borrowed from GLMCheckError.. slightly different
	
	if (!comment)
	{
		comment = "";
	}
	
	GLenum errorcode = (GLenum)gGL->glGetError();
	GLenum errorcode2 = 0;
	if ( errorcode != GL_NO_ERROR )
	{
		const char	*decodedStr = GLMDecode( eGL_ERROR, errorcode );
		const char	*decodedStr2 = "";
				
		if ( errorcode == GL_INVALID_FRAMEBUFFER_OPERATION_EXT )
		{
			// dig up the more detailed FBO status
			errorcode2 = gGL->glCheckFramebufferStatusEXT( GL_FRAMEBUFFER_EXT );
			
			decodedStr2 = GLMDecode( eGL_ERROR, errorcode2 );

			sprintf( errbuf, "\n%s - GL Error %08x/%08x = '%s / %s'\n", comment, errorcode, errorcode2, decodedStr, decodedStr2 );
		}
		else
		{
			sprintf( errbuf, "\n%s - GL Error %08x = '%s'\n", comment, errorcode, decodedStr );
		}

		if ( m_params.m_glErrToConsole )
		{
			printf("%s", errbuf );
		}
		
		if ( m_params.m_glErrToDebugger )
		{
			DebuggerBreak();
		}
	}
}

void GLMTester::InternalError( int errcode, char *comment )
{
	if (errcode)
	{
		if (m_params.m_intlErrToConsole)
		{	
			printf("%s - error %d\n", comment, errcode );
		}

		if (m_params.m_intlErrToDebugger)
		{
			DebuggerBreak();
		}
	}
}


void GLMTester::RunTests( void )
{
	int *testList = m_params.m_testList;
	
	while( (*testList >=0) && (*testList < 20) )
	{
		RunOneTest( *testList++ );
	}
}

void GLMTester::RunOneTest( int testindex )
{
	// this might be better with 'ptmf' style
	switch(testindex)
	{
		case 0:	Test0();	break;
		case 1:	Test1();	break;
		case 2:	Test2();	break;
		case 3:	Test3();	break;

		default:
			DebuggerBreak();	// unrecognized
	}
}

// #####################################################################################################################

// some fixed lists which may be useful to all tests

D3DFORMAT g_drawTexFormatsGLMT[] =		// -1 terminated
{
	D3DFMT_A8R8G8B8,
	D3DFMT_A4R4G4B4,
	D3DFMT_X8R8G8B8,
	D3DFMT_X1R5G5B5,
	D3DFMT_A1R5G5B5,
	D3DFMT_L8,
	D3DFMT_A8L8,	
	D3DFMT_R8G8B8,	
	D3DFMT_A8,
	D3DFMT_R5G6B5,
	D3DFMT_DXT1,
	D3DFMT_DXT3,
	D3DFMT_DXT5,
	D3DFMT_A32B32G32R32F,
	D3DFMT_A16B16G16R16,

	(D3DFORMAT)-1
};

D3DFORMAT g_fboColorTexFormatsGLMT[] =		// -1 terminated
{
	D3DFMT_A8R8G8B8,
	//D3DFMT_A4R4G4B4,			//unsupported
	D3DFMT_X8R8G8B8,
	D3DFMT_X1R5G5B5,
	//D3DFMT_A1R5G5B5,			//unsupported
	D3DFMT_A16B16G16R16F,
	D3DFMT_A32B32G32R32F,
	D3DFMT_R5G6B5,

	(D3DFORMAT)-1			
};

D3DFORMAT g_fboDepthTexFormatsGLMT[] =		// -1 terminated, but note 0 for "no depth" mode
{
	(D3DFORMAT)0,
	D3DFMT_D16,
	D3DFMT_D24X8,
	D3DFMT_D24S8,
	
	(D3DFORMAT)-1	
};


// #####################################################################################################################

void GLMTester::Test0( void )
{
	// make and delete a bunch of textures.
	// lock and unlock them.
	// use various combos of - 

	//	âˆštexel format
	//	âˆš2D | 3D | cube map
	//	âˆšmipped / not
	//	âˆšPOT / NPOT
	//	large / small / square / rect
	//	square / rect
	
	GLMContext *ctx = m_params.m_ctx;	
	ctx->MakeCurrent();
	
	CUtlVector< CGLMTex* >	testTextures;		// will hold all the built textures
	
	// test stage loop
	// 0 is creation
	// 1 is lock/unlock
	// 2 is deletion
	
	for( int teststage = 0; teststage < 3; teststage++)
	{
		int innerindex = 0;	// increment at stage switch
		// format loop
		for( D3DFORMAT *fmtPtr = g_drawTexFormatsGLMT; *fmtPtr != ((D3DFORMAT)-1); fmtPtr++ )
		{
			// form loop
			GLenum	forms[] = { GL_TEXTURE_2D, GL_TEXTURE_3D, GL_TEXTURE_CUBE_MAP, (GLenum)-1 };

			for( GLenum *formPtr = forms; *formPtr != ((GLenum)-1); formPtr++ )
			{
				// mip loop
				for( int mipped = 0; mipped < 2; mipped++ )
				{
					// large / square / pot loop
					// &4 == large		&2 == square		&1 == POT
					// NOTE you *have to be square* for cube maps.
					
					for( int aspect = 0; aspect < 8; aspect++ )
					{
						switch( teststage )
						{
							case 0:
							{
								GLMTexLayoutKey key;
								memset( &key, 0, sizeof(key) );
								
								key.m_texGLTarget	= *formPtr;
								key.m_texFormat		= *fmtPtr;
								if (mipped)
									key.m_texFlags |= kGLMTexMipped;
								
								// assume big, square, POT, and 3D, then adjust as needed
								key.m_xSize = key.m_ySize = key.m_zSize = 256;
								
								if ( !(aspect&4) )		// big or little ?
								{
									// little
									key.m_xSize >>= 2;
									key.m_ySize >>= 2;
									key.m_zSize >>= 2;
								}
								
								if ( key.m_texGLTarget != GL_TEXTURE_CUBE_MAP )
								{
									if ( !(aspect & 2) )	// square or rect?
									{
										// rect
										key.m_ySize >>= 1;
										key.m_zSize >>= 2;
									}
								}
								
								if ( !(aspect&1) )		// POT or NPOT?
								{
									// NPOT
									key.m_xSize += 56;
									key.m_ySize += 56;
									key.m_zSize += 56;
								}
								
								// 2D, 3D, cube map ?
								if (key.m_texGLTarget!=GL_TEXTURE_3D)
								{
									// 2D or cube map: flatten Z extent to one texel
									key.m_zSize = 1;
								}
								else
								{
									// 3D: knock down Z quite a bit so our test case does not run out of RAM
									key.m_zSize >>= 3;
									if (!key.m_zSize)
									{
										key.m_zSize = 1;
									}
								}

								CGLMTex *newtex = ctx->NewTex( &key );
								CheckGLError( "tex create test");
								InternalError( newtex==NULL, "tex create test" );
								
								testTextures.AddToTail( newtex );
								printf("\n[%5d] created tex %s",innerindex,newtex->m_layout->m_layoutSummary );
							}
							break;

							case 1:
							{
								CGLMTex	*ptex = testTextures[innerindex];

								for( int face=0; face <ptex->m_layout->m_faceCount; face++)
								{
									for( int mip=0; mip <ptex->m_layout->m_mipCount; mip++)
									{
										GLMTexLockParams lockreq;
										
										lockreq.m_tex = ptex;
										lockreq.m_face = face;
										lockreq.m_mip = mip;

										GLMTexLayoutSlice *slice = &ptex->m_layout->m_slices[ ptex->CalcSliceIndex( face, mip ) ];
										
										lockreq.m_region.xmin = lockreq.m_region.ymin = lockreq.m_region.zmin = 0;
										lockreq.m_region.xmax = slice->m_xSize;
										lockreq.m_region.ymax = slice->m_ySize;
										lockreq.m_region.zmax = slice->m_zSize;
										
										char	*lockAddress;
										int		yStride;
										int		zStride;
										
										ptex->Lock( &lockreq, &lockAddress, &yStride, &zStride );
										CheckGLError( "tex lock test");
										InternalError( lockAddress==NULL, "null lock address");

										// write some texels of this flavor:
										//	red 75%  green 40%  blue 15%  alpha 80%
										
										GLMGenTexelParams gtp;

										gtp.m_format			=	ptex->m_layout->m_format->m_d3dFormat;
										gtp.m_dest				=	lockAddress;
										gtp.m_chunkCount		=	(slice->m_xSize * slice->m_ySize * slice->m_zSize) / (ptex->m_layout->m_format->m_chunkSize * ptex->m_layout->m_format->m_chunkSize);
										gtp.m_byteCountLimit	=	slice->m_storageSize;
										gtp.r = 0.75;
										gtp.g = 0.40;
										gtp.b = 0.15;
										gtp.a = 0.80;

										GLMGenTexels( &gtp );
										
										InternalError( gtp.m_bytesWritten != gtp.m_byteCountLimit, "byte count mismatch from GLMGenTexels" );
									}
								}

								for( int face=0; face <ptex->m_layout->m_faceCount; face++)
								{
									for( int mip=0; mip <ptex->m_layout->m_mipCount; mip++)
									{
										GLMTexLockParams unlockreq;
										
										unlockreq.m_tex = ptex;
										unlockreq.m_face = face;
										unlockreq.m_mip = mip;

										// region need not matter for unlocks
										unlockreq.m_region.xmin = unlockreq.m_region.ymin = unlockreq.m_region.zmin = 0;
										unlockreq.m_region.xmax = unlockreq.m_region.ymax = unlockreq.m_region.zmax = 0;

										//char	*lockAddress;
										//int		yStride;
										//int		zStride;
										
										ptex->Unlock( &unlockreq );

										CheckGLError( "tex unlock test");
									}
								}
								printf("\n[%5d] locked/wrote/unlocked tex %s",innerindex, ptex->m_layout->m_layoutSummary );
							}
							break;

							case 2:
							{
								CGLMTex	*dtex = testTextures[innerindex];

								printf("\n[%5d] deleting tex %s",innerindex, dtex->m_layout->m_layoutSummary );								
								ctx->DelTex( dtex );
								CheckGLError( "tex delete test");
							}
							break;
						}	// end stage switch
						innerindex++;
					}	// end aspect loop
				}	// end mip loop
			}	// end form loop
		}	// end format loop
	}	// end stage loop
}

// #####################################################################################################################
void GLMTester::Test1( void )
{
	// FBO exercises
	GLMContext *ctx = m_params.m_ctx;	
	ctx->MakeCurrent();

	// FBO color format loop
	for( D3DFORMAT *colorFmtPtr = g_fboColorTexFormatsGLMT; *colorFmtPtr != ((D3DFORMAT)-1); colorFmtPtr++ )
	{
		// FBO depth format loop
		for( D3DFORMAT *depthFmtPtr = g_fboDepthTexFormatsGLMT; *depthFmtPtr != ((D3DFORMAT)-1); depthFmtPtr++ )
		{
			// mip loop
			for( int mipped = 0; mipped < 2; mipped++ )
			{
				GLenum	forms[] = { GL_TEXTURE_2D, GL_TEXTURE_3D, GL_TEXTURE_CUBE_MAP, (GLenum)-1 };

				// form loop
				for( GLenum *formPtr = forms; *formPtr != ((GLenum)-1); formPtr++ )
				{
					//=============================================== make an FBO
					CGLMFBO *fbo = ctx->NewFBO();					

					//=============================================== make a color texture
					GLMTexLayoutKey colorkey;
					memset( &colorkey, 0, sizeof(colorkey) );
					
					switch(*formPtr)
					{
						case GL_TEXTURE_2D:
							colorkey.m_texGLTarget = GL_TEXTURE_2D;
							colorkey.m_xSize =	800;
							colorkey.m_ySize =	600;
							colorkey.m_zSize =	1;
						break;
						
						case GL_TEXTURE_3D:
							colorkey.m_texGLTarget = GL_TEXTURE_3D;
							colorkey.m_xSize =	800;
							colorkey.m_ySize =	600;
							colorkey.m_zSize =	32;
						break;
						
						case GL_TEXTURE_CUBE_MAP:
							colorkey.m_texGLTarget = GL_TEXTURE_CUBE_MAP;
							colorkey.m_xSize =	800;
							colorkey.m_ySize =	800;	// heh, cube maps have to have square sides...
							colorkey.m_zSize =	1;
						break;
					}

					colorkey.m_texFormat	= *colorFmtPtr;
					colorkey.m_texFlags		= kGLMTexRenderable;
					// decide if we want mips
					if (mipped)
					{
						colorkey.m_texFlags		|= kGLMTexMipped;
					}

					CGLMTex	*colorTex = ctx->NewTex( &colorkey );
					// Note that GLM will notice the renderable flag, and force texels to be written
					// so the FBO will be complete

					//=============================================== attach color
					GLMFBOTexAttachParams	colorParams;
					memset( &colorParams, 0, sizeof(colorParams) );
					
					colorParams.m_tex	= colorTex;
					colorParams.m_face	= (colorkey.m_texGLTarget == GL_TEXTURE_CUBE_MAP) ? 2 : 0;	// just steer to an alternate face as a test

					colorParams.m_mip	= (colorkey.m_texFlags & kGLMTexMipped) ? 2 : 0;	// pick non-base mip slice

					colorParams.m_zslice= (colorkey.m_texGLTarget == GL_TEXTURE_3D) ? 3 : 0;		// just steer to an alternate slice as a test;
					
					fbo->TexAttach( &colorParams, kAttColor0 );
					

					//=============================================== optional depth tex
					CGLMTex *depthTex = NULL;
					
					if (*depthFmtPtr > 0 )
					{
						GLMTexLayoutKey depthkey;
						memset( &depthkey, 0, sizeof(depthkey) );
						
						depthkey.m_texGLTarget		= GL_TEXTURE_2D;
						depthkey.m_xSize			= colorkey.m_xSize >> colorParams.m_mip;	// scale depth tex to match color tex
						depthkey.m_ySize			= colorkey.m_ySize >> colorParams.m_mip;
						depthkey.m_zSize			= 1;

						depthkey.m_texFormat		= *depthFmtPtr;
						depthkey.m_texFlags			= kGLMTexRenderable | kGLMTexIsDepth;		// no mips.
						if (depthkey.m_texFormat==D3DFMT_D24S8)
						{
							depthkey.m_texFlags |= kGLMTexIsStencil;
						}

						depthTex = ctx->NewTex( &depthkey );


						//=============================================== attach depth
						GLMFBOTexAttachParams	depthParams;
						memset( &depthParams, 0, sizeof(depthParams) );
						
						depthParams.m_tex	= depthTex;
						depthParams.m_face	= 0;
						depthParams.m_mip	= 0;
						depthParams.m_zslice= 0;
						
						EGLMFBOAttachment depthAttachIndex = (depthkey.m_texFlags & kGLMTexIsStencil) ? kAttDepthStencil : kAttDepth;
						fbo->TexAttach( &depthParams, depthAttachIndex );
					}

					printf("\n FBO:\n   color tex %s\n   depth tex %s",
						colorTex->m_layout->m_layoutSummary,
						depthTex ? depthTex->m_layout->m_layoutSummary : "none"
						);
					
					// see if FBO is happy
					bool ready = fbo->IsReady();

					printf("\n   -> %s\n", ready ? "pass" : "fail" );
					
					// unbind
					ctx->BindFBOToCtx( NULL, GL_FRAMEBUFFER_EXT );
					
					// del FBO
					ctx->DelFBO(fbo);
					
					// del texes
					ctx->DelTex( colorTex );
					if (depthTex) ctx->DelTex( depthTex );
				} // end form loop
			} // end mip loop
		} // end depth loop
	} // end color loop
}

// #####################################################################################################################

static int selftest2_seed = 0;	// inc this every run to force main thread to teardown/reset display view
void GLMTester::Test2( void )
{
	GLMContext *ctx = m_params.m_ctx;	
	ctx->MakeCurrent();

	StdSetup();	// default test case drawing setup

	// draw stuff (loop...)
	for( int i=0; i<m_params.m_frameCount; i++)
	{
		// ramping shades of blue...
		GLfloat clear_color[4] = { 0.50f, 0.05f, ((float)(i%100)) / 100.0f, 1.0f };		
		gGL->glClearColor(clear_color[0], clear_color[1], clear_color[2], clear_color[3]);
		CheckGLError("test2 clear color");

		gGL->glClear(GL_COLOR_BUFFER_BIT+GL_DEPTH_BUFFER_BIT+GL_STENCIL_BUFFER_BIT);
		CheckGLError("test2 clearing");

		// try out debug text
		for( int j=0; j<16; j++)
		{
			char text[256];
			sprintf(text, "The quick brown fox jumped over the lazy dog %d times", i );
			
			float theta = ( (i*0.10f) + (j * 6.28f) ) / 16.0f;
			
			float posx = cos(theta) * 0.5;
			float posy = sin(theta) * 0.5;
			
			float charwidth = 6.0 * (2.0 / 1024.0);
			float charheight = 11.0 * (2.0 / 768.0);
			
			ctx->DrawDebugText( posx, posy, 0.0f, charwidth, charheight, text );
		}
		gGL->glFinish();
		CheckGLError("test2 finish");

		Present( selftest2_seed );
	}
	
	StdCleanup();
	
	selftest2_seed++;
}

// #####################################################################################################################

static char g_testVertexProgram01 [] = 
{
	"!!ARBvp1.0  \n"
	"TEMP vertexClip;  \n"
	"DP4 vertexClip.x, state.matrix.mvp.row[0], vertex.position;  \n"
	"DP4 vertexClip.y, state.matrix.mvp.row[1], vertex.position;  \n"
	"DP4 vertexClip.z, state.matrix.mvp.row[2], vertex.position;  \n"
	"DP4 vertexClip.w, state.matrix.mvp.row[3], vertex.position;  \n"
	"ADD vertexClip.y, vertexClip.x, vertexClip.y;  \n"
	"MOV result.position, vertexClip;  \n"
	"MOV result.color, vertex.color;  \n"
	"MOV result.texcoord[0], vertex.texcoord;  \n"
	"END  \n"
};

static char g_testFragmentProgram01 [] =
{
	"!!ARBfp1.0  \n"
	"TEMP color;  \n"
	"MUL color, fragment.texcoord[0].y, 2.0;  \n"
	"ADD color, 1.0, -color;  \n"
	"ABS color, color;  \n"
	"ADD result.color, 1.0, -color;  \n"
	"MOV result.color.a, 1.0;  \n"
	"END  \n"
};


// generic attrib versions..

static char g_testVertexProgram01_GA [] = 
{
	"!!ARBvp1.0  \n"
	"TEMP vertexClip;  \n"
	"DP4 vertexClip.x, state.matrix.mvp.row[0], vertex.attrib[0];  \n"
	"DP4 vertexClip.y, state.matrix.mvp.row[1], vertex.attrib[0];  \n"
	"DP4 vertexClip.z, state.matrix.mvp.row[2], vertex.attrib[0];  \n"
	"DP4 vertexClip.w, state.matrix.mvp.row[3], vertex.attrib[0];  \n"
	"ADD vertexClip.y, vertexClip.x, vertexClip.y;  \n"
	"MOV result.position, vertexClip;  \n"
	"MOV result.color, vertex.attrib[3];  \n"
	"MOV result.texcoord[0], vertex.attrib[8];  \n"
	"END  \n"
};

static char g_testFragmentProgram01_GA [] =
{
	"!!ARBfp1.0  \n"
	"TEMP color;  \n"
	"TEX color, fragment.texcoord[0], texture[0], 2D;"
	//"MUL color, fragment.texcoord[0].y, 2.0;  \n"
	//"ADD color, 1.0, -color;  \n"
	//"ABS color, color;  \n"
	//"ADD result.color, 1.0, -color;  \n"
	//"MOV result.color.a, 1.0;  \n"
	"MOV result.color, color;  \n"
	"END  \n"
};


void GLMTester::Test3( void )
{
	/**************************
	XXXXXXXXXXXXXXXXXXXXXX	stale test code until we revise the program interface
		
	GLMContext *ctx = m_params.m_ctx;	
	ctx->MakeCurrent();

	StdSetup();	// default test case drawing setup

	// make vertex&pixel shader
	CGLMProgram *vprog = ctx->NewProgram( kGLMVertexProgram, g_testVertexProgram01_GA );
	ctx->BindProgramToCtx( kGLMVertexProgram, vprog );
	
	CGLMProgram *fprog = ctx->NewProgram( kGLMFragmentProgram, g_testFragmentProgram01_GA );
	ctx->BindProgramToCtx( kGLMFragmentProgram, fprog );
	
	// draw stuff (loop...)
	for( int i=0; i<m_params.m_frameCount; i++)
	{
		// ramping shades of blue...
		GLfloat clear_color[4] = { 0.50f, 0.05f, ((float)(i%100)) / 100.0, 1.0f };		
		glClearColor(clear_color[0], clear_color[1], clear_color[2], clear_color[3]);
		CheckGLError("test3 clear color");

		glClear(GL_COLOR_BUFFER_BIT+GL_DEPTH_BUFFER_BIT+GL_STENCIL_BUFFER_BIT);
		CheckGLError("test3 clearing");

		// try out debug text
		for( int j=0; j<16; j++)
		{
			char text[256];
			sprintf(text, "This here is running through a trivial vertex shader");
			
			float theta = ( (i*0.10f) + (j * 6.28f) ) / 16.0f;
			
			float posx = cos(theta) * 0.5;
			float posy = sin(theta) * 0.5;
			
			float charwidth = 6.0 * (2.0 / 800.0);
			float charheight = 11.0 * (2.0 / 640.0);
			
			ctx->DrawDebugText( posx, posy, 0.0f, charwidth, charheight, text );
		}
		glFinish();
		CheckGLError("test3 finish");

		Present( 3333 );
	}
	
	StdCleanup();
	*****************************/
}

#if GLMDEBUG
void GLMTriggerDebuggerBreak()
{
	// we call an obscure GL function which we know has been breakpointed in the OGLP function list
	static signed short nada[] = { -1,-1,-1,-1 };
	gGL->glColor4sv( nada );
}
#endif
