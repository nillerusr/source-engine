//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include	"cbase.h"
#include	"ai_default.h"
#include	"ai_task.h"
#include	"ai_schedule.h"
#include	"ai_node.h"
#include	"ai_hull.h"
#include	"ai_hint.h"
#include	"ai_memory.h"
#include	"ai_route.h"
#include	"ai_motor.h"
#include	"soundent.h"
#include	"game.h"
#include	"npcevent.h"
#include	"entitylist.h"
#include	"activitylist.h"
#include	"hl1_basegrenade.h"
#include	"animation.h"
#include	"IEffects.h"
#include	"vstdlib/random.h"
#include	"engine/IEngineSound.h"
#include	"ammodef.h"
#include	"soundenvelope.h"
#include	"hl1_CBaseHelicopter.h"
#include	"ndebugoverlay.h"
#include	"smoke_trail.h"
#include	"beam_shared.h"
#include	"grenade_homer.h"

#define	 HOMER_TRAIL0_LIFE		0.1
#define	 HOMER_TRAIL1_LIFE		0.2
#define	 HOMER_TRAIL2_LIFE		3.0//	1.0

#define  SF_NOTRANSITION 128

extern short g_sModelIndexFireball;

class CNPC_Apache : public CBaseHelicopter
{
	DECLARE_CLASS( CNPC_Apache, CBaseHelicopter );
public:
	DECLARE_DATADESC();

	void Spawn( void );
	void Precache( void );

	int  BloodColor( void ) { return DONT_BLEED; }
	Class_T  Classify( void ) { return CLASS_HUMAN_MILITARY; };
	void InitializeRotorSound( void );
	void LaunchRocket( Vector &viewDir, int damage, int radius, Vector vecLaunchPoint );

	void Flight( void );

	bool FireGun( void );
	void AimRocketGun( void );
	void FireRocket( void );
	void DyingThink( void );

	int  ObjectCaps( void );
	
	void TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr, CDmgAccumulator *pAccumulator );

/*	bool OnInternalDrawModel( ClientModelRenderInfo_t *pInfo )
	{
		BaseClass::OnInteralDrawModel( pInfo );
		Vector origin = GetAbsOrigin();
		origin.z += 32;
		SetAbsOrigin( origin );
	}*/

/*(	void SetAbsOrigin( const Vector& absOrigin )
	{
		((Vector&)absOrigin).z += 32;
		BaseClass::SetAbsOrigin( absOrigin );
	}*/

	

   /* int		Save( CSave &save );
	int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	void Spawn( void );
	void Precache( void );

	
	void Killed( entvars_t *pevAttacker, int iGib );
	void GibMonster( void );

	void EXPORT HuntThink( void );
	void EXPORT FlyTouch( CBaseEntity *pOther );
	void EXPORT CrashTouch( CBaseEntity *pOther );
	void EXPORT DyingThink( void );
	void EXPORT StartupUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void EXPORT NullThink( void );

	void ShowDamage( void );
	void Flight( void );
	void FireRocket( void );
	BOOL FireGun( void );
	
	int  TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType );
	void TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType);*/

	int m_iRockets;
	float m_flForce;
	float m_flNextRocket;

	int m_iAmmoType;

	Vector m_vecTarget;
	Vector m_posTarget;

	Vector m_vecDesired;
	Vector m_posDesired;

	Vector m_vecGoal;

	QAngle m_angGun;
	
	int m_iSoundState; // don't save this

	int m_iExplode;
	int m_iBodyGibs;
	int	m_nDebrisModel;

	float m_flGoalSpeed;

	CHandle<SmokeTrail>	m_hSmoke;
	CBeam *m_pBeam;
};

BEGIN_DATADESC( CNPC_Apache )
	DEFINE_FIELD( m_iRockets, FIELD_INTEGER ),
	DEFINE_FIELD( m_flForce, FIELD_FLOAT ),
	DEFINE_FIELD( m_flNextRocket, FIELD_TIME ),
	DEFINE_FIELD( m_vecTarget, FIELD_VECTOR ),
	DEFINE_FIELD( m_posTarget, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( m_vecDesired, FIELD_VECTOR ),
	DEFINE_FIELD( m_posDesired, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( m_vecGoal, FIELD_VECTOR ),
	DEFINE_FIELD( m_angGun, FIELD_VECTOR ),
	DEFINE_FIELD( m_flLastSeen, FIELD_TIME ),
	DEFINE_FIELD( m_flPrevSeen, FIELD_TIME ),
//	DEFINE_FIELD( m_iSoundState, FIELD_INTEGER ),
//	DEFINE_FIELD( m_iSpriteTexture, FIELD_INTEGER ),
//	DEFINE_FIELD( m_iExplode, FIELD_INTEGER ),
//	DEFINE_FIELD( m_iBodyGibs, FIELD_INTEGER ),
	DEFINE_FIELD( m_pBeam, FIELD_CLASSPTR ),
	DEFINE_FIELD( m_flGoalSpeed, FIELD_FLOAT ),
	DEFINE_FIELD( m_hSmoke, FIELD_EHANDLE ),
END_DATADESC()

ConVar	sk_apache_health( "sk_apache_health","100");

static Vector s_vecSurroundingMins( -300, -300, -172);
static Vector s_vecSurroundingMaxs(300, 300, 8);


void CNPC_Apache::Spawn( void )
{
	Precache( );
	SetModel( "models/apache.mdl" );

	BaseClass::Spawn();

	AddFlag( FL_NPC );
	SetSolid( SOLID_BBOX );
	SetMoveType( MOVETYPE_STEP );
	AddFlag( FL_FLY );


	m_iHealth			=  sk_apache_health.GetFloat();

	m_flFieldOfView = -0.707; // 270 degrees

	m_fHelicopterFlags	= BITS_HELICOPTER_MISSILE_ON | BITS_HELICOPTER_GUN_ON;

	InitBoneControllers();

	SetRenderColor( 255, 255, 255, 255 );

	m_iRockets = 10;

	UTIL_SetSize( this, Vector( -32, -32, -32 ), Vector( 32, 32, 32 ) );

	//CollisionProp()->SetSurroundingBoundsType( USE_SPECIFIED_BOUNDS, &s_vecSurroundingMins, &s_vecSurroundingMaxs );
	//AddSolidFlags( FSOLID_CUSTOMRAYTEST | FSOLID_CUSTOMBOXTEST );

	m_hSmoke = NULL;
}

LINK_ENTITY_TO_CLASS ( monster_apache, CNPC_Apache );

int CNPC_Apache::ObjectCaps( void ) 
{ 
	if ( GetSpawnFlags() & SF_NOTRANSITION )
	     return BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; 
	else 
		 return BaseClass::ObjectCaps();
}

void CNPC_Apache::Precache( void )
{
	// Get to tha chopper!
	PrecacheModel( "models/apache.mdl" );
	PrecacheScriptSound( "Apache.Rotor" );
	m_nDebrisModel = PrecacheModel(   "models/metalplategibs_green.mdl" );

	// Gun
	PrecacheScriptSound( "Apache.FireGun" );
	m_iAmmoType = GetAmmoDef()->Index("9mmRound"); 

	// Rockets
	UTIL_PrecacheOther( "grenade_homer" );
	PrecacheScriptSound( "Apache.RPG" );
	PrecacheModel( "models/weapons/w_missile.mdl" );

	BaseClass::Precache();
}

void CNPC_Apache::InitializeRotorSound( void )
{
	CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();

	CPASAttenuationFilter filter( this );
	m_pRotorSound	= controller.SoundCreate( filter, entindex(), "Apache.Rotor" );

	BaseClass::InitializeRotorSound();
}

void CNPC_Apache::Flight( void )
{
	StudioFrameAdvance( );

	float flDistToDesiredPosition = (GetAbsOrigin() - m_vecDesiredPosition).Length();
	// NDebugOverlay::Line(GetAbsOrigin(), m_vecDesiredPosition, 0,0,255, true, 0.1);

	if (m_flGoalSpeed < 800 )
		m_flGoalSpeed += GetAcceleration();


//	Vector vecGoalOrientation;
	if (flDistToDesiredPosition > 250) // 500
	{
		Vector v1 = (m_vecTargetPosition - GetAbsOrigin());
		Vector v2 = (m_vecDesiredPosition - GetAbsOrigin());

		VectorNormalize( v1 );
		VectorNormalize( v2 );

		if (m_flLastSeen + 90 > gpGlobals->curtime && DotProduct( v1, v2 ) > 0.25)
		{
			m_vecGoalOrientation = ( m_vecTargetPosition - GetAbsOrigin());
		}
		else
		{
			m_vecGoalOrientation = (m_vecDesiredPosition - GetAbsOrigin());
		}
		VectorNormalize( m_vecGoalOrientation );
	}
	else
	{
		AngleVectors( GetGoalEnt()->GetAbsAngles(), &m_vecGoalOrientation );
	}
//	SetGoalOrientation( vecGoalOrientation );
	

	if (GetGoalEnt())
	{
		// ALERT( at_console, "%.0f\n", flLength );
		if ( HasReachedTarget() )
		{
			// If we get this close to the desired position, it's assumed that we've reached
			// the desired position, so move on.

			// Fire target that I've reached my goal
			m_AtTarget.FireOutput( GetGoalEnt(), this );

			OnReachedTarget( GetGoalEnt() );

			SetGoalEnt( gEntList.FindEntityByName( NULL, GetGoalEnt()->m_target ) );

			if (GetGoalEnt())
			{
				m_vecDesiredPosition = GetGoalEnt()->GetAbsOrigin();
				
//				Vector vecGoalOrientation;
				AngleVectors( GetGoalEnt()->GetAbsAngles(), &m_vecGoalOrientation );

//				SetGoalOrientation( vecGoalOrientation );

				flDistToDesiredPosition = (GetAbsOrigin() - m_vecDesiredPosition).Length();
			}
		}
	}
	else
	{
		// If we can't find a new target, just stay where we are.
		m_vecDesiredPosition = GetAbsOrigin();
	}

	// tilt model 5 degrees
	QAngle angAdj = QAngle( 5.0, 0, 0 );

	// estimate where I'll be facing in one seconds
	Vector forward, right, up;
	AngleVectors( GetAbsAngles() + GetLocalAngularVelocity() * 2 + angAdj, &forward, &right, &up );
	// Vector vecEst1 = GetAbsOrigin() + pev->velocity + gpGlobals->v_up * m_flForce - Vector( 0, 0, 384 );
	// float flSide = DotProduct( m_posDesired - vecEst1, gpGlobals->v_right );
	

	QAngle angVel = GetLocalAngularVelocity();
	float flSide = DotProduct( m_vecGoalOrientation, right );

	if (flSide < 0)
	{
		if ( angVel.y < 60)
		{
			 angVel.y += 8; // 9 * (3.0/2.0);
		}
	}
	else
	{
		if ( angVel.y > -60)
		{
			 angVel.y -= 8; // 9 * (3.0/2.0);
		}
	}
	angVel.y *= 0.98;
	SetLocalAngularVelocity( angVel );

	Vector vecVel = GetAbsVelocity();

	// estimate where I'll be in two seconds
	AngleVectors( GetAbsAngles() + GetLocalAngularVelocity() * 1 + angAdj, &forward, &right, &up );
	Vector vecEst = GetAbsOrigin() + vecVel * 2.0 + up * m_flForce * 20 - Vector( 0, 0, 384 * 2 );

	// add immediate force
	AngleVectors( GetAbsAngles() + angAdj, &forward, &right, &up );

	vecVel.x += up.x * m_flForce;
	vecVel.y += up.y * m_flForce;
	vecVel.z += up.z * m_flForce;
	// add gravity
	vecVel.z -= 38.4; // 32ft/sec

	float flSpeed = vecVel.Length();
	float flDir = DotProduct( Vector( forward.x, forward.y, 0 ), Vector( vecVel.x, vecVel.y, 0 ) );
	if (flDir < 0)
		flSpeed = -flSpeed;

	float flDist = DotProduct( m_vecDesiredPosition - vecEst, forward );

	// float flSlip = DotProduct( pev->velocity, gpGlobals->v_right );
	float flSlip = -DotProduct( m_vecDesiredPosition - vecEst, right );

	angVel = GetLocalAngularVelocity();
	// fly sideways
	if (flSlip > 0)
	{
		if (GetAbsAngles().z > -30 && angVel.z > -15)
			angVel.z -= 4;
		else
			angVel.z += 2;
	}
	else
	{

		if (GetAbsAngles().z < 30 && angVel.z < 15)
			angVel.z += 4;
		else
			angVel.z -= 2;
	}
	SetLocalAngularVelocity( angVel );

	// sideways drag
	vecVel.x = vecVel.x * (1.0 - fabs( right.x ) * 0.05);
    vecVel.y = vecVel.y * (1.0 - fabs( right.y ) * 0.05);
	vecVel.z = vecVel.z * (1.0 - fabs( right.z ) * 0.05);

	// general drag
	vecVel = vecVel * 0.995;

	// Set final computed velocity
	SetAbsVelocity( vecVel );
	
	// apply power to stay correct height
	if (m_flForce < 80 && vecEst.z < m_vecDesiredPosition.z) 
	{
		m_flForce += 12;
	}
	else if (m_flForce > 30)
	{
		if (vecEst.z > m_vecDesiredPosition.z) 
			m_flForce -= 8;
	}


	angVel = GetLocalAngularVelocity();
	// pitch forward or back to get to target
	if (flDist > 0 && flSpeed < m_flGoalSpeed && GetAbsAngles().x + angVel.x < 40)
	{
		// ALERT( at_console, "F " );
		// lean forward
		angVel.x += 12.0;
	}
	else if (flDist < 0 && flSpeed > -50 && GetAbsAngles().x + angVel.x  > -20)
	{
		// ALERT( at_console, "B " );
		// lean backward
		angVel.x -= 12.0;
	}
	else if (GetAbsAngles().x + angVel.x < 0)
	{
		// ALERT( at_console, "f " );
		angVel.x += 4.0;
	}
	else if (GetAbsAngles().x + angVel.x > 0)
	{
		// ALERT( at_console, "b " );
		angVel.x -= 4.0;
	}

	// Set final computed angular velocity
	SetLocalAngularVelocity( angVel );

	// ALERT( at_console, "%.0f %.0f : %.0f %.0f : %.0f %.0f : %.0f\n", GetAbsOrigin().x, pev->velocity.x, flDist, flSpeed, GetAbsAngles().x, pev->avelocity.x, m_flForce ); 
	// ALERT( at_console, "%.0f %.0f : %.0f %0.f : %.0f\n", GetAbsOrigin().z, pev->velocity.z, vecEst.z, m_posDesired.z, m_flForce ); 
}

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------

#define CHOPPER_AP_GUN_TIP			0
#define CHOPPER_AP_GUN_BASE			1

#define CHOPPER_BC_GUN_YAW			0
#define CHOPPER_BC_GUN_PITCH		1
#define CHOPPER_BC_POD_PITCH		2

bool CNPC_Apache::FireGun( )
{
	if ( !GetEnemy() )
		return false;

	Vector vForward, vRight, vUp;

	AngleVectors( GetAbsAngles(), &vForward, &vUp, &vRight );
		
	Vector posGun;
	QAngle angGun;
	GetAttachment( 1, posGun, angGun );

	Vector vecTarget = (m_vecTargetPosition - posGun);

	VectorNormalize( vecTarget );

	Vector vecOut;

	vecOut.x = DotProduct( vForward, vecTarget );
	vecOut.y = -DotProduct( vUp, vecTarget );
	vecOut.z = DotProduct( vRight, vecTarget );

	QAngle angles;

	VectorAngles( vecOut, angles );
	
	angles.y = AngleNormalize(angles.y);
	angles.x = AngleNormalize(angles.x);	

	if (angles.x > m_angGun.x)
		m_angGun.x = MIN( angles.x, m_angGun.x + 12 );
	if (angles.x < m_angGun.x)
		m_angGun.x = MAX( angles.x, m_angGun.x - 12 );
	if (angles.y > m_angGun.y)
		m_angGun.y = MIN( angles.y, m_angGun.y + 12 );
	if (angles.y < m_angGun.y)
		m_angGun.y = MAX( angles.y, m_angGun.y - 12 );

	// hacks - shouldn't be hardcoded, oh well.
	// limit it so it doesn't pop if you try to set it to the max value
	m_angGun.y = clamp( m_angGun.y, -89.9, 89.9 );
	m_angGun.x = clamp( m_angGun.x, -9.9, 44.9 );

	m_angGun.y = SetBoneController( 0, m_angGun.y );
	m_angGun.x = SetBoneController( 1, m_angGun.x );

	Vector posBarrel;
	QAngle angBarrel;
	GetAttachment( 0, posBarrel, angBarrel );

	Vector forward;
	AngleVectors( angBarrel + m_angGun, &forward );

	Vector2D vec2LOS = ( GetEnemy()->GetAbsOrigin() - GetAbsOrigin() ).AsVector2D();
	vec2LOS.NormalizeInPlace();

	float flDot = vec2LOS.Dot( forward.AsVector2D() );

	//forward
//	NDebugOverlay::Line(  GetAbsOrigin(), GetAbsOrigin() + ( forward * 200 ), 255,0,0, false, 0.1);
	//LOS
//	NDebugOverlay::Line( posGun, m_vecTargetPosition , 0,0,255, false, 0.1);
//	NDebugOverlay::Box( GetAbsOrigin(), s_vecSurroundingMins, s_vecSurroundingMaxs, 0, 255,0, false, 0.1);

	if ( flDot > 0.98 )
	{
		CPASAttenuationFilter filter( this, 0.2f );

		EmitSound( filter, entindex(), "Apache.FireGun" );//<<TEMP>>temp sound

		// gun is a bit dodgy, just fire at the target if we are close
		FireBullets( 1, posGun, vecTarget, VECTOR_CONE_4DEGREES, 8192, m_iAmmoType, 2 );

		return true;
	}

	return false;
}

void CNPC_Apache::FireRocket( void )
{
	static float side = 1.0;
	static int count;
	Vector vForward, vRight, vUp;


	AngleVectors( GetAbsAngles(), &vForward, &vRight, &vUp );
	Vector vecSrc = GetAbsOrigin() + 1.5 * ( vForward * 21 + vRight * 70 * side + vUp * -79 );

	// pick firing pod to launch from
	switch( m_iRockets % 5)
	{
	case 0:	vecSrc = vecSrc + vRight * 10; break;
	case 1: vecSrc = vecSrc - vRight * 10; break;
	case 2: vecSrc = vecSrc + vUp * 10; break;
	case 3: vecSrc = vecSrc - vUp * 10; break;
	case 4: break;
	}

	Vector vTargetDir = m_vecTargetPosition - GetAbsOrigin();
	VectorNormalize ( vTargetDir );
	LaunchRocket( vTargetDir, 100, 150, vecSrc);

	m_iRockets--;

	side = - side;
}
void CNPC_Apache::AimRocketGun( void )
{
	Vector vForward, vRight, vUp;
	
	if (m_iRockets <= 0)
		return;
	
	Vector vTargetDir = m_vecTargetPosition - GetAbsOrigin();
	VectorNormalize ( vTargetDir );

	AngleVectors( GetAbsAngles(), &vForward, &vRight, &vUp );
	Vector vecEst = ( vForward * 800 + GetAbsVelocity());
	VectorNormalize ( vecEst );

	trace_t tr1;
	UTIL_TraceLine( GetAbsOrigin(), GetAbsOrigin() + vecEst * 4096, MASK_ALL, this, COLLISION_GROUP_NONE, &tr1);

//	NDebugOverlay::Line(GetAbsOrigin(), tr1.endpos, 255,255,0, false, 0.1);

	UTIL_TraceLine( GetAbsOrigin(), GetAbsOrigin() + vTargetDir * 4096, MASK_ALL, this, COLLISION_GROUP_NONE, &tr1);

//	NDebugOverlay::Line(GetAbsOrigin(), tr1.endpos, 0,255,0, false, 0.1);

	// ALERT( at_console, "%d %d %d %4.2f\n", GetAbsAngles().x < 0, DotProduct( pev->velocity, gpGlobals->v_forward ) > -100, m_flNextRocket < gpGlobals->curtime, DotProduct( m_vecTarget, vecEst ) );

	if ((m_iRockets % 2) == 1)
	{
		FireRocket( );
		m_flNextRocket = gpGlobals->curtime + 0.5;
		if (m_iRockets <= 0)
		{
			m_flNextRocket = gpGlobals->curtime + 10;
			m_iRockets = 10;
		}
	}
	else if (DotProduct( GetAbsVelocity(), vForward ) > -100 && m_flNextRocket < gpGlobals->curtime)
	{
		if (m_flLastSeen + 60 > gpGlobals->curtime)
		{
			if (GetEnemy() != NULL)
			{
				// make sure it's a good shot
				//if (DotProduct( vTargetDir, vecEst) > 0.5)
				{
					trace_t tr;
					
					UTIL_TraceLine( GetAbsOrigin(), GetAbsOrigin() + vecEst * 4096, MASK_ALL, this, COLLISION_GROUP_NONE, &tr);

			//		NDebugOverlay::Line(GetAbsOrigin(), tr.endpos, 255,0,255, false, 5);

//					if ((tr.endpos - m_vecTargetPosition).Length() < 512)
					if ((tr.endpos - m_vecTargetPosition).Length() < 1024)
						FireRocket( );
				}
			}
			else
			{
				trace_t tr;
				
				UTIL_TraceLine( GetAbsOrigin(), GetAbsOrigin() + vecEst * 4096, MASK_ALL, this, COLLISION_GROUP_NONE, &tr);
				// just fire when close
				if ((tr.endpos - m_vecTargetPosition).Length() < 512)
					FireRocket( );
			}
		}
	}
}

#define MISSILE_HOMING_STRENGTH		0.3
#define MISSILE_HOMING_DELAY		5.0
#define MISSILE_HOMING_RAMP_UP		0.5
#define MISSILE_HOMING_DURATION		1.0
#define MISSILE_HOMING_RAMP_DOWN	0.5

void CNPC_Apache::LaunchRocket( Vector &viewDir, int damage, int radius, Vector vecLaunchPoint )
{

	CGrenadeHomer *pGrenade = CGrenadeHomer::CreateGrenadeHomer( 
		MAKE_STRING("models/weapons/w_missile.mdl"), 
		MAKE_STRING( "Apache.RPG" ), 
		vecLaunchPoint, vec3_angle, edict() );
	pGrenade->Spawn( );
	pGrenade->SetHoming(MISSILE_HOMING_STRENGTH, MISSILE_HOMING_DELAY,
						MISSILE_HOMING_RAMP_UP,	MISSILE_HOMING_DURATION,
						MISSILE_HOMING_RAMP_DOWN);
	pGrenade->SetDamage( damage );
	pGrenade->m_DmgRadius = radius;

	pGrenade->Launch( this, GetEnemy(), viewDir * 1500, 500, 0, HOMER_SMOKE_TRAIL_ON );

}


//-----------------------------------------------------------------------------
// Purpose:	Lame, temporary death
//-----------------------------------------------------------------------------
void CNPC_Apache::DyingThink( void )
{
	StudioFrameAdvance( );
	SetNextThink( gpGlobals->curtime + 0.1f );

	if( gpGlobals->curtime > m_flNextCrashExplosion )
	{
		CPASFilter pasFilter( GetAbsOrigin() );
		Vector pos;

		pos = GetAbsOrigin();
		pos.x += random->RandomFloat( -150, 150 );
		pos.y += random->RandomFloat( -150, 150 );
		pos.z += random->RandomFloat( -150, -50 );

		te->Explosion( pasFilter, 0.0,	&pos, g_sModelIndexFireball, 10, 15, TE_EXPLFLAG_NONE, 100, 0 );
		
		Vector vecSize = Vector( 500, 500, 60 );
		CPVSFilter pvsFilter( GetAbsOrigin() );

		te->BreakModel( pvsFilter, 0.0, GetAbsOrigin(), vec3_angle, vecSize, vec3_origin, 
			m_nDebrisModel, 100, 0, 2.5, BREAK_METAL );

		m_flNextCrashExplosion = gpGlobals->curtime + random->RandomFloat( 0.3, 0.5 );
	}

	QAngle angVel = GetLocalAngularVelocity();
	if( angVel.y < 400 )
	{
		angVel.y *= 1.1;
		SetLocalAngularVelocity( angVel );
	}

	Vector vecImpulse( 0, 0, -38.4 );	// gravity - 32ft/sec
	ApplyAbsVelocityImpulse( vecImpulse );

	if( m_hSmoke )
	{
		m_hSmoke->SetLifetime(0.1f);
		m_hSmoke = NULL;
	}

}



void CNPC_Apache::TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr, CDmgAccumulator *pAccumulator )
{

	CTakeDamageInfo dmgInfo = info;

	// HITGROUPS don't work currently.
	// ignore blades
	//if (ptr->hitgroup == 6 && (info.GetDamageType() & (DMG_ENERGYBEAM|DMG_BULLET|DMG_CLUB)))
	//		return;

	// hit hard, hits cockpit
	if (info.GetDamage() > 50 || ptr->hitgroup == 1 || ptr->hitgroup == 2 || ptr->hitgroup == 3 )
	{
		// ALERT( at_console, "%map .0f\n", flDamage );
		AddMultiDamage( dmgInfo, this );

		if ( info.GetDamage() > 50 )
		{
			if ( m_hSmoke == NULL && (m_hSmoke = SmokeTrail::CreateSmokeTrail()) != NULL )
			{
				m_hSmoke->m_Opacity = 1.0f;
				m_hSmoke->m_SpawnRate = 60;
				m_hSmoke->m_ParticleLifetime = 1.3f;
				m_hSmoke->m_StartColor.Init( 0.65f, 0.65f , 0.65f );
				m_hSmoke->m_EndColor.Init( 0.65f, 0.65f, 0.65f );
				m_hSmoke->m_StartSize = 12;
				m_hSmoke->m_EndSize = 64;
				m_hSmoke->m_SpawnRadius = 8;
				m_hSmoke->m_MinSpeed = 2;
				m_hSmoke->m_MaxSpeed = 24;				

				m_hSmoke->SetLifetime( 1e6 );
				m_hSmoke->FollowEntity( this );

			}
		}
	}
	else
	{
		// do half damage in the body
		dmgInfo.ScaleDamage(0.5);
		AddMultiDamage( dmgInfo, this );
		g_pEffects->Ricochet( ptr->endpos, ptr->plane.normal );
	}

	if ( m_iHealth < 10 )
	{
		if ( m_hSmoke == NULL && (m_hSmoke = SmokeTrail::CreateSmokeTrail()) != NULL )
		{
			m_hSmoke->m_Opacity = 1.0f;
			m_hSmoke->m_SpawnRate = 60;
			m_hSmoke->m_ParticleLifetime = 1.3f;
			m_hSmoke->m_StartColor.Init( 0.65f, 0.65f , 0.65f );
			m_hSmoke->m_EndColor.Init( 0.65f, 0.65f, 0.65f );
			m_hSmoke->m_StartSize = 12;
			m_hSmoke->m_EndSize = 64;
			m_hSmoke->m_SpawnRadius = 8;
			m_hSmoke->m_MinSpeed = 2;
			m_hSmoke->m_MaxSpeed = 24;				

			m_hSmoke->SetLifetime( 1e6 );
			m_hSmoke->FollowEntity( this );

		}
	}
}
