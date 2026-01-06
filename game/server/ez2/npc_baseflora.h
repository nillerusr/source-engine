//=============================================================================//
//
// Purpose:		Base class for plant NPCs, specifically Xen plants
//
// Author:		1upD
//
//=============================================================================//

#include "ai_basenpc.h"


//-----------------------------------------------------------------------------
// monster-specific Conditions
//-----------------------------------------------------------------------------
enum
{
	COND_FLORA_RETRACT = LAST_SHARED_CONDITION + 1,
	COND_FLORA_EXTEND
};
class CNPC_BaseFlora : public CAI_BaseNPC
{
	DECLARE_CLASS( CNPC_BaseFlora, CAI_BaseNPC );
	DECLARE_DATADESC();

public:
	void Spawn();
	void Precache( void );

	Disposition_t IRelationType( CBaseEntity *pTarget );

	virtual int		OnTakeDamage_Alive( const CTakeDamageInfo &info );

	// Plants can't turn
	float MaxYawSpeed ( void ) { return 0.0f; }

	virtual Class_T Classify() { return CLASS_ALIEN_FLORA; }
	virtual bool IsRetracted() { return m_bIsRetracted; }

	virtual void	GatherConditions( void );

	virtual int SelectSchedule( void );
	virtual int SelectExtendSchedule() { return SCHED_NONE; }
	virtual int SelectRetractSchedule() { return SCHED_NONE; }

	void BuildScheduleTestBits();

	bool HasStartleCondition();

	CBaseEntity *GetRetractActivator();

	virtual	float		CalcIdealYaw( const Vector &vecTarget ) { return UTIL_AngleMod( GetLocalAngles().y ); } // Plants can't turn, so the ideal yaw is the current yaw

	int GetSoundInterests ( void );

protected:
	virtual float GetViewDistance() { return 0.0f; }
	virtual float GetFieldOfView() { return 0.0f; }

	COutputEvent m_OnRise;
	COutputEvent m_OnLower;

	bool m_bInvincible;
	bool m_bIsRetracted;

	// Stay retracted until this time
	float m_flRetractUntil;

	// This is used if the one who made us retract is not in enemy memory
	EHANDLE m_hLastAttacker;

	DEFINE_CUSTOM_AI;

};