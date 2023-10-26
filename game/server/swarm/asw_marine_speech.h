#ifndef ASW_MARINE_SPEECH_H
#define ASW_MARINE_SPEECH_H
#ifdef _WIN32
#pragma once
#endif

#include "baseentity.h"
#include "asw_marine_profile.h"

class CASW_Marine;

#define ASW_NUM_CONVERSATIONS 21

// conversations
enum
{
	CONV_NONE,	// 0
	CONV_SYNUP,	// 1
	CONV_CRASH_COMPLAIN,	// 2
	CONV_MEDIC_COMPLAIN,	// 3
	CONV_HEALING_CRASH,	// 4
	CONV_TEQUILA, // 5
	CONV_CRASH_IDLE, // 6
	CONV_SERIOUS_INJURY, // 7 
	CONV_STILL_BREATHING, // 8
	CONV_SARGE_IDLE,// 9
	CONV_BIG_ALIEN,// 10
	CONV_AUTOGUN,// 11
	CONV_WOLFE_BEST,// 12
	CONV_SARGE_JAEGER_CONV1,// 13
	CONV_SARGE_JAEGER_CONV2,// 14
	CONV_WILDCAT_KILL,// 15
	CONV_WOLFE_KILL,// 16
	CONV_COMPLIMENT_JAEGER, // 17
	CONV_COMPLIMENT_SARGE, // 18
	CONV_COMPLIMENT_WOLFE, // 19
	CONV_COMPLIMENT_WILDCAT, // 20
};

// timer types to set when we play a chatter
enum
{
	ASW_CHATTER_TIMER_TEAM,
	ASW_CHATTER_TIMER_PERSONAL,
	ASW_CHATTER_TIMER_NONE
};

class CASW_Queued_Speech
{
public:
	int iChatterType;
	float fPlayTime;
	float fMaxPlayTime;
	CHandle<CBasePlayer> hOnlyForPlayer;
};

// This class triggers marine speech upon certain events

class CASW_MarineSpeech
{
public:
	CASW_MarineSpeech(CASW_Marine *pMarine);
	virtual ~CASW_MarineSpeech();

	void Precache();

	// NOTE: chatter functions return true if the speech played, or if the speech was skipped because the random % chance of playing meant it shouldn't play this time
	//  they will return false if the speech failed to play for other reasons (someone else was talking, no live marine, etc.)

	// player has requested a chatter to be played, we allow some to be played for comms/taunts/flavour
	bool ClientRequestChatter(int iChatterType, int iSubChatter=-1);

	// play a chatter from the marine's profile
	bool Chatter(int iChatterType, int iSubChatter=-1, CBasePlayer* pOnlyForPlayer=NULL);

	// this will play the chatter even if someone else is talking, but won't interrupt ourselves
	bool PersonalChatter(int iChatterType);

	// this will play the chatter even if someone else is talking and will even interrupt ourselves
	bool ForceChatter(int iChatterType, int iTimerType);

	// translates chatter type based on things going on in-game (i.e. selection chatter into injured selection if the marine is low on health)
	bool TranslateChatter(int &iChatterType, CASW_Marine_Profile *pProfile);

	float GetTeamChatterTime();
	float GetIncomingChatterTime();	
	void SetTeamChatterTime(float f);
	void SetIncomingChatterTime(float f);	

	void SetPersonalChatterTime(float f);
	float GetPersonalChatterTime() { return m_fPersonalChatterTime; }

	bool FindDirectionalHoldingChatter(int &iChatterType);

	// queue up a chatter line to be played at a certain time
	void QueueChatter(int iChatterType, float fPlayTime, float fMaxPlayTime, CBasePlayer *pOnlyForPlayer=NULL);

	// check if any queued speech needs to be played
	void Update();

	static int s_CurrentConversation;
	static int s_CurrentConversationChatterStage;
	static CHandle<CASW_Marine> s_hCurrentActor;
	static CHandle<CASW_Marine> s_hActor1;
	static CHandle<CASW_Marine> s_hActor2;
	static int s_iCurrentConvLine;
	static void StopConversation();
	static bool StartConversation(int iConversation, CASW_Marine *pMarine, CASW_Marine *pSecondMarine = NULL);

	// has wildcat killed more aliens?
	bool GetWildcatAhead();

	// checks when a marine last fired to see if it's okay to do calm conversations
	bool AllowCalmConversations(int iConversation);

protected:
	// plays the specified sound on the marine and sets the next chatter time to just after that
	void InternalPlayChatter(CASW_Marine* pMarine, const char* szSoundName, int iSetTimer = ASW_CHATTER_TIMER_TEAM, int iChatterType=-1, int iSubChatter=-1, CBasePlayer *pOnlyForPlayer=NULL);

	CASW_Marine* m_pMarine;
	float m_fPersonalChatterTime;

	float m_fFriendlyFireTime;

	CUtlVector<CASW_Queued_Speech> m_SpeechQueue;
};

#endif /* ASW_MARINE_SPEECH_H */
