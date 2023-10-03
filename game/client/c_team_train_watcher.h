//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef C_TEAM_TRAIN_WATCHER_H
#define C_TEAM_TRAIN_WATCHER_H
#ifdef _WIN32
#pragma once
#endif

#include "c_baseentity.h"

class C_TeamTrainWatcher : public C_BaseEntity
{
	DECLARE_CLASS( C_TeamTrainWatcher, C_BaseEntity );
public:
	DECLARE_CLIENTCLASS();

	C_TeamTrainWatcher();
	~C_TeamTrainWatcher();

	virtual void OnPreDataChanged( DataUpdateType_t updateType );
	virtual void OnDataChanged( DataUpdateType_t updateType );

	float GetTotalProgress( void ) { return m_flTotalProgress; }
	int GetSpeedLevel( void ) { return m_iTrainSpeedLevel; }

private:

	// === Networked Data ===

	// percent distance to cp1, distance to cp2, etc
	// adds up to 1.0
	//CNetworkArray( float, m_flDistances, MAX_CONTROL_POINTS );

	// current total progress, percentage
	CNetworkVar( float, m_flTotalProgress );
	float m_flOldProgress;

	CNetworkVar( int, m_iTrainSpeedLevel );
	int m_iOldTrainSpeedLevel;

	CNetworkVar( float, m_flRecedeTime );
	float m_flOldRecedeTime;

	CNetworkVar( int, m_nNumCappers );
	int m_nOldNumCappers;
};

extern C_TeamTrainWatcher *g_pTrainWatcher;

inline C_TeamTrainWatcher *TrainWatcher()
{
	return g_pTrainWatcher;
}

#endif //C_TEAM_TRAIN_WATCHER_H