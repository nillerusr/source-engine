#include "cbase.h"
#include "asw_item_crate.h"
#include "asw_generic_emitter_entity.h"
#include "asw_radiation_volume.h"
#include "asw_marine.h"
#include "soundenvelope.h"
#include "item_creation.h"
#include "asw_weapon.h"
#include "ammodef.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar asw_random_weapons;

#define ASW_ITEM_CRATE_MODEL "models/swarm/crates/asw_wood_crate_big.mdl"

LINK_ENTITY_TO_CLASS( asw_item_crate, CASW_Item_Crate );
PRECACHE_REGISTER( asw_item_crate );

BEGIN_DATADESC( CASW_Item_Crate )	

END_DATADESC()

CASW_Item_Crate::CASW_Item_Crate()
{
}

CASW_Item_Crate::~CASW_Item_Crate()
{
}

void CASW_Item_Crate::Spawn()
{
	DisableAutoFade();
	SetModelName( AllocPooledString( ASW_ITEM_CRATE_MODEL ) );
	Precache();
	SetModel( ASW_ITEM_CRATE_MODEL );
	AddSpawnFlags(SF_PHYSPROP_AIMTARGET);
	BaseClass::Spawn();
	m_nSkin = 2;
	SetHealth(30);
	SetMaxHealth(30);
	m_takedamage = DAMAGE_YES;
}

void CASW_Item_Crate::Precache()
{
	PrecacheModel( ASW_ITEM_CRATE_MODEL );
	SetModel( ASW_ITEM_CRATE_MODEL );

	BaseClass::Precache();
}

int CASW_Item_Crate::OnTakeDamage( const CTakeDamageInfo &info )
{
	if (info.GetAttacker() && info.GetAttacker()->Classify() == CLASS_PLAYER_ALLY_VITAL)
	{
		CASW_Marine* pMarine = dynamic_cast<CASW_Marine*>(info.GetAttacker());
		if (pMarine)
			pMarine->HurtJunkItem(this, info);
	}
		
	int iResult = BaseClass::OnTakeDamage(info);

	return iResult;
}

void CASW_Item_Crate::Event_Killed( const CTakeDamageInfo &info )
{
	BaseClass::Event_Killed( info );

	randomitemcriteria_t pCriteria;
	pCriteria.iItemLevel = 1;
	pCriteria.pszItemName = NULL; // weapon class
	pCriteria.iItemQuality = AE_NORMAL;
	float fQuality = RandomFloat();
	if ( fQuality < 0.25f )
	{
		pCriteria.iItemQuality = AE_RARE;
	}
	else if ( fQuality < 0.5f )
	{
		pCriteria.iItemQuality = AE_COMMON;
	}
	pCriteria.vecAbsOrigin = GetAbsOrigin() + Vector(0, 0, 20);
	pCriteria.vecAbsAngles = GetAbsAngles();
	CASW_Weapon* pWeapon = dynamic_cast<CASW_Weapon*>( ItemGeneration()->GenerateRandomItem( &pCriteria ) );
	if ( pWeapon )
	{
		// fill it with ammo
		int iPrimaryAmmo = pWeapon->GetDefaultClip1();	
		int iSecondaryAmmo = pWeapon->GetDefaultClip2();

		pWeapon->SetClip1( iPrimaryAmmo );
		pWeapon->SetClip2( iSecondaryAmmo );

		if ( pWeapon->GetPrimaryAmmoType()!=-1 )
		{
			int iBullets = GetAmmoDef()->MaxCarry( pWeapon->GetPrimaryAmmoType() );
			pWeapon->SetPrimaryAmmoCount( iBullets - iPrimaryAmmo );
		}
	}
}
