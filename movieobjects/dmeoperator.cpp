//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================
#include "movieobjects/dmeoperator.h"
#include "datamodel/dmelementfactoryhelper.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ABSTRACT_ELEMENT( DmeOperator, CDmeOperator );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeOperator::OnConstruction()
{
}

void CDmeOperator::OnDestruction()
{
}

//-----------------------------------------------------------------------------
// IsDirty - ie needs to operate
//-----------------------------------------------------------------------------
bool CDmeOperator::IsDirty()
{
	return BaseClass::IsDirty();
}
