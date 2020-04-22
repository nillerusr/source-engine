//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "prediction.h"
#include "c_baseplayer.h"
#include "igamemovement.h"


static CMoveData g_MoveData;
CMoveData *g_pMoveData = &g_MoveData;


class CTFCPrediction : public CPrediction
{
DECLARE_CLASS( CTFCPrediction, CPrediction );

public:
	virtual void	SetupMove( C_BasePlayer *player, CUserCmd *ucmd, IMoveHelper *pHelper, CMoveData *move );
	virtual void	FinishMove( C_BasePlayer *player, CUserCmd *ucmd, CMoveData *move );
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFCPrediction::SetupMove( C_BasePlayer *player, CUserCmd *ucmd, IMoveHelper *pHelper, 
	CMoveData *move )
{
	// Call the default SetupMove code.
	BaseClass::SetupMove( player, ucmd, pHelper, move );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFCPrediction::FinishMove( C_BasePlayer *player, CUserCmd *ucmd, CMoveData *move )
{
	// Call the default FinishMove code.
	BaseClass::FinishMove( player, ucmd, move );
}


// Expose interface to engine
// Expose interface to engine
static CTFCPrediction g_Prediction;

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CTFCPrediction, IPrediction, VCLIENT_PREDICTION_INTERFACE_VERSION, g_Prediction );

CPrediction *prediction = &g_Prediction;

