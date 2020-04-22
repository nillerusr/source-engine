//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: TF's derived BaseCombatWeapon
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_BASECOMBATWEAPON_H
#define TF_BASECOMBATWEAPON_H
#ifdef _WIN32
#pragma once
#endif

#include "baseplayer_shared.h"
#include "basetfplayer_shared.h"
#include "basetfcombatweapon_shared.h"

class CBaseObject;
class CBaseTechnology;
class CBaseTFPlayer;

//-----------------------------------------------------------------------------
// Purpose: Base TF Machinegun
//-----------------------------------------------------------------------------
class CTFMachineGun : public CBaseTFCombatWeapon
{
	DECLARE_CLASS( CTFMachineGun, CBaseTFCombatWeapon );
public:

	DECLARE_SERVERCLASS();

	virtual void	PrimaryAttack( void );
	virtual void	FireBullets( CBaseTFCombatWeapon *pWeapon, int cShots, const Vector &vecSrc, const Vector &vecDirShooting, const Vector &vecSpread, float flDistance, int iBulletType, int iTracerFreq);
	virtual const	Vector& GetBulletSpread( void );
	virtual float	GetFireRate( void );
};

#endif // TF_BASECOMBATWEAPON_H

