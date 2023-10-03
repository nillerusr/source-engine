#include "cbase.h"
#include "asw_weapon_ammo_satchel_shared.h"
#include "in_buttons.h"

#ifdef CLIENT_DLL
#include "c_asw_player.h"
#include "c_asw_weapon.h"
#include "c_asw_marine.h"
//#include "prediction.h"
#else
#include "asw_marine.h"
#include "asw_player.h"
#include "asw_weapon.h"
#include "npcevent.h"
#include "asw_ammo_drop.h"
#include "func_movelinear.h"
//#include "shot_manipulator.h"
#endif
#include "asw_util_shared.h"
#include "particle_parse.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_NETWORKCLASS_ALIASED( ASW_Weapon_Ammo_Satchel, DT_ASW_Weapon_Ammo_Satchel )

BEGIN_NETWORK_TABLE( CASW_Weapon_Ammo_Satchel, DT_ASW_Weapon_Ammo_Satchel )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CASW_Weapon_Ammo_Satchel )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( asw_weapon_ammo_satchel, CASW_Weapon_Ammo_Satchel );
PRECACHE_WEAPON_REGISTER( asw_weapon_ammo_satchel );

#ifndef CLIENT_DLL

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CASW_Weapon_Ammo_Satchel )
	
END_DATADESC()
#endif /* not client */

CASW_Weapon_Ammo_Satchel::CASW_Weapon_Ammo_Satchel()
{
	m_fLastAmmoDropTime = FLT_MIN;

	//m_nAmmoDrops = AMMO_SATCHEL_DEFAULT_DROP_COUNT;
}


CASW_Weapon_Ammo_Satchel::~CASW_Weapon_Ammo_Satchel()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Activity
//-----------------------------------------------------------------------------
Activity CASW_Weapon_Ammo_Satchel::GetPrimaryAttackActivity( void )
{
	return ACT_VM_PRIMARYATTACK;
}

void CASW_Weapon_Ammo_Satchel::PrimaryAttack( void )
{
	DeployAmmoDrop();
}

bool CASW_Weapon_Ammo_Satchel::OffhandActivate()
{
	if (!GetMarine() || GetMarine()->GetFlags() & FL_FROZEN)	// don't allow this if the marine is frozen
		return false;
	PrimaryAttack();

	return true;
}

void CASW_Weapon_Ammo_Satchel::DeployAmmoDrop()
{
	CASW_Player *pPlayer = GetCommander();
	if ( !pPlayer )
		return;

	if ( gpGlobals->curtime <= m_fLastAmmoDropTime + 1.0f )
	{
		return;
	}
	m_fLastAmmoDropTime = gpGlobals->curtime;

	if( m_iClip1 <= 0 )
	{
		Assert( false );
		return;
	}

	CASW_Marine *pMarine = GetMarine();
	if ( !pMarine )
		return;
	
	WeaponSound(SINGLE);

#ifndef CLIENT_DLL
	Vector	vecAiming	= pPlayer->GetAutoaimVectorForMarine(pMarine, GetAutoAimAmount(), GetVerticalAdjustOnlyAutoAimAmount());	// 45 degrees = 0.707106781187
	Vector	vecSrc		= pMarine->Weapon_ShootPosition( ) + (vecAiming * 8);

	if ( !pMarine->IsInhabited() && vecSrc.DistTo( pMarine->m_vecOffhandItemSpot ) < 150.0f )
	{
		vecSrc.x = pMarine->m_vecOffhandItemSpot.x;
		vecSrc.y = pMarine->m_vecOffhandItemSpot.y;
	}

	Vector newVel = GetBulletSpread();
	if ( pMarine->GetWaterLevel() == 3 )
	{		
		CBaseEntity::EmitSound( "ASW_Ammobag.Fail" );
		return;
	}
	
	Vector vecSatchelMins = Vector(-20,-20,0);
	Vector vecSatchelMaxs = Vector(20,20,60);
	trace_t tr;

	UTIL_TraceHull( vecSrc + Vector( 0, 0, 50 ),
				vecSrc + Vector( 0, 0, -50 ),
				vecSatchelMins,
				vecSatchelMaxs,
				MASK_SOLID,
				pMarine,
				COLLISION_GROUP_NONE,
				&tr );

	if ( tr.startsolid || tr.allsolid || ( tr.DidHit() && tr.fraction <= 0 ) || tr.fraction >= 1.0f ) // if there's something in the way, or no floor, don't deploy
	{
		// Try right under the player
		vecSrc = pMarine->GetAbsOrigin();

		UTIL_TraceHull( vecSrc + Vector( 0, 0, 50 ),
			vecSrc + Vector( 0, 0, 50 ),
			vecSatchelMins,
			vecSatchelMaxs,
			MASK_SOLID,
			pMarine,
			COLLISION_GROUP_NONE,
			&tr );

		if ( tr.startsolid || tr.allsolid || ( tr.DidHit() && tr.fraction <= 0 ) || tr.fraction >= 1.0f )
		{
			// Ok, fail
			CBaseEntity::EmitSound( "ASW_Ammobag.Fail" );
			return;
		}
	}

	vecSrc.z = tr.endpos.z;

	CASW_Ammo_Drop *pAmmoDrop = (CASW_Ammo_Drop *)CreateEntityByName( "asw_ammo_drop" );	

	UTIL_TraceLine( vecSrc, vecSrc + Vector(0, 0, -60), 
		MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr);

	UTIL_SetOrigin( pAmmoDrop, tr.endpos );
	pAmmoDrop->SetDeployer( pMarine );
	DispatchSpawn( pAmmoDrop );
	pAmmoDrop->PlayDeploySound();

	// Rotate it in the marine's facing direction
	QAngle angMarine = pMarine->GetAbsAngles();
	angMarine.x = 0;
	angMarine.z = 0;
	pAmmoDrop->SetAbsAngles( angMarine );

	CBaseEntity *pHelpHelpImBeingSupressed = (CBaseEntity*) te->GetSuppressHost();
	te->SetSuppressHost( NULL );
	DispatchParticleEffect( "ammo_satchel_take_med", pAmmoDrop->GetAbsOrigin() + Vector( 0, 0, 4 ), vec3_angle );
	te->SetSuppressHost( pHelpHelpImBeingSupressed );
	pMarine->OnWeaponFired( this, 1 );

	IGameEvent * event = gameeventmanager->CreateEvent( "player_deploy_ammo" );
	if ( event )
	{
		CASW_Marine *pMarine = GetMarine();
		CASW_Player *pPlayer = NULL;

		if ( pMarine )
		{
			pPlayer = pMarine->GetCommander();
		}

		event->SetInt( "userid", ( pPlayer ? pPlayer->GetUserID() : 0 ) );

		gameeventmanager->FireEvent( event );
	}
#endif

	m_iClip1--;
	if ( m_iClip1 <= 0 )
	{
#ifndef CLIENT_DLL
		pMarine->Weapon_Detach(this);
		Kill();
#endif
		pMarine->SwitchToNextBestWeapon( NULL );
	}
}

void CASW_Weapon_Ammo_Satchel::Precache()
{
	BaseClass::Precache();
}

