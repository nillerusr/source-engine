//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_FUNC_WELDABLE_DOOR_H
#define TF_FUNC_WELDABLE_DOOR_H
#ifdef _WIN32
#pragma once
#endif

#include "doors.h"

class CBeam;


//-----------------------------------------------------------------------------
// Purpose: Weld Point for a weldable door
//-----------------------------------------------------------------------------
class CWeldPoint : public CPointEntity
{
	DECLARE_CLASS( CWeldPoint, CPointEntity );
public:
	void	Spawn( void );
	void	SetEndPoint( const Vector &vecEndPoint );
	const Vector &GetStartPoint( void ) const;
	const Vector &GetEndPoint( void ) const;

private:
	Vector	m_vecEndPoint;
};

// Weld Leader
enum
{
	WL_UNASSIGNED,
	WL_WELD_LEADER,
	WL_WELD_CHILD,
};

//-----------------------------------------------------------------------------
// Purpose: A weldable door
//-----------------------------------------------------------------------------
class CWeldableDoor : public CBaseDoor
{
public:
	DECLARE_CLASS( CWeldableDoor, CBaseDoor );

	DECLARE_DATADESC();

	virtual void	Spawn( void );
	virtual void	Activate( void );

	// Welding
	virtual bool	IsWeldable( CBaseTFPlayer *pWeldee );
	virtual void	StartWelding( CBaseTFPlayer *pWeldee );
	virtual void	StopWelding( void );
	virtual float	GetWeldPercentage( void );
	virtual Vector	GetPlayerWeldPoint( void );
	virtual void	UpdateWeld( bool bCutting, float flWeldPercentage );
	CWeldableDoor	*GetWeldLeader( void );

	// Welding
	int			m_iWeldLeader;
	EHANDLE		m_hWeldingPlayer;

	// Weld points
	string_t	m_iszWeldPoints;
	CUtlVector	< CWeldPoint * >	m_aWeldPoints;

	// Weld beams
	float		m_flMaxWeldedPercentage;
	float		m_flWeldedPercentage;
	CUtlVector	< CBeam * >			m_aWeldBeams;
	CUtlVector	< CBeam * >			m_aCutBeams;
};


#endif // TF_FUNC_WELDABLE_DOOR_H
