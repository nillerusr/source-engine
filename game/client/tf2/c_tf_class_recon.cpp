//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "c_tf_class_recon.h"

//=============================================================================
//
// Recon Data Table
//
BEGIN_RECV_TABLE_NOBASE( C_PlayerClassRecon, DT_PlayerClassReconData )
	RecvPropInt		( RECVINFO( m_ClassData.m_nJumpCount ) ),
	RecvPropFloat	( RECVINFO( m_ClassData.m_flSuppressionJumpTime ) ),
	RecvPropFloat	( RECVINFO( m_ClassData.m_flSuppressionImpactTime ) ),
	RecvPropFloat	( RECVINFO( m_ClassData.m_flStickTime ) ),
	RecvPropFloat	( RECVINFO( m_ClassData.m_flActiveJumpTime ) ),
	RecvPropFloat	( RECVINFO( m_ClassData.m_flImpactDist ) ),
	RecvPropVector	( RECVINFO( m_ClassData.m_vecImpactNormal ) ),
	RecvPropVector	( RECVINFO( m_ClassData.m_vecUnstickVelocity ) ),
	RecvPropInt		( RECVINFO( m_ClassData.m_bTrailParticles ) ),
END_RECV_TABLE()

BEGIN_PREDICTION_DATA_NO_BASE( C_PlayerClassRecon )

	DEFINE_PRED_TYPEDESCRIPTION( m_ClassData, PlayerClassReconData_t ),

END_PREDICTION_DATA()

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
C_PlayerClassRecon::C_PlayerClassRecon( C_BaseTFPlayer *pPlayer ) :
	C_PlayerClass( pPlayer )
{
	m_ClassData.m_nJumpCount = 0;
	m_ClassData.m_flSuppressionJumpTime = -99999;
	m_ClassData.m_flSuppressionImpactTime = -99999;
	m_ClassData.m_flActiveJumpTime = -99999;
	m_ClassData.m_flStickTime = -99999;
	m_ClassData.m_flImpactDist = -99999;
	m_ClassData.m_vecImpactNormal.Init();
	m_ClassData.m_vecUnstickVelocity.Init();
	m_ClassData.m_bTrailParticles = false;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
C_PlayerClassRecon::~C_PlayerClassRecon()
{
}
