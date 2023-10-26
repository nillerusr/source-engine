#ifndef _INCLUDED_ASW_DIRECTOR_H
#define _INCLUDED_ASW_DIRECTOR_H
#ifdef _WIN32
#pragma once
#endif

#include "igamesystem.h"

// A marine's intensity level (how much tension/excitement they're experiencing)
class CASW_Intensity
{
public:
	CASW_Intensity();

	enum IntensityType
	{
		NONE,
		MILD,
		MODERATE,
		HIGH,
		EXTREME,
		MAXIMUM			// peg the intensity meter
	};

	void  Update( float fDeltaTime );
	void  Increase( IntensityType i );
	float GetCurrent( void ) const;				// return value from 0 (none) to 1 (extreme)
	void  InhibitDecay( float duration );		// inhibit intensity decay for given duration
	void  Reset() { m_flIntensity = 0.0f; }

protected:
	float m_flIntensity;							// current "intensity" from 0 to 1
	CountdownTimer m_decayInhibitTimer;
};

inline void CASW_Intensity::InhibitDecay( float duration )
{
	if ( m_decayInhibitTimer.GetRemainingTime() < duration )
	{
		m_decayInhibitTimer.Start( duration );
	}
}

inline float CASW_Intensity::GetCurrent( void ) const
{
	return m_flIntensity;
}

class CASW_Marine;
class CASW_Spawner;

class CASW_Director : public CAutoGameSystemPerFrame
{
public:
	CASW_Director();
	~CASW_Director();

	virtual bool Init();
	virtual void Shutdown();

	virtual void LevelInitPreEntity();
	virtual void LevelInitPostEntity();
	virtual void FrameUpdatePreEntityThink();
	virtual void FrameUpdatePostEntityThink();

	void Event_AlienKilled( CBaseEntity *pAlien, const CTakeDamageInfo &info );
	void MarineTookDamage( CASW_Marine *pMarine, const CTakeDamageInfo &info, bool bFriendlyFire );
	void OnMarineStartedHack( CASW_Marine *pMarine, CBaseEntity *pComputer );
	void UpdateMarineInsideEscapeRoom( CASW_Marine *pMarine );

	void OnMissionStarted();

	// Spawning hordes of aliens
	void OnHordeFinishedSpawning();

	// intensity access
	float GetMaxIntensity();

	bool CanSpawnAlien( CASW_Spawner *pSpawner );			// if director is controlling alien spawns, then mapper set spawners ask permission before spawning	

	void StartFinale();
	void SetHordesEnabled( bool bHordes ) { m_bHordesEnabled = bHordes; }
	void SetWanderersEnabled( bool bWanderers ) { m_bWanderersEnabled = bWanderers; }

protected:
	void UpdateHorde();
	void UpdateIntensity();
	void UpdateSpawningState();
	void UpdateWanderers();
	void UpdateMarineRooms();

private:
	IntervalTimer m_IntensityUpdateTimer;
	CountdownTimer m_HordeTimer;			// director attempts to spawn a horde when this timer is up
	bool m_bHordeInProgress;

	bool m_bInitialWait;
	bool m_bSpawningAliens;
	bool m_bReachedIntensityPeak;
	CountdownTimer m_SustainTimer;
	float m_fTimeBetweenAliens;
	CountdownTimer m_AlienSpawnTimer;

	// marines are about to escape, throw everything at them
	bool m_bFinale;
	bool m_bWanderersEnabled;
	bool m_bHordesEnabled;
	bool m_bFiredEscapeRoom;
	bool m_bDirectorControlsSpawners;
};

CASW_Director* ASWDirector();

#endif // _INCLUDED_ASW_DIRECTOR_H