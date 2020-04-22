//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"

#include "buy_preset_debug.h"
#include "buy_presets.h"
#include "weapon_csbase.h"
#include "cs_ammodef.h"
#include "cs_gamerules.h"
#include "cstrike/bot/shared_util.h"
#include "vgui/ILocalize.h"
#include <vgui_controls/Controls.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar cl_rebuy;

//--------------------------------------------------------------------------------------------------------------
/**
 *  Returns true if the local player is a CT and the map is a defuse map.
 */
static bool CanBuyDefuser()
{
	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
	return ( pPlayer && pPlayer->GetTeamNumber() == TEAM_CT && CSGameRules()->IsBombDefuseMap() );
}


void BuyPresetManager::GetCurrentLoadout( WeaponSet *weaponSet )
{
	if ( !weaponSet )
		return;

	C_CSPlayer *player = C_CSPlayer::GetLocalCSPlayer();
	if ( !player )
		return;

	CWeaponCSBase *pWeapon;
	const CCSWeaponInfo *pInfo;

	int ammo[MAX_AMMO_TYPES];
	memset( ammo, 0, sizeof(ammo) );
	FillClientAmmo( ammo );

	weaponSet->Reset();

	// Grab current armor values
	weaponSet->m_armor = ( player->ArmorValue() > 0 ) ? 100 : 0;
	weaponSet->m_helmet = player->HasHelmet();

	// Grab current smoke grenade
	pWeapon = dynamic_cast< CWeaponCSBase * >(player->GetCSWeapon( WEAPON_SMOKEGRENADE ));
	pInfo = GetWeaponInfo( WEAPON_SMOKEGRENADE );
	int ammoType = (pInfo)?pInfo->iAmmoType:0;
	weaponSet->m_smokeGrenade = (pWeapon && player->GetAmmoCount( ammoType ));

	// Grab current HE grenade
	pWeapon = dynamic_cast< CWeaponCSBase * >(player->GetCSWeapon( WEAPON_HEGRENADE ));
	pInfo = GetWeaponInfo( WEAPON_HEGRENADE );
	ammoType = (pInfo)?pInfo->iAmmoType:0;
	weaponSet->m_HEGrenade = (pWeapon && player->GetAmmoCount( ammoType ));

	// Grab current flashbangs
	pWeapon = dynamic_cast< CWeaponCSBase * >(player->GetCSWeapon( WEAPON_FLASHBANG ));
	pInfo = GetWeaponInfo( WEAPON_FLASHBANG );
	ammoType = (pInfo)?pInfo->iAmmoType:0;
	weaponSet->m_flashbangs = (!pWeapon) ? 0 : player->GetAmmoCount( ammoType );

	// Grab current equipment
	weaponSet->m_defuser = player->HasDefuser();
	weaponSet->m_nightvision = player->HasNightVision();

	// Grab current primary weapon
	CSWeaponID primaryID = GetClientWeaponID( true );		///< The local player's current primary weapon
	pInfo = GetWeaponInfo( primaryID );
	if ( pInfo )
	{
		int roundsNeeded = ammo[pInfo->iAmmoType];
		int buySize = GetCSAmmoDef()->GetBuySize( pInfo->iAmmoType );
		int numClips = ceil(roundsNeeded/(float)buySize);

		weaponSet->m_primaryWeapon.SetWeaponID( primaryID );
		weaponSet->m_primaryWeapon.SetAmmoType( AMMO_CLIPS );
		weaponSet->m_primaryWeapon.SetAmmoAmount( numClips );
		weaponSet->m_primaryWeapon.SetFillAmmo( false );
	}

	// Grab current pistol
	CSWeaponID secondaryID = GetClientWeaponID( false );	///< The local player's current pistol
	pInfo = GetWeaponInfo( secondaryID );
	if ( pInfo )
	{
		int roundsNeeded = ammo[pInfo->iAmmoType];
		int buySize = GetCSAmmoDef()->GetBuySize( pInfo->iAmmoType );
		int numClips = ceil(roundsNeeded/(float)buySize);

		weaponSet->m_secondaryWeapon.SetWeaponID( secondaryID );
		weaponSet->m_secondaryWeapon.SetAmmoType( AMMO_CLIPS );
		weaponSet->m_secondaryWeapon.SetAmmoAmount( numClips );
		weaponSet->m_secondaryWeapon.SetFillAmmo( false );
	}
}


//--------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------
WeaponSet::WeaponSet()
{
	Reset();
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Sets reasonable defaults for an empty weapon set: no primary/secondary weapons, no equipment, and
 *  everything optional.
 */
void WeaponSet::Reset()
{
	BuyPresetWeapon blank;
	m_primaryWeapon = blank;
	m_secondaryWeapon = blank;

	m_armor = 0;
	m_helmet = false;
	m_smokeGrenade = false;
	m_HEGrenade = false;
	m_flashbangs = 0;
	m_defuser = false;
	m_nightvision = false;
}


//--------------------------------------------------------------------------------------------------------------
/**
 *  Constructs a WeaponSet containing everything that could be purchased right now.  The cost is also
 *  returned, and is -1 if the set is not purchasable (as opposed to completely purchased already, in
 *  which case cost is 0.)
 */
void WeaponSet::GetCurrent( int& cost, WeaponSet& ws ) const
{
	cost = -1;
	ws.Reset();

	if ( !engine->IsConnected() )
		return;

	C_CSPlayer *player = CCSPlayer::GetLocalCSPlayer();
	if ( !player )
		return;

	if ( player->GetTeamNumber() != TEAM_CT && player->GetTeamNumber() != TEAM_TERRORIST )
		return;

	// we're connected, and a valid team, so we can purchase
	cost = 0;

	if ( player->IsVIP() )
		return; // can't buy anything as the VIP

	int iHelmetPrice = HELMET_PRICE;
	int iKevlarPrice = KEVLAR_PRICE;
	int iNVGPrice = NVG_PRICE;

	if ( CSGameRules()->IsBlackMarket() )
	{
		iHelmetPrice = CSGameRules()->GetBlackMarketPriceForWeapon( WEAPON_ASSAULTSUIT ) - CSGameRules()->GetBlackMarketPriceForWeapon( WEAPON_KEVLAR );
		iKevlarPrice = CSGameRules()->GetBlackMarketPriceForWeapon( WEAPON_KEVLAR );
		iNVGPrice = CSGameRules()->GetBlackMarketPriceForWeapon( WEAPON_NVG );
	}

	//-------------------------------------------------------------------------
	//-------------------------------------------------------------------------
	// Buy any required items

	//-------------------------------------------------------------------------
	// Armor
	if ( m_armor > player->ArmorValue() )
	{
		cost += iKevlarPrice;
		ws.m_armor = 100;
	}

	if ( (m_helmet && m_armor > 0) && !player->HasHelmet() )
	{
		cost += iHelmetPrice;
		ws.m_armor = 100;
		ws.m_helmet = true;
	}

	CWeaponCSBase *pWeapon;
	CCSWeaponInfo *pInfo;

	//-------------------------------------------------------------------------
	// Smoke grenade
	pWeapon = dynamic_cast< CWeaponCSBase * >(player->GetCSWeapon( WEAPON_SMOKEGRENADE ));
	pInfo = GetWeaponInfo( WEAPON_SMOKEGRENADE );
	int ammoType = (pInfo)?pInfo->iAmmoType:0;

	bool hasSmokeGrenade = (pWeapon && player->GetAmmoCount( ammoType ));
	if ( m_smokeGrenade && !hasSmokeGrenade )
	{
		cost += pInfo->GetWeaponPrice();
		ws.m_smokeGrenade = true;
		hasSmokeGrenade = true;
	}

	//-------------------------------------------------------------------------
	// HE grenade
	pWeapon = dynamic_cast< CWeaponCSBase * >(player->GetCSWeapon( WEAPON_HEGRENADE ));
	pInfo = GetWeaponInfo( WEAPON_HEGRENADE );
	ammoType = (pInfo)?pInfo->iAmmoType:0;

	bool hasHEGrenade = (pWeapon && player->GetAmmoCount( ammoType ));
	if ( m_HEGrenade && !hasHEGrenade )
	{
		cost += pInfo->GetWeaponPrice();
		ws.m_HEGrenade = true;
		hasHEGrenade = true;
	}

	//-------------------------------------------------------------------------
	// Flashbang grenades
	pWeapon = dynamic_cast< CWeaponCSBase * >(player->GetCSWeapon( WEAPON_FLASHBANG ));
	pInfo = GetWeaponInfo( WEAPON_FLASHBANG );
	ammoType = (pInfo)?pInfo->iAmmoType:0;

	int numFlashbangs = (!pWeapon) ? 0 : player->GetAmmoCount( ammoType );
	if ( m_flashbangs && numFlashbangs < m_flashbangs )
	{
		int count = m_flashbangs - numFlashbangs;
		cost += pInfo->GetWeaponPrice() * count;
		ws.m_flashbangs = count;
		numFlashbangs += count;
	}

	//-------------------------------------------------------------------------
	// defuser
	if ( m_defuser && player->GetTeamNumber() == TEAM_CT && !player->HasDefuser() && CanBuyDefuser() )
	{
		cost += DEFUSEKIT_PRICE;
		ws.m_defuser = true;
	}

	//-------------------------------------------------------------------------
	// nightvision goggles
	if ( m_nightvision && !player->HasNightVision() )
	{
		cost += iNVGPrice;
		ws.m_nightvision = true;
	}

	//-------------------------------------------------------------------------
	// Construct a list of weapons to buy from.
	CSWeaponID primaryID = GetClientWeaponID( true );		///< The local player's current primary weapon
	CSWeaponID secondaryID = GetClientWeaponID( false );	///< The local player's current pistol

	BuyPresetWeaponList primaryWeapons;
	primaryWeapons.AddToTail( m_primaryWeapon );

	BuyPresetWeaponList secondaryWeapons;
	secondaryWeapons.AddToTail( m_secondaryWeapon );

	/**
	 *  Determine best weapons & ammo to purchase
	 *
	 *  For each primary weapon in the list, do the following until one satisfies the current money situation:
	 *    1. buy primary weapon and its ammo (NEED TO WATCH FOR WEAPONS PLAYER CANNOT BUY!!!)
	 *    2. for each non-optional secondary weapon (if any),
	 *         try to buy it and its ammo
	 *    3. when a set can be purchased, add it to the total cost and stop processing
	 *  If we can't purchase the primary weapon, or a required secondary weapon, the WeaponSet can't be bought,
	 *  so we return -1.
	 *
	 *  To find if a weapon is owned, we can compare it's weaponID to either primaryID or secondaryID from above.
	 */

	int currentCost = cost;
	int ammo[MAX_AMMO_TYPES];
	memset( ammo, 0, sizeof(ammo) );
	FillClientAmmo( ammo );
	const BuyPresetWeapon *primaryWeaponToBuy = NULL;
	const BuyPresetWeapon *secondaryWeaponToBuy = NULL;
	int primaryClipsToBuy = 0;
	int secondaryClipsToBuy = 0;
	int currentNonWeaponCost = currentCost;
	bool doneBuyingWeapons = false;

	int currentCash = player->GetAccount();

	//-------------------------------------------------------------------------
	// Try to buy the primary weapon
	int i;
	for ( i=0; i<primaryWeapons.Count() && !doneBuyingWeapons; ++i )
	{
		const BuyPresetWeapon *primaryWeapon = &primaryWeapons[i];
		primaryWeaponToBuy = NULL;
		int primaryClips = 0;
		primaryClipsToBuy = 0;
		currentCost = currentNonWeaponCost;
		FillClientAmmo( ammo );

		CSWeaponID weaponID = primaryWeapon->GetWeaponID();
		if ( weaponID == WEAPON_NONE )
			weaponID = primaryID;

		CCSWeaponInfo *primaryInfo = GetWeaponInfo( weaponID );
		int primaryAmmoCost = 0;
		int primaryAmmoBuySize = 0;
		if ( primaryInfo )
		{
			primaryClips = CalcClipsNeeded( primaryWeapon, primaryInfo, ammo );
			int primaryWeaponCost = ( weaponID != primaryID ) ? primaryInfo->GetWeaponPrice() : 0;
			primaryAmmoCost = GetCSAmmoDef()->GetCost( primaryInfo->iAmmoType );
			primaryAmmoBuySize = GetCSAmmoDef()->GetBuySize( primaryInfo->iAmmoType );
			currentCost += primaryWeaponCost;
			currentCost += primaryAmmoCost * primaryClips;
			ammo[primaryInfo->iAmmoType] += primaryClips * primaryAmmoBuySize;

			CURRENTCOST_DEBUG("\t%5.5d (%s)\n", primaryWeaponCost, WeaponIDToAlias( weaponID ));
			CURRENTCOST_DEBUG("\t%5.5d (ammo x %d)\n", primaryAmmoCost * primaryClips, primaryClips);
			if ( currentCash < currentCost )
			{
				currentCost = currentNonWeaponCost;	// can't afford it, so try the next weapon
				continue;
			}

			// check for as_* maps, and CTs buying AK47s, etc
			if ( !CanBuyWeapon(primaryID, secondaryID, weaponID) && weaponID != primaryID )
			{
				currentCost = currentNonWeaponCost;	// can't buy it, so try the next weapon
				continue;
			}

			primaryWeaponToBuy = primaryWeapon;
			primaryClipsToBuy = primaryClips;
		}

		if ( !secondaryWeapons.Count() )
		{
			CURRENTCOST_DEBUG("\t\t\tDone buying weapons (no secondary)\n");
			break;
		}

		//-------------------------------------------------------------------------
		// Try to buy a (required) secondary weapon to go with primaryWeaponToBuy.
		// If not, reset cost to not reflect either weapon, so we can look at buying
		// the next primary weapon in the list.
		int j;
		for ( j=0; j<secondaryWeapons.Count(); ++j )
		{
			const BuyPresetWeapon *secondaryWeapon = &secondaryWeapons[j];

			// reset ammo counts and cost
			secondaryWeaponToBuy = NULL;
			secondaryClipsToBuy = 0;
			currentCost = currentNonWeaponCost;
			FillClientAmmo( ammo );

			// add in ammo we're already buying for the primary weapon to the client's actual ammo
			if ( primaryInfo )
			{
				currentCost += ( weaponID != primaryID ) ? primaryInfo->GetWeaponPrice() : 0;
				currentCost += primaryAmmoCost * primaryClips;
				ammo[primaryInfo->iAmmoType] += primaryClips * primaryAmmoBuySize;
			}

			int currentCostWithoutSecondary = currentCost;

			CSWeaponID weaponID = secondaryWeapon->GetWeaponID();
			if ( weaponID == WEAPON_NONE )
				weaponID = secondaryID;

			CCSWeaponInfo *secondaryInfo = GetWeaponInfo( weaponID );
			if ( !secondaryInfo )
			{
				currentCost = currentCostWithoutSecondary;
				continue;
			}

			int secondaryClips = CalcClipsNeeded( secondaryWeapon, secondaryInfo, ammo );
			int secondaryWeaponCost = ( weaponID != secondaryID ) ? secondaryInfo->GetWeaponPrice() : 0;
			int secondaryAmmoCost = GetCSAmmoDef()->GetCost( secondaryInfo->iAmmoType );
			int secondaryAmmoBuySize = GetCSAmmoDef()->GetBuySize( secondaryInfo->iAmmoType );
			currentCost += secondaryWeaponCost;
			currentCost += secondaryAmmoCost * secondaryClips;
			ammo[secondaryInfo->iAmmoType] += secondaryClips * secondaryAmmoBuySize;

			CURRENTCOST_DEBUG("\t%5.5d (%s)\n", secondaryWeaponCost, WeaponIDToAlias( weaponID ));
			CURRENTCOST_DEBUG("\t%5.5d (ammo x %d)\n", secondaryAmmoCost * secondaryClips, secondaryClips);
			if ( currentCash < currentCost )
			{
				currentCost = currentCostWithoutSecondary;	// can't afford it, so skip
				continue;
			}

			// check for as_* maps, and CTs buying elites, etc
			if ( !CanBuyWeapon(primaryID, secondaryID, weaponID) && weaponID != secondaryID )
			{
				currentCost = currentCostWithoutSecondary;	// can't buy it, so skip
				continue;
			}

			// We found both a primary and secondary weapon to buy.  Stop looking for more.
			secondaryWeaponToBuy = secondaryWeapon;
			secondaryClipsToBuy = secondaryClips;
			doneBuyingWeapons = true;
			CURRENTCOST_DEBUG("\t\t\tDone buying weapons (primary && secondary)\n");
			break;
		}
	}

	// Update our cost to reflect the weapons and ammo purchased
	cost = currentCost;

	if ( primaryWeaponToBuy )
	{
		BuyPresetWeapon weapon = *primaryWeaponToBuy;
		weapon.SetAmmoAmount( primaryClipsToBuy );
		weapon.SetAmmoType( AMMO_CLIPS );
		ws.m_primaryWeapon = weapon;
	}
	else
	{
		// If we failed to buy a primary weapon, and one was in the list, bail - the player can't afford this WeaponSet.
		for ( int i=0; i<primaryWeapons.Count(); ++i )
		{
			int weaponID = primaryWeapons[i].GetWeaponID();
			if ( weaponID != WEAPON_NONE )
			{
				cost = -1;
				ws.Reset();
				return;
			}
		}
	}

	if ( secondaryWeaponToBuy )
	{
		BuyPresetWeapon weapon = *secondaryWeaponToBuy;
		weapon.SetAmmoAmount( secondaryClipsToBuy );
		weapon.SetAmmoType( AMMO_CLIPS );
		ws.m_secondaryWeapon = weapon;
	}
	else
	{
		// If we failed to buy a required secondary weapon, and one was in the list, bail - the player can't afford this WeaponSet.
		for ( int i=0; i<secondaryWeapons.Count(); ++i )
		{
			int weaponID = secondaryWeapons[i].GetWeaponID();
			if ( weaponID != WEAPON_NONE )
			{
				cost = -1;
				ws.Reset();
				return;
			}
		}
	}

	//-------------------------------------------------------------------------
	// now any extra ammo, in rebuy order
	const char *remainder = SharedParse( cl_rebuy.GetString() );
	const char *token;

	while ( remainder )
	{
		token = SharedGetToken();
		if ( !token || !*token )
			return;

		if ( !stricmp( token, "PrimaryAmmo" ) )
		{
			if ( primaryWeaponToBuy )
			{
				CSWeaponID id = primaryWeaponToBuy->GetWeaponID();
				if ( id == WEAPON_NONE )
					id = primaryID;

				if ( primaryWeaponToBuy->GetFillAmmo() )
				{
					const CCSWeaponInfo *info = GetWeaponInfo( id );
					if ( info )
					{
						int maxRounds = GetCSAmmoDef()->MaxCarry( info->iAmmoType );
						int ammoCost = GetCSAmmoDef()->GetCost( info->iAmmoType );
						int ammoBuySize = GetCSAmmoDef()->GetBuySize( info->iAmmoType );
						int roundsLeft = maxRounds - ammo[info->iAmmoType];
						int clipsLeft = (ammoBuySize > 0) ? ceil(roundsLeft / (float)ammoBuySize) : 0;
						while ( clipsLeft > 0 && cost + ammoCost <= currentCash )
						{
							ws.m_primaryWeapon.SetAmmoAmount( ws.m_primaryWeapon.GetAmmoAmount() + 1 );
							--clipsLeft;
							ammo[info->iAmmoType] += MIN(maxRounds, ammoBuySize);
							cost += ammoCost;
						}
					}
				}
			}
		}
		else if ( !stricmp( token, "SecondaryAmmo" ) )
		{
			if ( secondaryWeaponToBuy )
			{
				if ( secondaryWeaponToBuy->GetFillAmmo() )
				{
					CSWeaponID weaponID = secondaryWeaponToBuy->GetWeaponID();
					if ( weaponID == WEAPON_NONE )
						weaponID = secondaryID;

					const CCSWeaponInfo *info = GetWeaponInfo( weaponID );
					if ( info )
					{
						int maxRounds = GetCSAmmoDef()->MaxCarry( info->iAmmoType );
						int ammoCost = GetCSAmmoDef()->GetCost( info->iAmmoType );
						int ammoBuySize = GetCSAmmoDef()->GetBuySize( info->iAmmoType );
						int roundsLeft = maxRounds - ammo[info->iAmmoType];
						int clipsLeft = (ammoBuySize > 0) ? ceil(roundsLeft / (float)ammoBuySize) : 0;
						while ( clipsLeft > 0 && cost + ammoCost <= currentCash )
						{
							ws.m_secondaryWeapon.SetAmmoAmount( ws.m_secondaryWeapon.GetAmmoAmount() + 1 );
							--clipsLeft;
							ammo[info->iAmmoType] += MIN(maxRounds, ammoBuySize);
							cost += ammoCost;
						}
					}
				}
			}
		}

		remainder = SharedParse( remainder );
	}

	if ( cost > currentCash )
	{
		cost = -1; // can't buy it
		ws.Reset();
	}
}


//--------------------------------------------------------------------------------------------------------------
/**
 *  Constructs a WeaponSet containing everything that can be purchased.
 *  The cost (including extra cash) is also calculated and returned.
 */
void WeaponSet::GetFromScratch( int& cost, WeaponSet& ws ) const
{
	cost = 0;
	ws.Reset();

	int ammo[MAX_AMMO_TYPES];
	memset( ammo, 0, sizeof(ammo) );

	int iHelmetPrice = HELMET_PRICE;
	int iKevlarPrice = KEVLAR_PRICE;
	int iNVGPrice = NVG_PRICE;

	if ( CSGameRules()->IsBlackMarket() )
	{
		iHelmetPrice = CSGameRules()->GetBlackMarketPriceForWeapon( WEAPON_ASSAULTSUIT ) - CSGameRules()->GetBlackMarketPriceForWeapon( WEAPON_KEVLAR );
		iKevlarPrice = CSGameRules()->GetBlackMarketPriceForWeapon( WEAPON_KEVLAR );
		iNVGPrice = CSGameRules()->GetBlackMarketPriceForWeapon( WEAPON_NVG ); 
	}

	//-------------------------------------------------------------------------
	// Primary weapon
	CCSWeaponInfo *primaryInfo = GetWeaponInfo( m_primaryWeapon.GetWeaponID() );
	if ( primaryInfo )
	{
		int primaryClips = CalcClipsNeeded( &m_primaryWeapon, primaryInfo, ammo );
		int primaryWeaponCost = primaryInfo->GetWeaponPrice();
		int primaryAmmoCost = GetCSAmmoDef()->GetCost( primaryInfo->iAmmoType );
		int primaryAmmoBuySize = GetCSAmmoDef()->GetBuySize( primaryInfo->iAmmoType );
		cost += primaryWeaponCost;
		cost += primaryAmmoCost * primaryClips;
		ammo[primaryInfo->iAmmoType] += primaryClips * primaryAmmoBuySize;

		ws.m_primaryWeapon = m_primaryWeapon;
		ws.m_primaryWeapon.SetAmmoAmount( primaryClips );
		ws.m_primaryWeapon.SetAmmoType( AMMO_CLIPS );
		ws.m_primaryWeapon.SetFillAmmo( false );
	}

	//-------------------------------------------------------------------------
	// secondary weapon
	CCSWeaponInfo *secondaryInfo = GetWeaponInfo( m_secondaryWeapon.GetWeaponID() );
	if ( secondaryInfo )
	{
		int secondaryClips = CalcClipsNeeded( &m_secondaryWeapon, secondaryInfo, ammo );
		int secondaryWeaponCost = secondaryInfo->GetWeaponPrice();
		int secondaryAmmoCost = GetCSAmmoDef()->GetCost( secondaryInfo->iAmmoType );
		int secondaryAmmoBuySize = GetCSAmmoDef()->GetBuySize( secondaryInfo->iAmmoType );
		cost += secondaryWeaponCost;
		cost += secondaryAmmoCost * secondaryClips;
		ammo[secondaryInfo->iAmmoType] += secondaryClips * secondaryAmmoBuySize;

		ws.m_secondaryWeapon = m_secondaryWeapon;
		ws.m_secondaryWeapon.SetAmmoAmount( secondaryClips );
		ws.m_secondaryWeapon.SetAmmoType( AMMO_CLIPS );
		ws.m_secondaryWeapon.SetFillAmmo( false );
	}

	//-------------------------------------------------------------------------
	// equipment
	if ( m_armor )
	{
		cost += (m_helmet) ? (iKevlarPrice + iHelmetPrice) : iKevlarPrice;
		ws.m_armor = m_armor;
		ws.m_helmet = m_helmet;
	}

	if ( m_smokeGrenade )
	{
		CCSWeaponInfo *pInfo = GetWeaponInfo( WEAPON_SMOKEGRENADE );
		cost += ( pInfo ) ? pInfo->GetWeaponPrice() : 0;
		ws.m_smokeGrenade = m_smokeGrenade;
	}

	if ( m_HEGrenade )
	{
		CCSWeaponInfo *pInfo = GetWeaponInfo( WEAPON_HEGRENADE );
		cost += ( pInfo ) ? pInfo->GetWeaponPrice() : 0;
		ws.m_HEGrenade = m_HEGrenade;
	}

	CCSWeaponInfo *pInfo = GetWeaponInfo( WEAPON_FLASHBANG );
	cost += ( pInfo ) ? pInfo->GetWeaponPrice() * m_flashbangs : 0;
	ws.m_flashbangs = m_flashbangs;

	if ( m_defuser )
	{
		cost += DEFUSEKIT_PRICE;
		ws.m_defuser = m_defuser;
	}

	if ( m_nightvision )
	{
		cost += iNVGPrice;
		ws.m_nightvision = m_nightvision;
	}
}


//--------------------------------------------------------------------------------------------------------------
/**
 *  Convenience function that wraps GetFromScratch() and discards the generated WeaponSet.  Returns the cost
 *  to buy the full WeaponSet from scratch.
 */
int WeaponSet::FullCost() const
{
	WeaponSet fullSet;
	int cost = 0;
	GetFromScratch( cost, fullSet );
	return cost;
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Generates a list of buy commands that will buy the current WeaponSet.
 */
void WeaponSet::GenerateBuyCommands( char command[BUY_PRESET_COMMAND_LEN] ) const
{
	command[0] = 0;
	char *tmp = command;
	int remainder = BUY_PRESET_COMMAND_LEN;
	int i;

	CSWeaponID primaryID = GetClientWeaponID( true );		///< The local player's current primary weapon
	CSWeaponID secondaryID = GetClientWeaponID( false );	///< The local player's current pistol

	//-------------------------------------------------------------------------
	// Primary weapon
	CSWeaponID weaponID = m_primaryWeapon.GetWeaponID();
	if ( weaponID == WEAPON_NONE )
		weaponID = primaryID;
	const CCSWeaponInfo *primaryInfo = GetWeaponInfo( weaponID );
	if ( primaryInfo )
	{
		if ( weaponID != primaryID )
		{
			tmp = BufPrintf( tmp, remainder, "buy %s\n", WeaponIDToAlias(weaponID) );
		}
		for ( i=0; i<m_primaryWeapon.GetAmmoAmount(); ++i )
		{
			tmp = BufPrintf( tmp, remainder, "buyammo1\n" );
		}
	}

	//-------------------------------------------------------------------------
	// secondary weapon
	weaponID = m_secondaryWeapon.GetWeaponID();
	if ( weaponID == WEAPON_NONE )
		weaponID = secondaryID;
	const CCSWeaponInfo *secondaryInfo = GetWeaponInfo( weaponID );
	if ( secondaryInfo )
	{
		if ( weaponID != secondaryID )
		{
			tmp = BufPrintf( tmp, remainder, "buy %s\n", WeaponIDToAlias(weaponID) );
		}
		for ( i=0; i<m_secondaryWeapon.GetAmmoAmount(); ++i )
		{
			tmp = BufPrintf( tmp, remainder, "buyammo2\n" );
		}
	}

	//-------------------------------------------------------------------------
	// equipment
	if ( m_armor )
	{
		if ( m_helmet )
		{
			tmp = BufPrintf( tmp, remainder, "buy vesthelm\n" );
		}
		else
		{
			tmp = BufPrintf( tmp, remainder, "buy vest\n" );
		}
	}

	if ( m_smokeGrenade )
	{
		tmp = BufPrintf( tmp, remainder, "buy smokegrenade\n" );
	}

	if ( m_HEGrenade )
	{
		tmp = BufPrintf( tmp, remainder, "buy hegrenade\n" );
	}

	for ( i=0; i<m_flashbangs; ++i )
	{
		tmp = BufPrintf( tmp, remainder, "buy flashbang\n" );
	}

	if ( m_defuser )
	{
		tmp = BufPrintf( tmp, remainder, "buy defuser\n" );
	}

	if ( m_nightvision )
	{
		tmp = BufPrintf( tmp, remainder, "buy nvgs\n" );
	}
}


//--------------------------------------------------------------------------------------------------------------
/**
 *  Return images out of a subdir, so when the buy preset system resizes the images, they don't resize in the
 *  main buy menu.
 */
const char *ImageFnameFromWeaponID( CSWeaponID weaponID, bool isPrimary )
{
	switch (weaponID)
	{
	case WEAPON_NONE:
		return "gfx/vgui/defaultweapon";
	case WEAPON_SCOUT:
		return "gfx/vgui/scout";
	case WEAPON_XM1014:
		return "gfx/vgui/xm1014";
	case WEAPON_MAC10:
		return "gfx/vgui/mac10";
	case WEAPON_AUG:
		return "gfx/vgui/aug";
	case WEAPON_UMP45:
		return "gfx/vgui/ump45";
	case WEAPON_SG550:
		return "gfx/vgui/sg550";
	case WEAPON_GALIL:
		return "gfx/vgui/galil";
	case WEAPON_FAMAS:
		return "gfx/vgui/famas";
	case WEAPON_AWP:
		return "gfx/vgui/awp";
	case WEAPON_MP5NAVY:
		return "gfx/vgui/mp5";
	case WEAPON_M249:
		return "gfx/vgui/m249";
	case WEAPON_M3:
		return "gfx/vgui/m3";
	case WEAPON_M4A1:
		return "gfx/vgui/m4a1";
	case WEAPON_TMP:
		return "gfx/vgui/tmp";
	case WEAPON_G3SG1:
		return "gfx/vgui/g3sg1";
	case WEAPON_SG552:
		return "gfx/vgui/sg552";
	case WEAPON_AK47:
		return "gfx/vgui/ak47";
	case WEAPON_P90:
		return "gfx/vgui/p90";
	case WEAPON_SHIELDGUN:
		return "gfx/vgui/shield";

	case WEAPON_USP:
		return "gfx/vgui/usp45";
	case WEAPON_GLOCK:
		return "gfx/vgui/glock18";
	case WEAPON_DEAGLE:
		return "gfx/vgui/deserteagle";
	case WEAPON_ELITE:
		return "gfx/vgui/elites";
	case WEAPON_P228:
		return "gfx/vgui/p228";
	case WEAPON_FIVESEVEN:
		return "gfx/vgui/fiveseven";

	default:
		return "";
	}
}


//--------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------
BuyPreset::BuyPreset()
{
	SetName( L"" );
}


//--------------------------------------------------------------------------------------------------------------
BuyPreset::BuyPreset( const BuyPreset& other )
{
	*this = other;
}


//--------------------------------------------------------------------------------------------------------------
BuyPreset::~BuyPreset()
{
}


//--------------------------------------------------------------------------------------------------------------
void BuyPreset::SetName( const wchar_t *name )
{
	wcsncpy( m_name, name, MaxBuyPresetName );
	if ( m_name[0] == 0 )
	{
		const wchar_t * defaultName = g_pVGuiLocalize->Find( "#Cstrike_BuyPresetBlank" );
		if ( defaultName )
		{
			wcsncpy( m_name, defaultName, MaxBuyPresetName );
		}
	}
	m_name[MaxBuyPresetName-1] = 0;
}


//--------------------------------------------------------------------------------------------------------------
// Parse KeyValues string into a BuyPresetWeapon vector
static void ParseWeaponString( const char *str, BuyPresetWeaponList& weapons, bool isPrimary )
{
	weapons.RemoveAll();

	if ( !str )
		return;

	const char *remainder = SharedParse( str );
	const char *token;

	const int BufLen = 32;
	char tmpBuf[BufLen];
	tmpBuf[0] = '\0';
	char weaponBuf[BufLen];
	weaponBuf[0] = '\0';
	char clipModifier = 0;
	int numClips = 0;

	while ( remainder )
	{
		token = SharedGetToken();
		if ( !token || strlen(token) >= BufLen )
			return;

		Q_strncpy( tmpBuf, token, BufLen );
		tmpBuf[BufLen - 1] = 0;
		char *tmp = tmpBuf;
		while ( *tmp )
		{
			if ( *tmp == '/' )
			{
				*tmp = ' ';
			}
			++tmp;
		}
		// sscanf is safe, since the size of the string being scanned is at least as small as any individual destination buffer
		if ( sscanf( tmpBuf, "%s %d%c", weaponBuf, &numClips, &clipModifier ) != 3 )
			return;

		CSWeaponID weaponID = AliasToWeaponID( weaponBuf );
		if ( weaponID != WEAPON_NONE )
		{
			const CCSWeaponInfo *info = GetWeaponInfo( weaponID );
			if ( info )
			{
				int maxRounds = GetCSAmmoDef()->MaxCarry( info->iAmmoType );
				int buySize = GetCSAmmoDef()->GetBuySize( info->iAmmoType );
				numClips = MIN(ceil(maxRounds/(float)buySize), MAX(0, numClips));
				if ( ((!isPrimary) ^ IsPrimaryWeapon( weaponID )) == 0 )
					return;
			}
			else
			{
				numClips = MIN(NUM_CLIPS_FOR_CURRENT, MAX(0, numClips));
			}
		}
		else
		{
			numClips = MIN(NUM_CLIPS_FOR_CURRENT, MAX(0, numClips));
		}

		BuyPresetWeapon weapon( weaponID );
		weapon.SetAmmoType( AMMO_CLIPS );
		weapon.SetAmmoAmount( numClips );
		weapon.SetFillAmmo( (clipModifier == '+') );
		weapons.AddToTail( weapon );

		remainder = SharedParse( remainder );
	}
}


//--------------------------------------------------------------------------------------------------------------
/**
 *  Populates data from a KeyValues structure, for loading
 */
void BuyPreset::Parse( KeyValues *data )
{
	m_name[0] = 0;
	m_weaponList.RemoveAll();

	if (!data)
		return;

	//-------------------------------------------------------------------------
	// Read name
	wcsncpy( m_name, data->GetWString( "PresetName", L""), MaxBuyPresetName );
	m_name[ MaxBuyPresetName - 1 ] = 0;
	PRESET_DEBUG( "Parsing Buy Preset %ls\n", m_name );

	int version = data->GetInt( "Version", 0 );
	if ( version < 4 || version > 4 )
	{
		PRESET_DEBUG( "Invalid preset version %d\n", version );
		return;
	}

	const char *primaryString = data->GetString( "Primary", NULL );
	const char *secondaryString = data->GetString( "Secondary", NULL );
	const char *equipmentString = data->GetString( "Equipment", NULL );

	CUtlVector< BuyPresetWeapon > weapons;
	ParseWeaponString( primaryString, weapons, true );

	WeaponSet ws;
	if ( weapons.Count() )
		ws.m_primaryWeapon = weapons[0];

	weapons.RemoveAll();
	ParseWeaponString( secondaryString, weapons, false );
	if ( weapons.Count() )
		ws.m_secondaryWeapon = weapons[0];

	// parse equipment
	if ( equipmentString )
	{
		const char *remainder = SharedParse( equipmentString );
		const char *token;

		const int BufLen = 32;
		char tmpBuf[BufLen];
		char itemBuf[BufLen];
		int intVal;

		while ( remainder )
		{
			token = SharedGetToken();
			if ( !token || Q_strlen(token) >= BufLen )
				break;

			Q_strncpy( tmpBuf, token, BufLen );
			tmpBuf[BufLen - 1] = 0;
			char *tmp = tmpBuf;
			while ( *tmp )
			{
				if ( *tmp == '/' )
				{
					*tmp = ' ';
				}
				++tmp;
			}
			// sscanf is safe, since the size of the string being scanned is at least as small as any individual destination buffer
			if ( sscanf( tmpBuf, "%s %d", itemBuf, &intVal ) != 2 )
				break;

			if ( !strcmp( itemBuf, "vest" ) )
			{
				ws.m_armor = (intVal > 0) ? 100 : 0;
				ws.m_helmet = false;
			}
			else if ( !strcmp( itemBuf, "vesthelm" ) )
			{
				ws.m_armor = (intVal > 0) ? 100 : 0;
				ws.m_helmet = true;
			}
			else if ( !strcmp( itemBuf, "defuser" ) )
			{
				ws.m_defuser = (intVal > 0);
			}
			else if ( !strcmp( itemBuf, "nvgs" ) )
			{
				ws.m_nightvision = (intVal > 0);
			}
			else if ( !strcmp( itemBuf, "sgren" ) )
			{
				ws.m_smokeGrenade = (intVal > 0);
			}
			else if ( !strcmp( itemBuf, "hegren" ) )
			{
				ws.m_HEGrenade = (intVal > 0);
			}
			else if ( !strcmp( itemBuf, "flash" ) )
			{
				ws.m_flashbangs = MIN( 2, MAX( 0, intVal ) );
			}

			remainder = SharedParse( remainder );
		}

		m_weaponList.AddToTail( ws );
	}
}


//--------------------------------------------------------------------------------------------------------------
// Build a string of weapons+ammo for KeyValues saving/loading
static const char* ConstructWeaponString( const BuyPresetWeapon& weapon )
{
	const int WeaponLen = 1024;
	static char weaponString[WeaponLen];
	weaponString[0] = 0;
	int remainder = WeaponLen;
	char *tmp = weaponString;

	tmp = BufPrintf( tmp, remainder, "%s/%d%c",
		WeaponIDToAlias( weapon.GetWeaponID() ),
		weapon.GetAmmoAmount(),
		(weapon.GetFillAmmo()) ? '+' : '=' );
	return weaponString;
}


//--------------------------------------------------------------------------------------------------------------
/**
 *  Fills in a KeyValues structure with data, for saving
 */
void BuyPreset::Save( KeyValues *data )
{
	//-------------------------------------------------------------------------
	// Save name and version
	KeyValues *presetKey = data->CreateNewKey();
	presetKey->SetWString( "PresetName", m_name );

	/**
	 *  Version History
	 *  ---------------
	 *
	 *  PRE-RELEASE
	 *  1.  2/2004
	 *      First-pass implementation.  Allowed for multiple weapons per fallback.
	 *      Individual items could be marked optional.  Cash reserve could be specified.
	 *  2.  3/2004
	 *      Second-pass implementation.  Removed multiple weapons per fallback.  The
	 *      pistol could be marked optional.  A preset could be marked as 'use for
	 *      Autobuy'.
	 *
	 *  CZ RELEASE
	 *  3.  4/22/2004
	 *      The first released version.  Removed optional/required status.  Also
	 *      removed the cash reserve and autobuy status.  Corresponds to streamlined
	 *      editing interface mocked up by Greg Coomer.
	 *
	 *  CS:S RELEASE
	 *  4.  3/10/2004
	 *      Removed fallbacks to correspond to new UI.
	 */
	presetKey->SetInt( "Version", 4 );
	if ( m_weaponList.Count() > 0 )
	{
		//-------------------------------------------------------------------------
		// Save a fallback WeaponSet
		const WeaponSet& ws = m_weaponList[0];

		presetKey->SetString( "Primary", ConstructWeaponString( ws.m_primaryWeapon ) );
		presetKey->SetString( "Secondary", ConstructWeaponString( ws.m_secondaryWeapon ) );

		presetKey->SetString( "Equipment",
			SharedVarArgs("vest%s/%d flash/%d sgren/%d hegren/%d defuser/%d nvgs/%d",
			(ws.m_helmet)?"helm":"", ws.m_armor,
			ws.m_flashbangs,
			ws.m_smokeGrenade,
			ws.m_HEGrenade,
			ws.m_defuser,
			ws.m_nightvision
			) );
	}
}


//--------------------------------------------------------------------------------------------------------------
/**
 *  Calculates the full cost of the preset, which is exactly the full cost of the first fallback WeaponSet
 */
int BuyPreset::FullCost() const
{
	if ( m_weaponList.Count() == 0 )
		return 0;

	return m_weaponList[0].FullCost();
}


//--------------------------------------------------------------------------------------------------------------
/**
 *  Returns the specified fallback WeaponSet, or NULL if it doesn't exist
 */
const WeaponSet * BuyPreset::GetSet( int index ) const
{
	if ( index < 0 || index >= m_weaponList.Count() )
		return NULL;

	return &(m_weaponList[index]);
}


//--------------------------------------------------------------------------------------------------------------
/**
 *  Deletes a fallback
 */
void BuyPreset::DeleteSet( int index )
{
	if ( index < 0 || index >= m_weaponList.Count() )
		return;

	m_weaponList.Remove( index );
}


//--------------------------------------------------------------------------------------------------------------
/**
 *  Switches the order of two fallbacks
 */
void BuyPreset::SwapSet( int firstIndex, int secondIndex )
{
	if ( firstIndex < 0 || firstIndex >= m_weaponList.Count() )
		return;

	if ( secondIndex < 0 || secondIndex >= m_weaponList.Count() )
		return;

	WeaponSet tmpSet = m_weaponList[firstIndex];
	m_weaponList[firstIndex] = m_weaponList[secondIndex];
	m_weaponList[secondIndex] = tmpSet;
}


//--------------------------------------------------------------------------------------------------------------
/**
 *  Replaces a fallback with the supplied WeaponSet
 */
void BuyPreset::ReplaceSet( int index, const WeaponSet& weaponSet )
{
	if ( index < 0 || index > m_weaponList.Count() )
		return;

	if ( index == m_weaponList.Count() )
	{
		m_weaponList.AddToTail( weaponSet );
	}
	else
	{
		m_weaponList[index] = weaponSet;
	}
}

//--------------------------------------------------------------------------------------------------------------
