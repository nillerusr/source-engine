//=//=============================================================================//
//
// Purpose:		Plant-like Xen creature that swings a cruel scythe-like
//				appendage to defend itself.
//
//				These things really made me nervous when I first played Half-Life.
//
// Author:		1upD
//
//=============================================================================//


#include "ai_basenpc.h"
#include "npc_baseflora.h"

class CNPC_XenTree : public CNPC_BaseFlora
{
	DECLARE_CLASS( CNPC_XenTree, CNPC_BaseFlora );

public:
	void Spawn();
	void Precache( void );

	void OnChangeActivity( Activity eNewActivity );
	void HandleAnimEvent( animevent_t *pEvent );

	virtual int SelectExtendSchedule() { return SCHED_MELEE_ATTACK1; }
	virtual int SelectRetractSchedule() { return SCHED_MELEE_ATTACK1; }

protected:
	virtual float GetViewDistance() { return 128.0f; }
	virtual float GetFieldOfView() { return 0.35f; }

private:
};