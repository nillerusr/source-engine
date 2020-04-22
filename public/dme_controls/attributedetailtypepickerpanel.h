//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef ATTRIBUTEDETAILTYPEPICKERPANEL_H
#define ATTRIBUTEDETAILTYPEPICKERPANEL_H

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
// CAttributeDetailTypePickerPanel
//-----------------------------------------------------------------------------
class CAttributeDetailTypePickerPanel : public CAttributeBasePickerPanel
{
	DECLARE_CLASS_SIMPLE( CAttributeDetailTypePickerPanel, CAttributeBasePickerPanel );

public:
	CAttributeDetailTypePickerPanel( vgui::Panel *parent, const AttributeWidgetInfo_t &info );
	~CAttributeDetailTypePickerPanel();

private:
	// Reads the detail types
	void AddDetailTypesToList( PickerList_t &list );

	MESSAGE_FUNC_PARAMS( OnPicked, "Picked", kv );
	virtual void ShowPickerDialog();
};



#endif // ATTRIBUTEDETAILTYPEPICKERPANEL_H
