//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "c_order_respawnstation.h"


IMPLEMENT_CLIENTCLASS_DT( C_OrderRespawnStation, DT_OrderRespawnStation, COrderRespawnStation )
END_RECV_TABLE()


void C_OrderRespawnStation::GetDescription( char *pDest, int bufferSize )
{
	Q_strncpy( pDest, "Build Respawn Station Near Object", bufferSize );
}

