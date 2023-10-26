//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: Ambient light controller entity.
//
// $NoKeywords: $
//===========================================================================//

#include "cbase.h"
#include "spatialentity.h"
#include "env_ambient_light.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


#define AMBIENT_LIGHT_ENT_THINK_RATE TICK_INTERVAL

LINK_ENTITY_TO_CLASS(env_ambient_light, CEnvAmbientLight);

BEGIN_DATADESC( CEnvAmbientLight )
	DEFINE_KEYFIELD(	m_Color,	FIELD_COLOR32,	"Color" ),
	DEFINE_FIELD(		m_vecColor, FIELD_VECTOR ),
	DEFINE_INPUTFUNC( FIELD_COLOR32, "SetColor",  InputSetColor ),
END_DATADESC()

IMPLEMENT_SERVERCLASS_ST(CEnvAmbientLight, DT_EnvAmbientLight)
	SendPropVector( SENDINFO(m_vecColor), 0, SPROP_NOSCALE),
END_SEND_TABLE()


void CEnvAmbientLight::Spawn( void )
{
	BaseClass::Spawn();

	m_vecColor.SetX( static_cast<float>( m_Color.r ) / 255.0f );
	m_vecColor.SetY( static_cast<float>( m_Color.g ) / 255.0f );
	m_vecColor.SetZ( static_cast<float>( m_Color.b ) / 255.0f );
}


void CEnvAmbientLight::InputSetColor(inputdata_t &inputdata)
{
	m_Color = inputdata.value.Color32();
	m_vecColor.SetX( static_cast<float>( m_Color.r ) / 255.0f );
	m_vecColor.SetY( static_cast<float>( m_Color.g ) / 255.0f );
	m_vecColor.SetZ( static_cast<float>( m_Color.b ) / 255.0f );
}


void CEnvAmbientLight::SetColor( const Vector &vecColor )
{
	m_vecColor = vecColor;
}