#include "cbase.h"
#include "asw_ammo_drop_shared.h"
#include "ammodef.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

bool CASW_Ammo_Drop_Shared::ms_bAmmoCostInitiated = false;
int CASW_Ammo_Drop_Shared::ms_iAmmoType[ASW_AMMO_DROP_TYPE_COUNT];
int CASW_Ammo_Drop_Shared::ms_iAmmoUnitCost[ASW_AMMO_DROP_TYPE_COUNT];
int CASW_Ammo_Drop_Shared::ms_iAmmoClips[ASW_AMMO_DROP_TYPE_COUNT];

#define AMMO_COST_INVALID 9999

void CASW_Ammo_Drop_Shared::InitAmmoCosts()
{
	ms_iAmmoType[ASW_AMMO_DROP_TYPE_RIFLE] = GetAmmoDef()->Index("ASW_R");
	ms_iAmmoType[ASW_AMMO_DROP_TYPE_AUTOGUN] = GetAmmoDef()->Index("ASW_AG");
	ms_iAmmoType[ASW_AMMO_DROP_TYPE_SHOTGUN] = GetAmmoDef()->Index("ASW_SG");
	ms_iAmmoType[ASW_AMMO_DROP_TYPE_ASSAULT_SHOTGUN] = GetAmmoDef()->Index("ASW_ASG");
	ms_iAmmoType[ASW_AMMO_DROP_TYPE_FLAMER] = GetAmmoDef()->Index("ASW_F");
	ms_iAmmoType[ASW_AMMO_DROP_TYPE_RAILGUN] = GetAmmoDef()->Index("ASW_RG");
	ms_iAmmoType[ASW_AMMO_DROP_TYPE_PDW] = GetAmmoDef()->Index("ASW_PDW");
	ms_iAmmoType[ASW_AMMO_DROP_TYPE_PISTOL] = GetAmmoDef()->Index("ASW_P");
	ms_iAmmoType[ASW_AMMO_DROP_TYPE_MINING_LASER] = GetAmmoDef()->Index("ASW_ML");
	ms_iAmmoType[ASW_AMMO_DROP_TYPE_TESLA_CANNON] = GetAmmoDef()->Index("ASW_TG");
	ms_iAmmoType[ASW_AMMO_DROP_TYPE_GRENADE_LAUNCHER] = GetAmmoDef()->Index("ASW_GL");
	ms_iAmmoType[ASW_AMMO_DROP_TYPE_SNIPER] = GetAmmoDef()->Index("ASW_SNIPER");

	ms_iAmmoUnitCost[ASW_AMMO_DROP_TYPE_RIFLE] = 20;
	ms_iAmmoUnitCost[ASW_AMMO_DROP_TYPE_AUTOGUN] = 100;
	ms_iAmmoUnitCost[ASW_AMMO_DROP_TYPE_SHOTGUN] = 10;
	ms_iAmmoUnitCost[ASW_AMMO_DROP_TYPE_ASSAULT_SHOTGUN] = 20;
	ms_iAmmoUnitCost[ASW_AMMO_DROP_TYPE_FLAMER] = 20;
	ms_iAmmoUnitCost[ASW_AMMO_DROP_TYPE_RAILGUN] = 10;
	ms_iAmmoUnitCost[ASW_AMMO_DROP_TYPE_PDW] = 20;
	ms_iAmmoUnitCost[ASW_AMMO_DROP_TYPE_PISTOL] = 20;
	ms_iAmmoUnitCost[ASW_AMMO_DROP_TYPE_MINING_LASER] = 20;
	ms_iAmmoUnitCost[ASW_AMMO_DROP_TYPE_TESLA_CANNON] = 20;
	ms_iAmmoUnitCost[ASW_AMMO_DROP_TYPE_GRENADE_LAUNCHER] = 20;
	ms_iAmmoUnitCost[ASW_AMMO_DROP_TYPE_SNIPER] = 20;

	ms_iAmmoClips[ASW_AMMO_DROP_TYPE_RIFLE] = 1;
	ms_iAmmoClips[ASW_AMMO_DROP_TYPE_AUTOGUN] = 1;
	ms_iAmmoClips[ASW_AMMO_DROP_TYPE_SHOTGUN] = 1;
	ms_iAmmoClips[ASW_AMMO_DROP_TYPE_ASSAULT_SHOTGUN] = 1;
	ms_iAmmoClips[ASW_AMMO_DROP_TYPE_FLAMER] = 1;
	ms_iAmmoClips[ASW_AMMO_DROP_TYPE_RAILGUN] = 5;
	ms_iAmmoClips[ASW_AMMO_DROP_TYPE_PDW] = 1;
	ms_iAmmoClips[ASW_AMMO_DROP_TYPE_PISTOL] = 1;
	ms_iAmmoClips[ASW_AMMO_DROP_TYPE_MINING_LASER] = 1;
	ms_iAmmoClips[ASW_AMMO_DROP_TYPE_TESLA_CANNON] = 1;
	ms_iAmmoClips[ASW_AMMO_DROP_TYPE_GRENADE_LAUNCHER] = 3;
	ms_iAmmoClips[ASW_AMMO_DROP_TYPE_SNIPER] = 1;

	ms_bAmmoCostInitiated = true;
}

int CASW_Ammo_Drop_Shared::GetAmmoUnitCost( int iAmmoType )
{
	if ( !ms_bAmmoCostInitiated )
	{
		InitAmmoCosts();
	}

	for ( int i = 0; i < ASW_AMMO_DROP_TYPE_COUNT; i++ )
	{
		if ( iAmmoType == ms_iAmmoType[i] )
		{
			return ms_iAmmoUnitCost[i];
		}
	}

	return AMMO_COST_INVALID;
}

int CASW_Ammo_Drop_Shared::GetAmmoClipsToGive( int iAmmoType )
{
	if ( !ms_bAmmoCostInitiated )
	{
		InitAmmoCosts();
	}

	for ( int i = 0; i < ASW_AMMO_DROP_TYPE_COUNT; i++ )
	{
		if ( iAmmoType == ms_iAmmoType[i] )
		{
			return ms_iAmmoClips[i];
		}
	}
	return 1;
}