//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef VGUI_HELPERS_H
#define VGUI_HELPERS_H
#ifdef _WIN32
#pragma once
#endif


#include <vgui_controls/TreeView.h>
#include <vgui_controls/CheckButton.h>


class KeyValues;
class ConVar;


// This control keeps a ConVar's value updated with a CheckButton's value.
class CConVarCheckButton : public vgui::CheckButton
{
public:
	
	typedef vgui::CheckButton BaseClass;


	CConVarCheckButton( vgui::Panel *parent, const char *panelName, const char *text );
	
	// Call this to initialize it with a cvar. The CheckButton will be set to the current
	// value of the ConVar.
	void SetConVar( ConVar *pVar );
	
	virtual void SetSelected( bool state );
	

public:
	
	ConVar *m_pConVar;
};




// Return true if the state was changed at all (in any way that would require an InvalidateLayout on the control).
typedef bool (*UpdateItemStateFn)(
	vgui::TreeView *pTree, 
	int iChildItemId, 
	KeyValues *pSub );


// This function takes a bunch of KeyValues entries and incrementally updates
// a tree control. This can be a lot more efficient than clearing the whole tree
// control and re-adding all the elements if most of the elements don't usually change.
//
// NOTE: Only KeyValues nodes with a string named "Text" will be treated as items
// that should be added to the tree.
//
// If iRoot is -1, then it uses GetRootItemIndex().
// 
// Returns true if any elements were added or changed.
bool IncrementalUpdateTree( 
	vgui::TreeView *pTree, 
	KeyValues *pValues,
	UpdateItemStateFn fn,
	int iRoot = -1
	);


// Copy the contents of the list panel to the clipboard in tab-delimited form for Excel.
void CopyListPanelToClipboard( vgui::ListPanel *pListPanel );


#endif // VGUI_HELPERS_H
