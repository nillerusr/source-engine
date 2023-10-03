#ifndef _DEFINED_ASW_ALIEN_JUMPER_H
#define _DEFINED_ASW_ALIEN_JUMPER_H
#ifdef _WIN32
#pragma once
#endif

#include "asw_alien_shover.h"

// this class implements aliens that can jump

#define	ALIEN_SHOVER_FARTHEST_PHYSICS_OBJECT	350

class CASW_Alien_Jumper : public CASW_Alien_Shover
{
public:

	DECLARE_CLASS( CASW_Alien_Jumper, CASW_Alien_Shover  );	
	DECLARE_DATADESC();
	CASW_Alien_Jumper();

	DEFINE_CUSTOM_AI;

	void ManageFleeCapabilities( bool bEnable );
	void HandleAnimEvent( animevent_t *pEvent );
	bool IsUnusableNode(int iNodeID, CAI_Hint *pHint);
	bool AllowedToBePushed( void );
	void StartTask( const Task_t *pTask );
	void RunTask( const Task_t *pTask );
	void Spawn();
	bool CheckLanding();
	void CreateDust( bool placeDecal );
	bool OnObstructionPreSteer( AILocalMoveGoal_t *pMoveGoal, float distClear, AIMoveResult_t *pResult );
	virtual int		SelectCombatSchedule( void );
	void BuildScheduleTestBits( void );
	void PrescheduleThink();
	bool		IsJumpLegal( const Vector &startPos, const Vector &apex, const Vector &endPos ) const;
	float		GetMaxJumpSpeed() const { return 1024.0f; }
	float		m_flNextJumpPushTime;
	void	InputDisableJump( inputdata_t &inputdata );
	void	InputEnableJump( inputdata_t &inputdata );
	virtual bool	ShouldJump( void );
	bool IsJumping();
	void	StartJump( void );
	void	LockJumpNode( void );
	virtual bool DoJumpOffHead();
	virtual bool DoJumpTo( Vector &vecDest );	// causes an immediate jump to this vector, if a viable jump
	virtual bool DoForcedJump( Vector &vecVelocity );
	virtual void WaitAndRetryJump(Vector &vecDest);
	float	m_flJumpTime;

	virtual float GetMinJumpHeight() const	{ return 64; }

	Vector		m_vecSavedJump;
	Vector		m_vecLastJumpAttempt;
	bool		m_bDisableJump;
	bool		m_bForcedStuckJump;
	bool		m_bTriggerJumped;	// has this jumper done a trigger jump?

	enum
	{
		COND_ASW_ALIEN_CAN_JUMP = BaseClass::NEXT_CONDITION,
		NEXT_CONDITION,
	};

protected:

};

enum
{
	SCHED_ASW_ALIEN_JUMP = LAST_ASW_ALIEN_SHOVER_SHARED_SCHEDULE,
	SCHED_ASW_WAIT_AND_RETRY_JUMP,
	LAST_ASW_ALIEN_JUMPER_SHARED_SCHEDULE,
};

enum
{
	TASK_ASW_ALIEN_JUMP = LAST_ASW_ALIEN_SHOVER_SHARED_TASK,
	TASK_ASW_ALIEN_FACE_JUMP,
	TASK_ASW_ALIEN_RETRY_JUMP,
	LAST_ASW_ALIEN_JUMPER_SHARED_TASK,
};

#endif // _DEFINED_ASW_ALIEN_JUMPER_H
