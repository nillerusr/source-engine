//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef ENTITYFREEZING_H
#define ENTITYFREEZING_H

#ifdef _WIN32
#pragma once
#endif


class CEntityFreezing : public CBaseEntity 
{
public:
	DECLARE_SERVERCLASS();
	DECLARE_CLASS( CEntityFreezing, CBaseEntity );

	static CEntityFreezing	*Create( CBaseAnimating *pTarget );
	
	void	Precache();
	void	Spawn();
	void	AttachToEntity( CBaseEntity *pTarget );
	void	SetFreezingOrigin( Vector vOrigin ) { m_vFreezingOrigin = vOrigin; }
	Vector	GetFreezingOrigin( void ) { return m_vFreezingOrigin; }
	void	SetFrozen( float flFrozen ){ m_flFrozen = flFrozen; }
	int		GetFrozen( void ) { return m_flFrozen; }
	void	FinishFreezing( void ) { m_bFinishFreezing = true; }

	DECLARE_DATADESC();

protected:
	void	InputFreeze( inputdata_t &inputdata );

	CNetworkVector( m_vFreezingOrigin );
	CNetworkVar( float, m_flFrozen );
	CNetworkVar( bool, m_bFinishFreezing );

public:
	CNetworkArray( float, m_flFrozenPerHitbox, 50 );
};

#endif // ENTITYFREEZING_H
