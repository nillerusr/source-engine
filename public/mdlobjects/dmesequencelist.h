//========= Copyright Valve Corporation, All rights reserved. ============//
//
// A list of DmeSequences's
//
//===========================================================================//


#ifndef DMESEQUENCELIST_H
#define DMESEQUENCELIST_H


#ifdef _WIN32
#pragma once
#endif


#include "datamodel/dmattributevar.h"
#include "mdlobjects/dmemdllist.h"


//-----------------------------------------------------------------------------
// Forward Declarations
//-----------------------------------------------------------------------------
class CDmeSequence;


//-----------------------------------------------------------------------------
// A class representing a list of sequences
//-----------------------------------------------------------------------------
class CDmeSequenceList : public CDmeMdlList
{
	DEFINE_ELEMENT( CDmeSequenceList, CDmeMdlList );

public:
	virtual CDmAttribute *GetListAttr() { return m_Sequences.GetAttribute(); }

	CDmaElementArray< CDmeSequence > m_Sequences;

};


#endif // DMESEQUENCELIST_H
