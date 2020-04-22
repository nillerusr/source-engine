//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "stdafx.h"
#include "ChunkFile.h"
#include "SaveInfo.h"
#include "MapClass.h"
#include "MapEntity.h"			// dvs: evil - base knows about the derived class
#include "MapGroup.h"			// dvs: evil - base knows about the derived class
#include "MapWorld.h"			// dvs: evil - base knows about the derived class
#include "GlobalFunctions.h"
#include "MapDoc.h"
#include "VisGroup.h"
#include "mapdefs.h"
#include "tier0/minidump.h"

int CMapAtom::s_nObjectIDCtr = 1;

static CUtlVector<MCMSTRUCT> s_Classes;

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


bool CMapClass::s_bLoadingVMF = false;


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : Type - 
//			pfnNew - 
//-----------------------------------------------------------------------------
CMapClassManager::CMapClassManager(MAPCLASSTYPE Type, CMapClass *(*pfnNew)())
{

	MCMSTRUCT mcms;
	mcms.Type = Type;
	mcms.pfnNew = pfnNew;
	s_Classes.AddToTail(mcms);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CMapClassManager::~CMapClassManager(void)
{
	s_Classes.RemoveAll();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : Type - 
// Output : CMapClass
//-----------------------------------------------------------------------------
CMapClass *CMapClassManager::CreateObject(MAPCLASSTYPE Type)
{
	unsigned uLen = strlen(Type)+1;
	for (int i = s_Classes.Count() - 1; i >= 0; i--)
	{
		MCMSTRUCT &mcms = s_Classes[i];
		if (!memcmp(mcms.Type, Type, uLen))
		{
			return (*mcms.pfnNew)();
		}
	}

	Assert(FALSE);
	return(NULL);
}


//-----------------------------------------------------------------------------
// Purpose: Constructor. Initializes data members.
//-----------------------------------------------------------------------------
CMapClass::CMapClass(void)
{
	m_pSafeObject = CSafeObject<CMapClass>::Create( this );
	
	//
	// The document manages the unique object IDs. Eventually all object construction
	// should be done through the document, eliminating the need for CMapClass to know
	// about CMapDoc.
	//
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if (pDoc != NULL)
	{
		m_nID = pDoc->GetNextMapObjectID();
	}
	else
	{
		m_nID = 0;
	}

	dwKept = 0;
	m_bTemporary = FALSE;

	m_bVisible = true;
	m_bVisible2D = true;
	m_bVisGroupShown = true;
	m_bVisGroupAutoShown = true;
	m_pColorVisGroup = NULL;

	r = g = b = 220;
	m_pParent = NULL;
	m_nRenderFrame = 0;
	m_pEditorKeys = NULL;
	m_Dependents.Purge();
}


//-----------------------------------------------------------------------------
// Purpose: Destructor. Deletes all children.
//-----------------------------------------------------------------------------
CMapClass::~CMapClass(void)
{
	// Delete all of our children.
	m_Children.PurgeAndDeleteElements();

	delete m_pEditorKeys;
	
	// In case any CMapDocs are pointing at us, let them know we're gone.
	m_pSafeObject->m_pObject = NULL;
	
	// Show a warning if anyone is left pointing at us.
	static bool bCheckSafeObjects = true;
	if ( bCheckSafeObjects && m_pSafeObject->GetRefCount() != 1 )
	{
		int ret = AfxMessageBox(	"Warning: a CMapClass is being deleted but is still referenced by a CMapDoc.\n"
									"Please tell a programmer.\n"
									"Click Yes to write a minidump and continue.\n"
									"Click No to ignore.", 
						MB_YESNO );
		
		if ( ret == IDYES )
		{
			WriteMiniDump();
		}
		else if ( ret == IDNO )
		{
			// Ignore it and don't get in here again.
			bCheckSafeObjects = false;
		}		
	}
}


const CSmartPtr< CSafeObject< CMapClass > >& CMapClass::GetSafeObjectSmartPtr()
{
	return m_pSafeObject;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pDependent - 
//-----------------------------------------------------------------------------
void CMapClass::AddDependent(CMapClass *pDependent)
{
	//
	// Never add ourselves to our dependents. It creates a circular dependency
	// which is bad.
	//
	if (pDependent == this)
		return;

	//
	// Don't add the same dependent twice.
	//
	int nIndex = m_Dependents.Find(pDependent);
	if (nIndex != -1)
		return;

	//
	// Also, never add one of our ancestors as a dependent. This too creates a
	// nasty circular dependency.
	//
	bool bIsOurAncestor = false;
	CMapClass *pTestParent = GetParent();
	while (pTestParent != NULL)
	{
		if (pTestParent == pDependent)
		{
			bIsOurAncestor = true;
			break;
		}

		pTestParent = pTestParent->GetParent();
	}

	if (!bIsOurAncestor)
	{
		m_Dependents.AddToTail(pDependent);
		Assert(m_Dependents.Count() < 1000);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Returns a copy of this object. We should never call this implementation
//			since CMapClass cannot be instantiated.
// Input  : bUpdateDependencies - Whether to update object dependencies when copying object pointers.
//-----------------------------------------------------------------------------
CMapClass *CMapClass::Copy(bool bUpdateDependencies)
{
	Assert(FALSE);
	return(NULL);
}


//-----------------------------------------------------------------------------
// Purpose: Turns this object into a duplicate of the given object.
// Input  : pFrom - The object to replicate.
// Output : Returns a pointer to this object.
//-----------------------------------------------------------------------------
CMapClass *CMapClass::CopyFrom(CMapClass *pFrom, bool bUpdateDependencies)
{
	// Copy CMapPoint stuff. dvs: should be in CMapPoint implementation!
	m_Origin = pFrom->m_Origin;
	    
	//
	// Copy CMapClass stuff.
	//
	int nVisGroupCount = pFrom->GetVisGroupCount();
	for (int nVisGroup = 0; nVisGroup < nVisGroupCount; nVisGroup++)
	{
		CVisGroup *pVisGroup = pFrom->GetVisGroup(nVisGroup);
		if (!pVisGroup->IsAutoVisGroup())
		{
			AddVisGroup(pVisGroup);
		}
	}

	//m_bVisible = pFrom->m_bVisible;
	//m_bVisGroupShown = pFrom->m_bVisGroupShown;
	m_bTemporary = pFrom->m_bTemporary;
	m_bVisible2D = pFrom->m_bVisible2D;
	m_nRenderFrame = pFrom->m_nRenderFrame;
	m_CullBox = pFrom->m_CullBox;
	m_BoundingBox = pFrom->m_BoundingBox;
	m_Render2DBox = pFrom->m_Render2DBox;

	r = pFrom->r;
	g = pFrom->g;
	b = pFrom->b;

	m_Dependents.RemoveAll();
	m_Dependents.AddVectorToTail(pFrom->m_Dependents);

	// dvs: should I copy m_pEditorKeys?

	//
	// Don't link to the parent if we're not updating dependencies, just copy the pointer.
	//
	if (bUpdateDependencies)
	{
		UpdateParent( pFrom->GetParent() );
	}
	else
	{
		m_pParent = pFrom->GetParent();
	}

	return(this);
}


//-----------------------------------------------------------------------------
// Purpose: Returns the culling bbox of this object.
// Input  : mins - receives the minima for culling
//			maxs - receives the maxima for culling.
//-----------------------------------------------------------------------------
void CMapClass::GetCullBox(Vector &mins, Vector &maxs)
{
	m_CullBox.GetBounds(mins, maxs);
}


//-----------------------------------------------------------------------------
// Purpose: Initialize the cull box with the bounds of the faces.
//-----------------------------------------------------------------------------
void CMapClass::SetCullBoxFromFaceList( CMapFaceList *pFaces )
{
	SetBoxFromFaceList( pFaces, m_CullBox );
}


//-----------------------------------------------------------------------------
// Purpose: Returns the bounding bbox of this object.
// Input  : mins - receives the minima for culling
//			maxs - receives the maxima for culling.
//-----------------------------------------------------------------------------
void CMapClass::GetBoundingBox( Vector &mins, Vector &maxs )
{
	m_BoundingBox.GetBounds( mins, maxs );
}


//-----------------------------------------------------------------------------
// Purpose: Initialize the bounding box with the bounds of the faces.
//-----------------------------------------------------------------------------
void CMapClass::SetBoundingBoxFromFaceList( CMapFaceList *pFaces )
{
	SetBoxFromFaceList( pFaces, m_BoundingBox );
}


//-----------------------------------------------------------------------------
// Purpose: Initialize box with the bounds of the faces.
//-----------------------------------------------------------------------------
void CMapClass::SetBoxFromFaceList( CMapFaceList *pFaces, BoundBox &Box )
{
	//
	// Calculate our 3D bounds.
	//
	Box.ResetBounds();
	for (int iFace = 0; iFace < pFaces->Count(); iFace++)
	{
		CMapFace *pFace = pFaces->Element( iFace );
		int nPoints = pFace->GetPointCount();
		for (int i = 0; i < nPoints; i++)
		{
			Vector point;
			pFace->GetPoint(point, i);

			//
			// Push the culling box out in all directions.
			// TODO: rotate the culling box based on the cone orientation
			//			
			for (int nDim = 0; nDim < 3; nDim++)
			{
				Box.bmins[0] = min(Box.bmins[0], m_Origin[0] - point[nDim]);
				Box.bmins[1] = min(Box.bmins[1], m_Origin[1] - point[nDim]);
				Box.bmins[2] = min(Box.bmins[2], m_Origin[2] - point[nDim]);

				Box.bmaxs[0] = max(Box.bmaxs[0], m_Origin[0] + point[nDim]);
				Box.bmaxs[1] = max(Box.bmaxs[1], m_Origin[1] + point[nDim]);
				Box.bmaxs[2] = max(Box.bmaxs[2], m_Origin[2] + point[nDim]);
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Returns the bbox for 2D rendering of this object.
//			FIXME: this can be removed if we do all our 2D rendering in this->Render2D.
// Input  : mins - receives the minima for culling
//			maxs - receives the maxima for culling.
//-----------------------------------------------------------------------------
void CMapClass::GetRender2DBox(Vector &mins, Vector &maxs)
{
	m_Render2DBox.GetBounds(mins, maxs);
}


//-----------------------------------------------------------------------------
// Purpose: Returns the number of keys that were loaded from the "editor"
//			section of the VMF. These keys are held until they are handled, then
//			the memory is freed.
//-----------------------------------------------------------------------------
int CMapClass::GetEditorKeyCount(void)
{
	if (m_pEditorKeys == NULL)
	{
		return NULL;
	}

	return m_pEditorKeys->GetCount();
}


//-----------------------------------------------------------------------------
// Purpose: Returns the key name for the given editor key index.
//-----------------------------------------------------------------------------
const char *CMapClass::GetEditorKey(int nIndex)
{
	if (m_pEditorKeys == NULL)
	{
		return NULL;
	}

	return m_pEditorKeys->GetKey(nIndex);
}


//-----------------------------------------------------------------------------
// Purpose: Returns the value for the given editor key index.
//-----------------------------------------------------------------------------
const char *CMapClass::GetEditorKeyValue(int nIndex)
{
	if (m_pEditorKeys == NULL)
	{
		return NULL;
	}

	return m_pEditorKeys->GetValue(nIndex);
}


//-----------------------------------------------------------------------------
// Purpose: Returns the value for the given editor key name.
//			NOTE: this is used for unique keys and will return the value for the
//			FIRST key with the given name.
//-----------------------------------------------------------------------------
const char *CMapClass::GetEditorKeyValue(const char *szKey)
{
	if (m_pEditorKeys == NULL)
	{
		return NULL;
	}

	return m_pEditorKeys->GetValue(szKey);
}

//-----------------------------------------------------------------------------
// Purpose: Begins a depth-first search of the map heirarchy.
// Input  : pos - An iterator 
// Output : CMapClass
//-----------------------------------------------------------------------------
CMapClass *CMapClass::GetFirstDescendent(EnumChildrenPos_t &pos)
{
	pos.nDepth = 0;
	pos.Stack[0].pParent = this;

	if ( m_Children.Count() )
	{
		pos.Stack[0].pos = 0;
		return(GetNextDescendent(pos));
	}
	else
	{
		pos.Stack[0].pos = -1;
		return NULL;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Continues a depth-first search of the map heirarchy.
// Input  : &pos - 
// Output : CMapClass
//-----------------------------------------------------------------------------
CMapClass *CMapClass::GetNextDescendent(EnumChildrenPos_t &pos)
{
	while (pos.nDepth >= 0)
	{
		while (pos.Stack[pos.nDepth].pos != -1)
		{
			//
			// Get the next child of the parent on top of the stack.
			//
			CMapClass *pParent = pos.Stack[pos.nDepth].pParent;
			CMapClass *pChild = pParent->m_Children[pos.Stack[pos.nDepth].pos];
			pos.Stack[pos.nDepth].pos++;

			if ( pos.Stack[pos.nDepth].pos == pParent->m_Children.Count() )
				pos.Stack[pos.nDepth].pos= -1;


			// If this object has children, push it onto the stack.

			if ( pChild->m_Children.Count() )
			{
				pos.nDepth++;

				if (pos.nDepth < MAX_ENUM_CHILD_DEPTH)
				{
					pos.Stack[pos.nDepth].pParent = pChild;
					pos.Stack[pos.nDepth].pos = 0;
				}
				else
				{
					// dvs: stack overflow!
					pos.nDepth--;
				}
			}
			//
			// If this object has no children, return it.
			//
			else
			{
				return(pChild);
			}
		}
		
		//
		// Finished with this object's children, pop the stack and return the object.
		//
		pos.nDepth--;
		if (pos.nDepth >= 0)
		{
			return(pos.Stack[pos.nDepth + 1].pParent);
		}
	}

	return(NULL);
}


//-----------------------------------------------------------------------------
// Purpose: Returns the world object that the given object belongs to.
// Input  : pStart - Object to traverse up from to find the world object.
//-----------------------------------------------------------------------------
CMapWorld *CMapClass::GetWorldObject(CMapAtom *pStart)
{
	CMapAtom *pObject = pStart;

	while (pObject != NULL)
	{
		if ( IsWorldObject( pObject ) )
		{
			return (CMapWorld*)pObject;
		}
		pObject = pObject->GetParent();
	}

	// has no world:
	return NULL;
}


BOOL CMapClass::IsChildOf(CMapAtom *pObject)
{
	CMapAtom *pParent = m_pParent;

	while( pParent )
	{
		if( pParent == pObject )
			return TRUE;

		if( IsWorldObject(pParent) )
			return FALSE;	// world object, not parent .. return false.

		pParent = pParent->GetParent();
	}

	return FALSE;
}


//-----------------------------------------------------------------------------
// Purpose: Returns whether this object belongs to the given visgroup.
// Input  : pVisGroup - 
//-----------------------------------------------------------------------------
int CMapClass::IsInVisGroup(CVisGroup *pVisGroup)
{
	if (pVisGroup != NULL)
	{
		if ( m_VisGroups.Find( pVisGroup ) != -1 )
		{
			return 1;
		}
		else
		{
			return 0;
		}
	}
		
	return 0;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true if the color was specified by this call, false if not.
//-----------------------------------------------------------------------------
bool CMapClass::UpdateObjectColor(void)
{
	//
	// The user can choose a visgroup from which to get the color from.
	// If one was chosen, set our color from that visgroup.
	//
	if (m_pColorVisGroup)
	{
		color32 rgbColor = m_pColorVisGroup->GetColor();
		SetRenderColor(rgbColor);
		return true;
	}
	else if (m_pParent && !IsWorldObject(m_pParent))
	{
		color32 rgbColor = m_pParent->GetRenderColor();
		SetRenderColor(rgbColor);
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Sets the visgroup that this object gets its color from.
//-----------------------------------------------------------------------------
void CMapClass::SetColorVisGroup(CVisGroup *pVisGroup)
{
	m_pColorVisGroup = pVisGroup;
	UpdateObjectColor();
}


//-----------------------------------------------------------------------------
// Purpose: Adds the given visgroup to the list of visgroups that this object
//			belongs to.
//-----------------------------------------------------------------------------
void CMapClass::AddVisGroup(CVisGroup *pVisGroup)
{
	if (m_VisGroups.Find(pVisGroup) == -1)
	{
		m_VisGroups.AddToTail(pVisGroup);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Removes the given visgroup from the list of visgroups that this object
//			belongs to.
//-----------------------------------------------------------------------------
void CMapClass::RemoveVisGroup(CVisGroup *pVisGroup)
{
	int nIndex = m_VisGroups.Find(pVisGroup);
	
	if (nIndex != -1 )
	{
		m_VisGroups.FastRemove(nIndex);
		CheckVisibility();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CMapClass::GetVisGroupCount(void)
{
	return m_VisGroups.Count();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CVisGroup *CMapClass::GetVisGroup(int nIndex)
{
	return m_VisGroups.Element(nIndex);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapClass::RemoveAllVisGroups(void)
{
	m_VisGroups.RemoveAll();

	// Remove all visgroups from children as well.
	FOR_EACH_OBJ( m_Children, pos )
	{
		CMapClass *pChild = m_Children.Element(pos);
		pChild->RemoveAllVisGroups();
	}

	// Not in any visgroups; can't be hidden that way.
	VisGroupShow(true);
}


//-----------------------------------------------------------------------------
// Purpose: Adds the specified child to this object.
// Input  : pChild - Object to add as a child of this object.
//-----------------------------------------------------------------------------
void CMapClass::AddChild(CMapClass *pChild)
{
	if ( m_Children.Find(pChild) != -1 )
	{
		pChild->m_pParent = this;
		return;
	}

	m_Children.AddToTail(pChild);
	pChild->m_pParent = this;

	//
	// Update our bounds with the child's bounds.
	//
	Vector vecMins;
	Vector vecMaxs;

	pChild->GetCullBox(vecMins, vecMaxs);
	m_CullBox.UpdateBounds(vecMins, vecMaxs);

	pChild->GetBoundingBox( vecMins, vecMaxs );
	m_BoundingBox.UpdateBounds( vecMins, vecMaxs );

	pChild->GetRender2DBox(vecMins, vecMaxs);
	m_Render2DBox.UpdateBounds(vecMins, vecMaxs);

	if (m_pParent != NULL)
	{
		GetParent()->UpdateChild(this);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Removes all of this object's children.
//-----------------------------------------------------------------------------
void CMapClass::RemoveAllChildren(void)
{
	//
	// Detach the children from us. They are no longer in our world heirarchy.
	//
	FOR_EACH_OBJ( m_Children, pos )
	{	
		m_Children[pos]->m_pParent = NULL;
	}	

	//
	// Remove them from our list.
	//
	m_Children.RemoveAll();
}


//-----------------------------------------------------------------------------
// Purpose: Removes the specified child from this object.
// Input  : pChild - The child to remove.
//			bUpdateBounds - TRUE to calculate new bounds, FALSE not to.
//-----------------------------------------------------------------------------
void CMapClass::RemoveChild(CMapClass *pChild, bool bUpdateBounds)
{
	int index = m_Children.Find(pChild);

	if (index == -1)
	{
		pChild->m_pParent = NULL;
		return;
	}

	m_Children.Remove(index);
	pChild->m_pParent = NULL;

	if (bUpdateBounds)
	{
		PostUpdate(Notify_Removed);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Copies all children of a given object as children of this object.
//			NOTE: The child objects are replicated, not merely added as children.
// Input  : pobj - The object whose children are to be copied.
//-----------------------------------------------------------------------------
void CMapClass::CopyChildrenFrom(CMapClass *pobj, bool bUpdateDependencies)
{
	FOR_EACH_OBJ( pobj->m_Children, pos )
	{
		CMapClass *pChild = pobj->m_Children.Element(pos);
		CMapClass *pChildCopy = pChild->Copy(bUpdateDependencies);
		pChildCopy->CopyChildrenFrom(pChild, bUpdateDependencies);
		AddChild(pChildCopy);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Recalculate's this object's bounding boxes. CMapClass-derived classes
//			should call this first, then update using their local data.
// Input  : bFullUpdate - When set to TRUE, call CalcBounds on all children
//				before updating our bounds.
//-----------------------------------------------------------------------------
void CMapClass::CalcBounds(BOOL bFullUpdate)
{
	if ( CMapClass::s_bLoadingVMF )
		return;
		
	m_CullBox.ResetBounds();
	m_BoundingBox.ResetBounds();
	m_Render2DBox.ResetBounds();

	FOR_EACH_OBJ( m_Children, pos )
	{
		CMapClass *pChild = m_Children.Element(pos);
		if (bFullUpdate)
		{
			pChild->CalcBounds(TRUE);
		}

		m_CullBox.UpdateBounds(&pChild->m_CullBox);
		m_BoundingBox.UpdateBounds(&pChild->m_BoundingBox);
		m_Render2DBox.UpdateBounds(&pChild->m_Render2DBox);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Sets the render color of this object and all its children.
// Input  : uchRed, uchGreen, uchBlue - Color components.
//-----------------------------------------------------------------------------
void CMapClass::SetRenderColor(color32 rgbColor)
{
	CMapAtom::SetRenderColor(rgbColor);

	//
	// Set the render color of all our children.
	//
	FOR_EACH_OBJ( m_Children, pos )
	{
		CMapClass *pChild = m_Children.Element(pos);
		if (pChild != NULL)
		{
			pChild->SetRenderColor(rgbColor);
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Sets the render color of this object and all its children.
// Input  : uchRed, uchGreen, uchBlue - Color components.
//-----------------------------------------------------------------------------
void CMapClass::SetRenderColor(unsigned char uchRed, unsigned char uchGreen, unsigned char uchBlue)
{
	CMapAtom::SetRenderColor(uchRed, uchGreen, uchBlue);

	//
	// Set the render color of all our children.
	//
	FOR_EACH_OBJ( m_Children, pos )
	{
		CMapClass *pChild = m_Children.Element(pos);
		if (pChild != NULL)
		{
			pChild->SetRenderColor(uchRed, uchGreen, uchBlue);
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Returns a pointer to the object that should be added to the selection
//			list because this object was clicked on with a given selection mode.
// Input  : eSelectMode - 
//-----------------------------------------------------------------------------
CMapClass *CMapClass::PrepareSelection(SelectMode_t eSelectMode)
{
	if ((eSelectMode == selectGroups) && (m_pParent != NULL) && !IsWorldObject(m_pParent))
	{
		return GetParent()->PrepareSelection(eSelectMode);
	}

	return this;
}


//-----------------------------------------------------------------------------
// Purpose: Calls an enumerating function for each of our children that are of
//			of a given type, recursively enumerating their children also.
// Input  : pfn - Enumeration callback function. Called once per child.
//			dwParam - User data to pass into the enumerating callback.
//			Type - Unless NULL, only objects of the given type will be enumerated.
// Output : Returns FALSE if the enumeration was terminated early, TRUE if it completed.
//-----------------------------------------------------------------------------
BOOL CMapClass::EnumChildren(ENUMMAPCHILDRENPROC pfn, unsigned int dwParam, MAPCLASSTYPE Type)
{
	FOR_EACH_OBJ( m_Children, pos )
	{
		CMapClass *pChild = m_Children.Element(pos);
		if (!Type || pChild->IsMapClass(Type))
		{
			if(!(*pfn)(pChild, dwParam))
			{
				return FALSE;
			}
		}

		// enum this child's children
		if (!pChild->EnumChildren(pfn, dwParam, Type))
		{
			return FALSE;
		}
	}

	return TRUE;
}


//-----------------------------------------------------------------------------
// Purpose: Enumerates a this object's children, only recursing into groups.
//			Children of entities will not be enumerated.
// Input  : pfn - Enumeration callback function. Called once per child.
//			dwParam - User data to pass into the enumerating callback.
//			Type - Unless NULL, only objects of the given type will be enumerated.
// Output : Returns FALSE if the enumeration was terminated early, TRUE if it completed.
//-----------------------------------------------------------------------------
BOOL CMapClass::EnumChildrenRecurseGroupsOnly(ENUMMAPCHILDRENPROC pfn, unsigned int dwParam, MAPCLASSTYPE Type)
{
	FOR_EACH_OBJ( m_Children, pos )
	{
		CMapClass *pChild = m_Children.Element(pos);

		if (!Type || pChild->IsMapClass(Type))
		{
			if (!(*pfn)(pChild, dwParam))
			{
				return FALSE;
			}
		}

		if (pChild->IsGroup())
		{
			if (!pChild->EnumChildrenRecurseGroupsOnly(pfn, dwParam, Type))
			{
				return FALSE;
			}
		}
	}

	return TRUE;
}


//-----------------------------------------------------------------------------
// Purpose: Iterates through an object, and all it's children, looking for an
//			entity with a matching key and value
// Input  : key - 
//			value - 
// Output : CMapEntity - the entity found
//-----------------------------------------------------------------------------
CMapEntity *CMapClass::FindChildByKeyValue( const char* key, const char* value, bool *bIsInInstance, VMatrix *InstanceMatrix )
{
	if ( !key || !value )
		return NULL;

	FOR_EACH_OBJ( m_Children, pos )
	{
		CMapClass *pChild = m_Children.Element( pos );
		CMapEntity *e = pChild->FindChildByKeyValue( key, value, bIsInInstance, InstanceMatrix );
		if ( e )
			return e;
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Called after this object is added to the world.
//
//			NOTE: This function is NOT called during serialization. Use PostloadWorld
//				  to do similar bookkeeping after map load.
//
// Input  : pWorld - The world that we have been added to.
//-----------------------------------------------------------------------------
void CMapClass::OnAddToWorld(CMapWorld *pWorld)
{
	//
	// Notify all our children.
	//
	FOR_EACH_OBJ( m_Children, pos )
	{
		CMapClass *pChild = m_Children.Element(pos);
		pChild->OnAddToWorld(pWorld);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Called to notify the object that it has just been cloned
//			iterates through and notifies all the children of their cloned state
//			NOTE: assumes that the children are in the same order in both the
//			original and the clone
// Input  : pNewObj - the clone of this object
//			OriginalList - The list of objects that were cloned
//			NewList - The parallel list of clones of objects in OriginalList
//-----------------------------------------------------------------------------
void CMapClass::OnClone( CMapClass *pNewObj, CMapWorld *pWorld, const CMapObjectList &OriginalList, CMapObjectList &NewList )
{
	Assert( m_Children.Count() == pNewObj->m_Children.Count() );
	
	FOR_EACH_OBJ( m_Children, pos )
	{
		CMapClass *pChild = m_Children.Element( pos );
		CMapClass *pNewChild = pNewObj->m_Children.Element( pos );
		pChild->OnClone( pNewChild, pWorld, OriginalList, NewList );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Called to notify the object that it has just been cloned
//			iterates through and notifies all the children of their cloned state
//			NOTE: assumes that the children are in the same order in both the
//			original and the clone
// Input  : pNewObj - the clone of this object
//			OriginalList - The list of objects that were cloned
//			NewList - The parallel list of clones of objects in OriginalList
//-----------------------------------------------------------------------------
void CMapClass::OnPreClone( CMapClass *pNewObj, CMapWorld *pWorld, const CMapObjectList &OriginalList, CMapObjectList &NewList )
{
	Assert( m_Children.Count() == pNewObj->m_Children.Count() );
	
	FOR_EACH_OBJ( m_Children, pos )
	{
		CMapClass *pChild = m_Children.Element( pos );
		CMapClass *pNewChild = pNewObj->m_Children.Element( pos );
		pChild->OnPreClone( pNewChild, pWorld, OriginalList, NewList );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Notifies this object that a copy of itself is about to be pasted.
//			Allows the object to generate new unique IDs in the copy of itself.
// Input  : pCopy - 
//			pSourceWorld - 
//			pDestWorld - 
//			OriginalList - 
//			NewList - 
//-----------------------------------------------------------------------------
void CMapClass::OnPrePaste(CMapClass *pCopy, CMapWorld *pSourceWorld, CMapWorld *pDestWorld, const CMapObjectList &OriginalList, CMapObjectList &NewList)
{
	Assert( m_Children.Count() == pCopy->m_Children.Count() );

	FOR_EACH_OBJ( m_Children, pos )
	{
		CMapClass *pChild = m_Children.Element(pos);
		CMapClass *pCopyChild = pCopy->m_Children.Element(pos);

		pChild->OnPrePaste(pCopyChild, pSourceWorld, pDestWorld, OriginalList, NewList);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Notifies this object that a copy of itself is being pasted.
//			Allows the object to fixup any references to other objects in the
//			clipboard with references to their copies.
// Input  : pCopy - 
//			pSourceWorld - 
//			pDestWorld - 
//			OriginalList - 
//			NewList - 
//-----------------------------------------------------------------------------
void CMapClass::OnPaste(CMapClass *pCopy, CMapWorld *pSourceWorld, CMapWorld *pDestWorld, const CMapObjectList &OriginalList, CMapObjectList &NewList)
{
	Assert( m_Children.Count() == pCopy->m_Children.Count() );

	FOR_EACH_OBJ( m_Children, pos )
	{
		CMapClass *pChild = m_Children.Element(pos);
		CMapClass *pCopyChild = pCopy->m_Children.Element(pos);

		pChild->OnPaste(pCopyChild, pSourceWorld, pDestWorld, OriginalList, NewList);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Called just after this object has been removed from the world so
//			that it can unlink itself from other objects in the world.
// Input  : pWorld - The world that we were just removed from.
//			bNotifyChildren - Whether we should forward notification to our children.
//-----------------------------------------------------------------------------
void CMapClass::OnRemoveFromWorld(CMapWorld *pWorld, bool bNotifyChildren)
{
	//
	// Since we are being removed from the world, we cannot have any dependents.
	// Notify any dependent objects, so they can release pointers to us.
	// Our dependencies will be regenerated if we are added back into the world.
	//
	NotifyDependents(Notify_Removed);
	m_Dependents.RemoveAll();

	if (bNotifyChildren)
	{
		FOR_EACH_OBJ( m_Children, pos )
		{
			CMapClass *pChild = m_Children.Element(pos);
			pChild->OnRemoveFromWorld(pWorld, true);
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Called after a map file has been completely loaded.
// Input  : pWorld - The world that we are in.
//-----------------------------------------------------------------------------
void CMapClass::PostloadWorld(CMapWorld *pWorld)
{
	FOR_EACH_OBJ( m_Children, pos )
	{
		CMapClass *pChild = m_Children.Element(pos);
		pChild->PostloadWorld(pWorld);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called after all visgroups have been completely loaded.  Checks for 
//			objects hidden but without a visgroup.
// Input  : void
//-----------------------------------------------------------------------------
bool CMapClass::PostloadVisGroups( bool bLoading )
{
	FOR_EACH_OBJ( m_Children, pos )
	{
		CMapClass *pChild = m_Children.Element(pos);
		pChild->PostloadVisGroups( bLoading);
	}
	return CheckVisibility( bLoading );
}

//-----------------------------------------------------------------------------
// Purpose: Calls RenderPreload for each of our children. This allows them to
//			cache any resources that they need for rendering.
// Input  : pRender - Pointer to the 3D renderer.
//-----------------------------------------------------------------------------
bool CMapClass::RenderPreload(CRender3D *pRender, bool bNewContext)
{
	FOR_EACH_OBJ( m_Children, pos )
	{
		CMapClass *pChild = m_Children.Element(pos);
		pChild->RenderPreload(pRender, bNewContext);
	}

	return(true);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pRender - 
//-----------------------------------------------------------------------------
void CMapClass::Render2D(CRender2D *pRender)
{
// This is not needed because the recursion is performed in CMapView2D::Render
//	POSITION pos = Children.GetHeadPosition();
//	while (pos != NULL)
//	{
//		CMapClass *pChild = Children.GetNext(pos);
//		if (pChild->IsVisible() && pChild->IsVisible2D())
//		{
//			pChild->Render2D(pRender);
//		}
//	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pRender - 
//-----------------------------------------------------------------------------
void CMapClass::Render3D(CRender3D *pRender)
{
}


//-----------------------------------------------------------------------------
// Purpose: Transforms all children. Derived implementations should call this,
//			then do their own thing.
// Input  : t - Pointer to class containing transformation information.
//-----------------------------------------------------------------------------
void CMapClass::DoTransform(const VMatrix &matrix)
{
	CMapPoint::DoTransform(matrix);

	FOR_EACH_OBJ( m_Children, pos )
	{
		CMapClass *pChild = m_Children.Element(pos);
		pChild->Transform( matrix );
	}
}


//-----------------------------------------------------------------------------
// Default logical box 
//-----------------------------------------------------------------------------
void CMapClass::GetRenderLogicalBox( Vector2D &mins, Vector2D &maxs ) 
{ 
	mins.Init( COORD_NOTINIT, COORD_NOTINIT ); 
	maxs.Init( COORD_NOTINIT, COORD_NOTINIT ); 
}

const Vector2D& CMapClass::GetLogicalPosition( )
{
	static Vector2D pos( COORD_NOTINIT, COORD_NOTINIT ); 
	return pos; 
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
size_t CMapClass::GetSize(void)
{
	return(sizeof(*this));
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CMapClass::HitTest2D(CMapView2D *pView, const Vector2D &point, HitInfo_t &HitData)
{
	HitData.pObject = NULL;
	HitData.nDepth = g_MAX_MAP_COORD*3;
	HitData.uData = 0;
	bool bFoundHit = false;
	
	if ( !IsVisible() )
		return false;

	FOR_EACH_OBJ( m_Children, pos )
	{
		CMapClass *pChild = m_Children.Element(pos);

		HitInfo_t testHitData;

		if ( pChild->HitTest2D(pView, point, testHitData) )
		{
			Assert( testHitData.pObject != NULL );

			if ( testHitData.nDepth < HitData.nDepth )
			{
				HitData = testHitData;
				bFoundHit = true;
			}
		}
	}

	return bFoundHit;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CMapClass::HitTestLogical(CMapViewLogical *pView, const Vector2D &point, HitInfo_t &hitData)
{
	if ( !IsVisibleLogical() )
		return false;

	FOR_EACH_OBJ( m_Children, pos )
	{
		CMapClass *pChild = m_Children.Element(pos);
		if ( pChild->HitTestLogical(pView, point, hitData) )
			return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Sets the selection state of this object's children.
// Input  : eSelectionState - 
//-----------------------------------------------------------------------------
SelectionState_t CMapClass::SetSelectionState(SelectionState_t eSelectionState)
{
	FOR_EACH_OBJ( m_Children, pos )
	{
		CMapAtom *pObject = m_Children.Element(pos);
		pObject->SetSelectionState(eSelectionState);
	}

	return CMapAtom::SetSelectionState(eSelectionState);
}


//-----------------------------------------------------------------------------
// Purpose: Our child's bounding box has changed - notify our parent. The real
//			work will be done in CMapWorld::UpdateChild.
// Input  : pChild - The child whose bounding box changed.
//-----------------------------------------------------------------------------
void CMapClass::UpdateChild(CMapClass *pChild)
{
	if (m_pParent != NULL)
	{
		GetParent()->UpdateChild(this);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Returns a coordinate frame to render in
// Input  : matrix - 
// Output : returns true if a new matrix is returned, false if it is invalid
//-----------------------------------------------------------------------------
bool CMapClass::GetTransformMatrix( VMatrix& matrix )
{
	// try and get our parents transform matrix
	CMapClass *p = CMapClass::GetParent();
	if ( p )
	{
		return p->GetTransformMatrix( matrix );
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pLoadInfo - 
//			pWorld - 
// Output : 
//-----------------------------------------------------------------------------
ChunkFileResult_t CMapClass::LoadEditorCallback(CChunkFile *pFile, CMapClass *pObject)
{
	return(pFile->ReadChunk((KeyHandler_t)LoadEditorKeyCallback, pObject));
}


//-----------------------------------------------------------------------------
// Purpose: Handles keyvalues when loading the editor chunk of an object from the
//			MAP file. Keys are transferred to a special keyvalue list for use after
//			the entire map has been loaded.
// Input  : szKey - Key to handle.
//			szValue - Value of key.
//			pObject - Object being loaded.
// Output : Returns ChunkFile_Ok.
//-----------------------------------------------------------------------------
ChunkFileResult_t CMapClass::LoadEditorKeyCallback(const char *szKey, const char *szValue, CMapClass *pObject)
{
	if (!stricmp(szKey, "color"))
	{
		CChunkFile::ReadKeyValueColor(szValue, pObject->r, pObject->g, pObject->b);
	}
	else if (!stricmp(szKey, "id"))
	{
		CChunkFile::ReadKeyValueInt(szValue, pObject->m_nID);
	}
	else  if (!stricmp(szKey, "comments"))
	{
		//
		// Load the object comments.
		// HACK: upcast to CEditGameClass *
		//
		CEditGameClass *pEdit = dynamic_cast <CEditGameClass *> (pObject);
		if (pEdit != NULL)
		{
			pEdit->SetComments(szValue);
		}
	}
	else if (!stricmp(szKey, "visgroupshown"))
	{
		CChunkFile::ReadKeyValueBool(szValue, pObject->m_bVisGroupShown);
	}
	else if ( !stricmp(szKey, "visgroupautoshown") )
	{
		CChunkFile::ReadKeyValueBool(szValue, pObject->m_bVisGroupAutoShown);
	}
	else
	{
		pObject->SetEditorKeyValue(szKey, szValue);
	}

	return(ChunkFile_Ok);
}


//-----------------------------------------------------------------------------
// Purpose: Call this function after changing this object via transformation,
//			etc. Notifies dependents and updates the parent with this object's
//			new size.
//-----------------------------------------------------------------------------
void CMapClass::PostUpdate(Notify_Dependent_t eNotifyType)
{
	if (m_pParent != NULL)
	{
		GetParent()->UpdateChild(this);
	}
	else if (eNotifyType != Notify_Removed)
	{
		CalcBounds(TRUE);
	}

	NotifyDependents(eNotifyType);
}


//-----------------------------------------------------------------------------
// Purpose: Notifies all our dependents that something about us has changed,
//			giving them the chance to update themselves.
//-----------------------------------------------------------------------------
void CMapClass::NotifyDependents(Notify_Dependent_t eNotifyType)
{
	Assert(m_Dependents.Count() < 1000);

	if (m_Dependents.Count() != 0)
	{
		CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
		if (pDoc)
		{
			pDoc->NotifyDependents(this, eNotifyType);
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Informs us that an object that we are dependent upon has changed,
//			giving us the opportunity to update ourselves accordingly.
// Input  : pObject - Object that we are dependent upon that has changed.
//-----------------------------------------------------------------------------
void CMapClass::OnNotifyDependent(CMapClass *pObject, Notify_Dependent_t eNotifyType)
{
}


//-----------------------------------------------------------------------------
// Purpose: Default implementation for saving editor-specific data. Does nothing.
// Input  : pFile - 
// Output : ChunkFileResult_t
//-----------------------------------------------------------------------------
ChunkFileResult_t CMapClass::SaveEditorData(CChunkFile *pFile)
{
	return(ChunkFile_Ok);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pFile - 
// Output : ChunkFileResult_t
//-----------------------------------------------------------------------------
ChunkFileResult_t CMapClass::SaveVMF(CChunkFile *pFile, CSaveInfo *pSaveInfo)
{
	//
	// Write the editor chunk.
	//
	ChunkFileResult_t eResult = pFile->BeginChunk("editor");

	//
	// Save the object's color.
	//
	if (eResult == ChunkFile_Ok)
	{
		eResult = pFile->WriteKeyValueColor("color", r, g, b);
	}

	//
	// Save the group ID, if any.
	//
	if (eResult == ChunkFile_Ok)
	{
		CMapGroup *pGroup = dynamic_cast<CMapGroup *>(m_pParent);
		if (pGroup != NULL)
		{
			eResult = pFile->WriteKeyValueInt("groupid", pGroup->GetID());
		}
	}

	//
	// Save the visgroup IDs, if any.
	//
	if (m_VisGroups.Count())
	{
		if ((eResult == ChunkFile_Ok) && m_VisGroups.Count())
		{
			for (int i = 0; i < m_VisGroups.Count(); i++)
			{
				CVisGroup *pVisGroup = m_VisGroups.Element(i);
				if ( !pVisGroup->IsAutoVisGroup() )
				{
					eResult = pFile->WriteKeyValueInt("visgroupid", pVisGroup->GetID());
					if (eResult != ChunkFile_Ok)
					{
						break;
					}
				}
			}
		}
	}

	if (eResult == ChunkFile_Ok)
	{
		eResult = pFile->WriteKeyValueBool("visgroupshown", m_bVisGroupShown);
	}

	if (eResult == ChunkFile_Ok)
	{
		eResult = pFile->WriteKeyValueBool("visgroupautoshown", m_bVisGroupAutoShown);
	}

	//
	// Save the object comments, if any.
	// HACK: upcast to CEditGameClass *
	//
	CEditGameClass *pEdit = dynamic_cast <CEditGameClass *> (this);
	if (pEdit != NULL)
	{
		if ((eResult == ChunkFile_Ok) && (strlen(pEdit->GetComments()) > 0))
		{
			eResult = pFile->WriteKeyValue("comments", pEdit->GetComments());
		}
	}

	//
	// Save any other editor-specific data.
	//
	if (eResult == ChunkFile_Ok)
	{
		eResult = SaveEditorData(pFile);
	}

	if (eResult == ChunkFile_Ok)
	{
		eResult = pFile->EndChunk();
	}

	return(eResult);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pDependent - 
//-----------------------------------------------------------------------------
void CMapClass::RemoveDependent(CMapClass *pDependent)
{
	int nIndex = m_Dependents.Find(pDependent);
	if (nIndex != -1)
	{
		m_Dependents.FastRemove(nIndex);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Frees all the keys that were loaded from the editor chunk of the MAP file.
//-----------------------------------------------------------------------------
void CMapClass::RemoveEditorKeys(void)
{
	delete m_pEditorKeys;
	m_pEditorKeys = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *szOldName - 
//			*szNewName - 
//-----------------------------------------------------------------------------
void CMapClass::ReplaceTargetname(const char *szOldName, const char *szNewName)
{
	FOR_EACH_OBJ( m_Children, pos )
	{
		CMapClass *pObject = m_Children.Element(pos);
		pObject->ReplaceTargetname(szOldName, szNewName);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Updates an object attachment, making this object no longer dependent
//			on changes to the old object, and dependent on changes to the new object.
// Input  : pOldAttached - Object that this object was attached to (possibly NULL).
//			pNewAttached - New object being attached to (possibly NULL).
// Output : Returns pNewAttached.
//-----------------------------------------------------------------------------
CMapClass *CMapClass::UpdateDependency(CMapClass *pOldAttached, CMapClass *pNewAttached)
{
	if (pOldAttached != pNewAttached)
	{
		//
		// If we were attached to another object via this pointer, detach us now.
		//
		if (pOldAttached != NULL)
		{
			pOldAttached->RemoveDependent(this);
		}

		//
		// Attach ourselves as a dependent of the other object. We will now be notified
		// of any changes to that object.
		//
		if (pNewAttached != NULL)
		{
			pNewAttached->AddDependent(this);
		}
	}

	return(pNewAttached);
}


//-----------------------------------------------------------------------------
// Purpose: Updates this object's parent, removing it from it's old parent (if any)
//			attaching it to the new parent (if any).
// Input  : pNewParent - A pointer to the new parent for this object.
// Output : Returns a pointer to the new parent.
//-----------------------------------------------------------------------------
void CMapClass::UpdateParent(CMapClass *pNewParent)
{
	CMapClass *pOldParent = GetParent();

	if (pOldParent != pNewParent)
	{
		if (pOldParent != NULL)
		{
			pOldParent->RemoveChild(this);
		}

		if (pNewParent != NULL)
		{
			pNewParent->AddChild(this);
		}

		m_pParent = pNewParent;

		UpdateObjectColor();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *szKey - 
// Output : const char
//-----------------------------------------------------------------------------
void CMapClass::SetEditorKeyValue(const char *szKey, const char *szValue)
{
	if (m_pEditorKeys == NULL)
	{
		m_pEditorKeys = new WCKeyValuesVector;
	}

	Assert( m_pEditorKeys != NULL );
	
	m_pEditorKeys->AddKeyValue(szKey, szValue);
}


//-----------------------------------------------------------------------------
// Purpose: Sets the origin of this object and its children.
//			FIXME: Should our children necessarily have the same origin as us?
//				   Seems like we should translate our children by our origin delta
//-----------------------------------------------------------------------------
void CMapClass::SetOrigin( Vector &origin )
{
	CMapPoint::SetOrigin( origin );
 
	FOR_EACH_OBJ( m_Children, pos )
	{
		CMapClass *pChild = m_Children.Element( pos );
		pChild->SetOrigin( origin );
	}

	PostUpdate(Notify_Changed);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bVisible - 
//-----------------------------------------------------------------------------
void CMapClass::SetVisible(bool bVisible)
{
	FOR_EACH_OBJ( m_Children, pos )
	{
		CMapClass *pChild = m_Children.Element(pos);
		pChild->SetVisible(bVisible);
	}

	m_bVisible = bVisible;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bShow - 
//-----------------------------------------------------------------------------
void CMapClass::VisGroupShow(bool bShow, VisGroupSelection eVisGroup) 
{
	FOR_EACH_OBJ( m_Children, pos )
	{
		CMapClass *pChild = m_Children.Element(pos);
		pChild->VisGroupShow(bShow, eVisGroup);
	}

	if ( eVisGroup == AUTO )
	{
		m_bVisGroupAutoShown = bShow;
	}		
	if ( eVisGroup == USER )
	{
		//since user visgroup visibility has precedence over auto, it is possible to change an object's auto
		//visibility through an action in a user visgroup.
		if ( bShow )
		{
			m_bVisGroupAutoShown = bShow;
		}
		m_bVisGroupShown = bShow;

	}
}


//-----------------------------------------------------------------------------
// Purpose: Causes all objects in the world to update any object dependencies (pointers)
//			that they might be holding. This is a static function.
//-----------------------------------------------------------------------------
void CMapClass::UpdateAllDependencies(CMapClass *pObject)
{
	//
	// Try to locate the world object.
	//
	CMapWorld *pWorld;
	if (pObject == NULL)
	{
		CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
		if ((pDoc == NULL) || (pDoc->IsLoading()))
		{
			return;
		}
		
		pWorld = pDoc->GetMapWorld();
	}
	else
	{
		pWorld = pObject->GetWorldObject(pObject);
	}

	if (pWorld == NULL)
	{
		return;
	}

	pWorld->UpdateAllDependencies( pObject );

	EnumChildrenPos_t pos;
	CMapClass *pChild = pWorld->GetFirstDescendent( pos );
	while ( pChild != NULL )
	{
		pChild->UpdateDependencies( pWorld, pObject );
		pChild = pWorld->GetNextDescendent( pos );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Returns whether this object should be hidden based on the given
//			cordon bounds.
// Output : Returns true to cull the object, false to keep it.
//-----------------------------------------------------------------------------
bool CMapClass::IsCulledByCordon(const Vector &vecMins, const Vector &vecMaxs)
{
	return !IsIntersectingBox(vecMins, vecMaxs);
}

//-----------------------------------------------------------------------------
// Purpose: Checks to see if the object is hidden by auto or user visgroups
//			without being assigned to one.  This solves the problem of objects
//			being destructively hidden by obsolete visgroups.
//-----------------------------------------------------------------------------
bool CMapClass::CheckVisibility( bool bLoading )
{
	CVisGroup* pVisGroup;
	bool bInUser = false;
	bool bInAuto = false;
	int nVisGroupCount = m_VisGroups.Count();
	bool bFoundOrphans = false;
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	
	for ( int i = 0; i < nVisGroupCount; i++ )
	{
		pVisGroup = m_VisGroups.Element( i );
		if ( pVisGroup->IsAutoVisGroup() )
		{
			bInAuto = true;
		}
		else
		{
			bInUser = true;
		}
	}
	if ( !bInAuto && !m_bVisGroupAutoShown )
	{	
		VisGroupShow( true, AUTO );			
	}		
	if ( !bInUser && !m_bVisGroupShown )
	{
		VisGroupShow( true, USER );
		if ( bLoading && pDoc->VisGroups_ObjectCanBelongToVisGroup( this ) )
		{
			//if this object is an orphan, we want it to be hidden but placed in a new visgroup.			
			bFoundOrphans = true;						
			VisGroupShow( false, USER );
		}				
	}

	return bFoundOrphans;
}


//-----------------------------------------------------------------------------
// Purpose: this routine will indicate if the object is editable.  Generally it
//			will not be editable if it is located in a separate instance or
//			submap.
//-----------------------------------------------------------------------------
bool CMapClass::IsEditable( void ) 
{ 
	if ( GetParent() )
	{
		return GetParent()->IsEditable();
	}
	
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: this function will notify all children that the instance they belong to has been moved.
//			it will also notify dependents of a translation.  this function is currently not
//			used but may be.
//-----------------------------------------------------------------------------
void CMapClass::InstanceMoved( void )
{
#if 0
	FOR_EACH_OBJ( m_Children, pos )
	{
		CMapClass *pChild = m_Children.Element(pos);
		pChild->InstanceMoved();
	}

	CMapWorld *pThisWorld = GetWorldObject( this );

	for (int i = 0; i < m_Dependents.Count(); i++)
	{
		CMapClass *pDependent = m_Dependents.Element(i);

		CMapWorld *pDependentWorld = GetWorldObject( pDependent );
		if ( pDependentWorld != pThisWorld )
		{
			pDependent->OnNotifyDependent( this, Notify_Transform );
		}
	}
#endif
}

