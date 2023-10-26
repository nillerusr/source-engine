#ifndef _INCLUDED_ASW_T75_H
#define _INCLUDED_ASW_T75_H
#ifdef _WIN32
#pragma once
#endif

#include "basecombatcharacter.h"
#include "iasw_server_usable_entity.h"

class CASW_T75 : public CBaseCombatCharacter, public IASW_Server_Usable_Entity
{
	DECLARE_CLASS( CASW_T75, CBaseCombatCharacter );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CASW_T75();
	virtual ~CASW_T75();

public:
	void				Spawn( void );
	void				Precache( void );
	unsigned int		PhysicsSolidMaskForEntity() const { return MASK_NPCSOLID; }
	static CASW_T75* ASW_T75_Create( const Vector &position, const QAngle &angles, const Vector &velocity, const AngularImpulse &angVelocity, CBaseEntity *pOwner, CBaseEntity *pCreatorWeapon = NULL );	
	void				TouchT75( CBaseEntity *pOther );
	Class_T				Classify( void ) { return (Class_T) CLASS_ASW_T75; }
	
	void				Arm();
	void				Explode();
	void				CountdownThink( void );

	// IASW_Server_Usable_Entity implementation
	virtual CBaseEntity* GetEntity() { return this; }
	virtual bool IsUsable(CBaseEntity *pUser);
	virtual bool RequirementsMet( CBaseEntity *pUser ) { return true; }
	virtual void ActivateUseIcon( CASW_Marine* pMarine, int nHoldType );
	virtual void MarineUsing(CASW_Marine* pMarine, float deltatime);
	virtual void MarineStartedUsing(CASW_Marine* pMarine);
	virtual void MarineStoppedUsing(CASW_Marine* pMarine);
	virtual bool NeedsLOSCheck() { return true; }
	
	CNetworkVar( bool, m_bArmed );
	CNetworkVar( bool, m_bIsInUse );
	CNetworkVar( float, m_flArmProgress );
	
	int m_iCountdown;
	float m_flDamageRadius;
	float m_flDamage;

	EHANDLE m_hCreatorWeapon;
};


#endif // _INCLUDED_ASW_T75_H
