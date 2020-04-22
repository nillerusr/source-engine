//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Client side CTFTeam class
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_TFTEAM_H
#define C_TFTEAM_H
#ifdef _WIN32
#pragma once
#endif

#include "c_team.h"
#include "shareddefs.h"
#include "techtree.h"
#include "imessagechars.h"

class C_BaseEntity;
class C_BaseObject;
class CBaseTechnology;

//-----------------------------------------------------------------------------
// Purpose: TF's Team manager
//-----------------------------------------------------------------------------
class C_TFTeam : public C_Team
{
	DECLARE_CLASS( C_TFTeam, C_Team );
public:
	DECLARE_CLIENTCLASS();

					C_TFTeam();
	virtual			~C_TFTeam();

	// Data Access
	virtual float	GetTeamResources( void );
	virtual float	GetPotentialTeamResources( void );
	virtual bool	GetHaveZone( void );

	// Objects, note GetObject can return NULL!
	int				GetNumObjects( int iObjectType = -1 );
	C_BaseObject	*GetObject( int iIndex );
	bool			IsObjectValid( int iIndex );

	void			NotifyBaseUnderAttack( const Vector &vecPosition, bool bPlaySound = true, bool bForce = false );

	virtual	void	ReceiveMessage( int classID,  bf_read &msg );

public:
	// Resource UI data
	bool	m_bHaveZone;

	float	m_fResources;				// Current amounts of resources
	float	m_fPotentialResources;		// Potential amounts of each resource when all harvesters have returned

	float	m_LastAttackNotificationTime;

	CUtlVector< EHANDLE > m_aObjects;
};


#endif // C_TFTEAM_H
