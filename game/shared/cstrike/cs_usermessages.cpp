//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "usermessages.h"
#include "shake.h"
#include "voice_gamemgr.h"

// NVNT include to register in haptic user messages
#include "haptics/haptic_msgs.h"

void RegisterUserMessages()
{
	usermessages->Register( "Geiger", 1 );		// geiger info data
	usermessages->Register( "Train", 1 );		// train control data
	usermessages->Register( "HudText", -1 );
	usermessages->Register( "SayText", -1 );
	usermessages->Register( "SayText2", -1 );
	usermessages->Register( "TextMsg", -1 );
	usermessages->Register( "HudMsg", -1 );
	usermessages->Register( "ResetHUD", 1 );	// called every respawn
	usermessages->Register( "GameTitle", 0 );	// show game title
	usermessages->Register( "ItemPickup", -1 );	// for item history on screen
	usermessages->Register( "ShowMenu", -1 );	// show hud menu
	usermessages->Register( "Shake", 13 );		// shake view
	usermessages->Register( "Fade", 10 );	// fade HUD in/out
	usermessages->Register( "VGUIMenu", -1 );	// Show VGUI menu
	usermessages->Register( "Rumble", 3 );	// Send a rumble to a controller
	usermessages->Register( "CloseCaption", -1 ); // Show a caption (by string id number)(duration in 10th of a second)

	usermessages->Register( "SendAudio", -1 );	// play radio command
	usermessages->Register( "RawAudio", -1 );	// play a .wav as a radio command

	usermessages->Register( "VoiceMask", VOICE_MAX_PLAYERS_DW*4 * 2 + 1 );
	usermessages->Register( "RequestState", 0 );

	usermessages->Register( "BarTime", -1 );	// For the C4 progress bar.
	usermessages->Register( "Damage", -1 );		// for HUD damage indicators
	usermessages->Register( "RadioText", -1 );		// for radio text display
	usermessages->Register( "HintText", -1 );	// Displays hint text display
	usermessages->Register( "KeyHintText", -1 );	// Displays hint text display
	
	usermessages->Register( "ReloadEffect", 2 );			// a player reloading..
	usermessages->Register( "PlayerAnimEvent", -1 );	// jumping, firing, reload, etc.

	usermessages->Register( "AmmoDenied", 2 );
	usermessages->Register( "UpdateRadar", -1 );
	usermessages->Register( "KillCam", -1 );
	usermessages->Register( "MarkAchievement", -1 );

	// Voting
	usermessages->Register( "CallVoteFailed", -1 );
	usermessages->Register( "VoteStart", -1 );
	usermessages->Register( "VotePass", -1 );
	usermessages->Register( "VoteFailed", 2 );
	usermessages->Register( "VoteSetup", -1 );  // Initiates client-side voting UI

	// NVNT register haptic user messages
	RegisterHapticMessages();

	//=============================================================================
	// HPE_BEGIN:
	// [menglish] Registering PlayerStatsUpdate for Stats implementation
	// [dwenger] AchievementEvent: Necessary for server-side achievement awarding
    // [tj]      Added support for absolute current match statistics updates   
	//=============================================================================
	 
	usermessages->Register( "PlayerStatsUpdate_DEPRECATED", -1 ); // Protocol changed, this message replaced below
    usermessages->Register( "AchievementEvent", -1 );
	usermessages->Register( "MatchEndConditions", -1 ); //The end conditions for the match.  long frag limit, long max rounds, long rounds needed won, and long time
    usermessages->Register( "MatchStatsUpdate", -1 ); 
	usermessages->Register( "PlayerStatsUpdate", -1 ); //Processes stats update

    //=============================================================================
    // HPE_END
    //=============================================================================
}
