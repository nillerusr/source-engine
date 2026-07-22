#ifndef IGAMEPADUI_H
#define IGAMEPADUI_H
#ifdef _WIN32
#pragma once
#endif

#include "tier1/interface.h"
#include "vgui/VGUI.h"
#include "mathlib/vector.h"
#include "ivrenderview.h"

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
};

#define GAMEPADUI_INTERFACE_VERSION "GamepadUI001"

// Lil easter egg :-)
#ifdef GAMEPADUI_GAME_PORTAL
#define GamepadUI_Log(...) ConColorMsg( Color( 61, 189, 237, 255 ), "[GamepadUI] " __VA_ARGS__ )
#else
#define GamepadUI_Log(...) ConColorMsg( Color( 255, 134, 44, 255 ), "[GamepadUI] " __VA_ARGS__ )
#endif

#endif // IGAMEPADUI_H
