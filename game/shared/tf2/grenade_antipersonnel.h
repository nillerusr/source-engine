//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef GRENADE_ANTIPERSONNEL_H
#define GRENADE_ANTIPERSONNEL_H
#ifdef _WIN32
#pragma once
#endif

class CSprite;

#include "grenade_base_empable.h"

//-----------------------------------------------------------------------------
// Purpose: Antipersonnel grenade
//-----------------------------------------------------------------------------
class CGrenadeAntiPersonnel : public CBaseEMPableGrenade
{
	DECLARE_CLASS( CGrenadeAntiPersonnel, CBaseEMPableGrenade );
public:
	CGrenadeAntiPersonnel();

	DECLARE_SERVERCLASS();

	virtual void	Spawn( void );
	virtual void	Precache( void );
	virtual void	UpdateOnRemove( void );
	virtual void	BounceTouch( CBaseEntity *pOther );
//	virtual void	BounceSound( void );
	virtual float	GetShakeRadius( void );

	virtual void	Detonate( void );   

	// Damage type accessors
	virtual int GetDamageType() const { return DMG_BLAST; }

	static CGrenadeAntiPersonnel *CGrenadeAntiPersonnel::Create( const Vector &vecOrigin, const Vector &vecAngles, CBasePlayer *pOwner );

	void	SetExplodeOnContact( bool bExplode ) { m_bExplodeOnContact = bExplode; }

private:
	CSprite		*m_pLiveSprite;
	bool		m_bExplodeOnContact;
};

#endif // GRENADE_ANTIPERSONNEL_H
