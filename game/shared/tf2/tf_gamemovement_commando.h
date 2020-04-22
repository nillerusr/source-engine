//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Auto Repair
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_GAMEMOVEMENT_COMMANDO_H
#define TF_GAMEMOVEMENT_COMMANDO_H
#ifdef _WIN32
#pragma once
#endif

#include "tf_gamemovement.h"
#include "tfclassdata_shared.h"

class CTFMoveData;

//=============================================================================
//
// Commando Game Movement Class
//
class CTFGameMovementCommando : public CTFGameMovement
{
public:

	DECLARE_CLASS( CTFGameMovementCommando, CTFGameMovement );

	CTFGameMovementCommando();

	// Interface Implementation
	void			ProcessClassMovement( CBaseTFPlayer *pPlayer, CTFMoveData *pTFMoveData );
	Vector const   &GetPlayerMins( bool bDucked ) const;
	Vector const   &GetPlayerMaxs( bool bDucked ) const;
	Vector const   &GetPlayerViewOffset( bool bDucked ) const;

protected:

	// TF2 movement overrides.
	bool			PrePlayerMove( void );
	void			HandlePlayerMove( void );
	void			HandleDuck( void );

	void			SetupViewAngles( void );
	void			UpdateTimers( void );
	void			SetupSpeed( void );

	bool			CalcWishVelocityAndPosition( Vector &vWishPos, Vector &vWishDir, float &flWishSpeed );

	bool			CheckDoubleTapForward( void );

	void			CheckBullRush( void );
	void			BullRushMove( void );

	PlayerClassCommandoData_t  *m_pCommandoData; 
	Vector						m_vStandMins;
	Vector						m_vStandMaxs;
	Vector						m_vStandViewOffset;
	Vector						m_vDuckMins;
	Vector						m_vDuckMaxs;
	Vector						m_vDuckViewOffset;
};

#endif // TF_GAMEMOVEMENT_COMMANDO_H