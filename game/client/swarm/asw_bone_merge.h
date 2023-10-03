#ifndef _INCLUDED_ASW_BONE_MERGE_H
#define _INCLUDED_ASW_BONE_MERGE_H
#ifdef _WIN32
#pragma once
#endif

#include "bone_merge_cache.h"

class CASW_Bone_Merge_Cache : public CBoneMergeCache
{
public:
	typedef CBoneMergeCache BaseClass;

	CASW_Bone_Merge_Cache();

	// This copies the transform from all bones in the followed entity that have 
	// names that match our bones.
	virtual void MergeMatchingBones( int boneMask, CBoneBitList &boneComputed, bool bOverrideDirection, const Vector &vecDir );

	int m_nRightHandBoneID;
};

#endif //_INCLUDED_ASW_BONE_MERGE_H