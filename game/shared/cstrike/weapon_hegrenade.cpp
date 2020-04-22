//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "weapon_csbase.h"
#include "gamerules.h"
#include "npcevent.h"
#include "engine/IEngineSound.h"
#include "weapon_hegrenade.h"


#ifdef CLIENT_DLL
	
#else

	#include "cs_player.h"
	#include "items.h"
	#include "hegrenade_projectile.h"

#endif


#define GRENADE_TIMER	3.0f //Seconds



IMPLEMENT_NETWORKCLASS_ALIASED( HEGrenade, DT_HEGrenade )

BEGIN_NETWORK_TABLE(CHEGrenade, DT_HEGrenade)
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CHEGrenade )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_hegrenade, CHEGrenade );
PRECACHE_WEAPON_REGISTER( weapon_hegrenade );


#ifndef CLIENT_DLL

	BEGIN_DATADESC( CHEGrenade )
	END_DATADESC()

	void CHEGrenade::EmitGrenade( Vector vecSrc, QAngle vecAngles, Vector vecVel, AngularImpulse angImpulse, CBasePlayer *pPlayer )
	{
		//=============================================================================
		// HPE_BEGIN:
		// [Forrest] Start a new bullet group so that damage dealt by the grenade will be counted as a separate hit from damage previously dealt.
		//=============================================================================
#ifdef GAME_DLL
		CCSPlayer *pCSPlayer = ToCSPlayer( pPlayer );
		if ( pCSPlayer )
		{
			pCSPlayer->StartNewBulletGroup();
		}
#endif
		//=============================================================================
		// HPE_END
		//=============================================================================
		CHEGrenadeProjectile::Create( vecSrc, vecAngles, vecVel, angImpulse, pPlayer, GRENADE_TIMER );
	}
	
#endif

