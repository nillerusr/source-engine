//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//			TODO: add an autoregistration system for tools a la LINK_ENTITY_TO_CLASS
//=============================================================================//

#include "stdafx.h"
#include "MapDoc.h"
#include "MainFrm.h"
#include "MapView2D.h"			// FIXME: for MapView2D::updTool
#include "ToolAxisHandle.h"
#include "ToolDecal.h"
#include "ToolDisplace.h"
#include "ToolManager.h"
#include "ToolMagnify.h"
#include "ToolMaterial.h"
#include "ToolPickFace.h"
#include "ToolPickAngles.h"
#include "ToolPickEntity.h"
#include "ToolPointHandle.h"
#include "ToolSphere.h"
#include "ToolSweptHull.h"
#include "ToolBlock.h"
#include "ToolCamera.h"
#include "ToolClipper.h"
#include "ToolCordon.h"
#include "ToolEntity.h"
#include "ToolMorph.h"
#include "ToolOverlay.h"
#include "ToolSelection.h"
#include "ToolMagnify.h"
#include "ToolMaterial.h"
#include "ChunkFile.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

static CToolManager s_DummyToolmanager;

CToolManager* ToolManager()
{
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();

	if ( pDoc )
		return pDoc->GetTools();

	return &s_DummyToolmanager;
}
//-----------------------------------------------------------------------------
// Purpose: Prepares the tool manager for use.
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CToolManager::Init( CMapDoc *pDocument )
{
	
	// add default tools

	//
	// Create the tools that are held by the tool manager and add them
	// to the internal tools list.
	//
	
	RemoveAllTools();

	m_pDocument = pDocument;

	AddTool( new CToolDisplace );
	AddTool( new CToolMagnify );
	AddTool( new CToolDecal );
	AddTool( new CToolMaterial );
	AddTool( new CToolAxisHandle );
	AddTool( new CToolPointHandle );
	AddTool( new CToolSphere );
	AddTool( new CToolPickAngles );
	AddTool( new CToolPickEntity );
	AddTool( new CToolPickFace );
	AddTool( new CToolSweptPlayerHull );
	AddTool( new Selection3D );
	AddTool( new CToolBlock );
	AddTool( new CToolEntity );
	AddTool( new Camera3D );
	AddTool( new Morph3D );
	AddTool( new Clipper3D );
	AddTool( new Cordon3D );
	AddTool( new CToolOverlay );
	

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Shuts down the tool manager - called on app exit.
//-----------------------------------------------------------------------------
void CToolManager::Shutdown()
{
	m_pActiveTool = NULL;
	m_pDocument = NULL;
	RemoveAllTools();
}


//-----------------------------------------------------------------------------
// Purpose: Constructor. Allocates the tools.
//-----------------------------------------------------------------------------
CToolManager::CToolManager()
{
    m_pActiveTool = NULL;
	m_pDocument = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Destructor. Deletes the tools.
//-----------------------------------------------------------------------------
CToolManager::~CToolManager()
{
	Shutdown();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CToolManager::AddTool(CBaseTool *pTool)
{
	if ( GetToolForID( pTool->GetToolID() ) )
	{
		Assert( !pTool );
		Msg("CToolManager::AddTool: Tool %i already registered.\n", pTool->GetToolID());
		return;
	}

	pTool->Init( m_pDocument );

	m_Tools.AddToTail(pTool);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseTool *CToolManager::GetActiveTool()
{
	return m_pActiveTool;
}


//-----------------------------------------------------------------------------
// Purpose: Returns a tool pointer for a given tool ID, NULL if there is no
//			corresponding tool.
//-----------------------------------------------------------------------------
CBaseTool *CToolManager::GetToolForID(ToolID_t eToolID)
{
	int nToolCount = GetToolCount();
	for (int i = 0; i < nToolCount; i++)
	{
		CBaseTool *pTool = GetTool(i);
		if (pTool->GetToolID() == eToolID)
		{
			return pTool;
		}
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Returns the ID of the active tool.
//-----------------------------------------------------------------------------
ToolID_t CToolManager::GetActiveToolID()
{
	if ( m_pActiveTool )
		return m_pActiveTool->GetToolID();
	else
        return TOOL_NONE;
}


//-----------------------------------------------------------------------------
// Purpose: Pushes a new tool onto the tool stack and activates it. The active
//			tool will be deactivated and reactivated when PopTool is called.
//-----------------------------------------------------------------------------
void CToolManager::PushTool(ToolID_t eToolID)
{
	//
	// Add the new tool to the top of the tool stack.
	//
	if (eToolID != GetActiveToolID())
	{
		m_ToolIDStack.AddToHead(GetActiveToolID());
	}

	SetTool(eToolID);
}


//-----------------------------------------------------------------------------
// Purpose: Restores the active tool to what it was when PushTool was called.
//			If the stack is underflowed somehow, the pointer is restored.
//-----------------------------------------------------------------------------
void CToolManager::PopTool()
{
	int nCount = m_ToolIDStack.Count();
	if (nCount > 0)
	{
		ToolID_t eNewTool = m_ToolIDStack.Element(0);
		m_ToolIDStack.Remove(0);

		SetTool(eNewTool);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Sets the current active tool by ID.
// Input  : iTool - ID of the tool to activate.
//-----------------------------------------------------------------------------
void CToolManager::SetTool(ToolID_t eNewTool)
{
	CBaseTool *pNewTool = GetToolForID(eNewTool);
	CBaseTool *pOldTool = m_pActiveTool;

	// Check to see that we can deactive the current tool
	if ( pOldTool && (pOldTool != pNewTool) )
	{
		// Deactivate the current tool unless we are just 'reactivating' it.
		if( !pOldTool->CanDeactivate() )
			return;
	}

	// set active tool to new tool already so old tool can peek whats coming next
	m_pActiveTool = pNewTool; 

	// deactivate the old tool if different.
	if ( pOldTool && (pOldTool != pNewTool) )
	{
		pOldTool->Deactivate();
	}

	// always activate the new tool
	if ( pNewTool )
	{
		pNewTool->Activate();
	}

	// FIXME: When we start up, we end up here before the main window is created because
	//		  CFaceEditDispPage::OnSetActive() calls SetTool(TOOL_FACEEDIT_DISP). This
	//		  behavior is rather nonsensical during startup.
	CMainFrame *pwndMain = GetMainWnd();
	if (pwndMain != NULL)
	{
		pwndMain->m_ObjectBar.UpdateListForTool(eNewTool);
	}

	if ( m_pDocument )
		m_pDocument->UpdateAllViews( MAPVIEW_UPDATE_TOOL );
}

ChunkFileResult_t CToolManager::SaveVMF(CChunkFile *pFile, CSaveInfo *pSaveInfo) 
{
	for (int i=0;i<m_Tools.Count(); i++)
	{
		if ( m_Tools[i]->GetVMFChunkName() != NULL  )
		{
			m_Tools[i]->SaveVMF( pFile, pSaveInfo );
		}
	}

	return ChunkFile_Ok;
}


ChunkFileResult_t CToolManager::LoadCallback(CChunkFile *pFile, CBaseTool *pTool)
{
	return pTool->LoadVMF( pFile );
}


void CToolManager::AddToolHandlers( CChunkHandlerMap *pHandlersMap )
{
	for (int i=0;i<m_Tools.Count(); i++)
	{
		if ( m_Tools[i]->GetVMFChunkName() != NULL  )
		{
			pHandlersMap->AddHandler( m_Tools[i]->GetVMFChunkName(), (ChunkHandler_t)LoadCallback, m_Tools[i] );
		}
	}
}

	 

//-----------------------------------------------------------------------------
// Purpose: Removes all the document-created tools from the tools list.
//-----------------------------------------------------------------------------
void CToolManager::RemoveAllTools()
{
	m_pActiveTool = NULL;
	m_Tools.PurgeAndDeleteElements();
	m_ToolIDStack.RemoveAll();
}


