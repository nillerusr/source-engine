//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "in_buttons.h"
#include "takedamageinfo.h"
#include "weapon_dodbase.h"
#include "ammodef.h"
#include "dod_gamerules.h"

#ifdef CLIENT_DLL
extern IVModelInfoClient* modelinfo;
#else
extern IVModelInfo* modelinfo;
#include "ilagcompensationmanager.h"
#endif


#if defined( CLIENT_DLL )

	#include "vgui/ISurface.h"
	#include "vgui_controls/Controls.h"
	#include "c_dod_player.h"
	#include "hud_crosshair.h"
	#include "SoundEmitterSystem/isoundemittersystembase.h"

#else

	#include "dod_player.h"

#endif

#include "effect_dispatch_data.h"


// ----------------------------------------------------------------------------- //
// Global functions.
// ----------------------------------------------------------------------------- //

bool IsAmmoType( int iAmmoType, const char *pAmmoName )
{
	return GetAmmoDef()->Index( pAmmoName ) == iAmmoType;
}

//--------------------------------------------------------------------------------------------------------
//
// Given a weapon ID, return its alias
//
const char *WeaponIDToAlias( int id )
{
	if ( (id >= WEAPON_MAX) || (id < 0) )
		return NULL;

	return s_WeaponAliasInfo[id];
}

// ----------------------------------------------------------------------------- //
// CWeaponDODBase tables.
// ----------------------------------------------------------------------------- //

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponDODBase, DT_WeaponDODBase )

BEGIN_NETWORK_TABLE( CWeaponDODBase, DT_WeaponDODBase )
#ifdef CLIENT_DLL
	RecvPropInt( RECVINFO(m_iReloadModelIndex) ),
	RecvPropVector( RECVINFO( m_vInitialDropVelocity ) ),
	RecvPropTime( RECVINFO( m_flSmackTime ) )
#else
	SendPropVector( SENDINFO( m_vInitialDropVelocity ), 
			20,		// nbits
			0,		// flags
			-3000,	// low value
			3000	// high value
			),
	SendPropModelIndex( SENDINFO(m_iReloadModelIndex) ),
	SendPropTime( SENDINFO( m_flSmackTime ) )
#endif
END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( weapon_dod_base, CWeaponDODBase );


#ifdef GAME_DLL

	BEGIN_DATADESC( CWeaponDODBase )

		DEFINE_FUNCTION( FallThink ),
		DEFINE_FUNCTION( Die ),

		DEFINE_FUNCTION( Smack )

	END_DATADESC()

#else
	BEGIN_PREDICTION_DATA( CWeaponDODBase ) 
		DEFINE_PRED_FIELD( m_flSmackTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),	// for rifle melee attacks
		DEFINE_FIELD( m_bInAttack, FIELD_BOOLEAN )
	END_PREDICTION_DATA()
#endif

Vector head_hull_mins( -16, -16, -18 );
Vector head_hull_maxs( 16, 16, 18 );

void FindHullIntersection( const Vector &vecSrc, trace_t &tr, const Vector &mins, const Vector &maxs, CBaseEntity *pEntity )
{
	int			i, j, k;
	float		distance;
	Vector minmaxs[2] = {mins, maxs};
	trace_t tmpTrace;
	Vector		vecHullEnd = tr.endpos;
	Vector		vecEnd;

	CTraceFilterSimple filter( pEntity, COLLISION_GROUP_NONE );

	distance = 1e6f;

	vecHullEnd = vecSrc + ((vecHullEnd - vecSrc)*2);
	UTIL_TraceLine( vecSrc, vecHullEnd, MASK_SOLID, &filter, &tmpTrace );
	if ( tmpTrace.fraction < 1.0 )
	{
		tr = tmpTrace;
		return;
	}

	for ( i = 0; i < 2; i++ )
	{
		for ( j = 0; j < 2; j++ )
		{
			for ( k = 0; k < 2; k++ )
			{
				vecEnd.x = vecHullEnd.x + minmaxs[i][0];
				vecEnd.y = vecHullEnd.y + minmaxs[j][1];
				vecEnd.z = vecHullEnd.z + minmaxs[k][2];

				UTIL_TraceLine( vecSrc, vecEnd, MASK_SOLID, &filter, &tmpTrace );
				if ( tmpTrace.fraction < 1.0 )
				{
					float thisDistance = (tmpTrace.endpos - vecSrc).Length();
					if ( thisDistance < distance )
					{
						tr = tmpTrace;
						distance = thisDistance;
					}
				}
			}
		}
	}
}

// ----------------------------------------------------------------------------- //
// CWeaponDODBase implementation. 
// ----------------------------------------------------------------------------- //
CWeaponDODBase::CWeaponDODBase()
{
	SetPredictionEligible( true );
	m_bInAttack = false;
	m_iAltFireHint = 0;
	AddSolidFlags( FSOLID_TRIGGER ); // Nothing collides with these but it gets touches.

	m_flNextPrimaryAttack = 0;
}


bool CWeaponDODBase::IsPredicted() const
{ 
	return true;
}

bool CWeaponDODBase::PlayEmptySound()
{
	CPASAttenuationFilter filter( this );
	filter.UsePredictionRules();
	EmitSound( filter, entindex(), "Default.ClipEmpty_Rifle" );

	return false;
}


CBasePlayer* CWeaponDODBase::GetPlayerOwner() const
{
	return dynamic_cast< CBasePlayer* >( GetOwner() );
}

CDODPlayer* CWeaponDODBase::GetDODPlayerOwner() const
{
	return dynamic_cast< CDODPlayer* >( GetOwner() );
}

bool CWeaponDODBase::SendWeaponAnim( int iActivity )
{
	return BaseClass::SendWeaponAnim( iActivity );
}

bool CWeaponDODBase::CanAttack( void )
{
	CDODPlayer *pPlayer = ToDODPlayer( GetPlayerOwner() );

	if ( pPlayer )
	{
		return pPlayer->CanAttack();
	}

	return false;
}

bool CWeaponDODBase::ShouldAutoReload( void )
{
	CDODPlayer *pPlayer = ToDODPlayer( GetPlayerOwner() );

	if ( pPlayer )
	{
		return pPlayer->ShouldAutoReload();
	}

	return false;
}

void CWeaponDODBase::ItemPostFrame()
{
	if ( m_flSmackTime > 0 && gpGlobals->curtime > m_flSmackTime )
	{
		Smack();
		m_flSmackTime = -1;
	}

	CBasePlayer *pPlayer = GetPlayerOwner();

	if ( !pPlayer )
		return;

#ifdef _DEBUG
	CDODGameRules *mp = DODGameRules();
#endif

	assert( mp );

	if ((m_bInReload) && (pPlayer->m_flNextAttack <= gpGlobals->curtime))
	{
		// complete the reload. 
		int j = MIN( GetMaxClip1() - m_iClip1, pPlayer->GetAmmoCount( m_iPrimaryAmmoType ) );	

		// Add them to the clip
		m_iClip1 += j;
		pPlayer->RemoveAmmo( j, m_iPrimaryAmmoType );

		m_bInReload = false;

		FinishReload();
	}

	if ((pPlayer->m_nButtons & IN_ATTACK2) && (m_flNextSecondaryAttack <= gpGlobals->curtime))
	{
		if ( m_iClip2 != -1 && !pPlayer->GetAmmoCount( GetSecondaryAmmoType() ) )
		{
			m_bFireOnEmpty = TRUE;
		}

		SecondaryAttack();

		pPlayer->m_nButtons &= ~IN_ATTACK2;
	}
	else if ((pPlayer->m_nButtons & IN_ATTACK) && (m_flNextPrimaryAttack <= gpGlobals->curtime ) && !m_bInAttack )
	{
		if ( (m_iClip1 == 0/* && pszAmmo1()*/) || (GetMaxClip1() == -1 && !pPlayer->GetAmmoCount( GetPrimaryAmmoType() ) ) )
		{
			m_bFireOnEmpty = TRUE;
		}

		if( CanAttack() )
			PrimaryAttack();
	}
	else if ( pPlayer->m_nButtons & IN_RELOAD && GetMaxClip1() != WEAPON_NOCLIP && !m_bInReload && m_flNextPrimaryAttack < gpGlobals->curtime) 
	{
		// reload when reload is pressed, or if no buttons are down and weapon is empty.		
		Reload();	
	}
	else if ( !(pPlayer->m_nButtons & (IN_ATTACK|IN_ATTACK2) ) )
	{
		// no fire buttons down

		m_bFireOnEmpty = false;

		m_bInAttack = false;	//reset semi-auto

		if ( !IsUseable() && m_flNextPrimaryAttack < gpGlobals->curtime ) 
		{
			// Intentionally blank -- used to switch weapons here
		}
		else if( ShouldAutoReload() )
		{
			// weapon is useable. Reload if empty and weapon has waited as long as it has to after firing
			if ( m_iClip1 == 0 && !(GetWeaponFlags() & ITEM_FLAG_NOAUTORELOAD) && m_flNextPrimaryAttack < gpGlobals->curtime )
			{
				Reload();
				return;
			}
		}

		WeaponIdle( );
		return;
	}
}

void CWeaponDODBase::WeaponIdle()
{
	if (m_flTimeWeaponIdle > gpGlobals->curtime)
		return;

	SendWeaponAnim( GetIdleActivity() );

	m_flTimeWeaponIdle = gpGlobals->curtime + SequenceDuration();
}

Activity CWeaponDODBase::GetIdleActivity( void )
{
	return ACT_VM_IDLE;
}

const CDODWeaponInfo &CWeaponDODBase::GetDODWpnData() const
{
	const FileWeaponInfo_t *pWeaponInfo = &GetWpnData();
	const CDODWeaponInfo *pDODInfo;

	#ifdef _DEBUG
		pDODInfo = dynamic_cast< const CDODWeaponInfo* >( pWeaponInfo );
		Assert( pDODInfo );
	#else
		pDODInfo = static_cast< const CDODWeaponInfo* >( pWeaponInfo );
	#endif

	return *pDODInfo;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CWeaponDODBase::GetViewModel( int /*viewmodelindex = 0 -- this is ignored in the base class here*/ ) const
{
	if ( GetPlayerOwner() == NULL )
	{
		 return BaseClass::GetViewModel();
	}
	
	return GetWpnData().szViewModel;
}

void CWeaponDODBase::Precache( void )
{
	// precache base first, it loads weapon scripts
	BaseClass::Precache();

	PrecacheScriptSound( "Default.ClipEmpty_Rifle" );

	PrecacheParticleSystem( "muzzle_pistols" );
	PrecacheParticleSystem( "muzzle_fullyautomatic" );
	PrecacheParticleSystem( "muzzle_rifles" );
	PrecacheParticleSystem( "muzzle_rockets" );
	PrecacheParticleSystem( "muzzle_mg42" );

	PrecacheParticleSystem( "view_muzzle_pistols" );
	PrecacheParticleSystem( "view_muzzle_fullyautomatic" );
	PrecacheParticleSystem( "view_muzzle_rifles" );
	PrecacheParticleSystem( "view_muzzle_rockets" );
	PrecacheParticleSystem( "view_muzzle_mg42" );

	const CDODWeaponInfo &info = GetDODWpnData();

	int iWpnNameLen = Q_strlen(info.m_szReloadModel);

#ifdef DEBUG
	// Make sure that if we declare an alt weapon, that we have criteria to show it
	// and vice-versa
	//Assert( ((info.m_iAltWpnCriteria & (ALTWPN_CRITERIA_RELOADING | ALTWPN_CRITERIA_FIRING)) > 0 ) ==
	//		(iWpnNameLen > 0) );  	
#endif

	if( iWpnNameLen > 0 )
		m_iReloadModelIndex = CBaseEntity::PrecacheModel( info.m_szReloadModel );	
}

bool CWeaponDODBase::DefaultDeploy( char *szViewModel, char *szWeaponModel, int iActivity, char *szAnimExt )
{
	CBasePlayer *pOwner = GetPlayerOwner();
	if ( !pOwner )
	{
		return false;
	}

	pOwner->SetAnimationExtension( szAnimExt );

	SetViewModel();
	SendWeaponAnim( iActivity );

	pOwner->SetNextAttack( gpGlobals->curtime + SequenceDuration() );
	m_flNextPrimaryAttack	= MAX( m_flNextPrimaryAttack, gpGlobals->curtime );
	m_flNextSecondaryAttack	= gpGlobals->curtime;

	SetWeaponVisible( true );
	SetWeaponModelIndex( szWeaponModel );

	CBaseViewModel *vm = pOwner->GetViewModel( m_nViewModelIndex );

	Assert( vm );

	if( vm )
	{
		//set sleeves to proper team
		switch( pOwner->GetTeamNumber() )
		{
		case TEAM_ALLIES:
			vm->m_nSkin = SLEEVE_ALLIES;
			break;
		case TEAM_AXIS:
			vm->m_nSkin = SLEEVE_AXIS;
			break;
		default:
			Assert( !"TEAM_UNASSIGNED or spectator getting a view model assigned" );
			break;
		}
	}	

	return true;
}

void CWeaponDODBase::SetWeaponModelIndex( const char *pName )
{
 	 m_iWorldModelIndex = modelinfo->GetModelIndex( pName );
}

bool CWeaponDODBase::CanBeSelected( void )
{
	if ( !VisibleInWeaponSelection() )
		return false;

	return true;
}

bool CWeaponDODBase::CanDeploy( void )
{
	return BaseClass::CanDeploy();
}

bool CWeaponDODBase::CanHolster( void )
{
	return BaseClass::CanHolster();
}

void CWeaponDODBase::Drop( const Vector &vecVelocity )
{
#ifndef CLIENT_DLL
	if ( m_iAltFireHint )
	{
		CDODPlayer *pPlayer = GetDODPlayerOwner();
		if ( pPlayer )
		{
			pPlayer->StopHintTimer( m_iAltFireHint );
		}
	}
#endif

	// cancel any reload in progress
	m_bInReload = false;

	m_flSmackTime = -1;
	
	m_vInitialDropVelocity = vecVelocity;

	BaseClass::Drop( m_vInitialDropVelocity );
}

bool CWeaponDODBase::Holster( CBaseCombatWeapon *pSwitchingTo )
{
#ifndef CLIENT_DLL
	CDODPlayer *pPlayer = GetDODPlayerOwner();

	if ( pPlayer )
	{
		pPlayer->SetFOV( pPlayer, 0 ); // reset the default FOV.

		if ( m_iAltFireHint )
		{
			pPlayer->StopHintTimer( m_iAltFireHint );
		}
	}
#endif

	m_bInReload = false;

	m_flSmackTime = -1;

	return BaseClass::Holster( pSwitchingTo );
}

bool CWeaponDODBase::Deploy()
{
#ifndef CLIENT_DLL
	CDODPlayer *pPlayer = GetDODPlayerOwner();

	if ( pPlayer )
	{
		pPlayer->SetFOV( pPlayer, 0 );

		if ( m_iAltFireHint )
		{
			pPlayer->StartHintTimer( m_iAltFireHint );
		}
	}
#endif

	return BaseClass::Deploy();
}

#ifdef CLIENT_DLL
	
	void CWeaponDODBase::PostDataUpdate( DataUpdateType_t updateType )
	{
		// We need to do this before the C_BaseAnimating code starts to drive
		// clientside animation sequences on this model, which will be using bad sequences for the world model.
		int iDesiredModelIndex = 0;
		C_BasePlayer *localplayer = C_BasePlayer::GetLocalPlayer();
		if ( localplayer && localplayer == GetOwner() && !C_BasePlayer::ShouldDrawLocalPlayer() )		// FIXME: use localplayer->ShouldDrawThisPlayer() instead.
		{
			iDesiredModelIndex = m_iViewModelIndex;
		}
		else
		{
			iDesiredModelIndex = GetWorldModelIndex();

			// Our world models never animate
			SetSequence( 0 );
		}

		if ( GetModelIndex() != iDesiredModelIndex )
		{
			SetModelIndex( iDesiredModelIndex );
		}

		BaseClass::PostDataUpdate( updateType );
	}

	void CWeaponDODBase::OnDataChanged( DataUpdateType_t type )
	{
		if ( m_iState == WEAPON_NOT_CARRIED && m_iOldState != WEAPON_NOT_CARRIED ) 
		{
			// we are being notified of the weapon being dropped
			// add an interpolation history so the movement is smoother

			// Now stick our initial velocity into the interpolation history 
			CInterpolatedVar< Vector > &interpolator = GetOriginInterpolator();

			interpolator.ClearHistory();
			float changeTime = GetLastChangeTime( LATCH_SIMULATION_VAR );

			// Add a sample 1 second back.
			Vector vCurOrigin = GetLocalOrigin() - m_vInitialDropVelocity;
			interpolator.AddToHead( changeTime - 1.0, &vCurOrigin, false );

			// Add the current sample.
			vCurOrigin = GetLocalOrigin();
			interpolator.AddToHead( changeTime, &vCurOrigin, false );

			Vector estVel;
			EstimateAbsVelocity( estVel );

			/*Msg( "estimated velocity ( %.1f %.1f %.1f )  initial velocity ( %.1f %.1f %.1f )\n",
				estVel.x,
				estVel.y,
				estVel.z,
				m_vInitialDropVelocity.m_Value.x,
				m_vInitialDropVelocity.m_Value.y,
				m_vInitialDropVelocity.m_Value.z );*/

			OnWeaponDropped();
		}

		BaseClass::OnDataChanged( type );

		if ( GetPredictable() && !ShouldPredict() )
			ShutdownPredictable();
	}

	int CWeaponDODBase::GetWorldModelIndex( void )
	{
		if( m_bUseAltWeaponModel && GetOwner() != NULL )
			return m_iReloadModelIndex;
		else
            return m_iWorldModelIndex;
	}

	bool CWeaponDODBase::ShouldPredict()
	{
		if ( GetOwner() && GetOwner() == C_BasePlayer::GetLocalPlayer() )
			return true;

		return BaseClass::ShouldPredict();
	}

	void CWeaponDODBase::ProcessMuzzleFlashEvent()
	{
		CDODPlayer *pPlayer = GetDODPlayerOwner();
		if ( pPlayer )
			pPlayer->ProcessMuzzleFlashEvent();

		BaseClass::ProcessMuzzleFlashEvent();
	}
#else
		

	//-----------------------------------------------------------------------------
	// Purpose: Get the accuracy derived from weapon and player, and return it
	//-----------------------------------------------------------------------------
	const Vector& CWeaponDODBase::GetBulletSpread()
	{
		static Vector cone = VECTOR_CONE_8DEGREES;
		return cone;
	}

	//-----------------------------------------------------------------------------
	// Purpose: 
	//-----------------------------------------------------------------------------
	void CWeaponDODBase::ItemBusyFrame()
	{
		if( ShouldAutoReload() && !m_bInReload )
		{
			// weapon is useable. Reload if empty and weapon has waited as long as it has to after firing
			if ( m_iClip1 == 0 && !(GetWeaponFlags() & ITEM_FLAG_NOAUTORELOAD) && m_flNextPrimaryAttack < gpGlobals->curtime )
			{
				Reload();
			}
		}

		BaseClass::ItemBusyFrame();
	}

	//-----------------------------------------------------------------------------
	// Purpose: Match the anim speed to the weapon speed while crouching
	//-----------------------------------------------------------------------------
	float CWeaponDODBase::GetDefaultAnimSpeed()
	{
		return 1.0;
	}

	bool CWeaponDODBase::ShouldRemoveOnRoundRestart()
	{
		if ( GetPlayerOwner() )
			return false;
		else
			return true;
	}


	//=========================================================
	// Materialize - make a CWeaponDODBase visible and tangible
	//=========================================================
	void CWeaponDODBase::Materialize()
	{
		if ( IsEffectActive( EF_NODRAW ) )
		{
			RemoveEffects( EF_NODRAW );
			DoMuzzleFlash();
		}

		AddSolidFlags( FSOLID_TRIGGER );

		SetThink (&CWeaponDODBase::SUB_Remove);
		SetNextThink( gpGlobals->curtime + 1 );
	}

	//=========================================================
	// AttemptToMaterialize - the item is trying to rematerialize,
	// should it do so now or wait longer?
	//=========================================================
	void CWeaponDODBase::AttemptToMaterialize()
	{
		float time = g_pGameRules->FlWeaponTryRespawn( this );

		if ( time == 0 )
		{
			Materialize();
			return;
		}

		SetNextThink( gpGlobals->curtime + time );
	}

	//=========================================================
	// CheckRespawn - a player is taking this weapon, should 
	// it respawn?
	//=========================================================
	void CWeaponDODBase::CheckRespawn()
	{
		//GOOSEMAN : Do not respawn weapons!
		return;
	}

		
	//=========================================================
	// Respawn- this item is already in the world, but it is
	// invisible and intangible. Make it visible and tangible.
	//=========================================================
	CBaseEntity* CWeaponDODBase::Respawn()
	{
		// make a copy of this weapon that is invisible and inaccessible to players (no touch function). The weapon spawn/respawn code
		// will decide when to make the weapon visible and touchable.
		CBaseEntity *pNewWeapon = CBaseEntity::Create( GetClassname(), g_pGameRules->VecWeaponRespawnSpot( this ), GetAbsAngles(), GetOwner() );

		if ( pNewWeapon )
		{
			pNewWeapon->AddEffects( EF_NODRAW );// invisible for now
			pNewWeapon->SetTouch( NULL );// no touch
			pNewWeapon->SetThink( &CWeaponDODBase::AttemptToMaterialize );

			UTIL_DropToFloor( this, MASK_SOLID );

			// not a typo! We want to know when the weapon the player just picked up should respawn! This new entity we created is the replacement,
			// but when it should respawn is based on conditions belonging to the weapon that was taken.
			pNewWeapon->SetNextThink( gpGlobals->curtime + g_pGameRules->FlWeaponRespawnTime( this ) );
		}
		else
		{
			Msg( "Respawn failed to create %s!\n", GetClassname() );
		}

		return pNewWeapon;
	}

	bool CWeaponDODBase::Reload()
	{
		return BaseClass::Reload();
	}

	void CWeaponDODBase::Spawn()
	{
		BaseClass::Spawn();

		// Set this here to allow players to shoot dropped weapons
		SetCollisionGroup( COLLISION_GROUP_WEAPON );
		
		SetExtraAmmoCount(0);	//Start with no additional ammo

		CollisionProp()->UseTriggerBounds( true, 10.0f );
	}
	
	void CWeaponDODBase::SetDieThink( bool bDie )
	{
		if( bDie )
			SetContextThink( &CWeaponDODBase::Die, gpGlobals->curtime + 45.0f, "DieContext" );
		else
			SetContextThink( NULL, gpGlobals->curtime, "DieContext" );
	}

	void CWeaponDODBase::Die( void )
	{
		UTIL_Remove( this );
	}

#endif

void CWeaponDODBase::OnPickedUp( CBaseCombatCharacter *pNewOwner )
{
	BaseClass::OnPickedUp( pNewOwner );

#if !defined( CLIENT_DLL )
	SetDieThink( false );
#endif
}

bool CWeaponDODBase::DefaultReload( int iClipSize1, int iClipSize2, int iActivity )
{
	CBaseCombatCharacter *pOwner = GetOwner();
	if (!pOwner)
		return false;

	// If I don't have any spare ammo, I can't reload
	if ( pOwner->GetAmmoCount(m_iPrimaryAmmoType) <= 0 )
		return false;

	bool bReload = false;

	// If you don't have clips, then don't try to reload them.
	if ( UsesClipsForAmmo1() )
	{
		// need to reload primary clip?
		int primary	= min(iClipSize1 - m_iClip1, pOwner->GetAmmoCount(m_iPrimaryAmmoType));
		if ( primary != 0 )
		{
			bReload = true;
		}
	}

	if ( UsesClipsForAmmo2() )
	{
		// need to reload secondary clip?
		int secondary = min(iClipSize2 - m_iClip2, pOwner->GetAmmoCount(m_iSecondaryAmmoType));
		if ( secondary != 0 )
		{
			bReload = true;
		}
	}

	if ( !bReload )
		return false;

	CDODPlayer *pPlayer = GetDODPlayerOwner();
	if ( pPlayer )
	{
#ifdef CLIENT_DLL
		PlayWorldReloadSound( pPlayer );
#else
		pPlayer->DoAnimationEvent( PLAYERANIMEVENT_RELOAD );
#endif
	}

	SendWeaponAnim( iActivity );

	// Play the player's reload animation
	if ( pOwner->IsPlayer() )
	{
		( ( CBasePlayer * )pOwner)->SetAnimation( PLAYER_RELOAD );
	}

	float flSequenceEndTime = gpGlobals->curtime + SequenceDuration();
	pOwner->SetNextAttack( flSequenceEndTime );
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = flSequenceEndTime;

	m_bInReload = true;

	return true;
}

#ifdef CLIENT_DLL
	void CWeaponDODBase::PlayWorldReloadSound( CDODPlayer *pPlayer )
	{
		Assert( pPlayer );

		const char *shootsound = GetShootSound( RELOAD );
		if ( !shootsound || !shootsound[0] )
			return;

		CSoundParameters params;

		if ( !GetParametersForSound( shootsound, params, NULL ) )
			return;

		// Play weapon sound from the owner
		CPASAttenuationFilter filter( pPlayer, params.soundlevel );
		filter.RemoveRecipient( pPlayer );	// no local player, that is done in the model

		EmitSound( filter, pPlayer->entindex(), shootsound, NULL, 0.0 ); 
	}
#endif

bool CWeaponDODBase::IsUseable()
{
	CBasePlayer *pPlayer = GetPlayerOwner();

	if ( Clip1() <= 0 )
	{
		if ( pPlayer->GetAmmoCount( GetPrimaryAmmoType() ) <= 0 && GetMaxClip1() != -1 )			
		{
			// clip is empty (or nonexistant) and the player has no more ammo of this type. 
			return false;
		}
	}

	return true;
}

#ifndef CLIENT_DLL
ConVar dod_meleeattackforcescale( "dod_meleeattackforcescale", "8.0", FCVAR_CHEAT | FCVAR_GAMEDLL );
#endif

void CWeaponDODBase::RifleButt( void )
{
	//MeleeAttack( 60, MELEE_DMG_BUTTSTOCK | MELEE_DMG_SECONDARYATTACK, 0.2f, 0.9f );
}

void CWeaponDODBase::Bayonet( void )
{
	//MeleeAttack( 60, MELEE_DMG_BAYONET | MELEE_DMG_SECONDARYATTACK, 0.2f, 0.9f );
}

void CWeaponDODBase::Punch( void )
{
	MeleeAttack( 60, MELEE_DMG_FIST | MELEE_DMG_SECONDARYATTACK, 0.2f, 0.4f );
}

//--------------------------------------------
// iDamageAmount - how much damage to give
// iDamageType - DMG_ bits 
// flDmgDelay - delay between attack and the giving of damage, usually timed to animation
// flAttackDelay - time until we can next attack 
//--------------------------------------------
CBaseEntity *CWeaponDODBase::MeleeAttack( int iDamageAmount, int iDamageType, float flDmgDelay, float flAttackDelay )
{
	if ( !CanAttack() )
		return NULL;

	CDODPlayer *pPlayer = ToDODPlayer( GetPlayerOwner() );

#if !defined (CLIENT_DLL)
	// Move other players back to history positions based on local player's lag
	lagcompensation->StartLagCompensation( pPlayer, pPlayer->GetCurrentCommand() );
#endif

	Vector vForward, vRight, vUp;
	AngleVectors( pPlayer->EyeAngles(), &vForward, &vRight, &vUp );
	Vector vecSrc	= pPlayer->Weapon_ShootPosition();
	Vector vecEnd	= vecSrc + vForward * 48;

	CTraceFilterSimple filter( pPlayer, COLLISION_GROUP_NONE );

	int iTraceMask = MASK_SOLID | CONTENTS_HITBOX | CONTENTS_DEBRIS;

	trace_t tr;
	UTIL_TraceLine( vecSrc, vecEnd, iTraceMask, &filter, &tr );

	const float rayExtension = 40.0f;
	UTIL_ClipTraceToPlayers( vecSrc, vecEnd + vForward * rayExtension, iTraceMask, &filter, &tr );

	// If the exact forward trace did not hit, try a larger swept box 
	if ( tr.fraction >= 1.0 )
	{
		Vector head_hull_mins( -16, -16, -18 );
		Vector head_hull_maxs( 16, 16, 18 );

		UTIL_TraceHull( vecSrc, vecEnd, head_hull_mins, head_hull_maxs, MASK_SOLID, &filter, &tr );
		if ( tr.fraction < 1.0 )
		{
			// Calculate the point of intersection of the line (or hull) and the object we hit
			// This is and approximation of the "best" intersection
			CBaseEntity *pHit = tr.m_pEnt;
			if ( !pHit || pHit->IsBSPModel() )
				FindHullIntersection( vecSrc, tr, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX, pPlayer );
			vecEnd = tr.endpos;	// This is the point on the actual surface (the hull could have hit space)

			// Make sure it is in front of us
			Vector vecToEnd = vecEnd - vecSrc;
			VectorNormalize( vecToEnd );

			// if zero length, always hit
			if ( vecToEnd.Length() > 0 )
			{
				float dot = DotProduct( vForward, vecToEnd );

				// sanity that our hit is within range
				if ( abs(dot) < 0.95 )
				{
					// fake that we actually missed
					tr.fraction = 1.0;
				}
			}			
		}
	}

	WeaponSound( MELEE_MISS );

	bool bDidHit = ( tr.fraction < 1.0f );

	if ( bDidHit )	//if the swing hit 
	{	
		// delay the decal a bit
		m_trHit = tr;

		// Store the ent in an EHANDLE, just in case it goes away by the time we get into our think function.
		m_pTraceHitEnt = tr.m_pEnt; 

		m_iSmackDamage = iDamageAmount;
		m_iSmackDamageType = iDamageType;

		m_flSmackTime = gpGlobals->curtime + flDmgDelay;
	}

	SendWeaponAnim( GetMeleeActivity() );

	// player animation
	pPlayer->DoAnimationEvent( PLAYERANIMEVENT_SECONDARY_ATTACK );

	m_flNextPrimaryAttack = gpGlobals->curtime + flAttackDelay;
	m_flNextSecondaryAttack = gpGlobals->curtime + flAttackDelay;
	m_flTimeWeaponIdle = gpGlobals->curtime + SequenceDuration();

#ifndef CLIENT_DLL
	IGameEvent * event = gameeventmanager->CreateEvent( "dod_stats_weapon_attack" );
	if ( event )
	{
		event->SetInt( "attacker", pPlayer->GetUserID() );
		event->SetInt( "weapon", GetAltWeaponID() );

		gameeventmanager->FireEvent( event );
	}

	lagcompensation->FinishLagCompensation( pPlayer );
#endif	//CLIENT_DLL

	return tr.m_pEnt;
}

//Think function to delay the impact decal until the animation is finished playing
void CWeaponDODBase::Smack()
{
	Assert( GetPlayerOwner() );

	if ( !GetPlayerOwner() )
		return;

	CDODPlayer *pPlayer = ToDODPlayer( GetPlayerOwner() );

	if ( !pPlayer )
		return;

	// Check that we are still facing the victim
	Vector vForward, vRight, vUp;
	AngleVectors( pPlayer->EyeAngles(), &vForward, &vRight, &vUp );
	Vector vecSrc	= pPlayer->Weapon_ShootPosition();
	Vector vecEnd	= vecSrc + vForward * 48;

	CTraceFilterSimple filter( pPlayer, COLLISION_GROUP_NONE );

	int iTraceMask = MASK_SOLID | CONTENTS_HITBOX | CONTENTS_DEBRIS;

	trace_t tr;
	UTIL_TraceLine( vecSrc, vecEnd, iTraceMask, &filter, &tr );

	const float rayExtension = 40.0f;
	UTIL_ClipTraceToPlayers( vecSrc, vecEnd + vForward * rayExtension, iTraceMask, &filter, &tr );

	if ( tr.fraction >= 1.0 )
	{
		Vector head_hull_mins( -16, -16, -18 );
		Vector head_hull_maxs( 16, 16, 18 );

		UTIL_TraceHull( vecSrc, vecEnd, head_hull_mins, head_hull_maxs, MASK_SOLID, &filter, &tr );
		if ( tr.fraction < 1.0 )
		{
			// Calculate the point of intersection of the line (or hull) and the object we hit
			// This is and approximation of the "best" intersection
			CBaseEntity *pHit = tr.m_pEnt;
			if ( !pHit || pHit->IsBSPModel() )
				FindHullIntersection( vecSrc, tr, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX, pPlayer );
			vecEnd = tr.endpos;	// This is the point on the actual surface (the hull could have hit space)
		}
	}

	m_trHit = tr;

	if ( !m_trHit.m_pEnt || (m_trHit.surface.flags & SURF_SKY) )
		return;

	if ( m_trHit.fraction == 1.0 )
		return;

	CPASAttenuationFilter attenuationFilter( this );
	attenuationFilter.UsePredictionRules();

	if( m_trHit.m_pEnt->IsPlayer() )
	{
		if ( m_iSmackDamageType & MELEE_DMG_STRONGATTACK )
			WeaponSound( SPECIAL1 );
		else
            WeaponSound( MELEE_HIT );
	}
	else
		WeaponSound( MELEE_HIT_WORLD );

	int iDamageType = DMG_CLUB | DMG_NEVERGIB;

#ifndef CLIENT_DLL
	//if they hit the bounding box, just assume a chest hit
	if( m_trHit.hitgroup == HITGROUP_GENERIC )
		m_trHit.hitgroup = HITGROUP_CHEST;

	float flDamage = (float)m_iSmackDamage;

	CTakeDamageInfo info( pPlayer, pPlayer, flDamage, iDamageType );

	if ( m_iSmackDamageType & MELEE_DMG_SECONDARYATTACK )
		info.SetDamageCustom( MELEE_DMG_SECONDARYATTACK );

	float flScale = (1.0f / flDamage) * dod_meleeattackforcescale.GetFloat();

	Vector vecForceDir = vForward;

	CalculateMeleeDamageForce( &info, vecForceDir, m_trHit.endpos, flScale );

	Assert( m_trHit.m_pEnt != GetPlayerOwner() );

	m_trHit.m_pEnt->DispatchTraceAttack( info, vForward, &m_trHit ); 
	ApplyMultiDamage();
#endif

	// We've gotten minidumps where this happened.
	if ( !GetPlayerOwner() )
		return;

	CEffectData data;
	data.m_vOrigin = m_trHit.endpos;
	data.m_vStart = m_trHit.startpos;
	data.m_nSurfaceProp = m_trHit.surface.surfaceProps;
	data.m_nHitBox = m_trHit.hitbox;
#ifdef CLIENT_DLL
	data.m_hEntity = m_trHit.m_pEnt->GetRefEHandle();
#else
	data.m_nEntIndex = m_trHit.m_pEnt->entindex();
#endif

	CPASFilter effectfilter( data.m_vOrigin );

#ifndef CLIENT_DLL
	effectfilter.RemoveRecipient( GetPlayerOwner() );
#endif

	data.m_vAngles = GetPlayerOwner()->GetAbsAngles();
	data.m_fFlags = 0x1;	//IMPACT_NODECAL;
	data.m_nDamageType = iDamageType;

	bool bHitPlayer = m_trHit.m_pEnt && m_trHit.m_pEnt->IsPlayer();

	// don't do any impacts if we hit a teammate and ff is off
	if ( bHitPlayer && 
		m_trHit.m_pEnt->GetTeamNumber() == GetPlayerOwner()->GetTeamNumber() && 
		!friendlyfire.GetBool() )
		return;

	if ( bHitPlayer )
	{
		te->DispatchEffect( effectfilter, 0.0, data.m_vOrigin, "Impact", data );
	}
	else if ( m_iSmackDamageType & MELEE_DMG_EDGE )
	{
		data.m_nDamageType = DMG_SLASH;
		te->DispatchEffect( effectfilter, 0.0, data.m_vOrigin, "KnifeSlash", data );
	}
}

#if defined( CLIENT_DLL )

	float	g_lateralBob = 0;
	float	g_verticalBob = 0;

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
	float CWeaponDODBase::CalcViewmodelBob( void )
	{
		static	float bobtime;
		static	float lastbobtime;
		static  float lastspeed;
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
		float flmaxSpeedDelta = MAX( 0, (gpGlobals->curtime - lastbobtime) * 320.0f );

		// don't allow too big speed changes
		speed = clamp( speed, lastspeed-flmaxSpeedDelta, lastspeed+flmaxSpeedDelta );
		speed = clamp( speed, -320, 320 );

		lastspeed = speed;

		//FIXME: This maximum speed value must come from the server.
		//		 MaxSpeed() is not sufficient for dealing with sprinting - jdw

		float bob_offset = RemapVal( speed, 0, 320, 0.0f, 1.0f );
		
		bobtime += ( gpGlobals->curtime - lastbobtime ) * bob_offset;
		lastbobtime = gpGlobals->curtime;

		//Calculate the vertical bob
		cycle = bobtime - (int)(bobtime/cl_bobcycle.GetFloat())*cl_bobcycle.GetFloat();
		cycle /= cl_bobcycle.GetFloat();

		if ( cycle < cl_bobup.GetFloat() )
		{
			cycle = M_PI * cycle / cl_bobup.GetFloat();
		}
		else
		{
			cycle = M_PI + M_PI*(cycle-cl_bobup.GetFloat())/(1.0 - cl_bobup.GetFloat());
		}
		
		g_verticalBob = speed*0.005f;
		g_verticalBob = g_verticalBob*0.3 + g_verticalBob*0.7*sin(cycle);

		g_verticalBob = clamp( g_verticalBob, -7.0f, 4.0f );

		//Calculate the lateral bob
		cycle = bobtime - (int)(bobtime/cl_bobcycle.GetFloat()*2)*cl_bobcycle.GetFloat()*2;
		cycle /= cl_bobcycle.GetFloat()*2;

		if ( cycle < cl_bobup.GetFloat() )
		{
			cycle = M_PI * cycle / cl_bobup.GetFloat();
		}
		else
		{
			cycle = M_PI + M_PI*(cycle-cl_bobup.GetFloat())/(1.0 - cl_bobup.GetFloat());
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
	void CWeaponDODBase::AddViewmodelBob( CBaseViewModel *viewmodel, Vector &origin, QAngle &angles )
	{
		Vector	forward, right;
		AngleVectors( angles, &forward, &right, NULL );

		CalcViewmodelBob();

		// Apply bob, but scaled down to 40%
		VectorMA( origin, g_verticalBob * 0.4f, forward, origin );
		
		// Z bob a bit more
		origin[2] += g_verticalBob * 0.1f;
		
		// bob the angles
		angles[ ROLL ]	+= g_verticalBob * 0.5f;
		angles[ PITCH ]	-= g_verticalBob * 0.4f;

		angles[ YAW ]	-= g_lateralBob  * 0.3f;

	//	VectorMA( origin, g_lateralBob * 0.2f, right, origin );
	}
	
	#include "c_te_effect_dispatch.h"

	#define NUM_MUZZLE_FLASH_TYPES 4

	bool CWeaponDODBase::OnFireEvent( C_BaseViewModel *pViewModel, const Vector& origin, const QAngle& angles, int event, const char *options )
	{
		if( event == 5001 )
		{
			if ( ShouldDrawMuzzleFlash() )
			{
				Assert( GetOwnerEntity() == C_BasePlayer::GetLocalPlayer() );

				const char *pszMuzzleFlashEffect;

				switch( GetDODWpnData().m_iMuzzleFlashType )
				{
				case DOD_MUZZLEFLASH_PISTOL:
					pszMuzzleFlashEffect = "view_muzzle_pistols";
					break;
				case DOD_MUZZLEFLASH_AUTO:
					pszMuzzleFlashEffect = "view_muzzle_fullyautomatic";
					break;
				case DOD_MUZZLEFLASH_RIFLE:
					pszMuzzleFlashEffect = "view_muzzle_rifles";
					break;
				case DOD_MUZZLEFLASH_MG:
					pszMuzzleFlashEffect = "view_muzzle_miniguns";
					break;
				case DOD_MUZZLEFLASH_ROCKET:
					pszMuzzleFlashEffect = "view_muzzle_rockets";
					break;
				case DOD_MUZZLEFLASH_MG42:
					pszMuzzleFlashEffect = "view_muzzle_mg42";
					break;
				default:
					pszMuzzleFlashEffect = NULL;
					break;
				}

				if ( pszMuzzleFlashEffect )
				{
					pViewModel->ParticleProp()->Create( pszMuzzleFlashEffect, PATTACH_POINT_FOLLOW, 1 );
				}				
			}
			return true;
		}
		else if( event == 6002 )
		{
			CEffectData data;
			data.m_nHitBox = atoi( options );
			data.m_hEntity = GetPlayerOwner() ? GetPlayerOwner()->GetRefEHandle() : INVALID_EHANDLE_INDEX;
			pViewModel->GetAttachment( 2, data.m_vOrigin, data.m_vAngles );

			DispatchEffect( "DOD_EjectBrass", data );
			return true;
		}

		return BaseClass::OnFireEvent( pViewModel, origin, angles, event, options );
	}

	bool CWeaponDODBase::ShouldAutoEjectBrass( void )
	{
		// Don't eject brass if further than N units from the local player
		C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
		if ( !pLocalPlayer )
			return true;

		float flMaxDistSqr = 250;
		flMaxDistSqr *= flMaxDistSqr;

		float flDistSqr = pLocalPlayer->EyePosition().DistToSqr( GetAbsOrigin() );
		return ( flDistSqr < flMaxDistSqr );
	}

	bool CWeaponDODBase::GetEjectBrassShellType( void )
	{
		return 1;
	}

	void CWeaponDODBase::SetUseAltModel( bool bUseAltModel )
	{
		m_bUseAltWeaponModel = bUseAltModel;
	}

	void CWeaponDODBase::CheckForAltWeapon( int iCurrentState )
	{
		int iCriteria = GetDODWpnData().m_iAltWpnCriteria;

		bool bUseAltModel = false;

		if( ( iCriteria & iCurrentState ) != 0 )
			bUseAltModel = true;

		SetUseAltModel( bUseAltModel );
	}

	Vector CWeaponDODBase::GetDesiredViewModelOffset( C_DODPlayer *pOwner )
	{
		Vector viewOffset = pOwner->GetViewOffset();

		float flPercent = ( viewOffset.z - VEC_PRONE_VIEW_SCALED( pOwner ).z ) / ( VEC_VIEW_SCALED( pOwner ).z - VEC_PRONE_VIEW_SCALED( pOwner ).z );

		return ( flPercent * GetDODWpnData().m_vecViewNormalOffset +
			( 1.0 - flPercent ) * GetDODWpnData().m_vecViewProneOffset );
	}

	bool CWeaponDODBase::ShouldDraw( void )
	{
		if ( GetModel() == NULL )
		{
			// XXX(johns): Removed, doesn't seem to be the proper spot for this warning given that weapons can call
			//             ShouldDraw before their properties are filled.

			// C_DODPlayer *pPlayer = C_DODPlayer::GetLocalDODPlayer();

			// Warning( "BADNESS! Tell Matt that the weapon '%s' tried to draw with a null model ( %d, %d, %s ) \n",
			// 	GetDODWpnData().szClassName,
			// 	m_iWorldModelIndex.Get(), m_iReloadModelIndex.Get(), m_bUseAltWeaponModel ? "alt" : "not alt" );

			return false;
		}

		return BaseClass::ShouldDraw();
	}

#else

	void CWeaponDODBase::AddViewmodelBob( CBaseViewModel *viewmodel, Vector &origin, QAngle &angles )
	{

	}

	float CWeaponDODBase::CalcViewmodelBob( void )
	{
		return 0.0f;
	}

	void CWeaponDODBase::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
	{
		if ( CanDrop() == false )
			return;

		CDODPlayer *pPlayer = ToDODPlayer( pActivator );

		if ( pPlayer )
		{
			pPlayer->PickUpWeapon( this );
		}         
	}

#endif

