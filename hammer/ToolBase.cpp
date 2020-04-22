//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "stdafx.h"
#include "ToolInterface.h"
#include "mapdoc.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

void CBaseTool::Init( CMapDoc *pDocument )
{
	m_bActiveTool = false;
	m_pDocument = pDocument;
}

//-----------------------------------------------------------------------------
// Purpose: Called whtn this tool is becoming the active tool.
// Input  : eOldTool - The tool that was previously active.
//-----------------------------------------------------------------------------
void CBaseTool::Activate()
{
	OnActivate();

	m_pDocument->UpdateAllViews( MAPVIEW_UPDATE_TOOL );

	m_bActiveTool = true;
}

//-----------------------------------------------------------------------------
// Purpose: Called when this tool is no longer the active tool.
// Input  : eNewTool - The tool that is being activated.
//-----------------------------------------------------------------------------
void CBaseTool::Deactivate()
{
	OnDeactivate();

	if ( m_pDocument )
		m_pDocument->UpdateAllViews( MAPVIEW_UPDATE_TOOL );

	m_bActiveTool = false;
}