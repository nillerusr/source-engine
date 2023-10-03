//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "c_asw_playerlocaldata.h"
#include "dt_recv.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

BEGIN_RECV_TABLE_NOBASE( C_ASWPlayerLocalData, DT_ASWLocal )
	RecvPropEHandle( RECVINFO(m_hAutoAimTarget) ),
	RecvPropVector( RECVINFO(m_vecAutoAimPoint) ),
	RecvPropBool( RECVINFO(m_bAutoAimTarget) ),
END_RECV_TABLE()

BEGIN_PREDICTION_DATA_NO_BASE( C_ASWPlayerLocalData )
END_PREDICTION_DATA()

C_ASWPlayerLocalData::C_ASWPlayerLocalData()
{
}

