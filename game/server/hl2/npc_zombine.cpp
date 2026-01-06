//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Combine Zombie... Zombie Combine... its like a... Zombine... get it?
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "ai_basenpc.h"
#include "ai_default.h"
#include "ai_schedule.h"
#include "ai_hull.h"
#include "ai_motor.h"
#include "ai_memory.h"
#include "ai_route.h"
#include "ai_squad.h"
#include "soundent.h"
#include "game.h"
#include "npcevent.h"
#include "entitylist.h"
#include "ai_task.h"
#include "activitylist.h"
#include "engine/IEngineSound.h"
#include "npc_BaseZombie.h"
#include "movevars_shared.h"
#include "IEffects.h"
#include "props.h"
#include "physics_npc_solver.h"
#include "hl2_player.h"
#include "hl2_gamerules.h"

#include "basecombatweapon.h"
#include "basegrenade_shared.h"
#include "grenade_frag.h"
#ifdef EZ2
#include "grenade_hopwire.h"
#include "ez2/npc_husk_soldier.h"
#endif

#include "ai_interactions.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

enum
{	
	SQUAD_SLOT_ZOMBINE_SPRINT1 = LAST_SHARED_SQUADSLOT,
	SQUAD_SLOT_ZOMBINE_SPRINT2,
};

#define MIN_SPRINT_TIME 3.5f
#define MAX_SPRINT_TIME 5.5f

#define MIN_SPRINT_DISTANCE 64.0f
#define MAX_SPRINT_DISTANCE 1024.0f

#define SPRINT_CHANCE_VALUE 10
#define SPRINT_CHANCE_VALUE_DARKNESS 50

#define GRENADE_PULL_MAX_DISTANCE 256.0f

#define ZOMBINE_MAX_GRENADES 1

int ACT_ZOMBINE_GRENADE_PULL;
int ACT_ZOMBINE_GRENADE_WALK;
int ACT_ZOMBINE_GRENADE_RUN;
int ACT_ZOMBINE_GRENADE_IDLE;
int ACT_ZOMBINE_ATTACK_FAST;
int ACT_ZOMBINE_GRENADE_FLINCH_BACK;
int ACT_ZOMBINE_GRENADE_FLINCH_FRONT;
int ACT_ZOMBINE_GRENADE_FLINCH_WEST;
int ACT_ZOMBINE_GRENADE_FLINCH_EAST;

int AE_ZOMBINE_PULLPIN;

extern bool IsAlyxInDarknessMode();

ConVar	sk_zombie_soldier_health( "sk_zombie_soldier_health","150"); // Breadman - was 0

float g_flZombineGrenadeTimes = 0;

#ifdef EZ
string_t gm_iszZombineGrenadeTypeFrag;
string_t gm_iszZombineGrenadeTypeXen;
string_t gm_iszZombineGrenadeTypeStunstick;
string_t gm_iszZombineGrenadeTypeManhack;
string_t gm_iszZombineGrenadeTypeCrowbar;

int	g_interactionZombinePullGrenade		= 0;
#endif

class CNPC_Zombine : public CAI_BlendingHost<CNPC_BaseZombie>, public CDefaultPlayerPickupVPhysics
{
	DECLARE_DATADESC();
	DECLARE_CLASS( CNPC_Zombine, CAI_BlendingHost<CNPC_BaseZombie> );

public:

	void Spawn( void );
	void EquipWeapon();
	void Precache( void );

	void AllocPooledStringsForGrenadeTypes();

	void SetZombieModel( void );

	virtual void PrescheduleThink( void );
	virtual int SelectSchedule( void );
	virtual void BuildScheduleTestBits( void );

	virtual void HandleAnimEvent( animevent_t *pEvent );
#ifndef EZ
	virtual const char *GetLegsModel( void );
	virtual const char *GetTorsoModel( void );
#endif
	virtual const char *GetHeadcrabClassname( void );
	virtual const char *GetHeadcrabModel( void );

#ifdef EZ
	virtual CBaseEntity *ClawAttack( float flDist, int iDamage, QAngle &qaViewPunch, Vector &vecVelocityPunch, int BloodOrigin );
#endif

	virtual void PainSound( const CTakeDamageInfo &info );
	virtual void DeathSound( const CTakeDamageInfo &info );
	virtual void AlertSound( void );
	virtual void IdleSound( void );
	virtual void AttackSound( void );
	virtual void AttackHitSound( void );
	virtual void AttackMissSound( void );
	virtual void FootstepSound( bool fRightFoot );
	virtual void FootscuffSound( bool fRightFoot );
	virtual void MoanSound( envelopePoint_t *pEnvelope, int iEnvelopeSize );
#ifdef EZ
	virtual void ChargeSound( void );
	virtual void ReadyGrenadeSound( void );

	virtual void ChooseDefaultGrenadeType();
#endif

	virtual void Event_Killed( const CTakeDamageInfo &info );
	virtual void TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr, CDmgAccumulator *pAccumulator );
	virtual void RunTask( const Task_t *pTask );
	virtual int  MeleeAttack1Conditions ( float flDot, float flDist );

	virtual bool ShouldBecomeTorso( const CTakeDamageInfo &info, float flDamageThreshold );

	virtual void OnScheduleChange ( void );
	virtual bool CanRunAScriptedNPCInteraction( bool bForced );

#ifdef EZ
	virtual Disposition_t	IRelationType( CBaseEntity *pTarget );
#endif

	void GatherGrenadeConditions( void );

	virtual Activity NPC_TranslateActivity( Activity baseAct );

	const char *GetMoanSound( int nSound );

	bool AllowedToSprint( void );
	void Sprint( bool bMadSprint = false );
	void StopSprint( void );

	void DropGrenade( Vector vDir );

	bool IsSprinting( void ) { return m_flSprintTime > gpGlobals->curtime;	}
	bool HasGrenade( void ) { return m_hGrenade != NULL; }

	int TranslateSchedule( int scheduleType );

	void InputStartSprint ( inputdata_t &inputdata );
	void InputPullGrenade ( inputdata_t &inputdata );

	virtual CBaseEntity *OnFailedPhysGunPickup ( Vector vPhysgunPos );

	//Called when we want to let go of a grenade and let the physcannon pick it up.
	void ReleaseGrenade( Vector vPhysgunPos );

	virtual bool HandleInteraction( int interactionType, void *data, CBaseCombatCharacter *sourceEnt );

	enum
	{
		COND_ZOMBINE_GRENADE = LAST_BASE_ZOMBIE_CONDITION,
	};

	enum
	{
		SCHED_ZOMBINE_PULL_GRENADE = LAST_BASE_ZOMBIE_SCHEDULE,
	};

public:
	DEFINE_CUSTOM_AI;

private:

	float	m_flSprintTime;
	float	m_flSprintRestTime;

	float	m_flSuperFastAttackTime;
	float   m_flGrenadePullTime;

#ifdef MAPBASE
	int		m_iGrenadeCount = ZOMBINE_MAX_GRENADES;
#else
	int		m_iGrenadeCount;
#endif

#ifdef MAPBASE
	COutputEHANDLE m_OnGrenade;
#endif
	EHANDLE	m_hGrenade;

protected:
	static const char *pMoanSounds[];

#ifdef EZ
	string_t	m_iszGrenadeType = NULL_STRING;
#endif
};

LINK_ENTITY_TO_CLASS( npc_zombine, CNPC_Zombine );

BEGIN_DATADESC( CNPC_Zombine )
	DEFINE_FIELD( m_flSprintTime, FIELD_TIME ),
	DEFINE_FIELD( m_flSprintRestTime, FIELD_TIME ),
	DEFINE_FIELD( m_flSuperFastAttackTime, FIELD_TIME ),
	DEFINE_FIELD( m_hGrenade, FIELD_EHANDLE ),
	DEFINE_FIELD( m_flGrenadePullTime, FIELD_TIME ),
#ifdef MAPBASE
	DEFINE_KEYFIELD( m_iGrenadeCount, FIELD_INTEGER, "NumGrenades" ),
	DEFINE_OUTPUT( m_OnGrenade, "OnPullGrenade" ),
#else
	DEFINE_FIELD( m_iGrenadeCount, FIELD_INTEGER ),
#endif

#ifdef EZ
	DEFINE_KEYFIELD( m_iszGrenadeType, FIELD_STRING, "GrenadeType" ),
#endif

	DEFINE_INPUTFUNC( FIELD_VOID,	"StartSprint", InputStartSprint ),
	DEFINE_INPUTFUNC( FIELD_VOID,	"PullGrenade", InputPullGrenade ),
END_DATADESC()

#ifdef EZ2
ConVar	sk_zombiecop_health( "sk_zombiecop_health", "0" );

class CNPC_MetroZombie : public CNPC_Zombine
{
	DECLARE_CLASS( CNPC_MetroZombie, CNPC_Zombine );

	void Spawn( void );
	void Precache( void );
	void ChooseDefaultGrenadeType();

	virtual void PainSound( const CTakeDamageInfo &info );
	virtual void DeathSound( const CTakeDamageInfo &info );
	virtual void AlertSound( void );
	virtual void IdleSound( void );
	virtual void FootscuffSound( bool fRightFoot );
	virtual void ChargeSound( void );
	virtual void ReadyGrenadeSound( void );
};

LINK_ENTITY_TO_CLASS( npc_metrozombie, CNPC_MetroZombie );

//---------------------------------------------------------------------------------

ConVar	sk_hevzombie_health( "sk_hevzombie_health", "0" );

class CNPC_HEVZombie : public CNPC_Zombine
{
	DECLARE_DATADESC()
	DECLARE_CLASS( CNPC_HEVZombie, CNPC_Zombine );

public:
	CNPC_HEVZombie() {
		m_bDropItems = true;
		m_ArmorValue = 100;
	}

	void Spawn( void );
	void Precache( void );
	void ChooseDefaultGrenadeType();

	virtual void PainSound( const CTakeDamageInfo &info );
	virtual void DeathSound( const CTakeDamageInfo &info );
	virtual void AlertSound( void );
	virtual void IdleSound( void );
	virtual void FootscuffSound( bool fRightFoot );
	virtual void ChargeSound( void );
	virtual void ReadyGrenadeSound( void );

	int	OnTakeDamage( const CTakeDamageInfo &info );
	virtual void Event_Killed( const CTakeDamageInfo &info );

	// HEV zombies never drop headcrabs or become torsos
	virtual bool ShouldBecomeTorso( const CTakeDamageInfo &info, float flDamageThreshold ) { return false; }
	virtual HeadcrabRelease_t ShouldReleaseHeadcrab( const CTakeDamageInfo &info, float flDamageThreshold ) { return RELEASE_NO; }

private:
	bool m_bDropItems;
	int	m_ArmorValue;
};

BEGIN_DATADESC( CNPC_HEVZombie )
DEFINE_KEYFIELD( m_bDropItems, FIELD_BOOLEAN, "DropItems" ),
DEFINE_INPUT( m_ArmorValue, FIELD_INTEGER, "SetArmor" ), // From npc_clonecop
END_DATADESC()

LINK_ENTITY_TO_CLASS( npc_hevzombie, CNPC_HEVZombie );

//---------------------------------------------------------------------------------

ConVar	sk_zombie_elite_health( "sk_zombie_elite_health", "140" );
ConVar	sk_zombie_grunt_health( "sk_zombie_grunt_health", "75" );
ConVar	sk_zombie_ordinal_health( "sk_zombie_ordinal_health", "90" );
ConVar	sk_zombie_charger_health( "sk_zombie_charger_health", "125" );
ConVar	sk_zombie_suppressor_health( "sk_zombie_suppressor_health", "140" );

extern ConVar sk_husk_charger_armor_hitgroup_scale;

static ConVar *g_HuskZombineValues_Health[] =
{
	NULL, // Default, should never be used

	&sk_zombie_soldier_health,
	&sk_zombie_elite_health,

	&sk_zombie_grunt_health,
	&sk_zombie_ordinal_health,
	&sk_zombie_charger_health,
	&sk_zombie_suppressor_health,
};

static const char *g_HuskZombineValues_DefaultModels[] =
{
	NULL, // Default, should never be used

	"models/zombie/%s_soldier.mdl",
	"models/zombie/%s_super_soldier.mdl",

	"models/zombie/%s_grunt.mdl",
	"models/zombie/%s_ordinal.mdl",
	"models/zombie/%s_charger.mdl",
	"models/zombie/%s_suppressor.mdl",
};

class CNPC_HuskZombine : public CNPC_Zombine
{
	DECLARE_DATADESC()
	DECLARE_CLASS( CNPC_HuskZombine, CNPC_Zombine );

	void Spawn( void );
	void Precache( void );

	void ChooseDefaultGrenadeType();

	void FootstepSound( bool fRightFoot );
	
	bool ShouldBecomeTorso( const CTakeDamageInfo &info, float flDamageThreshold );
	bool IsChopped( const CTakeDamageInfo &info );

	float GetHitgroupDamageMultiplier( int iHitGroup, const CTakeDamageInfo &info );

private:

	HuskSoldierType_t	m_tHuskSoldierType;
};

BEGIN_DATADESC( CNPC_HuskZombine )
	DEFINE_KEYFIELD( m_tHuskSoldierType, FIELD_INTEGER, "HuskSoldierType" ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( npc_husk_zombine, CNPC_HuskZombine );
#endif

//---------------------------------------------------------
//---------------------------------------------------------
const char *CNPC_Zombine::pMoanSounds[] =
{
	"ATV_engine_null",
};

void CNPC_Zombine::Spawn( void )
{
	Precache();

	m_fIsTorso = false;
#ifndef MAPBASE // Controlled by KV
	m_fIsHeadless = false;
#endif
	
#ifdef EZ
	if ( m_tEzVariant == EZ_VARIANT_RAD ) 
	{
		SetBloodColor( BLOOD_COLOR_BLUE );
	}
	else 
	{
		SetBloodColor( BLOOD_COLOR_ZOMBIE );
	}
#elif HL2_EPISODIC
	SetBloodColor( BLOOD_COLOR_ZOMBIE );
#else
	SetBloodColor( BLOOD_COLOR_GREEN );
#endif // HL2_EPISODIC

	m_iHealth			= sk_zombie_soldier_health.GetFloat();
	SetMaxHealth( m_iHealth );

	m_flFieldOfView		= 0.2;

	CapabilitiesClear();

	BaseClass::Spawn();

	m_flSprintTime = 0.0f;
	m_flSprintRestTime = 0.0f;

	m_flNextMoanSound = gpGlobals->curtime + random->RandomFloat( 1.0, 4.0 );

	g_flZombineGrenadeTimes = gpGlobals->curtime;
	m_flGrenadePullTime = gpGlobals->curtime;

#ifndef MAPBASE
	m_iGrenadeCount = ZOMBINE_MAX_GRENADES;
#endif

#ifdef EZ
	if ( m_iszGrenadeType == NULL_STRING )
	{
		ChooseDefaultGrenadeType();
	}

	if (strncmp( STRING( m_iszGrenadeType ), "weapon_", 7 ) == 0)
	{
		EquipWeapon();
		return;
	}
#endif
}

#ifdef EZ
void CNPC_Zombine::EquipWeapon()
{
	GiveWeapon( m_iszGrenadeType );
	m_iGrenadeCount = 0;

	if (GetActiveWeapon() && GetActiveWeapon()->IsMeleeWeapon())
	{
		CapabilitiesAdd( bits_CAP_WEAPON_MELEE_ATTACK1 );
	}
}
#endif

void CNPC_Zombine::Precache( void )
{
#ifndef EZ
	BaseClass::Precache();

	PrecacheModel( "models/zombie/zombie_soldier.mdl" );

	PrecacheScriptSound( "Zombie.FootstepRight" );
	PrecacheScriptSound( "Zombie.FootstepLeft" );
	PrecacheScriptSound( "Zombine.ScuffRight" );
	PrecacheScriptSound( "Zombine.ScuffLeft" );
	PrecacheScriptSound( "Zombie.AttackHit" );
	PrecacheScriptSound( "Zombie.AttackMiss" );
	PrecacheScriptSound( "Zombine.Pain" );
	PrecacheScriptSound( "Zombine.Die" );
	PrecacheScriptSound( "Zombine.Alert" );
	PrecacheScriptSound( "Zombine.Idle" );
	PrecacheScriptSound( "Zombine.ReadyGrenade" );

	PrecacheScriptSound( "ATV_engine_null" );
	PrecacheScriptSound( "Zombine.Charge" );
	PrecacheScriptSound( "Zombie.Attack" );
#else
	AllocPooledStringsForGrenadeTypes();

	char * modelVariant;
	switch (m_tEzVariant)
	{
		case EZ_VARIANT_RAD:
			modelVariant = "glowbie";
			PrecacheScriptSound( "Glowbie.FootstepRight" );
			PrecacheScriptSound( "Glowbie.FootstepLeft" );
			PrecacheScriptSound( "Glowbine.ScuffRight" );
			PrecacheScriptSound( "Glowbine.ScuffLeft" );
			PrecacheScriptSound( "Glowbie.AttackHit" );
			PrecacheScriptSound( "Glowbie.AttackMiss" );
			PrecacheScriptSound( "Glowbine.Pain" );
			PrecacheScriptSound( "Glowbine.Die" );
			PrecacheScriptSound( "Glowbine.Alert" );
			PrecacheScriptSound( "Glowbine.Idle" );
			PrecacheScriptSound( "Glowbine.ReadyGrenade" );

			PrecacheScriptSound( "ATV_engine_null" );
			PrecacheScriptSound( "Glowbine.Charge" );
			PrecacheScriptSound( "Glowbie.Attack" );
			break;
		case EZ_VARIANT_XEN:
			modelVariant = "xenbie";
			PrecacheScriptSound( "Xenbie.FootstepRight" );
			PrecacheScriptSound( "Xenbie.FootstepLeft" );
			PrecacheScriptSound( "Xenbine.ScuffRight" );
			PrecacheScriptSound( "Xenbine.ScuffLeft" );
			PrecacheScriptSound( "Xenbie.AttackHit" );
			PrecacheScriptSound( "Xenbie.AttackMiss" );
			PrecacheScriptSound( "Xenbine.Pain" );
			PrecacheScriptSound( "Xenbine.Die" );
			PrecacheScriptSound( "Xenbine.Alert" );
			PrecacheScriptSound( "Xenbine.Idle" );
			PrecacheScriptSound( "Xenbine.ReadyGrenade" );

			PrecacheScriptSound( "ATV_engine_null" );
			PrecacheScriptSound( "Xenbine.Charge" );
			PrecacheScriptSound( "Xenbie.Attack" );
			break;
		default:
			modelVariant = "zombie";
			PrecacheScriptSound( "Zombie.FootstepRight" );
			PrecacheScriptSound( "Zombie.FootstepLeft" );
			PrecacheScriptSound( "Zombine.ScuffRight" );
			PrecacheScriptSound( "Zombine.ScuffLeft" );
			PrecacheScriptSound( "Zombie.AttackHit" );
			PrecacheScriptSound( "Zombie.AttackMiss" );
			PrecacheScriptSound( "Zombine.Pain" );
			PrecacheScriptSound( "Zombine.Die" );
			PrecacheScriptSound( "Zombine.Alert" );
			PrecacheScriptSound( "Zombine.Idle" );
			PrecacheScriptSound( "Zombine.ReadyGrenade" );

			PrecacheScriptSound( "ATV_engine_null" );
			PrecacheScriptSound( "Zombine.Charge" );
			PrecacheScriptSound( "Zombie.Attack" );
			break;
	}
	if (GetModelName() == NULL_STRING)
	{
		SetModelName( AllocPooledString( UTIL_VarArgs( "models/zombie/%s_soldier.mdl", modelVariant ) ) );
	}
	if (GetTorsoModelName() == NULL_STRING && m_tEzVariant != EZ_VARIANT_XEN)
	{
		SetTorsoModelName( AllocPooledString( UTIL_VarArgs( "models/zombie/%s_soldier_torso.mdl", modelVariant ) ) );
	}
	if (GetLegsModelName() == NULL_STRING && m_tEzVariant != EZ_VARIANT_XEN)
	{
		SetLegsModelName( AllocPooledString( UTIL_VarArgs( "models/zombie/%s_soldier_legs.mdl", modelVariant ) ) );
	}

	PrecacheModel( STRING( GetModelName() ) );

	BaseClass::Precache();
#endif
}

void CNPC_Zombine::AllocPooledStringsForGrenadeTypes()
{
	gm_iszZombineGrenadeTypeFrag = AllocPooledString( "npc_grenade_frag" );
	gm_iszZombineGrenadeTypeXen = AllocPooledString( "npc_grenade_hopwire" );
	gm_iszZombineGrenadeTypeStunstick = AllocPooledString( "weapon_stunstick" );
	gm_iszZombineGrenadeTypeManhack = AllocPooledString( "npc_manhack" );
	gm_iszZombineGrenadeTypeCrowbar = AllocPooledString( "weapon_crowbar" );
}

void CNPC_Zombine::SetZombieModel( void )
{
#ifndef EZ
	SetModel( "models/zombie/zombie_soldier.mdl" );
#else
	SetModel( STRING( GetModelName() ) );
#endif  
	SetHullType( HULL_HUMAN );

	SetBodygroup( ZOMBIE_BODYGROUP_HEADCRAB, !m_fIsHeadless );

	SetHullSizeNormal( true );
	SetDefaultEyeOffset();
	SetActivity( ACT_IDLE );
}

void CNPC_Zombine::PrescheduleThink( void )
{
	GatherGrenadeConditions();

	if( gpGlobals->curtime > m_flNextMoanSound )
	{
		if( CanPlayMoanSound() )
		{
			// Classic guy idles instead of moans.
			IdleSound();

			m_flNextMoanSound = gpGlobals->curtime + random->RandomFloat( 10.0, 15.0 );
		}
		else
		{
			m_flNextMoanSound = gpGlobals->curtime + random->RandomFloat( 2.5, 5.0 );
		}
	}

	if ( HasGrenade () )
	{
		CSoundEnt::InsertSound ( SOUND_DANGER, GetAbsOrigin() + GetSmoothedVelocity() * 0.5f , 256, 0.1, this, SOUNDENT_CHANNEL_ZOMBINE_GRENADE );

		if( IsSprinting() && GetEnemy() && GetEnemy()->Classify() == CLASS_PLAYER_ALLY_VITAL && HasCondition( COND_SEE_ENEMY ) )
		{
			if( GetAbsOrigin().DistToSqr(GetEnemy()->GetAbsOrigin()) < Square( 144 ) )
			{
				StopSprint();
			}
		}
	}

	BaseClass::PrescheduleThink();
}

void CNPC_Zombine::OnScheduleChange( void )
{
	if ( HasCondition( COND_CAN_MELEE_ATTACK1 ) && IsSprinting() == true )
	{
		m_flSuperFastAttackTime = gpGlobals->curtime + 1.0f;
	}

	BaseClass::OnScheduleChange();
}
bool CNPC_Zombine::CanRunAScriptedNPCInteraction( bool bForced )
{
	if ( HasGrenade() == true )
		return false;

	return BaseClass::CanRunAScriptedNPCInteraction( bForced );
}

#ifdef EZ
Disposition_t CNPC_Zombine::IRelationType( CBaseEntity * pTarget )
{
	// Treat any held NPCs as neutral
	if ( pTarget == m_hGrenade )
	{
		return D_NU;
	}

	return BaseClass::IRelationType( pTarget );
}
#endif

int CNPC_Zombine::SelectSchedule( void )
{
	if ( GetHealth() <= 0 )
		return BaseClass::SelectSchedule();

	if ( HasCondition( COND_ZOMBINE_GRENADE ) )
	{
		ClearCondition( COND_ZOMBINE_GRENADE );
		
		return SCHED_ZOMBINE_PULL_GRENADE;
	}

	return BaseClass::SelectSchedule();
}

void CNPC_Zombine::BuildScheduleTestBits( void )
{
	BaseClass::BuildScheduleTestBits();

	SetCustomInterruptCondition( COND_ZOMBINE_GRENADE );
}

Activity CNPC_Zombine::NPC_TranslateActivity( Activity baseAct )
{
	if ( baseAct == ACT_MELEE_ATTACK1 )
	{
		if ( m_flSuperFastAttackTime > gpGlobals->curtime || HasGrenade() )
		{
			return (Activity)ACT_ZOMBINE_ATTACK_FAST;
		}
	}

	if ( baseAct == ACT_IDLE )
	{
		if ( HasGrenade() )
		{
			return (Activity)ACT_ZOMBINE_GRENADE_IDLE;
		}
	}

	return BaseClass::NPC_TranslateActivity( baseAct );
}

int CNPC_Zombine::MeleeAttack1Conditions ( float flDot, float flDist )
{

#ifdef EZ
	int iBase;
	if ( GetActiveWeapon() && GetActiveWeapon()->IsMeleeWeapon() )
	{
		iBase = GetActiveWeapon()->WeaponMeleeAttack1Condition( flDot, flDist );
	}
	else
	{
		iBase = BaseClass::MeleeAttack1Conditions( flDot, flDist );
	}
#else
	int iBase = BaseClass::MeleeAttack1Conditions( flDot, flDist );
#endif

	if( HasGrenade() )
	{
		//Adrian: stop spriting if we get close enough to melee and we have a grenade
		//this gives NPCs time to move away from you (before it was almost impossible cause of the high sprint speed)
		if ( iBase == COND_CAN_MELEE_ATTACK1 
#ifdef EZ
			&& m_tEzVariant != EZ_VARIANT_RAD // EZ2 Glowing zombine keep running
#endif

		)
		{
			StopSprint();
		}
	}

	return iBase;
}

void CNPC_Zombine::GatherGrenadeConditions( void )
{
	if ( m_iGrenadeCount <= 0 )
		return;

	if ( g_flZombineGrenadeTimes > gpGlobals->curtime )
		return;

	if ( m_flGrenadePullTime > gpGlobals->curtime )
		return;

	if ( m_flSuperFastAttackTime >= gpGlobals->curtime )
		return;
	
	if ( HasGrenade() )
		return;

	if ( GetEnemy() == NULL )
		return;

	if ( FVisible( GetEnemy() ) == false )
		return;

	if ( IsSprinting() )
		return;

	if ( IsOnFire() )
		return;
	
	if ( IsRunningDynamicInteraction() == true )
		return;

	if ( m_ActBusyBehavior.IsActive() )
		return;

	CBasePlayer *pPlayer = AI_GetSinglePlayer();

	if ( pPlayer && pPlayer->FVisible( this ) )
	{
		float flLengthToPlayer = (pPlayer->GetAbsOrigin() - GetAbsOrigin()).Length();
		float flLengthToEnemy = flLengthToPlayer;

		if ( pPlayer != GetEnemy() )
		{
			flLengthToEnemy = ( GetEnemy()->GetAbsOrigin() - GetAbsOrigin()).Length();
		}

		if ( flLengthToPlayer <= GRENADE_PULL_MAX_DISTANCE && flLengthToEnemy <= GRENADE_PULL_MAX_DISTANCE )
		{
			float flPullChance = 1.0f - ( flLengthToEnemy / GRENADE_PULL_MAX_DISTANCE );
			m_flGrenadePullTime = gpGlobals->curtime + 0.5f;

			if ( flPullChance >= random->RandomFloat( 0.0f, 1.0f ) )
			{
				g_flZombineGrenadeTimes = gpGlobals->curtime + 10.0f;
				SetCondition( COND_ZOMBINE_GRENADE );
			}
		}
	}
}

int CNPC_Zombine::TranslateSchedule( int scheduleType ) 
{
#ifdef EZ
	// Even when equipped with a weapon, zombine chase enemies instead of moving to weapon range
	if ( scheduleType == SCHED_MOVE_TO_WEAPON_RANGE )
		return SCHED_CHASE_ENEMY;
#endif

	return BaseClass::TranslateSchedule( scheduleType );
}

void CNPC_Zombine::DropGrenade( Vector vDir )
{
	if ( m_hGrenade == NULL )
		 return;

	m_hGrenade->SetParent( NULL );
	m_hGrenade->SetOwnerEntity( NULL );

	Vector vGunPos;
	QAngle angles;
	GetAttachment( "grenade_attachment", vGunPos, angles );

	IPhysicsObject *pPhysObj = m_hGrenade->VPhysicsGetObject();

	if ( pPhysObj == NULL )
	{
		m_hGrenade->SetMoveType( MOVETYPE_VPHYSICS );
		m_hGrenade->SetSolid( SOLID_VPHYSICS );
		m_hGrenade->SetCollisionGroup( COLLISION_GROUP_WEAPON );

		m_hGrenade->CreateVPhysics();
	}

	if ( pPhysObj )
	{
		pPhysObj->Wake();
		pPhysObj->SetPosition( vGunPos, angles, true );
		pPhysObj->ApplyForceCenter( vDir * 0.2f );

		pPhysObj->RecheckCollisionFilter();
	}

	m_hGrenade = NULL;
}

void CNPC_Zombine::Event_Killed( const CTakeDamageInfo &info )
{
	BaseClass::Event_Killed( info );

	if ( HasGrenade() )
	{
		DropGrenade( vec3_origin );
	}
}

//-----------------------------------------------------------------------------
// Purpose:  This is a generic function (to be implemented by sub-classes) to
//			 handle specific interactions between different types of characters
//			 (For example the barnacle grabbing an NPC)
// Input  :  Constant for the type of interaction
// Output :	 true  - if sub-class has a response for the interaction
//			 false - if sub-class has no response
//-----------------------------------------------------------------------------
bool CNPC_Zombine::HandleInteraction( int interactionType, void *data, CBaseCombatCharacter *sourceEnt )
{
	if ( interactionType == g_interactionBarnacleVictimGrab )
	{
		if ( HasGrenade() )
		{
			DropGrenade( vec3_origin );
		}
	}
#ifdef EZ2
	else if ( interactionType == g_interactionXenGrenadeHop )
	{
		if ( HasGrenade() )
		{
			DropGrenade( vec3_origin );
		}
	}
#endif

	return BaseClass::HandleInteraction( interactionType, data, sourceEnt );
}

void CNPC_Zombine::TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr, CDmgAccumulator *pAccumulator )
{
	BaseClass::TraceAttack( info, vecDir, ptr, pAccumulator );

	//Only knock grenades off their hands if it's a player doing the damage.
	if ( info.GetAttacker() && info.GetAttacker()->IsNPC() )
		return;

	if ( info.GetDamageType() & ( DMG_BULLET | DMG_CLUB ) )
	{
		if ( ptr->hitgroup == HITGROUP_LEFTARM )
		{
			if ( HasGrenade() )
			{
				DropGrenade( info.GetDamageForce() );
				StopSprint();
			}
		}
	}
}

void CNPC_Zombine::HandleAnimEvent( animevent_t *pEvent )
{
	if ( pEvent->event == AE_ZOMBINE_PULLPIN )
	{
		Vector vecStart;
		QAngle angles;
		GetAttachment( "grenade_attachment", vecStart, angles );

#ifdef EZ
		CBaseEntity *pGrenade = NULL;

		if (m_iszGrenadeType == NULL_STRING)
		{
			ChooseDefaultGrenadeType();
		}

		if (m_iszGrenadeType == gm_iszZombineGrenadeTypeFrag)
		{
			pGrenade = Fraggrenade_Create( vecStart, vec3_angle, vec3_origin, AngularImpulse( 0, 0, 0 ), this, 3.5f, true );
		}
#ifdef EZ2
		else if (m_iszGrenadeType == gm_iszZombineGrenadeTypeXen)
		{
			// Glowbines pull Xen grenades. Yikes!
			pGrenade = HopWire_Create( vecStart, vec3_angle, vec3_origin, AngularImpulse( 0, 0, 0 ), this, 3.5f );
		}
#endif
		else if (m_iszGrenadeType == gm_iszZombineGrenadeTypeStunstick)
		{
			EquipWeapon();
			return;
		}
		else
		{
			const char * pGrenadeClass = STRING( m_iszGrenadeType );

			DevMsg( "npc_zombine is using generic handling for grenade type: %s\n", pGrenadeClass );

			pGrenade = CBaseEntity::CreateNoSpawn( pGrenadeClass, vecStart, vec3_angle, this );
			pGrenade->SetEZVariant( m_tEzVariant );

			DispatchSpawn( pGrenade );

			if(pGrenade)
			pGrenade->SetOwnerEntity( this );
		}

#else
		CBaseGrenade *pGrenade = Fraggrenade_Create( vecStart, vec3_angle, vec3_origin, AngularImpulse( 0, 0, 0 ), this, 3.5f, true );
#endif

		if ( pGrenade )
		{
#ifdef EZ
			pGrenade->DispatchInteraction( g_interactionZombinePullGrenade, NULL, this );
#endif

			// Move physobject to shadow
			IPhysicsObject *pPhysicsObject = pGrenade->VPhysicsGetObject();

			if ( pPhysicsObject )
			{
				pGrenade->VPhysicsDestroyObject();

				int iAttachment = LookupAttachment( "grenade_attachment");

				pGrenade->SetMoveType( MOVETYPE_NONE );
				pGrenade->SetSolid( SOLID_NONE );
				pGrenade->SetCollisionGroup( COLLISION_GROUP_DEBRIS );

				pGrenade->SetAbsOrigin( vecStart );
				pGrenade->SetAbsAngles( angles );

				pGrenade->SetParent( this, iAttachment );

				pGrenade->SetDamage( 200.0f );
#ifdef MAPBASE
				m_OnGrenade.Set(pGrenade, pGrenade, this);
#endif
				m_hGrenade = pGrenade;
				
#ifdef EZ
				ReadyGrenadeSound();
#else
				EmitSound( "Zombine.ReadyGrenade" );
#endif
				// Tell player allies nearby to regard me!
				CAI_BaseNPC **ppAIs = g_AI_Manager.AccessAIs();
				CAI_BaseNPC *pNPC;
				for ( int i = 0; i < g_AI_Manager.NumAIs(); i++ )
				{
					pNPC = ppAIs[i];
#ifndef EZ
					if( pNPC->Classify() == CLASS_PLAYER_ALLY || ( pNPC->Classify() == CLASS_PLAYER_ALLY_VITAL && pNPC->FVisible(this) ) )
#else
					// Citizens and Combine prioritize zombine with grenades pulled
					if( pNPC->Classify() == CLASS_PLAYER_ALLY || pNPC->Classify() == CLASS_COMBINE || (pNPC->Classify() == CLASS_PLAYER_ALLY_VITAL && pNPC->FVisible( this ) ) )
#endif
					{
						int priority;
						Disposition_t disposition;

						priority = pNPC->IRelationPriority(this);
						disposition = pNPC->IRelationType(this);

						pNPC->AddEntityRelationship( this, disposition, priority + 1 );
					}
				}
			}

			m_iGrenadeCount--;
		}
		else
		{
			DevMsg( "Warning: Zombine tried to pull grenade but grenade is null!\n" );
		}

		return;
	}

	if ( pEvent->event == AE_NPC_ATTACK_BROADCAST )
	{
		if ( HasGrenade() )
			return;
	}

	BaseClass::HandleAnimEvent( pEvent );
}

#ifdef EZ
void CNPC_Zombine::ChooseDefaultGrenadeType()
{
	if (m_tEzVariant == EZ_VARIANT_RAD)
	{
		m_iszGrenadeType = gm_iszZombineGrenadeTypeXen;
	}
	else
	{
		m_iszGrenadeType = gm_iszZombineGrenadeTypeFrag;
	}
}
#endif

bool CNPC_Zombine::AllowedToSprint( void )
{
	if ( IsOnFire() )
		return false;
	
	//If you're sprinting then there's no reason to sprint again.
	if ( IsSprinting() )
		return false;

	int iChance = SPRINT_CHANCE_VALUE;

	CHL2_Player *pPlayer = dynamic_cast <CHL2_Player*> ( AI_GetSinglePlayer() );

	if ( pPlayer )
	{
		if ( HL2GameRules()->IsAlyxInDarknessMode() && pPlayer->FlashlightIsOn() == false )
		{
			iChance = SPRINT_CHANCE_VALUE_DARKNESS;
		}

		//Bigger chance of this happening if the player is not looking at the zombie
		if ( pPlayer->FInViewCone( this ) == false )
		{
			iChance *= 2;
		}
	}

	if ( HasGrenade() ) 
	{
		iChance *= 4;
	}

	//Below 25% health they'll always sprint
	if ( ( GetHealth() > GetMaxHealth() * 0.5f ) 
#ifdef EZ
		// EZ2 Glowing zombine always run
		||  m_tEzVariant == EZ_VARIANT_RAD
#endif
	)
	{
		if ( IsStrategySlotRangeOccupied( SQUAD_SLOT_ZOMBINE_SPRINT1, SQUAD_SLOT_ZOMBINE_SPRINT2 ) == true )
			return false;
		
		if ( random->RandomInt( 0, 100 ) > iChance )
			return false;
		
		if ( m_flSprintRestTime > gpGlobals->curtime )
			return false;
	}

	float flLength = ( GetEnemy()->WorldSpaceCenter() - WorldSpaceCenter() ).Length();

	if ( flLength > MAX_SPRINT_DISTANCE )
		return false;

	return true;
}

void CNPC_Zombine::StopSprint( void )
{
	GetNavigator()->SetMovementActivity( ACT_WALK );

	m_flSprintTime = gpGlobals->curtime;
	m_flSprintRestTime = m_flSprintTime + random->RandomFloat( 2.5f, 5.0f );
}

void CNPC_Zombine::Sprint( bool bMadSprint )
{
	if ( IsSprinting() )
		return;

	OccupyStrategySlotRange( SQUAD_SLOT_ZOMBINE_SPRINT1, SQUAD_SLOT_ZOMBINE_SPRINT2 );
	GetNavigator()->SetMovementActivity( ACT_RUN );

	float flSprintTime = random->RandomFloat( MIN_SPRINT_TIME, MAX_SPRINT_TIME );

	//If holding a grenade then sprint until it blows up.
	if ( HasGrenade() || bMadSprint == true )
	{
		flSprintTime = 9999;
	}

	m_flSprintTime = gpGlobals->curtime + flSprintTime;

	//Don't sprint for this long after I'm done with this sprint run.
	m_flSprintRestTime = m_flSprintTime + random->RandomFloat( 2.5f, 5.0f );

#ifdef EZ
	ChargeSound();
#else
	EmitSound( "Zombine.Charge" );
#endif
}

void CNPC_Zombine::RunTask( const Task_t *pTask )
{
	switch ( pTask->iTask )
	{
		case TASK_WAIT_FOR_MOVEMENT_STEP:
		case TASK_WAIT_FOR_MOVEMENT:
		{
			BaseClass::RunTask( pTask );

			if ( IsOnFire() && IsSprinting() )
			{
				StopSprint();
			}

			//Only do this if I have an enemy
			if ( GetEnemy() )
			{
				if ( AllowedToSprint() == true )
				{
					Sprint( ( GetHealth() <= GetMaxHealth() * 0.5f ) );
					return;
				}

				if ( HasGrenade() )
				{
					if ( IsSprinting() )
					{
						GetNavigator()->SetMovementActivity( (Activity)ACT_ZOMBINE_GRENADE_RUN );
					}
					else
					{
						GetNavigator()->SetMovementActivity( (Activity)ACT_ZOMBINE_GRENADE_WALK );
					}

					return;
				}

				if ( GetNavigator()->GetMovementActivity() != ACT_WALK )
				{
					if ( IsSprinting() == false )
					{
						GetNavigator()->SetMovementActivity( ACT_WALK );
					}
				}
			}
			else
			{
				GetNavigator()->SetMovementActivity( ACT_WALK );
			}
		
			break;
		}
		default:
		{
			BaseClass::RunTask( pTask );
			break;
		}
	}
}

void CNPC_Zombine::InputStartSprint ( inputdata_t &inputdata )
{
	Sprint();
}

void CNPC_Zombine::InputPullGrenade ( inputdata_t &inputdata )
{
	g_flZombineGrenadeTimes = gpGlobals->curtime + 5.0f;
	SetCondition( COND_ZOMBINE_GRENADE );
}

//-----------------------------------------------------------------------------
// Purpose: Returns a moan sound for this class of zombie.
//-----------------------------------------------------------------------------
const char *CNPC_Zombine::GetMoanSound( int nSound )
{
	return pMoanSounds[ nSound % ARRAYSIZE( pMoanSounds ) ];
}

//-----------------------------------------------------------------------------
// Purpose: Sound of a footstep
//-----------------------------------------------------------------------------
void CNPC_Zombine::FootstepSound( bool fRightFoot )
{
#ifdef EZ
	if (m_tEzVariant == EZ_VARIANT_RAD && fRightFoot)
	{
		EmitSound( "Glowbie.FootstepRight" );
	}
	else if (m_tEzVariant == EZ_VARIANT_RAD)
	{
		EmitSound( "Glowbie.FootstepLeft" );
	}
	if (m_tEzVariant == EZ_VARIANT_XEN && fRightFoot)
	{
		EmitSound( "Xenbie.FootstepRight" );
	}
	else if (m_tEzVariant == EZ_VARIANT_XEN)
	{
		EmitSound( "Xenbie.FootstepLeft" );
	}
	else
#endif
	if( fRightFoot )
	{
		EmitSound( "Zombie.FootstepRight" );
	}
	else
	{
		EmitSound( "Zombie.FootstepLeft" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Overloaded so that explosions don't split the zombine in twain.
//-----------------------------------------------------------------------------

bool CNPC_Zombine::ShouldBecomeTorso( const CTakeDamageInfo &info, float flDamageThreshold )
{
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Sound of a foot sliding/scraping
//-----------------------------------------------------------------------------
void CNPC_Zombine::FootscuffSound( bool fRightFoot )
{
#ifdef EZ
	if (m_tEzVariant == EZ_VARIANT_RAD && fRightFoot)
	{
		EmitSound( "Glowbine.ScuffRight" );
	}
	else if (m_tEzVariant == EZ_VARIANT_RAD)
	{
		EmitSound( "Glowbine.ScuffLeft" );
	}
	if (m_tEzVariant == EZ_VARIANT_XEN && fRightFoot)
	{
		EmitSound( "Xenbine.ScuffRight" );
	}
	else if (m_tEzVariant == EZ_VARIANT_XEN)
	{
		EmitSound( "Xenbine.ScuffLeft" );
	}
	else
#endif
	if( fRightFoot )
	{
		EmitSound( "Zombine.ScuffRight" );
	}
	else
	{
		EmitSound( "Zombine.ScuffLeft" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Play a random attack hit sound
//-----------------------------------------------------------------------------
void CNPC_Zombine::AttackHitSound( void )
{
#ifdef EZ
	switch ( m_tEzVariant )
	{
	case EZ_VARIANT_RAD:
		EmitSound( "Glowbie.AttackHit" );
		break;
	case EZ_VARIANT_XEN:
		EmitSound( "Xenbie.AttackHit" );
		break;
	default:
		EmitSound( "Zombie.AttackHit" );
		break;
	}
#else
	EmitSound( "Zombie.AttackHit" );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Play a random attack miss sound
//-----------------------------------------------------------------------------
void CNPC_Zombine::AttackMissSound( void )
{
#ifdef EZ
	switch ( m_tEzVariant )
	{
	case EZ_VARIANT_RAD:
		EmitSound( "Glowbie.AttackMiss" );
		break;
	case EZ_VARIANT_XEN:
		EmitSound( "Xenbie.AttackMiss" );
		break;
	default:
		EmitSound( "Zombie.AttackMiss" );
		break;
	}
#else
	// Play a random attack miss sound
	EmitSound( "Zombie.AttackMiss" );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Zombine::PainSound( const CTakeDamageInfo &info )
{
	// We're constantly taking damage when we are on fire. Don't make all those noises!
	if ( IsOnFire() )
	{
		return;
	}

#ifdef EZ
	switch ( m_tEzVariant )
	{
	case EZ_VARIANT_RAD:
		EmitSound( "Glowbine.Pain" );
		break;
	case EZ_VARIANT_XEN:
		EmitSound( "Xenbine.Pain" );
		break;
	default:
		EmitSound( "Zombine.Pain" );
		break;
	}
#else
	EmitSound( "Zombine.Pain" );
#endif
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Zombine::DeathSound( const CTakeDamageInfo &info ) 
{
#ifdef EZ
	switch ( m_tEzVariant )
	{
	case EZ_VARIANT_RAD:
		EmitSound( "Glowbine.Die" );
		break;
	case EZ_VARIANT_XEN:
		EmitSound( "Xenbine.Die" );
		break;
	default:
		EmitSound( "Zombine.Die" );
		break;
	}
#else
	EmitSound( "Zombine.Die" );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Zombine::AlertSound( void )
{
#ifdef EZ
	switch ( m_tEzVariant )
	{
	case EZ_VARIANT_RAD:
		EmitSound( "Glowbine.Alert" );
		break;
	case EZ_VARIANT_XEN:
		EmitSound( "Xenbine.Alert" );
		break;
	default:
		EmitSound( "Zombine.Alert" );
		break;
	}
#else
	EmitSound( "Zombine.Alert" );
#endif
	// Don't let a moan sound cut off the alert sound.
	m_flNextMoanSound += random->RandomFloat( 2.0, 4.0 );
}

//-----------------------------------------------------------------------------
// Purpose: Play a random idle sound.
//-----------------------------------------------------------------------------
void CNPC_Zombine::IdleSound( void )
{
	if( GetState() == NPC_STATE_IDLE && random->RandomFloat( 0, 1 ) == 0 )
	{
		// Moan infrequently in IDLE state.
		return;
	}

	if( IsSlumped() )
	{
		// Sleeping zombies are quiet.
		return;
	}

#ifdef EZ
	switch ( m_tEzVariant )
	{
	case EZ_VARIANT_RAD:
		EmitSound( "Glowbine.Idle" );
		break;
	case EZ_VARIANT_XEN:
		EmitSound( "Xenbine.Idle" );
		break;
	default:
		EmitSound( "Zombine.Idle" );
		break;
	}
#else
	EmitSound( "Zombine.Idle" );
#endif
	MakeAISpookySound( 360.0f );
}

//-----------------------------------------------------------------------------
// Purpose: Play a random attack sound.
//-----------------------------------------------------------------------------
void CNPC_Zombine::AttackSound( void )
{
	
}
#ifdef EZ
//-----------------------------------------------------------------------------
// Purpose: Play a sound while charging towards the enemy
//-----------------------------------------------------------------------------
void CNPC_Zombine::ChargeSound( void )
{
#ifdef EZ
	switch ( m_tEzVariant )
	{
	case EZ_VARIANT_RAD:
		EmitSound( "Glowbine.Charge" );
		break;
	case EZ_VARIANT_XEN:
		EmitSound( "Xenbine.Charge" );
		break;
	default:
		EmitSound( "Zombine.Charge" );
		break;
	}
#else
	EmitSound( "Zombine.Charge" );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Play a sound while readying a grenade
//-----------------------------------------------------------------------------
void CNPC_Zombine::ReadyGrenadeSound( void )
{
#ifdef EZ
	switch ( m_tEzVariant )
	{
	case EZ_VARIANT_RAD:
		EmitSound( "Glowbine.ReadyGrenade" );
		break;
	case EZ_VARIANT_XEN:
		EmitSound( "Xenbine.ReadyGrenade" );
		break;
	default:
		EmitSound( "Zombine.ReadyGrenade" );
		break;
	}
#else
	EmitSound( "Zombine.ReadyGrenade" );
#endif
}
#endif
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
const char *CNPC_Zombine::GetHeadcrabModel( void )
{
	switch ( m_tEzVariant )
	{
		case EZ_VARIANT_RAD:
			return "models/glowcrabclassic.mdl";
		default:
			return "models/headcrabclassic.mdl";
	}
}

#ifdef EZ
CBaseEntity * CNPC_Zombine::ClawAttack( float flDist, int iDamage, QAngle & qaViewPunch, Vector & vecVelocityPunch, int BloodOrigin )
{
	if ( GetActiveWeapon() && GetActiveWeapon()->IsMeleeWeapon() )
	{
		animevent_t * pEvent = new animevent_t();
		pEvent->event = EVENT_WEAPON_MELEE_HIT;

		// If this is a right hand claw attack or both, check weapons
		switch (BloodOrigin)
		{
		case ZOMBIE_BLOOD_RIGHT_HAND:
		case ZOMBIE_BLOOD_BOTH_HANDS:
			GetActiveWeapon()->Operator_HandleAnimEvent( pEvent, this );

			if (BloodOrigin == ZOMBIE_BLOOD_RIGHT_HAND)
				return NULL;
		}
	}

	return BaseClass::ClawAttack(flDist, iDamage, qaViewPunch, vecVelocityPunch, BloodOrigin);
}
#endif

#ifndef EZ
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CNPC_Zombine::GetLegsModel( void )
{
	return "models/zombie/zombie_soldier_legs.mdl";
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
const char *CNPC_Zombine::GetTorsoModel( void )
{
	return "models/zombie/zombie_soldier_torso.mdl";
}
#endif
//---------------------------------------------------------
// Classic zombie only uses moan sound if on fire.
//---------------------------------------------------------
void CNPC_Zombine::MoanSound( envelopePoint_t *pEnvelope, int iEnvelopeSize )
{
	if( IsOnFire() )
	{
		BaseClass::MoanSound( pEnvelope, iEnvelopeSize );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns the classname (ie "npc_headcrab") to spawn when our headcrab bails.
//-----------------------------------------------------------------------------
const char *CNPC_Zombine::GetHeadcrabClassname( void )
{
	return "npc_headcrab";
}

void CNPC_Zombine::ReleaseGrenade( Vector vPhysgunPos )
{
	if ( HasGrenade() == false )
		return;

	Vector vDir = vPhysgunPos - m_hGrenade->GetAbsOrigin();
	VectorNormalize( vDir );

	Activity aActivity;

	Vector vForward, vRight;
	GetVectors( &vForward, &vRight, NULL );

	float flDotForward	= DotProduct( vForward, vDir );
	float flDotRight	= DotProduct( vRight, vDir );

	bool bNegativeForward = false;
	bool bNegativeRight = false;

	if ( flDotForward < 0.0f )
	{
		bNegativeForward = true;
		flDotForward = flDotForward * -1;
	}

	if ( flDotRight < 0.0f )
	{
		bNegativeRight = true;
		flDotRight = flDotRight * -1;
	}

	if ( flDotRight > flDotForward )
	{
		if ( bNegativeRight == true )
			aActivity = (Activity)ACT_ZOMBINE_GRENADE_FLINCH_WEST;
		else 
			aActivity = (Activity)ACT_ZOMBINE_GRENADE_FLINCH_EAST;
	}
	else
	{
		if ( bNegativeForward == true )
			aActivity = (Activity)ACT_ZOMBINE_GRENADE_FLINCH_BACK;
		else 
			aActivity = (Activity)ACT_ZOMBINE_GRENADE_FLINCH_FRONT;
	}

	AddGesture( aActivity );

	DropGrenade( vec3_origin );

	if ( IsSprinting() )
	{
		StopSprint();
	}
	else
	{
		Sprint();
	}
}

CBaseEntity *CNPC_Zombine::OnFailedPhysGunPickup( Vector vPhysgunPos )
{
	CBaseEntity *pGrenade = m_hGrenade;
	ReleaseGrenade( vPhysgunPos );
	return pGrenade;
}

#ifdef EZ2
void CNPC_MetroZombie::Spawn( void )
{
	BaseClass::Spawn();

	m_iHealth = sk_zombiecop_health.GetFloat();
	SetMaxHealth( m_iHealth );
}

void CNPC_MetroZombie::Precache( void )
{
	AllocPooledStringsForGrenadeTypes();

	char * modelVariant;
	switch (m_tEzVariant)
	{
	case EZ_VARIANT_RAD:
		modelVariant = "glowbie";
		PrecacheScriptSound( "Glowbie.FootstepRight" );
		PrecacheScriptSound( "Glowbie.FootstepLeft" );
		PrecacheScriptSound( "Glowbiecop.ScuffRight" );
		PrecacheScriptSound( "Glowbiecop.ScuffLeft" );
		PrecacheScriptSound( "Glowbie.AttackHit" );
		PrecacheScriptSound( "Glowbie.AttackMiss" );
		PrecacheScriptSound( "Glowbiecop.Pain" );
		PrecacheScriptSound( "Glowbiecop.Die" );
		PrecacheScriptSound( "Glowbiecop.Alert" );
		PrecacheScriptSound( "Glowbiecop.Idle" );
		PrecacheScriptSound( "Glowbiecop.ReadyGrenade" );

		PrecacheScriptSound( "ATV_engine_null" );
		PrecacheScriptSound( "Glowbiecop.Charge" );
		PrecacheScriptSound( "Glowbie.Attack" );
		break;
	case EZ_VARIANT_XEN:
		modelVariant = "xenbie";
		PrecacheScriptSound( "Xenbie.FootstepRight" );
		PrecacheScriptSound( "Xenbie.FootstepLeft" );
		PrecacheScriptSound( "Xenbiecop.ScuffRight" );
		PrecacheScriptSound( "Xenbiecop.ScuffLeft" );
		PrecacheScriptSound( "Xenbie.AttackHit" );
		PrecacheScriptSound( "Xenbie.AttackMiss" );
		PrecacheScriptSound( "Xenbiecop.Pain" );
		PrecacheScriptSound( "Xenbiecop.Die" );
		PrecacheScriptSound( "Xenbiecop.Alert" );
		PrecacheScriptSound( "Xenbiecop.Idle" );
		PrecacheScriptSound( "Xenbiecop.ReadyGrenade" );

		PrecacheScriptSound( "ATV_engine_null" );
		PrecacheScriptSound( "Xenbiecop.Charge" );
		PrecacheScriptSound( "Xenbie.Attack" );
		break;
	default:
		modelVariant = "zombie";
		PrecacheScriptSound( "Zombie.FootstepRight" );
		PrecacheScriptSound( "Zombie.FootstepLeft" );
		PrecacheScriptSound( "Zombiecop.ScuffRight" );
		PrecacheScriptSound( "Zombiecop.ScuffLeft" );
		PrecacheScriptSound( "Zombie.AttackHit" );
		PrecacheScriptSound( "Zombie.AttackMiss" );
		PrecacheScriptSound( "Zombiecop.Pain" );
		PrecacheScriptSound( "Zombiecop.Die" );
		PrecacheScriptSound( "Zombiecop.Alert" );
		PrecacheScriptSound( "Zombiecop.Idle" );
		PrecacheScriptSound( "Zombiecop.ReadyGrenade" );

		PrecacheScriptSound( "ATV_engine_null" );
		PrecacheScriptSound( "Zombiecop.Charge" );
		PrecacheScriptSound( "Zombie.Attack" );
		break;
	}
	if (GetModelName() == NULL_STRING)
	{
		SetModelName( AllocPooledString( UTIL_VarArgs( "models/zombie/%scop.mdl", modelVariant ) ) );
	}
	if (GetTorsoModelName() == NULL_STRING && m_tEzVariant != EZ_VARIANT_XEN)
	{
		SetTorsoModelName( AllocPooledString( UTIL_VarArgs( "models/zombie/%scop_torso.mdl", modelVariant ) ) );
	}
	if (GetLegsModelName() == NULL_STRING && m_tEzVariant != EZ_VARIANT_XEN)
	{
		SetLegsModelName( AllocPooledString( UTIL_VarArgs( "models/zombie/%scop_legs.mdl", modelVariant ) ) );
	}

	PrecacheModel( STRING( GetModelName() ) );

	BaseClass::Precache();
}

void CNPC_MetroZombie::ChooseDefaultGrenadeType()
{
	int chance = random->RandomInt( 0, 2 );

	switch (chance)
	{
	case 0:
		m_iszGrenadeType = gm_iszZombineGrenadeTypeFrag;
		break;
	case 1:
		m_iszGrenadeType = gm_iszZombineGrenadeTypeStunstick;
		break;
	default:
		m_iszGrenadeType = gm_iszZombineGrenadeTypeManhack;
		break;
	}
	return;
}

//-----------------------------------------------------------------------------
// Purpose: Sound of a foot sliding/scraping
//-----------------------------------------------------------------------------
void CNPC_MetroZombie::FootscuffSound( bool fRightFoot )
{
	if (m_tEzVariant == EZ_VARIANT_RAD && fRightFoot)
	{
		EmitSound( "Glowbiecop.ScuffRight" );
	}
	else if (m_tEzVariant == EZ_VARIANT_RAD)
	{
		EmitSound( "Glowbiecop.ScuffLeft" );
	}
	if (m_tEzVariant == EZ_VARIANT_XEN && fRightFoot)
	{
		EmitSound( "Xenbiecop.ScuffRight" );
	}
	else if (m_tEzVariant == EZ_VARIANT_XEN)
	{
		EmitSound( "Xenbiecop.ScuffLeft" );
	}
	else if (fRightFoot)
	{
		EmitSound( "Zombiecop.ScuffRight" );
	}
	else
	{
		EmitSound( "Zombiecop.ScuffLeft" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_MetroZombie::PainSound( const CTakeDamageInfo &info )
{
	// We're constantly taking damage when we are on fire. Don't make all those noises!
	if (IsOnFire())
	{
		return;
	}

	switch (m_tEzVariant)
	{
	case EZ_VARIANT_RAD:
		EmitSound( "Glowbiecop.Pain" );
		break;
	case EZ_VARIANT_XEN:
		EmitSound( "Xenbiecop.Pain" );
		break;
	default:
		EmitSound( "Zombiecop.Pain" );
		break;
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_MetroZombie::DeathSound( const CTakeDamageInfo &info )
{
	switch (m_tEzVariant)
	{
	case EZ_VARIANT_RAD:
		EmitSound( "Glowbiecop.Die" );
		break;
	case EZ_VARIANT_XEN:
		EmitSound( "Xenbiecop.Die" );
		break;
	default:
		EmitSound( "Zombiecop.Die" );
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_MetroZombie::AlertSound( void )
{
	switch (m_tEzVariant)
	{
	case EZ_VARIANT_RAD:
		EmitSound( "Glowbiecop.Alert" );
		break;
	case EZ_VARIANT_XEN:
		EmitSound( "Xenbiecop.Alert" );
		break;
	default:
		EmitSound( "Zombiecop.Alert" );
		break;
	}

	// Don't let a moan sound cut off the alert sound.
	m_flNextMoanSound += random->RandomFloat( 2.0, 4.0 );
}

//-----------------------------------------------------------------------------
// Purpose: Play a random idle sound.
//-----------------------------------------------------------------------------
void CNPC_MetroZombie::IdleSound( void )
{
	if (GetState() == NPC_STATE_IDLE && random->RandomFloat( 0, 1 ) == 0)
	{
		// Moan infrequently in IDLE state.
		return;
	}

	if (IsSlumped())
	{
		// Sleeping zombies are quiet.
		return;
	}


	switch (m_tEzVariant)
	{
	case EZ_VARIANT_RAD:
		EmitSound( "Glowbiecop.Idle" );
		break;
	case EZ_VARIANT_XEN:
		EmitSound( "Xenbiecop.Idle" );
		break;
	default:
		EmitSound( "Zombiecop.Idle" );
		break;
	}

	MakeAISpookySound( 360.0f );
}


//-----------------------------------------------------------------------------
// Purpose: Play a sound while charging towards the enemy
//-----------------------------------------------------------------------------
void CNPC_MetroZombie::ChargeSound( void )
{
	switch (m_tEzVariant)
	{
	case EZ_VARIANT_RAD:
		EmitSound( "Glowbiecop.Charge" );
		break;
	case EZ_VARIANT_XEN:
		EmitSound( "Xenbiecop.Charge" );
		break;
	default:
		EmitSound( "Zombiecop.Charge" );
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Play a sound while readying a grenade
//-----------------------------------------------------------------------------
void CNPC_MetroZombie::ReadyGrenadeSound( void )
{
	switch (m_tEzVariant)
	{
	case EZ_VARIANT_RAD:
		EmitSound( "Glowbiecop.ReadyGrenade" );
		break;
	case EZ_VARIANT_XEN:
		EmitSound( "Xenbiecop.ReadyGrenade" );
		break;
	default:
		EmitSound( "Zombiecop.ReadyGrenade" );
		break;
	}
}

void CNPC_HEVZombie::Spawn( void )
{
	BaseClass::Spawn();

	m_iHealth = sk_hevzombie_health.GetFloat();
	SetMaxHealth( m_iHealth );
}

void CNPC_HEVZombie::Precache( void )
{
	AllocPooledStringsForGrenadeTypes();

	char * modelVariant;
	switch (m_tEzVariant)
	{
	case EZ_VARIANT_RAD:
		modelVariant = "glowbie";
		PrecacheScriptSound( "Glowbie.FootstepRight" );
		PrecacheScriptSound( "Glowbie.FootstepLeft" );
		PrecacheScriptSound( "HEVGlowbie.ScuffRight" );
		PrecacheScriptSound( "HEVGlowbie.ScuffLeft" );
		PrecacheScriptSound( "Glowbie.AttackHit" );
		PrecacheScriptSound( "Glowbie.AttackMiss" );
		PrecacheScriptSound( "HEVGlowbie.Pain" );
		PrecacheScriptSound( "HEVGlowbie.Die" );
		PrecacheScriptSound( "HEVGlowbie.Alert" );
		PrecacheScriptSound( "HEVGlowbie.Idle" );
		PrecacheScriptSound( "HEVGlowbie.ReadyGrenade" );
		PrecacheScriptSound( "HEVGlowbie.Vox_Idle" );
		PrecacheScriptSound( "HEVGlowbie.Vox_Alert" );
		PrecacheScriptSound( "HEVGlowbie.Vox_Charge" );
		PrecacheScriptSound( "HEVGlowbie.Vox_Flatline" );
		PrecacheScriptSound( "HEVGlowbie.Vox_Pain" );

		PrecacheScriptSound( "ATV_engine_null" );
		PrecacheScriptSound( "HEVGlowbie.Charge" );
		PrecacheScriptSound( "Glowbie.Attack" );
		break;
	case EZ_VARIANT_XEN:
		modelVariant = "xenbie";
		PrecacheScriptSound( "Xenbie.FootstepRight" );
		PrecacheScriptSound( "Xenbie.FootstepLeft" );
		PrecacheScriptSound( "HEVXenbie.ScuffRight" );
		PrecacheScriptSound( "HEVXenbie.ScuffLeft" );
		PrecacheScriptSound( "Xenbie.AttackHit" );
		PrecacheScriptSound( "Xenbie.AttackMiss" );
		PrecacheScriptSound( "HEVXenbie.Pain" );
		PrecacheScriptSound( "HEVXenbie.Die" );
		PrecacheScriptSound( "HEVXenbie.Alert" );
		PrecacheScriptSound( "HEVXenbie.Idle" );
		PrecacheScriptSound( "HEVXenbie.ReadyGrenade" );
		PrecacheScriptSound( "HEVXenbie.Vox_Idle" );
		PrecacheScriptSound( "HEVXenbie.Vox_Alert" );
		PrecacheScriptSound( "HEVXenbie.Vox_Charge" );
		PrecacheScriptSound( "HEVXenbie.Vox_Flatline" );
		PrecacheScriptSound( "HEVXenbie.Vox_Pain" );

		PrecacheScriptSound( "ATV_engine_null" );
		PrecacheScriptSound( "HEVXenbie.Charge" );
		PrecacheScriptSound( "Xenbie.Attack" );
		break;
	default:
		modelVariant = "zombie";
		PrecacheScriptSound( "Zombie.FootstepRight" );
		PrecacheScriptSound( "Zombie.FootstepLeft" );
		PrecacheScriptSound( "HEVZombie.ScuffRight" );
		PrecacheScriptSound( "HEVZombie.ScuffLeft" );
		PrecacheScriptSound( "Zombie.AttackHit" );
		PrecacheScriptSound( "Zombie.AttackMiss" );
		PrecacheScriptSound( "HEVZombie.Pain" );
		PrecacheScriptSound( "HEVZombie.Die" );
		PrecacheScriptSound( "HEVZombie.Alert" );
		PrecacheScriptSound( "HEVZombie.Idle" );
		PrecacheScriptSound( "HEVZombie.ReadyGrenade" );
		PrecacheScriptSound( "HEVZombie.Vox_Idle" );
		PrecacheScriptSound( "HEVZombie.Vox_Alert" );
		PrecacheScriptSound( "HEVZombie.Vox_Charge" );
		PrecacheScriptSound( "HEVZombie.Vox_Flatline" );
		PrecacheScriptSound( "HEVZombie.Vox_Pain" );

		PrecacheScriptSound( "ATV_engine_null" );
		PrecacheScriptSound( "HEVZombie.Charge" );
		PrecacheScriptSound( "Zombie.Attack" );
		break;
	}
	if (GetModelName() == NULL_STRING)
	{
		SetModelName( AllocPooledString( UTIL_VarArgs( "models/zombie/%s_hev.mdl", modelVariant ) ) );
	}
	if (GetTorsoModelName() == NULL_STRING && m_tEzVariant != EZ_VARIANT_XEN)
	{
		SetTorsoModelName( AllocPooledString( UTIL_VarArgs( "models/zombie/%s_hev_torso.mdl", modelVariant ) ) );
	}
	if (GetLegsModelName() == NULL_STRING && m_tEzVariant != EZ_VARIANT_XEN)
	{
		SetLegsModelName( AllocPooledString( UTIL_VarArgs( "models/zombie/%_hev_legs.mdl", modelVariant ) ) );
	}

	PrecacheModel( STRING( GetModelName() ) );

	BaseClass::Precache();
}

void CNPC_HEVZombie::ChooseDefaultGrenadeType()
{
	int chance = random->RandomInt( 0, 3 );

	// If we set the type to "Arbeit" or "glow", give them Xen grenades
	if ( m_tEzVariant == EZ_VARIANT_ARBEIT || m_tEzVariant == EZ_VARIANT_RAD )
	{
		m_iszGrenadeType = gm_iszZombineGrenadeTypeXen;
	}

	switch (chance)
	{
	case 0:
		m_iszGrenadeType = gm_iszZombineGrenadeTypeCrowbar;
		break;
	// Default to Xen greandes so the grenade type isn't null string, but set the count to 0
	// TODO: I would really like to add an 'electrical grenade' - a battery damaged by the HEV zombie. Out of scope for now, but a nice to have in the future - 1upD
	default:
		m_iszGrenadeType = gm_iszZombineGrenadeTypeXen;
		KeyValue( "NumGrenades", "0" );
		break;
	}
	return;
}

//-----------------------------------------------------------------------------
// Purpose: Sound of a foot sliding/scraping
//-----------------------------------------------------------------------------
void CNPC_HEVZombie::FootscuffSound( bool fRightFoot )
{
	if (m_tEzVariant == EZ_VARIANT_RAD && fRightFoot)
	{
		EmitSound( "HEVGlowbie.ScuffRight" );
	}
	else if (m_tEzVariant == EZ_VARIANT_RAD)
	{
		EmitSound( "HEVGlowbie.ScuffLeft" );
	}
	if (m_tEzVariant == EZ_VARIANT_XEN && fRightFoot)
	{
		EmitSound( "HEVXenbie.ScuffRight" );
	}
	else if (m_tEzVariant == EZ_VARIANT_XEN)
	{
		EmitSound( "HEVXenbie.ScuffLeft" );
	}
	else if (fRightFoot)
	{
		EmitSound( "HEVZombie.ScuffRight" );
	}
	else
	{
		EmitSound( "HEVZombie.ScuffLeft" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_HEVZombie::PainSound( const CTakeDamageInfo &info )
{
	// We're constantly taking damage when we are on fire. Don't make all those noises!
	if (IsOnFire())
	{
		return;
	}

	switch (m_tEzVariant)
	{
	case EZ_VARIANT_RAD:
		EmitSound( "HEVGlowbie.Pain" );
		EmitSound( "HEVGlowbie.Vox_Pain" ); // This soundscript should be on a different channel
		break;
	case EZ_VARIANT_XEN:
		EmitSound( "HEVXenbie.Pain" );
		EmitSound( "HEVXenbie.Vox_Pain" ); // This soundscript should be on a different channel
		break;
	default:
		EmitSound( "HEVZombie.Pain" );
		EmitSound( "HEVZombie.Vox_Pain" ); // This soundscript should be on a different channel
		break;
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_HEVZombie::DeathSound( const CTakeDamageInfo &info )
{
	switch (m_tEzVariant)
	{
	case EZ_VARIANT_RAD:
		EmitSound( "HEVGlowbie.Die" );
		EmitSound( "HEVGlowbie.Vox_Flatline" ); // This soundscript should be on a different channel
		break;
	case EZ_VARIANT_XEN:
		EmitSound( "HEVXenbie.Die" );
		EmitSound( "HEVXenbie.Vox_Flatline" ); // This soundscript should be on a different channel
		break;
	default:
		EmitSound( "HEVZombie.Die" );
		EmitSound( "HEVZombie.Vox_Flatline" ); // This soundscript should be on a different channel
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_HEVZombie::AlertSound( void )
{
	switch (m_tEzVariant)
	{
	case EZ_VARIANT_RAD:
		EmitSound( "HEVGlowbie.Alert" );
		EmitSound( "HEVGlowbie.Vox_Alert" ); // This soundscript should be on a different channel
		break;
	case EZ_VARIANT_XEN:
		EmitSound( "HEVXenbie.Alert" );
		EmitSound( "HEVXenbie.Vox_Alert" ); // This soundscript should be on a different channel
		break;
	default:
		EmitSound( "HEVZombie.Alert" );
		EmitSound( "HEVZombie.Vox_Alert" ); // This soundscript should be on a different channel
		break;
	}

	// Don't let a moan sound cut off the alert sound.
	m_flNextMoanSound += random->RandomFloat( 2.0, 4.0 );
}

//-----------------------------------------------------------------------------
// Purpose: Play a random idle sound.
//-----------------------------------------------------------------------------
void CNPC_HEVZombie::IdleSound( void )
{
	if (GetState() == NPC_STATE_IDLE && random->RandomFloat( 0, 1 ) == 0)
	{
		// Moan infrequently in IDLE state.
		return;
	}

	if (IsSlumped())
	{
		// Sleeping zombies are quiet.
		return;
	}


	switch (m_tEzVariant)
	{
	case EZ_VARIANT_RAD:
		EmitSound( "HEVGlowbie.Idle" );
		EmitSound( "HEVGlowbie.Vox_Idle" ); // This soundscript should be on a different channel
		break;
	case EZ_VARIANT_XEN:
		EmitSound( "HEVXenbie.Idle" );
		EmitSound( "HEVXenbie.Vox_Idle" ); // This soundscript should be on a different channel
		break;
	default:
		EmitSound( "HEVZombie.Idle" );
		EmitSound( "HEVZombie.Vox_Idle" ); // This soundscript should be on a different channel
		break;
	}

	MakeAISpookySound( 360.0f );
}


//-----------------------------------------------------------------------------
// Purpose: Play a sound while charging towards the enemy
//-----------------------------------------------------------------------------
void CNPC_HEVZombie::ChargeSound( void )
{
	switch (m_tEzVariant)
	{
	case EZ_VARIANT_RAD:
		EmitSound( "HEVGlowbie.Charge" );
		EmitSound( "HEVGlowbie.Vox_Charge" ); // This soundscript should be on a different channel
		break;
	case EZ_VARIANT_XEN:
		EmitSound( "HEVXenbie.Charge" );
		EmitSound( "HEVXenbie.Vox_Charge" ); // This soundscript should be on a different channel
		break;
	default:
		EmitSound( "HEVZombie.Charge" );
		EmitSound( "HEVZombie.Vox_Charge" ); // This soundscript should be on a different channel
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Play a sound while readying a grenade
//-----------------------------------------------------------------------------
void CNPC_HEVZombie::ReadyGrenadeSound( void )
{
	switch (m_tEzVariant)
	{
	case EZ_VARIANT_RAD:
		EmitSound( "HEVGlowbie.ReadyGrenade" );
		break;
	case EZ_VARIANT_XEN:
		EmitSound( "HEVXenbie.ReadyGrenade" );
		break;
	default:
		EmitSound( "HEVZombie.ReadyGrenade" );
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: HEV Zombies have suit charge
//-----------------------------------------------------------------------------
int CNPC_HEVZombie::OnTakeDamage( const CTakeDamageInfo &inputInfo )
{
	CTakeDamageInfo info = inputInfo;

	// Armor code taken from CNPC_CloneCop::OnTakeDamage
	if (info.GetDamage() && m_ArmorValue && !(info.GetDamageType() & (DMG_FALL | DMG_DROWN | DMG_POISON | DMG_RADIATION)))// armor doesn't protect against fall or drown damage!
	{
		float flRatio = 0.2f;

		float flNew = info.GetDamage() * flRatio;

		float flArmor;

		flArmor = (info.GetDamage() - flNew);

		if (flArmor < 1.0)
		{
			flArmor = 1.0;
		}

		// Does this use more armor than we have?
		if (flArmor > m_ArmorValue)
		{
			flArmor = m_ArmorValue;
			flNew = info.GetDamage() - flArmor;
			m_ArmorValue = 0;
		}
		else
		{
			m_ArmorValue -= flArmor;
		}

		info.SetDamage( flNew );
	}

	return BaseClass::OnTakeDamage( info );
}

extern ConVar sk_battery; // Need to know how much battery is applied per item

//-----------------------------------------------------------------------------
// Purpose: HEV Zombies should drop a bunch of items on death
// TODO - SHould we be worried about the variant of the item?
//-----------------------------------------------------------------------------
void CNPC_HEVZombie::Event_Killed( const CTakeDamageInfo &info )
{
	BaseClass::Event_Killed( info );

	if (m_bDropItems) {
		int numBatteries = MAX( m_ArmorValue / sk_battery.GetInt(), random->RandomInt( 1, 3 ));
		for (int i = 0; i < numBatteries; i++)
		{
			DropItem( "item_battery", WorldSpaceCenter() + RandomVector( -4, 4 ), RandomAngle( 0, 360 ) );
		}
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_HuskZombine::Spawn( void )
{
	BaseClass::Spawn();

	SetHealth( g_HuskZombineValues_Health[m_tHuskSoldierType]->GetInt() );
	SetMaxHealth( GetHealth() );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_HuskZombine::Precache( void )
{
	char * modelVariant;
	switch (m_tEzVariant)
	{
		case EZ_VARIANT_RAD:
			modelVariant = "glowbie";
			break;
		case EZ_VARIANT_XEN:
			modelVariant = "xenbie";
			break;
		case EZ_VARIANT_ASH:
			modelVariant = "ashbie";
			break;
		default:
			modelVariant = "zombie";
			break;
	}

	if (GetModelName() == NULL_STRING)
	{
		// Default models
		if (m_tHuskSoldierType == HUSK_DEFAULT)
		{
			SetModelName( AllocPooledString( UTIL_VarArgs( g_HuskZombineValues_DefaultModels[HUSK_SOLDIER], modelVariant ) ) );
		}
		else
		{
			SetModelName( AllocPooledString( UTIL_VarArgs( g_HuskZombineValues_DefaultModels[m_tHuskSoldierType], modelVariant ) ) );
		}
	}

	if (m_tHuskSoldierType == HUSK_DEFAULT)
	{
		// This is normally precached in the base class, but we need the index now
		int nModel = PrecacheModel( STRING( GetModelName() ) );
		HuskSoldier_AscertainModelType( this, modelinfo->GetModel( nModel ), m_tHuskSoldierType );
	}

	if (m_tHuskSoldierType == HUSK_CHARGER)
	{
		PrecacheScriptSound( "NPC_HuskCharger.FootstepRight" );
		PrecacheScriptSound( "NPC_HuskCharger.FootstepLeft" );
	}

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_HuskZombine::ChooseDefaultGrenadeType()
{
	if (m_tHuskSoldierType == HUSK_ORDINAL)
	{
		int chance = random->RandomInt( 0, 1 );

		// 50% chance of pulling out a manhack
		switch (chance)
		{
			default:
			case 0:
				m_iszGrenadeType = gm_iszZombineGrenadeTypeFrag;
				break;
			case 1:
				m_iszGrenadeType = gm_iszZombineGrenadeTypeManhack;
				break;
		}
	}
	else if (m_tHuskSoldierType == HUSK_CHARGER)
	{
		m_iszGrenadeType = gm_iszZombineGrenadeTypeStunstick;
	}
	else
	{
		BaseClass::ChooseDefaultGrenadeType();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Sound of a footstep
//-----------------------------------------------------------------------------
void CNPC_HuskZombine::FootstepSound( bool fRightFoot )
{
	if (m_tHuskSoldierType == HUSK_CHARGER)
	{
		if (fRightFoot)
		{
			EmitSound( "NPC_HuskCharger.FootstepRight" );
		}
		else
		{
			EmitSound( "NPC_HuskCharger.FootstepLeft" );
		}
		return;
	}

	BaseClass::FootstepSound( fRightFoot );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CNPC_HuskZombine::ShouldBecomeTorso( const CTakeDamageInfo &info, float flDamageThreshold )
{
	if (!BaseClass::ShouldBecomeTorso( info, flDamageThreshold ))
		return false;

	// For now, only soldiers turn into torsos
	if (m_tHuskSoldierType != HUSK_SOLDIER)
		return false;

	return true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CNPC_HuskZombine::IsChopped( const CTakeDamageInfo &info )
{
	if (!BaseClass::IsChopped( info ))
		return false;

	// For now, only soldiers can be chopped
	if (m_tHuskSoldierType != HUSK_SOLDIER)
		return false;

	return true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
float CNPC_HuskZombine::GetHitgroupDamageMultiplier( int iHitGroup, const CTakeDamageInfo &info )
{
	float flBase = BaseClass::GetHitgroupDamageMultiplier( iHitGroup, info );

	if (m_tHuskSoldierType == HUSK_CHARGER)
	{
		switch (iHitGroup)
		{
		case HITGROUP_HEAD:
		case HITGROUP_CHEST:
		case HITGROUP_STOMACH:
		case HITGROUP_LEFTLEG:
		case HITGROUP_RIGHTLEG:
			{
				flBase *= sk_husk_charger_armor_hitgroup_scale.GetFloat();
			}
		}
	}

	return flBase;
}
#endif

//-----------------------------------------------------------------------------
//
// Schedules
//
//-----------------------------------------------------------------------------

AI_BEGIN_CUSTOM_NPC( npc_zombine, CNPC_Zombine )

#ifdef EZ
	DECLARE_INTERACTION( g_interactionZombinePullGrenade )
#endif

	//Squad slots
	DECLARE_SQUADSLOT( SQUAD_SLOT_ZOMBINE_SPRINT1 )
	DECLARE_SQUADSLOT( SQUAD_SLOT_ZOMBINE_SPRINT2 )

	DECLARE_CONDITION( COND_ZOMBINE_GRENADE )

	DECLARE_ACTIVITY( ACT_ZOMBINE_GRENADE_PULL )
	DECLARE_ACTIVITY( ACT_ZOMBINE_GRENADE_WALK )
	DECLARE_ACTIVITY( ACT_ZOMBINE_GRENADE_RUN )
	DECLARE_ACTIVITY( ACT_ZOMBINE_GRENADE_IDLE )
	DECLARE_ACTIVITY( ACT_ZOMBINE_ATTACK_FAST )
	DECLARE_ACTIVITY( ACT_ZOMBINE_GRENADE_FLINCH_BACK )
	DECLARE_ACTIVITY( ACT_ZOMBINE_GRENADE_FLINCH_FRONT )
	DECLARE_ACTIVITY( ACT_ZOMBINE_GRENADE_FLINCH_WEST)
	DECLARE_ACTIVITY( ACT_ZOMBINE_GRENADE_FLINCH_EAST )

	DECLARE_ANIMEVENT( AE_ZOMBINE_PULLPIN )


	DEFINE_SCHEDULE
	(
	SCHED_ZOMBINE_PULL_GRENADE,

	"	Tasks"
	"		TASK_PLAY_SEQUENCE					ACTIVITY:ACT_ZOMBINE_GRENADE_PULL"


	"	Interrupts"

	)

AI_END_CUSTOM_NPC()



