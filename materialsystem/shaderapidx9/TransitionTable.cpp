//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#define DISABLE_PROTECTED_THINGS
#include "togl/rendermechanism.h"
#include "TransitionTable.h"
#include "recording.h"
#include "shaderapidx8.h"
#include "shaderapi/ishaderutil.h"
#include "tier1/convar.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "vertexshaderdx8.h"
#include "tier0/vprof.h"
#include "shaderdevicedx8.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"
 

enum
{
	TEXTURE_STAGE_BIT_COUNT = 4,
	TEXTURE_STAGE_MAX_STAGE = 1 << TEXTURE_STAGE_BIT_COUNT,
	TEXTURE_STAGE_MASK = TEXTURE_STAGE_MAX_STAGE - 1,

	TEXTURE_OP_BIT_COUNT = 7 - TEXTURE_STAGE_BIT_COUNT,
	TEXTURE_OP_SHIFT = TEXTURE_STAGE_BIT_COUNT,
	TEXTURE_OP_MASK = ((1 << TEXTURE_OP_BIT_COUNT) - 1) << TEXTURE_OP_SHIFT,
};


//-----------------------------------------------------------------------------
// Texture op compressing/uncompressing
//-----------------------------------------------------------------------------
inline unsigned char TextureOp( TextureStateFunc_t func, int stage )
{
	// This fails if we've added too many texture stages states to fit in a byte.
	COMPILE_TIME_ASSERT( TEXTURE_STATE_COUNT < (1 << TEXTURE_OP_BIT_COUNT) );
	Assert( stage < TEXTURE_STAGE_MAX_STAGE );

	return ((func << TEXTURE_OP_SHIFT) & TEXTURE_OP_MASK) | (stage & TEXTURE_STAGE_MASK);
}

inline void GetTextureOp( unsigned char nBits, TextureStateFunc_t *pFunc, int *pStage )
{
	*pStage = (nBits & TEXTURE_STAGE_MASK);
	*pFunc = (TextureStateFunc_t)((nBits & TEXTURE_OP_MASK) >> TEXTURE_OP_SHIFT);
}


//-----------------------------------------------------------------------------
// Stats
//-----------------------------------------------------------------------------
static int s_pRenderTransitions[RENDER_STATE_COUNT];
static int s_pTextureTransitions[TEXTURE_STATE_COUNT][TEXTURE_STAGE_MAX_STAGE];


//-----------------------------------------------------------------------------
// Singleton
//-----------------------------------------------------------------------------
CTransitionTable *g_pTransitionTable = NULL;

#ifdef DEBUG_BOARD_STATE
inline ShadowState_t& BoardState()
{
	return g_pTransitionTable->BoardState();
}
#endif

inline CTransitionTable::CurrentState_t& CurrentState()
{
	return g_pTransitionTable->CurrentState();
}


//-----------------------------------------------------------------------------
// Less functions
//-----------------------------------------------------------------------------
bool CTransitionTable::ShadowStateDictLessFunc::Less( const CTransitionTable::ShadowStateDictEntry_t &src1, const CTransitionTable::ShadowStateDictEntry_t &src2, void *pCtx )
{
	return src1.m_nChecksum < src2.m_nChecksum;
}

bool CTransitionTable::SnapshotDictLessFunc::Less( const CTransitionTable::SnapshotDictEntry_t &src1, const CTransitionTable::SnapshotDictEntry_t &src2, void *pCtx )
{
	return src1.m_nChecksum < src2.m_nChecksum;
}

bool CTransitionTable::UniqueSnapshotLessFunc::Less( const CTransitionTable::TransitionList_t &src1, const CTransitionTable::TransitionList_t &src2, void *pCtx )
{
	return src1.m_NumOperations > src2.m_NumOperations;
}

	
//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CTransitionTable::CTransitionTable() : m_DefaultStateSnapshot(-1),
	m_CurrentShadowId(-1), m_CurrentSnapshotId(-1), m_TransitionOps( 0, 8192 ), m_ShadowStateList( 0, 256 ),
	m_TransitionTable( 0, 256 ), m_SnapshotList( 0, 256 ), 
	m_ShadowStateDict(0, 256 ), 
	m_SnapshotDict( 0, 256 ), 
	m_UniqueTransitions( 0, 4096 ) 
{
	Assert( !g_pTransitionTable );
	g_pTransitionTable = this;

#ifdef DEBUG_BOARD_STATE
	memset( &m_BoardState, 0, sizeof( m_BoardState ) );
	memset( &m_BoardShaderState, 0, sizeof( m_BoardShaderState ) );
#endif
}

CTransitionTable::~CTransitionTable()
{
	Assert( g_pTransitionTable == this );
	g_pTransitionTable = NULL;
}


//-----------------------------------------------------------------------------
// Initialization, shutdown
//-----------------------------------------------------------------------------
bool CTransitionTable::Init( )
{
	return true;
}

void CTransitionTable::Shutdown( )
{
	Reset();
}


//-----------------------------------------------------------------------------
// Creates a shadow, adding an entry into the shadow list and transition table
//-----------------------------------------------------------------------------
StateSnapshot_t CTransitionTable::CreateStateSnapshot( ShadowStateId_t shadowStateId, const ShadowShaderState_t& currentShaderState )
{
	StateSnapshot_t snapshotId = m_SnapshotList.AddToTail();

	// Copy our snapshot into the list
	SnapshotShaderState_t &shaderState = m_SnapshotList[snapshotId];
	shaderState.m_ShadowStateId = shadowStateId;
	memcpy( &shaderState.m_ShaderState, &currentShaderState, sizeof(ShadowShaderState_t) );
	memset( shaderState.m_ShaderState.m_nReserved, 0, sizeof( shaderState.m_ShaderState.m_nReserved ) );
	shaderState.m_nReserved = 0;	// needed to get a good CRC
	shaderState.m_nReserved2 = 0;

	// Insert entry into the lookup table
	SnapshotDictEntry_t insert;

	CRC32_Init(	&insert.m_nChecksum );
	CRC32_ProcessBuffer( &insert.m_nChecksum, &shaderState, sizeof(SnapshotShaderState_t) );
	CRC32_Final( &insert.m_nChecksum );

	insert.m_nSnapshot = snapshotId;
	m_SnapshotDict.Insert( insert );

	return snapshotId;
}


//-----------------------------------------------------------------------------
// Creates a shadow, adding an entry into the shadow list and transition table
//-----------------------------------------------------------------------------
CTransitionTable::ShadowStateId_t CTransitionTable::CreateShadowState( const ShadowState_t &currentState )
{
	int newShaderState = m_ShadowStateList.AddToTail();

	// Copy our snapshot into the list
	memcpy( &m_ShadowStateList[newShaderState], &currentState, sizeof(ShadowState_t) );

	// all existing states must transition to the new state
	int i;
	for ( i = 0; i < newShaderState; ++i )
	{
		// Add a new transition to all existing states
		int newElem = m_TransitionTable[i].AddToTail();
		m_TransitionTable[i][newElem].m_FirstOperation = INVALID_TRANSITION_OP;
		m_TransitionTable[i][newElem].m_NumOperations = 0;
	}

	// Add a new vector for this transition
	int newTransitionElem = m_TransitionTable.AddToTail();
	m_TransitionTable[newTransitionElem].EnsureCapacity( 32 );
	Assert( newShaderState == newTransitionElem );

	for ( i = 0; i <= newShaderState; ++i )
	{
		// Add a new transition from all existing states
		int newElem = m_TransitionTable[newShaderState].AddToTail();
		m_TransitionTable[newShaderState][newElem].m_FirstOperation = INVALID_TRANSITION_OP;
		m_TransitionTable[newShaderState][newElem].m_NumOperations = 0;
	}

	// Insert entry into the lookup table
	ShadowStateDictEntry_t insert;

	CRC32_Init(	&insert.m_nChecksum );
	CRC32_ProcessBuffer( &insert.m_nChecksum, &m_ShadowStateList[newShaderState], sizeof(ShadowState_t) );
	CRC32_Final( &insert.m_nChecksum );

	insert.m_nShadowStateId = newShaderState;
	m_ShadowStateDict.Insert( insert );

	return newShaderState;
}


//-----------------------------------------------------------------------------
// Finds a snapshot, if it exists. Or creates a new one if it doesn't.
//-----------------------------------------------------------------------------
CTransitionTable::ShadowStateId_t CTransitionTable::FindShadowState( const ShadowState_t& currentState ) const
{
	ShadowStateDictEntry_t find;

	CRC32_Init(	&find.m_nChecksum );
	CRC32_ProcessBuffer( &find.m_nChecksum, &currentState, sizeof(ShadowState_t) );
	CRC32_Final( &find.m_nChecksum );
	
	int nDictCount = m_ShadowStateDict.Count();
	int i = m_ShadowStateDict.FindLessOrEqual( find );
	if ( i < 0 )
		return (ShadowStateId_t)-1;

	for ( ; i < nDictCount; ++i )
	{
		const ShadowStateDictEntry_t &entry = m_ShadowStateDict[i];

		// Didn't find a match
		if ( entry.m_nChecksum > find.m_nChecksum )
			break;

		if ( entry.m_nChecksum != find.m_nChecksum )
			continue;

		ShadowStateId_t nShadowState = entry.m_nShadowStateId;
		if (!memcmp(&m_ShadowStateList[nShadowState], &currentState, sizeof(ShadowState_t) ))
			return nShadowState;
	}

	// Need to create a new one
	return (ShadowStateId_t)-1;
}


//-----------------------------------------------------------------------------
// Finds a snapshot, if it exists. Or creates a new one if it doesn't.
//-----------------------------------------------------------------------------
StateSnapshot_t CTransitionTable::FindStateSnapshot( ShadowStateId_t id, const ShadowShaderState_t& currentState ) const
{
	SnapshotShaderState_t temp;
	temp.m_ShaderState = currentState;
	temp.m_ShadowStateId = id;
	memset( temp.m_ShaderState.m_nReserved, 0, sizeof( temp.m_ShaderState.m_nReserved ) );
	temp.m_nReserved = 0;	// needed to get a good CRC
	temp.m_nReserved2 = 0;

	SnapshotDictEntry_t find;

	CRC32_Init(	&find.m_nChecksum );
	CRC32_ProcessBuffer( &find.m_nChecksum, &temp, sizeof(temp) );
	CRC32_Final( &find.m_nChecksum );

	int nDictCount = m_SnapshotDict.Count();
	int i = m_SnapshotDict.FindLessOrEqual( find );
	if ( i < 0 )
		return (StateSnapshot_t)-1;

	for ( ; i < nDictCount; ++i )
	{
		// Didn't find a match
		if ( m_SnapshotDict[i].m_nChecksum > find.m_nChecksum )
			break;

		if ( m_SnapshotDict[i].m_nChecksum != find.m_nChecksum )
			continue;

		StateSnapshot_t nShapshot = m_SnapshotDict[i].m_nSnapshot;
		if ( (id == m_SnapshotList[nShapshot].m_ShadowStateId) && 
			!memcmp(&m_SnapshotList[nShapshot].m_ShaderState, &currentState, sizeof(ShadowShaderState_t)) )
		{
			return nShapshot;
		}
	}

	// Need to create a new one
	return (StateSnapshot_t)-1;
}


//-----------------------------------------------------------------------------
// Used to clear the transition table when we know it's become invalid.
//-----------------------------------------------------------------------------
void CTransitionTable::Reset()
{
	m_ShadowStateList.RemoveAll();
	m_SnapshotList.RemoveAll();
	m_TransitionTable.RemoveAll();
	m_TransitionOps.RemoveAll();
	m_ShadowStateDict.RemoveAll();
	m_SnapshotDict.RemoveAll();
	m_UniqueTransitions.RemoveAll();
	m_CurrentShadowId = -1;
	m_CurrentSnapshotId = -1;
	m_DefaultStateSnapshot = -1;
}


//-----------------------------------------------------------------------------
// Sets the texture stage state
//-----------------------------------------------------------------------------
#ifdef _WIN32
#pragma warning( disable : 4189 )
#endif

static inline void SetTextureStageState( int stage, D3DTEXTURESTAGESTATETYPE state, DWORD val )
{
#if !defined( _X360 )
	Assert( !g_pShaderDeviceDx8->IsDeactivated() );
	Dx9Device()->SetTextureStageState( stage, state, val );
#endif
}

//Moved to a #define so every instance of this skips unsupported render states at compile time
#define SetSamplerState( _stage, _state, _val )								\
	{																		\
		if ( (_state != D3DSAMP_NOTSUPPORTED) )								\
		{																	\
			Assert( !g_pShaderDeviceDx8->IsDeactivated() );					\
			Dx9Device()->SetSamplerState( _stage, _state, _val );			\
		}																	\
	}

//Moved to a #define so every instance of this skips unsupported render states at compile time
#define SetRenderState( _state, _val )						\
	{														\
		if ( _state != D3DRS_NOTSUPPORTED )					\
		{													\
			Assert( !g_pShaderDeviceDx8->IsDeactivated() ); \
			Dx9Device()->SetRenderState( _state, _val );	\
		}													\
	}

#ifdef DX_TO_GL_ABSTRACTION
	#define SetRenderStateConstMacro( state, val ) { if ( state != D3DRS_NOTSUPPORTED ) Dx9Device()->SetRenderStateConstInline( state, val ); }
#else
	#define SetRenderStateConstMacro( state, val ) SetRenderState( state, val )
#endif

#ifdef _WIN32
#pragma warning( default : 4189 )
#endif

//-----------------------------------------------------------------------------
// Methods that actually apply the state
//-----------------------------------------------------------------------------
#ifdef DEBUG_BOARD_STATE

static bool g_SpewTransitions = false;

#define UPDATE_BOARD_RENDER_STATE( _d3dState, _state )					\
	{																	\
		BoardState().m_ ## _state = shaderState.m_ ## _state;			\
		if (g_SpewTransitions)											\
		{																\
			char buf[128];												\
			sprintf( buf, "Apply %s : %d\n", #_d3dState, shaderState.m_ ## _state ); \
			Plat_DebugString(buf);										\
		}																\
	}

#define UPDATE_BOARD_TEXTURE_STAGE_STATE( _d3dState, _state, _stage )	\
	{																	\
		BoardState().m_TextureStage[_stage].m_ ## _state = shaderState.m_TextureStage[_stage].m_ ## _state;	\
		if (g_SpewTransitions)											\
		{																\
			char buf[128];												\
			sprintf( buf, "Apply Tex %s (%d): %d\n", #_d3dState, _stage, shaderState.m_TextureStage[_stage].m_ ## _state ); \
			Plat_DebugString(buf);										\
		}																\
	}

#define UPDATE_BOARD_SAMPLER_STATE( _d3dState, _state, _stage )			\
	{																	\
		BoardState().m_SamplerState[_stage].m_ ## _state = shaderState.m_SamplerState[_stage].m_ ## _state;	\
		if (g_SpewTransitions)											\
		{																\
			char buf[128];												\
			sprintf( buf, "Apply SamplerSate %s (%d): %d\n", #_d3dState, stage, shaderState.m_SamplerState[_stage].m_ ## _state ); \
			Plat_DebugString(buf);										\
		}																\
	}

#else

#define UPDATE_BOARD_RENDER_STATE( _d3dState, _state ) {}
#define UPDATE_BOARD_TEXTURE_STAGE_STATE( _d3dState, _state, _stage ) {}
#define UPDATE_BOARD_SAMPLER_STATE( _d3dState, _state, _stage ) {}

#endif


#define APPLY_RENDER_STATE_FUNC( _d3dState, _state )					\
	void Apply ## _state( const ShadowState_t& shaderState, int arg )	\
	{																	\
		SetRenderState( _d3dState, shaderState.m_ ## _state );			\
		UPDATE_BOARD_RENDER_STATE( _d3dState, _state );					\
	}

#define APPLY_TEXTURE_STAGE_STATE_FUNC( _d3dState, _state )				\
	void Apply ## _state( const ShadowState_t& shaderState, int stage )	\
	{																	\
		SetTextureStageState( stage, _d3dState, shaderState.m_TextureStage[stage].m_ ## _state );	\
		UPDATE_BOARD_TEXTURE_STAGE_STATE( _d3dState, _state, stage );	\
	}

#define APPLY_SAMPLER_STATE_FUNC( _d3dState, _state )					\
	void Apply ## _state( const ShadowState_t& shaderState, int stage )	\
	{																	\
		SetSamplerState( stage, _d3dState, shaderState.m_SamplerState[stage].m_ ## _state );	\
		UPDATE_BOARD_SAMPLER_STATE( _d3dState, _state, stage );			\
	}	


// Special overridden sampler state to turn on Fetch4 on ATI hardware (and 360?)
void ApplyFetch4Enable( const ShadowState_t& shaderState, int stage )
{
	if ( ShaderAPI()->SupportsFetch4() )
	{
		SetSamplerState( stage, ATISAMP_FETCH4, shaderState.m_SamplerState[stage].m_Fetch4Enable ? ATI_FETCH4_ENABLE : ATI_FETCH4_DISABLE );
	}

	UPDATE_BOARD_SAMPLER_STATE( ATISAMP_FETCH4, Fetch4Enable, stage );
}																	

#ifdef DX_TO_GL_ABSTRACTION
void ApplyShadowFilterEnable( const ShadowState_t& shaderState, int stage )
{
	SetSamplerState( stage, D3DSAMP_SHADOWFILTER, shaderState.m_SamplerState[stage].m_ShadowFilterEnable );
	
	UPDATE_BOARD_SAMPLER_STATE( D3DSAMP_SHADOWFILTER, ShadowFilterEnable, stage );
}																	
#endif


//APPLY_RENDER_STATE_FUNC( D3DRS_ZWRITEENABLE,			ZWriteEnable )
//APPLY_RENDER_STATE_FUNC( D3DRS_COLORWRITEENABLE,		ColorWriteEnable )
APPLY_RENDER_STATE_FUNC( D3DRS_FILLMODE,				FillMode )
APPLY_RENDER_STATE_FUNC( D3DRS_LIGHTING,				Lighting )
APPLY_RENDER_STATE_FUNC( D3DRS_SPECULARENABLE,			SpecularEnable )
APPLY_RENDER_STATE_FUNC( D3DRS_DIFFUSEMATERIALSOURCE,	DiffuseMaterialSource )
APPLY_TEXTURE_STAGE_STATE_FUNC( D3DTSS_TEXCOORDINDEX,	TexCoordIndex )


void ApplyZWriteEnable( const ShadowState_t& shaderState, int arg )
{
	SetRenderStateConstMacro( D3DRS_ZWRITEENABLE, shaderState.m_ZWriteEnable );
#if defined( _X360 )
	//SetRenderStateConstMacro( D3DRS_HIZWRITEENABLE, shaderState.m_ZWriteEnable ? D3DHIZ_AUTOMATIC : D3DHIZ_DISABLE );
#endif

	UPDATE_BOARD_RENDER_STATE( D3DRS_ZWRITEENABLE, ZWriteEnable );
}

void ApplyColorWriteEnable( const ShadowState_t& shaderState, int arg )
{
	SetRenderState( D3DRS_COLORWRITEENABLE, shaderState.m_ColorWriteEnable );
	g_pTransitionTable->CurrentState().m_ColorWriteEnable = shaderState.m_ColorWriteEnable;

	UPDATE_BOARD_RENDER_STATE( D3DRS_COLORWRITEENABLE, ColorWriteEnable );
}

void ApplySRGBReadEnable( const ShadowState_t& shaderState, int stage )
{
#	if ( !defined( _X360 ) )
	{
		SetSamplerState( stage, D3DSAMP_SRGBTEXTURE, shaderState.m_SamplerState[stage].m_SRGBReadEnable );
	}
#	else
	{
		ShaderAPI()->ApplySRGBReadState( stage, shaderState.m_SamplerState[stage].m_SRGBReadEnable );
	}
#	endif

	UPDATE_BOARD_SAMPLER_STATE( D3DSAMP_SRGBTEXTURE, SRGBReadEnable, stage );
}


void ApplySRGBWriteEnable( const ShadowState_t& shadowState, int stageUnused )
{
	g_pTransitionTable->ApplySRGBWriteEnable( shadowState );
}


void CTransitionTable::ApplySRGBWriteEnable( const ShadowState_t& shaderState  )
{
	// ApplySRGBWriteEnable set to true means that the shader is writing linear values.
	if ( CurrentState().m_bLinearColorSpaceFrameBufferEnable )
	{
		// The shader had better be writing linear values since we can't convert to gamma here.
		// Can't leave this assert here since there are cases where the shader is doing the right thing.
		// This is good to test occasionally to make sure that the shaders are doing the right thing.
		//		Assert( shaderState.m_SRGBWriteEnable );

		// render target is linear
		SetRenderStateConstMacro( D3DRS_SRGBWRITEENABLE, 0 );
		ShaderAPI()->EnabledSRGBWrite( false );

		// fog isn't fixed-function with linear frame buffers, so don't bother with that here.
	}
	else
	{
		// render target is gamma

		// SRGBWrite enable can affect the space in which fog color is defined		
		if ( HardwareConfig()->NeedsShaderSRGBConversion() )
		{
			if ( HardwareConfig()->SupportsPixelShaders_2_b() ) //in 2b supported devices, we never actually enable SRGB writes, but instead handle the conversion in the pixel shader. But we want all other code to be unaware.
			{
				SetRenderStateConstMacro( D3DRS_SRGBWRITEENABLE, 0 );
			}
			else
			{
				SetRenderStateConstMacro( D3DRS_SRGBWRITEENABLE, shaderState.m_SRGBWriteEnable );
			}
		}
		else
		{
			SetRenderStateConstMacro( D3DRS_SRGBWRITEENABLE, shaderState.m_SRGBWriteEnable );
		}

		ShaderAPI()->EnabledSRGBWrite( shaderState.m_SRGBWriteEnable );

		if ( HardwareConfig()->SpecifiesFogColorInLinearSpace() )
		{
			ShaderAPI()->ApplyFogMode( shaderState.m_FogMode, shaderState.m_SRGBWriteEnable, shaderState.m_bDisableFogGammaCorrection );
		}
	}

#ifdef _DEBUG
	BoardState().m_SRGBWriteEnable = shaderState.m_SRGBWriteEnable;
	if (g_SpewTransitions)											
	{																
		char buf[128];												
		sprintf( buf, "Apply %s : %d\n", "D3DRS_SRGBWRITEENABLE", shaderState.m_SRGBWriteEnable );
		Plat_DebugString(buf);										
	}																
#endif
}

void ApplyDisableFogGammaCorrection( const ShadowState_t& shadowState, int stageUnused )
{
	ShaderAPI()->ApplyFogMode( shadowState.m_FogMode, shadowState.m_SRGBWriteEnable, shadowState.m_bDisableFogGammaCorrection );
		
#ifdef DEBUG_BOARD_STATE
	g_pTransitionTable->BoardState().m_bDisableFogGammaCorrection = shadowState.m_bDisableFogGammaCorrection;
#endif
}




void ApplyDepthTest( const ShadowState_t& state, int stage )
{
	g_pTransitionTable->ApplyDepthTest( state );
}

void CTransitionTable::SetZEnable( D3DZBUFFERTYPE nEnable )
{
	if (m_CurrentState.m_ZEnable != nEnable )
	{
		SetRenderStateConstMacro( D3DRS_ZENABLE, nEnable );
#if defined( _X360 )
		//SetRenderState( D3DRS_HIZENABLE, nEnable ? D3DHIZ_AUTOMATIC : D3DHIZ_DISABLE );
#endif
		m_CurrentState.m_ZEnable = nEnable;
	}
}

void CTransitionTable::SetZFunc( D3DCMPFUNC nCmpFunc )
{
	if (m_CurrentState.m_ZFunc != nCmpFunc )
	{
		SetRenderStateConstMacro( D3DRS_ZFUNC, nCmpFunc );
		m_CurrentState.m_ZFunc = nCmpFunc;
	}
}

void CTransitionTable::ApplyDepthTest( const ShadowState_t& state )
{
	SetZEnable( state.m_ZEnable );
	if (state.m_ZEnable != D3DZB_FALSE)
	{
		SetZFunc( state.m_ZFunc );
	}
	if (m_CurrentState.m_ZBias != state.m_ZBias)
	{
		ShaderAPI()->ApplyZBias( state );
		m_CurrentState.m_ZBias = (PolygonOffsetMode_t) state.m_ZBias; // Cast two bits from m_ZBias
	}

#ifdef DEBUG_BOARD_STATE
	// This isn't quite true, but it's necessary for other error checking to work
	BoardState().m_ZEnable = state.m_ZEnable;
	BoardState().m_ZFunc = state.m_ZFunc;
	BoardState().m_ZBias = state.m_ZBias;
#endif
}

void ApplyAlphaTest( const ShadowState_t& state, int stage )
{
	g_pTransitionTable->ApplyAlphaTest( state );
}

void CTransitionTable::ApplyAlphaTest( const ShadowState_t& state )
{
	if (m_CurrentState.m_AlphaTestEnable != state.m_AlphaTestEnable)
	{
		SetRenderStateConstMacro( D3DRS_ALPHATESTENABLE, state.m_AlphaTestEnable );
		m_CurrentState.m_AlphaTestEnable = state.m_AlphaTestEnable;
	}

	if (state.m_AlphaTestEnable)
	{
		// Set the blend state here...
		if (m_CurrentState.m_AlphaFunc != state.m_AlphaFunc)
		{
			SetRenderStateConstMacro( D3DRS_ALPHAFUNC, state.m_AlphaFunc );
			m_CurrentState.m_AlphaFunc = state.m_AlphaFunc;
		}

		if (m_CurrentState.m_AlphaRef != state.m_AlphaRef)
		{
			SetRenderStateConstMacro( D3DRS_ALPHAREF, state.m_AlphaRef );
			m_CurrentState.m_AlphaRef = state.m_AlphaRef;
		}
	}

#ifdef DEBUG_BOARD_STATE
	// This isn't quite true, but it's necessary for other error checking to work
	BoardState().m_AlphaTestEnable = state.m_AlphaTestEnable;
	BoardState().m_AlphaFunc = state.m_AlphaFunc;
	BoardState().m_AlphaRef = state.m_AlphaRef;
#endif
}

void ApplyAlphaBlend( const ShadowState_t& state, int stage )
{
	g_pTransitionTable->ApplyAlphaBlend( state );
}

void CTransitionTable::ApplyAlphaBlend( const ShadowState_t& state )
{
	if (m_CurrentState.m_AlphaBlendEnable != state.m_AlphaBlendEnable)
	{
		SetRenderStateConstMacro( D3DRS_ALPHABLENDENABLE, state.m_AlphaBlendEnable );
		m_CurrentState.m_AlphaBlendEnable = state.m_AlphaBlendEnable;
	}

	if (state.m_AlphaBlendEnable)
	{
		// Set the blend state here...
		if (m_CurrentState.m_SrcBlend != state.m_SrcBlend)
		{
			SetRenderStateConstMacro( D3DRS_SRCBLEND, state.m_SrcBlend );
			m_CurrentState.m_SrcBlend = state.m_SrcBlend;
		}

		if (m_CurrentState.m_DestBlend != state.m_DestBlend)
		{
			SetRenderStateConstMacro( D3DRS_DESTBLEND, state.m_DestBlend );
			m_CurrentState.m_DestBlend = state.m_DestBlend;
		}

		if (m_CurrentState.m_BlendOp != state.m_BlendOp )
		{
			SetRenderStateConstMacro( D3DRS_BLENDOP, state.m_BlendOp );
			m_CurrentState.m_BlendOp = state.m_BlendOp;
		}
	}

#ifdef DEBUG_BOARD_STATE
	// This isn't quite true, but it's necessary for other error checking to work
	BoardState().m_AlphaBlendEnable = state.m_AlphaBlendEnable;
	BoardState().m_SrcBlend = state.m_SrcBlend;
	BoardState().m_DestBlend = state.m_DestBlend;
	BoardState().m_BlendOp = state.m_BlendOp;
#endif
}

void ApplySeparateAlphaBlend( const ShadowState_t& state, int stage )
{
	g_pTransitionTable->ApplySeparateAlphaBlend( state );
}

void CTransitionTable::ApplySeparateAlphaBlend( const ShadowState_t& state )
{
	if (m_CurrentState.m_SeparateAlphaBlendEnable != state.m_SeparateAlphaBlendEnable)
	{
		SetRenderStateConstMacro( D3DRS_SEPARATEALPHABLENDENABLE, state.m_SeparateAlphaBlendEnable );
		m_CurrentState.m_SeparateAlphaBlendEnable = state.m_SeparateAlphaBlendEnable;
	}

	if (state.m_SeparateAlphaBlendEnable)
	{
		// Set the blend state here...
		if (m_CurrentState.m_SrcBlendAlpha != state.m_SrcBlendAlpha)
		{
			SetRenderStateConstMacro( D3DRS_SRCBLENDALPHA, state.m_SrcBlendAlpha );
			m_CurrentState.m_SrcBlendAlpha = state.m_SrcBlendAlpha;
		}

		if (m_CurrentState.m_DestBlendAlpha != state.m_DestBlendAlpha)
		{
			SetRenderStateConstMacro( D3DRS_DESTBLENDALPHA, state.m_DestBlendAlpha );
			m_CurrentState.m_DestBlendAlpha = state.m_DestBlendAlpha;
		}

		if (m_CurrentState.m_BlendOpAlpha != state.m_BlendOpAlpha )
		{
			SetRenderStateConstMacro( D3DRS_BLENDOPALPHA, state.m_BlendOpAlpha );
			m_CurrentState.m_BlendOpAlpha = state.m_BlendOpAlpha;
		}
	}

#ifdef DEBUG_BOARD_STATE
	// This isn't quite true, but it's necessary for other error checking to work
	BoardState().m_SeparateAlphaBlendEnable = state.m_SeparateAlphaBlendEnable;
	BoardState().m_SrcBlendAlpha = state.m_SrcBlendAlpha;
	BoardState().m_DestBlendAlpha = state.m_DestBlendAlpha;
	BoardState().m_BlendOpAlpha = state.m_BlendOpAlpha;
#endif
}

//-----------------------------------------------------------------------------
// Applies alpha texture op
//-----------------------------------------------------------------------------
void ApplyColorTextureStage( const ShadowState_t& state, int stage )
{
	g_pTransitionTable->ApplyColorTextureStage( state, stage );
}

void ApplyAlphaTextureStage( const ShadowState_t& state, int stage )
{
	g_pTransitionTable->ApplyAlphaTextureStage( state, stage );
}

void CTransitionTable::ApplyColorTextureStage( const ShadowState_t& state, int stage )
{
	D3DTEXTUREOP op = state.m_TextureStage[stage].m_ColorOp;
	int arg1 = state.m_TextureStage[stage].m_ColorArg1;
	int arg2 = state.m_TextureStage[stage].m_ColorArg2;

	if (m_CurrentState.m_TextureStage[stage].m_ColorOp != op)
	{
		SetTextureStageState( stage, D3DTSS_COLOROP, op );
		m_CurrentState.m_TextureStage[stage].m_ColorOp = op;
	}

	if (op != D3DTOP_DISABLE)
	{
		if (m_CurrentState.m_TextureStage[stage].m_ColorArg1 != arg1)
		{
			SetTextureStageState( stage, D3DTSS_COLORARG1, arg1 );
			m_CurrentState.m_TextureStage[stage].m_ColorArg1 = arg1;
		}
		if (m_CurrentState.m_TextureStage[stage].m_ColorArg2 != arg2)
		{
			SetTextureStageState( stage, D3DTSS_COLORARG2, arg2 );
			m_CurrentState.m_TextureStage[stage].m_ColorArg2 = arg2;
		}
	}

#ifdef DEBUG_BOARD_STATE
	// This isn't quite true, but it's necessary for other error checking to work
	BoardState().m_TextureStage[stage].m_ColorOp = op;
	BoardState().m_TextureStage[stage].m_ColorArg1 = arg1;
	BoardState().m_TextureStage[stage].m_ColorArg2 = arg2;
#endif
}

void CTransitionTable::ApplyAlphaTextureStage( const ShadowState_t& state, int stage )
{
	D3DTEXTUREOP op = state.m_TextureStage[stage].m_AlphaOp;
	int arg1 = state.m_TextureStage[stage].m_AlphaArg1;
	int arg2 = state.m_TextureStage[stage].m_AlphaArg2;

	if (m_CurrentState.m_TextureStage[stage].m_AlphaOp != op)
	{
		SetTextureStageState( stage, D3DTSS_ALPHAOP, op );
		m_CurrentState.m_TextureStage[stage].m_AlphaOp = op;
	}

	if (op != D3DTOP_DISABLE)
	{
		if (m_CurrentState.m_TextureStage[stage].m_AlphaArg1 != arg1)
		{
			SetTextureStageState( stage, D3DTSS_ALPHAARG1, arg1 );
			m_CurrentState.m_TextureStage[stage].m_AlphaArg1 = arg1;
		}
		if (m_CurrentState.m_TextureStage[stage].m_AlphaArg2 != arg2)
		{
			SetTextureStageState( stage, D3DTSS_ALPHAARG2, arg2 );
			m_CurrentState.m_TextureStage[stage].m_AlphaArg2 = arg2;
		}
	}

#ifdef DEBUG_BOARD_STATE
	// This isn't quite true, but it's necessary for other error checking to work
	BoardState().m_TextureStage[stage].m_AlphaOp = op;
	BoardState().m_TextureStage[stage].m_AlphaArg1 = arg1;
	BoardState().m_TextureStage[stage].m_AlphaArg2 = arg2;
#endif
}


void ApplyActivateFixedFunction( const ShadowState_t& state, int stage )
{
	int nStageCount = HardwareConfig()->GetTextureStageCount();
	for ( int i = 0; i < nStageCount; ++i )
	{
		g_pTransitionTable->ApplyColorTextureStage( state, i );
		g_pTransitionTable->ApplyAlphaTextureStage( state, i );
	}
}


//-----------------------------------------------------------------------------
// Enables textures
//-----------------------------------------------------------------------------
void ApplyTextureEnable( const ShadowState_t& state, int stage )
{
	// This may well enable/disable textures that are already enabled/disabled
	// but the ShaderAPI will handle that
	int i;
	int nSamplerCount = HardwareConfig()->GetSamplerCount();
	for ( i = 0; i < nSamplerCount; ++i )
	{
		ShaderAPI()->ApplyTextureEnable( state, i );

#ifdef DEBUG_BOARD_STATE
		BoardState().m_SamplerState[i].m_TextureEnable = state.m_SamplerState[i].m_TextureEnable;
#endif
	}

	// Needed to prevent mat_dxlevel assertions
#ifdef DEBUG_BOARD_STATE
	for ( i = nSamplerCount; i < MAX_SAMPLERS; ++i )
	{
		BoardState().m_SamplerState[i].m_TextureEnable = false;
	}
#endif
}


//-----------------------------------------------------------------------------
// All transitions below this point depend on dynamic render state
// FIXME: Eliminate these virtual calls?
//-----------------------------------------------------------------------------
void ApplyCullEnable( const ShadowState_t& state, int arg )
{
	ShaderAPI()->ApplyCullEnable( state.m_CullEnable );

#ifdef DEBUG_BOARD_STATE
	BoardState().m_CullEnable = state.m_CullEnable;
#endif
}

//-----------------------------------------------------------------------------
void ApplyAlphaToCoverage( const ShadowState_t& state, int arg )
{
	ShaderAPI()->ApplyAlphaToCoverage( state.m_EnableAlphaToCoverage );

#ifdef DEBUG_BOARD_STATE
	BoardState().m_EnableAlphaToCoverage = state.m_EnableAlphaToCoverage;
#endif
}

//-----------------------------------------------------------------------------
void ApplyVertexBlendEnable( const ShadowState_t& state, int stage )
{
	ShaderAPI()->SetVertexBlendState( state.m_VertexBlendEnable ? -1 : 0 );

#ifdef DEBUG_BOARD_STATE
	BoardState().m_VertexBlendEnable = state.m_VertexBlendEnable;
#endif
}


//-----------------------------------------------------------------------------
// Outputs the fog mode string
//-----------------------------------------------------------------------------
#ifdef RECORDING
const char *ShaderFogModeToString( ShaderFogMode_t fogMode )
{
	switch( fogMode )
	{
	case SHADER_FOGMODE_DISABLED:
		return "SHADER_FOGMODE_DISABLED";
	case SHADER_FOGMODE_OO_OVERBRIGHT:
		return "SHADER_FOGMODE_OO_OVERBRIGHT";
	case SHADER_FOGMODE_BLACK:
		return "SHADER_FOGMODE_BLACK";
	case SHADER_FOGMODE_GREY:
		return "SHADER_FOGMODE_GREY";
	case SHADER_FOGMODE_FOGCOLOR:
		return "SHADER_FOGMODE_FOGCOLOR";
	case SHADER_FOGMODE_WHITE:
		return "SHADER_FOGMODE_WHITE";
	case SHADER_FOGMODE_NUMFOGMODES:
		return "SHADER_FOGMODE_NUMFOGMODES";
	default:
		return "ERROR";
	}
}
#endif

// Uses GetConfig().overbright and GetSceneFogMode, so 
// will have to fix up the state manually when those change.
void ApplyFogMode( const ShadowState_t& state, int arg )
{
#ifdef RECORDING
	char buf[1024];
	sprintf( buf, "ApplyFogMode( %s )", ShaderFogModeToString( state.m_FogMode ) );
	RECORD_DEBUG_STRING( buf );
#endif

	ShaderAPI()->ApplyFogMode( state.m_FogMode, state.m_SRGBWriteEnable, state.m_bDisableFogGammaCorrection );

#ifdef DEBUG_BOARD_STATE
	BoardState().m_FogMode = state.m_FogMode;
#endif
}


//-----------------------------------------------------------------------------
// Function tables mapping enum to function
//-----------------------------------------------------------------------------
ApplyStateFunc_t s_pRenderFunctionTable[] = 
{
	ApplyDepthTest,
	ApplyZWriteEnable,
	ApplyColorWriteEnable,
	ApplyAlphaTest,
	ApplyFillMode,
	ApplyLighting,
	ApplySpecularEnable,
	ApplySRGBWriteEnable,
	ApplyAlphaBlend,
	ApplySeparateAlphaBlend,
	ApplyCullEnable,
	ApplyVertexBlendEnable,
	ApplyFogMode,
	ApplyActivateFixedFunction,
	ApplyTextureEnable,			// Enables textures on *all* stages
	ApplyDiffuseMaterialSource,
	ApplyDisableFogGammaCorrection,
	ApplyAlphaToCoverage,
};

ApplyStateFunc_t s_pTextureFunctionTable[] =
{
	ApplyTexCoordIndex,
	ApplySRGBReadEnable,
	ApplyFetch4Enable,
#ifdef DX_TO_GL_ABSTRACTION
	ApplyShadowFilterEnable,
#endif
	// Fixed function states
	ApplyColorTextureStage,
	ApplyAlphaTextureStage,
};


//-----------------------------------------------------------------------------
// Creates an entry in the state transition table
//-----------------------------------------------------------------------------
inline void CTransitionTable::AddTransition( RenderStateFunc_t func )
{
	int nElem = m_TransitionOps.AddToTail();
	TransitionOp_t &op = m_TransitionOps[nElem];
	op.m_nInfo.m_bIsTextureCode = false;
	op.m_nInfo.m_nOpCode = func;

	// Stats
//	++s_pRenderTransitions[ func ];
}

inline void CTransitionTable::AddTextureTransition( TextureStateFunc_t func, int stage )
{
	int nElem = m_TransitionOps.AddToTail();
	TransitionOp_t &op = m_TransitionOps[nElem];
	op.m_nInfo.m_bIsTextureCode = true;
	op.m_nInfo.m_nOpCode = TextureOp( func, stage );

	// Stats
//	++s_pTextureTransitions[ func ][stage];
}

#define ADD_RENDER_STATE_TRANSITION( _state )				\
	if (bForce || (toState.m_ ## _state != fromState.m_ ## _state))	\
	{														\
		AddTransition( RENDER_STATE_ ## _state );			\
		++numOps;											\
	}

#define ADD_TEXTURE_STAGE_STATE_TRANSITION( _stage, _state )\
	if (bForce || (toState.m_TextureStage[_stage].m_ ## _state != fromState.m_TextureStage[_stage].m_ ## _state))	\
	{														\
		Assert( _stage < MAX_TEXTURE_STAGES );				\
		AddTextureTransition( TEXTURE_STATE_ ## _state, _stage );	\
		++numOps;											\
	}

#define ADD_SAMPLER_STATE_TRANSITION( _stage, _state )\
	if (bForce || (toState.m_SamplerState[_stage].m_ ## _state != fromState.m_SamplerState[_stage].m_ ## _state))	\
	{														\
		Assert( _stage < MAX_SAMPLERS );				\
		AddTextureTransition( TEXTURE_STATE_ ## _state, _stage );	\
		++numOps;											\
	}

int CTransitionTable::CreateNormalTransitions( const ShadowState_t& fromState, const ShadowState_t& toState, bool bForce )
{
	int numOps = 0;

	// Special case for alpha blending to eliminate extra transitions
	bool blendEnableDifferent = (toState.m_AlphaBlendEnable != fromState.m_AlphaBlendEnable);
	bool srcBlendDifferent = toState.m_AlphaBlendEnable && (toState.m_SrcBlend != fromState.m_SrcBlend);
	bool destBlendDifferent = toState.m_AlphaBlendEnable && (toState.m_DestBlend != fromState.m_DestBlend);
	bool blendOpDifferent = toState.m_AlphaBlendEnable && ( toState.m_BlendOp != fromState.m_BlendOp );
	if (bForce || blendOpDifferent || blendEnableDifferent || srcBlendDifferent || destBlendDifferent)
	{
		AddTransition( RENDER_STATE_AlphaBlend );
		++numOps;
	}

	// Shouldn't have m_SeparateAlphaBlendEnable set unless m_AlphaBlendEnable is also set.
	Assert ( toState.m_AlphaBlendEnable || !toState.m_SeparateAlphaBlendEnable );
	bool blendSeparateAlphaEnableDifferent = (toState.m_SeparateAlphaBlendEnable != fromState.m_SeparateAlphaBlendEnable);
	bool srcBlendAlphaDifferent = toState.m_SeparateAlphaBlendEnable && (toState.m_SrcBlendAlpha != fromState.m_SrcBlendAlpha);
	bool destBlendAlphaDifferent = toState.m_SeparateAlphaBlendEnable && (toState.m_DestBlendAlpha != fromState.m_DestBlendAlpha);
	bool blendOpAlphaDifferent = toState.m_SeparateAlphaBlendEnable && ( toState.m_BlendOpAlpha != fromState.m_BlendOpAlpha );
	if (bForce || blendOpAlphaDifferent || blendSeparateAlphaEnableDifferent || srcBlendAlphaDifferent || destBlendAlphaDifferent)
	{
		AddTransition( RENDER_STATE_SeparateAlphaBlend );
		++numOps;
	}

	bool bAlphaTestEnableDifferent = (toState.m_AlphaTestEnable != fromState.m_AlphaTestEnable);
	bool bAlphaFuncDifferent = toState.m_AlphaTestEnable && (toState.m_AlphaFunc != fromState.m_AlphaFunc);
	bool bAlphaRefDifferent = toState.m_AlphaTestEnable && (toState.m_AlphaRef != fromState.m_AlphaRef);
	if (bForce || bAlphaTestEnableDifferent || bAlphaFuncDifferent || bAlphaRefDifferent)
	{
		AddTransition( RENDER_STATE_AlphaTest );
		++numOps;
	}

	bool bDepthTestEnableDifferent = (toState.m_ZEnable != fromState.m_ZEnable);
	bool bDepthFuncDifferent = (toState.m_ZEnable != D3DZB_FALSE) && (toState.m_ZFunc != fromState.m_ZFunc);
	bool bDepthBiasDifferent = (toState.m_ZBias != fromState.m_ZBias);
	if (bForce || bDepthTestEnableDifferent || bDepthFuncDifferent || bDepthBiasDifferent)
	{
		AddTransition( RENDER_STATE_DepthTest );
		++numOps;
	}

	if ( bForce || (toState.m_UsingFixedFunction && !fromState.m_UsingFixedFunction) )
	{
		AddTransition( RENDER_STATE_ActivateFixedFunction );
		++numOps;
	}

	if ( bForce || (toState.m_bDisableFogGammaCorrection != fromState.m_bDisableFogGammaCorrection) )
	{
		AddTransition( RENDER_STATE_DisableFogGammaCorrection );
		++numOps;
	}

	int nStageCount = HardwareConfig()->GetTextureStageCount();
	int i;
	for ( i = 0; i < nStageCount; ++i )
	{
		// Special case for texture stage ops to eliminate extra transitions
		// NOTE: If we're forcing transitions, then ActivateFixedFunction above will take care of all these transitions
		if ( !bForce && toState.m_UsingFixedFunction && fromState.m_UsingFixedFunction )
		{
			const TextureStageShadowState_t& fromTexture = fromState.m_TextureStage[i];
			const TextureStageShadowState_t& toTexture = toState.m_TextureStage[i];

			bool fromEnabled = (fromTexture.m_ColorOp != D3DTOP_DISABLE);
			bool toEnabled = (toTexture.m_ColorOp != D3DTOP_DISABLE);
			if ( fromEnabled || toEnabled )
			{
				bool opDifferent = (toTexture.m_ColorOp != fromTexture.m_ColorOp);
				bool arg1Different = (toTexture.m_ColorArg1 != fromTexture.m_ColorArg1);
				bool arg2Different = (toTexture.m_ColorArg2 != fromTexture.m_ColorArg2);
				if (opDifferent || arg1Different || arg2Different )
				{
					AddTextureTransition( TEXTURE_STATE_ColorTextureStage, i );
					++numOps;
				}
			}

			fromEnabled = (fromTexture.m_AlphaOp != D3DTOP_DISABLE);
			toEnabled = (toTexture.m_AlphaOp != D3DTOP_DISABLE);
			if ( fromEnabled || toEnabled )
			{
				bool opDifferent = (toTexture.m_AlphaOp != fromTexture.m_AlphaOp);
				bool arg1Different = (toTexture.m_AlphaArg1 != fromTexture.m_AlphaArg1);
				bool arg2Different = (toTexture.m_AlphaArg2 != fromTexture.m_AlphaArg2);
				if (opDifferent || arg1Different || arg2Different )
				{
					AddTextureTransition( TEXTURE_STATE_AlphaTextureStage, i );
					++numOps;
				}
			}
		}

		ADD_TEXTURE_STAGE_STATE_TRANSITION( i, TexCoordIndex );
	}

	int nSamplerCount = HardwareConfig()->GetSamplerCount();
	for ( int i = 0; i < nSamplerCount; ++i )
	{
		ADD_SAMPLER_STATE_TRANSITION( i, SRGBReadEnable );
		ADD_SAMPLER_STATE_TRANSITION( i, Fetch4Enable );
#ifdef DX_TO_GL_ABSTRACTION
		ADD_SAMPLER_STATE_TRANSITION( i, ShadowFilterEnable );
#endif
	}

	return numOps;
}

void CTransitionTable::CreateTransitionTableEntry( int to, int from )
{
	// You added or removed a state to the enums but not to the function table lists!
	COMPILE_TIME_ASSERT( sizeof(s_pRenderFunctionTable) == sizeof(ApplyStateFunc_t) * RENDER_STATE_COUNT );
	COMPILE_TIME_ASSERT( sizeof(s_pTextureFunctionTable) == sizeof(ApplyStateFunc_t) * TEXTURE_STATE_COUNT );

	// If from < 0, that means add *all* transitions into it.
	unsigned int firstElem = m_TransitionOps.Count();
	unsigned short numOps = 0;

	const ShadowState_t& toState = m_ShadowStateList[to];
	const ShadowState_t& fromState = (from >= 0) ? m_ShadowStateList[from] : m_ShadowStateList[to];
	bool bForce = (from < 0);

	ADD_RENDER_STATE_TRANSITION( ZWriteEnable )
	ADD_RENDER_STATE_TRANSITION( ColorWriteEnable )
	ADD_RENDER_STATE_TRANSITION( FillMode )
	ADD_RENDER_STATE_TRANSITION( Lighting )
	ADD_RENDER_STATE_TRANSITION( SpecularEnable )
	ADD_RENDER_STATE_TRANSITION( SRGBWriteEnable )
	ADD_RENDER_STATE_TRANSITION( DiffuseMaterialSource )

	// Some code for the non-trivial transitions
	numOps += CreateNormalTransitions( fromState, toState, bForce );

	// NOTE: From here on down are transitions that depend on dynamic state
	// and which can therefore not appear in the state block
	ADD_RENDER_STATE_TRANSITION( CullEnable )
	ADD_RENDER_STATE_TRANSITION( EnableAlphaToCoverage )
	ADD_RENDER_STATE_TRANSITION( VertexBlendEnable )

	// NOTE! : Have to do the extra check for changes in m_UsingFixedFunction
	// since d3d fog state is different if you are using fixed function vs.
	// using a vsh/psh.
	// This code is derived from: ADD_RENDER_STATE_TRANSITION( FogMode )
	// If ADD_RENDER_STATE_TRANSITION ever changes, this needs to be updated!
	// This is another reason to try to have very little fixed function in the dx8/dx9 path.
	if( bForce || (toState.m_FogMode != fromState.m_FogMode ) || 
	    ( toState.m_UsingFixedFunction != fromState.m_UsingFixedFunction ) )
	{
		AddTransition( RENDER_STATE_FogMode );
		++numOps;
	}

	bool bDifferentTexturesEnabled = false;
	int nSamplerCount = HardwareConfig()->GetSamplerCount();
	for ( int i = 0; i < nSamplerCount; ++i )
	{
		if ( toState.m_SamplerState[i].m_TextureEnable != fromState.m_SamplerState[i].m_TextureEnable )
		{
			bDifferentTexturesEnabled = true;
			break;
		}
	}

	if ( bForce || bDifferentTexturesEnabled )
	{
		AddTransition( RENDER_STATE_TextureEnable );
		++numOps;
	}

	// Look for identical transition lists, and use those instead...
	TransitionList_t& transition = (from >= 0) ? 
							m_TransitionTable[to][from] : m_DefaultTransition;
	Assert( numOps <= 255 );
	transition.m_NumOperations = numOps;

	// This condition can happen, and is valid. It occurs when we snapshot
	// state but do not generate a transition function for that state
	if (numOps == 0)
	{
		transition.m_FirstOperation = INVALID_TRANSITION_OP;
		return;
	}

	// An optimization to try to early out of the identical transition check
	// taking advantage of the fact that the matrix is usually diagonal.
	unsigned int nFirstTest = INVALID_TRANSITION_OP;
	if (from >= 0)
	{
		TransitionList_t &diagonalList = m_TransitionTable[from][to]; 
		if ( diagonalList.m_NumOperations == numOps )
		{
			nFirstTest = diagonalList.m_FirstOperation;
		}
	}

	unsigned int identicalListFirstElem = FindIdenticalTransitionList( firstElem, numOps, nFirstTest ); 
	if (identicalListFirstElem == INVALID_TRANSITION_OP)
	{
		transition.m_FirstOperation = firstElem;
		m_UniqueTransitions.Insert( transition );
		Assert( (int)firstElem + (int)numOps < 16777215 );

		if( (int)firstElem + (int)numOps >= 16777215 )
		{
			Warning("**** WARNING: Transition table overflow. Grab Brian\n");
		}
	}
	else
	{
		// Remove the transitions ops we made; use the duplicate copy
		transition.m_FirstOperation = identicalListFirstElem;
		m_TransitionOps.RemoveMultiple( firstElem, numOps );
	}
}


//-----------------------------------------------------------------------------
// Tests a snapshot to see if it can be used
//-----------------------------------------------------------------------------

#define PERFORM_RENDER_STATE_TRANSITION( _state, _func )	\
	::Apply ## _func( _state, 0 );
#define PERFORM_TEXTURE_STAGE_STATE_TRANSITION( _state, _stage, _func )	\
	::Apply ## _func( _state, _stage );
#define PERFORM_SAMPLER_STATE_TRANSITION( _state, _stage, _func )	\
	::Apply ## _func( _state, _stage );

bool CTransitionTable::TestShadowState( const ShadowState_t& state, const ShadowShaderState_t &shaderState )
{
	PERFORM_RENDER_STATE_TRANSITION( state, DepthTest )
	PERFORM_RENDER_STATE_TRANSITION( state, ZWriteEnable )
	PERFORM_RENDER_STATE_TRANSITION( state, ColorWriteEnable )
	PERFORM_RENDER_STATE_TRANSITION( state, AlphaTest )
	PERFORM_RENDER_STATE_TRANSITION( state, FillMode )
	PERFORM_RENDER_STATE_TRANSITION( state, Lighting )
	PERFORM_RENDER_STATE_TRANSITION( state, SpecularEnable )
	PERFORM_RENDER_STATE_TRANSITION( state, SRGBWriteEnable )
	PERFORM_RENDER_STATE_TRANSITION( state, AlphaBlend )
	PERFORM_RENDER_STATE_TRANSITION( state, SeparateAlphaBlend )
	PERFORM_RENDER_STATE_TRANSITION( state, CullEnable )
	PERFORM_RENDER_STATE_TRANSITION( state, AlphaToCoverage )
	PERFORM_RENDER_STATE_TRANSITION( state, VertexBlendEnable )
	PERFORM_RENDER_STATE_TRANSITION( state, FogMode )
	PERFORM_RENDER_STATE_TRANSITION( state, ActivateFixedFunction )
	PERFORM_RENDER_STATE_TRANSITION( state, TextureEnable )
	PERFORM_RENDER_STATE_TRANSITION( state, DiffuseMaterialSource )

	int i;
	int nStageCount = HardwareConfig()->GetTextureStageCount();
	for ( i = 0; i < nStageCount; ++i )
	{
		PERFORM_TEXTURE_STAGE_STATE_TRANSITION( state, i, ColorTextureStage );
		PERFORM_TEXTURE_STAGE_STATE_TRANSITION( state, i, AlphaTextureStage );
		PERFORM_TEXTURE_STAGE_STATE_TRANSITION( state, i, TexCoordIndex );
	}

	int nSamplerCount = HardwareConfig()->GetSamplerCount();
	for ( i = 0; i < nSamplerCount; ++i )
	{
		PERFORM_SAMPLER_STATE_TRANSITION( state, i, SRGBReadEnable );
		PERFORM_SAMPLER_STATE_TRANSITION( state, i, Fetch4Enable );
#ifdef DX_TO_GL_ABSTRACTION
		PERFORM_SAMPLER_STATE_TRANSITION( state, i, ShadowFilterEnable );
#endif
	}

	// Just make sure we've got a good snapshot
	RECORD_COMMAND( DX8_VALIDATE_DEVICE, 0 );

#if !defined( _X360 )
	DWORD numPasses;
	HRESULT hr = Dx9Device()->ValidateDevice( &numPasses );
	bool ok = !FAILED(hr);
#else
	bool ok = true;
#endif

	// Now set the board state to match the default state
	ApplyTransition( m_DefaultTransition, m_DefaultStateSnapshot );

	ShaderManager()->SetVertexShader( shaderState.m_VertexShader );
	ShaderManager()->SetPixelShader( shaderState.m_PixelShader );

	return ok;
}

//-----------------------------------------------------------------------------
// Finds identical transition lists and shares them 
//-----------------------------------------------------------------------------
unsigned int CTransitionTable::FindIdenticalTransitionList( unsigned int firstElem, 
					unsigned short numOps, unsigned int nFirstTest ) const
{
	VPROF("CTransitionTable::FindIdenticalTransitionList");
	// As it turns out, this works most of the time
	if ( nFirstTest != INVALID_TRANSITION_OP )
	{
		const TransitionOp_t *pCurrOp = &m_TransitionOps[firstElem];
		const TransitionOp_t *pTestOp = &m_TransitionOps[nFirstTest];
		if ( !memcmp( pCurrOp, pTestOp, numOps * sizeof(TransitionOp_t) ) )
			return nFirstTest;	
	}

	// Look for a common list
	const TransitionOp_t &op = m_TransitionOps[firstElem];

	int nCount = m_UniqueTransitions.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		const TransitionList_t &list = m_UniqueTransitions[i];

		// We can early out here because we've sorted the unique transitions
		// descending by count 
		if ( list.m_NumOperations < numOps )
			return INVALID_TRANSITION_OP;

		// If we don't find a match in the first 
		int nPotentialMatch;
		int nLastTest = list.m_FirstOperation + list.m_NumOperations - numOps;
		for ( nPotentialMatch = list.m_FirstOperation; nPotentialMatch <= nLastTest; ++nPotentialMatch )
		{
			// Find the first match
			const TransitionOp_t &testOp = m_TransitionOps[nPotentialMatch];
			if ( testOp.m_nBits == op.m_nBits )
				break;
		}

		// No matches found, continue
		if ( nPotentialMatch > nLastTest )
			continue;

		// Ok, found a match of the first op, lets see if they all match
		if ( numOps == 1 )
			return nPotentialMatch;

		const TransitionOp_t *pCurrOp = &m_TransitionOps[firstElem + 1];
		const TransitionOp_t *pTestOp = &m_TransitionOps[nPotentialMatch + 1];
		if ( !memcmp( pCurrOp, pTestOp, (numOps - 1) * sizeof(TransitionOp_t) ) )
			return nPotentialMatch;	
	}
	return INVALID_TRANSITION_OP;
}


//-----------------------------------------------------------------------------
// Create startup snapshot
//-----------------------------------------------------------------------------
void CTransitionTable::TakeDefaultStateSnapshot( )
{
	if (m_DefaultStateSnapshot == -1)
	{
		m_DefaultStateSnapshot = TakeSnapshot();

		// This will create a transition which sets *all* shadowed state
		CreateTransitionTableEntry( m_DefaultStateSnapshot, -1 );
	}
}


//-----------------------------------------------------------------------------
// Applies the transition list
//-----------------------------------------------------------------------------
void CTransitionTable::ApplyTransitionList( int snapshot, int nFirstOp, int nOpCount )
{
	VPROF("CTransitionTable::ApplyTransitionList");
	// Don't bother if there's nothing to do
	if (nOpCount > 0)
	{
		// Trying to avoid function overhead here
		ShadowState_t& shadowState = m_ShadowStateList[snapshot];
		TransitionOp_t* pTransitionOp = &m_TransitionOps[nFirstOp];

		for (int i = 0; i < nOpCount; ++i )
		{
			// invoke the transition method
			if ( pTransitionOp->m_nInfo.m_bIsTextureCode )
			{
				TextureStateFunc_t code;
				int nStage;
				GetTextureOp( pTransitionOp->m_nInfo.m_nOpCode, &code, &nStage );
				(*s_pTextureFunctionTable[code])( shadowState, nStage );
			}
			else
			{
				(*s_pRenderFunctionTable[pTransitionOp->m_nInfo.m_nOpCode])( shadowState, 0 );
			}
			++pTransitionOp;
		}
	}
}


//-----------------------------------------------------------------------------
// Apply startup snapshot
//-----------------------------------------------------------------------------
#ifdef _WIN32
#pragma warning( disable : 4189 )
#endif

void CTransitionTable::ApplyTransition( TransitionList_t& list, int snapshot )
{
	VPROF("CTransitionTable::ApplyTransition");
	if ( g_pShaderDeviceDx8->IsDeactivated() )
		return;

	// Transition lists when using state blocks have 2 parts: the first
	// is the stateblock part, which is states that are not related to
	// dynamic state at all; followed by states that *are* affected by dynamic state
	int nFirstOp = list.m_FirstOperation;
	int nOpCount = list.m_NumOperations;

	ApplyTransitionList( snapshot, nFirstOp, nOpCount );

	// Semi-hacky code to override what the transitions are doing
	PerformShadowStateOverrides();

	// Set the current snapshot id
	m_CurrentShadowId = snapshot;

#ifdef DEBUG_BOARD_STATE
	// Copy over the board states that aren't explicitly in the transition table
	// so the assertion works...

	int i;
	int nSamplerCount = HardwareConfig()->GetSamplerCount();
	for ( i = nSamplerCount; i < MAX_SAMPLERS; ++i )
	{
		m_BoardState.m_SamplerState[i].m_TextureEnable = 
			CurrentShadowState()->m_SamplerState[i].m_TextureEnable;
	}

	int nTextureStageCount = HardwareConfig()->GetTextureStageCount();
	for ( i = nTextureStageCount; i < MAX_TEXTURE_STAGES; ++i )
	{
		memcpy( &m_BoardState.m_TextureStage[i], &CurrentShadowState()->m_TextureStage[i], sizeof(TextureStageShadowState_t) );
	}
	m_BoardState.m_UsingFixedFunction = CurrentShadowState()->m_UsingFixedFunction;

	// State blocks bypass the code that sets the board state
#ifdef _DEBUG
	// NOTE: A memcmp here isn't enough since we don't set alpha args in cases where the op is nothing.
	// Assert( !memcmp( &m_BoardState, &CurrentShadowState(), sizeof(m_BoardState) ) );
	const ShadowState_t &testState1 = *CurrentShadowState();
	ShadowState_t testState2 = m_BoardState;

	if ( testState1.m_ZEnable == D3DZB_FALSE )
	{
		testState2.m_ZBias = testState1.m_ZBias;
		testState2.m_ZFunc = testState1.m_ZFunc;
	}

	if ( !testState1.m_AlphaTestEnable )
	{
		testState2.m_AlphaRef = testState1.m_AlphaRef;
		testState2.m_AlphaFunc = testState1.m_AlphaFunc;
	}
	for( i = 0; i < nTextureStageCount; i++ )
	{
		if ( !testState1.m_UsingFixedFunction )
		{
			testState2.m_TextureStage[i].m_ColorOp = testState1.m_TextureStage[i].m_ColorOp;
			testState2.m_TextureStage[i].m_ColorArg1 = testState1.m_TextureStage[i].m_ColorArg1;
			testState2.m_TextureStage[i].m_ColorArg2 = testState1.m_TextureStage[i].m_ColorArg2;
			testState2.m_TextureStage[i].m_AlphaOp = testState1.m_TextureStage[i].m_AlphaOp;
			testState2.m_TextureStage[i].m_AlphaArg1 = testState1.m_TextureStage[i].m_AlphaArg1;
			testState2.m_TextureStage[i].m_AlphaArg2 = testState1.m_TextureStage[i].m_AlphaArg2;
		}
		else
		{
			if ( testState1.m_TextureStage[i].m_ColorOp == D3DTOP_DISABLE )
			{
				testState2.m_TextureStage[i].m_ColorArg1 = testState1.m_TextureStage[i].m_ColorArg1;
				testState2.m_TextureStage[i].m_ColorArg2 = testState1.m_TextureStage[i].m_ColorArg2;
			}
			if ( testState1.m_TextureStage[i].m_AlphaOp == D3DTOP_DISABLE )
			{
				testState2.m_TextureStage[i].m_AlphaArg1 = testState1.m_TextureStage[i].m_AlphaArg1;
				testState2.m_TextureStage[i].m_AlphaArg2 = testState1.m_TextureStage[i].m_AlphaArg2;
			}
		}
	}

	Assert( !memcmp( &testState1, &testState2, sizeof( testState1 ) ) );
#endif
#endif
}

#ifdef _WIN32
#pragma warning( default : 4189 )
#endif

//-----------------------------------------------------------------------------
// Takes a snapshot, hooks it into the material
//-----------------------------------------------------------------------------
StateSnapshot_t CTransitionTable::TakeSnapshot( )
{
	// Do any final computation of the shadow state
	ShaderShadow()->ComputeAggregateShadowState();

	// Get the current snapshot
	const ShadowState_t& currentState = ShaderShadow()->GetShadowState();

	// Create a new snapshot
	ShadowStateId_t shadowStateId = FindShadowState( currentState );
	if (shadowStateId == -1)
	{
		// Create entry in state transition table
		shadowStateId = CreateShadowState( currentState );

		// Now create new transition entries
		for (int to = 0; to < shadowStateId; ++to)
		{
			CreateTransitionTableEntry( to, shadowStateId );
		}

		for (int from = 0; from < shadowStateId; ++from)
		{
			CreateTransitionTableEntry( shadowStateId, from );
		}
	}

	const ShadowShaderState_t& currentShaderState = ShaderShadow()->GetShadowShaderState();
 	StateSnapshot_t snapshotId = FindStateSnapshot( shadowStateId, currentShaderState );
	if (snapshotId == -1)
	{
		// Create entry in state transition table
		snapshotId = CreateStateSnapshot( shadowStateId, currentShaderState );
	}

	return snapshotId;
}


//-----------------------------------------------------------------------------
// Apply shader state (stuff that doesn't lie in the transition table)
//-----------------------------------------------------------------------------
void CTransitionTable::ApplyShaderState( const ShadowState_t &shadowState, const ShadowShaderState_t &shaderState )
{
	VPROF("CTransitionTable::ApplyShaderState");
	// Don't bother testing against the current state because there
	// could well be dynamic state modifiers affecting this too....
	if ( !shadowState.m_UsingFixedFunction )
	{
		// FIXME: Improve early-binding of vertex shader index
		ShaderManager()->SetVertexShader( shaderState.m_VertexShader );
		ShaderManager()->SetPixelShader( shaderState.m_PixelShader );

#ifdef DEBUG_BOARD_STATE
		BoardShaderState().m_VertexShader = shaderState.m_VertexShader;
		BoardShaderState().m_PixelShader = shaderState.m_PixelShader;
		BoardShaderState().m_nStaticVshIndex = shaderState.m_nStaticVshIndex;
		BoardShaderState().m_nStaticPshIndex = shaderState.m_nStaticPshIndex;
#endif
	}
	else
	{
		ShaderManager()->SetVertexShader( INVALID_SHADER );
		ShaderManager()->SetPixelShader( INVALID_SHADER );
#if defined( _X360 )
		// no fixed function support
		Assert( 0 );
#endif

#ifdef DEBUG_BOARD_STATE
		BoardShaderState().m_VertexShader = INVALID_SHADER;
		BoardShaderState().m_PixelShader = INVALID_SHADER;
		BoardShaderState().m_nStaticVshIndex = 0;
		BoardShaderState().m_nStaticPshIndex = 0;
#endif
	}
}

//-----------------------------------------------------------------------------
// Makes the board state match the snapshot
//-----------------------------------------------------------------------------
void CTransitionTable::UseSnapshot( StateSnapshot_t snapshotId )
{
	VPROF("CTransitionTable::UseSnapshot");
	ShadowStateId_t id = m_SnapshotList[snapshotId].m_ShadowStateId;
	if (m_CurrentSnapshotId != snapshotId)
	{
		// First apply things that are in the transition table
		if ( m_CurrentShadowId != id )
		{
			TransitionList_t& transition = m_TransitionTable[id][m_CurrentShadowId];
			ApplyTransition( transition, id );
		}

		// NOTE: There is an opportunity here to set non-dynamic state that we don't
		// store in the transition list if we ever need it.

		m_CurrentSnapshotId = snapshotId;
	}

	// NOTE: This occurs regardless of whether the snapshot changed because it depends
	// on dynamic state (namely, the dynamic vertex + pixel shader index)
	// Followed by things that are not
	ApplyShaderState( m_ShadowStateList[id], m_SnapshotList[snapshotId].m_ShaderState );

#ifdef _DEBUG
	// NOTE: We can't ship with this active because mod makers may well violate this rule
	// We don't want no stinking fixed-function on hardware that has vertex and pixel shaders. . 
	// This could cause a serious perf hit. 
	if( HardwareConfig()->SupportsVertexAndPixelShaders() )
	{
//		Assert( !CurrentShadowState().m_UsingFixedFunction );
	}
#endif
}


//-----------------------------------------------------------------------------
// Cause the board to match the default state snapshot
//-----------------------------------------------------------------------------
void CTransitionTable::UseDefaultState( )
{
	VPROF("CTransitionTable::UseDefaultState");
	// Need to blat these out because they are tested during transitions
	m_CurrentState.m_AlphaBlendEnable = false;
	m_CurrentState.m_SrcBlend = D3DBLEND_ONE;
	m_CurrentState.m_DestBlend = D3DBLEND_ZERO;
	m_CurrentState.m_BlendOp = D3DBLENDOP_ADD;
	SetRenderStateConstMacro( D3DRS_ALPHABLENDENABLE, m_CurrentState.m_AlphaBlendEnable );
	SetRenderStateConstMacro( D3DRS_SRCBLEND, m_CurrentState.m_SrcBlend );
	SetRenderStateConstMacro( D3DRS_DESTBLEND, m_CurrentState.m_DestBlend );
	SetRenderStateConstMacro( D3DRS_BLENDOP, m_CurrentState.m_BlendOp );

	m_CurrentState.m_SeparateAlphaBlendEnable = false;
	m_CurrentState.m_SrcBlendAlpha = D3DBLEND_ONE;
	m_CurrentState.m_DestBlendAlpha = D3DBLEND_ZERO;
	m_CurrentState.m_BlendOpAlpha = D3DBLENDOP_ADD;
	SetRenderStateConstMacro( D3DRS_SEPARATEALPHABLENDENABLE, m_CurrentState.m_SeparateAlphaBlendEnable );
	SetRenderStateConstMacro( D3DRS_SRCBLENDALPHA, m_CurrentState.m_SrcBlendAlpha );
	SetRenderStateConstMacro( D3DRS_DESTBLENDALPHA, m_CurrentState.m_DestBlendAlpha );
	SetRenderStateConstMacro( D3DRS_BLENDOPALPHA, m_CurrentState.m_BlendOpAlpha );

	m_CurrentState.m_ZEnable = D3DZB_TRUE;
	m_CurrentState.m_ZFunc = D3DCMP_LESSEQUAL;
	m_CurrentState.m_ZBias = SHADER_POLYOFFSET_DISABLE;
	SetRenderStateConstMacro( D3DRS_ZENABLE, m_CurrentState.m_ZEnable );
#if defined( _X360 )
	//SetRenderStateConstMacro( D3DRS_HIZENABLE, m_CurrentState.m_ZEnable ? D3DHIZ_AUTOMATIC : D3DHIZ_DISABLE );
#endif
	SetRenderStateConstMacro( D3DRS_ZFUNC, m_CurrentState.m_ZFunc );

	m_CurrentState.m_AlphaTestEnable = false;
	m_CurrentState.m_AlphaFunc = D3DCMP_GREATEREQUAL;
	m_CurrentState.m_AlphaRef = 0;
	SetRenderStateConstMacro( D3DRS_ALPHATESTENABLE, m_CurrentState.m_AlphaTestEnable );
	SetRenderStateConstMacro( D3DRS_ALPHAFUNC, m_CurrentState.m_AlphaFunc );
	SetRenderStateConstMacro( D3DRS_ALPHAREF, m_CurrentState.m_AlphaRef );

	int nTextureStages = ShaderAPI()->GetActualTextureStageCount();
	for ( int i = 0; i < nTextureStages; ++i)
	{
		TextureStage(i).m_ColorOp = D3DTOP_DISABLE;
		TextureStage(i).m_ColorArg1 = D3DTA_TEXTURE;
		TextureStage(i).m_ColorArg2 = (i == 0) ? D3DTA_DIFFUSE : D3DTA_CURRENT;
		TextureStage(i).m_AlphaOp = D3DTOP_DISABLE;
		TextureStage(i).m_AlphaArg1 = D3DTA_TEXTURE;
		TextureStage(i).m_AlphaArg2 = (i == 0) ? D3DTA_DIFFUSE : D3DTA_CURRENT;

		SetTextureStageState( i, D3DTSS_COLOROP,	TextureStage(i).m_ColorOp );
		SetTextureStageState( i, D3DTSS_COLORARG1,	TextureStage(i).m_ColorArg1 );
		SetTextureStageState( i, D3DTSS_COLORARG2,	TextureStage(i).m_ColorArg2 );
		SetTextureStageState( i, D3DTSS_ALPHAOP,	TextureStage(i).m_AlphaOp );
		SetTextureStageState( i, D3DTSS_ALPHAARG1,	TextureStage(i).m_AlphaArg1 );
		SetTextureStageState( i, D3DTSS_ALPHAARG2,	TextureStage(i).m_AlphaArg2 );
	}

	int nSamplerCount = ShaderAPI()->GetActualSamplerCount();
	for ( int i = 0; i < nSamplerCount; ++i)
	{
		SetSamplerState( i, D3DSAMP_SRGBTEXTURE, SamplerState(i).m_SRGBReadEnable );

		// Set default Fetch4 state on parts which support it
		if ( ShaderAPI()->SupportsFetch4() )
		{
			SetSamplerState( i, ATISAMP_FETCH4, SamplerState(i).m_Fetch4Enable ? ATI_FETCH4_ENABLE : ATI_FETCH4_DISABLE );
		}
		
#ifdef DX_TO_GL_ABSTRACTION
		SetSamplerState( i, D3DSAMP_SHADOWFILTER, SamplerState(i).m_ShadowFilterEnable );
#endif
	}

	// Disable z overrides...
	m_CurrentState.m_bOverrideDepthEnable = false;
	m_CurrentState.m_bOverrideAlphaWriteEnable = false;
	m_CurrentState.m_bOverrideColorWriteEnable = false;
	m_CurrentState.m_ForceDepthFuncEquals = false;
	m_CurrentState.m_bLinearColorSpaceFrameBufferEnable = false;
	ApplyTransition( m_DefaultTransition, m_DefaultStateSnapshot );

	ShaderManager()->SetVertexShader( INVALID_SHADER );
	ShaderManager()->SetPixelShader( INVALID_SHADER );

	m_CurrentSnapshotId = -1;
}


//-----------------------------------------------------------------------------
// Snapshotted state overrides
//-----------------------------------------------------------------------------
void CTransitionTable::ForceDepthFuncEquals( bool bEnable )
{
	if( bEnable != m_CurrentState.m_ForceDepthFuncEquals )
	{
		// Do this so that we can call this from within the rendering code
		// See OverrideDepthEnable + PerformShadowStateOverrides for a version
		// that isn't expected to be called from within rendering code
		if( !ShaderAPI()->IsRenderingMesh() )
		{
			ShaderAPI()->FlushBufferedPrimitives();
		}

		m_CurrentState.m_ForceDepthFuncEquals = bEnable;

		if( bEnable )
		{
			SetZFunc( D3DCMP_EQUAL );
		}
		else
		{
			if ( CurrentShadowState() )
			{
				SetZFunc( CurrentShadowState()->m_ZFunc );
			}
		}
	}
}

void CTransitionTable::OverrideDepthEnable( bool bEnable, bool bDepthEnable )
{
	if ( bEnable != m_CurrentState.m_bOverrideDepthEnable )
	{
		ShaderAPI()->FlushBufferedPrimitives();
		m_CurrentState.m_bOverrideDepthEnable = bEnable;
		m_CurrentState.m_OverrideZWriteEnable = bDepthEnable ? D3DZB_TRUE : D3DZB_FALSE;

		if ( m_CurrentState.m_bOverrideDepthEnable )
		{
			SetZEnable( D3DZB_TRUE );
			SetRenderStateConstMacro( D3DRS_ZWRITEENABLE, m_CurrentState.m_OverrideZWriteEnable );
#if defined( _X360 )
			//SetRenderStateConstMacro( D3DRS_HIZWRITEENABLE, m_CurrentState.m_OverrideZWriteEnable ? D3DHIZ_AUTOMATIC : D3DHIZ_DISABLE );
#endif
		}
		else
		{
			if ( CurrentShadowState() )
			{
				SetZEnable( CurrentShadowState()->m_ZEnable );
				SetRenderStateConstMacro( D3DRS_ZWRITEENABLE, CurrentShadowState()->m_ZWriteEnable );
#if defined( _X360 )
				//SetRenderStateConstMacro( D3DRS_HIZWRITEENABLE, CurrentShadowState()->m_ZWriteEnable ? D3DHIZ_AUTOMATIC : D3DHIZ_DISABLE );
#endif
			}
		}
	}
}

void CTransitionTable::OverrideAlphaWriteEnable( bool bOverrideEnable, bool bAlphaWriteEnable )
{
	if ( bOverrideEnable != m_CurrentState.m_bOverrideAlphaWriteEnable )
	{
		ShaderAPI()->FlushBufferedPrimitives();
		m_CurrentState.m_bOverrideAlphaWriteEnable = bOverrideEnable;
		m_CurrentState.m_bOverriddenAlphaWriteValue = bAlphaWriteEnable;

		DWORD dwSetValue = m_CurrentState.m_ColorWriteEnable;
		if ( m_CurrentState.m_bOverrideAlphaWriteEnable )
		{			
			if( m_CurrentState.m_bOverriddenAlphaWriteValue )
			{
				dwSetValue |= D3DCOLORWRITEENABLE_ALPHA;
			}
			else
			{
				dwSetValue &= ~D3DCOLORWRITEENABLE_ALPHA;
			}
		}
		else
		{
			if ( CurrentShadowState() )
			{
				//probably being paranoid, but only copy the alpha flag from the shadow state
				dwSetValue &= ~D3DCOLORWRITEENABLE_ALPHA;
				dwSetValue |= CurrentShadowState()->m_ColorWriteEnable & D3DCOLORWRITEENABLE_ALPHA;
			}
		}

		if( dwSetValue != m_CurrentState.m_ColorWriteEnable )
		{
			m_CurrentState.m_ColorWriteEnable = dwSetValue;
			SetRenderState( D3DRS_COLORWRITEENABLE, m_CurrentState.m_ColorWriteEnable );
		}
	}
}

void CTransitionTable::OverrideColorWriteEnable( bool bOverrideEnable, bool bColorWriteEnable )
{
	if ( bOverrideEnable != m_CurrentState.m_bOverrideColorWriteEnable )
	{
		ShaderAPI()->FlushBufferedPrimitives();
		m_CurrentState.m_bOverrideColorWriteEnable = bOverrideEnable;
		m_CurrentState.m_bOverriddenColorWriteValue = bColorWriteEnable;

		DWORD dwSetValue = m_CurrentState.m_ColorWriteEnable;
		if ( m_CurrentState.m_bOverrideColorWriteEnable )
		{			
			if( m_CurrentState.m_bOverriddenColorWriteValue )
			{
				dwSetValue |= (D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE);
			}
			else
			{
				dwSetValue &= ~(D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE);
			}
		}
		else
		{
			if ( CurrentShadowState() )
			{
				//probably being paranoid, but only copy the alpha flag from the shadow state
				dwSetValue &= ~(D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE);
				dwSetValue |= CurrentShadowState()->m_ColorWriteEnable & (D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE);
			}
		}

		if( dwSetValue != m_CurrentState.m_ColorWriteEnable )
		{
			m_CurrentState.m_ColorWriteEnable = dwSetValue;
			SetRenderState( D3DRS_COLORWRITEENABLE, m_CurrentState.m_ColorWriteEnable );
		}
	}
}

void CTransitionTable::EnableLinearColorSpaceFrameBuffer( bool bEnable )
{
	if ( m_CurrentState.m_bLinearColorSpaceFrameBufferEnable != bEnable && CurrentShadowState() )
	{
		ShaderAPI()->FlushBufferedPrimitives();
		m_CurrentState.m_bLinearColorSpaceFrameBufferEnable = bEnable;
		ApplySRGBWriteEnable( *CurrentShadowState() );	
	}
}

//-----------------------------------------------------------------------------
// Perform state block overrides
//-----------------------------------------------------------------------------
void CTransitionTable::PerformShadowStateOverrides( )
{
	VPROF("CTransitionTable::PerformShadowStateOverrides");
	// Deal with funky overrides here, because the state blocks can't...
	if ( m_CurrentState.m_ForceDepthFuncEquals )
	{
		SetZFunc( D3DCMP_EQUAL );
	}

	if ( m_CurrentState.m_bOverrideDepthEnable )
	{
		SetZEnable( D3DZB_TRUE );
		SetRenderStateConstMacro( D3DRS_ZWRITEENABLE, m_CurrentState.m_OverrideZWriteEnable );
#if defined( _X360 )
		//SetRenderStateConstMacro( D3DRS_HIZWRITEENABLE, m_CurrentState.m_OverrideZWriteEnable ? D3DHIZ_AUTOMATIC : D3DHIZ_DISABLE );
#endif
	}

	if ( m_CurrentState.m_bOverrideAlphaWriteEnable )
	{
		DWORD dwSetValue = m_CurrentState.m_ColorWriteEnable & ~D3DCOLORWRITEENABLE_ALPHA;
		dwSetValue |= m_CurrentState.m_bOverriddenAlphaWriteValue ? D3DCOLORWRITEENABLE_ALPHA : 0;
		if ( dwSetValue != m_CurrentState.m_ColorWriteEnable )
		{
			m_CurrentState.m_ColorWriteEnable = dwSetValue;
			SetRenderState( D3DRS_COLORWRITEENABLE, m_CurrentState.m_ColorWriteEnable );
		}
	}

	if ( m_CurrentState.m_bOverrideColorWriteEnable )
	{
		DWORD dwSetValue = m_CurrentState.m_ColorWriteEnable & ~(D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE);
		dwSetValue |= m_CurrentState.m_bOverriddenColorWriteValue ? (D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE) : 0;
		if ( dwSetValue != m_CurrentState.m_ColorWriteEnable )
		{
			m_CurrentState.m_ColorWriteEnable = dwSetValue;
			SetRenderState( D3DRS_COLORWRITEENABLE, m_CurrentState.m_ColorWriteEnable );
		}
	}
}
