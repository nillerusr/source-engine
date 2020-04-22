//========= Copyright Valve Corporation, All rights reserved. ============//
// tf_bot_mission_suicide_bomber.h
// Move to target and explode
// Michael Booth, October 2011

#ifndef TF_BOT_MISSION_REPROGRAMMED_H
#define TF_BOT_MISSION_REPROGRAMMED_H

#include "Path/NextBotPathFollow.h"

class CTFBotMissionReprogrammed : public Action< CTFBot >
{
#ifdef STAGING_ONLY
public:
	CTFBotMissionReprogrammed( void );

	virtual ActionResult< CTFBot >	OnStart( CTFBot *me, Action< CTFBot > *priorAction );
	virtual ActionResult< CTFBot >	Update( CTFBot *me, float interval );
	virtual void					OnEnd( CTFBot *me, Action< CTFBot > *nextAction );

	virtual EventDesiredResult< CTFBot > OnStuck( CTFBot *me );
	virtual EventDesiredResult< CTFBot > OnKilled( CTFBot *me, const CTakeDamageInfo &info );

	virtual QueryResultType ShouldAttack( const INextBot *me, const CKnownEntity *them ) const;	// should we attack "them"?

	virtual const char *GetName( void ) const	{ return "MissionReprogrammed"; };

private:
	CHandle< CBaseEntity > m_victim;	// the victim we are trying to destroy
	Vector m_lastKnownVictimPosition;

	PathFollower m_path;
	CountdownTimer m_repathTimer;

	CountdownTimer m_talkTimer;
	CountdownTimer m_detonateTimer;
	CountdownTimer m_detonateSeekTimer;
	CountdownTimer m_reprogrammedTimer;

	void StartDetonate( CTFBot *me, bool wasSuccessful = false );
	void Detonate( CTFBot *me );
	CTFPlayer *FindNearestEnemy( CTFBot *me );
	bool m_hasDetonated;
	bool m_wasSuccessful;

	int m_consecutivePathFailures;

	Vector m_vecDetLocation;
#endif // STAGING_ONLY
};


#endif // TF_BOT_MISSION_REPROGRAMMED_H
