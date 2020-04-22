//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Dialog used to edit properties of a particle system definition
//
//===========================================================================//

#ifndef PARTICLESYSTEMPROPERTIESCONTAINER_H
#define PARTICLESYSTEMPROPERTIESCONTAINER_H
#ifdef _WIN32
#pragma once
#endif

#include "dme_controls/particlesystempropertiespanel.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CPetDoc;


//-----------------------------------------------------------------------------
// Panel used to edit a particle system	definition
//-----------------------------------------------------------------------------
class CParticleSystemPropertiesContainer : public CParticleSystemPropertiesPanel, public IParticleSystemPropertiesPanelQuery
{
	DECLARE_CLASS_SIMPLE( CParticleSystemPropertiesContainer, CParticleSystemPropertiesPanel );

public:
	CParticleSystemPropertiesContainer( CPetDoc *pDoc, vgui::Panel* pParent );   // standard constructor

	// Inherited from IParticleSystemPropertiesPanelQuery
	virtual void GetKnownParticleDefinitions( CUtlVector< CDmeParticleSystemDefinition* > &definitions );

private:
	MESSAGE_FUNC_PARAMS( OnParticleFunctionSelChanged, "ParticleFunctionSelChanged", params );

	// For inheriting classes to get notified without having to listen to messages
	virtual void OnParticleSystemModified();

	CPetDoc *m_pDoc;
};


#endif // PARTICLESYSTEMPROPERTIESCONTAINER_H
