//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#ifndef C_NEUROTOXIN_COUNTDOWN_H
#define C_NEUROTOXIN_COUNTDOWN_H

#include "cbase.h"
#include "utlvector.h"


class C_NeurotoxinCountdown : public C_BaseEntity
{
public:
	DECLARE_CLASS( C_NeurotoxinCountdown, CBaseEntity );
	DECLARE_CLIENTCLASS();

	C_NeurotoxinCountdown();
	virtual ~C_NeurotoxinCountdown();

	bool IsEnabled( void ) { return m_bEnabled; }

	int GetMinutes( void );
	int GetSeconds( void );
	int GetMilliseconds( void );

private:

	bool	m_bEnabled;
};


extern CUtlVector< C_NeurotoxinCountdown* > g_NeurotoxinCountdowns;


#endif //C_NEUROTOXIN_COUNTDOWN_H