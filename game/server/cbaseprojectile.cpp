//========= Copyright © 1996-2007, Valve Corporation, All rights reserved. ============//
//
// Purpose:		Base class for simple projectiles that use studio models
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//

//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// This file was made before I
// realized Portal had already
// made the same thing 
// (probably because it was 
// squirreled away in the PORTAL
// source folder). Anyway, this
// needs to be reconciled with that
// file since clearly there is
// demand for this feature (sjb) 11/27/07
//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!



#include "cbase.h"
#include "cbaseprojectile.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS( baseprojectile, CBaseProjectile );

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CBaseProjectile )

DEFINE_FIELD( m_iDmg,		FIELD_INTEGER ),
DEFINE_FIELD( m_iDmgType,	FIELD_INTEGER ),
DEFINE_FIELD( m_hIntendedTarget, FIELD_EHANDLE ),

END_DATADESC()

//---------------------------------------------------------
//---------------------------------------------------------
void CBaseProjectile::Spawn(	char *pszModel,
								  const Vector &vecOrigin,
								  const Vector &vecVelocity,
								  edict_t *pOwner,
								  MoveType_t	iMovetype,
								  MoveCollide_t nMoveCollide,
								  int	iDamage,
								  int iDamageType,
								  CBaseEntity *pIntendedTarget )
{
	Precache();

	SetModel( pszModel );

	m_iDmg = iDamage;
	m_iDmgType = iDamageType;


	SetMoveType( iMovetype, nMoveCollide );
	UTIL_SetSize( this, -Vector(1,1,1), Vector(1,1,1) );
	SetSolid( SOLID_BBOX );

	SetCollisionGroup( COLLISION_GROUP_PROJECTILE );

	UTIL_SetOrigin( this, vecOrigin );
	SetAbsVelocity( vecVelocity );

	SetOwnerEntity( Instance( pOwner ) );

	m_hIntendedTarget.Set( pIntendedTarget );

	QAngle angles;

	VectorAngles( vecVelocity, angles );

	SetAbsAngles( angles );

	// Call think for free the first time. It's up to derived classes to rethink.
	SetNextThink( gpGlobals->curtime );
}

//---------------------------------------------------------
//---------------------------------------------------------
void CBaseProjectile::Touch( CBaseEntity *pOther )
{
	if ( pOther->IsSolidFlagSet(FSOLID_TRIGGER) )
	{
			return;
	}

	HandleTouch( pOther );
}

//---------------------------------------------------------
//---------------------------------------------------------
void CBaseProjectile::HandleTouch( CBaseEntity *pOther )
{
	CBaseEntity *pOwner;

	pOwner = GetOwnerEntity();

	if( !pOwner )
	{
		pOwner = this;
	}

	trace_t	tr;
	tr = BaseClass::GetTouchTrace( );

	CTakeDamageInfo info( this, pOwner, m_iDmg, m_iDmgType );
	GuessDamageForce( &info, (tr.endpos - tr.startpos), tr.endpos );
	pOther->TakeDamage( info );

	UTIL_Remove( this );
}

//---------------------------------------------------------
//---------------------------------------------------------
void CBaseProjectile::Think()
{
	HandleThink();
}

//---------------------------------------------------------
//---------------------------------------------------------
void CBaseProjectile::HandleThink()
{
}

