//========= Copyright Valve Corporation, All rights reserved. ============//
//
// A Dme element intended to be a base class for a common pattern of
// MDLOBJECTS, that is an element which contains simply one attribute
// of type AT_ELEMENTARRAY
//
//===========================================================================//


#ifndef DMEMDLLIST_H
#define DMEMDLLIST_H


#ifdef _WIN32
#pragma once
#endif


#include "datamodel/dmattributevar.h"
#include "datamodel/dmelement.h"


//-----------------------------------------------------------------------------
// A base class intended to be used for the common pattern in MDLOBJECTS
// of an element which is nothing but a container for an array of element
// attributes
//-----------------------------------------------------------------------------
class CDmeMdlList : public CDmElement
{
	DEFINE_ELEMENT( CDmeMdlList, CDmElement );

public:
	virtual CDmAttribute *GetListAttr() { return NULL; }

};


#endif // DMEMDLLIST_H
