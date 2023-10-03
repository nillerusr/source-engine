#ifndef _INCLUDED_ASW_MISSION_MANAGER_H
#define _INCLUDED_ASW_MISSION_MANAGER_H
#pragma once

#include "baseentity.h"


class CASW_Marine_Profile;
class CASW_Marine;
class CASW_Player;
class CASW_Egg;
class CASW_Alien;
class CASW_Alien_Goo;
class CASW_Objective_Escape;

// This class notifies objectives of important occurances in the game (aliens, eggs, marines dying, etc).
//   and will make the mission fail/complete as objectives change status, or if all marines die

class CASW_Mission_Manager : public CBaseEntity
{
public:
	DECLARE_CLASS( CASW_Mission_Manager, CBaseEntity );
	DECLARE_DATADESC();

	CASW_Mission_Manager();
	virtual ~CASW_Mission_Manager();

	virtual void Spawn();
	virtual void ManagerThink();

	bool CheckMissionComplete();
	virtual void AlienKilled(CBaseEntity *pAlien);
	virtual void MarineKilled(CASW_Marine *pMarine);
	virtual void EggKilled(CASW_Egg *pEgg);
	virtual void MissionStarted(void);
	virtual void MissionFail(void);
	virtual void MissionSuccess(void);
	virtual void GooKilled(CASW_Alien_Goo *pGoo);
	virtual bool AllMarinesDead();
	virtual void CheatCompleteMission();
	virtual CASW_Objective_Escape* GetEscapeObjective();

	bool m_bAllMarinesDead;
	float m_flLastMarineDeathTime;
	bool m_bDoneLeavingChatter;

	CHandle<CASW_Objective_Escape> m_hEscapeObjective;
};

#endif /* _INCLUDED_ASW_MISSION_MANAGER_H */