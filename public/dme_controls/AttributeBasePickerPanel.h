//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef ATTRIBUTEBASEPICKERPANEL_H
#define ATTRIBUTEBASEPICKERPANEL_H

#ifdef _WIN32
#pragma once
#endif

#include "dme_controls/AttributeTextPanel.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CDmElement;

namespace vgui
{
	class Button;
}


//-----------------------------------------------------------------------------
// CAttributeBasePickerPanel
//-----------------------------------------------------------------------------
class CAttributeBasePickerPanel : public CAttributeTextPanel
{
	DECLARE_CLASS_SIMPLE( CAttributeBasePickerPanel, CAttributeTextPanel );

public:
	CAttributeBasePickerPanel( vgui::Panel *parent, const AttributeWidgetInfo_t &info );

	// Inherited from Panel
	virtual void	OnCommand( const char *cmd );
	virtual void	PerformLayout();

private:
	// Inherited classes must implement this
	virtual void	ShowPickerDialog() = 0;

	vgui::Button	*m_pOpen;
};


#endif // ATTRIBUTEBASEPICKERPANEL_H
