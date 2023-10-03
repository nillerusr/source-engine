#ifndef _INCLUDED_ASW_AI_SENSES_H
#define _INCLUDED_ASW_AI_SENSES_H
#ifdef _WIN32
#pragma once
#endif

#include "ai_senses.h"

class CASW_BaseAI_Senses : public CAI_Senses
{
public:
	virtual int 	LookForHighPriorityEntities( int iDistance ) { return 0; }	// We never look at players
	virtual bool	WaitingUntilSeen( CBaseEntity *pSightEnt );
};

class CASW_Marine_AI_Senses : public CASW_BaseAI_Senses
{
public:
	DECLARE_CLASS( CASW_Marine_AI_Senses, CAI_Senses );

	CASW_Marine_AI_Senses();
	float			GetHeightLook() const				{ return m_flLookHeight; }
	void			SetHeightLook( float flHeightLook ) { m_flLookHeight = flHeightLook; }
	virtual bool	IsWithinSenseDistance( const Vector &source, const Vector &dest, float dist );

	DECLARE_SIMPLE_DATADESC();

	float			m_flLookHeight;			// max vertical distance npc sees (Default 128)
};

class CASW_AI_Senses : public CASW_BaseAI_Senses
{
public:
	DECLARE_CLASS( CASW_AI_Senses, CAI_Senses );

	CASW_AI_Senses();
	virtual ~CASW_AI_Senses();

	DECLARE_SIMPLE_DATADESC();

	virtual	void PerformSensing();

	void SwarmSense(int iDistance);		// the swarm can sense marines through walls within a certain radius
	int SwarmSenseMarines(int iDistance);
	bool CanSwarmSense( CBaseEntity *pSightEnt );
	bool SwarmSenseEntity( CBaseEntity *pSightEnt );

	float			GetDistSwarmSense() const				{ return m_SwarmSenseDist; }
	void			SetDistSwarmSense( float flDistSense ) { m_SwarmSenseDist = flDistSense; }

	CBaseEntity *	GetFirstSwarmSenseEntity( AISightIter_t *pIter, seentype_t iSeenType = SEEN_ALL ) const;
	CBaseEntity *	GetNextSwarmSenseEntity( AISightIter_t *pIter ) const;

	float m_SwarmSenseDist;
	float m_LastSwarmSenseDist;
	float m_TimeLastSwarmSense;

	CUtlVector<EHANDLE> m_SwarmSensedMarines;
	CUtlVector<EHANDLE> *m_SwarmSenseArrays[1];
	CSimTimer		m_SwarmSenseMarinesTimer;
};

#endif /* _INCLUDED_ASW_AI_SENSES_H */