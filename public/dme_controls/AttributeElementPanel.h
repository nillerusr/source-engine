//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef ATTRIBUTEELEMENTPANEL_H
#define ATTRIBUTEELEMENTPANEL_H

#ifdef _WIN32
#pragma once
#endif

#include "dme_controls/BaseAttributePanel.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CDmElement;
class CAttributeTextEntry;

namespace vgui
{
	class Label;
}


//-----------------------------------------------------------------------------
// CAttributeElementPanel
//-----------------------------------------------------------------------------
class CAttributeElementPanel : public CBaseAttributePanel
{
	DECLARE_CLASS_SIMPLE( CAttributeElementPanel, CBaseAttributePanel );

public:
	CAttributeElementPanel( vgui::Panel *parent, const AttributeWidgetInfo_t &info );

	virtual void PostConstructor();
	virtual void Apply();

protected:
	virtual vgui::Panel *GetDataPanel();
	virtual void OnCreateDragData( KeyValues *msg );

	MESSAGE_FUNC(OnTextChanged, "TextChanged")
	{
		SetDirty( true );
	}

private:
	virtual void Refresh();

	CAttributeTextEntry		*m_pData;
	bool					m_bShowMemoryUsage;
};


#endif // ATTRIBUTEELEMENTPANEL_H
