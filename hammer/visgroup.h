//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef VISGROUP_H
#define VISGROUP_H
#pragma once


#include "BaseTypes.h"
#include "utlvector.h"


class CChunkFile;
class CSaveInfo;
class CMapDoc;

struct LoadVisGroupData_t;


enum ChunkFileResult_t;


enum VisGroupState_t
{
	VISGROUP_UNDEFINED = -1,	// Used for initialization when updating state
	VISGROUP_HIDDEN,			// All members are currently hidden
	VISGROUP_SHOWN,				// All members are currently shown
	VISGROUP_PARTIAL,			// Some members are currently hidden, some are shown
};


class CVisGroup
{
	public:

		CVisGroup(void);

		inline unsigned int GetID(void) { return(m_dwID); }
		inline void SetID(unsigned int dwID) { m_dwID = dwID; }

		inline const char *GetName(void)
		{
			return(m_szName);
		}

		inline void SetName(const char *pszName)
		{
			if (pszName != NULL)
			{
				strncpy(m_szName, pszName, sizeof(m_szName));
			}
		}

		inline color32 GetColor(void);
		inline void SetColor(color32 rgbColor);
		inline void SetColor(unsigned char red, unsigned char green, unsigned char blue);

		inline CVisGroup *GetParent(void);
		inline void SetParent(CVisGroup *pNewParent);

		inline int GetChildCount(void);
		inline CVisGroup *GetChild(int nIndex);

		void AddChild(CVisGroup *pChild);
		void RemoveChild(CVisGroup *pChild);

		bool FindDescendent(CVisGroup *pGroup);

		void MoveUp(CVisGroup *pChild);
		void MoveDown(CVisGroup *pChild);

		bool CanMoveUp(CVisGroup *pChild);
		bool CanMoveDown(CVisGroup *pChild);

		VisGroupState_t GetVisible(void);
		void VisGroups_UpdateParent( VisGroupState_t state );
		inline void SetVisible(VisGroupState_t  eVisible) { m_eVisible = eVisible; }

		static bool IsShowAllActive(void);
		static void ShowAllVisGroups(bool bShow);

		//
		// Serialization.
		//
		ChunkFileResult_t LoadVMF(CChunkFile *pFile, CMapDoc *pDoc);
		ChunkFileResult_t SaveVMF(CChunkFile *pFile, CSaveInfo *pSaveInfo);

		static ChunkFileResult_t LoadVisGroupCallback(CChunkFile *pFile, LoadVisGroupData_t *pLoadData);
		static ChunkFileResult_t LoadVisGroupsCallback(CChunkFile *pFile, CMapDoc *pDoc);

		static bool IsConvertingOldVisGroups();

		bool IsAutoVisGroup(void);
		void SetAuto( bool bAuto );

	protected:

		CUtlVector<CVisGroup *> m_Children;
		CVisGroup *m_pParent;

		bool m_bIsAuto;

		static ChunkFileResult_t LoadKeyCallback(const char *szKey, const char *szValue, CVisGroup *pGroup);

		static bool s_bShowAll;
		static bool s_bIsConvertingOldVisGroups;

		char m_szName[128];
		color32 m_rgbColor;

		unsigned int m_dwID;
		VisGroupState_t m_eVisible;
};


//-----------------------------------------------------------------------------
// Purpose: Returns the render color of this visgroup.
//-----------------------------------------------------------------------------
inline color32 CVisGroup::GetColor(void)
{
	return m_rgbColor;
}


//-----------------------------------------------------------------------------
// Purpose: Sets the color of this visgroup.
//-----------------------------------------------------------------------------
inline void CVisGroup::SetColor(color32 rgbColor)
{
	m_rgbColor = rgbColor;
}


//-----------------------------------------------------------------------------
// Purpose: Sets the color of this visgroup using RGB values.
//-----------------------------------------------------------------------------
inline void CVisGroup::SetColor(unsigned char red, unsigned char green, unsigned char blue)
{
	m_rgbColor.r = red;
	m_rgbColor.g = green;
	m_rgbColor.b = blue;
	m_rgbColor.a = 0;
}


//-----------------------------------------------------------------------------
// Purpose: Returns the number of visgroups that are children of this visgroup.
//-----------------------------------------------------------------------------
inline int CVisGroup::GetChildCount(void)
{
	return m_Children.Count();
}


//-----------------------------------------------------------------------------
// Purpose: Returns the given child visgroup.
//-----------------------------------------------------------------------------
inline CVisGroup *CVisGroup::GetChild(int nIndex)
{
	return m_Children.Element(nIndex);
}


//-----------------------------------------------------------------------------
// Purpose: Returns this visgroup's parent in the hierarchy.
//-----------------------------------------------------------------------------
inline CVisGroup *CVisGroup::GetParent(void)
{
	return m_pParent;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
inline void CVisGroup::SetParent(CVisGroup *pNewParent)
{
	m_pParent = pNewParent;
}


//-----------------------------------------------------------------------------
// A list of visgroups.
//-----------------------------------------------------------------------------
class CVisGroupList
{
public:

	inline int AddToTail(CVisGroup *pVisGroup);
	inline int Count(void);
	inline CVisGroup *Element(int nElement);
	inline int Find(CVisGroup *pVisGroup);
	inline void FastRemove(int nElement);
	inline void RemoveAll(void);

private:

	CUtlVector<CVisGroup *> m_List;
};


int CVisGroupList::AddToTail(CVisGroup *pVisGroup)
{
	return m_List.AddToTail(pVisGroup);
}


int CVisGroupList::Count(void)
{
	return m_List.Count();
}


CVisGroup *CVisGroupList::Element(int nElement)
{
	return m_List.Element(nElement);
}


int CVisGroupList::Find(CVisGroup *pVisGroup)
{
	return m_List.Find(pVisGroup);
}


inline void CVisGroupList::FastRemove(int nElement)
{
	m_List.FastRemove(nElement);
}


inline void CVisGroupList::RemoveAll(void)
{
	m_List.RemoveAll();
}


#endif // VISGROUP_H
