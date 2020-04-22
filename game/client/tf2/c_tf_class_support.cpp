//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "c_tf_class_support.h"

//=============================================================================
//
// Support Data Table
//
BEGIN_RECV_TABLE_NOBASE( C_PlayerClassSupport, DT_PlayerClassSupportData )
END_RECV_TABLE()

BEGIN_PREDICTION_DATA_NO_BASE( C_PlayerClassSupport )

	DEFINE_PRED_TYPEDESCRIPTION( m_ClassData, PlayerClassSupportData_t ),

END_PREDICTION_DATA()

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
C_PlayerClassSupport::C_PlayerClassSupport( C_BaseTFPlayer *pPlayer ) :
	C_PlayerClass( pPlayer )
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
C_PlayerClassSupport::~C_PlayerClassSupport()
{
}