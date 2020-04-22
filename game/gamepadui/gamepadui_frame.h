#ifndef GAMEPADUI_FRAME_H
#define GAMEPADUI_FRAME_H
#ifdef _WIN32
#pragma once
#endif
#include "gamepadui_panel.h"
#include "gamepadui_button.h"
#include "gamepadui_string.h"

#include "vgui_controls/Frame.h"

class GamepadUIFrame : public vgui::Frame, public SchemeValueMap
{
    DECLARE_CLASS_SIMPLE( GamepadUIFrame, vgui::Frame );
public:
    GamepadUIFrame( vgui::Panel *pParent, const char *pszPanelName, bool bShowTaskbarIcon = true, bool bPopup = true );
    void ApplySchemeSettings( vgui::IScheme* pScheme ) OVERRIDE;
    void OnThink() OVERRIDE;
    void Paint() OVERRIDE;
    void OnKeyCodePressed( vgui::KeyCode code ) OVERRIDE;
    void OnKeyCodeReleased( vgui::KeyCode code ) OVERRIDE;

          GamepadUIString& GetFrameTitle()              { return m_strFrameTitle; }
    const GamepadUIString& GetFrameTitle()        const { return m_strFrameTitle; }

    void PaintTitle();
    void SetFooterButtons( FooterButtonMask mask, FooterButtonMask controllerOnlyMask = 0 );
    void OnClose();

    virtual void UpdateGradients();
protected:
    void LayoutFooterButtons();
    void PaintBackgroundGradients();

    GamepadUIString m_strFrameTitle;
    GamepadUIButton *m_pFooterButtons[ FooterButtons::MaxFooterButtons ];
    FooterButtonMask m_ControllerOnlyFooterMask = 0;

    GAMEPADUI_PANEL_PROPERTY( Color, m_colTitleColor, "Title", "255 255 255 255", SchemeValueTypes::Color );

    GAMEPADUI_PANEL_PROPERTY( float, m_flTitleOffsetX, "Title.OffsetX", "0", SchemeValueTypes::ProportionalFloat );
    GAMEPADUI_PANEL_PROPERTY( float, m_flTitleOffsetY, "Title.OffsetY", "0", SchemeValueTypes::ProportionalFloat );

    GAMEPADUI_PANEL_PROPERTY( float, m_flFooterButtonsOffsetX, "FooterButtons.OffsetX", "0", SchemeValueTypes::ProportionalFloat );
    GAMEPADUI_PANEL_PROPERTY( float, m_flFooterButtonsOffsetY, "FooterButtons.OffsetY", "0", SchemeValueTypes::ProportionalFloat );
    GAMEPADUI_PANEL_PROPERTY( float, m_flFooterButtonsSpacing, "FooterButtons.Spacing", "0", SchemeValueTypes::ProportionalFloat );

    vgui::HFont m_hTitleFont = vgui::INVALID_FONT;
    vgui::HFont m_hGenericFont = vgui::INVALID_FONT;
    int m_nFooterButtonWidth = 0;
    int m_nFooterButtonHeight = 0;
};

#endif // GAMEPADUI_FRAME_H
