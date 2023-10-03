#include "cbase.h"
#include "asw_drone_navigator.h"
#include "asw_drone_advanced.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


CASW_Drone_Navigator::CASW_Drone_Navigator(CAI_BaseNPC *pOuter) : BaseClass(pOuter)
{
}

// make sure we do the override move if required, even if we have no goal set in the navigator
bool CASW_Drone_Navigator::Move( float flInterval )
{
	if (flInterval > 1.0)
	{
		// Bound interval so we don't get ludicrous motion when debugging
		// or when framerate drops catostrophically
		flInterval = 1.0;
	}

	bool b = BaseClass::Move(flInterval);

	if ((int) GetOuter()->GetTask() == TASK_DRONE_WAIT_FOR_OVERRIDE_MOVE)
	{
		Msg("in navigator move during the right task!\n");
		//GetOuter()->OverrideMove(flInterval);
		//return true;
	}
	return b;
}

bool CASW_Drone_Navigator::ShouldMove( bool bHasAGoal )
{
	// Don't move if we've already run automovement
	if ( GetOuter()->TaskRanAutomovement() )
		return false;

	return BaseClass::ShouldMove(bHasAGoal);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CASW_Drone_Navigator::MarkCurWaypointFailedLink( void )
{
	// We don't mark doors as blocking us, since we want drones to BASH EM!
	if (GetOuter() && GetOuter()->Classify() == CLASS_ASW_DRONE)
		return false;

	return BaseClass::MarkCurWaypointFailedLink();
}