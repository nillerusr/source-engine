//========= Copyright © 1996-2007, Valve Corporation, All rights reserved. ============//
//
// Purpose: A single behavior that handles running all of the NPC's add ons
//
//=====================================================================================//
/*

#ifndef AI_BEHAVIOR_ADDONHOST_H
#define AI_BEHAVIOR_ADDONHOST_H
#ifdef _WIN32
#pragma once
#endif

#include "ai_behavior.h"

class CAI_AddOn;

//=====================================================================================//
//=====================================================================================//
class CAI_AddOnHostBehavior : public CAI_SimpleBehavior
{
	DECLARE_CLASS( CAI_AddOnHostBehavior, CAI_SimpleBehavior );

public:
	CAI_AddOnHostBehavior();
	virtual const char *GetName() {	return "AddOnHost"; }
	virtual void GatherConditions();
	virtual void GatherConditionsNotActive();

private:
	//----------------------------------------------
	// Conditions gathering
	//----------------------------------------------
	void	GatherConditionsCentral();

public:
	//----------------------------------------------
	// AddOn management
	//----------------------------------------------
	void RegisterAddOn( CAI_AddOn *pAddOn );

private:
	CUtlVector<CHandle<CAI_AddOn>>m_AddOns;

	DECLARE_DATADESC();
};

#endif//AI_BEHAVIOR_ADDONHOST_H
*/
