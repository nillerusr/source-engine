//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=============================================================================

#ifndef DUAL_PANEL_LIST_H
#define DUAL_PANEL_LIST_H

#if defined( _WIN32 )
#pragma once
#endif

#include <utllinkedlist.h>
#include <utlvector.h>
#include <vgui/VGUI.h>
#include <vgui_controls/Panel.h>

class KeyValues;

//-----------------------------------------------------------------------------
// Purpose: A list of variable height child panels
//  each list item consists of a label-panel pair. Height of the item is
// determined from the lable.
//-----------------------------------------------------------------------------
class CDualPanelList : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CDualPanelList, vgui::Panel );

public:
	CDualPanelList( vgui::Panel *parent, char const *panelName );
	~CDualPanelList();

	// DATA & ROW HANDLING
	// The list now owns the panel
	virtual int AddItem( vgui::Panel *labelPanel, vgui::Panel *panel );
	int	GetItemCount() const;
	int GetItemIDFromRow( int nRow ) const;

	// Iteration. Use these until they return InvalidItemID to iterate all the items.
	int FirstItem() const;
	int NextItem( int nItemID ) const;
	int InvalidItemID() const;

	virtual Panel *GetItemLabel( int itemID ); 
	virtual Panel *GetItemPanel( int itemID ); 
	virtual bool IsItemVisible( int nItemID ) const;
	virtual void SetItemVisible( int nItemID, bool bVisible );

//	vgui::ScrollBar *GetScrollbar() { return m_pScrollBar; }

	virtual void RemoveItem( int itemID );		// removes an item from the table (changing the indices of all following items)
	virtual void DeleteAllItems();				// clears and deletes all the memory used by the data items
	void RemoveAll();

	// painting
	virtual vgui::Panel *GetCellRenderer( int row );

	// layout
	void SetFirstColumnWidth( int width );
	int GetFirstColumnWidth();
	void SetNumColumns( int iNumColumns );
	int GetNumColumns( void );
//	void MoveScrollBarToTop();

	// selection
	void SetSelectedPanel( vgui::Panel *panel );
	Panel *GetSelectedPanel();
	/*
		On a panel being selected, a message gets sent to it
			"PanelSelected"		int "state"
		where state is 1 on selection, 0 on deselection
	*/

	void		SetVerticalBufferPixels( int buffer );

	void		ScrollToItem( int itemNumber );

	CUtlVector< int > *GetSortedVector( void )
	{
		return &m_SortedItems;
	}

protected:
	// overrides
	virtual void OnSizeChanged(int wide, int tall);
	MESSAGE_FUNC_INT( OnSliderMoved, "ScrollBarSliderMoved", position );
	virtual void PerformLayout();
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void OnMouseWheeled(int delta);

private:
	int	ComputeVPixelsNeeded();

	enum { DEFAULT_HEIGHT = 24, PANELBUFFER = 5 };

	class CDataItem
	{
	public:
		CDataItem()
		: m_bVisible( true )
		{
		}

		void SetVisible( int bVisible )
		{
			m_bVisible = bVisible;

			if ( panel )
			{
				panel->SetVisible( m_bVisible );
			}

			if ( labelPanel )
			{
				labelPanel->SetVisible( m_bVisible );
			}
		}

		bool IsVisible() const { return m_bVisible; }

		// Always store a panel pointer
		vgui::Panel *panel;
		vgui::Panel *labelPanel;
		bool m_bVisible;
	};

	// list of the column headers

	CUtlLinkedList< CDataItem, int>		m_DataItems;
	CUtlVector<int>						m_SortedItems;

	vgui::ScrollBar				*m_pScrollBar;
	vgui::Panel					*m_pPanelEmbedded;

	vgui::PHandle					m_hSelectedItem;
	int						m_iFirstColumnWidth;
	int						m_nNumColumns;
	int						m_iDefaultHeight;
	int						m_iPanelBuffer;

//	CPanelAnimationVar( bool, m_bAutoHideScrollbar, "autohide_scrollbar", "0" );
};


#endif // DUAL_PANEL_LIST_H
