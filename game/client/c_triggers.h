//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef C_TRIGGERS_H
#define C_TRIGGERS_H
#ifdef _WIN32
#pragma once
#endif

#include "c_basetoggle.h"
#include "triggers_shared.h"

class C_BaseTrigger : public C_BaseToggle
{
	DECLARE_CLASS( C_BaseTrigger, C_BaseToggle );
	DECLARE_CLIENTCLASS();

public:

	bool	m_bClientSidePredicted;
};

#endif // C_TRIGGERS_H
