#include "cbase.h"
#include "props.h"
#include "asw_ammo_drop.h"
#include "asw_ammo_drop_shared.h"
#include "asw_player.h"
#include "asw_marine.h"
#include "asw_weapon.h"
#include "asw_marine_skills.h"
#include "asw_marine_speech.h"
#include "asw_marine_resource.h"
#include "world.h"
#include "asw_util_shared.h"
#include "asw_fx_shared.h"
#include "asw_gamerules.h"
#include "ammodef.h"
#include "asw_weapon_ammo_bag_shared.h"
#include "particle_parse.h"
#include "asw_achievements.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define AMMO_DROP_MODEL "models/items/Ammobag/AmmoBag.mdl"

LINK_ENTITY_TO_CLASS( asw_ammo_drop, CASW_Ammo_Drop );
PRECACHE_WEAPON_REGISTER( asw_ammo_drop );

IMPLEMENT_SERVERCLASS_ST(CASW_Ammo_Drop, DT_ASW_Ammo_Drop)
	SendPropInt(SENDINFO(m_iAmmoUnitsRemaining)),	
END_SEND_TABLE()

BEGIN_DATADESC( CASW_Ammo_Drop )
	DEFINE_FIELD( m_iAmmoUnitsRemaining, FIELD_INTEGER ),
	DEFINE_FIELD( m_bSuppliedAmmo, FIELD_BOOLEAN ),
END_DATADESC()

IMPLEMENT_AUTO_LIST( IAmmoDropAutoList );

CASW_Ammo_Drop::CASW_Ammo_Drop()
{
	m_iAmmoUnitsRemaining = DEFAULT_AMMO_DROP_UNITS;
}

CASW_Ammo_Drop::~CASW_Ammo_Drop()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_Ammo_Drop::Spawn( void )
{
	SetMoveType( MOVETYPE_NONE );

	SetSolid( SOLID_BBOX );
	SetCollisionGroup( ASW_COLLISION_GROUP_PASSABLE );

	Precache();
	SetModel(AMMO_DROP_MODEL);

	BaseClass::Spawn();

	AddEFlags( EFL_NO_DISSOLVE | EFL_NO_MEGAPHYSCANNON_RAGDOLL | EFL_NO_PHYSCANNON_INTERACTION );	

	SetCollisionBounds( Vector(-26,-26,0), Vector(26,26,60));

	m_takedamage = DAMAGE_NO;

	// check for attaching to elevators
	trace_t	tr;
	UTIL_TraceLine( GetAbsOrigin() + Vector(0, 0, 2),
					GetAbsOrigin() - Vector(0, 0, 32), MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );
	if ( tr.fraction < 1.0f && tr.m_pEnt && !tr.m_pEnt->IsWorld() && !tr.m_pEnt->IsNPC() )
	{
		SetParent( tr.m_pEnt );
	}

	m_iAmmoUnitsRemaining = DEFAULT_AMMO_DROP_UNITS;
}

void CASW_Ammo_Drop::PlayDeploySound()
{
	EmitSound("ASW_Ammobag.DropImpact");
}

void CASW_Ammo_Drop::Precache()
{
	PrecacheModel( AMMO_DROP_MODEL );
	PrecacheScriptSound( "ASW_Ammobag.DropImpact" );
	PrecacheScriptSound( "ASW_Ammobag.Pickup_sml" );
	PrecacheScriptSound( "ASW_Ammobag.Pickup_med" );
	PrecacheScriptSound( "ASW_Ammobag.Pickup_lrg" );
	PrecacheScriptSound( "ASW_Ammobag.Fail" );
	PrecacheParticleSystem( "ammo_satchel_take_sml" );
	PrecacheParticleSystem( "ammo_satchel_take_med" );
	PrecacheParticleSystem( "ammo_satchel_take_lrg" );

	BaseClass::Precache();
}


int CASW_Ammo_Drop::ShouldTransmit( const CCheckTransmitInfo *pInfo )
{
	return FL_EDICT_ALWAYS;
}

int CASW_Ammo_Drop::UpdateTransmitState()
{
	return SetTransmitState( FL_EDICT_FULLCHECK );
}

void CASW_Ammo_Drop::ActivateUseIcon( CASW_Marine* pMarine, int nHoldType )
{
	if ( nHoldType == ASW_USE_HOLD_START )
		return;

	CASW_Weapon *pWeapon = GetAmmoUseUnits( pMarine );

	if( pWeapon )
	{
		int iAmmoType = pWeapon->GetPrimaryAmmoType();
		int iGuns = pMarine->GetNumberOfWeaponsUsingAmmo( iAmmoType );
		int iMaxAmmoCount = GetAmmoDef()->MaxCarry( iAmmoType, pMarine ) * iGuns;
		int iBullets = pMarine->GetAmmoCount( iAmmoType );
		int iAmmoCost = GetAmmoUnitCost( iAmmoType );
		int iClipsToGive = CASW_Ammo_Drop_Shared::GetAmmoClipsToGive( iAmmoType );

		pMarine->SetAmmoCount( MIN( iBullets + pWeapon->GetMaxClip1() * iClipsToGive, iMaxAmmoCount ), iAmmoType );
		m_iAmmoUnitsRemaining -= iAmmoCost;

		pMarine->GetMarineSpeech()->Chatter(CHATTER_USE);

		IGameEvent * event = gameeventmanager->CreateEvent( "ammo_pickup" );
		if ( event )
		{
			CASW_Player *pPlayer = pMarine->GetCommander();
			event->SetInt( "userid", ( pPlayer ? pPlayer->GetUserID() : 0 ) );
			event->SetInt( "entindex", pMarine->entindex() );

			gameeventmanager->FireEvent( event );
		}

		if ( m_iAmmoUnitsRemaining <= 0 )
		{
			CTakeDamageInfo info;

			BaseClass::Event_Killed( info );
		}

		CASW_Marine *pDeployer = m_hDeployer.Get();

		if ( pDeployer && pMarine != pDeployer && !m_bSuppliedAmmo )
		{
			m_bSuppliedAmmo = true;
			if ( pDeployer->GetCommander() )
			{
				pDeployer->GetCommander()->AwardAchievement( ACHIEVEMENT_ASW_AMMO_RESUPPLY );
			}
		}
	}
	else
	{
		if ( pMarine->IsInhabited() )
		{
			CASW_Player *pCommander = pMarine->GetCommander();
			if ( pCommander )
			{
				CSingleUserRecipientFilter filter( pCommander );
				EmitSound( filter, pMarine->entindex(), "ASW_Ammobag.Fail", NULL, 0.0f );
			}
		}
	}
}

void CASW_Ammo_Drop::MarineUsing(CASW_Marine* pMarine, float deltatime)
{
}

void CASW_Ammo_Drop::MarineStartedUsing(CASW_Marine* pMarine)
{
}
 
void CASW_Ammo_Drop::MarineStoppedUsing(CASW_Marine* pMarine)
{
}

bool CASW_Ammo_Drop::IsUsable(CBaseEntity *pUser)
{
	return (pUser && pUser->GetAbsOrigin().DistTo(GetAbsOrigin()) < ASW_MARINE_USE_RADIUS);	// near enough?
}

int CASW_Ammo_Drop::GetAmmoUnitCost( int iAmmoType )
{
	return CASW_Ammo_Drop_Shared::GetAmmoUnitCost( iAmmoType );
}

CASW_Weapon* CASW_Ammo_Drop::GetAmmoUseUnits( CASW_Marine *pMarine )
{
	if ( pMarine )
	{
		CASW_Weapon *pWeapon = pMarine->GetActiveASWWeapon();
		if ( !pWeapon || pWeapon->Classify() == CLASS_ASW_AMMO_SATCHEL )
		{
			//pWeapon
			CASW_Weapon *pOtherWeapon = pMarine->GetASWWeapon( 0 );
			if ( pOtherWeapon && pOtherWeapon->Classify() != CLASS_ASW_AMMO_SATCHEL )
			{
				pWeapon = pOtherWeapon;
			}
			else
			{
				pOtherWeapon = pMarine->GetASWWeapon( 1 );
				if ( pOtherWeapon && pOtherWeapon->Classify() != CLASS_ASW_AMMO_SATCHEL )
				{
					pWeapon = pOtherWeapon;
				}
			}
		}

		if ( pWeapon && pWeapon->IsOffensiveWeapon() )
		{
			if ( pWeapon->Classify() == CLASS_ASW_CHAINSAW )
				return NULL;

			int iAmmoType = pWeapon->GetPrimaryAmmoType();
			int iGuns = pMarine->GetNumberOfWeaponsUsingAmmo( iAmmoType );
			int iMaxAmmoCount = GetAmmoDef()->MaxCarry( iAmmoType, pMarine ) * iGuns;
			int iBullets = pMarine->GetAmmoCount( iAmmoType );
			int iAmmoCost = CASW_Ammo_Drop_Shared::GetAmmoUnitCost( iAmmoType );

			if ( ( iBullets < iMaxAmmoCount ) && ( m_iAmmoUnitsRemaining >= iAmmoCost ) )
			{
				return pWeapon;
			}
		}
	}

	return NULL;
}
int CASW_Ammo_Drop::GetWeaponAmmoInUnits( CASW_Marine *pMarine )
{
	if ( pMarine )
	{
		CASW_Weapon *pWeapon = pMarine->GetActiveASWWeapon();

		if ( pWeapon && pWeapon->IsOffensiveWeapon() && ( pWeapon->GetMaxClip1() > 0 ) )
		{
			int iAmmoType = pWeapon->GetPrimaryAmmoType();
			//int iGuns = pMarine->GetNumberOfWeaponsUsingAmmo( iAmmoType );
			int iClipsRemaining = pMarine->GetAmmoCount( iAmmoType ) / pWeapon->GetMaxClip1();
			int iAmmoCost = GetAmmoUnitCost( iAmmoType );

			return iClipsRemaining * iAmmoCost;
		}
		else
		{
			return AMMO_UNITS_MAX;	// no need for ammo if no offensive weapon
		}
	}

	return AMMO_UNITS_MAX;	// no need for ammo if no marine
}

// does first marine need ammo more than second? this allows us to compare regardless of weapon
bool CASW_Ammo_Drop::NeedsAmmoMoreThan( CASW_Marine *pFirstMarine, CASW_Marine *pSecondMarine )
{
	return GetWeaponAmmoInUnits( pFirstMarine ) < GetWeaponAmmoInUnits( pSecondMarine );
}

bool CASW_Ammo_Drop::AllowedToPickup( CASW_Marine *pMarine )
{
	// if the marine can't use it, the use portion is zero
	return ( GetAmmoUseUnits( pMarine ) != NULL );
}
