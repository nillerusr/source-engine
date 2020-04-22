//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef BASEPROPERTIESCONTAINER_H
#define BASEPROPERTIESCONTAINER_H
#ifdef _WIN32
#pragma once
#endif

#include "dme_controls/ElementPropertiesTree.h"

class CBasePropertiesContainer : public CElementPropertiesTreeInternal
{
	DECLARE_CLASS_SIMPLE( CBasePropertiesContainer, CElementPropertiesTreeInternal);

public:

	CBasePropertiesContainer( vgui::Panel *parent, IDmNotify *pNotify, CDmeEditorTypeDictionary *pDict = NULL );

	virtual bool IsDroppable( CUtlVector< KeyValues * >& msglist );
	virtual void OnPanelDropped( CUtlVector< KeyValues * >& msglist );
};

#endif // BASEPROPERTIESCONTAINER_H
