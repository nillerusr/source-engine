//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "basetfplayer_shared.h"
#include "basetfcombatweapon_shared.h"
#include "weapon_twohandedcontainer.h"

#if !defined( CLIENT_DLL )
#include "soundent.h"
#else
#include "functionproxy.h"
#include "ivrenderview.h"
#include "cl_animevent.h"
#include "fx.h"
#endif

CBaseTFCombatWeapon::CBaseTFCombatWeapon ( void )
{
	m_bReflectViewModelAnimations = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTFCombatWeapon::Precache( void )
{
	BaseClass::Precache();

#if !defined( CLIENT_DLL )
	// Connect to my CVars
	m_pDamageCVar = cvar->FindVar( UTIL_VarArgs( "%s_damage", GetClassname() ) );
	m_pRangeCVar = cvar->FindVar( UTIL_VarArgs( "%s_range", GetClassname() ) );
#endif
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CBaseTFCombatWeapon::GetPrimaryAmmo( void )
{
	// Get the local player
	CBasePlayer *pPlayer = ToBaseTFPlayer( GetOwner() );
	if ( pPlayer == NULL )
		return 0;

	return pPlayer->GetAmmoCount( m_iPrimaryAmmoType );
}


//-----------------------------------------------------------------------------
// Purpose: Checks if the owner is EMPed
//-----------------------------------------------------------------------------
bool CBaseTFCombatWeapon::IsOwnerEMPed()
{
	CBaseTFPlayer *pPlayer = ToBaseTFPlayer( GetOwner() );
	if ((!pPlayer) || (!pPlayer->GetPlayerClass()))
		return false;

	return ( pPlayer->HasPowerup(POWERUP_EMP) );
}

//-----------------------------------------------------------------------------
// Purpose: Temporarily remove disguise
//-----------------------------------------------------------------------------
void CBaseTFCombatWeapon::CheckRemoveDisguise( void )
{
#if !defined( CLIENT_DLL )
	CBaseTFPlayer *player = static_cast< CBaseTFPlayer * >( GetOwner());
	if ( player )
	{
		// Always remove camo
		player->ClearCamouflage();
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Factor in the player's anim speed to the sequence duration for weapons
//-----------------------------------------------------------------------------
float CBaseTFCombatWeapon::SequenceDuration( int iSequence )
{
	float flDuration = BaseClass::SequenceDuration( iSequence );
	CBaseTFPlayer *pOwner = (CBaseTFPlayer *)GetOwner();
	if ( pOwner )
	{
		flDuration /= pOwner->GetDefaultAnimSpeed();
	}

	return flDuration;
}


//-----------------------------------------------------------------------------
// Purpose: Override weapon sound. TF doesn't use ATTN_GUNFIRE for it's weapon attenuations.
//-----------------------------------------------------------------------------
void CBaseTFCombatWeapon::WeaponSound( WeaponSound_t shoot_type, float soundtime /*= 0.0f*/ )
{
#if !defined( CLIENT_DLL )
	// HACKHACK: Force a combat sound to alert NPCs, for antlion prototype
	CSoundEnt::InsertSound( SOUND_COMBAT, GetAbsOrigin(), 600, 0.1 );
#endif

	BaseClass::WeaponSound( shoot_type, soundtime );
}

//-----------------------------------------------------------------------------
// Purpose: Get the point from which to create the weapon's tracer
//-----------------------------------------------------------------------------
Vector CBaseTFCombatWeapon::GetTracerSrc( Vector &vecSrc, Vector &vecFireDir )
{
	QAngle vecAngles;
	VectorAngles( vecFireDir, vecAngles );
	Vector right;
	AngleVectors( vecAngles, NULL, &right, NULL );
	return (vecSrc + Vector ( 0,0,-4 ) + right * 2 + vecFireDir * 16);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : activity - 
//-----------------------------------------------------------------------------
void CBaseTFCombatWeapon::PlayAttackAnimation( int activity )
{
	SendWeaponAnim( activity );
	CBaseTFPlayer *pPlayer = ToBaseTFPlayer( GetOwner() );
	if ( pPlayer )
	{
		pPlayer->SetAnimation( PLAYER_ATTACK1 );
		pPlayer->SetLastAttackTime( gpGlobals->curtime );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : sequence - 
//-----------------------------------------------------------------------------
bool CBaseTFCombatWeapon::SendWeaponAnim( int iActivity )
{
	CBaseTFPlayer *pPlayer = ToBaseTFPlayer( GetOwner() );
	if ( !pPlayer )
		return false;

	CBaseTFCombatWeapon *pOther = NULL;

	// See if we are wielding multiple weapons
	CWeaponTwoHandedContainer *pContainer = dynamic_cast< CWeaponTwoHandedContainer * >( pPlayer->GetActiveWeapon() );
	if ( pContainer )
	{
		// Make sure they exist
		CBaseTFCombatWeapon  *left = static_cast< CBaseTFCombatWeapon  * >( pContainer->GetLeftWeapon() );
		CBaseTFCombatWeapon  *right = static_cast< CBaseTFCombatWeapon  * >( pContainer->GetRightWeapon() );
		if ( !left || !right )
			return false;

		// Make sure one of them is this!!!
		if ( left != this && right != this )
			return false;

		// Get pointer to other one
		pOther = left;
		if ( left == this )
		{
			pOther = right;
		}

		Assert(pOther);

		// Now ask our other weapon if it would like to stomp my animation attempt
		iActivity = pOther->ReplaceOtherWeaponsActivity( iActivity );
		if ( iActivity == -1 )
			return false;
	}

	// Always pass through to base
	BaseClass::SendWeaponAnim( iActivity );

	if ( !IsReflectingAnimations() )
		return false;

	// See if we are wielding multiple weapons
	if ( !pContainer )
		return false;

	Assert(pOther);
	// Send our other weapon the activity code
	iActivity = GetOtherWeaponsActivity( iActivity );
	if ( iActivity != -1 )
	{
		pOther->SendWeaponAnim( iActivity );

		// Remember it for weapon switching
		m_iLastReflectedActivity = iActivity;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : reflect - 
//-----------------------------------------------------------------------------
void CBaseTFCombatWeapon::SetReflectViewModelAnimations( bool reflect )
{
	m_bReflectViewModelAnimations = reflect;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseTFCombatWeapon::IsReflectingAnimations( void ) const
{
	return m_bReflectViewModelAnimations;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CBaseTFCombatWeapon::IsCamouflaged( void )
{
	CBaseTFPlayer *pPlayer = (CBaseTFPlayer *)GetOwner();
	if ( pPlayer && pPlayer->IsCamouflaged() )
		return true;
		
	return false;
}

#if defined( CLIENT_DLL )
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
float CBaseTFCombatWeapon::CalcViewmodelBob( void )
{
	static	double	bobtime;
	static float	bob;
	float	cycle;
	
	CBasePlayer *player = ToBasePlayer( GetOwner() );
	if ( !player )
		return 0.0f;

	if ( ( player->GetGroundEntity() == NULL ) || !gpGlobals->frametime )
	{
		return bob;		// just use old value
	}

	bobtime += gpGlobals->frametime;
	
	cycle = bobtime - (int)(bobtime/cl_bobcycle.GetFloat())*cl_bobcycle.GetFloat();
	cycle /= cl_bobcycle.GetFloat();

	if (cycle < cl_bobup.GetFloat())
	{
		cycle = M_PI * cycle / cl_bobup.GetFloat();
	}
	else
	{
		cycle = M_PI + M_PI*(cycle-cl_bobup.GetFloat())/(1.0 - cl_bobup.GetFloat());
	}

	// bob is proportional to simulated velocity in the xy plane
	// (don't count Z, or jumping messes it up)
	bob = player->GetAbsVelocity().Length2D() * cl_bob.GetFloat();
	
	bob = bob*0.3 + bob*0.7*sin(cycle);

	bob = MIN( 4.0, bob );
	bob = MAX( -7.0, bob );
	return bob;
	
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : float
//-----------------------------------------------------------------------------
void CBaseTFCombatWeapon::AddViewmodelBob( CBaseViewModel *viewmodel, Vector &origin, QAngle &angles )
{
	float fIdleScale = 2.0f;

	// Bias so view models aren't all synced to each other
	//float curtime = gpGlobals->curtime + ( viewmodelindex * 2 * M_PI / GetViewModelCount() );

	float curtime = gpGlobals->curtime + ( viewmodel->entindex() * 2 * M_PI );

	origin[ROLL]	-= fIdleScale * sin(curtime*v_iroll_cycle.GetFloat()) * v_iroll_level.GetFloat();
	origin[PITCH]	-= fIdleScale * sin(curtime*v_ipitch_cycle.GetFloat()) * (v_ipitch_level.GetFloat() * 0.5);
	origin[YAW]		-= fIdleScale * sin(curtime*v_iyaw_cycle.GetFloat()) * v_iyaw_level.GetFloat();

	Vector	forward;
	AngleVectors( angles, &forward, NULL, NULL );

	float	flBob = CalcViewmodelBob();

	// Apply bob, but scaled down to 40%
	VectorMA( origin, flBob * 0.4, forward, origin );

	// Z bob a bit more
	origin[2] += flBob;

	// throw in a little tilt.
	angles[ YAW ]	-= flBob * 0.6;
	angles[ ROLL ]	-= flBob * 0.5;
	angles[ PITCH ]	-= flBob * 0.4;
}

//-----------------------------------------------------------------------------
// Purpose: TF specific weapon anim events
//-----------------------------------------------------------------------------
bool CBaseTFCombatWeapon::OnFireEvent( C_BaseViewModel *pViewModel, const Vector& origin, const QAngle& angles, int event, const char *options )
{
	switch( event )
	{
	case CL_EVENT_MUZZLEFLASH0:
	case CL_EVENT_MUZZLEFLASH1:
	case CL_EVENT_MUZZLEFLASH2:
	case CL_EVENT_MUZZLEFLASH3:
		{
			int iAttachment = -1;
			Vector attachOrigin;
			QAngle attachAngles; 

			// First person muzzle flashes
			switch (event) 
			{
			case CL_EVENT_MUZZLEFLASH0:
				iAttachment = 0;
				break;

			case CL_EVENT_MUZZLEFLASH1:
				iAttachment = 1;
				break;

			case CL_EVENT_MUZZLEFLASH2:
				iAttachment = 2;
				break;

			case CL_EVENT_MUZZLEFLASH3:
				iAttachment = 3;
				break;
			}

			// Did we find it?
			if ( pViewModel->GetAttachment( iAttachment+1, attachOrigin, attachAngles ) )
			{
				//int iType = atoi( options );

				// Is our owner boosted?
				CBasePlayer *pPlayer = ToBaseTFPlayer( GetOwner() );
				if ( pPlayer && pPlayer->HasPowerup(POWERUP_BOOST) )
				{
					unsigned char color[3];
					color[0] = 50;
					color[1] = 255;
					color[2] = 50;
					FX_MuzzleEffect( attachOrigin, attachAngles, 1.0, GetRefEHandle(), &color[0] );
				}
				else
				{
					FX_MuzzleEffect( attachOrigin, attachAngles, 1.0, GetRefEHandle() );
				}

				return true;
			}
		}
		break;

	default:
		break;
	}

	return false;
}

//============================================================================================================
// OWNER BOOSTED PROXY FOR WEAPONS / PLAYERS
//============================================================================================================
class CPlayerBoostedProxy : public CResultProxy
{
public:
	bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	void OnBind( void *pC_BaseEntity );

private:
	CFloatInput	m_Factor;
};

bool CPlayerBoostedProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	if (!CResultProxy::Init( pMaterial, pKeyValues ))
		return false;

	if (!m_Factor.Init( pMaterial, pKeyValues, "scale", 1 ))
		return false;

	return true;
}

void CPlayerBoostedProxy::OnBind( void *pRenderable )
{
	// Find the view angle between the player and this entity....
	IClientRenderable *pRend = (IClientRenderable *)pRenderable;
	C_BaseEntity *pEntity = pRend->GetIClientUnknown()->GetBaseEntity();
	CBaseTFPlayer *pPlayer = NULL;
	C_BaseViewModel *pViewModel = dynamic_cast<C_BaseViewModel*>(pEntity);
	if ( pViewModel )
	{
		pPlayer = C_BaseTFPlayer::GetLocalPlayer();
	}
	else
	{
		CBaseTFCombatWeapon *pWeapon = dynamic_cast<CBaseTFCombatWeapon*>(pEntity);
		if ( pWeapon )
		{
			pPlayer = ToBaseTFPlayer( pWeapon->GetOwner() );
		}
		else 
		{
			pPlayer = dynamic_cast<CBaseTFPlayer*>(pEntity);
		}
	}

	// Find him?
	if ( pPlayer )
	{
		float flBoosted = (int)pPlayer->HasPowerup( POWERUP_BOOST );
		SetFloatResult( flBoosted * m_Factor.GetFloat() );
	}
}

EXPOSE_INTERFACE( CPlayerBoostedProxy, IMaterialProxy, "PlayerBoosted" IMATERIAL_PROXY_INTERFACE_VERSION );


#else

//-----------------------------------------------------------------------------
// Purpose: 
// Output : float
//-----------------------------------------------------------------------------
float CBaseTFCombatWeapon::CalcViewmodelBob( void )
{
	return 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : float
//-----------------------------------------------------------------------------
void CBaseTFCombatWeapon::AddViewmodelBob( CBaseViewModel *viewmodel, Vector &origin, QAngle &angles )
{
}

#endif

LINK_ENTITY_TO_CLASS( basetfcombatweapon, CBaseTFCombatWeapon );

IMPLEMENT_NETWORKCLASS_ALIASED( BaseTFCombatWeapon , DT_BaseTFCombatWeapon )

BEGIN_NETWORK_TABLE( CBaseTFCombatWeapon , DT_BaseTFCombatWeapon )
#if !defined( CLIENT_DLL )

	 // Don't network any animation stuff to client
 	SendPropExclude( "DT_AnimTimeMustBeFirst", "m_flAnimTime" ),
 	SendPropExclude( "DT_BaseAnimating", "m_flCycle" ),

	SendPropInt( SENDINFO( m_bReflectViewModelAnimations ), 1, SPROP_UNSIGNED ),
#else
	RecvPropInt( RECVINFO( m_bReflectViewModelAnimations ) ),
#endif
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CBaseTFCombatWeapon )

	// If true, reflect and weapon animations to all view models
	DEFINE_PRED_FIELD( m_bReflectViewModelAnimations, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_FIELD( m_iLastReflectedActivity, FIELD_INTEGER )

END_PREDICTION_DATA()
