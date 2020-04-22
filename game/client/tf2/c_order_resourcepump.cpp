//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "c_order_resourcepump.h"
#include "tf_hints.h"


IMPLEMENT_CLIENTCLASS_DT( C_OrderResourcePump, DT_OrderResourcePump, COrderResourcePump )
END_RECV_TABLE()


C_OrderResourcePump::C_OrderResourcePump()
{
	m_nHintID = TF_HINT_BUILDRESOURCEPUMP;
}


void C_OrderResourcePump::GetDescription( char *pDest, int bufferSize )
{
	Q_strncpy( pDest, "Build Resource Pump", bufferSize );
}

