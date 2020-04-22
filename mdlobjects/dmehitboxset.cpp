//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Dme version of a hitbox set
//
//===========================================================================//

#include "mdlobjects/dmehitboxset.h"
#include "datamodel/dmelementfactoryhelper.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeHitboxSet, CDmeHitboxSet );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeHitboxSet::OnConstruction()
{
	m_Hitboxes.Init( this, "hitboxes" );
}

void CDmeHitboxSet::OnDestruction()
{
}


