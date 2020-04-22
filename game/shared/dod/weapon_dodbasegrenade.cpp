//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "weapon_dodbasegrenade.h"
#include "in_buttons.h"
#include "dod_gamerules.h"


#if defined( CLIENT_DLL )
	#include "c_dod_player.h"
#else
	#include "dod_player.h"
#endif

extern ConVar dod_bonusround;

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponDODBaseGrenade, DT_WeaponDODBaseGrenade )

BEGIN_NETWORK_TABLE(CWeaponDODBaseGrenade, DT_WeaponDODBaseGrenade)
#if !defined( CLIENT_DLL )
	SendPropBool( SENDINFO( m_bPinPulled ) ),
	SendPropBool( SENDINFO( m_bArmed ) ),
#else
	RecvPropBool( RECVINFO( m_bPinPulled ) ),
	RecvPropBool( RECVINFO( m_bArmed ) ),
#endif
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponDODBaseGrenade )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_basedodgrenade, CWeaponDODBaseGrenade );

acttable_t CWeaponDODBaseGrenade::m_acttable[] = 
{
	// Move this out to the specific grenades???
	{ ACT_DOD_STAND_AIM,					ACT_DOD_STAND_AIM_GREN_FRAG,				false },
	{ ACT_DOD_CROUCH_AIM,					ACT_DOD_CROUCH_AIM_GREN_FRAG,				false },
	{ ACT_DOD_CROUCHWALK_AIM,				ACT_DOD_CROUCHWALK_AIM_GREN_FRAG,			false },
	{ ACT_DOD_WALK_AIM,						ACT_DOD_WALK_AIM_GREN_FRAG,					false },
	{ ACT_DOD_RUN_AIM,						ACT_DOD_RUN_AIM_GREN_FRAG,					false },
	{ ACT_PRONE_IDLE,						ACT_DOD_PRONE_AIM_GREN_FRAG,				false },
	{ ACT_PRONE_FORWARD,					ACT_DOD_PRONEWALK_AIM_GREN_FRAG,			false },
	{ ACT_DOD_STAND_IDLE,					ACT_DOD_STAND_AIM_GREN_FRAG,				false },
	{ ACT_DOD_CROUCH_IDLE,					ACT_DOD_CROUCH_AIM_GREN_FRAG,				false },
	{ ACT_DOD_CROUCHWALK_IDLE,				ACT_DOD_CROUCHWALK_AIM_GREN_FRAG,			false },
	{ ACT_DOD_WALK_IDLE,					ACT_DOD_WALK_AIM_GREN_FRAG,					false },
	{ ACT_DOD_RUN_IDLE,						ACT_DOD_RUN_AIM_GREN_FRAG,					false },
	{ ACT_SPRINT,							ACT_DOD_SPRINT_AIM_GREN_FRAG,				false },

	{ ACT_RANGE_ATTACK1,				ACT_DOD_PRIMARYATTACK_GREN_FRAG,		false },
	{ ACT_DOD_PRIMARYATTACK_PRONE,		ACT_DOD_PRIMARYATTACK_PRONE_GREN_FRAG,	false },
	{ ACT_DOD_PRIMARYATTACK_CROUCH,		ACT_DOD_PRIMARYATTACK_CROUCH_GREN_FRAG, false },

	// Hand Signals
	{ ACT_DOD_HS_IDLE,						ACT_DOD_HS_IDLE_STICKGRENADE,			false },
	{ ACT_DOD_HS_CROUCH,					ACT_DOD_HS_CROUCH_STICKGRENADE,			false },
};

IMPLEMENT_ACTTABLE( CWeaponDODBaseGrenade );

CWeaponDODBaseGrenade::CWeaponDODBaseGrenade()
{
	m_bRedraw = false;
	m_bPinPulled = false;
	SetArmed( false );

	m_iAltFireHint = HINT_USE_PRIME;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CWeaponDODBaseGrenade::CanHolster( void )
{
	// can only holster hand grenades when not primed!
	return ( m_bPinPulled == false && !IsArmed() );
}

#ifdef CLIENT_DLL

	void CWeaponDODBaseGrenade::PrimaryAttack()
	{
		//nothing on the client
	}

#else

	BEGIN_DATADESC( CWeaponDODBaseGrenade )
		DEFINE_FIELD( m_bRedraw, FIELD_BOOLEAN ),
		DEFINE_INPUTFUNC( FIELD_FLOAT, "DetonateTime", InputSetDetonateTime ),
	END_DATADESC()

	//-----------------------------------------------------------------------------
	// Purpose: 
	//-----------------------------------------------------------------------------
	void CWeaponDODBaseGrenade::Precache()
	{
		PrecacheScriptSound( "Weapon_Grenade.Throw" );

		// Precache all the grenade minimap icons. 
		PrecacheMaterial( "sprites/minimap_icons/minimap_riflegren_ger" );
		PrecacheMaterial( "sprites/minimap_icons/minimap_riflegren_us" );
		PrecacheMaterial( "sprites/minimap_icons/grenade_hltv" );
		PrecacheMaterial( "sprites/minimap_icons/stick_hltv" );
		PrecacheMaterial( "sprites/minimap_icons/minimap_smoke_us" );
		PrecacheMaterial( "sprites/minimap_icons/minimap_smoke_ger" );

		BaseClass::Precache();
	}
	
	//-----------------------------------------------------------------------------
	// Purpose: 
	//-----------------------------------------------------------------------------
	bool CWeaponDODBaseGrenade::Deploy()
	{
		m_bRedraw = false;
		m_bPinPulled = false;

		return BaseClass::Deploy();
	}

	//-----------------------------------------------------------------------------
	// Purpose: 
	// Output : Returns true on success, false on failure.
	//-----------------------------------------------------------------------------
	bool CWeaponDODBaseGrenade::Holster( CBaseCombatWeapon *pSwitchingTo )
	{
		m_bRedraw = false;

#ifndef CLIENT_DLL
		// If they attempt to switch weapons before the throw animation is done, 
		// allow it, but kill the weapon if we have to.

		CDODPlayer *pPlayer = ToDODPlayer( GetPlayerOwner() );

		if( pPlayer && pPlayer->GetAmmoCount(m_iPrimaryAmmoType) <= 0 )
		{
			pPlayer->Weapon_Drop( this, NULL, NULL );
			UTIL_Remove(this);
		}
#endif

		return BaseClass::Holster( pSwitchingTo );
	}

	//-----------------------------------------------------------------------------
	// Purpose: 
	// Output : Returns true on success, false on failure.
	//-----------------------------------------------------------------------------
	bool CWeaponDODBaseGrenade::Reload()
	{
		if ( ( m_bRedraw ) && ( m_flNextPrimaryAttack <= gpGlobals->curtime ) && ( m_flNextSecondaryAttack <= gpGlobals->curtime ) )
		{
			//Redraw the weapon
			SendWeaponAnim( GetDrawActivity() );

			//Update our times
			m_flNextPrimaryAttack	= gpGlobals->curtime + SequenceDuration();
			m_flNextSecondaryAttack	= gpGlobals->curtime + SequenceDuration();
			m_flTimeWeaponIdle = gpGlobals->curtime + SequenceDuration();

			//Mark this as done
			m_bRedraw = false;
		}

		return true;
	}

enum
{
	THROW_THROW	= 0,
	THROW_PRIME
};
	
	//-----------------------------------------------------------------------------
	// Purpose: 
	//-----------------------------------------------------------------------------
	void CWeaponDODBaseGrenade::PrimaryAttack()
	{
		if ( IsArmed() )	// live grenade already?
		{
			StartThrow( THROW_THROW );
		}
		else
		{
			StartThrow( THROW_PRIME );
		}
	}

	void CWeaponDODBaseGrenade::StartThrow( int throwType )
	{
		if( GetPlayerOwner()->GetWaterLevel() > 2 )
		{
			PlayEmptySound();
			m_flNextPrimaryAttack = gpGlobals->curtime + 1.0;
			return;
		}

		if ( !m_bPinPulled )
		{
			m_bPinPulled = true; 

			SendWeaponAnim( GetPrimaryAttackActivity() );

			// Can't prime an already primed grenade!
			if ( !IsArmed() && throwType == THROW_PRIME )
			{
				SetArmed( true );

				m_flDetonateTime = gpGlobals->curtime + GetDetonateTimerLength();

				// start the hissing noise
			}

			m_flTimeWeaponIdle		= gpGlobals->curtime + SequenceDuration();
			m_flNextPrimaryAttack	= gpGlobals->curtime + SequenceDuration();
			m_flNextSecondaryAttack	= gpGlobals->curtime + SequenceDuration();
		}
	}

	//-----------------------------------------------------------------------------
	// Purpose: 
	//-----------------------------------------------------------------------------
	void CWeaponDODBaseGrenade::ItemPostFrame()
	{
		CDODPlayer *pPlayer = ToDODPlayer( GetPlayerOwner() );
		if ( !pPlayer )
			return;

		CBaseViewModel *pViewModel = pPlayer->GetViewModel( m_nViewModelIndex );
		if ( !pViewModel )
			return;

		if( IsArmed() && gpGlobals->curtime > m_flDetonateTime )
		{
			//Drop it!
			DropGrenade();
			m_flNextPrimaryAttack = gpGlobals->curtime + 1.0;
			m_flNextSecondaryAttack = gpGlobals->curtime + 1.0;
			return;
		}

		if ( m_bPinPulled &&
			 !(pPlayer->m_nButtons & IN_ATTACK) && 
			 !(pPlayer->m_nButtons & IN_ATTACK2) // If they let go of the fire button, they want to throw the grenade.
			 )
		{
			pPlayer->DoAnimationEvent( PLAYERANIMEVENT_THROW_GRENADE );

			ThrowGrenade(false);

			m_bRedraw = true;
			m_bPinPulled = false;
			SetArmed( false );

			DecrementAmmo( pPlayer );		

			SendWeaponAnim( ACT_VM_THROW );
		}
		else
		{
			BaseClass::ItemPostFrame();
			
			if ( m_bRedraw )
			{
				if (pPlayer->GetAmmoCount(m_iPrimaryAmmoType) <= 0)
				{
					pPlayer->Weapon_Drop( this, NULL, NULL );
					UTIL_Remove(this);
				}
				else
                    Reload();
			}
		}
	}


	//-----------------------------------------------------------------------------
	// Purpose: 
	// Input  : *pOwner - 
	//-----------------------------------------------------------------------------
	void CWeaponDODBaseGrenade::DecrementAmmo( CBaseCombatCharacter *pOwner )
	{
		pOwner->RemoveAmmo( 1, m_iPrimaryAmmoType );

		if (pOwner->GetAmmoCount(m_iPrimaryAmmoType) <= 0)
		{
            pOwner->Weapon_Drop( this, NULL, NULL );
			UTIL_Remove(this);
		}
	}

	void CWeaponDODBaseGrenade::DropGrenade( void )
	{
		CDODPlayer *pPlayer = ToDODPlayer( GetPlayerOwner() );
		if ( !pPlayer )
			return;

		pPlayer->DoAnimationEvent( PLAYERANIMEVENT_THROW_GRENADE );

		bool bThrow = true;

		DODRoundState state = DODGameRules()->State_Get();

		// If the player is holding a nade that is going to explode
		// and the round is over, not allowing them to throw it
		// just don't emit a grenade.
		if ( dod_bonusround.GetBool() )
		{
			int team = pPlayer->GetTeamNumber();

			if ( ( team == TEAM_ALLIES && state == STATE_AXIS_WIN ) || 
				( team == TEAM_AXIS && state == STATE_ALLIES_WIN ) )
			{
				bThrow = false;
			}
		}

		if ( bThrow )
			ThrowGrenade(true);

		m_bRedraw = true;
		m_bPinPulled = false;
		SetArmed( false );

		DecrementAmmo( pPlayer );		

		SendWeaponAnim( ACT_VM_THROW );
	}

	ConVar dod_grenadespeed( "dod_grenadespeed", "4.2", FCVAR_CHEAT );
	ConVar dod_grenademaxspeed( "dod_grenademaxspeed", "1400", FCVAR_CHEAT );
	ConVar dod_grenademinspeed( "dod_grenademinspeed", "500", FCVAR_CHEAT );

	//-----------------------------------------------------------------------------
	// Purpose: 
	//-----------------------------------------------------------------------------
	void CWeaponDODBaseGrenade::ThrowGrenade( bool bDrop )
	{
		CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
		if ( !pPlayer )
		{
			Assert( false );
			return;
		}

		QAngle angThrow = pPlayer->LocalEyeAngles();

		Vector vForward, vRight, vUp;

		if( angThrow.x > 180 )
			angThrow.x -= 360;

		if (angThrow.x < 0)
			angThrow.x = -10 + angThrow.x * ((90 - 10) / 90.0);
		else
			angThrow.x = -10 + angThrow.x * ((90 + 10) / 90.0);

		AngleVectors( angThrow, &vForward, &vRight, &vUp );
		
		Vector eyes = pPlayer->GetAbsOrigin() + pPlayer->GetViewOffset();
		Vector vecSrc = eyes + vForward * 16; 	

		// Start with the player's velocity as the grenade vel
		Vector vecPlayerVel;
		pPlayer->GetVelocity( &vecPlayerVel, NULL );

		// Get player angles, not eye angles!
		QAngle angPlayerAngles = pPlayer->GetAbsAngles();
		Vector vecPlayerForward;
		AngleVectors( angPlayerAngles, &vecPlayerForward );
		Vector vecThrow = DotProduct( vecPlayerVel, vecPlayerForward ) * vecPlayerForward;

		if( bDrop )
		{
			vecThrow = vForward;
		}
		else	// we are throwing the grenade
		{
			// change the speed depending on the throw angle
			// straight down is 0%, straight up is 100%, linear between
			float flSpeed = ( 90 - angThrow.x ) * dod_grenadespeed.GetFloat();

			//Msg( "speed %.f\n", flSpeed );

			flSpeed = clamp( flSpeed, dod_grenademinspeed.GetFloat(), dod_grenademaxspeed.GetFloat() );

			vecThrow += vForward * flSpeed;
		}

		trace_t tr;
		UTIL_TraceLine( eyes, vecSrc, MASK_SOLID, NULL, COLLISION_GROUP_NONE, &tr );
		
		// don't go into the ground
		if( tr.fraction < 1.0 ) 
		{
			vecSrc = tr.endpos;
		}

		float flTimeLeft;

		if ( IsArmed() )
			flTimeLeft = MAX( 0, m_flDetonateTime - gpGlobals->curtime );
		else
			flTimeLeft = GetDetonateTimerLength();

		EmitGrenade( vecSrc, vec3_angle, vecThrow, AngularImpulse(600,random->RandomInt(-1200,1200),0), pPlayer, flTimeLeft );

		pPlayer->EmitSound( "Weapon_Grenade.Throw" );

#ifndef CLIENT_DLL
		IGameEvent * event = gameeventmanager->CreateEvent( "dod_stats_weapon_attack" );
		if ( event )
		{
			event->SetInt( "attacker", pPlayer->GetUserID() );
			event->SetInt( "weapon", GetStatsWeaponID() );

			gameeventmanager->FireEvent( event );
		}
#endif	//CLIENT_DLL
	}

	//-----------------------------------------------------------------------------
	// Purpose: 
	//-----------------------------------------------------------------------------
	void CWeaponDODBaseGrenade::EmitGrenade( Vector vecSrc, QAngle vecAngles, Vector vecVel, AngularImpulse angImpulse, CBasePlayer *pPlayer, float flLifeTime /* = GRENADE_FUSE_LENGTH */ )
	{
		Assert( 0 && "CBaseCSGrenade::EmitGrenade should not be called. Make sure to implement this in your subclass!\n" );
	}

	//-----------------------------------------------------------------------------
	// Purpose:
	//-----------------------------------------------------------------------------
	bool CWeaponDODBaseGrenade::AllowsAutoSwitchFrom( void ) const
	{
		return ( m_bPinPulled == false );
	}

	void CWeaponDODBaseGrenade::InputSetDetonateTime( inputdata_t &inputdata )
	{
		SetDetonateTime( inputdata.value.Float() );
	}

#endif	// !CLIENT_DLL

Activity CWeaponDODBaseGrenade::GetIdleActivity( void )
{
	if ( IsArmed() )
		return ACT_VM_IDLE_EMPTY;
	else
		return ACT_VM_IDLE;
}

Activity CWeaponDODBaseGrenade::GetPrimaryAttackActivity( void )
{
	if ( IsArmed() )
		return ACT_VM_PRIMARYATTACK_EMPTY;
	else
		return ACT_VM_PULLPIN;
}

Activity CWeaponDODBaseGrenade::GetDrawActivity( void )
{
	if ( IsArmed() )
		return ACT_VM_DRAW_EMPTY;
	else
		return ACT_VM_DRAW;
}

