//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "cbase.h"
#include "bone_merge_cache.h"
#include "bone_setup.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// CBoneMergeCache
//-----------------------------------------------------------------------------

CBoneMergeCache::CBoneMergeCache()
{
	m_pOwner = NULL;
	m_pFollow = NULL;
	m_pFollowHdr = NULL;
	m_pOwnerHdr = NULL;
	m_nFollowBoneSetupMask = 0;
}

void CBoneMergeCache::Init( C_BaseAnimating *pOwner )
{
	m_pOwner = pOwner;
	m_pFollow = NULL;
	m_pFollowHdr = NULL;
	m_pOwnerHdr = NULL;
	m_nFollowBoneSetupMask = 0;
}

void CBoneMergeCache::UpdateCache()
{
	if ( !m_pOwner )
		return;

	CStudioHdr *pOwnerHdr = m_pOwner->GetModelPtr();
	if ( !pOwnerHdr )
		return;

	C_BaseAnimating *pTestFollow = m_pOwner->FindFollowedEntity();
	CStudioHdr *pTestHdr = (pTestFollow ? pTestFollow->GetModelPtr() : NULL);
	// if the follow parent has changed, or any of the underlying models has changed, reset the MergedBones list
	if ( pTestFollow != m_pFollow || pTestHdr != m_pFollowHdr || pOwnerHdr != m_pOwnerHdr )
	{
		m_MergedBones.Purge();
	
		// Update the cache.
		if ( pTestFollow && pTestHdr && pOwnerHdr )
		{
			m_pFollow = pTestFollow;
			m_pFollowHdr = pTestHdr;
			m_pOwnerHdr = pOwnerHdr;

			m_BoneMergeBits.Resize( pOwnerHdr->numbones() );
			m_BoneMergeBits.ClearAll();

			mstudiobone_t *pOwnerBones = m_pOwnerHdr->pBone( 0 );
			
			m_nFollowBoneSetupMask = BONE_USED_BY_BONE_MERGE;
			const bool bDeveloperDebugPrints = developer.GetBool();
			for ( int i = 0; i < m_pOwnerHdr->numbones(); i++ )
			{
				int parentBoneIndex = Studio_BoneIndexByName( m_pFollowHdr, pOwnerBones[i].pszName() );
				if ( parentBoneIndex < 0 )
					continue;

				// Add a merged bone here.
				CMergedBone mergedBone;
				mergedBone.m_iMyBone = i;
				mergedBone.m_iParentBone = parentBoneIndex;
				m_MergedBones.AddToTail( mergedBone );
				m_BoneMergeBits.Set( i );

				// Warn for performance-negative ad hoc bone merges. They're bad. Don't do them.
				if ( ( m_pFollowHdr->boneFlags( parentBoneIndex ) & BONE_USED_BY_BONE_MERGE ) == 0 )
				{
					// go ahead and mark the bone and its parents
					int n = parentBoneIndex;
					while (n != -1)
					{
						m_pFollowHdr->setBoneFlags( n, BONE_USED_BY_BONE_MERGE );
						n = m_pFollowHdr->boneParent( n );
					}
					// dump out a warning
					if ( bDeveloperDebugPrints )
					{
						char sz[ 256 ];
						Q_snprintf( sz, sizeof( sz ), "Performance warning: Add $bonemerge \"%s\" to QC that builds \"%s\"\n",
							m_pFollowHdr->pBone( parentBoneIndex )->pszName(), m_pFollowHdr->pszName() ); 

						static CUtlSymbolTableMT s_FollowerWarnings;
						if ( UTL_INVAL_SYMBOL == s_FollowerWarnings.Find( sz )  )
						{
							s_FollowerWarnings.AddString( sz );
							Warning( "%s", sz );
						}
					}
				}
			}

			// No merged bones found? Slam the mask to 0
			if ( !m_MergedBones.Count() )
			{
				m_nFollowBoneSetupMask = 0;
			}
		}
		else
		{
			m_pFollow = NULL;
			m_pFollowHdr = NULL;
			m_pOwnerHdr = NULL;
			m_nFollowBoneSetupMask = 0;
		}
	}
}

void CBoneMergeCache::MergeMatchingBones( int boneMask, CBoneBitList &boneComputed )
{
	UpdateCache();

	// If this is set, then all the other cache data is set.
	if ( !m_pOwnerHdr || m_MergedBones.Count() == 0 )
		return;

	// Have the entity we're following setup its bones.
	m_pFollow->SetupBones( NULL, -1, m_nFollowBoneSetupMask, gpGlobals->curtime );

	// Now copy the bone matrices.
	for ( int i=0; i < m_MergedBones.Count(); i++ )
	{
		int iOwnerBone = m_MergedBones[i].m_iMyBone;
		int iParentBone = m_MergedBones[i].m_iParentBone;
		
		// Only update bones reference by the bone mask.
		if ( !( m_pOwnerHdr->boneFlags( iOwnerBone ) & boneMask ) )
			continue;

#ifdef SWARM_DLL
		matrix3x4_t matPitchUp;
		AngleMatrix( QAngle( 15, 0, 0 ), matPitchUp );
		ConcatTransforms( m_pFollow->GetBone( iParentBone ), matPitchUp, m_pOwner->GetBoneForWrite( iOwnerBone ) );
#else
		MatrixCopy(m_pFollow->GetBone(iParentBone), m_pOwner->GetBoneForWrite(iOwnerBone));
#endif

		boneComputed.Set( i );
	}
}


bool CBoneMergeCache::GetAimEntOrigin( Vector *pAbsOrigin, QAngle *pAbsAngles )
{
	UpdateCache();

	// If this is set, then all the other cache data is set.
	if ( !m_pOwnerHdr || m_MergedBones.Count() == 0 )
		return false;

	// We want the abs origin such that if we put the entity there, the first merged bone
	// will be aligned. This way the entity will be culled in the correct position.
	//
	// ie: mEntity * mBoneLocal = mFollowBone
	// so: mEntity = mFollowBone * Inverse( mBoneLocal )
	//
	// Note: the code below doesn't take animation into account. If the attached entity animates
	// all over the place, then this won't get the right results.
	
	// Get mFollowBone.
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( 0 );
	m_pFollow->SetupBones( NULL, -1, m_nFollowBoneSetupMask, gpGlobals->curtime );
	const matrix3x4_t &mFollowBone = m_pFollow->GetBone( m_MergedBones[0].m_iParentBone );

	// Get Inverse( mBoneLocal )
	matrix3x4_t mBoneLocal, mBoneLocalInv;
	SetupSingleBoneMatrix( m_pOwnerHdr, m_pOwner->GetSequence(), 0, m_MergedBones[0].m_iMyBone, mBoneLocal );
	MatrixInvert( mBoneLocal, mBoneLocalInv );

	// Now calculate mEntity = mFollowBone * Inverse( mBoneLocal )
	matrix3x4_t mEntity;
	ConcatTransforms( mFollowBone, mBoneLocalInv, mEntity );
	MatrixAngles( mEntity, *pAbsAngles, *pAbsOrigin );

	return true;
}

