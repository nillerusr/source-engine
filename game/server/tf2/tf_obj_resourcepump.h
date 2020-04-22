//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Resource pump
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_OBJ_RESOURCEPUMP_H
#define TF_OBJ_RESOURCEPUMP_H
#ifdef _WIN32
#pragma once
#endif


#include "tf_obj.h"


class CResourceZone;

// ------------------------------------------------------------------------ //
// Pump defines
#define RESOURCEPUMP_MINS			Vector(-20, -20, 0)
#define RESOURCEPUMP_MAXS			Vector( 20,  20, 140)

// ------------------------------------------------------------------------ //
// Resupply object that's built by the player
// ------------------------------------------------------------------------ //
class CObjectResourcePump : public CBaseObject
{
DECLARE_CLASS( CObjectResourcePump, CBaseObject );

public:
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	static CObjectResourcePump* Create(const Vector &vOrigin, const QAngle &vAngles);

	CObjectResourcePump();

	virtual void	Spawn();
	virtual void	Activate( void );
	virtual void	GetControlPanelInfo( int nPanelIndex, const char *&pPanelName );
	virtual void	FinishedBuilding( void );
	void			SetupPump( void );
	virtual void	Precache();
	virtual bool	CanTakeEMPDamage( void ) { return true; }

	virtual void	ResourcePumpThink( void );
	virtual bool	ClientCommand( CBaseTFPlayer *pPlayer, const CCommand &args );
	virtual void	SetupTeamModel( void );

	// Gets the resource zone (may be NULL!)
	CResourceZone* GetResourceZone();

private:
	float	m_flPumpSpeed;
	CNetworkVar( int, m_iPumpLevel );
};

#endif // TF_OBJ_RESOURCEPUMP_H
