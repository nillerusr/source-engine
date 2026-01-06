//=============================================================================//
//
// Purpose:		Glowing Xen plants that retract into the ground and go dark
//				when threatened.
//
//				They are able to alert squadmates when an intruder is near.
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
	TASK_LIGHTSTALK_EXTEND = LAST_SHARED_TASK + 1,
	TASK_LIGHTSTALK_RETRACT
};

//=========================================================
// monster-specific schedule types
//=========================================================
enum
{
	SCHED_LIGHTSTALK_EXTEND = LAST_SHARED_SCHEDULE + 1,
	SCHED_LIGHTSTALK_RETRACT
};

class CNPC_LightStalk : public CNPC_BaseFlora
{
	DECLARE_CLASS( CNPC_LightStalk, CNPC_BaseFlora );
	DECLARE_DATADESC();

public:
	void Precache( void );

	virtual int SelectExtendSchedule() { return SCHED_LIGHTSTALK_EXTEND; }
	virtual int SelectRetractSchedule() { return SCHED_LIGHTSTALK_RETRACT; }

	virtual void StartTask ( const Task_t *pTask );
	virtual void RunTask ( const Task_t *pTask );

	virtual Activity NPC_TranslateActivity( Activity eNewActivity );

	virtual void AlertSound( void );

#ifdef EZ
	// Lightstalks NEVER get displaced
	// This is because their lights wouldn't follow them. Trees are fine.
	virtual bool	IsDisplacementImpossible() { return true; }
#endif

protected:
	EyeGlow_t	* GetEyeGlowData( int i );
	virtual float GetViewDistance() { return 128.0f; }
	virtual float GetFieldOfView() { return -1.0f; /* 360 degrees */ }

	DEFINE_CUSTOM_AI;

private:
	color32 m_LightColor;
};