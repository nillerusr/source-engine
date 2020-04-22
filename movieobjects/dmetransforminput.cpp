//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "movieobjects/dmetransforminput.h"
#include "movieobjects_interfaces.h"
#include "datamodel/dmelementfactoryhelper.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeTranslationInput, CDmeTranslationInput );


void CDmeTranslationInput::OnConstruction()
{
	m_translation.Init( this, "translation" );
}

void CDmeTranslationInput::OnDestruction()
{
}

bool CDmeTranslationInput::IsDirty()
{
	return true;
}

void CDmeTranslationInput::Operate()
{
}

void CDmeTranslationInput::GetInputAttributes( CUtlVector< CDmAttribute * > &attrs )
{
}

void CDmeTranslationInput::GetOutputAttributes( CUtlVector< CDmAttribute * > &attrs )
{
	attrs.AddToTail( m_translation.GetAttribute() );
}


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeRotationInput, CDmeRotationInput );


void CDmeRotationInput::OnConstruction()
{
	m_orientation.Init( this, "orientation" );
	m_angles.Init( this, "angles" );
}

void CDmeRotationInput::OnDestruction()
{
}

bool CDmeRotationInput::IsDirty()
{
	return true;
}

void CDmeRotationInput::Operate()
{
}

void CDmeRotationInput::GetInputAttributes( CUtlVector< CDmAttribute * > &attrs )
{
}

void CDmeRotationInput::GetOutputAttributes( CUtlVector< CDmAttribute * > &attrs )
{
	attrs.AddToTail( m_orientation.GetAttribute() );
	attrs.AddToTail( m_angles.GetAttribute() );
}

void CDmeRotationInput::SetRotation( const Quaternion& quat )
{
	QAngle qangle;
	QuaternionAngles( quat, qangle );
	m_angles = qangle;
	m_orientation = quat;
}

void CDmeRotationInput::SetRotation( const QAngle& qangle )
{
	Quaternion quat;
	AngleQuaternion( qangle, quat );
	m_orientation = quat;
	m_angles = qangle;
}
