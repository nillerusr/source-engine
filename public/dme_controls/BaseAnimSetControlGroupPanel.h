//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef BASEANIMSETCONTROLGROUPPANEL_H
#define BASEANIMSETCONTROLGROUPPANEL_H
#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/EditablePanel.h"
#include "datamodel/dmehandle.h"
#include "tier1/utlntree.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CBaseAnimationSetEditor;
class CDmeAnimationSet;

namespace vgui
{
	class TreeView;
	class IScheme;
	class Menu;
};


//-----------------------------------------------------------------------------
// Panel which shows a tree of controls
//-----------------------------------------------------------------------------
class CBaseAnimSetControlGroupPanel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CBaseAnimSetControlGroupPanel, EditablePanel );
public:
	CBaseAnimSetControlGroupPanel( vgui::Panel *parent, char const *className, CBaseAnimationSetEditor *editor );
	virtual ~CBaseAnimSetControlGroupPanel();

	void ChangeAnimationSet( CDmeAnimationSet *newAnimSet );
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );

protected:

	MESSAGE_FUNC_INT( OnTreeViewItemSelected, "TreeViewItemSelected", itemIndex );
	MESSAGE_FUNC_INT( OnTreeViewItemDeselected, "TreeViewItemDeselected", itemIndex );
	MESSAGE_FUNC( OnTreeViewItemSelectionCleared, "TreeViewItemSelectionCleared" );

protected:
	enum
	{
		EP_EXPANDED = (1<<0),
		EP_SELECTED = (1<<1),
	};

	struct TreeItem_t
	{
		TreeItem_t() : m_pAttributeName() {}
		CUtlString m_pAttributeName;
	};

	// Used to build a list of open element for refresh
	struct TreeInfo_t
	{
		TreeInfo_t() : m_nFlags( 0 ) {}
		TreeItem_t	m_Item;	// points to the element referenced in an element array
		int			m_nFlags;
	};

	typedef CUtlNTree< TreeInfo_t, int > OpenItemTree_t;
	// Expands all items in the open item tree if they exist
	void ExpandOpenItems( OpenItemTree_t &tree, int nOpenTreeIndex, int nItemIndex, bool makeVisible );
	// Builds a list of open items
	void BuildOpenItemList( OpenItemTree_t &tree, int nParent, int nItemIndex );
	void FillInDataForItem( TreeItem_t &item, int nItemIndex );
	// Finds the tree index of a child matching the particular element + attribute
	int FindTreeItem( int nParentIndex, const TreeItem_t &info );

	vgui::DHANDLE< CBaseAnimationSetEditor >	m_hEditor;

	vgui::DHANDLE< vgui::TreeView >		m_hGroups;
	CUtlVector< int >					m_hSelectableIndices;

	CDmeHandle< CDmeAnimationSet >		m_AnimSet;

	bool								m_bStartItemWasSelected;
	CUtlVector< int >					m_SavedSelectedGroups;
	CUtlSymbolTable						m_SliderNames;
	CUtlVector< CDmeHandle< CDmElement > > m_GroupList;

	friend class CAnimGroupTree;
};

#endif // BASEANIMSETCONTROLGROUPPANEL_H
