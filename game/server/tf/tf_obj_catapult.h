//========= Copyright Valve Corporation, All rights reserved. ============//
//
//
//
//=============================================================================

#ifndef TF_OBJ_CATAPULT_H
#define TF_OBJ_CATAPULT_H
#ifdef _WIN32
#pragma once
#endif

#include "tf_obj.h"

class CTFPlayer;

#define CATAPULT_MAX_HEALTH		150

#ifdef STAGING_ONLY

// ------------------------------------------------------------------------ //
// Base catapult object
// ------------------------------------------------------------------------ //
class CObjectCatapult : public CBaseObject
{
	DECLARE_CLASS( CObjectCatapult, CBaseObject );
	DECLARE_DATADESC();
public:
	//DECLARE_SERVERCLASS();

	CObjectCatapult();

	virtual void	Spawn();
	virtual void	Precache();
	virtual bool	IsPlacementPosValid( void );

	virtual void StartTouch( CBaseEntity *pOther );
	virtual void EndTouch( CBaseEntity *pOther );

	void CatapultThink();

	virtual void	OnGoActive();

	// Wrench hits
	//virtual bool	Command_Repair( CTFPlayer *pActivator, float flRepairMod ); 
	//virtual bool	InputWrenchHit( CTFPlayer *pPlayer, CTFWrench *pWrench, Vector hitLoc );

	// Upgrading
	//virtual bool	IsUpgrading( void ) const { return false; }
	//virtual bool	CheckUpgradeOnHit( CTFPlayer *pPlayer );

	virtual int		GetBaseHealth( void ) { return CATAPULT_MAX_HEALTH; }
private:
	void Launch( CBaseEntity* pEnt );

	struct Jumper_t
	{
		CHandle< CBaseEntity > m_hJumper;
		float flTouchTime;
	};
	CUtlVector< Jumper_t > m_jumpers;
};

#endif // STAGING_ONLY

#endif // TF_OBJ_CATAPULT_H
