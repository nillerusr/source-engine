//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// cl_pred.h
#ifndef CL_PRED_H
#define CL_PRED_H
#ifdef _WIN32
#pragma once
#endif

typedef enum
{
	PREDICTION_SIMULATION_RESULTS_ARRIVING_ON_SEND_FRAME = 0,
	PREDICTION_NORMAL,
} PREDICTION_REASON;

void CL_RunPrediction( PREDICTION_REASON reason );

#endif // CL_PRED_H
