//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"

#define MAX_ROCK_MODELS		6

// Rock models
char *sRockModels[ MAX_ROCK_MODELS ] =
{
	"models/props/cliffside/inhibitor_rocks/inhibitor_rock12.mdl",
	"models/props/cliffside/inhibitor_rocks/inhibitor_rock13.mdl",
	"models/props/cliffside/inhibitor_rocks/inhibitor_rock14.mdl",
	"models/props/cliffside/inhibitor_rocks/inhibitor_rock19.mdl",
	"models/props/cliffside/inhibitor_rocks/inhibitor_rock20.mdl",
	"models/props/cliffside/inhibitor_rocks/inhibitor_rock21.mdl",
};

//-----------------------------------------------------------------------------
// Purpose: A falling rock entity
//-----------------------------------------------------------------------------
class CFallingRock : public CBaseAnimating
{
	DECLARE_CLASS( CFallingRock, CBaseAnimating );
public:
	DECLARE_DATADESC();

	CFallingRock( void );
	virtual void Spawn( void );
	virtual void VPhysicsUpdate( IPhysicsObject *pPhysics );
	static CFallingRock *Create( const Vector &vecOrigin, const QAngle &vecAngles, const Vector &vecVelocity, const AngularImpulse &vecRotationSpeed );

	void	RockTouch( CBaseEntity *pOther );
public:
};

BEGIN_DATADESC( CFallingRock )

	// functions
	DEFINE_FUNCTION( RockTouch ),

END_DATADESC()
LINK_ENTITY_TO_CLASS( fallingrock, CFallingRock );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CFallingRock::CFallingRock( void ) 
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFallingRock::Spawn( void )
{
	SetModel( sRockModels[ random->RandomInt(0,MAX_ROCK_MODELS-1) ] );
	SetMoveType( MOVETYPE_NONE );
	m_takedamage = DAMAGE_NO;

	// Create the object in the physics system
	VPhysicsInitNormal( SOLID_BBOX, 0, false );
	UTIL_SetSize( this, Vector(-4,-4,-4), Vector(4,4,4) );

	SetTouch( RockTouch );
	SetThink( SUB_Remove );
	SetNextThink( gpGlobals->curtime + random->RandomFloat( 20.0, 30.0 ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFallingRock::VPhysicsUpdate( IPhysicsObject *pPhysics )
{
	BaseClass::VPhysicsUpdate( pPhysics );
}

//-----------------------------------------------------------------------------
// Purpose: Create a falling rock
//-----------------------------------------------------------------------------
CFallingRock *CFallingRock::Create( const Vector &vecOrigin, const QAngle &vecAngles, const Vector &vecVelocity, const AngularImpulse &vecRotationSpeed )
{
	CFallingRock *pRock = (CFallingRock*)CreateEntityByName("fallingrock");

	UTIL_SetOrigin( pRock, vecOrigin );
	pRock->SetLocalAngles( vecAngles );
	pRock->Spawn();

	IPhysicsObject *pPhysicsObject = pRock->VPhysicsGetObject();
	if ( pPhysicsObject )
	{
		pPhysicsObject->AddVelocity( &vecVelocity, &vecRotationSpeed );
	}

	return pRock;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFallingRock::RockTouch( CBaseEntity *pOther )
{
}






//-----------------------------------------------------------------------------
// Purpose: A falling rock spawner entity
//-----------------------------------------------------------------------------
class CEnv_FallingRocks : public CPointEntity
{
	DECLARE_CLASS( CEnv_FallingRocks, CPointEntity );
public:
	DECLARE_DATADESC();

	virtual void Spawn( void );
	virtual void Precache( void );
	void		 RockThink( void );
	void		 InputSpawnRock( inputdata_t &inputdata );

public:
	float			m_flFallStrength;
	float			m_flRotationSpeed;
	float			m_flMinSpawnTime;
	float			m_flMaxSpawnTime;
	COutputEvent	m_pOutputRockSpawned;
};

BEGIN_DATADESC( CEnv_FallingRocks )

	// Fields
	DEFINE_KEYFIELD( m_flFallStrength, FIELD_FLOAT, "FallSpeed"),
	DEFINE_KEYFIELD( m_flRotationSpeed, FIELD_FLOAT, "RotationSpeed"),
	DEFINE_KEYFIELD( m_flMinSpawnTime, FIELD_FLOAT, "MinSpawnTime"),
	DEFINE_KEYFIELD( m_flMaxSpawnTime, FIELD_FLOAT, "MaxSpawnTime"),

	// Inputs
	DEFINE_INPUTFUNC( FIELD_VOID,	"SpawnRock",	InputSpawnRock ),

	// Outputs
	DEFINE_OUTPUT( m_pOutputRockSpawned,	"OnRockSpawned" ),

	// Functions
	DEFINE_FUNCTION( RockThink ),

END_DATADESC()
LINK_ENTITY_TO_CLASS( env_fallingrocks, CEnv_FallingRocks );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnv_FallingRocks::Spawn( void )
{
	Precache();
	SetThink( RockThink );
	SetNextThink( gpGlobals->curtime + random->RandomFloat( m_flMinSpawnTime, m_flMaxSpawnTime ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnv_FallingRocks::Precache( void )
{
	for (int i = 0; i < MAX_ROCK_MODELS; i++ )
	{
		PrecacheModel( sRockModels[i] );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnv_FallingRocks::RockThink( void )
{
	// Spawn a rock
	// Make it aim a little around the angle supplied
	QAngle angFire = GetAbsAngles();
	angFire.y += random->RandomFloat( -10, 10 );
	Vector vecForward;
	AngleVectors( angFire, &vecForward );
	CFallingRock::Create( GetAbsOrigin(), GetAbsAngles(), (vecForward * m_flFallStrength), AngularImpulse(0,0,m_flRotationSpeed) );

	// Fire our output
	m_pOutputRockSpawned.FireOutput( NULL,this );

	SetNextThink( gpGlobals->curtime + random->RandomFloat( m_flMinSpawnTime, m_flMaxSpawnTime ) );
}

//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
void CEnv_FallingRocks::InputSpawnRock( inputdata_t &inputdata )
{
	RockThink();
}