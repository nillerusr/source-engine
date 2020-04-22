//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:		Base class for simple projectiles
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "cbaseanimatingprojectile.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS( baseanimating_projectile, CBaseAnimatingProjectile );

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CBaseAnimatingProjectile )

	DEFINE_FIELD( m_iDmg,		FIELD_INTEGER ),
	DEFINE_FIELD( m_iDmgType,	FIELD_INTEGER ),

END_DATADESC()

//---------------------------------------------------------
//---------------------------------------------------------
void CBaseAnimatingProjectile::Spawn(	char *pszModel,
										const Vector &vecOrigin,
										const Vector &vecVelocity,
										edict_t *pOwner,
										MoveType_t	iMovetype,
										MoveCollide_t nMoveCollide,
										int	iDamage,
										int iDamageType )
{
	Precache();

	SetSolid( SOLID_BBOX );
	SetModel( pszModel );

	UTIL_SetSize( this, vec3_origin, vec3_origin );

	m_iDmg = iDamage;
	m_iDmgType = iDamageType;

	SetMoveType( iMovetype, nMoveCollide );

	UTIL_SetOrigin( this, vecOrigin );
	SetAbsVelocity( vecVelocity );

	SetOwnerEntity( Instance( pOwner ) );

	QAngle qAngles;
	VectorAngles( vecVelocity, qAngles );
	SetAbsAngles( qAngles );
}


//---------------------------------------------------------
//---------------------------------------------------------
void CBaseAnimatingProjectile::Touch( CBaseEntity *pOther )
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
