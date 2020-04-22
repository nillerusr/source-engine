//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef C_WALKER_MINISTRIDER_H
#define C_WALKER_MINISTRIDER_H
#ifdef _WIN32
#pragma once
#endif


#include "c_walker_base.h"
#include "c_rope.h"


#define STRIDER_BEAM_LIFETIME	1.0
#define STRIDER_BEAM_MATERIAL	"sprites/physbeam"
#define STRIDER_BEAM_WIDTH		25
#define STRIDER_NUM_ROPES		6


class CStriderBeamEffect
{
public:
	Vector m_vHitPos;
	float m_flStartTime;
};


class C_WalkerMiniStrider : public C_WalkerBase
{
public:
	DECLARE_CLIENTCLASS();
	DECLARE_CLASS( C_WalkerMiniStrider, C_WalkerBase );

	C_WalkerMiniStrider();
	virtual ~C_WalkerMiniStrider();


// IClientThinkable.
public:
	virtual void ClientThink();


// C_BaseEntity.
public:

	virtual bool ShouldPredict() { return false; }
	virtual void SetupMove( CBasePlayer *pPlayer, CUserCmd *ucmd, IMoveHelper *pHelper, CMoveData *move );


// C_BaseAnimating.
public:


private:
	C_WalkerMiniStrider( const C_WalkerMiniStrider &other ) {}
};


#endif // C_WALKER_MINISTRIDER_H
