//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef ELEMENTPROPERTIESTREE_H
#define ELEMENTPROPERTIESTREE_H

#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/Frame.h"
#include "dme_controls/AttributeWidgetFactory.h"
#include "vgui_controls/TreeView.h"
#include "vgui_controls/TreeViewListControl.h"
#include "datamodel/dmelement.h"
#include "datamodel/dmattribute.h"
#include "datamodel/dmattributevar.h"
#include "datamodel/dmehandle.h"
#include "tier1/utlntree.h"
#include "tier1/utlstring.h"
#include "tier1/utlvector.h"
#include "vgui_controls/InputDialog.h"
#include "vgui/KeyCode.h"
#include "dme_controls/inotifyui.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CDmElement;
class IDmNotify;
class CDocAllElements;
class IAttributeWidgetFactory;
class CDmeEditorTypeDictionary;
class CPropertiesTreeToolbar;

namespace vgui
{
	class TextEntry;
	class ComboBox;
	class Button;
	class PanelListPanel;
	class Menu;
}

//-----------------------------------------------------------------------------
// CElementTreeViewListControl
//-----------------------------------------------------------------------------
class CElementTreeViewListControl : public vgui::CTreeViewListControl
{
	DECLARE_CLASS_SIMPLE( CElementTreeViewListControl, CTreeViewListControl );

public:
	CElementTreeViewListControl( Panel *pParent, const char *pName );

	virtual void	ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual int		AddItem( KeyValues *data, bool allowLabelEditing, int parentItemIndex, CUtlVector< vgui::Panel * >& columnPanels );
	virtual void	RemoveItem( int nItemIndex ); 
	virtual void	PerformLayout();
	virtual void	RemoveAll();
	virtual vgui::HFont	GetFont( int size );
	virtual void	SetFont( vgui::HFont font );
	virtual int		GetFontSize();
	virtual void	SetFontSize( int size );
	virtual void	PostChildPaint();
    virtual void	ExpandItem( int itemIndex, bool bExpand );
	virtual bool	IsItemExpanded( int itemIndex );
	virtual bool	IsItemSelected( int itemIndex );
	virtual KeyValues *GetItemData( int itemIndex );
	virtual int		GetTreeColumnWidth();
	virtual void	SetTreeColumnWidth( int w );
	virtual void	OnCursorMoved( int x, int y );
	virtual void	OnMousePressed( vgui::MouseCode code );
	virtual void	OnMouseReleased( vgui::MouseCode code );
	virtual void	OnMouseDoublePressed( vgui::MouseCode code );
	virtual void	OnMouseWheeled( int delta );
	virtual int		GetScrollBarSize();
	virtual void	ToggleDrawGrid();
	virtual bool	IsDrawingGrid();

	void			ResizeTreeToExpandedWidth();

private:
	struct ColumnPanels_t
	{
		ColumnPanels_t() :
			treeViewItem( -1 )
		{
		}

		ColumnPanels_t( const ColumnPanels_t& src )
		{
			treeViewItem = src.treeViewItem;
			int i, c;
			c = src.m_Columns.Count();
			for ( i = 0; i < c; ++i )
			{
				m_Columns.AddToTail( src.m_Columns[ i ] );
			}
		}

		void SetList( CUtlVector< vgui::Panel * >& list )
		{
			m_Columns.RemoveAll();
			int c = list.Count();
			for ( int i = 0; i < c; ++i )
			{
				m_Columns.AddToTail( list[ i ] );
			}
		}

		int							treeViewItem;
		CUtlVector< vgui::Panel * >	m_Columns;
	};

	// Removes an item from the tree recursively
	void RemoveItem_R( int nItemIndex );

	void	 HideAll();

	static bool PanelsLessFunc( const ColumnPanels_t& lhs, const ColumnPanels_t& rhs )
	{
		return lhs.treeViewItem < rhs.treeViewItem;
	}

	int		m_iTreeColumnWidth;
	int		m_iFontSize;  // 1 = verySmall, small, normal, large, verylarge
	bool	m_bMouseLeftIsDown;
	bool	m_bMouseIsDragging;
	bool	m_bDrawGrid;
	CUtlRBTree< ColumnPanels_t, int >	m_Panels;
};

//-----------------------------------------------------------------------------
// CElementPropertiesTreeInternal 
//-----------------------------------------------------------------------------
class CElementPropertiesTreeInternal : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CElementPropertiesTreeInternal, vgui::EditablePanel );

public:
	enum RefreshType_t
	{
		REFRESH_REBUILD = 0,	// Close the entire tree
		REFRESH_VALUES_ONLY,	// Tree topology hasn't changed; only update values
		REFRESH_TREE_VIEW,		// Tree topology changed; some attributes may be added or removed
	};

	CElementPropertiesTreeInternal( vgui::Panel *parent, IDmNotify *pNotify, 
		CDmElement *pObject, bool autoApply = true, CDmeEditorTypeDictionary *pDict = NULL );
	~CElementPropertiesTreeInternal();

	virtual void Init( );
	virtual void Refresh( RefreshType_t rebuild = REFRESH_TREE_VIEW, bool preservePrevSelectedItem = false );
	virtual void ApplyChanges();
	virtual void GenerateChildrenOfNode( int itemIndex );
	virtual void GenerateContextMenu( int itemIndex, int x, int y );
	virtual void GenerateDragDataForItem( int itemIndex, KeyValues *msg );
	virtual void OnLabelChanged( int itemIndex, char const *oldString, char const *newString );
	virtual bool IsItemDroppable( int itemIndex, CUtlVector< KeyValues * >& msglist );
	virtual void OnItemDropped( int itemIndex, CUtlVector< KeyValues * >& msglist );
	virtual bool GetItemDropContextMenu( int itemIndex, vgui::Menu *menu, CUtlVector< KeyValues * >& msglist );
	virtual vgui::HCursor GetItemDropCursor( int itemIndex, CUtlVector< KeyValues * >& msglist );
	virtual void SetObject( CDmElement *object );
	virtual void OnCommand( const char *cmd );

	MESSAGE_FUNC( OnShowMemoryUsage, "OnShowMemoryUsage" );

	CDmElement *GetObject();
	bool		IsLabelBeingEdited() const;
	bool		HasItemsSelected() const;

	enum
	{
		DME_PROPERTIESTREE_MENU_BACKWARD = 0,
		DME_PROPERTIESTREE_MENU_FORWARD,
		DME_PROPERTIESTREE_MENU_SEARCHHSITORY,
	};

	virtual void	PopulateHistoryMenu( int whichMenu, vgui::Menu *menu );
	virtual int		GetHistoryMenuItemCount( int whichMenu );
	void			AddToSearchHistory( char const *str );
	void			SetTypeDictionary( CDmeEditorTypeDictionary *pDict );

	MESSAGE_FUNC_CHARPTR( OnNavSearch, "OnNavigateSearch", text );

protected:
	KeyValues *GetTreeItemData( int itemIndex );

protected:

	struct AttributeWidgets_t
	{
		vgui::Panel *m_pValueWidget;

		bool operator==( const AttributeWidgets_t &src ) const
		{
			return m_pValueWidget == src.m_pValueWidget;
		}
	};

	enum
	{
		EP_EXPANDED = (1<<0),
		EP_SELECTED = (1<<1),
	};

	struct TreeItem_t
	{
		TreeItem_t() :
			m_pElement( 0 ),
			m_pAttributeName(),
			m_pArrayElement( 0 )
		{
		}
		CDmElement *m_pElement;
		CUtlString m_pAttributeName;
		CDmElement *m_pArrayElement;	// points to the element referenced in an element array
	};

	// Used to build a list of open element for refresh
	struct TreeInfo_t
	{
		TreeInfo_t() :
			m_nFlags( 0 )
		{
		}
		
		TreeItem_t	m_Item;	// points to the element referenced in an element array

		int			m_nFlags;
		TreeItem_t	m_Preserved;
	};

	typedef CUtlNTree< TreeInfo_t, int > OpenItemTree_t;

	struct SearchResult_t
	{
		CDmeHandle< CDmElement >	handle;
		CUtlString					attributeName;

		bool operator == ( const SearchResult_t &other ) const
		{
			if ( &other == this )
				return true;

			if ( other.handle != handle )
				return false;
			if ( other.attributeName != attributeName )
				return false;

			return true;
		}
	};

	bool BuildExpansionListToFindElement_R( CUtlRBTree< CDmElement *, int >& visited, int depth, SearchResult_t& sr, CDmElement *owner, CDmElement *element, char const *attributeName, int arrayIndex, CUtlVector< int >& expandIndices );
	void FindMatchingElements_R( CUtlRBTree< CDmElement *, int >& visited, char const *searchstr, CDmElement *root, CUtlVector< SearchResult_t >& list );
	void NavigateToSearchResult();

	void SpewOpenItems( int depth, OpenItemTree_t &tree, int nOpenTreeIndex, int nItemIndex );

	// Finds the tree index of a child matching the particular element + attribute
	int FindTreeItem( int nParentIndex, const TreeItem_t &info );

	// Expands all items in the open item tree if they exist
	void ExpandOpenItems( OpenItemTree_t &tree, int nOpenTreeIndex, int nItemIndex, bool makeVisible );

	// Builds a list of open items
	void BuildOpenItemList( OpenItemTree_t &tree, int nParent, int nItemIndex, bool preservePrevSelectedItem );

	void FillInDataForItem( TreeItem_t &item, int nItemIndex );

	// Removes an item from the tree
	void RemoveItem( int nItemIndex );

	// Removes an item recursively
	void RemoveItem_R( int nItemIndex );

	// Adds a single entry into the tree
	void CreateTreeEntry( int parentNodeIndex, CDmElement* obj, CDmAttribute *pAttribute, int nArrayIndex, AttributeWidgets_t &entry );

	// Sets up the attribute widget init info for a particular attribute
	void SetupWidgetInfo( AttributeWidgetInfo_t *pInfo, CDmElement *obj, CDmAttribute *pAttribute, int nArrayIndex = -1 );

	// Creates an attribute data widget using a specifically requested widget
	vgui::Panel *CreateAttributeDataWidget( CDmElement *pElement, const char *pWidgetName, CDmElement *obj, CDmAttribute *pAttribute, int nArrayIndex = -1 );

	void UpdateTree();
	void InsertAttributes( int parentNodeIndex, CDmElement *obj );
	void InsertAttributeArrayMembers( int parentNodeIndex, CDmElement *obj, CDmAttribute *array );

	// Adds a single editable attribute of the element to the tree
	void InsertSingleAttribute( int parentNodeIndex, CDmElement *obj, CDmAttribute *pAttribute, int nArrayIndex = -1 );

	// Refreshes the tree view
	void RefreshTreeView( bool preservePrevSelectedItem = false );

	// Gets tree view text
	void GetTreeViewText( CDmElement* obj, CDmAttribute *pAttribute, int nArrayIndex, char *pBuffer, int nMaxLen, bool& editableText );

	void	RemoveSelected( bool selectLeft );

	void			AddToHistory( CDmElement *element );

	void			ValidateHistory();
	void			SpewHistory();
	void			JumpToHistoryItem();

	void			UpdateButtonState();

	KEYBINDING_FUNC( ondelete, KEY_DELETE, 0, OnKeyDelete, "#elementpropertiestree_ondelete_help", 0 );
	KEYBINDING_FUNC( onbackspace, KEY_BACKSPACE, 0, OnKeyBackspace, "#elementpropertiestree_ondelete_help", 0 );
	KEYBINDING_FUNC( onrefresh, KEY_F5, 0, OnRefresh, "#elementpropertiestree_onrefresh_help", 0 );

	MESSAGE_FUNC( OnRename, "OnRename" );
	MESSAGE_FUNC( OnRemove, "OnRemove" );
	MESSAGE_FUNC( OnClear, "OnClear" );
	MESSAGE_FUNC( OnSortByName, "OnSortByName" );

	MESSAGE_FUNC( OnCut, "OnCut" );
	MESSAGE_FUNC( OnCopy, "OnCopy" );
	MESSAGE_FUNC( OnPaste, "OnPaste" );
	MESSAGE_FUNC( OnPasteReference, "OnPasteReference" );
	MESSAGE_FUNC( OnPasteInsert, "OnPasteInsert" );
	MESSAGE_FUNC( OnDeleteSelected, "OnDelete" );

	MESSAGE_FUNC_INT( OnElementChangedExternally, "ElementChangedExternally", valuesOnly );
	MESSAGE_FUNC_INT( OnNavBack, "OnNavigateBack", item );
	MESSAGE_FUNC_INT( OnNavForward, "OnNavigateForward", item );
	MESSAGE_FUNC_INT( OnNavigateSearchAgain, "OnNavigateSearchAgain", direction );
	MESSAGE_FUNC( OnShowSearchResults, "OnShowSearchResults" );

protected:

	MESSAGE_FUNC_PARAMS( OnInputCompleted, "InputCompleted", params );
	MESSAGE_FUNC( OnAddItem, "OnAddItem" );

	MESSAGE_FUNC_PARAMS( OnSetShared, "OnSetShared", pParams );

	MESSAGE_FUNC_PARAMS( OnChangeFile, "OnChangeFile", pParams );

	MESSAGE_FUNC_PARAMS( OnShowFileDialog, "OnShowFileDialog", pParams );
	MESSAGE_FUNC_PARAMS( OnFileSelected, "FileSelected", params );

	enum DropOperation_t
	{
		DO_MOVE,
		DO_LINK,
		DO_COPY,
		DO_UNKNOWN,
	};

	DropOperation_t GetDropOperation( int itemIndex, CUtlVector< KeyValues * >& msglist );

	void						DropItemsIntoArray( CDmrElementArray<> &array,
													CUtlVector< KeyValues* > &msglist,
													CUtlVector< CDmElement* > &list,
													int nArrayIndex, DropOperation_t op );

	bool						OnRemoveFromData( CUtlVector< KeyValues * >& list );
	bool						OnRemoveFromData( KeyValues *item );
	void						OnPaste_( bool reference );
	bool						ShowAddAttributeDialog( CDmElement *pElement, const char *pAttributeType );
	void						AddAttribute( const char *pAttributeName, KeyValues *pContext );
	bool						ShowSetElementAttributeDialog( CDmElement *pOwner, const char *pAttributeName, int nArrayItem, const char *pElementType );
	void						SetElementAttribute( const char *pElementName, KeyValues *pContext );
	void						OnImportElement( const char *pFullPath, KeyValues *pContext );
	void						OnExportElement( const char *pFullPath, KeyValues *pContext );

	void GetPathToItem( CUtlVector< TreeItem_t > &path, int itemIndex );
	int OpenPath( const CUtlVector< TreeItem_t > &path );

	// Refreshes the color state of the tree
	void						RefreshTreeItemState( int nItemID );

	// Refreshes the color state of the tree
	void						SetTreeItemColor( int nItemID, CDmElement *pEntryElement, bool bIsElementArrayItem, bool bEditableLabel );

	CDmeHandle< CDmeEditorTypeDictionary >	m_hTypeDictionary;
	CUtlVector< AttributeWidgets_t >		m_AttributeWidgets;
	IDmNotify					*m_pNotify;
	CDmeHandle< CDmElement >	m_hObject;
	CElementTreeViewListControl	*m_pTree;
	bool						m_bAutoApply;
	bool						m_bShowMemoryUsage;
	vgui::DHANDLE< vgui::Menu >	m_hContextMenu;

	CPropertiesTreeToolbar		*m_pToolBar; // Forward/backward navigation and search fields

	enum
	{
		DME_PROPERTIESTREE_MAXHISTORYITEMS = 32,
		DME_PROPERTIESTREE_MAXSEARCHHISTORYITEMS = 32,
	};

	// Most recent are at the head
	CUtlVector< CDmeHandle< CDmElement > >	m_hHistory;
	int										m_nCurrentHistoryPosition;
	bool									m_bSuppressHistoryUpdates;
	char									m_szSearchStr[ 128 ];

	CUtlVector< SearchResult_t >			m_SearchResults;
	int										m_nCurrentSearchResult;

	CUtlVector< CUtlString >				m_SearchHistory;

	CDmeHandle< CDmElement >				m_SearchResultsRoot;

	vgui::HCursor							m_hDragCopyCursor;
	vgui::HCursor							m_hDragLinkCursor;
	vgui::HCursor							m_hDragMoveCursor;
};


//-----------------------------------------------------------------------------
// CElementPropertiesTree 
//-----------------------------------------------------------------------------
class CElementPropertiesTree : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CElementPropertiesTree, vgui::Frame );
public:

	CElementPropertiesTree( vgui::Panel *parent, IDmNotify *pNotify, CDmElement *pObject, CDmeEditorTypeDictionary *pDict = NULL );

	virtual void Init( );
	virtual void Refresh( CElementPropertiesTreeInternal::RefreshType_t rebuild = CElementPropertiesTreeInternal::REFRESH_REBUILD, bool preservePrevSelectedItem = false );
	virtual void GenerateChildrenOfNode( int itemIndex );
	virtual void SetObject( CDmElement *object );
	virtual void OnCommand( const char *cmd );
	virtual void ActivateBuildMode();

	CElementPropertiesTreeInternal *GetInternal();

protected:
	CElementPropertiesTreeInternal	 *m_pProperties;
	vgui::Button		*m_pOK;
	vgui::Button		*m_pApply;
	vgui::Button		*m_pCancel;
};


//-----------------------------------------------------------------------------
// Inline methods
//-----------------------------------------------------------------------------
inline CElementPropertiesTreeInternal *CElementPropertiesTree::GetInternal()
{
	return m_pProperties;
}


//-----------------------------------------------------------------------------
// Wrapper panel to hook into DmePanels
//-----------------------------------------------------------------------------
class CDmeElementPanel : public CElementPropertiesTreeInternal, public IDmNotify
{
	DECLARE_CLASS_SIMPLE( CDmeElementPanel, CElementPropertiesTreeInternal );

public:
	CDmeElementPanel( vgui::Panel *pParent, const char *pPanelName );

	void SetDmeElement( CDmElement *pElement );

	// Inherited from IDmNotify
	virtual void NotifyDataChanged( const char *pReason, int nNotifySource, int nNotifyFlags );
};

#endif // ELEMENTPROPERTIESTREE_H
