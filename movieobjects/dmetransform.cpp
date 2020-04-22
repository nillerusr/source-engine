//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================
#include "movieobjects/dmetransform.h"
#include "tier0/dbg.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "mathlib/vector.h"
#include "mathlib/mathlib.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeTransform, CDmeTransform );


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
void CDmeTransform::OnConstruction()
{
	m_Position.Init( this, "position" );
	m_Orientation.Init( this, "orientation" );
}

void CDmeTransform::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// FIXME: Replace this with actual methods to do editing
//-----------------------------------------------------------------------------
void CDmeTransform::SetTransform( const matrix3x4_t &transform )
{
	Vector origin;
	Quaternion angles;
	MatrixAngles( transform, angles, origin );
	m_Orientation.Set( angles );
	m_Position.Set( origin );
}

void CDmeTransform::GetTransform( matrix3x4_t &transform )
{
	QuaternionMatrix( m_Orientation.Get(), m_Position.Get(), transform );
}

const Vector &CDmeTransform::GetPosition() const
{
	return m_Position.Get();
}

void CDmeTransform::SetPosition( const Vector &vecPosition )
{
	m_Position = vecPosition;
}

const Quaternion &CDmeTransform::GetOrientation() const 
{
	return m_Orientation.Get();
}

void CDmeTransform::SetOrientation( const Quaternion &orientation )
{
	m_Orientation = orientation;
}


CDmAttribute *CDmeTransform::GetPositionAttribute()
{
	return m_Position.GetAttribute();
}

CDmAttribute *CDmeTransform::GetOrientationAttribute()
{
	return m_Orientation.GetAttribute();
}
