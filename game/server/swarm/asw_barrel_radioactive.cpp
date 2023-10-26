#include "cbase.h"
#include "asw_barrel_radioactive.h"
#include "asw_generic_emitter_entity.h"
#include "asw_radiation_volume.h"
#include "asw_marine.h"
#include "soundenvelope.h"
#include "cvisibilitymonitor.h"
#include "particle_parse.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define ASW_RADIOACTIVE_BARREL_MODEL_NAME "models/swarm/Barrel/barrel.mdl"


LINK_ENTITY_TO_CLASS( asw_barrel_radioactive, CASW_Barrel_Radioactive );

BEGIN_DATADESC( CASW_Barrel_Radioactive )	
	DEFINE_SOUNDPATCH( m_pRadSound ),
	DEFINE_FIELD(m_hRadJet, FIELD_EHANDLE),
	DEFINE_FIELD(m_hRadCloud, FIELD_EHANDLE),
	DEFINE_FIELD(m_bBurst, FIELD_BOOLEAN),

	DEFINE_INPUTFUNC( FIELD_VOID,	"Burst",	InputBurst ),	
END_DATADESC()

CASW_Barrel_Radioactive::CASW_Barrel_Radioactive()
{
}

CASW_Barrel_Radioactive::~CASW_Barrel_Radioactive()
{
}

void CASW_Barrel_Radioactive::Spawn()
{
	DisableAutoFade();
	SetModelName( AllocPooledString( ASW_RADIOACTIVE_BARREL_MODEL_NAME ) );
	Precache();
	SetModel( ASW_RADIOACTIVE_BARREL_MODEL_NAME );

	AddSpawnFlags(SF_PHYSPROP_AIMTARGET);
	BaseClass::Spawn();
	m_nSkin = 1;
	SetHealth(30);
	SetMaxHealth(30);
	m_takedamage = DAMAGE_YES;

	VisibilityMonitor_AddEntity( this, asw_visrange_generic.GetFloat() * 0.9f, NULL, NULL );
}

void CASW_Barrel_Radioactive::Precache()
{
	PrecacheScriptSound( "Misc.Geiger" );
	PrecacheModel( ASW_RADIOACTIVE_BARREL_MODEL_NAME );
	PrecacheParticleSystem( "barrel_rad_gas_cloud" );
	PrecacheParticleSystem( "barrel_rad_gas_jet" );

	BaseClass::Precache();
}

int CASW_Barrel_Radioactive::OnTakeDamage( const CTakeDamageInfo &info )
{
	int saveFlags = m_takedamage;
	// don't burst open if melee'd
	if  ( info.GetDamageType() & ( DMG_CLUB | DMG_SLASH ) )
	{
		m_takedamage = DAMAGE_EVENTS_ONLY;
	}

	if (info.GetAttacker() && info.GetAttacker()->Classify() == CLASS_ASW_MARINE)
	{
		CASW_Marine* pMarine = assert_cast<CASW_Marine*>(info.GetAttacker());
		if ( pMarine )
		{
			pMarine->HurtJunkItem(this, info);
		}
	}
		
	// skip the breakable prop's complex damage handling and just hurt us
	int iResult = CBaseEntity::OnTakeDamage(info);
	m_takedamage = saveFlags;
	//Msg("%d Barrel took %d damage\n", entindex(), iDamage);

	return iResult;
}

void CASW_Barrel_Radioactive::Event_Killed( const CTakeDamageInfo &info )
{
	//Msg("%d Barrel died\n", entindex());
	// doesn't die when hits 0 health, but starts to leak radioactive gas
	//m_takedamage = DAMAGE_NO;
	m_lifeState = LIFE_DEAD;
	Burst(info);
	m_takedamage = DAMAGE_YES;
}

void CASW_Barrel_Radioactive::Burst( const CTakeDamageInfo &info )
{
	if (m_bBurst)
		return;

	m_bBurst = true;
	
	// spawn a jet in the direction of the damage
	QAngle ang(0,0,0);
	float shotheight = info.GetDamagePosition().z - GetAbsOrigin().z;
	shotheight = clamp(shotheight, 5, 51);
	Vector dir = info.GetDamagePosition() - WorldSpaceCenter();
	dir.z = 0;
	VectorNormalize(dir);
	ang[YAW] = UTIL_VecToYaw(dir);
	DispatchParticleEffect( "barrel_rad_gas_jet", WorldSpaceCenter()/*GetAbsOrigin()+Vector(0,0,shotheight)*/, ang, PATTACH_CUSTOMORIGIN_FOLLOW, this );
	
	// also spawn our big cloud marking out the area of radiation
	DispatchParticleEffect( "barrel_rad_gas_cloud", WorldSpaceCenter(), QAngle( 0, 0, 0 ), PATTACH_CUSTOMORIGIN_FOLLOW, this );

	/*
	CASW_Emitter* pEmitter = (CASW_Emitter*) CreateEntityByName("asw_emitter");
	if (pEmitter)
	{
		QAngle ang(0,0,0);
		float shotheight = info.GetDamagePosition().z - GetAbsOrigin().z;
		shotheight = clamp(shotheight, 5, 51);
		Vector dir = info.GetDamagePosition() - WorldSpaceCenter();
		dir.z = 0;
		VectorNormalize(dir);
		ang[YAW] = UTIL_VecToYaw(dir);
		pEmitter->UseTemplate("radjet1");
		pEmitter->SetParent(this);
		pEmitter->SetLocalOrigin(Vector(0,0,shotheight));
		pEmitter->SetLocalAngles(ang);
		//pEmitter->SetAbsOrigin(WorldSpaceCenter());
		pEmitter->Spawn();
		m_hRadJet = pEmitter;
	}
	// also spawn our big cloud marking out the area of radiation
	pEmitter = (CASW_Emitter*) CreateEntityByName("asw_emitter");
	if (pEmitter)
	{
		pEmitter->UseTemplate("radcloud1");
		pEmitter->SetParent(this);
		pEmitter->SetLocalOrigin(Vector(0,0,25));	// puts us roughly in the middle of our 51.5 height barrel
		//pEmitter->SetAbsOrigin(WorldSpaceCenter());
		pEmitter->Spawn();
		m_hRadCloud = pEmitter;
	}
	*/
	StartRadLoopSound();

	// create a volume to HURT people
	m_hRadVolume = (CASW_Radiation_Volume*) CreateEntityByName("asw_radiation_volume");
	if (m_hRadVolume)
	{
		m_hRadVolume->SetParent(this);
		m_hRadVolume->SetLocalOrigin(vec3_origin);
		m_hRadVolume->m_hCreator = info.GetAttacker();
		m_hRadVolume->Spawn();
	}

	// send a user message marking this entity as scanner noisy (makes the player's minimap distort)	
	CRecipientFilter filter;
	filter.AddAllPlayers();
	UserMessageBegin( filter, "ASWScannerNoiseEnt" );
		WRITE_SHORT( entindex() );
		WRITE_FLOAT( 150.0f );
		WRITE_FLOAT( 400.0f );
	MessageEnd();
}

void CASW_Barrel_Radioactive::InputBurst( inputdata_t &inputdata )
{
	CTakeDamageInfo info(inputdata.pActivator, inputdata.pCaller, Vector(0,0,0), WorldSpaceCenter() + RandomVector(-1, 1), 99, DMG_CLUB);	
	Burst(info);
}

void CASW_Barrel_Radioactive::StopLoopingSounds()
{
	BaseClass::StopLoopingSounds();
}

void CASW_Barrel_Radioactive::StartRadLoopSound()
{
	if( !m_pRadSound )
	{		
		const char *pszSound = "Misc.Geiger";
		float fRadPitch = random->RandomInt( 95, 105 );

		CPASAttenuationFilter filter( this );
		m_pRadSound = CSoundEnvelopeController::GetController().SoundCreate( filter, entindex(), CHAN_STATIC, pszSound, ATTN_NORM );

		CSoundEnvelopeController::GetController().Play( m_pRadSound, 1.0, fRadPitch );
	}
}