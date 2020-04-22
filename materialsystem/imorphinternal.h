//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef IMORPHINTERNAL_H
#define IMORPHINTERNAL_H

#ifdef _WIN32
#pragma once
#endif

#include "materialsystem/imorph.h"
#include "shaderapi/shareddefs.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class ITextureInternal;


//-----------------------------------------------------------------------------
// Render context for morphs
//-----------------------------------------------------------------------------
abstract_class IMorphMgrRenderContext
{
public:
};


//-----------------------------------------------------------------------------
//
// Morph data class 
//
//-----------------------------------------------------------------------------
abstract_class IMorphInternal : public IMorph
{
public:
	virtual void Init( MorphFormat_t format, const char *pDebugName ) = 0;
	virtual void Bind( IMorphMgrRenderContext *pRenderContext ) = 0;
	virtual MorphFormat_t GetMorphFormat() const = 0;
};


//-----------------------------------------------------------------------------
// Morph manager class
//-----------------------------------------------------------------------------
abstract_class IMorphMgr
{
public:
	// Allocate,free scratch textures + materials
	virtual bool ShouldAllocateScratchTextures() = 0;
	virtual void AllocateScratchTextures() = 0;
	virtual void FreeScratchTextures() = 0;
	virtual void AllocateMaterials() = 0;
	virtual void FreeMaterials() = 0;

	// Returns the morph accumulator scratch texture
	virtual ITextureInternal *MorphAccumulator() = 0;
	virtual ITextureInternal *MorphWeights() = 0;

	// Class factory
	virtual IMorphInternal *CreateMorph() = 0;
	virtual void DestroyMorph( IMorphInternal *pMorphData ) = 0;

	// Max morphs between Begin/End
	virtual int MaxHWMorphBatchCount() const = 0;

	// Begin, end morph accumulation phase
	virtual void BeginMorphAccumulation( IMorphMgrRenderContext *pRenderContext ) = 0;
	virtual void EndMorphAccumulation( IMorphMgrRenderContext *pRenderContext ) = 0;

	// Accumulate a morph
	virtual void AccumulateMorph( IMorphMgrRenderContext *pRenderContext, IMorph* pMorph, int nMorphCount, const MorphWeight_t* pWeights ) = 0;

	// Advances frame count (for debugging)
	virtual void AdvanceFrame() = 0;

	// Returns the location of a particular vertex in the morph accumulator
	virtual bool GetMorphAccumulatorTexCoord( IMorphMgrRenderContext *pRenderContext, Vector2D *pTexCoord, IMorph *pMorph, int nVertex ) = 0;

	// Allocate, free morph mgr render context data.
	virtual IMorphMgrRenderContext *AllocateRenderContext() = 0;
	virtual void FreeRenderContext( IMorphMgrRenderContext *pRenderContext ) = 0;
};

extern IMorphMgr *g_pMorphMgr;

#endif // IMORPHINTERNAL_H
