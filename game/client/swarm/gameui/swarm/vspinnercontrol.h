//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __VGENERICPANELLIST_H__
#define __VGENERICPANELLIST_H__

#include "basemodui.h"

namespace BaseModUI {

struct GenericSpinnerItem
{
	const char* LocalizedTextKey;
	const char* LocalizedToolTipKey;
	int UIGameDataValue;
};

class SpinnerControlItem
{
public:
	SpinnerControlItem(const char* text = 0, const char* tooltip = 0, int userData = 0);
	SpinnerControlItem(const SpinnerControlItem& rhs);
	~SpinnerControlItem();

	void SetText(const char* text);
	const char* GetText() const;

	void SetTooltipText(const char* tooltipText);
	const char* GetTooltipText() const;

	void SetUserData(int userData);
	int GetUserData() const;

private:
	int m_UserData;
	char* m_Text;
	char* m_TooltipText;
};

class SpinnerControl : public vgui::Panel, public IBaseModFrameListener
{
	DECLARE_CLASS_SIMPLE( SpinnerControl, vgui::Panel );

public:
	SpinnerControl(vgui::Panel *parent, const char *panelName );
	virtual ~SpinnerControl();

	virtual void AddItem(const char* newItem, const char* newTooltip = NULL, int userData = 0);
	virtual void AddItem( const GenericSpinnerItem &item );

	virtual int GetItemCount();
	virtual const char* GetItem(int itemIndex);
	virtual const char* GetActiveItem();
	virtual int GetItemUserData(int itemIndex);
	virtual int GetActiveItemUserData();
	virtual bool RemoveItem(const char* oldItem);
	virtual bool RemoveItem(int itemIndex);
	virtual bool SetCurrentItem(const char* item);
	virtual bool SetCurrentItem(int itemIndex);
	virtual bool SetCurrentItemByUserData(int userData);

	virtual void OnKeyCodePressed(vgui::KeyCode code);

#ifdef _X360
	virtual void NavigateTo();
	virtual void NavigateFrom();
#endif // _X360

	virtual void PaintBackground();

	virtual void RunFrame();

	MESSAGE_FUNC_CHARPTR( OnSetCurrentItem, "OnSetCurrentItem", panelName );

protected:
	virtual void PerformLayout();
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void ApplySettings(KeyValues *inResourceData);
	virtual void ApplyCurrentItem();

private:
	vgui::Label* m_LblLeftArrow;
	vgui::Label* m_LblCurrentText;
	vgui::Label* m_LblRightArrow;

	int m_LabelBorder;

	Color m_InactiveColor;
	Color m_ActiveColor;

	int m_CurrentItem;
	int m_BackgroundX;
	int m_BackgroundWide;

	CUtlVector<SpinnerControlItem> m_Items;

	float m_HighlightTimer;

	enum LAST_SPIN_DIRECTION { LSD_NONE, LSD_LEFT, LSD_RIGHT } m_LastSpinDirection;
};

}

#endif // __VGENERICPANELLIST_H__
