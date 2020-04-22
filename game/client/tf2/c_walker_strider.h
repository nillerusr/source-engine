//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef C_WALKER_STRIDER_H
#define C_WALKER_STRIDER_H
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


class C_WalkerStrider : public C_WalkerBase
{
public:
	DECLARE_CLIENTCLASS();
	DECLARE_CLASS( C_WalkerStrider, C_WalkerBase );

	C_WalkerStrider();
	virtual ~C_WalkerStrider();


// IClientThinkable.
public:
	virtual void ClientThink();


// C_BaseEntity.
public:
	virtual void ReceiveMessage( int classID, bf_read &msg );
	virtual void OnDataChanged( DataUpdateType_t type );
	virtual bool ShouldPredict() { return false; }


// C_BaseAnimating.
public:
	virtual bool GetAttachment( int iAttachment, matrix3x4_t &attachmentToWorld );
	virtual int DrawModel( int flags );


private:
	C_WalkerStrider( const C_WalkerStrider &other ) {}

	bool m_bCrouched;

	CUtlLinkedList<CStriderBeamEffect,int> m_BeamEffects;

	CHandle<C_RopeKeyframe> m_hRopes[STRIDER_NUM_ROPES];
};


#endif // C_WALKER_STRIDER_H
