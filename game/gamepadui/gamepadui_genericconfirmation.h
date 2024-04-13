#ifndef GAMEPADUI_GENERICCONFIRMATION_H
#define GAMEPADUI_GENERICCONFIRMATION_H
#ifdef _WIN32
#pragma once
#endif

#include "gamepadui_frame.h"
#include "tier0/valve_minmax_off.h"
#include <functional>
#include "tier0/valve_minmax_on.h"

class GamepadUIGenericConfirmationPanel : public GamepadUIFrame
{
    DECLARE_CLASS_SIMPLE( GamepadUIGenericConfirmationPanel, GamepadUIFrame );

public:
    GamepadUIGenericConfirmationPanel( vgui::Panel *pParent, const char* pPanelName, const char *pTitle, const char *pText, std::function<void()> pCommand, bool bSmallFont = false, bool bShowCancel = true );
    GamepadUIGenericConfirmationPanel( vgui::Panel *pParent, const char* pPanelName, const wchar_t *pTitle, const wchar_t *pText, std::function<void()> pCommand, bool bSmallFont = false, bool bShowCancel = true );

    void Paint() OVERRIDE;
    void OnCommand( const char *pCommand ) OVERRIDE;
    void ApplySchemeSettings( vgui::IScheme* pScheme ) OVERRIDE;
    void UpdateGradients() OVERRIDE;


private:
    void PaintText();

    GamepadUIString m_strText;

    std::function<void()> m_pCommand;

    GAMEPADUI_PANEL_PROPERTY( Color, m_colGenericConfirmationColor, "GenericConfirmation", "255 255 255 255", SchemeValueTypes::Color );

    GAMEPADUI_PANEL_PROPERTY( float, m_flGenericConfirmationOffsetX, "GenericConfirmation.OffsetX", "0", SchemeValueTypes::ProportionalFloat );
    GAMEPADUI_PANEL_PROPERTY( float, m_flGenericConfirmationOffsetY, "GenericConfirmation.OffsetY", "0", SchemeValueTypes::ProportionalFloat );

    vgui::HFont m_hGenericConfirmationFont;
    const char *m_pszGenericConfirmationFontName;
};

#endif
