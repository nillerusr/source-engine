//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "asw_playerlocaldata.h"
#include "asw_player.h"
#include "mathlib/mathlib.h"
#include "entitylist.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

BEGIN_SEND_TABLE_NOBASE( CASWPlayerLocalData, DT_ASWLocal )
	SendPropEHandle( SENDINFO(m_hAutoAimTarget) ),
	SendPropVector( SENDINFO(m_vecAutoAimPoint) ),
	SendPropBool( SENDINFO(m_bAutoAimTarget) ),
END_SEND_TABLE()

BEGIN_SIMPLE_DATADESC( CASWPlayerLocalData )
END_DATADESC()

CASWPlayerLocalData::CASWPlayerLocalData()
{
	m_hAutoAimTarget.Set(NULL);
	m_vecAutoAimPoint.GetForModify().Init();
}

