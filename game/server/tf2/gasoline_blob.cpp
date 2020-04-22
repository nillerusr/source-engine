//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "gasoline_blob.h"
#include "gasoline_shared.h"
#include "utllinkedlist.h"
#include "fire_damage_mgr.h"
#include "tf_gamerules.h"


// Flamethrower blobs wait a bit before they cause damage so they don't hurt the guy
// shooting them.
#define BLOB_DAMAGE_WAIT_TIME	1.0


// At what heat level does an unlit blob ignite?
#define IGNITION_HEAT			0.1


#define FIRE_DAMAGE_SEARCH_DISTANCE	200		// It searches within this sphere for entities to damage.
#define FIRE_DAMAGE_DISTANCE		90		// This is how far fire can damage an entity from.


ConVar fire_enable( "fire_enable", "1", 0, "Enable or disable fire." );


// ------------------------------------------------------------------------------------------ //
// CGasolineBlob implementation.
// ------------------------------------------------------------------------------------------ //

IMPLEMENT_SERVERCLASS_ST_NOBASE( CGasolineBlob, DT_GasolineBlob )
	SendPropVector( SENDINFO(m_vecOrigin), -1,  SPROP_COORD ),
	SendPropEHandle (SENDINFO_NAME(m_hMoveParent, moveparent)),
	SendPropFloat( SENDINFO(m_flLitStartTime), -1, SPROP_NOSCALE ),
	
	SendPropFloat( SENDINFO(m_flCreateTime), -1, SPROP_NOSCALE ),
	SendPropFloat( SENDINFO(m_flMaxLifetime), -1, SPROP_NOSCALE ),
	
	SendPropInt( SENDINFO(m_iTeamNum),		TEAMNUM_NUM_BITS, 0 ),
	SendPropInt( SENDINFO( m_BlobFlags ), NUM_BLOB_FLAGS, SPROP_UNSIGNED ),
	SendPropVector( SENDINFO( m_vSurfaceNormal ), 0, SPROP_NORMAL ),
END_SEND_TABLE()


LINK_ENTITY_TO_CLASS( gasoline_blob, CGasolineBlob );


CUtlLinkedList<CGasolineBlob*, int> g_GasolineBlobs;


CGasolineBlob* CGasolineBlob::Create( 
	CBaseEntity *pOwner, 
	const Vector &vOrigin, 
	const Vector &vStartVelocity, 
	bool bUseGravity,
	float flAirLifetime,
	float flLifetime )
{
	CGasolineBlob *pBlob = (CGasolineBlob*)CreateEntityByName( "gasoline_blob" );
	if ( !pBlob )
		return NULL;

	// The "constructor".	
	pBlob->SetLocalOrigin( vOrigin );
	pBlob->SetAbsVelocity( vStartVelocity );
	pBlob->SetThink( &CGasolineBlob::Think );
	pBlob->SetNextThink( gpGlobals->curtime );
	pBlob->SetCollisionBounds( Vector( -GASOLINE_BLOB_RADIUS, -GASOLINE_BLOB_RADIUS, -GASOLINE_BLOB_RADIUS ), Vector( GASOLINE_BLOB_RADIUS, GASOLINE_BLOB_RADIUS, GASOLINE_BLOB_RADIUS ) );
	pBlob->SetMoveType( MOVETYPE_NONE );
	pBlob->SetSolid( SOLID_BBOX );
	pBlob->AddSolidFlags( FSOLID_NOT_SOLID );
	pBlob->AddEFlags( EFL_FORCE_CHECK_TRANSMIT );
	pBlob->m_BlobFlags = 0;
	pBlob->m_HeatLevel = 0;
	pBlob->m_hOwner = pOwner;
	pBlob->m_flCreateTime = gpGlobals->curtime;
	pBlob->m_flMaxLifetime = flLifetime;
	pBlob->m_takedamage = DAMAGE_YES;
	pBlob->m_flLitStartTime = 0;
	pBlob->ChangeTeam( pOwner->GetTeamNumber() );
	
	if ( bUseGravity )
		pBlob->m_BlobFlags |= BLOBFLAG_USE_GRAVITY;

	pBlob->m_flAirLifetime = flAirLifetime;
	pBlob->m_flTimeInAir = 0;
	
	pBlob->SetNextThink( gpGlobals->curtime );
	g_GasolineBlobs.AddToTail( pBlob );

	return pBlob;
}


CGasolineBlob::~CGasolineBlob()
{
	g_GasolineBlobs.Remove( g_GasolineBlobs.Find( this ) );
}


void CGasolineBlob::AddAutoBurnBlob( CGasolineBlob *pBlob )
{
	int index = m_AutoBurnBlobs.AddToTail();
	m_AutoBurnBlobs[index] = pBlob;
}


int CGasolineBlob::OnTakeDamage( const CTakeDamageInfo &info )
{
	m_HeatLevel += info.GetDamage();
	if ( m_HeatLevel >= IGNITION_HEAT )
		SetLit( true );

	return 0;
}


void CGasolineBlob::SetLit( bool bLit )
{
	if ( bLit != IsLit() )
	{
		if ( bLit )
		{
			m_BlobFlags |= BLOBFLAG_LIT;
			m_flLitStartTime = gpGlobals->curtime;
		}
		else
		{
			m_BlobFlags &= ~BLOBFLAG_LIT;
		}
	}
}


bool CGasolineBlob::IsLit() const
{
	return (m_BlobFlags & BLOBFLAG_LIT) != 0;
}


bool CGasolineBlob::IsStopped() const
{
	return (m_BlobFlags & BLOBFLAG_STOPPED) != 0;
}


void CGasolineBlob::AutoBurn_R( CGasolineBlob *pParent )
{
	SetLit( true );

	for ( int i=0; i < m_AutoBurnBlobs.Count(); i++ )
	{
		CGasolineBlob *pTestBlob = m_AutoBurnBlobs[i];

		if ( pTestBlob )
		{
			if ( pTestBlob != pParent )
				pTestBlob->AutoBurn_R( this );
		}
		else
		{
			m_AutoBurnBlobs.Remove( i );
			--i;
		}
	}
}


void CGasolineBlob::Think()
{
	if ( !fire_enable.GetInt() )
	{
		UTIL_Remove( this );
		return;
	}

	// Decay quickly while in the air.
	if ( !IsStopped() )
	{
		m_flTimeInAir += gpGlobals->frametime;
		if ( m_flTimeInAir >= m_flAirLifetime )
		{
			UTIL_Remove( this );
			return;
		}
	}

	float flLifetime = gpGlobals->curtime - m_flCreateTime;
	if ( flLifetime >= m_flMaxLifetime )
	{
		UTIL_Remove( this );
		return;
	}

	if ( IsLit() )
	{
		// Have we burnt out?
		float litPercent = 1 - (flLifetime / m_flMaxLifetime);
		if ( litPercent <= 0 )
		{
			UTIL_Remove( this );
			return;
		}

		// Look for nearby entities to burn.
		CBaseEntity *ents[512];
		float dists[512];
		int nEnts = FindBurnableEntsInSphere( ents, dists, ARRAYSIZE( ents ), GetAbsOrigin(), FIRE_DAMAGE_SEARCH_DISTANCE, m_hOwner );
		
		for ( int i=0; i < nEnts; i++ )
		{
			float flDistFromBorder = MAX( 0, FIRE_DAMAGE_DISTANCE - dists[i] );
			if ( flDistFromBorder <= 0 )
				continue;

			float flDamage = litPercent * flDistFromBorder / FIRE_DAMAGE_DISTANCE * FIRE_DAMAGE_PER_SEC;
			GetFireDamageMgr()->AddDamage( ents[i], m_hOwner, flDamage, !IsGasolineBlob( ents[i] ) );
		}

		// Ignite our "auto burn" blobs.
		AutoBurn_R( NULL );
	}

	// Figure out where we want to go.
	if ( !IsStopped() )
	{
		// Apply gravity.
		Vector vecNewVelocity = GetAbsVelocity();
		if ( m_BlobFlags & BLOBFLAG_USE_GRAVITY )
		{
			vecNewVelocity.z -= 800 * gpGlobals->frametime;
			SetAbsVelocity( vecNewVelocity );
		}
		
		Vector vNewPos = GetAbsOrigin() + vecNewVelocity * gpGlobals->frametime;

		// Can we go there?
		trace_t trace;
		UTIL_TraceLine( GetAbsOrigin(), vNewPos, CONTENTS_SOLID, NULL, COLLISION_GROUP_NONE, &trace );
		bool bStopped = (trace.fraction != 1);

		if ( !bStopped )
		{
			// Trace against shields.
			if ( TFGameRules()->IsTraceBlockedByWorldOrShield( GetAbsOrigin(), vNewPos, m_hOwner, DMG_BURN, &trace ) )
			{
				// Blobs just fizzle out when they hit a shield.
				UTIL_Remove( this );
			}
		}

		if( bStopped )
		{
			SetLocalOrigin( trace.endpos + trace.plane.normal * 2 );
			
			// Ok, we hit something. Stop moving.
			m_BlobFlags |= BLOBFLAG_STOPPED;
			m_vSurfaceNormal = trace.plane.normal;
		}
		else
		{
			SetLocalOrigin( vNewPos );
		}
	}	

	SetNextThink( gpGlobals->curtime );
}


bool IsGasolineBlob( CBaseEntity *pEnt )
{
	return FClassnameIs( pEnt, "gasoline_blob" );
}

