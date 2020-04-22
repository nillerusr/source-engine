//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Dme version of a body groups. Each body group contains a list
// of LOD lists which are the various options to switch in for that part
// of the body
//
//===========================================================================//

#ifndef DMEBODYGROUP_H
#define DMEBODYGROUP_H

#ifdef _WIN32
#pragma once
#endif

#include "datamodel/dmelement.h"
#include "datamodel/dmattributevar.h"


//-----------------------------------------------------------------------------
// Forward Declarations
//-----------------------------------------------------------------------------
class CDmeBodyPart;
class CDmeLODList;


//-----------------------------------------------------------------------------
// A class representing a body group. Each element of the body parts array
// is an option that can be switched into that part of the body.
//-----------------------------------------------------------------------------
class CDmeBodyGroup : public CDmElement
{
	DEFINE_ELEMENT( CDmeBodyGroup, CDmElement );

public:
	// Finds a body part by name 
	CDmeLODList *FindBodyPart( const char *pName );

	CDmaElementArray< CDmeBodyPart > m_BodyParts;
};


#endif // DMEBODYGROUP_H
