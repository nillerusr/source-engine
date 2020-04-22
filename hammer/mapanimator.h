//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef MAPANIMATOR_H
#define MAPANIMATOR_H
#pragma once


#include "MapClass.h"
#include "MapKeyFrame.h"


class CMapAnimator : public CMapKeyFrame
{
public:

	DECLARE_MAPCLASS( CMapAnimator, CMapKeyFrame );

	//
	// Factories.
	//
	static CMapClass *CreateMapAnimator(CHelperInfo *pHelperInfo, CMapEntity *pParent);

	//
	// Construction/destruction.
	//
	CMapAnimator();
	~CMapAnimator();

	virtual size_t GetSize( void ) { return sizeof(*this); }
	virtual CMapClass *Copy(bool bUpdateDependencies);
	virtual CMapClass *CopyFrom(CMapClass *pFrom, bool bUpdateDependencies);

	virtual void UpdateAnimation( float animTime );
	virtual void OnClone( CMapClass *pClone, CMapWorld *pWorld, const CMapObjectList &OriginalList, CMapObjectList &NewList );
	virtual bool GetTransformMatrix( VMatrix& matrix );
	virtual void OnParentKeyChanged( const char* key, const char* value );
	virtual CMapAnimator *GetAnimator(void) { return this; }
	void RebuildPath( void );

	float GetKeyFramesAtTime( float time, CMapKeyFrame *&pKeyFrame, CMapKeyFrame *&pPrevKeyFrame );
	CMapEntity *CreateNewKeyFrame( float time );
	void GetAnimationAtTime( float animTime, Vector& newOrigin, Quaternion &newAngles );
	static void GetAnimationAtTime( CMapKeyFrame *currentKey, CMapKeyFrame *pPrevKey, float animTime, Vector& newOrigin, Quaternion &newAngles, int posInterpolator, int rotInterpolator );

	int GetNumKeysChanged() {return m_nKeysChanged;}

private:
	// Used by the keys to detect when changes need to be sent to the position interpolators.
	int			m_nKeysChanged;
	
	VMatrix m_CoordFrame;
	bool m_bCurrentlyAnimating;
	CMapKeyFrame *m_pCurrentKeyFrame;	// keyframe it's currently at

	int m_iTimeModifier;
	int m_iPositionInterpolator;
	int m_iRotationInterpolator;

	friend CMapKeyFrame;
};


#endif // MAPANIMATOR_H
