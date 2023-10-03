#ifndef ASW_AMMO_DROP_H
#define ASW_AMMO_DROP_H
#pragma once

#include "asw_shareddefs.h"
#include "iasw_server_usable_entity.h"

class CASW_Marine;
class CASW_Weapon;

DECLARE_AUTO_LIST( IAmmoDropAutoList );

class CASW_Ammo_Drop : public CBaseAnimating, public IASW_Server_Usable_Entity, public IAmmoDropAutoList
{
public:
	DECLARE_CLASS( CASW_Ammo_Drop, CBaseAnimating );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	virtual void Precache();

	CASW_Ammo_Drop();
	virtual ~CASW_Ammo_Drop();
	void Spawn( void );
	virtual int	ShouldTransmit( const CCheckTransmitInfo *pInfo );
	int UpdateTransmitState();	
	
	CNetworkVar( int, m_iAmmoUnitsRemaining );

private:
	int GetWeaponAmmoInUnits( CASW_Marine *pMarine );
public:

	int GetAmmoUnitCost( int iAmmoType );
	CASW_Weapon* GetAmmoUseUnits( CASW_Marine *pMarine );
	bool NeedsAmmoMoreThan( CASW_Marine *pFirstMarine, CASW_Marine *pSecondMarine );	// does first marine need ammo more than second?
	bool AllowedToPickup( CASW_Marine *pMarine );
	void PlayDeploySound();
	void SetDeployer( CASW_Marine *pMarine ) { m_hDeployer = pMarine; }

	// IASW_Server_Usable_Entity implementation
	virtual CBaseEntity* GetEntity() { return this; }
	virtual bool IsUsable(CBaseEntity *pUser);
	virtual bool RequirementsMet( CBaseEntity *pUser ) { return true; }
	virtual void ActivateUseIcon( CASW_Marine* pMarine, int nHoldType );
	virtual void MarineUsing(CASW_Marine* pMarine, float deltatime);
	virtual void MarineStartedUsing(CASW_Marine* pMarine);
	virtual void MarineStoppedUsing(CASW_Marine* pMarine);
	virtual bool NeedsLOSCheck() { return true; }

protected:
	bool m_bSuppliedAmmo;
	CHandle<CASW_Marine> m_hDeployer;
};

#endif /* ASW_AMMO_DROP_H */