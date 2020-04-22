//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Chargeable Plasma & Shield combo
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "basetfplayer_shared.h"
#include "weapon_combatshield.h"
#include "in_buttons.h"
#include "weapon_combat_usedwithshieldbase.h"
#include "plasmaprojectile.h"
#include "in_buttons.h"
#include "tf_shareddefs.h"

#if defined( CLIENT_DLL )

#include "iefx.h"
#include "dlight.h"
#include "clienteffectprecachesystem.h"
#include "beamdraw.h"

#define CWeaponCombatPlasmaRifle C_WeaponCombatPlasmaRifle
#define CWeaponCombatPlasmaRifleHuman C_WeaponCombatPlasmaRifleHuman
#define CWeaponCombatPlasmaRifleAlien C_WeaponCombatPlasmaRifleAlien

#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#if defined( CLIENT_DLL )

class CChargeBall;
// Precache the effects
CLIENTEFFECT_REGISTER_BEGIN( PrecacheWeaponCombatPlasmaRifle )
CLIENTEFFECT_MATERIAL( "sprites/chargeball_team1" )
CLIENTEFFECT_MATERIAL( "sprites/chargeball_team2" )
CLIENTEFFECT_REGISTER_END()

#endif

// Damage CVars
ConVar	weapon_combat_plasmarifle_damage( "weapon_combat_plasmarifle_damage","10", FCVAR_REPLICATED, "Plasma Rifle maximum damage" );
ConVar	weapon_combat_plasmarifle_range( "weapon_combat_plasmarifle_range","500", FCVAR_REPLICATED, "Plasma Rifle maximum range" );
ConVar	weapon_combat_plasmarifle_radius( "weapon_combat_plasmarifle_radius","90", FCVAR_REPLICATED, "Plasma Rifle explosion radius when charged" );
ConVar	weapon_combat_plasmarifle_ducking_mod( "weapon_combat_plasmarifle_ducking_mod", "0.6f", FCVAR_REPLICATED, "Plasma Rifle ducking speed modifier" );

//-----------------------------------------------------------------------------
// Purpose: Shared viersion of CWeaponCombatPlasmaRifle
//-----------------------------------------------------------------------------
class CWeaponCombatPlasmaRifle : public CWeaponCombatUsedWithShieldBase
{
	DECLARE_CLASS( CWeaponCombatPlasmaRifle, CWeaponCombatUsedWithShieldBase );
public:
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();
#if !defined( CLIENT_DLL )
	DECLARE_DATADESC();
#endif

	CWeaponCombatPlasmaRifle( void ) {}

	virtual void	ItemPostFrame( void );
	virtual void	PrimaryAttack( void );
	virtual float 	GetFireRate( void );
	virtual void	Spawn();
	virtual bool	Deploy( void );
	virtual bool	Holster( CBaseCombatWeapon *pSwitchingTo = NULL );
	void			ChargeThink( void );
	virtual float	GetDefaultAnimSpeed( void );

	// All predicted weapons need to implement and return true
	virtual bool			IsPredicted( void ) const
	{ 
		return true;
	}

private:
	CWeaponCombatPlasmaRifle( const CWeaponCombatPlasmaRifle & );
public:

#if defined( CLIENT_DLL )
	virtual bool	ShouldPredict( void )
	{
		if ( GetOwner() && 
			GetOwner() == C_BasePlayer::GetLocalPlayer() )
			return true;

		return BaseClass::ShouldPredict();
	}

	virtual void	OnDataChanged( DataUpdateType_t updateType );
	virtual int		DrawModel( int flags );
	virtual void	ViewModelDrawn( CBaseViewModel *pBaseViewModel );
	virtual void	ClientThink( );
	virtual bool	IsTransparent( );
private:
	// Purpose: Draws the charging effect
	void DrawChargingEffect( float flSize, CBaseAnimating *pAttachedEnt );
	CMaterialReference m_hMaterial;

#endif
private:
	CNetworkVar( float, m_flPower );
	CNetworkVar( bool, m_bCharging );
};

//-----------------------------------------------------------------------------
// Spawn weapon
//-----------------------------------------------------------------------------
void CWeaponCombatPlasmaRifle::Spawn()
{
	BaseClass::Spawn();
	m_flPower = 1;
	m_bCharging = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponCombatPlasmaRifle::ItemPostFrame( void )
{
	ChargeThink();

	CBaseTFPlayer *pOwner = ToBaseTFPlayer( GetOwner() );
	if (!pOwner)
		return;

	if ( UsesClipsForAmmo1() )
	{
		CheckReload();
	}

	// Handle charge firing
	if ( GetShieldState() == SS_DOWN && !m_bInReload )
	{
		if ( (pOwner->m_nButtons & IN_ATTACK ) && (m_flNextPrimaryAttack <= gpGlobals->curtime) )
		{
			if ( m_iClip1 > 0 )
			{
				// Fire the plasma shot
				PrimaryAttack();
				m_flPower = 1.0;
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
void CWeaponCombatPlasmaRifle::PrimaryAttack( void )
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

	// Shift it down a bit so the firer can see it
	Vector right, forward;
	AngleVectors( pPlayer->EyeAngles() + pPlayer->m_Local.m_vecPunchAngle, &forward, &right, NULL );
	Vector vecStartSpot = vecSrc;

	Vector gunOffset = Vector(0,0,-8) + right * 12 + forward * 16;

	CPowerPlasmaProjectile *pPlasma = CPowerPlasmaProjectile::CreatePredicted( vecStartSpot, vecAiming, gunOffset, DMG_ENERGYBEAM, pPlayer );
	if ( pPlasma )
	{
		pPlasma->SetDamage( m_flPower * weapon_combat_plasmarifle_damage.GetFloat() );
		pPlasma->m_hOwner = pPlayer;
		pPlasma->SetPower( m_flPower );
		// Calculate range based upon charge power
		float flRange = weapon_combat_plasmarifle_range.GetFloat() + RemapVal( m_flPower, 1.0, MAX_RIFLE_POWER, 0, weapon_combat_plasmarifle_range.GetFloat() * 0.75 );
		pPlasma->SetMaxRange( flRange );
		pPlasma->Activate();
	}

	// Go explosive if fully charged
//	if ( m_flPower >= MAX_RIFLE_POWER )
//	{
//		pPlasma->SetExplosive( weapon_combat_plasmarifle_radius.GetFloat() );
//		pPlasma->SetPlasmaType( PLASMATYPE_PLASMABALL_EXPLOSIVE );
//	}

	m_flNextPrimaryAttack = gpGlobals->curtime + GetFireRate();
	m_iClip1 = m_iClip1 - 1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CWeaponCombatPlasmaRifle::GetFireRate( void )
{
	float flFireRate = ( SequenceDuration() * 0.4 ) + SHARED_RANDOMFLOAT( 0.0, 0.035f );

	// Get the player.
	CBaseTFPlayer *pPlayer = ( CBaseTFPlayer* )GetOwner();
	if ( pPlayer )
	{
		// Fire more rapidly when we are ducking.
		if ( pPlayer->GetFlags() & FL_DUCKING )
		{
			flFireRate *= weapon_combat_plasmarifle_ducking_mod.GetFloat();
		}
	}

	return flFireRate;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CWeaponCombatPlasmaRifle::Deploy( void )
{
	if ( BaseClass::Deploy() )
	{
		m_bCharging = true;
		GainedNewTechnology(NULL);
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Our player just died
//-----------------------------------------------------------------------------
bool CWeaponCombatPlasmaRifle::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	if ( BaseClass::Holster(pSwitchingTo) )
	{
		m_bCharging = false;
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Match the anim speed to the weapon speed while crouching
//-----------------------------------------------------------------------------
float CWeaponCombatPlasmaRifle::GetDefaultAnimSpeed( void )
{
	if ( GetOwner() && GetOwner()->IsPlayer() )
	{
		if ( GetOwner()->GetFlags() & FL_DUCKING )
			return (1.0 + (1.0 - weapon_combat_plasmarifle_ducking_mod.GetFloat()) );
	}

	return 1.0;
}

//-----------------------------------------------------------------------------
// Purpose: Charge up over time
//-----------------------------------------------------------------------------
void CWeaponCombatPlasmaRifle::ChargeThink( void )
{
	if ( !m_bCharging )
		return;

	if ( IsOwnerEMPed() )
	{
		m_flPower = 1;
	}
	else if ( m_iClip1 > 0 && m_flPower < MAX_RIFLE_POWER )
	{
		m_flPower = MIN( MAX_RIFLE_POWER, m_flPower + (((MAX_RIFLE_POWER-1.0) / RIFLE_CHARGE_TIME) * gpGlobals->frametime ) );
	}
}

#if defined( CLIENT_DLL )
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponCombatPlasmaRifle::OnDataChanged( DataUpdateType_t updateType )
{
	SetPredictionEligible( true );

	BaseClass::OnDataChanged( updateType );

	if (updateType == DATA_UPDATE_CREATED)
	{
		if ( GetTeamNumber() == 1 )
			m_hMaterial.Init( "sprites/chargeball_team1", TEXTURE_GROUP_CLIENT_EFFECTS );
		else 
			m_hMaterial.Init( "sprites/chargeball_team2", TEXTURE_GROUP_CLIENT_EFFECTS );
	}

	if (WeaponState() == WEAPON_IS_ACTIVE)
	{
		// Start thinking so we can manipulate the light
		SetNextClientThink( CLIENT_THINK_ALWAYS );
	}
	else
	{
		SetNextClientThink( CLIENT_THINK_NEVER );
	}
}


//-----------------------------------------------------------------------------
// Deal with dynamic lighting
//-----------------------------------------------------------------------------
void CWeaponCombatPlasmaRifle::ClientThink( )
{
	BaseClass::ClientThink();

	if (!inv_demo.GetInt())
	{
		C_BaseTFPlayer *pPlayer = (C_BaseTFPlayer *)GetOwner();
		if ( !pPlayer || (pPlayer->GetHealth() <= 0) || !IsDormant() )
		{
			SetNextClientThink( CLIENT_THINK_NEVER );
			return;
		}

		// FIXME: dl->origin should be based on the attachment point
		dlight_t *dl = effects->CL_AllocDlight( entindex() );
		dl->origin = GetRenderOrigin();
		if (GetTeamNumber() == 1)
		{
			dl->color.r = 40;
			dl->color.g = 60;
			dl->color.b = 250;
		}
		else
		{
			dl->color.r = 250;
			dl->color.g = 60;
			dl->color.b = 40;
		}
		dl->color.exponent = 7;
		dl->radius = 20 * m_flPower + 10;
		dl->die = gpGlobals->curtime + 0.01;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Draws the charging effect
//-----------------------------------------------------------------------------
void CWeaponCombatPlasmaRifle::DrawChargingEffect( float flSize, CBaseAnimating *pAttachedEnt )
{
	if (!pAttachedEnt)
		return;

	Vector vecMuzzleOrigin, vecBarrelOrigin;
	QAngle angMuzzleAngles, angBarrelAngles;
	int iMuzzle = pAttachedEnt->LookupAttachment( "muzzle" );
	//int iBarrel = pAttachedEnt->LookupAttachment( "barrel" );

	if ( pAttachedEnt->GetAttachment( iMuzzle, vecMuzzleOrigin, angMuzzleAngles ) )
	{
		// View model attachments are modified so you can place entities at the attachment
		// point and when they are rendered (with a different FOV than the view model itself uses)
		// they will render in the right spot when the view model is drawn.
		// 
		// In this case though, we are rendering at the same time as the view model, so we want
		// the attachment point before the correction has been applied.
		pAttachedEnt->UncorrectViewModelAttachment( vecMuzzleOrigin );

		// If I'm fully charged, put funky effects on the ball
		materials->Bind( m_hMaterial, this );
		
		if ( m_flPower >= MAX_RIFLE_POWER )
		{
			float frac = fmod( gpGlobals->curtime, 1.0 );
			frac *= 2 * M_PI;
			frac = sin( frac );
			flSize += (frac * 2) - 1.5;
			int colorFade = 190 + (int)( frac * 32.0f );

			color32 color = { 0, 0, 0, 255 };
			if ( GetTeamNumber() == 1 )
			{
				color.r = colorFade;
				color.g = colorFade;
			}
			else 
			{
				color.g = colorFade;
			}
			DrawSprite( vecMuzzleOrigin, flSize, flSize, color );
		}
		else
		{
			color32 color = { 255, 255, 255, 255 };
			DrawSprite( vecMuzzleOrigin, flSize, flSize, color );
		}
	}
}


//-----------------------------------------------------------------------------
// We're transparent because we draw a transparent charging effect
//-----------------------------------------------------------------------------
bool CWeaponCombatPlasmaRifle::IsTransparent( )
{
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Draws the model
//-----------------------------------------------------------------------------
int CWeaponCombatPlasmaRifle::DrawModel( int flags )
{
	int retval = BaseClass::DrawModel( flags );
	if (retval == 0)
		return 0;

	if (IsCarrierAlive())
	{
		// FIXME: Maybe do some client-side simulation on the size?
		// It may get jerky otherwise

		// Draw the charging effect
		float flSize = 10 * m_flPower + 5;

		DrawChargingEffect( flSize, this );
	}
	return retval;
}


//-----------------------------------------------------------------------------
// Purpose: Draws the model
//-----------------------------------------------------------------------------
void CWeaponCombatPlasmaRifle::ViewModelDrawn( CBaseViewModel *pBaseViewModel )
{
	// Draw the charging effect
	float flSize = 4 * m_flPower + 1;

	if ( m_iClip1 > 0 )
	{
		DrawChargingEffect( flSize, pBaseViewModel );
	}
}
#endif


IMPLEMENT_NETWORKCLASS_ALIASED( WeaponCombatPlasmaRifle, DT_WeaponCombatPlasmaRifle )

BEGIN_NETWORK_TABLE( CWeaponCombatPlasmaRifle, DT_WeaponCombatPlasmaRifle )
#if !defined( CLIENT_DLL )
	SendPropFloat( SENDINFO( m_flPower ), 14, SPROP_ROUNDUP, 1.0f, MAX_RIFLE_POWER ),
	SendPropInt( SENDINFO( m_bCharging ), 1, SPROP_UNSIGNED ),
#else
	RecvPropFloat( RECVINFO( m_flPower ) ),
	RecvPropInt( RECVINFO( m_bCharging ) ),
#endif
END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( weapon_combat_plasmarifle_base, CWeaponCombatPlasmaRifle );

BEGIN_PREDICTION_DATA( CWeaponCombatPlasmaRifle  )

	DEFINE_PRED_FIELD_TOL( m_flPower, FIELD_FLOAT, FTYPEDESC_INSENDTABLE, 0.05f ),
	DEFINE_PRED_FIELD( m_bCharging, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),

END_PREDICTION_DATA()

#if !defined( CLIENT_DLL )

BEGIN_DATADESC( CWeaponCombatPlasmaRifle )

	// Function Pointers
	DEFINE_FUNCTION( ChargeThink ),

END_DATADESC()

// PRECACHE_WEAPON_REGISTER(weapon_combat_plasmarifle_base);
#endif

//-----------------------------------------------------------------------------
// Purpose: Need to do different art on client vs server
//-----------------------------------------------------------------------------
class CWeaponCombatPlasmaRifleHuman : public CWeaponCombatPlasmaRifle
{
	DECLARE_CLASS( CWeaponCombatPlasmaRifleHuman, CWeaponCombatPlasmaRifle );
public:
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CWeaponCombatPlasmaRifleHuman( void ) {}

private:
	CWeaponCombatPlasmaRifleHuman( const CWeaponCombatPlasmaRifleHuman & );
};

//-----------------------------------------------------------------------------
// Purpose: Need to do different art on client vs server
//-----------------------------------------------------------------------------
class CWeaponCombatPlasmaRifleAlien : public CWeaponCombatPlasmaRifle
{
	DECLARE_CLASS( CWeaponCombatPlasmaRifleAlien, CWeaponCombatPlasmaRifle );
public:
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CWeaponCombatPlasmaRifleAlien( void ) {}

private:
	CWeaponCombatPlasmaRifleAlien( const CWeaponCombatPlasmaRifleAlien & );
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponCombatPlasmaRifleHuman, DT_WeaponCombatPlasmaRifleHuman )

BEGIN_NETWORK_TABLE( CWeaponCombatPlasmaRifleHuman, DT_WeaponCombatPlasmaRifleHuman )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponCombatPlasmaRifleHuman )
END_PREDICTION_DATA()

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponCombatPlasmaRifleAlien, DT_WeaponCombatPlasmaRifleAlien )

BEGIN_NETWORK_TABLE( CWeaponCombatPlasmaRifleAlien, DT_WeaponCombatPlasmaRifleAlien )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponCombatPlasmaRifleAlien )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_combat_plasmarifle, CWeaponCombatPlasmaRifleHuman );
LINK_ENTITY_TO_CLASS( weapon_combat_plasmarifle_alien, CWeaponCombatPlasmaRifleAlien );

PRECACHE_WEAPON_REGISTER(weapon_combat_plasmarifle);
PRECACHE_WEAPON_REGISTER(weapon_combat_plasmarifle_alien);
