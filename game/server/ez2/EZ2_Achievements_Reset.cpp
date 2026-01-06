// Breadman all Breadman
// Sets up a command that we can call from the console to clear steam achievements. For development purposes only.

#include "cbase.h"
#include "ai_basenpc.h"
#include "player.h"
#include "entitylist.h"
#include "ai_networkmanager.h"
#include "ndebugoverlay.h"
#include "datacache/imdlcache.h"
#include "achievementmgr.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

void CC_SAPI_EZ2RESET(void)
{
	steamapicontext->SteamUserStats()->ResetAllStats(true);
}
static ConCommand ez2_sapi_resetall("ez2_sapi_resetall", CC_SAPI_EZ2RESET, "Attempts to reset achievements for Entropy Zero 2.");

void CC_SAPI_EZ2REQUEST(void)
{
	steamapicontext->SteamUserStats()->RequestCurrentStats();
}
static ConCommand ez2_sapi_requestall("ez2_sapi_requestall", CC_SAPI_EZ2RESET, "Attempts to get store achievements for Entropy Zero 2.");

void CC_SAPI_EZ2STORESTATS(void)
{
	steamapicontext->SteamUserStats()->StoreStats();
}
static ConCommand ez2_sapi_storestats("ez2_sapi_storestats", CC_SAPI_EZ2RESET, "Attempts to send the changed stats and achievements data to the server for permanent storage.");

void CC_SAPI_EZ2CLEARACHS(void)
{
	steamapicontext->SteamUserStats()->ClearAchievement("ACH_EZ2_RECORDING");
	steamapicontext->SteamUserStats()->ClearAchievement("ACH_EZ2_NMODE");
	steamapicontext->SteamUserStats()->ClearAchievement("ACH_EZ2_HMODE");
	steamapicontext->SteamUserStats()->ClearAchievement("ACH_EZ2_CH0");
	steamapicontext->SteamUserStats()->ClearAchievement("ACH_EZ2_CH1");
	steamapicontext->SteamUserStats()->ClearAchievement("ACH_EZ2_CH2");
	steamapicontext->SteamUserStats()->ClearAchievement("ACH_EZ2_CH3");
	steamapicontext->SteamUserStats()->ClearAchievement("ACH_EZ2_CH4");
	steamapicontext->SteamUserStats()->ClearAchievement("ACH_EZ2_CH4a");
	steamapicontext->SteamUserStats()->ClearAchievement("ACH_EZ2_CH5");
	steamapicontext->SteamUserStats()->ClearAchievement("ACH_EZ2_CH6");
	steamapicontext->SteamUserStats()->ClearAchievement("ACH_EZ2_STILL_ALIVE");
	steamapicontext->SteamUserStats()->ClearAchievement("ACH_EZ2_SPARE_CC");
	steamapicontext->SteamUserStats()->ClearAchievement("ACH_EZ2_MEET_WILSON");
	steamapicontext->SteamUserStats()->ClearAchievement("ACH_EZ2_DELIVER_WILSON");
	steamapicontext->SteamUserStats()->ClearAchievement("ACH_EZ2_MIND_WIPE");
	steamapicontext->SteamUserStats()->ClearAchievement("ACH_EZ2_SUPERFUTURE");
	steamapicontext->SteamUserStats()->ClearAchievement("ACH_EZ2_KILL_BEAST");
	steamapicontext->SteamUserStats()->ClearAchievement("ACH_EZ2_KILL_TWOGONOMES");
	steamapicontext->SteamUserStats()->ClearAchievement("ACH_EZ2_ADVISOR_DEAD");
	steamapicontext->SteamUserStats()->ClearAchievement("ACH_EZ2_KILL_ALIENSWXBOW");
	steamapicontext->SteamUserStats()->ClearAchievement("ACH_EZ2_XENGRENADE_HELICOPTER");
	steamapicontext->SteamUserStats()->ClearAchievement("ACH_EZ2_DELIVER_LONEWOLF");
	steamapicontext->SteamUserStats()->ClearAchievement("ACH_EZ2_SEE_BOREALIS");
	steamapicontext->SteamUserStats()->ClearAchievement("ACH_EZ2_MEET_CC");
	steamapicontext->SteamUserStats()->ClearAchievement("ACH_EZ2_KILL_REBELSW357");
	steamapicontext->SteamUserStats()->ClearAchievement("ACH_EZ2_KILL_JUMPREBELMIDAIR");
	steamapicontext->SteamUserStats()->ClearAchievement("ACH_EZ2_KILL_TEMPORALCRABS");
	steamapicontext->SteamUserStats()->ClearAchievement("ACH_EZ2_XENGRENADE_WEIGHT");
	steamapicontext->SteamUserStats()->ClearAchievement("ACH_EZ2_KICK_DOORS");
	steamapicontext->SteamUserStats()->ClearAchievement("ACH_EZ2_KICK_ADVISOR");
	steamapicontext->SteamUserStats()->ClearAchievement("ACH_EZ2_SQUAD_CH1");
	steamapicontext->SteamUserStats()->ClearAchievement("ACH_EZ2_SQUAD_CH2");
	steamapicontext->SteamUserStats()->ClearAchievement("ACH_EZ2_FLIP_APC");
	steamapicontext->SteamUserStats()->ClearAchievement("ACH_EZ2_SURRENDERED_HEAL");
	steamapicontext->SteamUserStats()->ClearAchievement("ACH_EZ2_WILSON_CLOSETS");
}
static ConCommand ez2_sapi_clearachs("ez2_sapi_clearachs", CC_SAPI_EZ2RESET, "Attempts to reset achievements for Entropy Zero 2.");