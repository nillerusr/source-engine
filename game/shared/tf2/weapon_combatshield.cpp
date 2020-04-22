//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Combative shield weapon
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "basetfplayer_shared.h"
#include "weapon_twohandedcontainer.h"
#include "weapon_combatshield.h"
#include "engine/IEngineSound.h"
#include "in_buttons.h"
#include "weapon_combat_usedwithshieldbase.h"

#if !defined( CLIENT_DLL )
#include "tf_shield.h"

extern ConVar tf_knockdowntime;

#else

#include "iviewrender_beams.h"
#include "c_team.h"
#include "cdll_int.h"
#include "hudelement.h"
#include "bone_setup.h"
#include "beamdraw.h"
#include <vgui/ISurface.h>

#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Damage CVars
ConVar	weapon_combat_shield_rechargetime( "weapon_combat_shield_rechargetime","4", FCVAR_REPLICATED, "Time after taking damage before the shields starts recharging" );
ConVar	weapon_combat_shield_rechargeamount( "weapon_combat_shield_rechargeamount","0.03", FCVAR_REPLICATED, "Amount shield recharges every 10th of a second (must be an int)" );
ConVar	weapon_combat_shield_factor( "weapon_combat_shield_factor","0.4", FCVAR_REPLICATED, "Factor applied to damage the shield blocks" );

ConVar  weapon_combat_shield_teslaspeed( "weapon_combat_shield_teslaspeed", "0.025f", FCVAR_REPLICATED, "Speed of the tesla effect on the view model." );
ConVar	weapon_combat_shield_teslaskitter( "weapon_combat_shield_teslaskitter", "0.3f", FCVAR_REPLICATED, "Speed of the tesla skitter effect (percentage)." );
ConVar	weapon_combat_shield_teslaeffect( "weapon_combat_shield_teslaeffect", "4", FCVAR_REPLICATED, "Experimenting with effects." );

ConVar	weapon_combat_shield_health( "weapon_combat_shield_health", "100", FCVAR_REPLICATED, "Combat shield's maximum health." );

// HACK:  If we don't set this then we get a pop when transitioning into / out of idle animation
//  for commando_test model because the origin is wrong
// This can be removed once the model itself is fixed
#define SHIELD_FADOUT_TIME 0.2f

//-----------------------------------------------------------------------------
// Constructor, destructor: 
//-----------------------------------------------------------------------------
CWeaponCombatShield::CWeaponCombatShield()
{
	m_bAllowPostFrame = true;
	m_bHasShieldParry = false;
	m_flShieldHealth = 1.0;
#if defined( CLIENT_DLL )
	m_flFlashTimeEnd = 0;

	m_flTeslaSpeed = weapon_combat_shield_teslaspeed.GetFloat();
	m_flTeslaSkitter = weapon_combat_shield_teslaskitter.GetFloat();
	m_flTeslaLeftInc = 0.0f;
	m_flTeslaRightInc = 0.0f;
	m_pTeslaBeam = NULL;
	m_pTeslaBeam2 = NULL;

	m_flShieldInc = 1.0f;
	m_pShieldBeam = NULL;
	m_pShieldBeam2 = NULL;
	m_pShieldBeam3 = NULL;
#endif
	SetPredictionEligible( true );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponCombatShield::Precache( void )
{
	BaseClass::Precache();

	PrecacheModel( "sprites/blueflare1.vmt" );
	PrecacheModel( "sprites/physbeam.vmt" );

	PrecacheScriptSound( "WeaponCombatShield.TakeBash" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CWeaponCombatShield::Deploy( void )
{
	if ( BaseClass::Deploy() )
	{
		GainedNewTechnology(NULL);
		SetShieldState( SS_DOWN );
		return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Get the activity the other weapon in our twohanded container should play for this activity
//-----------------------------------------------------------------------------
int	CWeaponCombatShield::GetOtherWeaponsActivity( int iActivity )
{
	switch ( iActivity )
	{
	case ACT_VM_HAULBACK:
		return ACT_VM_DRAW;

	case ACT_VM_SECONDARYATTACK:
		return ACT_VM_HOLSTER;

	default:
		break;
	};

	return -1;
}

//-----------------------------------------------------------------------------
// Purpose: Get the activity the other weapon in our twohanded container should 
//			play instead of the one it's attempting to play.
//-----------------------------------------------------------------------------
int	CWeaponCombatShield::ReplaceOtherWeaponsActivity( int iActivity )
{
	switch ( iActivity )
	{
	case ACT_VM_IDLE:
		// If I'm active, don't let it idle
		if ( GetShieldState() != SS_DOWN )
			return -1;

	default:
		break;
	};

	return BaseClass::ReplaceOtherWeaponsActivity(iActivity);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CWeaponCombatShield::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	CBaseTFPlayer *player = ToBaseTFPlayer( GetOwner() );
	if ( player )
	{
		player->SetBlocking( false );
		player->SetParrying( false );

		if ( m_iShieldState != SS_DOWN && 
			 m_iShieldState != SS_UNAVAILABLE )
		{
			SetShieldState( SS_LOWERING );
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: I've been bashed by another player's shield
//-----------------------------------------------------------------------------
bool CWeaponCombatShield::TakeShieldBash( CBaseTFPlayer *pBasher )
{
	CBaseTFPlayer *pOwner = ToBaseTFPlayer( GetOwner() );
	if ( !pOwner )
		return false;

	// If I'm blocking, drop my block and prevent me from doing anything
	if ( GetShieldState() == SS_UP ||
		 GetShieldState() == SS_RAISING ||
		 GetShieldState() == SS_LOWERING )
	{
		// Make the shield unavailable
		SetShieldState( SS_UNAVAILABLE );
		SendWeaponAnim( ACT_VM_HITCENTER );

		m_flShieldUnavailableEndTime = gpGlobals->curtime + 2.0;

		// Play a sound	
		EmitSound( "WeaponCombatShield.TakeBash" );		
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponCombatShield::SetShieldState( int iShieldState )
{
	CBaseTFPlayer *pOwner = ToBaseTFPlayer( GetOwner() );
	if ( !pOwner )
		return;

	switch (iShieldState )
	{
	default:
	case SS_DOWN:
		pOwner->SetBlocking( false );
		m_flShieldUpStartTime = 0;
		m_flShieldParryEndTime = 0;
		m_flShieldUnavailableEndTime = 0;
		m_flShieldRaisedTime = 0.0f;
		m_flShieldLoweredTime = 0.0f;
		break;

	case SS_UP:
		SendWeaponAnim( ACT_VM_FIDGET );
		pOwner->SetBlocking( true );
		m_flShieldDownStartTime = 0.0f;
		m_flShieldParryEndTime = 0;
		m_flShieldUnavailableEndTime = 0;
		m_flShieldRaisedTime = 0.0f;
		m_flShieldLoweredTime = 0.0f;
		break;

	case SS_PARRYING:
		pOwner->SetBlocking( false );
		pOwner->SetParrying( true );
		m_flShieldParryEndTime = gpGlobals->curtime + PARRY_OPPORTUNITY_LENGTH;
		m_flShieldParrySwingEndTime = gpGlobals->curtime + 1.0f;	// a hack to make it look ok
		m_flShieldUnavailableEndTime = gpGlobals->curtime + SequenceDuration();
		m_flNextPrimaryAttack = m_flShieldUnavailableEndTime;
		m_flShieldRaisedTime = 0.0f;
		m_flShieldLoweredTime = 0.0f;
		break;

	case SS_PARRYING_FINISH_SWING:
		pOwner->SetBlocking( false );
		pOwner->SetParrying( false );
		break;

	case SS_UNAVAILABLE:
		SendWeaponAnim( ACT_VM_HAULBACK );
		pOwner->SetBlocking( false );
		pOwner->SetParrying( false );
		break;

	case SS_RAISING:
		{
			pOwner->SetBlocking( false );
			pOwner->SetParrying( false );
			m_flShieldRaisedTime = gpGlobals->curtime + SequenceDuration();
			m_flShieldUpStartTime = gpGlobals->curtime;
		}
		break;
	case SS_LOWERING:
		{
			SendWeaponAnim( ACT_VM_HAULBACK );
			pOwner->SetBlocking( false );
			pOwner->SetParrying( false );
			m_flShieldLoweredTime = gpGlobals->curtime + SequenceDuration();
			m_flShieldDownStartTime = gpGlobals->curtime;
		}
		break;
	};

	m_iShieldState = iShieldState;
}

//-----------------------------------------------------------------------------
// Purpose: Check to see if the shield state should change
//-----------------------------------------------------------------------------
void CWeaponCombatShield::UpdateShieldState( void )
{
	CBaseTFPlayer *pOwner = ToBaseTFPlayer( GetOwner() );
	if ( !pOwner )
		return;

	// Check to see if I should move out of the current state
	switch ( m_iShieldState )
	{
	default:
	case SS_DOWN:
	case SS_UP:
		break;

	case SS_RAISING:
			if ( gpGlobals->curtime > m_flShieldRaisedTime )
			{
				SetShieldState( SS_UP );
			}
			break;
	case SS_LOWERING:
		if ( gpGlobals->curtime > m_flShieldLoweredTime )
		{
			SetShieldState( SS_DOWN );
		}
		break;

	case SS_PARRYING:
		if ( gpGlobals->curtime > m_flShieldParryEndTime )
		{
			SetShieldState( SS_PARRYING_FINISH_SWING );
		}
		break;

	case SS_PARRYING_FINISH_SWING:
		if ( gpGlobals->curtime > m_flShieldParrySwingEndTime )
		{
			SetShieldState( SS_UNAVAILABLE );
		}
		break;

	case SS_UNAVAILABLE:
		if ( gpGlobals->curtime > m_flShieldUnavailableEndTime )
		{
			SetShieldState( SS_DOWN );
		}
		break;
	};
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CWeaponCombatShield::GetShieldState( void )
{
	return m_iShieldState;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeaponCombatShield::SetShieldUsable( bool bUsable )
{
	// Shutting down!
	if ( !bUsable )
	{
		if ( m_iShieldState == SS_UP || m_iShieldState == SS_RAISING )
		{
			// We've got our shield up, so drop it (play sound & animation).
			SendWeaponAnim( ACT_VM_HAULBACK );
			WeaponSound( SPECIAL2 );			
			SetShieldState( SS_LOWERING );			
		}
	}

	m_bUsable = bUsable;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CWeaponCombatShield::ShieldUsable( void )
{
	return m_bUsable;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : allow - 
//-----------------------------------------------------------------------------
void CWeaponCombatShield::SetAllowPostFrame( bool allow )
{
	m_bAllowPostFrame = allow;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponCombatShield::ItemPostFrame( void )
{
	if ( m_bAllowPostFrame )
	{
		ShieldPostFrame();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Allow the shield to interrupt reloads, etc.
//-----------------------------------------------------------------------------
void CWeaponCombatShield::ItemBusyFrame( void )
{
	if ( m_bAllowPostFrame )
	{
		ShieldPostFrame();
	}
}

//-----------------------------------------------------------------------------
// Purpose: The player holding this weapon has just gained new technology.
//-----------------------------------------------------------------------------
void CWeaponCombatShield::GainedNewTechnology( CBaseTechnology *pTechnology )
{
	BaseClass::GainedNewTechnology( pTechnology );

	CBaseTFPlayer *pPlayer = ToBaseTFPlayer( GetOwner() );
	if ( pPlayer )
	{
		// Has a parry?
		if ( pPlayer->HasNamedTechnology( "com_comboshield_parry" ) )
		{
			m_bHasShieldParry = true;
		}
		else
		{
			m_bHasShieldParry = false;
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Handle the shield input
//-----------------------------------------------------------------------------
void CWeaponCombatShield::ShieldPostFrame( void )
{
	CBaseTFPlayer *pOwner = ToBaseTFPlayer( GetOwner() );
	if (!pOwner)
		return;

	UpdateShieldState();
	CheckReload();

	// Store off EMP state
	bool isEMPed = IsOwnerEMPed();

	// If the shield's unavailable, just abort
	if ( GetShieldState() == SS_UNAVAILABLE )
		return;

	if ( m_flNextPrimaryAttack > gpGlobals->curtime )
		return;

	bool shieldRaised = ( GetShieldState() == SS_UP );
	bool shieldRaising = ( GetShieldState() == SS_RAISING );

	//	GetShieldState() == SS_LOWERING );

	// If my shield's out of power, I can't do anything with it
	if ( !GetShieldHealth() )
		return;

	// Was the shield button just pressed?
	if ( GetShieldState() == SS_DOWN && !isEMPed && pOwner->m_nButtons & IN_ATTACK2 )
	{
		// Play sound & anim
		WeaponSound( SPECIAL1 );
		SendWeaponAnim( ACT_VM_SECONDARYATTACK );

		SetShieldState( SS_RAISING );
		
		// Abort any reloads in progess
		pOwner->AbortReload();
	}
	else if ( ( shieldRaised || shieldRaising ) && !FBitSet( pOwner->m_nButtons, IN_ATTACK2 ) )
	{
		// Shield button was just released, check to see if we were parrying
		bool shouldParry = (gpGlobals->curtime < (m_flShieldUpStartTime + PARRY_DETECTION_TIME ));

		if ( m_bHasShieldParry && shouldParry )
		{
			// Parry!
			// Play sound & anim
			WeaponSound( SPECIAL2 );
			SendWeaponAnim( ACT_VM_SWINGHIT );

			SetShieldState( SS_PARRYING );

			// Bash enemies in front of me
			ShieldBash();
		}
		else
		{
			// Player's just lowered his shield
			// Play sound & anim
			WeaponSound( SPECIAL2 );
			SendWeaponAnim( ACT_VM_HAULBACK );

			SetShieldState( SS_LOWERING );
			m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration();
		}
	}
	else if ( GetShieldState() == SS_UP && ( pOwner->m_nButtons & IN_ATTACK2 ) && ( isEMPed ) )
	{
		// We've got our shield up, and we were just EMPed, so drop it
		// Play sound & anim
		SendWeaponAnim( ACT_VM_HAULBACK );
		WeaponSound( SPECIAL2 );

		SetShieldState( SS_LOWERING );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Bash enemies in front of me with my shield
//-----------------------------------------------------------------------------
void CWeaponCombatShield::ShieldBash( void )
{
#if 0
	// ROBIN: Disabled shield bash
	return;

	// Get any players in front of me
	CBaseTFPlayer *pOwner = ToBaseTFPlayer( GetOwner() );
	if ( !pOwner )
		return;

	// Get the target point and location
	Vector vecAiming;
	Vector vecSrc = pOwner->Weapon_ShootPosition( pOwner->GetOrigin() );	
	pOwner->EyeVectors( &vecAiming );

	// Find a player in range of this player, and make sure they're healable
	trace_t tr;
	Vector vecEnd = vecSrc + (vecAiming * SHIELD_BASH_RANGE);
	UTIL_TraceLine( vecSrc, vecEnd, MASK_SHOT, pOwner->edict(), COLLISION_GROUP_NONE, &tr);
	if (tr.fraction != 1.0)
	{
		CBaseEntity *pEntity = CBaseEntity::Instance(tr.u.ent);
		if ( pEntity )
		{
			CBaseTFPlayer *pPlayer = ToBaseTFPlayer( pEntity );
			if ( pPlayer && (pPlayer != pOwner) )
			{
				// Target needs to be on the eneny team
				if ( pPlayer->IsAlive() && !pPlayer->InSameTeam( pOwner ) )
				{
					// Ok, we have an enemy player
					pPlayer->TakeShieldBash( pOwner );
				}
			}
		}
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Attempt to block the incoming attack, and return the damage it 
//			should do after the block, if any.
//-----------------------------------------------------------------------------
float CWeaponCombatShield::AttemptToBlock( float flDamage )
{
	CBaseTFPlayer *pPlayer = ToBaseTFPlayer( GetOwner() );
	if ( !pPlayer || !weapon_combat_shield_factor.GetFloat() )
		return 0;

	// Block as much of the damage as we can
	float flPowerNeeded = flDamage * weapon_combat_shield_factor.GetFloat();
	flPowerNeeded = RemapVal( flPowerNeeded, 0, weapon_combat_shield_health.GetFloat(), 0, 1 );
	float flPowerUsed = MIN( flPowerNeeded, GetShieldHealth() );

#ifndef CLIENT_DLL
	RemoveShieldHealth( flPowerUsed );

	// Start recharging shortly after taking damage
	SetThink( ShieldRechargeThink );
	SetNextThink( gpGlobals->curtime + weapon_combat_shield_rechargetime.GetFloat() );
#endif

	// Failed to block it all?
	if ( flPowerUsed < flPowerNeeded )
	{
#ifndef CLIENT_DLL
		// Force the shield to drop if it's up
		if ( GetShieldState() == SS_UP )
		{
			// Play sound & anim
			SendWeaponAnim( ACT_VM_HAULBACK );
			WeaponSound( SPECIAL2 );
			SetShieldState( SS_LOWERING );
		}
#endif

		return ( flDamage - (flPowerUsed * (1.0 / weapon_combat_shield_factor.GetFloat())) );
	}

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CWeaponCombatShield::GetShieldHealth( void )
{
	return m_flShieldHealth;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponCombatShield::AddShieldHealth( float flHealth )
{
	m_flShieldHealth = MIN( 1.0, m_flShieldHealth + flHealth );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponCombatShield::RemoveShieldHealth( float flHealth )
{
	m_flShieldHealth = MAX( 0.0, m_flShieldHealth - flHealth );
}

//-----------------------------------------------------------------------------
// Purpose: Recharge the shield
//-----------------------------------------------------------------------------
void CWeaponCombatShield::ShieldRechargeThink( void )
{
// FIXME:
//xxx
#if !defined( CLIENT_DLL )
	if ( GetShieldHealth() >= 1.0 )
	{
		SetThink( NULL );
		return;
	}

	AddShieldHealth( weapon_combat_shield_rechargeamount.GetFloat() );
	SetNextThink( gpGlobals->curtime + 0.1f );
#endif
}


//====================================================================================
// WEAPON CLIENT HANDLING
//====================================================================================
int CWeaponCombatShield::UpdateClientData( CBasePlayer *pPlayer )
{
	if ( !pPlayer )
	{
		return BaseClass::UpdateClientData( pPlayer );
	}

	CWeaponTwoHandedContainer *pContainer = ( CWeaponTwoHandedContainer * )pPlayer->Weapon_OwnsThisType( "weapon_twohandedcontainer" );
	if ( !pContainer || pContainer != pPlayer->GetActiveWeapon() )
		return BaseClass::UpdateClientData( pPlayer );

	// Make sure this weapon is one of the container's active weapons
	if ( pContainer->GetLeftWeapon() != this && pContainer->GetRightWeapon() != this )
		return BaseClass::UpdateClientData( pPlayer );

	int retval = pContainer->UpdateClientData( pPlayer );
	m_iState =  pContainer->m_iState;
	return retval;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CWeaponCombatShield::VisibleInWeaponSelection( void )
{
	return false;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
bool CWeaponCombatShield::IsUp( void )
{
	return ( GetShieldState() == SS_UP );
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
float CWeaponCombatShield::GetRaisingTime( void )
{
	if ((GetShieldState() != SS_UP ) && (GetShieldState() != SS_RAISING))
		return 0.0f;

	return gpGlobals->curtime - m_flShieldUpStartTime;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
float CWeaponCombatShield::GetLoweringTime( void )
{
	if ((GetShieldState() != SS_DOWN ) && (GetShieldState() != SS_LOWERING))
		return 0.0f;

	return gpGlobals->curtime - m_flShieldDownStartTime;
}

#if defined( CLIENT_DLL )
//-----------------------------------------------------------------------------
// Purpose: Draw the ammo counts
//-----------------------------------------------------------------------------
void CWeaponCombatShield::DrawAmmo( void )
{
	// ROBIN: Removed this now that the shield colors itself to show health level
	return;

	int r, g, b, a;
	int x, y;

	// Get the shield power level
	float flPowerLevel = GetShieldHealth();
	float flInverseFactor = 1.0 - flPowerLevel;

	// Set our color
	gHUD.m_clrNormal.GetColor( r, g, b, a );
	
	int iWidth = XRES(12);
	int iHeight = YRES(64);

	x = XRES(548);
	y = ( ScreenHeight() - YRES(2) - iHeight );

	// Flashing the power level?
	float flFlash = 0;
	if ( gpGlobals->curtime < m_flFlashTimeEnd && !GetPrimaryAmmo() )
	{
		flFlash = fmod( gpGlobals->curtime, 0.25 );
		flFlash *= 2 * M_PI;
		flFlash = cos( flFlash );
	}

	// draw the exhausted portion of the bar.
	vgui::surface()->DrawSetColor( Color( r, g * flPowerLevel, b * flPowerLevel, 100 + (flFlash * 100) ) );
	vgui::surface()->DrawFilledRect( x, y, x + iWidth, y + iHeight * flInverseFactor );

	// draw the powerered portion of the bar
	vgui::surface()->DrawSetColor( Color( r, g * flPowerLevel, b * flPowerLevel, 190 ) );
	vgui::surface()->DrawFilledRect( x, y + iHeight * flInverseFactor, x + iWidth, y + iHeight * flInverseFactor + iHeight * flPowerLevel);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponCombatShield::GetViewmodelBoneControllers( C_BaseViewModel *pViewModel, float controllers[MAXSTUDIOBONECTRLS])
{
	// Dial shows the shield power level
	float flPowerLevel = 1.0;
	if ( GetShieldState() == SS_UP )
	{
		flPowerLevel = GetShieldHealth();
	}
	else if ( GetShieldState() == SS_RAISING )
	{
		// Bring the power up with the animation
		float flTotal = m_flShieldRaisedTime - m_flShieldUpStartTime;
		float flCurrent = (gpGlobals->curtime - m_flShieldUpStartTime);
		flPowerLevel = flCurrent / flTotal;
	}
	else if ( GetShieldState() == SS_LOWERING )
	{
		// Bring the power down with the animation
		float flTotal = (m_flShieldLoweredTime - m_flShieldDownStartTime);
		float flCurrent = (gpGlobals->curtime - m_flShieldDownStartTime);
		flPowerLevel = 1.0 - (flCurrent / flTotal);
	}

	// Make the middle point be full power (right of the middle being powered up)
	// Adjust a little for the perspective.
	flPowerLevel *= 0.55;

	// Add some shake
	flPowerLevel += RandomFloat( -0.02, 0.02 );
	controllers[0] = flPowerLevel;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CWeaponCombatShield::ViewModelDrawn( C_BaseViewModel *pViewModel )
{
	if ( m_iShieldState == SS_DOWN )
		return;

	DrawBeams( pViewModel );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeaponCombatShield::InitShieldBeam( void )
{
	BeamInfo_t beamInfo;

	beamInfo.m_vecStart.Init();
	beamInfo.m_vecEnd.Init();
	beamInfo.m_pszModelName = "sprites/physbeam.vmt";
	beamInfo.m_flHaloScale = 0.0f;
	beamInfo.m_flLife = 0.0f;
	beamInfo.m_flWidth = 2.0f;
	beamInfo.m_flEndWidth = 2.0f;
	beamInfo.m_flFadeLength = 0.0f;
	beamInfo.m_flAmplitude = 4.0f;
	beamInfo.m_flBrightness = 50.0f;
	beamInfo.m_flSpeed = 5.0f;
	beamInfo.m_nStartFrame = 0;
	beamInfo.m_flFrameRate = 0.0f;
	beamInfo.m_flRed = 255.0f;
	beamInfo.m_flGreen = 255.0f;
	beamInfo.m_flBlue = 128.0f;
	beamInfo.m_nSegments = 15;
	beamInfo.m_bRenderable = false;
	
	m_pShieldBeam = beams->CreateBeamPoints( beamInfo );

	beamInfo.m_vecStart.Init();
	beamInfo.m_vecEnd.Init();
	beamInfo.m_pszModelName = "sprites/physbeam.vmt";
	beamInfo.m_flHaloScale = 0.0f;
	beamInfo.m_flLife = 0.0f;
	beamInfo.m_flWidth = 1.5f;
	beamInfo.m_flEndWidth = 1.5f;
	beamInfo.m_flFadeLength = 0.0f;
	beamInfo.m_flAmplitude = 8.0f;
	beamInfo.m_flBrightness = 75.0f;
	beamInfo.m_flSpeed = 10.0f;
	beamInfo.m_nStartFrame = 0;
	beamInfo.m_flFrameRate = 0.0f;
	beamInfo.m_flRed = 255.0f;
	beamInfo.m_flGreen = 255.0f;
	beamInfo.m_flBlue = 128.0f;
	beamInfo.m_nSegments = 20;
	beamInfo.m_bRenderable = false;

	m_pShieldBeam2 = beams->CreateBeamPoints( beamInfo );

	beamInfo.m_vecStart.Init();
	beamInfo.m_vecEnd.Init();
	beamInfo.m_pszModelName = "sprites/physbeam.vmt";
	beamInfo.m_flHaloScale = 0.0f;
	beamInfo.m_flLife = 0.0f;
	beamInfo.m_flWidth = 3.0f;
	beamInfo.m_flEndWidth = 3.0f;
	beamInfo.m_flFadeLength = 0.0f;
	beamInfo.m_flAmplitude = 5.5f;
	beamInfo.m_flBrightness = 50.0f;
	beamInfo.m_flSpeed = 10.0f;
	beamInfo.m_nStartFrame = 0;
	beamInfo.m_flFrameRate = 0.0f;
	beamInfo.m_flRed = 255.0f;
	beamInfo.m_flGreen = 255.0f;
	beamInfo.m_flBlue = 128.0f;
	beamInfo.m_nSegments = 18;
	beamInfo.m_bRenderable = false;

	m_pShieldBeam3 = beams->CreateBeamPoints( beamInfo );	

	m_hShieldSpriteMaterial.Init( "sprites/blueflare1", TEXTURE_GROUP_CLIENT_EFFECTS );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeaponCombatShield::InitTeslaBeam( void )
{
	BeamInfo_t beamInfo;

	beamInfo.m_vecStart.Init();
	beamInfo.m_vecEnd.Init();
	beamInfo.m_pszModelName = "sprites/blueflare1.vmt";
	beamInfo.m_flHaloScale = 0.0f;
	beamInfo.m_flLife = 0.0f;
	beamInfo.m_flWidth = 1.0f;
	beamInfo.m_flEndWidth = 1.0f;
	beamInfo.m_flFadeLength = 0.0f;
	beamInfo.m_flAmplitude = 16.0f;
	beamInfo.m_flBrightness = 255.0f;
	beamInfo.m_flSpeed = 25.0f;
	beamInfo.m_nStartFrame = 0;
	beamInfo.m_flFrameRate = 0.0f;
	beamInfo.m_flRed = 206.0f;
	beamInfo.m_flGreen = 181.0f;
	beamInfo.m_flBlue = 127.0f;
	beamInfo.m_nSegments = 15;
	beamInfo.m_bRenderable = false;
	
	m_pTeslaBeam = beams->CreateBeamPoints( beamInfo );

	beamInfo.m_vecStart.Init();
	beamInfo.m_vecEnd.Init();
	beamInfo.m_pszModelName = "sprites/blueflare1.vmt";
	beamInfo.m_flHaloScale = 0.0f;
	beamInfo.m_flLife = 0.0f;
	beamInfo.m_flWidth = 1.0f;
	beamInfo.m_flEndWidth = 1.0f;
	beamInfo.m_flFadeLength = 0.0f;
	beamInfo.m_flAmplitude = 27.0f;
	beamInfo.m_flBrightness = 100.0f;
	beamInfo.m_flSpeed = 15.0f;
	beamInfo.m_nStartFrame = 0;
	beamInfo.m_flFrameRate = 0.0f;
	beamInfo.m_flRed = 206.0f;
	beamInfo.m_flGreen = 181.0f;
	beamInfo.m_flBlue = 127.0f;
	beamInfo.m_nSegments = 8;
	beamInfo.m_bRenderable = false;
	
	m_pTeslaBeam2 = beams->CreateBeamPoints( beamInfo );

	m_hTeslaSpriteMaterial.Init( "sprites/blueflare1", TEXTURE_GROUP_CLIENT_EFFECTS );
}

//-----------------------------------------------------------------------------
// Purpose: Render a tesla beam in the veiw model.
//
// NOTE: This is a big ugly mess that will get cleaned up when I nail down
//       the effect.
//-----------------------------------------------------------------------------
void CWeaponCombatShield::DrawBeams( C_BaseViewModel *pViewModel )
{
	// Verify data.
	if ( !pViewModel )
		return;

	// Only humans have the tesla effects
	if ( GetTeamNumber() == TEAM_ALIENS )
		return;

	// Init
	if ( !m_pTeslaBeam ) 
	{ 
		InitTeslaBeam(); 
	}
	if ( !m_pShieldBeam ) 
	{ 
		InitShieldBeam(); 
	}

	if ( !m_pShieldBeam || !m_pTeslaBeam )
		return;

	// Variables
	BeamInfo_t beamInfo;
	QAngle vecAngle;
	int iAttachment;

	// Setup a color reflecting the health
	float flShieldHealth = GetShieldHealth();
	color32 color;
	color.r = 206;
	color.g = flShieldHealth * 182;
	color.b = flShieldHealth * 127;
	color.a = 255;

	// Tesla Effect
	Vector vecRightTop, vecRightBottom;
	Vector vecLeftTop, vecLeftBottom;
	iAttachment = pViewModel->LookupAttachment( "LeftBottom" );
	pViewModel->GetAttachment( iAttachment, vecLeftBottom, vecAngle );
	pViewModel->UncorrectViewModelAttachment( vecLeftBottom );

	iAttachment = pViewModel->LookupAttachment( "LeftTip" );
	pViewModel->GetAttachment( iAttachment, vecLeftTop, vecAngle );
	pViewModel->UncorrectViewModelAttachment( vecLeftTop );

	iAttachment = pViewModel->LookupAttachment( "RightBottom" );
	pViewModel->GetAttachment( iAttachment, vecRightBottom, vecAngle );
	pViewModel->UncorrectViewModelAttachment( vecRightBottom );

	iAttachment = pViewModel->LookupAttachment( "RightTip" );
	pViewModel->GetAttachment( iAttachment, vecRightTop, vecAngle );
	pViewModel->UncorrectViewModelAttachment( vecRightTop );

	m_flTeslaLeftInc += weapon_combat_shield_teslaspeed.GetFloat();
	m_flTeslaRightInc += weapon_combat_shield_teslaspeed.GetFloat();
	if ( m_flTeslaLeftInc > 1.0f ) { m_flTeslaLeftInc = 0.0f; }
	if ( m_flTeslaRightInc > 1.0f ) { m_flTeslaRightInc = 0.0f; }

	Vector vecLeft = vecLeftTop - vecLeftBottom;
	Vector vecRight = vecRightTop - vecRightBottom;
	Vector vecStart = vecLeftBottom + ( m_flTeslaLeftInc * vecLeft );
	Vector vecEnd = vecRightBottom + ( m_flTeslaRightInc * vecRight );

	beamInfo.m_vecStart = vecStart;
	beamInfo.m_vecEnd = vecEnd;
	beamInfo.m_flRed = color.r;
	beamInfo.m_flGreen = color.g;
	beamInfo.m_flBlue = color.b;
	beams->UpdateBeamInfo( m_pTeslaBeam, beamInfo );	
	beams->UpdateBeamInfo( m_pTeslaBeam2, beamInfo );	
	beams->DrawBeam( m_pTeslaBeam );
	beams->DrawBeam( m_pTeslaBeam2 );

	// Draw a sprite at the tip of the tesla coil.
	float flSize = 4.0f;

	materials->Bind( m_hShieldSpriteMaterial, this );
	DrawSprite( vecStart, flSize, flSize, color );
	DrawSprite( vecEnd, flSize, flSize, color );

	// Shield Effect
	float flPercentage = random->RandomFloat( 0.0f, 1.0f );
	if ( flPercentage < weapon_combat_shield_teslaskitter.GetFloat() )
	{
		char szShieldJoint[16];
		int nJoint = random->RandomInt( 1, 8 );
		Q_snprintf( szShieldJoint, sizeof( szShieldJoint ), "Shield%d", nJoint );

		Vector vecJoint;
		int iAttachment = pViewModel->LookupAttachment( &szShieldJoint[0] );
		pViewModel->GetAttachment( iAttachment, vecJoint, vecAngle );
		pViewModel->UncorrectViewModelAttachment( vecJoint );	

		if ( nJoint < 5 )
		{
			beamInfo.m_vecStart = vecLeftTop;
		}
		else
		{
			beamInfo.m_vecStart = vecRightTop;
		}
		beamInfo.m_vecEnd = vecJoint;
		beams->UpdateBeamInfo( m_pTeslaBeam, beamInfo );	
		beams->UpdateBeamInfo( m_pTeslaBeam2, beamInfo );	
		beams->DrawBeam( m_pTeslaBeam );
		beams->DrawBeam( m_pTeslaBeam2 );

		float flSize = 7.0f; 
		color32 color = { 206, 181, 127, 255 };
		materials->Bind( m_hShieldSpriteMaterial, this );
		DrawSprite( beamInfo.m_vecStart, flSize, flSize, color );
	}

#if 0
	// Shield Effect
	char szShieldJoint[16];
	Vector vecShieldJoints[8];
	for( int iJoint = 0; iJoint < 8; ++iJoint )
	{
		Q_snprintf( szShieldJoint, sizeof( szShieldJoint ), "Shield%d", iJoint+1 );
		iAttachment = pViewModel->LookupAttachment( &szShieldJoint[0] );
		pViewModel->GetAttachment( iAttachment, vecShieldJoints[iJoint], vecAngle );
		pViewModel->UncorrectViewModelAttachment( vecShieldJoints[iJoint] );	
	}

	// Shield Internal
	if ( m_flShieldInc < 1.0f )
	{
		Vector vecEdge, vecEnd;
		if ( m_bLeftToRight )
		{
			vecEdge = vecShieldJoints[((m_nShieldEdge+1)%8)] - vecShieldJoints[m_nShieldEdge];
			vecEnd = vecShieldJoints[m_nShieldEdge] + ( m_flShieldInc * vecEdge );
		}
		else
		{
			vecEdge = vecShieldJoints[m_nShieldEdge] - vecShieldJoints[((m_nShieldEdge+1)%8)];
			vecEnd = vecShieldJoints[((m_nShieldEdge+1)%8)] + ( m_flShieldInc * vecEdge );
		}

		if ( m_nShieldEdge < 5 )
		{
			beamInfo.m_vecStart = vecLeftTop;
		}
		else
		{
			beamInfo.m_vecStart = vecRightTop;
		}

		beamInfo.m_vecEnd = vecEnd;
		beams->UpdateBeamPoints( m_pShieldBeam2, beamInfo );
		beams->UpdateBeamPoints( m_pShieldBeam3, beamInfo );
		beams->DrawBeam( m_pShieldBeam2 );
		beams->DrawBeam( m_pShieldBeam3 );

		m_flShieldInc += m_flShieldSpeed;
	}
	else
	{
		m_flShieldInc = 0.0f;
		m_flShieldSpeed = random->RandomFloat( 0.015f, 0.15f );
		m_nShieldEdge = random->RandomInt( 0, 7 );

		float flSide = random->RandomFloat( 0.0f, 1.0f );
		m_bLeftToRight = ( flSide < 0.5f );
	}

	// Shield Outline
	for( iJoint = 0; iJoint < 8; ++iJoint )
	{
		beamInfo.m_vecStart = vecShieldJoints[iJoint];
		beamInfo.m_vecEnd = vecShieldJoints[(iJoint+1)%8];
		beams->UpdateBeamPoints( m_pShieldBeam, beamInfo );
		beams->UpdateBeamPoints( m_pShieldBeam2, beamInfo );
		beams->DrawBeam( m_pShieldBeam );
		beams->DrawBeam( m_pShieldBeam2 );

		// Draw a sprite at the tip of the tesla coil.
		float flSize = 3.0f;
		color32 color = { 255, 255, 0, 255 };
		materials->Bind( m_hShieldSpriteMaterial, this );
		DrawSprite( vecShieldJoints[iJoint], flSize, flSize, color );
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: If the shield has no health, and they're trying to raise it, flash the power level
//-----------------------------------------------------------------------------
void CWeaponCombatShield::HandleInput( void )
{
	// If the player's dead, ignore input
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer || pPlayer->GetHealth() < 0 )
		return;

	// Attempting to raise the shield?
	if ( !GetShieldHealth() && ( gHUD.m_iKeyBits & IN_ATTACK2 ) )
	{
		m_flFlashTimeEnd = gpGlobals->curtime + 1.0;
	}
}
#endif

LINK_ENTITY_TO_CLASS( weapon_combat_shield, CWeaponCombatShield );
LINK_ENTITY_TO_CLASS( weapon_combat_shield_alien, CWeaponCombatShieldAlien );

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponCombatShield , DT_WeaponCombatShield )
IMPLEMENT_NETWORKCLASS_ALIASED( WeaponCombatShieldAlien, DT_WeaponCombatShieldAlien )

#if !defined( CLIENT_DLL )
//-----------------------------------------------------------------------------
// Purpose: Only send the LocalWeaponData to the player carrying the weapon
//-----------------------------------------------------------------------------
void* SendProxy_SendCombatShieldLocalWeaponDataTable( const SendProp *pProp, const void *pStruct, const void *pVarData, CSendProxyRecipients *pRecipients, int objectID )
{
	// Get the weapon entity
	CBaseCombatWeapon *pWeapon = (CBaseCombatWeapon*)pVarData;
	if ( pWeapon )
	{
		// Only send this chunk of data to the player carrying this weapon
		CBasePlayer *pPlayer = ToBasePlayer( pWeapon->GetOwner() );
		if ( pPlayer )
		{
			pRecipients->SetOnly( pPlayer->GetClientIndex() );
			return (void*)pVarData;
		}
	}
	
	return NULL;
}
REGISTER_SEND_PROXY_NON_MODIFIED_POINTER( SendProxy_SendCombatShieldLocalWeaponDataTable );
#endif

//-----------------------------------------------------------------------------
// Purpose: Propagation data for weapons. Only sent when a player's holding it.
//-----------------------------------------------------------------------------
BEGIN_NETWORK_TABLE_NOBASE( CWeaponCombatShield, DT_WeaponCombatShieldLocal )
#if !defined( CLIENT_DLL )
	SendPropTime( SENDINFO( m_flShieldParryEndTime ) ),
	SendPropTime( SENDINFO( m_flShieldParrySwingEndTime ) ),
	SendPropTime( SENDINFO( m_flShieldUnavailableEndTime ) ),
	SendPropTime( SENDINFO( m_flShieldRaisedTime ) ),
	SendPropTime( SENDINFO( m_flShieldLoweredTime ) ),
#else
	RecvPropTime( RECVINFO( m_flShieldParryEndTime ) ),
	RecvPropTime( RECVINFO( m_flShieldParrySwingEndTime ) ),
	RecvPropTime( RECVINFO( m_flShieldUnavailableEndTime ) ),
	RecvPropTime( RECVINFO( m_flShieldRaisedTime ) ),
	RecvPropTime( RECVINFO( m_flShieldLoweredTime ) ),
#endif
END_NETWORK_TABLE()

BEGIN_NETWORK_TABLE( CWeaponCombatShield , DT_WeaponCombatShield )
#if !defined( CLIENT_DLL )
	SendPropDataTable("this", 0, &REFERENCE_SEND_TABLE(DT_WeaponCombatShieldLocal), SendProxy_SendCombatShieldLocalWeaponDataTable ),
	SendPropTime( SENDINFO( m_flShieldUpStartTime ) ),
	SendPropTime( SENDINFO( m_flShieldDownStartTime ) ),
	SendPropInt( SENDINFO( m_iShieldState ), 3, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_bAllowPostFrame ), 1, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_bHasShieldParry ), 1, SPROP_UNSIGNED ),
	SendPropFloat(SENDINFO( m_flShieldHealth ), 8, SPROP_ROUNDDOWN, 0.0f, 1.0f ),
#else
	RecvPropDataTable("this", 0, 0, &REFERENCE_RECV_TABLE(DT_WeaponCombatShieldLocal)),
	RecvPropTime( RECVINFO( m_flShieldUpStartTime ) ),
	RecvPropTime( RECVINFO( m_flShieldDownStartTime ) ),
	RecvPropInt( RECVINFO( m_iShieldState ) ),
	RecvPropInt( RECVINFO( m_bAllowPostFrame ) ),
	RecvPropInt( RECVINFO( m_bHasShieldParry ) ),
	RecvPropFloat(RECVINFO( m_flShieldHealth ) ),
#endif
END_NETWORK_TABLE()

BEGIN_NETWORK_TABLE( CWeaponCombatShieldAlien, DT_WeaponCombatShieldAlien )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponCombatShield  )

	DEFINE_PRED_FIELD( m_iShieldState, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bAllowPostFrame, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bHasShieldParry, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_flShieldHealth, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),

	DEFINE_PRED_FIELD_TOL( m_flShieldUpStartTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE, TD_MSECTOLERANCE ),
	DEFINE_PRED_FIELD_TOL( m_flShieldDownStartTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE, TD_MSECTOLERANCE ),
	DEFINE_PRED_FIELD_TOL( m_flShieldParryEndTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE, TD_MSECTOLERANCE ),
	DEFINE_PRED_FIELD_TOL( m_flShieldParrySwingEndTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE, TD_MSECTOLERANCE ),
	DEFINE_PRED_FIELD_TOL( m_flShieldUnavailableEndTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE, TD_MSECTOLERANCE ),
	DEFINE_PRED_FIELD_TOL( m_flShieldRaisedTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE, TD_MSECTOLERANCE ),
	DEFINE_PRED_FIELD_TOL( m_flShieldLoweredTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE, TD_MSECTOLERANCE ),

END_PREDICTION_DATA()

BEGIN_PREDICTION_DATA( CWeaponCombatShieldAlien )
END_PREDICTION_DATA()

PRECACHE_WEAPON_REGISTER(weapon_combat_shield);
PRECACHE_WEAPON_REGISTER(weapon_combat_shield_alien);





