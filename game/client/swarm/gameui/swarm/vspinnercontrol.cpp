//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "VSpinnerControl.h"
#include "basemodui.h"
#include "VFooterPanel.h"

#include "vgui_controls/Label.h"
#include "tier1/KeyValues.h"
#include "vgui_controls/Tooltip.h"
#include "EngineInterface.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define HIGHLIGHT_TIMER 0.25f

using namespace vgui;
using namespace BaseModUI;

//=============================================================================
SpinnerControlItem::SpinnerControlItem(const char* text, const char* tooltip, int userData):
m_Text( 0 ),
m_TooltipText( 0 )
{ 
	SetText(text);
	SetTooltipText( tooltip );
	SetUserData(userData);
}

//=============================================================================
SpinnerControlItem::SpinnerControlItem(const SpinnerControlItem& rhs):
m_Text( 0 ),
m_TooltipText( 0 )
{
	SetText(rhs.GetText());
	SetTooltipText( rhs.GetTooltipText() );
	SetUserData(rhs.GetUserData());
}

//=============================================================================
SpinnerControlItem::~SpinnerControlItem()
{
	if(m_Text)
	{
		delete [] m_Text;
	}

	if(m_TooltipText)
	{
		delete [] m_TooltipText;
	}
}

//=============================================================================
void SpinnerControlItem::SetText(const char* text)
{
	if(m_Text)
	{
		delete [] m_Text;
		m_Text = 0;
	}

	if(text != 0)
	{
		m_Text = new char[Q_strlen(text) + 1];
		Q_strcpy(m_Text, text);
	}
}

//=============================================================================
const char* SpinnerControlItem::GetText() const
{
	return m_Text;
}

//=============================================================================
void SpinnerControlItem::SetTooltipText(const char* tooltipText)
{
	if(m_TooltipText)
	{
		delete [] m_TooltipText;
		m_TooltipText = 0;
	}

	if(tooltipText != 0)
	{
		m_TooltipText = new char[Q_strlen(tooltipText) + 1];
		Q_strcpy(m_TooltipText, tooltipText);
	}
}

//=============================================================================
const char* SpinnerControlItem::GetTooltipText() const
{
	return m_TooltipText;
}

//=============================================================================
void SpinnerControlItem::SetUserData(int userData)
{
	m_UserData = userData;
}

//=============================================================================
int SpinnerControlItem::GetUserData() const
{
	return m_UserData;
}

//=============================================================================
SpinnerControl::SpinnerControl(vgui::Panel *parent, const char *panelName ):
BaseClass(parent, panelName)
{
	m_LblLeftArrow = new Label(this, "LblLeftArrow", "#GameUI_Icons_LEFT_ARROW");
	m_LblCurrentText = new Label( this, "LblCurrentText", "" );
	m_LblRightArrow = new Label(this, "LblRightArrow", "#GameUI_Icons_RIGHT_ARROW");

	m_LabelBorder = 2;
	m_CurrentItem = 0;
	m_BackgroundX = 0;
	m_BackgroundWide = 0;
	m_LastSpinDirection = LSD_NONE;
	m_HighlightTimer = 0.0f;

	CBaseModFrame::AddFrameListener(this);
}

//=============================================================================
SpinnerControl::~SpinnerControl()
{
	CBaseModFrame::RemoveFrameListener(this);

	delete m_LblLeftArrow;
	delete m_LblCurrentText;
	delete m_LblRightArrow;

}

//=============================================================================
void SpinnerControl::AddItem( const char* newItem, const char* newTooltip, int userData )
{
	m_Items.AddToTail( SpinnerControlItem( newItem, newTooltip, userData ) );
}

void SpinnerControl::AddItem( const GenericSpinnerItem &item )
{
	AddItem( item.LocalizedTextKey, item.LocalizedToolTipKey, item.UIGameDataValue );
}

//=============================================================================
int SpinnerControl::GetItemCount()
{
	return m_Items.Count();
}

//=============================================================================
const char* SpinnerControl::GetItem(int itemIndex)
{
	if(itemIndex < GetItemCount())
	{
		return m_Items[itemIndex].GetText();
	}

	return 0;
}

//=============================================================================
 const char* SpinnerControl::GetActiveItem()
 {
	if(m_CurrentItem < GetItemCount())
	{
		return m_Items[m_CurrentItem].GetText();
	}

	return 0;
 }

 //=============================================================================
int SpinnerControl::GetItemUserData(int itemIndex)
{
	if(itemIndex < GetItemCount())
	{
		return m_Items[itemIndex].GetUserData();
	}

	return 0;
}

//=============================================================================
int SpinnerControl::GetActiveItemUserData()
{
	if(m_CurrentItem < GetItemCount())
	{
		return m_Items[m_CurrentItem].GetUserData();
	}

	return 0;
}

//=============================================================================
bool SpinnerControl::RemoveItem(const char* oldItem)
{
	for(int i = 0; i < GetItemCount(); ++i)
	{
		if(!Q_strcmp(oldItem, m_Items[i].GetText()))
		{
			m_Items.Remove(i);

			if(m_CurrentItem == i)
			{
				m_CurrentItem = 0;
				ApplyCurrentItem();
			}

			return true;
		}
	}

	return false;
}

//=============================================================================
bool SpinnerControl::RemoveItem(int itemIndex)
{
	if(itemIndex < GetItemCount())
	{
		m_Items.Remove(itemIndex);

		if(m_CurrentItem == itemIndex)
		{
			m_CurrentItem = 0;
			ApplyCurrentItem();
		}

		return true;
	}

	return false;
}

//=============================================================================
bool SpinnerControl::SetCurrentItem(const char* item)
{
	for(int i = 0; i < GetItemCount(); ++i)
	{
		if(!Q_strcmp(item, m_Items[i].GetText()))
		{
			m_CurrentItem = i;
			ApplyCurrentItem();
			return true;
		}
	}

	return false;
}

//=============================================================================
bool SpinnerControl::SetCurrentItem(int itemIndex)
{
	if(itemIndex < GetItemCount())
	{
		m_CurrentItem = itemIndex;
		ApplyCurrentItem();
		return true;
	}

	return false;
}

//=============================================================================
bool SpinnerControl::SetCurrentItemByUserData(int userData)
{
	for(int i = 0; i < GetItemCount(); ++i)
	{
		if(userData == m_Items[i].GetUserData())
		{
			m_CurrentItem = i;
			ApplyCurrentItem();
			return true;
		}
	}

	return false;
}

//=============================================================================
void SpinnerControl::OnKeyCodePressed(KeyCode code)
{
	int previousItem = m_CurrentItem;

	switch ( GetBaseButtonCode( code ) )
	{
	case KEY_XSTICK1_LEFT:
	case KEY_XSTICK2_LEFT:
	case KEY_XBUTTON_LEFT:
		m_CurrentItem--;
		m_LastSpinDirection = LSD_LEFT;
		m_HighlightTimer = Plat_FloatTime();
		break;

	case KEY_XSTICK1_RIGHT:
	case KEY_XSTICK2_RIGHT:
	case KEY_XBUTTON_RIGHT:
		m_CurrentItem++;
		m_LastSpinDirection = LSD_RIGHT;
		m_HighlightTimer = Plat_FloatTime();
		break;

	default:
		BaseClass::OnKeyCodePressed(code);
		break;
	}

	if((m_CurrentItem < 0) && (m_Items.Count() > 0))
	{
		m_CurrentItem = m_Items.Count() - 1;
	}
	else if((m_CurrentItem >= m_Items.Count()) || (m_Items.Count() == 0))
	{
		m_CurrentItem = 0;
	}

	if(previousItem != m_CurrentItem)
	{
		ApplyCurrentItem();
	}
}

#ifdef _X360
//=============================================================================
void SpinnerControl::NavigateTo()
{
	BaseClass::NavigateTo();

	m_LblCurrentText->SetBgColor(m_ActiveColor);
}

//=============================================================================
void SpinnerControl::NavigateFrom()
{
	BaseClass::NavigateFrom();

	m_LblCurrentText->SetBgColor(m_InactiveColor);
}
#endif // _X360

//=============================================================================
void SpinnerControl::PaintBackground()
{

}

//=============================================================================
void SpinnerControl::RunFrame()
{
	if(Plat_FloatTime() < (m_HighlightTimer + HIGHLIGHT_TIMER))
	{
		switch(m_LastSpinDirection)
		{
		case LSD_LEFT:
			m_LblLeftArrow->SetAlpha(255);
			break;

		case LSD_RIGHT:
			m_LblRightArrow->SetAlpha(255);
			break;
		}
	}
	else
	{
		m_LblLeftArrow->SetAlpha(64);
		m_LblRightArrow->SetAlpha(64);
	}
}

//=============================================================================
void SpinnerControl::OnSetCurrentItem(const char* panelName)
{
}

//=============================================================================
void SpinnerControl::PerformLayout()
{
	BaseClass::PerformLayout();

	int x, y, wide, tall;
	GetBounds(x, y, wide, tall);

	int leftArrowWide, leftArrowTall;
	m_LblLeftArrow->GetContentSize(leftArrowWide, leftArrowTall);
	m_LblLeftArrow->SetPos(0, 0);
	m_LblLeftArrow->SetSize(leftArrowWide, leftArrowTall);
	m_LblLeftArrow->SetZPos( 2 );

	int rightArrowWide, rightArrowTall;
	m_LblRightArrow->GetContentSize(rightArrowWide, rightArrowTall);
	m_LblRightArrow->SetPos(wide - rightArrowWide, 0);
	m_LblRightArrow->SetSize(rightArrowWide, rightArrowTall);
	m_LblRightArrow->SetZPos( 2 );

	int textWide = wide - ( m_LabelBorder * 4 );
	m_LblCurrentText->SetPos( ( m_LabelBorder * 2 ), 0 );
	m_LblCurrentText->SetSize( textWide < 0 ? 0 : textWide, tall );
	m_LblCurrentText->SetPaintBorderEnabled( false );
	m_LblCurrentText->SetZPos( 1 );

	int backgroundWide = wide - (m_LabelBorder * 2);
	m_BackgroundX = m_LabelBorder;
	m_BackgroundWide = backgroundWide < 0 ? 0 : backgroundWide;

	ApplyCurrentItem();
}

//=============================================================================
void SpinnerControl::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	m_LblCurrentText->SetBorder(pScheme->GetBorder("ButtonBorder"));

	m_LblLeftArrow->SetFont(pScheme->GetFont("GameUIButtons"));
	m_LblRightArrow->SetFont(pScheme->GetFont("GameUIButtons"));

	m_LblCurrentText->SetPaintBackgroundEnabled(true);
}

//=============================================================================
void SpinnerControl::ApplySettings(KeyValues *inResourceData)
{
	BaseClass::ApplySettings(inResourceData);

	m_InactiveColor = inResourceData->GetColor( "inactiveColor", Color( 64, 64, 64, 255 ) );
	m_ActiveColor = inResourceData->GetColor( "activeColor", Color( 128, 0, 0, 255 ) );
	m_LblCurrentText->SetBgColor(m_InactiveColor);
	
	m_LblCurrentText->SetPaintBorderEnabled( inResourceData->GetInt( "paintTextBorder", 0 ) == 1 );

	const char *alignmentString = inResourceData->GetString("textAlignment", "");
	if(alignmentString)
	{
		int align = -1;

		if(!stricmp(alignmentString, "north-west"))
		{
			align = Label::a_northwest;
		}
		else if(!stricmp(alignmentString, "north"))
		{
			align = Label::a_north;
		}
		else if(!stricmp(alignmentString, "north-east"))
		{
			align = Label::a_northeast;
		}
		else if(!stricmp(alignmentString, "west"))
		{
			align = Label::a_west;
		}
		else if(!stricmp(alignmentString, "center"))
		{
			align = Label::a_center;
		}
		else if(!stricmp(alignmentString, "east"))
		{
			align = Label::a_east;
		}
		else if(!stricmp(alignmentString, "south-west"))
		{
			align = Label::a_southwest;
		}
		else if(!stricmp(alignmentString, "south"))
		{
			align = Label::a_south;
		}
		else if(!stricmp(alignmentString, "south-east"))
		{
			align = Label::a_southeast;
		}

		if(align != -1)
		{
			m_LblCurrentText->SetContentAlignment((Label::Alignment)align);
		}
	}
}

//=============================================================================
void SpinnerControl::ApplyCurrentItem()
{
	if((m_CurrentItem < m_Items.Count()))
	{
		SpinnerControlItem &item = m_Items[ m_CurrentItem ];
		m_LblCurrentText->SetText( item.GetText() );
		if( item.GetTooltipText() != 0 )
		{
			GetTooltip()->SetText( item.GetTooltipText() );
		}
	}
	else
	{
		m_LblCurrentText->SetText("");
	}

	if( HasFocus() )
	{
		CallParentFunction(new KeyValues("OnNavigateTo", "panelName", GetName()));
	}

	CallParentFunction(new KeyValues("OnSetCurrentItem", "panelName", GetName()));
}
