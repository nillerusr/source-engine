//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "stdafx.h"
#include "maplight.h"
#include "hammer.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


IMPLEMENT_MAPCLASS( CMapLight );


CMapClass* CMapLight::CreateMapLight(CHelperInfo *pHelperInfo, CMapEntity *pParent)
{
	return new CMapLight;
}


void CMapLight::OnParentKeyChanged(const char* key, const char* value)
{
	
}


CMapClass *CMapLight::Copy(bool bUpdateDependencies)
{
	CMapLight *pNew = new CMapLight;
	pNew->CopyFrom( this, bUpdateDependencies );
	return pNew;
}


//-----------------------------------------------------------------------------
// Purpose: Never select anything because of this helper.
//-----------------------------------------------------------------------------
CMapClass *CMapLight::PrepareSelection(SelectMode_t eSelectMode)
{
	return NULL;
}
