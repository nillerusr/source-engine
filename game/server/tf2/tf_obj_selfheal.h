//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Upgrade that heals the object over time
//
//=============================================================================//

#ifndef TF_OBJ_SELFHEAL_H
#define TF_OBJ_SELFHEAL_H
#ifdef _WIN32
#pragma once
#endif

#include "tf_obj_baseupgrade_shared.h"

// ------------------------------------------------------------------------ //
// Self-Heal upgrade
// ------------------------------------------------------------------------ //
class CObjectSelfHeal : public CBaseObjectUpgrade
{
DECLARE_CLASS( CObjectSelfHeal, CBaseObjectUpgrade );

public:
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	CObjectSelfHeal();

	virtual void	Spawn();
	virtual void	Precache();
	virtual bool	CanTakeEMPDamage( void ) { return true; }
	virtual void	FinishedBuilding( void );

	// Repairing
	virtual void	SelfHealThink( void );
};

#endif // TF_OBJ_SELFHEAL_H
