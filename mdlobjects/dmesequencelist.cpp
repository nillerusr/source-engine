//========= Copyright Valve Corporation, All rights reserved. ============//
//
// A list of DmeSequences's
//
//===========================================================================//


#include "datamodel/dmelementfactoryhelper.h"
#include "mdlobjects/dmesequence.h"
#include "mdlobjects/dmesequencelist.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeSequenceList, CDmeSequenceList );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeSequenceList::OnConstruction()
{
	m_Sequences.Init( this, "sequences" );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeSequenceList::OnDestruction()
{
}