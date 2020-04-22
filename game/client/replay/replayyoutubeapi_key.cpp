//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//

#include "cbase.h"


void GetYouTubeAPIKey( const char *pGameDir, bool bOnSteamPublic, const char **ppSource, const char **ppDeveloperTag, const char **ppDeveloperKey )
{
	// Team Fortress 2?
	if ( FStrEq( pGameDir, "tf" ) )
	{
		*ppSource = "Team Fortress 2";
		*ppDeveloperTag = "TF2";
		*ppDeveloperKey = bOnSteamPublic ?
			"AI39si6dQGxX2TWkOT9V9ihFpKmokaDhqIw3mgJcnnRFjX5f00HMRXqj69Fg1ulTdYF9Aw4wIt5NYHCxQAXHPPEcbQ89rEaCeg" :
		"AI39si5q3V-l7DhNbdSn-F2P3l0sUOIOnHBqJC5kvdGsuwpIinmcOH5GFC1PGG0olcZM2ID0Fvbsodj6g0pOUkKhuRNxcEErLQ";
	}
	// Team Fortress 2 Beta?
	else if ( FStrEq( pGameDir, "tf_beta" ) )
	{
		*ppSource = "Team Fortress 2 Beta";
		*ppDeveloperTag = "TF2";
		*ppDeveloperKey = bOnSteamPublic ?
			"AI39si7XuLuXg3-2T06aVUaM-45HSXYFqzXfyPR6y5K4XotWKf78lfCByXnD1T8Kj9jeIR85NelpjGYGsH8pR3RO4k3TrwlTbw" :
		"AI39si79TOshUv9FcIT6cjEH0Q9KK_eEOH1q6-_lIdNIsmHrKcal1R8Uf0TzMhdq-rQ7iUEZ3uqSKlLXa2J-lQvuKwNq5SSnMA";
	}
	// Counter-Strike: Source?
	else if ( FStrEq( pGameDir, "cstrike" ) )
	{
		*ppSource = "Counter-Strike: Source";
		*ppDeveloperTag = "CSS";
		*ppDeveloperKey = bOnSteamPublic ?
			"AI39si7JIn2nj67YoxWPzmsGHO2R-WSpN0su1f3-BF9lVC5jWz9DEOPbOxFz-MiXuaMtoCZnS3nSPjwGfqHenXC6RKGcICI5HQ" :
		"AI39si4bpW1q3D0gcWWNWFNHjHgsM7YL3RGCdEAIKV2k_mH5Cwj-YwXinVv933tFhy-6583HcuZ8NWTrvfbB8XTWEI-hYidEjA";
	}
	// Counter-Strike: Source Beta?
	else if ( FStrEq( pGameDir, "cstrike_beta" ) )
	{
		*ppSource = "Counter-Strike: Source Beta";
		*ppDeveloperTag = "CSS";
		*ppDeveloperKey = bOnSteamPublic ?
			"AI39si5JUyCvGdavFg02OusWk9lkSxkpEX99KnJCjTwOzLJH7N9MS40YMFk-o8dTwyO0w2Qi2CSU8qrB4bdTohHj35mAa0iMDQ" :
		"AI39si4Oq2O35MHD5qahqODCKQfsssq5STE6ISolJKsvFuxtPpqhG4sQbDF8pGdZ02c-_s5KRB5nhTjqZMLB4h0lKKHh8I52Tg";
	}
}
