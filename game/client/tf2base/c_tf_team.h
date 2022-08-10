//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: Client side CTFTeam class
//
// $NoKeywords: $
//=============================================================================

#ifndef C_TF_TEAM_H
#define C_TF_TEAM_H
#ifdef _WIN32
#pragma once
#endif

#include "c_team.h"
#include "shareddefs.h"
#include "c_baseobject.h"

class C_BaseEntity;
class C_BaseObject;
class CBaseTechnology;

//-----------------------------------------------------------------------------
// Purpose: TF's Team manager
//-----------------------------------------------------------------------------
class C_TFTeam : public C_Team
{
	DECLARE_CLASS( C_TFTeam, C_Team );
	DECLARE_CLIENTCLASS();

public:

					C_TFTeam();
	virtual			~C_TFTeam();

	int				GetFlagCaptures( void ) { return m_nFlagCaptures; }
	int				GetRole( void ) { return m_iRole; }
	char			*Get_Name( void );

	int				GetNumObjects( int iObjectType = -1 );
	CBaseObject		*GetObject( int num );

	CUtlVector< CHandle<C_BaseObject> > m_aObjects;

private:

	int		m_nFlagCaptures;
	int		m_iRole;
};

C_TFTeam *GetGlobalTFTeam( int iTeamNumber );

#endif // C_TF_TEAM_H
