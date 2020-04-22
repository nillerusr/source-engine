//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "stdafx.h"
#include "CullTreeNode.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


// dvs: decide how this code should be organized
bool BoxesIntersect(Vector const &mins1, Vector const &maxs1, Vector const &mins2, Vector const &maxs2);


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCullTreeNode::CCullTreeNode(void)
{
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCullTreeNode::~CCullTreeNode(void)
{
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pChild - 
//-----------------------------------------------------------------------------
void CCullTreeNode::AddCullTreeChild(CCullTreeNode *pChild)
{
	if ( pChild == NULL )
		Assert( pChild );
	else
		m_Children.AddToTail(pChild);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pObject - 
//-----------------------------------------------------------------------------
void CCullTreeNode::AddCullTreeObject(CMapClass *pObject)
{
	// First make sure the object isn't already in this node.

	// If it's already here, bail out.
	if ( m_Objects.Find( pObject ) != -1 )
		return;
	
	// Add the object.
	m_Objects.AddToTail(pObject);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pObject - 
//-----------------------------------------------------------------------------
void CCullTreeNode::AddCullTreeObjectRecurse(CMapClass *pObject)
{
	//
	// If the object intersects this node, add it to this node and recurse,
	// testing each of our children in the same fashion.
	//
	Vector ObjMins;
	Vector ObjMaxs;
	pObject->GetCullBox(ObjMins, ObjMaxs);
	if (BoxesIntersect(ObjMins, ObjMaxs, bmins, bmaxs))
	{
		int nChildCount = GetChildCount();
		if (nChildCount != 0)
		{
			// dvs: we should split when appropriate!
			// otherwise the tree becomes less optimal over time.
			for (int nChild = 0; nChild < nChildCount; nChild++)
			{
				CCullTreeNode *pChild = GetCullTreeChild(nChild);
				pChild->AddCullTreeObjectRecurse(pObject);
			}
		}
		else
		{
			AddCullTreeObject(pObject);
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Removes all objects from this node.
//-----------------------------------------------------------------------------
void CCullTreeNode::RemoveAllCullTreeObjects(void)
{
	m_Objects.RemoveAll();
}


//-----------------------------------------------------------------------------
// Purpose: Removes all objects from this branch of the tree recursively.
//-----------------------------------------------------------------------------
void CCullTreeNode::RemoveAllCullTreeObjectsRecurse(void)
{
	RemoveAllCullTreeObjects();

	int nChildCount = GetChildCount();
	for (int nChild = 0; nChild < nChildCount; nChild++)
	{
		CCullTreeNode *pChild = GetCullTreeChild(nChild);
		pChild->RemoveAllCullTreeObjectsRecurse();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Removes all instances of a given object from this node.
// Input  : pObject - 
//-----------------------------------------------------------------------------
void CCullTreeNode::RemoveCullTreeObject(CMapClass *pObject)
{
	// Remove occurrence of pObject from the array
	
	m_Objects.FindAndRemove( pObject );

	// make sure it's not in there twice
	Assert( m_Objects.Find( pObject) == -1 );
}


//-----------------------------------------------------------------------------
// Purpose: Removes all instances of a given object from this node.
// Input  : pObject - 
//-----------------------------------------------------------------------------
void CCullTreeNode::RemoveCullTreeObjectRecurse(CMapClass *pObject)
{
	RemoveCullTreeObject(pObject);

	for (int nChild = 0; nChild < m_Children.Count(); nChild++)
	{
		CCullTreeNode *pChild = m_Children[nChild];
		pChild->RemoveCullTreeObjectRecurse(pObject);
	}	
}


//-----------------------------------------------------------------------------
// Purpose: Removes all instances of a given object from this node.
// Input  : pObject - 
//-----------------------------------------------------------------------------
CCullTreeNode *CCullTreeNode::FindCullTreeObjectRecurse(CMapClass *pObject)
{
	for (int i = 0; i < m_Objects.Count(); i++)
	{
		CMapClass *pCurrent = m_Objects[i];
		if (pCurrent == pObject)
		{
			return(this);
		}
	}

	int nChildCount = GetChildCount();
	for (int nChild = 0; nChild < nChildCount; nChild++)
	{
		CCullTreeNode *pChild = GetCullTreeChild(nChild);
		CCullTreeNode *pFound = pChild->FindCullTreeObjectRecurse(pObject);
		if (pFound != NULL)
		{
			return(pFound);
		}
	}

	return(NULL);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pObject - 
//-----------------------------------------------------------------------------
void CCullTreeNode::UpdateCullTreeObject(CMapClass *pObject)
{
	Vector mins;
	Vector maxs;
	pObject->GetCullBox(mins, maxs);

	if (!BoxesIntersect(mins, maxs, bmins, bmaxs))
	{
		RemoveCullTreeObject(pObject);
	}
	else
	{
		AddCullTreeObject(pObject);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Updates the culling tree due to a change in the bounding box of a
//			given object within the tree. The object is added to any leaf nodes
//			that it now intersects, and is removed from any leaf nodes that it
//			no longer intersects.
// Input  : pObject - The object whose bounding box has changed.
//-----------------------------------------------------------------------------
void CCullTreeNode::UpdateCullTreeObjectRecurse(CMapClass *pObject)
{
	int nChildCount = GetChildCount();
	if (nChildCount != 0)
	{
		for (int nChild = 0; nChild < nChildCount; nChild++)
		{
			CCullTreeNode *pChild = GetCullTreeChild(nChild);
			pChild->UpdateCullTreeObjectRecurse(pObject);
		}
	}
	else
	{
		UpdateCullTreeObject(pObject);
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pObject - The object whose bounding box has changed.
//-----------------------------------------------------------------------------
void CCullTreeNode::UpdateAllCullTreeObjectsRecurse(void)
{
	int nChildCount = GetChildCount();
	if (nChildCount != 0)
	{
		for (int nChild = 0; nChild < nChildCount; nChild++)
		{
			CCullTreeNode *pChild = GetCullTreeChild(nChild);
			pChild->UpdateAllCullTreeObjectsRecurse();
		}
	}
	else
	{
		int nObjectCount = GetObjectCount();
		for (int nObject = 0; nObject < nObjectCount; nObject++)
		{
			CMapClass *pObject = GetCullTreeObject(nObject);

			Vector mins;
			Vector maxs;
			pObject->GetCullBox(mins, maxs);
			if (!BoxesIntersect(mins, maxs, bmins, bmaxs))
			{
				RemoveCullTreeObject(pObject);
			}
		}
	}
}
