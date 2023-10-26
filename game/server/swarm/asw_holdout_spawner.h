#ifndef _INCLUDED_ASW_HOLDOUT_SPAWNER_H
#define _INCLUDED_ASW_HOLDOUT_SPAWNER_H
#ifdef _WIN32
#pragma once
#endif

#include "asw_base_spawner.h"

//===============================================
//  A spawner used in holdout mode.
//===============================================
class CASW_Holdout_Spawner : public CASW_Base_Spawner
{
public:
	DECLARE_CLASS( CASW_Holdout_Spawner, CASW_Base_Spawner );
	DECLARE_DATADESC();

	CASW_Holdout_Spawner();
	virtual ~CASW_Holdout_Spawner();

	virtual void Spawn();

	Class_T Classify() { return (Class_T) CLASS_ASW_HOLDOUT_SPAWNER; }

protected:

};

#endif /* _INCLUDED_ASW_HOLDOUT_SPAWNER_H */