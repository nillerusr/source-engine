//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "c_order_repair.h"


IMPLEMENT_CLIENTCLASS_DT( C_OrderRepair, DT_OrderRepair, COrderRepair )
END_RECV_TABLE()


void C_OrderRepair::GetDescription( char *pDest, int bufferSize )
{
	Q_strncpy( pDest, "Repair Structure", bufferSize );
}

