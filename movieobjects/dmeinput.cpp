//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================
#include "movieobjects/dmeinput.h"
#include "datamodel/dmelementfactoryhelper.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ABSTRACT_ELEMENT( DmeInput, CDmeInput );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeInput::OnConstruction()
{
}

void CDmeInput::OnDestruction()
{
}

//-----------------------------------------------------------------------------
// IsDirty - ie needs to operate
//-----------------------------------------------------------------------------
bool CDmeInput::IsDirty()
{
	return true;
}
