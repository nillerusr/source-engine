//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "util.h"
#include "weapon_tfc_spanner.h"

#if defined( CLIENT_DLL )
	#include "c_tfc_player.h"
#else
	#include "tfc_player.h"
#endif


#define	KNIFE_BODYHIT_VOLUME 128
#define	KNIFE_WALLHIT_VOLUME 512


static ConVar tfc_spanner_damage_first( "tfc_spanner_damage_first", "25", 0, "First spanner hit damage." );
static ConVar tfc_spanner_damage_next( "tfc_spanner_damage_next", "12.5", 0, "Spanner hit damage after first hit." );
		

static Vector head_hull_mins( -16, -16, -18 );
static Vector head_hull_maxs( 16, 16, 18 );


// ----------------------------------------------------------------------------- //
// CTFCSpanner tables.
// ----------------------------------------------------------------------------- //

IMPLEMENT_NETWORKCLASS_ALIASED( TFCSpanner, DT_WeaponSpanner )

BEGIN_NETWORK_TABLE( CTFCSpanner, DT_WeaponSpanner )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CTFCSpanner )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_spanner, CTFCSpanner );
PRECACHE_WEAPON_REGISTER( weapon_spanner );

#ifndef CLIENT_DLL

	BEGIN_DATADESC( CTFCSpanner )
		DEFINE_FUNCTION( Smack )
	END_DATADESC()

#endif

// ----------------------------------------------------------------------------- //
// CTFCSpanner implementation.
// ----------------------------------------------------------------------------- //

CTFCSpanner::CTFCSpanner()
{
}


void CTFCSpanner::Precache()
{
	BaseClass::Precache();

	PrecacheScriptSound( "Weapon_Spanner.Slash" );
	PrecacheScriptSound( "Weapon_Spanner.HitFlesh" );
}


TFCWeaponID CTFCSpanner::GetWeaponID( void ) const
{ 
	return WEAPON_SPANNER;
}


#ifdef CLIENT_DLL

	// ------------------------------------------------------------------------------------------------ //
	// ------------------------------------------------------------------------------------------------ //
	// CLIENT DLL SPECIFIC CODE
	// ------------------------------------------------------------------------------------------------ //
	// ------------------------------------------------------------------------------------------------ //


#else

	// ------------------------------------------------------------------------------------------------ //
	// ------------------------------------------------------------------------------------------------ //
	// GAME DLL SPECIFIC CODE
	// ------------------------------------------------------------------------------------------------ //
	// ------------------------------------------------------------------------------------------------ //

	void CTFCSpanner::AxeHit( CBaseEntity *pTarget, bool bFirstSwing, trace_t &tr, float *flDamage, bool *bDoEffects )
	{
		CTFCPlayer *pPlayer = GetPlayerOwner();
		if ( !pPlayer )
			return;

		// Check to see if it's a trigger that's activatable by a Spanner hit
		
		// If it's not on our team, whack it.
		if ( !pPlayer->IsAlly( pTarget ) )
		{
			*flDamage = 20;
			return;
		}

		// Otherwise, the Engineer can repair his buildings or repair his teammate's armor.
		variant_t voidVariant;
		*bDoEffects = pTarget->AcceptInput( "EngineerUse", pPlayer, this, voidVariant, 0 );
	}

#endif
