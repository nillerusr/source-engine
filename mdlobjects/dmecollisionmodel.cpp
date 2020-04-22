//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Dme version of a collisionmodel
//
//===========================================================================//

#include "mdlobjects/dmecollisionmodel.h"
#include "datamodel/dmelementfactoryhelper.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeCollisionModel, CDmeCollisionModel );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeCollisionModel::OnConstruction()
{
	m_flMass.Init( this, "mass" );
	m_bAutomaticMassComputation.Init( this, "automaticMassComputation" );
	m_flInertia.Init( this, "inertia" );
	m_flDamping.Init( this, "damping" );
	m_flRotationalDamping.Init( this, "rotationalDamping" );
	m_flDrag.Init( this, "drag" );
	m_flRollingDrag.Init( this, "rollingDrag" );
	m_nMaxConvexPieces.Init( this, "maxConvexPieces" );
	m_bRemove2D.Init( this, "remove2d" );
	m_bConcavePerJoint.Init( this, "concavePerJoint" );
	m_flWeldPositionTolerance.Init( this, "weldPositionTolerance" );
	m_flWeldNormalTolerance.Init( this, "weldNormalTolerance" );
	m_bConcave.Init( this, "concave" );
	m_bForceMassCenter.Init( this, "forceMassCenter" );
	m_vecMassCenter.Init( this, "massCenter" );
	m_bNoSelfCollisions.Init( this, "noSelfCollisions" );
	m_bAssumeWorldSpace.Init( this, "assumeWorldSpace" );
	m_SurfaceProperty.InitAndSet( this, "surfaceProperty", "default" );
}

void CDmeCollisionModel::OnDestruction()
{
}