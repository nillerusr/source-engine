//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"

#include <KeyValues.h>
#include <vgui/MouseCode.h>
#include <vgui/IInput.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>

#include <vgui_controls/EditablePanel.h>
#include <vgui_controls/ScrollBar.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/Controls.h>
#include "buypreset_listbox.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

#ifndef max
#define max(a,b)            (((a) > (b)) ? (a) : (b))
#endif


//--------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------
BuyPresetListBox::BuyPresetListBox( vgui::Panel *parent, char const *panelName ) : Panel( parent, panelName )
{
	m_visibleIndex = 0;
	m_lastSize = 0;

	SetBounds( 0, 0, 100, 100 );

	m_vbar = new ScrollBar(this, "PanelListPanelVScroll", true);
	m_vbar->SetBounds( 0, 0, 20, 20 );
	m_vbar->SetVisible(true);
	m_vbar->AddActionSignalTarget( this );

	m_pPanelEmbedded = new EditablePanel(this, "PanelListEmbedded");
	m_pPanelEmbedded->SetBounds(0, 0, 20, 20);
	m_pPanelEmbedded->SetPaintBackgroundEnabled( false );
	m_pPanelEmbedded->SetPaintBorderEnabled(false);

	if( IsProportional() )
	{
		int width, height;
		int sw,sh;
		surface()->GetProportionalBase( width, height );
		GetHudSize(sw, sh);

		// resize scrollbar, etc
		m_iScrollbarSize = static_cast<int>( static_cast<float>( SCROLLBAR_SIZE )*( static_cast<float>( sw )/ static_cast<float>( width )));
		m_iDefaultHeight = static_cast<int>( static_cast<float>( DEFAULT_HEIGHT )*( static_cast<float>( sw )/ static_cast<float>( width )));
		m_iPanelBuffer = static_cast<int>( static_cast<float>( PANELBUFFER )*( static_cast<float>( sw )/ static_cast<float>( width )));
	}
	else
	{
		m_iScrollbarSize = SCROLLBAR_SIZE;
		m_iDefaultHeight = DEFAULT_HEIGHT;
		m_iPanelBuffer = PANELBUFFER;
	}
}

//--------------------------------------------------------------------------------------------------------------
BuyPresetListBox::~BuyPresetListBox()
{
	// free data from table
	DeleteAllItems();
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Passes commands up to the parent
 */
void BuyPresetListBox::OnCommand( const char *command )
{
	GetParent()->OnCommand( command );
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Scrolls the list according to the mouse wheel movement
 */
void BuyPresetListBox::OnMouseWheeled(int delta)
{
	int scale = 3;
	if ( m_items.Count() )
	{
		Panel *panel = m_items[0].panel;
		if ( panel )
		{
			scale = panel->GetTall() + m_iPanelBuffer;
		}
	}
	int val = m_vbar->GetValue();
	val -= (delta * scale);
	m_vbar->SetValue(val);
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Computes vertical pixels needed by listbox contents
 */
int	BuyPresetListBox::computeVPixelsNeeded( void )
{
	int pixels = 0;

	int i; 
	for ( i = 0; i < m_items.Count(); i++ )
	{
		Panel *panel = m_items[i].panel;
		if ( !panel )
			continue;

		int w, h;
		panel->GetSize( w, h );

		pixels += m_iPanelBuffer; // add in buffer. between items.
		pixels += h;	
	}

	pixels += m_iPanelBuffer; // add in buffer below last item

	return pixels;
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Adds an item to the end of the listbox.  UserData is assumed to be a pointer that can be freed by the listbox if non-NULL.
 */
int BuyPresetListBox::AddItem( vgui::Panel *panel, void * userData )
{
	assert(panel);

	DataItem item = { panel, userData };

	panel->SetParent( m_pPanelEmbedded );

	m_items.AddToTail( item );

	InvalidateLayout();
	return m_items.Count();
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Exchanges two items in the listbox
 */
void BuyPresetListBox::SwapItems( int index1, int index2 )
{
	if ( index1 < 0 || index2 < 0 || index1 >= m_items.Count() || index2 >= m_items.Count() )
	{
		return;
	}

	DataItem temp = m_items[index1];
	m_items[index1] = m_items[index2];
	m_items[index2] = temp;

	InvalidateLayout();
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Returns the number of items in the listbox
 */
int	BuyPresetListBox::GetItemCount( void ) const
{
	return m_items.Count();
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Returns the panel in the given index, or NULL
 */
Panel * BuyPresetListBox::GetItemPanel(int index) const
{
	if ( index < 0 || index >= m_items.Count() )
		return NULL;

	return m_items[index].panel;
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Returns the userData in the given index, or NULL
 */
void * BuyPresetListBox::GetItemUserData(int index)
{
	if ( index < 0 || index >= m_items.Count() )
	{
		return NULL;
	}

	return m_items[index].userData;
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Sets the userData in the given index
 */
void BuyPresetListBox::SetItemUserData( int index, void * userData )
{
	if ( index < 0 || index >= m_items.Count() )
		return;

	m_items[index].userData = userData;
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Removes an item from the table (changing the indices of all following items), deleting the panel and userData
 */
void BuyPresetListBox::RemoveItem(int index)
{
	if ( index < 0 || index >= m_items.Count() )
		return;

	DataItem item = m_items[index];
	if ( item.panel )
	{
		item.panel->MarkForDeletion();
	}
	if ( item.userData )
	{
		delete item.userData;
	}

	m_items.Remove( index );

	InvalidateLayout();
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  clears the listbox, deleting all panels and userData
 */
void BuyPresetListBox::DeleteAllItems()
{
	while ( m_items.Count() )
	{
		RemoveItem( 0 );
	}

	// move the scrollbar to the top of the list
	m_vbar->SetValue(0);
	InvalidateLayout();
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Handles Count changes
 */
void BuyPresetListBox::OnSizeChanged(int wide, int tall)
{
	BaseClass::OnSizeChanged(wide, tall);
	InvalidateLayout();
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Positions listbox items, etc after internal changes
 */
void BuyPresetListBox::PerformLayout()
{
	int wide, tall;
	GetSize( wide, tall );

	int vpixels = computeVPixelsNeeded();

	int visibleIndex = m_visibleIndex;

	//!! need to make it recalculate scroll positions
	m_vbar->SetVisible(true);
	m_vbar->SetEnabled(false);
	m_vbar->SetRange( 0, (MAX( 0, vpixels - tall + m_iDefaultHeight ))  );
	m_vbar->SetRangeWindow( m_iDefaultHeight );
	m_vbar->SetButtonPressedScrollValue( m_iDefaultHeight ); // standard height of labels/buttons etc.
	m_vbar->SetPos(wide - m_iScrollbarSize, 1);
	m_vbar->SetSize(m_iScrollbarSize, tall - 2);

	m_visibleIndex = visibleIndex;

	int top = MAX( 0, m_vbar->GetValue() );

	m_pPanelEmbedded->SetPos( 1, -top );
	m_pPanelEmbedded->SetSize( wide-m_iScrollbarSize -2, vpixels );

	// Now lay out the controls on the embedded panel
	int y = 0;
	int h = 0;
	int totalh = 0;
	
	int i;
	for ( i = 0; i < m_items.Count(); i++, y += h )
	{
		// add in a little buffer between panels
		y += m_iPanelBuffer;
		DataItem item = m_items[i];

		h = item.panel->GetTall();

		totalh += h;
		item.panel->SetBounds( 8, y, wide - m_iScrollbarSize - 8 - 8, h );
		item.panel->InvalidateLayout();
	}

	if ( m_visibleIndex >= 0 && m_visibleIndex < m_items.Count() )
	{

		int vpos = 0;

		int tempWide, tempTall;
		GetSize( tempWide, tempTall );

		int vtop, vbottom;
		m_vbar->GetRange( vtop, vbottom );

		int tempTop = MAX( 0, m_vbar->GetValue() ); // top pixel in the embedded panel
		int bottom = tempTop + tempTall - 2;

		int itemTop, itemLeft, itemBottom, itemRight;
		m_items[m_visibleIndex].panel->GetBounds( itemLeft, itemTop, itemRight, itemBottom );
		itemBottom += itemTop;
		itemRight += itemLeft;

		if ( itemTop < tempTop )
		{
			// item's top is too high
			vpos -= ( tempTop - itemTop );

			m_vbar->SetValue(vpos);
			OnSliderMoved(vpos);
		}
		else if ( itemBottom > bottom )
		{
			// item's bottom is too low
			vpos += ( itemBottom - bottom );

			m_vbar->SetValue(vpos);
			OnSliderMoved(vpos);
		}
	}

	if ( m_lastSize == vpixels )
	{
		m_visibleIndex = -1;
	}
	m_lastSize = vpixels;
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Try to ensure that the given index is visible
 */
void BuyPresetListBox::MakeItemVisible( int index )
{
	m_visibleIndex = index;
	m_lastSize = 0;
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Loads colors, fonts, etc
 */
void BuyPresetListBox::ApplySchemeSettings(IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	SetBgColor(GetSchemeColor("BuyPresetListBox.BgColor", GetBgColor(), pScheme));

	SetBorder(pScheme->GetBorder("BrowserBorder"));
	m_vbar->SetBorder(pScheme->GetBorder("BrowserBorder"));

	PerformLayout();
}


//--------------------------------------------------------------------------------------------------------------
/**
 *  Handles slider being dragged
 */
void BuyPresetListBox::OnSliderMoved( int position )
{
	InvalidateLayout();
	Repaint();
}


//--------------------------------------------------------------------------------------------------------------
/**
 *  Moves slider to the top
 */
void BuyPresetListBox::MoveScrollBarToTop()
{
	m_vbar->SetValue(0);
	OnSliderMoved(0);
}

