//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Clients TF Base combat weapon
//
// $NoKeywords: $
//=============================================================================//
#ifndef C_TF_BASECOMBATWEAPON_H
#define C_TF_BASECOMBATWEAPON_H
#ifdef _WIN32
#pragma once
#endif

#include "c_baseanimating.h"

class CViewSetup;
class C_BaseTFPlayer;

// Crosshair info positions
#define CROSSHAIR_HEALTH_TOP		YRES(266)
#define CROSSHAIR_HEALTH_OFFSET		XRES(80)
#define CROSSHAIR_HEALTH_LEFT		((ScreenWidth() - CROSSHAIR_HEALTH_OFFSET) / 2)
#define CROSSHAIR_AMMO_TOP			CROSSHAIR_HEALTH_TOP
#define CROSSHAIR_AMMO_LEFT			(ScreenWidth() / 2)

namespace vgui
{
	class Label;
};

#include "basetfcombatweapon_shared.h"

class C_TFMachineGun : public C_BaseTFCombatWeapon
{
private:
	DECLARE_CLASS( C_TFMachineGun, C_BaseTFCombatWeapon );

public:
	C_TFMachineGun() {}

	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();

private:
	C_TFMachineGun( const C_TFMachineGun & );
};

#endif // C_TF_BASECOMBATWEAPON_H
