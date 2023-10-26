#ifndef _INCLUDED_ASW_FAIL_ADVICE_H
#define _INCLUDED_ASW_FAIL_ADVICE_H
#ifdef _WIN32
#pragma once
#endif

#include "asw_shareddefs.h"
#include "igamesystem.h"


struct FailAdviceMessageStatus_t
{
	int nPoints;
	int nPointsLastRound;
	int nCooldownRounds;
	int nData;

	FailAdviceMessageStatus_t( void )
	{
		nPoints = 0;
		nPointsLastRound = 0;
		nCooldownRounds = 0;
		nData = 0;
	}
};


class CASW_Fail_Advice : public CAutoGameSystem
{
public:
	CASW_Fail_Advice();
	~CASW_Fail_Advice();

	virtual bool Init();
	virtual void Shutdown();

	void DumpPointsToConsole( void );

	int UseCurrentFailAdvice( void );
	void IncrementPoints( int nMessage, int nPoints = 1 );

	void OnMissionStart( void );

	// low ammo
	void OnMarineOutOfAmmo( void );

	// infested lots
	void OnMarineInfested( void );

	// infested
	void OnMarineInfestedGibbed( void );

	// swarmed
	void OnSentryUsedWell( void );
	void OnMarineMobAttacked( void );

	// friendly fire
	void OnFriendlyFire( int nDamage );

	// hacker damaged
	void OnHackerHurt( int nDamage );
	void OnHackerDied( void );

	// wasted healing
	void OnMarineOverhealed( int nOverhealAmount );
	void OnMedSatchelEmpty( void );

	// ignored healing
	void OnHealGrenadeUsedWell( void );
	void OnMarineHealed( void );
	void OnMarineKilled( void );

	// ignored adrenaline
	void OnMarineUsedAdrenaline( void );
	//void OnMarineMobAttacked( void );

	// ignored secondary
	void OnMarineUsedSecondary( void );
	//void OnMarineMobAttacked( void );

	// ignored welder
	void OnAlienOpenDoor( void );
	void OnMarineWeldedDoor( void );

	// ignored lighting

	// slow progression
	void OnAlienSpawnedInfinite( void );

	// shield bug
	void OnShiedbugBlocked( void );
	void OnShiedbugKilled( void );

	// died alone
	void OnMarineKilledAlone( void );

	// started without any medics
	void OnNoMedicStart( void );

	// Retrieve total fail advice status
	const FailAdviceMessageStatus_t *GetFailAdviceStatus( void );

private:

	FailAdviceMessageStatus_t m_Status[ ASW_FAIL_ADVICE_TOTAL ];

};


CASW_Fail_Advice* ASWFailAdvice();


#endif // _INCLUDED_ASW_FAIL_ADVICE_H