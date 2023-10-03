#include "cbase.h"
#include "input.h"
#include "c_asw_player.h"
#include "c_asw_marine_resource.h"
#include "c_asw_game_resource.h"
#include "c_asw_marine.h"
#include "c_asw_weapon.h"
#include "c_asw_objective.h"
#include "asw_marine_profile.h"
#include "c_asw_generic_emitter.h"
#include "c_asw_generic_emitter_entity.h"
#include "clientmode_asw.h"
#include "asw_vgui_edit_emitter.h"
#include "engine/IEngineSound.h"
#include "c_asw_jeep_clientside.h"
#include "vgui\asw_hud_minimap.h"
#include "asw_vgui_manipulator.h"
#include "c_asw_camera_volume.h"
#include "c_asw_mesh_emitter_entity.h"
#include "MedalCollectionPanel.h"
#include "PlayerListPanel.h"
#include "PlayerListContainer.h"
#include "vgui\nb_mission_panel.h"
#ifndef _X360
#include "steam/isteamuserstats.h"
#include "steam/isteamfriends.h"
#include "steam/isteamutils.h"
#include "steam/steam_api.h"
#include "matchmaking/imatchframework.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar cam_idealdist;
extern ConVar cam_idealpitch;
extern ConVar cam_idealyaw;

extern vgui::DHANDLE<vgui::Frame> g_hBriefingFrame;

// displays the mission objectives in the game resource
void ListObjectives(void)
{
	C_ASW_Game_Resource *pGameResource = ASWGameResource();
	if ( !pGameResource )
		return;

	for (int i=0;i<12;i++)
	{
		if ( pGameResource->GetObjective(i) == NULL )
			Msg("Objective %d = empty\n", i);
	}
}

static ConCommand listobjectives("listobjectives", ListObjectives, "Shows names of objectives in the objectives array", FCVAR_CHEAT);

void ListMarineResources(void)
{
	C_ASW_Game_Resource *pGameResource = ASWGameResource();
	if ( !pGameResource )
		return;

	for (int i=0;i<pGameResource->GetMaxMarineResources();i++)
	{
		if (pGameResource->GetMarineResource(i) == NULL)
			Msg("MarineResource %d = empty\n", i);
		else
		{
			Msg("MarineResource %d = present, profileindex %d, commander %d commander index %d\n",
				i, pGameResource->GetMarineResource(i)->m_MarineProfileIndex,
				pGameResource->GetMarineResource(i)->GetCommander(),
				pGameResource->GetMarineResource(i)->GetCommanderIndex());
		}
	}
}

static ConCommand listmarineresources("listmarineresources", ListMarineResources, "Shows contents of the marine resource array", FCVAR_CHEAT);

void listroster_f(void)
{
	C_ASW_Game_Resource *pGameResource = ASWGameResource();
	if (!pGameResource)
		return;

	for (int i=0;i<ASW_NUM_MARINE_PROFILES;i++)
	{
		Msg("[C] Roster %d selected=%d\n", i, pGameResource->IsRosterSelected(i));
	}
}

static ConCommand listroster("listroster", listroster_f, "Shows which marines in the roster are selected", FCVAR_CHEAT);

void asw_credits_f()
{
	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if (pPlayer)
	{
		pPlayer->LaunchCredits();
	}
}
static ConCommand asw_credits("asw_credits", asw_credits_f, "Test shows credits", FCVAR_CHEAT);


void asw_cain_mail_f()
{
	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if (pPlayer)
	{
		pPlayer->LaunchCainMail();
	}
}
static ConCommand asw_cain_mail("asw_cain_mail", asw_cain_mail_f, "Test shows cain mail", FCVAR_CHEAT);

// lists a few basic details about a marine's profile
void ASW_InspectProfile( const CCommand &args )
{	
	int i = atoi( args[1] );
	Msg("Marine profile %d\n", i);

	CASW_Marine_Profile *profile = MarineProfileList()->m_Profiles[i];
	if (profile != NULL)
	{
		Msg("Name: %s\n", profile->m_ShortName);
		Msg("Age: %d\n",
			profile->m_Age);
		if (profile->GetMarineClass() == MARINE_CLASS_TECH)
			Msg("Tech\n");
		if (profile->GetMarineClass() == MARINE_CLASS_MEDIC)
			Msg("First Aid\n");
		if (profile->GetMarineClass() == MARINE_CLASS_SPECIAL_WEAPONS)
			Msg("Special Weapons\n");
		if (profile->GetMarineClass() == MARINE_CLASS_NCO)
			Msg("Sapper\n");
	}
}

static ConCommand asw_inspect_profile("asw_inspect_profile", ASW_InspectProfile, "Display a marine's profile", FCVAR_CHEAT);


void CC_ASWEditEmitterFrame(void)
{
	using namespace vgui;

	// find the asw player
	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if (!pPlayer)
		return;

	// fine the nearest emitter
	float distance = 0;
	float best_distance = 0;
	C_ASW_Emitter* pEmitter = NULL;
	C_ASW_Emitter* pTemp = NULL;

	unsigned int c = ClientEntityList().GetHighestEntityIndex();
	for ( unsigned int i = 0; i <= c; i++ )
	{
		C_BaseEntity *e = ClientEntityList().GetBaseEntity( i );
		if ( !e )
			continue;

		pTemp = dynamic_cast<C_ASW_Emitter*>(e);
		if (pTemp)
		{
			distance = pTemp->GetAbsOrigin().DistTo(pPlayer->GetAbsOrigin());
			if (best_distance == 0 || distance < best_distance)
			{
				best_distance = distance;
				pEmitter = pTemp;
			}
		}
	}

	if (pEmitter == NULL)
	{
		Msg("Couldn't find any asw_emitter to edit\n");
		return;
	}

	// create the basic frame which holds our briefing panels
	CASW_VGUI_Edit_Emitter* pEditEmitterFrame = new CASW_VGUI_Edit_Emitter( GetClientMode()->GetViewport(), "EditEmitterFrame" );
	HScheme scheme = vgui::scheme()->LoadSchemeFromFile("resource/SwarmSchemeNew.res", "SwarmSchemeNew");
	pEditEmitterFrame->SetScheme(scheme);	
	pEditEmitterFrame->Activate();// set visible, move to front, request focus	
	pEditEmitterFrame->SetEmitter((C_ASW_Emitter*) pEmitter);
	pEditEmitterFrame->InitFrom((C_ASW_Emitter*) pEmitter);
}

static ConCommand ASW_EditEmitterFrame("ASW_EditEmitterFrame", CC_ASWEditEmitterFrame, "The vgui panel used to edit emitters", FCVAR_CHEAT);


void ASW_MessageLog_f(void)
{
	// find the asw player
	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if (pPlayer)
		pPlayer->ShowMessageLog();
}
static ConCommand ASW_MessageLog("ASW_MessageLog", ASW_MessageLog_f, "Shows a log of info messages you've read so far in this mission", 0);


void asw_weapon_switch_f()
{
	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if ( !pPlayer )
		return;

	C_ASW_Marine *pMarine = pPlayer->GetMarine();
	if ( !pMarine )
		return;

	C_BaseCombatWeapon *pCurrent = pMarine->GetActiveWeapon();
	C_BaseCombatWeapon *pPrimary = pMarine->GetWeapon( ASW_INVENTORY_SLOT_PRIMARY );
	if ( pCurrent != pPrimary && pPrimary )
	{
		::input->MakeWeaponSelection( pPrimary );
	}
	C_BaseCombatWeapon *pSecondary = pMarine->GetWeapon( ASW_INVENTORY_SLOT_SECONDARY );
	if ( pCurrent != pSecondary && pSecondary )
	{
		::input->MakeWeaponSelection( pSecondary );
	}
}
ConCommand ASW_InvLast( "ASW_InvLast", asw_weapon_switch_f, "Switches between primary and secondary weapons", 0 );
ConCommand ASW_InvNext( "ASW_InvNext", asw_weapon_switch_f, "Makes your marine select the next weapon", 0 );
ConCommand ASW_InvPrev( "ASW_InvPrev", asw_weapon_switch_f, "Makes your marine select the previous weapon", 0 );

// Binds for activating primary/secondary/extra items

void ASW_ActivatePrimary_f()
{
	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();

	if (pPlayer && pPlayer->GetMarine())
	{
		pPlayer->ActivateInventoryItem(0);
	}
}
void ASW_ActivateSecondary_f()
{
	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();

	if (pPlayer && pPlayer->GetMarine())
	{
		pPlayer->ActivateInventoryItem(1);
	}
}
void ASW_ActivateExtra_f()
{
	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();

	if (pPlayer && pPlayer->GetMarine())
	{
		pPlayer->ActivateInventoryItem(2);
	}
}
ConCommand ASW_ActivatePrimary( "ASW_ActivatePrimary", ASW_ActivatePrimary_f, "Activates the item in your primary inventory slot", 0 );
ConCommand ASW_ActivateSecondary( "ASW_ActivateSecondary", ASW_ActivateSecondary_f, "Activates the item in your secondary inventory slot", 0 );
ConCommand ASW_ActivateExtra( "ASW_ActivateExtra", ASW_ActivateExtra_f, "Activates the item in your extra inventory slot", 0 );

C_ASW_PropJeep_Clientside* g_pJeep = NULL;
void asw_make_jeep_f()
{
	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if (pPlayer)
	{
		C_ASW_PropJeep_Clientside* pJeep = C_ASW_PropJeep_Clientside::CreateNew(false);
		pJeep->SetAbsOrigin(pPlayer->GetAbsOrigin());
		pJeep->Initialize();
		g_pJeep = pJeep;
		// need to set player?
	}
}
ConCommand asw_make_jeep( "asw_make_jeep", asw_make_jeep_f, "Creates a clientside jeep", FCVAR_CHEAT );


void asw_make_jeep_phys_f()
{
	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if (pPlayer && g_pJeep)
	{
		g_pJeep->InitPhysics();
	}
}
ConCommand asw_make_jeep_phys( "asw_make_jeep_phys", asw_make_jeep_phys_f, "Creates physics for test clientside jeep", FCVAR_CHEAT );

/*
void asw_snow_test_f()
{
	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if (pPlayer && pPlayer->GetMarine())
	{
		int i = atoi( args[1] );
		if (i == 0)
		{
			pPlayer->GetMarine()->m_hSnowEmitter->m_bLocalCoordSpace = false;
			pPlayer->GetMarine()->m_hSnowEmitter->m_bWrapParticlesToSpawnBounds = false;
		}
		else if (i == 1)
		{
			pPlayer->GetMarine()->m_hSnowEmitter->m_bLocalCoordSpace = true;
			pPlayer->GetMarine()->m_hSnowEmitter->m_bWrapParticlesToSpawnBounds = false;
		}
		else if (i == 2)
		{
			pPlayer->GetMarine()->m_hSnowEmitter->m_bLocalCoordSpace = true;
			pPlayer->GetMarine()->m_hSnowEmitter->m_bWrapParticlesToSpawnBounds = true;
		}
		else if (i == 3)
		{
			pPlayer->GetMarine()->m_hSnowEmitter->m_bLocalCoordSpace = false;
			pPlayer->GetMarine()->m_hSnowEmitter->m_bWrapParticlesToSpawnBounds = true;
		}
	}
}
ConCommand asw_snow_test( "asw_snow_test", asw_snow_test_f, "Changes snow emitter state", FCVAR_CHEAT );
*/




void asw_minimap_scale_f( const CCommand &args )
{
	CASWHudMinimap *pMiniMap = GET_HUDELEMENT(CASWHudMinimap);
	if (!pMiniMap)
		return;

	if (args.ArgC() == 2)
	{		
		pMiniMap->m_fMapScale = atof( args[1] );
		Msg("Set minimap scale to %f\n", pMiniMap->m_fMapScale);
	}
}
ConCommand asw_minimap_scale( "asw_minimap_scale", asw_minimap_scale_f, "Overrides scale of the minimap", FCVAR_CHEAT );

void asw_entindex_f()
{
	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if (pPlayer)
	{
		Msg("Hidehud is %d\n", pPlayer->m_Local.m_iHideHUD);		
		Msg("Local player entity index is %d\n", pPlayer->entindex());
		if (pPlayer->GetMarine())
		{
			Msg("  and your current marine's entity index is %d\n", pPlayer->GetMarine()->entindex());
			if (pPlayer->GetMarine()->GetMarineResource())
			{
				Msg("    and your current marine's marine info's entity index is %d\n", pPlayer->GetMarine()->GetMarineResource()->entindex());
			}
			else
			{
				Msg("    and your current marine has no marine info\n");
			}
		}
		else
		{
			Msg("  and you have no current marine\n");
		}
	}
	else
	{
		Msg("No local player!\n");
	}
}
ConCommand asw_entindex( "asw_entindex", asw_entindex_f, "Returns the entity index of the player", FCVAR_CHEAT );

/*
void asw_campaign_test_f()
{
	if (!ASWGameRules())
		return;

	C_ASW_Game_Resource* pGameResource = ASWGameRules()->ASWGameResource();
	if (!pGameResource)
		return;

	Msg("Making test campaign...\n");
	ASWGameRules()->m_pCampaignInfo = new CASW_Campaign_Info;
	if (ASWGameRules()->m_pCampaignInfo)
	{
		Msg("Loading jacob campaign into it\n");
		ASWGameRules()->m_pCampaignInfo->LoadCampaign("jacob");
		int num_missions = ASWGameRules()->m_pCampaignInfo->GetNumMissions();
		Msg("  Num Missions = %d\n", num_missions);
		for (int i=0;i<num_missions;i++)
		{
			Msg("Mission %d name is %s\n", i, ASWGameRules()->m_pCampaignInfo->GetMission(i)->m_MissionName);
		}
	}
}
ConCommand asw_campaign_test( "asw_campaign_test", asw_campaign_test_f, "Campaign code test function", FCVAR_CHEAT );

void asw_campaign_panel_f()
{
	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if (pPlayer)
		pPlayer->LaunchCampaignFrame();
}
ConCommand asw_campaign_panel( "asw_campaign_panel", asw_campaign_panel_f, "Campaign panel test function", FCVAR_CHEAT );
*/

void asw_edit_panel_f( const CCommand &args )
{
	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if (!pPlayer)
		return;
	if (!GetClientMode()->GetViewport())
	{
		Msg("No viewport!\n");
		return;
	}
	GetClientMode()->GetViewport();
	vgui::Panel *pPanel = GetClientMode()->GetViewport()->FindChildByName(args[1], true);
	if (pPanel)
	{
		vgui::Panel *pParent = GetClientModeASW()->m_hCampaignFrame.Get();
		if (!pParent)
			pParent = GetClientModeASW()->GetViewport();
		CASW_VGUI_Manipulator::EditPanel( pParent, pPanel );
	}
	else
	{
		CASW_VGUI_Manipulator::EditPanel(NULL, NULL);
		Msg("No panel found with that name in viewport! Clearing manipulator.\n");
	}
}
ConCommand asw_edit_panel( "asw_edit_panel", asw_edit_panel_f, "ASW Edit a VGUI panel by name", FCVAR_CHEAT );
void asw_marine_update_visibility_f()
{
	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if (pPlayer && pPlayer->GetMarine())
	{
		pPlayer->GetMarine()->UpdateVisibility();
	}
}
ConCommand asw_marine_update_visibility( "asw_marine_update_visibility", asw_marine_update_visibility_f, "Updates marine visibility", FCVAR_CHEAT );

void asw_camera_volume_f()
{
	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if (pPlayer && pPlayer->GetMarine())
	{
		Msg("Marine inside camera volume = %d\n", C_ASW_Camera_Volume::IsPointInCameraVolume(pPlayer->GetMarine()->GetAbsOrigin()));
	}
}
ConCommand asw_camera_volume( "asw_camera_volume", asw_camera_volume_f, "check if the marine is inside an asw_camera_control volume", FCVAR_CHEAT );

void asw_camera_report_defaults_f()
{
	QAngle default_ang(cam_idealpitch.GetFloat(),cam_idealyaw.GetFloat(),0);
	Vector default_dir;
	AngleVectors(default_ang, &default_dir);
	Msg("Default dir: %f, %f, %f\n", default_dir.x, default_dir.y, default_dir.z);
	default_dir *= -cam_idealdist.GetFloat();
	Msg("Default offset: %f, %f, %f\n", default_dir.x, default_dir.y, default_dir.z);
	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if (pPlayer)
	{
		Msg("RTS cam is at: %f, %f, %f\n", VectorExpand( pPlayer->GetAbsOrigin() ) );
	}
}
ConCommand asw_camera_report_defaults( "asw_camera_report_defaults", asw_camera_report_defaults_f, "Report default vectors on the camera based on current settings", FCVAR_CHEAT );



void asw_mesh_emitter_test_f()
{
	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if (pPlayer && pPlayer->GetMarine())
	{
		C_ASW_Marine *pMarine = pPlayer->GetMarine();
		C_ASW_Mesh_Emitter *pEmitter = new C_ASW_Mesh_Emitter;
		if (pEmitter)
		{
			if (pEmitter->InitializeAsClientEntity( "models/swarm/DroneGibs/dronepart01.mdl", false ))
			{
				Vector vecForward;
				AngleVectors(pMarine->GetAbsAngles(), &vecForward);
				Vector vecEmitterPos = pMarine->GetAbsOrigin() + vecForward * 200.0f;
				Q_snprintf(pEmitter->m_szTemplateName, sizeof(pEmitter->m_szTemplateName), "dronegiblots");
				pEmitter->m_fScale = 1.0f;
				pEmitter->m_bEmit = true;
				pEmitter->SetAbsOrigin(vecEmitterPos);
				pEmitter->CreateEmitter(vec3_origin);
				pEmitter->SetAbsOrigin(vecEmitterPos);
				pEmitter->SetDieTime(gpGlobals->curtime + 15.0f);
			}
			else
			{
				pEmitter->Release();
			}
		}
	}
}
ConCommand asw_mesh_emitter_test( "asw_mesh_emitter_test", asw_mesh_emitter_test_f, "Test spawning a clientside mesh emitter", FCVAR_CHEAT );


void ShowPlayerList()
{
	if ( gpGlobals->maxClients <= 1 )
		return;

	using namespace vgui;

	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if (!pPlayer)
		return;

	if (engine->IsLevelMainMenuBackground())		// don't show player list on main menu
	{		
		return;
	}
	vgui::Panel *pContainer = GetClientMode()->GetViewport()->FindChildByName("g_PlayerListFrame", true);
	if (pContainer)	
	{
		pContainer->SetVisible(false);
		pContainer->MarkForDeletion();
		pContainer = NULL;
		return;
	}

	vgui::Frame* pFrame = NULL;

	if (g_hBriefingFrame.Get())
		pContainer = new PlayerListContainer( g_hBriefingFrame.Get(), "g_PlayerListFrame" );
	else
	{
		if (GetClientModeASW()->m_hCampaignFrame.Get())
		{
			pContainer = new PlayerListContainer( GetClientModeASW()->m_hCampaignFrame.Get(), "g_PlayerListFrame" );
		}
		else
		{
			if (GetClientModeASW()->m_hMissionCompleteFrame.Get())
			{
				pContainer = new PlayerListContainer( GetClientModeASW()->m_hMissionCompleteFrame.Get(), "g_PlayerListFrame" );
			}
			else
			{
				pFrame = new PlayerListContainerFrame( GetClientMode()->GetViewport(), "g_PlayerListFrame" );
				pContainer = pFrame;
			}
		}
	}
	HScheme scheme = vgui::scheme()->LoadSchemeFromFile("resource/SwarmSchemeNew.res", "SwarmSchemeNew");
	pContainer->SetScheme(scheme);		

	// the panel to show the info
	PlayerListPanel *playerlistpanel = new PlayerListPanel( pContainer, "PlayerListPanel" );
	playerlistpanel->SetVisible( true );

	if (!pContainer)
	{
		Msg("Error: Player list pContainer frame was closed immediately on opening\n");
	}
	else
	{
		pContainer->RequestFocus();
		pContainer->SetVisible(true);
		pContainer->SetEnabled(true);
		pContainer->SetKeyBoardInputEnabled(false);
		pContainer->SetZPos(200);		
	}
}

static ConCommand playerlist("playerlist", ShowPlayerList, "Shows the player list and allows voting", 0);

void ShowInGameBriefing()
{
	using namespace vgui;

	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if (!pPlayer)
		return;

	if (engine->IsLevelMainMenuBackground())		// don't show player list on main menu
	{		
		return;
	}
	vgui::Panel *pContainer = GetClientMode()->GetViewport()->FindChildByName("InGameBriefingContainer", true);
	if (pContainer)	
	{
		pContainer->SetVisible(false);
		pContainer->MarkForDeletion();
		pContainer = NULL;
		return;
	}

	if (g_hBriefingFrame.Get() || GetClientModeASW()->m_hCampaignFrame.Get() || GetClientModeASW()->m_hMissionCompleteFrame.Get())
	{
		return;
	}

	vgui::Frame* pFrame = new InGameMissionPanelFrame( GetClientMode()->GetViewport(), "InGameBriefingContainer" );
	HScheme scheme = vgui::scheme()->LoadSchemeFromFile("resource/SwarmSchemeNew.res", "SwarmSchemeNew");
	pFrame->SetScheme(scheme);		

	// the panel to show the info
	CNB_Mission_Panel *missionpanel = new CNB_Mission_Panel( pFrame, "MissionPanel" );
	missionpanel->SetVisible( true );	

	if (!pFrame)
	{
		Msg("Error: ingame briefing frame was closed immediately on opening\n");
	}
	else
	{
		pFrame->RequestFocus();
		pFrame->SetVisible(true);
		pFrame->SetEnabled(true);
		pFrame->SetKeyBoardInputEnabled(false);
		pFrame->SetZPos(200);		
		GetClientModeASW()->m_hInGameBriefingFrame = pFrame;
	}
}

static ConCommand ingamebriefing("ingamebriefing", ShowInGameBriefing, "Shows the mission briefing panel", 0);


void ShowMedalCollection()
{
	using namespace vgui;

	vgui::Panel *pMedalPanel = GetClientMode()->GetViewport()->FindChildByName("MedalCollectionPanel", true);
	if (pMedalPanel)	
	{
		pMedalPanel->SetVisible(false);
		pMedalPanel->MarkForDeletion();
		pMedalPanel = NULL;
		return;
	}

	vgui::Frame* pFrame = NULL;
	// create the basic frame which holds our briefing panels
	//Msg("Assigning player list frame\n");
	if (g_hBriefingFrame.Get())	// todo: handle if they bring it up during debrief or campaign map too
		pMedalPanel = new vgui::Panel( g_hBriefingFrame.Get(), "MedalCollectionPanel" );
	else
	{
		pFrame = new vgui::Frame( GetClientMode()->GetViewport(), "MedalCollectionPanel" );
		pMedalPanel = pFrame;
	}
	HScheme scheme = vgui::scheme()->LoadSchemeFromFile("resource/SwarmSchemeNew.res", "SwarmSchemeNew");
	pMedalPanel->SetScheme(scheme);
	pMedalPanel->SetBounds(0, 0, GetClientMode()->GetViewport()->GetWide(), GetClientMode()->GetViewport()->GetTall());
	//pMedalPanel->SetPos(GetClientMode()->GetViewport()->GetWide() * 0.15f, GetClientMode()->GetViewport()->GetTall() * 0.15f);
	//pMedalPanel->SetSize( GetClientMode()->GetViewport()->GetWide() * 0.7f, GetClientMode()->GetViewport()->GetTall() * 0.7f );

	if (pFrame)
	{		
		pFrame->SetMoveable(false);
		pFrame->SetSizeable(false);
		pFrame->SetMenuButtonVisible(false);
		pFrame->SetMaximizeButtonVisible(false);
		pFrame->SetMinimizeToSysTrayButtonVisible(false);
		pFrame->SetCloseButtonVisible(true);
		pFrame->SetTitleBarVisible(false);
	}
	pMedalPanel->SetPaintBackgroundEnabled(false);
	pMedalPanel->SetBgColor(Color(0,0,0, 192));

	// the panel to show the info
	MedalCollectionPanel *collection = new MedalCollectionPanel( pMedalPanel, "Collection" );
	collection->SetVisible( true );
	collection->SetBounds(0, 0, pMedalPanel->GetWide(),pMedalPanel->GetTall());

	if (!pMedalPanel)
	{
		Msg("Error: pMedalPanel frame was closed immediately on opening\n");
	}
	else
	{
		pMedalPanel->RequestFocus();
		pMedalPanel->SetVisible(true);
		pMedalPanel->SetEnabled(true);
		pMedalPanel->SetKeyBoardInputEnabled(false);
		pMedalPanel->SetZPos(200);		
	}
}

//static ConCommand asw_medals("asw_medals", ShowMedalCollection, "Shows the players medal collection", FCVAR_CHEAT);



void asw_list_sounds_f()
{
	Msg("listing all sounds\n");

	CUtlVector< SndInfo_t > sndlist;
	enginesound->GetActiveSounds(sndlist);
	for (int i=0;i<sndlist.Count();i++)
	{
		//SndInfo_t& sound = sndlist[i];
		//Msg("sound %d: %s\n", i, sound.m);
		Msg("sound %d\n", i);
	}
}
ConCommand asw_sounds("asw_sounds", asw_list_sounds_f, "lists sounds playing", 0);

void asw_test_music_f()
{
	Msg("listing all active sounds:\n");

	CUtlVector< SndInfo_t > sndlist;
	enginesound->GetActiveSounds(sndlist);
	for (int i=0;i<sndlist.Count();i++)
	{
		//SndInfo_t& sound = sndlist[i];
		//Msg("sound %d: %s\n", i, sound.m_pszName);
		Msg("sound %d\n", i);
	}
	if (GetClientModeASW())
	{
		Msg("Briefing music pointer is: %d\n", GetClientModeASW()->m_pBriefingMusic);
		if (GetClientModeASW()->m_pBriefingMusic)
		{
			Msg("asw_test_music_f calling StopBriefingMusic\n");
			GetClientModeASW()->StopBriefingMusic();
		}
	}
	else
	{
		Msg("No local player\n");
	}
}
ConCommand asw_test_music("asw_test_music", asw_test_music_f, "lists music pointer", 0);

void asw_debug_spectator_f()
{
	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if (pPlayer)
	{
		Msg("Clientside spectator flag is %d\n", GetClientModeASW() ? GetClientModeASW()->m_bSpectator : 0);
		engine->ClientCmd("asw_debug_spectator_server");
	}
}
ConCommand asw_debug_spectator( "asw_debug_spectator", asw_debug_spectator_f, "Prints info on spectator", FCVAR_CHEAT );

// TODO: Remove this before ship?
void reset_steam_stats_f()
{
	Assert( steamapicontext->SteamUserStats() );
	if ( !steamapicontext->SteamUserStats() )
		return;

	steamapicontext->SteamUserStats()->ResetAllStats( false );

	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if ( pPlayer )
	{
		pPlayer->RequestExperience();
	}
}
ConCommand reset_steam_stats( "reset_steam_stats", reset_steam_stats_f, "Resets steam stats (experience, etc.)", FCVAR_DEVELOPMENTONLY );



void asw_show_xp_f()
{
	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if ( pPlayer )
	{
		Msg( "pPlayer->GetLevel() = %d\n", pPlayer->GetLevel() );
		Msg( "pPlayer->GetExperience() = %d\n", pPlayer->GetExperience() );
		Msg( "pPlayer->GetExperienceBeforeDebrief() = %d\n", pPlayer->GetExperienceBeforeDebrief() );
	}
}
ConCommand asw_show_xp( "asw_show_xp", asw_show_xp_f, "Print local player's XP and level", FCVAR_NONE );

CON_COMMAND( make_game_public, "Changes access for the current game to public." )
{
	if ( !g_pMatchFramework || !g_pMatchFramework->GetMatchSession() )
		return;

	if ( !ASWGameResource() || ASWGameResource()->GetLeader() != C_ASW_Player::GetLocalASWPlayer() )
		return;

	KeyValues *pSettings = new KeyValues( "update" );
	KeyValues::AutoDelete autodelete( pSettings );

	pSettings->SetString( "update/system/access", "public" );

	g_pMatchFramework->GetMatchSession()->UpdateSessionSettings( pSettings );
}

CON_COMMAND( make_game_friends_only, "Changes access for the current game to friends only." )
{
	if ( !g_pMatchFramework || !g_pMatchFramework->GetMatchSession() )
		return;

	if ( !ASWGameResource() || ASWGameResource()->GetLeader() != C_ASW_Player::GetLocalASWPlayer() )
		return;

	KeyValues *pSettings = new KeyValues( "update" );
	KeyValues::AutoDelete autodelete( pSettings );

	pSettings->SetString( "update/system/access", "friends" );

	g_pMatchFramework->GetMatchSession()->UpdateSessionSettings( pSettings );
}