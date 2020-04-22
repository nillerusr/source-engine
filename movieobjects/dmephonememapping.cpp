//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================
#include "movieobjects/dmephonememapping.h"
#include "datamodel/dmelementfactoryhelper.h"

//-----------------------------------------------------------------------------
// CDmePhonemeMapping
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmePhonemeMapping, CDmePhonemeMapping );

void CDmePhonemeMapping::OnConstruction()
{
	m_Preset.Init( this, "preset" );
	m_Weight.InitAndSet( this, "weight", 1.0f );
}

void CDmePhonemeMapping::OnDestruction()
{
}