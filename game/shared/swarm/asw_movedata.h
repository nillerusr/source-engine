//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef ASW_MOVEDATA_H
#define ASW_MOVEDATA_H
#ifdef _WIN32
#pragma once
#endif


#include "igamemovement.h"

// This class contains ASW-specific prediction data.
class CASW_MoveData : public CMoveData
{
public:
	int		m_iForcedAction;

	Vector	m_vecSkillDest;
};

#endif // ASW_MOVEDATA_H
