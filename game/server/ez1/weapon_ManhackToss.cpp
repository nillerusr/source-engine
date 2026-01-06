//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:		ManhackToss - Manhack weapon - Breadman
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "npcevent.h"
#include "basehlcombatweapon.h"
#include "basecombatcharacter.h"
#include "ai_basenpc.h"
#include "player.h"
#include "npc_manhack.h"
#include "gamerules.h"
#include "in_buttons.h"
#include "soundent.h"
#include "game.h"
#include "vstdlib/random.h"
#include "gamestats.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define	MANHACKTOSS_FASTEST_REFIRE_TIME		1.0f
#define	MANHACKTOSS_FASTEST_DRY_REFIRE_TIME	1.0f

#define	MANHACKTOSS_ACCURACY_SHOT_PENALTY_TIME		0.2f	// Applied amount of time each shot adds to the time we must recover from
#define	MANHACKTOSS_ACCURACY_MAXIMUM_PENALTY_TIME	1.5f	// Maximum penalty to deal out
#define	MAXBURST	1

ConVar	manhacktoss_use_new_accuracy("manhacktoss_use_new_accuracy", "1");

//-----------------------------------------------------------------------------
// CWeaponManhackToss
//-----------------------------------------------------------------------------

class CWeaponManhackToss : public CBaseHLCombatWeapon
{
	DECLARE_DATADESC();

public:
	DECLARE_CLASS(CWeaponManhackToss, CBaseHLCombatWeapon);

	CWeaponManhackToss(void);

	DECLARE_SERVERCLASS();

	void	Precache(void);
	void	ItemPostFrame(void);
	void	ItemPreFrame(void);
	void	ItemBusyFrame(void);
	void	PrimaryAttack(void);
	void	AddViewKick(void);
	void	DryFire(void);
	void	Operator_HandleAnimEvent(animevent_t *pEvent, CBaseCombatCharacter *pOperator);


	void	UpdatePenaltyTime(void);

	int		CapabilitiesGet(void) { return bits_CAP_WEAPON_RANGE_ATTACK1; }
	Activity	GetPrimaryAttackActivity(void);

	virtual bool Reload(void);

	virtual const Vector& GetBulletSpread(void)
	{
		// Handle NPCs first
		static Vector npcCone = VECTOR_CONE_5DEGREES;
		if (GetOwner() && GetOwner()->IsNPC())
			return npcCone;

		static Vector cone;

		if (manhacktoss_use_new_accuracy.GetBool())
		{
			float ramp = RemapValClamped(m_flAccuracyPenalty,
				0.0f,
				MANHACKTOSS_ACCURACY_MAXIMUM_PENALTY_TIME,
				0.0f,
				1.0f);

			// We lerp from very accurate to inaccurate over time
			VectorLerp(VECTOR_CONE_1DEGREES, VECTOR_CONE_6DEGREES, ramp, cone);
		}
		else
		{
			// Old value
			cone = VECTOR_CONE_4DEGREES;
		}

		return cone;
	}

	virtual int	GetMinBurst()
	{
		return 1;
	}

	virtual int	GetMaxBurst()
	{
		return 3;
	}

	virtual float GetFireRate(void)
	{
		return 2.0f;
	}

	DECLARE_ACTTABLE();

private:
	float	m_flSoonestPrimaryAttack;
	float	m_flLastAttackTime;
	float	m_flAccuracyPenalty;
	int		m_nNumShotsFired;
	int		m_iSemi;
};


IMPLEMENT_SERVERCLASS_ST(CWeaponManhackToss, DT_WeaponManhackToss)
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS(weapon_manhacktoss, CWeaponManhackToss);
PRECACHE_WEAPON_REGISTER(weapon_manhacktoss);

BEGIN_DATADESC(CWeaponManhackToss)

DEFINE_FIELD(m_flSoonestPrimaryAttack, FIELD_TIME),
DEFINE_FIELD(m_flLastAttackTime, FIELD_TIME),
DEFINE_FIELD(m_flAccuracyPenalty, FIELD_FLOAT), //NOTENOTE: This is NOT tracking game time
DEFINE_FIELD(m_nNumShotsFired, FIELD_INTEGER),

END_DATADESC()

acttable_t	CWeaponManhackToss::m_acttable[] =
{
	//{ ACT_IDLE, ACT_IDLE_PISTOL, true },
	//{ ACT_IDLE_ANGRY, ACT_IDLE_ANGRY_PISTOL, true },
	//{ ACT_RANGE_ATTACK1, ACT_RANGE_ATTACK_PISTOL, true },
	//{ ACT_RELOAD, ACT_RELOAD_PISTOL, true },
	//{ ACT_WALK_AIM, ACT_WALK_AIM_PISTOL, true },
	//{ ACT_RUN_AIM, ACT_RUN_AIM_PISTOL, true },
	//{ ACT_GESTURE_RANGE_ATTACK1, ACT_GESTURE_RANGE_ATTACK_PISTOL, true },
	//{ ACT_RELOAD_LOW, ACT_RELOAD_PISTOL_LOW, false },
	//{ ACT_RANGE_ATTACK1_LOW, ACT_RANGE_ATTACK_PISTOL_LOW, false },
	//{ ACT_COVER_LOW, ACT_COVER_PISTOL_LOW, false },
	//{ ACT_RANGE_AIM_LOW, ACT_RANGE_AIM_PISTOL_LOW, false },
	//{ ACT_GESTURE_RELOAD, ACT_GESTURE_RELOAD_PISTOL, false },
	//{ ACT_WALK, ACT_WALK_PISTOL, false },
	//{ ACT_RUN, ACT_RUN_PISTOL, false },
	{ ACT_RANGE_ATTACK1, ACT_RANGE_ATTACK_SLAM, true }
};


IMPLEMENT_ACTTABLE(CWeaponManhackToss);

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeaponManhackToss::CWeaponManhackToss(void)
{
	m_flSoonestPrimaryAttack = gpGlobals->curtime;
	m_flAccuracyPenalty = 0.0f;

	m_fMinRange1 = 24;
	m_fMaxRange1 = 1500;
	m_fMinRange2 = 24;
	m_fMaxRange2 = 200;

	m_bFiresUnderwater = true;
	m_iSemi = MAXBURST;
}

//-----------------------------------------------------------------------------
// Purpose: Precache the manhack npc ent
//-----------------------------------------------------------------------------
void CWeaponManhackToss::Precache(void)
{
	BaseClass::Precache();

	UTIL_PrecacheOther("npc_manhack");
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CWeaponManhackToss::Operator_HandleAnimEvent(animevent_t *pEvent, CBaseCombatCharacter *pOperator)
{
	switch (pEvent->event)
	{
	case EVENT_WEAPON_PISTOL_FIRE:
	{
		Vector vecShootOrigin, vecShootDir;
		vecShootOrigin = pOperator->Weapon_ShootPosition();

		CAI_BaseNPC *npc = pOperator->MyNPCPointer();
		ASSERT(npc != NULL);

		vecShootDir = npc->GetActualShootTrajectory(vecShootOrigin);

		CSoundEnt::InsertSound(SOUND_COMBAT | SOUND_CONTEXT_GUNFIRE, pOperator->GetAbsOrigin(), SOUNDENT_VOLUME_PISTOL, 0.2, pOperator, SOUNDENT_CHANNEL_WEAPON, pOperator->GetEnemy());

		WeaponSound(SINGLE_NPC);
		pOperator->FireBullets(1, vecShootOrigin, vecShootDir, VECTOR_CONE_PRECALCULATED, MAX_TRACE_LENGTH, m_iPrimaryAmmoType, 2);
		pOperator->DoMuzzleFlash();

		m_iClip1 = m_iClip1 - 1;
	}
	break;
	default:
		BaseClass::Operator_HandleAnimEvent(pEvent, pOperator);
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeaponManhackToss::DryFire(void)
{
	WeaponSound(EMPTY);
	//SendWeaponAnim(ACT_VM_DRYFIRE);

	m_flSoonestPrimaryAttack = gpGlobals->curtime + MANHACKTOSS_FASTEST_DRY_REFIRE_TIME;
	m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration();
}

//-----------------------------------------------------------------------------
// Purpose: Primary attack - Spawn the Manhack
//-----------------------------------------------------------------------------
void CWeaponManhackToss::PrimaryAttack(void)
{
	SendWeaponAnim(ACT_VM_THROW);

	if ((gpGlobals->curtime - m_flLastAttackTime) > 0.5f)
	{
		m_nNumShotsFired = 0;
	}
	else
	{
		m_nNumShotsFired++;
	}

	// Time we wait before allowing to throw another
	m_flNextPrimaryAttack = gpGlobals->curtime + 0.75;

	// Remove a bullet from the clip
	m_iClip1 = m_iClip1 - 1;

	CBasePlayer *pOwner = ToBasePlayer(GetOwner());
	
	Vector vecThrow;
	AngleVectors(pOwner->EyeAngles() + pOwner->GetPunchAngle(), &vecThrow);
	VectorScale(vecThrow, 25.0f, vecThrow);

	// This is where we actually make the manhack spawn
	CNPC_Manhack *PlayerManhacks = (CNPC_Manhack * )CBaseEntity::CreateNoSpawn("npc_manhack", pOwner->Weapon_ShootPosition() + vecThrow, GetOwner()->EyeAngles(), GetOwnerEntity());

	if (PlayerManhacks == NULL) { return; }

	PlayerManhacks->AddSpawnFlags(SF_MANHACK_PACKED_UP);
	PlayerManhacks->KeyValue("squadname", "controllable_manhack_squad");
	DispatchSpawn( PlayerManhacks );
	PlayerManhacks->Activate();
	PlayerManhacks->ShouldFollowPlayer(true);

	// View punch stuff inherited from the Pistol
	if (pOwner)
	{
		pOwner->ViewPunchReset();
	}

	if (m_iSemi <= 0)
		return;
	m_iSemi--;

	m_iPrimaryAttacks++;

	gamestats->Event_WeaponFired(pOwner, true, GetClassname());

	/*if ( !HasPrimaryAmmo() )
	{
	pPlayer->SwitchToNextBestWeapon( this );
	}*/
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponManhackToss::UpdatePenaltyTime(void)
{
	CBasePlayer *pOwner = ToBasePlayer(GetOwner());

	if (pOwner == NULL)
		return;

	// Check our penalty time decay
	if (((pOwner->m_nButtons & IN_ATTACK) == false) && (m_flSoonestPrimaryAttack < gpGlobals->curtime))
	{
		m_flAccuracyPenalty -= gpGlobals->frametime;
		m_flAccuracyPenalty = clamp(m_flAccuracyPenalty, 0.0f, MANHACKTOSS_ACCURACY_MAXIMUM_PENALTY_TIME);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponManhackToss::ItemPreFrame(void)
{
	UpdatePenaltyTime();

	BaseClass::ItemPreFrame();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponManhackToss::ItemBusyFrame(void)
{
	UpdatePenaltyTime();

	BaseClass::ItemBusyFrame();
}

//-----------------------------------------------------------------------------
// Purpose: Allows firing as fast as button is pressed
//-----------------------------------------------------------------------------
void CWeaponManhackToss::ItemPostFrame(void)
{
	BaseClass::ItemPostFrame();

	if (m_bInReload)
		return;

	CBasePlayer *pOwner = ToBasePlayer(GetOwner());
	if (!(pOwner->m_nButtons & IN_ATTACK))
		m_iSemi = MAXBURST;

	if (pOwner == NULL)
		return;

	//Allow a refire as fast as the player can click
	if (((pOwner->m_nButtons & IN_ATTACK) == false) && (m_flSoonestPrimaryAttack < gpGlobals->curtime))
	{
		m_flNextPrimaryAttack = gpGlobals->curtime - 1.0f;
	}
	else if ((pOwner->m_nButtons & IN_ATTACK) && (m_flNextPrimaryAttack < gpGlobals->curtime) && (m_iClip1 <= 0))
	{
		DryFire();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
Activity CWeaponManhackToss::GetPrimaryAttackActivity(void)
{
	if (m_nNumShotsFired < 1)
		return ACT_VM_THROW;

	if (m_nNumShotsFired < 2)
		return ACT_VM_RECOIL1;

	if (m_nNumShotsFired < 3)
		return ACT_VM_RECOIL2;

	return ACT_VM_RECOIL3;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CWeaponManhackToss::Reload(void)
{
	bool fRet = DefaultReload(GetMaxClip1(), GetMaxClip2(), ACT_VM_RELOAD);
	if (fRet)
	{
		WeaponSound(RELOAD);
		m_flAccuracyPenalty = 0.0f;
	}
	return fRet;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponManhackToss::AddViewKick(void)
{
	CBasePlayer *pPlayer = ToBasePlayer(GetOwner());

	if (pPlayer == NULL)
		return;

	QAngle	viewPunch;

	viewPunch.x = random->RandomFloat(0.25f, 0.5f);
	viewPunch.y = random->RandomFloat(-.6f, .6f);
	viewPunch.z = 0.0f;

	//Add it to the view punch
	pPlayer->ViewPunch(viewPunch);
}
