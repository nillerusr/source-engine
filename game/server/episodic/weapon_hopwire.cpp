//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "basehlcombatweapon.h"
#include "player.h"
#include "gamerules.h"
#include "grenade_frag.h"
#include "npcevent.h"
#include "engine/IEngineSound.h"
#include "items.h"
#include "in_buttons.h"
#include "soundent.h"
#include "gamestats.h"
#include "grenade_hopwire.h"
#ifdef EZ2
#include "ez2/ez2_player.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define GRENADE_TIMER	2.0f	//Seconds

#define GRENADE_PAUSED_NO			0
#define GRENADE_PAUSED_PRIMARY		1
#define GRENADE_PAUSED_SECONDARY	2

#define GRENADE_RADIUS	4.0f	// inches

//-----------------------------------------------------------------------------
// Fragmentation grenades
//-----------------------------------------------------------------------------
class CWeaponHopwire: public CBaseHLCombatWeapon
{
	DECLARE_CLASS( CWeaponHopwire, CBaseHLCombatWeapon );
public:
	DECLARE_SERVERCLASS();

	CWeaponHopwire();
	void	Precache( void );
	void	Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator );
	void	PrimaryAttack( void );
	void	SecondaryAttack( void );
	void	DecrementAmmo( CBaseCombatCharacter *pOwner );
	void	ItemPostFrame( void );

	void	HandleFireOnEmpty( void );
	bool	HasAnyAmmo( void );
	bool	Deploy( void );
	bool	Holster( CBaseCombatWeapon *pSwitchingTo = NULL );

	int		CapabilitiesGet( void ) { return bits_CAP_WEAPON_RANGE_ATTACK1; }
	
	bool	Reload( void );

	bool	ShouldDisplayHUDHint() { return true; }


	virtual HopwireStyle GetHopwireStyle() { return HOPWIRE_XEN; }

protected:
	virtual void	ThrowGrenade( CBasePlayer *pPlayer );
	virtual void	RollGrenade( CBasePlayer *pPlayer );
	virtual void	LobGrenade( CBasePlayer *pPlayer );

private:
	// check a throw from vecSrc.  If not valid, move the position back along the line to vecEye
	void	CheckThrowPosition( CBasePlayer *pPlayer, const Vector &vecEye, Vector &vecSrc );

	bool	m_bRedraw;	//Draw the weapon again after throwing a grenade
	
	int		m_AttackPaused;
	bool	m_fDrawbackFinished;

	CHandle<CGrenadeHopwire>	m_hActiveHopWire;

	DECLARE_ACTTABLE();

	DECLARE_DATADESC();
};


BEGIN_DATADESC( CWeaponHopwire )
	DEFINE_FIELD( m_bRedraw, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_AttackPaused, FIELD_INTEGER ),
	DEFINE_FIELD( m_fDrawbackFinished, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_hActiveHopWire, FIELD_EHANDLE ),
END_DATADESC()

acttable_t	CWeaponHopwire::m_acttable[] = 
{
	{ ACT_RANGE_ATTACK1, ACT_RANGE_ATTACK_SLAM, true },
};

IMPLEMENT_ACTTABLE(CWeaponHopwire);

IMPLEMENT_SERVERCLASS_ST(CWeaponHopwire, DT_WeaponHopwire)
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( weapon_hopwire, CWeaponHopwire );

#ifdef EZ2
class CWeaponStasisGrenade : public CWeaponHopwire
{
	DECLARE_CLASS( CWeaponHopwire, CWeaponHopwire );
public:

	virtual HopwireStyle GetHopwireStyle() { return HOPWIRE_STASIS; }

	DECLARE_SERVERCLASS();
};

IMPLEMENT_SERVERCLASS_ST( CWeaponStasisGrenade, DT_WeaponStasisGrenade )
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( weapon_stasis_grenade, CWeaponStasisGrenade );
#endif

// In E:Z2, the hopwire's Xen spawning feature precaches a massive number of assets which shouldn't be used on levels which don't have Xen grenades.
// As a result, the hopwire is precached in the same way as any other entity. (i.e. not precached at all times like other weapons are)
#ifndef EZ2
PRECACHE_WEAPON_REGISTER(weapon_hopwire);
#endif

CWeaponHopwire::CWeaponHopwire() :
	CBaseHLCombatWeapon(),
	m_bRedraw( false )
{
	NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponHopwire::Precache( void )
{
	BaseClass::Precache();

#ifdef EZ2
	if (GetHopwireStyle() == HOPWIRE_XEN)
	{
		VerifyXenRecipeManager( GetClassname() );
		UTIL_PrecacheOther( "npc_grenade_hopwire" );
	}
	else if (GetHopwireStyle() == HOPWIRE_STASIS)
	{
		UTIL_PrecacheOther( "npc_grenade_stasis" );
	}
#else
	UTIL_PrecacheOther( "npc_grenade_hopwire" );
#endif

	PrecacheScriptSound( "WeaponFrag.Throw" );
	PrecacheScriptSound( "WeaponFrag.Roll" );

	m_bRedraw = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CWeaponHopwire::Deploy( void )
{
	m_bRedraw = false;
	m_fDrawbackFinished = false;

	return BaseClass::Deploy();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CWeaponHopwire::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	if ( hopwire_trap.GetBool() && m_hActiveHopWire != NULL )
		return false;

	m_bRedraw = false;
	m_fDrawbackFinished = false;

	return BaseClass::Holster( pSwitchingTo );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEvent - 
//			*pOperator - 
//-----------------------------------------------------------------------------
void CWeaponHopwire::Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator )
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	bool fThrewGrenade = false;

	switch( pEvent->event )
	{
		case EVENT_WEAPON_SEQUENCE_FINISHED:
			m_fDrawbackFinished = true;
			break;

		case EVENT_WEAPON_THROW:
			ThrowGrenade( pOwner );
			DecrementAmmo( pOwner );
			fThrewGrenade = true;
			break;

		case EVENT_WEAPON_THROW2:
			RollGrenade( pOwner );
			DecrementAmmo( pOwner );
			fThrewGrenade = true;
			break;

		case EVENT_WEAPON_THROW3:
			LobGrenade( pOwner );
			DecrementAmmo( pOwner );
			fThrewGrenade = true;
			break;

		default:
			BaseClass::Operator_HandleAnimEvent( pEvent, pOperator );
			break;
	}

#define RETHROW_DELAY	0.5
	if( fThrewGrenade )
	{
		m_flNextPrimaryAttack	= gpGlobals->curtime + RETHROW_DELAY;
		m_flNextSecondaryAttack	= gpGlobals->curtime + RETHROW_DELAY;
		m_flTimeWeaponIdle = FLT_MAX; //NOTE: This is set once the animation has finished up!

		// Make a sound designed to scare snipers back into their holes!
		CBaseCombatCharacter *pOwner = GetOwner();

		if( pOwner )
		{
			Vector vecSrc = pOwner->Weapon_ShootPosition();
			Vector	vecDir;

			AngleVectors( pOwner->EyeAngles(), &vecDir );

			trace_t tr;

			UTIL_TraceLine( vecSrc, vecSrc + vecDir * 1024, MASK_SOLID_BRUSHONLY, pOwner, COLLISION_GROUP_NONE, &tr );

			CSoundEnt::InsertSound( SOUND_DANGER_SNIPERONLY, tr.endpos, 384, 0.2, pOwner );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Override the ammo behavior so we never disallow pulling the weapon out
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CWeaponHopwire::HasAnyAmmo( void )
{
	if ( hopwire_trap.GetBool() && m_hActiveHopWire != NULL )
		return true;

	return BaseClass::HasAnyAmmo();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CWeaponHopwire::Reload( void )
{
	if ( !HasPrimaryAmmo() )
		return false;

	if ( ( m_bRedraw ) && ( m_flNextPrimaryAttack <= gpGlobals->curtime ) && ( m_flNextSecondaryAttack <= gpGlobals->curtime ) )
	{
		//Redraw the weapon
		SendWeaponAnim( ACT_VM_DRAW );

		//Update our times
		m_flNextPrimaryAttack	= gpGlobals->curtime + SequenceDuration();
		m_flNextSecondaryAttack	= gpGlobals->curtime + SequenceDuration();
		m_flTimeWeaponIdle = gpGlobals->curtime + SequenceDuration();

		//Mark this as done
		m_bRedraw = false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponHopwire::SecondaryAttack( void )
{
	if ( m_bRedraw )
		return;

	if ( !HasPrimaryAmmo() )
		return;

	CBaseCombatCharacter *pOwner  = GetOwner();

	if ( pOwner == NULL )
		return;

	CBasePlayer *pPlayer = ToBasePlayer( pOwner );
	
	if ( pPlayer == NULL )
		return;

	// See if we're in trap mode
	if (hopwire_trap.GetBool() && (m_hActiveHopWire != NULL))
	{
		// Spring the trap
		m_hActiveHopWire->Detonate();
		m_hActiveHopWire = NULL;

		// Don't allow another throw for awhile
		m_flTimeWeaponIdle = m_flNextSecondaryAttack = gpGlobals->curtime + 2.0f;

		return;
	}

	// Note that this is a secondary attack and prepare the grenade attack to pause.
	m_AttackPaused = GRENADE_PAUSED_SECONDARY;
	SendWeaponAnim( ACT_VM_PULLBACK_LOW );

	// Don't let weapon idle interfere in the middle of a throw!
	m_flTimeWeaponIdle = FLT_MAX;
	m_flNextSecondaryAttack	= FLT_MAX;

	// If I'm now out of ammo, switch away
	if (!HasPrimaryAmmo() && !hopwire_trap.GetBool())
	{
		pPlayer->SwitchToNextBestWeapon( this );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Allow activation even if this is our last piece of ammo
//-----------------------------------------------------------------------------
void CWeaponHopwire::HandleFireOnEmpty( void )
{
	if (hopwire_trap.GetBool() && m_hActiveHopWire!= NULL )
	{
		// FIXME: This toggle is hokey
		m_bRedraw = false;
		PrimaryAttack();
		m_bRedraw = true;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponHopwire::PrimaryAttack( void )
{
	if ( m_bRedraw )
		return;

	if (!HasPrimaryAmmo())
		return;

	CBaseCombatCharacter *pOwner  = GetOwner();
	if ( pOwner == NULL )
		return;

	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );;
	if ( pPlayer == NULL )
		return;

	// See if we're in trap mode
	if ( hopwire_trap.GetBool() && ( m_hActiveHopWire != NULL ) )
	{
		// Spring the trap
		m_hActiveHopWire->Detonate();
		m_hActiveHopWire = NULL;

		// Don't allow another throw for awhile
		m_flTimeWeaponIdle = m_flNextPrimaryAttack = gpGlobals->curtime + 2.0f;

		return;
	}

	// Note that this is a primary attack and prepare the grenade attack to pause.
	m_AttackPaused = GRENADE_PAUSED_PRIMARY;
	SendWeaponAnim( ACT_VM_PULLBACK_HIGH );
	
	// Put both of these off indefinitely. We do not know how long
	// the player will hold the grenade.
	m_flTimeWeaponIdle = FLT_MAX;
	m_flNextPrimaryAttack = FLT_MAX;

	// If I'm now out of ammo, switch away
	if ( !HasPrimaryAmmo() && !hopwire_trap.GetBool() )
	{
		pPlayer->SwitchToNextBestWeapon( this );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pOwner - 
//-----------------------------------------------------------------------------
void CWeaponHopwire::DecrementAmmo( CBaseCombatCharacter *pOwner )
{
	pOwner->RemoveAmmo( 1, m_iPrimaryAmmoType );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponHopwire::ItemPostFrame( void )
{
	if( m_fDrawbackFinished )
	{
		CBasePlayer *pOwner = ToBasePlayer( GetOwner() );

		if (pOwner)
		{
			switch( m_AttackPaused )
			{
			case GRENADE_PAUSED_PRIMARY:
				if( !(pOwner->m_nButtons & IN_ATTACK) )
				{
					SendWeaponAnim( ACT_VM_THROW );
					m_fDrawbackFinished = false;
				}
				break;

			case GRENADE_PAUSED_SECONDARY:
				if( !(pOwner->m_nButtons & IN_ATTACK2) )
				{
					//See if we're ducking
					if ( pOwner->m_nButtons & IN_DUCK )
					{
						//Send the weapon animation
						SendWeaponAnim( ACT_VM_SECONDARYATTACK );
					}
					else
					{
						//Send the weapon animation
						SendWeaponAnim( ACT_VM_HAULBACK );
					}

					m_fDrawbackFinished = false;
				}
				break;

			default:
				break;
			}
		}
	}

	BaseClass::ItemPostFrame();

	if ( m_bRedraw )
	{
		if ( IsViewModelSequenceFinished() )
		{
			Reload();
		}
	}
}

	// check a throw from vecSrc.  If not valid, move the position back along the line to vecEye
void CWeaponHopwire::CheckThrowPosition( CBasePlayer *pPlayer, const Vector &vecEye, Vector &vecSrc )
{
	trace_t tr;

	UTIL_TraceHull( vecEye, vecSrc, -Vector(GRENADE_RADIUS+2,GRENADE_RADIUS+2,GRENADE_RADIUS+2), Vector(GRENADE_RADIUS+2,GRENADE_RADIUS+2,GRENADE_RADIUS+2), 
		pPlayer->PhysicsSolidMaskForEntity(), pPlayer, pPlayer->GetCollisionGroup(), &tr );
	
	if ( tr.DidHit() )
	{
		vecSrc = tr.endpos;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPlayer - 
//-----------------------------------------------------------------------------
void CWeaponHopwire::ThrowGrenade( CBasePlayer *pPlayer )
{
	Vector	vecEye = pPlayer->EyePosition();
	Vector	vForward, vRight;

	pPlayer->EyeVectors( &vForward, &vRight, NULL );
	Vector vecSrc = vecEye + vForward * 18.0f + vRight * 8.0f;
	CheckThrowPosition( pPlayer, vecEye, vecSrc );
	vForward[2] += 0.1f;

	Vector vecThrow;
	pPlayer->GetVelocity( &vecThrow, NULL );
	vecThrow += vForward * 1200;

#ifndef EZ2
	m_hActiveHopWire = static_cast<CGrenadeHopwire *> (HopWire_Create( vecSrc, vec3_angle, vecThrow, AngularImpulse(600,random->RandomInt(-1200,1200),0), pPlayer, GRENADE_TIMER, GetWorldModel(), GetSecondaryWorldModel()));
#else
	switch (GetHopwireStyle())
	{
	case HOPWIRE_STASIS:
		m_hActiveHopWire = static_cast<CGrenadeStasis *> (StasisGrenade_Create( vecSrc, vec3_angle, vecThrow, AngularImpulse( 600, random->RandomInt( -1200, 1200 ), 0 ), pPlayer, stasis_timer.GetFloat(), GetWorldModel(), GetSecondaryWorldModel()));
		break;
	default:
		m_hActiveHopWire = static_cast<CGrenadeHopwire *> (HopWire_Create( vecSrc, vec3_angle, vecThrow, AngularImpulse( 600, random->RandomInt( -1200, 1200 ), 0 ), pPlayer, hopwire_timer.GetFloat(), GetWorldModel(), GetSecondaryWorldModel() ));
		break;

	}
#endif

	m_bRedraw = true;

	WeaponSound( SINGLE );
#ifdef EZ2
	// Blixibon
	CEZ2_Player *pEZ2Player = assert_cast<CEZ2_Player*>(pPlayer);
	if (pEZ2Player)
	{
		pEZ2Player->Event_ThrewGrenade(this);
	}
#endif
	m_iPrimaryAttacks++;
	gamestats->Event_WeaponFired( pPlayer, true, GetClassname() );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPlayer - 
//-----------------------------------------------------------------------------
void CWeaponHopwire::LobGrenade( CBasePlayer *pPlayer )
{
	Vector	vecEye = pPlayer->EyePosition();
	Vector	vForward, vRight;

	pPlayer->EyeVectors( &vForward, &vRight, NULL );
	Vector vecSrc = vecEye + vForward * 18.0f + vRight * 8.0f + Vector( 0, 0, -8 );
	CheckThrowPosition( pPlayer, vecEye, vecSrc );
	
	Vector vecThrow;
	pPlayer->GetVelocity( &vecThrow, NULL );
	vecThrow += vForward * 350 + Vector( 0, 0, 50 );
#ifndef EZ2
	m_hActiveHopWire = static_cast<CGrenadeHopwire *> (HopWire_Create( vecSrc, vec3_angle, vecThrow, AngularImpulse( 200, random->RandomInt( -600, 600 ), 0 ), pPlayer, GRENADE_TIMER, GetWorldModel(), GetSecondaryWorldModel() ));
#else
	switch (GetHopwireStyle())
	{
	case HOPWIRE_STASIS:
		m_hActiveHopWire = static_cast<CGrenadeStasis *> (StasisGrenade_Create( vecSrc, vec3_angle, vecThrow, AngularImpulse( 200, random->RandomInt( -600, 600 ), 0 ), pPlayer, stasis_timer.GetFloat(), GetWorldModel(), GetSecondaryWorldModel() ));
		break;
	default:
		m_hActiveHopWire = static_cast<CGrenadeHopwire *> (HopWire_Create( vecSrc, vec3_angle, vecThrow, AngularImpulse( 200, random->RandomInt( -600, 600 ), 0 ), pPlayer, hopwire_timer.GetFloat(), GetWorldModel(), GetSecondaryWorldModel() ));
		break;

	}
#endif


	WeaponSound( WPN_DOUBLE );

	m_bRedraw = true;

	m_iPrimaryAttacks++;
	gamestats->Event_WeaponFired( pPlayer, true, GetClassname() );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPlayer - 
//-----------------------------------------------------------------------------
void CWeaponHopwire::RollGrenade( CBasePlayer *pPlayer )
{
	// BUGBUG: Hardcoded grenade width of 4 - better not change the model :)
	Vector vecSrc;
	pPlayer->CollisionProp()->NormalizedToWorldSpace( Vector( 0.5f, 0.5f, 0.0f ), &vecSrc );
	vecSrc.z += GRENADE_RADIUS;

	Vector vecFacing = pPlayer->BodyDirection2D( );
	// no up/down direction
	vecFacing.z = 0;
	VectorNormalize( vecFacing );
	trace_t tr;
	UTIL_TraceLine( vecSrc, vecSrc - Vector(0,0,16), MASK_PLAYERSOLID, pPlayer, COLLISION_GROUP_NONE, &tr );
	if ( tr.fraction != 1.0 )
	{
		// compute forward vec parallel to floor plane and roll grenade along that
		Vector tangent;
		CrossProduct( vecFacing, tr.plane.normal, tangent );
		CrossProduct( tr.plane.normal, tangent, vecFacing );
	}
	vecSrc += (vecFacing * 18.0);
	CheckThrowPosition( pPlayer, pPlayer->WorldSpaceCenter(), vecSrc );

	Vector vecThrow;
	pPlayer->GetVelocity( &vecThrow, NULL );
	vecThrow += vecFacing * 700;
	// put it on its side
	QAngle orientation(0,pPlayer->GetLocalAngles().y,-90);
	// roll it
	AngularImpulse rotSpeed(0,0,720);
#ifndef EZ2
	m_hActiveHopWire = static_cast<CGrenadeHopwire *> (HopWire_Create( vecSrc, orientation, vecThrow, rotSpeed, pPlayer, GRENADE_TIMER, GetWorldModel(), GetSecondaryWorldModel()));
#else
	switch (GetHopwireStyle())
	{
	case HOPWIRE_STASIS:
		m_hActiveHopWire = static_cast<CGrenadeStasis *> (StasisGrenade_Create( vecSrc, orientation, vecThrow, rotSpeed, pPlayer, stasis_timer.GetFloat(), GetWorldModel(), GetSecondaryWorldModel() ));
		break;
	default:
		m_hActiveHopWire = static_cast<CGrenadeHopwire *> (HopWire_Create( vecSrc, orientation, vecThrow, rotSpeed, pPlayer, hopwire_timer.GetFloat(), GetWorldModel(), GetSecondaryWorldModel() ));
		break;

	}
#endif

	WeaponSound( SPECIAL1 );

	m_bRedraw = true;

	m_iPrimaryAttacks++;
	gamestats->Event_WeaponFired( pPlayer, true, GetClassname() );
}

#ifdef EZ
class CWeaponEndGame : public CWeaponHopwire
{
	DECLARE_DATADESC();
public:
	DECLARE_CLASS( CWeaponEndGame, CWeaponHopwire );
	DECLARE_SERVERCLASS();

	void	Precache( void );

	bool	ShouldDisplayHUDHint() { return true; }
protected:
	void	ThrowGrenade( CBasePlayer *pPlayer );
	void	RollGrenade( CBasePlayer *pPlayer );
	void	LobGrenade( CBasePlayer *pPlayer );
private:
	void EndGame();
	COutputEvent m_OnEndGame;


};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponEndGame::Precache( void )
{
	// Skip Xen grenade precache
	CBaseHLCombatWeapon::Precache();
}

void CWeaponEndGame::ThrowGrenade( CBasePlayer * pPlayer )
{
	EndGame();
}

void CWeaponEndGame::RollGrenade( CBasePlayer * pPlayer )
{
	EndGame();
}

void CWeaponEndGame::LobGrenade( CBasePlayer * pPlayer )
{
	EndGame();
}

void CWeaponEndGame::EndGame()
{
	m_OnEndGame.FireOutput( this, GetOwnerEntity(), 0.0f );
}


IMPLEMENT_SERVERCLASS_ST( CWeaponEndGame, DT_WeaponEndGame )
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( weapon_endgame, CWeaponEndGame );
PRECACHE_WEAPON_REGISTER( weapon_endgame );

BEGIN_DATADESC( CWeaponEndGame )
DEFINE_OUTPUT( m_OnEndGame, "OnEndGame" ),
END_DATADESC()
#endif
