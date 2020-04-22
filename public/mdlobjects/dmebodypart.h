//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Body part base class
//
// Body parts can be either:
//
//  * A list of LODs (DmeLODList)
//  * empty (DmeBlankBodyPart)
//
//===========================================================================//

#ifndef DMEBODYPART_H
#define DMEBODYPART_H


#ifdef _WIN32
#pragma once
#endif


#include "datamodel/dmelement.h"
#include "datamodel/dmattributevar.h"
#include "mdlobjects/dmelod.h"


//-----------------------------------------------------------------------------
// A generic body part
//-----------------------------------------------------------------------------
class CDmeBodyPart : public CDmElement
{
	DEFINE_ELEMENT( CDmeBodyPart, CDmElement );

public:
	// Returns the number of LODs in this body part, can be 0
	virtual int LODCount() const { return 0; }

	// Returns the root LOD. This is the one with the switch metric 0
	virtual CDmeLOD *GetRootLOD() { return NULL; }

	// Returns the shadow LOD
	virtual CDmeLOD *GetShadowLOD() { return NULL; }
};


#endif // DMEBODYPART_H
