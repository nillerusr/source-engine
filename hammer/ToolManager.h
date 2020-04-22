//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef TOOLMANAGER_H
#define TOOLMANAGER_H
#ifdef _WIN32
#pragma once
#endif

#include "ToolInterface.h"
#include "utlvector.h"


class CToolAxisHandle;
class CToolDecal;
class CToolDisplace;
class CToolMagnify;
class CToolMaterial;
class CToolPickAngles;
class CToolPickEntity;
class CToolPickFace;
class CToolPointHandle;
class CToolSphere;
class CBaseTool;
class CToolSweptPlayerHull;
class CChunkHandlerMap;

class CToolManager
{
public:

	CToolManager();
	~CToolManager();

	bool Init(CMapDoc *pDocument);
	void Shutdown();

	CBaseTool *GetActiveTool();
	ToolID_t GetActiveToolID();				

	CBaseTool *GetToolForID(ToolID_t eToolID);

	void SetTool(ToolID_t nToolID); // changes current tool without touching the tool stack
	void PushTool(ToolID_t nToolID); // activates a new tool and put current tool on stack
	void PopTool(); // restores last tool on stack

	inline int GetToolCount();
	inline CBaseTool *GetTool(int nIndex);

	void RemoveAllTools();
	void AddTool(CBaseTool *pTool);

	static ChunkFileResult_t LoadCallback(CChunkFile *pFile, CBaseTool *pTool);
	void AddToolHandlers( CChunkHandlerMap *pHandlersMap );
	ChunkFileResult_t SaveVMF(CChunkFile *pFile, CSaveInfo *pSaveInfo);
	ChunkFileResult_t LoadVMF(CChunkFile *pFile);

private:

	void ActivateTool( CBaseTool *pTool );
	void DeactivateTool( CBaseTool *pTool );

	CUtlVector<CBaseTool *> m_Tools;			// List of ALL the tools.
	
	CMapDoc		*m_pDocument;					// document the manager is responisble for
    CBaseTool	*m_pActiveTool;					// Pointer to the active new tool, NULL if none.
	
	CUtlVector<ToolID_t> m_ToolIDStack;			// Stack of active tool IDs, for PushTool/PopTool.
};

//-----------------------------------------------------------------------------
// Purpose: Accessor for iterating tools.
//-----------------------------------------------------------------------------
int CToolManager::GetToolCount()
{
	return m_Tools.Count();
}


//-----------------------------------------------------------------------------
// Purpose: Accessor for iterating tools.
//-----------------------------------------------------------------------------
CBaseTool *CToolManager::GetTool(int nIndex)
{
	return m_Tools.Element(nIndex);
}

// get the tool manager for the current active document:
CToolManager *ToolManager();



#endif // TOOLMANAGER_H
