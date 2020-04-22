//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#include "cbase.h"
#include "prop_portal.h"
#include "portal_player.h"
#include "portal/weapon_physcannon.h"
#include "physics_npc_solver.h"
#include "envmicrophone.h"
#include "env_speaker.h"
#include "func_portal_detector.h"
#include "model_types.h"
#include "te_effect_dispatch.h"
#include "collisionutils.h"
#include "physobj.h"
#include "world.h"
#include "hierarchy.h"
#include "physics_saverestore.h"
#include "PhysicsCloneArea.h"
#include "portal_gamestats.h"
#include "prop_portal_shared.h"
#include "weapon_portalgun.h"
#include "portal_placement.h"
#include "physicsshadowclone.h"
#include "particle_parse.h"
#include "rumble_shared.h"
#include "func_portal_orientation.h"
#include "env_debughistory.h"
#include "tier1/callqueue.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


#define MINIMUM_FLOOR_PORTAL_EXIT_VELOCITY 50.0f
#define MINIMUM_FLOOR_TO_FLOOR_PORTAL_EXIT_VELOCITY 225.0f
#define MINIMUM_FLOOR_PORTAL_EXIT_VELOCITY_PLAYER 300.0f
#define MAXIMUM_PORTAL_EXIT_VELOCITY 1000.0f

CCallQueue *GetPortalCallQueue();


ConVar sv_portal_debug_touch("sv_portal_debug_touch", "0", FCVAR_REPLICATED );
ConVar sv_portal_placement_never_fail("sv_portal_placement_never_fail", "0", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar sv_portal_new_velocity_check("sv_portal_new_velocity_check", "1", FCVAR_CHEAT );

static CUtlVector<CProp_Portal *> s_PortalLinkageGroups[256];


BEGIN_DATADESC( CProp_Portal )
	//saving
	DEFINE_FIELD( m_hLinkedPortal,		FIELD_EHANDLE ),
	DEFINE_KEYFIELD( m_iLinkageGroupID,	FIELD_CHARACTER,	"LinkageGroupID" ),
	DEFINE_FIELD( m_matrixThisToLinked, FIELD_VMATRIX ),
	DEFINE_KEYFIELD( m_bActivated,		FIELD_BOOLEAN,		"Activated" ),
	DEFINE_KEYFIELD( m_bIsPortal2,		FIELD_BOOLEAN,		"PortalTwo" ),
	DEFINE_FIELD( m_vPrevForward,		FIELD_VECTOR ),
	DEFINE_FIELD( m_hMicrophone,		FIELD_EHANDLE ),
	DEFINE_FIELD( m_hSpeaker,			FIELD_EHANDLE ),

	DEFINE_SOUNDPATCH( m_pAmbientSound ),

	DEFINE_FIELD( m_vAudioOrigin,		FIELD_VECTOR ),
	DEFINE_FIELD( m_vDelayedPosition,	FIELD_VECTOR ),
	DEFINE_FIELD( m_qDelayedAngles,		FIELD_VECTOR ),
	DEFINE_FIELD( m_iDelayedFailure,	FIELD_INTEGER ),
	DEFINE_FIELD( m_hPlacedBy,			FIELD_EHANDLE ),

	// DEFINE_FIELD( m_plane_Origin, cplane_t ),
	// DEFINE_FIELD( m_pAttachedCloningArea, CPhysicsCloneArea ),
	// DEFINE_FIELD( m_PortalSimulator, CPortalSimulator ),
	// DEFINE_FIELD( m_pCollisionShape, CPhysCollide ),
	
	DEFINE_FIELD( m_bSharedEnvironmentConfiguration, FIELD_BOOLEAN ),
	DEFINE_ARRAY( m_vPortalCorners, FIELD_POSITION_VECTOR, 4 ),

	// Function Pointers
	DEFINE_THINKFUNC( DelayedPlacementThink ),
	DEFINE_THINKFUNC( TestRestingSurfaceThink ),
	DEFINE_THINKFUNC( FizzleThink ),

	DEFINE_INPUTFUNC( FIELD_BOOLEAN, "SetActivatedState", InputSetActivatedState ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Fizzle", InputFizzle ),
	DEFINE_INPUTFUNC( FIELD_STRING, "NewLocation", InputNewLocation ),

	DEFINE_OUTPUT( m_OnPlacedSuccessfully, "OnPlacedSuccessfully" ),

END_DATADESC()

IMPLEMENT_SERVERCLASS_ST( CProp_Portal, DT_Prop_Portal )
	SendPropEHandle( SENDINFO(m_hLinkedPortal) ),
	SendPropBool( SENDINFO(m_bActivated) ),
	SendPropBool( SENDINFO(m_bIsPortal2) ),
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( prop_portal, CProp_Portal );






CProp_Portal::CProp_Portal( void )
{
	m_vPrevForward = Vector( 0.0f, 0.0f, 0.0f );
	m_PortalSimulator.SetPortalSimulatorCallbacks( this );

	// Init to something safe
	for ( int i = 0; i < 4; ++i )
	{
		m_vPortalCorners[i] = Vector(0,0,0);
	}

	//create the collision shape.... TODO: consider having one shared collideable between all portals
	float fPlanes[6*4];
	fPlanes[(0*4) + 0] = 1.0f;
	fPlanes[(0*4) + 1] = 0.0f;
	fPlanes[(0*4) + 2] = 0.0f;
	fPlanes[(0*4) + 3] = CProp_Portal_Shared::vLocalMaxs.x;

	fPlanes[(1*4) + 0] = -1.0f;
	fPlanes[(1*4) + 1] = 0.0f;
	fPlanes[(1*4) + 2] = 0.0f;
	fPlanes[(1*4) + 3] = -CProp_Portal_Shared::vLocalMins.x;

	fPlanes[(2*4) + 0] = 0.0f;
	fPlanes[(2*4) + 1] = 1.0f;
	fPlanes[(2*4) + 2] = 0.0f;
	fPlanes[(2*4) + 3] = CProp_Portal_Shared::vLocalMaxs.y;

	fPlanes[(3*4) + 0] = 0.0f;
	fPlanes[(3*4) + 1] = -1.0f;
	fPlanes[(3*4) + 2] = 0.0f;
	fPlanes[(3*4) + 3] = -CProp_Portal_Shared::vLocalMins.y;

	fPlanes[(4*4) + 0] = 0.0f;
	fPlanes[(4*4) + 1] = 0.0f;
	fPlanes[(4*4) + 2] = 1.0f;
	fPlanes[(4*4) + 3] = CProp_Portal_Shared::vLocalMaxs.z;

	fPlanes[(5*4) + 0] = 0.0f;
	fPlanes[(5*4) + 1] = 0.0f;
	fPlanes[(5*4) + 2] = -1.0f;
	fPlanes[(5*4) + 3] = -CProp_Portal_Shared::vLocalMins.z;

	CPolyhedron *pPolyhedron = GeneratePolyhedronFromPlanes( fPlanes, 6, 0.00001f, true );
	Assert( pPolyhedron != NULL );
	CPhysConvex *pConvex = physcollision->ConvexFromConvexPolyhedron( *pPolyhedron );
	pPolyhedron->Release();
	Assert( pConvex != NULL );
	m_pCollisionShape = physcollision->ConvertConvexToCollide( &pConvex, 1 );

	CProp_Portal_Shared::AllPortals.AddToTail( this );
}

CProp_Portal::~CProp_Portal( void )
{
	CProp_Portal_Shared::AllPortals.FindAndRemove( this );
	s_PortalLinkageGroups[m_iLinkageGroupID].FindAndRemove( this );
}


void CProp_Portal::UpdateOnRemove( void )
{
	m_PortalSimulator.ClearEverything();

	RemovePortalMicAndSpeaker();

	CProp_Portal *pRemote = m_hLinkedPortal;
	if( pRemote != NULL )
	{
		m_PortalSimulator.DetachFromLinked();
		m_hLinkedPortal = NULL;
		m_bActivated = false;
		pRemote->UpdatePortalLinkage();
		pRemote->UpdatePortalTeleportMatrix();
	}

	if( m_pAttachedCloningArea )
	{
		UTIL_Remove( m_pAttachedCloningArea );
		m_pAttachedCloningArea = NULL;
	}
	

	BaseClass::UpdateOnRemove();
}

void CProp_Portal::Precache( void )
{
	PrecacheScriptSound( "Portal.ambient_loop" );

	PrecacheScriptSound( "Portal.open_blue" );
	PrecacheScriptSound( "Portal.open_red" );
	PrecacheScriptSound( "Portal.close_blue" );
	PrecacheScriptSound( "Portal.close_red" );
	PrecacheScriptSound( "Portal.fizzle_moved" );
	PrecacheScriptSound( "Portal.fizzle_invalid_surface" );

	PrecacheModel( "models/portals/portal1.mdl" );
	PrecacheModel( "models/portals/portal2.mdl" );

	PrecacheParticleSystem( "portal_1_particles" );
	PrecacheParticleSystem( "portal_2_particles" );
	PrecacheParticleSystem( "portal_1_edge" );
	PrecacheParticleSystem( "portal_2_edge" );
	PrecacheParticleSystem( "portal_1_nofit" );
	PrecacheParticleSystem( "portal_2_nofit" );
	PrecacheParticleSystem( "portal_1_overlap" );
	PrecacheParticleSystem( "portal_2_overlap" );
	PrecacheParticleSystem( "portal_1_badvolume" );
	PrecacheParticleSystem( "portal_2_badvolume" );
	PrecacheParticleSystem( "portal_1_badsurface" );
	PrecacheParticleSystem( "portal_2_badsurface" );
	PrecacheParticleSystem( "portal_1_close" );
	PrecacheParticleSystem( "portal_2_close" );
	PrecacheParticleSystem( "portal_1_cleanser" );
	PrecacheParticleSystem( "portal_2_cleanser" );
	PrecacheParticleSystem( "portal_1_near" );
	PrecacheParticleSystem( "portal_2_near" );
	PrecacheParticleSystem( "portal_1_success" );
	PrecacheParticleSystem( "portal_2_success" );

	BaseClass::Precache();
}

void CProp_Portal::CreateSounds()
{
	if (!m_pAmbientSound)
	{
		CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();

		CPASAttenuationFilter filter( this );

		m_pAmbientSound = controller.SoundCreate( filter, entindex(), "Portal.ambient_loop" );
		controller.Play( m_pAmbientSound, 0, 100 );
	}
}

void CProp_Portal::StopLoopingSounds()
{
	if ( m_pAmbientSound )
	{
		CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();

		controller.SoundDestroy( m_pAmbientSound );
		m_pAmbientSound = NULL;
	}

	BaseClass::StopLoopingSounds();
}

void CProp_Portal::Spawn( void )
{
	Precache();

	Assert( s_PortalLinkageGroups[m_iLinkageGroupID].Find( this ) == -1 );
	s_PortalLinkageGroups[m_iLinkageGroupID].AddToTail( this );

	m_matrixThisToLinked.Identity(); //don't accidentally teleport objects to zero space
	
	AddEffects( EF_NORECEIVESHADOW | EF_NOSHADOW );

	SetSolid( SOLID_OBB );
	SetSolidFlags( FSOLID_TRIGGER | FSOLID_NOT_SOLID | FSOLID_CUSTOMBOXTEST | FSOLID_CUSTOMRAYTEST );
	SetMoveType( MOVETYPE_NONE );
	SetCollisionGroup( COLLISION_GROUP_PLAYER );

	//VPhysicsInitNormal( SOLID_VPHYSICS, FSOLID_TRIGGER, false );
	//CreateVPhysics();
	ResetModel();	
	SetSize( CProp_Portal_Shared::vLocalMins, CProp_Portal_Shared::vLocalMaxs );

	UpdateCorners();

	BaseClass::Spawn();

	m_pAttachedCloningArea = CPhysicsCloneArea::CreatePhysicsCloneArea( this );
}

void CProp_Portal::OnRestore()
{
	UpdateCorners();

	Assert( m_pAttachedCloningArea == NULL );
	m_pAttachedCloningArea = CPhysicsCloneArea::CreatePhysicsCloneArea( this );

	BaseClass::OnRestore();

	if ( m_bActivated )
	{
		DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_2_particles" ) : ( "portal_1_particles" ) ), PATTACH_POINT_FOLLOW, this, "particles_2", true );
		DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_2_edge" ) : ( "portal_1_edge" ) ), PATTACH_POINT_FOLLOW, this, "particlespin" );
	}
}

void DumpActiveCollision( const CPortalSimulator *pPortalSimulator, const char *szFileName );
void PortalSimulatorDumps_DumpCollideToGlView( CPhysCollide *pCollide, const Vector &origin, const QAngle &angles, float fColorScale, const char *pFilename );

bool CProp_Portal::TestCollision( const Ray_t &ray, unsigned int fContentsMask, trace_t& tr )
{
	physcollision->TraceBox( ray, MASK_ALL, NULL, m_pCollisionShape, GetAbsOrigin(), GetAbsAngles(), &tr );
	return tr.DidHit();
}

//-----------------------------------------------------------------------------
// Purpose: Runs when a fired portal shot reaches it's destination wall. Detects current placement valididty state.
//-----------------------------------------------------------------------------
void CProp_Portal::DelayedPlacementThink( void )
{
	Vector vOldOrigin = GetLocalOrigin();
	QAngle qOldAngles = GetLocalAngles();

	Vector vForward;
	AngleVectors( m_qDelayedAngles, &vForward );

	// Check if something made the spot invalid mid flight
	// Bad surface and near fizzle effects take priority
	if ( m_iDelayedFailure != PORTAL_FIZZLE_BAD_SURFACE && m_iDelayedFailure != PORTAL_FIZZLE_NEAR_BLUE && m_iDelayedFailure != PORTAL_FIZZLE_NEAR_RED )
	{
		if ( IsPortalOverlappingOtherPortals( this, m_vDelayedPosition, m_qDelayedAngles ) )
		{
			m_iDelayedFailure = PORTAL_FIZZLE_OVERLAPPED_LINKED;
		}
		else if ( IsPortalIntersectingNoPortalVolume( m_vDelayedPosition, m_qDelayedAngles, vForward ) )
		{
			m_iDelayedFailure = PORTAL_FIZZLE_BAD_VOLUME;
		}
	}

	if ( sv_portal_placement_never_fail.GetBool() )
	{
		m_iDelayedFailure = PORTAL_FIZZLE_SUCCESS;
	}

	DoFizzleEffect( m_iDelayedFailure );

	if ( m_iDelayedFailure != PORTAL_FIZZLE_SUCCESS )
	{
		// It didn't successfully place
		return;
	}

	// Do effects at old location if it was active
	if ( m_bActivated )
	{
		DoFizzleEffect( PORTAL_FIZZLE_CLOSE, false );
	}

	CWeaponPortalgun *pPortalGun = dynamic_cast<CWeaponPortalgun*>( m_hPlacedBy.Get() );

	if( pPortalGun )
	{
		CPortal_Player *pFiringPlayer = dynamic_cast<CPortal_Player *>( pPortalGun->GetOwner() );
		if( pFiringPlayer )
		{
			pFiringPlayer->IncrementPortalsPlaced();

			// Placement successful, fire the output
			m_OnPlacedSuccessfully.FireOutput( pPortalGun, this );

		}
	}

	// Move to new location
	NewLocation( m_vDelayedPosition, m_qDelayedAngles );

	SetContextThink( &CProp_Portal::TestRestingSurfaceThink, gpGlobals->curtime + 0.1f, s_pTestRestingSurfaceContext );
}

//-----------------------------------------------------------------------------
// Purpose: When placed on a surface that could potentially go away (anything but world geo), we test for that condition and fizzle
//-----------------------------------------------------------------------------
void CProp_Portal::TestRestingSurfaceThink( void )
{
	// Make sure there's still a surface behind the portal
	Vector vOrigin = GetAbsOrigin();

	Vector vForward, vRight, vUp;
	GetVectors( &vForward, &vRight, &vUp );

	trace_t tr;
	CTraceFilterSimpleClassnameList baseFilter( NULL, COLLISION_GROUP_NONE );
	UTIL_Portal_Trace_Filter( &baseFilter );
	baseFilter.AddClassnameToIgnore( "prop_portal" );
	CTraceFilterTranslateClones traceFilterPortalShot( &baseFilter );

	int iCornersOnVolatileSurface = 0;

	// Check corners
	for ( int iCorner = 0; iCorner < 4; ++iCorner )
	{
		Vector vCorner = vOrigin;

		if ( iCorner % 2 == 0 )
			vCorner += vRight * ( PORTAL_HALF_WIDTH - PORTAL_BUMP_FORGIVENESS * 1.1f );
		else
			vCorner += -vRight * ( PORTAL_HALF_WIDTH - PORTAL_BUMP_FORGIVENESS * 1.1f );

		if ( iCorner < 2 )
			vCorner += vUp * ( PORTAL_HALF_HEIGHT - PORTAL_BUMP_FORGIVENESS * 1.1f );
		else
			vCorner += -vUp * ( PORTAL_HALF_HEIGHT - PORTAL_BUMP_FORGIVENESS * 1.1f );

		Ray_t ray;
		ray.Init( vCorner, vCorner - vForward );
		enginetrace->TraceRay( ray, MASK_SOLID_BRUSHONLY, &traceFilterPortalShot, &tr );

		// This corner isn't on a valid brush (skipping phys converts or physboxes because they frequently go through portals and can't be placed upon).
		if ( tr.fraction == 1.0f && !tr.startsolid && ( !tr.m_pEnt || ( tr.m_pEnt && !FClassnameIs( tr.m_pEnt, "func_physbox" ) && !FClassnameIs( tr.m_pEnt, "simple_physics_brush" ) ) ) )
		{
			DevMsg( "Surface removed from behind portal.\n" );
			Fizzle();
			SetContextThink( NULL, TICK_NEVER_THINK, s_pTestRestingSurfaceContext );
			break;
		}

		if ( !tr.DidHitWorld() )
		{
			iCornersOnVolatileSurface++;
		}
	}

	// Still on a movable or deletable surface
	if ( iCornersOnVolatileSurface > 0 )
	{
		SetContextThink ( &CProp_Portal::TestRestingSurfaceThink, gpGlobals->curtime + 0.1f, s_pTestRestingSurfaceContext );
	}
	else
	{
		// All corners on world, we don't need to test
		SetContextThink( NULL, TICK_NEVER_THINK, s_pTestRestingSurfaceContext );
	}
}

bool CProp_Portal::IsActivedAndLinked( void ) const
{
	return ( m_bActivated && m_hLinkedPortal.Get() != NULL );
}

void CProp_Portal::ResetModel( void )
{
	if( !m_bIsPortal2 )
		SetModel( "models/portals/portal1.mdl" );
	else
		SetModel( "models/portals/portal2.mdl" );

	SetSize( CProp_Portal_Shared::vLocalMins, CProp_Portal_Shared::vLocalMaxs );

	SetSolid( SOLID_OBB );
	SetSolidFlags( FSOLID_TRIGGER | FSOLID_NOT_SOLID | FSOLID_CUSTOMBOXTEST | FSOLID_CUSTOMRAYTEST );
}

void CProp_Portal::DoFizzleEffect( int iEffect, bool bDelayedPos /*= true*/ )
{
	m_vAudioOrigin = ( ( bDelayedPos ) ? ( m_vDelayedPosition ) : ( GetAbsOrigin() ) );

	CEffectData	fxData;

	fxData.m_vAngles = ( ( bDelayedPos ) ? ( m_qDelayedAngles ) : ( GetAbsAngles() ) );

	Vector vForward, vUp;
	AngleVectors( fxData.m_vAngles, &vForward, &vUp, NULL );
	fxData.m_vOrigin = m_vAudioOrigin + vForward * 1.0f;

	fxData.m_nColor = ( ( m_bIsPortal2 ) ? ( 1 ) : ( 0 ) );

	EmitSound_t ep;
	CPASAttenuationFilter filter( m_vDelayedPosition );

	ep.m_nChannel = CHAN_STATIC;
	ep.m_flVolume = 1.0f;
	ep.m_pOrigin = &m_vAudioOrigin;

	// Rumble effects on the firing player (if one exists)
	CWeaponPortalgun *pPortalGun = dynamic_cast<CWeaponPortalgun*>( m_hPlacedBy.Get() );

	if ( pPortalGun && (iEffect != PORTAL_FIZZLE_CLOSE ) 
				    && (iEffect != PORTAL_FIZZLE_SUCCESS )
				    && (iEffect != PORTAL_FIZZLE_NONE )		)
	{
		CBasePlayer* pPlayer = (CBasePlayer*)pPortalGun->GetOwner();
		if ( pPlayer )
		{
			pPlayer->RumbleEffect( RUMBLE_PORTAL_PLACEMENT_FAILURE, 0, RUMBLE_FLAGS_NONE );
		}
	}
	
	// Pick a fizzle effect
	switch ( iEffect )
	{
		case PORTAL_FIZZLE_CANT_FIT:
			//DispatchEffect( "PortalFizzleCantFit", fxData );
			//ep.m_pSoundName = "Portal.fizzle_invalid_surface";
			VectorAngles( vUp, vForward, fxData.m_vAngles );
			DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_2_nofit" ) : ( "portal_1_nofit" ) ), fxData.m_vOrigin, fxData.m_vAngles, this );
			break;

		case PORTAL_FIZZLE_OVERLAPPED_LINKED:
		{
			/*CProp_Portal *pLinkedPortal = m_hLinkedPortal;
			if ( pLinkedPortal )
			{
				Vector vLinkedForward;
				pLinkedPortal->GetVectors( &vLinkedForward, NULL, NULL );
				fxData.m_vStart = pLink3edPortal->GetAbsOrigin() + vLinkedForward * 5.0f;
			}*/

			//DispatchEffect( "PortalFizzleOverlappedLinked", fxData );
			VectorAngles( vUp, vForward, fxData.m_vAngles );
			DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_2_overlap" ) : ( "portal_1_overlap" ) ), fxData.m_vOrigin, fxData.m_vAngles, this );
			ep.m_pSoundName = "Portal.fizzle_invalid_surface";
			break;
		}

		case PORTAL_FIZZLE_BAD_VOLUME:
			//DispatchEffect( "PortalFizzleBadVolume", fxData );
			VectorAngles( vUp, vForward, fxData.m_vAngles );
			DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_2_badvolume" ) : ( "portal_1_badvolume" ) ), fxData.m_vOrigin, fxData.m_vAngles );
			ep.m_pSoundName = "Portal.fizzle_invalid_surface";
			break;

		case PORTAL_FIZZLE_BAD_SURFACE:
			//DispatchEffect( "PortalFizzleBadSurface", fxData );
			VectorAngles( vUp, vForward, fxData.m_vAngles );
			DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_2_badsurface" ) : ( "portal_1_badsurface" ) ), fxData.m_vOrigin, fxData.m_vAngles );
			ep.m_pSoundName = "Portal.fizzle_invalid_surface";
			break;

		case PORTAL_FIZZLE_KILLED:
			//DispatchEffect( "PortalFizzleKilled", fxData );
			VectorAngles( vUp, vForward, fxData.m_vAngles );
			DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_2_close" ) : ( "portal_1_close" ) ), fxData.m_vOrigin, fxData.m_vAngles );
			ep.m_pSoundName = "Portal.fizzle_moved";
			break;

		case PORTAL_FIZZLE_CLEANSER:
			//DispatchEffect( "PortalFizzleCleanser", fxData );
			VectorAngles( vUp, vForward, fxData.m_vAngles );
			DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_2_cleanser" ) : ( "portal_1_cleanser" ) ), fxData.m_vOrigin, fxData.m_vAngles, this );
			ep.m_pSoundName = "Portal.fizzle_invalid_surface";
			break;

		case PORTAL_FIZZLE_CLOSE:
			//DispatchEffect( "PortalFizzleKilled", fxData );
			VectorAngles( vUp, vForward, fxData.m_vAngles );
			DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_2_close" ) : ( "portal_1_close" ) ), fxData.m_vOrigin, fxData.m_vAngles );
			ep.m_pSoundName = ( ( m_bIsPortal2 ) ? ( "Portal.close_red" ) : ( "Portal.close_blue" ) );
			break;

		case PORTAL_FIZZLE_NEAR_BLUE:
		{
			if ( !m_bIsPortal2 )
			{
				Vector vLinkedForward;
				m_hLinkedPortal->GetVectors( &vLinkedForward, NULL, NULL );
				fxData.m_vOrigin = m_hLinkedPortal->GetAbsOrigin() + vLinkedForward * 16.0f;
				fxData.m_vAngles = m_hLinkedPortal->GetAbsAngles();
			}
			else
			{
				GetVectors( &vForward, NULL, NULL );
				fxData.m_vOrigin = GetAbsOrigin() + vForward * 16.0f;
				fxData.m_vAngles = GetAbsAngles();
			}

			//DispatchEffect( "PortalFizzleNear", fxData );
			AngleVectors( fxData.m_vAngles, &vForward, &vUp, NULL );
			VectorAngles( vUp, vForward, fxData.m_vAngles );
			DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_2_near" ) : ( "portal_1_near" ) ), fxData.m_vOrigin, fxData.m_vAngles );
			ep.m_pSoundName = "Portal.fizzle_invalid_surface";
			break;
		}

		case PORTAL_FIZZLE_NEAR_RED:
		{
			if ( m_bIsPortal2 )
			{
				Vector vLinkedForward;
				m_hLinkedPortal->GetVectors( &vLinkedForward, NULL, NULL );
				fxData.m_vOrigin = m_hLinkedPortal->GetAbsOrigin() + vLinkedForward * 16.0f;
				fxData.m_vAngles = m_hLinkedPortal->GetAbsAngles();
			}
			else
			{
				GetVectors( &vForward, NULL, NULL );
				fxData.m_vOrigin = GetAbsOrigin() + vForward * 16.0f;
				fxData.m_vAngles = GetAbsAngles();
			}

			//DispatchEffect( "PortalFizzleNear", fxData );
			AngleVectors( fxData.m_vAngles, &vForward, &vUp, NULL );
			VectorAngles( vUp, vForward, fxData.m_vAngles );
			DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_2_near" ) : ( "portal_1_near" ) ), fxData.m_vOrigin, fxData.m_vAngles );
			ep.m_pSoundName = "Portal.fizzle_invalid_surface";
			break;
		}

		case PORTAL_FIZZLE_SUCCESS:
			VectorAngles( vUp, vForward, fxData.m_vAngles );
			DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_2_success" ) : ( "portal_1_success" ) ), fxData.m_vOrigin, fxData.m_vAngles );
			// Don't make a sound!
			return;

		case PORTAL_FIZZLE_NONE:
			// Don't do anything!
			return;
	}

	EmitSound( filter, SOUND_FROM_WORLD, ep );
}

//-----------------------------------------------------------------------------
// Purpose: Fizzle the portal
//-----------------------------------------------------------------------------
void CProp_Portal::FizzleThink( void )
{
	CProp_Portal *pRemotePortal = m_hLinkedPortal;

	RemovePortalMicAndSpeaker();

	if ( m_pAmbientSound )
	{
		CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();

		controller.SoundChangeVolume( m_pAmbientSound, 0.0, 0.0 );
	}

	StopParticleEffects( this );

	m_bActivated = false;
	m_hLinkedPortal = NULL;
	m_PortalSimulator.DetachFromLinked();
	m_PortalSimulator.ReleaseAllEntityOwnership();

	if( pRemotePortal )
	{
		//pRemotePortal->m_hLinkedPortal = NULL;
		pRemotePortal->UpdatePortalLinkage();
	}

	SetContextThink( NULL, TICK_NEVER_THINK, s_pFizzleThink );
}


//-----------------------------------------------------------------------------
// Purpose: Portal will fizzle next time we get to think
//-----------------------------------------------------------------------------
void CProp_Portal::Fizzle( void )
{
	SetContextThink( &CProp_Portal::FizzleThink, gpGlobals->curtime, s_pFizzleThink );
}

//-----------------------------------------------------------------------------
// Purpose: Removes the portal microphone and speakers. This is done in two places
//			(fizzle and UpdateOnRemove) so the code is consolidated here.
// Input  :  - 
//-----------------------------------------------------------------------------
void CProp_Portal::RemovePortalMicAndSpeaker()
{

	// Shut down microphone/speaker if they exist
	if ( m_hMicrophone )
	{
		CEnvMicrophone *pMicrophone = (CEnvMicrophone*)(m_hMicrophone.Get());
		if ( pMicrophone )
		{
			inputdata_t in;
			pMicrophone->InputDisable( in );
			UTIL_Remove( pMicrophone );
		}
		m_hMicrophone = 0;
	}

	if ( m_hSpeaker )
	{
		CSpeaker *pSpeaker = (CSpeaker *)(m_hSpeaker.Get());
		if ( pSpeaker )
		{
			// Remove the remote portal's microphone, as it references the speaker we're about to remove.
			if ( m_hLinkedPortal.Get() )
			{
				CProp_Portal* pRemotePortal =  m_hLinkedPortal.Get();
				if ( pRemotePortal->m_hMicrophone )
				{
					inputdata_t inputdata;
					inputdata.pActivator = this;
					inputdata.pCaller = this;
					CEnvMicrophone* pRemotePortalMic = dynamic_cast<CEnvMicrophone*>(pRemotePortal->m_hMicrophone.Get());
					if ( pRemotePortalMic )
					{
						pRemotePortalMic->Remove();
					}
				}
			}
			inputdata_t in;
			pSpeaker->InputTurnOff( in );
			UTIL_Remove( pSpeaker );
		}
		m_hSpeaker = 0;
	}
}

void CProp_Portal::PunchPenetratingPlayer( CBaseEntity *pPlayer )
{
	if( m_PortalSimulator.IsReadyToSimulate() )
	{
		ICollideable *pCollideable = pPlayer->GetCollideable();
		if ( pCollideable )
		{
			Vector vMin, vMax;

			pCollideable->WorldSpaceSurroundingBounds( &vMin, &vMax );

			if ( UTIL_IsBoxIntersectingPortal( ( vMin + vMax ) / 2.0f, ( vMax - vMin ) / 2.0f, this ) )
			{
				Vector vForward;
				GetVectors( &vForward, 0, 0 );
				vForward *= 100.0f;
				pPlayer->VelocityPunch( vForward );
			}
		}
	}
}

void CProp_Portal::PunchAllPenetratingPlayers( void )
{
	for( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
		if( pPlayer )
			PunchPenetratingPlayer( pPlayer );
	}
}

void CProp_Portal::Activate( void )
{
	if( s_PortalLinkageGroups[m_iLinkageGroupID].Find( this ) == -1 )
		s_PortalLinkageGroups[m_iLinkageGroupID].AddToTail( this );

	if( m_pAttachedCloningArea == NULL )
		m_pAttachedCloningArea = CPhysicsCloneArea::CreatePhysicsCloneArea( this );

	UpdatePortalTeleportMatrix();
	
	UpdatePortalLinkage();

	BaseClass::Activate();

	CreateSounds();

	AddEffects( EF_NOSHADOW | EF_NORECEIVESHADOW );

	if( m_bActivated && (m_hLinkedPortal.Get() != NULL) )
	{
		Vector ptCenter = GetAbsOrigin();
		QAngle qAngles = GetAbsAngles();
		m_PortalSimulator.MoveTo( ptCenter, qAngles );

		//resimulate everything we're touching
		touchlink_t *root = ( touchlink_t * )GetDataObject( TOUCHLINK );
		if( root )
		{
			for( touchlink_t *link = root->nextLink; link != root; link = link->nextLink )
			{
				CBaseEntity *pOther = link->entityTouched;
				if( CProp_Portal_Shared::IsEntityTeleportable( pOther ) )
				{
					CCollisionProperty *pOtherCollision = pOther->CollisionProp();
					Vector vWorldMins, vWorldMaxs;
					pOtherCollision->WorldSpaceAABB( &vWorldMins, &vWorldMaxs );
					Vector ptOtherCenter = (vWorldMins + vWorldMaxs) / 2.0f;

					if( m_plane_Origin.normal.Dot( ptOtherCenter ) > m_plane_Origin.dist )
					{
						//we should be interacting with this object, add it to our environment
						if( SharedEnvironmentCheck( pOther ) )
						{
							Assert( ((m_PortalSimulator.GetLinkedPortalSimulator() == NULL) && (m_hLinkedPortal.Get() == NULL)) || 
								(m_PortalSimulator.GetLinkedPortalSimulator() == &m_hLinkedPortal->m_PortalSimulator) ); //make sure this entity is linked to the same portal as our simulator

							CPortalSimulator *pOwningSimulator = CPortalSimulator::GetSimulatorThatOwnsEntity( pOther );
							if( pOwningSimulator && (pOwningSimulator != &m_PortalSimulator) )
								pOwningSimulator->ReleaseOwnershipOfEntity( pOther );

							m_PortalSimulator.TakeOwnershipOfEntity( pOther );
						}
					}
				}
			}
		}
	}
}

bool CProp_Portal::ShouldTeleportTouchingEntity( CBaseEntity *pOther )
{
	if( !m_PortalSimulator.OwnsEntity( pOther ) ) //can't teleport an entity we don't own
	{
#if !defined ( DISABLE_DEBUG_HISTORY )
		if ( !IsMarkedForDeletion() )
		{
			ADD_DEBUG_HISTORY( HISTORY_PLAYER_DAMAGE, UTIL_VarArgs( "Portal %i not teleporting %s because it's not simulated by this portal.\n", ((m_bIsPortal2)?(2):(1)), pOther->GetDebugName() ) );
		}
#endif
		if ( sv_portal_debug_touch.GetBool() )
		{
			Msg( "Portal %i not teleporting %s because it's not simulated by this portal. : %f \n", ((m_bIsPortal2)?(2):(1)), pOther->GetDebugName(), gpGlobals->curtime );
		}
		return false;
	}

	if( !CProp_Portal_Shared::IsEntityTeleportable( pOther ) )
		return false;
	
	if( m_hLinkedPortal.Get() == NULL )
	{
#if !defined ( DISABLE_DEBUG_HISTORY )
		if ( !IsMarkedForDeletion() )
		{
			ADD_DEBUG_HISTORY( HISTORY_PLAYER_DAMAGE, UTIL_VarArgs( "Portal %i not teleporting %s because it has no linked partner portal.\n", ((m_bIsPortal2)?(2):(1)), pOther->GetDebugName() ) );
		}
#endif
		if ( sv_portal_debug_touch.GetBool() )
		{
			Msg( "Portal %i not teleporting %s because it has no linked partner portal.\n", ((m_bIsPortal2)?(2):(1)), pOther->GetDebugName() );
		}
		return false;
	}

	//Vector ptOtherOrigin = pOther->GetAbsOrigin();
	Vector ptOtherCenter = pOther->WorldSpaceCenter();

	IPhysicsObject *pOtherPhysObject = pOther->VPhysicsGetObject();

	Vector vOtherVelocity;
	//grab current velocity
	{
		if( sv_portal_new_velocity_check.GetBool() )
		{
			//we're assuming that physics velocity is the most reliable of all if the convar is true
			if( pOtherPhysObject )
			{
				//pOtherPhysObject->GetImplicitVelocity( &vOtherVelocity, NULL );
				pOtherPhysObject->GetVelocity( &vOtherVelocity, NULL );

				if( vOtherVelocity == vec3_origin )
				{
					pOther->GetVelocity( &vOtherVelocity );
				}
			}
			else
			{
				pOther->GetVelocity( &vOtherVelocity );
			}
		}
		else
		{
			//old style of velocity grabbing, which uses implicit velocity as a last resort
			if( pOther->GetMoveType() == MOVETYPE_VPHYSICS )
			{
				if( pOtherPhysObject && (pOtherPhysObject->GetShadowController() == NULL) )
					pOtherPhysObject->GetVelocity( &vOtherVelocity, NULL );
				else
					pOther->GetVelocity( &vOtherVelocity );
			}
			else
			{
				pOther->GetVelocity( &vOtherVelocity );
			}

			if( vOtherVelocity == vec3_origin )
			{
				// Recorded velocity is sometimes zero under pushed or teleported movement, or after position correction.
				// In these circumstances, we want implicit velocity ((last pos - this pos) / timestep )
				if ( pOtherPhysObject )
				{
					Vector vOtherImplicitVelocity;
					pOtherPhysObject->GetImplicitVelocity( &vOtherImplicitVelocity, NULL );
					vOtherVelocity += vOtherImplicitVelocity;
				}
			}
		}
	}

	// Test for entity's center being past portal plane
	if(m_PortalSimulator.m_DataAccess.Placement.PortalPlane.m_Normal.Dot( ptOtherCenter ) < m_PortalSimulator.m_DataAccess.Placement.PortalPlane.m_Dist)
	{
		//entity wants to go further into the plane
		if( m_PortalSimulator.EntityIsInPortalHole( pOther ) )
		{
#ifdef _DEBUG
			static int iAntiRecurse = 0;
			if( pOther->IsPlayer() && (iAntiRecurse == 0) )
			{
				++iAntiRecurse;
				ShouldTeleportTouchingEntity( pOther ); //do it again for debugging
				--iAntiRecurse;
			}
#endif
			return true;
		}
		else
		{
#if !defined ( DISABLE_DEBUG_HISTORY )
			if ( !IsMarkedForDeletion() )
			{
				ADD_DEBUG_HISTORY( HISTORY_PLAYER_DAMAGE, UTIL_VarArgs( "Portal %i not teleporting %s because it was not in the portal hole.\n", ((m_bIsPortal2)?(2):(1)), pOther->GetDebugName() ) );
			}
#endif
			if ( sv_portal_debug_touch.GetBool() )
			{
				Msg( "Portal %i not teleporting %s because it was not in the portal hole.\n", ((m_bIsPortal2)?(2):(1)), pOther->GetDebugName() );
			}
		}
	}

	return false;
}

void CProp_Portal::TeleportTouchingEntity( CBaseEntity *pOther )
{
	if ( GetPortalCallQueue() )
	{
		GetPortalCallQueue()->QueueCall( this, &CProp_Portal::TeleportTouchingEntity, pOther );
		return;
	}

	Assert( m_hLinkedPortal.Get() != NULL );

	Vector ptOtherOrigin = pOther->GetAbsOrigin();
	Vector ptOtherCenter;

	bool bPlayer = pOther->IsPlayer();
	QAngle qPlayerEyeAngles;
	CPortal_Player *pOtherAsPlayer;

	
	if( bPlayer )
	{
		//NDebugOverlay::EntityBounds( pOther, 255, 0, 0, 128, 60.0f );
		pOtherAsPlayer = (CPortal_Player *)pOther;
		qPlayerEyeAngles = pOtherAsPlayer->pl.v_angle;
	}
	else
	{
		pOtherAsPlayer = NULL;
	}

	ptOtherCenter = pOther->WorldSpaceCenter();

	bool bNonPhysical = false; //special case handling for non-physical objects such as the energy ball and player

	
	
	QAngle qOtherAngles;
	Vector vOtherVelocity;

	//grab current velocity
	{
		IPhysicsObject *pOtherPhysObject = pOther->VPhysicsGetObject();
		if( pOther->GetMoveType() == MOVETYPE_VPHYSICS )
		{
			if( pOtherPhysObject && (pOtherPhysObject->GetShadowController() == NULL) )
				pOtherPhysObject->GetVelocity( &vOtherVelocity, NULL );
			else
				pOther->GetVelocity( &vOtherVelocity );
		}
		else if ( bPlayer && pOther->VPhysicsGetObject() )
		{
			pOther->VPhysicsGetObject()->GetVelocity( &vOtherVelocity, NULL );

			if ( vOtherVelocity == vec3_origin )
			{
				vOtherVelocity = pOther->GetAbsVelocity();
			}
		}
		else
		{
			pOther->GetVelocity( &vOtherVelocity );
		}

		if( vOtherVelocity == vec3_origin )
		{
			// Recorded velocity is sometimes zero under pushed or teleported movement, or after position correction.
			// In these circumstances, we want implicit velocity ((last pos - this pos) / timestep )
			if ( pOtherPhysObject )
			{
				Vector vOtherImplicitVelocity;
				pOtherPhysObject->GetImplicitVelocity( &vOtherImplicitVelocity, NULL );
				vOtherVelocity += vOtherImplicitVelocity;
			}
		}
	}

	const PS_InternalData_t &RemotePortalDataAccess = m_hLinkedPortal->m_PortalSimulator.m_DataAccess;
	const PS_InternalData_t &LocalPortalDataAccess = m_PortalSimulator.m_DataAccess;

	
	if( bPlayer )
	{
		qOtherAngles = pOtherAsPlayer->EyeAngles();
		pOtherAsPlayer->m_qPrePortalledViewAngles = qOtherAngles;
		pOtherAsPlayer->m_bFixEyeAnglesFromPortalling = true;
		pOtherAsPlayer->m_matLastPortalled = m_matrixThisToLinked;
		bNonPhysical = true;
		//if( (fabs( RemotePortalDataAccess.Placement.vForward.z ) + fabs( LocalPortalDataAccess.Placement.vForward.z )) > 0.7071f ) //some combination of floor/ceiling
		if( fabs( LocalPortalDataAccess.Placement.vForward.z ) > 0.0f )
		{
			//we may have to compensate for the fact that AABB's don't rotate ever
			
			float fAbsLocalZ = fabs( LocalPortalDataAccess.Placement.vForward.z );
			float fAbsRemoteZ = fabs( RemotePortalDataAccess.Placement.vForward.z );
			
			if( (fabs(fAbsLocalZ - 1.0f) < 0.01f) &&
				(fabs(fAbsRemoteZ - 1.0f) < 0.01f) )
				//(fabs( LocalPortalDataAccess.Placement.vForward.z + RemotePortalDataAccess.Placement.vForward.z ) < 0.01f) )
			{
				//portals are both aligned on the z axis, no need to shrink the player
				
			}
			else
			{
				//curl the player up into a little ball
				pOtherAsPlayer->SetGroundEntity( NULL );

				if( !pOtherAsPlayer->IsDucked() )
				{
					pOtherAsPlayer->ForceDuckThisFrame();
					pOtherAsPlayer->m_Local.m_bInDuckJump = true;

					if( LocalPortalDataAccess.Placement.vForward.z > 0.0f )
						ptOtherCenter.z -= 16.0f; //portal facing up, shrink downwards
					else
						ptOtherCenter.z += 16.0f; //portal facing down, shrink upwards
				}
			}			
		}
	}
	else
	{
		qOtherAngles = pOther->GetAbsAngles();
		bNonPhysical = FClassnameIs( pOther, "prop_energy_ball" );
	}

	
	Vector ptNewOrigin;
	QAngle qNewAngles;
	Vector vNewVelocity;
	//apply transforms to relevant variables (applied to the entity later)
	{
		if( bPlayer )
		{
			ptNewOrigin = m_matrixThisToLinked * ptOtherCenter;
			ptNewOrigin += ptOtherOrigin - ptOtherCenter;	
		}
		else
		{
			ptNewOrigin = m_matrixThisToLinked * ptOtherOrigin;
		}
		
		// Reorient object angles, originally we did a transformation on the angles, but that doesn't quite work right for gimbal lock cases
		qNewAngles = TransformAnglesToWorldSpace( qOtherAngles, m_matrixThisToLinked.As3x4() );

		qNewAngles.x = AngleNormalizePositive( qNewAngles.x );
		qNewAngles.y = AngleNormalizePositive( qNewAngles.y );
		qNewAngles.z = AngleNormalizePositive( qNewAngles.z );

		// Reorient the velocity		
		vNewVelocity = m_matrixThisToLinked.ApplyRotation( vOtherVelocity );
	}

	//help camera reorientation for the player
	if( bPlayer )
	{
		Vector vPlayerForward;
		AngleVectors( qOtherAngles, &vPlayerForward, NULL, NULL );

		float fPlayerForwardZ = vPlayerForward.z;
		vPlayerForward.z = 0.0f;

		float fForwardLength = vPlayerForward.Length();

		if ( fForwardLength > 0.0f )
		{
			VectorNormalize( vPlayerForward );
		}

		float fPlayerFaceDotPortalFace = LocalPortalDataAccess.Placement.vForward.Dot( vPlayerForward );
		float fPlayerFaceDotPortalUp = LocalPortalDataAccess.Placement.vUp.Dot( vPlayerForward );

		CBaseEntity *pHeldEntity = GetPlayerHeldEntity( pOtherAsPlayer );

		// Sometimes reorienting by pitch is more desirable than by roll depending on the portals' orientations and the relative player facing direction
		if ( pHeldEntity )	// never pitch reorient while holding an object
		{
			pOtherAsPlayer->m_bPitchReorientation = false;
		}
		else if ( LocalPortalDataAccess.Placement.vUp.z > 0.99f && // entering wall portal
				  ( fForwardLength == 0.0f ||			// facing strait up or down
				    fPlayerFaceDotPortalFace > 0.5f ||	// facing mostly away from portal
					fPlayerFaceDotPortalFace < -0.5f )	// facing mostly toward portal
				)
		{
			pOtherAsPlayer->m_bPitchReorientation = true;
		}
		else if ( ( LocalPortalDataAccess.Placement.vForward.z > 0.99f || LocalPortalDataAccess.Placement.vForward.z < -0.99f ) &&	// entering floor or ceiling portal
				  ( RemotePortalDataAccess.Placement.vForward.z > 0.99f || RemotePortalDataAccess.Placement.vForward.z < -0.99f ) && // exiting floor or ceiling portal 
				  (	fPlayerForwardZ < -0.5f || fPlayerForwardZ > 0.5f )		// facing mustly up or down
				)
		{
			pOtherAsPlayer->m_bPitchReorientation = true;
		}
		else if ( ( RemotePortalDataAccess.Placement.vForward.z > 0.75f && RemotePortalDataAccess.Placement.vForward.z <= 0.99f ) && // exiting wedge portal
				  ( fPlayerFaceDotPortalUp > 0.0f ) // facing toward the top of the portal
				)
		{
			pOtherAsPlayer->m_bPitchReorientation = true;
		}
		else
		{
			pOtherAsPlayer->m_bPitchReorientation = false;
		}
	}

	//velocity hacks
	{
		//minimum floor exit velocity if both portals are on the floor or the player is coming out of the floor
		if( RemotePortalDataAccess.Placement.vForward.z > 0.7071f )
		{
			if ( bPlayer )
			{
				if( vNewVelocity.z < MINIMUM_FLOOR_PORTAL_EXIT_VELOCITY_PLAYER ) 
					vNewVelocity.z = MINIMUM_FLOOR_PORTAL_EXIT_VELOCITY_PLAYER;
			}
			else
			{
				if( LocalPortalDataAccess.Placement.vForward.z > 0.7071f )
				{
					if( vNewVelocity.z < MINIMUM_FLOOR_TO_FLOOR_PORTAL_EXIT_VELOCITY ) 
						vNewVelocity.z = MINIMUM_FLOOR_TO_FLOOR_PORTAL_EXIT_VELOCITY;
				}
				else
				{
					if( vNewVelocity.z < MINIMUM_FLOOR_PORTAL_EXIT_VELOCITY )
						vNewVelocity.z = MINIMUM_FLOOR_PORTAL_EXIT_VELOCITY;
				}
			}
		}


		if ( vNewVelocity.LengthSqr() > (MAXIMUM_PORTAL_EXIT_VELOCITY * MAXIMUM_PORTAL_EXIT_VELOCITY)  )
			vNewVelocity *= (MAXIMUM_PORTAL_EXIT_VELOCITY / vNewVelocity.Length());
	}

	//untouch the portal(s), will force a touch on destination after the teleport
	{
		m_PortalSimulator.ReleaseOwnershipOfEntity( pOther, true );
		this->PhysicsNotifyOtherOfUntouch( this, pOther );
		pOther->PhysicsNotifyOtherOfUntouch( pOther, this );

		m_hLinkedPortal->m_PortalSimulator.TakeOwnershipOfEntity( pOther );

		//m_hLinkedPortal->PhysicsNotifyOtherOfUntouch( m_hLinkedPortal, pOther );
		//pOther->PhysicsNotifyOtherOfUntouch( pOther, m_hLinkedPortal );
	}

	if( sv_portal_debug_touch.GetBool() )
	{
		DevMsg( "PORTAL %i TELEPORTING: %s\n", ((m_bIsPortal2)?(2):(1)), pOther->GetClassname() );
	}
#if !defined ( DISABLE_DEBUG_HISTORY )
	if ( !IsMarkedForDeletion() )
	{
		ADD_DEBUG_HISTORY( HISTORY_PLAYER_DAMAGE, UTIL_VarArgs( "PORTAL %i TELEPORTING: %s\n", ((m_bIsPortal2)?(2):(1)), pOther->GetClassname() ) );
	}
#endif

	//do the actual teleportation
	{
		pOther->SetGroundEntity( NULL );

		if( bPlayer )
		{
			QAngle qTransformedEyeAngles = TransformAnglesToWorldSpace( qPlayerEyeAngles, m_matrixThisToLinked.As3x4() );
			qTransformedEyeAngles.x = AngleNormalizePositive( qTransformedEyeAngles.x );
			qTransformedEyeAngles.y = AngleNormalizePositive( qTransformedEyeAngles.y );
			qTransformedEyeAngles.z = AngleNormalizePositive( qTransformedEyeAngles.z );
			
			pOtherAsPlayer->pl.v_angle = qTransformedEyeAngles;
			pOtherAsPlayer->pl.fixangle = FIXANGLE_ABSOLUTE;
			pOtherAsPlayer->UpdateVPhysicsPosition( ptNewOrigin, vNewVelocity, 0.0f );
			pOtherAsPlayer->Teleport( &ptNewOrigin, &qNewAngles, &vNewVelocity );
			//pOtherAsPlayer->UnDuck();

			//pOtherAsPlayer->m_angEyeAngles = qTransformedEyeAngles;
			//pOtherAsPlayer->pl.v_angle = qTransformedEyeAngles;
			//pOtherAsPlayer->pl.fixangle = FIXANGLE_ABSOLUTE;
		}
		else
		{
			if( bNonPhysical )
			{
				pOther->Teleport( &ptNewOrigin, &qNewAngles, &vNewVelocity );
			}
			else
			{
				//doing velocity in two stages as a bug workaround, setting the velocity to anything other than 0 will screw up how objects rest on this entity in the future
				pOther->Teleport( &ptNewOrigin, &qNewAngles, &vec3_origin );
				pOther->ApplyAbsVelocityImpulse( vNewVelocity );
			}
		}
	}

	IPhysicsObject *pPhys = pOther->VPhysicsGetObject();
	if( (pPhys != NULL) && (pPhys->GetGameFlags() & FVPHYSICS_PLAYER_HELD) )
	{
		CPortal_Player *pHoldingPlayer = (CPortal_Player *)GetPlayerHoldingEntity( pOther );
		pHoldingPlayer->ToggleHeldObjectOnOppositeSideOfPortal();
		if ( pHoldingPlayer->IsHeldObjectOnOppositeSideOfPortal() )
			pHoldingPlayer->SetHeldObjectPortal( this );
		else
			pHoldingPlayer->SetHeldObjectPortal( NULL );
	}
	else if( bPlayer )
	{
		CBaseEntity *pHeldEntity = GetPlayerHeldEntity( pOtherAsPlayer );
		if( pHeldEntity )
		{
			pOtherAsPlayer->ToggleHeldObjectOnOppositeSideOfPortal();
			if( pOtherAsPlayer->IsHeldObjectOnOppositeSideOfPortal() )
			{
				pOtherAsPlayer->SetHeldObjectPortal( m_hLinkedPortal );
			}
			else
			{
				pOtherAsPlayer->SetHeldObjectPortal( NULL );

				//we need to make sure the held object and player don't interpenetrate when the player's shape changes
				Vector vTargetPosition;
				QAngle qTargetOrientation;
				UpdateGrabControllerTargetPosition( pOtherAsPlayer, &vTargetPosition, &qTargetOrientation );

				pHeldEntity->Teleport( &vTargetPosition, &qTargetOrientation, 0 );

				FindClosestPassableSpace( pHeldEntity, RemotePortalDataAccess.Placement.vForward );
			}
		}
		
		//we haven't found a good way of fixing the problem of "how do you reorient an AABB". So we just move the player so that they fit
		//m_hLinkedPortal->ForceEntityToFitInPortalWall( pOtherAsPlayer );
	}

	//force the entity to be touching the other portal right this millisecond
	{
		trace_t Trace;
		memset( &Trace, 0, sizeof(trace_t) );
		//UTIL_TraceEntity( pOther, ptNewOrigin, ptNewOrigin, MASK_SOLID, pOther, COLLISION_GROUP_NONE, &Trace ); //fires off some asserts, and we just need a dummy anyways

		pOther->PhysicsMarkEntitiesAsTouching( m_hLinkedPortal.Get(), Trace );
		m_hLinkedPortal.Get()->PhysicsMarkEntitiesAsTouching( pOther, Trace );
	}

	// Notify the entity that it's being teleported
	// Tell the teleported entity of the portal it has just arrived at
	notify_teleport_params_t paramsTeleport;
	paramsTeleport.prevOrigin		= ptOtherOrigin;
	paramsTeleport.prevAngles		= qOtherAngles;
	paramsTeleport.physicsRotate	= true;
	notify_system_event_params_t eventParams ( &paramsTeleport );
	pOther->NotifySystemEvent( this, NOTIFY_EVENT_TELEPORT, eventParams );

	//notify clients of the teleportation
	{
		CBroadcastRecipientFilter filter;
		filter.MakeReliable();
		UserMessageBegin( filter, "EntityPortalled" );
		WRITE_EHANDLE( this );
		WRITE_EHANDLE( pOther );
		WRITE_FLOAT( ptNewOrigin.x );
		WRITE_FLOAT( ptNewOrigin.y );
		WRITE_FLOAT( ptNewOrigin.z );
		WRITE_FLOAT( qNewAngles.x );
		WRITE_FLOAT( qNewAngles.y );
		WRITE_FLOAT( qNewAngles.z );
		MessageEnd();
	}

#ifdef _DEBUG
	{
		Vector ptTestCenter = pOther->WorldSpaceCenter();

		float fNewDist, fOldDist;
		fNewDist = RemotePortalDataAccess.Placement.PortalPlane.m_Normal.Dot( ptTestCenter ) - RemotePortalDataAccess.Placement.PortalPlane.m_Dist;
		fOldDist = LocalPortalDataAccess.Placement.PortalPlane.m_Normal.Dot( ptOtherCenter ) - LocalPortalDataAccess.Placement.PortalPlane.m_Dist;
		AssertMsg( fNewDist >= 0.0f, "Entity portalled behind the destination portal." );
	}
#endif


	pOther->NetworkProp()->NetworkStateForceUpdate();
	if( bPlayer )
		pOtherAsPlayer->pl.NetworkStateChanged();

	//if( bPlayer )
	//	NDebugOverlay::EntityBounds( pOther, 0, 255, 0, 128, 60.0f );

	Assert( (bPlayer == false) || (pOtherAsPlayer->m_hPortalEnvironment.Get() == m_hLinkedPortal.Get()) );
}


void CProp_Portal::Touch( CBaseEntity *pOther )
{
	BaseClass::Touch( pOther );
	pOther->Touch( this );

	// Don't do anything on touch if it's not active
	if( !m_bActivated || (m_hLinkedPortal.Get() == NULL) )
	{
		Assert( !m_PortalSimulator.OwnsEntity( pOther ) );
		Assert( !pOther->IsPlayer() || (((CPortal_Player *)pOther)->m_hPortalEnvironment.Get() != this) );
		
		//I'd really like to fix the root cause, but this will keep the game going
		m_PortalSimulator.ReleaseOwnershipOfEntity( pOther );
		return;
	}

	Assert( ((m_PortalSimulator.GetLinkedPortalSimulator() == NULL) && (m_hLinkedPortal.Get() == NULL)) || 
		(m_PortalSimulator.GetLinkedPortalSimulator() == &m_hLinkedPortal->m_PortalSimulator) ); //make sure this entity is linked to the same portal as our simulator

	// Fizzle portal with any moving brush
	Vector vVelocityCheck;
	AngularImpulse vAngularImpulseCheck;
	pOther->GetVelocity( &vVelocityCheck, &vAngularImpulseCheck );

	if( vVelocityCheck != vec3_origin || vAngularImpulseCheck != vec3_origin )
	{
		if ( modelinfo->GetModelType( pOther->GetModel() ) == mod_brush )
		{
			if ( !FClassnameIs( pOther, "func_physbox" ) && !FClassnameIs( pOther, "simple_physics_brush" ) )	// except CPhysBox
			{
				Vector vForward;
				GetVectors( &vForward, NULL, NULL );

				Vector vMin, vMax;
				pOther->GetCollideable()->WorldSpaceSurroundingBounds( &vMin, &vMax );

				if ( UTIL_IsBoxIntersectingPortal( ( vMin + vMax ) / 2.0f, ( vMax - vMin ) / 2.0f - Vector( 2.0f, 2.0f, 2.0f ), this, 0.0f ) )
				{
					DevMsg( "Moving brush intersected portal plane.\n" );

					DoFizzleEffect( PORTAL_FIZZLE_KILLED, false );
					Fizzle();
				}
				else
				{
					Vector vOrigin = GetAbsOrigin();

					trace_t tr;

					UTIL_TraceLine( vOrigin, vOrigin - vForward * PORTAL_HALF_DEPTH, MASK_SOLID_BRUSHONLY, NULL, COLLISION_GROUP_NONE, &tr );

					// Something went wrong
					if ( tr.fraction == 1.0f && !tr.startsolid )
					{
						DevMsg( "Surface removed from behind portal.\n" );

						DoFizzleEffect( PORTAL_FIZZLE_KILLED, false );
						Fizzle();
					}
					else if ( tr.m_pEnt && tr.m_pEnt->IsMoving() )
					{
						DevMsg( "Surface behind portal is moving.\n" );

						DoFizzleEffect( PORTAL_FIZZLE_KILLED, false );
						Fizzle();
					}
				}
			}
		}
	}
	
	if( m_hLinkedPortal == NULL )
		return;

	//see if we should even be interacting with this object, this is a bugfix where some objects get added to physics environments through walls
	if( !m_PortalSimulator.OwnsEntity( pOther ) )
	{
		//hmm, not in our environment, plane tests, sharing tests
		if( SharedEnvironmentCheck( pOther ) )
		{
			bool bObjectCenterInFrontOfPortal	= (m_plane_Origin.normal.Dot( pOther->WorldSpaceCenter() ) > m_plane_Origin.dist);
			bool bIsStuckPlayer					= ( pOther->IsPlayer() )? ( !UTIL_IsSpaceEmpty( pOther, pOther->WorldAlignMins(), pOther->WorldAlignMaxs() ) ) : ( false );

			if ( bIsStuckPlayer )
			{
				Assert ( !"Player stuck" );
				DevMsg( "Player in solid behind behind portal %i's plane, Adding to it's environment to run find closest passable space.\n", ((m_bIsPortal2)?(2):(1)) );
			}

			if ( bObjectCenterInFrontOfPortal || bIsStuckPlayer )
			{
				if( sv_portal_debug_touch.GetBool() )
				{
					DevMsg( "Portal %i took control of shared object: %s\n", ((m_bIsPortal2)?(2):(1)), pOther->GetClassname() );
				}
#if !defined ( DISABLE_DEBUG_HISTORY )
				if ( !IsMarkedForDeletion() )
				{
					ADD_DEBUG_HISTORY( HISTORY_PLAYER_DAMAGE, UTIL_VarArgs( "Portal %i took control of shared object: %s\n", ((m_bIsPortal2)?(2):(1)), pOther->GetClassname() ) );
				}
#endif

				//we should be interacting with this object, add it to our environment
				CPortalSimulator *pOwningSimulator = CPortalSimulator::GetSimulatorThatOwnsEntity( pOther );
				if( pOwningSimulator && (pOwningSimulator != &m_PortalSimulator) )
					pOwningSimulator->ReleaseOwnershipOfEntity( pOther );

				m_PortalSimulator.TakeOwnershipOfEntity( pOther );
			}
		}
		else
		{
			return; //we shouldn't interact with this object
		}
	}

	if( ShouldTeleportTouchingEntity( pOther ) )
		TeleportTouchingEntity( pOther );
}

void CProp_Portal::StartTouch( CBaseEntity *pOther )
{
	BaseClass::StartTouch( pOther );

	// Since prop_portal is a trigger it doesn't send back start touch, so I'm forcing it
	pOther->StartTouch( this );

	if( sv_portal_debug_touch.GetBool() )
	{
		DevMsg( "Portal %i StartTouch: %s : %f\n", ((m_bIsPortal2)?(2):(1)), pOther->GetClassname(), gpGlobals->curtime );
	}
#if !defined ( DISABLE_DEBUG_HISTORY )
	if ( !IsMarkedForDeletion() )
	{
		ADD_DEBUG_HISTORY( HISTORY_PLAYER_DAMAGE, UTIL_VarArgs( "Portal %i StartTouch: %s : %f\n", ((m_bIsPortal2)?(2):(1)), pOther->GetClassname(), gpGlobals->curtime ) );
	}
#endif

	if( (m_hLinkedPortal == NULL) || (m_bActivated == false) )
		return;

	if( CProp_Portal_Shared::IsEntityTeleportable( pOther ) )
	{
		CCollisionProperty *pOtherCollision = pOther->CollisionProp();
		Vector vWorldMins, vWorldMaxs;
		pOtherCollision->WorldSpaceAABB( &vWorldMins, &vWorldMaxs );
		Vector ptOtherCenter = (vWorldMins + vWorldMaxs) / 2.0f;

		if( m_plane_Origin.normal.Dot( ptOtherCenter ) > m_plane_Origin.dist )
		{
			//we should be interacting with this object, add it to our environment
			if( SharedEnvironmentCheck( pOther ) )
			{
				Assert( ((m_PortalSimulator.GetLinkedPortalSimulator() == NULL) && (m_hLinkedPortal.Get() == NULL)) || 
					(m_PortalSimulator.GetLinkedPortalSimulator() == &m_hLinkedPortal->m_PortalSimulator) ); //make sure this entity is linked to the same portal as our simulator

				CPortalSimulator *pOwningSimulator = CPortalSimulator::GetSimulatorThatOwnsEntity( pOther );
				if( pOwningSimulator && (pOwningSimulator != &m_PortalSimulator) )
					pOwningSimulator->ReleaseOwnershipOfEntity( pOther );

				m_PortalSimulator.TakeOwnershipOfEntity( pOther );
			}
		}
	}	
}

void CProp_Portal::EndTouch( CBaseEntity *pOther )
{
	BaseClass::EndTouch( pOther );

	// Since prop_portal is a trigger it doesn't send back end touch, so I'm forcing it
	pOther->EndTouch( this );

	// Don't do anything on end touch if it's not active
	if( !m_bActivated )
	{
		return;
	}

	if( ShouldTeleportTouchingEntity( pOther ) ) //an object passed through the plane and all the way out of the touch box
		TeleportTouchingEntity( pOther );
	else if( pOther->IsPlayer() && //player
			(m_PortalSimulator.m_DataAccess.Placement.vForward.z < -0.7071f) && //most likely falling out of the portal
			(m_PortalSimulator.m_DataAccess.Placement.PortalPlane.m_Normal.Dot( pOther->WorldSpaceCenter() ) < m_PortalSimulator.m_DataAccess.Placement.PortalPlane.m_Dist) && //but behind the portal plane
			(((CPortal_Player *)pOther)->m_Local.m_bInDuckJump) ) //while ducking
	{
		//player has pulled their feet up (moving their center instantaneously) while falling downward out of the portal, send them back (probably only for a frame)
		
		DevMsg( "Player pulled feet above the portal they fell out of, postponing Releasing ownership\n" );
		//TeleportTouchingEntity( pOther );
	}
	else
		m_PortalSimulator.ReleaseOwnershipOfEntity( pOther );

	if( sv_portal_debug_touch.GetBool() )
	{
		DevMsg( "Portal %i EndTouch: %s : %f\n", ((m_bIsPortal2)?(2):(1)), pOther->GetClassname(), gpGlobals->curtime );
	}

#if !defined( DISABLE_DEBUG_HISTORY )
	if ( !IsMarkedForDeletion() )
	{
		ADD_DEBUG_HISTORY( HISTORY_PLAYER_DAMAGE, UTIL_VarArgs( "Portal %i EndTouch: %s : %f\n", ((m_bIsPortal2)?(2):(1)), pOther->GetClassname(), gpGlobals->curtime ) );
	}
#endif
}

void CProp_Portal::PortalSimulator_TookOwnershipOfEntity( CBaseEntity *pEntity )
{
	if( pEntity->IsPlayer() )
		((CPortal_Player *)pEntity)->m_hPortalEnvironment = this;
}

void CProp_Portal::PortalSimulator_ReleasedOwnershipOfEntity( CBaseEntity *pEntity )
{
	if( pEntity->IsPlayer() && (((CPortal_Player *)pEntity)->m_hPortalEnvironment.Get() == this) )
		((CPortal_Player *)pEntity)->m_hPortalEnvironment = NULL;
}

bool CProp_Portal::SharedEnvironmentCheck( CBaseEntity *pEntity )
{
	Assert( ((m_PortalSimulator.GetLinkedPortalSimulator() == NULL) && (m_hLinkedPortal.Get() == NULL)) || 
		(m_PortalSimulator.GetLinkedPortalSimulator() == &m_hLinkedPortal->m_PortalSimulator) ); //make sure this entity is linked to the same portal as our simulator

	CPortalSimulator *pOwningSimulator = CPortalSimulator::GetSimulatorThatOwnsEntity( pEntity );
	if( (pOwningSimulator == NULL) || (pOwningSimulator == &m_PortalSimulator) )
	{
		//nobody else is claiming ownership
		return true;
	}

	Vector ptCenter = pEntity->WorldSpaceCenter();
	if( (ptCenter - m_PortalSimulator.m_DataAccess.Placement.ptCenter).LengthSqr() < (ptCenter - pOwningSimulator->m_DataAccess.Placement.ptCenter).LengthSqr() )
		return true;

	/*if( !m_hLinkedPortal->m_PortalSimulator.EntityIsInPortalHole( pEntity ) )
	{
		Vector vOtherVelocity;
		pEntity->GetVelocity( &vOtherVelocity );

		if( vOtherVelocity.Dot( m_PortalSimulator.m_DataAccess.Placement.vForward ) < vOtherVelocity.Dot( m_hLinkedPortal->m_PortalSimulator.m_DataAccess.Placement.vForward ) )
			return true; //entity is going towards this portal more than the other
	}*/
	return false;

	//we're in the shared configuration, and the other portal already owns the object, see if we'd be a better caretaker (distance check
	/*CCollisionProperty *pEntityCollision = pEntity->CollisionProp();
	Vector vWorldMins, vWorldMaxs;
	pEntityCollision->WorldSpaceAABB( &vWorldMins, &vWorldMaxs );
	Vector ptEntityCenter = (vWorldMins + vWorldMaxs) / 2.0f;

	Vector vEntToThis = GetAbsOrigin() - ptEntityCenter;
	Vector vEntToRemote = m_hLinkedPortal->GetAbsOrigin() - ptEntityCenter;

	return ( vEntToThis.LengthSqr() < vEntToRemote.LengthSqr() );*/
}

void CProp_Portal::WakeNearbyEntities( void )
{
	CBaseEntity*	pList[ 1024 ];

	Vector vForward, vUp, vRight;
	GetVectors( &vForward, &vRight, &vUp );

	Vector ptOrigin = GetAbsOrigin();
	QAngle qAngles = GetAbsAngles();

	Vector ptOBBStart = ptOrigin;
	ptOBBStart += vForward * CProp_Portal_Shared::vLocalMins.x;
	ptOBBStart += vRight * CProp_Portal_Shared::vLocalMins.y;
	ptOBBStart += vUp * CProp_Portal_Shared::vLocalMins.z;


	vForward *= CProp_Portal_Shared::vLocalMaxs.x - CProp_Portal_Shared::vLocalMins.x;
	vRight *= CProp_Portal_Shared::vLocalMaxs.y - CProp_Portal_Shared::vLocalMins.y;
	vUp *= CProp_Portal_Shared::vLocalMaxs.z - CProp_Portal_Shared::vLocalMins.z;


	Vector vAABBMins, vAABBMaxs;
	vAABBMins = vAABBMaxs = ptOBBStart;

	for( int i = 1; i != 8; ++i )
	{
		Vector ptTest = ptOBBStart;
		if( i & (1 << 0) ) ptTest += vForward;
		if( i & (1 << 1) ) ptTest += vRight;
		if( i & (1 << 2) ) ptTest += vUp;

		if( ptTest.x < vAABBMins.x ) vAABBMins.x = ptTest.x;
		if( ptTest.y < vAABBMins.y ) vAABBMins.y = ptTest.y;
		if( ptTest.z < vAABBMins.z ) vAABBMins.z = ptTest.z;
		if( ptTest.x > vAABBMaxs.x ) vAABBMaxs.x = ptTest.x;
		if( ptTest.y > vAABBMaxs.y ) vAABBMaxs.y = ptTest.y;
		if( ptTest.z > vAABBMaxs.z ) vAABBMaxs.z = ptTest.z;
	}
	
	int count = UTIL_EntitiesInBox( pList, 1024, vAABBMins, vAABBMaxs, 0 );

	//Iterate over all the possible targets
	for ( int i = 0; i < count; i++ )
	{
		CBaseEntity *pEntity = pList[i];

		if ( pEntity && (pEntity != this) )
		{
			CCollisionProperty *pEntCollision = pEntity->CollisionProp();
			Vector ptEntityCenter = pEntCollision->GetCollisionOrigin();

			//double check intersection at the OBB vs OBB level, we don't want to affect large piles of physics objects if we don't have to. It gets slow
			if( IsOBBIntersectingOBB( ptOrigin, qAngles, CProp_Portal_Shared::vLocalMins, CProp_Portal_Shared::vLocalMaxs, 
				ptEntityCenter, pEntCollision->GetCollisionAngles(), pEntCollision->OBBMins(), pEntCollision->OBBMaxs() ) )
			{
				if( FClassnameIs( pEntity, "func_portal_detector" ) )
				{
					// It's a portal detector
					CFuncPortalDetector *pPortalDetector = static_cast<CFuncPortalDetector*>( pEntity );

					if ( pPortalDetector->IsActive() && pPortalDetector->GetLinkageGroupID() == m_iLinkageGroupID )
					{
						// It's detecting this portal's group
						Vector vMin, vMax;
						pPortalDetector->CollisionProp()->WorldSpaceAABB( &vMin, &vMax );

						Vector vBoxCenter = ( vMin + vMax ) * 0.5f;
						Vector vBoxExtents = ( vMax - vMin ) * 0.5f;

						if ( UTIL_IsBoxIntersectingPortal( vBoxCenter, vBoxExtents, this ) )
						{
							// It's intersecting this portal
							if ( m_bIsPortal2 )
								pPortalDetector->m_OnStartTouchPortal2.FireOutput( this, pPortalDetector );
							else
								pPortalDetector->m_OnStartTouchPortal1.FireOutput( this, pPortalDetector );

							if ( IsActivedAndLinked() )
							{
								pPortalDetector->m_OnStartTouchLinkedPortal.FireOutput( this, pPortalDetector );

								if ( UTIL_IsBoxIntersectingPortal( vBoxCenter, vBoxExtents, m_hLinkedPortal ) )
								{
									pPortalDetector->m_OnStartTouchBothLinkedPortals.FireOutput( this, pPortalDetector );
								}
							}
						}
					}
				}

				pEntity->WakeRestingObjects();
				//pEntity->SetGroundEntity( NULL );

				if ( pEntity->GetMoveType() == MOVETYPE_VPHYSICS )
				{
					IPhysicsObject *pPhysicsObject = pEntity->VPhysicsGetObject();

					if ( pPhysicsObject && pPhysicsObject->IsMoveable() )
					{
						pPhysicsObject->Wake();

						// If the target is debris, convert it to non-debris
						if ( pEntity->GetCollisionGroup() == COLLISION_GROUP_DEBRIS )
						{
							// Interactive debris converts back to debris when it comes to rest
							pEntity->SetCollisionGroup( COLLISION_GROUP_INTERACTIVE_DEBRIS );
						}
					}
				}
			}
		}
	}
}

void CProp_Portal::ForceEntityToFitInPortalWall( CBaseEntity *pEntity )
{
	CCollisionProperty *pCollision = pEntity->CollisionProp();
	Vector vWorldMins, vWorldMaxs;
	pCollision->WorldSpaceAABB( &vWorldMins, &vWorldMaxs );
	Vector ptCenter = pEntity->WorldSpaceCenter(); //(vWorldMins + vWorldMaxs) / 2.0f;
	Vector ptOrigin = pEntity->GetAbsOrigin();
	Vector vEntityCenterToOrigin = ptOrigin - ptCenter;


	Vector ptPortalCenter = GetAbsOrigin();
	Vector vPortalCenterToEntityCenter = ptCenter - ptPortalCenter;
	Vector vPortalForward;
	GetVectors( &vPortalForward, NULL, NULL );
	Vector ptProjectedEntityCenter = ptPortalCenter + ( vPortalForward * vPortalCenterToEntityCenter.Dot( vPortalForward ) );

	Vector ptDest;

	if ( m_PortalSimulator.IsReadyToSimulate() )
	{
		Ray_t ray;
		ray.Init( ptProjectedEntityCenter, ptCenter, vWorldMins - ptCenter, vWorldMaxs - ptCenter );

		trace_t ShortestTrace;
		ShortestTrace.fraction = 2.0f;

		if( m_PortalSimulator.m_DataAccess.Simulation.Static.Wall.Local.Brushes.pCollideable )
		{
			physcollision->TraceBox( ray, m_PortalSimulator.m_DataAccess.Simulation.Static.Wall.Local.Brushes.pCollideable, vec3_origin, vec3_angle, &ShortestTrace );
		}

		/*if( pEnvironment->LocalCollide.pWorldCollide )
		{
			trace_t TempTrace;
			physcollision->TraceBox( ray, pEnvironment->LocalCollide.pWorldCollide, vec3_origin, vec3_angle, &TempTrace );
			if( TempTrace.fraction < ShortestTrace.fraction )
				ShortestTrace = TempTrace;
		}

		if( pEnvironment->LocalCollide.pWallShellCollide )
		{
			trace_t TempTrace;
			physcollision->TraceBox( ray, pEnvironment->LocalCollide.pWallShellCollide, vec3_origin, vec3_angle, &TempTrace );
			if( TempTrace.fraction < ShortestTrace.fraction )
				ShortestTrace = TempTrace;
		}

		if( pEnvironment->LocalCollide.pRemoteWorldWallCollide )
		{
			trace_t TempTrace;
			physcollision->TraceBox( ray, pEnvironment->LocalCollide.pRemoteWorldWallCollide, vec3_origin, vec3_angle, &TempTrace );
			if( TempTrace.fraction < ShortestTrace.fraction )
				ShortestTrace = TempTrace;
		}

		//Add displacement checks here too?

		*/

		if( ShortestTrace.fraction < 2.0f )
		{
			Vector ptNewPos = ShortestTrace.endpos + vEntityCenterToOrigin;
			pEntity->Teleport( &ptNewPos, NULL, NULL );
			pEntity->IncrementInterpolationFrame();
#if !defined ( DISABLE_DEBUG_HISTORY )
			if ( !IsMarkedForDeletion() )
			{
				ADD_DEBUG_HISTORY( HISTORY_PLAYER_DAMAGE, UTIL_VarArgs( "Teleporting %s inside 'ForceEntityToFitInPortalWall'\n", pEntity->GetDebugName() ) );
			}
#endif
			if( sv_portal_debug_touch.GetBool() )
			{
				DevMsg( "Teleporting %s inside 'ForceEntityToFitInPortalWall'\n", pEntity->GetDebugName() );
			}
			//pEntity->SetAbsOrigin( ShortestTrace.endpos + vEntityCenterToOrigin );
		}
	}	
}

void CProp_Portal::UpdatePortalTeleportMatrix( void )
{
	ResetModel();

	//setup our origin plane
	GetVectors( &m_plane_Origin.normal, NULL, NULL );
	m_plane_Origin.dist = m_plane_Origin.normal.Dot( GetAbsOrigin() );
	m_plane_Origin.signbits = SignbitsForPlane( &m_plane_Origin );

	Vector vAbsNormal;
	vAbsNormal.x = fabs(m_plane_Origin.normal.x);
	vAbsNormal.y = fabs(m_plane_Origin.normal.y);
	vAbsNormal.z = fabs(m_plane_Origin.normal.z);

	if( vAbsNormal.x > vAbsNormal.y )
	{
		if( vAbsNormal.x > vAbsNormal.z )
		{
			if( vAbsNormal.x > 0.999f )
				m_plane_Origin.type = PLANE_X;
			else
				m_plane_Origin.type = PLANE_ANYX;
		}
		else
		{
			if( vAbsNormal.z > 0.999f )
				m_plane_Origin.type = PLANE_Z;
			else
				m_plane_Origin.type = PLANE_ANYZ;
		}
	}
	else
	{
		if( vAbsNormal.y > vAbsNormal.z )
		{
			if( vAbsNormal.y > 0.999f )
				m_plane_Origin.type = PLANE_Y;
			else
				m_plane_Origin.type = PLANE_ANYY;
		}
		else
		{
			if( vAbsNormal.z > 0.999f )
				m_plane_Origin.type = PLANE_Z;
			else
				m_plane_Origin.type = PLANE_ANYZ;
		}
	}



	if ( m_hLinkedPortal != NULL )
	{
		CProp_Portal_Shared::UpdatePortalTransformationMatrix( EntityToWorldTransform(), m_hLinkedPortal->EntityToWorldTransform(), &m_matrixThisToLinked );

		m_hLinkedPortal->ResetModel();
		//update the remote portal
		MatrixInverseTR( m_matrixThisToLinked, m_hLinkedPortal->m_matrixThisToLinked );
	}
	else
	{
		m_matrixThisToLinked.Identity(); //don't accidentally teleport objects to zero space
	}
}

void CProp_Portal::UpdatePortalLinkage( void )
{
	if( m_bActivated )
	{
		CProp_Portal *pLink = m_hLinkedPortal.Get();

		if( !(pLink && pLink->m_bActivated) )
		{
			//no old link, or inactive old link

			if( pLink )
			{
				//we had an old link, must be inactive
				if( pLink->m_hLinkedPortal.Get() != NULL )
					pLink->UpdatePortalLinkage();

				pLink = NULL;
			}

			int iPortalCount = s_PortalLinkageGroups[m_iLinkageGroupID].Count();

			if( iPortalCount != 0 )
			{
				CProp_Portal **pPortals = s_PortalLinkageGroups[m_iLinkageGroupID].Base();
				for( int i = 0; i != iPortalCount; ++i )
				{
					CProp_Portal *pCurrentPortal = pPortals[i];
					if( pCurrentPortal == this )
						continue;
					if( pCurrentPortal->m_bActivated && pCurrentPortal->m_hLinkedPortal.Get() == NULL )
					{
						pLink = pCurrentPortal;
						pCurrentPortal->m_hLinkedPortal = this;
						pCurrentPortal->UpdatePortalLinkage();
						break;
					}
				}
			}
		}

		m_hLinkedPortal = pLink;


		if( pLink != NULL )
		{
			CHandle<CProp_Portal> hThis = this;
			CHandle<CProp_Portal> hRemote = pLink;

			this->m_hLinkedPortal = hRemote;
			pLink->m_hLinkedPortal = hThis;
			m_bIsPortal2 = !m_hLinkedPortal->m_bIsPortal2;

			// Initialize mics/speakers
			if( m_hMicrophone == 0 )
			{
				inputdata_t inputdata;

				m_hMicrophone = CreateEntityByName( "env_microphone" );
				CEnvMicrophone *pMicrophone = static_cast<CEnvMicrophone*>( m_hMicrophone.Get() );
				pMicrophone->AddSpawnFlags( SF_MICROPHONE_IGNORE_NONATTENUATED );
				pMicrophone->AddSpawnFlags( SF_MICROPHONE_SOUND_COMBAT | SF_MICROPHONE_SOUND_WORLD | SF_MICROPHONE_SOUND_PLAYER | SF_MICROPHONE_SOUND_BULLET_IMPACT | SF_MICROPHONE_SOUND_EXPLOSION );
				DispatchSpawn( pMicrophone );

				m_hSpeaker = CreateEntityByName( "env_speaker" );
				CSpeaker *pSpeaker = static_cast<CSpeaker*>( m_hSpeaker.Get() );

				if( !m_bIsPortal2 )
				{
					pSpeaker->SetName( MAKE_STRING( "PortalSpeaker1" ) );
					pMicrophone->SetName( MAKE_STRING( "PortalMic1" ) );
					pMicrophone->Activate();
					pMicrophone->SetSpeakerName( MAKE_STRING( "PortalSpeaker2" ) );
					pMicrophone->SetSensitivity( 10.0f );
				}
				else
				{
					pSpeaker->SetName( MAKE_STRING( "PortalSpeaker2" ) );
					pMicrophone->SetName( MAKE_STRING( "PortalMic2" ) );
					pMicrophone->Activate();
					pMicrophone->SetSpeakerName( MAKE_STRING( "PortalSpeaker1" ) );
					pMicrophone->SetSensitivity( 10.0f );
				}
			}

			if ( m_hLinkedPortal->m_hMicrophone == 0 )
			{
				inputdata_t inputdata;

				m_hLinkedPortal->m_hMicrophone = CreateEntityByName( "env_microphone" );
				CEnvMicrophone *pLinkedMicrophone = static_cast<CEnvMicrophone*>( m_hLinkedPortal->m_hMicrophone.Get() );
				pLinkedMicrophone->AddSpawnFlags( SF_MICROPHONE_IGNORE_NONATTENUATED );
				pLinkedMicrophone->AddSpawnFlags( SF_MICROPHONE_SOUND_COMBAT | SF_MICROPHONE_SOUND_WORLD | SF_MICROPHONE_SOUND_PLAYER | SF_MICROPHONE_SOUND_BULLET_IMPACT | SF_MICROPHONE_SOUND_EXPLOSION );
				DispatchSpawn( pLinkedMicrophone );

				m_hLinkedPortal->m_hSpeaker = CreateEntityByName( "env_speaker" );
				CSpeaker *pLinkedSpeaker = static_cast<CSpeaker*>( m_hLinkedPortal->m_hSpeaker.Get() );

				if ( !m_bIsPortal2 )
				{
					pLinkedSpeaker->SetName( MAKE_STRING( "PortalSpeaker2" ) );
					pLinkedMicrophone->SetName( MAKE_STRING( "PortalMic2" ) );
					pLinkedMicrophone->Activate();
					pLinkedMicrophone->SetSpeakerName( MAKE_STRING( "PortalSpeaker1" ) );
					pLinkedMicrophone->SetSensitivity( 10.0f );
				}
				else
				{
					pLinkedSpeaker->SetName( MAKE_STRING( "PortalSpeaker1" ) );
					pLinkedMicrophone->SetName( MAKE_STRING( "PortalMic1" ) );
					pLinkedMicrophone->Activate();
					pLinkedMicrophone->SetSpeakerName( MAKE_STRING( "PortalSpeaker2" ) );
					pLinkedMicrophone->SetSensitivity( 10.0f );
				}
			}

			// Set microphone/speaker positions
			Vector vZero( 0.0f, 0.0f, 0.0f );

			CEnvMicrophone *pMicrophone = static_cast<CEnvMicrophone*>( m_hMicrophone.Get() );
			pMicrophone->AddSpawnFlags( SF_MICROPHONE_IGNORE_NONATTENUATED );
			pMicrophone->Teleport( &GetAbsOrigin(), &GetAbsAngles(), &vZero );
			inputdata_t in;
			pMicrophone->InputEnable( in );

			CSpeaker *pSpeaker = static_cast<CSpeaker*>( m_hSpeaker.Get() );
			pSpeaker->Teleport( &GetAbsOrigin(), &GetAbsAngles(), &vZero );
			pSpeaker->InputTurnOn( in );

			UpdatePortalTeleportMatrix();
		}
		else
		{
			m_PortalSimulator.DetachFromLinked();
			m_PortalSimulator.ReleaseAllEntityOwnership();
		}

		Vector ptCenter = GetAbsOrigin();
		QAngle qAngles = GetAbsAngles();
		m_PortalSimulator.MoveTo( ptCenter, qAngles );

		if( pLink )
			m_PortalSimulator.AttachTo( &pLink->m_PortalSimulator );

		if( m_pAttachedCloningArea )
			m_pAttachedCloningArea->UpdatePosition();
	}
	else
	{
		CProp_Portal *pRemote = m_hLinkedPortal;
		//apparently we've been deactivated
		m_PortalSimulator.DetachFromLinked();
		m_PortalSimulator.ReleaseAllEntityOwnership();

		m_hLinkedPortal = NULL;
		if( pRemote )
			pRemote->UpdatePortalLinkage();
	}
}

void CProp_Portal::PlacePortal( const Vector &vOrigin, const QAngle &qAngles, float fPlacementSuccess, bool bDelay /*= false*/ )
{
	Vector vOldOrigin = GetLocalOrigin();
	QAngle qOldAngles = GetLocalAngles();

	Vector vNewOrigin = vOrigin;
	QAngle qNewAngles = qAngles;

	UTIL_TestForOrientationVolumes( qNewAngles, vNewOrigin, this );

	if ( sv_portal_placement_never_fail.GetBool() )
	{
		fPlacementSuccess = PORTAL_FIZZLE_SUCCESS;
	}

	if ( fPlacementSuccess < 0.5f )
	{
		// Prepare fizzle
		m_vDelayedPosition = vOrigin;
		m_qDelayedAngles = qAngles;

		if ( fPlacementSuccess == PORTAL_ANALOG_SUCCESS_CANT_FIT )
			m_iDelayedFailure = PORTAL_FIZZLE_CANT_FIT;
		else if ( fPlacementSuccess == PORTAL_ANALOG_SUCCESS_OVERLAP_LINKED )
			m_iDelayedFailure = PORTAL_FIZZLE_OVERLAPPED_LINKED;
		else if ( fPlacementSuccess == PORTAL_ANALOG_SUCCESS_INVALID_VOLUME )
			m_iDelayedFailure = PORTAL_FIZZLE_BAD_VOLUME;
		else if ( fPlacementSuccess == PORTAL_ANALOG_SUCCESS_INVALID_SURFACE )
			m_iDelayedFailure = PORTAL_FIZZLE_BAD_SURFACE;
		else if ( fPlacementSuccess == PORTAL_ANALOG_SUCCESS_PASSTHROUGH_SURFACE )
			m_iDelayedFailure = PORTAL_FIZZLE_NONE;

		CWeaponPortalgun *pPortalGun = dynamic_cast<CWeaponPortalgun*>( m_hPlacedBy.Get() );

		if( pPortalGun )
		{
			CPortal_Player *pFiringPlayer = dynamic_cast<CPortal_Player *>( pPortalGun->GetOwner() );
			if( pFiringPlayer )
			{
				g_PortalGameStats.Event_PortalPlacement( pFiringPlayer->GetAbsOrigin(), vOrigin, m_iDelayedFailure );
			}
		}

		return;
	}

	if ( !bDelay )
	{
		m_vDelayedPosition = vNewOrigin;
		m_qDelayedAngles = qNewAngles;
		m_iDelayedFailure = PORTAL_FIZZLE_SUCCESS;

		NewLocation( vNewOrigin, qNewAngles );
	}
	else
	{
		m_vDelayedPosition = vNewOrigin;
		m_qDelayedAngles = qNewAngles;
		m_iDelayedFailure = PORTAL_FIZZLE_SUCCESS;
	}

	CWeaponPortalgun *pPortalGun = dynamic_cast<CWeaponPortalgun*>( m_hPlacedBy.Get() );

	if( pPortalGun )
	{
		CPortal_Player *pFiringPlayer = dynamic_cast<CPortal_Player *>( pPortalGun->GetOwner() );
		if( pFiringPlayer )
		{
			g_PortalGameStats.Event_PortalPlacement( pFiringPlayer->GetAbsOrigin(), vOrigin, m_iDelayedFailure );
		}
	}	
}

void CProp_Portal::NewLocation( const Vector &vOrigin, const QAngle &qAngles )
{
	// Tell our physics environment to stop simulating it's entities.
	// Fast moving objects can pass through the hole this frame while it's in the old location.
	m_PortalSimulator.ReleaseAllEntityOwnership();
	Vector vOldForward;
	GetVectors( &vOldForward, 0, 0 );

	m_vPrevForward = vOldForward;

	WakeNearbyEntities();

	Teleport( &vOrigin, &qAngles, 0 );

	if ( m_hMicrophone )
	{
		CEnvMicrophone *pMicrophone = static_cast<CEnvMicrophone*>( m_hMicrophone.Get() );
		pMicrophone->Teleport( &vOrigin, &qAngles, 0 );
		inputdata_t in;
		pMicrophone->InputEnable( in );
	}

	if ( m_hSpeaker )
	{
		CSpeaker *pSpeaker = static_cast<CSpeaker*>( m_hSpeaker.Get() );
		pSpeaker->Teleport( &vOrigin, &qAngles, 0 );
		inputdata_t in;
		pSpeaker->InputTurnOn( in );
	}

	CreateSounds();

	if ( m_pAmbientSound )
	{
		CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();
		controller.SoundChangeVolume( m_pAmbientSound, 0.4, 0.1 );
	}

	DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_2_particles" ) : ( "portal_1_particles" ) ), PATTACH_POINT_FOLLOW, this, "particles_2", true );
	DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_2_edge" ) : ( "portal_1_edge" ) ), PATTACH_POINT_FOLLOW, this, "particlespin" );

	//if the other portal should be static, let's not punch stuff resting on it
	bool bOtherShouldBeStatic = false;
	if( !m_hLinkedPortal )
		bOtherShouldBeStatic = true;

	m_bActivated = true;

	UpdatePortalLinkage();
	UpdatePortalTeleportMatrix();

	// Update the four corners of this portal for faster reference
	UpdateCorners();

	WakeNearbyEntities();

	if ( m_hLinkedPortal )
	{
		m_hLinkedPortal->WakeNearbyEntities();
		if( !bOtherShouldBeStatic ) 
		{
			m_hLinkedPortal->PunchAllPenetratingPlayers();
		}
	}

	if ( m_bIsPortal2 )
	{
		EmitSound( "Portal.open_red" );
	}
	else
	{
		EmitSound( "Portal.open_blue" );
	}
}

void CProp_Portal::InputSetActivatedState( inputdata_t &inputdata )
{
	m_bActivated = inputdata.value.Bool();
	m_hPlacedBy = NULL;

	if ( m_bActivated )
	{
		Vector vOrigin;
		vOrigin = GetAbsOrigin();

		Vector vForward, vUp;
		GetVectors( &vForward, 0, &vUp );

		CTraceFilterSimpleClassnameList baseFilter( this, COLLISION_GROUP_NONE );
		UTIL_Portal_Trace_Filter( &baseFilter );
		CTraceFilterTranslateClones traceFilterPortalShot( &baseFilter );

		trace_t tr;
		UTIL_TraceLine( vOrigin + vForward, vOrigin + vForward * -8.0f, MASK_SHOT_PORTAL, &traceFilterPortalShot, &tr );

		QAngle qAngles;
		VectorAngles( tr.plane.normal, vUp, qAngles );

		float fPlacementSuccess = VerifyPortalPlacement( this, tr.endpos, qAngles, PORTAL_PLACED_BY_FIXED );
		PlacePortal( tr.endpos, qAngles, fPlacementSuccess );

		// If the fixed portal is overlapping a portal that was placed before it... kill it!
		if ( fPlacementSuccess )
		{
			IsPortalOverlappingOtherPortals( this, vOrigin, GetAbsAngles(), true );

			CreateSounds();

			if ( m_pAmbientSound )
			{
				CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();

				controller.SoundChangeVolume( m_pAmbientSound, 0.4, 0.1 );
			}

			DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_2_particles" ) : ( "portal_1_particles" ) ), PATTACH_POINT_FOLLOW, this, "particles_2", true );
			DispatchParticleEffect( ( ( m_bIsPortal2 ) ? ( "portal_2_edge" ) : ( "portal_1_edge" ) ), PATTACH_POINT_FOLLOW, this, "particlespin" );

			if ( m_bIsPortal2 )
			{
				EmitSound( "Portal.open_red" );
			}
			else
			{
				EmitSound( "Portal.open_blue" );
			}
		}
	}
	else
	{
		if ( m_pAmbientSound )
		{
			CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();

			controller.SoundChangeVolume( m_pAmbientSound, 0.0, 0.0 );
		}

		StopParticleEffects( this );
	}

	UpdatePortalTeleportMatrix();

	UpdatePortalLinkage();
}

void CProp_Portal::InputFizzle( inputdata_t &inputdata )
{
	DoFizzleEffect( PORTAL_FIZZLE_KILLED, false );
	Fizzle();
}

//-----------------------------------------------------------------------------
// Purpose: Map can call new location, so far it's only for debugging purposes so it's not made to be very robust.
// Input  : &inputdata - String with 6 float entries with space delimiters, location and orientation
//-----------------------------------------------------------------------------
void CProp_Portal::InputNewLocation( inputdata_t &inputdata )
{
	char sLocationStats[MAX_PATH];
	Q_strncpy( sLocationStats, inputdata.value.String(), sizeof(sLocationStats) );

	// first 3 are location of new origin
	Vector vNewOrigin;
	char* pTok = strtok( sLocationStats, " " ); 
	vNewOrigin.x = atof(pTok);
	pTok = strtok( NULL, " " );
	vNewOrigin.y = atof(pTok);
	pTok = strtok( NULL, " " );
	vNewOrigin.z = atof(pTok);

	// Next 3 entries are new angles
	QAngle vNewAngles;
	pTok = strtok( NULL, " " );
	vNewAngles.x = atof(pTok);
	pTok = strtok( NULL, " " );
	vNewAngles.y = atof(pTok);
	pTok = strtok( NULL, " " );
	vNewAngles.z = atof(pTok);

	// Call main placement function (skipping placement rules)
	NewLocation( vNewOrigin, vNewAngles );
}

void CProp_Portal::UpdateCorners()
{
	Vector vOrigin = GetAbsOrigin();
	Vector vUp, vRight;
	GetVectors( NULL, &vRight, &vUp );

	for ( int i = 0; i < 4; ++i )
	{
		Vector vAddPoint = vOrigin;

		vAddPoint += vRight * ((i & (1<<0))?(PORTAL_HALF_WIDTH):(-PORTAL_HALF_WIDTH));
		vAddPoint += vUp * ((i & (1<<1))?(PORTAL_HALF_HEIGHT):(-PORTAL_HALF_HEIGHT));

		m_vPortalCorners[i] = vAddPoint;
	}
}




void CProp_Portal::ChangeLinkageGroup( unsigned char iLinkageGroupID )
{
	Assert( s_PortalLinkageGroups[m_iLinkageGroupID].Find( this ) != -1 );
	s_PortalLinkageGroups[m_iLinkageGroupID].FindAndRemove( this );
	s_PortalLinkageGroups[iLinkageGroupID].AddToTail( this );
	m_iLinkageGroupID = iLinkageGroupID;
}



CProp_Portal *CProp_Portal::FindPortal( unsigned char iLinkageGroupID, bool bPortal2, bool bCreateIfNothingFound /*= false*/ )
{
	int iPortalCount = s_PortalLinkageGroups[iLinkageGroupID].Count();

	if( iPortalCount != 0 )
	{
		CProp_Portal *pFoundInactive = NULL;
		CProp_Portal **pPortals = s_PortalLinkageGroups[iLinkageGroupID].Base();
		for( int i = 0; i != iPortalCount; ++i )
		{
			if( pPortals[i]->m_bIsPortal2 == bPortal2 )
			{
				if( pPortals[i]->m_bActivated )
					return pPortals[i];
				else
					pFoundInactive = pPortals[i];
			}
		}

		if( pFoundInactive )
			return pFoundInactive;
	}

	if( bCreateIfNothingFound )
	{
		CProp_Portal *pPortal = (CProp_Portal *)CreateEntityByName( "prop_portal" );
		pPortal->m_iLinkageGroupID = iLinkageGroupID;
		pPortal->m_bIsPortal2 = bPortal2;
		DispatchSpawn( pPortal );
		return pPortal;
	}

	return NULL;
}

const CUtlVector<CProp_Portal *> *CProp_Portal::GetPortalLinkageGroup( unsigned char iLinkageGroupID )
{
	return &s_PortalLinkageGroups[iLinkageGroupID];
}



