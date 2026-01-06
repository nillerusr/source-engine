//=================== Half-Life 2: Short Stories Mod 2009 ====================================//
//
// Purpose: All Weapons derivied from CBaseCombatWeapon will have const int HLSS_GetWeaponId()
//
//============================================================================================//


#ifndef HLSS_WEAPON_ID
#define HLSS_WEAPON_ID

enum HLSS_WeaponId {
	HLSS_WEAPON_ID_NONE = 0,

	HLSS_WEAPON_ID_SMG1,
	HLSS_WEAPON_ID_SMG2,
	HLSS_WEAPON_ID_SHOTGUN,
	HLSS_WEAPON_ID_AR2,
	HLSS_WEAPON_ID_RPG,
	HLSS_WEAPON_ID_TURRET,
	HLSS_WEAPON_ID_SNIPER,

	HLSS_WEAPON_ID_CROSSBOW,
	HLSS_WEAPON_ID_PHYSGUN,

	HLSS_WEAPON_ID_PISTOL,
	HLSS_WEAPON_ID_357,
	HLSS_WEAPON_ID_ALYXGUN,

	HLSS_WEAPON_ID_STUNSTICK,
	HLSS_WEAPON_ID_CROWBAR,
};

#endif