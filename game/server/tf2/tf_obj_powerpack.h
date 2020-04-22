//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Human's power pack
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_OBJ_POWERPACK_H
#define TF_OBJ_POWERPACK_H
#ifdef _WIN32
#pragma once
#endif

#include "tf_obj.h"

// ------------------------------------------------------------------------ //
// Pack defines
#define POWERPACK_MINS			Vector(-20, -20, 0)
#define POWERPACK_MAXS			Vector( 20,  20, 80)
#define POWERPACK_RANGE			(600 * 600)

// ------------------------------------------------------------------------ //
// Resupply object that's built by the player
// ------------------------------------------------------------------------ //
class CObjectPowerPack : public CBaseObject
{
	DECLARE_CLASS( CObjectPowerPack, CBaseObject );
public:
	DECLARE_SERVERCLASS();

	CObjectPowerPack();
	static CObjectPowerPack *Create(const Vector &vOrigin, const QAngle &vAngles);

	virtual void	Spawn();
	virtual void	FinishedBuilding( void );
	virtual void	Precache();
	virtual bool	CanTakeEMPDamage( void ) { return true; }
	virtual void	DestroyObject( void );
	virtual bool	CalculatePlacement( CBaseTFPlayer *pPlayer );

	// This is called by the base object when it's time to spawn the control panels
	virtual void GetControlPanelInfo( int nPanelIndex, const char *&pPanelName );

	// Find nearby objects and provide them with power
	void PowerNearbyObjects( CBaseObject *pObjectToTarget = NULL, bool bPlacing = false );
	void UnPowerAllObjects( void );
	void UnPowerObject( CBaseObject *pObject );
	void EnsureObjectPower( CBaseObject *pObject );

	// Powerpack switches models after assembly
	virtual void OnActivityChanged( Activity act );

private:
	void PowerObject( CBaseObject *pObject, bool bPlacing = false );
	bool IsWithinPowerRange( CBaseObject *pObject );

	// Objects powered from this pack
	typedef CHandle<CBaseObject>	ObjectHandle;
	CUtlVector< ObjectHandle >		m_hPoweredObjects;
	int								m_iFreeAttachments;
	CNetworkVar( int, m_iObjectsAttached );
};

#endif // TF_OBJ_POWERPACK_H
