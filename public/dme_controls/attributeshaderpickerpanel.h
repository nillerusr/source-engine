//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef ATTRIBUTESHADERPICKERPANEL_H
#define ATTRIBUTESHADERPICKERPANEL_H

#ifdef _WIN32
#pragma once
#endif

#include "dme_controls/AttributeBasePickerPanel.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CPickerFrame;


//-----------------------------------------------------------------------------
// CAttributeShaderPickerPanel
//-----------------------------------------------------------------------------
class CAttributeShaderPickerPanel : public CAttributeBasePickerPanel
{
	DECLARE_CLASS_SIMPLE( CAttributeShaderPickerPanel, CAttributeBasePickerPanel );

public:
	CAttributeShaderPickerPanel( vgui::Panel *parent, const AttributeWidgetInfo_t &info );
	~CAttributeShaderPickerPanel();

private:
	MESSAGE_FUNC_PARAMS( OnPicked, "Picked", kv );
	virtual void ShowPickerDialog();
};



#endif // ATTRIBUTESHADERPICKERPANEL_H
