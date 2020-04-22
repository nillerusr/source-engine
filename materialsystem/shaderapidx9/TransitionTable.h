//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef TRANSITION_TABLE_H
#define TRANSITION_TABLE_H

#ifdef _WIN32
#pragma once
#endif

#include "utlvector.h"
#include "shadershadowdx8.h"
#include "UtlSortVector.h"
#include "checksum_crc.h"
#include "shaderapi/ishaderapi.h"

// Required for DEBUG_BOARD_STATE
#include "shaderapidx8_global.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
struct IDirect3DStateBlock9;
//-----------------------------------------------------------------------------
// Enumeration for ApplyStateFunc_ts
//-----------------------------------------------------------------------------
// Any function that does not require a texture stage
// NOTE: If you change this, change the function table s_pRenderFunctionTable[] below!!
enum RenderStateFunc_t
{
	RENDER_STATE_DepthTest = 0,
	RENDER_STATE_ZWriteEnable,
	RENDER_STATE_ColorWriteEnable,
	RENDER_STATE_AlphaTest,
	RENDER_STATE_FillMode,
	RENDER_STATE_Lighting,
	RENDER_STATE_SpecularEnable,
	RENDER_STATE_SRGBWriteEnable,
	RENDER_STATE_AlphaBlend,
	RENDER_STATE_SeparateAlphaBlend,
	RENDER_STATE_CullEnable,
	RENDER_STATE_VertexBlendEnable,
	RENDER_STATE_FogMode,
	RENDER_STATE_ActivateFixedFunction,
	RENDER_STATE_TextureEnable,
	RENDER_STATE_DiffuseMaterialSource,
	RENDER_STATE_DisableFogGammaCorrection,
	RENDER_STATE_EnableAlphaToCoverage,
	
	RENDER_STATE_COUNT,
};


// Any function that requires a texture stage
// NOTE: If you change this, change the function table s_pTextureFunctionTable[] below!!
enum TextureStateFunc_t	
{
	TEXTURE_STATE_TexCoordIndex = 0,
	TEXTURE_STATE_SRGBReadEnable,
	TEXTURE_STATE_Fetch4Enable,
#ifdef DX_TO_GL_ABSTRACTION
	TEXTURE_STATE_ShadowFilterEnable,
#endif	
	// Fixed function states
	TEXTURE_STATE_ColorTextureStage,
	TEXTURE_STATE_AlphaTextureStage,
	TEXTURE_STATE_COUNT
};


//-----------------------------------------------------------------------------
// Types related to transition table entries
//-----------------------------------------------------------------------------
typedef void (*ApplyStateFunc_t)( const ShadowState_t& shadowState, int arg );


//-----------------------------------------------------------------------------
// The DX8 implementation of the transition table
//-----------------------------------------------------------------------------
class CTransitionTable
{
public:
	struct CurrentTextureStageState_t
	{
		D3DTEXTUREOP			m_ColorOp;
		int						m_ColorArg1;
		int						m_ColorArg2;
		D3DTEXTUREOP			m_AlphaOp;
		int						m_AlphaArg1;
		int						m_AlphaArg2;
	};
	struct CurrentSamplerState_t
	{
		bool					m_SRGBReadEnable;
		bool					m_Fetch4Enable;
		bool					m_ShadowFilterEnable;
	};
	struct CurrentState_t
	{
		// Everything in this 'CurrentState' structure is a state whose value we don't care about
		// under certain circumstances, (which therefore can diverge from the shadow state),
		// or states which we override in the dynamic pass.

		// Alpha state
		bool				m_AlphaBlendEnable;
		D3DBLEND			m_SrcBlend;
		D3DBLEND			m_DestBlend;
		D3DBLENDOP			m_BlendOp;

		// GR - Separate alpha state
		bool				m_SeparateAlphaBlendEnable;
		D3DBLEND			m_SrcBlendAlpha;
		D3DBLEND			m_DestBlendAlpha;
		D3DBLENDOP			m_BlendOpAlpha;

		// Depth testing states
		D3DZBUFFERTYPE		m_ZEnable;
		D3DCMPFUNC			m_ZFunc;
		PolygonOffsetMode_t	m_ZBias;

		// Alpha testing states
		bool				m_AlphaTestEnable;
		D3DCMPFUNC			m_AlphaFunc;
		int					m_AlphaRef;

		bool				m_ForceDepthFuncEquals;
		bool				m_bOverrideDepthEnable;
		D3DZBUFFERTYPE		m_OverrideZWriteEnable;

		bool				m_bOverrideAlphaWriteEnable;
		bool				m_bOverriddenAlphaWriteValue;
		bool				m_bOverrideColorWriteEnable;
		bool				m_bOverriddenColorWriteValue;
		DWORD				m_ColorWriteEnable;

		bool				m_bLinearColorSpaceFrameBufferEnable;

		bool				m_StencilEnable;
		D3DCMPFUNC			m_StencilFunc;
		int					m_StencilRef;
		int					m_StencilMask;
		DWORD				m_StencilFail;
		DWORD				m_StencilZFail;
		DWORD				m_StencilPass;
		int					m_StencilWriteMask;

		// Texture stage state
		CurrentTextureStageState_t m_TextureStage[MAX_TEXTURE_STAGES];
		CurrentSamplerState_t m_SamplerState[MAX_SAMPLERS];
	};

public:
	// constructor, destructor
	CTransitionTable( );
	virtual ~CTransitionTable();

	// Initialization, shutdown
	bool Init( );
	void Shutdown( );

	// Resets the snapshots...
	void Reset();

	// Takes a snapshot
	StateSnapshot_t TakeSnapshot( );

	// Take startup snapshot
	void TakeDefaultStateSnapshot( );

	// Makes the board state match the snapshot
	void UseSnapshot( StateSnapshot_t snapshotId );

	// Cause the board to match the default state snapshot
	void UseDefaultState();

	// Snapshotted state overrides
	void ForceDepthFuncEquals( bool bEnable );
	void OverrideDepthEnable( bool bEnable, bool bDepthEnable );
	void OverrideAlphaWriteEnable( bool bOverrideEnable, bool bAlphaWriteEnable );
	void OverrideColorWriteEnable( bool bOverrideEnable, bool bColorWriteEnable );
	void EnableLinearColorSpaceFrameBuffer( bool bEnable );

	// Returns a particular snapshot
	const ShadowState_t &GetSnapshot( StateSnapshot_t snapshotId ) const;
	const ShadowShaderState_t &GetSnapshotShader( StateSnapshot_t snapshotId ) const;

	// Gets the current shadow state
	const ShadowState_t *CurrentShadowState() const;
	const ShadowShaderState_t *CurrentShadowShaderState() const;

	// Return the current shapshot
	int CurrentSnapshot() const { return m_CurrentSnapshotId; }

	CurrentState_t& CurrentState() { return m_CurrentState; }

#ifdef DEBUG_BOARD_STATE
	ShadowState_t& BoardState() { return m_BoardState; }
	ShadowShaderState_t& BoardShaderState() { return m_BoardShaderState; }
#endif

	// The following are meant to be used by the transition table only
public:
	// Applies alpha blending
	void ApplyAlphaBlend( const ShadowState_t& state );
	// GR - separate alpha blend
	void ApplySeparateAlphaBlend( const ShadowState_t& state );
	void ApplyAlphaTest( const ShadowState_t& state );
	void ApplyDepthTest( const ShadowState_t& state );

	// Applies alpha texture op
	void ApplyColorTextureStage( const ShadowState_t& state, int stage );
	void ApplyAlphaTextureStage( const ShadowState_t& state, int stage );

	void ApplySRGBWriteEnable( const ShadowState_t& state );
private:
	enum
	{
		INVALID_TRANSITION_OP = 0xFFFFFF
	};

	typedef short ShadowStateId_t;

	// For the transition table
	struct TransitionList_t
	{
		unsigned int m_FirstOperation : 24;
		unsigned int m_NumOperations : 8;
	};

	union TransitionOp_t
	{
		unsigned char m_nBits;
		struct
		{
			unsigned char m_nOpCode : 7;
			unsigned char m_bIsTextureCode : 1;
		} m_nInfo;
	};

	struct SnapshotShaderState_t
	{
		ShadowShaderState_t m_ShaderState;
		ShadowStateId_t m_ShadowStateId;
		unsigned short m_nReserved;	// Pad to 2 ints
		unsigned int m_nReserved2;
	};

	struct ShadowStateDictEntry_t
	{
		CRC32_t	m_nChecksum;
		ShadowStateId_t m_nShadowStateId;
	};

	struct SnapshotDictEntry_t
	{
		CRC32_t	m_nChecksum;
		StateSnapshot_t m_nSnapshot;
	};

	class ShadowStateDictLessFunc
	{
	public:
		bool Less( const ShadowStateDictEntry_t &src1, const ShadowStateDictEntry_t &src2, void *pCtx );
	};

	class SnapshotDictLessFunc
	{
	public:
		bool Less( const SnapshotDictEntry_t &src1, const SnapshotDictEntry_t &src2, void *pCtx );
	};

	class UniqueSnapshotLessFunc
	{
	public:
		bool Less( const TransitionList_t &src1, const TransitionList_t &src2, void *pCtx );
	};

	CurrentTextureStageState_t &TextureStage( int stage ) { return m_CurrentState.m_TextureStage[stage]; }
	const CurrentTextureStageState_t &TextureStage( int stage ) const { return m_CurrentState.m_TextureStage[stage]; }

	CurrentSamplerState_t &SamplerState( int stage ) { return m_CurrentState.m_SamplerState[stage]; }
	const CurrentSamplerState_t &SamplerState( int stage ) const { return m_CurrentState.m_SamplerState[stage]; }

	// creates state snapshots
	ShadowStateId_t  CreateShadowState( const ShadowState_t &currentState );
	StateSnapshot_t  CreateStateSnapshot( ShadowStateId_t shadowStateId, const ShadowShaderState_t& currentShaderState );

	// finds state snapshots
	ShadowStateId_t FindShadowState( const ShadowState_t& currentState ) const;
	StateSnapshot_t FindStateSnapshot( ShadowStateId_t id, const ShadowShaderState_t& currentState ) const;

	// Finds identical transition lists
	unsigned int FindIdenticalTransitionList( unsigned int firstElem, 
		unsigned short numOps, unsigned int nFirstTest ) const;

	// Adds a transition
	void AddTransition( RenderStateFunc_t func );
	void AddTextureTransition( TextureStateFunc_t func, int stage );

	// Apply a transition
	void ApplyTransition( TransitionList_t& list, int snapshot );

	// Creates an entry in the transition table
	void CreateTransitionTableEntry( int to, int from );

	// Checks if a state is valid
	bool TestShadowState( const ShadowState_t& state, const ShadowShaderState_t &shaderState );

	// Perform state block overrides
	void PerformShadowStateOverrides( );

	// Applies the transition list
	void ApplyTransitionList( int snapshot, int nFirstOp, int nOpCount );

	// Apply shader state (stuff that doesn't lie in the transition table)
	void ApplyShaderState( const ShadowState_t &shadowState, const ShadowShaderState_t &shaderState );

	// Wrapper for the non-standard transitions for stateblock + non-stateblock cases
	int CreateNormalTransitions( const ShadowState_t& fromState, const ShadowState_t& toState, bool bForce );

	// State setting methods
	void SetZEnable( D3DZBUFFERTYPE nEnable );
	void SetZFunc( D3DCMPFUNC nCmpFunc );

private:
	// Sets up the default state
	StateSnapshot_t m_DefaultStateSnapshot;
	TransitionList_t m_DefaultTransition;
	ShadowState_t m_DefaultShadowState;
	
	// The current snapshot id
	ShadowStateId_t m_CurrentShadowId;
	StateSnapshot_t m_CurrentSnapshotId;

	// Maintains a list of all used snapshot transition states
	CUtlVector< ShadowState_t >	m_ShadowStateList;

	// Lookup table for fast snapshot finding
	CUtlSortVector< ShadowStateDictEntry_t, ShadowStateDictLessFunc >	m_ShadowStateDict;

	// The snapshot transition table
	CUtlVector< CUtlVector< TransitionList_t > >	m_TransitionTable;

	// List of unique transitions
	CUtlSortVector< TransitionList_t, UniqueSnapshotLessFunc >	m_UniqueTransitions;

	// Stores all state transition operations
	CUtlVector< TransitionOp_t > m_TransitionOps;

	// Stores all state for a particular snapshot
	CUtlVector< SnapshotShaderState_t >	m_SnapshotList;

	// Lookup table for fast snapshot finding
	CUtlSortVector< SnapshotDictEntry_t, SnapshotDictLessFunc >	m_SnapshotDict;

	// The current board state.
	CurrentState_t m_CurrentState;

#ifdef DEBUG_BOARD_STATE
	// Maintains the total shadow state
	ShadowState_t m_BoardState;
	ShadowShaderState_t m_BoardShaderState;
#endif
};


//-----------------------------------------------------------------------------
// Inline methods
//-----------------------------------------------------------------------------
inline const ShadowState_t &CTransitionTable::GetSnapshot( StateSnapshot_t snapshotId ) const
{
	Assert( (snapshotId >= 0) && (snapshotId < m_SnapshotList.Count()) );
	return m_ShadowStateList[m_SnapshotList[snapshotId].m_ShadowStateId];
}

inline const ShadowShaderState_t &CTransitionTable::GetSnapshotShader( StateSnapshot_t snapshotId ) const
{
	Assert( (snapshotId >= 0) && (snapshotId < m_SnapshotList.Count()) );
	return m_SnapshotList[snapshotId].m_ShaderState;
}

inline const ShadowState_t *CTransitionTable::CurrentShadowState() const
{
	if ( m_CurrentShadowId == -1 )
		return NULL;

	Assert( (m_CurrentShadowId >= 0) && (m_CurrentShadowId < m_ShadowStateList.Count()) );
	return &m_ShadowStateList[m_CurrentShadowId];
}

inline const ShadowShaderState_t *CTransitionTable::CurrentShadowShaderState() const
{
	if ( m_CurrentShadowId == -1 )
		return NULL;

	Assert( (m_CurrentShadowId >= 0) && (m_CurrentShadowId < m_ShadowStateList.Count()) );
	return &m_SnapshotList[m_CurrentShadowId].m_ShaderState;
}


#endif // TRANSITION_TABLE_H
