//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "tf_obj_barbed_wire.h"
#include "tf_player.h"
#include "basetfvehicle.h"
#include "engine/IEngineSound.h"
#include "te_effect_dispatch.h"
#include "ndebugoverlay.h"

#define BARBED_WIRE_MINS	Vector(-5, -5, 0)
#define BARBED_WIRE_MAXS	Vector( 5,  5, 40)
#define BARBED_WIRE_MODEL	"models/objects/obj_barbed_wire.mdl"

#define MAX_BARBED_WIRE_DISTANCE	768
#define BARBED_WIRE_THINK_CONTEXT	"BarbedWireThinkContext"

#define BARBED_WIRE_THINK_INTERVAL	0.2

ConVar obj_barbed_wire_damage( "obj_barbed_wire_damage", "80" );
ConVar obj_barbed_wire_health( "obj_barbed_wire_health", "100" );


IMPLEMENT_SERVERCLASS_ST( CObjectBarbedWire, DT_ObjectBarbedWire )
	SendPropEHandle( SENDINFO( m_hConnectedTo ) )
END_SEND_TABLE()


LINK_ENTITY_TO_CLASS( obj_barbed_wire, CObjectBarbedWire );
PRECACHE_REGISTER( obj_barbed_wire );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CObjectBarbedWire::CObjectBarbedWire()
{
	m_iHealth = obj_barbed_wire_health.GetInt();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectBarbedWire::Precache()
{
	PrecacheModel( BARBED_WIRE_MODEL );
	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectBarbedWire::Spawn()
{
	Precache();

	SetModel( BARBED_WIRE_MODEL );
	SetSolid( SOLID_BBOX );
	SetType( OBJ_BARBED_WIRE );
	UTIL_SetSize(this, BARBED_WIRE_MINS, BARBED_WIRE_MAXS );

	// Set our flags.
	m_fObjectFlags |= OF_DOESNT_NEED_POWER | OF_SUPPRESS_APPEAR_ON_MINIMAP | OF_SUPPRESS_NOTIFY_UNDER_ATTACK | OF_ALLOW_REPEAT_PLACEMENT;

	// Get the ball rolling here.
	BarbedWireThink();

	BaseClass::Spawn();
}

//-----------------------------------------------------------------------------
// Purpose: Enumerator 
//-----------------------------------------------------------------------------
class CBarbedWireEnumerator : public IEntityEnumerator
{
public:
	CBarbedWireEnumerator( CObjectBarbedWire *pWire, Ray_t *pRay, int contentsMask )
	{
		m_pWire = pWire;
		m_pRay = pRay;
		m_ContentsMask = contentsMask;
		m_aDamagedEntities.Purge();
	}

	virtual bool EnumEntity( IHandleEntity *pHandleEntity )
	{
		CBaseEntity *pEntity = gEntList.GetBaseEntity( pHandleEntity->GetRefEHandle() );
		if ( pEntity && !m_pWire->InSameTeam( pEntity ) )
		{
			trace_t tr;
			enginetrace->ClipRayToEntity( *m_pRay, m_ContentsMask, pHandleEntity, &tr );
			if ( tr.fraction < 1.0f )
			{
				// Add them to the list & damage them later.
				// Done this way so entities don't remove themselves from leaves while we're tracing
				if ( m_aDamagedEntities.Find( pEntity ) == m_aDamagedEntities.InvalidIndex() )
				{
					m_aDamagedEntities.AddToTail( pEntity );
				}
			}
		}

		return true;
	}

	void DamageEntities( void )
	{
		int iSize = m_aDamagedEntities.Count();
		for ( int i = iSize-1; i >= 0; i-- )
		{
			CBaseEntity *pEntity = m_aDamagedEntities[i].Get();
			if ( pEntity )
			{
				// DMG_CRUSH added so there's no physics force generated
				CTakeDamageInfo info( m_pWire, m_pWire->GetBuilder(), obj_barbed_wire_damage.GetFloat() * BARBED_WIRE_THINK_INTERVAL, DMG_SLASH | DMG_CRUSH );
				pEntity->TakeDamage( info );

				// Bloodspray
				CEffectData	data;
				data.m_vOrigin = pEntity->WorldSpaceCenter();
				data.m_vNormal = Vector(0,0,1);
				data.m_flScale = 4;
				data.m_fFlags = FX_BLOODSPRAY_ALL;
				data.m_nEntIndex = pEntity->entindex();
				DispatchEffect( "tf2blood", data );
			}
		}
	}

private:
	CObjectBarbedWire		*m_pWire;
	int						m_ContentsMask;
	Ray_t		
		*m_pRay;
	CUtlVector< EHANDLE >	m_aDamagedEntities;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectBarbedWire::BarbedWireThink( void )
{
	// Cut the rope if we're too far from the entity it's attached to.
	if ( m_hConnectedTo.Get() )
	{
		if ( (WorldSpaceCenter() - m_hConnectedTo->WorldSpaceCenter()).Length() > MAX_BARBED_WIRE_DISTANCE )
		{
			m_hConnectedTo = NULL;
		}

		if ( m_hConnectedTo )
		{		
			Ray_t ray;
			ray.Init( WorldSpaceCenter(), m_hConnectedTo->WorldSpaceCenter() );
			
			//NDebugOverlay::Line( WorldSpaceCenter(), m_hConnectedTo->WorldSpaceCenter(), 255,255,255, false, 0.1 );
			//NDebugOverlay::Box( WorldSpaceCenter(), -Vector(5,5,5), Vector(5,5,5), 0,255,0,8, 0.1 );
			//NDebugOverlay::Box( m_hConnectedTo->WorldSpaceCenter(), -Vector(6,6,6), Vector(6,6,6), 255,255,255,8, 0.1 );

			CBarbedWireEnumerator bwEnum( this, &ray, MASK_SHOT_HULL );
			enginetrace->EnumerateEntities( ray, false, &bwEnum );
			bwEnum.DamageEntities();
		}
	}
	
	SetContextThink( BarbedWireThink, gpGlobals->curtime + BARBED_WIRE_THINK_INTERVAL, BARBED_WIRE_THINK_CONTEXT );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectBarbedWire::StartPlacement( CBaseTFPlayer *pPlayer )
{
	if ( pPlayer && !m_hConnectedTo )
	{
		// Automatically connect to the nearest barbed wire on our team.
		float flClosest = 1e24;
		CObjectBarbedWire *pClosest = NULL;
		
		CBaseEntity *pCur = gEntList.FirstEnt();
		while ( pCur )
		{
			CObjectBarbedWire *pWire = dynamic_cast< CObjectBarbedWire* >( pCur );
			if ( pWire )
			{
				if ( pWire->GetTeamNumber() == pPlayer->GetTeamNumber() )
				{
					float flDist = (pWire->WorldSpaceCenter() - pPlayer->WorldSpaceCenter()).Length();
					if ( flDist < flClosest )
					{
						flClosest = flDist;
						pClosest = pWire;
					}
				}
			}

			pCur = gEntList.NextEnt( pCur );
		}

		if ( pClosest )
		{
			m_hConnectedTo = pClosest;
		}
	}

	BaseClass::StartPlacement( pPlayer );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CObjectBarbedWire::PreStartBuilding()
{
	const CObjectBarbedWire *pWire = dynamic_cast< const CObjectBarbedWire* >( m_hBuiltOnEntity.Get() );
	if ( pWire )
	{
		// Reconnect the wire to this entity and don't build yet.
		m_hConnectedTo = pWire;

		return false;
	}
	else
	{
		// Go ahead and build.
		return true;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectBarbedWire::FinishedBuilding()
{
	BaseClass::FinishedBuilding();
}

