//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "weapon_dodbasegrenade.h"

#ifdef CLIENT_DLL
#define CWeaponRifleGrenadeGER_Live C_WeaponRifleGrenadeGER_Live
#else 
#include "dod_riflegrenade_ger.h"	//the thing that we throw
#endif


class CWeaponRifleGrenadeGER_Live : public CWeaponDODBaseGrenade
{
public:
	DECLARE_CLASS( CWeaponRifleGrenadeGER_Live, CWeaponDODBaseGrenade );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CWeaponRifleGrenadeGER_Live() {}

	virtual DODWeaponID GetWeaponID( void ) const		{ return WEAPON_RIFLEGREN_GER_LIVE; }

#ifndef CLIENT_DLL

	virtual void EmitGrenade( Vector vecSrc, QAngle vecAngles, Vector vecVel, AngularImpulse angImpulse, CBasePlayer *pPlayer, float flLifeTime = GRENADE_FUSE_LENGTH )
	{
		// align the grenade vertically and spin end over end
		QAngle vecNewAngles = QAngle(45,pPlayer->EyeAngles().y,0);
		AngularImpulse angNewImpulse = AngularImpulse( 0, 600, 0 );

		CDODRifleGrenadeGER::Create( vecSrc, vecNewAngles, vecVel, angNewImpulse, pPlayer, flLifeTime, GetWeaponID() );
	}

#endif

	virtual bool IsArmed( void ) { return true; }

private:
	CWeaponRifleGrenadeGER_Live( const CWeaponRifleGrenadeGER_Live & );
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponRifleGrenadeGER_Live, DT_WeaponRifleGrenadeGER_Live )

BEGIN_NETWORK_TABLE(CWeaponRifleGrenadeGER_Live, DT_WeaponRifleGrenadeGER_Live)
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponRifleGrenadeGER_Live )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_riflegren_ger_live, CWeaponRifleGrenadeGER_Live );
PRECACHE_WEAPON_REGISTER( weapon_riflegren_ger_live );