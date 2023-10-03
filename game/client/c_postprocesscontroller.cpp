//====== Copyright © 1996-2008, Valve Corporation, All rights reserved. =======
//
// Purpose: stores map postprocess params
//
//=============================================================================
#include "cbase.h"
#include "C_PostProcessController.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifdef CPostProcessController
#undef CPostProcessController
#endif

IMPLEMENT_CLIENTCLASS_DT( C_PostProcessController, DT_PostProcessController, CPostProcessController )
	RecvPropArray3( RECVINFO_NAME( m_PostProcessParameters.m_flParameters[0], m_flPostProcessParameters ), POST_PROCESS_PARAMETER_COUNT, RecvPropFloat( RECVINFO_NAME( m_PostProcessParameters.m_flParameters[0], m_flPostProcessParameters[0] ) ) ),
	RecvPropBool( RECVINFO(m_bMaster) )
END_RECV_TABLE()

C_PostProcessController* C_PostProcessController::ms_pMasterController = NULL;

//-----------------------------------------------------------------------------
C_PostProcessController::C_PostProcessController( void )
: 	m_bMaster( false )
{
	if ( ms_pMasterController == NULL )
	{
		ms_pMasterController = this;
	}
}

//-----------------------------------------------------------------------------
C_PostProcessController::~C_PostProcessController( void )
{
	if ( ms_pMasterController == this )
	{
		ms_pMasterController = NULL;
	}
}

void C_PostProcessController::PostDataUpdate( DataUpdateType_t updateType )
{
	BaseClass::PostDataUpdate( updateType );

	if ( m_bMaster )
	{
		ms_pMasterController = this;
	}
}
