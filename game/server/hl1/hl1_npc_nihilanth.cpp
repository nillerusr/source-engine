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
#include	"animation.h"
#include	"basecombatweapon.h"
#include	"IEffects.h"
#include	"vstdlib/random.h"
#include	"engine/IEngineSound.h"
#include	"ammodef.h"
#include	"Sprite.h"
#include	"hl1_ai_basenpc.h"
#include	"ai_senses.h"
#include	"Sprite.h"
#include	"beam_shared.h"
#include	"logicrelay.h"
#include	"ai_navigator.h"


#define N_SCALE		15
#define N_SPHERES	20

ConVar sk_nihilanth_health( "sk_nihilanth_health", "800" );
ConVar sk_nihilanth_zap( "sk_nihilanth_zap", "30" );

class CNPC_Nihilanth : public CHL1BaseNPC
{
	DECLARE_CLASS( CNPC_Nihilanth, CHL1BaseNPC );
public:
	void Spawn( void );
	void Precache( void );

	Class_T Classify( void ) { return CLASS_ALIEN_MILITARY; };

	/*	void Killed( entvars_t *pevAttacker, int iGib );
	void GibMonster( void );

	void TargetSphere( USE_TYPE useType, float value );
	CBaseEntity *RandomTargetname( const char *szName );
	void MakeFriend( Vector vecPos );
	
	int  TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType );
	void TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType);
	*/

	int	OnTakeDamage_Alive( const CTakeDamageInfo &info );
	void TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr, CDmgAccumulator *pAccumulator );
	bool ShouldGib( const CTakeDamageInfo &info ) { return false; }

	void PainSound( const CTakeDamageInfo &info );
	void DeathSound( const CTakeDamageInfo &info );
	
	void StartupThink( void );
	void NullThink( void );

	void HuntThink( void );
	void DyingThink( void );

	void Flight( void );
	void NextActivity( void );
	void FloatSequence( void );
	void HandleAnimEvent( animevent_t *pEvent );
	bool EmitSphere( void );

	void ShootBalls( void );
	bool AbsorbSphere( void );

	void MakeFriend( Vector vecStart );

	void InputTurnBabyOn( inputdata_t &inputdata );
	void InputTurnBabyOff( inputdata_t &inputdata );

	float m_flForce;

	float m_flNextPainSound;

	Vector m_velocity;
	Vector m_avelocity;

	Vector m_vecTarget;
	Vector m_posTarget;

	Vector m_vecDesired;
	Vector m_posDesired;

	float  m_flMinZ;
	float  m_flMaxZ;

	Vector m_vecGoal;

	float m_flLastSeen;
	float m_flPrevSeen;

	int m_irritation;

	int m_iLevel;
	int m_iTeleport;

	EHANDLE m_hRecharger;

	EHANDLE m_hSphere[N_SPHERES];
	int	m_iActiveSpheres;

	float m_flAdj;

	CSprite *m_pBall;

	char m_szRechargerTarget[64];
	char m_szDrawUse[64];
	char m_szTeleportUse[64];
	char m_szTeleportTouch[64];
	char m_szDeadUse[64];
	char m_szDeadTouch[64];

	float m_flShootEnd;
	float m_flShootTime;

	EHANDLE m_hFriend[3];

	bool m_bDead;

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS( monster_nihilanth, CNPC_Nihilanth );

BEGIN_DATADESC( CNPC_Nihilanth )
	DEFINE_FIELD( m_flForce, FIELD_FLOAT ),
	DEFINE_FIELD( m_flNextPainSound, FIELD_TIME ),
	DEFINE_FIELD( m_velocity, FIELD_VECTOR ),
	DEFINE_FIELD( m_avelocity, FIELD_VECTOR ),
	DEFINE_FIELD( m_vecTarget, FIELD_VECTOR ),
	DEFINE_FIELD( m_posTarget, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( m_vecDesired, FIELD_VECTOR ),
	DEFINE_FIELD( m_posDesired, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( m_flMinZ, FIELD_FLOAT ),
	DEFINE_FIELD( m_flMaxZ, FIELD_FLOAT ),
	DEFINE_FIELD( m_vecGoal, FIELD_VECTOR ),
	DEFINE_FIELD( m_flLastSeen, FIELD_TIME ),
	DEFINE_FIELD( m_flPrevSeen, FIELD_TIME ),
	DEFINE_FIELD( m_irritation, FIELD_INTEGER ),
	DEFINE_FIELD( m_iLevel, FIELD_INTEGER ),
	DEFINE_FIELD( m_iTeleport, FIELD_INTEGER ),
	DEFINE_FIELD( m_hRecharger, FIELD_EHANDLE ),
	DEFINE_ARRAY( m_hSphere, FIELD_EHANDLE, N_SPHERES ),
	DEFINE_FIELD( m_iActiveSpheres, FIELD_INTEGER ),
	DEFINE_FIELD( m_flAdj, FIELD_FLOAT ),
	DEFINE_FIELD( m_pBall, FIELD_CLASSPTR ),
	DEFINE_ARRAY( m_szRechargerTarget, FIELD_CHARACTER, 64 ),
	DEFINE_ARRAY( m_szDrawUse, FIELD_CHARACTER, 64 ),
	DEFINE_ARRAY( m_szTeleportUse, FIELD_CHARACTER, 64 ),
	DEFINE_ARRAY( m_szTeleportTouch, FIELD_CHARACTER, 64 ),
	DEFINE_ARRAY( m_szDeadUse, FIELD_CHARACTER, 64 ),
	DEFINE_ARRAY( m_szDeadTouch, FIELD_CHARACTER, 64 ),
	DEFINE_FIELD( m_flShootEnd, FIELD_TIME ),
	DEFINE_FIELD( m_flShootTime, FIELD_TIME ),
	DEFINE_ARRAY( m_hFriend, FIELD_EHANDLE, 3 ),
	DEFINE_FIELD( m_bDead, FIELD_BOOLEAN ),
	DEFINE_THINKFUNC( NullThink ),
	DEFINE_THINKFUNC( StartupThink ),
	DEFINE_THINKFUNC( HuntThink ),
	DEFINE_THINKFUNC( DyingThink ),

	DEFINE_INPUTFUNC( FIELD_VOID, "TurnBabyOn", InputTurnBabyOn ),
	DEFINE_INPUTFUNC( FIELD_VOID, "TurnBabyOff", InputTurnBabyOff ),

END_DATADESC()

class CNihilanthHVR : public CAI_BaseNPC
{
	DECLARE_CLASS( CNihilanthHVR, CAI_BaseNPC );
public:
	void Spawn( void );
	void Precache( void );

	void CircleInit( CBaseEntity *pTarget );
	void AbsorbInit( void );
	void GreenBallInit( void );
	

	void RemoveTouch( CBaseEntity *pOther );
	
	/*void Zap( void );
	void Teleport( void );*/
	
	void HoverThink( void );
	bool CircleTarget( Vector vecTarget );
	void BounceTouch( CBaseEntity *pOther );

	void ZapThink( void );
	void ZapInit( CBaseEntity *pEnemy );
	void ZapTouch( CBaseEntity *pOther );

	void TeleportThink( void );
	void TeleportTouch( CBaseEntity *pOther );

	void MovetoTarget( Vector vecTarget );

	void DissipateThink( void );

	CSprite *SpriteInit( const char *pSpriteName, CNihilanthHVR *pOwner );

	void TeleportInit( CNPC_Nihilanth *pOwner, CBaseEntity *pEnemy, CBaseEntity *pTarget, CBaseEntity *pTouch );

	float m_flIdealVel;
	Vector m_vecIdeal;
	CNPC_Nihilanth *m_pNihilanth;
	EHANDLE m_hTouch;
	
	
	float m_flBallScale;

	void SetSprite( CBaseEntity *pSprite )
	{
		m_hSprite = pSprite;	
	}

	CBaseEntity *GetSprite( void )
	{
		return m_hSprite.Get();
	}

	void SetBeam( CBaseEntity *pBeam )
	{
		m_hBeam = pBeam;	
	}

	CBaseEntity *GetBeam( void )
	{
		return m_hBeam.Get();
	}

private:
		
	EHANDLE m_hSprite;
	EHANDLE m_hBeam;


	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS( nihilanth_energy_ball, CNihilanthHVR );


BEGIN_DATADESC( CNihilanthHVR )
	DEFINE_FIELD( m_flIdealVel, FIELD_FLOAT ),
	DEFINE_FIELD( m_flBallScale, FIELD_FLOAT ),
	DEFINE_FIELD( m_vecIdeal, FIELD_VECTOR ),
	DEFINE_FIELD( m_pNihilanth, FIELD_CLASSPTR ),
	DEFINE_FIELD( m_hTouch, FIELD_EHANDLE ),
	DEFINE_FIELD( m_hSprite, FIELD_EHANDLE ),
	DEFINE_FIELD( m_hBeam, FIELD_EHANDLE ),

	DEFINE_THINKFUNC( HoverThink ),
	DEFINE_ENTITYFUNC( BounceTouch ),
	DEFINE_THINKFUNC( ZapThink ),
	DEFINE_ENTITYFUNC( ZapTouch ),
	DEFINE_THINKFUNC( DissipateThink ),
	DEFINE_THINKFUNC( TeleportThink ),
	DEFINE_ENTITYFUNC( TeleportTouch ),
	DEFINE_ENTITYFUNC( RemoveTouch ),
END_DATADESC()


//=========================================================
// Nihilanth, final Boss monster
//=========================================================

void CNPC_Nihilanth::Spawn( void )
{
	Precache( );
	// motor
	SetMoveType( MOVETYPE_FLY );
	SetSolid( SOLID_BBOX );

	SetModel( "models/nihilanth.mdl" );
	//UTIL_SetSize( this, Vector( -300, -300, 0), Vector(300, 300, 512));
	//UTIL_SetSize(this, Vector( -32, -32, 0), Vector(32, 32, 64 ));

	UTIL_SetSize(this, Vector( -16 * N_SCALE, -16 * N_SCALE, -48 * N_SCALE ), Vector( 16 * N_SCALE, 16 * N_SCALE, 28 * N_SCALE ) );
	
	Vector vecSurroundingMins( -16 * N_SCALE, -16 * N_SCALE, -48 * N_SCALE );
	Vector vecSurroundingMaxs( 16 * N_SCALE, 16 * N_SCALE, 28 * N_SCALE );
	CollisionProp()->SetSurroundingBoundsType( USE_SPECIFIED_BOUNDS, &vecSurroundingMins, &vecSurroundingMaxs );

	UTIL_SetOrigin( this, GetAbsOrigin() - Vector( 0, 0, 64 ) );

	AddFlag( FL_NPC );
	m_takedamage		= DAMAGE_AIM;
	m_iHealth			= sk_nihilanth_health.GetFloat();
	SetViewOffset ( Vector( 0, 0, 300 ) );

	m_flFieldOfView = -1; // 360 degrees

	SetSequence( 0 );
	ResetSequenceInfo( );

	InitBoneControllers();

	SetThink( &CNPC_Nihilanth::StartupThink );
	SetNextThink( gpGlobals->curtime + 0.1 );

	m_vecDesired = Vector( 1, 0, 0 );
	m_posDesired = Vector( GetAbsOrigin().x, GetAbsOrigin().y, 512 );

	m_iLevel = 1; 
	m_iTeleport = 1;

	if (m_szRechargerTarget[0] == '\0')	Q_strncpy( m_szRechargerTarget, "n_recharger", sizeof( m_szRechargerTarget ) );
	if (m_szDrawUse[0] == '\0')			Q_strncpy( m_szDrawUse, "n_draw", sizeof( m_szDrawUse ) );
	if (m_szTeleportUse[0] == '\0')		Q_strncpy( m_szTeleportUse, "n_leaving", sizeof( m_szTeleportUse ) );
	if (m_szTeleportTouch[0] == '\0')	Q_strncpy( m_szTeleportTouch, "n_teleport", sizeof( m_szTeleportTouch ) );
	if (m_szDeadUse[0] == '\0')			Q_strncpy( m_szDeadUse, "n_dead", sizeof( m_szDeadUse ) );
	if (m_szDeadTouch[0] == '\0')		Q_strncpy( m_szDeadTouch, "n_ending", sizeof( m_szDeadTouch ) );

	SetBloodColor( BLOOD_COLOR_YELLOW );
}


void CNPC_Nihilanth::Precache( void )
{
	PrecacheModel("models/nihilanth.mdl");
	PrecacheModel("sprites/lgtning.vmt");
	UTIL_PrecacheOther( "nihilanth_energy_ball" );
	UTIL_PrecacheOther( "monster_alien_controller" );
	UTIL_PrecacheOther( "monster_alien_slave" );

	PrecacheScriptSound( "Nihilanth.PainLaugh" );
	PrecacheScriptSound( "Nihilanth.Pain" );
	PrecacheScriptSound( "Nihilanth.Die" );
	PrecacheScriptSound( "Nihilanth.FriendBeam" );
	PrecacheScriptSound( "Nihilanth.Attack" );
	PrecacheScriptSound( "Nihilanth.BallAttack" );
	PrecacheScriptSound( "Nihilanth.Recharge" );

}

void CNPC_Nihilanth::PainSound( const CTakeDamageInfo &info )
{
	if (m_flNextPainSound > gpGlobals->curtime)
		return;
	
	m_flNextPainSound = gpGlobals->curtime + random->RandomFloat( 2, 5 );

	if ( m_iHealth > sk_nihilanth_health.GetFloat() / 2 )
	{
		CPASAttenuationFilter filter( this );
		EmitSound( filter, entindex(), "Nihilanth.PainLaugh" ); 
	}
	else if (m_irritation >= 2)
	{
		CPASAttenuationFilter filter( this );
		EmitSound( filter, entindex(), "Nihilanth.Pain" ); 
	}
}	

void CNPC_Nihilanth::DeathSound( const CTakeDamageInfo &info )
{
	CPASAttenuationFilter filter( this );
	EmitSound( filter, entindex(), "Nihilanth.Die" ); 
}

int	CNPC_Nihilanth::OnTakeDamage_Alive( const CTakeDamageInfo &info )
{
	if ( info.GetInflictor() == this )
		 return 0;

	if ( m_bDead )
		return 0;

	if ( info.GetDamage() >= m_iHealth )
	{
		m_iHealth = 1;
		if ( m_irritation != 3 )
			 return 0;
	}
	
	PainSound( info );

	m_iHealth -= info.GetDamage();

	if( m_iHealth < 0 )
	{
		m_iHealth = 1;
		m_bDead = true;
		m_takedamage = DAMAGE_NO;
	}

	return 0;
}

void CNPC_Nihilanth::TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr, CDmgAccumulator *pAccumulator )
{
	if (m_irritation == 3)
		m_irritation = 2;

	if (m_irritation == 2 && ptr->hitgroup == 0 && info.GetDamage() > 2)
		m_irritation = 3;

	if (m_irritation != 3)
	{
		Vector vecBlood = (ptr->endpos - GetAbsOrigin() );
		
		VectorNormalize( vecBlood );

		UTIL_BloodStream( ptr->endpos, vecBlood, BloodColor(), info.GetDamage() + (100 - 100 * (m_iHealth / sk_nihilanth_health.GetFloat() )));
	}

	// SpawnBlood(ptr->vecEndPos, BloodColor(), flDamage * 5.0);// a little surface blood.
	AddMultiDamage( info, this );
}

bool CNPC_Nihilanth::EmitSphere( void )
{
	m_iActiveSpheres = 0;
	int empty = 0;

	for (int i = 0; i < N_SPHERES; i++)
	{
		if (m_hSphere[i] != NULL)
		{
			m_iActiveSpheres++;
		}
		else
		{
			empty = i;
		}
	}

	if (m_iActiveSpheres >= N_SPHERES)
		return false;

	Vector vecSrc = m_hRecharger->GetAbsOrigin();
	CNihilanthHVR *pEntity = (CNihilanthHVR *)CREATE_ENTITY( CNihilanthHVR,  "nihilanth_energy_ball" );
	
	
	pEntity->SetAbsOrigin( vecSrc );
	pEntity->SetAbsAngles( GetAbsAngles() );
	pEntity->SetOwnerEntity( this );
	pEntity->Spawn();

	pEntity->SetAbsVelocity( GetAbsOrigin() - vecSrc );
	pEntity->CircleInit( this );
	m_hSphere[empty] = pEntity;

	return true;
}

void CNPC_Nihilanth::NullThink( void )
{
	StudioFrameAdvance( );
	SetNextThink( gpGlobals->curtime + 0.5 );
}

void CNPC_Nihilanth::StartupThink( void )
{
	m_irritation = 0;
	m_flAdj = 512;

	CBaseEntity *pEntity;

	pEntity = gEntList.FindEntityByName( NULL, "n_min" );
	if (pEntity)
		m_flMinZ = pEntity->GetAbsOrigin().z;
	else
		m_flMinZ = -4096;

	pEntity = gEntList.FindEntityByName( NULL, "n_max" );
	if (pEntity)
		m_flMaxZ = pEntity->GetAbsOrigin().z;
	else
		m_flMaxZ = 4096;

	m_hRecharger = this;

	//TODO
	for (int i = 0; i < N_SPHERES; i++)
	{
		EmitSphere();
	}

	m_hRecharger = NULL;

	SetUse( NULL );
	SetThink( &CNPC_Nihilanth::HuntThink);
	
	SetNextThink( gpGlobals->curtime + 0.1 );
}

void CNPC_Nihilanth::InputTurnBabyOn( inputdata_t &inputdata )
{
	if ( m_irritation == 0 )
	{
		m_irritation = 1;
	}
}

void CNPC_Nihilanth::InputTurnBabyOff( inputdata_t &inputdata )
{
	CBaseEntity *pTouch = gEntList.FindEntityByName( NULL, m_szDeadTouch );
	
	if ( pTouch && GetEnemy() != NULL )
		 pTouch->Touch( GetEnemy() );
}

bool CNPC_Nihilanth::AbsorbSphere( void )
{
	for (int i = 0; i < N_SPHERES; i++)
	{
		if (m_hSphere[i] != NULL)
		{
			CNihilanthHVR *pSphere = (CNihilanthHVR *)m_hSphere[i].Get();
			pSphere->AbsorbInit();
			m_hSphere[i] = NULL;
			m_iActiveSpheres--;
			return TRUE;
		}
	}
	return FALSE;
}

void CNPC_Nihilanth::HuntThink( void )
{
	SetNextThink( gpGlobals->curtime + 0.1 );
	DispatchAnimEvents( this );
	StudioFrameAdvance( );

	ShootBalls();

	// if dead, force cancelation of current animation
	if ( m_bDead )
	{
		SetThink( &CNPC_Nihilanth::DyingThink );
		SetCycle( 1.0f );

		StudioFrameAdvance();
		return;
	}

	// ALERT( at_console, "health %.0f\n", pev->health );

	// if damaged, try to abosorb some spheres
	if ( m_iHealth < sk_nihilanth_health.GetFloat() && AbsorbSphere() )
	{
		m_iHealth += sk_nihilanth_health.GetFloat() / N_SPHERES;
	}

	// get new sequence
	if ( IsSequenceFinished() )
	{
		SetCycle( 0 );

		NextActivity( );
		ResetSequenceInfo( );
		m_flPlaybackRate = 2.0 - 1.0 * ( m_iHealth / sk_nihilanth_health.GetFloat() );
	}

	// look for current enemy	
	if ( GetEnemy() != NULL && m_hRecharger == NULL)
	{
		if (FVisible( GetEnemy() ))
		{
			if (m_flLastSeen < gpGlobals->curtime - 5)
				m_flPrevSeen = gpGlobals->curtime;
			
			m_flLastSeen = gpGlobals->curtime;
			m_posTarget = GetEnemy()->GetAbsOrigin();
			m_vecTarget = m_posTarget - GetAbsOrigin();
			
			VectorNormalize( m_vecTarget );

			m_vecDesired = m_vecTarget;
			m_posDesired = Vector( GetAbsOrigin().x, GetAbsOrigin().y, m_posTarget.z + m_flAdj );
		}
		else
		{
			m_flAdj = MIN( m_flAdj + 10, 1000 );
		}
	}

	// don't go too high
	if (m_posDesired.z > m_flMaxZ)
		m_posDesired.z = m_flMaxZ;

	// don't go too low
	if (m_posDesired.z < m_flMinZ)
		m_posDesired.z = m_flMinZ;

	Flight( );
}

void CNPC_Nihilanth::Flight( void )
{
	Vector vForward, vRight, vUp;

	QAngle vAngle = QAngle( GetAbsAngles().x + m_avelocity.x, GetAbsAngles().y + m_avelocity.y, GetAbsAngles().z + m_avelocity.z );

	AngleVectors( vAngle, &vForward, &vRight, &vUp );
	float flSide = DotProduct( m_vecDesired, vRight );

	if (flSide < 0)
	{
		if (m_avelocity.y < 180)
		{
			m_avelocity.y += 6; // 9 * (3.0/2.0);
		}
	}
	else
	{
		if (m_avelocity.y > -180)
		{
			m_avelocity.y -= 6; // 9 * (3.0/2.0);
		}
	}
	m_avelocity.y *= 0.98;

	// estimate where I'll be in two seconds
	Vector vecEst = GetAbsOrigin() + m_velocity * 2.0 + vUp * m_flForce * 20;

	// add immediate force
	AngleVectors( GetAbsAngles(), &vForward, &vRight, &vUp );

	m_velocity.x += vUp.x * m_flForce;
	m_velocity.y += vUp.y * m_flForce;
	m_velocity.z += vUp.z * m_flForce;


	float flSpeed = m_velocity.Length();
	float flDir = DotProduct( Vector( vForward.x, vForward.y, 0 ), Vector( m_velocity.x, m_velocity.y, 0 ) );
	if (flDir < 0)
		flSpeed = -flSpeed;

	// sideways drag
	m_velocity.x = m_velocity.x * (1.0 - fabs( vRight.x ) * 0.05);
	m_velocity.y = m_velocity.y * (1.0 - fabs( vRight.y ) * 0.05);
	m_velocity.z = m_velocity.z * (1.0 - fabs( vRight.z ) * 0.05);

	// general drag
	m_velocity = m_velocity * 0.995;
	
	// apply power to stay correct height
	if (m_flForce < 100 && vecEst.z < m_posDesired.z) 
	{
		m_flForce += 10;
	}
	else if (m_flForce > -100 && vecEst.z > m_posDesired.z)
	{
		if (vecEst.z > m_posDesired.z) 
			m_flForce -= 10;
	}

	SetAbsVelocity( m_velocity );

	vAngle = QAngle( GetAbsAngles().x + m_avelocity.x * 0.1, GetAbsAngles().y + m_avelocity.y * 0.1, GetAbsAngles().z + m_avelocity.z * 0.1 );

	SetAbsAngles( vAngle );

	// ALERT( at_console, "%5.0f %5.0f : %4.0f : %3.0f : %2.0f\n", m_posDesired.z, pev->origin.z, m_velocity.z, m_avelocity.y, m_flForce ); 
}

void CNPC_Nihilanth::NextActivity( )
{
	Vector vForward, vRight, vUp;

	SetIdealActivity( ACT_DO_NOT_DISTURB );

	AngleVectors( GetAbsAngles(), &vForward, &vRight, &vUp );

	if (m_irritation >= 2)
	{
		if (m_pBall == NULL)
		{
			m_pBall = CSprite::SpriteCreate( "sprites/tele1.vmt", GetAbsOrigin(), true );
			if (m_pBall)
			{
				m_pBall->SetTransparency( kRenderTransAdd, 255, 255, 255, 255, kRenderFxNoDissipation );
				m_pBall->SetAttachment( this, 1 );
				m_pBall->SetScale( 4.0 );
				m_pBall->m_flSpriteFramerate = 10.0f;
				m_pBall->TurnOn( );
			}
		}

		if (m_pBall)
		{
			CBroadcastRecipientFilter filterlight;
			Vector vOrigin;
			QAngle vAngle;

			GetAttachment( 2, vOrigin, vAngle );

			te->DynamicLight( filterlight, 0.0, &vOrigin, 255, 192, 64, 0, 256, 20, 0 );
		}
	}
	

	if (( m_iHealth < sk_nihilanth_health.GetFloat() / 2 || m_iActiveSpheres < N_SPHERES / 2) && m_hRecharger == NULL && m_iLevel <= 9)
	{
		char szName[64];

		CBaseEntity *pEnt = NULL;
		CBaseEntity *pRecharger = NULL;
		float flDist = 8192;

		Q_snprintf(szName, sizeof( szName ), "%s%d", m_szRechargerTarget, m_iLevel );

		while ((pEnt = gEntList.FindEntityByName( pEnt, szName )) != NULL )
		{
			float flLocal = (pEnt->GetAbsOrigin() - GetAbsOrigin() ).Length();

			if ( flLocal < flDist )
			{
				flDist = flLocal;
				pRecharger = pEnt;
			}	
		}
		
		if (pRecharger)
		{
			m_hRecharger = pRecharger;
			m_posDesired = Vector( GetAbsOrigin().x, GetAbsOrigin().y, pRecharger->GetAbsOrigin().z );
			m_vecDesired = pRecharger->GetAbsOrigin() - m_posDesired;

			VectorNormalize( m_vecDesired );

			m_vecDesired.z = 0;
			
			VectorNormalize( m_vecDesired );
		}
		else
		{
			m_hRecharger = NULL;
			Msg( "nihilanth can't find %s\n", szName );

			m_iLevel++;

			if ( m_iLevel > 9 )
				 m_irritation = 2;
		}
	}

	float flDist = ( m_posDesired - GetAbsOrigin() ).Length();
	float flDot = DotProduct( m_vecDesired, vForward );

	if (m_hRecharger != NULL)
	{
		// at we at power up yet?
		if (flDist < 128.0)
		{
			int iseq = LookupSequence( "recharge" );

			if (iseq != GetSequence())
			{
				char szText[64];

				Q_snprintf( szText, sizeof( szText ), "%s%d", m_szDrawUse, m_iLevel );
				FireTargets( szText, this, this, USE_ON, 1.0 );

				Msg( "fireing %s\n", szText );
			}
			SetSequence ( LookupSequence( "recharge" ) );
		}
		else
		{
			FloatSequence( );
		}
		return;
	}

	if (GetEnemy() != NULL && !GetEnemy()->IsAlive())
	{
		SetEnemy( NULL );
	}

	if (m_flLastSeen + 15 < gpGlobals->curtime)
	{
		SetEnemy( NULL );
	}

	if ( GetEnemy() == NULL)
	{
		GetSenses()->Look( 4096 );
		SetEnemy( BestEnemy() );
	}

	if ( GetEnemy() != NULL && m_irritation != 0)
	{
		if (m_flLastSeen + 5 > gpGlobals->curtime && flDist < 256 && flDot > 0)
		{
			if (m_irritation >= 2 && m_iHealth < sk_nihilanth_health.GetFloat() / 2.0)
			{
				SetSequence( LookupSequence( "attack1_open" ) );
			}
			else 
			{
				if ( random->RandomInt(0, 1 ) == 0)
				{
					SetSequence( LookupSequence( "attack1" ) ); // zap
				}
				else
				{
					char szText[64];

					Q_snprintf( szText, sizeof( szText ), "%s%d", m_szTeleportTouch, m_iTeleport );
					CBaseEntity *pTouch = gEntList.FindEntityByName( NULL, szText );

					Q_snprintf( szText, sizeof( szText ), "%s%d", m_szTeleportUse, m_iTeleport );
					CBaseEntity *pTrigger = gEntList.FindEntityByName( NULL, szText );

					if (pTrigger != NULL || pTouch != NULL)
					{
						SetSequence( LookupSequence( "attack2" ) ); // teleport
					}
					else
					{
						m_iTeleport++;
						SetSequence( LookupSequence( "attack1" ) ); // zap
					}
				}
			}
			return;
		}
	}

	FloatSequence( );		
}

void CNPC_Nihilanth::MakeFriend( Vector vecStart )
{
	int i;

	for (i = 0; i < 3; i++)
	{
		if (m_hFriend[i] != NULL && !m_hFriend[i]->IsAlive())
		{
			if ( m_nRenderMode == kRenderNormal) // don't do it if they are already fading
				m_hFriend[i]->MyNPCPointer()->CorpseFade();

			m_hFriend[i] = NULL;
		}

		if (m_hFriend[i] == NULL)
		{
			if ( random->RandomInt( 0, 1 ) == 0)
			{
				int iNode = GetNavigator()->GetNetwork()->NearestNodeToPoint( vecStart );
				CAI_Node *pNode = GetNavigator()->GetNetwork()->GetNode( iNode );

				if ( pNode && pNode->GetType() == NODE_AIR )
				{
					trace_t tr;
					Vector vNodeOrigin = pNode->GetOrigin();

					UTIL_TraceHull( vNodeOrigin + Vector( 0, 0, 32 ), vNodeOrigin + Vector( 0, 0, 32 ), Vector(-40,-40,   0),	Vector(40, 40, 100), MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );

					if ( tr.startsolid == 0 )
						 m_hFriend[i] = Create("monster_alien_controller", vNodeOrigin, GetAbsAngles() );
				}
			}
			else
			{
				int iNode = GetNavigator()->GetNetwork()->NearestNodeToPoint( vecStart );
				CAI_Node *pNode = GetNavigator()->GetNetwork()->GetNode( iNode );

				if ( pNode && ( pNode->GetType() == NODE_GROUND || pNode->GetType() == NODE_WATER ) )
				{
					trace_t tr;
					Vector vNodeOrigin = pNode->GetOrigin();

					UTIL_TraceHull( vNodeOrigin + Vector( 0, 0, 36 ), vNodeOrigin + Vector( 0, 0, 36 ), Vector( -15, -15, 0),	Vector( 20, 15, 72 ), MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );
					
					if (tr.startsolid == 0)
						m_hFriend[i] = Create("monster_alien_slave", vNodeOrigin, GetAbsAngles() );
				}
			}
			if (m_hFriend[i] != NULL)
			{
				CPASAttenuationFilter filter( this );
				EmitSound( filter, m_hFriend[i]->entindex(), "Nihilanth.FriendBeam" ); 
			}

			return;
		}
	}
}

void CNPC_Nihilanth::ShootBalls( void )
{
	if (m_flShootEnd > gpGlobals->curtime)
	{
		Vector vecHand;
		QAngle vecAngle;
		
		while (m_flShootTime < m_flShootEnd && m_flShootTime < gpGlobals->curtime)
		{
			if ( GetEnemy() != NULL)
			{
				Vector vecSrc, vecDir;
				CNihilanthHVR *pEntity = NULL;

				GetAttachment( 3, vecHand, vecAngle );
				vecSrc = vecHand + GetAbsVelocity() * (m_flShootTime - gpGlobals->curtime);
				vecDir = m_posTarget - GetAbsOrigin();
				VectorNormalize( vecDir );
				vecSrc = vecSrc + vecDir * (gpGlobals->curtime - m_flShootTime);
				
				pEntity = (CNihilanthHVR *)CREATE_ENTITY( CNihilanthHVR,  "nihilanth_energy_ball" );

				pEntity->SetAbsOrigin( vecSrc );
				pEntity->SetAbsAngles( vecAngle );
				pEntity->SetOwnerEntity( this );
				pEntity->Spawn();

				pEntity->SetAbsVelocity( vecDir * 200.0 );
				pEntity->ZapInit( GetEnemy() );
				
				GetAttachment( 4, vecHand, vecAngle );
				vecSrc = vecHand + GetAbsVelocity() * (m_flShootTime - gpGlobals->curtime);
				vecDir = m_posTarget - GetAbsOrigin();
				VectorNormalize( vecDir );
				
				vecSrc = vecSrc + vecDir * (gpGlobals->curtime - m_flShootTime);
				
				pEntity = (CNihilanthHVR *)CREATE_ENTITY( CNihilanthHVR,  "nihilanth_energy_ball" );

				pEntity->SetAbsOrigin( vecSrc );
				pEntity->SetAbsAngles( vecAngle );
				pEntity->SetOwnerEntity( this );
				pEntity->Spawn();

				pEntity->SetAbsVelocity( vecDir * 200.0 );
				pEntity->ZapInit( GetEnemy() );

			}

			m_flShootTime += 0.2;
		}
	}
}

void CNPC_Nihilanth::FloatSequence( void )
{
	if (m_irritation >= 2)
	{
		SetSequence( LookupSequence( "float_open" ) );
	}
	else if (m_avelocity.y > 30)
	{
		SetSequence( LookupSequence( "walk_r" ) );
	}
	else if (m_avelocity.y < -30)
	{
		SetSequence( LookupSequence( "walk_l" ) );
	}
	else if (m_velocity.z > 30)
	{
		SetSequence( LookupSequence( "walk_u" ) );
	} 
	else if (m_velocity.z < -30)
	{
		SetSequence( LookupSequence( "walk_d" ) );
	}
	else
	{
		SetSequence( LookupSequence( "float" ) );
	}
}

void CNPC_Nihilanth::DyingThink( void )
{
	SetNextThink( gpGlobals->curtime + 0.1 );
	DispatchAnimEvents( this );
	StudioFrameAdvance( );

	if ( m_lifeState == LIFE_ALIVE )
	{
		CTakeDamageInfo info;
		DeathSound( info );
		m_lifeState = LIFE_DYING;

		m_posDesired.z = m_flMaxZ;
	}

	if ( GetAbsOrigin().z < m_flMaxZ && m_lifeState == LIFE_DEAD )
	{
		 SetAbsOrigin( Vector( GetAbsOrigin().x, GetAbsOrigin().y, m_flMaxZ ) );
		 SetAbsVelocity( Vector( 0, 0, 0 ) );
	}

	if ( m_lifeState == LIFE_DYING )
	{
		Flight( );

		if (fabs( GetAbsOrigin().z - m_flMaxZ ) < 16)
		{
			CBaseEntity *pTrigger = NULL;

			SetAbsVelocity( Vector( 0, 0, 0 ) );
			SetGravity( 0 );

			while( ( pTrigger = gEntList.FindEntityByName( pTrigger, m_szDeadUse ) ) != NULL )
			{
				CLogicRelay *pRelay = (CLogicRelay*)pTrigger;
				pRelay->m_OnTrigger.FireOutput( this, this );
			}

			m_lifeState = LIFE_DEAD;
		}
	}

	if ( IsSequenceFinished() )
	{
		QAngle qAngularVel = GetLocalAngularVelocity();

		qAngularVel.y += random->RandomFloat( -100, 100 );
	
		if ( qAngularVel.y < -100)
			 qAngularVel.y = -100;
		if ( qAngularVel.y > 100)
			 qAngularVel.y = 100;

		SetLocalAngularVelocity( qAngularVel );
		SetSequence( LookupSequence( "die1" ) );
	}

	if ( m_pBall )
	{
		if (m_pBall->GetBrightness() > 0)
		{
			m_pBall->SetBrightness( MAX( 0, m_pBall->GetBrightness() - 7 ), 0 );
		}
		else
		{
			UTIL_Remove( m_pBall );
			m_pBall = NULL;
		}
	}

	Vector vecDir, vecSrc;
	QAngle vecAngles;
	Vector vForward, vRight, vUp;

	AngleVectors( GetAbsAngles(), &vForward, &vRight, &vUp );

	int iAttachment = random->RandomInt( 1, 4 );

	do {
		vecDir = Vector( random->RandomFloat( -1, 1 ), random->RandomFloat( -1, 1 ), random->RandomFloat( -1, 1 ) );
	} while (DotProduct( vecDir, vecDir) > 1.0);

	switch( random->RandomInt( 1, 4 ))
	{
	case 1: // head
		vecDir.z = fabs( vecDir.z ) * 0.5;
		vecDir = vecDir + 2 * vUp;
		break;
	case 2: // eyes
		if (DotProduct( vecDir, vForward ) < 0)
			vecDir = vecDir * -1;

		vecDir = vecDir + 2 * vForward;
		break;
	case 3: // left hand
		if (DotProduct( vecDir, vRight ) > 0)
			vecDir = vecDir * -1;
		vecDir = vecDir - 2 * vRight;
		break;
	case 4: // right hand
		if (DotProduct( vecDir, vRight ) < 0)
			vecDir = vecDir * -1;
		vecDir = vecDir + 2 * vRight;
		break;
	}

	GetAttachment( iAttachment, vecSrc, vecAngles );

	trace_t tr;

	UTIL_TraceLine( vecSrc, vecSrc + vecDir * 4096, MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );

	CBeam *pBeam = CBeam::BeamCreate( "sprites/laserbeam.vmt", 16 );

	if ( pBeam == NULL )
		 return;

	pBeam->PointEntInit( tr.endpos, this );
	pBeam->SetEndAttachment( iAttachment );
	pBeam->SetColor( 64, 128, 255 );
	pBeam->SetFadeLength( 50 );
	pBeam->SetBrightness( 255 );
	pBeam->SetNoise( 12 );
	pBeam->SetScrollRate( 1.0 );
	pBeam->LiveForTime( 0.5 );
	
	GetAttachment( 2, vecSrc, vecAngles ); 
	CNihilanthHVR *pEntity = (CNihilanthHVR *)CREATE_ENTITY( CNihilanthHVR, "nihilanth_energy_ball" );
	
	pEntity->SetAbsOrigin( vecSrc );
	pEntity->SetAbsAngles( GetAbsAngles() );
	pEntity->SetOwnerEntity( this );
	pEntity->SetAbsVelocity( Vector ( random->RandomFloat( -0.7, 0.7 ), random->RandomFloat( -0.7, 0.7 ), 1.0 ) * 600.0 );
	pEntity->Spawn();

	pEntity->GreenBallInit();

	return;
}


void CNPC_Nihilanth::HandleAnimEvent( animevent_t *pEvent )
{
	switch( pEvent->event )
	{
	case 1:	// shoot 
		break;
	case 2:	// zen
		if ( GetEnemy() != NULL)
		{
			Vector vOrigin;
			QAngle vAngle;
			CPASAttenuationFilter filter( this );
		
			if ( random->RandomInt(0,4) == 0)
				EmitSound( filter, entindex(), "Nihilanth.Attack" ); 
			
			EmitSound( filter, entindex(), "Nihilanth.BallAttack" ); 

			GetAttachment( 2, vOrigin, vAngle);
			
			CBroadcastRecipientFilter filterlight;

			te->DynamicLight( filterlight, 0.0, &vOrigin, 128, 128, 255, 0, 256, 1.0f, 128 );

			GetAttachment( 3, vOrigin, vAngle);
			te->DynamicLight( filterlight, 0.0, &vOrigin, 128, 128, 255, 0, 256, 1.0f, 128 );
		
			m_flShootTime = gpGlobals->curtime;
			m_flShootEnd = gpGlobals->curtime + 1.0;
		}
		break;
	case 3:	// prayer
		if (GetEnemy() != NULL)
		{
			char szText[32];

			Q_snprintf( szText, sizeof( szText ), "%s%d", m_szTeleportTouch, m_iTeleport );
			CBaseEntity *pTouch = gEntList.FindEntityByName( NULL, szText );

			Q_snprintf( szText, sizeof( szText ), "%s%d", m_szTeleportUse, m_iTeleport );
			CBaseEntity *pTrigger = gEntList.FindEntityByName( NULL, szText );

			if (pTrigger != NULL || pTouch != NULL)
			{
				CPASAttenuationFilter filter( this );
				EmitSound( filter, entindex(), "Nihilanth.Attack" ); 

				Vector vecSrc;
				QAngle vecAngles;

				GetAttachment( 2, vecSrc, vecAngles ); 

				CNihilanthHVR *pEntity = (CNihilanthHVR *)CREATE_ENTITY( CNihilanthHVR, "nihilanth_energy_ball" );
				
				pEntity->SetAbsOrigin( vecSrc );
				pEntity->SetAbsAngles( vecAngles );
				pEntity->SetOwnerEntity( this );
				pEntity->Spawn();

				pEntity->TeleportInit( this, GetEnemy(), pTrigger, pTouch );

				pEntity->SetAbsVelocity( GetAbsOrigin() - vecSrc );
				pEntity->SetAbsVelocity( Vector( GetAbsVelocity().x, GetAbsVelocity().y, GetAbsVelocity().z * 0.2 ) );

			}
			else
			{
				Vector vOrigin;
				QAngle vAngle;

				m_iTeleport++; // unexpected failure

				CPASAttenuationFilter filter( this );
				EmitSound( filter, entindex(), "Nihilanth.BallAttack" ); 

				Msg( "nihilanth can't target %s\n", szText );

				GetAttachment( 2, vOrigin, vAngle);
			
				CBroadcastRecipientFilter filterlight;

				te->DynamicLight( filterlight, 0.0, &vOrigin, 128, 128, 255, 0, 256, 1.0f, 128 );

				GetAttachment( 3, vOrigin, vAngle);
				te->DynamicLight( filterlight, 0.0, &vOrigin, 128, 128, 255, 0, 256, 1.0f, 128 );

				m_flShootTime = gpGlobals->curtime;
				m_flShootEnd = gpGlobals->curtime + 1.0;
			}
		}
		break;
	case 4:	// get a sphere
		{
			if (m_hRecharger != NULL)
			{
				if (!EmitSphere( ))
				{
					m_hRecharger = NULL;
				}
			}
		}
		break;
	case 5:	// start up sphere machine
		{
			CPASAttenuationFilter filter( this );
			EmitSound( filter, entindex(), "Nihilanth.Recharge" ); 
		}
		break;
	case 6:
		if ( GetEnemy() != NULL)
		{
			Vector vecSrc;
			QAngle vecAngles;
			GetAttachment( 3, vecSrc, vecAngles ); 
						
			CNihilanthHVR *pEntity = (CNihilanthHVR *)CREATE_ENTITY( CNihilanthHVR,  "nihilanth_energy_ball" );

			pEntity->SetAbsOrigin( vecSrc );
			pEntity->SetAbsAngles( vecAngles );
			pEntity->SetOwnerEntity( this );
			pEntity->Spawn();

			pEntity->SetAbsVelocity( GetAbsOrigin() - vecSrc );
			pEntity->ZapInit( GetEnemy() );
		}
		break;
	case 7:
		/*
		Vector vecSrc, vecAngles;
		GetAttachment( 0, vecSrc, vecAngles ); 
		CNihilanthHVR *pEntity = (CNihilanthHVR *)Create( "nihilanth_energy_ball", vecSrc, pev->angles, edict() );
		pEntity->pev->velocity = Vector ( RANDOM_FLOAT( -0.7, 0.7 ), RANDOM_FLOAT( -0.7, 0.7 ), 1.0 ) * 600.0;
		pEntity->GreenBallInit( );
		*/
		break;
	}
}


//=========================================================
// Controller bouncy ball attack
//=========================================================



void CNihilanthHVR::Spawn( void )
{
	Precache( );
}


void CNihilanthHVR::Precache( void )
{
	PrecacheModel("sprites/flare6.vmt");
	PrecacheModel("sprites/nhth1.vmt");
	PrecacheModel("sprites/exit1.vmt");
	PrecacheModel("sprites/tele1.vmt");
	PrecacheModel("sprites/animglow01.vmt");
	PrecacheModel("sprites/xspark4.vmt");
	PrecacheModel("sprites/muzzleflash3.vmt");

	PrecacheModel("sprites/laserbeam.vmt");

	PrecacheScriptSound( "NihilanthHVR.Zap" );
	PrecacheScriptSound( "NihilanthHVR.TeleAttack" );
}

void CNihilanthHVR::CircleInit( CBaseEntity *pTarget )
{
	SetMoveType( MOVETYPE_FLY );
	SetSolid( SOLID_NONE );

	UTIL_SetSize( this, Vector( 0, 0, 0), Vector(0, 0, 0));
	UTIL_SetOrigin( this, GetAbsOrigin() );

	SetThink( &CNihilanthHVR::HoverThink );
	SetTouch( &CNihilanthHVR::BounceTouch );
	SetNextThink( gpGlobals->curtime + 0.1 );

	CSprite *pSprite = SpriteInit( "sprites/muzzleflash3.vmt", this );
	
	if ( pSprite )
	{
		m_flBallScale = 2.0f;
		pSprite->SetScale( 2.0 );
		pSprite->SetTransparency( kRenderTransAdd, 255, 224, 192, 255, kRenderFxNone );
	}
	
	SetTarget( pTarget );
}

CSprite *CNihilanthHVR::SpriteInit( const char *pSpriteName, CNihilanthHVR *pOwner )
{
	pOwner->SetSprite( CSprite::SpriteCreate( pSpriteName, pOwner->GetAbsOrigin(), true ) );

	CSprite *pSprite = (CSprite*)pOwner->GetSprite();

	if ( pSprite )
	{
		pSprite->SetAttachment( pOwner, 0 );
		pSprite->SetOwnerEntity( pOwner );
		pSprite->AnimateForTime( 5, 9999 );
	}

	return pSprite;
}

void CNihilanthHVR::HoverThink( void  )
{
	SetNextThink( gpGlobals->curtime + 0.1 );

	if ( GetTarget() != NULL )
	{
		CircleTarget( GetTarget()->GetAbsOrigin() + Vector( 0, 0, 16 * N_SCALE ) );
	}
	else
	{
		UTIL_Remove( GetSprite() );
		UTIL_Remove( this );
	}
}

void CNihilanthHVR::BounceTouch( CBaseEntity *pOther )
{
	Vector vecDir = m_vecIdeal;

	VectorNormalize( vecDir );

	const trace_t &tr = GetTouchTrace();

	float n = -DotProduct(tr.plane.normal, vecDir);

	vecDir = 2.0 * tr.plane.normal * n + vecDir;

	m_vecIdeal = vecDir * m_vecIdeal.Length();
}

bool CNihilanthHVR::CircleTarget( Vector vecTarget )
{
	bool fClose = false;

	vecTarget = vecTarget + Vector( 0, 0, 64 );

	Vector vecDest = vecTarget;
	Vector vecEst = GetAbsOrigin() + GetAbsVelocity() * 0.5;
	Vector vecSrc = GetAbsOrigin();
	vecDest.z = 0;
	vecEst.z = 0;
	vecSrc.z = 0;
	float d1 = (vecDest - vecSrc).Length() - 24 * N_SCALE;
	float d2 = (vecDest - vecEst).Length() - 24 * N_SCALE;

	if ( m_vecIdeal == vec3_origin )
	{
		m_vecIdeal = GetAbsVelocity();
	}

	if (d1 < 0 && d2 <= d1)
	{
		// ALERT( at_console, "too close\n");
		Vector vTemp = (vecDest - vecSrc);

		VectorNormalize( vTemp );

		m_vecIdeal = m_vecIdeal - vTemp * 50;
	}

	else if (d1 > 0 && d2 >= d1)
	{
		Vector vTemp = (vecDest - vecSrc);

		VectorNormalize( vTemp );

		m_vecIdeal = m_vecIdeal + vTemp * 50;
	}

	SetLocalAngularVelocity( QAngle( GetLocalAngularVelocity().x, GetLocalAngularVelocity().y, d1 * 20 ) );
	
	if (d1 < 32)
	{
		fClose = true;
	}

	m_vecIdeal = m_vecIdeal + Vector( random->RandomFloat( -2, 2 ), random->RandomFloat( -2, 2 ), random->RandomFloat( -2, 2 ));
	
	float flIdealZ = m_vecIdeal.z;
	
	m_vecIdeal = Vector( m_vecIdeal.x, m_vecIdeal.y, 0 );
	
	VectorNormalize( m_vecIdeal );
	
	m_vecIdeal = (m_vecIdeal * 200) + Vector( 0, 0, flIdealZ );
	
	// move up/down
	d1 = vecTarget.z - GetAbsOrigin().z;
	if (d1 > 0 && m_vecIdeal.z < 200)
		m_vecIdeal.z += 200;
	else if (d1 < 0 && m_vecIdeal.z > -200)
		m_vecIdeal.z -= 200;

	SetAbsVelocity( m_vecIdeal );

	// ALERT( at_console, "%.0f %.0f %.0f\n", m_vecIdeal.x, m_vecIdeal.y, m_vecIdeal.z );
	return fClose;
}


void CNihilanthHVR::ZapInit( CBaseEntity *pEnemy )
{
	SetMoveType( MOVETYPE_FLY );
	SetSolid( SOLID_BBOX );
	
	CSprite *pSprite = SpriteInit( "sprites/nhth1.vmt", this );
	
	if ( pSprite )
	{
		m_flBallScale = 2.0f;
		pSprite->SetScale( 2.0 );
		pSprite->SetTransparency( kRenderTransAdd, 255, 255, 255, 255, kRenderFxNone );
	}
	

	Vector vVelocity = pEnemy->GetAbsOrigin() - GetAbsOrigin();
	VectorNormalize( vVelocity );
	SetAbsVelocity ( vVelocity * 300 );

	SetEnemy( pEnemy );
	SetThink( &CNihilanthHVR::ZapThink );
	SetTouch( &CNihilanthHVR::ZapTouch );
	SetNextThink( gpGlobals->curtime + 0.1 );

	CPASAttenuationFilter filter( this );
	EmitSound( filter, entindex(), "NihilanthHVR.Zap" );
}

void CNihilanthHVR::ZapThink( void  )
{
	SetNextThink( gpGlobals->curtime + 0.05 );

	// check world boundaries
	if ( GetEnemy() == NULL ||  GetAbsOrigin().x < -4096 || GetAbsOrigin().x > 4096 || GetAbsOrigin().y < -4096 || GetAbsOrigin().y > 4096 || GetAbsOrigin().z < -4096 || GetAbsOrigin().z > 4096)
	{
		SetTouch( NULL );
		UTIL_Remove( GetSprite() );
		UTIL_Remove( this );
		return;
	}

	if ( GetAbsVelocity().Length() < 2000)
	{
		SetAbsVelocity( GetAbsVelocity() * 1.2 );
	}

	if (( GetEnemy()->WorldSpaceCenter() - GetAbsOrigin()).Length() < 256)
	{
		trace_t tr;

		UTIL_TraceLine( GetAbsOrigin(), GetEnemy()->WorldSpaceCenter(), MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );

		CBaseEntity *pEntity = tr.m_pEnt;

		if (pEntity != NULL && pEntity->m_takedamage )
		{
			ClearMultiDamage( );
			CTakeDamageInfo info( this, this, sk_nihilanth_zap.GetFloat(), DMG_SHOCK );
			CalculateMeleeDamageForce( &info, (tr.endpos - tr.startpos), tr.endpos );
			pEntity->DispatchTraceAttack( info, GetAbsVelocity(), &tr  );
			ApplyMultiDamage();
		}

		CBeam *pBeam = CBeam::BeamCreate( "sprites/laserbeam.vmt", 2.0f );

		if ( pBeam == NULL )
			 return;

		pBeam->PointEntInit( tr.endpos, this );
		pBeam->SetColor( 64, 196, 255 );
		pBeam->SetBrightness( 255 );
		pBeam->SetNoise( 7.2 );
		pBeam->SetScrollRate( 10 );
		pBeam->LiveForTime( 0.1 );

		UTIL_EmitAmbientSound( GetSoundSourceIndex(), tr.endpos, "Controller.ElectroSound", 0.5, SNDLVL_NORM, 0, random->RandomInt( 140, 160 ) );

		SetTouch( NULL );
		GetSprite()->SetThink( &CBaseEntity::SUB_Remove );
		SetThink( &CBaseEntity::SUB_Remove );
		SetNextThink( gpGlobals->curtime + 0.2 );
		GetSprite()->SetNextThink( gpGlobals->curtime + 0.2 );
		return;
	}

	CBroadcastRecipientFilter filterlight;
	te->DynamicLight( filterlight, 0.0, &GetAbsOrigin(), 128, 128, 255, 0, 128, 10, 128 );
}


void CNihilanthHVR::ZapTouch( CBaseEntity *pOther )
{
	UTIL_EmitAmbientSound( GetSoundSourceIndex(), GetAbsOrigin(), "Controller.ElectroSound", 1.0, SNDLVL_NORM, 0, random->RandomInt( 90, 95 ) );

	RadiusDamage( CTakeDamageInfo( this, this, 50, DMG_SHOCK ), GetAbsOrigin(), 125,  CLASS_NONE, NULL );
	SetAbsVelocity( GetAbsVelocity() * 0 );

	SetTouch( NULL );
	UTIL_Remove( GetSprite() );
	UTIL_Remove( this );
	SetNextThink( gpGlobals->curtime + 0.2 );
}

void CNihilanthHVR::AbsorbInit( void  )
{
	CBroadcastRecipientFilter filter;

	SetThink( &CNihilanthHVR::DissipateThink );
	SetRenderColorA( 255 );

	SetBeam( CBeam::BeamCreate( "sprites/laserbeam.vmt", 8.0f ) );

	CBeam *pBeam = (CBeam*)GetBeam();

	if ( pBeam == NULL )
		 return;

	pBeam->EntsInit( this, GetTarget() );
	pBeam->SetEndAttachment( 1 );
	pBeam->SetColor( 255, 128, 64 );
	pBeam->SetBrightness( 255 );
	pBeam->SetNoise( 18 );
}

void CNihilanthHVR::DissipateThink( void  )
{
	CSprite *pSprite = (CSprite*)GetSprite();

	SetNextThink ( gpGlobals->curtime + 0.1 );

	if ( m_flBallScale > 5.0)
	{
		UTIL_Remove( this );
		UTIL_Remove( GetSprite() );
		UTIL_Remove( GetBeam() );
	}

	pSprite->SetBrightness( pSprite->GetBrightness() - 7, 0 );
	
	m_flBallScale += 0.1;
	pSprite->SetScale( m_flBallScale );

	if ( GetTarget() != NULL)
	{
		CircleTarget( GetTarget()->GetAbsOrigin() + Vector( 0, 0, 4096 ) );
	}
	else
	{
		UTIL_Remove( this );
		UTIL_Remove( GetSprite() );
		UTIL_Remove( GetBeam() );
	}

/*	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_ELIGHT );
		WRITE_SHORT( entindex( ) );		// entity, attachment
		WRITE_COORD( pev->origin.x );		// origin
		WRITE_COORD( pev->origin.y );
		WRITE_COORD( pev->origin.z );
		WRITE_COORD( pev->renderamt );	// radius
		WRITE_BYTE( 255 );	// R
		WRITE_BYTE( 192 );	// G
		WRITE_BYTE( 64 );	// B
		WRITE_BYTE( 2 );	// life * 10
		WRITE_COORD( 0 ); // decay
	MESSAGE_END();*/
}

void CNihilanthHVR::TeleportInit( CNPC_Nihilanth *pOwner, CBaseEntity *pEnemy, CBaseEntity *pTarget, CBaseEntity *pTouch )
{
	SetMoveType( MOVETYPE_FLY );
	SetSolid( SOLID_BBOX );

	SetModel( "" );

	CSprite *pSprite = SpriteInit( "sprites/exit1.vmt", this );
	
	if ( pSprite )
	{
		m_flBallScale = 2.0f;
		pSprite->SetScale( 2.0 );
		pSprite->SetTransparency( kRenderTransAdd, 255, 255, 255, 255, kRenderFxNone );
	}


	m_pNihilanth = pOwner;
	SetEnemy( pEnemy );
	SetTarget( pTarget );
	m_hTouch = pTouch;

	SetThink( &CNihilanthHVR::TeleportThink );
	SetTouch( &CNihilanthHVR::TeleportTouch );
	SetNextThink( gpGlobals->curtime + 0.1 );

	CPASAttenuationFilter filter( this );
	EmitSound( filter, entindex(), "NihilanthHVR.TeleAttack" ); 
}

void CNihilanthHVR::MovetoTarget( Vector vecTarget )
{
	if ( m_vecIdeal == vec3_origin )
	{
		m_vecIdeal = GetAbsVelocity();
	}

	// accelerate
	float flSpeed = m_vecIdeal.Length();

	if (flSpeed > 300)
	{
		VectorNormalize( m_vecIdeal );
		m_vecIdeal= m_vecIdeal * 300;
	}

	Vector vTemp = vecTarget - GetAbsOrigin();
	VectorNormalize( vTemp );

	m_vecIdeal = m_vecIdeal + vTemp * 300;
	SetAbsVelocity( m_vecIdeal );
}

void CNihilanthHVR::TeleportThink( void  )
{
	SetNextThink( gpGlobals->curtime + 0.1 );

	// check world boundaries
	if ( GetEnemy() == NULL || !GetEnemy()->IsAlive() || GetAbsOrigin().x < -4096 || GetAbsOrigin().x > 4096 || GetAbsOrigin().y < -4096 || GetAbsOrigin().y > 4096 || GetAbsOrigin().z < -4096 || GetAbsOrigin().z > 4096)
	{
		StopSound( entindex(), "NihilanthHVR.TeleAttack" );
		UTIL_Remove( this );
		UTIL_Remove( GetSprite() );
		return;
	}

	if (( GetEnemy()->WorldSpaceCenter() - GetAbsOrigin() ).Length() < 128)
	{
		StopSound( entindex(), "NihilanthHVR.TeleAttack" );
		UTIL_Remove( this );
		UTIL_Remove( GetSprite() );

		if ( GetTarget() != NULL)
		{
			 CLogicRelay *pRelay = (CLogicRelay*)GetTarget();
			 pRelay->m_OnTrigger.FireOutput( this, this );
		}

		if ( m_hTouch != NULL && GetEnemy() != NULL )
			m_hTouch->Touch( GetEnemy() );
	}
	else 
	{
		MovetoTarget( GetEnemy()->WorldSpaceCenter( ) );
	}

	CBroadcastRecipientFilter filterlight;
	te->DynamicLight( filterlight, 0.0, &GetAbsOrigin(), 0, 255, 0, 0, 256, 1.0, 256 );
}

void CNihilanthHVR::TeleportTouch( CBaseEntity *pOther )
{
	CBaseEntity *pEnemy = GetEnemy();

	if (pOther == pEnemy)
	{
		if (GetTarget() != NULL)
		{
			if ( GetTarget() != NULL)
			{
				 CLogicRelay *pRelay = (CLogicRelay*)GetTarget();
				 pRelay->m_OnTrigger.FireOutput( this, this );
			}
		}

		if (m_hTouch != NULL && pEnemy != NULL )
			m_hTouch->Touch( pEnemy );
	}
	else
	{
		m_pNihilanth->MakeFriend( GetAbsOrigin() );
	}

	SetTouch( NULL );
	StopSound( entindex(), "NihilanthHVR.TeleAttack" );
	UTIL_Remove( this );
	UTIL_Remove( GetSprite() );
}

void CNihilanthHVR::GreenBallInit( )
{
	SetMoveType( MOVETYPE_FLY );
	SetSolid( SOLID_BBOX );

	SetModel( "" );

	CSprite *pSprite = SpriteInit( "sprites/exit1.spr", this );
	
	if ( pSprite )
	{
		pSprite->SetScale( 1.0 );
		pSprite->SetTransparency( kRenderTransAdd, 255, 255, 255, 255, kRenderFxNone );
	}

	SetTouch( &CNihilanthHVR::RemoveTouch );
}

void CNihilanthHVR::RemoveTouch( CBaseEntity *pOther )
{
	StopSound( entindex(), "NihilanthHVR.TeleAttack" );
	UTIL_Remove( this );
	UTIL_Remove( GetSprite() );
}
