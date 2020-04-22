//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#include "stdafx.h"
#include "collisionutils.h"
#include "fgdlib/gdclass.h"
#include "IEditorTexture.h"
#include "GlobalFunctions.h"
#include "hammer_mathlib.h"
#include "HelperFactory.h"
#include "MapAlignedBox.h"
#include "MapSweptPlayerHull.h"
#include "MapDefs.h"
#include "MapDoc.h"
#include "MapEntity.h"
#include "MapAnimator.h"
#include "MapSolid.h"
#include "MapView2D.h" // dvs FIXME: For HitTest2D implementation
#include "MapViewLogical.h"
#include "MapWorld.h"
#include "Options.h"
#include "Render2D.h"
#include "SaveInfo.h"
#include "VisGroup.h"
#include "MapSprite.h"
#include "camera.h"
#include "hammer.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


IMPLEMENT_MAPCLASS(CMapEntity)


#define LOGICAL_BOX_WIDTH 300
#define LOGICAL_BOX_HEIGHT 300
#define LOGICAL_BOX_INNER_OFFSET 10
#define LOGICAL_BOX_CONNECTOR_INPUT_WIDTH 50
#define LOGICAL_BOX_CONNECTOR_OUTPUT_WIDTH 50
#define LOGICAL_BOX_CONNECTOR_RADIUS 10
#define LOGICAL_BOX_ARROW_LENGTH 25
#define LOGICAL_BOX_ARROW_HEIGHT 10

class CMapAnimator;
class CMapKeyFrame;


bool CMapEntity::s_bShowEntityNames = true;
bool CMapEntity::s_bShowEntityConnections = false;
bool CMapEntity::s_bShowUnconnectedEntities = true;


static CMapObjectList FoundEntities;


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pEntity - 
//			pKV - 
// Output : 
//-----------------------------------------------------------------------------
static BOOL FindKeyValue(CMapEntity *pEntity, MDkeyvalue *pKV)
{
	LPCTSTR pszValue = pEntity->GetKeyValue(pKV->szKey);
	if (!pszValue || strcmpi(pszValue, pKV->szValue))
	{
		return TRUE;
	}

	FoundEntities.AddToTail(pEntity);

	return TRUE;
}


//-----------------------------------------------------------------------------
// Purpose: Compares two entity names, allowing wildcards in EITHER string.
//			Assumes that the wildcard character '*' marks the end of comparison,
//			in other words, these are identical:
//
//			test*
//			test*stuff
//
// Input  : szName1 - 
//			szName2 - 
// Output : int
//-----------------------------------------------------------------------------
int CompareEntityNames(const char *szName1, const char *szName2)
{
	int nCompareLen = -1;

	const char *pszWildcard1 = strchr(szName1, '*');
	if (pszWildcard1)
	{
		nCompareLen = pszWildcard1 - szName1;
	}

	const char *pszWildcard2 = strchr(szName2, '*');
	if (pszWildcard2)
	{
		if (nCompareLen == -1)
		{
			nCompareLen = pszWildcard2 - szName2;
		}
		else
		{
			// Wildcards in both strings -- use the shorter pattern.
			nCompareLen = min(nCompareLen, pszWildcard2 - szName2);
		}
	}

	if (nCompareLen != -1)
	{
		if (nCompareLen > 0)
		{
			return strnicmp(szName1, szName2, nCompareLen);
		}

		// One of the strings had a wildcard as the first character.
		return 0;
	}

	return stricmp(szName1, szName2);
}


//-----------------------------------------------------------------------------
// Replaces references to the old node ID with references to the new node ID.
//-----------------------------------------------------------------------------
static void ReplaceNodeIDRecursive(CMapClass *pRoot, int nOldNodeID, int nNewNodeID)
{
	CMapEntity *pEntity = dynamic_cast <CMapEntity *>(pRoot);
	if (pEntity)
	{
		GDclass *pClass = pEntity->GetClass();
		if (!pClass)
			return;
		
		int nVarCount = pClass->GetVariableCount();
		for (int i = 0; i < nVarCount; i++)
		{
			GDinputvariable *pVar = pClass->GetVariableAt(i);
			if (pVar->GetType() == ivNodeDest)
			{
				const char *pszValue = pEntity->GetKeyValue(pVar->GetName());
				if (pszValue && (atoi(pszValue) == nOldNodeID))
				{
					char szValue[100];
					itoa(nNewNodeID, szValue, 10);
					pEntity->SetKeyValue(pVar->GetName(), szValue);
				}
			}
		}
	}
	else
	{
		
		const CMapObjectList *pChildren = pRoot->GetChildren();
		FOR_EACH_OBJ( *pChildren, pos )
		{
			ReplaceNodeIDRecursive(pChildren->Element(pos), nOldNodeID, nNewNodeID);
		}
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static void ReplaceNodeIDRefs(CMapObjectList &newList, int nOldNodeID, int nNewNodeID)
{
	// If they are the same, do nothing. This can happen when pasting from one
	// map to another map.
	if (nOldNodeID == nNewNodeID)
		return;

	FOR_EACH_OBJ( newList, pos )
	{
		CMapClass *pNew = newList.Element(pos);
		ReplaceNodeIDRecursive(pNew, nOldNodeID, nNewNodeID);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Constructor.
//-----------------------------------------------------------------------------
CMapEntity::CMapEntity(void) : flags(0)
{
	m_pMoveParent = NULL;
	m_pAnimatorChild = NULL;
	m_vecLogicalPosition.Init( COORD_NOTINIT, COORD_NOTINIT );
	CalculateTypeFlags();
}
	

//-----------------------------------------------------------------------------
// Purpose: Destructor.
//-----------------------------------------------------------------------------
CMapEntity::~CMapEntity(void)
{
	SignalChanged();
}


//-----------------------------------------------------------------------------
// Purpose: Adds a bounding box helper to this entity. If this entity's class
//			specifies a bounding box, it will be the correct size.
// Input  : pClass - 
//-----------------------------------------------------------------------------
void CMapEntity::AddBoundBoxForClass(GDclass *pClass, bool bLoading)
{
	Vector Mins;
	Vector Maxs;

	//
	// If we have a class and it specifies a class, use that bounding box.
	//
	if ((pClass != NULL) && (pClass->HasBoundBox()))
	{
		pClass->GetBoundBox(Mins, Maxs);
	}
	//
	// Otherwise, use a default bounding box.
	//
	else
	{
		VectorFill(Mins, -8);
		VectorFill(Maxs, 8);
	}

	//
	// Create the box and add it as one of our children.
	//
	CMapAlignedBox *pBox = new CMapAlignedBox(Mins, Maxs);
	pBox->SetOrigin(m_Origin);
	
	pBox->SetSelectionState(GetSelectionState());

	//
	// HACK: Make sure that the new child gets properly linked into the world.
	//		 This is not correct because it bypasses the doc's AddObjectToWorld code.
	//
	// Don't call AddObjectToWorld during VMF load because we don't want to call
	// OnAddToWorld during VMF load. We update our helpers during PostloadWorld.
	//
	CMapWorld *pWorld = (CMapWorld *)GetWorldObject(this);
	if ((!bLoading) && (pWorld != NULL))
	{
		pWorld->AddObjectToWorld(pBox, this);
	}
	else
	{
		AddChild(pBox);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Sets our child's render color to our render color.
// Input  : pChild - Child object being added.
//-----------------------------------------------------------------------------
void CMapEntity::AddChild(CMapClass *pChild)
{
	CMapClass::AddChild(pChild);

	//
	// Notify the new child of all our keys. Don't bother for solids.
	//
	if (dynamic_cast<CMapSolid*>(pChild) == NULL)
	{
		for ( int i=GetFirstKeyValue(); i != GetInvalidKeyValue(); i=GetNextKeyValue( i ) )
		{
			MDkeyvalue KeyValue = m_KeyValues.GetKeyValue(i); 
			pChild->OnParentKeyChanged( KeyValue.szKey, KeyValue.szValue );
		}	
	}
}



//-----------------------------------------------------------------------------
// Purpose: Adds a helper object as a child of this entity.
// Input  : pHelper - The helper object.
//			bLoading - True if this is being called from Postload, false otherwise.
//-----------------------------------------------------------------------------
void CMapEntity::AddHelper(CMapClass *pHelper, bool bLoading)
{
	if (!IsPlaceholder())
	{
		//
		// Solid entities have no origin, so place the helper at our center.
		//
		Vector vecCenter;
		m_Render2DBox.GetBoundsCenter(vecCenter);
		pHelper->SetOrigin(vecCenter);
	}
	else
	{
		pHelper->SetOrigin(m_Origin);
	}

	pHelper->SetSelectionState(GetSelectionState());

	//
	// If we have a game data class, set the child's render color to the color
	// dictated by the game data class.
	//
	// Note: in AddChild, where it calls OnParentKeyChanged for everything, the color in the helper can
	// get set to something else based on the entity's properties (CMapLightCone does this, for example).
	//
	GDclass *pClass = GetClass();
	if ( pClass )
	{
		color32 rgbColor = pClass->GetColor();
		pHelper->SetRenderColor(rgbColor);
	}

	//
	// HACK: Make sure that the new child gets properly linked into the world.
	//		 This is not correct because it bypasses the doc's AddObjectToWorld code.
	//
	// Don't call AddObjectToWorld during VMF load because we don't want to call
	// OnAddToWorld during VMF load. We update our helpers during PostloadWorld.
	//
	CMapWorld *pWorld = (CMapWorld *)GetWorldObject(this);
	if ((!bLoading) && (pWorld != NULL))
	{
		pWorld->AddObjectToWorld(pHelper, this);
	}
	else
	{
		AddChild(pHelper);
	}
	
	//
	// dvs: HACK for animator children. Better for CMapEntity to have a SetAnimatorChild
	//		function that the CMapAnimator could call. Better still, eliminate the knowledge
	//		that CMapEntity has about its animator child.
	//
	CMapAnimator *pAnim = dynamic_cast<CMapAnimator *>(pHelper);
	if (pAnim != NULL)
	{
		m_pAnimatorChild = pAnim;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Creates all helper objects defined by the FGD and adds them as
//			children of this entity. Helper objects perform rendering, UI, and
//			bookkeeping functions for their parent entities. If the class
//			definition does not specify any helpers, or none of the helpers
//			could be added, a box helper is added so that the entity has some
//			visual representation.
// Inputs : pClass - 
//			bLoading - True if this is being called from Postload, false otherwise.
//-----------------------------------------------------------------------------
void CMapEntity::AddHelpersForClass(GDclass *pClass, bool bLoading)
{
	bool bAddedOneVisual = false;

	if (((pClass != NULL) && (pClass->HasBoundBox())))
	{
		AddBoundBoxForClass(pClass, bLoading);
		bAddedOneVisual = true;
	}

	//
	// If we have a game class from the FGD, add whatever helpers are declared in that
	// class definition.
	//
	if (pClass != NULL)
	{
		//
		// Add all the helpers that this class declares in the FGD.
		//
		GDclass *pClassLocal = GetClass();
		
		//
		// For every helper in the class definition...
		//
		int nHelperCount = pClassLocal->GetHelperCount();
		for (int i = 0; i < nHelperCount; i++)
		{
			CHelperInfo *pHelperInfo = pClassLocal->GetHelper(i);

			//
			// Create the helper and attach it to this entity.
			//
			CMapClass *pHelper = CHelperFactory::CreateHelper(pHelperInfo, this);
			if (pHelper != NULL)
			{
				AddHelper(pHelper, bLoading);
				if (pHelper->IsVisualElement())
				{
					bAddedOneVisual = true;
				}
			}
		}
	
		//
		// Look for keys that define helpers.
		//
		// FIXME: make this totally data driven like the helper factory, or better
		//		  yet, like the LINK_ENTITY_TO_CLASS stuff in the game DLL
		int nVarCount = pClassLocal->GetVariableCount();
		for (int i = 0; i < nVarCount; i++)
		{
			GDinputvariable *pVar = pClassLocal->GetVariableAt(i);
			GDIV_TYPE eType = pVar->GetType();
		
			CHelperInfo HelperInfo;
			bool bCreate = false;
			switch (eType)
			{
				case ivOrigin:
				{
					const char *pszKey = pVar->GetName();
					HelperInfo.SetName("origin");
					HelperInfo.AddParameter(pszKey);
					bCreate = true;
					break;
				}

				case ivVecLine:
				{
					const char *pszKey = pVar->GetName();
					HelperInfo.SetName("vecline");
					HelperInfo.AddParameter(pszKey);
					bCreate = true;
					break;
				}

				case ivAxis:
				{
					const char *pszKey = pVar->GetName();
					HelperInfo.SetName("axis");
					HelperInfo.AddParameter(pszKey);
					bCreate = true;
					break;
				}
			}

			//
			// Create the helper and attach it to this entity.
			//
			if (bCreate)
			{
				CMapClass *pHelper = CHelperFactory::CreateHelper(&HelperInfo, this);
				if (pHelper != NULL)
				{
					AddHelper(pHelper, bLoading);
					if (pHelper->IsVisualElement())
					{
						bAddedOneVisual = true;
					}
				}
			}
		}
	}

	//
	// Any solid children we have will also work as visual elements.
	//
	if (!IsPlaceholder())
	{
		bAddedOneVisual = true;
	}
	//
	// If we have no game class and we are a point entity, add an "obsolete" sprite helper
	// so level designers know to update the entity.
	//
	else if (pClass == NULL)
	{
		CHelperInfo HelperInfo;
		HelperInfo.SetName("iconsprite");
		HelperInfo.AddParameter("sprites/obsolete.spr");

		CMapClass *pSprite = CHelperFactory::CreateHelper(&HelperInfo, this);
		if (pSprite != NULL)
		{
			AddHelper(pSprite, bLoading);
			bAddedOneVisual = true;
		}
	}
	
	//
	// If we still haven't added any visible helpers, we need to add a bounding box so that there
	// is some visual representation for this entity. We also add the bounding box if the
	// entity's class specifies a bounding box.
	//
	if (!bAddedOneVisual)
	{
		AddBoundBoxForClass(pClass, bLoading);
	}

	if ( !CMapClass::s_bLoadingVMF )
	{
		CalcBounds(TRUE);
		PostUpdate(Notify_Changed);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Returns a deep copy of this object.
// Output : Returns a pointer to the new allocated object.
//-----------------------------------------------------------------------------
CMapClass *CMapEntity::Copy(bool bUpdateDependencies)
{
	CMapEntity *pNew = new CMapEntity;
	pNew->CopyFrom(this, bUpdateDependencies);
	return pNew;
}


//-----------------------------------------------------------------------------
// Purpose: Performs a deep copy of a given object into this object.
// Input  : pobj - Object to copy from.
// Output : Returns a pointer to this object.
//-----------------------------------------------------------------------------
CMapClass *CMapEntity::CopyFrom(CMapClass *pobj, bool bUpdateDependencies)
{
	Assert(pobj->IsMapClass(MAPCLASS_TYPE(CMapEntity)));
	CMapEntity *pFrom = (CMapEntity*) pobj;
	
	flags = pFrom->flags;
	
	m_Origin = pFrom->m_Origin;
	m_vecLogicalPosition = pFrom->m_vecLogicalPosition;

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
	CalculateTypeFlags();
	SignalChanged();
	return(this);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bFullUpdate - 
//-----------------------------------------------------------------------------
void CMapEntity::CalcBounds(BOOL bFullUpdate)
{
	CMapClass::CalcBounds(bFullUpdate);

	//
	// If we are a solid entity, set our origin to our bounds center.
	//
	if (IsSolidClass())
	{
		m_Render2DBox.GetBoundsCenter(m_Origin);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Debugging hook.
//-----------------------------------------------------------------------------
#pragma warning (disable:4189)
void CMapEntity::Debug(void)
{
	int i = m_KeyValues.GetFirst();
	MDkeyvalue &KeyValue = m_KeyValues.GetKeyValue(i);
}
#pragma warning (default:4189)


//-----------------------------------------------------------------------------
// Purpose: If this entity has a name key, returns a string with "<name> <classname>"
//			in it. Otherwise returns a buffer with "<classname>" in it.
// Output : String description of the entity.
//-----------------------------------------------------------------------------
const char* CMapEntity::GetDescription(void)
{
	static char szBuf[128];
	const char *pszName = GetKeyValue("targetname");

	if (pszName != NULL)
	{
		sprintf(szBuf, "%s - %s", pszName, GetClassName());
	}
	else
	{
		V_strcpy_safe( szBuf, GetClassName() );
	}

	return(szBuf);
}


//-----------------------------------------------------------------------------
// Purpose: Returns the color that this entity should use for rendering.
//-----------------------------------------------------------------------------
void CMapEntity::GetRenderColor( CRender2D *pRender, unsigned char &red, unsigned char &green, unsigned char &blue )
{
	if ( IsSelected() )
	{
		red = GetRValue(Options.colors.clrSelection);
		green = GetGValue(Options.colors.clrSelection);
		blue = GetBValue(Options.colors.clrSelection);
	}
	else
	{
		GDclass *pClass = GetClass();
		if (pClass)
		{
			color32 rgbColor = pClass->GetColor();

			red = rgbColor.r;
			green = rgbColor.g;
			blue = rgbColor.b;
		}
		else
		{
			red = GetRValue(Options.colors.clrEntity);
			green = GetGValue(Options.colors.clrEntity);
			blue = GetBValue(Options.colors.clrEntity);
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Returns the color that this entity should use for rendering.
//-----------------------------------------------------------------------------
color32 CMapEntity::GetRenderColor( CRender2D *pRender )
{
	color32 clr;

	GetRenderColor( pRender, clr.r, clr.g, clr.b );
	return clr;
}


//-----------------------------------------------------------------------------
// Purpose: Returns the size of this object.
// Output : Size, in bytes, of this object, not including any dynamically
//			allocated data members.
//-----------------------------------------------------------------------------
size_t CMapEntity::GetSize(void)
{
	return(sizeof(*this));
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pFile - 
// Output : ChunkFileResult_t
//-----------------------------------------------------------------------------
ChunkFileResult_t CMapEntity::LoadVMF(CChunkFile *pFile)
{
	//
	// Set up handlers for the subchunks that we are interested in.
	//
	CChunkHandlerMap Handlers;
	Handlers.AddHandler("solid", (ChunkHandler_t)LoadSolidCallback, this);
	Handlers.AddHandler("hidden", (ChunkHandler_t)LoadHiddenCallback, this);
	Handlers.AddHandler("editor", (ChunkHandler_t)LoadEditorCallback, this);
	Handlers.AddHandler("connections", (ChunkHandler_t)LoadConnectionsCallback, (CEditGameClass *)this);

	pFile->PushHandlers(&Handlers);
	ChunkFileResult_t eResult = pFile->ReadChunk((KeyHandler_t)LoadKeyCallback, this);
	pFile->PopHandlers();

	return(eResult);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *szKey - 
//			*szValue - 
//			*pEntity - 
// Output : ChunkFileResult_t
//-----------------------------------------------------------------------------
ChunkFileResult_t CMapEntity::LoadKeyCallback(const char *szKey, const char *szValue, CMapEntity *pEntity)
{
	if (!stricmp(szKey, "id"))
	{
		pEntity->SetID(atoi(szValue));
	}
	else
	{
		//
		// While loading, set key values directly rather than via SetKeyValue. This avoids
		// all the unnecessary bookkeeping that goes on in SetKeyValue.
		//
		pEntity->m_KeyValues.SetValue(szKey, szValue);
	}

	pEntity->CalculateTypeFlags();
	pEntity->SignalChanged();
	return(ChunkFile_Ok);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bVisible - 
// Output : ChunkFileResult_t
//-----------------------------------------------------------------------------
ChunkFileResult_t CMapEntity::LoadHiddenCallback(CChunkFile *pFile, CMapEntity *pEntity)
{
	//
	// Set up handlers for the subchunks that we are interested in.
	//
	CChunkHandlerMap Handlers;
	Handlers.AddHandler("solid", (ChunkHandler_t)LoadSolidCallback, pEntity);
	Handlers.AddHandler("editor", (ChunkHandler_t)LoadEditorCallback, pEntity);

	pFile->PushHandlers(&Handlers);
	ChunkFileResult_t eResult = pFile->ReadChunk();
	pFile->PopHandlers();

	return(eResult);
}


ChunkFileResult_t CMapEntity::LoadEditorKeyCallback( const char *szKey, const char *szValue, CMapEntity *pMapEntity )
{
	if ( !stricmp( szKey, "logicalpos" ) )
	{
		CChunkFile::ReadKeyValueVector2(szValue, pMapEntity->m_vecLogicalPosition );
		return ChunkFile_Ok;
	}
	
	return CMapClass::LoadEditorKeyCallback( szKey, szValue, pMapEntity );
}

		
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
ChunkFileResult_t CMapEntity::LoadEditorCallback(CChunkFile *pFile, CMapEntity *pObject)
{
	return pFile->ReadChunk( (KeyHandler_t)LoadEditorKeyCallback, pObject );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pFile - 
//			*pEntity - 
// Output : ChunkFileResult_t
//-----------------------------------------------------------------------------
ChunkFileResult_t CMapEntity::LoadSolidCallback(CChunkFile *pFile, CMapEntity *pEntity)
{
	CMapSolid *pSolid = new CMapSolid;

	bool bValid;
	ChunkFileResult_t eResult = pSolid->LoadVMF(pFile, bValid);

	if ((eResult == ChunkFile_Ok) && (bValid))
	{
		pEntity->AddChild(pSolid);
	}
	else
	{
		delete pSolid;
	}

	return(eResult);
}


//-----------------------------------------------------------------------------
// Purpose: Sets this entity's origin and updates the bounding box.
// Input  : o - Origin to set.
//-----------------------------------------------------------------------------
void CMapEntity::SetOrigin(Vector& o)
{
	Vector vecOrigin;
	GetOrigin(vecOrigin);
	if (vecOrigin == o)
		return;

	CMapClass::SetOrigin(o);

	// dvs: is this still necessary?
	if (!(flags & flagPlaceholder))
	{
		// not a placeholder.. no origin.
		return;
	}

	if ( !CMapClass::s_bLoadingVMF )
	{	
		CalcBounds( TRUE );
		PostUpdate(Notify_Changed);
		SignalChanged();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Removes all of this entity's helpers.
// Input  : bRemoveSolidChildren - Whether to also remove any solid children. This
//			is true when changing from a solid entity to a point entity.
//-----------------------------------------------------------------------------
void CMapEntity::RemoveHelpers(bool bRemoveSolids)
{
	for( int pos=m_Children.Count()-1; pos>=0; pos-- )
	{
		CMapClass *pChild = m_Children[pos];
		if (bRemoveSolids || ((dynamic_cast <CMapSolid *> (pChild)) == NULL))
		{
			m_Children.FastRemove(pos);
		}
		// LEAKLEAK: need to KeepForDestruction to avoid undo crashes, but how? where?
		//delete pChild;
	}
}


//-----------------------------------------------------------------------------
// Building targetnames which deal with *
//-----------------------------------------------------------------------------
static inline void BuildNewTargetName( const char *pOldName, const char *pNewName, char *pBuffer )
{
	strcpy(pBuffer, pNewName);

	// If we matched a key value that contains wildcards, preserve the
	// wildcards when we replace the name.
	//
	// For example, "oldname*" would become "newname*" instead of just "newname"
	// FIXME: ??? handle different-length names with wildcards, eg. "old_vort*" => "new_weasel*"
	const char *pszWildcard = strchr(pOldName, '*');
	if (pszWildcard)
	{
		strcpy(&pBuffer[pszWildcard - pOldName], "*");
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CMapEntity::ReplaceTargetname(const char *szOldName, const char *szNewName)
{
	// NOTE: Case-sensitive compare because people might want to replace one case with another.
	if ( !Q_strcmp( szOldName, szNewName ) )
	{
		// The names already match. There is nothing to do!
		return;
	}

	char szTempName[MAX_KEYVALUE_LEN];

	//
	// Replace any keys whose value matches the old name.
	//
	for ( int i=GetFirstKeyValue(); i != GetInvalidKeyValue(); i=GetNextKeyValue( i ) )
	{
		MDkeyvalue KeyValue = m_KeyValues.GetKeyValue(i);
		if (!CompareEntityNames(KeyValue.szValue, szOldName))
		{
			BuildNewTargetName( KeyValue.szValue, szNewName, szTempName );
			SetKeyValue( KeyValue.szKey, szTempName );
		}
	}

	//
	// Replace any connections that target the old name.
	//
	int nConnCount = Connections_GetCount();
	for (int i = 0; i < nConnCount; i++)
	{
		CEntityConnection *pConn = Connections_Get(i);
		if (!CompareEntityNames( pConn->GetTargetName(), szOldName ))
		{
			BuildNewTargetName( pConn->GetTargetName(), szNewName, szTempName );
			pConn->SetTargetName(szTempName);
		}

		if (!CompareEntityNames( pConn->GetSourceName(), szOldName ))
		{
			BuildNewTargetName( pConn->GetSourceName(), szNewName, szTempName );
			pConn->SetSourceName(szTempName);
		}

		if ( !CompareEntityNames( pConn->GetParam(), szOldName ))
		{
			BuildNewTargetName( pConn->GetParam(), szNewName, szTempName );
			pConn->SetParam(szTempName);
		}
	}
	
	CMapClass::ReplaceTargetname(szOldName, szNewName);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Inputs : pszClass - 
//			bLoading - True if this is being called from Postload, false otherwise.
//-----------------------------------------------------------------------------
void CMapEntity::SetClass(LPCTSTR pszClass, bool bLoading)
{
	Assert(pszClass);

	//
	// If we are just setting to the same class, don't do anything.
	//
	if (IsClass(pszClass))
	{
		return;
	}

	//
	// Copy class name & resolve GDclass pointer.
	//
	CEditGameClass::SetClass(pszClass, bLoading);
	UpdateObjectColor();

	//
	// If our new class is defined in the FGD, set our color and our default keys
	// from the class.
	//
	if (IsClass())
	{
		SetPlaceholder(!IsSolidClass());
		GetDefaultKeys();

		if (IsNodeClass() && (GetNodeID() == 0))
		{
			AssignNodeID();
		}
	}
	//
	// If not, use whether or not we have solid children to determine whether
	// we are a point entity or a solid entity.
	//
	else
	{
		SetPlaceholder(HasSolidChildren() ? FALSE : TRUE);
	}

	//
	// Add whatever helpers our class requires, or a default bounding box if
	// our class is unknown and we are a point entity.
	//
	UpdateHelpers(bLoading);
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if ( !pDoc->IsLoading() )
	{
		pDoc->RemoveFromAutoVisGroups( this );
		pDoc->AddToAutoVisGroup( this );
	}

	//
	// HACK: If we are now a decal, make sure we have a valid texture.
	//
	if (!strcmp(pszClass, "infodecal"))
	{
		if (!GetKeyValue("texture"))
		{
			SetKeyValue("texture", "clip");
		}
	}
	CalculateTypeFlags();
	SignalChanged();
}


//-----------------------------------------------------------------------------
// Purpose: Assigns the next unique node ID to this entity.
//-----------------------------------------------------------------------------
void CMapEntity::AssignNodeID(void)
{
	char szID[80];
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	itoa(pDoc->GetNextNodeID(), szID, 10);
	SetKeyValue("nodeid", szID);
}

struct CClassNameFlagsMatcher
{
	char const *m_pClassname;
	int m_nFlagsToOR;
};

static CClassNameFlagsMatcher s_ClassFlagsTable[]={
	{ "light_environment", ENTITY_FLAG_IS_LIGHT },
	{ "light", ENTITY_FLAG_IS_LIGHT },
	{ "light_spot", ENTITY_FLAG_IS_LIGHT },
	{ "prop_static", ENTITY_FLAG_SHOW_IN_LPREVIEW2 },
	{ "func_instance", ENTITY_FLAG_IS_INSTANCE },
};


void CMapEntity::CalculateTypeFlags( void )
{
	m_EntityTypeFlags = 0;
	const char *pszClassName = GetClassName();
	if (pszClassName != NULL)
		for(int i=0; i<NELEMS( s_ClassFlagsTable ); i++)
			if ( ! stricmp( pszClassName, s_ClassFlagsTable[i].m_pClassname ) )
				m_EntityTypeFlags |= s_ClassFlagsTable[i].m_nFlagsToOR;
}


void CMapEntity::SignalChanged( void )
{
	if ( m_EntityTypeFlags & ENTITY_FLAG_IS_LIGHT )
		SignalUpdate( EVTYPE_LIGHTING_CHANGED );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapEntity::EnsureUniqueNodeID(CMapWorld *pWorld)
{
	bool bBuildNewNodeID = true;
	int nOurNodeID = GetNodeID();
	if (nOurNodeID != 0)
	{
		//
		// We already have a node ID. Make sure that it is unique. If not,
		// we need to generate a new one.
		//
		bBuildNewNodeID = false;

		EnumChildrenPos_t pos;
		CMapClass *pChild = pWorld->GetFirstDescendent(pos);
		while (pChild != NULL)
		{
			CMapEntity *pEntity = dynamic_cast <CMapEntity *> (pChild);
			if ((pEntity != NULL) && (pEntity != this))
			{
				int nThisNodeID = pEntity->GetNodeID();
				if (nThisNodeID)
				{
					if (nThisNodeID == nOurNodeID)
					{
						bBuildNewNodeID = true;
						break;
					}
				}
			}

			pChild = pWorld->GetNextDescendent(pos);
		}
	}

	if (bBuildNewNodeID)
	{
		AssignNodeID();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Called after the entire map has been loaded. This allows the object
//			to perform any linking with other map objects or to do other operations
//			that require all world objects to be present.
//-----------------------------------------------------------------------------
void CMapEntity::PostloadWorld(CMapWorld *pWorld)
{
	int nIndex;

	//
	// Set our origin from our "origin" key and discard the key.
	//
	const char *pszValue = m_KeyValues.GetValue("origin", &nIndex);
	if (pszValue != NULL)
	{
		Vector Origin;
		sscanf(pszValue, "%f %f %f", &Origin[0], &Origin[1], &Origin[2]);
		SetOrigin(Origin);
	}

	//
	// Set our angle from our "angle" key and discard the key.
	//
	pszValue = m_KeyValues.GetValue("angle", &nIndex);
	if (pszValue != NULL)
	{
		ImportAngle(atoi(pszValue));
		RemoveKey(nIndex);
	}

	//
	// Set the class name from our "classname" key and discard the key.
	// This also adds the helpers appropriate for the class.
	//
	pszValue = m_KeyValues.GetValue("classname", &nIndex);
	if (pszValue != NULL)
	{
		//
		// Copy the classname to a temp buffer because SetClass mucks with the
		// keyvalues and our pointer might become bad.
		//
		char szClassName[MAX_CLASS_NAME_LEN];
		strcpy(szClassName, pszValue);
		SetClass(szClassName, true);

		//
		// Need to re-get the index of the classname key since it may have changed
		// as a result of the above SetClass call.
		//
		pszValue = m_KeyValues.GetValue("classname", &nIndex);
		if (pszValue != NULL)
		{
			RemoveKey(nIndex);
		}
	}

	//
	// Now that we have set the class, remove the origin key if this entity isn't
	// supposed to expose it in the keyvalues list.
	//
	if (IsPlaceholder() && (!IsClass() || GetClass()->VarForName("origin") == NULL))
	{
		pszValue = m_KeyValues.GetValue("origin", &nIndex);
		if (pszValue != NULL)
		{
			RemoveKey(nIndex);
		}
	}

	//
	// Must do this after assigning the class.
	//
	if (IsNodeClass() && (GetKeyValue("nodeid") == NULL))
	{
		AssignNodeID();
	}

	// Set a reasonable default 
	Vector2D vecLogicalPos = GetLogicalPosition();
	if ( vecLogicalPos.x == COORD_NOTINIT )
	{
		CMapDoc::GetActiveMapDoc()->GetDefaultNewLogicalPosition( vecLogicalPos );
		SetLogicalPosition( vecLogicalPos );
	}

	//
	// Call in all our children (some of which were created above).
	//
	CMapClass::PostloadWorld(pWorld);

	CalculateTypeFlags();
}


//-----------------------------------------------------------------------------
// Purpose: Insures that the entity has all the helpers that it needs (and no more
//			than it should) given its class.
//-----------------------------------------------------------------------------
void CMapEntity::UpdateHelpers(bool bLoading)
{
	//
	// If we have any helpers, delete them. Delete any solid children if we are
	// a point class.
	//
	RemoveHelpers(IsPlaceholder() == TRUE);

	//
	// Add the helpers appropriate for our current class.
	//	
	AddHelpersForClass(GetClass(), bLoading);	
}


//-----------------------------------------------------------------------------
// Safely sets the move parent. Will assert and not set it if pEnt is equal to this ent,
// or if this ent is already a parent of pEnt.
//-----------------------------------------------------------------------------
void CMapEntity::SetMoveParent( CMapEntity *pEnt )
{
	// Make sure pEnt is not already parented to (or identical to) me.
	CMapEntity *pCur = pEnt;
	for ( int i=0; i < 300; i++ )
	{
		if ( pCur == NULL )
		{
			break;
		}
		else if ( pCur == this )
		{
			Assert( !"SetMoveParent: recursive parenting!" );
			m_pMoveParent = NULL;
			return;
		}
		
		pCur = pCur->m_pMoveParent;
	}
	
	m_pMoveParent = pEnt;
}


//-----------------------------------------------------------------------------
// Purpose: Allows the entity to update its key values based on a change in one
//			of its children. The child exposes the property as a key value pair.
// Input  : pChild - The child whose property changed.
//			szKey - The name of the property that changed.
//			szValue - The new value of the property.
//-----------------------------------------------------------------------------
void CMapEntity::NotifyChildKeyChanged(CMapClass *pChild, const char *szKey, const char *szValue)
{
	m_KeyValues.SetValue(szKey, szValue);

	//
	// Notify all our other non-solid children that a key has changed.
	//
	FOR_EACH_OBJ( m_Children, pos )
	{
		CMapClass *pObject = m_Children.Element(pos);
		if ((pObject != pChild) && (pChild != NULL) && (dynamic_cast<CMapSolid *>(pObject) == NULL))
		{
			pObject->OnParentKeyChanged(szKey, szValue);
		}
	}

	CalcBounds();
	CalculateTypeFlags();
	SignalChanged();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapEntity::DeleteKeyValue(LPCSTR pszKey)
{
	char szOldValue[KEYVALUE_MAX_VALUE_LENGTH];
	const char *pszOld = GetKeyValue(pszKey);
	if (pszOld != NULL)
	{
		strcpy(szOldValue, pszOld);
	}
	else
	{
	  szOldValue[0] = '\0';
	}

	CEditGameClass::DeleteKeyValue(pszKey);

	OnKeyValueChanged(pszKey, szOldValue, "");
	CalculateTypeFlags();
	SignalChanged();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapEntity::SetKeyValue(LPCSTR pszKey, LPCSTR pszValue)
{
	//
	// Get the current value so we can tell if it is changing.
	//
	char szOldValue[KEYVALUE_MAX_VALUE_LENGTH];
	const char *pszOld = GetKeyValue(pszKey);
	if (pszOld != NULL)
	{
		V_strcpy_safe(szOldValue, pszOld);
	}
	else
	{
		szOldValue[0] = '\0';
	}

	CEditGameClass::SetKeyValue(pszKey, pszValue);

	OnKeyValueChanged(pszKey, szOldValue, pszValue);
	SignalChanged();
}


//-----------------------------------------------------------------------------
// Purpose: Notifies the entity that it has been cloned.
// Input  : pClone - 
//-----------------------------------------------------------------------------
void CMapEntity::OnPreClone(CMapClass *pClone, CMapWorld *pWorld, const CMapObjectList &OriginalList, CMapObjectList &NewList)
{
	CMapClass::OnPreClone(pClone, pWorld, OriginalList, NewList);

	if (OriginalList.Count() == 1)
	{
		// dvs: TODO: make this FGD-driven instead of hardcoded, see also MapKeyFrame.cpp
		// dvs: TODO: use letters of the alphabet between adjacent numbers, ie path2a path2b, etc.
		if (!stricmp(GetClassName(), "path_corner") || !stricmp(GetClassName(), "path_track"))
		{
			//
			// Generate a new name for the clone.
			//
			CMapEntity *pNewEntity = dynamic_cast<CMapEntity*>(pClone);
			Assert(pNewEntity != NULL);
			if (!pNewEntity)
				return;

			// create a new targetname for the clone
			char newName[128];
			const char *oldName = GetKeyValue("targetname");
			if (!oldName || oldName[0] == 0)
				oldName = "path";

			pWorld->GenerateNewTargetname(oldName, newName, sizeof(newName), true, NULL);
			pNewEntity->SetKeyValue("targetname", newName);
		}
	}
	
	if (IsNodeClass())
	{
		((CMapEntity *)pClone)->AssignNodeID();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pClone - 
//			pWorld - 
//			OriginalList - 
//			NewList - 
//-----------------------------------------------------------------------------
void CMapEntity::OnClone(CMapClass *pClone, CMapWorld *pWorld, const CMapObjectList &OriginalList, CMapObjectList &NewList)
{
	CMapClass::OnClone(pClone, pWorld, OriginalList, NewList);

	if (OriginalList.Count() == 1)
	{
		if (!stricmp(GetClassName(), "path_corner") || !stricmp(GetClassName(), "path_track"))
		{
			// dvs: TODO: make this FGD-driven instead of hardcoded, see also MapKeyFrame.cpp
			// dvs: TODO: use letters of the alphabet between adjacent numbers, ie path2a path2b, etc.
			CMapEntity *pNewEntity = dynamic_cast<CMapEntity*>(pClone);
			Assert(pNewEntity != NULL);
			if (!pNewEntity)
				return;

			// Point the clone at what we were pointing at.
			const char *pszNext = GetKeyValue("target");
			if (pszNext)
			{
				pNewEntity->SetKeyValue("target", pszNext);
			}

			// Point this path corner at the clone.
			SetKeyValue("target", pNewEntity->GetKeyValue("targetname"));
		}
	}

	if (IsNodeClass())
	{
		ReplaceNodeIDRefs(NewList, GetNodeID(), ((CMapEntity *)pClone)->GetNodeID());
	}
}


//-----------------------------------------------------------------------------
// Purpose: Notifies the object that a copy of it is being pasted from the
//			clipboard before the copy is added to the world.
// Input  : pCopy - The copy of this object that is being added to the world.
//			pSourceWorld - The world that the originals were in.
//			pDestWorld - The world that the copies are being added to.
//			OriginalList - The list of original objects that were copied.
//			NewList - The list of copied.
//-----------------------------------------------------------------------------
void CMapEntity::OnPrePaste( CMapClass *pCopy, CMapWorld *pSourceWorld, CMapWorld *pDestWorld, const CMapObjectList &OriginalList, CMapObjectList &NewList )
{
	if (IsNodeClass())
	{
		// Generate a new node ID.
		((CMapEntity *)pCopy)->AssignNodeID();
	}

	CMapClass::OnPrePaste(pCopy, pSourceWorld, pDestWorld, OriginalList, NewList);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pCopy - 
//			pSourceWorld - 
//			pDestWorld - 
//			OriginalList - 
//			NewList - 
//-----------------------------------------------------------------------------
void CMapEntity::OnPaste(CMapClass *pCopy, CMapWorld *pSourceWorld, CMapWorld *pDestWorld, const CMapObjectList &OriginalList, CMapObjectList &NewList)
{
	if (IsNodeClass())
	{
		ReplaceNodeIDRefs(NewList, GetNodeID(), ((CMapEntity *)pCopy)->GetNodeID());
	}

	CMapClass::OnPaste(pCopy, pSourceWorld, pDestWorld, OriginalList, NewList);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pszKey - 
//			pszOldValue - 
//			pszValue - 
//-----------------------------------------------------------------------------
void CMapEntity::OnKeyValueChanged(const char *pszKey, const char *pszOldValue, const char *pszValue)
{
	// notify all our children that a key has changed

	FOR_EACH_OBJ( m_Children, pos )
	{
		CMapClass *pChild = m_Children.Element( pos );
		if ( pChild != NULL )
		{
			pChild->OnParentKeyChanged( pszKey, pszValue );
		}
	}

	//
	// Changing our movement parent. Store a pointer to the movement parent
	// for when we're playing animations.
	//
	if ( !stricmp(pszKey, "parentname") )
	{
		CMapWorld *pWorld = (CMapWorld *)GetWorldObject( this );
		if (pWorld != NULL)
		{
			CMapEntity *pMoveParent = (CMapEntity *)UpdateDependency(m_pMoveParent, pWorld->FindEntityByName( pszValue));
			SetMoveParent( pMoveParent );
		}
	}
	//
	// Changing our model - rebuild the helpers from scratch.
	// dvs: this could probably go away - move support into the helper code.
	//
	else if (!stricmp(pszKey, "model"))
	{
		if (stricmp(pszOldValue, pszValue) != 0)
		{
			// We don't call SetKeyValue during VMF load.
			UpdateHelpers(false);
		}
	}
	//
	// If our targetname has changed, we have to relink EVERYTHING, not
	// just our dependents, because someone else may point to our new targetname.
	//
	else if (!stricmp(pszKey, "targetname") && (stricmp(pszOldValue, pszValue) != 0))
	{
		UpdateAllDependencies(this);
	}
	SignalChanged();
}


//-----------------------------------------------------------------------------
// Purpose: Returns true if this entity has any solid children. Entities of
//			classes that are not in the FGD are considered solid entities if
//			they have at least one solid child, point entities if not.
//-----------------------------------------------------------------------------
bool CMapEntity::HasSolidChildren(void)
{
	FOR_EACH_OBJ( m_Children, pos )
	{
		CMapClass *pChild = m_Children.Element(pos);
		if ((dynamic_cast <CMapSolid *> (pChild)) != NULL)
		{
			return(true);
		}
	}

	return(false);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CMapEntity::OnApply( void )
{
	FOR_EACH_OBJ( m_Children, pos )
	{
		CMapClass *pChild = m_Children.Element(pos);
		if ( pChild )
		{
			pChild->OnApply();
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Called after this object is added to the world.
//
//			NOTE: This function is NOT called during serialization. Use PostloadWorld
//				  to do similar bookkeeping after map load.
//
// Input  : pWorld - The world that we have been added to.
//-----------------------------------------------------------------------------
void CMapEntity::OnAddToWorld(CMapWorld *pWorld)
{
	CMapClass::OnAddToWorld(pWorld);

	//
	// If we are a node class, we must insure that we have a valid unique ID.
	//
	if (IsNodeClass())
	{
		EnsureUniqueNodeID(pWorld);
	}

	//
	// If we have a targetname, relink all the targetname pointers in the world
	// because someone might be looking for our targetname.
	//
	if (GetKeyValue("targetname") != NULL)
	{
		UpdateAllDependencies(this);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called before this object is deleted from the world.
//
// Input  : pWorld - The world that we have been added to.
//			b
//-----------------------------------------------------------------------------
void CMapEntity::OnRemoveFromWorld(CMapWorld *pWorld, bool bNotifyChildren)
{
	// Disconnect this now removed entity from the rest of the world
	Connections_FixBad(false);
	Upstream_FixBad();
	
	CMapClass::OnRemoveFromWorld(pWorld, bNotifyChildren);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pObject - The object that changed.
//-----------------------------------------------------------------------------
void CMapEntity::OnNotifyDependent(CMapClass *pObject, Notify_Dependent_t eNotifyType)
{
	CMapClass::OnNotifyDependent(pObject, eNotifyType);

	if (eNotifyType == Notify_Removed)
	{
		//
		// Check for our move parent going away.
		//
		if (pObject == m_pMoveParent)
		{
			CMapWorld *pWorld = (CMapWorld *)GetWorldObject(this);
			const char *pszParentName = CEditGameClass::GetKeyValue("parentname");
			if ((pWorld != NULL) && (pszParentName != NULL))
			{
				CMapEntity *pMoveParent = (CMapEntity *)UpdateDependency(m_pMoveParent, pWorld->FindEntityByName( pszParentName));
				SetMoveParent( pMoveParent );
			}
			else
			{
				CMapEntity *pMoveParent = (CMapEntity *)UpdateDependency(m_pMoveParent, NULL);
				SetMoveParent( pMoveParent );
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Iterates through an object, and all it's children, looking for an
//			entity with a matching key and value
// Input  : key - 
//			value - 
// Output : Returns a pointer to the entity found.
//-----------------------------------------------------------------------------
CMapEntity *CMapEntity::FindChildByKeyValue( LPCSTR key, LPCSTR value, bool *bIsInInstance, VMatrix *InstanceMatrix )
{
	if ((key == NULL) || (value == NULL))
	{
		return(NULL);
	}

	int index;
	LPCSTR val = CEditGameClass::GetKeyValue(key, &index);

	if ( val && value && !stricmp(value, val) )
	{
		return this;
	}

	return CMapClass::FindChildByKeyValue( key, value, bIsInInstance, InstanceMatrix );
}


//-----------------------------------------------------------------------------
// Purpose: Returns a coordinate frame to render in, if the entity is animating
// Input  : matrix - 
// Output : returns true if a new matrix is returned, false if it is just the identity
//-----------------------------------------------------------------------------
bool CMapEntity::GetTransformMatrix( VMatrix& matrix )
{
	bool gotMatrix = false;

	// if we have a move parent, get its transformation matrix
	if ( m_pMoveParent )
	{
		if ( m_pMoveParent == this )
		{
			Assert( !"Recursive parenting." );
		}
		else
		{
			gotMatrix = m_pMoveParent->GetTransformMatrix( matrix );
		}
	}

	if ( m_pAnimatorChild )
	{
		// return a matrix that will transform any vector into our (animated) space
		if ( gotMatrix )
		{
			// return ParentMatrix * OurMatrix
			VMatrix tmpMat, animatorMat;
			bool gotAnimMatrix = m_pAnimatorChild->GetTransformMatrix( animatorMat );
			if ( !gotAnimMatrix )
			{
				// since we didn't get a new matrix from our child just return our parent's
				return true;
			}

			matrix = matrix * animatorMat;
		}
		else
		{
			// no parent, we're at the top of the game
			gotMatrix = m_pAnimatorChild->GetTransformMatrix( matrix );
		}
	}

	return gotMatrix;
}


//-----------------------------------------------------------------------------
// Saves editor data
//-----------------------------------------------------------------------------
ChunkFileResult_t CMapEntity::SaveEditorData(CChunkFile *pFile)
{
#ifndef SDK_BUILD
	ChunkFileResult_t eResult = pFile->WriteKeyValueVector2("logicalpos", m_vecLogicalPosition);
	if (eResult != ChunkFile_Ok)
		return eResult;
#endif // SDK_BUILD

	return BaseClass::SaveEditorData( pFile );
}

		
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
ChunkFileResult_t CMapEntity::SaveVMF(CChunkFile *pFile, CSaveInfo *pSaveInfo)
{
	//
	// Check rules before saving this object.
	//
	if (!pSaveInfo->ShouldSaveObject(this))
	{
		return(ChunkFile_Ok);
	}

	ChunkFileResult_t eResult = ChunkFile_Ok;

	//
	// If it's a solidentity but it doesn't have any solids, 
	// don't save it.
	//
	if (!IsPlaceholder() && !m_Children.Count())
	{
		return(ChunkFile_Ok);
	}

	//
	// If we are hidden, place this object inside of a hidden chunk.
	//
	if (!IsVisible())
	{
		eResult = pFile->BeginChunk("hidden");
	}

	//
	// Begin this entity's scope.
	//
	eResult = pFile->BeginChunk("entity");

	//
	// Save the entity's ID.
	//
	if (eResult == ChunkFile_Ok)
	{
		eResult = pFile->WriteKeyValueInt("id", GetID());
	}

	//
	// Save our keys.
	//
	if (eResult == ChunkFile_Ok)
	{
		eResult = CEditGameClass::SaveVMF(pFile, pSaveInfo);
	}

	//
	// If this is a point entity of an unknown type or a point entity that doesn't
	// declare an origin key, save our origin.
	//
	if (IsPlaceholder() && (!IsClass() || GetClass()->VarForName("origin") == NULL))
	{
		char szOrigin[80];
		sprintf(szOrigin, "%g %g %g", (double)m_Origin[0], (double)m_Origin[1], (double)m_Origin[2]);
		pFile->WriteKeyValue("origin", szOrigin);
	}

	//
	// Save all our descendents.
	//
	eResult = ChunkFile_Ok;
	EnumChildrenPos_t pos;
	CMapClass *pChild = GetFirstDescendent(pos);
	while ((pChild != NULL) && (eResult == ChunkFile_Ok))
	{
		if ( pChild->ShouldSerialize() )
		{
			eResult = pChild->SaveVMF(pFile, pSaveInfo);
		}
		pChild = GetNextDescendent(pos);
	}

	//
	// Save our base class' information within our chunk.
	//
	if (eResult == ChunkFile_Ok)
	{
		eResult = CMapClass::SaveVMF(pFile, pSaveInfo);
	}
	
	//
	// End this entity's scope.
	//
	if (eResult == ChunkFile_Ok)
	{
		pFile->EndChunk();
	}

	//
	// End the hidden chunk if we began it.
	//
	if (!IsVisible())
	{
		eResult = pFile->EndChunk();
	}

	return(eResult);
}


//-----------------------------------------------------------------------------
// Purpose: Overloaded to use the color from our FGD definition.
// Output : Returns true if the color was specified by this call, false if not.
//-----------------------------------------------------------------------------
bool CMapEntity::UpdateObjectColor()
{
	if (!BaseClass::UpdateObjectColor())
	{
		if (IsClass())
		{
			color32 rgbColor = m_pClass->GetColor();
			SetRenderColor(rgbColor);
			return true;
		}
	}
	else
	{
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pWorld - 
//			pObject - 
//-----------------------------------------------------------------------------
void CMapEntity::UpdateDependencies(CMapWorld *pWorld, CMapClass *pObject)
{
	CMapClass::UpdateDependencies(pWorld, pObject);

	//
	// If we have a movement parent, relink to our movement parent.
	//
	const char *pszParentName = CEditGameClass::GetKeyValue("parentname");
	if (pszParentName != NULL)
	{
		CMapEntity *pMoveParent = (CMapEntity *)UpdateDependency(m_pMoveParent, pWorld->FindEntityByName( pszParentName));
		SetMoveParent( pMoveParent );
	}
	else
	{
		CMapEntity *pMoveParent = (CMapEntity *)UpdateDependency(m_pMoveParent, NULL);
		SetMoveParent( pMoveParent );
	}

	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if ( pDoc && !pDoc->IsLoading() )
	{
		// Update any downstream/upstream connections objects associated with this entity
		Connections_FixBad();
		Upstream_FixBad();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Places the entity properly on a plane surface, at a given location
// Input:	pos - position on the plane
//			plane - surface plane to align to
//			align - alignment type (top, bottom)
// Output: 
//-----------------------------------------------------------------------------

#define	ALIGN_EPSILON	1	// World units

void CMapEntity::AlignOnPlane( Vector& pos, PLANE *plane, alignType_e align )
{
	float	fOffset = 0.0f;
	Vector	vecNewPos;

	//Depending on the alignment type, get the offset from the surface
	switch ( align )
	{
	case ALIGN_BOTTOM:
		fOffset = m_Origin[2] - m_Render2DBox.bmins[2];
		break;

	case ALIGN_TOP:
		fOffset = m_Render2DBox.bmaxs[2] - m_Origin[2];
		break;
	}

	//Push our point out and away from this surface
	VectorMA( pos, fOffset + ALIGN_EPSILON, plane->normal, vecNewPos );
	
	//Update the entity and children
	SetOrigin( vecNewPos );
	SignalChanged();
}


//-----------------------------------------------------------------------------
// Purpose: Looks for an input with a given name in the entity list. ALL entities
//			in the list must have the given input for a match to be found.
// Input  : szInput - Name of the input.
// Output : Returns true if the input name was found in all entities, false if not.
//-----------------------------------------------------------------------------
bool MapEntityList_HasInput(const CMapEntityList *pList, const char *szInput, InputOutputType_t eType)
{
	GDclass *pLastClass = NULL;
	FOR_EACH_OBJ( *pList, pos )
	{
		CMapEntity *pEntity = pList->Element(pos);
		GDclass *pClass = pEntity->GetClass();
		if ((pClass != pLastClass) && (pClass != NULL))
		{
			CClassInput *pInput = pClass->FindInput(szInput);
			if (!pInput)
			{
				return false;
			}

			if ((eType != iotInvalid) && (pInput->GetType() != eType))
			{
				return false;
			}

			//
			// Cheap optimization to help minimize redundant checks.
			//
			pLastClass = pClass;
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Returns a pointer to the object that should be added to the selection
//			list because this object was clicked on with a given selection mode.
// Input  : eSelectMode - 
//-----------------------------------------------------------------------------
CMapClass *CMapEntity::PrepareSelection(SelectMode_t eSelectMode)
{
	//
	// Select up the hierarchy when in Groups selection mode if we belong to a group.
	//
	if ((eSelectMode == selectGroups) && (m_pParent != NULL) && !IsWorldObject(m_pParent))
	{
		return GetParent()->PrepareSelection(eSelectMode);
	}

	//
	// Don't select solid entities when in Solids selection mode. We'll select
	// their solid children.
	//
	if ((eSelectMode == selectSolids) && !IsPlaceholder())
	{
		return NULL;
	}

	return this;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pRender - 
//-----------------------------------------------------------------------------
void CMapEntity::Render2D(CRender2D *pRender)
{
	// Render all our children (helpers & solids)
	BaseClass::Render2D(pRender);

	CMapView2D *pView = (CMapView2D*)pRender->GetView();

	Vector vecMins, vecMaxs;
	GetRender2DBox(vecMins, vecMaxs);
	if ( pRender->GetInstanceRendering() )
	{
		Vector vecExpandedMins, vecExpandedMaxs;

		pRender->TransformInstanceAABB( vecMins, vecMaxs, vecExpandedMins, vecExpandedMaxs );
		vecMins = vecExpandedMins;
		vecMaxs = vecExpandedMaxs;
	}

	Vector2D pt, pt2;
	pView->WorldToClient(pt, vecMins);
	pView->WorldToClient(pt2, vecMaxs);

	color32 rgbColor = GetRenderColor( pRender );

	pRender->SetDrawColor( rgbColor.r, rgbColor.g, rgbColor.b );

	// Render the entity's name and class name if enabled.
	if (s_bShowEntityNames && pView->GetZoom() >= 1)
	{
 		pRender->SetTextColor( rgbColor.r, rgbColor.g, rgbColor.b );

		const char *pszTargetName = GetKeyValue("targetname");
		if (pszTargetName != NULL)
		{
			pRender->DrawText(pszTargetName, pt.x, pt.y + 2, CRender2D::TEXT_JUSTIFY_BOTTOM );
		}

		const char *pszClassName = GetClassName();
		if (pszClassName != NULL)
		{
			pRender->DrawText(pszClassName, pt.x, pt2.y - 2, CRender2D::TEXT_JUSTIFY_TOP );
		}

	}

	//
	// Draw the connections between entities and their targets if enabled.
	//
	if (s_bShowEntityConnections)
	{
		LPCTSTR pszTarget = GetKeyValue("target");
		
		if (pszTarget != NULL)
		{
			CMapWorld *pWorld = GetWorldObject(this);
			MDkeyvalue kv("targetname", pszTarget);

			CMapObjectList FoundEntitiesTarget;
			FoundEntitiesTarget.RemoveAll();
			pWorld->EnumChildren((ENUMMAPCHILDRENPROC)FindKeyValue, (DWORD)&kv, MAPCLASS_TYPE(CMapEntity));

			Vector vCenter1,vCenter2;
			GetBoundsCenter( vCenter1 );
			
			FOR_EACH_OBJ( FoundEntitiesTarget, p )
			{
				CMapClass *pEntity = (CMapEntity *)FoundEntitiesTarget.Element(p);
				pEntity->GetBoundsCenter(vCenter2);
				pRender->DrawLine( vCenter1, vCenter2 );
			}
		}
	}

	// Draw the forward vector if we have an "angles" key and we're selected.
	// HACK: don't draw the forward vector for lights, they negate pitch. The model helper will handle it.
	if ((GetSelectionState() != SELECT_NONE) &&
		(!GetClassName() || (strnicmp(GetClassName(), "light_", 6) != 0)) && 
		(GetKeyValue("angles") != NULL))
	{
		Vector vecOrigin;
		GetOrigin(vecOrigin);

		QAngle vecAngles;
		GetAngles(vecAngles);
		Vector vecForward;
		AngleVectors(vecAngles, &vecForward);

		pRender->SetDrawColor( 255, 255, 0 );
		pRender->DrawLine(vecOrigin, vecOrigin + vecForward * 24);
	}
}


//-----------------------------------------------------------------------------
// Gets the 2D logical view bounding box
//-----------------------------------------------------------------------------
void CMapEntity::GetRenderLogicalBox( Vector2D &mins, Vector2D &maxs )
{
	mins.x = m_vecLogicalPosition.x;
	maxs.x = m_vecLogicalPosition.x + LOGICAL_BOX_WIDTH + LOGICAL_BOX_CONNECTOR_INPUT_WIDTH + LOGICAL_BOX_CONNECTOR_OUTPUT_WIDTH;
	mins.y = m_vecLogicalPosition.y;
	maxs.y = m_vecLogicalPosition.y + LOGICAL_BOX_HEIGHT;
}


//-----------------------------------------------------------------------------
// Logical position accessor
//-----------------------------------------------------------------------------
const Vector2D& CMapEntity::GetLogicalPosition( )
{
	return m_vecLogicalPosition;
}

void CMapEntity::SetLogicalPosition( const Vector2D &vecPosition )
{
	m_vecLogicalPosition = vecPosition;
}


//-----------------------------------------------------------------------------
// Returns a logical position
//-----------------------------------------------------------------------------
void CMapEntity::GetLogicalConnectionPosition( LogicalConnection_t i, Vector2D &vecPosition )
{
	Vector2D vecMins, vecMaxs;
	GetRenderLogicalBox( vecMins, vecMaxs );
	
	vecPosition.y = ( vecMins.y + vecMaxs.y ) * 0.5f;

	if ( i == LOGICAL_CONNECTION_INPUT )
	{
		vecPosition.x = vecMins.x;
	}
	else
	{
		vecPosition.x = vecMaxs.x;
	}
}


//-----------------------------------------------------------------------------
// Renders into the logical view
//-----------------------------------------------------------------------------
void CMapEntity::RenderLogical( CRender2D *pRender )
{		    
	// Render all our children (helpers & solids)
	BaseClass::RenderLogical(pRender);

	Vector2D vecMins, vecMaxs;
	GetRenderLogicalBox( vecMins, vecMaxs );

	Vector2D vecBoxMins = vecMins;
	Vector2D vecBoxMaxs = vecMaxs;
	vecBoxMins.x += LOGICAL_BOX_CONNECTOR_INPUT_WIDTH;
	vecBoxMaxs.x -= LOGICAL_BOX_CONNECTOR_OUTPUT_WIDTH;
	 
	// Define the entity highlight/lowlight edges
	Vector2D vecInnerMins = vecBoxMins, vecInnerMaxs = vecBoxMaxs;
	vecInnerMins.x += LOGICAL_BOX_INNER_OFFSET;
	vecInnerMins.y += LOGICAL_BOX_INNER_OFFSET;
	vecInnerMaxs.x -= LOGICAL_BOX_INNER_OFFSET;
	vecInnerMaxs.y -= LOGICAL_BOX_INNER_OFFSET;

	// Get the entity render color
	color32 rgbColor = GetRenderColor( pRender );
	color32	rgbHighlight = {(byte)(7*rgbColor.r/8), (byte)(7*rgbColor.g/8), (byte)(7*rgbColor.b/8), (byte)255 };
	color32 rgbLowlight = { (byte)(5*rgbColor.r/8), (byte)(5*rgbColor.g/8), (byte)(5*rgbColor.b/8), (byte)255 };
	color32 rgbEdgeColor = { (byte)(3*rgbColor.r/8), (byte)(3*rgbColor.g/8), (byte)(3*rgbColor.b/8), (byte)255 };
	color32 rgbInterior = { (byte)(2*rgbColor.r/8), (byte)(2*rgbColor.g/8), (byte)(2*rgbColor.b/8), (byte)255 };

	// Draw an inside UpperLeft highlight rect (leading edge highlight)
	pRender->SetDrawColor( rgbHighlight.r, rgbHighlight.g, rgbHighlight.b );
	pRender->DrawRectangle( Vector( vecBoxMins.x, vecBoxMins.y, 0.0f ), Vector( vecBoxMaxs.x, vecBoxMaxs.y, 0.0f ), true, 0 );

	// Draw an inside LowerRight lowlight rect (trailing edge lowlight)
	pRender->SetDrawColor( rgbLowlight.r, rgbLowlight.g, rgbLowlight.b );
	pRender->DrawRectangle( Vector( vecInnerMins.x, vecBoxMins.y, 0.0f ), Vector( vecBoxMaxs.x, vecInnerMaxs.y, 0.0f ), true, 0 );

	// Draw an outside border rect in the entities render color
	pRender->SetDrawColor( rgbEdgeColor.r, rgbEdgeColor.g, rgbEdgeColor.b );
	pRender->DrawRectangle( Vector( vecBoxMins.x, vecBoxMins.y, 0.0f ), Vector( vecBoxMaxs.x, vecBoxMaxs.y, 0.0f ), false, 0 );

	// Draw the small diagonals connecting the outer and inner corners
	pRender->DrawLine( Vector( vecBoxMins.x, vecBoxMins.y, 0.0f ), Vector( vecBoxMaxs.x, vecBoxMaxs.y, 0.0f ) );
	pRender->DrawLine( Vector( vecBoxMins.x, vecBoxMaxs.y, 0.0f ), Vector( vecBoxMaxs.x, vecBoxMins.y, 0.0f ) );

	// Draw interior background first
	pRender->SetDrawColor( rgbInterior.r, rgbInterior.g, rgbInterior.b );
	pRender->DrawRectangle( Vector( vecInnerMins.x, vecInnerMins.y, 0.0f ), Vector( vecInnerMaxs.x, vecInnerMaxs.y, 0.0f ), true, 0 );

	// Draws the sprite helper(s) (if it has them)
	bool bFoundSpriteHelper = false;

	FOR_EACH_OBJ( m_Children, pos )
	{
		CMapSprite *pSprite = dynamic_cast<CMapSprite*>( m_Children[pos] );
		if ( pSprite )
		{
			// Render the sprite on top of the background
			pSprite->RenderLogicalAt( pRender, vecInnerMins, vecInnerMaxs );
			bFoundSpriteHelper = true;
		}
	}

	// Fill in the interior with entity color if no sprite was found
	if ( !bFoundSpriteHelper )
	{
		// Redraw the interior with the entity's render color
		pRender->SetDrawColor( rgbColor.r, rgbColor.g, rgbColor.b );
		pRender->DrawRectangle( Vector( vecInnerMins.x, vecInnerMins.y, 0.0f ), Vector( vecInnerMaxs.x, vecInnerMaxs.y, 0.0f ), true, 0 );

		// Put an inner border around the entity color block
		pRender->SetDrawColor( rgbEdgeColor.r, rgbEdgeColor.g, rgbEdgeColor.b );
		pRender->DrawRectangle( Vector( vecInnerMins.x, vecInnerMins.y, 0.0f ), Vector( vecInnerMaxs.x, vecInnerMaxs.y, 0.0f ), false, 0 );
	}

	// Draw the rest of the entity in the entity color
	pRender->SetDrawColor( rgbColor.r, rgbColor.g, rgbColor.b );

	// Draws the connectors
	float flConnectorY = ( vecMins.y + vecMaxs.y ) * 0.5f;
	pRender->DrawCircle( Vector( vecMins.x + LOGICAL_BOX_CONNECTOR_RADIUS, flConnectorY, 0.0f ), LOGICAL_BOX_CONNECTOR_RADIUS );
	pRender->MoveTo( Vector( vecMins.x + 2 * LOGICAL_BOX_CONNECTOR_RADIUS, flConnectorY, 0.0f ) );
	pRender->DrawLineTo( Vector( vecBoxMins.x, flConnectorY, 0.0f ) );
	  
	pRender->MoveTo( Vector( vecBoxMaxs.x, flConnectorY, 0.0f ) );
	pRender->DrawLineTo( Vector( vecMaxs.x - LOGICAL_BOX_ARROW_LENGTH, flConnectorY, 0.0f ) );
	pRender->DrawLineTo( Vector( vecMaxs.x - LOGICAL_BOX_ARROW_LENGTH, flConnectorY + LOGICAL_BOX_ARROW_HEIGHT, 0.0f ) );
	pRender->DrawLineTo( Vector( vecMaxs.x, flConnectorY, 0.0f ) );
	pRender->DrawLineTo( Vector( vecMaxs.x - LOGICAL_BOX_ARROW_LENGTH, flConnectorY - LOGICAL_BOX_ARROW_HEIGHT, 0.0f ) );
	pRender->DrawLineTo( Vector( vecMaxs.x - LOGICAL_BOX_ARROW_LENGTH, flConnectorY, 0.0f ) );
	   
	// Stop drawing the text once the entity itself gets too small.
	Vector2D pt, pt2;
	pRender->GetView()->WorldToClient( pt, Vector( vecBoxMins.x, vecBoxMins.y, 0.0f ) );
	pRender->GetView()->WorldToClient( pt2, Vector( vecBoxMaxs.x, vecBoxMaxs.y, 0.0f ) );
	if ( fabs( pt.y - pt2.y ) < 32 )
		return;
	
	// Render the entity's name and class name if enabled.
 	pRender->SetTextColor( rgbColor.r, rgbColor.g, rgbColor.b );

	// Draw the inputs and outputs
	const char *pszTargetName = GetKeyValue("targetname");
	if (pszTargetName != NULL)
	{
		pRender->DrawText( pszTargetName, Vector2D( (vecMins.x+vecMaxs.x)/2, vecMaxs.y ), 0, -1, CRender2D::TEXT_JUSTIFY_TOP | CRender2D::TEXT_JUSTIFY_HORZ_CENTER );
	}

	if ( fabs( pt.y - pt2.y ) < 50 )
		return;
	
	const char *pszClassName = GetClassName();
	if (pszClassName != NULL)
	{
		pRender->DrawText( pszClassName, Vector2D( (vecMins.x+vecMaxs.x)/2, vecMins.y ), 0, 1, CRender2D::TEXT_JUSTIFY_BOTTOM | CRender2D::TEXT_JUSTIFY_HORZ_CENTER );
	}
}

		
//-----------------------------------------------------------------------------
// Purpose: Returns whether this entity snaps to half grid or not. Some entities,
//			such as hinges, need to snap to a 0.5 grid to center on geometry.
//-----------------------------------------------------------------------------
bool CMapEntity::ShouldSnapToHalfGrid()
{
	return (GetClass() && GetClass()->ShouldSnapToHalfGrid());
}


//-----------------------------------------------------------------------------
// Purpose: Returns the integer value of the nodeid key of this entity.
//-----------------------------------------------------------------------------
int CMapEntity::GetNodeID(void)
{
	int nNodeID = 0;
	const char *pszNodeID = GetKeyValue("nodeid");
	if (pszNodeID)
	{
		nNodeID = atoi(pszNodeID);
	}
	return nNodeID;
}


//-----------------------------------------------------------------------------
// Purpose: Returns whether this object should be hidden based on the given
//			cordon bounds.
// Output : Returns true to cull the object, false to keep it.
//-----------------------------------------------------------------------------
bool CMapEntity::IsCulledByCordon(const Vector &vecMins, const Vector &vecMaxs)
{
	// Point entities are culled by their origin, not by their bounding box.
	// An exception to that is swept hulls, such as ladders, that are more like solid ents.
	if ( IsPointClass() && !IsSweptHullClass( this ) )
	{
		Vector vecOrigin;
		GetOrigin(vecOrigin);
		return !IsPointInBox(vecOrigin, vecMins, vecMaxs);
	}

	return !IsIntersectingBox(vecMins, vecMaxs);
}



//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pView - 
//			vecPoint - 
//			nHitData - 
// Output : 
//-----------------------------------------------------------------------------
bool CMapEntity::HitTest2D(CMapView2D *pView, const Vector2D &point, HitInfo_t &HitData)
{
	if ( !IsVisible() )
		return false;
	
	if ( BaseClass::HitTest2D(pView, point, HitData) )
		return true;

	//
	// Only check point entities; brush entities are selected via their brushes.
	//
	if ( !IsPointClass() )
		return false;
	
	// First check center X.

	Vector vecCenter, vecViewPoint;
	GetBoundsCenter(vecCenter);

	Vector2D vecClientCenter;
	pView->WorldToClient(vecClientCenter, vecCenter);
	pView->GetCamera()->GetViewPoint( vecViewPoint );

	HitData.pObject = this;
	HitData.nDepth = vecViewPoint[pView->axThird]-vecCenter[pView->axThird];

	if ( pView->CheckDistance( point, vecClientCenter, HANDLE_RADIUS) )
	{
		HitData.uData = 0;
		return true;
	}
	else if (!Options.view2d.bSelectbyhandles)
	{
		//
		// See if any edges of the bbox are within a certain distance from the the point.
		//
		int iSelUnits = 2;
		int x1 = point.x - iSelUnits;
		int x2 = point.x + iSelUnits;
		int y1 = point.y - iSelUnits;
		int y2 = point.y + iSelUnits;

		Vector vecMins;
		Vector vecMaxs;
		GetRender2DBox(vecMins, vecMaxs);

		Vector2D vecClientMins;
		Vector2D vecClientMaxs;
		pView->WorldToClient(vecClientMins, vecMins);
		pView->WorldToClient(vecClientMaxs, vecMaxs);

		Vector2D vecEdges[4] =
		{
			Vector2D(vecClientMins.x, vecClientMins.y),
			Vector2D(vecClientMaxs.x, vecClientMins.y),
			Vector2D(vecClientMaxs.x, vecClientMaxs.y),
			Vector2D(vecClientMins.x, vecClientMaxs.y),
		};

		for (int i = 0; i < 4; i++)
		{
			if (IsLineInside(vecEdges[i], vecEdges[(i + 1) % 4], x1, y1, x2, y2))
			{
				HitData.uData = i+1;
				return true;
			}
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Hit test for the logical view
//-----------------------------------------------------------------------------
bool CMapEntity::HitTestLogical( CMapViewLogical *pView, const Vector2D &vecPoint, HitInfo_t &hitData )
{
	if ( !IsVisible() || !IsLogical() || !IsVisibleLogical() )
		return false;

	if ( BaseClass::HitTestLogical( pView, vecPoint, hitData ) )
		return true;

	// Is the point inside the box?
	Vector2D vecMins;
	Vector2D vecMaxs;
	GetRenderLogicalBox( vecMins, vecMaxs );

	Vector2D vecClientMins;
	Vector2D vecClientMaxs;
	pView->WorldToClient(vecClientMins, vecMins);
	pView->WorldToClient(vecClientMaxs, vecMaxs);
	NormalizeBox( vecClientMins, vecClientMaxs );

	if ( IsPointInside( vecPoint, vecClientMins, vecClientMaxs ) )
	{
		hitData.pObject = this;
		hitData.uData = 0;
		hitData.nDepth = 0.0f;
		return true;
	}

	return false;
}

		
//-----------------------------------------------------------------------------
// Is this logical?
//-----------------------------------------------------------------------------
bool CMapEntity::IsLogical(void)
{
	GDclass *pClass = GetClass();
	return pClass && (( pClass->GetInputCount() > 0 ) || ( pClass->GetOutputCount() > 0 )) || (m_Connections.Count() || m_Upstream.Count());
}


//-----------------------------------------------------------------------------
// Is it visible in the logical view? 
//-----------------------------------------------------------------------------
bool CMapEntity::IsVisibleLogical(void)
{ 
	return IsVisible(); 
}


//-----------------------------------------------------------------------------
// Purpose: Returns true if this entity's name matches the given name, considering
//			wildcards.
// Input  : szName - 
//-----------------------------------------------------------------------------
bool CMapEntity::NameMatches(const char *szName)
{
	const char *pszTargetName = GetKeyValue( "targetname" );
	if (pszTargetName)
	{
		return !CompareEntityNames(pszTargetName, szName);
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Returns true if this entity's classname matches the given name, considering
//			wildcards.
// Input  : szName - 
//-----------------------------------------------------------------------------
bool CMapEntity::ClassNameMatches(const char *szName)
{
	const char *pszClassName = GetClassName();
	if (pszClassName)
	{
		return !CompareEntityNames(pszClassName, szName);
	}

	return false;
}
