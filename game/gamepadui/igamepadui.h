#ifndef IGAMEPADUI_H
#define IGAMEPADUI_H
#ifdef _WIN32
#pragma once
#endif

#include "tier1/interface.h"
#include "vgui/VGUI.h"
#include "mathlib/vector.h"
#include "ivrenderview.h"

class ISource2013SteamInput;

abstract_class IGamepadUI : public IBaseInterface
{
public:
    virtual void Initialize( CreateInterfaceFn factory ) = 0;
    virtual void Shutdown() = 0;

    virtual void OnUpdate( float flFrametime ) = 0;
    virtual void OnLevelInitializePreEntity() = 0;
    virtual void OnLevelInitializePostEntity() = 0;
    virtual void OnLevelShutdown() = 0;
	
    virtual void VidInit() = 0;
#ifdef STEAM_INPUT
    //// TODO: Replace with proper singleton interface in the future
    virtual void SetSteamInput( ISource2013SteamInput *pSteamInput ) = 0;
#endif

#ifdef MAPBASE
	virtual void BonusMapChallengeNames( char *pchFileName, char *pchMapName, char *pchChallengeName ) = 0;
	virtual void BonusMapChallengeObjectives( int &iBronze, int &iSilver, int &iGold ) = 0;
#endif
};

#define GAMEPADUI_INTERFACE_VERSION "GamepadUI001"

// Lil easter egg :-)
#ifdef GAMEPADUI_GAME_PORTAL
#define GamepadUI_Log(...) ConColorMsg( Color( 61, 189, 237, 255 ), "[GamepadUI] " __VA_ARGS__ )
#else
#define GamepadUI_Log(...) ConColorMsg( Color( 255, 134, 44, 255 ), "[GamepadUI] " __VA_ARGS__ )
#endif

#endif // IGAMEPADUI_H
