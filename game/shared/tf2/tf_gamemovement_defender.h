//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Auto Repair
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_GAMEMOVEMENT_DEFENDER_H
#define TF_GAMEMOVEMENT_DEFENDER_H
#ifdef _WIN32
#pragma once
#endif

#include "tf_gamemovement.h"
#include "tfclassdata_shared.h"

class CTFMoveData;

//=============================================================================
//
// Defender Game Movement Class
//
class CTFGameMovementDefender : public CTFGameMovement
{

	DECLARE_CLASS( CTFGameMovementDefender, CTFGameMovement );

public:

	CTFGameMovementDefender();

	// Interface Implementation
//	virtual void ProcessMovement( CTFMoveData *pTFMoveData );
	virtual void ProcessClassMovement( CBaseTFPlayer *pPlayer, CTFMoveData *pTFMoveData );
	virtual const Vector &GetPlayerMins( bool bDucked ) const;
	virtual const Vector &GetPlayerMaxs( bool bDucked ) const;
	virtual const Vector &GetPlayerViewOffset( bool bDucked ) const;

protected:

	PlayerClassDefenderData_t	*m_pDefenderData; 
	Vector						m_vStandMins;
	Vector						m_vStandMaxs;
	Vector						m_vStandViewOffset;
	Vector						m_vDuckMins;
	Vector						m_vDuckMaxs;
	Vector						m_vDuckViewOffset;
};

#endif // TF_GAMEMOVEMENT_DEFENDER_H