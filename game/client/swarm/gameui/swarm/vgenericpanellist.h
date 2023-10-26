//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __VGENERICPANELLIST_H__
#define __VGENERICPANELLIST_H__

#include "basemodui.h"

namespace BaseModUI {

class GenericPanelList;

typedef bool __cdecl GPL_LHS_less_RHS(const vgui::Panel &item1, const vgui::Panel &item2);
typedef bool __cdecl GPL_SHOW_ITEM(const vgui::Panel &item);

class GenericPanelList : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( GenericPanelList, vgui::Panel );

public:
	enum ITEM_SELECTION_MODE { ISM_PERITEM = 1, ISM_ELEVATOR = 2, ISM_ALPHA_INVISIBLE = 4 };
	GenericPanelList(vgui::Panel *parent, const char *panelName, int selectionModeMask);
	virtual ~GenericPanelList();

	virtual void OnKeyCodePressed(vgui::KeyCode code);

	template <class PanelItemClass>
	PanelItemClass* AddPanelItem(const char* panelName)
	{
		PanelItemClass* newItem = new PanelItemClass(this, panelName);
		AddPanelItem( newItem, true );
		return newItem;
	}

	template <class PanelItemClass>
	PanelItemClass* AddPanelItem(const char* panelName, const char* text)
	{
		PanelItemClass* newItem = new PanelItemClass(this, panelName, text);
		AddPanelItem( newItem, true );
		return newItem;
	}

	template <class PanelItemClass>
	PanelItemClass* AddPanelItem(const char* panelName, const char* text, 
		vgui::Panel* commandReceiver, const char* command)
	{
		PanelItemClass* newItem = new PanelItemClass(this, panelName, text, commandReceiver, command);
		AddPanelItem( newItem, true );
		return newItem;
	}
	virtual unsigned short AddPanelItem(vgui::Panel* panelItem, bool bNeedsInvalidateScheme );
	virtual void MovePanelItemToBottom( vgui::Panel* panelItem );
	virtual void SortPanelItems( int (__cdecl *pfnCompare)( vgui::Panel* const *, vgui::Panel* const *) );

	virtual bool RemovePanelItem(unsigned short index, bool bDeletePanel = true);
	virtual void RemoveAllPanelItems();

	vgui::Panel* GetSelectedPanelItem();

	unsigned short GetPanelItemCount();
	vgui::Panel* GetPanelItem(unsigned short index);
	bool GetPanelItemIndex(vgui::Panel* panelItem, unsigned short& panelItemIndex);

	enum SEARCH_DIRECTION { SD_UP, SD_DOWN };
	virtual bool SelectPanelItem(unsigned short index, SEARCH_DIRECTION direction = SD_DOWN, bool scrollToItem = true, bool bAllowStealFocus = true);
	virtual bool SelectPanelItemByPanel( Panel *pPanelItem );
	virtual void ScrollToPanelItem(unsigned short index);

	virtual void NavigateTo();
	virtual void NavigateFrom();
	virtual void NavigateToChild( Panel *pNavigateTo );

	virtual void Sort(GPL_LHS_less_RHS* sortFunction);
	virtual void Filter(GPL_SHOW_ITEM* filterFunction);
	virtual bool GetScrollArrowsVisible( );
	virtual void SetScrollBarVisible( bool visible );
	virtual void SetScrollArrowsVisible( bool visible );
	virtual void OnMouseWheeled(int delta);

	void SetSchemeBgColorName(const char* schemeBgColorName);

	unsigned short GetLastItemAdded();

	int GetFirstVisibleItemNumber( bool bRequireFullyVisible = false ); //returns -1 if it can't find one
	int GetLastVisibleItemNumber( bool bRequireFullyVisible = IsX360() );  //returns -1 if it can't find one
	bool IsPanelItemVisible( Panel *pPanelItem, bool bRequireFullyVisible = IsX360() );

	MESSAGE_FUNC_CHARPTR( OnItemSelected, "OnItemSelected", panelName );
	MESSAGE_FUNC_CHARPTR( OnItemAdded, "OnItemAdded", panelName );
	MESSAGE_FUNC_CHARPTR( OnItemRemoved, "OnItemRemoved", panelName );
	MESSAGE_FUNC( OnSliderMoved, "ScrollBarSliderMoved" );
	void ShowScrollProgress( bool bShow ) { m_bShowScrollProgress = bShow; }

	MESSAGE_FUNC( OnChildResized, "ChildResized" );

	void RelinkNavigation( void ); //re-links all the NavUp()/NavDown() links for panel items

protected:	

	virtual void PerformLayout();
	virtual void PaintBackground();
	virtual void Paint();
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void ApplySettings(KeyValues *inResourceData);
	virtual void UpdateArrows();
	virtual void UpdatePanels();

	vgui::Panel* GetFirstVisibleItem();
	vgui::Panel* GetLastVisibleItem();
	void ElevatorScroll( bool bScrollUp );

	void SetNavigationChangedCallback( void (*pFunction)( GenericPanelList *, vgui::Panel * ) );

private:
	CUtlVector<vgui::Panel*> m_PanelItems;
	char m_SchemeBgColorName[128];
	vgui::Panel* m_PnlItemRegion;
	vgui::Panel* m_CurrentSelectedItem;
	vgui::ScrollBar* m_ScrVerticalScroll;
	vgui::Label* m_LblDownArrow;
	vgui::Label* m_LblUpArrow;
	vgui::Label* m_LblScrollProgress;
	int m_PanelItemBorder;
	int m_ItemSelectionModeMask;
	unsigned short m_LastItemAdded;
	bool m_bShowScrollProgress;
	bool m_bWrap;
	void (*m_pItemNavigationChangedCallback)( GenericPanelList *pThis, vgui::Panel *pPanel );
};

class IGenericPanelListItem
{
public:
	virtual bool IsLabel() = 0;
};

};

#endif // __VGENERICPANELLIST_H__