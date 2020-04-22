//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef UPDATEHINT_H
#define UPDATEHINT_H
#ifdef _WIN32
#pragma once
#endif



class CUpdateHint : public CObject
{
	#define MAX_NOTIFY_CODES 16

	struct NotifyList_t
	{
		int nCode;
		CMapObjectList Objects;
	};

	public:

		CUpdateHint(void) {}

		//
		// Called by the code that modifies map objects:
		//
		inline void PreUpdateObject(CMapClass *pObject);
		inline void PreUpdateObjects(CMapObjectList *pObjects);

		inline void PostUpdateObject(CMapClass *pObject, int nNotifyCode);
		inline void PostUpdateObjects(CMapObjectList *pObjects, int nNotifyCode);

		inline BoundBox const &GetUpdateRegion(void);
		inline void Reset(void);
		inline void UpdateBounds(BoundBox &bbox);

		//
		// Called by the document when processing an update:
		//
		inline int GetNotifyCodeCount(void);
		inline int GetNotifyCode(int nIndex);

		inline POSITION GetHeadPosition(int nIndex);
		inline CMapClass *GetNext(int nIndex, POSITION &pos);

	protected:

		NotifyList_t m_NotifyList[MAX_NOTIFY_CODES];	// Lists of objects with common notification codes.
		int m_nListEntries;								// Number of items in the notify list.

		BoundBox m_UpdateRegion;						// 3D map extents that were affected by the change.
};


#endif // UPDATEHINT_H
