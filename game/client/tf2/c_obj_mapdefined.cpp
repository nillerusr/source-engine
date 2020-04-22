//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "hud.h"
#include "tf_shareddefs.h"
#include "c_obj_mapdefined.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_CLIENTCLASS_DT(C_ObjectMapDefined, DT_ObjectMapDefined, CObjectMapDefined)
	RecvPropString( RECVINFO(m_szCustomName) ),
END_RECV_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_ObjectMapDefined::C_ObjectMapDefined()
{
	memset( m_szCustomName, 0, sizeof(m_szCustomName) );
}


//-----------------------------------------------------------------------------
// Purpose: Get a text description for the object target
//-----------------------------------------------------------------------------
const char *C_ObjectMapDefined::GetTargetDescription( void ) const
{
	if ( m_szCustomName && m_szCustomName[0] )
		return m_szCustomName;

	return BaseClass::GetTargetDescription();
}