//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef ATTRIBUTESURFACEPROPERTYPICKERPANEL_H
#define ATTRIBUTESURFACEPROPERTYPICKERPANEL_H

#ifdef _WIN32
#pragma once
#endif

#include "dme_controls/AttributeBasePickerPanel.h"
#include "matsys_controls/Picker.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CPickerFrame;


//-----------------------------------------------------------------------------
// CAttributeSurfacePropertyPickerPanel
//-----------------------------------------------------------------------------
class CAttributeSurfacePropertyPickerPanel : public CAttributeBasePickerPanel
{
	DECLARE_CLASS_SIMPLE( CAttributeSurfacePropertyPickerPanel, CAttributeBasePickerPanel );

public:
	CAttributeSurfacePropertyPickerPanel( vgui::Panel *parent, const AttributeWidgetInfo_t &info );
	~CAttributeSurfacePropertyPickerPanel();

private:
	// Reads the surface properties
	void AddSurfacePropertiesToList( PickerList_t &list );

	MESSAGE_FUNC_PARAMS( OnPicked, "Picked", kv );
	virtual void ShowPickerDialog();
};



#endif // ATTRIBUTESURFACEPROPERTYPICKERPANEL_H
