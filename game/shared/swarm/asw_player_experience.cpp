#include "cbase.h"

#ifdef CLIENT_DLL
	#define CASW_Equip_Req C_ASW_Equip_Req 
	#include "c_asw_player.h"
	#include "c_asw_marine.h"
	#include "c_asw_game_resource.h"
	#include "c_asw_marine_resource.h"
	#include "asw_input.h"
	#include "c_asw_weapon.h"
	#include "c_asw_game_resource.h"
	#include "clientmode_asw.h"
	#include "c_asw_objective.h"
	#include "c_asw_debrief_stats.h"
	#include "asw_hud_objective.h"	
	#include "asw_equip_req.h"	
	#include "achievementmgr.h"
	#include "asw_achievements.h"
	#include "asw_medal_store.h"
	#include "asw_equipment_list.h"
	#include "asw_marine_profile.h"
	#include "clientmode_asw.h"
#ifndef _X360
	#include "steam/isteamuserstats.h"
	#include "steam/isteamfriends.h"
	#include "steam/isteamutils.h"
	#include "steam/steam_api.h"
	#include "c_asw_steamstats.h"
#endif
	#define CASW_Marine C_ASW_Marine
	#define CASW_Marine_Resource C_ASW_Marine_Resource
	#define CASW_Weapon C_ASW_Weapon
	#define CASW_Debrief_Stats C_ASW_Debrief_Stats
	#define CASW_Objective C_ASW_Objective	
#else
	#include "player.h"
	#include "asw_player.h"
	#include "asw_marine.h"
	#include "asw_marine_resource.h"
	#include "asw_weapon.h"
	#include "asw_objective.h"
	#include "asw_debrief_stats.h"
	#include "asw_achievements.h"
#ifndef _X360
	#include "steam/isteamgameserverstats.h"
	#include "gameinterface.h"
#endif
#endif
#include "asw_gamerules.h"
#include "asw_shareddefs.h"
#include "asw_weapon_parse.h"
#include "asw_medals_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar asw_skill;
ConVar asw_show_xp_details( "asw_show_xp_details", "0", FCVAR_REPLICATED, "Output XP rewards to the console" );

// Experience levels. NOTE: Level shown in the UI is one higher than used in code

int g_iLevelExperience[ ASW_NUM_EXPERIENCE_LEVELS ]=
{
	1000,	// XP under this = 1evel 1 in UI
	2050,
	3150,
	4300,
	5500,	// XP under this = 1evel 5 in UI
	6750,
	8050,
	9400,
	10800,
	12250,	// XP under this = 1evel 10 in UI
	13750,
	15300,
	16900,
	18550,
	20250,	// XP under this = 1evel 15 in UI
	22000,
	23800,
	25650,
	27550,
	29500,	// XP under this = 1evel 20 in UI
	31500,
	33550,
	35650,
	37800,
	40000,	// XP under this = 1evel 25 in UI
	42250,	// XP under this = level 26 in UI, XP equal to this = level 27 in the UI
};

int g_iXPAward[ ASW_NUM_XP_TYPES ]=
{
	1000, // ASW_XP_MISSION,
	100, // ASW_XP_KILLS,
	100, // ASW_XP_TIME,
	100, // ASW_XP_FRIENDLY_FIRE,
	100, // ASW_XP_DAMAGE_TAKEN,
 	50, // ASW_XP_HEALING,
 	50, // ASW_XP_HACKING
};

// scalar applied to XP required to level, based on your current promotion
float g_flPromotionXPScale[ ASW_PROMOTION_CAP + 1 ]=
{
	1.0f,
	1.0f,
	1.0f,
	1.0f,
	2.0f,
	4.0f,
	6.0f,
};

#define ASW_MISSION_XP_AWARD_ON_FAILURE 750					// XP award divided up between objectives

// NOTE: If you change this, update the labels in CExperienceReport::OnThink too
float g_flXPDifficultyScale[5]=
{
	0.5f,	// easy
	1.0f,	// normal
	1.2f,	// hard
	1.4f,	// insane
	1.5f,	// imba
};

// Weapon unlocks

struct ASW_Weapon_Unlock
{
	ASW_Weapon_Unlock( const char *szWeaponClass, int iLevel )
	{
		m_pszWeaponClass = szWeaponClass;
		m_iLevel = iLevel;
	}
	const char *m_pszWeaponClass;
	int m_iLevel;					// player level needed to unlock
};

// Keep Equipment.res in the same order as this

ASW_Weapon_Unlock g_WeaponUnlocks[]=
{
	ASW_Weapon_Unlock( "asw_weapon_normal_armor",				1 ),//
	ASW_Weapon_Unlock( "asw_weapon_shotgun",					2 ),
	ASW_Weapon_Unlock( "asw_weapon_buff_grenade",				3 ),//
	ASW_Weapon_Unlock( "asw_weapon_tesla_gun",					4 ),
	ASW_Weapon_Unlock( "asw_weapon_hornet_barrage",				5 ),//
	ASW_Weapon_Unlock( "asw_weapon_railgun",					6 ),
	ASW_Weapon_Unlock( "asw_weapon_freeze_grenades",			7 ),//
	ASW_Weapon_Unlock( "asw_weapon_heal_gun",					8 ),
	ASW_Weapon_Unlock( "asw_weapon_stim",						9 ),//
	ASW_Weapon_Unlock( "asw_weapon_pdw",						10 ),
	ASW_Weapon_Unlock( "asw_weapon_tesla_trap",					11 ),//
	ASW_Weapon_Unlock( "asw_weapon_flamer",						12 ),
	ASW_Weapon_Unlock( "asw_weapon_electrified_armor",			13 ),//
	ASW_Weapon_Unlock( "asw_weapon_sentry_freeze",				14 ),
	ASW_Weapon_Unlock( "asw_weapon_mines",						15 ),//
	ASW_Weapon_Unlock( "asw_weapon_minigun",					16 ),
	ASW_Weapon_Unlock( "asw_weapon_flashlight",					17 ),//
	ASW_Weapon_Unlock( "asw_weapon_sniper_rifle",				18 ),
	ASW_Weapon_Unlock( "asw_weapon_fist",						19 ),//
	ASW_Weapon_Unlock( "asw_weapon_sentry_flamer",				20 ),
	ASW_Weapon_Unlock( "asw_weapon_grenades",					21 ),//
	ASW_Weapon_Unlock( "asw_weapon_chainsaw",					22 ),
	ASW_Weapon_Unlock( "asw_weapon_night_vision",				23 ),//
	ASW_Weapon_Unlock( "asw_weapon_sentry_cannon",				24 ),
	ASW_Weapon_Unlock( "asw_weapon_smart_bomb",					25 ),//
	ASW_Weapon_Unlock( "asw_weapon_grenade_launcher",			26 ),		// ASW_NUM_EXPERIENCE_LEVELS
};

// given an Experience total, this tells you the player's level
int LevelFromXP( int iExperience, int iPromotion )
{
	iExperience = MIN( iExperience, ASW_XP_CAP * g_flPromotionXPScale[ iPromotion ] );

	for ( int i = 0; i < NELEMS( g_iLevelExperience ); i++ )
	{
		int iRequiredXP = (int) ( g_flPromotionXPScale[ iPromotion ] * g_iLevelExperience[ i ] );
		if ( iExperience < iRequiredXP )
		{
			return i;
		}
	}
	return NELEMS( g_iLevelExperience );
}

void CASW_Player::CalculateEarnedXP()
{
	for ( int i = 0; i < ASW_NUM_XP_TYPES; i++ )
	{
		m_iEarnedXP[ i ] = 0;
		m_iStatNumXP[ i ] = 0;
	}

	// no XP if you don't have a marine resource
	if ( !ASWGameResource() || !ASWGameResource()->GetFirstMarineResourceForPlayer( this ) )
		return;

	// no earning XP in singleplayer
	if ( gpGlobals->maxClients <= 1 )
		return;

	if ( ASWGameRules() && ASWGameRules()->m_bCheated.Get() )
		return;
	
#ifdef CLIENT_DLL
	if ( engine->IsPlayingDemo() )
		return;

	//if ( GetClientModeASW() && !GetClientModeASW()->IsOfficialMap() )
		//return;
#endif

	int iNumObjectives = 0;
	float flNumObjectivesComplete = 0;
	float flPartialScale = 1.0f;	// this is used to scale XP rewards that should be small if you've only completed part of the mission
	if ( ASWGameRules()->GetMissionSuccess() )
	{
		// XP for completing the mission
		m_iEarnedXP[ ASW_XP_MISSION ] = g_iXPAward[ ASW_XP_MISSION ];
		m_iStatNumXP[ ASW_XP_MISSION ] = 100;
	}
	else
	{
		// if failed the mission, award XP per completed objective		
		for ( int i = 0; i < ASW_MAX_OBJECTIVES; i++ )
		{
			CASW_Objective *pObjective = ASWGameResource()->GetObjective( i );
			if ( pObjective )
			{
				iNumObjectives++;
				flNumObjectivesComplete += pObjective->GetObjectiveProgress();
			}
		}
		if ( iNumObjectives > 0 )
		{
			flPartialScale = ( flNumObjectivesComplete / (float) iNumObjectives );
			m_iEarnedXP[ ASW_XP_MISSION ] += g_iXPAward[ ASW_XP_MISSION ] * flPartialScale;
			m_iStatNumXP[ ASW_XP_MISSION ] = flPartialScale * 100.0f;
		}
		else
		{
			flPartialScale = 0.0f;
		}
	}

	// query debrief stats to see how much XP we should award based on performance
	CASW_Debrief_Stats *pDebrief = GetDebriefStats();
	if ( pDebrief )
	{
		CASW_Marine_Resource *pMR = ASWGameResource()->GetFirstMarineResourceForPlayer( this );
		if ( pMR )
		{
			int iMarineIndex = ASWGameResource()->GetMarineResourceIndex( pMR );
			if ( iMarineIndex != -1 )
			{
				// XP per kill, capped
				m_iEarnedXP[ ASW_XP_KILLS ] = clamp( pDebrief->GetKills( iMarineIndex ), 0, g_iXPAward[ ASW_XP_KILLS ] );
				m_iStatNumXP[ ASW_XP_KILLS ] = pDebrief->GetKills( iMarineIndex );

				if ( ASWGameRules()->GetMissionSuccess() )
				{
					// FF
					m_iEarnedXP[ ASW_XP_FRIENDLY_FIRE ] = clamp( g_iXPAward[ ASW_XP_FRIENDLY_FIRE ] - pDebrief->GetFriendlyFire( iMarineIndex ), 0, g_iXPAward[ ASW_XP_FRIENDLY_FIRE ] );
					m_iStatNumXP[ ASW_XP_FRIENDLY_FIRE ] = pDebrief->GetFriendlyFire( iMarineIndex );
				}

				if ( ASWGameRules()->GetMissionSuccess() )
				{
					// damage taken
					m_iEarnedXP[ ASW_XP_DAMAGE_TAKEN ] = clamp( g_iXPAward[ ASW_XP_DAMAGE_TAKEN ] - pDebrief->GetDamageTaken( iMarineIndex ), 0, g_iXPAward[ ASW_XP_DAMAGE_TAKEN ] );
					m_iStatNumXP[ ASW_XP_DAMAGE_TAKEN ] = pDebrief->GetDamageTaken( iMarineIndex );
				}

				// healing
				m_iEarnedXP[ ASW_XP_HEALING ] = clamp( pDebrief->GetHealthHealed( iMarineIndex ) / 10, 0, g_iXPAward[ ASW_XP_HEALING ] );
				m_iStatNumXP[ ASW_XP_HEALING ] = pDebrief->GetHealthHealed( iMarineIndex );

				// hacking
				if ( pDebrief->GetFastHacks( iMarineIndex ) >= 2 )
				{
					m_iEarnedXP[ ASW_XP_HACKING ] = g_iXPAward[ ASW_XP_HACKING ];
				}
				else if ( pDebrief->GetFastHacks( iMarineIndex ) >= 1 )
				{
					m_iEarnedXP[ ASW_XP_HACKING ] = g_iXPAward[ ASW_XP_HACKING ] / 2;
				}
				else
				{
					m_iEarnedXP[ ASW_XP_HACKING ] = 0;
				}
				m_iStatNumXP[ ASW_XP_HACKING ] = pDebrief->GetFastHacks( iMarineIndex );
			}

			if ( ASWGameRules()->GetMissionSuccess() )
			{
				// time
				int speedrun_time = pDebrief->GetSpeedrunTime();
				int nTimeTaken = pDebrief->GetTimeTaken();

				// award full XP if you get less than the speedrun_time
				const float flMaxTime = 5.0f * 60.0f;	// max of 5 minutes over
				if ( nTimeTaken <= speedrun_time )
				{
					m_iEarnedXP[ ASW_XP_TIME ] = g_iXPAward[ ASW_XP_TIME ];
					m_iStatNumXP[ ASW_XP_TIME ] = nTimeTaken;
				}
				else
				{
					float flReduction = 1.0f - clamp( static_cast<float>( nTimeTaken - speedrun_time ) / flMaxTime, 0.0f, 1.0f );
					m_iEarnedXP[ ASW_XP_TIME ] = g_iXPAward[ ASW_XP_TIME ] * flReduction;
					m_iStatNumXP[ ASW_XP_TIME ] = nTimeTaken;
				}
			}

			// medals XP
#ifdef GAME_DLL
			const char *szMedalString = pMR->m_MedalsAwarded.Get();
#else
			const char *szMedalString = pMR->m_MedalsAwarded;
#endif
			const char	*p = szMedalString;
			char		token[128];

			p = nexttoken( token, p, ' ' );
			while ( Q_strlen( token ) > 0 )  
			{
				int iMedalIndex = atoi(token);

				m_iEarnedXP[ ASW_XP_MEDALS ] += GetXPForMedal( iMedalIndex );
				
				if (p)
					p = nexttoken( token, p, ' ' );
				else
					token[0] = '\0';
			}

			// achievement XP
#ifdef GAME_DLL
			CUtlVector<int> *pAchievementsEarned = &pMR->m_aAchievementsEarned;
#else
			CUtlVector<int> *pAchievementsEarned = &m_aNonLocalPlayerAchievementsEarned;
			if ( IsLocalPlayer() )
			{
				pAchievementsEarned = &GetClientModeASW()->m_aAchievementsEarned;
			}
#endif
			for ( int i = 0; i < pAchievementsEarned->Count(); i++ )
			{
				m_iEarnedXP[ ASW_XP_MEDALS ] += GetXPForMedal( -(*pAchievementsEarned)[i] );
			}
		}
	}

	for ( int i = 0; i < ASW_XP_TOTAL; i++ )
	{
		m_iEarnedXP[ ASW_XP_TOTAL ] += m_iEarnedXP[ i ];
	}

	// apply difficulty bonus
	if ( ASWGameRules() )
	{
		m_iEarnedXP[ ASW_XP_TOTAL ] *= g_flXPDifficultyScale[ ASWGameRules()->GetSkillLevel() - 1 ];
	}

	if ( asw_show_xp_details.GetBool() )
	{
		Msg( "[%s] Awarding XP to player %s:\n", IsServerDll() ? "Server" : "Client", GetPlayerName() );
		Msg( "   EarnedXP[ ASW_XP_MISSION ] = %d\n", m_iEarnedXP[ ASW_XP_MISSION ] );
		Msg( "   EarnedXP[ ASW_XP_KILLS ] = %d\n", m_iEarnedXP[ ASW_XP_KILLS ] );
		Msg( "   EarnedXP[ ASW_XP_TIME ] = %d\n", m_iEarnedXP[ ASW_XP_TIME ] );
		Msg( "   EarnedXP[ ASW_XP_FRIENDLY_FIRE ] = %d\n", m_iEarnedXP[ ASW_XP_FRIENDLY_FIRE ] );
		Msg( "   EarnedXP[ ASW_XP_DAMAGE_TAKEN ] = %d\n", m_iEarnedXP[ ASW_XP_DAMAGE_TAKEN ] );
		Msg( "   EarnedXP[ ASW_XP_HEALING ] = %d\n", m_iEarnedXP[ ASW_XP_HEALING ] );
		Msg( "   EarnedXP[ ASW_XP_HACKING ] = %d\n", m_iEarnedXP[ ASW_XP_HACKING ] );
		Msg( "   EarnedXP[ ASW_XP_MEDALS ] = %d\n", m_iEarnedXP[ ASW_XP_MEDALS ] );
		Msg( "   EarnedXP[ ASW_XP_TOTAL ] = %d (Difficulty scale = %f)\n", m_iEarnedXP[ ASW_XP_TOTAL ], ASWGameRules() ? ( ASWGameRules()->GetSkillLevel() - 1 ) : 1.0f );
	}
}


int CASW_Player::GetLevel()
{
	return LevelFromXP( GetExperience(), GetPromotion() );
}


int CASW_Player::GetLevelBeforeDebrief()
{
	return LevelFromXP( GetExperienceBeforeDebrief(), GetPromotion() );
}

void CASW_Player::RequestExperience()
{
#ifdef CLIENT_DLL
	// pull XP/level out of the medal store.  Steam stats will overwrite these numbers if stat get is successful and USE_XP_FROM_STEAM is defined
	if ( IsLocalPlayer() && GetMedalStore() )
	{
		m_iExperience = GetMedalStore()->GetExperience();
		m_iPromotion = GetMedalStore()->GetPromotion();
	}
#endif
#if !defined(NO_STEAM)

#ifdef CLIENT_DLL

#if !defined(USE_XP_FROM_STEAM)
	// if we're not pulling XP from Steam stats, then we don't request it for other players
	//  (instead we wait for the server to network their XP)
	if ( !IsLocalPlayer() )
		return;
#endif

	Assert( steamapicontext->SteamUserStats() );
	if ( steamapicontext->SteamUserStats() )
	{
		if ( IsLocalPlayer( this ) )
		{
			steamapicontext->SteamUserStats()->RequestCurrentStats();
		}
		else
		{
			player_info_t pi;
			if ( engine->GetPlayerInfo( entindex(), &pi ) && pi.friendsID )
			{
				CSteamID steamID( pi.friendsID, 1, steamapicontext->SteamUtils()->GetConnectedUniverse(), k_EAccountTypeIndividual );
				Msg( "Requesting Steam stats for %s (%s)\n", pi.name ? pi.name : "NULL", pi.friendsName ? pi.friendsName : "NULL" );
				steamapicontext->SteamUserStats()->RequestUserStats( steamID );
			}
		}
		m_bPendingSteamStats = true;
		m_flPendingSteamStatsStart = gpGlobals->curtime;
	}	
#else
	Assert( Steam3Server().SteamGameServerStats() );
	if ( Steam3Server().SteamGameServerStats() )
	{
		CSteamID steamID;
		if ( GetSteamID( &steamID ) )
		{
			SteamAPICall_t hSteamAPICall = Steam3Server().SteamGameServerStats()->RequestUserStats( steamID );
			if ( hSteamAPICall != 0 )
			{
				m_CallbackGSStatsReceived.Set( hSteamAPICall, this, &CASW_Player::Steam_OnGSStatsReceived );
			}
		}
		m_bPendingSteamStats = true;
		m_flPendingSteamStatsStart = gpGlobals->curtime;
	}
#endif

#endif
}

void CASW_Player::AwardExperience()
{
#ifdef CLIENT_DLL
	if ( !GetClientModeASW() || GetClientModeASW()->HasAwardedExperience( this ) || !ASWGameRules() || !ASWGameResource() )
		return;
#else
	if ( m_bHasAwardedXP || !ASWGameRules() || !ASWGameResource() )
		return;
#endif

	if ( IsX360() )
		return;
	
	m_iExperience = GetExperience();		// make sure m_iExperience has the correct number (no change if local player, otherwise using networked value)
	m_iExperienceBeforeDebrief = m_iExperience;

	CalculateEarnedXP();

	Msg( "%s: AwardExperience: Pre XP is %d\n", IsServerDll() ? "S" : "C", m_iExperience );

	m_iExperience += m_iEarnedXP[ ASW_XP_TOTAL ];
	m_iExperience = MIN( m_iExperience, ASW_XP_CAP * g_flPromotionXPScale[ GetPromotion() ] );

#ifdef CLIENT_DLL
	if ( IsLocalPlayer() )
	{
		#if !defined(NO_STEAM)
		// only upload if Steam is running
		if ( steamapicontext->SteamUserStats() )
		{
			if( GetLocalASWPlayer() == this )
				g_ASW_Steamstats.PrepStatsForSend( this );
			steamapicontext->SteamUserStats()->SetStat( "experience", m_iExperience );
			g_ASW_AchievementMgr.UploadUserData( GetSplitScreenPlayerSlot() );

			if ( GetMedalStore() )
			{
				GetMedalStore()->SetExperience( m_iExperience );
				GetMedalStore()->SetPromotion( m_iPromotion );
				GetMedalStore()->SaveMedalStore();
			}
		}
		#endif	// NO_STEAM
		GetClientModeASW()->SetAwardedExperience( this );
	}
#else
	m_bHasAwardedXP = true;
#endif

	Msg( "%s: Awarded %d XP for player %s (total is now %d)\n", IsServerDll() ? "S" : "C", m_iEarnedXP[ ASW_XP_TOTAL ], GetPlayerName(), m_iExperience );
}

#ifdef CLIENT_DLL
#ifdef _DEBUG
ConVar asw_unlock_all_weapons( "asw_unlock_all_weapons", "0", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "If enabled, all weapons will be available in the briefing" );
#endif

int CASW_Player::GetWeaponLevelRequirement( const char *szWeaponClass )
{
	for ( int i = 0; i < NELEMS( g_WeaponUnlocks ); i++ )
	{
		if ( !Q_stricmp( szWeaponClass, g_WeaponUnlocks[ i ].m_pszWeaponClass ) )
		{
			return g_WeaponUnlocks[ i ].m_iLevel;
		}
	}
	return -1;
}

bool CASW_Player::IsWeaponUnlocked( const char *szWeaponClass )
{
#ifdef _DEBUG
	if ( asw_unlock_all_weapons.GetBool() )
		return true;
#endif

	if ( C_ASW_Equip_Req::ForceWeaponUnlocked( szWeaponClass ) )
		return true;

	int iPlayerLevel = GetLevel();
	int iWeaponReq = GetWeaponLevelRequirement( szWeaponClass );

	if ( iWeaponReq == -1 )
	{
		// if it's not in the list of unlocked weapons, assume it's unlocked by default
		return true;
	}

	return iWeaponReq <= iPlayerLevel;
}

const char* CASW_Player::GetNextWeaponClassUnlocked()
{
	int iPlayerLevel = GetLevel();
	for ( int i = 0; i < NELEMS( g_WeaponUnlocks ); i++ )
	{
		//Msg( "GetNextWeaponClassUnlocked: player level = %d weapon %s level = %d\n", iPlayerLevel, g_WeaponUnlocks[ i ].m_pszWeaponClass, g_WeaponUnlocks[ i ].m_iLevel );
		if ( g_WeaponUnlocks[ i ].m_iLevel > iPlayerLevel )
		{
			return g_WeaponUnlocks[ i ].m_pszWeaponClass;
		}
	}
	return "";
}

const char *CASW_Player::GetWeaponUnlockedAtLevel( int nLevel )
{
	for ( int i = 0; i < NELEMS( g_WeaponUnlocks ); i++ )
	{
		if ( g_WeaponUnlocks[ i ].m_iLevel == nLevel )
		{
			return g_WeaponUnlocks[ i ].m_pszWeaponClass;
		}
	}
	return "";
}

#if !defined(NO_STEAM)
//-----------------------------------------------------------------------------
// Purpose: called when stat download is complete
//-----------------------------------------------------------------------------
void CASW_Player::Steam_OnUserStatsReceived( UserStatsReceived_t *pUserStatsReceived )
{
	Assert( steamapicontext->SteamUserStats() );
	if ( !steamapicontext->SteamUserStats() )
		return;

	if ( pUserStatsReceived->m_eResult != k_EResultOK )
	{
		//Msg( "CASW_Player: failed to download stats from Steam, EResult %d\n", pUserStatsReceived->m_eResult );
		//Msg( " m_nGameID = %I64u\n", pUserStatsReceived->m_nGameID );
		//Msg( " SteamID = %I64u\n", pUserStatsReceived->m_steamIDUser.ConvertToUint64() );
		m_bPendingSteamStats = false;
		return;
	}

	CSteamID steamID;
	if ( IsLocalPlayer() )
	{
		steamID = steamapicontext->SteamUser()->GetSteamID();
	}
	else
	{
		player_info_t pi;
		if ( !engine->GetPlayerInfo( entindex(), &pi ) )
			return;

		if ( !pi.friendsID )
			return;
			
		CSteamID steamIDForPlayer( pi.friendsID, 1, steamapicontext->SteamUtils()->GetConnectedUniverse(), k_EAccountTypeIndividual );
		steamID = steamIDForPlayer;

		Msg( "Steam_OnUserStatsReceived for non local player %s (%s)\n", pi.name ? pi.name : "NULL", pi.friendsName ? pi.friendsName : "NULL" );
	}
	if ( steamID != pUserStatsReceived->m_steamIDUser )
		return;

#ifdef USE_XP_FROM_STEAM
	if ( steamapicontext->SteamUserStats()->GetUserStat( steamID, "experience", &m_iExperience ) )
	{
		m_bPendingSteamStats = false;
	}
	else
	{
		Msg( "Error retrieving experience stat for player %s.\n", GetPlayerName() );
	}

	if ( !steamapicontext->SteamUserStats()->GetUserStat( steamID, "promotion", &m_iPromotion ) )
	{
		Msg( "Error retrieving promotion stat for player %s.\n", GetPlayerName() );
	}
#else
	if ( IsLocalPlayer( this ) )
	{
		if ( GetMedalStore() )
		{
			if ( !GetMedalStore()->m_bFoundNewClientDat )		// if we failed to find the new client dat, then take steam numbers
			{
				if ( steamapicontext->SteamUserStats()->GetUserStat( steamID, "experience", &m_iExperience ) )
				{
					m_bPendingSteamStats = false;
				}
				else
				{
					Msg( "Error retrieving experience stat for player %s.\n", GetPlayerName() );
				}

				if ( !steamapicontext->SteamUserStats()->GetUserStat( steamID, "promotion", &m_iPromotion ) )
				{
					Msg( "Error retrieving promotion stat for player %s.\n", GetPlayerName() );
				}

				KeyValues *kv = new KeyValues( "XPUpdate" );
				kv->SetInt( "xp", m_iExperience );
				kv->SetInt( "pro", m_iPromotion );
				engine->ServerCmdKeyValues( kv );		// kv gets deleted in here
			}
		}
	}
	m_bPendingSteamStats = false;
#endif

	// Fetch the rest of the steam stats here
	if( GetLocalASWPlayer() == this )
		g_ASW_Steamstats.FetchStats( steamID, this );

	if ( IsLocalPlayer() && GetLevel() >= ASW_NUM_EXPERIENCE_LEVELS )
	{
		CAchievementMgr *pAchievementMgr = dynamic_cast<CAchievementMgr *>( engine->GetAchievementMgr() );
		if ( !pAchievementMgr )
			return;
		pAchievementMgr->OnAchievementEvent( ACHIEVEMENT_ASW_UNLOCK_ALL_WEAPONS, STEAM_PLAYER_SLOT );
	}
}

void CASW_Player::Steam_OnUserStatsStored( UserStatsStored_t *pUserStatsStored )
{
	if ( !IsLocalPlayer( this ) )
		return;

	CSteamID steamID = steamapicontext->SteamUser()->GetSteamID();

	if ( k_EResultOK != pUserStatsStored->m_eResult )
	{
		DevMsg( "CASW_Player: failed to upload stats to Steam, EResult %d\n", pUserStatsStored->m_eResult );
#ifdef USE_XP_FROM_STEAM
		steamapicontext->SteamUserStats()->GetStat( "experience", &m_iExperience );
		steamapicontext->SteamUserStats()->GetStat( "promotion", &m_iPromotion );
#endif

		// Set stats to whatever is stored on steam
		if( GetLocalASWPlayer() == this )
			g_ASW_Steamstats.FetchStats( steamID, this );
	} 
}
#endif

void asw_debug_xp_f()
{
	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if ( pPlayer )
	{
		Msg( "Local player Level = %d\n", pPlayer->GetLevel() );
		Msg( "Local player XP = %d\n", pPlayer->GetExperience() );
		Msg( "Local player earned XP = %d\n", pPlayer->GetEarnedXP( ASW_XP_TOTAL ) );
		Msg( "Local player promotion level = %d\n", pPlayer->GetPromotion() );
	}
}
ConCommand asw_debug_xp( "asw_debug_xp", asw_debug_xp_f, "Lists XP details for local player", FCVAR_NONE );



void CASW_Player::AcceptPromotion()
{
	if ( GetExperience() < ASW_XP_CAP * g_flPromotionXPScale[ GetPromotion() ] )
		return;

	if ( GetPromotion() >= ASW_PROMOTION_CAP )
		return;

	m_iExperience = 0;
	m_iPromotion++;

#if !defined(NO_STEAM)
	// only upload if Steam is running
	if ( steamapicontext->SteamUserStats() )
	{
		steamapicontext->SteamUserStats()->SetStat( "experience", m_iExperience );
		steamapicontext->SteamUserStats()->SetStat( "promotion", m_iPromotion );
		steamapicontext->SteamUserStats()->SetStat( "level", 1 );
		steamapicontext->SteamUserStats()->SetStat( "level.xprequired", 0 );
		g_ASW_AchievementMgr.UploadUserData( GetSplitScreenPlayerSlot() );

		if ( GetMedalStore() )
		{
			GetMedalStore()->ClearNewWeapons();
			GetMedalStore()->SetExperience( m_iExperience );
			GetMedalStore()->SetPromotion( m_iPromotion );
			GetMedalStore()->SaveMedalStore();
		}
		KeyValues *kv = new KeyValues( "XPUpdate" );
		kv->SetInt( "xp", m_iExperience );
		kv->SetInt( "pro", m_iPromotion );
		engine->ServerCmdKeyValues( kv );		// kv gets deleted in here
	}
#endif	// NO_STEAM

	CLocalPlayerFilter filter;
	C_BaseEntity::EmitSound( filter, -1 /*SOUND_FROM_LOCAL_PLAYER*/, "ASW_XP.LevelUp" );
	engine->ClientCmd( VarArgs( "cl_promoted %d", m_iPromotion ) );

	// reset the player's selected equipment
	if ( !ASWGameResource() || !ASWEquipmentList() )
		return;

	C_ASW_Marine_Resource *pMR = ASWGameResource()->GetFirstMarineResourceForPlayer( this );
	if ( !pMR )
		return;

	CASW_Marine_Profile *pProfile = pMR->GetProfile();
	if ( !pProfile )
		return;

	for ( int i = 0; i < ASW_NUM_INVENTORY_SLOTS; i++ )
	{
		const char *szWeaponClass = pProfile->m_DefaultWeaponsInSlots[ i ];
		int nWeaponIndex = ASWEquipmentList()->GetIndexForSlot( i, szWeaponClass );
		engine->ClientCmd( VarArgs( "cl_loadout %d %d %d", pProfile->m_ProfileIndex, i, nWeaponIndex ) );
	}
}

#else
// SERVER
#if !defined(NO_STEAM)

void CASW_Player::Steam_OnGSStatsReceived( GSStatsReceived_t *pGSStatsReceived, bool bError )
{
	Assert( Steam3Server().SteamGameServerStats() );
	if ( !Steam3Server().SteamGameServerStats() )
		return;

	if ( pGSStatsReceived->m_eResult != k_EResultOK )
	{
		Msg( "CASW_Player: Server failed to download stats from Steam, EResult %d\n", pGSStatsReceived->m_eResult );
		//Msg( " SteamID = %I64u\n", pGSStatsReceived->m_steamIDUser.ConvertToUint64() );
		m_bPendingSteamStats = false;
		return;
	}

	CSteamID steamIDForPlayer;
	if ( !GetSteamID( &steamIDForPlayer ) )
		return;
	
	if ( steamIDForPlayer != pGSStatsReceived->m_steamIDUser )
		return;

#ifdef USE_XP_FROM_STEAM
	if ( Steam3Server().SteamGameServerStats()->GetUserStat( steamIDForPlayer, "experience", &m_iExperience ) )
	{
		m_bPendingSteamStats = false;
	}
	else
	{
		Msg( "Server error retrieving experience stat for player %s.\n", GetPlayerName() );
	}
	Steam3Server().SteamGameServerStats()->GetUserStat( steamIDForPlayer, "promotion", &m_iPromotion )
#endif
}
#endif	// NO_STEAM
#endif

int GetXPForMedal( int nMedal )
{
	return 50;  // TODO: Different XP for different medal types?
}

int CASW_Player::GetExperience()
{
#ifdef USE_XP_FROM_STEAM
	return m_iExperience;		// stat taken from steam stats
#else

#ifdef CLIENT_DLL
	if ( IsLocalPlayer() )
	{
		return m_iExperience;	// XP read from medal store
	}
#endif
	return m_iNetworkedXP.Get();	// XP networked down from the server

#endif
}

int CASW_Player::GetPromotion()
{
#ifdef USE_XP_FROM_STEAM
	return m_iPromotion;		// stat taken from steam stats
#else

#ifdef CLIENT_DLL
	if ( IsLocalPlayer() )
	{
		return m_iPromotion;	// XP read from medal store
	}
#endif
	return m_iNetworkedPromotion.Get();	// XP networked down from the server

#endif
}