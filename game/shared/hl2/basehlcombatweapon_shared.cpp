//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "basehlcombatweapon_shared.h"
#if defined(EZ2) && defined(GAME_DLL)
#include "npcevent.h"
#endif

#include "hl2_player_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS( basehlcombatweapon, CBaseHLCombatWeapon );

IMPLEMENT_NETWORKCLASS_ALIASED( BaseHLCombatWeapon , DT_BaseHLCombatWeapon )

BEGIN_NETWORK_TABLE( CBaseHLCombatWeapon , DT_BaseHLCombatWeapon )
#if !defined( CLIENT_DLL )
//	SendPropInt( SENDINFO( m_bReflectViewModelAnimations ), 1, SPROP_UNSIGNED ),
#else
//	RecvPropInt( RECVINFO( m_bReflectViewModelAnimations ) ),
#endif
END_NETWORK_TABLE()


#if !defined( CLIENT_DLL )

#include "globalstate.h"

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CBaseHLCombatWeapon )

	DEFINE_FIELD( m_bLowered,			FIELD_BOOLEAN ),
	DEFINE_FIELD( m_flRaiseTime,		FIELD_TIME ),
	DEFINE_FIELD( m_flHolsterTime,		FIELD_TIME ),
	DEFINE_FIELD( m_iPrimaryAttacks,	FIELD_INTEGER ),
	DEFINE_FIELD( m_iSecondaryAttacks,	FIELD_INTEGER ),

END_DATADESC()

#endif

BEGIN_PREDICTION_DATA( CBaseHLCombatWeapon )
END_PREDICTION_DATA()

ConVar sk_auto_reload_time( "sk_auto_reload_time", "3", FCVAR_REPLICATED );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseHLCombatWeapon::ItemHolsterFrame( void )
{
	BaseClass::ItemHolsterFrame();

	// Must be player held
	if ( GetOwner() && GetOwner()->IsPlayer() == false )
		return;

	// We can't be active
	if ( GetOwner()->GetActiveWeapon() == this )
		return;

	// If it's been longer than three seconds, reload
	if ( ( gpGlobals->curtime - m_flHolsterTime ) > sk_auto_reload_time.GetFloat() )
	{
		// Just load the clip with no animations
		FinishReload();
		m_flHolsterTime = gpGlobals->curtime;
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CBaseHLCombatWeapon::CanLower()
{
	if ( SelectWeightedSequence( ACT_VM_IDLE_LOWERED ) == ACTIVITY_NOT_AVAILABLE )
		return false;
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Drops the weapon into a lowered pose
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseHLCombatWeapon::Lower( void )
{
	//Don't bother if we don't have the animation
	if ( SelectWeightedSequence( ACT_VM_IDLE_LOWERED ) == ACTIVITY_NOT_AVAILABLE )
		return false;

	m_bLowered = true;
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Brings the weapon up to the ready position
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseHLCombatWeapon::Ready( void )
{
	//Don't bother if we don't have the animation
	if ( SelectWeightedSequence( ACT_VM_LOWERED_TO_IDLE ) == ACTIVITY_NOT_AVAILABLE )
		return false;

	m_bLowered = false;	
	m_flRaiseTime = gpGlobals->curtime + 0.5f;
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseHLCombatWeapon::Deploy( void )
{
	// If we should be lowered, deploy in the lowered position
	// We have to ask the player if the last time it checked, the weapon was lowered
	if ( GetOwner() && GetOwner()->IsPlayer() )
	{
		CHL2_Player *pPlayer = assert_cast<CHL2_Player*>( GetOwner() );
		if ( pPlayer->IsWeaponLowered() )
		{
			if ( SelectWeightedSequence( ACT_VM_IDLE_LOWERED ) != ACTIVITY_NOT_AVAILABLE )
			{
				if ( DefaultDeploy( (char*)GetViewModel(), (char*)GetWorldModel(), ACT_VM_IDLE_LOWERED, (char*)GetAnimPrefix() ) )
				{
					m_bLowered = true;

					// Stomp the next attack time to fix the fact that the lower idles are long
					pPlayer->SetNextAttack( gpGlobals->curtime + 1.0 );
					m_flNextPrimaryAttack = gpGlobals->curtime + 1.0;
					m_flNextSecondaryAttack	= gpGlobals->curtime + 1.0;
					return true;
				}
			}
		}
	}

	m_bLowered = false;

#if defined(EZ2) && defined(GAME_DLL)
	if ( BaseClass::Deploy() )
	{
		if ( GetLeftHandGun() )
		{
			GetLeftHandGun()->RemoveEffects( EF_NODRAW );
		}
		return true;
	}

	return false;
#else
	return BaseClass::Deploy();
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseHLCombatWeapon::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	if ( BaseClass::Holster( pSwitchingTo ) )
	{
#if defined(EZ2) && defined(GAME_DLL)
		if ( GetLeftHandGun() )
		{
			GetLeftHandGun()->AddEffects( EF_NODRAW );
		}
#endif

		m_flHolsterTime = gpGlobals->curtime;
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseHLCombatWeapon::WeaponShouldBeLowered( void )
{
	// Can't be in the middle of another animation
  	if ( GetIdealActivity() != ACT_VM_IDLE_LOWERED && GetIdealActivity() != ACT_VM_IDLE &&
		 GetIdealActivity() != ACT_VM_IDLE_TO_LOWERED && GetIdealActivity() != ACT_VM_LOWERED_TO_IDLE )
  		return false;

	if ( m_bLowered )
		return true;
	
#if !defined( CLIENT_DLL )

	if ( GlobalEntity_GetState( "friendly_encounter" ) == GLOBAL_ON )
		return true;

#endif

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Allows the weapon to choose proper weapon idle animation
//-----------------------------------------------------------------------------
void CBaseHLCombatWeapon::WeaponIdle( void )
{
	//See if we should idle high or low
	if ( WeaponShouldBeLowered() )
	{
#if !defined( CLIENT_DLL )
		CHL2_Player *pPlayer = dynamic_cast<CHL2_Player*>(GetOwner());

		if( pPlayer )
		{
			pPlayer->Weapon_Lower();
		}
#endif

		// Move to lowered position if we're not there yet
		if ( GetActivity() != ACT_VM_IDLE_LOWERED && GetActivity() != ACT_VM_IDLE_TO_LOWERED 
			 && GetActivity() != ACT_TRANSITION )
		{
			SendWeaponAnim( ACT_VM_IDLE_LOWERED );
		}
		else if ( HasWeaponIdleTimeElapsed() )
		{
			// Keep idling low
			SendWeaponAnim( ACT_VM_IDLE_LOWERED );
		}
	}
	else
	{
		// See if we need to raise immediately
		if ( m_flRaiseTime < gpGlobals->curtime && GetActivity() == ACT_VM_IDLE_LOWERED ) 
		{
			SendWeaponAnim( ACT_VM_IDLE );
		}
		else if ( HasWeaponIdleTimeElapsed() ) 
		{
			SendWeaponAnim( ACT_VM_IDLE );
		}
	}
}

#if defined(EZ2) && defined(GAME_DLL)
// HACKHACK: Draws directly from npc_assassin private animevents
extern int AE_PISTOL_FIRE_LEFT;
extern int AE_PISTOL_FIRE_RIGHT;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseHLCombatWeapon::Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator )
{
	// For dual wielding
	if (pEvent->event == AE_PISTOL_FIRE_RIGHT)
	{
		Vector vecShootOrigin, vecShootDir;
		vecShootOrigin = pOperator->Weapon_ShootPosition();

		CAI_BaseNPC *npc = pOperator->MyNPCPointer();
		ASSERT( npc != NULL );

		vecShootDir = npc->GetActualShootTrajectory( vecShootOrigin );

		FireNPCPrimaryAttack( pOperator, vecShootOrigin, vecShootDir );
		return;
	}
	else if (pEvent->event == AE_PISTOL_FIRE_LEFT)
	{
		Vector vecShootOrigin, vecShootDir;

		// Use the dual wield's attachment directly
		CBaseAnimating *pLeftHandGun = GetLeftHandGun();
		if (pLeftHandGun)
		{
			pLeftHandGun->GetAttachment( pLeftHandGun->LookupAttachment( "muzzle" ), vecShootOrigin );
		}

		CAI_BaseNPC *npc = pOperator->MyNPCPointer();
		ASSERT( npc != NULL );

		vecShootDir = npc->GetActualShootTrajectory( vecShootOrigin );

		FireNPCPrimaryAttack( pOperator, vecShootOrigin, vecShootDir );
		return;
	}
	else
	{
		BaseClass::Operator_HandleAnimEvent( pEvent, pOperator );
	}
}

acttable_t	CBaseHLCombatWeapon::m_dual_acttable[] =
{
	{ ACT_IDLE,						ACT_IDLE_DUAL_PISTOLS,				true },
	{ ACT_IDLE_ANGRY,				ACT_IDLE_ANGRY_DUAL_PISTOLS,			true },
	{ ACT_RANGE_ATTACK1,			ACT_RANGE_ATTACK_DUAL_PISTOLS,		true },
	{ ACT_RELOAD,					ACT_RELOAD_DUAL_PISTOLS,				true },
	{ ACT_WALK_AIM,					ACT_WALK_AIM_DUAL_PISTOLS,			true },
	{ ACT_RUN_AIM,					ACT_RUN_AIM_DUAL_PISTOLS,				true },
	{ ACT_GESTURE_RANGE_ATTACK1,	ACT_GESTURE_RANGE_ATTACK_DUAL_PISTOLS,true },
	{ ACT_RELOAD_LOW,				ACT_RELOAD_DUAL_PISTOLS_LOW,			false },
	{ ACT_RANGE_ATTACK1_LOW,		ACT_RANGE_ATTACK_DUAL_PISTOLS_LOW,	false },
	{ ACT_COVER_LOW,				ACT_COVER_DUAL_PISTOLS_LOW,			false },
	{ ACT_RANGE_AIM_LOW,			ACT_RANGE_AIM_DUAL_PISTOLS_LOW,		false },
	{ ACT_GESTURE_RELOAD,			ACT_GESTURE_RELOAD_DUAL_PISTOLS,		false },
	{ ACT_WALK,						ACT_WALK_DUAL_PISTOLS,				false },
	{ ACT_RUN,						ACT_RUN_DUAL_PISTOLS,					false },

	// Readiness activities (not aiming)
	{ ACT_IDLE_RELAXED,				ACT_IDLE_DUAL_PISTOLS_RELAXED,		false },//never aims
	{ ACT_IDLE_STIMULATED,			ACT_IDLE_DUAL_PISTOLS_STIMULATED,		false },
	{ ACT_IDLE_AGITATED,			ACT_IDLE_ANGRY_DUAL_PISTOLS,			false },//always aims
	{ ACT_IDLE_STEALTH,				ACT_IDLE_STEALTH_PISTOL,		false },

	{ ACT_WALK_RELAXED,				ACT_WALK_DUAL_PISTOLS_RELAXED,		false },//never aims
	{ ACT_WALK_STIMULATED,			ACT_WALK_DUAL_PISTOLS_STIMULATED,		false },
	{ ACT_WALK_AGITATED,			ACT_WALK_AIM_DUAL_PISTOLS,			false },//always aims
	{ ACT_WALK_STEALTH,				ACT_WALK_STEALTH_PISTOL,		false },

	{ ACT_RUN_RELAXED,				ACT_RUN_DUAL_PISTOLS_RELAXED,			false },//never aims
	{ ACT_RUN_STIMULATED,			ACT_RUN_DUAL_PISTOLS_STIMULATED,		false },
	{ ACT_RUN_AGITATED,				ACT_RUN_AIM_DUAL_PISTOLS,				false },//always aims
	{ ACT_RUN_STEALTH,				ACT_RUN_STEALTH_PISTOL,			false },

	// Readiness activities (aiming)
	{ ACT_IDLE_AIM_RELAXED,			ACT_IDLE_DUAL_PISTOLS_RELAXED,		false },//never aims	
	{ ACT_IDLE_AIM_STIMULATED,		ACT_IDLE_AIM_DUAL_PISTOLS_STIMULATED,	false },
	{ ACT_IDLE_AIM_AGITATED,		ACT_IDLE_ANGRY_DUAL_PISTOLS,			false },//always aims
	{ ACT_IDLE_AIM_STEALTH,			ACT_IDLE_STEALTH_PISTOL,		false },

	{ ACT_WALK_AIM_RELAXED,			ACT_WALK_DUAL_PISTOLS_RELAXED,		false },//never aims
	{ ACT_WALK_AIM_STIMULATED,		ACT_WALK_AIM_DUAL_PISTOLS,			false },
	{ ACT_WALK_AIM_AGITATED,		ACT_WALK_AIM_DUAL_PISTOLS,			false },//always aims
	{ ACT_WALK_AIM_STEALTH,			ACT_WALK_AIM_STEALTH_PISTOL,	false },//always aims

	{ ACT_RUN_AIM_RELAXED,			ACT_RUN_DUAL_PISTOLS_RELAXED,			false },//never aims
	{ ACT_RUN_AIM_STIMULATED,		ACT_RUN_AIM_DUAL_PISTOLS,				false },
	{ ACT_RUN_AIM_AGITATED,			ACT_RUN_AIM_DUAL_PISTOLS,				false },//always aims
	{ ACT_RUN_AIM_STEALTH,			ACT_RUN_AIM_STEALTH_PISTOL,		false },//always aims
	//End readiness activities

	// Crouch activities
	{ ACT_CROUCHIDLE_STIMULATED,	ACT_CROUCHIDLE_STIMULATED,		false },
	{ ACT_CROUCHIDLE_AIM_STIMULATED,ACT_RANGE_AIM_DUAL_PISTOLS_LOW,		false },//always aims
	{ ACT_CROUCHIDLE_AGITATED,		ACT_RANGE_AIM_DUAL_PISTOLS_LOW,		false },//always aims

	// Readiness translations
	{ ACT_READINESS_RELAXED_TO_STIMULATED, ACT_READINESS_PISTOL_RELAXED_TO_STIMULATED, false },
	{ ACT_READINESS_RELAXED_TO_STIMULATED_WALK, ACT_READINESS_PISTOL_RELAXED_TO_STIMULATED_WALK, false },
	{ ACT_READINESS_AGITATED_TO_STIMULATED, ACT_READINESS_PISTOL_AGITATED_TO_STIMULATED, false },
	{ ACT_READINESS_STIMULATED_TO_RELAXED, ACT_READINESS_PISTOL_STIMULATED_TO_RELAXED, false },

	//{ ACT_WALK_CROUCH,				ACT_WALK_CROUCH_PISTOL,			true },
	//{ ACT_WALK_CROUCH_AIM,			ACT_WALK_CROUCH_AIM_PISTOL,		true },
	//{ ACT_RUN_CROUCH,				ACT_RUN_CROUCH_PISTOL,			true },
	//{ ACT_RUN_CROUCH_AIM,			ACT_RUN_CROUCH_AIM_PISTOL,		true },

	{ ACT_RANGE_AIM_MED,			ACT_RANGE_AIM_DUAL_PISTOLS_MED,			false },
	{ ACT_RANGE_ATTACK1_MED,		ACT_RANGE_ATTACK_DUAL_PISTOLS_MED,		false },

	//{ ACT_COVER_WALL_R,			ACT_COVER_WALL_R_DUAL_PISTOLS,		false },
	//{ ACT_COVER_WALL_L,			ACT_COVER_WALL_L_DUAL_PISTOLS,		false },
	//{ ACT_COVER_WALL_LOW_R,		ACT_COVER_WALL_LOW_R_DUAL_PISTOLS,	false },
	//{ ACT_COVER_WALL_LOW_L,		ACT_COVER_WALL_LOW_L_DUAL_PISTOLS,	false },

	// HL2:DM activities (for third-person animations in SP)
	{ ACT_HL2MP_IDLE,                    ACT_HL2MP_IDLE_DUAL_PISTOLS,                    false },
	{ ACT_HL2MP_RUN,                    ACT_HL2MP_RUN_DUAL_PISTOLS,                    false },
	{ ACT_HL2MP_IDLE_CROUCH,            ACT_HL2MP_IDLE_CROUCH_DUAL_PISTOLS,            false },
	{ ACT_HL2MP_WALK_CROUCH,            ACT_HL2MP_WALK_CROUCH_DUAL_PISTOLS,            false },
	{ ACT_HL2MP_GESTURE_RANGE_ATTACK,    ACT_HL2MP_GESTURE_RANGE_ATTACK_DUAL_PISTOLS,    false },
	{ ACT_HL2MP_GESTURE_RELOAD,            ACT_HL2MP_GESTURE_RELOAD_DUAL_PISTOLS,        false },
	{ ACT_HL2MP_JUMP,                    ACT_HL2MP_JUMP_DUAL_PISTOLS,                    false },
	{ ACT_HL2MP_WALK,					ACT_HL2MP_WALK_DUAL_PISTOLS,						false },
	{ ACT_HL2MP_GESTURE_RANGE_ATTACK2,	ACT_HL2MP_GESTURE_RANGE_ATTACK2_DUAL_PISTOLS,		false },
};

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
Activity CBaseHLCombatWeapon::ActivityOverride( Activity baseAct, bool *pRequired )
{
	if (GetLeftHandGun())
	{
		acttable_t *pTable = m_dual_acttable;
		int actCount = ARRAYSIZE( m_dual_acttable );

		for (int i = 0; i < actCount; i++, pTable++)
		{
			if (baseAct == pTable->baseAct)
			{
				if (pRequired)
				{
					*pRequired = pTable->required;
				}
				return (Activity)pTable->weaponAct;
			}
		}
	}

	return BaseClass::ActivityOverride( baseAct, pRequired );
}
#endif

float	g_lateralBob;
float	g_verticalBob;

#if defined( CLIENT_DLL ) && ( !defined( HL2MP ) && !defined( PORTAL ) )

#define	HL2_BOB_CYCLE_MIN	1.0f
#define	HL2_BOB_CYCLE_MAX	0.45f
#define	HL2_BOB			0.002f
#define	HL2_BOB_UP		0.5f


static ConVar	cl_bobcycle( "cl_bobcycle","0.8" );
static ConVar	cl_bob( "cl_bob","0.002" );
static ConVar	cl_bobup( "cl_bobup","0.5" );

// Register these cvars if needed for easy tweaking
static ConVar	v_iyaw_cycle( "v_iyaw_cycle", "2"/*, FCVAR_UNREGISTERED*/ );
static ConVar	v_iroll_cycle( "v_iroll_cycle", "0.5"/*, FCVAR_UNREGISTERED*/ );
static ConVar	v_ipitch_cycle( "v_ipitch_cycle", "1"/*, FCVAR_UNREGISTERED*/ );
static ConVar	v_iyaw_level( "v_iyaw_level", "0.3"/*, FCVAR_UNREGISTERED*/ );
static ConVar	v_iroll_level( "v_iroll_level", "0.1"/*, FCVAR_UNREGISTERED*/ );
static ConVar	v_ipitch_level( "v_ipitch_level", "0.3"/*, FCVAR_UNREGISTERED*/ );

//-----------------------------------------------------------------------------
// Purpose: 
// Output : float
//-----------------------------------------------------------------------------
float CBaseHLCombatWeapon::CalcViewmodelBob( void )
{
	static	float bobtime;
	static	float lastbobtime;
	float	cycle;
	
	CBasePlayer *player = ToBasePlayer( GetOwner() );
	//Assert( player );

	//NOTENOTE: For now, let this cycle continue when in the air, because it snaps badly without it

	if ( ( !gpGlobals->frametime ) || ( player == NULL ) )
	{
		//NOTENOTE: We don't use this return value in our case (need to restructure the calculation function setup!)
		return 0.0f;// just use old value
	}

	//Find the speed of the player
	float speed = player->GetLocalVelocity().Length2D();

	//FIXME: This maximum speed value must come from the server.
	//		 MaxSpeed() is not sufficient for dealing with sprinting - jdw

	speed = clamp( speed, -320, 320 );

	float bob_offset = RemapVal( speed, 0, 320, 0.0f, 1.0f );
	
	bobtime += ( gpGlobals->curtime - lastbobtime ) * bob_offset;
	lastbobtime = gpGlobals->curtime;

	//Calculate the vertical bob
	cycle = bobtime - (int)(bobtime/HL2_BOB_CYCLE_MAX)*HL2_BOB_CYCLE_MAX;
	cycle /= HL2_BOB_CYCLE_MAX;

	if ( cycle < HL2_BOB_UP )
	{
		cycle = M_PI * cycle / HL2_BOB_UP;
	}
	else
	{
		cycle = M_PI + M_PI*(cycle-HL2_BOB_UP)/(1.0 - HL2_BOB_UP);
	}
	
	g_verticalBob = speed*0.005f;
	g_verticalBob = g_verticalBob*0.3 + g_verticalBob*0.7*sin(cycle);

	g_verticalBob = clamp( g_verticalBob, -7.0f, 4.0f );

	//Calculate the lateral bob
	cycle = bobtime - (int)(bobtime/HL2_BOB_CYCLE_MAX*2)*HL2_BOB_CYCLE_MAX*2;
	cycle /= HL2_BOB_CYCLE_MAX*2;

	if ( cycle < HL2_BOB_UP )
	{
		cycle = M_PI * cycle / HL2_BOB_UP;
	}
	else
	{
		cycle = M_PI + M_PI*(cycle-HL2_BOB_UP)/(1.0 - HL2_BOB_UP);
	}

	g_lateralBob = speed*0.005f;
	g_lateralBob = g_lateralBob*0.3 + g_lateralBob*0.7*sin(cycle);
	g_lateralBob = clamp( g_lateralBob, -7.0f, 4.0f );

#ifdef MAPBASE
	if (GetBobScale() != 1.0f)
	{
		//g_verticalBob *= GetBobScale();
		g_lateralBob *= GetBobScale();
	}
#endif
	
	//NOTENOTE: We don't use this return value in our case (need to restructure the calculation function setup!)
	return 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &origin - 
//			&angles - 
//			viewmodelindex - 
//-----------------------------------------------------------------------------
void CBaseHLCombatWeapon::AddViewmodelBob( CBaseViewModel *viewmodel, Vector &origin, QAngle &angles )
{
	Vector	forward, right;
	AngleVectors( angles, &forward, &right, NULL );

	CalcViewmodelBob();

	// Apply bob, but scaled down to 40%
	VectorMA( origin, g_verticalBob * 0.1f, forward, origin );
	
	// Z bob a bit more
	origin[2] += g_verticalBob * 0.1f;
	
	// bob the angles
	angles[ ROLL ]	+= g_verticalBob * 0.5f;
	angles[ PITCH ]	-= g_verticalBob * 0.4f;

	angles[ YAW ]	-= g_lateralBob  * 0.3f;

	VectorMA( origin, g_lateralBob * 0.8f, right, origin );
}

//-----------------------------------------------------------------------------
Vector CBaseHLCombatWeapon::GetBulletSpread( WeaponProficiency_t proficiency )
{
	return BaseClass::GetBulletSpread( proficiency );
}

//-----------------------------------------------------------------------------
float CBaseHLCombatWeapon::GetSpreadBias( WeaponProficiency_t proficiency )
{
	return BaseClass::GetSpreadBias( proficiency );
}
//-----------------------------------------------------------------------------

const WeaponProficiencyInfo_t *CBaseHLCombatWeapon::GetProficiencyValues()
{
	return NULL;
}

#else

// Server stubs
float CBaseHLCombatWeapon::CalcViewmodelBob( void )
{
	return 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &origin - 
//			&angles - 
//			viewmodelindex - 
//-----------------------------------------------------------------------------
void CBaseHLCombatWeapon::AddViewmodelBob( CBaseViewModel *viewmodel, Vector &origin, QAngle &angles )
{
}


//-----------------------------------------------------------------------------
Vector CBaseHLCombatWeapon::GetBulletSpread( WeaponProficiency_t proficiency )
{
	Vector baseSpread = BaseClass::GetBulletSpread( proficiency );

	const WeaponProficiencyInfo_t *pProficiencyValues = GetProficiencyValues();
	float flModifier = (pProficiencyValues)[ proficiency ].spreadscale;
	return ( baseSpread * flModifier );
}

//-----------------------------------------------------------------------------
float CBaseHLCombatWeapon::GetSpreadBias( WeaponProficiency_t proficiency )
{
	const WeaponProficiencyInfo_t *pProficiencyValues = GetProficiencyValues();
	return (pProficiencyValues)[ proficiency ].bias;
}

//-----------------------------------------------------------------------------
const WeaponProficiencyInfo_t *CBaseHLCombatWeapon::GetProficiencyValues()
{
	return GetDefaultProficiencyValues();
}

//-----------------------------------------------------------------------------
const WeaponProficiencyInfo_t *CBaseHLCombatWeapon::GetDefaultProficiencyValues()
{
	// Weapon proficiency table. Keep this in sync with WeaponProficiency_t enum in the header!!
	static WeaponProficiencyInfo_t g_BaseWeaponProficiencyTable[] =
	{
		{ 2.50, 1.0	},
		{ 2.00, 1.0	},
		{ 1.50, 1.0	},
		{ 1.25, 1.0 },
		{ 1.00, 1.0	},
	};

	COMPILE_TIME_ASSERT( ARRAYSIZE(g_BaseWeaponProficiencyTable) == WEAPON_PROFICIENCY_PERFECT + 1);

	return g_BaseWeaponProficiencyTable;
}

#endif
