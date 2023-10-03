#include "cbase.h"
#include "asw_weapon_sentry_shared.h"
#include "in_buttons.h"

#ifdef CLIENT_DLL
#include "c_asw_player.h"
#include "c_asw_weapon.h"
#include "c_asw_marine.h"
#else
#include "asw_marine.h"
#include "asw_player.h"
#include "asw_weapon.h"
#include "npcevent.h"
#include "asw_sentry_base.h"
#include "func_movelinear.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_NETWORKCLASS_ALIASED( ASW_Weapon_Sentry, DT_ASW_Weapon_Sentry )

BEGIN_NETWORK_TABLE( CASW_Weapon_Sentry, DT_ASW_Weapon_Sentry )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CASW_Weapon_Sentry )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( asw_weapon_sentry, CASW_Weapon_Sentry );
PRECACHE_WEAPON_REGISTER(asw_weapon_sentry);

#ifndef CLIENT_DLL

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CASW_Weapon_Sentry )
	
END_DATADESC()
#endif /* not client */

CASW_Weapon_Sentry::CASW_Weapon_Sentry()
{
	m_fMinRange1	= 0;
	m_fMaxRange1	= 2048;

	m_fMinRange2	= 256;
	m_fMaxRange2	= 1024;

#ifndef CLIENT_DLL
	m_iSentryMunitionType = CASW_Sentry_Base::kAUTOGUN;
	m_nSentryAmmo = CASW_Sentry_Base::GetBaseAmmoForGunType( (CASW_Sentry_Base::GunType_t) m_iSentryMunitionType );
#else
	m_flNextDeployCheckThink = 0;
	m_bDisplayActive = false;
#endif
	m_bDisplayValid = true;
}


CASW_Weapon_Sentry::~CASW_Weapon_Sentry()
{

}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Activity
//-----------------------------------------------------------------------------
Activity CASW_Weapon_Sentry::GetPrimaryAttackActivity( void )
{
	return ACT_VM_PRIMARYATTACK;
}

void CASW_Weapon_Sentry::PrimaryAttack( void )
{
	DeploySentry();
}

bool CASW_Weapon_Sentry::OffhandActivate()
{
	if (!GetMarine() || GetMarine()->GetFlags() & FL_FROZEN)	// don't allow this if the marine is frozen
		return false;
	PrimaryAttack();

	return true;
}

#ifdef CLIENT_DLL
void CASW_Weapon_Sentry::OnDataChanged( DataUpdateType_t type )
{	
	BaseClass::OnDataChanged( type );

	if ( type == DATA_UPDATE_CREATED)
	{
		SetNextClientThink(CLIENT_THINK_ALWAYS);
	}

	CASW_Marine *pMarine = GetMarine();
	if ( pMarine )
	{
		bool bSentryActive = ( pMarine->GetActiveASWWeapon() == this );
		if ( bSentryActive && m_bDisplayActive == false )
		{
			m_hOwningMarine = pMarine;
			pMarine->CreateSentryBuildDisplay();
			m_bDisplayActive = true;
		}
		else if ( !bSentryActive && m_bDisplayActive == true )
		{	
			pMarine->DestroySentryBuildDisplay();
			m_bDisplayActive = false;
		}
	}	
}

void CASW_Weapon_Sentry::UpdateOnRemove()
{
	BaseClass::UpdateOnRemove();

	DestroySentryBuildDisplay( static_cast<CASW_Marine*>(m_hOwningMarine.Get()) );
	m_bDisplayActive = false;
}

void CASW_Weapon_Sentry::ClientThink( void )
{
	BaseClass::ClientThink();

	CASW_Marine *pMarine;
	if ( m_hOwningMarine.Get() )
	{
		// this means it's been removed or dropped, so destroy the display
		if ( !GetMarine() )
		{
			DestroySentryBuildDisplay( static_cast<CASW_Marine*>(m_hOwningMarine.Get()) );
			m_bDisplayActive = false;
			m_hOwningMarine = NULL;
			return;
		}

		pMarine = static_cast<CASW_Marine*>(m_hOwningMarine.Get());
	}
	else
		pMarine = GetMarine();

	if ( pMarine )
	{
		bool bSentryActive = ( pMarine->GetActiveASWWeapon() == this );
		if ( bSentryActive && m_flNextDeployCheckThink < gpGlobals->curtime )
		{
			CASW_Marine *pMarine = GetMarine();
			if (pMarine && pMarine->GetActiveASWWeapon() == this )
			{
				m_bDisplayValid = FindValidSentrySpot();
				pMarine->SetSentryBuildDisplayEnabled( m_bDisplayValid );
			}

			m_flNextDeployCheckThink = gpGlobals->curtime + 0.2;
		}
	}
}

void CASW_Weapon_Sentry::DestroySentryBuildDisplay( CASW_Marine *pMarine )
{
	if ( pMarine )
	{
		pMarine->DestroySentryBuildDisplay();
	}
}

#endif //CLIENT_DLL

void CASW_Weapon_Sentry::Drop( const Vector &vecVelocity )
{
#ifdef CLIENT_DLL
	DestroySentryBuildDisplay( static_cast<CASW_Marine*>(m_hOwningMarine.Get()) );
	m_bDisplayActive = false;
#endif

	BaseClass::Drop( vecVelocity );
}

bool CASW_Weapon_Sentry::Holster( CBaseCombatWeapon *pSwitchingTo )
{
#ifdef CLIENT_DLL
	DestroySentryBuildDisplay( static_cast<CASW_Marine*>(m_hOwningMarine.Get()) );
	m_bDisplayActive = false;
#endif

	return BaseClass::Holster( pSwitchingTo );
}

bool CASW_Weapon_Sentry::FindValidSentrySpot()
{
	CASW_Marine *pMarine = GetMarine();
	if ( !pMarine )
		return false;

	//QAngle ang = pMarine->ASWEyeAngles();
	QAngle ang = pMarine->GetAbsAngles();
	ang.x = 0;
	matrix3x4_t matrix;
	AngleMatrix( ang, matrix );
	Vector vecSpawnPos;
	VectorTransform(Vector(70, 0, 0), matrix, vecSpawnPos);

	// check we have room for the sentry
	vecSpawnPos += pMarine->GetAbsOrigin();
#ifndef CLIENT_DLL
	if ( !pMarine->IsInhabited() && vecSpawnPos.DistTo( pMarine->m_vecOffhandItemSpot ) < 150.0f )
	{
		vecSpawnPos.x = pMarine->m_vecOffhandItemSpot.x;
		vecSpawnPos.y = pMarine->m_vecOffhandItemSpot.y;
	}
#endif

	Vector vecSentryMins = Vector(-26,-26,0);
	Vector vecSentryMaxs = Vector(26,26,60);

	trace_t tr;
	UTIL_TraceHull( vecSpawnPos + Vector(0, 0, 50),
		vecSpawnPos + Vector( 0, 0, -50 ),
		vecSentryMins,
		vecSentryMaxs,
		MASK_SOLID,
		pMarine,
		COLLISION_GROUP_NONE,
		&tr );

#ifdef GAME_DLL
	CFuncMoveLinear *pElevator = NULL;
#endif

	// so we don't put the sentry on something that might move/disappear
	if ( tr.DidHitNonWorldEntity() )
	{
#ifdef GAME_DLL		// todo: allow clients to predict spawning on top of elevator too?
		pElevator = dynamic_cast<CFuncMoveLinear*>(tr.m_pEnt);
		if ( !pElevator )
#endif
			return false;
	}

	if ( tr.startsolid || tr.allsolid || ( tr.DidHit() && tr.fraction <= 0 ) || tr.fraction >= 1.0f ) // if there's something in the way, or no floor, don't deploy
		return false;

	vecSpawnPos = tr.endpos;

	// check the feet have ground to stand on
	trace_t trace;
	Vector vecTest = vecSpawnPos + Vector(0,9,5);
	Vector vecTestMins = Vector(-3, -3, -3);
	Vector vecTestMaxs = Vector(3, 3, 3);
	UTIL_TraceHull( vecTest, vecTest - Vector(0,0,20), vecTestMins, vecTestMaxs, MASK_SOLID, pMarine, COLLISION_GROUP_NONE, &trace );
	if (trace.fraction == 1.0)
		return false;

	vecTest = vecSpawnPos + Vector(0,-9,6);
	UTIL_TraceHull( vecTest, vecTest - Vector(0,0,20), vecTestMins, vecTestMaxs, MASK_SOLID, pMarine, COLLISION_GROUP_NONE, &trace );
	if (trace.fraction == 1.0)
		return false;

	vecTest = vecSpawnPos + Vector(9,0,6);
	UTIL_TraceHull( vecTest, vecTest - Vector(0,0,20), vecTestMins, vecTestMaxs, MASK_SOLID, pMarine, COLLISION_GROUP_NONE, &trace );
	if (trace.fraction == 1.0)
		return false;

	vecTest = vecSpawnPos + Vector(-9,0,6);
	UTIL_TraceHull( vecTest, vecTest - Vector(0,0,20), vecTestMins, vecTestMaxs, MASK_SOLID, pMarine, COLLISION_GROUP_NONE, &trace );
	if (trace.fraction == 1.0)
		return false;

#ifdef GAME_DLL
	m_vecValidSentrySpot = vecSpawnPos;
	m_angValidSentryFacing = ang;
	m_hValidSentryParent = pElevator;
#endif

	return true;
}

void CASW_Weapon_Sentry::DeploySentry()
{
	CASW_Marine *pMarine = GetMarine();
	if ( !pMarine )		// firing from a marine
		return;

	if ( !FindValidSentrySpot() )
		return;

	// MUST call sound before removing a round from the clip of a CMachineGun
	WeaponSound(SINGLE);

	// sets the animation on the marine holding this weapon
	bool bSentryActive = (pMarine->GetActiveASWWeapon() == this);

#ifndef CLIENT_DLL
	CASW_Sentry_Base* pBase = (CASW_Sentry_Base *)CreateEntityByName( "asw_sentry_base" );	
	
    pBase->SetAbsAngles( m_angValidSentryFacing );
    pBase->m_hDeployer = pMarine;
    pBase->SetGunType( m_iSentryMunitionType );
	pBase->SetAmmo( m_nSentryAmmo );

    UTIL_SetOrigin( pBase, m_vecValidSentrySpot );
    pBase->SetAbsVelocity( vec3_origin );
    pBase->Spawn();
    pBase->PlayDeploySound();

	IGameEvent * event = gameeventmanager->CreateEvent( "sentry_placed" );
	if ( event )
	{
		CBasePlayer *pPlayer = pMarine->GetCommander();
		event->SetInt( "userid", ( pPlayer ? pPlayer->GetUserID() : 0 ) );

		gameeventmanager->FireEvent( event );
	}

	if ( m_hValidSentryParent.Get() )
	{
		pBase->SetParent( m_hValidSentryParent );
	}

	// auto start setting it up
	pBase->ActivateUseIcon( pMarine, ASW_USE_RELEASE_QUICK );

	pMarine->Weapon_Detach(this);
	pMarine->OnWeaponFired( this, 1 );
	Kill();
#else
	pMarine->DestroySentryBuildDisplay();
#endif				
	if (bSentryActive)
		pMarine->SwitchToNextBestWeapon(NULL);
}

void CASW_Weapon_Sentry::Precache()
{
	BaseClass::Precache();
}

//============================================

IMPLEMENT_NETWORKCLASS_ALIASED( ASW_Weapon_Sentry_Flamer, DT_ASW_Weapon_Sentry_Flamer )

BEGIN_NETWORK_TABLE( CASW_Weapon_Sentry_Flamer, DT_ASW_Weapon_Sentry_Flamer )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CASW_Weapon_Sentry_Flamer )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( asw_weapon_sentry_flamer, CASW_Weapon_Sentry_Flamer );
PRECACHE_WEAPON_REGISTER( asw_weapon_sentry_flamer );

CASW_Weapon_Sentry_Flamer::CASW_Weapon_Sentry_Flamer()
{
#ifndef CLIENT_DLL
	m_iSentryMunitionType = CASW_Sentry_Base::kFLAME;
	m_nSentryAmmo = CASW_Sentry_Base::GetBaseAmmoForGunType( (CASW_Sentry_Base::GunType_t) m_iSentryMunitionType );
#endif
}

//============================================

IMPLEMENT_NETWORKCLASS_ALIASED( ASW_Weapon_Sentry_Cannon, DT_ASW_Weapon_Sentry_Cannon )

BEGIN_NETWORK_TABLE( CASW_Weapon_Sentry_Cannon, DT_ASW_Weapon_Sentry_Cannon )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CASW_Weapon_Sentry_Cannon )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( asw_weapon_sentry_cannon, CASW_Weapon_Sentry_Cannon );
PRECACHE_WEAPON_REGISTER( asw_weapon_sentry_cannon );

CASW_Weapon_Sentry_Cannon::CASW_Weapon_Sentry_Cannon()
{
#ifndef CLIENT_DLL
	m_iSentryMunitionType = CASW_Sentry_Base::kCANNON;
	m_nSentryAmmo = CASW_Sentry_Base::GetBaseAmmoForGunType( (CASW_Sentry_Base::GunType_t) m_iSentryMunitionType );
#endif
}

//============================================

IMPLEMENT_NETWORKCLASS_ALIASED( ASW_Weapon_Sentry_Freeze, DT_ASW_Weapon_Sentry_Freeze )

BEGIN_NETWORK_TABLE( CASW_Weapon_Sentry_Freeze, DT_ASW_Weapon_Sentry_Freeze )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CASW_Weapon_Sentry_Freeze )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( asw_weapon_sentry_freeze, CASW_Weapon_Sentry_Freeze );
PRECACHE_WEAPON_REGISTER( asw_weapon_sentry_freeze );

CASW_Weapon_Sentry_Freeze::CASW_Weapon_Sentry_Freeze()
{
#ifndef CLIENT_DLL
	m_iSentryMunitionType = CASW_Sentry_Base::kICE;
	m_nSentryAmmo = CASW_Sentry_Base::GetBaseAmmoForGunType( (CASW_Sentry_Base::GunType_t) m_iSentryMunitionType );
#endif
}
