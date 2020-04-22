//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_OBJ_EMPGENERATOR_H
#define TF_OBJ_EMPGENERATOR_H
#ifdef _WIN32
#pragma once
#endif

// ------------------------------------------------------------------------ //
// EMP Generator defines
#define EMPGENERATOR_MINS				Vector(-20, -20, 0)
#define EMPGENERATOR_MAXS				Vector( 20,  20, 90)
#define EMPGENERATOR_RADIUS				400			
#define EMPGENERATOR_LIFETIME			15
#define EMPGENERATOR_RATE				3			// Rate at which it looks for enemies to EMP
#define EMPGENERATOR_EMP_TIME			5			
#define EMPGENERATOR_MODEL				"models/objects/obj_antimortar.mdl"

// ------------------------------------------------------------------------ //
// EMP Generator Combat Object
// ------------------------------------------------------------------------ //
class CObjectEMPGenerator : public CBaseObject
{
DECLARE_CLASS( CObjectEMPGenerator, CBaseObject );

public:
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	static CObjectEMPGenerator* Create(const Vector &vOrigin, const QAngle &vAngles);

	CObjectEMPGenerator();

	virtual void	Spawn();
	virtual void	Precache();
	virtual bool	CanTakeEMPDamage( void ) { return true; }

	// EMP
	virtual void	EMPThink( void );

private:
	float		m_flExpiresAt;
};


#endif // TF_OBJ_EMPGENERATOR_H
