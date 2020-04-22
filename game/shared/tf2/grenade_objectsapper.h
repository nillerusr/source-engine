//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef GRENADE_OBJECTSAPPER_H
#define GRENADE_OBJECTSAPPER_H
#ifdef _WIN32
#pragma once
#endif

class CBaseObject;

//-----------------------------------------------------------------------------
// Purpose: Object sapper grenade
//-----------------------------------------------------------------------------
class CGrenadeObjectSapper : public CBaseGrenade
{
	DECLARE_CLASS( CGrenadeObjectSapper, CBaseGrenade );
public:
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	virtual void	Spawn( void );
	virtual void	Precache( void );
	virtual int		GetDamageType() const { return DMG_BLAST; }
	virtual void	SapperThink( void );
	void			SetTargetObject( CBaseObject *pObject );

	void			SetArmed( bool armed );
	bool			GetArmed( void ) const;

	void			PlayArmingSound( void );

	// Pickup
	virtual int		ObjectCaps( void ) { return FCAP_IMPULSE_USE; };
	virtual void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	static CGrenadeObjectSapper *CGrenadeObjectSapper::Create( const Vector &vecOrigin, const Vector &vecAngles, CBasePlayer *pOwner, CBaseObject *pObject );

public:
	CNetworkVar( bool, m_bSapping );
	CHandle<CBaseObject>	m_hTargetObject;

	bool					m_bArmed;
};

#endif // GRENADE_OBJECTSAPPER_H
