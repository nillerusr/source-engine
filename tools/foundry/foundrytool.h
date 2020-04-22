//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Foundry tool; main UI smarts class
//
//=============================================================================

#ifndef FOUNDRYTOOL_H
#define FOUNDRYTOOL_H

#ifdef _WIN32
#pragma once
#endif

#include "tier0/platform.h"
#include "datamodel/idatamodel.h"

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CDmeEditorTypeDictionary;
class CDmeVMFEntity;

namespace vgui
{
	class Panel;
}


//-----------------------------------------------------------------------------
// Singleton interfaces
//-----------------------------------------------------------------------------
extern CDmeEditorTypeDictionary *g_pEditorTypeDict;


//-----------------------------------------------------------------------------
// Allows the doc to call back into the Foundry editor tool
//-----------------------------------------------------------------------------
abstract_class IFoundryDocCallback
{
public:
	// Called by the doc when the data changes
	virtual void OnDocChanged( const char *pReason, int nNotifySource, int nNotifyFlags ) = 0;
};


//-----------------------------------------------------------------------------
// Global methods of the foundry tool
//-----------------------------------------------------------------------------
abstract_class IFoundryTool
{
public:
	// Gets at the rool panel (for modal dialogs)
	virtual vgui::Panel *GetRootPanel() = 0;

	// Gets the registry name (for saving settings)
	virtual const char *GetRegistryName() = 0;

	// Shows a particular entity in the entity properties dialog
	virtual void ShowEntityInEntityProperties( CDmeVMFEntity *pEntity ) = 0;
};

extern IFoundryTool *g_pFoundryTool;


#endif // FOUNDRYTOOL_H
