#ifndef _DEFINED_ASW_SENTRY_TOP_CANNON_H
#define _DEFINED_ASW_SENTRY_TOP_CANNON_H
#pragma once

#include "asw_sentry_top.h"

class CASW_Sentry_Top_Cannon : public CASW_Sentry_Top
{
public:
	DECLARE_CLASS( CASW_Sentry_Top_Cannon, CASW_Sentry_Top );
	// DECLARE_SERVERCLASS();
	DECLARE_DATADESC();


	CASW_Sentry_Top_Cannon();
	// virtual ~CASW_Sentry_Top_Machinegun();
	virtual void SetTopModel();
	virtual void Fire();

	// helper function used by FindEnemy
	virtual CAI_BaseNPC *SelectOptimalEnemy() ;

	// Classification
	virtual Class_T		Classify( void ) { return (Class_T) CLASS_ASW_SENTRY_CANNON; }

};


#endif /* _DEFINED_ASW_SENTRY_TOP_H */