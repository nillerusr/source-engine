//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Minigun
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "weapon_combat_usedwithshieldbase.h"
#include "in_buttons.h"
#include "takedamageinfo.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "tf_gamerules.h"

// Damage CVars
ConVar	weapon_minigun_damage( "weapon_minigun_damage","10", FCVAR_REPLICATED, "Minigun damage per pellet" );
ConVar	weapon_minigun_range( "weapon_minigun_range","1500", FCVAR_REPLICATED, "Minigun maximum range" );
ConVar	weapon_minigun_pellets( "weapon_minigun_pellets","2", FCVAR_REPLICATED, "Minigun pellets per fire" );
ConVar	weapon_minigun_ducking_mod( "weapon_minigun_ducking_mod", "0.75", FCVAR_REPLICATED, "Minigun ducking speed modifier" );

#if defined( CLIENT_DLL )
#include "hud.h"
#include "fx.h"
#define CWeaponMinigun C_WeaponMinigun
extern ConVar zoom_sensitivity_ratio;
extern ConVar default_fov;
#else
#endif

// Time taken to fully wind up/down
#define MINIGUN_WIND_TIME		2

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CWeaponMinigun : public CWeaponCombatUsedWithShieldBase
{
	DECLARE_CLASS( CWeaponMinigun, CWeaponCombatUsedWithShieldBase );
public:
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

	CWeaponMinigun( void );

	virtual void	Precache();

	virtual bool	Holster( CBaseCombatWeapon *pSwitchingTo );
	virtual const Vector& GetBulletSpread( void );
	virtual void	ItemPostFrame( void );
	virtual void	PrimaryAttack( void );
	virtual void	AddViewKick( void );
	virtual float 	GetFireRate( void );
	virtual float	GetDefaultAnimSpeed( void );
	virtual void	BulletWasFired( const Vector &vecStart, const Vector &vecEnd );

	// All predicted weapons need to implement and return true
	virtual bool	IsPredicted( void ) const
	{ 
		return true;
	}

	void			ReduceRotation( void );
	void			AttemptToReload( void );

#if defined( CLIENT_DLL )
public:
	virtual bool	ShouldPredict( void )
	{
		if ( GetOwner() && 
			GetOwner() == C_BasePlayer::GetLocalPlayer() )
			return true;

		return BaseClass::ShouldPredict();
	}

	virtual bool	OnFireEvent( C_BaseViewModel *pViewModel, const Vector& origin, const QAngle& angles, int event, const char *options );
	virtual void	OnDataChanged( DataUpdateType_t updateType );
	virtual void	ClientThink( void );
#endif

public:
	float	m_flOwnersMaxSpeed;
	CNetworkVar( float, m_flRotationSpeed );	// When 1, firing commences.
	bool	m_bSoundPlaying;

private:
	CWeaponMinigun( const CWeaponMinigun & );
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CWeaponMinigun::CWeaponMinigun( void )
{
	SetPredictionEligible( true );
	m_flRotationSpeed = 0;
	m_bSoundPlaying = false;
}

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponMinigun, DT_WeaponMinigun )

BEGIN_NETWORK_TABLE( CWeaponMinigun, DT_WeaponMinigun )
#if !defined( CLIENT_DLL )
	SendPropFloat( SENDINFO( m_flRotationSpeed ), 8, SPROP_ROUNDDOWN, 0, 1 ),
#else
	RecvPropFloat( RECVINFO( m_flRotationSpeed ) ),
#endif
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponMinigun )
	DEFINE_PRED_FIELD_TOL( m_flRotationSpeed, FIELD_FLOAT, FTYPEDESC_INSENDTABLE, 0.01f ),
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_minigun, CWeaponMinigun );
PRECACHE_WEAPON_REGISTER(weapon_minigun);

void CWeaponMinigun::Precache()
{
	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CWeaponMinigun::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	m_flRotationSpeed = 0;
	
	return BaseClass::Holster( pSwitchingTo );
}

//-----------------------------------------------------------------------------
// Purpose: Get the accuracy derived from weapon and player, and return it
//-----------------------------------------------------------------------------
const Vector& CWeaponMinigun::GetBulletSpread( void )
{
	static Vector cone = VECTOR_CONE_8DEGREES;
	return cone;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponMinigun::ItemPostFrame( void )
{
	CBaseTFPlayer *pOwner = ToBaseTFPlayer( GetOwner() );
	if (!pOwner)
		return;

	// This should work, and avoids sending extra network data. If it doesn't, we'll have to send down the unchanged max speed.
	if ( !m_flRotationSpeed )
	{
		m_flOwnersMaxSpeed = pOwner->MaxSpeed();
	}

	float flLastRotationSpeed = m_flRotationSpeed;

	CheckReload();

	// Handle firing
	if ( (pOwner->m_nButtons & IN_ATTACK ) && (m_flNextPrimaryAttack <= gpGlobals->curtime) )
	{
		if ( m_iClip1 > 0 )
		{
			if ( m_flRotationSpeed < 0.99 )
			{
				// If we're starting, play the sound
				m_flRotationSpeed = min(1, m_flRotationSpeed + (gpGlobals->frametime / MINIGUN_WIND_TIME) );
			}
			else
			{
				PrimaryAttack();
			}
		}
		else
		{
			AttemptToReload();
		}
	}

	// Reload button (or fire button when we're out of ammo)
	if ( m_flNextPrimaryAttack <= gpGlobals->curtime ) 
	{
		if ( pOwner->m_nButtons & IN_RELOAD ) 
		{
			AttemptToReload();
		}
		else if ( !((pOwner->m_nButtons & IN_ATTACK) || (pOwner->m_nButtons & IN_ATTACK2) || (pOwner->m_nButtons & IN_RELOAD)) )
		{
			if ( !m_iClip1 && HasPrimaryAmmo() )
			{
				AttemptToReload();
			}
			else
			{
				ReduceRotation();
			}
		}
	}

	// If the speed changed, modify our movement speed
	if ( m_flRotationSpeed != flLastRotationSpeed )
	{
		pOwner->SetMaxSpeed( m_flOwnersMaxSpeed * (1.0 - (m_flRotationSpeed * 0.5)) );
	}

	WeaponIdle();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponMinigun::ReduceRotation( void )
{
	if ( m_flRotationSpeed > 0 )
	{
		m_flRotationSpeed = MAX(0, m_flRotationSpeed - (gpGlobals->frametime / MINIGUN_WIND_TIME) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponMinigun::AttemptToReload( void )
{
	// Wind down before reloading
	if ( m_flRotationSpeed > 0 )
	{
		ReduceRotation();
	}
	else
	{
		Reload();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponMinigun::PrimaryAttack( void )
{
	CBaseTFPlayer *pPlayer = (CBaseTFPlayer*)GetOwner();
	if (!pPlayer)
		return;
	
	// Fire the bullets
	Vector vecSrc = pPlayer->Weapon_ShootPosition( );
	Vector vecAiming;
	pPlayer->EyeVectors( &vecAiming );

	PlayAttackAnimation( GetPrimaryAttackActivity() );

	// Make a satisfying force, and knock them into the air
	float flForceScale = (100) * 75 * 4;
	Vector vecForce = vecAiming;
	vecForce.z += 0.7;
	vecForce *= flForceScale;
	
	CTakeDamageInfo info( this, pPlayer, vecForce, vec3_origin, weapon_minigun_damage.GetFloat(), DMG_BULLET | DMG_BUCKSHOT);
	TFGameRules()->FireBullets( info, weapon_minigun_pellets.GetFloat(), 
		vecSrc, vecAiming, GetBulletSpread(), weapon_minigun_range.GetFloat(), m_iPrimaryAmmoType, 0, entindex(), 0 );

	AddViewKick();

	m_flNextPrimaryAttack = gpGlobals->curtime + GetFireRate();
	m_iClip1 = m_iClip1 - 1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponMinigun::AddViewKick( void )
{
	// Get the view kick
	CBaseTFPlayer *pPlayer = ToBaseTFPlayer( GetOwner() );
	if ( !pPlayer )
		return;

	QAngle viewPunch;
	viewPunch.x = SHARED_RANDOMFLOAT( -0.5f, 0.5f );
	viewPunch.y = SHARED_RANDOMFLOAT( -1.0f, 1.0f );
	viewPunch.z = 0;

	if ( pPlayer->GetFlags() & FL_DUCKING )
	{
		viewPunch *= 0.25;
	}

	pPlayer->ViewPunch( viewPunch );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CWeaponMinigun::GetFireRate( void )
{	
	float flFireRate = SHARED_RANDOMFLOAT( 0.05, 0.1 );

	CBaseTFPlayer *pPlayer = static_cast<CBaseTFPlayer*>( GetOwner() );
	if ( pPlayer )
	{
		// Ducking players should fire more rapidly.
		if ( pPlayer->GetFlags() & FL_DUCKING )
		{
			flFireRate *= weapon_minigun_ducking_mod.GetFloat();
		}
	}
	
	return flFireRate;
}

//-----------------------------------------------------------------------------
// Purpose: Match the anim speed to the weapon speed while crouching
//-----------------------------------------------------------------------------
float CWeaponMinigun::GetDefaultAnimSpeed( void )
{
	if ( GetOwner() && GetOwner()->IsPlayer() )
	{
		if ( GetOwner()->GetFlags() & FL_DUCKING )
			return (1.0 + (1.0 - weapon_minigun_ducking_mod.GetFloat()) );
	}

	return 1.0;
}

//-----------------------------------------------------------------------------
// Purpose: Draw the minigun effect
//-----------------------------------------------------------------------------
void CWeaponMinigun::BulletWasFired( const Vector &vecStart, const Vector &vecEnd )
{
	UTIL_Tracer( (Vector&)vecStart, (Vector&)vecEnd, entindex(), 1, 5000, false, "MinigunTracer" );
}

#if defined ( CLIENT_DLL )
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CWeaponMinigun::OnFireEvent( C_BaseViewModel *pViewModel, const Vector& origin, const QAngle& angles, int event, const char *options )
{
	// Suppress the shell ejection from the HL2 model we're using for prototyping
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : updateType - 
//-----------------------------------------------------------------------------
void CWeaponMinigun::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	if ( updateType == DATA_UPDATE_CREATED )
	{
		ClientThinkList()->SetNextClientThink( GetClientHandle(), CLIENT_THINK_ALWAYS );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponMinigun::ClientThink()
{
	CBaseTFPlayer *pPlayer = (CBaseTFPlayer*)GetOwner();
	if (!pPlayer)
		return;

	if ( m_flRotationSpeed )
	{
		WeaponSound_t nSound = SPECIAL1;

		// If we're firing, play that sound instead
		if ( m_flRotationSpeed >= 0.99 )
		{
			nSound = SINGLE;
		}
		else
		{
			m_bSoundPlaying = true;
		}

		// If we have some sounds from the weapon classname.txt file, play a random one of them
		const char *shootsound = GetShootSound( nSound );
		if ( !shootsound || !shootsound[0] )
			return; 

		CSoundParameters params;
		if ( !GetParametersForSound( shootsound, params, NULL ) )
			return;

		// Shift pitch according to barrel rotation
		float flPitch = 30 + (90 * m_flRotationSpeed);

		CPASAttenuationFilter filter( GetOwner(), params.soundlevel );
		Vector vecOrigin = GetOwner()->GetAbsOrigin();
		
		EmitSound_t ep;
		ep.m_nChannel = CHAN_WEAPON;
		ep.m_pSoundName = shootsound;
		ep.m_flVolume = params.volume;
		ep.m_SoundLevel = params.soundlevel;
		ep.m_nFlags = SND_CHANGE_PITCH;
		ep.m_nPitch = (int)flPitch;
		ep.m_pOrigin = &vecOrigin;


		EmitSound( filter, GetOwner()->entindex(), ep ); 
	}
	else if ( m_bSoundPlaying )
	{
		m_bSoundPlaying = false;
		StopWeaponSound( SPECIAL1 );
		StopWeaponSound( SINGLE );
	}
}

#endif