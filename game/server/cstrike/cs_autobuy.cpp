//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Data for Autobuy and Rebuy 
//
//=============================================================================//

#include "cbase.h"
#include "cs_autobuy.h"

// Weapon class information for each weapon including the class name and the buy command alias.
AutoBuyInfoStruct g_autoBuyInfo[] = 
{
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_RIFLE), "galil", "weapon_galil" }, // galil
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_RIFLE), "ak47", "weapon_ak47" },  // ak47
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_SNIPERRIFLE), "scout", "weapon_scout" }, // scout
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_RIFLE), "sg552", "weapon_sg552" }, // sg552
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_SNIPERRIFLE), "awp", "weapon_awp" }, // awp
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_SNIPERRIFLE), "g3sg1", "weapon_g3sg1" }, // g3sg1
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_RIFLE), "famas", "weapon_famas" }, // famas
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_RIFLE), "m4a1", "weapon_m4a1" }, // m4a1
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_RIFLE), "aug", "weapon_aug" }, // aug
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_SNIPERRIFLE), "sg550", "weapon_sg550" }, // sg550
	{ (AutoBuyClassType)(AUTOBUYCLASS_SECONDARY | AUTOBUYCLASS_PISTOL), "glock", "weapon_glock" }, // glock
	{ (AutoBuyClassType)(AUTOBUYCLASS_SECONDARY | AUTOBUYCLASS_PISTOL), "usp", "weapon_usp" }, // usp
	{ (AutoBuyClassType)(AUTOBUYCLASS_SECONDARY | AUTOBUYCLASS_PISTOL), "p228", "weapon_p228" }, // p228
	{ (AutoBuyClassType)(AUTOBUYCLASS_SECONDARY | AUTOBUYCLASS_PISTOL), "deagle", "weapon_deagle" }, // deagle
	{ (AutoBuyClassType)(AUTOBUYCLASS_SECONDARY | AUTOBUYCLASS_PISTOL), "elite", "weapon_elite" },	// elites
	{ (AutoBuyClassType)(AUTOBUYCLASS_SECONDARY | AUTOBUYCLASS_PISTOL), "fn57", "weapon_fiveseven" },	// fn57
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_SHOTGUN), "m3", "weapon_m3" }, // m3
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_SHOTGUN), "xm1014", "weapon_xm1014" },	// xm1014
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_SMG), "mac10", "weapon_mac10" },	// mac10
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_SMG), "tmp", "weapon_tmp" },	// tmp
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_SMG), "mp5navy", "weapon_mp5navy" },	// mp5navy
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_SMG), "ump45", "weapon_ump45" },	// ump45
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_SMG), "p90", "weapon_p90" },	// p90
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_MACHINEGUN), "m249", "weapon_m249" },	// m249
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_AMMO), "primammo", "primammo" },	// primammo
	{ (AutoBuyClassType)(AUTOBUYCLASS_SECONDARY | AUTOBUYCLASS_AMMO), "secammo", "secammo" }, // secmmo
	{ (AutoBuyClassType)(AUTOBUYCLASS_ARMOR), "vest", "item_kevlar" }, // vest
	{ (AutoBuyClassType)(AUTOBUYCLASS_ARMOR), "vesthelm", "item_assaultsuit" }, // vesthelm
	{ (AutoBuyClassType)(AUTOBUYCLASS_GRENADE), "flashbang", "weapon_flashbang" }, // flash
	{ (AutoBuyClassType)(AUTOBUYCLASS_GRENADE), "hegrenade", "weapon_hegrenade" }, // hegren
	{ (AutoBuyClassType)(AUTOBUYCLASS_GRENADE), "smokegrenade", "weapon_smokegrenade" }, // sgren
	{ (AutoBuyClassType)(AUTOBUYCLASS_NIGHTVISION), "nvgs", "nvgs" }, // nvgs
	{ (AutoBuyClassType)(AUTOBUYCLASS_DEFUSER), "defuser", "defuser" }, // defuser
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_SHIELD), "shield", "shield" }, // shield

	{ (AutoBuyClassType)0, "", "" } // last one, must be at end.
};
