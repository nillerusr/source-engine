#include "cbase.h"
#include "asw_intro_control.h"
#include "util_shared.h"
#include "asw_gamerules.h"
#include "asw_player.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS( asw_intro_control, CASW_Intro_Control );

BEGIN_DATADESC( CASW_Intro_Control )
	DEFINE_INPUTFUNC( FIELD_VOID,	"LaunchCampaignMap",	InputLaunchCampaignMap ),
	DEFINE_INPUTFUNC( FIELD_VOID,	"ShowCredits",	InputShowCredits ),
	DEFINE_INPUTFUNC( FIELD_VOID,	"ShowCainMail",	InputShowCainMail ),
	DEFINE_INPUTFUNC( FIELD_VOID,	"CheckReconnect",	InputCheckReconnect ),	
	DEFINE_OUTPUT( m_IntroStarted,	"IntroStarted" ),
	DEFINE_THINKFUNC(OnIntroStarted),
	DEFINE_FIELD(m_bLaunchedCampaignMap, FIELD_BOOLEAN),
	DEFINE_FIELD(m_bShownCredits, FIELD_BOOLEAN),
END_DATADESC()

extern ConVar asw_default_mission;

void CASW_Intro_Control::Spawn()
{
	m_bLaunchedCampaignMap = false;
	BaseClass::Spawn();
	SetThink(&CASW_Intro_Control::OnIntroStarted);
	SetNextThink(gpGlobals->curtime + 1.0f);
}

void CASW_Intro_Control::InputLaunchCampaignMap( inputdata_t &inputdata )
{
	LaunchCampaignMap();
}

void CASW_Intro_Control::LaunchCampaignMap()
{
	if (m_bLaunchedCampaignMap || !ASWGameRules())
		return;

	ASWGameRules()->SetGameState(ASW_GS_CAMPAIGNMAP);

	CReliableBroadcastRecipientFilter filter;
	UserMessageBegin( filter, "LaunchCampaignMap" );
		
	MessageEnd();

	m_bLaunchedCampaignMap = true;
}

void CASW_Intro_Control::InputShowCredits( inputdata_t &inputdata )
{
	m_bShownCredits = true;
	CReliableBroadcastRecipientFilter filter;
	UserMessageBegin( filter, "LaunchCredits" );
		
	MessageEnd();
}

void CASW_Intro_Control::InputShowCainMail( inputdata_t &inputdata )
{
		// listen server goes back to the default mission
	if ( !engine->IsDedicatedServer() && ASWGameRules() && gpGlobals->maxClients > 1 )
	{
		CBasePlayer *pPlayer = UTIL_GetListenServerHost();
		if (pPlayer)
		{
			CSingleUserRecipientFilter filter(pPlayer);
			filter.MakeReliable();
			UserMessageBegin( filter, "LaunchCainMail" );
		
			MessageEnd();
		}
	}
	else
	{
		CReliableBroadcastRecipientFilter filter;
		UserMessageBegin( filter, "LaunchCainMail" );		
		MessageEnd();
	}
}
	

void CASW_Intro_Control::OnIntroStarted()
{
	CBasePlayer *pPlayer = UTIL_PlayerByIndex( 1 );	
	if (pPlayer == NULL)
	{
		SetThink(&CASW_Intro_Control::OnIntroStarted);
		SetNextThink(gpGlobals->curtime + 1.0f);
	}
	else
	{
		m_IntroStarted.FireOutput(pPlayer, this);
		SetThink(NULL);
	}
}

// tell clients to check for reconnecting to the previous server after finishing the outro
void CASW_Intro_Control::InputCheckReconnect( inputdata_t &inputdata )
{
	CheckReconnect();
}

void CASW_Intro_Control::CheckReconnect()
{
	// reconnect to the last server we were on (used when we're watching the outro as a singleplayer map after a multiplayer game)
	CReliableBroadcastRecipientFilter users;
	users.MakeReliable();
	UserMessageBegin( users, "ASWReconnectAfterOutro" );
	MessageEnd();
		
	if (!engine->IsDedicatedServer() && ASWGameRules())
	{
		// listen server goes back to the default mission
		if ( gpGlobals->maxClients > 1 )
		{
			CBasePlayer *pPlayer = UTIL_GetListenServerHost();
			if (pPlayer)
			{
				//engine->ChangeLevel(asw_default_mission.GetString(), NULL);
			}
		}
	}
}

// if we've already started showing the credits and another player has joined up, show him them too
void CASW_Intro_Control::PlayerSpawned(CASW_Player *pPlayer)
{
	Msg("CASW_Intro_Control::PlayerSpawned, showncredits = %d\n", m_bShownCredits);
	if (m_bShownCredits && pPlayer)
	{
		Msg("Sending user message to this player telling him to show the credits\n");
		CRecipientFilter filter;
		filter.AddRecipient(pPlayer);
		UserMessageBegin( filter, "LaunchCredits" );			
		MessageEnd();
	}
}