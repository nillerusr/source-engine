//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:		Crowbar - an old favorite
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "convar.h"
#include "iconvar.h"
#include "hl1mp_basecombatweapon_shared.h"

#ifdef CLIENT_DLL
#include "c_baseplayer.h"
#include "fx_impact.h"
#include "fx.h"
#else
#include "player.h"
#include "soundent.h"
#endif
#include "decals.h"
#include "hl1_basecombatweapon_shared.h"

#include "gamerules.h"
#include "ammodef.h"
#include "mathlib/mathlib.h"
#include "in_buttons.h"

#include "vstdlib/random.h"
//For CrowbarVolume function
#include <string.h>

extern ConVar sk_plr_dmg_crowbar;

//new conVars for crowbar
//Crowbar Sounds
ConVar hl1_crowbar_sound("hl1_crowbar_sound", "1", FCVAR_NONE, NULL);
ConVar hl1_crowbar_concrete("hl1_crowbar_concrete", "1", FCVAR_NONE, NULL);
ConVar hl1_crowbar_metal("hl1_crowbar_metal", "1", FCVAR_NONE, NULL);
ConVar hl1_crowbar_dirt("hl1_crowbar_dirt", "1", FCVAR_NONE, NULL);
ConVar hl1_crowbar_vent("hl1_crowbar_vent", "1", FCVAR_NONE, NULL);
ConVar hl1_crowbar_grate("hl1_crowbar_grate", "1", FCVAR_NONE, NULL);
ConVar hl1_crowbar_tile("hl1_crowbar_tile", "1", FCVAR_NONE, NULL);
ConVar hl1_crowbar_wood("hl1_crowbar_wood", "1", FCVAR_NONE, NULL);
ConVar hl1_crowbar_glass("hl1_crowbar_glass", "1", FCVAR_NONE, NULL);
ConVar hl1_crowbar_computer("hl1_crowbar_computer", "1", FCVAR_NONE, NULL);

ConVar hl1_crowbar_concrete_vol("hl1_crowbar_concrete_vol", "1", FCVAR_NONE, NULL);
ConVar hl1_crowbar_metal_vol("hl1_crowbar_metal_vol", "1", FCVAR_NONE, NULL);
ConVar hl1_crowbar_dirt_vol("hl1_crowbar_dirt_vol", "1", FCVAR_NONE, NULL);
ConVar hl1_crowbar_vent_vol("hl1_crowbar_vent_vol", "1", FCVAR_NONE, NULL);
ConVar hl1_crowbar_grate_vol("hl1_crowbar_grate_vol", "1", FCVAR_NONE, NULL);
ConVar hl1_crowbar_tile_vol("hl1_crowbar_tile_vol", "1", FCVAR_NONE, NULL);
ConVar hl1_crowbar_wood_vol("hl1_crowbar_wood_vol", "1", FCVAR_NONE, NULL);
ConVar hl1_crowbar_glass_vol("hl1_crowbar_glass_vol", "1", FCVAR_NONE, NULL);
ConVar hl1_crowbar_computer_vol("hl1_crowbar_computer_vol", "1", FCVAR_NONE, NULL);

#define	CROWBAR_RANGE		64.0f
#define	CROWBAR_REFIRE_MISS	0.5f
#define	CROWBAR_REFIRE_HIT	0.25f


#ifdef CLIENT_DLL
#define CWeaponCrowbar C_WeaponCrowbar
#endif


struct TextureVolume
{
    char type;
    ConVar* enabledVar;
    ConVar* volumeVar;
};
//-----------------------------------------------------------------------------
// CWeaponCrowbar
//-----------------------------------------------------------------------------
// Define texture types and their associated ConVars
    TextureVolume textureVolumes[] = {
        {CHAR_TEX_CONCRETE, &hl1_crowbar_concrete, &hl1_crowbar_concrete_vol},
        {CHAR_TEX_METAL, &hl1_crowbar_metal, &hl1_crowbar_metal_vol},
        {CHAR_TEX_DIRT, &hl1_crowbar_dirt, &hl1_crowbar_dirt_vol},
        {CHAR_TEX_VENT, &hl1_crowbar_vent, &hl1_crowbar_vent_vol},
        {CHAR_TEX_GRATE, &hl1_crowbar_grate, &hl1_crowbar_grate_vol},
        {CHAR_TEX_TILE, &hl1_crowbar_tile, &hl1_crowbar_tile_vol},
        {CHAR_TEX_WOOD, &hl1_crowbar_wood, &hl1_crowbar_wood_vol},
        {CHAR_TEX_GLASS, &hl1_crowbar_glass, &hl1_crowbar_glass_vol},
        {CHAR_TEX_COMPUTER, &hl1_crowbar_computer, &hl1_crowbar_computer_vol}
    };

float GetCrowbarVolume(const trace_t &ptr)
{
    if (!hl1_crowbar_sound.GetBool())
        return 0.0f;

    float fvol = 0.0;
    // Assume we determine the texture type based on entity properties or surface data
    char mType = 0;

    if (ptr.surface.name)
    {
        // Check the surface name for certain substrings to classify texture types
        if (strstr(ptr.surface.name, "concrete")) mType = CHAR_TEX_CONCRETE;
        else if (strstr(ptr.surface.name, "metal")) mType = CHAR_TEX_METAL;
        else if (strstr(ptr.surface.name, "dirt")) mType = CHAR_TEX_DIRT;
        else if (strstr(ptr.surface.name, "vent")) mType = CHAR_TEX_VENT;
        else if (strstr(ptr.surface.name, "grate")) mType = CHAR_TEX_GRATE;
        else if (strstr(ptr.surface.name, "tile")) mType = CHAR_TEX_TILE;
        else if (strstr(ptr.surface.name, "wood")) mType = CHAR_TEX_WOOD;
        else if (strstr(ptr.surface.name, "glass")) mType = CHAR_TEX_GLASS;
        else if (strstr(ptr.surface.name, "computer")) mType = CHAR_TEX_COMPUTER;
    }

    for (const auto& tv : textureVolumes)
    {
        if (mType == tv.type && tv.enabledVar->GetBool())
        {
            fvol = tv.volumeVar->GetFloat() / 10;
            break;
        }
    }

    CBaseEntity* pEntity = ptr.m_pEnt;
    if (pEntity && FClassnameIs(pEntity, "func_breakable"))
    {
        fvol /= 2.0;
    }

    return fvol;
}

class CWeaponCrowbar : public CBaseHL1MPCombatWeapon
{
	DECLARE_CLASS( CWeaponCrowbar, CBaseHL1MPCombatWeapon );
public:
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
#ifndef CLIENT_DLL
	DECLARE_DATADESC();
#endif

	CWeaponCrowbar();

	void			Precache( void );
	virtual void	ItemPostFrame( void );
	void			PrimaryAttack( void );

public:
	trace_t		m_traceHit;
	Activity	m_nHitActivity;

private:
	virtual void		Swing( void );
	virtual	void		Hit( void );
	virtual	void		ImpactEffect( void );
	void		ImpactSound( CBaseEntity *pHitEntity);
	virtual Activity	ChooseIntersectionPointAndActivity( trace_t &hitTrace, const Vector &mins, const Vector &maxs, CBasePlayer *pOwner );

public:

};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponCrowbar, DT_WeaponCrowbar );

BEGIN_NETWORK_TABLE( CWeaponCrowbar, DT_WeaponCrowbar )
/// what
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponCrowbar )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_crowbar, CWeaponCrowbar );
PRECACHE_WEAPON_REGISTER( weapon_crowbar );

#ifndef CLIENT_DLL
BEGIN_DATADESC( CWeaponCrowbar )

	// DEFINE_FIELD( m_trLineHit, trace_t ),
	// DEFINE_FIELD( m_trHullHit, trace_t ),
	// DEFINE_FIELD( m_nHitActivity, FIELD_INTEGER ),
	// DEFINE_FIELD( m_traceHit, trace_t ),

	// Class CWeaponCrowbar:
	// DEFINE_FIELD( m_nHitActivity, FIELD_INTEGER ),

	// Function Pointers
	DEFINE_FUNCTION( Hit ),

END_DATADESC()
#endif

#define BLUDGEON_HULL_DIM		16

static const Vector g_bludgeonMins(-BLUDGEON_HULL_DIM,-BLUDGEON_HULL_DIM,-BLUDGEON_HULL_DIM);
static const Vector g_bludgeonMaxs(BLUDGEON_HULL_DIM,BLUDGEON_HULL_DIM,BLUDGEON_HULL_DIM);

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CWeaponCrowbar::CWeaponCrowbar()
{
	m_bFiresUnderwater = true;
}

//-----------------------------------------------------------------------------
// Purpose: Precache the weapon
//-----------------------------------------------------------------------------
void CWeaponCrowbar::Precache( void )
{
	//Call base class first
	BaseClass::Precache();
}

//------------------------------------------------------------------------------
// Purpose : Update weapon
//------------------------------------------------------------------------------
void CWeaponCrowbar::ItemPostFrame( void )
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	
	if ( pOwner == NULL )
		return;

	if ( (pOwner->m_nButtons & IN_ATTACK) && (m_flNextPrimaryAttack <= gpGlobals->curtime) )
	{
		PrimaryAttack();
	} 
	else 
	{
		WeaponIdle();
		return;
	}
}

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CWeaponCrowbar::PrimaryAttack()
{
	this->Swing();
}


//------------------------------------------------------------------------------
// Purpose: Implement impact function
//------------------------------------------------------------------------------
void CWeaponCrowbar::Hit( void )
{
	//Make sound for the AI
#ifndef CLIENT_DLL

	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );

	CSoundEnt::InsertSound( SOUND_BULLET_IMPACT, m_traceHit.endpos, 400, 0.2f, pPlayer );

	CBaseEntity	*pHitEntity = m_traceHit.m_pEnt;

	//Apply damage to a hit target
	if ( pHitEntity != NULL )
	{
		Vector hitDirection;
		pPlayer->EyeVectors( &hitDirection, NULL, NULL );
		VectorNormalize( hitDirection );

		ClearMultiDamage();
		CTakeDamageInfo info( GetOwner(), GetOwner(), sk_plr_dmg_crowbar.GetFloat(), DMG_CLUB );
		CalculateMeleeDamageForce( &info, hitDirection, m_traceHit.endpos );
		pHitEntity->DispatchTraceAttack( info, hitDirection, &m_traceHit ); 
		ApplyMultiDamage();

		// Now hit all triggers along the ray that... 
		TraceAttackToTriggers( CTakeDamageInfo( GetOwner(), GetOwner(), sk_plr_dmg_crowbar.GetFloat(), DMG_CLUB ), m_traceHit.startpos, m_traceHit.endpos, hitDirection );

		//Play an impact sound	
		ImpactSound( pHitEntity );
	}
#endif

	//Apply an impact effect
	ImpactEffect();
}

//-----------------------------------------------------------------------------
// Purpose: Play the impact sound
// Input  : pHitEntity - entity that we hit
// assumes pHitEntity is not null
//-----------------------------------------------------------------------------
void CWeaponCrowbar::ImpactSound( CBaseEntity *pHitEntity )
{
	bool bIsWorld = ( pHitEntity->entindex() == 0 );
#ifndef CLIENT_DLL
	if ( !bIsWorld )
	{
		bIsWorld |=	pHitEntity->Classify() == CLASS_NONE ||  pHitEntity->Classify() == CLASS_MACHINE;
	}
#endif
    trace_t Trace;
	memset(&Trace, 0, sizeof(trace_t));
    Trace.m_pEnt = pHitEntity;

    float m_SoundTexture = GetCrowbarVolume(Trace);
	if (bIsWorld)
    {
        WeaponSound(MELEE_HIT_WORLD, m_SoundTexture);
    }
    else
    {
        WeaponSound(MELEE_HIT, m_SoundTexture);
    }
}

Activity CWeaponCrowbar::ChooseIntersectionPointAndActivity( trace_t &hitTrace, const Vector &mins, const Vector &maxs, CBasePlayer *pOwner )
{
	int			i, j, k;
	float		distance;
	const float	*minmaxs[2] = {mins.Base(), maxs.Base()};
	trace_t		tmpTrace;
	Vector		vecHullEnd = hitTrace.endpos;
	Vector		vecEnd;

	distance = 1e6f;
	Vector vecSrc = hitTrace.startpos;

	vecHullEnd = vecSrc + ((vecHullEnd - vecSrc)*2);
	UTIL_TraceLine( vecSrc, vecHullEnd, MASK_SHOT_HULL, pOwner, COLLISION_GROUP_NONE, &tmpTrace );
	if ( tmpTrace.fraction == 1.0 )
	{
		for ( i = 0; i < 2; i++ )
		{
			for ( j = 0; j < 2; j++ )
			{
				for ( k = 0; k < 2; k++ )
				{
					vecEnd.x = vecHullEnd.x + minmaxs[i][0];
					vecEnd.y = vecHullEnd.y + minmaxs[j][1];
					vecEnd.z = vecHullEnd.z + minmaxs[k][2];

					UTIL_TraceLine( vecSrc, vecEnd, MASK_SHOT_HULL, pOwner, COLLISION_GROUP_NONE, &tmpTrace );
					if ( tmpTrace.fraction < 1.0 )
					{
						float thisDistance = (tmpTrace.endpos - vecSrc).Length();
						if ( thisDistance < distance )
						{
							hitTrace = tmpTrace;
							distance = thisDistance;
						}
					}
				}
			}
		}
	}
	else
	{
		hitTrace = tmpTrace;
	}


	return ACT_VM_HITCENTER;
}

#ifdef HL1MP_CLIENT_DLL
//-----------------------------------------------------------------------------
// Purpose: Handle jeep impacts
//-----------------------------------------------------------------------------
void ImpactCrowbarCallback( const CEffectData &data )
{
	trace_t tr;
	Vector vecOrigin, vecStart, vecShotDir;
	int iMaterial, iDamageType, iHitbox;
	short nSurfaceProp;
	C_BaseEntity *pEntity = ParseImpactData( data, &vecOrigin, &vecStart, &vecShotDir, nSurfaceProp, iMaterial, iDamageType, iHitbox );

	bool bIsWorld = ( pEntity->entindex() == 0 );

	if ( !pEntity )
	{
		// This happens for impacts that occur on an object that's then destroyed.
		// Clear out the fraction so it uses the server's data
		tr.fraction = 1.0;
		GetActiveWeapon()->WeaponSound( bIsWorld ? MELEE_HIT_WORLD : MELEE_HIT );
		return;
	}

	// If we hit, perform our custom effects and play the sound
	if ( Impact( vecOrigin, vecStart, iMaterial, iDamageType, iHitbox, pEntity, tr ) )
	{
		// Check for custom effects based on the Decal index
		PerformCustomEffects( vecOrigin, tr, vecShotDir, iMaterial, 2 );
	}

	GetActiveWeapon()->WeaponSound( bIsWorld ? MELEE_HIT_WORLD : MELEE_HIT );
}

DECLARE_CLIENT_EFFECT( "ImpactCrowbar", ImpactCrowbarCallback );

#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponCrowbar::ImpactEffect( void )
{
	//FIXME: need new decals
#ifdef HL1MP_CLIENT_DLL
	// in hl1mp force the basic crowbar sound
	UTIL_ImpactTrace( &m_traceHit, DMG_CLUB, "ImpactCrowbar" );
#else
	UTIL_ImpactTrace( &m_traceHit, DMG_CLUB );
#endif
}

//------------------------------------------------------------------------------
// Purpose : Starts the swing of the weapon and determines the animation
//------------------------------------------------------------------------------
void CWeaponCrowbar::Swing( void )
{
	// Try a ray
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( !pOwner )
		return;

	Vector swingStart = pOwner->Weapon_ShootPosition( );
	Vector forward;

	pOwner->EyeVectors( &forward, NULL, NULL );

	Vector swingEnd = swingStart + forward * CROWBAR_RANGE;

	UTIL_TraceLine( swingStart, swingEnd, MASK_SHOT_HULL, pOwner, COLLISION_GROUP_NONE, &m_traceHit );
	m_nHitActivity = ACT_VM_HITCENTER;

	if ( m_traceHit.fraction == 1.0 )
	{
		float bludgeonHullRadius = 1.732f * BLUDGEON_HULL_DIM;  // hull is +/- 16, so use cuberoot of 2 to determine how big the hull is from center to the corner point

		// Back off by hull "radius"
		swingEnd -= forward * bludgeonHullRadius;

		UTIL_TraceHull( swingStart, swingEnd, g_bludgeonMins, g_bludgeonMaxs, MASK_SHOT_HULL, pOwner, COLLISION_GROUP_NONE, &m_traceHit );
		if ( m_traceHit.fraction < 1.0 )
		{
			m_nHitActivity = ChooseIntersectionPointAndActivity( m_traceHit, g_bludgeonMins, g_bludgeonMaxs, pOwner );
		}
	}


	// -------------------------
	//	Miss
	// -------------------------
	if ( m_traceHit.fraction == 1.0f )
	{
		m_nHitActivity = ACT_VM_MISSCENTER;

		//Play swing sound
		WeaponSound( SINGLE );

		//Setup our next attack times
		m_flNextPrimaryAttack = gpGlobals->curtime + CROWBAR_REFIRE_MISS;
	}
	else
	{
		Hit();

		//Setup our next attack times
		m_flNextPrimaryAttack = gpGlobals->curtime + CROWBAR_REFIRE_HIT;
	}

	//Send the anim
	SendWeaponAnim( m_nHitActivity );
	pOwner->SetAnimation( PLAYER_ATTACK1 );
}
