//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//

#ifndef AMMO_BOX_H
#define AMMO_BOX_H
#ifdef _WIN32
#pragma once
#endif

#include "items.h"

class CAmmoBox : public CItem
{
public:
	DECLARE_CLASS( CAmmoBox, CItem );

	CAmmoBox() {}

	virtual void Spawn();
	virtual void Precache();

	void EXPORT FlyThink( void );
	void EXPORT BoxTouch( CBaseEntity *pOther );
	bool MyTouch( CBasePlayer *pBasePlayer );

	static CAmmoBox *Create( const Vector &vecOrigin, const QAngle &vecAngles, CBaseEntity *pOwner, int team );

	void SetAmmoTeam( int team );

private:
	EHANDLE m_hOldOwner;
	int m_iAmmoTeam;
	
private:
	CAmmoBox( const CAmmoBox & );

	DECLARE_DATADESC();
};

#endif //GENERIC_AMMO_H