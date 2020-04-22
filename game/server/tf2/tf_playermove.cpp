//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "player_command.h"
#include "tf_player.h"
#include "igamemovement.h"
#include "tf_shareddefs.h"
#include "in_buttons.h"
#include "tf_movedata.h"
#include "tf_class_recon.h"
#include "tf_reconvars.h"
#include "IserverVehicle.h"
#include "tf_class_commando.h"
#include "ipredictionsystem.h"

ConVar jetpack_infinite("jetpack_infinite", "0");
extern float g_JetpackDepleteRate;		// How fast the jetpack depletes.
extern float g_JetpackNegSnap;			// When the jetpack is fully depleted, it snaps to this so the pilot sputters.

static CTFMoveData g_TFMoveData;
CMoveData *g_pMoveData = &g_TFMoveData;

IPredictionSystem *IPredictionSystem::g_pPredictionSystems = NULL;


//-----------------------------------------------------------------------------
// Sets up the move data for TF2
//-----------------------------------------------------------------------------
class CTFPlayerMove : public CPlayerMove
{
DECLARE_CLASS( CTFPlayerMove, CPlayerMove );

public:
	virtual void	SetupMove( CBasePlayer *player, CUserCmd *ucmd, IMoveHelper *pHelper, CMoveData *move );
	virtual void	FinishMove( CBasePlayer *player, CUserCmd *ucmd, CMoveData *move );

private:

	void SetupMoveCommando( CBaseTFPlayer *pTFPlayer, CUserCmd *pUcmd, IMoveHelper *pHelper, CTFMoveData *pTFMove );
	void SetupMoveRecon( CBaseTFPlayer *pTFPlayer, CUserCmd *pUcmd, IMoveHelper *pHelper, CTFMoveData *pTFMove );

	void FinishMoveCommando( CBaseTFPlayer *pTFPlayer, CTFMoveData *pTFMove, CUserCmd *ucmd );
	void FinishMoveRecon( CBaseTFPlayer *pTFPlayer, CTFMoveData *pTFMove, CUserCmd *ucmd );
};

// PlayerMove Interface
static CTFPlayerMove g_PlayerMove;

//-----------------------------------------------------------------------------
// Singleton accessor
//-----------------------------------------------------------------------------
CPlayerMove *PlayerMove()
{
	return &g_PlayerMove;
}

//-----------------------------------------------------------------------------
// Main setup, finish
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Purpose: This is called pre player movement and copies all the data necessary
//          from the player for movement. (Server-side, the client-side version
//          of this code can be found in prediction.cpp.)
//-----------------------------------------------------------------------------
void CTFPlayerMove::SetupMove( CBasePlayer *player, CUserCmd *ucmd, IMoveHelper *pHelper, CMoveData *move )
{
	// Call the default SetupMove code.
	BaseClass::SetupMove( player, ucmd, pHelper, move );

	//
	// Convert to TF2 data.
	//
	CBaseTFPlayer *pTFPlayer = static_cast<CBaseTFPlayer*>( player );
	Assert( pTFPlayer );

	CTFMoveData *pTFMove = static_cast<CTFMoveData*>( move );
	Assert( pTFMove );

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

	pTFMove->m_nClassID = pTFPlayer->PlayerClass();

	IVehicle *pVehicle = player->GetVehicle();
	if (!pVehicle)
	{
		// Handle player class specific setup.
		switch( pTFPlayer->PlayerClass() )
		{
		case TFCLASS_RECON: 
			{ 
				SetupMoveRecon( pTFPlayer, ucmd, pHelper, pTFMove ); 
				break; 
			}
		case TFCLASS_COMMANDO: 
			{ 
				SetupMoveCommando( pTFPlayer, ucmd, pHelper, pTFMove ); 
				break;
			}
		default:
			{
	//			pTFMove->m_nClassID = TFCLASS_UNDECIDED;
				break;
			}
		}
	}
	else
	{
		pVehicle->SetupMove( player, ucmd, pHelper, move ); 
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFPlayerMove::SetupMoveRecon( CBaseTFPlayer *pTFPlayer, CUserCmd *pUcmd, IMoveHelper *pHelper, 
								    CTFMoveData *pTFMove )
{
	CPlayerClassRecon *pRecon = static_cast<CPlayerClassRecon*>( pTFPlayer->GetPlayerClass() );
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
void CTFPlayerMove::SetupMoveCommando( CBaseTFPlayer *pTFPlayer, CUserCmd *pUcmd, IMoveHelper *pHelper, 
								       CTFMoveData *pTFMove )
{
	CPlayerClassCommando *pCommando = static_cast<CPlayerClassCommando*>( pTFPlayer->GetPlayerClass() );
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
// Purpose: This is called post player movement to copy back all data that
//          movement could have modified and that is necessary for future
//          movement. (Server-side, the client-side version of this code can 
//          be found in prediction.cpp.)
//-----------------------------------------------------------------------------
void CTFPlayerMove::FinishMove( CBasePlayer *player, CUserCmd *ucmd, CMoveData *move )
{
	// Call the default FinishMove code.
	BaseClass::FinishMove( player, ucmd, move );

	//
	// Convert to TF2 data.
	//
	CBaseTFPlayer *pTFPlayer = static_cast<CBaseTFPlayer*>( player );
	Assert( pTFPlayer );

	CTFMoveData *pTFMove = static_cast<CTFMoveData*>( move );
	Assert( pTFMove );

	// The class had better not have changed during the move!!
	Assert( pTFMove->m_nClassID == pTFPlayer->PlayerClass() );

	IVehicle *pVehicle = player->GetVehicle();
	if (!pVehicle)
	{
		// Handle player class specific setup.
		switch( pTFPlayer->PlayerClass() )
		{
		case TFCLASS_RECON: 
			{ 
				FinishMoveRecon( pTFPlayer, pTFMove, ucmd ); 
				break; 
			}
		case TFCLASS_COMMANDO: 
			{ 
				FinishMoveCommando( pTFPlayer, pTFMove, ucmd ); 
				break;
			}
		default:
			{
				break;
			}
		}
	}
	else
	{
		// Similarly, the vehicle had better not have changed during the move!
		Assert( pTFPlayer->IsInAVehicle() );
		pVehicle->FinishMove( pTFPlayer, ucmd, pTFMove );
	}

	//
	// Player movement data.
	//

	// Copy the position delta.
	pTFPlayer->m_vecPosDelta = pTFMove->m_vecPosDelta;

	COMPILE_TIME_ASSERT( CBaseTFPlayer::MOMENTUM_MAXSIZE == CTFMoveData::MOMENTUM_MAXSIZE );
	
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
void CTFPlayerMove::FinishMoveRecon( CBaseTFPlayer *pTFPlayer, CTFMoveData *pTFMove, 
									 CUserCmd *ucmd )
{
	CPlayerClassRecon *pRecon = static_cast<CPlayerClassRecon*>( pTFPlayer->GetPlayerClass() );
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
void CTFPlayerMove::FinishMoveCommando( CBaseTFPlayer *pTFPlayer, CTFMoveData *pTFMove, 
									    CUserCmd *ucmd )
{
	CPlayerClassCommando *pCommando = static_cast<CPlayerClassCommando*>( pTFPlayer->GetPlayerClass() );
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
