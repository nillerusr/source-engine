//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose:
//
//===========================================================================//

#include "cbase.h"
#include "team_train_watcher.h"
#include "team_control_point.h"
#include "trains.h"
#include "team_objectiveresource.h"
#include "teamplayroundbased_gamerules.h"
#include "team_control_point.h"
#include "team_control_point_master.h"
#include "engine/IEngineSound.h"
#include "soundenvelope.h"
#include "mp_shareddefs.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static CSoundPatch *g_pAlarm = NULL;

BEGIN_DATADESC( CTeamTrainWatcher )

	// Inputs.
	DEFINE_INPUTFUNC( FIELD_VOID, "RoundActivate", InputRoundActivate ),
	DEFINE_INPUTFUNC( FIELD_INTEGER, "SetNumTrainCappers", InputSetNumTrainCappers ),
	DEFINE_INPUTFUNC( FIELD_VOID, "OnStartOvertime", InputOnStartOvertime ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Enable", InputEnable ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Disable", InputDisable ),

	// Outputs
	DEFINE_OUTPUT( m_OnTrainStartRecede, "OnTrainStartRecede" ),

	// key
	DEFINE_KEYFIELD( m_iszTrain, FIELD_STRING, "train" ),
	DEFINE_KEYFIELD( m_iszStartNode, FIELD_STRING, "start_node" ),
	DEFINE_KEYFIELD( m_iszGoalNode, FIELD_STRING, "goal_node" ),

	DEFINE_KEYFIELD( m_iszLinkedPathTracks[0], FIELD_STRING, "linked_pathtrack_1" ),
	DEFINE_KEYFIELD( m_iszLinkedCPs[0], FIELD_STRING, "linked_cp_1" ),

	DEFINE_KEYFIELD( m_iszLinkedPathTracks[1], FIELD_STRING, "linked_pathtrack_2" ),
	DEFINE_KEYFIELD( m_iszLinkedCPs[1], FIELD_STRING, "linked_cp_2" ),

	DEFINE_KEYFIELD( m_iszLinkedPathTracks[2], FIELD_STRING, "linked_pathtrack_3" ),
	DEFINE_KEYFIELD( m_iszLinkedCPs[2], FIELD_STRING, "linked_cp_3" ),

	DEFINE_KEYFIELD( m_iszLinkedPathTracks[3], FIELD_STRING, "linked_pathtrack_4" ),
	DEFINE_KEYFIELD( m_iszLinkedCPs[3], FIELD_STRING, "linked_cp_4" ),

	// can be up to 8 links

	// min speed for train hud speed levels
	DEFINE_KEYFIELD( m_flSpeedLevels[0], FIELD_FLOAT, "hud_min_speed_level_1" ),
	DEFINE_KEYFIELD( m_flSpeedLevels[1], FIELD_FLOAT, "hud_min_speed_level_2" ),
	DEFINE_KEYFIELD( m_flSpeedLevels[2], FIELD_FLOAT, "hud_min_speed_level_3" ),

	DEFINE_KEYFIELD( m_bDisabled,	FIELD_BOOLEAN,	"StartDisabled" ),

END_DATADESC()


IMPLEMENT_SERVERCLASS_ST_NOBASE(CTeamTrainWatcher, DT_TeamTrainWatcher)

	SendPropFloat( SENDINFO( m_flTotalProgress ), 11, 0, 0.0f, 1.0f ),
	SendPropInt( SENDINFO( m_iTrainSpeedLevel ), 4 ),
	SendPropTime( SENDINFO( m_flRecedeTime ) ),
	SendPropInt( SENDINFO( m_nNumCappers ), 5, SPROP_UNSIGNED ),

END_SEND_TABLE()


LINK_ENTITY_TO_CLASS( team_train_watcher, CTeamTrainWatcher );

CTeamTrainWatcher::CTeamTrainWatcher()
{
	m_bDisabled = false;
	m_flRecedeTime = 0;
	m_bWaitingToRecede = false;
	m_bCapBlocked = false;

	m_flNextSpeakForwardConceptTime = 0;
	m_hAreaCap = NULL;
}

int CTeamTrainWatcher::UpdateTransmitState()
{
	if ( m_bDisabled )
	{
		return SetTransmitState( FL_EDICT_DONTSEND );
	}

	return SetTransmitState( FL_EDICT_ALWAYS );
}

void CTeamTrainWatcher::InputRoundActivate( inputdata_t &inputdata )
{
	StopCaptureAlarm();

	if ( !m_bDisabled )
	{
		WatcherActivate();
	}
}

void CTeamTrainWatcher::InputEnable( inputdata_t &inputdata )
{
	StopCaptureAlarm();

	m_bDisabled = false;

	WatcherActivate();

	UpdateTransmitState();
}

void CTeamTrainWatcher::InputDisable( inputdata_t &inputdata )
{
	StopCaptureAlarm();

	m_bDisabled = true;
	SetThink( NULL );

	m_bWaitingToRecede = false;

	UpdateTransmitState();
}

ConVar tf_escort_recede_time( "tf_escort_recede_time", "30", 0, "", true, 0, false, 0 );
ConVar tf_escort_recede_time_overtime( "tf_escort_recede_time_overtime", "5", 0, "", true, 0, false, 0 );

void CTeamTrainWatcher::InputSetNumTrainCappers( inputdata_t &inputdata )
{
	if ( IsDisabled() )
	{
		return;
	}

	int iNumCappers = inputdata.value.Int();
	m_nNumCappers = iNumCappers;

	// inputdata.pCaller is hopefully an area capture
	// lets see if its blocked, and not start receding if it is
	CTriggerAreaCapture *pAreaCap = dynamic_cast<CTriggerAreaCapture *>(inputdata.pCaller);
	if ( pAreaCap )
	{
		m_bCapBlocked = pAreaCap->IsBlocked();
		m_hAreaCap = pAreaCap;
	}

	if ( iNumCappers <= 0 && !m_bCapBlocked )
	{
		if ( !m_bWaitingToRecede )
		{
			// start receding in [tf_escort_cart_recede_time] seconds
			m_bWaitingToRecede = true;

			if ( TeamplayRoundBasedRules() && TeamplayRoundBasedRules()->InOvertime() )
			{
				m_flRecedeTotalTime = tf_escort_recede_time_overtime.GetFloat();
			}
			else
			{
				m_flRecedeTotalTime = tf_escort_recede_time.GetFloat();
			}

			m_flRecedeStartTime = gpGlobals->curtime;
			m_flRecedeTime = m_flRecedeStartTime + m_flRecedeTotalTime;
		}		
	}
	else
	{
		// cancel receding
		m_bWaitingToRecede = false;
		m_flRecedeTime = 0;
	}
}

void CTeamTrainWatcher::InputOnStartOvertime( inputdata_t &inputdata )
{
	// recalculate the recede time
	if ( m_bWaitingToRecede )
	{
		float flRecedeTimeRemaining = m_flRecedeTime - gpGlobals->curtime;
		float flOvertimeRecedeLen = tf_escort_recede_time_overtime.GetFloat();

		// drop to overtime recede time if it's more than that
		if ( flRecedeTimeRemaining > flOvertimeRecedeLen )
		{
			m_flRecedeTotalTime = flOvertimeRecedeLen;
			m_flRecedeStartTime = gpGlobals->curtime;
			m_flRecedeTime = m_flRecedeStartTime + m_flRecedeTotalTime;
		}
	}
}

// ==========================================================
// given a start node and a list of goal nodes
// calculate the distance between each
// ==========================================================
void CTeamTrainWatcher::WatcherActivate( void )
{		
	m_flRecedeTime = 0;
	m_bWaitingToRecede = false;
	m_bCapBlocked = false;
	m_flNextSpeakForwardConceptTime = 0;
	m_hAreaCap = NULL;

	StopCaptureAlarm();

	// init our train
	m_hTrain = dynamic_cast<CFuncTrackTrain*>( gEntList.FindEntityByName( NULL, m_iszTrain ) );
	if ( !m_hTrain )
	{
		Warning("%s failed to find train named '%s'\n", GetClassname(), STRING(m_iszTrain) );
	}

	// init our array of path_tracks linked to control points
	m_iNumCPLinks = 0;

	int i;
	for ( i=0;i<MAX_CONTROL_POINTS;i++ )
	{
		CPathTrack *pPathTrack = dynamic_cast<CPathTrack*>( gEntList.FindEntityByName( NULL, m_iszLinkedPathTracks[i] ) );
		CTeamControlPoint *pCP = dynamic_cast<CTeamControlPoint*>( gEntList.FindEntityByName( NULL, m_iszLinkedCPs[i] ) );
		if ( pPathTrack && pCP )
		{
			m_CPLinks[m_iNumCPLinks].hPathTrack = pPathTrack;
			m_CPLinks[m_iNumCPLinks].hCP = pCP;
			m_CPLinks[m_iNumCPLinks].flDistanceFromStart = 0;	// filled in when we parse the nodes
			m_CPLinks[m_iNumCPLinks].bAlertPlayed = false;
			m_iNumCPLinks++;
		}
	}

	// init our start and goal nodes
	m_hStartNode = dynamic_cast<CPathTrack*>( gEntList.FindEntityByName( NULL, m_iszStartNode ) );
	if ( !m_hStartNode )
	{
		Warning("%s failed to find path_track named '%s'\n", GetClassname(), STRING(m_iszStartNode) );
	}

	m_hGoalNode = dynamic_cast<CPathTrack*>( gEntList.FindEntityByName( NULL, m_iszGoalNode ) );
	if ( !m_hGoalNode )
	{
		Warning("%s failed to find path_track named '%s'\n", GetClassname(), STRING(m_iszGoalNode) );
	}

	m_flTotalPathDistance = 0.0f;

	if( m_hStartNode.Get() && m_hGoalNode.Get() )
	{
		CPathTrack *pNode = m_hStartNode;
		CPathTrack *pPrev = pNode;
		pNode = pNode->GetNext();

		// don't check the start node for links. If it's linked, it will have 0 distance anyway

		while ( pNode )
		{
			Vector dir = pNode->GetLocalOrigin() - pPrev->GetLocalOrigin();
			float length = dir.Length();

			m_flTotalPathDistance += length;

			// if pNode is one of our cp nodes, store its distance from m_hStartNode
			for ( i=0;i<m_iNumCPLinks;i++ )
			{
				if ( m_CPLinks[i].hPathTrack == pNode )
				{
					m_CPLinks[i].flDistanceFromStart = m_flTotalPathDistance;
					break;
				}
			}

			if ( pNode == m_hGoalNode )
				break;

			pPrev = pNode;
			pNode = pNode->GetNext();
		}
	}

	// We have total distance and increments in our links array
	for ( i=0;i<m_iNumCPLinks;i++ )
	{
		int iCPIndex = m_CPLinks[i].hCP.Get()->GetPointIndex();
		ObjectiveResource()->SetTrainPathDistance( iCPIndex, m_CPLinks[i].flDistanceFromStart / m_flTotalPathDistance );
		DevMsg( "link %d = %.2f\n", iCPIndex, m_CPLinks[i].flDistanceFromStart / m_flTotalPathDistance );
	}

	SetThink( &CTeamTrainWatcher::WatcherThink );
	SetNextThink( gpGlobals->curtime + 0.1f );
}

void CTeamTrainWatcher::StopCaptureAlarm( void )
{
	if ( g_pAlarm )
	{
		CSoundEnvelopeController::GetController().SoundDestroy( g_pAlarm );
		g_pAlarm = NULL;
	}
}

void CTeamTrainWatcher::StartCaptureAlarm( CTeamControlPoint *pPoint )
{
	StopCaptureAlarm();

	if ( pPoint )
	{
		CReliableBroadcastRecipientFilter filter;
		CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();
		g_pAlarm = controller.SoundCreate( filter, pPoint->entindex(), CHAN_STATIC, TF_TRAIN_ALARM, ATTN_NORM );
		controller.Play( g_pAlarm, 1.0, PITCH_NORM );
	}
}

void CTeamTrainWatcher::PlayCaptureAlert( CTeamControlPoint *pPoint, bool bFinalPointInMap )
{
	if ( !pPoint )
		return;

	if ( TeamplayRoundBasedRules() )
	{
		TeamplayRoundBasedRules()->PlayTrainCaptureAlert( pPoint, bFinalPointInMap );
	}
}

void CTeamTrainWatcher::WatcherThink( void )
{
	if ( m_bWaitingToRecede )
	{
		if ( m_flRecedeTime < gpGlobals->curtime )
		{
			m_bWaitingToRecede = false;

			// don't actually recede in overtime
			if ( TeamplayRoundBasedRules() && !TeamplayRoundBasedRules()->InOvertime() )
			{
				// fire recede output
				m_OnTrainStartRecede.FireOutput( this, this );
			}
		}
	}

	if ( TeamplayRoundBasedRules() && TeamplayRoundBasedRules()->State_Get() == GR_STATE_TEAM_WIN )
	{
		StopCaptureAlarm();
	}

	// given its next node, we can walk the nodes and find the linear
	// distance to the next cp node, or to the goal node

	CFuncTrackTrain *pTrain = m_hTrain;
	if ( pTrain )
	{
		int iOldTrainSpeedLevel = m_iTrainSpeedLevel;

		// how fast is the train moving?
		float flSpeed = pTrain->GetDesiredSpeed();

		// divide speed into regions
		// anything negative is -1

		if ( flSpeed < 0 )
		{
			m_iTrainSpeedLevel = -1;

			// even though our desired speed might be negative,
			// our actual speed might be zero if we're at a dead end...
			// this will turn off the < image when the train is done moving backwards
			if ( pTrain->GetCurrentSpeed() == 0 )
			{
				m_iTrainSpeedLevel = 0;
			}
		}
		else if ( flSpeed > m_flSpeedLevels[2] )
		{
			m_iTrainSpeedLevel = 3;
		}
		else if ( flSpeed > m_flSpeedLevels[1] )
		{
			m_iTrainSpeedLevel = 2;
		}
		else if ( flSpeed > m_flSpeedLevels[0] )
		{
			m_iTrainSpeedLevel = 1;
		}
		else
		{
			m_iTrainSpeedLevel = 0;
		}

		// play any concepts that we might need to play
		if ( m_iTrainSpeedLevel != iOldTrainSpeedLevel )
		{	
			if ( TeamplayRoundBasedRules() )
			{
				if ( m_iTrainSpeedLevel == 0 && iOldTrainSpeedLevel != 0 )
				{
					TeamplayRoundBasedRules()->HaveAllPlayersSpeakConceptIfAllowed( MP_CONCEPT_CART_STOP );
					m_flNextSpeakForwardConceptTime = 0;
				}
				else if ( m_iTrainSpeedLevel < 0 && iOldTrainSpeedLevel == 0 )
				{
					TeamplayRoundBasedRules()->HaveAllPlayersSpeakConceptIfAllowed( MP_CONCEPT_CART_MOVING_BACKWARD );
					m_flNextSpeakForwardConceptTime = 0;
				}
			}
		}

		if ( m_iTrainSpeedLevel > 0 && m_flNextSpeakForwardConceptTime < gpGlobals->curtime )
		{
			if ( m_hAreaCap.Get() )
			{
				for ( int i = 1; i <= gpGlobals->maxClients; i++ )
				{
					CBaseMultiplayerPlayer *pPlayer = ToBaseMultiplayerPlayer( UTIL_PlayerByIndex( i ) );
					if ( pPlayer )
					{
						if ( m_hAreaCap->IsTouching( pPlayer ) )
						{
							pPlayer->SpeakConceptIfAllowed( MP_CONCEPT_CART_MOVING_FORWARD );
						}
					}
				}
			}

			m_flNextSpeakForwardConceptTime = gpGlobals->curtime + 3.0;
		}

		// what percent progress are we at?
		CPathTrack *pNode = ( pTrain->m_ppath ) ? pTrain->m_ppath->GetNext() : NULL;

		// if we're moving backwards, GetNext is going to be wrong
		if ( flSpeed < 0 )
		{
			pNode = pTrain->m_ppath;
		}

		if ( pNode )
		{
			float flDistanceToGoal = 0;

			// distance to next node
			Vector vecDir = pNode->GetLocalOrigin() - pTrain->GetLocalOrigin();
			flDistanceToGoal = vecDir.Length();

			// distance of next node to goal node
			if ( pNode && pNode != m_hGoalNode )
			{
				// walk this until we get to goal node, or a dead end
				CPathTrack *pPrev = pNode;
				pNode = pNode->GetNext();
				while ( pNode )
				{
					vecDir = pNode->GetLocalOrigin() - pPrev->GetLocalOrigin();
					flDistanceToGoal += vecDir.Length();

					if ( pNode == m_hGoalNode )
						break;

					pPrev = pNode;
					pNode = pNode->GetNext();
				}
			}

			if ( m_flTotalPathDistance <= 0 )
			{
				Assert( !"No path distance in team_train_watcher\n" );
				m_flTotalPathDistance = 1;
			}

			m_flTotalProgress = clamp( 1.0 - ( flDistanceToGoal / m_flTotalPathDistance ), 0.0, 1.0 );

			float flTrainDistanceFromStart = m_flTotalPathDistance - flDistanceToGoal;

			// play alert sounds if necessary
			for ( int iCount = 0 ; iCount < m_iNumCPLinks ; iCount++ )
			{
				if ( flTrainDistanceFromStart < m_CPLinks[iCount].flDistanceFromStart - TF_TRAIN_ALERT_DISTANCE )
				{
					// back up twice the alert distance before resetting our flag to play the warning again
					if ( flTrainDistanceFromStart < m_CPLinks[iCount].flDistanceFromStart - ( TF_TRAIN_ALERT_DISTANCE * 2 ) )
					{
						// reset our alert flag
						m_CPLinks[iCount].bAlertPlayed = false;
					}
				}
				else
				{
					if ( flTrainDistanceFromStart < m_CPLinks[iCount].flDistanceFromStart && !m_CPLinks[iCount].bAlertPlayed )
					{
						m_CPLinks[iCount].bAlertPlayed = true;

						CTeamControlPoint *pLastPoint = NULL;
						CTeamControlPoint *pCurrentPoint = m_CPLinks[iCount].hCP.Get();
						CTeamControlPointMaster *pMaster = g_hControlPointMasters.Count() ? g_hControlPointMasters[0] : NULL;
						if ( pMaster )
						{
							pLastPoint = pMaster->GetControlPoint( pMaster->GetNumPoints() - 1 );
						}

						bool bFinalPointInMap = pLastPoint && ( pLastPoint == pCurrentPoint );

						PlayCaptureAlert( pCurrentPoint, bFinalPointInMap );
					}
				}
			}

			// check to see if we need to start or stop the alarm
			if ( flDistanceToGoal <= TF_TRAIN_ALARM_DISTANCE )
			{
				if ( !g_pAlarm && m_iNumCPLinks > 0 )
				{
					// start the alarm at the final point
					StartCaptureAlarm( m_CPLinks[m_iNumCPLinks-1].hCP.Get() );
				}
			}
			else
			{
				if ( g_pAlarm )
				{
					StopCaptureAlarm();
				}
			}
		}
	}

	SetNextThink( gpGlobals->curtime + 0.1 );
}


CBaseEntity *CTeamTrainWatcher::GetTrainEntity( void )
{
	return m_hTrain.Get();
}

bool CTeamTrainWatcher::TimerMayExpire( void )
{
	if ( IsDisabled() )
	{
		return true;
	}

	// Still in overtime if we're waiting to recede
	if ( m_bWaitingToRecede )
		return false;

	// capture blocked so we're not receding, but game shouldn't end
	if ( m_bCapBlocked )
		return false;

	// not waiting, so we're capping, in which case the area capture
	// will not let us expire
	return true;
}
