//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: Ambient light controller entity with simple radial falloff
//
// $NoKeywords: $
//===========================================================================//
#include "cbase.h"

#include "c_spatialentity.h"
#include "spatialentitymgr.h"
#include "c_env_ambient_light.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


static ConVar cl_ambient_light_disableentities( "cl_ambient_light_disableentities", "0", FCVAR_NONE, "Disable map ambient light entities." );


static CSpatialEntityMgr s_EnvAmbientLightMgr;


IMPLEMENT_CLIENTCLASS_DT( C_EnvAmbientLight, DT_EnvAmbientLight, CEnvAmbientLight )
	RecvPropVector( RECVINFO_NAME( m_Value, m_vecColor ) ),
END_RECV_TABLE()


void C_EnvAmbientLight::ApplyAccumulation( void )
{
	Vector rgbVal = BlendedValue();

	static ConVarRef mat_ambient_light_r( "mat_ambient_light_r" );
	static ConVarRef mat_ambient_light_g( "mat_ambient_light_g" );
	static ConVarRef mat_ambient_light_b( "mat_ambient_light_b" );

	if ( mat_ambient_light_r.IsValid() )
	{
		mat_ambient_light_r.SetValue( rgbVal.x );
	}

	if ( mat_ambient_light_g.IsValid() )
	{
		mat_ambient_light_g.SetValue( rgbVal.y );
	}

	if ( mat_ambient_light_b.IsValid() )
	{
		mat_ambient_light_b.SetValue( rgbVal.z );
	}
}


void C_EnvAmbientLight::AddToPersonalSpatialEntityMgr( void )
{
	s_EnvAmbientLightMgr.AddSpatialEntity( this );
}

void C_EnvAmbientLight::RemoveFromPersonalSpatialEntityMgr( void )
{
	s_EnvAmbientLightMgr.RemoveSpatialEntity( this );
}



void C_EnvAmbientLight::SetColor( const Vector &vecColor, float flLerpTime )
{
	if ( flLerpTime <= 0 )
	{
		m_Value = vecColor / 255.0f;
		m_colorTimer.Invalidate();
		return;
	}

	m_vecStartColor = m_Value;
	m_vecTargetColor = vecColor / 255.0f;
	m_colorTimer.Start( flLerpTime );
}

void C_EnvAmbientLight::ClientThink( void )
{
	BaseClass::ClientThink();

	if ( m_colorTimer.HasStarted() )
	{
		if ( m_colorTimer.IsElapsed() )
		{
			m_Value = m_vecTargetColor;
			m_colorTimer.Invalidate();
		}
		else
		{
			m_Value = m_vecTargetColor - ( m_vecTargetColor - m_vecStartColor ) * m_colorTimer.GetRemainingRatio();
		}
	}
}