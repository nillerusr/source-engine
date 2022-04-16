//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "cbase.h"
#include "weapon_dodbasebomb.h"


#ifdef CLIENT_DLL
	#include "c_dod_player.h"
#else
	#include "dod_player.h"
	#include "dod_bombtarget.h"
	#include "collisionutils.h"
	#include "in_buttons.h"
#endif


IMPLEMENT_NETWORKCLASS_ALIASED( DODBaseBombWeapon, DT_BaseBombWeapon )


BEGIN_NETWORK_TABLE( CDODBaseBombWeapon, DT_BaseBombWeapon )

#ifdef CLIENT_DLL
	//RecvPropBool( RECVINFO(m_bDeployed) )
#else
	//SendPropBool( SENDINFO(m_bDeployed) )
#endif

END_NETWORK_TABLE()


BEGIN_PREDICTION_DATA( CDODBaseBombWeapon )
END_PREDICTION_DATA()

#ifndef CLIENT_DLL

BEGIN_DATADESC( CDODBaseBombWeapon )
END_DATADESC()

#endif

LINK_ENTITY_TO_CLASS( weapon_basebomb, CDODBaseBombWeapon );
PRECACHE_WEAPON_REGISTER( weapon_basebomb );

acttable_t CDODBaseBombWeapon::m_acttable[] = 
{
	{ ACT_PRONE_IDLE,						ACT_DOD_PRONEWALK_IDLE_PISTOL,			false },	//?
	{ ACT_PRONE_FORWARD,					ACT_DOD_PRONEWALK_IDLE_PISTOL,			false },	//?
	{ ACT_DOD_STAND_IDLE,					ACT_DOD_STAND_IDLE_TNT,					false },
	{ ACT_DOD_CROUCH_IDLE,					ACT_DOD_CROUCH_IDLE_TNT,				false },	//?
	{ ACT_DOD_CROUCHWALK_IDLE,				ACT_DOD_CROUCHWALK_IDLE_TNT,			false },
	{ ACT_DOD_WALK_IDLE,					ACT_DOD_WALK_IDLE_TNT,					false },
	{ ACT_DOD_RUN_IDLE,						ACT_DOD_RUN_IDLE_TNT,					false },
	{ ACT_SPRINT,							ACT_DOD_SPRINT_IDLE_TNT,				false },

	// Hand Signals
	{ ACT_DOD_HS_IDLE,						ACT_DOD_HS_IDLE_PISTOL,					false },
	{ ACT_DOD_HS_CROUCH,					ACT_DOD_HS_CROUCH_PISTOL,				false },
};

IMPLEMENT_ACTTABLE( CDODBaseBombWeapon );

CDODBaseBombWeapon::CDODBaseBombWeapon()
{
}

void CDODBaseBombWeapon::Spawn( )
{
	WEAPON_FILE_INFO_HANDLE	hWpnInfo = LookupWeaponInfoSlot( GetClassname() );

	Assert( hWpnInfo != GetInvalidWeaponInfoHandle() );

	CDODWeaponInfo *pWeaponInfo = dynamic_cast< CDODWeaponInfo* >( GetFileWeaponInfoFromHandle( hWpnInfo ) );

	Assert( pWeaponInfo && "Failed to get CDODWeaponInfo in weapon spawn" );

	m_pWeaponInfo = pWeaponInfo;

	SetPlanting( false );

	BaseClass::Spawn();
}

void CDODBaseBombWeapon::Precache()
{
	BaseClass::Precache();
}

bool CDODBaseBombWeapon::Deploy( )
{
#ifdef CLIENT_DLL
	CDODPlayer *pPlayer = GetDODPlayerOwner();

	if ( pPlayer )
	{
		pPlayer->HintMessage( HINT_BOMB_FIRST_SELECT );
	}
#endif

	return DefaultDeploy( (char*)GetViewModel(), (char*)GetWorldModel(), GetDrawActivity(), (char*)GetAnimPrefix() );
}

void CDODBaseBombWeapon::PrimaryAttack()
{
#ifndef CLIENT_DLL
	if ( IsPlanting() )
	{
		CDODBombTarget *pTarget = (CDODBombTarget *)m_hBombTarget.Get();
		CBasePlayer *pPlayer = GetPlayerOwner();

		if ( !pTarget || !pPlayer )
		{
			CancelPlanting();
			return;
		}

		if ( pTarget->CanPlantHere( GetDODPlayerOwner() ) == false )
		{
			// if the target is not active anymore, cancel ( someone planted there already? )
			CancelPlanting();
		}
		else if ( ( pTarget->GetAbsOrigin() - pPlayer->WorldSpaceCenter() ).Length() > DOD_BOMB_PLANT_RADIUS )
		{
			// if we're too far away, cancel
			CancelPlanting();			
		}
		else if ( IsLookingAtBombTarget( pPlayer, pTarget ) == false || ( pPlayer->GetFlags() & FL_ONGROUND ) == 0 )
		{
			// not looking at the target anymore
			CancelPlanting();
		}
		else if ( gpGlobals->curtime > m_flPlantCompleteTime )
		{
			// we finished the plant
			CompletePlant();
		}
		else
		{
			m_flNextPlantCheck = gpGlobals->curtime + 0.2;
		}

		return;
	}

	// find nearby, visible bomb targets
	CBaseEntity *pEnt = NULL;
	CDODBombTarget *pBestTarget = NULL;

	float flBestDist = FLT_MAX;

	CDODPlayer *pPlayer = GetDODPlayerOwner();

	if ( !pPlayer )
		return;
	
	while( ( pEnt = gEntList.FindEntityByClassname( pEnt, "dod_bomb_target" ) ) != NULL )
	{
		CDODBombTarget *pTarget = static_cast<CDODBombTarget *>( pEnt );

		if ( !pTarget->CanPlantHere( pPlayer ) )
			continue;

		Vector pos = pPlayer->WorldSpaceCenter();

		float flDist = ( pos - pTarget->GetAbsOrigin() ).Length();

		// if we are looking directly at a bomb target and it is within our radius, that automatically wins
		if ( flDist < flBestDist &&
			flDist < DOD_BOMB_PLANT_RADIUS &&
			IsLookingAtBombTarget( pPlayer, pTarget ) &&
			( pPlayer->GetFlags() & FL_ONGROUND ) )
		{
			flBestDist = flDist;
			pBestTarget = pTarget;
		}
	}

	if ( pBestTarget )
	{
		StartPlanting( pBestTarget );
	}

	m_flNextPlantCheck = gpGlobals->curtime + 0.2;

	// true if the player is not holding primary attack
	m_bUsePlant = !( pPlayer->m_nButtons & (IN_ATTACK|IN_ATTACK2) );
#endif
}

void CDODBaseBombWeapon::SecondaryAttack()
{
	PrimaryAttack();
}

#ifndef CLIENT_DLL
void CDODBaseBombWeapon::StartPlanting( CDODBombTarget *pTarget )
{
	// we have already checked that we can plant here

	// store a pointer to the target we're bombing
	m_hBombTarget = pTarget;

	// must do this after setting the bomb target as we tell the planter
	// what target they are at
	SetPlanting( true );

	// set the timer for when we'll be done
	m_flPlantCompleteTime = gpGlobals->curtime + DOD_BOMB_PLANT_TIME;

	// play the planting animation
	SendWeaponAnim( ACT_VM_PRIMARYATTACK );

	CDODPlayer *pPlayer = GetDODPlayerOwner();

	if ( pPlayer )
	{
		pPlayer->DoAnimationEvent( PLAYERANIMEVENT_PLANT_TNT );

		pPlayer->SetMaxSpeed( 1 );

		pPlayer->SetProgressBarTime( DOD_BOMB_PLANT_TIME );
	}
}

bool CDODBaseBombWeapon::CancelPlanting( void )
{
	bool bHolster = false;

	SetPlanting( false );

	// play a stop animation
	SendWeaponAnim( ACT_VM_IDLE );

	CDODPlayer *pPlayer = GetDODPlayerOwner();

	if ( pPlayer )
	{
		pPlayer->DoAnimationEvent( PLAYERANIMEVENT_CANCEL_GESTURES );

		// restore player speed
		pPlayer->SetMaxSpeed( 600 );

		pPlayer->ResetProgressBar();

		if ( m_bUsePlant )
		{
			pPlayer->SelectLastItem();

			bHolster = true;
		}
	}

	return bHolster;
}

void CDODBaseBombWeapon::CompletePlant( void )
{
	CDODPlayer *pPlayer = ToDODPlayer( GetPlayerOwner() );

	if ( pPlayer )
	{
		SetPlanting( false );

		// restore player speed
		GetDODPlayerOwner()->SetMaxSpeed( 600 );

		// Tell the target that we finished planting the bomb
		((CDODBombTarget *)m_hBombTarget.Get())->CompletePlanting( pPlayer );

		// destroy the bomb weapon
		pPlayer->Weapon_Drop( this, NULL, NULL );
		UTIL_Remove(this);

		pPlayer->ResetProgressBar();

		pPlayer->SelectLastItem();
	}
}

bool CDODBaseBombWeapon::IsLookingAtBombTarget( CBasePlayer *pPlayer, CDODBombTarget *pTarget )
{
	Vector forward;
	AngleVectors( pPlayer->EyeAngles(), &forward );

	Vector toBomb = pTarget->GetAbsOrigin() - pPlayer->EyePosition();
	toBomb.NormalizeInPlace();

	return ( DotProduct( forward, toBomb ) >= 0.8 );
}

#endif

void CDODBaseBombWeapon::ItemPostFrame()
{
#ifndef CLIENT_DLL
	CBasePlayer *pPlayer = GetPlayerOwner();

	if ( !pPlayer )
		return;

	if ( pPlayer->m_nButtons & (IN_ATTACK|IN_ATTACK2) )
	{
		PrimaryAttack();
	}
	// Only use the time check if we are planting with the +use key
	// adds a slight lag to breaking the player lock otherwise
	else if ( !m_bUsePlant || m_flNextPlantCheck < gpGlobals->curtime )
	{
		if ( IsPlanting() )
		{
			// reset all planting
			bool bHolster = CancelPlanting();

			// sometimes after canceling we put the weapon away and switch
			// to our last weapon. In that case, we don't want to send any more
			// anim calls b/c it confuses the client.
			if ( bHolster )
			{
				// we've put this weapon away, stop everything
				return;
			}

			// anim now
			m_flTimeWeaponIdle = 0;
		}
		
		// idle
		if (m_flTimeWeaponIdle > gpGlobals->curtime)
			return;

		SendWeaponAnim( GetIdleActivity() );

		m_flTimeWeaponIdle = gpGlobals->curtime + SequenceDuration();

		// if we're not planting, why do we have the bomb out?
		// switch to our next best weapon
		pPlayer->SelectLastItem();
	}
	
#endif	// CLIENT_DLL
}

bool CDODBaseBombWeapon::Holster( CBaseCombatWeapon *pSwitchingTo )
{
#ifndef CLIENT_DLL
	if ( IsPlanting() )
		CancelPlanting();
#endif

	return BaseClass::Holster( pSwitchingTo );
}

void CDODBaseBombWeapon::SetPlanting( bool bPlanting )
{
	m_bPlanting = bPlanting;

#ifndef CLIENT_DLL
	CDODPlayer *pPlayer = GetDODPlayerOwner();

	if ( pPlayer )
	{
		pPlayer->SetPlanting( m_bPlanting ? (CDODBombTarget *)m_hBombTarget.Get() : NULL );
	}
#endif
}

bool CDODBaseBombWeapon::IsPlanting( void )
{
	return m_bPlanting;
}