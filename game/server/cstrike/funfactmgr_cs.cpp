//========= Copyright Valve Corporation, All rights reserved. ============//
#include "cbase.h"
#include "usermessages.h"
#include "funfactmgr_cs.h"

const float kCooldownRatePlayer		= 0.4f;
const float kCooldownRateFunFact	= 0.2f;

const float kWeightPlayerCooldown	= 0.8f;
const float kWeightFunFactCooldown	= 1.0f;
const float kWeightCoolness			= 2.0f;
const float kWeightRarity			= 1.0f;

#define DEBUG_FUNFACT_SCORING 0

//-----------------------------------------------------------------------------
// Purpose: constructor
//-----------------------------------------------------------------------------
CCSFunFactMgr::CCSFunFactMgr() : 
	CAutoGameSystemPerFrame( "CCSFunFactMgr" ),
	m_funFactDatabase(0, 100, DefLessFunc(int) )
{
	for ( int i = 0; i < MAX_PLAYERS; ++i )
	{
		m_playerCooldown[i] = 0.0f;
	}
}

CCSFunFactMgr::~CCSFunFactMgr()
{
	Shutdown();
}

//-----------------------------------------------------------------------------
// Purpose: Initializes the fun fact manager
//-----------------------------------------------------------------------------
bool CCSFunFactMgr::Init()
{
	ListenForGameEvent( "player_connect" );

	CFunFactHelper *pFunFactHelper = CFunFactHelper::s_pFirst;

	// create database of all fun fact evaluators (and initial usage metrics)
	while ( pFunFactHelper )
	{
		FunFactDatabaseEntry entry;
		entry.fCooldown = 0.0f;
		entry.iOccurrences = 0;
		entry.pEvaluator = pFunFactHelper->m_pfnCreate();
		m_funFactDatabase.Insert(entry.pEvaluator->GetId(), entry);

		pFunFactHelper = pFunFactHelper->m_pNext;
	}

	for (int i = 0; i < ARRAYSIZE(m_playerCooldown); ++i)
	{
		m_playerCooldown[i] = 0.0f;
	}

	m_numRounds = 0;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Shuts down the fun fact manager
//-----------------------------------------------------------------------------
void CCSFunFactMgr::Shutdown()
{
	FOR_EACH_MAP( m_funFactDatabase, iter )
	{
		delete m_funFactDatabase[iter].pEvaluator;
	}
	m_funFactDatabase.RemoveAll();
}

//-----------------------------------------------------------------------------
// Purpose: Per frame processing
//-----------------------------------------------------------------------------
void CCSFunFactMgr::Update( float frametime )
{

}

//-----------------------------------------------------------------------------
// Purpose: Listens for game events.  Clears out map based stats and player based stats when necessary
//-----------------------------------------------------------------------------
void CCSFunFactMgr::FireGameEvent( IGameEvent *event )
{
	const char *eventname = event->GetName();

	if ( Q_strcmp( "player_connect", eventname ) == 0 )
	{
		int index = event->GetInt("index");// player slot (entity index-1)
		ASSERT( index >= 0 && index < MAX_PLAYERS );
		if( index >= 0 && index < MAX_PLAYERS )
		{
			m_playerCooldown[index] = 0.0f;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Finds the best fun fact to display and returns all necessary information through the parameters
//-----------------------------------------------------------------------------
bool CCSFunFactMgr::GetRoundEndFunFact( int iWinningTeam, int iRoundResult, FunFact& funfact )
{
	FunFactVector validFunFacts;

	// Generate a vector of all valid fun facts for this round
	FOR_EACH_MAP( m_funFactDatabase, i )
	{
		FunFact funFact;
		if ( m_funFactDatabase[i].pEvaluator->Evaluate(validFunFacts) )
		{
			m_funFactDatabase[i].iOccurrences++;
		}
	}

	m_numRounds++;

	if (validFunFacts.Size() == 0)
		return false;

	// pick the fun fact with the highest score
	float fBestScore = -FLT_MAX;
	int iFunFactIndex = -1;

#if DEBUG_FUNFACT_SCORING
	Msg("Scoring fun facts:\n");
#endif

	FOR_EACH_VEC(validFunFacts, i)
	{
		float fScore = ScoreFunFact(validFunFacts[i]);

#if DEBUG_FUNFACT_SCORING
		char szPlayerName[64];
		const FunFact& funfact = validFunFacts[i];
		if (funfact.iPlayer > 0)
			V_strncpy(szPlayerName, ToCSPlayer(UTIL_PlayerByIndex(funfact.iPlayer))->GetPlayerName(), sizeof(szPlayerName));
		else
			V_strcpy(szPlayerName, "");

		Msg("(%5.4f) %s, %s, %i, %i, %i\n", fScore, funfact.szLocalizationToken, szPlayerName, funfact.iData1, funfact.iData2, funfact.iData3);
#endif

		if (fScore > fBestScore)
		{
			fBestScore = fScore;
			iFunFactIndex = i;
		}
	}

	if (iFunFactIndex < 0)
		return false;
	
	funfact = validFunFacts[iFunFactIndex];

	// decay player cooldowns
	for (int i = 0; i < MAX_PLAYERS; ++i )
	{
		m_playerCooldown[i] *= (1.0f - kCooldownRatePlayer);
	}

	// decay funfact cooldowns
	FOR_EACH_MAP(m_funFactDatabase, i)
	{
		m_funFactDatabase[i].fCooldown *= (1.0f - kCooldownRateFunFact);
	}

	// set player cooldown for player in funfact
	if ( funfact.iPlayer )
	{
		m_playerCooldown[funfact.iPlayer - 1] = 1.0f;
	}

	// set funfact cooldown for current funfact
	m_funFactDatabase[m_funFactDatabase.Find(funfact.id)].fCooldown = 1.0f;

	return true;
}

float CCSFunFactMgr::ScoreFunFact( const FunFact& funfact )
{
	float fScore = 0.0f;
	const FunFactDatabaseEntry& dbEntry = m_funFactDatabase[m_funFactDatabase.Find(funfact.id)];

	// add the coolness score for the funfact
	fScore += kWeightCoolness * dbEntry.pEvaluator->GetCoolness() * (1.0f + funfact.fMagnitude);

	// subtract the cooldown for the funfact
	fScore -= kWeightFunFactCooldown * dbEntry.fCooldown;
	
	// subtract the cooldown for the player
	if ( funfact.iPlayer ) {
		fScore -= kWeightPlayerCooldown * m_playerCooldown[funfact.iPlayer - 1];
	}

	// add the rarity bonus
	fScore += kWeightRarity * powf((1.0f - (float)dbEntry.iOccurrences / m_numRounds), 2.0f);

	return fScore;
}
