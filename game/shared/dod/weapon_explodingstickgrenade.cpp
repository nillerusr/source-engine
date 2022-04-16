//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "weapon_dodbasegrenade.h"

#ifdef CLIENT_DLL
	#define CWeaponExplodingStickGrenade C_WeaponExplodingStickGrenade
#else
	#include "dod_stickgrenade.h"	//the thing that we throw
#endif

class CWeaponExplodingStickGrenade : public CWeaponDODBaseGrenade
{
public:
	DECLARE_CLASS( CWeaponExplodingStickGrenade, CWeaponDODBaseGrenade );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CWeaponExplodingStickGrenade() {}

	virtual DODWeaponID GetWeaponID( void ) const		{ return WEAPON_FRAG_GER_LIVE; }

#ifndef CLIENT_DLL

	virtual void EmitGrenade( Vector vecSrc, QAngle vecAngles, Vector vecVel, AngularImpulse angImpulse, CBasePlayer *pPlayer, float flLifeTime = GRENADE_FUSE_LENGTH )
	{
		// align the stickgrenade vertically and spin end over end
		QAngle vecNewAngles = QAngle(45,pPlayer->EyeAngles().y,0);
		AngularImpulse angNewImpulse = AngularImpulse( 0, 600, 0 );

		CDODStickGrenade::Create( vecSrc, vecNewAngles, vecVel, angNewImpulse, pPlayer, flLifeTime, GetWeaponID() );
	}

#endif

	virtual bool IsArmed( void ) { return true; }

	CWeaponExplodingStickGrenade( const CWeaponExplodingStickGrenade & ) {}
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponExplodingStickGrenade, DT_WeaponExplodingStickGrenade )

BEGIN_NETWORK_TABLE(CWeaponExplodingStickGrenade, DT_WeaponExplodingStickGrenade)
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponExplodingStickGrenade )	//MATTTODO: are these necessary?
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_frag_ger_live, CWeaponExplodingStickGrenade );
PRECACHE_WEAPON_REGISTER( weapon_frag_ger_live );