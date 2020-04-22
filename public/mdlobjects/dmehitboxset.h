//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Dme version of a hitbox set
//
//===========================================================================//

#ifndef DMEHITBOXSET_H
#define DMEHITBOXSET_H

#ifdef _WIN32
#pragma once
#endif

#include "datamodel/dmelement.h"
#include "datamodel/dmattributevar.h"
#include "mdlobjects/dmehitbox.h"


//-----------------------------------------------------------------------------
// A class representing an attachment point
//-----------------------------------------------------------------------------
class CDmeHitboxSet : public CDmElement
{
	DEFINE_ELEMENT( CDmeHitboxSet, CDmElement );

public:
	CDmaElementArray< CDmeHitbox > m_Hitboxes;	

private:
};


#endif // DMEHITBOXSET_H
