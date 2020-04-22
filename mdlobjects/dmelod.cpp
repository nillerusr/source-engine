//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Dme version of a hitbox
//
//===========================================================================//

#include "mdlobjects/dmelod.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "movieobjects/dmemodel.h"
#include "movieobjects/dmedag.h"
#include "movieobjects/dmecombinationoperator.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeLOD, CDmeLOD );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeLOD::OnConstruction()
{
	m_Model.Init( this, "model" );
	m_Skeleton.Init( this, "skeleton" );
	m_CombinationOperator.Init( this, "combinationOperator" );
	m_flSwitchMetric.Init( this, "switchMetric" );
	m_bNoFlex.Init( this, "noFlex" );
	m_bIsShadowLOD.Init( this, "isShadowLOD" );
}

void CDmeLOD::OnDestruction()
{
}