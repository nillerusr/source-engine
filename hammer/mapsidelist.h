//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Implements a helper that manages a single keyvalue of type "side"
//			or "sidelist" for the entity that is its parent.
//
// $NoKeywords: $
//=============================================================================//

#ifndef MAPSIDELIST_H
#define MAPSIDELIST_H
#ifdef _WIN32
#pragma once
#endif


#include "MapFace.h"
#include "MapHelper.h"


class CHelperInfo;
class CMapEntity;
class CMapSolid;


class CMapSideList : public CMapHelper
{
	public:

		//
		// Factory function.
		//
		static CMapClass *CreateMapSideList(CHelperInfo *pHelperInfo, CMapEntity *pParent);

		//
		// Construction/destruction:
		//
		CMapSideList(void);
		CMapSideList(char const *pszKeyName);
		virtual ~CMapSideList(void);
		DECLARE_MAPCLASS(CMapSideList,CMapHelper)

	public:

		virtual size_t GetSize(void);

		//
		// Replication.
		//
		virtual CMapClass *Copy(bool bUpdateDependencies);
		virtual CMapClass *CopyFrom(CMapClass *pOther, bool bUpdateDependencies);

		//
		// Should NOT have children.
		//
		virtual void AddChild(CMapClass *pChild) { Assert(false); } 
		virtual void UpdateChild(CMapClass *pChild) { Assert(false); }

		//
		// Notifications.
		//
		virtual void OnClone(CMapClass *pClone, CMapWorld *pWorld, const CMapObjectList &OriginalList, CMapObjectList &NewList);
		virtual void OnNotifyDependent(CMapClass *pObject, Notify_Dependent_t eNotifyType);
		virtual void OnParentKeyChanged(const char* key, const char* value);
		virtual void OnPaste(CMapClass *pCopy, CMapWorld *pSourceWorld, CMapWorld *pDestWorld, const CMapObjectList &OriginalList, CMapObjectList &NewList);
		virtual void OnRemoveFromWorld(CMapWorld *pWorld, bool bNotifyChildren);

		//
		// Spatial functions.
		//
		virtual void CalcBounds(BOOL bFullUpdate = FALSE);

		virtual const char* GetDescription(void) { return "Side list helper"; }

		//
		// Serialization.
		//
		virtual bool ShouldSerialize(void) { return(false); }

		//
		// Can be rendered:
		//
		virtual void Render2D(CRender2D *pRender);
		virtual void Render3D(CRender3D *pRender);

		// 
		// Getting face data.
		//
		int GetFaceCount( void )			{ return m_Faces.Count(); }
		CMapFace *GetFace( int index )		{ return m_Faces.Element( index ); }

		const char *GetKeyName( void )		{ return m_szKeyName; }

	protected:

		virtual void UpdateDependencies(CMapWorld *pWorld, CMapClass *pObject);

		void RebuildFaceList();
		void BuildFaceListForValue(char const *pszValue, CMapWorld *pWorld);
		CMapFace *FindFaceIDInList(int nFaceID, const CMapObjectList &List);
		void ReplaceFacesInCopy(CMapSideList *pCopy, const CMapObjectList &OriginalList, CMapObjectList &NewList);
		bool ReplaceSolidFaces(CMapSolid *pOrigSolid, CMapSolid *pNewSolid);
		void RemoveFacesNotInList(const CMapObjectList &List);
		void UpdateParentKey(void);

		char m_szKeyName[80];			// The name of the key in our parent entity that we represent.
		CMapFaceList m_Faces;			// The list of solid faces that this object manages.
		CUtlVector<int> m_LostFaceIDs;	// The list of face IDs that were in m_Faces, but were lost during this session through the deletion of their solid.
};


#endif // MAPSIDELIST_H
