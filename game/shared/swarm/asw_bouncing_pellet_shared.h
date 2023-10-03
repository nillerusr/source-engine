#ifndef _INCLUDED_ASW_BOUNCING_PELLET_SHARED_H
#define _INCLUDED_ASW_BOUNCING_PELLET_SHARED_H
#ifdef _WIN32
#pragma once
#endif

#ifdef CLIENT_DLL
#include "c_basecombatcharacter.h"
#define CASW_Bouncing_Pellet C_ASW_Bouncing_Pellet
#define CBaseCombatCharacter C_BaseCombatCharacter
class Beam_t;
#else
#include "basecombatcharacter.h"
#endif

class CSprite;

// test of a projectile that bounces off walls.  Not currently used in AS:I

class CASW_Bouncing_Pellet : public CBaseCombatCharacter
{
	DECLARE_CLASS( CASW_Bouncing_Pellet, CBaseCombatCharacter );
public:
	CASW_Bouncing_Pellet();
	DECLARE_NETWORKCLASS();

	virtual void	Spawn( void );
	virtual void	Precache( void );
	virtual void	UpdateOnRemove( void );
	virtual void	PelletTouch( CBaseEntity *pOther );

	static CASW_Bouncing_Pellet *CASW_Bouncing_Pellet::CreatePellet( const Vector &vecOrigin, const Vector &vecForward, CBaseEntity *pMarine, float flDamage );

#ifdef CLIENT_DLL
	virtual void	OnDataChanged( DataUpdateType_t updateType );
	virtual void ClientThink();

	void CreateBeamTrail();
	void ReleaseBeamTrail();
	Beam_t* m_pTrail;
	bool m_bClientPellet;

#else
	void PelletHurt( CBaseEntity *pOther, trace_t &tr);
#endif

	float m_flDamage;
	EHANDLE m_hLastHit;
	int m_iBounces;

private:
	CASW_Bouncing_Pellet( const CASW_Bouncing_Pellet & );
};

#endif // _INCLUDED_ASW_BOUNCING_PELLET_SHARED_H
