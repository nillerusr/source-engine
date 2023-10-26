#ifndef _INCLUDED_ASW_JUMP_TRIGGER_H
#define _INCLUDED_ASW_JUMP_TRIGGER_H

#include "triggers.h"

class CASW_Drone_Advanced;
class CASW_Alien_Jumper;

#define ASW_MAX_JUMP_DESTS 8

// a volume that causes asw_alien_jump npcs to jump somewhere when they touch it

class CASW_Jump_Trigger : public CTriggerMultiple
{
	DECLARE_CLASS( CASW_Jump_Trigger, CTriggerMultiple );
public:
	CASW_Jump_Trigger();
	void Spawn( void );
	void VolumeTouch( CBaseEntity *pOther );
	bool ReasonableJump(CASW_Alien_Jumper *pJumper, int iJumpNum);

	DECLARE_DATADESC();

	string_t m_JumpDestName;
	Vector m_vecJumpDestination[ASW_MAX_JUMP_DESTS];
	int m_iNumJumpDests;
	float m_fMinMarineDistance;
	bool m_bClearOrders;
	bool m_bCheckEnemyDirection;
	bool m_bOneJumpPerAlien;
	bool m_bRetryFailedJumps;
	bool m_bForceJump;
	QAngle m_ForceAngle;
	float m_fForceSpeed;
};

#endif /* _INCLUDED_ASW_JUMP_TRIGGER_H */