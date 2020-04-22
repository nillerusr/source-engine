//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Implements a helper that manages a single keyvalue of type "side"
//			or "sidelist" for the entity that is its parent.
//
//=============================================================================//

#include "stdafx.h"
#include "fgdlib/HelperInfo.h"
#include "materialsystem/imesh.h"
#include "MapClass.h"
#include "MapSolid.h"
#include "MapWorld.h"			// For the world's face ID functions.
#include "MapSideList.h"
#include "Material.h"
#include "Render3D.h"
#include "mapdoc.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

IMPLEMENT_MAPCLASS(CMapSideList);


//-----------------------------------------------------------------------------
// Purpose: Factory function. Used for creating a CMapSideList from a set
//			of string parameters from the FGD file.
// Input  : pInfo - Pointer to helper info class which gives us information
//				about how to create the class.
// Output : Returns a pointer to the class, NULL if an error occurs.
//-----------------------------------------------------------------------------
CMapClass *CMapSideList::CreateMapSideList(CHelperInfo *pHelperInfo, CMapEntity *pParent)
{
	CMapSideList *pSideList = NULL;

	const char *pszParam = pHelperInfo->GetParameter(0);
	if (pszParam != NULL)
	{
		pSideList = new CMapSideList(pszParam);
	}
	else
	{
		pSideList = new CMapSideList("sides");
	}

	return(pSideList);
}


//-----------------------------------------------------------------------------
// Purpose: Constructor.
//-----------------------------------------------------------------------------
CMapSideList::CMapSideList(void)
{
	m_szKeyName[0] = '\0';
}


//-----------------------------------------------------------------------------
// Purpose: Constructor with key name.
//-----------------------------------------------------------------------------
CMapSideList::CMapSideList(char const *pszKeyName)
{
	strcpy( m_szKeyName, pszKeyName );
}


//-----------------------------------------------------------------------------
// Purpose: Destructor.
//-----------------------------------------------------------------------------
CMapSideList::~CMapSideList(void)
{
}


//-----------------------------------------------------------------------------
// Gets the keyvalue from our parent entity and rebuilds the face list from that.
//-----------------------------------------------------------------------------
void CMapSideList::RebuildFaceList()
{
	CMapWorld *pWorld = GetWorldObject(this);
	if (pWorld == NULL)
	{
		return;
	}

	CMapEntity *pParent = dynamic_cast <CMapEntity *>(GetParent());
	const char *pszValue = pParent->GetKeyValue(m_szKeyName);
	if (pszValue != NULL)
	{
		BuildFaceListForValue(pszValue, pWorld);
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pszValue - 
//			pWorld - The world object that we are contained in.
//-----------------------------------------------------------------------------
void CMapSideList::BuildFaceListForValue(char const *pszValue, CMapWorld *pWorld)
{
	CMapFaceList NewFaces;
	pWorld->FaceID_StringToFaceLists(&NewFaces, NULL, pszValue);

	//
	// Detach from the faces that are not in the new list. Go
	// in reverse order since we are removing items as we go.
	//
	if (m_Faces.Count() > 0)
	{
		for (int i = m_Faces.Count() - 1; i >= 0; i--)
		{
			CMapFace *pFace = m_Faces.Element(i);
			Assert(pFace != NULL);
			if ((pFace != NULL) && (NewFaces.Find(pFace) == -1))
			{
				CMapSolid *pSolid = (CMapSolid *)pFace->GetParent();
				UpdateDependency(pSolid, NULL);
				m_Faces.FastRemove(i);
			}
		}
	}

	//
	// Attach to the faces that are not in the old list.
	//
	for (int i = 0; i < NewFaces.Count(); i++)
	{
		CMapFace *pFace = NewFaces.Element(i);
		Assert(pFace != NULL);

		if ((pFace != NULL) && (m_Faces.Find(pFace) == -1))
		{
			CMapSolid *pSolid = (CMapSolid *)pFace->GetParent();
			UpdateDependency(NULL, pSolid);
			m_Faces.AddToTail(pFace);
		}
	}

	CalcBounds();
}


//-----------------------------------------------------------------------------
// Purpose: Calculates our bounds.
// Input  : bFullUpdate - 
//-----------------------------------------------------------------------------
void CMapSideList::CalcBounds(BOOL bFullUpdate)
{
	//
	// We're just a point in the 2D view because we don't render there.
	//
	m_Render2DBox.ResetBounds();
	m_Render2DBox.UpdateBounds(m_Origin);

	//
	// Our culling bounds includes the endpoints of all the lines we draw when
	// our parent entity is selected.
	//
	m_CullBox.ResetBounds();
	m_CullBox.UpdateBounds(m_Origin);

	for (int i = 0; i < m_Faces.Count(); i++)
	{
		CMapFace *pFace = m_Faces.Element(i);

		Vector Center;
		pFace->GetCenter(Center);
		m_CullBox.UpdateBounds(Center);
	}
	m_BoundingBox = m_CullBox;
}


//-----------------------------------------------------------------------------
// Purpose: Returns a copy of this object.
// Input  : bUpdateDependencies - Whether the new object should link to any
//				other objects in the world when it copies pointers.
//-----------------------------------------------------------------------------
CMapClass *CMapSideList::Copy(bool bUpdateDependencies)
{
	CMapSideList *pCopy = new CMapSideList;
	if (pCopy != NULL)
	{
		pCopy->CopyFrom(this, bUpdateDependencies);
	}
	return(pCopy);
}


//-----------------------------------------------------------------------------
// Purpose: Turns us into an exact copy of the given object.
// Input  : pFrom - Object to copy.
// Input  : bUpdateDependencies - Whether we should link to any other objects
//				in the world when we copy pointers.
//-----------------------------------------------------------------------------
CMapClass *CMapSideList::CopyFrom(CMapClass *pOther, bool bUpdateDependencies)
{
	CMapSideList *pFrom = dynamic_cast <CMapSideList *>(pOther);
	Assert(pFrom != NULL);

	CMapClass::CopyFrom(pOther, bUpdateDependencies);

	strcpy(m_szKeyName, pFrom->m_szKeyName);
	m_Faces = pFrom->m_Faces;

	if (bUpdateDependencies)
	{
		for (int i = 0; i < m_Faces.Count(); i++)
		{
			CMapFace *pFace = m_Faces.Element(i);
			CMapSolid *pSolid = (CMapSolid *)pFace->GetParent();
			UpdateDependency(pSolid, NULL);
		}
	}

	return(this);
}


//-----------------------------------------------------------------------------
// Purpose: Searches for a face with the given unique ID in the list of objects.
//			FIXME: should this be in CMapObjectList?
// Input  : nFaceID - Face ID to search for.
//			List - List of objects to search.
// Output : Returns the face with the given ID if it was found, NULL if not.
//-----------------------------------------------------------------------------
CMapFace *CMapSideList::FindFaceIDInList(int nFaceID, const CMapObjectList &List)
{
	FOR_EACH_OBJ( List, pos )
	{
		//
		// If this object is a solid, look for the face there.
		//
		CMapClass *pObject = List.Element(pos);
		CMapSolid *pSolid = dynamic_cast <CMapSolid *>(pObject);
		if (pSolid != NULL)
		{
			CMapFace *pFace = pSolid->FindFaceID(nFaceID);
			if (pFace != NULL)
			{
				return(pFace);
			}
		}

		//
		// Check all of this object's solid children.
		//
		EnumChildrenPos_t pos2;
		CMapClass *pChild = pObject->GetFirstDescendent(pos2);
		while (pChild != NULL)
		{
			pSolid = dynamic_cast <CMapSolid *>(pChild);
			if (pSolid != NULL)
			{
				CMapFace *pFace = pSolid->FindFaceID(nFaceID);
				if (pFace != NULL)
				{
					return(pFace);
				}
			}	

			pChild = pObject->GetNextDescendent(pos2);
		}
	}

	return(NULL);
}


//-----------------------------------------------------------------------------
// Purpose: Returns the total approximate memory consumed by this object.
//-----------------------------------------------------------------------------
size_t CMapSideList::GetSize(void)
{
	return(sizeof(this) + m_Faces.Count() * sizeof(CMapFace *));
}


//-----------------------------------------------------------------------------
// Purpose: Notification that we have just been cloned. Fix up our clone's
//			face list based on the the objects that were cloned with it.
// Input  : pClone - 
//			OriginalList - 
//			NewList - 
//-----------------------------------------------------------------------------
void CMapSideList::OnClone(CMapClass *pCloneObject, CMapWorld *pWorld, const CMapObjectList &OriginalList, CMapObjectList &NewList)
{
	ReplaceFacesInCopy((CMapSideList *)pCloneObject, OriginalList, NewList);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pCopy - 
//			pSourceWorld - 
//			pDestWorld - 
//			OriginalList - 
//			NewList - 
//-----------------------------------------------------------------------------
void CMapSideList::OnPaste(CMapClass *pCopyObject, CMapWorld *pSourceWorld, CMapWorld *pDestWorld, const CMapObjectList &OriginalList, CMapObjectList &NewList)
{
	CMapSideList *pCopy = (CMapSideList *)pCopyObject;

	//
	// HACK: This is kinda nasty. Build a list of face IDs from our parent keyvalue so
	// we can remap them to faces in the clipboard.
	//
	CMapEntity *pParent = dynamic_cast <CMapEntity *>(pCopy->GetParent());
	const char *pszValue = pParent->GetKeyValue(m_szKeyName);
	if (pszValue != NULL)
	{
		char szVal[KEYVALUE_MAX_VALUE_LENGTH];
		strcpy(szVal, pszValue);

		char *psz = strtok(szVal, " ");
		while (psz != NULL)
		{
			//
			// The substring should now be a single face ID. Get the corresponding
			// face and add it to the list.
			//
			int nFaceID = atoi(psz);
			CMapFace *pFace = FindFaceIDInList(nFaceID, OriginalList);
			if (pFace != NULL)
			{
				pCopy->m_Faces.AddToTail(pFace);
			}

			//
			// Get the next substring.
			//
			psz = strtok(NULL, " ");
		}
	}

	//
	// Now replace all faces with their new counterparts, as in Clone.
	//
	ReplaceFacesInCopy(pCopy, OriginalList, NewList);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pObject - 
//			eNotifyType - 
//-----------------------------------------------------------------------------
void CMapSideList::OnNotifyDependent(CMapClass *pObject, Notify_Dependent_t eNotifyType)
{
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if ( pDoc->IsLoading() )
		return;

	if ( eNotifyType == Notify_Changed )
	{
		// Remove? Purge?
		m_Faces.RemoveAll();
		
		// Rebuild the face list 
		RebuildFaceList();
	}

	if (eNotifyType == Notify_Removed || eNotifyType == Notify_Clipped)
	{
		//
		// Check for a solid that we refer to via face ID going away.
		//
		CMapSolid *pSolid = dynamic_cast<CMapSolid *>(pObject);
		if ((pSolid != NULL) && (m_Faces.Count() > 0))
		{
			//
			// Remove faces from our list that are in this solid.
			// Do it backwards so we can remove them as we go. Also, add
			// the face IDs to our list of lost IDs so that we can reacquire
			// the face in our list if the solid comes back later.
			//
			for (int i = m_Faces.Count() - 1; i >= 0; i--)
			{
				CMapFace *pFace = m_Faces.Element(i);
				if (pFace != NULL)
				{
					CMapSolid *pParent = (CMapSolid *)pFace->GetParent();
					if (pParent == pSolid)
					{
						m_LostFaceIDs.AddToTail(pFace->GetFaceID());
						m_Faces.FastRemove(i);
					}
				}
			}
		
			//
			// Submit the updated face list to our parent entity.
			//
			UpdateParentKey();
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : key - 
//			value - 
//-----------------------------------------------------------------------------
void CMapSideList::OnParentKeyChanged(char const *pszKey, char const *pszValue)
{
	CMapWorld *pWorld = GetWorldObject(this);
	if (pWorld == NULL)
	{
		// We're probably being copied into the clipboard.
		return;
	}

	//
	// Update our face list if the key we care about is changing.
	//
	if (!stricmp(pszKey, m_szKeyName))
	{
		BuildFaceListForValue(pszValue, pWorld);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Called just after this object has been removed from the world so
//			that it can unlink itself from other objects in the world.
// Input  : pWorld - The world that we were just removed from.
//			bNotifyChildren - Whether we should forward notification to our children.
//-----------------------------------------------------------------------------
void CMapSideList::OnRemoveFromWorld(CMapWorld *pWorld, bool bNotifyChildren)
{
	CMapClass::OnRemoveFromWorld(pWorld, bNotifyChildren);

	for (int i = 0; i < m_Faces.Count(); i++)
	{
		CMapFace *pFace = m_Faces.Element(i);
		CMapSolid *pSolid = (CMapSolid *)pFace->GetParent();
		UpdateDependency(pSolid, NULL);
	}
	
	m_Faces.RemoveAll();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : List - 
//-----------------------------------------------------------------------------
void CMapSideList::RemoveFacesNotInList(const CMapObjectList &List)
{
	if (m_Faces.Count() > 0)
	{
		for (int i = m_Faces.Count() - 1; i >= 0; i--)
		{
			CMapFace *pFace = m_Faces.Element(i);

			if (FindFaceIDInList(pFace->GetFaceID(), List) == NULL)
			{
				CMapSolid *pSolid = (CMapSolid *)pFace->GetParent();
				UpdateDependency(pSolid, NULL);
				m_Faces.FastRemove(i);
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Called from OnClone and OnPaste. Replaces references (in the
//			cloned object) to faces in the original list of objects with references
//			to corresponding faces in the new list of objects.
// Input  : pClone - 
//			OriginalList - 
//			NewList - 
//-----------------------------------------------------------------------------
void CMapSideList::ReplaceFacesInCopy(CMapSideList *pCopy, const CMapObjectList &OriginalList, CMapObjectList &NewList)
{
	Assert( OriginalList.Count() == NewList.Count() );
	
	FOR_EACH_OBJ( OriginalList, pos )
	{
		CMapClass *pOriginal = OriginalList.Element(pos);
		CMapClass *pNew = NewList.Element(pos);

		if (pOriginal != this)
		{
			//
			// Check to see if these two objects are solids.
			//
			CMapSolid *pOrigSolid = dynamic_cast <CMapSolid *>(pOriginal);
			if (pOrigSolid != NULL)
			{
				CMapSolid *pNewSolid = dynamic_cast <CMapSolid *>(pNew);
				Assert(pNewSolid != NULL);

				if (pNewSolid != NULL)
				{
					pCopy->ReplaceSolidFaces(pOrigSolid, pNewSolid);
				}
			}

			//
			// Check all of these objects' children.
			//
			EnumChildrenPos_t e1;
			EnumChildrenPos_t e2;

			CMapClass *pOrigChild = pOriginal->GetFirstDescendent(e1);
			CMapClass *pNewChild = pNew->GetFirstDescendent(e2);

			while (pOrigChild != NULL)
			{
				Assert(pNewChild != NULL);

				pOrigSolid = dynamic_cast <CMapSolid *>(pOrigChild);
				if (pOrigSolid != NULL)
				{
					CMapSolid *pNewSolid = dynamic_cast <CMapSolid *>(pNewChild);
					Assert(pNewSolid != NULL);

					if (pNewSolid != NULL)
					{
						pCopy->ReplaceSolidFaces(pOrigSolid, pNewSolid);
					}
				}

				pOrigChild = pOriginal->GetNextDescendent(e1);
				pNewChild = pNew->GetNextDescendent(e2);
			}
		}
	}
	
	//
	// Update the keyvalue in the copy's parent entity.
	//
	pCopy->UpdateParentKey();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pRender - Interface to use for rendering.
//-----------------------------------------------------------------------------
void CMapSideList::Render2D(CRender2D *pRender)
{
}


//-----------------------------------------------------------------------------
// Purpose: Renders us in the 3D view.
// Input  : pRender - Interface to use for rendering.
//-----------------------------------------------------------------------------
void CMapSideList::Render3D(CRender3D *pRender)
{
	if ( !m_pParent->IsSelected() )
		return;
	
	//
	// Draw lines from us to the center of all faces in the list.
	//
	pRender->PushRenderMode(RENDER_MODE_WIREFRAME);

	CMeshBuilder meshBuilder;
	CMatRenderContextPtr pRenderContext( MaterialSystemInterface() );
	IMesh* pMesh = pRenderContext->GetDynamicMesh();

	meshBuilder.Begin(pMesh, MATERIAL_LINES, m_Faces.Count());

	for (int i = 0; i < m_Faces.Count(); i++)
	{
		CMapFace *pFace = m_Faces.Element(i);

		Vector Center;
		pFace->GetCenter(Center);

		unsigned char color[3];
		color[0] = SELECT_EDGE_RED; 
		color[1] = SELECT_EDGE_GREEN;
		color[2] = SELECT_EDGE_BLUE;

		meshBuilder.Color3ubv( color );
		meshBuilder.Position3f(m_Origin.x, m_Origin.y, m_Origin.z);
		meshBuilder.AdvanceVertex();

		meshBuilder.Color3ubv( color );
		meshBuilder.Position3f(Center.x, Center.y, Center.z);
		meshBuilder.AdvanceVertex();
	}

	meshBuilder.End();
	pMesh->Draw();

	pRender->PopRenderMode();
}


//-----------------------------------------------------------------------------
// Purpose: Called from OnClone and OnPaste, updates references to face IDs 
//			in the one solid with references to corresponding face IDs in
//			another solid.
// Input  : pOrigSolid - Solid with faces to find.
//			pNewSolid - Solid with faces to replace with.
// Output : Returns true if it replaced at least one face.
//-----------------------------------------------------------------------------
bool CMapSideList::ReplaceSolidFaces(CMapSolid *pOrigSolid, CMapSolid *pNewSolid)
{
	bool bDidSomething = false;
	for (int i = 0; i < pOrigSolid->GetFaceCount(); i++)
	{
		CMapFace *pFace = pOrigSolid->GetFace(i);

		int nIndex = m_Faces.FindFaceID(pFace->GetFaceID());
		if (nIndex != -1)
		{
			//
			// Replace the element in our face list and unlink
			// us from the original solid, relinking us to the new solid.
			//
			m_Faces.Element(nIndex) = pNewSolid->GetFace(i);
			UpdateDependency(pOrigSolid, pNewSolid);
			bDidSomething = true;
		}
	}

	return(bDidSomething);
}


//-----------------------------------------------------------------------------
// Purpose: Something happened in the world that requires us to refresh our
//			dependencies. Try to reacquire face IDs in our deleted faces list.
// Input  : pWorld - 
//-----------------------------------------------------------------------------
void CMapSideList::UpdateDependencies(CMapWorld *pWorld, CMapClass *pObject)
{
	CMapClass::UpdateDependencies(pWorld, pObject);

	//
	// See if it is a solid that holds faces in our lost faces list.
	//
	CMapSolid *pSolid = dynamic_cast <CMapSolid *>(pObject);
	if ((pSolid != NULL) && (m_LostFaceIDs.Count() > 0))
	{
		//
		// Walk the list backwards so we can remove as we go.
		//
		for (int i = m_LostFaceIDs.Count() - 1; i >= 0; i--)
		{
			int nFaceID = m_LostFaceIDs.Element(i);

			CMapFace *pFace = pSolid->FindFaceID(nFaceID);
			if (pFace != NULL)
			{
				if (m_Faces.Find(pFace) == -1)
				{
					m_Faces.AddToTail(pFace);
				}

				//
				// We've reacquired the face, so it's no longer lost.
				//
				m_LostFaceIDs.FastRemove(i);
			}
		}

		UpdateParentKey();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Builds a new value string for our parent's facelist key from our
//			current list of faces and submits the keyvalue to our parent for
//			storage.
//-----------------------------------------------------------------------------
void CMapSideList::UpdateParentKey(void)
{
	char szValue[KEYVALUE_MAX_VALUE_LENGTH];
	CMapWorld::FaceID_FaceListsToString(szValue, sizeof(szValue), &m_Faces, NULL);

	CMapEntity *pEntity = dynamic_cast<CMapEntity *>(m_pParent);
	if (pEntity != NULL)
	{
		pEntity->NotifyChildKeyChanged(this, m_szKeyName, szValue);
	}
}
