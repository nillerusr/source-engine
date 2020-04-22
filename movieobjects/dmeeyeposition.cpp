//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=============================================================================


// Valve includes
#include "datamodel/dmelementfactoryhelper.h"
#include "movieobjects/dmeeyeposition.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeEyePosition, CDmeEyePosition );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeEyePosition::OnConstruction()
{
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeEyePosition::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// Returns the model relative position of the eyePosition
//-----------------------------------------------------------------------------
void CDmeEyePosition::GetWorldPosition( Vector &worldPosition )
{
	matrix3x4_t mWorld;
	GetShapeToWorldTransform( mWorld );

	MatrixPosition( mWorld, worldPosition );
}
