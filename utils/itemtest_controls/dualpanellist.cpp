//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=============================================================================


// Valve includes
#include "tier1/KeyValues.h"

#include "vgui/MouseCode.h"
#include "vgui/IInput.h"
#include "vgui/IScheme.h"
#include "vgui/ISurface.h"

#include "vgui_controls/EditablePanel.h"
#include "vgui_controls/ScrollBar.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/Button.h"
#include "vgui_controls/Controls.h"


// Local includes
#include "dualpanellist.h"


// Last include
#include "tier0/memdbgon.h"


//=============================================================================
//
//=============================================================================
CDualPanelList::CDualPanelList( vgui::Panel *pParent, char const *pszPanelName )
: vgui::Panel( pParent, pszPanelName )
, m_pScrollBar( NULL )
{
	SetBounds( 0, 0, 100, 100 );

	if ( false )
	{
		m_pScrollBar = new vgui::ScrollBar( this, "CDualPanelListVScroll", true );
		m_pScrollBar->AddActionSignalTarget( this );
	}

	m_pPanelEmbedded = new vgui::EditablePanel( this, "PanelListEmbedded" );
	m_pPanelEmbedded->SetBounds( 0, 0, 20, 20 );
	m_pPanelEmbedded->SetPaintBackgroundEnabled( false );
	m_pPanelEmbedded->SetPaintBorderEnabled( false );

	m_iFirstColumnWidth = 100; // default width
	m_nNumColumns = 1; // 1 column by default

	if ( false && IsProportional() )
	{
		m_iDefaultHeight = vgui::scheme()->GetProportionalScaledValueEx( GetScheme(), DEFAULT_HEIGHT );
		m_iPanelBuffer = vgui::scheme()->GetProportionalScaledValueEx( GetScheme(), PANELBUFFER );
	}
	else
	{
		m_iDefaultHeight = DEFAULT_HEIGHT;
		m_iPanelBuffer = PANELBUFFER;
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CDualPanelList::~CDualPanelList()
{
	// free data from table
	DeleteAllItems();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDualPanelList::SetVerticalBufferPixels( int buffer )
{
	m_iPanelBuffer = buffer;
	InvalidateLayout();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int	CDualPanelList::ComputeVPixelsNeeded()
{
	int iCurrentItem = 0;
	int iLargestH = 0;

	int nPixels = 0;
	for ( int i = 0; i < m_SortedItems.Count(); i++ )
	{
		Panel *pPanel = m_DataItems[ m_SortedItems[i] ].panel;
		if ( !pPanel || !pPanel->IsVisible() )
			continue;

		if ( pPanel->IsLayoutInvalid() )
		{
			pPanel->InvalidateLayout( true );
		}

		int iCurrentColumn = iCurrentItem % m_nNumColumns;

		int w, h;
		pPanel->GetSize( w, h );

		if ( iLargestH < h )
			iLargestH = h;

		if ( iCurrentColumn == 0 )
			nPixels += m_iPanelBuffer; // add in buffer. between rows.

		if ( iCurrentColumn >= m_nNumColumns - 1 )
		{
			nPixels += iLargestH;
			iLargestH = 0;
		}

		iCurrentItem++;
	}

	// Add in remaining largest height
	nPixels += iLargestH;

	nPixels += m_iPanelBuffer; // add in buffer below last item

	/*
	nPixels = 0;
	for ( int i = 0; i < m_SortedItems.Count(); ++i )
	{
		Panel *pPanel = m_DataItems[ m_SortedItems[i] ].panel;
		if ( !pPanel )
			continue;

		int nWide = 0;
		int nHeight = 0;
		pPanel->GetSize( nWide, nHeight );

		nPixels += nHeight;
	}
	*/

	return nPixels;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
vgui::Panel *CDualPanelList::GetCellRenderer( int row )
{
	if ( !m_SortedItems.IsValidIndex(row) )
		return NULL;

	Panel *panel = m_DataItems[ m_SortedItems[row] ].panel;
	return panel;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int CDualPanelList::AddItem( vgui::Panel *labelPanel, vgui::Panel *panel)
{
	Assert(panel);

	if ( labelPanel )
	{
		labelPanel->SetParent( m_pPanelEmbedded );
	}

	panel->SetParent( m_pPanelEmbedded );

	int itemID = m_DataItems.AddToTail();
	CDataItem &newitem = m_DataItems[itemID];
	newitem.labelPanel = labelPanel;
	newitem.panel = panel;
	m_SortedItems.AddToTail(itemID);

	InvalidateLayout();
	return itemID;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int	CDualPanelList::GetItemCount() const
{
	return m_DataItems.Count();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int CDualPanelList::GetItemIDFromRow( int nRow ) const
{
	if ( nRow < 0 || nRow >= GetItemCount() )
		return m_DataItems.InvalidIndex();

	return m_SortedItems[ nRow ];
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int CDualPanelList::FirstItem() const
{
	return m_DataItems.Head();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int CDualPanelList::NextItem( int nItemID ) const
{
	return m_DataItems.Next( nItemID );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int CDualPanelList::InvalidItemID() const
{
	return m_DataItems.InvalidIndex();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
vgui::Panel *CDualPanelList::GetItemLabel(int nItemID)
{
	if ( !m_DataItems.IsValidIndex( nItemID ) )
		return NULL;

	return m_DataItems[nItemID].labelPanel;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
vgui::Panel *CDualPanelList::GetItemPanel( int nItemID )
{
	if ( !m_DataItems.IsValidIndex( nItemID ) )
		return NULL;

	return m_DataItems[nItemID].panel;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDualPanelList::IsItemVisible( int nItemID ) const
{
	if ( !m_DataItems.IsValidIndex( nItemID ) )
		return true;

	return m_DataItems[nItemID].IsVisible();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDualPanelList::SetItemVisible( int nItemID, bool bVisible )
{
	if ( !m_DataItems.IsValidIndex( nItemID ) )
		return;

	m_DataItems[nItemID].SetVisible( bVisible );

	InvalidateLayout();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDualPanelList::RemoveItem( int nItemID )
{
	if ( !m_DataItems.IsValidIndex( nItemID ) )
		return;

	CDataItem &item = m_DataItems[nItemID];
	if ( item.panel )
	{
		item.panel->MarkForDeletion();
	}

	if ( item.labelPanel )
	{
		item.labelPanel->MarkForDeletion();
	}

	m_DataItems.Remove( nItemID );
	m_SortedItems.FindAndRemove( nItemID );

	InvalidateLayout();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDualPanelList::DeleteAllItems()
{
	FOR_EACH_LL( m_DataItems, i )
	{
		if ( m_DataItems[i].panel )
		{
			delete m_DataItems[i].panel;
		}
	}

	m_DataItems.RemoveAll();
	m_SortedItems.RemoveAll();

	InvalidateLayout();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDualPanelList::RemoveAll()
{
	m_DataItems.RemoveAll();
	m_SortedItems.RemoveAll();

//	m_pScrollBar->SetValue( 0 );
	InvalidateLayout();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDualPanelList::OnSizeChanged( int nWide, int nTall )
{
	BaseClass::OnSizeChanged( nWide, nTall );
	InvalidateLayout();
	Repaint();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDualPanelList::PerformLayout()
{
	int nWide, nTall;
	GetSize( nWide, nTall );

	int vpixels = ComputeVPixelsNeeded();

	int top = 0;
	if ( m_pScrollBar )
	{

		m_pScrollBar->SetRange( 0, vpixels );
		m_pScrollBar->SetRangeWindow( nTall );
		m_pScrollBar->SetButtonPressedScrollValue( nTall / 4 ); // standard height of labels/buttons etc.

		m_pScrollBar->SetPos( nWide - m_pScrollBar->GetWide() - 2, 0 );
		m_pScrollBar->SetSize( m_pScrollBar->GetWide(), nTall - 2 );

		top = m_pScrollBar->GetValue();
	}

	m_pPanelEmbedded->SetPos( 1, -top );
	if ( m_pScrollBar )
	{
		m_pPanelEmbedded->SetSize( nWide - m_pScrollBar->GetWide() - 2, vpixels );
	}
	else
	{
		m_pPanelEmbedded->SetSize( nWide - 2, vpixels );
	}

	SetSize( nWide, vpixels );

	/*
	bool bScrollbarVisible = true;
	// If we're supposed to automatically hide the scrollbar when unnecessary, check it now
	if ( m_bAutoHideScrollbar )
	{
		bScrollbarVisible = (m_pPanelEmbedded->GetTall() > nTall);
	}
	m_pScrollBar->SetVisible( bScrollbarVisible );
	*/

	// Now lay out the controls on the embedded panel
	int y = 0;
	int h = 0;
	int totalh = 0;

	int xpos = m_iFirstColumnWidth + m_iPanelBuffer;
	int iColumnWidth = ( nWide - xpos - 12 ) / m_nNumColumns;
	if ( m_pScrollBar )
	{
		iColumnWidth = ( nWide - xpos - m_pScrollBar->GetWide() - 12 ) / m_nNumColumns;
	}

	for ( int i = 0; i < m_SortedItems.Count(); i++ )
	{
		CDataItem &item = m_DataItems[ m_SortedItems[i] ];
		if ( !item.IsVisible() )
			continue;

		int iCurrentColumn = i % m_nNumColumns;

		// add in a little buffer between panels
		if ( iCurrentColumn == 0 )
			y += m_iPanelBuffer;

		if ( h < item.panel->GetTall() )
			h = item.panel->GetTall();

		if ( item.labelPanel )
		{
			item.labelPanel->SetBounds( 0, y, m_iFirstColumnWidth, item.panel->GetTall() );
		}

		item.panel->SetBounds( xpos + iCurrentColumn * iColumnWidth, y, iColumnWidth, item.panel->GetTall() );

		if ( iCurrentColumn >= m_nNumColumns - 1 )
		{
			y += h;
			totalh += h;

			h = 0;
		}
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDualPanelList::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	SetBorder( pScheme->GetBorder( "ButtonDepressedBorder" ) );
	SetBgColor( GetSchemeColor( "ListPanel.BgColor", GetBgColor(), pScheme ) );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDualPanelList::OnSliderMoved( int /* nPosition */ )
{
	InvalidateLayout();
	Repaint();
}


/*
//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDualPanelList::MoveScrollBarToTop()
{
	m_pScrollBar->SetValue( 0 );
}
*/


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDualPanelList::SetFirstColumnWidth( int nWidth )
{
	m_iFirstColumnWidth = nWidth;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int CDualPanelList::GetFirstColumnWidth()
{
	return m_iFirstColumnWidth;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDualPanelList::SetNumColumns( int nNumColumns )
{
	m_nNumColumns = nNumColumns;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int CDualPanelList::GetNumColumns()
{
	return m_nNumColumns;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDualPanelList::OnMouseWheeled( int nDelta )
{
	if ( !m_pScrollBar )
		return;

	int nVal = m_pScrollBar->GetValue();
	nVal -= ( nDelta * DEFAULT_HEIGHT );
	m_pScrollBar->SetValue( nVal );	
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDualPanelList::SetSelectedPanel( vgui::Panel *pPanel )
{
	if ( pPanel != m_hSelectedItem )
	{
		// notify the panels of the selection change
		if ( m_hSelectedItem )
		{
			PostMessage( m_hSelectedItem.Get(), new KeyValues( "PanelSelected", "state", 0 ) );
		}

		if ( pPanel )
		{
			PostMessage( pPanel, new KeyValues( "PanelSelected", "state", 1 ) );
		}

		m_hSelectedItem = pPanel;
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
vgui::Panel *CDualPanelList::GetSelectedPanel()
{
	return m_hSelectedItem;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDualPanelList::ScrollToItem( int nItemNumber )
{
	if ( !m_pScrollBar || !m_pScrollBar->IsVisible() )
		return;

	CDataItem& item = m_DataItems[ m_SortedItems[ nItemNumber ] ];
	if ( !item.panel )
		return;

	int x, y;
	item.panel->GetPos( x, y );
	int lx, ly;
	lx = x;
	ly = y;
	m_pPanelEmbedded->LocalToScreen( lx, ly );
	ScreenToLocal( lx, ly );

	int h = item.panel->GetTall();

	if ( ly >= 0 && ly + h < GetTall() )
		return;

	m_pScrollBar->SetValue( y );
	InvalidateLayout();
}
