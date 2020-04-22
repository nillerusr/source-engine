//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_DEMO_ENTITIES_H
#define C_DEMO_ENTITIES_H

#include "c_ai_basenpc.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class C_Cycler_TF2Commando : public C_AI_BaseNPC
{
	DECLARE_CLASS( C_Cycler_TF2Commando, C_AI_BaseNPC );
public:
	DECLARE_CLIENTCLASS();

	C_Cycler_TF2Commando();

	float	GetShieldRaiseTime() const;
	float	GetShieldLowerTime() const;
	bool	IsShieldActive() const;

private:
	C_Cycler_TF2Commando( const C_Cycler_TF2Commando& );

	bool	m_bShieldActive;
	float	m_flShieldRaiseTime;
	float	m_flShieldLowerTime;
};

#endif // C_DEMO_ENTITIES_H
