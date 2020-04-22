//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//

#ifndef DOD_AREA_CAPTURE_H
#define DOD_AREA_CAPTURE_H
#ifdef _WIN32
#pragma once
#endif

#include "triggers.h"
#include "dod_control_point.h"

#define AREA_ATTEND_TIME 0.7f

#define AREA_THINK_TIME 0.1f

#define CAPTURE_NORMAL					0
#define CAPTURE_CATCHUP_ALIVEPLAYERS	1

#define MAX_CLIENT_AREAS 128

class CAreaCapture : public CBaseTrigger
{
public:
	DECLARE_CLASS( CAreaCapture, CBaseTrigger );

	virtual void Spawn( void );
	virtual void Precache( void );
	virtual bool KeyValue( const char *szKeyName, const char *szValue );

	void	area_SetIndex( int index );

	bool	IsActive( void );

	bool	CheckIfDeathCausesBlock( CDODPlayer *pVictim, CDODPlayer *pKiller );

private:
	void EXPORT AreaTouch( CBaseEntity *pOther );
	void	Think( void );

	void	StartCapture( int team, int capmode );
	void	EndCapture( int team );
	void	BreakCapture( bool bNotEnoughPlayers );
	void	SwitchCapture( int team );
	void	SendNumPlayers( CBasePlayer *pPlayer = NULL );

	void	SetOwner( int team );	//sets the owner of this point - useful for resetting all to -1
	
	void	InputEnable( inputdata_t &inputdata );
	void	InputDisable( inputdata_t &inputdata );
	void	InputRoundInit( inputdata_t &inputdata );

private:
	int		m_iCapMode;			//which capture mode we're in
	int		m_bCapturing;
	int		m_nCapturingTeam;	//the team that is capturing this point
	int		m_nOwningTeam;	//the team that has captured this point
	float	m_flCapTime;		//the total time it takes to capture the area, in seconds
	float	m_fTimeRemaining;	//the time left in the capture

	int		m_nAlliesNumCap;	//number of allies required to cap
	int		m_nAxisNumCap;		//number of axis required to cap

	int		m_nNumAllies;
	int		m_nNumAxis;

	bool	m_bAlliesCanCap;
	bool	m_bAxisCanCap;

	//used for catchup capping
	int		m_iCappingRequired;	//how many players are currently capping
	int		m_iCappingPlayers;	//how many are required?

	bool	m_bActive;

	COutputEvent m_AlliesStartOutput;
	COutputEvent m_AlliesBreakOutput;
	COutputEvent m_AlliesCapOutput;

	COutputEvent m_AxisStartOutput;
	COutputEvent m_AxisBreakOutput;
	COutputEvent m_AxisCapOutput;

	COutputEvent m_StartOutput;
	COutputEvent m_BreakOutput;
	COutputEvent m_CapOutput;

	int		m_iAreaIndex;	//index of this area among all other areas

	CControlPoint *m_pPoint;	//the capture point that we are linked to!

	bool	m_bRequiresObject;

	string_t m_iszCapPointName;			//name of the cap point that we're linked to

	int	m_iCapAttemptNumber;	// number used to keep track of discrete cap attempts, for block tracking

	DECLARE_DATADESC();
};

#endif //DOD_AREA_CAPTURE_H