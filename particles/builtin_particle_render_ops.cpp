//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: particle system code
//
//===========================================================================//

#include "tier0/platform.h"
#include "particles/particles.h"
#include "filesystem.h"
#include "tier2/tier2.h"
#include "tier2/fileutils.h"
#include "tier2/renderutils.h"
#include "tier2/beamsegdraw.h"
#include "tier1/UtlStringMap.h"
#include "tier1/strtools.h"
#include "materialsystem/imesh.h"
#include "materialsystem/itexture.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialvar.h"
#include "psheet.h"
#include "tier0/vprof.h"

#ifdef USE_BLOBULATOR
// TODO: These should be in public by the time the SDK ships
	#include "../common/blobulator/Implicit/ImpDefines.h"
	#include "../common/blobulator/Implicit/ImpRenderer.h"
	#include "../common/blobulator/Implicit/ImpTiler.h"
	#include "../common/blobulator/Implicit/UserFunctions.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Vertex instancing (1 vert submitted per particle, duplicated to 4 (a quad) on the GPU) is supported only on 360
const bool bUseInstancing = IsX360();


//-----------------------------------------------------------------------------
// Utility method to compute the max # of particles per batch
//-----------------------------------------------------------------------------
static inline int GetMaxParticlesPerBatch( IMatRenderContext *pRenderContext, IMaterial *pMaterial, bool bWithInstancing )
{
	int nMaxVertices = pRenderContext->GetMaxVerticesToRender( pMaterial );
	int nMaxIndices  = pRenderContext->GetMaxIndicesToRender();

	if ( bWithInstancing )
		return nMaxVertices;
	else
		return min( (nMaxVertices / 4), (nMaxIndices / 6) );
}

void SetupParticleVisibility( CParticleCollection *pParticles, CParticleVisibilityData *pVisibilityData, const CParticleVisibilityInputs *pVisibilityInputs, int *nQueryHandle )
{
	float flScale = pVisibilityInputs->m_flProxyRadius;
	Vector vecOrigin;
	/*
	if ( pVisibilityInputs->m_bUseBBox )
	{
		Vector vecMinBounds;
		Vector vecMaxBounds;
		Vector mins;
		Vector maxs;

		pParticles->GetBounds( &vecMinBounds, &vecMaxBounds );

		vecOrigin = ( ( vecMinBounds + vecMaxBounds ) / 2 );

		Vector vecBounds = ( vecMaxBounds - vecMinBounds );

		flScale = ( max(vecBounds.x, max (vecBounds.y, vecBounds.z) ) * pVisibilityInputs->m_flBBoxScale );
	}
	if ( pVisibilityInputs->m_nCPin >= 0 )
	{
		vecOrigin = pParticles->GetControlPointAtCurrentTime( pVisibilityInputs->m_nCPin );
	}
	*/
	vecOrigin = pParticles->GetControlPointAtCurrentTime( pVisibilityInputs->m_nCPin );
	float flVisibility = g_pParticleSystemMgr->Query()->GetPixelVisibility( nQueryHandle, vecOrigin, flScale );

	pVisibilityData->m_flAlphaVisibility = RemapValClamped( flVisibility, pVisibilityInputs->m_flInputMin, 
		pVisibilityInputs->m_flInputMax, pVisibilityInputs->m_flAlphaScaleMin, pVisibilityInputs->m_flAlphaScaleMax  );
	pVisibilityData->m_flRadiusVisibility = RemapValClamped( flVisibility, pVisibilityInputs->m_flInputMin, 
		pVisibilityInputs->m_flInputMax, pVisibilityInputs->m_flRadiusScaleMin, pVisibilityInputs->m_flRadiusScaleMax  );

	pVisibilityData->m_flCameraBias = pVisibilityInputs->m_flCameraBias;
}

static SheetSequenceSample_t s_DefaultSheetSequence = 
{
	{
		{ 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f }, // SequenceSampleTextureCoords_t image 0
		{ 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f }  // SequenceSampleTextureCoords_t image 1
	},
	1.0f // m_fBlendFactor
};


class C_OP_RenderPoints : public CParticleRenderOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_OP_RenderPoints );

	uint32 GetWrittenAttributes( void ) const
	{
		return 0;
	}

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_XYZ_MASK;
	}

	virtual void Render( IMatRenderContext *pRenderContext, CParticleCollection *pParticles, void *pContext ) const;

	struct C_OP_RenderPointsContext_t
	{
		CParticleVisibilityData m_VisibilityData;
		int		m_nQueryHandle;
	};

	size_t GetRequiredContextBytes( void ) const
	{
		return sizeof( C_OP_RenderPointsContext_t );
	}

	virtual void InitializeContextData( CParticleCollection *pParticles, void *pContext ) const
	{
		C_OP_RenderPointsContext_t *pCtx = reinterpret_cast<C_OP_RenderPointsContext_t *>( pContext );
		pCtx->m_VisibilityData.m_bUseVisibility = false;
		pCtx->m_VisibilityData.m_flCameraBias = VisibilityInputs.m_flCameraBias;
	}
};

DEFINE_PARTICLE_OPERATOR( C_OP_RenderPoints, "render_points", OPERATOR_SINGLETON );

BEGIN_PARTICLE_RENDER_OPERATOR_UNPACK( C_OP_RenderPoints ) 
END_PARTICLE_OPERATOR_UNPACK( C_OP_RenderPoints )

void C_OP_RenderPoints::Render( IMatRenderContext *pRenderContext, CParticleCollection *pParticles, void *pContext ) const
{
	C_OP_RenderPointsContext_t *pCtx = reinterpret_cast<C_OP_RenderPointsContext_t *>( pContext );
	IMaterial *pMaterial = pParticles->m_pDef->GetMaterial();

	int nParticles;
	const ParticleRenderData_t *pRenderList = 
		pParticles->GetRenderList( pRenderContext, true, &nParticles, &pCtx->m_VisibilityData  );

	size_t xyz_stride;
	const fltx4 *xyz = pParticles->GetM128AttributePtr( PARTICLE_ATTRIBUTE_XYZ, &xyz_stride );

	pRenderContext->Bind( pMaterial );

	CMeshBuilder meshBuilder;

	int nMaxVertices = pRenderContext->GetMaxVerticesToRender( pMaterial );
	while ( nParticles )
	{
		IMesh* pMesh = pRenderContext->GetDynamicMesh( true );

		int nParticlesInBatch = min( nMaxVertices, nParticles );
		nParticles -= nParticlesInBatch;
		g_pParticleSystemMgr->TallyParticlesRendered( nParticlesInBatch );

		meshBuilder.Begin( pMesh, MATERIAL_POINTS, nParticlesInBatch );
		for( int i = 0; i < nParticlesInBatch; i++ )
		{
			int hParticle = (--pRenderList)->m_nIndex;
			int nIndex = ( hParticle / 4 ) * xyz_stride;
			int nOffset = hParticle & 0x3;
			meshBuilder.Position3f( SubFloat( xyz[nIndex], nOffset ), SubFloat( xyz[nIndex+1], nOffset ), SubFloat( xyz[nIndex+2], nOffset ) );
			meshBuilder.Color4ub( 255, 255, 255, 255 );
			meshBuilder.AdvanceVertex();
		}
		meshBuilder.End();
		pMesh->Draw();
	}
}

//-----------------------------------------------------------------------------
//
// Sprite Rendering
//
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Utility struct to help with sprite rendering
//-----------------------------------------------------------------------------
struct SpriteRenderInfo_t
{
	size_t m_nXYZStride;
	const fltx4 *m_pXYZ;
	size_t m_nRotStride;
	const fltx4 *m_pRot;
	size_t m_nYawStride;
	const fltx4 *m_pYaw;
	size_t m_nRGBStride;
	const fltx4 *m_pRGB;
	size_t m_nCreationTimeStride;
	const fltx4 *m_pCreationTimeStamp;
	size_t m_nSequenceStride;
	const fltx4 *m_pSequenceNumber;
	size_t m_nSequence1Stride;
	const fltx4 *m_pSequence1Number;
	float m_flAgeScale;
	float m_flAgeScale2;

	CSheet *m_pSheet;
	int m_nVertexOffset;
	CParticleCollection *m_pParticles;

	void Init( CParticleCollection *pParticles, int nVertexOffset, float flAgeScale, float flAgeScale2, CSheet *pSheet )
	{
		m_pParticles = pParticles;
		m_pXYZ = pParticles->GetM128AttributePtr( PARTICLE_ATTRIBUTE_XYZ, &m_nXYZStride );
		m_pRot = pParticles->GetM128AttributePtr( PARTICLE_ATTRIBUTE_ROTATION, &m_nRotStride );
		m_pYaw = pParticles->GetM128AttributePtr( PARTICLE_ATTRIBUTE_YAW, &m_nYawStride );
		m_pRGB = pParticles->GetM128AttributePtr( PARTICLE_ATTRIBUTE_TINT_RGB, &m_nRGBStride );
		m_pCreationTimeStamp = pParticles->GetM128AttributePtr( PARTICLE_ATTRIBUTE_CREATION_TIME, &m_nCreationTimeStride );
		m_pSequenceNumber = pParticles->GetM128AttributePtr( PARTICLE_ATTRIBUTE_SEQUENCE_NUMBER, &m_nSequenceStride );
		m_pSequence1Number = pParticles->GetM128AttributePtr( PARTICLE_ATTRIBUTE_SEQUENCE_NUMBER1, &m_nSequence1Stride );
		m_flAgeScale = flAgeScale;
		m_flAgeScale2 = flAgeScale2;
		m_pSheet = pSheet;
		m_nVertexOffset = nVertexOffset;
	}
};

class C_OP_RenderSprites : public C_OP_RenderPoints
{
	DECLARE_PARTICLE_OPERATOR( C_OP_RenderSprites );

	struct C_OP_RenderSpritesContext_t
	{
		unsigned int m_nOrientationVarToken;
		unsigned int m_nOrientationMatrixVarToken;
		CParticleVisibilityData m_VisibilityData;
		int		m_nQueryHandle;
	};

	size_t GetRequiredContextBytes( void ) const
	{
		return sizeof( C_OP_RenderSpritesContext_t );
	}

	virtual void InitializeContextData( CParticleCollection *pParticles, void *pContext ) const
	{
		C_OP_RenderSpritesContext_t *pCtx = reinterpret_cast<C_OP_RenderSpritesContext_t *>( pContext );
		pCtx->m_nOrientationVarToken = 0;
		pCtx->m_nOrientationMatrixVarToken = 0;
		if ( VisibilityInputs.m_nCPin >= 0 )
			pCtx->m_VisibilityData.m_bUseVisibility = true;
		else
			pCtx->m_VisibilityData.m_bUseVisibility = false;

		pCtx->m_VisibilityData.m_flCameraBias = VisibilityInputs.m_flCameraBias;
	}

	virtual uint64 GetReadControlPointMask() const
	{
		if ( m_nOrientationControlPoint >= 0 )
			return 1ULL << m_nOrientationControlPoint;
		return 0;
	}

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_XYZ_MASK | PARTICLE_ATTRIBUTE_ROTATION_MASK | PARTICLE_ATTRIBUTE_RADIUS_MASK | 
			PARTICLE_ATTRIBUTE_TINT_RGB_MASK | PARTICLE_ATTRIBUTE_ALPHA_MASK | PARTICLE_ATTRIBUTE_CREATION_TIME_MASK |
			PARTICLE_ATTRIBUTE_SEQUENCE_NUMBER1_MASK |
			PARTICLE_ATTRIBUTE_SEQUENCE_NUMBER_MASK | PARTICLE_ATTRIBUTE_LIFE_DURATION_MASK;
	}

	virtual void InitParams( CParticleSystemDefinition *pDef, CDmxElement *pElement );
	virtual int GetParticlesToRender( CParticleCollection *pParticles, void *pContext, int nFirstParticle, int nRemainingVertices, int nRemainingIndices, int *pVertsUsed, int *pIndicesUsed ) const;
	virtual void Render( IMatRenderContext *pRenderContext, CParticleCollection *pParticles, void *pContext ) const;
	virtual void RenderUnsorted( CParticleCollection *pParticles, void *pContext, IMatRenderContext *pRenderContext, CMeshBuilder &meshBuilder, int nVertexOffset, int nFirstParticle, int nParticleCount ) const;
	void RenderSpriteCard( CMeshBuilder &meshBuilder, C_OP_RenderSpritesContext_t *pCtx, SpriteRenderInfo_t& info, int hParticle, ParticleRenderData_t const *pSortList, Vector *pCamera ) const;
	void RenderTwoSequenceSpriteCard( CMeshBuilder &meshBuilder, C_OP_RenderSpritesContext_t *pCtx, SpriteRenderInfo_t& info, int hParticle, ParticleRenderData_t const *pSortList, Vector *pCamera ) const;

	void RenderNonSpriteCardCameraFacing( CParticleCollection *pParticles, void *pContext, IMatRenderContext *pRenderContext, IMaterial *pMaterial ) const;

	void RenderNonSpriteCardZRotating( CMeshBuilder &meshBuilder, C_OP_RenderSpritesContext_t *pCtx, SpriteRenderInfo_t& info, int hParticle, const Vector& vecCameraPos, ParticleRenderData_t const *pSortList ) const;
	void RenderNonSpriteCardZRotating( CParticleCollection *pParticles, void *pContext, IMatRenderContext *pRenderContext, IMaterial *pMaterial ) const;
	void RenderUnsortedNonSpriteCardZRotating( CParticleCollection *pParticles, void *pContext, IMatRenderContext *pRenderContext, CMeshBuilder &meshBuilder, int nVertexOffset, int nFirstParticle, int nParticleCount ) const;

	void RenderNonSpriteCardOriented( CMeshBuilder &meshBuilder, C_OP_RenderSpritesContext_t *pCtx, SpriteRenderInfo_t& info, int hParticle, const Vector& vecCameraPos, ParticleRenderData_t const *pSortList, bool bUseYaw ) const;
	void RenderNonSpriteCardOriented( CParticleCollection *pParticles, void *pContext, IMatRenderContext *pRenderContext, IMaterial *pMaterial, bool bUseYaw ) const;
	void RenderUnsortedNonSpriteCardOriented( CParticleCollection *pParticles, void *pContext, IMatRenderContext *pRenderContext, CMeshBuilder &meshBuilder, int nVertexOffset, int nFirstParticle, int nParticleCount ) const;

	// cycles per second
	float	m_flAnimationRate;
	float	m_flAnimationRate2;
	bool	m_bFitCycleToLifetime;
	bool	m_bAnimateInFPS;
	int		m_nOrientationType;


	int m_nOrientationControlPoint;
};

DEFINE_PARTICLE_OPERATOR( C_OP_RenderSprites, "render_animated_sprites", OPERATOR_GENERIC );

BEGIN_PARTICLE_RENDER_OPERATOR_UNPACK( C_OP_RenderSprites ) 
	DMXELEMENT_UNPACK_FIELD( "animation rate", ".1", float, m_flAnimationRate )
	DMXELEMENT_UNPACK_FIELD( "animation_fit_lifetime", "0", bool, m_bFitCycleToLifetime )
	DMXELEMENT_UNPACK_FIELD( "orientation_type", "0", int, m_nOrientationType )
	DMXELEMENT_UNPACK_FIELD( "orientation control point", "-1", int, m_nOrientationControlPoint )
	DMXELEMENT_UNPACK_FIELD( "second sequence animation rate", "0", float, m_flAnimationRate2 )
	DMXELEMENT_UNPACK_FIELD( "use animation rate as FPS", "0", bool, m_bAnimateInFPS )
END_PARTICLE_OPERATOR_UNPACK( C_OP_RenderSprites )

void C_OP_RenderSprites::InitParams( CParticleSystemDefinition *pDef, CDmxElement *pElement )
{
}

const SheetSequenceSample_t *GetSampleForSequence( CSheet *pSheet, float flCreationTime, float flCurTime, float flAgeScale, int nSequence )
{
	if ( pSheet == NULL )
		return NULL;

	if ( pSheet->m_nNumFrames[nSequence] == 1 )
		return (const SheetSequenceSample_t *) &pSheet->m_pSamples[nSequence][0];

	float flAge = flCurTime - flCreationTime;

	flAge *= flAgeScale;
	unsigned int nFrame = flAge;
	if ( pSheet->m_bClamp[nSequence] )
	{
		nFrame = min( nFrame, (unsigned int)SEQUENCE_SAMPLE_COUNT-1 );
	}
	else
	{
		nFrame &= SEQUENCE_SAMPLE_COUNT-1;
	}

	return (const SheetSequenceSample_t *) &pSheet->m_pSamples[nSequence][nFrame];
}

int C_OP_RenderSprites::GetParticlesToRender( CParticleCollection *pParticles, 
											 void *pContext, int nFirstParticle, 
											  int nRemainingVertices, int nRemainingIndices,
											  int *pVertsUsed, int *pIndicesUsed ) const
{
	int nMaxParticles = ( (nRemainingVertices / 4) > (nRemainingIndices / 6) ) ? nRemainingIndices / 6 : nRemainingVertices / 4;
	int nParticleCount = pParticles->m_nActiveParticles - nFirstParticle;
	if ( nParticleCount > nMaxParticles )
	{
		nParticleCount = nMaxParticles;
	}
	*pVertsUsed = nParticleCount * 4;
	*pIndicesUsed = nParticleCount * 6;
	return nParticleCount;
}

void C_OP_RenderSprites::RenderNonSpriteCardCameraFacing( CParticleCollection *pParticles, void *pContext, IMatRenderContext *pRenderContext, IMaterial *pMaterial ) const
{

	C_OP_RenderSpritesContext_t *pCtx = reinterpret_cast<C_OP_RenderSpritesContext_t *>( pContext );

	// generate the sort list before this code starts messing with the matrices
	int nParticles;
	const ParticleRenderData_t *pSortList = pParticles->GetRenderList( pRenderContext, true, &nParticles, &pCtx->m_VisibilityData );

	bool bCameraBias = ( &pCtx->m_VisibilityData )->m_flCameraBias != 0.0f;
	float flCameraBias = (&pCtx->m_VisibilityData )->m_flCameraBias;

	Vector vecCamera;
	pRenderContext->GetWorldSpaceCameraPosition( &vecCamera );

	// NOTE: This is interesting to support because at first we won't have all the various
	// pixel-shader versions of SpriteCard, like modulate, twotexture, etc. etc.
	VMatrix tempView;

	// Store matrices off so we can restore them in RenderEnd().
	pRenderContext->GetMatrix(MATERIAL_VIEW, &tempView);

	// Force the user clip planes to use the old view matrix
	pRenderContext->EnableUserClipTransformOverride( true );
	pRenderContext->UserClipTransform( tempView );

	// The particle renderers want to do things in camera space
	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	size_t xyz_stride;
	const fltx4 *xyz = pParticles->GetM128AttributePtr( PARTICLE_ATTRIBUTE_XYZ, &xyz_stride );

	size_t rot_stride;
	const fltx4 *pRot = pParticles->GetM128AttributePtr( PARTICLE_ATTRIBUTE_ROTATION, &rot_stride );

	size_t rgb_stride;
	const fltx4 *pRGB = pParticles->GetM128AttributePtr( PARTICLE_ATTRIBUTE_TINT_RGB, &rgb_stride );

	size_t ct_stride;
	const fltx4 *pCreationTimeStamp = pParticles->GetM128AttributePtr( PARTICLE_ATTRIBUTE_CREATION_TIME, &ct_stride );

	size_t seq_stride;
	const fltx4 *pSequenceNumber = pParticles->GetM128AttributePtr( PARTICLE_ATTRIBUTE_SEQUENCE_NUMBER, &seq_stride );

	size_t ld_stride;
	const fltx4 *pLifeDuration = pParticles->GetM128AttributePtr( PARTICLE_ATTRIBUTE_LIFE_DURATION, &ld_stride );


	float flAgeScale;
	int nMaxParticlesInBatch = GetMaxParticlesPerBatch( pRenderContext, pMaterial, false );
	
	CSheet *pSheet = pParticles->m_Sheet();
	while ( nParticles )
	{
		int nParticlesInBatch = min( nMaxParticlesInBatch, nParticles );
		nParticles -= nParticlesInBatch;
		g_pParticleSystemMgr->TallyParticlesRendered( nParticlesInBatch * 4 );

		IMesh* pMesh = pRenderContext->GetDynamicMesh( true );
		CMeshBuilder meshBuilder;
		meshBuilder.Begin( pMesh, MATERIAL_QUADS, nParticlesInBatch );
		for( int i = 0; i < nParticlesInBatch; i++ )
		{
			int hParticle = (--pSortList)->m_nIndex;
			int nGroup = hParticle / 4;
			int nOffset = hParticle & 0x3;

			unsigned char ac = pSortList->m_nAlpha;
			if ( ac == 0 )
				continue;

			int nColorIndex = nGroup * rgb_stride;
			float r = SubFloat( pRGB[nColorIndex], nOffset );
			float g = SubFloat( pRGB[nColorIndex+1], nOffset );
			float b = SubFloat( pRGB[nColorIndex+2], nOffset );

			Assert( IsFinite(r) && IsFinite(g) && IsFinite(b) );
			Assert( (r >= 0.0f) && (g >= 0.0f) && (b >= 0.0f) );
			Assert( (r <= 1.0f) && (g <= 1.0f) && (b <= 1.0f) );

			unsigned char rc = FastFToC( r );
			unsigned char gc = FastFToC( g );
			unsigned char bc = FastFToC( b );

			float rad = pSortList->m_flRadius;

			int nXYZIndex = nGroup * xyz_stride;
			Vector vecWorldPos( SubFloat( xyz[ nXYZIndex ], nOffset ), SubFloat( xyz[ nXYZIndex+1 ], nOffset ), SubFloat( xyz[ nXYZIndex+2 ], nOffset ) );

			// Move the Particle if their is a camerabias
			if ( bCameraBias )
			{
				Vector vEyeDir = vecCamera - vecWorldPos;
				VectorNormalizeFast( vEyeDir );
				vEyeDir *= flCameraBias;
				vecWorldPos += vEyeDir;
			}

			Vector vecViewPos;
			Vector3DMultiplyPosition( tempView, vecWorldPos, vecViewPos );

			if ( !IsFinite( vecViewPos.x ) )
				continue;

			float rot = SubFloat( pRot[ nGroup * rot_stride ], nOffset );
			float ca = (float)cos(rot);
			float sa = (float)sin(rot);

			// Find the sample for this frame
			const SheetSequenceSample_t *pSample = &s_DefaultSheetSequence;
			if ( pSheet )
			{
				if ( m_bFitCycleToLifetime )
				{
					float flLifetime = SubFloat( pLifeDuration[ nGroup * ld_stride ], nOffset );
					flAgeScale = ( flLifetime > 0.0f ) ? ( 1.0f / flLifetime ) * SEQUENCE_SAMPLE_COUNT : 0.0f;
				}
				else
				{
					flAgeScale = m_flAnimationRate * SEQUENCE_SAMPLE_COUNT;
					if ( m_bAnimateInFPS )
					{
						int nSequence = SubFloat( pSequenceNumber[ nGroup * seq_stride ], nOffset );
						flAgeScale = flAgeScale / pSheet->m_flFrameSpan[nSequence];
					}
				}
				pSample = GetSampleForSequence( pSheet,
												SubFloat( pCreationTimeStamp[ nGroup * ct_stride ], nOffset ), 
												pParticles->m_flCurTime, 
												flAgeScale, 
												SubFloat( pSequenceNumber[ nGroup * seq_stride ], nOffset ) );
			}
			const SequenceSampleTextureCoords_t *pSample0 = &(pSample->m_TextureCoordData[0]);

			meshBuilder.Position3f( vecViewPos.x + (-ca + sa) * rad, vecViewPos.y + (-sa - ca) * rad, vecViewPos.z );
			meshBuilder.Color4ub( rc, gc, bc, ac );
			meshBuilder.TexCoord2f( 0, pSample0->m_fLeft_U0, pSample0->m_fBottom_V0 );
			meshBuilder.AdvanceVertex();

			meshBuilder.Position3f( vecViewPos.x + (-ca - sa) * rad, vecViewPos.y + (-sa + ca) * rad, vecViewPos.z );
			meshBuilder.Color4ub( rc, gc, bc, ac );
			meshBuilder.TexCoord2f( 0, pSample0->m_fLeft_U0, pSample0->m_fTop_V0 );
			meshBuilder.AdvanceVertex();

			meshBuilder.Position3f( vecViewPos.x + (ca - sa) * rad, vecViewPos.y + (sa + ca) * rad, vecViewPos.z );
			meshBuilder.Color4ub( rc, gc, bc, ac );
			meshBuilder.TexCoord2f( 0, pSample0->m_fRight_U0, pSample0->m_fTop_V0 );
			meshBuilder.AdvanceVertex();

			meshBuilder.Position3f( vecViewPos.x + (ca + sa) * rad, vecViewPos.y + (sa - ca) * rad, vecViewPos.z );
			meshBuilder.Color4ub( rc, gc, bc, ac );
			meshBuilder.TexCoord2f( 0, pSample0->m_fRight_U0, pSample0->m_fBottom_V0 );
			meshBuilder.AdvanceVertex();
		}
		meshBuilder.End();
		pMesh->Draw();
	}

	pRenderContext->EnableUserClipTransformOverride( false );

	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PopMatrix();
}



//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_OP_RenderSprites::RenderNonSpriteCardZRotating( CMeshBuilder &meshBuilder, C_OP_RenderSpritesContext_t *pCtx, SpriteRenderInfo_t& info, int hParticle, const Vector& vecCameraPos, ParticleRenderData_t const *pSortList ) const
{
	Assert( hParticle != -1 );
	int nGroup = hParticle / 4;
	int nOffset = hParticle & 0x3;

	unsigned char ac = pSortList->m_nAlpha;
	if ( ac == 0 )
		return;

	bool bCameraBias = ( &pCtx->m_VisibilityData )->m_flCameraBias != 0.0f;
	float flCameraBias = ( &pCtx->m_VisibilityData )->m_flCameraBias;

	int nColorIndex = nGroup * info.m_nRGBStride;
	float r = SubFloat( info.m_pRGB[nColorIndex], nOffset );
	float g = SubFloat( info.m_pRGB[nColorIndex+1], nOffset );
	float b = SubFloat( info.m_pRGB[nColorIndex+2], nOffset );

	Assert( IsFinite(r) && IsFinite(g) && IsFinite(b) );
	Assert( (r >= 0.0f) && (g >= 0.0f) && (b >= 0.0f) );
	Assert( (r <= 1.0f) && (g <= 1.0f) && (b <= 1.0f) );

	unsigned char rc = FastFToC( r );
	unsigned char gc = FastFToC( g );
	unsigned char bc = FastFToC( b );

	float rad = pSortList->m_flRadius;
	float rot = SubFloat( info.m_pRot[ nGroup * info.m_nRotStride ], nOffset );

	float ca = (float)cos(-rot);
	float sa = (float)sin(-rot);

	int nXYZIndex = nGroup * info.m_nXYZStride;
	Vector vecWorldPos( SubFloat( info.m_pXYZ[ nXYZIndex ], nOffset ), SubFloat( info.m_pXYZ[ nXYZIndex+1 ], nOffset ), SubFloat( info.m_pXYZ[ nXYZIndex+2 ], nOffset ) );

	// Move the Particle if their is a camerabias
	if ( bCameraBias )
	{
		Vector vEyeDir = vecCameraPos - vecWorldPos;
		VectorNormalizeFast( vEyeDir );
		vEyeDir *= flCameraBias;
		vecWorldPos += vEyeDir;
	}

	Vector vecViewToPos;
	VectorSubtract( vecWorldPos, vecCameraPos, vecViewToPos );
	float flLength = vecViewToPos.Length();
	if ( flLength < rad / 2 )
		return;

	Vector vecUp( 0, 0, 1 );
	Vector vecRight;
	CrossProduct( vecUp, vecCameraPos, vecRight );
	VectorNormalize( vecRight );

	// Find the sample for this frame
	const SheetSequenceSample_t *pSample = &s_DefaultSheetSequence;
	if ( info.m_pSheet )
	{
		pSample = GetSampleForSequence( info.m_pSheet,
			SubFloat( info.m_pCreationTimeStamp[ nGroup * info.m_nCreationTimeStride ], nOffset ), 
			info.m_pParticles->m_flCurTime, 
			info.m_flAgeScale, 
			SubFloat( info.m_pSequenceNumber[ nGroup * info.m_nSequenceStride ], nOffset ) );
	}

	const SequenceSampleTextureCoords_t *pSample0 = &(pSample->m_TextureCoordData[0]);
	vecRight *= rad;

	float x, y;
	Vector vecCorner;

	x = - ca - sa; y = - ca + sa;
	VectorMA( vecWorldPos, x, vecRight, vecCorner );
	meshBuilder.Position3f( vecCorner.x, vecCorner.y, vecCorner.z + y * rad );
	meshBuilder.Color4ub( rc, gc, bc, ac );
	meshBuilder.TexCoord2f( 0, pSample0->m_fLeft_U0, pSample0->m_fBottom_V0 );
	meshBuilder.AdvanceVertex();

	x = - ca + sa; y = + ca + sa;
	VectorMA( vecWorldPos, x, vecRight, vecCorner );
	meshBuilder.Position3f( vecCorner.x, vecCorner.y, vecCorner.z + y * rad );
	meshBuilder.Color4ub( rc, gc, bc, ac );
	meshBuilder.TexCoord2f( 0, pSample0->m_fLeft_U0, pSample0->m_fTop_V0 );
	meshBuilder.AdvanceVertex();

	x = + ca + sa; y = + ca - sa;
	VectorMA( vecWorldPos, x, vecRight, vecCorner );
	meshBuilder.Position3f( vecCorner.x, vecCorner.y, vecCorner.z + y * rad );
	meshBuilder.Color4ub( rc, gc, bc, ac );
	meshBuilder.TexCoord2f( 0, pSample0->m_fRight_U0, pSample0->m_fTop_V0 );
	meshBuilder.AdvanceVertex();

	x = + ca - sa; y = - ca - sa;
	VectorMA( vecWorldPos, x, vecRight, vecCorner );
	meshBuilder.Position3f( vecCorner.x, vecCorner.y, vecCorner.z + y * rad );
	meshBuilder.Color4ub( rc, gc, bc, ac );
	meshBuilder.TexCoord2f( 0, pSample0->m_fRight_U0, pSample0->m_fBottom_V0 );
	meshBuilder.AdvanceVertex();

	meshBuilder.FastIndex( info.m_nVertexOffset );
	meshBuilder.FastIndex( info.m_nVertexOffset + 1 );
	meshBuilder.FastIndex( info.m_nVertexOffset + 2 );
	meshBuilder.FastIndex( info.m_nVertexOffset );
	meshBuilder.FastIndex( info.m_nVertexOffset + 2 );
	meshBuilder.FastIndex( info.m_nVertexOffset + 3 );
	info.m_nVertexOffset += 4;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_OP_RenderSprites::RenderNonSpriteCardZRotating( CParticleCollection *pParticles, void *pContext, IMatRenderContext *pRenderContext, IMaterial *pMaterial ) const
{
	C_OP_RenderSpritesContext_t *pCtx = reinterpret_cast<C_OP_RenderSpritesContext_t *>( pContext );

	// NOTE: This is interesting to support because at first we won't have all the various
	// pixel-shader versions of SpriteCard, like modulate, twotexture, etc. etc.
	Vector vecCameraPos;
	pRenderContext->GetWorldSpaceCameraPosition( &vecCameraPos );
	float flAgeScale = m_flAnimationRate * SEQUENCE_SAMPLE_COUNT;

	SpriteRenderInfo_t info;
	info.Init( pParticles, 0, flAgeScale, 0, pParticles->m_Sheet() );

	int nParticles;
	const ParticleRenderData_t *pSortList = pParticles->GetRenderList( pRenderContext, true, &nParticles, &pCtx->m_VisibilityData );

	int nMaxParticlesInBatch = GetMaxParticlesPerBatch( pRenderContext, pMaterial, false );
	while ( nParticles )
	{
		int nParticlesInBatch = min( nMaxParticlesInBatch, nParticles );
		nParticles -= nParticlesInBatch;

		g_pParticleSystemMgr->TallyParticlesRendered( nParticlesInBatch * 4 * 3, nParticlesInBatch * 6 * 3 );

		IMesh* pMesh = pRenderContext->GetDynamicMesh( true );
		CMeshBuilder meshBuilder;
		meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, nParticlesInBatch * 4, nParticlesInBatch * 6 );
		info.m_nVertexOffset = 0;

		for( int i = 0; i < nParticlesInBatch; i++ )
		{
			int hParticle = (--pSortList)->m_nIndex;
			RenderNonSpriteCardZRotating( meshBuilder, pCtx, info, hParticle, vecCameraPos, pSortList );
		}
		meshBuilder.End();
		pMesh->Draw();
	}
}

void C_OP_RenderSprites::RenderUnsortedNonSpriteCardZRotating( CParticleCollection *pParticles, void *pContext, IMatRenderContext *pRenderContext, CMeshBuilder &meshBuilder, int nVertexOffset, int nFirstParticle, int nParticleCount ) const
{
	C_OP_RenderSpritesContext_t *pCtx = reinterpret_cast<C_OP_RenderSpritesContext_t *>( pContext );
	// NOTE: This is interesting to support because at first we won't have all the various
	// pixel-shader versions of SpriteCard, like modulate, twotexture, etc. etc.
	Vector vecCameraPos;
	pRenderContext->GetWorldSpaceCameraPosition( &vecCameraPos );

	float flAgeScale = m_flAnimationRate * SEQUENCE_SAMPLE_COUNT;
	SpriteRenderInfo_t info;
	info.Init( pParticles, nVertexOffset, flAgeScale, 0, pParticles->m_Sheet() );

	int nParticles;
	const ParticleRenderData_t *pSortList = pParticles->GetRenderList( pRenderContext, false, &nParticles, &pCtx->m_VisibilityData );

	int hParticle = nFirstParticle;
	for( int i = 0; i < nParticleCount; i++, hParticle++ )
	{
		RenderNonSpriteCardZRotating( meshBuilder, pCtx, info, hParticle, vecCameraPos, pSortList );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_OP_RenderSprites::RenderNonSpriteCardOriented( 
	CMeshBuilder &meshBuilder, C_OP_RenderSpritesContext_t *pCtx, SpriteRenderInfo_t& info, int hParticle, const Vector& vecCameraPos, ParticleRenderData_t const *pSortList, bool bUseYaw ) const
{
	Assert( hParticle != -1 );
	int nGroup = hParticle / 4;
	int nOffset = hParticle & 0x3;

	unsigned char ac = pSortList->m_nAlpha;
	if ( ac == 0 )
		return;

	bool bCameraBias = ( &pCtx->m_VisibilityData )->m_flCameraBias != 0.0f;
	float flCameraBias = ( &pCtx->m_VisibilityData )->m_flCameraBias;

	int nColorIndex = nGroup * info.m_nRGBStride;
	float r = SubFloat( info.m_pRGB[nColorIndex], nOffset );
	float g = SubFloat( info.m_pRGB[nColorIndex+1], nOffset );
	float b = SubFloat( info.m_pRGB[nColorIndex+2], nOffset );

	Assert( IsFinite(r) && IsFinite(g) && IsFinite(b) );
	Assert( (r >= 0.0f) && (g >= 0.0f) && (b >= 0.0f) );
	Assert( (r <= 1.0f) && (g <= 1.0f) && (b <= 1.0f) );

	unsigned char rc = FastFToC( r );
	unsigned char gc = FastFToC( g );
	unsigned char bc = FastFToC( b );

	float rad = pSortList->m_flRadius;
	float rot = SubFloat( info.m_pRot[ nGroup * info.m_nRotStride ], nOffset );

	float ca = (float)cos(-rot);
	float sa = (float)sin(-rot);

	int nXYZIndex = nGroup * info.m_nXYZStride;
	Vector vecWorldPos( SubFloat( info.m_pXYZ[ nXYZIndex ], nOffset ), SubFloat( info.m_pXYZ[ nXYZIndex+1 ], nOffset ), SubFloat( info.m_pXYZ[ nXYZIndex+2 ], nOffset ) );

	// Move the Particle if their is a camerabias
	if ( bCameraBias )
	{
		Vector vEyeDir = vecCameraPos - vecWorldPos;
		VectorNormalizeFast( vEyeDir );
		vEyeDir *= flCameraBias;
		vecWorldPos += vEyeDir;
	}

	Vector vecViewToPos;
	VectorSubtract( vecWorldPos, vecCameraPos, vecViewToPos );
	float flLength = vecViewToPos.Length();
	if ( flLength < rad / 2 )
		return;

	Vector vecNormal, vecRight, vecUp;
	if ( m_nOrientationControlPoint < 0 )
	{
		vecNormal.Init( 0, 0, 1 );
		vecRight.Init( 1, 0, 0 );
		vecUp.Init( 0, -1, 0 );
	}
	else
	{
		info.m_pParticles->GetControlPointOrientationAtCurrentTime( 
			m_nOrientationControlPoint, &vecRight, &vecUp, &vecNormal );
	}

	if ( bUseYaw )
	{
		float yaw = SubFloat( info.m_pYaw[nGroup * info.m_nYawStride], nOffset );

		if ( yaw != 0.0f )
		{
			Vector particleRight = Vector( 1, 0, 0 );
			yaw = RAD2DEG( yaw );	// I hate you source (VectorYawRotate will undo this)
			matrix3x4_t matRot;
			MatrixBuildRotationAboutAxis( vecUp, yaw, matRot );
			VectorRotate( vecRight, matRot, particleRight );
			vecRight = particleRight;
		}
	}

	// Find the sample for this frame
	const SheetSequenceSample_t *pSample = &s_DefaultSheetSequence;
	if ( info.m_pSheet )
	{
		pSample = GetSampleForSequence( info.m_pSheet,
			SubFloat( info.m_pCreationTimeStamp[ nGroup * info.m_nCreationTimeStride ], nOffset ), 
			info.m_pParticles->m_flCurTime, 
			info.m_flAgeScale, 
			SubFloat( info.m_pSequenceNumber[ nGroup * info.m_nSequenceStride ], nOffset ) );
	}

	const SequenceSampleTextureCoords_t *pSample0 = &(pSample->m_TextureCoordData[0]);
	vecRight *= rad;
	vecUp *= rad;

	float x, y;
	Vector vecCorner;

	x = + ca - sa; y = - ca - sa;
	VectorMA( vecWorldPos, x, vecRight, vecCorner );
	VectorMA( vecCorner, y, vecUp, vecCorner );
	meshBuilder.Position3f( vecCorner.x, vecCorner.y, vecCorner.z );
	meshBuilder.Color4ub( rc, gc, bc, ac );
	meshBuilder.TexCoord2f( 0, pSample0->m_fRight_U0, pSample0->m_fBottom_V0 );
	meshBuilder.AdvanceVertex();

	x = + ca + sa; y = + ca - sa;
	VectorMA( vecWorldPos, x, vecRight, vecCorner );
	VectorMA( vecCorner, y, vecUp, vecCorner );
	meshBuilder.Position3f( vecCorner.x, vecCorner.y, vecCorner.z );
	meshBuilder.Color4ub( rc, gc, bc, ac );
	meshBuilder.TexCoord2f( 0, pSample0->m_fRight_U0, pSample0->m_fTop_V0 );
	meshBuilder.AdvanceVertex();

	x = - ca + sa; y = + ca + sa;
	VectorMA( vecWorldPos, x, vecRight, vecCorner );
	VectorMA( vecCorner, y, vecUp, vecCorner );
	meshBuilder.Position3f( vecCorner.x, vecCorner.y, vecCorner.z );
	meshBuilder.Color4ub( rc, gc, bc, ac );
	meshBuilder.TexCoord2f( 0, pSample0->m_fLeft_U0, pSample0->m_fTop_V0 );
	meshBuilder.AdvanceVertex();

	x = - ca - sa; y = - ca + sa;
	VectorMA( vecWorldPos, x, vecRight, vecCorner );
	VectorMA( vecCorner, y, vecUp, vecCorner );
	meshBuilder.Position3f( vecCorner.x, vecCorner.y, vecCorner.z );
	meshBuilder.Color4ub( rc, gc, bc, ac );
	meshBuilder.TexCoord2f( 0, pSample0->m_fLeft_U0, pSample0->m_fBottom_V0 );
	meshBuilder.AdvanceVertex();

	meshBuilder.FastIndex( info.m_nVertexOffset );
	meshBuilder.FastIndex( info.m_nVertexOffset + 1 );
	meshBuilder.FastIndex( info.m_nVertexOffset + 2 );
	meshBuilder.FastIndex( info.m_nVertexOffset );
	meshBuilder.FastIndex( info.m_nVertexOffset + 2 );
	meshBuilder.FastIndex( info.m_nVertexOffset + 3 );
	info.m_nVertexOffset += 4;
}

void C_OP_RenderSprites::RenderNonSpriteCardOriented( CParticleCollection *pParticles, void *pContext, IMatRenderContext *pRenderContext, IMaterial *pMaterial, bool bUseYaw ) const
{
	C_OP_RenderSpritesContext_t *pCtx = reinterpret_cast<C_OP_RenderSpritesContext_t *>( pContext );

	// NOTE: This is interesting to support because at first we won't have all the various
	// pixel-shader versions of SpriteCard, like modulate, twotexture, etc. etc.
	Vector vecCameraPos;
	pRenderContext->GetWorldSpaceCameraPosition( &vecCameraPos );

	float flAgeScale = m_flAnimationRate * SEQUENCE_SAMPLE_COUNT;
	SpriteRenderInfo_t info;
	info.Init( pParticles, 0, flAgeScale, 0, pParticles->m_Sheet() );

	int nParticles;
	const ParticleRenderData_t *pSortList = pParticles->GetRenderList( pRenderContext, true, &nParticles, &pCtx->m_VisibilityData );

	int nMaxParticlesInBatch = GetMaxParticlesPerBatch( pRenderContext, pMaterial, false );
	while ( nParticles )
	{
		int nParticlesInBatch = min( nMaxParticlesInBatch, nParticles );
		nParticles -= nParticlesInBatch;

		g_pParticleSystemMgr->TallyParticlesRendered( nParticlesInBatch * 4 * 3, nParticlesInBatch * 6 * 3 );

		IMesh* pMesh = pRenderContext->GetDynamicMesh( true );
		CMeshBuilder meshBuilder;
		meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, nParticlesInBatch * 4, nParticlesInBatch * 6 );
		info.m_nVertexOffset = 0;

		for( int i = 0; i < nParticlesInBatch; i++)
		{
			int hParticle = (--pSortList)->m_nIndex;
			RenderNonSpriteCardOriented( meshBuilder, pCtx, info, hParticle, vecCameraPos, pSortList, bUseYaw );
		}

		meshBuilder.End();
		pMesh->Draw();
	}
}

void C_OP_RenderSprites::RenderUnsortedNonSpriteCardOriented( CParticleCollection *pParticles, void *pContext, IMatRenderContext *pRenderContext, CMeshBuilder &meshBuilder, int nVertexOffset, int nFirstParticle, int nParticleCount ) const
{
	C_OP_RenderSpritesContext_t *pCtx = reinterpret_cast<C_OP_RenderSpritesContext_t *>( pContext );
	// NOTE: This is interesting to support because at first we won't have all the various
	// pixel-shader versions of SpriteCard, like modulate, twotexture, etc. etc.
	Vector vecCameraPos;
	pRenderContext->GetWorldSpaceCameraPosition( &vecCameraPos );

	float flAgeScale = m_flAnimationRate * SEQUENCE_SAMPLE_COUNT;
	SpriteRenderInfo_t info;
	info.Init( pParticles, nVertexOffset, flAgeScale, 0, pParticles->m_Sheet() );

	int nParticles;
	const ParticleRenderData_t *pSortList = pParticles->GetRenderList( pRenderContext, false, &nParticles, &pCtx->m_VisibilityData );

	int hParticle = nFirstParticle;
	for( int i = 0; i < nParticleCount; i++, hParticle++ )
	{
		RenderNonSpriteCardOriented( meshBuilder, pCtx, info, hParticle, vecCameraPos, pSortList, false );
	}
}

void C_OP_RenderSprites::RenderSpriteCard( CMeshBuilder &meshBuilder, C_OP_RenderSpritesContext_t *pCtx, SpriteRenderInfo_t& info, int hParticle, ParticleRenderData_t const *pSortList, Vector *pCamera ) const
{
	Assert( hParticle != -1 );
	int nGroup = hParticle / 4;
	int nOffset = hParticle & 0x3;

	int nColorIndex = nGroup * info.m_nRGBStride;
	float r = SubFloat( info.m_pRGB[nColorIndex], nOffset );
	float g = SubFloat( info.m_pRGB[nColorIndex+1], nOffset );
	float b = SubFloat( info.m_pRGB[nColorIndex+2], nOffset );

	Assert( IsFinite(r) && IsFinite(g) && IsFinite(b) );
	Assert( (r >= -1e-6f) && (g >= -1e-6f) && (b >= -1e-6f) );
	if ( !HushAsserts() )
		Assert( (r <= 1.0f) && (g <= 1.0f) && (b <= 1.0f) );

	unsigned char rc = FastFToC( r );
	unsigned char gc = FastFToC( g );
	unsigned char bc = FastFToC( b );
	unsigned char ac = pSortList->m_nAlpha;

	float rad = pSortList->m_flRadius;
	if ( !IsFinite( rad ) )
	{
		return;
	}

	bool bCameraBias = ( &pCtx->m_VisibilityData )->m_flCameraBias != 0.0f;
	float flCameraBias = ( &pCtx->m_VisibilityData )->m_flCameraBias;

	float rot = SubFloat( info.m_pRot[ nGroup * info.m_nRotStride ], nOffset );
	float yaw = SubFloat( info.m_pYaw[ nGroup * info.m_nYawStride ], nOffset );

	int nXYZIndex = nGroup * info.m_nXYZStride;
	Vector vecWorldPos;
	vecWorldPos.x = SubFloat( info.m_pXYZ[ nXYZIndex ], nOffset );
	vecWorldPos.y = SubFloat( info.m_pXYZ[ nXYZIndex+1 ], nOffset );
	vecWorldPos.z = SubFloat( info.m_pXYZ[ nXYZIndex+2 ], nOffset );

	if ( bCameraBias )
	{
		Vector vEyeDir = *pCamera - vecWorldPos;
		VectorNormalizeFast( vEyeDir );
		vEyeDir *= flCameraBias;
		vecWorldPos += vEyeDir;
	}

	// Find the sample for this frame
	const SheetSequenceSample_t *pSample = &s_DefaultSheetSequence;
	if ( info.m_pSheet )
	{
		float flAgeScale = info.m_flAgeScale;
// 		if ( m_bFitCycleToLifetime )
// 		{
// 			float flLifetime = SubFloat( pLifeDuration[ nGroup * ld_stride ], nOffset );
// 			flAgeScale = ( flLifetime > 0.0f ) ? ( 1.0f / flLifetime ) * SEQUENCE_SAMPLE_COUNT : 0.0f;
// 		}
		if ( m_bAnimateInFPS )
		{
			int nSequence = SubFloat( info.m_pSequenceNumber[ nGroup * info.m_nSequenceStride ], nOffset );
			flAgeScale = flAgeScale / info.m_pParticles->m_Sheet()->m_flFrameSpan[nSequence];
		}
		pSample = GetSampleForSequence( info.m_pSheet,
			SubFloat( info.m_pCreationTimeStamp[ nGroup * info.m_nCreationTimeStride ], nOffset ), 
			info.m_pParticles->m_flCurTime, 
			flAgeScale,
			SubFloat( info.m_pSequenceNumber[ nGroup * info.m_nSequenceStride ], nOffset ) );
	}

	const SequenceSampleTextureCoords_t *pSample0 = &(pSample->m_TextureCoordData[0]);
	const SequenceSampleTextureCoords_t *pSecondTexture0 = &(pSample->m_TextureCoordData[1]);

	// Submit 1 (instanced) or 4 (non-instanced) verts (if we're instancing, we don't produce indices either)
	meshBuilder.Position3f( vecWorldPos.x, vecWorldPos.y, vecWorldPos.z );
	meshBuilder.Color4ub( rc, gc, bc, ac );
	meshBuilder.TexCoord4f( 0, pSample0->m_fLeft_U0, pSample0->m_fTop_V0, pSample0->m_fRight_U0, pSample0->m_fBottom_V0 );
	meshBuilder.TexCoord4f( 1, pSample0->m_fLeft_U1, pSample0->m_fTop_V1, pSample0->m_fRight_U1, pSample0->m_fBottom_V1 );
	meshBuilder.TexCoord4f( 2, pSample->m_fBlendFactor, rot, rad, yaw );
	// FIXME: change the vertex decl (remove texcoord3/cornerid) if instancing - need to adjust elements beyond texcoord3 down, though
	if ( !bUseInstancing )
		meshBuilder.TexCoord2f( 3, 0, 0 );
	meshBuilder.TexCoord4f( 4, pSecondTexture0->m_fLeft_U0, pSecondTexture0->m_fTop_V0, pSecondTexture0->m_fRight_U0, pSecondTexture0->m_fBottom_V0 );
	meshBuilder.AdvanceVertex();

	if ( !bUseInstancing )
	{
		meshBuilder.Position3f( vecWorldPos.x, vecWorldPos.y, vecWorldPos.z );
		meshBuilder.Color4ub( rc, gc, bc, ac );
		meshBuilder.TexCoord4f( 0, pSample0->m_fLeft_U0, pSample0->m_fTop_V0, pSample0->m_fRight_U0, pSample0->m_fBottom_V0 );
		meshBuilder.TexCoord4f( 1, pSample0->m_fLeft_U1, pSample0->m_fTop_V1, pSample0->m_fRight_U1, pSample0->m_fBottom_V1 );
		meshBuilder.TexCoord4f( 2, pSample->m_fBlendFactor, rot, rad, yaw );
		meshBuilder.TexCoord2f( 3, 1, 0 );
		meshBuilder.TexCoord4f( 4, pSecondTexture0->m_fLeft_U0, pSecondTexture0->m_fTop_V0, pSecondTexture0->m_fRight_U0, pSecondTexture0->m_fBottom_V0 );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3f( vecWorldPos.x, vecWorldPos.y, vecWorldPos.z );
		meshBuilder.Color4ub( rc, gc, bc, ac );
		meshBuilder.TexCoord4f( 0, pSample0->m_fLeft_U0, pSample0->m_fTop_V0, pSample0->m_fRight_U0, pSample0->m_fBottom_V0 );
		meshBuilder.TexCoord4f( 1, pSample0->m_fLeft_U1, pSample0->m_fTop_V1, pSample0->m_fRight_U1, pSample0->m_fBottom_V1 );
		meshBuilder.TexCoord4f( 2, pSample->m_fBlendFactor, rot, rad, yaw );
		meshBuilder.TexCoord2f( 3, 1, 1 );
		meshBuilder.TexCoord4f( 4, pSecondTexture0->m_fLeft_U0, pSecondTexture0->m_fTop_V0, pSecondTexture0->m_fRight_U0, pSecondTexture0->m_fBottom_V0 );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3f( vecWorldPos.x, vecWorldPos.y, vecWorldPos.z );
		meshBuilder.Color4ub( rc, gc, bc, ac );
		meshBuilder.TexCoord4f( 0, pSample0->m_fLeft_U0, pSample0->m_fTop_V0, pSample0->m_fRight_U0, pSample0->m_fBottom_V0 );
		meshBuilder.TexCoord4f( 1, pSample0->m_fLeft_U1, pSample0->m_fTop_V1, pSample0->m_fRight_U1, pSample0->m_fBottom_V1 );
		meshBuilder.TexCoord4f( 2, pSample->m_fBlendFactor, rot, rad, yaw );
		meshBuilder.TexCoord2f( 3, 0, 1 );
		meshBuilder.TexCoord4f( 4, pSecondTexture0->m_fLeft_U0, pSecondTexture0->m_fTop_V0, pSecondTexture0->m_fRight_U0, pSecondTexture0->m_fBottom_V0 );
		meshBuilder.AdvanceVertex();

		meshBuilder.FastIndex( info.m_nVertexOffset );
		meshBuilder.FastIndex( info.m_nVertexOffset + 1 );
		meshBuilder.FastIndex( info.m_nVertexOffset + 2 );
		meshBuilder.FastIndex( info.m_nVertexOffset );
		meshBuilder.FastIndex( info.m_nVertexOffset + 2 );
		meshBuilder.FastIndex( info.m_nVertexOffset + 3 );
		info.m_nVertexOffset += 4;
	}
}

void C_OP_RenderSprites::RenderTwoSequenceSpriteCard( CMeshBuilder &meshBuilder, C_OP_RenderSpritesContext_t *pCtx, SpriteRenderInfo_t& info, int hParticle, ParticleRenderData_t const *pSortList, Vector *pCamera ) const
{
	Assert( hParticle != -1 );
	int nGroup = hParticle / 4;
	int nOffset = hParticle & 0x3;

	int nColorIndex = nGroup * info.m_nRGBStride;
	float r = SubFloat( info.m_pRGB[nColorIndex], nOffset );
	float g = SubFloat( info.m_pRGB[nColorIndex+1], nOffset );
	float b = SubFloat( info.m_pRGB[nColorIndex+2], nOffset );

	Assert( IsFinite(r) && IsFinite(g) && IsFinite(b) );
	Assert( (r >= 0.0f) && (g >= 0.0f) && (b >= 0.0f) );
	Assert( (r <= 1.0f) && (g <= 1.0f) && (b <= 1.0f) );

	unsigned char rc = FastFToC( r );
	unsigned char gc = FastFToC( g );
	unsigned char bc = FastFToC( b );
	unsigned char ac = pSortList->m_nAlpha;

	bool bCameraBias = ( &pCtx->m_VisibilityData )->m_flCameraBias != 0.0f;
	float flCameraBias = ( &pCtx->m_VisibilityData )->m_flCameraBias;

	float rad = pSortList->m_flRadius;
	float rot = SubFloat( info.m_pRot[ nGroup * info.m_nRotStride ], nOffset );
	float yaw = SubFloat( info.m_pYaw[ nGroup * info.m_nYawStride ], nOffset );

	int nXYZIndex = nGroup * info.m_nXYZStride;
	Vector vecWorldPos;
	vecWorldPos.x = SubFloat( info.m_pXYZ[ nXYZIndex ], nOffset );
	vecWorldPos.y = SubFloat( info.m_pXYZ[ nXYZIndex+1 ], nOffset );
	vecWorldPos.z = SubFloat( info.m_pXYZ[ nXYZIndex+2 ], nOffset );

	if ( bCameraBias )
	{
		Vector vEyeDir = *pCamera - vecWorldPos;
		VectorNormalizeFast( vEyeDir );
		vEyeDir *= flCameraBias;
		vecWorldPos += vEyeDir;
	}

	// Find the sample for this frame
	const SheetSequenceSample_t *pSample = &s_DefaultSheetSequence;
	const SheetSequenceSample_t *pSample1 = &s_DefaultSheetSequence;
	if ( info.m_pSheet )
	{
		pSample = GetSampleForSequence( info.m_pSheet,
			SubFloat( info.m_pCreationTimeStamp[ nGroup * info.m_nCreationTimeStride ], nOffset ), 
			info.m_pParticles->m_flCurTime, 
			info.m_flAgeScale, 
			SubFloat( info.m_pSequenceNumber[ nGroup * info.m_nSequenceStride ], nOffset ) );
		pSample1 = GetSampleForSequence( info.m_pSheet,
			SubFloat( info.m_pCreationTimeStamp[ nGroup * info.m_nCreationTimeStride ], nOffset ), 
			info.m_pParticles->m_flCurTime, 
			info.m_flAgeScale2, 
			SubFloat( info.m_pSequence1Number[ nGroup * info.m_nSequence1Stride ], nOffset ) );
	}

	const SequenceSampleTextureCoords_t *pSample0 = &(pSample->m_TextureCoordData[0]);
	const SequenceSampleTextureCoords_t *pSecondTexture0 = &(pSample->m_TextureCoordData[1]);
	const SequenceSampleTextureCoords_t *pSample1Frame = &(pSample1->m_TextureCoordData[0]);

	// Submit 1 (instanced) or 4 (non-instanced) verts (if we're instancing, we don't produce indices either)
	meshBuilder.Position3f( vecWorldPos.x, vecWorldPos.y, vecWorldPos.z );
	meshBuilder.Color4ub( rc, gc, bc, ac );
	meshBuilder.TexCoord4f( 0, pSample0->m_fLeft_U0, pSample0->m_fTop_V0, pSample0->m_fRight_U0, pSample0->m_fBottom_V0 );
	meshBuilder.TexCoord4f( 1, pSample0->m_fLeft_U1, pSample0->m_fTop_V1, pSample0->m_fRight_U1, pSample0->m_fBottom_V1 );
	meshBuilder.TexCoord4f( 2, pSample->m_fBlendFactor, rot, rad, yaw );
	// FIXME: change the vertex decl (remove texcoord3/cornerid) if instancing - need to adjust elements beyond texcoord3 down, though
	if ( ! bUseInstancing )
		meshBuilder.TexCoord2f( 3, 0, 0 );
	meshBuilder.TexCoord4f( 4, pSecondTexture0->m_fLeft_U0, pSecondTexture0->m_fTop_V0, pSecondTexture0->m_fRight_U0, pSecondTexture0->m_fBottom_V0 );
	meshBuilder.TexCoord4f( 5, pSample1Frame->m_fLeft_U0, pSample1Frame->m_fTop_V0, pSample1Frame->m_fRight_U0, pSample1Frame->m_fBottom_V0 );
	meshBuilder.TexCoord4f( 6, pSample1Frame->m_fLeft_U1, pSample1Frame->m_fTop_V1, pSample1Frame->m_fRight_U1, pSample1Frame->m_fBottom_V1 );
	meshBuilder.TexCoord4f( 7, pSample1->m_fBlendFactor, 0, 0, 0 );
	meshBuilder.AdvanceVertex();

	if ( !bUseInstancing )
	{
		meshBuilder.Position3f( vecWorldPos.x, vecWorldPos.y, vecWorldPos.z );
		meshBuilder.Color4ub( rc, gc, bc, ac );
		meshBuilder.TexCoord4f( 0, pSample0->m_fLeft_U0, pSample0->m_fTop_V0, pSample0->m_fRight_U0, pSample0->m_fBottom_V0 );
		meshBuilder.TexCoord4f( 1, pSample0->m_fLeft_U1, pSample0->m_fTop_V1, pSample0->m_fRight_U1, pSample0->m_fBottom_V1 );
		meshBuilder.TexCoord4f( 2, pSample->m_fBlendFactor, rot, rad, yaw );
		meshBuilder.TexCoord2f( 3, 1, 0 );
		meshBuilder.TexCoord4f( 4, pSecondTexture0->m_fLeft_U0, pSecondTexture0->m_fTop_V0, pSecondTexture0->m_fRight_U0, pSecondTexture0->m_fBottom_V0 );
		meshBuilder.TexCoord4f( 5, pSample1Frame->m_fLeft_U0, pSample1Frame->m_fTop_V0, pSample1Frame->m_fRight_U0, pSample1Frame->m_fBottom_V0 );
		meshBuilder.TexCoord4f( 6, pSample1Frame->m_fLeft_U1, pSample1Frame->m_fTop_V1, pSample1Frame->m_fRight_U1, pSample1Frame->m_fBottom_V1 );
		meshBuilder.TexCoord4f( 7, pSample1->m_fBlendFactor, 0, 0, 0 );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3f( vecWorldPos.x, vecWorldPos.y, vecWorldPos.z );
		meshBuilder.Color4ub( rc, gc, bc, ac );
		meshBuilder.TexCoord4f( 0, pSample0->m_fLeft_U0, pSample0->m_fTop_V0, pSample0->m_fRight_U0, pSample0->m_fBottom_V0 );
		meshBuilder.TexCoord4f( 1, pSample0->m_fLeft_U1, pSample0->m_fTop_V1, pSample0->m_fRight_U1, pSample0->m_fBottom_V1 );
		meshBuilder.TexCoord4f( 2, pSample->m_fBlendFactor, rot, rad, yaw );
		meshBuilder.TexCoord2f( 3, 1, 1 );
		meshBuilder.TexCoord4f( 4, pSecondTexture0->m_fLeft_U0, pSecondTexture0->m_fTop_V0, pSecondTexture0->m_fRight_U0, pSecondTexture0->m_fBottom_V0 );
		meshBuilder.TexCoord4f( 5, pSample1Frame->m_fLeft_U0, pSample1Frame->m_fTop_V0, pSample1Frame->m_fRight_U0, pSample1Frame->m_fBottom_V0 );
		meshBuilder.TexCoord4f( 6, pSample1Frame->m_fLeft_U1, pSample1Frame->m_fTop_V1, pSample1Frame->m_fRight_U1, pSample1Frame->m_fBottom_V1 );
		meshBuilder.TexCoord4f( 7, pSample1->m_fBlendFactor, 0, 0, 0 );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3f( vecWorldPos.x, vecWorldPos.y, vecWorldPos.z );
		meshBuilder.Color4ub( rc, gc, bc, ac );
		meshBuilder.TexCoord4f( 0, pSample0->m_fLeft_U0, pSample0->m_fTop_V0, pSample0->m_fRight_U0, pSample0->m_fBottom_V0 );
		meshBuilder.TexCoord4f( 1, pSample0->m_fLeft_U1, pSample0->m_fTop_V1, pSample0->m_fRight_U1, pSample0->m_fBottom_V1 );
		meshBuilder.TexCoord4f( 2, pSample->m_fBlendFactor, rot, rad, yaw );
		meshBuilder.TexCoord2f( 3, 0, 1 );
		meshBuilder.TexCoord4f( 4, pSecondTexture0->m_fLeft_U0, pSecondTexture0->m_fTop_V0, pSecondTexture0->m_fRight_U0, pSecondTexture0->m_fBottom_V0 );
		meshBuilder.TexCoord4f( 5, pSample1Frame->m_fLeft_U0, pSample1Frame->m_fTop_V0, pSample1Frame->m_fRight_U0, pSample1Frame->m_fBottom_V0 );
		meshBuilder.TexCoord4f( 6, pSample1Frame->m_fLeft_U1, pSample1Frame->m_fTop_V1, pSample1Frame->m_fRight_U1, pSample1Frame->m_fBottom_V1 );
		meshBuilder.TexCoord4f( 7, pSample1->m_fBlendFactor, 0, 0, 0 );
		meshBuilder.AdvanceVertex();

		meshBuilder.FastIndex( info.m_nVertexOffset );
		meshBuilder.FastIndex( info.m_nVertexOffset + 1 );
		meshBuilder.FastIndex( info.m_nVertexOffset + 2 );
		meshBuilder.FastIndex( info.m_nVertexOffset );
		meshBuilder.FastIndex( info.m_nVertexOffset + 2 );
		meshBuilder.FastIndex( info.m_nVertexOffset + 3 );
		info.m_nVertexOffset += 4;
	}
}



//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_OP_RenderSprites::Render( IMatRenderContext *pRenderContext, CParticleCollection *pParticles, void *pContext ) const
{
	IMaterial *pMaterial = pParticles->m_pDef->GetMaterial();
	C_OP_RenderSpritesContext_t *pCtx = reinterpret_cast<C_OP_RenderSpritesContext_t *>( pContext );

	if ( pCtx->m_VisibilityData.m_bUseVisibility )
	{
		SetupParticleVisibility( pParticles, &pCtx->m_VisibilityData, &VisibilityInputs, &pCtx->m_nQueryHandle );
	}


	IMaterialVar* pVar = pMaterial->FindVarFast( "$orientation", &pCtx->m_nOrientationVarToken );
	if ( pVar )
	{
		pVar->SetIntValue( m_nOrientationType );
	}

	pRenderContext->Bind( pMaterial );

	if ( !pMaterial->IsSpriteCard() )
	{
		switch( m_nOrientationType )
		{
		case 0:
			RenderNonSpriteCardCameraFacing( pParticles, pContext, pRenderContext, pMaterial );
			break;

		case 1:
			RenderNonSpriteCardZRotating( pParticles, pContext, pRenderContext, pMaterial );
			break;
		
		case 2:
			RenderNonSpriteCardOriented( pParticles, pContext, pRenderContext, pMaterial, false );
			break;

		case 3:
			RenderNonSpriteCardOriented( pParticles, pContext, pRenderContext, pMaterial, true );
			break;
		}

		return;
	}

	if ( m_nOrientationType == 2 )
	{
		pVar = pMaterial->FindVarFast( "$orientationMatrix", &pCtx->m_nOrientationMatrixVarToken );
		if ( pVar )
		{
			VMatrix mat;
			if ( m_nOrientationControlPoint < 0 )
			{
				MatrixSetIdentity( mat );
			}
			else
			{
				pParticles->GetControlPointTransformAtCurrentTime( m_nOrientationControlPoint, &mat );
			}
			pVar->SetMatrixValue( mat );
		}
	}

	float flAgeScale = m_flAnimationRate * SEQUENCE_SAMPLE_COUNT;
	float flAgeScale2 = m_flAnimationRate2 * SEQUENCE_SAMPLE_COUNT;

	SpriteRenderInfo_t info;
	info.Init( pParticles, 0, flAgeScale, flAgeScale2, pParticles->m_Sheet() );

	MaterialPrimitiveType_t	primType = bUseInstancing ? MATERIAL_INSTANCED_QUADS : MATERIAL_TRIANGLES;
	int nMaxParticlesInBatch = GetMaxParticlesPerBatch( pRenderContext, pMaterial, bUseInstancing );

	int nParticles;
	const ParticleRenderData_t *pSortList = pParticles->GetRenderList( pRenderContext, true, &nParticles, &pCtx->m_VisibilityData );

	Vector vecCamera;
	pRenderContext->GetWorldSpaceCameraPosition( &vecCamera );

	while ( nParticles )
	{
		int nParticlesInBatch = min( nMaxParticlesInBatch, nParticles );
		nParticles -= nParticlesInBatch;

		int vertexCount	= bUseInstancing ? nParticlesInBatch	: nParticlesInBatch * 4;
		int indexCount	= bUseInstancing ? 0					: nParticlesInBatch * 6;

		IMesh* pMesh = pRenderContext->GetDynamicMesh( true );
		CMeshBuilder meshBuilder;

		if ( bUseInstancing )
		{
			g_pParticleSystemMgr->TallyParticlesRendered( vertexCount * ( primType == MATERIAL_TRIANGLES ? 3 : 4 ) );
			meshBuilder.Begin( pMesh, primType, vertexCount );
		}
		else
		{
			g_pParticleSystemMgr->TallyParticlesRendered( vertexCount * ( primType == MATERIAL_TRIANGLES ? 3 : 4 ), indexCount * ( primType == MATERIAL_TRIANGLES ? 3 : 4 ) );
			meshBuilder.Begin( pMesh, primType, vertexCount, indexCount );
		}
		info.m_nVertexOffset = 0;
		if ( meshBuilder.TextureCoordinateSize( 5 ) )		// second sequence?
		{
			for( int i = 0; i < nParticlesInBatch; i++ )
			{
				int hParticle = (--pSortList)->m_nIndex;
				RenderTwoSequenceSpriteCard( meshBuilder, pCtx, info, hParticle, pSortList, &vecCamera );
			}
		}
		else
		{			
			for( int i = 0; i < nParticlesInBatch; i++ )
			{
				int hParticle = (--pSortList)->m_nIndex;
				RenderSpriteCard( meshBuilder, pCtx, info, hParticle, pSortList, &vecCamera );
			}
		}
		meshBuilder.End();
		pMesh->Draw();
	}
}


void C_OP_RenderSprites::RenderUnsorted( CParticleCollection *pParticles, void *pContext, IMatRenderContext *pRenderContext, CMeshBuilder &meshBuilder, int nVertexOffset, int nFirstParticle, int nParticleCount ) const
{
	if ( !pParticles->m_pDef->GetMaterial()->IsSpriteCard() )
	{
		switch( m_nOrientationType )
		{
		case 0:
			// FIXME: Implement! Requires removing MATERIAL_VIEW modification from sorted version
			Warning( "C_OP_RenderSprites::RenderUnsorted: Attempting to use an unimplemented sprite renderer for system \"%s\"!\n",
				pParticles->m_pDef->GetName() );
//			RenderUnsortedNonSpriteCardCameraFacing( pParticles, pContext, pRenderContext, meshBuilder, nVertexOffset, nFirstParticle, nParticleCount );
			break;

		case 1:
			RenderUnsortedNonSpriteCardZRotating( pParticles, pContext, pRenderContext, meshBuilder, nVertexOffset, nFirstParticle, nParticleCount );
			break;

		case 2:
			RenderUnsortedNonSpriteCardOriented( pParticles, pContext, pRenderContext, meshBuilder, nVertexOffset, nFirstParticle, nParticleCount );
			break;
		}
		return;
	}

	C_OP_RenderSpritesContext_t *pCtx = reinterpret_cast<C_OP_RenderSpritesContext_t *>( pContext );

	float flAgeScale = m_flAnimationRate * SEQUENCE_SAMPLE_COUNT;
	float flAgeScale2 = m_flAnimationRate2 * SEQUENCE_SAMPLE_COUNT;

	SpriteRenderInfo_t info;
	info.Init( pParticles, 0, flAgeScale, flAgeScale2, pParticles->m_Sheet() );

	int hParticle = nFirstParticle;

	int nParticles;
	const ParticleRenderData_t *pSortList = pParticles->GetRenderList( pRenderContext, false, &nParticles, &pCtx->m_VisibilityData );

	Vector vecCamera;
	pRenderContext->GetWorldSpaceCameraPosition( &vecCamera );

	for( int i = 0; i < nParticleCount; i++, hParticle++ )
	{
		RenderSpriteCard( meshBuilder, pCtx, info, hParticle, pSortList, &vecCamera );
	}
}

//
//
//
//

struct SpriteTrailRenderInfo_t : public SpriteRenderInfo_t
{
	size_t m_nPrevXYZStride;
	const fltx4 *m_pPrevXYZ;
	size_t length_stride;
	const fltx4 *m_pLength;

	const fltx4 *m_pCreationTime;
	size_t m_nCreationTimeStride;


	void Init( CParticleCollection *pParticles, int nVertexOffset, float flAgeScale, CSheet *pSheet )
	{
		SpriteRenderInfo_t::Init( pParticles, nVertexOffset, flAgeScale, 0, pSheet );
		m_pParticles = pParticles;
		m_pPrevXYZ = pParticles->GetM128AttributePtr( PARTICLE_ATTRIBUTE_PREV_XYZ, &m_nPrevXYZStride );
		m_pLength = pParticles->GetM128AttributePtr( PARTICLE_ATTRIBUTE_TRAIL_LENGTH, &length_stride );
		m_pCreationTime = pParticles->GetM128AttributePtr( PARTICLE_ATTRIBUTE_CREATION_TIME, &m_nCreationTimeStride );
	}
};

class C_OP_RenderSpritesTrail : public CParticleRenderOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_OP_RenderSpritesTrail );

	struct C_OP_RenderSpriteTrailContext_t
	{
		CParticleVisibilityData m_VisibilityData;
		int		m_nQueryHandle;
	};

	size_t GetRequiredContextBytes( void ) const
	{
		return sizeof( C_OP_RenderSpriteTrailContext_t );
	}

	virtual void InitializeContextData( CParticleCollection *pParticles, void *pContext ) const
	{
		C_OP_RenderSpriteTrailContext_t *pCtx = reinterpret_cast<C_OP_RenderSpriteTrailContext_t *>( pContext );
		if ( VisibilityInputs.m_nCPin >= 0 )
			pCtx->m_VisibilityData.m_bUseVisibility = true;
		else
			pCtx->m_VisibilityData.m_bUseVisibility = false;
		pCtx->m_VisibilityData.m_flCameraBias = VisibilityInputs.m_flCameraBias;
	}

	uint32 GetWrittenAttributes( void ) const
	{
		return 0;
	}

	void InitParams( CParticleSystemDefinition *pDef, CDmxElement *pElement )
	{
	}

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_XYZ_MASK | PARTICLE_ATTRIBUTE_PREV_XYZ_MASK | PARTICLE_ATTRIBUTE_RADIUS_MASK | 
			PARTICLE_ATTRIBUTE_TINT_RGB_MASK | PARTICLE_ATTRIBUTE_ALPHA_MASK | PARTICLE_ATTRIBUTE_CREATION_TIME_MASK |
			PARTICLE_ATTRIBUTE_SEQUENCE_NUMBER_MASK | PARTICLE_ATTRIBUTE_TRAIL_LENGTH_MASK;
	}

	virtual int GetParticlesToRender( CParticleCollection *pParticles, void *pContext, int nFirstParticle, int nRemainingVertices, int nRemainingIndices, int *pVertsUsed, int *pIndicesUsed ) const ;
	virtual void Render( IMatRenderContext *pRenderContext, CParticleCollection *pParticles, void *pContext ) const;
	virtual void RenderUnsorted( CParticleCollection *pParticles, void *pContext, IMatRenderContext *pRenderContext, CMeshBuilder &meshBuilder, int nVertexOffset, int nFirstParticle, int nParticleCount ) const;

	void RenderSpriteTrail( CMeshBuilder &meshBuilder, SpriteTrailRenderInfo_t& info, int hParticle, const Vector &vecCameraPos, float flOODt, ParticleRenderData_t const *pSortlist ) const;

	float m_flAnimationRate;
	float m_flLengthFadeInTime;
	float m_flMaxLength;
	float m_flMinLength;

};

DEFINE_PARTICLE_OPERATOR( C_OP_RenderSpritesTrail, "render_sprite_trail", OPERATOR_SINGLETON );

BEGIN_PARTICLE_RENDER_OPERATOR_UNPACK( C_OP_RenderSpritesTrail ) 
	DMXELEMENT_UNPACK_FIELD( "animation rate", ".1", float, m_flAnimationRate )
	DMXELEMENT_UNPACK_FIELD( "length fade in time", "0", float, m_flLengthFadeInTime )
	DMXELEMENT_UNPACK_FIELD( "max length", "2000", float, m_flMaxLength )
	DMXELEMENT_UNPACK_FIELD( "min length", "0", float, m_flMinLength )
END_PARTICLE_OPERATOR_UNPACK( C_OP_RenderSpritesTrail )

int C_OP_RenderSpritesTrail::GetParticlesToRender( CParticleCollection *pParticles, 
												   void *pContext, int nFirstParticle, int nRemainingVertices,
												   int nRemainingIndices, 
												   int *pVertsUsed, int *pIndicesUsed ) const
{
	int nMaxParticles = ( (nRemainingVertices / 4) > (nRemainingIndices / 6) ) ? nRemainingIndices / 6 : nRemainingVertices / 4;
	int nParticleCount = pParticles->m_nActiveParticles - nFirstParticle;
	if ( nParticleCount > nMaxParticles )
	{
		nParticleCount = nMaxParticles;
	}
	*pVertsUsed = nParticleCount * 4;
	*pIndicesUsed = nParticleCount * 6;
	return nParticleCount;
}

void C_OP_RenderSpritesTrail::RenderSpriteTrail( CMeshBuilder &meshBuilder, 
												 SpriteTrailRenderInfo_t& info, int hParticle,
												 const Vector &vecCameraPos, float flOODt, ParticleRenderData_t const *pSortList ) const
{
	Assert( hParticle != -1 );
	int nGroup = hParticle / 4;
	int nOffset = hParticle & 0x3;

	// Setup our alpha
	unsigned char ac = pSortList->m_nAlpha;
	if ( ac == 0 )
		return;

	// Setup our colors
	int nColorIndex = nGroup * info.m_nRGBStride;
	float r = SubFloat( info.m_pRGB[nColorIndex], nOffset );
	float g = SubFloat( info.m_pRGB[nColorIndex+1], nOffset );
	float b = SubFloat( info.m_pRGB[nColorIndex+2], nOffset );

	Assert( IsFinite(r) && IsFinite(g) && IsFinite(b) );
	Assert( (r >= -1e-6f) && (g >= -1e-6f) && (b >= -1e-6f) );
	Assert( (r <= 1.0f) && (g <= 1.0f) && (b <= 1.0f) );

	unsigned char rc = FastFToC( r );
	unsigned char gc = FastFToC( g );
	unsigned char bc = FastFToC( b );

	// Setup the scale and rotation
	float rad = pSortList->m_flRadius;

	// Find the sample for this frame
	const SheetSequenceSample_t *pSample = &s_DefaultSheetSequence;
	if ( info.m_pSheet )
	{
		pSample = GetSampleForSequence( info.m_pSheet,
			SubFloat( info.m_pCreationTimeStamp[ nGroup * info.m_nCreationTimeStride ], nOffset ), 
			info.m_pParticles->m_flCurTime, 
			info.m_flAgeScale, 
			SubFloat( info.m_pSequenceNumber[ nGroup * info.m_nSequenceStride ], nOffset ) );
	}

	const SequenceSampleTextureCoords_t *pSample0 = &(pSample->m_TextureCoordData[0]);

	int nCreationTimeIndex = nGroup * info.m_nCreationTimeStride;
	float flAge = info.m_pParticles->m_flCurTime - SubFloat( info.m_pCreationTimeStamp[ nCreationTimeIndex ], nOffset );

	float flLengthScale = ( flAge >= m_flLengthFadeInTime ) ? 1.0 : ( flAge / m_flLengthFadeInTime );

	int nXYZIndex = nGroup * info.m_nXYZStride;
	Vector vecWorldPos( SubFloat( info.m_pXYZ[ nXYZIndex ], nOffset ), SubFloat( info.m_pXYZ[ nXYZIndex+1 ], nOffset ), SubFloat( info.m_pXYZ[ nXYZIndex+2 ], nOffset ) );
	Vector vecViewPos = vecWorldPos;

	// Get our screenspace last position
	int nPrevXYZIndex = nGroup * info.m_nPrevXYZStride;
	Vector vecPrevWorldPos( SubFloat( info.m_pPrevXYZ[ nPrevXYZIndex ], nOffset ), SubFloat( info.m_pPrevXYZ[ nPrevXYZIndex+1 ], nOffset ), SubFloat( info.m_pPrevXYZ[ nPrevXYZIndex+2 ], nOffset ) );
	Vector vecPrevViewPos = vecPrevWorldPos;

	// Get the delta direction and find the magnitude, then scale the length by the desired length amount
	Vector vecDelta;
	VectorSubtract( vecPrevViewPos, vecViewPos, vecDelta );
	float flMag = VectorNormalize( vecDelta );
	float flLength = flLengthScale * flMag * flOODt * SubFloat( info.m_pLength[ nGroup * info.length_stride ], nOffset );
	if ( flLength <= 0.0f )
		return;

	flLength = max( m_flMinLength, min( m_flMaxLength, flLength ) );

	vecDelta *= flLength;

	// Fade the width as the length fades to keep it at a square aspect ratio
	if ( flLength < rad )
	{
		rad = flLength;
	}

	// Find our tangent direction which "fattens" the line
	Vector vDirToBeam, vTangentY;
	VectorSubtract( vecWorldPos, vecCameraPos, vDirToBeam );
	CrossProduct( vDirToBeam, vecDelta, vTangentY );
	VectorNormalizeFast( vTangentY );

	// Calculate the verts we'll use as our points
	Vector verts[4];
	VectorMA( vecWorldPos, rad*0.5f, vTangentY, verts[0] );
	VectorMA( vecWorldPos, -rad*0.5f, vTangentY, verts[1] );
	VectorAdd( verts[0], vecDelta, verts[3] );
	VectorAdd( verts[1], vecDelta, verts[2] );
	Assert( verts[0].IsValid() && verts[1].IsValid() && verts[2].IsValid() && verts[3].IsValid() );

	meshBuilder.Position3fv( verts[0].Base() );
	meshBuilder.Color4ub( rc, gc, bc, ac );
	meshBuilder.TexCoord2f( 0, pSample0->m_fLeft_U0, pSample0->m_fBottom_V0 );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3fv( verts[1].Base() );
	meshBuilder.Color4ub( rc, gc, bc, ac );
	meshBuilder.TexCoord2f( 0, pSample0->m_fRight_U0, pSample0->m_fBottom_V0 );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3fv( verts[2].Base() );
	meshBuilder.Color4ub( rc, gc, bc, ac );
	meshBuilder.TexCoord2f( 0, pSample0->m_fRight_U0, pSample0->m_fTop_V0 );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3fv( verts[3].Base() );
	meshBuilder.Color4ub( rc, gc, bc, ac );
	meshBuilder.TexCoord2f( 0, pSample0->m_fLeft_U0, pSample0->m_fTop_V0 );
	meshBuilder.AdvanceVertex();

	meshBuilder.FastIndex( info.m_nVertexOffset );
	meshBuilder.FastIndex( info.m_nVertexOffset + 1 );
	meshBuilder.FastIndex( info.m_nVertexOffset + 2 );
	meshBuilder.FastIndex( info.m_nVertexOffset );
	meshBuilder.FastIndex( info.m_nVertexOffset + 2 );
	meshBuilder.FastIndex( info.m_nVertexOffset + 3 );
	info.m_nVertexOffset += 4;
}


void C_OP_RenderSpritesTrail::Render( IMatRenderContext *pRenderContext, CParticleCollection *pParticles, void *pContext ) const
{
	C_OP_RenderSpriteTrailContext_t *pCtx = reinterpret_cast<C_OP_RenderSpriteTrailContext_t *>( pContext );
	IMaterial *pMaterial = pParticles->m_pDef->GetMaterial();

	if ( pCtx->m_VisibilityData.m_bUseVisibility )
	{
		SetupParticleVisibility( pParticles, &pCtx->m_VisibilityData, &VisibilityInputs, &pCtx->m_nQueryHandle );
	}

	// Right now we only have a meshbuilder version!
	if ( !HushAsserts() )
		Assert( pMaterial->IsSpriteCard() == false );
	if ( pMaterial->IsSpriteCard() )
		return;
		 
	// Store matrices off so we can restore them in RenderEnd().
	pRenderContext->Bind( pMaterial );

	float flAgeScale = m_flAnimationRate * SEQUENCE_SAMPLE_COUNT;

	// Get the camera's worldspace position
	Vector vecCameraPos;
	pRenderContext->GetWorldSpaceCameraPosition( &vecCameraPos );

	SpriteTrailRenderInfo_t info;
	info.Init( pParticles, 0, flAgeScale, pParticles->m_Sheet() );

	int nParticles;
	const ParticleRenderData_t *pSortList = pParticles->GetRenderList( pRenderContext, true, &nParticles, &pCtx->m_VisibilityData );

	int nMaxParticlesInBatch = GetMaxParticlesPerBatch( pRenderContext, pMaterial, false );
	float flOODt = ( pParticles->m_flDt != 0.0f ) ? ( 1.0f / pParticles->m_flDt ) : 1.0f;
	while ( nParticles )
	{
		int nParticlesInBatch = min( nMaxParticlesInBatch, nParticles );
		nParticles -= nParticlesInBatch;

		g_pParticleSystemMgr->TallyParticlesRendered( nParticlesInBatch * 4 * 3, nParticlesInBatch * 6 * 3 );

		IMesh* pMesh = pRenderContext->GetDynamicMesh( true );
		CMeshBuilder meshBuilder;
		meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, nParticlesInBatch * 4, nParticlesInBatch * 6 );
		info.m_nVertexOffset = 0;
		for( int i = 0; i < nParticlesInBatch; i++ )
		{
			int hParticle = (--pSortList)->m_nIndex;
			RenderSpriteTrail( meshBuilder, info, hParticle, vecCameraPos, flOODt, pSortList );
		}
		meshBuilder.End();
		pMesh->Draw();
	}
}

void C_OP_RenderSpritesTrail::RenderUnsorted( CParticleCollection *pParticles, void *pContext, IMatRenderContext *pRenderContext, CMeshBuilder &meshBuilder, int nVertexOffset, int nFirstParticle, int nParticleCount ) const
{
	C_OP_RenderSpriteTrailContext_t *pCtx = reinterpret_cast<C_OP_RenderSpriteTrailContext_t *>( pContext );
	// NOTE: This is interesting to support because at first we won't have all the various
	// pixel-shader versions of SpriteCard, like modulate, twotexture, etc. etc.
	Vector vecCameraPos;
	pRenderContext->GetWorldSpaceCameraPosition( &vecCameraPos );

	float flAgeScale = m_flAnimationRate * SEQUENCE_SAMPLE_COUNT;
	SpriteTrailRenderInfo_t info;
	info.Init( pParticles, nVertexOffset, flAgeScale, pParticles->m_Sheet() );

	int nParticles;
	const ParticleRenderData_t *pSortList = pParticles->GetRenderList( pRenderContext, false, &nParticles, &pCtx->m_VisibilityData );

	float flOODt = ( pParticles->m_flDt != 0.0f ) ? ( 1.0f / pParticles->m_flDt ) : 1.0f;
	int hParticle = nFirstParticle;
	for( int i = 0; i < nParticleCount; i++, hParticle++ )
	{
		RenderSpriteTrail( meshBuilder, info, hParticle, vecCameraPos, flOODt, pSortList );
	}
}

//-----------------------------------------------------------------------------
//
// Rope renderer
//
//-----------------------------------------------------------------------------
struct RopeRenderInfo_t
{
	size_t m_nXYZStride;
	const fltx4 *m_pXYZ;
	size_t m_nRadStride;
	const fltx4 *m_pRadius;
	size_t m_nRGBStride;
	const fltx4 *m_pRGB;
	size_t m_nAlphaStride;
	const fltx4 *m_pAlpha;
	CParticleCollection *m_pParticles;

	void Init( CParticleCollection *pParticles )
	{
		m_pParticles = pParticles;
		m_pXYZ = pParticles->GetM128AttributePtr( PARTICLE_ATTRIBUTE_XYZ, &m_nXYZStride );
		m_pRadius = pParticles->GetM128AttributePtr( PARTICLE_ATTRIBUTE_RADIUS, &m_nRadStride );
		m_pRGB = pParticles->GetM128AttributePtr( PARTICLE_ATTRIBUTE_TINT_RGB, &m_nRGBStride );
		m_pAlpha = pParticles->GetM128AttributePtr( PARTICLE_ATTRIBUTE_ALPHA, &m_nAlphaStride );
	}

	void GenerateSeg( int hParticle, BeamSeg_t& seg )
	{
		Assert( hParticle != -1 );
		int nGroup = hParticle / 4;
		int nOffset = hParticle & 0x3;

		int nXYZIndex = nGroup * m_nXYZStride;
		int nColorIndex = nGroup * m_nRGBStride;
		seg.m_vPos.Init( SubFloat( m_pXYZ[ nXYZIndex ], nOffset ), SubFloat( m_pXYZ[ nXYZIndex+1 ], nOffset ), SubFloat( m_pXYZ[ nXYZIndex+2 ], nOffset ) );
		seg.m_vColor.Init( SubFloat( m_pRGB[ nColorIndex ], nOffset ), SubFloat( m_pRGB[ nColorIndex+1 ], nOffset ), SubFloat( m_pRGB[nColorIndex+2], nOffset ) );
		seg.m_flAlpha = SubFloat( m_pAlpha[ nGroup * m_nAlphaStride ], nOffset );
		seg.m_flWidth = SubFloat( m_pRadius[ nGroup * m_nRadStride ], nOffset );
	}
};


struct RenderRopeContext_t
{
	float m_flRenderedRopeLength;
};

class C_OP_RenderRope : public CParticleOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_OP_RenderRope );

	uint32 GetWrittenAttributes( void ) const
	{
		return 0;
	}

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_XYZ_MASK | PARTICLE_ATTRIBUTE_RADIUS_MASK | 
			PARTICLE_ATTRIBUTE_TINT_RGB_MASK | PARTICLE_ATTRIBUTE_ALPHA_MASK;
	}

	virtual void InitializeContextData( CParticleCollection *pParticles, void *pContext ) const
	{
		RenderRopeContext_t *pCtx = reinterpret_cast<RenderRopeContext_t *>( pContext );
		pCtx->m_flRenderedRopeLength = false;
		float *pSubdivList = (float*)( pCtx + 1 );
		for ( int iSubdiv = 0; iSubdiv < m_nSubdivCount; iSubdiv++ )
		{
			pSubdivList[iSubdiv] = (float)iSubdiv / (float)m_nSubdivCount;
		}

		// NOTE: Has to happen here, and not in InitParams, since the material isn't set up yet
		const_cast<C_OP_RenderRope*>( this )->m_flTextureScale = 1.0f / ( pParticles->m_pDef->GetMaterial()->GetMappingHeight() * m_flTexelSizeInUnits );
	}

	size_t GetRequiredContextBytes( void ) const
	{
		return sizeof( RenderRopeContext_t ) + m_nSubdivCount * sizeof(float);
	}

	virtual void InitParams( CParticleSystemDefinition *pDef, CDmxElement *pElement )
	{
		if ( m_nSubdivCount <= 0 )
		{
			m_nSubdivCount = 1;
		}
		if ( m_flTexelSizeInUnits <= 0 )
		{
			m_flTexelSizeInUnits = 1.0f;
		}
		m_flTStep = 1.0 / m_nSubdivCount;
	}

	virtual int GetParticlesToRender( CParticleCollection *pParticles, void *pContext, int nFirstParticle, int nRemainingVertices, int nRemainingIndices, int *pVertsUsed, int *pIndicesUsed ) const;
	virtual void Render( IMatRenderContext *pRenderContext, CParticleCollection *pParticles, void *pContext ) const;
	virtual void RenderSpriteCard( CParticleCollection *pParticles, void *pContext, IMaterial *pMaterial ) const;
	virtual void RenderUnsorted( CParticleCollection *pParticles, void *pContext, IMatRenderContext *pRenderContext, CMeshBuilder &meshBuilder, int nVertexOffset, int nFirstParticle, int nParticleCount ) const;

	// We connect neighboring particle instances to each other, so if the order isn't maintained we will have a particle that jumps
	// back to the wrong place and look terrible.
	virtual bool RequiresOrderInvariance( void ) const OVERRIDE
	{
		return true;
	}

	int		m_nSubdivCount;

	float	m_flTexelSizeInUnits;
	float	m_flTextureScale;
	float	m_flTextureScrollRate;
	float	m_flTStep;

};

DEFINE_PARTICLE_OPERATOR( C_OP_RenderRope, "render_rope", OPERATOR_SINGLETON );

BEGIN_PARTICLE_OPERATOR_UNPACK( C_OP_RenderRope ) 
	DMXELEMENT_UNPACK_FIELD( "subdivision_count", "3", int, m_nSubdivCount )
	DMXELEMENT_UNPACK_FIELD( "texel_size", "4.0f", float, m_flTexelSizeInUnits )
	DMXELEMENT_UNPACK_FIELD( "texture_scroll_rate", "0.0f", float, m_flTextureScrollRate )
END_PARTICLE_OPERATOR_UNPACK( C_OP_RenderRope )


//-----------------------------------------------------------------------------
// Returns the number of particles to render
//-----------------------------------------------------------------------------
int C_OP_RenderRope::GetParticlesToRender( CParticleCollection *pParticles, 
	void *pContext, int nFirstParticle, int nRemainingVertices, int nRemainingIndices, 
										   int *pVertsUsed, int *pIndicesUsed ) const
{
	if ( ( nFirstParticle >= pParticles->m_nActiveParticles - 1 ) || ( pParticles->m_nActiveParticles <= 1 ) )
	{
		*pVertsUsed = 0;
		*pIndicesUsed = 0;
		return 0;
	}

	// NOTE: This is only true for particles *after* the first particle.
	// First particle takes 2 verts, no indices.
	int nVertsPerParticle = 2 * m_nSubdivCount;
	int nIndicesPerParticle = 6 * m_nSubdivCount;

	// Subtract 2 is because the first particle uses an extra pair of vertices
	int nMaxParticleCount = 1 + ( nRemainingVertices - 2 ) / nVertsPerParticle;
	int nMaxParticleCount2 = nRemainingIndices / nIndicesPerParticle;
	if ( nMaxParticleCount > nMaxParticleCount2 )
	{
		nMaxParticleCount = nMaxParticleCount2;
	}

	int nParticleCount = pParticles->m_nActiveParticles - nFirstParticle;

	// We can't choose a max particle count so that we only have 1 particle to render next time
	if ( nMaxParticleCount == nParticleCount - 1 )
	{
		--nMaxParticleCount;
		Assert( nMaxParticleCount > 0 );
	}

	if ( nParticleCount > nMaxParticleCount )
	{
		nParticleCount = nMaxParticleCount;
	}

	*pVertsUsed = ( nParticleCount - 1 ) * m_nSubdivCount * 2 + 2;
	*pIndicesUsed = nParticleCount * m_nSubdivCount * 6;
	return nParticleCount;
}


#define OUTPUT_2SPLINE_VERTS( t ) 																								   \
			meshBuilder.Color4ub( FastFToC( vecColor.x ), FastFToC( vecColor.y), FastFToC( vecColor.z), FastFToC( vecColor.w ) );  \
			meshBuilder.Position3f( (t), flU, 0 );																				   \
			meshBuilder.TexCoord4fv( 0, vecP0.Base() );																			   \
			meshBuilder.TexCoord4fv( 1, vecP1.Base() );																			   \
			meshBuilder.TexCoord4fv( 2, vecP2.Base() );																			   \
			meshBuilder.TexCoord4fv( 3, vecP3.Base() );																			   \
            meshBuilder.AdvanceVertex();																						   \
			meshBuilder.Color4ub( FastFToC( vecColor.x ), FastFToC( vecColor.y), FastFToC( vecColor.z), FastFToC( vecColor.w ) );  \
			meshBuilder.Position3f( (t), flU, 1 );																				   \
			meshBuilder.TexCoord4fv( 0, vecP0.Base() );																			   \
			meshBuilder.TexCoord4fv( 1, vecP1.Base() );																			   \
			meshBuilder.TexCoord4fv( 2, vecP2.Base() );																			   \
			meshBuilder.TexCoord4fv( 3, vecP3.Base() );																			   \
			meshBuilder.AdvanceVertex();

void C_OP_RenderRope::RenderSpriteCard( CParticleCollection *pParticles, void *pContext, IMaterial *pMaterial ) const
{
	int nParticles = pParticles->m_nActiveParticles; 

	int nSegmentsToRender = nParticles - 1;
	if ( ! nSegmentsToRender )
		return;

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->Bind( pMaterial );

	int nMaxVertices = pRenderContext->GetMaxVerticesToRender( pMaterial );
	int nMaxIndices = pRenderContext->GetMaxIndicesToRender();

	int nNumIndicesPerSegment = 6 * m_nSubdivCount;
	int nNumVerticesPerSegment = 2 * m_nSubdivCount;

	int nNumSegmentsPerBatch = min( ( nMaxVertices - 2 )/nNumVerticesPerSegment,
									( nMaxIndices ) / nNumIndicesPerSegment );

	const float *pXYZ = pParticles->GetFloatAttributePtr( 
		PARTICLE_ATTRIBUTE_XYZ, 0 );
	const float *pColor = pParticles->GetFloatAttributePtr( 
		PARTICLE_ATTRIBUTE_TINT_RGB, 0 );

	const float *pRadius = pParticles->GetFloatAttributePtr( 
		PARTICLE_ATTRIBUTE_RADIUS, 0 );
	const float *pAlpha = pParticles->GetFloatAttributePtr( 
		PARTICLE_ATTRIBUTE_ALPHA, 0 );
	
	IMesh* pMesh = pRenderContext->GetDynamicMesh( true );
	CMeshBuilder meshBuilder;
	
	int nNumSegmentsIWillRenderPerBatch = min( nNumSegmentsPerBatch, nSegmentsToRender );
	
	bool bFirstPoint = true;

	float flTexOffset = m_flTextureScrollRate * pParticles->m_flCurTime;
	float flU = flTexOffset;

	// initialize first spline segment
	Vector4D vecP1( pXYZ[0], pXYZ[4], pXYZ[8], pRadius[0] );
	Vector4D vecP2( pXYZ[1], pXYZ[5], pXYZ[9], pRadius[1] );
	Vector4D vecP0 = vecP1;
	Vector4D vecColor( pColor[0], pColor[4], pColor[8], pAlpha[0] );
	Vector4D vecDelta = vecP2;
	vecDelta -= vecP1;
	vecP0 -= vecDelta;

	Vector4D vecP3;

	if ( nParticles < 3 )
	{
		vecP3 = vecP2;
		vecP3 += vecDelta;
	}
	else
	{
		vecP3.Init( pXYZ[2], pXYZ[6], pXYZ[10], pRadius[2] );
	}
	int nPnt = 3;
	int nCurIDX = 0;

	int nSegmentsAvailableInBuffer = nNumSegmentsIWillRenderPerBatch;

	g_pParticleSystemMgr->TallyParticlesRendered( 2 + nNumSegmentsIWillRenderPerBatch * nNumVerticesPerSegment * 3, nNumIndicesPerSegment * nNumSegmentsIWillRenderPerBatch * 3 );

	meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES,
					   2 + nNumSegmentsIWillRenderPerBatch * nNumVerticesPerSegment,
					   nNumIndicesPerSegment * nNumSegmentsIWillRenderPerBatch );
	


	float flDUScale = ( m_flTStep * m_flTexelSizeInUnits );
	float flT = 0;

	do
	{
		if ( ! nSegmentsAvailableInBuffer )
		{
			meshBuilder.End();
			pMesh->Draw();

			g_pParticleSystemMgr->TallyParticlesRendered( 2 + nNumSegmentsIWillRenderPerBatch * nNumVerticesPerSegment * 3, nNumIndicesPerSegment * nNumSegmentsIWillRenderPerBatch * 3 );


			meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, 2 + nNumSegmentsIWillRenderPerBatch * nNumVerticesPerSegment,
							   nNumIndicesPerSegment * nNumSegmentsIWillRenderPerBatch );

			// copy the last emitted points
			OUTPUT_2SPLINE_VERTS( flT );
			nSegmentsAvailableInBuffer = nNumSegmentsIWillRenderPerBatch;
			nCurIDX = 0;
		}
		nSegmentsAvailableInBuffer--;
		flT = 0.;
		float flDu = flDUScale * ( vecP2.AsVector3D() - vecP1.AsVector3D() ).Length();
		for( int nSlice = 0 ; nSlice < m_nSubdivCount; nSlice++ )
		{
			OUTPUT_2SPLINE_VERTS( flT );
			flT += m_flTStep;
			flU += flDu;
			if ( ! bFirstPoint )
			{
				meshBuilder.FastIndex( nCurIDX );
				meshBuilder.FastIndex( nCurIDX+1 );
				meshBuilder.FastIndex( nCurIDX+2 );
				meshBuilder.FastIndex( nCurIDX+1 );
				meshBuilder.FastIndex( nCurIDX+3 );
				meshBuilder.FastIndex( nCurIDX+2 );
				nCurIDX += 2;
			}
			bFirstPoint = false;
		}
		// next segment
		if ( nSegmentsToRender > 1 )
		{
			vecP0 = vecP1;
			vecP1 = vecP2;
			vecP2 = vecP3;
			pRadius = pParticles->GetFloatAttributePtr( 
				PARTICLE_ATTRIBUTE_RADIUS, nPnt );
			pAlpha = pParticles->GetFloatAttributePtr( 
				PARTICLE_ATTRIBUTE_ALPHA, nPnt -2  );
			vecColor.Init( pColor[0], pColor[4], pColor[8], pAlpha[0] );

			if ( nPnt < nParticles )
			{
				pXYZ = pParticles->GetFloatAttributePtr( 
					PARTICLE_ATTRIBUTE_XYZ, nPnt );
				vecP3.Init( pXYZ[0], pXYZ[4], pXYZ[8], pRadius[0] );
				nPnt++;
			}
			else
			{
				// fake last point by extrapolating
				vecP3 += vecP2;
				vecP3 -= vecP1;
			}
		}
	} while( --nSegmentsToRender );

	// output last piece
	OUTPUT_2SPLINE_VERTS( 1.0 );
	meshBuilder.FastIndex( nCurIDX );
	meshBuilder.FastIndex( nCurIDX+1 );
	meshBuilder.FastIndex( nCurIDX+2 );
	meshBuilder.FastIndex( nCurIDX+1 );
	meshBuilder.FastIndex( nCurIDX+3 );
	meshBuilder.FastIndex( nCurIDX+2 );


	meshBuilder.End();
	pMesh->Draw();
}

//-----------------------------------------------------------------------------
// Renders particles, sorts them (?)
//-----------------------------------------------------------------------------
void C_OP_RenderRope::Render( IMatRenderContext *pRenderContext, CParticleCollection *pParticles, void *pContext ) const
{
	// FIXME: What does this even mean?	Ropes can't really be sorted.

	IMaterial *pMaterial = pParticles->m_pDef->GetMaterial();
	if ( pMaterial->IsSpriteCard() )
	{
		RenderSpriteCard( pParticles, pContext, pMaterial );
		return;
	}

	pRenderContext->Bind( pMaterial );

	int nMaxVertices = pRenderContext->GetMaxVerticesToRender( pMaterial );
	int nMaxIndices = pRenderContext->GetMaxIndicesToRender();
	int nParticles = pParticles->m_nActiveParticles; 

	int nFirstParticle = 0;
	while ( nParticles )
	{
		int nVertCount, nIndexCount;
		int nParticlesInBatch = GetParticlesToRender( pParticles, pContext, nFirstParticle, nMaxVertices, nMaxIndices, &nVertCount, &nIndexCount );
		if ( nParticlesInBatch == 0 )
			break;

		nParticles -= nParticlesInBatch;

		g_pParticleSystemMgr->TallyParticlesRendered( nVertCount * 3, nIndexCount * 3 );

		IMesh* pMesh = pRenderContext->GetDynamicMesh( true );
		CMeshBuilder meshBuilder;
		meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, nVertCount, nIndexCount );

		RenderUnsorted( pParticles, pContext, pRenderContext, meshBuilder, 0, nFirstParticle, nParticlesInBatch );

		meshBuilder.End();
		pMesh->Draw();

		nFirstParticle += nParticlesInBatch;
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void C_OP_RenderRope::RenderUnsorted( CParticleCollection *pParticles, void *pContext, IMatRenderContext *pRenderContext, CMeshBuilder &meshBuilder, int nVertexOffset, int nFirstParticle, int nParticleCount ) const
{
	IMaterial *pMaterial = pParticles->m_pDef->GetMaterial();

	// Right now we only have a meshbuilder version!
	Assert( pMaterial->IsSpriteCard() == false );
	if ( pMaterial->IsSpriteCard() )
		return;

	RenderRopeContext_t *pCtx = reinterpret_cast<RenderRopeContext_t *>( pContext );
	float *pSubdivList = (float*)( pCtx + 1 );
	if ( nFirstParticle == 0 )
	{
		pCtx->m_flRenderedRopeLength = 0.0f;
	}

	float flTexOffset = m_flTextureScrollRate * pParticles->m_flCurTime;

	RopeRenderInfo_t info;
	info.Init( pParticles );
	
	CBeamSegDraw beamSegment;
	beamSegment.Start( pRenderContext, ( nParticleCount - 1 ) * m_nSubdivCount + 1, pMaterial, &meshBuilder, nVertexOffset );

	Vector vecCatmullRom[4];
	BeamSeg_t seg[2];
	info.GenerateSeg( nFirstParticle, seg[0] );
	seg[0].m_flTexCoord = ( pCtx->m_flRenderedRopeLength + flTexOffset ) * m_flTextureScale;

	beamSegment.NextSeg( &seg[0] );
	vecCatmullRom[1] = seg[0].m_vPos;
	if ( nFirstParticle == 0 )
	{
		vecCatmullRom[0] = vecCatmullRom[1];
	}
	else
	{
		int nGroup = ( nFirstParticle-1 ) / 4;
		int nOffset = ( nFirstParticle-1 ) & 0x3;
		int nXYZIndex = nGroup * info.m_nXYZStride;
		vecCatmullRom[0].Init( SubFloat( info.m_pXYZ[ nXYZIndex ], nOffset ), SubFloat( info.m_pXYZ[ nXYZIndex+1 ], nOffset ), SubFloat( info.m_pXYZ[ nXYZIndex+2 ], nOffset ) );
	}

	float flOOSubDivCount = 1.0f / m_nSubdivCount;
	int hParticle = nFirstParticle + 1;
	for ( int i = 1; i < nParticleCount; ++i, ++hParticle )
	{
		int nCurr = i & 1;
		int nPrev = 1 - nCurr;
		info.GenerateSeg( hParticle, seg[nCurr] );
		pCtx->m_flRenderedRopeLength += seg[nCurr].m_vPos.DistTo( seg[nPrev].m_vPos );
		seg[nCurr].m_flTexCoord = ( pCtx->m_flRenderedRopeLength + flTexOffset ) * m_flTextureScale;

		if ( m_nSubdivCount > 1 )
		{
			vecCatmullRom[ (i+1) & 0x3 ] = seg[nCurr].m_vPos;
			if ( hParticle != info.m_pParticles->m_nActiveParticles - 1 )
			{
				int nGroup = ( hParticle+1 ) / 4;
				int nOffset = ( hParticle+1 ) & 0x3;
				int nXYZIndex = nGroup * info.m_nXYZStride;
				vecCatmullRom[ (i+2) & 0x3 ].Init( SubFloat( info.m_pXYZ[ nXYZIndex ], nOffset ), SubFloat( info.m_pXYZ[ nXYZIndex+1 ], nOffset ), SubFloat( info.m_pXYZ[ nXYZIndex+2 ], nOffset ) );
			}
			else
			{
				vecCatmullRom[ (i+2) & 0x3 ] = vecCatmullRom[ (i+1) & 0x3 ];
			}

			BeamSeg_t &subDivSeg = seg[nPrev];
			Vector vecColorInc = ( seg[nCurr].m_vColor - seg[nPrev].m_vColor ) * flOOSubDivCount;
			float flAlphaInc = ( seg[nCurr].m_flAlpha - seg[nPrev].m_flAlpha ) * flOOSubDivCount;
			float flTexcoordInc = ( seg[nCurr].m_flTexCoord - seg[nPrev].m_flTexCoord ) * flOOSubDivCount;
			float flWidthInc = ( seg[nCurr].m_flWidth - seg[nPrev].m_flWidth ) * flOOSubDivCount;
			for( int iSubdiv = 1; iSubdiv < m_nSubdivCount; ++iSubdiv )
			{
				subDivSeg.m_vColor += vecColorInc;
				subDivSeg.m_vColor.x = clamp( subDivSeg.m_vColor.x, 0.0f, 1.0f );
				subDivSeg.m_vColor.y = clamp( subDivSeg.m_vColor.y, 0.0f, 1.0f );
				subDivSeg.m_vColor.z = clamp( subDivSeg.m_vColor.z, 0.0f, 1.0f );
				subDivSeg.m_flAlpha += flAlphaInc;
				subDivSeg.m_flAlpha = clamp( subDivSeg.m_flAlpha, 0.0f, 1.0f );
				subDivSeg.m_flTexCoord += flTexcoordInc;
				subDivSeg.m_flWidth += flWidthInc;

				Catmull_Rom_Spline( vecCatmullRom[ (i+3) & 0x3 ], vecCatmullRom[ i & 0x3 ],
					vecCatmullRom[ (i+1) & 0x3 ], vecCatmullRom[ (i+2) & 0x3 ],
					pSubdivList[iSubdiv], subDivSeg.m_vPos );

				beamSegment.NextSeg( &subDivSeg );
			}
		}

		beamSegment.NextSeg( &seg[nCurr] );
	}

	beamSegment.End();
}

#ifdef USE_BLOBULATOR										// Enable blobulator for EP3

//-----------------------------------------------------------------------------
// Installs renderers
//-----------------------------------------------------------------------------
class C_OP_RenderBlobs : public CParticleRenderOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_OP_RenderBlobs );

	float m_cubeWidth;
	float m_cutoffRadius;
	float m_renderRadius;


	struct C_OP_RenderBlobsContext_t
	{
		CParticleVisibilityData m_VisibilityData;
		int		m_nQueryHandle;
	};

	size_t GetRequiredContextBytes( void ) const
	{
		return sizeof( C_OP_RenderBlobsContext_t );
	}

	virtual void InitializeContextData( CParticleCollection *pParticles, void *pContext ) const
	{
		C_OP_RenderBlobsContext_t *pCtx = reinterpret_cast<C_OP_RenderBlobsContext_t *>( pContext );
		if ( VisibilityInputs.m_nCPin >= 0 )
			pCtx->m_VisibilityData.m_bUseVisibility = true;
		else
			pCtx->m_VisibilityData.m_bUseVisibility = false;
		pCtx->m_VisibilityData.m_flCameraBias = VisibilityInputs.m_flCameraBias;
	}

	uint32 GetWrittenAttributes( void ) const
	{
		return 0;
	}

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_XYZ_MASK;
	}

	virtual void Render( IMatRenderContext *pRenderContext, CParticleCollection *pParticles, void *pContext ) const;
	
	virtual bool IsBatchable() const
	{
		return false;
	}
};

DEFINE_PARTICLE_OPERATOR( C_OP_RenderBlobs, "render_blobs", OPERATOR_SINGLETON );

BEGIN_PARTICLE_RENDER_OPERATOR_UNPACK( C_OP_RenderBlobs ) 
	DMXELEMENT_UNPACK_FIELD( "cube_width", "1.0f", float, m_cubeWidth )
	DMXELEMENT_UNPACK_FIELD( "cutoff_radius", "3.3f", float, m_cutoffRadius )
	DMXELEMENT_UNPACK_FIELD( "render_radius", "1.3f", float, m_renderRadius )
END_PARTICLE_OPERATOR_UNPACK( C_OP_RenderBlobs )


void C_OP_RenderBlobs::Render( IMatRenderContext *pRenderContext, CParticleCollection *pParticles, void *pContext ) const
{
	ImpTiler* tiler = ImpTilerFactory::factory->getTiler();
	//RENDERER_CLASS* sweepRenderer = tiler->getRenderer();

	C_OP_RenderBlobsContext_t *pCtx = reinterpret_cast<C_OP_RenderBlobsContext_t *>( pContext );

	if ( pCtx->m_VisibilityData.m_bUseVisibility )
	{
		SetupParticleVisibility( pParticles, &pCtx->m_VisibilityData, &VisibilityInputs, &pCtx->m_nQueryHandle );
	}



	#if 0
		// Note: it is not good to have these static variables here.
		static RENDERER_CLASS* sweepRenderer = NULL;
		static ImpTiler* tiler = NULL;
		if(!sweepRenderer)
		{
		sweepRenderer = new RENDERER_CLASS();
		tiler = new ImpTiler(sweepRenderer);
		}
	#endif

	// TODO: I should get rid of this static array and static calls
	//		 to setCubeWidth, etc...
	static SmartArray<ImpParticle> imp_particles_sa;  // This doesn't specify alignment, might have problems with SSE

	RENDERER_CLASS::setCubeWidth(m_cubeWidth);
	RENDERER_CLASS::setRenderR(m_renderRadius);
	RENDERER_CLASS::setCutoffR(m_cutoffRadius);

	RENDERER_CLASS::setCalcSignFunc(calcSign);
	RENDERER_CLASS::setCalcSign2Func(calcSign2);
	#if 0
		RENDERER_CLASS::setCalcCornerFunc(CALC_CORNER_NORMAL_COLOR_CI_SIZE, calcCornerNormalColor);
		RENDERER_CLASS::setCalcVertexFunc(calcVertexNormalNColor);
	#endif
	#if 1
		RENDERER_CLASS::setCalcCornerFunc(CALC_CORNER_NORMAL_CI_SIZE, calcCornerNormal);
		RENDERER_CLASS::setCalcVertexFunc(calcVertexNormalDebugColor);
	#endif
	#if 0
		RENDERER_CLASS::setCalcCornerFunc(CALC_CORNER_NORMAL_COLOR_CI_SIZE, calcCornerNormalHiFreqColor);
		RENDERER_CLASS::setCalcVertexFunc(calcVertexNormalNColor);
	#endif


	IMaterial *pMaterial = pParticles->m_pDef->GetMaterial();

	// TODO: I don't need to load this as a sorted list. See Lennard Jones forces for better way!
	int nParticles;
	const ParticleRenderData_t *pSortList = pParticles->GetRenderList( pRenderContext, false, &nParticles, &pCtx->m_VisibilityData );
	size_t xyz_stride;
	const fltx4 *xyz = pParticles->GetM128AttributePtr( PARTICLE_ATTRIBUTE_XYZ, &xyz_stride );

	Vector bbMin;
	Vector bbMax;
	pParticles->GetBounds( &bbMin, &bbMax );
	Vector bbCenter = 0.5f * ( bbMin + bbMax );

	// FIXME: Make this configurable. Not all shaders perform lighting. Although it's pretty likely for isosurface shaders.
	g_pParticleSystemMgr->Query()->SetUpLightingEnvironment( bbCenter );

	// FIXME: Ugly hack to get particle system location to a special blob shader lighting proxy
	pRenderContext->Bind( pMaterial, &bbCenter );

	//CMeshBuilder meshBuilder;
	//int nMaxVertices = pRenderContext->GetMaxVerticesToRender( pMaterial );

	tiler->beginFrame(Point3D(0.0f, 0.0f, 0.0f), (void*)&pRenderContext);

	while(imp_particles_sa.size < nParticles)
	{
		imp_particles_sa.pushAutoSize(ImpParticle());
	}

	for( int i = 0; i < nParticles; i++ )
	{
		int hParticle = (--pSortList)->m_nIndex;
		int nIndex = ( hParticle / 4 ) * xyz_stride;
		int nOffset = hParticle & 0x3;
		float x = SubFloat( xyz[nIndex], nOffset );
		float y = SubFloat( xyz[nIndex+1], nOffset );
		float z = SubFloat( xyz[nIndex+2], nOffset );

		ImpParticle* imp_particle = &imp_particles_sa[i];
		imp_particle->center[0]=x;
		imp_particle->center[1]=y;
		imp_particle->center[2]=z;
		imp_particle->setFieldScale(1.0f);
		//imp_particle->interpolants1.set(1.0f, 1.0f, 1.0f);
		//imp_particle->interpolants1[3] = 0.0f; //m_flSurfaceV[i];
		tiler->insertParticle(imp_particle);
	}


	tiler->drawSurface(); // NOTE: need to call drawSurfaceSorted for transparency
	tiler->endFrame();

	ImpTilerFactory::factory->returnTiler(tiler);

}

#endif //blobs



//-----------------------------------------------------------------------------
// Installs renderers
//-----------------------------------------------------------------------------
class C_OP_RenderScreenVelocityRotate : public CParticleRenderOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_OP_RenderScreenVelocityRotate );

	float m_flRotateRateDegrees;
	float m_flForwardDegrees;

	struct C_OP_RenderScreenVelocityRotateContext_t
	{
		CParticleVisibilityData m_VisibilityData;
		int		m_nQueryHandle;
	};

	size_t GetRequiredContextBytes( void ) const
	{
		return sizeof( C_OP_RenderScreenVelocityRotateContext_t );
	}

	virtual void InitializeContextData( CParticleCollection *pParticles, void *pContext ) const
	{
		C_OP_RenderScreenVelocityRotateContext_t *pCtx = reinterpret_cast<C_OP_RenderScreenVelocityRotateContext_t *>( pContext );
		if ( VisibilityInputs.m_nCPin >= 0 )
			pCtx->m_VisibilityData.m_bUseVisibility = true;
		else
			pCtx->m_VisibilityData.m_bUseVisibility = false;
		pCtx->m_VisibilityData.m_flCameraBias = VisibilityInputs.m_flCameraBias;
	}
	uint32 GetWrittenAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_ROTATION_MASK;
	}

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_XYZ_MASK | PARTICLE_ATTRIBUTE_PREV_XYZ_MASK | PARTICLE_ATTRIBUTE_ROTATION_MASK ;
	}

	virtual void Render( IMatRenderContext *pRenderContext, CParticleCollection *pParticles, void *pContext ) const;
};

DEFINE_PARTICLE_OPERATOR( C_OP_RenderScreenVelocityRotate, "render_screen_velocity_rotate", OPERATOR_SINGLETON );

BEGIN_PARTICLE_RENDER_OPERATOR_UNPACK( C_OP_RenderScreenVelocityRotate ) 
	DMXELEMENT_UNPACK_FIELD( "rotate_rate(dps)", "0.0f", float, m_flRotateRateDegrees )
	DMXELEMENT_UNPACK_FIELD( "forward_angle", "-90.0f", float, m_flForwardDegrees )
END_PARTICLE_OPERATOR_UNPACK( C_OP_RenderScreenVelocityRotate )


void C_OP_RenderScreenVelocityRotate::Render( IMatRenderContext *pRenderContext, CParticleCollection *pParticles, void *pContext ) const
{
	C_OP_RenderScreenVelocityRotateContext_t *pCtx = reinterpret_cast<C_OP_RenderScreenVelocityRotateContext_t *>( pContext );
	if ( pCtx->m_VisibilityData.m_bUseVisibility )
	{
		SetupParticleVisibility( pParticles, &pCtx->m_VisibilityData, &VisibilityInputs, &pCtx->m_nQueryHandle );
	}



	// NOTE: This is interesting to support because at first we won't have all the various
	// pixel-shader versions of SpriteCard, like modulate, twotexture, etc. etc.
	VMatrix tempView;

	// Store matrices off so we can restore them in RenderEnd().
	pRenderContext->GetMatrix(MATERIAL_VIEW, &tempView);

	int nParticles;
	const ParticleRenderData_t *pSortList = pParticles->GetRenderList( pRenderContext, false, &nParticles, &pCtx->m_VisibilityData );

	size_t xyz_stride;
	const fltx4 *xyz = pParticles->GetM128AttributePtr( PARTICLE_ATTRIBUTE_XYZ, &xyz_stride );

	size_t prev_xyz_stride;
	const fltx4 *prev_xyz = pParticles->GetM128AttributePtr( PARTICLE_ATTRIBUTE_PREV_XYZ, &prev_xyz_stride );

	size_t rot_stride;
	// const fltx4 *pRot = pParticles->GetM128AttributePtr( PARTICLE_ATTRIBUTE_ROTATION, &rot_stride );
	fltx4 *pRot = pParticles->GetM128AttributePtrForWrite( PARTICLE_ATTRIBUTE_ROTATION, &rot_stride );

	float flForwardRadians = m_flForwardDegrees * ( M_PI / 180.0f );
	//float flRotateRateRadians = m_flRotateRateDegrees * ( M_PI / 180.0f );

	for( int i = 0; i < nParticles; i++ )
	{
		int hParticle = (--pSortList)->m_nIndex;
		int nGroup = ( hParticle / 4 );
		int nOffset = hParticle & 0x3;

		int nXYZIndex = nGroup * xyz_stride;
		Vector vecWorldPos( SubFloat( xyz[ nXYZIndex ], nOffset ), SubFloat( xyz[ nXYZIndex+1 ], nOffset ), SubFloat( xyz[ nXYZIndex+2 ], nOffset ) );
		Vector vecViewPos;
		Vector3DMultiplyPosition( tempView, vecWorldPos, vecViewPos );

		if (!IsFinite(vecViewPos.x))
			continue;

		int nPrevXYZIndex = nGroup * prev_xyz_stride;
		Vector vecPrevWorldPos( SubFloat( prev_xyz[ nPrevXYZIndex ], nOffset ), SubFloat( prev_xyz[ nPrevXYZIndex+1 ], nOffset ), SubFloat( prev_xyz[ nPrevXYZIndex+2 ], nOffset ) );
		Vector vecPrevViewPos;
		Vector3DMultiplyPosition( tempView, vecPrevWorldPos, vecPrevViewPos );

		float rot = atan2( vecViewPos.y - vecPrevViewPos.y, vecViewPos.x - vecPrevViewPos.x ) + flForwardRadians;
		SubFloat( pRot[ nGroup * rot_stride ], nOffset ) = rot;
	}
}




//-----------------------------------------------------------------------------
// Installs renderers
//-----------------------------------------------------------------------------
void AddBuiltInParticleRenderers( void )
{
#ifdef _DEBUG
	REGISTER_PARTICLE_OPERATOR( FUNCTION_RENDERER, C_OP_RenderPoints );
#endif
	REGISTER_PARTICLE_OPERATOR( FUNCTION_RENDERER, C_OP_RenderSprites );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_RENDERER, C_OP_RenderSpritesTrail );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_RENDERER, C_OP_RenderRope );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_RENDERER, C_OP_RenderScreenVelocityRotate );
#ifdef USE_BLOBULATOR
	REGISTER_PARTICLE_OPERATOR( FUNCTION_RENDERER, C_OP_RenderBlobs );
#endif // blobs
}




