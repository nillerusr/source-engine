//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Dme blank/empty body part base class
//
//===========================================================================//


#include "datamodel/dmelementfactoryhelper.h"
#include "mdlobjects/dmeblankbodypart.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeBlankBodyPart, CDmeBlankBodyPart );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeBlankBodyPart::OnConstruction()
{
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeBlankBodyPart::OnDestruction()
{
}