//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Dissolve entity to be attached to target entity. Serves two purposes:
//
//			1) An entity that can be placed by a level designer and triggered
//			   to ignite a target entity.
//
//			2) An entity that can be created at runtime to ignite a target entity.
//
//=============================================================================//

#include "cbase.h"

#include "EntityFreezing.h"
#include "baseanimating.h"
#include "ai_basenpc.h"
#include "dt_utlvector_send.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static const char *s_pElectroThinkContext = "ElectroThinkContext";

//-----------------------------------------------------------------------------
// Model 
//-----------------------------------------------------------------------------
#define FREEZING_SPRITE_NAME	"sprites/blueglow1.vmt"	


//-----------------------------------------------------------------------------
// Save/load 
//-----------------------------------------------------------------------------
BEGIN_DATADESC( CEntityFreezing )

	DEFINE_FIELD( m_vFreezingOrigin, FIELD_VECTOR ),
	DEFINE_AUTO_ARRAY( m_flFrozenPerHitbox, FIELD_FLOAT ),
	DEFINE_KEYFIELD( m_flFrozen, FIELD_FLOAT, "frozen" ),

	DEFINE_FIELD( m_bFinishFreezing, FIELD_BOOLEAN ),

	DEFINE_INPUTFUNC( FIELD_STRING, "Freeze", InputFreeze ),

END_DATADESC()


//-----------------------------------------------------------------------------
// Networking
//-----------------------------------------------------------------------------
IMPLEMENT_SERVERCLASS_ST( CEntityFreezing, DT_EntityFreezing )
	SendPropVector( SENDINFO(m_vFreezingOrigin), 0, SPROP_NOSCALE ),
	SendPropArray3( SENDINFO_ARRAY3(m_flFrozenPerHitbox), SendPropFloat( SENDINFO_ARRAY(m_flFrozenPerHitbox) ) ),
	SendPropFloat( SENDINFO( m_flFrozen ) ),
	SendPropBool( SENDINFO( m_bFinishFreezing ) ),
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( env_entity_freezing, CEntityFreezing );
PRECACHE_REGISTER( env_entity_freezing );


//-----------------------------------------------------------------------------
// Precache
//-----------------------------------------------------------------------------
void CEntityFreezing::Precache()
{
	if ( NULL_STRING != GetModelName() )
	{
		PrecacheModel( STRING( GetModelName() ) );
	}


}


//-----------------------------------------------------------------------------
// Spawn
//-----------------------------------------------------------------------------
void CEntityFreezing::Spawn()
{
	BaseClass::Spawn();
	Precache();
	UTIL_SetModel( this, STRING( GetModelName() ) );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : inputdata - 
//-----------------------------------------------------------------------------
void CEntityFreezing::InputFreeze( inputdata_t &inputdata )
{
	string_t strTarget = inputdata.value.StringID();

	if (strTarget == NULL_STRING)
	{
		strTarget = m_target;
	}

	CBaseEntity *pTarget = NULL;
	while ((pTarget = gEntList.FindEntityGeneric(pTarget, STRING(strTarget), this, inputdata.pActivator)) != NULL)
	{
		CBaseAnimating *pBaseAnim = pTarget->GetBaseAnimating();
		if ( pBaseAnim )
		{
			pBaseAnim->Freeze( m_flFrozen );
		}
	}
}



//-----------------------------------------------------------------------------
// Purpose: Creates a flame and attaches it to a target entity.
// Input  : pTarget - 
//-----------------------------------------------------------------------------
CEntityFreezing *CEntityFreezing::Create( CBaseAnimating *pTarget )
{
	if ( pTarget )
	{
		if ( pTarget->IsPlayer() )
		{
			// Simply immediately kill the player.
			CBasePlayer *pPlayer = assert_cast< CBasePlayer* >( pTarget );
			pPlayer->SetArmorValue( 0 );
			CTakeDamageInfo info( pPlayer, pPlayer, pPlayer->GetHealth(), DMG_GENERIC | DMG_REMOVENORAGDOLL | DMG_PREVENT_PHYSICS_FORCE );
			pPlayer->TakeDamage( info );
			return NULL;
		}
	}

	CEntityFreezing *pFreezing = (CEntityFreezing *) CreateEntityByName( "env_entity_freezing" );

	if ( pFreezing == NULL )
		return NULL;

	if ( pTarget )
	{
		pFreezing->AttachToEntity( pTarget );
		pFreezing->SetModelName( pTarget->GetModelName() );
		pFreezing->SetFrozen( pTarget->GetFrozenAmount() );
	}

	DispatchSpawn( pFreezing );

	// Send to the client even though we don't have a model
	pFreezing->AddEFlags( EFL_FORCE_CHECK_TRANSMIT );

	return pFreezing;
}

	
//-----------------------------------------------------------------------------
// Purpose: Attaches the flame to an entity and moves with it
// Input  : pTarget - target entity to attach to
//-----------------------------------------------------------------------------
void CEntityFreezing::AttachToEntity( CBaseEntity *pTarget )
{
	// So our dissolver follows the entity around on the server.
	SetParent( pTarget );
	SetLocalOrigin( vec3_origin );
	SetLocalAngles( vec3_angle );
}
