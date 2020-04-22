//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "c_order_buildsentrygun.h"
#include "tf_hints.h"


IMPLEMENT_CLIENTCLASS_DT( C_OrderBuildSentryGun, DT_OrderBuildSentryGun, COrderBuildSentryGun )
END_RECV_TABLE()


C_OrderBuildSentryGun::C_OrderBuildSentryGun()
{
	m_nHintID = TF_HINT_BUILDSENTRYGUN_PLASMA;
}


void C_OrderBuildSentryGun::GetDescription( char *pDest, int bufferSize )
{
	Q_strncpy( pDest, "Build Sentry Gun To Protect Object", bufferSize );
}

