//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "hl1_basecombatweapon_shared.h"

#include "hl1_player_shared.h"

LINK_ENTITY_TO_CLASS( basehl1combatweapon, CBaseHL1CombatWeapon );

IMPLEMENT_NETWORKCLASS_ALIASED( BaseHL1CombatWeapon , DT_BaseHL1CombatWeapon )

BEGIN_NETWORK_TABLE( CBaseHL1CombatWeapon , DT_BaseHL1CombatWeapon )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CBaseHL1CombatWeapon )
END_PREDICTION_DATA()


void CBaseHL1CombatWeapon::Spawn( void )
{
	Precache();

	SetSolid( SOLID_BBOX );
	m_flNextEmptySoundTime = 0.0f;

	// Weapons won't show up in trace calls if they are being carried...
	RemoveEFlags( EFL_USE_PARTITION_WHEN_NOT_SOLID );

	m_iState = WEAPON_NOT_CARRIED;
	// Assume 
	m_nViewModelIndex = 0;

	// If I use clips, set my clips to the default
	if ( UsesClipsForAmmo1() )
	{
		m_iClip1 = GetDefaultClip1();
	}
	else
	{
		SetPrimaryAmmoCount( GetDefaultClip1() );
		m_iClip1 = WEAPON_NOCLIP;
	}
	if ( UsesClipsForAmmo2() )
	{
		m_iClip2 = GetDefaultClip2();
	}
	else
	{
		SetSecondaryAmmoCount( GetDefaultClip2() );
		m_iClip2 = WEAPON_NOCLIP;
	}

	SetModel( GetWorldModel() );

#if !defined( CLIENT_DLL )
	FallInit();
	SetCollisionGroup( COLLISION_GROUP_WEAPON );

	m_takedamage = DAMAGE_EVENTS_ONLY;

	SetBlocksLOS( false );

	// Default to non-removeable, because we don't want the
	// game_weapon_manager entity to remove weapons that have
	// been hand-placed by level designers. We only want to remove
	// weapons that have been dropped by NPC's.
	SetRemoveable( false );
#endif

	//Make weapons easier to pick up in MP.
	if ( g_pGameRules->IsMultiplayer() )
	{
		CollisionProp()->UseTriggerBounds( true, 36 );
	}
	else
	{
		CollisionProp()->UseTriggerBounds( true, 24 );
	}

	// Use more efficient bbox culling on the client. Otherwise, it'll setup bones for most
	// characters even when they're not in the frustum.
	AddEffects( EF_BONEMERGE_FASTCULL );
}

#if defined( CLIENT_DLL )

#define	HL1_BOB_CYCLE_MIN	1.0f
#define	HL1_BOB_CYCLE_MAX	0.45f
#define	HL1_BOB			0.002f
#define	HL1_BOB_UP		0.5f

float	g_lateralBob;
float	g_verticalBob;

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
float CBaseHL1CombatWeapon::CalcViewmodelBob( void )
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
	cycle = bobtime - (int)(bobtime/HL1_BOB_CYCLE_MAX)*HL1_BOB_CYCLE_MAX;
	cycle /= HL1_BOB_CYCLE_MAX;

	if ( cycle < HL1_BOB_UP )
	{
		cycle = M_PI * cycle / HL1_BOB_UP;
	}
	else
	{
		cycle = M_PI + M_PI*(cycle-HL1_BOB_UP)/(1.0 - HL1_BOB_UP);
	}
	
	g_verticalBob = speed*0.005f;
	g_verticalBob = g_verticalBob*0.3 + g_verticalBob*0.7*sin(cycle);

	g_verticalBob = clamp( g_verticalBob, -7.0f, 4.0f );

	//Calculate the lateral bob
	cycle = bobtime - (int)(bobtime/HL1_BOB_CYCLE_MAX*2)*HL1_BOB_CYCLE_MAX*2;
	cycle /= HL1_BOB_CYCLE_MAX*2;

	if ( cycle < HL1_BOB_UP )
	{
		cycle = M_PI * cycle / HL1_BOB_UP;
	}
	else
	{
		cycle = M_PI + M_PI*(cycle-HL1_BOB_UP)/(1.0 - HL1_BOB_UP);
	}

	g_lateralBob = speed*0.005f;
	g_lateralBob = g_lateralBob*0.3 + g_lateralBob*0.7*sin(cycle);
	g_lateralBob = clamp( g_lateralBob, -7.0f, 4.0f );
	
	//NOTENOTE: We don't use this return value in our case (need to restructure the calculation function setup!)
	return 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &origin - 
//			&angles - 
//			viewmodelindex - 
//-----------------------------------------------------------------------------
void CBaseHL1CombatWeapon::AddViewmodelBob( CBaseViewModel *viewmodel, Vector &origin, QAngle &angles )
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


#else

Vector CBaseHL1CombatWeapon::GetSoundEmissionOrigin() const
{
	if ( gpGlobals->maxClients == 1 || !GetOwner() )
		return CBaseCombatWeapon::GetSoundEmissionOrigin();

//	Vector vecOwner = GetOwner()->GetSoundEmissionOrigin();
//	Vector vecThis = WorldSpaceCenter();
//	DevMsg("SoundEmissionOrigin: Owner: %4.1f,%4.1f,%4.1f Default:%4.1f,%4.1f,%4.1f\n",
//			vecOwner.x, vecOwner.y, vecOwner.z,
//			vecThis.x, vecThis.y, vecThis.z );

	// TEMP fix for HL1MP... sounds are sometimes beeing emitted underneath the ground
	return GetOwner()->GetSoundEmissionOrigin();
}

#endif