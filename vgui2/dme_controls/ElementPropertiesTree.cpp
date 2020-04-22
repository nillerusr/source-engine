//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "dme_controls/elementpropertiestree.h"
#include "tier1/KeyValues.h"
#include "datamodel/dmelement.h"

#include "vgui/IInput.h"
#include "vgui/ISurface.h"
#include "vgui/ISystem.h"
#include "vgui/IVgui.h"
#include "vgui/Cursor.h"
#include "vgui_controls/TextEntry.h"
#include "vgui_controls/ComboBox.h"
#include "vgui_controls/Button.h"
#include "vgui_controls/FileOpenDialog.h"
#include "vgui_controls/Menu.h"
#include "vgui_controls/MenuItem.h"
#include "vgui_controls/MenuButton.h"
#include "vgui_controls/PanelListPanel.h"
#include "vgui_controls/ScrollBar.h"
#include "movieobjects/dmeeditortypedictionary.h"
#include "dme_controls/AttributeTextPanel.h"
#include "dme_controls/DmePanel.h"
#include "dme_controls/dmecontrols_utils.h"
#include "tier1/ConVar.h"
#include "tier2/fileutils.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CElementTreeViewListControl;

using namespace vgui;


//-----------------------------------------------------------------------------
//
// CElementTree
//
//-----------------------------------------------------------------------------
class CElementTree : public TreeView
{
	DECLARE_CLASS_SIMPLE( CElementTree, TreeView );
public:
	CElementTree( CElementPropertiesTreeInternal *parent, const char *panelName );
	~CElementTree();

	virtual void OnCommand( const char *cmd );
	virtual void ApplySchemeSettings( IScheme *pScheme );
	virtual void InvalidateLayout( bool layoutNow = false, bool reloadScheme = false );
	virtual void GenerateChildrenOfNode(int itemIndex);
	// override to open a custom context menu on a node being selected and right-clicked
	virtual void GenerateContextMenu( int itemIndex, int x, int y );

	virtual void GenerateDragDataForItem( int itemIndex, KeyValues *msg );

	virtual void OnLabelChanged( int itemIndex, const char *oldString, const char *newString );

	virtual bool IsItemDroppable( int m_ItemIndex, CUtlVector< KeyValues * >& msglist );
	virtual void OnItemDropped( int m_ItemIndex, CUtlVector< KeyValues * >& msglist );
	virtual bool GetItemDropContextMenu( int itemIndex, Menu *menu, CUtlVector< KeyValues * >& msglist );
	virtual HCursor GetItemDropCursor( int itemIndex, CUtlVector< KeyValues * >& msglist );

	ScrollBar	*GetScrollBar();

private:
	Menu		*m_pEditMenu;
	CElementPropertiesTreeInternal	*m_pParent;
	ScrollBar		*m_pVertSB;
};


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CElementTree::CElementTree( CElementPropertiesTreeInternal *parent, const char *panelName ) :
	BaseClass( (Panel *)parent, panelName ),
	m_pEditMenu( 0 ),
	m_pParent( parent )
{
	SetAllowLabelEditing( true );
	SetAllowMultipleSelections( true );

	m_pVertSB = SetScrollBarExternal( true, this );
}

//-----------------------------------------------------------------------------
// Destructor
//-----------------------------------------------------------------------------
CElementTree::~CElementTree()
{
	delete m_pEditMenu;
}

ScrollBar *CElementTree::GetScrollBar()
{
	return m_pVertSB;
}

bool CElementTree::IsItemDroppable( int itemIndex, CUtlVector< KeyValues * >& msglist )
{
	return m_pParent->IsItemDroppable( itemIndex, msglist );
}

bool CElementTree::GetItemDropContextMenu( int itemIndex, Menu *menu, CUtlVector< KeyValues * >& msglist )
{
	return m_pParent->GetItemDropContextMenu( itemIndex, menu, msglist );
}

void CElementTree::OnItemDropped( int itemIndex, CUtlVector< KeyValues * >& msglist )
{
	m_pParent->OnItemDropped( itemIndex, msglist );
}

HCursor CElementTree::GetItemDropCursor( int itemIndex, CUtlVector< KeyValues * >& msglist )
{
	return m_pParent->GetItemDropCursor( itemIndex, msglist );
}

void CElementTree::OnLabelChanged( int itemIndex, const char *oldString, const char *newString )
{
	m_pParent->OnLabelChanged( itemIndex, oldString, newString );
}

void CElementTree::GenerateDragDataForItem( int itemIndex, KeyValues *msg )
{
	m_pParent->GenerateDragDataForItem( itemIndex, msg );
}

// override to open a custom context menu on a node being selected and right-clicked
void CElementTree::GenerateContextMenu( int itemIndex, int x, int y )
{
	m_pParent->GenerateContextMenu( itemIndex, x, y );
}


void CElementTree::ApplySchemeSettings( IScheme *pScheme )
{
	// Intentionally skip to Panel:: instead of BaseClass::!!!
	Panel::ApplySchemeSettings( pScheme );

	SetFont( pScheme->GetFont( "DmePropertyVerySmall", IsProportional() ) );
}

void CElementTree::InvalidateLayout( bool layoutNow, bool reloadScheme )
{
	BaseClass::InvalidateLayout( layoutNow, reloadScheme );
	if ( GetParent() && !reloadScheme )
	{
		GetParent()->InvalidateLayout( layoutNow, false );
	}
}

void CElementTree::OnCommand( const char *cmd )
{
	// Relay to parent
	GetParent()->OnCommand( cmd );
}

void CElementTree::GenerateChildrenOfNode(int itemIndex)
{
	m_pParent->GenerateChildrenOfNode( itemIndex );
}


//-----------------------------------------------------------------------------
//
// Class: CElementTreeViewListControl
//
//-----------------------------------------------------------------------------
CElementTreeViewListControl::CElementTreeViewListControl( Panel *pParent, const char *pName )
	: BaseClass( pParent, pName ), m_Panels( 0, 0, PanelsLessFunc )
{

	m_iTreeColumnWidth = 200;
	m_iFontSize = 1;
	m_bMouseLeftIsDown = false;
	m_bMouseIsDragging = false;
	m_bDrawGrid = false;

	// why do this here?
	SetScheme( vgui::scheme()->LoadSchemeFromFile( "Resource/BoxRocket.res", "BoxRocket" ) );

	// the column lable font
	vgui::IScheme *scheme = vgui::scheme()->GetIScheme( GetScheme() );
	HFont font = scheme->GetFont( "DefaultVerySmall", IsProportional() );

	SetTitleBarInfo( font, 18 );

	SetPostChildPaintEnabled( true );
	SetBorderColor( Color( 255, 255, 196, 64 ) );

	SetKeyBoardInputEnabled( true );
}


int CElementTreeViewListControl::AddItem( KeyValues *data, bool allowLabelEditing, int parentItemIndex, CUtlVector< vgui::Panel * >& columnPanels )
{
	int itemIndex = GetTree()->AddItem( data, parentItemIndex );
	if ( allowLabelEditing )
	{
		GetTree()->SetLabelEditingAllowed( itemIndex, allowLabelEditing );
	}

	GetTree()->SetItemFgColor( itemIndex, GetFgColor() );
	GetTree()->SetItemBgColor( itemIndex, GetBgColor() );

	ColumnPanels_t search;
	search.treeViewItem = itemIndex;

	int idx = m_Panels.Find( search );
	if ( idx == m_Panels.InvalidIndex() )
	{
		ColumnPanels_t newInfo;
		newInfo.treeViewItem = itemIndex;
		idx = m_Panels.Insert( newInfo );
	}

	ColumnPanels_t& info = m_Panels[ idx ];

	info.SetList( columnPanels );

	int c = columnPanels.Count();
	for ( int i = 0; i < c; ++i )
	{
		if ( columnPanels[ i ] )
		{
			columnPanels[ i ]->SetParent( this );
		}
	}

//	GetTree()->InvalidateLayout( false, true );

	return itemIndex;
}


//-----------------------------------------------------------------------------
// Removes an item recursively
//-----------------------------------------------------------------------------
void CElementTreeViewListControl::RemoveItem_R( int nItemIndex )
{
	ColumnPanels_t search;
	search.treeViewItem = nItemIndex;
	int idx = m_Panels.Find( search );
	if ( idx != m_Panels.InvalidIndex() )
	{
		ColumnPanels_t& info = m_Panels[ idx ];
		int nCount = info.m_Columns.Count();
		for ( int i = 0; i < nCount; ++i )
		{
			if ( info.m_Columns[i] )
			{
				info.m_Columns[i]->SetParent( (Panel*)NULL );
				info.m_Columns[i]->MarkForDeletion();
			}
		}
		m_Panels.RemoveAt( idx );
	}

	int nCount = GetTree()->GetNumChildren( nItemIndex );
	for ( int i = 0; i < nCount; ++i )
	{
		RemoveItem_R( GetTree()->GetChild( nItemIndex, i ) ); 
	}
}


//-----------------------------------------------------------------------------
// Removes an item
//-----------------------------------------------------------------------------
void CElementTreeViewListControl::RemoveItem( int nItemIndex )
{
	RemoveItem_R( nItemIndex );
	GetTree()->RemoveItem( nItemIndex, false, true );
	RecalculateRows();
	InvalidateLayout();
}


int CElementTreeViewListControl::GetTreeColumnWidth()
{
	return m_iTreeColumnWidth;
}

void CElementTreeViewListControl::SetTreeColumnWidth(int w)
{
	m_iTreeColumnWidth = w;
	SetColumnInfo( 0, "Tree", m_iTreeColumnWidth );
}

void CElementTreeViewListControl::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	GetTree()->SetFont( pScheme->GetFont( "DmePropertyVerySmall", IsProportional() ) );
}


//-----------------------------------------------------------------------------
// Purpose: Handle mouse drag resize of tree column (hack)
//-----------------------------------------------------------------------------
void CElementTreeViewListControl::OnCursorMoved(int x, int y)
{

	if ( ( x > m_iTreeColumnWidth - 12 ) &&
		 ( x < m_iTreeColumnWidth + 12 ) )
	{
		SetCursor( dc_sizewe );
		if ( m_bMouseLeftIsDown )
		{
			m_bMouseIsDragging = true;
		}
	}
	else
	{
		SetCursor( dc_arrow );
	}

	if ( m_bMouseIsDragging )
	{
		SetCursor( dc_sizewe );
		SetTreeColumnWidth( x );
		InvalidateLayout( true );
	}

}

void CElementTreeViewListControl::OnMousePressed( MouseCode code )
{
	BaseClass::OnMousePressed( code );
	if ( code == MOUSE_LEFT )
	{
		m_bMouseLeftIsDown = true;
	}
	input()->SetMouseCapture(GetVPanel());
	RequestFocus();
}

void CElementTreeViewListControl::OnMouseReleased( MouseCode code )
{
	BaseClass::OnMouseReleased( code );
	if ( code == MOUSE_LEFT )
	{
		m_bMouseLeftIsDown = false;
		m_bMouseIsDragging = false;
	}
	input()->SetMouseCapture(NULL);
}

void CElementTreeViewListControl::OnMouseDoublePressed( MouseCode code )
{
	int x, y;
	input()->GetCursorPos(x, y);
	ScreenToLocal(x, y);

	// resize the column to the max width of the tree
	if ( ( x > m_iTreeColumnWidth - 12 ) &&
		 ( x < m_iTreeColumnWidth + 12 ) )
	{
		ResizeTreeToExpandedWidth();
	}

	BaseClass::OnMouseDoublePressed( code );

}

void CElementTreeViewListControl::ResizeTreeToExpandedWidth()
{
	int rows = GetNumRows();
	int vbarTop, nItemsVisible;
	bool hbarVisible = false;
	GetTree()->GetVBarInfo( vbarTop, nItemsVisible, hbarVisible );
	int vBarWidth = 0;
	if ( nItemsVisible <= rows )
	{
		vBarWidth = 27;
	}
	SetTreeColumnWidth( GetTree()->GetVisibleMaxWidth() + vBarWidth + 14 );
}

void CElementTreeViewListControl::OnMouseWheeled(int delta)
{

	bool ctrl = (input()->IsKeyDown(KEY_LCONTROL) || input()->IsKeyDown(KEY_RCONTROL));
	bool alt = (input()->IsKeyDown(KEY_LALT) || input()->IsKeyDown(KEY_RALT));

	if ( ctrl )
	{
		SetFontSize( GetFontSize() + delta );
	}
	else if ( alt )
	{
		ToggleDrawGrid();
	}
	else
	{
		// scroll the treeview control
		ScrollBar *sb = ((CElementTree *)GetTree())->GetScrollBar();
		sb->SetValue( sb->GetValue() + ( delta * -3 ) );
	}
}

void CElementTreeViewListControl::ToggleDrawGrid()
{
	m_bDrawGrid = !m_bDrawGrid;
}

bool CElementTreeViewListControl::IsDrawingGrid()
{
	return m_bDrawGrid;
}

void CElementTreeViewListControl::PostChildPaint()
{
	// why isn't SetBorderColor doing the job???
	vgui::surface()->DrawSetColor( Color( 255, 255, 196, 32 ) );

	int left, top, right, bottom;
	int wide, tall;
	GetSize( wide, tall );
	GetGridElementBounds( 0, 0, left, top, right, bottom );
	vgui::surface()->DrawFilledRect( right, 1, right+3, tall );

	if ( m_bDrawGrid )
	{
		int numColumns = GetNumColumns();
		int rows = GetNumRows();

		int vbarTop, nItemsVisible;
		bool hbarVisible = false;
		GetTree()->GetVBarInfo( vbarTop, nItemsVisible, hbarVisible );

		int vBarWidth = 0;
		if ( nItemsVisible <= rows )
		{
			vBarWidth = 21;
		}

		if ( hbarVisible )
		{
			--nItemsVisible;
		}

		for ( int col = 0; col < numColumns; ++col )
		{
			for ( int row = 0; row < rows; ++row )
			{
				GetGridElementBounds( col, row, left, top, right, bottom );
				if (col == 0)
				{
					vgui::surface()->DrawLine( left+4, bottom, right, bottom );
				}
				else
				{
					vgui::surface()->DrawLine( left-3, bottom, right-2, bottom );
				}
			}
		}
	}
}

int	CElementTreeViewListControl::GetScrollBarSize()
{
	ScrollBar *sb = ((CElementTree *)GetTree())->GetScrollBar();
	if ( sb )
	{
		return sb->GetWide();
	}
	return 0;
}

void CElementTreeViewListControl::PerformLayout()
{
	BaseClass::PerformLayout();

	// Assume all invisible at first
	HideAll();

	GetTree()->PerformLayout();

	ScrollBar *sb = ((CElementTree *)GetTree())->GetScrollBar();
	if ( sb && sb->GetParent() )
	{
		sb->SetBounds( sb->GetParent()->GetWide() - sb->GetWide(), 0, sb->GetWide(), sb->GetParent()->GetTall() );
	}

	int rowheight = GetTree()->GetRowHeight();
	int treetop, visitems;
	bool hbarVisible = false;
	GetTree()->GetVBarInfo( treetop, visitems, hbarVisible );
	if ( hbarVisible )
	{
		--visitems;
	}

	int offset = -treetop * rowheight;

	int headerHeight = GetTitleBarHeight();

	int numColumns = GetNumColumns();
	// Now position column panels into the correct spot
	int rows = GetNumRows();
	int visItemCount = 0;

	for ( int row = 0; row < rows; ++row )
	{
		int tvi = GetTreeItemAtRow( row );

		for ( int col = 0; col < numColumns; ++col )
		{
			int left, top, right, bottom;
			GetGridElementBounds( col, row, left, top, right, bottom );

			ColumnPanels_t search;
			search.treeViewItem = tvi;

			int idx = m_Panels.Find( search );
			if ( idx != m_Panels.InvalidIndex() )
			{
				ColumnPanels_t& info = m_Panels[ idx ];

				if ( col >= info.m_Columns.Count() )
					continue;

				vgui::Panel *p = info.m_Columns[ col ];
				if ( !p )
				{
					continue;
				}

				bool vis = top + offset >= headerHeight;
				if ( vis )
				{
					++visItemCount;

					if ( visItemCount > visitems )
					{
						vis = false;
					}
				}

				p->SetVisible( vis );
				p->SetBounds( left + 4, top + offset, right - left, bottom - top );

				p->InvalidateLayout();
			}
			else
			{
				Assert( 0 );
			}
		}
	}
}

void CElementTreeViewListControl::HideAll()
{
	for ( int i = m_Panels.FirstInorder(); i != m_Panels.InvalidIndex(); i = m_Panels.NextInorder( i ) )
	{
		ColumnPanels_t& info = m_Panels[ i ];
		int c = info.m_Columns.Count();
		for ( int j = 0 ; j < c; ++j )
		{
			Panel *panel = info.m_Columns[ j ];
			if ( !panel )
			{
				continue;
			}
			panel->SetVisible( false );
		}
	}
}

void CElementTreeViewListControl::RemoveAll()
{
	GetTree()->RemoveAll();

	for ( int i = m_Panels.FirstInorder(); i != m_Panels.InvalidIndex(); i = m_Panels.NextInorder( i ) )
	{
		ColumnPanels_t& info = m_Panels[ i ];
		int c = info.m_Columns.Count();
		for ( int j = 0 ; j < c; ++j )
		{
			delete info.m_Columns[ j ];
		}
		info.m_Columns.RemoveAll();
	}
	m_Panels.RemoveAll();
	InvalidateLayout();
}

HFont CElementTreeViewListControl::GetFont( int size )
{
	vgui::IScheme *scheme = vgui::scheme()->GetIScheme( GetScheme() );

	switch(size)
	{
	case 1:
		return scheme->GetFont( "DmePropertyVerySmall", IsProportional() );
	case 2:
		return scheme->GetFont( "DmePropertySmall", IsProportional() );
	case 3:
		return scheme->GetFont( "DmeProperty", IsProportional() );
	case 4:
		return scheme->GetFont( "DmePropertyLarge", IsProportional() );
	case 5:
		return scheme->GetFont( "DmePropertyVeryLarge", IsProportional() );
	default:
		return NULL;
	}
}

void CElementTreeViewListControl::SetFont( HFont font )
{
	// set the font for the tree
	GetTree()->SetFont( font );

	// and now set the font on the data column...
	for ( int i = m_Panels.FirstInorder(); i != m_Panels.InvalidIndex(); i = m_Panels.NextInorder( i ) )
	{
		ColumnPanels_t& info = m_Panels[ i ];
		int c = info.m_Columns.Count();
		for ( int j = 0 ; j < c; ++j )
		{
			Panel *panel = info.m_Columns[ j ];
			if ( !panel )
			{
				continue;
			}

			CBaseAttributePanel *attrPanel = dynamic_cast< CBaseAttributePanel * >( panel );
			if ( !attrPanel )
			{
				continue;
			}
			attrPanel->SetFont( font );
		}
	}
}

int CElementTreeViewListControl::GetFontSize()
{
	return m_iFontSize;
}

void CElementTreeViewListControl::SetFontSize( int size )
{
	m_iFontSize = min( 5, max( 1, size ) );
	SetFont( GetFont( m_iFontSize ) );
}

void CElementTreeViewListControl::ExpandItem(int itemIndex, bool bExpand)
{
	GetTree()->ExpandItem( itemIndex, bExpand );
}
				   
bool CElementTreeViewListControl::IsItemExpanded( int itemIndex )
{
	return GetTree()->IsItemExpanded( itemIndex );
}

bool CElementTreeViewListControl::IsItemSelected( int itemIndex )
{
	return GetTree()->IsItemSelected( itemIndex );
}


KeyValues *CElementTreeViewListControl::GetItemData(int itemIndex)
{
	return GetTree()->GetItemData( itemIndex );
}


class CHistoryMenuButton : public MenuButton
{
DECLARE_CLASS_SIMPLE( CHistoryMenuButton, MenuButton );
public:
	CHistoryMenuButton( Panel *parent, const char *panelName, const char *text, CElementPropertiesTreeInternal *tree, int whichMenu );

	virtual void OnShowMenu( Menu *menu );
	virtual int	 OnCheckMenuItemCount();


private:
	CElementPropertiesTreeInternal	*m_pPropertiesTreeInternal;
	int								m_nWhichMenu;
};

CHistoryMenuButton::CHistoryMenuButton( Panel *parent, const char *panelName, const char *text, CElementPropertiesTreeInternal *tree, int whichMenu )
	: BaseClass( parent, panelName, text ), m_pPropertiesTreeInternal( tree ), m_nWhichMenu( whichMenu )
{
	Assert( m_pPropertiesTreeInternal );
}

int CHistoryMenuButton::OnCheckMenuItemCount()
{
	Assert( m_pPropertiesTreeInternal );
	if ( !m_pPropertiesTreeInternal )
		return 0;

	return m_pPropertiesTreeInternal->GetHistoryMenuItemCount( m_nWhichMenu );
}

void CHistoryMenuButton::OnShowMenu( Menu *menu )
{
	Assert( m_pPropertiesTreeInternal );
	if ( !m_pPropertiesTreeInternal )
		return;

	m_pPropertiesTreeInternal->PopulateHistoryMenu( m_nWhichMenu, menu );
}

class CSearchComboBox : public ComboBox
{
	DECLARE_CLASS_SIMPLE( CSearchComboBox, ComboBox );
public:

	CSearchComboBox( CElementPropertiesTreeInternal *tree, vgui::Panel *parent, const char *panelName, int numLines, bool allowEdit );

	virtual void OnMenuItemSelected();
	virtual void OnShowMenu(Menu *menu);

private:

	CElementPropertiesTreeInternal	*m_pTree;
};

CSearchComboBox::CSearchComboBox( CElementPropertiesTreeInternal *tree, vgui::Panel *parent, const char *panelName, int numLines, bool allowEdit )
	: BaseClass( parent, panelName, numLines, allowEdit ), m_pTree( tree )
{
	Assert( m_pTree );
}

void CSearchComboBox::OnShowMenu(Menu *menu)
{
	menu->DeleteAllItems();
	Assert( m_pTree );
	if ( m_pTree )
	{
		m_pTree->PopulateHistoryMenu( CElementPropertiesTreeInternal::DME_PROPERTIESTREE_MENU_SEARCHHSITORY, menu );
	}
}

void CSearchComboBox::OnMenuItemSelected()
{
	BaseClass::OnMenuItemSelected();

	int idx = GetActiveItem();
	if ( idx < 0 )
		return;

	char name[ 256 ];
	GetItemText( idx, name, sizeof( name ) );

	Assert( m_pTree );
	if ( m_pTree && name[ 0 ] )
	{
		m_pTree->OnNavSearch( name );
	}
}

class CPropertiesTreeToolbar : public Panel
{
	DECLARE_CLASS_SIMPLE( CPropertiesTreeToolbar, Panel );
public:
	CPropertiesTreeToolbar( vgui::Panel *parent, const char *panelName, CElementPropertiesTreeInternal *tree );

	virtual void ApplySchemeSettings( IScheme *scheme );

	virtual void PerformLayout();

	MESSAGE_FUNC( OnTextNewLine, "TextNewLine" );

	virtual void OnKeyCodeTyped( KeyCode code );

	void			UpdateButtonState();
private:

	CElementPropertiesTreeInternal	*m_pTree;

	CHistoryMenuButton	*m_pBack;
	CHistoryMenuButton	*m_pFwd;
	Label				*m_pSearchLabel;
	CSearchComboBox		*m_pSearch;
	// Button				*m_pShowSearchResults;
};

CPropertiesTreeToolbar::CPropertiesTreeToolbar( vgui::Panel *parent, const char *panelName, CElementPropertiesTreeInternal *tree ) :
	BaseClass( parent, panelName ), m_pTree( tree )
{
	Assert( m_pTree );

	SetPaintBackgroundEnabled( false );

	m_pBack = new CHistoryMenuButton( this, "Nav_Back", "#Dme_NavBack", tree, CElementPropertiesTreeInternal::DME_PROPERTIESTREE_MENU_BACKWARD );
	m_pBack->SetCommand( new KeyValues( "OnNavigateBack", "item", -1 ) );
	m_pBack->AddActionSignalTarget( parent );
	m_pBack->SetDropMenuButtonStyle( true );

	m_pBack->SetMenu( new Menu( this, "Nav_BackMenu" ) );

	m_pFwd = new CHistoryMenuButton( this, "Nav_Forward", "#Dme_NavForward", tree, CElementPropertiesTreeInternal::DME_PROPERTIESTREE_MENU_FORWARD );
	m_pFwd->SetCommand( new KeyValues( "OnNavigateForward", "item", -1 ) );
	m_pFwd->AddActionSignalTarget( parent );
	m_pFwd->SetDropMenuButtonStyle( true );
	m_pFwd->SetMenu( new Menu( this, "Nav_FwdMenu" ) );	
	
	m_pSearch = new CSearchComboBox( tree, this, "Nav_Search", 20, true );
	m_pSearch->SendNewLine( true );
	m_pSearch->SelectAllOnFocusAlways( true );
	m_pSearch->AddActionSignalTarget( this );

	/*
	m_pShowSearchResults = new Button( this, "Nav_ShowResults", "Show Results" );
	m_pShowSearchResults->SetCommand( new KeyValues( "OnShowSearchResults" ) );
	m_pShowSearchResults->AddActionSignalTarget( parent );
	*/

	m_pSearchLabel = new Label( this, "Nav_SearchLabel", "#Dme_NavSearch" );
}

void CPropertiesTreeToolbar::UpdateButtonState()
{
	m_pBack->SetEnabled( m_pTree->GetHistoryMenuItemCount( CElementPropertiesTreeInternal::DME_PROPERTIESTREE_MENU_BACKWARD ) > 0 ? true : false );
	m_pFwd->SetEnabled( m_pTree->GetHistoryMenuItemCount( CElementPropertiesTreeInternal::DME_PROPERTIESTREE_MENU_FORWARD ) > 0 ? true : false );
	
	//m_pShowSearchResults->SetEnabled( m_pTree->GetHistoryMenuItemCount( CElementPropertiesTreeInternal::DME_PROPERTIESTREE_MENU_SEARCHHSITORY ) > 0 ? true : false );
}

void CPropertiesTreeToolbar::OnTextNewLine()
{
	Panel *parent = GetParent();
	Assert( parent );
	if ( !parent )
		return;

	char searchBuf[ 256 ];
	m_pSearch->GetText( searchBuf, sizeof( searchBuf ) );

	KeyValues *msg = new KeyValues( "OnNavigateSearch", "text", searchBuf );

	PostMessage( parent, msg );
}

void CPropertiesTreeToolbar::OnKeyCodeTyped( KeyCode code )
{
	switch ( code )
	{
	case KEY_F3:
		{
			bool shift = (input()->IsKeyDown(KEY_LSHIFT) || input()->IsKeyDown(KEY_RSHIFT));
			
			Panel *parent = GetParent();
			Assert( parent );
			if ( parent )
			{
				KeyValues *msg = new KeyValues( "OnNavigateSearchAgain", "direction", shift ? -1 : 1 );
				PostMessage( parent, msg );
			}
		}
		break;
	default:
		BaseClass::OnKeyCodeTyped( code );
		break;
	}
}

void CPropertiesTreeToolbar::ApplySchemeSettings( IScheme *scheme )
{
	BaseClass::ApplySchemeSettings( scheme );

	m_pBack->SetFont( scheme->GetFont( "DefaultVerySmall" ) );
	m_pFwd->SetFont( scheme->GetFont( "DefaultVerySmall" ) );
	m_pSearch->SetFont( scheme->GetFont( "DefaultVerySmall" ) );
	m_pSearchLabel->SetFont( scheme->GetFont( "DefaultVerySmall" ) );
	//m_pShowSearchResults->SetFont( scheme->GetFont( "DefaultVerySmall" ) );

	m_pSearch->SendNewLine( true );
	m_pSearch->SelectAllOnFocusAlways( true );

	m_pBack->GetMenu()->SetFont( scheme->GetFont( "DefaultVerySmall" ) );
	m_pFwd->GetMenu()->SetFont( scheme->GetFont( "DefaultVerySmall" ) );
}

void CPropertiesTreeToolbar::PerformLayout()
{
	BaseClass::PerformLayout();
	int w, h;
	GetSize( w, h );

	int buttonw = 75;
	int buttonh =  h - 6;

	int x = 2;

	m_pBack->SetBounds( x, 3, buttonw, buttonh );

	x += buttonw + 2;

	m_pFwd->SetBounds( x, 3, buttonw, buttonh );

	x += buttonw + 15;

	m_pSearchLabel->SetBounds( x, 2, 50, buttonh );

	x += 50 + 2;

	int textw = ( w - 2 ) - x;

	//textw -= 75;

	m_pSearch->SetBounds( x, 2, textw, buttonh );

	//x += textw;

	//m_pShowSearchResults->SetBounds( x, 2, 75, buttonh );
	
}

//-----------------------------------------------------------------------------
//
// CElementPropertiesTreeInternal 
//
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CElementPropertiesTreeInternal::CElementPropertiesTreeInternal( 
	vgui::Panel *parent, IDmNotify *pNotify, CDmElement *pObject, bool autoApply /* = true */, CDmeEditorTypeDictionary *pDict /* = NULL */ ) :
	BaseClass( parent, "ElementPropertiesTree" ),
	m_pNotify( pNotify ),
	m_hTypeDictionary( pDict ),
	m_bAutoApply( autoApply ), m_bShowMemoryUsage( false )
{
	m_hObject = pObject;
	m_bSuppressHistoryUpdates = false;
	m_nCurrentHistoryPosition = 0;
	m_szSearchStr[ 0 ] = 0;
	m_nCurrentSearchResult = 0;

	SetVisible( true );

	Assert( m_pNotify );
	
	CElementTree *dmeTree = new CElementTree( this, "ElementTree" );
	dmeTree->SetDragEnabledItems( true );

	m_pTree = new CElementTreeViewListControl( this, "ElementTreeList" );
	m_pTree->SetTreeView( dmeTree );
	m_pTree->SetNumColumns( 2 );
	m_pTree->SetColumnInfo( 0, "Tree", m_pTree->GetTreeColumnWidth() );
	m_pTree->SetColumnInfo( 1, "Data", 1600 );

	m_pToolBar = new CPropertiesTreeToolbar( this, "ElementTreeToolbar", this );
	// m_pToolBar->SetTreeView( dmeTree );

	ScrollBar *sb = dmeTree->GetScrollBar();
	if ( sb )
	{
		sb->SetParent( m_pTree );
	}

	SETUP_PANEL( dmeTree );
	SETUP_PANEL( m_pTree );
	SETUP_PANEL( m_pToolBar );

	{
		CDmElement *pResults = CreateElement< CDmElement >( "Search Results", DMFILEID_INVALID );
		Assert( pResults );
		pResults->AddAttributeElementArray< CDmElement >( "results" );
		m_SearchResultsRoot = pResults;
	}

	LoadControlSettings( "resource/BxElementPropertiesTree.res" );

	m_hDragCopyCursor = surface()->CreateCursorFromFile( "resource/drag_copy.cur" );
	m_hDragLinkCursor = surface()->CreateCursorFromFile( "resource/drag_link.cur" );
	m_hDragMoveCursor = surface()->CreateCursorFromFile( "resource/drag_move.cur" );
}


//-----------------------------------------------------------------------------
// Destructor
//-----------------------------------------------------------------------------
CElementPropertiesTreeInternal::~CElementPropertiesTreeInternal()
{
	if ( m_SearchResultsRoot.Get() )
	{
		g_pDataModel->DestroyElement( m_SearchResultsRoot );
	}
}

void CElementPropertiesTreeInternal::UpdateButtonState()
{
	m_pToolBar->UpdateButtonState();
}


//-----------------------------------------------------------------------------
// Message sent when something changed the element you're looking at
//-----------------------------------------------------------------------------
void CElementPropertiesTreeInternal::OnElementChangedExternally( int valuesOnly )
{
	Refresh( valuesOnly ? REFRESH_VALUES_ONLY : REFRESH_TREE_VIEW );
}


//-----------------------------------------------------------------------------
// Sets the type dictionary
//-----------------------------------------------------------------------------
void CElementPropertiesTreeInternal::SetTypeDictionary( CDmeEditorTypeDictionary *pDict )
{
	m_hTypeDictionary = pDict;
}


//-----------------------------------------------------------------------------
// Initialization of the tree 
//-----------------------------------------------------------------------------
void CElementPropertiesTreeInternal::Init( )
{
	if ( !m_hObject.Get() )
		return;

	UpdateTree();
}


//-----------------------------------------------------------------------------
// Applies changes to all attributes 
//-----------------------------------------------------------------------------
void CElementPropertiesTreeInternal::ApplyChanges()
{
	Assert( !m_bAutoApply );

	if ( !m_hObject.Get() )
		return;

	int nCount = m_AttributeWidgets.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		attributewidgetfactorylist->ApplyChanges( m_AttributeWidgets[i].m_pValueWidget, this );
	}
}


//-----------------------------------------------------------------------------
// Refreshes all attributes 
//-----------------------------------------------------------------------------
void CElementPropertiesTreeInternal::Refresh( RefreshType_t rebuild /* = false */, bool preservePrevSelectedItem /*= false*/ )
{
	if ( !m_hObject.Get() )
		return;

	if ( rebuild == REFRESH_REBUILD )
	{
		SetObject( m_hObject.Get() );
		return;
	}

	if ( rebuild != REFRESH_VALUES_ONLY )
	{
		RefreshTreeView( preservePrevSelectedItem );
	}
	else
	{
		RefreshTreeItemState( m_pTree->GetTree()->GetRootItemIndex() );
	}
	int nCount = m_AttributeWidgets.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		attributewidgetfactorylist->Refresh( m_AttributeWidgets[i].m_pValueWidget, this );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Start editing label, in place
// Input  :  - 
//-----------------------------------------------------------------------------
void CElementPropertiesTreeInternal::OnRename()
{
	if ( m_pTree->GetTree()->GetSelectedItemCount() != 1 )
		return;

	m_pTree->GetTree()->StartEditingLabel( m_pTree->GetTree()->GetFirstSelectedItem() );
}

void CElementPropertiesTreeInternal::OnCopy()
{
	CUtlVector< int > selected;
	m_pTree->GetTree()->GetSelectedItems( selected ) ;
	int c = selected.Count();
	if ( c <= 0 )
		return;

	// add in reverse order, since selection[0] is the last item selected
	CUtlVector< KeyValues * > list;
	for ( int i = c - 1; i >= 0; --i )
	{
		KeyValues *data = new KeyValues( "Clipboard" );
		m_pTree->GetTree()->GenerateDragDataForItem( selected[ i ], data );
		list.AddToTail( data );
	}	

	if ( list.Count() > 0 )
	{
		g_pDataModel->SetClipboardData( list );
	}
}

void CElementPropertiesTreeInternal::GetPathToItem( CUtlVector< TreeItem_t > &path, int itemIndex )
{
	for ( int idx = itemIndex; idx != m_pTree->GetTree()->GetRootItemIndex(); idx = m_pTree->GetTree()->GetItemParent( idx ) )
	{
		KeyValues *itemData = m_pTree->GetTree()->GetItemData( idx );
		bool isArrayElement = !itemData->IsEmpty( "arrayIndex" );

		TreeItem_t treeitem;
		treeitem.m_pElement = GetElementKeyValue< CDmElement >( itemData, "ownerelement" );
		treeitem.m_pAttributeName = itemData->GetString( "attributeName", "" );
		treeitem.m_pArrayElement = isArrayElement ? GetElementKeyValue< CDmElement >( itemData, "dmeelement" ) : NULL;
		path.AddToTail( treeitem );
	}
}

int CElementPropertiesTreeInternal::OpenPath( const CUtlVector< TreeItem_t > &path )
{
	bool bFound = false;

	int itemIndex = m_pTree->GetTree()->GetRootItemIndex();
	int nPathItems = path.Count();
	for ( int i = 0; i < nPathItems; ++i )
	{
		const TreeItem_t &childTreeItem = path[ i ];

		bFound = false;

		int nChildren = m_pTree->GetTree()->GetNumChildren( itemIndex );
		for ( int i = 0; i < nChildren; ++i )
		{
			int nChildIndex = m_pTree->GetTree()->GetChild( itemIndex, i );
			KeyValues *childData = m_pTree->GetTree()->GetItemData( nChildIndex );

			bool isArrayElement = !childData->IsEmpty( "arrayIndex" );
			CDmElement *pOwnerElement = GetElementKeyValue< CDmElement >( childData, "ownerelement" );
			const char *pAttributeName = childData->GetString( "attributeName", "" );
			CDmAttribute *pAttribute = pOwnerElement->GetAttribute( pAttributeName );

			if ( isArrayElement )
			{
				Assert( childTreeItem.m_pArrayElement );
				Assert( !V_strcmp( childTreeItem.m_pAttributeName, pAttributeName ) );
				int nArrayIndex = childData->GetInt( "arrayIndex", -1 );
				const CDmrElementArray<> array( pAttribute );
				if ( nArrayIndex >= 0 && array[ nArrayIndex ] == childTreeItem.m_pArrayElement )
				{
					bFound = true;
					itemIndex = nChildIndex;
					break;
				}
			}
			else
			{
				Assert( !childTreeItem.m_pArrayElement );
				if ( !V_strcmp( childTreeItem.m_pAttributeName, pAttributeName ) )
				{
					bFound = true;
					itemIndex = nChildIndex;
					break;
				}
			}
		}

		if ( !bFound )
			return -1;
	}

	return bFound ? itemIndex : -1;
}

void CElementPropertiesTreeInternal::OnPaste_( bool reference )
{
	CUtlVector< int > selected;
	m_pTree->GetTree()->GetSelectedItems( selected ) ;
	int c = selected.Count();
	if ( !c )
		return;

	// Just choose first item for now
	int itemIndex = selected[ 0 ];
	KeyValues *itemData = m_pTree->GetItemData( itemIndex );
	if ( !itemData )
		return;

	const char *elementType = itemData->GetString( "droppableelementtype" );
	if ( !elementType || !elementType[ 0 ] )
		return;

	bool isArrayElement = !itemData->IsEmpty( "arrayIndex" );

	//Check to see if this attribute refers to an element
	CDmAttribute *pAttribute = ElementTree_GetAttribute( itemData );
	if ( !pAttribute )
		return;

	DmAttributeType_t attType = pAttribute ? pAttribute->GetType() : AT_UNKNOWN;
	bool isElementAttribute = attType == AT_ELEMENT || attType == AT_ELEMENT_ARRAY;
	if ( !isElementAttribute )
		return;

	// get source data that will be pasted
	CUtlVector< KeyValues * > msglist;
	g_pDataModel->GetClipboardData( msglist );

	CUtlVector< CDmElement * > list;
	ElementTree_GetDroppableItems( msglist, "dmeelement", list );
	if ( !list.Count() )
		return;

	// Pasting after an element array item or at the end of an element array
	if ( isArrayElement || attType == AT_ELEMENT_ARRAY )
	{
		CElementTreeUndoScopeGuard guard( NOTIFY_SETDIRTYFLAG, m_pNotify, reference ? "Paste Reference" : "Paste" );

		CDmrElementArray<> array( pAttribute );
		int nArrayIndex = isArrayElement ? itemData->GetInt( "arrayIndex" ) + 1 : array.Count();
		DropItemsIntoArray( array, msglist, list, nArrayIndex, reference ? DO_LINK : DO_COPY );
	}
	// Pasting onto an element attribute
	else
	{
		CElementTreeUndoScopeGuard guard( NOTIFY_SETDIRTYFLAG, m_pNotify, reference ? "Paste Reference" : "Paste" );

		pAttribute->SetValue( reference ? list[ 0 ] : list[ 0 ]->Copy() );
	}

	CUtlVector< TreeItem_t > dropTargetPath;
	if ( isArrayElement )
	{
		itemIndex = m_pTree->GetTree()->GetItemParent( itemIndex ); // if we're an array element, start with the array itself
	}
	GetPathToItem( dropTargetPath, itemIndex );

	// Does a forced refresh
	Refresh( REFRESH_TREE_VIEW );

	itemIndex = OpenPath( dropTargetPath );
	if ( attType == AT_ELEMENT_ARRAY )
	{
		m_pTree->GetTree()->ExpandItem( itemIndex, true );
	}
}

void CElementPropertiesTreeInternal::OnPaste()
{
	Warning( "CElementPropertiesTreeInternal::OnPaste\n" );
	OnPaste_( false );
}

void CElementPropertiesTreeInternal::OnPasteReference()
{
	Warning( "CElementPropertiesTreeInternal::OnPasteReference\n" );
	OnPaste_( true );
}

void CElementPropertiesTreeInternal::OnPasteInsert()
{
	Warning( "CElementPropertiesTreeInternal::OnPasteInsert\n" );
}

void RemoveAllReferencesToElement( CDmElement *pElement )
{
	if ( pElement == NULL )
		return;

	for ( DmElementHandle_t hElement = g_pDataModel->FirstAllocatedElement();
		hElement != DMELEMENT_HANDLE_INVALID;
		hElement = g_pDataModel->NextAllocatedElement( hElement ) )
	{
		CDmElement *pElt = g_pDataModel->GetElement( hElement );
		if ( pElt )
		{
			pElt->RemoveAllReferencesToElement( pElement );
		}
	}
}

void CElementPropertiesTreeInternal::OnDeleteSelected()
{
	CUtlVector< KeyValues * > selection;
	m_pTree->GetTree()->GetSelectedItemData( selection );
	int nSelected = selection.Count();
	if ( !nSelected )
		return;

	bool bChangeOccurred = false;
	{
		CElementTreeUndoScopeGuard guard( NOTIFY_SETDIRTYFLAG, m_pNotify, "Delete Elements" );

		for ( int si = 0; si < nSelected; ++si )
		{
			KeyValues *item = selection[ si ];
			Assert( item );

			// Check to see if this attribute refers to an element
			CDmElement *pOwner = GetElementKeyValue<CDmElement>( item, "ownerelement" );
			if ( pOwner == NULL )
				continue;

			CDmAttribute *pAttr = pOwner->GetAttribute( item->GetString( "attributeName" ) );
			if ( pAttr == NULL )
				continue;

			bChangeOccurred = true;

			DmAttributeType_t attrType = pAttr->GetType();
			if ( attrType == AT_ELEMENT )
			{
				CDmElement *pElement = pAttr->GetValueElement<CDmElement>();
				RemoveAllReferencesToElement( pElement );
			}
			else if ( attrType == AT_ELEMENT_ARRAY )
			{
				const CDmrElementArray<> array( pAttr );
				int n = array.Count();
				int index = item->GetInt( "arrayIndex", -1 );
				if ( index >= 0 )
				{
					CDmElement *pElement = array[ index ];
					RemoveAllReferencesToElement( pElement );
				}
				else
				{
					for ( int i = 0; i < n; ++i )
					{
						CDmElement *pElement = array[ i ];
						RemoveAllReferencesToElement( pElement );
					}
				}
			}
		}
	}

	// Does a forced refresh
	if ( bChangeOccurred )
	{
		Refresh( REFRESH_TREE_VIEW );
	}
}

void CElementPropertiesTreeInternal::OnCut()
{
	OnCopy();
	OnRemove();
}

void CElementPropertiesTreeInternal::OnClear()
{
	bool bNeedRefresh = false;
	CUtlVector< KeyValues * > data;
	m_pTree->GetTree()->GetSelectedItemData( data );
	int c = data.Count();
	if ( !c )
		return;

	CElementTreeNotifyScopeGuard notify( "CElementPropertiesTreeInternal::OnClear", NOTIFY_SETDIRTYFLAG, m_pNotify );

	for ( int i = 0; i  < c; ++i )
	{
		KeyValues *item = data[ i ];
		Assert( item );

		//Check to see if this attribute refers to an element
		CDmElement *pOwner = GetElementKeyValue<CDmElement>( item, "ownerelement" );
		const char *pAttributeName = item->GetString( "attributeName" );

		if ( pOwner && pAttributeName[ 0 ] )
		{
			CDmAttribute *pAttribute = pOwner->GetAttribute( pAttributeName );
			DmAttributeType_t attType = pAttribute ? pAttribute->GetType( ) : AT_UNKNOWN;
			switch ( attType )
			{
			default:
				break;

			case AT_ELEMENT:
				{
					bNeedRefresh = true;
					CElementTreeUndoScopeGuard guard( NOTIFY_SETDIRTYFLAG, "Remove Element" );
					pAttribute->SetValue( DMELEMENT_HANDLE_INVALID );
				}
				break;

			case AT_ELEMENT_ARRAY:
				{
					bNeedRefresh = true;
					CElementTreeUndoScopeGuard guard( NOTIFY_SETDIRTYFLAG, "Remove Element" );
					CDmrGenericArray array( pAttribute );
					if ( array.IsValid() )
					{
						array.RemoveAll();
					}
				}
				break;
			}
		}
	}

	if ( bNeedRefresh )
	{
		// Does a forced refresh
		Refresh( REFRESH_TREE_VIEW );
	}
}

// For each owner/attribute have an entry and them for each arrayIndex into any array type, need to sort by arrayIndex so we can remove them in reverse order

static bool ArrayIndexLessFunc( KeyValues * const &lhs, KeyValues* const &rhs )
{
	bool arrayItem1 = !lhs->IsEmpty( "arrayIndex" ) ? true : false;
	int arrayIndex1 = lhs->GetInt( "arrayIndex" );
	bool arrayItem2 = !rhs->IsEmpty( "arrayIndex" ) ? true : false;
	int arrayIndex2 = rhs->GetInt( "arrayIndex" );

	if ( !arrayItem1 || !arrayItem2 )
		return lhs < rhs;

	return arrayIndex1 < arrayIndex2;
}

struct OwnerAttribute_t
{
	OwnerAttribute_t() : sortedData( 0, 0, ArrayIndexLessFunc )
	{
	}

	OwnerAttribute_t( const OwnerAttribute_t& src ) : sortedData( 0, 0, ArrayIndexLessFunc )
	{
		pOwner = src.pOwner;
		symAttribute = src.symAttribute;
		for ( int i = src.sortedData.FirstInorder(); i != src.sortedData.InvalidIndex(); i = src.sortedData.NextInorder( i ) )
		{
			sortedData.Insert( src.sortedData[ i ] );
		}
	}

	static bool LessFunc( const OwnerAttribute_t& lhs, const OwnerAttribute_t& rhs )
	{
		if ( lhs.pOwner != rhs.pOwner )
			return lhs.pOwner < rhs.pOwner;

		return Q_stricmp( lhs.symAttribute.String(), rhs.symAttribute.String() ) < 0;
	}

	CDmElement		*pOwner;
	CUtlSymbol		symAttribute;

	CUtlRBTree< KeyValues *, int >	sortedData;
};

class CSortedElementData
{
public:
	CSortedElementData() : m_Sorted( 0, 0, OwnerAttribute_t::LessFunc )
	{
	}

	void AddData( CDmElement *pOwner, const char *attribute, KeyValues *data )
	{
		OwnerAttribute_t search;
		search.pOwner = pOwner;
		search.symAttribute = attribute;

		int idx = m_Sorted.Find( search );
		if ( idx == m_Sorted.InvalidIndex() )
		{
			idx = m_Sorted.Insert( search );
		}

		OwnerAttribute_t *entry = &m_Sorted[ idx ];
		Assert( entry );

		entry->sortedData.Insert( data );
	}

	CUtlRBTree< OwnerAttribute_t, int > m_Sorted;
};

bool CElementPropertiesTreeInternal::OnRemoveFromData( KeyValues *item )
{
	Assert( item );

	bool arrayItem = !item->IsEmpty( "arrayIndex" );
	int arrayIndex = item->GetInt( "arrayIndex" );

	//Warning( "   item[ %i ] (array? %s)\n", arrayIndex, arrayItem ? "yes" : "no" );

	CDmElement *pOwner = GetElementKeyValue< CDmElement >( item, "ownerelement" );
	const char *pAttributeName = item->GetString( "attributeName" );
	if ( !pOwner || !pAttributeName[ 0 ] )
		return false;

	CDmAttribute *pAttribute = pOwner->GetAttribute( pAttributeName );
	DmAttributeType_t attType = pAttribute ? pAttribute->GetType( ) : AT_UNKNOWN;

	if ( arrayItem && IsArrayType( attType ) )
	{
		CDmrGenericArray array( pAttribute );
		if ( !array.IsValid() )
			return false;

		CElementTreeUndoScopeGuard guard( NOTIFY_SETDIRTYFLAG, "Remove Array element" );

		array.Remove( arrayIndex );
		return true;
	}

	if ( attType == AT_ELEMENT )
	{
		if ( pOwner->GetValue< DmElementHandle_t >( pAttributeName ) != DMELEMENT_HANDLE_INVALID )
		{
			// remove the referenced element from this attribute
			CElementTreeUndoScopeGuard guard( NOTIFY_SETDIRTYFLAG, "Remove Element" );
			pAttribute->SetValue( DMELEMENT_HANDLE_INVALID );
		}
		else if ( !pAttribute->IsFlagSet( FATTRIB_EXTERNAL ) && !pAttribute->IsFlagSet( FATTRIB_READONLY ) )
		{
			// remove the attribute
			CElementTreeUndoScopeGuard guard( NOTIFY_SETDIRTYFLAG, "Remove Attribute" );
			pOwner->RemoveAttribute( pAttributeName );
		}
		return true;
	}

	if ( attType == AT_ELEMENT_ARRAY )
	{
		CDmrGenericArray array( pOwner, pAttributeName );
		if ( array.IsValid() && array.Count() > 0 )
		{
			// remove the all the elements from the array
			CElementTreeUndoScopeGuard guard( NOTIFY_SETDIRTYFLAG, "Remove Element Array Items" );
			array.RemoveAll();
		}
		else if ( !pAttribute->IsFlagSet( FATTRIB_EXTERNAL ) && !pAttribute->IsFlagSet( FATTRIB_READONLY ) )
		{
			// remove the attribute
			CElementTreeUndoScopeGuard guard( NOTIFY_SETDIRTYFLAG, "Remove Attribute" );
			pOwner->RemoveAttribute( pAttributeName );
		}
		return true;
	}

	if ( !pAttribute->IsFlagSet( FATTRIB_EXTERNAL )
		&& !pAttribute->IsFlagSet( FATTRIB_TOPOLOGICAL )
		&& !pAttribute->IsFlagSet( FATTRIB_READONLY ) )
	{
		if ( attType >= AT_FIRST_ARRAY_TYPE )
		{
			CDmrGenericArray array( pOwner, pAttributeName );
			if ( array.IsValid() && array.Count() > 0 )
			{
				CElementTreeUndoScopeGuard guard( NOTIFY_SETDIRTYFLAG, "Remove Array Items" );
				array.RemoveAll();
			}
			else
			{
				CElementTreeUndoScopeGuard guard( NOTIFY_SETDIRTYFLAG, "Remove Attribute" );
				pOwner->RemoveAttribute( pAttributeName );
			}
		}
		else
		{
			CElementTreeUndoScopeGuard guard( NOTIFY_SETDIRTYFLAG, "Remove Attribute" );
			pOwner->RemoveAttribute( pAttributeName );
		}
		return true;
	}
	return false;
}

bool CElementPropertiesTreeInternal::OnRemoveFromData( CUtlVector< KeyValues * >& list )
{
	CSortedElementData sorted;
	int i;
	int c = list.Count();
	for ( i = 0 ; i < c; ++i )
	{
		KeyValues *item = list[ i ];
		//Check to see if this attribute refers to an element
		CDmElement *pOwner = GetElementKeyValue< CDmElement >( item, "ownerelement" );
		const char *pAttributeName = item->GetString( "attributeName" );
		if ( !pOwner || !pAttributeName[ 0 ] )
			continue;

		sorted.AddData( pOwner, pAttributeName, item );
	}

	bool bRefreshRequired = false;

	// Now walk the data in reverse order
	for ( i = sorted.m_Sorted.FirstInorder(); i != sorted.m_Sorted.InvalidIndex(); i = sorted.m_Sorted.NextInorder( i ) )
	{
		OwnerAttribute_t& entry = sorted.m_Sorted[ i ];

		// Walk it backward by array index...
		for ( int j = entry.sortedData.LastInorder(); j != entry.sortedData.InvalidIndex(); j = entry.sortedData.PrevInorder( j ) )
		{
			KeyValues *item = entry.sortedData[ j ];
			bRefreshRequired = OnRemoveFromData( item ) || bRefreshRequired;
		}
	}

	return bRefreshRequired;
}

void CElementPropertiesTreeInternal::OnRemove()
{
	CElementTreeNotifyScopeGuard notify( "CElementPropertiesTreeInternal::OnRemove", NOTIFY_SETDIRTYFLAG, m_pNotify );

	CUtlVector< KeyValues * > data;
	m_pTree->GetTree()->GetSelectedItemData( data );
    bool bRefreshNeeded = OnRemoveFromData( data );
	if ( bRefreshNeeded )
	{
		// Refresh the tree
		Refresh( REFRESH_TREE_VIEW );
	}
}


//-----------------------------------------------------------------------------
// Sorts by name
//-----------------------------------------------------------------------------
int ElementNameSortFunc( const void *arg1, const void *arg2 )
{
	CDmElement *pElement1 = *(CDmElement**)arg1;
	CDmElement *pElement2 = *(CDmElement**)arg2;

	const char *pName1 = pElement1 ? pElement1->GetName() : "";
	const char *pName2 = pElement2 ? pElement2->GetName() : "";

	return Q_stricmp( pName1, pName2 );
}

void CElementPropertiesTreeInternal::OnSortByName()
{
	CElementTreeUndoScopeGuard guard( NOTIFY_SETDIRTYFLAG, m_pNotify, "Sort Element Array Attribute" );

	CUtlVector< KeyValues * > list;
	m_pTree->GetTree()->GetSelectedItemData( list );

	int c = list.Count();

	bool bRefreshNeeded = false;
	for ( int i = 0 ; i < c; ++i )
	{
		KeyValues *item = list[ i ];

		//Check to see if this attribute refers to an element
		CDmElement *pOwner = GetElementKeyValue< CDmElement >( item, "ownerelement" );
		const char *pAttributeName = item->GetString( "attributeName" );

		CDmrElementArray<> elementArray( pOwner, pAttributeName );
		if ( !elementArray.IsValid() )
			continue;

		int nCount = elementArray.Count();
		if ( nCount == 0 )
			continue;

		bRefreshNeeded = true;
		CDmElement **pArray = ( CDmElement** )_alloca( nCount * sizeof( CDmElement* ) );
		for ( int i = 0; i < nCount; ++i )
		{
			pArray[i] = elementArray[i];
		}

		qsort( pArray, nCount, sizeof( CDmElement* ), ElementNameSortFunc );

		elementArray.RemoveAll();
		elementArray.AddMultipleToTail( nCount );

		for ( int i = 0; i < nCount; ++i )
		{
			elementArray.Set( i, pArray[i] );
		}
	}

	if ( bRefreshNeeded )
	{
		// Refresh the tree
		Refresh( REFRESH_TREE_VIEW );
	}
}

void CElementPropertiesTreeInternal::JumpToHistoryItem()
{
	if ( m_nCurrentHistoryPosition < 0 || m_nCurrentHistoryPosition >= m_hHistory.Count() )
	{
		m_nCurrentHistoryPosition = 0;
	}

	if ( !m_hHistory.Count() )
		return;

	CDmElement *element = m_hHistory[ m_nCurrentHistoryPosition ].Get();
	if ( !element )
		return;

    bool save = m_bSuppressHistoryUpdates;
	m_bSuppressHistoryUpdates = true;

	SetObject( element );

	m_bSuppressHistoryUpdates = save;

	// Does a forced refresh
	Refresh( REFRESH_TREE_VIEW );

	// Used by the Dme panel to refresh the combo boxes when we change objects
	KeyValues *kv = new KeyValues( "NotifyViewedElementChanged" );
	SetElementKeyValue( kv, "dmeelement", element );
	PostActionSignal( kv );
}

void CElementPropertiesTreeInternal::OnShowSearchResults()
{
	if ( !m_SearchResults.Count() )
		return;

	if ( !m_SearchResultsRoot.Get() )
		return;

	SetObject( m_SearchResultsRoot.Get() );

	// Used by the Dme panel to refresh the combo boxes when we change objects
	KeyValues *kv = new KeyValues( "NotifyViewedElementChanged" );
	SetElementKeyValue( kv, "dmeelement", m_SearchResultsRoot.Get() );
	PostActionSignal( kv );
}

void CElementPropertiesTreeInternal::OnNavBack( int item )
{
	int c = m_hHistory.Count();
	if ( c <= 1 )
		return;

	if ( item == -1 )
	{
		if ( m_nCurrentHistoryPosition >= c - 1 )
			return;

		item = 1;
	}

	m_nCurrentHistoryPosition += item;
	Assert( m_nCurrentHistoryPosition < c );

	JumpToHistoryItem();

	UpdateButtonState();
}

void CElementPropertiesTreeInternal::OnNavForward( int item )
{
	int c = m_hHistory.Count();
	if ( c <= 0 )
		return;

	if ( item == -1 )
	{
		if ( m_nCurrentHistoryPosition <= 0 )
			return;

		item = 0;
	}

	++item;

	m_nCurrentHistoryPosition -= item;
	Assert( m_nCurrentHistoryPosition >= 0 );

	JumpToHistoryItem();

	UpdateButtonState();
}

bool CElementPropertiesTreeInternal::BuildExpansionListToFindElement_R( 
	CUtlRBTree< CDmElement *, int >& visited,
	int depth, 
	SearchResult_t &sr, 
	CDmElement *owner, 
	CDmElement *element, 
	const char *attributeName, 
	int arrayIndex, 
	CUtlVector< int >& expandIndices
	)
{
	if ( !element )
		return true;

	if ( visited.Find( element ) != visited.InvalidIndex() )
		return true;

	visited.Insert( element );

	int nAttributes = element->AttributeCount();

	if ( element == sr.handle.Get() )
	{
		if ( sr.attributeName.Length() > 0 )
		{
			int idx = nAttributes - 1;
			for ( CDmAttribute *attribute = element->FirstAttribute(); attribute; attribute = attribute->NextAttribute(), --idx )
			{
				const char *attributeName = attribute->GetName();
				if ( !Q_stricmp( attributeName, sr.attributeName.Get() ) )
				{
					expandIndices.AddToTail( idx );
					break;
				}
			}
		}
		return false;
	}

	int idx = nAttributes - 1;
	for ( CDmAttribute *attribute = element->FirstAttribute(); attribute; attribute = attribute->NextAttribute(), --idx )
	{
		const char *attributeName = attribute->GetName();
		if ( attribute->GetType() == AT_ELEMENT )
		{
			if ( !BuildExpansionListToFindElement_R( visited, depth + 1, sr, element, attribute->GetValueElement<CDmElement>(), attributeName, -1, expandIndices ) )
			{
				expandIndices.AddToTail( idx );
				return false;
			}
		}
		else if ( attribute->GetType() == AT_ELEMENT_ARRAY )
		{
			// Walk child objects
			const CDmrElementArray<CDmElement> elementArray( attribute );
			int c = elementArray.Count();
			for ( int i = 0; i < c; ++i )
			{
				if ( !BuildExpansionListToFindElement_R( visited, depth + 1, sr, element, elementArray[ i ], attributeName, i, expandIndices ) )
				{
					expandIndices.AddToTail( i );
					expandIndices.AddToTail( idx );
					return false;
				}
			}
		}
	}

	return true;
}

static ConVar dme_properties_maxsearchresults( "dme_properties_maxsearchresults", "50", 0, "Max number of search results to track." );

void CElementPropertiesTreeInternal::FindMatchingElements_R( CUtlRBTree< CDmElement *, int >& visited, const char *searchstr, CDmElement *element, CUtlVector< SearchResult_t >& list )
{
	if ( list.Count() >= dme_properties_maxsearchresults.GetInt() )
		return;

	if ( !element )
		return;

	if ( visited.Find( element ) != visited.InvalidIndex() )
		return;

	visited.Insert( element );

	if ( Q_stristr( element->GetName(), searchstr ) )
	{
		CDmeHandle< CDmElement > h;
		h = element;

		SearchResult_t sr;
		sr.handle = h;
		sr.attributeName = "";

		if ( list.Find( sr ) == list.InvalidIndex() )
		{
			list.AddToTail( sr );
		}
	}

	for ( CDmAttribute *attribute = element->FirstAttribute(); attribute; attribute = attribute->NextAttribute() )
	{
		const char *attributeName = attribute->GetName();
		if ( Q_stristr( attributeName, searchstr ) )
		{
			CDmeHandle< CDmElement > h;
			h = element;

			SearchResult_t sr;
			sr.handle = h;
			sr.attributeName = attributeName;

			if ( list.Find( sr ) == list.InvalidIndex() )
			{
				list.AddToTail( sr );
			}
		}

		if ( attribute->GetType() == AT_ELEMENT )
		{
			FindMatchingElements_R( visited, searchstr, attribute->GetValueElement<CDmElement>(), list );
		}
		else if ( attribute->GetType() == AT_ELEMENT_ARRAY )
		{
			// Walk child objects
			const CDmrElementArray<CDmElement> elementArray( attribute );
			int c = elementArray.Count();
			for ( int i = 0; i < c; ++i )
			{
				FindMatchingElements_R( visited, searchstr, elementArray[ i ], list );
			}
		}
	}
}

void CElementPropertiesTreeInternal::OnNavigateSearchAgain( int direction )
{
	if ( m_SearchResults.Count() <= 0 )
	{
		surface()->PlaySound("common/warning.wav");
		return;
	}

	if ( direction < 0 )
	{
		direction = -1;
	}
	else if ( direction >= 0 )
	{
		direction = 1;
	}

	m_nCurrentSearchResult = m_nCurrentSearchResult + direction;

	if ( m_nCurrentSearchResult < 0 )
	{
		m_nCurrentSearchResult = 0;
		surface()->PlaySound("common/warning.wav");
	}
	else if ( m_nCurrentSearchResult >= m_SearchResults.Count() )
	{
		m_nCurrentSearchResult = m_SearchResults.Count() - 1;
		surface()->PlaySound("common/warning.wav");
	}

	NavigateToSearchResult();

	UpdateButtonState();
}

void CElementPropertiesTreeInternal::NavigateToSearchResult()
{
	if ( !m_SearchResults.Count() )
		return;

// 	SetObject( m_SearchResultsRoot.Get() );

	CUtlVector< int > expandIndices;
	CUtlRBTree< CDmElement *, int > visited( 0, 0, DefLessFunc( CDmElement * ) );

	BuildExpansionListToFindElement_R( 
		visited,
		0, 
		m_SearchResults[ m_nCurrentSearchResult ], 
		m_hObject.Get(), 
		m_hObject.Get(), 
		"name", 
		-1, 
		expandIndices );

	expandIndices.AddToTail( 0 );

	// Close the tree and re-create the root node only
	UpdateTree();

	// NOTE: Updating the tree could have changed the root item index
	int nIndex = m_pTree->GetTree()->GetRootItemIndex();
	int c = expandIndices.Count();
	for ( int i = c - 2; i >= 0 ; --i )
	{
		int idx = expandIndices[ i ];

		// Expand the item
		m_pTree->ExpandItem( nIndex, true );

#ifdef _DEBUG
		int children = m_pTree->GetTree()->GetNumChildren( nIndex );
		if ( idx >= children ) 
		{
			Assert( 0 );
			break;
		}
#endif
		int childIndex = m_pTree->GetTree()->GetChild( nIndex, idx );
		nIndex = childIndex;
	}

	m_pTree->ExpandItem( nIndex, true );

	// Add to selection, but don't request focus (3rd param)
	m_pTree->GetTree()->AddSelectedItem( nIndex, true, false );
	m_pTree->GetTree()->MakeItemVisible( nIndex );

	m_pTree->ResizeTreeToExpandedWidth();

	DevMsg( "Displaying search result %d of %d\n", m_nCurrentSearchResult + 1, m_SearchResults.Count() );
}

void CElementPropertiesTreeInternal::OnNavSearch( const char *text )
{
	Msg( "OnNavSearch(%s)\n", text);
	if ( !text || !*text )
	{
		UpdateButtonState();
		return;
	}

	bool changed = Q_stricmp( text, m_szSearchStr ) != 0 ? true : false;
	if ( changed )
	{
		m_SearchResults.RemoveAll();
		Q_strncpy( m_szSearchStr, text, sizeof( m_szSearchStr ) );
		m_nCurrentSearchResult = 0;

		CUtlRBTree< CDmElement *, int > visited( 0, 0, DefLessFunc( CDmElement * ) );

		FindMatchingElements_R( visited, m_szSearchStr, m_hObject.Get(), m_SearchResults );

		AddToSearchHistory( text );

		if ( m_SearchResultsRoot.Get() )
		{
			CDisableUndoScopeGuard guard;
			
			int c = m_SearchResults.Count();

			char sz[ 512 ];
			Q_snprintf( sz, sizeof( sz ), "Search Results [%d] for '%s'", c, m_szSearchStr );

			m_SearchResultsRoot->SetName( sz );

			CDmrElementArray<> array( m_SearchResultsRoot, "results" );
			if ( array.IsValid() )
			{
				array.RemoveAll();
				for ( int i = 0; i < c; ++i )
				{
					if ( m_SearchResults[ i ].handle.Get() )
					{
						array.AddToTail( m_SearchResults[ i ].handle.GetHandle() );
					}
				}
			}
		}
	}
	else
	{
		++m_nCurrentSearchResult;
	}

	if ( !m_SearchResults.Count() )
	{
		// Close the tree and re-create the root node only
		UpdateTree();

		int nIndex = m_pTree->GetTree()->GetRootItemIndex();
		m_pTree->ExpandItem( nIndex, true );

		m_pTree->ResizeTreeToExpandedWidth();

		UpdateButtonState();
		return;
	}

	m_nCurrentSearchResult = clamp( m_nCurrentSearchResult, 0, m_SearchResults.Count() - 1 );

	NavigateToSearchResult();

	UpdateButtonState();
}

int CElementPropertiesTreeInternal::GetHistoryMenuItemCount( int whichMenu )
{
	int c = m_hHistory.Count();
	if ( !c )
		return 0;

	if ( m_nCurrentHistoryPosition == -1 )
	{
		m_nCurrentHistoryPosition = 0;
	}

	switch ( whichMenu )
	{
	default:
		Assert( 0 );
		break;
	case DME_PROPERTIESTREE_MENU_BACKWARD:
		{
			return c - ( m_nCurrentHistoryPosition + 1 );
		}
		break;
	case DME_PROPERTIESTREE_MENU_FORWARD:
		{
			return m_nCurrentHistoryPosition;
		}
		break;
	case DME_PROPERTIESTREE_MENU_SEARCHHSITORY:
		{
			return m_SearchHistory.Count();
		}
		break;
	}

	return 0;
}

void CElementPropertiesTreeInternal::PopulateHistoryMenu( int whichMenu, Menu *menu )
{
	ValidateHistory();

	int c = m_hHistory.Count();

	if ( m_nCurrentHistoryPosition == -1 )
	{
		m_nCurrentHistoryPosition = 0;
	}

	menu->DeleteAllItems();
	switch ( whichMenu )
	{
	default:
		Assert( 0 );
		break;
	case DME_PROPERTIESTREE_MENU_BACKWARD:
		{
			for ( int i = m_nCurrentHistoryPosition + 1; i < c; ++i )
			{
				CDmElement *element = m_hHistory[ i ].Get();
				char sz[ 256 ];
				Q_snprintf( sz, sizeof( sz ), "%s < %s >", element->GetName(), element->GetTypeString() );
                menu->AddMenuItem( "backitem", sz, new KeyValues( "OnNavigateBack", "item", i ), this );
			}
		}
		break;
	case DME_PROPERTIESTREE_MENU_FORWARD:
		{
			for ( int i = 0 ; i < m_nCurrentHistoryPosition; ++i )
			{
				CDmElement *element = m_hHistory[ m_nCurrentHistoryPosition - i - 1 ].Get();
				char sz[ 256 ];
				Q_snprintf( sz, sizeof( sz ), "%s < %s >", element->GetName(), element->GetTypeString() );
                menu->AddMenuItem( "fwditem", sz, new KeyValues( "OnNavigateForward", "item", i ), this );
			}
		}
		break;
	case DME_PROPERTIESTREE_MENU_SEARCHHSITORY:
		{
			int c = m_SearchHistory.Count();
			for ( int i = 0; i < c; ++i )
			{
				CUtlString& str = m_SearchHistory[ i ];
                menu->AddMenuItem( "search", str.Get(), new KeyValues( "OnNavSearch", "text", str.Get() ), this );
			}
		}
		break;
	}
}

void CElementPropertiesTreeInternal::AddToSearchHistory( const char *str )
{
	CUtlString historyString;
	historyString = str;

	int c = m_SearchHistory.Count();
	for ( int i = c - 1; i >= 0; --i )
	{
		CUtlString& entry = m_SearchHistory[ i ];
		if ( entry == historyString )
		{
			m_SearchHistory.Remove( i );
			break;
		}
	}

	while ( m_SearchHistory.Count() >= DME_PROPERTIESTREE_MAXSEARCHHISTORYITEMS )
	{
		m_SearchHistory.Remove( m_SearchHistory.Count() - 1 );
	}

	// Newest item at head of list
	m_SearchHistory.AddToHead( historyString );
}

void CElementPropertiesTreeInternal::AddToHistory( CDmElement *element )
{
	if ( m_bSuppressHistoryUpdates )
		return;

	if ( !element )
		return;

	CDmeHandle< CDmElement > h;
	h = element;

	// Purge the forward list
	if ( m_nCurrentHistoryPosition > 0 )
	{
		m_hHistory.RemoveMultiple( 0, m_nCurrentHistoryPosition );
		m_nCurrentHistoryPosition = 0;
	}
	
	// Remove if it's already in the list
	m_hHistory.FindAndRemove( h );

	// Make sure there's room
	while ( m_hHistory.Count() >= DME_PROPERTIESTREE_MAXHISTORYITEMS )
	{
		m_hHistory.Remove( m_hHistory.Count() - 1 );
	}

	// Most recent is at head
	m_hHistory.AddToHead( h );

	ValidateHistory();

	UpdateButtonState();
}

void CElementPropertiesTreeInternal::ValidateHistory()
{
	int i;
	int c = m_hHistory.Count();
	for ( i = c - 1 ; i >= 0; --i )
	{
		if ( !m_hHistory[ i ].Get() )
		{
			m_hHistory.Remove( i );
			if ( i && i == m_nCurrentHistoryPosition )
			{
				--m_nCurrentHistoryPosition;
			}
		}
	}
}

void CElementPropertiesTreeInternal::SpewHistory()
{
	int i;
	int c = m_hHistory.Count();
	for ( i = 0 ; i < c; ++i )
	{
		CDmElement *element = m_hHistory[ i ].Get();
		Assert( element );
		if ( !element )
			continue;

		Msg( "%s:  [%02d] %s <%s>\n",
			( ( i < m_nCurrentHistoryPosition ) ? "Fwd" : ( i == m_nCurrentHistoryPosition ? "Current" : "Backward" ) ),
			i, 
			element->GetName(), 
			element->GetTypeString() );
	}
}


void CElementPropertiesTreeInternal::AddAttribute( const char *pAttributeName, KeyValues *pContext )
{
	if ( !pAttributeName || !pAttributeName[ 0 ] )
	{
		Warning( "Can't add attribute with an empty name\n" );
		return;
	}

	const char *pAttributeType = pContext->GetString( "attributeType" );
	CDmElement *pElement = GetElementKeyValue< CDmElement >( pContext, "element" );
	if ( !pAttributeType || !pAttributeType[0] || !pElement )
		return;

	DmAttributeType_t attributeType = g_pDataModel->GetAttributeTypeForName( pAttributeType );
	if ( attributeType == AT_UNKNOWN )
	{
		Warning( "Can't add attribute '%s' because type '%s' is not known\n", pAttributeName, pAttributeType );
		return;
	}

	// Make sure attribute name isn't taken already
	if ( pElement->HasAttribute( pAttributeName ) )
	{
		Warning( "Can't add attribute '%s', attribute with that name already exists\n", pAttributeName );
		return;
	}

	CElementTreeUndoScopeGuard guard( NOTIFY_SETDIRTYFLAG, m_pNotify, "Add Attribute" );
	CDmAttribute *pAttribute = pElement->AddAttribute( pAttributeName, attributeType );
	if ( pAttribute )
	{
		pAttribute->AddFlag( FATTRIB_USERDEFINED );
	}
	Refresh( REFRESH_TREE_VIEW );
}

void CElementPropertiesTreeInternal::SetElementAttribute( const char *pElementName, KeyValues *pContext )
{
	if ( !pElementName || !pElementName[ 0 ] )
	{
		Warning( "Can't set an element attribute with an unnamed element!\n" );
		return;
	}

	const char *pAttributeName = pContext->GetString( "attributeName" );
	const char *pElementType = pContext->GetString( "elementType" );
	CDmElement *pElement = GetElementKeyValue< CDmElement >( pContext, "element" );
	if ( !pElementType || !pElementType[0] || !pElement )
		return;

	bool bRefreshRequired = false;

	{
		CElementTreeUndoScopeGuard guard( NOTIFY_SETDIRTYFLAG, m_pNotify, "Set Element" );
		DmElementHandle_t newElement = g_pDataModel->CreateElement( pElementType, pElementName, pElement->GetFileId() );
		if ( newElement == DMELEMENT_HANDLE_INVALID )
			return;

		CDmAttribute *pAttribute = pElement->GetAttribute( pAttributeName );
		DmAttributeType_t type = pAttribute ? pAttribute->GetType() : AT_UNKNOWN;
		switch( type )
		{
		case AT_ELEMENT:
			pAttribute->SetValue( newElement );
			bRefreshRequired = true;
			break;

		case AT_ELEMENT_ARRAY:
			{
				CDmrElementArray<> array( pAttribute );
				if ( !array.IsValid() )
				{
					g_pDataModel->DestroyElement( newElement );
					return;
				}

				int idx = pContext->GetInt( "index", -1 );

				bRefreshRequired = true;
				if ( idx == -1 )
				{
					array.AddToTail( newElement );
				}
				else
				{
					array.SetHandle( idx, newElement );
				}
			}
			break;
		}
	}

	if ( bRefreshRequired )
	{
		Refresh( REFRESH_TREE_VIEW );
	}
}


//-----------------------------------------------------------------------------
// Called by the input dialog for add attribute + set element
//-----------------------------------------------------------------------------
void CElementPropertiesTreeInternal::OnInputCompleted( KeyValues *pParams )
{
	KeyValues *pDlg = pParams->FindKey( "OnAddAttribute", false );
	if ( pDlg )
	{
		const char *pAttributeName = pParams->GetString( "text" );
		AddAttribute( pAttributeName, pDlg );
		return;
	}

	pDlg = pParams->FindKey( "OnSetElement", false );
	if ( pDlg )
	{
		const char *pElementName = pParams->GetString( "text" );
		SetElementAttribute( pElementName, pDlg );
		return;
	}
}


//-----------------------------------------------------------------------------
// Forwards commands to parent
//-----------------------------------------------------------------------------
bool CElementPropertiesTreeInternal::ShowSetElementAttributeDialog( CDmElement *pOwner, 
	const char *pAttributeName, int nArrayIndex, const char *pElementType )
{
	if ( !pOwner || !pAttributeName || !pAttributeName[ 0 ] || !pElementType || !pElementType[ 0 ] )
		return false;

	static int elemNum = 0;
	char elemName[ 512 ];
	if ( elemNum++ == 0 )
	{
		Q_snprintf( elemName, sizeof( elemName ), "newElement" );
	}
	else
	{
		Q_snprintf( elemName, sizeof( elemName ), "newElement%i", elemNum );
	}

	KeyValues *kv = new KeyValues( "OnSetElement", "attributeName", pAttributeName );
	SetElementKeyValue( kv, "element", pOwner );
	kv->SetInt( "index", nArrayIndex );
	kv->SetString( "elementType", pElementType );

	InputDialog *pSetAttributeDialog = new InputDialog( this, "Set Element", "Element Name:", elemName );
	pSetAttributeDialog->SetSmallCaption( true );
	pSetAttributeDialog->SetDeleteSelfOnClose( true );
	pSetAttributeDialog->DoModal( kv );
	return true;
}


bool CElementPropertiesTreeInternal::ShowAddAttributeDialog( CDmElement *pElement, const char *pAttributeType )
{
	if ( !pElement || !pAttributeType || !pAttributeType[ 0 ] )
		return false;

	static int attrNum = 0;
	char attrName[ 512 ];
	if ( attrNum++ == 0 )
	{
		Q_snprintf( attrName, sizeof( attrName ), "newAttribute" );
	}
	else
	{
		Q_snprintf( attrName, sizeof( attrName ), "newAttribute%i", attrNum );
	}

	KeyValues *kv = new KeyValues( "OnAddAttribute", "attributeType", pAttributeType );
	SetElementKeyValue( kv, "element", pElement );

	InputDialog *pAddDialog = new InputDialog( this, "Add Attribute", "Attribute Name:", attrName );
	pAddDialog->SetSmallCaption( true );
	pAddDialog->SetDeleteSelfOnClose( true );
	pAddDialog->DoModal( kv );
	return true;
}

																   
//-----------------------------------------------------------------------------
// Forwards commands to parent
//-----------------------------------------------------------------------------
void CElementPropertiesTreeInternal::OnCommand( const char *cmd )
{
	CUtlVector< KeyValues * > data;
	m_pTree->GetTree()->GetSelectedItemData( data );
	if ( !data.Count()  )
		return;

	int c = data.Count();
	for ( int i = 0; i  < c; ++i )
	{
		KeyValues *item = data[ i ];
		Assert( item );

		// Check to see if this attribute refers to an element
		const char *pElementType = StringAfterPrefix( cmd, "element_" );
		if ( pElementType )
		{
			CDmElement *pOwner = GetElementKeyValue< CDmElement >( item, "ownerelement" );
			const char *pAttributeName = item->GetString( "attributeName" );
			bool arrayItem = !item->IsEmpty( "arrayIndex" );
			int arrayIndex = item->GetInt( "arrayIndex" );
			if ( ShowSetElementAttributeDialog( pOwner, pAttributeName, arrayItem ? arrayIndex : -1, pElementType ) )
				return;
			continue;
		}

		const char *pAttributeType = StringAfterPrefix( cmd, "attribute_" );
		if ( pAttributeType )
		{
			CDmElement *pElement = GetElementKeyValue< CDmElement >( item, "dmeelement" );
			if ( ShowAddAttributeDialog( pElement, pAttributeType ) )
				return;
			continue;
		}
	}

	if ( GetParent() )
	{
		GetParent()->OnCommand( cmd );
	}
}

void CElementPropertiesTreeInternal::OnShowMemoryUsage()
{
	m_bShowMemoryUsage = !m_bShowMemoryUsage;
	Refresh( REFRESH_TREE_VIEW, true );
}

void CElementPropertiesTreeInternal::OnAddItem()
{
	CUtlVector< KeyValues * > data;
	m_pTree->GetTree()->GetSelectedItemData( data );
	int c = data.Count();
	if ( c == 0 )
		return;

	bool bRefreshRequired = false;
	{
		CElementTreeUndoScopeGuard guard( NOTIFY_SETDIRTYFLAG, m_pNotify, "Add Item(s)" );

		for ( int i = 0; i  < c; ++i )
		{
			KeyValues *item = data[ i ];
			Assert( item );

			CDmElement *pOwner = GetElementKeyValue< CDmElement >( item, "ownerelement" );
			const char *pAttributeName = item->GetString( "attributeName" );
			CDmAttribute *pAttribute = pOwner->GetAttribute( pAttributeName );
			DmAttributeType_t attType = pAttribute ? pAttribute->GetType() : AT_UNKNOWN;

			if ( attType == AT_ELEMENT_ARRAY )
			{
				CDmrElementArray<> array( pAttribute );
				if ( !array.IsValid() )
					continue;

				CUtlSymbol typeSymbol = array.GetElementType();
				const char *pElementType = g_pDataModel->GetString( typeSymbol );
				const char *pElementTypeName = StringAfterPrefix( pElementType, "Dme" );
				if ( !pElementTypeName )
				{
					Warning( "CElementPropertiesTreeInternal::OnAddItem: Unknown Element Type %s\n", pElementType );
					continue;
				}

				// make up a unique name
				static int elementNum = 0;
				char elementName[ 256 ];
				if ( elementNum++ == 0 )
				{
					Q_snprintf( elementName, sizeof( elementName ), "new%s", pElementTypeName );
				}
				else
				{
					Q_snprintf( elementName, sizeof( elementName ), "new%s%i", pElementTypeName, elementNum );
				}

				DmElementHandle_t newElement = g_pDataModel->CreateElement( pElementType, elementName, pOwner->GetFileId() );
				if ( newElement != DMELEMENT_HANDLE_INVALID )
				{
					array.AddToTail( newElement );
					bRefreshRequired = true;
				}
				continue;
			}

			if ( attType >= AT_FIRST_ARRAY_TYPE )
			{
				CDmrGenericArray arrayAttr( pAttribute );
				if ( arrayAttr.IsValid() )
				{
					arrayAttr.AddToTail();
					bRefreshRequired = true;
				}
				continue;
			}
		}

		if ( !bRefreshRequired )
		{
			guard.Abort();
		}
	}

	if ( bRefreshRequired )
	{
		// Does a forced refresh
		Refresh( REFRESH_TREE_VIEW );
	}
}

void CElementPropertiesTreeInternal::OnSetShared( KeyValues *params )
{
	bool bShared = params->GetInt( "shared" ) != 0;

	CElementTreeUndoScopeGuard guard( NOTIFY_SETDIRTYFLAG, m_pNotify, bShared ? "Mark Shared" : "Mark Not Shared" );

	CUtlVector< KeyValues* > selected;
	m_pTree->GetTree()->GetSelectedItemData( selected );
	int nSelected = selected.Count();
	for ( int i = 0; i < nSelected; ++i )
	{
		KeyValues *kv = selected[ i ];
		CDmElement *pElement = GetElementKeyValue<CDmElement>( kv, "dmeelement" );

		// element attribute or element array item
		if ( pElement )
		{
			pElement->SetShared( bShared );
			continue;
		}

		CDmElement *pOwner = GetElementKeyValue< CDmElement >( kv, "ownerelement" );
		const char *pAttributeName = kv->GetString( "attributeName" );

		const CDmrElementArray<> array( pOwner, pAttributeName );
		if ( !array.IsValid() )
			continue; // value attribute, value array item, or value array

		// element array attribute
		int nCount = array.Count();
		for ( int j = 0; j < nCount; ++j )
		{
			CDmElement *pElement = array[ j ];
			if ( !pElement )
				continue;

			pElement->SetShared( bShared );
		}
	}

	Refresh( REFRESH_TREE_VIEW, true );
}

void CElementPropertiesTreeInternal::OnChangeFile( KeyValues *params )
{
	const char *pFileName = params->GetString( "filename" );
	DmFileId_t fileid = g_pDataModel->GetFileId( pFileName );

	CUtlVector< KeyValues * > data;
	m_pTree->GetTree()->GetSelectedItemData( data );
	int nSelected = data.Count();
	if ( !nSelected )
		return;

	CElementTreeNotifyScopeGuard notify( "CElementPropertiesTreeInternal::OnChangeFile", NOTIFY_SETDIRTYFLAG, m_pNotify );

	bool bRefreshRequired = false;
	for ( int i = 0; i < nSelected; ++i )
	{
		KeyValues *item = data[ i ];
		Assert( item );

		//Check to see if this attribute refers to an element
		CDmElement *pElement = GetElementKeyValue< CDmElement >( item, "dmeelement" );
		if ( !pElement )
			continue;

		if ( fileid == DMFILEID_INVALID )
		{
			fileid = g_pDataModel->FindOrCreateFileId( pElement->GetName() );
			g_pDataModel->SetFileRoot( fileid, pElement->GetHandle() );
		}

		pElement->SetFileId( fileid, TD_DEEP );
		bRefreshRequired = true;
	}

	if ( bRefreshRequired )
	{
		Refresh( REFRESH_REBUILD );
	}
}

void CElementPropertiesTreeInternal::OnShowFileDialog( KeyValues *params )
{
	const char *pTitle = params->GetString( "title" );
	bool bOpenOnly = params->GetInt( "openOnly" ) != 0;
	KeyValues *pContext = params->FindKey( "context" );
	FileOpenDialog *pDialog = new FileOpenDialog( this, pTitle, bOpenOnly, pContext->MakeCopy() );

	char pStartingDir[ MAX_PATH ];
	GetModSubdirectory( NULL, pStartingDir, sizeof( pStartingDir ) );
	Q_StripTrailingSlash( pStartingDir );

	pDialog->SetStartDirectoryContext( pTitle, pStartingDir );
	pDialog->AddFilter( "*.*", "All Files (*.*)", false );
	pDialog->AddFilter( "*.dmx", "Generic MovieObjects File (*.dmx)", true, "movieobjects" ); // read/write generic movieobjects files

	pDialog->SetDeleteSelfOnClose( true );
	pDialog->AddActionSignalTarget( this );
	pDialog->DoModal( true );
}

void CElementPropertiesTreeInternal::OnImportElement( const char *pFullPath, KeyValues *pContext )
{
	CDmElement *pRoot = NULL;
	DmFileId_t tempFileid;
	{
		CDisableUndoScopeGuard guard;
		tempFileid = g_pDataModel->RestoreFromFile( pFullPath, NULL, NULL, &pRoot, CR_FORCE_COPY );
	}
	if ( !pRoot )
		return;

	CDmElement *pParent = GetElementKeyValue<CDmElement>( pContext, "owner" );

	pRoot->SetFileId( pParent->GetFileId(), TD_DEEP, true );
	g_pDataModel->RemoveFileId( tempFileid );

	{
		CElementTreeUndoScopeGuard guard( NOTIFY_SETDIRTYFLAG, m_pNotify, "Import Element" );

		const char *pAttributeName = pContext->GetString( "attribute" );
		int nArrayIndex = pContext->GetInt( "index", -1 );
		DmElementHandle_t hRoot = pRoot->GetHandle();
		if ( nArrayIndex >= 0 )
		{
			CDmrElementArray<> elemArrayAttr( pParent, pAttributeName );
			elemArrayAttr.SetHandle( nArrayIndex, hRoot );
		}
		else
		{
			CDmAttribute *pAttribute = pParent->GetAttribute( pAttributeName );
			if ( pAttribute->GetType() == AT_ELEMENT )
			{
				pAttribute->SetValue( hRoot );
			}
			else if ( pAttribute->GetType() == AT_ELEMENT_ARRAY )
			{
				CDmrElementArray<> elemArrayAttr( pAttribute );
				elemArrayAttr.AddToTail( hRoot );
			}
		}
	}

	Refresh( REFRESH_TREE_VIEW );
}

void CElementPropertiesTreeInternal::OnExportElement( const char *pFullPath, KeyValues *pContext )
{
	CDmElement *pRoot = NULL;

	CUtlVector< KeyValues * > selection;
	m_pTree->GetTree()->GetSelectedItemData( selection );
	int nSelected = selection.Count();
	if ( nSelected <= 1 )
	{
		pRoot = GetElementKeyValue<CDmElement>( pContext, "element" );
	}
	else
	{
		// HACK - this is just a temporary hack - we should really force serialization to traverse past fileid changes in this case
		KeyValues *item = selection[ 0 ];
		CDmElement *pOwner = GetElementKeyValue< CDmElement >( item, "ownerelement" );
		DmFileId_t fileid = pOwner->GetFileId();

		pRoot = CreateElement< CDmElement >( pFullPath, fileid );
		CDmrElementArray<> children( pRoot, "children", true );

		for ( int si = 0; si < nSelected; ++si )
		{
			KeyValues *item = selection[ si ];
			Assert( item );

			//Check to see if this attribute refers to an element
			CDmElement *pOwner = GetElementKeyValue< CDmElement >( item, "ownerelement" );
			if ( pOwner == NULL )
				continue;

			CDmAttribute *pAttr = pOwner->GetAttribute( item->GetString( "attributeName" ) );
			if ( pAttr == NULL )
				continue;

			DmAttributeType_t attrType = pAttr->GetType();
			if ( attrType == AT_ELEMENT )
			{
				children.AddToTail( pAttr->GetValue< DmElementHandle_t >() );
			}
			else if ( attrType == AT_ELEMENT_ARRAY )
			{
				const CDmrElementArray<> arrayAttr( pAttr );
				int n = arrayAttr.Count();
				int index = item->GetInt( "arrayIndex", -1 );
				if ( index >= 0 )
				{
					children.AddToTail( arrayAttr[ index ] );
				}
				else
				{
					for ( int i = 0; i < n; ++i )
					{
						children.AddToTail( arrayAttr[ i ] );
					}
				}
			}
		}
	}

	// if this control is ever moved to vgui_controls, change the default format to "dmx", the generic dmx format
	const char *pFileFormat = "movieobjects";
	const char *pFileEncoding = g_pDataModel->GetDefaultEncoding( pFileFormat );
	g_pDataModel->SaveToFile( pFullPath, NULL, pFileEncoding, pFileFormat, pRoot );

	if ( nSelected > 1 )
	{
		DestroyElement( pRoot );
	}
}

void CElementPropertiesTreeInternal::OnFileSelected( KeyValues *params )
{
	const char *pFullPath = params->GetString( "fullpath" );
	KeyValues *pContext = params->FindKey( "context" );
	const char *pCommand = pContext->GetString( "command" );
	if ( V_strcmp( pCommand, "OnImportElement" ) == 0 )
	{
		OnImportElement( pFullPath, pContext );
	}
	else if ( V_strcmp( pCommand, "OnExportElement" ) == 0 )
	{
		OnExportElement( pFullPath, pContext );
	}
	else
	{
		Assert( 0 );
	}
}


//-----------------------------------------------------------------------------
// Creates an attribute data widget using a specifically requested widget
//-----------------------------------------------------------------------------
vgui::Panel *CElementPropertiesTreeInternal::CreateAttributeDataWidget( CDmElement *pElement,
	const char *pWidgetName, CDmElement *obj, CDmAttribute *pAttribute, int nArrayIndex )
{
	AttributeWidgetInfo_t info;
	SetupWidgetInfo( &info, pElement, pAttribute, nArrayIndex );
	IAttributeWidgetFactory *pFactory = attributewidgetfactorylist->GetWidgetFactory( pWidgetName );
	if ( !pFactory )
		return NULL;
	return pFactory->Create( NULL, info );
}


// ------------------------------------------------------------------------------

void CElementPropertiesTreeInternal::UpdateTree()
{
	m_pTree->RemoveAll();
	if ( m_hObject.Get() )
	{
		m_AttributeWidgets.RemoveAll();

		char label[ 256 ];
		Q_snprintf( label, sizeof( label ), "%s", m_hObject->GetValueString( "name" ) );
		bool editableLabel = true;

		KeyValues *kv = new KeyValues( "item" );
		kv->SetString( "Text", label );
		kv->SetInt( "Expand", 1 );
		kv->SetInt( "dmeelement", m_hObject.Get() ? m_hObject.Get()->GetHandle() : DMELEMENT_HANDLE_INVALID );
		kv->SetInt( "ownerelement", m_hObject.Get() ? m_hObject.Get()->GetHandle() : DMELEMENT_HANDLE_INVALID );
		kv->SetString( "attributeName", "name" );
 		kv->SetInt( "root", m_hObject.Get() ? m_hObject.Get()->GetHandle() : DMELEMENT_HANDLE_INVALID);
		kv->SetInt( "editablelabel", editableLabel ? 1 : 0 );

		CDmElement *pElement = m_hObject.Get();
		vgui::Panel *widget = CreateAttributeDataWidget( pElement, "element", pElement, NULL );

		CUtlVector< Panel * >	columns;
		columns.AddToTail( NULL );
		columns.AddToTail( widget );
		int rootIndex = m_pTree->AddItem( kv, editableLabel, -1, columns );

		m_pTree->GetTree()->SetItemFgColor( rootIndex, Color( 66, 196, 66, 255 ) );
		m_pTree->GetTree()->SetItemSelectionUnfocusedBgColor( rootIndex, Color( 255, 153, 35, 255 ) );

		kv->deleteThis();

		// open up the root item (for now)
		m_pTree->ExpandItem(rootIndex, true);

		if ( m_SearchResultsRoot.Get() == m_hObject.Get() )
		{
			// Expand "results" too
			TreeItem_t item;

			item.m_pArrayElement = NULL;
			item.m_pElement = m_SearchResultsRoot.Get();
			item.m_pAttributeName = "results";

			// Look for a match
			int nChildIndex = FindTreeItem( rootIndex, item );
			if ( nChildIndex >= 0 )
			{
				m_pTree->ExpandItem( nChildIndex, true );
			}
		}
	}
	m_pTree->InvalidateLayout();
}

void CElementPropertiesTreeInternal::GenerateDragDataForItem( int itemIndex, KeyValues *msg )
{
	KeyValues *data = m_pTree->GetItemData( itemIndex );
	if ( !data || !msg )
	{
		return;
	}

	msg->SetInt( "dmeelement", data->GetInt( "dmeelement" ) );
	msg->SetInt( "ownerelement", data->GetInt( "ownerelement" ) );
	msg->SetString( "attributeName", data->GetString( "attributeName" ) );
	msg->SetInt( "arrayIndex", data->GetInt( "arrayIndex" ) );
	
	msg->SetString( "text", data->GetString( "Text" ) );
}

struct DataModelFilenameArray
{
	int Count() const
	{
		return g_pDataModel->NumFileIds();
	}
	const char *operator[]( int i ) const
	{
		return g_pDataModel->GetFileName( g_pDataModel->GetFileId( i ) );
	}
};

void CElementPropertiesTreeInternal::GenerateContextMenu( int itemIndex, int x, int y )
{
	KeyValues *data = m_pTree->GetItemData( itemIndex );
	if ( !data )
	{
		Assert( data );
		return;
	}

	if ( m_hContextMenu.Get() )
	{
		delete m_hContextMenu.Get();
		m_hContextMenu = NULL;
	}

	m_hContextMenu = new Menu( this, "ActionMenu" );
	m_hContextMenu->SetFont( m_pTree->GetTree()->GetFont() );
	Menu::PlaceContextMenu( this, m_hContextMenu.Get() );
	int id;

	// ----------------------------------------------------
	// What have we clicked on?

	// inspect the data
	CDmElement *pElement = GetElementKeyValue< CDmElement >( data, "dmeelement" );
	CDmElement *pOwner = GetElementKeyValue< CDmElement >( data, "ownerelement" );
	const char *pAttributeName = data->GetString( "attributeName" );
	int nArrayIndex = data->GetInt( "arrayIndex", -1 );

	// get the type
	CDmAttribute *pAttribute = pOwner->GetAttribute( pAttributeName );
	DmAttributeType_t attributeType = pAttribute->GetType();

	// figure out the context
	CDmrGenericArray array( pAttribute );

	bool bIsAttribute = data->IsEmpty( "arrayIndex" );
	bool bIsArrayItem = !bIsAttribute;
	bool bIsArrayAttribute = !bIsArrayItem && ( attributeType >= AT_FIRST_ARRAY_TYPE );
	bool bIsArrayAttributeEmpty = bIsArrayAttribute && ( array.Count() == 0 );
	bool bIsElementAttribute = bIsAttribute && ( attributeType == AT_ELEMENT );
	bool bIsElementArrayAttribute = bIsArrayAttribute && ( attributeType == AT_ELEMENT_ARRAY );
	bool bIsElementArrayItem = bIsArrayItem && ( attributeType == AT_ELEMENT_ARRAY );
	bool bIsElementAttributeNull = bIsElementAttribute && ( pElement == NULL );

	// ----------------------------------------------------
	// menu title == what's my context? ( 3 x 2 )
	//    3: Item | Array | Attribute
	//    2: Element | Not Element

	if ( bIsElementArrayItem )
	{
		m_hContextMenu->AddCheckableMenuItem( "* Element Item Operations *", this );
	}
	else if ( bIsElementArrayAttribute )
	{
		m_hContextMenu->AddCheckableMenuItem( "* Element Array Operations *", this );
	}
	else if ( bIsElementAttribute )
	{
		m_hContextMenu->AddCheckableMenuItem( "* Element Attribute Operations *", this );
	}
	else if ( bIsArrayItem )
	{
		m_hContextMenu->AddCheckableMenuItem( "* Item Operations *", this );
	}
	else if ( bIsArrayAttribute )
	{
		m_hContextMenu->AddCheckableMenuItem( "* Array Operations *", this );
	}
	else if ( bIsAttribute )
	{
		m_hContextMenu->AddCheckableMenuItem( "* Attribute Operations *", this );
	}

	m_hContextMenu->AddSeparator();

	// ----------------------------------------------------
	// basic ops:

	// cut / copy / paste
	m_hContextMenu->AddMenuItem( "#DmeElementPropertiesCut", new KeyValues( "OnCut" ), this );
	m_hContextMenu->AddMenuItem( "#DmeElementPropertiesCopy", new KeyValues( "OnCopy" ), this );
	id = m_hContextMenu->AddMenuItem( "#DmeElementPropertiesPaste", new KeyValues( "OnPaste" ), this );
	m_hContextMenu->SetItemEnabled( id, vgui::system()->GetClipboardTextCount() > 0 );

	// paste special
	// Would have to get the clipboard contents and examine to enable a cascading "Paste Special" menu here
	Menu *pasteSpecial = new Menu( this, "Paste Special" );
	pasteSpecial->SetFont( m_pTree->GetTree()->GetFont() );
	id = m_hContextMenu->AddCascadingMenuItem( "#DmeElementPropertiesPasteSpecial", this, pasteSpecial );
	m_hContextMenu->SetItemEnabled( id, vgui::system()->GetClipboardTextCount() > 0 );
	id = pasteSpecial->AddMenuItem( "Nothing Special", this );
	pasteSpecial->SetItemEnabled( id, false );

	// clear or remove
	int removeItemID;
	if ( bIsArrayAttribute && !bIsArrayAttributeEmpty )
	{
		removeItemID = m_hContextMenu->AddMenuItem( "#DmeElementPropertiesClear", new KeyValues( "OnRemove" ), this );
	}
	else
	{
		removeItemID = m_hContextMenu->AddMenuItem( "#DmeElementPropertiesRemove", new KeyValues( "OnRemove" ), this );
	}

	// ----------------------------------------------------
	// other ops

	// Rename...
	if ( data->GetInt( "editablelabel" ) )
	{
		if ( bIsArrayItem )
		{
			m_hContextMenu->AddMenuItem( "Rename Element...", new KeyValues( "OnRename" ), this );
		}
		else
		{
			m_hContextMenu->AddMenuItem( "Rename Attribute...", new KeyValues( "OnRename" ), this );
		}
	}

	// sort by name
	if ( bIsElementArrayAttribute && !bIsArrayAttributeEmpty )
	{
		m_hContextMenu->AddMenuItem( "#DmeElementPropertiesSortByName", new KeyValues( "OnSortByName" ), this );
	}

	// ----------------------------------------------------
	// Add item/attr/elem ops:

	// Add Item
	if ( bIsArrayAttribute && !bIsElementArrayAttribute )
	{
		m_hContextMenu->AddMenuItem( "#DmeElementPropertiesAddItem", new KeyValues( "OnAddItem" ), this );
	}

	// Add Attribute
 	if ( ( bIsElementAttribute && !bIsElementAttributeNull ) || m_pTree->GetTree()->GetRootItemIndex() == itemIndex || bIsElementArrayItem )
	{
		Menu *addMenu = new Menu( this, "AddAttribute" );
		addMenu->SetFont( m_pTree->GetTree()->GetFont() );
		m_hContextMenu->AddCascadingMenuItem( "#DmeElementPropertiesAddAttribute", this, addMenu );
		{
			for ( int i = AT_FIRST_VALUE_TYPE; i < AT_TYPE_COUNT; ++i )
			{
				const char *typeName = g_pDataModel->GetAttributeNameForType( (DmAttributeType_t)i );
				if ( typeName && typeName[ 0 ] )
				{
					char add_attribute[ 256 ];
					Q_snprintf( add_attribute, sizeof( add_attribute ), "attribute_%s", typeName );
					id = addMenu->AddMenuItem( typeName, new KeyValues( "Command", "command", add_attribute ), this );
					addMenu->GetMenuItem( id )->SetContentAlignment( Label::a_center );
				}
			}
		}

	}

	// New, Add or Replace Element
	if ( bIsElementAttribute || bIsElementArrayAttribute || bIsElementArrayItem )
	{
		Menu *addMenu = new Menu( this, "SetElement" );
		addMenu->SetFont( m_pTree->GetTree()->GetFont() );

		if ( bIsElementArrayAttribute )
		{
			m_hContextMenu->AddCascadingMenuItem( "#DmeElementPropertiesAddElement", this, addMenu );
		}
		else if ( bIsElementAttributeNull )
		{
			m_hContextMenu->AddCascadingMenuItem( "#DmeElementPropertiesNewElement", this, addMenu );
		}
		else if ( bIsElementAttribute || bIsElementArrayItem )
		{
			m_hContextMenu->AddCascadingMenuItem( "#DmeElementPropertiesReplaceElement", this, addMenu );
		}

		// Populate from factories
		for ( int i = g_pDataModel->GetFirstFactory(); g_pDataModel->IsValidFactory( i ); i = g_pDataModel->GetNextFactory( i ) )
		{
			const char *elementType = g_pDataModel->GetFactoryName( i );
			Assert( elementType && elementType[ 0 ] );

			char add_element[ 256 ];
			Q_snprintf( add_element, sizeof( add_element ), "element_%s", elementType );
			id = addMenu->AddMenuItem( elementType, new KeyValues( "Command", "command", add_element ), this );
			addMenu->GetMenuItem( id )->SetContentAlignment( Label::a_center );
		}
	}

	// sharing
	if ( ( bIsElementAttribute && !bIsElementAttributeNull ) || m_pTree->GetTree()->GetRootItemIndex() == itemIndex || bIsElementArrayAttribute || bIsElementArrayItem )
	{
		CUtlVector< KeyValues* > selected;
		m_pTree->GetTree()->GetSelectedItemData( selected );
		int nElements = 0;
		int nShared = 0;
		int nSelected = selected.Count();
		for ( int i = 0; i < nSelected; ++i )
		{
			KeyValues *kv = selected[ i ];
			CDmElement *pElement = GetElementKeyValue<CDmElement>( kv, "dmeelement" );

			// element attribute or element array item
			if ( pElement )
			{
				++nElements;
				if ( pElement->IsShared() )
				{
					++nShared;
				}
				continue;
			}

			CDmElement *pOwner = GetElementKeyValue< CDmElement >( kv, "ownerelement" );
			const char *pAttributeName = kv->GetString( "attributeName" );

			const CDmrElementArray<> array( pOwner, pAttributeName );
			if ( !array.IsValid() )
				continue; // value attribute, value array item, or value array

			// element array attribute
			int nCount = array.Count();
			for ( int j = 0; j < nCount; ++j )
			{
				CDmElement *pElement = array[ j ];
				if ( !pElement )
					continue;

				++nElements;
				if ( pElement->IsShared() )
				{
					++nShared;
				}
			}
		}

		if ( nShared < nElements )
		{
			m_hContextMenu->AddMenuItem( "Mark Shared", new KeyValues( "OnSetShared", "shared", 1 ), this );
		}
		if ( nShared > 0 )
		{
			m_hContextMenu->AddMenuItem( "Mark Not Shared", new KeyValues( "OnSetShared", "shared", 0 ), this );
		}
	}

	// import element
	if ( bIsElementAttribute || bIsElementArrayAttribute || bIsElementArrayItem )
	{
		KeyValues *pContext = new KeyValues( "context", "command", "OnImportElement" );
		pContext->SetInt( "owner", ( int )pOwner->GetHandle() );
		pContext->SetString( "attribute", pAttributeName );
		pContext->SetInt( "index", nArrayIndex );

		KeyValues *kv = new KeyValues( "OnShowFileDialog", "title", "Import Element" );
		kv->SetInt( "openOnly", 1 );
		kv->AddSubKey( pContext );
		m_hContextMenu->AddMenuItem( "Import element...", kv, this );
	}

	// export element
	if ( ( bIsElementAttribute && !bIsElementAttributeNull ) || m_pTree->GetTree()->GetRootItemIndex() == itemIndex || bIsElementArrayItem )
	{
		KeyValues *pContext = new KeyValues( "context", "command", "OnExportElement" );
		pContext->SetInt( "element", ( int )pElement->GetHandle() );

		KeyValues *kv = new KeyValues( "OnShowFileDialog", "title", "Export Element" );
		kv->SetInt( "openOnly", 0 );
		kv->AddSubKey( pContext );
		m_hContextMenu->AddMenuItem( "Export element...", kv, this );
	}

	if ( pElement )
	{
		Menu *menu = new Menu( this, "ChangeFile" );
		menu->SetFont( m_pTree->GetTree()->GetFont() );

		m_hContextMenu->AddCascadingMenuItem( "#DmeElementPropertiesChangeFileAssociation", this, menu );

		int nFiles = g_pDataModel->NumFileIds();
		for ( int i = 0; i < nFiles; ++i )
		{
			DmFileId_t fileid = g_pDataModel->GetFileId( i );
			const char *pFileName = g_pDataModel->GetFileName( fileid );

			if ( !pFileName || !*pFileName )
				continue; // skip invalid and default fileids

			char cmd[ 256 ];
			Q_snprintf( cmd, sizeof( cmd ), "element_changefile %s", pFileName );

			const char *pText = pFileName;
			char text[ 256 ];
			if ( pElement->GetFileId() == fileid )
			{
				Q_snprintf( text, sizeof( text ), "* %s", pFileName );
				pText = text;
			}

			menu->AddMenuItem( pText, new KeyValues( "OnChangeFile", "filename", pFileName ), this );
		}

		char filename[ MAX_PATH ];
		V_GenerateUniqueName( filename, sizeof( filename ), "unnamed", DataModelFilenameArray() );

		menu->AddMenuItem( "<new file>", new KeyValues( "OnChangeFile", "filename", filename ), this );
	}

	// ----------------------------------------------------
	// finally add a seperator after the "Remove" item, unless it's the last item

	if ( ( m_hContextMenu->GetItemCount() - 1 ) != removeItemID )
	{
		m_hContextMenu->AddSeparatorAfterItem( removeItemID );
	}

	// ----------------------------------------------------
}

void CElementPropertiesTreeInternal::GenerateChildrenOfNode( int itemIndex )
{
	KeyValues *data = m_pTree->GetItemData( itemIndex );
	if ( !data )
	{
		Assert( data );
		return;
	}

	//Check to see if this attribute refers to an element
	CDmElement *obj = GetElementKeyValue<CDmElement>( data, "dmeelement" );
	if ( obj )
	{
		InsertAttributes( itemIndex, obj );
		return;
	}

	// Check to see if this node is an array entry, and then do nothing
	if ( !data->IsEmpty( "arrayIndex" ) )
		return;

	// Check to see if this attribute is an array attribute
	CDmElement *pOwner = GetElementKeyValue< CDmElement >( data, "ownerelement" );
	if ( pOwner )
	{
		const char *pAttributeName = data->GetString( "attributeName" );
		CDmAttribute *pAttribute = pOwner->GetAttribute( pAttributeName ); 
		if ( pAttribute && IsArrayType( pAttribute->GetType() ) )
		{
			InsertAttributeArrayMembers( itemIndex, pOwner, pAttribute );
			return;
		}
	}
}

void CElementPropertiesTreeInternal::OnLabelChanged( int itemIndex, const char *oldString, const char *newString )
{
	KeyValues *data = m_pTree->GetItemData( itemIndex );
	if ( !data )
	{
		Assert( data );
		return;
	}

	// No change!!!
	if ( !Q_stricmp( oldString, newString ) )
		return;

	CDmElement *pElement = GetElementKeyValue< CDmElement >( data, "dmeelement" );
	bool bEditableLabel = data->GetInt( "editablelabel" );

	CDmElement *pOwner = GetElementKeyValue< CDmElement >( data, "ownerelement" );
	const char *pAttributeName = data->GetString( "attributeName" );

	int bIsAttribute = data->GetInt( "isAttribute" );

	int nNotifyFlags = 0;
	if ( bEditableLabel )
	{
		if ( pElement && !bIsAttribute )
		{
			CElementTreeUndoScopeGuard guard( NOTIFY_SETDIRTYFLAG, m_pNotify, "Rename Object" );
			pElement->SetName( newString );
			nNotifyFlags = NOTIFY_CHANGE_ATTRIBUTE_VALUE;
		}
		else if ( pOwner && pAttributeName )
		{
			CElementTreeUndoScopeGuard guard( NOTIFY_SETDIRTYFLAG, m_pNotify, "Rename Attribute" );
			pOwner->RenameAttribute( pAttributeName, newString );
			nNotifyFlags = NOTIFY_CHANGE_TOPOLOGICAL;
		}
	}

	if ( nNotifyFlags )
	{
		Refresh( ( nNotifyFlags == NOTIFY_CHANGE_ATTRIBUTE_VALUE ) ? REFRESH_VALUES_ONLY : REFRESH_TREE_VIEW );
	}
}

bool CElementPropertiesTreeInternal::IsItemDroppable( int itemIndex, CUtlVector< KeyValues * >& msglist )
{
	KeyValues *itemData = m_pTree->GetItemData( itemIndex );
	if ( !itemData )
		return false;

	const char *elementType = itemData->GetString( "droppableelementtype" );
	if ( !elementType || !elementType[ 0 ] )
		return false;

	CUtlVector< CDmElement * > list;
	return ElementTree_GetDroppableItems( msglist, elementType, list );
}

HCursor CElementPropertiesTreeInternal::GetItemDropCursor( int itemIndex, CUtlVector< KeyValues * >& msglist )
{
	DropOperation_t op = GetDropOperation( itemIndex, msglist );
	if ( op == DO_COPY )
		return m_hDragCopyCursor;
	if ( op == DO_MOVE )
		return m_hDragMoveCursor;
	Assert( op == DO_LINK );
	return m_hDragLinkCursor;
}

struct ArrayItem_t
{
	ArrayItem_t( CDmAttribute *pAttr = NULL, int nIndex = -1 ) : m_pAttr( pAttr ), m_nIndex( nIndex ) {}

	static bool LessFunc( const ArrayItem_t &lhs, const ArrayItem_t &rhs )
	{
		if ( lhs.m_pAttr != rhs.m_pAttr )
			return lhs.m_pAttr < rhs.m_pAttr;
		return lhs.m_nIndex < rhs.m_nIndex;
	}

	CDmAttribute *m_pAttr;
	int m_nIndex;
};

void CElementPropertiesTreeInternal::DropItemsIntoArray( CDmrElementArray<> &array, CUtlVector< KeyValues* > &msglist, CUtlVector< CDmElement* > &list, int nArrayIndex, DropOperation_t op )
{
	int nElements = list.Count();
	if ( op == DO_COPY )
	{
		CUtlVector< CDmElement* > copylist;
		CopyElements( list, copylist );
		list.Swap( copylist );
	}
	else if ( op == DO_MOVE )
	{
		m_pTree->GetTree()->ClearSelection();

		CUtlRBTree< ArrayItem_t > arrayItemSorter( 0, msglist.Count(), ArrayItem_t::LessFunc );

		// sort all element array items and set element attributes to NULL
		int nMsgs = msglist.Count();
		for ( int i = 0; i < nMsgs; ++i )
		{
			KeyValues *itemData = msglist[ i ];

			CDmElement *pOwner = GetElementKeyValue< CDmElement >( itemData, "ownerelement" );
			const char *pAttributeName = itemData->GetString( "attributeName" );

			if ( !pOwner || !pAttributeName || !*pAttributeName )
				continue;

			bool isArrayElement = !itemData->IsEmpty( "arrayIndex" );
			CDmAttribute *pAttribute = pOwner->GetAttribute( pAttributeName, isArrayElement ? AT_ELEMENT_ARRAY : AT_ELEMENT );
			if ( !pAttribute )
				continue;

			if ( isArrayElement )
			{
				int nIndex = itemData->GetInt( "arrayIndex", -1 );
				if ( nIndex < 0 )
					continue;

				arrayItemSorter.Insert( ArrayItem_t( pAttribute, nIndex ) );
			}
			else
			{
				pAttribute->SetValue( DMELEMENT_HANDLE_INVALID );
			}
		}

		// walk through all array items, back to front, so that removing won't mess up the indices
		for ( int i = arrayItemSorter.LastInorder(); i != arrayItemSorter.InvalidIndex(); i = arrayItemSorter.PrevInorder( i ) )
		{
			ArrayItem_t &arrayItem = arrayItemSorter[ i ];

			CDmrElementArray<> srcArray( arrayItem.m_pAttr );
			srcArray.Remove( arrayItem.m_nIndex );

			if ( arrayItem.m_pAttr == array.GetAttribute() && arrayItem.m_nIndex < nArrayIndex )
			{
				--nArrayIndex; // update nArrayIndex when items before it are removed
			}
		}
	}

	int base = array.InsertMultipleBefore( nArrayIndex, nElements );
	//			array.SetMultiple( base, nElements, list.Base() );
	for ( int i = 0; i < nElements; ++i )
	{
		array.Set( base + i, list[ i ] );
		if ( array[ base + i ] != list[ i ] )
		{
			// if couldn't be dropped into array, skip it and merge remaining items down by one
			Assert( array[ base + i ] == NULL );
			array.Remove( base + nElements - 1 );
			--base;
		}
	}
}

CElementPropertiesTreeInternal::DropOperation_t CElementPropertiesTreeInternal::GetDropOperation( int itemIndex, CUtlVector< KeyValues * >& msglist )
{
	bool bCtrlDown = input()->IsKeyDown( KEY_LCONTROL ) || input()->IsKeyDown( KEY_RCONTROL );
	bool bAltDown = input()->IsKeyDown( KEY_LALT ) || input()->IsKeyDown( KEY_RALT );
	bool bShiftDown = input()->IsKeyDown( KEY_LSHIFT ) || input()->IsKeyDown( KEY_RSHIFT );

	if ( bAltDown || ( bShiftDown && bCtrlDown ) )
		return DO_LINK;
	if ( bCtrlDown )
		return DO_COPY;
	if ( bShiftDown )
		return DO_MOVE;

	KeyValues *itemData = m_pTree->GetItemData( itemIndex );
	Assert( itemData );
	if ( !itemData )
		return DO_LINK;

	if ( !ElementTree_IsArrayItem( itemData ) && ElementTree_GetAttributeType( itemData ) != AT_ELEMENT_ARRAY )
		return DO_LINK; // dropping to a non-array attribute

	DropOperation_t op = DO_UNKNOWN;
	int nMsgs = msglist.Count();
	for ( int i = 0; i < nMsgs; ++i )
	{
		KeyValues *pMsg = msglist[ i ];
		if ( !pMsg || !GetElementKeyValue< CDmElement >( pMsg , "dmeelement" ) )
			continue; // skip non-element drag/drop items

		if ( !ElementTree_IsArrayItem( pMsg ) || ElementTree_GetAttributeType( pMsg ) != AT_ELEMENT_ARRAY )
			return DO_LINK; // dragging from a non-array attribute

		op = DO_MOVE; // basically, op will only stay DO_MOVE if *every* item is a non-element or is an array (or array item)
	}

	if ( op == DO_UNKNOWN )
	{
		Assert( 0 );
		return DO_LINK;
	}

	return op;
}

void CElementPropertiesTreeInternal::OnItemDropped( int itemIndex, CUtlVector< KeyValues * >& msglist )
{
	if ( !msglist.Count() )
		return;

	KeyValues *itemData = m_pTree->GetItemData( itemIndex );
	if ( !itemData )
		return;

	const char *elementType = itemData->GetString( "droppableelementtype" );
	if ( !elementType || !elementType[ 0 ] )
		return;

	CUtlVector< CDmElement * > list;
	ElementTree_GetDroppableItems( msglist, "dmeelement", list );
	if ( !list.Count() )
		return;

	bool isArrayElement = !itemData->IsEmpty( "arrayIndex" );
	//Check to see if this attribute refers to an element
	CDmElement *pOwner = GetElementKeyValue< CDmElement >( itemData, "ownerelement" );
	const char *pAttributeName = itemData->GetString( "attributeName" );

	if ( !pOwner )
		return;

	if ( !pAttributeName[ 0 ] )
		return;

	CDmAttribute *pAttribute = pOwner->GetAttribute( pAttributeName );
	DmAttributeType_t attType = pAttribute ? pAttribute->GetType() : AT_UNKNOWN;
	bool isElementAttribute = attType == AT_ELEMENT || attType == AT_ELEMENT_ARRAY;
	if ( !isElementAttribute )
		return;

	DropOperation_t op = GetDropOperation( itemIndex, msglist );

	const char *cmd = msglist[ 0 ]->GetString( "command" );

	// Mouse if over an array entry which is an element array type...
	if ( isArrayElement )
	{
		bool bReplace = Q_stricmp( cmd, "replace" ) == 0;
		bool bBefore  = Q_stricmp( cmd, "before" ) == 0;
		bool bAfter   = Q_stricmp( cmd, "after" ) == 0 || Q_stricmp( cmd, "default" ) == 0;
		if ( !bReplace && !bBefore && !bAfter )
		{
			Warning( "Unknown command '%s'\n", cmd );
			return;
		}

		char str[ 128 ];
		V_snprintf( str, sizeof( str ), "%s %s element%s",
			bReplace ? "Replace with" : "Insert",
			op == DO_COPY ? "copied" : ( op == DO_MOVE ? "moved" : "referenced" ),
			bBefore ? " before" : bAfter ? " after" : "" );

		CElementTreeUndoScopeGuard guard( NOTIFY_SETDIRTYFLAG, m_pNotify, str );

		int nArrayIndex = itemData->GetInt( "arrayIndex" );
		if ( bAfter )
		{
			++nArrayIndex;
		}

		CDmrElementArray<> array( pAttribute );
		if ( bReplace )
		{
			array.Remove( nArrayIndex );
		}

		DropItemsIntoArray( array, msglist, list, nArrayIndex, op );
	}
	// Mouse is over an element attribute or element array attribute
	else
	{
		// No head/tail stuff for AT_ELEMENT, just replace what's there
		if ( attType == AT_ELEMENT )
		{
			char str[ 128 ];
			V_snprintf( str, sizeof( str ), "Replace with %s element",
				op == DO_COPY ? "copied" : ( op == DO_MOVE ? "moved" : "referenced" ) );
			CElementTreeUndoScopeGuard guard( NOTIFY_SETDIRTYFLAG, m_pNotify, str );

			pAttribute->SetValue( op == DO_COPY ? list[ 0 ]->Copy() : list[ 0 ] );

			if ( op == DO_MOVE )
			{
				int c = msglist.Count();
				for ( int i = 0; i < c; ++i )
				{	
					KeyValues *data = msglist[ i ];
					CDmElement *e = GetElementKeyValue<CDmElement>( data, "dmeelement" );
					Assert( !e || e == list[ 0 ] );
					if ( e != list[ 0 ] )
						continue;

					OnRemoveFromData( data );
					break;
				}
				m_pTree->GetTree()->ClearSelection();
			}
		}
		else
		{
			bool bTail = !cmd[ 0 ] || !Q_stricmp( cmd, "default" ) || !Q_stricmp( cmd, "tail" );
			bool bHead = !bTail && !Q_stricmp( cmd, "head" );
			bool bReplace = !bTail && !bHead && !Q_stricmp( cmd, "replace" );
			if ( !bTail && !bHead && !bReplace )
			{
				Warning( "Unknown command '%s'\n", cmd );
				return;
			}

			char str[ 128 ];
			V_snprintf( str, sizeof( str ), "%s %s elements%s",
				bReplace ? "Replace array with" : "Insert",
				op == DO_COPY ? "copied" : ( op == DO_MOVE ? "moved" : "referenced" ),
				bHead ? " at head" : bTail ? " at tail" : "" );

			CElementTreeUndoScopeGuard guard( NOTIFY_SETDIRTYFLAG, m_pNotify, str );

			CDmrElementArray<> array( pAttribute );
			if ( bReplace )
			{
				array.RemoveAll();
			}

			DropItemsIntoArray( array, msglist, list, bTail ? array.Count() : 0, op );
		}
	}

	CUtlVector< TreeItem_t > dropTargetPath;
	if ( isArrayElement )
	{
		itemIndex = m_pTree->GetTree()->GetItemParent( itemIndex ); // if we're an array element, start with the array itself
	}
	GetPathToItem( dropTargetPath, itemIndex );

	// Does a forced refresh
	Refresh( REFRESH_TREE_VIEW );

	itemIndex = OpenPath( dropTargetPath );
	if ( attType == AT_ELEMENT_ARRAY )
	{
		m_pTree->GetTree()->ExpandItem( itemIndex, true );
	}

	if ( op == DO_MOVE )
	{
		if ( isArrayElement || attType == AT_ELEMENT_ARRAY )
		{
			int nElements = list.Count();
			for ( int i = 0; i < nElements; ++i )
			{
				int nChildren = m_pTree->GetTree()->GetNumChildren( itemIndex );
				for ( int ci = 0; ci < nChildren; ++ci )
				{
					int nChildItem = m_pTree->GetTree()->GetChild( itemIndex, ci );
					KeyValues *pChildData = m_pTree->GetTree()->GetItemData( nChildItem );
					if ( list[ i ] == GetElementKeyValue< CDmElement >( pChildData, "dmeelement" ) )
					{
						m_pTree->GetTree()->AddSelectedItem( nChildItem, false );
					}
				}
			}
		}
		else
		{
			m_pTree->GetTree()->AddSelectedItem( itemIndex, true );
		}
	}
}

bool CElementPropertiesTreeInternal::GetItemDropContextMenu( int itemIndex, Menu *menu, CUtlVector< KeyValues * >& msglist )
{
	KeyValues *itemData = m_pTree->GetItemData( itemIndex );

	bool isArrayElement = !itemData->IsEmpty( "arrayIndex" );
	//Check to see if this attribute refers to an element
	CDmElement *pOwner = GetElementKeyValue<CDmElement>( itemData, "ownerelement" );
	const char *pAttributeName = itemData->GetString( "attributeName" );

	if ( !pOwner )
		return false;
	if ( !pAttributeName[ 0 ] )
		return false;

	bool isElementAttribute = false;
	CDmAttribute *pAttribute = pOwner->GetAttribute( pAttributeName );
	DmAttributeType_t attType = pAttribute ? pAttribute->GetType() : AT_UNKNOWN;
	switch ( attType )
	{
	default:
		break;
	case AT_ELEMENT:
	case AT_ELEMENT_ARRAY:
		isElementAttribute = true;
		break;
	}	

	if ( isArrayElement && isElementAttribute )
	{
		menu->AddMenuItem( "After", "Insert after", "after", this );
		menu->AddMenuItem( "Before", "Insert before", "before", this );
		menu->AddMenuItem( "Replace", "Replace", "replace", this );
		return true;
	}
	else
	{
		if ( isElementAttribute && attType == AT_ELEMENT_ARRAY )
		{
			CDmrGenericArray array( pAttribute );
			if ( array.IsValid() && array.Count() > 0 )
			{
				menu->AddMenuItem( "Tail", "Insert at tail", "tail", this );
				menu->AddMenuItem( "Head", "Insert at head", "head", this );
				menu->AddMenuItem( "Replace", "Replace", "replace", this );
				return true;
			}
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Set/get object
//-----------------------------------------------------------------------------
void CElementPropertiesTreeInternal::SetObject( CDmElement *object )
{
	m_pTree->RemoveAll();
	m_AttributeWidgets.RemoveAll();

	AddToHistory( object );

	m_hObject = object;

	Init( );
}

CDmElement *CElementPropertiesTreeInternal::GetObject()
{
	return m_hObject.Get();
}


//-----------------------------------------------------------------------------
// Gets tree view text
//-----------------------------------------------------------------------------
void CElementPropertiesTreeInternal::GetTreeViewText( CDmElement* obj, CDmAttribute *pAttribute, int nArrayIndex, char *pBuffer, int nMaxLen, bool& editableText )
{
	pBuffer[0] = 0;

	editableText = false;

	if ( !obj )
		return;

	const char *pAttributeName = pAttribute->GetName();

	if ( nArrayIndex < 0 )
	{
		// non-array types
		Q_strncpy( pBuffer, pAttributeName, nMaxLen );

		editableText = !pAttribute->IsFlagSet( FATTRIB_EXTERNAL ) && !pAttribute->IsFlagSet( FATTRIB_READONLY );
	}
	else
	{
		// array types
		DmAttributeType_t type = pAttribute->GetType( );
		if ( type == AT_ELEMENT_ARRAY )
		{
			const CDmrElementArray<> elementArray( pAttribute );
			CDmElement *pEntryElement = elementArray[nArrayIndex];
			if ( pEntryElement )
			{
				Q_snprintf( pBuffer, nMaxLen, "%s", pEntryElement->GetValueString( "name" ) );
				editableText = true;
			}
		}
		else
		{
			Q_snprintf( pBuffer, nMaxLen, "%s[%d]", pAttributeName, nArrayIndex );
		}
	}
}


//-----------------------------------------------------------------------------
// Finds the tree index of a child matching the particular element + attribute
//-----------------------------------------------------------------------------
int CElementPropertiesTreeInternal::FindTreeItem( int nParentIndex, const TreeItem_t &info )
{
	// Look for a match
	int nCount = m_pTree->GetTree()->GetNumChildren( nParentIndex );
	for ( int i = nCount; --i >= 0; )
	{
		int nChildIndex = m_pTree->GetTree()->GetChild( nParentIndex, i );
		KeyValues *data = m_pTree->GetItemData( nChildIndex );
		Assert( data );

		CDmElement *pElement = GetElementKeyValue< CDmElement >( data, "ownerelement" );
		const char *pAttributeName = data->GetString( "attributeName" );
		CDmElement *pArrayElement = NULL;
		if ( data->GetInt( "arrayIndex", -1 ) != -1 )
		{
			// Only arrays of element pointers should refer to this
			pArrayElement = GetElementKeyValue< CDmElement >( data, "dmeelement" );
		}

		if ( ( pElement == info.m_pElement ) && ( pArrayElement == info.m_pArrayElement ) &&
			!Q_stricmp( pAttributeName, info.m_pAttributeName ) )
		{
			return nChildIndex;
		}
	}
	return -1;
}

void CElementPropertiesTreeInternal::SpewOpenItems( int depth, OpenItemTree_t &tree, int nOpenTreeIndex, int nItemIndex )
{
	int i = tree.FirstChild( nOpenTreeIndex );
	if ( nOpenTreeIndex != tree.InvalidIndex() )
	{
		TreeInfo_t& info = tree[ nOpenTreeIndex ];

		if ( info.m_nFlags & EP_EXPANDED )
		{
			Msg( "[%d] Marking %s <%s> %s array(%s) [expanded %i]\n",
				depth,
				info.m_Item.m_pElement->GetName(), 
				info.m_Item.m_pElement->GetTypeString(),
				info.m_Item.m_pAttributeName.Get(),
				info.m_Item.m_pArrayElement ? info.m_Item.m_pArrayElement->GetName() : "NULL",
				info.m_nFlags & EP_EXPANDED ? 1 : 0 );
		}
	}

	while ( i != tree.InvalidIndex() )
	{
		TreeInfo_t& info = tree[ i ];
		// Look for a match
		int nChildIndex = FindTreeItem( nItemIndex, info.m_Item );
		if ( nChildIndex != -1 )
		{
			SpewOpenItems( depth + 1, tree, i, nChildIndex );
		}
		else
		{
		}
		i = tree.NextSibling( i );
	}
}

//-----------------------------------------------------------------------------
// Expands all items in the open item tree if they exist
//-----------------------------------------------------------------------------
void CElementPropertiesTreeInternal::ExpandOpenItems( OpenItemTree_t &tree, int nOpenTreeIndex, int nItemIndex, bool makeVisible )
{
	int i = tree.FirstChild( nOpenTreeIndex );
	if ( nOpenTreeIndex != tree.InvalidIndex() )
	{
		TreeInfo_t& info = tree[ nOpenTreeIndex ];
		if ( info.m_nFlags & EP_EXPANDED )
		{
			// Expand the item
			m_pTree->ExpandItem( nItemIndex , true );
		}
		if ( info.m_nFlags & EP_SELECTED )
		{
			m_pTree->GetTree()->AddSelectedItem( nItemIndex, false, false );
			if ( makeVisible )
			{
				m_pTree->GetTree()->MakeItemVisible( nItemIndex );
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
				int nChildIndex = FindTreeItem( nItemIndex, info.m_Preserved );
				if ( nChildIndex != -1 )
				{
					m_pTree->GetTree()->AddSelectedItem( nChildIndex, false, false );
					if ( makeVisible )
					{
						m_pTree->GetTree()->MakeItemVisible( nChildIndex );
					}
				}
			}
		}
		i = tree.NextSibling( i );
	}
}

void CElementPropertiesTreeInternal::FillInDataForItem( TreeItem_t &item, int nItemIndex )
{
	KeyValues *data = m_pTree->GetItemData( nItemIndex );
	if ( !data )
		return;

	item.m_pElement = GetElementKeyValue< CDmElement >( data, "ownerelement" );
	item.m_pAttributeName = data->GetString( "attributeName" );
	if ( data->GetInt( "arrayIndex", -1 ) != -1 )
	{
		// Only arrays of element pointers should refer to this
		item.m_pArrayElement = GetElementKeyValue< CDmElement >( data, "dmeelement" );	
	}
	else
	{
		item.m_pArrayElement = NULL;
	}
}

//-----------------------------------------------------------------------------
// Builds a list of open items
//-----------------------------------------------------------------------------
void CElementPropertiesTreeInternal::BuildOpenItemList( OpenItemTree_t &tree, int nParent, int nItemIndex, bool preservePrevSelectedItem )
{
	KeyValues *data = m_pTree->GetItemData( nItemIndex );
	if ( !data )
		return;

	bool expanded = m_pTree->IsItemExpanded( nItemIndex );
	bool selected = m_pTree->IsItemSelected( nItemIndex );

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

	if ( selected )
	{
		// Set up prev an next item
		int preserve = preservePrevSelectedItem 
			?	m_pTree->GetTree()->GetPrevChildItemIndex( nItemIndex ) : 
				m_pTree->GetTree()->GetNextChildItemIndex( nItemIndex );

		if ( preserve != -1 )
		{
			FillInDataForItem( info.m_Preserved, preserve );	
		}
	}

	// Deal with children
	int nCount = m_pTree->GetTree()->GetNumChildren( nItemIndex );
	for ( int i = 0; i < nCount; ++i )
	{
		int nChildIndex = m_pTree->GetTree()->GetChild( nItemIndex, i );
		BuildOpenItemList( tree, nChild, nChildIndex, preservePrevSelectedItem );
	}
}


//-----------------------------------------------------------------------------
// Builds a list of open items
//-----------------------------------------------------------------------------
void CElementPropertiesTreeInternal::RefreshTreeView( bool preservePrevSelectedItem /*= false*/ )
{
	int nIndex = m_pTree->GetTree()->GetRootItemIndex();
	if ( nIndex >= 0 )
	{
		// remember where the tree is scrolled to
		ScrollBar *sBar = ((CElementTree *)m_pTree->GetTree())->GetScrollBar();
		int sBarPos = sBar->GetValue();

		// Build a tree of every open item in the tree view
		OpenItemTree_t openItems;
		BuildOpenItemList( openItems, openItems.InvalidIndex(), nIndex, preservePrevSelectedItem );

		// Close the tree and re-create the root node only
		UpdateTree();

		// NOTE: Updating the tree could have changed the root item index
		nIndex = m_pTree->GetTree()->GetRootItemIndex();

		// Iterate through all previously open items and expand them if they exist
		if ( openItems.Root() != openItems.InvalidIndex() )
		{
			ExpandOpenItems( openItems, openItems.Root(), nIndex, false );
		}

		// and now set the scroll pos back to where is was
		// note: the layout needs to be re-Performed so that the
		//       scrollbars _range values are corrent or the SetValue will fail.
		m_pTree->GetTree()->PerformLayout();
		sBar->SetValue( sBarPos );
	}
}


//-----------------------------------------------------------------------------
// Refreshes the color state of the tree
//-----------------------------------------------------------------------------
void CElementPropertiesTreeInternal::SetTreeItemColor( int nItemID, CDmElement *pEntryElement, bool bIsElementArrayItem, bool bEditableLabel )
{	
	// dim any element tree items if they are muted or not visible
	bool bIsDim = false;
	int dimAlpha = 128;
	if ( pEntryElement != NULL )
	{
		if ( ( pEntryElement->HasAttribute( "visible" ) && !pEntryElement->GetValue< bool >( "visible" ) )
			|| ( pEntryElement->HasAttribute( "mute" ) && pEntryElement->GetValue< bool >( "mute" ) ) )
		{
			bIsDim = true;
		}
	}

	// the unfocused Bg color should match the focused one
	// so that we can dim lables based on visibility and mute state.
	// note: focus is unimportant in this context ( I'm pretty sure )
	m_pTree->GetTree()->SetItemSelectionBgColor( nItemID, Color( 255, 153, 35, bIsDim ? dimAlpha : 255 ) );
	m_pTree->GetTree()->SetItemSelectionUnfocusedBgColor( nItemID, Color( 255, 153, 35, bIsDim ? dimAlpha : 255 ) );

	if ( bIsElementArrayItem )
	{
		// element array items are green
		m_pTree->GetTree()->SetItemFgColor( nItemID, Color( 66, 196, 66, bIsDim ? dimAlpha : 255 ) );
	}
	else if ( bEditableLabel )
	{
		// custom attributes are light yellow
		m_pTree->GetTree()->SetItemFgColor( nItemID, Color( 190, 190, 105, bIsDim ? dimAlpha : 255 ) );
	}
	else
	{
		// otherwise it's just light grey
		m_pTree->GetTree()->SetItemFgColor( nItemID, Color( 160, 160, 160, bIsDim ? dimAlpha : 255 ) );
	}
}


//-----------------------------------------------------------------------------
// Refreshes the color state of the tree
//-----------------------------------------------------------------------------
void CElementPropertiesTreeInternal::RefreshTreeItemState( int nItemID )
{
	if ( nItemID < 0 )
		return;

	KeyValues *kv = m_pTree->GetTree()->GetItemData( nItemID );
	CDmElement *pEntryElement = GetElementKeyValue<CDmElement>( kv, "dmeelement" );
	bool bIsElementArrayItem = kv->GetInt( "elementArrayItem", 0 );
	bool bEditableLabel = kv->GetInt( "editablelabel", 0 );
	SetTreeItemColor( nItemID, pEntryElement, bIsElementArrayItem, bEditableLabel );

	int nChildCount = m_pTree->GetTree()->GetNumChildren( nItemID );
	for ( int i = 0; i < nChildCount; ++i )
	{
		int nChildID = m_pTree->GetTree()->GetChild( nItemID, i );
		RefreshTreeItemState( nChildID );
	}
}


/*
//-----------------------------------------------------------------------------
// Adds a single entry into the tree
//-----------------------------------------------------------------------------
void CElementPropertiesTreeInternal::SetTreeEntryDimState(  )
{

}
*/


//-----------------------------------------------------------------------------
// Adds a single entry into the tree
//-----------------------------------------------------------------------------
void CElementPropertiesTreeInternal::CreateTreeEntry( int parentNodeIndex, CDmElement* obj, CDmAttribute *pAttribute, int nArrayIndex, AttributeWidgets_t &widgets )
{
	char pText[ 512 ];
	bool bEditableLabel = false;
	bool bIsExpandable = false;
	CDmElement *pEntryElement = NULL;

	GetTreeViewText( obj, pAttribute, nArrayIndex, pText, sizeof(pText), bEditableLabel );

	const char *pAttributeName = pAttribute->GetName();
	DmAttributeType_t type = pAttribute->GetType( );
	
	bool bIsArrayItem = ( nArrayIndex > -1 );
	bool bIsArrayAttribute = ( type >= AT_FIRST_ARRAY_TYPE ) && ! bIsArrayItem;
	bool bIsElementAttribute = ( type == AT_ELEMENT ) && ! bIsArrayItem;
	bool bIsElementArrayItem = ( type == AT_ELEMENT_ARRAY ) && bIsArrayItem;
	bool bIsElementArrayAttribute = ( type == AT_ELEMENT_ARRAY ) && ! bIsArrayItem;

	bool bIsDroppable = bIsElementArrayItem || bIsElementAttribute || bIsElementArrayAttribute;

	if ( bIsElementArrayItem )
	{
		const CDmrElementArray<> elementArray( pAttribute );
		pEntryElement = elementArray[nArrayIndex];
		bIsExpandable = true;
	}
	else if ( bIsElementAttribute )
	{
		pEntryElement = obj->GetValueElement< CDmElement>( pAttributeName );
		bIsExpandable = ( pEntryElement != NULL );
	}
	else if ( bIsArrayAttribute )
	{
		CDmrGenericArray array( pAttribute );
		bIsExpandable = array.Count() > 0;
	}

	KeyValues *kv = new KeyValues( "item" );
	kv->SetString( "Text", pText );
	kv->SetInt( "Expand", bIsExpandable );
	SetElementKeyValue( kv, "dmeelement", pEntryElement );
	SetElementKeyValue( kv, "ownerelement", obj );
	kv->SetString( "attributeName", pAttributeName );
 	kv->SetPtr( "widget", widgets.m_pValueWidget );
	kv->SetInt( "elementArrayItem", bIsElementArrayItem ? 1 : 0 );
	kv->SetInt( "editablelabel", bEditableLabel ? 1 : 0 );
	kv->SetInt( "isAttribute", bIsArrayItem ? 0 : 1 );
	kv->SetInt( "droppable", bIsDroppable ? 1 : 0 );
	kv->SetFloat( "drophoverdelay", 1.0f );

	if ( bIsArrayItem )
	{
		kv->SetInt( "arrayIndex", nArrayIndex );
	}

	if ( bIsDroppable )
	{
		// Can always drop onto arrays
		kv->SetString( "droppableelementtype", "dmeelement" ); // FIXME: Should be able to restrict to certain types!!!
	}

	CUtlVector< vgui::Panel * >	columns;
	columns.AddToTail( NULL );
	columns.AddToTail( widgets.m_pValueWidget );
	int itemIndex = m_pTree->AddItem( kv, bEditableLabel, parentNodeIndex, columns );
	SetTreeItemColor( itemIndex, pEntryElement, bIsElementArrayItem, bEditableLabel );
	kv->deleteThis();
}

//-----------------------------------------------------------------------------
// Sets up the attribute widget init info for a particular attribute
//-----------------------------------------------------------------------------
void CElementPropertiesTreeInternal::SetupWidgetInfo( AttributeWidgetInfo_t *pInfo, CDmElement *obj, CDmAttribute *pAttribute, int nArrayIndex )
{
	const char *pAttributeName = pAttribute ? pAttribute->GetName() : "";

	pInfo->m_pNotify = m_pNotify;
	pInfo->m_bAutoApply = m_bAutoApply;
	pInfo->m_pElement = obj;
	pInfo->m_pAttributeName = pAttributeName;
	pInfo->m_nArrayIndex = nArrayIndex;
	pInfo->m_pEditorTypeDictionary = m_hTypeDictionary;
	pInfo->m_pEditorInfo = NULL;
	pInfo->m_bShowMemoryUsage = m_bShowMemoryUsage;
	if ( m_hTypeDictionary && pAttributeName )
	{
		if ( nArrayIndex < 0 )
		{
			pInfo->m_pEditorInfo = m_hTypeDictionary->GetAttributeInfo( obj, pAttributeName );
		}
		else
		{
			pInfo->m_pEditorInfo = m_hTypeDictionary->GetAttributeArrayInfo( obj, pAttributeName );
		}
	}
}


//-----------------------------------------------------------------------------
// Adds a single editable attributes of the element to the tree
//-----------------------------------------------------------------------------
void CElementPropertiesTreeInternal::InsertSingleAttribute( int parentNodeIndex, CDmElement *obj, CDmAttribute *pAttribute, int nArrayIndex )
{
	const char *attributeName = pAttribute->GetName();
	NOTE_UNUSED( attributeName );

	// Get information about the widget to create

	IAttributeWidgetFactory *pFactory = NULL;
	if ( nArrayIndex >= 0 )
	{
		pFactory = attributewidgetfactorylist->GetArrayWidgetFactory( obj, pAttribute, m_hTypeDictionary );
	}
	else
	{
		pFactory = attributewidgetfactorylist->GetWidgetFactory( obj, pAttribute, m_hTypeDictionary );
	}
	if ( !pFactory )
		return;

	// Create the widget
	AttributeWidgetInfo_t info;
	SetupWidgetInfo( &info, obj, pAttribute, nArrayIndex );

	AttributeWidgets_t attributeWidget;
	attributeWidget.m_pValueWidget = pFactory->Create( NULL, info );

	// set it to the current font size
	CBaseAttributePanel *attrPanel = dynamic_cast< CBaseAttributePanel * >( attributeWidget.m_pValueWidget );
	if ( attrPanel )
	{
		attrPanel->SetFont( m_pTree->GetFont( m_pTree->GetFontSize() ) );
	}

	// Now create the tree-view entry
	CreateTreeEntry( parentNodeIndex, obj, pAttribute, nArrayIndex, attributeWidget );

	// Add the attribute to the list of them
	m_AttributeWidgets.AddToTail( attributeWidget );
}


//-----------------------------------------------------------------------------
// Used to insert attributes in alphabetical order
//-----------------------------------------------------------------------------
struct AttributeInfo_t
{
	CDmAttribute *m_pAttribute;
	const char *m_pName;
};


//-----------------------------------------------------------------------------
// Adds editable attributes of the element to the tree
//-----------------------------------------------------------------------------
void CElementPropertiesTreeInternal::InsertAttributes( int parentNodeIndex, CDmElement *obj )
{
	Assert( obj );

	// Build a list of attributes for sorting
	AttributeInfo_t *pInfo = (AttributeInfo_t*)_alloca( obj->AttributeCount() * sizeof(AttributeInfo_t) );
	int nCount = 0;
	for ( CDmAttribute *pAttribute = obj->FirstAttribute(); pAttribute; pAttribute = pAttribute->NextAttribute() )
	{
		pInfo[nCount].m_pAttribute = pAttribute;
		pInfo[nCount].m_pName = pAttribute->GetName();
		++nCount;
	}

	// Iterate over each element and create a widget and tree entry for it
	for ( int i = nCount - 1; i >= 0; --i )
	{
		InsertSingleAttribute( parentNodeIndex, obj, pInfo[i].m_pAttribute );
	}
}


//-----------------------------------------------------------------------------
// Removes an item from the tree recursively
//-----------------------------------------------------------------------------
void CElementPropertiesTreeInternal::RemoveItem_R( int nItemIndex )
{
	KeyValues *data = m_pTree->GetItemData( nItemIndex );
	if ( data )
	{
		AttributeWidgets_t search;
		search.m_pValueWidget = static_cast<vgui::Panel*>( data->GetPtr( "widget", NULL ) );
		if ( search.m_pValueWidget )
		{
			m_AttributeWidgets.FindAndRemove( search );
		}
	}

	int nCount = m_pTree->GetTree()->GetNumChildren( nItemIndex );
	for ( int i = 0; i < nCount; ++i )
	{
		RemoveItem_R( m_pTree->GetTree()->GetChild( nItemIndex, i ) ); 
	}
}


//-----------------------------------------------------------------------------
// Removes an item from the tree
//-----------------------------------------------------------------------------
void CElementPropertiesTreeInternal::RemoveItem( int nItemIndex )
{
	RemoveItem_R( nItemIndex );
	m_pTree->RemoveItem( nItemIndex );
}


//-----------------------------------------------------------------------------
// Adds editable attribute array entries to the tree
//-----------------------------------------------------------------------------
void CElementPropertiesTreeInternal::InsertAttributeArrayMembers( int parentNodeIndex, CDmElement *obj, CDmAttribute *pAttribute )
{
	Assert( obj );

	// Iterate over each element and create a widget and tree entry for it
	CDmrGenericArray array( pAttribute );
	int c = array.Count();
	for ( int i = 0; i < c; i++ )
	{
		InsertSingleAttribute( parentNodeIndex, obj, pAttribute, i );
	}
}

void CElementPropertiesTreeInternal::OnKeyDelete()
{
	RemoveSelected( false );
}

void CElementPropertiesTreeInternal::OnKeyBackspace()
{
	RemoveSelected( true );
}

void CElementPropertiesTreeInternal::RemoveSelected( bool selectLeft )
{
	CElementTreeUndoScopeGuard guard( NOTIFY_SETDIRTYFLAG, m_pNotify, "Delete Items" );

	CUtlVector< KeyValues * > itemData;
	m_pTree->GetTree()->GetSelectedItemData( itemData );
	bool bRefreshNeeded = OnRemoveFromData( itemData );

	if ( itemData.Count() > 1 )
	{
		// dont try to maintain the selection if multiple items are selected
		m_pTree->GetTree()->ClearSelection();
	}

	if ( bRefreshNeeded )
	{
		// Refresh the tree
		Refresh( REFRESH_TREE_VIEW, selectLeft );
	}
}

bool CElementPropertiesTreeInternal::IsLabelBeingEdited() const
{
	return m_pTree->GetTree()->IsLabelBeingEdited();
}

bool CElementPropertiesTreeInternal::HasItemsSelected() const
{
	return m_pTree->GetTree()->GetSelectedItemCount() > 0 ? true : false;
}


//-----------------------------------------------------------------------------
// protected accessors
//-----------------------------------------------------------------------------
KeyValues *CElementPropertiesTreeInternal::GetTreeItemData( int itemIndex )
{
	return m_pTree->GetItemData( itemIndex );
}


void CElementPropertiesTreeInternal::OnRefresh()
{
	// Does a forced refresh
	Refresh( REFRESH_TREE_VIEW );

	CElementTreeNotifyScopeGuard notify( "CElementPropertiesTreeInternal::OnRefresh", NOTIFY_SETDIRTYFLAG | NOTIFY_CHANGE_TOPOLOGICAL, m_pNotify );
}


//-----------------------------------------------------------------------------
//
// CElementPropertiesTree methods 
//
//-----------------------------------------------------------------------------
CElementPropertiesTree::CElementPropertiesTree( vgui::Panel *parent, IDmNotify *pNotify, CDmElement *pObject, CDmeEditorTypeDictionary *pDict )
	: BaseClass( parent, "ElementPropertiesTreeFrame" )
{
	SetTitle( "#BxElementPropertiesTree", true );

	SetSizeable( true );
	SetCloseButtonVisible( false );
	SetMinimumSize( 600, 200 );

	m_pProperties = new CElementPropertiesTreeInternal( this, pNotify, pObject, false, pDict );

	m_pOK = new Button( this, "OK", "OK", this, "close" );
	m_pApply = new Button( this, "Apply", "Apply", this, "apply" );
	m_pCancel = new Button( this, "Cancel", "Cancel", this, "cancel" );

	SetScheme( vgui::scheme()->LoadSchemeFromFile( "Resource/BoxRocket.res", "BoxRocket" ) );
	LoadControlSettings( "resource/BxElementPropertiesTreeFrame.res" );
}

void CElementPropertiesTree::Init( )
{
	m_pProperties->Init( );
}

void CElementPropertiesTree::ActivateBuildMode()
{
	BaseClass::ActivateBuildMode();
	m_pProperties->ActivateBuildMode();
}

void CElementPropertiesTree::Refresh( CElementPropertiesTreeInternal::RefreshType_t rebuild /* = REFRESH_REBUILD */, bool preservePrevSelectedItem /*= false*/ )
{
	m_pProperties->Refresh( rebuild, preservePrevSelectedItem );
}

void CElementPropertiesTree::GenerateChildrenOfNode(int itemIndex)
{
	m_pProperties->GenerateChildrenOfNode( itemIndex );
}

void CElementPropertiesTree::SetObject( CDmElement *object )
{
	m_pProperties->SetObject( object );
}

void CElementPropertiesTree::OnCommand( const char *cmd )
{
	if ( !Q_stricmp( cmd, "close" ) )
	{
		m_pProperties->ApplyChanges();
		MarkForDeletion();
	}
	else if ( !Q_stricmp( cmd, "apply" ) )
	{
		m_pProperties->ApplyChanges();
		m_pProperties->Refresh();
	}
	else if ( !Q_stricmp( cmd, "cancel" ) )
	{
		MarkForDeletion();
	}
	else
	{
		BaseClass::OnCommand( cmd );
	}
}


//-----------------------------------------------------------------------------
//
// Hook this into the DmePanel editing system 
//
//-----------------------------------------------------------------------------
IMPLEMENT_DMEPANEL_FACTORY( CDmeElementPanel, DmElement, "DmeElementDefault", "Dme Element Editor", true );


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
#pragma warning (disable:4355)
CDmeElementPanel::CDmeElementPanel( vgui::Panel *pParent, const char *pPanelName ) :
	BaseClass( pParent, this, NULL )
{
}
#pragma warning (default:4355)


//-----------------------------------------------------------------------------
// Called when the panel changes something
//-----------------------------------------------------------------------------
void CDmeElementPanel::NotifyDataChanged( const char *pReason, int nNotifySource, int nNotifyFlags )
{
	if ( nNotifyFlags & ( NOTIFY_CHANGE_TOPOLOGICAL | NOTIFY_CHANGE_ATTRIBUTE_VALUE | NOTIFY_CHANGE_ATTRIBUTE_ARRAY_SIZE ) )
	{
		KeyValues *pKeyValues = new KeyValues( "DmeElementChanged", "notifyFlags", nNotifyFlags );
		pKeyValues->SetString( "reason", pReason );
		pKeyValues->SetInt( "source", nNotifySource );
		PostActionSignal( pKeyValues );
	}
}


//-----------------------------------------------------------------------------
// Called by the DmePanel framework to hook an element to this
//-----------------------------------------------------------------------------
void CDmeElementPanel::SetDmeElement( CDmElement *pElement )
{
	SetObject( pElement );
}
