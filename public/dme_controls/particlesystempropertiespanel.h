//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Dialog used to edit properties of a particle system definition
//
//===========================================================================//

#ifndef PARTICLESYSTEMPROPERTIESPANEL_H
#define PARTICLESYSTEMPROPERTIESPANEL_H
#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/EditablePanel.h"
#include "tier1/utlstring.h"
#include "datamodel/dmehandle.h"
#include "particles/particles.h"

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CDmeParticleSystemDefinition;
class CParticleFunctionBrowser;
class CDmeElementPanel;

namespace vgui
{
	class Splitter;
	class ComboBox;
}


//-----------------------------------------------------------------------------
// Used by the panel to discover the list of known particle system definitions
//-----------------------------------------------------------------------------
class IParticleSystemPropertiesPanelQuery
{
public:
	virtual void GetKnownParticleDefinitions( CUtlVector< CDmeParticleSystemDefinition* > &definitions ) = 0;
};


//-----------------------------------------------------------------------------
// Panel used to edit a particle system	definition
//-----------------------------------------------------------------------------
class CParticleSystemPropertiesPanel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CParticleSystemPropertiesPanel, vgui::EditablePanel );

	// Sends the message 'ParticleSystemModified' when the particle system was modified in any way
	// Sends the message 'ParticleFunctionSelChanged' when the selected particle function changed
	//	-- stores the selected CDmeParticleFunction in a subkey called 'function' 

public:
	CParticleSystemPropertiesPanel( IParticleSystemPropertiesPanelQuery *pQuery, vgui::Panel* pParent );   // standard constructor

	// Sets the particle system to look at
	void SetParticleSystem( CDmeParticleSystemDefinition *pParticleSystem );

	// Refreshes display
	void Refresh( bool bValuesOnly = true );

private:
	// For inheriting classes to get notified without having to listen to messages
	virtual void OnParticleSystemModified() {}

	MESSAGE_FUNC( OnTextChanged, "TextChanged" );	
	MESSAGE_FUNC_PARAMS( OnDmeElementChanged, "DmeElementChanged", params );
	MESSAGE_FUNC( OnParticleSystemModifiedInternal, "ParticleSystemModified" );
	MESSAGE_FUNC_PARAMS( OnParticleFunctionSelChanged, "ParticleFunctionSelChanged", params );

	IParticleSystemPropertiesPanelQuery *m_pQuery;

	vgui::Splitter *m_pSplitter;
	vgui::ComboBox *m_pFunctionTypeCombo;
	vgui::EditablePanel *m_pFunctionBrowserArea;
	CParticleFunctionBrowser *m_pParticleFunctionBrowser[PARTICLE_FUNCTION_COUNT];

	CDmeElementPanel *m_pParticleFunctionProperties;

	CDmeHandle< CDmeParticleSystemDefinition > m_hParticleSystem;
};


#endif // PARTICLESYSTEMPROPERTIESPANEL_H
