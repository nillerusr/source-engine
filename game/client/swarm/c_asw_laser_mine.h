#ifndef _INCLUDED_C_ASW_LASER_MINE_H
#define _INCLUDED_C_ASW_LASER_MINE_H

#include "c_basecombatcharacter.h"
#include "asw_shareddefs.h"

struct dlight_t;

class C_ASW_Laser_Mine : public C_BaseCombatCharacter
{
public:
	DECLARE_CLASS( C_ASW_Laser_Mine, C_BaseCombatCharacter );
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();

					C_ASW_Laser_Mine();
	virtual			~C_ASW_Laser_Mine();
	virtual void ClientThink(void);
	virtual void OnDataChanged(DataUpdateType_t updateType);
	virtual void UpdateOnRemove();
	void UpdateLaser();
	void CreateLaserEffect();
	void RemoveLaserEffect();
	CUtlReference<CNewParticleEffect> m_pLaserEffect;

	CNetworkVar( QAngle, m_angLaserAim );
	CNetworkVar( bool, m_bFriendly );
	CNetworkVar( bool, m_bMineActive );

	// Classification
	virtual Class_T		Classify( void ) { return (Class_T) CLASS_ASW_LASER_MINE_PROJECTILE; }
private:
	C_ASW_Laser_Mine( const C_ASW_Laser_Mine & ); // not defined, not accessible
};

#endif // _INCLUDED_C_ASW_LASER_MINE_H