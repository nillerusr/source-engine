//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Dme version of a list of body groups. Each body group contains a list
// of LOD lists which are the various options to switch in for that part of the body
//
//===========================================================================//

#ifndef DMEBODYGROUPLIST_H
#define DMEBODYGROUPLIST_H

#ifdef _WIN32
#pragma once
#endif

#include "datamodel/dmelement.h"
#include "datamodel/dmattributevar.h"


//-----------------------------------------------------------------------------
// Forward Declarations
//-----------------------------------------------------------------------------
class CDmeBodyGroup;
class CDmeLODList;


//-----------------------------------------------------------------------------
// A class representing a list of body groups
//-----------------------------------------------------------------------------
class CDmeBodyGroupList : public CDmElement
{
	DEFINE_ELEMENT( CDmeBodyGroupList, CDmElement );

public:
	CDmeBodyGroup *FindBodyGroup( const char *pName );

	// Gets the 'main' body part (used for compilation)
	CDmeLODList *GetMainBodyPart();

	CDmaElementArray< CDmeBodyGroup >	m_BodyGroups;
};


#endif // DMEBODYGROUPLIST_H
