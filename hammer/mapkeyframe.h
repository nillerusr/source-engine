//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef MAPKEYFRAME_H
#define MAPKEYFRAME_H
#pragma once


#include "MapHelper.h"
#include "hammer_mathlib.h"
#include "keyframe/keyframe.h"


class CMapAnimator;
class CHelperInfo;


class CMapKeyFrame : public CMapHelper
{
public:

	DECLARE_MAPCLASS(CMapKeyFrame,CMapHelper);

	//
	// Factories.
	//
	static CMapClass *CreateMapKeyFrame(CHelperInfo *pHelperInfo, CMapEntity *pParent);

	//
	// Construction/destruction.
	//
	CMapKeyFrame(void);
	~CMapKeyFrame(void);

	void CalcBounds(BOOL bFullUpdate = FALSE);

	virtual size_t GetSize( void ) { return sizeof(*this); }
	void Render3D(CRender3D *pRender);
	virtual CMapClass *Copy(bool bUpdateDependencies);
	virtual CMapClass *CopyFrom(CMapClass *pFrom, bool bUpdateDependencies);
	virtual void SetOrigin( Vector& pfOrigin );
	virtual void GetQuatAngles( Quaternion &outQuat );

	virtual void OnClone( CMapClass *pClone, CMapWorld *pWorld, const CMapObjectList &OriginalList, CMapObjectList &NewList );
	virtual void OnNotifyDependent(CMapClass *pObject, Notify_Dependent_t eNotifyType);
	virtual void OnParentKeyChanged( const char* key, const char* value );
	virtual void OnRemoveFromWorld( CMapWorld *pWorld, bool bNotifyChildren );

	virtual void UpdateDependencies(CMapWorld *pWorld, CMapClass *pObject);

	bool IsAnyKeyInSequenceSelected( void );

	CMapKeyFrame *NextKeyFrame( void );

	void SetAnimator(CMapAnimator *pAnimator);
	virtual CMapAnimator *GetAnimator(void) { return m_pAnimator; }

	CMapEntity *GetParentEntity( void );

	float GetRemainingTime( CMapObjectList *pVisited = NULL );
	float MoveTime( void ) { return m_flMoveTime; }

	IPositionInterpolator*	SetupPositionInterpolator( int iInterpolator );

protected:

	void SetNextKeyFrame( CMapKeyFrame *pNext );
	void RecalculateTimeFromSpeed( void );
	void CheckForKeyFrameNextKeyLoops( void );
	void BuildPathSegment( CMapKeyFrame *pPrev );

	IPositionInterpolator		*m_pPositionInterpolator;
	int							m_iPositionInterpolator;
	int							m_iChangeFrame;

	Quaternion m_qAngles;
	QAngle m_Angles;
	float m_flMoveTime;				// time it takes to travel to next key frame
	float m_flSpeed;				// average speed travelling to next time; if this is non-zero, m_flMoveTime is calculated from it
	CMapAnimator *m_pAnimator;		// The animator that is at the head of our path.
	CMapKeyFrame *m_pNextKeyFrame;	// The next keyframe in our path.

	enum
	{
		MAX_LINE_POINTS = 10,
	};

	Vector m_LinePoints[MAX_LINE_POINTS];
	bool m_bRebuildPath;
	friend CMapAnimator;
};


#endif // MAPKEYFRAME_H
