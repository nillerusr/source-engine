//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef BONE_MERGE_CACHE_H
#define BONE_MERGE_CACHE_H
#ifdef _WIN32
#pragma once
#endif


class C_BaseAnimating;
class CStudioHdr;
class CBoneBitList;

#include "mathlib/vector.h"


class CBoneMergeCache
{
public:

	CBoneMergeCache();
	
	void Init( C_BaseAnimating *pOwner );

	// Updates the lookups that let it merge bones quickly.
	void UpdateCache();
	
	// This copies the transform from all bones in the followed entity that have 
	// names that match our bones.
	virtual void MergeMatchingBones( int boneMask, CBoneBitList &boneComputed );

	// Returns true if the specified bone is one that gets merged in MergeMatchingBones.
	int IsBoneMerged( int iBone ) const;

	// Gets the origin for the first merge bone on the parent.
	bool GetAimEntOrigin( Vector *pAbsOrigin, QAngle *pAbsAngles );


protected:

	// This is the entity that we're keeping the cache updated for.
	C_BaseAnimating *m_pOwner;

	// All the cache data is based off these. When they change, the cache data is regenerated.
	// These are either all valid pointers or all NULL.
	C_BaseAnimating *m_pFollow;
	CStudioHdr		*m_pFollowHdr;
	CStudioHdr		*m_pOwnerHdr;

	// This is the mask we need to use to set up bones on the followed entity to do the bone merge
	int				m_nFollowBoneSetupMask;

	// Cache data.
	class CMergedBone
	{
	public:
		unsigned short m_iMyBone;
		unsigned short m_iParentBone;
	};

	CUtlVector<CMergedBone> m_MergedBones;
	CVarBitVec m_BoneMergeBits;	// One bit for each bone. The bit is set if the bone gets merged.
};


inline int CBoneMergeCache::IsBoneMerged( int iBone ) const
{
	if ( m_pOwnerHdr )
		return m_BoneMergeBits.Get( iBone );
	else
		return 0;
}


#endif // BONE_MERGE_CACHE_H
