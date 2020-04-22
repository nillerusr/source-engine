//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef TF_WALKER_STRIDER_H
#define TF_WALKER_STRIDER_H
#ifdef _WIN32
#pragma once
#endif


#include "tf_walker_base.h"


class CWalkerStrider : public CWalkerBase
{
public:
	DECLARE_CLASS( CWalkerStrider, CWalkerBase );
	DECLARE_SERVERCLASS();

	CWalkerStrider();


// CWalkerBase.
protected:
	virtual void WalkerThink();


// CBaseObject.
public:
	virtual bool StartBuilding( CBaseEntity *pBuilder );
	

// CBaseEntity.
public:	
	virtual void Precache();
	virtual void Spawn();
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );


// CBaseAnimating.
public:
	virtual bool GetAttachment( int iAttachment, matrix3x4_t &attachmentToWorld );
	virtual void HandleAnimEvent( animevent_t *pEvent );


// IServerVehicle.
public:	
	virtual bool IsPassengerVisible( int nRole );


// IVehicle overrides.
public:
	virtual void SetupMove( CBasePlayer *pPlayer, CUserCmd *ucmd, IMoveHelper *pHelper, CMoveData *move );


private:
	void Fire();

	void Crouch();
	void UnCrouch();

	void FootHit( const char *pFootName );



private:
	CNetworkVar( bool, m_bCrouched );
	float m_flNextCrouchTime;
	
	bool m_bFiring;
	float m_flFireEndTime;
	float m_flNextShootTime;
	QAngle m_vFireAngles;

	float m_flOriginToLowestLegHeight;
	float m_flWantedZ;

	QAngle m_vDriverAngles;
};	


#endif // TF_WALKER_STRIDER_H
