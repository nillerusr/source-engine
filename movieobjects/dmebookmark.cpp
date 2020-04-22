//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================
#include "movieobjects/dmebookmark.h"
#include "tier0/dbg.h"
#include "datamodel/dmelementfactoryhelper.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Class factory 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeBookmark, CDmeBookmark );


//-----------------------------------------------------------------------------
// Constructor, destructor 
//-----------------------------------------------------------------------------
void CDmeBookmark::OnConstruction()
{
	m_Time.InitAndSet( this, "time", 0 );
	m_Duration.InitAndSet( this, "duration", 0 );
	m_Note.Init( this, "note" );
}

void CDmeBookmark::OnDestruction()
{
}
