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
#include "c_asw_prop_physics.h"
#include "tier0/vprof.h"
#include "ivrenderview.h"
#include "c_asw_egg.h"
#include "baseparticleentity.h"
#include "asw_util_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar asw_phys_prop_motion_blur_scale( "asw_phys_prop_motion_blur_scale", "0.5" );

IMPLEMENT_CLIENTCLASS_DT(C_ASW_Prop_Physics, DT_ASW_Prop_Physics, CASW_Prop_Physics)
	RecvPropBool( RECVINFO( m_bIsAimTarget ) ),
	RecvPropBool( RECVINFO( m_bOnFire ) ),
END_RECV_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_ASW_Prop_Physics::C_ASW_Prop_Physics( void ) :
	m_MotionBlurObject( this, asw_phys_prop_motion_blur_scale.GetFloat() )
{
	m_bClientOnFire = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_ASW_Prop_Physics::~C_ASW_Prop_Physics( void )
{
	UpdateFireEmitters();
}



// asw - potentially add the prop as an autoaim target
void C_ASW_Prop_Physics::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	if ( updateType == DATA_UPDATE_CREATED )
	{
		if (m_bIsAimTarget && !m_bMadeAimTarget)
		{
			m_bMadeAimTarget = true;
		}
	}

	UpdateFireEmitters();
}


void C_ASW_Prop_Physics::UpdateFireEmitters()
{
	bool bOnFire = (m_bOnFire && !IsEffectActive(EF_NODRAW));
	if (bOnFire != m_bClientOnFire)
	{
		m_bClientOnFire = bOnFire;
		if (m_bClientOnFire)
		{
			if ( !m_pBurningEffect )
			{
				m_pBurningEffect = UTIL_ASW_CreateFireEffect( this );
			}
			EmitSound( "ASWFire.BurningFlesh" );
		}
		else
		{
			if ( m_pBurningEffect )
			{
				ParticleProp()->StopEmission( m_pBurningEffect );
				m_pBurningEffect = NULL;
			}
			StopSound("ASWFire.BurningFlesh");
			if ( C_BaseEntity::IsAbsQueriesValid() )
				EmitSound("ASWFire.StopBurning");
		}
	}
}

void C_ASW_Prop_Physics::UpdateOnRemove( void )
{
	BaseClass::UpdateOnRemove();
	m_bOnFire = false;
	UpdateFireEmitters();
}