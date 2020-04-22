//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Dme representation of QC: $sequence
//
//===========================================================================//

#ifndef DMESEQUENCE_H
#define DMESEQUENCE_H


#ifdef _WIN32
#pragma once
#endif


#include "datamodel/dmattributevar.h"
#include "datamodel/dmelement.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CDmeDag;
class CDmeAnimationList;


//-----------------------------------------------------------------------------
// Animation data
//-----------------------------------------------------------------------------
class CDmeSequence : public CDmElement
{
	DEFINE_ELEMENT( CDmeSequence, CDmElement );

public:
	CDmaElement< CDmeDag > m_Skeleton;
	CDmaElement< CDmeAnimationList > m_AnimationList;

};


#endif // DMESEQUENCE_H
