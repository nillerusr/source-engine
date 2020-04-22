//========= Copyright Valve Corporation, All rights reserved. ============//
//
// The transform operator class - shortcut to setting transform values from floats
//
//=============================================================================
#include "movieobjects/dmetransformoperator.h"
#include "movieobjects/dmetransform.h"
#include "movieobjects_interfaces.h"
#include "datamodel/dmelementfactoryhelper.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeTransformOperator, CDmeTransformOperator );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeTransformOperator::OnConstruction()
{
	m_transform.Init( this, "transform" );
	m_positionX.Init( this, "positionX" );
	m_positionY.Init( this, "positionY" );
	m_positionZ.Init( this, "positionZ" );
	m_orientationX.Init( this, "orientationX" );
	m_orientationY.Init( this, "orientationY" );
	m_orientationZ.Init( this, "orientationZ" );
	m_orientationW.Init( this, "orientationW" );
}

void CDmeTransformOperator::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeTransformOperator::Operate()
{
	CDmeTransform *pTransform = m_transform.GetElement();
	if ( pTransform == NULL )
		return;

	Vector     position    = pTransform->GetValue< Vector     >( "position" );
	Quaternion orientation = pTransform->GetValue< Quaternion >( "orientation" );

	position.x = m_positionX.Get();
	position.y = m_positionY.Get();
	position.z = m_positionZ.Get();

	orientation.x = m_orientationX.Get();
	orientation.y = m_orientationY.Get();
	orientation.z = m_orientationZ.Get();
	orientation.w = m_orientationW.Get();

	pTransform->SetValue( "position", position );
	pTransform->SetValue( "orientation", orientation );
}

// hack to avoid MSVC complaining about multiply defined symbols
namespace TransformOp
{
void AddAttr( CUtlVector< CDmAttribute * > &attrs, CDmAttribute *pAttr )
{
	if ( pAttr == NULL )
		return;
	attrs.AddToTail( pAttr );
}
};
using namespace TransformOp;

void CDmeTransformOperator::GetInputAttributes( CUtlVector< CDmAttribute * > &attrs )
{
	AddAttr( attrs, m_positionX.GetAttribute() );
	AddAttr( attrs, m_positionY.GetAttribute() );
	AddAttr( attrs, m_positionZ.GetAttribute() );
	AddAttr( attrs, m_orientationX.GetAttribute() );
	AddAttr( attrs, m_orientationY.GetAttribute() );
	AddAttr( attrs, m_orientationZ.GetAttribute() );
	AddAttr( attrs, m_orientationW.GetAttribute() );
}

void CDmeTransformOperator::GetOutputAttributes( CUtlVector< CDmAttribute * > &attrs )
{
	CDmeTransform *pTransform = m_transform.GetElement();
	if ( pTransform == NULL )
		return;

	AddAttr( attrs, pTransform->GetAttribute( "position" ) );
	AddAttr( attrs, pTransform->GetAttribute( "orientation" ) );
}

void CDmeTransformOperator::SetTransform( CDmeTransform *pTransform )
{
	m_transform.Set( pTransform );
}

const CDmeTransform *CDmeTransformOperator::GetTransform() const
{
	return m_transform.GetElement();
}
