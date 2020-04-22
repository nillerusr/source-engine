//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================
#include "movieobjects/dmeunpackoperators.h"
#include "movieobjects_interfaces.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "datamodel/dmattribute.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// CDmeUnpackColorOperator
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeUnpackColorOperator, CDmeUnpackColorOperator );

void CDmeUnpackColorOperator::OnConstruction()
{
	m_color.Init( this, "color" );
	m_red  .Init( this, "red" );
	m_green.Init( this, "green" );
	m_blue .Init( this, "blue" );
	m_alpha.Init( this, "alpha" );
}

void CDmeUnpackColorOperator::OnDestruction()
{
}

bool CDmeUnpackColorOperator::IsDirty()
{
	const Color &c = m_color.Get();
//	float s = 255.999f;
//	return c.r() != s*m_red.Get() || c.g() != s*m_green.Get() || c.b() != s*m_blue.Get() || c.a() != s*m_alpha.Get();
	return c.r() != m_red.Get() || c.g() != m_green.Get() || c.b() != m_blue.Get() || c.a() != m_alpha.Get();
}

void CDmeUnpackColorOperator::Operate()
{
//	float s = 255.999f;
//	m_color.Set( Color( s*m_red.Get(), s*m_green.Get(), s*m_blue.Get(), s*m_alpha.Get() ) );
	m_red  .Set( m_color.Get().r() );
	m_green.Set( m_color.Get().g() );
	m_blue .Set( m_color.Get().b() );
	m_alpha.Set( m_color.Get().a() );
}

void CDmeUnpackColorOperator::GetInputAttributes( CUtlVector< CDmAttribute * > &attrs )
{
	attrs.AddToTail( m_color.GetAttribute() );
}

void CDmeUnpackColorOperator::GetOutputAttributes( CUtlVector< CDmAttribute * > &attrs )
{
	attrs.AddToTail( m_red.GetAttribute() );
	attrs.AddToTail( m_green.GetAttribute() );
	attrs.AddToTail( m_blue.GetAttribute() );
	attrs.AddToTail( m_alpha.GetAttribute() );
}


//-----------------------------------------------------------------------------
// CDmeUnpackVector2Operator 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeUnpackVector2Operator, CDmeUnpackVector2Operator );

void CDmeUnpackVector2Operator::OnConstruction()
{
	m_vector.Init( this, "vector" );
	m_x.Init( this, "x" );
	m_y.Init( this, "y" );
}

void CDmeUnpackVector2Operator::OnDestruction()
{
}

bool CDmeUnpackVector2Operator::IsDirty()
{
	const Vector2D &v = m_vector.Get();
	return v.x != m_x.Get() || v.y != m_y.Get();
}

void CDmeUnpackVector2Operator::Operate()
{
	m_x.Set( m_vector.Get().x );
	m_y.Set( m_vector.Get().y );
}

void CDmeUnpackVector2Operator::GetInputAttributes( CUtlVector< CDmAttribute * > &attrs )
{
	attrs.AddToTail( m_vector.GetAttribute() );
}

void CDmeUnpackVector2Operator::GetOutputAttributes( CUtlVector< CDmAttribute * > &attrs )
{
	attrs.AddToTail( m_x.GetAttribute() );
	attrs.AddToTail( m_y.GetAttribute() );
}


//-----------------------------------------------------------------------------
// CDmeUnpackVector3Operator 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeUnpackVector3Operator, CDmeUnpackVector3Operator );

void CDmeUnpackVector3Operator::OnConstruction()
{
	m_vector.Init( this, "vector" );
	m_x.Init( this, "x" );
	m_y.Init( this, "y" );
	m_z.Init( this, "z" );
}

void CDmeUnpackVector3Operator::OnDestruction()
{
}

bool CDmeUnpackVector3Operator::IsDirty()
{
	const Vector &v = m_vector.Get();
	return v.x != m_x.Get() || v.y != m_y.Get() || v.z != m_z.Get();
}

void CDmeUnpackVector3Operator::Operate()
{
	m_x.Set( m_vector.Get().x );
	m_y.Set( m_vector.Get().y );
	m_z.Set( m_vector.Get().z );
}

void CDmeUnpackVector3Operator::GetInputAttributes( CUtlVector< CDmAttribute * > &attrs )
{
	attrs.AddToTail( m_vector.GetAttribute() );
}

void CDmeUnpackVector3Operator::GetOutputAttributes( CUtlVector< CDmAttribute * > &attrs )
{
	attrs.AddToTail( m_x.GetAttribute() );
	attrs.AddToTail( m_y.GetAttribute() );
	attrs.AddToTail( m_z.GetAttribute() );
}


//-----------------------------------------------------------------------------
// CDmeUnpackVector4Operator 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeUnpackVector4Operator, CDmeUnpackVector4Operator );

void CDmeUnpackVector4Operator::OnConstruction()
{
	m_vector.Init( this, "vector" );
	m_x.Init( this, "x" );
	m_y.Init( this, "y" );
	m_z.Init( this, "z" );
	m_w.Init( this, "w" );
}

void CDmeUnpackVector4Operator::OnDestruction()
{
}

bool CDmeUnpackVector4Operator::IsDirty()
{
	const Vector4D &v = m_vector.Get();
	return v.x != m_x.Get() || v.y != m_y.Get() || v.z != m_z.Get() || v.w != m_w.Get();
}

void CDmeUnpackVector4Operator::Operate()
{
	m_x.Set( m_vector.Get().x );
	m_y.Set( m_vector.Get().y );
	m_z.Set( m_vector.Get().z );
	m_w.Set( m_vector.Get().w );
}

void CDmeUnpackVector4Operator::GetInputAttributes( CUtlVector< CDmAttribute * > &attrs )
{
	attrs.AddToTail( m_vector.GetAttribute() );
}

void CDmeUnpackVector4Operator::GetOutputAttributes( CUtlVector< CDmAttribute * > &attrs )
{
	attrs.AddToTail( m_x.GetAttribute() );
	attrs.AddToTail( m_y.GetAttribute() );
	attrs.AddToTail( m_z.GetAttribute() );
	attrs.AddToTail( m_w.GetAttribute() );
}


//-----------------------------------------------------------------------------
// CDmeUnpackQAngleOperator 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeUnpackQAngleOperator, CDmeUnpackQAngleOperator );

void CDmeUnpackQAngleOperator::OnConstruction()
{
	m_qangle.Init( this, "qangle" );
	m_x.Init( this, "x" );
	m_y.Init( this, "y" );
	m_z.Init( this, "z" );
}

void CDmeUnpackQAngleOperator::OnDestruction()
{
}

bool CDmeUnpackQAngleOperator::IsDirty()
{
	const QAngle &q = m_qangle.Get();
	return q.x != m_x.Get() || q.y != m_y.Get() || q.z != m_z.Get();
}

void CDmeUnpackQAngleOperator::Operate()
{
	m_x.Set( m_qangle.Get().x );
	m_y.Set( m_qangle.Get().y );
	m_z.Set( m_qangle.Get().z );
}

void CDmeUnpackQAngleOperator::GetInputAttributes( CUtlVector< CDmAttribute * > &attrs )
{
	attrs.AddToTail( m_qangle.GetAttribute() );
}

void CDmeUnpackQAngleOperator::GetOutputAttributes( CUtlVector< CDmAttribute * > &attrs )
{
	attrs.AddToTail( m_x.GetAttribute() );
	attrs.AddToTail( m_y.GetAttribute() );
	attrs.AddToTail( m_z.GetAttribute() );
}


//-----------------------------------------------------------------------------
// CDmeUnpackQuaternionOperator 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeUnpackQuaternionOperator, CDmeUnpackQuaternionOperator );

void CDmeUnpackQuaternionOperator::OnConstruction()
{
	m_quaternion.Init( this, "quaternion" );
	m_x.Init( this, "x" );
	m_y.Init( this, "y" );
	m_z.Init( this, "z" );
	m_w.Init( this, "w" );
}

void CDmeUnpackQuaternionOperator::OnDestruction()
{
}

bool CDmeUnpackQuaternionOperator::IsDirty()
{
	const Quaternion &q = m_quaternion.Get();
	return q.x != m_x.Get() || q.y != m_y.Get() || q.z != m_z.Get() || q.w != m_w.Get();
}

void CDmeUnpackQuaternionOperator::Operate()
{
	m_x.Set( m_quaternion.Get().x );
	m_y.Set( m_quaternion.Get().y );
	m_z.Set( m_quaternion.Get().z );
	m_w.Set( m_quaternion.Get().w );
}

void CDmeUnpackQuaternionOperator::GetInputAttributes( CUtlVector< CDmAttribute * > &attrs )
{
	attrs.AddToTail( m_quaternion.GetAttribute() );
}

void CDmeUnpackQuaternionOperator::GetOutputAttributes( CUtlVector< CDmAttribute * > &attrs )
{
	attrs.AddToTail( m_x.GetAttribute() );
	attrs.AddToTail( m_y.GetAttribute() );
	attrs.AddToTail( m_z.GetAttribute() );
	attrs.AddToTail( m_w.GetAttribute() );
}


//-----------------------------------------------------------------------------
// CDmeUnpackVMatrixOperator 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeUnpackVMatrixOperator, CDmeUnpackVMatrixOperator );

void CDmeUnpackVMatrixOperator::OnConstruction()
{
	m_vmatrix.Init( this, "vmatrix" );
	char name[ 4 ];
	for ( uint i = 0; i < 16; ++i )
	{
		Q_snprintf( name, sizeof(name), "m%d%d", i >> 2, i & 0x3 );
		m_cells[ i ].Init( this, name );
	}
}

void CDmeUnpackVMatrixOperator::OnDestruction()
{
}

bool CDmeUnpackVMatrixOperator::IsDirty()
{
	const VMatrix &v = m_vmatrix.Get();
	for ( uint i = 0; i < 16; ++i )
	{
		if ( *( v[ i ] ) != m_cells[ i ].Get() )
			return true;
	}
	return false;
}

void CDmeUnpackVMatrixOperator::Operate()
{
	VMatrix v;
	for ( uint i = 0; i < 16; ++i )
	{
		m_cells[ i ].Set( *( v[ i ] ) );
	}
	m_vmatrix.Set( v );
}

void CDmeUnpackVMatrixOperator::GetInputAttributes( CUtlVector< CDmAttribute * > &attrs )
{
	attrs.AddToTail( m_vmatrix.GetAttribute() );
}

void CDmeUnpackVMatrixOperator::GetOutputAttributes( CUtlVector< CDmAttribute * > &attrs )
{
	for ( uint i = 0; i < 16; ++i )
	{
		attrs.AddToTail( m_cells[i].GetAttribute() );
	}
}
