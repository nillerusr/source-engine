//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef ATTRIBUTESOUNDPICKERPANEL_H
#define ATTRIBUTESOUNDPICKERPANEL_H

#ifdef _WIN32
#pragma once
#endif

#include "dme_controls/AttributeBasePickerPanel.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// CAttributeSoundPickerPanel
//-----------------------------------------------------------------------------
class CAttributeSoundPickerPanel : public CAttributeBasePickerPanel
{
	DECLARE_CLASS_SIMPLE( CAttributeSoundPickerPanel, CAttributeBasePickerPanel );

public:
	CAttributeSoundPickerPanel( vgui::Panel *parent, const AttributeWidgetInfo_t &info );
	~CAttributeSoundPickerPanel();

private:
	MESSAGE_FUNC_PARAMS( OnSoundSelected, "SoundSelected", kv );
	virtual void ShowPickerDialog();
};


#endif // ATTRIBUTESOUNDPICKERPANEL_H
