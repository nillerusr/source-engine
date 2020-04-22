//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Defines a base class for all helper objects. Helper objects are
//			subordinate to their entity parents, and provide services such as
//			enhanced presentation and manipulation of keyvalues for their parent
//			entity.
//
//			Like all children, helpers are transformed with their parent.
//
// $NoKeywords: $
//=============================================================================//

#ifndef MAPHELPER_H
#define MAPHELPER_H
#ifdef _WIN32
#pragma once
#endif


#include "MapClass.h"


class CMapHelper : public CMapClass
{
public:

	//
	// Serialization.
	//
	virtual bool ShouldSerialize(void) { return(false); }

	virtual CMapClass *PrepareSelection(SelectMode_t eSelectMode);
};


#endif // MAPHELPER_H
