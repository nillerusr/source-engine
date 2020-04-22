//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "util.h"
#include "weapon_tfc_crowbar.h"
#include "decals.h"

#if defined( CLIENT_DLL )
	#include "c_tfc_player.h"
#else
	#include "tfc_player.h"
#endif


#define	KNIFE_BODYHIT_VOLUME 128
#define	KNIFE_WALLHIT_VOLUME 512


static ConVar tfc_crowbar_damage_first( "tfc_crowbar_damage_first", "25", 0, "First crowbar hit damage." );
static ConVar tfc_crowbar_damage_next( "tfc_crowbar_damage_next", "12.5", 0, "Crowbar hit damage after first hit." );
		

static Vector head_hull_mins( -16, -16, -18 );
static Vector head_hull_maxs( 16, 16, 18 );


// ----------------------------------------------------------------------------- //
// CTFCCrowbar tables.
// ----------------------------------------------------------------------------- //

IMPLEMENT_NETWORKCLASS_ALIASED( TFCCrowbar, DT_WeaponCrowbar )

BEGIN_NETWORK_TABLE( CTFCCrowbar, DT_WeaponCrowbar )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CTFCCrowbar )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_crowbar, CTFCCrowbar );
PRECACHE_WEAPON_REGISTER( weapon_crowbar );

#ifndef CLIENT_DLL

	BEGIN_DATADESC( CTFCCrowbar )
		DEFINE_FUNCTION( Smack )
	END_DATADESC()

#endif

// ----------------------------------------------------------------------------- //
// CTFCCrowbar implementation.
// ----------------------------------------------------------------------------- //

CTFCCrowbar::CTFCCrowbar()
{
}


bool CTFCCrowbar::HasPrimaryAmmo()
{
	return true;
}


bool CTFCCrowbar::CanBeSelected()
{
	return true;
}

void CTFCCrowbar::Precache()
{
	BaseClass::Precache();
}

void CTFCCrowbar::Spawn()
{
	Precache();

	m_iClip1 = -1;
	BaseClass::Spawn();
}


bool CTFCCrowbar::Deploy()
{
	CPASAttenuationFilter filter( this );
	filter.UsePredictionRules();
	EmitSound( filter, entindex(), "Weapon_Crowbar.Deploy" );

	return BaseClass::Deploy();
}

void CTFCCrowbar::Holster( int skiplocal )
{
	GetPlayerOwner()->m_flNextAttack = gpGlobals->curtime + 0.5;
}

void FindHullIntersection( const Vector &vecSrc, trace_t &tr, const Vector &mins, const Vector &maxs, CBaseEntity *pEntity )
{
	int			i, j, k;
	float		distance;
	Vector minmaxs[2] = {mins, maxs};
	trace_t tmpTrace;
	Vector		vecHullEnd = tr.endpos;
	Vector		vecEnd;

	distance = 1e6f;

	vecHullEnd = vecSrc + ((vecHullEnd - vecSrc)*2);
	UTIL_TraceLine( vecSrc, vecHullEnd, MASK_SOLID, pEntity, COLLISION_GROUP_NONE, &tmpTrace );
	if ( tmpTrace.fraction < 1.0 )
	{
		tr = tmpTrace;
		return;
	}

	for ( i = 0; i < 2; i++ )
	{
		for ( j = 0; j < 2; j++ )
		{
			for ( k = 0; k < 2; k++ )
			{
				vecEnd.x = vecHullEnd.x + minmaxs[i][0];
				vecEnd.y = vecHullEnd.y + minmaxs[j][1];
				vecEnd.z = vecHullEnd.z + minmaxs[k][2];

				UTIL_TraceLine( vecSrc, vecEnd, MASK_SOLID, pEntity, COLLISION_GROUP_NONE, &tmpTrace );
				if ( tmpTrace.fraction < 1.0 )
				{
					float thisDistance = (tmpTrace.endpos - vecSrc).Length();
					if ( thisDistance < distance )
					{
						tr = tmpTrace;
						distance = thisDistance;
					}
				}
			}
		}
	}
}


void CTFCCrowbar::ItemPostFrame()
{
	// Store this off so we can detect if it's our first swing or not later on.
	m_flStoredPrimaryAttack = m_flNextPrimaryAttack;
	BaseClass::ItemPostFrame();
}


void CTFCCrowbar::PrimaryAttack()
{
	CTFCPlayer *pPlayer = GetPlayerOwner();

	Vector vForward;
	AngleVectors( pPlayer->EyeAngles(), &vForward );
	Vector vecSrc	= pPlayer->Weapon_ShootPosition();
	Vector vecEnd	= vecSrc + vForward * 32;

	trace_t tr;
	UTIL_TraceLine( vecSrc, vecEnd, MASK_SOLID, pPlayer, COLLISION_GROUP_NONE, &tr );

	if ( tr.fraction >= 1.0 )
	{
		UTIL_TraceHull( vecSrc, vecEnd, head_hull_mins, head_hull_maxs, MASK_SOLID, pPlayer, COLLISION_GROUP_NONE, &tr );
		if ( tr.fraction < 1.0 )
		{
			// Calculate the point of intersection of the line (or hull) and the object we hit
			// This is and approximation of the "best" intersection
			CBaseEntity *pHit = tr.m_pEnt;
			if ( !pHit || pHit->IsBSPModel() )
				FindHullIntersection( vecSrc, tr, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX, pPlayer );
			vecEnd = tr.endpos;	// This is the point on the actual surface (the hull could have hit space)
		}
	}

	bool bDidHit = tr.fraction < 1.0f;

#ifndef CLIENT_DLL
	bool bFirstSwing = (gpGlobals->curtime - m_flStoredPrimaryAttack) >= 1;
#endif

	pPlayer->DoAnimationEvent( PLAYERANIMEVENT_FIRE_GUN );

	m_flTimeWeaponIdle = gpGlobals->curtime + 2;
	m_flNextPrimaryAttack = gpGlobals->curtime + 0.4f;

	if ( bDidHit )
	{
		SendWeaponAnim( ACT_VM_HITCENTER );
	}
	else
	{
		// Allow for there only being hit activities.
		if ( !SendWeaponAnim( ACT_VM_MISSCENTER ) )
			SendWeaponAnim( ACT_VM_HITCENTER );

		// play wiff or swish sound
		WeaponSound( MELEE_MISS );
	}

	bool bPlayImpactEffect = false;
	
#ifndef CLIENT_DLL

	if ( bDidHit )
	{
		CBaseEntity *pEntity = tr.m_pEnt;
		
		ClearMultiDamage();

		float flDamage = 0;
		bool bDoEffects = true;
		AxeHit( pEntity, bFirstSwing, tr, &flDamage, &bDoEffects );
		if ( flDamage != 0 )
		{
			CTakeDamageInfo info( pPlayer, pPlayer, flDamage, DMG_CLUB | DMG_NEVERGIB );

			CalculateMeleeDamageForce( &info, vForward, tr.endpos, 1.0f/flDamage );
			pEntity->DispatchTraceAttack( info, vForward, &tr ); 
			ApplyMultiDamage();
		}

		if ( bDoEffects )
		{
			if ( pEntity && pEntity->IsPlayer() )
			{
				WeaponSound( MELEE_HIT );

				if ( pEntity->IsAlive() )
					bPlayImpactEffect = true; // no blood effect on dead bodies
			}
			else
			{
				bPlayImpactEffect = true; // always show impact effects on world objects
			}
		}
		else
		{
			bDoEffects = false;
		}
	}


#endif

	if ( bPlayImpactEffect )
	{
		// delay the decal a bit
		m_trHit = tr;
		
		// Store the ent in an EHANDLE, just in case it goes away by the time we get into our think function.
		m_pTraceHitEnt = tr.m_pEnt; 

		SetThink( &CTFCCrowbar::Smack );
		SetNextThink( gpGlobals->curtime + 0.2f );
	}
}


void CTFCCrowbar::Smack()
{
	m_trHit.m_pEnt = m_pTraceHitEnt;
	UTIL_ImpactTrace( &m_trHit, DMG_CLUB );
	
	surfacedata_t *psurf = physprops->GetSurfaceData( m_trHit.surface.surfaceProps );
	if ( psurf->game.material != CHAR_TEX_FLESH && psurf->game.material != CHAR_TEX_BLOODYFLESH )
		WeaponSound( MELEE_HIT_WORLD );
}



void CTFCCrowbar::WeaponIdle()
{
	//ResetEmptySound();

	CTFCPlayer *pPlayer = GetPlayerOwner();

	if (m_flTimeWeaponIdle > gpGlobals->curtime)
		return;

	m_flTimeWeaponIdle = gpGlobals->curtime + 20;

	// only idle if the slid isn't back
	SendWeaponAnim( ACT_VM_IDLE );
}


bool CTFCCrowbar::CanDrop()
{
	return false;
}


TFCWeaponID CTFCCrowbar::GetWeaponID( void ) const
{ 
	return WEAPON_CROWBAR;
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

	void CTFCCrowbar::AxeHit( CBaseEntity *pHit, bool bFirstSwing, trace_t &tr, float *flDamage, bool *bDoEffects )
	{
		if ( bFirstSwing )
			*flDamage = tfc_crowbar_damage_first.GetFloat();
		else
			*flDamage = tfc_crowbar_damage_next.GetFloat();
	}

#endif
