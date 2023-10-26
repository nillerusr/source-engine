//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: Spatial entity with simple radial falloff
//
// $NoKeywords: $
//===========================================================================//
#include "cbase.h"

#include "c_spatialentity.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


IMPLEMENT_CLIENTCLASS_DT(C_SpatialEntity, DT_SpatialEntity, CSpatialEntity)
	RecvPropVector( RECVINFO(m_vecOrigin) ),
	RecvPropFloat(  RECVINFO(m_minFalloff) ),
	RecvPropFloat(  RECVINFO(m_maxFalloff) ),
	RecvPropFloat(  RECVINFO(m_flCurWeight) ),
	RecvPropBool(   RECVINFO(m_bEnabled) ),
END_RECV_TABLE()


void C_SpatialEntity::OnDataChanged(DataUpdateType_t updateType)
{
	BaseClass::OnDataChanged( updateType );

	if ( updateType == DATA_UPDATE_CREATED )
	{
		InitSpatialEntity();
	}
}

void C_SpatialEntity::InitSpatialEntity()
{
	SetNextClientThink( CLIENT_THINK_ALWAYS );

	AddToPersonalSpatialEntityMgr();
	m_flWeight = 0.0f;
	m_flInfluence = 0.0f;
}

void C_SpatialEntity::UpdateOnRemove( void )
{
	BaseClass::UpdateOnRemove();

	RemoveFromPersonalSpatialEntityMgr();
}


//------------------------------------------------------------------------------
// We don't draw...
//------------------------------------------------------------------------------
bool C_SpatialEntity::ShouldDraw()
{
	return false;
}

void C_SpatialEntity::ClientThink()
{
	if( !m_bEnabled && m_flCurWeight == 0.0f )
	{
		m_flWeight = 0.0f;
		return;
	}

	CBaseEntity *pPlayer = C_BasePlayer::GetLocalPlayer( 0 );
	if( !pPlayer )
		return;

	Vector playerOrigin = pPlayer->GetAbsOrigin();

	m_flWeight = 0.0f;

	if ( ( m_minFalloff != -1 ) && ( m_maxFalloff != -1 ) && m_minFalloff != m_maxFalloff )
	{
		float dist = (playerOrigin - m_vecOrigin).Length();
		m_flWeight = (dist-m_minFalloff) / (m_maxFalloff-m_minFalloff);
		m_flWeight = fpmax( 0.0f, m_flWeight );
		m_flWeight = fpmin( 1.0f, m_flWeight );

		m_flInfluence = fpmax( 0.0f, m_minFalloff - dist );
	}

	m_flWeight = m_flCurWeight * ( 1.0f - m_flWeight );

	BaseClass::ClientThink();
}
