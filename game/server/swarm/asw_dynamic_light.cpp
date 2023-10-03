#include "cbase.h"
#include "dlight.h"
#include "asw_dynamic_light.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define NUM_DL_EXPONENT_BITS	8
#define MIN_DL_EXPONENT_VALUE	-((1 << (NUM_DL_EXPONENT_BITS-1)) - 1)
#define MAX_DL_EXPONENT_VALUE	((1 << (NUM_DL_EXPONENT_BITS-1)) - 1)

LINK_ENTITY_TO_CLASS(asw_dynamic_light, CASW_Dynamic_Light);

BEGIN_DATADESC( CASW_Dynamic_Light )

	DEFINE_FIELD( m_ActualFlags, FIELD_CHARACTER ),
	DEFINE_FIELD( m_Flags, FIELD_CHARACTER ),
	DEFINE_FIELD( m_On, FIELD_BOOLEAN ),

	DEFINE_KEYFIELD( m_LightColor, FIELD_COLOR32, "LightColor" ),

	DEFINE_THINKFUNC( DynamicLightThink ),

	// Inputs
	DEFINE_INPUT( m_Radius,		FIELD_FLOAT,	"distance" ),
	DEFINE_INPUT( m_Exponent,	FIELD_INTEGER,	"brightness" ),
	DEFINE_INPUT( m_InnerAngle,	FIELD_FLOAT,	"_inner_cone" ),
	DEFINE_INPUT( m_OuterAngle,	FIELD_FLOAT,	"_cone" ),
	DEFINE_INPUT( m_SpotRadius,	FIELD_FLOAT,	"spotlight_radius" ),
	DEFINE_INPUT( m_LightStyle,	FIELD_CHARACTER,"style" ),
	
	// Input functions
	DEFINE_INPUTFUNC( FIELD_VOID, "TurnOn", InputTurnOn ),
	DEFINE_INPUTFUNC( FIELD_VOID, "TurnOff", InputTurnOff ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Toggle", InputToggle ),

END_DATADESC()


IMPLEMENT_SERVERCLASS_ST(CASW_Dynamic_Light, DT_ASW_Dynamic_Light)
	SendPropInt( SENDINFO(m_Flags), 4, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO(m_LightStyle), 4, SPROP_UNSIGNED ),
	SendPropFloat( SENDINFO(m_Radius), 0, SPROP_NOSCALE),
	SendPropInt( SENDINFO(m_Exponent), NUM_DL_EXPONENT_BITS),
	SendPropFloat( SENDINFO(m_InnerAngle), 8, 0, 0.0, 360.0f ),
	SendPropFloat( SENDINFO(m_OuterAngle), 8, 0, 0.0, 360.0f ),
	SendPropFloat( SENDINFO(m_SpotRadius), 0, SPROP_NOSCALE),
END_SEND_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CASW_Dynamic_Light::KeyValue( const char *szKeyName, const char *szValue )
{
	if ( FStrEq( szKeyName, "pitch" ) )
	{
		float angle = atof(szValue);
		if ( angle )
		{
			QAngle angles = GetAbsAngles();
			angles[PITCH] = -angle;
			SetAbsAngles( angles );
		}
	}
	else if ( FStrEq( szKeyName, "spawnflags" ) )
	{
		m_ActualFlags = m_Flags = atoi(szValue);
	}
	else
	{
		return BaseClass::KeyValue( szKeyName, szValue );
	}

	return true;
}

//------------------------------------------------------------------------------
// Turn on and off the light
//------------------------------------------------------------------------------
void CASW_Dynamic_Light::InputTurnOn( inputdata_t &inputdata )
{
	m_Flags = m_ActualFlags;
	m_On = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CASW_Dynamic_Light::InputTurnOff( inputdata_t &inputdata )
{
	// This basically shuts it off
	m_Flags = DLIGHT_NO_MODEL_ILLUMINATION | DLIGHT_NO_WORLD_ILLUMINATION;
	m_On = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CASW_Dynamic_Light::InputToggle( inputdata_t &inputdata )
{
	if (m_On)
	{
		InputTurnOff( inputdata );
	}
	else
	{
		InputTurnOn( inputdata );
	}
}

//------------------------------------------------------------------------------
// Purpose :
//------------------------------------------------------------------------------
void CASW_Dynamic_Light::Spawn( void )
{
	Precache();
	SetSolid( SOLID_NONE );
	m_On = true;
	UTIL_SetSize( this, vec3_origin, vec3_origin );
	AddEFlags( EFL_FORCE_CHECK_TRANSMIT );

	// If we have a target, think so we can orient towards it
	if ( m_target != NULL_STRING )
	{
		SetThink( &CASW_Dynamic_Light::DynamicLightThink );
		SetNextThink( gpGlobals->curtime + 0.1 );
	}

	m_InnerAngle = 30;
	m_OuterAngle = 45;
	m_SpotRadius = 80;

	SetRenderColor( m_LightColor.r, m_LightColor.g, m_LightColor.b );
	
	int clampedExponent = clamp( m_Exponent, MIN_DL_EXPONENT_VALUE, MAX_DL_EXPONENT_VALUE );
	if ( m_Exponent != clampedExponent )
	{
		Warning( "light_dynamic at [%d %d %d] has invalid exponent value (%d must be between %d and %d).\n",
			(int)GetAbsOrigin().x, (int)GetAbsOrigin().x, (int)GetAbsOrigin().x, 
			m_Exponent.Get(),
			MIN_DL_EXPONENT_VALUE,
			MAX_DL_EXPONENT_VALUE );
		
		m_Exponent = clampedExponent;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_Dynamic_Light::DynamicLightThink( void )
{
	if ( m_target == NULL_STRING )
		return;

	CBaseEntity *pEntity = GetNextTarget();
	if ( pEntity )
	{
		Vector vecToTarget = (pEntity->GetAbsOrigin() - GetAbsOrigin());
		QAngle vecAngles;
		VectorAngles( vecToTarget, vecAngles );
		SetAbsAngles( vecAngles );
	}
	Msg("serer radius is %f\n", m_Radius);
	
	SetNextThink( gpGlobals->curtime + 0.1 );
}
