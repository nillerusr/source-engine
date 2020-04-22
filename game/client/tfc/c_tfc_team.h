//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Client side CTFTeam class
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_TFC_TEAM_H
#define C_TFC_TEAM_H
#ifdef _WIN32
#pragma once
#endif

#include "c_team.h"
#include "shareddefs.h"

class C_BaseEntity;
class C_BaseObject;
class CBaseTechnology;

//-----------------------------------------------------------------------------
// Purpose: TF's Team manager
//-----------------------------------------------------------------------------
class C_TFCTeam : public C_Team
{
	DECLARE_CLASS( C_TFCTeam, C_Team );
	DECLARE_CLIENTCLASS();

public:

					C_TFCTeam();
	virtual			~C_TFCTeam();
};


#endif // C_TFC_TEAM_H
