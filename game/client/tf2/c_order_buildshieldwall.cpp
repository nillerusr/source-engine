//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "c_order_buildshieldwall.h"


IMPLEMENT_CLIENTCLASS_DT( C_OrderBuildShieldWall, DT_OrderBuildShieldWall, COrderBuildShieldWall )
END_RECV_TABLE()


void C_OrderBuildShieldWall::GetDescription( char *pDest, int bufferSize )
{
	Q_strncpy( pDest, "Build Shield Wall To Protect Object", bufferSize );
}

