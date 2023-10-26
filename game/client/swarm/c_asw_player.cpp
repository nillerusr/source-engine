#include "cbase.h"
#include "c_user_message_register.h"
#include "iclientvehicle.h"
#include "input.h"
#include "c_basetempentity.h"
#include "hud_macros.h"
#include "engine/ivdebugoverlay.h"
#include "bone_setup.h"
#include "in_buttons.h"
#include "r_efx.h"
#include "cl_animevent.h"
#include "c_asw_point_camera.h"
#include "soundenvelope.h"
#include <inetchannelinfo.h>
#include "materialsystem/imesh.h"		//for materials->FindMaterial
#include "iviewrender.h"				//for view->
#include "tier0/vprof.h"
#include "prediction.h"
#include "c_env_ambient_light.h"
#include "game_timescale_shared.h"

// effects
#include "ivieweffects.h"
#include "view.h"
#include "ieffects.h"
#include "fx.h"
#include "smoke_fog_overlay.h"
#include "dlight.h"
#include "shake.h"

// VGUI
#include <vgui/vgui.h>
#include <vgui_controls/Controls.h>
#include "vgui_controls/frame.h"
#include "vgui_controls/Label.h"
#include <vgui_controls/PHandle.h>
#include <vgui_controls/Button.h>
#include "controller_focus.h"
#include "iclientmode.h"

// ASW HUD
#include "vgui\asw_vgui_info_message.h"
#include "vgui\FadeInPanel.h"
#include "vgui\asw_vgui_ingame_panel.h"
#include "asw_vgui_stylin_cam.h"
#include "asw_hud_crosshair.h"
#include "asw_hud_chat.h"
#include "PlayerListPanel.h"
#include "asw_vgui_frame.h"
#include "MedalCollectionPanel.h"
#include "CreditsPanel.h"
#include "CainMailPanel.h"
#include "asw_vgui_skip_intro.h"
#include "CampaignFrame.h"
#include "OutroFrame.h"
#include "MissionCompleteFrame.h"
#include "BriefingFrame.h"
#include "clientmode_asw.h"
#include "vgui\PlayerListContainer.h"
#include "asw_loading_panel.h"

// ASW Game
#include "c_asw_player.h"
#include "c_asw_game_resource.h"
#include "c_asw_objective.h"
#include "asw_marine_profile.h"
#include "c_asw_marine_resource.h"
#include "c_asw_marine.h"
#include "c_asw_mesh_emitter_entity.h"
#include "c_asw_generic_emitter_entity.h"
#include "asw_weapon_parse.h"
#include "c_asw_pickup.h"
#include "c_asw_door_area.h"
#include "asw_gamerules.h"
#include "asw_equipment_list.h"
#include "c_asw_weapon.h"
#include "asw_marineandobjectenumerator.h"
#include "asw_usableobjectsenumerator.h"
#include "c_asw_jeep_clientside.h"
#include "obstacle_pushaway.h"
#include "asw_shareddefs.h"
#include "asw_campaign_info.h"
#include "c_asw_voting_missions.h"
#include "c_asw_camera_volume.h"
#include "asw_medal_store.h"
#include "asw_remote_turret_shared.h"
#include "c_asw_snow_volume.h"
#include "c_asw_snow_emitter.h"
#include "asw_util_shared.h"
#include "c_asw_flare_projectile.h"
#include "c_asw_flamer_projectile.h"
#include "collisionutils.h"
#include "asw_input.h"
#include "c_asw_sentry_base.h"
#include "asw_weapon_sniper_rifle.h"
#include "asw_melee_system.h"
#include "c_asw_jukebox.h"
#include "missionchooser/iasw_mission_chooser.h"
#include "missionchooser/iasw_random_missions.h"

#if defined( CASW_Player )
	#undef CASW_Player
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar asw_controls;
extern ConVar asw_marine_collision;
ConVar g_DrawPlayer("asw_drawplayermesh", "0", FCVAR_ARCHIVE, "Draw the player entity or not");
ConVar asw_clientside_avoidance("asw_clientside_avoidance", "1", FCVAR_CHEAT);
ConVar asw_debug_clientside_avoidance("asw_debug_clientside_avoidance", "0", FCVAR_CHEAT);
ConVar asw_clientside_avoidance_scale("asw_clientside_avoidance_scale", "1.0", FCVAR_CHEAT);
// stim cams: (p/y/r) (x/y/z)
// (0/-10/0)  (-60 60 30) - foot level forward looking
// (40/180/0) (50 0 70)	- close marine view cam
ConVar asw_stim_cam_pitch("asw_stim_cam_pitch", "10", 0, "Controls angle of the small in-picture Stim camera");
ConVar asw_stim_cam_yaw("asw_stim_cam_yaw", "15", 0, "Controls angle of the small in-picture Stim camera");
ConVar asw_stim_cam_roll("asw_stim_cam_roll", "0", 0, "Controls angle of the small in-picture Stim camera");
ConVar asw_stim_cam_x("asw_stim_cam_x", "-20", 0, "Controls offset of the small in-picture Stim camera");
ConVar asw_stim_cam_y("asw_stim_cam_y", "-30", 0, "Controls offset of the small in-picture Stim camera");
ConVar asw_stim_cam_z("asw_stim_cam_z", "70", 0, "Controls offset of the small in-picture Stim camera");
ConVar asw_stim_cam_rotate_speed("asw_stim_cam_rotate_speed", "0", 0, "Rotation speed of the stim camera");
ConVar asw_spinning_stim_cam("asw_spinning_stim_cam", "1", 0, "If set, slow motion will display a small spinning camera view");
ConVar asw_show_mouse_entity("asw_show_mouse_entity", "0", FCVAR_CHEAT, "Show entity under the mouse cursor");
ConVar asw_marine_switch_blend_speed("asw_marine_switch_blend_speed", "2.5", 0, "How quickly the camera blends between marines when switching");
ConVar asw_marine_switch_blend_max_dist("asw_marine_switch_blend_max_dist", "1500", 0, "Maximum distance apart marines can be for a camera blend to occur");

// default inventory convars
ConVar asw_default_primary_0("asw_default_primary_0", "-1", FCVAR_NONE, "Default primary equip for marine with this number");
ConVar asw_default_secondary_0("asw_default_secondary_0", "-1", FCVAR_NONE, "Default secondary equip for marine with this number");
ConVar asw_default_extra_0("asw_default_extra_0", "-1", FCVAR_NONE, "Default extra equip for marine with this number");
ConVar asw_default_primary_1("asw_default_primary_1", "-1", FCVAR_NONE, "Default primary equip for marine with this number");
ConVar asw_default_secondary_1("asw_default_secondary_1", "-1", FCVAR_NONE, "Default secondary equip for marine with this number");
ConVar asw_default_extra_1("asw_default_extra_1", "-1", FCVAR_NONE, "Default extra equip for marine with this number");
ConVar asw_default_primary_2("asw_default_primary_2", "-1", FCVAR_NONE, "Default primary equip for marine with this number");
ConVar asw_default_secondary_2("asw_default_secondary_2", "-1", FCVAR_NONE, "Default secondary equip for marine with this number");
ConVar asw_default_extra_2("asw_default_extra_2", "-1", FCVAR_NONE, "Default extra equip for marine with this number");
ConVar asw_default_primary_3("asw_default_primary_3", "-1", FCVAR_NONE, "Default primary equip for marine with this number");
ConVar asw_default_secondary_3("asw_default_secondary_3", "-1", FCVAR_NONE, "Default secondary equip for marine with this number");
ConVar asw_default_extra_3("asw_default_extra_3", "-1", FCVAR_NONE, "Default extra equip for marine with this number");
ConVar asw_default_primary_4("asw_default_primary_4", "-1", FCVAR_NONE, "Default primary equip for marine with this number");
ConVar asw_default_secondary_4("asw_default_secondary_4", "-1", FCVAR_NONE, "Default secondary equip for marine with this number");
ConVar asw_default_extra_4("asw_default_extra_4", "-1", FCVAR_NONE, "Default extra equip for marine with this number");
ConVar asw_default_primary_5("asw_default_primary_5", "-1", FCVAR_NONE, "Default primary equip for marine with this number");
ConVar asw_default_secondary_5("asw_default_secondary_5", "-1", FCVAR_NONE, "Default secondary equip for marine with this number");
ConVar asw_default_extra_5("asw_default_extra_5", "-1", FCVAR_NONE, "Default extra equip for marine with this number");
ConVar asw_default_primary_6("asw_default_primary_6", "-1", FCVAR_NONE, "Default primary equip for marine with this number");
ConVar asw_default_secondary_6("asw_default_secondary_6", "-1", FCVAR_NONE, "Default secondary equip for marine with this number");
ConVar asw_default_extra_6("asw_default_extra_6", "-1", FCVAR_NONE, "Default extra equip for marine with this number");
ConVar asw_default_primary_7("asw_default_primary_7", "-1", FCVAR_NONE, "Default primary equip for marine with this number");
ConVar asw_default_secondary_7("asw_default_secondary_7", "-1", FCVAR_NONE, "Default secondary equip for marine with this number");
ConVar asw_default_extra_7("asw_default_extra_7", "-1", FCVAR_NONE, "Default extra equip for marine with this number");
ConVar asw_default_primary_8("asw_default_primary_8", "-1", FCVAR_NONE, "Default primary equip for marine with this number");
ConVar asw_default_secondary_8("asw_default_secondary_8", "-1", FCVAR_NONE, "Default secondary equip for marine with this number");
ConVar asw_default_extra_8("asw_default_extra_8", "-1", FCVAR_NONE, "Default extra equip for marine with this number");

ConVar asw_particle_count("asw_particle_count", "0", 0, "Shows how many particles are being drawn");
ConVar asw_dlight_list("asw_dlight_list", "0", 0, "Lists dynamic lights");

ConVar asw_stim_music( "asw_stim_music", "", FCVAR_ARCHIVE, "Custom music file used for stim music" );
ConVar asw_player_avoidance( "asw_player_avoidance", "1", FCVAR_CHEAT, "Enable/Disable player avoidance." );
ConVar asw_player_avoidance_force( "asw_player_avoidance_force", "1024", FCVAR_CHEAT, "Marine avoidance separation force." );
ConVar asw_player_avoidance_bounce( "asw_player_avoidance_bounce", "1.0", FCVAR_CHEAT, "Marine avoidance bounce." );
ConVar asw_player_avoidance_fakehull( "asw_player_avoidance_fakehull", "25.0", FCVAR_CHEAT, "Marine avoidance hull size." );

ConVar asw_roster_select_bypass_steam( "asw_roster_select_bypass_steam", "0", FCVAR_CHEAT, "Bypass checking if data has been downloaded from steam when selecting a Marine." );

void fnAutoReloadChangedCallback(IConVar *var, const char *pOldString, float flOldValue )
{
	if ( engine->IsInGame() )
	{
		engine->ClientCmd( VarArgs( "cl_autoreload %d\n", ((ConVar *)var)->GetInt() ) );
	}
}
ConVar asw_auto_reload("asw_auto_reload", "1", FCVAR_ARCHIVE, "Whether your marines should autoreload when reaching 0 bullets", true, 0, true, 1, fnAutoReloadChangedCallback);

ConVar asw_turret_fog_start("asw_turret_fog_start", "900", 0, "Fog start distance for turret view");
ConVar asw_turret_fog_end("asw_turret_fog_end", "1200", 0, "Fog end distance for turret view");

extern ConVar asw_allow_detach;
extern ConVar asw_stim_cam_time;
extern ConVar asw_rts_controls;
extern ConVar asw_hud_alpha;
extern ConVar asw_building_room_thumbnails;

// How fast to avoid collisions with center of other object, in units per second
#define AVOID_SPEED 2000.0f
extern ConVar cl_forwardspeed;
extern ConVar cl_backspeed;
extern ConVar cl_sidespeed;

extern ConVar asw_marine_death_cam_time;
extern ConVar asw_time_scale_delay;

extern float g_fMarinePoisonDuration;

// -------------------------------------------------------------------------------- //
// Player animation event. Sent to the client when a player fires, jumps, reloads, etc..
// -------------------------------------------------------------------------------- //

class C_TEPlayerAnimEvent : public C_BaseTempEntity
{
public:
	DECLARE_CLASS( C_TEPlayerAnimEvent, C_BaseTempEntity );
	DECLARE_CLIENTCLASS();

	virtual void PostDataUpdate( DataUpdateType_t updateType )
	{
		// Create the effect.
		C_ASW_Player *pPlayer = dynamic_cast< C_ASW_Player* >( m_hPlayer.Get() );
		if ( pPlayer && !pPlayer->IsDormant() )
		{
			pPlayer->DoAnimationEvent( (PlayerAnimEvent_t)m_iEvent.Get() );
		}	
	}

public:
	CNetworkHandle( CBasePlayer, m_hPlayer );
	CNetworkVar( int, m_iEvent );
};

IMPLEMENT_CLIENTCLASS_EVENT( C_TEPlayerAnimEvent, DT_TEPlayerAnimEvent, CTEPlayerAnimEvent );

BEGIN_RECV_TABLE_NOBASE( C_TEPlayerAnimEvent, DT_TEPlayerAnimEvent )
	RecvPropEHandle( RECVINFO( m_hPlayer ) ),
	RecvPropInt( RECVINFO( m_iEvent ) )
END_RECV_TABLE()

// marine anim events
class C_TEMarineAnimEvent : public C_BaseTempEntity
{
public:
	DECLARE_CLASS( C_TEMarineAnimEvent, C_BaseTempEntity );
	DECLARE_CLIENTCLASS();

	virtual void PostDataUpdate( DataUpdateType_t updateType )
	{
		// ignore events we've already predicted
		if (C_BasePlayer::IsLocalPlayer( m_hExcludePlayer.Get() ))	// !engine->IsPlayingDemo() && 
			return;
		// play anim event
		C_ASW_Marine *pMarine = dynamic_cast< C_ASW_Marine* >( m_hMarine.Get() );
		if ( pMarine && !pMarine->IsDormant() )
		{
			pMarine->DoAnimationEvent( (PlayerAnimEvent_t)m_iEvent.Get() );
		}	
	}

public:
	CNetworkHandle( C_BasePlayer, m_hExcludePlayer );
	CNetworkHandle( C_ASW_Marine, m_hMarine );
	CNetworkVar( int, m_iEvent );
};

IMPLEMENT_CLIENTCLASS_EVENT( C_TEMarineAnimEvent, DT_TEMarineAnimEvent, CTEMarineAnimEvent );

BEGIN_RECV_TABLE_NOBASE( C_TEMarineAnimEvent, DT_TEMarineAnimEvent )
	RecvPropEHandle( RECVINFO( m_hMarine ) ),
	RecvPropEHandle( RECVINFO( m_hExcludePlayer ) ),
	RecvPropInt( RECVINFO( m_iEvent ) )
END_RECV_TABLE()


IMPLEMENT_NETWORKCLASS_ALIASED( ASW_Player, DT_ASW_Player )

BEGIN_NETWORK_TABLE( C_ASW_Player, DT_ASW_Player )
	RecvPropBool( RECVINFO( m_fIsWalking ) ),
	RecvPropFloat( RECVINFO( m_angEyeAngles[0] ) ),
	RecvPropFloat( RECVINFO( m_angEyeAngles[1] ) ),
	RecvPropFloat( RECVINFO( m_angEyeAngles[2] ) ),
	RecvPropEHandle( RECVINFO( m_hMarine ) ),
	RecvPropEHandle( RECVINFO( m_hSpectatingMarine ) ),
	RecvPropInt		(RECVINFO(m_iHealth)),
	RecvPropEHandle( RECVINFO ( m_pCurrentInfoMessage ) ),
	RecvPropEHandle( RECVINFO ( m_hVotingMissions ) ),	
	RecvPropEHandle( RECVINFO ( m_hOrderingMarine ) ),	
	RecvPropInt(RECVINFO(m_iLeaderVoteIndex) ),
	RecvPropInt(RECVINFO(m_iKickVoteIndex) ),
	RecvPropFloat(RECVINFO(m_fMapGenerationProgress) ),
	RecvPropTime( RECVINFO( m_flUseKeyDownTime ) ),
	RecvPropEHandle( RECVINFO ( m_hUseKeyDownEnt ) ),
	RecvPropInt		(RECVINFO(m_nChangingSlot)),
	RecvPropInt		(RECVINFO(m_iMapVoted)),
	RecvPropInt		(RECVINFO(m_iNetworkedXP)),
	RecvPropInt		(RECVINFO(m_iNetworkedPromotion)),
END_RECV_TABLE()

BEGIN_PREDICTION_DATA( C_ASW_Player )
	DEFINE_PRED_FIELD( m_fIsWalking, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_iHealth, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_hHighlightEntity, FIELD_EHANDLE, FTYPEDESC_NOERRORCHECK ),
	DEFINE_PRED_FIELD( m_flUseKeyDownTime, FIELD_FLOAT, FTYPEDESC_NOERRORCHECK ),
	DEFINE_PRED_FIELD( m_hUseKeyDownEnt, FIELD_EHANDLE, FTYPEDESC_NOERRORCHECK ),
END_PREDICTION_DATA()

vgui::DHANDLE<vgui::Frame> g_hBriefingFrame;

C_ASW_Player::C_ASW_Player() : 
	m_iv_angEyeAngles( "C_ASW_Player::m_iv_angEyeAngles" )

#if !defined(NO_STEAM)
	, m_CallbackUserStatsReceived( this, &C_ASW_Player::Steam_OnUserStatsReceived )
	, m_CallbackUserStatsStored( this, &C_ASW_Player::Steam_OnUserStatsStored )
#endif
{
	m_PlayerAnimState = CreatePlayerAnimState(this, this, LEGANIM_9WAY, false);
			
	AddVar( &m_angEyeAngles, &m_iv_angEyeAngles, LATCH_SIMULATION_VAR );

	// create the profile list for clients
	//  (server creates it in the game rules constructor serverside)
	MarineProfileList();
	ASWEquipmentList();		
	m_pStimMusic = NULL;
	m_bCheckedLevel = false;
	m_vecLastMarineOrigin = vec3_origin;	
	m_pCurrentInfoMessage = NULL;
	m_fNextThinkPushAway = 0;
	m_vecLastCameraPosition = vec3_origin;	
	m_angLastCamera = QAngle(0,0,0);
	m_nLastCameraFrame = -1;
	m_hLastMarine = NULL;
	m_hLastTurningMarine = NULL;
	m_hLastAimingFloorZMarine = NULL;
	m_fLastVehicleYaw = 0;
	m_bLastInVehicle = false;	
	m_fLastFloorZ = 0;
	m_fLastRestartTime = 0;
	m_fStimYaw = 0;		
	m_Local.m_vecPunchAngle.Set( ROLL, 0 );
	m_Local.m_vecPunchAngle.Set( PITCH, 0 );
	m_Local.m_vecPunchAngle.Set( YAW, 0 );
	m_fMarineChangeSmooth = 0.0f;
	m_vecMarineChangeCameraPos = vec3_origin;
	m_bGuidingMarine = false;
	m_bPlayingSingleBreathSound = false;
	m_bStartedStimMusic = false;
	m_iExperience = 0;
	m_iExperienceBeforeDebrief = 0;
	m_iPromotion = 0;
	m_bPendingSteamStats = false;
	m_flPendingSteamStatsStart = 0.0f;
	m_hUseKeyDownEnt = NULL;
	m_flUseKeyDownTime = 0.0f;
	m_roomDetailsCheckTimer.Invalidate();
	m_szSoundscape[0] = 0;
	for ( int i = 0; i < ASW_NUM_XP_TYPES; i++ )
	{
		m_iEarnedXP[ i ] = 0;
		m_iStatNumXP[ i ] = 0;
	}

	m_nChangingSlot = 0;
}


C_ASW_Player::~C_ASW_Player()
{
	m_PlayerAnimState->Release();

	if (m_bPlayingSingleBreathSound)
	{
		StopStimSound();
	}

	if (m_pStimCam)
		delete m_pStimCam;
}

void C_ASW_Player::Precache()
{
	BaseClass::Precache();	
}

void C_ASW_Player::StopStimSound()
{
	StopSound("noslow.SingleBreath");
	m_bPlayingSingleBreathSound = false;
	if (ASWGameRules()->GetGameState() == ASW_GS_INGAME)
		EmitSound("noslow.BulletTimeOut");
}

C_ASW_Player* C_ASW_Player::GetLocalASWPlayer( int nSlot )
{
	return ToASW_Player( C_BasePlayer::GetLocalPlayer( nSlot ) );
}


const QAngle& C_ASW_Player::GetRenderAngles()
{
	if ( IsRagdoll() )
	{
		return vec3_angle;
	}
	else
	{
		return m_PlayerAnimState->GetRenderAngles();
	}
}


void C_ASW_Player::UpdateClientSideAnimation()
{
	// We do this in a different order than the base class.
	// We need our cycle to be valid for when we call the playeranimstate update code, 
	// or else it'll synchronize the upper body anims with the wrong cycle.
//	if ( GetSequence() != -1 )
///	{
//		// move frame forward
//		FrameAdvance( gpGlobals->frametime );
//	}

	// Update the animation data. It does the local check here so this works when using
	// a third-person camera (and we don't have valid player angles).
	if (IsLocalPlayer(this))
	{

		ACTIVE_SPLITSCREEN_PLAYER_GUARD_ENT( this );
		//m_PlayerAnimState->Update( EyeAngles()[YAW], m_angEyeAngles[PITCH] );

		g_IngamePanelManager.UpdateMouseOvers();
	}
	//else
		//m_PlayerAnimState->Update( m_angEyeAngles[YAW], m_angEyeAngles[PITCH] );	

	if ( GetSequence() != -1 )
	{
		// latch old values
		OnLatchInterpolatedVariables( LATCH_ANIMATION_VAR );
	}
}


void C_ASW_Player::PostDataUpdate( DataUpdateType_t updateType )
{
	// C_BaseEntity assumes we're networking the entity's angles, so pretend that it
	// networked the same value we already have.
	SetNetworkAngles( GetLocalAngles() );
	
	BaseClass::PostDataUpdate( updateType );
}


void C_ASW_Player::DoAnimationEvent( PlayerAnimEvent_t event )
{
//	m_PlayerAnimState->DoAnimationEvent( event );
}

CBaseCombatWeapon* C_ASW_Player::ASWAnim_GetActiveWeapon()
{
	return GetActiveWeapon();
}


bool C_ASW_Player::ASWAnim_CanMove()
{
	return true;
}

// stops the player's current marine from using a computer console or other duration use object in the world
void C_ASW_Player::StopUsing()
{
	char buffer[64];
	Q_snprintf(buffer, sizeof(buffer), "cl_stopusing");
	engine->ClientCmd(buffer);
}

void C_ASW_Player::SelectHackOption(int iHackOption)
{
	char buffer[64];
	Q_snprintf(buffer, sizeof(buffer), "cl_selecthack %d", iHackOption);
	engine->ClientCmd(buffer);
}

void C_ASW_Player::SelectTumbler(int iTumblerImpulse)
{
	char buffer[64];
	Q_snprintf(buffer, sizeof(buffer), "impulse %d", iTumblerImpulse);
	Msg("Sending %s\n", buffer);
	engine->ClientCmd(buffer);
}

void C_ASW_Player::SendRosterSelectCommand( const char *command, int index, int nPreferredSlot )
{
	//if ( m_bPendingSteamStats && !asw_roster_select_bypass_steam.GetBool() && ( gpGlobals->curtime - m_flPendingSteamStatsStart ) < 2.0f )
		//return;

	char buffer[64];
	if ( index >= 0 && index < ASW_NUM_MARINE_PROFILES )
	{
		// grab default inventory numbers
		char primarybuf[24];
		char secondarybuf[26];
		char extrabuf[24];
		Q_snprintf(primarybuf, sizeof(primarybuf), "asw_default_primary_%d", index);
		Q_snprintf(secondarybuf, sizeof(secondarybuf), "asw_default_secondary_%d", index);
		Q_snprintf(extrabuf, sizeof(extrabuf), "asw_default_extra_%d", index);		
		int default_primary = -1;
		int default_secondary = -1;
		int default_extra = -1;
		ConVar *pCVar = cvar->FindVar(primarybuf);
		if (pCVar)
			default_primary = pCVar->GetInt();
		pCVar = cvar->FindVar(secondarybuf);
		if (pCVar)
			default_secondary = pCVar->GetInt();
		pCVar = cvar->FindVar(extrabuf);
		if (pCVar)
			default_extra = pCVar->GetInt();

		CASW_EquipItem *pPrimary = ASWEquipmentList()->GetRegular( default_primary );
		if ( pPrimary )
		{
			if ( !IsWeaponUnlocked( STRING( pPrimary->m_EquipClass ) ) )
			{
				default_primary = 0;
			}
		}
		CASW_EquipItem *pSecondary = ASWEquipmentList()->GetRegular( default_secondary );
		if ( pSecondary )
		{
			if ( !IsWeaponUnlocked( STRING( pSecondary->m_EquipClass ) ) )
			{
				default_secondary = 0;
			}
		}
		CASW_EquipItem *pExtra = ASWEquipmentList()->GetExtra( default_extra );
		if ( pExtra )
		{
			if ( !IsWeaponUnlocked( STRING( pExtra->m_EquipClass ) ) )
			{
				default_extra = ASWEquipmentList()->GetExtraIndex( "asw_weapon_medkit" );
			}
		}
		Q_snprintf(buffer, sizeof(buffer), "%s %d %d %d %d %d", command, index, nPreferredSlot, default_primary, default_secondary, default_extra );

		engine->ServerCmd(buffer);
	}
	else
	{	
		Q_snprintf(buffer, sizeof(buffer), "%s %d %d", command, index, nPreferredSlot);
		engine->ServerCmd(buffer);
	}
}

void C_ASW_Player::RosterSelectMarine( int index )
{
	SendRosterSelectCommand( "cl_selectm", index );
}

void C_ASW_Player::RosterSelectSingleMarine( int index )
{
	SendRosterSelectCommand( "cl_selectsinglem", index );
}

void C_ASW_Player::RosterSelectMarineForSlot( int index, int nPreferredSlot )
{
	SendRosterSelectCommand( "cl_selectm", index, nPreferredSlot );
}

void C_ASW_Player::RosterDeselectMarine(int iProfileIndex)
{
	char buffer[64];
	Q_snprintf(buffer, sizeof(buffer), "cl_dselectm %d", iProfileIndex);
	engine->ClientCmd(buffer);
}

void C_ASW_Player::RosterSpendSkillPoint( int iProfileIndex, int nSkillSlot )
{
	if (!ASWGameResource())
		return;
	if (iProfileIndex < 0 || iProfileIndex >= ASW_NUM_MARINE_PROFILES )
	{
		//Msg("bad profile index\n");
		return;
	}

	if (nSkillSlot < 0 || nSkillSlot >= ASW_SKILL_SLOT_SPARE )
	{
		//Msg("bad skill index\n");
		return;
	}

	CLocalPlayerFilter filter;
	C_BaseEntity::EmitSound( filter, -1 /*SOUND_FROM_LOCAL_PLAYER*/, "ASWInterface.SkillUpgrade2" );

	char buffer[64];
	Q_snprintf(buffer, sizeof(buffer), "cl_spendskill %d %d", iProfileIndex, nSkillSlot);
	//Msg("Sending command %s\n", buffer);
	engine->ClientCmd(buffer);
}

void C_ASW_Player::LoadoutSelectEquip(int iMarineIndex, int iInvSlot, int iEquipIndex)
{
	CASW_EquipItem *pWeapon = ASWEquipmentList()->GetItemForSlot( iInvSlot, iEquipIndex );
	if ( pWeapon )
	{
		if ( !IsWeaponUnlocked( STRING( pWeapon->m_EquipClass ) ) )
		{
			if ( iInvSlot == ASW_INVENTORY_SLOT_EXTRA )
			{
				iEquipIndex = ASWEquipmentList()->GetExtraIndex( "asw_weapon_medkit" );
			}
			else
			{
				iEquipIndex = 0;
			}
		}
	}

	int iProfileIndex = -1;
	if ( ASWGameResource() )
	{
		C_ASW_Marine_Resource *pMR = ASWGameResource()->GetMarineResource(iMarineIndex);
		if (pMR)
		{
			// save this loadout selection, to be autoselected next time we pick this marine
			iProfileIndex = pMR->GetProfileIndex();
			if (iProfileIndex >=0 &&iProfileIndex<ASW_NUM_MARINE_PROFILES && iInvSlot>=0 && iInvSlot <=2)
			{
				char buffer[32];
				if (iInvSlot == 0)
					Q_snprintf(buffer, sizeof(buffer), "asw_default_primary_%d", iProfileIndex);
				else if (iInvSlot == 1)
					Q_snprintf(buffer, sizeof(buffer), "asw_default_secondary_%d", iProfileIndex);
				else if (iInvSlot == 2)
					Q_snprintf(buffer, sizeof(buffer), "asw_default_extra_%d", iProfileIndex);
				ConVar *pCVar = cvar->FindVar(buffer);
				if (pCVar)
				{
					pCVar->SetValue(iEquipIndex);
				}
			}
		}
	}
	if (iProfileIndex != -1)
	{
		char buffer[64];
		Q_snprintf(buffer, sizeof(buffer), "cl_loadout %d %d %d", iProfileIndex, iInvSlot, iEquipIndex);
		engine->ClientCmd(buffer);
	}
}

void C_ASW_Player::LoadoutSendStored(C_ASW_Marine_Resource *pMR)
{	
	if (!pMR)
		return;

	C_ASW_Game_Resource *pGameResource = ASWGameResource();
	if (!pGameResource)
		return;

	// find our index in the list of marine infos
	int iMarineResourceIndex= -1;
	for (int i=0;i<pGameResource->GetMaxMarineResources();i++)
	{
		C_ASW_Marine_Resource *pOtherResource = pGameResource->GetMarineResource(i);
		if (pOtherResource == pMR)
		{
			iMarineResourceIndex = i;
			break;
		}
	}

	if (iMarineResourceIndex == -1)
		return;

	int iRosterIndex = pMR->m_MarineProfileIndex;

	int iPrimary = -1;
	int iSecondary = -1;
	int iExtra = -1;
	ConVar *pCVar = NULL;
	char buffer[32];
	Q_snprintf(buffer, sizeof(buffer), "asw_default_primary_%d", iRosterIndex);
	pCVar = cvar->FindVar(buffer);
	if (pCVar)
		iPrimary = pCVar->GetInt();

	Q_snprintf(buffer, sizeof(buffer), "asw_default_secondary_%d", iRosterIndex);
	pCVar = cvar->FindVar(buffer);
	if (pCVar)
		iSecondary = pCVar->GetInt();

	Q_snprintf(buffer, sizeof(buffer), "asw_default_extra_%d", iRosterIndex);
	pCVar = cvar->FindVar(buffer);
	if (pCVar)
		iExtra = pCVar->GetInt();

	CASW_EquipItem *pPrimary = ASWEquipmentList()->GetRegular( iPrimary );
	if ( pPrimary )
	{
		if ( !IsWeaponUnlocked( STRING( pPrimary->m_EquipClass ) ) )
		{
			iPrimary = 0;
		}
	}
	CASW_EquipItem *pSecondary = ASWEquipmentList()->GetRegular( iSecondary );
	if ( pSecondary )
	{
		if ( !IsWeaponUnlocked( STRING( pSecondary->m_EquipClass ) ) )
		{
			iSecondary = 0;
		}
	}
	CASW_EquipItem *pExtra = ASWEquipmentList()->GetExtra( iExtra );
	if ( pExtra )
	{
		if ( !IsWeaponUnlocked( STRING( pExtra->m_EquipClass ) ) )
		{
			iExtra = ASWEquipmentList()->GetExtraIndex( "asw_weapon_medkit" );
		}
	}

	char mbuffer[64];
	Q_snprintf(mbuffer, sizeof(mbuffer), "cl_loadouta %d %d %d %d", iRosterIndex, iPrimary, iSecondary, iExtra);
	engine->ClientCmd(mbuffer);
}

void C_ASW_Player::StartReady()
{
	// todo: if we're not the leader, do a cl_ready
	if (ASWGameResource() && ASWGameResource()->GetLeader() == this)
		engine->ClientCmd("cl_start");	
	else
		engine->ClientCmd("cl_ready");	
}

void C_ASW_Player::CampaignSaveAndShow()
{
	engine->ClientCmd("cl_campaignsas");	
}

void C_ASW_Player::NextCampaignMission(int iTargetMission)
{
	char buffer[64];
	Q_snprintf(buffer, sizeof(buffer), "cl_campaignnext %d", iTargetMission);
	engine->ClientCmd(buffer);	
}

void C_ASW_Player::CampaignLaunchMission(int iTargetMission)
{
	char buffer[64];
	Q_snprintf(buffer, sizeof(buffer), "cl_campaignlaunch %d", iTargetMission);
	engine->ClientCmd(buffer);	
}

bool C_ASW_Player::ShouldDraw()			// we don't draw the player at all (only the npc's that he's remote controlling)
{
	if (m_hMarine.Get()!=NULL)
		return false;

	return (g_DrawPlayer.GetBool());
}

// inventory
void C_ASW_Player::ActivateInventoryItem(int slot)
{
	C_ASW_Marine* pMarine = GetMarine();
	if (!pMarine || pMarine->GetHealth()<=0)
		return;

	if (pMarine->GetFlags() & FL_FROZEN)	// don't allow this if the marine is frozen
		return;

	// check we have an item in that slot
	C_ASW_Weapon* pWeapon = pMarine->GetASWWeapon(slot);
	if (!pWeapon)
		return;

	// if it's an offhand activate, tell the server we want to activate it
	if (pWeapon->GetWeaponInfo() && pWeapon->GetWeaponInfo()->m_bOffhandActivate)
	{
		char buffer[64];
		Q_snprintf(buffer, sizeof(buffer), "cl_offhand %d", slot);
		engine->ClientCmd(buffer);

		// and predict it?
		if ( prediction->InPrediction() && pWeapon->IsPredicted() )
			pWeapon->OffhandActivate();
		return;
	}

	// otherwise, make it our selected weapon
	if (pWeapon != pMarine->GetActiveWeapon())
		::input->MakeWeaponSelection( pWeapon );
}



void C_ASW_Player::LaunchMissionCompleteFrame(bool bSuccess)
{
	// create the basic frame which holds our briefing panels
	if (!GetClientModeASW())
		return;

	GetClientModeASW()->m_hMissionCompleteFrame = new MissionCompleteFrame( bSuccess, GetClientMode()->GetViewport(), "m_MissionCompleteFrame" );	
}

void C_ASW_Player::LaunchBriefingFrame(void)
{
	using namespace vgui;

	if (engine->IsLevelMainMenuBackground())		// don't show briefing on main menu
	{
		::input->CAM_ToFirstPerson();
		return;
	}
	else if (ASWGameRules())
	{				
		if (ASWGameRules()->IsIntroMap())			
		{		
			::input->CAM_ToFirstPerson();
			new CASW_VGUI_Skip_Intro(GetClientMode()->GetViewport(), "SkipIntro");
			return;
		}
		else if (ASWGameRules()->IsOutroMap())
		{			
			::input->CAM_ToFirstPerson();
			new CASW_VGUI_Skip_Intro(GetClientMode()->GetViewport(), "SkipIntro");
			return;
		}
	}

	// create the basic frame which holds our briefing panels
	//Msg("[%d] Assigning briefing frame\n", entindex());
	g_hBriefingFrame = new BriefingFrame( GetClientMode()->GetViewport(), "g_BriefingFrame" );

	if (!g_hBriefingFrame.Get())
	{
		Msg("Error: Briefing frame was closed immediately on opening - game isn't in briefing state?\n");
	}
}

/*
void C_ASW_Player::StopAllMusic()
{
	CUtlVector< SndInfo_t > sndlist;
	enginesound->GetActiveSounds(sndlist);
	for (int i=0;i<sndlist.Count();i++)
	{
		SndInfo_t& sound = sndlist[i];

	}
}
*/

void C_ASW_Player::LaunchOutroFrame(void)
{
	if (!GetClientModeASW())
	{
		Msg("Error, couldn't launch outro frame because clientmodeASW\n");
		return;
	}
	if (GetClientModeASW()->m_hOutroFrame.Get())
	{
		GetClientModeASW()->m_hOutroFrame->Close();
		GetClientModeASW()->m_hOutroFrame = NULL;
	}

	// create the basic frame which holds our briefing panels
	Msg("[%d] Assigning outro frame\n", entindex());
	GetClientModeASW()->m_hOutroFrame = new OutroFrame( GetClientMode()->GetViewport(), "m_OutroFrame" );	
}

void C_ASW_Player::LaunchCampaignFrame(void)
{
	if (GetClientModeASW()->m_hCampaignFrame.Get())
	{
		GetClientModeASW()->m_hCampaignFrame->Close();
		GetClientModeASW()->m_hCampaignFrame = NULL;
	}

	FadeInPanel *pFadeIn = dynamic_cast<FadeInPanel*>(GetClientMode()->GetViewport()->FindChildByName("FadeIn", true));
	char mapName[255];
	Q_FileBase( engine->GetLevelName(), mapName, sizeof(mapName) );
	if (!pFadeIn && UTIL_ASW_MissionHasBriefing(mapName))  // don't create one of these if there's already one around, or in the intro
	{
		Msg("%f: creating fadein panel\n", gpGlobals->curtime);
		new FadeInPanel(GetClientMode()->GetViewport(), "FadeIn");
	}
	else if (pFadeIn)
	{
		pFadeIn->MoveToFront();
	}

	// create the basic frame which holds our briefing panels
	Msg("[%d] Assigning campaign frame\n", entindex());
	GetClientModeASW()->m_hCampaignFrame = new CampaignFrame( GetClientMode()->GetViewport(), "m_CampaignFrame" );	
}

void C_ASW_Player::CloseBriefingFrame()
{
	if (g_hBriefingFrame.Get())
	{	
		Msg("C_ASW_Player::CloseBriefingFrame stopping music\n");
		if (GetClientModeASW())
			GetClientModeASW()->StopBriefingMusic();
		g_hBriefingFrame->SetDeleteSelfOnClose(true);
		g_hBriefingFrame->Close();
		g_hBriefingFrame = NULL;

		FadeInPanel *pFadeIn = dynamic_cast<FadeInPanel*>(GetClientMode()->GetViewport()->FindChildByName("FadeIn", true));
		if (pFadeIn)
		{
			pFadeIn->AllowFastRemove();
		}

		// clear the currently visible part of the chat
		CHudChat *pChat = GET_HUDELEMENT( CHudChat );
		if (pChat && pChat->GetChatHistory())
			pChat->GetChatHistory()->ResetAllFades( false, false, 0 );
	}
}



C_ASW_Marine* C_ASW_Player::GetMarine()
{
	return m_hMarine.Get();
}

C_ASW_Marine* C_ASW_Player::GetMarine() const
{
	return m_hMarine.Get();
}

C_ASW_Marine* C_ASW_Player::GetSpectatingMarine()
{
	return m_hSpectatingMarine.Get();
}


Vector C_ASW_Player::GetAutoaimVectorForMarine( C_ASW_Marine* marine, float flDelta, float flNearMissDelta )
{
	// Never autoaim a predicted weapon (for now)
	Vector	forward;
	AngleVectors( EyeAngles(), &forward );	//  + m_Local.m_vecPunchAngle
	return	forward;
}

void C_ASW_Player::PreThink( void )
{
	BaseClass::PreThink();

	HandleSpeedChanges();
}

void C_ASW_Player::ClientThink()
{
	VPROF_BUDGET( "C_ASW_Player::ClientThink", VPROF_BUDGETGROUP_ASW_CLIENT );
	BaseClass::ClientThink();

	if (!IsLocalPlayer( this ))
		return;

	ACTIVE_SPLITSCREEN_PLAYER_GUARD_ENT( this );

	// check for stims	
	if ( ASWGameRules() )
	{
		bool bDeathCam = ( gpGlobals->curtime >= ASWGameRules()->m_fMarineDeathTime && gpGlobals->curtime <= ASWGameRules()->m_fMarineDeathTime + asw_time_scale_delay.GetFloat() + asw_marine_death_cam_time.GetFloat() + 1.5f );

		// This isn't a death cam slowdown
		float f = GameTimescale()->GetCurrentTimescale();

		static float lastTimescale = -1;
		if ( f != lastTimescale )
		{
			if ( !m_bStartedStimMusic && f < lastTimescale && ASWGameRules()->GetGameState() == ASW_GS_INGAME )
			{
				if ( !m_bPlayingSingleBreathSound )
				{
					EmitSound("noslow.BulletTimeIn");
					EmitSound("noslow.SingleBreath");
				}

				if ( !bDeathCam )
				{
					StartStimMusic();
				}

				m_bPlayingSingleBreathSound = true;
			}
			else
			{
				if ( lastTimescale <= 0.35f && f > lastTimescale )
				{
					StopStimSound();
					StopStimMusic();
				}
				if ( lastTimescale < 1.0f && f >= 1.0f )
				{
					// if the stim has completely faded out, then clear the pointer to the music so the music is restarted from the beginning next time
					ClearStimMusic();
				}
				
			}			
			
			lastTimescale = f;
			engine->SetPitchScale(f);
			if ( f > asw_stim_cam_time.GetFloat() )
			{
				if (GetStimCam())
					GetStimCam()->SetActive( false );
			}
		}
		if (f < 1.0f && !ASWGameRules()->ShouldPlayStimMusic())	// check for stopping stim music if level music kicks in while we're rocking out to stims
		{
			StopStimMusic(true);
		}
		// we're in slomo, so make sure our stimcam is on
		if (f < asw_stim_cam_time.GetFloat())
		{			
			// enable the stylin' cam
			if (GetMarine())
			{
				// check if there's a mapper designed camera turned on
				C_PointCamera *pCameraEnt = GetPointCameraList();
				bool bMapperCam = false;
				for ( int cameraNum = 0; pCameraEnt != NULL; pCameraEnt = pCameraEnt->m_pNext )
				{
					if ( pCameraEnt != GetStimCam() && pCameraEnt->IsActive() && !pCameraEnt->IsDormant())
					{							
						bMapperCam = true;
						break;
					}

					++cameraNum;
				}

				if (!bMapperCam)
				{
					if (asw_spinning_stim_cam.GetBool())
					{
						if (!GetStimCam())
							CreateStimCamera();	
						if (GetStimCam())
						{																				
							// no mapper cam, turn on our stim one
							GetStimCam()->SetActive( true );
							Vector offset = Vector(asw_stim_cam_x.GetFloat(), asw_stim_cam_y.GetFloat(), asw_stim_cam_z.GetFloat());
							Vector offset_r;
							VectorRotate(offset, QAngle(0, GetMarine()->GetAbsAngles()[YAW] + m_fStimYaw, 0), offset_r);
							if (GetMarine())
								GetStimCam()->SetAbsOrigin(GetMarine()->GetAbsOrigin() + offset_r);
							// rotate it around us
							GetStimCam()->SetAbsAngles(QAngle(asw_stim_cam_pitch.GetFloat(), m_fStimYaw + GetMarine()->GetAbsAngles()[YAW]+asw_stim_cam_yaw.GetFloat(), asw_stim_cam_roll.GetFloat()));
							m_fStimYaw += gpGlobals->frametime * asw_stim_cam_rotate_speed.GetFloat();
							//Msg("Showing marine's cam\n");
						}
					}
				}
				else
				{
					// make sure the stim cam is off, so the mapper placed cam can work
					if (GetStimCam())
						GetStimCam()->SetActive( false );
					//Msg("Showing mapper's cam\n");
				}				
			}		
		}

		// check for launching the briefing
		static int last_state=1;
		if (ASWGameRules()->GetGameState() != last_state)
			Msg("[%d] Game rules state changed to %d\n", entindex(), ASWGameRules()->GetGameState());
		last_state = ASWGameRules()->GetGameState();
		if (ASWGameRules()->GetGameState() == ASW_GS_BRIEFING && !g_hBriefingFrame.Get() && !GetClientModeASW()->m_bLaunchedBriefing)
		{
			GetClientModeASW()->m_bLaunchedBriefing = true;
			//Msg("[%d] Launching briefing frame\n", entindex());
			LaunchBriefingFrame();
		}
		else if (ASWGameRules()->GetGameState() == ASW_GS_INGAME)
		{
			if (!m_bCheckedLevel)
			{
				m_bCheckedLevel = true;	
			}
		}
		else if (ASWGameRules()->GetGameState() == ASW_GS_DEBRIEF)
		{
			if (m_bPlayingSingleBreathSound)
				StopStimSound();
			if (!GetClientModeASW()->m_hMissionCompleteFrame.Get() && !GetClientModeASW()->m_bLaunchedDebrief)
			{
				if (ASWGameRules()->GetMissionSuccess())
				{
					GetClientModeASW()->m_bLaunchedDebrief = true;
					LaunchMissionCompleteFrame(true);
				}
				else if (ASWGameRules()->GetMissionFailed())
				{
					GetClientModeASW()->m_bLaunchedDebrief = true;
					LaunchMissionCompleteFrame(false);
				}
			}
		}
		else if (ASWGameRules()->GetGameState() == ASW_GS_CAMPAIGNMAP)
		{
			if (m_bPlayingSingleBreathSound)
				StopStimSound();
			if (!GetClientModeASW()->m_bLaunchedCampaignMap)
			{
				GetClientModeASW()->m_bLaunchedCampaignMap = true;
				LaunchCampaignFrame();
			}
		}
	}
	else
	{
		Msg("can't check briefing stuff, no game rules present\n");
	}

	if (m_pCurrentInfoMessage.Get() != GetClientModeASW()->m_hLastInfoMessage.Get())
	{
		GetClientModeASW()->m_hLastInfoMessage = m_pCurrentInfoMessage;
		// todo: launch a window with this message
		if (GetClientModeASW()->m_hLastInfoMessage.Get())
		{
			Msg("Launching window cos the info message changed!\n");			
			// add it to the message log if we haven't seen it before
			bool bOldMessage = false;
			for (int i=0;i<GetClientModeASW()->m_InfoMessageLog.Count();i++)
			{
				if (GetClientModeASW()->m_InfoMessageLog[i].Get() == m_pCurrentInfoMessage.Get())
				{
					bOldMessage = true;
					break;
				}
			}
			if (!bOldMessage)
			{
				ClientModeASW::InfoMessageHandle_t handle = m_pCurrentInfoMessage.Get();
				GetClientModeASW()->m_InfoMessageLog.AddToTail(handle);
			}
			CASW_VGUI_Info_Message *pInfoWindow = new CASW_VGUI_Info_Message( GetClientMode()->GetViewport(),
				"InfoMessageWindow",  m_pCurrentInfoMessage.Get());
			vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFile("resource/SwarmSchemeNew.res", "SwarmSchemeNew");
			pInfoWindow->SetScheme(scheme);				
			pInfoWindow->InvalidateLayout(true);
			//pInfoWindow->ASWInit();
		}
		else
			Msg("info message cleared!\n");
	}

	if ( gpGlobals->curtime >= m_fNextThinkPushAway )
	{
		if (GetMarine() && !GetMarine()->IsInVehicle())
		{
			PerformObstaclePushaway( GetMarine() );
		}
		m_fNextThinkPushAway =  gpGlobals->curtime + PUSHAWAY_THINK_INTERVAL;
	}

	int nprintIndex = 0;
	if ( asw_show_mouse_entity.GetBool() )
	{		
		engine->Con_NPrintf( nprintIndex, "Mouse over entity:");
		nprintIndex++;

		if ( ASWInput() && ASWInput()->GetMouseOverEntity() )
		{
			engine->Con_NPrintf( nprintIndex, "(%d) %s",
				ASWInput()->GetMouseOverEntity()->entindex(),
				ASWInput()->GetMouseOverEntity()->GetClassname()
				);
			nprintIndex++;
		}
		else
		{
			engine->Con_NPrintf( nprintIndex, "None");
			nprintIndex++;
		}
	}

	if (asw_particle_count.GetBool())
	{
		if (ParticleMgr())
		{
			engine->Con_NPrintf( nprintIndex, "Num Particles: %d / 2048",
				ParticleMgr()->GetNumParticles()
				);
			nprintIndex++;			
		}
		else
		{
			engine->Con_NPrintf( nprintIndex, "Num Particles: ??? / 2048" );
			nprintIndex++;
		}
	}

	if (asw_dlight_list.GetBool())
	{
		dlight_t *lights[MAX_DLIGHTS];
		int nLights = effects->CL_GetActiveDLights( lights );
		for (int i=0;i<MAX_DLIGHTS;i++)
		{
			if (i >= nLights)
			{
				engine->Con_NPrintf( nprintIndex, "Light: %d - off", i );
				nprintIndex++;
			}
			else
			{
				const char *szLightType = "Generic";
				if (lights[i]->key == 0)
					szLightType = "Explosion/Sparks";
				else if (lights[i]->key < 0)
					szLightType = "Spotlight end";
				else if (lights[i]->key == LIGHT_INDEX_TE_DYNAMIC)
					szLightType = "Legacy TE";
				else if (lights[i]->key >= ASW_LIGHT_INDEX_FIRES)
					szLightType = "env_fire";
				else if (lights[i]->key >= LIGHT_INDEX_MUZZLEFLASH)
					szLightType = "muzzleflash";

				if (lights[i]->key >= 0 && lights[i]->key < MAX_EDICTS)
				{
					C_BaseEntity *pEnt = C_BaseEntity::Instance(lights[i]->key);
					C_ASW_Flare_Projectile *pFlare = dynamic_cast<C_ASW_Flare_Projectile*>(pEnt);
					if (pFlare)
						szLightType = "Flare";
					C_ASW_Flamer_Projectile *pFlamer = dynamic_cast<C_ASW_Flamer_Projectile*>(pEnt);
					if (pFlamer)
						szLightType = "Flamer";
				}
				engine->Con_NPrintf( nprintIndex, "Light: %d - %s (key:%d)", i, szLightType, lights[i]->key);
				nprintIndex++;
			}
		}
	}
	
	if (m_hVotingMissions.Get())
		m_hVotingMissions->Update();

	// update snow
	C_ASW_Snow_Volume::UpdateSnow(this);

	UpdateLocalMarineGlow();

	if ( missionchooser->RandomMissions() && missionchooser->RandomMissions()->ValidMapLayout() )
	{
		UpdateRoomDetails();
	}
}

void ForceSoundscape( const char *pSoundscapeName, float radius );

void C_ASW_Player::UpdateRoomDetails()
{
	VPROF("C_ASW_Player::UpdateRoomDetails");
	if ( m_roomDetailsCheckTimer.HasStarted() && !m_roomDetailsCheckTimer.IsElapsed() )
		return;

	m_roomDetailsCheckTimer.Start( 1.0f );		// check soundscape every second

	if ( !missionchooser || !missionchooser->RandomMissions() || !ASWGameRules() || ASWGameRules()->GetGameState() != ASW_GS_INGAME )
	{
		return;
	}

	C_ASW_Marine *pMarine = GetMarine();
	if ( !pMarine )
	{
		pMarine = GetSpectatingMarine();
	}

	Vector vecAmbientPos = vec3_origin;
	if ( pMarine )
	{
		vecAmbientPos = pMarine->GetAbsOrigin();
	}
	else
	{
		vecAmbientPos = GetAbsOrigin();
	}

	IASW_Random_Missions *pRandomMissions = missionchooser->RandomMissions();
	IASW_Room_Details* pRoom = pRandomMissions->GetRoomDetails( vecAmbientPos );

	if ( pRoom )
	{
		// update soundscape
		static char szSoundscape[64];
		pRoom->GetSoundscape( szSoundscape, 64 );
		if ( szSoundscape[0] && Q_stricmp( szSoundscape, m_szSoundscape ) )
		{
			Q_strcpy( m_szSoundscape, szSoundscape );
			ForceSoundscape( m_szSoundscape, 36.0f );
		}

		// update ambient light
		if ( !m_hAmbientLight.Get() )
		{
			C_EnvAmbientLight *pAmbient = new C_EnvAmbientLight;
			if ( pAmbient )
			{
				if ( pAmbient->InitializeAsClientEntity( NULL, false ) )
				{
					pAmbient->InitSpatialEntity();
					pAmbient->SetFalloff( -1, -1 );			// make it global
					pAmbient->SetColor( Vector( 1, 1, 1 ) );
					pAmbient->SetCurWeight( 1.0f );
					pAmbient->SetEnabled( true );
					m_hAmbientLight = pAmbient;
				}
			}
		}
		if ( m_hAmbientLight.Get() )
		{
			Vector vecTargetColor = m_hAmbientLight->GetTargetColor();
			Vector vecNewColor = pRoom->GetAmbientLight();
			if ( vecTargetColor != vecNewColor )
			{
				// change to the new ambient light setting
				//Msg( "player set local ambient light to %f %f %f\n", VectorExpand( vecNewColor ) );
				m_hAmbientLight->SetColor( vecNewColor, 2.0f );
			}
		}
	}
}

void C_ASW_Player::ShowMessageLog()
{
	Msg("Launching window to show message log\n");
	CASW_VGUI_Message_Log *pInfoWindow = new CASW_VGUI_Message_Log( GetClientMode()->GetViewport(),
		"InfoMessageLog");
			
	vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFile("resource/SwarmSchemeNew.res", "SwarmSchemeNew");
	pInfoWindow->SetScheme(scheme);
	pInfoWindow->InvalidateLayout(true);
}

void C_ASW_Player::ShowPreviousInfoMessage(C_ASW_Info_Message *pMessage)
{
	if (!pMessage)
		return;

	bool bOldMessage = false;
	for (int i=0;i<GetClientModeASW()->m_InfoMessageLog.Count();i++)
	{
		if (GetClientModeASW()->m_InfoMessageLog[i].Get() == pMessage)
		{
			bOldMessage = true;
			break;
		}
	}

	if (!bOldMessage)
		return;

	Msg("Launching window to show old info message\n");
	CASW_VGUI_Info_Message *pInfoWindow = new CASW_VGUI_Info_Message( GetClientMode()->GetViewport(),
		"InfoMessageWindow",  pMessage);
			
	vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFile("resource/SwarmSchemeNew.res", "SwarmSchemeNew");
	pInfoWindow->SetScheme(scheme);				
	pInfoWindow->InvalidateLayout(true);			
}

void C_ASW_Player::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	if ( updateType == DATA_UPDATE_CREATED )
	{
		if ( GASWLoadingPanel() )
		{
			GASWLoadingPanel()->SetLoadingMapName( NULL );
		}
		if ( asw_building_room_thumbnails.GetBool() )
		{
			engine->ClientCmd( "mat_fullbright 1; wait; asw_buildroomthumbnails" );
			asw_building_room_thumbnails.SetValue( 0 );
		}
		if ( IsLocalPlayer( this ) )
		{
			int nSlot = C_BasePlayer::GetSplitScreenSlotForPlayer( this );
			ACTIVE_SPLITSCREEN_PLAYER_GUARD( nSlot );
			
#ifdef USE_MEDAL_STORE
			// inform server of our persistent counts
			int iKills, iMissions, iCampaigns;
			GetMedalStore()->GetCounts(iMissions, iCampaigns, iKills, (gpGlobals->maxClients <= 1));
			char buffer[48];
			Q_snprintf(buffer, sizeof(buffer), "cl_ccounts %d %d %d", iMissions, iCampaigns, iKills);
			engine->ClientCmd(buffer);
#endif
			// inform server of our autoreload preferences
			engine->ClientCmd( VarArgs( "cl_autoreload %d\n", asw_auto_reload.GetInt() ) );

			// tell other players that we're fully connected
			engine->ClientCmd( "cl_fullyjoined\n" );

			GetClientModeASW()->ClearCurrentColorCorrection();
		}

		RequestExperience();

#ifndef USE_XP_FROM_STEAM
		if ( IsLocalPlayer( this ) )
		{
			KeyValues *kv = new KeyValues( "XPUpdate" );
			kv->SetInt( "xp", m_iExperience );
			kv->SetInt( "pro", m_iPromotion );
			engine->ServerCmdKeyValues( kv );		// kv gets deleted in here
		}
#endif

		// We want to think every frame.
		SetNextClientThink( CLIENT_THINK_ALWAYS );
		return;
	}

	// update snow immediately upon changing marine
	if (g_hSnowEmitter.IsValid() && IsLocalPlayer(this))
	{
		C_ASW_Marine *pMarine = GetSpectatingMarine();
		if (!pMarine)
			pMarine = GetMarine();
		if (g_hSnowEmitter->m_hLastMarine.Get() != pMarine)
		{
			// update snow
			C_ASW_Snow_Volume::UpdateSnow(this);
		}
	}
}

C_BaseEntity* C_ASW_Player::GetUseEntity( void ) const 
{
	if ( m_iUseEntities > 0 && m_hUseEntities[ 0 ].Get() )
		return m_hUseEntities[ 0 ].Get();

	return BaseClass::GetUseEntity();
}

C_BaseEntity* C_ASW_Player::GetPotentialUseEntity( void ) const
{
	C_BaseEntity *pHighlightEntity = ASWInput()->GetHighlightEntity();

	if ( pHighlightEntity )
	{
		return pHighlightEntity;
	}

	return GetUseEntity();
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CalculateBouncePlane( const Vector &vecMarineCenter, const Vector &vecIntersectingCenter, float flHalfExtent, Vector &vecBouncePlane )
{
	vecBouncePlane.Init( 0.0f, 0.0f, 0.0f );
	if ( vecMarineCenter.x < vecIntersectingCenter.x - flHalfExtent )
	{
		vecBouncePlane.x = -1.0f;
	}
	else if ( vecMarineCenter.x > vecIntersectingCenter.x + flHalfExtent )
	{
		vecBouncePlane.x = 1.0f;
	}

	if ( vecMarineCenter.y < vecIntersectingCenter.y - flHalfExtent )
	{
		vecBouncePlane.y = -1.0f;
	}
	else if ( vecMarineCenter.y > vecIntersectingCenter.y + flHalfExtent )
	{
		vecBouncePlane.y = 1.0f;
	}

	VectorNormalize( vecBouncePlane );
}

#define ATTACH_ATTRACTION_FORCE 512.0f

//-----------------------------------------------------------------------------
// Purpose: Avoid marines and their sentry guns
//-----------------------------------------------------------------------------
void C_ASW_Player::AvoidMarines( CUserCmd *pCmd )
{
	// Turn off the player avoidance code.
	if ( !asw_player_avoidance.GetBool() )
		return;

	// Get the Alien Swarm game resource.
	C_ASW_Game_Resource *pGameResource = ASWGameResource();
	if ( !pGameResource )
		return;

	// Get the player controlled marine.
	// TODO: There may be more than one marine per player at some point.
	C_ASW_Marine *pMarine = GetMarine();
	if ( !pMarine )
		return;

	// Get the marine's collision properties.
	CCollisionProperty *pMarineCollision = pMarine->CollisionProp();
	if ( !pMarineCollision )
		return;

	// Test to see if the player controlled marine is dead.
	if ( !pMarine->IsAlive() )
		return;

	// Up vector.
	static Vector vecUp( 0.0f, 0.0f, 1.0f );

	// Get the player marine position and hull size.
	Vector vecMarineCenter = pMarineCollision->WorldSpaceCenter();
	Vector vecMarineMin, vecMarineMax;
	pMarineCollision->WorldSpaceAABB( &vecMarineMin, &vecMarineMax );

	// Find a marine to avoid - we only avoid one at a time.
	C_BaseEntity *pIntersectingEntity = NULL;
	CCollisionProperty *pIntersectingEntityCollision = NULL;
	Vector vecAvoidCenter, vecAvoidMin, vecAvoidMax;

	int nMarineCount = pGameResource->GetMaxMarineResources();
	for ( int iMarine = 0; iMarine < nMarineCount; ++iMarine )
	{	
		// Get the marine resource - if it exists.
		C_ASW_Marine_Resource *pTestMarineResource = pGameResource->GetMarineResource( iMarine );
		if ( !pTestMarineResource )
			continue;

		// Get the marine - if it exists.
		C_ASW_Marine *pTestMarine = pTestMarineResource->GetMarineEntity();
		if ( !pTestMarine )
			continue;

		// Is the player controlled marines?
		if ( pMarine == pTestMarine )
			continue;

		// Check to see if the avoid player is dormant.
		if ( pTestMarine->IsDormant() )
			continue;

		// Is the avoid player solid?
		if ( pTestMarine->IsSolidFlagSet( FSOLID_NOT_SOLID ) )
			continue;
		
		// Get the test marines collision properties.
		CCollisionProperty *pTestMarineCollision = pTestMarine->CollisionProp();
		if ( pTestMarineCollision )
		{
			vecAvoidCenter = pTestMarineCollision->WorldSpaceCenter();
			pTestMarineCollision->WorldSpaceAABB( &vecAvoidMin, &vecAvoidMax );

			// Collision?
			if ( IsBoxIntersectingBox( vecMarineMin, vecMarineMax, vecAvoidMin, vecAvoidMax ) )
			{
				pIntersectingEntity = pTestMarine;
				pIntersectingEntityCollision = pTestMarineCollision;
				break;
			}
		}
	}

	// No marines to avoid.  Check for sentry guns
	if ( !pIntersectingEntity )
	{
		for ( int i = 0; i < g_SentryGuns.Count(); i++ )
		{
			// Get the marine - if it exists.
			C_ASW_Sentry_Base *pTestSentry = g_SentryGuns[i];
			if ( !pTestSentry )
				continue;

			// Check to see if the avoid sentry is dormant.
			if ( pTestSentry->IsDormant() )
				continue;

			// Is the avoid sentry solid?
			if ( pTestSentry->IsSolidFlagSet( FSOLID_NOT_SOLID ) )
				continue;

			// Get the test sentry collision properties.
			CCollisionProperty *pTestSentryCollision = pTestSentry->CollisionProp();
			if ( pTestSentryCollision )
			{
				vecAvoidCenter = pTestSentryCollision->WorldSpaceCenter();
				pTestSentryCollision->WorldSpaceAABB( &vecAvoidMin, &vecAvoidMax );

				// Collision?
				if ( IsBoxIntersectingBox( vecMarineMin, vecMarineMax, vecAvoidMin, vecAvoidMax ) )
				{
					pIntersectingEntity = pTestSentry;
					pIntersectingEntityCollision = pTestSentryCollision;
					break;
				}
			}
		}
	}

	if ( !pIntersectingEntity )
		return;

	// Get marine speeds.
	float flMarineSpeed = pMarine->GetAbsVelocity().Length2D();
	float flIntersectingMarineSpeed = pIntersectingEntity->GetAbsVelocity().Length2D();

	// If the local marine is not moving and the intersecting marine is, then do nothing.
	if ( flMarineSpeed < 0.1f && flIntersectingMarineSpeed > 0.1f )
		return;

	// Find the distance between marines and scale the extraction push by the marine bounds.
	Vector vecDelta;
	Vector vecIntersectingMarineCenter = pIntersectingEntityCollision->WorldSpaceCenter();
	VectorSubtract( vecMarineCenter, vecIntersectingMarineCenter, vecDelta );
	float flAvoidRadius = pIntersectingEntityCollision->BoundingRadius();
	float flPushStrength = RemapValClamped( vecDelta.Length(), flAvoidRadius, 0, 0, asw_player_avoidance_force.GetInt() ); 
	if ( flPushStrength < 0.01f )
		return;

	Vector vecSeparationVelocity;
	Vector vecPush;
	Vector vecBouncePlane;
	CalculateBouncePlane( vecMarineCenter, vecIntersectingMarineCenter, asw_player_avoidance_fakehull.GetFloat(), vecBouncePlane );

	// The player marine is moving.
	float flPushThreshold = ASW_MOVEMENT_NORM_SPEED * 0.90f;
	if ( flMarineSpeed > flPushThreshold )
	{
		Vector vecVelocity = pMarine->GetAbsVelocity();
		VectorNormalize( vecVelocity );
		Vector vecDeltaNorm = vecDelta;
		VectorNormalize( vecDeltaNorm );
		float flBackoff = DotProduct( vecVelocity, vecDeltaNorm ) * asw_player_avoidance_bounce.GetFloat();
		for ( int iAxis = 0; iAxis < 3; ++iAxis )
		{
			vecPush[iAxis] = ( vecVelocity[iAxis] - ( vecDeltaNorm[iAxis] * flBackoff ) );
		}

		VectorNormalize( vecPush );
		vecSeparationVelocity = vecPush * flPushStrength * 0.25f;
	}
	else
	{
		if ( vecBouncePlane.Length2DSqr() < 0.1f )
		{
			vecBouncePlane = vecDelta;
			VectorNormalize( vecBouncePlane );
		}

		vecPush = vecBouncePlane;
 		VectorNormalize( vecPush );
		vecSeparationVelocity = vecPush * flPushStrength;

		// Debug!
// 		debugoverlay->AddLineOverlay( vecDelta + vecMarineCenter, ( vecBouncePlane * 225.0f ) + vecMarineCenter, 255, 0, 0, true, 0.05f );
// 		debugoverlay->AddLineOverlay( vecPush + vecMarineCenter, ( vecPush * 225.0f ) + vecMarineCenter, 255, 255, 0, true, 0.05f );
// 		debugoverlay->AddLineOverlay( vecVelocity + vecMarineCenter, ( vecVelocity * 225.0f ) + vecMarineCenter, 0, 0, 255, true, 0.05f );
	}

	// Don't allow the max push speed to be greater than the max player speed.
	float flMaxPlayerSpeed = ASW_MOVEMENT_NORM_SPEED;
	if ( IsWalking() )
	{
		flMaxPlayerSpeed = ASW_MOVEMENT_WALK_SPEED;
	}
	float flMaxPlayerSpeedSqr = flMaxPlayerSpeed * flMaxPlayerSpeed;
	if ( vecSeparationVelocity.LengthSqr() > flMaxPlayerSpeedSqr )
	{
		vecSeparationVelocity.NormalizeInPlace();
		VectorScale( vecSeparationVelocity, flMaxPlayerSpeed, vecSeparationVelocity );
	}

	Vector currentdir;
	Vector rightdir;
	AngleVectors( ASW_MOVEMENT_AXIS, &currentdir, &rightdir, NULL );

	Vector vDirection = vecSeparationVelocity;
	VectorNormalize( vDirection );
	float fwd = currentdir.Dot( vDirection );
	float rt = rightdir.Dot( vDirection );

	float forward = fwd * flPushStrength;
	float side = rt * flPushStrength;

	//Msg( "fwd: %f - rt: %f - forward: %f - side: %f\n", fwd, rt, forward, side );

	pCmd->forwardmove	+= forward;
	pCmd->sidemove		+= side;

	// Clamp the move to within legal limits, preserving direction. This is a little
	// complicated because we have different limits for forward, back, and side

	//Msg( "PRECLAMP: forwardmove=%f, sidemove=%f\n", pCmd->forwardmove, pCmd->sidemove );

	float flForwardScale = 1.0f;
	if ( pCmd->forwardmove > fabs( cl_forwardspeed.GetFloat() ) )
	{
		flForwardScale = fabs( cl_forwardspeed.GetFloat() ) / pCmd->forwardmove;
	}
	else if ( pCmd->forwardmove < -fabs( cl_backspeed.GetFloat() ) )
	{
		flForwardScale = fabs( cl_backspeed.GetFloat() ) / fabs( pCmd->forwardmove );
	}

	float flSideScale = 1.0f;
	if ( fabs( pCmd->sidemove ) > fabs( cl_sidespeed.GetFloat() ) )
	{
		flSideScale = fabs( cl_sidespeed.GetFloat() ) / fabs( pCmd->sidemove );
	}

	float flScale = MIN( flForwardScale, flSideScale );
	pCmd->forwardmove *= flScale;
	pCmd->sidemove *= flScale;

	//Msg( "Pforwardmove=%f, sidemove=%f\n", pCmd->forwardmove, pCmd->sidemove );
}


bool C_ASW_Player::CreateMove( float flInputSampleTime, CUserCmd *pCmd )
{
	bool bResult = BaseClass::CreateMove( flInputSampleTime, pCmd );
	AvoidMarines( pCmd );

	if ( !IsInAVehicle() && asw_clientside_avoidance.GetBool() && !asw_marine_collision.GetBool())
	{
		MarinePerformClientSideObstacleAvoidance( TICK_INTERVAL, pCmd );
	}

	C_ASW_Marine* pMarine = GetMarine();

	// if we're using a turret, clamp view angles to the ones set in the turret
	if (pMarine && pMarine->IsControllingTurret() && pMarine->GetRemoteTurret())
	{
		pMarine->GetRemoteTurret()->CreateMove( flInputSampleTime, pCmd );
	}

	// check if the marine is meant to be standing still	
	if (pMarine && 
		(gpGlobals->curtime < pMarine->GetStopTime() || pMarine->m_bPreventMovement
		|| CASW_VGUI_Info_Message::HasInfoMessageOpen()
		|| ( ASWInput()->ControllerModeActive() && pMarine->IsUsingComputerOrButtonPanel() )
		|| pMarine->IsUsingComputerOrButtonPanel()
		))
	{
		// asw temp comment
		/*
		pCmd->forwardmove = 0;
		pCmd->sidemove = 0;
		*/
	}

	if (!pMarine && asw_rts_controls.GetBool())
	{
		// set forward/side move to scroll the screen when mouse is at the edges
		int current_posx, current_posy;
		::input->GetFullscreenMousePos(&current_posx, &current_posy);

		//Msg("Cursor pos = %d %d ", current_posx, current_posy);
		if (current_posx <= 0)
		{
			//Msg("Scrolling left ");
			pCmd->sidemove = -cl_sidespeed.GetFloat();
		}
		else if (current_posx >= ScreenWidth()-1)
		{
			//Msg("Scrolling right ");
			pCmd->sidemove = cl_sidespeed.GetFloat();
		}
		if (current_posy <= 0)
		{
			//Msg("Scrolling up ");
			pCmd->forwardmove = cl_sidespeed.GetFloat();
		}
		else if (current_posy >= ScreenHeight()-1)
		{
			//Msg("Scrolling down ");
			pCmd->forwardmove = -cl_sidespeed.GetFloat();
		}
		//Msg("\n");
	}

	if ( pMarine && pMarine->GetCurrentMeleeAttack() )
	{
		ASWMeleeSystem()->CreateMove( flInputSampleTime, pCmd, pMarine );
	}

	return bResult;
}

#define LOOK_AHEAD 3.0f
void C_ASW_Player::MarineStopMoveIfBlocked(float flFrameTime, CUserCmd *pCmd, C_ASW_Marine* pMarine)
{
	C_ASW_Game_Resource *pGameResource = ASWGameResource();
	if (!pGameResource)
		return;
	Vector size = pMarine->WorldAlignSize();	
	//float radius = 1.0f * sqrt( size.x * size.x + size.y * size.y );
	size *= 2;
	Vector currentdir;
	Vector rightdir;
	QAngle vAngles = pCmd->viewangles;
	vAngles.x = 0;

	if ( asw_controls.GetInt() == 1 )
		AngleVectors( ASW_MOVEMENT_AXIS, &currentdir, &rightdir, NULL );
	else
		AngleVectors( vAngles, &currentdir, &rightdir, NULL );

	
	
	Vector vel = pMarine->GetLocalVelocity();	// our current velocity, will it take us inside an NPC?	
	bool bStill = (vel.Length2D() == 0);
	if (pCmd->sidemove == 00 && pCmd->forwardmove == 0)
		m_bGuidingMarine = false;	// make sure to stop all guiding if the player lets go of movement keys
	Vector vecPlayerPushing(pCmd->sidemove, pCmd->forwardmove, 0);
	if (m_bGuidingMarine)
	{
		// make the marine move in the direction we want to guide in
		pCmd->forwardmove = m_vecGuiding.y;
		pCmd->sidemove = m_vecGuiding.x;
		if ( pCmd->forwardmove > 0.0f )
		{
			pCmd->forwardmove *= cl_forwardspeed.GetFloat();
		}
		else
		{
			pCmd->forwardmove *= cl_backspeed.GetFloat();
		}
		pCmd->sidemove *= cl_sidespeed.GetFloat();
	}
	//Msg("vel %f, %f, %f 2dlen %f still %d\n", vel.x, vel.y, vel.z, vel.Length2D(), bStill);
	
	//INetChannelInfo *nci = engine->GetNetChannelInfo();
	

	// check if we're stuck inside any NPCS and push ourselves out if we are
	Vector vecMarinePos = pMarine->GetAbsOrigin();
	Vector vecMarineSize = pMarine->WorldAlignSize();	
	//float fMarineRadius = 1.0f * sqrt( vecMarineSize.x * vecMarineSize.x + vecMarineSize.y * vecMarineSize.y );
	int c = pGameResource->EnumerateMarinesInBox(vecMarinePos - (vecMarineSize * 0.5f), vecMarinePos + (vecMarineSize * 0.5f));
	//CASW_MarineAndObjectEnumerator stuckcheck( fMarineRadius );
	//partition->EnumerateElementsInBox(PARTITION_CLIENT_SOLID_EDICTS,
				//vecMarinePos - (vecMarineSize * 0.5f),
				//vecMarinePos + (vecMarineSize * 0.5f), false, &stuckcheck );
	if (asw_debug_clientside_avoidance.GetBool())
		debugoverlay->AddBoxOverlay( vecMarinePos, -vecMarineSize * 0.5f, vecMarineSize * 0.5f, vec3_angle, 0, 255, 0, true, 0 );

	float adjustforwardmove = 0.0f;
	float adjustsidemove	= 0.0f;

	//int c = stuckcheck.GetObjectCount();
	//bool bStuckInside = (c > 0);
	//if ( c <= 0 )
		//return;
	
	int i = 0;
	for ( i = 0; i < c; i++ )
	{
		//C_AI_BaseNPC *obj = dynamic_cast< C_AI_BaseNPC *>(stuckcheck.GetObject( i ));
		C_ASW_Marine *obj = pGameResource->EnumeratedMarine(i);

		if( !obj || obj == pMarine )
			continue;

		// don't avoid aliens laying dead on the floor
		if (!obj->IsAlive() || obj->GetHealth() <= 0)
			continue;
		
		Vector vecToObject = obj->GetAbsOrigin() - pMarine->GetAbsOrigin();
		Vector vRayDir = vecToObject;
		VectorNormalize( vRayDir );

		adjustforwardmove -= vRayDir.y * AVOID_SPEED;
		adjustsidemove -= vRayDir.x * AVOID_SPEED;

		//Msg("inside an npc %f %f %f\n", vRayDir.x, vRayDir.y, vRayDir.z);
	}

	pCmd->forwardmove	+= adjustforwardmove;
	pCmd->sidemove		+= adjustsidemove;

	if ( pCmd->forwardmove > 0.0f )
	{
		pCmd->forwardmove = clamp( pCmd->forwardmove, -cl_forwardspeed.GetFloat(), cl_forwardspeed.GetFloat() );
	}
	else
	{
		pCmd->forwardmove = clamp( pCmd->forwardmove, -cl_backspeed.GetFloat(), cl_backspeed.GetFloat() );
	}
	pCmd->sidemove = clamp( pCmd->sidemove, -cl_sidespeed.GetFloat(), cl_sidespeed.GetFloat() );


	Vector forward_vel = pCmd->forwardmove * currentdir;
	Vector side_vel = pCmd->sidemove * rightdir;
	if (bStill)
		vel = forward_vel + side_vel;
	float fVelScale = flFrameTime * LOOK_AHEAD;
	
	//fVelScale = nci->GetLatency( FLOW_OUTGOING );
	////int lerpTicks = TIME_TO_TICKS( 0.1f );	// fix: hardcoded 0.1f amount of lerptime!
	////fVelScale += TICKS_TO_TIME( lerpTicks );

	Vector vel_norm = vel;
	if (!bStill)
		vel_norm.NormalizeInPlace();
	Vector dest = pMarine->GetAbsOrigin() + (vel_norm * fVelScale);

	// if our new position puts us inside an NPC, make a move to get rid of our current velocity
	c = pGameResource->EnumerateMarinesInBox(dest - (size * 0.5f), dest + (size * 0.5f));
	//CASW_MarineAndObjectEnumerator avoid( radius );
	//partition->EnumerateElementsInBox(PARTITION_CLIENT_SOLID_EDICTS, dest - (size * 0.5f), dest + (size * 0.5f), false, &avoid );
	if (asw_debug_clientside_avoidance.GetBool())
		debugoverlay->AddBoxOverlay( dest, -size * 0.5f, size * 0.5f, vec3_angle, 0, 255, 255, true, 0 );

	//c = avoid.GetObjectCount();	
	for ( i = 0; i < c; i++ )
	{
		//C_AI_BaseNPC *obj = dynamic_cast< C_AI_BaseNPC *>(avoid.GetObject( i ));
		C_ASW_Marine *obj = pGameResource->EnumeratedMarine(i);

		if( !obj || obj == pMarine )
			continue;

		// don't avoid aliens laying dead on the floor
		if (!obj->IsAlive() || obj->GetHealth() <= 0)
			continue;

		if (asw_debug_clientside_avoidance.GetBool())
		{
			Vector vecOtherSize = obj->WorldAlignSize();
			debugoverlay->AddBoxOverlay( obj->GetAbsOrigin(), -vecOtherSize * 0.5f, vecOtherSize * 0.5f, vec3_angle, 255, 0, 0, true, 5.0f );
		}

		// this velocity would put us inside an NPC.  Construct a move to cancel the velocity
		//pCmd->forwardmove = 0;
		//pCmd->sidemove = 0;
		Vector moveDir = vel;
		VectorNormalize( moveDir );

		Vector vecToObject = obj->GetAbsOrigin() - pMarine->GetAbsOrigin();
		Vector vRayDir = vecToObject;
		VectorNormalize( vRayDir );
		
		if (bStill)
		{
			// okay, we're still, but trying to push into a marine, start the guiding code to walk us around him instead
			m_bGuidingMarine = true;
			Vector velCopy(vel);
			velCopy.NormalizeInPlace();
			float vel_yaw = UTIL_VecToYaw(velCopy);
			float object_yaw = UTIL_VecToYaw(vRayDir);
			float yaw_diff = UTIL_AngleDiff(vel_yaw, object_yaw);
			if (yaw_diff > 0)	// the object is ? of our current direction, so let's push 45 degrees to the ?
			{
				//Msg("travelling at %f degrees, will go left to %f degrees\n", vel_yaw, vel_yaw + 90);
				m_vecGuiding.x = cos(DEG2RAD(vel_yaw + 90));
				m_vecGuiding.y = sin(DEG2RAD(vel_yaw + 90));
			}
			else
			{
				//Msg("travelling at %f degrees, will go right to %f degrees\n", vel_yaw, vel_yaw - 90);
				m_vecGuiding.x = cos(DEG2RAD(vel_yaw - 90));
				m_vecGuiding.y = sin(DEG2RAD(vel_yaw - 90));
			}
			return;
		}
		float fMinDot = 0.6f;	// todo: make this depend on how far away we are
		if (moveDir.Dot(vRayDir) < fMinDot)	// is our velocity taking us away from the npc?  if so, it's okay
		{
			// but should check our command move isn't taking it towards it
			if (bStill)
				continue;
			
			vel = forward_vel + side_vel;
			moveDir = vel;
			VectorNormalize( moveDir );
			if (moveDir.Dot(vRayDir) > fMinDot)		// we're moving away from the NPC, but the player is trying to push back into it, stop him
			{				
				pCmd->forwardmove = 0;
				pCmd->sidemove = 0;
			}

			continue;
		}		
		// otherwise, assume we're trying to run into the NPC and need to stop, like now
		//if (!bStill)
		{
			pCmd->forwardmove = 0;
			pCmd->sidemove = 0;
			// IN_ASW_STOP removed
			//pCmd->buttons |= IN_ASW_STOP;
			return;
		}

		/*float fwd = currentdir.Dot( -moveDir );
		float rt = rightdir.Dot( -moveDir );

		if (bStill)
		{
			pCmd->forwardmove = 0;
			pCmd->sidemove = 0;
		}
		else
		{
			pCmd->forwardmove = fwd * cl_forwardspeed.GetFloat();
			pCmd->sidemove = rt * cl_forwardspeed.GetFloat();
		}

		pCmd->forwardmove = clamp( pCmd->forwardmove, -cl_forwardspeed.GetFloat(), cl_forwardspeed.GetFloat() );	
		pCmd->sidemove = clamp( pCmd->sidemove, -cl_sidespeed.GetFloat(), cl_sidespeed.GetFloat() );
		return;*/
	}
	if (m_bGuidingMarine)
	{
		// check if the original way is safe to revert to
		
		dest = pMarine->GetAbsOrigin() + (vecPlayerPushing * fVelScale);

		// if our new position puts us inside an NPC, make a move to get rid of our current velocity
		c = pGameResource->EnumerateMarinesInBox(dest - (size * 0.5f), dest + (size * 0.5f));
		//CASW_MarineAndObjectEnumerator avoidoriginal( radius );
		//partition->EnumerateElementsInBox(PARTITION_CLIENT_SOLID_EDICTS, dest - (size * 0.5f), dest + (size * 0.5f), false, &avoidoriginal );
		if (asw_debug_clientside_avoidance.GetBool())
			debugoverlay->AddBoxOverlay( dest, -size * 0.5f, size * 0.5f, vec3_angle, 0, 0, 255, true, 0 );

		//c = avoidoriginal.GetObjectCount();
		if ( c <= 0 )
		{
			m_bGuidingMarine = false;	// if we're moving freely, no need to guide anymore
			return;
		}
		
		for ( i = 0; i < c; i++ )
		{
			//C_AI_BaseNPC *obj = dynamic_cast< C_AI_BaseNPC *>(avoidoriginal.GetObject( i ));
			C_ASW_Marine *obj = pGameResource->EnumeratedMarine(i);

			if( !obj || obj == pMarine )
				continue;

			// don't avoid aliens laying dead on the floor
			if (!obj->IsAlive() || obj->GetHealth() <= 0)
				continue;

			return;
		}
		m_bGuidingMarine = false;	// if we're moving freely and the original way is clear, no need to guide anymore
	}	
	// if we got here, it means we didn't have to do any emergency stopping
	//if (!bStill)	// if we're moving somewhere and we're didn't have to emergency stop, chances are we're okay
		//return;	
}

void C_ASW_Player::MarinePerformClientSideObstacleAvoidance( float flFrameTime, CUserCmd *pCmd )
{
	C_ASW_Marine* pMarine = GetMarine();
	if (! pMarine)
	{
		return;
	}

	// Don't avoid if noclipping or in movetype none
	switch ( pMarine->GetMoveType() )
	{
	case MOVETYPE_NOCLIP:
	case MOVETYPE_NONE:
	case MOVETYPE_OBSERVER:
		return;
	default:
		break;
	}

// 	if (gpGlobals->maxClients > 1)
// 		MarineStopMoveIfBlocked(flFrameTime, pCmd, pMarine);		
	return;

	// Try to steer away from any objects/players we might interpenetrate
	Vector size = pMarine->WorldAlignSize();

	float radius = 0.7f * sqrt( size.x * size.x + size.y * size.y );
	float curspeed = pMarine->GetLocalVelocity().Length2D();

//	int slot = 1;

//	engine->Con_NPrintf( slot++, "speed %f\n", curspeed );
//	engine->Con_NPrintf( slot++, "radius %f\n", radius );

	// If running, use a larger radius
	float factor = 1.0f;

	if ( curspeed > 150.0f )
	{
		factor = ( 1.0f + ( curspeed - 150.0f ) / 150.0f );

	//	engine->Con_NPrintf( slot++, "scaleup (%f) to radius %f\n", factor, radius * factor );

		radius = radius * factor;
	}

	Vector currentdir;
	Vector rightdir;
	
	QAngle vAngles = pCmd->viewangles;	

	vAngles.x = 0;

	if (asw_controls.GetBool())
	{
		AngleVectors( ASW_MOVEMENT_AXIS, &currentdir, &rightdir, NULL );
	}
	else
	{
		AngleVectors( vAngles, &currentdir, &rightdir, NULL );
	}
	
	
		
	bool istryingtomove = false;
	bool ismovingforward = false;
	if ( fabs( pCmd->forwardmove ) > 0.0f || 
		 fabs( pCmd->sidemove ) > 0.0f )
	{
		istryingtomove = true;
		if ( pCmd->forwardmove > 1.0f )
		{
			ismovingforward = true;
		}
	}

	// asw - no..
	//if ( istryingtomove == true )
		 //radius *= 1.3f;

	CASW_MarineAndObjectEnumerator avoid( radius );
	partition->EnumerateElementsInSphere( PARTITION_CLIENT_SOLID_EDICTS, pMarine->GetAbsOrigin(), radius, false, &avoid );

	// Okay, decide how to avoid if there's anything close by
	int c = avoid.GetObjectCount();
	if ( c <= 0 )
		return;

	//engine->Con_NPrintf( slot++, "moving %s forward %s\n", istryingtomove ? "true" : "false", ismovingforward ? "true" : "false"  );

	float adjustforwardmove = 0.0f;
	float adjustsidemove	= 0.0f;

	int i;
	for ( i = 0; i < c; i++ )
	{
		C_AI_BaseNPC *obj = dynamic_cast< C_AI_BaseNPC *>(avoid.GetObject( i ));

		if( !obj || obj == pMarine )
			continue;

		// don't avoid aliens laying dead on the floor
		if (!obj->IsAlive() || obj->GetHealth() <= 0)
			continue;

		Vector vecToObject = obj->GetAbsOrigin() - pMarine->GetAbsOrigin();

		float flDist = vecToObject.Length2D();
		
		// Figure out a 2D radius for the object
		Vector vecWorldMins, vecWorldMaxs;
		obj->CollisionProp()->WorldSpaceAABB( &vecWorldMins, &vecWorldMaxs );
		Vector objSize = vecWorldMaxs - vecWorldMins;

		//float objectradius = 0.5f * sqrt( objSize.x * objSize.x + objSize.y * objSize.y );
		float objectradius = 1.0f * sqrt( objSize.x * objSize.x + objSize.y * objSize.y ) * asw_clientside_avoidance_scale.GetFloat();

		//Don't run this code if the NPC is not moving UNLESS we are in stuck inside of them.
		//if ( !obj->IsMoving() && flDist > objectradius )
			 // continue;

		float flHit1, flHit2;

		Vector vRayDir = vecToObject;

		VectorNormalize( vRayDir );

		if ( !IntersectInfiniteRayWithSphere( pMarine->GetAbsOrigin(), vRayDir,
												obj->GetAbsOrigin(), radius,
												&flHit1, &flHit2 ) )
			continue;

		float force = 1.0f;

		float forward = 0.0f, side = 0.0f;

		Vector vNPCForward, vNPCRight;
		AngleVectors( obj->GetAbsAngles(), &vNPCForward, &vNPCRight, NULL );

		Vector moveDir = -vecToObject;
		VectorNormalize( moveDir );

		float fwd = currentdir.Dot( moveDir );
		float rt = rightdir.Dot( moveDir );

		float sidescale = 2.0f;
		float forwardscale = 1.0f;

		if ( flDist < objectradius )
		{
			 fwd = currentdir.Dot( -vNPCForward );
			 rt = rightdir.Dot( -vNPCForward );

			 //obj->SetEffects( EF_NODRAW );
		}

		// If running, then do a lot more sideways veer since we're not going to do anything to
		//  forward velocity
		// asw - no, looks weird when you don't get pushed when still, but do when moving away
		//if ( istryingtomove )
		//{
			//sidescale = 6.0f;
		//}

	//	engine->Con_NPrintf( slot++, "sqrt(force) == %f\n", force );

	//	engine->Con_NPrintf( slot++, "fwd %f right %f\n", fwd, rt );
		forward = forwardscale * fwd * force * AVOID_SPEED;
		side = sidescale * rt * force * AVOID_SPEED;

	//	engine->Con_NPrintf( slot++, "forward %f side %f\n", forward, side );

		adjustforwardmove	+= forward;
		adjustsidemove		+= side;
	}

	pCmd->forwardmove	+= adjustforwardmove;
	pCmd->sidemove		+= adjustsidemove;

	if ( pCmd->forwardmove > 0.0f )
	{
		pCmd->forwardmove = clamp( pCmd->forwardmove, -cl_forwardspeed.GetFloat(), cl_forwardspeed.GetFloat() );
	}
	else
	{
		pCmd->forwardmove = clamp( pCmd->forwardmove, -cl_backspeed.GetFloat(), cl_backspeed.GetFloat() );
	}
	pCmd->sidemove = clamp( pCmd->sidemove, -cl_sidespeed.GetFloat(), cl_sidespeed.GetFloat() );
}

void C_ASW_Player::OnMissionRestart()
{
	GetClientModeASW()->m_bLaunchedBriefing = false;
	GetClientModeASW()->m_bLaunchedDebrief = false;
	m_fLastRestartTime = gpGlobals->curtime;
	// remove all decals	
	engine->ClientCmd( "r_cleardecals\n" );

	// is this needed?
	// recreate all client side physics props
	// check for physenv, because we sometimes crash on changelevel
	// if we get this message before fully connecting
	//if ( physenv )
	//{
        //C_PhysPropClientside::RecreateAll();
	//}

	// todo: remove other clientside effects from the map?

	// reset vehicle cam yaw
	m_fLastVehicleYaw = 0;
}

void C_ASW_Player::SendBlipSpeech(int iMarine)
{
	char buffer[64];
	sprintf(buffer, "cl_blipspeech %d", iMarine);
	engine->ClientCmd(buffer);
}

void C_ASW_Player::CreateStimCamera()
{
	m_pStimCam = new C_ASW_PointCamera;
	if (m_pStimCam)
	{
		if (!m_pStimCam->InitializeAsClientEntity( NULL, false ))
		{
			UTIL_Remove( m_pStimCam );
			return;
		}
		m_pStimCam->ApplyStimCamSettings();
	}
}

// smooth the Z motion of the camera
#define ASW_CAMERA_Z_SPEED 60
void C_ASW_Player::SmoothCameraZ(Vector &CameraPos)
{
	// no change if we have no marine or just starting out or just changed marine
	if (!GetMarine() || GetMarine()!=m_hLastMarine.Get() || m_vecLastCameraPosition == vec3_origin)
	{
		// clear any poison effects - bad place for this!
		g_fMarinePoisonDuration = 0;
		return;
	}

	float fAmountToMove = abs(CameraPos.z - m_vecLastCameraPosition.z) / 10.0f;
	fAmountToMove *= gpGlobals->frametime * ASW_CAMERA_Z_SPEED;
	if (CameraPos.z > m_vecLastCameraPosition.z)
	{
		//CameraPos.z = MIN(m_vecLastCameraPosition.z + gpGlobals->frametime * ASW_CAMERA_Z_SPEED, CameraPos.z);
		CameraPos.z = MIN(m_vecLastCameraPosition.z + fAmountToMove, CameraPos.z);
	}
	else if (CameraPos.z < m_vecLastCameraPosition.z)
	{
		//CameraPos.z = MAX(m_vecLastCameraPosition.z - gpGlobals->frametime * ASW_CAMERA_Z_SPEED, CameraPos.z);
		CameraPos.z = MAX(m_vecLastCameraPosition.z - fAmountToMove, CameraPos.z);
	}
}	

// smooth the camera's overall coords when the player changes from marine to marine
bool C_ASW_Player::SmoothMarineChangeCamera(Vector &CameraPos)
{
	if (GetMarine() && GetMarine() != m_hLastMarine.Get() && m_hLastMarine.Get())
	{
		// we changed, need to setup our smoothing vector		
		if (m_fMarineChangeSmooth <= 0 && GetMarine()->GetAbsOrigin().DistTo(m_vecMarineChangeCameraPos) < asw_marine_switch_blend_max_dist.GetFloat())
		{
			// check the camera can slide between the two camera positions without going inside a solid
			trace_t tr;
			UTIL_TraceLine(CameraPos, m_vecMarineChangeCameraPos, MASK_SOLID, NULL, COLLISION_GROUP_NONE, &tr);
			if (tr.fraction >= 1.0f)
				m_fMarineChangeSmooth = 1.0f;
		}
	}

	if (m_fMarineChangeSmooth > 0)	// we're in the middle of a smooth, transition between the two
	{
		Vector diff = m_vecMarineChangeCameraPos - CameraPos;
		float f = 1.0f - ( ( sin(m_fMarineChangeSmooth * DEG2RAD(180) + DEG2RAD(90)) * 0.5f )+ 0.5f);

		diff *= f;
		CameraPos += diff;

		m_fMarineChangeSmooth -= gpGlobals->frametime * asw_marine_switch_blend_speed.GetFloat();
	}
	else	// store the current camera pos incase we do a switch next frame
	{
		m_vecMarineChangeCameraPos = CameraPos;
		return false;
	}
	return true;
}

// smooth the Yaw motion of the camera when driving a vehicle
#define ASW_CAMERA_YAW_SPEED 60
float C_ASW_Player::ASW_ClampYaw( float yawSpeedPerSec, float current, float target, float time )
{
	if (current != target)
	{
		float speed = yawSpeedPerSec * time;
		float move = target - current;

		if (target > current)
		{
			if (move >= 180)
				move = move - 360;
		}
		else
		{
			if (move <= -180)
				move = move + 360;
		}

		speed *= abs(move) / 360.0f;

		if (move > 0)
		{// turning to the npc's left
			if (move > speed)
				move = speed;
		}
		else
		{// turning to the npc's right
			if (move < -speed)
				move = -speed;
		}
		
		return anglemod(current + move);
	}
	
	return target;
}

// smoothly rotates the camera yaw to the desired value, to give the camera a nice springy feeling when driving
void C_ASW_Player::SmoothCameraYaw(float &yaw)
{
	if (!GetMarine())
		return;
	// no change if we have no marine or just starting out or just changed marine
	if (GetMarine()->IsInVehicle()!=m_bLastInVehicle || m_fLastVehicleYaw == 0)
	{
		m_bLastInVehicle = GetMarine()->IsInVehicle();
		m_fLastVehicleYaw = yaw;
		return;
	}

	float dt = MIN( 0.2, gpGlobals->frametime );
	
	yaw = ASW_ClampYaw( 1000.0f, m_fLastVehicleYaw, yaw, dt );

	m_bLastInVehicle = GetMarine()->IsInVehicle();
	m_fLastVehicleYaw = yaw;//GetMarine()->EyeAngles()[YAW];
}

// smooth the simulated floo rused for aiming
#define ASW_FLOOR_Z_SPEED 100
void C_ASW_Player::SmoothAimingFloorZ(float &FloorZ)
{
	if (m_fLastFloorZ == 0 || m_hLastAimingFloorZMarine.Get() != GetMarine())
	{
		m_fLastFloorZ = FloorZ;
		m_hLastAimingFloorZMarine = GetMarine();
		return;
	}
	float fAmountToMove = abs(FloorZ - m_fLastFloorZ) / 10.0f;
	fAmountToMove *= gpGlobals->frametime * ASW_FLOOR_Z_SPEED;
	if (FloorZ > m_fLastFloorZ)
	{
		FloorZ = MIN(m_fLastFloorZ + fAmountToMove, FloorZ);
	}
	else if (FloorZ < m_fLastFloorZ)
	{
		FloorZ = MAX(m_fLastFloorZ - fAmountToMove, FloorZ);
	}
	m_fLastFloorZ = FloorZ;
}	

void C_ASW_Player::RequestMissionRestart()
{
	char buffer[20];
	Q_snprintf(buffer, sizeof(buffer), "cl_restart_mission");
	engine->ClientCmd(buffer);
}

void C_ASW_Player::RequestSkillUp()
{
	char buffer[20];
	Q_snprintf(buffer, sizeof(buffer), "cl_skill_up");
	engine->ClientCmd(buffer);
}

void C_ASW_Player::RequestSkillDown()
{
	char buffer[20];
	Q_snprintf(buffer, sizeof(buffer), "cl_skill_down");
	engine->ClientCmd(buffer);
}

bool C_ASW_Player::ShouldAutoReload()
{
	return asw_auto_reload.GetBool();
}

C_BaseCombatWeapon *C_ASW_Player::GetActiveWeapon( void ) const
{
	const C_ASW_Marine *pMarine = GetMarine();
	if ( !pMarine )
	{
		return BaseClass::GetActiveWeapon();
	}

	return pMarine->GetActiveWeapon();
}

C_BaseCombatWeapon *C_ASW_Player::GetWeapon( int i ) const
{
	const C_ASW_Marine *pMarine = GetMarine();
	if ( !pMarine )
	{
		return BaseClass::GetWeapon( i );
	}

	return pMarine->GetWeapon( i );
}

int C_ASW_Player::GetHealth() const
{
	const C_ASW_Marine *pMarine = GetMarine();
	if ( !pMarine )
	{
		return BaseClass::GetHealth();
	}

	return pMarine->GetHealth();
}

int C_ASW_Player::GetMaxHealth() const
{
	const C_ASW_Marine *pMarine = GetMarine();
	if ( !pMarine )
	{
		return BaseClass::GetMaxHealth();
	}

	return pMarine->GetMaxHealth();
}

C_ASW_Marine* C_ASW_Player::FindMarineToHoldOrder(const Vector &pos)
{
	C_ASW_Game_Resource *pGameResource = ASWGameResource();
	if (!pGameResource)
		return NULL;

	C_ASW_Marine *pMyMarine = GetMarine();
	if (!pMyMarine)
		return NULL;

	// check if we preselected a specific marine to order
	C_ASW_Marine *pTarget = m_hOrderingMarine.Get();

	// find the nearest marine
	if (!pTarget)
	{
		float nearest_dist = 9999;
		for (int i=0;i<pGameResource->GetMaxMarineResources();i++)
		{
			C_ASW_Marine_Resource* pMR = pGameResource->GetMarineResource(i);
			if (!pMR)
				continue;

			C_ASW_Marine* pMarine = pMR->GetMarineEntity();
			if (!pMarine || pMarine == pMyMarine || pMarine->GetHealth() <= 0
					|| pMarine->GetCommander()!=this)		// skip if dead
				continue;
		
			float distance = pos.DistTo(pMarine->GetAbsOrigin());
			//if (pMarine->GetASWOrders() != ASW_ORDER_FOLLOW)		// bias against marines that are already holding position somewhere
				//distance += 5000;
			if (distance < nearest_dist)
			{
				nearest_dist = distance;
				pTarget = pMarine;
			}
		}
	}
	return pTarget;
}

C_ASW_Marine* C_ASW_Player::FindMarineToFollowOrder(const Vector &pos)
{
	C_ASW_Game_Resource *pGameResource = ASWGameResource();
	if (!pGameResource)
		return NULL;

	C_ASW_Marine *pMyMarine = GetMarine();
	if (!pMyMarine)
		return NULL;

	// check if we preselected a specific marine to order
	C_ASW_Marine *pTarget = m_hOrderingMarine.Get();

	// find the nearest marine
	if (!pTarget)
	{
		float nearest_dist = 9999;
		for (int i=0;i<pGameResource->GetMaxMarineResources();i++)
		{
			C_ASW_Marine_Resource* pMR = pGameResource->GetMarineResource(i);
			if (!pMR)
				continue;

			C_ASW_Marine* pMarine = pMR->GetMarineEntity();
			if (!pMarine || pMarine == pMyMarine || pMarine->GetHealth() <= 0
					|| pMarine->GetCommander()!=this)		// skip if dead
				continue;

			if (pMarine->GetASWOrders() == ASW_ORDER_FOLLOW)
				continue;
		
			float distance = pos.DistTo(pMarine->GetAbsOrigin());
			
			if (distance < nearest_dist)
			{
				nearest_dist = distance;
				pTarget = pMarine;
			}
		}
	}
	return pTarget;
}

ConVar asw_local_dlight_radius("asw_local_dlight_radius", "0", FCVAR_CHEAT, "Radius of the light around the marine.");
ConVar asw_local_dlight_offsetx("asw_local_dlight_offsetx", "0", FCVAR_CHEAT, "Offset of the local dlight");
ConVar asw_local_dlight_offsety("asw_local_dlight_offsety", "-15", FCVAR_CHEAT, "Offset of the local local");
ConVar asw_local_dlight_offsetz("asw_local_dlight_offsetz", "95", FCVAR_CHEAT, "Offset of the flashlight dlight");
ConVar asw_local_dlight_r("asw_local_dlight_r", "250", FCVAR_CHEAT, "Red component of local colour");
ConVar asw_local_dlight_g("asw_local_dlight_g", "250", FCVAR_CHEAT, "Green component of local colour");
ConVar asw_local_dlight_b("asw_local_dlight_b", "250", FCVAR_CHEAT, "Blue component of local colour");
ConVar asw_local_dlight_exponent("asw_local_dlight_exponent", "3", FCVAR_CHEAT, "Exponent of local colour");
ConVar asw_local_dlight_rotate("asw_local_dlight_rotate", "0", FCVAR_CHEAT, "Whether local dlight's offset is rotated by marine facing");


void C_ASW_Player::UpdateLocalMarineGlow()
{
	C_ASW_Marine *pMarine = GetMarine();
	if ( !pMarine )
	{
		pMarine = GetSpectatingMarine();		
	}

	if ( !pMarine )
	{
		if ( m_pLocalMarineGlow )
		{
			m_pLocalMarineGlow->die = gpGlobals->curtime + 0.001;
			m_pLocalMarineGlow = NULL;
		}
	}
	else if ( asw_local_dlight_radius.GetFloat() > 0 )
	{
		if ( !m_pLocalMarineGlow )
		{
			m_pLocalMarineGlow = effects->CL_AllocDlight ( index );
		}

		if ( m_pLocalMarineGlow )
		{
			if ( asw_local_dlight_rotate.GetBool() )
			{
				Vector vecForward, vecRight, vecUp;
				AngleVectors( pMarine->GetLocalAngles(), &vecForward, &vecRight, &vecUp );
				m_pLocalMarineGlow->origin = pMarine->GetAbsOrigin() + vecForward * asw_local_dlight_offsetx.GetFloat()
					+ vecRight * asw_local_dlight_offsety.GetFloat() + vecUp * asw_local_dlight_offsetz.GetFloat();			
			}
			else
			{
				m_pLocalMarineGlow->origin = pMarine->GetAbsOrigin();
				m_pLocalMarineGlow->origin.x += asw_local_dlight_offsetx.GetFloat();
				m_pLocalMarineGlow->origin.y += asw_local_dlight_offsety.GetFloat();
				m_pLocalMarineGlow->origin.z += asw_local_dlight_offsetz.GetFloat();
			}
			m_pLocalMarineGlow->color.r = asw_local_dlight_r.GetInt();
			m_pLocalMarineGlow->color.g = asw_local_dlight_g.GetInt();
			m_pLocalMarineGlow->color.b = asw_local_dlight_b.GetInt();
			m_pLocalMarineGlow->radius = asw_local_dlight_radius.GetFloat();
			m_pLocalMarineGlow->color.exponent = asw_local_dlight_exponent.GetInt();
			m_pLocalMarineGlow->die = gpGlobals->curtime + 30.0f;
		}
	}
}

#ifdef PLATFORM_WINDOWS_PC
// Instead of including windows.h
extern "C"
{
	extern int __stdcall CopyFileA( char *pszSource, char *pszDest, int bFailIfExists );
};

namespace
{	
	// hacky function from mp3player to copy the custom sound file into the game folder so the filesystem can access it
	void GetLocalCopyOfStimMusic( const char *filename, char *outsong, size_t outlen )
	{
		char fn[ 512 ];
		Q_snprintf( fn, sizeof( fn ), "%s", filename );

		outsong[ 0 ] = 0;

		// Get temp filename from crc
		CRC32_t crc;
		CRC32_Init( &crc );
		CRC32_ProcessBuffer( &crc, fn, Q_strlen( fn ) );
		CRC32_Final( &crc );

		char hexname[ 16 ];
		Q_binarytohex( (const byte *)&crc, sizeof( crc ), hexname, sizeof( hexname ) );

		char hexfilename[ 512 ];
		Q_snprintf( hexfilename, sizeof( hexfilename ), "sound/_mp3/%s.mp3", hexname );

		Q_FixSlashes( hexfilename );

		if ( g_pFullFileSystem->FileExists( hexfilename, "MOD" ) )
		{
			Q_snprintf( outsong, outlen, "_mp3/%s.mp3", hexname );
		}
		else
		{
			// Make a local copy
			char mp3_temp_path[ 512 ];
			Q_snprintf( mp3_temp_path, sizeof( mp3_temp_path ), "sound/_mp3" );
			g_pFullFileSystem->CreateDirHierarchy( mp3_temp_path, "MOD" );

			char destpath[ 512 ];
			Q_snprintf( destpath, sizeof( destpath ), "%s/%s", engine->GetGameDirectory(), hexfilename );
			Q_FixSlashes( destpath );

			// !!!HACK HACK:
			// Total hack right now, using windows OS calls to copy file to full destination
			int success = ::CopyFileA( fn, destpath, TRUE );
			if ( success > 0 )
			{
				Q_snprintf( outsong, outlen, "_mp3/%s.mp3", hexname );
			}
		}

		Q_FixSlashes( outsong );
	}
}
#else
static void GetLocalCopyOfStimMusic( const char *filename, char *outsong, size_t outlen ) {};
#pragma message("TODO: GetLocalCopyOfStimMusic is a hack that must go away!")
#endif

void C_ASW_Player::StartStimMusic()
{
	if (ASWGameRules() && !ASWGameRules()->ShouldPlayStimMusic())
		return;

	CLocalPlayerFilter filter;	
	if (!m_pStimMusic)		
	{
		// Check if custom combat music is playing
		if( g_ASWJukebox.IsMusicPlaying() )
			return;

		if ( Q_strlen( asw_stim_music.GetString() ) > 0 )
		{
			char soundname[ 512 ];
			soundname[0] = '#';
			soundname[1] = 0;
			
			GetLocalCopyOfStimMusic( asw_stim_music.GetString(), &soundname[1], sizeof( soundname ) - 1 );
			if ( soundname[ 1 ] )
			{
				m_pStimMusic = CSoundEnvelopeController::GetController().SoundCreate( filter, 0, CHAN_STATIC, soundname, SNDLVL_NONE );
			}
		}
		
		if ( !m_pStimMusic )
		{
			m_pStimMusic = CSoundEnvelopeController::GetController().SoundCreate( filter, 0, "asw_song.stims" );
		}

		if (m_pStimMusic)
		{
			CSoundEnvelopeController::GetController().Play( m_pStimMusic, 1.0, 100 );			
		}
	}
	else
	{
		// already playing the stim music, make sure it's at full volume
		CSoundEnvelopeController::GetController().SoundChangeVolume( m_pStimMusic, 1.0, 0.3f );
	}

	m_bStartedStimMusic = true;
}

void C_ASW_Player::StopStimMusic(bool bInstantly)
{
	if (m_pStimMusic)
	{
		if (bInstantly)
		{
			CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();
			controller.SoundDestroy( m_pStimMusic );
			m_pStimMusic = NULL;
		}
		else
		{
			CSoundEnvelopeController::GetController().SoundChangeVolume( m_pStimMusic, 0.0, 1.0f );
		}
	}
	m_bStartedStimMusic = false;
}

void C_ASW_Player::ClearStimMusic()
{
	if ( m_pStimMusic )
	{
		CSoundEnvelopeController::GetController().CommandAdd( m_pStimMusic, 1.0f, SOUNDCTRL_DESTROY, 0.0f, 0.0f );
		m_pStimMusic = NULL;
	}
}

void C_ASW_Player::UpdateOnRemove()
{
	BaseClass::UpdateOnRemove();
	StopStimMusic();
	ClearStimMusic();
	if (m_bPlayingSingleBreathSound)
	{
		StopStimSound();
	}
}

fogparams_t* C_ASW_Player::GetFogParams( void )
{
	static fogparams_t RemoteTurretFog;
	if (GetMarine() && GetMarine()->IsControllingTurret())
	{
		RemoteTurretFog.enable = true;
		RemoteTurretFog.colorPrimary.SetR(0);
		RemoteTurretFog.colorPrimary.SetG(0);
		RemoteTurretFog.colorPrimary.SetB(0);
		RemoteTurretFog.colorPrimary.SetA(255);
		RemoteTurretFog.colorSecondary = RemoteTurretFog.colorPrimary;
		RemoteTurretFog.start = asw_turret_fog_start.GetFloat();
		RemoteTurretFog.end = asw_turret_fog_end.GetFloat();
		RemoteTurretFog.farz = asw_turret_fog_end.GetFloat();
		return &RemoteTurretFog;
	}
	
	return &m_CurrentFog;
}

// used by the intro to launch the campaign map when the intro finishes
void __MsgFunc_LaunchCampaignMap( bf_read &msg )
{
	char mapName[255];
	Q_FileBase( engine->GetLevelName(), mapName, sizeof(mapName) );	
	// check if this is the intro map, if so then launch the campaign page
	if ( !Q_strnicmp( mapName, "intro", 5 ) )
	{
		C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
		if (pPlayer)
		{			
			pPlayer->LaunchCampaignFrame();
		}
	}
}
USER_MESSAGE_REGISTER( LaunchCampaignMap );

void C_ASW_Player::LaunchCredits( vgui::Panel *pParent /*= NULL*/ )
{
	if ( !pParent )
	{
		pParent = GetClientMode()->GetViewport();
	}

	CreditsPanel *pPanel = new CreditsPanel( pParent, "Credits" );
	vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFile("resource/SwarmSchemeNew.res", "SwarmSchemeNew");
	pPanel->SetScheme(scheme);
	pPanel->SetZPos( 200 );
	//pPanel->MakePopup();
	//pPanel->MoveToFront();
}

void C_ASW_Player::LaunchCainMail()
{
	CainMailPanel *pPanel = new CainMailPanel( GetClientMode()->GetViewport(), "Credits" );
	vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFile("resource/SwarmSchemeNew.res", "SwarmSchemeNew");
	pPanel->SetScheme(scheme);
	vgui::IScheme *pScheme = vgui::scheme()->GetIScheme( scheme );
	pPanel->StartFadeIn(pScheme);
}

void __MsgFunc_LaunchCredits( bf_read &msg )
{	
	Msg("__MsgFunc_LaunchCredits\n");
	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if (pPlayer)
	{			
		Msg("  so launching credits\n");
		pPlayer->LaunchCredits();
	}	
}
USER_MESSAGE_REGISTER( LaunchCredits );

void __MsgFunc_LaunchCainMail( bf_read &msg )
{	
	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if (pPlayer)
	{			
		pPlayer->LaunchCainMail();
	}	
}
USER_MESSAGE_REGISTER( LaunchCainMail );

bool C_ASW_Player::ShouldRegenerateOriginFromCellBits() const
{
	return true;
}

bool C_ASW_Player::IsSniperScopeActive()
{
	C_ASW_Marine *pMarine = GetMarine();
	if ( !pMarine )
		return false;

	C_BaseCombatWeapon*	pWeapon = pMarine->GetActiveWeapon();
	if ( pWeapon && pWeapon->Classify() == CLASS_ASW_SNIPER_RIFLE )
	{
		C_ASW_Weapon_Sniper_Rifle *pSniper = assert_cast<C_ASW_Weapon_Sniper_Rifle*>( pWeapon );
		return pSniper->IsZoomed();
	}
	return false;
}

CSteamID C_ASW_Player::GetSteamID()
{
	int iIndex = entindex();
	player_info_t pi;
	if ( engine->GetPlayerInfo(iIndex, &pi) )
	{
		if ( pi.friendsID )
		{
			CSteamID steamIDForPlayer( pi.friendsID, 1, steamapicontext->SteamUtils()->GetConnectedUniverse(), k_EAccountTypeIndividual );
			return steamIDForPlayer;
		}
	}
	CSteamID invalid;
	return invalid;
}