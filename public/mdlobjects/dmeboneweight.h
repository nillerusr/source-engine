//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Dme version of a bone weight as in QC $WeightList
//
//===========================================================================//


#ifndef DMEBONEWEIGHT_H
#define DMEBONEWEIGHT_H


#ifdef _WIN32
#pragma once
#endif


#include "datamodel/dmattributevar.h"
#include "datamodel/dmelement.h"


//-----------------------------------------------------------------------------
// A class representing a bone weight
//-----------------------------------------------------------------------------
class CDmeBoneWeight : public CDmElement
{
	DEFINE_ELEMENT( CDmeBoneWeight, CDmElement );

public:
	CDmaVar< float > m_flWeight;

};


#endif // DMEBONEWEIGHT_H
