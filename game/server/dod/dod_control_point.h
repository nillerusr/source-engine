//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//

#ifndef DOD_CONTROL_POINT_H
#define DOD_CONTROL_POINT_H
#ifdef _WIN32
#pragma once
#endif

#include "dod_player.h"

#define CAP_ICON_ALLIES_FLAG	1
#define CAP_ICON_BRIT_FLAG		27	//from dod_objectives.cpp

#define CAP_POINT_HIDEFLAG		(1<<0)
#define CAP_POINT_HIDE_MODEL	(1<<1)
#define CAP_POINT_TICK_FOR_BOMBS_REMAINING	(1<<2)

#define PLAYER_POINTS_FOR_CAP	1
#define PLAYER_POINTS_FOR_BLOCK 1
#define PLAYER_POINTS_FOR_BOMB_PLANT	1
#define PLAYER_POINTS_FOR_BOMB_EXPLODED	3


class CControlPoint : public CBaseAnimating
{

public:
	DECLARE_CLASS( CControlPoint, CBaseAnimating );
	DECLARE_DATADESC();

	CControlPoint();

	virtual void Spawn( void );
	virtual bool KeyValue( const char *szKeyName, const char *szValue );
	virtual void Precache( void );

	void		Reset( void );

	//Inputs
	inline void Enable( inputdata_t &input )	{ SetActive( false ); }
	inline void Disable( inputdata_t &input )	{ SetActive( true ); }
	void		InputReset( inputdata_t &input );
	void		InputSetOwner( inputdata_t &input );

	void		InputShowModel( inputdata_t &input );
	void		InputHideModel( inputdata_t &input );

	int			PointValue( void );
	
	void		RoundRespawn( void );	//Mugsy - resetting
	void		TriggerTargets( void );

	void		SetActive( bool active );

	bool		PointIsVisible( void ) { return !( FBitSet( m_spawnflags, CAP_POINT_HIDEFLAG ) ); }

	void		SendCapString( int team, int iNumCappers, int *pCappingPlayers );

	void		SetOwner( int team, bool bMakeSound = true, int iNumCappers = 0, int *iCappingPlayers = NULL );
	int			GetOwner( void ) const;

	int			GetDefaultOwner( void ) const;

	inline const char *GetName( void ) { return STRING(m_iszPrintName); }
	int			GetCPGroup( void );
	int			GetPointIndex( void ) { return m_iPointIndex; }	//the mapper set index
	void		SetPointIndex( int index ) { m_iPointIndex = index; }
	int			GetAlliesIcon( void ) { return m_iAlliesIcon; }
	int			GetAxisIcon( void ) { return m_iAxisIcon; }
	int			GetNeutralIcon( void ) { return m_iNeutralIcon; }

	int			GetCurrentHudIconIndex( void );
	int			GetHudIconIndexForTeam( int team );
	int			GetTimerCapHudIcon( void );
	int			GetBombedHudIcon( void );

	inline bool	IsActive( void ) { return m_bActive; }

	void		SetNumCappersRequired( int alliesRequired, int axisRequired );

	void		CaptureBlocked( CDODPlayer *pPlayer );

	// Bomb interface
	void		BombPlanted( float flTimerLength, CDODPlayer *pPlantingPlayer );
	void		BombExploded( CDODPlayer *pPlantingPlayer = NULL, int iPlantingTeam = TEAM_UNASSIGNED );
	void		BombDisarmed( CDODPlayer *pDisarmingPlayer );
	void		CancelBombPlanted( void );

	int			GetBombsRemaining( void ) { return m_iBombsRemaining; }	// total bombs required
	int			GetBombsRequired( void ) { return m_iBombsRequired; }		// number of bombs remaining

private:
	void		InternalSetOwner( int team, bool bMakeSound = true, int iNumCappers = 0, int *iCappingPlayers = NULL );

	int			m_iTeam;			//0 - clear, 2 - allies, 3 - axis
	int			m_iDefaultOwner;	//team that initially owns the cap point
	int			m_iIndex;			//the index of this point in the controlpointArray

	string_t	m_iszPrintName;
	
	string_t	m_iszAlliesCapSound;	//the sound to play on cap
	string_t	m_iszAxisCapSound;
	string_t	m_iszResetSound;

	string_t	m_iszAlliesModel;		//models to set the ent to on capture
	string_t	m_iszAxisModel;
	string_t	m_iszResetModel;

	int			m_iAlliesModelBodygroup;//which bodygroup to use in the model
	int			m_iAxisModelBodygroup;
	int			m_iResetModelBodygroup;

	COutputEvent	m_AlliesCapOutput;	//outputs to fire when capped
	COutputEvent	m_AxisCapOutput;
	COutputEvent	m_PointResetOutput;

	COutputEvent m_OwnerChangedToAllies;
	COutputEvent m_OwnerChangedToAxis;

	int			m_iAlliesIcon;			//custom hud sprites for cap point
	int			m_iAxisIcon;
	int			m_iNeutralIcon;
	int			m_iTimerCapIcon;
	int			m_iBombedIcon;

	string_t	m_iszAlliesIcon;
	string_t	m_iszAxisIcon;
	string_t	m_iszNeutralIcon;
	string_t	m_iszTimerCapIcon;
	string_t    m_iszBombedIcon;

	int			m_bPointVisible;		//should this capture point be visible on the hud?
	int			m_iPointIndex;			//the mapper set index value of this control point

	int			m_iCPGroup;			//the group that this control point belongs to
	bool		m_bActive;			//

	string_t	m_iszName;				//Name used in cap messages

	bool		m_bStartDisabled;

	int m_iAlliesRequired;		// if we're controlled by an area cap, 
	int m_iAxisRequired;		// these hold the number of cappers required. Used to calc point value

	int m_iTimedPointsAllies;	// timed points value of this flag, per team
	int m_iTimedPointsAxis;

	bool m_bBombPlanted;
	float m_flBombExplodeTime;

	int m_iBombsRemaining;
	int m_iBombsRequired;	// number of bombs required to flip this control point

	bool m_bSetOwnerIsBombPlant;	// temp flag to indicate if the set owner we're doing is the result of a bomb

private:
	CControlPoint( const CControlPoint & );
};

#endif //DOD_CONTROL_POINT_H