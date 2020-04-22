//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_OBJ_MAPDEFINED_H
#define TF_OBJ_MAPDEFINED_H
#ifdef _WIN32
#pragma once
#endif

// ------------------------------------------------------------------------ //
// Purpose: Map Defined object placed by mapmakers
// ------------------------------------------------------------------------ //
class CObjectMapDefined : public CBaseObject
{
DECLARE_CLASS( CObjectMapDefined, CBaseObject );

public:
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	static CObjectMapDefined* Create(const Vector &vOrigin, const QAngle &vAngles);

	CObjectMapDefined();

	virtual void	Spawn();
	virtual void	Precache();
	virtual bool	CanTakeEMPDamage( void ) { return true; }
	virtual int		OnTakeDamage( const CTakeDamageInfo &info );

private:
	// Custom names for ID strings
	string_t	m_iszCustomName;
	CNetworkString( m_szCustomName, MAX_OBJ_CUSTOMNAME_SIZE );
};

#endif // TF_OBJ_MAPDEFINED_H
