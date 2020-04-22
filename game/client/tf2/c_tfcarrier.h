//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_TFCARRIER_H
#define C_TFCARRIER_H
#ifdef _WIN32
#pragma once
#endif

#include "c_rescollector.h"
#include "hud_minimap.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class C_TFCarrier : public C_AI_BaseNPC
{
	DECLARE_CLASS( C_TFCarrier, C_AI_BaseNPC );

public:
	DECLARE_CLIENTCLASS();
	DECLARE_ENTITY_PANEL();
	DECLARE_MINIMAP_PANEL( );

	C_TFCarrier();

	virtual void	SetDormant( bool bDormant );
	virtual int		GetHealth() const { return m_iHealth; }
	virtual int		GetMaxHealth() const { return m_iMaxHealth; }

public:
	int			m_iHealth;
	int			m_iMaxHealth;

private:
	C_TFCarrier( const C_TFCarrier & );
};

#endif // C_TFCARRIER_H
