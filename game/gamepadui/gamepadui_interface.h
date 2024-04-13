#ifndef GAMEPADUI_INTERFACE_H
#define GAMEPADUI_INTERFACE_H
#ifdef _WIN32
#pragma once
#endif

#include "igamepadui.h"

#include "cdll_int.h"
#include "engine/IEngineSound.h"
#include "gamepadui_gradient_helper.h"
#include "GameUI/IGameUI.h"
#include "iachievementmgr.h"
#include "ienginevgui.h"
#include "ivrenderview.h"
#include "materialsystem/imaterialsystem.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "tier1/interface.h"
#include "tier1/strtools.h"
#include "vgui_controls/AnimationController.h"
#include "vgui_controls/Panel.h"
#include "vgui/VGUI.h"
#include "VGuiMatSurface/IMatSystemSurface.h"
#include "view_shared.h"
#include "IGameUIFuncs.h"
#include "steam/steam_api.h"
#ifdef STEAM_INPUT
#include "expanded_steam/isteaminput.h"
#endif

class GamepadUIBasePanel;
class GamepadUIMainMenu;

#define GAMEPADUI_RESOURCE_FOLDER "gamepadui" CORRECT_PATH_SEPARATOR_S

class GamepadUIBasePanel;
class GamepadUISizingPanel;
class GamepadUIFrame;

class GamepadUI : public IGamepadUI
{
public:
    static GamepadUI& GetInstance();

    void Initialize( CreateInterfaceFn factory ) OVERRIDE;
    void Shutdown() OVERRIDE;

    void OnUpdate( float flFrametime ) OVERRIDE;
    void OnLevelInitializePreEntity() OVERRIDE;
    void OnLevelInitializePostEntity() OVERRIDE;
    void OnLevelShutdown() OVERRIDE;
	
    void VidInit() OVERRIDE;

    bool IsInLevel() const;
    bool IsInBackgroundLevel() const;
    bool IsInMultiplayer() const;

    bool IsGamepadUIVisible() const;

    vgui::VPANEL GetRootVPanel() const;
    vgui::Panel *GetBasePanel() const;
    vgui::VPANEL GetBaseVPanel() const;
    vgui::Panel *GetSizingPanel() const;
    vgui::VPANEL GetSizingVPanel() const;
    vgui::Panel *GetMainMenuPanel() const;
    vgui::VPANEL GetMainMenuVPanel() const;

    IAchievementMgr         *GetAchievementMgr()         const { return m_pAchievementMgr; }
    IEngineSound            *GetEngineSound()            const { return m_pEngineSound; }
    IEngineVGui             *GetEngineVGui()             const { return m_pEngineVGui; }
    IGameUI                 *GetGameUI()                 const { return m_pGameUI; }
    IGameUIFuncs            *GetGameUIFuncs()            const { return m_pGameUIFuncs; }
    IMaterialSystem         *GetMaterialSystem()         const { return m_pMaterialSystem; }
    IMatSystemSurface       *GetMaterialSystemSurface()  const { return m_pMaterialSystemSurface; }
    ISoundEmitterSystemBase *GetSoundEmitterSystemBase() const { return m_pSoundEmitterSystemBase; }
    IVEngineClient          *GetEngineClient()           const { return m_pEngineClient; }
    IVRenderView            *GetRenderView()             const { return m_pRenderView; }
#ifdef STEAM_INPUT
    ISource2013SteamInput   *GetSteamInput()             const { return m_pSteamInput; }
#endif

    vgui::AnimationController *GetAnimationController() const { return m_pAnimationController; }
    float GetTime() const { return Plat_FloatTime(); }
    GradientHelper *GetGradientHelper() { return &m_GradientHelper; }

    void ResetToMainMenuGradients();

    CSteamAPIContext* GetSteamAPIContext() { return &m_SteamAPIContext; }

    bool GetScreenRatio( float &flX, float &flY ) const { flX = m_flScreenXRatio; flY = m_flScreenYRatio; return (flX != 1.0f || flY != 1.0f); }

    void GetSizingPanelScale( float &flX, float &flY ) const;
    void GetSizingPanelOffset( int &nX, int &nY ) const;

    GamepadUIFrame *GetCurrentFrame() const;
    vgui::VPANEL GetCurrentFrameVPanel() const;

#ifdef MAPBASE
	void BonusMapChallengeNames( char *pchFileName, char *pchMapName, char *pchChallengeName ) OVERRIDE;
	void BonusMapChallengeObjectives( int &iBronze, int &iSilver, int &iGold ) OVERRIDE;

    void SetCurrentChallengeObjectives( int iBronze, int iSilver, int iGold );
    void SetCurrentChallengeNames( const char *pszFileName, const char *pszMapName, const char *pszChallengeName );
#endif

#ifdef STEAM_INPUT
    // TODO: Replace with proper singleton interface in the future
    void SetSteamInput( ISource2013SteamInput *pSteamInput ) override { m_pSteamInput = pSteamInput; }
#endif

private:

    IEngineSound            *m_pEngineSound            = NULL;
    IEngineVGui             *m_pEngineVGui             = NULL;
    IGameUIFuncs            *m_pGameUIFuncs            = NULL;
    IMaterialSystem         *m_pMaterialSystem         = NULL;
    IMatSystemSurface       *m_pMaterialSystemSurface  = NULL;
    ISoundEmitterSystemBase *m_pSoundEmitterSystemBase = NULL;
    IVEngineClient          *m_pEngineClient           = NULL;
    IVRenderView            *m_pRenderView             = NULL;

    IGameUI                 *m_pGameUI                 = NULL;
    IAchievementMgr         *m_pAchievementMgr         = NULL;

#ifdef STEAM_INPUT
    ISource2013SteamInput   *m_pSteamInput             = NULL;
#endif

    vgui::AnimationController *m_pAnimationController = NULL;
    GamepadUIBasePanel *m_pBasePanel = NULL;

    GradientHelper m_GradientHelper;
    CSteamAPIContext m_SteamAPIContext;

    GamepadUIMainMenu* GetMainMenu() const;

    float   m_flScreenXRatio = 1.0f;
    float   m_flScreenYRatio = 1.0f;

#ifdef MAPBASE
    char	m_szChallengeFileName[MAX_PATH];
    char	m_szChallengeMapName[48];
    char	m_szChallengeName[48];

    int		m_iBronze, m_iSilver, m_iGold;
#endif

    static GamepadUI *s_pGamepadUI;
};

#endif // GAMEPADUI_INTERFACE_H
