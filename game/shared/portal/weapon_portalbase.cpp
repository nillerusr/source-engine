//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "in_buttons.h"
#include "takedamageinfo.h"
#include "ammodef.h"
#include "portal_gamerules.h"


#ifdef CLIENT_DLL
extern IVModelInfoClient* modelinfo;
#else
extern IVModelInfo* modelinfo;
#endif


#if defined( CLIENT_DLL )

	#include "vgui/ISurface.h"
	#include "vgui_controls/Controls.h"
	#include "c_portal_player.h"
	#include "hud_crosshair.h"
	#include "PortalRender.h"

#else

	#include "portal_player.h"
	#include "vphysics/constraints.h"

#endif

#include "weapon_portalbase.h"


// ----------------------------------------------------------------------------- //
// Global functions.
// ----------------------------------------------------------------------------- //

bool IsAmmoType( int iAmmoType, const char *pAmmoName )
{
	return GetAmmoDef()->Index( pAmmoName ) == iAmmoType;
}

static const char * s_WeaponAliasInfo[] = 
{
	"none",	//	WEAPON_NONE = 0,

	//Melee
	"shotgun",	//WEAPON_AMERKNIFE,
	
	NULL,		// end of list marker
};


// ----------------------------------------------------------------------------- //
// CWeaponPortalBase tables.
// ----------------------------------------------------------------------------- //

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponPortalBase, DT_WeaponPortalBase )

BEGIN_NETWORK_TABLE( CWeaponPortalBase, DT_WeaponPortalBase )

#ifdef CLIENT_DLL
  
#else
	// world weapon models have no aminations
  //	SendPropExclude( "DT_AnimTimeMustBeFirst", "m_flAnimTime" ),
//	SendPropExclude( "DT_BaseAnimating", "m_nSequence" ),
//	SendPropExclude( "DT_LocalActiveWeaponData", "m_flTimeWeaponIdle" ),
#endif
	
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponPortalBase ) 
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_portal_base, CWeaponPortalBase );


#ifdef GAME_DLL

	BEGIN_DATADESC( CWeaponPortalBase )

	END_DATADESC()

#endif

// ----------------------------------------------------------------------------- //
// CWeaponPortalBase implementation. 
// ----------------------------------------------------------------------------- //
CWeaponPortalBase::CWeaponPortalBase()
{
	SetPredictionEligible( true );
	AddSolidFlags( FSOLID_TRIGGER ); // Nothing collides with these but it gets touches.

	m_flNextResetCheckTime = 0.0f;
}


bool CWeaponPortalBase::IsPredicted() const
{ 
	return false;
}

void CWeaponPortalBase::WeaponSound( WeaponSound_t sound_type, float soundtime /* = 0.0f */ )
{
#ifdef CLIENT_DLL

		// If we have some sounds from the weapon classname.txt file, play a random one of them
		const char *shootsound = GetWpnData().aShootSounds[ sound_type ]; 
		if ( !shootsound || !shootsound[0] )
			return;

		CBroadcastRecipientFilter filter; // this is client side only
		if ( !te->CanPredict() )
			return;
				
		CBaseEntity::EmitSound( filter, GetPlayerOwner()->entindex(), shootsound, &GetPlayerOwner()->GetAbsOrigin() ); 
#else
		BaseClass::WeaponSound( sound_type, soundtime );
#endif
}


CBasePlayer* CWeaponPortalBase::GetPlayerOwner() const
{
	return dynamic_cast< CBasePlayer* >( GetOwner() );
}

CPortal_Player* CWeaponPortalBase::GetPortalPlayerOwner() const
{
	return dynamic_cast< CPortal_Player* >( GetOwner() );
}

#ifdef CLIENT_DLL
	
void CWeaponPortalBase::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );

	if ( GetPredictable() && !ShouldPredict() )
		ShutdownPredictable();
}

int CWeaponPortalBase::DrawModel( int flags )
{
	if ( !m_bReadyToDraw )
		return 0;

	if ( GetOwner() && (GetOwner() == C_BasePlayer::GetLocalPlayer()) && !g_pPortalRender->IsRenderingPortal() && !C_BasePlayer::ShouldDrawLocalPlayer() )
		return 0;

	//Sometimes the return value of ShouldDrawLocalPlayer() fluctuates too often to draw the correct model all the time, so this is a quick fix if it's changed too fast
	int iOriginalIndex = GetModelIndex();
	bool bChangeModelBack = false;

	int iWorldModelIndex = GetWorldModelIndex();
	if( iOriginalIndex != iWorldModelIndex )
	{
		SetModelIndex( iWorldModelIndex );
		bChangeModelBack = true;
	}

	int iRetVal = BaseClass::DrawModel( flags );

	if( bChangeModelBack )
		SetModelIndex( iOriginalIndex );

	return iRetVal;
}

bool CWeaponPortalBase::ShouldDraw( void )
{
	if ( !GetOwner() || GetOwner() != C_BasePlayer::GetLocalPlayer() )
		return true;

	if ( !IsActiveByLocalPlayer() )
		return false;

	//if ( GetOwner() && GetOwner() == C_BasePlayer::GetLocalPlayer() && materials->GetRenderTarget() == 0 )
	//	return false;

	return true;
}

bool CWeaponPortalBase::ShouldPredict()
{
	if ( GetOwner() && GetOwner() == C_BasePlayer::GetLocalPlayer() )
		return true;

	return BaseClass::ShouldPredict();
}

//-----------------------------------------------------------------------------
// Purpose: Draw the weapon's crosshair
//-----------------------------------------------------------------------------
void CWeaponPortalBase::DrawCrosshair()
{
	C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();
	if ( !player )
		return;

	Color clr = gHUD.m_clrNormal;

	CHudCrosshair *crosshair = GET_HUDELEMENT( CHudCrosshair );
	if ( !crosshair )
		return;

	// Check to see if the player is in VGUI mode...
	if (player->IsInVGuiInputMode())
	{
		CHudTexture *pArrow	= gHUD.GetIcon( "arrow" );

		crosshair->SetCrosshair( pArrow, gHUD.m_clrNormal );
		return;
	}

	// Find out if this weapon's auto-aimed onto a target
	bool bOnTarget = ( m_iState == WEAPON_IS_ONTARGET );

	if ( player->GetFOV() >= 90 )
	{ 
		// normal crosshairs
		if ( bOnTarget && GetWpnData().iconAutoaim )
		{
			clr[3] = 255;

			crosshair->SetCrosshair( GetWpnData().iconAutoaim, clr );
		}
		else if ( GetWpnData().iconCrosshair )
		{
			clr[3] = 255;
			crosshair->SetCrosshair( GetWpnData().iconCrosshair, clr );
		}
		else
		{
			crosshair->ResetCrosshair();
		}
	}
	else
	{ 
		Color white( 255, 255, 255, 255 );

		// zoomed crosshairs
		if (bOnTarget && GetWpnData().iconZoomedAutoaim)
			crosshair->SetCrosshair(GetWpnData().iconZoomedAutoaim, white);
		else if ( GetWpnData().iconZoomedCrosshair )
			crosshair->SetCrosshair( GetWpnData().iconZoomedCrosshair, white );
		else
			crosshair->ResetCrosshair();
	}
}

void CWeaponPortalBase::DoAnimationEvents( CStudioHdr *pStudioHdr )
{
	// HACK: Because this model renders view and world models in the same frame 
	// it's using the wrong studio model when checking the sequences.
	C_BasePlayer *pPlayer = UTIL_PlayerByIndex( 1 );
	if ( pPlayer && pPlayer->GetActiveWeapon() == this )
	{
		C_BaseViewModel *pViewModel = pPlayer->GetViewModel();
		if ( pViewModel )
		{
			pStudioHdr = pViewModel->GetModelPtr();
		}
	}

	if ( pStudioHdr )
	{
		BaseClass::DoAnimationEvents( pStudioHdr );
	}
}

void CWeaponPortalBase::GetRenderBounds( Vector& theMins, Vector& theMaxs )
{
	if ( IsRagdoll() )
	{
		m_pRagdoll->GetRagdollBounds( theMins, theMaxs );
	}
	else if ( GetModel() )
	{
		CStudioHdr *pStudioHdr = NULL;

		// HACK: Because this model renders view and world models in the same frame 
		// it's using the wrong studio model when checking the sequences.
		C_BasePlayer *pPlayer = UTIL_PlayerByIndex( 1 );
		if ( pPlayer && pPlayer->GetActiveWeapon() == this )
		{
			C_BaseViewModel *pViewModel = pPlayer->GetViewModel();
			if ( pViewModel )
			{
				pStudioHdr = pViewModel->GetModelPtr();
			}
		}
		else
		{
			pStudioHdr = GetModelPtr();
		}

		if ( !pStudioHdr || !pStudioHdr->SequencesAvailable() || GetSequence() == -1 )
		{
			theMins = vec3_origin;
			theMaxs = vec3_origin;
			return;
		} 
		if (!VectorCompare( vec3_origin, pStudioHdr->view_bbmin() ) || !VectorCompare( vec3_origin, pStudioHdr->view_bbmax() ))
		{
			// clipping bounding box
			VectorCopy ( pStudioHdr->view_bbmin(), theMins);
			VectorCopy ( pStudioHdr->view_bbmax(), theMaxs);
		}
		else
		{
			// movement bounding box
			VectorCopy ( pStudioHdr->hull_min(), theMins);
			VectorCopy ( pStudioHdr->hull_max(), theMaxs);
		}

		mstudioseqdesc_t &seqdesc = pStudioHdr->pSeqdesc( GetSequence() );
		VectorMin( seqdesc.bbmin, theMins, theMins );
		VectorMax( seqdesc.bbmax, theMaxs, theMaxs );
	}
	else
	{
		theMins = vec3_origin;
		theMaxs = vec3_origin;
	}
}


#else
	
void CWeaponPortalBase::Spawn()
{
	BaseClass::Spawn();

	// Set this here to allow players to shoot dropped weapons
	SetCollisionGroup( COLLISION_GROUP_WEAPON );

	// Use less bloat for the collision box for this weapon. (bug 43800)
	CollisionProp()->UseTriggerBounds( true, 20 );
}

void CWeaponPortalBase::	Materialize( void )
{
	if ( IsEffectActive( EF_NODRAW ) )
	{
		// changing from invisible state to visible.
		EmitSound( "AlyxEmp.Charge" );
		
		RemoveEffects( EF_NODRAW );
		DoMuzzleFlash();
	}

	if ( HasSpawnFlags( SF_NORESPAWN ) == false )
	{
		VPhysicsInitNormal( SOLID_BBOX, GetSolidFlags() | FSOLID_TRIGGER, false );
		SetMoveType( MOVETYPE_VPHYSICS );

		//PortalRules()->AddLevelDesignerPlacedObject( this );
	}

	if ( HasSpawnFlags( SF_NORESPAWN ) == false )
	{
		if ( GetOriginalSpawnOrigin() == vec3_origin )
		{
			m_vOriginalSpawnOrigin = GetAbsOrigin();
			m_vOriginalSpawnAngles = GetAbsAngles();
		}
	}

	SetPickupTouch();

	SetThink (NULL);
}

#endif

const CPortalSWeaponInfo &CWeaponPortalBase::GetPortalWpnData() const
{
	const FileWeaponInfo_t *pWeaponInfo = &GetWpnData();
	const CPortalSWeaponInfo *pPortalInfo;

	#ifdef _DEBUG
		pPortalInfo = dynamic_cast< const CPortalSWeaponInfo* >( pWeaponInfo );
		Assert( pPortalInfo );
	#else
		pPortalInfo = static_cast< const CPortalSWeaponInfo* >( pWeaponInfo );
	#endif

	return *pPortalInfo;
}
void CWeaponPortalBase::FireBullets( const FireBulletsInfo_t &info )
{
	FireBulletsInfo_t modinfo = info;

	modinfo.m_iPlayerDamage = GetPortalWpnData().m_iPlayerDamage;

	BaseClass::FireBullets( modinfo );
}


#if defined( CLIENT_DLL )

#include "c_te_effect_dispatch.h"

#define NUM_MUZZLE_FLASH_TYPES 4

bool CWeaponPortalBase::OnFireEvent( C_BaseViewModel *pViewModel, const Vector& origin, const QAngle& angles, int event, const char *options )
{
	return BaseClass::OnFireEvent( pViewModel, origin, angles, event, options );
}


void UTIL_ClipPunchAngleOffset( QAngle &in, const QAngle &punch, const QAngle &clip )
{
	QAngle	final = in + punch;

	//Clip each component
	for ( int i = 0; i < 3; i++ )
	{
		if ( final[i] > clip[i] )
		{
			final[i] = clip[i];
		}
		else if ( final[i] < -clip[i] )
		{
			final[i] = -clip[i];
		}

		//Return the result
		in[i] = final[i] - punch[i];
	}
}

#endif

