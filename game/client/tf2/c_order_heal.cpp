//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "c_order_heal.h"


IMPLEMENT_CLIENTCLASS_DT( C_OrderHeal, DT_OrderHeal, COrderHeal )
END_RECV_TABLE()


void C_OrderHeal::GetDescription( char *pDest, int bufferSize )
{
	char targetDesc[512];
	GetTargetDescription( targetDesc, sizeof( targetDesc ) );

	Q_snprintf( pDest, bufferSize, "Heal %s", targetDesc );
}

