//========= Copyright Valve Corporation, All rights reserved. ============//
//
// A list of DmeBoneWeight elements, replacing QC's $WeightList
//
//===========================================================================//


#include "datamodel/dmelementfactoryhelper.h"
#include "mdlobjects/dmeboneweight.h"
#include "mdlobjects/dmebonemask.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeBoneMask, CDmeBoneMask );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeBoneMask::OnConstruction()
{
	m_BoneWeights.Init( this, "boneWeights" );
	m_bDefault.InitAndSet( this, "default", false );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeBoneMask::OnDestruction()
{
}