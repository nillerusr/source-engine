//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
//---------------------------------------------------------
//---------------------------------------------------------
#include "cbase.h"
#include "decals.h"
#include "asw_fire.h"
#include "entitylist.h"
#include "basecombatcharacter.h"
#include "ndebugoverlay.h"
#include "engine/IEngineSound.h"
#include "ispatialpartition.h"
#include "collisionutils.h"
#include "tier0/vprof.h"

#ifdef INFESTED_DLL
// asw
#include "asw_marine.h"
#include "asw_marine_resource.h"
#include "asw_alien.h"
#include "asw_extinguisher_projectile.h"
#include "asw_generic_emitter_entity.h"
#include "asw_dynamic_light.h"
#include "asw_flamer_projectile.h"
#include "asw_gamerules.h"
#include "iasw_spawnable_npc.h"
#include "asw_shareddefs.h"
#include "soundenvelope.h"
#define ASW_MIN_FIRE_HEIGHT 50		// minimum units high the bounding box should be (so horizontal aiming of fire extinguisher always hits the fire when going over it, no matter how small it is)
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


/********************************************************************
 NOTE: if you are looking at this file becase you would like flares 
 to be considered as fires (and thereby trigger gas traps), be aware 
 that the env_flare class is actually found in weapon_flaregun.cpp 
 and is really a repurposed piece of ammunition. (env_flare isn't the 
 rod-like safety flare prop, but rather the bit of flame on the end.)

 You will have some difficulty making it work here, because CFlare 
 does not inherit from CFire and will thus not be enumerated by 
 CFireSphere::EnumElement(). In order to have flares be detected and 
 used by this system, you will need to promote certain member functions 
 of CFire into an interface class from which both CFire and CFlare 
 inherit. You will also need to modify CFireSphere::EnumElement so that
 it properly disambiguates between fires and flares.

 For some partial work towards this end, see changelist 192474.

 ********************************************************************/




#define	FIRE_HEIGHT				256.0f
#define FIRE_SCALE_FROM_SIZE(firesize)		(firesize * (1/FIRE_HEIGHT))

#define	FIRE_MAX_GROUND_OFFSET	24.0f	//(2 feet)

#define	DEFAULT_ATTACK_TIME	4.0f
#define	DEFAULT_DECAY_TIME	8.0f

// UNDONE: This shouldn't be constant but depend on specific fire
#define	FIRE_WIDTH				128
#define	FIRE_MINS				Vector(-20,-20,0 )   // Sould be FIRE_WIDTH in size
#define FIRE_MAXS				Vector( 20, 20,20)	 // Sould be FIRE_WIDTH in size
#define FIRE_SPREAD_DAMAGE_MULTIPLIER 2.0

#define FIRE_MAX_HEAT_LEVEL		64.0f
#define	FIRE_NORMAL_ATTACK_TIME	20.0f
#define FIRE_THINK_INTERVAL		0.1

ConVar fire_maxabsorb( "fire_maxabsorb", "50" );
ConVar fire_absorbrate( "fire_absorbrate", "3" );
ConVar fire_extscale("fire_extscale", "12");
ConVar fire_extabsorb("fire_extabsorb", "5");
ConVar fire_heatscale( "fire_heatscale", "1.0" );
ConVar fire_incomingheatscale( "fire_incomingheatscale", "0.1" );
ConVar fire_dmgscale( "fire_dmgscale", "0.1" );
ConVar fire_dmgbase( "fire_dmgbase", "1" );
ConVar fire_growthrate( "fire_growthrate", "1.0" );
ConVar fire_dmginterval( "fire_dmginterval", "1.0" );
#ifdef INFESTED_DLL
ConVar asw_fire_spread_scale( "asw_fire_spread_scale", "2.0", FCVAR_CHEAT);
ConVar asw_fire_glow_radius( "asw_fire_glow_radius", "280", FCVAR_CHEAT );
ConVar asw_fire_glow( "asw_fire_glow", "1", FCVAR_CHEAT );
#endif

#define VPROF_FIRE(s) VPROF( s )


CFireSphere::CFireSphere( CFire **pList, int listMax, bool onlyActiveFires, const Vector &origin, float radius )
{
	m_pList = pList;
	m_listMax = listMax;
	m_count = 0;
	m_onlyActiveFires = onlyActiveFires;
	m_origin = origin;
	m_radiusSqr = radius * radius;
}

bool CFireSphere::AddToList( CFire *pFire )
{
	if ( m_count >= m_listMax )
		return false;
	m_pList[m_count] = pFire;
	m_count++;
	return true;
}

IterationRetval_t CFireSphere::EnumElement( IHandleEntity *pHandleEntity )
{
	CBaseEntity *pEntity = gEntList.GetBaseEntity( pHandleEntity->GetRefEHandle() );
	if ( pEntity )
	{
		// UNDONE: Measure which of these is faster
//		CFire *pFire = dynamic_cast<CFire *>(pEntity);
		if ( !FClassnameIs( pEntity, "env_fire" ) )
			return ITERATION_CONTINUE;

		CFire *pFire = static_cast<CFire *>(pEntity);
		if ( pFire )
		{
			if ( !m_onlyActiveFires || pFire->IsBurning() )
			{
				if ( (m_origin - pFire->GetAbsOrigin()).LengthSqr() < m_radiusSqr )
				{
					if ( !AddToList( pFire ) )
						return ITERATION_STOP;
				}
			}
		}
	}

	return ITERATION_CONTINUE;
}


int FireSystem_GetFiresInSphere( CFire **pList, int listMax, bool onlyActiveFires, const Vector &origin, float radius )
{
	CFireSphere sphereEnum( pList, listMax, onlyActiveFires, origin, radius );
	partition->EnumerateElementsInSphere( PARTITION_ENGINE_NON_STATIC_EDICTS, origin, radius, false, &sphereEnum );

	return sphereEnum.GetCount();
}


bool FireSystem_IsValidFirePosition( const Vector &position, float testRadius )
{
	CFire *pList[1];
	int count = FireSystem_GetFiresInSphere( pList, ARRAYSIZE(pList), true, position, testRadius );
	if ( count > 0 )
		return false;
	return true;
}


//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
bool FireSystem_IsFireInWall( Vector &position, fireType_e type )
{
	// Don't check natural fire against walls
	if (type == FIRE_NATURAL)
		return false;

	trace_t tr;
	UTIL_TraceHull( position, position+Vector(0,0,0.1), FIRE_MINS,FIRE_MAXS,MASK_SOLID, NULL, COLLISION_GROUP_NONE, &tr );
	if (tr.fraction != 1.0 || tr.startsolid)
	{
		//NDebugOverlay::Box(position,FIRE_MINS,FIRE_MAXS,255,0,0,50,10);
		return true;
	}
	//NDebugOverlay::Box(position,FIRE_MINS,FIRE_MAXS,0,255,0,50,10);

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Determines whether or not a new fire may be placed at a given location
// Input  : &position - where we are trying to put the new fire
//			separationRadius - the maximum distance fires must be apart from one another
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool FireSystem_CanAddFire( Vector *position, float separationRadius, fireType_e type, int flags )
{
	//See if we found a fire inside the sphere
	if ( !FireSystem_IsValidFirePosition( *position, separationRadius ) )
		return false;

	// Unless our fire is floating, make sure were not too high
	if (!(flags & SF_FIRE_DONT_DROP))
	{
		trace_t	tr;
		Vector	startpos = *position;
		Vector	endpos = *position;

		startpos[2] += 1;
		endpos[2] -= FIRE_MAX_GROUND_OFFSET;

		UTIL_TraceLine( startpos, endpos, MASK_SOLID, NULL, COLLISION_GROUP_NONE, &tr );

#ifdef INFESTED_DLL
		// asw - don't check for being inside things, that's already done elsewhere, if needed
		//See if we're floating too high 
		if ( tr.fraction == 1.0f )
			return false;
#else
		//See if we're floating too high 
		if ( ( tr.allsolid ) || ( tr.startsolid) || ( tr.fraction == 1.0f ) )
		{
			return false;
		}

		//TODO: If we've hit an entity here, start it on fire
		CBaseEntity *pEntity = tr.m_pEnt;

		if ( ENTINDEX( pEntity->edict() ) != 0 )
		{
			return false;
		}
#endif // INFESTED_DLL
	}



	// Check if fire is in a wall, if so try shifting around a bit
	if (FireSystem_IsFireInWall( *position, type ))
	{
		Vector vTestPos = *position;
		vTestPos.x		+= 10;
		if (FireSystem_IsValidFirePosition( vTestPos, separationRadius ) && !FireSystem_IsFireInWall( vTestPos, type ))
		{
			*position = vTestPos;
			return true;
		}
		vTestPos.y		+= 10;
		if (FireSystem_IsValidFirePosition( vTestPos, separationRadius ) && !FireSystem_IsFireInWall( vTestPos, type ))
		{
			*position = vTestPos;
			return true;
		}
		vTestPos.y		-= 20;
		if (FireSystem_IsValidFirePosition( vTestPos, separationRadius ) && !FireSystem_IsFireInWall( vTestPos, type ))
		{
			*position = vTestPos;
			return true;
		}
		vTestPos.x		-= 20;
		if (FireSystem_IsValidFirePosition( vTestPos, separationRadius ) && !FireSystem_IsFireInWall( vTestPos, type ))
		{
			*position = vTestPos;
			return true;
		}
		return false;
	}

	//Able to add here
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Starts a fire at a specified location
// Input  : &position - position to start the fire at
//			flags - any special modifiers
//-----------------------------------------------------------------------------
bool FireSystem_StartFire( const Vector &position, float fireHeight, float attack, float fuel, int flags, CBaseEntity *owner, fireType_e type, float flRotation, CBaseEntity *pCreatorWeapon )
{
	VPROF_FIRE( "FireSystem_StartFire1" );

	Vector testPos = position;
	//Must be okay to add fire here
	if ( FireSystem_CanAddFire( &testPos, 16.0f, type, flags ) == false )
	{
		CFire *pFires[16];
		int fireCount = FireSystem_GetFiresInSphere( pFires, ARRAYSIZE(pFires), true, position, 16.0f );
		for ( int i = 0; i < fireCount; i++ )
		{
			// add to this fire
			pFires[i]->AddHeat( fireHeight, false );
		}

		return false;
	}

	//Create a new fire entity
	CFire *fire = (CFire *) CreateEntityByName( "env_fire" );
	
	if ( fire == NULL )
		return false;

	//Spawn the fire
	// Fires not placed by a designer should be cleaned up automatically (not catch fire again)
	fire->AddSpawnFlags( SF_FIRE_DIE_PERMANENT );
	fire->Spawn();
	fire->SetAbsAngles( QAngle( 0, flRotation, 0 ) );
	fire->Init( testPos, fireHeight, attack, fuel, flags, type );
	fire->Start();
	fire->SetOwner( owner );
	fire->m_hCreatorWeapon = pCreatorWeapon;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Starts a fire on a specified model.
// Input  : pEntity - The model entity to catch on fire.
//			fireHeight - 
//			attack - 
//			fuel - 
//			flags - 
//			owner - 
//			type - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool FireSystem_StartFire( CBaseAnimating *pEntity, float fireHeight, float attack, float fuel, int flags, CBaseEntity *owner, fireType_e type, float flRotation )
{
	VPROF_FIRE( "FireSystem_StartFire2" );

	Vector position = pEntity->GetAbsOrigin();
	Vector testPos = position;

	// Make sure its a valid position for fire (not in a wall, etc)
	if ( FireSystem_CanAddFire( &testPos, 16.0f, type, flags ) == false )
	{
		// Contribute heat to all fires within 16 units of this fire.
		CFire *pFires[16];
		int fireCount = FireSystem_GetFiresInSphere( pFires, ARRAYSIZE(pFires), true, position, 16.0f );
		for ( int i = 0; i < fireCount; i++ )
		{
			pFires[i]->AddHeat( fireHeight, false );
		}

		return false;
	}

	// Create a new fire entity
	CFire *fire = (CFire *) CreateEntityByName( "env_fire" );
	if ( fire == NULL )
	{
		return false;
	}

	// Spawn the fire.
	// Fires not placed by a designer should be cleaned up automatically (not catch fire again).
	fire->AddSpawnFlags( SF_FIRE_DIE_PERMANENT );
	fire->Spawn();
	fire->SetAbsAngles( QAngle( 0, flRotation, 0 ) );
	fire->Init( testPos, fireHeight, attack, fuel, flags, type );
	fire->Start();
	fire->SetOwner( owner );

	return true;
}

#ifdef INFESTED_DLL
bool FireSystem_StartDormantFire( const Vector &position, float fireHeight, float attack, float fuel, int flags, CBaseEntity *owner, fireType_e type )
{
	VPROF_FIRE( "FireSystem_StartFire3" );

	Vector testPos = position;
	//Must be okay to add fire here
	if ( FireSystem_CanAddFire( &testPos, 16.0f, type, flags ) == false )
	{
		CFire *pFires[16];
		int fireCount = FireSystem_GetFiresInSphere( pFires, ARRAYSIZE(pFires), true, position, 16.0f );
		for ( int i = 0; i < fireCount; i++ )
		{
			// add to this fire
			pFires[i]->AddHeat( fireHeight, false );
		}

		return false;
	}

	//Create a new fire entity
	CFire *fire = (CFire *) CreateEntityByName( "env_fire" );
	
	if ( fire == NULL )
		return false;

	//Spawn the fire
	// Fires not placed by a designer should be cleaned up automatically (not catch fire again)
	/*
	fire->AddSpawnFlags( flags );
	Msg("adding flags %d\n", flags);
	fire->AddSpawnFlags( SF_FIRE_DIE_PERMANENT );
	fire->SetHeatLevel(1);
	fire->Spawn();
	fire->Init( testPos, fireHeight, attack, fuel, flags, type );
	//fire->Start();
	fire->SetOwner( owner );
	
	*/

	fire->AddSpawnFlags( SF_FIRE_DIE_PERMANENT );
	fire->SetHeatLevel(0.001f);
	fire->SetHealth(fuel);	
	fire->Spawn();	
	fire->Init( testPos, fireHeight, attack, fuel, flags, type );	
	//fire->Start();
	fire->SetOwner( owner );

	trace_t		tr;
	UTIL_TraceLine(testPos + Vector(0,0,10), testPos + Vector(0,0,-30), MASK_SOLID, owner, 
		COLLISION_GROUP_NONE, &tr);
	if (tr.m_pEnt)
		UTIL_DecalTrace( &tr, "BeerSplash" );	// flamerfuel doesn't work..

	return true;
}
#endif

void FireSystem_ExtinguishInRadius( const Vector &origin, float radius, float rate )
{
	// UNDONE: pass this instead of percent
	float heat = (1-rate) * fire_extscale.GetFloat();

	CFire *pFires[32];
	int fireCount = FireSystem_GetFiresInSphere( pFires, ARRAYSIZE(pFires), false, origin, radius );
	for ( int i = 0; i < fireCount; i++ )
	{
		pFires[i]->Extinguish( heat );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &origin - 
//			radius - 
//			heat - 
//-----------------------------------------------------------------------------
void FireSystem_AddHeatInRadius( const Vector &origin, float radius, float heat )
{
	VPROF_FIRE( "FireSystem_AddHeatInRadius" );

	CFire *pFires[32];

	int fireCount = FireSystem_GetFiresInSphere( pFires, ARRAYSIZE(pFires), false, origin, radius );
	for ( int i = 0; i < fireCount; i++ )
	{
		pFires[i]->AddHeat( heat );
	}
}

//-----------------------------------------------------------------------------

bool FireSystem_GetFireDamageDimensions( CBaseEntity *pEntity, Vector *pFireMins, Vector *pFireMaxs )
{
	CFire *pFire = dynamic_cast<CFire *>(pEntity);

	if ( pFire && pFire->GetFireDimensions( pFireMins, pFireMaxs ) )
	{
		*pFireMins /= FIRE_SPREAD_DAMAGE_MULTIPLIER;
		*pFireMaxs /= FIRE_SPREAD_DAMAGE_MULTIPLIER;
		return true;
	}
	pFireMins->Init();
	pFireMaxs->Init();
	return false;
}


//==================================================
// CFire
//==================================================
IMPLEMENT_SERVERCLASS_ST( CFire, DT_ASW_Fire )
SendPropInt( SENDINFO( m_nFireType ) ),
SendPropFloat( SENDINFO( m_flFireSize ) ),
SendPropFloat( SENDINFO( m_flHeatLevel ) ),
SendPropFloat( SENDINFO( m_flMaxHeat ) ),
SendPropBool( SENDINFO( m_bEnabled ) ),
END_SEND_TABLE()

BEGIN_DATADESC( CFire )

	DEFINE_FIELD( m_hEffect, FIELD_EHANDLE ),
#ifdef INFESTED_DLL
	//DEFINE_FIELD( m_hEmitter, FIELD_EHANDLE ), // asw
	DEFINE_FIELD( m_hFireLight, FIELD_EHANDLE ), // asw
	
	DEFINE_KEYFIELD( m_fLightRadiusScale, FIELD_FLOAT, "LightRadiusScale" ),		// asw
	DEFINE_KEYFIELD( m_iLightBrightness, FIELD_INTEGER, "LightBrightness" ),		// asw	
	DEFINE_SOUNDPATCH( m_pLoopSound ),
	DEFINE_KEYFIELD( m_LoopSoundName, FIELD_SOUNDNAME, "LoopSound" ),		// asw
	DEFINE_KEYFIELD( m_StartSoundName, FIELD_SOUNDNAME, "IgniteSound" ),		// asw
	DEFINE_KEYFIELD( m_LightColor, FIELD_COLOR32, "LightColor" ), // asw
#endif
	DEFINE_FIELD( m_hOwner, FIELD_EHANDLE ),
	DEFINE_KEYFIELD( m_nFireType,	FIELD_INTEGER, "firetype" ),

	DEFINE_FIELD( m_flFuel, FIELD_FLOAT ),
	DEFINE_FIELD( m_flDamageTime, FIELD_TIME ),
	DEFINE_FIELD( m_lastDamage, FIELD_TIME ),
	DEFINE_KEYFIELD( m_flFireSize,	FIELD_FLOAT, "firesize" ),

	DEFINE_KEYFIELD( m_flHeatLevel,	FIELD_FLOAT,	"ignitionpoint" ),
 	DEFINE_FIELD( m_flHeatAbsorb, FIELD_FLOAT ),
 	DEFINE_KEYFIELD( m_flDamageScale,FIELD_FLOAT,	"damagescale" ),

	DEFINE_FIELD( m_flMaxHeat, FIELD_FLOAT ),
	//DEFINE_FIELD( m_flLastHeatLevel,	FIELD_FLOAT  ),

	DEFINE_KEYFIELD( m_flAttackTime, FIELD_FLOAT, "fireattack" ),
	DEFINE_FIELD( m_bEnabled, FIELD_BOOLEAN ),
	DEFINE_KEYFIELD( m_bStartDisabled, FIELD_BOOLEAN, "StartDisabled" ),
	DEFINE_FIELD( m_bDidActivate, FIELD_BOOLEAN ),

	DEFINE_FUNCTION( BurnThink ),
	DEFINE_FUNCTION( GoOutThink ),

#ifdef INFESTED_DLL
	DEFINE_FUNCTION( FastBurnThink ),
	DEFINE_ENTITYFUNC( ASWFireTouch ),	// asw
#endif

	DEFINE_INPUTFUNC( FIELD_VOID, "StartFire", InputStartFire ),
	DEFINE_INPUTFUNC( FIELD_FLOAT, "Extinguish", InputExtinguish ),
	DEFINE_INPUTFUNC( FIELD_FLOAT, "ExtinguishTemporary", InputExtinguishTemporary ),

	DEFINE_INPUTFUNC( FIELD_VOID, "Enable", InputEnable ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Disable", InputDisable ),
	
	DEFINE_OUTPUT( m_OnIgnited, "OnIgnited" ),
	DEFINE_OUTPUT( m_OnExtinguished, "OnExtinguished" ),

END_DATADESC()

LINK_ENTITY_TO_CLASS( env_fire, CFire );

//==================================================
// CFire
//==================================================

CFire::CFire( void )
{
	m_flFuel = 0.0f;
	m_flAttackTime = 0.0f;
	m_flDamageTime = 0.0f;
	m_lastDamage = 0;
	m_nFireType = FIRE_NATURAL;

	//Spreading
	m_flHeatAbsorb = 8.0f;
	m_flHeatLevel = 0;

	m_LoopSoundName = MAKE_STRING( "d1_town.LargeFireLoop" );
	m_StartSoundName = MAKE_STRING( "ASW_Flare.IgniteFlare" );

	// Must be in the constructor!
	AddEFlags( EFL_USE_PARTITION_WHEN_NOT_SOLID );

#ifdef INFESTED_DLL
	m_LightColor.r = 115;
	m_LightColor.g = 105;
	m_LightColor.b = 90;
	m_iLightBrightness = 0;
	m_fLightRadiusScale = 1.0f;
	m_bNoFuelingOnceLit = false;
	m_bFirefighterCounted = false;
#endif
}

//-----------------------------------------------------------------------------
// UpdateOnRemove
//-----------------------------------------------------------------------------
void CFire::UpdateOnRemove( void )
{
	//Stop any looping sounds that might be playing
	StopSound( "Fire.Plasma" );
#ifdef INFESTED_DLL
	// asw
	if ( m_pLoopSound )
	{
		CSoundEnvelopeController::GetController().SoundDestroy( m_pLoopSound );
		m_pLoopSound = NULL;
	}
#endif

	DestroyEffect();

	// Chain at end to mimic destructor unwind order
	BaseClass::UpdateOnRemove();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFire::Precache( void )
{
#ifdef INFESTED_DLL
	if ( m_nFireType == FIRE_WALL_MINE )
	{
		PrecacheParticleSystem( "mine_fire" );
	}
#endif
	if ( m_nFireType == FIRE_NATURAL )
	{
		UTIL_PrecacheOther("_firesmoke");
#ifdef INFESTED_DLL
		PrecacheParticleSystem( "ground_fire" );
#else
		if ( m_spawnflags & SF_FIRE_SMOKELESS )
		{
			PrecacheParticleSystem( "env_fire_tiny" );
			PrecacheParticleSystem( "env_fire_small" );
			PrecacheParticleSystem( "env_fire_medium" );
			PrecacheParticleSystem( "env_fire_large" );
		}
		else
		{
			PrecacheParticleSystem( "env_fire_tiny_smoke" );
			PrecacheParticleSystem( "env_fire_small_smoke" );
			PrecacheParticleSystem( "env_fire_medium_smoke" );
			PrecacheParticleSystem( "env_fire_large_smoke" );
		}
#endif
	}

	if ( m_nFireType == FIRE_PLASMA )
	{
		UTIL_PrecacheOther("_plasma");
	}

	PrecacheScriptSound( "Fire.Plasma" );
#ifdef INFESTED_DLL
	PrecacheScriptSound( "ASWGrenade.FireLoop" );
	PrecacheScriptSound( STRING(m_LoopSoundName) );
	PrecacheScriptSound( STRING(m_StartSoundName) );
#endif
}

//------------------------------------------------------------------------------
// Purpose : Input handler for starting the fire.
//------------------------------------------------------------------------------
void CFire::InputStartFire( inputdata_t &inputdata )
{
	if ( !m_bEnabled )
		return;

	StartFire();
}

void CFire::InputEnable( inputdata_t &inputdata )
{
	m_bEnabled = true;
}

void CFire::InputDisable( inputdata_t &inputdata )
{
	Disable();
}

void CFire::Disable() 
{
	m_bEnabled = false;
	if ( IsBurning() )
	{
		GoOut();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CFire::InputExtinguish( inputdata_t &inputdata )
{
	m_spawnflags &= ~SF_FIRE_INFINITE;
	GoOutInSeconds( inputdata.value.Float() );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CFire::InputExtinguishTemporary( inputdata_t &inputdata )
{
	GoOutInSeconds( inputdata.value.Float() );
}

//-----------------------------------------------------------------------------
// Purpose: Starts burning.
//-----------------------------------------------------------------------------
void CFire::StartFire( void )
{
	if ( m_hEffect != NULL )
		return;

#ifdef INFESTED_DLL
	//if ( m_hEmitter != NULL )	// asw
	//	return;
#endif

	// Trace down and start a fire there. Nothing fancy yet.
	Vector vFirePos;
	trace_t tr;
	if ( m_spawnflags & SF_FIRE_DONT_DROP )
	{
		vFirePos = GetAbsOrigin();
	}
	else
	{
		UTIL_TraceLine( GetAbsOrigin(), GetAbsOrigin() + Vector( 0.0f, 0.0f, -1024.0f ), MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );
		vFirePos = tr.endpos;
	}

	int spawnflags = m_spawnflags;
	m_spawnflags |= SF_FIRE_START_ON;
	Init( vFirePos, m_flFireSize, m_flAttackTime, m_flFuel, m_spawnflags, (fireType_e) m_nFireType.Get() );
	Start();
	m_spawnflags = spawnflags;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFire::Spawn( void )
{
	BaseClass::Spawn();

	Precache();

	m_takedamage = DAMAGE_NO;

	SetSolid( SOLID_NONE );
	AddEffects( EF_NODRAW );
	SetToOutSize();

	// set up the ignition point
	m_flHeatAbsorb = m_flHeatLevel * 0.05;
	m_flHeatLevel = 0;
	Init( GetAbsOrigin(), m_flFireSize, m_flAttackTime, m_flFuel, m_spawnflags, m_nFireType );
	
	if( m_bStartDisabled )
	{
		Disable();
	}
	else
	{
		m_bEnabled = true;
	}
}

int CFire::UpdateTransmitState()
{
	// Don't want to be FL_EDICT_DONTSEND because our fire entity may make us transmit.
	return SetTransmitState( FL_EDICT_ALWAYS );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFire::Activate( void )
{
	BaseClass::Activate();
	
	//See if we should start active
	if ( !m_bDidActivate && ( m_spawnflags & SF_FIRE_START_ON ) )
	{
		m_flHeatLevel = m_flMaxHeat;

		StartFire();
	}

	m_bDidActivate = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFire::SpawnEffect( fireType_e type, float scale )
{
	CBaseFire *pEffect = NULL;

	switch ( type )
	{
	default:
	case FIRE_NATURAL:
		{
#ifndef INFESTED_DLL
			CFireSmoke	*fireSmoke = (CFireSmoke *) CreateEntityByName( "_firesmoke" );
			fireSmoke->EnableSmoke( ( m_spawnflags & SF_FIRE_SMOKELESS )==false );
			fireSmoke->EnableGlow( ( m_spawnflags & SF_FIRE_NO_GLOW )==false );
			fireSmoke->EnableVisibleFromAbove( ( m_spawnflags & SF_FIRE_VISIBLE_FROM_ABOVE )!=false );
			
			pEffect			= fireSmoke;
			m_nFireType		= FIRE_NATURAL;
			m_takedamage	= DAMAGE_YES;
#else
			if ( ( m_spawnflags & SF_FIRE_NO_GLOW )==false && asw_fire_glow.GetBool())
			{
				CASW_Dynamic_Light* pFireLight = (CASW_Dynamic_Light*) CreateEntityByName("asw_dynamic_light");
				if (pFireLight)
				{
					UTIL_SetOrigin( pFireLight, GetAbsOrigin() );
					pFireLight->Spawn();
					pFireLight->SetParent( this );
					// todo: set up colour etc?
					float strength = m_flHeatLevel / FIRE_MAX_HEAT_LEVEL;	
					float f = strength;
					if (m_flHeatLevel < 64)
					{
						f += (64 - m_flHeatLevel) * 0.005f;
					}
					pFireLight->SetLightRadius(f*asw_fire_glow_radius.GetFloat()*m_fLightRadiusScale);
					pFireLight->SetExponent(m_iLightBrightness);
					//color32 tmp;
					//UTIL_StringToColor32( &tmp, STRING(m_LightColor) );
					pFireLight->SetRenderColor( m_LightColor.r, m_LightColor.g, m_LightColor.b );
					//Msg("Fire: Spawned dynamic light with rad:%f exp:%d r:%d g:%d b:%d\n", f*asw_fire_glow_radius.GetFloat()*m_fLightRadiusScale,
						//m_iLightBrightness, m_LightColor.r, m_LightColor.g, m_LightColor.b);
					m_hFireLight = pFireLight;
				}
			}

			//m_flFireSize = 1.5f;

			if ( (m_spawnflags & SF_FIRE_NO_SOUND) == false )
			{
				if ( m_LoopSoundName.ToCStr() && m_LoopSoundName.ToCStr()[ 0 ] != '\0' )
				{
					CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();
					if ( !m_pLoopSound )
					{
						CPASAttenuationFilter filter( this );
						m_pLoopSound = controller.SoundCreate( filter, entindex(), STRING( m_LoopSoundName ) );
						controller.Play( m_pLoopSound, 0.1, 100 );
						controller.SoundChangeVolume( m_pLoopSound, 1.0f, 0.25f );
					}
				}
			}
			if ( (m_spawnflags & SF_FIRE_NO_IGNITE_SOUND) == false )
			{
				if ( m_StartSoundName.ToCStr() && m_StartSoundName.ToCStr()[ 0 ] != '\0' )
				{
					EmitSound_t ep;
					ep.m_nChannel = CHAN_AUTO;
					ep.m_pSoundName = STRING( m_StartSoundName );
					ep.m_flVolume = 1;
					ep.m_SoundLevel = SNDLVL_NORM;
					CPASAttenuationFilter filter( this );
					EmitSound( filter, entindex(), ep );
				}
			}
#endif // INFESTED_DLL
		}
		break;

	case FIRE_WALL_MINE:
		{
			if ( asw_fire_glow.GetBool() )
			{
				CASW_Dynamic_Light* pFireLight = (CASW_Dynamic_Light*) CreateEntityByName("asw_dynamic_light");
				if (pFireLight)
				{
					UTIL_SetOrigin( pFireLight, GetAbsOrigin() );
					pFireLight->Spawn();
					pFireLight->SetParent( this );
					// todo: set up colour etc?
					float strength = m_flHeatLevel / FIRE_MAX_HEAT_LEVEL;	
					float f = strength;
					if (m_flHeatLevel < 64)
					{
						f += (64 - m_flHeatLevel) * 0.005f;
					}
					pFireLight->SetLightRadius(f*asw_fire_glow_radius.GetFloat()*m_fLightRadiusScale);
					pFireLight->SetExponent(m_iLightBrightness);
					//color32 tmp;
					//UTIL_StringToColor32( &tmp, STRING(m_LightColor) );
					pFireLight->SetRenderColor( m_LightColor.r, m_LightColor.g, m_LightColor.b );
					//Msg("Fire: Spawned dynamic light with rad:%f exp:%d r:%d g:%d b:%d\n", f*asw_fire_glow_radius.GetFloat()*m_fLightRadiusScale,
					//m_iLightBrightness, m_LightColor.r, m_LightColor.g, m_LightColor.b);
					m_hFireLight = pFireLight;
				}
			}

			//m_flFireSize = 1.5f;

			if ( (m_spawnflags & SF_FIRE_NO_SOUND) == false )
			{
				if ( m_LoopSoundName.ToCStr() && m_LoopSoundName.ToCStr()[ 0 ] != '\0' )
				{
					CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();
					if ( !m_pLoopSound )
					{
						CPASAttenuationFilter filter( this );
						m_pLoopSound = controller.SoundCreate( filter, entindex(), "ASWGrenade.FireLoop" );
						controller.Play( m_pLoopSound, 0.1, 100 );
						controller.SoundChangeVolume( m_pLoopSound, 1.0f, 0.25f );
					}
				}
			}
			if ( (m_spawnflags & SF_FIRE_NO_IGNITE_SOUND) == false )
			{
				if ( m_StartSoundName.ToCStr() && m_StartSoundName.ToCStr()[ 0 ] != '\0' )
				{
					EmitSound_t ep;
					ep.m_nChannel = CHAN_AUTO;
					ep.m_pSoundName = STRING( m_StartSoundName );
					ep.m_flVolume = 1;
					ep.m_SoundLevel = SNDLVL_NORM;
					CPASAttenuationFilter filter( this );
					EmitSound( filter, entindex(), ep );
				}
			}
		}
		break;

	case FIRE_PLASMA:
		{
			CPlasma	*plasma = (CPlasma *) CreateEntityByName( "_plasma" );
			plasma->EnableSmoke( true );
		
			pEffect			= plasma;
			m_nFireType		= FIRE_PLASMA;
			m_takedamage	= DAMAGE_YES;

			// Start burn sound
			EmitSound( "Fire.Plasma" );
		}
		break;
	}

#ifdef INFESTED_DLL
	if (pEffect)	// asw
	{
#endif
	UTIL_SetOrigin( pEffect, GetAbsOrigin() );
	pEffect->Spawn();
	pEffect->SetParent( this );
	pEffect->Scale( m_flFireSize, m_flFireSize, 0 );
	//Start it going
	pEffect->Enable( ( m_spawnflags & SF_FIRE_START_ON ) );
#ifdef INFESTED_DLL
	}
#endif
	m_hEffect = pEffect;
}

//-----------------------------------------------------------------------------
// Purpose: Spawn and initialize the fire
// Input  : &position - where the fire resides
//			lifetime - 
//-----------------------------------------------------------------------------
void CFire::Init( const Vector &position, float scale, float attackTime, float fuel, int flags, int fireType )
{
	m_flAttackTime = attackTime;
	
	m_spawnflags = flags;
	m_nFireType = fireType;

	if ( flags & SF_FIRE_INFINITE )
	{
		fuel = 0;
	}
	m_flFuel = fuel;
	if ( m_flFuel )
	{
		m_spawnflags |= SF_FIRE_DIE_PERMANENT;
	}

	Vector localOrigin = position;
	if ( GetMoveParent() )
	{
		EntityMatrix parentMatrix;
		parentMatrix.InitFromEntity( GetMoveParent() );
		localOrigin = parentMatrix.WorldToLocal( position );
	}
	UTIL_SetOrigin( this, localOrigin );

	SetSolid( SOLID_NONE );
	m_flFireSize = scale;
	m_flMaxHeat = FIRE_MAX_HEAT_LEVEL * FIRE_SCALE_FROM_SIZE(scale);
	//See if we should start on
	if ( m_spawnflags & SF_FIRE_START_FULL )
	{
		m_flHeatLevel = m_flMaxHeat;
	}
	m_flLastHeatLevel = 0;

#ifdef INFESTED_DLL
	m_bNoFuelingOnceLit = (flags & SF_FIRE_NO_FUELLING_ONCE_LIT) != 0;
	if ( (flags & SF_FIRE_DONT_DROP ) == 0 )
	{
		trace_t	tr;
		Vector	startpos = GetAbsOrigin();
		Vector	endpos = GetAbsOrigin();

		startpos[2] += 1;
		endpos[2] -= FIRE_MAX_GROUND_OFFSET;

		UTIL_TraceLine( startpos, endpos, MASK_SOLID, NULL, COLLISION_GROUP_NONE, &tr );
		if ( tr.fraction < 1.0f && tr.m_pEnt && !tr.m_pEnt->IsWorld() && !tr.m_pEnt->IsNPC() )
		{
			SetParent( tr.m_pEnt );
		}
	}

	if ( fireType == FIRE_WALL_MINE )
	{
		m_LoopSoundName = MAKE_STRING( "ASWGrenade.FireLoop" );
	}

	SetSolid( SOLID_BBOX );
	float boxWidth = (m_flFireSize * (FIRE_WIDTH/FIRE_HEIGHT))*0.5f*0.8f;	// asw scale the box a bit
	UTIL_SetSize(this, Vector(-boxWidth,-boxWidth,0),Vector(boxWidth,boxWidth,2.0f * MAX(m_flFireSize, ASW_MIN_FIRE_HEIGHT)));

	// asw make it accept triggers on collision
	SetCollisionGroup( ASW_COLLISION_GROUP_PASSABLE );
	CollisionProp()->UseTriggerBounds( true, 24 );
	AddSolidFlags(FSOLID_TRIGGER);
	SetTouch( &CFire::ASWFireTouch );
#endif
}

void CFire::Start()
{
#ifdef INFESTED_DLL
	// asw so we can collide
	SetSolid( SOLID_BBOX );
#endif
	float boxWidth = (m_flFireSize * (FIRE_WIDTH/FIRE_HEIGHT))*0.5f;
	UTIL_SetSize(this, Vector(-boxWidth,-boxWidth,0),Vector(boxWidth,boxWidth,m_flFireSize));
#ifdef INFESTED_DLL
	// asw make it accept triggers on collision
	SetCollisionGroup( ASW_COLLISION_GROUP_PASSABLE );
	CollisionProp()->UseTriggerBounds( true, 24 );
	AddSolidFlags(FSOLID_TRIGGER);
	SetTouch( &CFire::ASWFireTouch );
#endif

	//Spawn the client-side effect
	SpawnEffect( (fireType_e)m_nFireType.Get(), FIRE_SCALE_FROM_SIZE(m_flFireSize) );
	m_OnIgnited.FireOutput( this, this );
#ifdef INFESTED_DLL	// asw - burn quicker?
	if (HasSpawnFlags(SF_FIRE_FAST_BURN_THINK))
		SetThink( &CFire::FastBurnThink );
	else
#endif
	SetThink( &CFire::BurnThink );
	m_flDamageTime = 0;
	// think right now
	BurnThink();
}

//-----------------------------------------------------------------------------
// Purpose: Determines whether or not the fire is still active
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CFire::IsBurning( void ) const
{
	if ( m_flHeatLevel > 0 )
		return true;

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Get the damage box of the fire
//-----------------------------------------------------------------------------
bool CFire::GetFireDimensions( Vector *pFireMins, Vector *pFireMaxs )
{
	if ( m_flHeatLevel <= 0 )
	{
		pFireMins->Init();
		pFireMaxs->Init();
		return false;
	}

	float scale = m_flHeatLevel / m_flMaxHeat;
	float damageRadius = scale * m_flFireSize * FIRE_WIDTH / FIRE_HEIGHT * 0.5;	

	damageRadius *= FIRE_SPREAD_DAMAGE_MULTIPLIER; //FIXME: Trying slightly larger radius for burning

	if ( damageRadius < 16 )
	{
		damageRadius = 16;
	}

	pFireMins->Init(-damageRadius,-damageRadius,0);
	pFireMaxs->Init(damageRadius,damageRadius,m_flFireSize*scale);

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Update the fire and its children
//-----------------------------------------------------------------------------
void CFire::Update( float simTime )
{
	VPROF_FIRE( "CFire::Update" );

	if ( m_flFuel != 0 )
	{
		m_flFuel -= simTime;
		if ( m_flFuel <= 0 )
		{
			GoOutInSeconds( 1 );
			return;
		}
	}

	float strength = m_flHeatLevel / FIRE_MAX_HEAT_LEVEL;
	if ( m_flHeatLevel != m_flLastHeatLevel )
	{
		m_flLastHeatLevel = m_flHeatLevel;
		// Make the effect the appropriate size given the heat level
#ifndef INFESTED_DLL
		m_hEffect->Scale( strength, 0.5f );		
#else
		if (m_hEffect.IsValid())
			m_hEffect->Scale( strength, 0.5f );
		
#endif // INFESTED_DLL
	}
	// add heat to myself (grow)
	float addedHeat = (m_flAttackTime > 0) ? m_flMaxHeat / m_flAttackTime : m_flMaxHeat;
	addedHeat *= simTime * fire_growthrate.GetFloat();
	AddHeat( addedHeat, true );

	// add heat to nearby fires
	float outputHeat = strength * m_flHeatLevel;

	Vector fireMins;
	Vector fireMaxs;
	Vector fireEntityDamageMins;
	Vector fireEntityDamageMaxs;

	GetFireDimensions( &fireMins, &fireMaxs );

	if ( FIRE_SPREAD_DAMAGE_MULTIPLIER != 1.0 ) // if set to 1.0, optimizer will remove this code
	{
		fireEntityDamageMins = fireMins / FIRE_SPREAD_DAMAGE_MULTIPLIER;
		fireEntityDamageMaxs = fireMaxs / FIRE_SPREAD_DAMAGE_MULTIPLIER;
	}

	//NDebugOverlay::Box( GetAbsOrigin(), fireMins, fireMaxs, 255, 255, 255, 0, fire_dmginterval.GetFloat() );
	fireMins += GetAbsOrigin();
	fireMaxs += GetAbsOrigin();

	if ( FIRE_SPREAD_DAMAGE_MULTIPLIER != 1.0 )
	{
		fireEntityDamageMins += GetAbsOrigin();
		fireEntityDamageMaxs += GetAbsOrigin();
	}

	CBaseEntity *pNearby[256];
	CFire *pFires[16];
	int nearbyCount = UTIL_EntitiesInBox( pNearby, ARRAYSIZE(pNearby), fireMins, fireMaxs, 0 );
	int fireCount = 0;
	int i;

	// is it time to do damage?
	bool damage = false;
	int outputDamage = 0;
	if ( m_flDamageTime <= gpGlobals->curtime )
	{
		m_flDamageTime = gpGlobals->curtime + fire_dmginterval.GetFloat();
		outputDamage = (fire_dmgbase.GetFloat() + outputHeat * fire_dmgscale.GetFloat() * m_flDamageScale) * fire_dmginterval.GetFloat();
		if ( outputDamage )
		{
			damage = true;
		}
	}
	int damageFlags = (m_nFireType == FIRE_NATURAL || m_nFireType == FIRE_WALL_MINE) ? DMG_BURN : DMG_PLASMA;
	for ( i = 0; i < nearbyCount; i++ )
	{
		CBaseEntity *pOther = pNearby[i];

		if ( pOther == this )
		{
			continue;
		}
		else if ( FClassnameIs( pOther, "env_fire" ) )
		{
#ifndef INFESTED_DLL			// asw: skip fires, we do them in a seperate iteration so we can cast a wider net
			if ( fireCount < ARRAYSIZE(pFires) )
			{
				pFires[fireCount] = (CFire *)pOther;
				fireCount++;
			}
#endif
			continue;
		}
		else if ( pOther->m_takedamage == DAMAGE_NO )
		{
			pNearby[i] = NULL;
		}
		else if ( damage )
		{
			bool bDoDamage;

			if ( FIRE_SPREAD_DAMAGE_MULTIPLIER != 1.0 && !pOther->IsPlayer() ) // if set to 1.0, optimizer will remove this code
			{
				Vector otherMins, otherMaxs;
				pOther->CollisionProp()->WorldSpaceAABB( &otherMins, &otherMaxs );
				bDoDamage = IsBoxIntersectingBox( otherMins, otherMaxs, 
												  fireEntityDamageMins, fireEntityDamageMaxs );

			}
			else
			{
				bDoDamage = true;
			}

			if ( bDoDamage )
			{
				// Make sure can actually see entity (don't damage through walls)
				trace_t tr;
				UTIL_TraceLine( this->WorldSpaceCenter(), pOther->WorldSpaceCenter(), MASK_FIRE_SOLID, pOther, COLLISION_GROUP_NONE, &tr );

				if (tr.fraction == 1.0 && !tr.startsolid)
				{
#ifdef INFESTED_DLL
					// don't hurt things like this if we're a mine fire - they have to be ignited by the touch function below so we can collect igniting stats
					if ( m_nFireType != FIRE_WALL_MINE )
					{
						CTakeDamageInfo info( m_hOwner.Get() ? m_hOwner.Get() : this, this, outputDamage, damageFlags );
						info.SetWeapon( m_hCreatorWeapon );
						pOther->TakeDamage( info );
					}
#else
					CTakeDamageInfo info( this, this, outputDamage, damageFlags );
					info.SetWeapon( m_hCreatorWeapon );
					pOther->TakeDamage( info );
#endif
				}
			}
		}
	}
#ifdef INFESTED_DLL
	// asw: do a seperate pass just picking up env_fires, in a wider radius
	GetFireDimensions( &fireMins, &fireMaxs );
	fireMins *= asw_fire_spread_scale.GetFloat();
	fireMaxs *= asw_fire_spread_scale.GetFloat();
	fireMins += GetAbsOrigin();
	fireMaxs += GetAbsOrigin();

	nearbyCount = UTIL_EntitiesInBox( pNearby, ARRAYSIZE(pNearby), fireMins, fireMaxs, 0 );
	for ( i = 0; i < nearbyCount; i++ )
	{
		CBaseEntity *pOther = pNearby[i];

		if ( pOther == this )
		{
			continue;
		}
		else if ( FClassnameIs( pOther, "env_fire" ) )
		{
			if ( fireCount < ARRAYSIZE(pFires) )
			{				
				pFires[fireCount] = (CFire *)pOther;
				fireCount++;
			}
			continue;
		}
	}
#endif
	outputHeat *= fire_heatscale.GetFloat() * simTime;

	if ( fireCount > 0 )
	{
		outputHeat /= fireCount;
		for ( i = 0; i < fireCount; i++ )
		{
			pFires[i]->AddHeat( outputHeat, false );
		}
	}
}

// Destroy any effect I have
void CFire::DestroyEffect()
{
#ifdef INFESTED_DLL
	// asw
	if ( m_pLoopSound )
	{
		CSoundEnvelopeController::GetController().SoundDestroy( m_pLoopSound );
		m_pLoopSound = NULL;
	}
#endif
	CBaseFire *pEffect = m_hEffect;
	if ( pEffect != NULL )
	{
		//disable the graphics and remove the entity
		pEffect->Enable( false );
		UTIL_Remove( pEffect );
	}
#ifdef INFESTED_DLL
	CASW_Dynamic_Light *pLight = m_hFireLight;
	if (pLight)
	{
		UTIL_Remove(pLight);
	}
#endif
}
//-----------------------------------------------------------------------------
// Purpose: Think
//-----------------------------------------------------------------------------
void CFire::BurnThink( void )
{
	SetNextThink( gpGlobals->curtime + FIRE_THINK_INTERVAL );

	Update( FIRE_THINK_INTERVAL );
}

void CFire::GoOutThink()
{
	GoOut();
}

void CFire::GoOutInSeconds( float seconds )
{
	Scale( 0.0f, seconds );

	if ( m_pLoopSound )
	{
		CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();
		controller.SoundChangeVolume( m_pLoopSound, 0.0f, seconds );
	}
	
	SetThink( &CFire::GoOutThink );
	SetNextThink( gpGlobals->curtime + seconds );
}

//------------------------------------------------------------------------------
// Purpose : Blasts of significant size blow out fires that take damage
// Input   :
// Output  :
//------------------------------------------------------------------------------
int CFire::OnTakeDamage( const CTakeDamageInfo &info )
{
	return 0;
}

void CFire::AddHeat( float heat, bool selfHeat )
{
#ifdef INFESTED_DLL		// asw - don't grow fires flagged to not fuel once lit
	if (m_bEnabled && IsBurning() && m_bNoFuelingOnceLit)
		return;
#endif
	if ( m_bEnabled )
	{
		if ( !selfHeat )
		{
			if ( IsBurning() )
			{
				// scale back the incoming heat from surrounding fires
				// if I've already ignited
				heat *= fire_incomingheatscale.GetFloat();
			}
		}
		m_lastDamage = gpGlobals->curtime + 0.5;
		bool start = m_flHeatLevel <= 0 ? true : false;
		if ( m_flHeatAbsorb > 0 )
		{
			float absorbDamage = heat * fire_absorbrate.GetFloat();
			if ( absorbDamage > m_flHeatAbsorb )
			{
				heat -= m_flHeatAbsorb / fire_absorbrate.GetFloat();
				m_flHeatAbsorb = 0;
			}
			else
			{
				m_flHeatAbsorb -= absorbDamage;
				heat = 0;
			}
		}

		m_flHeatLevel += heat;
		if ( start && m_flHeatLevel > 0 && m_hEffect == NULL )
		{
			StartFire();
		}
		if ( m_flHeatLevel > m_flMaxHeat )
			m_flHeatLevel = m_flMaxHeat;
#ifdef INFESTED_DLL
		// asw scale the collision box as we get bigger/smaller
		if (m_flMaxHeat > 0 && m_flHeatLevel > 0)
		{
			float boxWidth = (m_flFireSize * (FIRE_WIDTH/FIRE_HEIGHT))*0.5f;
			float f = (m_flHeatLevel / m_flMaxHeat) * 0.8f;
			boxWidth *= f;
			UTIL_SetSize(this, Vector(-boxWidth,-boxWidth,0),Vector(boxWidth,boxWidth,2.0f * MAX(m_flFireSize * f, ASW_MIN_FIRE_HEIGHT)));
		}
#endif
	}
}

	
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : end - 
//			time - 
//-----------------------------------------------------------------------------
void CFire::Scale( float end, float time )
{
	CBaseFire *pEffect = m_hEffect;
	if ( pEffect )
	{
		pEffect->Scale( end, time );
	}
#ifdef INFESTED_DLL

	float f = end;
	if (m_flHeatLevel < 64)
	{
		f += (64 - m_flHeatLevel) * 0.005f;
	}
	m_flFireSize = end * 1.5f;

	CASW_Dynamic_Light *pLight = m_hFireLight;
	if (pLight)
	{
		// todo: scale light radius, etc
		pLight->SetLightRadius(end * asw_fire_glow_radius.GetFloat()*m_fLightRadiusScale);
	}
#endif
}

#ifdef INFESTED_DLL	// asw - put out a specific fire
bool FireSystem_ExtinguishFire(CBaseEntity *pEnt)
{
	CFire *pFire = dynamic_cast<CFire*>(pEnt);
	if (!pFire)
		return false;

	pFire->Extinguish(10000);
	return true;
}
#endif

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : time - 
//-----------------------------------------------------------------------------
void CFire::Extinguish( float heat )
{
	if ( !m_bEnabled )
		return;

	m_lastDamage = gpGlobals->curtime + 0.5;
	bool out = m_flHeatLevel > 0 ? true : false;

	m_flHeatLevel -= heat;
	m_flHeatAbsorb += fire_extabsorb.GetFloat() * heat;
	if ( m_flHeatAbsorb > fire_maxabsorb.GetFloat() )
	{
		m_flHeatAbsorb = fire_maxabsorb.GetFloat();
	}

	// drift toward the average attack time after being sprayed
	// some fires are heavily scripted so their attack looks weird 
	// once interacted with.  Basically, this blends out the scripting 
	// as the fire is sprayed with the extinguisher.
	float averageAttackTime = m_flMaxHeat * (FIRE_NORMAL_ATTACK_TIME/FIRE_MAX_HEAT_LEVEL);
	m_flAttackTime = Approach( averageAttackTime, m_flAttackTime, 2 * gpGlobals->frametime );

	if ( m_flHeatLevel <= 0 )
	{
		m_flHeatLevel = 0;
		if ( out )
		{
			GoOut();
		}
	}
#ifdef INFESTED_DLL
	// asw
	else
	{
		// asw scale the collision box as we get bigger/smaller
		if (m_flMaxHeat > 0)
		{
			float boxWidth = (m_flFireSize * (FIRE_WIDTH/FIRE_HEIGHT))*0.5f;
			float f = (m_flHeatLevel / m_flMaxHeat) * 0.8f;
			boxWidth *= f;
			UTIL_SetSize(this, Vector(-boxWidth,-boxWidth,0),Vector(boxWidth,boxWidth,2.0f * MAX(m_flFireSize * f, ASW_MIN_FIRE_HEIGHT)));
		}
	}
#endif
}

bool CFire::GoOut()
{
	//Signal death
	m_OnExtinguished.FireOutput( this, this );

	DestroyEffect();
	m_flHeatLevel -= 20;
	if ( m_flHeatLevel > 0 )
		m_flHeatLevel = 0;

	m_flLastHeatLevel = m_flHeatLevel; 
	SetThink(NULL);
	SetNextThink( TICK_NEVER_THINK );
	if ( m_spawnflags & SF_FIRE_DIE_PERMANENT )
	{
		UTIL_Remove( this );
		return true;
	}
	SetToOutSize();
	
	return false;
}

//==================================================
// CEnvFireSource is a source of heat that the player 
// cannot put out
//==================================================

#define FIRESOURCE_THINK_TIME		0.25		// seconds to 

#define SF_FIRESOURCE_START_ON		0x0001

class CEnvFireSource : public CBaseEntity
{
	DECLARE_CLASS( CEnvFireSource, CBaseEntity );
public:
	void Spawn();
	void Think();
	void TurnOn();
	void TurnOff();
	void InputEnable( inputdata_t &inputdata );
	void InputDisable( inputdata_t &inputdata );

	DECLARE_DATADESC();

private:
	bool		m_bEnabled;
	float		m_radius;
	float		m_damage;
};

BEGIN_DATADESC( CEnvFireSource )

	DEFINE_FIELD( m_bEnabled, FIELD_BOOLEAN ),
	DEFINE_KEYFIELD( m_radius,	FIELD_FLOAT, "fireradius" ),
	DEFINE_KEYFIELD( m_damage,FIELD_FLOAT, "firedamage" ),

	DEFINE_INPUTFUNC( FIELD_VOID, "Enable", InputEnable ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Disable", InputDisable ),


END_DATADESC()

LINK_ENTITY_TO_CLASS( env_firesource, CEnvFireSource );

void CEnvFireSource::Spawn()
{
	if ( m_spawnflags & SF_FIRESOURCE_START_ON )
	{
		TurnOn();
	}
	else
	{
		TurnOff();
	}
}

void CEnvFireSource::Think()
{
	if ( !m_bEnabled )
		return;
	SetNextThink( gpGlobals->curtime + FIRESOURCE_THINK_TIME );

	CFire *pFires[128];
	int fireCount = FireSystem_GetFiresInSphere( pFires, ARRAYSIZE(pFires), false, GetAbsOrigin(), m_radius );

	for ( int i = 0; i < fireCount; i++ )
	{
		pFires[i]->AddHeat( m_damage * FIRESOURCE_THINK_TIME );
	}
}

void CEnvFireSource::TurnOn()
{
	if ( m_bEnabled )
		return;

	m_bEnabled = true;
	SetNextThink( gpGlobals->curtime );
}

void CEnvFireSource::TurnOff()
{
	if ( !m_bEnabled )
		return;

	m_bEnabled = false;
	SetNextThink( TICK_NEVER_THINK );
}
void CEnvFireSource::InputEnable( inputdata_t &inputdata )
{
	TurnOn();
}
void CEnvFireSource::InputDisable( inputdata_t &inputdata )
{
	TurnOff();
}

//==================================================
// CEnvFireSensor detects changes in heat
//==================================================
#define SF_FIRESENSOR_START_ON	1

class CEnvFireSensor : public CBaseEntity
{
	DECLARE_CLASS( CEnvFireSensor, CBaseEntity );
public:
	void Spawn();
	void Think();
	void TurnOn();
	void TurnOff();
	void InputEnable( inputdata_t &inputdata );
	void InputDisable( inputdata_t &inputdata );

	DECLARE_DATADESC();

private:
	bool			m_bEnabled;
	bool			m_bHeatAtLevel;
	float			m_radius;
	float			m_targetLevel;
	float			m_targetTime;
	float			m_levelTime;

	COutputEvent	m_OnHeatLevelStart;
	COutputEvent	m_OnHeatLevelEnd;
};

BEGIN_DATADESC( CEnvFireSensor )

	DEFINE_KEYFIELD( m_radius,	FIELD_FLOAT, "fireradius" ),
	DEFINE_KEYFIELD( m_targetLevel, FIELD_FLOAT, "heatlevel" ),
	DEFINE_KEYFIELD( m_targetTime, FIELD_FLOAT, "heattime" ),

	DEFINE_FIELD( m_bEnabled, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bHeatAtLevel, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_levelTime, FIELD_FLOAT ),
	
	DEFINE_INPUTFUNC( FIELD_VOID, "Enable", InputEnable ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Disable", InputDisable ),

	DEFINE_OUTPUT( m_OnHeatLevelStart, "OnHeatLevelStart"),
	DEFINE_OUTPUT( m_OnHeatLevelEnd, "OnHeatLevelEnd"),

END_DATADESC()

LINK_ENTITY_TO_CLASS( env_firesensor, CEnvFireSensor );

void CEnvFireSensor::Spawn()
{
	if ( m_spawnflags & SF_FIRESENSOR_START_ON )
	{
		TurnOn();
	}
	else
	{
		TurnOff();
	}
}

void CEnvFireSensor::Think()
{
	if ( !m_bEnabled )
		return;

	float time = m_targetTime * 0.25;
	if ( time < 0.1 )
	{
		time = 0.1;
	}
	SetNextThink( gpGlobals->curtime + time );

	float heat = 0;
	CFire *pFires[128];
	int fireCount = FireSystem_GetFiresInSphere( pFires, ARRAYSIZE(pFires), true, GetAbsOrigin(), m_radius );
	for ( int i = 0; i < fireCount; i++ )
	{
		heat += pFires[i]->GetHeatLevel();
	}

	if ( heat >= m_targetLevel )
	{
		m_levelTime += time;
		if ( m_levelTime >= m_targetTime )
		{
			if ( !m_bHeatAtLevel )
			{
				m_bHeatAtLevel = true;
				m_OnHeatLevelStart.FireOutput( this, this );
			}
		}
	}
	else
	{
		m_levelTime = 0;
		if ( m_bHeatAtLevel )
		{
			m_bHeatAtLevel = false;
			m_OnHeatLevelEnd.FireOutput( this, this );
		}
	}
}

void CEnvFireSensor::TurnOn()
{
	if ( m_bEnabled )
		return;

	m_bEnabled = true;
	SetNextThink( gpGlobals->curtime );
	m_bHeatAtLevel = false;
	m_levelTime = 0;
}

void CEnvFireSensor::TurnOff()
{
	if ( !m_bEnabled )
		return;

	m_bEnabled = false;
	SetNextThink( TICK_NEVER_THINK );
	if ( m_bHeatAtLevel )
	{
		m_bHeatAtLevel = false;
		m_OnHeatLevelEnd.FireOutput( this, this );
	}

}
void CEnvFireSensor::InputEnable( inputdata_t &inputdata )
{
	TurnOn();
}
void CEnvFireSensor::InputDisable( inputdata_t &inputdata )
{
	TurnOff();
}

#ifdef INFESTED_DLL
void CFire::FastBurnThink( void )
{
	SetNextThink( gpGlobals->curtime + FIRE_THINK_INTERVAL * 0.5f );

	Update( FIRE_THINK_INTERVAL * 0.5f );
}

// ASW
void CFire::ASWFireTouch( CBaseEntity *pOther )
{
	//Msg("fire touched by %s\n", pOther->GetClassname());	
	if (!m_bEnabled || !pOther)
		return;

	//Msg("Fire touched by %s %d\n", pOther->GetClassname(), pOther->entindex());

	if (IsBurning())
	{
		if (m_flFuel > 0 || (m_spawnflags & SF_FIRE_INFINITE))
		{
			CBaseAnimating *pAnim = dynamic_cast<CBaseAnimating*>(pOther);
			if ( !pAnim || ( m_nFireType != FIRE_WALL_MINE && pAnim->IsOnFire() ) )	// already on fire
			{
				//Msg("but it's already on fire\n");
				return;
			}

			CASW_Marine *pMarine = CASW_Marine::AsMarine( pOther );
			if ( pMarine )
			{
				// Burn for 1/3rd the time decided by difficulty
				pMarine->ASW_Ignite( ( m_nFireType == FIRE_WALL_MINE ) ? 0.5f : 1.0f, 0, m_hOwner.Get() ? m_hOwner.Get() : this, m_hCreatorWeapon );
				//Msg("OUCH OUCH BURNING MARINE\n");
			}

			IASW_Spawnable_NPC *pSpawnable = dynamic_cast< IASW_Spawnable_NPC* >(pOther);
			if ( pSpawnable )
			{
				//Msg("  it's a spawnable alien\n");
				//if (m_hOwner.Get())
					//Msg("  our owner is %d\n", m_hOwner->entindex());
				//else
					//Msg("  our owner is null\n");				
				if ( pSpawnable->GetNPC() && !pSpawnable->GetNPC()->IsOnFire() && m_nFireType == FIRE_WALL_MINE )
				{
					CASW_Marine *pFireStarterMarine = dynamic_cast< CASW_Marine* >( m_hOwner.Get() );
					if ( pFireStarterMarine && pFireStarterMarine->GetMarineResource() )
					{
						pFireStarterMarine->GetMarineResource()->m_iMineKills++;
					}
				}
				pSpawnable->ASW_Ignite( 10.0f, 0, m_hOwner.Get() ? m_hOwner.Get() : this, m_hCreatorWeapon );

				if ( m_nFireType == FIRE_WALL_MINE && pSpawnable->GetNPC() )
				{
					int damageFlags = (m_nFireType == FIRE_NATURAL || m_nFireType == FIRE_WALL_MINE) ? DMG_BURN : DMG_PLASMA;
					float flDamage = 7.0f;
					if ( ASWGameRules() )
					{
						ASWGameRules()->ModifyAlienHealthBySkillLevel( flDamage );
					}

					CTakeDamageInfo info( m_hOwner.Get() ? m_hOwner.Get() : this, this, flDamage, damageFlags );
					info.SetWeapon( m_hCreatorWeapon );
					pSpawnable->GetNPC()->TakeDamage( info );
				}
			}

			CASW_Extinguisher_Projectile *pExt = dynamic_cast< CASW_Extinguisher_Projectile* >(pOther);
			if ( pExt )
			{
				Extinguish( 3 );
				if (!m_bFirefighterCounted && m_flHeatLevel <= 0)
				{
					CASW_Marine *pExtMarine = dynamic_cast< CASW_Marine* >(pExt->GetFirer());
					if ( pExtMarine && pExtMarine->GetMarineResource() )
					{
						pExtMarine->GetMarineResource()->m_iFiresPutOut++;
						m_bFirefighterCounted++;
					}
				}					
				pExt->TouchedEnvFire();
			}
		}
	}
	else
	{
		CASW_Flamer_Projectile* pFlame = dynamic_cast<CASW_Flamer_Projectile*>(pOther);
		if (pFlame)
		{
			// ignite!
			StartFire();
		}
	}
}
#endif // INFESTED_DLL


//-----------------------------------------------------------------------------
// Purpose: Draw any debug text overlays
// Output : Current text offset from the top
//-----------------------------------------------------------------------------
int CFire::DrawDebugTextOverlays( void ) 
{
	int text_offset = BaseClass::DrawDebugTextOverlays();

	if (m_debugOverlays & OVERLAY_TEXT_BIT) 
	{
		char tempstr[512];

		// print flame size
		Q_snprintf(tempstr,sizeof(tempstr),"    size: %f", m_flFireSize);
		EntityText(text_offset,tempstr,0);
		text_offset++;
	}
	return text_offset;
}