//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#ifndef SHADERAPIDX8_H
#define SHADERAPIDX8_H

#include "shaderapibase.h"
#include "shaderapi/ishadershadow.h"
#include "locald3dtypes.h"

//-----------------------------------------------------------------------------
// Vendor-specific defines
//-----------------------------------------------------------------------------
#define ATI_FETCH4_ENABLE		MAKEFOURCC('G','E','T','4')
#define ATI_FETCH4_DISABLE		MAKEFOURCC('G','E','T','1')
#define ATISAMP_FETCH4			D3DSAMP_MIPMAPLODBIAS

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CMeshBase;
class CMeshBuilder;
struct ShadowState_t;
class IMaterialInternal;


//-----------------------------------------------------------------------------
// State that matters to buffered meshes... (for debugging only)
//-----------------------------------------------------------------------------
struct BufferedState_t
{
	D3DXMATRIX m_Transform[3];
	D3DVIEWPORT9 m_Viewport;
	int m_BoundTexture[16];
	void *m_VertexShader;
	void *m_PixelShader;
};


//-----------------------------------------------------------------------------
// The DX8 shader API
//-----------------------------------------------------------------------------
// FIXME: Remove this! Either move them into CShaderAPIBase or CShaderAPIDx8
class IShaderAPIDX8 : public CShaderAPIBase
{
public:
	// Draws the mesh
	virtual void DrawMesh( CMeshBase *pMesh ) = 0;

	// Draw the mesh with the currently bound vertex and index buffers.
	virtual void DrawWithVertexAndIndexBuffers( void ) = 0;

	// modifies the vertex data when necessary
	virtual void ModifyVertexData( ) = 0;

	// Gets the current buffered state... (debug only)
	virtual void GetBufferedState( BufferedState_t &state ) = 0;

	// Gets the current backface cull state....
	virtual D3DCULL GetCullMode() const = 0;

	// Measures fill rate
	virtual void ComputeFillRate() = 0;

	// Selection mode methods
	virtual bool IsInSelectionMode() const = 0;

	// We hit somefin in selection mode
	virtual void RegisterSelectionHit( float minz, float maxz ) = 0;

	// Get the currently bound material
	virtual IMaterialInternal* GetBoundMaterial() = 0;

	// These methods are called by the transition table
	// They depend on dynamic state so can't exist inside the transition table
	virtual void ApplyZBias( const ShadowState_t &shaderState ) = 0;
	virtual void ApplyTextureEnable( const ShadowState_t &state, int stage ) = 0;
	virtual void ApplyCullEnable( bool bEnable ) = 0;
	virtual void SetVertexBlendState( int numBones ) = 0;
	virtual void ApplyFogMode( ShaderFogMode_t fogMode, bool bSRGBWritesEnabled, bool bDisableGammaCorrection ) = 0;

	virtual int GetActualTextureStageCount() const = 0;
	virtual int GetActualSamplerCount() const = 0;

	virtual bool IsRenderingMesh() const = 0;

	// Fog methods...
	virtual void FogMode( MaterialFogMode_t fogMode ) = 0;

	virtual int GetCurrentFrameCounter( void ) const = 0;

	// Workaround hack for visualization of selection mode
	virtual void SetupSelectionModeVisualizationState() = 0;

	virtual bool UsingSoftwareVertexProcessing() const = 0;

	//notification that the SRGB write state is being changed
	virtual void EnabledSRGBWrite( bool bEnabled ) = 0;

	// Alpha to coverage
	virtual void ApplyAlphaToCoverage( bool bEnable ) = 0;

#if defined( _X360 )
	virtual void ApplySRGBReadState( int iTextureStage, bool bSRGBReadEnabled ) = 0; //360 needs to rebind the texture over again instead of setting a sampler state
#endif

	virtual void PrintfVA( char *fmt, va_list vargs ) = 0;
	virtual void Printf( PRINTF_FORMAT_STRING const char *fmt, ... ) = 0;	
	virtual float Knob( char *knobname, float *setvalue = NULL ) = 0;
};


#endif // SHADERAPIDX8_H
