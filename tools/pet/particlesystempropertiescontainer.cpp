//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Dialog used to edit properties of a particle system definition
//
//===========================================================================//

#include "ParticleSystemPropertiesContainer.h"
#include "petdoc.h"
#include "pettool.h"
#include "datamodel/dmelement.h"
#include "movieobjects/dmeparticlesystemdefinition.h"
#include "dme_controls/dmecontrols_utils.h"
#include "dme_controls/particlesystempanel.h"


// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
#pragma warning (disable:4355)
CParticleSystemPropertiesContainer::CParticleSystemPropertiesContainer( CPetDoc *pDoc, vgui::Panel* pParent ) :
	BaseClass( this, pParent ), m_pDoc( pDoc )
{
}
#pragma warning (default:4355)


//-----------------------------------------------------------------------------
// Refreshes the list of raw controls
//-----------------------------------------------------------------------------
void CParticleSystemPropertiesContainer::GetKnownParticleDefinitions( CUtlVector< CDmeParticleSystemDefinition* > &definitions )
{
	definitions.RemoveAll();

	CDmrParticleSystemList particleSystemList = g_pPetTool->GetDocument()->GetParticleSystemDefinitionList();
	if ( !particleSystemList.IsValid() )
		return;

	int nCount = particleSystemList.Count();
	definitions.EnsureCapacity( nCount );
	for ( int i = 0; i < nCount; ++i )
	{
		CDmeParticleSystemDefinition *pParticleSystem = particleSystemList[i];
		definitions.AddToTail( pParticleSystem );
	}
}

//-----------------------------------------------------------------------------
// Called when the base class changes anything at all in the particle system
//-----------------------------------------------------------------------------
void CParticleSystemPropertiesContainer::OnParticleSystemModified()
{
	CAppNotifyScopeGuard sg( "CParticleSystemPropertiesContainer::OnParticleSystemModified", NOTIFY_SETDIRTYFLAG );
}


//-----------------------------------------------------------------------------
// Called when the selected particle function changes
//-----------------------------------------------------------------------------
void CParticleSystemPropertiesContainer::OnParticleFunctionSelChanged( KeyValues *pParams )
{
	if ( g_pPetTool->GetParticlePreview() )
	{
		CDmeParticleFunction *pFunction = GetElementKeyValue<CDmeParticleFunction>( pParams, "function" );
		g_pPetTool->GetParticlePreview()->SetParticleFunction( pFunction );
	}
}

