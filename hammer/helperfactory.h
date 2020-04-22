//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef HELPERFACTORY_H
#define HELPERFACTORY_H
#pragma once


class CHelperInfo;
class CMapClass;
class CMapEntity;


class CHelperFactory
{
	public:

		static CMapClass *CreateHelper(CHelperInfo *pHelperInfo, CMapEntity *pParent);
};

#endif // HELPERFACTORY_H
