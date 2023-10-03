//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//
#include "cbase.h"
#include "IViewRender.h"
#include "precache_register.h"
#include "studio.h"
#include "bone_setup.h"
#include "engine/ivmodelinfo.h"
#include "c_fire_smoke.h"
#include "engine/IEngineSound.h"
#include "iefx.h"
#include "dlight.h"
#include "tier0/ICommandLine.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


PRECACHE_REGISTER_BEGIN( GLOBAL, SmokeStackMaterials )
	PRECACHE( MATERIAL, "particle/SmokeStack" )
PRECACHE_REGISTER_END()


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pRecvProp - 
//			*pStruct - 
//			*pVarData - 
//			*pIn - 
//			objectID - 
//-----------------------------------------------------------------------------
void RecvProxy_Scale( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	C_FireSmoke	*pFireSmoke	= (C_FireSmoke	*) pStruct;
	float scale				= pData->m_Value.m_Float;

	//If changed, update our internal information
	if ( ( pFireSmoke->m_flScale != scale ) && ( pFireSmoke->m_flScaleEnd != scale ) )
	{
		pFireSmoke->m_flScaleStart		= pFireSmoke->m_flScaleRegister;
		pFireSmoke->m_flScaleEnd		= scale;			
	}

	pFireSmoke->m_flScale = scale;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pRecvProp - 
//			*pStruct - 
//			*pVarData - 
//			*pIn - 
//			objectID - 
//-----------------------------------------------------------------------------
void RecvProxy_ScaleTime( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	C_FireSmoke	*pFireSmoke	= (C_FireSmoke	*) pStruct;
	float time				= pData->m_Value.m_Float;

	//If changed, update our internal information
	//if ( pFireSmoke->m_flScaleTime != time )
	{
		if ( time == -1.0f )
		{
			pFireSmoke->m_flScaleTimeStart	= Helper_GetTime()-1.0f;
			pFireSmoke->m_flScaleTimeEnd	= pFireSmoke->m_flScaleTimeStart;
		}
		else
		{
			pFireSmoke->m_flScaleTimeStart	= Helper_GetTime();
			pFireSmoke->m_flScaleTimeEnd	= Helper_GetTime() + time;
		}
	}

	pFireSmoke->m_flScaleTime = time;
}

//Receive datatable
IMPLEMENT_CLIENTCLASS_DT( C_FireSmoke, DT_FireSmoke, CFireSmoke )
	RecvPropFloat( RECVINFO( m_flStartScale )),
	RecvPropFloat( RECVINFO( m_flScale ), 0, RecvProxy_Scale ),
	RecvPropFloat( RECVINFO( m_flScaleTime ), 0, RecvProxy_ScaleTime ),
	RecvPropInt( RECVINFO( m_nFlags ) ),
	RecvPropInt( RECVINFO( m_nFlameModelIndex ) ),
	RecvPropInt( RECVINFO( m_nFlameFromAboveModelIndex ) ),
END_RECV_TABLE()

//==================================================
// C_FireSmoke
//==================================================

C_FireSmoke::C_FireSmoke()
{
}

C_FireSmoke::~C_FireSmoke()
{

	// Shut down our effect if we have it
	if ( m_hEffect )
	{
		m_hEffect->StopEmission(false, false , true);
		m_hEffect = NULL;
	}

}


#define	FLAME_ALPHA_START	0.9f
#define FLAME_ALPHA_END		1.0f

#define	FLAME_TRANS_START	0.75f

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_FireSmoke::AddFlames( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bnewentity - 
//-----------------------------------------------------------------------------
void C_FireSmoke::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );


	if ( updateType == DATA_UPDATE_CREATED )
	{
		Start();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_FireSmoke::UpdateEffects( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool C_FireSmoke::ShouldDraw()
{

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_FireSmoke::Start( void )
{
	const char *lpszEffectName;
	int nSize = (int) floor( m_flStartScale / 36.0f );
	switch ( nSize )
	{
	case 0:
		lpszEffectName = ( m_nFlags & bitsFIRESMOKE_SMOKE ) ? "env_fire_tiny_smoke" : "env_fire_tiny";
		break;

	case 1:
		lpszEffectName = ( m_nFlags & bitsFIRESMOKE_SMOKE ) ? "env_fire_small_smoke" : "env_fire_small";
		break;

	case 2:
		lpszEffectName = ( m_nFlags & bitsFIRESMOKE_SMOKE ) ? "env_fire_medium_smoke" : "env_fire_medium";
		break;

	case 3:
	default:
		lpszEffectName = ( m_nFlags & bitsFIRESMOKE_SMOKE ) ? "env_fire_large_smoke" : "env_fire_large";
		break;
	}

	// Create the effect of the correct size
	m_hEffect = ParticleProp()->Create( lpszEffectName, PATTACH_ABSORIGIN );

}


//-----------------------------------------------------------------------------
// Purpose: FIXME: what's the right way to do this?
//-----------------------------------------------------------------------------
void C_FireSmoke::StartClientOnly( void )
{
	Start();

	ClientEntityList().AddNonNetworkableEntity(	this );
	CollisionProp()->CreatePartitionHandle();
	AddEffects( EF_NORECEIVESHADOW | EF_NOSHADOW );
	AddToLeafSystem();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_FireSmoke::RemoveClientOnly(void)
{
	ClientThinkList()->RemoveThinkable( GetClientHandle() );

	// Remove from the client entity list.
	ClientEntityList().RemoveEntity( GetClientHandle() );

	partition->Remove( PARTITION_CLIENT_SOLID_EDICTS | PARTITION_CLIENT_RESPONSIVE_EDICTS | PARTITION_CLIENT_NON_STATIC_EDICTS, CollisionProp()->GetPartitionHandle() );

	RemoveFromLeafSystem();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_FireSmoke::UpdateAnimation( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_FireSmoke::UpdateFlames( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_FireSmoke::UpdateScale( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_FireSmoke::Update( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_FireSmoke::FindClipPlane( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: Spawn smoke (...duh)
//-----------------------------------------------------------------------------

void C_FireSmoke::SpawnSmoke( void )
{
}
