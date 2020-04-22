//========= Copyright Valve Corporation, All rights reserved. ============//
//
// The expression operator class - scalar math calculator
// for a good list of operators and simple functions, see:
//   \\fileserver\user\MarcS\boxweb\aliveDistLite\v4.2.0\doc\alive\functions.txt
// (although we'll want to implement elerp as the standard 3x^2 - 2x^3 with rescale)
//
//=============================================================================
#include "movieobjects/dmebalancetostereocalculatoroperator.h"
#include "movieobjects/dmechannel.h"
#include "movieobjects_interfaces.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "datamodel/dmattribute.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeBalanceToStereoCalculatorOperator, CDmeBalanceToStereoCalculatorOperator );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeBalanceToStereoCalculatorOperator::OnConstruction()
{
	m_result_left.Init( this, "result_left" );
	m_result_right.Init( this, "result_right" );
	m_result_multi.Init( this, "result_multi" );

	m_value.Init( this, "value" );
	m_balance.InitAndSet( this, "balance", 0.5f );
	m_multilevel.InitAndSet( this, "multilevel", 0.5f );

	m_bSpewResult.Init( this, "spewresult" );

	m_flDefaultValue = FLT_MAX;
}

void CDmeBalanceToStereoCalculatorOperator::OnDestruction()
{
}

void CDmeBalanceToStereoCalculatorOperator::GetInputAttributes ( CUtlVector< CDmAttribute * > &attrs )
{
	attrs.AddToTail( m_value.GetAttribute() );
	attrs.AddToTail( m_balance.GetAttribute() );
	attrs.AddToTail( m_multilevel.GetAttribute() );
}

void CDmeBalanceToStereoCalculatorOperator::GetOutputAttributes( CUtlVector< CDmAttribute * > &attrs )
{
	attrs.AddToTail( m_result_left.GetAttribute() );
	attrs.AddToTail( m_result_right.GetAttribute() );
	attrs.AddToTail( m_result_multi.GetAttribute() );
}

float CDmeBalanceToStereoCalculatorOperator::ComputeDefaultValue()
{
	// NOTE: This is a total hack, which we expect to remove soon, when the balance work is done.
	const static UtlSymId_t symToElement = g_pDataModel->GetSymbol( "toElement" );
	CDmeChannel *pChannel = FindReferringElement<CDmeChannel>( this, symToElement );
	if ( !pChannel )
		return 0.0f;

	CDmElement *pControl = pChannel->GetFromElement();
	if ( !pControl )
		return 0.0f;

	return pControl->GetValue<float>( "defaultValue" );
}

void CDmeBalanceToStereoCalculatorOperator::Operate()
{ 
	if ( m_flDefaultValue == FLT_MAX )
	{
		m_flDefaultValue = ComputeDefaultValue();
	}

	float flValue = m_value - m_flDefaultValue;
	if ( m_balance > 0.5f )
	{
		m_result_right = m_value;
		m_result_left = ( ( 1.0f - m_balance ) / 0.5f ) * flValue + m_flDefaultValue;
	}
	else
	{
		m_result_right = ( m_balance / 0.5f ) * flValue + m_flDefaultValue;
		m_result_left = m_value;
	}

	m_result_multi = m_multilevel;

	if ( m_bSpewResult )
	{
		Msg( "%s = l('%f') r('%f') m('%f')\n", GetName(), (float)m_result_left, (float)m_result_right, (float)m_result_multi );
	}
}

void CDmeBalanceToStereoCalculatorOperator::SetSpewResult( bool state )
{
	m_bSpewResult = state;
}
