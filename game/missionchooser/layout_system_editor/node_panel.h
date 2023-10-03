//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
// Base class for panels used to edit nodes in layout system definitions.
//
//===============================================================================

#ifndef NODE_PANEL_H
#define NODE_PANEL_H

#if defined( COMPILER_MSVC )
#pragma once
#endif

#include "vgui_controls/EditablePanel.h"
#include "utlvector.h"

class CLayoutSystemKVEditor;
class KeyValues;

namespace vgui
{
	class Panel;
	class CBoxSizer;
	class Label;
	class Button;
	class IScheme;
}

//-----------------------------------------------------------------------------
// Base class for all VGUI panels used by the Layout System Editor.
//-----------------------------------------------------------------------------
class CNodePanel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CNodePanel, vgui::EditablePanel );

public:
	CNodePanel( Panel *pParent, const char *pName );

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void OnCommand( const char *pCommand );

	MESSAGE_FUNC_PARAMS( OnAddRule, "AddRule", pKV );
	MESSAGE_FUNC_PARAMS( OnAddState, "AddState", pKV );

	MESSAGE_FUNC_PTR( OnTextChanged, "TextChanged", panel );

	//-----------------------------------------------------------------------------
	// Sets the data bound to this panel and recursively [re-]creates all
	// sub-controls & panels.
	//-----------------------------------------------------------------------------
	void SetData( KeyValues *pNodeKV );
	KeyValues *GetData() { return m_pNodeKV; }

	//-----------------------------------------------------------------------------
	// Recursively [re-]builds all panels & controls.
	//-----------------------------------------------------------------------------
	void RecreateControls();

	//-----------------------------------------------------------------------------
	// Refreshes the state of this panel's controls and all child panel's
	// without deleting/recreating controls.
	// NOTE: Derived classes should call BaseClass::UpdateState();
	//-----------------------------------------------------------------------------
	virtual void UpdateState();

	//-----------------------------------------------------------------------------
	// Stores/gets a pointer to the owning KV editor panel.
	//-----------------------------------------------------------------------------
	void SetEditor( CLayoutSystemKVEditor *pEditor ) { m_pEditor = pEditor; }
	CLayoutSystemKVEditor *GetEditor() { return m_pEditor; }

	//-----------------------------------------------------------------------------
	// Invalidates the layout of the parent editor control, which recursively
	// forces a re-layout of all child panels.
	//-----------------------------------------------------------------------------
	void InvalidateEditorLayout();

	void EnableDeleteButton( bool bEnabled ) { m_bShowDeleteButton = bEnabled; UpdateState(); }
	void EnableNameEditBox( bool bEnabled ) { m_bShowNameEditBox = bEnabled; UpdateState(); }

protected:
	//-----------------------------------------------------------------------------
	// Overridden in derived classes to create UI controls & all child panels
	// based on the key values data bound to the panel.
	//-----------------------------------------------------------------------------
	virtual void CreatePanelContents() = 0;

	void ClearHeading();
	void AddHeadingElement( vgui::Panel *pPanel, float flExpandFactor = 1.0f, int nPadding = 0 );
	void AddHeadingSpacer();
	void SetChildIndent( int nIndent );
	
	void ClearChildren();
	void AddChild( CNodePanel *pPanel, int nPadding = 0 );
	void AddChild( vgui::Panel *pPanel, int nPadding = 0 );
	CNodePanel *CreateChild( KeyValues *pKey );

	//-----------------------------------------------------------------------------
	// Inserts a child key values at the specified index and re-creates all
	// sub-controls & panels.
	//-----------------------------------------------------------------------------
	void InsertChildData( int nIndex, KeyValues *pKeyValues );

	//-----------------------------------------------------------------------------
	// Detaches this node's data from the parent tree and re-builds the parent
	// tree so that this panel will be deleted.
	// NOTE: do not access 'this' after calling this function
	//-----------------------------------------------------------------------------
	void DeleteSelf();

	//-----------------------------------------------------------------------------
	// Adds a panel which allows for the creation of new sub-nodes.
	// 'nIndex' is the sub-index where an element created from this panel should 
	// be inserted.
	//-----------------------------------------------------------------------------
	void AddNewElementPanel( int nIndex, bool bAllowRules, bool bAllowStates );

	virtual void SetNodeName( const char *pNodeName );

	static const int m_nNameLength = 100;
	
	// Text to display in the label. If m_NodeName is set to the empty string (""), the label will not be drawn.
	char m_NodeLabel[m_nNameLength];
	// Text to display in the editable text entry. The text entry is only displayed if m_bShowNameEditBox is true.
	char m_NodeName[m_nNameLength];

private:
	// Data may be modified by this class, but
	// parent is responsible for deletion.
	KeyValues *m_pNodeKV;

	// The owning editor object
	CLayoutSystemKVEditor *m_pEditor;

	// Do not need to delete any of these sizers; they are automatically cleaned up.
	vgui::CBoxSizer *m_pRootSizer;
	vgui::CBoxSizer *m_pLabelSizer;
	// Derived classes can add stuff to the header (appearing after the label)
	vgui::CBoxSizer *m_pHeadingSizer;
	// A horizontal sizer whose sole purpose is to indent child objects
	vgui::CBoxSizer *m_pChildIndentSizer;
	// Derived class can add stuff below the header here
	vgui::CBoxSizer *m_pChildSizer;

	// Label for the node.
	vgui::Label *m_pLabel;
	// Optional editable box, just to the right of the node's label.
	vgui::TextEntry *m_pNameEditBox;
	bool m_bShowNameEditBox;

	// Some nodes can be deleted from their parent container.
	// This feature can turned on/off by a call to EnableDeleteButton().
	// By default, this is not enabled.
	bool m_bShowDeleteButton;
	vgui::Button *m_pDeleteSelfButton;

	// Used to track child node panels, does not own memory (panels are automatically cleaned up by sizers)
	CUtlVector< CNodePanel * > m_ChildPanels;
};

//-----------------------------------------------------------------------------
// A panel with a set of "Add X" buttons where X is a state, rule, etc.
// Must be parented to a CNodePanel.
//-----------------------------------------------------------------------------
class CNewElementPanel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CNewElementPanel, vgui::EditablePanel );

public:
	CNewElementPanel( Panel *pParent, const char *pName, int nIndex );

	void AddButton( const char *pButtonText, const char *pActionName );

private:
	int m_nIndex;
	vgui::CBoxSizer *m_pHorizontalSizer;
};

#endif // NODE_PANEL_H