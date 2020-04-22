//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef OPTIONS_SUB_DIFFICULTY_H
#define OPTIONS_SUB_DIFFICULTY_H
#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/PropertyPage.h"

//-----------------------------------------------------------------------------
// Purpose: Difficulty selection options
//-----------------------------------------------------------------------------
class COptionsSubDifficulty : public vgui::PropertyPage
{
	DECLARE_CLASS_SIMPLE( COptionsSubDifficulty, vgui::PropertyPage );

public:
	COptionsSubDifficulty(vgui::Panel *parent);

	virtual void OnResetData();
	virtual void OnApplyChanges();

	MESSAGE_FUNC( OnRadioButtonChecked, "RadioButtonChecked" );

private:
	vgui::RadioButton *m_pEasyRadio;
	vgui::RadioButton *m_pNormalRadio;
	vgui::RadioButton *m_pHardRadio;
};


#endif // OPTIONS_SUB_DIFFICULTY_H