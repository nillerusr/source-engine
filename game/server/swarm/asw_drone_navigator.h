#ifndef _INCLUDED_ASW_DRONE_NAVIGATOR_H
#define _INCLUDED_ASW_DRONE_NAVIGATOR_H

#include "ai_navigator.h"

class CASW_Drone_Navigator : public CAI_Navigator
{
public:
	DECLARE_CLASS( CASW_Drone_Navigator, CAI_Navigator  );

	CASW_Drone_Navigator(CAI_BaseNPC *pOuter);
	virtual bool		Move( float flInterval = 0.1 );
	virtual bool		ShouldMove( bool bHasAGoal );
	//virtual ~CASW_Drone_Navigator();

	bool MarkCurWaypointFailedLink( void );
};

#endif // _INCLUDED_ASW_DRONE_NAVIGATOR_H