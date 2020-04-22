//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Control for displaying a Steam Controller hint icon
//
// $NoKeywords: $
//=============================================================================//

#ifndef SC_HINTICON_H
#define SC_HINTICON_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/IScheme.h>
#include <vgui/KeyCode.h>
#include <KeyValues.h>
#include <vgui/IVGui.h>
#include <vgui_controls/Label.h>

class CSCHintIcon : public vgui::Label
{
public:
	DECLARE_CLASS_SIMPLE( CSCHintIcon, vgui::Label );

	CSCHintIcon( vgui::Panel *parent, const char *panelName );

	virtual void ApplySettings( KeyValues *inResourceData );
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );

	bool IsActionMapped() const { return m_bIsActionMapped; }

private:
	bool m_bIsActionMapped;
	static const int nMaxActionNameLength = 63;
	char m_szActionName[nMaxActionNameLength+1];
	ControllerActionSetHandle_t m_actionSetHandle;
};

#endif  // SC_HINTICON_H