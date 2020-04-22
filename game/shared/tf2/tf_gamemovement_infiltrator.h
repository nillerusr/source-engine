//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Auto Repair
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_GAMEMOVEMENT_INFILTRATOR_H
#define TF_GAMEMOVEMENT_INFILTRATOR_H
#ifdef _WIN32
#pragma once
#endif

#include "tf_gamemovement.h"
#include "tfclassdata_shared.h"

class CTFMoveData;

//=============================================================================
//
// Infiltrator Game Movement Class
//
class CTFGameMovementInfiltrator : public CTFGameMovement
{

	DECLARE_CLASS( CTFGameMovementInfiltrator, CTFGameMovement );

public:

	CTFGameMovementInfiltrator();

	// Interface Implementation
//	virtual void ProcessMovement( CTFMoveData *pTFMoveData );
	virtual void ProcessClassMovement( CBaseTFPlayer *pPlayer, CTFMoveData *pTFMoveData );
	virtual const Vector &GetPlayerMins( bool bDucked ) const;
	virtual const Vector &GetPlayerMaxs( bool bDucked ) const;
	virtual const Vector &GetPlayerViewOffset( bool bDucked ) const;

protected:

	PlayerClassInfiltratorData_t	*m_pInfiltratorData; 
	Vector							m_vStandMins;
	Vector							m_vStandMaxs;
	Vector							m_vStandViewOffset;
	Vector							m_vDuckMins;
	Vector							m_vDuckMaxs;
	Vector							m_vDuckViewOffset;
};

#endif // TF_GAMEMOVEMENT_INFILTRATOR_H