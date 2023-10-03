#include "cbase.h"
#include "asw_marine_speech.h"
#include "asw_marine.h"
#include "asw_marine_resource.h"
#include "asw_gamerules.h"
#include "engine/IEngineSound.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "soundchars.h"
#include "ndebugoverlay.h"
#include "asw_game_resource.h"
#include "te_effect_dispatch.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ISoundEmitterSystemBase *soundemitterbase;

// chatter probabilites (1/x)
#define BASE_CHATTER_SELECTION_CHANCE 5.0f
#define CHATTER_INTERVAL 3.0f
#define ASW_INTERVAL_CHATTER_INCOMING 30.0f
#define ASW_INTERVAL_CHATTER_NO_AMMO 30.0f
#define ASW_INTERVAL_FF 12.0f

#define ASW_CALM_CHATTER_TIME 10.0f

// chance of doing a direction hold position shout (from 0 to 1)
#define ASW_DIRECTIONAL_HOLDING_CHATTER 0.5f

ConVar asw_debug_marine_chatter("asw_debug_marine_chatter", "0", 0, "Show debug info about when marine chatter is triggered");

#define ACTOR_SARGE (1 << 0)
#define ACTOR_WILDCAT (1 << 1)
#define ACTOR_FAITH (1 << 2)
#define ACTOR_CRASH (1 << 3)
#define ACTOR_JAEGER (1 << 4)
#define ACTOR_WOLFE (1 << 5)
#define ACTOR_BASTILLE (1 << 6)
#define ACTOR_VEGAS (1 << 7)
#define ACTOR_FLYNN (1 << 8)
#define ACTOR_ALL (ACTOR_SARGE | ACTOR_JAEGER | ACTOR_WILDCAT | ACTOR_WOLFE | ACTOR_FAITH | ACTOR_BASTILLE | ACTOR_CRASH | ACTOR_FLYNN)

int CASW_MarineSpeech::s_CurrentConversation = CONV_NONE;
int CASW_MarineSpeech::s_CurrentConversationChatterStage = -1;
int CASW_MarineSpeech::s_iCurrentConvLine = -1;
CHandle<CASW_Marine> CASW_MarineSpeech::s_hActor1 = NULL;
CHandle<CASW_Marine> CASW_MarineSpeech::s_hActor2 = NULL;
CHandle<CASW_Marine> CASW_MarineSpeech::s_hCurrentActor = NULL;
static int s_Actor1Indices[ASW_NUM_CONVERSATIONS]={
	0, // CONV_NONE,
	ACTOR_CRASH | ACTOR_VEGAS, // CONV_SYNUP,
	ACTOR_CRASH, // CONV_CRASH_COMPLAIN,
	ACTOR_FAITH | ACTOR_BASTILLE, // CONV_MEDIC_COMPLAIN,
	ACTOR_BASTILLE, // CONV_HEALING_CRASH
	ACTOR_VEGAS, // CONV_TEQUILA,
	ACTOR_CRASH, // CONV_CRASH_IDLE,
	ACTOR_FAITH | ACTOR_BASTILLE, // CONV_SERIOUS_INJURY,
	ACTOR_JAEGER, // CONV_STILL_BREATHING,
	ACTOR_SARGE, // CONV_SARGE_IDLE,
	ACTOR_CRASH, // CONV_BIG_ALIEN,
	ACTOR_WILDCAT | ACTOR_WOLFE, // CONV_AUTOGUN
	ACTOR_WOLFE, // CONV_WOLFE_BEST,
	ACTOR_SARGE, // CONV_SARGE_JAEGER_CONV1, (jaeger starts)
	ACTOR_JAEGER, // CONV_SARGE_JAEGER_CONV2,  (sarge starts)
	ACTOR_WILDCAT, // CONV_WILDCAT_KILL,
	ACTOR_WOLFE, // CONV_WOLFE_KILL
	ACTOR_SARGE, //CONV_COMPLIMENT_JAEGER, // 17
	ACTOR_JAEGER, //CONV_COMPLIMENT_SARGE, // 18
	ACTOR_WILDCAT, //CONV_COMPLIMENT_WOLFE, // 19
	ACTOR_WOLFE, //CONV_COMPLIMENT_WILDCAT, // 20
};

static int s_Actor2Indices[ASW_NUM_CONVERSATIONS]={
	0, // CONV_NONE,
	ACTOR_FAITH | ACTOR_BASTILLE, // CONV_SYNUP,
	ACTOR_BASTILLE, // CONV_CRASH_COMPLAIN,
	ACTOR_ALL, // CONV_MEDIC_COMPLAIN,
	ACTOR_CRASH, // CONV_HEALING_CRASH
	ACTOR_ALL &~ ACTOR_FLYNN, // CONV_TEQUILA,
	ACTOR_SARGE | ACTOR_JAEGER | ACTOR_BASTILLE, // CONV_CRASH_IDLE,
	ACTOR_ALL, // CONV_SERIOUS_INJURY,
	ACTOR_ALL &~ (ACTOR_SARGE | ACTOR_FLYNN | ACTOR_WOLFE), // CONV_STILL_BREATHING,
	ACTOR_SARGE | ACTOR_CRASH, // CONV_SARGE_IDLE,
	ACTOR_SARGE, // CONV_BIG_ALIEN,	
	ACTOR_SARGE | ACTOR_JAEGER, // CONV_AUTOGUN
	ACTOR_WILDCAT, // CONV_WOLFE_BEST,

	ACTOR_JAEGER, // CONV_SARGE_JAEGER_CONV1,  (sarge is 2nd)
	ACTOR_SARGE, // CONV_SARGE_JAEGER_CONV2, (jaeger is 2nd)
	ACTOR_WOLFE, // CONV_WILDCAT_KILL,
	ACTOR_WILDCAT, // CONV_WOLFE_KILL
	ACTOR_JAEGER, //CONV_COMPLIMENT_JAEGER, // 17
	ACTOR_SARGE, //CONV_COMPLIMENT_SARGE, // 18
	ACTOR_WOLFE, //CONV_COMPLIMENT_WOLFE, // 19
	ACTOR_WILDCAT, //CONV_COMPLIMENT_WILDCAT, // 20
};

static int s_ConvChatterLine1[ASW_NUM_CONVERSATIONS]={
	-1, // CONV_NONE,
	CHATTER_SYNUP_SPOTTED, // CONV_SYNUP,
	CHATTER_CRASH_COMPLAIN, // CONV_CRASH_COMPLAIN,
	CHATTER_MEDIC_COMPLAIN, // CONV_MEDIC_COMPLAIN,
	CHATTER_HEALING_CRASH, // CONV_HEALING_CRASH
	CHATTER_TEQUILA_START, // CONV_TEQUILA,
	CHATTER_CRASH_IDLE, // CONV_CRASH_IDLE,
	CHATTER_SERIOUS_INJURY, // CONV_SERIOUS_INJURY,
	CHATTER_STILL_BREATHING, // CONV_STILL_BREATHING,
	CHATTER_SARGE_IDLE, // CONV_SARGE_IDLE,
	CHATTER_BIG_ALIEN_DEAD, // CONV_BIG_ALIEN,
	CHATTER_AUTOGUN, // CONV_AUTOGUN
	CHATTER_WOLFE_BEST, // CONV_WOLFE_BEST,
	CHATTER_SARGE_JAEGER_CONV_1, // CONV_SARGE_JAEGER_CONV1, (jaeger starts)
	CHATTER_SARGE_JAEGER_CONV_2, // CONV_SARGE_JAEGER_CONV2,  (sarge starts)
	CHATTER_WILDCAT_KILL, // CONV_WILDCAT_KILL,
	CHATTER_WOLFE_KILL, // CONV_WOLFE_KILL	
	CHATTER_COMPLIMENTS_JAEGER, //CONV_COMPLIMENT_JAEGER, // 17
	CHATTER_COMPLIMENTS_SARGE, //CONV_COMPLIMENT_SARGE, // 18
	CHATTER_COMPLIMENTS_WOLFE, //CONV_COMPLIMENT_WOLFE, // 19
	CHATTER_COMPLIMENTS_WILDCAT, //CONV_COMPLIMENT_WILDCAT, // 20
};

static int s_ConvChatterLine2[ASW_NUM_CONVERSATIONS]={
	-1, // CONV_NONE,
	CHATTER_SYNUP_REPLY, // CONV_SYNUP,
	CHATTER_CRASH_COMPLAIN_REPLY, // CONV_CRASH_COMPLAIN,
	CHATTER_MEDIC_COMPLAIN_REPLY, // CONV_MEDIC_COMPLAIN,
	CHATTER_HEALING_CRASH_REPLY, // CONV_HEALING_CRASH
	CHATTER_TEQUILA_REPLY, // CONV_TEQUILA,
	CHATTER_CRASH_IDLE_REPLY, // CONV_CRASH_IDLE,
	CHATTER_SERIOUS_INJURY_REPLY, // CONV_SERIOUS_INJURY,
	CHATTER_STILL_BREATHING_REPLY, // CONV_STILL_BREATHING,
	CHATTER_SARGE_IDLE_REPLY, // CONV_SARGE_IDLE,
	CHATTER_BIG_ALIEN_REPLY, // CONV_BIG_ALIEN,
	CHATTER_AUTOGUN_REPLY, // CONV_AUTOGUN
	CHATTER_WOLFE_BEST_REPLY, // CONV_WOLFE_BEST,
	CHATTER_SARGE_JAEGER_CONV_1_REPLY, // CONV_SARGE_JAEGER_CONV1, (jaeger starts)
	CHATTER_SARGE_JAEGER_CONV_2_REPLY, // CONV_SARGE_JAEGER_CONV2,  (sarge starts)
	CHATTER_WILDCAT_KILL_REPLY_AHEAD, // CONV_WILDCAT_KILL,  NOTE: OR BEHIND...
	CHATTER_WOLFE_KILL_REPLY_AHEAD, // CONV_WOLFE_KILL
	-1, //CONV_COMPLIMENT_JAEGER, // 17
	-1, //CONV_COMPLIMENT_SARGE, // 18
	-1, //CONV_COMPLIMENT_WOLFE, // 19
	-1, //CONV_COMPLIMENT_WILDCAT, // 20
};

static int s_ConvNumStages[ASW_NUM_CONVERSATIONS]={
	0, // CONV_NONE,
	2, // CONV_SYNUP,
	3, // CONV_CRASH_COMPLAIN,
	2, // CONV_MEDIC_COMPLAIN,
	3, // CONV_HEALING_CRASH
	3, // CONV_TEQUILA,
	2, // CONV_CRASH_IDLE,
	3, // CONV_SERIOUS_INJURY,
	3, // CONV_STILL_BREATHING,
	3, // CONV_SARGE_IDLE,
	3, // CONV_BIG_ALIEN,		// todo:should be 4?
	3, // CONV_AUTOGUN
	3, // CONV_WOLFE_BEST,
	3, // CONV_SARGE_JAEGER_CONV1, (jaeger starts)
	3, // CONV_SARGE_JAEGER_CONV2,  (sarge starts)
	3, // CONV_WILDCAT_KILL,  NOTE: OR BEHIND...
	3, // CONV_WOLFE_KILL
	1, //CONV_COMPLIMENT_JAEGER, // 17
	1, //CONV_COMPLIMENT_SARGE, // 18
	1, //CONV_COMPLIMENT_WOLFE, // 19
	1, //CONV_COMPLIMENT_WILDCAT, // 20
};

/*
	CHATTER_TEQUILA_REPLY_SARGE, // Vegas only
	CHATTER_TEQUILA_REPLY_JAEGER, // Vegas only
	CHATTER_TEQUILA_REPLY_WILDCAT, // Vegas only
	CHATTER_TEQUILA_REPLY_WOLFE, // Vegas only
	CHATTER_TEQUILA_REPLY_FAITH, // Vegas only
	CHATTER_TEQUILA_REPLY_BASTILLE, // Vegas only
		
	CHATTER_SERIOUS_INJURY_FOLLOW_UP,   // faith/bastille only

	CHATTER_FIRST_BLOOD_START,	// vegas only
	CHATTER_FIRST_BLOOD_WIN,	// vegas only
		
	CHATTER_WILDCAT_KILL_REPLY_BEHIND,	
	CHATTER_WOLFE_KILL_REPLY_BEHIND,
*/

bool CASW_MarineSpeech::StartConversation(int iConversation, CASW_Marine *pMarine, CASW_Marine *pSecondMarine)
{
	if (!pMarine || !pMarine->GetMarineProfile() || iConversation < 0 || iConversation > CONV_COMPLIMENT_WILDCAT
		|| pMarine->GetHealth() <= 0 || pMarine->IsInfested() || pMarine->IsOnFire())
		return false;

	if ( !ASWGameResource() )
		return false;
	
	CASW_Game_Resource *pGameResource = ASWGameResource();

	// check this marine can start this conversation
	if (! ((1 << pMarine->GetMarineProfile()->m_ProfileIndex) & s_Actor1Indices[iConversation]) )
		return false;

	CASW_Marine *pChosen = NULL;
	if (pSecondMarine != NULL && pSecondMarine->GetMarineProfile()!=NULL)
	{
		// if the other actor was specified, check they can do the conversation
		if (! ((1 << pSecondMarine->GetMarineProfile()->m_ProfileIndex) & s_Actor2Indices[iConversation]) )
			return false;
		if (pSecondMarine->IsInfested() || pMarine->IsOnFire())
			return false;
		pChosen = pSecondMarine;
	}
	else
	{
		// try and find another actor to chat with
		int iNearby = 0;
		for (int i=0;i<pGameResource->GetMaxMarineResources();i++)
		{
			CASW_Marine_Resource *pMR = pGameResource->GetMarineResource(i);
			CASW_Marine *pOtherMarine = pMR ? pMR->GetMarineEntity() : NULL;
			if (pOtherMarine && pOtherMarine != pMarine && (pOtherMarine->GetAbsOrigin().DistTo(pMarine->GetAbsOrigin()) < 600)
						&& pOtherMarine->GetHealth() > 0 && !pOtherMarine->IsInfested() && !pOtherMarine->IsOnFire() && pOtherMarine->GetMarineProfile() 
						&& ((1 << pOtherMarine->GetMarineProfile()->m_ProfileIndex) & s_Actor2Indices[iConversation]) )
				iNearby++;
		}
		if (iNearby > 0)
		{
			int iChosen = random->RandomInt(0, iNearby-1);		
			for (int i=0;i<pGameResource->GetMaxMarineResources();i++)
			{
				CASW_Marine_Resource *pMR = pGameResource->GetMarineResource(i);
				CASW_Marine *pOtherMarine = pMR ? pMR->GetMarineEntity() : NULL;
				if (pOtherMarine && pOtherMarine != pMarine && (pOtherMarine->GetAbsOrigin().DistTo(pMarine->GetAbsOrigin()) < 600)
							&& pOtherMarine->GetHealth() > 0 && !pOtherMarine->IsInfested() && !pOtherMarine->IsOnFire() && pOtherMarine->GetMarineProfile() 
							&& ((1 << pOtherMarine->GetMarineProfile()->m_ProfileIndex) & s_Actor2Indices[iConversation]) )
				{
					if (iChosen <= 0)
					{
						pChosen = pOtherMarine;
						break;
					}
					iChosen--;
				}
			}
		}
	}
	if (!pChosen && iConversation != CONV_SYNUP)	// synup doesn't need 2 people to start the conv
		return false;

	if (asw_debug_marine_chatter.GetBool())
		Msg("%f: Starting conv %d successfully, between marines %s and %s\n", gpGlobals->curtime, iConversation, pMarine->GetMarineProfile()->m_ShortName,
			pChosen ? pChosen->GetMarineProfile()->m_ShortName : "no-one");
	s_CurrentConversation = iConversation;
	s_CurrentConversationChatterStage = 0;
	s_hCurrentActor = pMarine;
	s_hActor1 = pMarine;
	s_hActor2 = pChosen;
	s_iCurrentConvLine = s_ConvChatterLine1[iConversation];
	pMarine->GetMarineSpeech()->QueueChatter(s_iCurrentConvLine, gpGlobals->curtime, gpGlobals->curtime + 5.0f);
	return true;
}

void CASW_MarineSpeech::StopConversation()
{
	if (asw_debug_marine_chatter.GetBool())
		Msg("Stopping conversation");
	s_CurrentConversation = CONV_NONE;
	s_CurrentConversationChatterStage = 0;
	s_hCurrentActor = NULL;
	s_hActor1 = NULL;
	s_hActor2 = NULL;
}

CASW_MarineSpeech::CASW_MarineSpeech(CASW_Marine *pMarine)
{	
	m_pMarine = pMarine;
	m_fPersonalChatterTime = 0;
	m_SpeechQueue.Purge();
	m_fFriendlyFireTime = 0;
}

CASW_MarineSpeech::~CASW_MarineSpeech()
{

}

bool CASW_MarineSpeech::TranslateChatter(int &iChatterType, CASW_Marine_Profile *pProfile)
{	
	if (!pProfile)
		return false;
	// special holding position translations
	if (iChatterType == CHATTER_HOLDING_POSITION)
	{
		if (random->RandomFloat() < ASW_DIRECTIONAL_HOLDING_CHATTER)
			FindDirectionalHoldingChatter(iChatterType);
	}
	// random chance of playing some chatters
	else if (iChatterType == CHATTER_SELECTION)
	{	
		// check for using injured version
		if (m_pMarine->IsWounded())
			iChatterType = CHATTER_SELECTION_INJURED;
	}	

	return true;
}

bool CASW_MarineSpeech::ForceChatter(int iChatterType, int iTimerType)
{
	if (!m_pMarine || !m_pMarine->GetMarineResource() || (m_pMarine->GetHealth()<=0 && iChatterType != CHATTER_DIE))	
		return false;
	
	CASW_Marine_Profile *pProfile = m_pMarine->GetMarineResource()->GetProfile();
	if (!pProfile)
		return false;

	if (!TranslateChatter(iChatterType, pProfile))
		return false;

	// check for discarding chatter if it only has a random chance of playing and we don't hit
	if (random->RandomFloat() > pProfile->m_fChatterChance[iChatterType])
		return true;

	// if marine doesn't have this line, then don't try playing it
	const char *szChatter = pProfile->m_Chatter[iChatterType];
	if (szChatter[0] == '\0')
		return false;
		
	InternalPlayChatter(m_pMarine, szChatter, iTimerType, iChatterType);
	return true;
}

bool CASW_MarineSpeech::Chatter(int iChatterType, int iSubChatter, CBasePlayer *pOnlyForPlayer)
{
	if (iChatterType < 0 || iChatterType >= NUM_CHATTER_LINES)
	{
		AssertMsg1( false, "Faulty chatter type %d\n", iChatterType );
		return false;
	}

	if (!m_pMarine || !m_pMarine->GetMarineResource() || (m_pMarine->GetHealth()<=0 && iChatterType != CHATTER_DIE))	
	{
		AssertMsg( false, "Absent marine tried to chatter\n" );
		return false;
	}
	
	CASW_Marine_Profile *pProfile = m_pMarine->GetMarineResource()->GetProfile();
	if (!pProfile)
	{
		AssertMsg1( false, "Marine %s is missing a profile\n", m_pMarine->GetDebugName() );
		return false;
	}

	if (gpGlobals->curtime < GetTeamChatterTime())
		return false;
		
	if (!TranslateChatter(iChatterType, pProfile))
		return false;

	// check for discarding chatter if some marines are in the middle of a conversation
	if (s_CurrentConversation != CONV_NONE)
	{
		CASW_Marine *pCurrentActor = (s_CurrentConversationChatterStage == 1) ? s_hActor2.Get() : s_hActor1.Get();
		int iCurrentConvLine = s_iCurrentConvLine;
		// if this isn't the right speaker saying the right line...
		if (iChatterType != iCurrentConvLine || m_pMarine != pCurrentActor)
			return false;
	}

	// check for discarding chatter if it only has a random chance of playing and we don't hit
	if (random->RandomFloat() > pProfile->m_fChatterChance[iChatterType])
		return true;

	if (iChatterType == CHATTER_FIRING_AT_ALIEN)
	{
		if (gpGlobals->curtime < GetIncomingChatterTime())
			return false;

		SetIncomingChatterTime(gpGlobals->curtime + ASW_INTERVAL_CHATTER_INCOMING);
	}

	// if marine doesn't have this line, then don't try playing it
	const char *szChatter = pProfile->m_Chatter[iChatterType];
	if (szChatter[0] == '\0')
		return false;

	InternalPlayChatter(m_pMarine, szChatter, ASW_CHATTER_TIMER_TEAM, iChatterType, iSubChatter, pOnlyForPlayer);
	return true;
}

bool CASW_MarineSpeech::PersonalChatter(int iChatterType)
{
	if (iChatterType < 0 || iChatterType >= NUM_CHATTER_LINES)
		return false;

	if (!m_pMarine || !m_pMarine->GetMarineResource() || (m_pMarine->GetHealth()<=0 && iChatterType != CHATTER_DIE))
		return false;
	
	CASW_Marine_Profile *pProfile = m_pMarine->GetMarineResource()->GetProfile();
	if (!pProfile)
		return false;

	if (gpGlobals->curtime < GetPersonalChatterTime())
		return false;	
	
	if (!TranslateChatter(iChatterType, pProfile))
		return false;

	// check for discarding chatter if it only has a random chance of playing and we don't hit
	if (random->RandomFloat() > pProfile->m_fChatterChance[iChatterType])
		return true;

	// if marine doesn't have this line, then don't try playing it
	const char *szChatter = pProfile->m_Chatter[iChatterType];
	if (szChatter[0] == '\0')
		return false;

	InternalPlayChatter(m_pMarine, szChatter, ASW_CHATTER_TIMER_PERSONAL, iChatterType);
	return true;
}


void CASW_MarineSpeech::InternalPlayChatter(CASW_Marine* pMarine, const char* szSoundName, int iSetTimer, int iChatterType, int iSubChatter, CBasePlayer *pOnlyForPlayer)
{
	if (!pMarine || !ASWGameRules() || iSubChatter >= NUM_SUB_CHATTERS)
		return;

	if (ASWGameRules()->GetGameState() > ASW_GS_INGAME)
		return;

	// only allow certain chatters when marine is on fire
	if (pMarine->IsOnFire())
	{
		if (iChatterType != CHATTER_FRIENDLY_FIRE &&
			iChatterType != CHATTER_MEDIC &&
			iChatterType != CHATTER_INFESTED &&
			iChatterType != CHATTER_PAIN_SMALL &&
			iChatterType != CHATTER_PAIN_LARGE &&
			iChatterType != CHATTER_DIE &&
			iChatterType != CHATTER_ON_FIRE)
			return;
	}

	if (asw_debug_marine_chatter.GetBool())
		Msg("%s playing chatter %s %d\n", pMarine->GetMarineProfile()->m_ShortName, szSoundName, iChatterType);

	// select a sub chatter
	int iMaxChatter = pMarine->GetMarineProfile()->m_iChatterCount[iChatterType];
	if (iMaxChatter <= 0)
	{
		Msg("Warning, couldn't play chatter of type %d as no sub chatters of that type for this marine\n", iChatterType);
		return;
	}
	if (iSubChatter < 0)
		iSubChatter = random->RandomInt(0, iMaxChatter - 1);

	// asw temp comment
	CPASAttenuationFilter filter( pMarine );		
	//CSoundParameters params;
	//char chatterbuffer[128];
	//Q_snprintf(chatterbuffer, sizeof(chatterbuffer), "%s%d", szSoundName, iSubChatter);
	//if ( pMarine->GetParametersForSound( chatterbuffer, params, NULL ) )
	{
		//if (!params.soundname[0])
			//return;

		if (iSetTimer != ASW_CHATTER_TIMER_NONE)
		{
			// asw temp comment
			//char *skipped = PSkipSoundChars( params.soundname );
			//float duration = enginesound->GetSoundDuration( skipped );
			//Msg("ingame duration for sound %s is %f\n", skipped, duration);
			float duration = pMarine->GetMarineProfile()->m_fChatterDuration[iChatterType][iSubChatter];
			if (iSetTimer == ASW_CHATTER_TIMER_TEAM)
			{
				// make sure no one else talks until this is done
				SetTeamChatterTime(gpGlobals->curtime + duration);
				SetPersonalChatterTime(gpGlobals->curtime + duration);
			}
			else	// ASW_CHATTER_TIMER_PERSONAL
			{
				SetPersonalChatterTime(gpGlobals->curtime + duration);
			}
		}

		//EmitSound_t ep( params );
		//pMarine->EmitSound( filter, pMarine->entindex(), ep );
		
		
		// user messages based speech
		CEffectData	data;

		data.m_vOrigin = pMarine->GetAbsOrigin();
		data.m_nEntIndex = pMarine->entindex();
		data.m_nDamageType = iChatterType;
		data.m_nMaterial = iSubChatter;

		if (!pOnlyForPlayer)
		{
			CPASFilter filter( data.m_vOrigin );
			filter.SetIgnorePredictionCull(true);
			DispatchEffect( filter, 0.0, "ASWSpeech", data );
		}
		else
		{
			CSingleUserRecipientFilter filter( pOnlyForPlayer );
			filter.SetIgnorePredictionCull(true);
			DispatchEffect( filter, 0.0, "ASWSpeech", data );
		}
	}
}

void CASW_MarineSpeech::Precache()
{
	if (!m_pMarine || !m_pMarine->GetMarineResource())
	{
		//Msg("Error: Attempted to precache marine speech when either marine is null or marine has no marine resource.");
		return;
	}
	
	CASW_Marine_Profile *profile= m_pMarine->GetMarineResource()->GetProfile();
	// asw temp comment of speech
	profile->PrecacheSpeech(m_pMarine);
}

float CASW_MarineSpeech::GetTeamChatterTime()
{
	if (!ASWGameRules())
		return 0;

	return ASWGameRules()->GetChatterTime();
}

float CASW_MarineSpeech::GetIncomingChatterTime()
{
	if (!ASWGameRules())
		return 0;

	return ASWGameRules()->GetIncomingChatterTime();
}

void CASW_MarineSpeech::SetTeamChatterTime(float f)
{
	if ( asw_debug_marine_chatter.GetBool() )
		Msg( "%f:SetTeamChatterTime %f\n", gpGlobals->curtime, f );
	if (ASWGameRules())
		ASWGameRules()->SetChatterTime(f);
}

void CASW_MarineSpeech::SetIncomingChatterTime(float f)
{
	if ( asw_debug_marine_chatter.GetBool() )
		Msg( "%f:SetIncomingChatterTime %f\n", gpGlobals->curtime, f );
	if (ASWGameRules())
		ASWGameRules()->SetIncomingChatterTime(f);
}

void CASW_MarineSpeech::SetPersonalChatterTime(float f)
{
	if ( asw_debug_marine_chatter.GetBool() )
		Msg( "%f:SetPersonalChatterTime %f\n", gpGlobals->curtime, f );
	m_fPersonalChatterTime = f;
}

bool CASW_MarineSpeech::FindDirectionalHoldingChatter(int &iChatterType)
{
	if ( !m_pMarine )
		return false;

	Vector vecFacing;
	QAngle angHolding(0, m_pMarine->GetHoldingYaw(), 0);
	AngleVectors(angHolding, &vecFacing);

	// first check there's no other marines standing in front of us
	CASW_Game_Resource *pGameResource = ASWGameResource();
	if (!pGameResource)
		return false;
	int m = pGameResource->GetMaxMarineResources();
	for (int i=0;i<m;i++)
	{
		CASW_Marine_Resource *pMR = pGameResource->GetMarineResource(i);
		if (!pMR || !pMR->GetMarineEntity())
			continue;

		CASW_Marine *pOtherMarine = pMR->GetMarineEntity();
		Vector diff = pOtherMarine->GetAbsOrigin() - m_pMarine->GetAbsOrigin();
		if (diff.Length() > 1000)	// if they're too far away, don't count them as blocking our covering area
			continue;

		// check they're in front
		diff.z = 0;
		diff.NormalizeInPlace();
		if (diff.Dot(vecFacing) < 0.3f)
			continue;

		// this marine is in front, so we can't do a directional shout
		return false;
	}

	// do a few traces to roughly check we're not facing a wall
	trace_t tr;
	if (asw_debug_marine_chatter.GetBool())
		NDebugOverlay::Line( m_pMarine->WorldSpaceCenter(), m_pMarine->WorldSpaceCenter() + vecFacing * 300.0f, 255, 0, 0, false, 5.0f );
	UTIL_TraceLine(m_pMarine->WorldSpaceCenter(), m_pMarine->WorldSpaceCenter() + vecFacing * 300.0f, MASK_SOLID, m_pMarine, COLLISION_GROUP_NONE, &tr);
	if (tr.fraction < 1.0f)
		return false;

	angHolding[YAW] += 30;
	AngleVectors(angHolding, &vecFacing);
	if (asw_debug_marine_chatter.GetBool())
		NDebugOverlay::Line( m_pMarine->WorldSpaceCenter(), m_pMarine->WorldSpaceCenter() + vecFacing * 200.0f, 255, 0, 0, false, 5.0f );
	UTIL_TraceLine(m_pMarine->WorldSpaceCenter(), m_pMarine->WorldSpaceCenter() + vecFacing * 200.0f, MASK_SOLID, m_pMarine, COLLISION_GROUP_NONE, &tr);
	if (tr.fraction < 1.0f)
		return false;

	angHolding[YAW] -= 60;
	AngleVectors(angHolding, &vecFacing);
	if (asw_debug_marine_chatter.GetBool())
		NDebugOverlay::Line( m_pMarine->WorldSpaceCenter(), m_pMarine->WorldSpaceCenter() + vecFacing * 200.0f, 255, 0, 0, false, 5.0f );
	UTIL_TraceLine(m_pMarine->WorldSpaceCenter(), m_pMarine->WorldSpaceCenter() + vecFacing * 200.0f, MASK_SOLID, m_pMarine, COLLISION_GROUP_NONE, &tr);
	if (tr.fraction < 1.0f)
		return false;

	// okay, we're good to do a directional, let's see which one to do
	float fYaw = anglemod(m_pMarine->GetHoldingYaw());
	if (fYaw < 40 || fYaw > 320)
		iChatterType = CHATTER_HOLDING_EAST;
	else if (fYaw > 50 && fYaw < 130)
		iChatterType = CHATTER_HOLDING_NORTH;
	else if (fYaw > 140 && fYaw < 220)
		iChatterType = CHATTER_HOLDING_WEST;
	else if (fYaw > 230 && fYaw < 310)
		iChatterType = CHATTER_HOLDING_SOUTH;
	else
		return false;	// wasn't facing close enough to a cardinal
	
	return true;
}

void CASW_MarineSpeech::QueueChatter(int iChatterType, float fPlayTime, float fMaxPlayTime, CBasePlayer *pOnlyForPlayer)
{
	if (!m_pMarine || m_pMarine->GetHealth() <= 0)
		return;
	// check this speech isn't already in the queue
	int iCount = m_SpeechQueue.Count();
	for (int i=iCount-1;i>=0;i--)
	{
		if (m_SpeechQueue[i].iChatterType == iChatterType)
			return;
	}

	if (iChatterType == CHATTER_HOLDING_POSITION)		// if marine is holding
	{
		int iCount = m_SpeechQueue.Count();
		for (int i=iCount-1;i>=0;i--)
		{
			if (m_SpeechQueue[i].iChatterType == CHATTER_USE)		// then remove any 'confirming move order' type speech queued up
				m_SpeechQueue.Remove(i);
		}
	}
	else if (iChatterType == CHATTER_USE)		// if marine is confirming move order
	{
		int iCount = m_SpeechQueue.Count();
		for (int i=iCount-1;i>=0;i--)
		{
			if (m_SpeechQueue[i].iChatterType == CHATTER_HOLDING_POSITION)		// then remove any holding position speech queued up
				m_SpeechQueue.Remove(i);
		}
	}

	// don't FF fire too frequently
	if (iChatterType == CHATTER_FRIENDLY_FIRE)
	{
		if (gpGlobals->curtime < m_fFriendlyFireTime)
			return;

		m_fFriendlyFireTime = gpGlobals->curtime + ASW_INTERVAL_FF;
	}
	if (asw_debug_marine_chatter.GetBool())
		Msg("%f: queuing speech %d at time %f\n", gpGlobals->curtime, iChatterType, fPlayTime);
	CASW_Queued_Speech entry;
	entry.iChatterType = iChatterType;
	entry.fPlayTime = fPlayTime;
	entry.fMaxPlayTime = fMaxPlayTime;
	entry.hOnlyForPlayer = pOnlyForPlayer;
	m_SpeechQueue.AddToTail(entry);
}

void CASW_MarineSpeech::Update()
{
	if (!m_pMarine)
		return;

	int iCount = m_SpeechQueue.Count();
	
	// loop backwards because we might remove one
	for (int i=iCount-1;i>=0;i--)
	{
		if (gpGlobals->curtime >= m_SpeechQueue[i].fPlayTime)
		{
			// try to play the queued speech
			if (asw_debug_marine_chatter.GetBool())
				Msg("%f: trying to play queued speech %d\n", gpGlobals->curtime, m_SpeechQueue[i].iChatterType);
			bool bPlay = true;

			if ((m_SpeechQueue[i].iChatterType == CHATTER_HOLDING_POSITION
					|| m_SpeechQueue[i].iChatterType == CHATTER_USE)
					&& m_pMarine->IsInhabited())
			{
				bPlay = false;
			}

			if (!bPlay)
			{
				if (asw_debug_marine_chatter.GetBool())
					Msg(" removing queued speech; inappropriate to play it now (marine probably inhabited and this is an AI line)\n");
				m_SpeechQueue.Remove(i);
				continue;
			}

			if (Chatter(m_SpeechQueue[i].iChatterType, -1, m_SpeechQueue[i].hOnlyForPlayer.Get()))
			{
				if (s_CurrentConversation != CONV_NONE)
				{
					CASW_Marine *pCurrentActor = (s_CurrentConversationChatterStage == 1) ? s_hActor2.Get() : s_hActor1.Get();
					int iCurrentConvLine = s_iCurrentConvLine;
					// if we just finished the currently queued conversation line...
					if (m_SpeechQueue[i].iChatterType == iCurrentConvLine && m_pMarine == pCurrentActor)
					{
						s_CurrentConversationChatterStage++;
						int iNumStages = s_ConvNumStages[s_CurrentConversation];
						

/*
	CHATTER_WILDCAT_KILL_REPLY_BEHIND,	
	CHATTER_WOLFE_KILL_REPLY_BEHIND,
*/

						// check all the actors are alive still, etc and we're not at the end of the conversation
						if (s_CurrentConversationChatterStage >= iNumStages
							|| !s_hActor1.Get() || s_hActor1.Get()->GetHealth()<=0
							|| !s_hActor2.Get() || s_hActor2.Get()->GetHealth()<=0)
						{
							StopConversation();
						}
						else
						{
							// queue up the next line
							pCurrentActor = (s_CurrentConversationChatterStage == 1) ? s_hActor2.Get() : s_hActor1.Get();
							iCurrentConvLine = (s_CurrentConversationChatterStage == 0) ? s_ConvChatterLine1[s_CurrentConversation] : s_ConvChatterLine2[s_CurrentConversation];
							// make wolfe/wildcat pick the right line in their conv
							if (s_CurrentConversation == CONV_WILDCAT_KILL)
							{
								if (asw_debug_marine_chatter.GetBool())
									Msg("CONV_WILDCAT_KILL, stage %d\n", s_CurrentConversationChatterStage);
								if (s_CurrentConversationChatterStage == 1)	// wolfe replying, does he have more or less kills
								{
									if (GetWildcatAhead())
										iCurrentConvLine = CHATTER_WILDCAT_KILL_REPLY_BEHIND;
									else
										iCurrentConvLine = CHATTER_WILDCAT_KILL_REPLY_AHEAD;
								}
								else if (s_CurrentConversationChatterStage == 2)	// wildcat following up
								{
									if (GetWildcatAhead())
										iCurrentConvLine = CHATTER_WILDCAT_KILL_REPLY_AHEAD;
									else
										iCurrentConvLine = CHATTER_WILDCAT_KILL_REPLY_BEHIND;
								}
							}
							else if (s_CurrentConversation == CONV_WOLFE_KILL)
							{
								if (asw_debug_marine_chatter.GetBool())
									Msg("CONV_WILDCAT_KILL, stage %d\n", s_CurrentConversationChatterStage);
								if (s_CurrentConversationChatterStage == 1)	// wildcat replying, does he have more or less kills
								{
									if (GetWildcatAhead())
										iCurrentConvLine = CHATTER_WOLFE_KILL_REPLY_AHEAD;
									else
										iCurrentConvLine = CHATTER_WOLFE_KILL_REPLY_BEHIND;
								}
								else if (s_CurrentConversationChatterStage == 2)	// wolfe following up
								{
									if (GetWildcatAhead())
										iCurrentConvLine = CHATTER_WOLFE_KILL_REPLY_BEHIND;
									else
										iCurrentConvLine = CHATTER_WOLFE_KILL_REPLY_AHEAD;
								}
							}
						
							if (s_CurrentConversationChatterStage == 2)
							{
								if (s_CurrentConversation == CONV_TEQUILA && s_hActor2.Get()->GetMarineProfile())
								{
									int iProfileIndex = s_hActor2.Get()->GetMarineProfile()->m_ProfileIndex;
									// change the line to one appropriate for who we're talking to
									if (iProfileIndex == 0) iCurrentConvLine = CHATTER_TEQUILA_REPLY_SARGE;
									else if (iProfileIndex == 4) iCurrentConvLine = CHATTER_TEQUILA_REPLY_JAEGER;
									else if (iProfileIndex == 1) iCurrentConvLine = CHATTER_TEQUILA_REPLY_WILDCAT;
									else if (iProfileIndex == 5) iCurrentConvLine = CHATTER_TEQUILA_REPLY_WOLFE;
									else if (iProfileIndex == 2) iCurrentConvLine = CHATTER_TEQUILA_REPLY_FAITH;
									else if (iProfileIndex == 6) iCurrentConvLine = CHATTER_TEQUILA_REPLY_BASTILLE;
									else iCurrentConvLine = -1;
								}
								else if (s_CurrentConversation == CONV_SERIOUS_INJURY)
								{
									iCurrentConvLine = CHATTER_SERIOUS_INJURY_FOLLOW_UP;
								}
							}

							if (iCurrentConvLine == -1 || s_hActor2.Get()==NULL)
								StopConversation();
							else
							{
								if (asw_debug_marine_chatter.GetBool())
									Msg("Queuing up the next line in this conversation %d\n", iCurrentConvLine);
								s_iCurrentConvLine = iCurrentConvLine;
								pCurrentActor->GetMarineSpeech()->QueueChatter(iCurrentConvLine, GetTeamChatterTime() + 0.05f, GetTeamChatterTime() + 60.0f);
							}
						}
					}
				}
				if (asw_debug_marine_chatter.GetBool())
					Msg(" and removing the speech (%d) since it played ok (or failed the chance to play)\n", m_SpeechQueue[i].iChatterType);
				m_SpeechQueue.Remove(i);
			}
			else
			{
				if (asw_debug_marine_chatter.GetBool())
					Msg(" didn't play ok, trying to requeue\n");
				m_SpeechQueue[i].fPlayTime = GetTeamChatterTime() + 0.05f;		// try and queue it up for when we next have a gap
				if (m_SpeechQueue[i].fPlayTime > m_SpeechQueue[i].fMaxPlayTime)	// if it's too late, then abort
				{
					if (asw_debug_marine_chatter.GetBool())
						Msg(" but out of time!\n");
					m_SpeechQueue.Remove(i);
				}
			}
		}
	}
}

bool CASW_MarineSpeech::GetWildcatAhead()
{
	int iWildcatKills = 0;
	int iWolfeKills = 0;

	if ( !ASWGameResource() )
		return false;
	
	CASW_Game_Resource *pGameResource = ASWGameResource();
	for (int i=0;i<pGameResource->GetMaxMarineResources();i++)
	{
		CASW_Marine_Resource *pMR = pGameResource->GetMarineResource(i);

		if (pMR && pMR->GetProfile() && pMR->GetProfile()->m_VoiceType == ASW_VOICE_WILDCAT)
			iWildcatKills = pMR->m_iAliensKilled;
		else if (pMR && pMR->GetProfile() && pMR->GetProfile()->m_VoiceType == ASW_VOICE_WOLFE)
			iWolfeKills = pMR->m_iAliensKilled;
	}
	return iWildcatKills > iWolfeKills;
}

struct MarineFlavorSpeech
{
	unsigned char uchChatterType;
	unsigned char uchSubChatter;
};

const int g_nNumFlavorSpeech = 25;

MarineFlavorSpeech g_MarineFlavorSpeech[ g_nNumFlavorSpeech ][ ASW_VOICE_TYPE_TOTAL ] =
{
	{
		{ 44, 1 }, // Sarge
		{ 1,0 }, // Jaeger
		{ 3,1 }, // Wildcat
		{ 0,0 }, // Wolf
		{ 0,1 }, // Faith
		{ 2,2 }, // Bastille
		{ 0,1 }, // Crash
		{ 0,0 }, // Flynn
		{ 0,1 }, // Vegas
	},
	{
		{ 123, 0 }, // Sarge
		{ 3,5 }, // Jaeger
		{ 3,2 }, // Wildcat
		{ 1,0 }, // Wolf
		{ 1,3 }, // Faith
		{ 2,3 }, // Bastille
		{ 1,1 }, // Crash
		{ 1,1 }, // Flynn
		{ 1,2 }, // Vegas
	},
	{
		{ 123, 2 }, // Sarge
		{ 3,10 }, // Jaeger
		{ 3,4 }, // Wildcat
		{ 1,3 }, // Wolf
		{ 2,2 }, // Faith
		{ 3,0 }, // Bastille
		{ 2,0 }, // Crash
		{ 3,0 }, // Flynn
		{ 2,1 }, // Vegas
	},
	{
		{ 104, 0 }, // Sarge
		{ 3,12 }, // Jaeger
		{ 3,5 }, // Wildcat
		{ 2,0 }, // Wolf
		{ 2,0 }, // Faith
		{ 3,8 }, // Bastille
		{ 3,3 }, // Crash
		{ 3,2 }, // Flynn
		{ 3,0 }, // Vegas
	},
	{
		{ 104, 1 }, // Sarge
		{ 3,13 }, // Jaeger
		{ 3,6 }, // Wildcat
		{ 3,1 }, // Wolf
		{ 3,4 }, // Faith
		{ 3,10 }, // Bastille
		{ 3,4 }, // Crash
		{ 123,0 }, // Flynn
		{ 3,2 }, // Vegas
	},
	{
		{ 114, 1 }, // Sarge
		{ 3,9 }, // Jaeger
		{ 0,0 }, // Wildcat
		{ 3,0 }, // Wolf
		{ 3,7 }, // Faith
		{ 3,11 }, // Bastille
		{ 27,2 }, // Crash
		{ 3,5 }, // Flynn
		{ 3,8 }, // Vegas
	},
	{
		{ 2, 2 }, // Sarge
		{ 3,7 }, // Jaeger
		{ 1,2 }, // Wildcat
		{ 3,2 }, // Wolf
		{ 3,3 }, // Faith
		{ 3,13 }, // Bastille
		{ 54,3 }, // Crash
		{ 3,3 }, // Flynn
		{ 3,5 }, // Vegas
	},
	{
		{ 3, 5 }, // Sarge
		{ 3,6 }, // Jaeger
		{ 45,1 }, // Wildcat
		{ 3,3 }, // Wolf
		{ 3,8 }, // Faith
		{ 3,0 }, // Bastille
		{ 72,4 }, // Crash
		{ 3,1 }, // Flynn
		{ 6,3 }, // Vegas
	},
	{
		{ 3, 7 }, // Sarge
		{ 3,2 }, // Jaeger
		{ 54,1 }, // Wildcat
		{ 3,6 }, // Wolf
		{ 3,9 }, // Faith
		{ 3,15 }, // Bastille
		{ 73,1 }, // Crash
		{ 5,3 }, // Flynn
		{ 45,1 }, // Vegas
	},
	{
		{ 3, 8 }, // Sarge
		{ 5,2 }, // Jaeger
		{ 61,0 }, // Wildcat
		{ 3,4 }, // Wolf
		{ 54,2 }, // Faith
		{ 3,5 }, // Bastille
		{ 75,4 }, // Crash
		{ 75,0 }, // Flynn
		{ 47,2 }, // Vegas
	},
	{
		{ 3, 12 }, // Sarge
		{ 54,3 }, // Jaeger
		{ 3,3 }, // Wildcat
		{ 3,5 }, // Wolf
		{ 54,3 }, // Faith
		{ 45,2 }, // Bastille
		{ 75,4 }, // Crash
		{ 61,0 }, // Flynn
		{ 54,2 }, // Vegas
	},
	{
		{ 3, 2 }, // Sarge
		{ 54,4 }, // Jaeger
		{ 108,1 }, // Wildcat
		{ 3,6 }, // Wolf
		{ 71,2 }, // Faith
		{ 54,1 }, // Bastille
		{ 61,2 }, // Crash
		{ 123,0 }, // Flynn
		{ 54,0 }, // Vegas
	},
	{
		{ 3, 9 }, // Sarge
		{ 108,0 }, // Jaeger
		{ 63,0 }, // Wildcat
		{ 66,2 }, // Wolf
		{ 84,1 }, // Faith
		{ 69,13 }, // Bastille
		{ 82,0 }, // Crash
		{ 85,0 }, // Flynn
		{ 54,3 }, // Vegas
	},
	{
		{ 47, 1 }, // Sarge
		{ 101,0 }, // Jaeger
		{ 4,0 }, // Wildcat
		{ 123,3 }, // Wolf
		{ 0,1 }, // Faith
		{ 69,18 }, // Bastille
		{ 96,0 }, // Crash
		{ 0,0 }, // Flynn
		{ 72,0 }, // Vegas
	},
	{
		{ 52, 2 }, // Sarge
		{ 1,0 }, // Jaeger
		{ 3,1 }, // Wildcat
		{ 110,8 }, // Wolf
		{ 1,3 }, // Faith
		{ 69,10 }, // Bastille
		{ 2,1 }, // Crash
		{ 85, }, // Flynn
		{ 73,7 }, // Vegas
	},
	{
		{ 3, 13 }, // Sarge
		{ 3,5 }, // Jaeger
		{ 3,2 }, // Wildcat
		{ 85,0 }, // Wolf
		{ 2,2 }, // Faith
		{ 71,0 }, // Bastille
		{ 3,5 }, // Crash
		{ 1,1 }, // Flynn
		{ 75,4 }, // Vegas
	},
	{
		{ 3, 14 }, // Sarge
		{ 3,10 }, // Jaeger
		{ 3,4 }, // Wildcat
		{ 0,0 }, // Wolf
		{ 2,0 }, // Faith
		{ 61,0 }, // Bastille
		{ 0,1 }, // Crash
		{ 3,0 }, // Flynn
		{ 61,0 }, // Vegas
	},
	{
		{ 3, 14 }, // Sarge
		{ 3,12 }, // Jaeger
		{ 3,5 }, // Wildcat
		{ 1,0 }, // Wolf
		{ 3,4 }, // Faith
		{ 123,11 }, // Bastille
		{ 1,3 }, // Crash
		{ 3,2 }, // Flynn
		{ 61,1 }, // Vegas
	},
	{
		{ 3, 16 }, // Sarge
		{ 3,13 }, // Jaeger
		{ 3,6 }, // Wildcat
		{ 1,3 }, // Wolf
		{ 3,7 }, // Faith
		{ 123,7 }, // Bastille
		{ 2,0 }, // Crash
		{ 1,1 }, // Flynn
		{ 61,2 }, // Vegas
	},
	{
		{ 3, 17 }, // Sarge
		{ 3,9 }, // Jaeger
		{ 0,0 }, // Wildcat
		{ 2,0 }, // Wolf
		{ 3,3 }, // Faith
		{ 83,0 }, // Bastille
		{ 3,3 }, // Crash
		{ 3,5 }, // Flynn
		{ 63,0 }, // Vegas
	},
	{
		{ 61, 1 }, // Sarge
		{ 3,7 }, // Jaeger
		{ 1,2 }, // Wildcat
		{ 3,1 }, // Wolf
		{ 3,8 }, // Faith
		{ 84,1 }, // Bastille
		{ 3,4 }, // Crash
		{ 3,3 }, // Flynn
		{ 112,1 }, // Vegas
	},
	{
		{ 61, 2 }, // Sarge
		{ 3,6 }, // Jaeger
		{ 45,1 }, // Wildcat
		{ 3,0 }, // Wolf
		{ 3,0 }, // Faith
		{ 87,0 }, // Bastille
		{ 27,2 }, // Crash
		{ 3,1 }, // Flynn
		{ 112,0 }, // Vegas
	},
	{
		{ 1, 1 }, // Sarge
		{ 3,2 }, // Jaeger
		{ 54,1 }, // Wildcat
		{ 3,2 }, // Wolf
		{ 3,9 }, // Faith
		{ 98,1 }, // Bastille
		{ 54,3 }, // Crash
		{ 5,3 }, // Flynn
		{ 88,0 }, // Vegas
	},
	{
		{ 1, 3 }, // Sarge
		{ 5,2 }, // Jaeger
		{ 61,0 }, // Wildcat
		{ 3,4 }, // Wolf
		{ 54,2 }, // Faith
		{ 99,0 }, // Bastille
		{ 72,4 }, // Crash
		{ 75,0 }, // Flynn
		{ 99,0 }, // Vegas
	},
	{
		{ 38, 2 }, // Sarge
		{ 54,3 }, // Jaeger
		{ 3,3 }, // Wildcat
		{ 3,5 }, // Wolf
		{ 54,3 }, // Faith
		{ 2,2 }, // Bastille
		{ 73,1 }, // Crash
		{ 61,0 }, // Flynn
		{ 102,0 }, // Vegas
	},
};

bool CASW_MarineSpeech::ClientRequestChatter(int iChatterType, int iSubChatter)
{
	CASW_Marine_Profile *pProfile = m_pMarine->GetMarineResource()->GetProfile();
	if ( !pProfile )
	{
		return false;
	}

	if ( iChatterType == -1 )
	{
		int nRandomSpeech = RandomInt( 0, g_nNumFlavorSpeech - 1 );
		iChatterType = g_MarineFlavorSpeech[ nRandomSpeech ][ pProfile->GetVoiceType() ].uchChatterType;
		iSubChatter = g_MarineFlavorSpeech[ nRandomSpeech ][ pProfile->GetVoiceType() ].uchSubChatter;
	}

	// todo: disallow some chatter types (misleading or annoying ones like onfire, infested, etc)
	if (iChatterType < 0 || iChatterType >= NUM_CHATTER_LINES)
	{
		AssertMsg1( false, "Faulty chatter type %d\n", iChatterType );
		return false;
	}

	if (!m_pMarine || !m_pMarine->GetMarineResource() || (m_pMarine->GetHealth()<=0 && iChatterType != CHATTER_DIE))	
	{
		AssertMsg( false, "Absent marine tried to chatter\n" );
		return false;
	}

	if (gpGlobals->curtime < GetTeamChatterTime())
	{
		return false;
	}
		
	//if (!TranslateChatter(iChatterType, pProfile))
		//return false;

	// if marine doesn't have this line, then don't try playing it
	const char *szChatter = pProfile->m_Chatter[iChatterType];
	if (szChatter[0] == '\0')
	{
		return false;
	}

	InternalPlayChatter(m_pMarine, szChatter, ASW_CHATTER_TIMER_TEAM, iChatterType, iSubChatter);
	return true;
}

bool CASW_MarineSpeech::AllowCalmConversations(int iConversation)
{
	if (ASWGameRules() && ASWGameRules()->m_fLastFireTime > gpGlobals->curtime - ASW_CALM_CHATTER_TIME)
		return false;

	return true;
}