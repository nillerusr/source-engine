//=============================================================================//
//
// Purpose:		Monsters like bullsquids lay eggs. This NPC represents
//				an egg that has a physics model and can react to its surroundings.
//
// Author:		1upD
//
//=============================================================================//

#include "cbase.h"
#include "npc_egg.h"
#include "npc_basepredator.h"
#include "particle_parse.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS( npc_egg, CNPC_Egg );

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CNPC_Egg )
DEFINE_KEYFIELD( m_iszAlertSound, FIELD_SOUNDNAME, "AlertSound" ),
DEFINE_KEYFIELD( m_iszHatchSound, FIELD_SOUNDNAME, "HatchSound" ),
DEFINE_KEYFIELD( m_isChildClassname, FIELD_STRING, "ChildClassname" ),
DEFINE_KEYFIELD( m_isHatchParticle, FIELD_STRING, "HatchParticle" ),
DEFINE_KEYFIELD( m_flIncubation, FIELD_FLOAT, "IncubationTime" ),
DEFINE_KEYFIELD( m_BabyModelName, FIELD_MODELNAME, "babymodel" ),
DEFINE_KEYFIELD( m_AdultModelName, FIELD_MODELNAME, "adultmodel" ),

DEFINE_FIELD( m_flNextHatchTime, FIELD_TIME ),

DEFINE_OUTPUT( m_OnSpawnNPC, "OnSpawnNPC" ),

DEFINE_THINKFUNC( HatchThink ),
END_DATADESC()

CNPC_Egg::CNPC_Egg()
{
	// If no health was supplied, use 100
	m_iHealth = 100;

	// If no incubation period specified, set infinite
	m_flIncubation = -1;
}

void CNPC_Egg::Spawn()
{
	Precache();

	SetModel( STRING( GetModelName() ) );

	SetSolid( SOLID_VPHYSICS );
	AddSolidFlags( FSOLID_FORCE_WORLD_ALIGNED | FSOLID_NOT_STANDABLE );

	AddEFlags( EFL_NO_DISSOLVE );
	AddEFlags( EFL_NO_MEGAPHYSCANNON_RAGDOLL );

	SetHullType( HULL_SMALL_CENTERED );

	if ( m_tEzVariant == EZ_VARIANT_RAD )
	{
		SetBloodColor( BLOOD_COLOR_BLUE );
	}
	else
	{
		SetBloodColor( BLOOD_COLOR_GREEN );
	}

	SetRenderColor( 255, 255, 255, 255 );

	m_flFieldOfView = GetFieldOfView();
	m_NPCState = NPC_STATE_NONE;

	CapabilitiesClear();
	CapabilitiesAdd( bits_CAP_SQUAD );

	SetViewOffset( Vector( 0, 0, 64.0f ) );

	NPCInit();

	if ( m_flIncubation > -1 )
	{
		m_flNextHatchTime = gpGlobals->curtime + m_flIncubation;
	}
	else
	{
		m_flNextHatchTime = FLT_MAX;
	}

	// NOTE: This must occur *after* init, since init sets default dist look
	m_flDistTooFar = GetViewDistance();
	SetDistLook( m_flDistTooFar );

	SetContextThink( &CNPC_Egg::HatchThink, m_flNextHatchTime, "HatchThink" );
}

void CNPC_Egg::Precache( void )
{
	if (GetModelName() == NULL_STRING)
	{
		SetModelName( AllocPooledString( "models/eggs/bullsquid_egg.mdl" ) );
	}

	m_bIsRetracted = false;

	if ( m_iszAlertSound == NULL_STRING )
	{
		m_iszAlertSound = AllocPooledString( "npc_bullsquid.egg_alert" );
	}

	PrecacheScriptSound( STRING( m_iszAlertSound ) );

	if ( m_isChildClassname == NULL_STRING )
	{
		m_isChildClassname = AllocPooledString( "npc_bullsquid" );
	}

	UTIL_PrecacheEZVariant( STRING( m_isChildClassname ), m_tEzVariant );

	if ( m_isHatchParticle == NULL_STRING )
	{
		m_isHatchParticle = AllocPooledString( "bullsquid_egg_hatch" );
	}

	if ( m_iszHatchSound == NULL_STRING )
	{
		m_iszHatchSound = AllocPooledString( "npc_bullsquid.egg_hatch" );
	}

	PrecacheScriptSound( STRING( m_iszHatchSound ) );

	// If there's no specified model for babies from this egg, use the adult model
	// The adult model might still be NULL_STRING, so that must be handled upon spawning
	// If in doubt, use the class's default
	if (m_BabyModelName == NULL_STRING)
	{
		m_BabyModelName = m_AdultModelName;
	}

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: Overridden to create a physics object. Borrowed from Will-E.
// Output : True if the physics object is created.
//-----------------------------------------------------------------------------
bool CNPC_Egg::CreateVPhysics( void )
{
	//Spawn our physics hull
	IPhysicsObject *pPhys = VPhysicsInitNormal( SOLID_VPHYSICS, 0, false );
	if (pPhys == NULL)
	{
		DevMsg( "%s unable to spawn physics object!\n", GetDebugName() );
		return false;
	}

	return true;
}

void CNPC_Egg::StartTask( const Task_t * pTask )
{
	// Destroy the egg
	if ( pTask->iTask == TASK_EGG_HATCH )
	{
		HatchThink();
		TaskComplete();
	}

	BaseClass::StartTask( pTask );
}

void CNPC_Egg::RunTask( const Task_t * pTask )
{
	BaseClass::RunTask( pTask );
}


//-----------------------------------------------------------------------------
// Purpose: Eggs use alert sounds when disturbed
//-----------------------------------------------------------------------------
void CNPC_Egg::AlertSound( void )
{
	EmitSound( STRING( m_iszAlertSound ) );
}

//-----------------------------------------------------------------------------
// Purpose: Create a new baby bullsquid
// Output : True if the new bullsquid is created
//-----------------------------------------------------------------------------
bool CNPC_Egg::SpawnNPC()
{
	// Try to create entity
	CAI_BaseNPC *pChild = static_cast< CAI_BaseNPC * >(CreateEntityByName( STRING( m_isChildClassname ) ));
	if (pChild)
	{
		CNPC_BasePredator * pPredator = dynamic_cast< CNPC_BasePredator * >(pChild);
		if ( pPredator != NULL )
		{
			pPredator->SetIsBaby( true );
			{
				inputdata_t dummy;
				pPredator->InputSetWanderAlways( dummy );
			}
			{
				inputdata_t dummy;
				pPredator->InputEnableSpawning( dummy );
			}

			if ( m_AdultModelName != NULL_STRING )
			{
				pPredator->KeyValue( "adultmodel", STRING( m_AdultModelName ) );
			}
			
			if ( m_BabyModelName != NULL_STRING )
			{
				pPredator->KeyValue( "babymodel", STRING( m_BabyModelName ) );
			}
		}

		if ( m_AdultModelName != NULL_STRING )
		{
			pChild->SetModelName( m_AdultModelName );
		}

		pChild->m_tEzVariant = this->m_tEzVariant;
		pChild->m_nSkin = m_iChildSkin;
		pChild->Precache();

		DispatchSpawn( pChild );

		// Now attempt to drop into the world
		Vector spawnPos = GetAbsOrigin() + Vector( 0, 0, 16 );
		QAngle spawnAngle = QAngle( vec_t(), GetAbsAngles().y, vec_t() );
		pChild->Teleport( &spawnPos, &spawnAngle, NULL );

		pChild->SetSquad( this->GetSquad() );
		pChild->Activate();

		// Fire output
		variant_t value;
		value.SetEntity( pChild );
		m_OnSpawnNPC.CBaseEntityOutput::FireOutput( value, this, this );

		return true;
	}

	// Couldn't instantiate NPC
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_Egg::CorpseGib( const CTakeDamageInfo &info )
{
	DispatchParticleEffect( STRING( m_isHatchParticle ), WorldSpaceCenter(), GetAbsAngles() );
	EmitSound( STRING( m_iszHatchSound ) );

	SpawnNPC();

	SUB_Remove();

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Think function for hatching on a timer
//-----------------------------------------------------------------------------
void CNPC_Egg::HatchThink()
{
	CorpseGib( CTakeDamageInfo() );
}

AI_BEGIN_CUSTOM_NPC( npc_egg, CNPC_Egg )

DECLARE_TASK( TASK_EGG_HATCH )

//=========================================================
// > SCHED_EGG_HATCH
//=========================================================
// TODO - Add some fancy effects to SCHED_EGG_HATCH to show the egg starting to rupture when the player nears. For now, the sound will do.
DEFINE_SCHEDULE
(
	SCHED_EGG_HATCH,

	"	Tasks"
	"		TASK_SOUND_WAKE				0"
	"		TASK_WAIT					2"
	"       TASK_EGG_HATCH              0"
	"	"
	"	Interrupts"
	"       COND_LOST_ENEMY"
	"       COND_ENEMY_WENT_NULL"
	"       COND_ENEMY_TOO_FAR"
	"       COND_FLORA_EXTEND"
)

AI_END_CUSTOM_NPC()
