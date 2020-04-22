//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "stdafx.h"
#include "MapHelper.h"
#include "mapworld.h"
#include "globalfunctions.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


//-----------------------------------------------------------------------------
// Purpose: Returns the appropriate object to the selection code.
// Input  : dwFlags - selectPicky or selectNormal
// Output : Returns a pointer to the object that should be selected, based on
//			the selection mode.
//-----------------------------------------------------------------------------
CMapClass *CMapHelper::PrepareSelection(SelectMode_t eSelectMode)
{
	//
	// If we have a parent that is not the world, select our parent.
	//
	if (m_pParent && !IsWorldObject(m_pParent))
	{
		return GetParent()->PrepareSelection(eSelectMode);
	}

	//
	// We should never have a helper as a child of the world!
	//
	Assert(false);
	return NULL;
}

