//========= Copyright © 1996-2007, Valve Corporation, All rights reserved. ============//
//
// Purpose: A single behavior that handles running all of the NPC's add ons
//
//=====================================================================================//
/*
#include "cbase.h"
#include "ai_behavior_addonhost.h"
#include "ai_addon.h"
#include "saverestore_utlvector.h"

BEGIN_DATADESC(CAI_AddOnHostBehavior)
	DEFINE_UTLVECTOR(m_AddOns, FIELD_EHANDLE),
END_DATADESC()

//---------------------------------------------------------
//---------------------------------------------------------
CAI_AddOnHostBehavior::CAI_AddOnHostBehavior()
{

}

//---------------------------------------------------------
//---------------------------------------------------------
void CAI_AddOnHostBehavior::GatherConditions()
{
	BaseClass::GatherConditions();
	GatherConditionsCentral();
}

//---------------------------------------------------------
//---------------------------------------------------------
void CAI_AddOnHostBehavior::GatherConditionsNotActive()
{
	BaseClass::GatherConditionsNotActive();
	GatherConditionsCentral();
}

//---------------------------------------------------------
//---------------------------------------------------------
void CAI_AddOnHostBehavior::GatherConditionsCentral()
{
	// blah blah	
}

//---------------------------------------------------------
//---------------------------------------------------------
void CAI_AddOnHostBehavior::RegisterAddOn( CAI_AddOn *pAddOn )
{
	Assert( pAddOn != NULL );
	Assert( m_AddOns.Find(pAddOn) == -1 );
	m_AddOns.AddToTail( pAddOn );
}
*/
