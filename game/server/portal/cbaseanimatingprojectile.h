//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:		Base class for simple projectiles
//
// $NoKeywords: $
//=============================================================================//

#ifndef CBASEANIMATINGPROJECTILE_H
#define CBASEANIMATINGPROJECTILE_H
#ifdef _WIN32
#pragma once
#endif

enum MoveType_t;
enum MoveCollide_t;


//=============================================================================
//=============================================================================
class CBaseAnimatingProjectile : public CBaseAnimating
{
	DECLARE_DATADESC();
	DECLARE_CLASS( CBaseAnimatingProjectile, CBaseAnimating );

public:
	void Touch( CBaseEntity *pOther );

	void Spawn(	char *pszModel,
											const Vector &vecOrigin,
											const Vector &vecVelocity,
											edict_t *pOwner,
											MoveType_t	iMovetype,
											MoveCollide_t nMoveCollide,
											int	iDamage,
											int iDamageType );

	virtual void Precache( void ) {};

	int	m_iDmg;
	int m_iDmgType;
};

#endif // CBASEANIMATINGPROJECTILE_H
