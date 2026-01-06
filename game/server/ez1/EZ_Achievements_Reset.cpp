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

void CC_SAPI_EZRESET(void)
{
	steamapicontext->SteamUserStats()->ResetAllStats(true);
}
static ConCommand ez_sapi_resetall("ez_sapi_resetall", CC_SAPI_EZRESET, "Attempts to reset achievements for Entropy Zero.");

void CC_SAPI_EZREQUEST(void)
{
	steamapicontext->SteamUserStats()->RequestCurrentStats();
}
static ConCommand ez_sapi_requestall("ez_sapi_requestall", CC_SAPI_EZRESET, "Attempts to get store achievements for Entropy Zero.");

void CC_SAPI_EZSTORESTATS(void)
{
	steamapicontext->SteamUserStats()->StoreStats();
}
static ConCommand ez_sapi_storestats("ez_sapi_storestats", CC_SAPI_EZRESET, "Attempts to send the changed stats and achievements data to the server for permanent storage.");

void CC_SAPI_EZCLEARACHS(void)
{
	steamapicontext->SteamUserStats()->ClearAchievement("ACH_EZ_HULA");
	steamapicontext->SteamUserStats()->ClearAchievement("ACH_EZ_POEM");
	steamapicontext->SteamUserStats()->ClearAchievement("ACH_EZ_TCAN");
	steamapicontext->SteamUserStats()->ClearAchievement("ACH_EZ_NXTGEN");
	steamapicontext->SteamUserStats()->ClearAchievement("ACH_EZ_SCHISM");
	steamapicontext->SteamUserStats()->ClearAchievement("ACH_EZ_SLUCK");
	steamapicontext->SteamUserStats()->ClearAchievement("ACH_EZ_BSJOB");
	steamapicontext->SteamUserStats()->ClearAchievement("ACH_EZ_OICE");
	steamapicontext->SteamUserStats()->ClearAchievement("ACH_EZ_CLASSY");
	steamapicontext->SteamUserStats()->ClearAchievement("ACH_EZ_TENSC");
	steamapicontext->SteamUserStats()->ClearAchievement("ACH_EZ_TWENTYSC");
	steamapicontext->SteamUserStats()->ClearAchievement("ACH_EZ_THIRTYSC");
	steamapicontext->SteamUserStats()->ClearAchievement("ACH_EZ_FORTYSC");
	steamapicontext->SteamUserStats()->ClearAchievement("ACH_EZ_FIFTYSC");
}
static ConCommand ez_sapi_clearachs("ez_sapi_clearachs", CC_SAPI_EZRESET, "Attempts to reset achievements for Entropy Zero.");