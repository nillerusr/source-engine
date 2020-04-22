//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "weapon_dodbasegrenade.h"

#ifdef CLIENT_DLL
#define CWeaponRifleGrenadeUS_Live C_WeaponRifleGrenadeUS_Live
#else 
#include "dod_riflegrenade_us.h"	//the thing that we throw
#endif


class CWeaponRifleGrenadeUS_Live : public CWeaponDODBaseGrenade
{
public:
	DECLARE_CLASS( CWeaponRifleGrenadeUS_Live, CWeaponDODBaseGrenade );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CWeaponRifleGrenadeUS_Live() {}

	virtual DODWeaponID GetWeaponID( void ) const		{ return WEAPON_RIFLEGREN_US_LIVE; }

#ifndef CLIENT_DLL

	virtual void EmitGrenade( Vector vecSrc, QAngle vecAngles, Vector vecVel, AngularImpulse angImpulse, CBasePlayer *pPlayer, float flLifeTime = GRENADE_FUSE_LENGTH )
	{
		// align the grenade vertically and spin end over end
		QAngle vecNewAngles = QAngle(45,pPlayer->EyeAngles().y,0);
		AngularImpulse angNewImpulse = AngularImpulse( 0, 600, 0 );

		CDODRifleGrenadeUS::Create( vecSrc, vecNewAngles, vecVel, angNewImpulse, pPlayer, flLifeTime, GetWeaponID() );
	}

#endif

	virtual bool IsArmed( void ) { return true; }

private:
	CWeaponRifleGrenadeUS_Live( const CWeaponRifleGrenadeUS_Live & );
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponRifleGrenadeUS_Live, DT_WeaponRifleGrenadeUS_Live )

BEGIN_NETWORK_TABLE(CWeaponRifleGrenadeUS_Live, DT_WeaponRifleGrenadeUS_Live)
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponRifleGrenadeUS_Live )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_riflegren_us_live, CWeaponRifleGrenadeUS_Live );
PRECACHE_WEAPON_REGISTER( weapon_riflegren_us_live );