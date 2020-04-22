//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003

#include "cbase.h"
#include "cs_gamerules.h"
#include "cs_bot.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//--------------------------------------------------------------------------------------------------------------
ConVar bot_loadout( "bot_loadout", "", FCVAR_CHEAT, "bots are given these items at round start" );
ConVar bot_randombuy( "bot_randombuy", "0", FCVAR_CHEAT, "should bots ignore their prefered weapons and just buy weapons at random?" );

//--------------------------------------------------------------------------------------------------------------
/**
 *  Debug command to give a named weapon
 */
void CCSBot::GiveWeapon( const char *weaponAlias )
{
	const char *translatedAlias = GetTranslatedWeaponAlias( weaponAlias );

	char wpnName[128];
	Q_snprintf( wpnName, sizeof( wpnName ), "weapon_%s", translatedAlias );
	WEAPON_FILE_INFO_HANDLE	hWpnInfo = LookupWeaponInfoSlot( wpnName );
	if ( hWpnInfo == GetInvalidWeaponInfoHandle() )
	{
		return;
	}

	CCSWeaponInfo *pWeaponInfo = dynamic_cast< CCSWeaponInfo* >( GetFileWeaponInfoFromHandle( hWpnInfo ) );
	if ( !pWeaponInfo )
	{
		return;
	}

	if ( !Weapon_OwnsThisType( wpnName ) )
	{
		CBaseCombatWeapon *pWeapon = Weapon_GetSlot( pWeaponInfo->iSlot );
		if ( pWeapon )
		{
			if ( pWeaponInfo->iSlot == WEAPON_SLOT_PISTOL )
			{
				DropPistol();
			}
			else if ( pWeaponInfo->iSlot == WEAPON_SLOT_RIFLE )
			{
				DropRifle();
			}
		}
	}

	GiveNamedItem( wpnName );
}

//--------------------------------------------------------------------------------------------------------------
static bool HasDefaultPistol( CCSBot *me )
{
	CWeaponCSBase *pistol = (CWeaponCSBase *)me->Weapon_GetSlot( WEAPON_SLOT_PISTOL );

	if (pistol == NULL)
		return false;

	if (me->GetTeamNumber() == TEAM_TERRORIST && pistol->IsA( WEAPON_GLOCK ))
		return true;

	if (me->GetTeamNumber() == TEAM_CT && pistol->IsA( WEAPON_USP ))
		return true;

	return false;
}


//--------------------------------------------------------------------------------------------------------------
/**
 * Buy weapons, armor, etc.
 */
void BuyState::OnEnter( CCSBot *me )
{
	m_retries = 0;
	m_prefRetries = 0;
	m_prefIndex = 0;

	const char *cheatWeaponString = bot_loadout.GetString();
	if ( cheatWeaponString && *cheatWeaponString )
	{
		m_doneBuying = false; // we're going to be given weapons - ignore the eco limit
	}
	else
	{
		// check if we are saving money for the next round
		if (me->m_iAccount < cv_bot_eco_limit.GetFloat())
		{
			me->PrintIfWatched( "Saving money for next round.\n" );
			m_doneBuying = true;
		}
		else
		{
			m_doneBuying = false;
		}
	}

	m_isInitialDelay = true;

	// this will force us to stop holding live grenade
	me->EquipBestWeapon( MUST_EQUIP );

	m_buyDefuseKit = false;
	m_buyShield = false;

	if (me->GetTeamNumber() == TEAM_CT)
	{
		if (TheCSBots()->GetScenario() == CCSBotManager::SCENARIO_DEFUSE_BOMB)
		{
			// CT's sometimes buy defuse kits in the bomb scenario (except in career mode, where the player should defuse)
			if (CSGameRules()->IsCareer() == false)
			{
				const float buyDefuseKitChance = 100.0f * (me->GetProfile()->GetSkill() + 0.2f);
				if (RandomFloat( 0.0f, 100.0f ) < buyDefuseKitChance)
				{
					m_buyDefuseKit = true;
				}
			}
		}

		// determine if we want a tactical shield
		if (!me->HasPrimaryWeapon() && TheCSBots()->AllowTacticalShield())
		{
			if (me->m_iAccount > 2500)
			{
				if (me->m_iAccount < 4000)
					m_buyShield = (RandomFloat( 0, 100.0f ) < 33.3f) ? true : false;
				else
					m_buyShield = (RandomFloat( 0, 100.0f ) < 10.0f) ? true : false;
			}
		}
	}

	if (TheCSBots()->AllowGrenades())
	{
		m_buyGrenade = (RandomFloat( 0.0f, 100.0f ) < 33.3f) ? true : false;
	}
	else
	{
		m_buyGrenade = false;
	}


	m_buyPistol = false;
	if (TheCSBots()->AllowPistols())
	{
		// check if we have a pistol
		if (me->Weapon_GetSlot( WEAPON_SLOT_PISTOL ))
		{
			// if we have our default pistol, think about buying a different one
			if (HasDefaultPistol( me ))
			{
				// if everything other than pistols is disallowed, buy a pistol
				if (TheCSBots()->AllowShotguns() == false &&
					TheCSBots()->AllowSubMachineGuns() == false &&
					TheCSBots()->AllowRifles() == false &&
					TheCSBots()->AllowMachineGuns() == false &&
					TheCSBots()->AllowTacticalShield() == false &&
					TheCSBots()->AllowSnipers() == false)
				{
					m_buyPistol = (RandomFloat( 0, 100 ) < 75.0f);
				}
				else if (me->m_iAccount < 1000)
				{
					// if we're low on cash, buy a pistol
					m_buyPistol = (RandomFloat( 0, 100 ) < 75.0f);
				}
				else
				{
					m_buyPistol = (RandomFloat( 0, 100 ) < 33.3f);
				}
			}			
		}
		else
		{
			// we dont have a pistol - buy one
			m_buyPistol = true;
		}
	}
}


enum WeaponType
{
	PISTOL,
	SHOTGUN,
	SUB_MACHINE_GUN,
	RIFLE,
	MACHINE_GUN,
	SNIPER_RIFLE,
	GRENADE,

	NUM_WEAPON_TYPES
};

struct BuyInfo
{
	WeaponType type;
	bool preferred;			///< more challenging bots prefer these weapons
	const char *buyAlias;			///< the buy alias for this equipment
};

#define PRIMARY_WEAPON_BUY_COUNT 13
#define SECONDARY_WEAPON_BUY_COUNT 3

/**
 * These tables MUST be kept in sync with the CT and T buy aliases
 */

static BuyInfo primaryWeaponBuyInfoCT[ PRIMARY_WEAPON_BUY_COUNT ] =
{
	{ SHOTGUN,			false, "m3" },			// WEAPON_M3
	{ SHOTGUN,			false, "xm1014" },		// WEAPON_XM1014
	{ SUB_MACHINE_GUN,	false, "tmp" },			// WEAPON_TMP
	{ SUB_MACHINE_GUN,	false, "mp5navy" },			// WEAPON_MP5N
	{ SUB_MACHINE_GUN,	false, "ump45" },		// WEAPON_UMP45
	{ SUB_MACHINE_GUN,	false, "p90" },			// WEAPON_P90
	{ RIFLE,			true,  "famas" },		// WEAPON_FAMAS
	{ SNIPER_RIFLE,		false, "scout" },		// WEAPON_SCOUT
	{ RIFLE,			true,  "m4a1" },		// WEAPON_M4A1
	{ RIFLE,			false, "aug" },			// WEAPON_AUG
	{ SNIPER_RIFLE,		true,  "sg550" },		// WEAPON_SG550
	{ SNIPER_RIFLE,		true,  "awp" },			// WEAPON_AWP
	{ MACHINE_GUN,		false, "m249" }			// WEAPON_M249
};

static BuyInfo secondaryWeaponBuyInfoCT[ SECONDARY_WEAPON_BUY_COUNT ] =
{
//	{ PISTOL,	false,	"glock" },
//	{ PISTOL,	false,	"usp" },
	{ PISTOL, true,		"p228" },
	{ PISTOL, true,		"deagle" },
	{ PISTOL, true,		"fn57" }
};


static BuyInfo primaryWeaponBuyInfoT[ PRIMARY_WEAPON_BUY_COUNT ] =
{
	{ SHOTGUN,			false, "m3" },			// WEAPON_M3
	{ SHOTGUN,			false, "xm1014" },		// WEAPON_XM1014
	{ SUB_MACHINE_GUN,	false, "mac10" },		// WEAPON_MAC10
	{ SUB_MACHINE_GUN,	false, "mp5navy" },			// WEAPON_MP5N
	{ SUB_MACHINE_GUN,	false, "ump45" },		// WEAPON_UMP45
	{ SUB_MACHINE_GUN,	false, "p90" },			// WEAPON_P90
	{ RIFLE,			true,  "galil" },		// WEAPON_GALIL
	{ RIFLE,			true,  "ak47" },		// WEAPON_AK47
	{ SNIPER_RIFLE,		false, "scout" },		// WEAPON_SCOUT
	{ RIFLE,			true,  "sg552" },		// WEAPON_SG552
	{ SNIPER_RIFLE,		true,  "awp" },			// WEAPON_AWP
	{ SNIPER_RIFLE,		true,  "g3sg1" },		// WEAPON_G3SG1
	{ MACHINE_GUN,		false, "m249" }			// WEAPON_M249
};

static BuyInfo secondaryWeaponBuyInfoT[ SECONDARY_WEAPON_BUY_COUNT ] =
{
//	{ PISTOL,	false,	"glock" },
//	{ PISTOL,	false,	"usp" },
	{ PISTOL, true,		"p228" },
	{ PISTOL, true,		"deagle" },
	{ PISTOL, true,		"elites" }
};

/**
 * Given a weapon alias, return the kind of weapon it is
 */
inline WeaponType GetWeaponType( const char *alias )
{
	int i;
	
	for( i=0; i<PRIMARY_WEAPON_BUY_COUNT; ++i )
	{
		if (!stricmp( alias, primaryWeaponBuyInfoCT[i].buyAlias ))
			return primaryWeaponBuyInfoCT[i].type;

		if (!stricmp( alias, primaryWeaponBuyInfoT[i].buyAlias ))
			return primaryWeaponBuyInfoT[i].type;
	}

	for( i=0; i<SECONDARY_WEAPON_BUY_COUNT; ++i )
	{
		if (!stricmp( alias, secondaryWeaponBuyInfoCT[i].buyAlias ))
			return secondaryWeaponBuyInfoCT[i].type;

		if (!stricmp( alias, secondaryWeaponBuyInfoT[i].buyAlias ))
			return secondaryWeaponBuyInfoT[i].type;
	}

	return NUM_WEAPON_TYPES;
}




//--------------------------------------------------------------------------------------------------------------
void BuyState::OnUpdate( CCSBot *me )
{
	char cmdBuffer[256];

	// wait for a Navigation Mesh
	if (!TheNavMesh->IsLoaded())
		return;

	// apparently we cant buy things in the first few seconds, so wait a bit
	if (m_isInitialDelay)
	{
		const float waitToBuyTime = 0.25f;
		if (gpGlobals->curtime - me->GetStateTimestamp() < waitToBuyTime)
			return;

		m_isInitialDelay = false;
	}

	// if we're done buying and still in the freeze period, wait
	if (m_doneBuying)
	{
		if (CSGameRules()->IsMultiplayer() && CSGameRules()->IsFreezePeriod())
		{
			// make sure we're locked and loaded
			me->EquipBestWeapon( MUST_EQUIP );
			me->Reload();
			me->ResetStuckMonitor();
			return;
		}

		me->Idle();
		return;
	}

	// If we're supposed to buy a specific weapon for debugging, do so and then bail
	const char *cheatWeaponString = bot_loadout.GetString();
	if ( cheatWeaponString && *cheatWeaponString )
	{
		CUtlVector<char*, CUtlMemory<char*> > loadout;
		Q_SplitString( cheatWeaponString, " ", loadout );
		for ( int i=0; i<loadout.Count(); ++i )
		{
			const char *item = loadout[i];
			if ( FStrEq( item, "vest" ) )
			{
				me->GiveNamedItem( "item_kevlar" );
			}
			else if ( FStrEq( item, "vesthelm" ) )
			{
				me->GiveNamedItem( "item_assaultsuit" );
			}
			else if ( FStrEq( item, "defuser" ) )
			{
				if ( me->GetTeamNumber() == TEAM_CT )
				{
					me->GiveDefuser();
				}
			}
			else if ( FStrEq( item, "nvgs" ) )
			{
				me->m_bHasNightVision = true;
			}
			else if ( FStrEq( item, "primammo" ) )
			{
				me->AttemptToBuyAmmo( 0 );
			}
			else if ( FStrEq( item, "secammo" ) )
			{
				me->AttemptToBuyAmmo( 1 );
			}
			else
			{
				me->GiveWeapon( item );
			}
		}
		m_doneBuying = true;
		return;
	}


	if (!me->IsInBuyZone())
	{
		m_doneBuying = true;
		CONSOLE_ECHO( "%s bot spawned outside of a buy zone (%d, %d, %d)\n",
						(me->GetTeamNumber() == TEAM_CT) ? "CT" : "Terrorist",
						(int)me->GetAbsOrigin().x,
						(int)me->GetAbsOrigin().y,
						(int)me->GetAbsOrigin().z );
		return;
	}

	// try to buy some weapons
	const float buyInterval = 0.02f;
	if (gpGlobals->curtime - me->GetStateTimestamp() > buyInterval)
	{
		me->m_stateTimestamp = gpGlobals->curtime;

		bool isPreferredAllDisallowed = true;

		// try to buy our preferred weapons first
		if (m_prefIndex < me->GetProfile()->GetWeaponPreferenceCount() && bot_randombuy.GetBool() == false )
		{
			// need to retry because sometimes first buy fails??
			const int maxPrefRetries = 2;
			if (m_prefRetries >= maxPrefRetries)
			{
				// try to buy next preferred weapon
				++m_prefIndex;
				m_prefRetries = 0;
				return;
			}

			int weaponPreference = me->GetProfile()->GetWeaponPreference( m_prefIndex );

			// don't buy it again if we still have one from last round
			char weaponPreferenceName[32];
			Q_snprintf( weaponPreferenceName, sizeof(weaponPreferenceName), "weapon_%s", me->GetProfile()->GetWeaponPreferenceAsString( m_prefIndex ) );
			if( me->Weapon_OwnsThisType(weaponPreferenceName) )//Prefs and buyalias use the short version, this uses the long
			{
				// done with buying preferred weapon
				m_prefIndex = 9999;
				return;
			}

			if (me->HasShield() && weaponPreference == WEAPON_SHIELDGUN)
			{
				// done with buying preferred weapon
				m_prefIndex = 9999;
				return;
			}

			const char *buyAlias = NULL;

			if (weaponPreference == WEAPON_SHIELDGUN)
			{
				if (TheCSBots()->AllowTacticalShield())
					buyAlias = "shield";
			}
			else
			{
				buyAlias = WeaponIDToAlias( weaponPreference );
				WeaponType type = GetWeaponType( buyAlias );
				switch( type )
				{
					case PISTOL:
						if (!TheCSBots()->AllowPistols())
							buyAlias = NULL;
						break;

					case SHOTGUN:
						if (!TheCSBots()->AllowShotguns())
							buyAlias = NULL;
						break;

					case SUB_MACHINE_GUN:
						if (!TheCSBots()->AllowSubMachineGuns())
							buyAlias = NULL;
						break;

					case RIFLE:
						if (!TheCSBots()->AllowRifles())
							buyAlias = NULL;
						break;

					case MACHINE_GUN:
						if (!TheCSBots()->AllowMachineGuns())
							buyAlias = NULL;
						break;

					case SNIPER_RIFLE:
						if (!TheCSBots()->AllowSnipers())
							buyAlias = NULL;
						break;
				}
			}

			if (buyAlias)
			{
				Q_snprintf( cmdBuffer, 256, "buy %s\n", buyAlias );

				CCommand args;
				args.Tokenize( cmdBuffer );
				me->ClientCommand( args );

				me->PrintIfWatched( "Tried to buy preferred weapon %s.\n", buyAlias );
				isPreferredAllDisallowed = false;
			}

			++m_prefRetries;

			// bail out so we dont waste money on other equipment
			// unless everything we prefer has been disallowed, then buy at random
			if (isPreferredAllDisallowed == false)
				return;
		}

		// if we have no preferred primary weapon (or everything we want is disallowed), buy at random
		if (!me->HasPrimaryWeapon() && (isPreferredAllDisallowed || !me->GetProfile()->HasPrimaryPreference()))
		{
			if (m_buyShield)
			{
				// buy a shield
				CCommand args;
				args.Tokenize( "buy shield" );
				me->ClientCommand( args );

				me->PrintIfWatched( "Tried to buy a shield.\n" );
			}
			else 
			{
				// build list of allowable weapons to buy
				BuyInfo *masterPrimary = (me->GetTeamNumber() == TEAM_TERRORIST) ? primaryWeaponBuyInfoT : primaryWeaponBuyInfoCT;
				BuyInfo *stockPrimary[ PRIMARY_WEAPON_BUY_COUNT ];
				int stockPrimaryCount = 0;

				// dont choose sniper rifles as often
				const float sniperRifleChance = 50.0f;
				bool wantSniper = (RandomFloat( 0, 100 ) < sniperRifleChance) ? true : false;

				if ( bot_randombuy.GetBool() )
				{
					wantSniper = true;
				}

				for( int i=0; i<PRIMARY_WEAPON_BUY_COUNT; ++i )
				{
					if ((masterPrimary[i].type == SHOTGUN && TheCSBots()->AllowShotguns()) ||
						(masterPrimary[i].type == SUB_MACHINE_GUN && TheCSBots()->AllowSubMachineGuns()) ||
						(masterPrimary[i].type == RIFLE && TheCSBots()->AllowRifles()) ||
						(masterPrimary[i].type == SNIPER_RIFLE && TheCSBots()->AllowSnipers() && wantSniper) ||
						(masterPrimary[i].type == MACHINE_GUN && TheCSBots()->AllowMachineGuns()))
					{
						stockPrimary[ stockPrimaryCount++ ] = &masterPrimary[i];
					}
				}
 
				if (stockPrimaryCount)
				{
					// buy primary weapon if we don't have one
					int which;

					// on hard difficulty levels, bots try to buy preferred weapons on the first pass
					if (m_retries == 0 && TheCSBots()->GetDifficultyLevel() >= BOT_HARD && bot_randombuy.GetBool() == false )
					{
						// count up available preferred weapons
						int prefCount = 0;
						for( which=0; which<stockPrimaryCount; ++which )
							if (stockPrimary[which]->preferred)
								++prefCount;

						if (prefCount)
						{
							int whichPref = RandomInt( 0, prefCount-1 );
							for( which=0; which<stockPrimaryCount; ++which )
								if (stockPrimary[which]->preferred && whichPref-- == 0)
									break;
						}
						else
						{
							// no preferred weapons available, just pick randomly
							which = RandomInt( 0, stockPrimaryCount-1 );
						}
					}
					else
					{
						which = RandomInt( 0, stockPrimaryCount-1 );
					}

					Q_snprintf( cmdBuffer, 256, "buy %s\n", stockPrimary[ which ]->buyAlias );

					CCommand args;
					args.Tokenize( cmdBuffer );
					me->ClientCommand( args );

					me->PrintIfWatched( "Tried to buy %s.\n", stockPrimary[ which ]->buyAlias );
				}
			}
		}


		//
		// If we now have a weapon, or have tried for too long, we're done
		//
		if (me->HasPrimaryWeapon() || m_retries++ > 5)
		{
			// primary ammo
			CCommand args;
			if (me->HasPrimaryWeapon())
			{
				args.Tokenize( "buy primammo" );
				me->ClientCommand( args );
			}

			// buy armor last, to make sure we bought a weapon first
			args.Tokenize( "buy vesthelm" );
			me->ClientCommand( args );
			args.Tokenize( "buy vest" );
			me->ClientCommand( args );

			// pistols - if we have no preferred pistol, buy at random
			if (TheCSBots()->AllowPistols() && !me->GetProfile()->HasPistolPreference())
			{
				if (m_buyPistol)
				{
					int which = RandomInt( 0, SECONDARY_WEAPON_BUY_COUNT-1 );
					
					const char *what = NULL;

					if (me->GetTeamNumber() == TEAM_TERRORIST)
						what = secondaryWeaponBuyInfoT[ which ].buyAlias;
					else
						what = secondaryWeaponBuyInfoCT[ which ].buyAlias;

					Q_snprintf( cmdBuffer, 256, "buy %s\n", what );
					args.Tokenize( cmdBuffer );
					me->ClientCommand( args );


					// only buy one pistol
					m_buyPistol = false;
				}

				// make sure we have enough pistol ammo
				args.Tokenize( "buy secammo" );
				me->ClientCommand( args );
			}

			// buy a grenade if we wish, and we don't already have one
			if (m_buyGrenade && !me->HasGrenade())
			{
				if (UTIL_IsTeamAllBots( me->GetTeamNumber() ))
				{
					// only allow Flashbangs if everyone on the team is a bot (dont want to blind our friendly humans)
					float rnd = RandomFloat( 0, 100 );

					if (rnd < 10)
					{
						args.Tokenize( "buy smokegrenade" );
						me->ClientCommand( args );	// smoke grenade
					}
					else if (rnd < 35)
					{
						args.Tokenize( "buy flashbang" );
						me->ClientCommand( args );	// flashbang
					}
					else
					{
						args.Tokenize( "buy hegrenade" );
						me->ClientCommand( args );	// he grenade
					}
				}
				else
				{
					if (RandomFloat( 0, 100 ) < 10)
					{
						args.Tokenize( "buy smokegrenade" );	// smoke grenade
						me->ClientCommand( args );
					}
					else
					{
						args.Tokenize( "buy hegrenade" );	// he grenade
						me->ClientCommand( args );
					}
				}
			}

			if (m_buyDefuseKit)
			{
				args.Tokenize( "buy defuser" );
				me->ClientCommand( args );
			}

			m_doneBuying = true;
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
void BuyState::OnExit( CCSBot *me )
{
	me->ResetStuckMonitor();
	me->EquipBestWeapon();
}

