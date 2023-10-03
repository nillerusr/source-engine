//========= Copyright © 1996-2005, Valve LLC, All rights reserved. ============
//
//=============================================================================
#ifndef BASEMULTIPLAYERPLAYER_H
#define BASEMULTIPLAYERPLAYER_H
#pragma once

#include "player.h"
#include "ai_speech.h"

enum SpeechPriorityType
{
	SPEECH_PRIORITY_LOW,
	SPEECH_PRIORITY_NORMAL,
	SPEECH_PRIORITY_MANUAL,
	SPEECH_PRIORITY_UNINTERRUPTABLE,
};
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CBaseMultiplayerPlayer : public CAI_ExpresserHost<CBasePlayer>
{

	DECLARE_CLASS( CBaseMultiplayerPlayer, CAI_ExpresserHost<CBasePlayer> );

public:

	CBaseMultiplayerPlayer();
	~CBaseMultiplayerPlayer();

	virtual void		Spawn( void );

	virtual void		PostConstructor( const char *szClassname );
	virtual void		ModifyOrAppendCriteria( AI_CriteriaSet& criteriaSet );

	virtual bool		SpeakIfAllowed( AIConcept_t concept, SpeechPriorityType priority, const char *modifiers = NULL, char *pszOutResponseChosen = NULL, size_t bufsize = 0, IRecipientFilter *filter = NULL );
	void				SpeakConcept( AI_Response &outresponse, int iConcept );
	virtual bool		SpeakConceptIfAllowed( int iConcept, const char *modifiers = NULL, char *pszOutResponseChosen = NULL, size_t bufsize = 0, IRecipientFilter *filter = NULL );

	virtual bool		CanHearAndReadChatFrom( CBasePlayer *pPlayer );
	virtual bool		CanSpeak( void ) { return true; }

	virtual void		Precache( void )
	{
#ifndef SWARM_DLL
		PrecacheParticleSystem( "achieved" );
#endif

		BaseClass::Precache();
	}

	virtual bool		ClientCommand( const CCommand &args );

	virtual bool		CanSpeakVoiceCommand( void ) { return true; }
	virtual bool		ShouldShowVoiceSubtitleToEnemy( void );
	virtual void		NoteSpokeVoiceCommand( const char *pszScenePlayed ) {}

	virtual void OnAchievementEarned( int iAchievement ) {}

	enum
	{
		CHAT_IGNORE_NONE = 0,
		CHAT_IGNORE_ALL,
		CHAT_IGNORE_TEAM,
	};

	int m_iIgnoreGlobalChat;

	//---------------------------------
	// Speech support
	virtual CAI_Expresser *GetExpresser() { return m_pExpresser; }
	virtual CMultiplayer_Expresser *GetMultiplayerExpresser() { return m_pExpresser; }

	void SetLastForcedChangeTeamTimeToNow( void ) { m_flLastForcedChangeTeamTime = gpGlobals->curtime; }
	float GetLastForcedChangeTeamTime( void ) { return m_flLastForcedChangeTeamTime; }

	void SetTeamBalanceScore( int iScore ) { m_iBalanceScore = iScore; }
	int GetTeamBalanceScore( void ) { return m_iBalanceScore; }

	virtual int	CalculateTeamBalanceScore( void );

	void AwardAchievement( int iAchievement );
	int	GetPerLifeCounterKV( const char *name );
	void SetPerLifeCounterKV( const char *name, int value );
	void ResetPerLifeCounters( void );

	KeyValues *GetPerLifeCounterKeys( void ) { return m_pAchievementKV; }

	void EscortScoringThink( void );
	void StartScoringEscortPoints( float flRate );
	void StopScoringEscortPoints( void );
	float m_flAreaCaptureScoreAccumulator;
	float m_flCapPointScoreRate;

	float GetConnectionTime( void ) { return m_flConnectionTime; }

#if !defined(NO_STEAM)
	//----------------------------
	// Steam handling
	bool		GetSteamID( CSteamID *pID );
	uint64		GetSteamIDAsUInt64( void );
#endif

protected:
	virtual CAI_Expresser *CreateExpresser( void );

	int		m_iCurrentConcept;
private:
	//---------------------------------
	CMultiplayer_Expresser		*m_pExpresser;

	float m_flConnectionTime;
	float m_flLastForcedChangeTeamTime;

	int m_iBalanceScore;	// a score used to determine which players are switched to balance the teams

	KeyValues	*m_pAchievementKV;
};

//-----------------------------------------------------------------------------
// Inline methods
//-----------------------------------------------------------------------------
inline CBaseMultiplayerPlayer *ToBaseMultiplayerPlayer( CBaseEntity *pEntity )
{
	if ( !pEntity || !pEntity->IsPlayer() )
		return NULL;
#if _DEBUG
	return dynamic_cast<CBaseMultiplayerPlayer *>( pEntity );
#else
	return static_cast<CBaseMultiplayerPlayer *>( pEntity );
#endif
}

#endif	// BASEMULTIPLAYERPLAYER_H
