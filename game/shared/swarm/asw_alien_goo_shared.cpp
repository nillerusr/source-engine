#include "cbase.h"
#ifdef CLIENT_DLL
	#define CBaseFlex C_BaseFlex
	#define CASW_Alien_Goo C_ASW_Alien_Goo
	#define CASW_Grub_Sac C_ASW_Grub_Sac
	#include "c_asw_generic_emitter_entity.h"
	#include "c_asw_fx.h"
	#include "baseparticleentity.h"
	#include "asw_util_shared.h"
#else
	#include "EntityFlame.h"
	#include "asw_grub.h"
	#include "asw_simple_grub.h"
	#include "asw_marine.h"
	#include "asw_player.h"
	#include "asw_fx_shared.h"
	#include "asw_util_shared.h"
	#include "asw_gamerules.h"
	#include "asw_mission_manager.h"
	#include "asw_entity_dissolve.h"
	#include "asw_marine_speech.h"
	#include "asw_burning.h"
	#include "te_effect_dispatch.h"
	#include "cvisibilitymonitor.h"
#endif
#include "asw_alien_goo_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define ASW_GRUB_SAC_BURST_DISTANCE 180.0f
#define ACID_BURN_INTERVAL 1.0f
#define ASW_ACID_DAMAGE 10.0f


IMPLEMENT_NETWORKCLASS_ALIASED( ASW_Alien_Goo, DT_ASW_Alien_Goo )

BEGIN_NETWORK_TABLE( CASW_Alien_Goo, DT_ASW_Alien_Goo )
#ifdef CLIENT_DLL
	// recvprops
	RecvPropFloat	(RECVINFO(m_fPulseStrength)),
	RecvPropFloat	(RECVINFO(m_fPulseSpeed)),
	RecvPropBool(RECVINFO(m_bOnFire)),
#else
	// sendprops
	SendPropExclude( "DT_BaseFlex", "m_flexWeight" ),		// don't send the weights from the server, we'll animate them clientside
	SendPropFloat	(SENDINFO(m_fPulseStrength), 10, SPROP_UNSIGNED ),
	SendPropFloat	(SENDINFO(m_fPulseSpeed), 10, SPROP_UNSIGNED ),
	SendPropBool(SENDINFO(m_bOnFire)),
#endif
END_NETWORK_TABLE()

#ifdef GAME_DLL
ConVar asw_goo_volume("asw_goo_volume", "1.0f", FCVAR_CHEAT, "Volume of the alien goo looping sound");
LINK_ENTITY_TO_CLASS( asw_alien_goo, CASW_Alien_Goo );
PRECACHE_WEAPON_REGISTER(asw_alien_goo);
#endif

#ifndef CLIENT_DLL

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CASW_Alien_Goo )
	DEFINE_KEYFIELD( m_fPulseStrength, FIELD_FLOAT, "PulseStrength" ),
	DEFINE_KEYFIELD( m_fPulseSpeed, FIELD_FLOAT, "PulseSpeed" ),
	DEFINE_KEYFIELD( m_BurningLinkName, FIELD_STRING, "BurningLinkName" ),
	DEFINE_KEYFIELD( m_fGrubSpawnAngle, FIELD_FLOAT, "GrubSpawnAngle" ),
	DEFINE_KEYFIELD( m_bHasAmbientSound, FIELD_BOOLEAN, "HasAmbientSound" ),
	DEFINE_KEYFIELD( m_bRequiredByObjective, FIELD_BOOLEAN, "RequiredByObjective" ),
	DEFINE_FIELD( m_bSpawnedGrubs, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bHasGrubs, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bOnFire, FIELD_BOOLEAN ),
	DEFINE_THINKFUNC(BurningLinkThink),	
	DEFINE_THINKFUNC(InitThink),
	DEFINE_THINKFUNC(GrubSacThink),
	DEFINE_FUNCTION( GooTouch ),
	DEFINE_FUNCTION( GooAcidTouch ),
	DEFINE_INPUTFUNC( FIELD_VOID,	"Burst",	InputBurst ),
	DEFINE_OUTPUT( m_OnGooDestroyed,		"OnGooDestroyed" ),
END_DATADESC()
float CASW_Alien_Goo::s_fNextSpottedChatterTime = 0;

CUtlVector<CASW_Alien_Goo*>	g_AlienGoo;

#endif // not client

CASW_Alien_Goo::CASW_Alien_Goo()
{
#ifdef CLIENT_DLL
	m_fPulseCycle = 0;
	m_bClientOnFire = false;
	m_pBurningEffect = NULL;
#else
	m_bPlayingAmbientSound = false;
	m_bPlayingGooScream = false;
	m_fNextAcidBurnTime = 0;
#endif
	m_fPulseStrength = 1.0f;
	m_fPulseSpeed = 1.0f;	
#ifndef CLIENT_DLL
	g_AlienGoo.AddToTail( this );
#endif
}


CASW_Alien_Goo::~CASW_Alien_Goo()
{
#ifdef CLIENT_DLL
	m_bOnFire = false;
	UpdateFireEmitters();
#endif
#ifndef CLIENT_DLL
	g_AlienGoo.FindAndRemove( this );
#endif
}

#ifndef CLIENT_DLL

void CASW_Alien_Goo::Spawn()
{
	BaseClass::Spawn();
	char *szModel = (char *)STRING( GetModelName() );
	if (!szModel || !*szModel)
	{
		Warning( "%s at %.0f %.0f %0.f missing modelname\n", GetClassname(), GetAbsOrigin().x, GetAbsOrigin().y, GetAbsOrigin().z );
		UTIL_Remove( this );
		return;
	}

	if (FClassnameIs(this, "asw_grub_sac"))
	{
		m_bHasGrubs = true;
		m_fPulseStrength = 1.0f;
		m_fPulseSpeed = random->RandomFloat(1.5f, 3.0f);
		SetBlocksLOS(false);
	}

	PrecacheModel( szModel );
	Precache();
	SetModel( szModel );
	SetMoveType( MOVETYPE_NONE );
	SetSolid(SOLID_VPHYSICS);
	SetCollisionGroup( COLLISION_GROUP_NONE ); //COLLISION_GROUP_DEBRIS );
	m_takedamage = DAMAGE_YES;
	m_iHealth = 100;
	m_iMaxHealth = m_iHealth;	

	AddFlag( FL_STATICPROP );
	VPhysicsInitStatic();

	SetThink( &CASW_Alien_Goo::InitThink );
	SetNextThink( gpGlobals->curtime + random->RandomFloat(5.0f, 10.0f));

	if ( m_bRequiredByObjective )
	{
		VisibilityMonitor_AddEntity( this, asw_visrange_generic.GetFloat() * 0.9f, NULL, NULL );
	}
}

void CASW_Alien_Goo::InitThink()
{
	if (m_bHasAmbientSound && !m_bPlayingAmbientSound)
	{
		if (!UTIL_ASW_MissionHasBriefing(STRING(gpGlobals->mapname)))
		{
			StartGooSound();
		}
	}

	// if spawns grubs
	if (m_bHasGrubs)
	{
		SetTouch( &CASW_Alien_Goo::GooTouch );	
	}	
	else
	{
		SetTouch( &CASW_Alien_Goo::GooAcidTouch );
	}
	// all goo needs to think periodically, so marines can shout about it
	// check if we need to burst open thanks to nearby marines
	SetThink( &CASW_Alien_Goo::GrubSacThink );	
	SetNextThink( gpGlobals->curtime + 1.0f );
}

void CASW_Alien_Goo::Event_Killed( const CTakeDamageInfo &info )
{
	m_OnGooDestroyed.FireOutput( this, this );

	if (!m_bHasGrubs)
	{
		if (ASWGameRules() && ASWGameRules()->GetMissionManager())
			ASWGameRules()->GetMissionManager()->GooKilled(this);		
	}

	StopGooSound();

	BaseClass::Event_Killed(info);
}

void CASW_Alien_Goo::Precache()
{
	PrecacheModel( "models/aliens/biomass/biomasshelix.mdl" );
	PrecacheModel( "models/aliens/biomass/biomassl.mdl" );
	PrecacheModel( "models/aliens/biomass/biomasss.mdl" );
	PrecacheModel( "models/aliens/biomass/biomassu.mdl" );

	PrecacheScriptSound("ASWGoo.GooLoop");
	PrecacheScriptSound("ASWGoo.GooScream");
	PrecacheScriptSound("ASWGoo.GooDissolve");	
	PrecacheScriptSound("ASWFire.AcidBurn");	
	PrecacheParticleSystem( "biomass_dissolve" );
	PrecacheParticleSystem( "acid_touch" );
	PrecacheParticleSystem( "grubsack_death" );
	UTIL_PrecacheOther( "asw_grub" );

	BaseClass::Precache();
}

int CASW_Alien_Goo::OnTakeDamage( const CTakeDamageInfo &info )
{
	CASW_Marine* pMarine = dynamic_cast<CASW_Marine*>(info.GetAttacker());

	// if has grubs
	if ( m_bHasGrubs && HasSpawnFlags( ASW_SF_BURST_WHEN_DAMAGED ) )
	{
		SpawnGrubs();

		if ( pMarine) 
			pMarine->HurtAlien(this, info);
	}

	// goo is only damaged by fire!
	if ( !( info.GetDamageType() & DMG_BURN ) && !( info.GetDamageType() & DMG_ENERGYBEAM ) )
		return 0;

	// notify the marine that he's hurting this, so his accuracy doesn't drop
	if (info.GetAttacker() && info.GetAttacker()->Classify() == CLASS_ASW_MARINE)
	{
		if ( pMarine )
		{
			pMarine->HurtJunkItem( this, info );
		}
	}

	// if this damage would make us close to dying, then make us dissolve instead
	if ( m_iHealth - info.GetDamage() < 10.0f )
	{
		if ( !IsDissolving() )
		{	
			//bool bRagdollCreated = 
			Dissolve( NULL, gpGlobals->curtime, false, ENTITY_DISSOLVE_NORMAL );
			//Msg("Dissolved = %d\n", bRagdollCreated);
			return 0;
		}
		else if ( info.GetDamage() != 10000.0 )		// when dissolving, only the dissolve damage will kill us
		{
			return 0;
		}
	}
	
	int result = BaseClass::OnTakeDamage( info );

	if ( result > 0 )
	{		
		if ( info.GetDamageType() & DMG_BURN )
		{
			Ignite( 30.0f );

			CASW_Player *pPlayerAttacer = NULL;
			if ( pMarine )
			{
				pPlayerAttacer = pMarine->GetCommander();
			}

			IGameEvent * event = gameeventmanager->CreateEvent( "alien_ignited" );
			if ( event )
			{
				event->SetInt( "userid", ( pPlayerAttacer ? pPlayerAttacer->GetUserID() : 0 ) );
				event->SetInt( "entindex", entindex() );
				gameeventmanager->FireEventClientSide( event );
			}
		}
	}
	return result;
}

void CASW_Alien_Goo::Ignite( float flFlameLifetime, bool bNPCOnly, float flSize, bool bCalledByLevelDesigner )
{
	if ( IsOnFire() || m_bHasGrubs )
	{
		return;
	}

	AddFlag( FL_ONFIRE );
	m_bOnFire = true;

	if ( ASWBurning() )
	{
		ASWBurning()->BurnEntity(this, NULL, flFlameLifetime, 0.4f, 5.0f * 0.4f);	// 5 dps, applied every 0.4 seconds
	}

	m_OnIgnite.FireOutput( this, this );

	// wail
	StartScreaming();

	// set up a delay to burn our linked entities
	SetThink( &CASW_Alien_Goo::BurningLinkThink );	
	SetNextThink( gpGlobals->curtime + 2.0f );
}

void CASW_Alien_Goo::Extinguish()
{
	m_bOnFire = false;
	if (ASWBurning())
		ASWBurning()->Extinguish(this);
	RemoveFlag( FL_ONFIRE );
}

// periodically find the nearest marine and check if we should burst open
void CASW_Alien_Goo::GrubSacThink()
{
	if (m_bHasAmbientSound && !m_bPlayingAmbientSound)
	{
		if (UTIL_ASW_MissionHasBriefing(STRING(gpGlobals->mapname)) && ASWGameRules() && ASWGameRules()->GetGameState() == ASW_GS_INGAME)
			StartGooSound();
	}
	if (m_bHasGrubs)
	{
		float marine_distance = -1;
		if (UTIL_ASW_NearestMarine(GetAbsOrigin(), marine_distance ))	// returns the nearest marine to this point
		{		
			if (m_bHasGrubs && HasSpawnFlags(ASW_SF_BURST_WHEN_NEAR))			
			{
				if (marine_distance <= ASW_GRUB_SAC_BURST_DISTANCE)			
				{
					SpawnGrubs();
					return;
				}
			}
			// NOTE: Marines chattering about alien good disabled, as it sounded bad
			/*
			else if (marine_distance < 300 && gpGlobals->curtime > s_fNextSpottedChatterTime)
			{			
				CASW_Marine *pSpottedMarine = UTIL_ASW_Marine_Can_Chatter_Spot(this, 300);
				if (pSpottedMarine)
				{
					pSpottedMarine->GetMarineSpeech()->Chatter(CHATTER_BIOMASS);
					s_fNextSpottedChatterTime = gpGlobals->curtime + 30.0f;
				}
				else
					s_fNextSpottedChatterTime = gpGlobals->curtime + 1.0f;		
			}*/
		}
		// rethink interval based on how near the marines are
		if (marine_distance == -1 || marine_distance > 4096)
		{
			SetNextThink( gpGlobals->curtime + 5.0f );
		}
		else if (marine_distance > 1024)
		{
			SetNextThink( gpGlobals->curtime + 2.0f );
		}
		else
		{
			SetNextThink( gpGlobals->curtime + 1.0f );
		}
	}
	else
	{
		if (m_bHasAmbientSound && !m_bPlayingAmbientSound)
		{
			SetNextThink( gpGlobals->curtime + random->RandomFloat(2.0f, 5.0f) );
		}
	}
}

void CASW_Alien_Goo::BurningLinkThink()
{
	CBaseEntity *ent = NULL;
	while ( (ent = gEntList.NextEnt(ent)) != NULL )
	{
		CASW_Alien_Goo *pGoo = dynamic_cast<CASW_Alien_Goo*>(ent);
		if (pGoo)
		{
			if (  (pGoo->GetBurningLinkName() != NULL_STRING	&& FStrEq(STRING(m_BurningLinkName), STRING(pGoo->GetBurningLinkName())))	&& 
					(ent->GetClassname()!=NULL &&
						(FStrEq("asw_alien_goo", ent->GetClassname()) || FStrEq("asw_grub_sac", ent->GetClassname())
					)))
			{				
					pGoo->Ignite(30.0f);
			}
		}
	}
}

void CASW_Alien_Goo::GooAcidTouch(CBaseEntity* pOther)
{
	// if touched by a marine, acid burn him!
	if (pOther && pOther->Classify() == CLASS_ASW_MARINE)
	{
		if (m_fNextAcidBurnTime == 0 || gpGlobals->curtime > m_fNextAcidBurnTime)
		{
			m_fNextAcidBurnTime = gpGlobals->curtime + ACID_BURN_INTERVAL;
			CTakeDamageInfo info( this, this, ASW_ACID_DAMAGE, DMG_ACID );
			
			Vector	killDir = pOther->GetAbsOrigin() - GetAbsOrigin();
			VectorNormalize( killDir );

			info.SetDamageForce( killDir );
			Vector vecDamagePos = pOther->GetAbsOrigin() -killDir * 32.0f;
			info.SetDamagePosition( vecDamagePos );
			pOther->TakeDamage(info);

			EmitSound("ASWFire.AcidBurn");

			CEffectData	data;			
			data.m_vOrigin = vecDamagePos;
			data.m_nOtherEntIndex = pOther->entindex();
			DispatchEffect( "ASWAcidBurn", data );		
		}
	}
}

void CASW_Alien_Goo::GooTouch(CBaseEntity* pOther)
{
	// egg will open if touched by a marine
	CASW_Marine* pMarine = CASW_Marine::AsMarine( pOther );
	if (pMarine && m_bHasGrubs && HasSpawnFlags(ASW_SF_BURST_WHEN_TOUCHED))
	{
		SetTouch( NULL );
		SpawnGrubs();		
	}
}

bool CASW_Alien_Goo::RoomToSpawnGrub(const Vector& pos)
{
	// Check to see if there's enough room to spawn
	trace_t tr;
	UTIL_TraceHull( pos, pos, Vector(-12,-12,0),Vector(12,12,0), MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );
	if ( tr.m_pEnt || tr.startsolid )
	{
		return false;
	}

	return true;
}

// makes this bit of goo burst and spawn some grubs
void CASW_Alien_Goo::SpawnGrubs()
{
	if (m_bSpawnedGrubs)
		return;
	m_bSpawnedGrubs = true;
	AddSolidFlags( FSOLID_NOT_SOLID );
	int num_grubs = random->RandomInt(4,5);
	if (m_fGrubSpawnAngle <= 0)
		m_fGrubSpawnAngle = 180.0f;
	//Vector mins = WorldAlignMins() + GetAbsOrigin();
	//Vector maxs = WorldAlignMaxs() + GetAbsOrigin();
	if (!CollisionProp())
		return;
	// for some reason if we calculate these inside the loop, the random numbers all come out the same.  Worrying.
	QAngle angGrubFacing[5];
	angGrubFacing[0] = GetAbsAngles(); angGrubFacing[0].y += RandomFloat(-m_fGrubSpawnAngle, m_fGrubSpawnAngle);
	angGrubFacing[1] = GetAbsAngles(); angGrubFacing[1].y += RandomFloat(-m_fGrubSpawnAngle, m_fGrubSpawnAngle);
	angGrubFacing[2] = GetAbsAngles(); angGrubFacing[2].y += RandomFloat(-m_fGrubSpawnAngle, m_fGrubSpawnAngle);
	angGrubFacing[3] = GetAbsAngles(); angGrubFacing[3].y += RandomFloat(-m_fGrubSpawnAngle, m_fGrubSpawnAngle);
	angGrubFacing[4] = GetAbsAngles(); angGrubFacing[4].y += RandomFloat(-m_fGrubSpawnAngle, m_fGrubSpawnAngle);
	Vector vecSpawnPos[5];
	CollisionProp()->RandomPointInBounds( Vector(0.1f, 0.1f, 0.1f), Vector( 0.9f, 0.9f, 0.9f ), &vecSpawnPos[0] );
	CollisionProp()->RandomPointInBounds( Vector(0.1f, 0.1f, 0.1f), Vector( 0.9f, 0.9f, 0.9f ), &vecSpawnPos[1] );
	CollisionProp()->RandomPointInBounds( Vector(0.1f, 0.1f, 0.1f), Vector( 0.9f, 0.9f, 0.9f ), &vecSpawnPos[2] );
	CollisionProp()->RandomPointInBounds( Vector(0.1f, 0.1f, 0.1f), Vector( 0.9f, 0.9f, 0.9f ), &vecSpawnPos[3] );
	CollisionProp()->RandomPointInBounds( Vector(0.1f, 0.1f, 0.1f), Vector( 0.9f, 0.9f, 0.9f ), &vecSpawnPos[4] );
	Vector mins, maxs;
	CollisionProp()->WorldSpaceAABB( &mins, &mins );
	for (int i=0;i<num_grubs;i++)
	{
		angGrubFacing[i].x = 0;			
		vecSpawnPos[i].z += 1.0f;

		int count = 0;
		while (!RoomToSpawnGrub(vecSpawnPos[i]) && count < 5)
		{
			CollisionProp()->RandomPointInBounds( Vector(0.1f, 0.1f, 0.1f), Vector( 0.9f, 0.9f, 0.9f ), &vecSpawnPos[i] );
			count++;
		}
		if (count >= 5)
			continue;
		UTIL_ASW_DroneBleed( vecSpawnPos[i], Vector(0,0,1), 4 );

		CASW_Simple_Grub* pGrub = dynamic_cast<CASW_Simple_Grub*>(CreateNoSpawn("asw_grub", vecSpawnPos[i], angGrubFacing[i], this));
		if (pGrub)
		{
			ASWGameRules()->GrubSpawned(pGrub);
			pGrub->AddSpawnFlags(SF_NPC_FALL_TO_GROUND);	// stop it teleporting to the ground			
			pGrub->Spawn();
			pGrub->m_fFallSpeed = 250;
			pGrub->PlayFallingAnimation();
		}
	}

	EmitSound("ASW_Parasite.EggBurst");

	UTIL_ASW_EggGibs( WorldSpaceCenter(), EGG_FLAG_GRUBSACK_DIE, entindex() );

	trace_t	ptr;
	Vector vecDir( 0, 0, -1.0f );

	ptr.endpos = GetLocalOrigin();
	ptr.endpos[2] += 8.0f;
	
	// make blood decal on the floor
	trace_t Bloodtr;
	Vector vecTraceDir;
	int k;

	for ( k = 0 ; k < 4 ; k++ )
	{
		vecTraceDir = vecDir;

		vecTraceDir.x += random->RandomFloat( -0.8f, 0.8f );
		vecTraceDir.y += random->RandomFloat( -0.8f, 0.8f );
		vecTraceDir.z += random->RandomFloat( -0.8f, 0.8f );

		AI_TraceLine( ptr.endpos, ptr.endpos + vecTraceDir * 172, MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &Bloodtr);

		if ( Bloodtr.fraction != 1.0 )
		{
			UTIL_BloodDecalTrace( &Bloodtr, BLOOD_COLOR_BRIGHTGREEN );
		}
	}

	UTIL_Remove( this );
}

// make this goo sac burst open and spit out grubs
void CASW_Alien_Goo::InputBurst( inputdata_t &inputdata )
{
	if (m_bHasGrubs && HasSpawnFlags(ASW_SF_BURST_ON_INPUT))
		SpawnGrubs();
}

void CASW_Alien_Goo::StartGooSound()
{
	m_bPlayingAmbientSound = true;
	UTIL_EmitAmbientSound(GetSoundSourceIndex(), GetAbsOrigin(), "ASWGoo.GooLoop", 
				random->RandomFloat(0.9f, 1.0f) * asw_goo_volume.GetFloat(), SNDLVL_85dB, SND_CHANGE_VOL, random->RandomInt( 95, 105 ));
}

void CASW_Alien_Goo::StartScreaming()
{
	if (!m_bPlayingGooScream)
	{
		StopGooSound();
		m_bPlayingGooScream = true;
		UTIL_EmitAmbientSound(GetSoundSourceIndex(), GetAbsOrigin(), "ASWGoo.GooScream", 
				random->RandomFloat(0.9f, 1.0f) * asw_goo_volume.GetFloat(), SNDLVL_95dB, SND_CHANGE_VOL, random->RandomInt( 95, 105 ));
	}
}

void CASW_Alien_Goo::StopLoopingSounds()
{
	StopGooSound();
}

void CASW_Alien_Goo::StopGooSound()
{
	if (m_bPlayingGooScream)
		UTIL_EmitAmbientSound(GetSoundSourceIndex(), GetAbsOrigin(), "ASWGoo.GooLoop", 
						0, SNDLVL_NONE, SND_STOP, 0);
	else
		UTIL_EmitAmbientSound(GetSoundSourceIndex(), GetAbsOrigin(), "ASWGoo.GooLoop", 
						0, SNDLVL_NONE, SND_STOP, 0);
}

bool CASW_Alien_Goo::Dissolve( const char *pMaterialName, float flStartTime, bool bNPCOnly, int nDissolveType )
{
	// Right now this prevents stuff we don't want to catch on fire from catching on fire.
	if( bNPCOnly && !(GetFlags() & FL_NPC) )
		return false;

	// Can't dissolve twice
	if ( IsDissolving() )
		return false;

	bool bRagdollCreated = false;
	CASW_Entity_Dissolve *pDissolve = CASW_Entity_Dissolve::Create( this, pMaterialName, flStartTime, nDissolveType, &bRagdollCreated );
	if (pDissolve)
	{
		SetEffectEntity( pDissolve );

		AddFlag( FL_DISSOLVING );
//		m_flDissolveStartTime = flStartTime;

		EmitSound( "ASWGoo.GooDissolve" );
	}

	return bRagdollCreated;
}

#else

// make all our flex controllers sine in and out, each one time offset a bit to
//  create a rippling pulse in the mesh
void CASW_Alien_Goo::SetupWeights( const matrix3x4_t *pBoneToWorld, int nFlexWeightCount, float *pFlexWeights, float *pFlexDelayedWeights )
{
	m_fPulseCycle += gpGlobals->frametime * m_fPulseSpeed;
	float interval = 1.5f / nFlexWeightCount;
	for (int i = 0; i < nFlexWeightCount; i++)
	{
		pFlexWeights[i] = sin(m_fPulseCycle + i * interval);
		if (pFlexWeights[i] < 0)
			pFlexWeights[i] = -pFlexWeights[i];
		pFlexWeights[i] *= m_fPulseStrength;
	}

	if ( pFlexDelayedWeights )
	{
		memset( pFlexDelayedWeights, 0, nFlexWeightCount * sizeof(float) );
	}
}

void CASW_Alien_Goo::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );
	UpdateFireEmitters();
}

void CASW_Alien_Goo::UpdateFireEmitters()
{
	bool bOnFire = (m_bOnFire && !IsEffectActive(EF_NODRAW));
	if (bOnFire != m_bClientOnFire)
	{
		m_bClientOnFire = bOnFire;
		if (m_bClientOnFire)
		{
			if ( !m_pBurningEffect )
			{
				m_pBurningEffect = UTIL_ASW_CreateFireEffect( this );
			}
			EmitSound( "ASWFire.BurningFlesh" );
		}
		else
		{
			if ( m_pBurningEffect )
			{
				ParticleProp()->StopEmission( m_pBurningEffect );
				m_pBurningEffect = NULL;
			}
			StopSound("ASWFire.BurningFlesh");
			if ( C_BaseEntity::IsAbsQueriesValid() )
				EmitSound("ASWFire.StopBurning");
		}
	}
}

void CASW_Alien_Goo::UpdateOnRemove()
{
	BaseClass::UpdateOnRemove();
	m_bOnFire = false;
	UpdateFireEmitters();
}

#endif // not client

/// ========================== grub sac version =======================

#ifndef CLIENT_DLL
LINK_ENTITY_TO_CLASS( asw_grub_sac, CASW_Grub_Sac );
#endif

IMPLEMENT_NETWORKCLASS_ALIASED( ASW_Grub_Sac, DT_ASW_Grub_Sac )

BEGIN_NETWORK_TABLE( CASW_Grub_Sac, DT_ASW_Grub_Sac )
#ifdef CLIENT_DLL
	// recvprops
#else
	// sendprops
#endif
END_NETWORK_TABLE()

CASW_Grub_Sac::CASW_Grub_Sac()
{
}

CASW_Grub_Sac::~CASW_Grub_Sac()
{
}