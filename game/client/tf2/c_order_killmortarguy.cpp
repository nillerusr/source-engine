//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "c_order_killmortarguy.h"


IMPLEMENT_CLIENTCLASS_DT( C_OrderKillMortarGuy, DT_OrderKillMortarGuy, COrderKillMortarGuy )
END_RECV_TABLE()


void C_OrderKillMortarGuy::GetDescription( char *pDest, int bufferSize )
{
	char targetDesc[512];
	GetTargetDescription( targetDesc, sizeof( targetDesc ) );

	Q_snprintf( pDest, bufferSize, "Kill Mortar Guy: %s", targetDesc );
}

