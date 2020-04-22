//========= Copyright Valve Corporation, All rights reserved. ============//
//
// A general list element for the MDL pipeline
//
//===========================================================================//


#include "datamodel/dmelementfactoryhelper.h"
#include "mdlobjects/dmemdllist.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeMdlList, CDmeMdlList );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeMdlList::OnConstruction()
{
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeMdlList::OnDestruction()
{
}

