//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Game-specific explosion effects
//
//=============================================================================//
#include "cbase.h"
#include "c_te_effect_dispatch.h"
#include "tempent.h"
#include "c_te_legacytempents.h"
#include "dod_shareddefs.h"
#include "engine/IEngineSound.h"
#include "c_basetempentity.h"
#include "tier0/vprof.h"
#include "fx_explosion.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void DODExplosionCallback( const Vector &vecOrigin, const Vector &vecNormal )
{
	// Calculate the angles, given the normal.
	bool bInAir = false;
	QAngle angExplosion( 0.0f, 0.0f, 0.0f );

	// Cannot use zeros here because we are sending the normal at a smaller bit size.
	if ( fabs( vecNormal.x ) < 0.05f && fabs( vecNormal.y ) < 0.05f && fabs( vecNormal.z ) < 0.05f )
	{
		bInAir = true;
		angExplosion.Init();
	}
	else
	{
		VectorAngles( vecNormal, angExplosion );
		bInAir = false;
	}

	// Base explosion effect and sound.
	char *pszEffect = "explosion";
	char *pszSound = "BaseExplosionEffect.Sound";

	// Explosions.

	if ( UTIL_PointContents( vecOrigin ) & CONTENTS_WATER )
	{
		WaterExplosionEffect().Create( vecOrigin, 1 /*m_nMagnitude*/, 1 /*m_fScale*/, 0 /*m_nFlags*/ );
		return;
	}
	else if ( bInAir )
	{
		pszEffect = "explosioncore_midair";
	}
	else
	{
		pszEffect = "explosioncore_floor";
	}

	CLocalPlayerFilter filter;
	C_BaseEntity::EmitSound( filter, SOUND_FROM_WORLD, pszSound, &vecOrigin );

	DispatchParticleEffect( pszEffect, vecOrigin, angExplosion );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class C_TEDODExplosion : public C_BaseTempEntity
{
public:

	DECLARE_CLASS( C_TEDODExplosion, C_BaseTempEntity );
	DECLARE_CLIENTCLASS();

	C_TEDODExplosion( void );

	virtual void	PostDataUpdate( DataUpdateType_t updateType );

public:
	Vector		m_vecOrigin;
	Vector		m_vecNormal;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_TEDODExplosion::C_TEDODExplosion( void )
{
	m_vecOrigin.Init();
	m_vecNormal.Init();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_TEDODExplosion::PostDataUpdate( DataUpdateType_t updateType )
{
	VPROF( "C_TETFExplosion::PostDataUpdate" );
	DODExplosionCallback( m_vecOrigin, m_vecNormal );
}

IMPLEMENT_CLIENTCLASS_EVENT_DT( C_TEDODExplosion, DT_TEDODExplosion, CTEDODExplosion )
	RecvPropFloat( RECVINFO( m_vecOrigin[0] ) ),
	RecvPropFloat( RECVINFO( m_vecOrigin[1] ) ),
	RecvPropFloat( RECVINFO( m_vecOrigin[2] ) ),
	RecvPropVector( RECVINFO( m_vecNormal ) ),
END_RECV_TABLE()

