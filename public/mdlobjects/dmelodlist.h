//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Dme version of a list of lods
//
//===========================================================================//

#ifndef DMELODLIST_H
#define DMELODLIST_H

#ifdef _WIN32
#pragma once
#endif

#include "mdlobjects/dmebodypart.h"


//-----------------------------------------------------------------------------
// Forward Declarations
//-----------------------------------------------------------------------------
class CDmeLOD;

				 
//-----------------------------------------------------------------------------
// A class representing a list of LODs
//-----------------------------------------------------------------------------
class CDmeLODList : public CDmeBodyPart
{
	DEFINE_ELEMENT( CDmeLODList, CDmeBodyPart );

public:
	// Returns the number of LODs in this body part, can be 0
	virtual int LODCount() const;

	// Returns the root LOD. This is the one with the switch metric 0
	virtual CDmeLOD *GetRootLOD();

	// Returns the shadow LOD
	virtual CDmeLOD *GetShadowLOD();

	// NOTE: It may be possible to eliminate the skeleton here
	// and assume the LOD always uses the root skeleton.
	CDmaElementArray< CDmeLOD > m_LODs;

};


#endif // DMELODLIST_H
