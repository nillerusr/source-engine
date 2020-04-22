//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "tfclassdata_shared.h"

BEGIN_PREDICTION_DATA_NO_BASE( PlayerClassCommandoData_t )

	DEFINE_PRED_FIELD( m_bCanBullRush, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bBullRush, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_vecBullRushDir, FIELD_VECTOR, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_vecBullRushViewDir, FIELD_VECTOR, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_vecBullRushViewGoalDir, FIELD_VECTOR, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_flBullRushTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_flDoubleTapForwardTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),

END_PREDICTION_DATA()

BEGIN_PREDICTION_DATA_NO_BASE( PlayerClassReconData_t )

	DEFINE_FIELD( m_nJumpCount, FIELD_INTEGER ),
	DEFINE_FIELD( m_flSuppressionJumpTime, FIELD_FLOAT ),
	DEFINE_FIELD( m_flSuppressionImpactTime, FIELD_FLOAT ),
	DEFINE_FIELD( m_flActiveJumpTime, FIELD_FLOAT ),
	DEFINE_FIELD( m_flStickTime, FIELD_FLOAT ),
	DEFINE_FIELD( m_vecImpactNormal, FIELD_VECTOR ),
	DEFINE_FIELD( m_flImpactDist, FIELD_FLOAT ),
	DEFINE_FIELD( m_vecUnstickVelocity, FIELD_VECTOR ),
	DEFINE_FIELD( m_bTrailParticles, FIELD_BOOLEAN ),

END_PREDICTION_DATA()

BEGIN_PREDICTION_DATA_NO_BASE( PlayerClassDefenderData_t )
END_PREDICTION_DATA()

BEGIN_PREDICTION_DATA_NO_BASE( PlayerClassEscortData_t )
END_PREDICTION_DATA()

BEGIN_PREDICTION_DATA_NO_BASE( PlayerClassInfiltratorData_t )
END_PREDICTION_DATA()

BEGIN_PREDICTION_DATA_NO_BASE( PlayerClassMedicData_t )
END_PREDICTION_DATA()

BEGIN_PREDICTION_DATA_NO_BASE( PlayerClassSniperData_t )
END_PREDICTION_DATA()

BEGIN_PREDICTION_DATA_NO_BASE( PlayerClassSupportData_t )
END_PREDICTION_DATA()

BEGIN_PREDICTION_DATA_NO_BASE( PlayerClassSapperData_t )
END_PREDICTION_DATA()

BEGIN_PREDICTION_DATA_NO_BASE( PlayerClassPyroData_t )
END_PREDICTION_DATA()
