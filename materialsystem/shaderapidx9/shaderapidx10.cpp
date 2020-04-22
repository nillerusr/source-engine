//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#include "shaderapidx10.h"
#include "shaderapibase.h"
#include "shaderapi/ishaderutil.h"
#include "materialsystem/idebugtextureinfo.h"
#include "materialsystem/materialsystem_config.h"
#include "meshdx10.h"
#include "shadershadowdx10.h"
#include "shaderdevicedx10.h"
#include "shaderapidx10_global.h"
#include "imaterialinternal.h"


//-----------------------------------------------------------------------------
// Methods related to queuing functions to be called prior to rendering
//-----------------------------------------------------------------------------
CFunctionCommit::CFunctionCommit()
{
	m_pCommitFlags = NULL;
	m_nCommitBufferSize = 0;
}

CFunctionCommit::~CFunctionCommit()
{
	if ( m_pCommitFlags )
	{
		delete[] m_pCommitFlags;
		m_pCommitFlags = NULL;
	}
}

void CFunctionCommit::Init( int nFunctionCount )
{
	m_nCommitBufferSize = ( nFunctionCount + 7 ) >> 3;
	Assert( !m_pCommitFlags );
	m_pCommitFlags = new unsigned char[ m_nCommitBufferSize ];
	memset( m_pCommitFlags, 0, m_nCommitBufferSize );
}


//-----------------------------------------------------------------------------
// Methods related to queuing functions to be called per-(pMesh->Draw call) or per-pass
//-----------------------------------------------------------------------------
inline bool CFunctionCommit::IsCommitFuncInUse( int nFunc ) const
{
	Assert( nFunc >> 3 < m_nCommitBufferSize );
	return ( m_pCommitFlags[ nFunc >> 3 ] & ( 1 << ( nFunc & 0x7 ) ) ) != 0;
}

inline void CFunctionCommit::MarkCommitFuncInUse( int nFunc )
{
	Assert( nFunc >> 3 < m_nCommitBufferSize );
	m_pCommitFlags[ nFunc >> 3 ] |= 1 << ( nFunc & 0x7 );
}

inline void CFunctionCommit::AddCommitFunc( StateCommitFunc_t f )
{
	m_CommitFuncs.AddToTail( f );
}


//-----------------------------------------------------------------------------
// Clears all commit functions
//-----------------------------------------------------------------------------
inline void CFunctionCommit::ClearAllCommitFuncs( )
{
	memset( m_pCommitFlags, 0, m_nCommitBufferSize );
	m_CommitFuncs.RemoveAll();
}


//-----------------------------------------------------------------------------
// Calls all commit functions in a particular list
//-----------------------------------------------------------------------------
void CFunctionCommit::CallCommitFuncs( ID3D10Device *pDevice, const ShaderStateDx10_t &desiredState, ShaderStateDx10_t &currentState, bool bForce )
{
	int nCount = m_CommitFuncs.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		m_CommitFuncs[i]( pDevice, desiredState, currentState, bForce );
	}

	ClearAllCommitFuncs( );
}


//-----------------------------------------------------------------------------
// Helpers for commit functions
//-----------------------------------------------------------------------------
#define ADD_COMMIT_FUNC( _func_name )	\
	if ( !m_Commit.IsCommitFuncInUse( COMMIT_FUNC_ ## _func_name ) )	\
	{																	\
		m_Commit.AddCommitFunc( _func_name );							\
		m_Commit.MarkCommitFuncInUse( COMMIT_FUNC_ ## _func_name );		\
	}

#define ADD_RENDERSTATE_FUNC( _func_name, _state, _val )					\
	if ( m_bResettingRenderState || ( m_DesiredState. ## _state != _val ) )	\
	{																		\
		m_DesiredState. ## _state = _val;									\
		ADD_COMMIT_FUNC( _func_name )										\
	}

#define IMPLEMENT_RENDERSTATE_FUNC( _func_name, _state, _d3dFunc )			\
	static void _func_name( ID3D10Device *pDevice, const ShaderStateDx10_t &desiredState, ShaderStateDx10_t &currentState, bool bForce )	\
	{																			\
		if ( bForce || ( desiredState. ## _state != currentState. ## _state ) )	\
		{																		\
			pDevice->_d3dFunc( desiredState. ## _state );						\
			currentState. ## _state	= desiredState. ## _state;					\
		}																		\
	}

//-----------------------------------------------------------------------------
// D3D state setting methods
//-----------------------------------------------------------------------------

// NOTE: For each commit func you create, add to this enumeration.
enum CommitFunc_t
{
	COMMIT_FUNC_CommitSetViewports = 0,
	COMMIT_FUNC_CommitSetVertexShader,
	COMMIT_FUNC_CommitSetGeometryShader,
	COMMIT_FUNC_CommitSetPixelShader,
	COMMIT_FUNC_CommitSetVertexBuffer,
	COMMIT_FUNC_CommitSetIndexBuffer,
	COMMIT_FUNC_CommitSetInputLayout,
	COMMIT_FUNC_CommitSetTopology,
	COMMIT_FUNC_CommitSetRasterState,

	COMMIT_FUNC_COUNT,
};

IMPLEMENT_RENDERSTATE_FUNC( CommitSetTopology, m_Topology, IASetPrimitiveTopology )
IMPLEMENT_RENDERSTATE_FUNC( CommitSetVertexShader, m_pVertexShader, VSSetShader )
IMPLEMENT_RENDERSTATE_FUNC( CommitSetGeometryShader, m_pGeometryShader, GSSetShader )
IMPLEMENT_RENDERSTATE_FUNC( CommitSetPixelShader, m_pPixelShader, PSSetShader )

static void CommitSetInputLayout( ID3D10Device *pDevice, const ShaderStateDx10_t &desiredState, ShaderStateDx10_t &currentState, bool bForce )
{
	const ShaderInputLayoutStateDx10_t& newState = desiredState.m_InputLayout;
	if ( bForce || memcmp( &newState, &currentState.m_InputLayout, sizeof(ShaderInputLayoutStateDx10_t) ) )	
	{
		// FIXME: Deal with multiple streams
		ID3D10InputLayout *pInputLayout = g_pShaderDeviceDx10->GetInputLayout( 
			newState.m_hVertexShader, newState.m_pVertexDecl[0] );
		pDevice->IASetInputLayout( pInputLayout );						

		currentState.m_InputLayout = newState;
	}																		
}

static void CommitSetViewports( ID3D10Device *pDevice, const ShaderStateDx10_t &desiredState, ShaderStateDx10_t &currentState, bool bForce )
{
	bool bChanged = bForce || ( desiredState.m_nViewportCount != currentState.m_nViewportCount );
	if ( !bChanged && desiredState.m_nViewportCount > 0 )
	{
		bChanged = memcmp( desiredState.m_pViewports, currentState.m_pViewports, 
			desiredState.m_nViewportCount * sizeof( D3D10_VIEWPORT ) ) != 0;
	}

	if ( !bChanged )
		return;

	pDevice->RSSetViewports( desiredState.m_nViewportCount, desiredState.m_pViewports );
	currentState.m_nViewportCount = desiredState.m_nViewportCount;

#ifdef _DEBUG
	memset( currentState.m_pViewports, 0xDD, sizeof( currentState.m_pViewports ) );
#endif

	memcpy( currentState.m_pViewports, desiredState.m_pViewports, 
		desiredState.m_nViewportCount * sizeof( D3D10_VIEWPORT ) );
}

static void CommitSetIndexBuffer( ID3D10Device *pDevice, const ShaderStateDx10_t &desiredState, ShaderStateDx10_t &currentState, bool bForce )
{
	const ShaderIndexBufferStateDx10_t &newState = desiredState.m_IndexBuffer;
	bool bChanged = bForce || memcmp( &newState, &currentState.m_IndexBuffer, sizeof(ShaderIndexBufferStateDx10_t) );
	if ( !bChanged )
		return;

	pDevice->IASetIndexBuffer( newState.m_pBuffer, newState.m_Format, newState.m_nOffset );
	memcpy( &currentState.m_IndexBuffer, &newState, sizeof( ShaderIndexBufferStateDx10_t ) );
}

static void CommitSetVertexBuffer( ID3D10Device *pDevice, const ShaderStateDx10_t &desiredState, ShaderStateDx10_t &currentState, bool bForce )
{
	ID3D10Buffer *ppVertexBuffers[ MAX_DX10_STREAMS ];
	UINT pStrides[ MAX_DX10_STREAMS ];
	UINT pOffsets[ MAX_DX10_STREAMS ];

	UINT nFirstBuffer = 0;
	UINT nBufferCount = 0;
	bool bInMatch = true;
	for ( int i = 0; i < MAX_DX10_STREAMS; ++i )
	{
		const ShaderVertexBufferStateDx10_t &newState = desiredState.m_pVertexBuffer[i];
		bool bMatch = !bForce && !memcmp( &newState, &currentState.m_pVertexBuffer[i], sizeof(ShaderVertexBufferStateDx10_t) );
		if ( !bMatch )
		{
			ppVertexBuffers[i] = newState.m_pBuffer;
			pStrides[i] = newState.m_nStride;
		    pOffsets[i] = newState.m_nOffset;
			++nBufferCount;
			memcpy( &currentState.m_pVertexBuffer[i], &newState, sizeof( ShaderVertexBufferStateDx10_t ) );
		}

		if ( bInMatch )
		{
			if ( !bMatch )
			{
				bInMatch = false;
				nFirstBuffer = i;
			}
			continue;
		}

		if ( bMatch )
		{
			bInMatch = true;
			pDevice->IASetVertexBuffers( nFirstBuffer, nBufferCount, 
				&ppVertexBuffers[nFirstBuffer], &pStrides[nFirstBuffer], &pOffsets[nFirstBuffer] );
			nBufferCount = 0;
		}
	}

	if ( !bInMatch )
	{
		pDevice->IASetVertexBuffers( nFirstBuffer, nBufferCount, 
			&ppVertexBuffers[nFirstBuffer], &pStrides[nFirstBuffer], &pOffsets[nFirstBuffer] );
	}
}

static void GenerateRasterizerDesc( D3D10_RASTERIZER_DESC* pDesc, const ShaderRasterState_t& state )
{
	pDesc->FillMode = ( state.m_FillMode == SHADER_FILL_WIREFRAME ) ? D3D10_FILL_WIREFRAME : D3D10_FILL_SOLID;
	
	// Cull state
	if ( state.m_bCullEnable )
	{
		pDesc->CullMode = D3D10_CULL_NONE;
	}
	else
	{
		pDesc->CullMode = ( state.m_CullMode == MATERIAL_CULLMODE_CW ) ? D3D10_CULL_BACK : D3D10_CULL_FRONT;
	}
	pDesc->FrontCounterClockwise = TRUE;

	// Depth bias state
	if ( !state.m_bDepthBias )
	{
		pDesc->DepthBias = 0;
		pDesc->DepthBiasClamp = 0.0f;
		pDesc->SlopeScaledDepthBias = 0.0f;
		pDesc->DepthClipEnable = FALSE;
	}
	else
	{
		// FIXME: Implement! Read ConVars
	}

	pDesc->ScissorEnable = state.m_bScissorEnable ? TRUE : FALSE;
	pDesc->MultisampleEnable = state.m_bMultisampleEnable ? TRUE : FALSE;
	pDesc->AntialiasedLineEnable = FALSE;
}

static void CommitSetRasterState( ID3D10Device *pDevice, const ShaderStateDx10_t &desiredState, ShaderStateDx10_t &currentState, bool bForce )
{
	const ShaderRasterState_t& newState = desiredState.m_RasterState;
	if ( bForce || memcmp( &newState, &currentState.m_RasterState, sizeof(ShaderRasterState_t) ) )	
	{
		// Clear out the existing state
		if ( currentState.m_pRasterState )
		{
			currentState.m_pRasterState->Release();
		}

		D3D10_RASTERIZER_DESC desc;
		GenerateRasterizerDesc( &desc, newState );

		// NOTE: This does a search for existing matching state objects
		ID3D10RasterizerState *pState = NULL;
		HRESULT hr = pDevice->CreateRasterizerState( &desc, &pState );
		if ( !FAILED(hr) )
		{
			Warning( "Unable to create rasterizer state object!\n" );
		}

		pDevice->RSSetState( pState );						

		currentState.m_pRasterState = pState;
		memcpy( &currentState.m_RasterState, &newState, sizeof( ShaderRasterState_t ) );
	}																		
}


//-----------------------------------------------------------------------------
//
// Shader API Dx10
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Class Factory
//-----------------------------------------------------------------------------
static CShaderAPIDx10 s_ShaderAPIDx10;
CShaderAPIDx10* g_pShaderAPIDx10 = &s_ShaderAPIDx10;

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CShaderAPIDx10, IShaderAPI, 
								  SHADERAPI_INTERFACE_VERSION, s_ShaderAPIDx10 )

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CShaderAPIDx10, IDebugTextureInfo, 
								  DEBUG_TEXTURE_INFO_VERSION, s_ShaderAPIDx10 )

								  
//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CShaderAPIDx10::CShaderAPIDx10() 
{
	m_bResettingRenderState = false;
	m_Commit.Init( COMMIT_FUNC_COUNT );
	ClearShaderState( &m_DesiredState );
	ClearShaderState( &m_CurrentState );
}

CShaderAPIDx10::~CShaderAPIDx10()
{
}


//-----------------------------------------------------------------------------
// Clears the shader state to a well-defined value
//-----------------------------------------------------------------------------
void CShaderAPIDx10::ClearShaderState( ShaderStateDx10_t* pState )
{
	memset( pState, 0, sizeof( ShaderStateDx10_t ) );
}


//-----------------------------------------------------------------------------
// Resets the render state
//-----------------------------------------------------------------------------
void CShaderAPIDx10::ResetRenderState( bool bFullReset )
{
	D3D10_RASTERIZER_DESC rDesc;
	memset( &rDesc, 0, sizeof(rDesc) );
	rDesc.FillMode = D3D10_FILL_SOLID;
	rDesc.CullMode = D3D10_CULL_NONE;
	rDesc.FrontCounterClockwise = TRUE;	// right-hand rule 

	ID3D10RasterizerState *pRasterizerState;
	HRESULT hr = D3D10Device()->CreateRasterizerState( &rDesc, &pRasterizerState ); 
	Assert( !FAILED(hr) );
	D3D10Device()->RSSetState( pRasterizerState );

	D3D10_DEPTH_STENCIL_DESC dsDesc;
	memset( &dsDesc, 0, sizeof(dsDesc) );

	ID3D10DepthStencilState *pDepthStencilState;
	hr = D3D10Device()->CreateDepthStencilState( &dsDesc, &pDepthStencilState );
	Assert( !FAILED(hr) );
	D3D10Device()->OMSetDepthStencilState( pDepthStencilState, 0 );

	D3D10_BLEND_DESC bDesc;
	memset( &bDesc, 0, sizeof(bDesc) );
	bDesc.SrcBlend = D3D10_BLEND_ONE;
	bDesc.DestBlend = D3D10_BLEND_ZERO;
	bDesc.BlendOp = D3D10_BLEND_OP_ADD;
	bDesc.SrcBlendAlpha = D3D10_BLEND_ONE;
	bDesc.DestBlendAlpha = D3D10_BLEND_ZERO;
	bDesc.BlendOpAlpha = D3D10_BLEND_OP_ADD;
	bDesc.RenderTargetWriteMask[0] = D3D10_COLOR_WRITE_ENABLE_ALL;

	FLOAT pBlendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	ID3D10BlendState *pBlendState;
	hr = D3D10Device()->CreateBlendState( &bDesc, &pBlendState );
	Assert( !FAILED(hr) );
	D3D10Device()->OMSetBlendState( pBlendState, pBlendFactor, 0xFFFFFFFF );
}


//-----------------------------------------------------------------------------
// Commits queued-up state change requests
//-----------------------------------------------------------------------------
void CShaderAPIDx10::CommitStateChanges( bool bForce )
{
	// Don't bother committing anything if we're deactivated
	if ( g_pShaderDevice->IsDeactivated() )
		return;

	m_Commit.CallCommitFuncs( D3D10Device(), m_DesiredState, m_CurrentState, bForce );
}


//-----------------------------------------------------------------------------
// Methods of IShaderDynamicAPI
//-----------------------------------------------------------------------------
void CShaderAPIDx10::GetBackBufferDimensions( int& nWidth, int& nHeight ) const
{
	g_pShaderDeviceDx10->GetBackBufferDimensions( nWidth, nHeight );
} 

	
//-----------------------------------------------------------------------------
// Viewport-related methods
//-----------------------------------------------------------------------------
void CShaderAPIDx10::SetViewports( int nCount, const ShaderViewport_t* pViewports )
{
	nCount = min( nCount, MAX_DX10_VIEWPORTS );
	m_DesiredState.m_nViewportCount = nCount;

	for ( int i = 0; i < nCount; ++i )
	{
		Assert( pViewports[i].m_nVersion == SHADER_VIEWPORT_VERSION );

		D3D10_VIEWPORT& viewport = m_DesiredState.m_pViewports[i];
		viewport.TopLeftX = pViewports[i].m_nTopLeftX;
		viewport.TopLeftY = pViewports[i].m_nTopLeftY;
		viewport.Width = pViewports[i].m_nWidth;
		viewport.Height = pViewports[i].m_nHeight;
		viewport.MinDepth = pViewports[i].m_flMinZ;
		viewport.MaxDepth = pViewports[i].m_flMaxZ;
	}

	ADD_COMMIT_FUNC( CommitSetViewports );
}

int CShaderAPIDx10::GetViewports( ShaderViewport_t* pViewports, int nMax ) const
{
	int nCount = m_DesiredState.m_nViewportCount;
	if ( pViewports && nMax )
	{
		nCount = min( nCount, nMax );
		memcpy( pViewports, m_DesiredState.m_pViewports, nCount * sizeof( ShaderViewport_t ) );
	}
	return nCount;
}


//-----------------------------------------------------------------------------
// Methods related to state objects
//-----------------------------------------------------------------------------
void CShaderAPIDx10::SetRasterState( const ShaderRasterState_t& state )
{
	if ( memcmp( &state, &m_DesiredState.m_RasterState, sizeof(ShaderRasterState_t) ) )
	{
		memcpy( &m_DesiredState.m_RasterState, &state, sizeof(ShaderRasterState_t) );
		ADD_COMMIT_FUNC( CommitSetRasterState );
	}
}


//-----------------------------------------------------------------------------
// Methods related to clearing buffers
//-----------------------------------------------------------------------------
void CShaderAPIDx10::ClearColor3ub( unsigned char r, unsigned char g, unsigned char b )
{
	m_DesiredState.m_ClearColor[0] = r / 255.0f;
	m_DesiredState.m_ClearColor[1] = g / 255.0f;
	m_DesiredState.m_ClearColor[2] = b / 255.0f;
	m_DesiredState.m_ClearColor[3] = 1.0f;
}

void CShaderAPIDx10::ClearColor4ub( unsigned char r, unsigned char g, unsigned char b, unsigned char a )
{
	m_DesiredState.m_ClearColor[0] = r / 255.0f;
	m_DesiredState.m_ClearColor[1] = g / 255.0f;
	m_DesiredState.m_ClearColor[2] = b / 255.0f;
	m_DesiredState.m_ClearColor[3] = a / 255.0f;
}

void CShaderAPIDx10::ClearBuffers( bool bClearColor, bool bClearDepth, bool bClearStencil, int renderTargetWidth, int renderTargetHeight )
{
	// NOTE: State change commit isn't necessary since clearing doesn't use state
//	CommitStateChanges();

	// FIXME: This implementation is totally bust0red [doesn't guarantee exact color specified]
	if ( bClearColor )
	{
		D3D10Device()->ClearRenderTargetView( D3D10RenderTargetView(), m_DesiredState.m_ClearColor ); 
	}
}


//-----------------------------------------------------------------------------
// Methods related to binding shaders
//-----------------------------------------------------------------------------
void CShaderAPIDx10::BindVertexShader( VertexShaderHandle_t hVertexShader )
{
	ID3D10VertexShader *pVertexShader = g_pShaderDeviceDx10->GetVertexShader( hVertexShader );
	ADD_RENDERSTATE_FUNC( CommitSetVertexShader, m_pVertexShader, pVertexShader );

	if ( m_bResettingRenderState || ( m_DesiredState.m_InputLayout.m_hVertexShader != hVertexShader ) )
	{
		m_DesiredState.m_InputLayout.m_hVertexShader = hVertexShader;
		ADD_COMMIT_FUNC( CommitSetInputLayout );
	}
}

void CShaderAPIDx10::BindGeometryShader( GeometryShaderHandle_t hGeometryShader )
{
	ID3D10GeometryShader *pGeometryShader = g_pShaderDeviceDx10->GetGeometryShader( hGeometryShader );
	ADD_RENDERSTATE_FUNC( CommitSetGeometryShader, m_pGeometryShader, pGeometryShader );
}

void CShaderAPIDx10::BindPixelShader( PixelShaderHandle_t hPixelShader )
{
	ID3D10PixelShader *pPixelShader = g_pShaderDeviceDx10->GetPixelShader( hPixelShader );
	ADD_RENDERSTATE_FUNC( CommitSetPixelShader, m_pPixelShader, pPixelShader );
}

void CShaderAPIDx10::BindVertexBuffer( int nStreamID, IVertexBuffer *pVertexBuffer, int nOffsetInBytes, int nFirstVertex, int nVertexCount, VertexFormat_t fmt, int nRepetitions )
{
	// FIXME: What to do about repetitions?
	CVertexBufferDx10 *pVertexBufferDx10 = static_cast<CVertexBufferDx10 *>( pVertexBuffer );

	ShaderVertexBufferStateDx10_t state;
	if ( pVertexBufferDx10 )
	{
		state.m_pBuffer = pVertexBufferDx10->GetDx10Buffer();
		state.m_nStride = pVertexBufferDx10->VertexSize(); 
	}
	else
	{
		state.m_pBuffer = NULL;
		state.m_nStride = 0; 
	}
	state.m_nOffset = nOffsetInBytes;

	if ( m_bResettingRenderState || memcmp( &m_DesiredState.m_pVertexBuffer[ nStreamID ], &state, sizeof( ShaderVertexBufferStateDx10_t ) ) )
	{
		m_DesiredState.m_pVertexBuffer[ nStreamID ] = state;
		ADD_COMMIT_FUNC( CommitSetVertexBuffer );
	}

	if ( m_bResettingRenderState || ( m_DesiredState.m_InputLayout.m_pVertexDecl[ nStreamID ] != fmt ) )
	{
		m_DesiredState.m_InputLayout.m_pVertexDecl[ nStreamID ] = fmt;
		ADD_COMMIT_FUNC( CommitSetInputLayout );
	}
}

void CShaderAPIDx10::BindIndexBuffer( IIndexBuffer *pIndexBuffer, int nOffsetInBytes )
{
	CIndexBufferDx10 *pIndexBufferDx10 = static_cast<CIndexBufferDx10 *>( pIndexBuffer );

	ShaderIndexBufferStateDx10_t state;
	if ( pIndexBufferDx10 )
	{
		state.m_pBuffer = pIndexBufferDx10->GetDx10Buffer();
		state.m_Format = ( pIndexBufferDx10->GetIndexFormat() == MATERIAL_INDEX_FORMAT_16BIT ) ? 
			DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
	}
	else
	{
		state.m_pBuffer = NULL;
		state.m_Format = DXGI_FORMAT_R16_UINT;
	}
	state.m_nOffset = nOffsetInBytes;

	ADD_RENDERSTATE_FUNC( CommitSetIndexBuffer, m_IndexBuffer, state );
}


//-----------------------------------------------------------------------------
// Unbinds resources because they are about to be deleted
//-----------------------------------------------------------------------------
void CShaderAPIDx10::Unbind( VertexShaderHandle_t hShader )
{
	ID3D10VertexShader* pShader = g_pShaderDeviceDx10->GetVertexShader( hShader );
	Assert ( pShader );
	if ( m_DesiredState.m_pVertexShader == pShader )
	{
		BindVertexShader( VERTEX_SHADER_HANDLE_INVALID );
	}
	if ( m_CurrentState.m_pVertexShader == pShader )
	{
		CommitStateChanges();
	}
}

void CShaderAPIDx10::Unbind( GeometryShaderHandle_t hShader )
{
	ID3D10GeometryShader* pShader = g_pShaderDeviceDx10->GetGeometryShader( hShader );
	Assert ( pShader );
	if ( m_DesiredState.m_pGeometryShader == pShader )
	{
		BindGeometryShader( GEOMETRY_SHADER_HANDLE_INVALID );
	}
	if ( m_CurrentState.m_pGeometryShader == pShader )
	{
		CommitStateChanges();
	}
}

void CShaderAPIDx10::Unbind( PixelShaderHandle_t hShader )
{
	ID3D10PixelShader* pShader = g_pShaderDeviceDx10->GetPixelShader( hShader );
	Assert ( pShader );
	if ( m_DesiredState.m_pPixelShader == pShader )
	{
		BindPixelShader( PIXEL_SHADER_HANDLE_INVALID );
	}
	if ( m_CurrentState.m_pPixelShader == pShader )
	{
		CommitStateChanges();
	}
}

void CShaderAPIDx10::UnbindVertexBuffer( ID3D10Buffer *pBuffer )
{
	Assert ( pBuffer );

	for ( int i = 0; i < MAX_DX10_STREAMS; ++i )
	{
		if ( m_DesiredState.m_pVertexBuffer[i].m_pBuffer == pBuffer )
		{
			BindVertexBuffer( i, NULL, 0, 0, 0, VERTEX_POSITION, 0 );
		}
	}
	for ( int i = 0; i < MAX_DX10_STREAMS; ++i )
	{
		if ( m_CurrentState.m_pVertexBuffer[i].m_pBuffer == pBuffer )
		{
			CommitStateChanges();
			break;
		}
	}
}

void CShaderAPIDx10::UnbindIndexBuffer( ID3D10Buffer *pBuffer )
{
	Assert ( pBuffer );

	if ( m_DesiredState.m_IndexBuffer.m_pBuffer == pBuffer )
	{
		BindIndexBuffer( NULL, 0 );
	}
	if ( m_CurrentState.m_IndexBuffer.m_pBuffer == pBuffer )
	{
		CommitStateChanges();
	}
}


//-----------------------------------------------------------------------------
// Sets the topology state
//-----------------------------------------------------------------------------
void CShaderAPIDx10::SetTopology( MaterialPrimitiveType_t topology )
{
	D3D10_PRIMITIVE_TOPOLOGY d3dTopology;
	switch( topology )
	{
	case MATERIAL_POINTS:
		d3dTopology = D3D10_PRIMITIVE_TOPOLOGY_POINTLIST;
		break;

	case MATERIAL_LINES:
		d3dTopology = D3D10_PRIMITIVE_TOPOLOGY_LINELIST;
		break;

	case MATERIAL_TRIANGLES:
		d3dTopology = D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		break;

	case MATERIAL_TRIANGLE_STRIP:
		d3dTopology = D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
		break;

	case MATERIAL_LINE_STRIP:
		d3dTopology = D3D10_PRIMITIVE_TOPOLOGY_LINESTRIP;
		break;

	default:
	case MATERIAL_LINE_LOOP:
	case MATERIAL_POLYGON:
	case MATERIAL_QUADS:
		Assert( 0 );
		d3dTopology = D3D10_PRIMITIVE_TOPOLOGY_UNDEFINED;
		break;
	}

	ADD_RENDERSTATE_FUNC( CommitSetTopology, m_Topology, d3dTopology );
}


//-----------------------------------------------------------------------------
// Main entry point for rendering
//-----------------------------------------------------------------------------
void CShaderAPIDx10::Draw( MaterialPrimitiveType_t primitiveType, int nFirstIndex, int nIndexCount )
{
	SetTopology( primitiveType );

	CommitStateChanges();

	// FIXME: How do I set the base vertex location!?
	D3D10Device()->DrawIndexed( (UINT)nIndexCount, (UINT)nFirstIndex, 0 );
}


//-----------------------------------------------------------------------------
//
// Abandon all hope below this point
//
//-----------------------------------------------------------------------------

bool CShaderAPIDx10::DoRenderTargetsNeedSeparateDepthBuffer() const
{
	return false;
}

// Can we download textures?
bool CShaderAPIDx10::CanDownloadTextures() const
{
	return false;
}

// Used to clear the transition table when we know it's become invalid.
void CShaderAPIDx10::ClearSnapshots()
{
}

// Sets the default *dynamic* state
void CShaderAPIDx10::SetDefaultState()
{
}


// Returns the snapshot id for the shader state
StateSnapshot_t	 CShaderAPIDx10::TakeSnapshot( )
{
	StateSnapshot_t id = 0;
	if (g_pShaderShadowDx10->m_IsTranslucent)
		id |= TRANSLUCENT;
	if (g_pShaderShadowDx10->m_IsAlphaTested)
		id |= ALPHATESTED;
	if (g_pShaderShadowDx10->m_bUsesVertexAndPixelShaders)
		id |= VERTEX_AND_PIXEL_SHADERS;
	if (g_pShaderShadowDx10->m_bIsDepthWriteEnabled)
		id |= DEPTHWRITE;
	return id;
}

// Returns true if the state snapshot is transparent
bool CShaderAPIDx10::IsTranslucent( StateSnapshot_t id ) const
{
	return (id & TRANSLUCENT) != 0; 
}

bool CShaderAPIDx10::IsAlphaTested( StateSnapshot_t id ) const
{
	return (id & ALPHATESTED) != 0; 
}

bool CShaderAPIDx10::IsDepthWriteEnabled( StateSnapshot_t id ) const
{
	return (id & DEPTHWRITE) != 0; 
}

bool CShaderAPIDx10::UsesVertexAndPixelShaders( StateSnapshot_t id ) const
{
	return (id & VERTEX_AND_PIXEL_SHADERS) != 0; 
}

// Gets the vertex format for a set of snapshot ids
VertexFormat_t CShaderAPIDx10::ComputeVertexFormat( int numSnapshots, StateSnapshot_t* pIds ) const
{
	return 0;
}

// Gets the vertex format for a set of snapshot ids
VertexFormat_t CShaderAPIDx10::ComputeVertexUsage( int numSnapshots, StateSnapshot_t* pIds ) const
{
	return 0;
}

// Uses a state snapshot
void CShaderAPIDx10::UseSnapshot( StateSnapshot_t snapshot )
{
}

// Sets the color to modulate by
void CShaderAPIDx10::Color3f( float r, float g, float b )
{
}

void CShaderAPIDx10::Color3fv( float const* pColor )
{
}

void CShaderAPIDx10::Color4f( float r, float g, float b, float a )
{
}

void CShaderAPIDx10::Color4fv( float const* pColor )
{
}

// Faster versions of color
void CShaderAPIDx10::Color3ub( unsigned char r, unsigned char g, unsigned char b )
{
}

void CShaderAPIDx10::Color3ubv( unsigned char const* rgb )
{
}

void CShaderAPIDx10::Color4ub( unsigned char r, unsigned char g, unsigned char b, unsigned char a )
{
}

void CShaderAPIDx10::Color4ubv( unsigned char const* rgba )
{
}

void CShaderAPIDx10::GetStandardTextureDimensions( int *pWidth, int *pHeight, StandardTextureId_t id )
{
	ShaderUtil()->GetStandardTextureDimensions( pWidth, pHeight, id );
}


// The shade mode
void CShaderAPIDx10::ShadeMode( ShaderShadeMode_t mode )
{
}

// Binds a particular material to render with
void CShaderAPIDx10::Bind( IMaterial* pMaterial )
{
}

// Cull mode
void CShaderAPIDx10::CullMode( MaterialCullMode_t cullMode )
{
}

void CShaderAPIDx10::ForceDepthFuncEquals( bool bEnable )
{
}

// Forces Z buffering on or off
void CShaderAPIDx10::OverrideDepthEnable( bool bEnable, bool bDepthEnable )
{
}

void CShaderAPIDx10::OverrideAlphaWriteEnable( bool bOverrideEnable, bool bAlphaWriteEnable )
{
}

void CShaderAPIDx10::OverrideColorWriteEnable( bool bOverrideEnable, bool bColorWriteEnable )
{
}


//legacy fast clipping linkage
void CShaderAPIDx10::SetHeightClipZ( float z )
{
}

void CShaderAPIDx10::SetHeightClipMode( enum MaterialHeightClipMode_t heightClipMode )
{
}


// Sets the lights
void CShaderAPIDx10::SetLight( int lightNum, const LightDesc_t& desc )
{
}

void CShaderAPIDx10::SetAmbientLight( float r, float g, float b )
{
}

void CShaderAPIDx10::SetAmbientLightCube( Vector4D cube[6] )
{
}

// Get lights
int CShaderAPIDx10::GetMaxLights( void ) const
{
	return 0;
}

const LightDesc_t& CShaderAPIDx10::GetLight( int lightNum ) const
{
	static LightDesc_t blah;
	return blah;
}

// Render state for the ambient light cube (vertex shaders)
void CShaderAPIDx10::SetVertexShaderStateAmbientLightCube()
{
}

void CShaderAPIDx10::SetSkinningMatrices()
{
}

// Lightmap texture binding
void CShaderAPIDx10::BindLightmap( TextureStage_t stage )
{
}

void CShaderAPIDx10::BindBumpLightmap( TextureStage_t stage )
{
}

void CShaderAPIDx10::BindFullbrightLightmap( TextureStage_t stage )
{
}

void CShaderAPIDx10::BindWhite( TextureStage_t stage )
{
}

void CShaderAPIDx10::BindBlack( TextureStage_t stage )
{
}

void CShaderAPIDx10::BindGrey( TextureStage_t stage )
{
}

// Gets the lightmap dimensions
void CShaderAPIDx10::GetLightmapDimensions( int *w, int *h )
{
	g_pShaderUtil->GetLightmapDimensions( w, h );
}

// Special system flat normal map binding.
void CShaderAPIDx10::BindFlatNormalMap( TextureStage_t stage )
{
}

void CShaderAPIDx10::BindNormalizationCubeMap( TextureStage_t stage )
{
}

void CShaderAPIDx10::BindSignedNormalizationCubeMap( TextureStage_t stage )
{
}

void CShaderAPIDx10::BindFBTexture( TextureStage_t stage, int textureIndex )
{
}

// Flushes any primitives that are buffered
void CShaderAPIDx10::FlushBufferedPrimitives()
{
}

// Creates/destroys Mesh
IMesh* CShaderAPIDx10::CreateStaticMesh( VertexFormat_t fmt, const char *pTextureBudgetGroup, IMaterial * pMaterial )
{
	return &m_Mesh;
}

void CShaderAPIDx10::DestroyStaticMesh( IMesh* mesh )
{
}

// Gets the dynamic mesh; note that you've got to render the mesh
// before calling this function a second time. Clients should *not*
// call DestroyStaticMesh on the mesh returned by this call.
IMesh* CShaderAPIDx10::GetDynamicMesh( IMaterial* pMaterial, int nHWSkinBoneCount, bool buffered, IMesh* pVertexOverride, IMesh* pIndexOverride )
{
	Assert( (pMaterial == NULL) || ((IMaterialInternal *)pMaterial)->IsRealTimeVersion() );
	return &m_Mesh;
}

IMesh* CShaderAPIDx10::GetDynamicMeshEx( IMaterial* pMaterial, VertexFormat_t fmt, int nHWSkinBoneCount, bool buffered, IMesh* pVertexOverride, IMesh* pIndexOverride )
{
	// UNDONE: support compressed dynamic meshes if needed (pro: less VB memory, con: time spent compressing)
	Assert( CompressionType( pVertexOverride->GetVertexFormat() ) != VERTEX_COMPRESSION_NONE );
	Assert( (pMaterial == NULL) || ((IMaterialInternal *)pMaterial)->IsRealTimeVersion() );
	return &m_Mesh;
}

IMesh* CShaderAPIDx10::GetFlexMesh()
{
	return &m_Mesh;
}

// Begins a rendering pass that uses a state snapshot
void CShaderAPIDx10::BeginPass( StateSnapshot_t snapshot  )
{
}

// Renders a single pass of a material
void CShaderAPIDx10::RenderPass( int nPass, int nPassCount )
{
}

// stuff related to matrix stacks
void CShaderAPIDx10::MatrixMode( MaterialMatrixMode_t matrixMode )
{
}

void CShaderAPIDx10::PushMatrix()
{
}

void CShaderAPIDx10::PopMatrix()
{
}

void CShaderAPIDx10::LoadMatrix( float *m )
{
}

void CShaderAPIDx10::MultMatrix( float *m )
{
}

void CShaderAPIDx10::MultMatrixLocal( float *m )
{
}

void CShaderAPIDx10::GetMatrix( MaterialMatrixMode_t matrixMode, float *dst )
{
}

void CShaderAPIDx10::LoadIdentity( void )
{
}

void CShaderAPIDx10::LoadCameraToWorld( void )
{
}

void CShaderAPIDx10::Ortho( double left, double top, double right, double bottom, double zNear, double zFar )
{
}

void CShaderAPIDx10::PerspectiveX( double fovx, double aspect, double zNear, double zFar )
{
}

void CShaderAPIDx10::PerspectiveOffCenterX( double fovx, double aspect, double zNear, double zFar, double bottom, double top, double left, double right )
{
}

void CShaderAPIDx10::PickMatrix( int x, int y, int width, int height )
{
}

void CShaderAPIDx10::Rotate( float angle, float x, float y, float z )
{
}

void CShaderAPIDx10::Translate( float x, float y, float z )
{
}

void CShaderAPIDx10::Scale( float x, float y, float z )
{
}

void CShaderAPIDx10::ScaleXY( float x, float y )
{
}

// Fog methods...
void CShaderAPIDx10::FogMode( MaterialFogMode_t fogMode )
{
}

void CShaderAPIDx10::FogStart( float fStart )
{
}

void CShaderAPIDx10::FogEnd( float fEnd )
{
}

void CShaderAPIDx10::SetFogZ( float fogZ )
{
}

void CShaderAPIDx10::FogMaxDensity( float flMaxDensity )
{
}


void CShaderAPIDx10::GetFogDistances( float *fStart, float *fEnd, float *fFogZ )
{
}


void CShaderAPIDx10::SceneFogColor3ub( unsigned char r, unsigned char g, unsigned char b )
{
}


void CShaderAPIDx10::SceneFogMode( MaterialFogMode_t fogMode )
{
}

void CShaderAPIDx10::GetSceneFogColor( unsigned char *rgb )
{
}

MaterialFogMode_t CShaderAPIDx10::GetSceneFogMode( )
{
	return MATERIAL_FOG_NONE;
}

int CShaderAPIDx10::GetPixelFogCombo( )
{
	return 0; //FIXME
}

void CShaderAPIDx10::FogColor3f( float r, float g, float b )
{
}

void CShaderAPIDx10::FogColor3fv( float const* rgb )
{
}

void CShaderAPIDx10::FogColor3ub( unsigned char r, unsigned char g, unsigned char b )
{
}

void CShaderAPIDx10::FogColor3ubv( unsigned char const* rgb )
{
}

void CShaderAPIDx10::Viewport( int x, int y, int width, int height )
{
}

void CShaderAPIDx10::GetViewport( int& x, int& y, int& width, int& height ) const
{
}

// Sets the vertex and pixel shaders
void CShaderAPIDx10::SetVertexShaderIndex( int vshIndex )
{
}

void CShaderAPIDx10::SetPixelShaderIndex( int pshIndex )
{
}

// Sets the constant register for vertex and pixel shaders
void CShaderAPIDx10::SetVertexShaderConstant( int var, float const* pVec, int numConst, bool bForce )
{
}

void CShaderAPIDx10::SetPixelShaderConstant( int var, float const* pVec, int numConst, bool bForce )
{
}

void CShaderAPIDx10::InvalidateDelayedShaderConstants( void )
{
}

//Set's the linear->gamma conversion textures to use for this hardware for both srgb writes enabled and disabled(identity)
void CShaderAPIDx10::SetLinearToGammaConversionTextures( ShaderAPITextureHandle_t hSRGBWriteEnabledTexture, ShaderAPITextureHandle_t hIdentityTexture )
{
}


// Returns the nearest supported format
ImageFormat CShaderAPIDx10::GetNearestSupportedFormat( ImageFormat fmt, bool bFilteringRequired /* = true */ ) const
{
	return fmt;
}

ImageFormat CShaderAPIDx10::GetNearestRenderTargetFormat( ImageFormat fmt ) const
{
	return fmt;
}

// Sets the texture state
void CShaderAPIDx10::BindTexture( Sampler_t stage, ShaderAPITextureHandle_t textureHandle )
{
}

// Indicates we're going to be modifying this texture
// TexImage2D, TexSubImage2D, TexWrap, TexMinFilter, and TexMagFilter
// all use the texture specified by this function.
void CShaderAPIDx10::ModifyTexture( ShaderAPITextureHandle_t textureHandle )
{
}

// Texture management methods
void CShaderAPIDx10::TexImage2D( int level, int cubeFace, ImageFormat dstFormat, int zOffset, int width, int height, 
								 ImageFormat srcFormat, bool bSrcIsTiled, void *imageData )
{
}

void CShaderAPIDx10::TexSubImage2D( int level, int cubeFace, int xOffset, int yOffset, int zOffset, int width, int height,
									ImageFormat srcFormat, int srcStride, bool bSrcIsTiled, void *imageData )
{
}

void CShaderAPIDx10::TexImageFromVTF( IVTFTexture *pVTF, int iVTFFrame )
{
}

bool CShaderAPIDx10::TexLock( int level, int cubeFaceID, int xOffset, int yOffset, 
							  int width, int height, CPixelWriter& writer )
{
	return false;
}

void CShaderAPIDx10::TexUnlock( )
{
}


// These are bound to the texture, not the texture environment
void CShaderAPIDx10::TexMinFilter( ShaderTexFilterMode_t texFilterMode )
{
}

void CShaderAPIDx10::TexMagFilter( ShaderTexFilterMode_t texFilterMode )
{
}

void CShaderAPIDx10::TexWrap( ShaderTexCoordComponent_t coord, ShaderTexWrapMode_t wrapMode )
{
}

void CShaderAPIDx10::TexSetPriority( int priority )
{
}

ShaderAPITextureHandle_t CShaderAPIDx10::CreateTexture( 
	int width, 
	int height,
	int depth,
	ImageFormat dstImageFormat, 
	int numMipLevels, 
	int numCopies, 
	int flags, 
	const char *pDebugName,
	const char *pTextureGroupName )
{
	ShaderAPITextureHandle_t handle;
	CreateTextures( &handle, 1, width, height, depth, dstImageFormat, numMipLevels, numCopies, flags, pDebugName, pTextureGroupName );
	return handle;
}

void CShaderAPIDx10::CreateTextures( 
					ShaderAPITextureHandle_t *pHandles,
					int count,
					int width, 
					int height,
					int depth,
					ImageFormat dstImageFormat, 
					int numMipLevels, 
					int numCopies, 
					int flags, 
					const char *pDebugName,
					const char *pTextureGroupName )
{
	for ( int k = 0; k < count; ++ k )
	{
		pHandles[ k ] = 0;
	}
}

ShaderAPITextureHandle_t CShaderAPIDx10::CreateDepthTexture( ImageFormat renderFormat, int width, int height, const char *pDebugName, bool bTexture )
{
	return 0;
}

void CShaderAPIDx10::DeleteTexture( ShaderAPITextureHandle_t textureHandle )
{
}

bool CShaderAPIDx10::IsTexture( ShaderAPITextureHandle_t textureHandle )
{
	return true;
}

bool CShaderAPIDx10::IsTextureResident( ShaderAPITextureHandle_t textureHandle )
{
	return false;
}

// stuff that isn't to be used from within a shader
void CShaderAPIDx10::ClearBuffersObeyStencil( bool bClearColor, bool bClearDepth )
{
}

void CShaderAPIDx10::ClearBuffersObeyStencilEx( bool bClearColor, bool bClearAlpha, bool bClearDepth )
{
}

void CShaderAPIDx10::PerformFullScreenStencilOperation( void )
{
}

void CShaderAPIDx10::ReadPixels( int x, int y, int width, int height, unsigned char *data, ImageFormat dstFormat )
{
}

void CShaderAPIDx10::ReadPixels( Rect_t *pSrcRect, Rect_t *pDstRect, unsigned char *data, ImageFormat dstFormat, int nDstStride )
{
}

void CShaderAPIDx10::FlushHardware()
{
}

// Set the number of bone weights
void CShaderAPIDx10::SetNumBoneWeights( int numBones )
{
}

// Selection mode methods
int CShaderAPIDx10::SelectionMode( bool selectionMode )
{
	return 0;
}

void CShaderAPIDx10::SelectionBuffer( unsigned int* pBuffer, int size )
{
}

void CShaderAPIDx10::ClearSelectionNames( )
{
}

void CShaderAPIDx10::LoadSelectionName( int name )
{
}

void CShaderAPIDx10::PushSelectionName( int name )
{
}

void CShaderAPIDx10::PopSelectionName()
{
}


// Use this to get the mesh builder that allows us to modify vertex data
CMeshBuilder* CShaderAPIDx10::GetVertexModifyBuilder()
{
	return 0;
}

// Board-independent calls, here to unify how shaders set state
// Implementations should chain back to IShaderUtil->BindTexture(), etc.

// Use this to begin and end the frame
void CShaderAPIDx10::BeginFrame()
{
}

void CShaderAPIDx10::EndFrame()
{
}

// returns the current time in seconds....
double CShaderAPIDx10::CurrentTime() const
{
	return Sys_FloatTime();
}

// Get the current camera position in world space.
void CShaderAPIDx10::GetWorldSpaceCameraPosition( float * pPos ) const
{
}

void CShaderAPIDx10::ForceHardwareSync( void )
{
}

void CShaderAPIDx10::SetClipPlane( int index, const float *pPlane )
{
}

void CShaderAPIDx10::EnableClipPlane( int index, bool bEnable )
{
}

void CShaderAPIDx10::SetFastClipPlane( const float *pPlane )
{
}

void CShaderAPIDx10::EnableFastClip( bool bEnable )
{
}

int CShaderAPIDx10::GetCurrentNumBones( void ) const
{
	return 0;
}

// Is hardware morphing enabled?
bool CShaderAPIDx10::IsHWMorphingEnabled( ) const
{
	return false;
}

int CShaderAPIDx10::GetCurrentLightCombo( void ) const
{
	return 0;
}

int CShaderAPIDx10::MapLightComboToPSLightCombo( int nLightCombo ) const
{
	return 0;
}

MaterialFogMode_t CShaderAPIDx10::GetCurrentFogType( void ) const
{
	return MATERIAL_FOG_NONE;
}

void CShaderAPIDx10::RecordString( const char *pStr )
{
}

void CShaderAPIDx10::DestroyVertexBuffers( bool bExitingLevel )
{
}

int CShaderAPIDx10::GetCurrentDynamicVBSize( void )
{
	return 0;
}

void CShaderAPIDx10::EvictManagedResources()
{
}

void CShaderAPIDx10::ReleaseShaderObjects()
{
}

void CShaderAPIDx10::RestoreShaderObjects()
{
}

void CShaderAPIDx10::SetTextureTransformDimension( TextureStage_t textureStage, int dimension, bool projected )
{
}

void CShaderAPIDx10::SetBumpEnvMatrix( TextureStage_t textureStage, float m00, float m01, float m10, float m11 )
{
}

void CShaderAPIDx10::SyncToken( const char *pToken )
{
}
