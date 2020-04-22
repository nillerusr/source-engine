//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "weapon_dodbasegrenade.h"

#ifdef CLIENT_DLL
#define CWeaponSmokeGrenadeGER C_WeaponSmokeGrenadeGER
#else
#include "dod_smokegrenade_ger.h"	//the thing that we throw
#endif

class CWeaponSmokeGrenadeGER : public CWeaponDODBaseGrenade
{
public:
	DECLARE_CLASS( CWeaponSmokeGrenadeGER, CWeaponDODBaseGrenade );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CWeaponSmokeGrenadeGER() {}

	virtual DODWeaponID GetWeaponID( void ) const		{ return WEAPON_SMOKE_GER; }

#ifndef CLIENT_DLL

	virtual void EmitGrenade( Vector vecSrc, QAngle vecAngles, Vector vecVel, AngularImpulse angImpulse, CBasePlayer *pPlayer, float flLifeTime = GRENADE_FUSE_LENGTH )
	{
		// align the stickgrenade vertically and spin end over end
		QAngle vecNewAngles = QAngle(45,pPlayer->EyeAngles().y,0);
		AngularImpulse angNewImpulse = AngularImpulse( 0, 600, 0 );

		CDODSmokeGrenadeGER::Create( vecSrc, vecNewAngles, vecVel, angNewImpulse, pPlayer );
	}

#endif

	virtual float GetDetonateTimerLength( void ) { return 10; }

private:
	CWeaponSmokeGrenadeGER( const CWeaponSmokeGrenadeGER & ) {}
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponSmokeGrenadeGER, DT_WeaponSmokeGrenadeGER )

BEGIN_NETWORK_TABLE(CWeaponSmokeGrenadeGER, DT_WeaponSmokeGrenadeGER)
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponSmokeGrenadeGER )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_smoke_ger, CWeaponSmokeGrenadeGER );
PRECACHE_WEAPON_REGISTER( weapon_smoke_ger );