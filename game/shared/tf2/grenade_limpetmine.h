//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef GRENADE_LIMPETMINE_H
#define GRENADE_LIMPETMINE_H
#ifdef _WIN32
#pragma once
#endif

class CWeaponLimpetmine;

//=====================================================================================================
// LIMPET MINE
//=====================================================================================================
class CLimpetMine : public CBaseGrenade
{
	DECLARE_CLASS( CLimpetMine, CBaseGrenade );

public:
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CLimpetMine( void );
	virtual ~CLimpetMine( void );

	// Creation and Initialization
	virtual void	Spawn( void );
	virtual void	Precache( void );
	static CLimpetMine* CLimpetMine::Create( const Vector &vecOrigin, const Vector &vecAngles, CBasePlayer *pOwner );
	virtual int		GetDamageType() const { return DMG_BLAST; }
	virtual bool	CanBePoweredUp( void ) { return false; }

	// Detonation
	virtual void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	bool			IsLive( void );
	virtual int	    ObjectCaps( void ) { return BaseClass::ObjectCaps() | FCAP_IMPULSE_USE; }

	// EMP
	virtual bool	CanTakeEMPDamage() { return true; }
	virtual bool	TakeEMPDamage( float duration );	
	bool			IsEMPed( void ) { return m_bEMPed; }

	// Think and Touch
	void			LiveThink( void );
	void			StickyTouch( CBaseEntity *pOther );
	void			LimpetThink( void );

	// Parent
	void			SetLauncher( CWeaponLimpetmine *pLauncher );

public:
	static CLimpetMine* allLimpets;				// A linked list of all limpets
	CLimpetMine*		nextLimpet;				// The next limpet in list of all limpets


	CNetworkVar( bool, m_bLive );					// are we active?
	bool				m_bStuckToTarget;			// If true, the limpet stuck to something when it went active
	bool				m_bEMPed;					// have we been EMPed?
	bool				m_bFizzleInit;				// initialize the fizzle (EMP) process
	float				m_flFizzleDuration;			// fizzle duration

	CHandle<CWeaponLimpetmine> m_hLauncher;				// parent (weapon launched from)

};

#endif // GRENADE_LIMPETMINE_H
