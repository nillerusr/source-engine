//=============================================================================//
//
// Purpose:		Monsters like bullsquids lay eggs. This NPC represents
//				an egg that has a physics model and can react to its surroundings.
//
// Author:		1upD
//
//=============================================================================//

#include "ai_basenpc.h"
#include "npc_baseflora.h"
#include "player_pickup.h"

//=========================================================
// monster-specific tasks
// TODO Unimplemented
//=========================================================
enum
{
	TASK_EGG_HATCH
};

//=========================================================
// monster-specific schedule types
// TODO Unimplemented
//=========================================================
enum
{
	SCHED_EGG_HATCH = LAST_SHARED_SCHEDULE + 1,
};

class CNPC_Egg : public CNPC_BaseFlora, public CDefaultPlayerPickupVPhysics
{
	DECLARE_CLASS( CNPC_Egg, CNPC_BaseFlora );
	DECLARE_DATADESC();

public:
	CNPC_Egg();

	void Spawn();
	void Precache( void );
	bool CreateVPhysics( void );

	virtual int SelectExtendSchedule() { return SCHED_IDLE_STAND; }
	virtual int SelectRetractSchedule() { return SCHED_EGG_HATCH; }

	virtual void StartTask ( const Task_t *pTask );
	virtual void RunTask ( const Task_t *pTask );

	virtual void AlertSound( void );

	bool SpawnNPC( );

	bool		ShouldGib( const CTakeDamageInfo &info ) { return true; }
	bool		CorpseGib( const CTakeDamageInfo &info );

protected:
	virtual float GetViewDistance() { return 128.0f; }
	virtual float GetFieldOfView() { return -1.0f; /* 360 degrees */ }

	void HatchThink();

	float m_flIncubation;
	float m_flNextHatchTime;

	string_t m_iszAlertSound;
	string_t m_iszHatchSound;
	string_t m_isChildClassname;
	string_t m_isHatchParticle;

	string_t m_BabyModelName;
	string_t m_AdultModelName;

	int m_iChildSkin;

	COutputEvent m_OnSpawnNPC; // Output when a predator completes spawning offspring

	DEFINE_CUSTOM_AI;
};