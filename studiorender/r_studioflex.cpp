//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//===========================================================================//

#include "studio.h"
#include "studiorendercontext.h"
#include "bitmap/imageformat.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/itexture.h"
#include "materialsystem/imesh.h"
#include "mathlib/mathlib.h"
#include "studiorender.h"
#include "pixelwriter.h"
#include "vtf/vtf.h"
#include "tier1/convar.h"
#include "tier1/KeyValues.h"
#include "tier0/vprof.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define sign( a ) (((a) < 0) ? -1 : (((a) > 0) ? 1 : 0 ))

void CStudioRender::R_StudioEyeballPosition( const mstudioeyeball_t *peyeball, eyeballstate_t *pstate )
{
	// Vector  forward;
	// Vector  org, right, up;

	pstate->peyeball = peyeball;

	Vector tmp;
	// move eyeball into worldspace
	{
		// ConDMsg("%.2f %.2f %.2f\n", peyeball->org[0], peyeball->org[1], peyeball->org[2] );

		VectorCopy( peyeball->org, tmp );

		tmp[0] += m_pRC->m_Config.fEyeShiftX * sign( tmp[0] );
		tmp[1] += m_pRC->m_Config.fEyeShiftY * sign( tmp[1] );
		tmp[2] += m_pRC->m_Config.fEyeShiftZ * sign( tmp[2] );
	}
	VectorTransform( tmp, m_pBoneToWorld[peyeball->bone], pstate->org );
	VectorRotate( peyeball->up, m_pBoneToWorld[peyeball->bone], pstate->up );

	// look directly at target
	VectorSubtract( m_pRC->m_ViewTarget, pstate->org, pstate->forward );
	VectorNormalize( pstate->forward );

	if ( !m_pRC->m_Config.bEyeMove )
	{
		VectorRotate( peyeball->forward, m_pBoneToWorld[peyeball->bone], pstate->forward );
		VectorScale( pstate->forward, -1 ,pstate->forward ); // ???
	}

	CrossProduct( pstate->forward, pstate->up, pstate->right );
	VectorNormalize( pstate->right );

	// shift N degrees off of the target
	float dz;
	dz = peyeball->zoffset;

	VectorMA( pstate->forward, peyeball->zoffset + dz, pstate->right, pstate->forward );

#if 0
	// add random jitter
	VectorMA( forward, RandomFloat( -0.02, 0.02 ), right, forward );
	VectorMA( forward, RandomFloat( -0.02, 0.02 ), up, forward );
#endif

	VectorNormalize( pstate->forward );
	// re-aim eyes 
	CrossProduct( pstate->forward, pstate->up, pstate->right );
	VectorNormalize( pstate->right );

	CrossProduct( pstate->right, pstate->forward, pstate->up );
	VectorNormalize( pstate->up );

	float scale = (1.0 / peyeball->iris_scale) + m_pRC->m_Config.fEyeSize;

	if (scale > 0)
		scale = 1.0 / scale;

	VectorScale( &pstate->right[0], -scale, pstate->mat[0] );
	VectorScale( &pstate->up[0], -scale, pstate->mat[1] );

	pstate->mat[0][3] = -DotProduct( &pstate->org[0], pstate->mat[0] ) + 0.5f;
	pstate->mat[1][3] = -DotProduct( &pstate->org[0], pstate->mat[1] ) + 0.5f;

	// FIXME: push out vertices for cornea
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CStudioRender::R_StudioEyelidFACS( const mstudioeyeball_t *peyeball, const eyeballstate_t *pstate )
{
	if ( peyeball->m_bNonFACS )
		return;

	Vector  headup;
	Vector  headforward;
	Vector	pos;

	float upperlid = DEG2RAD( 9.5 );
	float lowerlid = DEG2RAD( -26.4 );

	// FIXME: Crash workaround
	Vector vecNormTarget;
	vecNormTarget.Init( peyeball->uppertarget[0], peyeball->uppertarget[1], peyeball->uppertarget[2] );
	vecNormTarget /= peyeball->radius;
	vecNormTarget.x = clamp( vecNormTarget.x, -1.0f, 1.0f );
	vecNormTarget.y = clamp( vecNormTarget.y, -1.0f, 1.0f );
	vecNormTarget.z = clamp( vecNormTarget.z, -1.0f, 1.0f );

	// get weighted position of eyeball angles based on the "raiser", "neutral", and "lowerer" controls
	upperlid = m_pFlexWeights[peyeball->upperflexdesc[0]] * asin( vecNormTarget.x );
	upperlid += m_pFlexWeights[peyeball->upperflexdesc[1]] * asin( vecNormTarget.y );
	upperlid += m_pFlexWeights[peyeball->upperflexdesc[2]] * asin( vecNormTarget.z );

	vecNormTarget.Init( peyeball->lowertarget[0], peyeball->lowertarget[1], peyeball->lowertarget[2] );
	vecNormTarget /= peyeball->radius;
	vecNormTarget.x = clamp( vecNormTarget.x, -1.0f, 1.0f );
	vecNormTarget.y = clamp( vecNormTarget.y, -1.0f, 1.0f );
	vecNormTarget.z = clamp( vecNormTarget.z, -1.0f, 1.0f );

	lowerlid = m_pFlexWeights[peyeball->lowerflexdesc[0]] * asin( vecNormTarget.x );
	lowerlid += m_pFlexWeights[peyeball->lowerflexdesc[1]] * asin( vecNormTarget.y );
	lowerlid += m_pFlexWeights[peyeball->lowerflexdesc[2]] * asin( vecNormTarget.z );

	// ConDMsg("%.1f %.1f\n", RAD2DEG( upperlid ), RAD2DEG( lowerlid ) );		

	float sinupper, cosupper, sinlower, coslower;
	SinCos( upperlid, &sinupper, &cosupper );
	SinCos( lowerlid, &sinlower, &coslower );

	// convert to head relative space
	VectorIRotate( pstate->up, m_pBoneToWorld[peyeball->bone], headup );
	VectorIRotate( pstate->forward, m_pBoneToWorld[peyeball->bone], headforward );

	// upper lid
	VectorScale( headup, sinupper * peyeball->radius, pos );
	VectorMA( pos, cosupper * peyeball->radius, headforward, pos );
	m_pFlexWeights[peyeball->upperlidflexdesc] = DotProduct( pos, peyeball->up );

	// lower lid
	VectorScale( headup, sinlower * peyeball->radius, pos );
	VectorMA( pos, coslower * peyeball->radius, headforward, pos );
	m_pFlexWeights[peyeball->lowerlidflexdesc] = DotProduct( pos, peyeball->up );
	// ConDMsg("%.4f %.4f\n", m_pRC->m_FlexWeights[peyeball->upperlidflex], m_pRC->m_FlexWeights[peyeball->lowerlidflex] );
}


void CStudioRender::MaterialPlanerProjection( const matrix3x4_t& mat, int count, const Vector *psrcverts, Vector2D *pdesttexcoords )
{
	for (int i = 0; i < count; i++)
	{
		pdesttexcoords[i][0] = DotProduct( &psrcverts[i].x, mat[0] ) + mat[0][3];
		pdesttexcoords[i][1] = DotProduct( &psrcverts[i].x, mat[1] ) + mat[1][3];
	}
}


//-----------------------------------------------------------------------------
// Ramp and clamp the flex weight
//-----------------------------------------------------------------------------
float CStudioRender::RampFlexWeight( mstudioflex_t &flex, float w )
{
	if (w <= flex.target0 || w >= flex.target3)
	{
		// value outside of range
		w = 0.0;
	}
	else if (w < flex.target1)
	{
		// 0 to 1 ramp
		w = (w - flex.target0) / (flex.target1 - flex.target0);
	}
	else if (w > flex.target2)
	{
		// 1 to 0 ramp
		w = (flex.target3 - w) / (flex.target3 - flex.target2);
	}
	else
	{
		// plat
		w = 1.0;
	}
	return w;
}

//-----------------------------------------------------------------------------
// Setup the flex verts for this rendering
//-----------------------------------------------------------------------------
void CStudioRender::R_StudioFlexVerts( mstudiomesh_t *pmesh, int lod )
{
	VPROF_BUDGET( "CStudioRender::R_StudioFlexVerts", VPROF_BUDGETGROUP_MODEL_RENDERING );

	Assert( pmesh );

	const float flVertAnimFixedPointScale = m_pStudioHdr->VertAnimFixedPointScale();

	// There's a chance we can actually do the flex twice on a single mesh
	// since there's flexed HW + SW portions of the mesh.
	if (m_VertexCache.IsFlexComputationDone())
		return;

	// get pointers to geometry
	if ( !pmesh->pModel()->CacheVertexData( m_pStudioHdr ) )
	{
		// not available yet
		return;
	}
	const mstudio_meshvertexdata_t *vertData = pmesh->GetVertexData( m_pStudioHdr );
	Assert( vertData );
	if ( !vertData )
	{
		static unsigned int warnCount = 0;
		if ( warnCount++ < 20 )
			Warning( "ERROR: R_StudioFlexVerts, model verts have been compressed, cannot render! (use \"-no_compressed_vvds\")" );
		return;
	}

	// The flex data should have been converted to the new (fixed-point) format on load:
	Assert( m_pStudioHdr->flags & STUDIOHDR_FLAGS_FLEXES_CONVERTED );
	if ( ( m_pStudioHdr->flags & STUDIOHDR_FLAGS_FLEXES_CONVERTED ) == 0 )
	{
		static unsigned int flexConversionTimesWarned = 0;
		if ( flexConversionTimesWarned++ < 6 )
			Warning( "ERROR: flex verts have not been converted (queued loader refcount bug?) - expect to see 'exploded' faces" );
	}


	mstudiovertex_t *pVertices = vertData->Vertex( 0 );
	Vector4D *pStudioTangentS;
	if ( vertData->HasTangentData() )
	{
		pStudioTangentS	= vertData->TangentS( 0 );
	}
	else
	{
		pStudioTangentS = NULL;
	}

	mstudioflex_t *pflex = pmesh->pFlex( 0 );
	
	m_VertexCache.SetupComputation( pmesh, true );

	// apply flex weights
	int i, j, n;

	for (i = 0; i < pmesh->numflexes; i++)
	{
		float w1 = RampFlexWeight( pflex[i], m_pFlexWeights[ pflex[i].flexdesc ] );
		float w2 = RampFlexWeight( pflex[i], m_pFlexDelayedWeights[ pflex[i].flexdesc ] );

		float w3, w4;
		if ( pflex[i].flexpair != 0)
		{
			w3 = RampFlexWeight( pflex[i], m_pFlexWeights[ pflex[i].flexpair ] );
			w4 = RampFlexWeight( pflex[i], m_pFlexDelayedWeights[ pflex[i].flexpair ] );
		}
		else
		{
			w3 = w1;
			w4 = w2;
		}

		if ( w1 > -0.001 && w1 < 0.001 && w2 > -0.001 && w2 < 0.001 )
		{
			if ( w3 > -0.001 && w3 < 0.001 && w4 > -0.001 && w4 < 0.001 )
			{
				continue;
			}
		}

		// We may have wrinkle information for this flex, but if we're software skinning
		// we're going to ignore it.
		byte *pvanim = pflex[i].pBaseVertanim();
		int nVAnimSizeBytes = pflex[i].VertAnimSizeBytes();

		for (j = 0; j < pflex[i].numverts; j++)
		{
			mstudiovertanim_t *pAnim = (mstudiovertanim_t*)( pvanim + j * nVAnimSizeBytes );
			n = pAnim->index;

			// Only flex the indices that are (still) part of this mesh
			// need lod restriction here
			if (n < pmesh->vertexdata.numLODVertexes[lod])
			{
				mstudiovertex_t &vert = pVertices[n];

				CachedPosNormTan_t* pFlexedVertex;
				if (!m_VertexCache.IsVertexFlexed(n))
				{
					// Add a new flexed vert to the flexed vertex list
					pFlexedVertex = m_VertexCache.CreateFlexVertex(n);
					// skip processing if no more flexed verts can be allocated
					if (pFlexedVertex == NULL)
						continue;

					VectorCopy( vert.m_vecPosition, pFlexedVertex->m_Position );
					VectorCopy( vert.m_vecNormal, pFlexedVertex->m_Normal );

					if (pStudioTangentS)
					{
						Vector4DCopy( pStudioTangentS[n], pFlexedVertex->m_TangentS );
						Assert( pFlexedVertex->m_TangentS.w == -1.0f || pFlexedVertex->m_TangentS.w == 1.0f );
					}
				}
				else
				{
					pFlexedVertex = m_VertexCache.GetFlexVertex(n);
				}

				float s = pAnim->speed * (1.0F/255.0F);
				float b = pAnim->side * (1.0F/255.0F);

				float w = (w1 * s + (1.0f - s) * w2) * (1.0f - b) + b * (w3 * s + (1.0f - s) * w4);

				// Accumulate weighted deltas
				pFlexedVertex->m_Position += pAnim->GetDeltaFixed( flVertAnimFixedPointScale ) * w;
				pFlexedVertex->m_Normal += pAnim->GetNDeltaFixed( flVertAnimFixedPointScale ) * w;

				if ( pStudioTangentS )
				{
					pFlexedVertex->m_TangentS.AsVector3D() += pAnim->GetNDeltaFixed( flVertAnimFixedPointScale ) * w;
					Assert( pFlexedVertex->m_TangentS.w == -1.0f || pFlexedVertex->m_TangentS.w == 1.0f );
				}
			}
		}
	}

	m_VertexCache.RenormalizeFlexVertices( vertData->HasTangentData() );
}

// REMOVED!!  Look in version 32 if you need it.
//static void R_StudioEyeballNormals( const mstudioeyeball_t *peyeball, int count, const Vector *psrcverts, Vector *pdestnorms )

#define KERNEL_DIAMETER 2
#define KERNEL_TEXELS (KERNEL_DIAMETER)
#define KERNEL_TEXEL_RADIUS (KERNEL_TEXELS / 2)

inline float GlintGaussSpotCoefficient( float dx, float dy /*, float *table */ )
{
	const float radius = KERNEL_DIAMETER / 2;
	const float rsq = 1.0f / (radius * radius);
	float r2 = (dx * dx + dy * dy) * rsq;
	if (r2 <= 1.0f)
	{
		return exp( -25.0 * r2 );
		// NOTE: This optimization doesn't make much of a difference
		//int index = r2 * (GLINT_TABLE_ENTRIES-1);
		//return table[index];
	}

	return 0;
}

void CStudioRender::AddGlint( CPixelWriter &pixelWriter, float x, float y, const Vector& color )
{
	x = (x + 0.5f) * m_GlintWidth;
	y = (y + 0.5f) * m_GlintHeight;
	const float texelRadius = KERNEL_DIAMETER / 2;

	int x0 = (int)x;
	int y0 = (int)y;
	int x1 = x0 + texelRadius;
	int y1 = y0 + texelRadius;
	x0 -= texelRadius;
	y0 -= texelRadius;

	// clip light to texture
	if ( (x0 >= m_GlintWidth) || (x1 < 0) || (y0 >= m_GlintHeight) || (y1 < 0) )
		return;

	// clamp coordinates
	if ( x0 < 0 )
	{
		x0 = 0;
	}
	if ( y0 < 0 )
	{
		y0 = 0;
	}
	if ( x1 >= m_GlintWidth )
	{
		x1 = m_GlintWidth-1;
	}
	if ( y1 >= m_GlintHeight )
	{
		y1 = m_GlintHeight-1;
	}

	for (int v = y0; v <= y1; ++v )
	{
		pixelWriter.Seek( x0, v );

		for (int u = x0; u <= x1; ++u )
		{
			float fu = ((float)u) - x;
			float fv = ((float)v) - y;
			const float offset = 0.25;
			float intensity =	GlintGaussSpotCoefficient( fu-offset, fv-offset ) + 
								GlintGaussSpotCoefficient( fu+offset, fv-offset ) + 
								5 * GlintGaussSpotCoefficient( fu, fv ) + 
								GlintGaussSpotCoefficient( fu-offset, fv+offset ) + 
								GlintGaussSpotCoefficient( fu+offset, fv+offset );
			
			// NOTE: Old filter code multiplies the signal by 8X, so we will too
			intensity *= (4.0f/9.0f);

			// NOTE: It's much faster to do the work in the dest texture than to touch the memory more
			// or make more buffers
			Vector outColor = intensity * color;
			int r, g, b, a;
			pixelWriter.ReadPixelNoAdvance( r, g, b, a );
			outColor.x += TextureToLinear(r);
			outColor.y += TextureToLinear(g);
			outColor.z += TextureToLinear(b);
			pixelWriter.WritePixel( LinearToTexture(outColor.x), LinearToTexture(outColor.y), LinearToTexture(outColor.z) );
		}
	}
}


//-----------------------------------------------------------------------------
// glint
//-----------------------------------------------------------------------------

// test/stub code
#if 0
class CEmptyTextureRegen : public ITextureRegenerator
{
public:
	virtual void RegenerateTextureBits( ITexture *pTexture, IVTFTexture *pVTFTexture, Rect_t *pRect )
	{
		// get the texture
		unsigned char *pTextureData = pVTFTexture->ImageData( 0, 0, 0 );
		int nImageSize = pVTFTexture->ComputeMipSize( 0 );
		memset( pTextureData, 0, nImageSize );
	}

	// We've got a global instance, no need to delete it
	virtual void Release() {}
};
static CEmptyTextureRegen s_GlintTextureRegen;
#endif

class CGlintTextureRegenerator : public ITextureRegenerator
{
public:
	virtual void RegenerateTextureBits( ITexture *pTexture, IVTFTexture *pVTFTexture, Rect_t *pRect )
	{
		// We don't need to reconstitute the bits after a task switch
		// since we reconstitute them every frame they are used anyways
		if ( !m_pStudioRender )
			return;

		if ( ( m_pStudioRender->m_GlintWidth != pVTFTexture->Width() ) || 
			( m_pStudioRender->m_GlintHeight != pVTFTexture->Height() ) )
		{
			m_pStudioRender->m_GlintWidth = pVTFTexture->Width();
			m_pStudioRender->m_GlintHeight = pVTFTexture->Height();
		}

		CStudioRender::GlintRenderData_t pRenderData[16];
		int nGlintCount = m_pStudioRender->BuildGlintRenderData( pRenderData, 
			ARRAYSIZE(pRenderData),	m_pState, *m_pVRight, *m_pVUp, *m_pROrigin );

		// setup glint texture
		unsigned char *pTextureData = pVTFTexture->ImageData( 0, 0, 0 );
		CPixelWriter pixelWriter;
		pixelWriter.SetPixelMemory( pVTFTexture->Format(), pTextureData, pVTFTexture->RowSizeInBytes( 0 ) );
  		int nImageSize = pVTFTexture->ComputeMipSize( 0 );
  		memset( pTextureData, 0, nImageSize );

		// Put in glints due to the lights in the scene
		for ( int i = 0; i < nGlintCount; ++i )
		{
			// NOTE: AddGlint is a more expensive solution but it looks better close-up
			m_pStudioRender->AddGlint( pixelWriter, pRenderData[i].m_vecPosition[0], 
				pRenderData[i].m_vecPosition[1], pRenderData[i].m_vecIntensity );
		}
	}

	// We've got a global instance, no need to delete it
	virtual void Release() {}

	const eyeballstate_t *m_pState;
	const Vector *m_pVRight;
	const Vector *m_pVUp;
	const Vector *m_pROrigin;
	CStudioRender *m_pStudioRender;
};

static CGlintTextureRegenerator s_GlintTextureRegen;

static ITexture *s_pProcGlint = NULL;
void CStudioRender::PrecacheGlint()
{
	if ( !m_pGlintTexture )
	{
		// Begin block in which all render targets should be allocated
		g_pMaterialSystem->BeginRenderTargetAllocation();

		// Get the texture that we are going to be updating procedurally.
		m_pGlintTexture = g_pMaterialSystem->CreateNamedRenderTargetTextureEx2( 
			"_rt_eyeglint", 32, 32, RT_SIZE_NO_CHANGE, IMAGE_FORMAT_BGRA8888, MATERIAL_RT_DEPTH_NONE );
		m_pGlintTexture->IncrementReferenceCount();

		// Begin block in which all render targets should be allocated
		g_pMaterialSystem->EndRenderTargetAllocation();

		if ( !IsX360() )
		{
			// Get the texture that we are going to be updating procedurally.
			s_pProcGlint = g_pMaterialSystem->CreateProceduralTexture( 
				"proc_eyeglint", TEXTURE_GROUP_MODEL, 32, 32, IMAGE_FORMAT_BGRA8888, TEXTUREFLAGS_NOMIP|TEXTUREFLAGS_NOLOD );
			s_pProcGlint->SetTextureRegenerator( &s_GlintTextureRegen );
		}

		// JAY: I don't see this pattern in the code often.  It looks like the material system
		// would rather than I deal exclusively with IMaterials instead.
		// So maybe we should bake the LOD texture into the eyes shader.
		// For now, just hardcode one
		// UNDONE: Add a $lodtexture to the eyes shader.  Maybe add a $lodsize too.
		// UNDONE: Make eyes texture load $lodtexture and switch to that here instead of black
		m_pGlintLODTexture = g_pMaterialSystem->FindTexture( IsX360() ? "black" : "vgui/black", NULL, false );
		m_pGlintLODTexture->IncrementReferenceCount();
	}
}

void CStudioRender::UncacheGlint()
{
	if ( m_pGlintTexture )
	{
		if ( s_pProcGlint )
		{
			s_pProcGlint->SetTextureRegenerator( NULL );
			s_pProcGlint->DecrementReferenceCount();
			s_pProcGlint = NULL;
		}
		m_pGlintTexture->DecrementReferenceCount();
		m_pGlintTexture = NULL;
		m_pGlintLODTexture->DecrementReferenceCount();
		m_pGlintLODTexture = NULL;
	}
}

int CStudioRender::BuildGlintRenderData( GlintRenderData_t *pData, int nMaxGlints,
	const eyeballstate_t *pState, const Vector& vright, const Vector& vup, const Vector& r_origin )
{
	// NOTE: See version 25 for lots of #if 0ed out stuff I removed
	Vector viewdelta;
	VectorSubtract( r_origin, pState->org, viewdelta );
	VectorNormalize( viewdelta );

	// hack cornea position
	float iris_radius = pState->peyeball->radius * (6.0 / 12.0);
	float cornea_radius = pState->peyeball->radius * (8.0 / 12.0);

	Vector cornea;
	// position on eyeball that matches iris radius
	float er = ( iris_radius / pState->peyeball->radius );
	er = FastSqrt( 1 - er * er );

	// position on cornea sphere that matches iris radius
	float cr = ( iris_radius / cornea_radius );
	cr = FastSqrt( 1 - cr * cr );

	float r = ( er * pState->peyeball->radius - cr * cornea_radius );
	VectorScale( pState->forward, r, cornea );

	// get offset for center of cornea
	float dx, dy;
	dx = DotProduct( vright, cornea );
	dy = DotProduct( vup, cornea );

	// move cornea to world space
	VectorAdd( cornea, pState->org, cornea );

	Vector delta, intensity;
	Vector reflection, coord;

	// Put in glints due to the lights in the scene
	int nGlintCount = 0;
	for ( int i = 0; R_LightGlintPosition( i, cornea, delta, intensity ); ++i )
	{
		VectorNormalize( delta );
		if ( DotProduct( delta, pState->forward ) <= 0 )
			continue;

		VectorAdd( delta, viewdelta, reflection );
		VectorNormalize( reflection );

		pData[nGlintCount].m_vecPosition[0] = dx + cornea_radius * DotProduct( vright, reflection );
		pData[nGlintCount].m_vecPosition[1] = dy + cornea_radius * DotProduct( vup, reflection );
		pData[nGlintCount].m_vecIntensity = intensity;
		if ( ++nGlintCount >= nMaxGlints )
			return nMaxGlints;

		if ( !R_LightGlintPosition( i, pState->org, delta, intensity ) )
			continue;

		VectorNormalize( delta );
		if ( DotProduct( delta, pState->forward ) >= er )
			continue;

		pData[nGlintCount].m_vecPosition[0] = pState->peyeball->radius * DotProduct( vright, reflection );
		pData[nGlintCount].m_vecPosition[1] = pState->peyeball->radius * DotProduct( vup, reflection );
		pData[nGlintCount].m_vecIntensity = intensity;
		if ( ++nGlintCount >= nMaxGlints )
			return nMaxGlints;
	}
	return nGlintCount;
}


//-----------------------------------------------------------------------------
// Renders a glint texture procedurally
//-----------------------------------------------------------------------------
ITexture* CStudioRender::RenderGlintTexture( const eyeballstate_t *pState,
	const Vector& vright, const Vector& vup, const Vector& r_origin )
{
	GlintRenderData_t pRenderData[16];
	int nGlintCount = BuildGlintRenderData( pRenderData, ARRAYSIZE(pRenderData),
		pState, vright, vup, r_origin );

	if ( nGlintCount == 0 )
		return m_pGlintLODTexture;

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->PushRenderTargetAndViewport( m_pGlintTexture );

	IMaterial *pPrevMaterial = pRenderContext->GetCurrentMaterial();
	void *pPrevProxy = pRenderContext->GetCurrentProxy();
	int nPrevBoneCount = pRenderContext->GetCurrentNumBones();
	MaterialHeightClipMode_t nPrevClipMode = pRenderContext->GetHeightClipMode( );
	bool bPrevClippingEnabled = pRenderContext->EnableClipping( false );
	bool bInFlashlightMode = pRenderContext->GetFlashlightMode();

	if ( bInFlashlightMode )
	{
		DisableScissor();
	}
	pRenderContext->ClearColor4ub( 0, 0, 0, 0 );
	pRenderContext->ClearBuffers( true, false, false );

	pRenderContext->SetFlashlightMode( false );
	pRenderContext->SetHeightClipMode( MATERIAL_HEIGHTCLIPMODE_DISABLE );
	pRenderContext->SetNumBoneWeights( 0 );
	pRenderContext->Bind( m_pGlintBuildMaterial );
	
	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	CMeshBuilder meshBuilder;
	IMesh *pMesh = pRenderContext->GetDynamicMesh( );
	meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, nGlintCount * 4, nGlintCount * 6 );

	const float epsilon = 0.5f / 32.0f;
	int nIndex = 0;
	for ( int i = 0; i < nGlintCount; ++i )
	{
		const GlintRenderData_t &glint = pRenderData[i];

		// Position of glint 0..31 range
		float x = (glint.m_vecPosition.x + 0.5f) * m_GlintWidth;
		float y = (glint.m_vecPosition.y + 0.5f) * m_GlintHeight;
		Vector vGlintCenter = Vector( x, y, 0.0f );
		float ooWidth  = 1.0f / (float)m_GlintWidth;
		float ooHeight = 1.0f / (float)m_GlintHeight;

		int x0 = floor(x);
		int y0 = floor(y);
		int x1 = x0 + 1.0f;
		int y1 = y0 + 1.0f;
		x0 -= 2.0f;				// Fill rules make us pad this out more than the procedural version
		y0 -= 2.0f;

		float screenX0 =   x0 * 2 * ooWidth  + epsilon - 1;
		float screenX1 =   x1 * 2 * ooWidth  + epsilon - 1;
		float screenY0 = -(y0 * 2 * ooHeight + epsilon - 1);
		float screenY1 = -(y1 * 2 * ooHeight + epsilon - 1);

		meshBuilder.Position3f( screenX0, screenY0, 0.0f );
		meshBuilder.TexCoord2f(  0, x0, y0 );
		meshBuilder.TexCoord2fv( 1, vGlintCenter.Base() );
		meshBuilder.TexCoord3fv( 2, glint.m_vecIntensity.Base() );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3f( screenX1, screenY0, 0.0f );
		meshBuilder.TexCoord2f(  0, x1, y0 );
		meshBuilder.TexCoord2fv( 1, vGlintCenter.Base() );
		meshBuilder.TexCoord3fv( 2, glint.m_vecIntensity.Base() );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3f( screenX1, screenY1, 0.0f );
		meshBuilder.TexCoord2f(  0, x1, y1 ); 
		meshBuilder.TexCoord2fv( 1, vGlintCenter.Base() );
		meshBuilder.TexCoord3fv( 2, glint.m_vecIntensity.Base() );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3f( screenX0, screenY1, 0.0f );
		meshBuilder.TexCoord2f(  0, x0, y1 ); 
		meshBuilder.TexCoord2fv( 1, vGlintCenter.Base() );
		meshBuilder.TexCoord3fv( 2, glint.m_vecIntensity.Base() );
		meshBuilder.AdvanceVertex();

		meshBuilder.FastIndex( nIndex );
		meshBuilder.FastIndex( nIndex+1 );
		meshBuilder.FastIndex( nIndex+2 );
		meshBuilder.FastIndex( nIndex );
		meshBuilder.FastIndex( nIndex+2 );
		meshBuilder.FastIndex( nIndex+3 );
		nIndex += 4;
	}

	meshBuilder.End();
	pMesh->Draw();

	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PopMatrix();

	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PopMatrix();

	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PopMatrix();

	if ( IsX360() )
	{
		pRenderContext->CopyRenderTargetToTextureEx( m_pGlintTexture, 0, NULL, NULL );
	}

	pRenderContext->PopRenderTargetAndViewport( );

	pRenderContext->Bind( pPrevMaterial, pPrevProxy );
	pRenderContext->SetNumBoneWeights( nPrevBoneCount );
	pRenderContext->SetHeightClipMode( nPrevClipMode );
	pRenderContext->EnableClipping( bPrevClippingEnabled );
	pRenderContext->SetFlashlightMode( bInFlashlightMode );

	return m_pGlintTexture;
}

static ConVar r_glint_procedural( "r_glint_procedural", "0" );
static ConVar r_glint_alwaysdraw( "r_glint_alwaysdraw", "0" );

void CStudioRender::R_StudioEyeballGlint( const eyeballstate_t *pstate, IMaterialVar *pGlintVar, 
							const Vector& vright, const Vector& vup, const Vector& r_origin )
{
	// Kick off a PIX event, since this process encompasses a bunch of locks etc...
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	PIXEVENT( pRenderContext, "GenerateEyeballGlint" );

	// Don't do a procedural glint texture if there are enough pixels covered by the eyeball onscreen,
	// and the eye isn't backfaced.
	if ( m_pGlintLODTexture && r_glint_alwaysdraw.GetInt() == 0 )
	{
		// backfaced or too small to bother?
		float pixelArea = pRenderContext->ComputePixelWidthOfSphere( pstate->org, pstate->peyeball->radius );
		if( 
			// FIXME: this backface doesn't work for something that isn't a plane.
			 // DotProduct( pstate->forward, m_ViewPlaneNormal ) > 0.0f ||
			 pixelArea < m_pRC->m_Config.fEyeGlintPixelWidthLODThreshold )
		{
			// use black glint texture
			pGlintVar->SetTextureValue( m_pGlintLODTexture );
			return;
		}
	}

	// Legacy method for DX8
	if ( !IsX360() && ( r_glint_procedural.GetInt() || g_pMaterialSystemHardwareConfig->GetDXSupportLevel() < 90 ) )
	{
		// Set up the texture regenerator
		s_GlintTextureRegen.m_pVRight = &vright;
		s_GlintTextureRegen.m_pVUp = &vup;
		s_GlintTextureRegen.m_pROrigin = &r_origin;
		s_GlintTextureRegen.m_pState = pstate;
		s_GlintTextureRegen.m_pStudioRender = this;

		// This will cause the glint texture to be re-generated and then downloaded
		s_pProcGlint->Download( );

		// This is necessary to make sure we don't reconstitute the bits
		// after coming back from a task switch
		s_GlintTextureRegen.m_pStudioRender = NULL;

		// Use the normal glint instead of the black glint
		pGlintVar->SetTextureValue( s_pProcGlint );
	}
	else	// Queued hardware version
	{
		// Make sure we know the correct size of the glint texture
		m_GlintWidth = m_pGlintTexture->GetActualWidth();
		m_GlintHeight = m_pGlintTexture->GetActualHeight();

		// Render glint render target
		ITexture *pUseGlintTexture = RenderGlintTexture( pstate, vright, vup, r_origin );

		// Use the normal glint instead of the black glint
		pGlintVar->SetTextureValue( pUseGlintTexture );
	}
}

void CStudioRender::ComputeGlintTextureProjection( eyeballstate_t const* pState, 
							const Vector& vright, const Vector& vup, matrix3x4_t& mat )
{
	// project eyeball into screenspace texture
	float scale = 1.0 / (pState->peyeball->radius * 2);
	VectorScale( &vright.x, scale, mat[0] );
	VectorScale( &vup.x, scale, mat[1] );

	mat[0][3] = -DotProduct( pState->org.Base(), mat[0] ) + 0.5;
	mat[1][3] = -DotProduct( pState->org.Base(), mat[1] ) + 0.5;
}


/*
void R_MouthLighting( int count, const Vector *psrcverts, const Vector *psrcnorms, Vector4D *pdestlightvalues )
{
	Vector forward;

	if (m_pStudioHdr->nummouths < 1) return;

	mstudiomouth_t *pMouth = r_pstudiohdr->pMouth( 0 ); // FIXME: this needs to get the mouth index from the shader

	float fIllum = m_FlexWeights[pMouth->flexdesc];
	if (fIllum < 0) fIllum = 0;
	if (fIllum > 1) fIllum = 1;
	fIllum = LinearToTexture( fIllum ) / 255.0;


	VectorRotate( pMouth->forward, g_StudioInternalState.boneToWorld[ pMouth->bone ], forward );
	
	for (int i = 0; i < count; i++)
	{
		float dot = -DotProduct( psrcnorms[i], forward );
		if (dot > 0)
		{
			dot = LinearToTexture( dot ) / 255.0; // FIXME: this isn't robust
			VectorScale( pdestlightvalues[i], dot, pdestlightvalues[i] );
		}
		else
			VectorFill( pdestlightvalues[i], 0 );

		VectorScale( pdestlightvalues[i], fIllum, pdestlightvalues[i] );
	}
}
*/

void CStudioRender::R_MouthComputeLightingValues( float& fIllum, Vector& forward )
{
	// FIXME: this needs to get the mouth index from the shader
	mstudiomouth_t *pMouth = m_pStudioHdr->pMouth( 0 ); 

	fIllum = m_pFlexWeights[pMouth->flexdesc];
	if (fIllum < 0) fIllum = 0;
	if (fIllum > 1) fIllum = 1;
	fIllum = LinearToTexture( fIllum ) / 255.0;

	VectorRotate( pMouth->forward, m_pBoneToWorld[ pMouth->bone ], forward );
}

void CStudioRender::R_MouthLighting( float fIllum, const Vector& normal, const Vector& forward, Vector &light )
{
	float dot = -DotProduct( normal, forward );
	if (dot > 0)
	{
		VectorScale( light, dot * fIllum, light );
	}
	else
	{
		VectorFill( light, 0 );
	}
}

static unsigned int illumVarCache = 0;
static unsigned int forwardVarCache = 0;
void CStudioRender::R_MouthSetupVertexShader( IMaterial* pMaterial )
{
	if (!pMaterial)
		return;

	// FIXME: this needs to get the mouth index from the shader
	mstudiomouth_t *pMouth = m_pStudioHdr->pMouth( 0 ); 

	// Don't deal with illum gamma, we apply it at a different point
	// for vertex shaders
	float fIllum = m_pFlexWeights[pMouth->flexdesc];
	if (fIllum < 0) fIllum = 0;
	if (fIllum > 1) fIllum = 1;

	Vector forward;
	VectorRotate( pMouth->forward, m_pBoneToWorld[ pMouth->bone ], forward );
	forward *= -1;

	IMaterialVar* pIllumVar = pMaterial->FindVarFast( "$illumfactor", &illumVarCache );
	if (pIllumVar)
	{
		pIllumVar->SetFloatValue( fIllum );
	}

	IMaterialVar* pFowardVar = pMaterial->FindVarFast( "$forward", &forwardVarCache );
	if (pFowardVar)
	{
		pFowardVar->SetVecValue( forward.Base(), 3 );
	}
}
