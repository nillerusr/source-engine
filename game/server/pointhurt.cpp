//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Implements hurting point entity
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "pointhurt.h"
#include "entitylist.h"
#include "gamerules.h"
#include "basecombatcharacter.h"
#include "ammodef.h"

#ifdef EZ
#include "particle_parse.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifdef EZ
ConVar ez2_goo_puddle_time_linger("ez2_goo_puddle_time_linger", "30");
ConVar ez2_goo_puddle_time_fade("ez2_goo_puddle_time_fade", "10");
ConVar ez2_goo_puddle_time_particle( "ez2_goo_puddle_time_particle", "1" );
#endif

const int SF_PHURT_START_ON			= 1;

BEGIN_DATADESC( CPointHurt )

	DEFINE_KEYFIELD( m_flRadius, FIELD_FLOAT, "DamageRadius" ),
	DEFINE_KEYFIELD( m_nDamage, FIELD_INTEGER, "Damage" ),
	DEFINE_KEYFIELD( m_flDelay, FIELD_FLOAT, "DamageDelay" ),
	DEFINE_KEYFIELD( m_bitsDamageType, FIELD_INTEGER, "DamageType" ),
	DEFINE_KEYFIELD( m_strTarget, FIELD_STRING, "DamageTarget" ),
	DEFINE_KEYFIELD( m_bDisabled, FIELD_BOOLEAN, "StartDisabled" ),
	DEFINE_KEYFIELD( m_flLifetime, FIELD_FLOAT, "Lifetime"),

	// Function Pointers
	DEFINE_FUNCTION( HurtThink ),

	// Inputs
	DEFINE_INPUTFUNC( FIELD_VOID, "TurnOn", InputTurnOn ),
	DEFINE_INPUTFUNC( FIELD_VOID, "TurnOff", InputTurnOff ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Toggle", InputToggle ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Hurt", InputHurt ),

	DEFINE_FIELD( m_pActivator, FIELD_EHANDLE ),
	DEFINE_FIELD( m_flExpirationTime, FIELD_TIME )
END_DATADESC()

LINK_ENTITY_TO_CLASS( point_hurt, CPointHurt );
#ifdef EZ // Blixibon - Unique classname for zombie puddle point_hurt
LINK_ENTITY_TO_CLASS( zombie_goo_puddle, CPointHurtGoo );
#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPointHurt::Spawn(void)
{
	if (HasSpawnFlags(SF_PHURT_START_ON))
		m_bDisabled = false;

	SetThink( NULL );
	SetUse( NULL );
		
	m_pActivator = NULL;
	
	if ( m_flRadius <= 0.0f )
	{
		m_flRadius = 128.0f;
	}

	if ( m_nDamage <= 0 )
	{
		m_nDamage = 2;
	}

	if ( m_flDelay <= 0 )
	{
		m_flDelay = 0.1f;
	}

	Precache();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPointHurt::Precache( void )
{
	BaseClass::Precache();
}

void CPointHurt::Activate(void)
{
	if (!m_bDisabled) {
		TurnOn(m_pActivator);
	}

	BaseClass::Activate();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPointHurt::HurtThink( void )
{
	if ( m_strTarget != NULL_STRING )
	{
		CBaseEntity	*pEnt = NULL;
			
		CTakeDamageInfo info( this, m_pActivator, m_nDamage, m_bitsDamageType );
		while ( ( pEnt = gEntList.FindEntityByName( pEnt, m_strTarget, NULL, m_pActivator ) ) != NULL )
		{
			GuessDamageForce( &info, (pEnt->GetAbsOrigin() - GetAbsOrigin()), pEnt->GetAbsOrigin() );
			pEnt->TakeDamage( info );
		}
	}
	else
	{
		RadiusDamage( CTakeDamageInfo( this, this, m_nDamage, m_bitsDamageType ), GetAbsOrigin(), m_flRadius, CLASS_NONE, NULL );
	}

	if (m_flExpirationTime > 0 && m_flExpirationTime < gpGlobals->curtime)
	{
		m_bDisabled = true;
		SetThink(NULL);
	}
	else 
	{
		SetNextThink(gpGlobals->curtime + m_flDelay);
	}

}

//-----------------------------------------------------------------------------
// Purpose: Trigger hurt that causes radiation will do a radius check and set
//			the player's geiger counter level according to distance from center
//			of trigger.
//-----------------------------------------------------------------------------
void CPointHurt::RadiationThink(void)
{
	// check to see if a player is in pvs
	// if not, continue	
	Vector vecSurroundMins, vecSurroundMaxs;
	CollisionProp()->WorldSpaceSurroundingBounds(&vecSurroundMins, &vecSurroundMaxs);
	CBasePlayer *pPlayer = static_cast<CBasePlayer *>(UTIL_FindClientInPVS(vecSurroundMins, vecSurroundMaxs));

	if (pPlayer)
	{
		// get range to player;
		float flRange = CollisionProp()->CalcDistanceFromPoint(pPlayer->WorldSpaceCenter());
		flRange *= 3.0f;
		pPlayer->NotifyNearbyRadiationSource(flRange);
	}

	HurtThink();
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for turning on the point hurt.
//-----------------------------------------------------------------------------
void CPointHurt::InputTurnOn( inputdata_t &data )
{
	TurnOn(data.pActivator);
}

void CPointHurt::TurnOn(CBaseEntity * activator)
{
	m_bDisabled = false;

	if (m_flLifetime > 0.0f)
	{
		m_flExpirationTime = gpGlobals->curtime + m_flLifetime;
	}

	if (m_bitsDamageType & DMG_RADIATION) 
	{
		SetThink(&CPointHurt::RadiationThink);
	}
	else 
	{
		SetThink(&CPointHurt::HurtThink);
	}
	
	SetNextThink(gpGlobals->curtime + m_flDelay);
	m_pActivator = activator;
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for turning off the point hurt.
//-----------------------------------------------------------------------------
void CPointHurt::InputTurnOff( inputdata_t &data )
{
	m_bDisabled = true;
	SetThink( NULL );

	m_pActivator = data.pActivator;
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for toggling the on/off state of the point hurt.
//-----------------------------------------------------------------------------
void CPointHurt::InputToggle( inputdata_t &data )
{
	m_pActivator = data.pActivator;

	if ( m_pfnThink == (void (CBaseEntity::*)())&CPointHurt::HurtThink )
	{
		m_bDisabled = true;
		SetThink( NULL );
	}
	else
	{
		TurnOn(data.pActivator);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for instantaneously hurting whatever is near us.
//-----------------------------------------------------------------------------
void CPointHurt::InputHurt( inputdata_t &data )
{
	m_pActivator = data.pActivator;

	HurtThink();
}

#ifdef MAPBASE
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CPointHurt::KeyValue( const char *szKeyName, const char *szValue )
{
	// Additional OR flags
	if (FStrEq( szKeyName, "damageor" ) || FStrEq( szKeyName, "damagepresets" ))
	{
		m_bitsDamageType |= atoi(szValue);
	}
	else
		return BaseClass::KeyValue( szKeyName, szValue );

	return true;
}
#endif

#ifdef EZ
void CPointHurtGoo::Precache( void )
{
	PrecacheParticleSystem( "glow_puddle" );
	PrecacheScriptSound( "NPC_BaseGlowbie.glow_puddle" );

	BaseClass::Precache();
}

void CPointHurtGoo::TurnOn( CBaseEntity * activator )
{
	BaseClass::TurnOn( activator );

	SetContextThink( &CPointHurtGoo::GooParticleThink, gpGlobals->curtime, "GooParticleThink" );
}

void CPointHurtGoo::GooParticleThink( void )
{
	int flNextParticleTime = gpGlobals->curtime + ez2_goo_puddle_time_particle.GetFloat();

	if ( m_bDisabled )
	{
		SetContextThink( NULL, gpGlobals->curtime, "GooParticleThink" );
		UTIL_Remove( this ); // Remove this goo puddle after the particle has expired
		return;
	}
	else
	{
		SetContextThink( &CPointHurtGoo::GooParticleThink, flNextParticleTime, "GooParticleThink" );
	}

	EmitSound( "NPC_BaseGlowbie.glow_puddle" );
	DispatchParticleEffect( "glow_puddle", WorldSpaceCenter(), GetAbsAngles() );
}
#endif
