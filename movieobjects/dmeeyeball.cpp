//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=============================================================================


// Valve includes
#include "datamodel/dmelementfactoryhelper.h"
#include "movieobjects/dmeeyeball.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeEyeball, CDmeEyeball );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeEyeball::OnConstruction()
{
	m_flDiameter.InitAndSet( this, "diameter", 1.0 );
	m_flYawAngle.InitAndSet( this, "angle", 2.0 );
	m_flPupilScale.InitAndSet( this, "pupilScale", 1.0 );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeEyeball::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// Returns the model relative position of the eyeball Position
//-----------------------------------------------------------------------------
void CDmeEyeball::GetWorldPosition( Vector &worldPosition )
{
	matrix3x4_t mWorld;
	GetShapeToWorldTransform( mWorld );

	MatrixPosition( mWorld, worldPosition );
}
