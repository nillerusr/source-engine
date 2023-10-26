#ifndef ASW_MARINEPROFILE_H
#define ASW_MARINEPROFILE_H
#pragma once

#include "asw_shareddefs.h"
#include "asw_marine_skills.h"
#include "responserules/response_types.h"

// This class describes all the fixed data about a particular marine profile
// e.g. the marine's name, class, voice, portraits, chatter lines, etc.

#define CHATTER_STRING_SIZE 64

#define NUM_CHATTER_LINES 124
#define NUM_SUB_CHATTERS 18		// max variations allowed for a chatter line

enum ASW_Voice_Type
{
	ASW_VOICE_SARGE,
	ASW_VOICE_JAEGER,
	ASW_VOICE_WILDCAT,
	ASW_VOICE_WOLFE,
	ASW_VOICE_FAITH,
	ASW_VOICE_BASTILLE,
	ASW_VOICE_CRASH,
	ASW_VOICE_FLYNN,
	ASW_VOICE_VEGAS,

	ASW_VOICE_TYPE_TOTAL
};
ASW_Voice_Type GetASWVoiceType( const char *szVoiceType );
const char* GetVoiceShortName( ASW_Voice_Type voice );

// different chatter lines
enum {
	CHATTER_SELECTION,
	CHATTER_SELECTION_INJURED,
	CHATTER_USE,
	CHATTER_IDLE,
	CHATTER_FRIENDLY_FIRE,
	CHATTER_FIRING_AT_ALIEN,
	CHATTER_FOLLOW_ME,
	CHATTER_HOLD_POSITION,
	CHATTER_NEED_AMMO,
	CHATTER_NO_AMMO,
	CHATTER_MEDIC,
	CHATTER_RELOADING,
	CHATTER_SENTRY,
	CHATTER_AMMO,
	CHATTER_MEDKIT,
	CHATTER_THANKS,
	CHATTER_QUESTION,
	CHATTER_SARGE,
	CHATTER_JAEGER,
	CHATTER_WILDCAT,
	CHATTER_WOLFE,
	CHATTER_FAITH,
	CHATTER_BASTILLE,
	CHATTER_CRASH,
	CHATTER_FLYNN,
	CHATTER_VEGAS,
	CHATTER_SUPPLIES,
	CHATTER_SUPPLIES_AMMO,
	CHATTER_LOCKED_TERMINAL,
	CHATTER_LOCKED_TERMINAL_CRASH,
	CHATTER_LOCKED_TERMINAL_FLYNN,
	CHATTER_LOCKED_TERMINAL_VEGAS,
	CHATTER_HOLDING_POSITION,
	CHATTER_HOLDING_NORTH,
	CHATTER_HOLDING_SOUTH,
	CHATTER_HOLDING_EAST,
	CHATTER_HOLDING_WEST,
	CHATTER_GOT_POINT,
	CHATTER_GOT_REAR,
	CHATTER_REQUEST_SEAL_DOOR,
	CHATTER_REQUEST_CUT_DOOR,
	CHATTER_REQUEST_SHOOT_DOOR,
	CHATTER_CUTTING_DOOR,
	CHATTER_SEALING_DOOR,
	CHATTER_BIOMASS,
	CHATTER_TIME_TO_LEAVE,
	CHATTER_WATCH_OUT,
	CHATTER_SHIELDBUG,
	CHATTER_SHIELDBUG_HINT,
	CHATTER_PARASITE,
	CHATTER_INFESTED,
	CHATTER_EGGS,
	CHATTER_GRENADE,
	CHATTER_ALIEN_TOO_CLOSE,
	CHATTER_MAD_FIRING,
	CHATTER_BREACHED_DOOR,
	CHATTER_MARINE_DOWN,
	CHATTER_PAIN_SMALL,
	CHATTER_PAIN_LARGE,
	CHATTER_DIE,
	CHATTER_ON_FIRE,
	CHATTER_COMPLIMENTS,
	CHATTER_STIM_NOW,
	CHATTER_IMPATIENCE,

	CHATTER_COMPLIMENTS_JAEGER,	// sarge only
	CHATTER_COMPLIMENTS_SARGE,	// jaeger only
	CHATTER_COMPLIMENTS_WILDCAT,	// wolfe only
	CHATTER_COMPLIMENTS_WOLFE,	// wildcat only
	CHATTER_COMPLIMENTS_CRASH,	// wildcat only	

	CHATTER_HEALING,										// class specific..
	CHATTER_MEDS_LOW,
	CHATTER_MEDS_NONE,

	CHATTER_HACK_STARTED,
	CHATTER_HACK_LONG_STARTED,
	CHATTER_HACK_HALFWAY,
	CHATTER_HACK_FINISHED,
	CHATTER_HACK_BUTTON_FINISHED, 

	CHATTER_SCANNER,
	CHATTER_SCANNER_MULTIPLE,	

	CHATTER_MINE_DEPLOYED,

	CHATTER_SYNUP_SPOTTED,		// crash/vegas only			// conversations...
	CHATTER_SYNUP_REPLY,		// faith/bastille only
	CHATTER_CRASH_COMPLAIN,
	CHATTER_CRASH_COMPLAIN_REPLY,
	CHATTER_MEDIC_COMPLAIN,
	CHATTER_MEDIC_COMPLAIN_REPLY,
	CHATTER_HEALING_CRASH,		// bastille only
	CHATTER_HEALING_CRASH_REPLY,
	CHATTER_TEQUILA_START,		// Vegas only
	CHATTER_TEQUILA_REPLY,
	CHATTER_TEQUILA_REPLY_SARGE, // Vegas only
	CHATTER_TEQUILA_REPLY_JAEGER, // Vegas only
	CHATTER_TEQUILA_REPLY_WILDCAT, // Vegas only
	CHATTER_TEQUILA_REPLY_WOLFE, // Vegas only
	CHATTER_TEQUILA_REPLY_FAITH, // Vegas only
	CHATTER_TEQUILA_REPLY_BASTILLE, // Vegas only
	CHATTER_CRASH_IDLE,		// crash only
	CHATTER_CRASH_IDLE_REPLY,	// sarge/crash only
	CHATTER_SERIOUS_INJURY,		// faith/bastille only
	CHATTER_SERIOUS_INJURY_REPLY,		// all marines
	CHATTER_SERIOUS_INJURY_FOLLOW_UP,   // faith/bastille only
	CHATTER_STILL_BREATHING,		// jaeger only
	CHATTER_STILL_BREATHING_REPLY,
	CHATTER_SARGE_IDLE,		// sarge only
	CHATTER_SARGE_IDLE_REPLY,
	CHATTER_BIG_ALIEN_DEAD,		// crash only
	CHATTER_BIG_ALIEN_REPLY,
	CHATTER_AUTOGUN,	// wildcat/wolfe only
	CHATTER_AUTOGUN_REPLY,
	CHATTER_WOLFE_BEST,	// wolfe only
	CHATTER_WOLFE_BEST_REPLY,	// wolfe only
	CHATTER_FIRST_BLOOD_START,	// vegas only
	CHATTER_FIRST_BLOOD_WIN,	// vegas only
	CHATTER_SARGE_JAEGER_CONV_1,
	CHATTER_SARGE_JAEGER_CONV_1_REPLY,
	CHATTER_SARGE_JAEGER_CONV_2,
	CHATTER_SARGE_JAEGER_CONV_2_REPLY,
	CHATTER_WILDCAT_KILL,
	CHATTER_WILDCAT_KILL_REPLY_AHEAD,
	CHATTER_WILDCAT_KILL_REPLY_BEHIND,
	CHATTER_WOLFE_KILL,
	CHATTER_WOLFE_KILL_REPLY_AHEAD,
	CHATTER_WOLFE_KILL_REPLY_BEHIND,			// 123rd chatter line

	CHATTER_MISC,								// 124th chatter line
};

class CASW_Marine_Profile
{
public:
					CASW_Marine_Profile();
	virtual			~CASW_Marine_Profile();
	
	void		ApplyKeyValues( KeyValues *pKeys );

	// this profile's position in the list of profiles
	int			m_ProfileIndex;	
	
	// Accessors
	inline bool CanHack( void );						///< Can this character type hack computers? (In general -- the marine may be unable to do it at this moment due to combat, distance, etc)
	inline bool CanScanner( void );						///< Does this character type have a scanner?
	inline bool CanUseTechWeapons( void );				///< Can use tech-specific weapons
	inline bool HasHackSkill( void );					///< Can hack faster than a normal marine and you can do GetSkillBasedValueByMarine to find out the skill score
	inline bool HasTechIcon( void );
	inline bool CanUseFirstAid( void );
	inline bool CanUseAutogun( void );
	
	void SetMarineClass( ASW_Marine_Class marineClass );
	ASW_Marine_Class GetMarineClass();

	// personal data
	const char *GetShortName() { return m_ShortName; }
	void SetShortName( const char *szName );
	bool IsFemale() { return m_bFemale; }
	void SetFemale( bool bFemale ) { m_bFemale = bFemale; }
	char	m_RankName[24];
	char	m_FirstName[24];
	char	m_LastName[24];
	char	m_ShortName[24];
	bool		m_bNickname;	// is the shortname a nickname to be put in quotes between 1st and last?
	char	m_Bio[512];
	int			m_Age;
	bool		m_bFemale;
#ifdef GAME_DLL
	ResponseRules::CriteriaSet::CritSymbol_t m_nResponseRulesName; // used for the "who" fact
#endif
	
	// model
	const char *GetModelName() { return m_ModelName; }
	void SetModelName( const char *szName );
	int GetSkinNum() { return m_SkinNum; }
	void SetSkinNum( int iSkin ) { m_SkinNum = iSkin; }
	char	m_ModelName[64];
	int			m_SkinNum;

	// portraits
	char m_PortraitName[24];		// name used for  portrait vmt files
	int m_nPortraitTextureID;		// regular small portrait texture ID
	int m_nClassTextureID;			// class icon

	// chatter stuff
	ASW_Voice_Type GetVoiceType();
	void SetVoiceType( ASW_Voice_Type vt );
	char m_Chatter[NUM_CHATTER_LINES][CHATTER_STRING_SIZE];
	void InitChatterNames(const char *szMarineName);
	int m_iChatterCount[NUM_CHATTER_LINES];	// how many speech lines this marine has for each chatter type
#ifndef CLIENT_DLL
	void PrecacheSpeech(CBaseEntity* pEnt);
	float m_fChatterChance[NUM_CHATTER_LINES];	
	float m_fChatterDuration[NUM_CHATTER_LINES][NUM_SUB_CHATTERS];	// how long each sub chatter line is
	void SaveSpeechDurations(CBaseEntity *pEnt);
#endif
	
	char	m_DefaultWeaponsInSlots[ ASW_MAX_EQUIP_SLOTS ][32];
	ASW_Voice_Type		m_VoiceType;

	ASW_Marine_Class m_iMarineClass;

	ASW_Skill GetSkillMapping( int nSkillSlot );

	int m_iStaticSkills[ ASW_NUM_SKILL_SLOTS ];			// skills for single mission mode
	ASW_Skill m_nSkillMapping[ ASW_NUM_SKILL_SLOTS - 1 ];	// maps skill slot to actual skill

	void LoadTextures();	// loads the portrait textures
};

class CASW_Marine_ProfileList
{
public:
	CASW_Marine_ProfileList();
	virtual ~CASW_Marine_ProfileList();

	void PrecacheSpeech(CBaseEntity* pEnt);
#ifndef CLIENT_DLL
	void SaveSpeechDurations(CBaseEntity *pEnt);
#endif

	CASW_Marine_Profile* GetProfile(int i)
	{		
		if (i >=0 && i < m_NumProfiles)
			return m_Profiles[i];

		return NULL;
	}

	CASW_Marine_Profile* m_Profiles[16];
	int m_NumProfiles;
};

extern CASW_Marine_ProfileList* MarineProfileList();

inline bool CASW_Marine_Profile::CanHack( void )
{
	return GetMarineClass() == MARINE_CLASS_TECH;
}

inline bool CASW_Marine_Profile::HasHackSkill( void )
{
	return GetMarineClass() == MARINE_CLASS_TECH;
}

inline bool CASW_Marine_Profile::CanScanner( void )
{
	return GetMarineClass() == MARINE_CLASS_TECH;
}

inline bool CASW_Marine_Profile::CanUseTechWeapons( void )
{
	return GetMarineClass() == MARINE_CLASS_TECH;
}

inline bool CASW_Marine_Profile::HasTechIcon( void ) 
{
	return GetMarineClass() == MARINE_CLASS_TECH;
}

inline bool CASW_Marine_Profile::CanUseFirstAid( void ) 
{
	return GetMarineClass() == MARINE_CLASS_MEDIC;
}

inline bool CASW_Marine_Profile::CanUseAutogun( void )
{
	return GetMarineClass() == MARINE_CLASS_SPECIAL_WEAPONS;
}
#endif /* ASW_MARINEPROFILE_H */