#include "cbase.h"
#include "asw_bone_merge.h"
#include "c_asw_weapon.h"
#include "bone_setup.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Alien Swarm weapons use custom bone merge cache so they can pitch the guns up slightly
void C_ASW_Weapon::CalcBoneMerge( CStudioHdr *hdr, int boneMask, CBoneBitList &boneComputed )
{
	// For EF_BONEMERGE entities, copy the bone matrices for any bones that have matching names.
	bool boneMerge = IsEffectActive(EF_BONEMERGE);
	if ( boneMerge || m_pBoneMergeCache )
	{
		if ( boneMerge )
		{
			if ( !m_pBoneMergeCache )
			{
				m_pBoneMergeCache = new CASW_Bone_Merge_Cache;
				m_pBoneMergeCache->Init( this );
			}
			CASW_Bone_Merge_Cache *pASWBoneMergeCache = static_cast<CASW_Bone_Merge_Cache*>( m_pBoneMergeCache );
			pASWBoneMergeCache->MergeMatchingBones( boneMask, boneComputed, ShouldAlignWeaponToLaserPointer(), m_vecLaserPointerDirection );

			int iAttachment = GetMuzzleAttachment();
			if ( iAttachment > 0 && m_pLaserPointerEffect )
			{
				Vector vecOrigin;
				QAngle angWeapon;
				GetAttachment( iAttachment, vecOrigin, angWeapon );
				m_pLaserPointerEffect->SetControlPoint( 1, vecOrigin );
			}
		}
		else
		{
			delete m_pBoneMergeCache;
			m_pBoneMergeCache = NULL;
		}
	}
}

ConVar asw_weapon_pitch( "asw_weapon_pitch", "12", FCVAR_NONE );


CASW_Bone_Merge_Cache::CASW_Bone_Merge_Cache()
{
	m_nRightHandBoneID = -1;
}

// apply custom pitch to bone merge
void CASW_Bone_Merge_Cache::MergeMatchingBones( int boneMask, CBoneBitList &boneComputed, bool bOverrideDirection, const Vector &vecDir )
{
	UpdateCache();

	// If this is set, then all the other cache data is set.
	if ( !m_pOwnerHdr || m_MergedBones.Count() == 0 )
		return;

	// Have the entity we're following setup its bones.
	m_pFollow->SetupBones( NULL, -1, m_nFollowBoneSetupMask, gpGlobals->curtime );

	matrix3x4_t matPitchUp;
	AngleMatrix( QAngle( asw_weapon_pitch.GetFloat(), 0, 0 ), matPitchUp );

	// Now copy the bone matrices.
	for ( int i=0; i < m_MergedBones.Count(); i++ )
	{
		int iOwnerBone = m_MergedBones[i].m_iMyBone;
		int iParentBone = m_MergedBones[i].m_iParentBone;

		// Only update bones reference by the bone mask.
		if ( !( m_pOwnerHdr->boneFlags( iOwnerBone ) & boneMask ) )
			continue;

		if ( bOverrideDirection && m_nRightHandBoneID == -1 )		// only want to change direction of the right hand bone, cache its index here
		{
			mstudiobone_t *pOwnerBones = m_pOwnerHdr->pBone( 0 );
			for ( int k = 0; k < m_pOwnerHdr->numbones(); k++ )
			{
				if ( !Q_stricmp( pOwnerBones[k].pszName(), "ValveBiped.Bip01_R_Hand" ) )
				{
					m_nRightHandBoneID = k;
					break;
				}
			}
		}
		
		if ( bOverrideDirection && i == m_nRightHandBoneID )
		{
			matrix3x4_t matParentBoneToWorld;
			m_pFollow->GetBoneTransform( iParentBone, matParentBoneToWorld );
			MatrixSetColumn( vec3_origin, 3, matParentBoneToWorld );		// remove translation

			matrix3x4_t matParentBoneToWorldInv;
			MatrixInvert( matParentBoneToWorld, matParentBoneToWorldInv );

			QAngle angAiming;
			VectorAngles( vecDir, Vector( 0, 0, -1 ), angAiming );
			matrix3x4_t matAimDirection;
			AngleMatrix( angAiming, matAimDirection );
			MatrixSetColumn( vec3_origin, 3, matAimDirection );		// remove translation

			matrix3x4_t matCorrection;
			ConcatTransforms( matParentBoneToWorldInv, matAimDirection, matCorrection );

			ConcatTransforms( m_pFollow->GetBone( iParentBone ), matCorrection, m_pOwner->GetBoneForWrite( iOwnerBone ) );
		}
		else
		{
			ConcatTransforms( m_pFollow->GetBone( iParentBone ), matPitchUp, m_pOwner->GetBoneForWrite( iOwnerBone ) );
		}

		boneComputed.Set( i );
	}
}