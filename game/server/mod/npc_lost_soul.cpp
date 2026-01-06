//=//=============================================================================//
//
// Purpose: A frightening flying creature with the appearance of a burning skull.
//		May be familiar to first person shooter fans.
//
//		npc_lost_soul is a modified version of a manhack inspired by Mallikas'
//		HalloweenVilleFour entry, Satanophobia, as well as Doom.
//
//	Author: 1upD
//
//=============================================================================//
#include "cbase.h"
#include "soundenvelope.h"
#include "npc_manhack.h"
#include "ai_default.h"
#include "ai_node.h"
#include "ai_navigator.h"
#include "ai_pathfinder.h"
#include "ai_moveprobe.h"
#include "ai_memory.h"
#include "ai_squad.h"
#include "ai_route.h"
#include "explode.h"
#include "basegrenade_shared.h"
#include "ndebugoverlay.h"
#include "decals.h"
#include "gib.h"
#include "game.h"			
#include "ai_interactions.h"
#include "IEffects.h"
#include "vstdlib/random.h"
#include "engine/IEngineSound.h"
#include "movevars_shared.h"
#include "npcevent.h"
#include "props.h"
#include "te_effect_dispatch.h"
#include "ai_squadslot.h"
#include "world.h"
#include "smoke_trail.h"
#include "func_break.h"
#include "physics_impact_damage.h"
#include "weapon_physcannon.h"
#include "physics_prop_ragdoll.h"
#include "soundent.h"
#include "ammodef.h"
#include "EntityFlame.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar	sk_lostsoul_health("sk_lostsoul_health", "0");
ConVar	sk_lostsoul_melee_dmg("sk_lostsoul_melee_dmg", "0");

#define MANHACK_NOISEMOD_HIDE 5000
//=========================================================
//=========================================================
class CNPC_LostSoul : public CNPC_Manhack
{
	DECLARE_CLASS( CNPC_LostSoul, CNPC_Manhack);

public:
	void	Precache( void );
	void	Spawn( void );
	Class_T Classify( void );

	virtual void	DeathSound(const CTakeDamageInfo &info);
	virtual bool	ShouldGib(const CTakeDamageInfo &info);

	void			BladesInit();
	void			SoundInit(void);
	void			PlayFlySound(void);
	void			StartEye(void);


	void			CheckCollisions(float flInterval);

	void			Slice(CBaseEntity *pHitEntity, float flInterval, trace_t &tr);

	virtual int				OnTakeDamage_Alive(const CTakeDamageInfo &info);

	void Ignite(float flFlameLifetime, bool bNPCOnly, float flSize, bool bCalledByLevelDesigner) { return; }

	// INPCInteractive Functions
	virtual bool	CanInteractWith(CAI_BaseNPC *pUser) { return false; } // Disabled for now (sjb)
	virtual	bool	HasBeenInteractedWith() { return false; }
	virtual void	NotifyInteraction(CAI_BaseNPC *pUser){}

	virtual void	InputPowerdown(inputdata_t &inputdata)
	{
		m_iHealth = 0;
	}

	void			MoveToTarget(float flInterval, const Vector &MoveTarget);

	void PlayAttackSound(bool hostile = true);
	void PlayDamagedSound(void);
	void SetEyeState(int state);

	DECLARE_DATADESC();


	DEFINE_CUSTOM_AI;
private:
	float			m_flNextEngineSoundTime;
};


LINK_ENTITY_TO_CLASS( npc_lost_soul, CNPC_LostSoul );
IMPLEMENT_CUSTOM_AI( npc_manhack,CNPC_LostSoul );


//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CNPC_LostSoul )
	DEFINE_KEYFIELD(m_iHealth, FIELD_INTEGER, "Health"),
	
	DEFINE_FIELD(m_flNextEngineSoundTime, FIELD_TIME)
END_DATADESC()

//-----------------------------------------------------------------------------
// Purpose: Initialize the custom schedules
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CNPC_LostSoul::InitCustomSchedules(void) 
{
	INIT_CUSTOM_AI(CNPC_LostSoul);
}

//-----------------------------------------------------------------------------
// Purpose: 
//
//
//-----------------------------------------------------------------------------
void CNPC_LostSoul::Precache( void )
{
	PrecacheModel( "models/skeleton/skeleton_torso3.mdl" ); // Replace this with setting from Hammer

	PrecacheScriptSound("NPC_LostSoul.Die");
	PrecacheScriptSound("NPC_LostSoul.Burn");
	PrecacheScriptSound("NPC_LostSoul.Float");
	PrecacheScriptSound("NPC_LostSoul.ChargeAnnounce");
	PrecacheScriptSound("NPC_LostSoul.ChargeEnd");
	PrecacheScriptSound("NPC_LostSoul.Stunned");
	PrecacheScriptSound("NPC_LostSoul.Bat");

	BaseClass::Precache();
}


//-----------------------------------------------------------------------------
// Purpose: 
//
//
//-----------------------------------------------------------------------------
void CNPC_LostSoul::Spawn( void )
{
	BaseClass::Spawn();

	Precache();

	SetModel("models/skeleton/skeleton_torso3.mdl");
	SetHullType(HULL_TINY_CENTERED); // There was an error being thrown about collision model being smaller than nav hull - need to look into
	SetHullSizeNormal();

	SetSolid(SOLID_BBOX);
	AddSolidFlags(FSOLID_NOT_STANDABLE);

	if (HasSpawnFlags(SF_MANHACK_CARRIED))
	{
		AddSolidFlags(FSOLID_NOT_SOLID);
		SetMoveType(MOVETYPE_NONE);
	}
	else
	{
		SetMoveType(MOVETYPE_VPHYSICS);
	}

	// Use the convar if health is less than 1
	if (m_iHealth < 1)
	{
		m_iHealth = sk_lostsoul_health.GetFloat();
	}

	SetViewOffset(Vector(0, 0, 10));		// Position of the eyes relative to NPC's origin.
	m_flFieldOfView = VIEW_FIELD_FULL;
	m_NPCState = NPC_STATE_NONE;

	if (m_spawnflags & SF_MANHACK_USE_AIR_NODES)
	{
		SetNavType(NAV_FLY);
	}
	else
	{
		SetNavType(NAV_GROUND);
	}

	AddEFlags(EFL_NO_DISSOLVE | EFL_NO_MEGAPHYSCANNON_RAGDOLL);
	AddEffects(EF_NOSHADOW);

	SetBloodColor(BLOOD_COLOR_RED);
	SetCurrentVelocity(vec3_origin);

	CapabilitiesAdd(bits_CAP_INNATE_MELEE_ATTACK1 | bits_CAP_MOVE_FLY | bits_CAP_SQUAD);

	// Set the noise mod to huge numbers right now, in case this manhack starts out waiting for a script
	// for instance, we don't want him to bob whilst he's waiting for a script. This allows designers
	// to 'hide' manhacks in small places. (sjb)
	SetNoiseMod(MANHACK_NOISEMOD_HIDE, MANHACK_NOISEMOD_HIDE, MANHACK_NOISEMOD_HIDE);

	m_fHeadYaw = 0;

	NPCInit();

	// Manhacks are designed to slam into things, so don't take much damage from it!
	SetImpactEnergyScale(0.001);

	// Manhacks get 30 seconds worth of free knowledge.
	GetEnemies()->SetFreeKnowledgeDuration(30.0);

	// don't be an NPC, we want to collide with debris stuff
	SetCollisionGroup(COLLISION_GROUP_NONE);

	CEntityFlame *pFlame = CEntityFlame::Create(this);
	if (pFlame)
	{
		pFlame->SetLifetime(HUGE_VAL);
		SetEffectEntity(pFlame);
		pFlame->SetSize(8);
		pFlame->SetDamage(0.0f);
	}

	m_flNextEngineSoundTime = gpGlobals->curtime;
}

int CNPC_LostSoul::OnTakeDamage_Alive(const CTakeDamageInfo &info)
{
	// Don't take burning damage!
	if (info.GetDamageType() & 8) {
		return 0;
	}

	return BaseClass::OnTakeDamage_Alive(info);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_LostSoul::DeathSound(const CTakeDamageInfo &info)
{
	CPASAttenuationFilter filter2(this, "NPC_LostSoul.Die");
	EmitSound(filter2, entindex(), "NPC_LostSoul.Die");
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_LostSoul::ShouldGib(const CTakeDamageInfo &info)
{
	// TODO: Add gibs to lost soul
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Overloads Manhack method. Lost Souls have no blades
//-----------------------------------------------------------------------------
void CNPC_LostSoul::BladesInit()
{
	SetActivity(ACT_FLY);
}

//-----------------------------------------------------------------------------
// Purpose: Overloads manhack engine sound. 
//-----------------------------------------------------------------------------
void CNPC_LostSoul::SoundInit(void)
{
	// Just don't even worry about it.
}

void CNPC_LostSoul::PlayFlySound(void)
{
	float flEnemyDist;

	if (GetEnemy())
	{
		flEnemyDist = (GetAbsOrigin() - GetEnemy()->GetAbsOrigin()).Length();
	}
	else
	{
		flEnemyDist = FLT_MAX;
	}

	// Play special engine every once in a while
	if (gpGlobals->curtime > m_flNextEngineSoundTime && flEnemyDist < 48)
	{
		m_flNextEngineSoundTime = gpGlobals->curtime + random->RandomFloat(0.5, 2.0);

		EmitSound("NPC_LostSoul.Float");
	}
}

void CNPC_LostSoul::StartEye(void)
{
	// No eyes
}

//-----------------------------------------------------------------------------
// Purpose: We've touched something that we can hurt. Slice it!
// Input  :
// Output :
//-----------------------------------------------------------------------------
#define MANHACK_SMASH_TIME	0.35		// How long after being thrown from a physcannon that a manhack is eligible to die from impact
void CNPC_LostSoul::Slice(CBaseEntity *pHitEntity, float flInterval, trace_t &tr)
{
	// Don't hurt the player if I'm in water
	if (GetWaterLevel() > 0 && pHitEntity->IsPlayer())
		return;

	if (pHitEntity->m_takedamage == DAMAGE_NO)
		return;

	// Damage must be scaled by flInterval so framerate independent
	float flDamage = sk_lostsoul_melee_dmg.GetFloat() * flInterval;

	if (pHitEntity->IsPlayer())
	{
		flDamage *= 1.0f;
	}

	if (dynamic_cast<CBreakableProp*>(pHitEntity)) {
		dynamic_cast<CBreakableProp*>(pHitEntity)->Ignite(10.0f, false);
	}

	else if (pHitEntity->IsNPC())
	{
		dynamic_cast<CAI_BaseNPC*>(pHitEntity)->Ignite(2.0f);
	}

	if (flDamage < 1.0f)
	{
		flDamage = 1.0f;
	}

	CTakeDamageInfo info(this, this, flDamage, DMG_SLASH);

	// check for actual "ownership" of damage
	CBasePlayer *pPlayer = HasPhysicsAttacker(MANHACK_SMASH_TIME);
	if (pPlayer)
	{
		info.SetAttacker(pPlayer);
	}

	Vector dir = (tr.endpos - tr.startpos);
	if (dir == vec3_origin)
	{
		dir = tr.m_pEnt->GetAbsOrigin() - GetAbsOrigin();
	}
	CalculateMeleeDamageForce(&info, dir, tr.endpos);
	pHitEntity->TakeDamage(info);

	// Play burning sound
	EmitSound("NPC_LostSoul.Burn");

	// Pop back a little bit after hitting the player
	// ComputeSliceBounceVelocity(pHitEntity, tr);

	// Save off when we last hit something
	m_flLastDamageTime = gpGlobals->curtime;

	// Reset our state and give the player time to react
	// StopBurst(true);
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CNPC_LostSoul::MoveToTarget(float flInterval, const Vector &vMoveTarget)
{
	if (flInterval <= 0)
	{
		return;
	}

	// -----------------------------------------
	// Don't steer if engine's have stalled
	// -----------------------------------------
	if (m_iHealth <= 0)
		return;

	if (GetEnemy() != NULL)
	{
		TurnHeadToTarget(flInterval, GetEnemy()->EyePosition());
	}
	else
	{
		TurnHeadToTarget(flInterval, vMoveTarget);
	}

	// -------------------------------------
	// Move towards our target
	// -------------------------------------
	float	myAccel;
	float	myZAccel = 300.0f;
	float	myDecay = 0.3f;

	Vector targetDir;
	float flDist;


		Vector vecCurrentDir = GetCurrentVelocity();
		VectorNormalize(vecCurrentDir);

		targetDir = vMoveTarget - GetAbsOrigin();
		flDist = VectorNormalize(targetDir);

		float flDot = DotProduct(targetDir, vecCurrentDir);

		// Otherwise we should steer towards our goal
		if (flDot > 0.25)
		{
			// If my target is in front of me, my flight model is a bit more accurate.
			myAccel = 300;
		}
		else
		{
			// Have a harder time correcting my course if I'm currently flying away from my target.
			myAccel = 200;
		}

	// Clamp lateral acceleration
	if (myAccel > (flDist / flInterval))
	{
		myAccel = flDist / flInterval;
	}

	// Clamp vertical movement
	if (myZAccel > flDist / flInterval)
	{
		myZAccel = flDist / flInterval;
	}

	MoveInDirection(flInterval, targetDir, myAccel, myZAccel, myDecay);

	// calc relative banking targets
	Vector forward, right;
	GetVectors(&forward, &right, NULL);
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CNPC_LostSoul::CheckCollisions(float flInterval)
{
	// Trace forward to see if I hit anything. But trace forward along the
	// owner's view direction if you're being carried.
	Vector vecTraceDir, vecCheckPos;
	VPhysicsGetObject()->GetVelocity(&vecTraceDir, NULL);
	vecTraceDir *= flInterval;

	VectorAdd(GetAbsOrigin(), vecTraceDir, vecCheckPos);

	trace_t			tr;
	CBaseEntity*	pHitEntity = NULL;

	AI_TraceHull(GetAbsOrigin(),
		vecCheckPos,
		GetHullMins(),
		GetHullMaxs(),
		MoveCollisionMask(),
		this,
		COLLISION_GROUP_NONE,
		&tr);

	if ((tr.fraction != 1.0 || tr.startsolid) && tr.m_pEnt)
	{
		PhysicsMarkEntitiesAsTouching(tr.m_pEnt, tr);
		pHitEntity = tr.m_pEnt;

		if (pHitEntity != NULL &&
			pHitEntity->m_takedamage == DAMAGE_YES &&
			pHitEntity->Classify() != CLASS_MANHACK)
		{
			// Slice this thing
			Slice(pHitEntity, flInterval, tr);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : hostile - 
//-----------------------------------------------------------------------------
void CNPC_LostSoul::PlayAttackSound(bool hostile /*= true*/)
{
	if (hostile)
	{
		EmitSound("NPC_LostSoul.ChargeAnnounce");
	}
	else
	{
		EmitSound("NPC_LostSoul.ChargeEnd");
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_LostSoul::PlayDamagedSound(void)
{
	EmitSound("NPC_LostSoul.Bat");
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : state - 
//-----------------------------------------------------------------------------
void CNPC_LostSoul::SetEyeState(int state)
{
	// Do nothing for now
}

//-----------------------------------------------------------------------------
// Purpose: 
//
//
// Output : 
//-----------------------------------------------------------------------------
Class_T	CNPC_LostSoul::Classify( void )
{
	return	CLASS_HEADCRAB;
}
