//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef CLOCKDRIFTMGR_H
#define CLOCKDRIFTMGR_H
#ifdef _WIN32
#pragma once
#endif


class CClockDriftMgr
{
friend class CBaseClientState;

public:
	CClockDriftMgr();

	// Is clock correction even enabled right now?
	static bool IsClockCorrectionEnabled();

	// Clear our state.
	void Clear();

	// This is called each time a server packet comes in. It is used to correlate
	// where the server is in time compared to us.
	void SetServerTick( int iServerTick );
	
	// Pass in the frametime you would use, and it will drift it towards the server clock.
	float AdjustFrameTime( float inputFrameTime );

	// Returns how many ticks ahead of the server the client is.
	float GetCurrentClockDifference() const;


private:

	void ShowDebugInfo( float flAdjustment );

	// This scales the offsets so the average produced is equal to the
	// current average + flAmount. This way, as we add corrections,
	// we lower the average accordingly so we don't keep responding
	// as much as we need to after we'd adjusted it a couple times.
	void AdjustAverageDifferenceBy( float flAmountInSeconds );


private:

	enum
	{
		// This controls how much it smoothes out the samples from the server.
		NUM_CLOCKDRIFT_SAMPLES=16
	};

	// This holds how many ticks the client is ahead each time we get a server tick.
	// We average these together to get our estimate of how far ahead we are.
	float m_ClockOffsets[NUM_CLOCKDRIFT_SAMPLES];
	int m_iCurClockOffset;

	int m_nServerTick;		// Last-received tick from the server.
	int	m_nClientTick;		// The client's own tick counter (specifically, for interpolation during rendering).
							// The server may be on a slightly different tick and the client will drift towards it.
};


#endif // CLOCKDRIFTMGR_H
