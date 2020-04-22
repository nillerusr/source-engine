//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef BUYPRESET_LISTBOX_H
#define BUYPRESET_LISTBOX_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/Panel.h>

#include <utlvector.h>

//--------------------------------------------------------------------------------------------------------------
/**
 *  ListBox-style control with behavior needed by weapon lists for BuyPreset editing
 */
class BuyPresetListBox : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( BuyPresetListBox, vgui::Panel );

public:
	BuyPresetListBox( vgui::Panel *parent, char const *panelName );
	~BuyPresetListBox();

	virtual int AddItem( vgui::Panel *panel, void * userData );	///< Adds an item to the end of the listbox.  UserData is assumed to be a pointer that can be freed by the listbox if non-NULL.
	virtual int	GetItemCount( void ) const;						///< Returns the number of items in the listbox
	void SwapItems( int index1, int index2 );					///< Exchanges two items in the listbox
	void MakeItemVisible( int index );							///< Try to ensure that the given index is visible

	vgui::Panel * GetItemPanel( int index ) const;				///< Returns the panel in the given index, or NULL
	void * GetItemUserData( int index );						///< Returns the userData in the given index, or NULL
	void SetItemUserData( int index, void * userData );			///< Sets the userData in the given index

	virtual void RemoveItem( int index );						///< Removes an item from the table (changing the indices of all following items), deleting the panel and userData
	virtual void DeleteAllItems();								///< clears the listbox, deleting all panels and userData

	// overrides
	virtual void OnSizeChanged(int wide, int tall);				////< Handles size changes
	MESSAGE_FUNC_INT( OnSliderMoved, "ScrollBarSliderMoved", position );	///< Handles slider being dragged
	virtual void OnMouseWheeled(int delta);						///< Scrolls the list according to the mouse wheel movement
	virtual void MoveScrollBarToTop();							///< Moves slider to the top

protected:

	virtual int	computeVPixelsNeeded( void );					///< Computes vertical pixels needed by listbox contents

	virtual void PerformLayout();								///< Positions listbox items, etc after internal changes
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );	///< Loads colors, fonts, etc

	virtual void OnCommand( const char *command );				///< Passes commands up to the parent

private:
	enum { SCROLLBAR_SIZE = 18, DEFAULT_HEIGHT = 24, PANELBUFFER = 5 };

	typedef struct dataitem_s
	{
		vgui::Panel *panel;
		void * userData;
	} DataItem;
	CUtlVector< DataItem >	m_items;

	vgui::ScrollBar			*m_vbar;
	vgui::Panel				*m_pPanelEmbedded;

	int						m_iScrollbarSize;
	int						m_iDefaultHeight;
	int						m_iPanelBuffer;

	int						m_visibleIndex;
	int						m_lastSize;
};

#endif // BUYPRESET_LISTBOX_H
