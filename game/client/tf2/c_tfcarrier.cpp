//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Client's CObjectSentrygun
//
// $NoKeywords: $
//=============================================================================//
#include "c_base.h
#include "bone_setup.h"
#include "CommanderOverlay.h"
#include "c_ai_basenpc.h"
#include "c_tfcarrier.h"

IMPLEMENT_CLIENTCLASS_DT(C_TFCarrier, DT_TFCarrier, CTFCarrier)
	RecvPropInt(RECVINFO(m_iHealth)),
	RecvPropInt(RECVINFO(m_iMaxHealth)),
END_RECV_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_TFCarrier::C_TFCarrier()
{
	CONSTRUCT_MINIMAP_PANEL( "minimap_helicopter", MINIMAP_COLLECTORS );
}

void C_TFCarrier::SetDormant( bool inside )
{
	BaseClass::SetDormant( bDormant );
	ENTITY_PANEL_ACTIVATE( "helicopter", !bDormant );
}
