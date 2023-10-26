#ifndef _INCLUDED_ASW_ENTITY_DISSOLVE_H
#define _INCLUDED_ASW_ENTITY_DISSOLVE_H

#ifdef _WIN32
#pragma once
#endif

class CASW_Entity_Dissolve : public CBaseEntity 
{
public:
	DECLARE_SERVERCLASS();
	DECLARE_CLASS( CASW_Entity_Dissolve, CBaseEntity );

	CASW_Entity_Dissolve( void );
	virtual ~CASW_Entity_Dissolve( void );

	static CASW_Entity_Dissolve	*Create( CBaseEntity *pTarget, const char *pMaterialName, 
		float flStartTime, int nDissolveType = 0, bool *pRagdollCreated = NULL );
	static CASW_Entity_Dissolve	*Create( CBaseEntity *pTarget, CBaseEntity *pSource );

	void	Precache();
	void	Spawn();
	void	AttachToEntity( CBaseEntity *pTarget );
	void	SetStartTime( float flStartTime );

	DECLARE_DATADESC();

protected:
	void	InputDissolve( inputdata_t &inputdata );
	void	DissolveThink( void );
	void	ElectrocuteThink( void );

	CNetworkVar( float, m_flStartTime );
	CNetworkVar( float, m_flFadeInStart );
	CNetworkVar( float, m_flFadeInLength );
	CNetworkVar( float, m_flFadeOutModelStart );
	CNetworkVar( float, m_flFadeOutModelLength );
	CNetworkVar( float, m_flFadeOutStart );
	CNetworkVar( float, m_flFadeOutLength );
	CNetworkVar( int, m_nDissolveType );
};

#endif // _INCLUDED_ASW_ENTITY_DISSOLVE_H
