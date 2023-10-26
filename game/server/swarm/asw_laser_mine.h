#ifndef _INCLUDED_ASW_LASER_MINE_H
#define _INCLUDED_ASW_LASER_MINE_H
#ifdef _WIN32
#pragma once
#endif

#include "asw_shareddefs.h"

static const char *s_pLaserMineSpawnFlipThink = "LaserMineSpawnFlipThink";

class CASW_Laser_Mine : public CBaseCombatCharacter, public IEntityListener
{
	DECLARE_CLASS( CASW_Laser_Mine, CBaseCombatCharacter );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CASW_Laser_Mine();
	virtual ~CASW_Laser_Mine();

public:
	void				Spawn( void );
	void				Precache( void );
	unsigned int		PhysicsSolidMaskForEntity() const { return MASK_NPCSOLID; }
	static CASW_Laser_Mine* ASW_Laser_Mine_Create( const Vector &position, const QAngle &angles, const QAngle &angLaserAim, CBaseEntity *pOwner, CBaseEntity *pMoveParent, bool bFriendly, CBaseEntity *pCreatorWeapon );
	void				LaserThink( void );
	
	void				StartSpawnFlipping( Vector vecStart, Vector vecEnd, QAngle angEnd, float flDuration );
	void				SpawnFlipThink();

	bool				ValidMineTarget(CBaseEntity *pOther);
	void				Prime();
	void				Explode( bool bRemove = true );
	virtual void StopLoopingSounds();
	void				SetLaserAngle( const QAngle &angLaser ) { m_angLaserAim = angLaser; }
	void				UpdateLaser();

	virtual void		OnEntityDeleted( CBaseEntity *pEntity );
	
	CNetworkVar( QAngle, m_angLaserAim );
	CNetworkVar( bool, m_bFriendly );
	CNetworkVar( bool, m_bMineActive );

	float m_flDamageRadius;
	float m_flDamage;

	bool m_bIsSpawnFlipping;
	bool m_bIsSpawnLanded;
	float m_flSpawnFlipStartTime;
	float m_flSpawnFlipEndTime;
	Vector m_vecSpawnFlipStartPos;
	Vector m_vecSpawnFlipEndPos;
	QAngle m_angSpawnFlipEndAngle;

	EHANDLE m_hCreatorWeapon;
	Class_T m_CreatorWeaponClass;

	// Classification
	virtual Class_T		Classify( void ) { return (Class_T) CLASS_ASW_LASER_MINE_PROJECTILE; }
};


#endif // _INCLUDED_ASW_LASER_MINE_H
