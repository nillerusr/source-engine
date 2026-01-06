//=//=============================================================================//
//
// Purpose:		Passive plants that retract when threatened
//
// Author:		1upD
//
//=============================================================================//


#include "ai_basenpc.h"
#include "npc_baseflora.h"

//=========================================================
// monster-specific tasks
//=========================================================
enum
{
	TASK_HAIR_EXTEND = LAST_SHARED_TASK + 1,
	TASK_HAIR_RETRACT
};

//=========================================================
// monster-specific schedule types
//=========================================================
enum
{
	SCHED_HAIR_EXTEND = LAST_SHARED_SCHEDULE + 1,
	SCHED_HAIR_RETRACT
};

class CNPC_XenHair : public CNPC_BaseFlora
{
	DECLARE_CLASS( CNPC_XenHair, CNPC_BaseFlora );
	DECLARE_DATADESC();

public:
	void Spawn();
	void Precache( void );

	virtual int SelectExtendSchedule() { return SCHED_HAIR_EXTEND; }
	virtual int SelectRetractSchedule() { return SCHED_HAIR_RETRACT; }

	virtual void StartTask ( const Task_t *pTask );
	virtual void RunTask ( const Task_t *pTask );

	virtual void AlertSound( void );

protected:
	virtual float GetViewDistance() { return 128.0f; }
	virtual float GetFieldOfView() { return -1.0f; /* 360 degrees */ }

	float m_flHairVelocity;
	float m_flIdealHeight;

	DEFINE_CUSTOM_AI;

private:
};