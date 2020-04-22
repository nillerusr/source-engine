//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Laser Rifle & Shield combo
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "weapon_combatshield.h"
#include "weapon_combat_usedwithshieldbase.h"
#include "in_buttons.h"
#include "takedamageinfo.h"
#include "beam_shared.h"
#include "tf_gamerules.h"

// Damage CVars
ConVar	weapon_combat_laserrifle_damage( "weapon_combat_laserrifle_damage","20", FCVAR_REPLICATED, "Laser rifle damage" );
ConVar	weapon_combat_laserrifle_range( "weapon_combat_laserrifle_range","1000", FCVAR_REPLICATED, "Laser rifle maximum range" );
ConVar	weapon_combat_laserrifle_ducking_mod( "weapon_combat_laserrifle_ducking_mod", "0.75", FCVAR_REPLICATED, "Laser rifle ducking ROF modifier" );

#if defined( CLIENT_DLL )
#include "fx.h"
#include "hud.h"
#include "c_te_effect_dispatch.h"
#include <vgui/ISurface.h>

#define CWeaponCombatLaserRifle C_WeaponCombatLaserRifle

#else

#include "te_effect_dispatch.h"

#endif


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CWeaponCombatLaserRifle : public CWeaponCombatUsedWithShieldBase
{
	DECLARE_CLASS( CWeaponCombatLaserRifle, CWeaponCombatUsedWithShieldBase );
public:
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

	CWeaponCombatLaserRifle( void );

	virtual const Vector& GetBulletSpread( void );
	virtual void	ItemBusyFrame( void );
	virtual void	ItemPostFrame( void );
	virtual void	PrimaryAttack( void );
	virtual float 	GetFireRate( void );
	virtual float	GetDefaultAnimSpeed( void );
	virtual void	BulletWasFired( const Vector &vecStart, const Vector &vecEnd );

	void			RecalculateAccuracy( void );

	// All predicted weapons need to implement and return true
	virtual bool	IsPredicted( void ) const
	{ 
		return true;
	}
private:
	CWeaponCombatLaserRifle( const CWeaponCombatLaserRifle & );

public:
#if defined( CLIENT_DLL )
	virtual bool	ShouldPredict( void )
	{
		if ( GetOwner() && 
			GetOwner() == C_BasePlayer::GetLocalPlayer() )
			return true;

		return BaseClass::ShouldPredict();
	}
	virtual void	DrawCrosshair( void );
#endif

private:
	float	m_flInaccuracy;
	float	m_flAccuracyTime;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CWeaponCombatLaserRifle::CWeaponCombatLaserRifle( void )
{
	SetPredictionEligible( true );
	m_flInaccuracy = 0;
	m_flAccuracyTime = 0;
}

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponCombatLaserRifle, DT_WeaponCombatLaserRifle )

BEGIN_NETWORK_TABLE( CWeaponCombatLaserRifle, DT_WeaponCombatLaserRifle )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponCombatLaserRifle )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_combat_laserrifle, CWeaponCombatLaserRifle );
PRECACHE_WEAPON_REGISTER(weapon_combat_laserrifle);

//-----------------------------------------------------------------------------
// Purpose: Get the accuracy derived from weapon and player, and return it
//-----------------------------------------------------------------------------
const Vector& CWeaponCombatLaserRifle::GetBulletSpread( void )
{
	static Vector cone = VECTOR_CONE_8DEGREES;
	return cone;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponCombatLaserRifle::ItemBusyFrame( void )
{
	BaseClass::ItemBusyFrame();

	RecalculateAccuracy();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponCombatLaserRifle::ItemPostFrame( void )
{
	CBaseTFPlayer *pOwner = ToBaseTFPlayer( GetOwner() );
	if (!pOwner)
		return;

	if ( UsesClipsForAmmo1() )
	{
		CheckReload();
	}

	RecalculateAccuracy();

	// Handle firing
	if ( GetShieldState() == SS_DOWN && !m_bInReload )
	{
		if ( (pOwner->m_nButtons & IN_ATTACK ) && (m_flNextPrimaryAttack <= gpGlobals->curtime) )
		{
			if ( m_iClip1 > 0 )
			{
				// Fire the plasma shot
				PrimaryAttack();
			}
			else
			{
				Reload();
			}
		}

		// Reload button (or fire button when we're out of ammo)
		if ( m_flNextPrimaryAttack <= gpGlobals->curtime ) 
		{
			if ( pOwner->m_nButtons & IN_RELOAD ) 
			{
				Reload();
			}
			else if ( !((pOwner->m_nButtons & IN_ATTACK) || (pOwner->m_nButtons & IN_ATTACK2) || (pOwner->m_nButtons & IN_RELOAD)) )
			{
				if ( !m_iClip1 && HasPrimaryAmmo() )
				{
					Reload();
				}
			}
		}
	}

	// Prevent shield post frame if we're not ready to attack, or we're charging
	AllowShieldPostFrame( m_flNextPrimaryAttack <= gpGlobals->curtime || m_bInReload );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponCombatLaserRifle::PrimaryAttack( void )
{
	CBaseTFPlayer *pPlayer = (CBaseTFPlayer*)GetOwner();
	if (!pPlayer)
		return;
	
	WeaponSound(SINGLE);

	// Fire the bullets
	Vector vecSrc = pPlayer->Weapon_ShootPosition( );
	Vector vecAiming;
	pPlayer->EyeVectors( &vecAiming );

	PlayAttackAnimation( GetPrimaryAttackActivity() );

	// Reduce the spread if the player's ducking
	Vector vecSpread = GetBulletSpread();
	vecSpread *= m_flInaccuracy;

	TFGameRules()->FireBullets( CTakeDamageInfo( this, pPlayer, weapon_combat_laserrifle_damage.GetFloat(), DMG_PLASMA), 1, 
		vecSrc, vecAiming, vecSpread, weapon_combat_laserrifle_range.GetFloat(), m_iPrimaryAmmoType, 0, entindex(), 0 );

	m_flInaccuracy += 0.3;
	m_flInaccuracy = clamp(m_flInaccuracy, 0, 1);

	m_flNextPrimaryAttack = gpGlobals->curtime + GetFireRate();
	m_iClip1 = m_iClip1 - 1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CWeaponCombatLaserRifle::GetFireRate( void )
{	
	float flFireRate = ( SequenceDuration() * 0.4 ) + SHARED_RANDOMFLOAT( 0.0, 0.035f );

	CBaseTFPlayer *pPlayer = static_cast<CBaseTFPlayer*>( GetOwner() );
	if ( pPlayer )
	{
		// Ducking players should fire more rapidly.
		if ( pPlayer->GetFlags() & FL_DUCKING )
		{
			flFireRate *= weapon_combat_laserrifle_ducking_mod.GetFloat();
		}
	}
	
	return flFireRate;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponCombatLaserRifle::RecalculateAccuracy( void )
{
	CBaseTFPlayer *pPlayer = (CBaseTFPlayer*)GetOwner();
	if (!pPlayer)
		return;

	m_flAccuracyTime += gpGlobals->frametime;

	while ( m_flAccuracyTime > 0.05 )
	{
		if ( !(pPlayer->GetFlags() & FL_ONGROUND) )
		{
			m_flInaccuracy += 0.05;
		}
		else if ( pPlayer->GetFlags() & FL_DUCKING )
		{
			m_flInaccuracy -= 0.08;
		}
/*
		else if ( pPlayer->GetLocalVelocity().LengthSqr() > (100*100) )
		{
			// Never get worse than 1/2 accuracy from running
			if ( m_flInaccuracy < 0.25 )
			{
				m_flInaccuracy += 0.01;
				if ( m_flInaccuracy > 0.5 )
				{
					m_flInaccuracy = 0.5;
				}
			}
			else if ( m_flInaccuracy > 0.25 )
			{
				m_flInaccuracy -= 0.01;
			}
		}
*/
		else
		{
			m_flInaccuracy -= 0.04;
		}

		// Crouching prevents accuracy ever going beyond a point
		if ( pPlayer->GetFlags() & FL_DUCKING )
		{
			m_flInaccuracy = clamp(m_flInaccuracy, 0, 0.8);
		}
		else
		{
			m_flInaccuracy = clamp(m_flInaccuracy, 0, 1);
		}

		m_flAccuracyTime -= 0.05;

#ifndef CLIENT_DLL
		//if ( m_flInaccuracy )
			//Msg("Inaccuracy %.2f (%.2f)\n", m_flInaccuracy, gpGlobals->curtime );
#endif
	}
}

//-----------------------------------------------------------------------------
// Purpose: Match the anim speed to the weapon speed while crouching
//-----------------------------------------------------------------------------
float CWeaponCombatLaserRifle::GetDefaultAnimSpeed( void )
{
	if ( GetOwner() && GetOwner()->IsPlayer() )
	{
		if ( GetOwner()->GetFlags() & FL_DUCKING )
			return (1.0 + (1.0 - weapon_combat_laserrifle_ducking_mod.GetFloat()) );
	}

	return 1.0;
}

//-----------------------------------------------------------------------------
// Purpose: Draw the laser rifle effect
//-----------------------------------------------------------------------------
void CWeaponCombatLaserRifle::BulletWasFired( const Vector &vecStart, const Vector &vecEnd )
{
	// Humans fire jazzed up bullets, Aliens fire laserbeams
	if ( GetTeamNumber() == TEAM_HUMANS )
	{
		UTIL_Tracer( (Vector&)vecStart, (Vector&)vecEnd, entindex(), 1, 5000, false, "HLaserTracer" );
	}
	else
	{
		UTIL_Tracer( (Vector&)vecStart, (Vector&)vecEnd, entindex(), 1, 5000, false, "ALaserTracer" );
	}
}

#ifdef CLIENT_DLL
//-----------------------------------------------------------------------------
// Purpose: Draw the weapon's crosshair
//-----------------------------------------------------------------------------
void CWeaponCombatLaserRifle::DrawCrosshair( void )
{
	BaseClass::DrawCrosshair();

	// Draw the targeting zone around the crosshair
	int r, g, b, a;
	gHUD.m_clrYellowish.GetColor( r, g, b, a );

	// Check to see if we are in vgui mode
	C_BaseTFPlayer *pPlayer	= static_cast<C_BaseTFPlayer*>( GetOwner() );
	if ( !pPlayer || pPlayer->IsInVGuiInputMode() )
		return;

	// Draw a crosshair & accuracy hoodad
	int iBarWidth = XRES(10);
	int iBarHeight = YRES(10);
	int iTotalWidth = (iBarWidth * 2) + (40 * m_flInaccuracy) + XRES(10);
	int iTotalHeight = (iBarHeight * 2) + (40 * m_flInaccuracy) + YRES(10);
	
	// Horizontal bars
	int iLeft = (ScreenWidth() - iTotalWidth) / 2;
	int iMidHeight = (ScreenHeight() / 2);

	Color dark( r, g, b, 32 );
	Color light( r, g, b, 160 );

	vgui::surface()->DrawSetColor( dark );

	vgui::surface()->DrawFilledRect( iLeft, iMidHeight-1, iLeft+ iBarWidth, iMidHeight + 2 );
	vgui::surface()->DrawFilledRect( iLeft + iTotalWidth - iBarWidth, iMidHeight-1, iLeft + iTotalWidth, iMidHeight + 2 );

	vgui::surface()->DrawSetColor( light );

	vgui::surface()->DrawFilledRect( iLeft, iMidHeight, iLeft + iBarWidth, iMidHeight + 1 );
	vgui::surface()->DrawFilledRect( iLeft + iTotalWidth - iBarWidth, iMidHeight, iLeft + iTotalWidth, iMidHeight + 1 );
	
	// Vertical bars
	int iTop = (ScreenHeight() - iTotalHeight) / 2;
	int iMidWidth = (ScreenWidth() / 2);

	vgui::surface()->DrawSetColor( dark );
	
	vgui::surface()->DrawFilledRect( iMidWidth-1, iTop, iMidWidth + 2, iTop + iBarHeight );
	vgui::surface()->DrawFilledRect( iMidWidth-1, iTop + iTotalHeight - iBarHeight, iMidWidth + 2, iTop + iTotalHeight );

	vgui::surface()->DrawSetColor( light );

	vgui::surface()->DrawFilledRect( iMidWidth, iTop, iMidWidth + 1, iTop + iBarHeight );
	vgui::surface()->DrawFilledRect( iMidWidth, iTop + iTotalHeight - iBarHeight, iMidWidth + 1, iTop + iTotalHeight );
}
#endif