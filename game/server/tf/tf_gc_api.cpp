//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"

#include "tf_gc_api.h"

#include <checksum_md5.h>

#include "econ_game_account_server.h"
#include "econ_gcmessages.h"
#include "tf_gcmessages.h"
#include "tf_gamerules.h"
#include "gc_clientsystem.h"

//-----------------------------------------------------------------------------

static ConVar tf_server_identity_token( "tf_server_identity_token", "", FCVAR_ARCHIVE | FCVAR_PROTECTED, "Server identity token, used to authenticate with the TF2 Game Coordinator." );
static ConVar tf_server_identity_account_id( "tf_server_identity_account_id", "0", FCVAR_ARCHIVE, "Server identity account id, used to authenticate with the TF2 Game Coordinator." );
static ConVar tf_server_identity_disable_quickplay( "tf_server_identity_disable_quickplay", "0", FCVAR_ARCHIVE | FCVAR_NOTIFY, "Disable this server from being chosen by the quickplay matchmaking." );
static ConVar sv_registration_successful( "sv_registration_successful", "0", FCVAR_DONTRECORD | FCVAR_HIDDEN | FCVAR_NOTIFY, "Nonzero if we were able to login OK" );
static ConVar sv_registration_message( "sv_registration_message", "No account specified", FCVAR_DONTRECORD | FCVAR_HIDDEN | FCVAR_NOTIFY, "Error message of other status text" );

static bool BSendMessage( const GCSDK::CProtoBufMsgBase& msg )
{
	return GCClientSystem()->BSendMessage( msg );
}

//-----------------------------------------------------------------------------
// Public API
//-----------------------------------------------------------------------------
void GameCoordinator_NotifyGameState()
{
	if ( TFGameRules() )
	{
		GCSDK::CProtoBufMsg< CMsgGC_GameServer_LevelInfo > msg( k_EMsgGC_GameServer_LevelInfo );
		msg.Body().set_level_loaded( true );
		msg.Body().set_level_name( gpGlobals->mapname.ToCStr() );
		BSendMessage( msg );
	}
	else
	{
		GameCoordinator_NotifyLevelShutdown();
	}
}

//-----------------------------------------------------------------------------
void GameCoordinator_NotifyLevelShutdown()
{
	GCSDK::CProtoBufMsg< CMsgGC_GameServer_LevelInfo> msg( k_EMsgGC_GameServer_LevelInfo );
	msg.Body().set_level_loaded( false );
	BSendMessage( msg );
}

//-----------------------------------------------------------------------------
const char *GameCoordinator_GetRegistrationString()
{
	return sv_registration_message.GetString();
}


//-----------------------------------------------------------------------------

/**
 * Game Coordinator wants to know if we have a token
 */
class CGC_GameServer_AuthChallenge : public GCSDK::CGCClientJob
{
public:
	CGC_GameServer_AuthChallenge( GCSDK::CGCClient *pClient ) : GCSDK::CGCClientJob( pClient ) {}
	virtual bool BYieldingRunGCJob( GCSDK::IMsgNetPacket *pNetPacket )
	{
		GCSDK::CProtoBufMsg< CMsgGC_GameServer_AuthChallenge > msg( pNetPacket );

		// send information on what level is loaded
		GameCoordinator_NotifyGameState();

		const uint32 unGameServerAccountID = tf_server_identity_account_id.GetInt();
		if ( unGameServerAccountID == 0 )
		{
			Msg( "%s not set; not logging into registered account\n", tf_server_identity_account_id.GetName() );
			UTIL_LogPrintf( "%s not set; not logging into registered account\n", tf_server_identity_account_id.GetName() );
			return true;
		}

		CUtlString challenge = msg.Body().challenge_string().c_str();
		if ( challenge.IsEmpty() )
		{
			Warning( "Received CGC_GameServer_AuthChallenge with invalid challenge from GC!\n" );
			UTIL_LogPrintf( "Received CGC_GameServer_AuthChallenge with invalid challenge from GC!\n" );
			Assert( false );
			return true;
		}

		char szKeyBuffer[16];
		int nKeyLength = Q_snprintf( szKeyBuffer, sizeof( szKeyBuffer ), "%s", tf_server_identity_token.GetString() );

		if ( nKeyLength <= 0 || nKeyLength >= ARRAYSIZE( szKeyBuffer ) )
		{
			Warning( "%s is not valid, or not set!  Not signing into gameserver account\n", tf_server_identity_token.GetName() );
			UTIL_LogPrintf( "%s is not valid, or not set!  Not signing into gameserver account\n", tf_server_identity_token.GetName() );
			return true;
		}

		MD5Context_t ctx;
		unsigned char digest[16]; // The MD5 Hash
		memset( &ctx, 0, sizeof( ctx ) );
		memset( digest, 0, sizeof( digest ) );

		// hash together the identity token and the challenge
		MD5Init( &ctx );
		MD5Update( &ctx, (unsigned char*)szKeyBuffer, nKeyLength );
		MD5Update( &ctx, (unsigned char*)challenge.Get(), challenge.Length() );
		MD5Final( digest, &ctx );

		GCSDK::CProtoBufMsg< CMsgGC_GameServer_AuthChallengeResponse > msg_response( k_EMsgGC_GameServer_AuthChallengeResponse );
		msg_response.Body().set_game_server_account_id( unGameServerAccountID );
		msg_response.Body().set_hashed_challenge_string( digest, sizeof( digest ) );
		BSendMessage( msg_response );

		Msg( "Received auth challenge; signing into gameserver account...\n" );
		UTIL_LogPrintf( "Received auth challenge; signing into gameserver account...\n" );
		return true;
	}
};
GC_REG_JOB( GCSDK::CGCClient, CGC_GameServer_AuthChallenge, "CGC_GameServer_AuthChallenge", k_EMsgGC_GameServer_AuthChallenge, GCSDK::k_EServerTypeGCClient );

/**
 * Game Coordinator tells game server whether authentication was successful or not
 */
class CGC_GameServer_AuthResult : public GCSDK::CGCClientJob
{
public:
	CGC_GameServer_AuthResult( GCSDK::CGCClient *pClient ) : GCSDK::CGCClientJob( pClient ) {}
	virtual bool BYieldingRunGCJob( GCSDK::IMsgNetPacket *pNetPacket )
	{
		GCSDK::CProtoBufMsg< CMsgGC_GameServer_AuthResult > msg( pNetPacket );

		if ( msg.Body().is_valve_server() )
		{
			// Note: I put in this cryptic message, in the hopes that if a server operator
			// actually sees it (when they are not supposed to), they will contact us,
			// as opposed to just secretly enjoying the benefits of their server being
			// treated as a valve server and us not knowing something is wrong.
			Msg( "WARNING: Game server status 'Gordon'.\n" );
			engine->LogPrint( "WARNING: Game server status 'Gordon'.\n" );
		}
		if ( msg.Body().authenticated() )
		{
			const char *pStanding = GameServerAccount_GetStandingString( (eGameServerScoreStanding)msg.Body().game_server_standing() );
			const char *pStandingTrend = GameServerAccount_GetStandingTrendString( (eGameServerScoreStandingTrend)msg.Body().game_server_standing_trend() );
			Msg( "Game server authentication: SUCCESS! Standing: %s. Trend: %s\n", pStanding, pStandingTrend );
			UTIL_LogPrintf( "Game server authentication: SUCCESS! Standing: %s. Trend: %s\n", pStanding, pStandingTrend );
			if ( !msg.Body().message().empty() )
			{
				Msg( "   %s\n", msg.Body().message().c_str() );
				UTIL_LogPrintf( "   %s\n", msg.Body().message().c_str() );
			}
			// What else could we save here?
			if ( msg.Body().is_valve_server() )
			{
				sv_registration_message.SetValue( "Status 'Gordon'" );
			}
			else
			{
				sv_registration_message.SetValue( "" );
			}
		}
		else
		{
			Warning( "Game server authentication: FAILURE!\n" );
			UTIL_LogPrintf( "Game server authentication: FAILURE!\n" );
			if ( !msg.Body().message().empty() )
			{
				Warning( "   %s\n", msg.Body().message().c_str() );
				UTIL_LogPrintf( "   %s\n", msg.Body().message().c_str() );
				sv_registration_message.SetValue( msg.Body().message().c_str() );
			}
			else
			{
				sv_registration_message.SetValue( "failed" );
			}
		}

		// Update the convar
		sv_registration_successful.SetValue( msg.Body().authenticated() );

		// !FIXME! How to force server to recalculate tags??

		return true;
	}
};
GC_REG_JOB( GCSDK::CGCClient, CGC_GameServer_AuthResult, "CGC_GameServer_AuthResult", k_EMsgGC_GameServer_AuthResult, GCSDK::k_EServerTypeGCClient );

//-----------------------------------------------------------------------------

// GC telling us that a player is joining via Quickplay
class CGCTFQuickplay_PlayerJoining : public GCSDK::CGCClientJob
{
public:
	CGCTFQuickplay_PlayerJoining( GCSDK::CGCClient *pClient ) : GCSDK::CGCClientJob( pClient ) {}

	virtual bool BYieldingRunGCJob( GCSDK::IMsgNetPacket *pNetPacket )
	{
		GCSDK::CProtoBufMsg<CMsgTFQuickplay_PlayerJoining> msg( pNetPacket );
		Log("(Quickplay) Incoming player (%d):\n", msg.Body().account_id() );
		return true;
	}
};
GC_REG_JOB( GCSDK::CGCClient, CGCTFQuickplay_PlayerJoining, "CGCTFQuickplay_PlayerJoining", k_EMsgGC_QP_PlayerJoining, GCSDK::k_EServerTypeGCClient );

//-----------------------------------------------------------------------------

// GC telling us that a player has moved an item into his or her backpack for the first time
class CGCTFItemAcknowledged : public GCSDK::CGCClientJob
{
public:
	CGCTFItemAcknowledged( GCSDK::CGCClient *pClient ) : GCSDK::CGCClientJob( pClient ) {}

	virtual bool BYieldingRunGCJob( GCSDK::IMsgNetPacket *pNetPacket )
	{
		GCSDK::CProtoBufMsg<CMsgItemAcknowledged> msg( pNetPacket );

		CSteamID steamID;
		CTFPlayer *pFoundPlayer = NULL;
		uint32 unAccountID = msg.Body().account_id();
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CTFPlayer *pPlayer = ToTFPlayer( UTIL_PlayerByIndex( i ) );
			if ( pPlayer == NULL )
				continue;

			if ( pPlayer->GetSteamID( &steamID ) == false )
				continue;

			if ( steamID.GetAccountID() == unAccountID )
			{
				pFoundPlayer = pPlayer;
				break;
			}
		}

		if ( pFoundPlayer )
		{
			// Crack open some attributes
			
			IGameEvent *event = gameeventmanager->CreateEvent( "item_found" );
			if ( event )
			{
				event->SetInt( "player", pFoundPlayer->entindex() );
				event->SetInt( "quality", msg.Body().quality() );
				event->SetInt( "method", GetUnacknowledgedReason( msg.Body().inventory() ) - 1 );
				event->SetInt( "itemdef", msg.Body().def_index() );
				event->SetInt( "isstrange", msg.Body().is_strange() );
				event->SetInt( "isunusual", msg.Body().is_unusual() );
				event->SetFloat( "wear", msg.Body().wear() );

				gameeventmanager->FireEvent( event );
			}
		}

		return true;
	}
};
GC_REG_JOB( GCSDK::CGCClient, CGCTFItemAcknowledged, "CGCTFItemAcknowledged", k_EMsgGCItemAcknowledged, GCSDK::k_EServerTypeGCClient );
