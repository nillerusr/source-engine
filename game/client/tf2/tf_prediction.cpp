//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "prediction.h"
#include "tf_movedata.h"
#include "c_basetfplayer.h"
#include "c_tf_class_recon.h"
#include "c_tf_class_commando.h"

static CTFMoveData g_TFMoveData;
CMoveData *g_pMoveData = &g_TFMoveData;


class CTFPrediction : public CPrediction
{
DECLARE_CLASS( CTFPrediction, CPrediction );

public:
	virtual void	SetupMove( C_BasePlayer *player, CUserCmd *ucmd, IMoveHelper *pHelper, CMoveData *move );
	virtual void	FinishMove( C_BasePlayer *player, CUserCmd *ucmd, CMoveData *move );

private:

	void SetupMoveRecon( CUserCmd *ucmd, C_BaseTFPlayer *pTFPlayer, IMoveHelper *pHelper, 
					     CTFMoveData *pTFMove );
	void SetupMoveCommando( CUserCmd *ucmd, C_BaseTFPlayer *pTFPlayer, IMoveHelper *pHelper, 
					        CTFMoveData *pTFMove );

	void FinishMoveRecon( CTFMoveData *pTFMove, C_BaseTFPlayer *pTFPlayer );
	void FinishMoveCommando( CTFMoveData *pTFMove, C_BaseTFPlayer *pTFPlayer );
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPrediction::SetupMove( C_BasePlayer *player, CUserCmd *ucmd, IMoveHelper *pHelper, 
	CMoveData *move )
{
	// Call the default SetupMove code.
	BaseClass::SetupMove( player, ucmd, pHelper, move );

	//
	// Convert to TF2 data.
	//
	C_BaseTFPlayer *pTFPlayer = static_cast<C_BaseTFPlayer*>( player );
	Assert( pTFPlayer );

	CTFMoveData *pTFMove = static_cast<CTFMoveData*>( move );
	Assert( pTFMove );

	//
	// Handle player class specific setup.
	//
	pTFMove->m_nClassID = pTFPlayer->PlayerClass();
	switch( pTFPlayer->GetClass() )
	{
	case TFCLASS_RECON: 
		{ 
			SetupMoveRecon( ucmd, pTFPlayer, pHelper, pTFMove ); 
			break; 
		}
	case TFCLASS_COMMANDO: 
		{ 
			SetupMoveCommando( ucmd, pTFPlayer, pHelper, pTFMove ); 
			break;
		}
	default:
		{
//			pTFMove->m_nClassID = TFCLASS_UNDECIDED;
			break;
		}
	}

	//
	// Player movement data.
	//

	// Copy the position delta.
	pTFMove->m_vecPosDelta = pTFPlayer->m_vecPosDelta;

	// Copy the momentum data.
	pTFMove->m_iMomentumHead = pTFPlayer->m_iMomentumHead;
	for ( int iMomentum = 0; iMomentum < CTFMoveData::MOMENTUM_MAXSIZE; iMomentum++ )
	{
		pTFMove->m_aMomentum[iMomentum] = pTFPlayer->m_aMomentum[iMomentum];
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFPrediction::SetupMoveRecon( CUserCmd *ucmd, C_BaseTFPlayer *pTFPlayer, IMoveHelper *pHelper, 
								    CTFMoveData *pTFMove )
{
	C_PlayerClassRecon *pRecon = static_cast<C_PlayerClassRecon*>( pTFPlayer->GetPlayerClass() );
	if ( pRecon )
	{
		PlayerClassReconData_t *pReconData = pRecon->GetClassData();
		if ( pReconData )
		{
			pTFMove->ReconData().m_nJumpCount = pReconData->m_nJumpCount;
			pTFMove->ReconData().m_flSuppressionJumpTime = pReconData->m_flSuppressionJumpTime;
			pTFMove->ReconData().m_flSuppressionImpactTime = pReconData->m_flSuppressionImpactTime;
			pTFMove->ReconData().m_flActiveJumpTime = pReconData->m_flActiveJumpTime;
			pTFMove->ReconData().m_flStickTime = pReconData->m_flStickTime;
			pTFMove->ReconData().m_flImpactDist = pReconData->m_flImpactDist;
			pTFMove->ReconData().m_vecImpactNormal = pReconData->m_vecImpactNormal;
			pTFMove->ReconData().m_vecUnstickVelocity = pReconData->m_vecUnstickVelocity;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFPrediction::SetupMoveCommando( CUserCmd *ucmd, C_BaseTFPlayer *pTFPlayer, IMoveHelper *pHelper, 
								       CTFMoveData *pTFMove )
{
	C_PlayerClassCommando *pCommando = static_cast<C_PlayerClassCommando*>( pTFPlayer->GetPlayerClass() );
	if ( pCommando )
	{
		PlayerClassCommandoData_t *pCommandoData = pCommando->GetClassData();
		if ( pCommandoData )
		{
			pTFMove->CommandoData().m_bCanBullRush = pCommandoData->m_bCanBullRush;
			pTFMove->CommandoData().m_bBullRush = pCommandoData->m_bBullRush;
			pTFMove->CommandoData().m_vecBullRushDir = pCommandoData->m_vecBullRushDir;
			pTFMove->CommandoData().m_vecBullRushViewDir = pCommandoData->m_vecBullRushViewDir;
			pTFMove->CommandoData().m_vecBullRushViewGoalDir = pCommandoData->m_vecBullRushViewGoalDir;
			pTFMove->CommandoData().m_flBullRushTime = pCommandoData->m_flBullRushTime;
			pTFMove->CommandoData().m_flDoubleTapForwardTime = pCommandoData->m_flDoubleTapForwardTime;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPrediction::FinishMove( C_BasePlayer *player, CUserCmd *ucmd, CMoveData *move )
{
	// Call the default FinishMove code.
	BaseClass::FinishMove( player, ucmd, move );

	//
	// Convert to TF2 data.
	//
	C_BaseTFPlayer *pTFPlayer = static_cast<C_BaseTFPlayer*>( player );
	Assert( pTFPlayer );

	CTFMoveData *pTFMove = static_cast<CTFMoveData*>( move );
	Assert( pTFMove );

	//
	// Handle player class specific setup.
	//
	switch( pTFPlayer->PlayerClass() )
	{
	case TFCLASS_RECON: 
		{ 
			FinishMoveRecon( pTFMove, pTFPlayer ); 
			break; 
		}
	case TFCLASS_COMMANDO: 
		{ 
			FinishMoveCommando( pTFMove, pTFPlayer ); 
			break;
		}
	default:
		{
			break;
		}
	}

	//
	// Player movement data.
	//

	// Copy the position delta.
	pTFPlayer->m_vecPosDelta = pTFMove->m_vecPosDelta;

	// Copy the momentum data back (the movement may have updated it!).
	pTFPlayer->m_iMomentumHead = pTFMove->m_iMomentumHead;
	for ( int iMomentum = 0; iMomentum < CTFMoveData::MOMENTUM_MAXSIZE; iMomentum++ )
	{
		pTFPlayer->m_aMomentum[iMomentum] = pTFMove->m_aMomentum[iMomentum];
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFPrediction::FinishMoveRecon( CTFMoveData *pTFMove, C_BaseTFPlayer *pTFPlayer )
{
	C_PlayerClassRecon *pRecon = static_cast<C_PlayerClassRecon*>( pTFPlayer->GetPlayerClass() );
	if ( pRecon )
	{
		PlayerClassReconData_t *pReconData = pRecon->GetClassData();
		if ( pReconData )
		{
			pReconData->m_nJumpCount = pTFMove->ReconData().m_nJumpCount;
			pReconData->m_flSuppressionJumpTime = pTFMove->ReconData().m_flSuppressionJumpTime;
			pReconData->m_flSuppressionImpactTime = pTFMove->ReconData().m_flSuppressionImpactTime;
			pReconData->m_flActiveJumpTime = pTFMove->ReconData().m_flActiveJumpTime;
			pReconData->m_flStickTime = pTFMove->ReconData().m_flStickTime;
			pReconData->m_flImpactDist = pTFMove->ReconData().m_flImpactDist;
			pReconData->m_vecImpactNormal = pTFMove->ReconData().m_vecImpactNormal;
			pReconData->m_vecUnstickVelocity = pTFMove->ReconData().m_vecUnstickVelocity;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFPrediction::FinishMoveCommando( CTFMoveData *pTFMove, C_BaseTFPlayer *pTFPlayer )
{
	C_PlayerClassCommando *pCommando = static_cast<C_PlayerClassCommando*>( pTFPlayer->GetPlayerClass() );
	if ( pCommando )
	{
		PlayerClassCommandoData_t *pCommandoData = pCommando->GetClassData();
		if ( pCommandoData )
		{
			pCommandoData->m_bCanBullRush = pTFMove->CommandoData().m_bCanBullRush;
			pCommandoData->m_bBullRush = pTFMove->CommandoData().m_bBullRush;
			pCommandoData->m_vecBullRushDir = pTFMove->CommandoData().m_vecBullRushDir;
			pCommandoData->m_vecBullRushViewDir = pTFMove->CommandoData().m_vecBullRushViewDir;
			pCommandoData->m_vecBullRushViewGoalDir = pTFMove->CommandoData().m_vecBullRushViewGoalDir;
			pCommandoData->m_flBullRushTime = pTFMove->CommandoData().m_flBullRushTime;
			pCommandoData->m_flDoubleTapForwardTime = pTFMove->CommandoData().m_flDoubleTapForwardTime;
		}
	}
}

// Expose interface to engine
// Expose interface to engine
static CTFPrediction g_Prediction;

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CTFPrediction, IPrediction, VCLIENT_PREDICTION_INTERFACE_VERSION, g_Prediction );

CPrediction *prediction = &g_Prediction;

