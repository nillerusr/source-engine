//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef BUY_PRESETS_H
#define BUY_PRESETS_H
#ifdef _WIN32
#pragma once
#endif

#define USE_BUY_PRESETS 1

/**
 * CLASS STRUCTURE:
 *  Buy presets are managed by TheBuyPresetManager, which has a list of BuyPreset objects.
 *  Each BuyPreset has a list of fallback WeaponSets.  Each WeaponSet is a complete set of
 *  weapons and equipment that could be purchased by a buy preset.
 *
 * BUYING:
 *  When purchasing a buy preset, the fallback WeaponSets are considered in order from first
 *  to last.  When one is deemed to be purchasable, the buy commands are generated and
 *  processing stops.
 *
 * EDITING:
 *  When editing buy presets, TheBuyPresetManager maintains a second list of BuyPresets, to
 *  allow an all-or-nothing undo system.  The editing is done on two main VGUI menus: a
 *  list of the four main buy presets (CBuyPresetEditMainMenu), and a menu showing the
 *  fallbacks for a single preset (CBuyPresetEditOutfitListMenu).
 *
 *  Presets and fallbacks can be copied, created, deleted, etc from these pages.  From the
 *  fallback menu, a wizard can edit the contents of a fallback, specifying primary weapon,
 *  pistol, and equipment.
 */

#include "weapon_csbase.h"
#include <KeyValues.h>
#include <utlvector.h>

class BuyPreset;
class CCSWeaponInfo;

//--------------------------------------------------------------------------------------------------------------
enum BuyPresetStringSizes
{
	BUY_PRESET_COMMAND_LEN = 256,
	MaxBuyPresetName = 64,
	MaxBuyPresetImageFname = 64,
};

enum AmmoSizeType
{
	AMMO_PERCENT,
	AMMO_CLIPS,
	AMMO_ROUNDS
};

enum { NUM_PRESETS = 4 };

//--------------------------------------------------------------------------------------------------------------
/**
 *  A BuyPresetWeapon stores the mp.dll version of the WeaponID, the increment for buying ammo (clips, rounds, etc),
 *  the number of increments to buy, etc.  This is the basic info for buying a weapon that is present in an
 *  item set.
 */
class BuyPresetWeapon
{
public:
	BuyPresetWeapon();
	BuyPresetWeapon( CSWeaponID weaponID );
	BuyPresetWeapon& operator= (const BuyPresetWeapon& other);

	const wchar_t*	GetName() const { return m_name; }				///< Returns the display name corresponding to the weapon ID
	CSWeaponID		GetWeaponID() const { return m_weaponID; }		///< Returns the WEAPON_* weapon ID
	void			SetWeaponID( CSWeaponID weaponID );				///< Sets the WEAPON_* weapon ID (and resets ammo, etc)

	void			SetAmmoType( AmmoSizeType ammoType ) { m_ammoType = ammoType; }	///< Sets the ammo type - percent (unused), clips, or rounds (unused)
	void			SetAmmoAmount( int ammoAmount ) { m_ammoAmount = ammoAmount; }	///< Sets the amount of ammo, in units relevant to the ammo type
	void			SetFillAmmo( bool fill ) { m_fillAmmo = fill; }	///< Sets whether the weapon's ammo should be topped off

	AmmoSizeType	GetAmmoType() const { return m_ammoType; }		///< Returns ammo type - percent (unused), clips, or rounds (unused)
	int				GetAmmoAmount() const { return m_ammoAmount; }	///< Returns the amount of ammo, in units relevant to the ammo type
	bool			GetFillAmmo() const { return m_fillAmmo; }		///< Returns true if the weapon's ammo should be topped off

protected:
	const wchar_t	*m_name;
	CSWeaponID		m_weaponID;
	AmmoSizeType	m_ammoType;
	int				m_ammoAmount;
	bool			m_fillAmmo;
};

typedef CUtlVector< BuyPresetWeapon > BuyPresetWeaponList;

//--------------------------------------------------------------------------------------------------------------
/**
 *  A WeaponSet is part of a buy preset.  Each buy preset is composed of a series of (fallback) WeaponSets.
 *  The WeaponSet contains a snapshot of a set of weapons and equipment that should be bought.
 */
class WeaponSet
{
public:
	WeaponSet();

	void GetCurrent( int& cost, WeaponSet& ws ) const;								///< Generates a WeaponSet containing only items that would be bought right now
	void GetFromScratch( int& cost, WeaponSet& ws ) const;							///< Generates a WeaponSet containing everything that would be bought from scratch

	void GenerateBuyCommands( char command[BUY_PRESET_COMMAND_LEN] ) const;			///< Generates a list of commands to buy the current WeaponSet.

	int FullCost() const;															///< Calculates the full cost of the WeaponSet, including all optional items

	void Reset( void );

	const BuyPresetWeapon& GetPrimaryWeapon() const { return m_primaryWeapon; }
	const BuyPresetWeapon& GetSecondaryWeapon() const { return m_secondaryWeapon; }

	BuyPresetWeapon m_primaryWeapon;
	BuyPresetWeapon m_secondaryWeapon;

	// Equipment --------------------------------
	int		m_armor;
	bool	m_helmet;
	bool	m_smokeGrenade;
	bool	m_HEGrenade;
	int		m_flashbangs;
	bool	m_defuser;
	bool	m_nightvision;
};

typedef CUtlVector< WeaponSet > WeaponSetList;

//--------------------------------------------------------------------------------------------------------------
/**
 *  The BuyPreset is a complete representation of a buy preset.  Essentially, it consists of the preset name,
 *  whether or not the preset is used for autobuy, and a list of fallback WeaponSets.
 */
class BuyPreset
{
public:
	BuyPreset();
	~BuyPreset();
	BuyPreset(const BuyPreset& other);

	void SetName( const wchar_t *name );
	const wchar_t * GetName() const { return m_name; }

	void Parse( KeyValues *data );								///< Populates data from a KeyValues structure, for loading
	void Save( KeyValues *data );								///< Fills in a KeyValues structure with data, for saving

	int GetNumSets() const { return m_weaponList.Count(); }		///< Returns the number of fallback WeaponSets
	const WeaponSet * GetSet( int index ) const;				///< Returns the specified fallback WeaponSet, or NULL if it doesn't exist

	int FullCost() const;										///< Calculates the full cost of the preset, which is exactly the full cost of the first fallback WeaponSet

	// editing functions --------------------
	void DeleteSet( int index );								///< Deletes a fallback
	void SwapSet( int firstIndex, int secondIndex );			///< Switches the order of two fallbacks
	void ReplaceSet( int index, const WeaponSet& weaponSet );	///< Replaces a fallback with the supplied WeaponSet

private:
	wchar_t m_name[MaxBuyPresetName];

	WeaponSetList m_weaponList;
};

typedef CUtlVector< BuyPreset > BuyPresetList;

//--------------------------------------------------------------------------------------------------------------
/**
 *  The BuyPresetManager singleton manages saving/loading/buying the individual BuyPresets.  It also tracks the
 *  ownership of some items that aren't explicitly known by the client (defuser, nightvision).
 *
 *  Two sets of BuyPresets are maintained - the live set used for purchasing, and a set that is acted upon for
 *  editing.  This provides the basic save changes/abandon changes ability when editing presets.
 */
class BuyPresetManager
{
public:
	BuyPresetManager();

	void Save();

	void PurchasePreset( int presetIndex );							///< Generates and sends buy commands to buy a specific preset

	int GetNumPresets() { VerifyLoadedTeam(); return m_presets.Count(); }
	const BuyPreset * GetPreset( int index ) const;					///< Returns the specified "live" preset, or NULL if it doesn't exist
	void SetPreset( int index, const BuyPreset *preset );

	void SetPresets( const BuyPresetList& presets ) { m_presets = presets; }

	void SetEditPresets( const BuyPresetList& presets ) { m_editPresets = presets; }
	int GetNumEditPresets() const { return m_editPresets.Count(); }

	BuyPreset * GetEditPreset( int index );							///< Returns the specified editing preset, or NULL if it doesn't exist
	const CUtlVector< BuyPreset >& GetEditPresets() const { return m_editPresets; }

	void ResetEditPresets() { m_editPresets = m_presets; }			///< Resets the editing presets to the live presets (e.g. when starting editing)
	void ResetEditToDefaults( void );							///< Loads the default presets into the editing presets (e.g. when hitting the "Use Defaults" button)

	void GetCurrentLoadout( WeaponSet *weaponSet );

private:
	BuyPresetList	m_presets;		///< Current (live) presets
	BuyPresetList	m_editPresets;	///< BuyPresets used when editing

	int m_loadedTeam;
	void VerifyLoadedTeam( void );
};

//--------------------------------------------------------------------------------------------------------------
extern BuyPresetManager *TheBuyPresets;

//--------------------------------------------------------------------------------------------------------------
/**
 *  Functions for controlling the display of the various buy preset editing menus.  Opening one closes any
 *  others open.
 */
enum { REOPEN_YES, REOPEN_NO, REOPEN_DONTCHANGE };					///< Determines if we need to re-open the buy menu when done editing.
void ShowBuyPresetMainMenu( bool runUpdate, int reopenBuyMenu );	///< Open the main preset editing menu
void ShowBuyPresetEditMenu( int presetIndex );						///< Open the menu for editing the list of fallbacks for an individual preset

//--------------------------------------------------------------------------------------------------------------
/**
 *  Returns the image filename for a given WEAPON_* weapon ID.  Primary is specified because WEAPON_NONE returns
 *  separate images for primary and pistol, to facilitate showing an outline version for "Current Weapon".
 */
const char *ImageFnameFromWeaponID( CSWeaponID weaponID, bool isPrimary );

//--------------------------------------------------------------------------------------------------------------
/**
 *  Constructs a list of weapons that will satisfy the any unfinished weapon-specific career tasks:
 *   1.  If there are no unfinished career tasks, the source list is returned
 *   2.  If there are unfinished career tasks, all weapons that can be used for the tasks are added
 *       to the front of the list.  This list of career weapons is sorted according to the order of
 *       weapons in the source list, so that if any weapons in the source list can be used for career
 *       tasks, they will be.
 */
BuyPresetWeaponList CareerWeaponList( const BuyPresetWeaponList& source, bool isPrimary, CSWeaponID currentClientID );

//--------------------------------------------------------------------------------------------------------------
/**
 *  Returns the number of clips that will have to be bought for the specified BuyPresetWeapon
 */
class CCSWeaponInfo;
int CalcClipsNeeded( const BuyPresetWeapon *pWeapon, const CCSWeaponInfo *pInfo, const int ammo[MAX_AMMO_TYPES] );

//--------------------------------------------------------------------------------------------------------------
/**
 *  Fills the ammo array with the ammo currently owned by the local player
 */
void FillClientAmmo( int ammo[MAX_AMMO_TYPES] );

//--------------------------------------------------------------------------------------------------------------
/**
 *  Returns true if the client can currently buy the weapon according to class/gamemode restrictions.  Returns
 *  true also if the player owns the weapon already.
 */
bool CanBuyWeapon( CSWeaponID currentPrimaryID, CSWeaponID currentSecondaryID, CSWeaponID weaponID );

//--------------------------------------------------------------------------------------------------------------
/**
 *  Returns the display name for a weapon, based on it's weaponID
 */
const wchar_t* WeaponIDToDisplayName( CSWeaponID weaponID );

//--------------------------------------------------------------------------------------------------------------
// If our weapon is NONE, we want to be able to buy up to this many clips for the current gun we're holding
enum { NUM_CLIPS_FOR_CURRENT = 4 };

#if USE_BUY_PRESETS
extern ConVar cl_buy_favorite_quiet;
extern ConVar cl_buy_favorite_nowarn;
#endif // USE_BUY_PRESETS

#endif // BUY_PRESETS_H
