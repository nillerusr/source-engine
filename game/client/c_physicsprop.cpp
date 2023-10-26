//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//
#include "cbase.h"
#include "model_types.h"
#include "vcollide.h"
#include "vcollide_parse.h"
#include "solidsetdefaults.h"
#include "bone_setup.h"
#include "engine/ivmodelinfo.h"
#include "physics.h"
#include "view.h"
#include "c_physicsprop.h"
#include "tier0/vprof.h"
#include "ivrenderview.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_CLIENTCLASS_DT(C_PhysicsProp, DT_PhysicsProp, CPhysicsProp)
	RecvPropBool( RECVINFO( m_bAwake ) ),
	RecvPropInt( RECVINFO( m_spawnflags ) ),
END_RECV_TABLE()

ConVar r_PhysPropStaticLighting( "r_PhysPropStaticLighting", "1" );

// @MULTICORE (toml 9/18/2006): this visualization will need to be implemented elsewhere
ConVar r_visualizeproplightcaching( "r_visualizeproplightcaching", "0" );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_PhysicsProp::C_PhysicsProp( void )
{
	m_pPhysicsObject = NULL;
	m_takedamage = DAMAGE_YES;

	// default true so static lighting will get recomputed when we go to sleep
	m_bAwakeLastTime = true;

	m_bCanUseStaticLighting = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_PhysicsProp::~C_PhysicsProp( void )
{
}


// Used to indicate if we can use the static lighting baking
//-----------------------------------------------------------------------------
void C_PhysicsProp::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );

	m_bCanUseStaticLighting = modelinfo->UsesStaticLighting( GetModel() );
	if ( m_bCanUseStaticLighting )
	{
		CreateModelInstance();
	}
}


//-----------------------------------------------------------------------------
// Hooks for the fast model rendering path
//-----------------------------------------------------------------------------
IClientModelRenderable*	C_PhysicsProp::GetClientModelRenderable()
{
	// Can't do this debug mode through the fast path since it uses 
	// color modulation, which we don't support yet.
	if ( !BaseClass::GetClientModelRenderable() )
		return NULL;

	if ( ( m_bAwakeLastTime != m_bAwake ) && r_visualizeproplightcaching.GetBool() )
		return NULL;

	// NOTE: This works because GetClientModelRenderable() is only queried
	// if the prop is known by the viewrender system to be opaque already
	// so we need no other opacity checks here.
	return this;
}

bool C_PhysicsProp::GetRenderData( void *pData, ModelDataCategory_t nCategory )
{
	switch ( nCategory )
	{
	case MODEL_DATA_LIGHTING_MODEL:
		{
			// FIXME: Cache bCanBeStaticLit off in OnDataChanged?
			if ( !m_bCanUseStaticLighting || !r_PhysPropStaticLighting.GetBool() )
			{
				*(RenderableLightingModel_t*)pData = LIGHTING_MODEL_STANDARD;
				return true;
			}

			if ( m_bAwakeLastTime && !m_bAwake )
			{
				// transition to sleep, bake lighting now, once
				if ( !modelrender->RecomputeStaticLighting( GetModelInstance() ) )
				{
					// Failed to bake, don't indicate it's asleep yet, use normal model
					*(RenderableLightingModel_t*)pData = LIGHTING_MODEL_STANDARD;
					return true;
				}
			}

			*(RenderableLightingModel_t*)pData = m_bAwake ? LIGHTING_MODEL_STANDARD : LIGHTING_MODEL_PHYSICS_PROP;
			m_bAwakeLastTime = m_bAwake;
		}
		return true;

	default:
		return BaseClass::GetRenderData( pData, nCategory );
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// Purpose: Draws the object
// Input  : flags - 
//-----------------------------------------------------------------------------
bool C_PhysicsProp::OnInternalDrawModel( ClientModelRenderInfo_t *pInfo )
{
	if ( !m_bCanUseStaticLighting || !r_PhysPropStaticLighting.GetBool() )
		return true;

	if ( m_bAwakeLastTime != m_bAwake )
	{
		if ( m_bAwakeLastTime && !m_bAwake )
		{
			// transition to sleep, bake lighting now, once
			if ( !modelrender->RecomputeStaticLighting( GetModelInstance() ) )
			{
				// not valid for drawing
				return false;
			}

			if ( r_visualizeproplightcaching.GetBool() )
			{
				float color[] = { 0.0f, 1.0f, 0.0f, 1.0f };
				render->SetColorModulation( color );
			}
		}
		else if ( r_visualizeproplightcaching.GetBool() )
		{
			float color[] = { 1.0f, 0.0f, 0.0f, 1.0f };
			render->SetColorModulation( color );
		}
	}

	if ( !m_bAwake )
	{
		// going to sleep, have static lighting
		pInfo->flags |= STUDIO_STATIC_LIGHTING;
	}
	
	// track state
	m_bAwakeLastTime = m_bAwake;
	return true;
}
