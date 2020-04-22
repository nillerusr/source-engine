//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Dialog used to edit properties of a particle system definition
//
//===========================================================================//

#ifndef PRESETPICKER_H
#define PRESETPICKER_H

#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/Frame.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CDmElement;

using namespace vgui;


//-----------------------------------------------------------------------------
//
// Purpose: Picker for animation set presets
//
//-----------------------------------------------------------------------------
class CPresetPickerFrame : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CPresetPickerFrame, vgui::Frame );

public:
	CPresetPickerFrame( vgui::Panel *pParent, const char *pTitle, bool bAllowMultiSelect = true );
	~CPresetPickerFrame();

	// Shows the modal dialog
	void DoModal( CDmElement *pPresetGroup, bool bSelectAll, KeyValues *pContextKeyValues );

	// Inherited from Frame
	virtual void OnCommand( const char *pCommand );

private:
	// Refreshes the list of presets
	void RefreshPresetList( CDmElement *pPresetGroup, bool bSelectAll );
	void CleanUpMessage();

	vgui::ListPanel *m_pPresetList;
	vgui::Button *m_pOpenButton;
	vgui::Button *m_pCancelButton;
	KeyValues *m_pContextKeyValues;
};

#endif // PRESETPICKER_H
