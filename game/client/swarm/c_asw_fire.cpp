//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
//---------------------------------------------------------
//---------------------------------------------------------
#include "cbase.h"
#include "c_asw_fire.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"



//==================================================
// C_Fire
//==================================================
IMPLEMENT_CLIENTCLASS_DT( C_Fire, DT_ASW_Fire, CFire )
RecvPropInt( RECVINFO( m_nFireType ) ),
RecvPropFloat( RECVINFO( m_flFireSize ) ),
RecvPropFloat( RECVINFO( m_flHeatLevel ) ),
RecvPropFloat( RECVINFO( m_flMaxHeat ) ),
RecvPropBool( RECVINFO( m_bEnabled ) ),
END_NETWORK_TABLE()


//==================================================
// C_Fire
//==================================================

C_Fire::C_Fire()
{
	m_nFireType = 0;
	m_flFireSize = 0.1f;
	m_flHeatLevel = 0;
	m_flMaxHeat = 64;
	m_bEnabled = false;
}


C_Fire::~C_Fire()
{
	if ( m_hFire )
	{
		m_hFire->StopEmission(false, false , true);
		m_hFire = NULL;
	}

	if ( m_hFireTop )
	{
		m_hFireTop->StopEmission(false, false , true);
		m_hFireTop = NULL;
	}
}

void C_Fire::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	if ( updateType == DATA_UPDATE_CREATED )
	{
		CreateFireParticles();		
		return;
	}

	if ( m_flHeatLevel > 0 && m_bEnabled && !m_hFire )
	{
		CreateFireParticles();	
	}
}

void C_Fire::CreateFireParticles()
{
	if ( !m_bEnabled )
		return;

	if ( m_nFireType == 1 )
		m_hFire = ParticleProp()->Create( "mine_fire", PATTACH_ABSORIGIN_FOLLOW );
	else
	{
		if ( m_flFireSize < 24 )
		{
			m_hFire = ParticleProp()->Create( "ground_fire_small", PATTACH_ABSORIGIN_FOLLOW );
			m_hFireTop = ParticleProp()->Create( "ground_fire_small_top", PATTACH_ABSORIGIN_FOLLOW );
		}
		else if ( m_flFireSize < 92 )
		{
			m_hFire = ParticleProp()->Create( "ground_fire", PATTACH_ABSORIGIN_FOLLOW );
			m_hFireTop = ParticleProp()->Create( "ground_fire_top", PATTACH_ABSORIGIN_FOLLOW );
		}
		else
		{
			m_hFire = ParticleProp()->Create( "ground_fire_large", PATTACH_ABSORIGIN_FOLLOW );
			m_hFireTop = ParticleProp()->Create( "ground_fire_large_top", PATTACH_ABSORIGIN_FOLLOW );
		}
		
	}

	if ( m_hFire )
	{			
		UpdateFireParticles();
		SetNextClientThink( CLIENT_THINK_ALWAYS );
	}
	else
	{
		Warning("Failed to create a fire emitter\n");
	}
}

void C_Fire::UpdateFireParticles()
{
	if ( m_hFire )
	{			
		//m_hFire->SetSortOrigin( GetAbsOrigin() );
		m_hFire->SetControlPoint( 0, GetAbsOrigin() );
		Vector vecForward, vecRight, vecUp;
		AngleVectors( GetAbsAngles(), &vecForward, &vecRight, &vecUp );
		m_hFire->SetControlPointOrientation( 0, vecForward, vecRight, vecUp );

		float flSize = 1.0f;
		if ( m_flFireSize < 24 )
			flSize = m_flFireSize / 12;
		else if ( m_flFireSize < 92 )
			flSize = m_flFireSize / 42;
		else
			flSize = MAX( m_flFireSize / 128, 0.25);

		float strength = (m_flHeatLevel / m_flMaxHeat) * MAX( flSize, 0.1);
		m_hFire->SetControlPoint( 1, Vector( strength, strength, 0 ) );
		m_hFire->SetControlPoint( 5, Vector( flSize, flSize, flSize ) );

		if ( m_nFireType != 1 && m_hFireTop )
		{
			m_hFireTop->SetControlPoint( 0, GetAbsOrigin() );
			m_hFireTop->SetControlPoint( 1, Vector( strength, strength, 0 ) );
			m_hFireTop->SetControlPoint( 5, Vector( flSize, flSize, flSize ) );
		}
	}
}

void C_Fire::ClientThink()
{
	BaseClass::ClientThink();

	UpdateFireParticles();

	if ( (m_flHeatLevel <= 0 || !m_bEnabled) && m_hFire )
	{
		m_hFire->StopEmission(false, false , true);
		m_hFire = NULL;

		if ( m_nFireType != 1 && m_hFireTop )
		{
			m_hFireTop->StopEmission(false, false , true);
			m_hFireTop = NULL;
		}
	}
}