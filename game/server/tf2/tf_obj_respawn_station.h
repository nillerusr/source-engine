//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Portable respawn station
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_OBJ_RESPAWN_STATION_H
#define TF_OBJ_RESPAWN_STATION_H

//-----------------------------------------------------------------------------
// forward declarations
//-----------------------------------------------------------------------------
class CBaseEntity;

//-----------------------------------------------------------------------------
// Respawn station defines
//-----------------------------------------------------------------------------
#define RESPAWN_STATION_MINS			Vector(-60, -45, 0)
#define RESPAWN_STATION_MAXS			Vector( 60,  45, 140)

#define RESPAWN_STATION_BUILD_MINS		Vector(-60, -45, 0)
#define RESPAWN_STATION_BUILD_MAXS		Vector( 60,  40, 140)

#define RESPAWN_EFFECT_TIME 5.0f
#define	RESPAWN_BEAM_HEIGHT 800.0f

//-----------------------------------------------------------------------------
// Portable respawn station
//-----------------------------------------------------------------------------
class CObjectRespawnStation : public CBaseObject
{
DECLARE_CLASS( CObjectRespawnStation, CBaseObject );

public:
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CObjectRespawnStation();

	// Gets called when someone respawns on this station
	void PerformRespawnEffect();

	static CObjectRespawnStation* Create(const Vector &vOrigin, const QAngle &vAngles);

	virtual void	Precache();
	virtual void	Spawn();

	virtual bool	WantsCover()	{ return true; }

	// Object using!
	virtual void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	virtual bool	CanTakeEMPDamage( void ) { return false; }

	// Map specified as the initial spawnpoint for a team
	bool	IsInitialSpawnPoint( void );

protected:
	float	m_fLastRespawnTime;
	int		m_iSpriteTexture;
	bool	m_bIsInitialSpawnPoint;
};

//-----------------------------------------------------------------------------
// Plays a respawn effect on a respawn station...
//-----------------------------------------------------------------------------
void PlayRespawnEffect(CBaseEntity *pRespawnStation);

#endif // TF_OBJ_RESPAWN_STATION_H
