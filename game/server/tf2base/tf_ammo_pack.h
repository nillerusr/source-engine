//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//

#ifndef TF_AMMO_PACK_H
#define TF_AMMO_PACK_H
#ifdef _WIN32
#pragma once
#endif

#include "items.h"

class CTFAmmoPack : public CItem
{
public:
	DECLARE_CLASS( CTFAmmoPack, CItem );
	DECLARE_SERVERCLASS();

	CTFAmmoPack() {}

	virtual void Spawn();
	virtual void Precache();		

	void EXPORT FlyThink( void );
	void EXPORT PackTouch( CBaseEntity *pOther );

	virtual unsigned int PhysicsSolidMaskForEntity( void ) const;

	int GiveAmmo( int iCount, int iAmmoType );

	static CTFAmmoPack *Create( const Vector &vecOrigin, const QAngle &vecAngles, CBaseEntity *pOwner, const char *pszModelName );

	float GetCreationTime( void ) { return m_flCreationTime; }
	void  SetInitialVelocity( Vector &vecVelocity );

private:
	int m_iAmmo[MAX_AMMO_SLOTS];

	float m_flCreationTime;

	bool m_bAllowOwnerPickup;
	CNetworkVector( m_vecInitialVelocity );

private:
	CTFAmmoPack( const CTFAmmoPack & );

	DECLARE_DATADESC();
};

#endif //TF_AMMO_PACK_H