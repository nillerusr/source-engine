//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef MAPLIGHT_H
#define MAPLIGHT_H
#ifdef _WIN32
#pragma once
#endif


#include "MapHelper.h"
#include "fgdlib/HelperInfo.h"
#include "MapEntity.h"


class CMapLight : public CMapHelper
{
public:

	DECLARE_MAPCLASS(CMapLight,CMapHelper);


	static CMapClass *CreateMapLight(CHelperInfo *pHelperInfo, CMapEntity *pParent);

	virtual void OnParentKeyChanged(const char* key, const char* value);
	virtual CMapClass *Copy(bool bUpdateDependencies);

	virtual CMapClass *PrepareSelection(SelectMode_t eSelectMode);
};


#endif // MAPLIGHT_H
