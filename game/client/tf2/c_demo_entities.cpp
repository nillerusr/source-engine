//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "c_demo_entities.h"


IMPLEMENT_CLIENTCLASS_DT(C_Cycler_TF2Commando, DT_Cycler_TF2Commando, CCycler_TF2Commando)
	RecvPropInt( RECVINFO(m_bShieldActive) ),
	RecvPropFloat( RECVINFO(m_flShieldRaiseTime) ),
	RecvPropFloat( RECVINFO(m_flShieldLowerTime) ),
END_RECV_TABLE()

C_Cycler_TF2Commando::C_Cycler_TF2Commando()
{
}

float C_Cycler_TF2Commando::GetShieldRaiseTime() const
{
	return m_bShieldActive ? gpGlobals->curtime - m_flShieldRaiseTime : 0.0f;
}

float C_Cycler_TF2Commando::GetShieldLowerTime() const
{
	return !m_bShieldActive ? gpGlobals->curtime - m_flShieldLowerTime : 0.0f;
}

bool C_Cycler_TF2Commando::IsShieldActive() const
{
	return m_bShieldActive;
}

