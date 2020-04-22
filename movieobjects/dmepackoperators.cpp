//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================
#include "movieobjects/dmepackoperators.h"
#include "movieobjects_interfaces.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "datamodel/dmattribute.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// CDmePackColorOperator
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmePackColorOperator, CDmePackColorOperator );

void CDmePackColorOperator::OnConstruction()
{
	m_color.Init( this, "color" );
	m_red  .Init( this, "red" );
	m_green.Init( this, "green" );
	m_blue .Init( this, "blue" );
	m_alpha.Init( this, "alpha" );
}

void CDmePackColorOperator::OnDestruction()
{
}

bool CDmePackColorOperator::IsDirty()
{
	const Color &c = m_color.Get();
//	float s = 255.999f;
//	return c.r() != s*m_red.Get() || c.g() != s*m_green.Get() || c.b() != s*m_blue.Get() || c.a() != s*m_alpha.Get();
	return c.r() != m_red.Get() || c.g() != m_green.Get() || c.b() != m_blue.Get() || c.a() != m_alpha.Get();
}

void CDmePackColorOperator::Operate()
{
//	float s = 255.999f;
//	m_color.Set( Color( s*m_red.Get(), s*m_green.Get(), s*m_blue.Get(), s*m_alpha.Get() ) );
	m_color.Set( Color( m_red.Get(), m_green.Get(), m_blue.Get(), m_alpha.Get() ) );
}

void CDmePackColorOperator::GetInputAttributes( CUtlVector< CDmAttribute * > &attrs )
{
	attrs.AddToTail( m_red.GetAttribute() );
	attrs.AddToTail( m_green.GetAttribute() );
	attrs.AddToTail( m_blue.GetAttribute() );
	attrs.AddToTail( m_alpha.GetAttribute() );
}

void CDmePackColorOperator::GetOutputAttributes( CUtlVector< CDmAttribute * > &attrs )
{
	attrs.AddToTail( m_color.GetAttribute() );
}


//-----------------------------------------------------------------------------
// CDmePackVector2Operator 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmePackVector2Operator, CDmePackVector2Operator );

void CDmePackVector2Operator::OnConstruction()
{
	m_vector.Init( this, "vector" );
	m_x.Init( this, "x" );
	m_y.Init( this, "y" );
}

void CDmePackVector2Operator::OnDestruction()
{
}

bool CDmePackVector2Operator::IsDirty()
{
	const Vector2D &v = m_vector.Get();
	return v.x != m_x.Get() || v.y != m_y.Get();
}

void CDmePackVector2Operator::Operate()
{
	m_vector.Set( Vector2D( m_x.Get(), m_y.Get() ) );
}

void CDmePackVector2Operator::GetInputAttributes( CUtlVector< CDmAttribute * > &attrs )
{
	attrs.AddToTail( m_x.GetAttribute() );
	attrs.AddToTail( m_y.GetAttribute() );
}

void CDmePackVector2Operator::GetOutputAttributes( CUtlVector< CDmAttribute * > &attrs )
{
	attrs.AddToTail( m_vector.GetAttribute() );
}


//-----------------------------------------------------------------------------
// CDmePackVector3Operator 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmePackVector3Operator, CDmePackVector3Operator );

void CDmePackVector3Operator::OnConstruction()
{
	m_vector.Init( this, "vector" );
	m_x.Init( this, "x" );
	m_y.Init( this, "y" );
	m_z.Init( this, "z" );
}

void CDmePackVector3Operator::OnDestruction()
{
}

bool CDmePackVector3Operator::IsDirty()
{
	const Vector &v = m_vector.Get();
	return v.x != m_x.Get() || v.y != m_y.Get() || v.z != m_z.Get();
}

void CDmePackVector3Operator::Operate()
{
	m_vector.Set( Vector( m_x.Get(), m_y.Get(), m_z.Get() ) );
}

void CDmePackVector3Operator::GetInputAttributes( CUtlVector< CDmAttribute * > &attrs )
{
	attrs.AddToTail( m_x.GetAttribute() );
	attrs.AddToTail( m_y.GetAttribute() );
	attrs.AddToTail( m_z.GetAttribute() );
}

void CDmePackVector3Operator::GetOutputAttributes( CUtlVector< CDmAttribute * > &attrs )
{
	attrs.AddToTail( m_vector.GetAttribute() );
}


//-----------------------------------------------------------------------------
// CDmePackVector4Operator 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmePackVector4Operator, CDmePackVector4Operator );

void CDmePackVector4Operator::OnConstruction()
{
	m_vector.Init( this, "vector" );
	m_x.Init( this, "x" );
	m_y.Init( this, "y" );
	m_z.Init( this, "z" );
	m_w.Init( this, "w" );
}

void CDmePackVector4Operator::OnDestruction()
{
}

bool CDmePackVector4Operator::IsDirty()
{
	const Vector4D &v = m_vector.Get();
	return v.x != m_x.Get() || v.y != m_y.Get() || v.z != m_z.Get() || v.w != m_w.Get();
}

void CDmePackVector4Operator::Operate()
{
	m_vector.Set( Vector4D( m_x.Get(), m_y.Get(), m_z.Get(), m_w.Get() ) );
}

void CDmePackVector4Operator::GetInputAttributes( CUtlVector< CDmAttribute * > &attrs )
{
	attrs.AddToTail( m_x.GetAttribute() );
	attrs.AddToTail( m_y.GetAttribute() );
	attrs.AddToTail( m_z.GetAttribute() );
	attrs.AddToTail( m_w.GetAttribute() );
}

void CDmePackVector4Operator::GetOutputAttributes( CUtlVector< CDmAttribute * > &attrs )
{
	attrs.AddToTail( m_vector.GetAttribute() );
}


//-----------------------------------------------------------------------------
// CDmePackQAngleOperator 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmePackQAngleOperator, CDmePackQAngleOperator );

void CDmePackQAngleOperator::OnConstruction()
{
	m_qangle.Init( this, "qangle" );
	m_x.Init( this, "x" );
	m_y.Init( this, "y" );
	m_z.Init( this, "z" );
}

void CDmePackQAngleOperator::OnDestruction()
{
}

bool CDmePackQAngleOperator::IsDirty()
{
	const QAngle &q = m_qangle.Get();
	return q.x != m_x.Get() || q.y != m_y.Get() || q.z != m_z.Get();
}

void CDmePackQAngleOperator::Operate()
{
	m_qangle.Set( QAngle( m_x.Get(), m_y.Get(), m_z.Get() ) );
}

void CDmePackQAngleOperator::GetInputAttributes( CUtlVector< CDmAttribute * > &attrs )
{
	attrs.AddToTail( m_x.GetAttribute() );
	attrs.AddToTail( m_y.GetAttribute() );
	attrs.AddToTail( m_z.GetAttribute() );
}

void CDmePackQAngleOperator::GetOutputAttributes( CUtlVector< CDmAttribute * > &attrs )
{
	attrs.AddToTail( m_qangle.GetAttribute() );
}


//-----------------------------------------------------------------------------
// CDmePackQuaternionOperator 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmePackQuaternionOperator, CDmePackQuaternionOperator );

void CDmePackQuaternionOperator::OnConstruction()
{
	m_quaternion.Init( this, "quaternion" );
	m_x.Init( this, "x" );
	m_y.Init( this, "y" );
	m_z.Init( this, "z" );
	m_w.Init( this, "w" );
}

void CDmePackQuaternionOperator::OnDestruction()
{
}

bool CDmePackQuaternionOperator::IsDirty()
{
	const Quaternion &q = m_quaternion.Get();
	return q.x != m_x.Get() || q.y != m_y.Get() || q.z != m_z.Get() || q.w != m_w.Get();
}

void CDmePackQuaternionOperator::Operate()
{
	m_quaternion.Set( Quaternion( m_x.Get(), m_y.Get(), m_z.Get(), m_w.Get() ) );
}

void CDmePackQuaternionOperator::GetInputAttributes( CUtlVector< CDmAttribute * > &attrs )
{
	attrs.AddToTail( m_x.GetAttribute() );
	attrs.AddToTail( m_y.GetAttribute() );
	attrs.AddToTail( m_z.GetAttribute() );
	attrs.AddToTail( m_w.GetAttribute() );
}

void CDmePackQuaternionOperator::GetOutputAttributes( CUtlVector< CDmAttribute * > &attrs )
{
	attrs.AddToTail( m_quaternion.GetAttribute() );
}


//-----------------------------------------------------------------------------
// CDmePackVMatrixOperator 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmePackVMatrixOperator, CDmePackVMatrixOperator );

void CDmePackVMatrixOperator::OnConstruction()
{
	m_vmatrix.Init( this, "vmatrix" );
	char name[ 4 ];
	for ( uint i = 0; i < 16; ++i )
	{
		Q_snprintf( name, sizeof(name), "m%d%d", i >> 2, i & 0x3 );
		m_cells[ i ].Init( this, name );
	}
}

void CDmePackVMatrixOperator::OnDestruction()
{
}

bool CDmePackVMatrixOperator::IsDirty()
{
	const VMatrix &v = m_vmatrix.Get();
	for ( uint i = 0; i < 16; ++i )
	{
		if ( *( v[ i ] ) != m_cells[ i ].Get() )
			return true;
	}
	return false;
}

void CDmePackVMatrixOperator::Operate()
{
	VMatrix v;
	for ( uint i = 0; i < 16; ++i )
	{
		*( v[ i ] ) = m_cells[ i ].Get();
	}
	m_vmatrix.Set( v );
}

void CDmePackVMatrixOperator::GetInputAttributes( CUtlVector< CDmAttribute * > &attrs )
{
	for ( uint i = 0; i < 16; ++i )
	{
		attrs.AddToTail( m_cells[i].GetAttribute() );
	}
}

void CDmePackVMatrixOperator::GetOutputAttributes( CUtlVector< CDmAttribute * > &attrs )
{
	attrs.AddToTail( m_vmatrix.GetAttribute() );
}
