//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef ATTRIBUTESEQUENCEPICKERPANEL_H
#define ATTRIBUTESEQUENCEPICKERPANEL_H

#ifdef _WIN32
#pragma once
#endif

#include "dme_controls/AttributeBasePickerPanel.h"
#include "vgui_controls/PHandle.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CMDLPickerFrame;
class CSequencePickerFrame;


//-----------------------------------------------------------------------------
// CAttributeSequencePickerPanel
//-----------------------------------------------------------------------------
class CAttributeSequencePickerPanel : public CAttributeBasePickerPanel
{
	DECLARE_CLASS_SIMPLE( CAttributeSequencePickerPanel, CAttributeBasePickerPanel );

public:
	CAttributeSequencePickerPanel( vgui::Panel *parent, const AttributeWidgetInfo_t &info );
	~CAttributeSequencePickerPanel();

private:
	MESSAGE_FUNC_PARAMS( OnMDLSelected, "AssetSelected", kv );
	MESSAGE_FUNC_PARAMS( OnSequenceSelected, "SequenceSelected", kv );
	virtual void ShowPickerDialog();
	void ShowSequencePickerDialog( const char *pMDLName );
};


#endif // ATTRIBUTESEQUENCEPICKERPANEL_H
