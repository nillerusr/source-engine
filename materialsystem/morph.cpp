//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#include "imorphinternal.h"
#include "tier0/dbg.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/MaterialSystemUtil.h"
#include "materialsystem/itexture.h"
#include "materialsystem/imesh.h"
#include "UtlSortVector.h"
#include "materialsystem_global.h"
#include "IHardwareConfigInternal.h"
#include "pixelwriter.h"
#include "itextureinternal.h"
#include "tier1/KeyValues.h"
#include "texturemanager.h"
#include "imaterialsysteminternal.h"
#include "imatrendercontextinternal.h"
#include "studio.h"
#include "tier0/vprof.h"
#include "renderparm.h"
#include "tier2/renderutils.h"
#include "bitmap/imageformat.h"
#include "materialsystem/IShader.h"
#include "imaterialinternal.h"


#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Activate to get stats
//-----------------------------------------------------------------------------
//#define REPORT_MORPH_STATS 1


//-----------------------------------------------------------------------------
// Used to collapse quads with small gaps
//-----------------------------------------------------------------------------
#define MIN_SEGMENT_GAP_SIZE 12


//-----------------------------------------------------------------------------
// Used to compile the morph data into a vertex texture
//-----------------------------------------------------------------------------
class CVertexMorphDict
{
public:
	CVertexMorphDict();

	// Adds a morph to the dictionary
	void AddMorph( const MorphVertexInfo_t &info );

	// Sets up, cleans up the morph information
	void Setup( );
	void CleanUp();

	// Gets at morph info
	int MorphCount() const;
	int GetMorphTargetId( int nMorphTargetIndex ) const;
	int GetMorphVertexCount( int nMorphTargetIndex ) const;
	const MorphVertexInfo_t &GetMorphVertexInfo( int nMorphTargetIndex, int nIndex ) const;

	// Sorts deltas by destination vertex
	void SortDeltas();

private:
	// Sort method for each morph target's vertices
	class CMorphVertexListLess
	{
	public:
		bool Less( const MorphVertexInfo_t& src1, const MorphVertexInfo_t& src2, void *pCtx )
		{
			return src1.m_nVertexId < src2.m_nVertexId;
		}
	};

	// A list of all vertices affecting a particular morph target
	struct MorphVertexList_t
	{
		MorphVertexList_t() {}
		MorphVertexList_t( const MorphVertexList_t& src ) : m_nMorphTargetId( src.m_nMorphTargetId ) {}

		int m_nMorphTargetId;
		CUtlSortVector< MorphVertexInfo_t, CMorphVertexListLess > m_MorphInfo;
	};

	// Sort function for the morph lists
	class VertexMorphDictLess
	{
	public:
		bool Less( const MorphVertexList_t& src1, const MorphVertexList_t& src2, void *pCtx );
	};

	// For each morph, store all target vertex indices 
	// List of all morphs affecting all vertices, used for constructing the morph only
	CUtlSortVector<	MorphVertexList_t, VertexMorphDictLess > m_MorphLists;
};


//-----------------------------------------------------------------------------
// Used to sort the morphs affecting a particular vertex
//-----------------------------------------------------------------------------
bool CVertexMorphDict::VertexMorphDictLess::Less( const CVertexMorphDict::MorphVertexList_t& src1, const CVertexMorphDict::MorphVertexList_t& src2, void *pCtx )
{
	return src1.m_nMorphTargetId < src2.m_nMorphTargetId;
}


//-----------------------------------------------------------------------------
// Dictionary of morphs affecting a particular vertex
//-----------------------------------------------------------------------------
CVertexMorphDict::CVertexMorphDict() : m_MorphLists()
{
}


//-----------------------------------------------------------------------------
// Adds a morph to the dictionary
//-----------------------------------------------------------------------------
void CVertexMorphDict::AddMorph( const MorphVertexInfo_t &info )
{
	Assert( info.m_nVertexId != 65535 );

	MorphVertexList_t find;
	find.m_nMorphTargetId = info.m_nMorphTargetId;
	int nIndex = m_MorphLists.Find( find );
	if ( nIndex == m_MorphLists.InvalidIndex() )
	{
		m_MorphLists.Insert( find );
		nIndex = m_MorphLists.Find( find );
	}

	m_MorphLists[nIndex].m_MorphInfo.InsertNoSort( info );
}


//-----------------------------------------------------------------------------
// Sets up, cleans up the morph information
//-----------------------------------------------------------------------------
void CVertexMorphDict::Setup(  )
{
	m_MorphLists.Purge();
}

void CVertexMorphDict::CleanUp(  )
{
}


//-----------------------------------------------------------------------------
// Gets at the dictionary elemenst
//-----------------------------------------------------------------------------
int CVertexMorphDict::MorphCount() const
{
	return m_MorphLists.Count();
}

int CVertexMorphDict::GetMorphTargetId( int i ) const
{
	if ( i >= m_MorphLists.Count() )
		return -1;

	return m_MorphLists[i].m_nMorphTargetId;
}

int CVertexMorphDict::GetMorphVertexCount( int nMorphTarget ) const
{
	return m_MorphLists[nMorphTarget].m_MorphInfo.Count();
}

const MorphVertexInfo_t &CVertexMorphDict::GetMorphVertexInfo( int nMorphTarget, int j ) const
{
	return m_MorphLists[nMorphTarget].m_MorphInfo[j];
}


//-----------------------------------------------------------------------------
// Sorts deltas by destination vertex
//-----------------------------------------------------------------------------
void CVertexMorphDict::SortDeltas()
{
	int nMorphTargetCount = m_MorphLists.Count();
	for ( int i = 0; i < nMorphTargetCount; ++i )
	{
		m_MorphLists[i].m_MorphInfo.RedoSort();
	}
}


//-----------------------------------------------------------------------------
//
// Morph data class 
//
//-----------------------------------------------------------------------------
class CMorph : public IMorphInternal, public ITextureRegenerator
{
public:
	// Constructor, destructor
	CMorph();
	~CMorph();

	// Inherited from IMorph
	virtual void Lock( float flFloatToFixedScale );
	virtual void AddMorph( const MorphVertexInfo_t &info );
	virtual void Unlock(  );

	// Inherited from IMorphInternal
	virtual void Init( MorphFormat_t format, const char *pDebugName );
	virtual void Bind( IMorphMgrRenderContext *pRenderContext );
	virtual MorphFormat_t GetMorphFormat() const;

	// Other public methods
	bool RenderMorphWeights( IMatRenderContext *pRenderContext, int nRenderId, int nWeightCount, const MorphWeight_t* pWeights );
	void AccumulateMorph( int nRenderId );

private:
	// A list of all morphs affecting a particular vertex
	// Assume that consecutive morphs are stored under each other in V coordinates
	// both in the src texture and destination texture (which is the morph accumulation texture).
	struct MorphSegment_t
	{
		unsigned int m_nFirstSrc;
		unsigned short m_nFirstDest;
		unsigned short m_nCount;
	};

	struct MorphQuad_t
	{
		unsigned int m_nFirstSrc;
		unsigned short m_nFirstDest;
		unsigned short m_nCount;
		unsigned short m_nQuadIndex;
	};

	enum MorphTextureId_t
	{
		MORPH_TEXTURE_POS_NORMAL_DELTA = 0,
		MORPH_TEXTURE_SPEED_SIDE_MAP,

		MORPH_TEXTURE_COUNT
	};

	typedef void (CMorph::*MorphPixelWriter_t)( CPixelWriter &pixelWriter, int x, int y, const MorphVertexInfo_t &info );

	typedef CUtlVector< MorphSegment_t > MorphSegmentList_t;
	typedef CUtlVector< MorphQuad_t > MorphQuadList_t;

private:
	// Inherited from ITextureRegenerator
	virtual void RegenerateTextureBits( ITexture *pTexture, IVTFTexture *pVTFTexture, Rect_t *pRect );
	virtual void Release() {}

	// Packs all morph data in the dictionary into a vertex texture layout
	void PackMorphData( );

	// Builds the list of segments to render, returns total # of src texels to read from
	void BuildSegmentList( CUtlVector< MorphSegmentList_t > &morphSegments );

	// Builds the list of quads to render
	void BuildQuadList( const CUtlVector< MorphSegmentList_t > &morphSegments );

	// Computes the vertex texture width
	void ComputeTextureDimensions( const CUtlVector< MorphSegmentList_t > &morphSegments );

	// Writes a morph delta into the texture
	void WriteDeltaPositionNormalToTexture( CPixelWriter &pixelWriter, int x, int y, const MorphVertexInfo_t &info );
	void WriteSideSpeedToTexture( CPixelWriter &pixelWriter, int x, int y, const MorphVertexInfo_t &info );

	// Computes the morph target 4tuple count
	int Get4TupleCount( MorphFormat_t format ) const;

	// Cleans up vertex textures
	void CleanUp( );

	// Is the morph locked?
	bool IsLocked() const;

	// Creates a material for use to do the morph accumulation
	void CreateAccumulatorMaterial( int nMaterialIndex );

	// Renders to the morph accumulator texture
	void RenderMorphQuads( IMatRenderContext *pRenderContext, int nRenderId, int nTotalQuadCount, int nWeightCount, int *pWeightLookup, const MorphWeight_t* pWeights );

	// Displays static morph data statistics
	void DisplayMorphStats();

	// Dynamic stat data
	void ClearMorphStats();
	void AccumulateMorphStats( int nActiveMorphCount, int nQuadsRendered, int nTexelsRendered );
	void ReportMorphStats( );
	void HandleMorphStats( int nActiveMorphCount, int nQuadsRendered, int nTexelsRendered );

	// Computes morph texture size in bytes
	int ComputeMorphTextureSizeInBytes( ) const;

	// Counts the total number of vertices to place in the static mesh
	int CountStaticMeshVertices() const;

	// Determines mesh vertex format
	VertexFormat_t ComputeVertexFormat( IMaterial * pMaterial ) const;

	// Builds the list of quads to render
	void CreateStaticMesh();

	// Builds a list of non-zero morph targets
	int BuildNonZeroMorphList( int *pWeightIndices, int nWeightCount, const MorphWeight_t* pWeights );

	// Determines the total number of deltas
	int DetermineTotalDeltaCount( const CUtlVector< MorphSegmentList_t > &morphSegments ) const;

	// Binds the morph weight texture
	void BindMorphWeight( int nRenderId );

private:
	// Used when constructing the morph targets
	CVertexMorphDict m_MorphDict;
	bool m_bLocked;

	// The morph format
	MorphFormat_t m_Format;

	// The compiled vertex textures
	ITextureInternal *m_pMorphTexture[MORPH_TEXTURE_COUNT];
	
	// The compiled vertex streams
	IMesh* m_pMorphBuffer;

	// Describes all morph line segments required to draw a particular morph
	CUtlVector< MorphQuadList_t > m_MorphQuads;
	CUtlVector< int > m_MorphTargetIdToQuadIndex;

	// Caches off the morph weights when in the middle of performing morph accumulation
	int m_nMaxMorphTargetCount;
	MorphWeight_t *m_pRenderMorphWeight;

	CMaterialReference m_MorphAccumulationMaterial;

	// Float->fixed scale
	float m_flFloatToFixedScale;

	// Morph input texture size
	int m_nTextureWidth;
	int m_nTextureHeight;

#ifdef _DEBUG
	CUtlString m_pDebugName;
#endif

	// Used to unique-ify morph texture names
	static int s_nUniqueId;
};


//-----------------------------------------------------------------------------
// Render context for morphing. Only is used to determine
// where in the morph accumulator to put the texture.
//-----------------------------------------------------------------------------
class CMorphMgrRenderContext : public IMorphMgrRenderContext
{
public:
	enum UnnamedEnumsAreNotLegal
	{
		MAX_MODEL_MORPHS = 4,
	};

	CMorphMgrRenderContext();
	int GetRenderId( CMorph* pMorph );

public:
	int m_nMorphCount;
	CMorph *m_pMorphsToAccumulate[MAX_MODEL_MORPHS];

#ifdef DBGFLAG_ASSERT
	bool m_bInMorphAccumulation;
#endif
};


//-----------------------------------------------------------------------------
// Morph manager class
//-----------------------------------------------------------------------------
class CMorphMgr : public IMorphMgr
{
public:
	CMorphMgr();

	// Methods of IMorphMgr
	virtual bool ShouldAllocateScratchTextures();
	virtual void AllocateScratchTextures();
	virtual void FreeScratchTextures();
	virtual void AllocateMaterials();
	virtual void FreeMaterials();
	virtual ITextureInternal *MorphAccumulator();
	virtual ITextureInternal *MorphWeights();
	virtual IMorphInternal *CreateMorph();
	virtual void DestroyMorph( IMorphInternal *pMorphData );
	virtual int MaxHWMorphBatchCount() const;
	virtual void BeginMorphAccumulation( IMorphMgrRenderContext *pIRenderContext );
	virtual void EndMorphAccumulation( IMorphMgrRenderContext *pIRenderContext );
	virtual void AccumulateMorph( IMorphMgrRenderContext *pIRenderContext, IMorph* pMorph, int nMorphCount, const MorphWeight_t* pWeights );
	virtual void AdvanceFrame();
	virtual bool GetMorphAccumulatorTexCoord( IMorphMgrRenderContext *pRenderContext, Vector2D *pTexCoord, IMorph *pMorph, int nVertex );
	virtual IMorphMgrRenderContext *AllocateRenderContext();
	virtual void FreeRenderContext( IMorphMgrRenderContext *pRenderContext );

	// Other public methods
public:
	// Computes texel offsets for the upper corner of the morph accumulator for a particular block
	void ComputeAccumulatorSubrect( int *pXOffset, int *pYOffset, int *pWidth, int *pHeight, int nMorphAccumBlockId );
	void GetAccumulatorSubrectDimensions( int *pWidth, int *pHeight );
	int GetAccumulator4TupleCount() const;

	// Computes texel offsets for the upper corner of the morph weight texture for a particular block
	void ComputeWeightSubrect( int *pXOffset, int *pYOffset, int *pWidth, int *pHeight, int nMorphAccumBlockId );

	// Used to compute stats of memory used
	void RegisterMorphSizeInBytes( int nSizeInBytes );
	int GetTotalMemoryUsage() const;

	// Are we using the constant register method?
	bool IsUsingConstantRegisters() const { return m_bUsingConstantRegisters; }

private:
	// Displays 32bit float texture data
	void Display32FTextureData( float *pBuf, int nTexelID, int *pSubRect, ITexture *pTexture, int n4TupleCount );

	// A debugging utility to display the morph accumulator
	void DebugMorphAccumulator( IMatRenderContext *pRenderContext );

	// A debugging utility to display the morph weights
	void DebugMorphWeights( IMatRenderContext *pRenderContext );

	// Draws the morph accumulator + morph weights
	void DrawMorphTempTexture( IMatRenderContext *pRenderContext, IMaterial *pMaterial, ITexture *pTexture );

private:
	enum
	{
		MAX_MORPH_ACCUMULATOR_VERTICES = 32768,
		MORPH_ACCUMULATOR_4TUPLES = 2,	// 1 for pos + wrinkle, 1 for normal
	};

	int m_nAccumulatorWidth;
	int m_nAccumulatorHeight;
	int m_nSubrectVerticalCount;
	int m_nWeightWidth;
	int m_nWeightHeight;
	int m_nFrameCount;
	int m_nTotalMorphSizeInBytes;
	IMaterial *m_pPrevMaterial;
	void *m_pPrevProxy;
	int m_nPrevBoneCount;
	MaterialHeightClipMode_t m_nPrevClipMode;
	bool m_bPrevClippingEnabled;
	bool m_bUsingConstantRegisters;
	bool m_bFlashlightMode;

	ITextureInternal *m_pMorphAccumTexture;
	ITextureInternal *m_pMorphWeightTexture;
	IMaterial *m_pVisualizeMorphAccum;
	IMaterial *m_pVisualizeMorphWeight;
	IMaterial *m_pRenderMorphWeight; 
};


//-----------------------------------------------------------------------------
// Singleton
//-----------------------------------------------------------------------------
static CMorphMgr s_MorphMgr;
IMorphMgr *g_pMorphMgr = &s_MorphMgr;



//-----------------------------------------------------------------------------
// Globals
//-----------------------------------------------------------------------------
int CMorph::s_nUniqueId = 0;


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CMorph::CMorph()
{
	memset( m_pMorphTexture, 0, sizeof(m_pMorphTexture) );
	m_pMorphBuffer = NULL;
	m_nTextureWidth = 0;
	m_nTextureHeight = 0;
	m_bLocked = false;
	m_Format = 0;
	m_flFloatToFixedScale = 1.0f;
	m_pRenderMorphWeight = 0;
	m_nMaxMorphTargetCount = 0;
}

CMorph::~CMorph()
{
	CleanUp();
}


//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------
void CMorph::Init( MorphFormat_t format, const char *pDebugName )
{
	m_Format = format;

#ifdef _DEBUG
	m_pDebugName = pDebugName;
#endif
}


//-----------------------------------------------------------------------------
// Returns the morph format
//-----------------------------------------------------------------------------
MorphFormat_t CMorph::GetMorphFormat() const
{
	return m_Format;
}


//-----------------------------------------------------------------------------
// Binds morph accumulator, morph weights
//-----------------------------------------------------------------------------
void CMorph::Bind( IMorphMgrRenderContext *pIRenderContext )
{
	CMorphMgrRenderContext *pMorphRenderContext = static_cast< CMorphMgrRenderContext* >( pIRenderContext );
	int nRenderId = pMorphRenderContext->GetRenderId( this );
	if ( nRenderId < 0 )
		return;

	int nXOffset, nYOffset, nWidth, nHeight;
	s_MorphMgr.ComputeAccumulatorSubrect( &nXOffset, &nYOffset, &nWidth, &nHeight, nRenderId );

	g_pShaderAPI->SetIntRenderingParameter( INT_RENDERPARM_MORPH_ACCUMULATOR_4TUPLE_COUNT, s_MorphMgr.GetAccumulator4TupleCount() );
	g_pShaderAPI->SetIntRenderingParameter( INT_RENDERPARM_MORPH_ACCUMULATOR_X_OFFSET, nXOffset );
	g_pShaderAPI->SetIntRenderingParameter( INT_RENDERPARM_MORPH_ACCUMULATOR_Y_OFFSET, nYOffset );
	g_pShaderAPI->SetIntRenderingParameter( INT_RENDERPARM_MORPH_ACCUMULATOR_SUBRECT_WIDTH, nWidth );
	g_pShaderAPI->SetIntRenderingParameter( INT_RENDERPARM_MORPH_ACCUMULATOR_SUBRECT_HEIGHT, nHeight );
}

void CMorph::BindMorphWeight( int nRenderId )
{
	int nXOffset, nYOffset, nWidth, nHeight;
	s_MorphMgr.ComputeWeightSubrect( &nXOffset, &nYOffset, &nWidth, &nHeight, nRenderId );

	g_pShaderAPI->SetIntRenderingParameter( INT_RENDERPARM_MORPH_WEIGHT_X_OFFSET, nXOffset );
	g_pShaderAPI->SetIntRenderingParameter( INT_RENDERPARM_MORPH_WEIGHT_Y_OFFSET, nYOffset );
	g_pShaderAPI->SetIntRenderingParameter( INT_RENDERPARM_MORPH_WEIGHT_SUBRECT_WIDTH, nWidth );
	g_pShaderAPI->SetIntRenderingParameter( INT_RENDERPARM_MORPH_WEIGHT_SUBRECT_HEIGHT, nHeight );
}


//-----------------------------------------------------------------------------
// Computes morph texture size in bytes
//-----------------------------------------------------------------------------
int CMorph::ComputeMorphTextureSizeInBytes( ) const
{
	int nSize = 0;

	if ( m_pMorphTexture[MORPH_TEXTURE_POS_NORMAL_DELTA] )
	{
		int nTotal4Tuples = Get4TupleCount( m_Format );
		nSize += m_nTextureWidth * m_nTextureHeight * nTotal4Tuples * ImageLoader::SizeInBytes( IMAGE_FORMAT_RGBA16161616 );
	}

	if ( m_pMorphTexture[MORPH_TEXTURE_SPEED_SIDE_MAP] )
	{
		nSize += m_nTextureWidth * m_nTextureHeight * ImageLoader::SizeInBytes( IMAGE_FORMAT_RGBA8888 );
	}

	// NOTE: Vertex size here is kind of a hack, but whatever.
	int nVertexCount = CountStaticMeshVertices();
	nSize += nVertexCount * 5 * sizeof(float);  
	return nSize;
}


//-----------------------------------------------------------------------------
// Cleans up vertex textures
//-----------------------------------------------------------------------------
void CMorph::CleanUp( )
{
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );

	int nMorphTextureSize = ComputeMorphTextureSizeInBytes();
	s_MorphMgr.RegisterMorphSizeInBytes( -nMorphTextureSize );

	IMaterial *pMat = m_MorphAccumulationMaterial;
	m_MorphAccumulationMaterial.Shutdown();
	if ( pMat )
	{
		pMat->DeleteIfUnreferenced();
	}

	if ( m_pMorphBuffer )
	{
		pRenderContext->DestroyStaticMesh( m_pMorphBuffer );
		m_pMorphBuffer = NULL;
	}

	for ( int i = 0; i < MORPH_TEXTURE_COUNT; ++i )
	{
		if ( m_pMorphTexture[i] )
		{
			m_pMorphTexture[i]->SetTextureRegenerator( NULL );
			m_pMorphTexture[i]->DecrementReferenceCount( );
			m_pMorphTexture[i]->DeleteIfUnreferenced();
			m_pMorphTexture[i] = NULL;
		}
	}

	if ( m_pRenderMorphWeight )
	{
		delete[] m_pRenderMorphWeight;
		m_pRenderMorphWeight = NULL;
	}

	m_nMaxMorphTargetCount = 0;
}


//-----------------------------------------------------------------------------
// Is the morph locked?
//-----------------------------------------------------------------------------
bool CMorph::IsLocked() const
{
	return m_bLocked;
}


//-----------------------------------------------------------------------------
// Locks the morph data
//-----------------------------------------------------------------------------
void CMorph::Lock( float flFloatToFixedScale )
{
	Assert( !IsLocked() );
	m_bLocked = true;
	CleanUp();
	m_flFloatToFixedScale = flFloatToFixedScale;
	m_MorphQuads.Purge();
	m_MorphTargetIdToQuadIndex.RemoveAll();
	m_MorphDict.Setup( );
}


//-----------------------------------------------------------------------------
// Adds morph data to the morph dictionary
//-----------------------------------------------------------------------------
void CMorph::AddMorph( const MorphVertexInfo_t &info )
{
	Assert( IsLocked() );
	m_MorphDict.AddMorph( info );
}


//-----------------------------------------------------------------------------
// Unlocks the morph data, builds the vertex textures
//-----------------------------------------------------------------------------
void CMorph::Unlock( )
{
	Assert( IsLocked() );

	// Sort the deltas by destination vertex
	m_MorphDict.SortDeltas();

	// Now lay out morph data as if it were in a vertex texture
	PackMorphData( );

	// Free up temporary memory used in building
	m_MorphDict.CleanUp();

	m_bLocked = false;

	// Gather stats
	int nMorphTextureSize = ComputeMorphTextureSizeInBytes();
	s_MorphMgr.RegisterMorphSizeInBytes( nMorphTextureSize );
}


//-----------------------------------------------------------------------------
// Creates a material for use to do the morph accumulation
//-----------------------------------------------------------------------------
void CMorph::CreateAccumulatorMaterial( int nMaterialIndex )
{
	// NOTE: Delta scale is a little tricky. The numbers are store in fixed-point 16 bit.
	// The pixel shader will interpret 65536 as 1.0, and 0 as 0.0. In the pixel shader,
	// we will read the delta, multiply it by 2, and subtract 1 to get a -1 to 1 range.
	// The float to fixed scale is applied prior to writing it in (delta * scale + 32768).
	// Therefore the max representable positive value = 
	//		65536 = max positive delta * scale + 32768
	//		max positive delta = 32768 / scale
	// This is what we will multiply our -1 to 1 values by in the pixel shader.
	char pTemp[256];
	KeyValues *pVMTKeyValues = new KeyValues( "MorphAccumulate" );
	pVMTKeyValues->SetInt( "$nocull", 1 );
	pVMTKeyValues->SetFloat( "$deltascale", ( m_flFloatToFixedScale != 0.0f ) ? 32768.0f / m_flFloatToFixedScale : 1.0f );
	if ( m_pMorphTexture[MORPH_TEXTURE_POS_NORMAL_DELTA] )
	{
		pVMTKeyValues->SetString( "$delta", m_pMorphTexture[MORPH_TEXTURE_POS_NORMAL_DELTA]->GetName() );
	}
	if ( m_pMorphTexture[MORPH_TEXTURE_SPEED_SIDE_MAP] )
	{
		pVMTKeyValues->SetString( "$sidespeed", m_pMorphTexture[MORPH_TEXTURE_SPEED_SIDE_MAP]->GetName() );
	}
	Q_snprintf( pTemp, sizeof(pTemp), "[%d %d %d]", m_nTextureWidth, m_nTextureHeight, Get4TupleCount(m_Format) );
	pVMTKeyValues->SetString( "$dimensions", pTemp );
	
	Q_snprintf( pTemp, sizeof(pTemp), "___AccumulateMorph%d.vmt", nMaterialIndex );
	m_MorphAccumulationMaterial.Init( pTemp, pVMTKeyValues );
}



//-----------------------------------------------------------------------------
// Computes the morph target field count
//-----------------------------------------------------------------------------
int CMorph::Get4TupleCount( MorphFormat_t format ) const
{
	int nSize = 0;
	if ( format & ( MORPH_POSITION | MORPH_WRINKLE ) )
	{
		++nSize;
	}
	if ( format & MORPH_NORMAL )
	{
		++nSize;
	}
	return nSize;
}


//-----------------------------------------------------------------------------
// Determines the total number of deltas
//-----------------------------------------------------------------------------
int CMorph::DetermineTotalDeltaCount( const CUtlVector< MorphSegmentList_t > &morphSegments ) const
{
	int nDeltaCount = 0;
	int nMorphCount = morphSegments.Count();
	for ( int i = 0; i < nMorphCount; ++i )
	{
		const MorphSegmentList_t& list = morphSegments[i];
		int nSegmentCount = list.Count();
		for ( int j = 0; j < nSegmentCount; ++j )
		{
			nDeltaCount += list[j].m_nCount;	
		}
	}
	return nDeltaCount;
}


//-----------------------------------------------------------------------------
// Computes the texture width
//-----------------------------------------------------------------------------
void CMorph::ComputeTextureDimensions( const CUtlVector< MorphSegmentList_t > &morphSegments )
{
	int nTotalDeltas = DetermineTotalDeltaCount( morphSegments );
	m_nTextureHeight = ceil( sqrt( (float)nTotalDeltas ) );

	// Round the dimension up to a multiple of 4
	m_nTextureHeight = ( m_nTextureHeight + 3 ) & ( ~0x3 );
	m_nTextureWidth = ( m_nTextureHeight != 0 ) ? ( nTotalDeltas + ( m_nTextureHeight - 1 ) ) / m_nTextureHeight : 0;
	m_nTextureWidth = ( m_nTextureWidth + 3 ) & ( ~0x3 );

	int nTotal4Tuples = Get4TupleCount( m_Format );

	// Make sure it obeys bounds
	int nMaxTextureWidth = HardwareConfig()->MaxTextureWidth();
	int nMaxTextureHeight = HardwareConfig()->MaxTextureHeight();
	while( m_nTextureWidth * nTotal4Tuples > nMaxTextureWidth )
	{
		m_nTextureWidth >>= 1;
		m_nTextureHeight <<= 1;
		if ( m_nTextureHeight > nMaxTextureHeight )
		{
			Warning( "Morph texture is too big!!! Make brian add support for morphs having multiple textures.\n" );
			Assert( 0 );
			m_nTextureHeight = nMaxTextureHeight;
			break;
		}
	}
}


//-----------------------------------------------------------------------------
// Displays morph data statistics
//-----------------------------------------------------------------------------
void CMorph::DisplayMorphStats()
{
	ITexture *pDest = g_pMorphMgr->MorphAccumulator( );
	int nDestTextureHeight = pDest->GetActualHeight();

#ifdef _DEBUG
	Msg( "Morph %s:\n", m_pDebugName.Get() );
#else
	Msg( "Morph :\n" );
#endif

	int nMorphCount = m_MorphQuads.Count();
	Msg( "\tMorph Target Count : %d\n", nMorphCount );

	int nTotalQuadCount = 0;
	int nTotalVertexCount = 0;
	CUtlVector<int> quadHisto;
	CUtlVector<int> vertexHisto;
	CUtlVector<int> gapSizeHisto;
	for ( int i = 0; i < nMorphCount; ++i )
	{
		MorphQuadList_t &list = m_MorphQuads[i];
		int nQuadCount = list.Count();
		int nVertexCount = 0;
		for ( int j = 0; j < nQuadCount; ++j )
		{
			nVertexCount += list[j].m_nCount;
			if ( j != 0 )
			{
				// Filter out src gaps + wraparound gaps
				if ( ( list[j].m_nFirstDest / nDestTextureHeight == list[j-1].m_nFirstDest / nDestTextureHeight ) &&
					 ( list[j].m_nFirstSrc / m_nTextureHeight == list[j-1].m_nFirstSrc / m_nTextureHeight ) )
				{
					int nGapSize = list[j].m_nFirstDest - ( list[j-1].m_nFirstDest + list[j-1].m_nCount );
					while ( nGapSize >= gapSizeHisto.Count() )
					{
						gapSizeHisto.AddToTail( 0 );
					}
					gapSizeHisto[nGapSize] += 1;
				}
			}
		}
		while ( nQuadCount >= quadHisto.Count() )
		{
			quadHisto.AddToTail( 0 );
		}
		while ( nVertexCount >= vertexHisto.Count() )
		{
			vertexHisto.AddToTail( 0 );
		}
		quadHisto[nQuadCount]+=1;
		vertexHisto[nVertexCount]+=1;
		nTotalQuadCount += nQuadCount;
		nTotalVertexCount += nVertexCount;
	}

	Msg( "\tAverage # of vertices per target: %d\n", nTotalVertexCount / nMorphCount );
	Msg( "\tAverage # of quad draws per target: %d\n", nTotalQuadCount / nMorphCount );

	Msg( "\tQuad Count Histogram :\n\t\t" );
	for ( int i = 0; i < quadHisto.Count(); ++i )
	{
		if ( quadHisto[i] == 0 )
			continue;
		Msg( "[%d : %d] ", i, quadHisto[i] );
	}
	Msg( "\n\tVertex Count Histogram :\n\t\t" );
	for ( int i = 0; i < vertexHisto.Count(); ++i )
	{
		if ( vertexHisto[i] == 0 )
			continue;
		Msg( "[%d : %d] ", i, vertexHisto[i] );
	}
	Msg( "\n\tGap size Count Histogram :\n\t\t" );
	for ( int i = 0; i < gapSizeHisto.Count(); ++i )
	{
		if ( gapSizeHisto[i] == 0 )
			continue;
		Msg( "[%d : %d] ", i, gapSizeHisto[i] );
	}
	Msg( "\n" );
}


//-----------------------------------------------------------------------------
// Packs all morph data in the dictionary into a vertex texture layout
//-----------------------------------------------------------------------------
void CMorph::PackMorphData( )
{
	CUtlVector< MorphSegmentList_t > morphSegments;
		  
	BuildSegmentList( morphSegments );
	ComputeTextureDimensions( morphSegments );
	BuildQuadList( morphSegments );

	if ( m_nTextureWidth == 0 || m_nTextureHeight == 0 )
		return;

	char pTemp[512];
	if ( m_Format & ( MORPH_POSITION | MORPH_WRINKLE | MORPH_NORMAL ) )
	{
		Q_snprintf( pTemp, sizeof(pTemp), "__morphtarget[%d]: pos/norm", s_nUniqueId );

		int nTotal4Tuples = Get4TupleCount( m_Format );
		ITexture *pTexture = g_pMaterialSystem->CreateProceduralTexture( pTemp, TEXTURE_GROUP_MORPH_TARGETS, 
			m_nTextureWidth * nTotal4Tuples, m_nTextureHeight, IMAGE_FORMAT_RGBA16161616, 
			TEXTUREFLAGS_NOMIP | TEXTUREFLAGS_NOLOD | TEXTUREFLAGS_NODEBUGOVERRIDE |
			TEXTUREFLAGS_SINGLECOPY | TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT | TEXTUREFLAGS_POINTSAMPLE );
		m_pMorphTexture[MORPH_TEXTURE_POS_NORMAL_DELTA] = static_cast<ITextureInternal*>( pTexture );
	}

	if ( m_Format & ( MORPH_SIDE | MORPH_SPEED ) )
	{
		Q_snprintf( pTemp, sizeof(pTemp), "__morphtarget[%d]: side/speed", s_nUniqueId );

		ITexture *pTexture = g_pMaterialSystem->CreateProceduralTexture( pTemp, TEXTURE_GROUP_MORPH_TARGETS, 
			m_nTextureWidth, m_nTextureHeight, IMAGE_FORMAT_RGBA8888, 
			TEXTUREFLAGS_NOMIP | TEXTUREFLAGS_NOLOD | TEXTUREFLAGS_NODEBUGOVERRIDE |
			TEXTUREFLAGS_SINGLECOPY | TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT | TEXTUREFLAGS_POINTSAMPLE );
		m_pMorphTexture[MORPH_TEXTURE_SPEED_SIDE_MAP] = static_cast<ITextureInternal*>( pTexture );
	}

	for ( int i = 0; i < MORPH_TEXTURE_COUNT; ++i )
	{
		if ( m_pMorphTexture[i] )
		{
			m_pMorphTexture[i]->SetTextureRegenerator( this );
			m_pMorphTexture[i]->Download();
		}
	}

	CreateAccumulatorMaterial( s_nUniqueId );
	++s_nUniqueId;

	CreateStaticMesh();

#ifdef REPORT_MORPH_STATS
	DisplayMorphStats( );
#endif
}


//-----------------------------------------------------------------------------
// Writes a morph delta into the texture
//-----------------------------------------------------------------------------
void CMorph::WriteDeltaPositionNormalToTexture( CPixelWriter &pixelWriter, int x, int y, const MorphVertexInfo_t &info )
{
	// NOTE: 0 = -max range, 32767 = 0, 65534, 65535 = maxrange. 
	// This way we can encode +/- maxrange and 0 exactly
	Assert ( m_Format & ( MORPH_POSITION | MORPH_WRINKLE | MORPH_NORMAL ) );

	int n4TupleCount = Get4TupleCount( m_Format );
	pixelWriter.Seek( x*n4TupleCount, y );

	// NOTE: int cast is where it is to force round-to-zero prior to offset
	if ( m_Format & ( MORPH_POSITION | MORPH_WRINKLE ) )
	{
		int r = 32767, g = 32767, b = 32767, a = 32767;
		if ( m_Format & MORPH_POSITION )
		{
			r = (int)( info.m_PositionDelta.x * m_flFloatToFixedScale ) + 32767;
			g = (int)( info.m_PositionDelta.y * m_flFloatToFixedScale ) + 32767;
			b = (int)( info.m_PositionDelta.z * m_flFloatToFixedScale ) + 32767;
			r = clamp( r, 0, 65534 );
			g = clamp( g, 0, 65534 );
			b = clamp( b, 0, 65534 );
		}
		if ( m_Format & MORPH_WRINKLE )
		{
			a = (int)( info.m_flWrinkleDelta * m_flFloatToFixedScale ) + 32767;
			a = clamp( a, 0, 65534 );
		}
		pixelWriter.WritePixel( r, g, b, a );
	}

	if ( m_Format & MORPH_NORMAL )
	{
		int r = 32767, g = 32767, b = 32767, a = 32767;
		r = (int)( info.m_NormalDelta.x * m_flFloatToFixedScale ) + 32767;
		g = (int)( info.m_NormalDelta.y * m_flFloatToFixedScale ) + 32767;
		b = (int)( info.m_NormalDelta.z * m_flFloatToFixedScale ) + 32767;
		r = clamp( r, 0, 65534 );
		g = clamp( g, 0, 65534 );
		b = clamp( b, 0, 65534 );

		pixelWriter.WritePixel( r, g, b, a );
	}
}

void CMorph::WriteSideSpeedToTexture( CPixelWriter &pixelWriter, int x, int y, const MorphVertexInfo_t &info )
{
	Assert ( m_Format & ( MORPH_SPEED | MORPH_SIDE ) );

	// Speed + size go from 0 to 1.
	int r = 0, g = 0, b = 0, a = 0;
	if ( m_Format & MORPH_SIDE )
	{
		r = info.m_flSide * 255;
	}
	if ( m_Format & MORPH_SPEED )
	{
		g = info.m_flSpeed * 255;
	}
	r = clamp( r, 0, 255 );
	g = clamp( g, 0, 255 );

	pixelWriter.Seek( x, y );
	pixelWriter.WritePixel( r, g, b, a );
}


//-----------------------------------------------------------------------------
// Builds the list of segments to render
//-----------------------------------------------------------------------------
void CMorph::BuildSegmentList( CUtlVector< MorphSegmentList_t > &morphSegments )
{
	// Find the dimensions of the destination texture
	int nDestTextureWidth, nDestTextureHeight;
	s_MorphMgr.GetAccumulatorSubrectDimensions( &nDestTextureWidth, &nDestTextureHeight );

	// Prepares the morph segments array
	m_nMaxMorphTargetCount = 0;
	int nMorphTargetCount = m_MorphDict.MorphCount();
	for ( int i = 0; i < nMorphTargetCount; ++i )
	{
		if ( m_nMaxMorphTargetCount <= m_MorphDict.GetMorphTargetId(i) )
		{
			m_nMaxMorphTargetCount = m_MorphDict.GetMorphTargetId(i) + 1;
		}
	}

	// Allocate space to cache off the morph weights when in the middle of performing morph accumulation
	Assert( !m_pRenderMorphWeight );
	m_pRenderMorphWeight = new MorphWeight_t[ m_nMaxMorphTargetCount ];

	Assert( m_nMaxMorphTargetCount < 1024 );	// This algorithm of storing a full array is bogus if this isn't true
	m_MorphTargetIdToQuadIndex.SetCount( m_nMaxMorphTargetCount );
	memset( m_MorphTargetIdToQuadIndex.Base(), 0xFF, m_nMaxMorphTargetCount * sizeof(int) );

	// Builds the segment list
	int nSrcIndex = 0;
	for ( int i = 0; i < nMorphTargetCount; ++i )
	{
		int nMorphTargetId = m_MorphDict.GetMorphTargetId( i );
		m_MorphTargetIdToQuadIndex[nMorphTargetId] = i;

		int nSegmentIndex = morphSegments.AddToTail();
		MorphSegmentList_t &list = morphSegments[nSegmentIndex];
		Assert( nSegmentIndex == i );

		MorphSegment_t segment;
		segment.m_nCount = 0;

		int nVertexCount = m_MorphDict.GetMorphVertexCount( i );
		int nLastDestIndex = -1;
		for ( int j = 0; j < nVertexCount; ++j )
		{
			const MorphVertexInfo_t &info = m_MorphDict.GetMorphVertexInfo( i, j );

			// Check for segment break conditions
			if ( segment.m_nCount )
			{
				// Vertical overflow, non-contiguous destination verts, or contiguous dest
				// verts which happen to lie in different columns are the break conditions
				if ( ( nLastDestIndex < 0 ) || ( info.m_nVertexId > nLastDestIndex + MIN_SEGMENT_GAP_SIZE ) || 
					 ( info.m_nVertexId / nDestTextureHeight != nLastDestIndex / nDestTextureHeight ) )
				{
					list.AddToTail( segment );
					nLastDestIndex = -1;
				}
			}
				  
			// Start new segment, or append to existing segment
			if ( nLastDestIndex < 0 )
			{
				segment.m_nFirstSrc = nSrcIndex;
				segment.m_nFirstDest = info.m_nVertexId;
				segment.m_nCount = 1;
				++nSrcIndex;
			}
			else 
			{
				int nSegmentCount = info.m_nVertexId - nLastDestIndex;
				segment.m_nCount += nSegmentCount;
				nSrcIndex += nSegmentCount;
			}
			nLastDestIndex = info.m_nVertexId;
		}

		// Add any trailing segment
		if ( segment.m_nCount )
		{
			list.AddToTail( segment );
		}
	}
}


//-----------------------------------------------------------------------------
// Builds the list of quads to render
//-----------------------------------------------------------------------------
void CMorph::BuildQuadList( const CUtlVector< MorphSegmentList_t > &morphSegments )
{
	m_MorphQuads.RemoveAll();

	int nQuadIndex = 0;
	int nMorphCount = morphSegments.Count();
	for ( int i = 0; i < nMorphCount; ++i )
	{
		int k = m_MorphQuads.AddToTail();
		MorphQuadList_t &quadList = m_MorphQuads[k];

		const MorphSegmentList_t& segmentList = morphSegments[i];
		int nSegmentCount = segmentList.Count();
		for ( int j = 0; j < nSegmentCount; ++j )
		{
			const MorphSegment_t &segment = segmentList[j];

			int nSrc = segment.m_nFirstSrc;
			int nDest = segment.m_nFirstDest;
			int nTotalCount = segment.m_nCount;

			do
			{
				int sx = nSrc / m_nTextureHeight;
				int sy = nSrc - sx * m_nTextureHeight;

				int nMaxCount = m_nTextureHeight - sy;
				int nCount = min( nMaxCount, nTotalCount );
				nTotalCount -= nCount;

				int l = quadList.AddToTail();
				MorphQuad_t &quad = quadList[l];
				quad.m_nQuadIndex = nQuadIndex++;
				quad.m_nCount = nCount;
				quad.m_nFirstSrc = nSrc;
				quad.m_nFirstDest = nDest;

				nSrc += nCount;
				nDest += nCount;
			} while ( nTotalCount > 0 );
		}
	}
}


//-----------------------------------------------------------------------------
// Counts the total number of vertices to place in the static mesh
//-----------------------------------------------------------------------------
int CMorph::CountStaticMeshVertices() const
{
	// FIXME: I'm doing the simple thing here of 4 verts per segment.
	// I believe I should be able to share any edge that isn't on the edges of the texture
	// so I should be able to get down to nearly 2 (or is it 1?) verts per segment.
	int nVertexCount = 0;
	int nMorphCount = m_MorphQuads.Count();
	for ( int i = 0; i < nMorphCount; ++i )
	{
		const MorphQuadList_t& quadList = m_MorphQuads[i];
		nVertexCount += quadList.Count() * 4;
	}

	return nVertexCount;
}

//-----------------------------------------------------------------------------
// Determines mesh vertex format
//-----------------------------------------------------------------------------
VertexFormat_t CMorph::ComputeVertexFormat( IMaterial * pMaterial ) const
{
	// We believe this material's vertex format is reliable (unlike many others as of June 07)
	VertexFormat_t vertexFormat = pMaterial->GetVertexFormat();

	// UNDONE: optimize the vertex format to compress or remove elements where possible
	vertexFormat &= ~VERTEX_FORMAT_COMPRESSED;

	return vertexFormat;
}

//-----------------------------------------------------------------------------
// Builds the list of segments to render
//-----------------------------------------------------------------------------
void CMorph::CreateStaticMesh()
{
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	m_MorphAccumulationMaterial->Refresh();
	VertexFormat_t vertexFormat = ComputeVertexFormat( m_MorphAccumulationMaterial );
	m_pMorphBuffer = pRenderContext->CreateStaticMesh( vertexFormat, TEXTURE_GROUP_MORPH_TARGETS, m_MorphAccumulationMaterial );

	int nVertexCount = CountStaticMeshVertices();
	if ( nVertexCount >= 65535 )
	{
		Warning( "Too many morph vertices! Call brian\n" );
	}
	Assert( nVertexCount < 65535 );

	int n4TupleCount = Get4TupleCount( m_Format );

	float flOOTexWidth = 1.0f / ( n4TupleCount * m_nTextureWidth );
	float flOOTexHeight = 1.0f / m_nTextureHeight;

	int nDestTextureWidth, nDestTextureHeight;
	s_MorphMgr.GetAccumulatorSubrectDimensions( &nDestTextureWidth, &nDestTextureHeight );
	float flOODestWidth = 1.0f / nDestTextureWidth;
	float flOODestHeight = 1.0f / nDestTextureHeight;

	// NOTE: zero index count implies no index buffer
	CMeshBuilder meshBuilder;
	meshBuilder.Begin( m_pMorphBuffer, MATERIAL_TRIANGLES, nVertexCount, 0 ); 

	int nMorphCount = m_MorphQuads.Count();
	for ( int i = 0; i < nMorphCount; ++i )
	{
		MorphQuadList_t& quadList = m_MorphQuads[i];
		int nQuadCount = quadList.Count();
		for ( int j = 0; j < nQuadCount; ++j )
		{
			MorphQuad_t &quad = quadList[j];

			int sx = quad.m_nFirstSrc / m_nTextureHeight;
			int sy = quad.m_nFirstSrc - sx * m_nTextureHeight;
			int dx = quad.m_nFirstDest / nDestTextureHeight;
			int dy = quad.m_nFirstDest - dx * nDestTextureHeight;
			sx *= n4TupleCount; dx *= n4TupleCount;

			meshBuilder.TexCoord4f( 0, sx * flOOTexWidth, sy * flOOTexHeight, 
				( dx - 0.5f ) * flOODestWidth, ( dy - 0.5f ) * flOODestHeight );	// Stores the source to read from 
			meshBuilder.TexCoord1f( 1, i );
			meshBuilder.AdvanceVertex();

			meshBuilder.TexCoord4f( 0, sx * flOOTexWidth, ( sy + quad.m_nCount ) * flOOTexHeight, 
				( dx - 0.5f ) * flOODestWidth, ( dy + quad.m_nCount - 0.5f ) * flOODestHeight );	// Stores the source to read from 
			meshBuilder.TexCoord1f( 1, i );
			meshBuilder.AdvanceVertex();

			meshBuilder.TexCoord4f( 0, (sx + n4TupleCount) * flOOTexWidth, ( sy + quad.m_nCount ) * flOOTexHeight, 
				( dx + n4TupleCount - 0.5f ) * flOODestWidth, ( dy + quad.m_nCount - 0.5f ) * flOODestHeight );	// Stores the source to read from 
			meshBuilder.TexCoord1f( 1, i );
			meshBuilder.AdvanceVertex();

			meshBuilder.TexCoord4f( 0, (sx + n4TupleCount) * flOOTexWidth, sy * flOOTexHeight, 
				( dx + n4TupleCount - 0.5f ) * flOODestWidth, ( dy - 0.5f ) * flOODestHeight );	// Stores the source to read from 
			meshBuilder.TexCoord1f( 1, i );
			meshBuilder.AdvanceVertex();
		}
	}

	meshBuilder.End();
}


//-----------------------------------------------------------------------------
// Inherited from ITextureRegenerator
//-----------------------------------------------------------------------------
void CMorph::RegenerateTextureBits( ITexture *pTexture, IVTFTexture *pVTFTexture, Rect_t *pRect )
{
	Assert( pVTFTexture->FrameCount() == 1 && pVTFTexture->FaceCount() == 1 );
	Assert( pVTFTexture->Height() == m_nTextureHeight );

	int nTextureType;
	for ( nTextureType = 0; nTextureType < MORPH_TEXTURE_COUNT; ++nTextureType )
	{
		if ( pTexture == m_pMorphTexture[nTextureType] )
			break;
	}
	Assert( nTextureType < MORPH_TEXTURE_COUNT );
	MorphPixelWriter_t pWriteFuncs[MORPH_TEXTURE_COUNT] = 
	{
		&CMorph::WriteDeltaPositionNormalToTexture,
		&CMorph::WriteSideSpeedToTexture,
	};

	MorphPixelWriter_t writeFunc = pWriteFuncs[nTextureType];

	CPixelWriter pixelWriter;
	pixelWriter.SetPixelMemory( pVTFTexture->Format(), pVTFTexture->ImageData(), pVTFTexture->RowSizeInBytes( 0 ) );

	// Clear the buffer
	MorphVertexInfo_t zeroDelta;
	zeroDelta.m_PositionDelta.Init();
	zeroDelta.m_NormalDelta.Init();
	zeroDelta.m_flWrinkleDelta = 0.0f;
	zeroDelta.m_flSpeed = 1.0f;
	zeroDelta.m_flSide = 0.5f;

	int nWidth = pVTFTexture->Width() / Get4TupleCount( m_Format );
	int nHeight = pVTFTexture->Height();
	for ( int i = 0; i < nHeight; ++i )
	{
		for ( int j = 0; j < nWidth; ++j )
		{
			(this->*writeFunc)( pixelWriter, j, i, zeroDelta );
		}
	}

	int nQuadListCount = m_MorphQuads.Count();
	for ( int i = 0; i < nQuadListCount; ++i )
	{
		MorphQuadList_t &quadList = m_MorphQuads[i];
		int nQuadCount = quadList.Count();
		int nVertIndex = 0;
		for ( int j = 0; j < nQuadCount; ++j )
		{
			MorphQuad_t &quad = quadList[j];
			int sx = quad.m_nFirstSrc / m_nTextureHeight;
			int sy = quad.m_nFirstSrc - sx * m_nTextureHeight;
			int nDest = quad.m_nFirstDest;
			for ( int k = 0; k < quad.m_nCount; ++k )
			{
				const MorphVertexInfo_t &info = m_MorphDict.GetMorphVertexInfo( i, nVertIndex );
				if ( info.m_nVertexId > nDest )
				{
					(this->*writeFunc)( pixelWriter, sx, sy+k, zeroDelta );
				}
				else
				{
					(this->*writeFunc)( pixelWriter, sx, sy+k, info );
					++nVertIndex;
				}
				++nDest;
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Deals with morph stats
//-----------------------------------------------------------------------------
static ConVar mat_morphstats( "mat_morphstats", "0", FCVAR_CHEAT );
static CUtlVector<int> s_ActiveMorphHisto;
static CUtlVector<int> s_RenderedQuadHisto;
static CUtlVector<int> s_RenderedTexelHisto;
static int s_nStatFrameCount = 0;
static int s_nStatMorphCount = 0;
static int s_nTotalMorphCount = 0;
static int s_nTotalQuadCount = 0;
static int s_nTotalTexelCount = 0;

void CMorph::ClearMorphStats()
{
	s_ActiveMorphHisto.Purge();
	s_RenderedQuadHisto.Purge();
	s_RenderedTexelHisto.Purge();

	s_nStatFrameCount = 0;
	s_nTotalMorphCount = 0;
	s_nTotalQuadCount = 0;
	s_nTotalTexelCount = 0;
}

void CMorph::AccumulateMorphStats( int nActiveMorphCount, int nQuadsRendered, int nTexelsRendered )
{
	while ( nActiveMorphCount >= s_ActiveMorphHisto.Count() )
	{
		s_ActiveMorphHisto.AddToTail( 0 );
	}
	while ( nQuadsRendered >= s_RenderedQuadHisto.Count() )
	{
		s_RenderedQuadHisto.AddToTail( 0 );
	}
	while ( nTexelsRendered >= s_RenderedTexelHisto.Count() )
	{
		s_RenderedTexelHisto.AddToTail( 0 );
	}
	s_ActiveMorphHisto[nActiveMorphCount] += 1;
	s_RenderedQuadHisto[nQuadsRendered] += 1;
	s_RenderedTexelHisto[nTexelsRendered] += 1;

	s_nStatMorphCount++;
	s_nTotalMorphCount += nActiveMorphCount;
	s_nTotalQuadCount += nQuadsRendered;
	s_nTotalTexelCount += nTexelsRendered;
}

void CMorph::ReportMorphStats( )
{
	Msg( "Morph stats:\n" );
	if ( s_nStatMorphCount == 0 )
	{
		Msg( "\tNo morphing done\n" );
		return;
	}

	Msg( "\tAverage # of active morph targets per mesh group: %d\n", s_nTotalMorphCount / s_nStatMorphCount );
	Msg( "\tAverage # of actual quad draws per morph: %d\n", s_nTotalQuadCount / s_nStatMorphCount );
	Msg( "\tAverage # of actual rendered texels per morph: %d\n", s_nTotalTexelCount / s_nStatMorphCount );

	Msg( "\tRendered Quad Count Histogram :\n\t\t" );
	for ( int i = 0; i < s_RenderedQuadHisto.Count(); ++i )
	{
		if ( s_RenderedQuadHisto[i] == 0 )
			continue;
		Msg( "[%d : %d] ", i, s_RenderedQuadHisto[i] );
	}
	Msg( "\n\tRendered Texel Count Histogram :\n\t\t" );
	for ( int i = 0; i < s_RenderedTexelHisto.Count(); ++i )
	{
		if ( s_RenderedTexelHisto[i] == 0 )
			continue;
		Msg( "[%d : %d] ", i, s_RenderedTexelHisto[i] );
	}
	Msg( "\n\tActive morph target Count Histogram :\n\t\t" );
	for ( int i = 0; i < s_ActiveMorphHisto.Count(); ++i )
	{
		if ( s_ActiveMorphHisto[i] == 0 )
			continue;
		Msg( "[%d : %d] ", i, s_ActiveMorphHisto[i] );
	}
	Msg( "\n" );
}

void CMorph::HandleMorphStats( int nActiveMorphCount, int nQuadsRendered, int nTexelsRendered )
{
	static bool s_bLastMorphStats = false;
	bool bDoStats = mat_morphstats.GetInt() != 0;
	if ( bDoStats )
	{
		if ( !s_bLastMorphStats )
		{
			ClearMorphStats();
		}
		AccumulateMorphStats( nActiveMorphCount, nQuadsRendered, nTexelsRendered );
	}
	else
	{
		if ( s_bLastMorphStats )
		{
			ReportMorphStats();
			ClearMorphStats();
		}
	}

	s_bLastMorphStats = bDoStats;
}


//-----------------------------------------------------------------------------
// Renders to the morph accumulator texture
//-----------------------------------------------------------------------------
void CMorph::RenderMorphQuads( IMatRenderContext *pRenderContext, int nRenderId, int nTotalQuadCount, int nWeightCount, int *pWeightLookup, const MorphWeight_t* pWeights )
{
	if ( s_MorphMgr.IsUsingConstantRegisters() )
	{
		pRenderContext->SetFlexWeights( 0, m_nMaxMorphTargetCount, m_pRenderMorphWeight );
	}
	else
	{
		BindMorphWeight( nRenderId );
	}

	int nXOffset, nYOffset, nWidth, nHeight;
	s_MorphMgr.ComputeAccumulatorSubrect( &nXOffset, &nYOffset, &nWidth, &nHeight, nRenderId );
	pRenderContext->Viewport( nXOffset, nYOffset, nWidth, nHeight );

	CMeshBuilder meshBuilder;
	IMesh *pMesh = pRenderContext->GetDynamicMesh( false, m_pMorphBuffer );
	meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, 0, nTotalQuadCount * 6 );

#ifdef REPORT_MORPH_STATS
	int nTexelsRendered = 0;
#endif

	for ( int i = 0; i < nWeightCount; ++i )
	{
		int nMorphIndex = m_MorphTargetIdToQuadIndex[ pWeightLookup[i] ];
		if ( nMorphIndex < 0 )
			continue;

		const MorphQuadList_t& quadList = m_MorphQuads[nMorphIndex];
		int nQuadCount = quadList.Count();
		for ( int j = 0; j < nQuadCount; ++j )
		{
			const MorphQuad_t &quad = quadList[j];

#ifdef _DEBUG
			static int s_nMinDest = -1, s_nMaxDest = -1;
			if ( s_nMinDest >= 0 && quad.m_nFirstDest + quad.m_nCount <= s_nMinDest )
				continue;
			if ( s_nMaxDest >= 0 && quad.m_nFirstDest > s_nMaxDest )
				continue;
#endif

#ifdef REPORT_MORPH_STATS
			nTexelsRendered += n4TupleCount * quad.m_nCount;
#endif

			int nBaseIndex = quad.m_nQuadIndex * 4;
			meshBuilder.FastIndex( nBaseIndex );
			meshBuilder.FastIndex( nBaseIndex+1 );
			meshBuilder.FastIndex( nBaseIndex+2 );

			meshBuilder.FastIndex( nBaseIndex );
			meshBuilder.FastIndex( nBaseIndex+2 );
			meshBuilder.FastIndex( nBaseIndex+3 );
		}
	}
	meshBuilder.End();
	pMesh->Draw();

#ifdef REPORT_MORPH_STATS
	HandleMorphStats( nWeightCount, nTotalQuadCount, nTexelsRendered );
#endif
}


//-----------------------------------------------------------------------------
// Should a morph weight be treated as zero
//-----------------------------------------------------------------------------
static inline bool IsMorphWeightZero( const MorphWeight_t &weight )
{
	return ( FloatMakePositive( weight.m_pWeight[MORPH_WEIGHT] ) < 0.001 && 
			FloatMakePositive( weight.m_pWeight[MORPH_WEIGHT_LAGGED] ) < 0.001 &&
			FloatMakePositive( weight.m_pWeight[MORPH_WEIGHT_STEREO] ) < 0.001 &&
			FloatMakePositive( weight.m_pWeight[MORPH_WEIGHT_STEREO_LAGGED] ) < 0.001 );
}


//-----------------------------------------------------------------------------
// Builds a list of non-zero morph targets
//-----------------------------------------------------------------------------
int CMorph::BuildNonZeroMorphList( int *pWeightIndices, int nWeightCount, const MorphWeight_t* pWeights )
{
	int nWeightIndexCount = 0;
	for ( int i = 0; i < m_nMaxMorphTargetCount; ++i )
	{
		const MorphWeight_t& weight = pWeights[i];

		// Don't bother with weights that aren't represented in the morph
		if ( m_MorphTargetIdToQuadIndex[i] < 0 )
			continue;

		// Don't bother with small weights
		if ( IsMorphWeightZero( weight ) )
			continue;

		pWeightIndices[nWeightIndexCount++] = i;
	}

	return nWeightIndexCount;
}


//-----------------------------------------------------------------------------
// Renders to the morph weight texture
//-----------------------------------------------------------------------------
bool CMorph::RenderMorphWeights( IMatRenderContext *pRenderContext, int nRenderId, int nWeightCount, const MorphWeight_t* pWeights )
{
	VPROF_BUDGET( "CMorph::RenderMorphWeights", _T("HW Morphing") );
	if ( m_nMaxMorphTargetCount == 0 )
		return false;

	// Cache off the weights, we need them when we accumulate the morphs later.
	int nCountToCopy = min( nWeightCount, m_nMaxMorphTargetCount );
	memcpy( m_pRenderMorphWeight, pWeights, nCountToCopy * sizeof(MorphWeight_t) );
	int nCountToClear = m_nMaxMorphTargetCount - nWeightCount;
	if ( nCountToClear > 0 )
	{
		memset( &m_pRenderMorphWeight[nCountToCopy], 0, nCountToClear * sizeof(MorphWeight_t) );
	}

	int *pWeightIndices = (int*)_alloca( nCountToCopy * sizeof(int) );
	int nIndexCount = BuildNonZeroMorphList( pWeightIndices, nCountToCopy, pWeights ); 
	if ( nIndexCount == 0 )
		return false;

	if ( s_MorphMgr.IsUsingConstantRegisters() )
		return true;

	int x, y, w, h;
	s_MorphMgr.ComputeWeightSubrect( &x, &y, &w, &h, nRenderId );

	ITexture *pMorphWeightTexture = s_MorphMgr.MorphWeights();
	int nWidth = pMorphWeightTexture->GetActualWidth();
	int nHeight = pMorphWeightTexture->GetActualHeight();
	float flOOWidth = ( nWidth != 0 ) ? 1.0f / nWidth : 1.0f;
	float flOOHeight = ( nHeight != 0 ) ? 1.0f / nHeight : 1.0f;

	// Render the weights into the morph weight texture
	CMeshBuilder meshBuilder;
	IMesh *pMesh = pRenderContext->GetDynamicMesh( );
	meshBuilder.Begin( pMesh, MATERIAL_POINTS, nCountToCopy );

	for ( int i = 0; i < nIndexCount; ++i )
	{
		int nMorphId = pWeightIndices[i];
		const MorphWeight_t& weight = pWeights[ nMorphId ];

		int nLocalX = nMorphId / h;
		int nLocalY = nMorphId - nLocalX * h;
		meshBuilder.TexCoord2f( 0, ( nLocalX + x ) * flOOWidth, ( nLocalY + y ) * flOOHeight );
		meshBuilder.TexCoord4fv( 1, weight.m_pWeight );
		meshBuilder.AdvanceVertex();
	}

	meshBuilder.End();
	pMesh->Draw();

	return true;
}


//-----------------------------------------------------------------------------
// This will generate an accumulated morph target based on the passed-in weights
//-----------------------------------------------------------------------------
void CMorph::AccumulateMorph( int nRenderId )
{
	VPROF_BUDGET( "CMorph::AccumulateMorph", _T("HW Morphing") );

	// Build a non-zero weight list and a total quad count
	int *pTargets = (int*)_alloca( m_nMaxMorphTargetCount * sizeof(int) );
	int nTargetCount = BuildNonZeroMorphList( pTargets, m_nMaxMorphTargetCount, m_pRenderMorphWeight ); 

	// Count the total number of quads to draw
	int nTotalQuadCount = 0;
	for ( int i = 0; i < nTargetCount; ++i )
	{
		int nMorphIndex = m_MorphTargetIdToQuadIndex[ pTargets[i] ];
		if ( nMorphIndex < 0 )
			continue;

		const MorphQuadList_t& quadList = m_MorphQuads[ nMorphIndex ];
		nTotalQuadCount += quadList.Count();
	}

	// Clear the morph accumulator
	// FIXME: Can I avoid even changing the render target if I know the last time
	// the morph accumulator was used that it was also cleared to black? Yes, but
	// I need to deal with alt-tab.
	bool bRenderQuads = ( nTotalQuadCount != 0 ) && ( m_nTextureWidth != 0 ) && ( m_nTextureHeight != 0 );
	if ( !bRenderQuads )
		return;

	// Next, iterate over all non-zero morphs and add them in.
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->Bind( m_MorphAccumulationMaterial );
	RenderMorphQuads( pRenderContext, nRenderId, nTotalQuadCount, nTargetCount, pTargets, m_pRenderMorphWeight );
}


//-----------------------------------------------------------------------------
//
// Morph mgr render context
//
//-----------------------------------------------------------------------------
CMorphMgrRenderContext::CMorphMgrRenderContext()
{
	m_nMorphCount = 0;

#ifdef DBGFLAG_ASSERT
	m_bInMorphAccumulation = false;
#endif
}

int CMorphMgrRenderContext::GetRenderId( CMorph* pMorph )
{
	// FIXME: This could be done without all these comparisons, at the cost of memory + complexity.
	// NOTE: m_nMorphCount <= 4.
	for ( int i = 0; i < m_nMorphCount; ++i )
	{
		if ( m_pMorphsToAccumulate[i] == pMorph )
			return i;
	}
	return -1;
}

	
//-----------------------------------------------------------------------------
//
// Morph manager implementation starts here
//
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CMorphMgr::CMorphMgr()
{
	m_pMorphAccumTexture = NULL;
	m_pMorphWeightTexture = NULL;
	m_pVisualizeMorphAccum = NULL;
	m_pVisualizeMorphWeight = NULL;
	m_pRenderMorphWeight = NULL;
	m_nFrameCount = 0;
	m_nTotalMorphSizeInBytes = 0;
	m_bUsingConstantRegisters = false;
}


//-----------------------------------------------------------------------------
// Should we allocate textures?
//-----------------------------------------------------------------------------
bool CMorphMgr::ShouldAllocateScratchTextures()
{
	return g_pMaterialSystemHardwareConfig->HasFastVertexTextures(); 
}


//-----------------------------------------------------------------------------
// Allocates scratch textures used in hw morphing
//-----------------------------------------------------------------------------
void CMorphMgr::AllocateScratchTextures()
{
	// Debug using 32323232F because we can read that back reasonably.
#ifdef _DEBUG
	ImageFormat fmt = IMAGE_FORMAT_RGBA32323232F;
#else
	ImageFormat fmt = IMAGE_FORMAT_RGBA16161616F;
#endif

	// NOTE: I'm not writing code to compute an appropriate width and height
	// given a MAX_MORPH_ACCUMULATOR_VERTICES and MORPH_ACCUMULATOR_4TUPLES
	// because this will rarely change. Just hard code it to something that will fit it.
	m_nAccumulatorWidth = 256;
	m_nAccumulatorHeight = 256;
	Assert( m_nAccumulatorWidth * m_nAccumulatorHeight == MAX_MORPH_ACCUMULATOR_VERTICES * MORPH_ACCUMULATOR_4TUPLES );

	Assert( IsPowerOfTwo( CMorphMgrRenderContext::MAX_MODEL_MORPHS ) );
	int nMultFactor = sqrt( (float)CMorphMgrRenderContext::MAX_MODEL_MORPHS );
	m_nSubrectVerticalCount = nMultFactor;

	m_pMorphAccumTexture = TextureManager()->CreateRenderTargetTexture( "_rt_MorphAccumulator", 
		m_nAccumulatorWidth * nMultFactor, m_nAccumulatorHeight * nMultFactor, 
		RT_SIZE_OFFSCREEN, fmt, RENDER_TARGET_NO_DEPTH, 
		TEXTUREFLAGS_NOMIP | TEXTUREFLAGS_NOLOD | TEXTUREFLAGS_NODEBUGOVERRIDE |
		TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT | TEXTUREFLAGS_POINTSAMPLE | TEXTUREFLAGS_VERTEXTEXTURE, 
		0 );
	m_pMorphAccumTexture->IncrementReferenceCount();

	int nDim = (int)sqrt( (float)MAXSTUDIOFLEXDESC );
	while( nDim * nDim < MAXSTUDIOFLEXDESC )
	{
		++nDim;
	}

	m_nWeightWidth = m_nWeightHeight = nDim;

	// FIXME: Re-enable if NVidia gets a fast implementation using more shader constants
	m_bUsingConstantRegisters = false; //( g_pMaterialSystemHardwareConfig->NumVertexShaderConstants() >= VERTEX_SHADER_FLEX_WEIGHTS + VERTEX_SHADER_MAX_FLEX_WEIGHT_COUNT );

	if ( !m_bUsingConstantRegisters )
	{
		m_pMorphWeightTexture = TextureManager()->CreateRenderTargetTexture( "_rt_MorphWeight", 
			m_nWeightWidth * nMultFactor, m_nWeightHeight * nMultFactor, 
			RT_SIZE_OFFSCREEN, fmt, RENDER_TARGET_NO_DEPTH, 
			TEXTUREFLAGS_NOMIP | TEXTUREFLAGS_NOLOD | TEXTUREFLAGS_NODEBUGOVERRIDE |
			TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT | TEXTUREFLAGS_POINTSAMPLE, 
			0 );
		m_pMorphWeightTexture->IncrementReferenceCount();
	}
}

void CMorphMgr::FreeScratchTextures()
{
	if ( m_pMorphAccumTexture )
	{
		m_pMorphAccumTexture->DecrementReferenceCount();
		m_pMorphAccumTexture->DeleteIfUnreferenced();
		m_pMorphAccumTexture = NULL;
	}

	if ( m_pMorphWeightTexture )
	{
		m_pMorphWeightTexture->DecrementReferenceCount();
		m_pMorphWeightTexture->DeleteIfUnreferenced();
		m_pMorphWeightTexture = NULL;
	}
}


//-----------------------------------------------------------------------------
// Allocates, frees materials used in hw morphing
//-----------------------------------------------------------------------------
void CMorphMgr::AllocateMaterials()
{
	KeyValues *pVMTKeyValues = new KeyValues( "debugmorphaccumulator" );
	pVMTKeyValues->SetString( "$basetexture", "_rt_MorphAccumulator" );
	pVMTKeyValues->SetString( "$nocull", "1" );
	pVMTKeyValues->SetString( "$ignorez", "1" );
	m_pVisualizeMorphAccum = g_pMaterialSystem->CreateMaterial( "___visualizeMorphAccum.vmt", pVMTKeyValues );

	if ( !m_bUsingConstantRegisters )
	{
		pVMTKeyValues = new KeyValues( "morphweight" );
		pVMTKeyValues->SetString( "$model", "0" );
		pVMTKeyValues->SetString( "$nocull", "1" );
		pVMTKeyValues->SetString( "$ignorez", "1" );
		m_pRenderMorphWeight = g_pMaterialSystem->CreateMaterial( "___morphweight.vmt", pVMTKeyValues );

		pVMTKeyValues = new KeyValues( "debugmorphaccumulator" );
		pVMTKeyValues->SetString( "$basetexture", "_rt_MorphWeight" );
		pVMTKeyValues->SetString( "$nocull", "1" );
		pVMTKeyValues->SetString( "$ignorez", "1" );
		m_pVisualizeMorphWeight = g_pMaterialSystem->CreateMaterial( "___visualizeMorphWeight.vmt", pVMTKeyValues );
	}
}


void CMorphMgr::FreeMaterials()
{
	if ( m_pVisualizeMorphAccum )
	{
		m_pVisualizeMorphAccum->DecrementReferenceCount();
		m_pVisualizeMorphAccum->DeleteIfUnreferenced();
		m_pVisualizeMorphAccum = NULL;
	}

	if ( m_pVisualizeMorphWeight )
	{
		m_pVisualizeMorphWeight->DecrementReferenceCount();
		m_pVisualizeMorphWeight->DeleteIfUnreferenced();
		m_pVisualizeMorphWeight = NULL;
	}

	if ( m_pRenderMorphWeight )
	{
		m_pRenderMorphWeight->DecrementReferenceCount();
		m_pRenderMorphWeight->DeleteIfUnreferenced();
		m_pRenderMorphWeight = NULL;
	}
}


//-----------------------------------------------------------------------------
// Morph render context
//-----------------------------------------------------------------------------
IMorphMgrRenderContext *CMorphMgr::AllocateRenderContext()
{
	return new CMorphMgrRenderContext;
}

void CMorphMgr::FreeRenderContext( IMorphMgrRenderContext *pRenderContext )
{
	delete static_cast< CMorphMgrRenderContext* >( pRenderContext );
}


//-----------------------------------------------------------------------------
// Returns the morph accumulation texture
//-----------------------------------------------------------------------------
ITextureInternal *CMorphMgr::MorphAccumulator()
{
	return m_pMorphAccumTexture;
}

ITextureInternal *CMorphMgr::MorphWeights()
{
	return m_pMorphWeightTexture;
}


//-----------------------------------------------------------------------------
// Class factory
//-----------------------------------------------------------------------------
IMorphInternal *CMorphMgr::CreateMorph()
{
	return new CMorph;
}

void CMorphMgr::DestroyMorph( IMorphInternal *pMorphData )
{
	if ( pMorphData )
	{
		delete static_cast< CMorph*>( pMorphData );
	}
}


//-----------------------------------------------------------------------------
// Advances the frame (for debugging)
//-----------------------------------------------------------------------------
void CMorphMgr::AdvanceFrame()
{
	++m_nFrameCount;
}


//-----------------------------------------------------------------------------
// Computes texel offsets for the upper corner of the morph weight texture for a particular block
//-----------------------------------------------------------------------------
void CMorphMgr::ComputeWeightSubrect( int *pXOffset, int *pYOffset, int *pWidth, int *pHeight, int nMorphAccumBlockId )
{
	*pXOffset = nMorphAccumBlockId / m_nSubrectVerticalCount;
	*pYOffset = nMorphAccumBlockId - m_nSubrectVerticalCount * (*pXOffset);
	*pXOffset *= m_nWeightWidth;
	*pYOffset *= m_nWeightHeight;
	*pWidth = m_nWeightWidth;
	*pHeight = m_nWeightHeight;
}


//-----------------------------------------------------------------------------
// Computes texel offsets for the upper corner of the morph accumulator for a particular block
//-----------------------------------------------------------------------------
void CMorphMgr::ComputeAccumulatorSubrect( int *pXOffset, int *pYOffset, int *pWidth, int *pHeight, int nMorphAccumBlockId )
{
	*pXOffset = nMorphAccumBlockId / m_nSubrectVerticalCount;
	*pYOffset = nMorphAccumBlockId - m_nSubrectVerticalCount * (*pXOffset);
	*pXOffset *= m_nAccumulatorWidth;
	*pYOffset *= m_nAccumulatorHeight;
	*pWidth = m_nAccumulatorWidth;
	*pHeight = m_nAccumulatorHeight;
}

void CMorphMgr::GetAccumulatorSubrectDimensions( int *pWidth, int *pHeight )
{
	*pWidth = m_nAccumulatorWidth;
	*pHeight = m_nAccumulatorHeight;
}

int CMorphMgr::GetAccumulator4TupleCount() const
{
	return MORPH_ACCUMULATOR_4TUPLES;
}


//-----------------------------------------------------------------------------
// Used to compute stats of memory used
//-----------------------------------------------------------------------------
CON_COMMAND_F( mat_reporthwmorphmemory, "Reports the amount of size in bytes taken up by hardware morph textures.", FCVAR_CHEAT )
{
	ConMsg( "Total HW Morph memory used: %dk\n", s_MorphMgr.GetTotalMemoryUsage() /1024 );
}

void CMorphMgr::RegisterMorphSizeInBytes( int nSizeInBytes )
{
	m_nTotalMorphSizeInBytes += nSizeInBytes;
	Assert( m_nTotalMorphSizeInBytes >= 0 );
}

int CMorphMgr::GetTotalMemoryUsage() const
{
	int nSize = 0;
	if ( m_pMorphAccumTexture )
	{
		nSize += m_pMorphAccumTexture->GetActualWidth() * m_pMorphAccumTexture->GetActualHeight() *
			ImageLoader::SizeInBytes( m_pMorphAccumTexture->GetImageFormat() );
	}
	if ( m_pMorphWeightTexture )
	{
		nSize += m_pMorphWeightTexture->GetActualWidth() * m_pMorphWeightTexture->GetActualHeight() *
			ImageLoader::SizeInBytes( m_pMorphWeightTexture->GetImageFormat() );
	}
	nSize += m_nTotalMorphSizeInBytes;
	return nSize;
}


//-----------------------------------------------------------------------------
// Displays 32bit float texture data
//-----------------------------------------------------------------------------
void CMorphMgr::Display32FTextureData( float *pBuf, int nTexelID, int *pSubRect, ITexture *pTexture, int n4TupleCount )
{
	int nColumn = nTexelID / pSubRect[3];
	int nRow = nTexelID - nColumn * pSubRect[3];
	nColumn *= n4TupleCount;
	nColumn += pSubRect[0];
	nRow += pSubRect[1];

	Msg( "[%d] : ", nTexelID );
	for ( int i = 0; i < n4TupleCount; ++i )
	{
		float *pBase = &pBuf[ (nRow * pTexture->GetActualWidth() + nColumn + i ) * 4 ];
		Msg( "[ %.4f %.4f %.4f %.4f ] ", pBase[0], pBase[1], pBase[2], pBase[3] );
	}
	Msg( "\n" );
}


//-----------------------------------------------------------------------------
// A debugging utility to display the morph accumulator
//-----------------------------------------------------------------------------
void CMorphMgr::DebugMorphAccumulator( IMatRenderContext *pRenderContext )
{
	static bool s_bDebug = false;
	if ( !s_bDebug )
		return;

	ITexture *pDest = g_pMorphMgr->MorphAccumulator( );
	if ( pDest->GetImageFormat() != IMAGE_FORMAT_RGBA32323232F )
		return;

	int nDestWidth = pDest->GetActualWidth();
	int nDestHeight = pDest->GetActualHeight();

	float* pBuf = (float*)malloc( nDestWidth * nDestHeight * 4 * sizeof(float) );
	pRenderContext->ReadPixels( 0, 0, nDestWidth, nDestHeight, (unsigned char*)pBuf, IMAGE_FORMAT_RGBA32323232F );

	Msg( "Morph Accumulator:\n" );

	static int s_nMinDisplay = 0;
	static int s_nMaxDisplay = -1;
	static int s_nMorphIndex = 0;

	int pSubRect[4];
	ComputeAccumulatorSubrect( &pSubRect[0], &pSubRect[1], &pSubRect[2], &pSubRect[3], s_nMorphIndex ); 

	if ( s_nMaxDisplay < 0 )
	{
		Display32FTextureData( pBuf, s_nMinDisplay, pSubRect, pDest, MORPH_ACCUMULATOR_4TUPLES );
	}
	else
	{
		for ( int i = s_nMinDisplay; i <= s_nMaxDisplay; ++i )
		{
			Display32FTextureData( pBuf, i, pSubRect, pDest, MORPH_ACCUMULATOR_4TUPLES );
		}
	}
	free( pBuf );
}


//-----------------------------------------------------------------------------
// A debugging utility to display the morph weights
//-----------------------------------------------------------------------------
void CMorphMgr::DebugMorphWeights( IMatRenderContext *pRenderContext )
{
	static bool s_bDebug = false;
	if ( !s_bDebug )
		return;

	ITexture *pTexture = MorphWeights();
	int nWidth = pTexture->GetActualWidth();
	int nHeight = pTexture->GetActualHeight();
	if ( pTexture->GetImageFormat() != IMAGE_FORMAT_RGBA32323232F )
		return;

	pRenderContext->Flush();
	float* pBuf = (float*)malloc( nWidth * nHeight * 4 * sizeof(float) );
	pRenderContext->ReadPixels( 0, 0, nWidth, nHeight, (unsigned char*)pBuf, IMAGE_FORMAT_RGBA32323232F );

	Msg( "Morph Weights:\n" );

	static int s_nMinDisplay = 0;
	static int s_nMaxDisplay = -1;
	static int s_nMorphIndex = 0;

	int pSubRect[4];
	ComputeWeightSubrect( &pSubRect[0], &pSubRect[1], &pSubRect[2], &pSubRect[3], s_nMorphIndex ); 

	if ( s_nMaxDisplay < 0 )
	{
		Display32FTextureData( pBuf, s_nMinDisplay, pSubRect, pTexture, 1 );
	}
	else
	{
		for ( int i = s_nMinDisplay; i <= s_nMaxDisplay; ++i )
		{
			Display32FTextureData( pBuf, i, pSubRect, pTexture, 1 );
		}
	}
	free( pBuf );
}


//-----------------------------------------------------------------------------
// Draws the morph accumulator
//-----------------------------------------------------------------------------
#ifdef _DEBUG
ConVar mat_drawmorphaccumulator( "mat_drawmorphaccumulator", "0", FCVAR_CHEAT );
ConVar mat_drawmorphweights( "mat_drawmorphweights", "0", FCVAR_CHEAT );
#endif

void CMorphMgr::DrawMorphTempTexture( IMatRenderContext *pRenderContext, IMaterial *pMaterial, ITexture *pTexture )
{
	pMaterial = ((IMaterialInternal *)pMaterial)->GetRealTimeVersion(); //always work with the real time version of materials internally.

	static int s_nLastFrameCount = -1;
	static int s_nX = 0, s_nY = 0;
	if ( s_nLastFrameCount != m_nFrameCount )
	{
		s_nX = 0; s_nY = 0;
		s_nLastFrameCount = m_nFrameCount;
	}

	pRenderContext->Flush();

	int nWidth = pTexture->GetActualWidth();
	int nHeight = pTexture->GetActualHeight();
	::DrawScreenSpaceRectangle( pMaterial, s_nX, s_nY, nWidth, nHeight,
		0, 0, nWidth-1, nHeight-1, nWidth, nHeight );

	s_nX += nWidth;
	if ( s_nX > 1024 )
	{
		s_nX = 0;
		s_nY += nHeight;
	}
	pRenderContext->Flush();
}


//-----------------------------------------------------------------------------
// Starts, ends morph accumulation.
//-----------------------------------------------------------------------------
int CMorphMgr::MaxHWMorphBatchCount() const
{
	return CMorphMgrRenderContext::MAX_MODEL_MORPHS;
}


//-----------------------------------------------------------------------------
// Returns the texcoord associated with a morph
//-----------------------------------------------------------------------------
bool CMorphMgr::GetMorphAccumulatorTexCoord( IMorphMgrRenderContext *pRenderContext, Vector2D *pTexCoord, IMorph *pMorph, int nVertex )
{
	CMorphMgrRenderContext *pMorphRenderContext = static_cast< CMorphMgrRenderContext* >( pRenderContext );
	int nRenderId = pMorphRenderContext->GetRenderId( static_cast<CMorph*>( pMorph ) );
	if ( nRenderId < 0 )
	{
		pTexCoord->Init();
		return false;
	}

	int nWidth = m_pMorphAccumTexture->GetActualWidth();
	int nHeight = m_pMorphAccumTexture->GetActualHeight();
	if ( !nWidth || !nHeight )
	{
		pTexCoord->Init();
		return false;
	}

	float flOOWidth = ( nWidth != 0 ) ? 1.0f / nWidth : 1.0f;
	float flOOHeight = ( nHeight != 0 ) ? 1.0f / nHeight : 1.0f;

	int x, y, w, h;
	ComputeAccumulatorSubrect( &x, &y, &w, &h, nRenderId );
	int nColumn = nVertex / h;
	int nRow = nVertex - h * nColumn;
	nColumn *= MORPH_ACCUMULATOR_4TUPLES;

	pTexCoord->x = ( x + nColumn + 0.5f ) * flOOWidth;
	pTexCoord->y = ( y + nRow + 0.5f ) * flOOHeight;
	Assert( IsFinite( pTexCoord->x ) && IsFinite( pTexCoord->y ) );
	return true;
}


//-----------------------------------------------------------------------------
// Starts, ends morph accumulation.
//-----------------------------------------------------------------------------
void CMorphMgr::BeginMorphAccumulation( IMorphMgrRenderContext *pIRenderContext )
{
	VPROF_BUDGET( "CMorph::BeginMorphAccumulation", _T("HW Morphing") );

	// Set up the render context
	CMorphMgrRenderContext *pMorphRenderContext = static_cast< CMorphMgrRenderContext* >( pIRenderContext );
	Assert( !pMorphRenderContext->m_bInMorphAccumulation );
	pMorphRenderContext->m_nMorphCount = 0;

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	IMatRenderContextInternal *pRenderContextInternal = static_cast<IMatRenderContextInternal*>( (IMatRenderContext*)pRenderContext );

	// Cache off the current material and other render state
	// NOTE: We always have to do this because pushing the morph accumulator
	// may cause it to be unbound; therefore we must force a rebind of the material.
	m_pPrevMaterial = pRenderContextInternal->GetCurrentMaterial();
	m_pPrevProxy = pRenderContextInternal->GetCurrentProxy();
	m_nPrevBoneCount = pRenderContextInternal->GetCurrentNumBones();
	m_nPrevClipMode = pRenderContext->GetHeightClipMode( );
	m_bPrevClippingEnabled = pRenderContext->EnableClipping( false );
	m_bFlashlightMode = pRenderContext->GetFlashlightMode();
	pRenderContext->SetHeightClipMode( MATERIAL_HEIGHTCLIPMODE_DISABLE );
	pRenderContext->SetNumBoneWeights( 0 );
	pRenderContext->SetFlashlightMode( false );

	if ( !m_bUsingConstantRegisters )
	{
		// FIXME: We could theoretically avoid pushing this if we copied off all the 
		// weights and set the weight texture only at the end if any non-zero weights
		// were sent down
		pRenderContext->PushRenderTargetAndViewport( m_pMorphWeightTexture );

#ifdef _DEBUG
		// NOTE: No need to clear the texture; we will only be reading out of that
		// texture at points where we've rendered to in this pass. 
		// But, we'll do it for debugging reasons.
		// I believe this pattern of weights is the least likely to occur naturally.
		pRenderContext->ClearColor4ub( 0, 0, 0, 0 );
		pRenderContext->ClearBuffers( true, false, false );
#endif
	}
	 
#ifdef DBGFLAG_ASSERT
	pMorphRenderContext->m_bInMorphAccumulation = true;
#endif
}

void CMorphMgr::EndMorphAccumulation( IMorphMgrRenderContext *pIRenderContext )
{
	VPROF_BUDGET( "CMorph::EndMorphAccumulation", _T("HW Morphing") );

	CMorphMgrRenderContext *pMorphRenderContext = static_cast< CMorphMgrRenderContext* >( pIRenderContext );
	Assert( pMorphRenderContext->m_bInMorphAccumulation );
	VPROF_INCREMENT_COUNTER( "HW Morph Count", pMorphRenderContext->m_nMorphCount );

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );

#ifdef _DEBUG
	if ( !m_bUsingConstantRegisters )
	{
		DebugMorphWeights( pRenderContext );
	}
#endif

	// Now that all the weights have been rendered, accumulate the morphs
	// First, clear the morph accumulation texture
	int nWidth = m_pMorphAccumTexture->GetActualWidth();
	int nHeight = m_pMorphAccumTexture->GetActualHeight();
	if ( !m_bUsingConstantRegisters )
	{
		pRenderContext->SetRenderTargetEx( 0, m_pMorphAccumTexture );
		pRenderContext->Viewport( 0, 0, nWidth, nHeight );
	}
	else
	{
		pRenderContext->PushRenderTargetAndViewport( m_pMorphAccumTexture );
	}
	pRenderContext->ClearColor4ub( 0, 0, 0, 0 );
	pRenderContext->ClearBuffers( true, false, false );

	for ( int i = 0; i < pMorphRenderContext->m_nMorphCount; ++i )
	{
		pMorphRenderContext->m_pMorphsToAccumulate[i]->AccumulateMorph( i );
	}
#ifdef _DEBUG
	DebugMorphAccumulator( pRenderContext );
#endif
	pRenderContext->PopRenderTargetAndViewport();
	 
#ifdef _DEBUG
	if ( mat_drawmorphweights.GetInt() )
	{
		if ( !m_bUsingConstantRegisters )
		{
			DrawMorphTempTexture( pRenderContext, m_pVisualizeMorphWeight, MorphWeights( ) );
		}
	}
	if ( mat_drawmorphaccumulator.GetInt() )
	{
		DrawMorphTempTexture( pRenderContext, m_pVisualizeMorphAccum, MorphAccumulator( ) );
	}
#endif

	pRenderContext->Bind( m_pPrevMaterial, m_pPrevProxy );
	pRenderContext->SetNumBoneWeights( m_nPrevBoneCount );
	pRenderContext->SetHeightClipMode( m_nPrevClipMode );
	pRenderContext->EnableClipping( m_bPrevClippingEnabled );
	pRenderContext->SetFlashlightMode( m_bFlashlightMode );

#ifdef DBGFLAG_ASSERT
	pMorphRenderContext->m_bInMorphAccumulation = false;
#endif
}

						 
//-----------------------------------------------------------------------------
// Accumulates a morph target into the morph texture
//-----------------------------------------------------------------------------
void CMorphMgr::AccumulateMorph( IMorphMgrRenderContext *pIRenderContext, IMorph* pMorph, int nMorphCount, const MorphWeight_t* pWeights )
{
	CMorphMgrRenderContext *pMorphRenderContext = static_cast< CMorphMgrRenderContext* >( pIRenderContext );
	Assert( pMorphRenderContext->m_bInMorphAccumulation );

	Assert( pMorphRenderContext->m_nMorphCount < CMorphMgrRenderContext::MAX_MODEL_MORPHS );
	if ( pMorphRenderContext->m_nMorphCount >= CMorphMgrRenderContext::MAX_MODEL_MORPHS )
	{
		Warning( "Attempted to morph too many meshes in a single model!\n" );
		Assert(0);
		return;
	}

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	
	CMorph *pMorphInternal = static_cast<CMorph*>( pMorph );
	if ( !m_bUsingConstantRegisters )
	{
		pRenderContext->Bind( m_pRenderMorphWeight );
	}
	if ( pMorphInternal->RenderMorphWeights( pRenderContext, pMorphRenderContext->m_nMorphCount, nMorphCount, pWeights ) )
	{
		pMorphRenderContext->m_pMorphsToAccumulate[pMorphRenderContext->m_nMorphCount] = pMorphInternal;
		++pMorphRenderContext->m_nMorphCount;
	}
}

