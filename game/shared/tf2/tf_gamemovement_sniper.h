//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Auto Repair
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_GAMEMOVEMENT_SNIPER_H
#define TF_GAMEMOVEMENT_SNIPER_H
#ifdef _WIN32
#pragma once
#endif

#include "tf_gamemovement.h"
#include "tfclassdata_shared.h"

class CTFMoveData;

//=============================================================================
//
// Sniper Game Movement Class
//
class CTFGameMovementSniper : public CTFGameMovement
{

	DECLARE_CLASS( CTFGameMovementSniper, CTFGameMovement );

public:

	CTFGameMovementSniper();

	// Interface Implementation
//	virtual void ProcessMovement( CTFMoveData *pTFMoveData );
	virtual void ProcessClassMovement( CBaseTFPlayer *pPlayer, CTFMoveData *pTFMoveData );
	virtual const Vector &GetPlayerMins( bool bDucked ) const;
	virtual const Vector &GetPlayerMaxs( bool bDucked ) const;
	virtual const Vector &GetPlayerViewOffset( bool bDucked ) const;

protected:

	PlayerClassSniperData_t	*m_pSniperData; 
	Vector					m_vStandMins;
	Vector					m_vStandMaxs;
	Vector					m_vStandViewOffset;
	Vector					m_vDuckMins;
	Vector					m_vDuckMaxs;
	Vector					m_vDuckViewOffset;
};

#endif // TF_GAMEMOVEMENT_SNIPER_H