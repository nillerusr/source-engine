//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef NAILGUN_NAIL_H
#define NAILGUN_NAIL_H
#ifdef _WIN32
#pragma once
#endif


class CTFNailgunNail : public CBaseAnimating
{
public:
	DECLARE_CLASS( CTFNailgunNail, CBaseAnimating );
	DECLARE_DATADESC();
	
	void	Spawn();
	void	Precache();
	
	// Functions to create all the various types of nails.
	static  CTFNailgunNail *CreateNail( bool fSendClientNail, Vector vecOrigin, QAngle vecAngles, CBaseEntity *pOwner, CBaseEntity *pLauncher, bool fCreateClientNail );
	static  CTFNailgunNail *CreateSuperNail( Vector vecOrigin, QAngle vecAngles, CBaseEntity *pOwner, CBaseEntity *pLauncher );
	static	CTFNailgunNail *CreateTranqNail( Vector vecOrigin, QAngle vecAngles, CBaseEntity *pOwner, CBaseEntity *pLauncher );
	static	CTFNailgunNail *CreateRailgunNail( Vector vecOrigin, QAngle vecAngles, CBaseEntity *pOwner, CBaseEntity *pLauncher );
	static	CTFNailgunNail *CreateNailGrenNail( Vector vecOrigin, QAngle vecAngles, CBaseEntity *pOwner, CBaseEntity *pNailGren );


private:

	void	RailgunNail_Think();
	void	NailTouch( CBaseEntity *pOther );
	void	TranqTouch( CBaseEntity *pOther );
	void	RailgunNailTouch( CBaseEntity *pOther );


private:
	Vector m_vecPreviousVelocity;
	int m_iDamage;		// How much damage this nail does when it hits an enemy.
};


#endif // NAILGUN_NAIL_H
