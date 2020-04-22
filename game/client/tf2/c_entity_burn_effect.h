//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_ENTITY_BURN_EFFECT_H
#define C_ENTITY_BURN_EFFECT_H
#ifdef _WIN32
#pragma once
#endif

#include "c_baseentity.h"
#include "particle_util.h"
#include "particles_simple.h"

class C_EntityBurnEffect : public C_BaseEntity
{
public:

	DECLARE_CLASS( C_EntityBurnEffect, C_BaseEntity );
	DECLARE_CLIENTCLASS();

					C_EntityBurnEffect();


// Overrides.
public:

	virtual void	OnDataChanged( DataUpdateType_t updateType );
	virtual void	ClientThink();


private:
	int		m_hBurningEntity;	// todo: this should be an ehandle but base networkables aren't setup for ehandles yet.
	
	TimedEvent					m_Timer;
	CSmartPtr<CSimpleEmitter>	m_pEmitter;
	PMaterialHandle				m_hFireMaterial;
};


#endif // C_ENTITY_BURN_EFFECT_H
