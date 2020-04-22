//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "tfc_shareddefs.h"


const char *g_AmmoTypeNames[] =
{
	"DUMMY AMMO",
	TFC_AMMO_SHELLS_NAME,
	TFC_AMMO_NAILS_NAME,
	TFC_AMMO_ROCKETS_NAME,
	TFC_AMMO_CELLS_NAME,
	TFC_AMMO_MEDIKIT_NAME,
	TFC_AMMO_DETPACK_NAME,
	TFC_AMMO_GRENADES1_NAME,
	TFC_AMMO_GRENADES2_NAME
};


int g_nMaxGrenades[NUM_GRENADE_TYPES] =
{
	0,	// GR_TYPE_NONE
	4,	// GR_TYPE_NORMAL
	3,	// GR_TYPE_CONCUSSION
	2,	// GR_TYPE_NAIL
	2,	// GR_TYPE_MIRV
	2,	// GR_TYPE_NAPALM
	2,	// GR_TYPE_GAS
	4,	// GR_TYPE_EMP
	3,	// GR_TYPE_CALTROP
};


CTFCPlayerClassInfo g_TFCPlayerClassInfo[PC_LASTCLASS];


// Note: this could go in a script, but it's not much trouble to have it all here.
class CClassInfoInitializer
{
public:

	CClassInfoInitializer()
	{
		memset( &g_TFCPlayerClassInfo, 0xFE, sizeof( g_TFCPlayerClassInfo ) );
		
		// PC_UNDEFINED.
			CTFCPlayerClassInfo *pInfo = &g_TFCPlayerClassInfo[PC_UNDEFINED];
			pInfo->m_pClassName = "undefined";
			pInfo->m_pModelName = "models/player/civilian.mdl";
			pInfo->m_flMaxSpeed = 1;
			memset( pInfo->m_MaxAmmo, 0, sizeof( pInfo->m_MaxAmmo ) );

		// PC_SCOUT.
			pInfo = &g_TFCPlayerClassInfo[PC_SCOUT];
			pInfo->m_pClassName = "scout";
			pInfo->m_pModelName = "models/player/scout.mdl";
			pInfo->m_flMaxSpeed = 400;
			pInfo->m_iMaxHealth = 75;
			
			pInfo->m_iInitArmor = 25;
			pInfo->m_iMaxArmor = 50;
			
			pInfo->m_flInitArmorType = 0.3;
			pInfo->m_flMaxArmorType = 0.3;
			pInfo->m_nArmorClasses = 3;
			pInfo->m_iInitArmorClass = 0;

			pInfo->m_iGrenadeType1 = GR_TYPE_CALTROP;
			pInfo->m_iGrenadeType2 = GR_TYPE_CONCUSSION;

			pInfo->m_InitAmmo[TFC_AMMO_SHELLS] = 25;
			pInfo->m_InitAmmo[TFC_AMMO_NAILS] = 100;
			pInfo->m_InitAmmo[TFC_AMMO_ROCKETS] = 0;
			pInfo->m_InitAmmo[TFC_AMMO_CELLS] = 50;
			pInfo->m_InitAmmo[TFC_AMMO_MEDIKIT] = 0;
			pInfo->m_InitAmmo[TFC_AMMO_DETPACK] = 0;
			pInfo->m_InitAmmo[TFC_AMMO_GRENADES1] = 2;
			pInfo->m_InitAmmo[TFC_AMMO_GRENADES2] = 3;

			pInfo->m_MaxAmmo[TFC_AMMO_SHELLS] = 50;
			pInfo->m_MaxAmmo[TFC_AMMO_NAILS] = 200;
			pInfo->m_MaxAmmo[TFC_AMMO_ROCKETS] = 25;
			pInfo->m_MaxAmmo[TFC_AMMO_CELLS] = 100;
			pInfo->m_MaxAmmo[TFC_AMMO_MEDIKIT] = 0;
			pInfo->m_MaxAmmo[TFC_AMMO_DETPACK] = 0;
			pInfo->m_MaxAmmo[TFC_AMMO_GRENADES1] = 4;
			pInfo->m_MaxAmmo[TFC_AMMO_GRENADES2] = 4;

		// PC_SNIPER.
			pInfo = &g_TFCPlayerClassInfo[PC_SNIPER];
			pInfo->m_pClassName = "sniper";
			pInfo->m_pModelName = "models/player/sniper.mdl";
			pInfo->m_flMaxSpeed = 300;
			pInfo->m_iMaxHealth = 90;
			
			pInfo->m_iInitArmor = 25;
			pInfo->m_iMaxArmor = 50;
			
			pInfo->m_flInitArmorType = 0.3;
			pInfo->m_flMaxArmorType = 0.3;
			pInfo->m_nArmorClasses = 3;
			pInfo->m_iInitArmorClass = 0;

			pInfo->m_iGrenadeType1 = GR_TYPE_NORMAL;
			pInfo->m_iGrenadeType2 = GR_TYPE_NONE;

			pInfo->m_InitAmmo[TFC_AMMO_SHELLS] = 60;
			pInfo->m_InitAmmo[TFC_AMMO_NAILS] = 50;
			pInfo->m_InitAmmo[TFC_AMMO_ROCKETS] = 0;
			pInfo->m_InitAmmo[TFC_AMMO_CELLS] = 0;
			pInfo->m_InitAmmo[TFC_AMMO_MEDIKIT] = 0;
			pInfo->m_InitAmmo[TFC_AMMO_DETPACK] = 0;
			pInfo->m_InitAmmo[TFC_AMMO_GRENADES1] = 2;
			pInfo->m_InitAmmo[TFC_AMMO_GRENADES2] = 0;

			pInfo->m_MaxAmmo[TFC_AMMO_SHELLS] = 75;
			pInfo->m_MaxAmmo[TFC_AMMO_NAILS] = 100;
			pInfo->m_MaxAmmo[TFC_AMMO_ROCKETS] = 25;
			pInfo->m_MaxAmmo[TFC_AMMO_CELLS] = 50;
			pInfo->m_MaxAmmo[TFC_AMMO_MEDIKIT] = 0;
			pInfo->m_MaxAmmo[TFC_AMMO_DETPACK] = 0;
			pInfo->m_MaxAmmo[TFC_AMMO_GRENADES1] = 4;
			pInfo->m_MaxAmmo[TFC_AMMO_GRENADES2] = 4;

		// PC_SOLDIER.
			pInfo = &g_TFCPlayerClassInfo[PC_SOLDIER];
			pInfo->m_pClassName = "soldier";
			pInfo->m_pModelName = "models/player/soldier.mdl";
			pInfo->m_flMaxSpeed = 240;
			pInfo->m_iMaxHealth = 100;
			
			pInfo->m_iInitArmor = 100;
			pInfo->m_iMaxArmor = 200;
			
			pInfo->m_flInitArmorType = 0.8;
			pInfo->m_flMaxArmorType = 0.8;
			pInfo->m_nArmorClasses = 31;
			pInfo->m_iInitArmorClass = 0;

			pInfo->m_iGrenadeType1 = GR_TYPE_NORMAL;
			pInfo->m_iGrenadeType2 = GR_TYPE_NAIL;

			pInfo->m_InitAmmo[TFC_AMMO_SHELLS] = 50;
			pInfo->m_InitAmmo[TFC_AMMO_NAILS] = 0;
			pInfo->m_InitAmmo[TFC_AMMO_ROCKETS] = 10;
			pInfo->m_InitAmmo[TFC_AMMO_CELLS] = 0;
			pInfo->m_InitAmmo[TFC_AMMO_MEDIKIT] = 0;
			pInfo->m_InitAmmo[TFC_AMMO_DETPACK] = 0;
			pInfo->m_InitAmmo[TFC_AMMO_GRENADES1] = 2;
			pInfo->m_InitAmmo[TFC_AMMO_GRENADES2] = 1;

			pInfo->m_MaxAmmo[TFC_AMMO_SHELLS] = 100;
			pInfo->m_MaxAmmo[TFC_AMMO_NAILS] = 100;
			pInfo->m_MaxAmmo[TFC_AMMO_ROCKETS] = 50;
			pInfo->m_MaxAmmo[TFC_AMMO_CELLS] = 50;
			pInfo->m_MaxAmmo[TFC_AMMO_MEDIKIT] = 0;
			pInfo->m_MaxAmmo[TFC_AMMO_DETPACK] = 0;
			pInfo->m_MaxAmmo[TFC_AMMO_GRENADES1] = 4;
			pInfo->m_MaxAmmo[TFC_AMMO_GRENADES2] = 4;


		// PC_DEMOMAN.
			pInfo = &g_TFCPlayerClassInfo[PC_DEMOMAN];
			pInfo->m_pClassName = "demoman";
			pInfo->m_pModelName = "models/player/demo.mdl";
			pInfo->m_flMaxSpeed = 280;
			pInfo->m_iMaxHealth = 90;
			
			pInfo->m_iInitArmor = 50;
			pInfo->m_iMaxArmor = 120;
			
			pInfo->m_flInitArmorType = 0.6;
			pInfo->m_flMaxArmorType = 0.6;
			pInfo->m_nArmorClasses = 31;
			pInfo->m_iInitArmorClass = 0;

			pInfo->m_iGrenadeType1 = GR_TYPE_NORMAL;
			pInfo->m_iGrenadeType2 = GR_TYPE_MIRV;

			pInfo->m_InitAmmo[TFC_AMMO_SHELLS] = 30;
			pInfo->m_InitAmmo[TFC_AMMO_NAILS] = 0;
			pInfo->m_InitAmmo[TFC_AMMO_ROCKETS] = 20;
			pInfo->m_InitAmmo[TFC_AMMO_CELLS] = 0;
			pInfo->m_InitAmmo[TFC_AMMO_MEDIKIT] = 0;
			pInfo->m_InitAmmo[TFC_AMMO_DETPACK] = 0;
			pInfo->m_InitAmmo[TFC_AMMO_GRENADES1] = 2;
			pInfo->m_InitAmmo[TFC_AMMO_GRENADES2] = 2;

			pInfo->m_MaxAmmo[TFC_AMMO_SHELLS] = 75;
			pInfo->m_MaxAmmo[TFC_AMMO_NAILS] = 50;
			pInfo->m_MaxAmmo[TFC_AMMO_ROCKETS] = 50;
			pInfo->m_MaxAmmo[TFC_AMMO_CELLS] = 50;
			pInfo->m_MaxAmmo[TFC_AMMO_MEDIKIT] = 0;
			pInfo->m_MaxAmmo[TFC_AMMO_DETPACK] = 0;
			pInfo->m_MaxAmmo[TFC_AMMO_GRENADES1] = 4;
			pInfo->m_MaxAmmo[TFC_AMMO_GRENADES2] = 4;


		// PC_MEDIC.
			pInfo = &g_TFCPlayerClassInfo[PC_MEDIC];
			pInfo->m_pClassName = "medic";
			pInfo->m_pModelName = "models/player/medic.mdl";
			pInfo->m_flMaxSpeed = 320;
			pInfo->m_iMaxHealth = 90;
			
			pInfo->m_iInitArmor = 50;
			pInfo->m_iMaxArmor = 100;
			
			pInfo->m_flInitArmorType = 0.3;
			pInfo->m_flMaxArmorType = 0.6;
			pInfo->m_nArmorClasses = 11;
			pInfo->m_iInitArmorClass = 0;

			pInfo->m_iGrenadeType1 = GR_TYPE_NORMAL;
			pInfo->m_iGrenadeType2 = GR_TYPE_CONCUSSION;

			pInfo->m_InitAmmo[TFC_AMMO_SHELLS] = 50;
			pInfo->m_InitAmmo[TFC_AMMO_NAILS] = 50;
			pInfo->m_InitAmmo[TFC_AMMO_ROCKETS] = 0;
			pInfo->m_InitAmmo[TFC_AMMO_CELLS] = 0;
			pInfo->m_InitAmmo[TFC_AMMO_MEDIKIT] = 50;
			pInfo->m_InitAmmo[TFC_AMMO_DETPACK] = 0;
			pInfo->m_InitAmmo[TFC_AMMO_GRENADES1] = 2;
			pInfo->m_InitAmmo[TFC_AMMO_GRENADES2] = 2;

			pInfo->m_MaxAmmo[TFC_AMMO_SHELLS] = 75;
			pInfo->m_MaxAmmo[TFC_AMMO_NAILS] = 150;
			pInfo->m_MaxAmmo[TFC_AMMO_ROCKETS] = 25;
			pInfo->m_MaxAmmo[TFC_AMMO_CELLS] = 50;
			pInfo->m_MaxAmmo[TFC_AMMO_MEDIKIT] = 50;
			pInfo->m_MaxAmmo[TFC_AMMO_DETPACK] = 0;
			pInfo->m_MaxAmmo[TFC_AMMO_GRENADES1] = 4;
			pInfo->m_MaxAmmo[TFC_AMMO_GRENADES2] = 4;


		// PC_HWGUY.
			pInfo = &g_TFCPlayerClassInfo[PC_HWGUY];
			pInfo->m_pClassName = "hwguy";
			pInfo->m_pModelName = "models/player/hvyweapon.mdl";
			pInfo->m_flMaxSpeed = 230;
			pInfo->m_iMaxHealth = 100;
			
			pInfo->m_iInitArmor = 150;
			pInfo->m_iMaxArmor = 300;
			
			pInfo->m_flInitArmorType = 0.8;
			pInfo->m_flMaxArmorType = 0.8;
			pInfo->m_nArmorClasses = 31;
			pInfo->m_iInitArmorClass = 0;

			pInfo->m_iGrenadeType1 = GR_TYPE_NORMAL;
			pInfo->m_iGrenadeType2 = GR_TYPE_MIRV;

			pInfo->m_InitAmmo[TFC_AMMO_SHELLS] = 200;
			pInfo->m_InitAmmo[TFC_AMMO_NAILS] = 0;
			pInfo->m_InitAmmo[TFC_AMMO_ROCKETS] = 0;
			pInfo->m_InitAmmo[TFC_AMMO_CELLS] = 30;
			pInfo->m_InitAmmo[TFC_AMMO_MEDIKIT] = 0;
			pInfo->m_InitAmmo[TFC_AMMO_DETPACK] = 0;
			pInfo->m_InitAmmo[TFC_AMMO_GRENADES1] = 2;
			pInfo->m_InitAmmo[TFC_AMMO_GRENADES2] = 1;

			pInfo->m_MaxAmmo[TFC_AMMO_SHELLS] = 200;
			pInfo->m_MaxAmmo[TFC_AMMO_NAILS] = 200;
			pInfo->m_MaxAmmo[TFC_AMMO_ROCKETS] = 25;
			pInfo->m_MaxAmmo[TFC_AMMO_CELLS] = 50;
			pInfo->m_MaxAmmo[TFC_AMMO_MEDIKIT] = 0;
			pInfo->m_MaxAmmo[TFC_AMMO_DETPACK] = 0;
			pInfo->m_MaxAmmo[TFC_AMMO_GRENADES1] = 4;
			pInfo->m_MaxAmmo[TFC_AMMO_GRENADES2] = 4;


		// PC_PYRO.
			pInfo = &g_TFCPlayerClassInfo[PC_PYRO];
			pInfo->m_pClassName = "pyro";
			pInfo->m_pModelName = "models/player/pyro.mdl";
			pInfo->m_flMaxSpeed = 300;
			pInfo->m_iMaxHealth = 100;
			
			pInfo->m_iInitArmor = 50;
			pInfo->m_iMaxArmor = 150;
			
			pInfo->m_flInitArmorType = 0.6;
			pInfo->m_flMaxArmorType = 0.6;
			pInfo->m_nArmorClasses = 27;
			pInfo->m_iInitArmorClass = 0;

			pInfo->m_iGrenadeType1 = GR_TYPE_NORMAL;
			pInfo->m_iGrenadeType2 = GR_TYPE_NAPALM;

			pInfo->m_InitAmmo[TFC_AMMO_SHELLS] = 20;
			pInfo->m_InitAmmo[TFC_AMMO_NAILS] = 0;
			pInfo->m_InitAmmo[TFC_AMMO_ROCKETS] = 5;
			pInfo->m_InitAmmo[TFC_AMMO_CELLS] = 120;
			pInfo->m_InitAmmo[TFC_AMMO_MEDIKIT] = 0;
			pInfo->m_InitAmmo[TFC_AMMO_DETPACK] = 0;
			pInfo->m_InitAmmo[TFC_AMMO_GRENADES1] = 2;
			pInfo->m_InitAmmo[TFC_AMMO_GRENADES2] = 4;

			pInfo->m_MaxAmmo[TFC_AMMO_SHELLS] = 40;
			pInfo->m_MaxAmmo[TFC_AMMO_NAILS] = 50;
			pInfo->m_MaxAmmo[TFC_AMMO_ROCKETS] = 20;
			pInfo->m_MaxAmmo[TFC_AMMO_CELLS] = 200;
			pInfo->m_MaxAmmo[TFC_AMMO_MEDIKIT] = 0;
			pInfo->m_MaxAmmo[TFC_AMMO_DETPACK] = 0;
			pInfo->m_MaxAmmo[TFC_AMMO_GRENADES1] = 4;
			pInfo->m_MaxAmmo[TFC_AMMO_GRENADES2] = 4;


		// PC_SPY.
			pInfo = &g_TFCPlayerClassInfo[PC_SPY];
			pInfo->m_pClassName = "spy";
			pInfo->m_pModelName = "models/player/spy.mdl";
			pInfo->m_flMaxSpeed = 300;
			pInfo->m_iMaxHealth = 90;
			
			pInfo->m_iInitArmor = 25;
			pInfo->m_iMaxArmor = 100;
			
			pInfo->m_flInitArmorType = 0.6;
			pInfo->m_flMaxArmorType = 0.6;
			pInfo->m_nArmorClasses = 27;
			pInfo->m_iInitArmorClass = 0;

			pInfo->m_iGrenadeType1 = GR_TYPE_NORMAL;
			pInfo->m_iGrenadeType2 = GR_TYPE_GAS;

			pInfo->m_InitAmmo[TFC_AMMO_SHELLS] = 40;
			pInfo->m_InitAmmo[TFC_AMMO_NAILS] = 50;
			pInfo->m_InitAmmo[TFC_AMMO_ROCKETS] = 0;
			pInfo->m_InitAmmo[TFC_AMMO_CELLS] = 10;
			pInfo->m_InitAmmo[TFC_AMMO_MEDIKIT] = 0;
			pInfo->m_InitAmmo[TFC_AMMO_DETPACK] = 0;
			pInfo->m_InitAmmo[TFC_AMMO_GRENADES1] = 2;
			pInfo->m_InitAmmo[TFC_AMMO_GRENADES2] = 2;

			pInfo->m_MaxAmmo[TFC_AMMO_SHELLS] = 40;
			pInfo->m_MaxAmmo[TFC_AMMO_NAILS] = 100;
			pInfo->m_MaxAmmo[TFC_AMMO_ROCKETS] = 15;
			pInfo->m_MaxAmmo[TFC_AMMO_CELLS] = 30;
			pInfo->m_MaxAmmo[TFC_AMMO_MEDIKIT] = 0;
			pInfo->m_MaxAmmo[TFC_AMMO_DETPACK] = 0;
			pInfo->m_MaxAmmo[TFC_AMMO_GRENADES1] = 4;
			pInfo->m_MaxAmmo[TFC_AMMO_GRENADES2] = 4;


		// PC_ENGINEER.
			pInfo = &g_TFCPlayerClassInfo[PC_ENGINEER];
			pInfo->m_pClassName = "engineer";
			pInfo->m_pModelName = "models/player/engineer.mdl";
			pInfo->m_flMaxSpeed = 300;
			pInfo->m_iMaxHealth = 80;
			
			pInfo->m_iInitArmor = 25;
			pInfo->m_iMaxArmor = 50;
			
			pInfo->m_flInitArmorType = 0.3;
			pInfo->m_flMaxArmorType = 0.6;
			pInfo->m_nArmorClasses = 31;
			pInfo->m_iInitArmorClass = 0;

			pInfo->m_iGrenadeType1 = GR_TYPE_NORMAL;
			pInfo->m_iGrenadeType2 = GR_TYPE_EMP;

			pInfo->m_InitAmmo[TFC_AMMO_SHELLS] = 20;
			pInfo->m_InitAmmo[TFC_AMMO_NAILS] = 25;
			pInfo->m_InitAmmo[TFC_AMMO_ROCKETS] = 0;
			pInfo->m_InitAmmo[TFC_AMMO_CELLS] = 100;
			pInfo->m_InitAmmo[TFC_AMMO_MEDIKIT] = 0;
			pInfo->m_InitAmmo[TFC_AMMO_DETPACK] = 0;
			pInfo->m_InitAmmo[TFC_AMMO_GRENADES1] = 2;
			pInfo->m_InitAmmo[TFC_AMMO_GRENADES2] = 2;

			pInfo->m_MaxAmmo[TFC_AMMO_SHELLS] = 50;
			pInfo->m_MaxAmmo[TFC_AMMO_NAILS] = 50;
			pInfo->m_MaxAmmo[TFC_AMMO_ROCKETS] = 30;
			pInfo->m_MaxAmmo[TFC_AMMO_CELLS] = 200;
			pInfo->m_MaxAmmo[TFC_AMMO_MEDIKIT] = 0;
			pInfo->m_MaxAmmo[TFC_AMMO_DETPACK] = 0;
			pInfo->m_MaxAmmo[TFC_AMMO_GRENADES1] = 4;
			pInfo->m_MaxAmmo[TFC_AMMO_GRENADES2] = 4;


		// PC_CIVILIAN.
			pInfo = &g_TFCPlayerClassInfo[PC_CIVILIAN];
			pInfo->m_pClassName = "civilian";
			pInfo->m_pModelName = "models/player/civilian.mdl";
			pInfo->m_flMaxSpeed = 240;
			pInfo->m_iMaxHealth = 50;
			
			pInfo->m_iInitArmor = 0;
			pInfo->m_iMaxArmor = 0;
			
			pInfo->m_flInitArmorType = 0;
			pInfo->m_flMaxArmorType = 0;
			pInfo->m_nArmorClasses = 0;
			pInfo->m_iInitArmorClass = 0;

			pInfo->m_iGrenadeType1 = GR_TYPE_NONE;
			pInfo->m_iGrenadeType2 = GR_TYPE_NONE;

			pInfo->m_InitAmmo[TFC_AMMO_SHELLS] = 0;
			pInfo->m_InitAmmo[TFC_AMMO_NAILS] = 0;
			pInfo->m_InitAmmo[TFC_AMMO_ROCKETS] = 0;
			pInfo->m_InitAmmo[TFC_AMMO_CELLS] = 0;
			pInfo->m_InitAmmo[TFC_AMMO_MEDIKIT] = 0;
			pInfo->m_InitAmmo[TFC_AMMO_DETPACK] = 0;
			pInfo->m_InitAmmo[TFC_AMMO_GRENADES1] = 0;
			pInfo->m_InitAmmo[TFC_AMMO_GRENADES2] = 0;

			pInfo->m_MaxAmmo[TFC_AMMO_SHELLS] = 0;
			pInfo->m_MaxAmmo[TFC_AMMO_NAILS] = 0;
			pInfo->m_MaxAmmo[TFC_AMMO_ROCKETS] = 0;
			pInfo->m_MaxAmmo[TFC_AMMO_CELLS] = 0;
			pInfo->m_MaxAmmo[TFC_AMMO_MEDIKIT] = 0;
			pInfo->m_MaxAmmo[TFC_AMMO_DETPACK] = 0;
			pInfo->m_MaxAmmo[TFC_AMMO_GRENADES1] = 4;
			pInfo->m_MaxAmmo[TFC_AMMO_GRENADES2] = 4;
	}

} g_ClassInfoInitializer;




const CTFCPlayerClassInfo* GetTFCClassInfo( int iClass )
{
	Assert( iClass >= 0 && iClass < PC_LASTCLASS );
	return &g_TFCPlayerClassInfo[iClass];
}

