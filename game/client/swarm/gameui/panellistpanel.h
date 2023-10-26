//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#if !defined( PANELLISTPANEL_H )
#define PANELLISTPANEL_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/Panel.h>

class KeyValues;


//-----------------------------------------------------------------------------
// Purpose: A list of variable height child panels
//-----------------------------------------------------------------------------
class CPanelListPanel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CPanelListPanel, vgui::Panel ); 

public:
	typedef struct dataitem_s
	{
		// Always store a panel pointer
		vgui::Panel *panel;
	} DATAITEM;

	CPanelListPanel( vgui::Panel *parent, char const *panelName, bool inverseButtons = false );
	~CPanelListPanel();

	// DATA & ROW HANDLING
	// The list now owns the panel
	virtual int	computeVPixelsNeeded( void );
	virtual int AddItem( vgui::Panel *panel );
	virtual int	GetItemCount( void );
	virtual vgui::Panel *GetItem(int itemIndex); // returns pointer to data the row holds
	virtual void RemoveItem(int itemIndex); // removes an item from the table (changing the indices of all following items)
	virtual void DeleteAllItems(); // clears and deletes all the memory used by the data items

	// career-mode UI wants to nudge sub-controls around
	void SetSliderYOffset(int pixels);

	// PAINTING
	virtual vgui::Panel *GetCellRenderer( int row );

	MESSAGE_FUNC_INT( OnSliderMoved, "ScrollBarSliderMoved", position );

	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);

	vgui::Panel *GetEmbedded()
	{
		return _embedded;
	}

protected:

	DATAITEM	*GetDataItem( int itemIndex );

	virtual void PerformLayout();
	virtual void PaintBackground();
	virtual void OnMouseWheeled(int delta);

private:
	// list of the column headers
	vgui::Dar<DATAITEM *>	_dataItems;
	vgui::ScrollBar		*_vbar;
	vgui::Panel			*_embedded;

	int					_tableStartX;
	int					_tableStartY;
	int					_sliderYOffset;
};

#endif // PANELLISTPANEL_H