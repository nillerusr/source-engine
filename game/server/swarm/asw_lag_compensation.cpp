#include "cbase.h"
#include "asw_lag_compensation.h"
#include "asw_player.h"
#include "asw_marine.h"
#include "inetchannelinfo.h"
#include "ai_basenpc.h"
#include "asw_game_resource.h"
#include "asw_marine_resource.h"
#include "asw_lag_compensation.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CUtlVector<CASW_Lag_Compensation*>	g_LagCompensatingEntities;
bool CASW_Lag_Compensation::s_bInLagCompensation = false;
CBasePlayer* CASW_Lag_Compensation::s_pLagCompensatingPlayer = NULL;

ConVar asw_alien_unlag("asw_alien_unlag", "1", 0, "Unlag alien positions by player's ping");
extern ConVar sv_maxunlag;
extern ConVar sv_showlagcompensation;

CASW_Lag_Compensation::CASW_Lag_Compensation()
{	
	g_LagCompensatingEntities.AddToTail(this);

	m_iPositionHistoryTail = 0;
	for (int i=0;i<ASW_LAG_NUM_POSITION_HISTORY_SAMPLES;i++)
	{
		m_vecPositionHistory[i] = Vector(0,0,0);
		m_fPositionHistoryTime[i] = 0;
	}	
	m_vecRealPosition = Vector(0,0,0);
	m_bSetRealPosition = false;
	m_fRealSimulationTime = 0;
	m_hOwnerEntity = NULL;	
}

CASW_Lag_Compensation::~CASW_Lag_Compensation()
{
	g_LagCompensatingEntities.FindAndRemove(this);
}

void CASW_Lag_Compensation::Init(CBaseAnimating *pOwner)
{
	m_hOwnerEntity = pOwner;	
}

CAI_BaseNPC *CASW_Lag_Compensation::GetOwnerNPC()
{
	if ( !m_hOwnerEntity.Get() || !m_hOwnerEntity->IsNPC() )
		return NULL;

	return static_cast<CAI_BaseNPC*>( m_hOwnerEntity.Get() );
}

void CASW_Lag_Compensation::StorePositionHistory()
{
	if ( !m_hOwnerEntity.Get() || !asw_alien_unlag.GetBool() )
		return;
	if ( GetOwnerNPC() && GetOwnerNPC()->GetSleepState() != AISS_AWAKE )
		return;
	if (gpGlobals->curtime - m_fPositionHistoryTime[m_iPositionHistoryTail] < ASW_LAG_MIN_SAMPLE_TIME_DIFFERENCE)
		return;
	
	m_iPositionHistoryTail++;
	if ( m_iPositionHistoryTail >= ASW_LAG_NUM_POSITION_HISTORY_SAMPLES)
		m_iPositionHistoryTail = 0;

	m_vecPositionHistory[m_iPositionHistoryTail] = m_hOwnerEntity->GetAbsOrigin();
	m_fPositionHistoryTime[m_iPositionHistoryTail] = gpGlobals->curtime;
}

const Vector& CASW_Lag_Compensation::GetLaggedPosition( const float fLaggedTime )
{
	if ( !m_hOwnerEntity.Get() )
		return vec3_origin;
	if ( GetOwnerNPC() && GetOwnerNPC()->GetSleepState() != AISS_AWAKE )
		return m_hOwnerEntity->GetAbsOrigin();

	int iCurrentIndex = m_iPositionHistoryTail;
	int iChosenIndex = -1;
	for (int i=0;i<ASW_LAG_NUM_POSITION_HISTORY_SAMPLES;i++)	// go through all our history samples and find the oldest one that's just past the lagged time requested
	{
		if (m_fPositionHistoryTime[iCurrentIndex] == 0)	// this isn't a real index
		{
			break;
		}
		iChosenIndex = iCurrentIndex;					// it's a real sample, so we might use this one
		if (m_fPositionHistoryTime[iCurrentIndex] <= fLaggedTime)		// if the sample is behind our lagged time, then stop, we'll use this one
		{
			break;
		}
		iCurrentIndex--;	// count the index back
		if (iCurrentIndex == -1)		// loop around if we need to
			iCurrentIndex = ASW_LAG_NUM_POSITION_HISTORY_SAMPLES-1;
	}

	static Vector vecResult;
	vecResult = m_hOwnerEntity->GetAbsOrigin();
	if (iChosenIndex != -1)		// if we found an index, then work out what % of the movement we need to do based on the difference between now and the lagged time
	{
		const float base = (gpGlobals->curtime - m_fPositionHistoryTime[iChosenIndex]);
		if (base <= 0)	// if our only sample is 'now' then we can't do any lag compensation
			return vecResult;
		const float fFraction = (gpGlobals->curtime - fLaggedTime) / base;
		vecResult = m_hOwnerEntity->GetAbsOrigin() + 
			(m_vecPositionHistory[iChosenIndex] - m_hOwnerEntity->GetAbsOrigin()) * fFraction;
	}
	return vecResult;
}

void CASW_Lag_Compensation::MoveToLaggedPosition(const float fLaggedTime)
{	
	if ( !m_hOwnerEntity.Get() )
		return;
	// if entity is attached to a marine, don't do lag compensation (fixes parasites detaching when the marine is shot)
	if ( m_hOwnerEntity->GetMoveParent() && m_hOwnerEntity->GetMoveParent()->Classify() == CLASS_ASW_MARINE )
		return;

	if ( GetOwnerNPC() && GetOwnerNPC()->GetSleepState() != AISS_AWAKE )
		return;

	int iCurrentIndex = m_iPositionHistoryTail;
	int iChosenIndex = -1;
	for (int i=0;i<ASW_LAG_NUM_POSITION_HISTORY_SAMPLES;i++)	// go through all our history samples and find the oldest one that's just past the lagged time requested
	{
		if (m_fPositionHistoryTime[iCurrentIndex] == 0)	// this isn't a real index
		{
			break;
		}
		iChosenIndex = iCurrentIndex;					// it's a real sample, so we might use this one
		if (m_fPositionHistoryTime[iCurrentIndex] <= fLaggedTime)		// if the sample is behind our lagged time, then stop, we'll use this one
		{
			break;
		}
		iCurrentIndex--;	// count the index back
		if (iCurrentIndex == -1)		// loop around if we need to
			iCurrentIndex = ASW_LAG_NUM_POSITION_HISTORY_SAMPLES-1;
	}

	if (iChosenIndex != -1)		// if we found an index, then work out what % of the movement we need to do based on the difference between now and the lagged time
	{
		const float base = (gpGlobals->curtime - m_fPositionHistoryTime[iChosenIndex]);
		if (base <= 0)	// if our only sample is 'now' then we can't do any lag compensation
			return;
		const float fFraction = (gpGlobals->curtime - fLaggedTime) / base;
		m_vecRealPosition = m_hOwnerEntity->GetAbsOrigin();	// store our real position, so we can restore it once lag compensation is done
		m_fRealSimulationTime = m_hOwnerEntity->GetSimulationTime();
		const Vector vecNewPos = m_vecRealPosition + 
							(m_vecPositionHistory[iChosenIndex] - m_vecRealPosition) * fFraction;
		
		m_bSetRealPosition = true;
		if (asw_alien_unlag.GetInt() < 2)
		{
			m_hOwnerEntity->SetAbsOrigin(vecNewPos);			// move us to the lagged position
		}
	}
}

void CASW_Lag_Compensation::UndoLaggedPosition()
{	
	if ( !m_hOwnerEntity.Get() )
		return;
	// if entity is attached to a marine, don't do lag compensation (fixes parasites detaching when the marine is shot)
	if ( m_hOwnerEntity->GetMoveParent() && m_hOwnerEntity->GetMoveParent()->Classify() == CLASS_ASW_MARINE )
		return;

	if ( m_bSetRealPosition )
	{
		m_hOwnerEntity->SetAbsOrigin(m_vecRealPosition);
		m_hOwnerEntity->SetSimulationTime(m_fRealSimulationTime);
		m_bSetRealPosition = false;
	}
}

Vector CASW_Lag_Compensation::GetLagCompensationOffset()
{
	if (!m_bSetRealPosition)
		return vec3_origin;

	return m_vecRealPosition - m_hOwnerEntity->GetAbsOrigin();
}

void CASW_Lag_Compensation::AllowLagCompensation(CBasePlayer *player)
{
	s_pLagCompensatingPlayer = player;
}

void CASW_Lag_Compensation::RequestLagCompensation(CASW_Player *player, const CUserCmd *cmd )
{	
	if (player != s_pLagCompensatingPlayer)
		return;

	if ( !player
		 || !player->m_bLagCompensation		// Player not wanting lag compensation
		 || (gpGlobals->maxClients <= 1)	// no lag compensation in single player
		 || !asw_alien_unlag.GetBool()				// disabled by server admin
		 || player->IsBot() 				// not for bots
		 //|| player->IsObserver()			// not for spectators
		)
		return;

	if (s_bInLagCompensation)
	{
		return;
	}

	s_bInLagCompensation = true;

	// Get true latency

	// correct is the amout of time we have to correct game time
	float correct = 0.0f;

	INetChannelInfo *nci = engine->GetPlayerNetInfo( player->entindex() ); 

	if ( nci )
	{
		// add network latency
		correct+= nci->GetLatency( FLOW_OUTGOING );
	}

	// calc number of view interpolation ticks - 1
	int lerpTicks = TIME_TO_TICKS( player->m_fLerpTime );

	// add view interpolation latency see C_BaseEntity::GetInterpolationAmount()
	correct += TICKS_TO_TIME( lerpTicks );
	
	// check bouns [0,sv_maxunlag]
	correct = clamp( correct, 0.0f, sv_maxunlag.GetFloat() );

	// correct tick send by player 
	int targettick = cmd->tick_count - lerpTicks;

	// calc difference between tick send by player and our latency based tick
	float deltaTime =  correct - TICKS_TO_TIME(gpGlobals->tickcount - targettick);

	if ( fabs( deltaTime ) > 0.2f )
	{
		// difference between cmd time and latency is too big > 200ms, use time correction based on latency
		// DevMsg("StartLagCompensation: delta too big (%.3f)\n", deltaTime );
		targettick = gpGlobals->tickcount - TIME_TO_TICKS( correct );
	}

	// check the player has enough lag to warrant doing the work of lag compensation
	const float fLaggedTime = TICKS_TO_TIME( targettick );
	if (gpGlobals->curtime - fLaggedTime < ASW_MIN_LAG_TIME)
		return;

	CASW_Marine *pMarine = player->GetMarine();

	for (int i=0;i<g_LagCompensatingEntities.Count();i++)
	{
		if ( g_LagCompensatingEntities[i]->m_hOwnerEntity.Get() == pMarine )		// don't lag compensate my own marine
			continue;

		g_LagCompensatingEntities[i]->MoveToLaggedPosition( fLaggedTime );

		if( sv_showlagcompensation.GetInt() == 1)
		{
			CBaseAnimating *pAnim = g_LagCompensatingEntities[i]->m_hOwnerEntity.Get();
			if (pAnim)
				pAnim->DrawServerHitboxes(4, true);
		}
	}
}

void CASW_Lag_Compensation::FinishLagCompensation()
{
	if (!s_bInLagCompensation)
	{
		return;
	}
	s_bInLagCompensation = false;
	s_pLagCompensatingPlayer = NULL;
	for (int i=0;i<g_LagCompensatingEntities.Count();i++)
	{
		g_LagCompensatingEntities[i]->UndoLaggedPosition();
	}
}