#ifndef GAMEPADUI_SCROLLBAR_H
#define GAMEPADUI_SCROLLBAR_H
#ifdef _WIN32
#pragma once
#endif

#include "gamepadui_button.h"
#include "gamepadui_scroll.h"

class GamepadUIScrollBar : public GamepadUIButton
{
    DECLARE_CLASS_SIMPLE( GamepadUIScrollBar, GamepadUIButton );
public:
    GamepadUIScrollBar( vgui::Panel *pParent, vgui::Panel *pActionSignalTarget, const char *pSchemeFile, GamepadUIScrollState *pScrollState, bool bHorizontal )
        : BaseClass( pParent, pActionSignalTarget, pSchemeFile, "", "", "" )
    {
        m_pScrollState = pScrollState;
        m_bHorizontal = bHorizontal;

        SetVisible( false );
    }

    void ApplySchemeSettings( vgui::IScheme *pScheme ) OVERRIDE;
    void OnThink() OVERRIDE;

    void InitScrollBar( GamepadUIScrollState *pScrollState, int nX, int nY );
    void UpdateScrollBounds( float flMin, float flMax, float flRegionSize, float flScrollSize );

    void OnMousePressed( vgui::MouseCode code ) OVERRIDE;
    void OnMouseReleased( vgui::MouseCode code ) OVERRIDE;

    void OnKeyCodePressed( vgui::KeyCode code ) OVERRIDE;
    void OnKeyCodeReleased( vgui::KeyCode code ) OVERRIDE;

private:
    int m_nStartX = 0;
    int m_nStartY = 0;

    float m_flScrollSize = 0.0f;
    float m_flRegionSize = 0.0f;
    float m_flMin = 0;
    float m_flMax = 0;
    GamepadUIScrollState *m_pScrollState = NULL;

    int m_nMouseOffset = -1;
    int m_flKeyDir = 0;

    bool m_bHorizontal = false;

    GAMEPADUI_PANEL_PROPERTY( float, m_flScrollSpeed, "ScrollBar.Speed", "64", SchemeValueTypes::ProportionalFloat );
};

#endif // GAMEPADUI_SCROLLBAR_H
