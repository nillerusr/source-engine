#ifndef GAMEPADUI_BASEPANEL_H
#define GAMEPADUI_BASEPANEL_H
#ifdef _WIN32
#pragma once
#endif

#include "gamepadui_interface.h"

class GamepadUIMainMenu;
class GamepadUIFrame;

class GamepadUIBasePanel : public vgui::Panel
{
    DECLARE_CLASS_SIMPLE( GamepadUIBasePanel, vgui::Panel );
public:
    GamepadUIBasePanel( vgui::VPANEL parent );

    void ApplySchemeSettings( vgui::IScheme* pScheme ) OVERRIDE;

    GamepadUISizingPanel *GetSizingPanel() const;
	GamepadUIMainMenu *GetMainMenuPanel() const;

    GamepadUIFrame *GetCurrentFrame() const;
    void SetCurrentFrame( GamepadUIFrame *pFrame );

    void OnMenuStateChanged();

    void ActivateBackgroundEffects();
    bool IsBackgroundMusicPlaying();
    bool StartBackgroundMusic( float flVolume );
    void ReleaseBackgroundMusic();

private:
    GamepadUISizingPanel *m_pSizingPanel = NULL;
    GamepadUIMainMenu *m_pMainMenu = NULL;

    GamepadUIFrame *m_pCurrentFrame = NULL;

    int m_nBackgroundMusicGUID;
    bool m_bBackgroundMusicEnabled;

};

class GamepadUISizingPanel : public vgui::Panel
{
    DECLARE_CLASS_SIMPLE( GamepadUISizingPanel, vgui::Panel );
public:
    GamepadUISizingPanel( vgui::Panel *pParent );

    void ApplySchemeSettings( vgui::IScheme* pScheme ) OVERRIDE;

    void GetScale( float &flX, float &flY ) const { flX = m_flScaleX; flY = m_flScaleY; }

private:

    float m_flScaleX;
    float m_flScaleY;
};

#endif // GAMEPADUI_BASEPANEL_H
