//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#include "stdafx.h"

#include "gcsdk/enumutils.h"
#include "gcsdk/gcbase.h"
#include "gcsdk/http.h"
#include "gcsdk/job.h"
#include "steam/isteamuserstats.h"
#include "gcleaderboardapi.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace GCSDK;

// @note Tom Bui: copied from steam
ENUMSTRINGS_START( ELeaderboardSortMethod )
{ k_ELeaderboardSortMethodNone, "" },
{ k_ELeaderboardSortMethodAscending, "Ascending" },
{ k_ELeaderboardSortMethodDescending, "Descending" },
ENUMSTRINGS_REVERSE( ELeaderboardSortMethod, k_ELeaderboardSortMethodNone )

ENUMSTRINGS_START( ELeaderboardDisplayType )
{ k_ELeaderboardDisplayTypeNone, "" }, 
{ k_ELeaderboardDisplayTypeNumeric, "Numeric" },
{ k_ELeaderboardDisplayTypeTimeSeconds, "Seconds" },
{ k_ELeaderboardDisplayTypeTimeMilliSeconds, "MilliSeconds" },
ENUMSTRINGS_REVERSE( ELeaderboardDisplayType, k_ELeaderboardDisplayTypeNone )

ENUMSTRINGS_START( ELeaderboardUploadScoreMethod )
{ k_ELeaderboardUploadScoreMethodNone, "" }, 
{ k_ELeaderboardUploadScoreMethodKeepBest, "KeepBest" },
{ k_ELeaderboardUploadScoreMethodForceUpdate, "ForceUpdate" },
ENUMSTRINGS_REVERSE( ELeaderboardUploadScoreMethod, k_ELeaderboardUploadScoreMethodNone )

namespace GCSDK
{
	uint32 Leaderboard_YieldingFind( const char *pName, ELeaderboardSortMethod eLeaderboardSortMethod, ELeaderboardDisplayType eLeaderboardDisplayType, bool bCreateIfNotFound )
	{
		CSteamAPIRequest apiRequest( k_EHTTPMethodPOST, "ISteamLeaderboards", "FindOrCreateLeaderboard", 1 );
		apiRequest.SetPOSTParamUInt32( "appid", GGCBase()->GetAppID() );
		apiRequest.SetPOSTParamString( "name", pName );

		apiRequest.SetPOSTParamString( "sortmethod", PchNameFromELeaderboardSortMethod( eLeaderboardSortMethod ) );
		apiRequest.SetPOSTParamString( "displaytype", PchNameFromELeaderboardDisplayType( eLeaderboardDisplayType ) );
		apiRequest.SetPOSTParamBool( "createifnotfound", bCreateIfNotFound );

		KeyValuesAD kvAPIResponse( "response" );
		const EResult eCallResult = GGCBase()->YieldingSendHTTPRequestKV( &apiRequest, kvAPIResponse );
		const EResult eAPIResult = eCallResult == k_EResultOK ? static_cast<EResult>( kvAPIResponse->GetInt( "result", k_EResultFail ) ) : k_EResultFail;
		if ( eAPIResult == k_EResultOK )
		{
			KeyValues *pKVEntry = kvAPIResponse->FindKey( pName );
			if ( pKVEntry )
			{
				return pKVEntry->GetInt( "leaderBoardID", kInvalidLeaderboardID );
			}
		}
		return kInvalidLeaderboardID;
	}

	bool Leaderboard_YieldingSetScore( uint32 unLeaderboardID, const CSteamID &steamID, ELeaderboardUploadScoreMethod eLeaderboardUploadScoreMethod, int score )
	{
		Assert( unLeaderboardID != kInvalidLeaderboardID );
		if ( unLeaderboardID == kInvalidLeaderboardID )
			return false;

		CSteamAPIRequest apiRequest( k_EHTTPMethodPOST, "ISteamLeaderboards", "SetLeaderboardScore", 1 );
		apiRequest.SetPOSTParamUInt32( "appid", GGCBase()->GetAppID() );
		apiRequest.SetPOSTParamUInt32( "leaderboardid", unLeaderboardID );
		apiRequest.SetPOSTParamUInt64( "steamid", steamID.ConvertToUint64() );
		apiRequest.SetPOSTParamInt32( "score", score );

		apiRequest.SetPOSTParamString( "scoremethod", PchNameFromELeaderboardUploadScoreMethod( eLeaderboardUploadScoreMethod ) );

		KeyValuesAD kvAPIResponse( "response" );
		const EResult eCallResult = GGCBase()->YieldingSendHTTPRequestKV( &apiRequest, kvAPIResponse );
		const EResult eAPIResult = eCallResult == k_EResultOK ? static_cast<EResult>( kvAPIResponse->GetInt( "result", k_EResultFail ) ) : k_EResultFail;
		if ( eAPIResult == k_EResultOK )
			return true;

		EmitError( SPEW_GC, __FUNCTION__ ": error code %u/%u setting leaderboard %u to %i (%s) for user '%s'.\n",
				   eCallResult, eAPIResult, unLeaderboardID, score, PchNameFromELeaderboardUploadScoreMethod( eLeaderboardUploadScoreMethod ), steamID.Render() );
		return false;
	}

}; // namespace GCSDK
