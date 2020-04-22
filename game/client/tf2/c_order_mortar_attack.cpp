//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "c_order_mortar_attack.h"


IMPLEMENT_CLIENTCLASS_DT( C_OrderMortarAttack, DT_OrderMortarAttack, COrderMortarAttack )
END_RECV_TABLE()


void C_OrderMortarAttack::GetDescription( char *pDest, int bufferSize )
{
	char targetDesc[512];
	GetTargetDescription( targetDesc, sizeof( targetDesc ) );

	Q_snprintf( pDest, bufferSize, "Attack %s with mortar", targetDesc );
}

