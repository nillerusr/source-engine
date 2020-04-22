//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Auto Repair
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_GAMEMOVEMENT_RECON_H
#define TF_GAMEMOVEMENT_RECON_H
#ifdef _WIN32
#pragma once
#endif

#include "tf_gamemovement.h"
#include "tfclassdata_shared.h"

class CTFMoveData;

//=============================================================================
//
// Recon Game Movement Class
//
class CTFGameMovementRecon : public CTFGameMovement
{

	DECLARE_CLASS( CTFGameMovementRecon, CTFGameMovement );

public:

	CTFGameMovementRecon();

	// Interface Implementation
//	virtual void ProcessMovement( CTFMoveData *pTFMoveData );
	virtual void ProcessClassMovement( CBaseTFPlayer *pPlayer, CTFMoveData *pTFMoveData );
	virtual const Vector &GetPlayerMins( bool bDucked ) const;
	virtual const Vector &GetPlayerMaxs( bool bDucked ) const;
	virtual const Vector &GetPlayerViewOffset( bool bDucked ) const;

	// Purpose:
	virtual void			AirMove();

protected:
	virtual void			PostPlayerMove( void );

	void					UpdateTimers( void );

	// Purpose: Check the jump button to make various jumps
	bool					CheckJumpButton();

	// Check various jump types
	bool					CheckWaterJump();
	bool					CheckWallJump(CTFMoveData *pTFMove);
	bool					CheckBackJump( bool bWasInAir );
	bool					CheckStrafeJump( bool bWasInAir );
	bool					CheckForwardJump( bool bWasInAir );

	// Resets the impact time
	void					ResetWallImpact(CTFMoveData *pTFMove);

	// Implement this if you want to know when the player collides during OnPlayerMove
	virtual void			OnTryPlayerMoveCollision( trace_t &tr );

	PlayerClassReconData_t	*m_pReconData; 
	Vector					m_vStandMins;
	Vector					m_vStandMaxs;
	Vector					m_vStandViewOffset;
	Vector					m_vDuckMins;
	Vector					m_vDuckMaxs;
	Vector					m_vDuckViewOffset;
	bool					m_bPerformingAirMove;
};

#endif // TF_GAMEMOVEMENT_RECON_H
