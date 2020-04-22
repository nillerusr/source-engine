//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//

#ifndef MODELFACTORY_H
#define MODELFACTORY_H
#pragma once


class CMapClass;


enum ModelType_t
{
	ModelTypeStudio = 0
};


class CModelFactory
{
	public:

		static CMapClass *CreateModel(ModelType_t eModelType, const char *pszModelData);
};


#endif // MODELFACTORY_H
