//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "c_tf_class_sapper.h"

//=============================================================================
//
// Sapper Data Table
//
BEGIN_RECV_TABLE_NOBASE( C_PlayerClassSapper, DT_PlayerClassSapperData )
	RecvPropFloat( RECVINFO(m_flDrainedEnergy) ),
END_RECV_TABLE()

BEGIN_PREDICTION_DATA_NO_BASE( C_PlayerClassSapper )

	DEFINE_PRED_TYPEDESCRIPTION( m_ClassData, PlayerClassSapperData_t ),

END_PREDICTION_DATA()

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
C_PlayerClassSapper::C_PlayerClassSapper( C_BaseTFPlayer *pPlayer ) :
	C_PlayerClass( pPlayer )
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
C_PlayerClassSapper::~C_PlayerClassSapper()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float C_PlayerClassSapper::GetDrainedEnergy( void )
{
	return m_flDrainedEnergy;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_PlayerClassSapper::DeductDrainedEnergy( float flEnergy )
{
	m_flDrainedEnergy = MAX( 0, m_flDrainedEnergy - flEnergy );
}
