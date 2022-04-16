//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Clones a physics object (usually with a matrix transform applied)
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "physicsshadowclone.h"
#include "portal_util_shared.h"
#include "vphysics/object_hash.h"
#include "trains.h"
#include "props.h"
#include "model_types.h"
#include "portal/weapon_physcannon.h" //grab controllers

#include "PortalSimulation.h"

#define MAX_SHADOW_CLONE_COUNT 200

static int g_iShadowCloneCount = 0;
ConVar sv_debug_physicsshadowclones("sv_debug_physicsshadowclones", "0", FCVAR_REPLICATED );
ConVar sv_use_shadow_clones( "sv_use_shadow_clones", "1", FCVAR_REPLICATED | FCVAR_CHEAT ); //should we create shadow clones?

static void DrawDebugOverlayForShadowClone( CPhysicsShadowClone *pClone );

LINK_ENTITY_TO_CLASS( physicsshadowclone, CPhysicsShadowClone );

static CUtlVector<CPhysicsShadowClone *> s_ActiveShadowClones;
CUtlVector<CPhysicsShadowClone *> const &CPhysicsShadowClone::g_ShadowCloneList = s_ActiveShadowClones;
static bool s_IsShadowClone[MAX_EDICTS] = { false };

static CPhysicsShadowCloneLL *s_EntityClones[MAX_EDICTS] = { NULL };
struct ShadowCloneLLEntryManager
{
	CPhysicsShadowCloneLL m_ShadowCloneLLEntries[MAX_SHADOW_CLONE_COUNT];
	CPhysicsShadowCloneLL *m_pFreeShadowCloneLLEntries[MAX_SHADOW_CLONE_COUNT];
	int m_iUsedEntryIndex;

	ShadowCloneLLEntryManager( void )
	{
		m_iUsedEntryIndex = 0;
		for( int i = 0; i != MAX_SHADOW_CLONE_COUNT; ++i )
		{
			m_pFreeShadowCloneLLEntries[i] = &m_ShadowCloneLLEntries[i];
		}
	}

	inline CPhysicsShadowCloneLL *Alloc( void )
	{
		return m_pFreeShadowCloneLLEntries[m_iUsedEntryIndex++];
	}

	inline void Free( CPhysicsShadowCloneLL *pFree )
	{
		m_pFreeShadowCloneLLEntries[--m_iUsedEntryIndex] = pFree;
	}
};
static ShadowCloneLLEntryManager s_SCLLManager;


CPhysicsShadowClone::CPhysicsShadowClone( void )
{
	m_matrixShadowTransform.Identity();
	m_matrixShadowTransform_Inverse.Identity();
	m_bShadowTransformIsIdentity = true;
	s_ActiveShadowClones.AddToTail( this );
}

CPhysicsShadowClone::~CPhysicsShadowClone( void )
{
	VPhysicsDestroyObject();
	VPhysicsSetObject( NULL );
	m_hClonedEntity = NULL;
	s_ActiveShadowClones.FindAndRemove( this ); //also removed in UpdateOnRemove()
	Assert( s_IsShadowClone[entindex()] == true );
	s_IsShadowClone[entindex()] = false;
}

void CPhysicsShadowClone::UpdateOnRemove( void )
{
	CBaseEntity *pSource = m_hClonedEntity;
	if( pSource )
	{
		CPhysicsShadowCloneLL *pCloneListHead = s_EntityClones[pSource->entindex()];
		Assert( pCloneListHead != NULL );

		CPhysicsShadowCloneLL *pFind = pCloneListHead;
		CPhysicsShadowCloneLL *pLast = pFind;
		while( pFind->pClone != this )
		{
			pLast = pFind;
			Assert( pFind->pNext != NULL );
			pFind = pFind->pNext;
		}

		if( pFind == pCloneListHead )
		{
			s_EntityClones[pSource->entindex()] = pFind->pNext;
		}
		else
		{
			pLast->pNext = pFind->pNext;
		}
		s_SCLLManager.Free( pFind );
	}
#ifdef _DEBUG
	else
	{
		//verify that it didn't weasel into a list somewhere and get left behind
		for( int i = 0; i != MAX_SHADOW_CLONE_COUNT; ++i )
		{
			CPhysicsShadowCloneLL *pCloneSearch = s_EntityClones[i];
			while( pCloneSearch )
			{
				Assert( pCloneSearch->pClone != this );
				pCloneSearch = pCloneSearch->pNext;
			}
		}
	}
#endif
	VPhysicsDestroyObject();
	VPhysicsSetObject( NULL );
	m_hClonedEntity = NULL;
	s_ActiveShadowClones.FindAndRemove( this ); //also removed in Destructor
	BaseClass::UpdateOnRemove();
}

void CPhysicsShadowClone::Spawn( void )
{
	AddFlag( FL_DONTTOUCH );
	AddEffects( EF_NODRAW | EF_NOSHADOW | EF_NORECEIVESHADOW );

	FullSync( false );
	m_bInAssumedSyncState = false;
	
	BaseClass::Spawn();

	s_IsShadowClone[entindex()] = true;
}


void CPhysicsShadowClone::FullSync( bool bAllowAssumedSync )
{
	Assert( IsMarkedForDeletion() == false );

	CBaseEntity *pClonedEntity = m_hClonedEntity.Get();

	if( pClonedEntity == NULL )
	{
		AssertMsg( VPhysicsGetObject() != NULL, "Been linkless for more than this update, something should have killed this clone." );
		SetMoveType( MOVETYPE_NONE );
		SetSolid( SOLID_NONE );
		SetSolidFlags( 0 );
		SetCollisionGroup( COLLISION_GROUP_NONE );
		VPhysicsDestroyObject();
		return;
	}

	SetGroundEntity( NULL );

	bool bIsSynced = bAllowAssumedSync;
	bool bBigChanges = true; //assume there are, and be proven wrong

	if( bAllowAssumedSync )
	{
		IPhysicsObject *pSourceObjects[1024];
		int iObjectCount = pClonedEntity->VPhysicsGetObjectList( pSourceObjects, 1024 );

		//scan for really big differences that would definitely require a full sync
		bBigChanges = ( iObjectCount != m_CloneLinks.Count() );
		if( !bBigChanges )
		{
			for( int i = 0; i != iObjectCount; ++i )
			{
				IPhysicsObject *pSourcePhysics = pSourceObjects[i];
				IPhysicsObject *pClonedPhysics = m_CloneLinks[i].pClone;

				if( (pSourcePhysics != m_CloneLinks[i].pSource) || 
					(pSourcePhysics->IsCollisionEnabled() != pClonedPhysics->IsCollisionEnabled()) )
				{
					bBigChanges = true;
					bIsSynced = false;
					break;
				}

				Vector ptSourcePosition, ptClonePosition;
				pSourcePhysics->GetPosition( &ptSourcePosition, NULL );
				if( !m_bShadowTransformIsIdentity )
					ptSourcePosition = m_matrixShadowTransform * ptSourcePosition;

				pClonedPhysics->GetPosition( &ptClonePosition, NULL );

				if( (ptClonePosition - ptSourcePosition).LengthSqr() > 2500.0f )
				{
					bBigChanges = true;
					bIsSynced = false;
					break;
				}
				
				//Vector vSourceVelocity, vCloneVelocity;


				if( !pSourcePhysics->IsAsleep() ) //only allow full syncrosity if the source entity is entirely asleep
					bIsSynced = false;

				if( m_bInAssumedSyncState && !pClonedPhysics->IsAsleep() )
					bIsSynced = false;
			}
		}
		else
		{
			bIsSynced = false;
		}

		bIsSynced = false;

		if( bIsSynced )
		{
			//good enough to skip a full update
			if( !m_bInAssumedSyncState )
			{
				//do one last sync
				PartialSync( true );

				//if we don't do this, objects just fall out of the world (it happens, I swear)
				
				for( int i = m_CloneLinks.Count(); --i >= 0; )
				{
					if( (m_CloneLinks[i].pSource->GetShadowController() == NULL) && m_CloneLinks[i].pClone->IsMotionEnabled() )
					{
						//m_CloneLinks[i].pClone->SetVelocityInstantaneous( &vec3_origin, &vec3_origin );
						//m_CloneLinks[i].pClone->SetVelocity( &vec3_origin, &vec3_origin );
						m_CloneLinks[i].pClone->EnableGravity( false );
						m_CloneLinks[i].pClone->EnableMotion( false );
						m_CloneLinks[i].pClone->Sleep();
					}
				}

				m_bInAssumedSyncState = true;
			}
			
			if( sv_debug_physicsshadowclones.GetBool() )
				DrawDebugOverlayForShadowClone( this );

			return;
		}
	}
	
	m_bInAssumedSyncState = false;
	


	

	//past this point, we're committed to a broad update

	if( bBigChanges )
	{
		MoveType_t sourceMoveType = pClonedEntity->GetMoveType();

			
		IPhysicsObject *pPhysObject = pClonedEntity->VPhysicsGetObject();
		if( (sourceMoveType == MOVETYPE_CUSTOM) || 
			(sourceMoveType == MOVETYPE_STEP) || 
			(sourceMoveType == MOVETYPE_WALK) ||
			(pPhysObject && 
				( 
					(pPhysObject->GetGameFlags() & FVPHYSICS_PLAYER_HELD) || 
					(pPhysObject->GetShadowController() != NULL) 
				)
			)
		  )
		{
//#ifdef _DEBUG
			SetMoveType( MOVETYPE_NONE ); //to kill an assert
//#endif
			//PUSH should be used sparingly, you can't stand on a MOVETYPE_PUSH object :/
			SetMoveType( MOVETYPE_VPHYSICS, pClonedEntity->GetMoveCollide() ); //either an unclonable movetype, or a shadow/held object
		}
		/*else if(sourceMoveType == MOVETYPE_STEP)
		{
			//SetMoveType( MOVETYPE_NONE ); //to kill an assert
			SetMoveType( MOVETYPE_VPHYSICS, pClonedEntity->GetMoveCollide() );
		}*/
		else
		{
			//if( m_bShadowTransformIsIdentity )
				SetMoveType( sourceMoveType, pClonedEntity->GetMoveCollide() );
			//else
			//{
			//	SetMoveType( MOVETYPE_NONE ); //to kill an assert
			//	SetMoveType( MOVETYPE_PUSH, pClonedEntity->GetMoveCollide() );
			//}
		}

		SolidType_t sourceSolidType = pClonedEntity->GetSolid();
		if( sourceSolidType == SOLID_BBOX )
			SetSolid( SOLID_VPHYSICS );
		else
			SetSolid( sourceSolidType );
		//SetSolid( SOLID_VPHYSICS );

		SetElasticity( pClonedEntity->GetElasticity() );
		SetFriction( pClonedEntity->GetFriction() );


		
		int iSolidFlags = pClonedEntity->GetSolidFlags() | FSOLID_CUSTOMRAYTEST;
		if( m_bShadowTransformIsIdentity )
			iSolidFlags |= FSOLID_CUSTOMBOXTEST; //need this at least for the player or they get stuck in themselves
		else
			iSolidFlags &= ~FSOLID_FORCE_WORLD_ALIGNED;
		/*if( pClonedEntity->IsPlayer() )
		{
			iSolidFlags |= FSOLID_CUSTOMRAYTEST | FSOLID_CUSTOMBOXTEST;
		}*/

		SetSolidFlags( iSolidFlags );



		SetEffects( pClonedEntity->GetEffects() | (EF_NODRAW | EF_NOSHADOW | EF_NORECEIVESHADOW) );

		SetCollisionGroup( pClonedEntity->GetCollisionGroup() );

		SetModelIndex( pClonedEntity->GetModelIndex() );
		SetModelName( pClonedEntity->GetModelName() );

		if( modelinfo->GetModelType( pClonedEntity->GetModel() ) == mod_studio )
			SetModel( STRING( pClonedEntity->GetModelName() ) );


		CCollisionProperty *pClonedCollisionProp = pClonedEntity->CollisionProp();
		SetSize( pClonedCollisionProp->OBBMins(), pClonedCollisionProp->OBBMaxs() );
	}

	FullSyncClonedPhysicsObjects( bBigChanges );
	SyncEntity( true );

	if( bBigChanges )
		CollisionRulesChanged();

	if( sv_debug_physicsshadowclones.GetBool() )
		DrawDebugOverlayForShadowClone( this );
}

void CPhysicsShadowClone::SyncEntity( bool bPullChanges )
{
	m_bShouldUpSync = false;

	CBaseEntity *pSource, *pDest;
	VMatrix *pTransform;
	if( bPullChanges )
	{
		pSource = m_hClonedEntity.Get();
		pDest = this;
		pTransform = &m_matrixShadowTransform;

		if( pSource == NULL )
			return;
	}
	else
	{
		pSource = this;
		pDest = m_hClonedEntity.Get();
		pTransform = &m_matrixShadowTransform_Inverse;

		if( pDest == NULL )
			return;
	}


	Vector ptOrigin, vVelocity;
	QAngle qAngles;

	ptOrigin = pSource->GetAbsOrigin();
	qAngles = pSource->GetAbsAngles();
	vVelocity = pSource->GetAbsVelocity();

	if( !m_bShadowTransformIsIdentity )
	{
		ptOrigin = (*pTransform) * ptOrigin;
		qAngles = TransformAnglesToWorldSpace( qAngles, pTransform->As3x4() );
		vVelocity = pTransform->ApplyRotation( vVelocity );	
	}
	//else
	//{
	//	pDest->SetGroundEntity( pSource->GetGroundEntity() );
	//}

	if( (ptOrigin != pDest->GetAbsOrigin()) || (qAngles != pDest->GetAbsAngles()) )
	{
		pDest->Teleport( &ptOrigin, &qAngles, NULL );
	}
	
	if( vVelocity != pDest->GetAbsVelocity() )
	{
		//pDest->IncrementInterpolationFrame();
		pDest->SetAbsVelocity( vec3_origin ); //the two step process helps, I don't know why, but it does
		pDest->ApplyAbsVelocityImpulse( vVelocity );
	}
}


static void FullSyncPhysicsObject( IPhysicsObject *pSource, IPhysicsObject *pDest, const VMatrix *pTransform, bool bTeleport )
{
	CGrabController *pGrabController = NULL;

	if( !pSource->IsAsleep() )
		pDest->Wake();

	float fSavedMass = 0.0f, fSavedRotationalDamping; //setting mass to 0.0f purely to kill a warning that I can't seem to kill with pragmas
	if( pSource->GetGameFlags() & FVPHYSICS_PLAYER_HELD )
	{
		//CBasePlayer *pPlayer = UTIL_PlayerByIndex( 1 );
		//Assert( pPlayer );

		CBaseEntity *pLookingForEntity = (CBaseEntity *)pSource->GetGameData();

		CBasePlayer *pHoldingPlayer = GetPlayerHoldingEntity( pLookingForEntity );
		if( pHoldingPlayer )
		{
			pGrabController = GetGrabControllerForPlayer( pHoldingPlayer );

			if ( !pGrabController )
				pGrabController = GetGrabControllerForPhysCannon( pHoldingPlayer->GetActiveWeapon() );
		}

		AssertMsg( pGrabController, "Physics object is held, but we can't find the holding controller." );
		GetSavedParamsForCarriedPhysObject( pGrabController, pSource, &fSavedMass, &fSavedRotationalDamping );
	}

	//Boiler plate
	{
		pDest->SetGameIndex( pSource->GetGameIndex() ); //what's it do?
		pDest->SetCallbackFlags( pSource->GetCallbackFlags() ); //wise?
		pDest->SetGameFlags( pSource->GetGameFlags() | FVPHYSICS_NO_SELF_COLLISIONS | FVPHYSICS_IS_SHADOWCLONE );
		pDest->SetMaterialIndex( pSource->GetMaterialIndex() );
		pDest->SetContents( pSource->GetContents() );

		pDest->EnableCollisions( pSource->IsCollisionEnabled() );
		pDest->EnableGravity( pSource->IsGravityEnabled() );
		pDest->EnableDrag( pSource->IsDragEnabled() );
		pDest->EnableMotion( pSource->IsMotionEnabled() );
	}	

	//Damping
	{
		float fSpeedDamp, fRotDamp;
		if( pGrabController )
		{
			pSource->GetDamping( &fSpeedDamp, NULL );
			pDest->SetDamping( &fSpeedDamp, &fSavedRotationalDamping );
		}
		else
		{
			pSource->GetDamping( &fSpeedDamp, &fRotDamp );
			pDest->SetDamping( &fSpeedDamp, &fRotDamp );
		}		
	}

	//stuff that we really care about
	{
		if( pGrabController )
			pDest->SetMass( fSavedMass );
		else
			pDest->SetMass( pSource->GetMass() );

		Vector ptOrigin, vVelocity, vAngularVelocity, vInertia;
		QAngle qAngles;

		pSource->GetPosition( &ptOrigin, &qAngles );
		pSource->GetVelocity( &vVelocity, &vAngularVelocity );
		vInertia = pSource->GetInertia();

		if( pTransform )
		{
#if 0
			pDest->SetPositionMatrix( pTransform->As3x4(), true ); //works like we think?
#else		
			ptOrigin = (*pTransform) * ptOrigin;
			qAngles = TransformAnglesToWorldSpace( qAngles, pTransform->As3x4() );
			vVelocity = pTransform->ApplyRotation( vVelocity );
			vAngularVelocity = pTransform->ApplyRotation( vAngularVelocity );
#endif
		}

		//avoid oversetting variables (I think that even setting them to the same value they already are disrupts the delicate physics balance)
		if( vInertia != pDest->GetInertia() )		
			pDest->SetInertia( vInertia );

		Vector ptDestOrigin, vDestVelocity, vDestAngularVelocity;
		QAngle qDestAngles;
		pDest->GetPosition( &ptDestOrigin, &qDestAngles );

		if( (ptOrigin != ptDestOrigin) || (qAngles != qDestAngles) )
			pDest->SetPosition( ptOrigin, qAngles, bTeleport );

		//pDest->SetVelocityInstantaneous( &vec3_origin, &vec3_origin );
		//pDest->Sleep();

		pDest->GetVelocity( &vDestVelocity, &vDestAngularVelocity );

		if( (vVelocity != vDestVelocity) || (vAngularVelocity != vDestAngularVelocity) )
			pDest->SetVelocityInstantaneous( &vVelocity, &vAngularVelocity );

		IPhysicsShadowController *pSourceController = pSource->GetShadowController();
		if( pSourceController == NULL )
		{
			if( pDest->GetShadowController() != NULL )
			{
				//we don't need a shadow controller anymore
				pDest->RemoveShadowController();
			}
		}
		else
		{
			IPhysicsShadowController *pDestController = pDest->GetShadowController();
			if( pDestController == NULL )
			{
				//we need a shadow controller
				float fMaxSpeed, fMaxAngularSpeed;
				pSourceController->GetMaxSpeed( &fMaxSpeed, &fMaxAngularSpeed );

				pDest->SetShadow( fMaxSpeed, fMaxAngularSpeed, pSourceController->AllowsTranslation(), pSourceController->AllowsRotation() );
				pDestController = pDest->GetShadowController();
				pDestController->SetTeleportDistance( pSourceController->GetTeleportDistance() );
				pDestController->SetPhysicallyControlled( pSourceController->IsPhysicallyControlled() );
			}

			//sync shadow controllers
			float fTimeOffset;
			Vector ptTargetPosition;
			QAngle qTargetAngles;
			fTimeOffset = pSourceController->GetTargetPosition( &ptTargetPosition, &qTargetAngles );

			if( pTransform )
			{
				ptTargetPosition = (*pTransform) * ptTargetPosition;
				qTargetAngles = TransformAnglesToWorldSpace( qTargetAngles, pTransform->As3x4() );
			}

			pDestController->Update( ptTargetPosition, qTargetAngles, fTimeOffset );
		}

		
	}

	//pDest->RecheckContactPoints();
}

static void PartialSyncPhysicsObject( IPhysicsObject *pSource, IPhysicsObject *pDest, const VMatrix *pTransform )
{
	Vector ptOrigin, vVelocity, vAngularVelocity, vInertia;
	QAngle qAngles;

	pSource->GetPosition( &ptOrigin, &qAngles );
	pSource->GetVelocity( &vVelocity, &vAngularVelocity );
	vInertia = pSource->GetInertia();

	if( pTransform )
	{
#if 0
		//pDest->SetPositionMatrix( matTransform.As3x4(), true ); //works like we think?
#else	
		ptOrigin = (*pTransform) * ptOrigin;
		qAngles = TransformAnglesToWorldSpace( qAngles, pTransform->As3x4() );
		vVelocity = pTransform->ApplyRotation( vVelocity );
		vAngularVelocity = pTransform->ApplyRotation( vAngularVelocity );
#endif
	}

	//avoid oversetting variables (I think that even setting them to the same value they already are disrupts the delicate physics balance)
	if( vInertia != pDest->GetInertia() )
		pDest->SetInertia( vInertia );

	Vector ptDestOrigin, vDestVelocity, vDestAngularVelocity;
	QAngle qDestAngles;
	pDest->GetPosition( &ptDestOrigin, &qDestAngles );
	pDest->GetVelocity( &vDestVelocity, &vDestAngularVelocity );


	if( (ptOrigin != ptDestOrigin) || (qAngles != qDestAngles) )
		pDest->SetPosition( ptOrigin, qAngles, false );

	if( (vVelocity != vDestVelocity) || (vAngularVelocity != vDestAngularVelocity) )
		pDest->SetVelocity( &vVelocity, &vAngularVelocity );

	pDest->EnableCollisions( pSource->IsCollisionEnabled() );
}



void CPhysicsShadowClone::FullSyncClonedPhysicsObjects( bool bTeleport )
{
	CBaseEntity *pClonedEntity = m_hClonedEntity.Get();
	if( pClonedEntity == NULL )
	{
		VPhysicsDestroyObject();
		return;
	}

	VMatrix *pTransform;
	if( m_bShadowTransformIsIdentity )
		pTransform = NULL;
	else
		pTransform = &m_matrixShadowTransform;

	IPhysicsObject *(pSourceObjects[1024]);
	int iObjectCount = pClonedEntity->VPhysicsGetObjectList( pSourceObjects, 1024 );

	//easy out if nothing has changed
	if( iObjectCount == m_CloneLinks.Count() )
	{
		int i;
		for( i = 0; i != iObjectCount; ++i )
		{
			if( pSourceObjects[i] == NULL )
				break;

			if( pSourceObjects[i] != m_CloneLinks[i].pSource )
				break;			
		}

		if( i == iObjectCount ) //no changes
		{
			for( i = 0; i != iObjectCount; ++i )
				FullSyncPhysicsObject( m_CloneLinks[i].pSource, m_CloneLinks[i].pClone, pTransform, bTeleport );

			return;
		}
	}



	//copy the existing list of clone links to a temp array, we're going to be starting from scratch and copying links as we need them
	PhysicsObjectCloneLink_t *pExistingLinks = NULL;
	int iExistingLinkCount = m_CloneLinks.Count();
	if( iExistingLinkCount != 0 )
	{
		pExistingLinks = (PhysicsObjectCloneLink_t *)stackalloc( sizeof(PhysicsObjectCloneLink_t) * m_CloneLinks.Count() );
		memcpy( pExistingLinks, m_CloneLinks.Base(), sizeof(PhysicsObjectCloneLink_t) * m_CloneLinks.Count() );
	}
	m_CloneLinks.RemoveAll();

	//now, go over the object list we just got from the source entity, and either copy or create links as necessary
	int i;
	for( i = 0; i != iObjectCount; ++i )
	{
		IPhysicsObject *pSource = pSourceObjects[i];

		if( pSource == NULL ) //this really shouldn't happen, but it does >_<
			continue;

		PhysicsObjectCloneLink_t cloneLink;

		int j;
		for( j = 0; j != iExistingLinkCount; ++j )
		{
			if( pExistingLinks[j].pSource == pSource )
				break;
		}

		if( j != iExistingLinkCount )
		{
			//copyable link found
			cloneLink = pExistingLinks[j];
			memset( &pExistingLinks[j], 0, sizeof( PhysicsObjectCloneLink_t ) ); //zero out this slot so we don't destroy it in cleanup
		}
		else
		{
			//no link found to copy, create a new one
			cloneLink.pSource = pSource;

			//apparently some collision code gets called on creation before we've set extra game flags, so we're going to cheat a bit and temporarily set our extra flags on the source
			unsigned int iOldGameFlags = pSource->GetGameFlags();
			pSource->SetGameFlags( iOldGameFlags | FVPHYSICS_IS_SHADOWCLONE );

			unsigned int size = physenv->GetObjectSerializeSize(pSource);
			byte *pBuffer = (byte *)stackalloc(size);
			memset( pBuffer, 0, size );

			physenv->SerializeObjectToBuffer( pSource, pBuffer, size ); //this should work across physics environments because the serializer doesn't write anything about itself to the template
			pSource->SetGameFlags( iOldGameFlags );
			cloneLink.pClone = m_pOwnerPhysEnvironment->UnserializeObjectFromBuffer( this, pBuffer, size, false ); //unserializer has to be in the target environment
			assert( cloneLink.pClone ); //there should be absolutely no case where we can't clone a valid existing physics object
	
			stackfree(pBuffer);
		}

		FullSyncPhysicsObject( cloneLink.pSource, cloneLink.pClone, pTransform, bTeleport );

		//cloneLink.pClone->Wake();

		m_CloneLinks.AddToTail( cloneLink );
	}


	//now go over the existing links, if any of them haven't been nullified, they need to be deleted
	for( i = 0; i != iExistingLinkCount; ++i )
	{
		if( pExistingLinks[i].pClone )
			m_pOwnerPhysEnvironment->DestroyObject(	pExistingLinks[i].pClone ); //also destroys shadow controller
	}


	VPhysicsSetObject( NULL );

	IPhysicsObject *pSource = m_hClonedEntity->VPhysicsGetObject();

	for( i = m_CloneLinks.Count(); --i >= 0; )
	{
		if( m_CloneLinks[i].pSource == pSource )
		{
			//m_CloneLinks[i].pClone->Wake();
			VPhysicsSetObject( m_CloneLinks[i].pClone );
			break;
		}
	}

	if( (i < 0) && (m_CloneLinks.Count() != 0) )
	{
		VPhysicsSetObject( m_CloneLinks[0].pClone );
	}

	stackfree( pExistingLinks );

	//CollisionRulesChanged();
}



void CPhysicsShadowClone::PartialSync( bool bPullChanges )
{
	VMatrix *pTransform;
	
	if( bPullChanges )
	{
		if( m_bShadowTransformIsIdentity )
			pTransform = NULL;
		else
			pTransform = &m_matrixShadowTransform;

		for( int i = m_CloneLinks.Count(); --i >= 0; )
			PartialSyncPhysicsObject( m_CloneLinks[i].pSource, m_CloneLinks[i].pClone, pTransform );
	}
	else
	{
		if( m_bShadowTransformIsIdentity )
			pTransform = NULL;
		else
			pTransform = &m_matrixShadowTransform_Inverse;

		for( int i = m_CloneLinks.Count(); --i >= 0; )
			PartialSyncPhysicsObject( m_CloneLinks[i].pClone, m_CloneLinks[i].pSource, pTransform );
	}

	SyncEntity( bPullChanges );
}



int CPhysicsShadowClone::VPhysicsGetObjectList( IPhysicsObject **pList, int listMax )
{
	int iCountStop = m_CloneLinks.Count();
	if( iCountStop > listMax ) 
		iCountStop = listMax;

	for( int i = 0; i != iCountStop; ++i, ++pList )
		*pList = m_CloneLinks[i].pClone;

	return iCountStop;
}


void CPhysicsShadowClone::VPhysicsDestroyObject( void )
{
	VPhysicsSetObject( NULL );
	
	for( int i = m_CloneLinks.Count(); --i >= 0; )
	{
		Assert( m_CloneLinks[i].pClone != NULL );
		m_pOwnerPhysEnvironment->DestroyObject(	m_CloneLinks[i].pClone );
	}
	m_CloneLinks.RemoveAll();

	SetMoveType( MOVETYPE_NONE );
	SetSolid( SOLID_NONE );
	SetSolidFlags( 0 );
	SetCollisionGroup( COLLISION_GROUP_NONE );

	BaseClass::VPhysicsDestroyObject();
}






bool CPhysicsShadowClone::ShouldCollide( int collisionGroup, int contentsMask ) const
{
	CBaseEntity *pClonedEntity = m_hClonedEntity.Get();

	if( pClonedEntity )
		return pClonedEntity->ShouldCollide( collisionGroup, contentsMask );
	else
		return false;
}

bool CPhysicsShadowClone::TestCollision( const Ray_t &ray, unsigned int fContentsMask, trace_t& trace )
{
	return false;

	/*CBaseEntity *pSourceEntity = m_hClonedEntity.Get();
	if( pSourceEntity == NULL )
		return false;

	enginetrace->ClipRayToEntity( ray, fContentsMask, pSourceEntity, &trace );
	return trace.DidHit();*/
}

int	CPhysicsShadowClone::ObjectCaps( void )
{
	return ((BaseClass::ObjectCaps() | FCAP_DONT_SAVE) & ~(FCAP_FORCE_TRANSITION | FCAP_ACROSS_TRANSITION | FCAP_MUST_SPAWN | FCAP_SAVE_NON_NETWORKABLE));
}





void CPhysicsShadowClone::SetCloneTransformationMatrix( const matrix3x4_t &sourceMatrix )
{
	m_matrixShadowTransform = sourceMatrix;
	m_bShadowTransformIsIdentity = m_matrixShadowTransform.IsIdentity();

	if( m_matrixShadowTransform.InverseGeneral( m_matrixShadowTransform_Inverse ) == false )
	{
		m_matrixShadowTransform.InverseTR( m_matrixShadowTransform_Inverse ); //probably not the right matrix, but we're out of options
	}

	FullSync();
	//PartialSync( true );
}






void CPhysicsShadowClone::SetClonedEntity( EHANDLE hEntToClone )
{
	VPhysicsDestroyObject();
	
	m_hClonedEntity = hEntToClone;

	//FullSyncClonedPhysicsObjects();
}

EHANDLE CPhysicsShadowClone::GetClonedEntity( void )
{
	return m_hClonedEntity;
}




//damage relays to source entity
bool CPhysicsShadowClone::PassesDamageFilter( const CTakeDamageInfo &info )
{
	CBaseEntity *pClonedEntity = m_hClonedEntity.Get();

	if( pClonedEntity )
		return pClonedEntity->PassesDamageFilter( info );
	else
		return BaseClass::PassesDamageFilter( info );
}

bool CPhysicsShadowClone::CanBeHitByMeleeAttack( CBaseEntity *pAttacker )
{
	CBaseEntity *pClonedEntity = m_hClonedEntity.Get();

	if( pClonedEntity )
		return pClonedEntity->CanBeHitByMeleeAttack( pAttacker );
	else
		return BaseClass::CanBeHitByMeleeAttack( pAttacker );
}

int CPhysicsShadowClone::OnTakeDamage( const CTakeDamageInfo &info )
{
	CBaseEntity *pClonedEntity = m_hClonedEntity.Get();

	if( pClonedEntity )
		return pClonedEntity->OnTakeDamage( info );
	else
		return BaseClass::OnTakeDamage( info );
}

int CPhysicsShadowClone::TakeHealth( float flHealth, int bitsDamageType )
{
	CBaseEntity *pClonedEntity = m_hClonedEntity.Get();

	if( pClonedEntity )
		return pClonedEntity->TakeHealth( flHealth, bitsDamageType );
	else
		return BaseClass::TakeHealth( flHealth, bitsDamageType );
}

void CPhysicsShadowClone::Event_Killed( const CTakeDamageInfo &info )
{
	CBaseEntity *pClonedEntity = m_hClonedEntity.Get();

	if( pClonedEntity )
		pClonedEntity->Event_Killed( info );
	else
		BaseClass::Event_Killed( info );
}

CPhysicsShadowClone *CPhysicsShadowClone::CreateShadowClone( IPhysicsEnvironment *pInPhysicsEnvironment, EHANDLE hEntToClone, const char *szDebugMarker, const matrix3x4_t *pTransformationMatrix /*= NULL*/ )
{
	AssertMsg( szDebugMarker != NULL, "All shadow clones must have a debug marker for where it came from in debug builds." );

	if( !sv_use_shadow_clones.GetBool() )
		return NULL;

	CBaseEntity *pClonedEntity = hEntToClone.Get();
	if( pClonedEntity == NULL )
		return NULL;

	AssertMsg( IsShadowClone( pClonedEntity ) == false, "Shouldn't attempt to clone clones" );

	if( pClonedEntity->IsMarkedForDeletion() )
		return NULL;

	//if( pClonedEntity->IsPlayer() )
	//	return NULL;

	IPhysicsObject *pPhysics = pClonedEntity->VPhysicsGetObject();

	if( pPhysics == NULL )
		return NULL;

	if( pPhysics->IsStatic() )
		return NULL;

	if( pClonedEntity->GetSolid() == SOLID_BSP )
		return NULL;

	if( pClonedEntity->GetSolidFlags() & (FSOLID_NOT_SOLID | FSOLID_TRIGGER) )
		return NULL;

	if( pClonedEntity->GetFlags() & (FL_WORLDBRUSH | FL_STATICPROP) )
		return NULL;

	/*if( FClassnameIs( pClonedEntity, "func_door" ) )
	{
		//only clone func_door's that are in front of the portal
		
		return NULL;
	}*/

	// Too many shadow clones breaks the game (too many entities)
	if( g_iShadowCloneCount >= MAX_SHADOW_CLONE_COUNT )
	{
		AssertMsg( false, "Too many shadow clones, consider upping the limit or reducing the level's physics props" );
		return NULL;
	}
	++g_iShadowCloneCount;

	CPhysicsShadowClone *pClone = (CPhysicsShadowClone*)CreateEntityByName("physicsshadowclone");
	s_IsShadowClone[pClone->entindex()] = true;
	pClone->m_pOwnerPhysEnvironment = pInPhysicsEnvironment;
	pClone->m_hClonedEntity = hEntToClone;
	DBG_CODE_NOSCOPE( pClone->m_szDebugMarker = szDebugMarker; );

	CPhysicsShadowCloneLL *pCloneLLEntry = s_SCLLManager.Alloc();
	pCloneLLEntry->pClone = pClone;
	pCloneLLEntry->pNext = s_EntityClones[pClonedEntity->entindex()];
	s_EntityClones[pClonedEntity->entindex()] = pCloneLLEntry;

	if( pTransformationMatrix )
	{
		pClone->m_matrixShadowTransform = *pTransformationMatrix;
		pClone->m_bShadowTransformIsIdentity = pClone->m_matrixShadowTransform.IsIdentity();

		if( !pClone->m_bShadowTransformIsIdentity )
		{
			if( pClone->m_matrixShadowTransform.InverseGeneral( pClone->m_matrixShadowTransform_Inverse ) == false )
			{
				pClone->m_matrixShadowTransform.InverseTR( pClone->m_matrixShadowTransform_Inverse ); //probably not the right matrix, but we're out of options
			}
		}
	}

	DispatchSpawn( pClone );

	return pClone;
}

void CPhysicsShadowClone::Free( void )
{
	VPhysicsDestroyObject();

	UTIL_Remove( this );

	//Too many shadow clones breaks the game (too many entities)
	--g_iShadowCloneCount;
}


void CPhysicsShadowClone::FullSyncAllClones( void )
{
	for( int i = s_ActiveShadowClones.Count(); --i >= 0; )
	{
		s_ActiveShadowClones[i]->FullSync( true );
	}
}


IPhysicsObject *CPhysicsShadowClone::TranslatePhysicsToClonedEnt( const IPhysicsObject *pPhysics )
{
	if( m_hClonedEntity.Get() != NULL )
	{
		for( int i = m_CloneLinks.Count(); --i >= 0; )
		{
			if( m_CloneLinks[i].pClone == pPhysics )
				return m_CloneLinks[i].pSource;
		}
	}

	return NULL;
}


void CPhysicsShadowClone::VPhysicsCollision( int index, gamevcollisionevent_t *pEvent )
{
	//the baseclass just screenshakes, makes sounds, and outputs dust, we rely on the original entity to do this when applicable
}




bool CPhysicsShadowClone::IsShadowClone( const CBaseEntity *pEntity )
{
	return s_IsShadowClone[pEntity->entindex()];
}

CPhysicsShadowCloneLL *CPhysicsShadowClone::GetClonesOfEntity( const CBaseEntity *pEntity )
{
	return s_EntityClones[pEntity->entindex()];
}



static void DrawDebugOverlayForShadowClone( CPhysicsShadowClone *pClone )
{
	unsigned char iColorIntensity = (pClone->IsInAssumedSyncState())?(127):(255);

	int iRed = (pClone->IsUntransformedClone())?(0):(iColorIntensity);
	int iGreen = iColorIntensity;
	int iBlue = iColorIntensity;

	NDebugOverlay::EntityBounds( pClone, iRed, iGreen, iBlue, (iColorIntensity>>2), 0.05f );
}


bool CTraceFilterTranslateClones::ShouldHitEntity( IHandleEntity *pEntity, int contentsMask )
{
	CBaseEntity *pEnt = EntityFromEntityHandle( pEntity );
	if( CPhysicsShadowClone::IsShadowClone( pEnt ) )
	{
		CBaseEntity *pClonedEntity = ((CPhysicsShadowClone *)pEnt)->GetClonedEntity();
		CPortalSimulator *pSimulator = CPortalSimulator::GetSimulatorThatOwnsEntity( pClonedEntity );
		if( pSimulator->m_DataAccess.Simulation.Dynamic.EntFlags[pClonedEntity->entindex()] & PSEF_IS_IN_PORTAL_HOLE )
			return m_pActualFilter->ShouldHitEntity( pClonedEntity, contentsMask );
		else
			return false;
	}
	else
	{
		return m_pActualFilter->ShouldHitEntity( pEntity, contentsMask );
	}
}

TraceType_t	CTraceFilterTranslateClones::GetTraceType() const
{
	return m_pActualFilter->GetTraceType();
}



