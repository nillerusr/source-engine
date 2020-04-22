//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "c_order_assist.h"


IMPLEMENT_CLIENTCLASS_DT( C_OrderAssist, DT_OrderAssist, COrderAssist )
END_RECV_TABLE()


void C_OrderAssist::GetDescription( char *pDest, int bufferSize )
{
	char targetDesc[512];
	GetTargetDescription( targetDesc, sizeof( targetDesc ) );

	Q_snprintf( pDest, bufferSize, "Assist %s", targetDesc );
}

