//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:		Hand grenade
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "npcevent.h"
#include "hl1mp_basecombatweapon_shared.h"
//#include "basecombatcharacter.h"
//#include "AI_BaseNPC.h"
#ifdef CLIENT_DLL
#include "hl1/hl1_c_player.h"
#else
#include "hl1_player.h"
#endif
#include "gamerules.h"
#include "in_buttons.h"
#ifdef CLIENT_DLL
#else
#include "soundent.h"
#include "game.h"
#endif
#include "vstdlib/random.h"
#include "engine/IEngineSound.h"
#ifdef CLIENT_DLL
#else
#include "hl1_basegrenade.h"
#endif


#define HANDGRENADE_MODEL "models/w_grenade.mdl"


#ifndef CLIENT_DLL

extern ConVar sk_plr_dmg_grenade;

//-----------------------------------------------------------------------------
// CHandGrenade
//-----------------------------------------------------------------------------
LINK_ENTITY_TO_CLASS( grenade_hand, CHandGrenade );

BEGIN_DATADESC( CHandGrenade )
	DEFINE_ENTITYFUNC( BounceTouch ),
END_DATADESC()


void CHandGrenade::Spawn( void )
{
	SetMoveType( MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_BOUNCE );
	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_NOT_STANDABLE );

	SetModel( HANDGRENADE_MODEL ); 

	UTIL_SetSize( this, Vector( 0, 0, 0 ), Vector( 0, 0, 0 ) );

	m_bHasWarnedAI = false;
}


void CHandGrenade::Precache( void )
{
	BaseClass::Precache( );

	PrecacheScriptSound( "Weapon_HandGrenade.GrenadeBounce" );

	PrecacheModel( HANDGRENADE_MODEL );
}


void CHandGrenade::ShootTimed( CBaseCombatCharacter *pOwner, Vector vecVelocity, float flTime )
{
	SetAbsVelocity( vecVelocity );

	SetThrower( pOwner );
	SetOwnerEntity( pOwner );

	SetTouch( &CHandGrenade::BounceTouch );	// Bounce if touched

	m_flDetonateTime = gpGlobals->curtime + flTime;
	SetThink( &CBaseGrenade::TumbleThink );
	SetNextThink( gpGlobals->curtime + 0.1 );
	if ( flTime < 0.1 )
	{
		SetNextThink( gpGlobals->curtime );
		SetAbsVelocity( vec3_origin );
	}

//	SetSequence( SelectWeightedSequence( ACT_GRENADE_TOSS ) );
	SetSequence( 0 );
	m_flPlaybackRate = 1.0;

	SetAbsAngles( QAngle( 0,0,60) );

	AngularImpulse angImpulse;
	angImpulse[0] = random->RandomInt( -200, 200 );
	angImpulse[1] = random->RandomInt( 400, 500 );
	angImpulse[2] = random->RandomInt( -100, 100 );
	ApplyLocalAngularVelocityImpulse( angImpulse );	

	SetGravity( UTIL_ScaleForGravity( 400 ) );	// use a lower gravity for grenades to make them easier to see
	SetFriction( 0.8 );

	SetDamage( sk_plr_dmg_grenade.GetFloat() );
	SetDamageRadius( GetDamage() * 2.5 );
}


void CHandGrenade ::BounceSound( void )
{
	EmitSound( "Weapon_HandGrenade.GrenadeBounce" );
}


void CHandGrenade::BounceTouch( CBaseEntity *pOther )
{
	if ( pOther->IsSolidFlagSet(FSOLID_TRIGGER | FSOLID_VOLUME_CONTENTS) )
		return;

	// don't hit the guy that launched this grenade
	if ( pOther == GetThrower() )
		return;

	// Do a special test for players
	if ( pOther->IsPlayer() )
	{
		// Never hit a player again (we'll explode and fixup anyway)
		SetCollisionGroup( COLLISION_GROUP_DEBRIS );
	}
	// only do damage if we're moving fairly fast
	if ( (pOther->m_takedamage != DAMAGE_NO) && (m_flNextAttack < gpGlobals->curtime && GetAbsVelocity().Length() > 100))
	{
		if ( GetThrower() )
		{
			trace_t tr;
			tr = CBaseEntity::GetTouchTrace( );
			ClearMultiDamage( );
			Vector forward;
			AngleVectors( GetAbsAngles(), &forward );

			CTakeDamageInfo info( this, GetThrower(), 1, DMG_CLUB );
			CalculateMeleeDamageForce( &info, forward, tr.endpos );
			pOther->DispatchTraceAttack( info, forward, &tr ); 
			ApplyMultiDamage();
		}
		m_flNextAttack = gpGlobals->curtime + 1.0; // debounce
	}

	Vector vecTestVelocity;
	// m_vecAngVelocity = Vector (300, 300, 300);

	// this is my heuristic for modulating the grenade velocity because grenades dropped purely vertical
	// or thrown very far tend to slow down too quickly for me to always catch just by testing velocity. 
	// trimming the Z velocity a bit seems to help quite a bit.
	vecTestVelocity = GetAbsVelocity(); 
	vecTestVelocity.z *= 0.45;

	if ( !m_bHasWarnedAI && vecTestVelocity.Length() <= 60 )
	{
		// grenade is moving really slow. It's probably very close to where it will ultimately stop moving. 
		// emit the danger sound.
		
		// register a radius louder than the explosion, so we make sure everyone gets out of the way
		CSoundEnt::InsertSound ( SOUND_DANGER, GetAbsOrigin(), m_flDamage / 0.4, 0.3 );
		m_bHasWarnedAI = TRUE;
	}

	// HACKHACK - On ground isn't always set, so look for ground underneath
	trace_t tr;
	UTIL_TraceLine( GetAbsOrigin(), GetAbsOrigin() - Vector(0,0,10), MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr );

	if ( tr.fraction < 1.0 )
	{
		// add a bit of static friction
//		SetAbsVelocity( GetAbsVelocity() * 0.8 );
		SetSequence( SelectWeightedSequence( ACT_IDLE ) );
		SetAbsAngles( vec3_angle );
	}

	// play bounce sound
	BounceSound();

	m_flPlaybackRate = GetAbsVelocity().Length() / 200.0;
	if (m_flPlaybackRate > 1.0)
		m_flPlaybackRate = 1;
	else if (m_flPlaybackRate < 0.5)
		m_flPlaybackRate = 0;
}

#endif

#ifdef CLIENT_DLL
#define CWeaponHandGrenade C_WeaponHandGrenade
#endif

//-----------------------------------------------------------------------------
// CWeaponHandGrenade
//-----------------------------------------------------------------------------

class CWeaponHandGrenade : public CBaseHL1MPCombatWeapon
{
	DECLARE_CLASS( CWeaponHandGrenade, CBaseHL1MPCombatWeapon );
public:

	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

	CWeaponHandGrenade( void );

	void	Precache( void );
	void	PrimaryAttack( void );
	void	WeaponIdle( void );
	bool	Deploy( void );
	bool	Holster( CBaseCombatWeapon *pSwitchingTo = NULL );

//	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

private:
//	float m_flStartThrow;
//	float m_flReleaseThrow;
	CNetworkVar( float, m_flStartThrow );
	CNetworkVar( float, m_flReleaseThrow );
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponHandGrenade, DT_WeaponHandGrenade );

BEGIN_NETWORK_TABLE( CWeaponHandGrenade, DT_WeaponHandGrenade )
#ifdef CLIENT_DLL
	RecvPropFloat( RECVINFO( m_flStartThrow ) ),
	RecvPropFloat( RECVINFO( m_flReleaseThrow ) ),
#else
	SendPropFloat( SENDINFO( m_flStartThrow ) ),
	SendPropFloat( SENDINFO( m_flReleaseThrow ) ),
#endif
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponHandGrenade )
#ifdef CLIENT_DLL
	DEFINE_PRED_FIELD( m_flStartThrow, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_flReleaseThrow, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
#endif
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_handgrenade, CWeaponHandGrenade );

PRECACHE_WEAPON_REGISTER( weapon_handgrenade );

//IMPLEMENT_SERVERCLASS_ST( CWeaponHandGrenade, DT_WeaponHandGrenade )
//END_SEND_TABLE()

BEGIN_DATADESC( CWeaponHandGrenade )
END_DATADESC()

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeaponHandGrenade::CWeaponHandGrenade( void )
{
	m_bReloadsSingly	= false;
	m_bFiresUnderwater	= true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponHandGrenade::Precache( void )
{
#ifndef CLIENT_DLL
	UTIL_PrecacheOther( "grenade_hand" );
#endif

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeaponHandGrenade::PrimaryAttack( void )
{
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if ( !pPlayer )
	{
		return;
	}

	if ( ( m_flStartThrow <= 0 ) && pPlayer->GetAmmoCount( m_iPrimaryAmmoType ) > 0 )
	{
		m_flStartThrow		= gpGlobals->curtime;
		m_flReleaseThrow	= 0;

		SendWeaponAnim( ACT_VM_PRIMARYATTACK );
		SetWeaponIdleTime( gpGlobals->curtime + 0.5 );
	}
}


void CWeaponHandGrenade::WeaponIdle( void )
{
	if ( m_flReleaseThrow == 0 && m_flStartThrow )
		 m_flReleaseThrow = gpGlobals->curtime;

	if ( !HasWeaponIdleTimeElapsed() )
		return;

	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if ( !pPlayer )
	{
		return;
	}

	if ( m_flStartThrow )
	{
		Vector vecAiming = pPlayer->GetAutoaimVector( 0 );

		QAngle angThrow;
		VectorAngles( vecAiming, angThrow );

		Vector vecUp;
		Vector vecRight;
		AngleVectors( angThrow, NULL, &vecRight, &vecUp );

		if ( angThrow.x > 180 )	// player is pitching up
			angThrow.x = -15 - ( 360 - angThrow.x ) * ( ( 90 - 10 ) / 90.0 );
		else					// player is pitching down
			angThrow.x = -15 + angThrow.x * ( ( 90 + 10 ) / 90.0 );

		float flVel = ( 90 - angThrow.x ) * 4;
		if ( flVel > 500 )
			flVel = 500;

		Vector vecFwd;
		AngleVectors( angThrow, &vecFwd );

		Vector vecSrc	= pPlayer->EyePosition() + vecFwd * 16;
		Vector vecThrow	= vecFwd * flVel + pPlayer->GetAbsVelocity();

		QAngle angles;
		VectorAngles( vecThrow, angles );
#ifndef CLIENT_DLL
		CHandGrenade *pGrenade = (CHandGrenade*)Create( "grenade_hand", vecSrc, angles );
		if ( pGrenade )
		{
			// always explode 3 seconds after the pin was pulled
			float flTime = m_flStartThrow - gpGlobals->curtime + 3.0;
			if ( flTime < 0 )
			{
				flTime = 0;
			}

			pGrenade->ShootTimed( pPlayer, vecThrow, flTime );
		}
#endif

		if ( flVel < 500 )
		{
			SendWeaponAnim( ACT_HANDGRENADE_THROW1 );
		}
		else if ( flVel < 1000 )
		{
			SendWeaponAnim( ACT_HANDGRENADE_THROW2 );
		}
		else
		{
			SendWeaponAnim( ACT_HANDGRENADE_THROW3 );
		}

		// player "shoot" animation
		pPlayer->SetAnimation( PLAYER_ATTACK1 );

		m_flReleaseThrow	= 0;
		m_flStartThrow		= 0;

		SetWeaponIdleTime( gpGlobals->curtime + 0.5 );
		m_flNextPrimaryAttack	= gpGlobals->curtime + 0.5;
		m_flNextSecondaryAttack	= gpGlobals->curtime + 0.5;

		pPlayer->RemoveAmmo( 1, m_iPrimaryAmmoType );

		return;
	}
	else if ( m_flReleaseThrow > 0 )
	{
		// we've finished the throw, restart.
		m_flStartThrow = 0;

		if ( pPlayer->GetAmmoCount( m_iPrimaryAmmoType ) > 0 )
		{
			SendWeaponAnim( ACT_VM_DRAW );
		}
		else
		{
//			RetireWeapon();
			return;
		}

		SetWeaponIdleTime( gpGlobals->curtime + random->RandomFloat( 10, 15 ) );
		m_flReleaseThrow = -1;
		return;
	}

	if ( pPlayer->GetAmmoCount( m_iPrimaryAmmoType ) > 0 )
	{
		float flRand = random->RandomFloat( 0, 1 );
		if ( flRand <= 0.75 )
		{
			SendWeaponAnim( ACT_VM_IDLE );
			SetWeaponIdleTime( gpGlobals->curtime + random->RandomFloat( 10, 15 ) );// how long till we do this again.
		}
		else 
		{
			SendWeaponAnim( ACT_VM_FIDGET );
		}
	}
}

bool CWeaponHandGrenade::Deploy( void )
{
	m_flReleaseThrow = -1;

	return DefaultDeploy( (char*)GetViewModel(), (char*)GetWorldModel(), ACT_VM_DRAW, (char*)GetAnimPrefix() );
}

bool CWeaponHandGrenade::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if ( !pPlayer )
	{
		return false;
	}

	if ( m_flStartThrow > 0 )
	{
		return false;
	}

	if ( !BaseClass::Holster( pSwitchingTo ) )
	{
		return false;
	}

	if ( pPlayer->GetAmmoCount( m_iPrimaryAmmoType ) <= 0 )
	{
#ifndef CLIENT_DLL
		SetThink( &CWeaponHandGrenade::DestroyItem );
		SetNextThink( gpGlobals->curtime + 0.1 );
#endif
	}

	pPlayer->SetNextAttack( gpGlobals->curtime + 0.5 );

	return true;
}
