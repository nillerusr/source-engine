//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "stdafx.h"
#include "ChunkFile.h"
#include "MapDoc.h"		// dvs: FIXME: I'd rather not have this class know about the doc
#include "VisGroup.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


bool CVisGroup::s_bShowAll = false;
bool CVisGroup::s_bIsConvertingOldVisGroups = false;


//
// Holds context info for loading the hierarchical groups.
//
struct LoadVisGroupData_t
{
	CMapDoc *pDoc;			// The document that is loading.
	CVisGroup *pParent;		// The parent visgroup of the visgroup being loaded, NULL if this is a root-level group.
};


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CVisGroup::CVisGroup(void)
{
	m_dwID = 0;

	m_rgbColor.r = 0;
	m_rgbColor.g = 0;
	m_rgbColor.b = 0;
	m_rgbColor.a = 0;

	m_pParent = NULL;
	
	m_eVisible = VISGROUP_HIDDEN;
	m_szName[0] = '\0';
	m_bIsAuto = false;
}


//-----------------------------------------------------------------------------
// Purpose: Pre-hierarchical visgroups, the visibility state of each group was
//			kept in the VMF, whereas now it's purely a function of the visibility
//			of the member objects. So when loading old maps, we skip the step
//			in which we generate the visgroup state from the objects.
//-----------------------------------------------------------------------------
bool CVisGroup::IsConvertingOldVisGroups()
{
	return s_bIsConvertingOldVisGroups;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pFile - 
//			pData - 
// Output : ChunkFileResult_t
//-----------------------------------------------------------------------------
ChunkFileResult_t CVisGroup::LoadKeyCallback(const char *szKey, const char *szValue, CVisGroup *pGroup)
{
	if (!stricmp(szKey, "name"))
	{
		pGroup->SetName(szValue);
		if ( !stricmp(szValue, "Auto" ) )
		{
			pGroup->SetAuto(true);
		}
	}
	else if (!stricmp(szKey, "visgroupid"))
	{
		pGroup->SetID(atoi(szValue));
	}
	else if (!stricmp(szKey, "color"))
	{
		unsigned char chRed;
		unsigned char chGreen;
		unsigned char chBlue;

		CChunkFile::ReadKeyValueColor(szValue, chRed, chGreen, chBlue);
		pGroup->SetColor(chRed, chGreen, chBlue);
	}
	else if (!stricmp(szKey, "visible"))
	{
		// This is a pre-hierarchical visgroups map -- mark this visgroup as hidden.
		// We'll skip the code in CMapDoc::PostLoad that recalculates visibility.
		pGroup->SetVisible((atoi(szValue) == 1) ? VISGROUP_SHOWN : VISGROUP_HIDDEN);
		s_bIsConvertingOldVisGroups = true;
 	}

	return(ChunkFile_Ok);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pFile - 
// Output : 
//-----------------------------------------------------------------------------
ChunkFileResult_t CVisGroup::LoadVMF(CChunkFile *pFile, CMapDoc *pDoc)
{
	// Fill out a little context blob for passing to the handler.
	LoadVisGroupData_t LoadData;
	LoadData.pDoc = pDoc;
	LoadData.pParent = this;

	CChunkHandlerMap Handlers;
	Handlers.AddHandler("visgroup", (ChunkHandler_t)LoadVisGroupCallback, &LoadData);
	pFile->PushHandlers(&Handlers);

	ChunkFileResult_t eResult = pFile->ReadChunk((KeyHandler_t)LoadKeyCallback, this);
	pFile->PopHandlers();

	return(eResult);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pFile - 
//			pData - 
// Output : ChunkFileResult_t
//-----------------------------------------------------------------------------
ChunkFileResult_t CVisGroup::LoadVisGroupCallback(CChunkFile *pFile, LoadVisGroupData_t *pLoadData)
{
	CVisGroup *pVisGroup = new CVisGroup;
	ChunkFileResult_t eResult = pVisGroup->LoadVMF(pFile, pLoadData->pDoc);
	if (eResult == ChunkFile_Ok)
	{
		if (pLoadData->pParent != NULL)
		{
			pLoadData->pParent->AddChild(pVisGroup);
			pVisGroup->SetParent(pLoadData->pParent);
		}

        if ( !pVisGroup->IsAutoVisGroup() )
		{
			pLoadData->pDoc->VisGroups_AddGroup(pVisGroup);
		}
	}

	return(eResult);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pFile - 
//			pData - 
// Output : ChunkFileResult_t
//-----------------------------------------------------------------------------
ChunkFileResult_t CVisGroup::LoadVisGroupsCallback(CChunkFile *pFile, CMapDoc *pDoc)
{
	s_bIsConvertingOldVisGroups = false;

	// Fill out a little context blob for passing to the handler.
	LoadVisGroupData_t LoadData;
	LoadData.pDoc = pDoc;
	LoadData.pParent = NULL;

	//
	// Set up handlers for the subchunks that we are interested in.
	//
	CChunkHandlerMap Handlers;
	Handlers.AddHandler("visgroup", (ChunkHandler_t)LoadVisGroupCallback, &LoadData);
	
	pFile->PushHandlers(&Handlers);
	ChunkFileResult_t eResult = pFile->ReadChunk();
	pFile->PopHandlers();

	return(eResult);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pChild - 
//-----------------------------------------------------------------------------
void CVisGroup::MoveUp(CVisGroup *pChild)
{
	int nIndex = m_Children.Find(pChild);
	if (nIndex > 0)
	{
		m_Children.Remove(nIndex);
		m_Children.InsertBefore(nIndex - 1, pChild);
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pChild - 
//-----------------------------------------------------------------------------
void CVisGroup::MoveDown(CVisGroup *pChild)
{
	int nIndex = m_Children.Find(pChild);
	if ((nIndex >= 0) && (nIndex < (m_Children.Count() - 1)))
	{
		m_Children.Remove(nIndex);
		m_Children.InsertAfter(nIndex, pChild);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Returns whether or not this visgroup is currently visible.
//-----------------------------------------------------------------------------
VisGroupState_t CVisGroup::GetVisible(void)
{
	return m_eVisible;
}


//-----------------------------------------------------------------------------
// Purpose: Returns whether or not visgroup visibility is being overridden by
//			the "Show All" button.
//-----------------------------------------------------------------------------
bool CVisGroup::IsShowAllActive(void)
{
	return s_bShowAll;
}


//-----------------------------------------------------------------------------
// Purpose: Saves this visgroup.
// Input  : pFile - File to save into.
// Output : Returns ChunkFile_Ok on success, an error code on failure.
//-----------------------------------------------------------------------------
ChunkFileResult_t CVisGroup::SaveVMF(CChunkFile *pFile, CSaveInfo *pSaveInfo)
{
	ChunkFileResult_t eResult = pFile->BeginChunk("visgroup");

	if (eResult == ChunkFile_Ok)
	{
		eResult = pFile->WriteKeyValue("name", GetName());
	}

	if (eResult == ChunkFile_Ok)
	{
		DWORD dwID = GetID();
		eResult = pFile->WriteKeyValueInt("visgroupid", dwID);
	}
	
	if (eResult == ChunkFile_Ok)
	{
		color32 rgbColor = GetColor();
		eResult = pFile->WriteKeyValueColor("color", rgbColor.r, rgbColor.g, rgbColor.b);
	}

	//
	// Recurse into children, writing them within this chunk.
	//
	for (int i = 0; i < GetChildCount(); i++)
	{
		CVisGroup *pChild = GetChild(i);
		eResult = pChild->SaveVMF(pFile, pSaveInfo);

		if (eResult != ChunkFile_Ok)
			break;
	}

	if (eResult == ChunkFile_Ok)
	{
		eResult = pFile->EndChunk();
	}

	return(eResult);
}


//-----------------------------------------------------------------------------
// Purpose: Overrides normal visgroup visibility, making all visgroups visible. 
//-----------------------------------------------------------------------------
void CVisGroup::ShowAllVisGroups(bool bShow)
{
	s_bShowAll = bShow;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CVisGroup::AddChild(CVisGroup *pChild)
{
	int nIndex = m_Children.Find(pChild);
	if (nIndex == -1)
	{
		m_Children.AddToTail(pChild);
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pChild - 
//-----------------------------------------------------------------------------
bool CVisGroup::CanMoveUp(CVisGroup *pChild)
{
	return (m_Children.Find(pChild) > 0);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pChild - 
//-----------------------------------------------------------------------------
bool CVisGroup::CanMoveDown(CVisGroup *pChild)
{
	int nIndex = m_Children.Find(pChild);
	return (nIndex >= 0) && (nIndex < m_Children.Count() - 1);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CVisGroup::RemoveChild(CVisGroup *pChild)
{
	int nIndex = m_Children.Find(pChild);
	if (nIndex != -1)
	{
		m_Children.Remove(nIndex);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Returns true if the group is one of our descendents, false if not.
//-----------------------------------------------------------------------------
bool CVisGroup::FindDescendent(CVisGroup *pGroup)
{
	for (int i = 0; i < m_Children.Count(); i++)
	{
		CVisGroup *pChild = m_Children.Element(i);
		if ((pChild == pGroup) || (pChild->FindDescendent(pGroup)))
		{
			return true;
		}
	}

	return false;
}

bool CVisGroup::IsAutoVisGroup()
{
	return m_bIsAuto;	
}

void CVisGroup::SetAuto( bool bAuto )
{
	m_bIsAuto = bAuto;
}


void CVisGroup::VisGroups_UpdateParent( VisGroupState_t state )
{
    CVisGroup *pParent = GetParent();
	VisGroupState_t parentState = pParent->GetVisible();
	if ( state == VISGROUP_PARTIAL )
	{
		pParent->SetVisible( VISGROUP_PARTIAL );
	}

	if ( parentState == VISGROUP_UNDEFINED )
	{
		pParent->SetVisible( state );
	}
	else if ( parentState != state )
	{
		pParent->SetVisible( VISGROUP_PARTIAL );
	}

	if ( pParent->GetParent() != NULL )
	{
		pParent->VisGroups_UpdateParent( pParent->GetVisible() );
	}
}