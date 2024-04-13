#ifndef GAMEPADUI_GENERICFRAME_H
#define GAMEPADUI_GENERICFRAME_H
#ifdef _WIN32
#pragma once
#endif

#include "gamepadui_frame.h"
#include "gamepadui_image.h"
#include "tier0/valve_minmax_off.h"
#include <functional>
#include "tier0/valve_minmax_on.h"

class GamepadUIGenericBasePanel : public GamepadUIFrame
{
    DECLARE_CLASS_SIMPLE( GamepadUIGenericBasePanel, GamepadUIFrame );

public:
    GamepadUIGenericBasePanel( vgui::Panel *pParent, const char *pPanelName, const char *pTitle, std::function<void()> pCommand, bool bShowCancel = true );
    GamepadUIGenericBasePanel( vgui::Panel *pParent, const char *pPanelName, const wchar_t *pTitle, std::function<void()> pCommand, bool bShowCancel = true );

    void Paint() OVERRIDE;
    void OnCommand( const char *pCommand ) OVERRIDE;
    void ApplySchemeSettings( vgui::IScheme* pScheme ) OVERRIDE;
    void UpdateGradients() OVERRIDE;

    virtual void PaintContent() {}

    virtual void ContentSize( int &nWide, int &nTall ) {}
    virtual bool CenterPanel() { return false; }

protected:

    GAMEPADUI_PANEL_PROPERTY( Color, m_colContentColor, "Content", "255 255 255 255", SchemeValueTypes::Color );

    GAMEPADUI_PANEL_PROPERTY( float, m_flContentOffsetX, "Content.OffsetX", "64", SchemeValueTypes::ProportionalFloat );
    GAMEPADUI_PANEL_PROPERTY( float, m_flContentOffsetY, "Content.OffsetY", "110", SchemeValueTypes::ProportionalFloat );

private:
    std::function<void()> m_pCommand;
};

class GamepadUIGenericImagePanel : public GamepadUIGenericBasePanel
{
    DECLARE_CLASS_SIMPLE( GamepadUIGenericImagePanel, GamepadUIGenericBasePanel );

public:
    GamepadUIGenericImagePanel( vgui::Panel *pParent, const char* pPanelName, const char *pTitle, const char *pImage, float flImageWide, float flImageTall, std::function<void()> pCommand, bool bShowCancel = false );
    GamepadUIGenericImagePanel( vgui::Panel *pParent, const char* pPanelName, const wchar_t *pTitle, const char *pImage, float flImageWide, float flImageTall, std::function<void()> pCommand, bool bShowCancel = false );

    void ApplySchemeSettings( vgui::IScheme *pScheme ) OVERRIDE;
    void PaintContent() OVERRIDE;

private:
    GamepadUIImage m_Image;

    float m_flBaseImageWide;
    float m_flBaseImageTall;
    float m_flImageWide; // Proportionally scaled
    float m_flImageTall; // Proportionally scaled
};

#endif
