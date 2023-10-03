#include "cbase.h"
#include "asw_fail_advice.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


CASW_Fail_Advice g_ASWFailAdvice;
CASW_Fail_Advice* ASWFailAdvice() { return &g_ASWFailAdvice; }

CASW_Fail_Advice::CASW_Fail_Advice( void ) : CAutoGameSystem( "CASW_Fail_Advice" )
{
	
}

CASW_Fail_Advice::~CASW_Fail_Advice()
{
	
}

bool CASW_Fail_Advice::Init()
{
	return true;
}

void CASW_Fail_Advice::Shutdown()
{

}


void CASW_Fail_Advice::DumpPointsToConsole( void )
{
	for ( int nFailAdvice = 0; nFailAdvice < ASW_FAIL_ADVICE_TOTAL; ++nFailAdvice )
	{
		DevMsg( "#%i\t%i\t%i\n", nFailAdvice, m_Status[ nFailAdvice ].nPoints, m_Status[ nFailAdvice ].nData );
	}
}


int CASW_Fail_Advice::UseCurrentFailAdvice( void )
{
	int nBestFailAdvice = 0;
	int nMostPoints = 0;

	for ( int nFailAdvice = 0; nFailAdvice < ASW_FAIL_ADVICE_TOTAL; ++nFailAdvice )
	{
		if ( nMostPoints < m_Status[ nFailAdvice ].nPoints && 
			 ( m_Status[ nFailAdvice ].nCooldownRounds == 0 || nMostPoints == 0 ) && 
			 m_Status[ nFailAdvice ].nPointsLastRound != m_Status[ nFailAdvice ].nPoints )
		{
			// This one is the highest, hasn't been displayed lately, and has incremented this round
			nMostPoints = m_Status[ nFailAdvice ].nPoints;
			nBestFailAdvice = nFailAdvice;
		}
	}

	// Don't show this one for at least 3 more rounds
	m_Status[ nBestFailAdvice ].nPoints = 0;
	m_Status[ nBestFailAdvice ].nCooldownRounds = 3;

	return nBestFailAdvice;
}

void CASW_Fail_Advice::IncrementPoints( int nMessage, int nPoints )
{
	if ( m_Status[ nMessage ].nCooldownRounds <= 0 )
	{
		m_Status[ nMessage ].nPoints += nPoints;
	}
}

void CASW_Fail_Advice::OnMissionStart( void )
{
	for ( int nFailAdvice = 0; nFailAdvice < ASW_FAIL_ADVICE_TOTAL; ++nFailAdvice )
	{
		// Remember not to advise about something that hasn't changed this round
		m_Status[ nFailAdvice ].nPointsLastRound = m_Status[ nFailAdvice ].nPoints;

		if ( m_Status[ nFailAdvice ].nCooldownRounds > 0 )
		{
			// One less round before we can show this again
			m_Status[ nFailAdvice ].nCooldownRounds--;
		}

		// Start with fresh data for the round
		m_Status[ nFailAdvice ].nData = 0;
	}
}

void CASW_Fail_Advice::OnMarineOutOfAmmo( void )
{
	IncrementPoints( ASW_FAIL_ADVICE_LOW_AMMO );
}

void CASW_Fail_Advice::OnMarineInfested( void )
{
	m_Status[ ASW_FAIL_ADVICE_INFESTED_LOTS ].nData++;

	if ( m_Status[ ASW_FAIL_ADVICE_INFESTED_LOTS ].nData > 2 )
	{
		// They've done a lot of FF
		m_Status[ ASW_FAIL_ADVICE_INFESTED_LOTS ].nData = 0;
		IncrementPoints( ASW_FAIL_ADVICE_INFESTED_LOTS );
	}
}

void CASW_Fail_Advice::OnMarineInfestedGibbed( void )
{
	IncrementPoints( ASW_FAIL_ADVICE_INFESTED );
}

void CASW_Fail_Advice::OnSentryUsedWell( void )
{
	m_Status[ ASW_FAIL_ADVICE_SWARMED ].nData--;
}

void CASW_Fail_Advice::OnMarineMobAttacked( void )
{
	m_Status[ ASW_FAIL_ADVICE_SWARMED ].nData++;

	if ( m_Status[ ASW_FAIL_ADVICE_SWARMED ].nData > 2 )
	{
		// They haven't used a sentry in very many good locations
		IncrementPoints( ASW_FAIL_ADVICE_SWARMED );
	}

	m_Status[ ASW_FAIL_ADVICE_IGNORED_ADRENALINE ].nData++;

	if ( m_Status[ ASW_FAIL_ADVICE_IGNORED_ADRENALINE ].nData > 2 )
	{
		// They haven't used a adrenaline much
		IncrementPoints( ASW_FAIL_ADVICE_IGNORED_ADRENALINE );
	}

	m_Status[ ASW_FAIL_ADVICE_IGNORED_SECONDARY ].nData += 2;

	if ( m_Status[ ASW_FAIL_ADVICE_IGNORED_SECONDARY ].nData > 4 )
	{
		// They haven't used secondary much
		IncrementPoints( ASW_FAIL_ADVICE_IGNORED_SECONDARY );
	}
}

void CASW_Fail_Advice::OnFriendlyFire( int nDamage )
{
	m_Status[ ASW_FAIL_ADVICE_FRIENDLY_FIRE ].nData += nDamage;

	if ( m_Status[ ASW_FAIL_ADVICE_FRIENDLY_FIRE ].nData > 50 )
	{
		// They've done a lot of FF
		m_Status[ ASW_FAIL_ADVICE_FRIENDLY_FIRE ].nData = 0;
		IncrementPoints( ASW_FAIL_ADVICE_FRIENDLY_FIRE );
	}
}

void CASW_Fail_Advice::OnHackerHurt( int nDamage )
{
	m_Status[ ASW_FAIL_ADVICE_HACKER_DAMAGED ].nData += nDamage;

	if ( m_Status[ ASW_FAIL_ADVICE_HACKER_DAMAGED ].nData > 40 )
	{
		// They've done a lot of damage to the hacker
		m_Status[ ASW_FAIL_ADVICE_HACKER_DAMAGED ].nData = 0;
		IncrementPoints( ASW_FAIL_ADVICE_HACKER_DAMAGED );
	}
}

void CASW_Fail_Advice::OnHackerDied( void )
{
	// They've killed the hacker
	m_Status[ ASW_FAIL_ADVICE_HACKER_DAMAGED ].nData = 0;
	IncrementPoints( ASW_FAIL_ADVICE_HACKER_DAMAGED );
}

void CASW_Fail_Advice::OnMarineOverhealed( int nOverhealAmount )
{
	// Accumulate how much health they're wasting
	m_Status[ ASW_FAIL_ADVICE_WASTED_HEALS ].nData += MAX( 0, nOverhealAmount );
}

void CASW_Fail_Advice::OnMedSatchelEmpty( void )
{
	if ( m_Status[ ASW_FAIL_ADVICE_WASTED_HEALS ].nData >= 80 )
	{
		// They've been wasting health before running dry
		m_Status[ ASW_FAIL_ADVICE_WASTED_HEALS ].nData = 0;
		IncrementPoints( ASW_FAIL_ADVICE_WASTED_HEALS, 2 );
	}
}

void CASW_Fail_Advice::OnHealGrenadeUsedWell( void )
{
	m_Status[ ASW_FAIL_ADVICE_IGNORED_HEALING ].nData++;
}

void CASW_Fail_Advice::OnMarineHealed()
{
	m_Status[ ASW_FAIL_ADVICE_IGNORED_HEALING ].nData++;
}

void CASW_Fail_Advice::OnMarineKilled()
{
	m_Status[ ASW_FAIL_ADVICE_IGNORED_HEALING ].nData--;

	if ( m_Status[ ASW_FAIL_ADVICE_IGNORED_HEALING ].nData < -1 )
	{
		// They've been letting lots of dudes die without healing much
		m_Status[ ASW_FAIL_ADVICE_IGNORED_HEALING ].nData = 0;
		IncrementPoints( ASW_FAIL_ADVICE_IGNORED_HEALING, 2 );
	}
}

void CASW_Fail_Advice::OnMarineUsedAdrenaline()
{
	m_Status[ ASW_FAIL_ADVICE_IGNORED_ADRENALINE ].nData--;
}

void CASW_Fail_Advice::OnMarineUsedSecondary()
{
	m_Status[ ASW_FAIL_ADVICE_IGNORED_SECONDARY ].nData--;
}

void CASW_Fail_Advice::OnAlienOpenDoor( void )
{
	m_Status[ ASW_FAIL_ADVICE_IGNORED_WELDER ].nData++;

	if ( m_Status[ ASW_FAIL_ADVICE_IGNORED_WELDER ].nData > 60 )
	{
		// They let a lot of aliens through doors
		m_Status[ ASW_FAIL_ADVICE_IGNORED_WELDER ].nData = 0;
		IncrementPoints( ASW_FAIL_ADVICE_IGNORED_WELDER );
	}
}

void CASW_Fail_Advice::OnMarineWeldedDoor( void )
{
	m_Status[ ASW_FAIL_ADVICE_IGNORED_WELDER ].nData -= 75;
}

void CASW_Fail_Advice::OnAlienSpawnedInfinite( void )
{
	m_Status[ ASW_FAIL_ADVICE_SLOW_PROGRESSION ].nData++;

	if ( m_Status[ ASW_FAIL_ADVICE_SLOW_PROGRESSION ].nData > 50 )
	{
		// They let a lot of aliens through doors
		m_Status[ ASW_FAIL_ADVICE_SLOW_PROGRESSION ].nData = 0;
		IncrementPoints( ASW_FAIL_ADVICE_SLOW_PROGRESSION );
	}
}

void CASW_Fail_Advice::OnShiedbugBlocked( void )
{
	m_Status[ ASW_FAIL_ADVICE_SHIELD_BUG ].nData++;

	if ( m_Status[ ASW_FAIL_ADVICE_SHIELD_BUG ].nData > 80 )
	{
		// They let a lot of aliens through doors
		m_Status[ ASW_FAIL_ADVICE_SHIELD_BUG ].nData = 0;
		IncrementPoints( ASW_FAIL_ADVICE_SHIELD_BUG, 2 );
	}
}

void CASW_Fail_Advice::OnShiedbugKilled( void )
{
	m_Status[ ASW_FAIL_ADVICE_SHIELD_BUG ].nData = 0;
}

void CASW_Fail_Advice::OnMarineKilledAlone( void )
{
	m_Status[ ASW_FAIL_ADVICE_DIED_ALONE ].nPoints++;
}

void CASW_Fail_Advice::OnNoMedicStart( void )
{
	m_Status[ ASW_FAIL_ADVICE_NO_MEDICS ].nPoints += 2;
}

const FailAdviceMessageStatus_t * CASW_Fail_Advice::GetFailAdviceStatus( void )
{
	return m_Status;
}

CON_COMMAND_F( failadvice_dump_values, "Gives a list of all current points.", FCVAR_CHEAT )
{
	ASWFailAdvice()->DumpPointsToConsole();
}
