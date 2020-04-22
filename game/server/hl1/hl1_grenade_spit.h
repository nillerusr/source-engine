//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:		Projectile shot by bullsquid 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//

#ifndef	GRENADESPIT_H
#define	GRENADESPIT_H

#include "hl1_basegrenade.h"

enum SpitSize_e
{
	SPIT_SMALL,
	SPIT_MEDIUM,
	SPIT_LARGE,
};

#define SPIT_GRAVITY 0.9

class CGrenadeSpit : public CHL1BaseGrenade
{
public:
	DECLARE_CLASS( CGrenadeSpit, CHL1BaseGrenade );

	void		Spawn( void );
	void		Precache( void );
	void		SpitThink( void );
	void 		GrenadeSpitTouch( CBaseEntity *pOther );
	void		Event_Killed( const CTakeDamageInfo &info );
	void		SetSpitSize(int nSize);

	int			m_nSquidSpitSprite;
	float		m_fSpitDeathTime;		// If non-zero won't detonate

	void EXPORT				Detonate(void);
	CGrenadeSpit(void);

	DECLARE_DATADESC();
};

#endif	//GRENADESPIT_H
