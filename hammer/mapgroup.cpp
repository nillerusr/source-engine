//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "stdafx.h"
#include "MapDefs.h"
#include "MapGroup.h"
#include "SaveInfo.h"
#include "hammer.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


IMPLEMENT_MAPCLASS(CMapGroup)


//-----------------------------------------------------------------------------
// Purpose: Sets the new child's color to our own.
// Input  : pChild - Object being added to this group.
//-----------------------------------------------------------------------------
void CMapGroup::AddChild(CMapClass *pChild)
{
	pChild->SetRenderColor(r,g,b);
	CMapClass::AddChild(pChild);
	Vector2D	mins, maxs;
	GetRenderLogicalBox(mins, maxs);
	m_vecLogicalPosition = ( mins + maxs ) / 2;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pobj - 
// Output : CMapClass *
//-----------------------------------------------------------------------------
CMapClass *CMapGroup::CopyFrom(CMapClass *pobj, bool bUpdateDependencies)
{
	return(CMapClass::CopyFrom(pobj, bUpdateDependencies));
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : CMapClass *
//-----------------------------------------------------------------------------
CMapClass *CMapGroup::Copy(bool bUpdateDependencies)
{
	CMapGroup *pNew = new CMapGroup;
	return(pNew->CopyFrom(this, bUpdateDependencies));
}


//-----------------------------------------------------------------------------
// Purpose: Returns a string describing this group.
//-----------------------------------------------------------------------------
const char* CMapGroup::GetDescription(void)
{
	static char szBuf[128];
	sprintf(szBuf, "group of %d objects", m_Children.Count());
	return(szBuf);
}


void CMapGroup::SetLogicalPosition( const Vector2D &vecPosition )
{
	if ( ( m_vecLogicalPosition.x != COORD_NOTINIT  )
	  && ( m_vecLogicalPosition.y != COORD_NOTINIT )
	  && ( vecPosition != m_vecLogicalPosition ) )
	{
		Vector2D	vecDelta = vecPosition - m_vecLogicalPosition;
		
		FOR_EACH_OBJ( m_Children, pos )
		{		
			CMapClass *pobj = m_Children[pos];
			// update logical bounds
			pobj->SetLogicalPosition( pobj->GetLogicalPosition() + vecDelta );
		}

	}
	m_vecLogicalPosition = vecPosition;
}

const Vector2D& CMapGroup::GetLogicalPosition()
{
	return m_vecLogicalPosition;
}

void CMapGroup::GetRenderLogicalBox( Vector2D &mins, Vector2D &maxs ) 
{ 
	mins.Init( COORD_NOTINIT, COORD_NOTINIT ); 
	maxs.Init( -COORD_NOTINIT, -COORD_NOTINIT ); 

	FOR_EACH_OBJ( m_Children, pos )
	{		
		CMapClass *pobj = m_Children[pos];
		// update logical bounds
		Vector2D logicalMins,logicalMaxs;
		pobj->GetRenderLogicalBox( logicalMins, logicalMaxs );
		mins = mins.Min(logicalMins);
		maxs = maxs.Max(logicalMaxs);
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pFile - 
// Output : ChunkFileResult_t
//-----------------------------------------------------------------------------
ChunkFileResult_t CMapGroup::LoadVMF(CChunkFile *pFile)
{
	CChunkHandlerMap Handlers;
	Handlers.AddHandler("editor", (ChunkHandler_t)CMapClass::LoadEditorCallback, this);

	pFile->PushHandlers(&Handlers);
	ChunkFileResult_t eResult = pFile->ReadChunk((KeyHandler_t)CMapClass::LoadEditorKeyCallback, this);
	pFile->PopHandlers();

	return(eResult);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pFile - 
// Output : ChunkFileResult_t
//-----------------------------------------------------------------------------
ChunkFileResult_t CMapGroup::SaveVMF(CChunkFile *pFile, CSaveInfo *pSaveInfo)
{
	//
	// Check rules before saving this object.
	//
	if (!pSaveInfo->ShouldSaveObject(this))
	{
		return(ChunkFile_Ok);
	}

	ChunkFileResult_t eResult = pFile->BeginChunk("group");

	//
	// Save the group's ID.
	//
	if (eResult == ChunkFile_Ok)
	{
		eResult = pFile->WriteKeyValueInt("id", GetID());
	}

	if (eResult == ChunkFile_Ok)
	{
		eResult = CMapClass::SaveVMF(pFile, pSaveInfo);
	}	

	if (eResult == ChunkFile_Ok)
	{
		eResult = pFile->EndChunk();
	}

	return(eResult);
}


//-----------------------------------------------------------------------------
// Purpose: Groups don't accept visgroups themselves, they 
// Input  : *pVisGroup - 
//-----------------------------------------------------------------------------
void CMapGroup::AddVisGroup(CVisGroup *pVisGroup)
{
	FOR_EACH_OBJ( m_Children, pos )
	{
		m_Children[pos]->AddVisGroup( pVisGroup );
	}
}

