#include "cbase.h"

#ifdef CLIENT_DLL
#include "c_asw_player.h"
#include "c_asw_marine.h"
#include "c_asw_weapon.h"
#include "c_asw_marine_resource.h"

#define CASW_Marine C_ASW_Marine
#define CASW_Weapon C_ASW_Weapon
#define CASW_Pickup C_ASW_Pickup
#define CASW_Powerup C_ASW_Powerup
#define CASW_Powerup_Bullets C_ASW_Powerup_Bullets
#define CASW_Powerup_Freeze_Bullets C_ASW_Powerup_Freeze_Bullets
#define CASW_Powerup_Fire_Bullets C_ASW_Powerup_Fire_Bullets
#define CASW_Powerup_Electric_Bullets C_ASW_Powerup_Electric_Bullets
#define CASW_Powerup_Chemical_Bullets C_ASW_Powerup_Chemical_Bullets
#define CASW_Powerup_Explosive_Bullets C_ASW_Powerup_Explosive_Bullets
#define CASW_Powerup_Increased_Speed C_ASW_Powerup_Increased_Speed
#else
#include "asw_game_resource.h"
#include "asw_marine.h"
#include "asw_weapon.h"
#include "asw_player.h"
#include "asw_marine_resource.h"
#include "cvisibilitymonitor.h"
#endif

#include "particle_parse.h"
#include "asw_gamerules.h"
#include "asw_shareddefs.h"
#include "asw_powerup_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define ITEM_PICKUP_BOX_BLOAT_POWERUP		38

#define PICKUP_POWERUP_VISIBILITY_RANGE 500.0f

//IMPLEMENT_SERVERCLASS_ST(CASW_Powerup, DT_ASW_Powerup)
//END_SEND_TABLE()

IMPLEMENT_NETWORKCLASS_ALIASED( ASW_Powerup, DT_ASW_Powerup );

BEGIN_NETWORK_TABLE( CASW_Powerup, DT_ASW_Powerup )
#ifndef CLIENT_DLL

#else

#endif
END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( asw_powerup, CASW_Powerup );

PRECACHE_REGISTER( asw_powerup );

ConVar asw_powerup_friction( "asw_powerup_friction", "-0.5f", FCVAR_CHEAT | FCVAR_REPLICATED );
ConVar asw_powerup_gravity( "asw_powerup_gravity", "1.0f", FCVAR_CHEAT | FCVAR_REPLICATED );
ConVar asw_powerup_elasticity( "asw_powerup_elasticity", "0.6f", FCVAR_CHEAT | FCVAR_REPLICATED );


CASW_Powerup::CASW_Powerup()
{
	m_nPowerupType = 0;
	m_flPowerupDuration = -1;
	m_flCanPickupMaxTime = 0;
	m_bSpawnDelayDenyPickup = true;
}

CASW_Powerup::~CASW_Powerup()
{

}

#ifndef CLIENT_DLL
void CASW_Powerup::Spawn( void )
{
	BaseClass::Spawn();

	//m_nSkin = m_nPowerupType;

	SetModel( GetPowerupModelName() );
	
	DispatchParticleEffect( GetIdleParticleName(), GetAbsOrigin(), vec3_angle, PATTACH_ABSORIGIN_FOLLOW, this );

	//EmitSound("ASW_Powerup.Drop");

	// This will make them not collide with the player, but will collide
	// against other items + weapons
	SetCollisionGroup( COLLISION_GROUP_DEBRIS );
	CollisionProp()->UseTriggerBounds( true, ITEM_PICKUP_BOX_BLOAT_POWERUP );
	
	// ignore touch
	//SetTouch(&CASW_Powerup::ItemTouch);

	SetMoveType( MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_BOUNCE );
	// Tumble in air
	QAngle vecAngVelocity( 0, random->RandomFloat( -100, 100 ), 0 );
	SetLocalAngularVelocity( vecAngVelocity );
	
	SetGravity( asw_powerup_gravity.GetFloat() );
	SetFriction( asw_powerup_friction.GetFloat() );
	SetElasticity( asw_powerup_elasticity.GetFloat() );

	VisibilityMonitor_AddEntity( this, PICKUP_POWERUP_VISIBILITY_RANGE, NULL, NULL );
}

void CASW_Powerup::Precache( void )
{
	BaseClass::Precache( );

	PrecacheModel( GetPowerupModelName() );
	//PrecacheScriptSound( "ASW_Powerup.Drop" );
	//PrecacheScriptSound( GetPickupSoundName() );
	PrecacheParticleSystem( GetIdleParticleName() );
	PrecacheParticleSystem( GetPickupParticleName() );
}

void CASW_Powerup::SetIsSpawnFlipping( float flMaxPickupDelay )
{
	m_flCanPickupMaxTime = gpGlobals->curtime + flMaxPickupDelay;
}

/*
void CASW_Powerup::ItemTouch( CBaseEntity *pOther )
{
	return;

	if ( (pOther->IsWorld() || m_flCanPickupMaxTime <= gpGlobals->curtime) && m_bSpawnDelayDenyPickup == true  )
	{
		m_bSpawnDelayDenyPickup = false;	
		return;
	}

	if ( !pOther || pOther->Classify() != CLASS_ASW_MARINE )
		return;
    
	if ( m_bSpawnDelayDenyPickup == true && m_flCanPickupMaxTime > gpGlobals->curtime )
		return;

	CASW_Marine * RESTRICT pMarine = assert_cast<CASW_Marine*>( pOther );

	// incapacitated or nonexistent marines cannot pick up crystals.
	if ( !pMarine )
		return;

	PickupPowerup( pMarine );
	DispatchParticleEffect( GetPickupParticleName(), GetAbsOrigin(), vec3_angle, PATTACH_ABSORIGIN_FOLLOW, this );


	pMarine->CreatePowerupPickupIcon( this, iFXType, iAmount );

	int iFXType = 0;
	// send a message to c_asw_fx to show the particle effect
	CRecipientFilter filter;
	filter.AddAllPlayers();
	UserMessageBegin( filter, "ASWPickupPowerBall" );
	WRITE_SHORT( pOther->entindex() );
	WRITE_SHORT( iFXType );
	WRITE_FLOAT( WorldSpaceCenter().x );
	WRITE_FLOAT( WorldSpaceCenter().y );
	WRITE_FLOAT( WorldSpaceCenter().z );
	MessageEnd();


	// tell it to remove itself
	SetCollisionGroup( COLLISION_GROUP_DEBRIS );
	SetTouch( NULL );
	SetMoveType( MOVETYPE_NONE );

	//UTIL_Remove( this );
	SetThink( &CASW_Powerup::SUB_Remove );
	SetNextThink( gpGlobals->curtime + 0.1f );
}
*/

void CASW_Powerup::ActivateUseIcon( CASW_Marine* pMarine, int nHoldType )
{
	if ( nHoldType == ASW_USE_HOLD_START )
		return;

	PickupPowerup( pMarine );
}

void CASW_Powerup::PickupPowerup( CASW_Marine *pMarine )
{
	if ( !pMarine )
		return;

	//pMarine->EmitSound( GetPickupSoundName() );	

	pMarine->AddPowerup( m_nPowerupType, gpGlobals->curtime + m_flPowerupDuration );
	DispatchParticleEffect( GetPickupParticleName(), GetAbsOrigin(), vec3_angle, PATTACH_ABSORIGIN_FOLLOW, this );

	// tell it to remove itself
	SetCollisionGroup( COLLISION_GROUP_DEBRIS );
	SetTouch( NULL );
	SetMoveType( MOVETYPE_NONE );

	//UTIL_Remove( this );
	SetThink( &CASW_Powerup::SUB_Remove );
	SetNextThink( gpGlobals->curtime + 0.1f );
}
#endif

//-------------
// Bullet-based powerups
// these can only be picked up if the player is currently wielding a bullet-based weapon
//-------------
IMPLEMENT_NETWORKCLASS_ALIASED( ASW_Powerup_Bullets, DT_ASW_Powerup_Bullets );
BEGIN_NETWORK_TABLE( CASW_Powerup_Bullets, DT_ASW_Powerup_Bullets )
END_NETWORK_TABLE()

bool CASW_Powerup_Bullets::AllowedToPickup(CASW_Marine *pMarine)
{ 
	if ( !pMarine )
		return false;

	if ( !ASWGameRules()->MarineCanPickupPowerup( pMarine, this ) )
		return false;

	return true;
	/*
	bool bInRange = ( GetAbsOrigin().DistTo( pMarine->GetAbsOrigin() ) < HEALTSTTATION_USE_AREA_RANGE );

	return bInRange && !pMarine->IsIncap() && !pMarine->IsInfested();
	*/
}

#ifndef CLIENT_DLL
void CASW_Powerup_Bullets::ActivateUseIcon( CASW_Marine* pMarine, int nHoldType )
{
	if ( nHoldType == ASW_USE_HOLD_START )
		return;

	if ( ASWGameRules()->MarineCanPickupPowerup( pMarine, this ) )
		PickupPowerup( pMarine );
}
#endif

//-------------
// Freeze Bullets
//-------------
IMPLEMENT_NETWORKCLASS_ALIASED( ASW_Powerup_Freeze_Bullets, DT_ASW_Powerup_Freeze_Bullets );
BEGIN_NETWORK_TABLE( CASW_Powerup_Freeze_Bullets, DT_ASW_Powerup_Freeze_Bullets )
END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( asw_powerup_freeze_bullets, CASW_Powerup_Freeze_Bullets );

PRECACHE_REGISTER( asw_powerup_freeze_bullets );

CASW_Powerup_Freeze_Bullets::CASW_Powerup_Freeze_Bullets()
{ 
	m_nPowerupType = POWERUP_TYPE_FREEZE_BULLETS;
#ifdef CLIENT_DLL
	Q_snprintf(m_szUseIconText, sizeof(m_szUseIconText), "#asw_take_pow_freeze_bullets");
#endif
}

//-------------
// Fire Bullets
//-------------
IMPLEMENT_NETWORKCLASS_ALIASED( ASW_Powerup_Fire_Bullets, DT_ASW_Powerup_Fire_Bullets );
BEGIN_NETWORK_TABLE( CASW_Powerup_Fire_Bullets, DT_ASW_Powerup_Fire_Bullets )
END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( asw_powerup_fire_bullets, CASW_Powerup_Fire_Bullets );

PRECACHE_REGISTER( asw_powerup_fire_bullets );

CASW_Powerup_Fire_Bullets::CASW_Powerup_Fire_Bullets()
{ 
	m_nPowerupType = POWERUP_TYPE_FIRE_BULLETS;
#ifdef CLIENT_DLL
	Q_snprintf(m_szUseIconText, sizeof(m_szUseIconText), "#asw_take_pow_fire_bullets");
#endif
}

//-------------
// Electric Bullets
//-------------
IMPLEMENT_NETWORKCLASS_ALIASED( ASW_Powerup_Electric_Bullets, DT_ASW_Powerup_Electric_Bullets );
BEGIN_NETWORK_TABLE( CASW_Powerup_Electric_Bullets, DT_ASW_Powerup_Electric_Bullets )
END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( asw_powerup_electric_bullets, CASW_Powerup_Electric_Bullets );

PRECACHE_REGISTER( asw_powerup_electric_bullets );

CASW_Powerup_Electric_Bullets::CASW_Powerup_Electric_Bullets()
{ 
	m_nPowerupType = POWERUP_TYPE_ELECTRIC_BULLETS;
#ifdef CLIENT_DLL
	Q_snprintf(m_szUseIconText, sizeof(m_szUseIconText), "#asw_take_pow_elec_bullets");
#endif
}

//-------------
// Chem Bullets
//-------------
IMPLEMENT_NETWORKCLASS_ALIASED( ASW_Powerup_Chemical_Bullets, DT_ASW_Powerup_Chemical_Bullets );
BEGIN_NETWORK_TABLE( CASW_Powerup_Chemical_Bullets, DT_ASW_Powerup_Chemical_Bullets )
END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( asw_powerup_chemical_bullets, CASW_Powerup_Chemical_Bullets );

PRECACHE_REGISTER( asw_powerup_chemical_bullets );

CASW_Powerup_Chemical_Bullets::CASW_Powerup_Chemical_Bullets()
{ 
	//m_nPowerupType = POWERUP_TYPE_CHEMICAL_BULLETS;
#ifdef CLIENT_DLL
	Q_snprintf(m_szUseIconText, sizeof(m_szUseIconText), "#asw_take_pow_chem_bullets");
#endif
}

//-------------
// EXPLODEY Bullets
//-------------
IMPLEMENT_NETWORKCLASS_ALIASED( ASW_Powerup_Explosive_Bullets, DT_ASW_Powerup_Explosive_Bullets );
BEGIN_NETWORK_TABLE( CASW_Powerup_Explosive_Bullets, DT_ASW_Powerup_Explosive_Bullets )
END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( asw_powerup_explosive_bullets, CASW_Powerup_Explosive_Bullets );

PRECACHE_REGISTER( asw_powerup_explosive_bullets );

CASW_Powerup_Explosive_Bullets::CASW_Powerup_Explosive_Bullets()
{ 
	//m_nPowerupType = POWERUP_TYPE_EXPLOSIVE_BULLETS;
#ifdef CLIENT_DLL
	Q_snprintf(m_szUseIconText, sizeof(m_szUseIconText), "#asw_take_pow_explo_bullets");
#endif
}

//-------------
// Increased Speed
//-------------
IMPLEMENT_NETWORKCLASS_ALIASED( ASW_Powerup_Increased_Speed, DT_ASW_Powerup_Increased_Speed );
BEGIN_NETWORK_TABLE( CASW_Powerup_Increased_Speed, DT_ASW_Powerup_Increased_Speed )
END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( asw_powerup_increased_speed, CASW_Powerup_Increased_Speed );

PRECACHE_REGISTER( asw_powerup_increased_speed );

CASW_Powerup_Increased_Speed::CASW_Powerup_Increased_Speed()
{ 
	m_nPowerupType = POWERUP_TYPE_INCREASED_SPEED;
	m_flPowerupDuration = 15.0f;
#ifdef CLIENT_DLL
	Q_snprintf(m_szUseIconText, sizeof(m_szUseIconText), "#asw_take_pow_inc_speed");
#endif
}