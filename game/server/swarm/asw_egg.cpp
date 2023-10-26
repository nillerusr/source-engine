#include "cbase.h"
#include "props.h"
#include "asw_egg.h"
#include "asw_shareddefs.h"
#include "asw_marine.h"
#include "asw_marine_resource.h"
#include "asw_fx_shared.h"
#include "asw_parasite.h"
#include "asw_gamerules.h"
#include "asw_mission_manager.h"
#include "asw_util_shared.h"
#include "EntityFlame.h"
#include "te_effect_dispatch.h"
#include "asw_marine_speech.h"
#include "asw_burning.h"
#include "asw_game_resource.h"
#include "asw_player.h"
#include "asw_achievements.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define EGG_MODEL "models/aliens/egg/egg.mdl"
#define EGG_OPEN_ANIM "open"
#define EGG_CLOSED_ANIM "closed"
#define EGG_JIGGLE_ANIM "closed"
#define EGG_OPENING_ANIM "opening"
#define EGG_ON_FIRE_ANIM "fire_open"
#define EGG_HATCH_ANIM "egg_pop"

#define ASW_EGG_HATCH_DELAY 3.0f
#define ASW_EGG_ALWAYS_BURST_DISTANCE 120.0f
#define ASW_EGG_BURST_DISTANCE_EASY 250.0f
#define ASW_EGG_BURST_DISTANCE 450.0f
#define ASW_EGG_RESET_DELAY 20.0f


LINK_ENTITY_TO_CLASS( asw_egg, CASW_Egg );


IMPLEMENT_SERVERCLASS_ST(CASW_Egg, DT_ASW_Egg)
SendPropBool( SENDINFO( m_bOnFire ) ),
SendPropFloat( SENDINFO( m_fEggAwake ) ),
END_SEND_TABLE()

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CASW_Egg )
	DEFINE_THINKFUNC( AnimThink ),
	DEFINE_THINKFUNC( SetupParasiteThink ),	
	DEFINE_FUNCTION( EggTouch ),
	DEFINE_KEYFIELD( m_sParasiteClass,				FIELD_STRING,	"ParasiteClass" ),
	DEFINE_KEYFIELD( m_bFixedJumpDirection,			FIELD_BOOLEAN,	"FixedJumpDir" ),	
	DEFINE_KEYFIELD( m_bSkipEggChatter,				FIELD_BOOLEAN,	"SkipEggChatter" ),
	DEFINE_KEYFIELD( m_bSmallOpenRadius,			FIELD_BOOLEAN,	"SmallOpenRadius" ),
	DEFINE_FIELD(m_fNextMarineCheckTime, FIELD_FLOAT),
	DEFINE_FIELD(m_bOpen, FIELD_BOOLEAN),
	DEFINE_FIELD(m_bOpening, FIELD_BOOLEAN),
	DEFINE_FIELD(m_bHatched, FIELD_BOOLEAN),
	DEFINE_FIELD(m_fHatchTime, FIELD_TIME),
	DEFINE_FIELD(m_bStoredEggSize, FIELD_BOOLEAN),
	DEFINE_FIELD(m_vecStartSurroundMins, FIELD_VECTOR),
	DEFINE_FIELD(m_vecStartSurroundMaxs, FIELD_VECTOR),
	DEFINE_FIELD(m_bMadeParasiteVisible, FIELD_BOOLEAN),
	DEFINE_FIELD(m_hParasite, FIELD_EHANDLE),
	DEFINE_FIELD( m_bOnFire, FIELD_BOOLEAN ),
	DEFINE_FIELD(m_hBurner, FIELD_EHANDLE),
	DEFINE_FIELD(m_hBurnerWeapon, FIELD_EHANDLE),
	DEFINE_FIELD(m_fEggAwake, FIELD_FLOAT),
	DEFINE_FIELD(m_fEggResetTime, FIELD_TIME),

	DEFINE_INPUTFUNC( FIELD_VOID,	"EggOpen",	InputOpen ),
	DEFINE_INPUTFUNC( FIELD_VOID,	"EggHatch",	InputHatch ),

	DEFINE_OUTPUT( m_OnOpened,		"OnEggOpened" ),
	DEFINE_OUTPUT( m_OnHatched,		"OnEggHatched" ),
	DEFINE_OUTPUT( m_OnDestroyed,	"OnEggDestroyed" ),
	DEFINE_OUTPUT( m_OnEggReset,    "OnEggReset" ),
END_DATADESC()

ConVar asw_egg_respawn( "asw_egg_respawn", "0", FCVAR_CHEAT, "If set, eggs will respawn the parasite inside" );

float CASW_Egg::s_fNextSpottedChatterTime = 0;

CASW_Egg::CASW_Egg()
{
	m_bOpen = false;
	m_bOpening = false;
	m_bHatched = false;
	m_bMadeParasiteVisible = false;
	m_hParasite = NULL;
	m_bSmallOpenRadius = false;
	m_hBurner = NULL;
	m_fEggAwake = 0;
	m_fEggResetTime = 0;
}


CASW_Egg::~CASW_Egg()
{
	if (GetParasite())
		GetParasite()->SetEgg(NULL);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_Egg::Spawn( void )
{
	SetMoveType( MOVETYPE_NONE );
	//SetSolid( SOLID_BBOX );
	SetSolid( SOLID_VPHYSICS );
	CreateVPhysics();
	SetCollisionGroup( ASW_COLLISION_GROUP_EGG );

	Precache();
	SetModel(EGG_MODEL);
	ResetSequence( LookupSequence( EGG_CLOSED_ANIM ) );
	SetPlaybackRate( RandomFloat( 0.95, 1.05 ) ); // Slightly randomize the playback rate so they don't all match
	m_bStoredEggSize = false;	

	BaseClass::Spawn();

	AddEFlags( EFL_NO_DISSOLVE | EFL_NO_MEGAPHYSCANNON_RAGDOLL | EFL_NO_PHYSCANNON_INTERACTION );

	//SetCollisionBounds( Vector(-26,-26,0), Vector(26,26,60));
	SetThink( &CASW_Egg::SetupParasiteThink );
	SetNextThink( gpGlobals->curtime + 0.3f );
	SetTouch( &CASW_Egg::EggTouch );

	AddFlag( FL_AIMTARGET );

	// create our parasite to sit inside and await a hapless marine
	QAngle angParasiteFacing = GetAbsAngles();
	if (!m_bFixedJumpDirection)
	{
		angParasiteFacing.y = random->RandomInt(0,360);
	}
	

	m_hParasite = dynamic_cast<CASW_Parasite*>(CreateNoSpawn("asw_parasite", GetAbsOrigin(), angParasiteFacing, this));
	if (GetParasite())
	{
		//Msg("Telling parasite to idle in egg\n");
		GetParasite()->IdleInEgg(true);
		//GetParasite()->AddSpawnFlags(SF_NPC_WAIT_FOR_SCRIPT);
		GetParasite()->Spawn();		
		GetParasite()->SetSleepState(AISS_WAITING_FOR_INPUT);
		GetParasite()->SetEgg(this);

		GetParasite()->SetParent( this );
	}

	m_takedamage = DAMAGE_YES;
	m_iHealth = 50;
	m_iMaxHealth = m_iHealth;
	m_fNextMarineCheckTime = gpGlobals->curtime + random->RandomFloat(5.0f, 10.0f);

	if ( ASWGameResource() && ASWGameResource()->m_iStartingEggsInMap >= 0 )
	{
		ASWGameResource()->m_iStartingEggsInMap++;
	}
}

bool CASW_Egg::CreateVPhysics()
{
	VPhysicsInitStatic();
	return true;
}

void CASW_Egg::Precache()
{
	PrecacheModel(EGG_MODEL);
	PrecacheParticleSystem( "egg_open" );
	PrecacheParticleSystem( "egg_hatch" );
	PrecacheParticleSystem( "egg_death" );

	PrecacheModel ("models/aliens/egg/egggib_1.mdl"); 
	PrecacheModel ("models/aliens/egg/egggib_2.mdl");
	PrecacheModel ("models/aliens/egg/egggib_3.mdl");

	PrecacheScriptSound("ASW_Egg.Open");
	PrecacheScriptSound("ASW_Egg.Gib");
	PrecacheScriptSound("ASW_Parasite.EggBurst");
	BaseClass::Precache();
	UTIL_PrecacheOther( "asw_parasite" );
}


// int CASW_Egg::ShouldTransmit( const CCheckTransmitInfo *pInfo )
// {
// 	return FL_EDICT_ALWAYS;
// }
// 
// int CASW_Egg::UpdateTransmitState()
// {
// 	return SetTransmitState( FL_EDICT_FULLCHECK );
// }

void CASW_Egg::SetupParasiteThink()
{
	if (GetParasite() && GetParasite()->IsEffectActive(EF_NODRAW))
	{
		GetParasite()->RemoveEffects(EF_NODRAW);
		GetParasite()->SetActivity(ACT_IDLE);
		m_bMadeParasiteVisible = true;
		SetNextThink( gpGlobals->curtime + 0.5f );
		return;
	}

	if (!m_bMadeParasiteVisible)
	{
		SetNextThink( gpGlobals->curtime + 0.1f );
		return;
	}

	// if we're done setting up the parasite, turn thinking over to the normal egg processing
	SetThink(&CASW_Egg::AnimThink);
	SetNextThink( gpGlobals->curtime + 0.1f );
}

void CASW_Egg::ReachedEndOfSequence()
{
	if (  ( GetSequence() == LookupSequence( EGG_OPENING_ANIM ) )  )
	{
		ResetSequence( LookupSequence(EGG_OPEN_ANIM) );
		m_bOpen = true;
	}
	if (  ( GetSequence() == LookupSequence( EGG_ON_FIRE_ANIM ) )  )
	{
		m_bOpen = true;
	}
}

void CASW_Egg::AnimThink( void )
{
	if ( m_bOnFire )
	{	
		ResetSequence( LookupSequence( EGG_ON_FIRE_ANIM ) );
	}

	if (m_bOpen && !m_bHatched && gpGlobals->curtime >= m_fHatchTime)
	{
		Hatch(NULL);
	}

	if (!m_bStoredEggSize)
	{
		m_bStoredEggSize = true;
		m_vecStartSurroundMins = CollisionProp()->OBBMins();
		m_vecStartSurroundMaxs = CollisionProp()->OBBMaxs();
		//CollisionProp()->WorldSpaceSurroundingBounds( &m_vecStartSurroundMins, &m_vecStartSurroundMaxs );
		//Msg("Storing %f and %f, %f\n", m_vecStartSurroundMaxs.x, m_vecStartSurroundMaxs.y, m_vecStartSurroundMaxs.z);
	}
	// periodically find the nearest marine and check if we should burst open
	if (!m_bOpen && gpGlobals->curtime >= m_fNextMarineCheckTime)
	{
		float marine_distance = -1;
		CBaseEntity *pMarine = UTIL_ASW_NearestMarine(GetAbsOrigin(), marine_distance );
		if (!m_bSkipEggChatter && pMarine && marine_distance < 500 && gpGlobals->curtime > s_fNextSpottedChatterTime)
		{

			CASW_Marine *pSpottedMarine = UTIL_ASW_Marine_Can_Chatter_Spot(this, 500);
			if (pSpottedMarine)
			{
				pSpottedMarine->GetMarineSpeech()->Chatter(CHATTER_EGGS);
				s_fNextSpottedChatterTime = gpGlobals->curtime + 30.0f;
			}
			else
				s_fNextSpottedChatterTime = gpGlobals->curtime + 1.0f;		
		}
		float flOpenDist = ASW_EGG_BURST_DISTANCE;
		if ( ASWGameRules() && ASWGameRules()->GetSkillLevel() == 1 )
		{
			flOpenDist = ASW_EGG_BURST_DISTANCE_EASY;
		}
		if ( pMarine  )
		{
			//Msg( "Egg %d check.  Distance = %f\n", entindex(), marine_distance );
			if ( marine_distance <= ASW_EGG_ALWAYS_BURST_DISTANCE )
			{
				Open(pMarine);
			}
			else if ( marine_distance <= ASW_EGG_BURST_DISTANCE && RandomFloat() < 0.1f )
			{
				Open(pMarine);
			}
		}
		
		if ( !m_bOpen )
		{
			// rethink interval based on how near the marines are
			if (marine_distance == -1 || marine_distance > 4096)
			{
				m_fNextMarineCheckTime = gpGlobals->curtime + 5.0f;
			}
			else
			{
				if (ASWGameRules() && ASWGameRules()->GetSkillLevel() >= 4 )
				{
					m_fNextMarineCheckTime = gpGlobals->curtime + 1.5f;
				}
				else if (ASWGameRules() && ASWGameRules()->GetSkillLevel() == 3 )
				{
					m_fNextMarineCheckTime = gpGlobals->curtime + 2.0f;
				}
				else
				{
					m_fNextMarineCheckTime = gpGlobals->curtime + 2.4f;
				}
			}
		}
	}

	// fade in the green lines if we're about to wake up
	if ( m_bOpen && !m_bHatched )
	{
		m_fEggAwake = MIN( 1.0f, m_fEggAwake.Get() + gpGlobals->frametime * 3.0f );		
	}
	else
	{
		m_fEggAwake = MAX( 0, m_fEggAwake.Get() - gpGlobals->frametime * 0.5f );		// slow power down

		if ( m_bHatched && gpGlobals->curtime > m_fEggResetTime && m_fEggResetTime > 0)
		{
			ResetEgg();
		}
	}

	SetNextThink( gpGlobals->curtime + 0.1f );
	
	StudioFrameAdvance();
}

void CASW_Egg::ResetEgg()
{
	if ( m_bOnFire )
	{
		return;
	}
	if ( m_bHatched )
	{
		// spawn a new parasite
		QAngle angParasiteFacing = GetAbsAngles();
		if (!m_bFixedJumpDirection)
		{
			angParasiteFacing.y = random->RandomInt(0,360);
		}
	
		m_hParasite = dynamic_cast<CASW_Parasite*>(CreateNoSpawn("asw_parasite", GetAbsOrigin(), angParasiteFacing, this));
		if (GetParasite())
		{
			//Msg("Telling parasite to idle in egg\n");
		
			GetParasite()->IdleInEgg(true);
			//GetParasite()->AddSpawnFlags(SF_NPC_WAIT_FOR_SCRIPT);
			GetParasite()->Spawn();		
			GetParasite()->SetSleepState(AISS_WAITING_FOR_INPUT);
			GetParasite()->SetEgg(this);
			
			GetParasite()->SetParent( this );
		}
	}

	// reset the egg so it can open and hatch again
	m_bOpen = false;
	m_bHatched = false;
	ResetSequence( LookupSequence( EGG_CLOSED_ANIM ) );
	SetBodygroup( 1,0 );
	SetPlaybackRate( RandomFloat( 0.95, 1.05 ) ); // Slightly randomize the playback rate so they don't all match
	m_OnEggReset.FireOutput(this, this);
}

void CASW_Egg::Open(CBaseEntity* pOther)
{
	if ( !m_bOpen && !m_bHatched && !m_bOpening )
	{
		if ( m_bOnFire )
		{
			ResetSequence( LookupSequence ( EGG_ON_FIRE_ANIM ) );	
		}
		else
		{
			ResetSequence( LookupSequence ( EGG_OPENING_ANIM ) );
			SetCycle( 0 );
		}

		SpawnEffects(EGG_FLAG_OPEN);
		CheckEggSize();
		RemoveSolidFlags( FSOLID_NOT_SOLID );		
		//UTIL_ASW_BloodDrips( GetAbsOrigin()+Vector(0,0,50), Vector(0,0,1), BLOOD_COLOR_GREEN, 4 );
		m_fHatchTime = gpGlobals->curtime + (ASW_EGG_HATCH_DELAY * random->RandomFloat(1.0f, 2.0f));
		m_OnOpened.FireOutput(pOther, this);
		EmitSound("ASW_Egg.Open");
		m_bOpening = true;
	}

}

void  CASW_Egg::Hatch(CBaseEntity* pOther)
{
	if (!m_bHatched && m_bOpen)
	{
		SpawnEffects(EGG_FLAG_HATCH);
		SetBodygroup( 1,1 );
		ResetSequence( LookupSequence( EGG_HATCH_ANIM ) );
		m_bHatched = true;
		CheckEggSize();
		RemoveSolidFlags( FSOLID_NOT_SOLID );
		// todo: make some green goo spurt out
		//UTIL_ASW_BloodDrips( GetAbsOrigin()+Vector(0,0,30), Vector(0,0,1), BLOOD_COLOR_GREEN, 4 );
		// todo: spawn the parasite		
		m_OnHatched.FireOutput(pOther, this);
		EmitSound("ASW_Parasite.EggBurst");
		if (GetParasite())
		{
			if (IsOnFire())
			{
				CBaseEntity *pBurner = m_hBurner.Get();
				if (!pBurner)
					pBurner = this;
				GetParasite()->ASW_Ignite( 30, 0, pBurner, m_hBurnerWeapon.Get() );
			}
			else
			{
				if ( ASWGameResource() )
				{
					ASWGameResource()->m_iEggsHatched++;
				}
			}

			GetParasite()->SetJumpFromEgg(true);
			GetParasite()->IdleInEgg(false);
			GetParasite()->Wake();
		}

		if ( asw_egg_respawn.GetBool() )
		{
			m_fEggResetTime = gpGlobals->curtime + ASW_EGG_RESET_DELAY * random->RandomFloat(1.0f, 2.0f);
		}
	}
}

void CASW_Egg::EggTouch(CBaseEntity* pOther)
{
	// egg will open if touched by a marine
	CASW_Marine* pMarine = CASW_Marine::AsMarine( pOther );
	if (pMarine)
	{
		if (!m_bOpen)
			Open(pOther);
	}
}

// inputs

void CASW_Egg::InputOpen( inputdata_t &inputdata )
{
	if (!m_bOpen)
		Open(inputdata.pActivator);
}

void CASW_Egg::InputHatch( inputdata_t &inputdata )
{
	if (!m_bHatched)
	{
		m_bOpen = true; 
		Hatch(inputdata.pActivator);
	}
}

// check the egg hasn't got any bigger in size, otherwise nearby creatures could get stuck in it
void CASW_Egg::CheckEggSize()
{
	Vector vecSurroundMins, vecSurroundMaxs;
	vecSurroundMins = CollisionProp()->OBBMins();
	vecSurroundMaxs = CollisionProp()->OBBMaxs();

	if (vecSurroundMins.x < m_vecStartSurroundMins.x)
		vecSurroundMins.x = m_vecStartSurroundMins.x;
	if (vecSurroundMins.y < m_vecStartSurroundMins.y)
		vecSurroundMins.y = m_vecStartSurroundMins.y;
	if (vecSurroundMins.z < m_vecStartSurroundMins.z)
		vecSurroundMins.z = m_vecStartSurroundMins.z;

	if (vecSurroundMaxs.x > m_vecStartSurroundMaxs.x)
		vecSurroundMaxs.x = m_vecStartSurroundMaxs.x;
	if (vecSurroundMaxs.y > m_vecStartSurroundMaxs.y)
		vecSurroundMaxs.y = m_vecStartSurroundMaxs.y;
	if (vecSurroundMaxs.z > m_vecStartSurroundMaxs.z)
		vecSurroundMaxs.z = m_vecStartSurroundMaxs.z;

	SetCollisionBounds(	vecSurroundMins, vecSurroundMaxs );
}

void CASW_Egg::SpawnEffects(int flags)
{
	UTIL_ASW_EggGibs( WorldSpaceCenter(), flags, entindex() );
}

int CASW_Egg::OnTakeDamage( const CTakeDamageInfo &info )
{
	int result = BaseClass::OnTakeDamage(info);

	if (result > 0)
	{
		CASW_Marine* pMarine = dynamic_cast<CASW_Marine*>(info.GetAttacker());
		if (pMarine)
			pMarine->HurtAlien(this, info);

		if (info.GetDamageType() & DMG_BURN ||
			info.GetDamageType() & DMG_BLAST)
		{
			ASW_Ignite(30.0f, 0, info.GetAttacker(), info.GetWeapon() );
		}
	}
	return result;
}

void CASW_Egg::Event_Killed( const CTakeDamageInfo &info )
{
	// make the egg shatter in however many stages are left
	//if (m_lifeState == LIFE_DEAD)	// already dead?
	//return;
	if (!m_bHatched)
	{
		SpawnEffects(EGG_FLAG_DIE);

		m_bOpen = true;
		CheckEggSize();
		RemoveSolidFlags( FSOLID_NOT_SOLID );
	}
	else
	{
		SpawnEffects(EGG_FLAG_DIE);
	}

	// kill the parasite inside
	if (GetParasite() && !m_bHatched)
	{
		CTakeDamageInfo killsite(info.GetInflictor(), info.GetAttacker(), info.GetDamageForce(), info.GetDamagePosition(), 150,
			info.GetDamageType());
		killsite.SetWeapon( info.GetWeapon() );
		GetParasite()->TakeDamage(killsite);
	}

	if (ASWGameRules() && ASWGameRules()->GetMissionManager())
		ASWGameRules()->GetMissionManager()->EggKilled(this);

	CASW_Marine *pMarine = dynamic_cast<CASW_Marine*>(info.GetAttacker());
	if (pMarine && pMarine->GetMarineResource())
	{
		pMarine->GetMarineResource()->m_iEggKills++;
	}
	if ( ASWGameResource() )
	{
		ASWGameResource()->m_iEggsKilled++;
		if ( ASWGameResource()->m_iStartingEggsInMap > 5 && ASWGameResource()->m_iEggsKilled >= ASWGameResource()->m_iStartingEggsInMap && ASWGameResource()->m_iEggsHatched <= 0 )
		{
			for ( int i = 1; i <= gpGlobals->maxClients; i++ )	
			{
				CASW_Player* pPlayer = dynamic_cast<CASW_Player*>( UTIL_PlayerByIndex( i ) );
				if ( !pPlayer || !pPlayer->IsConnected() || !pPlayer->GetMarine() )
					continue;

				pPlayer->AwardAchievement( ACHIEVEMENT_ASW_EGGS_BEFORE_HATCH );
			}
		}
	}

	m_OnDestroyed.FireOutput(info.GetInflictor(), this);

	BaseClass::Event_Killed(info);

	SetThink(&CASW_Egg::SUB_Remove);
	SetNextThink(gpGlobals->curtime + 0.1f);

	// break the egg
	Vector	velocity;
	velocity = Vector( RandomFloat( 50, 100 ), RandomFloat( 50, 100 ), RandomFloat( 100, 250 ) );
	AngularImpulse	angVelocity = RandomAngularImpulse( -500.0f, 500.0f );
	breakablepropparams_t params( GetAbsOrigin(), GetAbsAngles(), velocity, angVelocity );
	params.impactEnergyScale = 1.0f;
	params.defBurstScale = 100.0f;
	params.defCollisionGroup = COLLISION_GROUP_DEBRIS;
	PropBreakableCreateAll( GetModelIndex(), NULL, params, this, -1, true, true ); 

	EmitSound("ASW_Egg.Gib");
}

void CASW_Egg::ParasiteDied(CASW_Parasite* pParasite)
{
	m_hParasite = NULL;
}

void CASW_Egg::ASW_Ignite( float flFlameLifetime, float flSize, CBaseEntity *pAttacker, CBaseEntity *pDamagingWeapon /*= NULL */ )
{
	if( IsOnFire() )
		return;

	AddFlag( FL_ONFIRE );
	m_bOnFire = true;
	if (ASWBurning())
		ASWBurning()->BurnEntity(this, pAttacker, flFlameLifetime, 0.4f, 5.0f * 0.4f, pDamagingWeapon );	// 5 dps, applied every 0.4 seconds

	m_hBurner = pAttacker;
	m_hBurnerWeapon = pDamagingWeapon;

	m_OnIgnite.FireOutput( this, this );
}

void CASW_Egg::Ignite( float flFlameLifetime, bool bNPCOnly, float flSize, bool bCalledByLevelDesigner )
{
	return;	// use ASW_Ignite instead
}

CASW_Parasite* CASW_Egg::GetParasite()
{
	return dynamic_cast<CASW_Parasite*>(m_hParasite.Get());
}

void CASW_Egg::TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr )
{
	if ( m_takedamage == DAMAGE_NO )
		return;

	CTakeDamageInfo subInfo = info;
	m_nForceBone = ptr->physicsbone;		// save this bone for physics forces

	Assert( m_nForceBone > -255 && m_nForceBone < 256 );	

	if ( subInfo.GetDamage() >= 1.0 && !(subInfo.GetDamageType() & DMG_SHOCK )
		&& !( subInfo.GetDamageType() & DMG_BURN ) )
	{
		Bleed( subInfo, ptr->endpos, vecDir, ptr );
	}

	if( !info.GetInflictor() )
	{
		subInfo.SetInflictor( info.GetAttacker() );
	}

	AddMultiDamage( subInfo, this );
}

void CASW_Egg::Bleed( const CTakeDamageInfo &info, const Vector &vecPos, const Vector &vecDir, trace_t *ptr )
{
	UTIL_ASW_DroneBleed( vecPos, -vecDir, 4 );
}
