//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//

#pragma once

#include "BoundBox.h"
#include "MapClass.h"

class CCullTreeNode;

class CCullTreeNode : public BoundBox
{
	public:

		CCullTreeNode(void);
		~CCullTreeNode(void);

		//
		// Children.
		//
		inline int GetChildCount(void) { return(m_Children.Count()); }
		inline CCullTreeNode *GetCullTreeChild(int nChild) { return(m_Children[nChild]); }

		void AddCullTreeChild(CCullTreeNode *pChild);
		
		//
		// Objects.
		//
		inline int GetObjectCount(void) { return(m_Objects.Count()); }
		inline CMapClass *GetCullTreeObject(int nObject) { return(m_Objects[nObject]); }

		void AddCullTreeObject(CMapClass *pObject);
		void AddCullTreeObjectRecurse(CMapClass *pObject);

		CCullTreeNode *FindCullTreeObjectRecurse(CMapClass *pObject);

		void RemoveAllCullTreeObjects(void);
		void RemoveAllCullTreeObjectsRecurse(void);

		void RemoveCullTreeObject(CMapClass *pObject);
		void RemoveCullTreeObjectRecurse(CMapClass *pObject);

		void UpdateAllCullTreeObjects(void);
		void UpdateAllCullTreeObjectsRecurse(void);

		void UpdateCullTreeObject(CMapClass *pObject);
		void UpdateCullTreeObjectRecurse(CMapClass *pObject);

	protected:

		CUtlVector<CCullTreeNode*> m_Children;	// The child nodes. This is an octree.
		CMapObjectList m_Objects;		// The objects contained in this node.
};

