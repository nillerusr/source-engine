//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: A base vehicle class
//
//=============================================================================//

#include "cbase.h"
#include "basetfvehicle.h"
#include "tf_movedata.h"
#include "in_buttons.h"
#include "baseplayer_shared.h"

#if defined( CLIENT_DLL )
	#include "hud_vehicle_role.h"
	#include "hud.h"
	#include "hud_crosshair.h"
#else
	#include "tf_team.h"
	#include "tf_gamerules.h"
	#include "tf_func_construction_yard.h"
	#include "ndebugoverlay.h"
#endif

IMPLEMENT_NETWORKCLASS_ALIASED( BaseTFVehicle, DT_BaseTFVehicle );

BEGIN_NETWORK_TABLE( CBaseTFVehicle, DT_BaseTFVehicle )
#if !defined( CLIENT_DLL )
	SendPropInt( SENDINFO(m_nMaxPassengers), CBaseTFVehicle::MAX_PASSENGER_BITS, SPROP_UNSIGNED ),
	SendPropArray( SendPropEHandle(SENDINFO_ARRAY(m_hPassengers)), m_hPassengers ),
	SendPropEHandle( SENDINFO(m_hDriverGun) ),
#else
	RecvPropInt( RECVINFO(m_nMaxPassengers) ),
	RecvPropArray( RecvPropEHandle(RECVINFO(m_hPassengers[0])), m_hPassengers ),
	RecvPropEHandle( RECVINFO(m_hDriverGun) ),
#endif
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CBaseTFVehicle )

	DEFINE_PRED_ARRAY( m_hPassengers, FIELD_EHANDLE, CBaseTFVehicle::MAX_PASSENGERS, FTYPEDESC_INSENDTABLE ),

END_PREDICTION_DATA()

extern float RemapAngleRange( float startInterval, float endInterval, float value );
extern ConVar road_feel;

ConVar vehicle_view_offset_forward( "vehicle_view_offset_forward", "-280", FCVAR_REPLICATED );
ConVar vehicle_view_offset_right( "vehicle_view_offset_right", "0", FCVAR_REPLICATED );
ConVar vehicle_view_offset_up( "vehicle_view_offset_up", "50", FCVAR_REPLICATED );
ConVar vehicle_thirdperson( "vehicle_thirdperson", "1", FCVAR_REPLICATED, "Enable/disable thirdperson camera view in vehicles" );

ConVar vehicle_attach_eye_angles( "vehicle_attach_eye_angles", "0", FCVAR_REPLICATED, "Attach player eye angles to vehicle attachments" );

#define PITCH_CURVE_ZERO		10	// pitch less than this is clamped to zero
#define PITCH_CURVE_LINEAR		45	// pitch greater than this is copied out

#define ROLL_CURVE_ZERO		5		// roll less than this is clamped to zero
#define ROLL_CURVE_LINEAR	45		// roll greater than this is copied out

#if defined( CLIENT_DLL )
	ConVar road_feel( "road_feel", "0.1", FCVAR_NOTIFY | FCVAR_REPLICATED );
#else
	// Deterioration
	#define DETERIORATION_THINK_CONTEXT			"VehicleDeteriorationThink"
	#define PASSENGER_THINK_CONTEXT				"VehiclePassengerThink"
	ConVar  vehicle_deterioration_start_time( "vehicle_deterioration_start_time", "90", 0, "Time it takes for a vehicle to start deteriorating after being left alone." );

	#define DETERIORATION_DISTANCE				(600 * 600)		// Never deteriorate if team mates are within this distance

#endif // CLIENT_DLL

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseTFVehicle::CBaseTFVehicle()
{
	SetPredictionEligible( true );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTFVehicle::Spawn()
{
	BaseClass::Spawn();
	CollisionProp()->SetSurroundingBoundsType( USE_OBB_COLLISION_BOUNDS );

#if defined( CLIENT_DLL )
	SetNextClientThink( CLIENT_THINK_ALWAYS );
	m_pIconDefaultCrosshair = NULL;
#else
	m_fObjectFlags |= OF_DOESNT_NEED_POWER | OF_MUST_BE_BUILT_ON_ATTACHMENT;
	SetContextThink( VehiclePassengerThink, 2.0, PASSENGER_THINK_CONTEXT );
#endif
}


//-----------------------------------------------------------------------------
// Vehicle overrides
//-----------------------------------------------------------------------------
CBaseEntity* CBaseTFVehicle::GetVehicleEnt()
{
	return this;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTFVehicle::SetupMove( CBasePlayer *player, CUserCmd *ucmd, IMoveHelper *pHelper, CMoveData *move )
{
	// animate + update attachment points
#ifdef CLIENT_DLL
	StudioFrameAdvance();
#else
	StudioFrameAdvance();
	// This calls StudioFrameAdvance, then we use the results from that to determine where to move.
	DispatchAnimEvents( this );
#endif
	
	CTFMoveData *pMoveData = (CTFMoveData*)move; 
	Assert( sizeof(VehicleBaseMoveData_t) <= pMoveData->VehicleDataMaxSize() );

	VehicleBaseMoveData_t *pVehicleData = (VehicleBaseMoveData_t*)pMoveData->VehicleData();
	pVehicleData->m_pVehicle = this;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTFVehicle::FinishMove( CBasePlayer *player, CUserCmd *ucmd, CMoveData *move )
{
	VehicleDriverGunThink();
}

//-----------------------------------------------------------------------------
// Purpose: Returns the driver as a tfplayer pointer
//-----------------------------------------------------------------------------
CBaseTFPlayer *CBaseTFVehicle::GetDriverPlayer()
{
	return m_hPassengers[VEHICLE_DRIVER].Get();
}

//-----------------------------------------------------------------------------
// Purpose: Can we get into the vehicle?
//-----------------------------------------------------------------------------
bool CBaseTFVehicle::CanGetInVehicle( CBaseTFPlayer *pPlayer )
{
	if ( !IsPowered() )
		return false;

	if ( !InSameTeam( pPlayer ) )
		return false;

	// Player/Class-specific query.
	return pPlayer->CanGetInVehicle();
}

//-----------------------------------------------------------------------------
// Purpose: Here's where we deal with weapons
//-----------------------------------------------------------------------------
void CBaseTFVehicle::OnItemPostFrame( CBaseTFPlayer *pDriver )
{
	// If we have a gun for the driver, handle it
	if ( m_hDriverGun )
	{
		if ( GetPassengerRole(pDriver) != VEHICLE_DRIVER )
			return;

		if ( pDriver->m_nButtons & (IN_ATTACK | IN_ATTACK2) )
		{
			// Time to fire?
			if ( m_hDriverGun->CanFireNow() )
			{
				m_hDriverGun->Fire( pDriver );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CBaseTFVehicle::GetPassengerRole( CBasePlayer *pEnt )
{
	Assert( pEnt->IsPlayer() );
	for ( int i = m_nMaxPassengers; --i >= 0; )
	{
		if (m_hPassengers[i] == pEnt)
		{
			return i;
		}
	}
	return -1;
}

Vector CBaseTFVehicle::GetSoundEmissionOrigin() const
{
	return WorldSpaceCenter() + Vector( 0, 0, 64 );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBasePlayer* CBaseTFVehicle::GetPassenger( int nRole )
{
	return m_hPassengers[nRole].Get();
}


//-----------------------------------------------------------------------------
// Is a particular player in the vehicle?
//-----------------------------------------------------------------------------
bool CBaseTFVehicle::IsPlayerInVehicle( CBaseTFPlayer *pPlayer )
{
	return (GetPassengerRole( pPlayer ) >= 0);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CBaseTFVehicle::GetPassengerCount() const
{
	// FIXME: Cache this off!
	int nCount = 0;
	for (int i = m_nMaxPassengers; --i >= 0; )
	{
		if (m_hPassengers[i].Get())
		{
			++nCount;
		}
	}
	return nCount;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CBaseTFVehicle::GetMaxPassengerCount() const
{
	return m_nMaxPassengers;
}

//-----------------------------------------------------------------------------
// Process input
//-----------------------------------------------------------------------------
void CBaseTFVehicle::ItemPostFrame( CBasePlayer *pPassenger )
{
#ifndef CLIENT_DLL
	Assert( GetPassengerRole( pPassenger ) != -1 );
	if (pPassenger->m_afButtonPressed & (IN_USE /*| IN_JUMP*/)) 
	{
		// Get the player out..
		pPassenger->LeaveVehicle();
		return;
	}
#endif

	OnItemPostFrame( static_cast<CBaseTFPlayer*>(pPassenger) );
}

//-----------------------------------------------------------------------------
// Purpose: Reset the time before this vehicle begins to deteriorate
//-----------------------------------------------------------------------------
void CBaseTFVehicle::ResetDeteriorationTime( void )
{
#if !defined (CLIENT_DLL)
	SetContextThink( VehicleDeteriorationThink, gpGlobals->curtime + vehicle_deterioration_start_time.GetFloat(), DETERIORATION_THINK_CONTEXT );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Prevent driving in construction yards
//-----------------------------------------------------------------------------
bool CBaseTFVehicle::IsReadyToDrive( void )
{
#if !defined( CLIENT_DLL )
	return ( PointInConstructionYard( GetAbsOrigin() ) == false );
#else
	return true;
#endif
}


//-----------------------------------------------------------------------------
// Process input
//-----------------------------------------------------------------------------
void CBaseTFVehicle::SetMaxPassengerCount( int nCount )
{
#if !defined( CLIENT_DLL )
	Assert( (nCount >= 1) && (nCount <= MAX_PASSENGERS) );
	m_nMaxPassengers = nCount;
#endif
}

//-----------------------------------------------------------------------------
//
// Server-only code here
//
//-----------------------------------------------------------------------------

#if !defined (CLIENT_DLL)
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTFVehicle::FinishedBuilding( void )
{
	BaseClass::FinishedBuilding();

	// See if we've finished building on a vehicle that has a passenger slot assigned to my buildpoint.
	CBaseObject	*pParent = GetParentObject();
	if ( pParent && pParent->IsAVehicle() )
	{
		CBaseTFVehicle *pVehicle = static_cast<CBaseTFVehicle*>(pParent);
		int iRole = pVehicle->GetChildVehicleRole( this );
		if ( iRole != -1 )
		{
			// Is there a player in the role assigned to this buildpoint?
			CBaseTFPlayer *pExistingPlayer = static_cast<CBaseTFPlayer*>( pVehicle->GetPassenger( iRole ) );
			if ( pExistingPlayer )
			{
				// Remove the player from my parent vehicle and put them in me
				pExistingPlayer->LeaveVehicle();

				// Get in the vehicle.
				pExistingPlayer->GetInVehicle( this, VEHICLE_DRIVER );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTFVehicle::VehicleDeteriorationThink( void )
{
	StartDeteriorating();

	SetContextThink( NULL, gpGlobals->curtime + vehicle_deterioration_start_time.GetFloat(), DETERIORATION_THINK_CONTEXT );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTFVehicle::VehiclePassengerThink( void )
{
	SetNextThink( gpGlobals->curtime + 10.0, PASSENGER_THINK_CONTEXT );

	if ( IsPlacing() )
	{
		ResetDeteriorationTime();
		return;
	}

	// If there are any passengers in the vehicle, push off deterioration time
	if ( GetPassengerCount() )
	{
		ResetDeteriorationTime();
		return;
	}

	// See if there are any team members nearby
	if ( GetTeam() )
	{
		int iNumPlayers = GetTFTeam()->GetNumPlayers();
		for ( int i = 0; i < iNumPlayers; i++ )
		{
			if ( GetTFTeam()->GetPlayer(i) )
			{
				Vector vecOrigin = GetTFTeam()->GetPlayer(i)->GetAbsOrigin();
				if ( (vecOrigin - GetAbsOrigin()).LengthSqr() < DETERIORATION_DISTANCE )
				{
					// Found one nearby, reset our deterioration time
					ResetDeteriorationTime();
					return;
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Figure out which role of a vehicle a child vehicle is sitting in..
//-----------------------------------------------------------------------------
int CBaseTFVehicle::GetChildVehicleRole( CBaseTFVehicle *pChild )
{
	int nBuildPoints = GetNumBuildPoints();
	for( int i = 0; i < nBuildPoints; i++ )
	{	
		CBaseObject* pObject = GetBuildPointObject(i);
		if (pObject == pChild)
		{
			return GetBuildPointPassenger(i);
		}
	}

	return -1;
}


//-----------------------------------------------------------------------------
// Purpose: Vehicles are permanently oriented off angle for vphysics.
//-----------------------------------------------------------------------------
void CBaseTFVehicle::GetVectors(Vector* pForward, Vector* pRight, Vector* pUp) const
{
	// This call is necessary to cause m_rgflCoordinateFrame to be recomputed
	const matrix3x4_t &entityToWorld = EntityToWorldTransform();

	if (pForward != NULL)
	{
		MatrixGetColumn( entityToWorld, 1, *pForward ); 
	}

	if (pRight != NULL)
	{
		MatrixGetColumn( entityToWorld, 0, *pRight ); 
	}

	if (pUp != NULL)
	{
		MatrixGetColumn( entityToWorld, 2, *pUp ); 
	}
}

//-----------------------------------------------------------------------------
// Purpose: Get into the vehicle
//-----------------------------------------------------------------------------
void CBaseTFVehicle::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	BaseClass::Use( pActivator, pCaller, useType, value );

	if ( useType == USE_ON )
	{
		CBaseTFPlayer *pPlayer = dynamic_cast<CBaseTFPlayer*>(pActivator);
		if ( pPlayer && InSameTeam(pPlayer) )
		{
			// Check to see if we are really using nearby build points:
			if( !UseAttachedItem( pActivator, pCaller, useType, value ) )
			{
				// Attempt to board the vehicle:
				AttemptToBoardVehicle( pPlayer );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Figure out if I should be using an attached item rather than this vehicle itself.
//-----------------------------------------------------------------------------
bool CBaseTFVehicle::UseAttachedItem( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CBaseTFPlayer* pPlayer = dynamic_cast<CBaseTFPlayer*>(pActivator);
	if ( !pPlayer || !InSameTeam(pPlayer) )
		return false;

	Vector vecPlayerOrigin = pPlayer->GetAbsOrigin();
	int nBestBuildPoint = -1;
	float fBestDistance = FLT_MAX;

	// Get the closest regular entry point:
	int nRole = LocateEntryPoint( pPlayer, &fBestDistance );

	// Iterate through each of the build points, if any, and see which we are closest to.
	int nBuildPoints = GetNumBuildPoints();
	for( int i = 0; i < nBuildPoints; i++ )
	{	
		CBaseObject* pObject = GetBuildPointObject(i);

		// If there's something in the build point that isn't in the process of being built or placed:
		if( pObject && !pObject->IsPlacing() && !pObject->IsBuilding() ) 
		{
			Vector vecOrigin;
			QAngle vecAngles;

			// If the build point is the default point for this role, just take it 
			if (GetBuildPointPassenger(i) == nRole)
			{
				nBestBuildPoint = i;
				break;
			}

			// And I can get the build point.
			if( GetBuildPoint( i, vecOrigin, vecAngles )  )
			{
				float fLength2dSqr = (vecOrigin - vecPlayerOrigin).AsVector2D().LengthSqr();
				if( fLength2dSqr < fBestDistance )
				{
					nBestBuildPoint = i;
					fBestDistance = fLength2dSqr; 
				}
			}
		}
	}

	if( nBestBuildPoint >= 0 )
	{
		// They're using an item on me, so push out the deterioration time
		ResetDeteriorationTime();
		GetBuildPointObject(nBestBuildPoint)->Use( pActivator, pCaller, useType, value );
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Object has been removed...
//-----------------------------------------------------------------------------
void CBaseTFVehicle::DestroyObject( void )
{
	for (int i = m_nMaxPassengers; --i >= 0; )
	{
		if (m_hPassengers[i])
		{
			m_hPassengers[i]->LeaveVehicle();
		}
	}

	BaseClass::DestroyObject();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int CBaseTFVehicle::GetEmptyRole( void )
{
	for ( int iPassenger = 0; iPassenger < m_nMaxPassengers; ++iPassenger )
	{
		if ( !m_hPassengers[iPassenger].Get() )
			return iPassenger;
	}

	return -1;	
}


//-----------------------------------------------------------------------------
// Purpose: Try to board the vehicle
//-----------------------------------------------------------------------------
void CBaseTFVehicle::AttemptToBoardVehicle( CBaseTFPlayer *pPlayer )
{
	if ( !CanGetInVehicle( pPlayer ) )
		return;

	// Locate the entry point.
	int nRole = LocateEntryPoint( pPlayer );
	if ( nRole != -1 )
	{
		// Set the owner flag.
		bool bOwner = ( pPlayer == GetOwner() );
		if ( bOwner )
		{
			// Check to see if a player exists at this location (role).
			CBaseTFPlayer *pExistingPlayer = static_cast<CBaseTFPlayer*>( GetPassenger( nRole ) );
			if ( pExistingPlayer )
			{
				pExistingPlayer->LeaveVehicle();

				// Get in the vehicle.
				pPlayer->GetInVehicle( this, nRole );

				// Then see if we can move the other player to another slot in this vehicle
				int nEmptyRole = GetEmptyRole();
				if ( nEmptyRole != -1 )
				{
					pExistingPlayer->GetInVehicle( this, nEmptyRole );
				}
				return;
			}
		}

		// Get in the vehicle.
		pPlayer->GetInVehicle( this, nRole );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Handle commands sent from vgui panels on the client 
//-----------------------------------------------------------------------------
bool CBaseTFVehicle::ClientCommand( CBaseTFPlayer *pPlayer, const CCommand &args )
{
	ResetDeteriorationTime();

	if ( FStrEq( pCmd, "toggle_use" ) )
	{
		AttemptToBoardVehicle( pPlayer );
		return true;
	}

	return BaseClass::ClientCommand( pPlayer, args );
}

//-----------------------------------------------------------------------------
// Get a position in *world space* inside the vehicle for the player to exit at
// NOTE: This doesn't check for obstructions
//-----------------------------------------------------------------------------
void CBaseTFVehicle::GetInitialPassengerExitPoint( int nRole, Vector *pAbsPoint, QAngle *pAbsAngles )
{
	char pAttachmentName[32];
	Q_snprintf( pAttachmentName, sizeof( pAttachmentName ), "vehicle_exit_passenger%d", nRole );
	int exitAttachmentIndex = LookupAttachment(pAttachmentName);
	if (exitAttachmentIndex <= 0)
	{
		// bad attachment, just return the origin
		*pAbsPoint = GetAbsOrigin();
		pAbsPoint->z += WorldAlignMaxs()[2] + 50.0f;
		return;
	}

	QAngle vehicleExitAngles;
	if( !pAbsAngles )
	{
		pAbsAngles = &vehicleExitAngles;
	}

	GetAttachment( exitAttachmentIndex, *pAbsPoint, *pAbsAngles );
}


//-----------------------------------------------------------------------------
// Get a point to leave the vehicle from
//-----------------------------------------------------------------------------
bool CBaseTFVehicle::IsValidExitPoint( int nRole, Vector *pExitPoint, QAngle *pAngles )
{
	GetInitialPassengerExitPoint( nRole, pExitPoint, pAngles );

	// Check the exit point:
	Vector vecStart = *pExitPoint;
	Vector vecEnd = *pExitPoint + Vector(0,0,20);
	trace_t tr;
	UTIL_TraceLine( vecStart, vecEnd, MASK_SOLID, this, COLLISION_GROUP_NONE, &tr);
	if ( (tr.fraction < 1.f)  )
		return false;

	vecEnd = *pExitPoint + Vector(20,20,20);
	UTIL_TraceLine( vecStart, vecEnd, MASK_SOLID, this, COLLISION_GROUP_NONE, &tr);
	if ( (tr.fraction < 1.f)  )
		return false;

	vecEnd = *pExitPoint + Vector(-20,-20,20);
	UTIL_TraceLine( vecStart, vecEnd, MASK_SOLID, this, COLLISION_GROUP_NONE, &tr);
	if ( (tr.fraction < 1.f)  )
		return false;

	vecEnd = *pExitPoint + Vector(20,-20,20);
	UTIL_TraceLine( vecStart, vecEnd, MASK_SOLID, this, COLLISION_GROUP_NONE, &tr);
	if ( (tr.fraction < 1.f)  )
		return false;

	vecEnd = *pExitPoint + Vector(-20,20,20);
	UTIL_TraceLine( vecStart, vecEnd, MASK_SOLID, this, COLLISION_GROUP_NONE, &tr);
	if ( (tr.fraction < 1.f)  )
		return false;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTFVehicle::GetPassengerExitPoint( CBasePlayer *pPlayer, int nRole, Vector *pAbsPosition, QAngle *pAbsAngles )
{

	// Deal with vehicles built on other vehicles
	CBaseTFVehicle *pParentVehicle = dynamic_cast<CBaseTFVehicle*>(GetMoveParent());
	if (pParentVehicle)
	{
		int nParentVehicleRole = pParentVehicle->GetChildVehicleRole( this );
		if (nParentVehicleRole >= 0)
		{
			pParentVehicle->GetPassengerExitPoint( pPlayer, nParentVehicleRole, pAbsPosition, pAbsAngles );
			return;
		}
	}

	// Deal with vehicles build on objects
	IHasBuildPoints *pMount = dynamic_cast<IHasBuildPoints*>(GetMoveParent());
	if (pMount)
	{
		int nBuildPoint = pMount->FindObjectOnBuildPoint( this );
		if (nBuildPoint >= 0)
		{
			pMount->GetExitPoint( pPlayer, nBuildPoint, pAbsPosition, pAbsAngles );
			return;
		}
	}

	Vector vNewPos;
	GetInitialPassengerExitPoint( nRole, pAbsPosition, pAbsAngles );
	if ( EntityPlacementTest(pPlayer, *pAbsPosition, vNewPos, true) )
	{
		*pAbsPosition = vNewPos;
		return;
	}

	// Find the first valid exit point
	for( int iExitPoint = 0; iExitPoint < m_nMaxPassengers; ++iExitPoint )
	{
		if (iExitPoint == nRole)
			continue;

		GetInitialPassengerExitPoint( iExitPoint, pAbsPosition, pAbsAngles );
		if ( EntityPlacementTest(pPlayer, *pAbsPosition, vNewPos, true) )
		{
			*pAbsPosition = vNewPos;
			return;
		}
	}

	// Worst case, we will be returning the vehicle's origin + 50z here
	*pAbsPosition = GetAbsOrigin();
	pAbsPosition->z = WorldAlignMaxs()[2] + 150.0f;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CBaseTFVehicle::GetPassengerExitPoint( int nRole, Vector *pAbsPosition, QAngle *pAbsAngles )
{
	// FIXME: Clean this up
	CBasePlayer *pPlayer = GetPassenger(nRole);
	GetPassengerExitPoint( pPlayer, nRole, pAbsPosition, pAbsAngles );
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CBaseTFVehicle::GetEntryAnimForPoint( const Vector &vecPoint )
{
	return ACTIVITY_NOT_AVAILABLE;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CBaseTFVehicle::GetExitAnimToUse( Vector &vecEyeExitEndpoint, bool &bAllPointsBlocked )
{
	bAllPointsBlocked = false;
	return ACTIVITY_NOT_AVAILABLE;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTFVehicle::HandleEntryExitFinish( bool bExitAnimOn, bool bResetAnim )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPlayer - 
//			false - 
//-----------------------------------------------------------------------------
void CBaseTFVehicle::HandlePassengerEntry( CBasePlayer *pPlayer, bool bAllowEntryOutsideZone )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPlayer - 
//-----------------------------------------------------------------------------
bool CBaseTFVehicle::HandlePassengerExit( CBasePlayer *pPlayer )
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Get and set the current driver.
//-----------------------------------------------------------------------------
void CBaseTFVehicle::SetPassenger( int nRole, CBasePlayer *pEnt )
{
	Assert( !pEnt || pEnt->IsPlayer() );
	Assert( nRole >= 0 && nRole < m_nMaxPassengers );
	Assert( !m_hPassengers[nRole].Get() || !pEnt );
	m_hPassengers.Set( nRole, dynamic_cast<CBaseTFPlayer*>(pEnt) );

	// If the vehicle's deteriorating, I get to own it now
	if ( IsDeteriorating() )
	{
		StopDeteriorating();
		SetBuilder( (CBaseTFPlayer*)pEnt, true );
	}

	ResetDeteriorationTime();
}

#endif

//-----------------------------------------------------------------------------
// Get a position in *world space* inside the vehicle for the player to start at
//-----------------------------------------------------------------------------
void CBaseTFVehicle::GetPassengerStartPoint( int nRole, Vector *pAbsPoint, QAngle *pAbsAngles )
{
	char pAttachmentName[32];
	Q_snprintf( pAttachmentName, sizeof( pAttachmentName ), "vehicle_feet_passenger%d", nRole );
	int nFeetAttachmentIndex = LookupAttachment(pAttachmentName);
	GetAttachment( nFeetAttachmentIndex, *pAbsPoint, *pAbsAngles );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
#define INITIAL_MAX_DISTANCE  999999.0f

int CBaseTFVehicle::LocateEntryPoint( CBaseTFPlayer *pPlayer, float* fBest2dDistanceSqr )
{
	// Get the players origin and compare it to all the entry points on the
	// vehicle.
	Vector vecPlayerPos = pPlayer->GetAbsOrigin();
	Vector vecEntryPos;
	QAngle vecEntryAngle;

	int   iMinEntry = -1;
	float flMinDistance2 = INITIAL_MAX_DISTANCE;

	// Is the player the owner of the vehicle?
	bool bOwner = ( pPlayer == GetOwner() );

	char szPassengerEyes[32];
	for( int iEntryPoint = 0; iEntryPoint < m_nMaxPassengers; ++iEntryPoint )
	{
		// If not the owner, check to see if the entry point is available.  The
		// entry point is always available for the owner.
		bool bOccupied = ( GetPassenger( iEntryPoint ) != NULL );

		// Also check for child vehicles...

		if ( bOccupied && !bOwner )
			continue;
	
		// FIXME: Cache off the entry point
		Q_snprintf( szPassengerEyes, sizeof( szPassengerEyes ), "vehicle_feet_passenger%d", iEntryPoint );
		int nAttachmentIndex = LookupAttachment( szPassengerEyes );

		float flDistance2;
		if (nAttachmentIndex > 0)
		{
			GetAttachment( nAttachmentIndex, vecEntryPos, vecEntryAngle );
			Vector vecDelta = vecEntryPos - vecPlayerPos;
			flDistance2 = vecDelta.AsVector2D().LengthSqr();
		}
		else
		{
			// No attachment? Choose it if we must as a last resort
			flDistance2 = INITIAL_MAX_DISTANCE - 1;
		}

		if ( flDistance2 < flMinDistance2 )
		{
			flMinDistance2 = flDistance2;
			iMinEntry = iEntryPoint;
		}
	}

	if( fBest2dDistanceSqr )
	{
		*fBest2dDistanceSqr = flMinDistance2;
	}
	return iMinEntry;
}

//-----------------------------------------------------------------------------
// Purpose: Set a gun that the driver can control
//-----------------------------------------------------------------------------
void CBaseTFVehicle::SetDriverGun( CBaseObjectDriverGun *pGun )
{
	m_hDriverGun = pGun;
}

//-----------------------------------------------------------------------------
// Purpose: Update the driver's gun
//-----------------------------------------------------------------------------
void CBaseTFVehicle::VehicleDriverGunThink( void )
{
	if ( !m_hDriverGun )
		return;

	// No driver?
	CBaseTFPlayer *pDriver = GetDriverPlayer();
	if ( !pDriver )
		return;

	QAngle vecTargetAngles = m_hDriverGun->GetCurrentAngles();

	// Cast a ray out of the view to see where the player is looking.
	trace_t trace;
	Vector vecForward;
	Vector vecSrc;
	QAngle angEyeAngles;
	GetVehicleViewPosition( VEHICLE_DRIVER, &vecSrc, &angEyeAngles, NULL );
	AngleVectors( angEyeAngles, &vecForward, NULL, NULL );
	Vector vecEnd = vecSrc + (vecForward * 10000);
	UTIL_TraceLine( vecSrc, vecEnd, MASK_OPAQUE, this, COLLISION_GROUP_NONE, &trace );

	//NDebugOverlay::Box( vecSrc, -Vector(10,10,10), Vector(10,10,10), 255,0,0,8, 5 );
	//NDebugOverlay::Box( vecEnd, -Vector(10,10,10), Vector(10,10,10), 0,255,0,8, 5 );
	//NDebugOverlay::Box( trace.endpos, -Vector(10,10,10), Vector(10,10,10), 255,255,255,8, 0.1 );

	if ( trace.fraction < 1 )
	{
		// Figure out what angles our turret needs to be at in order to hit the target.
		Vector vFireOrigin = m_hDriverGun->GetFireOrigin();

		//NDebugOverlay::Box( vFireOrigin, -Vector(10,10,10), Vector(10,10,10), 0,255,0,8, 0.1 );

		// Get a direction vector that points at the target.
		Vector vTo = trace.endpos - vFireOrigin;

		// Transform it into the tank's local space.
		matrix3x4_t tankToWorld;
		AngleMatrix( GetAbsAngles(), tankToWorld );
		
		Vector vLocalTo;
		VectorITransform( vTo, tankToWorld, vLocalTo );

		// Now figure out what the angles are in local space.
		QAngle localAngles;
		VectorAngles( vLocalTo, localAngles );

		vecTargetAngles[YAW] = localAngles[YAW] - 90;
		vecTargetAngles[PITCH] = anglemod( localAngles[PITCH] );
	}

	// Set the gun's angles
	m_hDriverGun->SetTargetAngles( vecTargetAngles );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CBaseTFVehicle::ShouldUseThirdPersonVehicleView()
{
	return true;
}

//-----------------------------------------------------------------------------
// Returns the unperterbed view position for a particular role
//-----------------------------------------------------------------------------
bool CBaseTFVehicle::GetRoleViewPosition( int nRole, Vector *pVehicleEyeOrigin, QAngle *pVehicleEyeAngles )
{
	// Generate the view position in world space.
	Vector vAbsOrigin;
	QAngle vAbsAngle;
	bool bUsingThirdPersonCamera = GetRoleAbsViewPosition( nRole, &vAbsOrigin, &vAbsAngle );

	
	// Make a matrix for it.
	matrix3x4_t absMatrix;
	AngleMatrix( vAbsAngle, absMatrix );
	MatrixSetColumn( vAbsOrigin, 3, absMatrix );


	// Transform the matrix into local space.
	matrix3x4_t worldToEntity, local;
	MatrixInvert( EntityToWorldTransform(), worldToEntity );
	ConcatTransforms( worldToEntity, absMatrix, local ); 


	// Suck out the origin and angles.
	pVehicleEyeOrigin->Init( local[0][3], local[1][3], local[2][3] );
	MatrixAngles( local, *pVehicleEyeAngles );

	return bUsingThirdPersonCamera;
}

bool CBaseTFVehicle::GetRoleAbsViewPosition( int nRole, Vector *pAbsVehicleEyeOrigin, QAngle *pAbsVehicleEyeAngles )
{
	int iAttachment = LookupAttachment( "ThirdPersonCameraOrigin" );
	if ( ShouldUseThirdPersonVehicleView() && vehicle_thirdperson.GetInt() && nRole == VEHICLE_DRIVER && iAttachment > 0 )
	{
		// Ok, we're using third person. Leave their angles intact but rotate theirt view around the
		// ThirdPersonCameraOrigin attachment.
		Vector vAttachOrigin;
		QAngle vAttachAngles;
		GetAttachment( iAttachment, vAttachOrigin, vAttachAngles );

		Vector vForward, vRight, vUp;
		AngleVectors( *pAbsVehicleEyeAngles, &vForward, &vRight, &vUp );

		*pAbsVehicleEyeOrigin = vAttachOrigin + vForward * vehicle_view_offset_forward.GetFloat() + 
			vRight * vehicle_view_offset_right.GetFloat() + 
			vUp * vehicle_view_offset_up.GetFloat();
	
		// Returning true tells the caller that we're using a third-person camera origin.
		return true;
	}
	else
	{
		// Use the vehicle_eyes_passengerX attachments.
		Assert( nRole >= 0 );
		char pAttachmentName[32];
		Q_snprintf( pAttachmentName, sizeof( pAttachmentName ), "vehicle_eyes_passenger%d", nRole );
		int eyeAttachmentIndex = LookupAttachment(pAttachmentName);
		
		QAngle vTempAngles;
		GetAttachment( eyeAttachmentIndex, *pAbsVehicleEyeOrigin, vTempAngles );
		
		if ( vehicle_attach_eye_angles.GetInt() )
			*pAbsVehicleEyeAngles = vTempAngles;

		return false;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Modify the player view/camera while in a vehicle
//-----------------------------------------------------------------------------
void CBaseTFVehicle::GetVehicleViewPosition( int nRole, Vector *pAbsOrigin, QAngle *pAbsAngles, float *pFOV /*= NULL*/ )
{
	// UNDONE: Use attachment point on the vehicle, not hardcoded player eyes
	Assert( nRole >= 0 );
	CBasePlayer *pPlayer = GetPassenger( nRole );
	Assert( pPlayer );

	Vector vehicleEyeOrigin;
	QAngle vehicleEyeAngles = pPlayer->LocalEyeAngles();
	
	GetRoleAbsViewPosition( nRole, &vehicleEyeOrigin, &vehicleEyeAngles ); 

	*pAbsOrigin = vehicleEyeOrigin;
	*pAbsAngles = vehicleEyeAngles;

	/*
 	if ( bUsingThirdPersonCamera )
	{
		*pAbsOrigin = vehicleEyeOrigin;
		*pAbsAngles = vehicleEyeAngles;
	}
	else
	{
		matrix3x4_t vehicleEyePosToWorld;

		AngleMatrix( vehicleEyeAngles, vehicleEyePosToWorld );

		// Compute the relative rotation between the unperterbed eye attachment + the eye angles
		matrix3x4_t cameraToWorld;
		AngleMatrix( *pAbsAngles, cameraToWorld );

		matrix3x4_t worldToEyePos;
		MatrixInvert( vehicleEyePosToWorld, worldToEyePos );

		matrix3x4_t vehicleCameraToEyePos;
		ConcatTransforms( worldToEyePos, cameraToWorld, vehicleCameraToEyePos );

		// Now perterb the attachment point
		if( inv_demo.GetInt() )
		{
			vehicleEyeAngles.x = RemapAngleRange( PITCH_CURVE_ZERO * road_feel.GetFloat(), PITCH_CURVE_LINEAR, vehicleEyeAngles.x );
			vehicleEyeAngles.z = RemapAngleRange( ROLL_CURVE_ZERO * road_feel.GetFloat(), ROLL_CURVE_LINEAR, vehicleEyeAngles.z );
		} 
		else
		{
			vehicleEyeAngles.x = RemapAngleRange( PITCH_CURVE_ZERO, PITCH_CURVE_LINEAR, vehicleEyeAngles.x );
			vehicleEyeAngles.z = RemapAngleRange( ROLL_CURVE_ZERO, ROLL_CURVE_LINEAR, vehicleEyeAngles.z );
		}

		AngleMatrix( vehicleEyeAngles, vehicleEyeOrigin, vehicleEyePosToWorld );

		// Now treat the relative eye angles as being relative to this new, perterbed view position...
		matrix3x4_t newCameraToWorld;
		ConcatTransforms( vehicleEyePosToWorld, vehicleCameraToEyePos, newCameraToWorld );

		// output new view abs angles
		MatrixAngles( newCameraToWorld, *pAbsAngles );

		// UNDONE: *pOrigin would already be correct in single player if the HandleView() on the server ran after vphysics
		MatrixGetColumn( newCameraToWorld, 3, *pAbsOrigin );
	}
	*/
}


//-----------------------------------------------------------------------------
//
// Client-only code here
//
//-----------------------------------------------------------------------------

#if defined (CLIENT_DLL)

void CBaseTFVehicle::ClientThink()
{
	BaseClass::ClientThink();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CBaseTFVehicle::ShouldPredict( void )
{
	// Only predict vehicles driven by local players
	return GetDriverPlayer() ? GetDriverPlayer()->IsLocalPlayer() : false;
}

//-----------------------------------------------------------------------------
// Purpose: Get the angles that a player in the specified role should be using for visuals
//-----------------------------------------------------------------------------
QAngle CBaseTFVehicle::GetPassengerAngles( QAngle angCurrent, int nRole )
{
	// Just use your current angles
	return angCurrent;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTFVehicle::DrawHudElements( void )
{
	// If we've got a driver gun, tell it to draw it's elements
	if ( m_hDriverGun )
	{
		m_hDriverGun->DrawHudElements();
	}

	DrawHudBoostData();
	SetupCrosshair();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CBaseTFVehicle::DrawHudBoostData( void )
{
#define HUD_IMAGE_LEFT	XRES( 568 )

	// Boostable vehicle
	if ( IsBoostable() )
	{
		// Set our color
		CHudTexture *pVehicleBoostLabel = gHUD.GetIcon( "no2" );
		if ( pVehicleBoostLabel )
		{
			int nScreenY = ScreenHeight() - YRES( 12 );
			float nOneHudHeight = ( YRES(10) + pVehicleBoostLabel->Height() );
			nScreenY -= ( nOneHudHeight * 3 );
		
			pVehicleBoostLabel->DrawSelf( HUD_IMAGE_LEFT, nScreenY - pVehicleBoostLabel->Height(), gHUD.m_clrNormal );
			gHUD.DrawProgressBar( HUD_IMAGE_LEFT, nScreenY + YRES( 4 ), XRES( 70 ), YRES( 4 ), m_nBoostTimeLeft / 100.0f, gHUD.m_clrNormal, CHud::HUDPB_HORIZONTAL_INV );
		}
	}

#undef HUD_IMAGE_LEFT
}

//-----------------------------------------------------------------------------
// Purpose: Set a crosshair when in a vehicle and we don't have a proper 
//          crosshair sprite (ie. a commando laser rifle).
//-----------------------------------------------------------------------------
void CBaseTFVehicle::SetupCrosshair( void )
{
	if ( !m_pIconDefaultCrosshair )
	{
		// Init the default crosshair the first time.
		CHudTexture newTexture;
		Q_strncpy( newTexture.szTextureFile, "sprites/crosshairs", sizeof( newTexture.szTextureFile ) );

		newTexture.rc.left		= 0;
		newTexture.rc.top		= 48;
		newTexture.rc.right		= newTexture.rc.left + 24;
		newTexture.rc.bottom	= newTexture.rc.top + 24;
		m_pIconDefaultCrosshair = gHUD.AddUnsearchableHudIconToList( newTexture );
	}

	CHudCrosshair *crosshair = GET_HUDELEMENT( CHudCrosshair );
	if ( crosshair )
	{
		if ( !crosshair->HasCrosshair() && m_pIconDefaultCrosshair )
		{
			crosshair->SetCrosshair( m_pIconDefaultCrosshair, gHUD.m_clrNormal );
		}
	}
}

#endif
