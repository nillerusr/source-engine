#include "cbase.h"
#include "asw_marine_profile.h"
#include "filesystem.h"
#include <KeyValues.h>
#include "asw_util_shared.h"
#include "stringpool.h"
#ifdef CLIENT_DLL
	#include <vgui/ISurface.h>
	#include <vgui/ISystem.h>
	#include <vgui_controls/Panel.h>
#else
	#include "engine/IEngineSound.h"
	#include "SoundEmitterSystem/isoundemittersystembase.h"
	#include "soundchars.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifndef CLIENT_DLL
ConVar asw_precache_speech("asw_precache_speech", "0", FCVAR_CHEAT, "If set, server will precache all speech files (increases load times a lot!)");
#endif

CASW_Marine_ProfileList *g_pMarineProfileList = NULL;

ASW_Voice_Type GetASWVoiceType( const char *szVoiceType )
{
	if ( !Q_stricmp( szVoiceType, "ASW_VOICE_SARGE" ) )			return ASW_VOICE_SARGE;
	if ( !Q_stricmp( szVoiceType, "ASW_VOICE_JAEGER" ) )		return ASW_VOICE_JAEGER;
	if ( !Q_stricmp( szVoiceType, "ASW_VOICE_WILDCAT" ) )		return ASW_VOICE_WILDCAT;
	if ( !Q_stricmp( szVoiceType, "ASW_VOICE_WOLFE" ) )			return ASW_VOICE_WOLFE;
	if ( !Q_stricmp( szVoiceType, "ASW_VOICE_FAITH" ) )			return ASW_VOICE_FAITH;
	if ( !Q_stricmp( szVoiceType, "ASW_VOICE_BASTILLE" ) )		return ASW_VOICE_BASTILLE;
	if ( !Q_stricmp( szVoiceType, "ASW_VOICE_CRASH" ) )			return ASW_VOICE_CRASH;
	if ( !Q_stricmp( szVoiceType, "ASW_VOICE_FLYNN" ) )			return ASW_VOICE_FLYNN;
	if ( !Q_stricmp( szVoiceType, "ASW_VOICE_VEGAS" ) )			return ASW_VOICE_VEGAS;

	return ASW_VOICE_SARGE;
}

const char* GetVoiceShortName( ASW_Voice_Type voice )
{
	switch( voice )
	{
		case ASW_VOICE_SARGE: return "Sarge";
		case ASW_VOICE_JAEGER: return "Jaeger";
		case ASW_VOICE_WILDCAT: return "Wildcat";
		case ASW_VOICE_WOLFE: return "Wolfe";
		case ASW_VOICE_FAITH: return "Faith";
		case ASW_VOICE_BASTILLE: return "Bastille";
		case ASW_VOICE_CRASH: return "Crash";
		case ASW_VOICE_FLYNN: return "Flynn";
		case ASW_VOICE_VEGAS: return "Vegas";
	}

	return "Sarge";
}

// ------------------
//      Profiles
// ------------------

CASW_Marine_Profile::CASW_Marine_Profile()
{
	m_nPortraitTextureID = -1;
	m_nClassTextureID = -1;
	SetVoiceType( ASW_VOICE_SARGE );

	memset( m_DefaultWeaponsInSlots, 0, sizeof( m_DefaultWeaponsInSlots ) );

	for (int i=0;i<NUM_CHATTER_LINES;i++)
	{
		m_Chatter[i][0] = '\0';
		m_iChatterCount[i] = 0;
#ifndef CLIENT_DLL
		m_fChatterChance[i] = 1.0f;		
		for (int k=0;k<NUM_SUB_CHATTERS;k++)
		{
			m_fChatterDuration[i][k] = 0;			
		}	
#endif
	}

	Q_snprintf(m_ModelName, sizeof(m_ModelName), "swarm/marine/marine.mdl");	 
	m_SkinNum = 0;

	for (int i=0;i<ASW_NUM_SKILL_SLOTS;i++)
	{
		m_iStaticSkills[i] = 0;
		m_nSkillMapping[i] = ASW_MARINE_SKILL_INVALID;
	}
	m_iMarineClass = MARINE_CLASS_UNDEFINED;
}

CASW_Marine_Profile::~CASW_Marine_Profile()
{
}

void CASW_Marine_Profile::LoadTextures()
{
	#ifdef CLIENT_DLL
		char buffer[64];
		// load the portrait textures
		m_nPortraitTextureID = vgui::surface()->CreateNewTextureID();
		Q_snprintf(buffer, sizeof(buffer), "vgui/briefing/face_%s", m_PortraitName);		
		vgui::surface()->DrawSetTextureFile( m_nPortraitTextureID, buffer, true, false);
		
		if (GetMarineClass() == MARINE_CLASS_NCO)
			Q_snprintf(buffer, sizeof(buffer), "vgui/swarm/ClassIcons/NCOClassIcon");
		else if (GetMarineClass() == MARINE_CLASS_SPECIAL_WEAPONS)
			Q_snprintf(buffer, sizeof(buffer), "vgui/swarm/ClassIcons/SpecialWeaponsClassIcon");
		else if (GetMarineClass() == MARINE_CLASS_MEDIC)
			Q_snprintf(buffer, sizeof(buffer), "vgui/swarm/ClassIcons/MedicClassIcon");
		else if (GetMarineClass() == MARINE_CLASS_TECH)
			Q_snprintf(buffer, sizeof(buffer), "vgui/swarm/ClassIcons/TechClassIcon");
		m_nClassTextureID = vgui::surface()->CreateNewTextureID();				
		vgui::surface()->DrawSetTextureFile( m_nClassTextureID, buffer, true, false);
		
	#endif
}


// ------------------
//    Profile list
// ------------------

CASW_Marine_ProfileList::CASW_Marine_ProfileList()
{
	Assert( !g_pMarineProfileList );
	g_pMarineProfileList = this;
	m_NumProfiles = 0;

	KeyValues *kv = new KeyValues( "resource/Profiles.res" );
	if (kv->LoadFromFile(filesystem, "resource/Profiles.res"))
	{
		// find each section and make a new marine profile, fill in data based on the keys in that section
		KeyValues *pKeys = kv;
		while ( pKeys )
		{		
			CASW_Marine_Profile* profile = new CASW_Marine_Profile();
			m_Profiles[m_NumProfiles] = profile;
			profile->m_ProfileIndex = m_NumProfiles;
			profile->ApplyKeyValues( pKeys );
			m_NumProfiles++;
			profile->InitChatterNames( GetVoiceShortName( profile->m_VoiceType ) );
			profile->LoadTextures();
			pKeys = pKeys->GetNextKey();
		}
	}
}

void CASW_Marine_Profile::ApplyKeyValues( KeyValues *pKeys )
{
	for (KeyValues *details = pKeys->GetFirstSubKey(); details; details = details->GetNextKey())
	{
		if ( FStrEq( details->GetName(), "Rank" ) )
			Q_snprintf(m_RankName, sizeof(m_RankName), "%s", details->GetString());
		else if ( FStrEq( details->GetName(), "FirstName" ) )
			Q_snprintf(m_FirstName, sizeof(m_FirstName), "%s", details->GetString());					
		else if ( FStrEq( details->GetName(), "LastName" ) )
			Q_snprintf(m_LastName, sizeof(m_LastName), "%s", details->GetString());					
		else if ( FStrEq( details->GetName(), "Bio" ) )
		{
			Q_snprintf(m_Bio, sizeof(m_Bio), "%s", details->GetString());
		}
		//else if ( FStrEq( details->GetName(), "NickName" ) )
			//Q_snprintf(m_NickName, sizeof(m_NickName), "%s", details->GetString());
		else if ( FStrEq( details->GetName(), "ShortName" ) )
			SetShortName( details->GetString() );
		//else if ( FStrEq( details->GetName(), "PrefixName" ) )
			//Q_snprintf(m_PrefixName, sizeof(m_PrefixName), "%s", details->GetString());
		//else if ( FStrEq( details->GetName(), "SuffixName" ) )
			//Q_snprintf(m_SuffixName, sizeof(m_SuffixName), "%s", details->GetString());	
	
#ifdef GAME_DLL
		else if ( FStrEq( details->GetName(), "RRName" ) )
			m_nResponseRulesName = ResponseRules::CriteriaSet::ComputeCriteriaSymbol( details->GetString() );
#endif

		else if ( FStrEq( details->GetName(), "PortraitName" ) )
			Q_snprintf(m_PortraitName, sizeof(m_PortraitName), "%s", details->GetString());

		else if ( FStrEq( details->GetName(), "HasNickname" ) )
			m_bNickname = (details->GetInt() > 0);
		//else if ( FStrEq( details->GetName(), "HasMiddleInitial" ) )
			//m_bMiddleInitial = (details->GetInt() > 0);
		else if ( FStrEq( details->GetName(), "Voice" ) )
			SetVoiceType( GetASWVoiceType( details->GetString() ) );
		else if ( FStrEq( details->GetName(), "Age" ) )
			m_Age = details->GetInt();
		else if ( FStrEq( details->GetName(), "Female" ) )
			SetFemale(details->GetInt() > 0);
		else if ( FStrEq( details->GetName(), "MarineClass" ) )
			m_iMarineClass = (ASW_Marine_Class) details->GetInt();
		else if ( FStrEq( details->GetName(), "Model" ) )
			SetModelName( details->GetString() );
		else if ( FStrEq( details->GetName(), "Skin" ) )
			m_SkinNum = details->GetInt();
		
		// default equip
		else if ( char const *szDeafultWeaponIdx = StringAfterPrefix( details->GetName(), "DefaultWeapon" ) )
		{
			int idx = Q_atoi( szDeafultWeaponIdx );
			if ( idx >= 0 && idx < ASW_MAX_EQUIP_SLOTS )
				Q_strncpy( m_DefaultWeaponsInSlots[ idx ], details->GetString(), sizeof( m_DefaultWeaponsInSlots[ idx ] ) );
		}

		// static skills
		else if ( FStrEq( details->GetName(), "SkillPointsSlot0" ) )
			m_iStaticSkills[ASW_SKILL_SLOT_0] = details->GetInt();
		else if ( FStrEq( details->GetName(), "SkillPointsSlot1" ) )
			m_iStaticSkills[ASW_SKILL_SLOT_1] = details->GetInt();
		else if ( FStrEq( details->GetName(), "SkillPointsSlot2" ) )
			m_iStaticSkills[ASW_SKILL_SLOT_2] = details->GetInt();
		else if ( FStrEq( details->GetName(), "SkillPointsSlot3" ) )
			m_iStaticSkills[ASW_SKILL_SLOT_3] = details->GetInt();
		else if ( FStrEq( details->GetName(), "SkillPointsSlot4" ) )
			m_iStaticSkills[ASW_SKILL_SLOT_4] = details->GetInt();
		else if ( FStrEq( details->GetName(), "SkillSlotSpare" ) )
			m_iStaticSkills[ASW_SKILL_SLOT_SPARE] = details->GetInt();
	}

	// skill mapping
	m_nSkillMapping[ASW_SKILL_SLOT_0] = SkillFromString( pKeys->GetString( "SkillSlot0", "ASW_MARINE_SKILL_INVALID" ) );
	m_nSkillMapping[ASW_SKILL_SLOT_1] = SkillFromString( pKeys->GetString( "SkillSlot1", "ASW_MARINE_SKILL_INVALID" ) );
	m_nSkillMapping[ASW_SKILL_SLOT_2] = SkillFromString( pKeys->GetString( "SkillSlot2", "ASW_MARINE_SKILL_INVALID" ) );
	m_nSkillMapping[ASW_SKILL_SLOT_3] = SkillFromString( pKeys->GetString( "SkillSlot3", "ASW_MARINE_SKILL_INVALID" ) );
	m_nSkillMapping[ASW_SKILL_SLOT_4] = SkillFromString( pKeys->GetString( "SkillSlot4", "ASW_MARINE_SKILL_INVALID" ) );
}

void CASW_Marine_Profile::InitChatterNames(const char *szMarineName)
{
	Q_snprintf(m_Chatter[CHATTER_SELECTION], CHATTER_STRING_SIZE, "%s.Selection", szMarineName);
	Q_snprintf(m_Chatter[CHATTER_SELECTION_INJURED], CHATTER_STRING_SIZE, "%s.InjuredSelection", szMarineName);
	Q_snprintf(m_Chatter[CHATTER_USE], CHATTER_STRING_SIZE, "%s.Use", szMarineName);
	Q_snprintf(m_Chatter[CHATTER_IDLE], CHATTER_STRING_SIZE, "%s.Idle", szMarineName);
	Q_snprintf(m_Chatter[CHATTER_FRIENDLY_FIRE], CHATTER_STRING_SIZE, "%s.FF", szMarineName);
	Q_snprintf(m_Chatter[CHATTER_FIRING_AT_ALIEN], CHATTER_STRING_SIZE, "%s.FiringAtAlien", szMarineName);
	Q_snprintf(m_Chatter[CHATTER_FOLLOW_ME], CHATTER_STRING_SIZE, "%s.FollowMe", szMarineName);
	Q_snprintf(m_Chatter[CHATTER_HOLD_POSITION], CHATTER_STRING_SIZE, "%s.HoldPosition", szMarineName);
	Q_snprintf(m_Chatter[CHATTER_NEED_AMMO], CHATTER_STRING_SIZE, "%s.NeedAmmo", szMarineName);
	Q_snprintf(m_Chatter[CHATTER_NO_AMMO], CHATTER_STRING_SIZE, "%s.NoAmmo", szMarineName);
	Q_snprintf(m_Chatter[CHATTER_MEDIC], CHATTER_STRING_SIZE, "%s.Medic", szMarineName);
	Q_snprintf(m_Chatter[CHATTER_RELOADING], CHATTER_STRING_SIZE, "%s.Reloading", szMarineName);
	Q_snprintf(m_Chatter[CHATTER_SENTRY], CHATTER_STRING_SIZE, "%s.SentryInPlace", szMarineName);
	Q_snprintf(m_Chatter[CHATTER_AMMO], CHATTER_STRING_SIZE, "%s.Ammo", szMarineName);
	Q_snprintf(m_Chatter[CHATTER_MEDKIT], CHATTER_STRING_SIZE, "%s.Medkit", szMarineName);
	Q_snprintf(m_Chatter[CHATTER_THANKS], CHATTER_STRING_SIZE, "%s.Thanks", szMarineName);
	Q_snprintf(m_Chatter[CHATTER_QUESTION], CHATTER_STRING_SIZE, "%s.Question", szMarineName);
	if (m_VoiceType != ASW_VOICE_SARGE)
		Q_snprintf(m_Chatter[CHATTER_SARGE], CHATTER_STRING_SIZE, "%s.CallSarge", szMarineName);
	if (m_VoiceType != ASW_VOICE_JAEGER)
		Q_snprintf(m_Chatter[CHATTER_JAEGER], CHATTER_STRING_SIZE, "%s.CallJaeger", szMarineName);
	if (m_VoiceType != ASW_VOICE_WILDCAT)
		Q_snprintf(m_Chatter[CHATTER_WILDCAT], CHATTER_STRING_SIZE, "%s.CallWildcat", szMarineName);
	if (m_VoiceType != ASW_VOICE_WOLFE)
		Q_snprintf(m_Chatter[CHATTER_WOLFE], CHATTER_STRING_SIZE, "%s.CallWolfe", szMarineName);
	if (m_VoiceType != ASW_VOICE_FAITH)
		Q_snprintf(m_Chatter[CHATTER_FAITH], CHATTER_STRING_SIZE, "%s.CallFaith", szMarineName);
	if (m_VoiceType != ASW_VOICE_BASTILLE)
		Q_snprintf(m_Chatter[CHATTER_BASTILLE], CHATTER_STRING_SIZE, "%s.CallBastille", szMarineName);
	if (m_VoiceType != ASW_VOICE_CRASH)
		Q_snprintf(m_Chatter[CHATTER_CRASH], CHATTER_STRING_SIZE, "%s.CallCrash", szMarineName);
	if (m_VoiceType != ASW_VOICE_FLYNN)
		Q_snprintf(m_Chatter[CHATTER_FLYNN], CHATTER_STRING_SIZE, "%s.CallFlynn", szMarineName);
	if (m_VoiceType != ASW_VOICE_VEGAS)
		Q_snprintf(m_Chatter[CHATTER_VEGAS], CHATTER_STRING_SIZE, "%s.CallVegas", szMarineName);
	Q_snprintf(m_Chatter[CHATTER_SUPPLIES], CHATTER_STRING_SIZE, "%s.Supplies", szMarineName);
	Q_snprintf(m_Chatter[CHATTER_SUPPLIES_AMMO], CHATTER_STRING_SIZE, "%s.SuppliesAmmo", szMarineName);
	if ( !CanHack() )	// techs don't have these lines
	{
		Q_snprintf(m_Chatter[CHATTER_LOCKED_TERMINAL], CHATTER_STRING_SIZE, "%s.LockedTerminal", szMarineName);
		Q_snprintf(m_Chatter[CHATTER_LOCKED_TERMINAL_CRASH], CHATTER_STRING_SIZE, "%s.LockedTerminalCrash", szMarineName);
		Q_snprintf(m_Chatter[CHATTER_LOCKED_TERMINAL_FLYNN], CHATTER_STRING_SIZE, "%s.LockedTerminalFlynn", szMarineName);
		Q_snprintf(m_Chatter[CHATTER_LOCKED_TERMINAL_VEGAS], CHATTER_STRING_SIZE, "%s.LockedTerminalVegas", szMarineName);
	}
	Q_snprintf(m_Chatter[CHATTER_HOLDING_POSITION], CHATTER_STRING_SIZE, "%s.HoldingPosition", szMarineName);
	Q_snprintf(m_Chatter[CHATTER_HOLDING_NORTH], CHATTER_STRING_SIZE, "%s.HoldingNorth", szMarineName);
	Q_snprintf(m_Chatter[CHATTER_HOLDING_SOUTH], CHATTER_STRING_SIZE, "%s.HoldingSouth", szMarineName);
	Q_snprintf(m_Chatter[CHATTER_HOLDING_EAST], CHATTER_STRING_SIZE, "%s.HoldingEast", szMarineName);
	Q_snprintf(m_Chatter[CHATTER_HOLDING_WEST], CHATTER_STRING_SIZE, "%s.HoldingWest", szMarineName);
	Q_snprintf(m_Chatter[CHATTER_GOT_POINT], CHATTER_STRING_SIZE, "%s.GotPoint", szMarineName);
	Q_snprintf(m_Chatter[CHATTER_GOT_REAR], CHATTER_STRING_SIZE, "%s.GotRear", szMarineName);
	Q_snprintf(m_Chatter[CHATTER_REQUEST_SEAL_DOOR], CHATTER_STRING_SIZE, "%s.RequestSealDoor", szMarineName);
	Q_snprintf(m_Chatter[CHATTER_REQUEST_CUT_DOOR], CHATTER_STRING_SIZE, "%s.RequestCutDoor", szMarineName);
	Q_snprintf(m_Chatter[CHATTER_REQUEST_SHOOT_DOOR], CHATTER_STRING_SIZE, "%s.RequestShootDoor", szMarineName);
	Q_snprintf(m_Chatter[CHATTER_CUTTING_DOOR], CHATTER_STRING_SIZE, "%s.CuttingDoor", szMarineName);
	Q_snprintf(m_Chatter[CHATTER_SEALING_DOOR], CHATTER_STRING_SIZE, "%s.SealingDoor", szMarineName);
	Q_snprintf(m_Chatter[CHATTER_BIOMASS], CHATTER_STRING_SIZE, "%s.Biomass", szMarineName);
	Q_snprintf(m_Chatter[CHATTER_TIME_TO_LEAVE], CHATTER_STRING_SIZE, "%s.TimeToLeave", szMarineName);
	Q_snprintf(m_Chatter[CHATTER_WATCH_OUT], CHATTER_STRING_SIZE, "%s.WatchOut", szMarineName);
	Q_snprintf(m_Chatter[CHATTER_SHIELDBUG], CHATTER_STRING_SIZE, "%s.Shieldbug", szMarineName);
	Q_snprintf(m_Chatter[CHATTER_SHIELDBUG_HINT], CHATTER_STRING_SIZE, "%s.ShieldbugHint", szMarineName);
	Q_snprintf(m_Chatter[CHATTER_PARASITE], CHATTER_STRING_SIZE, "%s.Parasite", szMarineName);
	Q_snprintf(m_Chatter[CHATTER_INFESTED], CHATTER_STRING_SIZE, "%s.BeenInfested", szMarineName);
	Q_snprintf(m_Chatter[CHATTER_EGGS], CHATTER_STRING_SIZE, "%s.SwarmEggs", szMarineName);
	Q_snprintf(m_Chatter[CHATTER_GRENADE], CHATTER_STRING_SIZE, "%s.Grenade", szMarineName);
	Q_snprintf(m_Chatter[CHATTER_ALIEN_TOO_CLOSE], CHATTER_STRING_SIZE, "%s.AlienTooClose", szMarineName);
	Q_snprintf(m_Chatter[CHATTER_MAD_FIRING], CHATTER_STRING_SIZE, "%s.MadFiring", szMarineName);
	Q_snprintf(m_Chatter[CHATTER_BREACHED_DOOR], CHATTER_STRING_SIZE, "%s.BreachedDoor", szMarineName);
	Q_snprintf(m_Chatter[CHATTER_MARINE_DOWN], CHATTER_STRING_SIZE, "%s.MarineDown", szMarineName);
	Q_snprintf(m_Chatter[CHATTER_PAIN_SMALL], CHATTER_STRING_SIZE, "%s.SmallPain", szMarineName);
	Q_snprintf(m_Chatter[CHATTER_PAIN_LARGE], CHATTER_STRING_SIZE, "%s.LargePain", szMarineName);
	Q_snprintf(m_Chatter[CHATTER_DIE], CHATTER_STRING_SIZE, "%s.Dead", szMarineName);
	Q_snprintf(m_Chatter[CHATTER_ON_FIRE], CHATTER_STRING_SIZE, "%s.OnFire", szMarineName);
	if ( GetVoiceType() != ASW_VOICE_JAEGER && GetVoiceType() != ASW_VOICE_FAITH && GetVoiceType() != ASW_VOICE_WOLFE )	// everyone but jaeger/faith/wolfe
		Q_snprintf(m_Chatter[CHATTER_COMPLIMENTS], CHATTER_STRING_SIZE, "%s.Compliments", szMarineName);
	Q_snprintf(m_Chatter[CHATTER_STIM_NOW], CHATTER_STRING_SIZE, "%s.ActivateStims", szMarineName);
	if ( GetVoiceType() == ASW_VOICE_SARGE || GetVoiceType() == ASW_VOICE_WILDCAT || GetVoiceType() == ASW_VOICE_VEGAS )	// sarge, wildcat and vegas only
		Q_snprintf(m_Chatter[CHATTER_IMPATIENCE], CHATTER_STRING_SIZE, "%s.Impatience", szMarineName);

	// don't print the following lines for marines that don't have them
	if ( GetVoiceType() == ASW_VOICE_SARGE )
		Q_snprintf(m_Chatter[CHATTER_COMPLIMENTS_JAEGER], CHATTER_STRING_SIZE, "%s.ComplimentsJaeger", szMarineName);
	if ( GetVoiceType() == ASW_VOICE_JAEGER )
		Q_snprintf(m_Chatter[CHATTER_COMPLIMENTS_SARGE], CHATTER_STRING_SIZE, "%s.ComplimentsSarge", szMarineName);
	if ( GetVoiceType() == ASW_VOICE_WOLFE )
		Q_snprintf(m_Chatter[CHATTER_COMPLIMENTS_WILDCAT], CHATTER_STRING_SIZE, "%s.ComplimentsWildcat", szMarineName);
	if ( GetVoiceType() == ASW_VOICE_WILDCAT )
		Q_snprintf(m_Chatter[CHATTER_COMPLIMENTS_WOLFE], CHATTER_STRING_SIZE, "%s.ComplimentsWolfe", szMarineName);
	if ( GetVoiceType() == ASW_VOICE_WILDCAT)
		Q_snprintf(m_Chatter[CHATTER_COMPLIMENTS_CRASH], CHATTER_STRING_SIZE, "%s.ComplimentsCrash", szMarineName);

	// medics only
	if ( GetVoiceType() == ASW_VOICE_FAITH || GetVoiceType() == ASW_VOICE_BASTILLE )
	{
		Q_snprintf(m_Chatter[CHATTER_HEALING], CHATTER_STRING_SIZE, "%s.Healing", szMarineName);
		if ( GetVoiceType() == ASW_VOICE_BASTILLE )	// bastille only
			Q_snprintf(m_Chatter[CHATTER_MEDS_LOW], CHATTER_STRING_SIZE, "%s.LowOnMeds", szMarineName);
		Q_snprintf(m_Chatter[CHATTER_MEDS_NONE], CHATTER_STRING_SIZE, "%s.NoMeds", szMarineName);
	}

	// techs only
	if ( GetMarineClass() == MARINE_CLASS_TECH )
	{
		Q_snprintf(m_Chatter[CHATTER_HACK_STARTED], CHATTER_STRING_SIZE, "%s.StartedHack", szMarineName);
		Q_snprintf(m_Chatter[CHATTER_HACK_LONG_STARTED], CHATTER_STRING_SIZE, "%s.StartedLongHack", szMarineName);
		Q_snprintf(m_Chatter[CHATTER_HACK_HALFWAY], CHATTER_STRING_SIZE, "%s.HalfwayHack", szMarineName);
		Q_snprintf(m_Chatter[CHATTER_HACK_FINISHED], CHATTER_STRING_SIZE, "%s.FinishedHack", szMarineName);	
		Q_snprintf(m_Chatter[CHATTER_HACK_BUTTON_FINISHED], CHATTER_STRING_SIZE, "%s.FinishedHackButton", szMarineName);			

		if ( GetVoiceType() != ASW_VOICE_VEGAS )
			Q_snprintf(m_Chatter[CHATTER_SCANNER], CHATTER_STRING_SIZE, "%s.Scanner", szMarineName);
		Q_snprintf(m_Chatter[CHATTER_SCANNER_MULTIPLE], CHATTER_STRING_SIZE, "%s.ScannerMultiple", szMarineName);
	}

	if ( GetVoiceType() == ASW_VOICE_SARGE || GetVoiceType() == ASW_VOICE_JAEGER )
		Q_snprintf(m_Chatter[CHATTER_MINE_DEPLOYED], CHATTER_STRING_SIZE, "%s.MineDeployed", szMarineName);

	if ( GetVoiceType() == ASW_VOICE_CRASH || GetVoiceType() == ASW_VOICE_VEGAS )
		Q_snprintf(m_Chatter[CHATTER_SYNUP_SPOTTED], CHATTER_STRING_SIZE, "%s.SynUpSpotted", szMarineName);
	if ( GetVoiceType() == ASW_VOICE_FAITH || GetVoiceType() == ASW_VOICE_BASTILLE )
		Q_snprintf(m_Chatter[CHATTER_SYNUP_REPLY], CHATTER_STRING_SIZE, "%s.SynUpReply", szMarineName);
	if ( GetVoiceType() == ASW_VOICE_CRASH )
		Q_snprintf(m_Chatter[CHATTER_CRASH_COMPLAIN], CHATTER_STRING_SIZE, "%s.CrashComplain", szMarineName);
	if ( GetVoiceType() == ASW_VOICE_CRASH || GetVoiceType() == ASW_VOICE_BASTILLE )
		Q_snprintf(m_Chatter[CHATTER_CRASH_COMPLAIN_REPLY], CHATTER_STRING_SIZE, "%s.CrashComplainReply", szMarineName);
	Q_snprintf(m_Chatter[CHATTER_MEDIC_COMPLAIN], CHATTER_STRING_SIZE, "%s.MedicComplain", szMarineName);
	Q_snprintf(m_Chatter[CHATTER_MEDIC_COMPLAIN_REPLY], CHATTER_STRING_SIZE, "%s.MedicComplainReply", szMarineName);
	if ( GetVoiceType() == ASW_VOICE_BASTILLE )
		Q_snprintf(m_Chatter[CHATTER_HEALING_CRASH], CHATTER_STRING_SIZE, "%s.HealingCrash", szMarineName);
	if ( GetVoiceType() == ASW_VOICE_CRASH || GetVoiceType() == ASW_VOICE_BASTILLE )
		Q_snprintf(m_Chatter[CHATTER_HEALING_CRASH_REPLY], CHATTER_STRING_SIZE, "%s.HealingCrashReply", szMarineName);
	if ( GetVoiceType() == ASW_VOICE_VEGAS )
	{
		Q_snprintf(m_Chatter[CHATTER_TEQUILA_START], CHATTER_STRING_SIZE, "%s.TequilaStart", szMarineName);
		Q_snprintf(m_Chatter[CHATTER_TEQUILA_REPLY_SARGE], CHATTER_STRING_SIZE, "%s.TequilaReplySarge", szMarineName);
		Q_snprintf(m_Chatter[CHATTER_TEQUILA_REPLY_JAEGER], CHATTER_STRING_SIZE, "%s.TequilaReplyJaeger", szMarineName);
		Q_snprintf(m_Chatter[CHATTER_TEQUILA_REPLY_WILDCAT], CHATTER_STRING_SIZE, "%s.TequilaReplyWildcat", szMarineName);
		Q_snprintf(m_Chatter[CHATTER_TEQUILA_REPLY_WOLFE], CHATTER_STRING_SIZE, "%s.TequilaReplyWolfe", szMarineName);
		Q_snprintf(m_Chatter[CHATTER_TEQUILA_REPLY_FAITH], CHATTER_STRING_SIZE, "%s.TequilaReplyFaith", szMarineName);
		Q_snprintf(m_Chatter[CHATTER_TEQUILA_REPLY_BASTILLE], CHATTER_STRING_SIZE, "%s.TequilaBastille", szMarineName);
	}	
	if ( GetVoiceType() != ASW_VOICE_FLYNN )	// all but flynn
		Q_snprintf(m_Chatter[CHATTER_TEQUILA_REPLY], CHATTER_STRING_SIZE, "%s.TequilaReply", szMarineName);
	
	if ( GetVoiceType() == ASW_VOICE_CRASH )	 // crash only
		Q_snprintf(m_Chatter[CHATTER_CRASH_IDLE], CHATTER_STRING_SIZE, "%s.CrashIdle", szMarineName);
	if ( GetVoiceType() == ASW_VOICE_SARGE || GetVoiceType() == ASW_VOICE_JAEGER || GetVoiceType() == ASW_VOICE_BASTILLE )	 // sarge/jaeger/bastille
		Q_snprintf(m_Chatter[CHATTER_CRASH_IDLE_REPLY], CHATTER_STRING_SIZE, "%s.CrashIdleReply", szMarineName);
	if ( GetVoiceType() == ASW_VOICE_FAITH || GetVoiceType() == ASW_VOICE_BASTILLE )	// medics only
	{
		Q_snprintf(m_Chatter[CHATTER_SERIOUS_INJURY], CHATTER_STRING_SIZE, "%s.SeriousInjury", szMarineName);
		Q_snprintf(m_Chatter[CHATTER_SERIOUS_INJURY_FOLLOW_UP], CHATTER_STRING_SIZE, "%s.SeriousInjuryFollowUp", szMarineName);
	}	
	Q_snprintf(m_Chatter[CHATTER_SERIOUS_INJURY_REPLY], CHATTER_STRING_SIZE, "%s.SeriousInjuryReply", szMarineName);
	if ( GetVoiceType() == ASW_VOICE_JAEGER )	// jaeger only
		Q_snprintf(m_Chatter[CHATTER_STILL_BREATHING], CHATTER_STRING_SIZE, "%s.StillBreathing", szMarineName);
	if ( GetVoiceType() != ASW_VOICE_SARGE && GetVoiceType() != ASW_VOICE_WOLFE && GetVoiceType() != ASW_VOICE_FLYNN )	// everyone but sarge/flynn/wolfe
		Q_snprintf(m_Chatter[CHATTER_STILL_BREATHING_REPLY], CHATTER_STRING_SIZE, "%s.StillBreathingReply", szMarineName);
	if ( GetVoiceType() == ASW_VOICE_SARGE )	 // sarge only
		Q_snprintf(m_Chatter[CHATTER_SARGE_IDLE], CHATTER_STRING_SIZE, "%s.SargeIdle", szMarineName);
	if ( GetVoiceType() == ASW_VOICE_SARGE || GetVoiceType() == ASW_VOICE_CRASH )	 // sarge/crash only
		Q_snprintf(m_Chatter[CHATTER_SARGE_IDLE_REPLY], CHATTER_STRING_SIZE, "%s.SargeIdleReply", szMarineName);
	if ( GetVoiceType() == ASW_VOICE_CRASH )	 // crash only
		Q_snprintf(m_Chatter[CHATTER_BIG_ALIEN_DEAD], CHATTER_STRING_SIZE, "%s.BigAlienDead", szMarineName);
	if ( GetVoiceType() == ASW_VOICE_CRASH || GetVoiceType() == ASW_VOICE_SARGE )	 // crash/sarge only
		Q_snprintf(m_Chatter[CHATTER_BIG_ALIEN_REPLY], CHATTER_STRING_SIZE, "%s.BigAlienDeadReply", szMarineName);
	if ( GetVoiceType() == ASW_VOICE_WILDCAT || GetVoiceType() == ASW_VOICE_WOLFE )	// wildcat/wolfe only
		Q_snprintf(m_Chatter[CHATTER_AUTOGUN], CHATTER_STRING_SIZE, "%s.Autogun", szMarineName);
	if ( GetVoiceType() == ASW_VOICE_SARGE || GetVoiceType() == ASW_VOICE_JAEGER || GetVoiceType() == ASW_VOICE_WILDCAT || GetVoiceType() == ASW_VOICE_WOLFE )	// sarge/jaeger/wildcat/wolfe only
		Q_snprintf(m_Chatter[CHATTER_AUTOGUN_REPLY], CHATTER_STRING_SIZE, "%s.AutogunReply", szMarineName);
	if ( GetVoiceType() == ASW_VOICE_WOLFE )	// wolfe only
		Q_snprintf(m_Chatter[CHATTER_WOLFE_BEST], CHATTER_STRING_SIZE, "%s.WolfeBest", szMarineName);
	if ( GetVoiceType() == ASW_VOICE_WILDCAT || GetVoiceType() == ASW_VOICE_WOLFE )	// wildcat/wolfe only
		Q_snprintf(m_Chatter[CHATTER_WOLFE_BEST_REPLY], CHATTER_STRING_SIZE, "%s.WolfeBestReply", szMarineName);
	if ( GetVoiceType() == ASW_VOICE_VEGAS ) // vegas only
	{
		Q_snprintf(m_Chatter[CHATTER_FIRST_BLOOD_START], CHATTER_STRING_SIZE, "%s.FirstBloodStart", szMarineName);
		Q_snprintf(m_Chatter[CHATTER_FIRST_BLOOD_WIN], CHATTER_STRING_SIZE, "%s.FirstBloodWin", szMarineName);
	}
	if ( GetVoiceType() == ASW_VOICE_SARGE )	// sarge only
		Q_snprintf(m_Chatter[CHATTER_SARGE_JAEGER_CONV_1], CHATTER_STRING_SIZE, "%s.SargeJaegerConv1", szMarineName);
	if ( GetVoiceType() == ASW_VOICE_SARGE || GetVoiceType() == ASW_VOICE_JAEGER )	// sarge/jaeger only
		Q_snprintf(m_Chatter[CHATTER_SARGE_JAEGER_CONV_1_REPLY], CHATTER_STRING_SIZE, "%s.SargeJaegerConv1Reply", szMarineName);
	if ( GetVoiceType() == ASW_VOICE_JAEGER )	// jaeger only
		Q_snprintf(m_Chatter[CHATTER_SARGE_JAEGER_CONV_2], CHATTER_STRING_SIZE, "%s.SargeJaegerConv2", szMarineName);
	if ( GetVoiceType() == ASW_VOICE_SARGE || GetVoiceType() == ASW_VOICE_JAEGER)	// sarge/jaeger only
		Q_snprintf(m_Chatter[CHATTER_SARGE_JAEGER_CONV_2_REPLY], CHATTER_STRING_SIZE, "%s.SargeJaegerConv2Reply", szMarineName);
	if ( GetVoiceType() == ASW_VOICE_WILDCAT ) // wildcat only
		Q_snprintf(m_Chatter[CHATTER_WILDCAT_KILL], CHATTER_STRING_SIZE, "%s.WildcatKillStart", szMarineName);
	if ( GetVoiceType() == ASW_VOICE_WOLFE) // wolfe only
		Q_snprintf(m_Chatter[CHATTER_WOLFE_KILL], CHATTER_STRING_SIZE, "%s.WolfeKillStart", szMarineName);
	if ( GetVoiceType() == ASW_VOICE_WILDCAT || GetVoiceType() == ASW_VOICE_WOLFE )	// wildcat/wofle only
	{
		Q_snprintf(m_Chatter[CHATTER_WILDCAT_KILL_REPLY_AHEAD], CHATTER_STRING_SIZE, "%s.WildcatKillReplyAhead", szMarineName);
		Q_snprintf(m_Chatter[CHATTER_WILDCAT_KILL_REPLY_BEHIND], CHATTER_STRING_SIZE, "%s.WildcatKillReplyBehind", szMarineName);
		Q_snprintf(m_Chatter[CHATTER_WOLFE_KILL_REPLY_AHEAD], CHATTER_STRING_SIZE, "%s.WolfeKillReplyAhead", szMarineName);
		Q_snprintf(m_Chatter[CHATTER_WOLFE_KILL_REPLY_BEHIND], CHATTER_STRING_SIZE, "%s.WolfeKillReplyBehind", szMarineName);
	}
	Q_snprintf(m_Chatter[CHATTER_MISC], CHATTER_STRING_SIZE, "%s.Misc", szMarineName);

	// read speech counts in
	char szKeyName[64];
	char szFileName[128];
	Q_snprintf(szKeyName, sizeof(szKeyName), "%sChatterCount", szMarineName);
	Q_snprintf(szFileName, sizeof(szFileName), "scripts/asw_speech_count_%s.txt", szMarineName);
	KeyValues *pCountKeyValues = new KeyValues( szKeyName );
	if (pCountKeyValues && pCountKeyValues->LoadFromFile(filesystem, szFileName))
	{
		// now go down all the chatters, grabbing the count
		for (int i=0;i<NUM_CHATTER_LINES;i++)
		{
			int iCount = pCountKeyValues->GetInt(m_Chatter[i]);;
			iCount = MIN(iCount, NUM_SUB_CHATTERS);
			m_iChatterCount[i] = iCount;
		}
	}	
	if (pCountKeyValues)
		pCountKeyValues->deleteThis();
		
#ifndef CLIENT_DLL
	// setup chatter chances
	m_fChatterChance[CHATTER_HOLDING_POSITION] = 0.2f;
	m_fChatterChance[CHATTER_HOLDING_NORTH] = 0.2f;
	m_fChatterChance[CHATTER_HOLDING_SOUTH] = 0.2f;
	m_fChatterChance[CHATTER_HOLDING_EAST] = 0.2f;
	m_fChatterChance[CHATTER_HOLDING_WEST] = 0.2f;
	//m_fChatterChance[CHATTER_SYNUP_SPOTTED] = 1.0f;
	m_fChatterChance[CHATTER_HEALING] = 0.5f;

	// read speech durations in
	Q_snprintf(szKeyName, sizeof(szKeyName), "%sChatterDuration", szMarineName);
	Q_snprintf(szFileName, sizeof(szFileName), "scripts/asw_speech_duration_%s.txt", szMarineName);
	KeyValues *pDurationKeyValues = new KeyValues( szKeyName );
	if (pDurationKeyValues->LoadFromFile(filesystem, szFileName))
	{
		// now go down all the chatters, grabbing the count
		for (int i=0;i<NUM_CHATTER_LINES;i++)
		{
			for (int k=0;k<m_iChatterCount[i];k++)
			{
				char chatterbuffer[128];
				Q_snprintf(chatterbuffer, sizeof(chatterbuffer), "%s%d", m_Chatter[i], k);
				m_fChatterDuration[i][k] = pDurationKeyValues->GetFloat(chatterbuffer);
			}
		}
	}
	if (pDurationKeyValues)
		pDurationKeyValues->deleteThis();

#endif

	// clear the lines that some marines don't have
	if ( GetVoiceType() == ASW_VOICE_SARGE )
	{
		m_Chatter[CHATTER_SARGE][0] = '\0';
		m_Chatter[CHATTER_COMPLIMENTS_SARGE][0] = '\0';
		m_Chatter[CHATTER_COMPLIMENTS_WILDCAT][0] = '\0';
		m_Chatter[CHATTER_COMPLIMENTS_WOLFE][0] = '\0';
		m_Chatter[CHATTER_COMPLIMENTS_CRASH][0] = '\0';
	}	
}

#ifndef CLIENT_DLL
void CASW_Marine_Profile::PrecacheSpeech(CBaseEntity *pEnt)
{
	if (asw_precache_speech.GetBool())
	{
		for (int i=0;i<NUM_CHATTER_LINES;i++)
		{
			for (int k=0;k<m_iChatterCount[i];k++)
			{
				if (m_Chatter[i][0] != '\0')
				{
					char buffer[128];
					Q_snprintf(buffer, sizeof(buffer), "%s%d", m_Chatter[i], k);
					Msg("Precaching speech %s\n", buffer);
					pEnt->PrecacheScriptSound(buffer);
				}
			}		
		}	
	}

	// precache the model here too
	pEnt->PrecacheModel(m_ModelName);
}
#endif

CASW_Marine_ProfileList::~CASW_Marine_ProfileList()
{
	Assert( g_pMarineProfileList == this );
	g_pMarineProfileList = NULL;

	// delete all the marine profiles too
	for (int i=0;i<m_NumProfiles;i++)
	{
		delete m_Profiles[i];
	}
}

CASW_Marine_ProfileList* MarineProfileList()
{
	if (g_pMarineProfileList == NULL)
	{
		g_pMarineProfileList = new CASW_Marine_ProfileList();
	}
	return g_pMarineProfileList;
}

void CASW_Marine_ProfileList::PrecacheSpeech(CBaseEntity* pEnt)
{
#ifndef CLIENT_DLL
	for (int i=0;i<m_NumProfiles;i++)
	{
		m_Profiles[i]->PrecacheSpeech(pEnt);
	}
#endif
}

#ifndef CLIENT_DLL
// These functions measure the duration of each speech line and save it to a file.
//   This is done so the server can know the duration of the speech wavs without actually loading in all the wav files.
void CASW_Marine_ProfileList::SaveSpeechDurations(CBaseEntity *pEnt)
{
	if (!asw_precache_speech.GetBool())
	{
		Msg("Please set asw_precache_speech to 1 and reload the map before building speech durations.  If you do not do this, they will build incorrectly with zero duration.");
		return;
	}
	for (int i=0;i<m_NumProfiles;i++)
	{
		m_Profiles[i]->SaveSpeechDurations(pEnt);
	}
}

// saves out a file containing speech durations
void CASW_Marine_Profile::SaveSpeechDurations(CBaseEntity *pEnt)
{
	// precache all speech
	for (int i=0;i<NUM_CHATTER_LINES;i++)
	{
		for (int k=0;k<m_iChatterCount[i];k++)
		{
			if (m_Chatter[i][0] != '\0')
			{
				char chatterbuffer[128];
				Q_snprintf(chatterbuffer, sizeof(chatterbuffer), "%s%d", m_Chatter[i], k);			
				pEnt->PrecacheScriptSound(chatterbuffer);
			}
		}
	}
	// create keyvalues
	char szKeyName[64];
	char szFileName[128];
	Q_snprintf(szKeyName, sizeof(szKeyName), "%sChatterDuration", GetVoiceShortName( GetVoiceType() ) );
	Q_snprintf(szFileName, sizeof(szFileName), "scripts/asw_speech_duration_%s.txt", GetVoiceShortName( GetVoiceType() ) );
	KeyValues *pDurationKeyValues = new KeyValues( szKeyName );
	if (pDurationKeyValues)
	{
		// now go down all the chatters, grabbing the duration
		for (int i=0;i<NUM_CHATTER_LINES;i++)
		{
			for (int k=0;k<m_iChatterCount[i];k++)
			{
				char chatterbuffer[128];
				Q_snprintf(chatterbuffer, sizeof(chatterbuffer), "%s%d", m_Chatter[i], k);
				float duration = 1.0f;
				CSoundParameters params;	
				if ( pEnt->GetParametersForSound( chatterbuffer, params, NULL ) )
				{
					if (!params.soundname[0])
						return;
					
					char* skipped = PSkipSoundChars( params.soundname );
					duration = enginesound->GetSoundDuration( skipped );
					Msg("Duration for sound %s is %f\n", skipped, duration);
				}
				pDurationKeyValues->SetFloat(chatterbuffer, duration);				
			}
		}
	}
	pDurationKeyValues->SaveToFile(filesystem, szFileName);
	if (pDurationKeyValues)
		pDurationKeyValues->deleteThis();
}
#endif
void CASW_Marine_Profile::SetMarineClass( ASW_Marine_Class marineClass )
{
	m_iMarineClass = marineClass;
}

ASW_Marine_Class CASW_Marine_Profile::GetMarineClass()
{
	return (ASW_Marine_Class) m_iMarineClass;
}
ASW_Voice_Type CASW_Marine_Profile::GetVoiceType()
{
	return m_VoiceType;
}

void CASW_Marine_Profile::SetVoiceType( ASW_Voice_Type vt )
{
	m_VoiceType = vt;
}
void CASW_Marine_Profile::SetShortName( const char *szName )
{
	Q_snprintf( m_ShortName, sizeof( m_ShortName ), "%s", szName );
}

void CASW_Marine_Profile::SetModelName( const char *szName )
{
	Q_snprintf( m_ModelName, sizeof( m_ModelName ), "%s", szName );
}

ASW_Skill CASW_Marine_Profile::GetSkillMapping( int nSkillSlot )
{
	if ( nSkillSlot < 0 || nSkillSlot >= ASW_NUM_SKILL_SLOTS )
		return ASW_MARINE_SKILL_INVALID;

	if ( nSkillSlot == ASW_SKILL_SLOT_SPARE )
		return ASW_MARINE_SKILL_SPARE;

	return m_nSkillMapping[ nSkillSlot ];
}