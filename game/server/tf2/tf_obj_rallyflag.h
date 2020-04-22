//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_OBJ_RALLYFLAG_H
#define TF_OBJ_RALLYFLAG_H
#ifdef _WIN32
#pragma once
#endif

// ------------------------------------------------------------------------ //
// Rally flag that's built by players
// ------------------------------------------------------------------------ //
class CObjectRallyFlag : public CBaseObject
{
DECLARE_CLASS( CObjectRallyFlag, CBaseObject );

public:
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	static CObjectRallyFlag* Create(const Vector &vOrigin, const QAngle &vAngles);

					CObjectRallyFlag();

	virtual void	Spawn();
	virtual void	Precache();
	virtual bool	CanTakeEMPDamage( void ) { return true; }

	// Rally
	virtual void	RallyThink( void );

private:
	float		m_flExpiresAt;
};

#endif // TF_OBJ_RALLYFLAG_H
