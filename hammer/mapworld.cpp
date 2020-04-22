//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "stdafx.h"
#include "generichash.h"
#include "CullTreeNode.h"
#include "GlobalFunctions.h"
#include "MainFrm.h"
#include "MapDefs.h"
#include "MapDoc.h"		// dvs: think of a way around the world knowing about the doc
#include "MapEntity.h"
#include "MapGroup.h"
#include "MapSolid.h"
#include "MapWorld.h"
#include "SaveInfo.h"
#include "StatusBarIDs.h"
#include "VisGroup.h"
#include "hammer.h"
#include "Worldsize.h"
#include "MapOverlay.h"
#include "Manifest.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


#pragma warning(disable:4244)


class CCullTreeNode;


IMPLEMENT_MAPCLASS(CMapWorld)


struct SaveLists_t
{
	CMapObjectList Solids;
	CMapObjectList Entities;
	CMapObjectList Groups;
};


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pSolid - 
//			*pList - 
// Output : static BOOL
//-----------------------------------------------------------------------------
static BOOL AddUsedTextures(CMapSolid *pSolid, CUsedTextureList *pList)
{
	if (!pSolid->IsVisible())
		return TRUE;

	int nFaces = pSolid->GetFaceCount();
	IEditorTexture *pLastTex = NULL;
	int nLastElement = 0;

	for (int i = 0; i < nFaces; i++)
	{
		CMapFace *pFace = pSolid->GetFace(i);

		UsedTexture_t Tex;
		Tex.pTex = pFace->GetTexture();
		Tex.nUsageCount = 0;

		if (Tex.pTex != NULL)
		{
			if (Tex.pTex != pLastTex)
			{
				int nElement = pList->Find(Tex.pTex);
				if (nElement == -1)
				{
					nElement = pList->AddToTail(Tex);
				}

				nLastElement = nElement;
				pLastTex = Tex.pTex;
			}

			pList->Element(nLastElement).nUsageCount++;
		}
	}

	return TRUE;
}


static BOOL AddOverlayTextures(CMapOverlay *pOverlay, CUsedTextureList *pList)
{
	if (!pOverlay->IsVisible())
		return TRUE;

	UsedTexture_t Tex;
	Tex.pTex = pOverlay->GetMaterial();
	Tex.nUsageCount = 0;

	if (Tex.pTex != NULL)
	{
		int nElement = pList->Find(Tex.pTex);
		if (nElement == -1)
			nElement = pList->AddToTail(Tex);

		pList->Element(nElement).nUsageCount++;
	}
		
	return TRUE;
}


//-----------------------------------------------------------------------------
// Purpose: Returns whether the two boxes intersect.
// Input  : mins1 - 
//			maxs1 - 
//			mins2 - 
//			maxs2 - 
//-----------------------------------------------------------------------------
bool BoxesIntersect(Vector const &mins1, Vector const &maxs1, Vector const &mins2, Vector const &maxs2)
{
	if ((maxs1[0] < mins2[0]) || (mins1[0] > maxs2[0]) ||
		(maxs1[1] < mins2[1]) || (mins1[1] > maxs2[1]) ||
		(maxs1[2] < mins2[2]) || (mins1[2] > maxs2[2]))
	{
		return(false);
	}

	return(true);
}


//-----------------------------------------------------------------------------
// Purpose: Constructor. Initializes data members.
//-----------------------------------------------------------------------------
CMapWorld::CMapWorld( void )
{

}


//-----------------------------------------------------------------------------
// Purpose: Constructor. Initializes data members.
//-----------------------------------------------------------------------------
CMapWorld::CMapWorld( CMapDoc *pOwningDocument )
{
	//
	// Make sure subsequent UpdateBounds() will be effective.
	//
	m_Render2DBox.ResetBounds();
	Vector pt( 0, 0, 0 );
	m_Render2DBox.UpdateBounds(pt);

	SetClass("worldspawn");
	m_pCullTree = NULL;

	m_nNextFaceID = 1;			// Face IDs start at 1. An ID of 0 means no ID.

	// create the world displacement manager
	m_pWorldDispMgr = CreateWorldEditDispMgr();

	m_pOwningDocument = pOwningDocument;
}


//-----------------------------------------------------------------------------
// Purpose: Destructor. Deletes all paths in the world and the culling tree.
//-----------------------------------------------------------------------------
CMapWorld::~CMapWorld(void)	
{
	// Delete paths.
	m_Paths.PurgeAndDeleteElements();
	
	//
	// Delete the culling tree.
	//
	CullTree_Free();

	// destroy the world displacement manager
	DestroyWorldEditDispMgr( &m_pWorldDispMgr );
}


//-----------------------------------------------------------------------------
// Purpose: Overridden to maintain the culling tree. Root level children of the
//			world are kept in the culling tree.
// Input  : pChild - object to add as a child.
//-----------------------------------------------------------------------------
void CMapWorld::AddChild(CMapClass *pChild)
{
	CMapClass::AddChild(pChild);

	//
	// Add the object to the culling tree.
	//
	if (m_pCullTree != NULL)
	{
		m_pCullTree->AddCullTreeObjectRecurse(pChild);
	}
}


//-----------------------------------------------------------------------------
// Purpose: The sole way to add an object to the world. 
//
//			NOTE: Do not call this during file load!! Similar (but different)
//				  bookkeeping is done in PostloadWorld during serialization.
//
// Input  : pObject - object to add to the world.
//			pParent - object to use as the new object's parent.
//-----------------------------------------------------------------------------
void CMapWorld::AddObjectToWorld(CMapClass *pObject, CMapClass *pParent)
{
	Assert(pObject != NULL);
	if (pObject == NULL)
	{
		return;
	}

	//
	// Link the object into the tree.
	//
	if (pParent == NULL)
	{
		pParent = this;
	}

	pParent->AddChild(pObject);

	//
	// If this object or any of its children are entities, add the entities
	// to our optimized list of entities.
	//
	EntityList_Add(pObject);

	//
	// Notify the object that it has been added to the world.
	//
	pObject->OnAddToWorld(this);
}


//-----------------------------------------------------------------------------
// Purpose: Sorts all the objects in the world into three lists: entities, solids,
//			and groups. These lists are then serialized in SaveVMF.
// Input  : pSaveLists - Receives lists of objects.
//-----------------------------------------------------------------------------
BOOL CMapWorld::BuildSaveListsCallback(CMapClass *pObject, SaveLists_t *pSaveLists)
{
	CMapEntity *pEntity = dynamic_cast<CMapEntity *>(pObject);
	if (pEntity != NULL)
	{
		pSaveLists->Entities.AddToTail(pEntity);
		return(TRUE);
	}

	CMapSolid *pSolid = dynamic_cast<CMapSolid *>(pObject);
	if (pSolid != NULL)
	{
		pSaveLists->Solids.AddToTail(pSolid);
		return(TRUE);
	}

	CMapGroup *pGroup = dynamic_cast<CMapGroup *>(pObject);
	if (pGroup != NULL)
	{
		pSaveLists->Groups.AddToTail(pGroup);
		return(TRUE);
	}

	return(TRUE);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : CMapClass
//-----------------------------------------------------------------------------
CMapClass *CMapWorld::Copy(bool bUpdateDependencies)
{
	CMapWorld *pWorld = new CMapWorld;
	pWorld->CopyFrom(this, bUpdateDependencies);
	return pWorld;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pobj - 
// Output : CMapClass
//-----------------------------------------------------------------------------
CMapClass *CMapWorld::CopyFrom(CMapClass *pobj, bool bUpdateDependencies)
{
	Assert(pobj->IsMapClass(MAPCLASS_TYPE(CMapWorld)));
	CMapWorld *pFrom = (CMapWorld *)pobj;
	
	CMapClass::CopyFrom(pobj, bUpdateDependencies);

	//
	// Copy our keys. If our targetname changed we must relink all targetname pointers.
	//
	const char *pszOldTargetName = CEditGameClass::GetKeyValue("targetname");
	char szOldTargetName[MAX_IO_NAME_LEN];
	if (pszOldTargetName != NULL)
	{
		strcpy(szOldTargetName, pszOldTargetName);
	}

	CEditGameClass::CopyFrom(pFrom);

	const char *pszNewTargetName = CEditGameClass::GetKeyValue("targetname");
	if ((bUpdateDependencies) && (pszNewTargetName != NULL))
	{
		if (stricmp(szOldTargetName, pszNewTargetName) != 0)
		{
			UpdateAllDependencies(this);
		}
	}

	return this;
}


//-----------------------------------------------------------------------------
// Hash the string to the bucket index where it belongs.
//-----------------------------------------------------------------------------
static inline int EntityBucketForName( const char *pszName )
{
	if ( !pszName )
		return 0;

	unsigned int nHash = HashStringCaseless( pszName );
	
	return nHash % NUM_HASHED_ENTITY_BUCKETS;
}	


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CMapWorld::FindEntityBucket( CMapEntity *pEntity, int *pnIndex )
{
	for ( int i = 0; i < NUM_HASHED_ENTITY_BUCKETS; i++ )
	{
		int nIndex = m_EntityListByName[ i ].Find( pEntity );
		if ( nIndex != -1 )
		{
			if ( pnIndex )
			{
				*pnIndex = nIndex;
			}
			
			return i;
		}
	}
	
	return -1;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CMapWorld::AddEntity( CMapEntity *pEntity )
{
	if ( m_EntityList.Find( pEntity ) != -1 )
		return;

	// Add it to the flat list.
	m_EntityList.AddToTail( pEntity );
	
	// If it has a name, add it to the list of entities hashed by name checksum.
	const char *pszName = pEntity->GetKeyValue( "targetname" );
	if ( pszName )
	{
		int nBucket = EntityBucketForName( pszName );
		m_EntityListByName[ nBucket ].AddToTail( pEntity );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Adds any entities found in the given object tree to the list of
//			entities that are in this world. Called whenever an object is added
//			to this world.
// Input  : pObject - object (and children) to add to the entity list.
//-----------------------------------------------------------------------------
void CMapWorld::EntityList_Add(CMapClass *pObject)
{
	CMapEntity *pEntity = dynamic_cast<CMapEntity *>(pObject);
	if (pEntity != NULL)
	{
		AddEntity(pEntity);
	}

	EnumChildrenPos_t pos;	
	CMapClass *pChild = pObject->GetFirstDescendent(pos);
	while (pChild != NULL)
	{
		pEntity = dynamic_cast<CMapEntity *>(pChild);
		if ((pEntity != NULL) && (m_EntityList.Find(pEntity) == -1))
		{
			AddEntity(pEntity);
		}

		pChild = pObject->GetNextDescendent(pos);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Removes this object (if it is an entity) or any of its entity
//			descendents from this world's entity list. Called when an object is
//			removed from this world.
// Input  : pObject - Object to remove from the entity list.
//-----------------------------------------------------------------------------
void CMapWorld::EntityList_Remove(CMapClass *pObject, bool bRemoveChildren)
{
	//
	// Remove the object itself.
	//
	CMapEntity *pEntity = dynamic_cast<CMapEntity *>(pObject);
	if (pEntity != NULL)
	{
		// Remove the entity from the flat list.
		int nIndex = m_EntityList.Find( pEntity );
		if ( nIndex != -1 )
		{
			m_EntityList.FastRemove( nIndex );
		}

		// Remove the entity from the hashed list.
		int nOldBucket = FindEntityBucket( pEntity, &nIndex );
		if ( nOldBucket != -1 )
		{
			m_EntityListByName[ nOldBucket ].FastRemove( nIndex );
		}

		Assert( m_EntityList.Find( pEntity ) == -1 );
	}
	
	//
	// Remove entity children.
	//
	if (bRemoveChildren)
	{
		EnumChildrenPos_t pos;
		CMapClass *pChild = pObject->GetFirstDescendent(pos);
		while (pChild != NULL)
		{
			pEntity = dynamic_cast<CMapEntity *>(pChild);
			if (pEntity != NULL)
			{
				m_EntityList.FindAndRemove(pEntity);
			}
			pChild = pObject->GetNextDescendent(pos);
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Overridden to maintain the culling tree. Root level children of the
//			world are kept in the culling tree.
// Input  : pChild - child to remove.
//-----------------------------------------------------------------------------
void CMapWorld::RemoveChild(CMapClass *pChild, bool bUpdateBounds)
{
	CMapClass::RemoveChild(pChild, bUpdateBounds);

	//
	// Remove the object from the culling tree because it is no longer a root-level child.
	//
	if (m_pCullTree != NULL)
	{
		m_pCullTree->RemoveCullTreeObjectRecurse(pChild);
	}
}


//-----------------------------------------------------------------------------
// Purpose: this function will attempt to find a child.  If the bool and matrix
//			are supplied, the localized matrix will be built.
// Input  : key - the key field to lookup
//			value - the value to find
// Output : returns the entity found 
//			bIsInInstance - optional parameter to indicate if the found entity is inside of an instance
//			InstanceMatrix - optional parameter to set the localized matrix of the instance stack
//-----------------------------------------------------------------------------
CMapEntity *CMapWorld::FindChildByKeyValue( const char* key, const char* value, bool *bIsInInstance, VMatrix *InstanceMatrix )
{
	if ( bIsInInstance )
	{
		*bIsInInstance = false;
	}

	return __super::FindChildByKeyValue( key, value, bIsInInstance, InstanceMatrix );
}


//-----------------------------------------------------------------------------
// Purpose: Removes an object from the world.
// Input  : pObject - object to remove from the world.
//			bChildren - whether we're removing the object's children as well.
//-----------------------------------------------------------------------------
void CMapWorld::RemoveObjectFromWorld(CMapClass *pObject, bool bRemoveChildren)
{
	Assert(pObject != NULL);
	if (pObject == NULL)
	{
		return;
	}

	//
	// Unlink the object from the tree.
	//
	CMapClass *pParent = pObject->GetParent();
	Assert(pParent != NULL);
	if (pParent != NULL)
	{
		pParent->RemoveChild(pObject);
	}

	//
	// If it (or any of its children) is an entity, remove it from this
	// world's list of entities.
	//
	EntityList_Remove(pObject, bRemoveChildren);

	//
	// Notify the object so it can release any pointers it may have to other
	// objects in the world. We don't do this in RemoveChild because the object
	// may only be changing parents rather than leaving the world.
	//
	pObject->OnRemoveFromWorld(this, bRemoveChildren);

}


//-----------------------------------------------------------------------------
// Purpose: Special implementation of UpdateChild for the world object. This
//			notifies the document that an object's bounding box has changed.
// Input  : pChild - 
//-----------------------------------------------------------------------------
void CMapWorld::UpdateChild(CMapClass *pChild)
{
	if ( CMapClass::s_bLoadingVMF )
		return;
	
	// Recalculate the bounds of this child's branch.
	pChild->CalcBounds(TRUE);

	// Recalculate own bounds
	CalcBounds( FALSE );

	//
	// Relink the child in the culling tree.
	//
	if (m_pCullTree != NULL)
	{
		m_pCullTree->UpdateCullTreeObjectRecurse(pChild);
	}

	//
	// Notify the document that an object in the world has changed.
	//
	if (!IsTemporary()) // HACK: check to avoid prefab objects ending up in the doc's update list
	{
		CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
		if (pDoc != NULL)
		{
			pDoc->UpdateObject(pChild);
		}
	}

	if ( CMapDoc::GetInLevelLoad() == 0 )
	{
		APP()->pMapDocTemplate->UpdateInstanceMap( m_pOwningDocument );
		APP()->pManifestDocTemplate->UpdateInstanceMap( m_pOwningDocument );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pList - 
//-----------------------------------------------------------------------------
void CMapWorld::GetUsedTextures(CUsedTextureList &List)
{
	List.RemoveAll();
	EnumChildren((ENUMMAPCHILDRENPROC)AddUsedTextures, (DWORD)&List, MAPCLASS_TYPE(CMapSolid));
	EnumChildren((ENUMMAPCHILDRENPROC)AddOverlayTextures, (DWORD)&List, MAPCLASS_TYPE(CMapOverlay));
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pNode - 
//-----------------------------------------------------------------------------
void CMapWorld::CullTree_FreeNode(CCullTreeNode *pNode)
{
	if ( pNode == NULL )
	{
		Assert(pNode != NULL);
		return;
	}

	int nChildCount = pNode->GetChildCount();
	if (nChildCount != 0)
	{
		for (int nChild = 0; nChild < nChildCount; nChild++)
		{
			CCullTreeNode *pChild = pNode->GetCullTreeChild(nChild);
			CullTree_FreeNode(pChild);
		}
	}

	delete pNode;
}


//-----------------------------------------------------------------------------
// Purpose: Recursively deletes the entire culling tree if is it not NULL.
//			This does not delete the map objects that the culling tree contains,
//			only the leaves and nodes themselves.
//-----------------------------------------------------------------------------
void CMapWorld::CullTree_Free(void)
{
	if (m_pCullTree != NULL)
	{
		CullTree_FreeNode(m_pCullTree);
		m_pCullTree = NULL;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Determines if this node is a node or a leaf. If it is a node, it will
//			be split into eight children and each child will be populated with
//			objects whose bounding boxes intersect them, then split recursively.
//			If this node is a leaf, no action is taken and recursion terminates.
// Input  : pNode - 
//-----------------------------------------------------------------------------
#define MIN_NODE_DIM			1024		// Minimum node size of 170 x 170 x 170 feet
#define MIN_NODE_OBJECT_SPLIT	2			// Don't split nodes with fewer than two objects.

void CMapWorld::CullTree_SplitNode(CCullTreeNode *pNode)
{
	Vector Mins;
	Vector Maxs;
	Vector Size;

	pNode->GetBounds(Mins, Maxs);
	VectorSubtract(Maxs, Mins, Size);

	if ((Size[0] > MIN_NODE_DIM) && (Size[1] > MIN_NODE_DIM) && (Size[2] > MIN_NODE_DIM))
	{
		Vector Mids;
		int nChild;

		Mids[0] = (Mins[0] + Maxs[0]) / 2.0;
		Mids[1] = (Mins[1] + Maxs[1]) / 2.0;
		Mids[2] = (Mins[2] + Maxs[2]) / 2.0;

		for (nChild = 0; nChild < 8; nChild++)
		{
			Vector ChildMins;
			Vector ChildMaxs;

			//
			// Create a child and set its bounding box.
			//
			CCullTreeNode *pChild = new CCullTreeNode;

			if (nChild & 1)
			{
				ChildMins[0] = Mins[0];
				ChildMaxs[0] = Mids[0];
			}
			else
			{
				ChildMins[0] = Mids[0];
				ChildMaxs[0] = Maxs[0];
			}

			if (nChild & 2)
			{
				ChildMins[1] = Mins[1];
				ChildMaxs[1] = Mids[1];
			}
			else
			{
				ChildMins[1] = Mids[1];
				ChildMaxs[1] = Maxs[1];
			}

			if (nChild & 4)
			{
				ChildMins[2] = Mins[2];
				ChildMaxs[2] = Mids[2];
			}
			else
			{
				ChildMins[2] = Mids[2];
				ChildMaxs[2] = Maxs[2];
			}

			pChild->UpdateBounds(ChildMins, ChildMaxs);
			
			pNode->AddCullTreeChild(pChild);

			Vector mins1;
			Vector maxs1;
			pChild->GetBounds(mins1, maxs1);

			//
			// Check all objects in this node against the child's bounding box, adding the
			// objects that intersect to the child's object list.
			//
			int nObjectCount = pNode->GetObjectCount();
			for (int nObject = 0; nObject < nObjectCount; nObject++)
			{
				CMapClass *pObject = pNode->GetCullTreeObject(nObject);
				Assert(pObject != NULL);

				Vector mins2;
				Vector maxs2;
				pObject->GetCullBox(mins2, maxs2);
				if (BoxesIntersect(mins1, maxs1, mins2, maxs2))
				{
					pChild->AddCullTreeObject(pObject);
				}
			}
		}
				
		//
		// Remove all objects from this node's object list (since we are not a leaf).
		//
		pNode->RemoveAllCullTreeObjects();

		//
		// Recurse into all children with at least two objects, splitting them.
		//
		int nChildCount = pNode->GetChildCount();
		for (nChild = 0; nChild < nChildCount; nChild++)
		{
			CCullTreeNode *pChild = pNode->GetCullTreeChild(nChild);
			if (pChild->GetObjectCount() >= MIN_NODE_OBJECT_SPLIT)
			{
				CullTree_SplitNode(pChild);
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pNode - 
//-----------------------------------------------------------------------------
void CMapWorld::CullTree_DumpNode(CCullTreeNode *pNode, int nDepth)
{
	int nChildCount = pNode->GetChildCount();
	char szText[100];

	if (nChildCount == 0)
	{
		// Leaf
		OutputDebugString("LEAF:\n");
		int nObjectCount = pNode->GetObjectCount();
		for (int nObject = 0; nObject < nObjectCount; nObject++)
		{
			CMapClass *pMapClass = pNode->GetCullTreeObject(nObject);
			sprintf(szText, "%*c %p %s\n", nDepth, ' ', pMapClass, pMapClass->GetType());
			OutputDebugString(szText);
		}
	}
	else
	{
		// Node
		sprintf(szText, "%*s\n", nDepth, "+");
		OutputDebugString(szText);

		for (int nChild = 0; nChild < nChildCount; nChild++)
		{
			CCullTreeNode *pChild = pNode->GetCullTreeChild(nChild);
			CullTree_DumpNode(pChild, nDepth + 1);
		}

		OutputDebugString("\n");
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapWorld::CullTree_Build(void)
{
	CullTree_Free();
	m_pCullTree = new CCullTreeNode;

	//
	// The top level node in the tree uses the largest possible bounding box.
	//
	Vector BoxMins( g_MIN_MAP_COORD, g_MIN_MAP_COORD, g_MIN_MAP_COORD );
	Vector BoxMaxs( g_MAX_MAP_COORD, g_MAX_MAP_COORD, g_MAX_MAP_COORD );
	m_pCullTree->UpdateBounds(BoxMins, BoxMaxs);

	//
	// Populate the top level node with the contents of the world.
	//
	FOR_EACH_OBJ( m_Children, pos )
	{
		CMapClass *pObject = m_Children.Element(pos);
		m_pCullTree->AddCullTreeObject(pObject);
	}

	//
	// Recursively split this node into children and populate them.
	//
	CullTree_SplitNode(m_pCullTree);

	//DumpCullTreeNode(m_pCullTree, 1);
	//OutputDebugString("\n");
}


//-----------------------------------------------------------------------------
// Purpose: Returns a list of all the groups in the world.
//-----------------------------------------------------------------------------
int CMapWorld::GetGroupList(CUtlVector<CMapGroup *> &GroupList)
{
	GroupList.RemoveAll();
	EnumChildrenPos_t pos;
	CMapClass *pChild = GetFirstDescendent(pos);
	while (pChild != NULL)
	{
		if (pChild->IsGroup())
		{
			GroupList.AddToTail((CMapGroup *)pChild);
		}

		pChild = GetNextDescendent(pos);
	}

	return GroupList.Count();
}


//-----------------------------------------------------------------------------
// Purpose: Called after all objects in the World have been loaded. Calls the
//			PostLoadWorld function for every object in the world, then
//			builds the culling tree.
//-----------------------------------------------------------------------------
void CMapWorld::PostloadWorld(void)
{
	// This causes certain calculations to get delayed until the end.
	CMapClass::s_bLoadingVMF = true;
	
	//
	// Set the class name from our "classname" key and discard the key.
	//
	int nIndex;
	const char *pszValue = pszValue = m_KeyValues.GetValue("classname", &nIndex);
	if (pszValue != NULL)
	{
		SetClass(pszValue);
		RemoveKey(nIndex);
	}

	//
	// Call PostLoadWorld on all our children and add any entities to the
	// entity list.
	//

	FOR_EACH_OBJ( m_Children, pos )
	{
		CMapClass *pChild = m_Children[pos];
		pChild->PostloadWorld(this);
		EntityList_Add(pChild);
	}
	
	// Since s_bLoadingVMF was on before, a bunch of stuff got delayed. Now let's do that stuff.
	CMapClass::s_bLoadingVMF = false;
	FOR_EACH_OBJ( m_Children, pos )
	{
		CMapClass *pChild = m_Children[pos];
		pChild->CalcBounds( TRUE );
		//
		// Relink the child in the culling tree.
		//
		if (m_pCullTree != NULL)
		{
			m_pCullTree->UpdateCullTreeObjectRecurse(pChild);
		}

		pChild->PostUpdate(Notify_Changed);
		pChild->SignalChanged();
	}
	CalcBounds( FALSE ); // Recalculate the world's bounds now that everyone else's bounds are upadted.
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pFile - 
//			pData - 
// Output : ChunkFileResult_t
//-----------------------------------------------------------------------------
ChunkFileResult_t CMapWorld::LoadGroupCallback(CChunkFile *pFile, CMapWorld *pWorld)
{
	CMapGroup *pGroup = new CMapGroup;

	ChunkFileResult_t eResult = pGroup->LoadVMF(pFile);
	if (eResult == ChunkFile_Ok)
	{
		pWorld->AddChild(pGroup);
	}

	return(eResult);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pLoadInfo - 
//			*pWorld - 
// Output : ChunkFileResult_t
//-----------------------------------------------------------------------------
ChunkFileResult_t CMapWorld::LoadHiddenCallback(CChunkFile *pFile, CMapWorld *pWorld)
{
	//
	// Set up handlers for the subchunks that we are interested in.
	//
	CChunkHandlerMap Handlers;
	Handlers.AddHandler("solid", (ChunkHandler_t)LoadSolidCallback, pWorld);
	
	pFile->PushHandlers(&Handlers);
	ChunkFileResult_t eResult = pFile->ReadChunk();
	pFile->PopHandlers();

	return(eResult);
}


//-----------------------------------------------------------------------------
// Purpose: Handles keyvalues when loading the world chunk of MAP files.
// Input  : szKey - Key to handle.
//			szValue - Value of key.
//			pWorld - World being loaded.
// Output : Returns ChunkFile_Ok if all is well.
//-----------------------------------------------------------------------------
ChunkFileResult_t CMapWorld::LoadKeyCallback(const char *szKey, const char *szValue, CMapWorld *pWorld)
{
	if (!stricmp(szKey, "id"))
	{
		pWorld->SetID(atoi(szValue));
	}
	else if (stricmp(szKey, "mapversion") != 0)
	{
		pWorld->SetKeyValue(szKey, szValue);
	}

	return(ChunkFile_Ok);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pLoadInfo - 
// Output : ChunkFileResult_t
//-----------------------------------------------------------------------------
ChunkFileResult_t CMapWorld::LoadVMF(CChunkFile *pFile)
{
	//
	// Set up handlers for the subchunks that we are interested in.
	//
	CChunkHandlerMap Handlers;
	Handlers.AddHandler("solid", (ChunkHandler_t)LoadSolidCallback, this);
	Handlers.AddHandler("hidden", (ChunkHandler_t)LoadHiddenCallback, this);
	Handlers.AddHandler("group", (ChunkHandler_t)LoadGroupCallback, this);
	Handlers.AddHandler("connections", (ChunkHandler_t)LoadConnectionsCallback, (CEditGameClass *)this);

	pFile->PushHandlers(&Handlers);
	ChunkFileResult_t eResult = pFile->ReadChunk((KeyHandler_t)LoadKeyCallback, this);
	pFile->PopHandlers();

	return(eResult);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pLoadInfo - 
//			*pWorld - 
// Output : ChunkFileResult_t
//-----------------------------------------------------------------------------
ChunkFileResult_t CMapWorld::LoadSolidCallback(CChunkFile *pFile, CMapWorld *pWorld)
{
	CMapSolid *pSolid = new CMapSolid;

	bool bValid;
	ChunkFileResult_t eResult = pSolid->LoadVMF(pFile, bValid);

	if ((eResult == ChunkFile_Ok) && (bValid))
	{
		const char *pszValue = pSolid->GetEditorKeyValue("cordonsolid");
		if (pszValue == NULL)
		{
			pWorld->AddChild(pSolid);
		}
	}
	else
	{
		delete pSolid;
	}

	return(eResult);
}


//-----------------------------------------------------------------------------
// Purpose: Calls PresaveWorld in all of the world's descendents.
//-----------------------------------------------------------------------------
void CMapWorld::PresaveWorld(void)
{
	EnumChildrenPos_t pos;
	CMapClass *pChild = GetFirstDescendent(pos);
	while (pChild != NULL)
	{
		pChild->PresaveWorld();
		pChild = GetNextDescendent(pos);
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
ChunkFileResult_t CMapWorld::SaveSolids(CChunkFile *pFile, CSaveInfo *pSaveInfo, int saveFlags)
{
	PresaveWorld();

	SaveLists_t SaveLists;
	EnumChildrenRecurseGroupsOnly((ENUMMAPCHILDRENPROC)BuildSaveListsCallback, (DWORD)&SaveLists);

	return SaveObjectListVMF(pFile, pSaveInfo, &SaveLists.Solids, saveFlags);
}


//-----------------------------------------------------------------------------
// Purpose: Saves all solids, entities, and groups in the world to a VMF file.
// Input  : pFile - File object to use for saving.
//			pSaveInfo - Holds rules for which objects to save.
// Output : Returns ChunkFile_Ok if the save was successful, or an error code.
//-----------------------------------------------------------------------------
ChunkFileResult_t CMapWorld::SaveVMF(CChunkFile *pFile, CSaveInfo *pSaveInfo, int saveFlags)
{
	PresaveWorld();

	//
	// Sort the world objects into lists for saving into different chunks.
	//
	SaveLists_t SaveLists;
	EnumChildrenRecurseGroupsOnly((ENUMMAPCHILDRENPROC)BuildSaveListsCallback, (DWORD)&SaveLists);

	//
	// Begin the world chunk.
	//
	ChunkFileResult_t  eResult = ChunkFile_Ok;

	if( !(saveFlags & SAVEFLAGS_LIGHTSONLY) )
	{
		eResult = pFile->BeginChunk("world");

		//
		// Save world ID - it's always zero.
		//
		if (eResult == ChunkFile_Ok)
		{
			eResult = pFile->WriteKeyValueInt("id", GetID());
		}

		//
		// HACK: Save map version. This is already being saved in the version info block by the doc.
		//
		if (eResult == ChunkFile_Ok)
		{
			eResult = pFile->WriteKeyValueInt("mapversion", CMapDoc::GetActiveMapDoc()->GetDocVersion());
		}

		//
		// Save world keys.
		//
		if (eResult == ChunkFile_Ok)
		{
			CEditGameClass::SaveVMF(pFile, pSaveInfo);
		}

		//
		// Save world solids.
		//
		if (eResult == ChunkFile_Ok)
		{
			eResult = SaveObjectListVMF(pFile, pSaveInfo, &SaveLists.Solids, saveFlags);
		}

		//
		// Save groups.
		//
		if (eResult == ChunkFile_Ok)
		{
			eResult = SaveObjectListVMF(pFile, pSaveInfo, &SaveLists.Groups, saveFlags);
		}

		//
		// End the world chunk.
		//
		if (eResult == ChunkFile_Ok)
		{
			pFile->EndChunk();
		}
	}

	//
	// Save entities and their solid children.
	//
	if (eResult == ChunkFile_Ok)
	{
		eResult = SaveObjectListVMF(pFile, pSaveInfo, &SaveLists.Entities, saveFlags);
	}

	return(eResult);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pFile - 
//			*pList - 
// Output : ChunkFileResult_t
//-----------------------------------------------------------------------------
ChunkFileResult_t CMapWorld::SaveObjectListVMF(CChunkFile *pFile, CSaveInfo *pSaveInfo, const CMapObjectList *pList, int saveFlags)
{
	FOR_EACH_OBJ( *pList, pos )
	{
		CMapClass *pObject = pList->Element(pos);

		// Only save lights if that's what they want.
		if( saveFlags & SAVEFLAGS_LIGHTSONLY )
		{
			CMapEntity *pMapEnt = dynamic_cast<CMapEntity*>( pObject );
			bool bIsLight = pMapEnt && strncmp( pMapEnt->GetClassName(), "light", 5 ) == 0;
			if( !bIsLight )
				continue;
		}

		
		if (pObject != NULL)
		{
			ChunkFileResult_t eResult = pObject->SaveVMF(pFile, pSaveInfo);
			if (eResult != ChunkFile_Ok)
			{
				return(eResult);
			}
		}
	}

	return(ChunkFile_Ok);
}


//-----------------------------------------------------------------------------
// Purpose: Adds a given character to the end of a string if there isn't one already.
// Input  : psz - String to add the backslash to.
//			ch - Character to check for (and add if not found).
//			nSize - Size of buffer pointer to by psz.
// Output : Returns true if there was enough space in the dest buffer, false if not.
//-----------------------------------------------------------------------------
static bool EnsureTrailingChar(char *psz, char ch, int nSize)
{
	int nLen = strlen(psz);
	if ((psz[0] != '\0') && (psz[nLen - 1] != ch))
	{
		if (nLen < (nSize - 1))
		{
			psz[nLen++] = ch;
			psz[nLen] = '\0';
		}
		else
		{
			// No room to add the character.
			return(false);
		}
	}

	return(true);
}


//-----------------------------------------------------------------------------
// Purpose: Finds the face with the corresponding face ID.
//			FIXME: AAARGH, slow!! Need to build a table or something.
// Input  : nFaceID - 
//-----------------------------------------------------------------------------
CMapFace *CMapWorld::FaceID_FaceForID(int nFaceID)
{
	EnumChildrenPos_t pos;
	CMapClass *pChild = GetFirstDescendent(pos);
	while (pChild != NULL)
	{
		CMapSolid *pSolid = dynamic_cast <CMapSolid *>(pChild);
		if (pSolid != NULL)
		{
			int nFaceCount = pSolid->GetFaceCount();
			for (int nFace = 0; nFace < nFaceCount; nFace++)
			{
				CMapFace *pFace = pSolid->GetFace(nFace);
				if (pFace->GetFaceID() == nFaceID)
				{
					return(pFace);
				}
			}
		}

		pChild = GetNextDescendent(pos);
	}

	return(NULL);
}


//-----------------------------------------------------------------------------
// Purpose: Concatenates strings without overrunning the dest buffer.
// Input  : szDest - 
//			szSrc - 
//			nDestSize - 
// Output : Returns true if all chars were copied, false if we ran out of room.
//-----------------------------------------------------------------------------
static bool AppendString(char *szDest, char const *szSrc, int nDestSize)
{
	int nDestLen = strlen(szDest);
	int nDestAvail = nDestSize - nDestLen - 1;

	char *pszStart = szDest + nDestLen;
	char *psz = pszStart;

	while ((nDestAvail > 0) && (*szSrc != '\0'))
	{
		*psz++ = *szSrc++;
		nDestAvail--;
	}

	*psz = '\0';

	if (*szSrc != '\0')
	{
		// If we ran out of room, don't append anything. We don't want partial strings.
		*pszStart = '\0';
	}

	return(*szSrc == '\0');
}


//-----------------------------------------------------------------------------
// Purpose: Encode the list of fully selected and partially selected faces in
//			a string of the form: "4 5 12 (1 8)"
//
//			This is used for multiselect editing of sidelist keyvalues.
//
// Input  : pszValue - The buffer to receive the face lists encoded as a string.
//			pFullFaceList - the list of faces that are considered fully in the list
//			pPartialFaceList - the list of faces that are partially in the list
//-----------------------------------------------------------------------------
bool CMapWorld::FaceID_FaceIDListsToString(char *pszList, int nSize, CMapFaceIDList *pFullFaceIDList, CMapFaceIDList *pPartialFaceIDList)
{
	if (pszList == NULL)
	{
		return(false);
	}

	pszList[0] = '\0';

	//
	// Add the fully selected faces, space delimited.
	//
	if (pFullFaceIDList != NULL)
	{
		for (int i = 0; i < pFullFaceIDList->Count(); i++)
		{
			int nFace = pFullFaceIDList->Element(i);

			char szID[64];
			itoa(nFace, szID, 10);
			if (!EnsureTrailingChar(pszList, ' ', nSize) || !AppendString(pszList, szID, nSize))
			{
				return(false);
			}
		}
	}

	//
	// Add the partially selected faces inside of parentheses.
	//
	if (pPartialFaceIDList != NULL)
	{
		if (pPartialFaceIDList->Count() > 0)
		{
			if (!EnsureTrailingChar(pszList, ' ', nSize) || !AppendString(pszList, "(", nSize))
			{
				return(false);
			}

			bool bFirst = true;
			
			for (int i = 0; i < pPartialFaceIDList->Count(); i++)
			{
				int nFace = pPartialFaceIDList->Element(i);

				char szID[64];
				itoa(nFace, szID, 10);
				if (!bFirst)
				{
					if (!EnsureTrailingChar(pszList, ' ', nSize))
					{
						return(false);
					}
				}
				bFirst = false;
				if (!AppendString(pszList, szID, nSize))
				{
					return(false);
				}
			}

			AppendString(pszList, ")", nSize);
		}
	}

	return(true);
}


//-----------------------------------------------------------------------------
// Purpose: Encode the list of fully selected and partially selected faces in
//			a string of the form: "4 5 12 (1 8)"
//
//			This is used for multiselect editing of sidelist keyvalues.
//
// Input  : pszValue - The buffer to receive the face lists encoded as a string.
//			pFullFaceList - the list of faces that are considered fully in the list
//			pPartialFaceList - the list of faces that are partially in the list
//-----------------------------------------------------------------------------
bool CMapWorld::FaceID_FaceListsToString(char *pszList, int nSize, CMapFaceList *pFullFaceList, CMapFaceList *pPartialFaceList)
{
	if (pszList == NULL)
	{
		return(false);
	}

	pszList[0] = '\0';

	//
	// Add the fully selected faces, space delimited.
	//
	if (pFullFaceList != NULL)
	{
		for (int i = 0; i < pFullFaceList->Count(); i++)
		{
			CMapFace *pFace = pFullFaceList->Element(i);

			char szID[64];
			itoa(pFace->GetFaceID(), szID, 10);
			if (!EnsureTrailingChar(pszList, ' ', nSize) || !AppendString(pszList, szID, nSize))
			{
				return(false);
			}
		}
	}

	//
	// Add the partially selected faces inside of parentheses.
	//
	if (pPartialFaceList != NULL)
	{
		if (pPartialFaceList->Count() > 0)
		{
			if (!EnsureTrailingChar(pszList, ' ', nSize) || !AppendString(pszList, "(", nSize))
			{
				return(false);
			}

			bool bFirst = true;
			
			for (int i = 0; i < pPartialFaceList->Count(); i++)
			{
				CMapFace *pFace = pPartialFaceList->Element(i);

				char szID[64];
				itoa(pFace->GetFaceID(), szID, 10);
				if (!bFirst)
				{
					if (!EnsureTrailingChar(pszList, ' ', nSize))
					{
						return(false);
					}
				}
				bFirst = false;
				if (!AppendString(pszList, szID, nSize))
				{
					return(false);
				}
			}

			AppendString(pszList, ")", nSize);
		}
	}

	return(true);
}


//-----------------------------------------------------------------------------
// Purpose: Decode a string of the form: "4 5 12 (1 8)" into a list of fully
//			selected and a list of partially selected face IDs.
//
//			This is used for multiselect editing of sidelist keyvalues.
//
// Input  : pszValue - The buffer to receive the face lists encoded as a string.
//			pFullFaceList - the list of faces that are considered fully in the list
//			pPartialFaceList - the list of faces that are partially in the list
//-----------------------------------------------------------------------------
void CMapWorld::FaceID_StringToFaceIDLists(CMapFaceIDList *pFullFaceList, CMapFaceIDList *pPartialFaceList, const char *pszValue)
{
	if (pFullFaceList != NULL)
	{
		pFullFaceList->RemoveAll();
	}

	if (pPartialFaceList != NULL)
	{
		pPartialFaceList->RemoveAll();
	}

	if (pszValue != NULL)
	{
		char szVal[KEYVALUE_MAX_VALUE_LENGTH];
		strcpy(szVal, pszValue);

		int nParens = 0;
		bool bInParens = false;

		char *psz = strtok(szVal, " ");
		while (psz != NULL)
		{
			//
			// Strip leading or trailing parentheses from the substring.
			//
			bool bFirstValid = true;
			char *pszRemoveParens = psz;
			while (*pszRemoveParens != '\0')
			{
				if (*pszRemoveParens == '(')
				{
					nParens++;
					*pszRemoveParens = '\0';
				}
				else if (*pszRemoveParens == ')')
				{
					nParens--;
					*pszRemoveParens = '\0';
				}
				else if (bFirstValid)
				{
					//
					// Note the parentheses depth at the start of this number.
					//
					if (nParens > 0)
					{
						bInParens = true;
					}
					else
					{
						bInParens = false;
					}

					psz = pszRemoveParens;
					bFirstValid = false;
				}
				pszRemoveParens++;
			}

			//
			// The substring should now be a single face ID. Get the corresponding
			// face and add it to the list.
			//
			int nFaceID = atoi(psz);
			if (bInParens)
			{
				if (pPartialFaceList != NULL)
				{
					pPartialFaceList->AddToTail(nFaceID);
				}
			}
			else
			{
				if (pFullFaceList != NULL)
				{
					pFullFaceList->AddToTail(nFaceID);
				}
			}

			//
			// Get the next substring.
			//
			psz = strtok(NULL, " ");
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Decode a string of the form: "4 5 12 (1 8)" into a list of fully
//			selected and a list of partially selected faces.
//
//			This is used for multiselect editing of sidelist keyvalues.
//
// Input  : pszValue - The buffer to receive the face lists encoded as a string.
//			pFullFaceList - the list of faces that are considered fully in the list
//			pPartialFaceList - the list of faces that are partially in the list
//-----------------------------------------------------------------------------
void CMapWorld::FaceID_StringToFaceLists(CMapFaceList *pFullFaceList, CMapFaceList *pPartialFaceList, const char *pszValue)
{
	CMapFaceIDList FullFaceIDList;
	CMapFaceIDList PartialFaceIDList;

	FaceID_StringToFaceIDLists(&FullFaceIDList, &PartialFaceIDList, pszValue);

	if (pFullFaceList != NULL)
	{
		pFullFaceList->RemoveAll();

		for (int i = 0; i < FullFaceIDList.Count(); i++)
		{
			//
			// Get the corresponding face and add it to the list.
			//
			// FACEID TODO: fix so we only interate the world objects once
			CMapFace *pFace = FaceID_FaceForID(FullFaceIDList.Element(i));
			if (pFace != NULL)
			{
				pFullFaceList->AddToTail(pFace);
			}
		}
	}

	if (pPartialFaceList != NULL)
	{
		pPartialFaceList->RemoveAll();

		for (int i = 0; i < PartialFaceIDList.Count(); i++)
		{
			//
			// Get the corresponding face and add it to the list.
			//
			// FACEID TODO: fix so we only interate the world objects once
			CMapFace *pFace = FaceID_FaceForID(PartialFaceIDList.Element(i));
			if (pFace != NULL)
			{
				pPartialFaceList->AddToTail(pFace);
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: increments the numerals at the end of a string
//			appends 0 if no numerals exist
// Input  : newName - 
//-----------------------------------------------------------------------------
static void IncrementStringName( char *str, int nMaxLength )
{
	// walk backwards through the string looking for where the digits stop
	int orgLen = Q_strlen(str);
	int pos = orgLen;
	while ( (pos > 0) && V_isdigit(str[pos-1]) )
	{
		pos--;
	}

	// if no digits found, append a "1"
	if ( pos == orgLen )
	{
		Q_strncat( str, "1", nMaxLength );
	}
	else
	{
		// get the number
		int iNum = Q_atoi( str+pos );

		// increment the number
		iNum++;

		// cut off old number
		str[pos]=0;

		// add the new number to the string
		Q_snprintf( str, nMaxLength, "%s%d", str, iNum );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Generates a new, unique targetname for the given entity based on an
//			existing entity name.
//			a static function
// Input  : pObject - the entity
//			startName - the name of the original entity - assumed to already exist in the map
//			outputName - the new name based on the original name, guaranteed to be unique
//			szPrefix - a string to prepend to the new name
//			newNameBufferSize - 
//			bMakeUnique - if true, the generated name will be unique in this world and pRoot
//			szPrefix - prefix to prepend to the new name
//			pRoot - an optional tree of objects to look in for uniqueness
//-----------------------------------------------------------------------------
bool CMapWorld::GenerateNewTargetname( const char *startName, char *outputName, int newNameBufferSize, bool bMakeUnique, const char *szPrefix, CMapClass *pRoot )
{
	outputName[0] = 0;

	if ( szPrefix )
	{
		// add prefix if any give
		Q_strncpy( outputName, szPrefix, newNameBufferSize );
	}

	// add start name 
	Q_strncat( outputName, startName, newNameBufferSize );

	// if new name is still empty, set entity as default
	if ( Q_strlen( outputName ) == 0 )
	{
		Q_strncpy( outputName, "entity", newNameBufferSize );
	}

	// Only append numbers to the name if we need to. It's possible that adding
	// the prefix was sufficient to make the name unique.
	if ( bMakeUnique && FindEntityByName( outputName, false, true ) )
	{
		// try to find entities that match the name
		CMapEntity *pEnt = NULL;
		do
		{
			// increment the entity name
			IncrementStringName( outputName, newNameBufferSize );

			pEnt = FindEntityByName( outputName, false, true );

			if ( !pEnt && pRoot )
			{
				pEnt = pRoot->FindChildByKeyValue( "targetname", outputName );
			}
			
		} while ( pEnt );
	}
	
	return true;
}

void CMapWorld::PostloadVisGroups()
{
	bool bFoundOrphans = false;
	CMapObjectList orphans;
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();

	FOR_EACH_OBJ( m_Children, pos )
	{
		CMapClass *pChild = m_Children[pos];

		if ( pChild->PostloadVisGroups( true ) == true )
		{
            orphans.AddToTail( pChild );
			bFoundOrphans = true;
		}
	}
	if ( bFoundOrphans == true )
	{
		pDoc->VisGroups_CreateNamedVisGroup( orphans, "_orphaned hidden", true, false );
		GetMainWnd()->MessageBox( "Orphaned objects were found and placed into the \"_orphaned hidden\" visgroup.", "Orphaned Objects Found", MB_OK | MB_ICONEXCLAMATION);
	}

	// Link up all the connections to the entities
	const CMapEntityList *pEntities = EntityList_GetList();
	FOR_EACH_OBJ( *pEntities, pos )
	{
		CMapEntity *pEntity = dynamic_cast< CMapEntity *>( (*pEntities)[pos] );
#if	defined(_DEBUG) && 0
		LPCTSTR	pszTargetName = pEntity->GetKeyValue("targetname");
		if ( pszTargetName && !strcmp(pszTargetName, "relay_cancelVCDs") )
		{
			// Set breakpoint here for debugging this entity's visiblity
			int foo = 0;
		}
#endif
		int nConnections = pEntity->Connections_GetCount();
		for ( int pos2 = 0; pos2 < nConnections; pos2++ )
		{
			CEntityConnection	*pEntityConnection = pEntity->Connections_Get(pos2);

			// Link this connection back to the entity 
			pEntityConnection->GetSourceEntityList()->AddToTail( pEntity );
			pEntityConnection->LinkTargetEntities();
		}
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CMapEntity *CMapWorld::FindEntityByName( const char *pszName, bool bVisiblesOnly, bool bSearchInstanceParms )
{
	if ( !pszName )
		return NULL;

	CMapEntityList *pList = &m_EntityList;

	if ( !strchr( pszName, '*' ) )
	{
		int nBucket = EntityBucketForName( pszName );
		pList = &m_EntityListByName[nBucket];
	}
	
	int nCount = pList->Count();
	for ( int i = 0; i < nCount; i++ )
	{
		CMapEntity *pEntity = pList->Element( i );
		
		if ( pEntity->IsVisible() || !bVisiblesOnly )
		{
			if ( pEntity->NameMatches( pszName ) )
			{
				return pEntity;
			}
		}
	}
	
	if ( bSearchInstanceParms == true )
	{
		const CMapEntityList *pEntities = EntityList_GetList();
		FOR_EACH_OBJ( *pEntities, pos )
		{
			CMapEntity *pEntity = dynamic_cast< CMapEntity *>( (*pEntities)[pos] );
			if ( pEntity->ClassNameMatches( "func_instance" ) == true )
			{
				for ( int j = pEntity->GetFirstKeyValue(); j != pEntity->GetInvalidKeyValue(); j = pEntity->GetNextKeyValue( j ) )
				{
					LPCTSTR	pInstanceKey = pEntity->GetKey( j );
					LPCTSTR	pInstanceValue = pEntity->GetKeyValue( j );
					if ( strnicmp( pInstanceKey, "replace", strlen( "replace" ) ) == 0 )
					{
						const char *InstancePos = strchr( pInstanceValue, ' ' );
						if ( InstancePos == NULL )
						{
							continue;
						}

						if ( strcmpi( pszName, InstancePos + 1 ) == 0 )
						{
							return pEntity;
						}
					}
				}
			}
		}
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Finds all entities in the map with a given class name.
// Input  : pFound - List of entities with the class name.
//			pszClassName - Class name to match, case insensitive.
// Output : Returns true if any matches were found, false if not.
//-----------------------------------------------------------------------------
bool CMapWorld::FindEntitiesByClassName(CMapEntityList &Found, const char *pszClassName, bool bVisiblesOnly)
{
	Found.RemoveAll();

	int nCount = EntityList_GetCount();
	for ( int i = 0; i < nCount; i++ )
	{
		CMapEntity *pEntity = EntityList_GetEntity( i );
		
		if ( pEntity->IsVisible() || !bVisiblesOnly )
		{
			if ( pEntity->ClassNameMatches( pszClassName ) )
			{
				Found.AddToTail( pEntity );
			}
		}
	}

	return( Found.Count() != 0 );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pFound - 
//			pszTargetName - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CMapWorld::FindEntitiesByKeyValue(CMapEntityList &Found, const char *pszKey, const char *pszValue, bool bVisiblesOnly)
{
	Found.RemoveAll();

	int nCount = EntityList_GetCount();
	for ( int i = 0; i < nCount; i++ )
	{
		CMapEntity *pEntity = EntityList_GetEntity( i );
		
		if ( pEntity->IsVisible() || !bVisiblesOnly )
		{
			const char *pszThisValue = pEntity->GetKeyValue( pszKey );

			if ( pszThisValue != NULL )
			{
				if (( pszValue != NULL ) && ( !stricmp( pszValue, pszThisValue )))
				{
					Found.AddToTail( pEntity );
				}
			}
			else if (pszValue == NULL)
			{
				Found.AddToTail( pEntity );
			}
		}
	}

	return( Found.Count() != 0 );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CMapWorld::FindEntitiesByName( CMapEntityList &Found, const char *pszName, bool bVisiblesOnly )
{
	Found.RemoveAll();

	if ( !pszName )
		return false;
		
	CMapEntityList *pList = &m_EntityList;

	if ( !strchr( pszName, '*' ) )
	{
		int nBucket = EntityBucketForName( pszName );
		pList = &m_EntityListByName[nBucket];
	}
	
	int nCount = pList->Count();
	for ( int i = 0; i < nCount; i++ )
	{
		CMapEntity *pEntity = pList->Element( i );
		
		if ( pEntity->IsVisible() || !bVisiblesOnly )
		{
			if ( pEntity->NameMatches( pszName ) )
			{
				Found.AddToTail( pEntity );
			}
		}
	}

	return( Found.Count() != 0 );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CMapWorld::FindEntitiesByNameOrClassName(CMapEntityList &Found, const char *pszName, bool bVisiblesOnly)
{
	Found.RemoveAll();

	int nCount = EntityList_GetCount();
	for ( int i = 0; i < nCount; i++ )
	{
		CMapEntity *pEntity = EntityList_GetEntity( i );
		
		if ( pEntity->IsVisible() || !bVisiblesOnly )
		{
			if ( pEntity->NameMatches( pszName ) || pEntity->ClassNameMatches( pszName ) )
			{
				Found.AddToTail( pEntity );
			}
		}
	}

	return( Found.Count() != 0 );
}


//-----------------------------------------------------------------------------
// Tell all our children to update their dependencies because of the given object.
//-----------------------------------------------------------------------------
void CMapWorld::UpdateAllDependencies( CMapClass *pObject )
{
	//
	// Entities need to be put in their proper hash bucket if the name changed.
	//
	CMapEntity *pEntity = dynamic_cast<CMapEntity *>(pObject);
	if ( pEntity )
	{
		int nNewBucket = -1;
		const char *pszName = pEntity->GetKeyValue( "targetname" );
		if ( pszName )
		{
			nNewBucket = EntityBucketForName( pszName );
		}

		int nIndex;
		int nOldBucket = FindEntityBucket( pEntity, &nIndex );

		if ( nOldBucket != nNewBucket )
		{
			// Remove the entity from the hashed list.
			if ( nOldBucket != -1 )
			{
				m_EntityListByName[ nOldBucket ].FastRemove( nIndex );
			}

			// Add the entity back to the hashed list in the proper bucket.
			if ( nNewBucket != -1 )
			{		
				m_EntityListByName[ nNewBucket ].AddToTail( pEntity );
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Returns if this map world is editable.  If it is not part of an instance
//			or manifest, then it is editable.  Otherwise, it lets the owning document
//			determine the editing state.
//-----------------------------------------------------------------------------
bool CMapWorld::IsEditable( void ) 
{
	if ( !m_pOwningDocument )
	{
		return true;
	}
	
	return m_pOwningDocument->IsEditable();
}

