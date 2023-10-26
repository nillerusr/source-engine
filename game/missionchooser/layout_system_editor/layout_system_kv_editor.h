//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
// Layout system editor.  Auto-generates UI from KeyValues data
// to edit rule instances.
//
//===============================================================================

#ifndef LAYOUT_SYSTEM_KV_EDITOR_H
#define LAYOUT_SYSTEM_KV_EDITOR_H

#if defined( COMPILER_MSVC )
#pragma once
#endif

#include "vgui_controls/EditablePanel.h"

class CScrollingWindow;
class CMissionPanel;
class CTilegenMissionPreprocessor;

class CLayoutSystemKVEditor : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CLayoutSystemKVEditor, vgui::EditablePanel );

public:
	CLayoutSystemKVEditor( Panel *pParent, const char *pName );
	virtual ~CLayoutSystemKVEditor();
	
	virtual void PerformLayout();

	void SetPreprocessor( const CTilegenMissionPreprocessor *pPreprocessor ) { m_pPreprocessor = pPreprocessor; }
	const CTilegenMissionPreprocessor *GetPreprocessor() { return m_pPreprocessor; }

	// This operation is a destructive operation (takes ownership of sub-keys) but caller
	// is responsible for cleaning up the container KV file.
	void SetMissionData( KeyValues *pMissionFileKV );
	KeyValues *GetMissionData() { return m_pMissionFileKV; }

	void RecreateMissionPanel();

	void ShowOptionalValues( bool bVisible );
	bool ShouldShowOptionalValues() const { return m_bShowOptionalValues; }

	void ShowAddButtons( bool bVisible );
	bool ShouldShowAddButtons() const { return m_bShowAddButtons; }

private:
	// The preprocessor and key-values data are not owned by this class
	const CTilegenMissionPreprocessor *m_pPreprocessor;
	KeyValues *m_pMissionFileKV;

	CScrollingWindow *m_pScrollingWindow;
	CMissionPanel *m_pMissionPanel;

	bool m_bShowOptionalValues;
	bool m_bShowAddButtons;
};

#endif // LAYOUT_SYSTEM_KV_EDITOR_H