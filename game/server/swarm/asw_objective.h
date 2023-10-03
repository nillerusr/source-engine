#ifndef ASW_OBJECTIVEINFO_H
#define ASW_OBJECTIVEINFO_H
#ifdef _WIN32
#pragma once
#endif

#include "baseentity.h"


class CASW_Marine_Profile;
class CASW_Marine;
class CASW_Player;
class CASW_Alien;
class CASW_Egg;
class CASW_Alien_Goo;

// This class holds information about a particular mission objective


class CASW_Objective : public CBaseEntity
{
public:
	DECLARE_CLASS( CASW_Objective, CBaseEntity );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CASW_Objective();
	virtual ~CASW_Objective();

	virtual void AlienKilled(CBaseEntity* pAlien);
	virtual void MarineKilled(CASW_Marine* pMarine);
	virtual void EggKilled(CASW_Egg* pEgg);
	virtual void GooKilled(CASW_Alien_Goo* pGoo);
	virtual void MissionStarted(void);
	virtual void MissionFail(void);
	virtual void MissionSuccess(void);
	virtual void SetComplete(bool bComplete);
	virtual void SetFailed(bool bFailed);

	virtual int	 ShouldTransmit( const CCheckTransmitInfo *pInfo );
	virtual bool KeyValue( const char *szKeyName, const char *szValue );

	virtual bool IsObjectiveComplete() { return m_bComplete; }
	virtual bool IsObjectiveFailed() { return m_bFailed; }
	virtual bool IsObjectiveOptional() { return m_bOptional; }
	virtual bool IsObjectiveDummy() { return m_bDummy; }
	virtual bool IsObjectiveHidden() { return !m_bVisible; }
	virtual float GetObjectiveProgress() { return IsObjectiveComplete() ? 1.0f : 0.0f; }


	CNetworkString( m_ObjectiveTitle, 255 );
	CNetworkString( m_ObjectiveDescription1, 255 );
	CNetworkString( m_ObjectiveDescription2, 255 );
	CNetworkString( m_ObjectiveDescription3, 255 );
	CNetworkString( m_ObjectiveDescription4, 255 );
	CNetworkString( m_ObjectiveImage, 255 );
	CNetworkString( m_ObjectiveMarkerName, 255 );
	CNetworkString( m_ObjectiveInfoIcon1, 255 );
	CNetworkString( m_ObjectiveInfoIcon2, 255 );
	CNetworkString( m_ObjectiveInfoIcon3, 255 );
	CNetworkString( m_ObjectiveInfoIcon4, 255 );
	CNetworkString( m_ObjectiveInfoIcon5, 255 );
	CNetworkString( m_ObjectiveIcon, 255 );
	CNetworkString( m_MapMarkings, 255 );
	CNetworkVar( bool,  m_bComplete );
	CNetworkVar( bool,  m_bFailed );
	CNetworkVar( bool,  m_bOptional );
	CNetworkVar( bool,  m_bDummy );
	CNetworkVar( bool,  m_bVisible);
	CNetworkVar( int, m_Priority );
	
	COutputEvent m_OnObjectiveComplete;
	void InputSetVisible( inputdata_t &inputdata );
};

#endif /* ASW_OBJECTIVEINFO_H */