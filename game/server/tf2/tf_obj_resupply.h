//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Medic's resupply beacon
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_OBJ_RESUPPLY_H
#define TF_OBJ_RESUPPLY_H
#ifdef _WIN32
#pragma once
#endif

#include "tf_obj.h"

// ------------------------------------------------------------------------ //
// Resupply defines
#define	RESUPPLY_NUM_PLAYERS_REFILLED		5		// Number of full resupplies before refill need

// An object is considered covered by a resupply station (for purposes of order creation)
// if it is within this distance of the station.
#define RESUPPLY_COVER_DIST			1500



// ------------------------------------------------------------------------ //
// Resupply object that's built by the player
// ------------------------------------------------------------------------ //
class CObjectResupply : public CBaseObject
{
DECLARE_CLASS( CObjectResupply, CBaseObject );

public:
	DECLARE_SERVERCLASS();

	CObjectResupply();

	static CObjectResupply* Create(const Vector &vOrigin, const QAngle &vAngles);

	virtual void	Spawn();
	virtual void	GetControlPanelInfo( int nPanelIndex, const char *&pPanelName );
	virtual void	Precache();
	virtual void	DestroyObject( void );

	virtual bool	CalculatePlacement( CBaseTFPlayer *pPlayer );
	virtual	void	SetupAttachedVersion( void );

	// Resupply
	virtual bool	ClientCommand( CBaseTFPlayer *pPlayer, const CCommand &args );

	virtual void	ChangeTeam( int iTeamNum ) OVERRIDE;

	virtual bool	CanTakeEMPDamage( void ) { return true; }

private:
	// Resupply Health 
	bool ResupplyHealth( CBaseTFPlayer *pPlayer, float flFraction );
};

#endif // TF_OBJ_RESUPPLY_H
