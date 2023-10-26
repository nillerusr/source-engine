#ifndef _DEFINED_ASW_SENTRY_TOP_ICER_H
#define _DEFINED_ASW_SENTRY_TOP_ICER_H
#pragma once

#include "asw_sentry_top_flamer.h"

class CASW_Sentry_Top_Icer : public CASW_Sentry_Top_Flamer
{
public:
	DECLARE_CLASS( CASW_Sentry_Top_Icer, CASW_Sentry_Top_Flamer );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();


	CASW_Sentry_Top_Icer();
	// virtual ~CASW_Sentry_Top_Icer();


	/// dq enemies that are already frozen
	virtual bool IsValidEnemy( CAI_BaseNPC *pNPC );

	virtual void SetTopModel();

	int GetSentryDamage();

	virtual ITraceFilter *GetVisibilityTraceFilter();

	// Classification
	virtual Class_T		Classify( void ) { return (Class_T) CLASS_ASW_SENTRY_FREEZE; }

protected:
	// helper function used by FindEnemy
	virtual CAI_BaseNPC *SelectOptimalEnemy() ;

	virtual void FireProjectiles( int numShotsToFire,
		const Vector &vecSrc, const Vector &vecAiming, const AngularImpulse &rotSpeed = AngularImpulse(0,0,720)	);

	float m_flEnemyOverfreezePermittedUntil;

};


#endif /* _DEFINED_ASW_SENTRY_TOP_ICER_H */
