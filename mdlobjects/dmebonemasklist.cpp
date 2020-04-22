//========= Copyright Valve Corporation, All rights reserved. ============//
//
// A list of DmeBoneWeight elements, replacing QC's $WeightList
//
//===========================================================================//


#include "datamodel/dmelementfactoryhelper.h"
#include "mdlobjects/dmebonemask.h"
#include "mdlobjects/dmebonemasklist.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeBoneMaskList, CDmeBoneMaskList );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeBoneMaskList::OnConstruction()
{
	m_BoneMasks.Init( this, "boneMasks" );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeBoneMaskList::OnDestruction()
{
}