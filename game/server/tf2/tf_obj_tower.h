//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: A stationary tower that players can take cover in.
//
//=============================================================================//

#ifndef TF_OBJ_TOWER_H
#define TF_OBJ_TOWER_H
#ifdef _WIN32
#pragma once
#endif

class CObjectTowerLadder;

// ------------------------------------------------------------------------ //
// Purpose: A stationary tower that players can take cover in.
// ------------------------------------------------------------------------ //
class CObjectTower : public CBaseObject
{
	DECLARE_CLASS( CObjectTower, CBaseObject );

public:
	DECLARE_SERVERCLASS();

	CObjectTower( void );

	virtual void	Spawn( void );
	virtual void	Precache( void );
	virtual void	UpdateOnRemove( void );
	virtual void	GetControlPanelInfo( int nPanelIndex, const char *&pPanelName );
	virtual void	FinishedBuilding( void );

private:
	CHandle<CObjectTowerLadder>		m_hLadder;
};


//-----------------------------------------------------------------------------
// Purpose: Ladder tower
//-----------------------------------------------------------------------------
class CObjectTowerLadder : public CBaseAnimating
{
	DECLARE_CLASS( CObjectTowerLadder, CBaseAnimating );

public:

	DECLARE_SERVERCLASS();

	static CObjectTowerLadder* Create( const Vector &vOrigin, const QAngle &vAngles, CBaseEntity *pParent );

	CObjectTowerLadder();

	void	Spawn();
	void	Precache();
	virtual int	OnTakeDamage( const CTakeDamageInfo &info );

public:
	EHANDLE		m_hTower;
};

#endif // TF_OBJ_TOWER_H
