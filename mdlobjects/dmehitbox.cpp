//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Dme version of a hitbox
//
//===========================================================================//

#include "mdlobjects/dmehitbox.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "tier2/renderutils.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeHitbox, CDmeHitbox );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeHitbox::OnConstruction()
{
	m_SurfaceProperty.Init( this, "surfaceProperty" );
	m_Group.Init( this, "groupName" );
	m_Bone.Init( this, "boneName" );
	m_vecMins.Init( this, "minBounds" );
	m_vecMaxs.Init( this, "maxBounds" );
	m_RenderColor.InitAndSet( this, "renderColor", Color( 255, 255, 255, 64 ) );
}

void CDmeHitbox::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// Rendering method for the dag
//-----------------------------------------------------------------------------
void CDmeHitbox::Draw( const matrix3x4_t &shapeToWorld, CDmeDrawSettings *pDrawSettings /* = NULL */ )
{
	Vector vecOrigin;
	QAngle angles;
	MatrixAngles( shapeToWorld, angles, vecOrigin );
	RenderBox( vecOrigin, angles, m_vecMins, m_vecMaxs, m_RenderColor, true );
}

