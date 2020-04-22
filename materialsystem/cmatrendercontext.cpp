//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================

#include "pch_materialsystem.h"

#define MATSYS_INTERNAL

#include <math.h>
#include "cmatrendercontext.h"
#include "tier2/renderutils.h"
#include "cmaterialsystem.h"
#include "occlusionquerymgr.h"
#include "texturemanager.h"
#include "IHardwareConfigInternal.h"
#include "ctype.h"

#include "tier1/fmtstr.h"
#include "togl/rendermechanism.h"

// NOTE: This must be the last file included!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------

// FIXME: right now, always keeping shader API in sync, because debug overlays don't seem to work 100% with the delayed matrix loading
#define FORCE_MATRIX_SYNC 1

#ifdef VALIDATE_MATRICES
#define ShouldValidateMatrices() true
#else
#define ShouldValidateMatrices() false
#endif

#ifdef VALIDATE_MATRICES
#define AllowLazyMatrixSync() false
#define ForceSync()	((void)(0))
#elif defined(FORCE_MATRIX_SYNC)
#define AllowLazyMatrixSync() false
#define ForceSync()	ForceSyncMatrix( m_MatrixMode )
#else
#define AllowLazyMatrixSync() true
#define ForceSync() ((void)(0))
#endif

#ifdef _X360
static bool s_bDirtyDisk = false;
#endif


void ValidateMatrices( const VMatrix &m1, const VMatrix &m2, float eps = .001 )
{
	if ( !ShouldValidateMatrices() )
		return;

	for ( int i = 0; i < 16; i++ )
	{
		AssertFloatEquals( m1.Base()[i], m1.Base()[i], eps );
	}
}


//-----------------------------------------------------------------------------
// The dirty disk error report function (NOTE: Could be called from any thread!)
//-----------------------------------------------------------------------------
#ifdef _X360
unsigned ThreadedDirtyDiskErrorDisplay( void *pParam )
{
	XShowDirtyDiscErrorUI( XBX_GetPrimaryUserId() );
}
#endif


void SpinPresent()
{
	while ( true )
	{
		g_pShaderAPI->ClearColor3ub( 0, 0, 0 );
		g_pShaderAPI->ClearBuffers( true, true, true, -1, -1 );
		g_pShaderDevice->Present();
	}
}

void ReportDirtyDisk()
{
#ifdef _X360
	s_bDirtyDisk = true;
	ThreadHandle_t h = CreateSimpleThread( ThreadedDirtyDiskErrorDisplay, NULL );
	ThreadSetPriority( h, THREAD_PRIORITY_HIGHEST );

	// If this is being called from the render thread, immediately swap
	if ( ( ThreadGetCurrentId() == MaterialSystem()->GetRenderThreadId() ) ||
		( ThreadInMainThread() && g_pMaterialSystem->GetThreadMode() != MATERIAL_QUEUED_THREADED ) )
	{
		SpinPresent();
	}
#endif
}


//-----------------------------------------------------------------------------
// Install dirty disk error reporting function (call after SetMode)
//-----------------------------------------------------------------------------
void SetupDirtyDiskReportFunc()
{
	g_pFullFileSystem->InstallDirtyDiskReportFunc( ReportDirtyDisk );
}


//-----------------------------------------------------------------------------
// Globals
//-----------------------------------------------------------------------------
CMemoryStack CMatRenderContextBase::sm_RenderData[2];
int	CMatRenderContextBase::sm_nRenderLockCount = 0;
int	CMatRenderContextBase::sm_nRenderStack = 0;
int	CMatRenderContextBase::sm_nInitializeCount = 0;


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CMatRenderContextBase::CMatRenderContextBase() :
	m_pMaterialSystem( NULL ), m_RenderTargetStack( 16, 32 ), m_MatrixMode( NUM_MATRIX_MODES )
{
	int i;

	m_bDirtyViewState = true;

	// Put a special element at the top of the RT stack (indicating back buffer is current top of stack)
	// NULL indicates back buffer, -1 indicates full-size viewport
#if !defined( _X360 )
                RenderTargetStackElement_t initialElement = { {NULL, NULL, NULL, NULL}, NULL, 0, 0, -1, -1 };
#else
                RenderTargetStackElement_t initialElement = { {NULL}, NULL, 0, 0, -1, -1 };
#endif


	m_RenderTargetStack.Push( initialElement );

	for ( i = 0; i < MAX_FB_TEXTURES; i++ )
	{
		m_pCurrentFrameBufferCopyTexture[i] = NULL;
	}

	m_pCurrentMaterial = NULL;
	m_pCurrentProxyData = NULL;
	m_pUserDefinedLightmap = NULL;
	m_HeightClipMode = MATERIAL_HEIGHTCLIPMODE_DISABLE;
	m_HeightClipZ = 0.0f;
	m_bEnableClipping = true;
	m_bFlashlightEnable = false;
	m_bFullFrameDepthIsValid = false;

	for ( i = 0; i < NUM_MATRIX_MODES; i++ )
	{
		m_MatrixStacks[i].Push();
		m_MatrixStacks[i].Top().matrix.Identity();
		m_MatrixStacks[i].Top().flags |= ( MSF_DIRTY| MSF_IDENTITY );
	}
	m_pCurMatrixItem = &m_MatrixStacks[0].Top();

	m_Viewport.Init( 0, 0, 0, 0 );

	m_LastSetToneMapScale=Vector(1,1,1);
	m_CurToneMapScale=1.0;
	m_GoalToneMapScale = 1.0f;
}


//-----------------------------------------------------------------------------
// Init, shutdown
//-----------------------------------------------------------------------------
InitReturnVal_t CMatRenderContextBase::Init( )
{
	MEM_ALLOC_CREDIT();
	if ( !sm_nInitializeCount )
	{
		int nSize = 2200 * 1024;
		int nCommitSize = 32 * 1024;

#ifdef SWDS
		nSize = nCommitSize = 1024;
#endif

		const char *gamedir = CommandLine()->ParmValue("-game", CommandLine()->ParmValue( "-defaultgamedir", "hl2" ) );
		if ( gamedir && !Q_stricmp( "garrysmod", gamedir ) )
		{
			nSize = 4400 * 1024;
		}

		sm_RenderData[0].Init( nSize, nCommitSize, 0, 32 );
		sm_RenderData[1].Init( nSize, nCommitSize, 0, 32 );
		sm_nRenderStack = 0;
		sm_nRenderLockCount = 0;
	}
	++sm_nInitializeCount;
	return INIT_OK;
}

void CMatRenderContextBase::Shutdown( )
{
	Assert( sm_nInitializeCount >= 0 );
	if ( --sm_nInitializeCount == 0 )
	{
		sm_RenderData[0].Term();
		sm_RenderData[1].Term();
	}
}

void CMatRenderContextBase::CompactMemory()
{
	if ( sm_nRenderLockCount )
	{
		DevWarning( "CMatRenderContext: Trying to compact with render data still locked!\n" );
		sm_nRenderLockCount = 0;
	}
	sm_RenderData[0].FreeAll();
	sm_RenderData[1].FreeAll();
}

void CMatRenderContextBase::MarkRenderDataUnused( bool bFrameBegin )
{
	if ( sm_nRenderLockCount )
	{
		DevWarning( "CMatRenderContext: Trying to clear render data with render data still locked (%d)!\n", sm_nRenderLockCount );
		sm_nRenderLockCount = 0;
	}


	// JAY: DO NOT MERGE FROM TF2 - L4D HAS CHANGED THE UNDERLYING INTERFACE IN A WAY THAT DOESN'T REQUIRE THIS
#if 0
	// Switch stacks
	if ( bFrameBegin )
	{
		sm_nRenderStack = 1 - sm_nRenderStack;
	}

	// Clear the new stack
#ifdef _DEBUG
	memset( sm_RenderData[sm_nRenderStack].GetBase(), 0xFF, RenderDataSizeUsed() );
#endif
	sm_RenderData[ sm_nRenderStack ].FreeAll( false );
#else
	// Just for TF2, don't free the stack until the end of frame.  TF2 Allocates render data and holds it over the lock
	// period because we haven't revised the studiorender interface yet to change patterns.
	// Switch stacks
	if ( bFrameBegin )
	{
		sm_nRenderStack = 1 - sm_nRenderStack;
		// Clear the new stack
#ifdef _DEBUG
		memset( sm_RenderData[sm_nRenderStack].GetBase(), 0xFF, RenderDataSizeUsed() );
#endif
		sm_RenderData[ sm_nRenderStack ].FreeAll( false );
	}
#endif


}

int CMatRenderContextBase::RenderDataSizeUsed() const
{
	return sm_RenderData[sm_nRenderStack].GetUsed();
}

bool CMatRenderContextBase::IsRenderData( const void *pData ) const
{
	intp nData = (intp)pData;
	intp nBaseAddress = (intp)sm_RenderData[sm_nRenderStack].GetBase();
	intp nLastAddress = nBaseAddress + RenderDataSizeUsed();
	return ( nData == 0 ) || ( nData >= nBaseAddress && nData < nLastAddress );
}


//-----------------------------------------------------------------------------
// debug logging - empty in base class
//-----------------------------------------------------------------------------

void	CMatRenderContextBase::PrintfVA( char *fmt, va_list vargs )
{
}

void	CMatRenderContextBase::Printf( const char *fmt, ... )
{
}

float	CMatRenderContextBase::Knob( char *knobname, float *setvalue )
{
	return 0.0f;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------

#define g_pShaderAPI Cannot_use_ShaderAPI_in_CMatRenderContextBase

void CMatRenderContextBase::InitializeFrom( CMatRenderContextBase *pInitialState )
{
	int i;

	m_pCurrentMaterial = pInitialState->m_pCurrentMaterial;
	m_pCurrentProxyData = pInitialState->m_pCurrentProxyData;
	m_lightmapPageID = pInitialState->m_lightmapPageID;
	m_pUserDefinedLightmap = pInitialState->m_pUserDefinedLightmap;
	m_pLocalCubemapTexture = pInitialState->m_pLocalCubemapTexture;

	memcpy( m_pCurrentFrameBufferCopyTexture, pInitialState->m_pCurrentFrameBufferCopyTexture, MAX_FB_TEXTURES * sizeof(ITexture *) );

	m_bEnableClipping = pInitialState->m_bEnableClipping;

	m_HeightClipMode = pInitialState->m_HeightClipMode;
	m_HeightClipZ = pInitialState->m_HeightClipZ;

	m_pBoundMorph = pInitialState->m_pBoundMorph; // not reference counted?

	m_RenderTargetStack.Clear();
	m_RenderTargetStack.EnsureCapacity( pInitialState->m_RenderTargetStack.Count() );

	for ( i = 0; i < pInitialState->m_RenderTargetStack.Count(); i++ )
	{
		m_RenderTargetStack.Push( pInitialState->m_RenderTargetStack[i] );
	}

	m_MatrixMode = pInitialState->m_MatrixMode;
	for ( i = 0; i < NUM_MATRIX_MODES; i++ )
	{
		m_MatrixStacks[i].CopyFrom( pInitialState->m_MatrixStacks[i] );
	}

	m_bFlashlightEnable = pInitialState->m_bFlashlightEnable;

	m_FrameTime = pInitialState->m_FrameTime;
	m_GoalToneMapScale = pInitialState->m_GoalToneMapScale;
	m_CurToneMapScale = pInitialState->m_CurToneMapScale;
	m_LastSetToneMapScale = pInitialState->m_LastSetToneMapScale;
}

void CMatRenderContextBase::Bind( IMaterial *iMaterial, void *proxyData )
{
	IMaterialInternal *material = static_cast<IMaterialInternal *>( iMaterial );

	if ( !material )
	{
		Warning( "Programming error: CMatRenderContext::Bind: NULL material\n" );
		material = static_cast<IMaterialInternal *>( g_pErrorMaterial );
	}
	material = material->GetRealTimeVersion(); //always work with the real time versions of materials internally

	if ( GetCurrentMaterialInternal() != material )
	{
		if( !material->IsPrecached() )
		{
			DevWarning( "Binding uncached material \"%s\", artificially incrementing refcount\n", material->GetName() );
			material->ArtificialAddRef();
			material->Precache();
		}
		SetCurrentMaterialInternal(material);
	}

	SetCurrentProxy( proxyData );
}

void CMatRenderContextBase::BindLightmapPage( int lightmapPageID )
{
	m_lightmapPageID = lightmapPageID;

}

void CMatRenderContextBase::SetRenderTargetEx( int nRenderTargetID, ITexture *pNewTarget ) 
{
	// Verify valid top of RT stack
	Assert ( m_RenderTargetStack.Count() > 0 );

	// Reset the top of stack to the new target with old viewport
	RenderTargetStackElement_t newTOS = m_RenderTargetStack.Top();
	newTOS.m_pRenderTargets[nRenderTargetID] = pNewTarget;
	m_RenderTargetStack.Pop( );
	m_RenderTargetStack.Push( newTOS );
}

void CMatRenderContextBase::BindLocalCubemap( ITexture *pTexture )
{
	if( pTexture )
	{
		m_pLocalCubemapTexture = pTexture;
	}
	else
	{
		m_pLocalCubemapTexture = TextureManager()->ErrorTexture();
	}
}

ITexture *CMatRenderContextBase::GetRenderTarget( void )
{
	if (m_RenderTargetStack.Count() > 0)
	{
		return m_RenderTargetStack.Top().m_pRenderTargets[0];
	}
	else
	{
		return NULL; // should this be something else, since NULL indicates back buffer?
	}
}

ITexture *CMatRenderContextBase::GetRenderTargetEx( int nRenderTargetID  )
{
	// Verify valid top of stack
	Assert ( m_RenderTargetStack.Count() > 0 );

	// Top of render target stack
	ITexture *pTexture = m_RenderTargetStack.Top().m_pRenderTargets[ nRenderTargetID ];
	return pTexture;
}

void CMatRenderContextBase::SetFrameBufferCopyTexture( ITexture *pTexture, int textureIndex )
{
	if( textureIndex < 0 || textureIndex > MAX_FB_TEXTURES )
	{
		Assert( 0 );
		return;
	}

	// FIXME: Do I need to increment/decrement ref counts here, or assume that the app is going to do it?
	m_pCurrentFrameBufferCopyTexture[textureIndex] = pTexture;
}

ITexture *CMatRenderContextBase::GetFrameBufferCopyTexture( int textureIndex )
{
	if( textureIndex < 0 || textureIndex > MAX_FB_TEXTURES )
	{
		Assert( 0 );
		return NULL; // FIXME!  This should return the error texture.
	}
	return m_pCurrentFrameBufferCopyTexture[textureIndex];
}


void CMatRenderContextBase::MatrixMode( MaterialMatrixMode_t mode )
{
	Assert( m_MatrixStacks[mode].Count() );
	m_MatrixMode = mode;
	m_pCurMatrixItem = &m_MatrixStacks[mode].Top();
}

void CMatRenderContextBase::CurrentMatrixChanged()
{
	if ( m_MatrixMode == MATERIAL_VIEW )
	{
		m_bDirtyViewState = true;
		m_bDirtyViewProjState = true;
	}
	else if ( m_MatrixMode == MATERIAL_PROJECTION )
	{
		m_bDirtyViewProjState = true;
	}
}

void CMatRenderContextBase::PushMatrix()
{
	CUtlStack<MatrixStackItem_t> &curStack = m_MatrixStacks[ m_MatrixMode ];
	Assert( curStack.Count() );
	int iNew = curStack.Push();
	curStack[ iNew ] = curStack[ iNew - 1 ];
	m_pCurMatrixItem = &curStack.Top();
	CurrentMatrixChanged();
}

void CMatRenderContextBase::PopMatrix()
{
	Assert( m_MatrixStacks[m_MatrixMode].Count() > 1 );
	m_MatrixStacks[ m_MatrixMode ].Pop();
	m_pCurMatrixItem = &m_MatrixStacks[m_MatrixMode].Top();
	CurrentMatrixChanged();
}

void CMatRenderContextBase::LoadMatrix( const VMatrix& matrix )
{
	m_pCurMatrixItem->matrix = matrix;
	m_pCurMatrixItem->flags = MSF_DIRTY; // clearing identity implicitly
	CurrentMatrixChanged();
}

void CMatRenderContextBase::LoadMatrix( const matrix3x4_t& matrix )
{
	m_pCurMatrixItem->matrix = matrix;
	m_pCurMatrixItem->flags = MSF_DIRTY; // clearing identity implicitly
	CurrentMatrixChanged();
}

void CMatRenderContextBase::MultMatrix( const VMatrix& matrix )
{
	VMatrix result;

	MatrixMultiply( matrix, m_pCurMatrixItem->matrix, result );
	m_pCurMatrixItem->matrix = result;
	m_pCurMatrixItem->flags = MSF_DIRTY; // clearing identity implicitly
	CurrentMatrixChanged();
}

void CMatRenderContextBase::MultMatrix( const matrix3x4_t& matrix )
{
	CMatRenderContextBase::MultMatrix( VMatrix( matrix ) );
}

void CMatRenderContextBase::MultMatrixLocal( const VMatrix& matrix )
{
	VMatrix result;
	MatrixMultiply( m_pCurMatrixItem->matrix, matrix, result );
	m_pCurMatrixItem->matrix = result;
	m_pCurMatrixItem->flags = MSF_DIRTY; // clearing identity implicitly
	CurrentMatrixChanged();
}

void CMatRenderContextBase::MultMatrixLocal( const matrix3x4_t& matrix )
{
	CMatRenderContextBase::MultMatrixLocal( VMatrix( matrix ) );
}

void CMatRenderContextBase::LoadIdentity()
{
	// FIXME: possibly track is identity so can call shader API LoadIdentity() later instead of LoadMatrix()?
	m_pCurMatrixItem->matrix.Identity();
	m_pCurMatrixItem->flags = ( MSF_DIRTY | MSF_IDENTITY );
	CurrentMatrixChanged();
}

void CMatRenderContextBase::Ortho( double left, double top, double right, double bottom, double zNear, double zFar )
{
	MatrixOrtho( m_pCurMatrixItem->matrix, left, top, right, bottom, zNear, zFar );
	m_pCurMatrixItem->flags = MSF_DIRTY; 
}

void CMatRenderContextBase::PerspectiveX( double flFovX, double flAspect, double flZNear, double flZFar )
{
	MatrixPerspectiveX( m_pCurMatrixItem->matrix, flFovX, flAspect, flZNear, flZFar );
	m_pCurMatrixItem->flags = MSF_DIRTY; 
}

void CMatRenderContextBase::PerspectiveOffCenterX( double flFovX, double flAspect, double flZNear, double flZFar, double bottom, double top, double left, double right )
{
	MatrixPerspectiveOffCenterX( m_pCurMatrixItem->matrix, flFovX, flAspect, flZNear, flZFar, bottom, top, left, right );
	m_pCurMatrixItem->flags = MSF_DIRTY; 
}

void CMatRenderContextBase::PickMatrix( int x, int y, int nWidth, int nHeight )
{
	int vx, vy, vwidth, vheight;
	GetViewport( vx, vy, vwidth, vheight );

	// Compute the location of the pick region in projection space...
	float px = 2.0 * (float)(x - vx) / (float)vwidth - 1;
	float py = 2.0 * (float)(y - vy)/ (float)vheight - 1;
	float pw = 2.0 * (float)nWidth / (float)vwidth;
	float ph = 2.0 * (float)nHeight / (float)vheight;

	// we need to translate (px, py) to the origin
	// and scale so (pw,ph) -> (2, 2)
	VMatrix mat;
	MatrixSetIdentity( mat );
	mat.m[0][0] = 2.0 / pw;
	mat.m[1][1] = 2.0 / ph;
	mat.m[0][3] = -2.0 * px / pw;
	mat.m[1][3] = -2.0 * py / ph;

	CMatRenderContextBase::MultMatrixLocal( mat );
}

void CMatRenderContextBase::Rotate( float flAngle, float x, float y, float z )
{
	MatrixRotate( m_pCurMatrixItem->matrix, Vector( x, y, z ), flAngle );
	m_pCurMatrixItem->flags = MSF_DIRTY; 
}

void CMatRenderContextBase::Translate( float x, float y, float z )
{
	MatrixTranslate( m_pCurMatrixItem->matrix, Vector( x, y, z ) );
	m_pCurMatrixItem->flags = MSF_DIRTY; 
}

void CMatRenderContextBase::Scale( float x, float y, float z )
{
	VMatrix mat;
	MatrixBuildScale( mat, x, y, z );
	CMatRenderContextBase::MultMatrixLocal( mat );
}

void CMatRenderContextBase::GetMatrix( MaterialMatrixMode_t matrixMode, VMatrix *pMatrix )
{
	CUtlStack<MatrixStackItem_t> &stack = m_MatrixStacks[ matrixMode ];

	if ( !stack.Count() )
	{
		pMatrix->Identity();
		return;
	}

	*pMatrix = stack.Top().matrix;
}

void CMatRenderContextBase::GetMatrix( MaterialMatrixMode_t matrixMode, matrix3x4_t *pMatrix )
{
	CUtlStack<MatrixStackItem_t> &stack = m_MatrixStacks[ matrixMode ];

	if ( !stack.Count() )
	{
		SetIdentityMatrix( *pMatrix );
		return;
	}

	*pMatrix = stack.Top().matrix.As3x4();
}

void CMatRenderContextBase::RecomputeViewState()
{
	if ( !m_bDirtyViewState )
		return;
	m_bDirtyViewState = false;

	// FIXME: Cache this off to make it less expensive?
	matrix3x4_t viewMatrix;
	GetMatrix( MATERIAL_VIEW, &viewMatrix );
	m_vecViewOrigin.x = 
		-( viewMatrix[0][3] * viewMatrix[0][0] + 
		viewMatrix[1][3] * viewMatrix[1][0] + 
		viewMatrix[2][3] * viewMatrix[2][0] );
	m_vecViewOrigin.y = 
		-( viewMatrix[0][3] * viewMatrix[0][1] + 
		viewMatrix[1][3] * viewMatrix[1][1] + 
		viewMatrix[2][3] * viewMatrix[2][1] );
	m_vecViewOrigin.z = 
		-( viewMatrix[0][3] * viewMatrix[0][2] + 
		viewMatrix[1][3] * viewMatrix[1][2] + 
		viewMatrix[2][3] * viewMatrix[2][2] );

	// FIXME Implement computation of m_vecViewForward, etc
	m_vecViewForward.Init();
	m_vecViewRight.Init();

	// FIXME: Is this correct?
	m_vecViewUp.Init( viewMatrix[1][0], viewMatrix[1][1], viewMatrix[1][2] );
}

void CMatRenderContextBase::GetWorldSpaceCameraPosition( Vector *pCameraPos )
{
	RecomputeViewState();
	VectorCopy( m_vecViewOrigin, *pCameraPos );
}

void CMatRenderContextBase::GetWorldSpaceCameraVectors( Vector *pVecForward, Vector *pVecRight, Vector *pVecUp )
{
	RecomputeViewState();

	// FIXME Implement computation of m_vecViewForward
	Assert( 0 );

	if ( pVecForward )
	{
		VectorCopy( m_vecViewForward, *pVecForward );
	}
	if ( pVecRight )
	{				   
		VectorCopy( m_vecViewRight, *pVecRight );
	}
	if ( pVecUp )
	{
		VectorCopy( m_vecViewUp, *pVecUp );
	}
}

void *CMatRenderContextBase::LockRenderData( int nSizeInBytes )
{
	MEM_ALLOC_CREDIT();
	void *pDest = sm_RenderData[ sm_nRenderStack ].Alloc( nSizeInBytes, false );
	if ( !pDest )
	{
		ExecuteNTimes( 10, Warning("MaterialSystem: Out of memory in render data!\n") );
	}
	AddRefRenderData();
	return pDest;
}

void CMatRenderContextBase::UnlockRenderData( void *pData )
{
	ReleaseRenderData();
}

void CMatRenderContextBase::AddRefRenderData()
{
	++sm_nRenderLockCount;
}

void CMatRenderContextBase::ReleaseRenderData()
{
	--sm_nRenderLockCount;
	Assert( sm_nRenderLockCount >= 0 );
	if ( sm_nRenderLockCount == 0 )
	{
		OnRenderDataUnreferenced();
	}
}

void CMatRenderContextBase::SyncMatrices()
{
}

void CMatRenderContextBase::SyncMatrix( MaterialMatrixMode_t mode )
{
}

void CMatRenderContextBase::SetHeightClipMode( enum MaterialHeightClipMode_t heightClipMode )
{
	if( m_HeightClipMode != heightClipMode  )
	{
		m_HeightClipMode = heightClipMode;
		UpdateHeightClipUserClipPlane();
		/*if ( HardwareConfig()->MaxUserClipPlanes() >= 1 && !HardwareConfig()->UseFastClipping())
		{
		UpdateHeightClipUserClipPlane();			
		}
		else
		{
		g_pShaderAPI->SetHeightClipMode( heightClipMode );
		}*/
	}
}

void CMatRenderContextBase::SetHeightClipZ( float z )
{
	if( z != m_HeightClipZ )
	{
		m_HeightClipZ = z;
		UpdateHeightClipUserClipPlane();
	}

	// FIXME!  : Need to move user clip plane support back to pre-dx9 cards (all of the pixel shaders
	// have texkill in them. . blich.)

	/*if ( HardwareConfig()->MaxUserClipPlanes() >= 1 && !HardwareConfig()->UseFastClipping() )
	{
	UpdateHeightClipUserClipPlane();			
	}
	else
	{
	g_pShaderAPI->SetHeightClipZ( z );
	}*/
}

bool CMatRenderContextBase::EnableClipping( bool bEnable )
{
	if( bEnable != m_bEnableClipping )
	{
		m_bEnableClipping = bEnable;
		ApplyCustomClipPlanes();		

		return !bEnable;
	}
	return bEnable;
}

void CMatRenderContextBase::Viewport( int x, int y, int width, int height )
{
	// Verify valid top of RT stack
	Assert ( m_RenderTargetStack.Count() > 0 );

	// Reset the top of stack to the new viewport
	RenderTargetStackElement_t newTOS;
	memcpy(&newTOS,&(m_RenderTargetStack.Top()),sizeof(newTOS));
	newTOS.m_nViewX = x;
	newTOS.m_nViewY = y;
	newTOS.m_nViewW = width;
	newTOS.m_nViewH = height;

	m_RenderTargetStack.Pop( );
	m_RenderTargetStack.Push( newTOS );
}


//-----------------------------------------------------------------------------
// This version will push the current rendertarget + current viewport onto the stack
//-----------------------------------------------------------------------------
void CMatRenderContextBase::PushRenderTargetAndViewport( )
{
	// Necessary to push the stack top onto itself; realloc could happen otherwise
	m_RenderTargetStack.EnsureCapacity( m_RenderTargetStack.Count() + 1 );
	m_RenderTargetStack.Push( m_RenderTargetStack.Top() );
	CommitRenderTargetAndViewport();
}


//-----------------------------------------------------------------------------
// Pushes a render target on the render target stack.  Without a specific
// viewport also being pushed, this function uses dummy values which indicate
// that the viewport should span the the full render target and pushes
// the RenderTargetStackElement_t onto the stack
//
// The push and pop methods also implicitly set the render target to the new top of stack
//
// NULL for pTexture indicates rendering to the back buffer
//-----------------------------------------------------------------------------
void CMatRenderContextBase::PushRenderTargetAndViewport( ITexture *pTexture )
{
	// Just blindly push the data on the stack with flags indicating full bounds
#if !defined( _X360 )
	RenderTargetStackElement_t element = { {pTexture, NULL, NULL, NULL}, 0, 0, -1, -1 };
#else
	RenderTargetStackElement_t element = { {pTexture}, 0, 0, -1, -1 };
#endif
	m_RenderTargetStack.Push( element );
	CommitRenderTargetAndViewport();
}


//-----------------------------------------------------------------------------
// Pushes a render target on the render target stack and sets the viewport
//
// NULL for pTexture indicates rendering to the back buffer
//
// The push and pop methods also implicitly set the render target to the new top of stack
//-----------------------------------------------------------------------------
void CMatRenderContextBase::PushRenderTargetAndViewport( ITexture *pTexture, int nViewX, int nViewY, int nViewW, int nViewH )
{
	CMatRenderContextBase::PushRenderTargetAndViewport( pTexture, NULL, nViewX, nViewY, nViewW, nViewH );
}


//-----------------------------------------------------------------------------
// Pushes a render target on the render target stack and sets the viewport
// The push and pop methods also implicitly set the render target to the new top of stack
//-----------------------------------------------------------------------------
void CMatRenderContextBase::PushRenderTargetAndViewport( ITexture *pTexture, ITexture *pDepthTexture, int nViewX, int nViewY, int nViewW, int nViewH )
{
	// Just blindly push the data on the stack
#if !defined( _X360 )
	RenderTargetStackElement_t element = { {pTexture, NULL, NULL, NULL}, pDepthTexture, nViewX, nViewY, nViewW, nViewH };
#else
	RenderTargetStackElement_t element = { {pTexture}, pDepthTexture, nViewX, nViewY, nViewW, nViewH };
#endif
	m_RenderTargetStack.Push( element );
	CommitRenderTargetAndViewport();
}


//-----------------------------------------------------------------------------
// Pops from the render target stack
// Also implicitly sets the render target to the new top of stack
//-----------------------------------------------------------------------------
void CMatRenderContextBase::PopRenderTargetAndViewport( void )
{
	// Check for underflow
	if ( m_RenderTargetStack.Count() == 0 )
	{
		Assert( !"CMatRenderContext::PopRenderTargetAndViewport:  Stack is empty!!!" );
		return;
	}

	// Changelist #266217 added this to main/src/materialsystem.
	Flush();

	// Remove the top of stack
	m_RenderTargetStack.Pop( );
	CommitRenderTargetAndViewport();
}

void CMatRenderContextBase::RecomputeViewProjState()
{
	if ( m_bDirtyViewProjState )
	{
		VMatrix viewMatrix, projMatrix;

		// FIXME: Should consider caching this upon change for projection or view matrix.
		GetMatrix( MATERIAL_VIEW, &viewMatrix );
		GetMatrix( MATERIAL_PROJECTION, &projMatrix );
		m_viewProjMatrix = projMatrix * viewMatrix;
		m_bDirtyViewProjState = false;
	}
}

//-----------------------------------------------------------------------------
// This returns the diameter of the sphere in pixels based on 
// the current model, view, + projection matrices and viewport.
//-----------------------------------------------------------------------------
float CMatRenderContextBase::ComputePixelDiameterOfSphere( const Vector& vecAbsOrigin, float flRadius )
{
	RecomputeViewState();
	RecomputeViewProjState();
	// This is sort of faked, but it's faster that way
	// FIXME: Also, there's a much faster way to do this with similar triangles
	// but I want to make sure it exactly matches the current matrices, so
	// for now, I do it this conservative way
	Vector4D testPoint1, testPoint2;
	VectorMA( vecAbsOrigin, flRadius, m_vecViewUp, testPoint1.AsVector3D() );
	VectorMA( vecAbsOrigin, -flRadius, m_vecViewUp, testPoint2.AsVector3D() );
	testPoint1.w = testPoint2.w = 1.0f;

	Vector4D clipPos1, clipPos2;
	Vector4DMultiply( m_viewProjMatrix, testPoint1, clipPos1 );
	Vector4DMultiply( m_viewProjMatrix, testPoint2, clipPos2 );
	if (clipPos1.w >= 0.001f)
	{
		clipPos1.y /= clipPos1.w;
	}
	else
	{
		clipPos1.y *= 1000;
	}
	if (clipPos2.w >= 0.001f)
	{
		clipPos2.y /= clipPos2.w;
	}
	else
	{
		clipPos2.y *= 1000;
	}
	int vx, vy, vwidth, vheight;
	GetViewport( vx, vy, vwidth, vheight );

	// The divide-by-two here is because y goes from -1 to 1 in projection space
	return vheight * fabs( clipPos2.y - clipPos1.y ) / 2.0f;
}

ConVar mat_accelerate_adjust_exposure_down( "mat_accelerate_adjust_exposure_down", "3.0", FCVAR_CHEAT );
ConVar mat_hdr_manual_tonemap_rate( "mat_hdr_manual_tonemap_rate", "1.0" );
ConVar mat_hdr_tonemapscale( "mat_hdr_tonemapscale", "1.0", FCVAR_CHEAT );
ConVar mat_tonemap_algorithm( "mat_tonemap_algorithm", "1", FCVAR_CHEAT, "0 = Original Algorithm 1 = New Algorithm" );

void CMatRenderContextBase::TurnOnToneMapping(void)
{
	if ( ( HardwareConfig()->GetHDRType() != HDR_TYPE_NONE ) && ( m_FrameTime > 0.0f ) )
	{
		float elapsed_time = m_FrameTime;
		float goalScale = m_GoalToneMapScale;
		float rate = mat_hdr_manual_tonemap_rate.GetFloat();
		
		if ( mat_tonemap_algorithm.GetInt() == 1 )
		{
			rate *= 2.0f; // Default 2x for the new tone mapping algorithm so it feels the same as the original
		}

		if ( rate == 0.0f ) // Zero indicates instantaneous tonemap scaling
		{
			m_CurToneMapScale = goalScale;
		}
		else
		{
			if ( goalScale < m_CurToneMapScale )
			{
				float acc_exposure_adjust = mat_accelerate_adjust_exposure_down.GetFloat();

				// Adjust at up to 4x rate when over-exposed.
				rate = min( ( acc_exposure_adjust * rate ), FLerp( rate, ( acc_exposure_adjust * rate ), 0.0f, 1.5f, ( m_CurToneMapScale - goalScale ) ) );
			}

			float flRateTimesTime = rate * elapsed_time;
			if ( mat_tonemap_algorithm.GetInt() == 1 )
			{
				// For the new tone mapping algorithm, limit the rate based on the number of bins to 
				// help reduce the tone map scalar "riding the wave" of the histogram re-building

				//Warning( "flRateTimesTime = %.4f", flRateTimesTime );
				flRateTimesTime = min( flRateTimesTime, ( 1.0f / 16.0f ) * 0.25f ); // 16 is number of HDR sample bins defined in viewpostprocess.cpp
				//Warning( " --> %.4f\n", flRateTimesTime );
			}

			float alpha = max( 0.0f, min( 1.0f, flRateTimesTime ) );
			m_CurToneMapScale = ( goalScale * alpha ) + ( m_CurToneMapScale * ( 1.0f - alpha ) );

			if ( !IsFinite( m_CurToneMapScale ) )
			{
				Assert( 0 );
				m_CurToneMapScale = goalScale;
			}
		}

		SetToneMappingScaleLinear( Vector( m_CurToneMapScale, m_CurToneMapScale, m_CurToneMapScale ) );
		m_LastSetToneMapScale = Vector( m_CurToneMapScale, m_CurToneMapScale, m_CurToneMapScale );
	}
}

void CMatRenderContextBase::ResetToneMappingScale(float sc)
{
	m_CurToneMapScale = sc;
	SetToneMappingScaleLinear( Vector( m_CurToneMapScale, m_CurToneMapScale, m_CurToneMapScale ) );
	m_LastSetToneMapScale = Vector( m_CurToneMapScale, m_CurToneMapScale, m_CurToneMapScale );
	// mat_hdr_tonemapscale.SetValue(1.0f);
	m_GoalToneMapScale = 1;
}

void CMatRenderContextBase::SetGoalToneMappingScale( float monoscale)
{
	Assert( IsFinite( monoscale ) );
	if( IsFinite( monoscale ) )
	{
		m_GoalToneMapScale = monoscale;
	}
}

Vector CMatRenderContextBase::GetToneMappingScaleLinear( void )
{
	if ( HardwareConfig()->GetHDRType() == HDR_TYPE_NONE )
		return Vector( 1.0f, 1.0f, 1.0f );
	else
		return m_LastSetToneMapScale;
}

void CMatRenderContextBase::OnAsyncCreateTextureFromRenderTarget( ITexture* pSrcRt, const char** ppDstName, IAsyncTextureOperationReceiver* pRecipient )
{
	Assert( pSrcRt != NULL );
	Assert( pRecipient != NULL );
	Assert( ppDstName != NULL && *ppDstName != NULL);
	
	// Bump the ref count on the recipient before handing it off. This ensures the receiver won't go away before we have completed our work. 
	pSrcRt->AddRef();
	pRecipient->AddRef();

	// Also, need to allocate a copy of the string and use that one s.t. the caller doesn't have to worry about it.
	char* pDstNameCopy = new char[ V_strlen( *ppDstName ) + 1 ];
	V_strcpy( pDstNameCopy, *ppDstName );
	( *ppDstName ) = pDstNameCopy;
}

// Map and unmap a texture. The pRecipient->OnAsyncMapComplete is called when complete. 
void CMatRenderContextBase::OnAsyncMap( ITextureInternal* pTexToMap, IAsyncTextureOperationReceiver* pRecipient, void* pExtraArgs )
{
	Assert( pTexToMap != NULL );
	Assert( pRecipient != NULL );

	pTexToMap->AddRef();
	pRecipient->AddRef();
}

void CMatRenderContextBase::OnAsyncUnmap( ITextureInternal* pTexToUnmap ) 
{
	Assert( pTexToUnmap != NULL );

	pTexToUnmap->AddRef();
}

void CMatRenderContextBase::OnAsyncCopyRenderTargetToStagingTexture( ITexture* pDst, ITexture* pSrc, IAsyncTextureOperationReceiver* pRecipient )
{
	Assert( pDst != NULL );
	Assert( pSrc != NULL );
	Assert( pRecipient != NULL );

	pDst->AddRef();
	pSrc->AddRef();
	pRecipient->AddRef();
}

#undef g_pShaderAPI 

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CMatRenderContext::CMatRenderContext()
{
	g_FrameNum = 0;
	m_pBatchIndices = NULL;
	m_pBatchMesh = NULL;
	m_pCurrentIndexBuffer = NULL;
	m_pMorphRenderContext = NULL;
	m_NonInteractiveMode = MATERIAL_NON_INTERACTIVE_MODE_NONE;
}

InitReturnVal_t CMatRenderContext::Init( CMaterialSystem *pMaterialSystem )
{
	InitReturnVal_t nRetVal = BaseClass::Init();
	if ( nRetVal != INIT_OK )
		return nRetVal;

	m_pMaterialSystem = pMaterialSystem;

	m_pBoundMorph = NULL;

	// Create some lovely textures
	m_pLocalCubemapTexture = TextureManager()->ErrorTexture();
	m_pMorphRenderContext = g_pMorphMgr->AllocateRenderContext();

	return INIT_OK;
}

void CMatRenderContext::Shutdown( )
{
	if ( m_pUserDefinedLightmap )
	{
		m_pUserDefinedLightmap->DecrementReferenceCount();
		m_pUserDefinedLightmap = NULL;
	}

	if ( m_pMorphRenderContext )
	{
		g_pMorphMgr->FreeRenderContext( m_pMorphRenderContext );
		m_pMorphRenderContext = NULL;
	}

	BaseClass::Shutdown();
}

void CMatRenderContext::OnReleaseShaderObjects()
{
	// alt-tab unbinds the morph
	m_pBoundMorph = NULL;
}

#ifdef DX_TO_GL_ABSTRACTION
void CMatRenderContext::DoStartupShaderPreloading( void )
{
	g_pShaderDevice->DoStartupShaderPreloading();
}
#endif

void CMatRenderContext::TextureManagerUpdate()
{
	TextureManager()->Update();
}


inline IMaterialInternal *CMatRenderContext::GetMaterialInternal( MaterialHandle_t h ) const
{
	return GetMaterialSystem()->GetMaterialInternal( h );
}

inline IMaterialInternal *CMatRenderContext::GetDrawFlatMaterial()
{
	return GetMaterialSystem()->GetDrawFlatMaterial();
}

inline IMaterialInternal *CMatRenderContext::GetBufferClearObeyStencil( int i )
{
	return GetMaterialSystem()->GetBufferClearObeyStencil(i );
}

inline ShaderAPITextureHandle_t CMatRenderContext::GetFullbrightLightmapTextureHandle() const
{
	return GetMaterialSystem()->GetFullbrightLightmapTextureHandle();
}

inline ShaderAPITextureHandle_t CMatRenderContext::GetFullbrightBumpedLightmapTextureHandle() const
{
	return GetMaterialSystem()->GetFullbrightBumpedLightmapTextureHandle();
}

inline ShaderAPITextureHandle_t CMatRenderContext::GetBlackTextureHandle() const
{
	return GetMaterialSystem()->GetBlackTextureHandle();
}

inline ShaderAPITextureHandle_t CMatRenderContext::GetFlatNormalTextureHandle() const
{
	return GetMaterialSystem()->GetFlatNormalTextureHandle();
}

inline ShaderAPITextureHandle_t CMatRenderContext::GetGreyTextureHandle() const
{
	return GetMaterialSystem()->GetGreyTextureHandle();
}

inline ShaderAPITextureHandle_t CMatRenderContext::GetGreyAlphaZeroTextureHandle() const
{
	return GetMaterialSystem()->GetGreyAlphaZeroTextureHandle();
}

inline ShaderAPITextureHandle_t CMatRenderContext::GetWhiteTextureHandle() const
{
	return GetMaterialSystem()->GetWhiteTextureHandle();
}

inline const CMatLightmaps *CMatRenderContext::GetLightmaps() const
{
	return GetMaterialSystem()->GetLightmaps();
}

inline CMatLightmaps *CMatRenderContext::GetLightmaps()
{
	return GetMaterialSystem()->GetLightmaps();
}

inline ShaderAPITextureHandle_t CMatRenderContext::GetMaxDepthTextureHandle() const
{
	return GetMaterialSystem()->GetMaxDepthTextureHandle();
}

void CMatRenderContext::BeginRender()
{
#if 1 // Rick's optimization: not sure this is needed anymore
	if ( GetMaterialSystem()->GetThreadMode() != MATERIAL_SINGLE_THREADED )
	{
		VPROF_INCREMENT_GROUP_COUNTER( "render/CMatBeginRender", COUNTER_GROUP_TELEMETRY, 1 );

		TelemetrySetLockName( TELEMETRY_LEVEL1, (char const *)&g_MatSysMutex, "MatSysMutex" );

		tmTryLock( TELEMETRY_LEVEL1, (char const *)&g_MatSysMutex, "BeginRender" );
		g_MatSysMutex.Lock();
		tmEndTryLock( TELEMETRY_LEVEL1, (char const *)&g_MatSysMutex, TMLR_SUCCESS );
		tmSetLockState( TELEMETRY_LEVEL1, (char const *)&g_MatSysMutex, TMLS_LOCKED, "BeginRender" );
	}
#endif
}

void CMatRenderContext::EndRender()
{
#if 1 // Rick's optimization: not sure this is needed anymore
	if ( GetMaterialSystem()->GetThreadMode() != MATERIAL_SINGLE_THREADED )
	{
		g_MatSysMutex.Unlock();
		tmSetLockState( TELEMETRY_LEVEL1, (char const *)&g_MatSysMutex, TMLS_RELEASED, "EndRender" );
	}
#endif
}

void CMatRenderContext::Flush( bool flushHardware )
{
	VPROF( "CMatRenderContextBase::Flush" );

	g_pShaderAPI->FlushBufferedPrimitives();
	if ( IsPC() && flushHardware )
	{
		g_pShaderAPI->FlushBufferedPrimitives();
	}
}

bool CMatRenderContext::TestMatrixSync( MaterialMatrixMode_t mode )
{
	if ( !ShouldValidateMatrices() )
	{
		return true;
	}

	VMatrix transposeMatrix, matrix;
	g_pShaderAPI->GetMatrix( mode, (float*)transposeMatrix.m );
	MatrixTranspose( transposeMatrix, matrix );

	ValidateMatrices( matrix, m_MatrixStacks[mode].Top().matrix );

	return true;
}

void CMatRenderContext::MatrixMode( MaterialMatrixMode_t mode )
{
	CMatRenderContextBase::MatrixMode( mode );
	g_pShaderAPI->MatrixMode( mode );
	if ( ShouldValidateMatrices() )
	{
		TestMatrixSync( mode );
	}

}
void CMatRenderContext::PushMatrix()
{
	if ( ShouldValidateMatrices() )
	{
		TestMatrixSync( m_MatrixMode );
	}

	CMatRenderContextBase::PushMatrix();
	g_pShaderAPI->PushMatrix();
	
	if ( ShouldValidateMatrices() )
	{
		TestMatrixSync( m_MatrixMode );
	}
}

void CMatRenderContext::PopMatrix()
{
	if ( ShouldValidateMatrices() )
	{
		TestMatrixSync( m_MatrixMode );
	}

	CMatRenderContextBase::PopMatrix();
	g_pShaderAPI->PopMatrix();

	if ( ShouldValidateMatrices() )
	{
		TestMatrixSync( m_MatrixMode );
	}
}

void CMatRenderContext::LoadMatrix( const VMatrix& matrix )
{
	CMatRenderContextBase::LoadMatrix( matrix );
	ForceSync();
	if ( ShouldValidateMatrices() )
	{
		VMatrix transposeMatrix;
		MatrixTranspose( matrix, transposeMatrix );
		g_pShaderAPI->LoadMatrix( transposeMatrix.Base() );
		TestMatrixSync( m_MatrixMode );
	}
}

void CMatRenderContext::LoadMatrix( const matrix3x4_t& matrix )
{
	CMatRenderContextBase::LoadMatrix( matrix );
	ForceSync();
	if ( ShouldValidateMatrices() )
	{
		VMatrix transposeMatrix;
		MatrixTranspose( VMatrix(matrix), transposeMatrix );
		g_pShaderAPI->LoadMatrix( transposeMatrix.Base() );
		TestMatrixSync( m_MatrixMode );
	}
}

void CMatRenderContext::MultMatrix( const VMatrix& matrix )
{
	CMatRenderContextBase::MultMatrix( matrix );
	ForceSync();
	if ( ShouldValidateMatrices() )
	{
		VMatrix transposeMatrix;
		MatrixTranspose( matrix, transposeMatrix );
		g_pShaderAPI->MultMatrix( transposeMatrix.Base() );
		TestMatrixSync( m_MatrixMode );
	}
}

void CMatRenderContext::MultMatrix( const matrix3x4_t& matrix )
{
	CMatRenderContextBase::MultMatrix( VMatrix( matrix ) );
	ForceSync();
	if ( ShouldValidateMatrices() )
	{
		VMatrix transposeMatrix;
		MatrixTranspose( VMatrix(matrix), transposeMatrix );
		g_pShaderAPI->MultMatrix( transposeMatrix.Base() );
		TestMatrixSync( m_MatrixMode );
	}
}

void CMatRenderContext::MultMatrixLocal( const VMatrix& matrix )
{
	CMatRenderContextBase::MultMatrixLocal( matrix );
	ForceSync();
	if ( ShouldValidateMatrices() )
	{
		VMatrix transposeMatrix;
		MatrixTranspose( matrix, transposeMatrix );
		g_pShaderAPI->MultMatrixLocal( transposeMatrix.Base() );
		TestMatrixSync( m_MatrixMode );
	}
}

void CMatRenderContext::MultMatrixLocal( const matrix3x4_t& matrix )
{
	CMatRenderContextBase::MultMatrixLocal( VMatrix( matrix ) );
	ForceSync();
	if ( ShouldValidateMatrices() )
	{
		VMatrix transposeMatrix;
		MatrixTranspose( VMatrix(matrix), transposeMatrix );
		g_pShaderAPI->MultMatrixLocal( transposeMatrix.Base() );
		TestMatrixSync( m_MatrixMode );
	}
}

void CMatRenderContext::LoadIdentity()
{
	CMatRenderContextBase::LoadIdentity();
	ForceSync();
	if ( ShouldValidateMatrices() )
	{
		g_pShaderAPI->LoadIdentity();
		TestMatrixSync( m_MatrixMode );
	}
}

void CMatRenderContext::Ortho( double left, double top, double right, double bottom, double zNear, double zFar )
{
	CMatRenderContextBase::Ortho( left, top, right, bottom, zNear, zFar );
	ForceSync();
	if ( ShouldValidateMatrices() )
	{
		g_pShaderAPI->Ortho( left, top, right, bottom, zNear, zFar );
		TestMatrixSync( m_MatrixMode );
	}
}

void CMatRenderContext::PerspectiveX( double flFovX, double flAspect, double flZNear, double flZFar )
{
	CMatRenderContextBase::PerspectiveX( flFovX, flAspect, flZNear, flZFar );
	ForceSync();
	if ( ShouldValidateMatrices() )
	{
		g_pShaderAPI->PerspectiveX( flFovX, flAspect, flZNear, flZFar );
		TestMatrixSync( m_MatrixMode );
	}
}

void CMatRenderContext::PerspectiveOffCenterX( double flFovX, double flAspect, double flZNear, double flZFar, double bottom, double top, double left, double right )
{
	CMatRenderContextBase::PerspectiveOffCenterX( flFovX, flAspect, flZNear, flZFar, bottom, top, left, right );
	ForceSync();
	if ( ShouldValidateMatrices() )
	{
		g_pShaderAPI->PerspectiveOffCenterX( flFovX, flAspect, flZNear, flZFar, bottom, top, left, right );
		TestMatrixSync( m_MatrixMode );
	}
}

void CMatRenderContext::PickMatrix( int x, int y, int nWidth, int nHeight )
{
	CMatRenderContextBase::PickMatrix( x, y, nWidth, nHeight );
	ForceSync();
	if ( ShouldValidateMatrices() )
	{
		g_pShaderAPI->PickMatrix( x, y, nWidth, nHeight );
		TestMatrixSync( m_MatrixMode );
	}
}

void CMatRenderContext::Rotate( float flAngle, float x, float y, float z )
{
	CMatRenderContextBase::Rotate( flAngle, x, y, z );
	ForceSync();
	if ( ShouldValidateMatrices() )
	{
		g_pShaderAPI->Rotate( flAngle, x, y, z );
		TestMatrixSync( m_MatrixMode );
	}
}

void CMatRenderContext::Translate( float x, float y, float z )
{
	CMatRenderContextBase::Translate( x, y, z );
	ForceSync();
	if ( ShouldValidateMatrices() )
	{
		g_pShaderAPI->Translate( x, y, z );
		TestMatrixSync( m_MatrixMode );
	}
}

void CMatRenderContext::Scale( float x, float y, float z )
{
	CMatRenderContextBase::Scale( x, y, z );
	ForceSync();
	if ( ShouldValidateMatrices() )
	{
		g_pShaderAPI->Scale( x, y, z );
		TestMatrixSync( m_MatrixMode );
	}
}

void CMatRenderContext::GetMatrix( MaterialMatrixMode_t matrixMode, VMatrix *pMatrix )
{
	CMatRenderContextBase::GetMatrix( matrixMode, pMatrix );

	ForceSync();
	if ( ShouldValidateMatrices() )
	{
		VMatrix testMatrix;
		VMatrix transposeMatrix;
		g_pShaderAPI->GetMatrix( matrixMode, (float*)transposeMatrix.m );
		MatrixTranspose( transposeMatrix, testMatrix );

		ValidateMatrices( testMatrix, *pMatrix );
	}
}

void CMatRenderContext::GetMatrix( MaterialMatrixMode_t matrixMode, matrix3x4_t *pMatrix )
{
	if ( !ShouldValidateMatrices() )
	{
		CMatRenderContextBase::GetMatrix( matrixMode, pMatrix );
	}
	else
	{
		VMatrix matrix;
		CMatRenderContext::GetMatrix( matrixMode, &matrix );
		*pMatrix = matrix.As3x4();
	}
}

void CMatRenderContext::SyncMatrices()
{
	if ( !ShouldValidateMatrices() && AllowLazyMatrixSync() )
	{
		for( int i = 0; i < NUM_MATRIX_MODES; i++ )
		{
			MatrixStackItem_t &top = m_MatrixStacks[i].Top();
			if ( top.flags & MSF_DIRTY )
			{
				g_pShaderAPI->MatrixMode( (MaterialMatrixMode_t)i );
				if ( !( top.flags & MSF_IDENTITY ) )
				{
					VMatrix transposeTop;
					MatrixTranspose( top.matrix, transposeTop );
					g_pShaderAPI->LoadMatrix( transposeTop.Base() );
				}
				else
				{
					g_pShaderAPI->LoadIdentity();
				}

				top.flags &= ~MSF_DIRTY;
			}
		}
	}
}

void CMatRenderContext::ForceSyncMatrix( MaterialMatrixMode_t mode )
{
	MatrixStackItem_t &top = m_MatrixStacks[mode].Top();
	if ( top.flags & MSF_DIRTY )
	{
		bool bSetMode = ( m_MatrixMode != mode );
		if ( bSetMode )
		{
			g_pShaderAPI->MatrixMode( (MaterialMatrixMode_t)mode );
		}

		if ( !( top.flags & MSF_IDENTITY ) )
		{
			VMatrix transposeTop;
			MatrixTranspose( top.matrix, transposeTop );
			g_pShaderAPI->LoadMatrix( transposeTop.Base() );
		}
		else
		{
			g_pShaderAPI->LoadIdentity();
		}

		if ( bSetMode )
		{
			g_pShaderAPI->MatrixMode( (MaterialMatrixMode_t)mode );
		}

		top.flags &= ~MSF_DIRTY;
	}
}

void CMatRenderContext::SyncMatrix( MaterialMatrixMode_t mode )
{
	if ( !ShouldValidateMatrices() && AllowLazyMatrixSync() )
	{
		ForceSyncMatrix( mode );
	}
}


//-----------------------------------------------------------------------------
// Swap buffers
//-----------------------------------------------------------------------------
void CMatRenderContext::SwapBuffers()
{
	g_pMorphMgr->AdvanceFrame();
	g_pOcclusionQueryMgr->AdvanceFrame();
	g_pShaderDevice->Present();

#ifdef _X360
	if ( s_bDirtyDisk )
	{
		SpinPresent();
	}
#endif
}


//-----------------------------------------------------------------------------
// Clears the render data after we're done with it
//-----------------------------------------------------------------------------
void CMatRenderContext::OnRenderDataUnreferenced()
{
	MarkRenderDataUnused( false );
}


//-----------------------------------------------------------------------------
// Custom clip planes
//-----------------------------------------------------------------------------
void CMatRenderContext::PushCustomClipPlane( const float *pPlane )
{
	PlaneStackElement psePlane;
	memcpy( psePlane.fValues, pPlane, sizeof( float ) * 4 );
	psePlane.bHack_IsHeightClipPlane = false;
	m_CustomClipPlanes.AddToTail( psePlane ); //we're doing this as a UtlVector so height clip planes never change their cached index
	ApplyCustomClipPlanes();
}

void CMatRenderContext::PopCustomClipPlane( void )
{
	Assert( m_CustomClipPlanes.Count() );
	
	//remove the endmost non-height plane found
	int i;
	for( i = m_CustomClipPlanes.Count(); --i >= 0; )
	{
		if( m_CustomClipPlanes[i].bHack_IsHeightClipPlane == false )
		{
			m_CustomClipPlanes.Remove(i);
			break;
		}
	}
	Assert( i != -1 ); //only the height clip plane was found, which means this pop had no associated push
	ApplyCustomClipPlanes();
}

void CMatRenderContext::ApplyCustomClipPlanes( void )
{	
	int iMaxClipPlanes = HardwareConfig()->MaxUserClipPlanes();
	int iCustomPlanes;

	if( m_bEnableClipping )
		iCustomPlanes = m_CustomClipPlanes.Count();
	else
		iCustomPlanes = 0;

	float fFakePlane[4];
	unsigned int iFakePlaneVal = 0xFFFFFFFF;
	fFakePlane[0] = fFakePlane[1] = fFakePlane[2] = fFakePlane[3] = *((float *)&iFakePlaneVal);

	SyncMatrices();

	if( iMaxClipPlanes >= 1 && !HardwareConfig()->UseFastClipping() )
	{
		//yay, we get to be the cool clipping club
		if( iMaxClipPlanes >= iCustomPlanes )
		{
			int i;
			for( i = 0; i < iCustomPlanes; ++i )
			{
				g_pShaderAPI->SetClipPlane( i, m_CustomClipPlanes[i].fValues );
				g_pShaderAPI->EnableClipPlane( i, true );
			}
			for( ; i < iMaxClipPlanes; ++i ) //disable unused planes
			{
				g_pShaderAPI->EnableClipPlane( i, false );
				g_pShaderAPI->SetClipPlane( i, fFakePlane );
			}
		}
		else
		{
			int iCustomPlaneOffset = iCustomPlanes - iMaxClipPlanes;

			//can't enable them all
			for( int i = iCustomPlaneOffset; i < iCustomPlanes; ++i )
			{
				g_pShaderAPI->SetClipPlane( i % iMaxClipPlanes, m_CustomClipPlanes[i].fValues );
				g_pShaderAPI->EnableClipPlane( i % iMaxClipPlanes, true );
			}
		}
	}
	else
	{
		//hmm, at most we can make 1 clip plane work
		if( iCustomPlanes == 0 )
		{
			//no planes at all
			g_pShaderAPI->EnableFastClip( false );
			g_pShaderAPI->SetFastClipPlane( fFakePlane );
		}
		else
		{
			//we have to wire the topmost plane into the fast clipping scheme
			g_pShaderAPI->EnableFastClip( true );
			g_pShaderAPI->SetFastClipPlane( m_CustomClipPlanes[iCustomPlanes - 1].fValues );
		}
	}	
}


//-----------------------------------------------------------------------------
// Creates/destroys morph data associated w/ a particular material
//-----------------------------------------------------------------------------
IMorph *CMatRenderContext::CreateMorph( MorphFormat_t format, const char *pDebugName )
{
	Assert( format != 0 );
	IMorphInternal *pMorph = g_pMorphMgr->CreateMorph( );
	pMorph->Init( format, pDebugName );
	return pMorph;
}

void CMatRenderContext::DestroyMorph( IMorph *pMorph )
{
	g_pMorphMgr->DestroyMorph( static_cast<IMorphInternal*>(pMorph) );
}

void CMatRenderContext::BindMorph( IMorph *pMorph )
{
	IMorphInternal *pMorphInternal = static_cast<IMorphInternal*>(pMorph);

	if ( m_pBoundMorph != pMorphInternal )
	{
		g_pShaderAPI->FlushBufferedPrimitives();

		m_pBoundMorph = pMorphInternal;

		bool bEnableHWMorph = false;
		if ( pMorphInternal == MATERIAL_MORPH_DECAL )
		{
			bEnableHWMorph = true;
		}
		else if ( pMorphInternal )
		{
			bEnableHWMorph = true;
			pMorphInternal->Bind( m_pMorphRenderContext );
		}
		g_pShaderAPI->EnableHWMorphing( bEnableHWMorph );
	}
}

IMesh* CMatRenderContext::GetDynamicMesh( bool buffered, IMesh* pVertexOverride, IMesh* pIndexOverride, IMaterial *pAutoBind )
{
	VPROF_ASSERT_ACCOUNTED( "CMatRenderContext::GetDynamicMesh" );
	if( pAutoBind )
	{
		Bind( pAutoBind, NULL );
	}

	if ( pVertexOverride )
	{
		if ( CompressionType( pVertexOverride->GetVertexFormat() ) != VERTEX_COMPRESSION_NONE )
		{
			// UNDONE: support compressed dynamic meshes if needed (pro: less VB memory, con: time spent compressing)
			DebuggerBreak();
			return NULL;
		}
	}

	// For anything more than 1 bone, imply the last weight from the 1 - the sum of the others.
	int nCurrentBoneCount = GetCurrentNumBones();
	Assert( nCurrentBoneCount <= 4 );
	if ( nCurrentBoneCount > 1 )
	{
		--nCurrentBoneCount;
	}

	return g_pShaderAPI->GetDynamicMesh( GetCurrentMaterialInternal(), nCurrentBoneCount, 
		buffered, pVertexOverride, pIndexOverride);
}

IMesh* CMatRenderContext::GetDynamicMeshEx( VertexFormat_t vertexFormat, bool bBuffered, IMesh* pVertexOverride, IMesh* pIndexOverride, IMaterial *pAutoBind )
{
	VPROF_ASSERT_ACCOUNTED( "CMatRenderContext::GetDynamicMesh" );
	if( pAutoBind )
	{
		Bind( pAutoBind, NULL );
	}

	if ( pVertexOverride )
	{
		if ( CompressionType( pVertexOverride->GetVertexFormat() ) != VERTEX_COMPRESSION_NONE )
		{
			// UNDONE: support compressed dynamic meshes if needed (pro: less VB memory, con: time spent compressing)
			DebuggerBreak();
			return NULL;
		}
	}

	// For anything more than 1 bone, imply the last weight from the 1 - the sum of the others.
	// FIXME: this seems wrong - in common_vs_fxc.h, we only infer the last weight if we have 3 (not 2)
	int nCurrentBoneCount = GetCurrentNumBones();
	Assert( nCurrentBoneCount <= 4 );
	if ( nCurrentBoneCount > 1 )
	{
		--nCurrentBoneCount;
	}

	return g_pShaderAPI->GetDynamicMeshEx( GetCurrentMaterialInternal(), vertexFormat, nCurrentBoneCount, 
		bBuffered, pVertexOverride, pIndexOverride );
}


//-----------------------------------------------------------------------------
// Deals with depth range
//-----------------------------------------------------------------------------
void CMatRenderContext::DepthRange( float zNear, float zFar )
{
	m_Viewport.m_flMinZ = zNear;
	m_Viewport.m_flMaxZ = zFar;
	g_pShaderAPI->SetViewports( 1, &m_Viewport );
}


//-----------------------------------------------------------------------------
// Private utility function to actually commit the top of the RT/Viewport stack
// to the device.  Only called by the push and pop routines above.
//-----------------------------------------------------------------------------
void CMatRenderContext::CommitRenderTargetAndViewport( void )
{
	Assert( m_RenderTargetStack.Count() > 0 );

	const RenderTargetStackElement_t &element = m_RenderTargetStack.Top( );

	for( int rt=0; rt<NELEMS(element.m_pRenderTargets); rt++ )
	{
		// If we're dealing with the back buffer
		if ( element.m_pRenderTargets[rt] == NULL )
		{
			g_pShaderAPI->SetRenderTargetEx(rt); // No texture parameter here indicates back buffer
			
			if ( IsPC() )
			{
				Assert( ImageLoader::SizeInBytes( g_pShaderDevice->GetBackBufferFormat() ) <= 4 );
				g_pShaderAPI->EnableLinearColorSpaceFrameBuffer( false );
			}
			
			if (rt == 0)										// the first rt sets the viewport
			{
				// If either dimension is negative, set to full bounds of back buffer
				if ( (element.m_nViewW < 0) || (element.m_nViewH < 0) )
				{
					m_Viewport.m_nTopLeftX = 0;
					m_Viewport.m_nTopLeftY = 0;
					g_pShaderAPI->GetBackBufferDimensions( m_Viewport.m_nWidth, m_Viewport.m_nHeight );
					g_pShaderAPI->SetViewports( 1, &m_Viewport );
				}
				else // use the bounds in the element
				{
					m_Viewport.m_nTopLeftX = element.m_nViewX;
					m_Viewport.m_nTopLeftY = element.m_nViewY;
					m_Viewport.m_nWidth = element.m_nViewW;
					m_Viewport.m_nHeight = element.m_nViewH;
					g_pShaderAPI->SetViewports( 1, &m_Viewport );
				}
			}
		}
		else // We're dealing with a texture
		{
			ITextureInternal *pTexInt = static_cast<ITextureInternal*>(element.m_pRenderTargets[rt]);
			pTexInt->SetRenderTarget( rt, element.m_pDepthTexture );

			if (rt == 0)
			{
				if ( IsPC() )
				{
					if( element.m_pRenderTargets[rt]->GetImageFormat() == IMAGE_FORMAT_RGBA16161616F )
					{
						g_pShaderAPI->EnableLinearColorSpaceFrameBuffer( true );
					}
					else
					{
						g_pShaderAPI->EnableLinearColorSpaceFrameBuffer( false );
					}
				}

				// If either dimension is negative, set to full bounds of target
				if ( (element.m_nViewW < 0) || (element.m_nViewH < 0) )
				{
					m_Viewport.m_nTopLeftX = 0;
					m_Viewport.m_nTopLeftY = 0;
					m_Viewport.m_nWidth = element.m_pRenderTargets[rt]->GetActualWidth();
					m_Viewport.m_nHeight = element.m_pRenderTargets[rt]->GetActualHeight();
					g_pShaderAPI->SetViewports( 1, &m_Viewport );
				}
				else // use the bounds passed in
				{
					m_Viewport.m_nTopLeftX = element.m_nViewX;
					m_Viewport.m_nTopLeftY = element.m_nViewY;
					m_Viewport.m_nWidth = element.m_nViewW;
					m_Viewport.m_nHeight = element.m_nViewH;
					g_pShaderAPI->SetViewports( 1, &m_Viewport );
				}
			}
		}
	}
}


void CMatRenderContext::SetFrameBufferCopyTexture( ITexture *pTexture, int textureIndex )
{
	if( textureIndex < 0 || textureIndex > MAX_FB_TEXTURES )
	{
		Assert( 0 );
		return;
	}
	if( m_pCurrentFrameBufferCopyTexture[textureIndex] != pTexture )
	{
		g_pShaderAPI->FlushBufferedPrimitives();
	}
	// FIXME: Do I need to increment/decrement ref counts here, or assume that the app is going to do it?
	m_pCurrentFrameBufferCopyTexture[textureIndex] = pTexture;
}

void CMatRenderContext::BindLocalCubemap( ITexture *pTexture )
{
	ITexture *pPreviousTexture = m_pLocalCubemapTexture;

	CMatRenderContextBase::BindLocalCubemap( pTexture );

	if( m_pLocalCubemapTexture != pPreviousTexture )
	{
		g_pShaderAPI->FlushBufferedPrimitives();
	}
}

void CMatRenderContext::SetNonInteractivePacifierTexture( ITexture *pTexture, float flNormalizedX, float flNormalizedY, float flNormalizedSize )
{
	m_pNonInteractivePacifier.Init( pTexture );
	m_flNormalizedX = flNormalizedX;
	m_flNormalizedY = flNormalizedY;
	m_flNormalizedSize = flNormalizedSize;
}

void CMatRenderContext::SetNonInteractiveTempFullscreenBuffer( ITexture *pTexture, MaterialNonInteractiveMode_t mode )
{
	if ( mode != MATERIAL_NON_INTERACTIVE_MODE_NONE )
	{
		m_pNonInteractiveTempFullscreenBuffer[mode].Init( pTexture );
	}
}

void CMatRenderContext::RefreshFrontBufferNonInteractive()
{
	g_pShaderDevice->RefreshFrontBufferNonInteractive();
#ifdef _X360
	if ( s_bDirtyDisk )
	{
		if ( m_NonInteractiveMode == MATERIAL_NON_INTERACTIVE_MODE_NONE )
		{
			SpinPresent();
		}
		else
		{
			while ( true )
			{
				g_pShaderDevice->RefreshFrontBufferNonInteractive();
			}
		}
	}
#endif
}

void CMatRenderContext::EnableNonInteractiveMode( MaterialNonInteractiveMode_t mode )
{
	m_NonInteractiveMode = mode;
	if ( mode == MATERIAL_NON_INTERACTIVE_MODE_NONE )
	{
		g_pShaderDevice->EnableNonInteractiveMode( mode );
	}
	else
	{
		ShaderNonInteractiveInfo_t info;
		info.m_flNormalizedX = m_flNormalizedX;
		info.m_flNormalizedY = m_flNormalizedY;
		info.m_flNormalizedSize = m_flNormalizedSize;

		ITextureInternal *pTexInternal = static_cast<ITextureInternal*>( (ITexture*)m_pNonInteractiveTempFullscreenBuffer[mode] );
		info.m_hTempFullscreenTexture = pTexInternal ? 
			pTexInternal->GetTextureHandle(0) : INVALID_SHADERAPI_TEXTURE_HANDLE;
		ITextureInternal *pTexPacifierInternal = static_cast<ITextureInternal*>( (ITexture*)m_pNonInteractivePacifier );
		info.m_nPacifierCount = pTexPacifierInternal ? pTexPacifierInternal->GetNumAnimationFrames() : 0;
		for ( int i = 0; i < info.m_nPacifierCount; ++i )
		{
			info.m_pPacifierTextures[i] = pTexPacifierInternal->GetTextureHandle( i ); 
		}
		g_pShaderDevice->EnableNonInteractiveMode( mode, &info );
	}
}

void CMatRenderContext::SetRenderTargetEx( int nRenderTargetID, ITexture *pNewTarget ) 
{
	// Verify valid top of RT stack
	Assert ( m_RenderTargetStack.Count() > 0 );
	
	// Grab the old target
	ITexture *pOldTarget = m_RenderTargetStack.Top().m_pRenderTargets[nRenderTargetID];

	CMatRenderContextBase::SetRenderTargetEx( nRenderTargetID, pNewTarget );

	// If we're actually changing render targets
	if( pNewTarget != pOldTarget )
	{
		// If we're going to render to the back buffer
		if ( pNewTarget == NULL )
		{
			if ( nRenderTargetID == 0)						// reset viewport on set of rt 0
			{
				m_Viewport.m_nTopLeftX = 0;
				m_Viewport.m_nTopLeftY = 0;
				g_pShaderAPI->GetBackBufferDimensions( m_Viewport.m_nWidth, m_Viewport.m_nHeight ); 
				g_pShaderAPI->SetViewports( 1, &m_Viewport ); 
			}
			g_pShaderAPI->SetRenderTargetEx( nRenderTargetID ); // No parameter here indicates back buffer
		}
		else
		{
			// If we're going to render to a texture
			// Make sure the texture is a render target...
			bool reset = true;
			if (nRenderTargetID==0)
			{
				// reset vp on change of rt#0
				m_Viewport.m_nTopLeftX = 0;
				m_Viewport.m_nTopLeftY = 0;
				m_Viewport.m_nWidth = pNewTarget->GetActualWidth();
				m_Viewport.m_nHeight = pNewTarget->GetActualHeight();
				g_pShaderAPI->SetViewports( 1, &m_Viewport ); 
			}
			ITextureInternal *pTexInt = static_cast<ITextureInternal*>(pNewTarget);
			if ( pTexInt )
			{
				reset = !pTexInt->SetRenderTarget( nRenderTargetID );
				if ( reset )
				{
					g_pShaderAPI->SetRenderTargetEx( nRenderTargetID );
				}
			}

			if( pNewTarget && pNewTarget->GetImageFormat() == IMAGE_FORMAT_RGBA16161616F )
			{
				g_pShaderAPI->EnableLinearColorSpaceFrameBuffer( true );
			}
			else
			{
				g_pShaderAPI->EnableLinearColorSpaceFrameBuffer( false );
			}
		}
	}
	CommitRenderTargetAndViewport();
}


void CMatRenderContext::GetRenderTargetDimensions( int &width, int &height ) const
{
	// Target at top of stack
	ITexture *pTOS = m_RenderTargetStack.Top().m_pRenderTargets[0];

	// If top of stack isn't the back buffer, get dimensions from the texture
	if ( pTOS != NULL )
	{
		width = pTOS->GetActualWidth();
		height = pTOS->GetActualHeight();
	}
	else // otherwise, get them from the shader API
	{
		g_pShaderAPI->GetBackBufferDimensions( width, height );
	}
}


//-----------------------------------------------------------------------------
// What are the lightmap dimensions?
//-----------------------------------------------------------------------------
void CMatRenderContext::GetLightmapDimensions( int *w, int *h )
{
	*w = GetMaterialSystem()->GetLightmapWidth( GetLightmapPage() );
	*h = GetMaterialSystem()->GetLightmapHeight( GetLightmapPage() );
}

void CMatRenderContext::DrawScreenSpaceQuad( IMaterial* pMaterial )
{
	// Despite saying we render a full screen quad, this actually renders a single triangle
	// that covers the whole screen.
	int w, h;

	GetRenderTargetDimensions( w, h );
	if ( ( w == 0 ) || ( h == 0 ) )
		return;

	// DX9 disagrees about (0, 0) in a render target and (0, 0) in the texture. 
	// Fix that here by doing a half-pixel offset for the pixel. 
	// Because we are working in clip space which is 2 units across, the adjustment factor is 1.
	float flOffsetW = 1.0f / w;
	float flOffsetH = 1.0f / h;

	Bind( pMaterial );
	IMesh* pMesh = GetDynamicMesh( true );

	CMeshBuilder meshBuilder;;
	meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, 1 );

	enum { TL, BL, TR, COORDS_COUNT };

	struct CoordSSQ_t
	{
		float x, y;
		float u, v;
	};

	CoordSSQ_t coords[] = { 
		{ -1.0f - 1.0f * flOffsetW,  1.0f + 1.0f * flOffsetH,  0.0f,  0.0f }, // TL
		{ -1.0f - 1.0f * flOffsetW, -3.0f + 1.0f * flOffsetH,  0.0f,  2.0f }, // BL
		{  3.0f - 1.0f * flOffsetW,  1.0f + 1.0f * flOffsetH,  2.0f,  0.0f }, // TR
	};

	static_assert( ARRAYSIZE( coords ) == COORDS_COUNT, "Unexpected number of coords in triangle, should match enum." );

	MatrixMode( MATERIAL_VIEW );
	PushMatrix();
	LoadIdentity();

	MatrixMode( MATERIAL_PROJECTION );
	PushMatrix();
	LoadIdentity();

	for ( int i = 0; i < COORDS_COUNT; ++i )
	{
		meshBuilder.Position3f(    coords[ i ].x, coords[ i ].y,          0.0f );
		meshBuilder.Normal3f(               0.0f,          0.0f,          1.0f );
		meshBuilder.TangentS3f(             0.0f,          1.0f,          0.0f );
		meshBuilder.TangentT3f(             1.0f,          0.0f,          0.0f );
		meshBuilder.TexCoord2f( 0, coords[ i ].u, coords[ i ].v                );

		meshBuilder.AdvanceVertex();
	}
									   
	meshBuilder.End();
	pMesh->Draw();

	MatrixMode( MATERIAL_VIEW );
	PopMatrix();

	MatrixMode( MATERIAL_PROJECTION );
	PopMatrix();
}

void CMatRenderContext::DrawScreenSpaceRectangle( 
	IMaterial *pMaterial,
	int destx, int desty,
	int width, int height,
	float src_texture_x0, float src_texture_y0,			// which texel you want to appear at
	// destx/y
	float src_texture_x1, float src_texture_y1,			// which texel you want to appear at
	// destx+width-1, desty+height-1
	int src_texture_width, int src_texture_height,		// needed for fixup
	void *pClientRenderable,
	int nXDice, int nYDice )							// Amount to tessellate the quad
{
	pMaterial = ((IMaterialInternal *)pMaterial)->GetRealTimeVersion();

	::DrawScreenSpaceRectangle( pMaterial, destx, desty, width, height, 
		src_texture_x0, src_texture_y0,	src_texture_x1, src_texture_y1,
		src_texture_width, src_texture_height, pClientRenderable, nXDice, nYDice );
	return;
}

static int CompareVertexFormats( VertexFormat_t Fmt1, VertexFormat_t Fmt2 )
{
	if ( Fmt1 != Fmt2 )
	{
		if ( Fmt1 > Fmt2 )
			return 1;
		else
			return -1;
	}
	else
		return 0;
}

int CMatRenderContext::CompareMaterialCombos( IMaterial *pMaterial1, IMaterial *pMaterial2, int lightMapID1, int lightMapID2 )
{
	pMaterial1 = ((IMaterialInternal *)pMaterial1)->GetRealTimeVersion(); //always work with the real time version of materials internally.
	pMaterial2 = ((IMaterialInternal *)pMaterial2)->GetRealTimeVersion(); //always work with the real time version of materials internally.

	IMaterialInternal *pMat1 = (IMaterialInternal *)pMaterial1;
	IMaterialInternal *pMat2 = (IMaterialInternal *)pMaterial2;
	ShaderRenderState_t *pState1 = pMat1->GetRenderState();
	ShaderRenderState_t *pState2 = pMat2->GetRenderState();
	int dPass = pState2->m_pSnapshots->m_nPassCount - pState1->m_pSnapshots->m_nPassCount;
	if ( dPass )
		return dPass;

	if ( pState1->m_pSnapshots->m_nPassCount > 1 )
	{
		int dFormat = CompareVertexFormats( pMat1->GetVertexFormat(), pMat2->GetVertexFormat() );
		if ( dFormat )
			return dFormat;
	}

	for ( int i = 0; i < pState1->m_pSnapshots->m_nPassCount; i++ )
	{
		// UNDONE: Compare snapshots in the shaderapi?
		int dSnapshot = pState1->m_pSnapshots->m_Snapshot[i] - pState2->m_pSnapshots->m_Snapshot[i];
		if ( dSnapshot )
		{
			dSnapshot = g_pShaderAPI->CompareSnapshots( pState1->m_pSnapshots->m_Snapshot[i], pState2->m_pSnapshots->m_Snapshot[i] );
			if ( dSnapshot )
				return dSnapshot;
		}
	}

	int dFormat = CompareVertexFormats( pMat1->GetVertexFormat(), pMat2->GetVertexFormat() );
	if ( dFormat )
		return dFormat;

	IMaterialVar **pParams1 = pMat1->GetShaderParams();
	IMaterialVar **pParams2 = pMat2->GetShaderParams();
	int nParams1 = pMat1->ShaderParamCount();
	int nParams2 = pMat2->ShaderParamCount();
	int nBaseTexParamType1 = pParams1 && nParams1 > BASETEXTURE ? pParams1[BASETEXTURE]->GetType() : -1;
	int nBaseTexParamType2 = pParams2 && nParams2 > BASETEXTURE ? pParams2[BASETEXTURE]->GetType() : -1;
	if( nBaseTexParamType1 == MATERIAL_VAR_TYPE_TEXTURE || nBaseTexParamType2 == MATERIAL_VAR_TYPE_TEXTURE )
	{
		if( nBaseTexParamType1 != nBaseTexParamType2 )
		{
			return nBaseTexParamType2 - nBaseTexParamType1;
		}
		int dBaseTexture = Q_stricmp( pParams1[BASETEXTURE]->GetTextureValue()->GetName(), pParams2[BASETEXTURE]->GetTextureValue()->GetName() );
		if ( dBaseTexture )
			return dBaseTexture;
	}

	int dLightmap = lightMapID1 - lightMapID2;
	if ( dLightmap )
		return dLightmap;

	return (int)pMat1 - (int)pMat2;
}


void CMatRenderContext::Bind( IMaterial *iMaterial, void *proxyData )
{
	if ( !iMaterial )
	{
		if ( !g_pErrorMaterial )
			return;
		Warning( "Programming error: CMatRenderContext::Bind: NULL material\n" );
		iMaterial = g_pErrorMaterial;
	}
	else
	{
		iMaterial = iMaterial->CheckProxyReplacement( proxyData );
	}

	IMaterialInternal *material = static_cast<IMaterialInternal *>( iMaterial );
	material = material->GetRealTimeVersion(); //always work with the real time versions of materials internally
	if ( material->GetReferenceCount() <= 0 )
	{
		static ConVarRef matTextureListConVar( "mat_texture_list" );
		static ConVarRef matShowWaterTextureConVar( "mat_showwatertextures" );

		if ( ( !matTextureListConVar.IsValid() || !matTextureListConVar.GetBool() ) &&
		     ( !matShowWaterTextureConVar.IsValid() || !matShowWaterTextureConVar.GetBool() ))
		{
			Warning( "Material %s has bad reference count %d when being bound\n", material->GetName(), material->GetReferenceCount() );
			// The usual solution for this for global materials that really don't need refcounting is to do material->AddRef();
			Assert( 0 );
			iMaterial = g_pErrorMaterial;
		}
	}

	if (g_config.bDrawFlat && !material->NoDebugOverride())
	{
		material = static_cast<IMaterialInternal *>( GetDrawFlatMaterial() );
	}

	CMatRenderContextBase::Bind( iMaterial, proxyData );

	// We've always gotta call the bind proxy
	SyncMatrices();
	if ( GetMaterialSystem()->GetThreadMode() == MATERIAL_SINGLE_THREADED )
	{
		GetCurrentMaterialInternal()->CallBindProxy( proxyData );
	}
	g_pShaderAPI->Bind( GetCurrentMaterialInternal() );
}

void CMatRenderContext::CopyRenderTargetToTextureEx( ITexture *pTexture, int nRenderTargetID, Rect_t *pSrcRect, Rect_t *pDstRect )
{
	if ( !pTexture )
	{
		Assert( 0 );
		return;
	}

	GetMaterialSystem()->Flush( false );
	ITextureInternal *pTextureInternal = (ITextureInternal *)pTexture;

	if ( IsPC() || !IsX360() )
	{
		pTextureInternal->CopyFrameBufferToMe( nRenderTargetID, pSrcRect, pDstRect );
	}
	else
	{
		// X360 only does 1:1 resolves. So we can do full resolves to textures of size 
		// equal or greater than the viewport trivially. Downsizing is nasty.
		Rect_t srcRect;
		if ( !pSrcRect )
		{
			// build out source rect
			pSrcRect = &srcRect;
			int x, y, w, h;
			GetViewport( x, y, w, h );

			pSrcRect->x = 0;
			pSrcRect->y = 0;
			pSrcRect->width = w;
			pSrcRect->height = h;
		}

		Rect_t dstRect;
		if ( !pDstRect )
		{
			// build out target rect
			pDstRect = &dstRect;

			pDstRect->x = 0;
			pDstRect->y = 0;
			pDstRect->width = pTexture->GetActualWidth();
			pDstRect->height = pTexture->GetActualHeight();
		}

		if ( pSrcRect->width == pDstRect->width && pSrcRect->height == pDstRect->height )
		{
			// 1:1 mapping, no stretching needed, use direct path
			pTextureInternal->CopyFrameBufferToMe( nRenderTargetID, pSrcRect, pDstRect );
			return;
		}

		if( (pDstRect->x == 0) && (pDstRect->y == 0) && 
			(pDstRect->width == pTexture->GetActualWidth()) && (pDstRect->height == pTexture->GetActualHeight()) &&
			(pDstRect->width >= pSrcRect->width) && (pDstRect->height >= pSrcRect->height) )
		{
			// Resolve takes up the whole texture, and the texture is large enough to hold the resolve.
			// This is turned into a 1:1 resolve within shaderapi by making D3D think the texture is smaller from now on. (Until it resolves from a bigger source)
			pTextureInternal->CopyFrameBufferToMe( nRenderTargetID, pSrcRect, pDstRect );
			return;
		}

		// currently assuming disparate copies are only for FB blits
		// ensure active render target is actually the back buffer
		Assert( m_RenderTargetStack.Top().m_pRenderTargets[0] == NULL );

		// nasty sequence:
		// resolve FB surface to matching clone DDR texture
		// gpu draw from clone DDR FB texture to disparate RT target surface
		// resolve to its matching DDR clone texture
		ITextureInternal *pFullFrameFB = (ITextureInternal*)GetMaterialSystem()->FindTexture( "_rt_FullFrameFB", TEXTURE_GROUP_RENDER_TARGET );
		pFullFrameFB->CopyFrameBufferToMe( nRenderTargetID, NULL, NULL );

		// target texture must be a render target
		PushRenderTargetAndViewport( pTexture );

		// blit FB source to render target
		DrawScreenSpaceRectangle(
			GetMaterialSystem()->GetRenderTargetBlitMaterial(),
			pDstRect->x, pDstRect->y, pDstRect->width, pDstRect->height,
			pSrcRect->x, pSrcRect->y, pSrcRect->x+pSrcRect->width-1, pSrcRect->y+pSrcRect->height-1, 
			pFullFrameFB->GetActualWidth(), pFullFrameFB->GetActualHeight() );

		// resolve render target to texture
		((ITextureInternal *)pTexture)->CopyFrameBufferToMe( 0, NULL, NULL );

		// restore render target and viewport
		PopRenderTargetAndViewport();
	}
}

void CMatRenderContext::CopyRenderTargetToTexture( ITexture *pTexture )
{
	CopyRenderTargetToTextureEx( pTexture, NULL, NULL );
}


void CMatRenderContext::CopyTextureToRenderTargetEx( int nRenderTargetID, ITexture *pTexture, Rect_t *pSrcRect, Rect_t *pDstRect )
{
	if ( !pTexture )
	{
		Assert( 0 );
		return;
	}

	GetMaterialSystem()->Flush( false );
	ITextureInternal *pTextureInternal = (ITextureInternal *)pTexture;

	if ( IsPC() || !IsX360() )
	{
		pTextureInternal->CopyMeToFrameBuffer( nRenderTargetID, pSrcRect, pDstRect );
	}
	else
	{
		Assert( 0 );
	}
}


void CMatRenderContext::ClearBuffers( bool bClearColor, bool bClearDepth, bool bClearStencil )
{
	int width, height;
	GetRenderTargetDimensions( width, height );
	g_pShaderAPI->ClearBuffers( bClearColor, bClearDepth, bClearStencil, width, height );
}

void CMatRenderContext::DrawClearBufferQuad( unsigned char r, unsigned char g, unsigned char b, unsigned char a, bool bClearColor, bool bClearAlpha, bool bClearDepth )
{
	IMaterialInternal *pClearMaterial = GetBufferClearObeyStencil( bClearColor + ( bClearAlpha << 1 ) + ( bClearDepth << 2 ) );
	Bind( pClearMaterial );

	IMesh* pMesh = GetDynamicMesh( true );

	MatrixMode( MATERIAL_MODEL );
	PushMatrix();
	LoadIdentity();

	MatrixMode( MATERIAL_VIEW );
	PushMatrix();
	LoadIdentity();

	MatrixMode( MATERIAL_PROJECTION );
	PushMatrix();
	LoadIdentity();

	float flDepth = GetMaterialSystem()->GetConfig().bReverseDepth ? 0.0f : 1.0f;

	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );

	//1.1 instead of 1.0 to fix small borders around the edges in full screen with anti-aliasing enabled
	meshBuilder.Position3f( -1.1f, -1.1f, flDepth );
	meshBuilder.Color4ub( r, g, b, a );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( -1.1f, 1.1f, flDepth );
	meshBuilder.Color4ub( r, g, b, a );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( 1.1f, 1.1f, flDepth );
	meshBuilder.Color4ub( r, g, b, a );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( 1.1f, -1.1f, flDepth );
	meshBuilder.Color4ub( r, g, b, a );
	meshBuilder.AdvanceVertex();

	meshBuilder.End();
	pMesh->Draw();

	MatrixMode( MATERIAL_MODEL );
	PopMatrix();

	MatrixMode( MATERIAL_VIEW );
	PopMatrix();

	MatrixMode( MATERIAL_PROJECTION );
	PopMatrix();
}

//-----------------------------------------------------------------------------
// Should really be called SetViewport
//-----------------------------------------------------------------------------
void CMatRenderContext::Viewport( int x, int y, int width, int height )
{
	CMatRenderContextBase::Viewport( x, y, width, height );

	// If either dimension is negative, set to full bounds of current target
	if ( (width < 0) || (height < 0) )
	{
		ITexture *pTarget = m_RenderTargetStack.Top().m_pRenderTargets[0];

		// If target is the back buffer
		if ( pTarget == NULL )
		{
			m_Viewport.m_nTopLeftX = 0;
			m_Viewport.m_nTopLeftY = 0;
			g_pShaderAPI->GetBackBufferDimensions( m_Viewport.m_nWidth, m_Viewport.m_nHeight );
			g_pShaderAPI->SetViewports( 1, &m_Viewport );
		}
		else // target is a texture
		{
			m_Viewport.m_nTopLeftX = 0;
			m_Viewport.m_nTopLeftY = 0;
			m_Viewport.m_nWidth = pTarget->GetActualWidth();
			m_Viewport.m_nHeight = pTarget->GetActualHeight();
			g_pShaderAPI->SetViewports( 1, &m_Viewport );
		}
	}
	else // use the bounds passed in
	{
		m_Viewport.m_nTopLeftX = x;
		m_Viewport.m_nTopLeftY = y;
		m_Viewport.m_nWidth = width;
		m_Viewport.m_nHeight = height;
		g_pShaderAPI->SetViewports( 1, &m_Viewport );
	}
}

void CMatRenderContext::GetViewport( int& x, int& y, int& width, int& height ) const
{
	// Verify valid top of RT stack
	Assert ( m_RenderTargetStack.Count() > 0 );

	// Grab the top of stack
	const RenderTargetStackElement_t& element = m_RenderTargetStack.Top();

	// If either dimension is not positive, set to full bounds of current target
	if ( (element.m_nViewW <= 0) || (element.m_nViewH <= 0) )
	{
		// Viewport origin at target origin
		x = y = 0;

		// If target is back buffer
		if ( element.m_pRenderTargets[0] == NULL )
		{
			g_pShaderAPI->GetBackBufferDimensions( width, height );
		}
		else // if target is texture
		{
			width = element.m_pRenderTargets[0]->GetActualWidth();
			height = element.m_pRenderTargets[0]->GetActualHeight();
		}
	}
	else // use the bounds from the stack directly
	{
		x = element.m_nViewX;
		y = element.m_nViewY;
		width = element.m_nViewW;
		height = element.m_nViewH;
	}
}



//-----------------------------------------------------------------------------
// Methods related to user clip planes
//-----------------------------------------------------------------------------
void CMatRenderContext::UpdateHeightClipUserClipPlane( void )
{
	PlaneStackElement pse;
	pse.bHack_IsHeightClipPlane = true;

	int iExistingHeightClipPlaneIndex;
	for( iExistingHeightClipPlaneIndex = m_CustomClipPlanes.Count(); --iExistingHeightClipPlaneIndex >= 0; )
	{
		if( m_CustomClipPlanes[iExistingHeightClipPlaneIndex].bHack_IsHeightClipPlane )
			break;
	}
	
	switch( m_HeightClipMode )
	{
	case MATERIAL_HEIGHTCLIPMODE_DISABLE:
		if( iExistingHeightClipPlaneIndex != -1 )
			m_CustomClipPlanes.Remove( iExistingHeightClipPlaneIndex );
		break;
	case MATERIAL_HEIGHTCLIPMODE_RENDER_ABOVE_HEIGHT:
		pse.fValues[0] = 0.0f;
		pse.fValues[1] = 0.0f;
		pse.fValues[2] = 1.0f;
		pse.fValues[3] = m_HeightClipZ;
		if( iExistingHeightClipPlaneIndex != -1 )
		{
			memcpy( m_CustomClipPlanes.Base() + iExistingHeightClipPlaneIndex, &pse, sizeof( PlaneStackElement ) );
		}
		else
		{
			m_CustomClipPlanes.AddToTail( pse );
		}
		break;
	case MATERIAL_HEIGHTCLIPMODE_RENDER_BELOW_HEIGHT:
		pse.fValues[0] = 0.0f;
		pse.fValues[1] = 0.0f;
		pse.fValues[2] = -1.0f;
		pse.fValues[3] = -m_HeightClipZ;
		if( iExistingHeightClipPlaneIndex != -1 )
		{
			memcpy( m_CustomClipPlanes.Base() + iExistingHeightClipPlaneIndex, &pse, sizeof( PlaneStackElement ) );
		}
		else
		{
			m_CustomClipPlanes.AddToTail( pse );
		}
		break;
	};

	ApplyCustomClipPlanes();
	
	/*switch( m_HeightClipMode )
	{
	case MATERIAL_HEIGHTCLIPMODE_DISABLE:
		g_pShaderAPI->EnableClipPlane( 0, false );
		break;
	case MATERIAL_HEIGHTCLIPMODE_RENDER_ABOVE_HEIGHT:
		plane[0] = 0.0f;
		plane[1] = 0.0f;
		plane[2] = 1.0f;
		plane[3] = m_HeightClipZ;
		g_pShaderAPI->SetClipPlane( 0, plane );
		g_pShaderAPI->EnableClipPlane( 0, true );
		break;
	case MATERIAL_HEIGHTCLIPMODE_RENDER_BELOW_HEIGHT:
		plane[0] = 0.0f;
		plane[1] = 0.0f;
		plane[2] = -1.0f;
		plane[3] = -m_HeightClipZ;
		g_pShaderAPI->SetClipPlane( 0, plane );
		g_pShaderAPI->EnableClipPlane( 0, true );
		break;
	}*/
}


//-----------------------------------------------------------------------------
// Lightmap stuff
//-----------------------------------------------------------------------------
void CMatRenderContext::BindLightmapPage( int lightmapPageID )
{
	if ( m_lightmapPageID == lightmapPageID )
		return;

	// We gotta make sure there's no buffered primitives 'cause this'll
	// change the render state.
	g_pShaderAPI->FlushBufferedPrimitives();

	CMatRenderContextBase::BindLightmapPage( lightmapPageID );
}

void CMatRenderContext::BindLightmapTexture( ITexture *pLightmapTexture )
{
	if ( ( m_lightmapPageID == MATERIAL_SYSTEM_LIGHTMAP_PAGE_USER_DEFINED ) && ( m_pUserDefinedLightmap == pLightmapTexture ) )
		return;

	// We gotta make sure there's no buffered primitives 'cause this'll
	// change the render state.
	g_pShaderAPI->FlushBufferedPrimitives();

	m_lightmapPageID = MATERIAL_SYSTEM_LIGHTMAP_PAGE_USER_DEFINED;
	if ( pLightmapTexture )
	{
		pLightmapTexture->IncrementReferenceCount();
	}
	if ( m_pUserDefinedLightmap )
	{
		m_pUserDefinedLightmap->DecrementReferenceCount();
	}
	m_pUserDefinedLightmap = static_cast<ITextureInternal*>( pLightmapTexture );
}


void CMatRenderContext::BindLightmap( Sampler_t sampler )
{
	switch ( m_lightmapPageID )
	{
	default:
		Assert( ( m_lightmapPageID == 0 && GetLightmaps()->GetNumLightmapPages() == 0 ) || ( m_lightmapPageID >= 0 && m_lightmapPageID < GetLightmaps()->GetNumLightmapPages() ) );
		if( m_lightmapPageID >= 0 && m_lightmapPageID < GetLightmaps()->GetNumLightmapPages() )
		{
			g_pShaderAPI->BindTexture( sampler, GetLightmaps()->GetLightmapPageTextureHandle( m_lightmapPageID ) );
		}
		break;

	case MATERIAL_SYSTEM_LIGHTMAP_PAGE_USER_DEFINED:
		AssertOnce( m_pUserDefinedLightmap );
		g_pShaderAPI->BindTexture( sampler, m_pUserDefinedLightmap->GetTextureHandle( 0 ) );
		break;

	case MATERIAL_SYSTEM_LIGHTMAP_PAGE_WHITE:
		BindFullbrightLightmap( sampler );
		break;

	case MATERIAL_SYSTEM_LIGHTMAP_PAGE_WHITE_BUMP:
		BindBumpedFullbrightLightmap( sampler );
		break;
	}
}

void CMatRenderContext::BindBumpLightmap( Sampler_t sampler )
{
	switch ( m_lightmapPageID )
	{
	default:
		Assert( m_lightmapPageID >= 0 && m_lightmapPageID < GetLightmaps()->GetNumLightmapPages() );
		if( m_lightmapPageID >= 0 && m_lightmapPageID < GetLightmaps()->GetNumLightmapPages() )
		{
			g_pShaderAPI->BindTexture( sampler, GetLightmaps()->GetLightmapPageTextureHandle( m_lightmapPageID ) );
			g_pShaderAPI->BindTexture( (Sampler_t)(sampler+1), GetLightmaps()->GetLightmapPageTextureHandle( m_lightmapPageID ) );
			g_pShaderAPI->BindTexture( (Sampler_t)(sampler+2), GetLightmaps()->GetLightmapPageTextureHandle( m_lightmapPageID ) );
		}
		break;
	case MATERIAL_SYSTEM_LIGHTMAP_PAGE_USER_DEFINED:
		AssertOnce( m_pUserDefinedLightmap );
		g_pShaderAPI->BindTexture( sampler, m_pUserDefinedLightmap->GetTextureHandle( 0 ) );
		g_pShaderAPI->BindTexture( (Sampler_t)(sampler+1), m_pUserDefinedLightmap->GetTextureHandle( 0 ) );
		g_pShaderAPI->BindTexture( (Sampler_t)(sampler+2), m_pUserDefinedLightmap->GetTextureHandle( 0 ) );
		break;
	case MATERIAL_SYSTEM_LIGHTMAP_PAGE_WHITE_BUMP:
		BindBumpedFullbrightLightmap( sampler );
		BindBumpedFullbrightLightmap( (Sampler_t)(sampler+1) );
		BindBumpedFullbrightLightmap( (Sampler_t)(sampler+2) );
		break;
	case MATERIAL_SYSTEM_LIGHTMAP_PAGE_WHITE:
		BindBumpedFullbrightLightmap( sampler );
		BindBumpedFullbrightLightmap( (Sampler_t)(sampler+1) );
		BindBumpedFullbrightLightmap( (Sampler_t)(sampler+2) );
		break;
	}
}

void CMatRenderContext::BindFullbrightLightmap( Sampler_t sampler )
{
	g_pShaderAPI->BindTexture( sampler, GetFullbrightLightmapTextureHandle() );
}

void CMatRenderContext::BindBumpedFullbrightLightmap( Sampler_t sampler )
{
	g_pShaderAPI->BindTexture( sampler, GetFullbrightBumpedLightmapTextureHandle() );
}


//-----------------------------------------------------------------------------
// Bind standard textures
//-----------------------------------------------------------------------------
void CMatRenderContext::BindStandardTexture( Sampler_t sampler, StandardTextureId_t id )
{
	switch ( id )
	{
	case TEXTURE_LIGHTMAP:
		BindLightmap( sampler );
		return;

	case TEXTURE_LIGHTMAP_BUMPED:
		BindBumpLightmap( sampler );
		return;

	case TEXTURE_LIGHTMAP_FULLBRIGHT:
		BindFullbrightLightmap( sampler );
		return;

	case TEXTURE_LIGHTMAP_BUMPED_FULLBRIGHT:
		BindBumpedFullbrightLightmap( sampler );
		return;

	case TEXTURE_WHITE:
		g_pShaderAPI->BindTexture( sampler, GetWhiteTextureHandle() );
		return;

	case TEXTURE_BLACK:
		g_pShaderAPI->BindTexture( sampler, GetBlackTextureHandle() );
		return;

	case TEXTURE_GREY:
		g_pShaderAPI->BindTexture( sampler, GetGreyTextureHandle() );
		return;

	case TEXTURE_GREY_ALPHA_ZERO:
		g_pShaderAPI->BindTexture( sampler, GetGreyAlphaZeroTextureHandle() );
		return;

	case TEXTURE_NORMALMAP_FLAT:
		g_pShaderAPI->BindTexture( sampler, GetFlatNormalTextureHandle() );
		return;

	case TEXTURE_NORMALIZATION_CUBEMAP:
		TextureManager()->NormalizationCubemap()->Bind( sampler );
		return;

	case TEXTURE_NORMALIZATION_CUBEMAP_SIGNED:
		TextureManager()->SignedNormalizationCubemap()->Bind( sampler );
		return;

	case TEXTURE_FRAME_BUFFER_FULL_TEXTURE_0:
	case TEXTURE_FRAME_BUFFER_FULL_TEXTURE_1:
		{
			int nTextureIndex = id - TEXTURE_FRAME_BUFFER_FULL_TEXTURE_0; 
			if( m_pCurrentFrameBufferCopyTexture[ nTextureIndex ] )
			{
				( ( ITextureInternal * )m_pCurrentFrameBufferCopyTexture[ nTextureIndex ] )->Bind( sampler );
			}
		}
		return;

	case TEXTURE_COLOR_CORRECTION_VOLUME_0:
	case TEXTURE_COLOR_CORRECTION_VOLUME_1:
	case TEXTURE_COLOR_CORRECTION_VOLUME_2:
	case TEXTURE_COLOR_CORRECTION_VOLUME_3:
		{
			ITextureInternal *pTexture = TextureManager()->ColorCorrectionTexture( id - TEXTURE_COLOR_CORRECTION_VOLUME_0 );
			if( pTexture )
			{
				pTexture->Bind( sampler );
			}
		}
		return;

	case TEXTURE_SHADOW_NOISE_2D:
		TextureManager()->ShadowNoise2D()->Bind( sampler );
		return;

	case TEXTURE_IDENTITY_LIGHTWARP:
		TextureManager()->IdentityLightWarp()->Bind( sampler );
		return;

	case TEXTURE_MORPH_ACCUMULATOR:
		g_pMorphMgr->MorphAccumulator()->Bind( sampler );
		return;

	case TEXTURE_MORPH_WEIGHTS:
		g_pMorphMgr->MorphWeights()->Bind( sampler );
		return;

	case TEXTURE_FRAME_BUFFER_FULL_DEPTH:
		if( m_bFullFrameDepthIsValid )
			TextureManager()->FullFrameDepthTexture()->Bind( sampler );
		else
			g_pShaderAPI->BindTexture( sampler, GetMaxDepthTextureHandle() );
		return;

	case TEXTURE_DEBUG_LUXELS:
		TextureManager()->DebugLuxels2D()->Bind( sampler );
		return;

	default:
		Assert(0);
	}
}

void CMatRenderContext::BindStandardVertexTexture( VertexTextureSampler_t sampler, StandardTextureId_t id )
{
	switch ( id )
	{
	case TEXTURE_MORPH_ACCUMULATOR:
		g_pMorphMgr->MorphAccumulator()->BindVertexTexture( sampler );
		return;

	case TEXTURE_MORPH_WEIGHTS:
		g_pMorphMgr->MorphWeights()->BindVertexTexture( sampler );
		return;

	default:
		Assert(0);
	}
}

void CMatRenderContext::GetStandardTextureDimensions( int *pWidth, int *pHeight, StandardTextureId_t id )
{
	ITexture *pTexture = NULL;
	switch ( id )
	{
	case TEXTURE_LIGHTMAP:
	case TEXTURE_LIGHTMAP_BUMPED:
	case TEXTURE_LIGHTMAP_FULLBRIGHT:
	case TEXTURE_LIGHTMAP_BUMPED_FULLBRIGHT:
		// NOTE: Doesn't exactly work since we may be in fullbright mode
//		GetLightmapDimensions( pWidth, pHeight );
//		break;

	case TEXTURE_WHITE:
	case TEXTURE_BLACK:
	case TEXTURE_GREY:
	case TEXTURE_GREY_ALPHA_ZERO:
	case TEXTURE_NORMALMAP_FLAT:
	default:
		Assert( 0 );
		Warning( "GetStandardTextureDimensions: still unimplemented for this type!\n" );
		*pWidth = *pHeight = -1;
		break;

	case TEXTURE_NORMALIZATION_CUBEMAP:
		pTexture = TextureManager()->NormalizationCubemap();
		break;

	case TEXTURE_NORMALIZATION_CUBEMAP_SIGNED:
		pTexture = TextureManager()->SignedNormalizationCubemap();
		break;

	case TEXTURE_FRAME_BUFFER_FULL_TEXTURE_0:
	case TEXTURE_FRAME_BUFFER_FULL_TEXTURE_1:
		{
			int nTextureIndex = id - TEXTURE_FRAME_BUFFER_FULL_TEXTURE_0; 
			pTexture = m_pCurrentFrameBufferCopyTexture[ nTextureIndex ];
		}
		break;

	case TEXTURE_COLOR_CORRECTION_VOLUME_0:
	case TEXTURE_COLOR_CORRECTION_VOLUME_1:
	case TEXTURE_COLOR_CORRECTION_VOLUME_2:
	case TEXTURE_COLOR_CORRECTION_VOLUME_3:
		pTexture = TextureManager()->ColorCorrectionTexture( id - TEXTURE_COLOR_CORRECTION_VOLUME_0 );
		break;

	case TEXTURE_SHADOW_NOISE_2D:
		pTexture = TextureManager()->ShadowNoise2D();
		break;

	case TEXTURE_IDENTITY_LIGHTWARP:
		pTexture = TextureManager()->IdentityLightWarp();
		return;

	case TEXTURE_MORPH_ACCUMULATOR:
		pTexture = g_pMorphMgr->MorphAccumulator();
		break;

	case TEXTURE_MORPH_WEIGHTS:
		pTexture = g_pMorphMgr->MorphWeights();
		break;

	case TEXTURE_DEBUG_LUXELS:
		pTexture = TextureManager()->DebugLuxels2D();
		break;
	}

	if ( pTexture )
	{
		*pWidth = pTexture->GetActualWidth();
		*pHeight = pTexture->GetActualHeight();
	}
	else
	{
		Warning( "GetStandardTextureDimensions: Couldn't find the texture to get the dimensions!\n" );
		*pWidth = *pHeight = -1;
	}
}

void CMatRenderContext::FogColor3f( float r, float g, float b )
{
	unsigned char fogColor[3];
	fogColor[0] = clamp( (int)(r * 255.0f), 0, 255 );
	fogColor[1] = clamp( (int)(g * 255.0f), 0, 255 );
	fogColor[2] = clamp( (int)(b * 255.0f), 0, 255 );
	g_pShaderAPI->SceneFogColor3ub( fogColor[0], fogColor[1], fogColor[2] );
}

void CMatRenderContext::FogColor3fv( const float* rgb )
{
	unsigned char fogColor[3];
	fogColor[0] = clamp( (int)(rgb[0] * 255.0f), 0, 255 );
	fogColor[1] = clamp( (int)(rgb[1] * 255.0f), 0, 255 );
	fogColor[2] = clamp( (int)(rgb[2] * 255.0f), 0, 255 );
	g_pShaderAPI->SceneFogColor3ub( fogColor[0], fogColor[1], fogColor[2] );
}



void CMatRenderContext::SetFlashlightMode( bool bEnable )
{
	if( bEnable != m_bFlashlightEnable )
	{
		g_pShaderAPI->FlushBufferedPrimitives();
		m_bFlashlightEnable = bEnable;
	}
}

bool CMatRenderContext::GetFlashlightMode( ) const
{
	return m_bFlashlightEnable;
}

void CMatRenderContext::SetFlashlightStateEx( const FlashlightState_t &state, const VMatrix &worldToTexture, ITexture *pFlashlightDepthTexture )
{
	g_pShaderAPI->SetFlashlightStateEx( state, worldToTexture, pFlashlightDepthTexture );
	if ( IsPC() && g_config.dxSupportLevel <= 70 )
	{
		// Going to go ahead and set a single hardware light here to do all lighting except for
		// the spotlight falloff function, which is done with a texture.
		SetAmbientLight( 0.0f, 0.0f, 0.0f );
		static Vector4D blackCube[6];
		int i;
		for( i = 0; i < 6; i++ )
		{
			blackCube[i].Init( 0.0f, 0.0f, 0.0f, 0.0f );
		}
		SetAmbientLightCube( blackCube );		

		// Disable all the lights except for the first one.
		for( i = 1; i < HardwareConfig()->MaxNumLights(); ++i )
		{
			LightDesc_t desc;
			desc.m_Type = MATERIAL_LIGHT_DISABLE;
			SetLight( i, desc );
		}

		LightDesc_t desc;
		desc.m_Type = MATERIAL_LIGHT_POINT;
		desc.m_Attenuation0 = state.m_fConstantAtten;
		desc.m_Attenuation1 = state.m_fLinearAtten;
		desc.m_Attenuation2 = state.m_fQuadraticAtten;
		// flashlightfixme: I don't know why this scale has to be here to get fixed function lighting to work.
		desc.m_Color.x = state.m_Color[0] * 17000.0f;
		desc.m_Color.y = state.m_Color[1] * 17000.0f;
		desc.m_Color.z = state.m_Color[2] * 17000.0f;
		desc.m_Position = state.m_vecLightOrigin;

		QAngle angles;
		QuaternionAngles( state.m_quatOrientation, angles );
		AngleVectors( angles, &desc.m_Direction );

		desc.m_Range = state.m_FarZ;
		desc.m_Falloff = 0.0f;
		SetLight( 0, desc );
	}
}

void CMatRenderContext::SetScissorRect( const int nLeft, const int nTop, const int nRight, const int nBottom, const bool bEnableScissor )
{
	g_pShaderAPI->SetScissorRect( nLeft, nTop, nRight, nBottom, bEnableScissor );
}

void CMatRenderContext::SetToneMappingScaleLinear( const Vector &scale )
{
	g_pShaderAPI->SetToneMappingScaleLinear( scale );
}

void CMatRenderContext::BeginBatch( IMesh* pIndices )
{
	Assert( !m_pBatchMesh && !m_pBatchIndices);
	m_pBatchIndices = pIndices;
}

void CMatRenderContext::BindBatch( IMesh* pVertices, IMaterial *pAutoBind )
{
	m_pBatchMesh = GetDynamicMesh( false, pVertices, m_pBatchIndices, pAutoBind );
}

void CMatRenderContext::DrawBatch(int firstIndex, int numIndices )
{
	Assert( m_pBatchMesh );
	m_pBatchMesh->Draw( firstIndex, numIndices );
}

void CMatRenderContext::EndBatch()
{
	m_pBatchIndices = NULL;
	m_pBatchMesh = NULL;
}

bool CMatRenderContext::OnDrawMesh( IMesh *pMesh, int firstIndex, int numIndices )
{
	SyncMatrices();
	return true;
}

bool CMatRenderContext::OnDrawMesh( IMesh *pMesh, CPrimList *pLists, int nLists )
{
	SyncMatrices();
	return true;
}

void CMatRenderContext::AsyncCreateTextureFromRenderTarget( ITexture* pSrcRt, const char* pDstName, ImageFormat dstFmt, bool bGenMips, int nAdditionalCreationFlags, IAsyncTextureOperationReceiver* pRecipient, void* pExtraArgs )
{
	if ( g_pMaterialSystem->GetThreadMode() == MATERIAL_SINGLE_THREADED ) 
	{
		OnAsyncCreateTextureFromRenderTarget( pSrcRt, &pDstName, pRecipient );
	}

	TextureManager()->AsyncCreateTextureFromRenderTarget( pSrcRt, pDstName, dstFmt, bGenMips, nAdditionalCreationFlags, pRecipient, pExtraArgs );
}

void CMatRenderContext::AsyncMap( ITextureInternal* pTexToMap, IAsyncTextureOperationReceiver* pRecipient, void* pExtraArgs )
{
	if ( g_pMaterialSystem->GetThreadMode() == MATERIAL_SINGLE_THREADED )
	{
		OnAsyncMap( pTexToMap, pRecipient, pExtraArgs );
	}

	void* pMemory = NULL;
	int nPitch = NULL;

	pTexToMap->Map( &pMemory, &nPitch );

	pRecipient->OnAsyncMapComplete( pTexToMap, pExtraArgs, pMemory, nPitch );

	// Release references held earlier in OnAsyncMap
	SafeRelease( &pRecipient );
	SafeRelease( &pTexToMap );
}

void CMatRenderContext::AsyncUnmap( ITextureInternal* pTexToUnmap )
{
	if ( g_pMaterialSystem->GetThreadMode() == MATERIAL_SINGLE_THREADED )
	{
		OnAsyncUnmap( pTexToUnmap );
	}

	pTexToUnmap->Unmap();
	SafeRelease( &pTexToUnmap ); // Matches AddRef from OnAsyncUnmap
}

void CMatRenderContext::AsyncCopyRenderTargetToStagingTexture( ITexture* pDst, ITexture* pSrc, IAsyncTextureOperationReceiver* pRecipient, void* pExtraArgs )
{
	if ( g_pMaterialSystem->GetThreadMode() == MATERIAL_SINGLE_THREADED )
	{
		OnAsyncCopyRenderTargetToStagingTexture( pDst, pSrc, pRecipient );
	}

	pSrc->CopyToStagingTexture( pDst );
	pRecipient->OnAsyncReadbackBegin( pDst, pSrc, pExtraArgs );

	SafeRelease( &pDst );
	SafeRelease( &pSrc );
	SafeRelease( &pRecipient );
}


//-----------------------------------------------------------------------------
// Methods related to morph accumulation
//-----------------------------------------------------------------------------
void CMatRenderContext::BeginMorphAccumulation()
{
	g_pMorphMgr->BeginMorphAccumulation( m_pMorphRenderContext );
}

void CMatRenderContext::EndMorphAccumulation()
{
	g_pMorphMgr->EndMorphAccumulation( m_pMorphRenderContext );
}

void CMatRenderContext::AccumulateMorph( IMorph* pMorph, int nMorphCount, const MorphWeight_t* pWeights )
{
	g_pMorphMgr->AccumulateMorph( m_pMorphRenderContext, pMorph, nMorphCount, pWeights );
}

bool CMatRenderContext::GetMorphAccumulatorTexCoord( Vector2D *pTexCoord, IMorph *pMorph, int nVertex )
{
	return g_pMorphMgr->GetMorphAccumulatorTexCoord( m_pMorphRenderContext, pTexCoord, pMorph, nVertex );
}


//-----------------------------------------------------------------------------
// Occlusion query support
//-----------------------------------------------------------------------------
OcclusionQueryObjectHandle_t CMatRenderContext::CreateOcclusionQueryObject()
{
	OcclusionQueryObjectHandle_t h = g_pOcclusionQueryMgr->CreateOcclusionQueryObject();
	g_pOcclusionQueryMgr->OnCreateOcclusionQueryObject( h );
	return h;
}

int CMatRenderContext::OcclusionQuery_GetNumPixelsRendered( OcclusionQueryObjectHandle_t h )
{
	return g_pOcclusionQueryMgr->OcclusionQuery_GetNumPixelsRendered( h, true );
}




void CMatRenderContext::SetFullScreenDepthTextureValidityFlag( bool bIsValid )
{
	m_bFullFrameDepthIsValid = bIsValid;
}

//-----------------------------------------------------------------------------
// Debug logging
//-----------------------------------------------------------------------------

void	CMatRenderContext::PrintfVA( char *fmt, va_list vargs )
{
	#if GLMDEBUG
		g_pShaderAPI->PrintfVA( fmt, vargs );
	#endif
}

void	CMatRenderContext::Printf( const char *fmt, ... )
{
	#if GLMDEBUG
		va_list	vargs;

		va_start(vargs, fmt);

		g_pShaderAPI->PrintfVA( (char *)fmt, vargs );

		va_end( vargs );
	#endif
}

float	CMatRenderContext::Knob( char *knobname, float *setvalue )
{
	#if GLMDEBUG
		return g_pShaderAPI->Knob( knobname, setvalue );
	#else
		return 0.0f;
	#endif
}

