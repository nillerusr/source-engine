//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#ifndef PARTICLESYSTEMDEFINITIONBROWSER_H
#define PARTICLESYSTEMDEFINITIONBROWSER_H
#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/editablepanel.h"
#include "tier1/utlstring.h"
#include "particles/particles.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CPetDoc;
class CDmeParticleSystemDefinition;
class CUndoScopeGuard;
namespace vgui
{
	class ComboBox;
	class Button;
	class TextEntry;
	class ListPanel;
	class CheckButton;
	class RadioButton;
}


//-----------------------------------------------------------------------------
// Panel that shows all entities in the level
//-----------------------------------------------------------------------------
class CParticleSystemDefinitionBrowser : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CParticleSystemDefinitionBrowser, vgui::EditablePanel );

public:
	CParticleSystemDefinitionBrowser( CPetDoc *pDoc, vgui::Panel* pParent, const char *pName );   // standard constructor
	virtual ~CParticleSystemDefinitionBrowser();

	// Inherited from Panel
	virtual void OnCommand( const char *pCommand );
	virtual void OnKeyCodeTyped( vgui::KeyCode code );
	MESSAGE_FUNC_CHARPTR( OnFileSelected, "FileSelected", fullpath );
	// Methods related to updating the listpanel
	void UpdateParticleSystemList();

	// Select a particular node
	void SelectParticleSystem( CDmeParticleSystemDefinition *pParticleSystem );

	// Copy, paste.
	void CopyToClipboard( );
	void PasteFromClipboard( );

private:
	// Messages handled
	MESSAGE_FUNC( OnItemDeselected, "ItemDeselected" );
	MESSAGE_FUNC( OnItemSelected, "ItemSelected" );
	MESSAGE_FUNC_PARAMS( OnInputCompleted, "InputCompleted", kv );

	void ReplaceDef_r( CUndoScopeGuard& guard, CDmeParticleSystemDefinition *pDef );

	// Gets the ith selected particle system
	CDmeParticleSystemDefinition* GetSelectedParticleSystem( int i );

	// Called when the selection changes
	void UpdateParticleSystemSelection();

	// Deletes selected particle systems
	void DeleteParticleSystems();

	// Create from KV
	void LoadKVSection( CDmeParticleSystemDefinition *pNew, KeyValues *pOverridesKv, ParticleFunctionType_t eType );
	CDmeParticleSystemDefinition* CreateParticleFromKV( KeyValues *pKeyValue );
	void CreateParticleSystemsFromKV( const char *pFilepath );

	// Shows the most recent selected object in properties window
	void OnProperties();

	CPetDoc *m_pDoc;
	vgui::ListPanel		*m_pParticleSystemsDefinitions;
};


#endif // PARTICLESYSTEMDEFINITIONBROWSER_H
