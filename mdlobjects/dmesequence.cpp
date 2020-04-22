//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Dme representation of QC: $sequence
//
//===========================================================================//


#include "datamodel/dmelementfactoryhelper.h"
#include "movieobjects/dmeanimationlist.h"
#include "movieobjects/dmechannel.h"
#include "movieobjects/dmedag.h"
#include "mdlobjects/dmesequence.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeSequence, CDmeSequence );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeSequence::OnConstruction()
{
	m_Skeleton.Init( this, "skeleton" );
	m_AnimationList.Init( this, "animationList" );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeSequence::OnDestruction()
{
}