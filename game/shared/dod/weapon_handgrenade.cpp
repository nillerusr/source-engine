//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "weapon_dodbasegrenade.h"

#ifdef CLIENT_DLL
	#define CWeaponHandGrenade C_WeaponHandGrenade
#else
	#include "dod_handgrenade.h"	//the thing that we throw
#endif

class CWeaponHandGrenade : public CWeaponDODBaseGrenade
{
public:
	DECLARE_CLASS( CWeaponHandGrenade, CWeaponDODBaseGrenade );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CWeaponHandGrenade() {}

	virtual DODWeaponID GetWeaponID( void ) const		{ return WEAPON_FRAG_US; }

#ifndef CLIENT_DLL

	virtual void EmitGrenade( Vector vecSrc, QAngle vecAngles, Vector vecVel, AngularImpulse angImpulse, CBasePlayer *pPlayer, float flLifeTime = GRENADE_FUSE_LENGTH )
	{
		CDODHandGrenade::Create( vecSrc, vecAngles, vecVel, angImpulse, pPlayer, flLifeTime, GetWeaponID() );
	}

#endif

private:
	CWeaponHandGrenade( const CWeaponHandGrenade & );
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponHandGrenade, DT_WeaponHandGrenade )

BEGIN_NETWORK_TABLE(CWeaponHandGrenade, DT_WeaponHandGrenade)
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponHandGrenade )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_frag_us, CWeaponHandGrenade );
PRECACHE_WEAPON_REGISTER( weapon_frag_us );