//=============================================================================//
//
// Purpose:		Base for husk classes.
//
// Author:		Blixibon
//
//=============================================================================//

#include "cbase.h"

//-----------------------------------------------------------------------------
// 
// Cvars
// 
//-----------------------------------------------------------------------------

ConVar	sk_husk_common_reaction_time_multiplier( "sk_husk_common_reaction_time_multiplier", "1.5", FCVAR_NONE, "How much longer husks should take to notice their enemies, as a multiplier of ai_reaction_delay_idle or ai_reaction_delay_alert" );

ConVar	sk_husk_common_escalation_time( "sk_husk_common_escalation_time", "15", FCVAR_NONE, "How long husks should be suspicious of a neutral target before attacking on their own" );
ConVar	sk_husk_common_escalation_sprint_speed( "sk_husk_common_escalation_sprint_speed", "225", FCVAR_NONE, "Husks get angry at suspicious players faster when they're going this speed" );
ConVar	sk_husk_common_escalation_sprint_delay( "sk_husk_common_escalation_sprint_delay", "-1.2", FCVAR_NONE, "How much to add to/remove from the escalation timer every think a husk sees a player sprinting" );

ConVar	sk_husk_common_infight_time( "sk_husk_common_infight_time", "5", FCVAR_NONE, "How long husks should be angry at their allies after being provoked" );

//-----------------------------------------------------------------------------
// 
// Interactions
// 
//-----------------------------------------------------------------------------

int g_interactionHuskSuspicious;
int g_interactionHuskAngry;
