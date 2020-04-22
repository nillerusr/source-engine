//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Instead of cloning all physics objects in a level to get proper
//			near-portal reactions, only clone from a larger area near portals.
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "PhysicsCloneArea.h"
#include "prop_portal.h"
#include "portal_shareddefs.h"
#include "collisionutils.h"
#include "env_debughistory.h"

LINK_ENTITY_TO_CLASS( physicsclonearea, CPhysicsCloneArea );


#define PHYSICSCLONEAREASCALE 4.0f

const Vector CPhysicsCloneArea::vLocalMins( 3.0f, 
										   -PORTAL_HALF_WIDTH * PHYSICSCLONEAREASCALE, 
										   -PORTAL_HALF_HEIGHT * PHYSICSCLONEAREASCALE );
const Vector CPhysicsCloneArea::vLocalMaxs( PORTAL_HALF_HEIGHT * PHYSICSCLONEAREASCALE,  //x is the forward which is fairly thin for portals, replacing with halfheight
											PORTAL_HALF_WIDTH * PHYSICSCLONEAREASCALE,
											PORTAL_HALF_HEIGHT * PHYSICSCLONEAREASCALE );

extern ConVar sv_portal_debug_touch;

void CPhysicsCloneArea::StartTouch( CBaseEntity *pOther )
{
	if( !m_bActive )
		return;

	if( sv_portal_debug_touch.GetBool() )
	{
		DevMsg( "PortalCloneArea %i Start Touch: %s : %f\n", ((m_pAttachedPortal->m_bIsPortal2)?(2):(1)), pOther->GetClassname(), gpGlobals->curtime );
	}
#if !defined( DISABLE_DEBUG_HISTORY )
	if ( !IsMarkedForDeletion() )
	{
		ADD_DEBUG_HISTORY( HISTORY_PLAYER_DAMAGE, UTIL_VarArgs( "PortalCloneArea %i Start Touch: %s : %f\n", ((m_pAttachedPortal->m_bIsPortal2)?(2):(1)), pOther->GetClassname(), gpGlobals->curtime  ) );
	}
#endif

	m_pAttachedSimulator->StartCloningEntity( pOther );
}

void CPhysicsCloneArea::Touch( CBaseEntity *pOther )
{
	if( !m_bActive )
		return;

	//TODO: Planar checks to see if it's a better idea to reclone/unclone
	
}

void CPhysicsCloneArea::EndTouch( CBaseEntity *pOther )
{
	if( !m_bActive )
		return;

	if( sv_portal_debug_touch.GetBool() )
	{
		DevMsg( "PortalCloneArea %i End Touch: %s : %f\n", ((m_pAttachedPortal->m_bIsPortal2)?(2):(1)), pOther->GetClassname(), gpGlobals->curtime );
	}
#if !defined( DISABLE_DEBUG_HISTORY )
	if ( !IsMarkedForDeletion() )
	{
		ADD_DEBUG_HISTORY( HISTORY_PLAYER_DAMAGE, UTIL_VarArgs( "PortalCloneArea %i End Touch: %s : %f\n", ((m_pAttachedPortal->m_bIsPortal2)?(2):(1)), pOther->GetClassname(), gpGlobals->curtime ) );
	}
#endif

	m_pAttachedSimulator->StopCloningEntity( pOther );
}

void CPhysicsCloneArea::Spawn( void )
{
	BaseClass::Spawn();

	Assert( m_pAttachedPortal );

	AddEffects( EF_NORECEIVESHADOW | EF_NOSHADOW | EF_NODRAW );

	SetSolid( SOLID_OBB );
	SetSolidFlags( FSOLID_TRIGGER | FSOLID_NOT_SOLID );
	SetMoveType( MOVETYPE_NONE );
	SetCollisionGroup( COLLISION_GROUP_PLAYER );

	SetSize( vLocalMins, vLocalMaxs );
}

void CPhysicsCloneArea::Activate( void )
{
	BaseClass::Activate();
}

int CPhysicsCloneArea::ObjectCaps( void )
{ 
	return BaseClass::ObjectCaps() | FCAP_DONT_SAVE; //don't save this entity in any way, we naively recreate them
}


void CPhysicsCloneArea::UpdatePosition( void )
{
	Assert( m_pAttachedPortal );

	//untouch everything we're touching
	touchlink_t *root = ( touchlink_t * )GetDataObject( TOUCHLINK );
	if( root )
	{
		//don't want to risk list corruption while untouching
		CUtlVector<CBaseEntity *> TouchingEnts;
		for( touchlink_t *link = root->nextLink; link != root; link = link->nextLink )
			TouchingEnts.AddToTail( link->entityTouched );


		for( int i = TouchingEnts.Count(); --i >= 0; )
		{
			CBaseEntity *pTouch = TouchingEnts[i];

			pTouch->PhysicsNotifyOtherOfUntouch( pTouch, this );
			PhysicsNotifyOtherOfUntouch( this, pTouch );
		}
	}

	SetAbsOrigin( m_pAttachedPortal->GetAbsOrigin() );
	SetAbsAngles( m_pAttachedPortal->GetAbsAngles() );
	m_bActive = m_pAttachedPortal->m_bActivated;

	//NDebugOverlay::EntityBounds( this, 0, 0, 255, 25, 5.0f );

	//RemoveFlag( FL_DONTTOUCH );
	CloneNearbyEntities(); //wake new objects so they can figure out that they touch
}

void CPhysicsCloneArea::CloneNearbyEntities( void )
{
	CBaseEntity*	pList[ 1024 ];

	Vector vForward, vUp, vRight;
	GetVectors( &vForward, &vRight, &vUp );

	Vector ptOrigin = GetAbsOrigin();
	QAngle qAngles = GetAbsAngles();

	Vector ptOBBStart = ptOrigin;
	ptOBBStart += vForward * vLocalMins.x;
	ptOBBStart += vRight * vLocalMins.y;
	ptOBBStart += vUp * vLocalMins.z;
	

	vForward *= vLocalMaxs.x - vLocalMins.x;
	vRight *= vLocalMaxs.y - vLocalMins.y;
	vUp *= vLocalMaxs.z - vLocalMins.z;


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
	

	/*{
		Vector ptAABBCenter = (vAABBMins + vAABBMaxs) * 0.5f;
		Vector vAABBExtent = (vAABBMaxs - vAABBMins) * 0.5f;
		NDebugOverlay::Box( ptAABBCenter, -vAABBExtent, vAABBExtent, 0, 0, 255, 128, 10.0f );
	}*/
	

	int count = UTIL_EntitiesInBox( pList, 1024, vAABBMins, vAABBMaxs, 0 );
	trace_t tr;
	UTIL_ClearTrace( tr );
	

	//Iterate over all the possible targets
	for ( int i = 0; i < count; i++ )
	{
		CBaseEntity *pEntity = pList[i];

		if ( pEntity  && (pEntity != this) )
		{
			IPhysicsObject *pPhysicsObject = pEntity->VPhysicsGetObject();

			if( pPhysicsObject )
			{
				CCollisionProperty *pEntCollision = pEntity->CollisionProp();
				Vector ptEntityCenter = pEntCollision->GetCollisionOrigin();

				//double check intersection at the OBB vs OBB level, we don't want to affect large piles of physics objects if we don't have to, it gets slow
				if( IsOBBIntersectingOBB( ptOrigin, qAngles, vLocalMins, vLocalMaxs, 
					ptEntityCenter, pEntCollision->GetCollisionAngles(), pEntCollision->OBBMins(), pEntCollision->OBBMaxs() ) )
				{
					tr.endpos = (ptOrigin + ptEntityCenter) * 0.5;
					PhysicsMarkEntitiesAsTouching( pEntity, tr );
					//StartTouch( pEntity );
					
					//pEntity->WakeRestingObjects();
					//pPhysicsObject->Wake();
				}
			}
		}
	}
}

void CPhysicsCloneArea::CloneTouchingEntities( void )
{
	if( m_pAttachedPortal && m_pAttachedPortal->m_bActivated )
	{
		touchlink_t *root = ( touchlink_t * )GetDataObject( TOUCHLINK );
		if( root )
		{
			for( touchlink_t *link = root->nextLink; link != root; link = link->nextLink )
				m_pAttachedSimulator->StartCloningEntity( link->entityTouched );
		}
	}
}





CPhysicsCloneArea *CPhysicsCloneArea::CreatePhysicsCloneArea( CProp_Portal *pFollowPortal )
{
	if( !pFollowPortal )
		return NULL;

	CPhysicsCloneArea *pCloneArea = (CPhysicsCloneArea *)CreateEntityByName( "physicsclonearea" );

	pCloneArea->m_pAttachedPortal = pFollowPortal;
	pCloneArea->m_pAttachedSimulator = &pFollowPortal->m_PortalSimulator;

	DispatchSpawn( pCloneArea );

	pCloneArea->UpdatePosition();

	return pCloneArea;
}

