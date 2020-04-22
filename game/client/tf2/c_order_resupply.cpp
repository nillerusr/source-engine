//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "c_order_resupply.h"


IMPLEMENT_CLIENTCLASS_DT( C_OrderResupply, DT_OrderResupply, COrderResupply )
END_RECV_TABLE()


void C_OrderResupply::GetDescription( char *pDest, int bufferSize )
{
	Q_strncpy( pDest, "Build Resupply Station Near Object", bufferSize );
}

