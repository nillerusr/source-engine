//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef FILTERCOMBOBOX_H
#define FILTERCOMBOBOX_H

#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/combobox.h"


//-----------------------------------------------------------------------------
// Combo box that adds entry to its history when focus is lost
//-----------------------------------------------------------------------------
class CFilterComboBox : public vgui::ComboBox
{
	DECLARE_CLASS_SIMPLE( CFilterComboBox, vgui::ComboBox );

public:
	CFilterComboBox( Panel *parent, const char *panelName, int numLines, bool allowEdit );
	virtual void OnKillFocus();
};


#endif // FILTERCOMBOBOX_H

	
