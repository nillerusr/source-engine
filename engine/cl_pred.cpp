//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
#include "client_pch.h"
#include "demo.h"
#include "host.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : world_start_state - 
//			validframe - 
//			start_command - 
//			stop_command - 
//-----------------------------------------------------------------------------
void CL_Predict( int world_start_state, bool validframe, int start_command, int stop_command )
{
	// Allow client .dll to do prediction.
	g_pClientSidePrediction->Update( 
		world_start_state, 
		validframe, 
		start_command, 
		stop_command );
}

//-----------------------------------------------------------------------------
// Purpose: Determine if we received a new message, which means we need to
//  redo prediction
//-----------------------------------------------------------------------------
void CL_RunPrediction( PREDICTION_REASON reason )
{
	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );

	if ( !cl.IsActive() )
		return;

	if ( cl.m_nDeltaTick < 0 ) 
	{
		// no valid snapshot received yet
		return;
	}

	// Don't do prediction if skipping ahead during demo playback
	if ( demoplayer->IsSkipping() )
	{
		return;
	}

	//Msg( "%i pred reason %s\n", host_framecount, reason == PREDICTION_SIMULATION_RESULTS_ARRIVING_ON_SEND_FRAME ?
	//	"same frame" : "normal" );

	bool valid = cl.m_nDeltaTick > 0; // cl.GetReceiveList( cl.delta_tick ) != NULL;

	//Msg( "%i/%i CL_RunPrediction:  last ack %i most recent %i\n", 
	//	host_framecount, cl.tickcount,
	//	cl.last_command_ack, 
	//	cl.netchan->m_nOutSequenceNr - 1 );

	CL_Predict( cl.m_nDeltaTick, 
		valid,  
		cl.last_command_ack, 
		cl.lastoutgoingcommand + cl.chokedcommands );
}
