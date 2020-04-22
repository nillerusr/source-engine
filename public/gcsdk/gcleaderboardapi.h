//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: API to interact with Steam leaderboards on the GC.
//
// $NoKeywords: $
//=============================================================================

#ifndef GCLEADERBOARDAPI_H
#define GCLEADERBOARDAPI_H

namespace GCSDK
{
	class CGCBase;

	enum { kInvalidLeaderboardID = 0 };

	/**
	 * Yielding call that attempts to find a leaderboard by name, creating one if necessary.
	 * @param pName
	 * @param eLeaderboardSortMethod
	 * @param eLeaderboardDisplayType
	 * @param bCreateIfNotFound
	 * @return 0 if the leaderboard was not found, > 0 otherwise
	 */
	uint32 Leaderboard_YieldingFind( const char *pName, ELeaderboardSortMethod eLeaderboardSortMethod, ELeaderboardDisplayType eLeaderboardDisplayType, bool bCreateIfNotFound );

	/**
	 * Yielding call that attempts to set the score for the steamID in the leaderboard.
	 * @param unLeaderboardID
	 * @param steamID
	 * @param eLeaderboardUploadScoreMethod
	 * @param score
	 * @param pDetails
	 * @param unDetailsLength
	 * @return true if successful, false otherwise.
	 */
	bool Leaderboard_YieldingSetScore( uint32 unLeaderboardID, const CSteamID &steamID, ELeaderboardUploadScoreMethod eLeaderboardUploadScoreMethod, int score );
}; // namespace GCSDK

#endif // GCLEADERBOARDAPI_H
