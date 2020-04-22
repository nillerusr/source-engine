//========= Copyright Valve Corporation, All rights reserved. ============//
//
// A list of DmeBoneWeight elements, replacing QC's $WeightList
//
//===========================================================================//


#ifndef DMEBONEMASK_H
#define DMEBONEMASK_H


#ifdef _WIN32
#pragma once
#endif


#include "datamodel/dmattributevar.h"
#include "mdlobjects/dmemdllist.h"


//-----------------------------------------------------------------------------
// Forward Declarations
//-----------------------------------------------------------------------------
class CDmeBoneWeight;


//-----------------------------------------------------------------------------
// A class representing a list of bone weights
//-----------------------------------------------------------------------------
class CDmeBoneMask : public CDmeMdlList
{
	DEFINE_ELEMENT( CDmeBoneMask, CDmeMdlList );

public:
	virtual CDmAttribute *GetListAttr() { return m_BoneWeights.GetAttribute(); }

private:
	CDmaElementArray< CDmeBoneWeight > m_BoneWeights;
	CDmaVar< bool > m_bDefault;

};


#endif // DMEBONEMASK_H
