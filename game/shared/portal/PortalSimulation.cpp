//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=====================================================================================//


#include "cbase.h"
#include "PortalSimulation.h"
#include "vphysics_interface.h"
#include "physics.h"
#include "portal_shareddefs.h"
#include "StaticCollisionPolyhedronCache.h"
#include "model_types.h"
#include "filesystem.h"
#include "collisionutils.h"
#include "tier1/callqueue.h"

#ifndef CLIENT_DLL

#include "world.h"
#include "portal_player.h" //TODO: Move any portal mod specific code to callback functions or something
#include "physicsshadowclone.h"
#include "portal/weapon_physcannon.h"
#include "player_pickup.h"
#include "isaverestore.h"
#include "hierarchy.h"
#include "env_debughistory.h"

#else

#include "c_world.h"

#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CCallQueue *GetPortalCallQueue();

extern IPhysicsConstraintEvent *g_pConstraintEvents;

static ConVar sv_portal_collision_sim_bounds_x( "sv_portal_collision_sim_bounds_x", "200", FCVAR_REPLICATED, "Size of box used to grab collision geometry around placed portals. These should be at the default size or larger only!" );
static ConVar sv_portal_collision_sim_bounds_y( "sv_portal_collision_sim_bounds_y", "200", FCVAR_REPLICATED, "Size of box used to grab collision geometry around placed portals. These should be at the default size or larger only!" );
static ConVar sv_portal_collision_sim_bounds_z( "sv_portal_collision_sim_bounds_z", "252", FCVAR_REPLICATED, "Size of box used to grab collision geometry around placed portals. These should be at the default size or larger only!" );

//#define DEBUG_PORTAL_SIMULATION_CREATION_TIMES //define to output creation timings to developer 2
//#define DEBUG_PORTAL_COLLISION_ENVIRONMENTS //define this to allow for glview collision dumps of portal simulators

#if defined( DEBUG_PORTAL_COLLISION_ENVIRONMENTS ) || defined( DEBUG_PORTAL_SIMULATION_CREATION_TIMES )
#	if !defined( PORTAL_SIMULATORS_EMBED_GUID )
#		pragma message( __FILE__ "(" __LINE__AS_STRING ") : error custom: Portal simulators require a GUID to debug, enable the GUID in PortalSimulation.h ." )
#	endif
#endif

#if defined( DEBUG_PORTAL_COLLISION_ENVIRONMENTS )
void DumpActiveCollision( const CPortalSimulator *pPortalSimulator, const char *szFileName ); //appends to the existing file if it exists
#endif

#define PORTAL_WALL_FARDIST 200.0f
#define PORTAL_WALL_TUBE_DEPTH 1.0f
#define PORTAL_WALL_TUBE_OFFSET 0.01f
#define PORTAL_WALL_MIN_THICKNESS 0.1f
#define PORTAL_POLYHEDRON_CUT_EPSILON (1.0f/1099511627776.0f) //    1 / (1<<40)
#define PORTAL_WORLD_WALL_HALF_SEPARATION_AMOUNT 0.1f //separating the world collision from wall collision by a small amount gets rid of extremely thin erroneous collision at the separating plane

#ifdef DEBUG_PORTAL_COLLISION_ENVIRONMENTS
static ConVar sv_dump_portalsimulator_collision( "sv_dump_portalsimulator_collision", "0", FCVAR_REPLICATED | FCVAR_CHEAT ); //whether to actually dump out the data now that the possibility exists
static void PortalSimulatorDumps_DumpCollideToGlView( CPhysCollide *pCollide, const Vector &origin, const QAngle &angles, float fColorScale, const char *pFilename );
static void PortalSimulatorDumps_DumpBoxToGlView( const Vector &vMins, const Vector &vMaxs, float fRed, float fGreen, float fBlue, const char *pszFileName );
#endif

#ifdef DEBUG_PORTAL_SIMULATION_CREATION_TIMES
#define STARTDEBUGTIMER(x) { x.Start(); }
#define STOPDEBUGTIMER(x) { x.End(); }
#define DEBUGTIMERONLY(x) x
#define CREATEDEBUGTIMER(x) CFastTimer x;
static const char *s_szTabSpacing[] = { "", "\t", "\t\t", "\t\t\t", "\t\t\t\t", "\t\t\t\t\t", "\t\t\t\t\t\t", "\t\t\t\t\t\t\t", "\t\t\t\t\t\t\t\t", "\t\t\t\t\t\t\t\t\t", "\t\t\t\t\t\t\t\t\t\t" };
static int s_iTabSpacingIndex = 0;
static int s_iPortalSimulatorGUID = 0; //used in standalone function that have no idea what a portal simulator is
#define INCREMENTTABSPACING() ++s_iTabSpacingIndex;
#define DECREMENTTABSPACING() --s_iTabSpacingIndex;
#define TABSPACING (s_szTabSpacing[s_iTabSpacingIndex])
#else
#define STARTDEBUGTIMER(x)
#define STOPDEBUGTIMER(x)
#define DEBUGTIMERONLY(x)
#define CREATEDEBUGTIMER(x)
#define INCREMENTTABSPACING()
#define DECREMENTTABSPACING()
#define TABSPACING
#endif

#define PORTAL_HOLE_HALF_HEIGHT (PORTAL_HALF_HEIGHT + 0.1f)
#define PORTAL_HOLE_HALF_WIDTH (PORTAL_HALF_WIDTH + 0.1f)


static void ConvertBrushListToClippedPolyhedronList( const int *pBrushes, int iBrushCount, const float *pOutwardFacingClipPlanes, int iClipPlaneCount, float fClipEpsilon, CUtlVector<CPolyhedron *> *pPolyhedronList );
static void ClipPolyhedrons( CPolyhedron * const *pExistingPolyhedrons, int iPolyhedronCount, const float *pOutwardFacingClipPlanes, int iClipPlaneCount, float fClipEpsilon, CUtlVector<CPolyhedron *> *pPolyhedronList );
static inline CPolyhedron *TransformAndClipSinglePolyhedron( CPolyhedron *pExistingPolyhedron, const VMatrix &Transform, const float *pOutwardFacingClipPlanes, int iClipPlaneCount, float fCutEpsilon, bool bUseTempMemory );
static int GetEntityPhysicsObjects( IPhysicsEnvironment *pEnvironment, CBaseEntity *pEntity, IPhysicsObject **pRetList, int iRetListArraySize );
static CPhysCollide *ConvertPolyhedronsToCollideable( CPolyhedron **pPolyhedrons, int iPolyhedronCount );

#ifndef CLIENT_DLL
static void UpdateShadowClonesPortalSimulationFlags( const CBaseEntity *pSourceEntity, unsigned int iFlags, int iSourceFlags );
static bool g_bPlayerIsInSimulator = false;
#endif

static CUtlVector<CPortalSimulator *> s_PortalSimulators;
CUtlVector<CPortalSimulator *> const &g_PortalSimulators = s_PortalSimulators;

static CPortalSimulator *s_OwnedEntityMap[MAX_EDICTS] = { NULL };
static CPortalSimulatorEventCallbacks s_DummyPortalSimulatorCallback;

const char *PS_SD_Static_World_StaticProps_ClippedProp_t::szTraceSurfaceName = "**studio**";
const int PS_SD_Static_World_StaticProps_ClippedProp_t::iTraceSurfaceFlags = 0;
CBaseEntity *PS_SD_Static_World_StaticProps_ClippedProp_t::pTraceEntity = NULL;



CPortalSimulator::CPortalSimulator( void )
: m_bLocalDataIsReady(false),
	m_bGenerateCollision(true),
	m_bSimulateVPhysics(true),
	m_bSharedCollisionConfiguration(false),
	m_pLinkedPortal(NULL),
	m_bInCrossLinkedFunction(false),
	m_pCallbacks(&s_DummyPortalSimulatorCallback),
	m_DataAccess(m_InternalData)
{
	s_PortalSimulators.AddToTail( this );

#ifdef CLIENT_DLL
	m_bGenerateCollision = (GameRules() && GameRules()->IsMultiplayer());
#endif

	m_CreationChecklist.bPolyhedronsGenerated = false;
	m_CreationChecklist.bLocalCollisionGenerated = false;
	m_CreationChecklist.bLinkedCollisionGenerated = false;
	m_CreationChecklist.bLocalPhysicsGenerated = false;
	m_CreationChecklist.bLinkedPhysicsGenerated = false;

#ifdef PORTAL_SIMULATORS_EMBED_GUID
	static int s_iPortalSimulatorGUIDAllocator = 0;
	m_iPortalSimulatorGUID = s_iPortalSimulatorGUIDAllocator++;
#endif

#ifndef CLIENT_DLL
	PS_SD_Static_World_StaticProps_ClippedProp_t::pTraceEntity = GetWorldEntity(); //will overinitialize, but it's cheap

	m_InternalData.Simulation.pCollisionEntity = (CPSCollisionEntity *)CreateEntityByName( "portalsimulator_collisionentity" );
	Assert( m_InternalData.Simulation.pCollisionEntity != NULL );
	if( m_InternalData.Simulation.pCollisionEntity )
	{
		m_InternalData.Simulation.pCollisionEntity->m_pOwningSimulator = this;
		MarkAsOwned( m_InternalData.Simulation.pCollisionEntity );
		m_InternalData.Simulation.Dynamic.EntFlags[m_InternalData.Simulation.pCollisionEntity->entindex()] |= PSEF_OWNS_PHYSICS;
		DispatchSpawn( m_InternalData.Simulation.pCollisionEntity );
	}
#else
	PS_SD_Static_World_StaticProps_ClippedProp_t::pTraceEntity = GetClientWorldEntity();
#endif
}



CPortalSimulator::~CPortalSimulator( void )
{
	//go assert crazy here
	DetachFromLinked();
	ClearEverything();

	for( int i = s_PortalSimulators.Count(); --i >= 0; )
	{
		if( s_PortalSimulators[i] == this )
		{
			s_PortalSimulators.FastRemove( i );
			break;
		}
	}

	if( m_InternalData.Placement.pHoleShapeCollideable )
		physcollision->DestroyCollide( m_InternalData.Placement.pHoleShapeCollideable );

#ifndef CLIENT_DLL
	if( m_InternalData.Simulation.pCollisionEntity )
	{
		m_InternalData.Simulation.pCollisionEntity->m_pOwningSimulator = NULL;
		m_InternalData.Simulation.Dynamic.EntFlags[m_InternalData.Simulation.pCollisionEntity->entindex()] &= ~PSEF_OWNS_PHYSICS;
		MarkAsReleased( m_InternalData.Simulation.pCollisionEntity );
		UTIL_Remove( m_InternalData.Simulation.pCollisionEntity );
		m_InternalData.Simulation.pCollisionEntity = NULL;
	}
#endif
}



void CPortalSimulator::MoveTo( const Vector &ptCenter, const QAngle &angles )
{
	if( (m_InternalData.Placement.ptCenter == ptCenter) && (m_InternalData.Placement.qAngles == angles) ) //not actually moving at all
		return;

	CREATEDEBUGTIMER( functionTimer );

	STARTDEBUGTIMER( functionTimer );
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::MoveTo() START\n", GetPortalSimulatorGUID(), TABSPACING ); );
	INCREMENTTABSPACING();

#ifndef CLIENT_DLL
	//create a list of all entities that are actually within the portal hole, they will likely need to be moved out of solid space when the portal moves
	CBaseEntity **pFixEntities = (CBaseEntity **)stackalloc( sizeof( CBaseEntity * ) * m_InternalData.Simulation.Dynamic.OwnedEntities.Count() );
	int iFixEntityCount = 0;
	for( int i = m_InternalData.Simulation.Dynamic.OwnedEntities.Count(); --i >= 0; )
	{
		CBaseEntity *pEntity = m_InternalData.Simulation.Dynamic.OwnedEntities[i];
		if( CPhysicsShadowClone::IsShadowClone( pEntity ) ||
			CPSCollisionEntity::IsPortalSimulatorCollisionEntity( pEntity ) )
			continue;

		if( EntityIsInPortalHole( pEntity) )
		{
			pFixEntities[iFixEntityCount] = pEntity;
			++iFixEntityCount;
		}
	}
	VPlane OldPlane = m_InternalData.Placement.PortalPlane; //used in fixing code
#endif

	//update geometric data
	{
		m_InternalData.Placement.ptCenter = ptCenter;
		m_InternalData.Placement.qAngles = angles;
		AngleVectors( angles, &m_InternalData.Placement.vForward, &m_InternalData.Placement.vRight, &m_InternalData.Placement.vUp );
		
		m_InternalData.Placement.PortalPlane.Init( m_InternalData.Placement.vForward, m_InternalData.Placement.vForward.Dot( m_InternalData.Placement.ptCenter ) );
	}

	//Clear();
#ifndef CLIENT_DLL
	ClearLinkedPhysics();
	ClearLocalPhysics();
#endif
	ClearLinkedCollision();
	ClearLocalCollision();
	ClearPolyhedrons();

	m_bLocalDataIsReady = true;
	UpdateLinkMatrix();

	//update hole shape - used to detect if an entity is within the portal hole bounds
	{
		if( m_InternalData.Placement.pHoleShapeCollideable )
			physcollision->DestroyCollide( m_InternalData.Placement.pHoleShapeCollideable );

		float fHolePlanes[6*4];

		//first and second planes are always forward and backward planes
		fHolePlanes[(0*4) + 0] = m_InternalData.Placement.PortalPlane.m_Normal.x;
		fHolePlanes[(0*4) + 1] = m_InternalData.Placement.PortalPlane.m_Normal.y;
		fHolePlanes[(0*4) + 2] = m_InternalData.Placement.PortalPlane.m_Normal.z;
		fHolePlanes[(0*4) + 3] = m_InternalData.Placement.PortalPlane.m_Dist - 0.5f;

		fHolePlanes[(1*4) + 0] = -m_InternalData.Placement.PortalPlane.m_Normal.x;
		fHolePlanes[(1*4) + 1] = -m_InternalData.Placement.PortalPlane.m_Normal.y;
		fHolePlanes[(1*4) + 2] = -m_InternalData.Placement.PortalPlane.m_Normal.z;
		fHolePlanes[(1*4) + 3] = (-m_InternalData.Placement.PortalPlane.m_Dist) + 500.0f;


		//the remaining planes will always have the same ordering of normals, with different distances plugged in for each convex we're creating
		//normal order is up, down, left, right

		fHolePlanes[(2*4) + 0] = m_InternalData.Placement.vUp.x;
		fHolePlanes[(2*4) + 1] = m_InternalData.Placement.vUp.y;
		fHolePlanes[(2*4) + 2] = m_InternalData.Placement.vUp.z;
		fHolePlanes[(2*4) + 3] = m_InternalData.Placement.vUp.Dot( m_InternalData.Placement.ptCenter + (m_InternalData.Placement.vUp * (PORTAL_HALF_HEIGHT * 0.98f)) );

		fHolePlanes[(3*4) + 0] = -m_InternalData.Placement.vUp.x;
		fHolePlanes[(3*4) + 1] = -m_InternalData.Placement.vUp.y;
		fHolePlanes[(3*4) + 2] = -m_InternalData.Placement.vUp.z;
		fHolePlanes[(3*4) + 3] = -m_InternalData.Placement.vUp.Dot( m_InternalData.Placement.ptCenter - (m_InternalData.Placement.vUp * (PORTAL_HALF_HEIGHT * 0.98f)) );

		fHolePlanes[(4*4) + 0] = -m_InternalData.Placement.vRight.x;
		fHolePlanes[(4*4) + 1] = -m_InternalData.Placement.vRight.y;
		fHolePlanes[(4*4) + 2] = -m_InternalData.Placement.vRight.z;
		fHolePlanes[(4*4) + 3] = -m_InternalData.Placement.vRight.Dot( m_InternalData.Placement.ptCenter - (m_InternalData.Placement.vRight * (PORTAL_HALF_WIDTH * 0.98f)) );

		fHolePlanes[(5*4) + 0] = m_InternalData.Placement.vRight.x;
		fHolePlanes[(5*4) + 1] = m_InternalData.Placement.vRight.y;
		fHolePlanes[(5*4) + 2] = m_InternalData.Placement.vRight.z;
		fHolePlanes[(5*4) + 3] = m_InternalData.Placement.vRight.Dot( m_InternalData.Placement.ptCenter + (m_InternalData.Placement.vRight * (PORTAL_HALF_WIDTH * 0.98f)) );

		CPolyhedron *pPolyhedron = GeneratePolyhedronFromPlanes( fHolePlanes, 6, PORTAL_POLYHEDRON_CUT_EPSILON, true );
		Assert( pPolyhedron != NULL );
		CPhysConvex *pConvex = physcollision->ConvexFromConvexPolyhedron( *pPolyhedron );
		pPolyhedron->Release();
		Assert( pConvex != NULL );
		m_InternalData.Placement.pHoleShapeCollideable = physcollision->ConvertConvexToCollide( &pConvex, 1 );
	}

#ifndef CLIENT_DLL
	for( int i = 0; i != iFixEntityCount; ++i )
	{
		if( !EntityIsInPortalHole( pFixEntities[i] ) )
		{
			//this entity is most definitely stuck in a solid wall right now
			//pFixEntities[i]->SetAbsOrigin( pFixEntities[i]->GetAbsOrigin() + (OldPlane.m_Normal * 50.0f) );
			FindClosestPassableSpace( pFixEntities[i], OldPlane.m_Normal );
			continue;
		}

		//entity is still in the hole, but it's possible the hole moved enough where they're in part of the wall
		{
			//TODO: figure out if that's the case and fix it
		}
	}
#endif

	CreatePolyhedrons();	
	CreateAllCollision();
#ifndef CLIENT_DLL
	CreateAllPhysics();
#endif

#if defined( DEBUG_PORTAL_COLLISION_ENVIRONMENTS ) && !defined( CLIENT_DLL )
	if(   sv_dump_portalsimulator_collision.GetBool() )
	{
		const char *szFileName = "pscd.txt";
		filesystem->RemoveFile( szFileName );
		DumpActiveCollision( this, szFileName );
		if( m_pLinkedPortal )
		{
			szFileName = "pscd_linked.txt";
			filesystem->RemoveFile( szFileName );
			DumpActiveCollision( m_pLinkedPortal, szFileName );
		}
	}
#endif

#ifndef CLIENT_DLL
	Assert( (m_InternalData.Simulation.pCollisionEntity == NULL) || OwnsEntity(m_InternalData.Simulation.pCollisionEntity) );
#endif

	STOPDEBUGTIMER( functionTimer );
	DECREMENTTABSPACING();
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::MoveTo() FINISH: %fms\n", GetPortalSimulatorGUID(), TABSPACING, functionTimer.GetDuration().GetMillisecondsF() ); );
}



void CPortalSimulator::UpdateLinkMatrix( void )
{
	if( m_pLinkedPortal && m_pLinkedPortal->m_bLocalDataIsReady )
	{
		Vector vLocalLeft = -m_InternalData.Placement.vRight;
		VMatrix matLocalToWorld( m_InternalData.Placement.vForward, vLocalLeft, m_InternalData.Placement.vUp );
		matLocalToWorld.SetTranslation( m_InternalData.Placement.ptCenter );

		VMatrix matLocalToWorldInverse;
		MatrixInverseTR( matLocalToWorld,matLocalToWorldInverse );

		//180 degree rotation about up
		VMatrix matRotation;
		matRotation.Identity();
		matRotation.m[0][0] = -1.0f;
		matRotation.m[1][1] = -1.0f;

		Vector vRemoteLeft = -m_pLinkedPortal->m_InternalData.Placement.vRight;
		VMatrix matRemoteToWorld( m_pLinkedPortal->m_InternalData.Placement.vForward, vRemoteLeft, m_pLinkedPortal->m_InternalData.Placement.vUp );
		matRemoteToWorld.SetTranslation( m_pLinkedPortal->m_InternalData.Placement.ptCenter );	

		//final
		m_InternalData.Placement.matThisToLinked = matRemoteToWorld * matRotation * matLocalToWorldInverse;
	}
	else
	{
		m_InternalData.Placement.matThisToLinked.Identity();
	}
	
	m_InternalData.Placement.matThisToLinked.InverseTR( m_InternalData.Placement.matLinkedToThis );

	MatrixAngles( m_InternalData.Placement.matThisToLinked.As3x4(), m_InternalData.Placement.ptaap_ThisToLinked.qAngleTransform, m_InternalData.Placement.ptaap_ThisToLinked.ptOriginTransform );
	MatrixAngles( m_InternalData.Placement.matLinkedToThis.As3x4(), m_InternalData.Placement.ptaap_LinkedToThis.qAngleTransform, m_InternalData.Placement.ptaap_LinkedToThis.ptOriginTransform );

	if( m_pLinkedPortal && (m_pLinkedPortal->m_bInCrossLinkedFunction == false) )
	{
		Assert( m_bInCrossLinkedFunction == false ); //I'm pretty sure switching to a stack would have negative repercussions
		m_bInCrossLinkedFunction = true;
		m_pLinkedPortal->UpdateLinkMatrix();
		m_bInCrossLinkedFunction = false;
	}
}

#ifdef DEBUG_PORTAL_COLLISION_ENVIRONMENTS
static ConVar sv_debug_dumpportalhole_nextcheck( "sv_debug_dumpportalhole_nextcheck", "0", FCVAR_CHEAT | FCVAR_REPLICATED );
#endif

bool CPortalSimulator::EntityIsInPortalHole( CBaseEntity *pEntity ) const
{
	if( m_bLocalDataIsReady == false )
		return false;

	Assert( m_InternalData.Placement.pHoleShapeCollideable != NULL );

#ifdef DEBUG_PORTAL_COLLISION_ENVIRONMENTS
	const char *szDumpFileName = "ps_entholecheck.txt";
	if( sv_debug_dumpportalhole_nextcheck.GetBool() )
	{
		filesystem->RemoveFile( szDumpFileName );

		DumpActiveCollision( this, szDumpFileName );
		PortalSimulatorDumps_DumpCollideToGlView( m_InternalData.Placement.pHoleShapeCollideable, vec3_origin, vec3_angle, 1.0f, szDumpFileName );
	}
#endif

	trace_t Trace;

	switch( pEntity->GetSolid() )
	{
	case SOLID_VPHYSICS:
		{
			ICollideable *pCollideable = pEntity->GetCollideable();
			vcollide_t *pVCollide = modelinfo->GetVCollide( pCollideable->GetCollisionModel() );
			
			//Assert( pVCollide != NULL ); //brush models?
			if( pVCollide != NULL )
			{
				Vector ptEntityPosition = pCollideable->GetCollisionOrigin();
				QAngle qEntityAngles = pCollideable->GetCollisionAngles();

#ifdef DEBUG_PORTAL_COLLISION_ENVIRONMENTS
				if( sv_debug_dumpportalhole_nextcheck.GetBool() )
				{
					for( int i = 0; i != pVCollide->solidCount; ++i )
						PortalSimulatorDumps_DumpCollideToGlView( m_InternalData.Placement.pHoleShapeCollideable, vec3_origin, vec3_angle, 0.4f, szDumpFileName );
				
					sv_debug_dumpportalhole_nextcheck.SetValue( false );
				}
#endif

				for( int i = 0; i != pVCollide->solidCount; ++i )
				{
					physcollision->TraceCollide( ptEntityPosition, ptEntityPosition, pVCollide->solids[i], qEntityAngles, m_InternalData.Placement.pHoleShapeCollideable, vec3_origin, vec3_angle, &Trace );

					if( Trace.startsolid )
						return true;
				}
			}
			else
			{
				//energy balls lack a vcollide
				Vector vMins, vMaxs, ptCenter;
				pCollideable->WorldSpaceSurroundingBounds( &vMins, &vMaxs );
				ptCenter = (vMins + vMaxs) * 0.5f;
				vMins -= ptCenter;
				vMaxs -= ptCenter;
				physcollision->TraceBox( ptCenter, ptCenter, vMins, vMaxs, m_InternalData.Placement.pHoleShapeCollideable, vec3_origin, vec3_angle, &Trace );

				return Trace.startsolid;
			}
			break;
		}

	case SOLID_BBOX:
		{
			Vector ptEntityPosition = pEntity->GetAbsOrigin();
			CCollisionProperty *pCollisionProp = pEntity->CollisionProp();

			physcollision->TraceBox( ptEntityPosition, ptEntityPosition, pCollisionProp->OBBMins(), pCollisionProp->OBBMaxs(), m_InternalData.Placement.pHoleShapeCollideable, vec3_origin, vec3_angle, &Trace );

#ifdef DEBUG_PORTAL_COLLISION_ENVIRONMENTS
			if( sv_debug_dumpportalhole_nextcheck.GetBool() )
			{
				Vector vMins = ptEntityPosition + pCollisionProp->OBBMins();
				Vector vMaxs = ptEntityPosition + pCollisionProp->OBBMaxs();
				PortalSimulatorDumps_DumpBoxToGlView( vMins, vMaxs, 1.0f, 1.0f, 1.0f, szDumpFileName );

				sv_debug_dumpportalhole_nextcheck.SetValue( false );
			}
#endif

			if( Trace.startsolid )
				return true;

			break;
		}
	case SOLID_NONE:
#ifdef DEBUG_PORTAL_COLLISION_ENVIRONMENTS
		if( sv_debug_dumpportalhole_nextcheck.GetBool() )
			sv_debug_dumpportalhole_nextcheck.SetValue( false );
#endif

		return false;

	default:
		Assert( false ); //make a handler
	};

#ifdef DEBUG_PORTAL_COLLISION_ENVIRONMENTS
	if( sv_debug_dumpportalhole_nextcheck.GetBool() )
		sv_debug_dumpportalhole_nextcheck.SetValue( false );
#endif

	return false;
}

bool CPortalSimulator::EntityHitBoxExtentIsInPortalHole( CBaseAnimating *pBaseAnimating ) const
{
	if( m_bLocalDataIsReady == false )
		return false;

	bool bFirstVert = true;
	Vector vMinExtent;
	Vector vMaxExtent;

	CStudioHdr *pStudioHdr = pBaseAnimating->GetModelPtr();
	if ( !pStudioHdr )
		return false;

	mstudiohitboxset_t *set = pStudioHdr->pHitboxSet( pBaseAnimating->m_nHitboxSet );
	if ( !set )
		return false;

	Vector position;
	QAngle angles;

	for ( int i = 0; i < set->numhitboxes; i++ )
	{
		mstudiobbox_t *pbox = set->pHitbox( i );

		pBaseAnimating->GetBonePosition( pbox->bone, position, angles );

		// Build a rotation matrix from orientation
		matrix3x4_t fRotateMatrix;
		AngleMatrix( angles, fRotateMatrix );

		//Vector pVerts[8];
		Vector vecPos;
		for ( int i = 0; i < 8; ++i )
		{
			vecPos[0] = ( i & 0x1 ) ? pbox->bbmax[0] : pbox->bbmin[0];
			vecPos[1] = ( i & 0x2 ) ? pbox->bbmax[1] : pbox->bbmin[1];
			vecPos[2] = ( i & 0x4 ) ? pbox->bbmax[2] : pbox->bbmin[2];

			Vector vRotVec;

			VectorRotate( vecPos, fRotateMatrix, vRotVec );
			vRotVec += position;

			if ( bFirstVert )
			{
				vMinExtent = vRotVec;
				vMaxExtent = vRotVec;
				bFirstVert = false;
			}
			else
			{
				vMinExtent = vMinExtent.Min( vRotVec );
				vMaxExtent = vMaxExtent.Max( vRotVec );
			}
		}
	}

	Vector ptCenter = (vMinExtent + vMaxExtent) * 0.5f;
	vMinExtent -= ptCenter;
	vMaxExtent -= ptCenter;

	trace_t Trace;
	physcollision->TraceBox( ptCenter, ptCenter, vMinExtent, vMaxExtent, m_InternalData.Placement.pHoleShapeCollideable, vec3_origin, vec3_angle, &Trace );

	if( Trace.startsolid )
		return true;

	return false;
}

void CPortalSimulator::RemoveEntityFromPortalHole( CBaseEntity *pEntity )
{
	if( EntityIsInPortalHole( pEntity ) )
	{
		FindClosestPassableSpace( pEntity, m_InternalData.Placement.PortalPlane.m_Normal );
	}
}

bool CPortalSimulator::RayIsInPortalHole( const Ray_t &ray ) const
{
	trace_t Trace;
	physcollision->TraceBox( ray, m_InternalData.Placement.pHoleShapeCollideable, vec3_origin, vec3_angle, &Trace );
	return Trace.DidHit();
}

void CPortalSimulator::ClearEverything( void )
{
	CREATEDEBUGTIMER( functionTimer );

	STARTDEBUGTIMER( functionTimer );
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::Clear() START\n", GetPortalSimulatorGUID(), TABSPACING ); );
	INCREMENTTABSPACING();	

#ifndef CLIENT_DLL
	ClearAllPhysics();
#endif
	ClearAllCollision();
	ClearPolyhedrons();

#ifndef CLIENT_DLL
	ReleaseAllEntityOwnership();

	Assert( (m_InternalData.Simulation.pCollisionEntity == NULL) || OwnsEntity(m_InternalData.Simulation.pCollisionEntity) );
#endif

	STOPDEBUGTIMER( functionTimer );
	DECREMENTTABSPACING();
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::Clear() FINISH: %fms\n", GetPortalSimulatorGUID(), TABSPACING, functionTimer.GetDuration().GetMillisecondsF() ); );
}



void CPortalSimulator::AttachTo( CPortalSimulator *pLinkedPortalSimulator )
{
	Assert( pLinkedPortalSimulator );

	if( pLinkedPortalSimulator == m_pLinkedPortal )
		return;

	CREATEDEBUGTIMER( functionTimer );

	STARTDEBUGTIMER( functionTimer );
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::AttachTo() START\n", GetPortalSimulatorGUID(), TABSPACING ); );
	INCREMENTTABSPACING();
	
	DetachFromLinked();

	m_pLinkedPortal = pLinkedPortalSimulator;
	pLinkedPortalSimulator->m_pLinkedPortal = this;

	if( m_bLocalDataIsReady && m_pLinkedPortal->m_bLocalDataIsReady )
	{
		UpdateLinkMatrix();
		CreateLinkedCollision();
#ifndef CLIENT_DLL
		CreateLinkedPhysics();
#endif
	}

#if defined( DEBUG_PORTAL_COLLISION_ENVIRONMENTS ) && !defined( CLIENT_DLL )
	if( sv_dump_portalsimulator_collision.GetBool() )
	{
		const char *szFileName = "pscd.txt";
		filesystem->RemoveFile( szFileName );
		DumpActiveCollision( this, szFileName );
		if( m_pLinkedPortal )
		{
			szFileName = "pscd_linked.txt";
			filesystem->RemoveFile( szFileName );
			DumpActiveCollision( m_pLinkedPortal, szFileName );
		}
	}
#endif

	STOPDEBUGTIMER( functionTimer );
	DECREMENTTABSPACING();
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::AttachTo() FINISH: %fms\n", GetPortalSimulatorGUID(), TABSPACING, functionTimer.GetDuration().GetMillisecondsF() ); );
}


#ifndef CLIENT_DLL
void CPortalSimulator::TakeOwnershipOfEntity( CBaseEntity *pEntity )
{
	AssertMsg( m_bLocalDataIsReady, "Tell the portal simulator where it is with MoveTo() before using it in any other way." );

	Assert( pEntity != NULL );
	if( pEntity == NULL )
		return;

	if( pEntity->IsWorld() )
		return;
	
	if( CPhysicsShadowClone::IsShadowClone( pEntity ) )
		return;

	if( pEntity->GetServerVehicle() != NULL ) //we don't take kindly to vehicles in these here parts. Their physics controllers currently don't migrate properly and cause a crash
		return;

	if( OwnsEntity( pEntity ) )
		return;

	Assert( GetSimulatorThatOwnsEntity( pEntity ) == NULL );
	MarkAsOwned( pEntity );
	Assert( GetSimulatorThatOwnsEntity( pEntity ) == this );

	if( EntityIsInPortalHole( pEntity ) )
		m_InternalData.Simulation.Dynamic.EntFlags[pEntity->entindex()] |= PSEF_IS_IN_PORTAL_HOLE;
	else
		m_InternalData.Simulation.Dynamic.EntFlags[pEntity->entindex()] &= ~PSEF_IS_IN_PORTAL_HOLE;

	UpdateShadowClonesPortalSimulationFlags( pEntity, PSEF_IS_IN_PORTAL_HOLE, m_InternalData.Simulation.Dynamic.EntFlags[pEntity->entindex()] );

	m_pCallbacks->PortalSimulator_TookOwnershipOfEntity( pEntity );

	if( IsSimulatingVPhysics() )
		TakePhysicsOwnership( pEntity );

	pEntity->CollisionRulesChanged(); //absolutely necessary in single-environment mode, possibly expendable in multi-environment moder
	//pEntity->SetGroundEntity( NULL );
	IPhysicsObject *pObject = pEntity->VPhysicsGetObject();
	if( pObject )
	{
		pObject->Wake();
		pObject->RecheckContactPoints();
	}
	
	CUtlVector<CBaseEntity *> childrenList;
	GetAllChildren( pEntity, childrenList );
	for ( int i = childrenList.Count(); --i >= 0; )
	{
		CBaseEntity *pEnt = childrenList[i];
		CPortalSimulator *pOwningSimulator = GetSimulatorThatOwnsEntity( pEnt );
		if( pOwningSimulator != this )
		{
			if( pOwningSimulator != NULL )
				pOwningSimulator->ReleaseOwnershipOfEntity( pEnt, (pOwningSimulator == m_pLinkedPortal) );

			TakeOwnershipOfEntity( childrenList[i] );
		}
	}
}



void CPortalSimulator::TakePhysicsOwnership( CBaseEntity *pEntity )
{
	if( m_InternalData.Simulation.pPhysicsEnvironment == NULL )
		return;

	if( CPSCollisionEntity::IsPortalSimulatorCollisionEntity( pEntity ) )
		return;

	Assert( CPhysicsShadowClone::IsShadowClone( pEntity ) == false );
	Assert( OwnsEntity( pEntity ) ); //taking physics ownership happens AFTER general ownership
	
	if( OwnsPhysicsForEntity( pEntity ) )
		return;

	int iEntIndex = pEntity->entindex();
	m_InternalData.Simulation.Dynamic.EntFlags[iEntIndex] |= PSEF_OWNS_PHYSICS;


	//physics cloning
	{	
#ifdef _DEBUG
		{
			int iDebugIndex;
			for( iDebugIndex = m_InternalData.Simulation.Dynamic.ShadowClones.FromLinkedPortal.Count(); --iDebugIndex >= 0; )
			{
				if( m_InternalData.Simulation.Dynamic.ShadowClones.FromLinkedPortal[iDebugIndex]->GetClonedEntity() == pEntity )
					break;
			}
			AssertMsg( iDebugIndex < 0, "Trying to own an entity, when a clone from the linked portal already exists" ); 

			if( m_pLinkedPortal )
			{
				for( iDebugIndex = m_pLinkedPortal->m_InternalData.Simulation.Dynamic.ShadowClones.FromLinkedPortal.Count(); --iDebugIndex >= 0; )
				{
					if( m_pLinkedPortal->m_InternalData.Simulation.Dynamic.ShadowClones.FromLinkedPortal[iDebugIndex]->GetClonedEntity() == pEntity )
						break;
				}
				AssertMsg( iDebugIndex < 0, "Trying to own an entity, when we're already exporting a clone to the linked portal" );
			}

			//Don't require a copy from main to already exist
		}
#endif

		EHANDLE hEnt = pEntity;

		//To linked portal
		if( m_pLinkedPortal && m_pLinkedPortal->m_InternalData.Simulation.pPhysicsEnvironment )
		{

			DBG_CODE(
				for( int i = m_pLinkedPortal->m_InternalData.Simulation.Dynamic.ShadowClones.FromLinkedPortal.Count(); --i >= 0; )
					AssertMsg( m_pLinkedPortal->m_InternalData.Simulation.Dynamic.ShadowClones.FromLinkedPortal[i]->GetClonedEntity() != pEntity, "Already cloning to linked portal." );
			);

			CPhysicsShadowClone *pClone = CPhysicsShadowClone::CreateShadowClone( m_pLinkedPortal->m_InternalData.Simulation.pPhysicsEnvironment, hEnt, "CPortalSimulator::TakePhysicsOwnership(): To Linked Portal", &m_InternalData.Placement.matThisToLinked.As3x4() );
			if( pClone )
			{
				//bool bHeldByPhyscannon = false;
				CBaseEntity *pHeldEntity = NULL;
				CPortal_Player *pPlayer = (CPortal_Player *)GetPlayerHoldingEntity( pEntity );

				if ( !pPlayer && pEntity->IsPlayer() )
				{
					pPlayer = (CPortal_Player *)pEntity;
				}

				if ( pPlayer )
				{
					pHeldEntity = GetPlayerHeldEntity( pPlayer );
					/*if ( !pHeldEntity )
					{
						pHeldEntity = PhysCannonGetHeldEntity( pPlayer->GetActiveWeapon() );
						bHeldByPhyscannon = true;
					}*/
				}

				if( pHeldEntity )
				{
					//player is holding the entity, force them to pick it back up again
					bool bIsHeldObjectOnOppositeSideOfPortal = pPlayer->IsHeldObjectOnOppositeSideOfPortal();
					pPlayer->m_bSilentDropAndPickup = true;
					pPlayer->ForceDropOfCarriedPhysObjects( pHeldEntity );
					pPlayer->SetHeldObjectOnOppositeSideOfPortal( bIsHeldObjectOnOppositeSideOfPortal );
				}

				m_pLinkedPortal->m_InternalData.Simulation.Dynamic.ShadowClones.FromLinkedPortal.AddToTail( pClone );
				m_pLinkedPortal->MarkAsOwned( pClone );
				m_pLinkedPortal->m_InternalData.Simulation.Dynamic.EntFlags[pClone->entindex()] |= PSEF_OWNS_PHYSICS;
				m_pLinkedPortal->m_InternalData.Simulation.Dynamic.EntFlags[pClone->entindex()] |= m_InternalData.Simulation.Dynamic.EntFlags[pEntity->entindex()] & PSEF_IS_IN_PORTAL_HOLE;
				pClone->CollisionRulesChanged(); //adding the clone to the portal simulator changes how it collides

				if( pHeldEntity )
				{
					/*if ( bHeldByPhyscannon )
					{
						PhysCannonPickupObject( pPlayer, pHeldEntity );
					}
					else*/
					{
						PlayerPickupObject( pPlayer, pHeldEntity );
					}
					pPlayer->m_bSilentDropAndPickup = false;
				}
			}
		}
	}

	m_pCallbacks->PortalSimulator_TookPhysicsOwnershipOfEntity( pEntity );
}

void RecheckEntityCollision( CBaseEntity *pEntity )
{
	CCallQueue *pCallQueue;
	if ( (pCallQueue = GetPortalCallQueue()) != NULL )
	{
		pCallQueue->QueueCall( RecheckEntityCollision, pEntity );
		return;
	}

	pEntity->CollisionRulesChanged(); //absolutely necessary in single-environment mode, possibly expendable in multi-environment mode
	//pEntity->SetGroundEntity( NULL );
	IPhysicsObject *pObject = pEntity->VPhysicsGetObject();
	if( pObject )
	{
		pObject->Wake();
		pObject->RecheckContactPoints();
	}
}


void CPortalSimulator::ReleaseOwnershipOfEntity( CBaseEntity *pEntity, bool bMovingToLinkedSimulator /*= false*/ )
{
	if( pEntity == NULL )
		return;

	if( pEntity->IsWorld() )
		return;

	if( !OwnsEntity( pEntity ) )
		return;

	if( m_InternalData.Simulation.pPhysicsEnvironment )
		ReleasePhysicsOwnership( pEntity, true, bMovingToLinkedSimulator );

	m_InternalData.Simulation.Dynamic.EntFlags[pEntity->entindex()] &= ~PSEF_IS_IN_PORTAL_HOLE;
	UpdateShadowClonesPortalSimulationFlags( pEntity, PSEF_IS_IN_PORTAL_HOLE, m_InternalData.Simulation.Dynamic.EntFlags[pEntity->entindex()] );

	Assert( GetSimulatorThatOwnsEntity( pEntity ) == this );
	MarkAsReleased( pEntity );
	Assert( GetSimulatorThatOwnsEntity( pEntity ) == NULL );

	for( int i = m_InternalData.Simulation.Dynamic.OwnedEntities.Count(); --i >= 0; )
	{
		if( m_InternalData.Simulation.Dynamic.OwnedEntities[i] == pEntity )
		{
			m_InternalData.Simulation.Dynamic.OwnedEntities.FastRemove(i);
			break;
		}
	}

	if( bMovingToLinkedSimulator == false )
	{
		RecheckEntityCollision( pEntity );
	}

	m_pCallbacks->PortalSimulator_ReleasedOwnershipOfEntity( pEntity );

	CUtlVector<CBaseEntity *> childrenList;
	GetAllChildren( pEntity, childrenList );
	for ( int i = childrenList.Count(); --i >= 0; )
		ReleaseOwnershipOfEntity( childrenList[i] );
}

void CPortalSimulator::ReleaseAllEntityOwnership( void )
{
	//Assert( m_bLocalDataIsReady || (m_InternalData.Simulation.Dynamic.OwnedEntities.Count() == 0) );
	int iSkippedObjects = 0;
	while( m_InternalData.Simulation.Dynamic.OwnedEntities.Count() != iSkippedObjects ) //the release function changes OwnedEntities
	{
		CBaseEntity *pEntity = m_InternalData.Simulation.Dynamic.OwnedEntities[iSkippedObjects];
		if( CPhysicsShadowClone::IsShadowClone( pEntity ) ||
			CPSCollisionEntity::IsPortalSimulatorCollisionEntity( pEntity ) )
		{
			++iSkippedObjects;
			continue;
		}
		RemoveEntityFromPortalHole( pEntity ); //assume that whenever someone wants to release all entities, it's because the portal is going away
		ReleaseOwnershipOfEntity( pEntity );
	}

	Assert( (m_InternalData.Simulation.pCollisionEntity == NULL) || OwnsEntity(m_InternalData.Simulation.pCollisionEntity) );
}


void CPortalSimulator::ReleasePhysicsOwnership( CBaseEntity *pEntity, bool bContinuePhysicsCloning /*= true*/, bool bMovingToLinkedSimulator /*= false*/ )
{
	if( CPSCollisionEntity::IsPortalSimulatorCollisionEntity( pEntity ) )
		return;

	Assert( OwnsEntity( pEntity ) ); //releasing physics ownership happens BEFORE releasing general ownership
	Assert( CPhysicsShadowClone::IsShadowClone( pEntity ) == false );

	if( m_InternalData.Simulation.pPhysicsEnvironment == NULL )
		return;

	if( !OwnsPhysicsForEntity( pEntity ) )
		return;

	if( IsSimulatingVPhysics() == false )
		bContinuePhysicsCloning = false;

	int iEntIndex = pEntity->entindex();
	m_InternalData.Simulation.Dynamic.EntFlags[iEntIndex] &= ~PSEF_OWNS_PHYSICS;
	
	//physics cloning
	{
#ifdef _DEBUG
		{
			int iDebugIndex;
			for( iDebugIndex = m_InternalData.Simulation.Dynamic.ShadowClones.FromLinkedPortal.Count(); --iDebugIndex >= 0; )
			{
				if( m_InternalData.Simulation.Dynamic.ShadowClones.FromLinkedPortal[iDebugIndex]->GetClonedEntity() == pEntity )
					break;
			}
			AssertMsg( iDebugIndex < 0, "Trying to release an entity, when a clone from the linked portal already exists." );
		}
#endif

		//clear exported clones
		{
			DBG_CODE_NOSCOPE( bool bFoundAlready = false; );
			DBG_CODE_NOSCOPE( const char *szLastFoundMarker = NULL; );

			//to linked portal
			if( m_pLinkedPortal )
			{
				DBG_CODE_NOSCOPE( bFoundAlready = false; );
				for( int i = m_pLinkedPortal->m_InternalData.Simulation.Dynamic.ShadowClones.FromLinkedPortal.Count(); --i >= 0; )
				{
					if( m_pLinkedPortal->m_InternalData.Simulation.Dynamic.ShadowClones.FromLinkedPortal[i]->GetClonedEntity() == pEntity )
					{
						CPhysicsShadowClone *pClone = m_pLinkedPortal->m_InternalData.Simulation.Dynamic.ShadowClones.FromLinkedPortal[i];
						AssertMsg( bFoundAlready == false, "Multiple clones to linked portal found." );
						DBG_CODE_NOSCOPE( bFoundAlready = true; );
						DBG_CODE_NOSCOPE( szLastFoundMarker = pClone->m_szDebugMarker );

						//bool bHeldByPhyscannon = false;
						CBaseEntity *pHeldEntity = NULL;
						CPortal_Player *pPlayer = (CPortal_Player *)GetPlayerHoldingEntity( pEntity );

						if ( !pPlayer && pEntity->IsPlayer() )
						{
							pPlayer = (CPortal_Player *)pEntity;
						}

						if ( pPlayer )
						{
							pHeldEntity = GetPlayerHeldEntity( pPlayer );
							/*if ( !pHeldEntity )
							{
								pHeldEntity = PhysCannonGetHeldEntity( pPlayer->GetActiveWeapon() );
								bHeldByPhyscannon = true;
							}*/
						}

						if( pHeldEntity )
						{
							//player is holding the entity, force them to pick it back up again
							bool bIsHeldObjectOnOppositeSideOfPortal = pPlayer->IsHeldObjectOnOppositeSideOfPortal();
							pPlayer->m_bSilentDropAndPickup = true;
							pPlayer->ForceDropOfCarriedPhysObjects( pHeldEntity );
							pPlayer->SetHeldObjectOnOppositeSideOfPortal( bIsHeldObjectOnOppositeSideOfPortal );
						}
						else
						{
							pHeldEntity = NULL;
						}

						m_pLinkedPortal->m_InternalData.Simulation.Dynamic.EntFlags[pClone->entindex()] &= ~PSEF_OWNS_PHYSICS;
						m_pLinkedPortal->MarkAsReleased( pClone );
						pClone->Free();
						m_pLinkedPortal->m_InternalData.Simulation.Dynamic.ShadowClones.FromLinkedPortal.FastRemove(i);

						if( pHeldEntity )
						{
							/*if ( bHeldByPhyscannon )
							{
								PhysCannonPickupObject( pPlayer, pHeldEntity );
							}
							else*/
							{
								PlayerPickupObject( pPlayer, pHeldEntity );
							}
							pPlayer->m_bSilentDropAndPickup = false;
						}

						DBG_CODE_NOSCOPE( continue; );
						break;
					}
				}
			}
		}
	}

	m_pCallbacks->PortalSimulator_ReleasedPhysicsOwnershipOfEntity( pEntity );
}

void CPortalSimulator::StartCloningEntity( CBaseEntity *pEntity )
{
	if( CPhysicsShadowClone::IsShadowClone( pEntity ) || CPSCollisionEntity::IsPortalSimulatorCollisionEntity( pEntity ) )
		return;

	if( (m_InternalData.Simulation.Dynamic.EntFlags[pEntity->entindex()] & PSEF_CLONES_ENTITY_FROM_MAIN) != 0 )
		return; //already cloned, no work to do

#ifdef _DEBUG
	for( int i = m_InternalData.Simulation.Dynamic.ShadowClones.ShouldCloneFromMain.Count(); --i >= 0; )
		Assert( m_InternalData.Simulation.Dynamic.ShadowClones.ShouldCloneFromMain[i] != pEntity );
#endif

	//NDebugOverlay::EntityBounds( pEntity, 0, 255, 0, 50, 5.0f );

	m_InternalData.Simulation.Dynamic.ShadowClones.ShouldCloneFromMain.AddToTail( pEntity );
	m_InternalData.Simulation.Dynamic.EntFlags[pEntity->entindex()] |= PSEF_CLONES_ENTITY_FROM_MAIN;
}

void CPortalSimulator::StopCloningEntity( CBaseEntity *pEntity )
{
	if( (m_InternalData.Simulation.Dynamic.EntFlags[pEntity->entindex()] & PSEF_CLONES_ENTITY_FROM_MAIN) == 0 )
	{
		Assert( m_InternalData.Simulation.Dynamic.ShadowClones.ShouldCloneFromMain.Find( pEntity ) == -1 );
		return; //not cloned, no work to do
	}

	//NDebugOverlay::EntityBounds( pEntity, 255, 0, 0, 50, 5.0f );

	m_InternalData.Simulation.Dynamic.ShadowClones.ShouldCloneFromMain.FastRemove(m_InternalData.Simulation.Dynamic.ShadowClones.ShouldCloneFromMain.Find( pEntity ));
	m_InternalData.Simulation.Dynamic.EntFlags[pEntity->entindex()] &= ~PSEF_CLONES_ENTITY_FROM_MAIN;
}


/*void CPortalSimulator::TeleportEntityToLinkedPortal( CBaseEntity *pEntity )
{
	//TODO: migrate teleportation code from CProp_Portal::Touch to here


}*/


void CPortalSimulator::MarkAsOwned( CBaseEntity *pEntity )
{
	Assert( pEntity != NULL );
	int iEntIndex = pEntity->entindex();
	Assert( s_OwnedEntityMap[iEntIndex] == NULL );
#ifdef _DEBUG
	for( int i = m_InternalData.Simulation.Dynamic.OwnedEntities.Count(); --i >= 0; )
		Assert( m_InternalData.Simulation.Dynamic.OwnedEntities[i] != pEntity );
#endif
	Assert( (m_InternalData.Simulation.Dynamic.EntFlags[iEntIndex] & PSEF_OWNS_ENTITY) == 0 );

	m_InternalData.Simulation.Dynamic.EntFlags[iEntIndex] |= PSEF_OWNS_ENTITY;
	s_OwnedEntityMap[iEntIndex] = this;
	m_InternalData.Simulation.Dynamic.OwnedEntities.AddToTail( pEntity );

	if ( pEntity->IsPlayer() )
	{
		g_bPlayerIsInSimulator = true;
	}
}

void CPortalSimulator::MarkAsReleased( CBaseEntity *pEntity )
{
	Assert( pEntity != NULL );
	int iEntIndex = pEntity->entindex();
	Assert( s_OwnedEntityMap[iEntIndex] == this );
	Assert( ((m_InternalData.Simulation.Dynamic.EntFlags[iEntIndex] & PSEF_OWNS_ENTITY) != 0) || CPSCollisionEntity::IsPortalSimulatorCollisionEntity(pEntity) );

	s_OwnedEntityMap[iEntIndex] = NULL;
	m_InternalData.Simulation.Dynamic.EntFlags[iEntIndex] &= ~PSEF_OWNS_ENTITY;
	int i;
	for( i = m_InternalData.Simulation.Dynamic.OwnedEntities.Count(); --i >= 0; )
	{
		if( m_InternalData.Simulation.Dynamic.OwnedEntities[i] == pEntity )
		{
			m_InternalData.Simulation.Dynamic.OwnedEntities.FastRemove(i);
			break;
		}
	}
	Assert( i >= 0 );


	if ( pEntity->IsPlayer() )
	{
		g_bPlayerIsInSimulator = false;
	}
}



void CPortalSimulator::CreateAllPhysics( void )
{
	if( IsSimulatingVPhysics() == false )
		return;

	CREATEDEBUGTIMER( functionTimer );

	STARTDEBUGTIMER( functionTimer );
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::CreateAllPhysics() START\n", GetPortalSimulatorGUID(), TABSPACING ); );
	INCREMENTTABSPACING();
	
	CreateMinimumPhysics();
	CreateLocalPhysics();
	CreateLinkedPhysics();

	STOPDEBUGTIMER( functionTimer );
	DECREMENTTABSPACING();
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::CreateAllPhysics() FINISH: %fms\n", GetPortalSimulatorGUID(), TABSPACING, functionTimer.GetDuration().GetMillisecondsF() ); );
}



void CPortalSimulator::CreateMinimumPhysics( void )
{
	if( IsSimulatingVPhysics() == false )
		return;

	if( m_InternalData.Simulation.pPhysicsEnvironment != NULL )
		return;

	CREATEDEBUGTIMER( functionTimer );

	STARTDEBUGTIMER( functionTimer );
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::CreateMinimumPhysics() START\n", GetPortalSimulatorGUID(), TABSPACING ); );
	INCREMENTTABSPACING();

	m_InternalData.Simulation.pPhysicsEnvironment = physenv_main;

	STOPDEBUGTIMER( functionTimer );
	DECREMENTTABSPACING();
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::CreateMinimumPhysics() FINISH: %fms\n", GetPortalSimulatorGUID(), TABSPACING, functionTimer.GetDuration().GetMillisecondsF() ); );
}



void CPortalSimulator::CreateLocalPhysics( void )
{
	if( IsSimulatingVPhysics() == false )
		return;

	AssertMsg( m_bLocalDataIsReady, "Portal simulator attempting to create local physics before being placed." );

	if( m_CreationChecklist.bLocalPhysicsGenerated )
		return;

	CREATEDEBUGTIMER( functionTimer );

	STARTDEBUGTIMER( functionTimer );
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::CreateLocalPhysics() START\n", GetPortalSimulatorGUID(), TABSPACING ); );
	INCREMENTTABSPACING();
	
	CreateMinimumPhysics();

	//int iDefaultSurfaceIndex = physprops->GetSurfaceIndex( "default" );
	objectparams_t params = g_PhysDefaultObjectParams;

	// Any non-moving object can point to world safely-- Make sure we dont use 'params' for something other than that beyond this point.
	if( m_InternalData.Simulation.pCollisionEntity )
		params.pGameData = m_InternalData.Simulation.pCollisionEntity;
	else
		GetWorldEntity();

	//World
	{
		Assert( m_InternalData.Simulation.Static.World.Brushes.pPhysicsObject == NULL ); //Be sure to find graceful fixes for asserts, performance is a big concern with portal simulation
		if( m_InternalData.Simulation.Static.World.Brushes.pCollideable != NULL )
		{
			m_InternalData.Simulation.Static.World.Brushes.pPhysicsObject = m_InternalData.Simulation.pPhysicsEnvironment->CreatePolyObjectStatic( m_InternalData.Simulation.Static.World.Brushes.pCollideable, m_InternalData.Simulation.Static.SurfaceProperties.surface.surfaceProps, vec3_origin, vec3_angle, &params );
			
			if( (m_InternalData.Simulation.pCollisionEntity != NULL) && (m_InternalData.Simulation.pCollisionEntity->VPhysicsGetObject() == NULL) )
				m_InternalData.Simulation.pCollisionEntity->VPhysicsSetObject(m_InternalData.Simulation.Static.World.Brushes.pPhysicsObject);

			m_InternalData.Simulation.Static.World.Brushes.pPhysicsObject->RecheckCollisionFilter(); //some filters only work after the variable is stored in the class
		}

		//Assert( m_InternalData.Simulation.Static.World.StaticProps.PhysicsObjects.Count() == 0 ); //Be sure to find graceful fixes for asserts, performance is a big concern with portal simulation
#ifdef _DEBUG
		for( int i = m_InternalData.Simulation.Static.World.StaticProps.ClippedRepresentations.Count(); --i >= 0; )
		{
			Assert( m_InternalData.Simulation.Static.World.StaticProps.ClippedRepresentations[i].pPhysicsObject == NULL ); //Be sure to find graceful fixes for asserts, performance is a big concern with portal simulation
		}
#endif
		
		if( m_InternalData.Simulation.Static.World.StaticProps.ClippedRepresentations.Count() != 0 )
		{
			Assert( m_InternalData.Simulation.Static.World.StaticProps.bCollisionExists );
			for( int i = m_InternalData.Simulation.Static.World.StaticProps.ClippedRepresentations.Count(); --i >= 0; )
			{
				PS_SD_Static_World_StaticProps_ClippedProp_t &Representation = m_InternalData.Simulation.Static.World.StaticProps.ClippedRepresentations[i];
				Assert( Representation.pCollide != NULL );
				Assert( Representation.pPhysicsObject == NULL );
				
				Representation.pPhysicsObject = m_InternalData.Simulation.pPhysicsEnvironment->CreatePolyObjectStatic( Representation.pCollide, Representation.iTraceSurfaceProps, vec3_origin, vec3_angle, &params );
				Assert( Representation.pPhysicsObject != NULL );
				Representation.pPhysicsObject->RecheckCollisionFilter(); //some filters only work after the variable is stored in the class
			}
		}
		m_InternalData.Simulation.Static.World.StaticProps.bPhysicsExists = true;
	}

	//Wall
	{
		Assert( m_InternalData.Simulation.Static.Wall.Local.Brushes.pPhysicsObject == NULL ); //Be sure to find graceful fixes for asserts, performance is a big concern with portal simulation
		if( m_InternalData.Simulation.Static.Wall.Local.Brushes.pCollideable != NULL )
		{
			m_InternalData.Simulation.Static.Wall.Local.Brushes.pPhysicsObject = m_InternalData.Simulation.pPhysicsEnvironment->CreatePolyObjectStatic( m_InternalData.Simulation.Static.Wall.Local.Brushes.pCollideable, m_InternalData.Simulation.Static.SurfaceProperties.surface.surfaceProps, vec3_origin, vec3_angle, &params );
			
			if( (m_InternalData.Simulation.pCollisionEntity != NULL) && (m_InternalData.Simulation.pCollisionEntity->VPhysicsGetObject() == NULL) )
				m_InternalData.Simulation.pCollisionEntity->VPhysicsSetObject(m_InternalData.Simulation.Static.World.Brushes.pPhysicsObject);

			m_InternalData.Simulation.Static.Wall.Local.Brushes.pPhysicsObject->RecheckCollisionFilter(); //some filters only work after the variable is stored in the class
		}

		Assert( m_InternalData.Simulation.Static.Wall.Local.Tube.pPhysicsObject == NULL ); //Be sure to find graceful fixes for asserts, performance is a big concern with portal simulation
		if( m_InternalData.Simulation.Static.Wall.Local.Tube.pCollideable != NULL )
		{
			m_InternalData.Simulation.Static.Wall.Local.Tube.pPhysicsObject = m_InternalData.Simulation.pPhysicsEnvironment->CreatePolyObjectStatic( m_InternalData.Simulation.Static.Wall.Local.Tube.pCollideable, m_InternalData.Simulation.Static.SurfaceProperties.surface.surfaceProps, vec3_origin, vec3_angle, &params );
			
			if( (m_InternalData.Simulation.pCollisionEntity != NULL) && (m_InternalData.Simulation.pCollisionEntity->VPhysicsGetObject() == NULL) )
				m_InternalData.Simulation.pCollisionEntity->VPhysicsSetObject(m_InternalData.Simulation.Static.World.Brushes.pPhysicsObject);

			m_InternalData.Simulation.Static.Wall.Local.Tube.pPhysicsObject->RecheckCollisionFilter(); //some filters only work after the variable is stored in the class
		}
	}

	//re-acquire environment physics for owned entities
	for( int i = m_InternalData.Simulation.Dynamic.OwnedEntities.Count(); --i >= 0; )
		TakePhysicsOwnership( m_InternalData.Simulation.Dynamic.OwnedEntities[i] );

	if( m_InternalData.Simulation.pCollisionEntity )
		m_InternalData.Simulation.pCollisionEntity->CollisionRulesChanged();
	
	STOPDEBUGTIMER( functionTimer );
	DECREMENTTABSPACING();
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::CreateLocalPhysics() FINISH: %fms\n", GetPortalSimulatorGUID(), TABSPACING, functionTimer.GetDuration().GetMillisecondsF() ); );

	m_CreationChecklist.bLocalPhysicsGenerated = true;
}



void CPortalSimulator::CreateLinkedPhysics( void )
{
	if( IsSimulatingVPhysics() == false )
		return;

	AssertMsg( m_bLocalDataIsReady, "Portal simulator attempting to create linked physics before being placed itself." );

	if( (m_pLinkedPortal == NULL) || (m_pLinkedPortal->m_bLocalDataIsReady == false) )
		return;

	if( m_CreationChecklist.bLinkedPhysicsGenerated )
		return;

	CREATEDEBUGTIMER( functionTimer );
	STARTDEBUGTIMER( functionTimer );
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::CreateLinkedPhysics() START\n", GetPortalSimulatorGUID(), TABSPACING ); );
	INCREMENTTABSPACING();
	
	CreateMinimumPhysics();

	//int iDefaultSurfaceIndex = physprops->GetSurfaceIndex( "default" );
	objectparams_t params = g_PhysDefaultObjectParams;

	if( m_InternalData.Simulation.pCollisionEntity )
		params.pGameData = m_InternalData.Simulation.pCollisionEntity;
	else
		params.pGameData = GetWorldEntity();

	//everything in our linked collision should be based on the linked portal's world collision
	PS_SD_Static_World_t &RemoteSimulationStaticWorld = m_pLinkedPortal->m_InternalData.Simulation.Static.World;

	Assert( m_InternalData.Simulation.Static.Wall.RemoteTransformedToLocal.Brushes.pPhysicsObject == NULL ); //Be sure to find graceful fixes for asserts, performance is a big concern with portal simulation
	if( RemoteSimulationStaticWorld.Brushes.pCollideable != NULL )
	{
		m_InternalData.Simulation.Static.Wall.RemoteTransformedToLocal.Brushes.pPhysicsObject = m_InternalData.Simulation.pPhysicsEnvironment->CreatePolyObjectStatic( RemoteSimulationStaticWorld.Brushes.pCollideable, m_pLinkedPortal->m_InternalData.Simulation.Static.SurfaceProperties.surface.surfaceProps, m_InternalData.Placement.ptaap_LinkedToThis.ptOriginTransform, m_InternalData.Placement.ptaap_LinkedToThis.qAngleTransform, &params );
		m_InternalData.Simulation.Static.Wall.RemoteTransformedToLocal.Brushes.pPhysicsObject->RecheckCollisionFilter(); //some filters only work after the variable is stored in the class
	}
	

	Assert( m_InternalData.Simulation.Static.Wall.RemoteTransformedToLocal.StaticProps.PhysicsObjects.Count() == 0 ); //Be sure to find graceful fixes for asserts, performance is a big concern with portal simulation
	if( RemoteSimulationStaticWorld.StaticProps.ClippedRepresentations.Count() != 0 )
	{
		for( int i = RemoteSimulationStaticWorld.StaticProps.ClippedRepresentations.Count(); --i >= 0; )
		{
			PS_SD_Static_World_StaticProps_ClippedProp_t &Representation = RemoteSimulationStaticWorld.StaticProps.ClippedRepresentations[i];
			IPhysicsObject *pPhysObject = m_InternalData.Simulation.pPhysicsEnvironment->CreatePolyObjectStatic( Representation.pCollide, Representation.iTraceSurfaceProps, m_InternalData.Placement.ptaap_LinkedToThis.ptOriginTransform, m_InternalData.Placement.ptaap_LinkedToThis.qAngleTransform, &params );
			if( pPhysObject )
			{
				m_InternalData.Simulation.Static.Wall.RemoteTransformedToLocal.StaticProps.PhysicsObjects.AddToTail( pPhysObject );
				pPhysObject->RecheckCollisionFilter(); //some filters only work after the variable is stored in the class
			}
		}
	}

	//re-clone physicsshadowclones from the remote environment
	CUtlVector<CBaseEntity *> &RemoteOwnedEntities = m_pLinkedPortal->m_InternalData.Simulation.Dynamic.OwnedEntities;
	for( int i = RemoteOwnedEntities.Count(); --i >= 0; )
	{
		if( CPhysicsShadowClone::IsShadowClone( RemoteOwnedEntities[i] ) ||
			CPSCollisionEntity::IsPortalSimulatorCollisionEntity( RemoteOwnedEntities[i] ) )
			continue;

		int j;
		for( j = m_InternalData.Simulation.Dynamic.ShadowClones.FromLinkedPortal.Count(); --j >= 0; )
		{
			if( m_InternalData.Simulation.Dynamic.ShadowClones.FromLinkedPortal[j]->GetClonedEntity() == RemoteOwnedEntities[i] )
				break;
		}

		if( j >= 0 ) //already cloning
			continue;
					
		

		EHANDLE hEnt = RemoteOwnedEntities[i];
		CPhysicsShadowClone *pClone = CPhysicsShadowClone::CreateShadowClone( m_InternalData.Simulation.pPhysicsEnvironment, hEnt, "CPortalSimulator::CreateLinkedPhysics(): From Linked Portal", &m_InternalData.Placement.matLinkedToThis.As3x4() );
		if( pClone )
		{
			MarkAsOwned( pClone );
			m_InternalData.Simulation.Dynamic.EntFlags[pClone->entindex()] |= PSEF_OWNS_PHYSICS;
			m_InternalData.Simulation.Dynamic.ShadowClones.FromLinkedPortal.AddToTail( pClone );
			pClone->CollisionRulesChanged(); //adding the clone to the portal simulator changes how it collides
		}
	}

	if( m_pLinkedPortal && (m_pLinkedPortal->m_bInCrossLinkedFunction == false) )
	{
		Assert( m_bInCrossLinkedFunction == false ); //I'm pretty sure switching to a stack would have negative repercussions
		m_bInCrossLinkedFunction = true;
		m_pLinkedPortal->CreateLinkedPhysics();
		m_bInCrossLinkedFunction = false;
	}

	if( m_InternalData.Simulation.pCollisionEntity )
		m_InternalData.Simulation.pCollisionEntity->CollisionRulesChanged();

	STOPDEBUGTIMER( functionTimer );
	DECREMENTTABSPACING();
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::CreateLinkedPhysics() FINISH: %fms\n", GetPortalSimulatorGUID(), TABSPACING, functionTimer.GetDuration().GetMillisecondsF() ); );

	m_CreationChecklist.bLinkedPhysicsGenerated = true;
}



void CPortalSimulator::ClearAllPhysics( void )
{
	CREATEDEBUGTIMER( functionTimer );

	STARTDEBUGTIMER( functionTimer );
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::ClearAllPhysics() START\n", GetPortalSimulatorGUID(), TABSPACING ); );
	INCREMENTTABSPACING();
	
	ClearLinkedPhysics();
	ClearLocalPhysics();
	ClearMinimumPhysics();

	STOPDEBUGTIMER( functionTimer );
	DECREMENTTABSPACING();
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::ClearAllPhysics() FINISH: %fms\n", GetPortalSimulatorGUID(), TABSPACING, functionTimer.GetDuration().GetMillisecondsF() ); );
}



void CPortalSimulator::ClearMinimumPhysics( void )
{
	if( m_InternalData.Simulation.pPhysicsEnvironment == NULL )
		return;

	CREATEDEBUGTIMER( functionTimer );

	STARTDEBUGTIMER( functionTimer );
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::ClearMinimumPhysics() START\n", GetPortalSimulatorGUID(), TABSPACING ); );
	INCREMENTTABSPACING();

	m_InternalData.Simulation.pPhysicsEnvironment = NULL;

	STOPDEBUGTIMER( functionTimer );
	DECREMENTTABSPACING();
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::ClearMinimumPhysics() FINISH: %fms\n", GetPortalSimulatorGUID(), TABSPACING, functionTimer.GetDuration().GetMillisecondsF() ); );
}



void CPortalSimulator::ClearLocalPhysics( void )
{
	if( m_CreationChecklist.bLocalPhysicsGenerated == false )
		return;

	if( m_InternalData.Simulation.pPhysicsEnvironment == NULL )
		return;

	CREATEDEBUGTIMER( functionTimer );

	STARTDEBUGTIMER( functionTimer );
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::ClearLocalPhysics() START\n", GetPortalSimulatorGUID(), TABSPACING ); );
	INCREMENTTABSPACING();
	
	m_InternalData.Simulation.pPhysicsEnvironment->CleanupDeleteList();
	m_InternalData.Simulation.pPhysicsEnvironment->SetQuickDelete( true ); //if we don't do this, things crash the next time we cleanup the delete list while checking mindists

	if( m_InternalData.Simulation.Static.World.Brushes.pPhysicsObject )
	{
		m_InternalData.Simulation.pPhysicsEnvironment->DestroyObject( m_InternalData.Simulation.Static.World.Brushes.pPhysicsObject );
		m_InternalData.Simulation.Static.World.Brushes.pPhysicsObject = NULL;
	}

	if( m_InternalData.Simulation.Static.World.StaticProps.bPhysicsExists && 
		(m_InternalData.Simulation.Static.World.StaticProps.ClippedRepresentations.Count() != 0) )
	{
		for( int i = m_InternalData.Simulation.Static.World.StaticProps.ClippedRepresentations.Count(); --i >= 0; )
		{
			PS_SD_Static_World_StaticProps_ClippedProp_t &Representation = m_InternalData.Simulation.Static.World.StaticProps.ClippedRepresentations[i];
			if( Representation.pPhysicsObject )
			{
				m_InternalData.Simulation.pPhysicsEnvironment->DestroyObject( Representation.pPhysicsObject );
				Representation.pPhysicsObject = NULL;
			}
		}
	}
	m_InternalData.Simulation.Static.World.StaticProps.bPhysicsExists = false;

	if( m_InternalData.Simulation.Static.Wall.Local.Brushes.pPhysicsObject )
	{
		m_InternalData.Simulation.pPhysicsEnvironment->DestroyObject( m_InternalData.Simulation.Static.Wall.Local.Brushes.pPhysicsObject );
		m_InternalData.Simulation.Static.Wall.Local.Brushes.pPhysicsObject = NULL;
	}

	if( m_InternalData.Simulation.Static.Wall.Local.Tube.pPhysicsObject )
	{
		m_InternalData.Simulation.pPhysicsEnvironment->DestroyObject( m_InternalData.Simulation.Static.Wall.Local.Tube.pPhysicsObject );
		m_InternalData.Simulation.Static.Wall.Local.Tube.pPhysicsObject = NULL;
	}

	//all physics clones
	{
		for( int i = m_InternalData.Simulation.Dynamic.ShadowClones.FromLinkedPortal.Count(); --i >= 0; )
		{
			CPhysicsShadowClone *pClone = m_InternalData.Simulation.Dynamic.ShadowClones.FromLinkedPortal[i];
			Assert( GetSimulatorThatOwnsEntity( pClone ) == this );
			m_InternalData.Simulation.Dynamic.EntFlags[pClone->entindex()] &= ~PSEF_OWNS_PHYSICS;
			MarkAsReleased( pClone );
			Assert( GetSimulatorThatOwnsEntity( pClone ) == NULL );
			pClone->Free();
		}

		m_InternalData.Simulation.Dynamic.ShadowClones.FromLinkedPortal.RemoveAll();		
	}

	Assert( m_InternalData.Simulation.Dynamic.ShadowClones.FromLinkedPortal.Count() == 0 );

	//release physics ownership of owned entities
	for( int i = m_InternalData.Simulation.Dynamic.OwnedEntities.Count(); --i >= 0; )
		ReleasePhysicsOwnership( m_InternalData.Simulation.Dynamic.OwnedEntities[i], false );

	Assert( m_InternalData.Simulation.Dynamic.ShadowClones.FromLinkedPortal.Count() == 0 );

	m_InternalData.Simulation.pPhysicsEnvironment->CleanupDeleteList();
	m_InternalData.Simulation.pPhysicsEnvironment->SetQuickDelete( false );

	if( m_InternalData.Simulation.pCollisionEntity )
		m_InternalData.Simulation.pCollisionEntity->CollisionRulesChanged();

	STOPDEBUGTIMER( functionTimer );
	DECREMENTTABSPACING();
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::ClearLocalPhysics() FINISH: %fms\n", GetPortalSimulatorGUID(), TABSPACING, functionTimer.GetDuration().GetMillisecondsF() ); );

	m_CreationChecklist.bLocalPhysicsGenerated = false;
}



void CPortalSimulator::ClearLinkedPhysics( void )
{
	if( m_CreationChecklist.bLinkedPhysicsGenerated == false )
		return;

	if( m_InternalData.Simulation.pPhysicsEnvironment == NULL )
		return;

	CREATEDEBUGTIMER( functionTimer );

	STARTDEBUGTIMER( functionTimer );
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::ClearLinkedPhysics() START\n", GetPortalSimulatorGUID(), TABSPACING ); );
	INCREMENTTABSPACING();

	m_InternalData.Simulation.pPhysicsEnvironment->CleanupDeleteList();
	m_InternalData.Simulation.pPhysicsEnvironment->SetQuickDelete( true ); //if we don't do this, things crash the next time we cleanup the delete list while checking mindists

	//static collideables
	{
		if( m_InternalData.Simulation.Static.Wall.RemoteTransformedToLocal.Brushes.pPhysicsObject )
		{
			m_InternalData.Simulation.pPhysicsEnvironment->DestroyObject( m_InternalData.Simulation.Static.Wall.RemoteTransformedToLocal.Brushes.pPhysicsObject );
			m_InternalData.Simulation.Static.Wall.RemoteTransformedToLocal.Brushes.pPhysicsObject = NULL;
		}

		if( m_InternalData.Simulation.Static.Wall.RemoteTransformedToLocal.StaticProps.PhysicsObjects.Count() )
		{
			for( int i = m_InternalData.Simulation.Static.Wall.RemoteTransformedToLocal.StaticProps.PhysicsObjects.Count(); --i >= 0; )
				m_InternalData.Simulation.pPhysicsEnvironment->DestroyObject( m_InternalData.Simulation.Static.Wall.RemoteTransformedToLocal.StaticProps.PhysicsObjects[i] );
			
			m_InternalData.Simulation.Static.Wall.RemoteTransformedToLocal.StaticProps.PhysicsObjects.RemoveAll();
		}
	}

	//clones from the linked portal
	{
		for( int i = m_InternalData.Simulation.Dynamic.ShadowClones.FromLinkedPortal.Count(); --i >= 0; )
		{
			CPhysicsShadowClone *pClone = m_InternalData.Simulation.Dynamic.ShadowClones.FromLinkedPortal[i];
			m_InternalData.Simulation.Dynamic.EntFlags[pClone->entindex()] &= ~PSEF_OWNS_PHYSICS;
			MarkAsReleased( pClone );
			pClone->Free();
		}

		m_InternalData.Simulation.Dynamic.ShadowClones.FromLinkedPortal.RemoveAll();
	}


	if( m_pLinkedPortal && (m_pLinkedPortal->m_bInCrossLinkedFunction == false) )
	{
		Assert( m_bInCrossLinkedFunction == false ); //I'm pretty sure switching to a stack would have negative repercussions
		m_bInCrossLinkedFunction = true;
		m_pLinkedPortal->ClearLinkedPhysics();
		m_bInCrossLinkedFunction = false;
	}

	Assert( (m_InternalData.Simulation.Dynamic.ShadowClones.FromLinkedPortal.Count() == 0) && 
		((m_pLinkedPortal == NULL) || (m_pLinkedPortal->m_InternalData.Simulation.Dynamic.ShadowClones.FromLinkedPortal.Count() == 0)) );

	m_InternalData.Simulation.pPhysicsEnvironment->CleanupDeleteList();
	m_InternalData.Simulation.pPhysicsEnvironment->SetQuickDelete( false );

	if( m_InternalData.Simulation.pCollisionEntity )
		m_InternalData.Simulation.pCollisionEntity->CollisionRulesChanged();

	STOPDEBUGTIMER( functionTimer );
	DECREMENTTABSPACING();
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::ClearLinkedPhysics() FINISH: %fms\n", GetPortalSimulatorGUID(), TABSPACING, functionTimer.GetDuration().GetMillisecondsF() ); );

	m_CreationChecklist.bLinkedPhysicsGenerated = false;
}


void CPortalSimulator::ClearLinkedEntities( void )
{
	//clones from the linked portal
	{
		for( int i = m_InternalData.Simulation.Dynamic.ShadowClones.FromLinkedPortal.Count(); --i >= 0; )
		{
			CPhysicsShadowClone *pClone = m_InternalData.Simulation.Dynamic.ShadowClones.FromLinkedPortal[i];
			m_InternalData.Simulation.Dynamic.EntFlags[pClone->entindex()] &= ~PSEF_OWNS_PHYSICS;
			MarkAsReleased( pClone );
			pClone->Free();
		}

		m_InternalData.Simulation.Dynamic.ShadowClones.FromLinkedPortal.RemoveAll();
	}
}
#endif //#ifndef CLIENT_DLL


void CPortalSimulator::CreateAllCollision( void )
{
	CREATEDEBUGTIMER( functionTimer );

	STARTDEBUGTIMER( functionTimer );
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::CreateAllCollision() START\n", GetPortalSimulatorGUID(), TABSPACING ); );
	INCREMENTTABSPACING();

	CreateLocalCollision();
	CreateLinkedCollision();

	STOPDEBUGTIMER( functionTimer );
	DECREMENTTABSPACING();
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::CreateAllCollision() FINISH: %fms\n", GetPortalSimulatorGUID(), TABSPACING, functionTimer.GetDuration().GetMillisecondsF() ); );
}



void CPortalSimulator::CreateLocalCollision( void )
{
	AssertMsg( m_bLocalDataIsReady, "Portal simulator attempting to create local collision before being placed." );

	if( m_CreationChecklist.bLocalCollisionGenerated )
		return;

	if( IsCollisionGenerationEnabled() == false )
		return;

	DEBUGTIMERONLY( s_iPortalSimulatorGUID = GetPortalSimulatorGUID() );

	CREATEDEBUGTIMER( functionTimer );

	STARTDEBUGTIMER( functionTimer );
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::CreateLocalCollision() START\n", GetPortalSimulatorGUID(), TABSPACING ); );
	INCREMENTTABSPACING();
	
	CREATEDEBUGTIMER( worldBrushTimer );
	STARTDEBUGTIMER( worldBrushTimer );
	Assert( m_InternalData.Simulation.Static.World.Brushes.pCollideable == NULL ); //Be sure to find graceful fixes for asserts, performance is a big concern with portal simulation
	if( m_InternalData.Simulation.Static.World.Brushes.Polyhedrons.Count() != 0 )
		m_InternalData.Simulation.Static.World.Brushes.pCollideable = ConvertPolyhedronsToCollideable( m_InternalData.Simulation.Static.World.Brushes.Polyhedrons.Base(), m_InternalData.Simulation.Static.World.Brushes.Polyhedrons.Count() );
	STOPDEBUGTIMER( worldBrushTimer );
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sWorld Brushes=%fms\n", GetPortalSimulatorGUID(), TABSPACING, worldBrushTimer.GetDuration().GetMillisecondsF() ); );

	CREATEDEBUGTIMER( worldPropTimer );
	STARTDEBUGTIMER( worldPropTimer );
#ifdef _DEBUG
	for( int i = m_InternalData.Simulation.Static.World.StaticProps.ClippedRepresentations.Count(); --i >= 0; )
	{
		Assert( m_InternalData.Simulation.Static.World.StaticProps.ClippedRepresentations[i].pCollide == NULL );
	}
#endif
	Assert( m_InternalData.Simulation.Static.World.StaticProps.bCollisionExists == false ); //Be sure to find graceful fixes for asserts, performance is a big concern with portal simulation
	if( m_InternalData.Simulation.Static.World.StaticProps.ClippedRepresentations.Count() != 0 )
	{
		Assert( m_InternalData.Simulation.Static.World.StaticProps.Polyhedrons.Count() != 0 );
		CPolyhedron **pPolyhedronsBase = m_InternalData.Simulation.Static.World.StaticProps.Polyhedrons.Base();
		for( int i = m_InternalData.Simulation.Static.World.StaticProps.ClippedRepresentations.Count(); --i >= 0; )
		{
			PS_SD_Static_World_StaticProps_ClippedProp_t &Representation = m_InternalData.Simulation.Static.World.StaticProps.ClippedRepresentations[i];
			
			Assert( Representation.pCollide == NULL );
			Representation.pCollide = ConvertPolyhedronsToCollideable( &pPolyhedronsBase[Representation.PolyhedronGroup.iStartIndex], Representation.PolyhedronGroup.iNumPolyhedrons );
			Assert( Representation.pCollide != NULL );
		}
	}
	m_InternalData.Simulation.Static.World.StaticProps.bCollisionExists = true;
	STOPDEBUGTIMER( worldPropTimer );
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sWorld Props=%fms\n", GetPortalSimulatorGUID(), TABSPACING, worldPropTimer.GetDuration().GetMillisecondsF() ); );

	if( IsSimulatingVPhysics() )
	{
		//only need the tube when simulating player movement

		//TODO: replace the complete wall with the wall shell
		CREATEDEBUGTIMER( wallBrushTimer );
		STARTDEBUGTIMER( wallBrushTimer );
		Assert( m_InternalData.Simulation.Static.Wall.Local.Brushes.pCollideable == NULL ); //Be sure to find graceful fixes for asserts, performance is a big concern with portal simulation
		if( m_InternalData.Simulation.Static.Wall.Local.Brushes.Polyhedrons.Count() != 0 )
			m_InternalData.Simulation.Static.Wall.Local.Brushes.pCollideable = ConvertPolyhedronsToCollideable( m_InternalData.Simulation.Static.Wall.Local.Brushes.Polyhedrons.Base(), m_InternalData.Simulation.Static.Wall.Local.Brushes.Polyhedrons.Count() );
		STOPDEBUGTIMER( wallBrushTimer );
		DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sWall Brushes=%fms\n", GetPortalSimulatorGUID(), TABSPACING, wallBrushTimer.GetDuration().GetMillisecondsF() ); );
	}

	CREATEDEBUGTIMER( wallTubeTimer );
	STARTDEBUGTIMER( wallTubeTimer );
	Assert( m_InternalData.Simulation.Static.Wall.Local.Tube.pCollideable == NULL ); //Be sure to find graceful fixes for asserts, performance is a big concern with portal simulation
	if( m_InternalData.Simulation.Static.Wall.Local.Tube.Polyhedrons.Count() != 0 )
		m_InternalData.Simulation.Static.Wall.Local.Tube.pCollideable = ConvertPolyhedronsToCollideable( m_InternalData.Simulation.Static.Wall.Local.Tube.Polyhedrons.Base(), m_InternalData.Simulation.Static.Wall.Local.Tube.Polyhedrons.Count() );
	STOPDEBUGTIMER( wallTubeTimer );
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sWall Tube=%fms\n", GetPortalSimulatorGUID(), TABSPACING, wallTubeTimer.GetDuration().GetMillisecondsF() ); );

	//grab surface properties to use for the portal environment
	{
		CTraceFilterWorldAndPropsOnly filter;
		trace_t Trace;
		UTIL_TraceLine( m_InternalData.Placement.ptCenter + m_InternalData.Placement.vForward, m_InternalData.Placement.ptCenter - (m_InternalData.Placement.vForward * 500.0f), MASK_SOLID_BRUSHONLY, &filter, &Trace );

		if( Trace.fraction != 1.0f )
		{
			m_InternalData.Simulation.Static.SurfaceProperties.contents = Trace.contents;
			m_InternalData.Simulation.Static.SurfaceProperties.surface = Trace.surface;
			m_InternalData.Simulation.Static.SurfaceProperties.pEntity = Trace.m_pEnt;
		}
		else
		{
			m_InternalData.Simulation.Static.SurfaceProperties.contents = CONTENTS_SOLID;
			m_InternalData.Simulation.Static.SurfaceProperties.surface.name = "**empty**";
			m_InternalData.Simulation.Static.SurfaceProperties.surface.flags = 0;
			m_InternalData.Simulation.Static.SurfaceProperties.surface.surfaceProps = 0;
#ifndef CLIENT_DLL
			m_InternalData.Simulation.Static.SurfaceProperties.pEntity = GetWorldEntity();
#else
			m_InternalData.Simulation.Static.SurfaceProperties.pEntity = GetClientWorldEntity();
#endif
		}
		
#ifndef CLIENT_DLL
		if( m_InternalData.Simulation.pCollisionEntity )
			m_InternalData.Simulation.Static.SurfaceProperties.pEntity = m_InternalData.Simulation.pCollisionEntity;
#endif		
	}

	STOPDEBUGTIMER( functionTimer );
	DECREMENTTABSPACING();
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::CreateLocalCollision() FINISH: %fms\n", GetPortalSimulatorGUID(), TABSPACING, functionTimer.GetDuration().GetMillisecondsF() ); );

	m_CreationChecklist.bLocalCollisionGenerated = true;
}



void CPortalSimulator::CreateLinkedCollision( void )
{
	if( m_CreationChecklist.bLinkedCollisionGenerated )
		return;

	if( IsCollisionGenerationEnabled() == false )
		return;

	//nothing to do for now, the current set of collision is just transformed from the linked simulator when needed. It's really cheap to transform in traces and physics generation.
	
	m_CreationChecklist.bLinkedCollisionGenerated = true;
}



void CPortalSimulator::ClearAllCollision( void )
{
	CREATEDEBUGTIMER( functionTimer );

	STARTDEBUGTIMER( functionTimer );
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::ClearAllCollision() START\n", GetPortalSimulatorGUID(), TABSPACING ); );
	INCREMENTTABSPACING();
	
	ClearLinkedCollision();
	ClearLocalCollision();

	STOPDEBUGTIMER( functionTimer );
	DECREMENTTABSPACING();
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::ClearAllCollision() FINISH: %fms\n", GetPortalSimulatorGUID(), TABSPACING, functionTimer.GetDuration().GetMillisecondsF() ); );
}



void CPortalSimulator::ClearLinkedCollision( void )
{
	if( m_CreationChecklist.bLinkedCollisionGenerated == false )
		return;

	//nothing to do for now, the current set of collision is just transformed from the linked simulator when needed. It's really cheap to transform in traces and physics generation.
	
	m_CreationChecklist.bLinkedCollisionGenerated = false;
}



void CPortalSimulator::ClearLocalCollision( void )
{
	if( m_CreationChecklist.bLocalCollisionGenerated == false )
		return;

	CREATEDEBUGTIMER( functionTimer );

	STARTDEBUGTIMER( functionTimer );
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::ClearLocalCollision() START\n", GetPortalSimulatorGUID(), TABSPACING ); );
	INCREMENTTABSPACING();
	
	if( m_InternalData.Simulation.Static.Wall.Local.Brushes.pCollideable )
	{
		physcollision->DestroyCollide( m_InternalData.Simulation.Static.Wall.Local.Brushes.pCollideable );
		m_InternalData.Simulation.Static.Wall.Local.Brushes.pCollideable = NULL;
	}

	if( m_InternalData.Simulation.Static.Wall.Local.Tube.pCollideable )
	{
		physcollision->DestroyCollide( m_InternalData.Simulation.Static.Wall.Local.Tube.pCollideable );
		m_InternalData.Simulation.Static.Wall.Local.Tube.pCollideable = NULL;
	}

	if( m_InternalData.Simulation.Static.World.Brushes.pCollideable )
	{
		physcollision->DestroyCollide( m_InternalData.Simulation.Static.World.Brushes.pCollideable );
		m_InternalData.Simulation.Static.World.Brushes.pCollideable = NULL;
	}

	if( m_InternalData.Simulation.Static.World.StaticProps.bCollisionExists && 
		(m_InternalData.Simulation.Static.World.StaticProps.ClippedRepresentations.Count() != 0) )
	{
		for( int i = m_InternalData.Simulation.Static.World.StaticProps.ClippedRepresentations.Count(); --i >= 0; )
		{
			PS_SD_Static_World_StaticProps_ClippedProp_t &Representation = m_InternalData.Simulation.Static.World.StaticProps.ClippedRepresentations[i];
			if( Representation.pCollide )
			{
				physcollision->DestroyCollide( Representation.pCollide );
				Representation.pCollide = NULL;
			}
		}
	}
	m_InternalData.Simulation.Static.World.StaticProps.bCollisionExists = false;

	STOPDEBUGTIMER( functionTimer );
	DECREMENTTABSPACING();
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::ClearLocalCollision() FINISH: %fms\n", GetPortalSimulatorGUID(), TABSPACING, functionTimer.GetDuration().GetMillisecondsF() ); );

	m_CreationChecklist.bLocalCollisionGenerated = false;
}



void CPortalSimulator::CreatePolyhedrons( void )
{
	if( m_CreationChecklist.bPolyhedronsGenerated )
		return;

	if( IsCollisionGenerationEnabled() == false )
		return;

	CREATEDEBUGTIMER( functionTimer );

	STARTDEBUGTIMER( functionTimer );
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::CreatePolyhedrons() START\n", GetPortalSimulatorGUID(), TABSPACING ); );
	INCREMENTTABSPACING();

	//forward reverse conventions signify whether the normal is the same direction as m_InternalData.Placement.PortalPlane.m_Normal
	//World and wall conventions signify whether it's been shifted in front of the portal plane or behind it

	float fWorldClipPlane_Forward[4] = {	m_InternalData.Placement.PortalPlane.m_Normal.x,
											m_InternalData.Placement.PortalPlane.m_Normal.y,
											m_InternalData.Placement.PortalPlane.m_Normal.z,
											m_InternalData.Placement.PortalPlane.m_Dist + PORTAL_WORLD_WALL_HALF_SEPARATION_AMOUNT };

	float fWorldClipPlane_Reverse[4] = {	-fWorldClipPlane_Forward[0],
											-fWorldClipPlane_Forward[1],
											-fWorldClipPlane_Forward[2],
											-fWorldClipPlane_Forward[3] };

	float fWallClipPlane_Forward[4] = {		m_InternalData.Placement.PortalPlane.m_Normal.x,
											m_InternalData.Placement.PortalPlane.m_Normal.y,
											m_InternalData.Placement.PortalPlane.m_Normal.z,
											m_InternalData.Placement.PortalPlane.m_Dist }; // - PORTAL_WORLD_WALL_HALF_SEPARATION_AMOUNT

	//float fWallClipPlane_Reverse[4] = {		-fWallClipPlane_Forward[0],
	//										-fWallClipPlane_Forward[1],
	//										-fWallClipPlane_Forward[2],
	//										-fWallClipPlane_Forward[3] };


	//World
	{
		Vector vOBBForward = m_InternalData.Placement.vForward;
		Vector vOBBRight = m_InternalData.Placement.vRight;
		Vector vOBBUp = m_InternalData.Placement.vUp;


		//scale the extents to usable sizes
		float flScaleX = sv_portal_collision_sim_bounds_x.GetFloat();
		if ( flScaleX < 200.0f )
			flScaleX = 200.0f;
		float flScaleY = sv_portal_collision_sim_bounds_y.GetFloat();
		if ( flScaleY < 200.0f )
			flScaleY = 200.0f;
		float flScaleZ = sv_portal_collision_sim_bounds_z.GetFloat();
		if ( flScaleZ < 252.0f )
			flScaleZ = 252.0f;

		vOBBForward *= flScaleX;
		vOBBRight	*= flScaleY;
		vOBBUp		*= flScaleZ;	// default size for scale z (252) is player (height + portal half height) * 2. Any smaller than this will allow for players to 
									// reach unsimulated geometry before an end touch with teh portal.

		Vector ptOBBOrigin = m_InternalData.Placement.ptCenter;
		ptOBBOrigin -= vOBBRight / 2.0f;
		ptOBBOrigin -= vOBBUp / 2.0f;

		Vector vAABBMins, vAABBMaxs;
		vAABBMins = vAABBMaxs = ptOBBOrigin;

		for( int i = 1; i != 8; ++i )
		{
			Vector ptTest = ptOBBOrigin;
			if( i & (1 << 0) ) ptTest += vOBBForward;
			if( i & (1 << 1) ) ptTest += vOBBRight;
			if( i & (1 << 2) ) ptTest += vOBBUp;

			if( ptTest.x < vAABBMins.x ) vAABBMins.x = ptTest.x;
			if( ptTest.y < vAABBMins.y ) vAABBMins.y = ptTest.y;
			if( ptTest.z < vAABBMins.z ) vAABBMins.z = ptTest.z;
			if( ptTest.x > vAABBMaxs.x ) vAABBMaxs.x = ptTest.x;
			if( ptTest.y > vAABBMaxs.y ) vAABBMaxs.y = ptTest.y;
			if( ptTest.z > vAABBMaxs.z ) vAABBMaxs.z = ptTest.z;
		}

		//Brushes
		{
			Assert( m_InternalData.Simulation.Static.World.Brushes.Polyhedrons.Count() == 0 );

			CUtlVector<int> WorldBrushes;
			enginetrace->GetBrushesInAABB( vAABBMins, vAABBMaxs, &WorldBrushes, MASK_SOLID_BRUSHONLY|CONTENTS_PLAYERCLIP|CONTENTS_MONSTERCLIP );

			//create locally clipped polyhedrons for the world
			{
				int *pBrushList = WorldBrushes.Base();
				int iBrushCount = WorldBrushes.Count();
				ConvertBrushListToClippedPolyhedronList( pBrushList, iBrushCount, fWorldClipPlane_Reverse, 1, PORTAL_POLYHEDRON_CUT_EPSILON, &m_InternalData.Simulation.Static.World.Brushes.Polyhedrons );
			}
		}

		//static props
		{
			Assert( m_InternalData.Simulation.Static.World.StaticProps.Polyhedrons.Count() == 0 );

			CUtlVector<ICollideable *> StaticProps;
			staticpropmgr->GetAllStaticPropsInAABB( vAABBMins, vAABBMaxs, &StaticProps );

			for( int i = StaticProps.Count(); --i >= 0; )
			{
				ICollideable *pProp = StaticProps[i];

				CPolyhedron *PolyhedronArray[1024];
				int iPolyhedronCount = g_StaticCollisionPolyhedronCache.GetStaticPropPolyhedrons( pProp, PolyhedronArray, 1024 );

				StaticPropPolyhedronGroups_t indices;
				indices.iStartIndex = m_InternalData.Simulation.Static.World.StaticProps.Polyhedrons.Count();

				for( int j = 0; j != iPolyhedronCount; ++j )
				{
					CPolyhedron *pPropPolyhedronPiece = PolyhedronArray[j];
					if( pPropPolyhedronPiece )
					{
						CPolyhedron *pClippedPropPolyhedron = ClipPolyhedron( pPropPolyhedronPiece, fWorldClipPlane_Reverse, 1, 0.01f, false );
						if( pClippedPropPolyhedron )
							m_InternalData.Simulation.Static.World.StaticProps.Polyhedrons.AddToTail( pClippedPropPolyhedron );
					}
				}

				indices.iNumPolyhedrons = m_InternalData.Simulation.Static.World.StaticProps.Polyhedrons.Count() - indices.iStartIndex;
				if( indices.iNumPolyhedrons != 0 )
				{
					int index = m_InternalData.Simulation.Static.World.StaticProps.ClippedRepresentations.AddToTail();
					PS_SD_Static_World_StaticProps_ClippedProp_t &NewEntry = m_InternalData.Simulation.Static.World.StaticProps.ClippedRepresentations[index];

					NewEntry.PolyhedronGroup = indices;
					NewEntry.pCollide = NULL;
#ifndef CLIENT_DLL
					NewEntry.pPhysicsObject = NULL;
#endif
					NewEntry.pSourceProp = pProp->GetEntityHandle();

					const model_t *pModel = pProp->GetCollisionModel();
					bool bIsStudioModel = pModel && (modelinfo->GetModelType( pModel ) == mod_studio);
					AssertOnce( bIsStudioModel );
					if( bIsStudioModel )
					{
						studiohdr_t *pStudioHdr = modelinfo->GetStudiomodel( pModel );
						Assert( pStudioHdr != NULL );
						NewEntry.iTraceContents = pStudioHdr->contents;						
						NewEntry.iTraceSurfaceProps = physprops->GetSurfaceIndex( pStudioHdr->pszSurfaceProp() );
					}
					else
					{
						NewEntry.iTraceContents = m_InternalData.Simulation.Static.SurfaceProperties.contents;
						NewEntry.iTraceSurfaceProps = m_InternalData.Simulation.Static.SurfaceProperties.surface.surfaceProps;
					}
				}
			}
		}
	}



	//(Holy) Wall
	{
		Assert( m_InternalData.Simulation.Static.Wall.Local.Tube.Polyhedrons.Count() == 0 );
		Assert( m_InternalData.Simulation.Static.Wall.Local.Brushes.Polyhedrons.Count() == 0 );

		Vector vBackward = -m_InternalData.Placement.vForward;
		Vector vLeft = -m_InternalData.Placement.vRight;
		Vector vDown = -m_InternalData.Placement.vUp;

		Vector vOBBForward = -m_InternalData.Placement.vForward;
		Vector vOBBRight = -m_InternalData.Placement.vRight;
		Vector vOBBUp = m_InternalData.Placement.vUp;

		//scale the extents to usable sizes
		vOBBForward *= PORTAL_WALL_FARDIST / 2.0f;
		vOBBRight *= PORTAL_WALL_FARDIST * 2.0f;
		vOBBUp *= PORTAL_WALL_FARDIST * 2.0f;

		Vector ptOBBOrigin = m_InternalData.Placement.ptCenter;
		ptOBBOrigin -= vOBBRight / 2.0f;
		ptOBBOrigin -= vOBBUp / 2.0f;

		Vector vAABBMins, vAABBMaxs;
		vAABBMins = vAABBMaxs = ptOBBOrigin;

		for( int i = 1; i != 8; ++i )
		{
			Vector ptTest = ptOBBOrigin;
			if( i & (1 << 0) ) ptTest += vOBBForward;
			if( i & (1 << 1) ) ptTest += vOBBRight;
			if( i & (1 << 2) ) ptTest += vOBBUp;

			if( ptTest.x < vAABBMins.x ) vAABBMins.x = ptTest.x;
			if( ptTest.y < vAABBMins.y ) vAABBMins.y = ptTest.y;
			if( ptTest.z < vAABBMins.z ) vAABBMins.z = ptTest.z;
			if( ptTest.x > vAABBMaxs.x ) vAABBMaxs.x = ptTest.x;
			if( ptTest.y > vAABBMaxs.y ) vAABBMaxs.y = ptTest.y;
			if( ptTest.z > vAABBMaxs.z ) vAABBMaxs.z = ptTest.z;
		}


		float fPlanes[6 * 4];

		//first and second planes are always forward and backward planes
		fPlanes[(0*4) + 0] = fWallClipPlane_Forward[0];
		fPlanes[(0*4) + 1] = fWallClipPlane_Forward[1];
		fPlanes[(0*4) + 2] = fWallClipPlane_Forward[2];
		fPlanes[(0*4) + 3] = fWallClipPlane_Forward[3] - PORTAL_WALL_TUBE_OFFSET;

		fPlanes[(1*4) + 0] = vBackward.x;
		fPlanes[(1*4) + 1] = vBackward.y;
		fPlanes[(1*4) + 2] = vBackward.z;
		float fTubeDepthDist = vBackward.Dot( m_InternalData.Placement.ptCenter + (vBackward * (PORTAL_WALL_TUBE_DEPTH + PORTAL_WALL_TUBE_OFFSET)) );
		fPlanes[(1*4) + 3] = fTubeDepthDist;


		//the remaining planes will always have the same ordering of normals, with different distances plugged in for each convex we're creating
		//normal order is up, down, left, right

		fPlanes[(2*4) + 0] = m_InternalData.Placement.vUp.x;
		fPlanes[(2*4) + 1] = m_InternalData.Placement.vUp.y;
		fPlanes[(2*4) + 2] = m_InternalData.Placement.vUp.z;
		fPlanes[(2*4) + 3] = m_InternalData.Placement.vUp.Dot( m_InternalData.Placement.ptCenter + (m_InternalData.Placement.vUp * PORTAL_HOLE_HALF_HEIGHT) );

		fPlanes[(3*4) + 0] = vDown.x;
		fPlanes[(3*4) + 1] = vDown.y;
		fPlanes[(3*4) + 2] = vDown.z;
		fPlanes[(3*4) + 3] = vDown.Dot( m_InternalData.Placement.ptCenter + (vDown * PORTAL_HOLE_HALF_HEIGHT) );

		fPlanes[(4*4) + 0] = vLeft.x;
		fPlanes[(4*4) + 1] = vLeft.y;
		fPlanes[(4*4) + 2] = vLeft.z;
		fPlanes[(4*4) + 3] = vLeft.Dot( m_InternalData.Placement.ptCenter + (vLeft * PORTAL_HOLE_HALF_WIDTH) );

		fPlanes[(5*4) + 0] = m_InternalData.Placement.vRight.x;
		fPlanes[(5*4) + 1] = m_InternalData.Placement.vRight.y;
		fPlanes[(5*4) + 2] = m_InternalData.Placement.vRight.z;
		fPlanes[(5*4) + 3] = m_InternalData.Placement.vRight.Dot( m_InternalData.Placement.ptCenter + (m_InternalData.Placement.vRight * PORTAL_HOLE_HALF_WIDTH) );

		float *fSidePlanesOnly = &fPlanes[(2*4)];

		//these 2 get re-used a bit
		float fFarRightPlaneDistance = m_InternalData.Placement.vRight.Dot( m_InternalData.Placement.ptCenter + m_InternalData.Placement.vRight * (PORTAL_WALL_FARDIST * 10.0f) );
		float fFarLeftPlaneDistance = vLeft.Dot( m_InternalData.Placement.ptCenter + vLeft * (PORTAL_WALL_FARDIST * 10.0f) );


		CUtlVector<int> WallBrushes;
		CUtlVector<CPolyhedron *> WallBrushPolyhedrons_ClippedToWall;
		CPolyhedron **pWallClippedPolyhedrons = NULL;
		int iWallClippedPolyhedronCount = 0;
		if( IsSimulatingVPhysics() ) //if not simulating vphysics, we skip making the entire wall, and just create the minimal tube instead
		{
			enginetrace->GetBrushesInAABB( vAABBMins, vAABBMaxs, &WallBrushes, MASK_SOLID_BRUSHONLY );

			if( WallBrushes.Count() != 0 )
				ConvertBrushListToClippedPolyhedronList( WallBrushes.Base(), WallBrushes.Count(), fPlanes, 1, PORTAL_POLYHEDRON_CUT_EPSILON, &WallBrushPolyhedrons_ClippedToWall );
			
			if( WallBrushPolyhedrons_ClippedToWall.Count() != 0 )
			{
				for( int i = WallBrushPolyhedrons_ClippedToWall.Count(); --i >= 0; )
				{
					CPolyhedron *pPolyhedron = ClipPolyhedron( WallBrushPolyhedrons_ClippedToWall[i], fSidePlanesOnly, 4, PORTAL_POLYHEDRON_CUT_EPSILON, true );
					if( pPolyhedron )
					{
						//a chunk of this brush passes through the hole, not eligible to be removed from cutting
						pPolyhedron->Release();
					}
					else
					{
						//no part of this brush interacts with the hole, no point in cutting the brush any later
						m_InternalData.Simulation.Static.Wall.Local.Brushes.Polyhedrons.AddToTail( WallBrushPolyhedrons_ClippedToWall[i] );
						WallBrushPolyhedrons_ClippedToWall.FastRemove( i );
					}
				}

				if( WallBrushPolyhedrons_ClippedToWall.Count() != 0 ) //might have become 0 while removing uncut brushes
				{
					pWallClippedPolyhedrons = WallBrushPolyhedrons_ClippedToWall.Base();
					iWallClippedPolyhedronCount = WallBrushPolyhedrons_ClippedToWall.Count();
				}
			}
		}


		//upper wall
		{
			//minimal portion that extends into the hole space
			//fPlanes[(1*4) + 3] = fTubeDepthDist;
			fPlanes[(2*4) + 3] = m_InternalData.Placement.vUp.Dot( m_InternalData.Placement.ptCenter + m_InternalData.Placement.vUp * (PORTAL_HOLE_HALF_HEIGHT + PORTAL_WALL_MIN_THICKNESS) );
			fPlanes[(3*4) + 3] = vDown.Dot( m_InternalData.Placement.ptCenter + m_InternalData.Placement.vUp * PORTAL_HOLE_HALF_HEIGHT );
			fPlanes[(4*4) + 3] = vLeft.Dot( m_InternalData.Placement.ptCenter + vLeft * (PORTAL_HOLE_HALF_WIDTH + PORTAL_WALL_MIN_THICKNESS) );
			fPlanes[(5*4) + 3] = m_InternalData.Placement.vRight.Dot( m_InternalData.Placement.ptCenter + m_InternalData.Placement.vRight * (PORTAL_HOLE_HALF_WIDTH + PORTAL_WALL_MIN_THICKNESS) );

			CPolyhedron *pTubePolyhedron = GeneratePolyhedronFromPlanes( fPlanes, 6, PORTAL_POLYHEDRON_CUT_EPSILON );
			if( pTubePolyhedron )
				m_InternalData.Simulation.Static.Wall.Local.Tube.Polyhedrons.AddToTail( pTubePolyhedron );

			//general hole cut
			//fPlanes[(1*4) + 3] += 2000.0f;
			fPlanes[(2*4) + 3] = m_InternalData.Placement.vUp.Dot( m_InternalData.Placement.ptCenter + m_InternalData.Placement.vUp * (PORTAL_WALL_FARDIST * 10.0f) );
			fPlanes[(3*4) + 3] = vDown.Dot( m_InternalData.Placement.ptCenter + m_InternalData.Placement.vUp * (PORTAL_HOLE_HALF_HEIGHT + PORTAL_WALL_MIN_THICKNESS) );
			fPlanes[(4*4) + 3] = fFarLeftPlaneDistance;
			fPlanes[(5*4) + 3] = fFarRightPlaneDistance;

			

			ClipPolyhedrons( pWallClippedPolyhedrons, iWallClippedPolyhedronCount, fSidePlanesOnly, 4, PORTAL_POLYHEDRON_CUT_EPSILON, &m_InternalData.Simulation.Static.Wall.Local.Brushes.Polyhedrons );
		}

		//lower wall
		{
			//minimal portion that extends into the hole space
			//fPlanes[(1*4) + 3] = fTubeDepthDist;
			fPlanes[(2*4) + 3] = m_InternalData.Placement.vUp.Dot( m_InternalData.Placement.ptCenter + (vDown * PORTAL_HOLE_HALF_HEIGHT) );
			fPlanes[(3*4) + 3] = vDown.Dot( m_InternalData.Placement.ptCenter + vDown * (PORTAL_HOLE_HALF_HEIGHT + PORTAL_WALL_MIN_THICKNESS) );
			fPlanes[(4*4) + 3] = vLeft.Dot( m_InternalData.Placement.ptCenter + vLeft * (PORTAL_HOLE_HALF_WIDTH + PORTAL_WALL_MIN_THICKNESS) );
			fPlanes[(5*4) + 3] = m_InternalData.Placement.vRight.Dot( m_InternalData.Placement.ptCenter + m_InternalData.Placement.vRight * (PORTAL_HOLE_HALF_WIDTH + PORTAL_WALL_MIN_THICKNESS) );

			CPolyhedron *pTubePolyhedron = GeneratePolyhedronFromPlanes( fPlanes, 6, PORTAL_POLYHEDRON_CUT_EPSILON );
			if( pTubePolyhedron )
				m_InternalData.Simulation.Static.Wall.Local.Tube.Polyhedrons.AddToTail( pTubePolyhedron );

			//general hole cut
			//fPlanes[(1*4) + 3] += 2000.0f;
			fPlanes[(2*4) + 3] = m_InternalData.Placement.vUp.Dot( m_InternalData.Placement.ptCenter + (vDown * (PORTAL_HOLE_HALF_HEIGHT + PORTAL_WALL_MIN_THICKNESS)) );
			fPlanes[(3*4) + 3] = vDown.Dot( m_InternalData.Placement.ptCenter + (vDown * (PORTAL_WALL_FARDIST * 10.0f)) );
			fPlanes[(4*4) + 3] = fFarLeftPlaneDistance;
			fPlanes[(5*4) + 3] = fFarRightPlaneDistance;

			ClipPolyhedrons( pWallClippedPolyhedrons, iWallClippedPolyhedronCount, fSidePlanesOnly, 4, PORTAL_POLYHEDRON_CUT_EPSILON, &m_InternalData.Simulation.Static.Wall.Local.Brushes.Polyhedrons );
		}

		//left wall
		{
			//minimal portion that extends into the hole space
			//fPlanes[(1*4) + 3] = fTubeDepthDist;
			fPlanes[(2*4) + 3] = m_InternalData.Placement.vUp.Dot( m_InternalData.Placement.ptCenter + (m_InternalData.Placement.vUp * PORTAL_HOLE_HALF_HEIGHT) );
			fPlanes[(3*4) + 3] = vDown.Dot( m_InternalData.Placement.ptCenter + (vDown * PORTAL_HOLE_HALF_HEIGHT) );
			fPlanes[(4*4) + 3] = vLeft.Dot( m_InternalData.Placement.ptCenter + (vLeft * (PORTAL_HOLE_HALF_WIDTH + PORTAL_WALL_MIN_THICKNESS)) );
			fPlanes[(5*4) + 3] = m_InternalData.Placement.vRight.Dot( m_InternalData.Placement.ptCenter + (vLeft * PORTAL_HOLE_HALF_WIDTH) );

			CPolyhedron *pTubePolyhedron = GeneratePolyhedronFromPlanes( fPlanes, 6, PORTAL_POLYHEDRON_CUT_EPSILON );
			if( pTubePolyhedron )
				m_InternalData.Simulation.Static.Wall.Local.Tube.Polyhedrons.AddToTail( pTubePolyhedron );

			//general hole cut
			//fPlanes[(1*4) + 3] += 2000.0f;
			fPlanes[(2*4) + 3] = m_InternalData.Placement.vUp.Dot( m_InternalData.Placement.ptCenter + (m_InternalData.Placement.vUp * (PORTAL_HOLE_HALF_HEIGHT + PORTAL_WALL_MIN_THICKNESS)) );
			fPlanes[(3*4) + 3] = vDown.Dot( m_InternalData.Placement.ptCenter - (m_InternalData.Placement.vUp * (PORTAL_HOLE_HALF_HEIGHT + PORTAL_WALL_MIN_THICKNESS)) );
			fPlanes[(4*4) + 3] = fFarLeftPlaneDistance;
			fPlanes[(5*4) + 3] = m_InternalData.Placement.vRight.Dot( m_InternalData.Placement.ptCenter + (vLeft * (PORTAL_HOLE_HALF_WIDTH + PORTAL_WALL_MIN_THICKNESS)) );

			ClipPolyhedrons( pWallClippedPolyhedrons, iWallClippedPolyhedronCount, fSidePlanesOnly, 4, PORTAL_POLYHEDRON_CUT_EPSILON, &m_InternalData.Simulation.Static.Wall.Local.Brushes.Polyhedrons );
		}

		//right wall
		{
			//minimal portion that extends into the hole space
			//fPlanes[(1*4) + 3] = fTubeDepthDist;
			fPlanes[(2*4) + 3] = m_InternalData.Placement.vUp.Dot( m_InternalData.Placement.ptCenter + (m_InternalData.Placement.vUp * (PORTAL_HOLE_HALF_HEIGHT)) );
			fPlanes[(3*4) + 3] = vDown.Dot( m_InternalData.Placement.ptCenter + (vDown * (PORTAL_HOLE_HALF_HEIGHT)) );
			fPlanes[(4*4) + 3] = vLeft.Dot( m_InternalData.Placement.ptCenter + m_InternalData.Placement.vRight * PORTAL_HOLE_HALF_WIDTH );
			fPlanes[(5*4) + 3] = m_InternalData.Placement.vRight.Dot( m_InternalData.Placement.ptCenter + m_InternalData.Placement.vRight * (PORTAL_HOLE_HALF_WIDTH + PORTAL_WALL_MIN_THICKNESS) );

			CPolyhedron *pTubePolyhedron = GeneratePolyhedronFromPlanes( fPlanes, 6, PORTAL_POLYHEDRON_CUT_EPSILON );
			if( pTubePolyhedron )
				m_InternalData.Simulation.Static.Wall.Local.Tube.Polyhedrons.AddToTail( pTubePolyhedron );

			//general hole cut
			//fPlanes[(1*4) + 3] += 2000.0f;
			fPlanes[(2*4) + 3] = m_InternalData.Placement.vUp.Dot( m_InternalData.Placement.ptCenter + (m_InternalData.Placement.vUp * (PORTAL_HOLE_HALF_HEIGHT + PORTAL_WALL_MIN_THICKNESS)) );
			fPlanes[(3*4) + 3] = vDown.Dot( m_InternalData.Placement.ptCenter + (vDown * (PORTAL_HOLE_HALF_HEIGHT + PORTAL_WALL_MIN_THICKNESS)) );
			fPlanes[(4*4) + 3] = vLeft.Dot( m_InternalData.Placement.ptCenter + m_InternalData.Placement.vRight * (PORTAL_HOLE_HALF_WIDTH + PORTAL_WALL_MIN_THICKNESS) );
			fPlanes[(5*4) + 3] = fFarRightPlaneDistance;

			ClipPolyhedrons( pWallClippedPolyhedrons, iWallClippedPolyhedronCount, fSidePlanesOnly, 4, PORTAL_POLYHEDRON_CUT_EPSILON, &m_InternalData.Simulation.Static.Wall.Local.Brushes.Polyhedrons );
		}

		for( int i = WallBrushPolyhedrons_ClippedToWall.Count(); --i >= 0; )
			WallBrushPolyhedrons_ClippedToWall[i]->Release();

		WallBrushPolyhedrons_ClippedToWall.RemoveAll();
	}

	STOPDEBUGTIMER( functionTimer );
	DECREMENTTABSPACING();
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::CreatePolyhedrons() FINISH: %fms\n", GetPortalSimulatorGUID(), TABSPACING, functionTimer.GetDuration().GetMillisecondsF() ); );

	m_CreationChecklist.bPolyhedronsGenerated = true;
}



void CPortalSimulator::ClearPolyhedrons( void )
{
	if( m_CreationChecklist.bPolyhedronsGenerated == false )
		return;

	CREATEDEBUGTIMER( functionTimer );

	STARTDEBUGTIMER( functionTimer );
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::ClearPolyhedrons() START\n", GetPortalSimulatorGUID(), TABSPACING ); );
	INCREMENTTABSPACING();
	
	if( m_InternalData.Simulation.Static.World.Brushes.Polyhedrons.Count() != 0 )
	{
		for( int i = m_InternalData.Simulation.Static.World.Brushes.Polyhedrons.Count(); --i >= 0; )
			m_InternalData.Simulation.Static.World.Brushes.Polyhedrons[i]->Release();
		
		m_InternalData.Simulation.Static.World.Brushes.Polyhedrons.RemoveAll();
	}

	if( m_InternalData.Simulation.Static.World.StaticProps.Polyhedrons.Count() != 0 )
	{
		for( int i = m_InternalData.Simulation.Static.World.StaticProps.Polyhedrons.Count(); --i >= 0; )
			m_InternalData.Simulation.Static.World.StaticProps.Polyhedrons[i]->Release();

		m_InternalData.Simulation.Static.World.StaticProps.Polyhedrons.RemoveAll();		
	}
#ifdef _DEBUG
	for( int i = m_InternalData.Simulation.Static.World.StaticProps.ClippedRepresentations.Count(); --i >= 0; )
	{
#ifndef CLIENT_DLL
		Assert( m_InternalData.Simulation.Static.World.StaticProps.ClippedRepresentations[i].pPhysicsObject == NULL );
#endif
		Assert( m_InternalData.Simulation.Static.World.StaticProps.ClippedRepresentations[i].pCollide == NULL );
	}
#endif
	m_InternalData.Simulation.Static.World.StaticProps.ClippedRepresentations.RemoveAll();

	if( m_InternalData.Simulation.Static.Wall.Local.Brushes.Polyhedrons.Count() != 0 )
	{
		for( int i = m_InternalData.Simulation.Static.Wall.Local.Brushes.Polyhedrons.Count(); --i >= 0; )
			m_InternalData.Simulation.Static.Wall.Local.Brushes.Polyhedrons[i]->Release();

		m_InternalData.Simulation.Static.Wall.Local.Brushes.Polyhedrons.RemoveAll();
	}

	if( m_InternalData.Simulation.Static.Wall.Local.Tube.Polyhedrons.Count() != 0 )
	{
		for( int i = m_InternalData.Simulation.Static.Wall.Local.Tube.Polyhedrons.Count(); --i >= 0; )
			m_InternalData.Simulation.Static.Wall.Local.Tube.Polyhedrons[i]->Release();

		m_InternalData.Simulation.Static.Wall.Local.Tube.Polyhedrons.RemoveAll();
	}

	STOPDEBUGTIMER( functionTimer );
	DECREMENTTABSPACING();
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::ClearPolyhedrons() FINISH: %fms\n", GetPortalSimulatorGUID(), TABSPACING, functionTimer.GetDuration().GetMillisecondsF() ); );

	m_CreationChecklist.bPolyhedronsGenerated = false;
}



void CPortalSimulator::DetachFromLinked( void )
{
	if( m_pLinkedPortal == NULL )
		return;

	CREATEDEBUGTIMER( functionTimer );

	STARTDEBUGTIMER( functionTimer );
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::DetachFromLinked() START\n", GetPortalSimulatorGUID(), TABSPACING ); );
	INCREMENTTABSPACING();
	
	//IMPORTANT: Physics objects must be destroyed before their associated collision data or a fairly cryptic crash will ensue
#ifndef CLIENT_DLL
	ClearLinkedEntities();
	ClearLinkedPhysics();
#endif
	ClearLinkedCollision();

	if( m_pLinkedPortal->m_bInCrossLinkedFunction == false )
	{
		Assert( m_bInCrossLinkedFunction == false ); //I'm pretty sure switching to a stack would have negative repercussions
		m_bInCrossLinkedFunction = true;
		m_pLinkedPortal->DetachFromLinked();
		m_bInCrossLinkedFunction = false;
	}

	m_pLinkedPortal = NULL;

	STOPDEBUGTIMER( functionTimer );
	DECREMENTTABSPACING();
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::DetachFromLinked() FINISH: %fms\n", GetPortalSimulatorGUID(), TABSPACING, functionTimer.GetDuration().GetMillisecondsF() ); );
}

void CPortalSimulator::SetPortalSimulatorCallbacks( CPortalSimulatorEventCallbacks *pCallbacks )
{
	if( pCallbacks )
		m_pCallbacks = pCallbacks;
	else
		m_pCallbacks = &s_DummyPortalSimulatorCallback; //always keep the pointer valid
}




void CPortalSimulator::SetVPhysicsSimulationEnabled( bool bEnabled )
{
	AssertMsg( (m_pLinkedPortal == NULL) || (m_pLinkedPortal->m_bSimulateVPhysics == m_bSimulateVPhysics), "Linked portals are in disagreement as to whether they would simulate VPhysics." );

	if( bEnabled == m_bSimulateVPhysics )
		return;

	CREATEDEBUGTIMER( functionTimer );

	STARTDEBUGTIMER( functionTimer );
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::SetVPhysicsSimulationEnabled() START\n", GetPortalSimulatorGUID(), TABSPACING ); );
	INCREMENTTABSPACING();
	
	m_bSimulateVPhysics = bEnabled;
	if( bEnabled )
	{
		//we took some local collision shortcuts when generating while physics simulation is off, regenerate
		ClearLocalCollision();
		ClearPolyhedrons();
		CreatePolyhedrons();
		CreateLocalCollision();
#ifndef CLIENT_DLL
		CreateAllPhysics();
#endif
	}
#ifndef CLIENT_DLL
	else
	{
		ClearAllPhysics();
	}
#endif

	if( m_pLinkedPortal && (m_pLinkedPortal->m_bInCrossLinkedFunction == false) )
	{
		Assert( m_bInCrossLinkedFunction == false ); //I'm pretty sure switching to a stack would have negative repercussions
		m_bInCrossLinkedFunction = true;
		m_pLinkedPortal->SetVPhysicsSimulationEnabled( bEnabled );
		m_bInCrossLinkedFunction = false;
	}

	STOPDEBUGTIMER( functionTimer );
	DECREMENTTABSPACING();
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::SetVPhysicsSimulationEnabled() FINISH: %fms\n", GetPortalSimulatorGUID(), TABSPACING, functionTimer.GetDuration().GetMillisecondsF() ); );
}


#ifndef CLIENT_DLL
void CPortalSimulator::PrePhysFrame( void )
{
	int iPortalSimulators = s_PortalSimulators.Count();

	if( iPortalSimulators != 0 )
	{
		CPortalSimulator **pAllSimulators = s_PortalSimulators.Base();
		for( int i = 0; i != iPortalSimulators; ++i )
		{
			CPortalSimulator *pSimulator = pAllSimulators[i];
			if( !pSimulator->IsReadyToSimulate() )
				continue;

			int iOwnedEntities = pSimulator->m_InternalData.Simulation.Dynamic.OwnedEntities.Count();
			if( iOwnedEntities != 0 )
			{
				CBaseEntity **pOwnedEntities = pSimulator->m_InternalData.Simulation.Dynamic.OwnedEntities.Base();

				for( int j = 0; j != iOwnedEntities; ++j )
				{
					CBaseEntity *pEntity = pOwnedEntities[j];
					if( CPhysicsShadowClone::IsShadowClone( pEntity ) )
						continue;

					Assert( (pEntity != NULL) && (pEntity->IsMarkedForDeletion() == false) );
					IPhysicsObject *pPhysObject = pEntity->VPhysicsGetObject();
					if( (pPhysObject == NULL) || pPhysObject->IsAsleep() )
						continue;

					int iEntIndex = pEntity->entindex();
					int iExistingFlags = pSimulator->m_InternalData.Simulation.Dynamic.EntFlags[iEntIndex];
					if( pSimulator->EntityIsInPortalHole( pEntity ) )
						pSimulator->m_InternalData.Simulation.Dynamic.EntFlags[iEntIndex] |= PSEF_IS_IN_PORTAL_HOLE;
					else
						pSimulator->m_InternalData.Simulation.Dynamic.EntFlags[iEntIndex] &= ~PSEF_IS_IN_PORTAL_HOLE;

					UpdateShadowClonesPortalSimulationFlags( pEntity, PSEF_IS_IN_PORTAL_HOLE, pSimulator->m_InternalData.Simulation.Dynamic.EntFlags[iEntIndex] );

					if( ((iExistingFlags ^ pSimulator->m_InternalData.Simulation.Dynamic.EntFlags[iEntIndex]) & PSEF_IS_IN_PORTAL_HOLE) != 0 ) //value changed
					{
						pEntity->CollisionRulesChanged(); //entity moved into or out of the portal hole, need to either add or remove collision with transformed geometry

						CPhysicsShadowCloneLL *pClones = CPhysicsShadowClone::GetClonesOfEntity( pEntity );
						while( pClones )
						{
							pClones->pClone->CollisionRulesChanged();
							pClones = pClones->pNext;
						}
					}
				}
			}
		}
	}
}

void CPortalSimulator::PostPhysFrame( void )
{
	if ( g_bPlayerIsInSimulator )
	{
		CPortal_Player* pPlayer = dynamic_cast<CPortal_Player*>( UTIL_GetLocalPlayer() );
		CProp_Portal* pTouchedPortal = pPlayer->m_hPortalEnvironment.Get();
		CPortalSimulator* pSim = GetSimulatorThatOwnsEntity( pPlayer );
		if ( pTouchedPortal && pSim && (pTouchedPortal->m_PortalSimulator.GetPortalSimulatorGUID() != pSim->GetPortalSimulatorGUID()) )
		{
			Warning ( "Player is simulated in a physics environment but isn't touching a portal! Can't teleport, but can fall through portal hole. Returning player to main environment.\n" );
			ADD_DEBUG_HISTORY( HISTORY_PLAYER_DAMAGE, UTIL_VarArgs( "Player in PortalSimulator but not touching a portal, removing from sim at : %f\n",  gpGlobals->curtime ) );
			
			if ( pSim )
			{
				pSim->ReleaseOwnershipOfEntity( pPlayer, false );
			}
		}
	}
}
#endif //#ifndef CLIENT_DLL


#ifndef CLIENT_DLL
int CPortalSimulator::GetMoveableOwnedEntities( CBaseEntity **pEntsOut, int iEntOutLimit )
{
	int iOwnedEntCount = m_InternalData.Simulation.Dynamic.OwnedEntities.Count();
	int iOutputCount = 0;

	for( int i = 0; i != iOwnedEntCount; ++i )
	{
		CBaseEntity *pEnt = m_InternalData.Simulation.Dynamic.OwnedEntities[i];
		Assert( pEnt != NULL );

		if( CPhysicsShadowClone::IsShadowClone( pEnt ) )
			continue;

		if( CPSCollisionEntity::IsPortalSimulatorCollisionEntity( pEnt ) )
			continue;

		if( pEnt->GetMoveType() == MOVETYPE_NONE )
			continue;

        pEntsOut[iOutputCount] = pEnt;
		++iOutputCount;

		if( iOutputCount == iEntOutLimit )
			break;
	}

	return iOutputCount;
}

CPortalSimulator *CPortalSimulator::GetSimulatorThatOwnsEntity( const CBaseEntity *pEntity )
{
#ifdef _DEBUG
	int iEntIndex = pEntity->entindex();
	CPortalSimulator *pOwningSimulatorCheck = NULL;

	for( int i = s_PortalSimulators.Count(); --i >= 0; )
	{
		if( s_PortalSimulators[i]->m_InternalData.Simulation.Dynamic.EntFlags[iEntIndex] & PSEF_OWNS_ENTITY )
		{
			AssertMsg( pOwningSimulatorCheck == NULL, "More than one portal simulator found owning the same entity." );
			pOwningSimulatorCheck = s_PortalSimulators[i];
		}
	}

	AssertMsg( pOwningSimulatorCheck == s_OwnedEntityMap[iEntIndex], "Owned entity mapping out of sync with individual simulator ownership flags." );
#endif

	return s_OwnedEntityMap[pEntity->entindex()];
}

CPortalSimulator *CPortalSimulator::GetSimulatorThatCreatedPhysicsObject( const IPhysicsObject *pObject, PS_PhysicsObjectSourceType_t *pOut_SourceType )
{
	for( int i = s_PortalSimulators.Count(); --i >= 0; )
	{
		if( s_PortalSimulators[i]->CreatedPhysicsObject( pObject, pOut_SourceType ) )
			return s_PortalSimulators[i];
	}
	
	return NULL;
}

bool CPortalSimulator::CreatedPhysicsObject( const IPhysicsObject *pObject, PS_PhysicsObjectSourceType_t *pOut_SourceType ) const
{
	if( (pObject == m_InternalData.Simulation.Static.World.Brushes.pPhysicsObject) || (pObject == m_InternalData.Simulation.Static.Wall.Local.Brushes.pPhysicsObject) )
	{
		if( pOut_SourceType )
			*pOut_SourceType = PSPOST_LOCAL_BRUSHES;

		return true;
	}

	if( pObject == m_InternalData.Simulation.Static.Wall.RemoteTransformedToLocal.Brushes.pPhysicsObject )
	{
		if( pOut_SourceType )
			*pOut_SourceType = PSPOST_REMOTE_BRUSHES;

		return true;
	}

	for( int i = m_InternalData.Simulation.Static.World.StaticProps.ClippedRepresentations.Count(); --i >= 0; )
	{
		if( m_InternalData.Simulation.Static.World.StaticProps.ClippedRepresentations[i].pPhysicsObject == pObject )
		{
			if( pOut_SourceType )
				*pOut_SourceType = PSPOST_LOCAL_STATICPROPS;
			return true;
		}
	}

	for( int i = m_InternalData.Simulation.Static.Wall.RemoteTransformedToLocal.StaticProps.PhysicsObjects.Count(); --i >= 0; )
	{
		if( m_InternalData.Simulation.Static.Wall.RemoteTransformedToLocal.StaticProps.PhysicsObjects[i] == pObject )
		{
			if( pOut_SourceType )
				*pOut_SourceType = PSPOST_REMOTE_STATICPROPS;

			return true;
		}
	}

	if( pObject == m_InternalData.Simulation.Static.Wall.Local.Tube.pPhysicsObject )
	{
		if( pOut_SourceType )
			*pOut_SourceType = PSPOST_HOLYWALL_TUBE;

		return true;
	}	

	return false;
}
#endif //#ifndef CLIENT_DLL








static void ConvertBrushListToClippedPolyhedronList( const int *pBrushes, int iBrushCount, const float *pOutwardFacingClipPlanes, int iClipPlaneCount, float fClipEpsilon, CUtlVector<CPolyhedron *> *pPolyhedronList )
{
	if( pPolyhedronList == NULL )
		return;

	if( (pBrushes == NULL) || (iBrushCount == 0) )
		return;

	for( int i = 0; i != iBrushCount; ++i )
	{
		CPolyhedron *pPolyhedron = ClipPolyhedron( g_StaticCollisionPolyhedronCache.GetBrushPolyhedron( pBrushes[i] ), pOutwardFacingClipPlanes, iClipPlaneCount, fClipEpsilon );
		if( pPolyhedron )
			pPolyhedronList->AddToTail( pPolyhedron );
	}
}

static void ClipPolyhedrons( CPolyhedron * const *pExistingPolyhedrons, int iPolyhedronCount, const float *pOutwardFacingClipPlanes, int iClipPlaneCount, float fClipEpsilon, CUtlVector<CPolyhedron *> *pPolyhedronList )
{
	if( pPolyhedronList == NULL )
		return;

	if( (pExistingPolyhedrons == NULL) || (iPolyhedronCount == 0) )
		return;

	for( int i = 0; i != iPolyhedronCount; ++i )
	{
		CPolyhedron *pPolyhedron = ClipPolyhedron( pExistingPolyhedrons[i], pOutwardFacingClipPlanes, iClipPlaneCount, fClipEpsilon );
		if( pPolyhedron )
			pPolyhedronList->AddToTail( pPolyhedron );
	}
}

static CPhysCollide *ConvertPolyhedronsToCollideable( CPolyhedron **pPolyhedrons, int iPolyhedronCount )
{
	if( (pPolyhedrons == NULL) || (iPolyhedronCount == 0 ) )
		return NULL;

	CREATEDEBUGTIMER( functionTimer );

	STARTDEBUGTIMER( functionTimer );
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sConvertPolyhedronsToCollideable() START\n", s_iPortalSimulatorGUID, TABSPACING ); );
	INCREMENTTABSPACING();

	CPhysConvex **pConvexes = (CPhysConvex **)stackalloc( iPolyhedronCount * sizeof( CPhysConvex * ) );
	int iConvexCount = 0;

	CREATEDEBUGTIMER( convexTimer );
	STARTDEBUGTIMER( convexTimer );
	for( int i = 0; i != iPolyhedronCount; ++i )
	{
		pConvexes[iConvexCount] = physcollision->ConvexFromConvexPolyhedron( *pPolyhedrons[i] );

		Assert( pConvexes[iConvexCount] != NULL );
		
		if( pConvexes[iConvexCount] )
			++iConvexCount;		
	}
	STOPDEBUGTIMER( convexTimer );
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sConvex Generation:%fms\n", s_iPortalSimulatorGUID, TABSPACING, convexTimer.GetDuration().GetMillisecondsF() ); );


	CPhysCollide *pReturn;
	if( iConvexCount != 0 )
	{
		CREATEDEBUGTIMER( collideTimer );
		STARTDEBUGTIMER( collideTimer );
		pReturn = physcollision->ConvertConvexToCollide( pConvexes, iConvexCount );
		STOPDEBUGTIMER( collideTimer );
		DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCollideable Generation:%fms\n", s_iPortalSimulatorGUID, TABSPACING, collideTimer.GetDuration().GetMillisecondsF() ); );
	}
	else
	{
		pReturn = NULL;
	}

	STOPDEBUGTIMER( functionTimer );
	DECREMENTTABSPACING();
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sConvertPolyhedronsToCollideable() FINISH: %fms\n", s_iPortalSimulatorGUID, TABSPACING, functionTimer.GetDuration().GetMillisecondsF() ); );

	return pReturn;
}


static inline CPolyhedron *TransformAndClipSinglePolyhedron( CPolyhedron *pExistingPolyhedron, const VMatrix &Transform, const float *pOutwardFacingClipPlanes, int iClipPlaneCount, float fCutEpsilon, bool bUseTempMemory )
{
	Vector *pTempPointArray = (Vector *)stackalloc( sizeof( Vector ) * pExistingPolyhedron->iVertexCount );
	Polyhedron_IndexedPolygon_t *pTempPolygonArray = (Polyhedron_IndexedPolygon_t *)stackalloc( sizeof( Polyhedron_IndexedPolygon_t ) * pExistingPolyhedron->iPolygonCount );

	Polyhedron_IndexedPolygon_t *pOriginalPolygons = pExistingPolyhedron->pPolygons;
	pExistingPolyhedron->pPolygons = pTempPolygonArray;

	Vector *pOriginalPoints = pExistingPolyhedron->pVertices;
	pExistingPolyhedron->pVertices = pTempPointArray;

	for( int j = 0; j != pExistingPolyhedron->iPolygonCount; ++j )
	{
		pTempPolygonArray[j].iFirstIndex = pOriginalPolygons[j].iFirstIndex;
		pTempPolygonArray[j].iIndexCount = pOriginalPolygons[j].iIndexCount;
		pTempPolygonArray[j].polyNormal = Transform.ApplyRotation( pOriginalPolygons[j].polyNormal );
	}

	for( int j = 0; j != pExistingPolyhedron->iVertexCount; ++j )
	{
		pTempPointArray[j] = Transform * pOriginalPoints[j];
	}

	CPolyhedron *pNewPolyhedron = ClipPolyhedron( pExistingPolyhedron, pOutwardFacingClipPlanes, iClipPlaneCount, fCutEpsilon, bUseTempMemory ); //copy the polyhedron

	//restore the original polyhedron to its former self
	pExistingPolyhedron->pVertices = pOriginalPoints;
	pExistingPolyhedron->pPolygons = pOriginalPolygons;

	return pNewPolyhedron;
}

static int GetEntityPhysicsObjects( IPhysicsEnvironment *pEnvironment, CBaseEntity *pEntity, IPhysicsObject **pRetList, int iRetListArraySize )
{	
	int iCount, iRetCount = 0;
	const IPhysicsObject **pList = pEnvironment->GetObjectList( &iCount );

	if( iCount > iRetListArraySize )
		iCount = iRetListArraySize;

	for ( int i = 0; i < iCount; ++i )
	{
		CBaseEntity *pEnvEntity = reinterpret_cast<CBaseEntity *>(pList[i]->GetGameData());
		if ( pEntity == pEnvEntity )
		{
			pRetList[iRetCount] = (IPhysicsObject *)(pList[i]);
			++iRetCount;
		}
	}

	return iRetCount;
}



#ifndef CLIENT_DLL
//Move all entities back to the main environment for removal, and make sure the main environment is in control during the UTIL_Remove process
struct UTIL_Remove_PhysicsStack_t
{
	IPhysicsEnvironment *pPhysicsEnvironment;
	CEntityList *pShadowList;
};
static CUtlVector<UTIL_Remove_PhysicsStack_t> s_UTIL_Remove_PhysicsStack;

void CPortalSimulator::Pre_UTIL_Remove( CBaseEntity *pEntity )
{
	int index = s_UTIL_Remove_PhysicsStack.AddToTail();
	s_UTIL_Remove_PhysicsStack[index].pPhysicsEnvironment = physenv;
	s_UTIL_Remove_PhysicsStack[index].pShadowList = g_pShadowEntities;
	int iEntIndex = pEntity->entindex();

	//NDebugOverlay::EntityBounds( pEntity, 0, 0, 0, 50, 5.0f );

	if( (CPhysicsShadowClone::IsShadowClone( pEntity ) == false) &&
		(CPSCollisionEntity::IsPortalSimulatorCollisionEntity( pEntity ) == false) )
	{
		CPortalSimulator *pOwningSimulator = CPortalSimulator::GetSimulatorThatOwnsEntity( pEntity );
		if( pOwningSimulator )
		{
			pOwningSimulator->ReleasePhysicsOwnership( pEntity, false );
			pOwningSimulator->ReleaseOwnershipOfEntity( pEntity );
		}

		//might be cloned from main to a few environments
		for( int i = s_PortalSimulators.Count(); --i >= 0; )
			s_PortalSimulators[i]->StopCloningEntity( pEntity );
	}

	for( int i = s_PortalSimulators.Count(); --i >= 0; )
	{
		s_PortalSimulators[i]->m_InternalData.Simulation.Dynamic.EntFlags[iEntIndex] = 0;
	}


	physenv = physenv_main;
	g_pShadowEntities = g_pShadowEntities_Main;
}

void CPortalSimulator::Post_UTIL_Remove( CBaseEntity *pEntity )
{
	int index = s_UTIL_Remove_PhysicsStack.Count() - 1;
	Assert( index >= 0 );
	UTIL_Remove_PhysicsStack_t &PhysicsStackEntry = s_UTIL_Remove_PhysicsStack[index];
	physenv = PhysicsStackEntry.pPhysicsEnvironment;
	g_pShadowEntities = PhysicsStackEntry.pShadowList;
	s_UTIL_Remove_PhysicsStack.FastRemove(index);

#ifdef _DEBUG
	for( int i = CPhysicsShadowClone::g_ShadowCloneList.Count(); --i >= 0; )
	{
		Assert( CPhysicsShadowClone::g_ShadowCloneList[i]->GetClonedEntity() != pEntity ); //shouldn't be any clones of this object anymore
	}
#endif
}

void UpdateShadowClonesPortalSimulationFlags( const CBaseEntity *pSourceEntity, unsigned int iFlags, int iSourceFlags )
{
	unsigned int iOrFlags = iSourceFlags & iFlags;

	CPhysicsShadowCloneLL *pClones = CPhysicsShadowClone::GetClonesOfEntity( pSourceEntity );
	while( pClones )
	{
		CPhysicsShadowClone *pClone = pClones->pClone;
		CPortalSimulator *pCloneSimulator = CPortalSimulator::GetSimulatorThatOwnsEntity( pClone );

		unsigned int *pFlags = (unsigned int *)&pCloneSimulator->m_DataAccess.Simulation.Dynamic.EntFlags[pClone->entindex()];
		*pFlags &= ~iFlags;
		*pFlags |= iOrFlags;

		Assert( ((iSourceFlags ^ *pFlags) & iFlags) == 0 );

		pClones = pClones->pNext;
	}
}
#endif



#ifndef CLIENT_DLL
	class CPS_AutoGameSys_EntityListener : public CAutoGameSystem, public IEntityListener
#else
	class CPS_AutoGameSys_EntityListener : public CAutoGameSystem
#endif
{
public:
	virtual void LevelInitPreEntity( void )
	{
		for( int i = s_PortalSimulators.Count(); --i >= 0; )
			s_PortalSimulators[i]->ClearEverything();
	}

	virtual void LevelShutdownPreEntity( void )
	{
		for( int i = s_PortalSimulators.Count(); --i >= 0; )
			s_PortalSimulators[i]->ClearEverything();
	}

#ifndef CLIENT_DLL
	virtual bool Init( void )
	{
		gEntList.AddListenerEntity( this );
		return true;
	}

	//virtual void OnEntityCreated( CBaseEntity *pEntity ) {}
	virtual void OnEntitySpawned( CBaseEntity *pEntity )
	{

	}
	virtual void OnEntityDeleted( CBaseEntity *pEntity )
	{
		CPortalSimulator *pSimulator = CPortalSimulator::GetSimulatorThatOwnsEntity( pEntity );
		if( pSimulator )
		{
			pSimulator->ReleasePhysicsOwnership( pEntity, false );
			pSimulator->ReleaseOwnershipOfEntity( pEntity );
		}
		Assert( CPortalSimulator::GetSimulatorThatOwnsEntity( pEntity ) == NULL );
	}
#endif //#ifndef CLIENT_DLL
};
static CPS_AutoGameSys_EntityListener s_CPS_AGS_EL_Singleton;





#ifndef CLIENT_DLL
LINK_ENTITY_TO_CLASS( portalsimulator_collisionentity, CPSCollisionEntity );

static bool s_PortalSimulatorCollisionEntities[MAX_EDICTS] = { false };

CPSCollisionEntity::CPSCollisionEntity( void )
{
	m_pOwningSimulator = NULL;
}

CPSCollisionEntity::~CPSCollisionEntity( void )
{
	if( m_pOwningSimulator )
	{
		m_pOwningSimulator->m_InternalData.Simulation.Dynamic.EntFlags[entindex()] &= ~PSEF_OWNS_PHYSICS;
		m_pOwningSimulator->MarkAsReleased( this );
		m_pOwningSimulator->m_InternalData.Simulation.pCollisionEntity = NULL;
		m_pOwningSimulator = NULL;
	}
	s_PortalSimulatorCollisionEntities[entindex()] = false;
}

void CPSCollisionEntity::UpdateOnRemove( void )
{
	VPhysicsSetObject( NULL );
	if( m_pOwningSimulator )
	{
		m_pOwningSimulator->m_InternalData.Simulation.Dynamic.EntFlags[entindex()] &= ~PSEF_OWNS_PHYSICS;
		m_pOwningSimulator->MarkAsReleased( this );
		m_pOwningSimulator->m_InternalData.Simulation.pCollisionEntity = NULL;
		m_pOwningSimulator = NULL;
	}
	s_PortalSimulatorCollisionEntities[entindex()] = false;

	BaseClass::UpdateOnRemove();
}

void CPSCollisionEntity::Spawn( void )
{
	BaseClass::Spawn();
	SetSolid( SOLID_CUSTOM );
	SetMoveType( MOVETYPE_NONE );
	SetCollisionGroup( COLLISION_GROUP_NONE );
	s_PortalSimulatorCollisionEntities[entindex()] = true;
	VPhysicsSetObject( NULL );
	AddFlag( FL_WORLDBRUSH );
	AddEffects( EF_NODRAW | EF_NOSHADOW | EF_NORECEIVESHADOW );
	IncrementInterpolationFrame();
}

void CPSCollisionEntity::Activate( void )
{
	BaseClass::Activate();
	CollisionRulesChanged();
}

int CPSCollisionEntity::ObjectCaps( void )
{
	return ((BaseClass::ObjectCaps() | FCAP_DONT_SAVE) & ~(FCAP_FORCE_TRANSITION | FCAP_ACROSS_TRANSITION | FCAP_MUST_SPAWN | FCAP_SAVE_NON_NETWORKABLE));
}

bool CPSCollisionEntity::ShouldCollide( int collisionGroup, int contentsMask ) const
{
	return GetWorldEntity()->ShouldCollide( collisionGroup, contentsMask );
}

IPhysicsObject *CPSCollisionEntity::VPhysicsGetObject( void )
{
	if( m_pOwningSimulator == NULL )
		return NULL;

	if( m_pOwningSimulator->m_DataAccess.Simulation.Static.World.Brushes.pPhysicsObject != NULL )
		return m_pOwningSimulator->m_DataAccess.Simulation.Static.World.Brushes.pPhysicsObject;
	else if( m_pOwningSimulator->m_DataAccess.Simulation.Static.Wall.Local.Brushes.pPhysicsObject != NULL )
		return m_pOwningSimulator->m_DataAccess.Simulation.Static.Wall.Local.Brushes.pPhysicsObject;
	else if( m_pOwningSimulator->m_DataAccess.Simulation.Static.Wall.Local.Tube.pPhysicsObject != NULL )
		return m_pOwningSimulator->m_DataAccess.Simulation.Static.Wall.Local.Brushes.pPhysicsObject;
	else if( m_pOwningSimulator->m_DataAccess.Simulation.Static.Wall.RemoteTransformedToLocal.Brushes.pPhysicsObject != NULL )
		return m_pOwningSimulator->m_DataAccess.Simulation.Static.Wall.RemoteTransformedToLocal.Brushes.pPhysicsObject;
	else
		return NULL;
}

int CPSCollisionEntity::VPhysicsGetObjectList( IPhysicsObject **pList, int listMax )
{
	if( m_pOwningSimulator == NULL )
		return 0;

	if( (pList == NULL) || (listMax == 0) )
		return 0;

	int iRetVal = 0;

	if( m_pOwningSimulator->m_DataAccess.Simulation.Static.World.Brushes.pPhysicsObject != NULL )
	{
		pList[iRetVal] = m_pOwningSimulator->m_DataAccess.Simulation.Static.World.Brushes.pPhysicsObject;
		++iRetVal;
		if( iRetVal == listMax )
			return iRetVal;
	}

	if( m_pOwningSimulator->m_DataAccess.Simulation.Static.Wall.Local.Brushes.pPhysicsObject != NULL )
	{
		pList[iRetVal] = m_pOwningSimulator->m_DataAccess.Simulation.Static.Wall.Local.Brushes.pPhysicsObject;
		++iRetVal;
		if( iRetVal == listMax )
			return iRetVal;
	}

	if( m_pOwningSimulator->m_DataAccess.Simulation.Static.Wall.Local.Tube.pPhysicsObject != NULL )
	{
		pList[iRetVal] = m_pOwningSimulator->m_DataAccess.Simulation.Static.Wall.Local.Tube.pPhysicsObject;
		++iRetVal;
		if( iRetVal == listMax )
			return iRetVal;
	}

	if( m_pOwningSimulator->m_DataAccess.Simulation.Static.Wall.RemoteTransformedToLocal.Brushes.pPhysicsObject != NULL )
	{
		pList[iRetVal] = m_pOwningSimulator->m_DataAccess.Simulation.Static.Wall.RemoteTransformedToLocal.Brushes.pPhysicsObject;
		++iRetVal;
		if( iRetVal == listMax )
			return iRetVal;
	}

	return iRetVal;
}

bool CPSCollisionEntity::IsPortalSimulatorCollisionEntity( const CBaseEntity *pEntity )
{
	return s_PortalSimulatorCollisionEntities[pEntity->entindex()];
}
#endif //#ifndef CLIENT_DLL








#ifdef DEBUG_PORTAL_COLLISION_ENVIRONMENTS

#include "filesystem.h"

static void PortalSimulatorDumps_DumpCollideToGlView( CPhysCollide *pCollide, const Vector &origin, const QAngle &angles, float fColorScale, const char *pFilename );
static void PortalSimulatorDumps_DumpPlanesToGlView( float *pPlanes, int iPlaneCount, const char *pszFileName );
static void PortalSimulatorDumps_DumpBoxToGlView( const Vector &vMins, const Vector &vMaxs, float fRed, float fGreen, float fBlue, const char *pszFileName );
static void PortalSimulatorDumps_DumpOBBoxToGlView( const Vector &ptOrigin, const Vector &vExtent1, const Vector &vExtent2, const Vector &vExtent3, float fRed, float fGreen, float fBlue, const char *pszFileName );

void DumpActiveCollision( const CPortalSimulator *pPortalSimulator, const char *szFileName )
{
	CREATEDEBUGTIMER( collisionDumpTimer );
	STARTDEBUGTIMER( collisionDumpTimer );
	
	//color coding scheme, static prop collision is brighter than brush collision. Remote world stuff transformed to the local wall is darker than completely local stuff
#define PSDAC_INTENSITY_LOCALBRUSH 0.25f
#define PSDAC_INTENSITY_LOCALPROP 1.0f
#define PSDAC_INTENSITY_REMOTEBRUSH 0.125f
#define PSDAC_INTENSITY_REMOTEPROP 0.5f

	if( pPortalSimulator->m_DataAccess.Simulation.Static.World.Brushes.pCollideable )
		PortalSimulatorDumps_DumpCollideToGlView( pPortalSimulator->m_DataAccess.Simulation.Static.World.Brushes.pCollideable, vec3_origin, vec3_angle, PSDAC_INTENSITY_LOCALBRUSH, szFileName );
	
	if( pPortalSimulator->m_DataAccess.Simulation.Static.World.StaticProps.bCollisionExists )
	{
		for( int i = pPortalSimulator->m_DataAccess.Simulation.Static.World.StaticProps.ClippedRepresentations.Count(); --i >= 0; )
		{
			Assert( pPortalSimulator->m_DataAccess.Simulation.Static.World.StaticProps.ClippedRepresentations[i].pCollide );
			PortalSimulatorDumps_DumpCollideToGlView( pPortalSimulator->m_DataAccess.Simulation.Static.World.StaticProps.ClippedRepresentations[i].pCollide, vec3_origin, vec3_angle, PSDAC_INTENSITY_LOCALPROP, szFileName );	
		}
	}

	if( pPortalSimulator->m_DataAccess.Simulation.Static.Wall.Local.Brushes.pCollideable )
		PortalSimulatorDumps_DumpCollideToGlView( pPortalSimulator->m_DataAccess.Simulation.Static.Wall.Local.Brushes.pCollideable, vec3_origin, vec3_angle, PSDAC_INTENSITY_LOCALBRUSH, szFileName );

	if( pPortalSimulator->m_DataAccess.Simulation.Static.Wall.Local.Tube.pCollideable )
		PortalSimulatorDumps_DumpCollideToGlView( pPortalSimulator->m_DataAccess.Simulation.Static.Wall.Local.Tube.pCollideable, vec3_origin, vec3_angle, PSDAC_INTENSITY_LOCALBRUSH, szFileName );

	//if( pPortalSimulator->m_DataAccess.Simulation.Static.Wall.RemoteTransformedToLocal.Brushes.pCollideable )
	//	PortalSimulatorDumps_DumpCollideToGlView( pPortalSimulator->m_DataAccess.Simulation.Static.Wall.RemoteTransformedToLocal.Brushes.pCollideable, vec3_origin, vec3_angle, PSDAC_INTENSITY_REMOTEBRUSH, szFileName );
	CPortalSimulator *pLinkedPortal = pPortalSimulator->GetLinkedPortalSimulator();
	if( pLinkedPortal )
	{
		if( pLinkedPortal->m_DataAccess.Simulation.Static.World.Brushes.pCollideable )
			PortalSimulatorDumps_DumpCollideToGlView( pLinkedPortal->m_DataAccess.Simulation.Static.World.Brushes.pCollideable, pPortalSimulator->m_DataAccess.Placement.ptaap_LinkedToThis.ptOriginTransform, pPortalSimulator->m_DataAccess.Placement.ptaap_LinkedToThis.qAngleTransform, PSDAC_INTENSITY_REMOTEBRUSH, szFileName );

		//for( int i = pPortalSimulator->m_DataAccess.Simulation.Static.Wall.RemoteTransformedToLocal.StaticProps.Collideables.Count(); --i >= 0; )
		//	PortalSimulatorDumps_DumpCollideToGlView( pPortalSimulator->m_DataAccess.Simulation.Static.Wall.RemoteTransformedToLocal.StaticProps.Collideables[i], vec3_origin, vec3_angle, PSDAC_INTENSITY_REMOTEPROP, szFileName );	
		if( pLinkedPortal->m_DataAccess.Simulation.Static.World.StaticProps.bCollisionExists )
		{
			for( int i = pLinkedPortal->m_DataAccess.Simulation.Static.World.StaticProps.ClippedRepresentations.Count(); --i >= 0; )
			{
				Assert( pLinkedPortal->m_DataAccess.Simulation.Static.World.StaticProps.ClippedRepresentations[i].pCollide );
				PortalSimulatorDumps_DumpCollideToGlView( pLinkedPortal->m_DataAccess.Simulation.Static.World.StaticProps.ClippedRepresentations[i].pCollide, pPortalSimulator->m_DataAccess.Placement.ptaap_LinkedToThis.ptOriginTransform, pPortalSimulator->m_DataAccess.Placement.ptaap_LinkedToThis.qAngleTransform, PSDAC_INTENSITY_REMOTEPROP, szFileName );	
			}
		}
	}

	STOPDEBUGTIMER( collisionDumpTimer );
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::DumpActiveCollision() Spent %fms generating a collision dump\n", pPortalSimulator->GetPortalSimulatorGUID(), TABSPACING, collisionDumpTimer.GetDuration().GetMillisecondsF() ); );
}

static void PortalSimulatorDumps_DumpCollideToGlView( CPhysCollide *pCollide, const Vector &origin, const QAngle &angles, float fColorScale, const char *pFilename )
{
	if ( !pCollide )
		return;

	printf("Writing %s...\n", pFilename );
	Vector *outVerts;
	int vertCount = physcollision->CreateDebugMesh( pCollide, &outVerts );
	FileHandle_t fp = filesystem->Open( pFilename, "ab" );
	int triCount = vertCount / 3;
	int vert = 0;
	VMatrix tmp = SetupMatrixOrgAngles( origin, angles );
	int i;
	for ( i = 0; i < vertCount; i++ )
	{
		outVerts[i] = tmp.VMul4x3( outVerts[i] );
	}

	for ( i = 0; i < triCount; i++ )
	{
		filesystem->FPrintf( fp, "3\n" );
		filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f 0 0\n", outVerts[vert].x, outVerts[vert].y, outVerts[vert].z, fColorScale );
		vert++;
		filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f 0 %.2f 0\n", outVerts[vert].x, outVerts[vert].y, outVerts[vert].z, fColorScale );
		vert++;
		filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f 0 0 %.2f\n", outVerts[vert].x, outVerts[vert].y, outVerts[vert].z, fColorScale );
		vert++;
	}
	filesystem->Close( fp );
	physcollision->DestroyDebugMesh( vertCount, outVerts );
}

static void PortalSimulatorDumps_DumpPlanesToGlView( float *pPlanes, int iPlaneCount, const char *pszFileName )
{
	FileHandle_t fp = filesystem->Open( pszFileName, "wb" );

	for( int i = 0; i < iPlaneCount; ++i )
	{
		Vector vPlaneVerts[4];

		float fRed, fGreen, fBlue;
		fRed = rand()/32768.0f;
		fGreen = rand()/32768.0f;
		fBlue = rand()/32768.0f;

		PolyFromPlane( vPlaneVerts, *(Vector *)(pPlanes + (i*4)), pPlanes[(i*4) + 3], 1000.0f );

		filesystem->FPrintf( fp, "4\n" );

		filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vPlaneVerts[3].x, vPlaneVerts[3].y, vPlaneVerts[3].z, fRed, fGreen, fBlue );
		filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vPlaneVerts[2].x, vPlaneVerts[2].y, vPlaneVerts[2].z, fRed, fGreen, fBlue );
		filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vPlaneVerts[1].x, vPlaneVerts[1].y, vPlaneVerts[1].z, fRed, fGreen, fBlue );
		filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vPlaneVerts[0].x, vPlaneVerts[0].y, vPlaneVerts[0].z, fRed, fGreen, fBlue );
	}

	filesystem->Close( fp );
}


static void PortalSimulatorDumps_DumpBoxToGlView( const Vector &vMins, const Vector &vMaxs, float fRed, float fGreen, float fBlue, const char *pszFileName )
{
	FileHandle_t fp = filesystem->Open( pszFileName, "ab" );

	//x min side
	filesystem->FPrintf( fp, "4\n" );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMins.x, vMins.y, vMins.z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMins.x, vMins.y, vMaxs.z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMins.x, vMaxs.y, vMaxs.z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMins.x, vMaxs.y, vMins.z, fRed, fGreen, fBlue );

	filesystem->FPrintf( fp, "4\n" );	
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMins.x, vMaxs.y, vMins.z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMins.x, vMaxs.y, vMaxs.z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMins.x, vMins.y, vMaxs.z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMins.x, vMins.y, vMins.z, fRed, fGreen, fBlue );

	//x max side
	filesystem->FPrintf( fp, "4\n" );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMaxs.x, vMins.y, vMins.z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMaxs.x, vMins.y, vMaxs.z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMaxs.x, vMaxs.y, vMaxs.z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMaxs.x, vMaxs.y, vMins.z, fRed, fGreen, fBlue );

	filesystem->FPrintf( fp, "4\n" );	
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMaxs.x, vMaxs.y, vMins.z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMaxs.x, vMaxs.y, vMaxs.z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMaxs.x, vMins.y, vMaxs.z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMaxs.x, vMins.y, vMins.z, fRed, fGreen, fBlue );


	//y min side
	filesystem->FPrintf( fp, "4\n" );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMins.x, vMins.y, vMins.z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMins.x, vMins.y, vMaxs.z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMaxs.x, vMins.y, vMaxs.z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMaxs.x, vMins.y, vMins.z, fRed, fGreen, fBlue );

	filesystem->FPrintf( fp, "4\n" );	
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMaxs.x, vMins.y, vMins.z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMaxs.x, vMins.y, vMaxs.z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMins.x, vMins.y, vMaxs.z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMins.x, vMins.y, vMins.z, fRed, fGreen, fBlue );



	//y max side
	filesystem->FPrintf( fp, "4\n" );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMins.x, vMaxs.y, vMins.z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMins.x, vMaxs.y, vMaxs.z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMaxs.x, vMaxs.y, vMaxs.z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMaxs.x, vMaxs.y, vMins.z, fRed, fGreen, fBlue );

	filesystem->FPrintf( fp, "4\n" );	
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMaxs.x, vMaxs.y, vMins.z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMaxs.x, vMaxs.y, vMaxs.z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMins.x, vMaxs.y, vMaxs.z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMins.x, vMaxs.y, vMins.z, fRed, fGreen, fBlue );



	//z min side
	filesystem->FPrintf( fp, "4\n" );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMins.x, vMins.y, vMins.z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMins.x, vMaxs.y, vMins.z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMaxs.x, vMaxs.y, vMins.z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMaxs.x, vMins.y, vMins.z, fRed, fGreen, fBlue );

	filesystem->FPrintf( fp, "4\n" );	
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMaxs.x, vMins.y, vMins.z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMaxs.x, vMaxs.y, vMins.z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMins.x, vMaxs.y, vMins.z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMins.x, vMins.y, vMins.z, fRed, fGreen, fBlue );



	//z max side
	filesystem->FPrintf( fp, "4\n" );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMins.x, vMins.y, vMaxs.z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMins.x, vMaxs.y, vMaxs.z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMaxs.x, vMaxs.y, vMaxs.z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMaxs.x, vMins.y, vMaxs.z, fRed, fGreen, fBlue );

	filesystem->FPrintf( fp, "4\n" );	
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMaxs.x, vMins.y, vMaxs.z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMaxs.x, vMaxs.y, vMaxs.z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMins.x, vMaxs.y, vMaxs.z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMins.x, vMins.y, vMaxs.z, fRed, fGreen, fBlue );

	filesystem->Close( fp );
}

static void PortalSimulatorDumps_DumpOBBoxToGlView( const Vector &ptOrigin, const Vector &vExtent1, const Vector &vExtent2, const Vector &vExtent3, float fRed, float fGreen, float fBlue, const char *pszFileName )
{
	FileHandle_t fp = filesystem->Open( pszFileName, "ab" );

	Vector ptExtents[8];
	int counter;
	for( counter = 0; counter != 8; ++counter )
	{
		ptExtents[counter] = ptOrigin;
		if( counter & (1<<0) ) ptExtents[counter] += vExtent1;
		if( counter & (1<<1) ) ptExtents[counter] += vExtent2;
		if( counter & (1<<2) ) ptExtents[counter] += vExtent3;
	}

	//x min side
	filesystem->FPrintf( fp, "4\n" );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[0].x, ptExtents[0].y, ptExtents[0].z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[4].x, ptExtents[4].y, ptExtents[4].z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[6].x, ptExtents[6].y, ptExtents[6].z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[2].x, ptExtents[2].y, ptExtents[2].z, fRed, fGreen, fBlue );

	filesystem->FPrintf( fp, "4\n" );	
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[2].x, ptExtents[2].y, ptExtents[2].z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[6].x, ptExtents[6].y, ptExtents[6].z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[4].x, ptExtents[4].y, ptExtents[4].z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[0].x, ptExtents[0].y, ptExtents[0].z, fRed, fGreen, fBlue );

	//x max side
	filesystem->FPrintf( fp, "4\n" );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[1].x, ptExtents[1].y, ptExtents[1].z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[5].x, ptExtents[5].y, ptExtents[5].z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[7].x, ptExtents[7].y, ptExtents[7].z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[3].x, ptExtents[3].y, ptExtents[3].z, fRed, fGreen, fBlue );

	filesystem->FPrintf( fp, "4\n" );	
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[3].x, ptExtents[3].y, ptExtents[3].z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[7].x, ptExtents[7].y, ptExtents[7].z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[5].x, ptExtents[5].y, ptExtents[5].z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[1].x, ptExtents[1].y, ptExtents[1].z, fRed, fGreen, fBlue );


	//y min side
	filesystem->FPrintf( fp, "4\n" );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[0].x, ptExtents[0].y, ptExtents[0].z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[4].x, ptExtents[4].y, ptExtents[4].z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[5].x, ptExtents[5].y, ptExtents[5].z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[1].x, ptExtents[1].y, ptExtents[1].z, fRed, fGreen, fBlue );

	filesystem->FPrintf( fp, "4\n" );	
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[1].x, ptExtents[1].y, ptExtents[1].z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[5].x, ptExtents[5].y, ptExtents[5].z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[4].x, ptExtents[4].y, ptExtents[4].z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[0].x, ptExtents[0].y, ptExtents[0].z, fRed, fGreen, fBlue );



	//y max side
	filesystem->FPrintf( fp, "4\n" );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[2].x, ptExtents[2].y, ptExtents[2].z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[6].x, ptExtents[6].y, ptExtents[6].z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[7].x, ptExtents[7].y, ptExtents[7].z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[3].x, ptExtents[3].y, ptExtents[3].z, fRed, fGreen, fBlue );

	filesystem->FPrintf( fp, "4\n" );	
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[3].x, ptExtents[3].y, ptExtents[3].z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[7].x, ptExtents[7].y, ptExtents[7].z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[6].x, ptExtents[6].y, ptExtents[6].z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[2].x, ptExtents[2].y, ptExtents[2].z, fRed, fGreen, fBlue );



	//z min side
	filesystem->FPrintf( fp, "4\n" );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[0].x, ptExtents[0].y, ptExtents[0].z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[2].x, ptExtents[2].y, ptExtents[2].z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[3].x, ptExtents[3].y, ptExtents[3].z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[1].x, ptExtents[1].y, ptExtents[1].z, fRed, fGreen, fBlue );

	filesystem->FPrintf( fp, "4\n" );	
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[1].x, ptExtents[1].y, ptExtents[1].z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[3].x, ptExtents[3].y, ptExtents[3].z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[2].x, ptExtents[2].y, ptExtents[2].z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[0].x, ptExtents[0].y, ptExtents[0].z, fRed, fGreen, fBlue );



	//z max side
	filesystem->FPrintf( fp, "4\n" );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[4].x, ptExtents[4].y, ptExtents[4].z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[6].x, ptExtents[6].y, ptExtents[6].z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[7].x, ptExtents[7].y, ptExtents[7].z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[5].x, ptExtents[5].y, ptExtents[5].z, fRed, fGreen, fBlue );

	filesystem->FPrintf( fp, "4\n" );	
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[5].x, ptExtents[5].y, ptExtents[5].z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[7].x, ptExtents[7].y, ptExtents[7].z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[6].x, ptExtents[6].y, ptExtents[6].z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[4].x, ptExtents[4].y, ptExtents[4].z, fRed, fGreen, fBlue );

	filesystem->Close( fp );
}



#endif














