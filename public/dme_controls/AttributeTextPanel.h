//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef ATTRIBUTETEXTPANEL_H
#define ATTRIBUTETEXTPANEL_H

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
// CAttributeTextPanel
//-----------------------------------------------------------------------------
class CAttributeTextPanel : public CBaseAttributePanel
{
	DECLARE_CLASS_SIMPLE( CAttributeTextPanel, CBaseAttributePanel );

public:
	CAttributeTextPanel( vgui::Panel *parent, const AttributeWidgetInfo_t &info );
	virtual void SetFont( HFont font );
	virtual void PostConstructor();
	virtual void Apply();
	virtual void Refresh();

	// Returns the text type
	const char *GetTextType();

protected:
	virtual vgui::Panel *GetDataPanel();

	MESSAGE_FUNC(OnTextChanged, "TextChanged")
	{
		SetDirty( true );
	}

protected:
	CAttributeTextEntry	*m_pData;
	bool m_bShowMemoryUsage;
};


#endif // ATTRIBUTETEXTPANEL_H
