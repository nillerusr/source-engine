#ifndef _INCLUDED_ASW_PROP_PHYSICS_H
#define _INCLUDED_ASW_PROP_PHYSICS_H

#include "props.h"
#include "asw_shareddefs.h"

class CASW_Prop_Physics : public CPhysicsProp
{
public:
	DECLARE_CLASS( CASW_Prop_Physics, CPhysicsProp );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	virtual void Spawn( void );
	virtual void Event_Killed( const CTakeDamageInfo &info );

	static bool KickingVismonCallback( CBaseEntity *pPhysProp, CBasePlayer *pViewingPlayer );
	static bool DetonateVismonCallback( CBaseEntity *pPhysProp, CBasePlayer *pViewingPlayer );

	virtual Class_T		Classify( void ) { return (Class_T) CLASS_ASW_PHYSICS_PROP; }

protected:
	virtual void	OnBreak( const Vector &vecVelocity, const AngularImpulse &angVel, CBaseEntity *pBreaker );
	virtual void	Ignite( float flFlameLifetime, bool bNPCOnly, float flSize, bool bCalledByLevelDesigner );
	virtual int		OnTakeDamage( const CTakeDamageInfo &inputInfo );

	CNetworkVar( bool, m_bIsAimTarget );
	CNetworkVar( bool, m_bOnFire );
	bool m_bBulletForceImmune;
	bool m_bMeleeHit;
};

#define SF_PHYSPROP_AIMTARGET					0x008000		// makes this prop a clientside autoaim target for marines


#endif // _INCLUDED_ASW_PROP_PHYSICS_H