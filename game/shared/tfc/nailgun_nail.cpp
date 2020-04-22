//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "nailgun_nail.h"


#define NAILGUN_MODEL "models/nail.mdl"


LINK_ENTITY_TO_CLASS( tf_nailgun_nail, CTFNailgunNail );
PRECACHE_REGISTER( tf_nailgun_nail );

BEGIN_DATADESC( CTFNailgunNail )
	DEFINE_FUNCTION( NailTouch )
END_DATADESC()



CTFNailgunNail *CTFNailgunNail::CreateNail( 
	bool fSendClientNail, 
	Vector vecOrigin, 
	QAngle vecAngles, 
	CBaseEntity *pOwner, 
	CBaseEntity *pLauncher, 
	bool fMakeClientNail )
{
	CTFNailgunNail *pNail = (CTFNailgunNail*)CreateEntityByName( "tf_nailgun_nail" );
	if ( !pNail )
		return NULL;

	pNail->SetAbsOrigin( vecOrigin );
	pNail->SetAbsAngles( vecAngles );
	pNail->SetOwnerEntity( pOwner );
	pNail->Spawn();
	pNail->SetTouch( &CTFNailgunNail::NailTouch );
	pNail->m_iDamage = 9;
/*
	if ( fMakeClientNail )
	{
		edict_t *pOwner;
		int indexOwner;

		// don't send to clients.
		pNail->pev->effects |= EF_NODRAW;
		pOwner = ENT( pNail->pev->owner );
		indexOwner = ENTINDEX(pOwner);

		if ( fSendClientNail )
			UTIL_ClientProjectile( pNail->pev->origin, pNail->pev->velocity, g_sModelIndexNail, indexOwner, 6 );
	}
*/
	return pNail;
}



CTFNailgunNail *CTFNailgunNail::CreateSuperNail( Vector vecOrigin, QAngle vecAngles, CBaseEntity *pOwner, CBaseEntity *pLauncher )
{
	// Super nails simply do more damage
	CTFNailgunNail *pNail = CreateNail( false, vecOrigin, vecAngles, pOwner, pLauncher, true );
	pNail->m_iDamage = 13;
	return pNail;
}


//=========================================================
// CTFNailgunNail::Spawn
//
// Creates an nail entity on the server that is invisible 
// so that it won't be sent to clients. At the same time sends a
// tiny message that creates a flying nail entity on all 
// clients. (sjb)
//=========================================================
void CTFNailgunNail::Spawn()
{
	// motor
	SetMoveType( MOVETYPE_FLYGRAVITY );
	SetSolid( SOLID_BBOX );

	SetModel( NAILGUN_MODEL );

	SetSize( Vector( 0, 0, 0), Vector(0, 0, 0) );

	Vector vForward;
	AngleVectors( GetAbsAngles(), &vForward );
	SetAbsVelocity( vForward * 1000 );
	SetGravity( 0.5 );

	SetNextThink( gpGlobals->curtime + 6 );
	SetThink( &CTFNailgunNail::SUB_Remove );
}


void CTFNailgunNail::Precache()
{
	PrecacheModel( NAILGUN_MODEL );
	BaseClass::Precache();
}


void CTFNailgunNail::NailTouch( CBaseEntity *pOther )
{
	// Damage person hit by the nail
	CBaseEntity *pevOwner = GetOwnerEntity();
	if ( !pevOwner || !pOther )
	{
		UTIL_Remove( this );
		return;
	}

	// Nails don't touch each other.
	if ( dynamic_cast< CTFNailgunNail* >( pOther ) == NULL )
	{
		Vector vVelNorm = GetAbsVelocity();
		VectorNormalize( vVelNorm );

		Vector vDamageForce = vVelNorm;
		Vector vDamagePosition = GetAbsOrigin();

		int iStartHealth = pOther->GetHealth();
		CTakeDamageInfo info( this, pevOwner, vDamageForce, vDamagePosition, m_iDamage, DMG_SLASH );
		pOther->TakeDamage( info );
		
		// Did they take the damage?
		if ( pOther->GetHealth() < iStartHealth )
		{
			SpawnBlood( GetAbsOrigin(), GetAbsVelocity(), pOther->BloodColor(), m_iDamage );
		}

		// Do an impact trace in case we hit the world.
		trace_t tr;
		UTIL_TraceLine( 
			GetAbsOrigin() - vVelNorm * 30,
			GetAbsOrigin() + vVelNorm * 50,
			MASK_SOLID_BRUSHONLY,
			pevOwner,
			COLLISION_GROUP_DEBRIS,
			&tr );

		if ( tr.fraction < 1 )
		{
			UTIL_ImpactTrace( &tr, DMG_SLASH );
		}

		UTIL_Remove( this );
	}	
}

