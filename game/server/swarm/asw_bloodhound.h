#ifndef _INCLUDED_ASW_BLOODHOUND_H
#define _INCLUDED_ASW_BLOODHOUND_H
#ifdef _WIN32
#pragma once
#endif

class CDropshipRope;
class CASW_Marine;

#define ASW_MAX_DROP_MARINES 4

class CASW_Bloodhound : public CBaseAnimating
{
public:
	DECLARE_CLASS( CASW_Bloodhound, CBaseAnimating  );
	DECLARE_DATADESC();
	CASW_Bloodhound();
	virtual ~CASW_Bloodhound();

	virtual void Spawn();
	virtual void Precache();

	// starting the ship on its way
	void StartDropshipSequence();

	// thinking
	virtual void BloodhoundThink();
	virtual void SleepThink();	
	float m_fLastThinkTime;	

	// movement
	bool PerformMovement(float deltatime);
	void SetDestinationPoint();
	void CheckReachedDestination();
	Vector& GetRandomDestinationOffset();

	EHANDLE m_hDestination;
	Vector m_vecDestination;
	Vector m_vecCurrentVelocity;
	Vector m_vecRandomDestinationOffset;
	float m_fNextRandomDestTime;

	// swaying our position
	float m_fSwayTime[3];
	Vector m_vecCurrentSway;
	void UpdateSwayOffset(float delta);

	// state
	void SetState(int iNewState);
	int GetState() { return m_iState; }
	int m_iState;		

	// resetting
	void ResetDropship();
	Vector m_vecStartingPosition;

	// dropping marines
	void DropMarine(int i);
	void MarineLanded(CASW_Marine *pMarine);

	virtual void StopLoopingSounds();
	void StartEngineSound();
	//CSoundPatch	*m_pEngineSound;

	// our rope
	CHandle<CDropshipRope> m_hDropshipRope;

	float m_fNextMarineDropTime;
	CHandle<CASW_Marine> m_hMarines[ASW_MAX_DROP_MARINES];
	int m_iMarinesDropped;
	bool m_bPlayedInPositionLine;
};

enum ASW_Bloodhound
{
	ASW_BLOODHOUND_NONE = 0,
	ASW_BLOODHOUND_DESCENDING,
	ASW_BLOODHOUND_MOVING_TO_DROP_POSITION,
	ASW_BLOODHOUND_UNLOADING,
};

class CASWRopeAnchor : public CPointEntity
{
	DECLARE_CLASS( CASWRopeAnchor, CPointEntity );

public:
	void Spawn( void );
	void FallThink( void );
	void RemoveThink( void );
	EHANDLE m_hRope;

	DECLARE_DATADESC();
};

#endif // _INCLUDED_ASW_BLOODHOUND_H