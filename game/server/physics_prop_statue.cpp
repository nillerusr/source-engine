//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "physics_prop_statue.h"
#include "baseanimating.h"
#include "studio.h"
#include "bone_setup.h"
#include "EntityFreezing.h"
//#include "particle_parse.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Networking
//-----------------------------------------------------------------------------
LINK_ENTITY_TO_CLASS( physics_prop_statue, CStatueProp );

IMPLEMENT_SERVERCLASS_ST( CStatueProp, DT_StatueProp )
	SendPropEHandle( SENDINFO( m_hInitBaseAnimating ) ),
	SendPropBool( SENDINFO( m_bShatter ) ),
	SendPropInt( SENDINFO( m_nShatterFlags ), 3 ),
	SendPropVector( SENDINFO( m_vShatterPosition ) ),
	SendPropVector( SENDINFO( m_vShatterForce ) ),
END_SEND_TABLE()

BEGIN_DATADESC( CStatueProp )
	DEFINE_FIELD( m_hInitBaseAnimating,	FIELD_EHANDLE ),
	DEFINE_FIELD( m_bShatter,			FIELD_BOOLEAN ),
	DEFINE_FIELD( m_nShatterFlags,		FIELD_INTEGER ),
	DEFINE_FIELD( m_vShatterPosition,	FIELD_VECTOR ),
	DEFINE_FIELD( m_vShatterForce,		FIELD_VECTOR ),

	DEFINE_THINKFUNC( CollisionPartnerThink ),
END_DATADESC()

ConVarRef *s_vcollide_wireframe = NULL;


CStatueProp::CStatueProp( void )
{
	static ConVarRef vcollide_wireframe( "vcollide_wireframe" );
	s_vcollide_wireframe = &vcollide_wireframe;
	m_pInitOBBs = NULL;
}

void CStatueProp::Spawn( void )
{
	// Make it breakable
	SetBreakableModel( MAKE_STRING( "ConcreteChunks" ) );
	SetBreakableCount( 6 );
	SetHealth( 5 );

	BaseClass::Spawn();

	m_flFrozen = 1.0f;
}

void CStatueProp::Precache( void )
{
}

bool CStatueProp::CreateVPhysics( void )
{
	if ( m_pInitOBBs )
	{
		return CreateVPhysicsFromOBBs( m_hInitBaseAnimating );
	}
	else
	{
		if ( !CreateVPhysicsFromHitBoxes( m_hInitBaseAnimating ) )
		{
			// Init model didn't work out, so just use our own
			return CreateVPhysicsFromHitBoxes( this );
		}

		return true;
	}
}

void CStatueProp::VPhysicsUpdate( IPhysicsObject *pPhysics )
{
	BaseClass::VPhysicsUpdate( pPhysics );

	if ( s_vcollide_wireframe->GetBool() )
	{
		const CPhysCollide *pCollide = pPhysics->GetCollide();

		Vector vecOrigin;
		QAngle angAngles;

		pPhysics->GetPosition( &vecOrigin, &angAngles );

		if ( pCollide )
		{
			Vector *outVerts;
			int vertCount = physcollision->CreateDebugMesh( pCollide, &outVerts );
			int triCount = vertCount / 3;
			int vert = 0;

			VMatrix tmp = SetupMatrixOrgAngles( vecOrigin, angAngles );
			int i;
			for ( i = 0; i < vertCount; i++ )
			{
				outVerts[i] = tmp.VMul4x3( outVerts[i] );
			}

			for ( i = 0; i < triCount; i++ )
			{
				NDebugOverlay::Line( outVerts[ vert + 0 ], outVerts[ vert + 1 ], 0, 255, 255, false, 0.0f );
				NDebugOverlay::Line( outVerts[ vert + 1 ], outVerts[ vert + 2 ], 0, 255, 255, false, 0.0f );
				NDebugOverlay::Line( outVerts[ vert + 2 ], outVerts[ vert + 0 ], 0, 255, 255, false, 0.0f );
				vert += 3;
			}

			physcollision->DestroyDebugMesh( vertCount, outVerts );
		}
	}
}

void CStatueProp::ComputeWorldSpaceSurroundingBox( Vector *pMins, Vector *pMaxs )
{
	CBaseAnimating *pBaseAnimating = m_hInitBaseAnimating;

	if ( pBaseAnimating )
	{
		pBaseAnimating->CollisionProp()->WorldSpaceSurroundingBounds( pMins, pMaxs );
		return;
	}

	CollisionProp()->WorldSpaceSurroundingBounds( pMins, pMaxs );
}

bool CStatueProp::TestCollision( const Ray_t &ray, unsigned int fContentsMask, trace_t& tr )
{
	IPhysicsObject *pPhysObject = VPhysicsGetObject();

	if ( pPhysObject )
	{
		Vector vecPosition;
		QAngle vecAngles;
		pPhysObject->GetPosition( &vecPosition, &vecAngles );
		const CPhysCollide *pScaledCollide = pPhysObject->GetCollide();
		physcollision->TraceBox( ray, pScaledCollide, vecPosition, vecAngles, &tr );

		return tr.DidHit();
	}

	return false;
}

int	CStatueProp::OnTakeDamage( const CTakeDamageInfo &info )
{
	return BaseClass::OnTakeDamage( info );
}

void CStatueProp::Event_Killed( const CTakeDamageInfo &info )
{
	IPhysicsObject *pPhysics = VPhysicsGetObject();

	if ( pPhysics && !pPhysics->IsMoveable() )
	{
		pPhysics->EnableMotion( true );
		VPhysicsTakeDamage( info );
	}
	
	m_nShatterFlags = 0; // If you have some flags to network for the shatter effect, put them here!
	m_vShatterPosition = info.GetDamagePosition();
	m_vShatterForce = info.GetDamageForce();
	m_bShatter = true;

	// Skip over breaking code!
	//Break( info.GetInflictor(), info );
	//BaseClass::Event_Killed( info );

	// FIXME: Short delay before we actually remove so that the client statue gets a network update before we need it
	// This isn't a reliable way to do this and needs to be rethought.
	AddSolidFlags( FSOLID_NOT_SOLID );

	SetNextThink( gpGlobals->curtime + 0.2f );
	SetThink( &CBaseEntity::SUB_Remove );
}

void CStatueProp::Freeze( float flFreezeAmount, CBaseEntity *pFreezer, Ray_t *pFreezeRay )
{
	// Can't freeze a statue
	TakeDamage( CTakeDamageInfo( pFreezer, pFreezer, 1, DMG_GENERIC ) );
}

void CStatueProp::CollisionPartnerThink( void )
{
	CBaseAnimating *pBaseAnimating = m_hInitBaseAnimating;
	if ( !pBaseAnimating )
	{
		// Our partner died, I have no reason to live!
		UTIL_Remove( this );
	}

	if ( GetHealth() <= 0 )
	{
		// Reset health here in case it was tweaked by the model parse
		SetHealth( 5 );
		m_takedamage = DAMAGE_YES;
	}

	SetNextThink( gpGlobals->curtime + 1.0f );
}


bool CStatueProp::CreateVPhysicsFromHitBoxes( CBaseAnimating *pInitBaseAnimating )
{
	if ( !pInitBaseAnimating )
		return false;

	// Use the current animation sequence and cycle
	CopyAnimationDataFrom( pInitBaseAnimating );

	// Copy over any render color
	color24 colorRender = pInitBaseAnimating->GetRenderColor();
	SetRenderColor( colorRender.r, colorRender.g, colorRender.b );
	SetRenderAlpha( pInitBaseAnimating->GetRenderAlpha() );

	// Get hitbox data
	CStudioHdr *pStudioHdr = GetModelPtr();
	if ( !pStudioHdr )
		return false;

	mstudiohitboxset_t *set = pStudioHdr->pHitboxSet( m_nHitboxSet );
	if ( !set )
		return false;

	Vector position;
	QAngle angles;

	// Make enough pointers to convexes for each hitbox
	CPhysConvex **ppConvex = new (CPhysConvex*[ set->numhitboxes ]);

	float flTotalVolume = 0.0f;
	float flTotalSurfaceArea = 0.0f;

	for ( int i = 0; i < set->numhitboxes; i++ )
	{
		// Get the hitbox info
		mstudiobbox_t *pbox = set->pHitbox( i );
		GetBonePosition( pbox->bone, position, angles );

		// Accumulate volume and area
		Vector flDimentions = pbox->bbmax - pbox->bbmin;
		flTotalVolume += flDimentions.x * flDimentions.y * flDimentions.z;
		flTotalSurfaceArea += 2.0f * ( flDimentions.x * flDimentions.y + flDimentions.x * flDimentions.z + flDimentions.y * flDimentions.z );

		// Get angled min and max extents
		Vector vecMins, vecMaxs;
		VectorRotate( pbox->bbmin, angles, vecMins );
		VectorRotate( pbox->bbmax, angles, vecMaxs );

		// Get the corners in world space
		Vector vecMinCorner = position + vecMins;
		Vector vecMaxCorner = position + vecMaxs;

		// Get the normals of the hitbox in world space
		Vector vecForward, vecRight, vecUp;
		AngleVectors( angles, &vecForward, &vecRight, &vecUp );
		vecRight = -vecRight;

		// Convert corners and normals to local space
		Vector vecCornerLocal[ 2 ];
		Vector vecNormalLocal[ 3 ];

		matrix3x4_t matToWorld = EntityToWorldTransform();
		VectorITransform( vecMaxCorner, matToWorld, vecCornerLocal[ 0 ] );
		VectorITransform( vecMinCorner, matToWorld, vecCornerLocal[ 1 ] );
		VectorIRotate( vecForward, matToWorld, vecNormalLocal[ 0 ] );
		VectorIRotate( vecRight, matToWorld, vecNormalLocal[ 1 ] );
		VectorIRotate( vecUp, matToWorld, vecNormalLocal[ 2 ] );

		// Create 6 planes from the local oriented hit box data
		float pPlanes[ 4 * 6 ];

		for ( int iPlane = 0; iPlane < 6; ++iPlane )
		{
			int iPlaneMod2 = iPlane % 2;
			int iPlaneDiv2 = iPlane / 2;
			bool bOdd = ( iPlaneMod2 == 1 );

			// Plane Normal
			pPlanes[ iPlane * 4 + 0 ] = vecNormalLocal[ iPlaneDiv2 ].x * ( bOdd ? -1.0f : 1.0f );
			pPlanes[ iPlane * 4 + 1 ] = vecNormalLocal[ iPlaneDiv2 ].y * ( bOdd ? -1.0f : 1.0f );
			pPlanes[ iPlane * 4 + 2 ] = vecNormalLocal[ iPlaneDiv2 ].z * ( bOdd ? -1.0f : 1.0f );

			// Plane D
			pPlanes[ iPlane * 4 + 3 ] = ( vecCornerLocal[ iPlaneMod2 ].x * vecNormalLocal[ iPlaneDiv2 ].x + 
				vecCornerLocal[ iPlaneMod2 ].y * vecNormalLocal[ iPlaneDiv2 ].y + 
				vecCornerLocal[ iPlaneMod2 ].z * vecNormalLocal[ iPlaneDiv2 ].z ) * ( bOdd ? -1.0f : 1.0f );
		}

		// Create convex from the intersection of these planes
		ppConvex[ i ] = physcollision->ConvexFromPlanes( pPlanes, 6, 0.0f );
	}

	// Make a single collide out of the group of convex boxes
	CPhysCollide *pPhysCollide = physcollision->ConvertConvexToCollide( ppConvex, set->numhitboxes );

	delete[] ppConvex;

	// Create the physics object
	objectparams_t params = g_PhysDefaultObjectParams;
	params.pGameData = static_cast<void *>( this );

	int nMaterialIndex = physprops->GetSurfaceIndex( "ice" );	// use ice material

	IPhysicsObject* p = physenv->CreatePolyObject( pPhysCollide, nMaterialIndex, GetAbsOrigin(), GetAbsAngles(), &params );
	Assert( p != NULL );

	// Set velocity
	Vector vecInitialVelocity = pInitBaseAnimating->GetAbsVelocity();
	p->SetVelocity( &vecInitialVelocity, NULL );

	// Compute mass
	float flMass;
	float flDensity, flThickness;
	physprops->GetPhysicsProperties( nMaterialIndex, &flDensity, &flThickness, NULL, NULL );

	// Make it more hollow
	flThickness = MIN ( 1.0f, flThickness + 0.5f );

	if ( flThickness > 0.0f )
	{
		flMass = flTotalSurfaceArea * flThickness * CUBIC_METERS_PER_CUBIC_INCH * flDensity;
	}
	else
	{
		// density is in kg/m^3, volume is in in^3
		flMass = flTotalVolume * CUBIC_METERS_PER_CUBIC_INCH * flDensity;
	}

	// Mass is somewhere between the original and if it was all ice
	p->SetMass( flMass );

	// Yes, gravity
	p->EnableGravity( true );

	// Use this as our vphysics
	VPhysicsSetObject( p );

	SetSolid( SOLID_VPHYSICS );
	AddSolidFlags( FSOLID_CUSTOMRAYTEST | FSOLID_CUSTOMBOXTEST );

	SetMoveType( MOVETYPE_VPHYSICS );

	if ( pInitBaseAnimating != this )
	{
		// Transfer children from the init base animating
		TransferChildren( pInitBaseAnimating, this );

		CBaseEntity *pChild = FirstMoveChild();

		while ( pChild )
		{
			CEntityFreezing *pFreezing = dynamic_cast<CEntityFreezing*>( pChild );
			if ( pFreezing )
			{
				pFreezing->FinishFreezing();
			}

			pChild = pChild->NextMovePeer();
		}
	}

	return true;
}

bool CStatueProp::CreateVPhysicsFromOBBs( CBaseAnimating *pInitBaseAnimating )
{
	// Make enough pointers to convexes for each hitbox
	CPhysConvex **ppConvex = new (CPhysConvex*[ m_pInitOBBs->Count() ]);

	float flTotalVolume = 0.0f;
	float flTotalSurfaceArea = 0.0f;

	for ( int i = 0; i < m_pInitOBBs->Count(); i++ )
	{
		const outer_collision_obb_t *pOBB = &((*m_pInitOBBs)[ i ]);

		// Accumulate volume and area
		Vector flDimentions = pOBB->vecMaxs - pOBB->vecMins;
		flTotalVolume += flDimentions.x * flDimentions.y * flDimentions.z;
		flTotalSurfaceArea += 2.0f * ( flDimentions.x * flDimentions.y + flDimentions.x * flDimentions.z + flDimentions.y * flDimentions.z );

		// Get angled min and max extents
		Vector vecMins, vecMaxs;
		VectorRotate( pOBB->vecMins, pOBB->angAngles, vecMins );
		VectorRotate( pOBB->vecMaxs, pOBB->angAngles, vecMaxs );

		// Get the corners in world space
		Vector vecMinCorner = pOBB->vecPos + vecMins;
		Vector vecMaxCorner = pOBB->vecPos + vecMaxs;

		// Get the normals of the hitbox in world space
		Vector vecForward, vecRight, vecUp;
		AngleVectors( pOBB->angAngles, &vecForward, &vecRight, &vecUp );
		vecRight = -vecRight;

		// Convert corners and normals to local space
		Vector vecCornerLocal[ 2 ];
		Vector vecNormalLocal[ 3 ];

		matrix3x4_t matToWorld = EntityToWorldTransform();
		VectorITransform( vecMaxCorner, matToWorld, vecCornerLocal[ 0 ] );
		VectorITransform( vecMinCorner, matToWorld, vecCornerLocal[ 1 ] );
		VectorIRotate( vecForward, matToWorld, vecNormalLocal[ 0 ] );
		VectorIRotate( vecRight, matToWorld, vecNormalLocal[ 1 ] );
		VectorIRotate( vecUp, matToWorld, vecNormalLocal[ 2 ] );

		// Create 6 planes from the local oriented hit box data
		float pPlanes[ 4 * 6 ];

		for ( int iPlane = 0; iPlane < 6; ++iPlane )
		{
			int iPlaneMod2 = iPlane % 2;
			int iPlaneDiv2 = iPlane / 2;
			bool bOdd = ( iPlaneMod2 == 1 );

			// Plane Normal
			pPlanes[ iPlane * 4 + 0 ] = vecNormalLocal[ iPlaneDiv2 ].x * ( bOdd ? -1.0f : 1.0f );
			pPlanes[ iPlane * 4 + 1 ] = vecNormalLocal[ iPlaneDiv2 ].y * ( bOdd ? -1.0f : 1.0f );
			pPlanes[ iPlane * 4 + 2 ] = vecNormalLocal[ iPlaneDiv2 ].z * ( bOdd ? -1.0f : 1.0f );

			// Plane D
			pPlanes[ iPlane * 4 + 3 ] = ( vecCornerLocal[ iPlaneMod2 ].x * vecNormalLocal[ iPlaneDiv2 ].x + 
				vecCornerLocal[ iPlaneMod2 ].y * vecNormalLocal[ iPlaneDiv2 ].y + 
				vecCornerLocal[ iPlaneMod2 ].z * vecNormalLocal[ iPlaneDiv2 ].z ) * ( bOdd ? -1.0f : 1.0f );
		}

		// Create convex from the intersection of these planes
		ppConvex[ i ] = physcollision->ConvexFromPlanes( pPlanes, 6, 0.0f );
	}

	// Make a single collide out of the group of convex boxes
	CPhysCollide *pPhysCollide = physcollision->ConvertConvexToCollide( ppConvex, m_pInitOBBs->Count() );

	delete[] ppConvex;

	// Create the physics object
	objectparams_t params = g_PhysDefaultObjectParams;
	params.pGameData = static_cast<void *>( this );

	int nMaterialIndex = physprops->GetSurfaceIndex( "ice" );	// use ice material

	IPhysicsObject* p = physenv->CreatePolyObject( pPhysCollide, nMaterialIndex, GetAbsOrigin(), GetAbsAngles(), &params );
	Assert( p != NULL );

	// Set velocity
	Vector vecInitialVelocity = pInitBaseAnimating->GetAbsVelocity();
	p->SetVelocity( &vecInitialVelocity, NULL );

	// Compute mass
	float flMass;
	float flDensity, flThickness;
	physprops->GetPhysicsProperties( nMaterialIndex, &flDensity, &flThickness, NULL, NULL );

	// Make it more hollow
	flThickness = MIN ( 1.0f, flThickness + 0.5f );

	if ( flThickness > 0.0f )
	{
		flMass = flTotalSurfaceArea * flThickness * CUBIC_METERS_PER_CUBIC_INCH * flDensity;
	}
	else
	{
		// density is in kg/m^3, volume is in in^3
		flMass = flTotalVolume * CUBIC_METERS_PER_CUBIC_INCH * flDensity;
	}

	// Mass is somewhere between the original and if it was all ice
	p->SetMass( flMass );

	// Yes, gravity
	p->EnableGravity( true );

	// Use this as our vphysics
	VPhysicsSetObject( p );

	SetSolid( SOLID_VPHYSICS );
	AddSolidFlags( FSOLID_CUSTOMRAYTEST | FSOLID_CUSTOMBOXTEST );

	SetMoveType( MOVETYPE_VPHYSICS );

	m_pInitOBBs = NULL;

	return true;
}


CBaseEntity *CreateServerStatue( CBaseAnimating *pAnimating, int collisionGroup )
{
	CStatueProp *pStatue = static_cast<CStatueProp *>( CreateEntityByName( "physics_prop_statue" ) );

	if ( pStatue )
	{
		pStatue->m_hInitBaseAnimating = pAnimating;
		pStatue->SetModelName( pAnimating->GetModelName() );
		pStatue->SetAbsOrigin( pAnimating->GetAbsOrigin() );
		pStatue->SetAbsAngles( pAnimating->GetAbsAngles() );
		DispatchSpawn( pStatue );
		pStatue->Activate();
	}

	return pStatue;
}

CBaseEntity *CreateServerStatueFromOBBs( const CUtlVector<outer_collision_obb_t> &vecSphereOrigins, CBaseAnimating *pAnimating )
{
	Assert( vecSphereOrigins.Count() > 0 );

	if ( vecSphereOrigins.Count() <= 0 )
		return NULL;

	CStatueProp *pStatue = static_cast<CStatueProp *>( CreateEntityByName( "physics_prop_statue" ) );

	if ( pStatue )
	{
		pStatue->m_pInitOBBs = &vecSphereOrigins;

		pStatue->m_hInitBaseAnimating = pAnimating;
		pStatue->SetModelName( pAnimating->GetModelName() );
		pStatue->SetAbsOrigin( pAnimating->GetAbsOrigin() );
		pStatue->SetAbsAngles( pAnimating->GetAbsAngles() );
		DispatchSpawn( pStatue );
		pStatue->Activate();

		pStatue->AddEffects( EF_NODRAW );
		pStatue->CollisionProp()->SetSurroundingBoundsType( USE_GAME_CODE );
		pStatue->AddSolidFlags( ( pAnimating->GetSolidFlags() & FSOLID_CUSTOMBOXTEST ) | ( pAnimating->GetSolidFlags() & FSOLID_CUSTOMRAYTEST ) );

		pAnimating->SetParent( pStatue );

		// You'll need to keep track of the child for collision rules
		pStatue->SetThink( &CStatueProp::CollisionPartnerThink );
		pStatue->SetNextThink( gpGlobals->curtime + 1.0f );
	}

	return pStatue;
}

