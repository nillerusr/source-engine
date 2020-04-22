//========= Copyright Valve Corporation, All rights reserved. ============//
//
// r_studio.cpp: routines for setting up to draw 3DStudio models 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//===========================================================================//


#include "studio.h"
#include "studiorender.h"
#include "studiorendercontext.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialvar.h"
#include "tier0/vprof.h"
#include "tier3/tier3.h"
#include "datacache/imdlcache.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Figures out what kind of lighting we're gonna want
//-----------------------------------------------------------------------------
FORCEINLINE StudioModelLighting_t CStudioRender::R_StudioComputeLighting( IMaterial *pMaterial, int materialFlags, ColorMeshInfo_t *pColorMeshes )
{
	// Here, we only do software lighting when the following conditions are met.
	// 1) The material is vertex lit and we don't have hardware lighting
	// 2) We're drawing an eyeball
	// 3) We're drawing mouth-lit stuff

	// FIXME: When we move software lighting into the material system, only need to
	// test if it's vertex lit

	Assert( pMaterial );
	bool doMouthLighting = materialFlags && (m_pStudioHdr->nummouths >= 1);

	if ( IsX360() )
	{
		// 360 does not do software lighting
		return doMouthLighting ? LIGHTING_MOUTH : LIGHTING_HARDWARE;
	}

	bool doSoftwareLighting = doMouthLighting ||
		(pMaterial->IsVertexLit() && pMaterial->NeedsSoftwareLighting() );

	if ( !m_pRC->m_Config.m_bSupportsVertexAndPixelShaders )
	{
		if ( !doSoftwareLighting && pColorMeshes )
		{
			pMaterial->SetUseFixedFunctionBakedLighting( true );
		}
		else
		{
			doSoftwareLighting = true;
			pMaterial->SetUseFixedFunctionBakedLighting( false );
		}
	}

	StudioModelLighting_t lighting = LIGHTING_HARDWARE;
	if ( doMouthLighting )
		lighting = LIGHTING_MOUTH;
	else if ( doSoftwareLighting )
		lighting = LIGHTING_SOFTWARE;

	return lighting;
}


IMaterial* CStudioRender::R_StudioSetupSkinAndLighting( IMatRenderContext *pRenderContext, int index, IMaterial **ppMaterials, int materialFlags,  
	void /*IClientRenderable*/ *pClientRenderable, ColorMeshInfo_t *pColorMeshes, StudioModelLighting_t &lighting )
{
	VPROF( "R_StudioSetupSkin" );
	IMaterial *pMaterial = NULL;
	bool bCheckForConVarDrawTranslucentSubModels = false;
	if( m_pRC->m_Config.bWireframe && !m_pRC->m_pForcedMaterial )
	{
		if ( m_pRC->m_Config.bDrawZBufferedWireframe )
			pMaterial = m_pMaterialMRMWireframeZBuffer;
		else
			pMaterial = m_pMaterialMRMWireframe;
	}
	else if( m_pRC->m_Config.bShowEnvCubemapOnly )
	{
		pMaterial = m_pMaterialModelEnvCubemap;
	}
	else
	{
		if ( !m_pRC->m_pForcedMaterial && ( m_pRC->m_nForcedMaterialType != OVERRIDE_DEPTH_WRITE && m_pRC->m_nForcedMaterialType != OVERRIDE_SSAO_DEPTH_WRITE ) )
		{
			pMaterial = ppMaterials[index];
			if ( !pMaterial )
			{
				Assert( 0 );
				return 0;
			}
		}
		else
		{
			materialFlags = 0;
			pMaterial = m_pRC->m_pForcedMaterial;
			if (m_pRC->m_nForcedMaterialType == OVERRIDE_BUILD_SHADOWS)
			{
				// Connect the original material up to the shadow building material
				// Also bind the original material so its proxies are in the correct state
				static unsigned int translucentCache = 0;
				IMaterialVar* pOriginalMaterialVar = pMaterial->FindVarFast( "$translucent_material", &translucentCache );
				Assert( pOriginalMaterialVar );
				IMaterial *pOriginalMaterial = ppMaterials[index];
				if ( pOriginalMaterial )
				{
					// Disable any alpha modulation on the original material that was left over from when it was last rendered
					pOriginalMaterial->AlphaModulate( 1.0f );
					pRenderContext->Bind( pOriginalMaterial, pClientRenderable );
					if ( pOriginalMaterial->IsTranslucent() || pOriginalMaterial->IsAlphaTested() )
					{
						if ( pOriginalMaterialVar )
							pOriginalMaterialVar->SetMaterialValue( pOriginalMaterial );
					}
					else
					{
						if ( pOriginalMaterialVar )
							pOriginalMaterialVar->SetMaterialValue( NULL );
					}
				}
				else
				{
					if ( pOriginalMaterialVar )
						pOriginalMaterialVar->SetMaterialValue( NULL );
				}
			}
			else if ( m_pRC->m_nForcedMaterialType == OVERRIDE_DEPTH_WRITE || m_pRC->m_nForcedMaterialType == OVERRIDE_SSAO_DEPTH_WRITE )
			{
				// Disable any alpha modulation on the original material that was left over from when it was last rendered
				ppMaterials[index]->AlphaModulate( 1.0f );

				// Bail if the material is still considered translucent after setting the AlphaModulate to 1.0
				if ( ppMaterials[index]->IsTranslucent() )
				{
					return NULL;
				}

				static unsigned int originalTextureVarCache = 0;
				IMaterialVar *pOriginalTextureVar = ppMaterials[index]->FindVarFast( "$basetexture", &originalTextureVarCache );

				// Select proper override material
				int nAlphaTest = (int) ( ppMaterials[index]->IsAlphaTested() && pOriginalTextureVar->IsTexture() ); // alpha tested base texture
				int nNoCull = (int) ppMaterials[index]->IsTwoSided();
				if ( m_pRC->m_nForcedMaterialType == OVERRIDE_SSAO_DEPTH_WRITE )
				{
					pMaterial = m_pSSAODepthWrite[nAlphaTest][nNoCull];
				}
				else
				{
					pMaterial = m_pDepthWrite[nAlphaTest][nNoCull];
				}

				// If we're alpha tested, we should set up the texture variables from the original material
				if ( nAlphaTest != 0 )
				{
					static unsigned int originalTextureFrameVarCache = 0;
					IMaterialVar *pOriginalTextureFrameVar = ppMaterials[index]->FindVarFast( "$frame", &originalTextureFrameVarCache );
					static unsigned int originalAlphaRefCache = 0;
					IMaterialVar *pOriginalAlphaRefVar = ppMaterials[index]->FindVarFast( "$AlphaTestReference", &originalAlphaRefCache );

					static unsigned int textureVarCache = 0;
					IMaterialVar *pTextureVar = pMaterial->FindVarFast( "$basetexture", &textureVarCache );
					static unsigned int textureFrameVarCache = 0;
					IMaterialVar *pTextureFrameVar = pMaterial->FindVarFast( "$frame", &textureFrameVarCache );
					static unsigned int alphaRefCache = 0;
					IMaterialVar *pAlphaRefVar = pMaterial->FindVarFast( "$AlphaTestReference", &alphaRefCache );

					if ( pOriginalTextureVar->IsTexture() ) // If $basetexture is defined
					{
						if( pTextureVar && pOriginalTextureVar )
						{
							pTextureVar->SetTextureValue( pOriginalTextureVar->GetTextureValue() );
						}

						if( pTextureFrameVar && pOriginalTextureFrameVar )
						{
							pTextureFrameVar->SetIntValue( pOriginalTextureFrameVar->GetIntValue() );
						}

						if( pAlphaRefVar && pOriginalAlphaRefVar )
						{
							pAlphaRefVar->SetFloatValue( pOriginalAlphaRefVar->GetFloatValue() );
						}
					}
				}
			}
		}

		// Set this bool to check after the bind below
		bCheckForConVarDrawTranslucentSubModels = true;

		if ( m_pRC->m_nForcedMaterialType != OVERRIDE_DEPTH_WRITE && m_pRC->m_nForcedMaterialType != OVERRIDE_SSAO_DEPTH_WRITE)
		{
			// Try to set the alpha based on the blend
			pMaterial->AlphaModulate( m_pRC->m_AlphaMod );

			// Try to set the color based on the colormod
			pMaterial->ColorModulate( m_pRC->m_ColorMod[0], m_pRC->m_ColorMod[1], m_pRC->m_ColorMod[2] );
		}
	}

	lighting = R_StudioComputeLighting( pMaterial, materialFlags, pColorMeshes );
	if ( lighting == LIGHTING_MOUTH )
	{
		if ( !m_pRC->m_Config.bTeeth || !R_TeethAreVisible() )
			return NULL;
		// skin it and light it, but only if we need to.
		if ( m_pRC->m_Config.m_bSupportsVertexAndPixelShaders )
		{
			R_MouthSetupVertexShader( pMaterial );
		}
	}

	// TODO: It's possible we don't want to use the color texels--for example because of a convar. 
	// We should check that here in addition to whether or not we have the data available.
	static unsigned int lightmapVarCache = 0;
	IMaterialVar *pLightmapVar = pMaterial->FindVarFast( "$lightmap", &lightmapVarCache );
	if ( pLightmapVar )
	{
		ITexture* newTex = pColorMeshes ? pColorMeshes->m_pLightmap : NULL;

		if (newTex)
			pLightmapVar->SetTextureValue(newTex);
		else 
			pLightmapVar->SetUndefined();
	}
	
	pRenderContext->Bind( pMaterial, pClientRenderable );

	if ( bCheckForConVarDrawTranslucentSubModels )
	{
		bool translucent = pMaterial->IsTranslucent();

		if (( m_bDrawTranslucentSubModels && !translucent ) ||
			( !m_bDrawTranslucentSubModels && translucent ))
		{
			m_bSkippedMeshes = true;
			return NULL;
		}
	}

	return pMaterial;
}



//=============================================================================


/*
=================
R_StudioSetupModel
	based on the body part, figure out which mesh it should be using.
inputs:
outputs:
	pstudiomesh
	pmdl
=================
*/
int R_StudioSetupModel( int bodypart, int entity_body, mstudiomodel_t **ppSubModel, 
	const studiohdr_t *pStudioHdr )
{
	int index;
	mstudiobodyparts_t   *pbodypart;

	if (bodypart > pStudioHdr->numbodyparts)
	{
		ConDMsg ("R_StudioSetupModel: no such bodypart %d\n", bodypart);
		bodypart = 0;
	}

	pbodypart = pStudioHdr->pBodypart( bodypart );

	if ( pbodypart->base == 0 )
	{
		Warning( "Model has missing body part: %s\n", pStudioHdr->pszName() );
		Assert( 0 );
	}
	index = entity_body / pbodypart->base;
	index = index % pbodypart->nummodels;

	Assert( ppSubModel );
	*ppSubModel = pbodypart->pModel( index );
	return index;
}



//-----------------------------------------------------------------------------
// Generates the PoseToBone Matrix nessecary to align the given bone with the 
// world.
//-----------------------------------------------------------------------------
static void ScreenAlignBone( matrix3x4_t *pPoseToWorld, mstudiobone_t *pCurBone, 
	const Vector& vecViewOrigin, const matrix3x4_t &boneToWorld )
{
	// Grab the world translation:
	Vector vT( boneToWorld[0][3], boneToWorld[1][3], boneToWorld[2][3] );

	// Construct the coordinate frame:
	// Initialized to get rid of compiler 
	Vector vX, vY, vZ;

	if( pCurBone->flags & BONE_SCREEN_ALIGN_SPHERE )
	{
		vX = vecViewOrigin - vT;		    
		VectorNormalize(vX);
		vZ = Vector(0,0,1);
		vY = vZ.Cross(vX);				
		VectorNormalize(vY);
		vZ = vX.Cross(vY);				
		VectorNormalize(vZ);
	} 
	else
	{
		Assert( pCurBone->flags & BONE_SCREEN_ALIGN_CYLINDER );
		vX.Init( boneToWorld[0][0], boneToWorld[1][0], boneToWorld[2][0] );
		vZ = vecViewOrigin - vT;			
		VectorNormalize(vZ);
		vY = vZ.Cross(vX);				
		VectorNormalize(vY);
		vZ = vX.Cross(vY);				
		VectorNormalize(vZ);
	}

	matrix3x4_t matBoneBillboard( 
		vX.x, vY.x, vZ.x, vT.x, 
		vX.y, vY.y, vZ.y, vT.y, 
		vX.z, vY.z, vZ.z, vT.z );
	ConcatTransforms( matBoneBillboard, pCurBone->poseToBone, *pPoseToWorld );
}


//-----------------------------------------------------------------------------
// Computes PoseToWorld from BoneToWorld
//-----------------------------------------------------------------------------
void ComputePoseToWorld( matrix3x4_t *pPoseToWorld, studiohdr_t *pStudioHdr, int boneMask, const Vector& vecViewOrigin, const matrix3x4_t *pBoneToWorld )
{ 
	if ( pStudioHdr->flags & STUDIOHDR_FLAGS_STATIC_PROP )
	{
		// by definition, these always have an identity poseToBone transform
		MatrixCopy( pBoneToWorld[ 0 ], pPoseToWorld[ 0 ] );
		return;
	}

	if ( !pStudioHdr->pLinearBones() )
	{
		// convert bone to world transformations into pose to world transformations
		for (int i = 0; i < pStudioHdr->numbones; i++)
		{
			mstudiobone_t *pCurBone = pStudioHdr->pBone( i );
			if ( !(pCurBone->flags & boneMask) )
				continue;

			ConcatTransforms( pBoneToWorld[ i ], pCurBone->poseToBone, pPoseToWorld[ i ] );
		}
	}
	else
	{
		mstudiolinearbone_t *pLinearBones = pStudioHdr->pLinearBones();

		// convert bone to world transformations into pose to world transformations
		for (int i = 0; i < pStudioHdr->numbones; i++)
		{
			if ( !(pLinearBones->flags(i) & boneMask) )
				continue;

			ConcatTransforms( pBoneToWorld[ i ], pLinearBones->poseToBone(i), pPoseToWorld[ i ] );
		}
	}

#if 0
			// These don't seem to be used in any existing QC file, re-enable in a future project?
			// Pretransform
			if( !( pCurBone->flags & ( BONE_SCREEN_ALIGN_SPHERE | BONE_SCREEN_ALIGN_CYLINDER )))
			{
				ConcatTransforms( pBoneToWorld[ i ], pCurBone->poseToBone, pPoseToWorld[ i ] );
			}
			else 
			{
				// If this bone is screen aligned, then generate a PoseToWorld matrix that billboards the bone
				ScreenAlignBone( &pPoseToWorld[i], pCurBone, vecViewOrigin, pBoneToWorld[i] );
			} 	
#endif
}


