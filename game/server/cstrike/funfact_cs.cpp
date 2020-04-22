//========= Copyright Valve Corporation, All rights reserved. ============//
#include "cbase.h"
#include "cs_gamerules.h"
#include "cs_gamestats.h"
#include "funfactmgr_cs.h"
#include "funfact_cs.h"
#include "../../game/shared/cstrike/weapon_csbase.h"
#include "cs_achievement_constants.h"

#define FIRST_BLOOD_TIME				45.0f
#define FIRST_KILL_TIME					45.0f
#define SHORT_ROUND_TIME				30.0f
#define MIN_SHOTS_FOR_ACCURACY			10

enum FunFactId
{
	FUNFACT_CT_WIN_NO_KILLS,
	FUNFACT_T_WIN_NO_KILLS,
	FUNFACT_KILL_DEFUSER,
	FUNFACT_KILL_RESCUER,
	FUNFACT_T_WIN_NO_CASUALTIES,
	FUNFACT_CT_WIN_NO_CASUALTIES,
	FUNFACT_DAMAGE_WITH_GRENADES,
	FUNFACT_KILLS_WITH_GRENADES,
	FUNFACT_KILLS_WITH_SINGLE_GRENADE,
	FUNFACT_DAMAGE_NO_KILLS,
	FUNFACT_KILLED_ENEMIES,
	FUNFACT_FIRST_KILL,
	FUNFACT_FIRST_BLOOD,
	FUNFACT_SHORT_ROUND,
	FUNFACT_BEST_ACCURACY,
	FUNFACT_KNIFE_KILLS,
	FUNFACT_BLIND_KILLS,
	FUNFACT_KILLS_WITH_LAST_ROUND,
	FUNFACT_DONATED_WEAPONS,
	FUNFACT_POSTHUMOUS_KILLS_WITH_GRENADE,    
	FUNFACT_KNIFE_IN_GUNFIGHT,
	FUNFACT_NUM_TIMES_JUMPED,
	FUNFACT_FALL_DAMAGE,
	FUNFACT_ITEMS_PURCHASED,
	FUNFACT_WON_AS_LAST_MEMBER,
	FUNFACT_NUMBER_OF_OVERKILLS,
	FUNFACT_SHOTS_FIRED,
	FUNFACT_MONEY_SPENT,
	FUNFACT_SURVIVED_MULTIPLE_ATTACKERS,
	FUNFACT_DIED_FROM_MULTIPLE_ATTACKERS,
	FUNFACT_DAMAGE_MULTIPLE_ENEMIES,
	FUNFACT_GRENADES_THROWN,
	FUNFACT_USED_ALL_AMMO,
	FUNFACT_DEFENDED_BOMB,
	FUNFACT_ITEMS_DROPPED_VALUE,
	FUNFACT_KILL_WOUNDED_ENEMIES,
	FUNFACT_USED_MULTIPLE_WEAPONS,
	FUNFACT_TERRORIST_ACCURACY,
	FUNFACT_CT_ACCURACY,
	FUNFACT_SAME_UNIFORM_TERRORIST,
	FUNFACT_SAME_UNIFORM_CT,
	FUNFACT_BEST_TERRORIST_ACCURACY,
	FUNFACT_BEST_COUNTERTERRORIST_ACCURACY,
	FUNFACT_FALLBACK1,
	FUNFACT_FALLBACK2,
	FUNFACT_KILLS_HEADSHOTS,
	FUNFACT_BROKE_WINDOWS,
	FUNFACT_NIGHTVISION_DAMAGE,
    FUNFACT_DEFUSED_WITH_DROPPED_KIT,
    FUNFACT_KILLED_HALF_OF_ENEMIES,
};


CFunFactHelper *CFunFactHelper::s_pFirst = NULL;


//=============================================================================
// Generic evaluation Fun Fact
// This fun fact will evaluate the specified function to determine when it is
// valid. This is basically just a glue class for simple evaluation functions.
//=============================================================================

// Function type that we use to evaluate our fun facts.  The data is returned as ints then floats that are passed in as reference parameters
typedef bool (*fFunFactEval)( int &iPlayer, int &data1, int &data2, int &data3 );

class CFunFact_GenericEvalFunction : public FunFactEvaluator
{
public:
	CFunFact_GenericEvalFunction(FunFactId id, const char* szLocalizationToken, float fCoolness, fFunFactEval pfnEval ) :
		FunFactEvaluator(id, szLocalizationToken, fCoolness),
		m_pfnEval(pfnEval)
		{}

	virtual bool Evaluate( FunFactVector& results ) const
	{
		FunFact funfact;
		if (m_pfnEval(funfact.iPlayer, funfact.iData1, funfact.iData2, funfact.iData3))
		{
			funfact.id = GetId();
			funfact.szLocalizationToken = GetLocalizationToken();
			funfact.fMagnitude = 0.0f;
			results.AddToTail(funfact);
			return true;
		}
		else
			return false;
	}

	private:
		fFunFactEval m_pfnEval;
};
#define DECLARE_FUNFACT_EVALFUNC(funfactId, szLocalizationToken, fCoolness, pfnEval)			\
static FunFactEvaluator *CreateFunFact_##funfactId( void )									\
{																								\
	return new CFunFact_GenericEvalFunction(funfactId, szLocalizationToken, fCoolness, pfnEval);\
};																								\
static CFunFactHelper g_##funfactId##_Helper( CreateFunFact_##funfactId );


//=============================================================================
// Per-player evaluation Fun Fact
// Evaluate the function per player and generate a fun fact for each valid or
// highest valid player
//=============================================================================

namespace EvalFlags
{
	enum Type
	{
		All					= 0x00,
		TeamCT				= 0x01,
		TeamTerrorist		= 0x02,
		HighestOnly			= 0x04,	// when not set, generates fun facts for all valid testees
		Alive				= 0x08,
		Dead				= 0x10,
		WinningTeam			= 0x20,
		LosingTeam			= 0x40,
	};
};


bool PlayerQualifies( const CBasePlayer* pPlayer, int flags )
{
	if ( (flags & EvalFlags::TeamCT) && pPlayer->GetTeamNumber() != TEAM_CT )
		return false;
	if ( (flags & EvalFlags::TeamTerrorist) && pPlayer->GetTeamNumber() != TEAM_TERRORIST )
		return false;
	if ( (flags & EvalFlags::Dead) && const_cast<CBasePlayer*>(pPlayer)->IsAlive() )	// IsAlive() really isn't const correct
		return false;
	if ( (flags & EvalFlags::Alive) && !const_cast<CBasePlayer*>(pPlayer)->IsAlive() )
		return false;
	if ( (flags & EvalFlags::WinningTeam) && pPlayer->GetTeamNumber() != CSGameRules()->m_iRoundWinStatus )
		return false;
	if ( (flags & EvalFlags::LosingTeam) && pPlayer->GetTeamNumber() == CSGameRules()->m_iRoundWinStatus )
		return false;
	return true;
}


typedef int (*PlayerEvalFunction)(CCSPlayer* pPlayer);

class CFunFact_PlayerEvalFunction : public FunFactEvaluator
{
public:
	CFunFact_PlayerEvalFunction(FunFactId id, const char* szLocalizationToken, float fCoolness, PlayerEvalFunction pfnEval, 
		int iMin, int flags ) : 
	FunFactEvaluator(id, szLocalizationToken, fCoolness),
		m_pfnEval(pfnEval),
		m_min(iMin),
		m_flags(flags)
	{}

	virtual bool Evaluate( FunFactVector& results ) const
	{
		int iBestValue = 0;
		int iBestPlayer = 0;
		bool bResult = false;

		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CCSPlayer* pPlayer = ToCSPlayer(UTIL_PlayerByIndex( i ) );
			if ( pPlayer )
			{
				if (!PlayerQualifies(pPlayer, m_flags))
					continue;

				int iValue = m_pfnEval(pPlayer);

				if (m_flags & EvalFlags::HighestOnly)
				{
					if ( iValue > iBestValue )
					{
						iBestValue = iValue;
						iBestPlayer = i;
					}
				}
				else
				{
					// generate fun facts for any player who meets the validation requirement
					if ( iValue >= m_min )
					{
						FunFact funfact;
						funfact.id = GetId();
						funfact.szLocalizationToken = GetLocalizationToken();
						funfact.iPlayer = i;
						funfact.iData1 = iValue;
						funfact.fMagnitude = 1.0f - ((float)m_min / iValue);
						results.AddToTail(funfact);
						bResult = true;
					}
				}
			}
		}
		if ( (m_flags & EvalFlags::HighestOnly) && iBestValue >= m_min )
		{
			FunFact funfact;
			funfact.id = GetId();
			funfact.szLocalizationToken = GetLocalizationToken();
			funfact.iPlayer = iBestPlayer;
			funfact.iData1 = iBestValue;
			funfact.fMagnitude = 1.0f - ((float)m_min / iBestValue);

			results.AddToTail(funfact);
			bResult = true;
		}
		return bResult;
	}

private:
	PlayerEvalFunction	m_pfnEval;
	int					m_min;
	int					m_flags;
};
#define DECLARE_FUNFACT_PLAYERFUNC(funfactId, szLocalizationToken, fCoolness, pfnEval, iMin, iFlags)			\
static FunFactEvaluator *CreateFunFact_##funfactId( void )													\
{																												\
	return new CFunFact_PlayerEvalFunction(funfactId, szLocalizationToken, fCoolness, pfnEval,	iMin, iFlags);	\
};																												\
static CFunFactHelper g_##funfactId##_Helper( CreateFunFact_##funfactId );


//=============================================================================
// Per-team evaluation Fun Fact
//=============================================================================

typedef bool (*TeamEvalFunction)(int iTeam, int &data1, int &data2, int &data3);

class CFunFact_TeamEvalFunction : public FunFactEvaluator
{
public:
	CFunFact_TeamEvalFunction(FunFactId id, const char* szLocalizationToken, float fCoolness, TeamEvalFunction pfnEval, int iTeam ) : 
	  FunFactEvaluator(id, szLocalizationToken, fCoolness),
		  m_pfnEval(pfnEval),
		  m_team(iTeam)
	  {}

	  virtual bool Evaluate( FunFactVector& results ) const
	  {
		  int iData1, iData2, iData3;
		  if ( m_pfnEval(m_team, iData1, iData2, iData3) )
		  {
			  FunFact funfact;
			  funfact.id = GetId();
			  funfact.szLocalizationToken = GetLocalizationToken();
			  funfact.fMagnitude = 0.0f;
			  results.AddToTail(funfact);
			  return true;
		  }
		  return false;
	  }

private:
	TeamEvalFunction m_pfnEval;
	int				m_team;
};
#define DECLARE_FUNFACT_TEAMFUNC(funfactId, szLocalizationToken, fCoolness, pfnEval, iTeam)			\
static FunFactEvaluator *CreateFunFact_##funfactId( void )										\
{																									\
	return new CFunFact_TeamEvalFunction(funfactId, szLocalizationToken, fCoolness, pfnEval, iTeam);\
};																									\
static CFunFactHelper g_##funfactId##_Helper( CreateFunFact_##funfactId );


//=============================================================================
// High Stat-based Fun Fact
// This fun fact will find the player with the highest value for a particular
// stat, and validate when that stat exceeds a specified minimum
//=============================================================================
class CFunFact_StatBest : public FunFactEvaluator
{
public:
	CFunFact_StatBest(FunFactId id, const char* szLocalizationToken, float fCoolness, CSStatType_t statId, int iMin, int flags ) :
	  FunFactEvaluator(id, szLocalizationToken, fCoolness),
		  m_statId(statId),
		  m_min(iMin),
		  m_flags(flags)
	  {
		  V_strncpy(m_singularLocalizationToken, szLocalizationToken, sizeof(m_singularLocalizationToken));
		  if (m_min == 1)
		  {
			  V_strncat(m_singularLocalizationToken, "_singular", sizeof(m_singularLocalizationToken));
		  }
	  }

	  virtual bool Evaluate( FunFactVector& results ) const
	  {
		  int iBestValue = 0;
		  int iBestPlayer = 0;
		  for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		  {
			  CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
			  if ( pPlayer )
			  {
				  if (!PlayerQualifies(pPlayer, m_flags))
					  continue;

				  int iValue = CCS_GameStats.FindPlayerStats(pPlayer).statsCurrentRound[m_statId];
				  if ( iValue > iBestValue )
				  {
					  iBestValue = iValue;
					  iBestPlayer = i;
				  }
			  }
		  }
		  if ( iBestValue >= m_min )
		  {
			  FunFact funfact;
			  funfact.id = GetId();
			  funfact.szLocalizationToken = iBestValue == 1 ? m_singularLocalizationToken : GetLocalizationToken();
			  funfact.iPlayer = iBestPlayer;
			  funfact.iData1 = iBestValue;
			  funfact.fMagnitude = 1.0f - ((float)m_min / iBestValue);

			  results.AddToTail(funfact);
			  return true;
		  }
		  return false;
	  }

private:
	CSStatType_t	m_statId;
	int				m_min;
	char			m_singularLocalizationToken[128];
	int				m_flags;

};
#define DECLARE_FUNFACT_STATBEST(funfactId, szLocalizationToken, fCoolness, statId, iMin, flags)	\
static FunFactEvaluator *CreateFunFact_##funfactId( void )										\
{																									\
	return new CFunFact_StatBest(funfactId, szLocalizationToken, fCoolness, statId, iMin, flags);	\
};																									\
static CFunFactHelper g_##funfactId##_Helper( CreateFunFact_##funfactId );


//=============================================================================
// Sum-based Fun Fact
// This fun fact will add up a stat for all players, and is valid when the 
// sum exceeds a threshold
//=============================================================================
class CFunFact_StatSum : public FunFactEvaluator
{
public:
	CFunFact_StatSum(FunFactId id, const char* szLocalizationToken, float fCoolness, CSStatType_t statId, int iMin, EvalFlags::Type flags ) :
	  FunFactEvaluator(id, szLocalizationToken, fCoolness),
		  m_statId(statId),
		  m_min(iMin),
		  m_flags(flags)
	  {}

	  virtual bool Evaluate( FunFactVector& results ) const
	  {
		  int iSum = 0;
		  for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		  {
			  CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
			  if ( pPlayer )
			  {
				  if (!PlayerQualifies(pPlayer, m_flags))
					  continue;

				  iSum += CCS_GameStats.FindPlayerStats(pPlayer).statsCurrentRound[m_statId];
			  }
		  }
		  if ( iSum >= m_min )
		  {
			  FunFact funfact;
			  funfact.id = GetId();
			  funfact.szLocalizationToken = GetLocalizationToken();
			  funfact.iPlayer = 0;
			  funfact.iData1 = iSum;
			  funfact.fMagnitude = 1.0f - ((float)m_min / iSum);

			  results.AddToTail(funfact);
			  return true;
		  }
		  return false;
	  }

private:
	CSStatType_t	m_statId;
	int				m_min;
	int				m_flags;

};
#define DECLARE_FUNFACT_STATSUM(funfactId, szLocalizationToken, fCoolness, statId, iMin, flags)	\
static FunFactEvaluator *CreateFunFact_##funfactId( void )								\
{																							\
	return new CFunFact_StatSum(funfactId, szLocalizationToken, fCoolness, statId, iMin, flags);	\
};																							\
static CFunFactHelper g_##funfactId##_Helper( CreateFunFact_##funfactId );



//=============================================================================
// Helper function to calculate team accuracy
//=============================================================================

float GetTeamAccuracy( int teamNumber )
{
	int teamShots = 0;
	int teamHits = 0;

	//Add up hits and shots
	CBasePlayer *pPlayer = NULL;
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		pPlayer = UTIL_PlayerByIndex( i );
		if (pPlayer)
		{
			if (pPlayer->GetTeamNumber() == teamNumber)
			{
				teamShots += CCS_GameStats.FindPlayerStats(pPlayer).statsCurrentRound[CSSTAT_SHOTS_FIRED];;
				teamHits += CCS_GameStats.FindPlayerStats(pPlayer).statsCurrentRound[CSSTAT_SHOTS_HIT];;
			}
		}
	}

	if (teamShots > MIN_SHOTS_FOR_ACCURACY)
		return (float)teamHits / teamShots;

	return 0.0f;
}



//=============================================================================
// fun fact explicit evaluation functions
//=============================================================================

bool FFEVAL_ALWAYS_TRUE( int &iPlayer, int &data1, int &data2, int &data3 )
{
	return true;
}

bool FFEVAL_CT_WIN_NO_KILLS( int &iPlayer, int &data1, int &data2, int &data3 )
{
	return ( CSGameRules()->m_iRoundWinStatus == WINNER_CT && CSGameRules()->m_bNoTerroristsKilled );
}

bool FFEVAL_T_WIN_NO_KILLS( int &iPlayer, int &data1, int &data2, int &data3 )
{
	return ( CSGameRules()->m_iRoundWinStatus == WINNER_TER && CSGameRules()->m_bNoCTsKilled );
}

bool FFEVAL_T_WIN_NO_CASUALTIES( int &iPlayer, int &data1, int &data2, int &data3 )
{
	return ( CSGameRules()->m_iRoundWinStatus == WINNER_TER && CSGameRules()->m_bNoTerroristsKilled );
}

bool FFEVAL_CT_WIN_NO_CASUALTIES( int &iPlayer, int &data1, int &data2, int &data3 )
{
	return ( CSGameRules()->m_iRoundWinStatus == WINNER_CT && CSGameRules()->m_bNoCTsKilled );
}

int FFEVAL_KILLED_DEFUSER( CCSPlayer* pPlayer )
{
	return pPlayer->GetKilledDefuser() ? 1 : 0;
}

int FFEVAL_KILLED_RESCUER( CCSPlayer* pPlayer )
{
	return pPlayer->GetKilledRescuer() ? 1 : 0;
}

int FFEVAL_KILLS_WITH_GRENADE( CCSPlayer* pPlayer )
{
	return pPlayer->GetMaxGrenadeKills();
}

int FFEVAL_DAMAGE_NO_KILLS( CCSPlayer* pPlayer )
{
	if (CCS_GameStats.FindPlayerStats(pPlayer).statsCurrentRound[CSSTAT_KILLS] == 0)
		return CCS_GameStats.FindPlayerStats(pPlayer).statsCurrentRound[CSSTAT_DAMAGE];
	else
		return 0;
}

int FFEVAL_FIRST_KILL( CCSPlayer* pPlayer )
{
	if ( pPlayer == CSGameRules()->m_pFirstKill && CSGameRules()->m_firstKillTime < FIRST_KILL_TIME )
		return CSGameRules()->m_firstKillTime;
	else
		return 0;
}

int FFEVAL_FIRST_BLOOD( CCSPlayer* pPlayer )
{
	if ( pPlayer == CSGameRules()->m_pFirstBlood && CSGameRules()->m_firstBloodTime < FIRST_BLOOD_TIME )
		return CSGameRules()->m_firstBloodTime;
	else
		return 0;
}

bool FFEVAL_SHORT_ROUND( int &iPlayer, int &data1, int &data2, int &data3 )
{
	if ( CSGameRules()->GetRoundLength() - CSGameRules()->GetRoundRemainingTime() < SHORT_ROUND_TIME )
	{
		data1 = CSGameRules()->GetRoundLength() - CSGameRules()->GetRoundRemainingTime();
		return true;
	}
	return false;
}

int FFEVAL_ACCURACY( CCSPlayer* pPlayer )
{
	float shots = CCS_GameStats.FindPlayerStats(pPlayer).statsCurrentRound[CSSTAT_SHOTS_FIRED];
	float hits = CCS_GameStats.FindPlayerStats(pPlayer).statsCurrentRound[CSSTAT_SHOTS_HIT];
	if (shots >= MIN_SHOTS_FOR_ACCURACY)
		return RoundFloatToInt(100.0f * hits / shots);
	return 0;
}

int FFEVAL_KILLED_HALF_OF_ENEMIES( CCSPlayer* pPlayer )
{
    return pPlayer->GetPercentageOfEnemyTeamKilled();
}

bool FFEVAL_WON_AS_LAST_MEMBER( int &iPlayer, int &data1, int &data2, int &data3 )
{
	CCSPlayer *pCSPlayer = NULL;
	int winningTeam = CSGameRules()->m_iRoundWinStatus;

	if (winningTeam != TEAM_TERRORIST && winningTeam != TEAM_CT)
	{
		return false;
	}

	int losingTeam = (winningTeam == TEAM_TERRORIST) ? TEAM_CT : TEAM_TERRORIST;

	CCSGameRules::TeamPlayerCounts playerCounts[TEAM_MAXCOUNT];
	CSGameRules()->GetPlayerCounts(playerCounts);

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		pCSPlayer = ToCSPlayer(UTIL_PlayerByIndex( i ) );
		if( pCSPlayer && pCSPlayer->GetTeamNumber() == winningTeam && pCSPlayer->IsAlive())
		{
			//Check if the player is still the only living member of his team ( on the off chance that a player joins late)
			//This check is a little hacky. We make sure that there are no enemies alive. Since the bomb causes the round to end before exploding,
			//the only way for only 1 person to be alive at round win time is extermination or defuse (in both cases, the last living player caused the win)
			if (playerCounts[winningTeam].totalAlivePlayers == 1 && playerCounts[losingTeam].totalAlivePlayers == 0)
			{
				const PlayerStats_t& playerStats = CCS_GameStats.FindPlayerStats( pCSPlayer );
				iPlayer = i;
				data1 = playerStats.statsCurrentRound[CSSTAT_KILLS_WHILE_LAST_PLAYER_ALIVE];
				if (data1 >= 2)
				{
					return true;
				}
			}
		}
	}

	return false;
}

int FFEVAL_KNIFE_IN_GUNFIGHT( CCSPlayer* pPlayer )
{
	return pPlayer->WasWieldingKnifeAndKilledByGun() ? 1 : 0;
}

int FFEVAL_MULTIPLE_ATTACKER_COUNT( CCSPlayer* pPlayer )
{
	return pPlayer->GetNumEnemyDamagers();
}

int FFEVAL_USED_ALL_AMMO( CCSPlayer* pPlayer )
{
    CWeaponCSBase *pRifleWeapon = dynamic_cast< CWeaponCSBase * >(pPlayer->Weapon_GetSlot( WEAPON_SLOT_RIFLE ));
    CWeaponCSBase *pHandgunWeapon = dynamic_cast< CWeaponCSBase * >(pPlayer->Weapon_GetSlot( WEAPON_SLOT_PISTOL ));
    if ( pRifleWeapon && !pRifleWeapon->HasAmmo() && pHandgunWeapon && !pHandgunWeapon->HasAmmo() )
		return 1;
	else
		return 0;
}

int FFEVAL_DAMAGE_MULTIPLE_ENEMIES( CCSPlayer* pPlayer )
{
	return pPlayer->GetNumEnemiesDamaged();
}

int FFEVAL_USED_MULTIPLE_WEAPONS( CCSPlayer* pPlayer )
{
	return pPlayer->GetNumFirearmsUsed();
}

int FFEVAL_DEFUSED_WITH_DROPPED_KIT( CCSPlayer* pPlayer )
{
    return pPlayer->GetDefusedWithPickedUpKit() ? 1 : 0;
}

bool FFEVAL_TERRORIST_ACCURACY( int &iPlayer, int &data1, int &data2, int &data3 )
{
	float terroristAccuracy = GetTeamAccuracy(TEAM_TERRORIST);
	float ctAccuracy = GetTeamAccuracy(TEAM_CT);

	if (terroristAccuracy > 0.2f && terroristAccuracy > ctAccuracy)
	{
		data1 = RoundFloatToInt(terroristAccuracy * 100.0f);
		return true;
	}
	return false;
}

bool FFEVAL_CT_ACCURACY( int &iPlayer, int &data1, int &data2, int &data3 )
{
	float terroristAccuracy = GetTeamAccuracy(TEAM_TERRORIST);
	float ctAccuracy = GetTeamAccuracy(TEAM_CT);

	if (ctAccuracy > 0.2f && ctAccuracy > terroristAccuracy)
	{
		data1 = RoundFloatToInt(ctAccuracy * 100.0f);
		return true;
	}
	return false;
}

bool FFEVAL_SAME_UNIFORM( int iTeam, int &iData1, int &iData2, int &iData3 )
{
    int numberInUniform = 0;
	int iUniform = -1;

    for ( int i = 1; i <= gpGlobals->maxClients; i++ )
    {
        CCSPlayer *pCSPlayer = ToCSPlayer(UTIL_PlayerByIndex( i ) );
		if ( pCSPlayer && pCSPlayer->GetTeamNumber() == iTeam && pCSPlayer->State_Get() != STATE_PICKINGCLASS)
        {		
            if (iUniform == -1)
			{
				iUniform = pCSPlayer->PlayerClass();
			}
			else if (pCSPlayer->PlayerClass() != iUniform)
			{
				return false;
			}
			++numberInUniform;
        }
    }

	return numberInUniform >= 3;
}

bool FFEVAL_BEST_TERRORIST_ACCURACY( int &iPlayer, int &data1, int &data2, int &data3 )
{
    float fAccuracy = 0.0f, fBestAccuracy = 0.0f;
    CBasePlayer *pPlayer = NULL;
    for ( int i = 1; i <= gpGlobals->maxClients; i++ )
    {
        pPlayer = UTIL_PlayerByIndex( i );

        // Look only at terrorist players
        if ( pPlayer && pPlayer->GetTeamNumber() == TEAM_TERRORIST )
        {
            // Calculate accuracy the terrorist
            float shots = CCS_GameStats.FindPlayerStats(pPlayer).statsCurrentRound[CSSTAT_SHOTS_FIRED];
            float hits = CCS_GameStats.FindPlayerStats(pPlayer).statsCurrentRound[CSSTAT_SHOTS_HIT];
            if (shots > MIN_SHOTS_FOR_ACCURACY)
            {
                fAccuracy = (float)hits / shots;
            }

            // Track the most accurate terrorist
            if ( fAccuracy > fBestAccuracy )
            {
                fBestAccuracy = fAccuracy;
                iPlayer = i;
            }
        }
    }

    if ( fBestAccuracy - GetTeamAccuracy( TEAM_TERRORIST ) >= 0.10f )
    {
		data1 = RoundFloatToInt(fBestAccuracy * 100.0f);
        data2 = RoundFloatToInt(GetTeamAccuracy( TEAM_TERRORIST ) * 100.0f);
        return true;
    }

    return false;
}

bool FFEVAL_BEST_COUNTERTERRORIST_ACCURACY( int &iPlayer, int &data1, int &data2, int &data3 )
{
	float fAccuracy = 0.0f, fBestAccuracy = 0.0f;
    CBasePlayer *pPlayer = NULL;
    for ( int i = 1; i <= gpGlobals->maxClients; i++ )
    {
        pPlayer = UTIL_PlayerByIndex( i );

        // Look only at counter-terrorist players
        if ( pPlayer && pPlayer->GetTeamNumber() == TEAM_CT )
        {
            // Calculate accuracy the counter-terrorist
            float shots = CCS_GameStats.FindPlayerStats(pPlayer).statsCurrentRound[CSSTAT_SHOTS_FIRED];
            float hits = CCS_GameStats.FindPlayerStats(pPlayer).statsCurrentRound[CSSTAT_SHOTS_HIT];
            if (shots > MIN_SHOTS_FOR_ACCURACY)
            {
                fAccuracy = (float)hits / shots;
            }

            // Track the most accurate counter-terrorist
            if ( fAccuracy > fBestAccuracy )
            {
                fBestAccuracy = fAccuracy;
                iPlayer = i;
            }
        }
    }

	if ( fBestAccuracy - GetTeamAccuracy( TEAM_CT ) >= 0.10f )
	{
		data1 = RoundFloatToInt(fBestAccuracy * 100.0f);
		data2 = RoundFloatToInt(GetTeamAccuracy( TEAM_CT ) * 100.0f);
        return true;
    }

    return false;
}


//=============================================================================
// Fun Fact Declarations
//=============================================================================

DECLARE_FUNFACT_STATBEST(	FUNFACT_DAMAGE_WITH_GRENADES,			"#funfact_damage_with_grenade",				0.5f,	CSSTAT_GRENADE_DAMAGE,					200,	EvalFlags::HighestOnly);
DECLARE_FUNFACT_STATBEST(	FUNFACT_KNIFE_KILLS,					"#funfact_knife_kills",						0.5f,	CSSTAT_KILLS_KNIFE,						1,		EvalFlags::HighestOnly);
DECLARE_FUNFACT_STATBEST(	FUNFACT_KILLS_WITH_GRENADES,			"#funfact_kills_grenades",					0.7f, 	CSSTAT_KILLS_HEGRENADE,					2,		EvalFlags::HighestOnly);
DECLARE_FUNFACT_STATBEST(	FUNFACT_BLIND_KILLS,					"#funfact_blind_kills",						0.9f,	CSSTAT_KILLS_WHILE_BLINDED,				1,		EvalFlags::HighestOnly);
DECLARE_FUNFACT_STATBEST(	FUNFACT_KILLED_ENEMIES,					"#funfact_killed_enemies",					0.6f,	CSSTAT_KILLS,							3,		EvalFlags::HighestOnly);
DECLARE_FUNFACT_STATBEST(	FUNFACT_KILLS_WITH_LAST_ROUND,			"#funfact_kills_with_last_round",			0.6f,	CSSTAT_KILLS_WITH_LAST_ROUND,			1,		EvalFlags::HighestOnly);
DECLARE_FUNFACT_STATBEST(	FUNFACT_DONATED_WEAPONS,				"#funfact_donated_weapons",					0.3f,	CSSTAT_WEAPONS_DONATED,					2,		EvalFlags::HighestOnly);
DECLARE_FUNFACT_STATBEST(	FUNFACT_NUM_TIMES_JUMPED,				"#funfact_num_times_jumped",				0.2f,	CSSTAT_TOTAL_JUMPS,						10,		EvalFlags::HighestOnly);
DECLARE_FUNFACT_STATBEST(	FUNFACT_FALL_DAMAGE,					"#funfact_fall_damage",						0.2f,	CSSTAT_FALL_DAMAGE,						50,		EvalFlags::HighestOnly);
DECLARE_FUNFACT_STATBEST(	FUNFACT_POSTHUMOUS_KILLS_WITH_GRENADE,	"#funfact_posthumous_kills_with_grenade",	1.0f,	CSSTAT_GRENADE_POSTHUMOUSKILLS,			1,		EvalFlags::HighestOnly);
DECLARE_FUNFACT_STATBEST(	FUNFACT_ITEMS_PURCHASED,				"#funfact_items_purchased",					0.2f,	CSSTAT_ITEMS_PURCHASED,					5,		EvalFlags::HighestOnly);
DECLARE_FUNFACT_STATBEST(	FUNFACT_NUMBER_OF_OVERKILLS,			"#funfact_number_of_overkills",				0.5f,	CSSTAT_DOMINATION_OVERKILLS,			2,		EvalFlags::HighestOnly);
DECLARE_FUNFACT_STATBEST(	FUNFACT_MONEY_SPENT,					"#funfact_money_spent",						0.2f,	CSSTAT_MONEY_SPENT,						5000,	EvalFlags::HighestOnly);
DECLARE_FUNFACT_STATBEST(	FUNFACT_GRENADES_THROWN,				"#funfact_grenades_thrown",					0.3f,	CSSTAT_GRENADES_THROWN,					2,		EvalFlags::HighestOnly);
DECLARE_FUNFACT_STATBEST(	FUNFACT_DEFENDED_BOMB,					"#funfact_defended_bomb",					0.5f,	CSSTAT_KILLS_WHILE_DEFENDING_BOMB,		2,		EvalFlags::HighestOnly);
DECLARE_FUNFACT_STATBEST(	FUNFACT_ITEMS_DROPPED_VALUE,			"#funfact_items_dropped_value",				0.5f,	CSTAT_ITEMS_DROPPED_VALUE,				10000,	EvalFlags::HighestOnly);
DECLARE_FUNFACT_STATBEST(	FUNFACT_KILL_WOUNDED_ENEMIES,			"#funfact_kill_wounded_enemies",			0.4f,	CSSTAT_KILLS_ENEMY_WOUNDED,				3,		EvalFlags::HighestOnly);
DECLARE_FUNFACT_STATBEST(	FUNFACT_KILLS_HEADSHOTS,				"#funfact_kills_headshots",					0.7f,	CSSTAT_KILLS_HEADSHOT,					3,		EvalFlags::HighestOnly);
DECLARE_FUNFACT_STATBEST(	FUNFACT_BROKE_WINDOWS,					"#funfact_broke_windows",					0.3f,	CSSTAT_NUM_BROKEN_WINDOWS,				5,		EvalFlags::HighestOnly);
DECLARE_FUNFACT_STATBEST(	FUNFACT_NIGHTVISION_DAMAGE,				"#funfact_nightvision_damage",				0.5f,	CSSTAT_NIGHTVISION_DAMAGE,				100,	EvalFlags::HighestOnly);

DECLARE_FUNFACT_STATSUM(	FUNFACT_SHOTS_FIRED,					"#funfact_shots_fired",						0.1f,	CSSTAT_SHOTS_FIRED,						200,	EvalFlags::All);

DECLARE_FUNFACT_PLAYERFUNC(	FUNFACT_KILL_DEFUSER,					"#funfact_kill_defuser",					0.6f, 	FFEVAL_KILLED_DEFUSER,					1,		EvalFlags::TeamTerrorist | EvalFlags::WinningTeam);
DECLARE_FUNFACT_PLAYERFUNC( FUNFACT_KILL_RESCUER,					"#funfact_kill_rescuer",					0.6f, 	FFEVAL_KILLED_RESCUER,					1,		EvalFlags::TeamTerrorist);
DECLARE_FUNFACT_PLAYERFUNC(	FUNFACT_KILLS_WITH_SINGLE_GRENADE,		"#funfact_kills_with_single_grenade",		0.8f, 	FFEVAL_KILLS_WITH_GRENADE,				2,		EvalFlags::HighestOnly);
DECLARE_FUNFACT_PLAYERFUNC(	FUNFACT_DAMAGE_NO_KILLS,				"#funfact_damage_no_kills",					0.4f, 	FFEVAL_DAMAGE_NO_KILLS,					200,	EvalFlags::HighestOnly);
DECLARE_FUNFACT_PLAYERFUNC( FUNFACT_FIRST_KILL,						"#funfact_first_kill",						0.2f, 	FFEVAL_FIRST_KILL,						1,		EvalFlags::HighestOnly);
DECLARE_FUNFACT_PLAYERFUNC(	FUNFACT_FIRST_BLOOD,					"#funfact_first_blood",						0.2f, 	FFEVAL_FIRST_BLOOD,						1,		EvalFlags::HighestOnly);
DECLARE_FUNFACT_PLAYERFUNC( FUNFACT_BEST_ACCURACY,					"#funfact_best_accuracy",					0.4f,	FFEVAL_ACCURACY,						20,		EvalFlags::HighestOnly);
DECLARE_FUNFACT_PLAYERFUNC( FUNFACT_KNIFE_IN_GUNFIGHT,				"#funfact_knife_in_gunfight",				0.6f, 	FFEVAL_KNIFE_IN_GUNFIGHT ,				1,		EvalFlags::All);
DECLARE_FUNFACT_PLAYERFUNC(	FUNFACT_SURVIVED_MULTIPLE_ATTACKERS,	"#funfact_survived_multiple_attackers",		0.3f,	FFEVAL_MULTIPLE_ATTACKER_COUNT,			3,		EvalFlags::HighestOnly | EvalFlags::Alive);
DECLARE_FUNFACT_PLAYERFUNC(	FUNFACT_DIED_FROM_MULTIPLE_ATTACKERS,	"#funfact_died_from_multiple_attackers",	0.5f,	FFEVAL_MULTIPLE_ATTACKER_COUNT,			3,		EvalFlags::HighestOnly | EvalFlags::Dead);
DECLARE_FUNFACT_PLAYERFUNC(	FUNFACT_USED_ALL_AMMO,					"#funfact_used_all_ammo",					0.5f,	FFEVAL_USED_ALL_AMMO,					1,		EvalFlags::All );
DECLARE_FUNFACT_PLAYERFUNC(	FUNFACT_DAMAGE_MULTIPLE_ENEMIES,		"#funfact_damage_multiple_enemies",			0.5f,	FFEVAL_DAMAGE_MULTIPLE_ENEMIES,			3,		EvalFlags::HighestOnly);
DECLARE_FUNFACT_PLAYERFUNC(	FUNFACT_USED_MULTIPLE_WEAPONS,			"#funfact_used_multiple_weapons",			0.5f,	FFEVAL_USED_MULTIPLE_WEAPONS,			4,		EvalFlags::HighestOnly);
DECLARE_FUNFACT_PLAYERFUNC(	FUNFACT_DEFUSED_WITH_DROPPED_KIT,       "#funfact_defused_with_dropped_kit",        0.4f, 	FFEVAL_DEFUSED_WITH_DROPPED_KIT,        1,		EvalFlags::TeamCT);
DECLARE_FUNFACT_PLAYERFUNC(	FUNFACT_KILLED_HALF_OF_ENEMIES,         "#funfact_killed_half_of_enemies",          0.5f, 	FFEVAL_KILLED_HALF_OF_ENEMIES,          50,		EvalFlags::WinningTeam | EvalFlags::HighestOnly);

DECLARE_FUNFACT_EVALFUNC(	FUNFACT_CT_WIN_NO_KILLS,				"#funfact_ct_win_no_kills",					0.4f,	FFEVAL_CT_WIN_NO_KILLS);
DECLARE_FUNFACT_EVALFUNC( 	FUNFACT_T_WIN_NO_KILLS,					"#funfact_t_win_no_kills",					0.4f, 	FFEVAL_T_WIN_NO_KILLS );
DECLARE_FUNFACT_EVALFUNC( 	FUNFACT_T_WIN_NO_CASUALTIES,			"#funfact_t_win_no_casualties",				0.2f, 	FFEVAL_T_WIN_NO_CASUALTIES );
DECLARE_FUNFACT_EVALFUNC( 	FUNFACT_CT_WIN_NO_CASUALTIES,			"#funfact_ct_win_no_casualties",			0.2f, 	FFEVAL_CT_WIN_NO_CASUALTIES );
DECLARE_FUNFACT_EVALFUNC( 	FUNFACT_SHORT_ROUND,					"#funfact_short_round",						0.3f, 	FFEVAL_SHORT_ROUND );
DECLARE_FUNFACT_EVALFUNC( 	FUNFACT_WON_AS_LAST_MEMBER,				"#funfact_won_as_last_member",				0.6f, 	FFEVAL_WON_AS_LAST_MEMBER );
DECLARE_FUNFACT_EVALFUNC(	FUNFACT_TERRORIST_ACCURACY,				"#funfact_terrorist_accuracy",				0.2f,	FFEVAL_TERRORIST_ACCURACY);
DECLARE_FUNFACT_EVALFUNC(	FUNFACT_CT_ACCURACY,					"#funfact_ct_accuracy",						0.2f,	FFEVAL_CT_ACCURACY);
DECLARE_FUNFACT_EVALFUNC(	FUNFACT_BEST_TERRORIST_ACCURACY,		"#funfact_best_terrorist_accuracy",			0.3f,	FFEVAL_BEST_TERRORIST_ACCURACY);
DECLARE_FUNFACT_EVALFUNC(	FUNFACT_BEST_COUNTERTERRORIST_ACCURACY, "#funfact_best_counterterrorist_accuracy",	0.3f,	FFEVAL_BEST_COUNTERTERRORIST_ACCURACY);
DECLARE_FUNFACT_EVALFUNC(	FUNFACT_FALLBACK1,						"#funfact_fallback1",						0.0f,	FFEVAL_ALWAYS_TRUE);
DECLARE_FUNFACT_EVALFUNC(	FUNFACT_FALLBACK2,						"#funfact_fallback2",						0.0f,	FFEVAL_ALWAYS_TRUE);

DECLARE_FUNFACT_TEAMFUNC(	FUNFACT_SAME_UNIFORM_TERRORIST,			"#funfact_same_uniform_terrorist",			0.5f,	FFEVAL_SAME_UNIFORM,					        TEAM_TERRORIST);
DECLARE_FUNFACT_TEAMFUNC(	FUNFACT_SAME_UNIFORM_CT,				"#funfact_same_uniform_ct",					0.5f,	FFEVAL_SAME_UNIFORM,					        TEAM_CT);
