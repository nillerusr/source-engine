//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================

#ifndef CMATERIALRENDERSTATE_H
#define CMATERIALRENDERSTATE_H

#include "tier1/delegates.h"
#include "tier1/utlstack.h"
#include "bitvec.h"
#include "materialsystem_global.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/ishaderapi.h"
#include "imaterialinternal.h"
#include "shadersystem.h"
#include "imorphinternal.h"
#include "imatrendercontextinternal.h"
#include "occlusionquerymgr.h"
#include "materialsystem/MaterialSystemUtil.h"

#ifndef MATSYS_INTERNAL
#error "This file is private to the implementation of IMaterialSystem/IMaterialSystemInternal"
#endif

#if defined( _WIN32 )
#pragma once
#endif

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class ITextureInternal;
class CMaterialSystem;
class CMatLightmaps;
typedef int ShaderAPITextureHandle_t;
class IMorphMgrRenderContext;
class CMatCallQueue;


//-----------------------------------------------------------------------------
// Render targets
//-----------------------------------------------------------------------------
#if !defined( _X360 )
#define MAX_RENDER_TARGETS 4
#else
#define MAX_RENDER_TARGETS 1
#endif


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
class CMatRenderContextBase : public CRefCounted1<IMatRenderContextInternal>
{
public:
	virtual void							InitializeFrom( CMatRenderContextBase *pInitialState );

	InitReturnVal_t							Init();
	void									Shutdown();
	void									CompactMemory();

	void									SetFrameTime( float frameTime ) { m_FrameTime = frameTime; }

	ICallQueue *							GetCallQueue() { return NULL; }
	CMatCallQueue *							GetCallQueueInternal() { return NULL; }

	ITexture *								GetRenderTarget( void );
	ITexture *								GetRenderTargetEx( int nRenderTargetID );

	IMaterialInternal*						GetCurrentMaterialInternal() const								{ return m_pCurrentMaterial;	}	
	virtual void							SetCurrentMaterialInternal(IMaterialInternal* pCurrentMaterial)	{ m_pCurrentMaterial = pCurrentMaterial; Assert( (m_pCurrentMaterial == NULL) || ((IMaterialInternal *)m_pCurrentMaterial)->IsRealTimeVersion() );	}
	IMaterial *								GetCurrentMaterial()											{ return GetCurrentMaterialInternal(); }
	virtual void *							GetCurrentProxy()												{ return m_pCurrentProxyData; }
	virtual void							SetCurrentProxy( void *pProxyData )								{ m_pCurrentProxyData = pProxyData; }

	void									Bind( IMaterial *material, void *proxyData = NULL );
	void									BindLightmapPage( int lightmapPageID );
	void									BindLocalCubemap( ITexture *pTexture );

	// matrix api
	void									MatrixMode( MaterialMatrixMode_t);
	void									PushMatrix();
	void									PopMatrix();
	void									LoadMatrix( const VMatrix& matrix );
	void									LoadMatrix( const matrix3x4_t& matrix );
	void									MultMatrix( const VMatrix& matrix );
	void									MultMatrixLocal( const VMatrix& matrix );
	void									MultMatrix( const matrix3x4_t& matrix );
	void									MultMatrixLocal( const matrix3x4_t& matrix );
	void									GetMatrix( MaterialMatrixMode_t matrixMode, VMatrix *pMatrix );
	void									GetMatrix( MaterialMatrixMode_t matrixMode, matrix3x4_t *pMatrix );
	void									LoadIdentity();
	void									Ortho( double, double, double, double, double, double);
	void									PerspectiveX( double, double, double, double);
	void									PerspectiveOffCenterX( double, double, double, double, double, double, double, double );
	void									PickMatrix( int, int, int, int);
	void									Rotate( float, float, float, float);
	void									Translate( float, float, float);
	void									Scale( float, float, float);
	// end matrix api

	// Set the current texture that is a copy of the framebuffer.
	void									SetFrameBufferCopyTexture( ITexture *pTexture, int textureIndex );
	ITexture *								GetFrameBufferCopyTexture( int textureIndex );

	void									SetHeightClipMode( MaterialHeightClipMode_t mode );
	MaterialHeightClipMode_t				GetHeightClipMode( void );
	void									SetHeightClipZ( float z );

	bool									EnableClipping( bool bEnable );

	void									SetRenderTarget( ITexture *pTexture );
	void									SetRenderTargetEx( int nRenderTargetID, ITexture *pTexture );

	void									Viewport( int x, int y, int width, int height );
	void									PushRenderTargetAndViewport( );
	void									PushRenderTargetAndViewport( ITexture *pTexture );
	void									PushRenderTargetAndViewport( ITexture *pTexture, int nViewX, int nViewY, int nViewW, int nViewH );
	void									PushRenderTargetAndViewport( ITexture *pTexture, ITexture *pDepthTexture, int nViewX, int nViewY, int nViewW, int nViewH );
	void									PopRenderTargetAndViewport( void );

	void									SyncMatrices();
	void									SyncMatrix( MaterialMatrixMode_t );
	const VMatrix &							AccessCurrentMatrix() const { return m_pCurMatrixItem->matrix; }

	virtual void							UpdateHeightClipUserClipPlane( void ) {}
	virtual void							ApplyCustomClipPlanes( void ) {}

	float									ComputePixelDiameterOfSphere( const Vector& origin, float flRadius );
	float									ComputePixelWidthOfSphere( const Vector& origin, float flRadius ); // FIXME: REMOVE THIS FUNCTION!

	virtual void							GetWorldSpaceCameraPosition( Vector *pCameraPos );
	virtual void							GetWorldSpaceCameraVectors( Vector *pVecForward, Vector *pVecRight, Vector *pVecUp );

	void									ResetToneMappingScale( float scvalue);
	void									TurnOnToneMapping();
	Vector									GetToneMappingScaleLinear();
	void									SetGoalToneMappingScale( float monoscale);

	// Inherited from IMaterialSystemInternal
	int										GetLightmapPage( void );

	virtual void *							LockRenderData( int nSizeInBytes );
	virtual void							UnlockRenderData( void *pData );
	virtual void							AddRefRenderData();
	virtual void							ReleaseRenderData();
	virtual bool							IsRenderData( const void *pData ) const;
	void									MarkRenderDataUnused( bool bFrameEnd );
	int										RenderDataSizeUsed() const;

	// debugging
	virtual void							PrintfVA( char *fmt, va_list vargs );
	virtual void							Printf( PRINTF_FORMAT_STRING const char *fmt, ... );
	virtual	float							Knob( char *knobname, float *setvalue = NULL );

protected:
	void									OnAsyncCreateTextureFromRenderTarget( ITexture* pSrcRt, const char** pDstName, IAsyncTextureOperationReceiver* pRecipient );
	void									OnAsyncMap( ITextureInternal* pTexToMap, IAsyncTextureOperationReceiver* pRecipient, void* pExtraArgs );
	void									OnAsyncUnmap( ITextureInternal* pTexToUnmap );
	void									OnAsyncCopyRenderTargetToStagingTexture( ITexture* pDst, ITexture* pSrc, IAsyncTextureOperationReceiver* pRecipient );

protected:
	enum MatrixStackFlags_t
	{
		MSF_DIRTY		= ( 1 << 0 ),
		MSF_IDENTITY	= ( 1 << 1 ),
	};

	struct MatrixStackItem_t
	{
		VMatrix matrix;
		unsigned flags;
	};

	struct RenderTargetStackElement_t
	{
		// The render target
		ITexture *m_pRenderTargets[MAX_RENDER_TARGETS];

		// Optional depth texture (used for shadow mapping)
		ITexture *m_pDepthTexture;

		// Viewport dimensions
		int m_nViewX;
		int m_nViewY;
		int m_nViewW;
		int m_nViewH;
	};

	struct PlaneStackElement
	{
		float fValues[4];
		bool bHack_IsHeightClipPlane; //used to hack in compatibility between the user clip planes and the existing height clip plane code
		//I'm doing the hack this way to retain modder's flexibility to mess with clip plane ordering so that they can make special hacks on their side if necessary
		PlaneStackElement( void ) : bHack_IsHeightClipPlane( false ) { };
	};

protected:
	CMatRenderContextBase();
	virtual void CommitRenderTargetAndViewport( void ) {}
	void RecomputeViewState();
	void RecomputeViewProjState();
	void CurrentMatrixChanged();
	virtual void OnRenderDataUnreferenced() {}

protected:
	IMaterialInternal *					m_pCurrentMaterial;
	void*								m_pCurrentProxyData;

	// The lightmap page
	int									m_lightmapPageID;
	ITextureInternal *					m_pUserDefinedLightmap;

	ITexture *							m_pLocalCubemapTexture;
	
	ITexture *							m_pCurrentFrameBufferCopyTexture[MAX_FB_TEXTURES];

	MaterialHeightClipMode_t			m_HeightClipMode;
	float								m_HeightClipZ;
	// The currently bound morph target
	IMorphInternal *					m_pBoundMorph;
	IMorphMgrRenderContext				*m_pMorphRenderContext;

	// Intially a stack of 32 elements allocated, growing by 16 on overflows
	CUtlStack< RenderTargetStackElement_t >	m_RenderTargetStack;

	MaterialMatrixMode_t				m_MatrixMode;
	MatrixStackItem_t *					m_pCurMatrixItem;
	CUtlStack<MatrixStackItem_t>		m_MatrixStacks[NUM_MATRIX_MODES];

	// View state
	VMatrix								m_viewProjMatrix;
	Vector								m_vecViewOrigin;
	Vector								m_vecViewForward;
	Vector								m_vecViewUp;
	Vector								m_vecViewRight;

	float								m_FrameTime;

	Vector								m_LastSetToneMapScale;							   	
	float								m_CurToneMapScale;									// last scale for chasing purposes
	float								m_GoalToneMapScale;
	ShaderViewport_t					m_Viewport;
	CMaterialSystem *					m_pMaterialSystem;

	static CMemoryStack					sm_RenderData[2];
	static int							sm_nRenderLockCount;
	static int							sm_nRenderStack;
	static int							sm_nInitializeCount;

	bool								m_bFlashlightEnable : 1;
	bool								m_bDirtyViewState : 1;
	bool								m_bDirtyViewProjState : 1;
	bool								m_bEnableClipping : 1;
	bool								m_bFullFrameDepthIsValid : 1;
};

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------

class CMatRenderContext : public CMatRenderContextBase
{
	typedef CMatRenderContextBase BaseClass;

public:
	CMatRenderContext();

	InitReturnVal_t							Init( CMaterialSystem *pMaterialSystem );
	void									Shutdown();

	void									BeginRender();
	void									EndRender();

	void									Flush( bool flushHardware = false );

	void									SwapBuffers();

	void									OnReleaseShaderObjects();

	// Set the current texture that is a copy of the framebuffer.
	void									SetFrameBufferCopyTexture( ITexture *pTexture, int textureIndex );

	IMesh *									CreateStaticMesh( VertexFormat_t vertexFormat, const char *pTextureBudgetGroup, IMaterial * pMaterial = NULL );
	DELEGATE_TO_OBJECT_1V(					DestroyStaticMesh, IMesh *, g_pShaderDevice );
	IMesh *									GetDynamicMesh( bool buffered, IMesh* pVertexOverride = 0, IMesh* pIndexOverride = 0, IMaterial *pAutoBind = 0 );
	virtual IMesh*							GetDynamicMeshEx( VertexFormat_t vertexFormat, bool bBuffered = true, IMesh* pVertexOverride = 0, IMesh* pIndexOverride = 0, IMaterial *pAutoBind = 0 );
	DELEGATE_TO_OBJECT_0( IMesh *,			GetFlexMesh, g_pShaderAPI );

// ------------ New Vertex/Index Buffer interface ----------------------------
	IVertexBuffer *CreateStaticVertexBuffer( VertexFormat_t fmt, int nVertexCount, const char *pBudgetGroup )
	{
		return g_pShaderDevice->CreateVertexBuffer( SHADER_BUFFER_TYPE_STATIC, fmt, nVertexCount, pBudgetGroup );
	}

	IIndexBuffer *CreateStaticIndexBuffer( MaterialIndexFormat_t fmt, int nIndexCount, const char *pBudgetGroup )
	{
		return g_pShaderDevice->CreateIndexBuffer( SHADER_BUFFER_TYPE_STATIC, fmt, nIndexCount, pBudgetGroup );
	}

	DELEGATE_TO_OBJECT_1V(					DestroyVertexBuffer, IVertexBuffer *, g_pShaderDevice );
	DELEGATE_TO_OBJECT_1V(					DestroyIndexBuffer, IIndexBuffer *, g_pShaderDevice );
	DELEGATE_TO_OBJECT_3( IVertexBuffer *, 	GetDynamicVertexBuffer, int, VertexFormat_t, bool, g_pShaderDevice );
	DELEGATE_TO_OBJECT_2( IIndexBuffer *, 	GetDynamicIndexBuffer, MaterialIndexFormat_t, bool, g_pShaderDevice );
	DELEGATE_TO_OBJECT_7V(					BindVertexBuffer, int, IVertexBuffer *, int, int, int, VertexFormat_t, int, g_pShaderAPI );
	DELEGATE_TO_OBJECT_2V(					BindIndexBuffer, IIndexBuffer *, int, g_pShaderAPI );
	DELEGATE_TO_OBJECT_3V(					Draw,  MaterialPrimitiveType_t, int, int, g_pShaderAPI );
// ------------ End ----------------------------

	void									BindLocalCubemap( ITexture *pTexture );
	ITexture *								GetLocalCubemap( void );
	void									SetRenderTargetEx( int nRenderTargetID, ITexture *pTexture );
	void									GetRenderTargetDimensions( int &width, int &height) const;

	// matrix api
	void									MatrixMode( MaterialMatrixMode_t);
	void									PushMatrix();
	void									PopMatrix();
	void									LoadMatrix( const VMatrix& matrix );
	void									LoadMatrix( const matrix3x4_t& matrix );
	void									MultMatrix( const VMatrix& matrix );
	void									MultMatrixLocal( const VMatrix& matrix );
	void									MultMatrix( const matrix3x4_t& matrix );
	void									MultMatrixLocal( const matrix3x4_t& matrix );
	void									GetMatrix( MaterialMatrixMode_t matrixMode, VMatrix *pMatrix );
	void									GetMatrix( MaterialMatrixMode_t matrixMode, matrix3x4_t *pMatrix );
	void									LoadIdentity();
	void									Ortho( double, double, double, double, double, double);
	void									PerspectiveX( double, double, double, double);
	void									PerspectiveOffCenterX( double, double, double, double, double, double, double, double );
	void									PickMatrix( int, int, int, int);
	void									Rotate( float, float, float, float);
	void									Translate( float, float, float);
	void									Scale( float, float, float);
	// end matrix api

	void									SyncMatrices();
	void									SyncMatrix( MaterialMatrixMode_t );
	bool									TestMatrixSync( MaterialMatrixMode_t );
	void									ForceSyncMatrix( MaterialMatrixMode_t );

	// Allows us to override the depth buffer setting of a material
	DELEGATE_TO_OBJECT_2V(					OverrideDepthEnable, bool, bool, g_pShaderAPI );
	DELEGATE_TO_OBJECT_2V(					OverrideAlphaWriteEnable, bool, bool, g_pShaderAPI );
	DELEGATE_TO_OBJECT_2V(					OverrideColorWriteEnable, bool, bool, g_pShaderAPI );

	// FIXME: This is a hack required for NVidia/XBox, can they fix in drivers?
	void									DrawScreenSpaceQuad( IMaterial* pMaterial );

	int										CompareMaterialCombos( IMaterial *pMaterial1, IMaterial *pMaterial2, int lightMapID1, int lightMapID2 );

	void									Bind( IMaterial *material, void *proxyData = NULL );
	void									BindLightmapPage( int lightmapPageID );

	DELEGATE_TO_OBJECT_2V(					SetLight, int, const LightDesc_t &, g_pShaderAPI );
	DELEGATE_TO_OBJECT_1V(					SetLightingOrigin, Vector, g_pShaderAPI );

	DELEGATE_TO_OBJECT_3V(					SetAmbientLight, float, float, float, g_pShaderAPI );
	DELEGATE_TO_OBJECT_1V(					SetAmbientLightCube, LightCube_t, g_pShaderAPI );
	DELEGATE_TO_OBJECT_0V(					DisableAllLocalLights, g_pShaderAPI );

	void									CopyRenderTargetToTexture( ITexture *pTexture );

	void									ClearBuffers( bool bClearColor, bool bClearDepth, bool bClearStencil );
	DELEGATE_TO_OBJECT_3V(					ClearColor3ub, unsigned char, unsigned char, unsigned char, g_pShaderAPI );
	DELEGATE_TO_OBJECT_4V(					ClearColor4ub, unsigned char, unsigned char, unsigned char, unsigned char, g_pShaderAPI );
	DELEGATE_TO_OBJECT_2V(					ClearBuffersObeyStencil, bool, bool, g_pShaderAPI );
	DELEGATE_TO_OBJECT_3V(					ClearBuffersObeyStencilEx, bool, bool, bool, g_pShaderAPI );
	DELEGATE_TO_OBJECT_0V(					PerformFullScreenStencilOperation, g_pShaderAPI );

	// read to a unsigned char rgb image.
	DELEGATE_TO_OBJECT_6V(					ReadPixels, int, int, int, int, unsigned char *, ImageFormat, g_pShaderAPI );
	void									ReadPixelsAndStretch( Rect_t *pSrcRect, Rect_t *pDstRect, unsigned char *pBuffer, ImageFormat dstFormat, int nDstStride ) { g_pShaderAPI->ReadPixels( pSrcRect, pDstRect, pBuffer, dstFormat, nDstStride ); }

	// Gets/sets viewport
	void									Viewport( int x, int y, int width, int height );
	void									GetViewport( int& x, int& y, int& width, int& height ) const;
	virtual void							DepthRange( float zNear, float zFar );

	// Selection mode methods
	DELEGATE_TO_OBJECT_1( int,				SelectionMode, bool, g_pShaderAPI );
	DELEGATE_TO_OBJECT_2V(					SelectionBuffer, unsigned int *, int, g_pShaderAPI );
	DELEGATE_TO_OBJECT_0V(					ClearSelectionNames, g_pShaderAPI );
	DELEGATE_TO_OBJECT_1V(					LoadSelectionName, int, g_pShaderAPI );
	DELEGATE_TO_OBJECT_1V(					PushSelectionName, int, g_pShaderAPI );
	DELEGATE_TO_OBJECT_0V(					PopSelectionName, g_pShaderAPI );

	// Sets the cull mode
	DELEGATE_TO_OBJECT_1V(					CullMode, MaterialCullMode_t, g_pShaderAPI );

	// Sets the number of bones for skinning
	DELEGATE_TO_OBJECT_1V(					SetNumBoneWeights, int, g_pShaderAPI );

	void									LoadBoneMatrix( int boneIndex, const matrix3x4_t& matrix );
	DELEGATE_TO_OBJECT_3V(					SetFlexWeights, int, int, const MorphWeight_t*, g_pShaderAPI );

	// Fog-related methods
	void									FogMode( MaterialFogMode_t fogMode )								{ g_pShaderAPI->SceneFogMode( fogMode ); }
	DELEGATE_TO_OBJECT_1V(					FogStart, float, g_pShaderAPI );
	DELEGATE_TO_OBJECT_1V(					FogEnd, float, g_pShaderAPI );
	DELEGATE_TO_OBJECT_1V(					SetFogZ, float, g_pShaderAPI );
	void									FogColor3f( float r, float g, float b );
	void									FogColor3fv( const float* rgb );
	void									FogColor3ub( unsigned char r, unsigned char g, unsigned char b )	{	g_pShaderAPI->SceneFogColor3ub( r, g, b ); }
	void									FogColor3ubv( unsigned char const* rgb );
	void									FogMaxDensity( float flMaxDensity );

	MaterialFogMode_t						GetFogMode( void )													{ return g_pShaderAPI->GetSceneFogMode(); }
	void									GetFogColor( unsigned char *rgb )									{ g_pShaderAPI->GetSceneFogColor( rgb ); }
	DELEGATE_TO_OBJECT_3V(					GetFogDistances, float *, float *, float *, g_pShaderAPI );
	int										GetCurrentNumBones( ) const											{ return g_pShaderAPI->GetCurrentNumBones(); }	

	// Bind standard textures
	void									BindStandardTexture( Sampler_t sampler, StandardTextureId_t id );
	virtual void							BindStandardVertexTexture( VertexTextureSampler_t sampler, StandardTextureId_t id );
	virtual void							GetStandardTextureDimensions( int *pWidth, int *pHeight, StandardTextureId_t id );

	// By default, the material system applies the VIEW and PROJECTION matrices	to the user clip
	// planes (which are specified in world space) to generate projection-space user clip planes
	// Occasionally (for the particle system in hl2, for example), we want to override that
	// behavior and explictly specify a ViewProj transform for user clip planes
	DELEGATE_TO_OBJECT_1V(					EnableUserClipTransformOverride, bool, g_pShaderAPI );
	DELEGATE_TO_OBJECT_1V(					UserClipTransform, const VMatrix &, g_pShaderAPI );

	// Gets the window size
	DELEGATE_TO_OBJECT_2VC(					GetWindowSize, int &, int &, g_pShaderDevice );

	// Methods related to occlusion query
	OcclusionQueryObjectHandle_t			CreateOcclusionQueryObject();
	DELEGATE_TO_OBJECT_1V(					DestroyOcclusionQueryObject, OcclusionQueryObjectHandle_t, g_pOcclusionQueryMgr );
	DELEGATE_TO_OBJECT_1V(					BeginOcclusionQueryDrawing, OcclusionQueryObjectHandle_t, g_pOcclusionQueryMgr );
	DELEGATE_TO_OBJECT_1V(					EndOcclusionQueryDrawing, OcclusionQueryObjectHandle_t, g_pOcclusionQueryMgr );
	DELEGATE_TO_OBJECT_1V(					ResetOcclusionQueryObject, OcclusionQueryObjectHandle_t, g_pOcclusionQueryMgr );
	int										OcclusionQuery_GetNumPixelsRendered( OcclusionQueryObjectHandle_t h );

	bool									InFlashlightMode() const;
	void									SetFlashlightMode( bool bEnable );
	bool									GetFlashlightMode( ) const;
	void									SetFlashlightState( const FlashlightState_t &state, const VMatrix &worldToTexture );
	void									SetFlashlightStateEx( const FlashlightState_t &state, const VMatrix &worldToTexture, ITexture *pFlashlightDepthTexture );

	void									SetScissorRect( const int nLeft, const int nTop, const int nRight, const int nBottom, const bool bEnableScissor );

	// Creates/destroys morph data associated w/ a particular material
	IMorph *								CreateMorph( MorphFormat_t format, const char *pDebugName );
	void									DestroyMorph( IMorph *pMorph );
	void									BindMorph( IMorph *pMorph );
	// Gets the bound morph's vertex format; returns 0 if no morph is bound
	MorphFormat_t							GetBoundMorphFormat();

	void									DrawScreenSpaceRectangle( IMaterial *pMaterial,
																		int destx, int desty,
																		int width, int height,
																		float src_texture_x0, float src_texture_y0,			// which texel you want to appear at
																															// destx/y
																		float src_texture_x1, float src_texture_y1,			// which texel you want to appear at
																															// destx+width-1, desty+height-1
																		int src_texture_width, int src_texture_height,		// needed for fixup
																		void *pClientRenderable = NULL,
																		int nXDice = 1,
																		int nYDice = 1 );

	// custom clip planes, beware that only the most recently pushed plane will actually be used in some hardware configurations
	void									PushCustomClipPlane( const float *fPlane );
	void									PopCustomClipPlane( void );
	void									ApplyCustomClipPlanes( void ); //updates the clip planes based on how many are supported by the hardware using the top of the stack first, at the end of the stack, the height clip plane will be evaluated
	
	// Force writes only when z matches. . . useful for stenciling things out
	// by rendering the desired Z values ahead of time.
	DELEGATE_TO_OBJECT_1V(					ForceDepthFuncEquals, bool, g_pShaderAPI );

	// Two methods of building lightmap bits from floating point src data

	// What are the lightmap dimensions?
	void									GetLightmapDimensions( int *w, int *h );

	void									DrawClearBufferQuad( unsigned char r, unsigned char g, unsigned char b, unsigned char a, bool bClearColor, bool bClearAlpha, bool bClearDepth );

	void									UpdateHeightClipUserClipPlane( void );

	// Private routine called by render target stack push and pop routines to commit new top of stack to the device
	void									CommitRenderTargetAndViewport( void );

	void									SyncToken( const char *pToken );

	void									BindLightmapTexture( ITexture *pLightmapTexture );
	void									CopyRenderTargetToTextureEx( ITexture *pTexture, int nRenderTargetID, Rect_t *pSrcRect, Rect_t *pDstRect = NULL );
	void									CopyTextureToRenderTargetEx( int nRenderTargetID, ITexture *pTexture, Rect_t *pSrcRect, Rect_t *pDstRect = NULL );
	
	DELEGATE_TO_OBJECT_2V(					SetFloatRenderingParameter, int, float, g_pShaderAPI );
	DELEGATE_TO_OBJECT_2V(					SetIntRenderingParameter, int, int, g_pShaderAPI );
	DELEGATE_TO_OBJECT_2V(					SetVectorRenderingParameter, int, const Vector &, g_pShaderAPI );
	DELEGATE_TO_OBJECT_1C( float,			GetFloatRenderingParameter, int, g_pShaderAPI );
	DELEGATE_TO_OBJECT_1C( int,				GetIntRenderingParameter, int, g_pShaderAPI );
	DELEGATE_TO_OBJECT_1C( Vector,			GetVectorRenderingParameter, int, g_pShaderAPI );

	DELEGATE_TO_OBJECT_4V(					GetMaxToRender, IMesh *, bool, int *, int *, g_pShaderAPI );
	DELEGATE_TO_OBJECT_1( int,				GetMaxVerticesToRender, IMaterial *, g_pShaderAPI );
	DELEGATE_TO_OBJECT_0( int,				GetMaxIndicesToRender, g_pShaderAPI );
	DELEGATE_TO_OBJECT_1V(					SetStencilEnable, bool, g_pShaderAPI );
	DELEGATE_TO_OBJECT_1V(					SetStencilFailOperation, StencilOperation_t, g_pShaderAPI );
	DELEGATE_TO_OBJECT_1V(					SetStencilZFailOperation, StencilOperation_t, g_pShaderAPI );
	DELEGATE_TO_OBJECT_1V(					SetStencilPassOperation, StencilOperation_t, g_pShaderAPI );
	DELEGATE_TO_OBJECT_1V(					SetStencilCompareFunction, StencilComparisonFunction_t, g_pShaderAPI );
	DELEGATE_TO_OBJECT_1V(					SetStencilReferenceValue, int, g_pShaderAPI );
	DELEGATE_TO_OBJECT_1V(					SetStencilTestMask, uint32, g_pShaderAPI );
	DELEGATE_TO_OBJECT_1V(					SetStencilWriteMask, uint32, g_pShaderAPI );
	DELEGATE_TO_OBJECT_5V(					ClearStencilBufferRectangle, int, int, int, int, int, g_pShaderAPI );

	// PIX support
	DELEGATE_TO_OBJECT_2V(					BeginPIXEvent, unsigned long, const char *, g_pShaderAPI );
	DELEGATE_TO_OBJECT_0V(					EndPIXEvent, g_pShaderAPI );
	DELEGATE_TO_OBJECT_2V(					SetPIXMarker, unsigned long, const char *, g_pShaderAPI );

	DELEGATE_TO_OBJECT_2V(					SetShadowDepthBiasFactors, float, float, g_pShaderAPI );

	void									BeginBatch( IMesh* pIndices );
	void									BindBatch( IMesh* pVertices, IMaterial *pAutoBind = NULL );
	void									DrawBatch(int firstIndex, int numIndices );
	void									EndBatch();

	void									SetToneMappingScaleLinear( const Vector &scale );

	bool									OnDrawMesh( IMesh *pMesh, int firstIndex, int numIndices );
	bool									OnDrawMesh( IMesh *pMesh, CPrimList *pLists, int nLists );
	bool									OnSetFlexMesh( IMesh *pStaticMesh, IMesh *pMesh, int nVertexOffsetInBytes ) { return true; }
	bool									OnSetColorMesh( IMesh *pStaticMesh, IMesh *pMesh, int nVertexOffsetInBytes ) { return true; }
	bool									OnSetPrimitiveType( IMesh *pMesh, MaterialPrimitiveType_t type ) { return true; }
	bool									OnFlushBufferedPrimitives() { SyncMatrices(); return true; }

	CMaterialSystem							*GetMaterialSystem() const;

	DELEGATE_TO_OBJECT_0V(					ForceHardwareSync, g_pShaderAPI );
	DELEGATE_TO_OBJECT_0V(					BeginFrame, g_pShaderAPI );
	DELEGATE_TO_OBJECT_0V(					EndFrame, g_pShaderAPI );

	virtual void							AsyncCreateTextureFromRenderTarget( ITexture* pSrcRt, const char* pDstName, ImageFormat dstFmt, bool bGenMips, int nAdditionalCreationFlags, IAsyncTextureOperationReceiver* pRecipient, void* pExtraArgs );

	virtual void							AsyncMap( ITextureInternal* pTexToMap, IAsyncTextureOperationReceiver* pRecipient, void* pExtraArgs ) OVERRIDE;
	virtual void							AsyncUnmap( ITextureInternal* pTexToUnmap ) OVERRIDE;
	virtual void							AsyncCopyRenderTargetToStagingTexture( ITexture* pDst, ITexture* pSrc, IAsyncTextureOperationReceiver* pRecipient, void* pExtraArgs ) OVERRIDE;



	virtual void							BeginMorphAccumulation();
	virtual void							EndMorphAccumulation();
	virtual void							AccumulateMorph( IMorph* pMorph, int nMorphCount, const MorphWeight_t* pWeights );
	virtual bool							GetMorphAccumulatorTexCoord( Vector2D *pTexCoord, IMorph *pMorph, int nVertex );

	DELEGATE_TO_OBJECT_1V(                  PushDeformation, const DeformationBase_t *, g_pShaderAPI );
	DELEGATE_TO_OBJECT_0V(                  PopDeformation, g_pShaderAPI );
	DELEGATE_TO_OBJECT_0C( int,             GetNumActiveDeformations, g_pShaderAPI );

	// Color correction related methods..
	DELEGATE_TO_OBJECT_1V(					EnableColorCorrection, bool, g_pColorCorrectionSystem );
	DELEGATE_TO_OBJECT_1( ColorCorrectionHandle_t, AddLookup, const char *, g_pColorCorrectionSystem );
	DELEGATE_TO_OBJECT_1( bool,				RemoveLookup, ColorCorrectionHandle_t, g_pColorCorrectionSystem );
	DELEGATE_TO_OBJECT_1V(					LockLookup, ColorCorrectionHandle_t, g_pColorCorrectionSystem );
	DELEGATE_TO_OBJECT_2V(					LoadLookup, ColorCorrectionHandle_t, const char *, g_pColorCorrectionSystem );
	DELEGATE_TO_OBJECT_1V(					UnlockLookup, ColorCorrectionHandle_t, g_pColorCorrectionSystem );
	DELEGATE_TO_OBJECT_2V(					SetLookupWeight, ColorCorrectionHandle_t, float, g_pColorCorrectionSystem );
	DELEGATE_TO_OBJECT_0V(					ResetLookupWeights, g_pColorCorrectionSystem );
	DELEGATE_TO_OBJECT_2V(					SetResetable, ColorCorrectionHandle_t, bool, g_pColorCorrectionSystem );

	virtual void							SetFullScreenDepthTextureValidityFlag( bool bIsValid );

#if defined( _X360 )
	DELEGATE_TO_OBJECT_1V(                  PushVertexShaderGPRAllocation, int, g_pShaderAPI );
	DELEGATE_TO_OBJECT_0V(                  PopVertexShaderGPRAllocation, g_pShaderAPI );
#endif

	// A special path used to tick the front buffer while loading on the 360
	virtual void							SetNonInteractivePacifierTexture( ITexture *pTexture, float flNormalizedX, float flNormalizedY, float flNormalizedSize );
	virtual void							SetNonInteractiveTempFullscreenBuffer( ITexture *pTexture, MaterialNonInteractiveMode_t mode );
	virtual void							EnableNonInteractiveMode( MaterialNonInteractiveMode_t mode );
	virtual void			                RefreshFrontBufferNonInteractive();

	// debug logging
	virtual void							PrintfVA( char *fmt, va_list vargs );
	virtual void							Printf( PRINTF_FORMAT_STRING const char *fmt, ... );
	virtual float							Knob( char *knobname, float *setvalue=NULL );

#ifdef DX_TO_GL_ABSTRACTION
	void									DoStartupShaderPreloading( void );
#endif

	virtual void							TextureManagerUpdate();

	//---------------------------------------------------------
protected:
	
	IMaterialInternal *GetMaterialInternal( MaterialHandle_t ) const;
	IMaterialInternal *GetDrawFlatMaterial();
	IMaterialInternal *GetRenderTargetBlitMaterial();
	IMaterialInternal *GetBufferClearObeyStencil( int i );

	ShaderAPITextureHandle_t GetFullbrightLightmapTextureHandle() const;
	ShaderAPITextureHandle_t GetFullbrightBumpedLightmapTextureHandle() const;
	ShaderAPITextureHandle_t GetBlackTextureHandle() const;
	ShaderAPITextureHandle_t GetFlatNormalTextureHandle() const;
	ShaderAPITextureHandle_t GetGreyTextureHandle() const;
	ShaderAPITextureHandle_t GetGreyAlphaZeroTextureHandle() const;
	ShaderAPITextureHandle_t GetWhiteTextureHandle() const;
	ShaderAPITextureHandle_t GetFrameBufferTextureHandle() const;
	ShaderAPITextureHandle_t GetMaxDepthTextureHandle() const;

	// Helper methods
	void BindLightmap( Sampler_t stage );
	void BindBumpLightmap( Sampler_t stage );
	void BindFullbrightLightmap( Sampler_t stage );
	void BindBumpedFullbrightLightmap( Sampler_t stage );

	virtual void OnRenderDataUnreferenced();

	const CMatLightmaps *GetLightmaps() const;
	CMatLightmaps *GetLightmaps();

	CUtlVector<PlaneStackElement>		m_CustomClipPlanes; //implemented as a vector so we can remove in special ways

	IMesh *m_pBatchIndices;
	IMesh *m_pBatchMesh;
	IIndexBuffer *m_pCurrentIndexBuffer;
	CTextureReference m_pNonInteractiveTempFullscreenBuffer[MATERIAL_NON_INTERACTIVE_MODE_COUNT];
	CTextureReference m_pNonInteractivePacifier;
	MaterialNonInteractiveMode_t m_NonInteractiveMode;
	float m_flNormalizedX;
	float m_flNormalizedY;
	float m_flNormalizedSize;
};


//-----------------------------------------------------------------------------

inline void CMatRenderContext::LoadBoneMatrix( int boneIndex, const matrix3x4_t& matrix )
{
	g_pShaderAPI->LoadBoneMatrix( boneIndex, matrix.Base() );
}

//-----------------------------------------------------------------------------


inline bool CMatRenderContext::InFlashlightMode() const
{
	return m_bFlashlightEnable;
}

inline void CMatRenderContext::SetFlashlightState( const FlashlightState_t &state, const VMatrix &worldToTexture )
{
	SetFlashlightStateEx( state, worldToTexture, NULL );
}

inline float CMatRenderContextBase::ComputePixelWidthOfSphere( const Vector& vecOrigin, float flRadius )
{
	return ComputePixelDiameterOfSphere( vecOrigin, flRadius ) * 2.0f;
}

inline void CMatRenderContext::FogColor3ubv( unsigned char const* rgb )
{
	g_pShaderAPI->SceneFogColor3ub( rgb[0], rgb[1], rgb[2] );
}

inline void CMatRenderContext::FogMaxDensity( float flMaxDensity )
{
	g_pShaderAPI->FogMaxDensity( flMaxDensity );
}

inline ITexture *CMatRenderContext::GetLocalCubemap( void )
{
	return m_pLocalCubemapTexture;
}

inline void CMatRenderContextBase::SetRenderTarget( ITexture *pTexture_ )
{
	SetRenderTargetEx( 0, pTexture_ );
}

//-----------------------------------------------------------------------------
// Gets the bound morph's vertex format; returns 0 if no morph is bound
//-----------------------------------------------------------------------------
inline MorphFormat_t CMatRenderContext::GetBoundMorphFormat()
{
	return m_pBoundMorph ? m_pBoundMorph->GetMorphFormat() : 0;
}

//-----------------------------------------------------------------------------
// Use this to create static vertex and index buffers 
//-----------------------------------------------------------------------------
inline IMesh* CMatRenderContext::CreateStaticMesh( VertexFormat_t vertexFormat, const char *pTextureBudgetGroup, IMaterial * pMaterial )
{
	return g_pShaderDevice->CreateStaticMesh( vertexFormat, pTextureBudgetGroup, pMaterial );
}

inline void CMatRenderContext::SyncToken( const char *pToken )
{
	if ( g_pShaderAPI )
	{
		g_pShaderAPI->SyncToken( pToken );
	}
}

inline enum MaterialHeightClipMode_t CMatRenderContextBase::GetHeightClipMode( void )
{
	return m_HeightClipMode;
}

inline int CMatRenderContextBase::GetLightmapPage( void )
{
	return m_lightmapPageID;
}

inline CMaterialSystem *CMatRenderContext::GetMaterialSystem() const
{
	return m_pMaterialSystem;
}

//-----------------------------------------------------------------------------

#endif // CMATERIALRENDERSTATE_H
