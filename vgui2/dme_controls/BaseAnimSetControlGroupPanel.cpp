//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================
#include "dme_controls/BaseAnimSetControlGroupPanel.h"
#include "vgui_controls/TreeView.h"
#include "vgui_controls/Menu.h"
#include "tier1/KeyValues.h"
#include "movieobjects/dmeanimationset.h"
#include "dme_controls/BaseAnimSetAttributeSliderPanel.h"
#include "dme_controls/BaseAnimationSetEditor.h"
#include "dme_controls/dmecontrols_utils.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;


//-----------------------------------------------------------------------------
// Shows the tree view of the animation groups
//-----------------------------------------------------------------------------
class CAnimGroupTree : public TreeView
{
	DECLARE_CLASS_SIMPLE( CAnimGroupTree, TreeView );
public:
	CAnimGroupTree( Panel *parent, const char *panelName, CBaseAnimSetControlGroupPanel *groupPanel );
	virtual ~CAnimGroupTree();

	virtual bool IsItemDroppable( int itemIndex, CUtlVector< KeyValues * >& msglist );
	virtual void OnItemDropped( int itemIndex, CUtlVector< KeyValues * >& msglist );
	virtual void GenerateContextMenu( int itemIndex, int x, int y );

private:
	MESSAGE_FUNC( OnImportAnimation, "ImportAnimation" );

	void CleanupContextMenu();

	vgui::DHANDLE< vgui::Menu >		m_hContextMenu;
	CBaseAnimSetControlGroupPanel	*m_pGroupPanel;
};

CAnimGroupTree::CAnimGroupTree( Panel *parent, const char *panelName, CBaseAnimSetControlGroupPanel *groupPanel ) : 
	BaseClass( parent, panelName ),
	m_pGroupPanel( groupPanel )
{
}

CAnimGroupTree::~CAnimGroupTree()
{
	CleanupContextMenu();
}

void CAnimGroupTree::CleanupContextMenu()
{
	if ( m_hContextMenu.Get() )
	{
		delete m_hContextMenu.Get();
		m_hContextMenu = NULL;
	}
}

bool CAnimGroupTree::IsItemDroppable( int itemIndex, CUtlVector< KeyValues * >& msglist )
{
	if ( msglist.Count() != 1 )
		return false;

	KeyValues *data = msglist[ 0 ];
	if ( !data )
		return false;

	if ( !data->FindKey( "color" ) )
		return false;

	KeyValues *itemData = GetItemData( itemIndex );
	if ( !itemData->FindKey( "handle" ) )
		return false;
	DmElementHandle_t handle = (DmElementHandle_t)itemData->GetInt( "handle" );
	if ( handle == DMELEMENT_HANDLE_INVALID )
		return false;

	return true;
}

void CAnimGroupTree::OnItemDropped( int itemIndex, CUtlVector< KeyValues * >& msglist )
{
	if ( !IsItemDroppable( itemIndex, msglist ) )
		return;

	KeyValues *data = msglist[ 0 ];
	if ( !data )
		return;

	KeyValues *itemData = GetItemData( itemIndex );

	CDmElement *group = GetElementKeyValue< CDmElement >( itemData, "handle" );
	Assert( m_pGroupPanel );
	Color clr = data->GetColor( "color" );
	SetItemFgColor( itemIndex, clr );
	SetItemSelectionTextColor( itemIndex, clr );

	if ( group )
	{
		CAppUndoScopeGuard guard( NOTIFY_SETDIRTYFLAG, "Change Group Color" );
		group->SetValue< Color >( "treeColor", clr );
	}
}

void CAnimGroupTree::OnImportAnimation()
{
	PostMessage( m_pGroupPanel->m_hEditor, new KeyValues( "ImportAnimation", "visibleOnly", "1" ), 0.0f );
}

// override to open a custom context menu on a node being selected and right-clicked
void CAnimGroupTree::GenerateContextMenu( int itemIndex, int x, int y )
{
	CleanupContextMenu();
	m_hContextMenu = new Menu( this, "ActionMenu" );
	m_hContextMenu->AddMenuItem( "#ImportAnimation", new KeyValues( "ImportAnimation" ), this );
	Menu::PlaceContextMenu( this, m_hContextMenu.Get() );
}



CBaseAnimSetControlGroupPanel::CBaseAnimSetControlGroupPanel( vgui::Panel *parent, const char *className, CBaseAnimationSetEditor *editor ) :
	BaseClass( parent, className ),
	m_bStartItemWasSelected( false ),
	m_SliderNames( 0, 0, true )
{
	m_hEditor = editor;
	m_hGroups = new CAnimGroupTree( this, "AnimSetGroups", this );
	m_hGroups->SetMultipleItemDragEnabled( true );
	m_hGroups->SetAutoResize
		( 
		Panel::PIN_TOPLEFT, 
		Panel::AUTORESIZE_DOWNANDRIGHT,
		0, 0,
		0, 0
		);
	m_hGroups->SetAllowMultipleSelections( true );
}

CBaseAnimSetControlGroupPanel::~CBaseAnimSetControlGroupPanel()
{
}

static int AddItemToTree( TreeView *tv, const char *label, int parentIndex, const Color& fg, int groupNumber, int handle )
{
	Color bgColor( 128, 128, 128, 128 );

	KeyValues *kv = new KeyValues( "item", "text", label );
	kv->SetInt( "groupNumber", groupNumber );
	kv->SetInt( "droppable", 1 );
	kv->SetInt( "handle", handle );
	int idx = tv->AddItem( kv, parentIndex );
	tv->SetItemFgColor( idx, fg );
	tv->SetItemSelectionTextColor( idx, fg );
	tv->SetItemSelectionBgColor( idx, bgColor );
	tv->SetItemSelectionUnfocusedBgColor( idx, bgColor );

	tv->RemoveSelectedItem( idx );
	tv->ExpandItem( idx, false );

	kv->deleteThis();

	return idx;
}

void CBaseAnimSetControlGroupPanel::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	m_hGroups->SetFont( pScheme->GetFont( "DefaultBold", IsProportional() ) );
}

void CBaseAnimSetControlGroupPanel::OnTreeViewItemSelectionCleared()
{
	// We check the entire group manually
	OnTreeViewItemSelected( -1 );
}

void CBaseAnimSetControlGroupPanel::OnTreeViewItemDeselected( int itemIndex )
{
	OnTreeViewItemSelected( -1 );
}

void CBaseAnimSetControlGroupPanel::OnTreeViewItemSelected( int itemIndex )
{
	if ( !m_AnimSet.Get() )
		return;

	// Build the list of selected groups, and notify the attribute slider panel
	CUtlVector< int > selection;
	m_hGroups->GetSelectedItems( selection );

	const CDmaElementArray<> &groups = m_AnimSet->GetSelectionGroups();
	int groupCount = groups.Count();

	int i;
	int rootIndex = m_hGroups->GetRootItemIndex();

	bool selectionHasRoot = false;
	for ( i = 0 ; i < selection.Count(); ++i )
	{
		if ( selection[ i ] == rootIndex )
		{
			selectionHasRoot = true;
			break;
		}
	}

	m_SliderNames.RemoveAll();

	if ( selectionHasRoot )
	{
		for ( i = 0; i < groups.Count(); ++i )
		{
			CDmElement *element = groups[ i ];
			if ( !element )
				continue;

			const CDmrStringArray array( element, "selectedControls" );
			if ( array.IsValid() )
			{
				for ( int j = 0 ; j < array.Count(); ++j )
				{
					const char *sliderName = array[ j ];
					if ( sliderName && *sliderName )
					{
						m_SliderNames.AddString( sliderName );
					}
				}
			}
		}
	}
	else
	{
		for ( i = 0 ; i < selection.Count(); ++i )
		{
			if ( selection[ i ] == rootIndex )
				continue;

			KeyValues *kv = m_hGroups->GetItemData( selection[ i ] );
			if ( !kv )
				continue;

			int groupNumber = kv->GetInt( "groupNumber" );
			if ( groupNumber < 0 || groupNumber >= groupCount )
			{
				const char *sliderName = kv->GetString( "text" );
				if ( sliderName && *sliderName )
				{
					m_SliderNames.AddString( sliderName );
				}
				continue;
			}

			CDmElement *element = groups[ groupNumber ];
			if ( !element )
				continue;

			const CDmrStringArray array( element, "selectedControls" );
			if ( array.IsValid() )
			{
				for ( int j = 0 ; j < array.Count(); ++j )
				{
					const char *sliderName = array[ j ];
					if ( sliderName && *sliderName )
					{
						m_SliderNames.AddString( sliderName );
					}
				}
			}
		}
	}

	// now notify the attribute slider panel
	CBaseAnimSetAttributeSliderPanel *attSliders = m_hEditor->GetAttributeSlider();
	if ( attSliders )
	{
		attSliders->SetVisibleControlsForSelectionGroup( m_SliderNames );
	}
}

void CBaseAnimSetControlGroupPanel::ChangeAnimationSet( CDmeAnimationSet *newAnimSet )
{
	bool changed = m_AnimSet.Get() != newAnimSet ? true : false;

	m_AnimSet = newAnimSet;

	if ( !m_AnimSet.Get() )
	{
		m_hGroups->RemoveAll();
		m_hSelectableIndices.RemoveAll();
		m_GroupList.RemoveAll();
		return;
	}

	// Compare groups 
	bool bRebuildGroups = false;
	const CDmaElementArray< CDmElement > &groups = m_AnimSet->GetSelectionGroups();
	int c = groups.Count();
	if ( c != m_GroupList.Count() )
	{
		bRebuildGroups = true;
	}
	else
	{
		for ( int i = 0; i < c; ++i )
		{
			CDmElement *group = groups[ i ];
			if ( group == m_GroupList[ i ].Get() )
			{
				continue;
			}

			bRebuildGroups = true;
			break;
		}
	}

	if ( bRebuildGroups )
	{
		m_hGroups->SetFont( scheme()->GetIScheme( GetScheme() )->GetFont( "DefaultBold", IsProportional() ) );

		// Build a tree of every open item in the tree view
		OpenItemTree_t openItems;
		int nRootIndex = m_hGroups->GetRootItemIndex();
		if ( nRootIndex != -1 )
		{
			BuildOpenItemList( openItems, openItems.InvalidIndex(), nRootIndex );
		}

		m_hGroups->RemoveAll();
		m_hSelectableIndices.RemoveAll();
		m_GroupList.RemoveAll();


		// Create root
		int rootIndex = AddItemToTree( m_hGroups, "root", -1, Color( 128, 128, 128, 255 ), -1, (int)DMELEMENT_HANDLE_INVALID );

		Color defaultColor( 0, 128, 255, 255 );

		CAppUndoScopeGuard *guard = NULL;

		for ( int i = 0; i < c; ++i )
		{
			CDmElement *group = groups[ i ];
			
			if ( !group->HasAttribute( "treeColor" ) )
			{
				if ( !guard )
				{
					guard = new CAppUndoScopeGuard( NOTIFY_SETDIRTYFLAG, "Set Default Colors" );
				}
				group->SetValue< Color >( "treeColor", defaultColor );
			}
			int groupIndex = AddItemToTree( m_hGroups, group->GetName(), rootIndex, group->GetValue< Color >( "treeColor" ), i, (int)group->GetHandle() );

			const CDmrStringArray array( group, "selectedControls" );
			if ( array.IsValid() )
			{
				for ( int j = 0 ; j < array.Count(); ++j )
				{
					AddItemToTree( m_hGroups, array[ j ], groupIndex, Color( 200, 200, 200, 255 ), -1, (int)DMELEMENT_HANDLE_INVALID );
				}
			}
			m_hSelectableIndices.AddToTail( groupIndex );

			m_GroupList.AddToTail( group->GetHandle() );
		}

		if ( ( nRootIndex >= 0 ) && ( rootIndex >= 0 ) && !changed )
		{
			// Iterate through all previously open items and expand them if they exist
			if ( openItems.Root() != openItems.InvalidIndex() )
			{
				ExpandOpenItems( openItems, openItems.Root(), rootIndex, true );
			}
		}
		else
		{
			m_hGroups->ExpandItem( rootIndex, true );
		}

		if ( guard )
		{
			delete guard;
		}
	}

	if ( changed  )
	{
		for ( int i = 0; i < m_hSelectableIndices.Count(); ++i )
		{

			m_hGroups->AddSelectedItem( m_hSelectableIndices[ i ], 
				false,  // don't clear selection
				true,   // put focus on tree
				false ); // don't expand tree to make all of these visible...
		}
	}
}


//-----------------------------------------------------------------------------
// Expands all items in the open item tree if they exist
//-----------------------------------------------------------------------------
void CBaseAnimSetControlGroupPanel::ExpandOpenItems( OpenItemTree_t &tree, int nOpenTreeIndex, int nItemIndex, bool makeVisible )
{
	int i = tree.FirstChild( nOpenTreeIndex );
	if ( nOpenTreeIndex != tree.InvalidIndex() )
	{
		TreeInfo_t& info = tree[ nOpenTreeIndex ];
		if ( info.m_nFlags & EP_EXPANDED )
		{
			// Expand the item
			m_hGroups->ExpandItem( nItemIndex , true );
		}
		if ( info.m_nFlags & EP_SELECTED )
		{
			m_hGroups->AddSelectedItem( nItemIndex, false, false );
			if ( makeVisible )
			{
				m_hGroups->MakeItemVisible( nItemIndex );
			}
		}
	}

	while ( i != tree.InvalidIndex() )
	{
		TreeInfo_t& info = tree[ i ];
		// Look for a match
		int nChildIndex = FindTreeItem( nItemIndex, info.m_Item );
		if ( nChildIndex != -1 )
		{
			ExpandOpenItems( tree, i, nChildIndex, makeVisible );
		}
		else
		{
			if ( info.m_nFlags & EP_SELECTED )
			{
				// Look for preserved item
				nChildIndex = FindTreeItem( nItemIndex, info.m_Item );
				if ( nChildIndex != -1 )
				{
					m_hGroups->AddSelectedItem( nChildIndex, false, false );
					if ( makeVisible )
					{
						m_hGroups->MakeItemVisible( nChildIndex );
					}
				}
			}
		}
		i = tree.NextSibling( i );
	}
}

void CBaseAnimSetControlGroupPanel::FillInDataForItem( TreeItem_t &item, int nItemIndex )
{
	KeyValues *data = m_hGroups->GetItemData( nItemIndex );
	if ( !data )
		return;

	item.m_pAttributeName = data->GetString( "text" );
}

//-----------------------------------------------------------------------------
// Builds a list of open items
//-----------------------------------------------------------------------------
void CBaseAnimSetControlGroupPanel::BuildOpenItemList( OpenItemTree_t &tree, int nParent, int nItemIndex )
{
	KeyValues *data = m_hGroups->GetItemData( nItemIndex );
	if ( !data )
		return;

	bool expanded = m_hGroups->IsItemExpanded( nItemIndex );
	bool selected = m_hGroups->IsItemSelected( nItemIndex );

	int flags = 0;
	if ( expanded )
	{
		flags |= EP_EXPANDED;
	}
	if ( selected )
	{
		flags |= EP_SELECTED;
	}

	int nChild = tree.InsertChildAfter( nParent, tree.InvalidIndex() );
	TreeInfo_t &info = tree[nChild];
	FillInDataForItem( info.m_Item, nItemIndex );
	info.m_nFlags = flags;

	// Deal with children
	int nCount = m_hGroups->GetNumChildren( nItemIndex );
	for ( int i = 0; i < nCount; ++i )
	{
		int nChildIndex = m_hGroups->GetChild( nItemIndex, i );
		BuildOpenItemList( tree, nChild, nChildIndex );
	}
}
//-----------------------------------------------------------------------------
// Finds the tree index of a child matching the particular element + attribute
//-----------------------------------------------------------------------------
int CBaseAnimSetControlGroupPanel::FindTreeItem( int nParentIndex, const TreeItem_t &info )
{
	// Look for a match
	int nCount = m_hGroups->GetNumChildren( nParentIndex );
	for ( int i = nCount; --i >= 0; )
	{
		int nChildIndex = m_hGroups->GetChild( nParentIndex, i );
		KeyValues *data = m_hGroups->GetItemData( nChildIndex );
		Assert( data );

		const char *pAttributeName = data->GetString( "text" );
		if ( !Q_stricmp( pAttributeName, info.m_pAttributeName ) )
		{
			return nChildIndex;
		}
	}
	return -1;
}
