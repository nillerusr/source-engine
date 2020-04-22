//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Sapper's game movement
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_GAMEMOVEMENT_SAPPER_H
#define TF_GAMEMOVEMENT_SAPPER_H
#ifdef _WIN32
#pragma once
#endif

#include "tf_gamemovement.h"
#include "tfclassdata_shared.h"

class CTFMoveData;

//=============================================================================
//
// Sapper Game Movement Class
//
class CTFGameMovementSapper : public CTFGameMovement
{

	DECLARE_CLASS( CTFGameMovementSapper, CTFGameMovement );

public:

	CTFGameMovementSapper();

	// Interface Implementation
//	virtual void ProcessMovement( CTFMoveData *pTFMoveData );
	virtual void ProcessClassMovement( CBaseTFPlayer *pPlayer, CTFMoveData *pTFMoveData );
	virtual const Vector &GetPlayerMins( bool bDucked ) const;
	virtual const Vector &GetPlayerMaxs( bool bDucked ) const;
	virtual const Vector &GetPlayerViewOffset( bool bDucked ) const;

protected:

	PlayerClassSapperData_t	*m_pSapperData; 
	Vector						m_vStandMins;
	Vector						m_vStandMaxs;
	Vector						m_vStandViewOffset;
	Vector						m_vDuckMins;
	Vector						m_vDuckMaxs;
	Vector						m_vDuckViewOffset;
};

#endif // TF_GAMEMOVEMENT_SAPPER_H