//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "usermessages.h"
#include "shake.h"
#include "voice_gamemgr.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

void RegisterUserMessages( void )
{
	usermessages->Register( "Geiger", 1 );
	usermessages->Register( "Train", 1 );
	usermessages->Register( "HudText", -1 );
	usermessages->Register( "SayText", -1 );
	usermessages->Register( "SayText2", -1 );
	usermessages->Register( "TextMsg", -1 );
	usermessages->Register( "HudMsg", -1 );
	usermessages->Register( "ResetHUD", 1);		// called every respawn
	usermessages->Register( "GameTitle", 0 );
	usermessages->Register( "ItemPickup", -1 );
	usermessages->Register( "ShowMenu", -1 );
	usermessages->Register( "Shake", 13 );
	usermessages->Register( "ShakeDir", -1 ); // directional shake
	usermessages->Register( "Tilt", 22 );
	usermessages->Register( "Fade", 10 );
	usermessages->Register( "VGUIMenu", -1 );	// Show VGUI menu
	usermessages->Register( "Rumble", 3 );	// Send a rumble to a controller
	usermessages->Register( "Battery", 2 );
	usermessages->Register( "Damage", -1 );
	usermessages->Register( "VoiceMask", VOICE_MAX_PLAYERS_DW*4 * 2 + 1 );
	usermessages->Register( "RequestState", 0 );
	usermessages->Register( "CloseCaption", -1 ); // Show a caption (by string id number)(duration in 10th of a second)
	usermessages->Register( "CloseCaptionDirect", -1 ); // Show a forced caption (by string id number)(duration in 10th of a second)
	usermessages->Register( "HintText", -1 );	// Displays hint text display
	usermessages->Register( "KeyHintText", -1 );	// Displays hint text display
	usermessages->Register( "SquadMemberDied", 0 );
	usermessages->Register( "AmmoDenied", 2 );
	usermessages->Register( "CreditsMsg", 1 );
	usermessages->Register( "LogoTimeMsg", 4 );
	usermessages->Register( "AchievementEvent", -1 );
	usermessages->Register( "UpdateJalopyRadar", -1 );
	usermessages->Register( "CurrentTimescale", 4 );	// Send one float for the new timescale
	usermessages->Register( "DesiredTimescale", 13 );	// Send timescale and some blending vars

	// asw
	usermessages->Register( "ASWMapLine", 10 ); // send 2 bytes payload
	usermessages->Register( "ASWBlur", 2 ); // send 2 bytes payload
	usermessages->Register( "ASWCampaignCompleted", 2 );
	usermessages->Register( "ASWReconnectAfterOutro", 0 );	
	usermessages->Register( "ASWUTracer", -1 );
	usermessages->Register( "ASWUTracerless", -1 );
	usermessages->Register( "ASWUTracerDual", -1 );
	usermessages->Register( "ASWUTracerDualLeft", -1 );
	usermessages->Register( "ASWUTracerDualRight", -1 );
	usermessages->Register( "ASWUTracerRG", -1 );
	usermessages->Register( "ASWEnvExplosionFX", -1 );
	usermessages->Register( "ASWGrenadeExplosion", -1 );	
	usermessages->Register( "ASWBarrelExplosion", -1 );	
	usermessages->Register( "ASWUTracerUnattached", -1 );
	usermessages->Register( "ASWSentryTracer", -1 );
	usermessages->Register( "ASWRemoteTurretTracer", -1 );
	usermessages->Register( "ASWScannerNoiseEnt", -1 );	
	usermessages->Register( "LaunchCainMail", 0 );
	usermessages->Register( "LaunchCredits", 0 );
	usermessages->Register( "LaunchCampaignMap", 0 );	
	usermessages->Register( "ASWTechFailure", 0 );
	usermessages->Register( "ASWSkillSpent", 2 );
	usermessages->Register( "ASWStopInfoMessageSound", 0 );
	usermessages->Register( "BroadcastAudio", -1 );
	usermessages->Register( "BroadcastStopAudio", -1 );
	usermessages->Register( "BroadcastPatchAudio", -1 );
	usermessages->Register( "BroadcastStopPatchAudio", -1 );
	usermessages->Register( "ASWMarineHitByMelee", -1 );
	usermessages->Register( "ASWMarineHitByFF", -1 );
	usermessages->Register( "ASWEnemyZappedByThorns", -1 );
	usermessages->Register( "ASWEnemyZappedByTesla", -1 );
	usermessages->Register( "ASWEnemyTeslaGunArcShock", -1 );
	usermessages->Register( "ASWMiningLaserZap", -1 );
	usermessages->Register( "ASWDamageNumber", -1 );
	usermessages->Register( "ASWOrderUseItemFX", -1 );
	usermessages->Register( "ASWOrderStopItemFX", -1 );
	usermessages->Register( "ASWInvalidDesination", -1 );
	usermessages->Register( "ASWNewHoldoutWave", -1 );
	usermessages->Register( "ASWShowHoldoutResupply", -1 );
	usermessages->Register( "ASWShowHoldoutWaveEnd", -1 );
	usermessages->Register( "ShowObjectives", -1 );
	usermessages->Register( "ASWBuzzerDeath", -1 );	
	usermessages->Register( "ASWEggEffects", -1 );	
}
