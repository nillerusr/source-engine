//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "c_weapon__stubs.h"
#include "basehlcombatweapon_shared.h"
#include "c_basehlcombatweapon.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

STUB_WEAPON_CLASS( cycler_weapon, WeaponCycler, C_BaseCombatWeapon );

STUB_WEAPON_CLASS( weapon_binoculars, WeaponBinoculars, C_BaseHLCombatWeapon );
STUB_WEAPON_CLASS( weapon_bugbait, WeaponBugBait, C_BaseHLCombatWeapon );
STUB_WEAPON_CLASS( weapon_flaregun, Flaregun, C_BaseHLCombatWeapon );
STUB_WEAPON_CLASS( weapon_annabelle, WeaponAnnabelle, C_BaseHLCombatWeapon );
STUB_WEAPON_CLASS( weapon_gauss, WeaponGaussGun, C_BaseHLCombatWeapon );
STUB_WEAPON_CLASS( weapon_cubemap, WeaponCubemap, C_BaseCombatWeapon );
STUB_WEAPON_CLASS( weapon_alyxgun, WeaponAlyxGun, C_HLSelectFireMachineGun );
STUB_WEAPON_CLASS( weapon_citizenpackage, WeaponCitizenPackage, C_BaseHLCombatWeapon );
STUB_WEAPON_CLASS( weapon_citizensuitcase, WeaponCitizenSuitcase, C_WeaponCitizenPackage );

#ifndef HL2MP
STUB_WEAPON_CLASS( weapon_ar2, WeaponAR2, C_HLMachineGun );
STUB_WEAPON_CLASS( weapon_frag, WeaponFrag, C_BaseHLCombatWeapon );
STUB_WEAPON_CLASS( weapon_rpg, WeaponRPG, C_BaseHLCombatWeapon );
STUB_WEAPON_CLASS( weapon_pistol, WeaponPistol, C_BaseHLCombatWeapon );
STUB_WEAPON_CLASS(weapon_manhacktoss, WeaponManhackToss, C_BaseHLCombatWeapon);
STUB_WEAPON_CLASS( weapon_shotgun, WeaponShotgun, C_BaseHLCombatWeapon );
STUB_WEAPON_CLASS( weapon_smg1, WeaponSMG1, C_HLSelectFireMachineGun );
#ifdef EZ
STUB_WEAPON_CLASS( weapon_smg2, WeaponSMG2, C_HLSelectFireMachineGun );
#endif
STUB_WEAPON_CLASS( weapon_357, Weapon357, C_BaseHLCombatWeapon );
STUB_WEAPON_CLASS( weapon_crossbow, WeaponCrossbow, C_BaseHLCombatWeapon );
#ifndef MAPBASE
STUB_WEAPON_CLASS( weapon_slam, Weapon_SLAM, C_BaseHLCombatWeapon );
#endif
STUB_WEAPON_CLASS( weapon_crowbar, WeaponCrowbar, C_BaseHLBludgeonWeapon );
#ifdef HL2_EPISODIC
STUB_WEAPON_CLASS( weapon_hopwire, WeaponHopwire, C_BaseHLCombatWeapon );
//STUB_WEAPON_CLASS( weapon_proto1, WeaponProto1, C_BaseHLCombatWeapon );
#endif
#ifdef HL2_LOSTCOAST
STUB_WEAPON_CLASS( weapon_oldmanharpoon, WeaponOldManHarpoon, C_WeaponCitizenPackage );
#endif
#endif

#ifdef EZ2
//STUB_WEAPON_CLASS( weapon_pulsepistol, WeaponPulsePistol, C_BaseHLCombatWeapon );
STUB_WEAPON_CLASS( weapon_displacer_pistol, DisplacerPistol, C_BaseHLCombatWeapon );
STUB_WEAPON_CLASS( weapon_ar2_proto, WeaponAR2Proto, C_BaseHLCombatWeapon );
STUB_WEAPON_CLASS( weapon_endgame, WeaponEndGame, C_BaseHLCombatWeapon );
STUB_WEAPON_CLASS( weapon_arbeit_clipboard, WeaponArbeitClipboard, C_WeaponCitizenPackage );

STUB_WEAPON_CLASS( weapon_flechette_shotgun, WeaponFlechetteShotgun, C_BaseHLCombatWeapon );
STUB_WEAPON_CLASS( weapon_stasis_grenade, WeaponStasisGrenade, C_BaseHLCombatWeapon );
#endif


