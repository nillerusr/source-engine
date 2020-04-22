//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "c_tf_class_commando.h"
#include "usercmd.h"

//=============================================================================
//
// Commando Data Table
//
BEGIN_RECV_TABLE_NOBASE( C_PlayerClassCommando, DT_PlayerClassCommandoData )
	RecvPropInt		( RECVINFO( m_ClassData.m_bCanBullRush ) ),
	RecvPropInt		( RECVINFO( m_ClassData.m_bBullRush ) ),
	RecvPropVector	( RECVINFO( m_ClassData.m_vecBullRushDir ) ),
	RecvPropVector	( RECVINFO( m_ClassData.m_vecBullRushViewDir ) ),
	RecvPropVector	( RECVINFO( m_ClassData.m_vecBullRushViewGoalDir ) ),
	RecvPropFloat	( RECVINFO( m_ClassData.m_flBullRushTime ) ),
	RecvPropFloat	( RECVINFO( m_ClassData.m_flDoubleTapForwardTime ) ),
END_RECV_TABLE()

BEGIN_PREDICTION_DATA_NO_BASE( C_PlayerClassCommando )

	DEFINE_PRED_TYPEDESCRIPTION( m_ClassData, PlayerClassCommandoData_t ),

END_PREDICTION_DATA()

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
C_PlayerClassCommando::C_PlayerClassCommando( C_BaseTFPlayer *pPlayer ) :
	C_PlayerClass( pPlayer )
{
	m_ClassData.m_bCanBullRush = false;
	m_ClassData.m_bBullRush = false;
	m_ClassData.m_vecBullRushDir.Init();
	m_ClassData.m_vecBullRushViewDir.Init();
	m_ClassData.m_vecBullRushViewGoalDir.Init();
	m_ClassData.m_flBullRushTime = COMMANDO_TIME_INVALID;
	m_ClassData.m_flDoubleTapForwardTime = COMMANDO_TIME_INVALID;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
C_PlayerClassCommando::~C_PlayerClassCommando()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_PlayerClassCommando::ClassThink( void )
{
	CheckBullRushState();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_PlayerClassCommando::PostClassThink( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_PlayerClassCommando::ClassPreDataUpdate( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_PlayerClassCommando::ClassOnDataChanged( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_PlayerClassCommando::CreateMove( float flInputSampleTime, CUserCmd *pCmd )
{
	if ( m_ClassData.m_bBullRush )
	{
		pCmd->viewangles = m_ClassData.m_vecBullRushViewDir;
		QAngle angles = m_ClassData.m_vecBullRushViewDir;
		engine->SetViewAngles( angles );
	} 
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool C_PlayerClassCommando::CanGetInVehicle( void )
{
	if ( m_ClassData.m_bBullRush )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_PlayerClassCommando::CheckBullRushState( void )
{
	if ( m_ClassData.m_bBullRush )
	{
		InterpolateBullRushViewAngles();
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void C_PlayerClassCommando::InterpolateBullRushViewAngles( void )
{
	// Determine the fraction.
	if ( m_ClassData.m_flBullRushTime < COMMANDO_BULLRUSH_VIEWDELTA_TEST )
	{
		m_ClassData.m_vecBullRushViewDir = m_ClassData.m_vecBullRushViewGoalDir;
		return;
	}

	float flFraction = 1.0f - ( ( m_ClassData.m_flBullRushTime - COMMANDO_BULLRUSH_VIEWDELTA_TEST ) / COMMANDO_BULLRUSH_VIEWDELTA_TIME );

	QAngle angCurrent;
	InterpolateAngles( m_ClassData.m_vecBullRushViewDir, m_ClassData.m_vecBullRushViewGoalDir, angCurrent, flFraction ); 

	NormalizeAngles( angCurrent );
	m_ClassData.m_vecBullRushViewDir = angCurrent;
}
