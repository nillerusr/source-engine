//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef TEAM_TRAIN_WATCHER_H
#define TEAM_TRAIN_WATCHER_H
#ifdef _WIN32
#pragma once
#endif

#include "cbase.h"
#include "trigger_area_capture.h"

class CFuncTrackTrain;
class CPathTrack;
class CTeamControlPoint;

#define TF_TRAIN_ALERT_DISTANCE			750   // alert is the VO warning
#define TF_TRAIN_ALARM_DISTANCE			200   // alarm is the looping sound played at the control point

#define TF_TRAIN_ATTACK_ALERT			"Announcer.Cart.AttackWarning"
#define TF_TRAIN_DEFEND_ALERT			"Announcer.Cart.DefendWarning"
#define TF_TRAIN_ATTACK_FINAL_ALERT		"Announcer.Cart.AttackFinalWarning"
#define TF_TRAIN_DEFEND_FINAL_ALERT		"Announcer.Cart.DefendFinalWarning"
#define TF_TRAIN_ALARM					"Cart.Warning"

class CTeamTrainWatcher : public CBaseEntity
{
	DECLARE_CLASS( CTeamTrainWatcher, CBaseEntity );
public:
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CTeamTrainWatcher();

	virtual int UpdateTransmitState();

	void InputRoundActivate( inputdata_t &inputdata );
	void InputEnable( inputdata_t &inputdata );
	void InputDisable( inputdata_t &inputdata );

	void InputSetNumTrainCappers( inputdata_t &inputdata );
	void InputOnStartOvertime( inputdata_t &inputdata );

	// ==========================================================
	// given a start node and a list of goal nodes
	// calculate the distance between each
	// ==========================================================
	void WatcherActivate( void );

	void WatcherThink( void );

	CBaseEntity *GetTrainEntity( void );
	bool IsDisabled( void ) { return m_bDisabled; }

	bool TimerMayExpire( void );

	void StopCaptureAlarm( void );

private:

	void StartCaptureAlarm( CTeamControlPoint *pPoint );
	void PlayCaptureAlert( CTeamControlPoint *pPoint, bool bFinalPointInMap );

private:

	bool m_bDisabled;

	// === Data ===

	// pointer to the train that we're checking
	CHandle<CFuncTrackTrain> m_hTrain;

	// start node
	CHandle<CPathTrack>	m_hStartNode;

	// goal node
	CHandle<CPathTrack>	m_hGoalNode;

	string_t m_iszTrain;
	string_t m_iszStartNode;
	string_t m_iszGoalNode;

	// list of node associations with control points
	typedef struct 
	{
		CHandle<CPathTrack>	hPathTrack;
		CHandle<CTeamControlPoint> hCP;
		float flDistanceFromStart;
		bool bAlertPlayed;
	} node_cp_pair_t;

	node_cp_pair_t m_CPLinks[MAX_CONTROL_POINTS];
	int m_iNumCPLinks;

	string_t m_iszLinkedPathTracks[MAX_CONTROL_POINTS];
	string_t m_iszLinkedCPs[MAX_CONTROL_POINTS];

	float m_flTotalPathDistance;	// calculated only at round start, node graph
	// may get chopped as the round progresses

	float m_flSpeedLevels[3];

	// === Networked Data ===

	// current total progress, percentage
	CNetworkVar( float, m_flTotalProgress );

	CNetworkVar( int, m_iTrainSpeedLevel );

	CNetworkVar( int, m_nNumCappers );

	bool m_bWaitingToRecede;
	CNetworkVar( float, m_flRecedeTime );
	float m_flRecedeTotalTime;
	float m_flRecedeStartTime;
	COutputEvent m_OnTrainStartRecede;

	bool m_bCapBlocked;

	float m_flNextSpeakForwardConceptTime; // used to have players speak the forward concept every X seconds
	CHandle<CTriggerAreaCapture> m_hAreaCap;
};

#endif //TEAM_TRAIN_WATCHER_H
