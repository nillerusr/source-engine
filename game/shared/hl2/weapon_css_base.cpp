//=============================================================================//
//
// Purpose: CS:S weapons recreated from scratch in Source SDK 2013 for usage in a Half-Life 2 setting.
//
// Author: Blixibon
//
//=============================================================================//

#include "cbase.h"
#include "npcevent.h"
#include "weapon_css_base.h"
#include "gamerules.h"
#include "gamestats.h"
#include "ammodef.h"
#ifdef CLIENT_DLL
#else
#include "basecombatcharacter.h"
#include "ai_basenpc.h"
#include "player.h"
#endif

#define	PISTOL_ACCURACY_SHOT_PENALTY_TIME		0.2f	// Applied amount of time each shot adds to the time we must recover from
#define	PISTOL_ACCURACY_MAXIMUM_PENALTY_TIME	1.5f	// Maximum penalty to deal out

#define SOUNDENT_VOLUME_PISTOL_CSS		IsSilenced() ? 500.0 : 1500.0

//-----------------------------------------------------------------------------
// CBase_CSS_HL2_Pistol
//-----------------------------------------------------------------------------
BEGIN_DATADESC( CBase_CSS_HL2_Pistol )

	DEFINE_FIELD( m_flSoonestPrimaryAttack, FIELD_TIME ),
	DEFINE_FIELD( m_flLastAttackTime,		FIELD_TIME ),
	DEFINE_FIELD( m_flAccuracyPenalty,		FIELD_FLOAT ), //NOTENOTE: This is NOT tracking game time
	DEFINE_FIELD( m_nNumShotsFired,			FIELD_INTEGER ),

	DEFINE_CSS_WEAPON_DATADESC()

END_DATADESC()

IMPLEMENT_NETWORKCLASS_DT( CBase_CSS_HL2_Pistol, DT_Base_CSS_HL2_Pistol )

#ifdef CLIENT_DLL
	RecvPropTime( RECVINFO( m_flSoonestPrimaryAttack ) ),
	RecvPropTime( RECVINFO( m_flLastAttackTime ) ),
	RecvPropFloat( RECVINFO( m_flAccuracyPenalty ) ),
	RecvPropInt( RECVINFO( m_nNumShotsFired ) ),
#else
	SendPropTime( SENDINFO( m_flSoonestPrimaryAttack ) ),
	SendPropTime( SENDINFO( m_flLastAttackTime ) ),
	SendPropFloat( SENDINFO( m_flAccuracyPenalty ) ),
	SendPropInt( SENDINFO( m_nNumShotsFired ) ),
#endif

	DEFINE_CSS_WEAPON_NETWORK_TABLE()

END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CBase_CSS_HL2_Pistol )

	DEFINE_PRED_FIELD( m_flSoonestPrimaryAttack, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_flLastAttackTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_flAccuracyPenalty, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_nNumShotsFired, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),

END_PREDICTION_DATA()
#endif

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CBase_CSS_HL2_Pistol::CBase_CSS_HL2_Pistol( void )
{
	m_flSoonestPrimaryAttack = gpGlobals->curtime;
	m_flAccuracyPenalty = 0.0f;

	m_fMinRange1		= 24;
	m_fMaxRange1		= 1500;
	m_fMinRange2		= 24;
	m_fMaxRange2		= 200;

	m_bFiresUnderwater	= true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBase_CSS_HL2_Pistol::Precache( void )
{
	BaseClass::Precache();
}

#ifndef CLIENT_DLL
//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CBase_CSS_HL2_Pistol::Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator )
{
	switch( pEvent->event )
	{
		case EVENT_WEAPON_PISTOL_FIRE:
		{
			Vector vecShootOrigin, vecShootDir;
			vecShootOrigin = pOperator->Weapon_ShootPosition();

			CAI_BaseNPC *npc = pOperator->MyNPCPointer();
			ASSERT( npc != NULL );

			vecShootDir = npc->GetActualShootTrajectory( vecShootOrigin );

			FireNPCPrimaryAttack( pOperator, vecShootOrigin, vecShootDir );
		}
		break;
		default:
			BaseClass::Operator_HandleAnimEvent( pEvent, pOperator );
			break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBase_CSS_HL2_Pistol::FireNPCPrimaryAttack( CBaseCombatCharacter *pOperator, Vector &vecShootOrigin, Vector &vecShootDir )
{
	CSoundEnt::InsertSound( SOUND_COMBAT|SOUND_CONTEXT_GUNFIRE, pOperator->GetAbsOrigin(), SOUNDENT_VOLUME_PISTOL_CSS, 0.2, pOperator, SOUNDENT_CHANNEL_WEAPON, pOperator->GetEnemy() );

	WeaponSound( SINGLE_NPC );

	FireBulletsInfo_t info( 1, vecShootOrigin, vecShootDir, VECTOR_CONE_PRECALCULATED, MAX_TRACE_LENGTH, m_iPrimaryAmmoType );
	info.m_iTracerFreq = 2;

	if (GetNPCDamageMultiplier() != 1.0f)
	{
		info.m_flDamage = round( GetAmmoDef()->NPCDamage( info.m_iAmmoType ) * GetNPCDamageMultiplier() );
		info.m_nFlags |= FIRE_BULLETS_NO_AUTO_GIB_TYPE;
	}

	pOperator->FireBullets( info );

	pOperator->DoMuzzleFlash();
	m_iClip1 = m_iClip1 - 1;
}

//-----------------------------------------------------------------------------
// Purpose: Some things need this. (e.g. the new Force(X)Fire inputs or blindfire actbusy)
//-----------------------------------------------------------------------------
void CBase_CSS_HL2_Pistol::Operator_ForceNPCFire( CBaseCombatCharacter *pOperator, bool bSecondary )
{
	// Ensure we have enough rounds in the clip
	m_iClip1++;

	Vector vecShootOrigin, vecShootDir;
	QAngle	angShootDir;
	GetAttachment( LookupAttachment( "muzzle" ), vecShootOrigin, angShootDir );
	AngleVectors( angShootDir, &vecShootDir );
	FireNPCPrimaryAttack( pOperator, vecShootOrigin, vecShootDir );
}
#endif

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CBase_CSS_HL2_Pistol::DryFire( void )
{
	WeaponSound( EMPTY );
	SendWeaponAnim( ACT_VM_DRYFIRE );
	
	m_flSoonestPrimaryAttack	= gpGlobals->curtime + GetDryRefireRate();
	m_flNextPrimaryAttack		= gpGlobals->curtime + SequenceDuration();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CBase_CSS_HL2_Pistol::PrimaryAttack( void )
{
	if ( ( gpGlobals->curtime - m_flLastAttackTime ) > 0.5f )
	{
		m_nNumShotsFired = 0;
	}
	else
	{
		m_nNumShotsFired++;
	}

	m_flLastAttackTime = gpGlobals->curtime;
	m_flSoonestPrimaryAttack = gpGlobals->curtime + GetRefireRate();
#ifndef CLIENT_DLL
	CSoundEnt::InsertSound( SOUND_COMBAT, GetAbsOrigin(), SOUNDENT_VOLUME_PISTOL_CSS, 0.2, GetOwner() );
#endif

	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );

	if( pOwner )
	{
		// Each time the player fires the pistol, reset the view punch. This prevents
		// the aim from 'drifting off' when the player fires very quickly. This may
		// not be the ideal way to achieve this, but it's cheap and it works, which is
		// great for a feature we're evaluating. (sjb)
		pOwner->ViewPunchReset();
	}

	// If my clip is empty (and I use clips) start reload
	if ( UsesClipsForAmmo1() && !m_iClip1 ) 
	{
		Reload();
		return;
	}

	// 
	// Copied from base
	// 
	pOwner->DoMuzzleFlash();

	// HACKHACK: Don't repeat burst sequences from the glock
	if (!InBurst() || GetActivity() != ACT_VM_SECONDARYATTACK)
	{
		SendWeaponAnim( GetPrimaryAttackActivity() );
	}

	// player "shoot" animation
	pOwner->SetAnimation( PLAYER_ATTACK1 );

	FireBulletsInfo_t info;
	info.m_vecSrc	 = pOwner->Weapon_ShootPosition( );
	
	info.m_vecDirShooting = pOwner->GetAutoaimVector( AUTOAIM_SCALE_DEFAULT );

	// To make the firing framerate independent, we may have to fire more than one bullet here on low-framerate systems, 
	// especially if the weapon we're firing has a really fast rate of fire.
	info.m_iShots = 0;
	float fireRate = GetFireRate();

	while ( m_flNextPrimaryAttack <= gpGlobals->curtime )
	{
		// MUST call sound before removing a round from the clip of a CMachineGun
		WeaponSound(SINGLE, m_flNextPrimaryAttack);
		m_flNextPrimaryAttack = m_flNextPrimaryAttack + fireRate;
		info.m_iShots++;
		if ( !fireRate )
			break;
	}

	// Make sure we don't fire more than the amount in the clip
	if ( UsesClipsForAmmo1() )
	{
		info.m_iShots = MIN( info.m_iShots, m_iClip1 );
		m_iClip1 -= info.m_iShots;
	}
	else
	{
		info.m_iShots = MIN( info.m_iShots, pOwner->GetAmmoCount( m_iPrimaryAmmoType ) );
		pOwner->RemoveAmmo( info.m_iShots, m_iPrimaryAmmoType );
	}

	info.m_flDistance = MAX_TRACE_LENGTH;
	info.m_iAmmoType = m_iPrimaryAmmoType;

	if (GetDamageMultiplier() != 1.0f)
	{
		info.m_flDamage = round( GetAmmoDef()->PlrDamage( info.m_iAmmoType ) * GetDamageMultiplier() );
		info.m_nFlags |= FIRE_BULLETS_NO_AUTO_GIB_TYPE;
	}

	info.m_iTracerFreq = 2;

#if !defined( CLIENT_DLL )
	// Fire the bullets
	info.m_vecSpread = pOwner->GetAttackSpread( this );
#else
	//!!!HACKHACK - what does the client want this function for? 
	info.m_vecSpread = GetActiveWeapon()->GetBulletSpread();
#endif // CLIENT_DLL

	pOwner->FireBullets( info );

	if (!m_iClip1 && pOwner->GetAmmoCount(m_iPrimaryAmmoType) <= 0)
	{
		// HEV suit - indicate out of ammo condition
		pOwner->SetSuitUpdate("!HEV_AMO0", FALSE, 0);
	}

	//Add our view kick in
	AddViewKick();

	// 
	// End copied from base
	// 

	// Add an accuracy penalty which can move past our maximum penalty time if we're really spastic
	m_flAccuracyPenalty += PISTOL_ACCURACY_SHOT_PENALTY_TIME;

	m_iPrimaryAttacks++;
#ifndef CLIENT_DLL
	gamestats->Event_WeaponFired( pOwner, true, GetClassname() );
#endif
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CBase_CSS_HL2_Pistol::SecondaryAttack( void )
{
	BaseClass::SecondaryAttack();

	if (CanToggleSilencer() && m_flNextSecondaryAttack <= gpGlobals->curtime)
	{
		// The base toggles the silencer in this case
		m_flSoonestPrimaryAttack = gpGlobals->curtime + SequenceDuration();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBase_CSS_HL2_Pistol::UpdatePenaltyTime( void )
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );

	if ( pOwner == NULL )
		return;

	// Check our penalty time decay
	if ( ( ( pOwner->m_nButtons & IN_ATTACK ) == false ) && ( m_flSoonestPrimaryAttack < gpGlobals->curtime ) )
	{
		m_flAccuracyPenalty -= gpGlobals->frametime;
		m_flAccuracyPenalty = clamp( m_flAccuracyPenalty, 0.0f, PISTOL_ACCURACY_MAXIMUM_PENALTY_TIME );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBase_CSS_HL2_Pistol::ItemPreFrame( void )
{
	UpdatePenaltyTime();

	BaseClass::ItemPreFrame();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBase_CSS_HL2_Pistol::ItemBusyFrame( void )
{
	UpdatePenaltyTime();

	BaseClass::ItemBusyFrame();
}

//-----------------------------------------------------------------------------
// Purpose: Allows firing as fast as button is pressed
//-----------------------------------------------------------------------------
void CBase_CSS_HL2_Pistol::ItemPostFrame( void )
{
	BaseClass::ItemPostFrame();

	if ( m_bInReload )
		return;
	
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );

	if ( pOwner == NULL )
		return;

	//Allow a refire as fast as the player can click
	if ( ( ( pOwner->m_nButtons & IN_ATTACK ) == false ) && ( m_flSoonestPrimaryAttack < gpGlobals->curtime ) )
	{
		m_flNextPrimaryAttack = gpGlobals->curtime - 0.1f;
	}
	else if ( ( pOwner->m_nButtons & IN_ATTACK ) && ( m_flNextPrimaryAttack < gpGlobals->curtime ) && ( m_iClip1 <= 0 ) )
	{
		DryFire();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
Activity CBase_CSS_HL2_Pistol::GetPrimaryAttackActivity( void )
{
	return ACT_VM_PRIMARYATTACK;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CBase_CSS_HL2_Pistol::Reload( void )
{
	bool fRet = DefaultReload( GetMaxClip1(), GetMaxClip2(), ACT_VM_RELOAD );
	if ( fRet )
	{
		WeaponSound( RELOAD );
		m_flAccuracyPenalty = 0.0f;
	}
	return fRet;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBase_CSS_HL2_Pistol::AddViewKick( void )
{
	CBasePlayer *pPlayer  = ToBasePlayer( GetOwner() );
	
	if ( pPlayer == NULL )
		return;

	QAngle	viewPunch;

	viewPunch.x = -(GetViewKickBase() * (m_flAccuracyPenalty + 1.0f));
	viewPunch.y = (viewPunch.x * 0.15f);

	viewPunch.x += random->RandomFloat( -0.25f, -0.5f );
	viewPunch.y += random->RandomFloat( -.6f, .6f );
	viewPunch.z = 0.0f;

	//Add it to the view punch
	pPlayer->ViewPunch( viewPunch );
}

//-----------------------------------------------------------------------------
// CBase_CSS_HL2_MachineGun
//-----------------------------------------------------------------------------
BEGIN_DATADESC( CBase_CSS_HL2_MachineGun )

	DEFINE_CSS_WEAPON_DATADESC()

END_DATADESC()

IMPLEMENT_NETWORKCLASS_DT( CBase_CSS_HL2_MachineGun, DT_Base_CSS_HL2_MachineGun )

	DEFINE_CSS_WEAPON_NETWORK_TABLE()

END_NETWORK_TABLE()

//=========================================================
CBase_CSS_HL2_MachineGun::CBase_CSS_HL2_MachineGun( )
{
	m_fMinRange1 = 24;
	m_fMaxRange1 = 1500;
	m_fMinRange2 = 24;
	m_fMaxRange2 = 200;

	m_bAltFiresUnderwater = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBase_CSS_HL2_MachineGun::Precache( void )
{
	BaseClass::Precache();
}

#ifndef CLIENT_DLL
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBase_CSS_HL2_MachineGun::FireNPCPrimaryAttack( CBaseCombatCharacter *pOperator, Vector &vecShootOrigin, Vector &vecShootDir )
{
	// FIXME: use the returned number of bullets to account for >10hz firerate
	WeaponSoundRealtime( SINGLE_NPC );

	CSoundEnt::InsertSound( SOUND_COMBAT|SOUND_CONTEXT_GUNFIRE, pOperator->GetAbsOrigin(), SOUNDENT_VOLUME_MACHINEGUN, 0.2, pOperator, SOUNDENT_CHANNEL_WEAPON, pOperator->GetEnemy() );

	FireBulletsInfo_t info( 1, vecShootOrigin, vecShootDir, VECTOR_CONE_PRECALCULATED, MAX_TRACE_LENGTH, m_iPrimaryAmmoType );
	info.m_iTracerFreq = 2;

	if (GetNPCDamageMultiplier() != 1.0f)
	{
		info.m_flDamage = round( GetAmmoDef()->NPCDamage( info.m_iAmmoType ) * GetNPCDamageMultiplier() );
		info.m_nFlags |= FIRE_BULLETS_NO_AUTO_GIB_TYPE;
	}

	pOperator->FireBullets( info );

	pOperator->DoMuzzleFlash();
	m_iClip1 = m_iClip1 - 1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBase_CSS_HL2_MachineGun::Operator_ForceNPCFire( CBaseCombatCharacter *pOperator, bool bSecondary )
{
	// Ensure we have enough rounds in the clip
	m_iClip1++;

	Vector vecShootOrigin, vecShootDir;
	QAngle	angShootDir;
	GetAttachment( LookupAttachment( "muzzle" ), vecShootOrigin, angShootDir );
	AngleVectors( angShootDir, &vecShootDir );
	FireNPCPrimaryAttack( pOperator, vecShootOrigin, vecShootDir );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBase_CSS_HL2_MachineGun::Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator )
{
	switch( pEvent->event )
	{
	case EVENT_WEAPON_SMG1:
	case EVENT_WEAPON_AR2:
		{
			Vector vecShootOrigin, vecShootDir;
			QAngle angDiscard;

			// Support old style attachment point firing
			if ((pEvent->options == NULL) || (pEvent->options[0] == '\0') || (!pOperator->GetAttachment(pEvent->options, vecShootOrigin, angDiscard)))
			{
				vecShootOrigin = pOperator->Weapon_ShootPosition();
			}

			CAI_BaseNPC *npc = pOperator->MyNPCPointer();
			ASSERT( npc != NULL );
			vecShootDir = npc->GetActualShootTrajectory( vecShootOrigin );

			FireNPCPrimaryAttack( pOperator, vecShootOrigin, vecShootDir );
		}
		break;

	default:
		BaseClass::Operator_HandleAnimEvent( pEvent, pOperator );
		break;
	}
}
#endif

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Activity
//-----------------------------------------------------------------------------
Activity CBase_CSS_HL2_MachineGun::GetPrimaryAttackActivity( void )
{
	return ACT_VM_PRIMARYATTACK;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBase_CSS_HL2_MachineGun::AddViewKick( void )
{
#ifndef CLIENT_DLL // TODO: Client viewpunch
	float flEasyDampen = 0.5f;
	float flMaxVerticalKick = 1.0f;	//Degrees
	float flSlideLimit = 2.0f;	//Seconds
	
	//Get the view kick
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );

	if ( pPlayer == NULL )
		return;

	float flBase = GetViewKickBase();
	if (flBase != 0.0f)
	{
		//flEasyDampen += (flBase * 0.15f);
		flMaxVerticalKick += flBase;
		flSlideLimit += (flBase * 0.25f);
	}

	DoMachineGunKick( pPlayer, flEasyDampen, flMaxVerticalKick, m_fFireDuration, flSlideLimit );
#endif
}

//-----------------------------------------------------------------------------
const WeaponProficiencyInfo_t *CBase_CSS_HL2_MachineGun::GetProficiencyValues()
{
	static WeaponProficiencyInfo_t proficiencyTable[] =
	{
		{ 7.0,		0.75	},
		{ 5.00,		0.75	},
		{ 10.0/3.0, 0.75	},
		{ 5.0/3.0,	0.75	},
		{ 1.00,		1.0		},
	};

	COMPILE_TIME_ASSERT( ARRAYSIZE(proficiencyTable) == WEAPON_PROFICIENCY_PERFECT + 1);

	return proficiencyTable;
}

//-----------------------------------------------------------------------------
// CBase_CSS_HL2_Rifle
//-----------------------------------------------------------------------------

const WeaponProficiencyInfo_t *CBase_CSS_HL2_Rifle::GetProficiencyValues()
{
	static WeaponProficiencyInfo_t proficiencyTable[] =
	{
		{ 7.0,		0.75	},
		{ 5.00,		0.75	},
		{ 3.0,		0.85	},
		{ 5.0/3.0,	0.75	},
		{ 1.00,		1.0		},
	};

	COMPILE_TIME_ASSERT( ARRAYSIZE(proficiencyTable) == WEAPON_PROFICIENCY_PERFECT + 1);

	return proficiencyTable;
}

//-----------------------------------------------------------------------------
// CBase_CSS_HL2_SniperRifle
//-----------------------------------------------------------------------------
BEGIN_DATADESC( CBase_CSS_HL2_SniperRifle )

	DEFINE_KEYFIELD( m_bCanUseScope, FIELD_BOOLEAN, "CanUseScope" ),
	DEFINE_FIELD( m_nZoomLevel, FIELD_INTEGER ),

END_DATADESC()

IMPLEMENT_NETWORKCLASS_DT( CBase_CSS_HL2_SniperRifle, DT_Base_CSS_HL2_SniperRifle )

#ifdef CLIENT_DLL
	RecvPropBool( RECVINFO( m_bCanUseScope ) ),
	RecvPropInt( RECVINFO( m_nZoomLevel ) ),
#else
	SendPropBool( SENDINFO( m_bCanUseScope ) ),
	SendPropInt( SENDINFO( m_nZoomLevel ) ),
#endif

END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CBase_CSS_HL2_SniperRifle )

	DEFINE_PRED_FIELD( m_nZoomLevel, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),

END_PREDICTION_DATA()
#endif

//=========================================================
CBase_CSS_HL2_SniperRifle::CBase_CSS_HL2_SniperRifle( )
{
	m_fMinRange1		= 0;// No minimum range. 
	m_fMaxRange1		= 1400;

	m_bAltFiresUnderwater = false;

	m_bCanUseScope = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const WeaponProficiencyInfo_t *CBase_CSS_HL2_SniperRifle::GetProficiencyValues()
{
	static WeaponProficiencyInfo_t proficiencyTable[] =
	{
		{ 5.0,		0.75	},
		{ 3.00,		0.75	},
		{ 2.0,		0.75	},
		{ 1.5,		0.85	},
		{ 1.00,		1.0		},
	};

	COMPILE_TIME_ASSERT( ARRAYSIZE(proficiencyTable) == WEAPON_PROFICIENCY_PERFECT + 1);

	return proficiencyTable;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBase_CSS_HL2_SniperRifle::Precache( void )
{
	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBase_CSS_HL2_SniperRifle::AddViewKick( void )
{
	CBasePlayer *pPlayer  = ToBasePlayer( GetOwner() );
	
	if ( pPlayer == NULL )
		return;

	QAngle	viewPunch;

	viewPunch.x = -GetViewKickBase();
	viewPunch.y = (viewPunch.x * 0.15f);

	viewPunch.x += random->RandomFloat( -0.25f, -0.5f );
	viewPunch.y += random->RandomFloat( -.6f, .6f );
	viewPunch.z = 0.0f;

	//Add it to the view punch
	pPlayer->ViewPunch( viewPunch );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &info - 
//-----------------------------------------------------------------------------
void CBase_CSS_HL2_SniperRifle::FireBullets( const FireBulletsInfo_t &info )
{
	FireBulletsInfo_t newInfo = info;

	// Sniper rifles don't deal enough force, so allow an overridable scale
	newInfo.m_flDamageForceScale = GetDamageForceScale();

	BaseClass::FireBullets( newInfo );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBase_CSS_HL2_SniperRifle::ToggleZoom( void )
{
	BaseClass::ToggleZoom();

	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	
	if ( pPlayer == NULL )
		return;

	switch (m_nZoomLevel)
	{
		case 0:
			// Turn on zoom to level 1
			if ( pPlayer->SetFOV( this, GetZoom1FOV(), GetZoomRate() ) )
				m_nZoomLevel++;
			break;
		case 1:
			// Set zoom to level 2
			if ( pPlayer->SetFOV( this, GetZoom2FOV(), GetZoomRate() ) )
				m_nZoomLevel++;
			break;
		case 2:
			// Unzoom
			if ( pPlayer->SetFOV( this, 0, GetUnZoomRate() ) )
				m_nZoomLevel = 0;
			break;
	}

	WeaponSound( SPECIAL3 );

	if (m_nZoomLevel > 0)
	{
		// Turn on scope
	}
	else
	{
		// Turn off scope
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBase_CSS_HL2_SniperRifle::StopZoom( void )
{
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	
	if ( pPlayer == NULL )
		return;

	// Unzoom
	if ( pPlayer->SetFOV( this, 0, GetUnZoomRate() ) )
		m_nZoomLevel = 0;
}

//-----------------------------------------------------------------------------
// CBase_CSS_HL2_Shotgun
//-----------------------------------------------------------------------------
BEGIN_DATADESC( CBase_CSS_HL2_Shotgun )

	DEFINE_FIELD( m_bNeedPump, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bDelayedFire1, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bDelayedFire2, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bDelayedReload, FIELD_BOOLEAN ),

	DEFINE_CSS_WEAPON_DATADESC()

END_DATADESC()

IMPLEMENT_NETWORKCLASS_DT( CBase_CSS_HL2_Shotgun, DT_Base_CSS_HL2_Shotgun )

#ifdef CLIENT_DLL
	RecvPropBool( RECVINFO( m_bNeedPump ) ),
	RecvPropBool( RECVINFO( m_bDelayedFire1 ) ),
	RecvPropBool( RECVINFO( m_bDelayedFire2 ) ),
	RecvPropBool( RECVINFO( m_bDelayedReload ) ),
#else
	SendPropBool( SENDINFO( m_bNeedPump ) ),
	SendPropBool( SENDINFO( m_bDelayedFire1 ) ),
	SendPropBool( SENDINFO( m_bDelayedFire2 ) ),
	SendPropBool( SENDINFO( m_bDelayedReload ) ),
#endif

	DEFINE_CSS_WEAPON_NETWORK_TABLE()

END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CBase_CSS_HL2_Shotgun )

	DEFINE_PRED_FIELD( m_bNeedPump, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bDelayedFire1, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bDelayedFire2, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bDelayedReload, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),

END_PREDICTION_DATA()
#endif

extern ConVar sk_auto_reload_time;

//=========================================================
CBase_CSS_HL2_Shotgun::CBase_CSS_HL2_Shotgun( )
{
	m_bReloadsSingly = true;

	m_bNeedPump		= false;
	m_bDelayedFire1 = false;
	m_bDelayedFire2 = false;

	m_fMinRange1		= 0.0;
	m_fMaxRange1		= 500;
	m_fMinRange2		= 0.0;
	m_fMaxRange2		= 200;
}

#ifndef CLIENT_DLL
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pOperator - 
//-----------------------------------------------------------------------------
void CBase_CSS_HL2_Shotgun::FireNPCPrimaryAttack( CBaseCombatCharacter *pOperator, bool bUseWeaponAngles )
{
	Vector vecShootOrigin, vecShootDir;
	CAI_BaseNPC *npc = pOperator->MyNPCPointer();
	ASSERT( npc != NULL );
	WeaponSound( SINGLE_NPC );
	pOperator->DoMuzzleFlash();
	m_iClip1 = m_iClip1 - 1;

	if ( bUseWeaponAngles )
	{
		QAngle	angShootDir;
		GetAttachment( LookupAttachment( "muzzle" ), vecShootOrigin, angShootDir );
		AngleVectors( angShootDir, &vecShootDir );
	}
	else 
	{
		vecShootOrigin = pOperator->Weapon_ShootPosition();
		vecShootDir = npc->GetActualShootTrajectory( vecShootOrigin );
	}

	pOperator->FireBullets( GetNumPellets(), vecShootOrigin, vecShootDir, GetBulletSpread(), MAX_TRACE_LENGTH, m_iPrimaryAmmoType, 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBase_CSS_HL2_Shotgun::Operator_ForceNPCFire( CBaseCombatCharacter *pOperator, bool bSecondary )
{
	// Ensure we have enough rounds in the clip
	m_iClip1++;

	FireNPCPrimaryAttack( pOperator, true );
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CBase_CSS_HL2_Shotgun::Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator )
{
	switch( pEvent->event )
	{
		case EVENT_WEAPON_SHOTGUN_FIRE:
		{
			FireNPCPrimaryAttack( pOperator, false );
		}
		break;

		default:
			CBaseCombatWeapon::Operator_HandleAnimEvent( pEvent, pOperator );
			break;
	}
}
#endif

//-----------------------------------------------------------------------------
// Purpose:	Uses episodic shotgun rest time by default
//-----------------------------------------------------------------------------
float CBase_CSS_HL2_Shotgun::GetMinRestTime()
{
	return 1.2f;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
float CBase_CSS_HL2_Shotgun::GetMaxRestTime()
{
	return 1.5f;
}

//-----------------------------------------------------------------------------
// Purpose: Override so only reload one shell at a time
// Input  :
// Output :
//-----------------------------------------------------------------------------
bool CBase_CSS_HL2_Shotgun::StartReload( void )
{
	CBaseCombatCharacter *pOwner  = GetOwner();
	
	if ( pOwner == NULL )
		return false;

	if (pOwner->GetAmmoCount(m_iPrimaryAmmoType) <= 0)
		return false;

	if (m_iClip1 >= GetMaxClip1())
		return false;

	// If shotgun totally emptied then a pump animation is needed
	
	//NOTENOTE: This is kinda lame because the player doesn't get strong feedback on when the reload has finished,
	//			without the pump.  Technically, it's incorrect, but it's good for feedback...

	if (m_iClip1 <= 0 && CanPump())
	{
		m_bNeedPump = true;
	}

	int j = MIN(1, pOwner->GetAmmoCount(m_iPrimaryAmmoType));

	if (j <= 0)
		return false;

	SendWeaponAnim( ACT_SHOTGUN_RELOAD_START );

	// Make shotgun shell visible
	SetBodygroup(1,0);

	pOwner->m_flNextAttack = gpGlobals->curtime;
	m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration();

	if ( pOwner->IsPlayer() )
	{
		static_cast<CBasePlayer*>(pOwner)->SetAnimation( PLAYER_RELOAD );
	}

	m_bInReload = true;
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Override so only reload one shell at a time
// Input  :
// Output :
//-----------------------------------------------------------------------------
bool CBase_CSS_HL2_Shotgun::Reload( void )
{
	// Check that StartReload was called first
	if (!m_bInReload)
	{
		Warning("ERROR: Shotgun Reload called incorrectly!\n");
	}

	CBaseCombatCharacter *pOwner  = GetOwner();
	
	if ( pOwner == NULL )
		return false;

	if (pOwner->GetAmmoCount(m_iPrimaryAmmoType) <= 0)
		return false;

	if (m_iClip1 >= GetMaxClip1())
		return false;

	int j = MIN(1, pOwner->GetAmmoCount(m_iPrimaryAmmoType));

	if (j <= 0)
		return false;

	FillClip();
	// Play reload on different channel as otherwise steals channel away from fire sound
	WeaponSound(RELOAD);
	SendWeaponAnim( ACT_VM_RELOAD );

	pOwner->m_flNextAttack = gpGlobals->curtime;
	m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration();

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Play finish reload anim and fill clip
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CBase_CSS_HL2_Shotgun::FinishReload( void )
{
	// Make shotgun shell invisible
	SetBodygroup(1,1);

	CBaseCombatCharacter *pOwner  = GetOwner();
	
	if ( pOwner == NULL )
		return;

	m_bInReload = false;

	if (PumpsInOtherAnims())
		m_bNeedPump = false;

	// Finish reload animation
	SendWeaponAnim( ACT_SHOTGUN_RELOAD_FINISH );

	pOwner->m_flNextAttack = gpGlobals->curtime;
	m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration();
}

//-----------------------------------------------------------------------------
// Purpose: Play finish reload anim and fill clip
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CBase_CSS_HL2_Shotgun::FillClip( void )
{
	CBaseCombatCharacter *pOwner  = GetOwner();
	
	if ( pOwner == NULL )
		return;

	// Add them to the clip
	if ( pOwner->GetAmmoCount( m_iPrimaryAmmoType ) > 0 )
	{
		if ( Clip1() < GetMaxClip1() )
		{
			m_iClip1++;
			pOwner->RemoveAmmo( 1, m_iPrimaryAmmoType );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Play weapon pump anim
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CBase_CSS_HL2_Shotgun::Pump( void )
{
	CBaseCombatCharacter *pOwner  = GetOwner();

	if ( pOwner == NULL )
		return;
	
	m_bNeedPump = false;
	
	WeaponSound( SPECIAL1 );

	// Finish reload animation
	SendWeaponAnim( ACT_SHOTGUN_PUMP );

	pOwner->m_flNextAttack	= gpGlobals->curtime + SequenceDuration();
	m_flNextPrimaryAttack	= gpGlobals->curtime + SequenceDuration();
}

//-----------------------------------------------------------------------------
// Purpose: 
//
//
//-----------------------------------------------------------------------------
bool CBase_CSS_HL2_Shotgun::Deploy( void )
{
	bool bBase = BaseClass::Deploy();

	if (bBase)
	{
		if (PumpsInOtherAnims())
			m_bNeedPump = false;
	}

	return bBase;
}

//-----------------------------------------------------------------------------
// Purpose: 
//
//
//-----------------------------------------------------------------------------
void CBase_CSS_HL2_Shotgun::DryFire( void )
{
	WeaponSound(EMPTY);
	SendWeaponAnim( ACT_VM_DRYFIRE );
	
	m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration();
}

//-----------------------------------------------------------------------------
// Purpose: 
//
//
//-----------------------------------------------------------------------------
void CBase_CSS_HL2_Shotgun::PrimaryAttack( void )
{
	// Only the player fires this way so we can cast
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );

	if (!pPlayer)
	{
		return;
	}

	// MUST call sound before removing a round from the clip of a CMachineGun
	WeaponSound(SINGLE);

	pPlayer->DoMuzzleFlash();

	SendWeaponAnim( ACT_VM_PRIMARYATTACK );

	// player "shoot" animation
	pPlayer->SetAnimation( PLAYER_ATTACK1 );

	// Don't fire again until fire animation has completed
	m_flNextPrimaryAttack = gpGlobals->curtime + GetViewModelSequenceDuration();
	m_iClip1 -= 1;

	Vector	vecSrc		= pPlayer->Weapon_ShootPosition( );
	Vector	vecAiming	= pPlayer->GetAutoaimVector( AUTOAIM_SCALE_DEFAULT );	
	
	// Fire the bullets, and force the first shot to be perfectly accuracy
	FireBulletsInfo_t info( GetNumPellets(), vecSrc, vecAiming, GetBulletSpread(), MAX_TRACE_LENGTH, m_iPrimaryAmmoType );
	info.m_pAttacker = pPlayer;
	info.m_iTracerFreq = 0;
	info.m_nFlags |= FIRE_BULLETS_FIRST_SHOT_ACCURATE;

	pPlayer->FireBullets( info );
	
	pPlayer->ViewPunch( QAngle( random->RandomFloat( -2, -1 ), random->RandomFloat( -2, 2 ), 0 ) );

#ifndef CLIENT_DLL
	pPlayer->SetMuzzleFlashTime( gpGlobals->curtime + 1.0 );

	CSoundEnt::InsertSound( SOUND_COMBAT, GetAbsOrigin(), SOUNDENT_VOLUME_SHOTGUN, 0.2, GetOwner() );
#endif

	if (!m_iClip1 && pPlayer->GetAmmoCount(m_iPrimaryAmmoType) <= 0)
	{
		// HEV suit - indicate out of ammo condition
		pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0); 
	}

	if( m_iClip1 && CanPump() && !PumpsInOtherAnims() )
	{
		// pump so long as some rounds are left.
		m_bNeedPump = true;
	}

	m_iPrimaryAttacks++;
#ifndef CLIENT_DLL
	gamestats->Event_WeaponFired( pPlayer, true, GetClassname() );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//
//
//-----------------------------------------------------------------------------
void CBase_CSS_HL2_Shotgun::SecondaryAttack( void )
{
	// Only the player fires this way so we can cast
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );

	if (!pPlayer)
	{
		return;
	}

	pPlayer->m_nButtons &= ~IN_ATTACK2;
	// MUST call sound before removing a round from the clip of a CMachineGun
	WeaponSound(WPN_DOUBLE);

	pPlayer->DoMuzzleFlash();

	SendWeaponAnim( ACT_VM_SECONDARYATTACK );

	// player "shoot" animation
#if MAPBASE_VER_INT >= 7000
	pPlayer->SetAnimation( PLAYER_ATTACK2 );
#else
	pPlayer->SetAnimation( PLAYER_ATTACK1 );
#endif

	// Don't fire again until fire animation has completed
	m_flNextPrimaryAttack = gpGlobals->curtime + GetViewModelSequenceDuration();
	m_iClip1 -= 2;	// Shotgun uses same clip for primary and secondary attacks

	Vector vecSrc	 = pPlayer->Weapon_ShootPosition();
	Vector vecAiming = pPlayer->GetAutoaimVector( AUTOAIM_SCALE_DEFAULT );	

	// Fire the bullets
	FireBulletsInfo_t info( GetNumDoublePellets(), vecSrc, vecAiming, GetBulletSpread(), MAX_TRACE_LENGTH, m_iPrimaryAmmoType );
	info.m_pAttacker = pPlayer;
	info.m_iTracerFreq = 0;
	info.m_bPrimaryAttack = false;

	pPlayer->FireBullets( info );
	pPlayer->ViewPunch( QAngle(random->RandomFloat( -5, 5 ),0,0) );

#ifndef CLIENT_DLL
	pPlayer->SetMuzzleFlashTime( gpGlobals->curtime + 1.0 );

	CSoundEnt::InsertSound( SOUND_COMBAT, GetAbsOrigin(), SOUNDENT_VOLUME_SHOTGUN, 0.2 );
#endif

	if (!m_iClip1 && pPlayer->GetAmmoCount(m_iPrimaryAmmoType) <= 0)
	{
		// HEV suit - indicate out of ammo condition
		pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0); 
	}

	if( m_iClip1 && CanPump() && !PumpsInOtherAnims() )
	{
		// pump so long as some rounds are left.
		m_bNeedPump = true;
	}

	m_iSecondaryAttacks++;
#ifndef CLIENT_DLL
	gamestats->Event_WeaponFired( pPlayer, false, GetClassname() );
#endif
}
	
//-----------------------------------------------------------------------------
// Purpose: Override so shotgun can do mulitple reloads in a row
//-----------------------------------------------------------------------------
void CBase_CSS_HL2_Shotgun::ItemPostFrame( void )
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if (!pOwner)
	{
		return;
	}

	if (m_bInReload)
	{
		// If I'm primary firing and have one round stop reloading and fire
		if ((pOwner->m_nButtons & IN_ATTACK ) && (m_iClip1 >=1))
		{
			m_bInReload		= false;
			m_bNeedPump		= false;
			m_bDelayedFire1 = true;
		}
		// If I'm secondary firing and have one round stop reloading and fire
		else if ((pOwner->m_nButtons & IN_ATTACK2 ) && (m_iClip1 >=2))
		{
			m_bInReload		= false;
			m_bNeedPump		= false;
			m_bDelayedFire2 = true;
		}
		else if (m_flNextPrimaryAttack <= gpGlobals->curtime)
		{
			// If out of ammo end reload
			if (pOwner->GetAmmoCount(m_iPrimaryAmmoType) <=0)
			{
				FinishReload();
				return;
			}
			// If clip not full reload again
			if (m_iClip1 < GetMaxClip1())
			{
				Reload();
				return;
			}
			// Clip full, stop reloading
			else
			{
				FinishReload();
				return;
			}
		}
	}
	else
	{			
		// Make shotgun shell invisible
		SetBodygroup(1,1);
	}

	if ((m_bNeedPump) && (m_flNextPrimaryAttack <= gpGlobals->curtime))
	{
		Pump();
		return;
	}
	
	// Shotgun uses same timing and ammo for secondary attack
	if ((m_bDelayedFire2 || pOwner->m_nButtons & IN_ATTACK2)&&(m_flNextPrimaryAttack <= gpGlobals->curtime))
	{
		m_bDelayedFire2 = false;
		
		if ( (m_iClip1 <= 1 && UsesClipsForAmmo1()))
		{
			// If only one shell is left, do a single shot instead	
			if ( m_iClip1 == 1 )
			{
				PrimaryAttack();
			}
			else if (!pOwner->GetAmmoCount(m_iPrimaryAmmoType))
			{
				DryFire();
			}
			else
			{
				StartReload();
			}
		}

		// Fire underwater?
		else if (GetOwner()->GetWaterLevel() == 3 && m_bFiresUnderwater == false)
		{
			WeaponSound(EMPTY);
			m_flNextPrimaryAttack = gpGlobals->curtime + 0.2;
			return;
		}
		else
		{
			// If the firing button was just pressed, reset the firing time
			if ( pOwner->m_afButtonPressed & IN_ATTACK )
			{
				 m_flNextPrimaryAttack = gpGlobals->curtime;
			}
			SecondaryAttack();
		}
	}
	else if ( (m_bDelayedFire1 || pOwner->m_nButtons & IN_ATTACK) && m_flNextPrimaryAttack <= gpGlobals->curtime)
	{
		m_bDelayedFire1 = false;
		if ( (m_iClip1 <= 0 && UsesClipsForAmmo1()) || ( !UsesClipsForAmmo1() && !pOwner->GetAmmoCount(m_iPrimaryAmmoType) ) )
		{
			if (!pOwner->GetAmmoCount(m_iPrimaryAmmoType))
			{
				DryFire();
			}
			else
			{
				StartReload();
			}
		}
		// Fire underwater?
		else if (pOwner->GetWaterLevel() == 3 && m_bFiresUnderwater == false)
		{
			WeaponSound(EMPTY);
			m_flNextPrimaryAttack = gpGlobals->curtime + 0.2;
			return;
		}
		else
		{
			// If the firing button was just pressed, reset the firing time
			CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
			if ( pPlayer && pPlayer->m_afButtonPressed & IN_ATTACK )
			{
				 m_flNextPrimaryAttack = gpGlobals->curtime;
			}
			PrimaryAttack();
		}
	}

	if ( pOwner->m_nButtons & IN_RELOAD && UsesClipsForAmmo1() && !m_bInReload ) 
	{
		// reload when reload is pressed, or if no buttons are down and weapon is empty.
		StartReload();
	}
	else 
	{
		// no fire buttons down
		m_bFireOnEmpty = false;

		if ( !HasAnyAmmo() && m_flNextPrimaryAttack < gpGlobals->curtime ) 
		{
			// weapon isn't useable, switch.
			if ( !(GetWeaponFlags() & ITEM_FLAG_NOAUTOSWITCHEMPTY) && pOwner->SwitchToNextBestWeapon( this ) )
			{
				m_flNextPrimaryAttack = gpGlobals->curtime + 0.3;
				return;
			}
		}
		else
		{
			// weapon is useable. Reload if empty and weapon has waited as long as it has to after firing
			if ( m_iClip1 <= 0 && !(GetWeaponFlags() & ITEM_FLAG_NOAUTORELOAD) && m_flNextPrimaryAttack < gpGlobals->curtime )
			{
				if (StartReload())
				{
					// if we've successfully started to reload, we're done
					return;
				}
			}
		}

		WeaponIdle( );
		return;
	}

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBase_CSS_HL2_Shotgun::ItemHolsterFrame( void )
{
	// Must be player held
	if ( GetOwner() && GetOwner()->IsPlayer() == false )
		return;

	// We can't be active
	if ( GetOwner()->GetActiveWeapon() == this )
		return;

	// If it's been longer than three seconds, reload
	if ( ( gpGlobals->curtime - m_flHolsterTime ) > sk_auto_reload_time.GetFloat() )
	{
		// Reset the timer
		m_flHolsterTime = gpGlobals->curtime;
	
		if ( GetOwner() == NULL )
			return;

		if ( m_iClip1 == GetMaxClip1() )
			return;

		// Just load the clip with no animations
		int ammoFill = MIN( (GetMaxClip1() - m_iClip1), GetOwner()->GetAmmoCount( GetPrimaryAmmoType() ) );
		
		GetOwner()->RemoveAmmo( ammoFill, GetPrimaryAmmoType() );
		m_iClip1 += ammoFill;
	}
}
