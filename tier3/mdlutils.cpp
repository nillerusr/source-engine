//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Utility methods for mdl files
//
//===========================================================================//

#include "tier3/mdlutils.h"
#include "tier0/dbg.h"
#include "tier1/callqueue.h"
#include "tier3/tier3.h"
#include "studio.h"
#include "istudiorender.h"
#include "bone_setup.h"


//-----------------------------------------------------------------------------
// Returns the bounding box for the model
//-----------------------------------------------------------------------------
void GetMDLBoundingBox( Vector *pMins, Vector *pMaxs, MDLHandle_t h, int nSequence )
{
	if ( h == MDLHANDLE_INVALID || !g_pMDLCache )
	{
		pMins->Init();
		pMaxs->Init();
		return;
	}

	pMins->Init( FLT_MAX, FLT_MAX );
	pMaxs->Init( -FLT_MAX, -FLT_MAX );

	studiohdr_t *pStudioHdr = g_pMDLCache->GetStudioHdr( h );
	if ( !VectorCompare( vec3_origin, pStudioHdr->view_bbmin ) || !VectorCompare( vec3_origin, pStudioHdr->view_bbmax ))
	{
		// look for view clip
		*pMins = pStudioHdr->view_bbmin;
		*pMaxs = pStudioHdr->view_bbmax;
	}
	else if ( !VectorCompare( vec3_origin, pStudioHdr->hull_min ) || !VectorCompare( vec3_origin, pStudioHdr->hull_max ))
	{
		// look for hull
		*pMins = pStudioHdr->hull_min;
		*pMaxs = pStudioHdr->hull_max;
	}

	// Else use the sequence box
	mstudioseqdesc_t &seqdesc = pStudioHdr->pSeqdesc( nSequence );
	VectorMin( seqdesc.bbmin, *pMins, *pMins );
	VectorMax( seqdesc.bbmax, *pMaxs, *pMaxs );
}


//-----------------------------------------------------------------------------
// Returns the radius of the model as measured from the origin
//-----------------------------------------------------------------------------
float GetMDLRadius( MDLHandle_t h, int nSequence )
{
	Vector vecMins, vecMaxs;
	GetMDLBoundingBox( &vecMins, &vecMaxs, h, nSequence );
	float flRadius = vecMaxs.Length();
	float flRadius2 = vecMins.Length();
	if ( flRadius2 > flRadius )
	{
		flRadius = flRadius2;
	}
	return flRadius;
}


//-----------------------------------------------------------------------------
// Returns a more accurate bounding sphere
//-----------------------------------------------------------------------------
void GetMDLBoundingSphere( Vector *pVecCenter, float *pRadius, MDLHandle_t h, int nSequence )
{
	Vector vecMins, vecMaxs;
	GetMDLBoundingBox( &vecMins, &vecMaxs, h, nSequence );
	VectorAdd( vecMins, vecMaxs, *pVecCenter );
	*pVecCenter *= 0.5f;
	*pRadius = vecMaxs.DistTo( *pVecCenter );
}


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CMDL::CMDL()
{
	m_MDLHandle = MDLHANDLE_INVALID;
	m_Color.SetColor( 255, 255, 255, 255 );
	m_nSkin = 0;
	m_nBody = 0;
	m_nSequence = 0;
	m_nLOD = 0;
	m_flPlaybackRate = 30.0f;
	m_flTime = 0.0f;
	m_vecViewTarget.Init( 0, 0, 0 );
	m_bWorldSpaceViewTarget = false;
	memset( m_pFlexControls, 0, sizeof(m_pFlexControls) );
	m_pProxyData = NULL;
}

CMDL::~CMDL()
{
	UnreferenceMDL();
}

void CMDL::SetMDL( MDLHandle_t h )
{
	UnreferenceMDL();
	m_MDLHandle = h;
	if ( m_MDLHandle != MDLHANDLE_INVALID )
	{
		g_pMDLCache->AddRef( m_MDLHandle );
		
		studiohdr_t *pHdr = g_pMDLCache->LockStudioHdr( m_MDLHandle );

		if ( pHdr )
		{
			for ( LocalFlexController_t i = LocalFlexController_t(0); i < pHdr->numflexcontrollers; ++i )
			{
				if ( pHdr->pFlexcontroller( i )->localToGlobal == -1 )
				{
					pHdr->pFlexcontroller( i )->localToGlobal = i;
				}
			}
		}
	}
}

MDLHandle_t CMDL::GetMDL() const
{
	return m_MDLHandle;
}


//-----------------------------------------------------------------------------
// Release the MDL handle
//-----------------------------------------------------------------------------
void CMDL::UnreferenceMDL()
{
	if ( !g_pMDLCache )
		return;

	if ( m_MDLHandle != MDLHANDLE_INVALID )
	{
		// XXX need to figure out where it is safe to flush the queue during map change to not crash
#if 0
		if ( ICallQueue *pCallQueue = materials->GetRenderContext()->GetCallQueue() )
		{
			// Parallel rendering: don't unlock model data until end of rendering
			pCallQueue->QueueCall( g_pMDLCache, &IMDLCache::UnlockStudioHdr, m_MDLHandle );
			pCallQueue->QueueCall( g_pMDLCache, &IMDLCache::Release, m_MDLHandle );
		}
		else
#endif
		{
			// Immediate-mode rendering, can unlock immediately
			g_pMDLCache->UnlockStudioHdr( m_MDLHandle );
			g_pMDLCache->Release( m_MDLHandle );
		}
		m_MDLHandle = MDLHANDLE_INVALID;
	}
}


//-----------------------------------------------------------------------------
// Gets the studiohdr
//-----------------------------------------------------------------------------
studiohdr_t *CMDL::GetStudioHdr()
{
	if ( !g_pMDLCache )
		return NULL;
	return g_pMDLCache->GetStudioHdr( m_MDLHandle );
}


//-----------------------------------------------------------------------------
// Draws the mesh
//-----------------------------------------------------------------------------
void CMDL::Draw( const matrix3x4_t& rootToWorld, const matrix3x4_t *pBoneToWorld )
{
	if ( !g_pMaterialSystem || !g_pMDLCache || !g_pStudioRender )
		return;

	if ( m_MDLHandle == MDLHANDLE_INVALID )
		return;

	// Color + alpha modulation
	Vector white( m_Color.r() / 255.0f, m_Color.g() / 255.0f, m_Color.b() / 255.0f );
	g_pStudioRender->SetColorModulation( white.Base() );
	g_pStudioRender->SetAlphaModulation( m_Color.a() / 255.0f );

	DrawModelInfo_t info;
	info.m_pStudioHdr = g_pMDLCache->GetStudioHdr( m_MDLHandle );
	info.m_pHardwareData = g_pMDLCache->GetHardwareData( m_MDLHandle );
	info.m_Decals = STUDIORENDER_DECAL_INVALID;
	info.m_Skin = m_nSkin;
	info.m_Body = m_nBody;
	info.m_HitboxSet = 0;
	info.m_pClientEntity = m_pProxyData;
	info.m_pColorMeshes = NULL;
	info.m_bStaticLighting = false;
	info.m_Lod = m_nLOD;

	Vector vecWorldViewTarget;
	if ( m_bWorldSpaceViewTarget )
	{
		vecWorldViewTarget = m_vecViewTarget;
	}
	else
	{
		VectorTransform( m_vecViewTarget, rootToWorld, vecWorldViewTarget );
	}
	g_pStudioRender->SetEyeViewTarget( info.m_pStudioHdr, info.m_Body, vecWorldViewTarget );

	// FIXME: Why is this necessary!?!?!?
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );

	// Set default flex values
	float *pFlexWeights = NULL;
	const int nFlexDescCount = info.m_pStudioHdr->numflexdesc;
	if ( nFlexDescCount )
	{
		CStudioHdr cStudioHdr( info.m_pStudioHdr, g_pMDLCache );

		g_pStudioRender->LockFlexWeights( info.m_pStudioHdr->numflexdesc, &pFlexWeights );
		cStudioHdr.RunFlexRules( m_pFlexControls, pFlexWeights );
		g_pStudioRender->UnlockFlexWeights();
	}

	Vector vecModelOrigin;
	MatrixGetColumn( rootToWorld, 3, vecModelOrigin );
	g_pStudioRender->DrawModel( NULL, info, const_cast<matrix3x4_t*>( pBoneToWorld ), 
		pFlexWeights, NULL, vecModelOrigin, STUDIORENDER_DRAW_ENTIRE_MODEL );
}

void CMDL::Draw( const matrix3x4_t &rootToWorld )
{
	if ( !g_pMaterialSystem || !g_pMDLCache || !g_pStudioRender )
		return;

	if ( m_MDLHandle == MDLHANDLE_INVALID )
		return;

	studiohdr_t *pStudioHdr = g_pMDLCache->GetStudioHdr( m_MDLHandle );

	matrix3x4_t *pBoneToWorld = g_pStudioRender->LockBoneMatrices( pStudioHdr->numbones );
	SetUpBones( rootToWorld, pStudioHdr->numbones, pBoneToWorld );
	g_pStudioRender->UnlockBoneMatrices();

	Draw( rootToWorld, pBoneToWorld );
}


void CMDL::SetUpBones( const matrix3x4_t& rootToWorld, int nMaxBoneCount, matrix3x4_t *pBoneToWorld, const float *pPoseParameters, MDLSquenceLayer_t *pSequenceLayers, int nNumSequenceLayers )
{
	CStudioHdr studioHdr( g_pMDLCache->GetStudioHdr( m_MDLHandle ), g_pMDLCache );

	float pPoseParameter[MAXSTUDIOPOSEPARAM];
	if ( pPoseParameters )
	{
		V_memcpy( pPoseParameter, pPoseParameters, sizeof(pPoseParameter) );
	}
	else
	{
		// Default to middle of the pose parameter range
		int nPoseCount = studioHdr.GetNumPoseParameters();
		for ( int i = 0; i < MAXSTUDIOPOSEPARAM; ++i )
		{
			pPoseParameter[i] = 0.5f;
			if ( i < nPoseCount )
			{
				const mstudioposeparamdesc_t &Pose = studioHdr.pPoseParameter( i );

				// Want to try for a zero state.  If one doesn't exist set it to .5 by default.
				if ( Pose.start < 0.0f && Pose.end > 0.0f )
				{
					float flPoseDelta = Pose.end - Pose.start;
					pPoseParameter[i] = -Pose.start / flPoseDelta;
				}
			}
		}
	}

	int nFrameCount = Studio_MaxFrame( &studioHdr, m_nSequence, pPoseParameter );
	if ( nFrameCount == 0 )
	{
		nFrameCount = 1;
	}
	float flCycle = ( m_flTime * m_flPlaybackRate ) / nFrameCount;

	// FIXME: We're always wrapping; may want to determing if we should clamp
	flCycle -= (int)(flCycle);

	Vector		pos[MAXSTUDIOBONES];
	Quaternion	q[MAXSTUDIOBONES];

	IBoneSetup boneSetup( &studioHdr, BONE_USED_BY_ANYTHING_AT_LOD( m_nLOD ), pPoseParameter, NULL );
	boneSetup.InitPose( pos, q );
	boneSetup.AccumulatePose( pos, q, m_nSequence, flCycle, 1.0f, m_flTime, NULL );

	// Accumulate the additional layers if specified.
	if ( pSequenceLayers )
	{
		int nNumSeq = studioHdr.GetNumSeq();
		for ( int i = 0; i < nNumSequenceLayers; ++i )
		{
			int nSeqIndex = pSequenceLayers[ i ].m_nSequenceIndex;
			if ( ( nSeqIndex >= 0 ) && ( nSeqIndex < nNumSeq ) )
			{				
				float flWeight = pSequenceLayers[ i ].m_flWeight;

				float flLayerCycle;
				int nLayerFrameCount = MAX( 1, Studio_MaxFrame( &studioHdr, nSeqIndex, pPoseParameter ) );

				if ( pSequenceLayers[i].m_bNoLoop )
				{
					if ( pSequenceLayers[i].m_flCycleBeganAt == 0 )
					{
						pSequenceLayers[i].m_flCycleBeganAt = m_flTime;
					}

					float flElapsedTime = m_flTime - pSequenceLayers[i].m_flCycleBeganAt;
					flLayerCycle = ( flElapsedTime * m_flPlaybackRate ) / nLayerFrameCount;

					// Should we keep playing layers that have ended?
					//if ( flLayerCycle >= 1.0 )
						//continue;
				}
				else
				{
					flLayerCycle = ( m_flTime * m_flPlaybackRate ) / nLayerFrameCount;

					// FIXME: We're always wrapping; may want to determing if we should clamp
					flLayerCycle -= (int)(flLayerCycle);
				}

				boneSetup.AccumulatePose( pos, q, nSeqIndex, flLayerCycle, flWeight, m_flTime, NULL );
			}
		}
	}

	// FIXME: Try enabling this?
	//	CalcAutoplaySequences( pStudioHdr, NULL, pos, q, pPoseParameter, BONE_USED_BY_VERTEX_AT_LOD( m_nLOD ), flTime );

	matrix3x4_t temp;

	if ( nMaxBoneCount > studioHdr.numbones() )
	{
		nMaxBoneCount = studioHdr.numbones();
	}

	for ( int i = 0; i < nMaxBoneCount; i++ ) 
	{
		// If it's not being used, fill with NAN for errors
#ifdef _DEBUG
		if ( !(studioHdr.pBone( i )->flags & BONE_USED_BY_ANYTHING_AT_LOD( m_nLOD ) ) )
		{
			int j, k;
			for (j = 0; j < 3; j++)
			{
				for (k = 0; k < 4; k++)
				{
					pBoneToWorld[i][j][k] = VEC_T_NAN;
				}
			}
			continue;
		}
#endif

		matrix3x4_t boneMatrix;
		QuaternionMatrix( q[i], boneMatrix );
		MatrixSetColumn( pos[i], 3, boneMatrix );

		if ( studioHdr.pBone(i)->parent == -1 ) 
		{
			ConcatTransforms( rootToWorld, boneMatrix, pBoneToWorld[i] );
		} 
		else 
		{
			ConcatTransforms( pBoneToWorld[ studioHdr.pBone(i)->parent ], boneMatrix, pBoneToWorld[i] );
		}
	}
	Studio_RunBoneFlexDrivers( m_pFlexControls, &studioHdr, pos, pBoneToWorld, rootToWorld );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMDL::SetupBonesWithBoneMerge( const CStudioHdr *pMergeHdr, matrix3x4_t *pMergeBoneToWorld, 
								    const CStudioHdr *pFollow, const matrix3x4_t *pFollowBoneToWorld,
									const matrix3x4_t &matModelToWorld )
{
	// Default to middle of the pose parameter range
	int nPoseCount = pMergeHdr->GetNumPoseParameters();
	float pPoseParameter[MAXSTUDIOPOSEPARAM];
	for ( int i = 0; i < MAXSTUDIOPOSEPARAM; ++i )
	{
		pPoseParameter[i] = 0.5f;
		if ( i < nPoseCount )
		{
			const mstudioposeparamdesc_t &Pose = ((CStudioHdr *)pMergeHdr)->pPoseParameter( i );

			// Want to try for a zero state.  If one doesn't exist set it to .5 by default.
			if ( Pose.start < 0.0f && Pose.end > 0.0f )
			{
				float flPoseDelta = Pose.end - Pose.start;
				pPoseParameter[i] = -Pose.start / flPoseDelta;
			}
		}
	}

	int nFrameCount = Studio_MaxFrame( pMergeHdr, m_nSequence, pPoseParameter );
	if ( nFrameCount == 0 )
	{
		nFrameCount = 1;
	}
	float flCycle = ( m_flTime * m_flPlaybackRate ) / nFrameCount;

	// FIXME: We're always wrapping; may want to determing if we should clamp
	flCycle -= (int)(flCycle);

	Vector pos[MAXSTUDIOBONES];
	Quaternion q[MAXSTUDIOBONES];

	IBoneSetup boneSetup( pMergeHdr,  BONE_USED_BY_ANYTHING_AT_LOD( m_nLOD ), pPoseParameter );
	boneSetup.InitPose( pos, q );
	boneSetup.AccumulatePose( pos, q, m_nSequence, flCycle, 1.0f, m_flTime, NULL );

	// Get the merge bone list.
	mstudiobone_t *pMergeBones = pMergeHdr->pBone( 0 );
	for ( int iMergeBone = 0; iMergeBone < pMergeHdr->numbones(); ++iMergeBone )
	{
		// Now find the bone in the parent entity.
		bool bMerged = false;
		int iParentBoneIndex = Studio_BoneIndexByName( pFollow, pMergeBones[iMergeBone].pszName() );
		if ( iParentBoneIndex >= 0 )
		{
			MatrixCopy( pFollowBoneToWorld[iParentBoneIndex], pMergeBoneToWorld[iMergeBone] );
			bMerged = true;
		}

		if ( !bMerged )
		{
			// If we get down here, then the bone wasn't merged.
			matrix3x4_t matBone;
			QuaternionMatrix( q[iMergeBone], pos[iMergeBone], matBone );

			if ( pMergeBones[iMergeBone].parent == -1 ) 
			{
				ConcatTransforms( matModelToWorld, matBone, pMergeBoneToWorld[iMergeBone] );
			} 
			else 
			{
				ConcatTransforms( pMergeBoneToWorld[pMergeBones[iMergeBone].parent], matBone, pMergeBoneToWorld[iMergeBone] );
			}
		}
	}
}

