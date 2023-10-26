#ifndef _DEFINED_ASW_MINE_H
#define _DEFINED_ASW_MINE_H
#pragma once

class CASW_Mine : public CBaseCombatCharacter
{
	DECLARE_CLASS( CASW_Mine, CBaseCombatCharacter );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CASW_Mine();
	virtual ~CASW_Mine();

public:
	void				Spawn( void );
	void				Precache( void );
	unsigned int		PhysicsSolidMaskForEntity() const;
	void				MineTouch( CBaseEntity *pOther );
	static CASW_Mine* ASW_Mine_Create( const Vector &position, const QAngle &angles, const Vector &velocity, const AngularImpulse &angVelocity, CBaseEntity *pOwner, CBaseEntity *pCreatorWeapon );	
	void				MineThink( void );
	bool				ValidMineTarget(CBaseEntity *pOther);
	void				Prime();
	void				Explode();
	virtual void StopLoopingSounds();
	
	bool m_bPrimed;
	float m_fForcePrimeTime;
	// when set, this mine will shortly explode
	CNetworkVar( bool, m_bMineTriggered );

	float m_flDurationScale;
	int m_iExtraFires;

	EHANDLE m_hCreatorWeapon;
};


#endif // _DEFINED_ASW_MINE_H
