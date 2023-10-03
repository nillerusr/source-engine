//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#include "cbase.h"
#include "c_entityflame.h"
#include "particle_property.h"
#include "iefx.h"
#include "dlight.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Datadesc
//-----------------------------------------------------------------------------
IMPLEMENT_CLIENTCLASS_DT( C_EntityFlame, DT_EntityFlame, CEntityFlame )
	RecvPropEHandle(RECVINFO(m_hEntAttached)),
	RecvPropBool(RECVINFO(m_bCheapEffect)),
END_RECV_TABLE()


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_EntityFlame::C_EntityFlame( void ) : m_hEffect( NULL )
{
	m_hOldAttached = NULL;
	AddToEntityList( ENTITY_LIST_SIMULATE );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_EntityFlame::~C_EntityFlame( void )
{
	StopEffect();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_EntityFlame::StopEffect( void )
{
	if ( m_hEffect )
	{
		ParticleProp()->StopEmission( m_hEffect, true );
		m_hEffect = NULL;
	}

	if ( m_hEntAttached )
	{
		m_hEntAttached->RemoveFlag( FL_ONFIRE );
		m_hEntAttached->SetEffectEntity( NULL );
		m_hEntAttached->StopSound( "General.BurningFlesh" );
		m_hEntAttached->StopSound( "General.BurningObject" );
		m_hEntAttached = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_EntityFlame::UpdateOnRemove( void )
{
	StopEffect();
	BaseClass::UpdateOnRemove();
}

void C_EntityFlame::CreateEffect( void )
{
	if ( m_hEffect )
	{
		m_hOldAttached = m_hEntAttached;
		ParticleProp()->StopEmission( m_hEffect, true );
		m_hEffect = NULL;
	}

	C_BaseEntity *pEntity = m_hEntAttached;
	if ( pEntity && !pEntity->IsAbleToHaveFireEffect() )
		return;

	m_hEffect = ParticleProp()->Create( m_bCheapEffect ? "burning_gib_01" : "burning_character", PATTACH_ABSORIGIN_FOLLOW );
	if ( m_hEffect )
	{
		m_hOldAttached = m_hEntAttached;

		ParticleProp()->AddControlPoint( m_hEffect, 1, pEntity, PATTACH_ABSORIGIN_FOLLOW );
		m_hEffect->SetControlPoint( 0, GetAbsOrigin() );
		m_hEffect->SetControlPoint( 1, GetAbsOrigin() );
		m_hEffect->SetControlPointEntity( 0, pEntity );
		m_hEffect->SetControlPointEntity( 1, pEntity );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_EntityFlame::OnDataChanged( DataUpdateType_t updateType )
{
	if ( updateType == DATA_UPDATE_CREATED )
	{
		CreateEffect();
	}

	// FIXME: This is a bit of a shady path
	if ( updateType == DATA_UPDATE_DATATABLE_CHANGED )
	{
		// If our owner changed, then recreate the effect
		if ( m_hEntAttached != m_hOldAttached )
		{
			CreateEffect();
		}
	}

	BaseClass::OnDataChanged( updateType );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool C_EntityFlame::Simulate( void )
{
	if ( gpGlobals->frametime <= 0.0f )
		return true;

#ifdef HL2_EPISODIC 
	if ( IsEffectActive(EF_BRIGHTLIGHT) || IsEffectActive(EF_DIMLIGHT) )
	{
		dlight_t *dl = effects->CL_AllocDlight( index );
		dl->origin = GetAbsOrigin();
 		dl->origin[2] += 16;
		dl->color.r = 254;
		dl->color.g = 174;
		dl->color.b = 10;
		dl->radius = random->RandomFloat(400,431);
		dl->die = gpGlobals->curtime + 0.001;
	}

#endif // HL2_EPISODIC 

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_EntityFlame::ClientThink( void )
{
	StopEffect();
	Release();
}
