//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: A stationary gun that players can man
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "tf_obj_base_manned_gun.h"
#include "tf_obj_manned_plasmagun_shared.h"
#include "in_buttons.h"
#include "tf_movedata.h"
#include "tf_gamerules.h"

#if defined( CLIENT_DLL )
#include "hudelement.h"
#include "bone_setup.h"
#include "hud_ammo.h"
#include "hud_crosshair.h"
#else
#endif

IMPLEMENT_NETWORKCLASS_ALIASED( ObjectBaseMannedGun, DT_ObjectBaseMannedGun )

BEGIN_NETWORK_TABLE( CObjectBaseMannedGun, DT_ObjectBaseMannedGun )
#if !defined( CLIENT_DLL )
	SendPropInt	(SENDINFO(m_nMoveStyle),	2, SPROP_UNSIGNED ),
	SendPropInt	(SENDINFO(m_nAmmoType),		8 ),
	SendPropInt	(SENDINFO(m_nAmmoCount),	6, SPROP_UNSIGNED ),
	SendPropAngle(SENDINFO(m_flGunYaw),		12 ),
	SendPropAngle(SENDINFO(m_flGunPitch),	12 ),
	SendPropAngle(SENDINFO(m_flBarrelPitch), 12 ),

	SendPropEHandle( SENDINFO( m_hLaserDesignation ) ),
	SendPropEHandle( SENDINFO( m_hBeam ) ),

#else
	RecvPropInt( RECVINFO(m_nMoveStyle) ),
	RecvPropInt( RECVINFO(m_nAmmoType) ),
	RecvPropInt( RECVINFO(m_nAmmoCount) ),
	RecvPropFloat( RECVINFO(m_flGunYaw) ),
	RecvPropFloat( RECVINFO(m_flGunPitch) ),
	RecvPropFloat( RECVINFO(m_flBarrelPitch) ),

	RecvPropEHandle( RECVINFO( m_hLaserDesignation ) ),
	RecvPropEHandle( RECVINFO( m_hBeam ) ),

#endif
END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( tf_obj_base_manned_gun, CObjectBaseMannedGun );

BEGIN_PREDICTION_DATA( CObjectBaseMannedGun  )

	DEFINE_PRED_FIELD( m_nMoveStyle, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_nAmmoType, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_nAmmoCount, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),

	DEFINE_PRED_FIELD_TOL( m_flGunYaw, FIELD_FLOAT, FTYPEDESC_INSENDTABLE, 0.5f),
	DEFINE_PRED_FIELD_TOL( m_flGunPitch, FIELD_FLOAT, FTYPEDESC_INSENDTABLE | FTYPEDESC_NOERRORCHECK, 0.125f ),
	DEFINE_PRED_FIELD_TOL( m_flBarrelPitch, FIELD_FLOAT, FTYPEDESC_INSENDTABLE | FTYPEDESC_NOERRORCHECK, 0.125f ),

	DEFINE_PRED_FIELD( m_hLaserDesignation, FIELD_EHANDLE, FTYPEDESC_INSENDTABLE ),

	DEFINE_PRED_FIELD( m_hBeam, FIELD_EHANDLE, FTYPEDESC_INSENDTABLE ),

	DEFINE_FIELD( m_flBarrelHeight, FIELD_FLOAT ),

//	DEFINE_FIELD( m_nBarrelAttachment, FIELD_INTEGER ),
//	DEFINE_FIELD( m_nBarrelPivotAttachment, FIELD_INTEGER ),
//	DEFINE_FIELD( m_nStandAttachment, FIELD_INTEGER ),
//	DEFINE_FIELD( m_nEyesAttachment, FIELD_INTEGER ),

END_PREDICTION_DATA()

extern ConVar mannedgun_usethirdperson;
static ConVar obj_manned_gun_designator_range( "obj_manned_gun_designator_range","2048", FCVAR_REPLICATED, "Manned gun's laser designation range" );
ConVar obj_child_range_factor( "obj_child_range_factor","1.1", FCVAR_REPLICATED, "Factor applied to range of objects that are built on a buildpoint" );

// Restoring initial state handling
#define OBJ_BASE_MANNEDGUN_THINK_CONTEXT		"BaseMannedGunThink"
#define MANNEDGUN_RESTORE_TIME					5.0
#define MANNEDGUN_RESTORE_TURN_RATE				150

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CObjectBaseMannedGun::CObjectBaseMannedGun()
{
	m_nMoveStyle = MOVEMENT_STYLE_STANDARD;
}


//-----------------------------------------------------------------------------
// Sets the movement style
//-----------------------------------------------------------------------------
void CObjectBaseMannedGun::SetMovementStyle( MovementStyle_t style )
{
	m_nMoveStyle = style;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectBaseMannedGun::Precache()
{
	BaseClass::Precache();
#if !defined( CLIENT_DLL )
	PrecacheVGuiScreen( "screen_obj_manned_plasmagun" );
	PrecacheMaterial( "sprites/laserbeam" );
#endif
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectBaseMannedGun::Spawn()
{
	m_takedamage = DAMAGE_YES;

	SetMaxPassengerCount( 1 );

	m_flGunYaw = 0;
	m_flGunPitch = 0;
	m_flBarrelPitch = 0;

	BaseClass::Spawn();

	// Manned guns don't need to be built like other vehicles
	int curFlags = GetObjectFlags();
	curFlags &= ~OF_MUST_BE_BUILT_IN_CONSTRUCTION_YARD;
	curFlags &= ~OF_MUST_BE_BUILT_ON_ATTACHMENT;
	curFlags &= ~OF_DOESNT_NEED_POWER;
	curFlags |= OF_DONT_PREVENT_BUILD_NEAR_OBJ;
	SetObjectFlags( curFlags );

	m_flMaxRange = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Calculate the max range of this gun
//-----------------------------------------------------------------------------
void CObjectBaseMannedGun::CalculateMaxRange( float flDefensiveRange, float flOffensiveRange )
{
	if ( GetTeamNumber() == TEAM_HUMANS )
	{
		m_flMaxRange = flDefensiveRange;
		if ( GetParentObject() )
		{
			m_flMaxRange *= obj_child_range_factor.GetFloat();
		}
	}
	else
	{
		m_flMaxRange = flOffensiveRange;
	}
}

//-----------------------------------------------------------------------------
// Sets up various attachment points once the model is selected 
//-----------------------------------------------------------------------------
void CObjectBaseMannedGun::OnModelSelected()
{
	m_nBarrelAttachment = LookupAttachment( "barrel" );
	m_nBarrelPivotAttachment = LookupAttachment( "barrelpivot" );
	m_nStandAttachment = LookupAttachment( "vehicle_feet_passenger0" );
	m_nEyesAttachment = LookupAttachment( "vehicle_eyes_passenger0" );

	// Find the barrel height in its quiescent state...
	Vector vBarrel;
	QAngle vBarrelAngles;
	GetAttachmentLocal( m_nBarrelAttachment, vBarrel, vBarrelAngles );
	m_flBarrelHeight = vBarrel.z;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectBaseMannedGun::UpdateOnRemove( void )
{
	if ( m_hLaserDesignation.Get() )
	{
		m_hLaserDesignation->Remove( );
		m_hLaserDesignation = NULL;
	}

	// Chain at end to mimic destructor unwind order
	BaseClass::UpdateOnRemove();
}


//-----------------------------------------------------------------------------
// Purpose: Gets info about the control panels
//-----------------------------------------------------------------------------
void CObjectBaseMannedGun::GetControlPanelInfo( int nPanelIndex, const char *&pPanelName )
{
	pPanelName = "screen_obj_manned_plasmagun";
}


//-----------------------------------------------------------------------------
// Purpose: Hide the base of the gun if it's on an attachment
//-----------------------------------------------------------------------------
void CObjectBaseMannedGun::SetupAttachedVersion( void )
{
	BaseClass::SetupAttachedVersion();

	SetBodygroup( 1, true );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectBaseMannedGun::SetupUnattachedVersion( void )
{
	BaseClass::SetupUnattachedVersion();

	SetBodygroup( 1, false );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectBaseMannedGun::OnGoInactive( void )
{
	BaseClass::OnGoInactive();

	// If we've got a player in the gun, tell him he's got to get out
	if ( GetDriverPlayer() )
	{
		ClientPrint( GetDriverPlayer(), HUD_PRINTCENTER, "Lost power to the manned gun!" );
		GetDriverPlayer()->LeaveVehicle();
	}

#if 0
	if ( GetBuffStation() )
	{
		GetBuffStation()->DeBuffObject( this );
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Can we get into the vehicle?
//-----------------------------------------------------------------------------
bool CObjectBaseMannedGun::CanGetInVehicle( CBaseTFPlayer *pPlayer )
{
	if ( !IsPowered() )
	{
		ClientPrint( pPlayer, HUD_PRINTCENTER, "No power source for the manned gun!" );
		return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Returns the eye position 
//-----------------------------------------------------------------------------
void CObjectBaseMannedGun::GetVehicleViewPosition( int nRole, Vector *pOrigin, QAngle *pAngles, float *pFOV /*= NULL*/ )
{
	BaseClass::GetVehicleViewPosition( nRole, pOrigin, pAngles, pFov );
	return;
	Assert( nRole == VEHICLE_DRIVER );
	QAngle vPlayerFeetAngles;
	GetAttachment(m_nEyesAttachment, *pOrigin, vPlayerFeetAngles);
}

//-----------------------------------------------------------------------------
// Purpose: Return to our original facing after a while
//-----------------------------------------------------------------------------
void CObjectBaseMannedGun::BaseMannedGunThink( void )
{
	// If someone's got in the gun, stop moving
	if ( GetDriverPlayer() )
		return;

	// Otherwise, move back towards the initial state
	if ( m_flGunPitch )
	{
		float flPitch = anglemod( m_flGunPitch );
		if (( flPitch <= 180 ) && ( flPitch >= 0 ))
		{
			m_flGunPitch = MAX( 0, flPitch - (gpGlobals->frametime * MANNEDGUN_RESTORE_TURN_RATE) );
		}
		else
		{
			m_flGunPitch = flPitch + (gpGlobals->frametime * MANNEDGUN_RESTORE_TURN_RATE);
			if ( m_flGunPitch >= 360 )
			{
				m_flGunPitch = 0;
			}
		}
	}
	else if ( m_flGunYaw )
	{
		if ( m_flGunYaw > 180 )
		{
			m_flGunYaw = m_flGunYaw + (gpGlobals->frametime * MANNEDGUN_RESTORE_TURN_RATE);
			if ( m_flGunYaw >= 360 )
			{
				m_flGunYaw = 0;
			}
		}
		else
		{
			m_flGunYaw = MAX( 0, m_flGunYaw - (gpGlobals->frametime * MANNEDGUN_RESTORE_TURN_RATE) );
		}

	}
	else
	{
		// We're done
		return;
	}

	// Keep thinking
	SetContextThink( BaseMannedGunThink, gpGlobals->curtime + 0.1, OBJ_BASE_MANNEDGUN_THINK_CONTEXT );
}

#ifndef CLIENT_DLL
//-----------------------------------------------------------------------------
// Purpose: Get and set the current driver.
//-----------------------------------------------------------------------------
void CObjectBaseMannedGun::SetPassenger( int nRole, CBasePlayer *pEnt )
{
	BaseClass::SetPassenger( nRole, pEnt );

	// If we don't have a driver anymore, return to our original facing after a while
	if ( !GetDriverPlayer() && (m_flGunPitch || m_flGunYaw) )
	{
		StopDesignating();
		SetContextThink( BaseMannedGunThink, gpGlobals->curtime + MANNEDGUN_RESTORE_TIME, OBJ_BASE_MANNEDGUN_THINK_CONTEXT );
	}
}
#endif

//-----------------------------------------------------------------------------
// Here's where we deal with weapons
//-----------------------------------------------------------------------------
void CObjectBaseMannedGun::OnItemPostFrame( CBaseTFPlayer *pDriver )
{
	// I can't do anything if I'm not active
	if ( !ShouldBeActive() )
		return;

	if ( !IsReadyToDrive() )
		return;

	// If we don't have a laser designator yet, create one
	if ( !m_hLaserDesignation )
	{
		m_hLaserDesignation = CEnvLaserDesignation::CreatePredicted( pDriver ); 
	}

	// Designating?
	if (pDriver->m_nButtons & IN_ATTACK2)
	{
		UpdateDesignator();
		return;
	}

	StopDesignating();

	// Fire our base weapon?
	if ( pDriver->m_nButtons & IN_ATTACK )
	{
		Fire();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectBaseMannedGun::StopDesignating( void )
{
	// Remove our beam if we just stopped designating
	if ( m_hBeam.Get() )
	{
		m_hBeam->Remove(  );
	}

	if ( m_hLaserDesignation.Get() )
	{
		m_hLaserDesignation->SetActive( false );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Update the designator position
//-----------------------------------------------------------------------------
void CObjectBaseMannedGun::UpdateDesignator( void )
{
	// Make the beam, if we don't have one yet
	if ( !m_hBeam && GetDriverPlayer() )
	{
		m_hBeam = BEAM_CREATE_PREDICTABLE_PERSIST( "sprites/laserbeam.vmt", 5, GetDriverPlayer() );
		if ( m_hBeam.Get() )
		{
			m_hBeam->PointEntInit( vec3_origin, this );
			m_hBeam->SetEndAttachment( m_nBarrelAttachment );
			m_hBeam->SetColor( 255, 32, 32 );
			m_hBeam->SetBrightness( 255 );
			m_hBeam->SetNoise( 0 );
			m_hBeam->SetWidth( 0.5 );
			m_hBeam->SetEndWidth( 0.5 );
		}
	}

	// We have to flush the bone cache because it's possible that only the bone controllers
	// have changed since the bonecache was generated, and bone controllers aren't checked.
	InvalidateBoneCache();

	QAngle vecAng;
	Vector vecSrc, vecAim;
	GetAttachment( m_nBarrelAttachment, vecSrc, vecAng );
	AngleVectors( vecAng, &vecAim, 0, 0 );

	// "Fire" the designator beam
	Vector vecEnd = vecSrc + vecAim * obj_manned_gun_designator_range.GetFloat();
	trace_t tr;
	TFGameRules()->WeaponTraceLine(vecSrc, vecEnd, MASK_SHOT, this, DMG_PROBE, &tr);

	if ( m_hLaserDesignation.Get() )
	{
		// Only update our designated target point if we hit something
		if ( tr.fraction != 1.0 )
		{
			m_hLaserDesignation->SetActive( true );
			m_hLaserDesignation->SetAbsOrigin( tr.endpos );
		}
		else
		{
			m_hLaserDesignation->SetActive( false );
		}
	}

	// Update beam visual
	if ( m_hBeam.Get() )
	{
		m_hBeam->SetStartPos( tr.endpos );
		m_hBeam->RelinkBeam();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectBaseMannedGun::SetupMove( CBasePlayer *pPlayer, CUserCmd *ucmd, IMoveHelper *pHelper, CMoveData *move )
{
	BaseClass::SetupMove( pPlayer, ucmd, pHelper, move );

	CTFMoveData *pMoveData = (CTFMoveData*)move; 
	Assert( sizeof(MannedPlasmagunData_t) <= pMoveData->VehicleDataMaxSize() );

	MannedPlasmagunData_t *pVehicleData = (MannedPlasmagunData_t*)pMoveData->VehicleData();
	pVehicleData->m_pVehicle = this;
	pVehicleData->m_flGunYaw = m_flGunYaw;
	pVehicleData->m_flGunPitch = m_flGunPitch;
	pVehicleData->m_flBarrelPitch = m_flBarrelPitch;
	pVehicleData->m_nMoveStyle = m_nMoveStyle;
	pVehicleData->m_flBarrelHeight = m_flBarrelHeight;
	pVehicleData->m_nBarrelPivotAttachment = m_nBarrelPivotAttachment;
	pVehicleData->m_nBarrelAttachment = m_nBarrelAttachment;
	pVehicleData->m_nStandAttachment = m_nStandAttachment;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectBaseMannedGun::FinishMove( CBasePlayer *player, CUserCmd *ucmd, CMoveData *move )
{
	BaseClass::FinishMove( player, ucmd, move );
	CTFMoveData *pMoveData = (CTFMoveData*)move; 
	Assert( sizeof(MannedPlasmagunData_t) <= pMoveData->VehicleDataMaxSize() );

	MannedPlasmagunData_t *pVehicleData = (MannedPlasmagunData_t*)pMoveData->VehicleData();
	m_flGunYaw = pVehicleData->m_flGunYaw;
	m_flGunPitch = pVehicleData->m_flGunPitch;
	m_flBarrelPitch = pVehicleData->m_flBarrelPitch;

	// Set the bone state..
	SetBoneController( 0, m_flGunYaw );
	SetBoneController( 1, m_flGunPitch );

	if ( m_nMoveStyle == MOVEMENT_STYLE_BARREL_PIVOT )
	{
		SetBoneController( 2, m_flBarrelPitch );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectBaseMannedGun::ProcessMovement( CBasePlayer *pPlayer, CMoveData *pMove )
{
	m_Movement.ProcessMovement( pPlayer, pMove );

	m_flGunPitch = AngleNormalize( m_flGunPitch );
	m_flBarrelPitch = AngleNormalize( m_flBarrelPitch );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CObjectBaseMannedGun::GetGunYaw() const
{
	return m_flGunYaw;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CObjectBaseMannedGun::GetGunPitch() const
{
	return m_flGunPitch;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CObjectBaseMannedGun::ShouldUseThirdPersonVehicleView( void )
{
	if ( mannedgun_usethirdperson.GetInt() )
	{
		// We want to use third person if we're mounted on a vehicle.
		return dynamic_cast< CBaseTFVehicle* >( GetMoveParent() ) != NULL;
	}
	else
	{
		return false;
	}
}

#if defined( CLIENT_DLL )
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_ObjectBaseMannedGun::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged(updateType);

	if ( updateType == DATA_UPDATE_CREATED )
	{
		// FIXME: Will this work with build animations models?

		m_nBarrelAttachment = LookupAttachment( "barrel" );
		m_nBarrelPivotAttachment = LookupAttachment( "barrelpivot" );
		m_nStandAttachment = LookupAttachment( "vehicle_feet_passenger0" );

		// Find the barrel height in its quiescent state...
		Vector vBarrel;
		QAngle vBarrelAngles;
		GetAttachmentLocal(m_nBarrelAttachment, vBarrel, vBarrelAngles);
		m_flBarrelHeight = vBarrel.z;

		// HACK HACK:  This should be read from a .txt file at some point!!!!
		CHudTexture newTexture;
		Q_strncpy( newTexture.szTextureFile, "sprites/crosshairs", sizeof( newTexture.szTextureFile ) );

		newTexture.rc.left		= 0;
		newTexture.rc.top		= 48;
		newTexture.rc.right		= newTexture.rc.left + 24;
		newTexture.rc.bottom	= newTexture.rc.top + 24;
		iconCrosshair = gHUD.AddUnsearchableHudIconToList( newTexture );
	}
	else
	{
		// Set the bone state..
		SetBoneController( 0, m_flGunYaw );
		SetBoneController( 1, m_flGunPitch );

		if ( m_nMoveStyle == MOVEMENT_STYLE_BARREL_PIVOT )
		{
			SetBoneController( 2, m_flBarrelPitch );
		}
	}
}

//-----------------------------------------------------------------------------
// Clamps the view angles while manning the gun 
//-----------------------------------------------------------------------------
void C_ObjectBaseMannedGun::UpdateViewAngles( C_BasePlayer *pLocalPlayer, CUserCmd *pCmd )
{
#if 0
	// Confine the view to the appropriate yaw range...
	float flAngleDiff = AngleDiff( pCmd->viewangles[YAW], flCenterYaw );

	// Here, we must clamp to the cone...
	if (flAngleDiff < m_Movement.GetMinYaw())
		pCmd->viewangles[YAW] = anglemod(flCenterYaw + m_Movement.GetMinYaw());
	else if (flAngleDiff > m_Movement.GetMaxYaw())
		pCmd->viewangles[YAW] = anglemod(flCenterYaw + m_Movement.GetMaxYaw());
#endif

	// Prevent too much downward looking
	if ( pCmd->viewangles[PITCH] > m_Movement.GetMaxPitch())
	{
		pCmd->viewangles[PITCH] = m_Movement.GetMaxPitch();
	}
}

//-----------------------------------------------------------------------------
// Orients the gun correctly 
//-----------------------------------------------------------------------------
void C_ObjectBaseMannedGun::GetBoneControllers(float controllers[MAXSTUDIOBONECTRLS], float dadt)
{
	// turret angle values:
	// 0 = front, 90 = left, 180 = back, 270 = right
	studiohdr_t *pModel = modelinfo->GetStudiomodel( GetModel() );
	Studio_SetController(pModel, 0, m_flGunYaw,   controllers[0]);
	Studio_SetController(pModel, 1, m_flGunPitch, controllers[1]);

	if ( m_nMoveStyle == MOVEMENT_STYLE_BARREL_PIVOT )
	{
		Studio_SetController(pModel, 2, m_flBarrelPitch, controllers[2]);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Get the angles that a player in the specified role should be using for visuals
//-----------------------------------------------------------------------------
QAngle C_ObjectBaseMannedGun::GetPassengerAngles( QAngle angCurrent, int nRole )
{
	// Stomp the current angle's pitch with our rotation
	QAngle vecNewAngles = angCurrent;
	angCurrent[PITCH] = m_flGunPitch;

	return angCurrent;
}

//-----------------------------------------------------------------------------
// Renders hud elements
//-----------------------------------------------------------------------------
void C_ObjectBaseMannedGun::DrawHudElements( void )
{
	GetHudAmmo()->SetPrimaryAmmo( m_nAmmoType, m_nAmmoCount );
	GetHudAmmo()->SetSecondaryAmmo( -1, -1 );

	// Let the plasma gun operator see a crosshair
	DrawCrosshair();
}


//-----------------------------------------------------------------------------
// Purpose: Draw the weapon's crosshair
//-----------------------------------------------------------------------------
void C_ObjectBaseMannedGun::DrawCrosshair()
{
	C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();
	if ( !player )
		return;

	CHudCrosshair *crosshair = GET_HUDELEMENT( CHudCrosshair );
	if ( !crosshair )
		return;
	
	if ( iconCrosshair )
	{
		crosshair->SetCrosshair( iconCrosshair, gHUD.m_clrNormal );
	}
	else
	{
		static wrect_t nullrc;
		crosshair->SetCrosshair( 0, Color( 255, 255, 255, 255 ) );
	}
}

#endif
